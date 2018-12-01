#ifndef MIHO_H_IO
#define MIHO_H_IO

#include <stddef.h>

#include "miho-internal.h"

#define MIHO_EXT_IO     // Used by other headers to add extension APIs

EXC_HEADER(IOError);
EXC_HEADER(SocketError);
EXC_HEADER(ClientError);

struct m_io;
struct m_sock;
struct m_client;

struct m_io *m_io_open();
void m_io_close(struct m_io *io);
int m_io_step(struct m_io *io, int timeout);

typedef void (*m_sock_cb_accept)(
    void *userdata, struct m_sock *sock, struct m_client *client
);

struct m_sock *m_sock_open(struct m_io *io, int port);
void m_sock_close(struct m_sock *sock);
int m_sock_listen(struct m_sock *sock, int backlog);
int m_sock_accept(
    struct m_sock *sock, m_sock_cb_accept callback, void *userdata
);

typedef int (*m_client_cb_rw)(
    void *userdata, struct m_client *client, int ok,
    size_t ok_length, size_t buffer_length, void *buffer
);
typedef void (*m_client_cb_close)(void *userdata, struct m_client *client);

void m_client_close(struct m_client *client);
void m_client_on_close(
    struct m_client *client, m_client_cb_close callback, void *userdata
);
int m_client_read(
    struct m_client *client, size_t buffer_length, void *buffer,
    m_client_cb_rw callback, void *userdata
);
int m_client_write(
    struct m_client *client, size_t data_length, void *data,
    m_client_cb_rw callback, void *userdata
);

#endif // include guard
