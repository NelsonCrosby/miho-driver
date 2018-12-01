#ifndef MIHO_H_EXC
#define MIHO_H_EXC

#include <stdlib.h>


typedef void *exc_value_t;
typedef struct exc_type {
    struct exc_type *exc_parent;
    const char *exc_name;
    void (*exc_clear)(exc_value_t exc_value);
    const char *(*exc_get_message)(exc_value_t exc_value);
} *exc_type_t;

#define _exc_ID(name, id) exc_ ## name ## _ ## id

#ifdef _MSC_VER
#define EXC_TYPE_(parent, name, ...) \
    struct exc_type _exc_ID(name, type) = \
    { parent, #name, __VA_ARGS__ }
#else
#define EXC_TYPE_(parent, name, ...) \
    struct exc_type _exc_ID(name, type) = \
    { parent, #name __VA_OPT__(,) __VA_ARGS__ }
#endif

#define EXC_TYPE(parent, name) \
    EXC_TYPE_(parent, name, \
        _exc_ID(name, clear), \
        _exc_ID(name, get_message))
#define EXC_TYPE_X(parent, name) \
    EXC_TYPE_(&_exc_ID(parent, type), name, NULL, NULL)
#define EXC_HEADER(name) \
    extern struct exc_type _exc_ID(name, type); \
    extern const exc_type_t exc_ ## name
#define EXC_DEFN(name) \
    extern const exc_type_t exc_ ## name = &_exc_ID(name, type)
#define EXC_DECL(parent, name) \
    extern EXC_TYPE(parent, name); \
    EXC_DEFN(name)
#define EXC_DECL_X(parent, name) \
    extern EXC_TYPE_X(parent, name); \
    EXC_DEFN(name)

// Root of error hierarchy.
/* EXC_DECL((exc_type_t)0, Error); */
EXC_HEADER(Error);

// Used for validation - an input that should probably
// be controlled by the caller was invalid. By default,
// causes the program to exit immediately.
/* EXC_DECL_X(exc_Error, CallerError) */
EXC_HEADER(CallerError);

// Used for cases when a particular functionality or
// set of inputs are not implemented, but are expected
// to be before a stable release. Should never be rasied
// in stable/production code.
/* EXC_DECL_X(exc_Error, TodoError) */
EXC_HEADER(TodoError);


extern exc_type_t exc_active_type;
extern exc_value_t exc_active_value;

struct exc_cause_entry {
    struct exc_cause_entry *cause_cause;
    exc_type_t cause_type;
    exc_value_t cause_value;
} *exc_active_cause;


#define exc_active_name() \
    exc_active_type ? exc_active_type->exc_name : NULL

inline int exc_is(exc_type_t exc_check_type)
{
    for (
        exc_type_t type = exc_active_type;
        type != exc_Error->exc_parent;
        type = type->exc_parent
        ) {
        if (type == exc_check_type) {
            return 1;
        }
    }

    return 0;
}

inline void exc_raise(exc_type_t exc_type, exc_value_t exc_value)
{
    if (exc_active_type) {
        struct exc_cause_entry *cause = malloc(sizeof(*cause));
        cause->cause_cause = exc_active_cause;
        cause->cause_type = exc_active_type;
        cause->cause_value = exc_active_value;
        exc_active_cause = cause;
    }

    exc_active_type = exc_type;
    exc_active_value = exc_value;
}

inline void exc_clear()
{
    if (exc_active_type) {
        for (
            exc_type_t type = exc_active_type;
            type != NULL;
            type = type->exc_parent
        ) {
            if (type->exc_clear != NULL) {
                type->exc_clear(exc_active_value);
                break;
            }
        }

        if (exc_active_cause) {
            struct exc_cause_entry *cause = exc_active_cause;
            exc_active_cause = cause->cause_cause;
            exc_active_type = cause->cause_type;
            exc_active_value = cause->cause_value;
            free(cause);
        } else {
            exc_active_type = exc_Error->exc_parent;
            exc_active_value = (exc_value_t)0;
        }
    }
}

inline const char *exc_active_message()
{
    if (exc_active_type) {
        for (
            exc_type_t type = exc_active_type;
            type != NULL;
            type = type->exc_parent
        ) {
            if (type->exc_get_message != NULL) {
                return type->exc_get_message(exc_active_value);
            }
        }
    }

    return NULL;
}

void exc_Error_raise(exc_type_t exc_type, const char *fmt, ...);

#endif // include guard
