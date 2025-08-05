#ifndef MONGORY_MATCHER_EXPLAINABLE_H
#define MONGORY_MATCHER_EXPLAINABLE_H

#include "mongory-core/matchers/matcher.h"
#include <mongory-core.h>

/**
 * @brief Context for explaining a matcher.
 *
 * @param pool The pool to use for the explanation.
 * @param count The count of the matcher.
 * @param total The total number of matchers.
 * @param prefix The prefix to use for the explanation.
 */
typedef struct mongory_matcher_explain_context {
  mongory_memory_pool *pool;
  int count;
  int total;
  char *prefix;
} mongory_matcher_explain_context;

/**
 * @brief Function pointer type for a matcher's explanation logic.
 *
 * @param matcher The matcher to explain.
 * @param ctx The context to use for the explanation.
 */
typedef void (*mongory_matcher_explain_func)(struct mongory_matcher *matcher, mongory_matcher_explain_context *ctx);

/**
 * @brief Explains a matcher.
 * @param matcher The matcher to explain.
 * @param ctx The context to use for the explanation.
 */

char *mongory_matcher_title(struct mongory_matcher *matcher, mongory_memory_pool *pool);

char *mongory_matcher_title_with_field(struct mongory_matcher *matcher, mongory_memory_pool *pool);

void mongory_matcher_base_explain(struct mongory_matcher *matcher, mongory_matcher_explain_context *ctx);

void mongory_matcher_composite_explain(struct mongory_matcher *matcher, mongory_matcher_explain_context *ctx);

void mongory_matcher_traverse_explain(struct mongory_matcher *matcher, mongory_matcher_explain_context *ctx);

void mongory_matcher_field_explain(struct mongory_matcher *matcher, mongory_matcher_explain_context *ctx);

void mongory_matcher_literal_explain(struct mongory_matcher *matcher, mongory_matcher_explain_context *ctx);

#endif /* MONGORY_MATCHER_EXPLAINABLE_H */
