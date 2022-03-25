#include <ws2tcpip.h>
#include "base/strings.h"
#include "sockets.h"

#pragma comment (lib, "ws2_32")

namespace iso {

const unsigned char socket_max_pending = 16;
static fixed_string<256> error;

const char* socket_error() {
	return error;
}
void socket_error(const char *reason, const char *msg) {
	error.clear().format("%s: %s", reason, msg);
}
void socket_error(const char *reason, unsigned long code) {
	error.clear().format("%s: 0x%08x", reason, code);
}
void socket_error_win(const char* reason, DWORD result) {
	char			*msg;
	if (::FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		result,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&msg,
		0, NULL
	)) {
		socket_error(reason, msg);
		::LocalFree(msg);
	} else {
		socket_error(reason, result);
	}
}
void socket_error(const char* reason) {
	socket_error_win(reason, WSAGetLastError());
}

bool socket_init() {
	WSADATA wsa_data;
	if (int result = WSAStartup(WINSOCK_VERSION, &wsa_data)) {
		socket_error("WSAStartup()", result);
		return false;
	}
	return true;
}

bool socket_term() {
	if (WSACleanup() == SOCKET_ERROR) {
		socket_error("WSACleanup()");
		return false;
	}
	return true;
}

SOCKET socket_create(int family, int type, int protocol) {
	return socket(family, type, protocol);
}

bool socket_connect(SOCKET sock, void *addr, size_t addr_len) {
	return connect(sock, (struct sockaddr*)addr, (int)addr_len) == 0;
}

SOCKET socket_accept(SOCKET sock, void *addr, size_t *addr_len) {
	int	_addr_len = 0;
	sock = accept(sock, (struct sockaddr*)addr, addr_len ? &_addr_len : 0);
	if (addr_len)
		*addr_len = _addr_len;
	return sock;
}

bool socket_setopt(SOCKET sock, int level, int option, const void *value, size_t size) {
	return setsockopt(sock, level, option, (const char*)value, (int)size) == 0;
}

int socket_getopt(SOCKET sock, int level, int option, void *value, size_t size) {
	int	isize = int(size);
	return getsockopt(sock, level, option, (char*)value, &isize) == 0 ? isize : 0;
}

struct socket_time : timeval {
	socket_time(float time) {
		tv_sec	= int(time);
		tv_usec	= int((time - tv_sec) * 1000000);
	}
	operator const timeval*() const { return this; }
};

//wait forever
bool socket_select(SOCKET sock) {
	fd_set	fds = {1, sock};
	return select(0, &fds, NULL, NULL, NULL) > 0;
}
int socket_select(SOCKET *socks, size_t nsocks) {
	fd_set	fds;
	fds.fd_count = nsocks;
	copy_n(socks, fds.fd_array, nsocks);
	return select(0, &fds, NULL, NULL, NULL);
}

bool socket_select(float time, SOCKET sock) {
	fd_set	fds = {1, sock};
	return select(0, &fds, NULL, NULL, socket_time(time)) > 0;
}

int socket_select(float time, SOCKET *socks, size_t nsocks) {
	fd_set	fds;
	fds.fd_count = nsocks;
	copy_n(socks, fds.fd_array, nsocks);
	return select(0, &fds, NULL, NULL, socket_time(time));
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
		sendto(sock2, target, (int)strlen(target), 0, (sockaddr*)&from, fromlen);
	//	closesocket(sock2);
	}
	return true;
}

bool socket_announce() {
	return socket_announce("pc");
}
#if 0
SOCKET socket_listen(PORT port) {
	// resolve
	addrinfo *address_info;
	addrinfo _address_info;
	memset(&_address_info, 0, sizeof(_address_info));
	_address_info.ai_family = AF_INET;
	_address_info.ai_socktype = SOCK_STREAM;
	_address_info.ai_protocol = IPPROTO_TCP;
	_address_info.ai_flags = AI_PASSIVE;
//	char _port[6];

	if (int result = getaddrinfo("localhost", to_string(port), &_address_info, &address_info)) {
		socket_error("getaddrinfo()", result);
		return INVALID_SOCKET;
	}

	// socket
	SOCKET server_socket = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
	if (server_socket == INVALID_SOCKET) {
		socket_error("socket()");
		freeaddrinfo(address_info);
		return INVALID_SOCKET;
	}

	socket_setopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 1);

	// bind
	if (bind(server_socket, address_info->ai_addr, (int)address_info->ai_addrlen) == SOCKET_ERROR) {
		socket_error("bind()");
		freeaddrinfo(address_info);
		socket_close(server_socket);
		return INVALID_SOCKET;
	}

	// release
	freeaddrinfo(address_info);

	// listen
	if (listen(server_socket, socket_max_pending) == SOCKET_ERROR) {
		socket_error("listen()");
		socket_close(server_socket);
		return INVALID_SOCKET;
	}

	// accept
	SOCKET client_socket = accept(server_socket, NULL, NULL);;
	if (client_socket == INVALID_SOCKET) {
		socket_error("accept()");
		socket_close(server_socket);
		return INVALID_SOCKET;
	}

   // release
    socket_close(server_socket);
	return client_socket;
}
#else

SOCKET socket_listen(PORT port) {
	SOCKET		sock = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr	addr = {AF_INET, {0}};

	if (bind(sock, &addr, sizeof(addr)))
		socket_error("bind()");
	else if (listen(sock, socket_max_pending))
		socket_error("listen()");
	else
		return sock;

	socket_close(sock);
	return INVALID_SOCKET;
}
#endif

size_t socket_send(SOCKET sock, const void *buffer, size_t size) {
	int result = send(sock, static_cast<const char*>(buffer), (int)size, 0);
	if (result == SOCKET_ERROR) {
		socket_error("send()");
		return 0;
	}
	return size_t(result);
}

//size_t socket_send_all(SOCKET sock, const void *buffer, size_t size) {
//	const char *p = static_cast<const char*>(buffer);
//	for (size_t result; size && (result = socket_send(sock, p, size)); size -= result)
//		p += result;
//	return p - static_cast<const char*>(buffer);
//}

size_t socket_receive(SOCKET sock, void *buffer, size_t size) {
	int	result = recv(sock, static_cast<char*>(buffer), (int)size, 0);
	if (result == SOCKET_ERROR) {
		socket_error("recv()");
		return 0;
	}
	return size_t(result);
}

//size_t socket_receive_all(SOCKET sock, void *buffer, size_t size) {
//	char *p = static_cast<char*>(buffer);
//	for (size_t result; size && (result = socket_receive(sock, p, size)); size -= result)
//		p += result;
//	return p - static_cast<char*>(buffer);
//}

size_t socket_peek(SOCKET sock, void *buffer, size_t size) {
	int	result = recv(sock, static_cast<char*>(buffer), (int)size, MSG_PEEK);
	if (result == SOCKET_ERROR) {
		socket_error("recv()");
		return 0;
	}
	return size_t(result);
}

bool socket_close(SOCKET sock) {
	if (sock != INVALID_SOCKET && closesocket(sock) == SOCKET_ERROR) {
		socket_error("closesocket()");
		return false;
	}
	return true;
}

SOCKET socket_broadcast(PORT port_out, PORT port_in, const void *buffer, size_t size) {
	uint32	broadcast	= 1;

	sockaddr_in	addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family			= AF_INET;
	addr.sin_port			= htons(port_out);
	addr.sin_addr.s_addr	= htonl(INADDR_BROADCAST);

	SOCKET	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast)) != 0
	||	sendto(sock, static_cast<const char*>(buffer), (int)size, 0, (sockaddr*)&addr, (int)sizeof(addr)) < 0
	) {
		socket_error("broadcast");
		socket_close(sock);
		return INVALID_SOCKET;
	}

	addr.sin_port			= uint16be(port_in);
	addr.sin_addr.s_addr	= 0;//htonl(INADDR_ANY);

	SOCKET	sock2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bind(sock2, (sockaddr*)&addr, sizeof(addr));
	return sock2;
}

} // namespace iso
