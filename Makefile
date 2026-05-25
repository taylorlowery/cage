CC = clang
OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl)
OPENSSL_LDFLAGS := $(shell pkg-config --libs openssl)
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -Itest/vendor/unity $(OPENSSL_CFLAGS)
LDFLAGS = $(OPENSSL_LDFLAGS)
OUT = cage
TEST_OUT = test_runner


SRC = main.c src/agent.c src/http_client.c src/json.c

# TODO: support multiple simultaneous test binaries.
# Because each Unity test module expects a setUp(), tearDown(), and main(),
# each needs its own binary, which Unity will run simultaneously.
# For now, we'll just work on supporting the JSON parser while we develop that.
# TEST_FILES = $(wildcard test/test_*.c)
# TEST_SRC = src/agent.c src/http_client.c $(TEST_FILES) test/vendor/unity/unity.c
TEST_SRC = src/agent.c src/http_client.c src/json.c test/test_json.c test/vendor/unity/unity.c

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

$(TEST_OUT): $(TEST_SRC)
	$(CC) $(CFLAGS) -Isrc $(TEST_SRC) -o $(TEST_OUT) $(LDFLAGS)

PHONY: test clean
test: $(TEST_OUT)
	./$(TEST_OUT)

clean:
	rm -f $(OUT) $(TEST_OUT)
