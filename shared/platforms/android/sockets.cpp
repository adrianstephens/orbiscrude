#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "sockets.h"
#include "base/strings.h"

namespace iso {

const unsigned char socket_max_pending = 16;
static fixed_string<256> error;

const char *socket_error() {
	return error;
}
void socket_error(const char *reason, unsigned long code) {
	error.format("%s: 0x%08x", reason, code);
}
void socket_error(const char *reason) {
	socket_error(reason, errno);
}

bool socket_init() {
	return true;
}

bool socket_term() {
	return true;
}

struct socket_time : timeval {
	socket_time(float time) {
		tv_sec	= int(time);
		tv_usec	= int((time - tv_sec) * 1000000);
	}
};

bool socket_select(float time, SOCKET sock) {
	fd_set	fds;
	FD_ZERO(&fds) ;
	FD_SET(sock, &fds) ;
	return select(sock + 1, &fds, NULL, NULL, &socket_time(time)) > 0;
}

int socket_select(float time, SOCKET *socks, size_t nsocks) {
	fd_set	fds;
	FD_ZERO(&fds);
	SOCKET	maxsock = socks[0];
	while (nsocks--) {
		auto	sock = *socks++;
		if (sock > maxsock)
			maxsock = sock;
		FD_SET(sock, &fds);
	}

	return select(maxsock + 1, &fds, NULL, NULL, &socket_time(time));
}

bool socket_announce(const char *target) {
	SOCKET	h = socket_broadcast(1801, 1800, target, strlen(target));
	for (;;) {
		char		buffer[256];
		sockaddr	from;
		socklen_t	fromlen	= sizeof(from);
		int			read	= recvfrom(h, buffer, sizeof(buffer), 0, &from, &fromlen);
		if (read <= 0)
			return false;

		SOCKET	sock2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		((sockaddr_in&)from).sin_port = htons(1801);
		sendto(sock2, target, strlen(target), 0, (sockaddr*)&from, fromlen);
	//	closesocket(sock2);
	}
	return true;
}

bool socket_announce() {
	return socket_announce("android");
}

SOCKET socket_listen(PORT port) {
	// socket
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		socket_error("socket()");
		return INVALID_SOCKET;
	}

	// force rebind
	int reuse_address = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address)) < 0) {
		socket_error("setsockopt()");
		close(server_socket);
		return INVALID_SOCKET;
	}

	// bind
	sockaddr_in addr;
	clear(addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	if (bind(server_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
		socket_error("bind()");
		close(server_socket);
		return INVALID_SOCKET;
	}

	// listen
	if (listen(server_socket, socket_max_pending) < 0) {
		socket_error("listen()");
		close(server_socket);
		return INVALID_SOCKET;
	}

	// accept
	SOCKET client_socket = accept(server_socket, NULL, NULL);
	if (client_socket < 0) {
		socket_error("accept()");
		close(server_socket);
		return INVALID_SOCKET;
	}

	// release
    close(server_socket);
	return client_socket;
}

bool socket_connect(SOCKET sock, void *addr, size_t addr_len) {
	return connect(sock, (struct sockaddr*)addr, (int)addr_len) == 0;
}

SOCKET socket_accept(SOCKET sock, void *addr, size_t *addr_len) {
	int	_addr_len = 0;
	sock = accept(sock, (struct sockaddr*)addr, &_addr_len);
	if (addr_len)
		*addr_len = _addr_len;
	return sock;
}


size_t socket_send(SOCKET sock, const void *buffer, size_t size) {
	int result = send(sock, buffer, size, 0);
	if (result < 0) {
		socket_error("send()");
		return 0;
	}
	return size_t(result);
}

size_t socket_receive(SOCKET sock, void *buffer, size_t size) {
	int	result = recv(sock, buffer, size, 0);
	if (result < 0) {
		socket_error("recv()");
		return 0;
	}
	return size_t(result);
}

bool socket_close(SOCKET sock) {
	if (close(sock) < 0) {
		socket_error("socketclose()");
		return false;
	}
	return true;
}

SOCKET socket_broadcast(PORT port_out, PORT port_in, const void *buffer, size_t size) {
	sockaddr_in	addr;
	clear(addr);
	addr.sin_family			= AF_INET;
	addr.sin_port			= htons(port_out);
	addr.sin_addr.s_addr	= htonl(INADDR_BROADCAST);

	SOCKET	sock			= socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	uint32	broadcast		= 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast)) != 0
	||	sendto(sock, static_cast<const char*>(buffer), size, 0, (sockaddr*)&addr, sizeof(addr)) < 0
	) {
		socket_error("broadcast");
		socket_close(sock);
		return INVALID_SOCKET;
	}

	addr.sin_port			= htons(port_in);
	addr.sin_addr.s_addr	= htonl(INADDR_ANY);
  
	SOCKET	sock2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bind(sock2, (sockaddr*)&addr, sizeof(addr));
	return sock2;
}

} // namespace iso