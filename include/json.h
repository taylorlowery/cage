/*
 *  A library for parsing, serializing, and deserializing JSON,
 *  following the Scanner and Parser chapters in Robert Nystrom's "Crafting Interpreters"
 */
#ifndef JSON_H
#define JSON_H

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

void initScanner(Scanner *scanner, const char *source);
Token scanToken(Scanner *scanner);

#endif // JSON_H
