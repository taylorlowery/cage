#ifndef ANTHROPIC_H

#define ANTHROPIC_H

#include "json.h"
#include "http_client.h"
#include "provider.h"

typedef enum AnthropicMessageRole{
    ANTHROPIC_ROLE_SYSTEM,
    ANTHROPIC_ROLE_USER,
    ANTHROPIC_ROLE_ASSISTANT,
} AnthropicMessageRole;

typedef struct {
    AnthropicMessageRole role;
    char *content;
} AnthropicMessage;

typedef struct {
    char *name;
    char *description;
    char *input_schema;
} AnthropicTool;

typedef struct {
    HttpHeader *headers;
    char *model;
    size_t max_tokens;
    AnthropicMessage *messages;
    size_t message_count;
    char *system;
    AnthropicTool *tools;
    size_t tool_count;
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
    size_t content_count;
    char *model;
    char *stop_reason;
    char *stop_sequence;
    AnthropicUsage usage;
} AnthropicResponse;

typedef struct {
    char *api_key;
    char *model;
    char *api_version;
    char *api_url;
    char *url_path;
    size_t max_tokens;
    FILE *error_stream;
    FILE *output_steam;
    
} AnthropicContext;

size_t serialize_request_body(char *body_buf, size_t buffer_len, AnthropicRequest *request);
AnthropicResponse *deserialize_response(JsonValue *json, FILE *error_stream);

AnthropicResponse *run_inference(char *api_key, char *model, int max_tokens, AnthropicMessage *messages, int message_count, FILE *error_stream);

void anthropic_complete_inference(void *context, const Conversation *conv, InferenceResponse out);

void Run(void);

#endif
