#ifndef TOOLS_H
#define TOOLS_H

#include "provider.h"

int read_file(const char *args, char **out);
int list_files(const char *args, char **out);
int edit_file(const char *args, char **out);
ToolSet get_default_tool_set(void);

#endif
