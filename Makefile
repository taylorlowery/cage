-include .env
export

CC = zig cc
OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl)
OPENSSL_LDFLAGS := $(shell pkg-config --libs openssl)
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -Itest/vendor/unity $(OPENSSL_CFLAGS)
LDFLAGS = $(OPENSSL_LDFLAGS)
OUT = cage
TEST_OUT = test_runner

SRC_FILES = src/agent.c \
			src/anthropic.c \
			src/http_client.c \
			src/json/lexer.c \
			src/json/parser.c

SRC = main.c $(SRC_FILES)

# TODO: support multiple simultaneous test binaries.
# Because each Unity test module expects a setUp(), tearDown(), and main(),
# each needs its own binary, which Unity will run simultaneously.
# TEST_FILES = $(wildcard test/test_*.c)
TEST_SRC = $(SRC_FILES) test/test_anthropic.c test/vendor/unity/unity.c

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

$(TEST_OUT): $(TEST_SRC)
	$(CC) $(CFLAGS) -Isrc $(TEST_SRC) -o $(TEST_OUT) $(LDFLAGS)

PHONY: test clean
test: $(TEST_OUT)
	./$(TEST_OUT)

clean:
	rm -f $(OUT) $(TEST_OUT)
