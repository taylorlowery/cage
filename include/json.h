/*
 *  A library for parsing, serializing, and deserializing JSON,
 *  following the Scanner and Parser chapters in Robert Nystrom's "Crafting Interpreters"
 */
#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    const char* start;
    const char* current;
} Scanner;


typedef enum {
    TOKEN_LEFTBRACE, TOKEN_RIGHTBRACE,
    TOKEN_LEFTBRACKET, TOKEN_RIGHTBRACKET,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
} Token;

typedef struct {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
  FILE *error_stream;
  Scanner scanner;
} Parser;

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;


typedef struct JsonValue JsonValue;

typedef struct {
    JsonValue *items;
    size_t count;
    size_t capacity;
} JsonArray;

typedef struct {
    char* key;
    JsonValue *value;
} JsonPair;

typedef struct {
    JsonPair *pairs;
    size_t count;
    size_t capacity;
} JsonObject;

typedef struct JsonValue{
    JsonType type;
    union {
        bool boolean;
        double number;
        char *string;
        JsonArray *array;
        JsonObject *object;
    } as;
} JsonValue;

void init_scanner(Scanner *scanner, const char *source);
Token scan_token(Scanner *scanner);
void init_parser(Parser *parser, const char *source, FILE *error_stream);
JsonValue *parse_json(Parser *parser);
void free_json_value(JsonValue *value);


#endif // JSON_H
