#include "anthropic.h"
#include "http_client.h"
#include "json.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

// types below based on the documentation at https://platform.claude.com/docs/en/build-with-claude/working-with-messages

#define DEFAULT_MODEL "claude-haiku-4-5"
#define DEFAULT_MAX_TOKENS 2048
#define ANTHROPIC_VERSION "2023-06-01"
#define ANTHROPIC_URL "api.anthropic.com"
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

AnthropicResponse *deserialize_response(JsonValue *json) {
    AnthropicResponse *resp = calloc(1, sizeof(AnthropicResponse));
    if (NULL == resp) {
        return NULL;
    }
    // since we got here, we assume that the json was successfully
    // read from the http response and parsed to a json value
    for (size_t i = 0; i < json->as.object->count; i++) {
        char *key = json->as.object->pairs[i].key;
        JsonValue *value = json->as.object->pairs[i].value;
        if (0 == strcmp(key, "id")) {
            if (value->type == JSON_STRING) {
                resp->id = value->as.string;
            }
            continue;
        }
        if (0 == strcmp(key, "type")) {
            if (value->type == JSON_STRING) {
                resp->type = value->as.string;
            }
            continue;
        }
        if (0 == strcmp(key, "role")) {
            if (value->type == JSON_STRING) {
                resp->role = value->as.string;
            }
            continue;
        }
        if (0 == strcmp(key, "model")) {
            if (value->type == JSON_STRING) {
                resp->model = value->as.string;
            }
            continue;
        }
        if (0 == strcmp(key, "stop_reason")) {
            if (value->type == JSON_STRING) {
                resp->stop_reason = value->as.string;
            }
            continue;
        }
        if (0 == strcmp(key, "stop_sequence")) {
            if (value->type == JSON_STRING) {
                resp->stop_sequence = value->as.string;
            } else if (value->type == JSON_NULL) {
                resp->stop_sequence = NULL;
            }
            continue;
        }
        if (0 == strcmp(key, "content")) {
            if (value->type != JSON_ARRAY) {
                continue;
            }
            int content_count = value->as.array->count;
            if (content_count < 1) {
                continue;
            }
            resp->content = calloc(content_count, sizeof(AnthropicContent));
            resp->content_count = content_count;
            for (int j = 0; j < content_count; j++) {
                JsonValue item_val = value->as.array->items[j];
                if (item_val.type != JSON_OBJECT) {
                    continue;
                }
                JsonObject *msg_json = value->as.array->items[j].as.object;
                for (size_t k = 0; k < msg_json->count; k++) {
                    JsonPair current_pair = msg_json->pairs[k];
                    if (0 == strcmp(current_pair.key, "type")) {
                        if (current_pair.value->type == JSON_STRING) {
                            resp->content[j].type = current_pair.value->as.string;
                        }
                        continue;
                    }
                    if (0 == strcmp(current_pair.key, "text")) {
                        if (current_pair.value->type == JSON_STRING){
                            resp->content[j].text= current_pair.value->as.string;
                        }
                        continue;
                    }
                }
            }
            continue;
        }
        if (0 == strcmp(key, "usage")) {
            if (value->type != JSON_OBJECT) {
                continue;
            }
            JsonObject *msg_json = value->as.object;
            for (size_t j = 0; j < msg_json->count; j++) {
                JsonPair current_pair = msg_json->pairs[j];
                if (0 == strcmp(current_pair.key, "input_tokens")) {
                    if (current_pair.value->type == JSON_NUMBER) { 
                        resp->usage.input_tokens = current_pair.value->as.number;
                    }
                    continue;
                }
                if (0 == strcmp(current_pair.key, "output_tokens")) {
                    if (current_pair.value->type == JSON_NUMBER) { 
                        resp->usage.output_tokens = current_pair.value->as.number;
                    }
                    continue;
                }
            }
        }
    }

    return resp;
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
void runInference(char *model, int max_tokens, AnthropicMessage *messages, int message_count) {
    char *anthropic_api_key = getenv("ANTHROPIC_API_KEY");
    if (NULL == anthropic_api_key) {
        fprintf(stderr, "Failed to find the API Key from the expected env var %s\n", "ANTHROPIC_API_KEY");
        return;
    }

    HttpHeader headers[3] = {
        {
            .key = "x-api-key",
            .value = anthropic_api_key,
        },
        {
            .key = "anthropic-version",
            .value = ANTHROPIC_VERSION,
        },
        {
            .key = "content-type",
            .value = "application/json"
        }
    };


    AnthropicRequest request = {
        .headers = headers,
        .model = model,
        .max_tokens = max_tokens,
        .messages = messages,
        .message_count = message_count
    };

    char json_buf[8192];

    serialize_request_body(json_buf, 8192, &request);

    HTTPResponse *http_resp = https_request(HTTP_POST, ANTHROPIC_URL, "443", ANTHROPIC_MESSAGES_PATH, headers, 3, json_buf, stdout, stderr);
    if (NULL == http_resp) {
        fprintf(stderr, "Failed to get response from Anthropic API\n");
        return;
    }

    fprintf(stdout, "The reply says... \"%s\"\n", http_resp->body);

    Parser p;
    init_parser(&p, http_resp->body, stderr);

    JsonValue *v = parse_json(&p);

    AnthropicResponse *resp = deserialize_response(v);
    fprintf(stdout, "Claude says: \"%s\"", resp->content[0].text);
}



// prepare messages and pass thadem to runInference()
void Run(void) {
    AnthropicMessage messages[1] = {
        {
            .role = ROLE_USER,
            .content = "Howdy!"
        }
    };

    runInference(DEFAULT_MODEL, DEFAULT_MAX_TOKENS, messages, 1);
}
