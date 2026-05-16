CC = cc
CFLAGS = -std=c99 -Wall -Wextra -pedantic

SRC = src/implementation/main.c \
      src/implementation/lexer.c \
      src/implementation/util.c

forgec: $(SRC)
	$(CC) $(CFLAGS) -o bin/forgec $(SRC)

clean:
	rm -f bin/forgec
