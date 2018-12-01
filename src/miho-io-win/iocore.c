#include "iointernals.h"


EXC_DECL_X(Error, IOError);


struct m_io *m_io_open()
{
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (iocp == NULL) {
		exc_IOError_raiseLastError(exc_IOError, "Failed to create IOCP");
        return NULL;
    }

    struct m_io *io = malloc(sizeof(*io));
    io->iocp = iocp;
	io->cq = NULL;
	io->cq_tail = NULL;
    return io;
}

void m_io_close(struct m_io *io)
{
    for (
        struct m_io_close_queue *node = io->cq;
        node != NULL;
        node = node->next
    ) {
        node->callback(node->userdata);
    }

    CloseHandle(io->iocp);
    free(io);
}

int m_io_step(struct m_io *io, int timeout)
{
    if (timeout < 0) {
        timeout = INFINITE;
    }

    OVERLAPPED_ENTRY ol_ents[8];
    int pop_count;

    while (
        GetQueuedCompletionStatusEx(
            io->iocp, ol_ents, 8, &pop_count, timeout, FALSE
        )
    ) {
        for (int i = 0; i < pop_count; i += 1) {
			struct m_io_oper *op = (struct m_io_oper *) ol_ents[i].lpOverlapped;
			op->on_complete(op, ol_ents[i].dwNumberOfBytesTransferred);
        }
    }

    switch (GetLastError()) {
        case WAIT_TIMEOUT:
        case WAIT_IO_COMPLETION:
            return 1;
        default:
			exc_IOError_raiseLastError(exc_IOError, "Failed to get IOCP completion");
            return 0;
    }
}


void _m_io_close_enqueue(
    struct m_io *io, m_cb_close callback, void *userdata
)
{
    struct m_io_close_queue *node = malloc(sizeof(*node));
    node->userdata = userdata;
    node->callback = callback;
    node->next = NULL;

    if (io->cq == NULL) {
        io->cq = node;
    } else {
        io->cq_tail->next = node;
    }

    io->cq_tail = node;
}

int _m_iocp_add(struct m_io *io, HANDLE handle)
{
    HANDLE result = CreateIoCompletionPort(handle, io->iocp, (ULONG_PTR) NULL, 0);
    if (result == NULL) {
		exc_IOError_raiseLastError(exc_IOError, "Failed to add handle to IOCP");
        return 0;
    }

    return 1;
}


void exc_IOError_raiseLastError(exc_type_t exc_type, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	size_t buflen = strlen(fmt) + 128;
	char *buffer = malloc(buflen);
	int x = vsnprintf(buffer, buflen, fmt, va);
	va_end(va);

	LPVOID lpMsgBuf;
	DWORD errcode = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL
	);

	static const char *const finalfmt = "%s (from system error %d: %s)";
	buflen = strlen(buffer) + strlen(finalfmt) + strlen(lpMsgBuf);
	char *finalbuffer = malloc(buflen);
	x = snprintf(finalbuffer, buflen, finalfmt, buffer, errcode, lpMsgBuf);

	free(buffer);
	LocalFree(lpMsgBuf);

	exc_raise(exc_type, (exc_value_t) finalbuffer);
}
