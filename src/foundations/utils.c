#ifndef MONGORY_UTILS_C
#define MONGORY_UTILS_C

#include "utils.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/**
 * @brief Attempts to parse a string `key` into an integer `out`.
 *
 * Uses `strtol` for conversion and checks for common parsing errors:
 * - Input string is NULL or empty.
 * - The string contains non-numeric characters after the number.
 * - The parsed number is out of the range of `int` (`INT_MIN`, `INT_MAX`).
 *
 * @param key The null-terminated string to parse.
 * @param out Pointer to an integer where the result is stored.
 * @return `true` if parsing is successful and the value fits in an `int`.
 *         `false` otherwise. `errno` may be set by `strtol`.
 */
bool mongory_try_parse_int(const char *key, int *out) {
  if (key == NULL) {
    return false; // Invalid input string.
  }

  // Strictly match "0" or "-?[1-9][0-9]*" (no leading whitespace, '+', "-0", or leading zeros like "01").
  if (key[0] == '0') {
    if (key[1] != '\0') {
      return false; // Disallow leading zeros like "00" or "01".
    }
    if (out != NULL) {
      *out = 0;
    }
    return true;
  }

  const char *p = (key[0] == '-') ? key + 1 : key;
  if (*p < '1') {
    return false; // Disallow "", "-", "-0", "-01", leading whitespace, or '+'.
  }

  char *endptr = NULL;
  errno = 0;                           // Clear errno before calling strtol.
  long val = strtol(key, &endptr, 10); // Base 10 conversion.

  if (*endptr != '\0' || errno == ERANGE || val < INT_MIN || val > INT_MAX) {
    return false; // Additional characters or value out of range for int.
  }

  if (out != NULL) {
    *out = (int)val; // Successfully parsed and within int range.
  }
  return true;
}

/**
 * @brief Creates a copy of a string using the specified memory pool.
 * @param pool The memory pool to use for allocation.
 * @param str The null-terminated string to copy.
 * @return A pointer to the newly allocated copy, or NULL if str is NULL or
 * allocation fails.
 */
char *mongory_string_cpy(mongory_memory_pool *pool, char *str) {
  if (str == NULL) {
    return NULL;
  }

  size_t len = strlen(str);
  char *new_str = (char *)MG_ALLOC(pool, len + 1); // +1 for null terminator.
  if (new_str == NULL) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }

  strcpy(new_str, str);
  return new_str;
}

char *mongory_string_cpyf(mongory_memory_pool *pool, char *format, ...) {
  if (!pool || !format) {
    return NULL;
  }
  va_list args;
  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (len < 0) {
    return NULL;
  }
  char *new_str = (char *)MG_ALLOC(pool, (size_t)len + 1);
  if (new_str == NULL) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  va_start(args, format);
  vsnprintf(new_str, (size_t)len + 1, format, args);
  va_end(args);
  new_str[len] = '\0';
  return new_str;
}

double mongory_log(double x, double base) {
  return log(x) / log(base);
}

void mongory_error_set(mongory_memory_pool *pool, mongory_error_type type, const char *message) {
  if (!pool || pool->error != NULL) {
    return;
  }
  mongory_error *error = MG_ALLOC_PTR(pool, mongory_error);
  if (error == NULL) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return;
  }
  error->type = type;
  error->message = message;
  pool->error = error;
}

bool mongory_validate_ptr(mongory_memory_pool *pool, char *name, void *ptr, char *file, int line) {
  if (!pool || pool->error != NULL) {
    return false;
  }
  if (ptr == NULL) {
    char *msg = mongory_string_cpyf(pool, "Null pointer: %s (at %s:%d)", name, file, line);
    if (pool->error == NULL) {
      mongory_error_set(pool, MONGORY_ERROR_INVALID_ARGUMENT, msg ? msg : "Null pointer");
    }
    return false;
  }
  return true;
}

static char *error_format =
  "[Mongory Core Error]\n"
  "%s needs %s, got %s\n"
  "(%s:%d)\n";

static bool mongory_validate_type_error(mongory_memory_pool *pool, char *name, const char *expected,
                                        mongory_value *value, char *file, int line) {
  char *msg = mongory_string_cpyf(pool, error_format, name, expected, mongory_type_to_string(value), file, line);
  if (pool->error == NULL) {
    mongory_error_set(pool, MONGORY_ERROR_INVALID_ARGUMENT, msg ? msg : "Invalid argument type");
  }
  return false;
}

bool mongory_validate_table(mongory_memory_pool *pool, char *name, mongory_value *value, char *file, int line) {
  if (!pool || pool->error != NULL) {
    return false;
  }
  if (!mongory_validate_ptr(pool, name, value, file, line)) {
    return false;
  }
  if (value->type != MONGORY_TYPE_TABLE) {
    return mongory_validate_type_error(pool, name, "Table", value, file, line);
  }
  return true;
}

bool mongory_validate_array(mongory_memory_pool *pool, char *name, mongory_value *value, char *file, int line) {
  if (!pool || pool->error != NULL) {
    return false;
  }
  if (!mongory_validate_ptr(pool, name, value, file, line)) {
    return false;
  }
  if (value->type != MONGORY_TYPE_ARRAY) {
    return mongory_validate_type_error(pool, name, "Array", value, file, line);
  }
  return true;
}

bool mongory_validate_string(mongory_memory_pool *pool, char *name, mongory_value *value, char *file, int line) {
  if (!pool || pool->error != NULL) {
    return false;
  }
  if (!mongory_validate_ptr(pool, name, value, file, line)) {
    return false;
  }
  if (value->type != MONGORY_TYPE_STRING) {
    return mongory_validate_type_error(pool, name, "String", value, file, line);
  }
  return true;
}

bool mongory_validate_number(mongory_memory_pool *pool, char *name, mongory_value *value, char *file, int line) {
  if (!pool || pool->error != NULL) {
    return false;
  }
  if (!mongory_validate_ptr(pool, name, value, file, line)) {
    return false;
  }
  if (value->type != MONGORY_TYPE_INT && value->type != MONGORY_TYPE_DOUBLE) {
    return mongory_validate_type_error(pool, name, "Number", value, file, line);
  }
  return true;
}

#endif // MONGORY_UTILS_C
