#include "anthropic.h"
#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>

// types below based on the documentation at https://platform.claude.com/docs/en/build-with-claude/working-with-messages

#define DEFAULT_MODEL "claude-haiku-4-5"
#define DEFAULT_MAX_TOKENS 2048
#define ANTHROPIC_VERSION "2023-06-01"
#define ANTHROPIC_URL "https://api.anthropic.com"
#define ANTHROPIC_MESSAGES_PATH "/v1/messages"
#define REQUEST_BUFFER_LEN 8192

char *role_to_string(AnthropicMessageRole role) {
    switch(role) {
        case ROLE_SYSTEM:
            return "system";
        case ROLE_USER:
            return "user";
        case ROLE_ASSISTANT:
            return "assistant";
        default:
            return "unsupported role";
    }
}


// serialize_request_body writes the contents of an AnthropicRequest
// to a buffer as a JSON string.
size_t serialize_request_body(char *body_buf, size_t buffer_len, AnthropicRequest *request){
    size_t cursor = snprintf(
        body_buf,
        buffer_len,
        "{\"model\": \"%s\", \"max_tokens\": %zu, ",
        request->model,
        request->max_tokens
    );

    if (0 < request->message_count) {
        cursor += snprintf(
            body_buf + cursor,
            buffer_len - cursor,
            "\"messages\": ["
        );

        for (size_t i = 0; i < request->message_count; i++) {
            AnthropicMessage message = request->messages[i];
            cursor += snprintf(
                body_buf + cursor,
                buffer_len - cursor,
                "{ \"role\": \"%s\", \"content\": \"%s\" }",
                role_to_string(message.role),
                message.content
            );
            // follow all but last message with comma
            if (i < request->message_count - 1) {
                cursor += snprintf(body_buf + cursor, buffer_len - cursor, ", ");
            }
            // TODO: resize buffer if necessary? error if we go over?
        }
        
        cursor += snprintf(
            body_buf + cursor,
            buffer_len - cursor,
            "]"
        );
    }

    cursor += snprintf(
        body_buf + cursor,
        buffer_len - cursor,
        "}"
    );

    return cursor;
}



// Essentially recreating this curl:
// curl https://api.anthropic.com/v1/messages \
//      --header "x-api-key: $ANTHROPIC_API_KEY" \
//      --header "anthropic-version: 2023-06-01" \
//      --header "content-type: application/json" \
//      --data \
// '{
//     "model": "claude-opus-4-6",
//     "max_tokens": 1024,
//     "messages": [
//         {"role": "user", "content": "Hello, Claude"}
//     ]
// }'
// return the latest response from the API.
// caller is responsible for freeing it.
void runInference(char *model, int max_tokens, struct AnthropicMessage *messages, int message_count) {
//     char data[4096];
//     // TODO: JSON Serialization rather than building a string.
//     sprintf(data, "{\n\"model\": \"%s\",\n\"max_tokens\": %d,\n\"messages\": [{\"role\": \"user\": \"Howdy, Pilgrim!\"}],\n}", model, max_tokens);

//     CURL *curl;
//     CURLcode result;
//     curl = curl_easy_init();
//     if (NULL == curl) {
//         fprintf(stderr, "Failed to initialize curl.\n");
//         return;
//     }

//     // Set the URL to the Anthropic API messages endpoint
//     curl_easy_setopt(curl, CURLOPT_URL, ANTHROPIC_MESSAGES_URL);

//     // Add the require headers
//     struct curl_slist *headers = NULL;
//     // get API key from env
//     char *anthropic_api_key = getenv("ANTHROPIC_API_KEY");
//     if (NULL == anthropic_api_key) {
//         fprintf(stderr, "ANTHROPIC_API_KEY environment variable not set.\n");
//         return;
//     }
//     char headerBuf[1024];
//     sprintf(headerBuf, "x-api-key: %s", anthropic_api_key);
//     headers = curl_slist_append(headers, headerBuf);
//     headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
//     headers = curl_slist_append(headers, "content-type: application/json");
//     curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

//     curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

//     // perform curl
//     result = curl_easy_perform(curl);

//     fprintf(stdout, "%s\n", headers->data);

//     curl_easy_cleanup(curl);
//     curl_slist_free_all(headers);
}

// prepare messages and pass thadem to runInference()
void Run(void) {}
