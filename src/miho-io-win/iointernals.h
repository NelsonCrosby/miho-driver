#ifndef MIHO_H_IO

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdlib.h>
#include <Windows.h>
#include <WinSock2.h>

#include "../miho-internal.h"
#include "../miho-io.h"

struct m_io_oper;

typedef void (*m_cb_close)(void *userdata);
typedef void (*m_io_cb_done)(struct m_io_oper *operation, int n_bytes);

struct m_io_close_queue {
    void *userdata;
    m_cb_close callback;
    struct m_io_close_queue *next;
};

struct m_io_oper {
	union {
		OVERLAPPED basic;
		WSAOVERLAPPED wsa;
	} overlapped;
	m_io_cb_done on_complete;
};

struct m_io {
    HANDLE iocp;
    struct m_io_close_queue *cq, *cq_tail;
};

/* internal operations */
void _m_io_close_enqueue(struct m_io *io, m_cb_close callback, void *userdata);
int _m_iocp_add(struct m_io *io, HANDLE handle);

void exc_IOError_raiseLastError(exc_type_t exc_type, const char *fmt, ...);

#endif // include guard
