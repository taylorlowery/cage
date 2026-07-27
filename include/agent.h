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

Agent *new_agent(char *display_name, InferenceProvider *client, FILE *input_stream,
                 FILE *output_stream, FILE *error_stream);

void free_agent(Agent *agent);

void call_tool(char *tool_name, char *args, char *out);

void run(Agent *agent);

#endif
