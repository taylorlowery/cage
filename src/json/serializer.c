#include "json.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// resize the buffer capacity.
// caller should request current length + bytes to append + 1 (for null terminator)
int resize_serializer_buffer(SerializerBuffer *buffer, size_t required_capacity) {
    if (NULL == buffer) {
        // TODO: handle error
        return -1;
    }
    if (required_capacity == 0) {
        return 0;
    }
    if (required_capacity <= buffer->buffer_capacity) {
        // no resize required
        return 0;
    }
    size_t new_capacity = buffer->buffer_capacity;
    if (0 == new_capacity) {
        new_capacity = 1;
    }
    while (new_capacity < required_capacity) {
        new_capacity = new_capacity * 2;
    }
    char *tmp = realloc(buffer->buffer, new_capacity);
    if (NULL == tmp) {
        // oh no!
        return -1;
    }
    buffer->buffer_capacity = new_capacity;
    buffer->buffer = tmp;
    return 0;
}

int append_bytes_to_serializer_buffer(SerializerBuffer *buf, const char *src, size_t src_len) {
    if (NULL == buf || NULL == src) {
        return -1;
    }
    if (0 == src_len) {
        return 0;
    }
    size_t required_capacity = buf->buffer_length + src_len + 1;
    if (buf->buffer_capacity < required_capacity) {
        if (0 != resize_serializer_buffer(buf, required_capacity)) {
            return -1;
        }

    }
    memcpy(buf->buffer + buf->buffer_length, src, src_len);
    buf->buffer_length = buf->buffer_length + src_len;
    buf->buffer[buf->buffer_length] = '\0';
    return 0;
}

int append_char_to_serializer_buffer(SerializerBuffer *buf, const char c) {
    return append_bytes_to_serializer_buffer(buf, &c, 1);
}

int append_json_string_to_serializer_buffer(SerializerBuffer *buf, const char *string) {
    if (NULL == string) {
        return -1;
    }
    int err = 0;
    err = append_char_to_serializer_buffer(buf, '"');
    if (0 != err) {
        return err;
    }
    // todo: sanitize/escape characters in string
    for (const char *c = string; '\0' != *c; c++) {
        switch (*c) {
            case '"':
                err = append_bytes_to_serializer_buffer(buf, "\\\"", 2);
                break;
            case '\\':
                err = append_bytes_to_serializer_buffer(buf, "\\\\", 2);
                break;
            case '\n':
                err = append_bytes_to_serializer_buffer(buf, "\\n", 2);
                break;
            case '\t':
                err = append_bytes_to_serializer_buffer(buf, "\\t", 2);
                break;
            case '\r':
                err = append_bytes_to_serializer_buffer(buf, "\\r", 2);
                break;
            // todo: control characters?
            default:
                err = append_char_to_serializer_buffer(buf, *c);
                break;
        }
        if (0 != err) {
            return err;
        }
    }
    err = append_char_to_serializer_buffer(buf, '"');
    // last instance will be 0 for success or error
    return err;
}

// returns a string representation of a JSON value.
// the caller must free the string.
char *json_value_to_string(const JsonValue *value) {
    if (NULL == value) {
        return NULL;
    }
    SerializerBuffer *buf = calloc(1, sizeof(SerializerBuffer));
    if (NULL == buf) {
        return NULL;
    }
    int err = resize_serializer_buffer(buf, 4096);
    if (0 != err) {
        free(buf);
        return NULL;
    }

    switch (value->type) {
    case JSON_NULL:
        if (0 != append_bytes_to_serializer_buffer(buf, "null", 4)) {
            goto cleanup;
        }
        break;
    case JSON_BOOL:
        if (value->as.boolean) {
            err = append_bytes_to_serializer_buffer(buf, "true", 4);
        } else {
            err = append_bytes_to_serializer_buffer(buf, "false", 5);
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
        if (0 != append_bytes_to_serializer_buffer(buf, number_buf, (size_t)chars_written)) {
            goto cleanup;
        }
        break;
    }
    case JSON_STRING:
        if (0 != append_json_string_to_serializer_buffer(buf, value->as.string)) {
            goto cleanup;
        }
        break;
    case JSON_ARRAY:
        if (0 != append_char_to_serializer_buffer(buf, '[')) {
            goto cleanup;
        }
        for (size_t i = 0; i < value->as.array->count; i++) {
            char *item_to_string = json_value_to_string(&value->as.array->items[i]);
            if (NULL == item_to_string) {
                goto cleanup;
            }
            err = append_bytes_to_serializer_buffer(buf, item_to_string, strlen(item_to_string));
            free(item_to_string);
            if (0 != err) {
                goto cleanup;
            }
            if (i < value->as.array->count - 1 &&
                0 != append_char_to_serializer_buffer(buf, ',')) {
                goto cleanup;
            }
        }
        if (0 != append_char_to_serializer_buffer(buf, ']')) {
            goto cleanup;
        }
        break;
    case JSON_OBJECT:
        if (0 != append_char_to_serializer_buffer(buf, '{')) {
            goto cleanup;
        }
        for (size_t i = 0; i < value->as.object->count; i++) {
            const char *key = value->as.object->pairs[i].key;
            char *serialized_value = json_value_to_string(value->as.object->pairs[i].value);
            if (NULL == serialized_value) {
                goto cleanup;
            }
            err = append_json_string_to_serializer_buffer(buf, key);
            if (0 == err) {
                err = append_char_to_serializer_buffer(buf, ':');
            }
            if (0 == err) {
                err = append_bytes_to_serializer_buffer(buf, serialized_value,
                                                         strlen(serialized_value));
            }
            free(serialized_value);
            if (0 != err) {
                goto cleanup;
            }
            if (i < value->as.object->count - 1 &&
                0 != append_char_to_serializer_buffer(buf, ',')) {
                goto cleanup;
            }
        }
        if (0 != append_char_to_serializer_buffer(buf, '}')) {
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
