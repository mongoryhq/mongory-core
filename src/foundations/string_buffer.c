#include "string_buffer.h"
#include "utils.h"
#include <mongory-core.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MONGORY_STRING_BUFFER_INITIAL_CAPACITY 16

mongory_string_buffer *mongory_string_buffer_new(mongory_memory_pool *pool) {
  if (!MONGORY_VALIDATE_POOL(pool)) {
    return NULL;
  }
  mongory_string_buffer *buffer = MG_ALLOC_PTR(pool, mongory_string_buffer);
  if (!buffer) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  char *data = (char *)MG_ALLOC(pool, MONGORY_STRING_BUFFER_INITIAL_CAPACITY);
  if (!data) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  buffer->pool = pool;
  buffer->buffer = data;
  buffer->size = 0;
  buffer->capacity = MONGORY_STRING_BUFFER_INITIAL_CAPACITY;
  buffer->buffer[0] = '\0';
  return buffer;
}

static inline bool mongory_string_buffer_grow(mongory_string_buffer *buffer, size_t additional_size) {
  if (!buffer || !MONGORY_VALIDATE_POOL(buffer->pool)) {
    return false;
  }
  if (additional_size > SIZE_MAX - 1 || buffer->size > SIZE_MAX - 1 - additional_size) {
    buffer->pool->error = &MONGORY_ALLOC_ERROR;
    return false;
  }
  size_t required = buffer->size + additional_size + 1;
  if (required <= buffer->capacity) {
    return true;
  }
  size_t new_capacity = buffer->capacity > 0 ? buffer->capacity : MONGORY_STRING_BUFFER_INITIAL_CAPACITY;
  while (new_capacity < required) {
    if (new_capacity > SIZE_MAX / 2) {
      new_capacity = required;
      break;
    }
    new_capacity *= 2;
  }
  char *new_buffer = (char *)MG_ALLOC(buffer->pool, new_capacity);
  if (!new_buffer) {
    buffer->pool->error = &MONGORY_ALLOC_ERROR;
    return false;
  }
  if (buffer->buffer) {
    memcpy(new_buffer, buffer->buffer, buffer->size + 1);
  } else {
    new_buffer[0] = '\0';
  }
  buffer->buffer = new_buffer;
  buffer->capacity = new_capacity;
  return true;
}

void mongory_string_buffer_append(mongory_string_buffer *buffer, const char *str) {
  if (!buffer || !str) {
    return;
  }
  size_t len = strlen(str);
  if (!mongory_string_buffer_grow(buffer, len)) {
    return;
  }
  memcpy(buffer->buffer + buffer->size, str, len + 1);
  buffer->size += len;
}

void mongory_string_buffer_appendf(mongory_string_buffer *buffer, const char *format, ...) {
  if (!buffer || !format) {
    return;
  }
  va_list args;
  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (len < 0) {
    return;
  }
  if (!mongory_string_buffer_grow(buffer, (size_t)len)) {
    return;
  }
  va_start(args, format);
  vsnprintf(buffer->buffer + buffer->size, buffer->capacity - buffer->size, format, args);
  va_end(args);
  buffer->size += (size_t)len;
  buffer->buffer[buffer->size] = '\0';
}

char *mongory_string_buffer_cstr(mongory_string_buffer *buffer) {
  return buffer ? buffer->buffer : NULL;
}

void mongory_string_buffer_clear(mongory_string_buffer *buffer) {
  if (!buffer || !MONGORY_VALIDATE_POOL(buffer->pool)) {
    return;
  }
  char *new_buffer = (char *)MG_ALLOC(buffer->pool, MONGORY_STRING_BUFFER_INITIAL_CAPACITY);
  if (!new_buffer) {
    buffer->pool->error = &MONGORY_ALLOC_ERROR;
    return;
  }
  buffer->buffer = new_buffer;
  buffer->capacity = MONGORY_STRING_BUFFER_INITIAL_CAPACITY;
  buffer->size = 0;
  buffer->buffer[0] = '\0';
}
