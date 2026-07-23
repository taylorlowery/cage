#include "anthropic.h"
#include "http_client.h"
#include "json.h"
#include "provider.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// types below based on the documentation at https://platform.claude.com/docs/en/build-with-claude/working-with-messages

#define DEFAULT_MODEL "claude-haiku-4-5"
#define DEFAULT_MAX_TOKENS 2048
#define ANTHROPIC_VERSION "2023-06-01"
#define ANTHROPIC_URL "api.anthropic.com"
#define ANTHROPIC_MESSAGES_PATH "/v1/messages"
#define REQUEST_BUFFER_LEN 8192

static int copy_string(const char *src, char **dest, const char *field, FILE *error_stream) {
    *dest = strdup(src);
    if (NULL == *dest) {
        fprintf(error_stream, "failed to deserialize field '%s'\n", field);
        return -1;
    }
    return 0;
}

const char *role_to_string(AnthropicMessageRole role) {
    switch(role) {
        case ANTHROPIC_ROLE_SYSTEM:
            return "system";
        case ANTHROPIC_ROLE_USER:
            return "user";
        case ANTHROPIC_ROLE_ASSISTANT:
            return "assistant";
        default:
            return "unsupported role";
    }
}


// serialize_request_body writes the contents of an AnthropicRequest
// to a buffer as a JSON string.
size_t serialize_anthropic_request(char *body_buf, size_t buffer_len, AnthropicRequest *request){
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


void free_anthropic_response(AnthropicResponse *resp) {
    if (NULL == resp) {
        // mission accomplished
        return;
    }
    free(resp->id);
    free(resp->type);
    free(resp->role);
    free(resp->model);
    free(resp->stop_reason);
    free(resp->stop_sequence);
    if (NULL != resp->content) {
        for (size_t i = 0; i < resp->content_count; i++) {
            free(resp->content[i].type);
            free(resp->content[i].text);
        }
    }
    if (NULL != resp->error) {
        free(resp->error->message);
        free(resp->error->type);
        free(resp);
    }
    free(resp->content);
    free(resp);
}

AnthropicResponse *deserialize_anthropic_response(JsonValue *json, FILE *error_stream) {
    if (NULL == json || JSON_OBJECT != json->type || NULL == error_stream) {
        return NULL;
    }

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
            if (value->type == JSON_STRING &&
                0 != copy_string(value->as.string, &resp->id, "id", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "type")) {
            if (value->type == JSON_STRING &&
                0 != copy_string(value->as.string, &resp->type, "type", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "role")) {
            if (value->type == JSON_STRING &&
                0 != copy_string(value->as.string, &resp->role, "role", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "model")) {
            if (value->type == JSON_STRING &&
                0 != copy_string(value->as.string, &resp->model, "model", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "stop_reason")) {
            if (value->type == JSON_STRING &&
                0 != copy_string(value->as.string, &resp->stop_reason,
                                 "stop_reason", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "stop_sequence")) {
            if (value->type == JSON_STRING) {
                if (0 != copy_string(value->as.string, &resp->stop_sequence,
                                     "stop_sequence", error_stream)) {
                    goto cleanup;
                }
            } else if (value->type == JSON_NULL) {
                resp->stop_sequence = NULL;
            }
            continue;
        }
        if (0 == strcmp(key, "content")) {
            if (value->type != JSON_ARRAY) {
                continue;
            }
            size_t content_count = value->as.array->count;
            if (content_count < 1) {
                continue;
            }
            resp->content = calloc(content_count, sizeof(AnthropicContent));
            if (NULL == resp->content) {
                fprintf(error_stream, "failed to allocate response content\n");
                goto cleanup;
            }
            resp->content_count = content_count;
            for (size_t j = 0; j < content_count; j++) {
                JsonValue item_val = value->as.array->items[j];
                if (item_val.type != JSON_OBJECT) {
                    continue;
                }
                JsonObject *msg_json = value->as.array->items[j].as.object;
                for (size_t k = 0; k < msg_json->count; k++) {
                    JsonPair current_pair = msg_json->pairs[k];
                    if (0 == strcmp(current_pair.key, "type")) {
                        if (current_pair.value->type == JSON_STRING &&
                            0 != copy_string(current_pair.value->as.string,
                                             &resp->content[j].type,
                                             "content.type", error_stream)) {
                            goto cleanup;
                        }
                        continue;
                    }
                    if (0 == strcmp(current_pair.key, "text")) {
                        if (current_pair.value->type == JSON_STRING &&
                            0 != copy_string(current_pair.value->as.string,
                                             &resp->content[j].text,
                                             "content.text", error_stream)) {
                            goto cleanup;
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
        if (0 == strcmp(key, "error")) {
            if (value->type != JSON_OBJECT) {
                continue;
            }
            resp->error = calloc(1, sizeof(AnthropicError));
            if (NULL == resp->error) {
                fprintf(error_stream, "failed to allocate space for error\n");
                goto cleanup;
            }
            JsonObject *error_json = value->as.object;
            for (size_t j = 0; j < error_json->count; j++) {
                JsonPair current_pair = error_json->pairs[j];
                if (0 == strcmp("type", current_pair.key)) {
                    if (JSON_STRING == current_pair.value->type) {
                        resp->error->type = strdup(current_pair.value->as.string);
                    }
                    continue;
                }
                if (0 == strcmp("message", current_pair.key)) {
                    if (JSON_STRING == current_pair.value->type) {
                        resp->error->message = strdup(current_pair.value->as.string);
                    }
                    continue;
                }
            }
        }
    }

    return resp;

cleanup:
    free_anthropic_response(resp);
    return NULL;
}



// TODO: get this out of its very rough state.
// Currently this is MVP for validating our parser, lexer, and http_client.
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
AnthropicResponse *anthropic_run_inference(char *api_key, char *model, int max_tokens, AnthropicMessage *messages, int message_count, FILE *error_stream) {
    if (NULL == api_key) {
        fprintf(error_stream, "no anthropic api ke provided\n");
        return NULL;
    }
    if (NULL == model) {
        fprintf(error_stream, "no model provided\n");
        return NULL;
    }

    HttpHeader headers[3] = {
        {
            .key = "x-api-key",
            .value = api_key,
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

    serialize_anthropic_request(json_buf, 8192, &request);

    HTTPResponse *http_resp = NULL;
    JsonValue *v = NULL;

    http_resp = https_request(HTTP_POST, ANTHROPIC_URL, "443", ANTHROPIC_MESSAGES_PATH, headers, 3, json_buf, stdout, stderr);
    if (NULL == http_resp) {
        fprintf(stderr, "Failed to get response from Anthropic API\n");
        goto cleanup;
    }

    Parser p;
    init_parser(&p, http_resp->body, stderr);

    v = parse_json(&p);
    if (NULL == v) {
        fprintf(stderr, "failed to parse response to json\n");
        goto cleanup;
    }

    AnthropicResponse *resp = deserialize_anthropic_response(v, stderr);
    if (NULL == resp) {
        fprintf(stderr, "failed to deserialize response\n");
        goto cleanup;
    }

    free_json_value(v);
    free_http_response(http_resp);

    return resp;

cleanup:
    if (NULL != v) {
        free_json_value(v);
    }
    if (NULL != http_resp) {
        free_http_response(http_resp);
    }
    return NULL;
}



// function to map agent conversation to anthropic conversation.
// must be freed by caller.
AnthropicMessage *agent_messages_to_anthropic_messages(const Conversation *conv) {
    if (NULL == conv->messages || conv->message_count == 0) {
        return NULL;
    }
    AnthropicMessage *anthropic_messages = calloc(conv->message_count, sizeof(AnthropicMessage));
    for (size_t i = 0; i < conv->message_count; i++) {
        anthropic_messages[i].content = conv->messages[i].message;
        switch (conv->messages[i].role) {
            case USER:
                anthropic_messages[i].role = ANTHROPIC_ROLE_USER;
                break;
            case ASSISTANT:
                anthropic_messages[i].role = ANTHROPIC_ROLE_ASSISTANT;
                break;
            case SYSTEM:
                anthropic_messages[i].role = ANTHROPIC_ROLE_SYSTEM;
                break;
        }
    }

    return anthropic_messages;
}

AnthropicContext *create_anthropic_context(char *api_key, char *model) {
    if (NULL == api_key) {
        char *anthropic_api_key = getenv("ANTHROPIC_API_KEY");
        if (NULL == anthropic_api_key) {
            fprintf(stderr, "Failed to find the API Key from the expected env var %s\n", "ANTHROPIC_API_KEY");
            return NULL;
        }
        api_key = strdup(anthropic_api_key);
    }

    if (NULL == model) {
        model = strdup(DEFAULT_MODEL);
    }

    AnthropicContext *a = calloc(1, sizeof(AnthropicContext));
    if (NULL == a) {
        fprintf(stderr, "failed to allocate space for anthropic context\n");
        return NULL;
    }

    a->api_key = api_key;
    a->model = model;
    a->api_version = strdup(ANTHROPIC_VERSION);
    a->api_url = strdup(ANTHROPIC_URL);
    a->url_path = strdup(ANTHROPIC_MESSAGES_PATH);
    a->max_tokens = DEFAULT_MAX_TOKENS;
    a->error_stream = stderr;
    a->output_steam = stdout;

    return a;
}

void anthropic_complete_inference (void *context, const Conversation *conv, InferenceResponse *out) {
    if (NULL == context) {
        // TODO: error somehow
        return;
    }
    AnthropicContext *anthropic_ctx = context;

    AnthropicMessage *anthropic_messages = agent_messages_to_anthropic_messages(conv);
    if (NULL == anthropic_messages) {
        // TODO: error somehow
        return;
    }

    AnthropicResponse *resp = anthropic_run_inference(anthropic_ctx->api_key, anthropic_ctx->model, anthropic_ctx->max_tokens, anthropic_messages, conv->message_count, stdout);

    if (NULL == resp) {
        // TODO: error somehow
        return;
    }

    if (0 == strcmp("error", resp->type)) {
        if (NULL == resp->error) {
            fprintf(stderr, "ERROR WAS NULL?!\n");
            free_anthropic_response(resp);
            return;
        }

        fprintf(stderr, "%s: %s\n", resp->error->type, resp->error->message);
        free_anthropic_response(resp);
        return;
    }

    if (NULL != resp->content && resp->content_count > 0) {
        out->text = resp->content->text ? strdup(resp->content->text) : NULL;
    }
    out->stop_reason = resp->stop_reason ? strdup(resp->stop_reason) : NULL;

    free_anthropic_response(resp);
    // todo: figure out tool calls (need to see a tool call response)
}


void destroy_anthropic_context (void *context) {
    if (NULL == context) {
        // TODO: is it chill of me to assume stderr?
        fprintf(stderr, "null context passed to destroy function\n");
        return;
    }
    AnthropicContext *c = context;
    free(c->api_key);
    free(c->model);
    free(c->api_version);
    free(c->api_url);
    free(c->url_path);
    free(c);
}
//
// TODO: remove this function, which exists to easily test
// preparing messages and pass them to run_inference()
void Run(void) {
    AnthropicContext *ctx = create_anthropic_context(NULL, NULL);
    Message messages[1] = {
        {
            .role = USER,
            .message = "Howdy!"
        }
    };
    Conversation conv = {
        .messages = messages,
        .message_capacity = 1,
        .message_count = 1,
    };

    InferenceResponse *resp = calloc(1, sizeof(InferenceResponse));

    anthropic_complete_inference(ctx, &conv, resp);

    fprintf(stdout, "Claude replies: %s\n", resp->text);
}
