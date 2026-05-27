#include "json.h"
#include <string.h>
#include <stdbool.h>
#include <assert.h>


void init_scanner(Scanner *scanner, const char *source) {
    assert(NULL != scanner);
    assert(NULL != source);
    scanner->start = source;
    scanner->current = source;
}

// checks whether we are at the end of the scanner stream,
// based on whether the current char is the null terminator.
static bool is_at_end(Scanner *scanner) {
    return *scanner->current == '\0';
}

// Generate a token object for a given tokentype.
static Token make_token(Scanner *scanner, TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    return token;
}

// create a token object with a given error message.
static Token error_token(const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    return token;
}

// advance to the next character in the scanner stream.
static char advance(Scanner *scanner) {
    if (is_at_end(scanner)) {
        return '\0';
    }
    scanner->current++;
    return scanner->current[-1];
}

// return the next character
static char peek(Scanner *scanner) {
    if (is_at_end(scanner)) {
        return '\0';
    }
    return *(scanner->current);
}

// return the character after next
static char peek_next(Scanner *scanner) {
    if (is_at_end(scanner)) {
        return '\0';
    }
    return scanner->current[1];
}

// advance past whitespace in the scanner stream.
static void skip_whitespace(Scanner *scanner) {
    for (;;) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                advance(scanner);
                break;
            default:
                return;
        }
    }
}

static Token string(Scanner *scanner) {
    while(peek(scanner) != '"' && !is_at_end(scanner)) {
        // if the next character is a backslash,
        // skip it
        if (peek(scanner) == '\\') {
            advance(scanner);
        }
        // advance past the normal or escaped char
        advance(scanner);
    }
    if (is_at_end(scanner)) {
        return error_token("unterminated string");
    }
    advance(scanner);
    return make_token(scanner, TOKEN_STRING);
}

static bool is_digit(const char c) {
    return '0' <= c && c <= '9';
}

static bool is_alpha(const char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('_' == c);
}

// number() attempts to tokenize a positive or negative number
// from the scanner's current stream.
// TODO: stricting parsing according to JSON specification,
// ie, no leading zeroes for integers, support scientific notation.
static Token number(Scanner *scanner) {
    while (is_digit(peek(scanner))) {
        advance(scanner);
    }
    // look for fractional parts:
    if (peek(scanner) == '.' && is_digit(peek_next(scanner))) {
        advance(scanner);
        while (is_digit(peek(scanner))) {
            advance(scanner);
        }
    }
    return make_token(scanner, TOKEN_NUMBER);
}

// checkKeyword validates that the next several characters of
// string input match those expected for a given keyword. If they
// do, returns the token type that represents the keyword.
static TokenType check_keyword(Scanner *scanner, const int start, const int length, const char *rest, const TokenType tokenType) {
    if ((scanner->current - scanner->start == start + length) && (memcmp(scanner->start + start, rest, length) == 0)) {
        return tokenType;
    }
    return TOKEN_ERROR;
}

static TokenType keyword(Scanner *scanner) {
    while (is_alpha(peek(scanner))) {
        advance(scanner);
    }
    switch(scanner->start[0]) {
        case 't':
            return check_keyword(scanner, 1, 3, "rue", TOKEN_TRUE);
        case 'f':
            return check_keyword(scanner, 1, 4, "alse", TOKEN_FALSE);
        case 'n':
            return check_keyword(scanner, 1, 3, "ull", TOKEN_NULL);
        default:
            return TOKEN_ERROR;
    }
}

Token scan_token(Scanner *scanner) {
    skip_whitespace(scanner);
    scanner->start = scanner->current;
    if(is_at_end(scanner)) return make_token(scanner, TOKEN_EOF);

    char c = advance(scanner);
    if (is_alpha(c)) {
        TokenType tokenType = keyword(scanner);
        if (TOKEN_ERROR == tokenType) {
            return error_token("invalid keyword");
        }
        return make_token(scanner, tokenType);
    }
    if (is_digit(c)) {
        return number(scanner);
    }
    switch (c) {
        case '[': return make_token(scanner, TOKEN_LEFTBRACKET);
        case ']': return make_token(scanner, TOKEN_RIGHTBRACKET);
        case '{': return make_token(scanner, TOKEN_LEFTBRACE);
        case '}': return make_token(scanner, TOKEN_RIGHTBRACE);
        case ',': return make_token(scanner, TOKEN_COMMA);
        case ':': return make_token(scanner, TOKEN_COLON);
        case '"': return string(scanner);
        case '-':
            if (is_digit(peek(scanner))) {
                return number(scanner);
            }
            return error_token("expected digit after '-'");
    }

    return error_token("unexpected character");
}
