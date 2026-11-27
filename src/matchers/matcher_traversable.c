#ifndef MONGORY_MATCHER_TRAVERSABLE_C
#define MONGORY_MATCHER_TRAVERSABLE_C

#include "matcher_traversable.h"
#include "../foundations/memory_pool_private.h"
#include "base_matcher.h"
#include "composite_matcher.h"
#include "value_matcher.h"

bool mongory_matcher_leaf_traverse(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  return ctx->callback(matcher, ctx);
}

bool mongory_matcher_composite_traverse(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  void *prev_acc = ctx->acc;
  if (!mongory_matcher_leaf_traverse(matcher, ctx))
    return false;
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  mongory_array *children = mongory_atomic_load_ptr_acquire(&composite->children);
  int total = (int)children->count;
  mongory_matcher_traverse_context child_ctx = {
      .level = ctx->level + 1,
      .acc = ctx->acc,
      .callback = ctx->callback,
  };

  for (int i = 0; i < total; i++) {
    mongory_matcher *child = (mongory_matcher *)children->get(children, i);
    if (!child->traverse(child, &child_ctx))
      return false;
  }
  ctx->acc = prev_acc;
  return true;
}

bool mongory_matcher_value_traverse(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  void *prev_acc = ctx->acc;
  if (!mongory_matcher_leaf_traverse(matcher, ctx))
    return false;
  mongory_value_matcher *literal = (mongory_value_matcher *)matcher;
  mongory_matcher *next_matcher = literal->implicit_array_matcher ? literal->implicit_array_matcher : literal->delegate_matcher;
  mongory_matcher_traverse_context child_ctx = {
      .level = ctx->level + 1,
      .acc = ctx->acc,
      .callback = ctx->callback,
  };
  bool result = next_matcher->traverse(next_matcher, &child_ctx);
  ctx->acc = prev_acc;
  return result;
}
#endif /* MONGORY_MATCHER_TRAVERSABLE_C */
