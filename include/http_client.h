#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>
#include <stdio.h>

typedef enum { HTTP_GET, HTTP_POST } HttpMethod;

const char *http_method_to_str(HttpMethod http_method);

typedef struct {
    char *key;
    char *value;
} HttpHeader;

typedef struct {
    int status_code;
    char* body;
    size_t body_length;
    char *headers;
} HTTPResponse;

HTTPResponse *http_request(const HttpMethod http_method, const char *host, const char *port, const char *path, const HttpHeader *headers, const size_t header_count, const char* body, FILE *output_stream, FILE *error_stream);
HTTPResponse *https_request(const HttpMethod http_method, const char *host, const char *port, const char *path, const HttpHeader *headers, const size_t header_count, const char* body, FILE *output_stream, FILE *error_stream);

#endif
