#ifndef MONGORY_FOUNDATIONS_CONFIG_PRIVATE_H
#define MONGORY_FOUNDATIONS_CONFIG_PRIVATE_H

typedef struct mongory_regex_adapter {
  mongory_regex_func func;
} mongory_regex_adapter;

extern mongory_regex_adapter *mongory_internal_regex_adapter;

void mongory_matcher_register(char *name, mongory_matcher_build_func build_func);
mongory_matcher_build_func mongory_matcher_build_func_get(char *name);

#endif
