-include .env
export

CC = zig cc
OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl)
OPENSSL_LDFLAGS := $(shell pkg-config --libs openssl)
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -Itest/vendor/unity $(OPENSSL_CFLAGS)
LDFLAGS = $(OPENSSL_LDFLAGS)
OUT = cage
TEST_OUT = test_runner
TEST_HTTP_CLIENT_OUT = test_http_client

SRC_FILES = src/agent.c \
			src/anthropic.c \
			src/http_client.c \
			src/json/lexer.c \
			src/json/parser.c

SRC = main.c $(SRC_FILES)

# Unity test framework
UNITY_SRC = test/vendor/unity/unity.c

# Common source files used by all test binaries
TEST_COMMON_SRC = $(SRC_FILES) $(UNITY_SRC)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

$(TEST_OUT): $(TEST_COMMON_SRC) test/test_anthropic.c
	$(CC) $(CFLAGS) -Isrc $^ -o $@ $(LDFLAGS)

$(TEST_HTTP_CLIENT_OUT): $(TEST_COMMON_SRC) test/test_http_client.c
	$(CC) $(CFLAGS) -Isrc $^ -o $@ $(LDFLAGS)

PHONY: test clean
test: $(TEST_OUT) $(TEST_HTTP_CLIENT_OUT)
	-./$(TEST_OUT)
	-./$(TEST_HTTP_CLIENT_OUT)

clean:
	rm -f $(OUT) $(TEST_OUT) $(TEST_HTTP_CLIENT_OUT)
