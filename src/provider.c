#include "provider.h"
#include <stddef.h>
#include <string.h>

static void free_content_fields(Content *content) {
    if (NULL == content) {
        return;
    }

    switch (content->type) {
    case TEXT:
        free(content->as.text.text);
        break;
    case TOOL_CALL:
        free(content->as.tool_call.id);
        free(content->as.tool_call.name);
        free(content->as.tool_call.input);
        break;
    case TOOL_RESULT:
        free(content->as.tool_result.tool_use_id);
        free(content->as.tool_result.content);
        break;
    case CONTENT_TYPE_UNKNOWN:
        break;
    }
}

// returns the allocated size of the conversation if successful,
// -1 on failure.
int resize_conversation(Conversation *conv) {
    size_t cap = conv->message_capacity < DEFAULT_CONVERSATION_LEN ? DEFAULT_CONVERSATION_LEN
                                                                   : conv->message_capacity * 2;
    conv->message_capacity = cap;
    Message *tmp_messages = realloc(conv->messages, conv->message_capacity * sizeof(Message));
    if (NULL == tmp_messages) {
        return -1;
    }
    conv->messages = tmp_messages;
    return cap;
}

void free_conversation(Conversation *conv) {
    if (NULL == conv) {
        return;
    }

    if (NULL != conv->messages) {
        for (size_t i = 0; i < conv->message_count; i++) {
            Content *content_blocks = conv->messages[i].content_blocks;
            if (NULL == content_blocks) {
                continue;
            }

            for (size_t j = 0; j < conv->messages[i].content_count; j++) {
                free_content_fields(&content_blocks[j]);
            }
            free(content_blocks);
        }
    }

    free(conv->messages);
    free(conv);
}

int add_content_message_to_conv(Conversation *conv, const Content *content_blocks,
                                 size_t content_count, MessageRole role) {
    if (NULL == conv || NULL == content_blocks || 0 == content_count ||
        ROLE_UNKNOWN == role) {
        return -1;
    }
    if (NULL == conv->messages) {
        return -1;
    }

    if (conv->message_count >= conv->message_capacity && resize_conversation(conv) < 0) {
        return -1;
    }

    Message message = {.role = role,
                       .content_blocks = calloc(content_count, sizeof(Content)),
                       .content_count = content_count};
    if (NULL == message.content_blocks) {
        return -1;
    }

    for (size_t i = 0; i < content_count; i++) {
        const Content *source = &content_blocks[i];
        Content *destination = &message.content_blocks[i];
        destination->type = source->type;

        switch (source->type) {
        case TEXT:
            if (NULL == source->as.text.text) {
                goto cleanup;
            }
            destination->as.text.text = strdup(source->as.text.text);
            if (NULL == destination->as.text.text) {
                goto cleanup;
            }
            break;
        case TOOL_CALL:
            if (NULL == source->as.tool_call.id || NULL == source->as.tool_call.name ||
                NULL == source->as.tool_call.input) {
                goto cleanup;
            }
            destination->as.tool_call.id = strdup(source->as.tool_call.id);
            destination->as.tool_call.name = strdup(source->as.tool_call.name);
            destination->as.tool_call.input = strdup(source->as.tool_call.input);
            if (NULL == destination->as.tool_call.id ||
                NULL == destination->as.tool_call.name ||
                NULL == destination->as.tool_call.input) {
                goto cleanup;
            }
            break;
        case TOOL_RESULT:
            if (NULL == source->as.tool_result.tool_use_id ||
                NULL == source->as.tool_result.content) {
                goto cleanup;
            }
            destination->as.tool_result.tool_use_id =
                strdup(source->as.tool_result.tool_use_id);
            destination->as.tool_result.content = strdup(source->as.tool_result.content);
            destination->as.tool_result.is_error = source->as.tool_result.is_error;
            if (NULL == destination->as.tool_result.tool_use_id ||
                NULL == destination->as.tool_result.content) {
                goto cleanup;
            }
            break;
        case CONTENT_TYPE_UNKNOWN:
            goto cleanup;
        }
    }

    conv->messages[conv->message_count] = message;
    conv->message_count++;
    return 0;

cleanup:
    for (size_t i = 0; i < content_count; i++) {
        free_content_fields(&message.content_blocks[i]);
    }
    free(message.content_blocks);
    return -1;
}

int add_message_to_conv(Conversation *conv, const char *message, MessageRole role) {
    if (NULL == message) {
        return -1;
    }

    Content content = {.type = TEXT, .as.text.text = (char *)message};
    return add_content_message_to_conv(conv, &content, 1, role);
}
