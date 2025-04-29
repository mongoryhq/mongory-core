CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c99
SRC = src/mongory.c
OBJ = $(SRC:.c=.o)
TEST = test/test_mongory.c

all: mongory.a test

mongory.a: $(OBJ)
	ar rcs $@ $^

test: mongory.a $(TEST)
	$(CC) $(CFLAGS) -o test_mongory $(TEST) mongory.a

clean:
	rm -f src/*.o mongory.a test_mongory
