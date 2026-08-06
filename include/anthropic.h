#ifndef ANTHROPIC_H
#define ANTHROPIC_H

#include "json.h"
#include "http_client.h"
#include "provider.h"
#include <stdatomic.h>

typedef enum AnthropicMessageRole {
    ANTHROPIC_ROLE_SYSTEM,
    ANTHROPIC_ROLE_USER,
    ANTHROPIC_ROLE_ASSISTANT,
} AnthropicMessageRole;

typedef enum {
    ANTHROPIC_CONTENT_TEXT,
    ANTHROPIC_CONTENT_TOOL_USE,
    ANTHROPIC_CONTENT_TOOL_RESULT,
    ANTHROPIC_CONTENT_UNKNOWN,
} AnthropicContentType;

typedef struct {
    AnthropicContentType type;
    union {
        // typical message block
        struct { char *text; } text;
        // a tool use call returned by assistant
        // input will be a json value serialized to string
        struct { char *id; char *name; char *input; } tool_use;
        // message block representing the result of a tool call,
        // sent by the user back to the assistant
        // content will be a json array of content blocks, serialized to string.
        struct { char *tool_use_id; char *content; bool is_error; } tool_result;
    } as;
} AnthropicContent;

typedef struct  {
    AnthropicMessageRole role;
    AnthropicContent *content;
} AnthropicMessage;

typedef struct {
    char *name;
    char *description;
    char *input_schema;
} AnthropicTool;

typedef struct {
    char *type;
    bool disable_parallel_tool_use;
} AnthropicToolChoice;

typedef struct {
    HttpHeader *headers;
    char *model;
    size_t max_tokens;
    AnthropicMessage *messages;
    size_t message_count;
    char *system;
    AnthropicTool *tools;
    AnthropicToolChoice *tool_choice;
    size_t tool_count;
} AnthropicRequest;

typedef struct {
    size_t input_tokens;
    size_t output_tokens;
} AnthropicUsage;

typedef struct {
    char *type;
    char *message;
} AnthropicError;

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
    AnthropicError *error;
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

AnthropicContext *create_anthropic_context(char *api_key, char *model);
void free_anthropic_context(void *context);

size_t serialize_anthropic_request(char *body_buf, size_t buffer_len, AnthropicRequest *request);
AnthropicResponse *deserialize_anthropic_response(JsonValue *json, FILE *error_stream);
void free_anthropic_response(AnthropicResponse *resp);

AnthropicResponse *anthropic_run_inference(char *api_key, char *model, size_t max_tokens,
                                           AnthropicMessage *messages, size_t message_count,
                                           AnthropicTool  *tools, size_t tool_count,                                           FILE *error_stream);

// fulfills contract for provider-agnostic agent
void anthropic_complete_inference(void *context, const Conversation *conv, const ToolSet *tools, InferenceResponse *out);

void Run(void);

#endif
