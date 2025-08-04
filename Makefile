COMMAND = gcc -Iinclude -I/opt/homebrew/include -Wall -Wextra -std=c99
CJSON_PREFIX := $(shell brew --prefix cjson)
CJSON_CFLAGS := -I$(CJSON_PREFIX)/include
CJSON_LDFLAGS := -L$(CJSON_PREFIX)/lib -lcjson

TEST_COMMAND = $(COMMAND) -I. -Itests $(CJSON_CFLAGS) -DUNITY_USE_COLOR -DUNITY_OUTPUT_COLOR
LDFLAGS = -L$(CJSON_PREFIX)/lib -lcjson
SRC_FOLDER = src
SRC = $(wildcard $(SRC_FOLDER)/**/*.c)
# Exclude test_helper from the main library
CORE_SRC = $(filter-out $(SRC_FOLDER)/test_helper/%.c, $(SRC))
OBJ = $(SRC:.c=.o)
CORE_OBJ = $(CORE_SRC:.c=.o)
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

BENCHMARK_SRC_FOLDER = benchmarks
BENCHMARK_SRC = $(wildcard $(BENCHMARK_SRC_FOLDER)/*.c)
BENCHMARK_OBJ_FOLDER = benchmark_runner
BENCHMARK_OBJ = $(patsubst $(BENCHMARK_SRC_FOLDER)/%.c,$(BENCHMARK_OBJ_FOLDER)/%,$(BENCHMARK_SRC))

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
		echo "\n👨‍💻Run test $$file:"; \
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

benchmark: $(BENCHMARK_OBJ)
	@echo "\nRunning benchmarks:"
	@for file in $(BENCHMARK_OBJ); do \
		echo "\nRun benchmark $$file:"; \
		$$file; \
		echo "Benchmark done."; \
	done

.PHONY: doc

doc:
	@echo "Generating Doxygen documentation..."
	@doxygen Doxyfile
	@echo "Documentation generated in docs/html"

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(CORE) $(UNITY_OBJ) $(BENCHMARK_OBJ)
	rm -rf $(TEST_OBJ_FOLDER) $(BENCHMARK_OBJ_FOLDER) docs

format:
	@find . \( -name '*.c' -o -name '*.h' \) -exec clang-format -i {} +

$(CORE): $(CORE_OBJ)
	ar rcs $@ $^

$(SRC_FOLDER)/test_helper/%.o: $(SRC_FOLDER)/test_helper/%.c
	$(TEST_COMMAND) -c -o $@ $<

$(SRC_FOLDER)/**/%.o: $(SRC_FOLDER)/**/%.c
	$(COMMAND) -c -o $@ $<

$(TEST_OBJ_FOLDER):
	mkdir -p $(TEST_OBJ_FOLDER)

$(BENCHMARK_OBJ_FOLDER):
	mkdir -p $(BENCHMARK_OBJ_FOLDER)

$(UNITY_OBJ): $(UNITY_SRC) $(TEST_OBJ_FOLDER)
	$(TEST_COMMAND) -I$(TEST_SRC_FOLDER)/unity -c -o $@ $<

$(TEST_OBJ_FOLDER)/%: $(TEST_SRC_FOLDER)/%.c $(CORE) $(UNITY_OBJ) $(TEST_OBJ_FOLDER) src/test_helper/test_helper.o
	$(TEST_COMMAND) -I$(TEST_SRC_FOLDER)/unity -o $@ $< $(UNITY_OBJ) $(CORE) src/test_helper/test_helper.o $(LDFLAGS)

$(BENCHMARK_OBJ_FOLDER)/%: $(BENCHMARK_SRC_FOLDER)/%.c $(CORE) $(UNITY_OBJ) $(BENCHMARK_OBJ_FOLDER) src/test_helper/test_helper.o
	$(TEST_COMMAND) -I$(TEST_SRC_FOLDER)/unity -o $@ $< $(UNITY_OBJ) $(CORE) src/test_helper/test_helper.o $(LDFLAGS)
