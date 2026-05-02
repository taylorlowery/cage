#include "anthropic.h"
#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>

#define DEFAULT_MODEL "claude-haiku-4-5"
#define DEFAULT_MAX_TOKENS 2048
#define ANTHROPIC_MESSAGES_URL "https://api.anthropic.com/v1/messages"

typedef enum MessageRole {
    ROLE_SYSTEM,
    ROLE_USER,
    ROLE_ASSISTANT,
} MessageRole;

typedef struct Message {
    MessageRole role;
    char *content;
} Message;

char *role_to_string(MessageRole role) {
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
void runInference(char *model, int max_tokens, struct Message *messages, int message_count) {
    char data[4096];
    // TODO: JSON Serialization rather than building a string.
    sprintf(data, "{\n\"model\": \"%s\",\n\"max_tokens\": %d,\n\"messages\": [{\"role\": \"user\": \"Howdy, Pilgrim!\"}],\n}", model, max_tokens);

    CURL *curl;
    CURLcode result;
    curl = curl_easy_init();
    if (NULL == curl) {
        fprintf(stderr, "Failed to initialize curl.\n");
        return;
    }

    // Set the URL to the Anthropic API messages endpoint
    curl_easy_setopt(curl, CURLOPT_URL, ANTHROPIC_MESSAGES_URL);

    // Add the require headers
    struct curl_slist *headers = NULL;
    // get API key from env
    char *anthropic_api_key = getenv("ANTHROPIC_API_KEY");
    if (NULL == anthropic_api_key) {
        fprintf(stderr, "ANTHROPIC_API_KEY environment variable not set.\n");
        return;
    }
    char headerBuf[1024];
    sprintf(headerBuf, "x-api-key: %s", anthropic_api_key);
    headers = curl_slist_append(headers, headerBuf);
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

    // perform curl
    result = curl_easy_perform(curl);

    fprintf(stdout, "%s\n", headers->data);

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}

// prepare messages and pass thadem to runInference()
void Run(void) {
    Message messages[1];
    Message message = {
        .role = ROLE_USER,
        .content = "Howdy, Pilgrim!"
    };
    messages[0] = message;
    runInference(
        DEFAULT_MODEL,
        DEFAULT_MAX_TOKENS,
        messages,
        1
    );
}
