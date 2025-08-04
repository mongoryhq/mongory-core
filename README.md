# Mongory Core

`mongory-core` is a lightweight, dependency-free C library for querying C data structures using a query syntax inspired by MongoDB. It is designed to be extensible, allowing you to integrate it with your own custom data types and matching logic.

## ✨ Features

- **MongoDB-like Queries**: Use familiar operators like `$eq`, `$gt`, `$lt`, `$in`, `$and`, `$or`, etc., to build complex queries.
- **Memory Pool**: All memory is managed through a simple memory pool, making resource management straightforward and helping to prevent memory leaks.
- **Generic Data Types**: Data is represented using a generic `mongory_value` type, which can encapsulate integers, doubles, strings, arrays, and tables (hash maps).
- **Extensible**: Provides interfaces to define your own custom value converters and regex engines, allowing `mongory-core` to work seamlessly with your existing data structures.
- **Well-Documented**: The public API is thoroughly documented with Doxygen-style comments in the header files.

## 🚀 Quick Start

Here is a simple example of how to use `mongory-core` to match a document against a query.

First, let's define a C data structure we want to query. Imagine we have a "document" representing a user:

```c
// Document to be queried
// {
//   "name": "Jules",
//   "language": "C",
//   "year": 2024
// }
```

And we want to see if it matches the following query:

```json
// Query
// {
//   "language": "C",
//   "year": { "$gt": 2020 }
// }
```

Here's the C code to perform this match using `mongory-core`:

```c
#include <stdio.h>
#include <mongory-core.h>

int main() {
    // 1. Initialize the Mongory library
    mongory_init();

    // 2. Create a memory pool for all allocations
    mongory_memory_pool *pool = mongory_memory_pool_new();

    // 3. Create the document to be queried as a mongory_table
    mongory_table *doc_table = mongory_table_new(pool);
    doc_table->set(doc_table, "name", mongory_value_wrap_s(pool, "Jules"));
    doc_table->set(doc_table, "language", mongory_value_wrap_s(pool, "C"));
    doc_table->set(doc_table, "year", mongory_value_wrap_i(pool, 2024));
    mongory_value *doc = mongory_value_wrap_t(pool, doc_table);

    // 4. Create the query condition
    // { "language": "C", "year": { "$gt": 2020 } }
    mongory_table *query_table = mongory_table_new(pool);
    mongory_table *year_cond_table = mongory_table_new(pool);

    // "$gt": 2020
    year_cond_table->set(year_cond_table, "$gt", mongory_value_wrap_i(pool, 2020));

    // "year": { ... }
    query_table->set(query_table, "year", mongory_value_wrap_t(pool, year_cond_table));
    // "language": "C"
    query_table->set(query_table, "language", mongory_value_wrap_s(pool, "C"));

    mongory_value *query = mongory_value_wrap_t(pool, query_table);

    // 5. Create a matcher with the query condition
    mongory_matcher *matcher = mongory_matcher_new(pool, query);

    // 6. Perform the match
    bool is_match = mongory_matcher_match(matcher, doc);

    // 7. Print the result
    if (is_match) {
        printf("The document matches the query!\\n");
    } else {
        printf("The document does not match the query.\\n");
    }

    // 8. Clean up
    pool->free(pool);
    mongory_cleanup();

    return 0;
}
```
*(Note: For a runnable version of this example, see `benchmarks/basic_test.c`)*

## 🛠️ Building and Testing

This project uses `make` for building and testing.

### Dependencies
- `gcc` (or any C99-compatible compiler)
- `make`
- `cJSON` (for running tests)

On macOS with Homebrew, you can install `cJSON` with:
```bash
brew install cjson
```

### Build the Library
To build the static library `mongory-core.a`:
```bash
make
```

### Run Tests
This project uses the Unity test framework. The `make test` command will automatically download Unity and run all tests.
```bash
make test
```

### Code Formatting
The project uses `clang-format` for code formatting. To format all `.c` and `.h` files:
```bash
make format
```

## 📚 API Documentation

The public API is documented in the header files in the `include/` directory. The comments are compatible with Doxygen. You can generate HTML documentation if you have Doxygen installed.
