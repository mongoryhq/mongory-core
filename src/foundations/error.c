#include <mongory-core/foundations/error.h>

const char* mongory_error_type_to_string(enum mongory_error_type type) {
    switch (type) {
        #define DEFINE_ERROR_STRING(name, num, str) case name: return str;
        MONGORY_ERROR_TYPE_MACRO(DEFINE_ERROR_STRING)
        #undef DEFINE_ERROR_STRING
        default: return "Unknown Error Type";
    }
}
