-include .env
export

CC = zig cc
OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl)
OPENSSL_LDFLAGS := $(shell pkg-config --libs openssl)
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -Itest/vendor/unity $(OPENSSL_CFLAGS)
LDFLAGS = $(OPENSSL_LDFLAGS)
OUT = cage

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

# binaries for each test file
TEST_BINS = test_anthropic \
			test_http_client \
			test_parser \
			test_lexer \
			test_agent

$(TEST_BINS): test_%: test/test_%.c $(TEST_COMMON_SRC)
	$(CC) $(CFLAGS) -Isrc $^ -o $@ $(LDFLAGS)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

.PHONY: build_tests test clean

# Build all test binaries
build_tests: $(TEST_BINS)

# run all tests
test: build_tests
	@for t in $(TEST_BINS); do ./$$t || exit 1; done

clean:
	rm -f $(OUT) $(TEST_BINS)
