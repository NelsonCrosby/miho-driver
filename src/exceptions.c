#include <stdlib.h>
#include <varargs.h>
#include <string.h>
#include <stdio.h>

#include "miho-internal.h"

extern exc_type_t exc_active_type = NULL;
extern exc_value_t exc_active_value = NULL;

static void exc_Error_clear(exc_value_t value);
static const char *exc_Error_get_message(exc_value_t value);

EXC_DECL(NULL, Error);
EXC_DECL_X(Error, CallerError);
EXC_DECL_X(Error, TodoError);

void exc_Error_raise(exc_type_t exc_type, const char *fmt, ...)
{
    va_list va;
#if _WIN32 && !defined(__cplusplus)
    va_start(va);
#else
    va_start(va, fmt);
#endif
    size_t buflen = strlen(fmt) + 128;
    char *buffer = (char *)malloc(buflen);
    int x = vsnprintf(buffer, buflen, fmt, va);
    va_end(va);

    exc_raise(exc_type, (exc_value_t) buffer);
}

static void exc_Error_clear(exc_value_t value)
{
    free(value);
}

static const char *exc_Error_get_message(exc_value_t value)
{
    return (const char *)value;
}