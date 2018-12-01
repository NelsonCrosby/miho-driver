#include <miho.h>
#include "internal.h"
#include "m-io.h"


struct miho {
    enum miho_status status;

    miho_update_callback_t update_callback;
    void *update_userdata;

    struct m_io *io;
    struct m_sock *sock;

    int port;
    int clients;
};


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


static void miho_on_close(miho_t miho, struct m_client *client)
{
    miho->clients -= 1;
    _mhupdate(MIHO_UPDATE_DISCONNECT);
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

    m_client_on_close(client, miho_on_close, miho);

    miho->clients += 1;
    _mhupdate(MIHO_UPDATE_CONNECT);

    // TODO: Start reading for commands
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
