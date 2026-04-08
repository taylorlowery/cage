CC = clang
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -Itest
OUT = cage
SRC = main.c src/agent.c

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

.PHONY: test clean
test:
	echo "TODO: get the tests working"

clean:
	rm -f $(OUT)
