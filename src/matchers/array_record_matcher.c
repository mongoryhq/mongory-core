#include "array_record_matcher.h"
#include "../foundations/config_private.h"
#include "base_matcher.h"
#include "compare_matcher.h"
#include "composite_matcher.h"
#include "literal_matcher.h"
#include "mongory-core/foundations/table.h"
#include "mongory-core/foundations/value.h"
#include <mongory-core.h>
#include <stdio.h>
#include <string.h>

typedef struct mongory_matcher_array_record_parse_table_context {
  mongory_table *parsed_table;
  mongory_table *elem_match_table;
} mongory_matcher_array_record_parse_table_context;

static inline bool
mongory_matcher_array_record_set_table_elem(char *key, mongory_value *value,
                                            void *acc) {
  mongory_matcher_array_record_parse_table_context *context =
      (mongory_matcher_array_record_parse_table_context *)acc;
  mongory_table *elem_match_table = context->elem_match_table;
  elem_match_table->set(elem_match_table, key, value);
  return true;
}

static inline bool mongory_matcher_array_record_parse_table_foreach(
    char *key, mongory_value *value, void *acc) {
  mongory_matcher_array_record_parse_table_context *context =
      (mongory_matcher_array_record_parse_table_context *)acc;
  mongory_table *parsed_table = context->parsed_table;
  mongory_table *elem_match_table = context->elem_match_table;
  if (strcmp(key, "$elemMatch") == 0 && value->type == MONGORY_TYPE_TABLE &&
      value->data.t != NULL) {
    mongory_table *value_table = value->data.t;
    value_table->each(value_table, context,
                      mongory_matcher_array_record_set_table_elem);
  } else if (*key == '$' || mongory_try_parse_int(key, NULL)) {
    parsed_table->set(parsed_table, key, value);
  } else {
    elem_match_table->set(elem_match_table, key, value);
  }
  return true;
}

static inline mongory_value *
mongory_matcher_array_record_parse_table(mongory_value *condition) {
  mongory_table *parsed_table = mongory_table_new(condition->pool);
  mongory_table *elem_match_table = mongory_table_new(condition->pool);
  mongory_matcher_array_record_parse_table_context context = {parsed_table,
                                                              elem_match_table};
  mongory_table *table = condition->data.t;
  table->each(table, &context,
              mongory_matcher_array_record_parse_table_foreach);
  if (elem_match_table->count > 0) {
    parsed_table->set(parsed_table, "$elemMatch",
                      mongory_value_wrap_t(condition->pool, elem_match_table));
  }
  return mongory_value_wrap_t(condition->pool, parsed_table);
}

static inline mongory_matcher *
mongory_matcher_array_record_elem_match_regex_new(mongory_memory_pool *pool,
                                                  mongory_value *condition) {
  mongory_table *table = mongory_table_new(pool);
  table->set(table, "$regex", condition);
  return mongory_matcher_elem_match_new(pool,
                                        mongory_value_wrap_t(pool, table));
}

static inline mongory_matcher *
mongory_matcher_array_record_elem_match_equal_new(mongory_memory_pool *pool,
                                                  mongory_value *condition) {
  mongory_table *table = mongory_table_new(pool);
  table->set(table, "$eq", condition);
  return mongory_matcher_elem_match_new(pool,
                                        mongory_value_wrap_t(pool, table));
}

static inline mongory_matcher *
mongory_matcher_array_record_left_delegate(mongory_memory_pool *pool,
                                           mongory_value *condition) {
  switch (condition->type) {
  case MONGORY_TYPE_TABLE:
    return mongory_matcher_table_cond_new(
        pool, mongory_matcher_array_record_parse_table(condition));
  case MONGORY_TYPE_REGEX:
    return mongory_matcher_array_record_elem_match_regex_new(pool, condition);
  default:
    return mongory_matcher_array_record_elem_match_equal_new(pool, condition);
  }
}

static inline mongory_matcher *
mongory_matcher_array_record_right_delegate(mongory_memory_pool *pool,
                                            mongory_value *condition) {
  switch (condition->type) {
  case MONGORY_TYPE_ARRAY:
    return mongory_matcher_equal_new(pool, condition);
  default:
    return NULL;
  }
}

static inline bool mongory_matcher_array_record_match(mongory_matcher *matcher,
                                                      mongory_value *value) {
  if (value == NULL) {
    return false;
  }
  if (value->type != MONGORY_TYPE_ARRAY) {
    return false;
  }

  return mongory_matcher_or_match(matcher, value);
}

mongory_matcher *mongory_matcher_array_record_new(mongory_memory_pool *pool,
                                                  mongory_value *condition) {
  mongory_matcher *left =
      mongory_matcher_array_record_left_delegate(pool, condition);
  mongory_matcher *right =
      mongory_matcher_array_record_right_delegate(pool, condition);
  if (right == NULL) {
    return left;
  }
  mongory_composite_matcher *composite =
      mongory_matcher_composite_new(pool, condition);
  composite->left = left;
  composite->right = right;
  composite->base.match = mongory_matcher_array_record_match;
  composite->base.context.original_match = mongory_matcher_array_record_match;
  return (mongory_matcher *)composite;
}
