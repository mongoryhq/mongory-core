COMMAND = gcc -Iinclude -Wall -Wextra -std=c99
SRC_FOLDER = src
SRC = $(wildcard $(SRC_FOLDER)/**/*.c)
OBJ = $(SRC:.c=.o)
CORE = mongory-core.a

TEST_SRC_FOLDER = tests
TEST_SRC = $(wildcard $(TEST_SRC_FOLDER)/*.c)
TEST_OBJ_FOLDER = test_runner
TEST_OBJ = $(patsubst $(TEST_SRC_FOLDER)/%.c,$(TEST_OBJ_FOLDER)/%,$(TEST_SRC))

all: $(CORE)

test: $(TEST_OBJ)
	@for file in $(TEST_OBJ); do \
		echo "\nRun test $$file:"; \
		$$file; \
		echo "Test done."; \
	done

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(CORE)
	rm -rf $(TEST_OBJ_FOLDER)

$(CORE): $(OBJ)
	ar rcs $@ $^

$(SRC_FOLDER)/**/%.o: $(SRC_FOLDER)/**/%.c
	$(COMMAND) -c -o $@ $<

$(TEST_OBJ_FOLDER):
	mkdir -p $(TEST_OBJ_FOLDER)

$(TEST_OBJ_FOLDER)/%: $(TEST_SRC_FOLDER)/%.c $(CORE) $(TEST_OBJ_FOLDER)
	$(COMMAND) -o $@ $< $(CORE)
