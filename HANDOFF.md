# CAGE Project Handoff

## Context

This is a learning capstone project: implementing Thorsten Ball's "How to Build an Agent" (https://ampcode.com/notes/how-to-build-an-agent) in C.

The goal is not to get to a polished product quickly, but to learn C deeply: sockets, SSL, manual memory management, JSON parsing, and clean module design.

## Current State of the Codebase

- `main.c`: Skeleton `main()` that prints a greeting.
- `src/agent.c` / `include/agent.h`: Stub module with a `greet()` function used to validate the build and test setup.
- `src/anthropic.c` / `include/anthropic.h`: Partial Anthropic provider module using `libcurl`. This was an early experiment and will be replaced by a custom sockets/OpenSSL HTTP client.
- `src/http_client.c` / `include/http_client.h`: **Phase 1 & 2 complete.** Raw BSD sockets + OpenSSL HTTP client. Decomposed `http_connect` / `http_send` / `http_receive` interface with a `Connection` struct abstracting both plain TCP and TLS. `http_request()` for HTTP, `https_request()` for HTTPS. Handles partial reads via `recv_all` with dynamic buffer resizing. Parses status code and extracts body after `\r\n\r\n`. Tested against `http://example.com:80` and `https://example.com:443`.
- `test/test_agent.c`: Unity test framework set up and running. Tests currently exercise the stub `greet()` function.
- `test/test_json.c`: Parser tests (21 tests) alongside lexer tests (25 tests, all in the same file). All passing with zero warnings. Covers: empty composites, single primitives, single-element arrays/objects, complex nested, growth beyond initial capacity (9+ elements/pairs), truncated inputs, trailing comma, invalid elements, non-string keys, missing brackets/braces, extra data after valid JSON, empty/whitespace input, JSON-like string values, and a real Anthropic API response.
- `Makefile`: Builds `cage` (main binary) and `test_runner` (tests). Uses `zig cc` with `-Wall -Wextra -Wpedantic`. Parser source wired in, all tests in a single `test_runner` binary.
- `compile_flags.txt`: For IDE/LSP integration.

## Architecture Plan

### Modules (proposed)

```
src/
  http_client.c       # Raw sockets + OpenSSL HTTP client
  json/
    lexer.c           # JSON tokenizer (complete, tested)
    parser.c          # Recursive descent parser (WIP)
  agent.c             # Core conversation loop
  anthropic.c         # Provider-specific API formatting (uses http_client)
  tools.c             # Tool registry and dispatcher
  tool_read_file.c    # read_file implementation
  tool_list_files.c   # list_files implementation
  tool_edit_file.c    # edit_file implementation
  utils.c             # Safe string helpers, buffer utilities

include/
  http_client.h
  json.h              # Scanner, Token, Parser, JsonValue definitions
  agent.h
  anthropic.h
  tools.h
  utils.h
```

### Design Decisions (Made So Far)

1. **HTTP Client**: Replace `libcurl` with raw BSD sockets + OpenSSL.
   - Phase 1: ✅ Complete. Plain TCP tested against `http://example.com:80`.
   - Phase 2: ✅ Infrastructure complete. `Connection` struct wraps `int sockfd` + `SSL *ssl`; `conn_write`/`conn_read` dispatch to `send`/`recv` or `SSL_write`/`SSL_read`. `ssl_connect` creates `SSL_CTX`, performs TLS handshake (`SSL_connect`), and returns a `Connection`. Tested against `https://example.com:443` and `https://httpbin.org:443` with both GET and POST. Remaining: smoke test against `https://api.anthropic.com/v1/messages`.
2. **HTTP Client Decomposition**: Split `http_request()` into `http_connect()` / `http_send()` / `http_receive()`. SSL inserts as a layer between connect and I/O: `ssl_connect()` calls `http_connect()` then wraps the fd with `SSL_set_fd` + `SSL_connect`. `conn_write`/`conn_read` dispatch to `send`/`recv` or `SSL_write`/`SSL_read` based on `conn->ssl`. A shared `free_connection()` helper cleans up both paths. Connect and parse logic are protocol-agnostic.

3. **JSON Parser**: Write a general-purpose mini-parser from scratch, following *Crafting Interpreters* recursive descent pattern.
    - **Capacity growth**: `realloc_json_array` and `realloc_json_object` double from 8, capped at `MAX_ARRAY_CAPACITY` (1 << 16 = 65536). Reports parser error on cap hit or OOM. Aspirational Power of 10 Rule #2 (bounded loops/growth).
   - **Lexer (`src/json/lexer.c`)**: ✅ Complete. Tokenizes objects, arrays, strings (with escape handling), numbers (int/float/negative), booleans, null, punctuation. Comprehensive Unity tests in `test/test_json.c`. All tests pass.
   - **Parser (`src/json/parser.c`)**: 🚧 WIP. Parser machinery (advance, consume, match, check, error handling) implemented. Data model (`JsonValue` tagged union, `JsonArray`, `JsonObject`) defined in `include/json.h`.
   - `json_value()` handles primitives: `null`, `true`, `false`, numbers (via `strtod`), strings (copied with `calloc`+`memcpy`).
    - `json_object()` is **implemented**. Consumes `{`, checks empty before growing, extracts `json_pair()` helper, grows `JsonPair` array with `realloc_json_object()`, and loops on `match(TOKEN_COMMA)`. Reallocator guards (`had_error` checks) present in both initial and loop paths.
    - `json_array()` is **implemented**. Consumes `[`, checks empty before growing, loops with `json_value()` and `match(TOKEN_COMMA)`, grows `JsonValue` array with `realloc_json_array()`, consumes `]`. Copies `JsonValue` structs inline into array (`items[count] = *temp; free(temp)`).
    - `parse()` changed to `parse_json(Parser *parser)` returning `JsonValue *`. Returns `NULL` on parse error or OOM. Calls `free_json_value(value)` on `had_error` before returning `NULL`.
    - `free_json_value_contents()` extracted as a static helper that frees contents without `free(value)`. Array loop calls `free_json_value_contents()` on inline elements. `free_json_value()` calls the contents helper then `free(value)`. Heap-corruption bug is **fixed**.
    - **Parser tests: 4 tests written and passing** (`test_empty_object`, `test_empty_array`, `test_truncated_object_returns_null`, `test_array_with_multiple_empty_objects`). All in `test/test_json.c` alongside lexer tests. `parser.c` is wired into `Makefile`.

4. **Tool Dispatch**: Explore both function pointers and `switch` statement dispatch.
   - Function pointers are idiomatic C but violate Power of 10 Rule #9.
   - A `switch` on an enum (`TOOL_READ_FILE`, `TOOL_LIST_FILES`, etc.) inside a single dispatcher is analyzable but less extensible.
   - We will likely implement both as an exercise and settle on the `switch` statement for the final design.

5. **Testing Strategy**:
   - **Unit tests**: Mock HTTP server or mock provider for testing logic without network/API keys.
   - **Integration tests**: Optional real API calls gated behind environment variables (e.g., `RUN_INTEGRATION_TESTS=1`).
   - JSON parser gets extensive standalone tests.

6. **Power of 10 Rules**:
   - Acknowledged as aspirational, not blocking on first pass.
   - Rule #3 (no dynamic allocation after init) will be intentionally relaxed for this project due to variable-size HTTP responses and file contents. We may explore hard caps (e.g., `MAX_RESPONSE_SIZE`) or arena allocators later.
   - Rules #1, #2, #4, #5, #6, #7, #8, #10 will be aimed for and reviewed during code review.
   - Rule #9 (no function pointers) will guide the final tool dispatch choice.

## Phased Implementation Roadmap

### Phase 1: HTTP Client (Sockets) — ✅ COMPLETE
- ✅ Implement `http_client.h` interface.
- ✅ Write `http_request()` (supports GET and POST via `HttpMethod` enum) that resolves hostnames (`getaddrinfo`), creates a socket, connects, sends an HTTP/1.1 request, and reads the response into a buffer.
- ✅ Decompose into `http_connect()` / `http_send()` / `http_receive()` to prepare for SSL insertion.
- ✅ Handle partial reads (`recv` returning less than expected) via `recv_all` with dynamic `realloc`-based buffer growth.
- ✅ Parse HTTP status code and extract body after `\r\n\r\n`.
- ✅ Test target: `http://example.com:80`.
- ✅ Success criteria met: Compiles with zero warnings, successfully prints response body to stdout.

### Phase 2: HTTP Client (OpenSSL) — ✅ INFRASTRUCTURE COMPLETE
- ✅ `Connection` struct abstracts both HTTP and HTTPS (`int sockfd` + `SSL *ssl`).
- ✅ `conn_write` / `conn_read` dispatch to `send`/`recv` or `SSL_write`/`SSL_read`.
- ✅ `ssl_connect()` creates `SSL_CTX` with peer verification, TLS 1.2+ minimum, performs `SSL_connect` handshake, and returns a `Connection`.
- ✅ `https_request()` orchestrates `ssl_connect` → `http_send` → `http_receive` → `free_connection`.
- ✅ `free_connection()` handles both SSL shutdown/cleanup and socket close.
- ✅ Tests: HTTP GET/POST against example.com:80 and httpbin.org:80; HTTPS GET/POST against example.com:443 and httpbin.org:443. All 5 tests pass.
- ⏳ **Remaining**: Smoke test against `https://api.anthropic.com/v1/messages` with a hardcoded JSON payload and `x-api-key` header.

**Known issues / remaining TODOs:**
- Response header parsing not implemented yet (`http_response->headers` is always NULL).
- `build_headers` hardcodes `Content-Type: application/json` and `Connection: close` — will need dynamic header passing for `x-api-key` and other provider headers.
- `http_method_to_str` returns NULL for unknown methods — handled by `build_headers`, but could be an assertion instead.

### Phase 2: HTTP Client (OpenSSL)
- ✅ Wrap the socket in OpenSSL (`SSL_CTX_new`, `SSL_new`, `SSL_set_fd`, `SSL_connect`).
- ✅ Replace `send`/`recv` with `SSL_write`/`SSL_read` via `conn_write`/`conn_read`.
- ✅ Tested against `https://example.com:443` and `https://httpbin.org:443`.
- ⏳ **Final test target**: `https://api.anthropic.com/v1/messages` with a hardcoded JSON payload and `x-api-key` header.
- **Success criteria**: Receives a valid HTTP 200 response from Anthropic.

### Phase 3: JSON Parser — 🚧 IN PROGRESS
- ✅ Lexer complete: tokenizes all JSON primitives and punctuation. Comprehensive tests passing.
- ✅ Parser machinery: `advance()`, `consume()`, `match()`, `check()`, error reporting with panic mode.
- ✅ `JsonValue` tagged union defined with `JsonArray` / `JsonObject` container types.
- ✅ `json_value()` handles primitives: `null`, `true`, `false`, `number`, `string`.
- ✅ `parse_json()` entry point orchestrates `advance → json_value → consume(EOF)`, returns `JsonValue *` or `NULL`.
- ✅ `json_object()` implemented with reallocator guards and empty-object early return.
- ✅ `json_array()` implemented with inline struct copies and empty-array early return.
- ✅ `json_value()` default case now frees its own `calloc`'d `value` on invalid token.
- ✅ **`free_json_value()` heap-corruption bug fixed**: Split into `free_json_value_contents()` (static, frees contents only) and `free_json_value()` (contents + `free(value)`). Array loop calls contents-only helper on inline elements. No `free()` on mid-allocation pointers.
- ✅ **`parser.c` wired into Makefile** — compiles with zero warnings.
- ✅ **21 parser unit tests written and passing**: covers empty/truncated composites, single primitives, single-element arrays/objects, growth beyond initial capacity, deeply nested, empty/whitespace inputs, extra data after valid JSON, JSON-like string values, and a real Anthropic API response.
- ⏳ **Next**: Expand parser test coverage (primitives, nested structures, error cases, complex real-world inputs). Use `fmemopen` to capture and assert error output instead of `stdout`.
- **Success criteria**: Can parse a known Anthropic API response structure correctly, with Valgrind/ASan-clean memory.

### Phase 4: Provider Abstraction (Anthropic)
- ✅ **JSON serialization for Anthropic requests**: `serialize_request_body()` in `src/anthropic.c` serializes `AnthropicRequest` (model, max_tokens, messages array) to a JSON string buffer.
- ✅ **`test_serialize_request_body_single_message`**: Tests single-user-message serialization. Passing.
- ✅ **`test_serialize_request_body_multiple_messages`**: Tests multi-turn conversation with system, user, assistant, user roles. Passing.
- ⏳ **Next**: JSON deserialization of Anthropic API responses (extract assistant reply from JSON).
- ⏳ **Next**: Wire `serialize_request_body` into `http_client` to send real API requests.
- ⏳ **Next**: Implement JSON deserialization (using `json.c`) to extract the assistant's reply.

### Phase 5: Agent Loop & Tools
- Implement the main conversation loop.
- Register tools (`read_file`, `list_files`, `edit_file`).
- Implement tool dispatch (try function pointers, then `switch`/enum).
- Connect everything: user input → agent loop → API call → tool use → file system action → back to loop.

## Future Phases / Extension Ideas

These are intermediate-to-advanced C topics that would extend the project once core functionality is working. Not required for a functional agent, but valuable for deepening systems programming knowledge.

### Phase 6: Concurrency with pthreads
Add threading to make the agent responsive and interruptible:
- Background thread for API calls so user can type/interrupt while waiting
- Mutex-protected shared state (conversation history)
- Clean thread cancellation and shutdown patterns
- **Learning value:** Thread lifecycle, mutexes, condition variables, async-safety boundaries

### Phase 7: Signal Handling
Graceful handling of `SIGINT` (Ctrl+C) and other signals:
- Set `volatile sig_atomic_t` flags in signal handlers (strict async-safety rules)
- Main loop checks flag, joins threads, frees memory, exits cleanly
- **Learning value:** C signal semantics, what can/cannot be called in signal context, kernel interruption model

### Phase 8: Arena / Pool Allocators
Replace malloc-per-node with structured memory management:
- Arena allocation: `arena_new(size_t cap)`, `arena_alloc()`, `arena_free_all()`
- Per-request arena: allocate entire JSON parse tree from arena, bulk-free at end
- Makes Power of 10 Rule #3 achievable (no dynamic alloc after init per request)
- **Learning value:** Custom allocators, memory pools, fragmentation avoidance

### Phase 9: Event Loop with select()/poll()
Non-blocking I/O using `select()` or `poll()`:
- Wait on multiple file descriptors (stdin for user input, socket for API response, pipe for cancellation)
- Single-threaded concurrency without pthread complexity
- Foundation of async runtimes (Node.js, Redis, Nginx all use this pattern)
- **Learning value:** File descriptor sets, timeout handling, state machines

**Recommended order:** Add pthreads (Phase 6) + signal handling (Phase 7) first—they directly improve UX and teach the hardest part of systems programming (coordinating concurrent operations). Skip arena allocators until Phase 3's JSON parser makes malloc tracking painful. Skip select()/poll() unless building a TUI.

## Open Questions / Decisions Pending

1. ~~**Buffer strategy for HTTP client**~~: ✅ Resolved — using `malloc`/`realloc` with dynamic doubling in `recv_all`, starting at 4096 bytes. No hard cap yet (may revisit for safety).
2. ~~**Error reporting in `http_client`**~~: ✅ Resolved — functions return -1 or NULL on error, callers use `perror`/`fprintf(stderr, ...)` with `strerror(errno)`. No central error enum yet.
3. **Header passing in `http_client`**: Not yet needed — `build_headers` hardcodes headers for now. Will need a `struct { char *name; char *value; }` array with count when headers become dynamic (Phase 2+).
4. ~~**JSON AST representation**~~: ✅ Resolved — tagged union (`JsonValue` with `JsonType` tag + `union { bool; double; char*; JsonArray*; JsonObject*; }`). `JsonArray` stores `JsonValue` structs **inline** in a dynamic array (not pointers), copied via `items[count] = *temp; free(temp)`. `JsonObject` uses dynamic array of `JsonPair` with `count`/`capacity`.
5. ~~**Parser memory management on failure**~~: ✅ Resolved — `parse_json()` now returns `JsonValue *`, checks `had_error` after parsing, and calls `free_json_value(value)` on failure before returning `NULL`. Callers own the returned pointer or get `NULL` on error.
6. **String token ownership**: `parser_current_str()` copies the raw token bytes (including `"` quotes) into a `calloc`'d buffer. For JSON semantics we may need to strip quotes and unescape `"`, `\\`, etc. TBD whether that happens at parse time or is left to consumers.
7. **`strtod` validation**: Currently calls `strtod(buf, NULL)` without checking `endptr`. This means malformed numbers accepted by the lexer (e.g., `"1.2.3"` can't happen because lexer stops at first invalid char, but `"-"` alone would tokenize as `TOKEN_NUMBER` with `-` if lexer changed) would silently produce partial results. Should use `endptr` to verify the full token was consumed.
8. **Compiler switch**: `Makefile` changed `CC = clang` to `CC = zig cc` in working tree. Need to decide whether to keep `zig cc` or revert to `clang`. `zig cc` provides built-in cross-compilation and caching, but may behave subtly differently.

## Session: 2026-06-19 — JSON Deserialization & String Token Bug

### What we worked on

- Added `deserialize_response()` to `src/anthropic.c` — walks a parsed `JsonValue` object and populates an `AnthropicResponse` struct by matching keys with `strcmp`.
- Wrote `test_deserialize_json` in `test/test_anthropic.c` with a real Anthropic API response (9 top-level fields, nested `content` array, nested `usage` object, `null` values).
- Discovered and diagnosed a **string token quoting bug** that prevents `deserialize_response` from matching any object key.

### Bug: `extract_token_str` includes quote delimiters in string keys and values

**Root cause**: The lexer's `string()` function produces `TOKEN_STRING` tokens whose span includes the opening and closing `"` characters. The parser's `extract_token_str()` copies the token's raw span verbatim, so parsed string keys like `"model"` retain their quotes — the key stored in the `JsonPair` is `"model"` (with quotes, 7 chars) rather than `model` (5 chars). Every `strcmp` in `deserialize_response` fails because it compares these quoted keys against unquoted C strings.

**Diagnosis pathway**: The test printed debug output (`DEBUG: processing key '"model"'`), confirming quotes were present. We verified by inspecting parsed keys with a standalone test program.

**Fix direction**: `extract_token_str` should strip the leading and trailing `"` for `TOKEN_STRING` tokens (adjust `start + 1`, `length - 2` before `memcpy`). This is the semantic boundary where "raw token bytes" become "string content." Alternatively, the lexer could exclude delimiters from the token span, but that changes what `make_token` captures and would break the existing lexer tests that assert `TOKEN_STRING` includes quotes (e.g., `test_lexer_minimal_json` expects `""key""`).

**Why existing tests didn't catch it**: All 21 parser tests check *structure* (type, count, capacity) but never inspect the *content* of parsed strings. No test asserts that `pairs[0].key` equals `"key"` or that `as.string` equals `"value"`. The lexer tests explicitly verify that string tokens include quotes, so the lexer is correct per its spec — the gap is in the parser's `extract_token_str` not stripping them.

**Action item**:  
1. Fix `extract_token_str` in `parser.c` to strip `"` delimiters for `TOKEN_STRING` (needs token type passed in or restructured).  
2. Update `test_lexer_minimal_json` and other lexer tests if the lexer's `string()` is changed instead.  
3. Add parser content tests: parse `{"key": "value"}` and assert `key == "key"` and `value == "value"`.  
4. Remove the `DEBUG` fprintf in `deserialize_response`.

### Other uncommitted changes in working tree

- `Makefile`: Added `-include .env` and `export` so `ANTHROPIC_API_KEY` is available to the binary at runtime.
- `include/anthropic.h`: Added `#include "json.h"`, `AnthropicTool` struct, `system`/`tools`/`tool_count` fields to `AnthropicRequest`, and `deserialize_response` declaration.
- `src/anthropic.c`: Implemented `deserialize_response`, rewrote `runInference` from commented-out `libcurl` stub to working `http_client`-based implementation. `ANTHROPIC_URL` changed from `"https://api.anthropic.com"` to `"api.anthropic.com"`.
- `test/test_anthropic.c`: Added `test_deserialize_json`, commented out old serialization tests in `main`.
- Compiler warnings: two `-Wmissing-field-initializers` warnings for `AnthropicRequest` initializers missing the new `system` field.

### New concern: `deserialize_response` does not handle `JSON_NULL`

The Anthropic API returns `"stop_sequence": null` and `"stop_details": null`. When `deserialize_response` encounters these keys, it reads `value->as.string` for a `JSON_NULL` value — undefined behavior since the union field for a null value is not `string`. Need to check `value->type` before accessing `value->as.string` or `value->as.object`/`value->as.array`, and handle `JSON_NULL` explicitly (set the corresponding field to `NULL` or skip it).

## Session: 2026-06-19 (continued) — Parser Fix, Test Hardening, and `content_count`

### What we fixed

- **`extract_token_str` in `parser.c`**: Fixed to strip `"` delimiters for `TOKEN_STRING` tokens by passing `token.type` and adjusting `start + 1`, `length - 2` before `memcpy`. Parsed keys and values now match expected C strings without quotes.
- **`test_deserialize_json`**: Hardened to assert every field: `id`, `type`, `role`, `model`, `stop_reason`, `content[0].type`, `content[0].text`, `usage.input_tokens`, `usage.output_tokens`, and `stop_sequence == NULL`. Changed `TEST_ASSERT_EQUAL_INT` to `TEST_ASSERT_EQUAL_size_t` for `size_t` fields. Changed `char *json` to `const char *json`.
- **Memory cleanup**: `test_deserialize_json` now frees `resp->content`, `resp`, and `json_parsed` (in that order, since `resp` borrows strings from `json_parsed`).
- **Designated initializers**: `test_serialize_request_body_single_message` and `test_serialize_request_body_multiple_messages` now use `.field = value` syntax for `AnthropicRequest`. This silences the two `-Wmissing-field-initializers` warnings and makes the struct layout self-documenting.
- **All 3 tests re-enabled**: `test_serialize_request_body_single_message`, `test_serialize_request_body_multiple_messages`, and `test_deserialize_json` all run and pass. `test_runner` output: `3 Tests 0 Failures 0 Ignored`.

### Added `content_count` to `AnthropicResponse`

- **`include/anthropic.h`**: Added `size_t content_count` to `AnthropicResponse` so callers know the length of the `content` array without walking it.
- **`src/anthropic.c`**: `deserialize_response` now sets `resp->content_count` to the number of items in the `content` array.
- **`test/test_anthropic.c`**: Asserts `TEST_ASSERT_EQUAL(1, resp->content_count)`.

### Added parser content test

- **`test/json/test_parser.c`**: Added `test_string_keys_and_values_have_no_quotes` using a real Anthropic API response JSON. Asserts that top-level keys, nested keys, and string values are bare (no surrounding quotes). Also asserts `JSON_NULL` for `stop_sequence` and `JSON_OBJECT` for `usage`. This test would have caught the quoting bug immediately.
- All 22 parser tests pass (0 failures).

### Remaining concerns and TODOs

- **`deserialize_response` JSON_NULL handling**: Still reads `value->as.string` for `stop_sequence` and `stop_details` without checking `value->type == JSON_STRING`. The test currently passes because `calloc` zeroed the union and `TEST_ASSERT_NULL` happens to match, but it's undefined behavior. **Fix**: Add `value->type == JSON_STRING` guards before accessing `as.string`; for `JSON_NULL`, set the field to `NULL`.
- **`deserialize_response` missing `stop_details`**: The test does not assert `resp->stop_details` because the struct field is missing. Consider adding `char *stop_details` to `AnthropicResponse`.
- **Error-path tests for `deserialize_response`**: No test for `NULL` input or a JSON object missing the `content` field. Consider adding:
  - `TEST_ASSERT_NULL(deserialize_response(NULL))`
  - A test with `{"id": "foo"}` (no `content` field) that asserts `resp->content == NULL` and `resp->content_count == 0`.
- **`deserialize_response` does not validate `value->type` before accessing `as.object`/`as.array`**: For `content`, `usage`, and nested objects, it assumes the value type matches the expected container type. If the API sends a scalar where an object is expected, this is UB. Add type checks for robustness.
- **`deserialize_response` `content_count` type**: The loop uses `int message_count` but `content_count` is `size_t`. Mixed signed/unsigned comparison. Consider making `message_count` `size_t` or `int` consistently.
- **Parser test coverage**: `test_string_keys_and_values_have_no_quotes` is the first parser test to inspect string content. Good precedent to follow for other tests. Consider adding simpler cases: `{"key": "value"}` and `{"nested": {"key": "value"}}`.
- **Test binary separation**: The `Makefile` currently only builds `test_runner` with `test/test_anthropic.c`. The parser tests in `test/json/test_parser.c` are not built by `make test`. Need to either add a second test target or merge the test files.

## Concerns from Current Session to Address Next

- ~~**`free_json_value()` heap-corruption bug**~~: ✅ **Fixed**. Split into `free_json_value_contents()` (static, frees contents without `free(value)`) and `free_json_value()` (contents + `free(value)`). Array loop calls contents-only helper on inline elements.
- **`json_pair()` error handling**: If `consume(TOKEN_STRING)` fails, `json_pair()` continues to `consume(TOKEN_COLON)` and `json_value()`, then returns a half-baked pair with `key == NULL`. Consider adding a `parser->had_error` guard before returning, or cleanup/free the pair and return `NULL`.
- **`json_object()` cleanup — per-pair allocation overhead**: Currently `calloc`s a `JsonPair`, copies into the array, then `free`s. Consider writing directly into `json_object->pairs[count]` to avoid intermediate allocation.
- **Parser tests in progress**: 12 tests (empty object/array, single primitives, complex object, array of primitives, truncated inputs, trailing comma, invalid element, non-string key, missing bracket/brace, array of empty objects). All pass.
- **`extract_token_str()` helper**: Consider unescaping JSON strings at parse time (strips surrounding `"` and handles `\"`, `\\\\`, etc.). Currently copies raw token bytes including quotes.
- **Public/private API split for `json.h`**: Eventually, `Scanner`, `Token`, `TokenType`, `Parser`, `init_scanner`, `scan_token`, and `init_parser` should move to an internal `json_internal.h` so the public `json.h` only exposes data model (`JsonValue`, `JsonArray`, `JsonObject`, `JsonType`) + `parse_json(const char *, FILE *)` + `free_json_value(JsonValue *)`. Deferred until parser is fully functional and tested.
- **`build_headers` truncation / buffer overflow risk in `http_client.c`**: `snprintf` returns the number of bytes it *would* have written, not what it actually wrote. In `build_headers`, the cursor advances by the return value unconditionally, and the remaining-length calculation (`buf_size - cursor`) is `size_t` so it underflows on truncation. Need to check that `snprintf` result fits before advancing `cursor`, and return `-1` on overflow. **Severity: correctness / potential OOB write.**
- **`send_all` / `recv_all` in `http_client.c` mishandle SSL errors**: `SSL_write`/`SSL_read` signal failure with `<= 0`, not just `-1`. The loops check `if (-1 == n)` which misses `0` and can spin forever on `SSL_write` returning `0`. Need to check `n <= 0` and treat as fatal. **Severity: correctness / infinite loop.**
- **`free_connection` skips file descriptor `0` in `http_client.c`**: `if (1 <= conn->sockfd)` means fd `0` is never closed. Valid fds start at `0`. Should use `conn->sockfd >= 0` or `-1 != conn->sockfd`. **Severity: minor resource leak (defensive).**
- **Missing TLS hostname verification in `http_client.c`**: `SSL_VERIFY_PEER` validates the certificate chain against the CA store, but does not check that the certificate was issued for the host being connected to. Need to add `SSL_set1_host(ssl, host)` before `SSL_connect` to prevent MITM with a valid cert for a different domain. **Severity: security.**
- **OpenSSL error reporting unused in `http_client.c`**: `<openssl/err.h>` is included but `ERR_print_errors_fp` is never called on SSL failures. Current error messages are opaque. Need to add `ERR_print_errors_fp(error_stream)` on `SSL_connect` or `SSL_write`/`SSL_read` failure. **Severity: debuggability.**
- **Internal linkage missing for `http_client.c` helpers**: `conn_write`, `conn_read`, `build_headers`, `send_all`, `recv_all`, `http_connect`, `ssl_connect`, `create_ssl_ctx`, etc. are non-static and have external linkage. Should mark them `static` to prevent symbol collisions and allow optimization. **Severity: design / hygiene.**
- **`http_method_to_str` returns non-const `char *` in `http_client.c`/`http_client.h`**: Returns a pointer to a string literal, inviting UB if caller writes through it. Should return `const char *`. **Severity: API safety.**
- **Binary body limitation in `http_client.c`**: `parse_response_body_to_http_response` uses `strstr`/`strdup`/`strlen` on the response buffer, which stops at the first embedded `\0`. Any binary body with a null byte will be silently truncated. `recv_all` already tracks the real length, so a future improvement is to carry that length through instead of re-deriving with `strlen`. **Severity: correctness / future limitation.**

## Session: 2026-06-21 — Chunked Transfer Encoding Decoding

### What we worked on

- **Type guards in `deserialize_response`**: Fixed `JSON_NULL` undefined behavior in `src/anthropic.c`. All string fields (`id`, `type`, `role`, `model`, `stop_reason`) now guard `value->type == JSON_STRING`. `stop_sequence` and `stop_details` correctly handle both `JSON_STRING` and `JSON_NULL`. `content` and `usage` fields now guard `JSON_ARRAY` and `JSON_OBJECT` respectively before accessing container members. Inner loop for `content` items also guards `JSON_STRING` for `type` and `text` fields.
- **Signed/unsigned mismatch**: Still needs fixing — `int content_count` on line 143 of `src/anthropic.c` should be `size_t`.
- **E2E test**: `runInference()` in `src/anthropic.c` now makes a real HTTPS request to `api.anthropic.com/v1/messages` and attempts to parse the response. However, it currently fails because the API returns `Transfer-Encoding: chunked`.

### The Chunked Response Problem

The Anthropic API returns `HTTP/1.1 200 OK` with `Transfer-Encoding: chunked` and `Connection: close`. The raw response body includes chunk length prefixes in hex:

```
204
{"model":"claude-haiku-4-5-20251001",...}
0
```

The current `parse_response_body_to_http_response()` in `src/http_client.c` splits at the first `\r\n\r\n` (end of headers) and `strdup`s everything after it as the body. This means the JSON parser receives a string starting with `204\r\n{...}` which fails immediately.

### What needs to happen next

Implement **chunked transfer encoding decoding** in the HTTP client. After receiving the raw response buffer:

1. Parse the header section to find `Transfer-Encoding: chunked` (and ideally `Content-Length` for non-chunked responses).
2. If chunked: walk the body section starting after `\r\n\r\n`, read each chunk's hex size, skip the `\r\n`, copy the chunk data into a new buffer, skip the trailing `\r\n`, repeat until chunk size is `0`, then skip the final `\r\n`.
3. If not chunked: use the body directly (or respect `Content-Length`).
4. Update `HTTPResponse` to store the decoded body and its length.

This is a correctness issue blocking the e2e smoke test. The current HTTP client works against `example.com` and `httpbin.org` because they return `Content-Length` and non-chunked bodies, but real-world APIs (Anthropic, Cloudflare) use chunked encoding.

### Design suggestion

Add a `parse_headers()` helper that returns a struct like:
```c
typedef struct {
    int status_code;
    bool is_chunked;
    size_t content_length;
    char *body_start; // pointer into the raw buffer
} HttpResponseInfo;
```

Then in `parse_response_body_to_http_response()`, call `parse_headers()`, and if `is_chunked`, decode the chunks before storing the body in `HTTPResponse`.

### Remaining items from earlier sessions

- Fix `usage` JSON_OBJECT guard in `deserialize_response` (already done in this session).
- Fix `content_count` signed/unsigned mismatch (`int` → `size_t`).
- `runInference` null dereference check (resp/v can be NULL).
- `runInference` memory leaks (http_resp, v, resp, resp->content never freed).
- Remove unused `#include <curl/curl.h>` from `src/anthropic.c`.
- Wire `test/json/test_parser.c` into the Makefile (22 parser tests not currently built).

## Environment Notes

- Compiler: `zig cc` (changed from `clang 22.1.3` in working tree)
- OpenSSL: Available (`pkg-config --exists openssl` passes).
- Test framework: Unity (already in `test/vendor/unity/`).
- API Key: Stored in `.env` file as `ANTHROPIC_API_KEY`. (Not committed to git.)

## Session: 2026-07-01 — Header Parsing & Chunked Decoding WIP

### What we worked on

- Added `free_http_response()` to `src/http_client.c` to properly free headers, body, and the response struct.
- Structured `parse_headers()` in `src/http_client.c` to populate `HTTPResponse->headers` from the raw response.
- Structured `parse_response_body_to_http_response()` to call `parse_headers()` first, detect `Transfer-Encoding: chunked`, and branch to `parse_chunked_body()` or `strdup()` for non-chunked bodies.
- Added `get_response_header()` to look up response headers case-insensitively.

### Bugs / issues identified in current `parse_headers()` (needs fixing next session)

1. **`realloc` missing `* sizeof(HttpHeader)`** — currently allocates `header_capacity` bytes instead of `header_capacity * sizeof(HttpHeader)`.
2. **`value_len` includes the colon** — `line_end - colon` should be `line_end - colon - 1`.
3. **Null terminator written to wrong buffer** — `h->key[value_len] = '\0'` should be `h->value[value_len] = '\0'`.
4. **Header values not trimmed** — leading/trailing whitespace after the colon should be stripped.
5. **`parse_headers()` allocates `headers` before checking `header_end`** — acceptable since `free_http_response()` cleans up, but the order could be cleaner.

### Remaining TODOs

- Fix `parse_headers()` bugs and implement whitespace trimming.
- Implement `parse_chunked_body()` to decode `Transfer-Encoding: chunked` responses.
- Add `free_http_response()` declaration to `include/http_client.h`.
- Remove the `[DEBUG]: raw response` print from `parse_response_body_to_http_response()`.
- Update `test/test_http_client.c` and `src/anthropic.c` to use `free_http_response()` instead of manual `free(resp->body); free(resp)`.
- Fix `send_all()` / `recv_all()` to handle SSL errors (`n <= 0` instead of `-1`).
- Fix `build_headers()` truncation/overflow risk where `snprintf` return value is added unconditionally.
- Wire `test/test_http_client.c` into the `Makefile` so the HTTP client tests are built.
- Remove unused `#include <curl/curl.h>` from `src/anthropic.c`.
- Fix `runInference()` null dereference checks and memory leaks.
- Wire `test/json/test_parser.c` into the `Makefile`.
- Fix `deserialize_response()` `content_count` signed/unsigned mismatch.
