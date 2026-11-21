#ifndef MONGORY_MATCHER_EXPLAINABLE_C
#define MONGORY_MATCHER_EXPLAINABLE_C

#include "matcher_explainable.h"
#include "../foundations/string_buffer.h"
#include "base_matcher.h"
#include "composite_matcher.h"
#include "value_matcher.h"
#include "../foundations/utils.h"
#include <stdio.h>

char *mongory_matcher_title(mongory_matcher *matcher, mongory_memory_pool *pool) {
  mongory_value *condition = matcher->condition;
  char *cond_str = (condition && condition->to_str) ? condition->to_str(condition, pool) : NULL;
  if (!cond_str) {
    return NULL;
  }
  return mongory_string_cpyf(pool, "%s: %s",
    matcher->name ? matcher->name : "",
    cond_str
  );
}

char *mongory_matcher_title_with_field(mongory_matcher *matcher, mongory_memory_pool *pool) {
  mongory_field_matcher *field_matcher = (mongory_field_matcher *)matcher;
  mongory_value *condition = matcher->condition;
  char *cond_str = (condition && condition->to_str) ? condition->to_str(condition, pool) : NULL;
  if (!cond_str) {
    return NULL;
  }
  return mongory_string_cpyf(pool, "Field: \"%s\", to match: %s",
    field_matcher->field ? field_matcher->field : "",
    cond_str
  );
}

static inline char *mongory_matcher_tail_connection(int count, int total) {
  if (total == 0) {
    return "";
  }
  if (total - count == 1) {
    return "└─ ";
  }
  return "├─ ";
}

static inline char *mongory_matcher_indent_connection(int count, int total) {
  if (total == 0) {
    return "";
  }
  if (total - count == 1) {
    return "   ";
  }
  return "│  ";
}

static mongory_explain_acc *mongory_explain_acc_child(
  mongory_memory_pool *pool,
  mongory_explain_acc *parent_acc,
  int count,
  int total
) {
  char *indent = mongory_matcher_indent_connection(count, total);
  mongory_explain_acc *next = MG_ALLOC_PTR(pool, mongory_explain_acc);
  if (!next) {
    return NULL;
  }
  next->prefix = mongory_string_cpyf(pool, "%s%s", parent_acc->prefix, indent);
  if (!next->prefix) {
    return NULL;
  }
  next->buf = parent_acc->buf;
  return next;
}

static bool mongory_matcher_append_explain_line(
  mongory_matcher_traverse_context *ctx,
  char *title
) {
  mongory_explain_acc *acc = (mongory_explain_acc *)ctx->acc;
  char *connection = mongory_matcher_tail_connection(ctx->count, ctx->total);
  if (!acc || !acc->buf || !title) {
    return false;
  }
  mongory_string_buffer_appendf(acc->buf, "%s%s%s\n", acc->prefix, connection, title);
  return ctx->pool->error == NULL;
}

bool mongory_matcher_base_explain(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  char *title = mongory_matcher_title(matcher, ctx->pool);
  return mongory_matcher_append_explain_line(ctx, title);
}

bool mongory_matcher_composite_explain(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  if (!mongory_matcher_base_explain(matcher, ctx)) {
    return false;
  }
  mongory_explain_acc *acc = (mongory_explain_acc *)ctx->acc;
  ctx->acc = mongory_explain_acc_child(ctx->pool, acc, ctx->count, ctx->total);
  return ctx->acc != NULL;
}

bool mongory_matcher_value_explain(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  return mongory_matcher_composite_explain(matcher, ctx);
}

bool mongory_matcher_field_explain(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  char *title = mongory_matcher_title_with_field(matcher, ctx->pool);
  if (!mongory_matcher_append_explain_line(ctx, title)) {
    return false;
  }
  mongory_explain_acc *acc = (mongory_explain_acc *)ctx->acc;
  ctx->acc = mongory_explain_acc_child(ctx->pool, acc, ctx->count, ctx->total);
  return ctx->acc != NULL;
}

#endif /* MONGORY_MATCHER_EXPLAINABLE_C */
