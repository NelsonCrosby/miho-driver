#ifndef MIHO_H_INTERNAL
#define MIHO_H_INTERNAL

#include <cbor.h>

#include <miho.h>
#include "exceptions.h"


enum miho_error_codes {
    MIHO_ERRCODE_MESSAGE = -2,
    MIHO_ERRCODE_CBOR_PARSE = -1,
    MIHO_ERRCODE_EXCEPTION = 0,
    MIHO_ERRCODE_COMMAND = 1,
    MIHO_ERRCODE_ARGUMENTS = 2,
    MIHO_ERRCODE_VALUES = 3,
};

struct m_subsys_call {
    miho_t miho;
    struct miho_client *client;
    unsigned subsystem, function;
    void *userdata;

    uint8_t *arg_buffer;
    CborParser arg_parser;
    CborValue arg_parser_it;
    CborValue arg_iter;
    unsigned arg_count;

    unsigned result_count;
    uint8_t *results_buffer;
    CborEncoder results;
    enum { M_RETURN_NOTHING = -1, M_RETURN_OK = 0, M_RETURN_ERROR = 1 }
        result_return;
};

typedef void (*m_subsys_func)(struct m_subsys_call *call);

struct m_subsys_function {
    const char *func_name;
    m_subsys_func func_call;
};

struct m_subsys {
    const char *subsys_name;
    size_t func_count, func_name_len_total;
    struct m_subsys_function *funcs;
    void *func_userdata;
};

struct miho {
    enum miho_status status;

    miho_update_callback_t update_callback;
    void *update_userdata;

    struct m_io *io;
    struct m_sock *sock;

    int port;
    int clients;

    size_t subsys_count, subsys_max, subsys_name_len_total;
    struct m_subsys *subsystems;
};


int m_subsys_register(
    miho_t miho, const char *name,
    struct m_subsys_function *funcs, void *func_userdata
);

void m_subsys_return(struct m_subsys_call *call);

extern struct m_subsys_function m_core_funcs[];
#ifdef MIHO_SUBSYSTEM_MOUSE
extern struct m_subsys_function m_mouse_funcs[];
#endif

inline void m_subsys_error(
    struct m_subsys_call *call, int err_code,
    const char *err_msg, size_t err_msg_len
)
{
    if (err_msg_len == 0 && err_msg != NULL) {
        err_msg_len = strlen(err_msg);
    }
    uint8_t *results_buffer = call->results_buffer = malloc(8 + err_msg_len);
    call->result_count = 2;
    cbor_encoder_init(&call->results, results_buffer, 8 + err_msg_len, 0);
    cbor_encode_int(&call->results, err_code);
    cbor_encode_text_string(&call->results, err_msg, err_msg_len);

    call->result_return = M_RETURN_ERROR;
    m_subsys_return(call);

    free(results_buffer);
}

inline int m_subsys_error_arg_count(
    struct m_subsys_call *call, unsigned expect
)
{
    static const char errmsg_few[] = "too few arguments";
    static const char errmsg_many[] = "too many arguments";
    if (call->arg_count == expect) return 0;

    int few = call->arg_count < expect;
    size_t strl = few ? strlen(errmsg_few) : strlen(errmsg_many);

    m_subsys_error(call, MIHO_ERRCODE_ARGUMENTS,
                   few ? errmsg_few : errmsg_many, strl);
    return 1;
}


#define MIHO_CLIENT_BUFFER_SIZE 1024
struct miho_client {
    struct miho *miho;
    struct m_client *mclient;
    int closing, pending;
    size_t rd_buffer_pos;
    uint8_t rd_buffer[MIHO_CLIENT_BUFFER_SIZE];
};

#endif // include guard
