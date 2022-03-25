#ifndef ISOLINK_H
#define ISOLINK_H

#include "sockets.h"

// typedefs
typedef SOCKET isolink_handle_t;
const isolink_handle_t isolink_invalid_handle = ~0u;

inline bool			isolink_init()		{ return iso::socket_init();	}
inline bool			isolink_term()		{ return iso::socket_term();	}
inline bool			isolink_announce()	{ return iso::socket_announce();}
inline const char*	isolink_get_error()	{ return iso::socket_error();	}

inline isolink_handle_t isolink_listen(PORT port) {
	return iso::socket_listen(port);
}
inline bool isolink_send(isolink_handle_t handle, const void *buffer, size_t size) {
	return !!iso::socket_send(handle, buffer, size);
}
inline isolink_handle_t	isolink_broadcast(PORT port_out, PORT port_in, const void *buffer, size_t size) {
	return iso::socket_broadcast(port_out, port_in, buffer, size);
}
inline size_t isolink_receive0(isolink_handle_t handle, void *buffer, size_t size) {
	return iso::socket_receive(handle, buffer, size);
}
inline size_t isolink_receive(isolink_handle_t handle, void *buffer, size_t size) {
	char *p = static_cast<char*>(buffer);
	for (size_t result; size && (result = iso::socket_receive(handle, p, size)); size -= result)
		p += result;
	return p - static_cast<char*>(buffer);
}
inline bool isolink_close(isolink_handle_t handle) {
	return iso::socket_close(handle);
}

#if defined(CROSS_PLATFORM) || defined(ISOLINK_TARGET_H)
	typedef const char*	isolink_platform_t;
	#define isolink_platform_invalid	0

	enum isolink_enum_flags {ISOLINK_ENUM_PLATFORMS, ISOLINK_ENUM_DEVICES};

	typedef bool		isolink_enum_host_callback(const char *target, const char *platform, void *param);
	bool				isolink_enum_hosts(isolink_enum_host_callback *callback, isolink_enum_flags flags, void *param);

	isolink_platform_t	isolink_resolve(const char *target, const char *service, const struct addrinfo *hints, struct addrinfo **result);
	isolink_platform_t	isolink_resolve(const char *target, unsigned short port);
	isolink_handle_t	isolink_send(const char *target, unsigned short port, const void *buffer, size_t size);
#endif

#endif // ISOLINK_H
