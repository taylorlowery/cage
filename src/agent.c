#include "agent.h"
#include "provider.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_AGENT_NAME "Cage"
#define ANSI_USER_STYLE "\033[1;36m"
#define ANSI_AGENT_STYLE "\033[1;35m"
#define ANSI_CLEAR_STYLE "\033[0m"

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

// allocates a new agent instance based on a given provider context.
// caller must free.
Agent *new_agent(char *display_name, InferenceProvider *client, FILE *input_stream,
                 FILE *output_stream, FILE *error_stream) {
    Agent *agent = calloc(1, sizeof(Agent));
    if (NULL == agent) {
        fprintf(stderr, "unable to allocate agent");
        return NULL;
    }

    agent->conversation = calloc(1, sizeof(Conversation));
    if (NULL == agent->conversation) {
        fprintf(stderr, "unable to allocate conversation for agent");
        goto cleanup;
        return NULL;
    }

    if (resize_conversation(agent->conversation) <= 0) {
        fprintf(stderr, "failed to allocate conversation for agent");
        goto cleanup;
        return NULL;
    }

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

    // greet;
    print_agent_message(agent, "Howdy, pilgrim!");

    // instructions

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
            break;
        }
        size_t user_message_len = strlen(user_message_buf);
        if (user_message_len > 0 && user_message_buf[user_message_len - 1] == '\n') {
            user_message_buf[user_message_len - 1] = '\0';
        }

        add_message_to_conv(agent->conversation, user_message_buf, USER);

        // send user message to LLM provider
        agent->client->complete_inference(agent->client->provider_context, agent->conversation,
                                          resp);
        if (NULL != resp->error_message) {
            fprintf(agent->error_stream, "%s", resp->error_message);
            // free(user_message);
            free(resp);
            break;
        }

        // <tool use>

        // add response to conversation
        add_message_to_conv(agent->conversation, resp->text, ASSISTANT);
        // print response
        print_agent_message(agent, resp->text);

        free(resp);
    }
}
