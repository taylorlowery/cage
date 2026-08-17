#include "tools.h"
#include "json.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "string_builder.h"

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
    StringBuilder buf = {0};
    FILE *fp = NULL;
    const char *filepath = NULL;
    if (0 != get_required_string_arg(args->as.object, "filepath", &filepath)) {
        goto cleanup;
    }

    // TODO: implementation that reads filepath into out.
    fp = fopen(filepath, "r");
    if (NULL== fp) {
        goto cleanup;
    }

    char chunk[1024];
    size_t bytes_read;
    while (0 < (bytes_read = fread(chunk, 1, sizeof(chunk), fp))) {
        if (0 != append_bytes_to_stringbuilder_buffer(&buf, chunk, bytes_read)) {
            // failure, clean up
            goto cleanup;
        }
    }
    if (ferror(fp)) {
        // failure, clean up
        goto cleanup;
    }
    int close_result = fclose(fp);
    fp = NULL;
    if (0 != close_result) {
        goto cleanup;
    }

    *out = buf.buffer;
    buf.buffer = NULL;

    free_json_value(args);
    return 0;

cleanup:
    if (NULL != fp) {
        fclose(fp);
    }
    free(buf.buffer);
    free_json_value(args);
    return -1;
}

// list all files in a directory,
// writing them to an `out` buffer separated by newlines.
// Arguments should be a JSON string containing {"directory_path":"<dir path>"}
int list_files(const char *args_json, char **out) {
    if (NULL == args_json || NULL == out) {
        return -1;
    }

    JsonValue *args = parse_tool_args_object(args_json);
    if (NULL == args) {
        return -1;
    }

    const char *directory_path = NULL;
    if (0 != get_required_string_arg(args->as.object, "directory_path", &directory_path)) {
        free_json_value(args);
        return -1;
    }

    // TODO: implementation that lists directory_path into out.
    (void)directory_path;
    *out = NULL;

    free_json_value(args);
    return 0;
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

    // TODO: read filepath, find old_str exactly once, and replace it with new_str.
    // Write a success or error message into out.
    (void)filepath;
    (void)old_str;
    (void)new_str;
    *out = NULL;

    free_json_value(args);
    return 0;
}
