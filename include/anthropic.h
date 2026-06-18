#ifndef ANTHROPIC_H

#define ANTHROPIC_H

#include "http_client.h"

typedef enum AnthropicMessageRole{
    ROLE_SYSTEM,
    ROLE_USER,
    ROLE_ASSISTANT,
} AnthropicMessageRole;

typedef struct {
    AnthropicMessageRole role;
    char *content;
} AnthropicMessage;

typedef struct {
    HttpHeader *headers;
    char *model;
    size_t max_tokens;
    AnthropicMessage *messages;
    size_t message_count;
} AnthropicRequest;

typedef struct {
    char *type;
    char *text;
} AnthropicContent;

typedef struct {
    size_t input_tokens;
    size_t output_tokens;
} AnthropicUsage;

typedef struct {
    char *id;
    char *type;
    char *role;
    AnthropicContent *content;
    char *model;
    char *stop_reason;
    char *stop_sequence;
    AnthropicUsage usage;
} AnthropicResponse;

size_t serialize_request_body(char *body_buf, size_t buffer_len, AnthropicRequest *request);

void Run(void);

#endif
