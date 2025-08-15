#include <mongory-core.h>
#include "functional_match.h"
#include "string.h"
#include "array_private.h"
#include "config_private.h"
#include "../matchers/base_matcher.h"

// Forward declarations
bool mongory_functional_condition_match(mongory_value *condition, mongory_value *value);
bool mongory_functional_condition_match_each_cb(char *key, mongory_value *sub_condition, void *ctx);
bool mongory_functional_eq_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_ne_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_gt_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_gte_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_lt_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_lte_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_in_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_nin_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_exists_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_present_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_regex_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_and_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_or_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_elem_match_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_every_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_not_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_size_match(mongory_value *condition, mongory_value *target);
bool mongory_functional_field_match(char *key, mongory_value *condition, mongory_value *target);
bool mongory_functional_literal_match(mongory_value *condition, mongory_value *target);

// Implementation
bool mongory_functional_match(mongory_value *condition, mongory_value *target) {
  return mongory_functional_condition_match(condition, target);
}

bool mongory_functional_condition_match(mongory_value *condition, mongory_value *target) {
  if (condition->type != MONGORY_TYPE_TABLE) {
    return false;
  }
  mongory_table *table = condition->data.t;
  return table->each(table, &target, mongory_functional_condition_match_each_cb);
}

bool mongory_functional_condition_match_each_cb(char *key, mongory_value *sub_condition, void *ctx) {
  mongory_value *target = (mongory_value *)ctx;
  if (strcmp(key, "$eq") == 0) {
    return mongory_functional_eq_match(sub_condition, target);
  }
  if (strcmp(key, "$ne") == 0) {
    return mongory_functional_ne_match(sub_condition, target);
  }
  if (strcmp(key, "$gt") == 0) {
    return mongory_functional_gt_match(sub_condition, target);
  }
  if (strcmp(key, "$gte") == 0) {
    return mongory_functional_gte_match(sub_condition, target);
  }
  if (strcmp(key, "$lt") == 0) {
    return mongory_functional_lt_match(sub_condition, target);
  }
  if (strcmp(key, "$lte") == 0) {
    return mongory_functional_lte_match(sub_condition, target);
  }
  if (strcmp(key, "$in") == 0) {
    return mongory_functional_in_match(sub_condition, target);
  }
  if (strcmp(key, "$nin") == 0) {
    return mongory_functional_nin_match(sub_condition, target);
  }
  if (strcmp(key, "$exists") == 0) {
    return mongory_functional_exists_match(sub_condition, target);
  }
  if (strcmp(key, "$present") == 0) {
    return mongory_functional_present_match(sub_condition, target);
  }
  if (strcmp(key, "$regex") == 0) {
    return mongory_functional_regex_match(sub_condition, target);
  }
  if (strcmp(key, "$and") == 0) {
    return mongory_functional_and_match(sub_condition, target);
  }
  if (strcmp(key, "$or") == 0) {
    return mongory_functional_or_match(sub_condition, target);
  }
  if (strcmp(key, "$elemMatch") == 0) {
    return mongory_functional_elem_match_match(sub_condition, target);
  }
  if (strcmp(key, "$every") == 0) {
    return mongory_functional_every_match(sub_condition, target);
  }
  if (strcmp(key, "$not") == 0) {
    return mongory_functional_not_match(sub_condition, target);
  }
  if (strcmp(key, "$size") == 0) {
    return mongory_functional_size_match(sub_condition, target);
  }
  return mongory_functional_field_match(key, sub_condition, target);
}

bool mongory_functional_eq_match(mongory_value *condition, mongory_value *target) {
  return target->comp(target, condition) == 0;
}

bool mongory_functional_ne_match(mongory_value *condition, mongory_value *target) {
  return target->comp(target, condition) != 0;
}

bool mongory_functional_gt_match(mongory_value *condition, mongory_value *target) {
  return target->comp(target, condition) == 1;
}

bool mongory_functional_gte_match(mongory_value *condition, mongory_value *target) {
  int result = target->comp(target, condition);
  return (result == 0) || (result == 1);
}

bool mongory_functional_lt_match(mongory_value *condition, mongory_value *target) {
  return target->comp(target, condition) == -1;
}

bool mongory_functional_lte_match(mongory_value *condition, mongory_value *target) {
  int result = target->comp(target, condition);
  return (result == 0) || (result == -1);
}

bool mongory_functional_in_match(mongory_value *condition, mongory_value *target) {
  if (condition->type != MONGORY_TYPE_ARRAY) {
    return false;
  }
  mongory_array_private *array = (mongory_array_private *)condition->data.a;
  for (size_t i = 0; i < array->base.count; i++) {
    if (target->comp(target, array->items[i]) == 0) {
      return true;
    }
  }
  return false;
}

bool mongory_functional_nin_match(mongory_value *condition, mongory_value *target) {
  return !mongory_functional_in_match(condition, target);
}

bool mongory_functional_exists_match(mongory_value *condition, mongory_value *target) {
  if (condition->type != MONGORY_TYPE_BOOL) {
    return false;
  }
  return (target != NULL) == (condition->data.b);
}

bool mongory_functional_present_match(mongory_value *condition, mongory_value *target) {
  if (condition->type != MONGORY_TYPE_BOOL) {
    return false;
  }
  return (target->type != MONGORY_TYPE_NULL) == (condition->data.b);
}

bool mongory_functional_regex_match(mongory_value *condition, mongory_value *target) {
  if (target->type != MONGORY_TYPE_STRING) {
    return false;
  }
  return mongory_internal_regex_adapter->match_func(target->pool, condition, target);
}

bool mongory_functional_and_match(mongory_value *condition, mongory_value *target) {
  if (condition->type != MONGORY_TYPE_ARRAY) {
    return false;
  }
  mongory_array_private *array = (mongory_array_private *)condition->data.a;
  for (size_t i = 0; i < array->base.count; i++) {
    if (!mongory_functional_condition_match(array->items[i], target)) {
      return false;
    }
  }
  return true;
}

bool mongory_functional_or_match(mongory_value *condition, mongory_value *target) {
  if (condition->type != MONGORY_TYPE_ARRAY) {
    return false;
  }
  mongory_array_private *array = (mongory_array_private *)condition->data.a;
  for (size_t i = 0; i < array->base.count; i++) {
    if (mongory_functional_match(array->items[i], target)) {
      return true;
    }
  }
  return false;
}

bool mongory_functional_elem_match_match(mongory_value *condition, mongory_value *target) {
  if (target->type != MONGORY_TYPE_ARRAY) {
    return false;
  }
  mongory_array_private *array = (mongory_array_private *)target->data.a;
  for (size_t i = 0; i < array->base.count; i++) {
    if (mongory_functional_condition_match(condition, array->items[i])) {
      return true;
    }
  }
  return false;
}

bool mongory_functional_every_match(mongory_value *condition, mongory_value *target) {
  if (target->type != MONGORY_TYPE_ARRAY) {
    return false;
  }
  mongory_array_private *array = (mongory_array_private *)target->data.a;
  for (size_t i = 0; i < array->base.count; i++) {
    if (!mongory_functional_condition_match(condition, array->items[i])) {
      return false;
    }
  }
  return true;
}

bool mongory_functional_not_match(mongory_value *condition, mongory_value *target) {
  return !mongory_functional_literal_match(condition, target);
}

bool mongory_functional_size_match(mongory_value *condition, mongory_value *target) {
  if (target->type != MONGORY_TYPE_ARRAY) {
    return false;
  }
  mongory_array *array = target->data.a;
  size_t size = array->count;
  switch (condition->type) {
    case MONGORY_TYPE_INT:
      return (size_t)condition->data.i == size;
    case MONGORY_TYPE_TABLE:
      return mongory_functional_literal_match(condition, mongory_value_wrap_i(target->pool, (int)size));
    default:
      return false;
  }
}

bool mongory_functional_field_match(char *key, mongory_value *condition, mongory_value *target) {
  mongory_value *field_value;
  int index;
  switch (target->type) {
    case MONGORY_TYPE_TABLE:
      field_value = target->data.t->get(target->data.t, key);
      break;
    case MONGORY_TYPE_ARRAY:
      if (!mongory_try_parse_int(key, &index))
        return false;
      if (index < 0) {
        if ((size_t)(-index) > target->data.a->count)
          return false;
        index = target->data.a->count + index;
      }
      field_value = target->data.a->get(target->data.a, (size_t)index);
      break;
    default:
      return false;
  }
  return mongory_functional_literal_match(condition, field_value);
}

bool mongory_functional_literal_match(mongory_value *condition, mongory_value *target) {
  if (target->type == MONGORY_TYPE_ARRAY) {
    return mongory_functional_array_record_match(condition, target);
  }  
  switch (condition->type) {
    case MONGORY_TYPE_TABLE:
      return mongory_functional_condition_match(condition, target);
    case MONGORY_TYPE_REGEX:
      return mongory_functional_regex_match(condition, target);
    case MONGORY_TYPE_NULL:
      return target == NULL || target->type == MONGORY_TYPE_NULL;
    default:
      return target->comp(target, condition) == 0;
  }
}

bool mongory_functional_array_record_match(mongory_value *condition, mongory_value *target) {
  if (target->type != MONGORY_TYPE_ARRAY) {
    return false;
  }
  if (condition->type == MONGORY_TYPE_ARRAY && target->comp(target, condition) == 0) {
    return true;
  }

  return false;
}