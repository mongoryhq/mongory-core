# Thin convenience wrapper delegating to CMake / build.sh (single source of truth).

.PHONY: all build test benchmark setup-unity setup-yyjson format doc clean

all: build

build:
	@./build.sh

test:
	@./build.sh --test

benchmark:
	@./build.sh --benchmark

setup-unity:
	@./scripts/setup_unity.sh

setup-yyjson:
	@./scripts/setup_yyjson.sh

format:
	@cmake --build build --target format

doc:
	@doxygen Doxyfile

clean:
	@rm -rf build test_runner benchmark_runner mongory-core.a
