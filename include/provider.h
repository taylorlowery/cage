#ifndef PROVIDER_H
#define PROVIDER_H

#include <stdlib.h>

typedef enum {
  SYSTEM,
  USER,
  ASSISTANT
} MessageRole;

typedef struct {
  MessageRole role;
  char *message;
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
  char **tool_calls;
  size_t tool_call_count;
} InferenceResponse;

typedef struct {
    // provider context for containing api keys and other provider-specific config.
    void *provider_context;
    // provider-specific code should fulfill this contract to map provider-specific responses
    // to our provider-agnostic structs.
    void (*complete_inference)(void *context, const Conversation *conv, InferenceResponse *out);
    // provider-specific context should provide a function for safely de-allocating
    void (*destroy_provider_context)(void *context);
} InferenceProvider;

#endif
