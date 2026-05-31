#include "json.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void init_parser(Parser *parser, const char *source, FILE *error_stream) {
  init_scanner(&parser->scanner, source);
  parser->current = (Token){.type = TOKEN_EOF, .start = NULL, .length = 0};
  parser->previous = (Token){.type = TOKEN_EOF, .start = NULL, .length = 0};
  parser->had_error = false;
  parser->panic_mode = false;
  parser->error_stream = error_stream;
}


// error_at prints an error message about a given token to a given output stream.
// TODO: track "previous" token to more easily locate token in input string?
static void error_at(Parser *parser, const Token *token, const char *message) {
  // TODO: print some message to assist with locating token in parser (in lieu of line number)
  if (parser->panic_mode) {
    return;
  }
  parser->panic_mode = true;
  fprintf(parser->error_stream, "Error");

  switch (token->type) {
    case TOKEN_EOF:
      fprintf(parser->error_stream, "at end");
      break;
    case TOKEN_ERROR:
      // nothing?
      break;
    default:
      fprintf(parser->error_stream, "at '%.*s'", token->length, token->start);
  }

  fprintf(parser->error_stream, ": %s\n", message);

  parser->had_error = true;
}

static void error_at_current(Parser *parser, const char *message) {
  error_at(parser, &parser->current, message);
}

// WIP
// TODO: address spin on error
static void advance(Parser *parser) {
  parser->previous = parser->current;
  parser->current = scan_token(&parser->scanner);
  

  for (;;) {
    if (parser->current.type != TOKEN_ERROR) {
      break;
    }
    error_at_current(parser, parser->current.start);
    parser->current = scan_token(&parser->scanner);
  }
}

static void consume(Parser *parser, TokenType token_type, const char *message) {
  if (token_type == parser->current.type) {
    advance(parser);
    return;
  }

  error_at_current(parser, message);
}

Token peek(Parser *parser) {
  return parser->current;
}

bool is_at_end(Parser *parser) {
  return peek(parser).type == TOKEN_EOF;
}

bool check(Parser *parser, TokenType token_type) {
  if (is_at_end(parser)) {
    return false;
  }
  return peek(parser).type == token_type;
}

static bool match(Parser *parser, TokenType token_type) {
  if (check(parser, token_type)) {
    advance(parser);
    return true;
  }
  return false;
}


// grammar for JSON
// json_value -> json_object | array | string | number | "true" | "false" | "null" ;
// json_object -> "{" (json_pair ("," json_pair)* )? "}" ;
// json_pair -> string ":" value
// array -> "[" (value ("," value) * )? "]"

static void json_object(Parser *parser, JsonValue *value) {}

static void json_array(Parser *parser, JsonValue *value) {}

// allocates a buffer, copies the contents of parser's current token to it,
// and returns a pointer to the buffer.
// caller must free.
static char *parser_current_str(Parser *parser) {
    char *buf = calloc(parser->current.length + 1, sizeof(char));
    if (NULL == buf) {
      fprintf(parser->error_stream, "unable to allocate buffer");
      return NULL;
    }
    memcpy(buf, parser->current.start, parser->current.length);
    buf[parser->current.length] = '\0';
    return buf;
}

static JsonValue *json_value(Parser *parser) {
  JsonValue *value = calloc(1, sizeof(JsonValue));
  if (NULL == value) {
    // honestly not sure if this is the right way to error for this.
    error_at_current(parser, "unable to calloc json value");
    return NULL;
  } 
  switch(parser->current.type) {
    case TOKEN_LEFTBRACE:
      json_object(parser, value);
      // return here to avoid double advance() at end of switch
      return value;
    case TOKEN_LEFTBRACKET:
      json_array(parser, value);
      // return here to avoid double advance() at end of switch
      return value;
    case TOKEN_NUMBER: {
        value->type = JSON_NUMBER;
        char *num_str = parser_current_str(parser);
        value->as.number = strtod(num_str, NULL);
        free(num_str);
        break;
      }
    case TOKEN_STRING:
      value->type = JSON_STRING;
      value->as.string = parser_current_str(parser);
      break;
    case TOKEN_TRUE:
      value->type = JSON_BOOL;
      value->as.boolean = true;
      break;
    case TOKEN_FALSE:
      value->type = JSON_BOOL;
      value->as.boolean = false;
      break;
    case TOKEN_NULL:
      value->type = JSON_NULL;
      break;
    default:
      error_at_current(parser, "invalid value");
      return NULL;
  }
  // TODO: address a bug where this double-advances
  // on objects and arrays.
  advance(parser);
  return value;
}


// parse() accepts an initialized parser and scanner
// and parses the JSON.
bool parse(Parser *parser) {
  advance(parser);
  json_value(parser);
  consume(parser, TOKEN_EOF, "expect end of input");
  return !(parser->had_error);
}

