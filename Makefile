CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c99
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
CORE = mongory-core.a

TEST_SRC_FOLDER = tests
TEST_SRC = $(wildcard $(TEST_SRC_FOLDER)/*.c)
TEST_OBJ_FOLDER = test_runner
TEST_OBJ = $(patsubst $(TEST_SRC_FOLDER)/%.c,$(TEST_OBJ_FOLDER)/%,$(TEST_SRC))

all: $(CORE)

$(CORE): $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TEST_OBJ)
	@for file in $(TEST_OBJ); do \
		echo "\nRun test $$file:"; \
		$$file; \
		echo "Test done."; \
	done

$(TEST_OBJ_FOLDER):
	mkdir -p $(TEST_OBJ_FOLDER)

$(TEST_OBJ_FOLDER)/%: $(TEST_SRC_FOLDER)/%.c $(CORE) $(TEST_OBJ_FOLDER)
	$(CC) $(CFLAGS) -o $@ $< $(CORE)

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(CORE)
	rm -rf $(TEST_OBJ_FOLDER)
