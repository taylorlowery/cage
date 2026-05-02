#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>

#define MAXDATASIZE 4096

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
int get_status_code_from_response_body(char *http_resp_raw) {
    // TODO: actually do this
    // just iterate through the body til the first space
    // as the code is always right after it.
    return 200;
}

// send_all sends a string buffer to a socket
// and returns the total number of bytes successfully sent.
int send_all(int socket_fd, char *buf, int buf_len) {
    size_t total_bytes_sent = 0;
    int remaining_byes = buf_len;
    int n = 0;

    while (total_bytes_sent < buf_len) {
        n = send(socket_fd, buf + total_bytes_sent, remaining_byes, 0);
        if (-1 == n) {
            // TODO: handle error
            // return -1?
            break;
        }
        total_bytes_sent += n;
        remaining_byes -= n;
    }

    return total_bytes_sent;
}


// post() sends a POST request to a given host (eg, "http://example.com"),
// and returns a pointer to a response object.
// The caller is responsible for freeing the response.
HTTPResponse *http_request(const HttpMethod http_method, const char *host, const char *port, const char *path, const char* body, const size_t body_len, FILE *output_stream, FILE *error_stream){
    HTTPResponse *resp = NULL;
    int sockfd;
    int bytes_received;
    struct addrinfo hints, *servinfo, *p;
    int status;
    char s[INET6_ADDRSTRLEN];
    char buf[MAXDATASIZE]; // whatever

    // zero out hints
    void *memset_res = memset(&hints, 0, sizeof(hints));
    if (NULL == memset_res) {
        fprintf(output_stream, "Failed to zero out memory for hints\n");
        return NULL;
    }
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(error_stream, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    // loop through results and connect to one
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        inet_ntop(
            p->ai_family,
            get_in_addr((struct sockaddr *)p->ai_addr),
            s,
            sizeof(s)
        );
        fprintf(output_stream, "Attempting connection to %s...\n", s);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(error_stream, "Failed to connect to %s :( \n", host);
        return NULL;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    fprintf(output_stream, "Connected to %s!\n", s);

    freeaddrinfo(servinfo);

    // finally send our body
    char headers[MAXDATASIZE];
    const char *method_str = http_method_to_str(http_method);
    const char *path_str = (NULL == path || strlen(path) == 0) ? "/" : path;
    int headers_len = snprintf(
        headers,
        sizeof(headers),
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        method_str,
        path_str,
        host,
        body_len
    );

    // send headers
    int bytes_sent = send_all(sockfd, headers, headers_len);
    if (-1 == bytes_sent) {
        perror("send headers");
        close(sockfd);
        return NULL;
    }

    // send body, if necessary
    if (NULL != body && 0 < body_len) {
        int bytes_sent = send_all(sockfd, body, body_len);
        if (-1 == bytes_sent) {
            perror("send body");
            close(sockfd);
            return NULL;
        }
    }


    // receive and parse response
    if ((bytes_received = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        return NULL;
    }

    buf[bytes_received] = '\0';
    close(sockfd);

    resp = malloc(sizeof(HTTPResponse));
    if (NULL == resp) {
        perror("malloc HTTPResponse");
        return NULL;
    }

    int resp_status_code = get_status_code_from_response_body(buf);

    char *resp_body = strstr(buf, "\r\n\r\n");
    if (NULL == resp_body) {
        perror("strstr on resp body");
        return NULL;
    }

    resp->status_code = resp_status_code;
    resp->body = strdup(resp_body);
    resp->body_length = bytes_received;
    resp->headers = NULL;

    return resp;
}
