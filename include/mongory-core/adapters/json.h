#ifndef MONGORY_ADAPTERS_JSON_H
#define MONGORY_ADAPTERS_JSON_H

/**
 * @file json.h
 * @brief Optional raw JSON adapter for mongory-core (enabled via -DMONGORY_WITH_JSON=ON).
 *
 * Provides lazy, shallow-wrapped matching directly over raw UTF-8 JSON bytes
 * backed by yyjson, avoiding full DOM-to-mongory_table materialization.
 */

#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/matchers/matcher.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Compiles a mongory_matcher directly from a raw JSON condition string.
 *
 * Parses `json_str` (`len` bytes) and builds a matcher in `pool`.
 * On parse or compile failure, sets `pool->error` and returns NULL.
 */
mongory_matcher *mongory_matcher_new_from_json(mongory_memory_pool *pool,
                                               const char *json_str,
                                               size_t len,
                                               void *extern_ctx);

/**
 * @brief Matches a raw UTF-8 JSON document string directly against `matcher`.
 *
 * Uses a lazy shallow wrapper so only keys/elements actually accessed by
 * `matcher` are wrapped on demand. Safe to call concurrently from multiple
 * threads on a shared `matcher`.
 *
 * @param matcher Compiled matcher (must be non-NULL).
 * @param json_str Pointer to UTF-8 JSON bytes (NUL-termination not required).
 * @param len Byte length of `json_str`.
 * @return true if `json_str` is valid JSON and satisfies `matcher`, false otherwise.
 */
bool mongory_matcher_match_json(mongory_matcher *matcher, const char *json_str, size_t len);

/**
 * @brief Reusable scratch context for high-throughput batch JSON filtering.
 *
 * Reuses a single internal scratch memory pool and yyjson buffer across calls
 * to avoid per-record heap allocations. Create one context per thread.
 */
typedef struct mongory_json_match_ctx mongory_json_match_ctx;

mongory_json_match_ctx *mongory_json_match_ctx_new(void);
void mongory_json_match_ctx_free(mongory_json_match_ctx *ctx);

/**
 * @brief Matches a raw UTF-8 JSON document using a reusable `ctx`.
 */
bool mongory_matcher_match_json_with_ctx(mongory_matcher *matcher,
                                         mongory_json_match_ctx *ctx,
                                         const char *json_str,
                                         size_t len);

#endif /* MONGORY_ADAPTERS_JSON_H */
