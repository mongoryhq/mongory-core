COMMAND = gcc -Iinclude -Wall -Wextra -std=c99
SRC_FOLDER = src
SRC = $(wildcard $(SRC_FOLDER)/**/*.c)
OBJ = $(SRC:.c=.o)
CORE = mongory-core.a

TEST_SRC_FOLDER = tests
TEST_SRC = $(wildcard $(TEST_SRC_FOLDER)/*.c)
TEST_OBJ_FOLDER = test_runner
TEST_OBJ = $(patsubst $(TEST_SRC_FOLDER)/%.c,$(TEST_OBJ_FOLDER)/%,$(TEST_SRC))
UNITY_SRC = $(TEST_SRC_FOLDER)/unity/unity.c
UNITY_OBJ = $(TEST_OBJ_FOLDER)/unity.o

all: $(CORE)

setup-unity:
	@if [ ! -f $(UNITY_SRC) ]; then \
		echo "Setting up Unity test framework..."; \
		chmod +x scripts/setup_unity.sh; \
		./scripts/setup_unity.sh; \
	fi

test: setup-unity $(TEST_OBJ)
	@for file in $(TEST_OBJ); do \
		echo "\nRun test $$file:"; \
		$$file; \
		echo "Test done."; \
	done

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(CORE) $(UNITY_OBJ)
	rm -rf $(TEST_OBJ_FOLDER)

$(CORE): $(OBJ)
	ar rcs $@ $^

$(SRC_FOLDER)/**/%.o: $(SRC_FOLDER)/**/%.c
	$(COMMAND) -c -o $@ $<

$(TEST_OBJ_FOLDER):
	mkdir -p $(TEST_OBJ_FOLDER)

$(UNITY_OBJ): $(UNITY_SRC) $(TEST_OBJ_FOLDER)
	$(COMMAND) -I$(TEST_SRC_FOLDER)/unity -c -o $@ $<

$(TEST_OBJ_FOLDER)/%: $(TEST_SRC_FOLDER)/%.c $(CORE) $(UNITY_OBJ) $(TEST_OBJ_FOLDER)
	$(COMMAND) -I$(TEST_SRC_FOLDER)/unity -o $@ $< $(UNITY_OBJ) $(CORE)
