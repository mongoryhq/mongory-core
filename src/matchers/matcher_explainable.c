#ifndef MONGORY_MATCHER_EXPLAINABLE_C
#define MONGORY_MATCHER_EXPLAINABLE_C

#include "matcher_explainable.h"
#include "../foundations/string_buffer.h"
#include "base_matcher.h"
#include "composite_matcher.h"
#include "literal_matcher.h"
#include <stdio.h>

char *mongory_matcher_title(mongory_matcher *matcher, mongory_memory_pool *pool) {
  mongory_string_buffer *buffer = mongory_string_buffer_new(pool);
  mongory_string_buffer_append(buffer, matcher->name);
  mongory_string_buffer_append(buffer, ": ");
  mongory_value *condition = matcher->condition;
  mongory_string_buffer_append(buffer, condition->to_str(condition, pool));
  return mongory_string_buffer_cstr(buffer);
}

char *mongory_matcher_title_with_field(mongory_matcher *matcher, mongory_memory_pool *pool) {
  mongory_string_buffer *buffer = mongory_string_buffer_new(pool);
  mongory_field_matcher *field_matcher = (mongory_field_matcher *)matcher;
  mongory_string_buffer_appendf(buffer, "Field: \"%s\", to match: ", field_matcher->field);
  mongory_value *condition = field_matcher->composite.base.condition;
  mongory_string_buffer_append(buffer, condition->to_str(condition, pool));
  return mongory_string_buffer_cstr(buffer);
}

static inline char *mongory_matcher_tail_connection(int count, int total) {
  if (count == (total - 1)) {
    return "└─ ";
  } else if (total == 0) {
    return "";
  }
  return "├─ ";
}

static inline char *mongory_matcher_indent_connection(int count, int total) {
  if (count < total) {
    return "│  ";
  } else if (total == 0) {
    return "";
  }
  return "   ";
}

void mongory_matcher_base_explain(mongory_matcher *matcher, mongory_matcher_explain_context *ctx) {
  char *connection = mongory_matcher_tail_connection(ctx->count, ctx->total);
  ctx->count++;
  char *title = mongory_matcher_title(matcher, ctx->pool);
  printf("%s%s%s\n", ctx->prefix, connection, title);
}

void mongory_matcher_traverse_explain(mongory_matcher *matcher, mongory_matcher_explain_context *ctx) {
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  if (composite->left) {
    composite->left->explain(composite->left, ctx);
  }
  if (composite->right) {
    composite->right->explain(composite->right, ctx);
  }
}

void mongory_matcher_composite_explain(mongory_matcher *matcher, mongory_matcher_explain_context *ctx) {
  mongory_matcher_base_explain(matcher, ctx);
  mongory_string_buffer *buffer = mongory_string_buffer_new(ctx->pool);
  mongory_string_buffer_appendf(buffer, "%s%s", ctx->prefix, mongory_matcher_indent_connection(ctx->count, ctx->total));
  mongory_matcher_explain_context child_ctx = {
      .pool = ctx->pool,
      .count = 0,
      .total = matcher->sub_count,
      .prefix = mongory_string_buffer_cstr(buffer),
  };
  mongory_matcher_traverse_explain(matcher, &child_ctx);
}

static inline void mongory_matcher_literal_shared_explain(mongory_matcher *matcher, mongory_matcher_explain_context *ctx) {
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  mongory_string_buffer *buffer = mongory_string_buffer_new(ctx->pool);
  mongory_string_buffer_appendf(buffer, "%s%s", ctx->prefix, mongory_matcher_indent_connection(ctx->count, ctx->total));
  mongory_matcher_explain_context child_ctx = {
      .pool = ctx->pool,
      .count = 0,
      .total = matcher->sub_count,
      .prefix = mongory_string_buffer_cstr(buffer),
  };
  if (composite->right) {
    composite->right->explain(composite->right, &child_ctx);
  } else {
    composite->left->explain(composite->left, &child_ctx);
  }
}

void mongory_matcher_literal_explain(mongory_matcher *matcher, mongory_matcher_explain_context *ctx) {
  mongory_matcher_base_explain(matcher, ctx);
  mongory_matcher_literal_shared_explain(matcher, ctx);
}

void mongory_matcher_field_explain(mongory_matcher *matcher, mongory_matcher_explain_context *ctx) {
  char *connection = mongory_matcher_tail_connection(ctx->count, ctx->total);
  char *title = mongory_matcher_title_with_field(matcher, ctx->pool);
  ctx->count++;
  printf("%s%s%s\n", ctx->prefix, connection, title);
  mongory_matcher_literal_shared_explain(matcher, ctx);
}

#endif /* MONGORY_MATCHER_EXPLAINABLE_C */
