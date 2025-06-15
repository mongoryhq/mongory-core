
#include <mongory-core.h>
#include "base_matcher.h"
#include "literal_matcher.h"
#include "mongory-core/foundations/value.h"
#include "../foundations/config_private.h"
#include "../foundations/iterable.h"
#include "composite_matcher.h"
#include "array_record_matcher.h"
#include "regex_matcher.h"
#include "compare_matcher.h"
#include "composite_matcher.h"
#include "existance_matcher.h"

bool mongory_matcher_literal_match(mongory_matcher *matcher, mongory_value *value) {
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  if (value && value->type == MONGORY_TYPE_ARRAY) {
    if (composite->right == NULL) {
      composite->right = mongory_matcher_array_record_new(matcher->pool, matcher->condition);
    }
    return composite->right->match(composite->right, value);
  } else {
    return composite->left->match(composite->left, value);
  }
}

mongory_matcher* mongory_matcher_null_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_composite_matcher *composite = mongory_matcher_composite_new(pool, condition);
  composite->left = mongory_matcher_equal_new(pool, mongory_value_wrap_n(pool, NULL));
  composite->right = mongory_matcher_exists_new(pool, mongory_value_wrap_b(pool, false));
  composite->base.match = mongory_matcher_or_match;
  composite->base.condition = condition;
  composite->base.context.original_match = mongory_matcher_or_match;
  return (mongory_matcher *)composite;
}

mongory_matcher* mongory_matcher_literal_delegate(mongory_memory_pool *pool, mongory_value *condition) {
  switch (condition->type) {
    case MONGORY_TYPE_TABLE:
      return mongory_matcher_table_cond_new(pool, condition);
    case MONGORY_TYPE_REGEX:
      return mongory_matcher_regex_new(pool, condition);
    case MONGORY_TYPE_NULL:
      return mongory_matcher_null_new(pool, condition);
    default:
      return mongory_matcher_equal_new(pool, condition);
  }
}

typedef struct mongory_field_matcher {
  mongory_composite_matcher composite;
  char *field;
} mongory_field_matcher;

bool mongory_matcher_field_match(mongory_matcher *matcher, mongory_value *value) {
  if (value == NULL) return false;
  mongory_field_matcher *field_matcher = (mongory_field_matcher *)matcher;
  mongory_value *field_value = NULL;
  char *field = field_matcher->field;
  if (value->type == MONGORY_TYPE_TABLE) {
    mongory_table *table = value->data.t;
    field_value = table->get(table, field);
  } else if (value->type == MONGORY_TYPE_ARRAY) {
    int index;
    mongory_array *array = value->data.a;
    if (!try_parse_int(field, &index)) return false;
    if (index < 0) {
      mongory_iterable *iterable = (mongory_iterable *)array->base;
      index = iterable->count + index;
    }
    field_value = array->get(array, (size_t)index);
  } else {
    return false;
  }

  if (field_value && field_value->type == MONGORY_TYPE_POINTER) {
    field_value = mongory_internal_value_converter->shallow_convert(field_value->pool, field_value->data.ptr);
  }

  return mongory_matcher_literal_match(matcher, field_value);
}

mongory_matcher* mongory_matcher_field_new(mongory_memory_pool *pool, char *field, mongory_value *condition) {
  mongory_field_matcher *field_matcher = pool->alloc(pool->ctx, sizeof(mongory_field_matcher));
  if (field_matcher == NULL) {
    return NULL;
  }
  field_matcher->field = field;

  field_matcher->composite.base.context.original_match = mongory_matcher_field_match;
  field_matcher->composite.base.match = mongory_matcher_field_match;
  field_matcher->composite.base.condition = condition;
  field_matcher->composite.base.pool = pool;
  field_matcher->composite.left = mongory_matcher_literal_delegate(pool, condition);
  field_matcher->composite.right = NULL;
  return (mongory_matcher *)field_matcher;
}

bool mongory_matcher_not_match(mongory_matcher *matcher, mongory_value *value) {
  return !mongory_matcher_literal_match(matcher, value);
}

mongory_matcher* mongory_matcher_not_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_composite_matcher *composite = mongory_matcher_composite_new(pool, condition);
  composite->left = mongory_matcher_literal_delegate(pool, condition);
  composite->right = NULL;
  composite->base.match = mongory_matcher_not_match;
  composite->base.context.original_match = mongory_matcher_not_match;
  return (mongory_matcher *)composite;
}

bool mongory_matcher_size_match(mongory_matcher *matcher, mongory_value *value) {
  if (value->type != MONGORY_TYPE_ARRAY) return false;
  mongory_array *array = value->data.a;
  mongory_iterable *iterable = (mongory_iterable *)array->base;
  mongory_value *size = mongory_value_wrap_i(value->pool, iterable->count);
  return mongory_matcher_literal_match(matcher, size);
}

mongory_matcher* mongory_matcher_size_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_composite_matcher *composite = mongory_matcher_composite_new(pool, condition);
  composite->left = mongory_matcher_literal_delegate(pool, condition);
  composite->right = NULL;
  composite->base.match = mongory_matcher_size_match;
  composite->base.context.original_match = mongory_matcher_size_match;
  return (mongory_matcher *)composite;
}
