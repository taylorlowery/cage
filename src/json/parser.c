#include "json.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARRAY_CAPACITY (1 << 16) // 65536

static JsonPair *json_pair(Parser *parser);
static void json_object(Parser *parser, JsonValue *value);
static void json_array(Parser *parser, JsonValue *value);
static JsonValue *json_value(Parser *parser);

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
  fprintf(parser->error_stream, "Error ");

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

// allocates a buffer, copies a token's string to it,
// and returns a pointer to the buffer.
// caller must free.
static char *extract_token_str(Token token, FILE *error_stream) {
    char *buf = calloc(token.length + 1, sizeof(char));
    if (NULL == buf) {
      fprintf(error_stream, "unable to allocate buffer");
      return NULL;
    }
    const char *start = token.start;
    int len = token.length;
    // strip any quotes from start/end.
    if (*token.start == '"') {
      start += 1;
      len -=1;
    }
    if (len > 0 && *(token.start + token.length - 1) == '"') {
      len -= 1;
    }
    memcpy(buf, start, len);
    buf[len] = '\0';
    return buf;
}

static void realloc_json_array(Parser *parser, JsonArray *json_array) {
  // TODO: more sophisticated capacity increase?
  size_t new_capacity = json_array->capacity < 8 ? 8 : json_array->capacity * 2;
  if (new_capacity > MAX_ARRAY_CAPACITY) {
    error_at_current(parser, "json array exceeds maximum capacity");
    return;
  }
  JsonValue *new_item_array = realloc(json_array->items, new_capacity * sizeof(JsonValue));
  if (NULL == new_item_array) {
    error_at_current(parser, "out of memory during parse");
    return;
  }
  json_array->items = new_item_array;
  json_array->capacity = new_capacity;
}

static void realloc_json_object(Parser *parser, JsonObject *json_object) {
  size_t new_capacity = json_object->capacity < 8 ? 8 : json_object->capacity * 2;
  if (new_capacity > MAX_ARRAY_CAPACITY) {
    error_at_current(parser, "json object exceeds maximum capacity");
    return;
  }
  JsonPair *new_pairs = realloc(json_object->pairs, new_capacity * sizeof(JsonPair));
  if (NULL == new_pairs) {
    error_at_current(parser, "out of memory during object parse");
    return;
  }
  json_object->pairs = new_pairs;
  json_object->capacity = new_capacity;
}

static void free_json_value_contents(JsonValue *value) {
  switch(value->type) {
    case JSON_ARRAY:
      if (NULL == value->as.array) {
        break;
      }
      if (NULL != value->as.array->items) {
        for (size_t i = 0; i < value->as.array->count; i++) {
          free_json_value_contents(&(value->as.array->items[i]));
        }
        free(value->as.array->items);
      }
      free(value->as.array);
      break;
    case JSON_OBJECT:
      if (NULL == value->as.object) {
        break;
      }
      if (NULL != value->as.object->pairs) {
        for (size_t i = 0; i < value->as.object->count; i++) {
          free_json_value((value->as.object->pairs[i].value));
          free(value->as.object->pairs[i].key);
        }
        free(value->as.object->pairs);
      }
      free(value->as.object);
      break;
    case JSON_STRING:
      free(value->as.string);
      break;
    default:
      break;
  }
}

// iterate through json value and free all pointers
void free_json_value(JsonValue *value) {
  if (NULL == value) {
    return;
  }
  free_json_value_contents(value);
  free(value);
}

// grammar for JSON
// json_value -> json_object | array | string | number | "true" | "false" | "null" ;
// json_object -> "{" (json_pair ("," json_pair)* )? "}" ;
// json_pair -> string ":" value
// array -> "[" (value ("," value) * )? "]"

static JsonPair *json_pair(Parser *parser) {
  JsonPair *pair = calloc(1, sizeof(JsonPair));
  if (NULL == pair) {
    fprintf(parser->error_stream, "unable to allocate for kv pair");
    return NULL;
  }
  // consume key
  consume(parser, TOKEN_STRING, "expected string key for pair");
  pair->key = extract_token_str(parser->previous, parser->error_stream);
  // consume colon
  consume(parser, TOKEN_COLON, "expected colon after string key");
  // consume value with json_value(parser, value);
  pair->value = json_value(parser);
  // free all this stuff on error
  
  return pair;
}


static void json_object(Parser *parser, JsonValue *value) {
  consume(parser, TOKEN_LEFTBRACE, "expected left brace '{'");
  if (parser->had_error) {
    return;
  }
  value->type = JSON_OBJECT;
  // allocate space for json object (capactity will be 0)
  value->as.object = calloc(1, sizeof(JsonObject));
  if (NULL == value->as.object) {
    error_at_current(parser, "unable to allocate new json object");
    return;
  }

  // check and return early on empty object
  if (check(parser, TOKEN_RIGHTBRACE)) {
    consume(parser, TOKEN_RIGHTBRACE, "expected right brace '}'");
    return;
  }

  // not empty, 
  // realloc the object to have a greater capacity
  realloc_json_object(parser, value->as.object);
  if (parser->had_error) {
    return;
  }

  // next should always be string?
  if (!check(parser, TOKEN_STRING)) {
    error_at_current(parser, "expected string key");
    return;
  }
  
  // get first kv pair
  JsonPair *pair = json_pair(parser);
  if (pair == NULL) {
    error_at_current(parser, "oh no, error parsing kv pair");
    return;
  }
  value->as.object->pairs[value->as.object->count] = *pair;
  value->as.object->count += 1;
  free(pair);

  while (match(parser, TOKEN_COMMA)) {
    JsonPair *pair = json_pair(parser);
    if (pair == NULL) {
      error_at_current(parser, "oh no, error parsing kv pair");
      return;
    }
    value->as.object->pairs[value->as.object->count] = *pair;
    value->as.object->count += 1;
    if (value->as.object->count == value->as.object->capacity) {
      realloc_json_object(parser, value->as.object);
      if (parser->had_error) {
        free(pair);
        return;
      }
    }
      free(pair);
  }

  consume(parser, TOKEN_RIGHTBRACE, "expected end of object");
  return;
}

static void json_array(Parser *parser, JsonValue *value) {
  consume(parser, TOKEN_LEFTBRACKET, "expected array to begin with left bracket '['");
  if (parser->had_error) {
    return;
  }
  value->type = JSON_ARRAY;
  // allocate space for the array items (capacity is 0)
  value->as.array = calloc(1, sizeof(JsonArray));
  if (NULL == value->as.array) {
    error_at_current(parser, "unable to allocate json array");
    return;
  }

  // check for empty array
  if (check(parser, TOKEN_RIGHTBRACKET)) {
    consume(parser, TOKEN_RIGHTBRACKET, "expected end of array '[]'");
    return;
  }


  // not empty, so reallocate for more space
  realloc_json_array(parser, value->as.array);
  if (parser->had_error) {
    // rats
    return;
  }
  
  // get first value
  JsonValue *first_array_value = json_value(parser);
  if (NULL == first_array_value) {
    return;
  }
  value->as.array->items[value->as.array->count] = *first_array_value;
  value->as.array->count += 1;
  free(first_array_value);

  while (match(parser, TOKEN_COMMA)) {
    JsonValue *next_array_value = json_value(parser);
    if (NULL == next_array_value) {
      // TODO: should I just continue?
      return;
    }
    value->as.array->items[value->as.array->count] = *next_array_value;
    value->as.array->count += 1;
    if (value->as.array->count == value->as.array->capacity) {
      realloc_json_array(parser, value->as.array);
      if (parser->had_error) {
        return;
      }
    }
    free(next_array_value);
  }

  consume(parser, TOKEN_RIGHTBRACKET, "expected end of array");
  return;
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
        char *num_str = extract_token_str(parser->current, parser->error_stream);
        value->as.number = strtod(num_str, NULL);
        free(num_str);
        break;
      }
    case TOKEN_STRING:
      value->type = JSON_STRING;
      value->as.string = extract_token_str(parser->current, parser->error_stream);
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
      free(value);
      return NULL;
  }
  // TODO: address a bug where this double-advances
  // on objects and arrays.
  advance(parser);
  return value;
}


// parse() accepts an initialized parser and scanner
// and parses the JSON.
JsonValue *parse_json(Parser *parser) {
  // move initial token into current
  advance(parser);
  // recursively iterate through parser
  JsonValue *value = json_value(parser);
  if (NULL == value) {
    return NULL;
  }
  // if there was an error parsing,
  // free the json value we parsed and return null
  if (parser->had_error) {
    free_json_value(value);
    return NULL;
  }
  // validate the end of the file and return what we parsed successfully
  consume(parser, TOKEN_EOF, "expect end of input");
  // if there was an error parsing,
  // free the json value we parsed and return null
  if (parser->had_error) {
    free_json_value(value);
    return NULL;
  }
  return value;
}

