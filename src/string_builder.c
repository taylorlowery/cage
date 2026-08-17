#include "string_builder.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// resize the buffer capacity.
// caller should request current length + bytes to append + 1 (for null terminator)
int resize_stringbuilder_buffer(StringBuilder *buf, size_t required_capacity) {
    if (NULL == buf) {
        // TODO: handle error
        return -1;
    }
    if (required_capacity == 0) {
        return 0;
    }
    if (required_capacity <= buf->buffer_capacity) {
        // no resize required
        return 0;
    }
    size_t new_capacity = buf->buffer_capacity;
    if (0 == new_capacity) {
        new_capacity = 1;
    }
    while (new_capacity < required_capacity) {
        new_capacity = new_capacity * 2;
    }
    char *tmp = realloc(buf->buffer, new_capacity);
    if (NULL == tmp) {
        // oh no!
        return -1;
    }
    buf->buffer_capacity = new_capacity;
    buf->buffer = tmp;
    return 0;
}

int append_bytes_to_stringbuilder_buffer(StringBuilder *buf, const char *src, size_t src_len) {
    if (NULL == buf || NULL == src) {
        return -1;
    }
    if (0 == src_len) {
        return 0;
    }
    size_t required_capacity = buf->buffer_length + src_len + 1;
    if (buf->buffer_capacity < required_capacity) {
        if (0 != resize_stringbuilder_buffer(buf, required_capacity)) {
            return -1;
        }

    }
    memcpy(buf->buffer + buf->buffer_length, src, src_len);
    buf->buffer_length = buf->buffer_length + src_len;
    buf->buffer[buf->buffer_length] = '\0';
    return 0;
}

int append_char_to_stringbuilder_buffer(StringBuilder *buf, const char c) {
    return append_bytes_to_stringbuilder_buffer(buf, &c, 1);
}

int append_json_string_to_stringbuilder_buffer(StringBuilder *buf, const char *string) {
    if (NULL == string) {
        return -1;
    }
    int err = 0;
    err = append_char_to_stringbuilder_buffer(buf, '"');
    if (0 != err) {
        return err;
    }
    // todo: sanitize/escape characters in string
    for (const char *c = string; '\0' != *c; c++) {
        switch (*c) {
            case '"':
                err = append_bytes_to_stringbuilder_buffer(buf, "\\\"", 2);
                break;
            case '\\':
                err = append_bytes_to_stringbuilder_buffer(buf, "\\\\", 2);
                break;
            case '\n':
                err = append_bytes_to_stringbuilder_buffer(buf, "\\n", 2);
                break;
            case '\t':
                err = append_bytes_to_stringbuilder_buffer(buf, "\\t", 2);
                break;
            case '\r':
                err = append_bytes_to_stringbuilder_buffer(buf, "\\r", 2);
                break;
            // todo: control characters?
            default:
                err = append_char_to_stringbuilder_buffer(buf, *c);
                break;
        }
        if (0 != err) {
            return err;
        }
    }
    err = append_char_to_stringbuilder_buffer(buf, '"');
    // last instance will be 0 for success or error
    return err;
}
