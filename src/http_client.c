/*
* Yes, I'm aware that libcurl would have made this a lot easier.
*/
#include "http_client.h"
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// arbitrary buffer size
#define DEFAULT_BUFFER_SIZE 8192

// Connection helper struct to assist in abstracting
// http and https requests.
typedef struct {
    int sockfd;
    SSL *ssl;
} Connection;

static void free_connection(Connection *conn) {
    if (NULL != conn->ssl) {
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
    }
    if (-1 != conn->sockfd) {
        close(conn->sockfd);
    }
    free(conn);
}

void free_http_response(HTTPResponse *http_response) {
    if (NULL == http_response) {
        return;
    }
    if (NULL != http_response->headers) {
        for (size_t i = 0; i < http_response->header_count; i++) {
            free(http_response->headers[i].key);
            free(http_response->headers[i].value);
        }
        free(http_response->headers);
    }
    if (NULL != http_response->body) {
        free(http_response->body);
    }
    free(http_response);
}


// conn_write writes from a buffer to an http or https connection.
// if the SSL pointer on the connection is not NULL, defaults to https,
// and falls through to http otherwise,
// returning the number of bytes successfully written to the connection.
// Returns -1 if the connection's socket fd is -1.
static ssize_t conn_write(const Connection *conn, const char *buf, size_t buf_len) {
    if (NULL != conn->ssl) {
        return SSL_write(conn->ssl, buf, buf_len);
    }
    if (-1 == conn->sockfd) {
        return -1;
    }
    return send(conn->sockfd, buf, buf_len, 0);
}


// conn_read reads from a connection into a provided buffer.
// If the connection's SSL pointer is not NULL, defaults to https,
// otherwise falling through to http,
// returning the number of bytes successfuly read from the connection.
// Returns -1 if the connection's socket fd is -1.
static ssize_t conn_read(const Connection *conn, char *buf, size_t buf_len) {
    if (NULL != conn->ssl) {
        return SSL_read(conn->ssl, buf, buf_len);
    }
    if (-1 == conn->sockfd) {
        return -1;
    }
    return recv(conn->sockfd, buf, buf_len, 0);
}

// http_method_to_str receives a HttpMethod enum
// and returns the appropriate string for use in an HTTP request.
const char *http_method_to_str(HttpMethod http_method) {
    switch (http_method) {
        case HTTP_GET:
            return "GET";
        case HTTP_POST:
            return "POST";
        // TODO: handle other http methods
        default:
            return NULL;
    }
}

// get_in_addr populates a sockaddr_in or sockaddr_in6 struct
// with the IP address of the given sockaddr struct.
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// parse the HTTP status code from a raw http response body
int get_status_code_from_response_body(const char *http_resp_raw) {
    char *first_space = strchr(http_resp_raw, ' ');
    if (NULL == first_space) {
        return -1;
    }

    char *end = NULL;
    long status_code = (int)strtol(first_space + 1, &end, 10);
    if (end == first_space + 1) {
        // no digits were consumed -- code parsing failed.
        return -1;
    }

    return (int)status_code;
}

// build_headers builds the headers for an HTTP Request.
// extra-headers should include content-type
static int build_headers(char *header_buf, size_t buf_size, const char *path, const HttpMethod http_method, const char *host, const size_t content_length, const HttpHeader *extra_headers, const size_t extra_headers_count) {
    const char *method_str = http_method_to_str(http_method);
    if (NULL == method_str) {
        return -1;
    }
    const char *path_str = (NULL == path || strlen(path) == 0) ? "/" : path;
    int cursor = snprintf(
        header_buf,
        buf_size,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n",
        method_str,
        path_str,
        host
    );

    if (0 < content_length) {
        cursor += snprintf(
            header_buf + cursor,
            buf_size - cursor,
            "Content-Length: %zu\r\n",
            content_length
        );
    }

    if (NULL != extra_headers && 0 < extra_headers_count) {
        for (size_t i = 0; i < extra_headers_count; i++) {
            // TODO: check if we need to resize

            // add each header to headers
            cursor += snprintf(
                header_buf + cursor,
                buf_size - cursor,
                "%s: %s\r\n",
                extra_headers[i].key,
                extra_headers[i].value
            );
        }
    }

    snprintf(header_buf + cursor, buf_size - cursor, "\r\n");
    return 0;
}

// send_all sends a string buffer to a socket,
// returning -1 on error and 0 on success.
static ssize_t send_all(const Connection *conn, const char *buf, const size_t buf_len){
    size_t remaining_bytes = buf_len;
    ssize_t n = 0;
    ssize_t total_bytes_sent = 0;
    while ((size_t)total_bytes_sent < buf_len) {
        n = conn_write(conn, buf + total_bytes_sent, remaining_bytes);
        if (-1 == n) {
            return -1;
        }
        total_bytes_sent += n;
        remaining_bytes -= n;
    }

    return total_bytes_sent;
}

static ssize_t recv_all(const Connection *conn, char **recv_buffer, size_t *buf_capacity) {
    ssize_t total_bytes_received = 0;
    ssize_t bytes_received = 0;
    while ((bytes_received = conn_read(conn, *recv_buffer + total_bytes_received, *buf_capacity - total_bytes_received - 1)) > 0) {
        total_bytes_received += bytes_received;
        // if necessary, resize buffer
        if ((size_t)total_bytes_received >= *buf_capacity - 1) {
            *buf_capacity = (*buf_capacity) * 2;
            char *tmp = realloc(*recv_buffer, *buf_capacity);
            if (NULL == tmp) {
                return -1;
            }
            *recv_buffer = tmp;
        }
    }

    if (-1 == bytes_received) {
        // recv() failed
        return -1;
    }

    (*recv_buffer)[total_bytes_received] = '\0';
    return total_bytes_received;
}

// parse headers from a raw http response and add them to
// an HTTPResponse struct.
static int parse_headers(HTTPResponse *http_response, const char *raw_response_buffer, FILE *error_stream) {
    // skip status line?
    const char *line = strstr(raw_response_buffer, "\r\n");
    if (NULL == line) {
        return -1;
    }
    line += 2;

    // find "\r\n\r\n" separator
    const char *header_end = strstr(raw_response_buffer, "\r\n\r\n");
    if (NULL == header_end) {
        return -1;
    }

    // allocate headers
    if (http_response->header_capacity < 8) {
        http_response->header_capacity = 8;
    }
    http_response->headers = calloc(http_response->header_capacity, sizeof(HttpHeader));
    if (NULL == http_response->headers) {
        fprintf(error_stream, "failed to allocate header space");
        return -1;
    }

    // up to the separator, read each line by reading up to "\r\n"
    while (line < header_end) {
        // find the separator that ends the current header line
        const char *line_end = strstr(line, "\r\n");
        if (NULL == line_end || line_end > header_end) {
            break;
        }

        // find the colon in the header
        const char *colon = memchr(line, ':', line_end - line);
        // if no colon was found, continue to the next header
        if (colon == NULL) {
            fprintf(error_stream, "unable to find colon in header: \"%.*s\"\n", (int)(line_end - line), line);
            line = line_end + 2;
            continue;
        }

        // if I have reached header capacity, increase and realloc
        if (http_response->header_count == http_response->header_capacity) {
            http_response->header_capacity *= 2;
            HttpHeader *temp = realloc(http_response->headers, http_response->header_capacity * sizeof(HttpHeader));
            if (NULL == temp) {
                return -1;
            }
            http_response->headers = temp;
        }

        const char *key_start = line;
        size_t key_len = colon - key_start;
        const char *value_start = colon + 1;
        // advance the "value" start past any whitespace.
        while (value_start < line_end && isspace((unsigned char)*value_start)) {
            value_start++;
        }
        size_t value_len = line_end - value_start;

        // create HTTPHeader, set before to key, after to value
        HttpHeader *h = &http_response->headers[http_response->header_count];

        // allocate space for the key and val from the header
        h->key = calloc(key_len + 1, sizeof(char));
        if (NULL == h->key) {
            fprintf(error_stream, "failed to allocate for header key");
            return -1;
        }
        h->value = calloc(value_len + 1, sizeof(char));
        if (NULL == h->value) {
            fprintf(error_stream, "failed to allocate for header value");
            free(h->key);
            return -1;
        }

        memcpy(h->key, key_start, key_len);
        h->key[key_len] = '\0';
        memcpy(h->value, value_start, value_len);
        h->value[value_len] = '\0';

        // advance header count
        http_response->header_count += 1;

        line = line_end + 2;
    }

    return 0;
}

// returns a borrowed pointer to the header value, owned by `resp`.
// Do not free or mutate. Only valid as long as `resp` is valid.
// returns NULL if the passed in key is not present in the resp headers.
static const char *get_response_header(const HTTPResponse *resp, const char *key) {
    if (NULL == resp || NULL == key) {
        return NULL;
    }
    for (size_t i = 0; i < resp->header_count; i++) {
        if (0 == strcasecmp(resp->headers[i].key, key)) {
            return resp->headers[i].value;
        }
    }
    return NULL;
}

// body_start should point to the body portion of the raw header response, after the headers.
static int parse_chunked_body(HTTPResponse *resp, char *body_start, size_t raw_body_len, FILE *error_stream){

    size_t total_body_bytes = 0;
    // first pass through:
    // iterate through hex byte lines
    // gather the full size,
    // then allocate a buffer for the body.

    char *line = body_start;
    char *line_end = NULL;
    const char *body_end = body_start + raw_body_len;

    while (line < body_end) {
        size_t chunk_size = strtoul(line, &line_end, 16);
        if (line_end == line) {
            // no hex digits parsed; show the offending token, bounded to the
            // next CRLF (or buffer end) and capped at 64 so a runaway line
            // doesn't dump the whole response.
            const char *crlf = memchr(line, '\r', (size_t)(body_end - line));
            size_t snippet = crlf ? (size_t)(crlf - line) : (size_t)(body_end - line);
            if (snippet > 64) snippet = 64;
            fprintf(error_stream,
                    "chunk-size line at offset %zu has no hex digits: \"%.*s\"\n",
                    (size_t)(line - body_start), (int)snippet, line);
            return -1;
        }
        // validate that the hex token is followed by "\r\n" (chunk extensions
        // like ";name=value" are rejected for now — add a comment if you
        // later skip past them).
        if ('\r' != *line_end || '\n' != *(line_end + 1)) {
            size_t snippet = (size_t)(line_end + 2 - line);
            if (snippet > 64) snippet = 64;
            fprintf(error_stream,
                    "chunk-size line at offset %zu not terminated by \\r\\n: \"%.*s\"\n",
                    (size_t)(line - body_start), (int)snippet, line);
            return -1;
        }
        if (chunk_size == 0) {
            break;
        }
        total_body_bytes += chunk_size;
        // advance past <hex>\r\n and <chunk>\r\n
        line = line_end + 2 + chunk_size + 2;
        if (line > body_end) {
            fprintf(error_stream, "chunk extends past body end\n");
            return -1;
        }
    }


    // if the body had no length,
    // just return.
    if (0 == total_body_bytes) {
        return 0;
    }
    char *body_buffer = calloc(total_body_bytes + 1, sizeof(char));
    if (NULL == body_buffer) {
        fprintf(error_stream, "failed to allocate memory for chunked body buffer");
        return -1;
    }

    // second pass:
    // iterate through the string lines and add them to the buffer
    line = body_start;
    line_end = NULL;
    size_t offset = 0;

    while (line < body_end) {
        // scan the hex line
        size_t chunk_size = strtoul(line, &line_end, 16);
        // TODO: validate that the ending is \r\n.
        // For now we'll trust first pass's validation.
        if (0 == chunk_size) {
            break;
        }
        //memcpy chars between line and line_end to body_buffer
        memcpy(body_buffer + offset, line_end + 2, chunk_size);
        offset += chunk_size;
        // advance past next hex line
        line = line_end + 2 + chunk_size + 2;
        // if we're past body_end, break
        if (line > body_end) {
            fprintf(error_stream, "chunk extends past body end\n");
            return -1;
        }
    }


    body_buffer[total_body_bytes] = '\0';

    // set the resp body to the buffer
    resp->body = body_buffer;
    resp->body_length = total_body_bytes;

    return 0;
}

// parse a raw http response to an HTTPResponse struct.
HTTPResponse *parse_response_body_to_http_response(const char *raw_response_buffer, FILE *error_stream) {
    HTTPResponse *http_response = calloc(1, sizeof(HTTPResponse));
    if (NULL == http_response) {
        perror("calloc HTTPResponse");
        return NULL;
    }

    // get http status
    int resp_status_code = get_status_code_from_response_body(raw_response_buffer);
    http_response->status_code = resp_status_code;

    // parse and set headers on response
    int parsed_headers = parse_headers(http_response, raw_response_buffer, error_stream);
    if (0 != parsed_headers) {
        free_http_response(http_response);
        return NULL;
    }

    // find the end of the headers/beginning of response body by locating separator
    char *resp_body = strstr(raw_response_buffer, "\r\n\r\n");
    if (NULL == resp_body) {
        fprintf(error_stream, "strstr on resp body");
        free_http_response(http_response);
        return NULL;
    }

    // skip four characters to advance past '\r\n\r\n' separator
    resp_body = resp_body + 4;
    size_t body_len = strlen(resp_body);

    // parse a the response body as chunked if so specified in in the response headers.
    // otherwise set the response body to the raw response body string.
    const char *transfer_encoding = get_response_header(http_response, "Transfer-Encoding");
    if (NULL != transfer_encoding && 0 == strcasecmp(transfer_encoding, "chunked")) {
        int body_parsed = parse_chunked_body(http_response, resp_body, body_len, error_stream);
        if (0 != body_parsed) {
            fprintf(error_stream, "failed to parse chunked body");
            free_http_response(http_response);
            return NULL;
        }
    } else {
        http_response->body = strdup(resp_body);
        http_response->body_length = strlen(http_response->body);
    }

    // validate strdup succeeded (no OOM)
    if (NULL == http_response->body) {
        fprintf(error_stream, "failed to allocate response body");
        free_http_response(http_response);
        return NULL;
    }


    return http_response;
}

// http connect attempts to connect to a socket and returns its socket fd,
// or returns -1 on error.
static Connection *http_connect(const char *host, const char *port, FILE *output_stream, FILE *error_stream) {
    int sockfd = -1;
    struct addrinfo hints;
    struct addrinfo *servinfo = NULL;
    struct addrinfo *p = NULL;
    int status = 0;
    char s[INET6_ADDRSTRLEN] = {0};

    // zero out hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(error_stream, "getaddrinfo: %s\n", gai_strerror(status));
        // returning directly without calling cleanup because nothing has been initialized yet.
        // If the code changes, we may need to call cleanup here.
        return NULL;
    }

    // loop through results and connect to one
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        const char *addr_str = inet_ntop(
            p->ai_family,
            get_in_addr((struct sockaddr *)p->ai_addr),
            s,
            sizeof(s)
        );

        // addr_str should never be NULL because by this point
        // we have successfully retrieved the address with `getaddrinfo()`.
        // We will check anyway in case of something going really wrong:
        if (NULL == addr_str) {
            // TODO: more informative log line
            fprintf(error_stream, "Failed to get addr as string in inet_ntop");
            // try the next IP
            continue;
        }

        fprintf(output_stream, "Attempting connection to %s...\n", s);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            sockfd = -1;
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(error_stream, "Failed to connect to %s :( \n", host);
        freeaddrinfo(servinfo);
        if (-1 < sockfd) {
            close(sockfd);
        }
        return NULL;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    fprintf(output_stream, "Connected to %s!\n", s);

    freeaddrinfo(servinfo);

    // allocate connection
    Connection *conn = NULL;
    conn = calloc(1, sizeof(Connection));
    if (NULL == conn) {
        fprintf(error_stream, "Failed to allocate memory for connection\n");
        return NULL;
    }

    conn->sockfd = sockfd;
    conn->ssl = NULL;
    return conn;
}

ssize_t http_send(Connection *conn, const HttpMethod http_method, const char *path, const char *host, const HttpHeader *extra_headers, size_t extra_headers_count, const char *body, FILE *error_stream) {
    ssize_t total_bytes_sent = 0;
    size_t body_len = (NULL != body) ? strlen(body) : 0;
    char header_buf[DEFAULT_BUFFER_SIZE];
    int success_code = build_headers(header_buf, DEFAULT_BUFFER_SIZE, path, http_method, host, body_len, extra_headers, extra_headers_count);
    if (0 != success_code) {
        fprintf(error_stream, "build_headers\n");
        return -1;
    }

    // send headers
    ssize_t header_bytes_sent = send_all(conn, header_buf, strlen(header_buf));
    if (-1 == header_bytes_sent) {
        fprintf(error_stream, "send headers: %s\n", strerror(errno));
        return -1;
    }
    total_bytes_sent += header_bytes_sent;

    // send body, if necessary
    if (NULL != body && 0 < body_len) {
        ssize_t body_bytes_sent = send_all(conn, body, body_len);
        if (-1 == body_bytes_sent) {
            fprintf(error_stream, "send body: %s\n", strerror(errno));
            return -1;
        }
        total_bytes_sent += body_bytes_sent;
    }

    return total_bytes_sent;
}

HTTPResponse *http_receive(Connection *conn, FILE *error_stream) {
    // arbitrary starting buf capacity
    size_t buf_capacity = 4096;
    char *recv_buffer = calloc(buf_capacity, sizeof(char));
    if (NULL == recv_buffer) {
        perror("calloc buf");
        return NULL;
    }

    // receive and parse response
    ssize_t total_bytes_received = recv_all(conn, &recv_buffer, &buf_capacity);
    if (-1 == total_bytes_received) {
        perror("Error on recv");
        free(recv_buffer);
        return NULL;
    }

    HTTPResponse *resp = parse_response_body_to_http_response(recv_buffer, error_stream);
    free(recv_buffer);
    return resp;
}

// post() sends a POST request to a given host (eg, "http://example.com"),
// and returns a pointer to a response object.
// The caller is responsible for freeing the response.
HTTPResponse *http_request(const HttpMethod http_method, const char *host, const char *port, const char *path, const HttpHeader *extra_headers, const size_t extra_headers_count, const char* body, FILE *output_stream, FILE *error_stream){
    // connect
    Connection *conn = http_connect(host, port, output_stream, error_stream);
    if (NULL == conn || -1 == conn->sockfd) {
        fprintf(error_stream, "Failed to connect to %s:%s\n", host, port);
        return NULL;
    }

    // send
    ssize_t total_bytes_sent = http_send(conn, http_method, path, host, extra_headers, extra_headers_count, body, error_stream);
    if (-1 == total_bytes_sent) {
        free_connection(conn);
        return NULL;
    }

    // receive response
    // on err, response will be NULL -- caller will have to handle it.
    HTTPResponse *resp = http_receive(conn, error_stream);
    free_connection(conn);
    return resp;
}


SSL_CTX *create_ssl_ctx(FILE *error_stream) {

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (NULL == ctx){
        fprintf(error_stream, "Failed to create SSL context.\n");
        return NULL;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    // set default certificate store
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        fprintf(error_stream, "Failed to set default certificate store.\n");
        SSL_CTX_free(ctx);
        return NULL;
    }

    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
        fprintf(error_stream, "Failed to set the minimum TLS protocol version.\n");
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}


// ssl_connect is a WIP.
// Currently in placeholder mode while I get other stuff sorted.
// Caller has to free the connection.
static Connection *ssl_connect(const char* host, const char *port, FILE *output_stream, FILE *error_stream) {

    SSL_CTX *ctx = create_ssl_ctx(error_stream);
    if (NULL == ctx) {
        return NULL;
    }

    SSL *ssl = SSL_new(ctx);
    if (NULL == ssl) {
        fprintf(error_stream, "Failed to create the SSL object.\n");
        SSL_CTX_free(ctx);
        return NULL;
    }
    SSL_CTX_free(ctx);

    // create connection object with connected socket.
    Connection *conn = http_connect(host, port, output_stream, error_stream);
    if (NULL == conn || -1 == conn->sockfd) {
        fprintf(error_stream, "Failed to create connection object\n");
        SSL_free(ssl);
        return NULL;
    }

    SSL_set_fd(ssl, conn->sockfd);
    SSL_set_tlsext_host_name(ssl, host);

    // TLS HANDSHAAAAAAKE!!!
    if (SSL_connect(ssl) <= 0) {
        fprintf(error_stream, "SSL connection failed!\n");
        SSL_free(ssl);
        free_connection(conn);
        return NULL;
    }

    conn->ssl = ssl;
    return conn;
}

HTTPResponse *https_request(const HttpMethod http_method, const char *host, const char *port, const char *path, const HttpHeader *headers, const size_t header_count, const char* body, FILE *output_stream, FILE *error_stream) {
    // connect
    Connection *conn = ssl_connect(host, port, output_stream, error_stream);
    if (NULL == conn || NULL == conn->ssl) {
        fprintf(error_stream, "Failed to connect to %s:%s\n", host, port);
        return NULL;
    }

    // send
    ssize_t total_bytes_sent = http_send(conn, http_method, path, host, headers, header_count, body, error_stream);
    if (-1 == total_bytes_sent) {
        free_connection(conn);
        return NULL;
    }

    // receive response
    // on err, response will be NULL -- caller will have to handle it.
    HTTPResponse *resp = http_receive(conn, error_stream);
    free_connection(conn);
    return resp;
}
