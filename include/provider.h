#ifndef PROVIDER_H
#define PROVIDER_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_CONVERSATION_LEN 8

typedef enum {
    ROLE_UNKNOWN = 0,
    SYSTEM,
    USER,
    ASSISTANT,
} MessageRole;

typedef enum {
    CONTENT_TYPE_UNKNOWN = 0,
    TEXT,
    TOOL_CALL,
    TOOL_RESULT,
} MessageContentType;

typedef struct {
    char *name;
    char *description;
    char *input_schema;
} Tool;

typedef struct {
    Tool *tools;
    size_t tool_count;
} ToolSet;

typedef struct {
    char *tool_name;
    char *tool_args;
} ToolCall;

typedef struct {
    MessageContentType type;
    union {
        // typical message block
        struct { char *text; } text;
        // a tool use call returned by assistant
        // input will be a json value serialized to string
        struct { char *id; char *name; char *input; } tool_call;
        // message block representing the result of a tool call,
        // sent by the user back to the assistant
        // content will be a json array of content blocks, serialized to string.
        struct { char *tool_use_id; char *content; bool is_error; } tool_result;
    } as;
} Content;

typedef struct {
    MessageRole role;
    Content *content_blocks;
    size_t content_count;
} Message;

typedef struct {
    Message *messages;
    size_t message_count;
    size_t message_capacity;
} Conversation;


typedef struct {
    char *text;
    char *stop_reason;
    char *error_message;
    ToolCall *tool_calls;
    size_t tool_call_count;
} InferenceResponse;

typedef struct {
    // provider context for containing api keys and other provider-specific config.
    void *provider_context;
    // provider-specific code should fulfill this contract to map provider-specific responses
    // to our provider-agnostic structs.
    void (*complete_inference)(void *context, const Conversation *conv, const ToolSet *tools, InferenceResponse *out);
    // provider-specific context should provide a function for safely de-allocating
    void (*destroy_provider_context)(void *context);
} InferenceProvider;

// returns the allocated size of the conversation if successful,
// -1 on failure.
int resize_conversation(Conversation *conv);

void free_conversation(Conversation *conv);

int add_content_message_to_conv(Conversation *conv, const Content *content_blocks,
                                 size_t content_count, MessageRole role);

int add_message_to_conv(Conversation *conv, const char *message, MessageRole role);

#endif
