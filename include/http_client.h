#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>
#include <stdio.h>

typedef struct {
    int status_code;
    char* body;
    size_t body_length;
    char *headers;
} HTTPResponse;

HTTPResponse *post(char *host, char *port, char* body, size_t body_len, FILE *output_stream, FILE *error_stream);

#endif
