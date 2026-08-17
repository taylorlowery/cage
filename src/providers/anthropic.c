#include "anthropic.h"
#include "http_client.h"
#include "json.h"
#include "provider.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// types below based on the documentation at
// https://platform.claude.com/docs/en/build-with-claude/working-with-messages

#define DEFAULT_MODEL "claude-haiku-4-5"
#define DEFAULT_MAX_TOKENS 2048
#define ANTHROPIC_VERSION "2023-06-01"
#define ANTHROPIC_URL "api.anthropic.com"
#define ANTHROPIC_MESSAGES_PATH "/v1/messages"
#define REQUEST_BUFFER_LEN 8192

static int copy_string(const char *src, char **dest) {
    if (NULL == src) {
        return -1;
    }
    *dest = strdup(src);
    if (NULL == *dest) {
        return -1;
    }
    return 0;
}

static int copy_field(const char *src, char **dest, const char *field, FILE *error_stream) {
    int err = copy_string(src, dest);
    if (0 != err) {
        fprintf(error_stream, "failed to deserialize field '%s'\n", field);
    }
    return err;
}

const char *role_to_string(AnthropicMessageRole role) {
    switch (role) {
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

// serialize_anthropic_request writes an Anthropic Messages API request to body_buf.
// The content-block structure follows Anthropic's tool-use documentation:
// https://platform.claude.com/docs/en/agents-and-tools/tool-use/build-a-tool-using-agent
//
// Representative output:
// {
//   "model": "claude-opus-4-8",
//   "max_tokens": 1024,
//   "tools": [{
//     "name": "read_file",
//     "description": "Read the contents of a relative file path.",
//     "input_schema": {
//       "type": "object",
//       "properties": {"path": {"type": "string"}},
//       "required": ["path"]
//     }
//   }],
//   "messages": [
//     {
//       "role": "user",
//       "content": [{"type": "text", "text": "Read README.md"}]
//     },
//     {
//       "role": "assistant",
//       "content": [{
//         "type": "tool_use",
//         "id": "toolu_123",
//         "name": "read_file",
//         "input": {"path": "README.md"}
//       }]
//     },
//     {
//       "role": "user",
//       "content": [{
//         "type": "tool_result",
//         "tool_use_id": "toolu_123",
//         "content": "file contents",
//         "is_error": false
//       }]
//     }
//   ]
// }
int serialize_anthropic_request(StringBuilder *body_buf, AnthropicRequest *request) {
    if (NULL == body_buf || NULL == request || NULL == request->model) {
        return -1;
    }

    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, "{\"model\": ",
                                                strlen("{\"model\": "))) {
        return -1;
    }
    if (0 != append_json_string_to_stringbuilder_buffer(body_buf, request->model)) {
        return -1;
    }
    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, ", \"max_tokens\": ",
                                                strlen(", \"max_tokens\": "))) {
        return -1;
    }

    char max_tokens_buf[32];
    int max_tokens_len = snprintf(max_tokens_buf, sizeof(max_tokens_buf), "%zu",
                                  request->max_tokens);
    if (max_tokens_len < 0 || (size_t)max_tokens_len >= sizeof(max_tokens_buf) ||
        0 != append_bytes_to_stringbuilder_buffer(body_buf, max_tokens_buf,
                                               (size_t)max_tokens_len)) {
        return -1;
    }

    if (request->message_count > 0) {
        if (NULL == request->messages ||
            0 != append_bytes_to_stringbuilder_buffer(body_buf, ", \"messages\": [",
                                                    strlen(", \"messages\": ["))) {
            return -1;
        }

        for (size_t i = 0; i < request->message_count; i++) {
            AnthropicMessage *message = &request->messages[i];
            if (message->role == ANTHROPIC_ROLE_UNKNOWN ||
                NULL == message->content_blocks || message->content_count == 0) {
                return -1;
            }

            if (0 != append_bytes_to_stringbuilder_buffer(body_buf, "{ \"role\": ",
                                                        strlen("{ \"role\": "))) {
                return -1;
            }
            if (0 != append_json_string_to_stringbuilder_buffer(body_buf,
                                                              role_to_string(message->role)) ||
                0 != append_bytes_to_stringbuilder_buffer(body_buf, ", \"content\": [",
                                                        strlen(", \"content\": ["))) {
                return -1;
            }

            for (size_t j = 0; j < message->content_count; j++) {
                AnthropicContent *content = &message->content_blocks[j];
                int err = 0;

                if (j > 0) {
                    err = append_bytes_to_stringbuilder_buffer(body_buf, ", ", 2);
                }
                if (0 != err) {
                    return -1;
                }

                switch (content->type) {
                case ANTHROPIC_CONTENT_TEXT:
                    if (NULL == content->as.text.text) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(
                                 body_buf, "{ \"type\": \"text\", \"text\": ",
                                 strlen("{ \"type\": \"text\", \"text\": "))) {
                        return -1;
                    }
                    if (0 != append_json_string_to_stringbuilder_buffer(body_buf,
                                                                       content->as.text.text)) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, " }", 2)) {
                        return -1;
                    }
                    break;
                case ANTHROPIC_CONTENT_TOOL_USE:
                    if (NULL == content->as.tool_use.id) {
                        return -1;
                    }
                    if (NULL == content->as.tool_use.name) {
                        return -1;
                    }
                    if (NULL == content->as.tool_use.input) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(
                                 body_buf, "{ \"type\": \"tool_use\", \"id\": ",
                                 strlen("{ \"type\": \"tool_use\", \"id\": "))) {
                        return -1;
                    }
                    if (0 != append_json_string_to_stringbuilder_buffer(body_buf,
                                                                       content->as.tool_use.id)) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, ", \"name\": ",
                                                                strlen(", \"name\": "))) {
                        return -1;
                    }
                    if (0 != append_json_string_to_stringbuilder_buffer(body_buf,
                                                                       content->as.tool_use.name)) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, ", \"input\": ",
                                                                strlen(", \"input\": "))) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(body_buf,
                                                                content->as.tool_use.input,
                                                                strlen(content->as.tool_use.input))) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, " }", 2)) {
                        return -1;
                    }
                    break;
                case ANTHROPIC_CONTENT_TOOL_RESULT:
                    if (NULL == content->as.tool_result.tool_use_id) {
                        return -1;
                    }
                    if (NULL == content->as.tool_result.content) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(
                                 body_buf, "{ \"type\": \"tool_result\", \"tool_use_id\": ",
                                 strlen("{ \"type\": \"tool_result\", \"tool_use_id\": "))) {
                        return -1;
                    }
                    if (0 != append_json_string_to_stringbuilder_buffer(
                                 body_buf, content->as.tool_result.tool_use_id)) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, ", \"is_error\": ",
                                                                strlen(", \"is_error\": "))) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(
                                 body_buf,
                                 content->as.tool_result.is_error ? "true" : "false",
                                 content->as.tool_result.is_error ? 4 : 5)) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, ", \"content\": ",
                                                                strlen(", \"content\": "))) {
                        return -1;
                    }
                    if (0 != append_json_string_to_stringbuilder_buffer(
                                 body_buf, content->as.tool_result.content)) {
                        return -1;
                    }
                    if (0 != append_bytes_to_stringbuilder_buffer(body_buf, " }", 2)) {
                        return -1;
                    }
                    break;
                default:
                    return -1;
                }
            }

            if (0 != append_bytes_to_stringbuilder_buffer(body_buf, "]}", 2)) {
                return -1;
            }
            if (i + 1 < request->message_count &&
                0 != append_bytes_to_stringbuilder_buffer(body_buf, ", ", 2)) {
                return -1;
            }
        }

        if (0 != append_char_to_stringbuilder_buffer(body_buf, ']')) {
            return -1;
        }
    }

    if (request->tool_count > 0) {
        if (NULL == request->tools) {
            return -1;
        }
        if (0 != append_bytes_to_stringbuilder_buffer(body_buf, ", \"tools\": [", strlen(", \"tools\": ["))) {
            return -1;
        }
        for (size_t i = 0; i < request->tool_count; i++) {
            if (NULL == request->tools[i].name || NULL == request->tools[i].description || NULL == request->tools[i].input_schema) {
                return -1;
            }
            if (0 != append_char_to_stringbuilder_buffer(body_buf, '{')) {
                return -1;
            }
            if (0 != append_bytes_to_stringbuilder_buffer(body_buf, "\"name\": ", strlen("\"name\": "))) {
                return -1;
            }
            if (0 != append_json_string_to_stringbuilder_buffer(body_buf, request->tools[i].name)) {
                return -1;
            }
            if (0 != append_char_to_stringbuilder_buffer(body_buf, ',')) {
                return -1;
            }
            if (0 != append_bytes_to_stringbuilder_buffer(body_buf, "\"description\": ", strlen("\"description\": "))) {
                return -1;
            }
            if (0 != append_json_string_to_stringbuilder_buffer(body_buf, request->tools[i].description)) {
                return -1;
            }
            if (0 != append_char_to_stringbuilder_buffer(body_buf, ',')) {
                return -1;
            }
            if (0 != append_bytes_to_stringbuilder_buffer(body_buf, "\"input_schema\": ", strlen("\"input_schema\": "))) {
                return -1;
            }
            if (0 != append_bytes_to_stringbuilder_buffer(body_buf, request->tools[i].input_schema, strlen(request->tools[i].input_schema))) {
                return -1;
            }
            if (0 != append_char_to_stringbuilder_buffer(body_buf, '}')) {
                return -1;
            }
            if (i < request->tool_count - 1) {
                if (0 != append_char_to_stringbuilder_buffer(body_buf, ',')) {
                    return -1;
                }
            }
        }
        if (0 != append_char_to_stringbuilder_buffer(body_buf, ']')) {
            return -1;
        }
    }

    return append_char_to_stringbuilder_buffer(body_buf, '}');
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
            switch (resp->content[i].type) {
            case ANTHROPIC_CONTENT_TEXT:
                free(resp->content[i].as.text.text);
                break;
            case ANTHROPIC_CONTENT_TOOL_USE:
                free(resp->content[i].as.tool_use.id);
                free(resp->content[i].as.tool_use.name);
                free(resp->content[i].as.tool_use.input);
                break;
            case ANTHROPIC_CONTENT_TOOL_RESULT:
                free(resp->content[i].as.tool_result.tool_use_id);
                free(resp->content[i].as.tool_result.content);
                break;
            case ANTHROPIC_CONTENT_UNKNOWN:
                break;
            }
        }
    }
    if (NULL != resp->error) {
        free(resp->error->message);
        free(resp->error->type);
        free(resp->error);
    }
    if (NULL != resp->content) {
        free(resp->content);
    }
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
                0 != copy_field(value->as.string, &resp->id, "id", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "type")) {
            if (value->type == JSON_STRING &&
                0 != copy_field(value->as.string, &resp->type, "type", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "role")) {
            if (value->type == JSON_STRING &&
                0 != copy_field(value->as.string, &resp->role, "role", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "model")) {
            if (value->type == JSON_STRING &&
                0 != copy_field(value->as.string, &resp->model, "model", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "stop_reason")) {
            if (value->type == JSON_STRING && 0 != copy_field(value->as.string, &resp->stop_reason,
                                                               "stop_reason", error_stream)) {
                goto cleanup;
            }
            continue;
        }
        if (0 == strcmp(key, "stop_sequence")) {
            if (value->type == JSON_STRING) {
                if (0 != copy_field(value->as.string, &resp->stop_sequence, "stop_sequence",
                                     error_stream)) {
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
                JsonObject *msg_json = item_val.as.object;
                AnthropicContentType block_type = ANTHROPIC_CONTENT_UNKNOWN;
                for (size_t k = 0; k < msg_json->count; k++) {
                    JsonPair current_pair = msg_json->pairs[k];
                    if (0 == strcmp(current_pair.key, "type")) {
                        if (current_pair.value->type == JSON_STRING) {
                            if (0 == strcmp(current_pair.value->as.string, "text")) {
                                block_type = ANTHROPIC_CONTENT_TEXT;
                            } else if (0 == strcmp(current_pair.value->as.string, "tool_use")) {
                                block_type = ANTHROPIC_CONTENT_TOOL_USE;
                            } else if (0 == strcmp(current_pair.value->as.string, "tool_result")) {
                                block_type = ANTHROPIC_CONTENT_TOOL_RESULT;
                            } else {
                                fprintf(error_stream, "unsupported content type: '%s'\n", current_pair.value->as.string);
                                goto cleanup;
                            }
                            resp->content[j].type = block_type;
                            continue;
                        } else {
                            fprintf(error_stream, "content type was not a string\n");
                            goto cleanup;
                        }
                    }
                }
                for (size_t k = 0; k < msg_json->count; k++) {
                    JsonPair current_pair = msg_json->pairs[k];
                    switch (block_type) {
                        case ANTHROPIC_CONTENT_TEXT:
                            if (0 == strcmp(current_pair.key, "text")) {
                                if (current_pair.value->type != JSON_STRING) {
                                    fprintf(error_stream, "text was not a string\n");
                                    goto cleanup;
                                }
                                if (0 != copy_field(current_pair.value->as.string, &resp->content[j].as.text.text,
                                                "content.text", error_stream)) {
                                    goto cleanup;
                                }
                            }
                            break;
                        case ANTHROPIC_CONTENT_TOOL_USE:
                            if (0 == strcmp(current_pair.key, "id")) {
                                if (current_pair.value->type != JSON_STRING ||
                                    0 != copy_field(current_pair.value->as.string,
                                                     &resp->content[j].as.tool_use.id,
                                                     "content.tool_use.id", error_stream)) {
                                    fprintf(error_stream, "tool_use id was not a string\n");
                                    goto cleanup;
                                }
                            } else if (0 == strcmp(current_pair.key, "name")) {
                                if (current_pair.value->type != JSON_STRING ||
                                    0 != copy_field(current_pair.value->as.string,
                                                     &resp->content[j].as.tool_use.name,
                                                     "content.tool_use.name", error_stream)) {
                                    fprintf(error_stream, "tool_use name was not a string\n");
                                    goto cleanup;
                                }
                            } else if (0 == strcmp(current_pair.key, "input")) {
                                if (JSON_OBJECT != current_pair.value->type) {
                                    fprintf(error_stream,
                                            "tool_use input must currently be a string\n");
                                    goto cleanup;
                                }
                                char *input_json = json_value_to_string(current_pair.value);
                                if (NULL == input_json) {
                                    fprintf(error_stream,
                                            "tool_use input must currently be a string\n");
                                    goto cleanup;
                                }
                                resp->content[j].as.tool_use.input = input_json;
                            }
                            break;
                        case ANTHROPIC_CONTENT_TOOL_RESULT:
                            if (0 == strcmp(current_pair.key, "tool_use_id")) {
                                if (current_pair.value->type != JSON_STRING ||
                                    0 != copy_field(current_pair.value->as.string,
                                                     &resp->content[j].as.tool_result.tool_use_id,
                                                     "content.tool_result.tool_use_id", error_stream)) {
                                    fprintf(error_stream, "tool_result tool_use_id was not a string\\n");
                                    goto cleanup;
                                }
                            } else if (0 == strcmp(current_pair.key, "content")) {
                                if (current_pair.value->type != JSON_STRING ||
                                    0 != copy_field(current_pair.value->as.string,
                                                     &resp->content[j].as.tool_result.content,
                                                     "content.tool_result.content", error_stream)) {
                                    fprintf(error_stream, "tool_result content was not a string\\n");
                                    goto cleanup;
                                }
                            } else if (0 == strcmp(current_pair.key, "is_error")) {
                                if (current_pair.value->type != JSON_BOOL) {
                                    fprintf(error_stream, "tool_result is_error was not a boolean\\n");
                                    goto cleanup;
                                }
                                resp->content[j].as.tool_result.is_error =
                                    current_pair.value->as.boolean;
                            }
                            break;
                        default:
                            fprintf(error_stream, "unsupported content type\n");
                            goto cleanup;
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

void free_anthropic_content_internal(AnthropicContent *content) {
    if (NULL == content) {
        return;
    }
    switch (content->type) {
        case ANTHROPIC_CONTENT_TEXT:
            free(content->as.text.text);
            break;
        case ANTHROPIC_CONTENT_TOOL_USE:
            free(content->as.tool_use.id);
            free(content->as.tool_use.name);
            free(content->as.tool_use.input);
            break;
        case ANTHROPIC_CONTENT_TOOL_RESULT:
            free(content->as.tool_result.tool_use_id);
            free(content->as.tool_result.content);
            break;
        default:
            break;
    }
}

void free_anthropic_content(AnthropicContent *content) {
    if (NULL == content) {
        return;
    }
    free_anthropic_content_internal(content);
    free(content);
}

void free_anthropic_message(AnthropicMessage *message) {
    if (NULL == message) {
        return;
    }
    for (size_t i = 0; i < message->content_count; i++) {
        free_anthropic_content_internal(&message->content_blocks[i]);
    }
    free(message->content_blocks);
    free(message);
}



void free_anthropic_tool(AnthropicTool *tool) {
    if (NULL == tool) {
        return;
    }
    free(tool->name);
    free(tool->description);
    free(tool->input_schema);
    free(tool);
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
AnthropicResponse *anthropic_run_inference(char *api_key, char *model, size_t max_tokens,
                                           AnthropicMessage *messages, size_t message_count,
                                           AnthropicTool  *tools, size_t tool_count,
                                           FILE *error_stream) {
    if (NULL == api_key) {
        fprintf(error_stream, "no anthropic api ke provided\n");
        return NULL;
    }
    if (NULL == model) {
        fprintf(error_stream, "no model provided\n");
        return NULL;
    }

    HttpHeader headers[3] = {{
                                 .key = "x-api-key",
                                 .value = api_key,
                             },
                             {
                                 .key = "anthropic-version",
                                 .value = ANTHROPIC_VERSION,
                             },
                             {.key = "content-type", .value = "application/json"}};

    AnthropicRequest request = {.headers = headers,
                                .model = model,
                                .max_tokens = max_tokens,
                                .messages = messages,
                                .message_count = message_count,
                                .tools = tools,
                                .tool_count = tool_count};

    HTTPResponse *http_resp = NULL;
    JsonValue *v = NULL;

    StringBuilder *json_buf = calloc(1, sizeof(StringBuilder));
    if (NULL == json_buf) {
        fprintf(stderr, "Failed to allocate buffer for response\n");
        goto cleanup;
    }
    resize_stringbuilder_buffer(json_buf, 8192);
    int err = serialize_anthropic_request(json_buf, &request);
    if (0 != err) {
        fprintf(stderr, "Failed to allocate buffer for response\n");
        goto cleanup;
    }

    http_resp = https_request(HTTP_POST, ANTHROPIC_URL, "443", ANTHROPIC_MESSAGES_PATH, headers, 3,
                              json_buf->buffer, stdout, stderr);
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

    free(json_buf->buffer);
    free(json_buf);

    free_json_value(v);
    free_http_response(http_resp);


    return resp;

cleanup:
    if (NULL != json_buf) {
        free(json_buf->buffer);
        free(json_buf);
    }
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
// TODO: pass in error stream for helpful error output
AnthropicMessage *agent_messages_to_anthropic_messages(const Conversation *conv) {
    if (NULL == conv || 0 == conv->message_count || NULL == conv->messages) {
        return NULL;
    }
    AnthropicMessage *anthropic_messages = calloc(conv->message_count, sizeof(AnthropicMessage));
    if (NULL == anthropic_messages) {
        return NULL;
    }
    for (size_t i = 0; i < conv->message_count; i++) {

        anthropic_messages[i].content_blocks = calloc(conv->messages[i].content_count, sizeof(AnthropicContent));
        if (NULL == anthropic_messages[i].content_blocks) {
            goto cleanup;
        }
        if (NULL == conv->messages[i].content_blocks || 0 == conv->messages[i].content_count) {
            goto cleanup;
        }

        anthropic_messages[i].content_count = conv->messages[i].content_count;
        switch (conv->messages[i].role) {
            case SYSTEM:
                anthropic_messages[i].role = ANTHROPIC_ROLE_SYSTEM;
                break;
            case USER:
                anthropic_messages[i].role = ANTHROPIC_ROLE_USER;
                break;
            case ASSISTANT:
                anthropic_messages[i].role = ANTHROPIC_ROLE_ASSISTANT;
                break;
            default:
                goto cleanup;
        }

        for (size_t j = 0; j < conv->messages[i].content_count; j++) {
            const Content *src = &conv->messages[i].content_blocks[j];
            AnthropicContent *dest = &anthropic_messages[i].content_blocks[j];
            switch (src->type) {
                case TEXT:
                    dest->type = ANTHROPIC_CONTENT_TEXT;
                    if (0 != copy_string(src->as.text.text, &dest->as.text.text)) {
                        goto cleanup;
                    }
                    break;
                case TOOL_CALL:
                    dest->type = ANTHROPIC_CONTENT_TOOL_USE;
                    if (0 != copy_string(src->as.tool_call.id, &dest->as.tool_use.id)) {
                        goto cleanup;
                    }
                    if (0 != copy_string(src->as.tool_call.name, &dest->as.tool_use.name)) {
                        goto cleanup;
                    }
                    if (0 != copy_string(src->as.tool_call.input, &dest->as.tool_use.input)) {
                        goto cleanup;
                    }
                    break;
                case TOOL_RESULT:
                    dest->type = ANTHROPIC_CONTENT_TOOL_RESULT;
                    if (0 != copy_string(src->as.tool_result.tool_use_id,
                                         &dest->as.tool_result.tool_use_id)) {
                        goto cleanup;
                    }
                    if (0 != copy_string(src->as.tool_result.content,
                                         &dest->as.tool_result.content)) {
                        goto cleanup;
                    }
                    dest->as.tool_result.is_error = src->as.tool_result.is_error;
                    break;
                default:
                    goto cleanup;
            }
        }
    }

    return anthropic_messages;
cleanup:
    for (size_t i = 0; i < conv->message_count; i++) {
        for (size_t j = 0; j < anthropic_messages[i].content_count; j++) {
            AnthropicContent *content = &anthropic_messages[i].content_blocks[j];
            free_anthropic_content_internal(content);
        }
        free(anthropic_messages[i].content_blocks);
    }
    free(anthropic_messages);
    return NULL;
}

// Generates a list of anthropic tool structs based on a list of agent tool structs.
// Must be freed by caller.
AnthropicTool *agent_tools_to_anthropic_tools(const ToolSet *tools) {
    if (NULL == tools || NULL == tools->tools || 0 == tools->tool_count) {
        return NULL;
    }
    AnthropicTool *anthropic_tools = calloc(tools->tool_count, sizeof(AnthropicTool));
    if (NULL == anthropic_tools) {
        return NULL;
    }

    for (size_t i = 0; i < tools->tool_count; i++) {
        if (NULL != tools->tools[i].name) {
            anthropic_tools[i].name = strdup(tools->tools[i].name);
        }
        if (NULL == anthropic_tools[i].name) {
            goto cleanup;
        }

        if (NULL != tools->tools[i].description) {
            anthropic_tools[i].description = strdup(tools->tools[i].description);
        }
        if (NULL == anthropic_tools[i].description) {
            goto cleanup;
        }

        if (NULL != tools->tools[i].input_schema) {
            anthropic_tools[i].input_schema = strdup(tools->tools[i].input_schema);
        }
        if (NULL == anthropic_tools[i].input_schema) {
            goto cleanup;
        }
    }

    return anthropic_tools;
cleanup:
    for (size_t i = 0; i < tools->tool_count; i++) {
        free(anthropic_tools[i].name);
        free(anthropic_tools[i].description);
        free(anthropic_tools[i].input_schema);
    }
    free(anthropic_tools);
    return NULL;
}

int anthropic_response_to_inference_response(AnthropicResponse *anthropic_response, InferenceResponse *out) {
    if (NULL == anthropic_response || NULL == out) {
        return -1;
    }
    if (NULL == anthropic_response->content || 0 == anthropic_response->content_count) {
        set_inference_error(out, "no anthropic content to parse");
        return -1;
    }
    out->content_blocks = calloc(anthropic_response->content_count, sizeof(Content));
    if (NULL == out->content_blocks) {
        set_inference_error(out, "failed to allocate agent content blocks.");
        return -1;
    }

    out->content_count = anthropic_response->content_count;
    for (size_t i = 0; i < anthropic_response->content_count; i++) {
        const AnthropicContent anthro = anthropic_response->content[i];
        Content *curr = &out->content_blocks[i];

        switch (anthro.type) {
        case ANTHROPIC_CONTENT_TEXT:
            if (NULL == anthro.as.text.text) {
                set_inference_error(out, "Anthropic text content was null.");
                goto cleanup;
            }
            curr->type = TEXT;
            curr->as.text.text = strdup(anthro.as.text.text);
            if (NULL == curr->as.text.text) {
                set_inference_error(out, "Failed to copy Anthropic text content.");
                goto cleanup;
            }
            break;
        case ANTHROPIC_CONTENT_TOOL_USE:
            if (NULL == anthro.as.tool_use.id || NULL == anthro.as.tool_use.name ||
                NULL == anthro.as.tool_use.input) {
                set_inference_error(out, "Anthropic tool-use content was incomplete.");
                goto cleanup;
            }
            curr->type = TOOL_CALL;
            curr->as.tool_call.id = strdup(anthro.as.tool_use.id);
            if (NULL == curr->as.tool_call.id) {
                set_inference_error(out, "Failed to copy Anthropic tool-use ID.");
                goto cleanup;
            }
            curr->as.tool_call.name = strdup(anthro.as.tool_use.name);
            if (NULL == curr->as.tool_call.name) {
                set_inference_error(out, "Failed to copy Anthropic tool name.");
                goto cleanup;
            }
            curr->as.tool_call.input = strdup(anthro.as.tool_use.input);
            if (NULL == curr->as.tool_call.input) {
                set_inference_error(out, "Failed to copy Anthropic tool input.");
                goto cleanup;
            }
            break;
        case ANTHROPIC_CONTENT_TOOL_RESULT:
            if (NULL == anthro.as.tool_result.tool_use_id ||
                NULL == anthro.as.tool_result.content) {
                out->error_message = "Anthropic tool-result content was incomplete.";
                goto cleanup;
            }
            curr->type = TOOL_RESULT;
            curr->as.tool_result.tool_use_id = strdup(anthro.as.tool_result.tool_use_id);
            if (NULL == curr->as.tool_result.tool_use_id) {
                set_inference_error(out, "Failed to copy Anthropic tool-use ID.");
                goto cleanup;
            }
            curr->as.tool_result.content = strdup(anthro.as.tool_result.content);
            if (NULL == curr->as.tool_result.content) {
                set_inference_error(out, "Failed to copy Anthropic tool result.");
                goto cleanup;
            }
            curr->as.tool_result.is_error = anthro.as.tool_result.is_error;
            break;
        default:
            set_inference_error(out, "Unknown Anthropic content type.");
            goto cleanup;
        }
    }

    return 0;
cleanup:
    if (NULL != out && NULL != out->content_blocks) {
        for (size_t i = 0 ; i < out->content_count; i++) {
            free_content_fields(&out->content_blocks[i]);
        }
        free(out->content_blocks);
    }
    return -1;
}

AnthropicContext *create_anthropic_context(char *api_key, char *model) {
    if (NULL == api_key) {
        char *anthropic_api_key = getenv("ANTHROPIC_API_KEY");
        if (NULL == anthropic_api_key) {
            fprintf(stderr, "Failed to find the API Key from the expected env var %s\n",
                    "ANTHROPIC_API_KEY");
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

void anthropic_complete_inference(void *context, const Conversation *conv, const ToolSet *tools, InferenceResponse *out) {
    if (NULL == out) {
        return;
    }
    if (NULL == context) {
        set_inference_error(out, "context was null");
        return;
    }
    AnthropicContext *anthropic_ctx = context;

    AnthropicResponse *resp = NULL;
    AnthropicMessage *anthropic_messages = NULL;
    AnthropicTool *anthropic_tools = NULL;
    size_t tool_count = 0;

    // convert agent messages to anthropic messages
    anthropic_messages = agent_messages_to_anthropic_messages(conv);
    if (NULL == anthropic_messages) {
        set_inference_error(out, "unable to convert agent messages to anthropic messages");
        goto cleanup;
    }

    // convert agent tools to anthropic tools
    if (NULL != tools) {
        tool_count = tools->tool_count;
        if (tool_count > 0) {
            anthropic_tools = agent_tools_to_anthropic_tools(tools);
            if (NULL == anthropic_tools) {
                set_inference_error(out, "unable to convert agent tools to anthropic tools");
                goto cleanup;
            }
        }
    }

    // run inference using anthropic-friendly context, messages, and tools
    resp = anthropic_run_inference(anthropic_ctx->api_key, anthropic_ctx->model,
                                                      anthropic_ctx->max_tokens, anthropic_messages,
                                                      conv->message_count, anthropic_tools, tool_count, stdout);

    if (NULL == resp) {
        set_inference_error(out, "anthropic_run_inference returned NULL");
        goto cleanup;
    }

    if (0 == strcmp("error", resp->type)) {
        if (NULL == resp->error) {
            set_inference_error(
                out, "anthropic response indicated error type but parsed error was null");
            goto cleanup;
        }

        // set the out->error message to a single string combining the anthropic error fields.
        size_t len = snprintf(NULL, 0, "%s: %s", resp->error->type, resp->error->message);
        char *buf = calloc(len + 1, sizeof(char));
        if (NULL == buf) {
            set_inference_error(out, "failed to allocate Anthropic error message");
            goto cleanup;
        }
        snprintf(buf, len + 1, "%s: %s", resp->error->type, resp->error->message);
        set_inference_error(out, buf);
        free(buf);
    }

    if (NULL != resp->content && resp->content_count > 0) {
        if (0 != anthropic_response_to_inference_response(resp, out)) {
            set_inference_error(out, "unable to parse content of anthropic response");
            goto cleanup;
        }
    }
    out->stop_reason = resp->stop_reason ? strdup(resp->stop_reason) : NULL;

cleanup:
    if (NULL != resp) {
        free_anthropic_response(resp);
    }
    if (NULL != anthropic_messages) {
        for (size_t i = 0; i < conv->message_count; i++) {
            for (size_t j = 0; j < anthropic_messages[i].content_count; j++) {
                free_anthropic_content_internal(&anthropic_messages[i].content_blocks[j]);
            }
            free(anthropic_messages[i].content_blocks);
        }
        free(anthropic_messages);
    }
    if (NULL != anthropic_tools) {
        for (size_t i = 0; i < tool_count; i++) {
            free(anthropic_tools[i].name);
            free(anthropic_tools[i].description);
            free(anthropic_tools[i].input_schema);
        }
        free(anthropic_tools);
    }
}

void free_anthropic_context(void *context) {
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
