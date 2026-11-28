/**
 * @file matcher.c
 * @brief Implements the generic mongory_matcher constructor.
 *
 * This file provides the implementation for the top-level matcher creation
 * function.
 */
#include <stdio.h>
#include <string.h>
#include "mongory-core/matchers/matcher.h" // Public API
#include "../foundations/utils.h"           // For mongory_string_cpyf
// Required internal headers for delegation
#include "../foundations/config_private.h" // Potentially for global settings
#include "../foundations/memory_pool_private.h"
#include "base_matcher.h"                  // For mongory_matcher_base_new if used directly
#include "composite_matcher.h"             // For mongory_matcher_condition_new
#include "value_matcher.h"               // Potentially for other default constructions
#include "mongory-core/foundations/array.h"
#include "../foundations/string_buffer.h"
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include <mongory-core.h> // General include, might not be strictly necessary here


// ============================================================================
// Dotted-Key Condition Expansion
// ============================================================================

static mongory_value *mongory_expand_table(mongory_memory_pool *pool, mongory_value *value, size_t depth);

static bool mongory_expand_deep_merge_set(mongory_memory_pool *pool, mongory_table *table, char *key,
                                          mongory_value *value);

typedef struct mongory_expand_merge_ctx {
  mongory_memory_pool *pool;
  mongory_table *target;
} mongory_expand_merge_ctx;

static bool mongory_expand_merge_each(char *key, mongory_value *value, void *acc) {
  mongory_expand_merge_ctx *ctx = (mongory_expand_merge_ctx *)acc;
  return mongory_expand_deep_merge_set(ctx->pool, ctx->target, key, value);
}

static bool mongory_expand_deep_merge_set(mongory_memory_pool *pool, mongory_table *table, char *key,
                                          mongory_value *value) {
  mongory_value *existing = table->get(table, key);
  if (!existing) {
    return table->set(table, key, value);
  }
  if (existing->type == MONGORY_TYPE_TABLE && existing->data.t &&
      value && value->type == MONGORY_TYPE_TABLE && value->data.t &&
      key[0] != '$') {
    mongory_expand_merge_ctx ctx = {pool, existing->data.t};
    return value->data.t->each(value->data.t, &ctx, mongory_expand_merge_each);
  }
  // Leaf collision: preserve both conditions in `$and`.
  table->del(table, key);
  mongory_table *left_branch = mongory_table_new(pool);
  mongory_table *right_branch = mongory_table_new(pool);
  if (!left_branch || !right_branch) {
    return false;
  }
  if (!left_branch->set(left_branch, key, existing) || !right_branch->set(right_branch, key, value)) {
    return false;
  }
  mongory_value *left_val = mongory_value_wrap_t(pool, left_branch);
  mongory_value *right_val = mongory_value_wrap_t(pool, right_branch);
  if (!left_val || !right_val) {
    return false;
  }

  mongory_value *and_val = table->get(table, "$and");
  mongory_array *and_arr = NULL;
  if (and_val && and_val->type == MONGORY_TYPE_ARRAY && and_val->data.a) {
    and_arr = and_val->data.a;
  } else {
    and_arr = mongory_array_new(pool);
    if (!and_arr) {
      return false;
    }
    if (and_val) {
      mongory_table *prev_and = mongory_table_new(pool);
      if (!prev_and || !prev_and->set(prev_and, "$and", and_val)) {
        return false;
      }
      mongory_value *prev_and_val = mongory_value_wrap_t(pool, prev_and);
      if (!prev_and_val || !and_arr->push(and_arr, prev_and_val)) {
        return false;
      }
    }
    mongory_value *and_arr_val = mongory_value_wrap_a(pool, and_arr);
    if (!and_arr_val || !table->set(table, "$and", and_arr_val)) {
      return false;
    }
  }
  return and_arr->push(and_arr, left_val) && and_arr->push(and_arr, right_val);
}

static char **mongory_split_dotted_key(mongory_memory_pool *pool, const char *key, size_t base_depth,
                                       size_t *count_out) {
  size_t len = strlen(key);
  char *buf = (char *)MG_ALLOC(pool, len + 1);
  if (!buf) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  size_t out = 0;
  size_t segments = 1;
  for (size_t i = 0; i < len; i++) {
    if (key[i] == '\\' && key[i + 1] == '.') {
      buf[out++] = '.';
      i++;
    } else if (key[i] == '.') {
      buf[out++] = '\0';
      segments++;
    } else {
      buf[out++] = key[i];
    }
  }
  buf[out] = '\0';
  while (segments > 1 && out > 0 && buf[out - 1] == '\0') {
    out--;
    segments--;
  }

  size_t max_depth = mongory_max_depth_get();
  if (max_depth > 0 && base_depth + (segments - 1) > max_depth) {
    pool->error = &MONGORY_DEPTH_LIMIT_ERROR;
    return NULL;
  }

  char **parts = MG_ALLOC_ARY(pool, char *, segments);
  if (!parts) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  char *cursor = buf;
  for (size_t i = 0; i < segments; i++) {
    parts[i] = cursor;
    cursor += strlen(cursor) + 1;
  }
  *count_out = segments;
  return parts;
}

static mongory_value *mongory_expand_value_for_key(mongory_memory_pool *pool, const char *key, mongory_value *value,
                                                   size_t depth) {
  if (!value) {
    return value;
  }
  if (key[0] == '$') {
    if (strcmp(key, "$and") == 0 || strcmp(key, "$or") == 0 || strcmp(key, "$nor") == 0) {
      if (value->type != MONGORY_TYPE_ARRAY || !value->data.a) {
        return value;
      }
      mongory_array *src = value->data.a;
      mongory_array *dst = mongory_array_new(pool);
      if (!dst) {
        return NULL;
      }
      for (size_t i = 0; i < src->count; i++) {
        mongory_value *elem = src->get(src, i);
        mongory_value *norm = (elem && elem->type == MONGORY_TYPE_TABLE && elem->data.t)
                                  ? mongory_expand_table(pool, elem, depth + 1)
                                  : elem;
        if (pool->error != NULL || !dst->push(dst, norm)) {
          return NULL;
        }
      }
      mongory_value *wrapped = mongory_value_wrap_a(pool, dst);
      if (wrapped) {
        wrapped->origin = value->origin;
      }
      return wrapped;
    }
    if (strcmp(key, "$elemMatch") == 0 || strcmp(key, "$every") == 0 ||
        strcmp(key, "$not") == 0 || strcmp(key, "$size") == 0) {
      if (value->type == MONGORY_TYPE_TABLE && value->data.t) {
        return mongory_expand_table(pool, value, depth + 1);
      }
    }
    // Literal comparison/inclusion/existence/regex and custom operators keep raw operands verbatim.
    return value;
  }
  if (value->type == MONGORY_TYPE_TABLE && value->data.t) {
    return mongory_expand_table(pool, value, depth + 1);
  }
  return value;
}

typedef struct mongory_expand_table_ctx {
  mongory_memory_pool *pool;
  mongory_table *result;
  size_t depth;
} mongory_expand_table_ctx;

static bool mongory_expand_table_pair(char *key, mongory_value *value, void *acc) {
  mongory_expand_table_ctx *ctx = (mongory_expand_table_ctx *)acc;
  mongory_memory_pool *pool = ctx->pool;
  size_t count = 0;
  char **parts = mongory_split_dotted_key(pool, key, ctx->depth, &count);
  if (!parts) {
    return false;
  }

  mongory_value *nested = mongory_expand_value_for_key(pool, parts[count - 1], value, ctx->depth + (count - 1));
  if (pool->error != NULL || (value != NULL && nested == NULL)) {
    return false;
  }
  for (size_t i = count - 1; i >= 1; i--) {
    mongory_table *wrapper = mongory_table_new(pool);
    if (!wrapper || !wrapper->set(wrapper, parts[i], nested)) {
      return false;
    }
    nested = mongory_value_wrap_t(pool, wrapper);
    if (!nested) {
      return false;
    }
  }
  return mongory_expand_deep_merge_set(pool, ctx->result, parts[0], nested);
}

static mongory_value *mongory_expand_table(mongory_memory_pool *pool, mongory_value *value, size_t depth) {
  size_t max_depth = mongory_max_depth_get();
  if (max_depth > 0 && depth > max_depth) {
    pool->error = &MONGORY_DEPTH_LIMIT_ERROR;
    return NULL;
  }
  mongory_table *result = mongory_table_new(pool);
  if (!result) {
    return NULL;
  }
  mongory_expand_table_ctx ctx = {pool, result, depth};
  if (!value->data.t->each(value->data.t, &ctx, mongory_expand_table_pair) || pool->error != NULL) {
    return NULL;
  }
  mongory_value *wrapped = mongory_value_wrap_t(pool, result);
  if (wrapped) {
    wrapped->origin = value->origin;
  }
  return wrapped;
}

mongory_matcher *mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition, void *extern_ctx) {
  if (!pool || !MONGORY_VALIDATE_TABLE(pool, condition)) {
    return NULL;
  }
  mongory_compile_budget saved = mongory_compile_budget_begin();
  mongory_value *expanded = mongory_expand_table(pool, condition, 1);
  mongory_matcher *matcher = (expanded && pool->error == NULL)
                                 ? mongory_matcher_condition_new(pool, expanded, extern_ctx)
                                 : NULL;
  mongory_compile_budget_end(saved);
  return matcher;
}

/**
 * @brief Executes the matching logic for the given matcher.
 *
 * This function is a polymorphic wrapper. It invokes the `match` function
 * pointer on the specific `mongory_matcher` instance, which will be one of
 * the internal matching functions (e.g., from a compare_matcher or
 * composite_matcher).
 *
 * @param matcher The matcher to use.
 * @param value The value to check against the matcher's condition.
 * @return True if the value satisfies the matcher's condition, false otherwise.
 */
bool mongory_matcher_match(mongory_matcher *matcher, mongory_value *value) { return matcher->match(matcher, value); }

static mongory_value *mongory_matcher_build_explain_node(
    mongory_matcher *matcher,
    mongory_memory_pool *pool,
    bool as_skip
) {
  MONGORY_VALIDATE_PTR(pool, matcher);
  if (pool->error != NULL) {
    return NULL;
  }
  mongory_table *node = mongory_table_new(pool);
  mongory_array *children = mongory_array_new(pool);
  if (!node || !children || pool->error != NULL) {
    return NULL;
  }
  const char *name = matcher->name ? matcher->name : "";
  node->set(node, "name", mongory_value_wrap_s(pool, (char *)name));
  node->set(node, "condition", matcher->condition ? matcher->condition : mongory_value_wrap_n(pool, NULL));
  node->set(node, "priority", mongory_value_wrap_d(pool, matcher->priority));
  if (strcmp(name, "Field") == 0) {
    mongory_field_matcher *field_matcher = (mongory_field_matcher *)matcher;
    if (field_matcher->field) {
      node->set(node, "field", mongory_value_wrap_s(pool, field_matcher->field));
    }
  }
  if (as_skip) {
    node->set(node, "result", mongory_value_wrap_s(pool, "skip"));
  }

  if (matcher->traverse == mongory_matcher_composite_traverse) {
    mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
    mongory_array *src_children = mongory_atomic_load_ptr_acquire(&composite->children);
    int total = src_children ? (int)src_children->count : 0;
    for (int i = 0; i < total; i++) {
      mongory_matcher *child = (mongory_matcher *)src_children->get(src_children, i);
      mongory_value *child_node = mongory_matcher_build_explain_node(child, pool, as_skip);
      if (!child_node || pool->error != NULL) {
        return NULL;
      }
      children->push(children, child_node);
    }
  } else if (matcher->traverse == mongory_matcher_value_traverse) {
    mongory_value_matcher *literal = (mongory_value_matcher *)matcher;
    mongory_matcher *next = literal->implicit_array_matcher ? literal->implicit_array_matcher : literal->delegate_matcher;
    if (next) {
      mongory_value *child_node = mongory_matcher_build_explain_node(next, pool, as_skip);
      if (!child_node || pool->error != NULL) {
        return NULL;
      }
      children->push(children, child_node);
    }
  }

  node->set(node, "children", mongory_value_wrap_a(pool, children));
  return mongory_value_wrap_t(pool, node);
}

static void mongory_explain_tree_print_node(
    mongory_value *node_val,
    mongory_memory_pool *pool,
    const char *prefix,
    int count,
    int total
) {
  if (!node_val || node_val->type != MONGORY_TYPE_TABLE || !node_val->data.t) {
    return;
  }
  mongory_table *node = node_val->data.t;
  mongory_value *name_val = node->get(node, "name");
  mongory_value *cond_val = node->get(node, "condition");
  mongory_value *field_val = node->get(node, "field");
  mongory_value *children_val = node->get(node, "children");

  const char *connection = "";
  const char *indent = "";
  if (total > 0) {
    if (total - count == 1) {
      connection = "└─ ";
      indent = "   ";
    } else {
      connection = "├─ ";
      indent = "│  ";
    }
  }

  const char *name = (name_val && name_val->type == MONGORY_TYPE_STRING && name_val->data.s) ? name_val->data.s : "";
  char *cond_str = (cond_val && cond_val->to_str) ? cond_val->to_str(cond_val, pool) : "null";
  if (field_val && field_val->type == MONGORY_TYPE_STRING && field_val->data.s) {
    printf("%s%sField: \"%s\", to match: %s\n", prefix, connection, field_val->data.s, cond_str);
  } else {
    printf("%s%s%s: %s\n", prefix, connection, name, cond_str);
  }

  if (children_val && children_val->type == MONGORY_TYPE_ARRAY && children_val->data.a) {
    mongory_array *children = children_val->data.a;
    int child_total = (int)children->count;
    char *next_prefix = mongory_string_cpyf(pool, "%s%s", prefix, indent);
    if (!next_prefix) {
      return;
    }
    for (int i = 0; i < child_total; i++) {
      mongory_explain_tree_print_node(children->get(children, i), pool, next_prefix, i, child_total);
    }
  }
}

mongory_value *mongory_matcher_explain_tree(mongory_matcher *matcher, mongory_memory_pool *temp_pool) {
  MONGORY_VALIDATE_PTR(temp_pool, matcher);
  if (temp_pool->error != NULL) {
    return NULL;
  }
  return mongory_matcher_build_explain_node(matcher, temp_pool, false);
}

void mongory_matcher_explain(mongory_matcher *matcher, mongory_memory_pool *temp_pool) {
  mongory_value *tree = mongory_matcher_explain_tree(matcher, temp_pool);
  if (tree != NULL) {
    mongory_explain_tree_print_node(tree, temp_pool, "", 0, 0);
  }
}

typedef struct mongory_matcher_traced_match_context {
  mongory_matcher *matcher;
  mongory_value *record;
  bool matched;
  int level;
} mongory_matcher_traced_match_context;

static bool mongory_matcher_traced_match(mongory_matcher *matcher, mongory_value *value) {
  bool matched = matcher->original_match(matcher, value);
  mongory_array *trace_stack = matcher->trace_stack;
  mongory_memory_pool *pool = trace_stack->pool;
  MONGORY_VALIDATE_PTR(pool, matcher->name);
  if (pool->error != NULL) {
    return false;
  }

  mongory_matcher_traced_match_context *trace_result = MG_ALLOC_PTR(pool, mongory_matcher_traced_match_context);
  if (!trace_result) {
    return matched;
  }
  trace_result->matcher = matcher;
  trace_result->record = value;
  trace_result->matched = matched;
  trace_result->level = matcher->trace_level;
  trace_stack->push(trace_stack, mongory_value_wrap_ptr(pool, (void *)trace_result));

  return matched;
}

static bool mongory_matcher_enable_trace_cb(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  matcher->trace_stack = (mongory_array *)ctx->acc;
  matcher->trace_level = ctx->level;
  matcher->match = mongory_matcher_traced_match;
  if (matcher->traverse == mongory_matcher_value_traverse) {
    mongory_value_matcher *literal = (mongory_value_matcher *)matcher;
    if (literal->implicit_array_matcher && literal->delegate_matcher) {
      mongory_matcher_traverse_context delegate_ctx = {
          .level = ctx->level + 1,
          .acc = ctx->acc,
          .callback = ctx->callback,
      };
      literal->delegate_matcher->traverse(literal->delegate_matcher, &delegate_ctx);
    }
  }
  return true;
}

static bool mongory_matcher_disable_trace_cb(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  matcher->match = matcher->original_match;
  matcher->trace_stack = NULL;
  if (matcher->traverse == mongory_matcher_value_traverse) {
    mongory_value_matcher *literal = (mongory_value_matcher *)matcher;
    if (literal->implicit_array_matcher && literal->delegate_matcher) {
      mongory_matcher_traverse_context delegate_ctx = {
          .level = ctx->level + 1,
          .callback = ctx->callback,
      };
      literal->delegate_matcher->traverse(literal->delegate_matcher, &delegate_ctx);
    }
  }
  return true;
}

static bool mongory_matcher_ptr_in_array(mongory_array *arr, mongory_matcher *target) {
  int total = arr ? (int)arr->count : 0;
  for (int i = 0; i < total; i++) {
    mongory_value *v = arr->get(arr, i);
    if (v && v->data.ptr == (void *)target) {
      return true;
    }
  }
  return false;
}

static mongory_array *mongory_matcher_build_trace_nodes(
    mongory_array *traces,
    int level,
    mongory_memory_pool *pool,
    mongory_array *executed_matchers_out
) {
  MONGORY_VALIDATE_PTR(pool, traces) && MONGORY_VALIDATE_PTR(pool, traces->get);
  if (pool->error != NULL) {
    return NULL;
  }
  mongory_array *nodes = mongory_array_new(pool);
  mongory_array *group = mongory_array_new(pool);
  if (!nodes || !group || pool->error != NULL) {
    return NULL;
  }
  int total = (int)traces->count;
  for (int i = 0; i < total; i++) {
    mongory_value *item = traces->get(traces, i);
    MONGORY_VALIDATE_PTR(pool, item) && MONGORY_VALIDATE_PTR(pool, item->data.ptr);
    if (pool->error != NULL) {
      return NULL;
    }
    mongory_matcher_traced_match_context *entry = (mongory_matcher_traced_match_context *)item->data.ptr;
    if (entry->level == level) {
      if (executed_matchers_out) {
        mongory_value *ptr_val = mongory_value_wrap_ptr(pool, (void *)entry->matcher);
        if (!ptr_val || !executed_matchers_out->push(executed_matchers_out, ptr_val)) {
          return NULL;
        }
      }
      mongory_array *executed_children = mongory_array_new(pool);
      if (!executed_children || pool->error != NULL) {
        return NULL;
      }
      mongory_array *children = mongory_matcher_build_trace_nodes(group, level + 1, pool, executed_children);
      if (!children || pool->error != NULL) {
        return NULL;
      }

      mongory_matcher *m = entry->matcher;
      if (m->traverse == mongory_matcher_composite_traverse) {
        mongory_composite_matcher *comp = (mongory_composite_matcher *)m;
        mongory_array *src_children = mongory_atomic_load_ptr_acquire(&comp->children);
        int child_total = src_children ? (int)src_children->count : 0;
        for (int j = 0; j < child_total; j++) {
          mongory_matcher *child = (mongory_matcher *)src_children->get(src_children, j);
          if (!mongory_matcher_ptr_in_array(executed_children, child)) {
            mongory_value *skip_node = mongory_matcher_build_explain_node(child, pool, true);
            if (!skip_node || pool->error != NULL) {
              return NULL;
            }
            children->push(children, skip_node);
          }
        }
      } else if (m->traverse == mongory_matcher_value_traverse) {
        mongory_value_matcher *lit = (mongory_value_matcher *)m;
        mongory_matcher *next = lit->implicit_array_matcher ? lit->implicit_array_matcher : lit->delegate_matcher;
        if (next && children->count == 0) {
          mongory_value *skip_node = mongory_matcher_build_explain_node(next, pool, true);
          if (!skip_node || pool->error != NULL) {
            return NULL;
          }
          children->push(children, skip_node);
        }
      }

      mongory_table *node = mongory_table_new(pool);
      if (!node || pool->error != NULL) {
        return NULL;
      }
      const char *name = m->name ? m->name : "";
      node->set(node, "name", mongory_value_wrap_s(pool, (char *)name));
      node->set(node, "condition", m->condition ? m->condition : mongory_value_wrap_n(pool, NULL));
      node->set(node, "priority", mongory_value_wrap_d(pool, m->priority));
      if (strcmp(name, "Field") == 0) {
        mongory_field_matcher *field_matcher = (mongory_field_matcher *)m;
        if (field_matcher->field) {
          node->set(node, "field", mongory_value_wrap_s(pool, field_matcher->field));
        }
      }
      if (entry->record != NULL) {
        node->set(node, "record", entry->record);
      }
      node->set(node, "result", mongory_value_wrap_s(pool, entry->matched ? "pass" : "fail"));
      node->set(node, "children", mongory_value_wrap_a(pool, children));

      mongory_value *wrapped_node = mongory_value_wrap_t(pool, node);
      if (!wrapped_node || !nodes->push(nodes, wrapped_node) || pool->error != NULL) {
        return NULL;
      }
      group = mongory_array_new(pool);
      if (!group) {
        return NULL;
      }
    } else {
      if (!group->push(group, item)) {
        return NULL;
      }
    }
  }
  return nodes;
}

void mongory_matcher_enable_trace_at(mongory_matcher *matcher, mongory_array *trace_stack, int level) {
  if (!matcher || !trace_stack) {
    return;
  }
  MONGORY_VALIDATE_PTR(trace_stack->pool, matcher->traverse);
  if (trace_stack->pool->error != NULL) {
    return;
  }
  mongory_matcher_traverse_context ctx = {
      .level = level,
      .acc = (void *)trace_stack,
      .callback = mongory_matcher_enable_trace_cb,
  };
  matcher->traverse(matcher, &ctx);
}

void mongory_matcher_enable_trace(mongory_matcher *matcher, mongory_memory_pool *temp_pool) {
  MONGORY_VALIDATE_PTR(temp_pool, matcher) && MONGORY_VALIDATE_PTR(temp_pool, matcher->traverse);
  if (temp_pool->error != NULL) {
    return;
  }
  mongory_array *trace_stack = mongory_array_new(temp_pool);
  if (!trace_stack) {
    return;
  }
  mongory_matcher_enable_trace_at(matcher, trace_stack, 0);
}

void mongory_matcher_disable_trace(mongory_matcher *matcher) {
  MONGORY_VALIDATE_PTR(matcher->pool, matcher) && MONGORY_VALIDATE_PTR(matcher->pool, matcher->traverse);
  if (matcher->pool->error != NULL) {
    return;
  }
  mongory_matcher_traverse_context ctx = {
      .level = 0,
      .callback = mongory_matcher_disable_trace_cb,
  };
  matcher->traverse(matcher, &ctx);
}

static void mongory_matcher_print_trace_node(mongory_value *node_val, mongory_memory_pool *pool, int level) {
  if (!node_val || node_val->type != MONGORY_TYPE_TABLE || !node_val->data.t) {
    return;
  }
  mongory_table *node = node_val->data.t;
  mongory_value *name_val = node->get(node, "name");
  mongory_value *cond_val = node->get(node, "condition");
  mongory_value *field_val = node->get(node, "field");
  mongory_value *record_val = node->get(node, "record");
  mongory_value *result_val = node->get(node, "result");
  mongory_value *children_val = node->get(node, "children");

  const char *name = (name_val && name_val->type == MONGORY_TYPE_STRING && name_val->data.s) ? name_val->data.s : "";
  const char *res_raw = (result_val && result_val->type == MONGORY_TYPE_STRING && result_val->data.s) ? result_val->data.s : "fail";
  const char *res_label = NULL;
  bool is_skip = strcmp(res_raw, "skip") == 0;
  bool is_pass = strcmp(res_raw, "pass") == 0;
  if (mongory_matcher_trace_result_colorful) {
    if (is_skip) {
      res_label = "\e[30;43mSkipped\e[0m";
    } else if (is_pass) {
      res_label = "\e[30;42mMatched\e[0m";
    } else {
      res_label = "\e[30;41mDismatch\e[0m";
    }
  } else {
    if (is_skip) {
      res_label = "Skipped";
    } else if (is_pass) {
      res_label = "Matched";
    } else {
      res_label = "Dismatch";
    }
  }

  char *cdtn = (cond_val && cond_val->to_str) ? cond_val->to_str(cond_val, pool) : "null";
  char *rcd = (record_val && record_val->to_str) ? record_val->to_str(record_val, pool) : "Nothing";

  for (int i = 0; i < level * 2; i++) {
    putchar(' ');
  }
  if (field_val && field_val->type == MONGORY_TYPE_STRING && field_val->data.s) {
    if (is_skip) {
      printf("%s: %s, field: \"%s\", condition: %s\n", name, res_label, field_val->data.s, cdtn);
    } else {
      printf("%s: %s, field: \"%s\", condition: %s, record: %s\n", name, res_label, field_val->data.s, cdtn, rcd);
    }
  } else {
    if (is_skip) {
      printf("%s: %s, condition: %s\n", name, res_label, cdtn);
    } else {
      printf("%s: %s, condition: %s, record: %s\n", name, res_label, cdtn, rcd);
    }
  }

  if (children_val && children_val->type == MONGORY_TYPE_ARRAY && children_val->data.a) {
    mongory_array *children = children_val->data.a;
    int child_total = (int)children->count;
    for (int i = 0; i < child_total; i++) {
      mongory_matcher_print_trace_node(children->get(children, i), pool, level + 1);
    }
  }
}

void mongory_matcher_print_trace(mongory_matcher *matcher) {
  if (!matcher || matcher->trace_stack == NULL) {
    return;
  }
  mongory_memory_pool *pool = matcher->trace_stack->pool;
  mongory_array *roots = mongory_matcher_build_trace_nodes(matcher->trace_stack, 0, pool, NULL);
  if (!roots || pool->error != NULL) {
    return;
  }
  int total = (int)roots->count;
  for (int i = 0; i < total; i++) {
    mongory_matcher_print_trace_node(roots->get(roots, i), pool, 0);
  }
}

mongory_value *mongory_matcher_trace_tree(mongory_matcher *matcher, mongory_value *value) {
  mongory_memory_pool *pool = value ? value->pool : (matcher ? matcher->pool : NULL);
  MONGORY_VALIDATE_PTR(pool, matcher) && MONGORY_VALIDATE_PTR(pool, matcher->match);
  if (!pool || pool->error != NULL) {
    return NULL;
  }
  mongory_matcher_enable_trace(matcher, pool);
  if (pool->error != NULL) {
    mongory_matcher_disable_trace(matcher);
    return NULL;
  }
  matcher->match(matcher, value);
  mongory_array *roots = mongory_matcher_build_trace_nodes(matcher->trace_stack, 0, pool, NULL);
  mongory_matcher_disable_trace(matcher);
  if (!roots || roots->count == 0 || pool->error != NULL) {
    return NULL;
  }
  return roots->get(roots, 0);
}

bool mongory_matcher_trace(mongory_matcher *matcher, mongory_value *value) {
  mongory_memory_pool *pool = value ? value->pool : (matcher ? matcher->pool : NULL);
  MONGORY_VALIDATE_PTR(pool, matcher) && MONGORY_VALIDATE_PTR(pool, matcher->match);
  if (!pool || pool->error != NULL) {
    return false;
  }
  mongory_value *tree = mongory_matcher_trace_tree(matcher, value);
  if (!tree || tree->type != MONGORY_TYPE_TABLE || !tree->data.t) {
    return false;
  }
  mongory_matcher_print_trace_node(tree, pool, 0);
  mongory_value *result_val = tree->data.t->get(tree->data.t, "result");
  return result_val && result_val->type == MONGORY_TYPE_STRING && result_val->data.s && strcmp(result_val->data.s, "pass") == 0;
}
