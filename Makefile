CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c99
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

TEST_SRC = $(wildcard tests/*.c)
TEST_OBJ = $(patsubst tests/%.c,test_runner/%,$(TEST_SRC))

all: mongory.a

mongory.a: $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TEST_OBJ)
	@for file in $(TEST_OBJ); do \
		echo "\nRun test $$file:"; \
		$$file; \
		echo "Test done."; \
	done

test_runner:
	mkdir -p test_runner

test_runner/%: tests/%.c mongory.a test_runner
	$(CC) $(CFLAGS) -o $@ $< mongory.a

clean:
	rm -f $(OBJ) $(TEST_OBJ) mongory.a
	rm -rf test_runner
