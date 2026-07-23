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
  char **tool_calls;
  size_t tool_call_count;
} InferenceResponse;

typedef struct {
    // some kind of state/context
    // function pointer for running inference?
    // function pointer for destroying provider instance?
    void *provider_context;
    void (*complete_inference)(void *context, const Conversation *conv, InferenceResponse out);
    void (*destroy_provider_context)(void *context);
} InferenceProvider;

#endif
