#ifndef AGENT_H
#define AGENT_H

#include "provider.h"
#include <stdio.h>

typedef struct {
    char *display_name;
    InferenceProvider *client;
    Conversation *conversation;
    ToolSet *tools;
    FILE *input_stream;
    FILE *output_stream;
    FILE *error_stream;
} Agent;

Agent *new_agent(char *display_name, InferenceProvider *client, ToolSet *tools, FILE *input_stream,
                 FILE *output_stream, FILE *error_stream);

void free_agent(Agent *agent);

int call_tool(Agent *agent, const char *tool_name, const char *args, char *out, const size_t out_size);

void run(Agent *agent);

#endif
