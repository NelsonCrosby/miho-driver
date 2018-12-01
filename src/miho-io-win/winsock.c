#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <ws2def.h>
#include <stdlib.h>

#include "iointernals.h"

#pragma comment(lib, "Ws2_32.lib")


EXC_DECL_X(IOError, SocketError);
EXC_DECL_X(IOError, ClientError);


int wsa_init = 0;
WSADATA wsa_data;

static int init()
{
    if (wsa_init) return 1;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0) {
        wsa_init = 1;
        return 1;
    } else {
        return 0;
    }
}


#define M_SOCK_ADDRLEN          (sizeof(SOCKADDR_IN) + 16)
#define M_SOCK_OUTBUF_ADDRS     (M_SOCK_ADDRLEN * 2)
#define M_SOCK_OUTBUF_RECVLEN   (0)
#define M_SOCK_OUTBUF_FULL      (M_SOCK_OUTBUF_RECVLEN + M_SOCK_OUTBUF_ADDRS)

struct m_sock {
    struct m_io *io;
    struct addrinfo *sysaddr;
    SOCKET syssock;
    LPFN_ACCEPTEX acc_ex;
};

struct m_sock_op_accept {
    struct m_io_oper io_header;
    struct m_sock *sock;

    SOCKET syssock;
    m_sock_cb_accept callback;
    void *userdata;

    DWORD recv_len;
    char outbuf[M_SOCK_OUTBUF_FULL];
};

struct m_sock *m_sock_open(struct m_io *io, int port)
{
    if (!init()) {
        exc_IOError_raiseLastError(
            exc_SocketError, "Failed to initialize winsock"
        );
        return NULL;
    }

    char port_str[8];
    _itoa_s(port, port_str, 8, 10);

    struct addrinfo *sysaddr, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port_str, &hints, &sysaddr)) {
        exc_IOError_raiseLastError(
            exc_SocketError, "Failed to configure socket"
        );
        return NULL;
    }

    SOCKET syssock = INVALID_SOCKET;
    syssock = socket(
        sysaddr->ai_family,
        sysaddr->ai_socktype,
        sysaddr->ai_protocol
    );
    if (syssock == INVALID_SOCKET) {
        exc_IOError_raiseLastError(exc_SocketError, "Failed to create socket");
        freeaddrinfo(sysaddr);
        return NULL;
    }

    int result_bind = bind(
        syssock, sysaddr->ai_addr, (int)sysaddr->ai_addrlen
    );
    if (result_bind == SOCKET_ERROR) {
        exc_IOError_raiseLastError(exc_SocketError, "Failed to bind socket");
        freeaddrinfo(sysaddr);
        closesocket(syssock);
        return NULL;
    }

    GUID acc_ex_guid = WSAID_ACCEPTEX;
    LPFN_ACCEPTEX acc_ex_addr;
    int unused;
    int result_ioctl = WSAIoctl(
        syssock, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &acc_ex_guid, sizeof(acc_ex_guid),
        &acc_ex_addr, sizeof(acc_ex_addr),
        &unused, NULL, NULL
    );
    if (result_ioctl == SOCKET_ERROR) {
        exc_IOError_raiseLastError(
            exc_SocketError, "Failed to find WSAAcceptEx"
        );
        freeaddrinfo(sysaddr);
        closesocket(syssock);
        return NULL;
    }

    struct m_sock *sock = malloc(sizeof(*sock));
    sock->io = io;
    sock->sysaddr = sysaddr;
    sock->syssock = syssock;
    sock->acc_ex = acc_ex_addr;
    return sock;
}

void m_sock_close(struct m_sock *sock)
{
    freeaddrinfo(sock->sysaddr);
    closesocket(sock->syssock);
    free(sock);
}

int m_sock_listen(struct m_sock *sock, int backlog)
{
    if (backlog < 0) {
        backlog = SOMAXCONN;
    }

    if (listen(sock->syssock, backlog) == SOCKET_ERROR) {
        exc_IOError_raiseLastError(exc_SocketError, "Failed to listen");
        m_sock_close(sock);
        return 0;
    }

    if (!_m_iocp_add(sock->io, (HANDLE) sock->syssock)) {
        // Exception already raised by _m_iocp_add
        m_sock_close(sock);
        return 0;
    }

    return 1;
}

static struct m_client *m_client_new(
    struct m_sock *sock, SOCKET syssock,
    const char *accept_buf
);

static void m_sock_done_accept(struct m_sock_op_accept *op, int n_bytes)
{
    int result_ol = GetOverlappedResult(
        (HANDLE) op->sock->syssock, &op->io_header.overlapped.basic,
        &n_bytes, FALSE
    );

    struct m_client *client = NULL;
    if (result_ol) {
        client = m_client_new(op->sock, op->syssock, op->outbuf);
    } else {
        exc_IOError_raiseLastError(
            exc_SocketError, "Accept task returned error"
        );
    }
    op->callback(op->userdata, op->sock, client);
}

int m_sock_accept(
    struct m_sock *sock, m_sock_cb_accept callback, void *userdata
)
{
    if (callback == NULL) {
        return 0;
    }

    SOCKET acc_sock = INVALID_SOCKET;
    acc_sock = socket(
        sock->sysaddr->ai_family,
        sock->sysaddr->ai_socktype,
        sock->sysaddr->ai_protocol
    );
    if (acc_sock == INVALID_SOCKET) {
        exc_IOError_raiseLastError(
            exc_SocketError, "Failed to create client socket"
        );
        m_sock_close(sock);
        return 0;
    }

    struct m_sock_op_accept *op = malloc(sizeof(*op));
    ZeroMemory(op, sizeof(op->io_header.overlapped));
    op->io_header.on_complete = (m_io_cb_done) m_sock_done_accept;
    op->sock = sock;
    op->syssock = acc_sock;
    op->callback = callback;
    op->userdata = userdata;
    op->recv_len = 0;

    int result_acc = sock->acc_ex(
        sock->syssock, acc_sock,
        op->outbuf, M_SOCK_OUTBUF_RECVLEN,
        M_SOCK_ADDRLEN, M_SOCK_ADDRLEN,
        &op->recv_len, &op->io_header.overlapped.basic
    );

    if (result_acc == FALSE) {
        if (WSAGetLastError() == WSA_IO_PENDING) {
            // Will be handled async-ly
            return 1;
        } else {
            exc_IOError_raiseLastError(
                exc_SocketError, "Accept call returned error"
            );
            free(op);
            m_sock_close(sock);
            return 0;
        }
    } else {
        // Sync accept
        // m_sock_iocp(sock, accept_op->recv_len, accept_op);
        // Done
        return 1;
    }
}


struct m_client_op {
    struct m_io_oper io_header;
    struct m_client *client;
    struct m_client_op *chain_prev, *chain_next;
};

struct m_client {
    struct m_io *io;
    SOCKET syssock;

    int addr_local_length, addr_remote_length;
    struct sockaddr *addr_local, *addr_remote;
    char sa_buf[M_SOCK_OUTBUF_ADDRS];

    int closing;
    m_client_cb_close close_callback;
    void *close_userdata;

    struct m_client_op *op_chain_head, *op_chain_tail;
};

struct m_client_op_rw {
    struct m_client_op super;

    m_client_cb_rw callback;
    void *userdata;
    DWORD done_len, flags;
    WSABUF buffer;
};

static struct m_client_op *m_client_op_new(
    struct m_client *client, size_t size, m_io_cb_done callback
)
{
    struct m_client_op *op = malloc(size);
    ZeroMemory(op, sizeof(op->io_header.overlapped));
    op->io_header.on_complete = callback;
    op->client = client;
    op->chain_prev = op->chain_next = NULL;
    return op;
}

static void m_client_op_append(struct m_client_op *op)
{
    if (op->client->op_chain_tail == NULL) {
        op->chain_prev = op->chain_next = NULL;
        op->client->op_chain_head = op->client->op_chain_tail = op;
    } else {
        op->chain_next = NULL;
        op->chain_prev = op->client->op_chain_tail;
        op->client->op_chain_tail = op;
        op->chain_prev->chain_next = op;
    }
}

static int m_client_op_clear(struct m_client_op *op)
{
    if (op->chain_prev != NULL)
        op->chain_prev->chain_next = op->chain_next;
    if (op->chain_next != NULL)
        op->chain_next->chain_prev = op->chain_prev;

    if (op->client->op_chain_tail == op)
        op->client->op_chain_tail = op->chain_prev;
    if (op->client->op_chain_head == op)
        op->client->op_chain_head = op->chain_next;

    int do_close =
        op->client->closing &&
        op->client->op_chain_head == NULL &&
        op->client->op_chain_tail == NULL;
    free(op);
    return do_close;
}

static struct m_client *m_client_new(
    struct m_sock *sock, SOCKET syssock,
    const char *accept_buf
)
{
    struct m_client *client = malloc(sizeof(*client));
    client->io = sock->io;
    client->syssock = syssock;
    client->closing = 0;
    client->close_callback = NULL;
    client->close_userdata = NULL;
    client->op_chain_head = client->op_chain_tail = NULL;

    memcpy(
        &client->sa_buf, accept_buf + M_SOCK_OUTBUF_RECVLEN,
        M_SOCK_OUTBUF_ADDRS
    );

    GetAcceptExSockaddrs(
        &client->sa_buf, 0,
        M_SOCK_ADDRLEN, M_SOCK_ADDRLEN,
        &client->addr_local, &client->addr_local_length,
        &client->addr_remote, &client->addr_remote_length
    );

    if (!_m_iocp_add(client->io, (HANDLE) syssock)) {
        // Exception already raised by _m_iocp_add
        m_client_close(client);
        return NULL;
    }

    return client;
}

static void m_client_actually_close(struct m_client *client)
{
    closesocket(client->syssock);
    if (client->close_callback != NULL) {
        client->close_callback(client->close_userdata, client);
    }
    free(client);
}

void m_client_close(struct m_client *client)
{
    shutdown(client->syssock, SD_BOTH);
    client->closing = 1;

    if (client->op_chain_head == NULL) {
        m_client_actually_close(client);
    } else {
        for (
            struct m_client_op *op = client->op_chain_head;
            op != NULL; op = op->chain_next
        ) {
            CancelIoEx(
                (HANDLE) client->syssock,
                &op->io_header.overlapped.basic
            );
        }
    }
}

void m_client_on_close(
    struct m_client *client, m_client_cb_close callback, void *userdata
)
{
    client->close_callback = callback;
    client->close_userdata = userdata;
}

static void m_client_done_rw(struct m_client_op_rw *op, int n_bytes)
{
    int result_ol = GetOverlappedResult(
        (HANDLE) op->super.client->syssock,
        &op->super.io_header.overlapped.basic,
        &n_bytes, FALSE
    );

    if (result_ol) {
        exc_IOError_raiseLastError(exc_ClientError, "R/W task returned error");
    }

    struct m_client *client = op->super.client;
    op->callback(
        op->userdata, client, result_ol, n_bytes,
        op->buffer.len, op->buffer.buf
    );

    if (m_client_op_clear(&op->super)) {
        m_client_actually_close(client);
    }
}

int m_client_read(
    struct m_client *client, size_t buffer_length, void *buffer,
    m_client_cb_rw callback, void *userdata
)
{
    if (callback == NULL) {
        exc_Error_raise(exc_CallerError, "Must specify a callback");
        return 0;
    } else if (client->closing) {
        exc_Error_raise(exc_ClientError, "Read on a closing socket");
        return 0;
    }

    struct m_client_op_rw *op = (struct m_client_op_rw *) m_client_op_new(
        client, sizeof(*op), (m_io_cb_done) m_client_done_rw
    );
    op->callback = callback;
    op->userdata = userdata;
    op->flags = 0;
    op->buffer.len = (ULONG) buffer_length;
    op->buffer.buf = buffer;

    int result_recv = WSARecv(
        client->syssock, &op->buffer, 1,
        &op->done_len, &op->flags,
        &op->super.io_header.overlapped.wsa, NULL
    );

    if (result_recv == SOCKET_ERROR) {
        switch (WSAGetLastError()) {
        case WSA_IO_PENDING:
            break;
        default:
            exc_IOError_raiseLastError(
                exc_ClientError, "Recv call returned error"
            );
            m_client_op_clear(&op->super);
            return 0;
        }
    }

    m_client_op_append(&op->super);
    return 1;
}

int m_client_write(
    struct m_client *client, size_t data_length, void *data,
    m_client_cb_rw callback, void *userdata
)
{
    if (callback == NULL) {
        exc_Error_raise(exc_CallerError, "Must specify a callback");
        return 0;
    } else if (client->closing) {
        exc_Error_raise(exc_ClientError, "Write on a closing socket");
    }

    struct m_client_op_rw *op = (struct m_client_op_rw *) m_client_op_new(
        client, sizeof(*op), (m_io_cb_done) m_client_done_rw
    );
    op->callback = callback;
    op->userdata = userdata;
    op->flags = 0;
    op->buffer.len = (ULONG) data_length;
    op->buffer.buf = data;

    int result_recv = WSASend(
        client->syssock, &op->buffer, 1,
        &op->done_len, op->flags,
        &op->super.io_header.overlapped.wsa, NULL
    );

    if (result_recv == SOCKET_ERROR) {
        switch (WSAGetLastError()) {
        case WSA_IO_PENDING:
            break;
        default:
            exc_IOError_raiseLastError(
                exc_IOError, "Send call returned error"
            );
            m_client_op_clear(&op->super);
            return 0;
        }
    }

    m_client_op_append(&op->super);
    return 1;
}
