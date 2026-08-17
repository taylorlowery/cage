#include "agent.h"
#include "provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_AGENT_NAME "Cage"
#define ANSI_USER_STYLE "\033[1;36m"
#define ANSI_AGENT_STYLE "\033[1;35m"
#define ANSI_CLEAR_STYLE "\033[0m"

static void free_content_array(Content *content_blocks, size_t content_count) {
    if (NULL == content_blocks) {
        return;
    }
    for (size_t i = 0; i < content_count; i++) {
        free_content_fields(&content_blocks[i]);
    }
    free(content_blocks);
}

void free_agent(Agent *agent) {
    if (NULL == agent) {
        return;
    }
    if (NULL != agent->client && NULL != agent->client->provider_context &&
        NULL != agent->client->destroy_provider_context) {
        agent->client->destroy_provider_context(agent->client->provider_context);
    }
    if (NULL != agent->conversation) {
        free_conversation(agent->conversation);
    }
    free(agent);
}

// calls a function for an agent with the provided args,
// and populates a provided buffer with the results.
// presumably needs to be a big buffer, if we're passing back whole files.
// (i.e, an 'ls' tool).
// instead of args as a single string, probably needs to be KV pairs?
// JSON parser should be able to deserialize the input into KV pairs.
// Returns 0 if the handler succeeded, -1 if the handler fails.
int call_tool(Agent *agent, const char *tool_name, const char *args, char **out) {
    if (NULL == agent) {
        fprintf(stderr, "tool call made against null agent\n");
        return -1;
    }
    if (NULL == tool_name) {
        fprintf(agent->error_stream, "tool call made with null tool name\n");
        return -1;
    }
    if (NULL == agent->tools || NULL == agent->tools->tools) {
        fprintf(agent->error_stream, "attempted to call tool '%s' on agent with a null toolset\n", tool_name);
        return -1;
    }
    if (NULL == out) {
        fprintf(agent->error_stream, "out argument is null\n");
        return -1;
    }
    for (size_t i = 0; i < agent->tools->tool_count; i++) {
       const  Tool *tool = &agent->tools->tools[i];
        if (NULL != tool->name && 0 == strcmp(tool_name, tool->name)) {
            if (NULL == tool->handler) {
                fprintf(agent->error_stream, "attempted to call tool '%s', but handler was NULL\n", tool_name);
                return -1;
            }

            return tool->handler(args, out);
        }
    }
    // TODO: pass in an error stream instead
    fprintf(agent->error_stream, "invalid tool name: '%s'\n", tool_name);
    return -1;
}

// allocates a new agent instance based on a given provider context.
// caller must free.
Agent *new_agent(char *display_name, InferenceProvider *client, ToolSet *tools, FILE *input_stream,
                 FILE *output_stream, FILE *error_stream) {
    Agent *agent = calloc(1, sizeof(Agent));
    if (NULL == agent) {
        fprintf(stderr, "unable to allocate agent\n");
        return NULL;
    }

    agent->conversation = calloc(1, sizeof(Conversation));
    if (NULL == agent->conversation) {
        fprintf(stderr, "unable to allocate conversation for agent\n");
        goto cleanup;
    }

    if (resize_conversation(agent->conversation) <= 0) {
        fprintf(stderr, "failed to allocate conversation for agent\n");
        goto cleanup;
    }

    agent->tools = tools;
    agent->client = client;
    agent->display_name = display_name ? display_name : DEFAULT_AGENT_NAME;
    agent->input_stream = input_stream;
    agent->output_stream = output_stream;
    agent->error_stream = error_stream;
    return agent;
cleanup:
    free_agent(agent);
    return NULL;
}

void print_agent_message(Agent *agent, char *message) {
    fprintf(agent->output_stream, "%s%s:%s %s\n", ANSI_AGENT_STYLE, agent->display_name,
            ANSI_CLEAR_STYLE, message);
}

void print_user_message(Agent *agent, char *message) {
    fprintf(agent->output_stream, "%sYou:%s %s\n", ANSI_USER_STYLE, ANSI_CLEAR_STYLE, message);
}

void run(Agent *agent) {
    if (NULL == agent) {
        fprintf(stderr, "agent is null\n");
        return;
    }
    if (NULL == agent->client) {
        fprintf(stderr, "agent's inference provider is null\n");
        return;
    }

    // greet;
    print_agent_message(agent, "Howdy, pilgrim!");


    // TODO: instructions/system prompt?

    // loop:
    while (true) {
        // get user input
        fprintf(agent->output_stream, "%sYou:%s", ANSI_USER_STYLE, ANSI_CLEAR_STYLE);
        // run inference
        InferenceResponse *resp = calloc(1, sizeof(InferenceResponse));
        if (NULL == resp) {
            fprintf(stderr, "failed to allocate response object");
            break;
        }

        // get user message and add to conversation
        char user_message_buf[4096];
        fflush(agent->output_stream);
        if (NULL == fgets(user_message_buf, sizeof(user_message_buf) - 1, agent->input_stream)) {
            fprintf(agent->error_stream, "error getting user input");
            clear_inference_response(resp);
            free(resp);
            break;
        }
        size_t user_message_len = strlen(user_message_buf);
        if (user_message_len > 0 && user_message_buf[user_message_len - 1] == '\n') {
            user_message_buf[user_message_len - 1] = '\0';
        }

        if (0 != add_message_to_conv(agent->conversation, user_message_buf, USER)) {
            fprintf(agent->error_stream, "failed to add user message to conversation\n");
            clear_inference_response(resp);
            free(resp);
            break;
        }
        clear_inference_response(resp);

        // send user message to LLM provider
        agent->client->complete_inference(agent->client->provider_context, agent->conversation, agent->tools,
                                          resp);
        if (NULL != resp->error_message) {
            fprintf(agent->error_stream, "%s", resp->error_message);
            clear_inference_response(resp);
            free(resp);
            break;
        }

        // add response to conversation
        if (0 != add_content_message_to_conv(agent->conversation, resp->content_blocks,
                                              resp->content_count, ASSISTANT)) {
            fprintf(agent->error_stream, "failed to add assistant response to conversation\n");
            clear_inference_response(resp);
            free(resp);
            break;
        }

        // call tools and send responses to LLM until all tools called
        while (NULL != resp->stop_reason && 0 == strcmp(resp->stop_reason, "tool_use")) {
            size_t tool_use_count = 0;
            for (size_t i = 0; i < resp->content_count; i++) {
                if (TOOL_CALL == resp->content_blocks[i].type) {
                    tool_use_count++;
                }
            }
            if (0 == tool_use_count) {
                break;
            }
            Content *tool_use_content = calloc(tool_use_count, sizeof(Content));
            if (NULL == tool_use_content) {
                fprintf(agent->error_stream, "failed to allocate space for tool use calls.\n");
                clear_inference_response(resp);
                free(resp);
                return;
            }
            size_t tool_use_index = 0;
            for (size_t i = 0; i < resp->content_count; i++) {
                Content *content = &resp->content_blocks[i];
                if (content->type == TOOL_CALL) {

                    char *buf;
                    int call_err = call_tool(agent, content->as.tool_call.name,
                                             content->as.tool_call.input, &buf);

                    Content *result = &tool_use_content[tool_use_index];
                    result->type = TOOL_RESULT;
                    result->as.tool_result.tool_use_id = strdup(content->as.tool_call.id);
                    result->as.tool_result.is_error = (0 != call_err);
                    if (0 != call_err) {
                        result->as.tool_result.content = strdup("failed to call tool");
                    } else {
                        result->as.tool_result.content = strdup(buf);
                    }
                    if (NULL == result->as.tool_result.tool_use_id ||
                        NULL == result->as.tool_result.content) {
                        free_content_array(tool_use_content, tool_use_count);
                        clear_inference_response(resp);
                        free(resp);
                        return;
                    }
                    tool_use_index++;
                }
            }

            if (tool_use_index != tool_use_count) {
                fprintf(agent->error_stream, "failed to build all tool results\n");
                free_content_array(tool_use_content, tool_use_count);
                clear_inference_response(resp);
                free(resp);
                return;
            }

            int add_err = add_content_message_to_conv(agent->conversation, tool_use_content,
                                                       tool_use_count, USER);
            free_content_array(tool_use_content, tool_use_count);
            if (0 != add_err) {
                fprintf(agent->error_stream, "failed to add tool result content to conversation\n");
                clear_inference_response(resp);
                free(resp);
                return;
            }

            // The conversation now owns copies of the assistant response and tool results.
            clear_inference_response(resp);
            agent->client->complete_inference(agent->client->provider_context,
                                              agent->conversation, agent->tools, resp);

            if (NULL != resp->error_message) {
                fprintf(agent->error_stream, "%s", resp->error_message);
                clear_inference_response(resp);
                free(resp);
                return;
            }

            if (0 != add_content_message_to_conv(agent->conversation, resp->content_blocks,
                                                  resp->content_count, ASSISTANT)) {
                fprintf(agent->error_stream, "failed to add assistant response to conversation\n");
                clear_inference_response(resp);
                free(resp);
                return;
            }
        }

        for (size_t i = 0; i < resp->content_count; i++) {
            if (resp->content_blocks[i].type == TEXT) {
                print_agent_message(agent, resp->content_blocks[i].as.text.text);
            }
        }

        clear_inference_response(resp);
        free(resp);
    }
}
