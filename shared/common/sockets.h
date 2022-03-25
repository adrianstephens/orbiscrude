#ifndef SOCKETS_H
#define SOCKETS_H

#include "base/defs.h"

#if defined(PLAT_PC) || defined(PLAT_X360) || defined(PLAT_XONE)
typedef UINT_PTR		SOCKET;
#define INVALID_SOCKET  (SOCKET)(~0)
#else
typedef int				SOCKET;
#define INVALID_SOCKET  -1
#endif

typedef unsigned short	PORT;

namespace iso {

bool		socket_init();
bool		socket_term();
bool		socket_announce();
void		socket_error(const char *reason);
const char*	socket_error();

SOCKET		socket_create(int family, int socktype, int protocol);
bool		socket_connect(SOCKET sock, void *addr, size_t addr_len);
SOCKET		socket_accept(SOCKET sock, void *addr, size_t *addr_len);
SOCKET		socket_listen(PORT port);
SOCKET		socket_broadcast(PORT out, PORT in, const void *buffer, size_t size);
size_t		socket_send(SOCKET sock, const void *buffer, size_t size);
size_t		socket_receive(SOCKET sock, void *buffer, size_t size);
size_t		socket_peek(SOCKET sock, void *buffer, size_t size);
bool		socket_close(SOCKET sock);
bool		socket_select(SOCKET sock);
int			socket_select(SOCKET *socks, size_t nsocks);
bool		socket_select(float time, SOCKET sock);
int			socket_select(float time, SOCKET *socks, size_t nsocks);
bool		socket_setopt(SOCKET sock, int level, int option, const void *value, size_t size);
int			socket_getopt(SOCKET sock, int level, int option, void *value, size_t size);

template<typename T> bool socket_setopt(SOCKET sock, int level, int option, const T &value) {
	return socket_setopt(sock, level, option, &value, sizeof(value));
}
inline bool socket_setopt(SOCKET sock, int level, int option, const char *value) {
	return socket_setopt(sock, level, option, value, strlen(value));
}
template<typename T> int socket_getopt(SOCKET sock, int level, int option, T &value) {
	return socket_getopt(sock, level, option, &value, sizeof(value));
}
}
#endif // SOCKETS_H
