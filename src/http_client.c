#include "http_client.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>

// arbitrary buffer size
#define DEFAULT_BUFFER_SIZE 4096

// http_method_to_str receives a HttpMethod enum
// and returns the appropriate string for use in an HTTP request.
char *http_method_to_str(HttpMethod http_method) {
    switch (http_method) {
        case HTTP_GET:
            return "GET";
        case HTTP_POST:
            return "POST";
        default:
            return "ERROR";
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

void build_headers(char *header_buf, size_t buf_size, const char *path, const HttpMethod http_method, const char *host, const size_t content_length) {
    const char *method_str = http_method_to_str(http_method);
    const char *path_str = (NULL == path || strlen(path) == 0) ? "/" : path;
    snprintf(
        header_buf,
        buf_size,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        method_str,
        path_str,
        host,
        content_length
    );
}

// send_all sends a string buffer to a socket,
// returning -1 on error and 0 on success.
ssize_t send_all(const int socket_fd, const char *buf, const size_t buf_len){
    size_t remaining_bytes = buf_len;
    ssize_t n = 0;
    ssize_t total_bytes_sent = 0;
    while ((size_t)total_bytes_sent < buf_len) {
        n = send(socket_fd, buf + total_bytes_sent, remaining_bytes, 0);
        if (-1 == n) {
            return -1;
        }
        total_bytes_sent += n;
        remaining_bytes -= n;
    }

    return total_bytes_sent;
}

ssize_t recv_all(int sockfd, char **recv_buffer, size_t *buf_capacity) {
    ssize_t total_bytes_received = 0;
    ssize_t bytes_received = 0;
    while ((bytes_received = recv(sockfd, *recv_buffer + total_bytes_received, *buf_capacity - total_bytes_received - 1, 0)) > 0) {
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

// TODO: return something meaningful.
HTTPResponse *parse_response_body_to_http_response(const char *raw_response_buffer, FILE *error_stream) {
    HTTPResponse *http_response = calloc(1, sizeof(HTTPResponse));
    if (NULL == http_response) {
        perror("calloc HTTPResponse");
        return NULL;
    }

    int resp_status_code = get_status_code_from_response_body(raw_response_buffer);

    char *resp_body = strstr(raw_response_buffer, "\r\n\r\n");
    if (NULL == resp_body) {
        fprintf(error_stream, "strstr on resp body");
        free(http_response);
        return NULL;
    }

    http_response->status_code = resp_status_code;
    // skip four spaces to avoid duplicating '\r\n\r\n'
    http_response->body = strdup(resp_body + 4);
    // validate strdup succeeded (no OOM)
    if (NULL == http_response) {
        fprintf(error_stream, "strdup on response body");
        free(http_response);
        return NULL;
    }

    http_response->body_length = strlen(http_response->body);
    // TODO: parse headers from response and store in resp->headers
    http_response->headers = NULL;

    return http_response;
}

// cleans up the resources allocated for an http request.
static void clean_up_http_request_resources(struct addrinfo *servinfo, int sockfd, HTTPResponse *resp, char *recv_buffer) {
    if (NULL != servinfo) {
        freeaddrinfo(servinfo);
    }
    if (sockfd >= 0) {
        close(sockfd);
    }
    if (NULL != recv_buffer) {
        free(recv_buffer);
    }
    if (NULL != resp) {
        free(resp->body);
        free(resp);
    }
}

// post() sends a POST request to a given host (eg, "http://example.com"),
// and returns a pointer to a response object.
// The caller is responsible for freeing the response.
HTTPResponse *http_request(const HttpMethod http_method, const char *host, const char *port, const char *path, const char* body, FILE *output_stream, FILE *error_stream){
    HTTPResponse *resp = NULL;
    int sockfd = -1;
    struct addrinfo hints;
    struct addrinfo *servinfo = NULL;
    struct addrinfo *p = NULL;
    int status = 0;
    char s[INET6_ADDRSTRLEN] = {0};

    size_t buf_capacity = 4096;
    char *recv_buffer = NULL;

    size_t body_len = (NULL != body) ? strlen(body) : 0;

    // zero out hints
    void *memset_res = memset(&hints, 0, sizeof(hints));
    if (NULL == memset_res) {
        fprintf(error_stream, "Failed to zero out memory for hints\n");
        // returning directly without calling cleanup because nothing has been initialized yet.
        // If the code changes, we may need to call cleanup here.
        return NULL;
    }
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
        clean_up_http_request_resources(servinfo, sockfd, resp, recv_buffer);
        return NULL;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    fprintf(output_stream, "Connected to %s!\n", s);

    // finally send our body
    char headers[DEFAULT_BUFFER_SIZE];
    build_headers(headers, DEFAULT_BUFFER_SIZE, path, http_method, host, body_len);

    // send headers
    ssize_t header_bytes_sent = send_all(sockfd, headers, strlen(headers));
    if (-1 == header_bytes_sent) {
        perror("send headers");
        clean_up_http_request_resources(servinfo, sockfd, resp, recv_buffer);
        return NULL;
    }

    // send body, if necessary
    if (NULL != body && 0 < body_len) {
        ssize_t body_bytes_sent = send_all(sockfd, body, body_len);
        if (-1 == body_bytes_sent) {
            perror("send body");
            clean_up_http_request_resources(servinfo, sockfd, resp, recv_buffer);
            return NULL;
        }
    }


    recv_buffer = calloc(buf_capacity, sizeof(char));
    if (NULL == recv_buffer) {
        perror("calloc buf");
        clean_up_http_request_resources(servinfo, sockfd, resp, recv_buffer);
        return NULL;
    }

    // receive and parse response
    ssize_t total_bytes_received = recv_all(sockfd, &recv_buffer, &buf_capacity);
    if (-1 == total_bytes_received) {
        perror("Error on recv");
        clean_up_http_request_resources(servinfo, sockfd, resp, recv_buffer);
        return NULL;
    }

    resp = parse_response_body_to_http_response(recv_buffer, error_stream);
    // explicitly handle NULL(error) response from parse,
    // even though the final cleanup and return of resp
    // is hypothetically identical if resp is NULL.
    if (NULL == resp) {
        clean_up_http_request_resources(servinfo, sockfd, resp, recv_buffer);
        return NULL;
    }

    // clean up all resources except the response, which will be returned.
    clean_up_http_request_resources(servinfo, sockfd, NULL, recv_buffer);
    return resp;
}
