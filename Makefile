CC = clang
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -Itest/vendor/unity
OUT = cage
TEST_OUT = test_runner


SRC = main.c src/agent.c src/http_client.c
TEST_FILES = $(wildcard test/test_*.c)
TEST_SRC = src/agent.c src/http_client.c test/test_http_client.c test/vendor/unity/unity.c
# TEST_SRC = src/agent.c src/http_client.c $(TEST_FILES) test/vendor/unity/unity.c

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

$(TEST_OUT): $(TEST_SRC)
	$(CC) $(CFLAGS) -Isrc $(TEST_SRC) -o $(TEST_OUT)

.PHONY: test clean
test: $(TEST_OUT)
	./$(TEST_OUT)

clean:
	rm -f $(OUT) $(TEST_OUT)
