#ifndef ISOLINK_TARGET_H
#define ISOLINK_TARGET_H

#include "base/defs.h"

#if defined(PLAT_PC)
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(PLAT_MAC)
#include <unistd.h>
#include <netdb.h>
#endif

#include "isolink.h"

void _set_error(const char *format, ...);
void _isolink_set_error(const char* reason);
bool _isolink_discover(isolink_handle_t h, char *target);

struct addrinfo_wrap : addrinfo {
	addrinfo	*info;
	addrinfo_wrap(int family, int socktype, int protocol) : info(this)	{
		memset(this, 0, sizeof(addrinfo));
		ai_family	= family;
		ai_socktype	= socktype;
		ai_protocol	= protocol;
	}
	~addrinfo_wrap()				{ if (info != this) freeaddrinfo(info); }

	operator addrinfo*()	const	{ return info;	}
	addrinfo*	operator->()		{ return info;	}
	addrinfo**	operator&()			{ return &info;	}
};

struct isolink_host_enum {
	static isolink_host_enum	*first;
	isolink_host_enum			*next;
	virtual	bool				enumerate(isolink_enum_host_callback *callback, isolink_enum_flags flags, void *param)	{ return false; }
	virtual	isolink_platform_t	resolve(const char *target, const char *service, const addrinfo *hints, addrinfo **result)=0;

	isolink_host_enum() { next = first; first = this; }
};

#endif // ISOLINK_TARGET_H