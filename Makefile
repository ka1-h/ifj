CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g

SRC = $(wildcard src/*.c)

TARGET = ifj

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

lex_test: $(TARGET)
	cd test_skripts && ./lex_test.sh

test: lex_test

clean:
	rm -f $(TARGET)

.PHONY: all clean
