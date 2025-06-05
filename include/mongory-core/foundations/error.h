#ifndef MONGORY_ERROR
#define MONGORY_ERROR

#define MONGORY_ERROR_TYPE_MAGIC 107
#define MONGORY_ERROR_TYPE_MACRO(_) \
    _(MONGORY_ERROR_NONE, 10, "No Error") \
    _(MONGORY_ERROR_MEMORY, 11, "Memory Allocation Error") \
    _(MONGORY_ERROR_INVALID_TYPE, 12, "Invalid Type Error") \
    _(MONGORY_ERROR_OUT_OF_BOUNDS, 13, "Out of Bounds Error") \
    _(MONGORY_ERROR_UNSUPPORTED_OPERATION, 14, "Unsupported Operation Error") \
    _(MONGORY_ERROR_INVALID_ARGUMENT, 15, "Invalid Argument Error") \
    _(MONGORY_ERROR_IO, 16, "I/O Error") \
    _(MONGORY_ERROR_PARSE, 17, "Parse Error") \
    _(MONGORY_ERROR_UNKNOWN, 99, "Unknown Error")

typedef enum mongory_error_type {
#define DEFINE_ERROR_ENUM(name, num, str) name = num * MONGORY_ERROR_TYPE_MAGIC,
    MONGORY_ERROR_TYPE_MACRO(DEFINE_ERROR_ENUM)
#undef DEFINE_ERROR_ENUM
} mongory_error_type;

const char* mongory_error_type_to_string(enum mongory_error_type type);

typedef struct mongory_error {
    mongory_error_type type;
    const char *message;
} mongory_error;

#endif
