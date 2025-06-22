COMMAND = gcc -Iinclude -I/opt/homebrew/include -Wall -Wextra -std=c99
CJSON_PREFIX := $(shell brew --prefix cjson)
CJSON_CFLAGS := -I$(CJSON_PREFIX)/include
CJSON_LDFLAGS := -L$(CJSON_PREFIX)/lib -lcjson

TEST_COMMAND = $(COMMAND) -I. -Itests -I$(CJSON_CFLAGS) -DUNITY_USE_COLOR -DUNITY_OUTPUT_COLOR
LDFLAGS = -L$(CJSON_PREFIX)/lib -lcjson
SRC_FOLDER = src
SRC = $(wildcard $(SRC_FOLDER)/**/*.c)
OBJ = $(SRC:.c=.o)
CORE = mongory-core.a

# color code
RED = \033[0;31m
GREEN = \033[0;32m
NC = \033[0m # No Color

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
	@failed_tests=""; \
	for file in $(TEST_OBJ); do \
		echo "\nRun test $$file:"; \
		if $$file; then \
			echo "$(GREEN)Test passed.$(NC)"; \
		else \
			echo "$(RED)Test failed with exit code $$?$(NC)"; \
			failed_tests="$$failed_tests\n$$file"; \
		fi; \
		echo "Test done."; \
	done; \
	if [ ! -z "$$failed_tests" ]; then \
		echo "\n$(RED)Failed tests:"; \
		echo "$(RED)$$failed_tests"; \
	fi

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(CORE) $(UNITY_OBJ)
	rm -rf $(TEST_OBJ_FOLDER)

format:
	@find . \( -name '*.c' -o -name '*.h' \) -exec clang-format -i {} +

$(CORE): $(OBJ)
	ar rcs $@ $^

$(SRC_FOLDER)/**/%.o: $(SRC_FOLDER)/**/%.c
	$(COMMAND) -c -o $@ $<

$(TEST_OBJ_FOLDER):
	mkdir -p $(TEST_OBJ_FOLDER)

$(UNITY_OBJ): $(UNITY_SRC) $(TEST_OBJ_FOLDER)
	$(TEST_COMMAND) -I$(TEST_SRC_FOLDER)/unity -c -o $@ $<

$(TEST_OBJ_FOLDER)/%: $(TEST_SRC_FOLDER)/%.c $(CORE) $(UNITY_OBJ) $(TEST_OBJ_FOLDER)
	$(TEST_COMMAND) -I$(TEST_SRC_FOLDER)/unity -o $@ $< $(UNITY_OBJ) $(CORE) $(LDFLAGS)

