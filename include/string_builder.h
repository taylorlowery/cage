#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stdlib.h>
typedef struct {
    char *buffer;
    size_t buffer_length;
    size_t buffer_capacity;
} StringBuilder;

// resize the string builder buffer to a required capacity.
int resize_stringbuilder_buffer(StringBuilder *buf, size_t required_capacity);

// append a character to the stringbuilder buffer.
// resizes the stringbuilder buffer if it reaches capacity.
int append_char_to_stringbuilder_buffer(StringBuilder *buf, const char c);

// append a series of chars to the stringbuilder buffer.
// resizes the stringbuilder buffer if it reaches capacity.
int append_bytes_to_stringbuilder_buffer(StringBuilder *buf, const char *src, size_t src_len);

// append a string to the string builder buffer,
// with characters escaped for use as formatted JSON.
int append_json_string_to_stringbuilder_buffer(StringBuilder *buf, const char *string);
#endif
