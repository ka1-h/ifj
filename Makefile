CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g

OBJS = main.o scanner.o symtable.o

TARGET = ifj

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c scanner.h
	$(CC) $(CFLAGS) -c main.c

scanner.o: scanner.c scanner.h
	$(CC) $(CFLAGS) -c scanner.c

symtable.o: symtable.c symtable.h scanner.h
	$(CC) $(CFLAGS) -c symtable.c

lex_test: $(TARGET)
	cd test_skripts && ./lex_test.sh

test: lex_test

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean lex_test test