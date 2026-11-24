CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g

SRC = $(wildcard src/*.c)
HDR = $(wildcard src/*.h)

TARGET = ifj

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

lex_test: $(TARGET)
	cd test_skripts && ./lex_test.sh

test: lex_test

format:
	clang-format -i $(SRC) $(HDR)

clean:
	rm -f $(TARGET)

.PHONY: all clean format
