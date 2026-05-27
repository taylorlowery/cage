#include "json.h"
#include <string.h>
#include <stdbool.h>
#include <assert.h>


void initScanner(Scanner *scanner, const char *source) {
    assert(NULL != scanner);
    assert(NULL != source);
    scanner->start = source;
    scanner->current = source;
}

// checks whether we are at the end of the scanner stream,
// based on whether the current char is the null terminator.
static bool isAtEnd(Scanner *scanner) {
    return *scanner->current == '\0';
}

// Generate a token object for a given tokentype.
static Token makeToken(Scanner *scanner, TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    return token;
}

// create a token object with a given error message.
static Token errorToken(const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    return token;
}

// advance to the next character in the scanner stream. 
static char advance(Scanner *scanner) {
    if (isAtEnd(scanner)) {
        return '\0';
    }
    scanner->current++;
    return scanner->current[-1];
}

// return the next character
static char peek(Scanner *scanner) {
    if (isAtEnd(scanner)) {
        return '\0';
    }
    return *(scanner->current);
}

// return the character after next
static char peekNext(Scanner *scanner) {
    if (isAtEnd(scanner)) {
        return '\0';
    }
    return scanner->current[1];
}

// advance past whitespace in the scanner stream.
static void skipWhitespace(Scanner *scanner) {
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
    while(peek(scanner) != '"' && !isAtEnd(scanner)) {
        // if the next character is a backslash,
        // skip it
        if (peek(scanner) == '\\') {
            advance(scanner);
        }
        // advance past the normal or escaped char
        advance(scanner);
    }
    if (isAtEnd(scanner)) {
        return errorToken("unterminated string");
    }
    advance(scanner);
    return makeToken(scanner, TOKEN_STRING);
}

static bool isDigit(const char c) {
    return '0' <= c && c <= '9';
}

static bool isAlpha(const char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('_' == c);
}

// number() attempts to tokenize a positive or negative number
// from the scanner's current stream.
// TODO: stricting parsing according to JSON specification,
// ie, no leading zeroes for integers, support scientific notation.
static Token number(Scanner *scanner) {
    while (isDigit(peek(scanner))) {
        advance(scanner);
    }
    // look for fractional parts:
    if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        advance(scanner);
        while (isDigit(peek(scanner))) {
            advance(scanner);
        }
    }
    return makeToken(scanner, TOKEN_NUMBER);
}

// checkKeyword validates that the next several characters of
// string input match those expected for a given keyword. If they
// do, returns the token type that represents the keyword.
static TokenType checkKeyword(Scanner *scanner, const int start, const int length, const char *rest, const TokenType tokenType) {
    if ((scanner->current - scanner->start == start + length) && (memcmp(scanner->start + start, rest, length) == 0)) {
        return tokenType;
    }
    return TOKEN_ERROR;
}

static TokenType keyword(Scanner *scanner) {
    while (isAlpha(peek(scanner))) {
        advance(scanner);
    }
    switch(scanner->start[0]) {
        case 't':
            return checkKeyword(scanner, 1, 3, "rue", TOKEN_TRUE);
        case 'f':
            return checkKeyword(scanner, 1, 4, "alse", TOKEN_FALSE);
        case 'n':
            return checkKeyword(scanner, 1, 3, "ull", TOKEN_NULL);
        default:
            return TOKEN_ERROR;
    }
}
 
Token scanToken(Scanner *scanner) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;
    if(isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF);

    char c = advance(scanner);
    if (isAlpha(c)) {
        TokenType tokenType = keyword(scanner);
        if (TOKEN_ERROR == tokenType) {
            return errorToken("invalid keyword");
        }
        return makeToken(scanner, tokenType);
    }
    if (isDigit(c)) {
        return number(scanner);
    }
    switch (c) {
        case '[': return makeToken(scanner, TOKEN_LEFTBRACKET);
        case ']': return makeToken(scanner, TOKEN_RIGHTBRACKET);
        case '{': return makeToken(scanner, TOKEN_LEFTBRACE);
        case '}': return makeToken(scanner, TOKEN_RIGHTBRACE);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case ':': return makeToken(scanner, TOKEN_COLON);
        case '"': return string(scanner);
        case '-':
            if (isDigit(peek(scanner))) {
                return number(scanner);
            }
            return errorToken("expected digit after '-'");
    }
    
    return errorToken("unexpected character");
}
