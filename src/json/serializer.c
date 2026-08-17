#include "json.h"
#include "string_builder.h"
#include <string.h>

// returns a string representation of a JSON value.
// the caller must free the string.
char *json_value_to_string(const JsonValue *value) {
    if (NULL == value) {
        return NULL;
    }
    StringBuilder *buf = calloc(1, sizeof(StringBuilder));
    if (NULL == buf) {
        return NULL;
    }
    int err = resize_stringbuilder_buffer(buf, 4096);
    if (0 != err) {
        free(buf);
        return NULL;
    }

    switch (value->type) {
    case JSON_NULL:
        if (0 != append_bytes_to_stringbuilder_buffer(buf, "null", 4)) {
            goto cleanup;
        }
        break;
    case JSON_BOOL:
        if (value->as.boolean) {
            err = append_bytes_to_stringbuilder_buffer(buf, "true", 4);
        } else {
            err = append_bytes_to_stringbuilder_buffer(buf, "false", 5);
        }
        if (0 != err) {
            goto cleanup;
        }
        break;
    case JSON_NUMBER: {
        char number_buf[64];
        int chars_written = snprintf(number_buf, sizeof(number_buf), "%.17g", value->as.number);
        if (chars_written < 0 || (size_t)chars_written >= sizeof(number_buf)) {
            goto cleanup;
        }
        if (0 != append_bytes_to_stringbuilder_buffer(buf, number_buf, (size_t)chars_written)) {
            goto cleanup;
        }
        break;
    }
    case JSON_STRING:
        if (0 != append_json_string_to_stringbuilder_buffer(buf, value->as.string)) {
            goto cleanup;
        }
        break;
    case JSON_ARRAY:
        if (0 != append_char_to_stringbuilder_buffer(buf, '[')) {
            goto cleanup;
        }
        for (size_t i = 0; i < value->as.array->count; i++) {
            char *item_to_string = json_value_to_string(&value->as.array->items[i]);
            if (NULL == item_to_string) {
                goto cleanup;
            }
            err = append_bytes_to_stringbuilder_buffer(buf, item_to_string, strlen(item_to_string));
            free(item_to_string);
            if (0 != err) {
                goto cleanup;
            }
            if (i < value->as.array->count - 1 &&
                0 != append_char_to_stringbuilder_buffer(buf, ',')) {
                goto cleanup;
            }
        }
        if (0 != append_char_to_stringbuilder_buffer(buf, ']')) {
            goto cleanup;
        }
        break;
    case JSON_OBJECT:
        if (0 != append_char_to_stringbuilder_buffer(buf, '{')) {
            goto cleanup;
        }
        for (size_t i = 0; i < value->as.object->count; i++) {
            const char *key = value->as.object->pairs[i].key;
            char *serialized_value = json_value_to_string(value->as.object->pairs[i].value);
            if (NULL == serialized_value) {
                goto cleanup;
            }
            err = append_json_string_to_stringbuilder_buffer(buf, key);
            if (0 == err) {
                err = append_char_to_stringbuilder_buffer(buf, ':');
            }
            if (0 == err) {
                err = append_bytes_to_stringbuilder_buffer(buf, serialized_value,
                                                         strlen(serialized_value));
            }
            free(serialized_value);
            if (0 != err) {
                goto cleanup;
            }
            if (i < value->as.object->count - 1 &&
                0 != append_char_to_stringbuilder_buffer(buf, ',')) {
                goto cleanup;
            }
        }
        if (0 != append_char_to_stringbuilder_buffer(buf, '}')) {
            goto cleanup;
        }
        break;
    default:
        goto cleanup;
    }

    buf->buffer[buf->buffer_length] = '\0';
    char *serialized = buf->buffer;
    free(buf);
    return serialized;

cleanup:
    free(buf->buffer);
    free(buf);
    return NULL;
}
