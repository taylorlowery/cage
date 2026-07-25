#include "provider.h"
#include <string.h>

// returns the allocated size of the conversation if successful,
// -1 on failure.
int resize_conversation(Conversation *conv) {
  size_t cap = conv->message_capacity < DEFAULT_CONVERSATION_LEN ? DEFAULT_CONVERSATION_LEN : conv->message_capacity * 2;
  conv->message_capacity = cap;
  Message *tmp_messages = realloc(conv->messages, conv->message_capacity * sizeof(Message));
  if (NULL == tmp_messages) {
    return -1;
  }
  conv->messages = tmp_messages;
  return cap;
}

void free_conversation(Conversation *conv) {
  for (size_t i = 0; i < conv->message_count; i++) {
    free(conv->messages[i].message);
  }
  free(conv->messages);
  free(conv);
}

void add_message_to_conv(Conversation *conv, char *message, MessageRole role) {
    Message msg = {
      .role = role,
      .message = strdup(message)
    };
   conv->messages[conv->message_count] = msg;
   conv->message_count++;
   if (conv->message_count > conv->message_capacity) {
       resize_conversation(conv);
   }
}
