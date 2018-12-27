#include <stdarg.h>
#include <stdio.h>

#include "internal.h"
#include "m-io.h"


#define _mhupdate(what) \
    miho->update_callback( \
        miho->update_userdata, miho, what, \
        miho->status, miho->port, miho->clients \
    )


miho_t MIHO_EXPORT miho_create(
    int port,
    miho_update_callback_t update_callback,
    void *update_userdata
)
{
    struct m_io *io = m_io_open();
    if (io == NULL) {
        exc_clear();
        return NULL;
    }

    miho_t miho = malloc(sizeof(*miho));
    miho->status = MIHO_STATUS_NEW;
    miho->update_callback = update_callback;
    miho->update_userdata = update_userdata;
    miho->io = io;
    miho->sock = NULL;
    miho->port = port;
    miho->clients = 0;
    miho->subsys_count = 0;
    miho->subsys_max = 0;
    miho->subsys_name_len_total = 0;
    miho->subsystems = NULL;

    m_subsys_register(miho, "core", m_core_funcs, NULL);

    return miho;
}


void MIHO_EXPORT miho_close(miho_t miho)
{
    if (
        miho->status != MIHO_STATUS_NEW &&
        miho->status != MIHO_STATUS_STOPPED
    ) {
        return;
    }

    m_io_close(miho->io);
    miho->io = NULL;

    miho->status = MIHO_STATUS_NULL;
    _mhupdate(MIHO_UPDATE_STATUS);

    free(miho);
}


inline void miho_client_close(struct miho_client *client)
{
    if (client->pending > 0) {
        client->closing = 1;
    } else {
        m_client_close(client->mclient);
    }
}


static void miho_on_close(struct miho_client *client, struct m_client *mclient)
{
    miho_t miho = client->miho;
    miho->clients -= 1;
    _mhupdate(MIHO_UPDATE_DISCONNECT);

    free(client);
}


static int miho_on_read(
    struct miho_client *client, struct m_client *mclient, int ok,
    size_t ok_length, size_t buffer_length, void *buffer
)
{
    if (client->closing) {
        miho_client_close(client);
        return 0;
    }

    if (!ok || ok_length == 0) {
        miho_client_close(client);
        return 0;
    }

    client->rd_buffer_pos += ok_length;

    miho_t miho = client->miho;

    struct m_subsys_call *call = malloc(sizeof(*call));
    call->miho = client->miho;
    call->client = client;
    call->result_return = M_RETURN_NOTHING;

    int err_code = 0;
    char *err_msg = NULL;

    if (
        cbor_parser_init(
            client->rd_buffer, client->rd_buffer_pos, 0,
            &call->arg_parser, &call->arg_parser_it
        ) != CborNoError
    ) {
        err_code = MIHO_ERRCODE_EXCEPTION;
        err_msg = "internal error";
        goto cleanup_err_return;
    }

    size_t msglen;
    if (!cbor_value_is_array(&call->arg_parser_it)) {
        err_code = MIHO_ERRCODE_MESSAGE;
        err_msg = "expected an array";
        goto cleanup_err_return;
    }
    switch (cbor_value_get_array_length(&call->arg_parser_it, &msglen)) {
        case CborNoError: break;
        case CborErrorUnknownLength:
            err_code = MIHO_ERRCODE_MESSAGE;
            err_msg = "expected definite length";
            goto cleanup_err_return;
        default:
            err_code = MIHO_ERRCODE_EXCEPTION;
            err_msg = "internal error";
            goto cleanup_err_return;
    }
    if (msglen < 2) {
        err_code = MIHO_ERRCODE_MESSAGE;
        err_msg = "expected at least subsystem and function";
        goto cleanup_err_return;
    }

    // Skip over message to verify and find end
    switch (cbor_value_advance(&call->arg_parser_it)) {
        case CborNoError:
            // Stream is valid; continue normally
            break;
        case CborErrorUnexpectedEOF:
            // Stream is so-far valid, but incomplete; read more
            free(call);
            if (client->rd_buffer_pos < MIHO_CLIENT_BUFFER_SIZE) {
                if (m_client_read(
                    mclient, MIHO_CLIENT_BUFFER_SIZE - client->rd_buffer_pos,
                    client->rd_buffer, miho_on_read, client
                )) {
                    return 1;
                } else {
                    // Error trying to read
                }
            } else {
                // Buffer is full; message too big
            }
            // Encountered an error
            m_client_close(mclient);
            return 0;
        default:
            // Stream has an error; kill client
            err_code = MIHO_ERRCODE_CBOR_PARSE;
            err_msg = "invalid cbor";
            goto cleanup_err_return;
    }

    // Copying occurs to move current message out of the way
    const uint8_t *message_end = cbor_value_get_next_byte(&call->arg_parser_it);
    size_t msgbytes = message_end - client->rd_buffer;
    uint8_t *arg_buffer = malloc(msgbytes);
    memcpy(arg_buffer, message_end, msgbytes);
    call->arg_buffer = arg_buffer;

    if (
        cbor_parser_init(
            client->rd_buffer, client->rd_buffer_pos, 0,
            &call->arg_parser, &call->arg_parser_it
        ) != CborNoError
    ) {
        err_code = MIHO_ERRCODE_EXCEPTION;
        err_msg = "internal error";
        goto cleanup_err_return;
    }

    // Dispatch command
    if (
        cbor_value_enter_container(&call->arg_parser_it, &call->arg_iter)
        != CborNoError
    ) {
        call->subsystem = (unsigned) -1;
        err_code = MIHO_ERRCODE_ARGUMENTS;
        err_msg = "internal error?";
        goto cleanup_err_return;
    }
    if (!cbor_value_is_unsigned_integer(&call->arg_iter)) {
        call->subsystem = (unsigned) -1;
        err_code = MIHO_ERRCODE_MESSAGE;
        err_msg = "message subsystem (0) must be of kind integer";
        goto cleanup_err_return;
    }
    if (cbor_value_get_int(&call->arg_iter, &call->subsystem) != CborNoError) {
        call->subsystem = (unsigned) -1;
        err_code = MIHO_ERRCODE_EXCEPTION;
        err_msg = "internal error";
        goto cleanup_err_return;
    }
    cbor_value_advance_fixed(&call->arg_iter);
    if (!cbor_value_is_unsigned_integer(&call->arg_iter)) {
        err_code = MIHO_ERRCODE_MESSAGE;
        err_msg = "message function (1) must be of kind integer";
        goto cleanup_err_return;
    }
    if (cbor_value_get_int(&call->arg_iter, &call->function) != CborNoError) {
        err_code = MIHO_ERRCODE_EXCEPTION;
        err_msg = "internal error";
        goto cleanup_err_return;
    }
    cbor_value_advance_fixed(&call->arg_iter);

    if (call->subsystem >= miho->subsys_count) {
        err_code = MIHO_ERRCODE_MESSAGE;
        err_msg = "no such subsystem";
        goto cleanup_err_return;
    }
    struct m_subsys *subsystem = &miho->subsystems[call->subsystem];

    if (call->function >= subsystem->func_count) {
        err_code = MIHO_ERRCODE_MESSAGE;
        err_msg = "no such function";
        goto cleanup_err_return;
    }
    struct m_subsys_function *function = &subsystem->funcs[call->function];

    if (function->func_call == NULL) {
        err_code = MIHO_ERRCODE_MESSAGE;
        err_msg = "no such function";
        goto cleanup_err_return;
    }

    call->arg_count = (unsigned)msglen - 2;
    call->userdata = subsystem->func_userdata;
    function->func_call(call);

    if (msgbytes < client->rd_buffer_pos) {
        // There's another message starting here
        size_t remaining = client->rd_buffer_pos - msgbytes;
        memmove(client->rd_buffer, client->rd_buffer + msgbytes, remaining);
        client->rd_buffer_pos = remaining;
        // Recurse on the next message in the buffer
        return miho_on_read(
            client, mclient, ok,
            remaining, MIHO_CLIENT_BUFFER_SIZE, client->rd_buffer
        );
    } else if (client->closing) {
        // Don't keep trying to read
        return 1;
    } else {
        // No more messages received yet; wait for next one
        client->rd_buffer_pos = 0;
        if (m_client_read(
            mclient, MIHO_CLIENT_BUFFER_SIZE, client->rd_buffer,
            miho_on_read, client
        )) {
            return 1;
        } else {
            goto cleanup_err;
        }
    }

cleanup_err_return:
    m_subsys_error(call, err_code, err_msg, 0);

cleanup_err:
    miho_client_close(client);
    return 0;
}


static void miho_on_accept(
    miho_t miho, struct m_sock *sock, struct m_client *client
)
{
    if (client == NULL) {
        exc_clear();
        miho_stop(miho);
        return;
    }

    struct miho_client *iclient = malloc(sizeof(*iclient));
    iclient->miho = miho;
    iclient->mclient = client;
    iclient->closing = 0;
    iclient->pending = 0;
    iclient->rd_buffer_pos = 0;

    m_client_on_close(client, miho_on_close, iclient);

    miho->clients += 1;
    _mhupdate(MIHO_UPDATE_CONNECT);

    if (!m_client_read(client, MIHO_CLIENT_BUFFER_SIZE, iclient->rd_buffer,
                       miho_on_read, iclient)) {
        m_client_close(client);
    }

    if (!m_sock_accept(sock, miho_on_accept, miho)) {
        miho_stop(miho);
    }
}


enum miho_result MIHO_EXPORT miho_start(miho_t miho)
{
    if (
        miho->status != MIHO_STATUS_NEW &&
        miho->status != MIHO_STATUS_STOPPED
    ) {
        return MIHO_RESULT_WRONG_STATE;
    }

    miho->sock = m_sock_open(miho->io, miho->port);
    if (miho->sock == NULL) {
        exc_clear();
        return MIHO_RESULT_PROBLEM;
    }

    if (!m_sock_listen(miho->sock, -1)) {
        exc_clear();
        return MIHO_RESULT_PROBLEM;
    }

    if (!m_sock_accept(miho->sock, miho_on_accept, miho)) {
        m_sock_close(miho->sock);
        exc_clear();
        return MIHO_RESULT_PROBLEM;
    }

    miho->status = MIHO_STATUS_STARTED;
    _mhupdate(MIHO_UPDATE_STATUS);
    return MIHO_RESULT_OK;
}


enum miho_result MIHO_EXPORT miho_stop(miho_t miho)
{
    if (miho->status != MIHO_STATUS_STARTED) {
        return MIHO_RESULT_WRONG_STATE;
    }

    m_sock_close(miho->sock);
    miho->sock = NULL;
    
    // TODO: UM HOW

    miho->status = MIHO_STATUS_STOPPED;
    _mhupdate(MIHO_UPDATE_STATUS);
    return MIHO_RESULT_OK;
}


enum miho_result MIHO_EXPORT miho_step(miho_t miho, int timeout)
{
    if (!m_io_step(miho->io, timeout)) {
        exc_clear();
        return MIHO_RESULT_PROBLEM;
    }

    return MIHO_RESULT_OK;
}


enum miho_result MIHO_EXPORT miho_run(miho_t miho)
{
    // TODO: How to stop this gracefully?
    while (m_io_step(miho->io, -1)) {}

    exc_clear();
    return MIHO_RESULT_PROBLEM;
}
