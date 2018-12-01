#include <Windows.h>
#include <strsafe.h>
#include <stdio.h>

#include "miho-io.h"


static void on_close(void *unused, struct m_client *client)
{
	printf("Client closed\n");
}


static int on_write_done(
	void *buffstart, struct m_client *client, int ok,
	size_t write_length, size_t buffer_length, void *buffer
)
{
	if (!ok) {
		printf("[%s in on_write_done: !ok] %s\n",
			   exc_active_name(), exc_active_message());
		m_client_close(client);
		return 1;
	}

	printf("wrote %zu of %zu\n", write_length, buffer_length);
	if (write_length < buffer_length) {
		if (!m_client_write(
			client,
			buffer_length - write_length, (char *) buffer + write_length,
			on_write_done, buffstart
		)) {
			printf("[%s in on_write_done: !m_client_write()] %s\n",
				   exc_active_name(), exc_active_message());
			m_client_close(client);
			return 1;
		}

		return 0;
	}

	printf("done\n");
	//free(buffstart);
	m_client_close(client);

	return 0;
}


static void on_accept(void *userdata, struct m_sock *sock, struct m_client *client)
{
	if (client == NULL) {
		printf("[%s in on_accept: client == NULL] %s\n",
			   exc_active_name(), exc_active_message());
		m_sock_close(sock);
		return;
	}

	if (!m_sock_accept(sock, on_accept, NULL)) {
		printf("[%s in on_accept: !m_sock_accept()] %s\n",
			   exc_active_name(), exc_active_message());
		m_client_close(client);
		m_sock_close(sock);
		return;
	}

	m_client_on_close(client, on_close, NULL);

	static const char *data = "hello world\n";

	if (!m_client_write(client, strlen(data), data, on_write_done, data)) {
		printf("[%s in on_accept: !m_client_write()] %s\n",
			   exc_active_name(), exc_active_messge());
		m_client_close(client);
	}
}


int main()
{
    struct m_io *io = m_io_open();
	if (io == NULL) {
		printf("[%s in main: io == NULL] %s\n",
			   exc_active_name(), exc_active_message());
		return 1;
	}

	struct m_sock *sock = m_sock_open(io, 1234);
	if (sock == NULL) {
		printf("[%s in main: sock == NULL] %s\n",
			   exc_active_name(), exc_active_message());
		return 1;
	}

	if (!m_sock_listen(sock, -1)) {
		printf("[%s in main: !m_sock_listen()] %s\n",
			   exc_active_name(), exc_active_message());
		return 1;
	}

	if (!m_sock_accept(sock, on_accept, NULL)) {
		printf("[%s in main: !m_sock_accept()] %s\n",
			   exc_active_name(), exc_active_message());
		return 1;
	}

	while (m_io_step(io, -1)) {}

    m_io_close(io);
}
