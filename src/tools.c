#include "tools.h"
#include "json.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "string_builder.h"
#include <dirent.h>
#include <errno.h>

static JsonValue *parse_tool_args_object(const char *args_json) {
    if (NULL == args_json || '\0' == args_json[0]) {
        return NULL;
    }

    Parser parser;
    init_parser(&parser, args_json, stderr);
    JsonValue *value = parse_json(&parser);
    if (NULL == value) {
        return NULL;
    }
    if (JSON_OBJECT != value->type) {
        free_json_value(value);
        return NULL;
    }

    return value;
}

static int get_required_string_arg(const JsonObject *object, const char *key,
                                   const char **out) {
    if (NULL == object || NULL == key || NULL == out) {
        return -1;
    }

    JsonValue *value = get_json_object_value_by_key(object, key);
    if (NULL == value || JSON_STRING != value->type || NULL == value->as.string) {
        return -1;
    }

    *out = value->as.string;
    return 0;
}

static int read_path_to_builder(const char *filepath, StringBuilder *builder) {
    if (NULL == filepath || NULL == builder) {
        return -1;
    }

    FILE *fp = fopen(filepath, "rb");
    if (NULL == fp) {
        return -1;
    }

    char chunk[1024];
    size_t bytes_read;
    while (0 < (bytes_read = fread(chunk, 1, sizeof(chunk), fp))) {
        if (0 != append_bytes_to_stringbuilder_buffer(builder, chunk, bytes_read)) {
            fclose(fp);
            return -1;
        }
    }
    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }
    if (0 != fclose(fp)) {
        return -1;
    }
    return 0;
}

// read the contents of a file and fill an `out` buffer
// arguments should be a JSON string containing {"filepath":"<filepath>"}
int read_file(const char *args_json, char **out) {
    if (NULL == args_json || NULL == out) {
        return -1;
    }

    JsonValue *args = parse_tool_args_object(args_json);
    if (NULL == args) {
        return -1;
    }

    *out = NULL;
    StringBuilder builder = {0};
    const char *filepath = NULL;
    if (0 != get_required_string_arg(args->as.object, "filepath", &filepath)) {
        goto cleanup;
    }

    if (0 != read_path_to_builder(filepath, &builder)) {
        goto cleanup;
    }

    *out = builder.buffer;
    builder.buffer = NULL;

    free_json_value(args);
    return 0;

cleanup:
    free(builder.buffer);
    free_json_value(args);
    return -1;
}

// list all files in a directory,
// writing them to an `out` buffer separated by newlines.
// Arguments should be a JSON string containing {"directory_path":"<dir path>"}
// directory paths containing ".." will not be honored.
int list_files(const char *args_json, char **out) {
    if (NULL == args_json || NULL == out) {
        return -1;
    }
    *out = NULL;

    JsonValue *args = parse_tool_args_object(args_json);
    if (NULL == args) {
        return -1;
    }

    StringBuilder builder = {0};
    DIR *directory = NULL;
    const char *directory_path = NULL;
    if (0 != get_required_string_arg(args->as.object, "directory_path", &directory_path)) {
        goto cleanup;
    }
    if (NULL != strstr(directory_path, "..")) {
        fprintf(stderr, "No navigating above the current dir!\n");
        goto cleanup;
    }

    directory = opendir(directory_path);
    if (NULL == directory) {
        goto cleanup;
    }

    errno = 0;
    struct dirent *entry = NULL;
    while (NULL != (entry = readdir(directory))) {
        if (0 == strcmp(entry->d_name, ".") || 0 == strcmp(entry->d_name, "..")) {
            continue;
        }
        if (0 != append_bytes_to_stringbuilder_buffer(&builder, entry->d_name,
                                                       strlen(entry->d_name))) {
            goto cleanup;
        }
        if (0 != append_char_to_stringbuilder_buffer(&builder, '\n')) {
            goto cleanup;
        }
    }

    if (0 != errno) {
        goto cleanup;
    }

    int close_result = closedir(directory);
    directory = NULL;
    if (0 != close_result) {
        goto cleanup;
    }

    *out = builder.buffer;
    builder.buffer = NULL;
    free_json_value(args);
    return 0;

cleanup:
    if (NULL != directory) {
        closedir(directory);
    }
    free(builder.buffer);
    free_json_value(args);
    return -1;
}

// write to a file
// arguments should be a JSON string containing:
// {
//  "path": "<filepath>",
//  "old_str": "<content to be replaced>",
//  "new_str": "<new string>"
// }
int edit_file(const char *args_json, char **out) {
    if (NULL == args_json || NULL == out) {
        return -1;
    }
    *out = NULL;

    JsonValue *args = parse_tool_args_object(args_json);
    if (NULL == args) {
        return -1;
    }

    const char *filepath = NULL;
    const char *old_str = NULL;
    const char *new_str = NULL;
    if (0 != get_required_string_arg(args->as.object, "path", &filepath) ||
        0 != get_required_string_arg(args->as.object, "old_str", &old_str) ||
        0 != get_required_string_arg(args->as.object, "new_str", &new_str)) {
        free_json_value(args);
        return -1;
    }
    if ('\0' == old_str[0] || 0 == strcmp(old_str, new_str)) {
        free_json_value(args);
        return -1;
    }

    StringBuilder original = {0};
    StringBuilder replacement = {0};
    if (0 != read_path_to_builder(filepath, &original) || NULL == original.buffer) {
        goto cleanup;
    }

    char *match = strstr(original.buffer, old_str);
    if (NULL == match) {
        goto cleanup;
    }
    char *next_match = strstr(match + strlen(old_str), old_str);
    if (NULL != next_match) {
        goto cleanup;
    }

    size_t prefix_length = (size_t)(match - original.buffer);
    if (0 != append_bytes_to_stringbuilder_buffer(&replacement, original.buffer,
                                                   prefix_length)) {
        goto cleanup;
    }
    if (0 != append_bytes_to_stringbuilder_buffer(&replacement, new_str, strlen(new_str))) {
        goto cleanup;
    }
    char *suffix = match + strlen(old_str);
    if (0 != append_bytes_to_stringbuilder_buffer(&replacement, suffix, strlen(suffix))) {
        goto cleanup;
    }

    FILE *fp = fopen(filepath, "wb");
    if (NULL == fp) {
        goto cleanup;
    }
    if (replacement.buffer_length > 0 &&
        replacement.buffer_length != fwrite(replacement.buffer, 1,
                                             replacement.buffer_length, fp)) {
        fclose(fp);
        goto cleanup;
    }
    if (0 != fclose(fp)) {
        goto cleanup;
    }

    *out = strdup("OK");
    if (NULL == *out) {
        goto cleanup;
    }

    free(replacement.buffer);
    free(original.buffer);
    free_json_value(args);
    return 0;

cleanup:
    free(replacement.buffer);
    free(original.buffer);
    free(*out);
    *out = NULL;
    free_json_value(args);
    return -1;
}

ToolSet get_default_tool_set(void) {
    static Tool tools[] = {
        {
            .name = "read_file",
            .description = "Read the contents of a relative file path.",
            .input_schema =
                "{\"type\":\"object\",\"properties\":{\"filepath\":"
                "{\"type\":\"string\"}},\"required\":[\"filepath\"]}",
            .handler = read_file,
        },
        {
            .name = "list_files",
            .description = "List files and directories at a relative directory path.",
            .input_schema =
                "{\"type\":\"object\",\"properties\":{\"directory_path\":"
                "{\"type\":\"string\"}},\"required\":[\"directory_path\"]}",
            .handler = list_files,
        },
        {
            .name = "edit_file",
            .description = "Replace one exact string in a relative file path.",
            .input_schema =
                "{\"type\":\"object\",\"properties\":{\"path\":"
                "{\"type\":\"string\"},\"old_str\":{\"type\":\"string\"},"
                "\"new_str\":{\"type\":\"string\"}},"
                "\"required\":[\"path\",\"old_str\",\"new_str\"]}",
            .handler = edit_file,
        },
    };

    ToolSet tool_set = {
        .tools = tools,
        .tool_count = sizeof(tools) / sizeof(tools[0]),
    };
    return tool_set;
}
