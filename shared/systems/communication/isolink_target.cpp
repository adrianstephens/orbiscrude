#include "isolink_target.h"
#include <string.h>
#include <stdio.h>

#if 0//def PLAT_PC
#pragma comment(linker, "/include:_isolink_platform_ps3")
#pragma comment(linker, "/include:_isolink_platform_x360")
#pragma comment(linker, "/include:_isolink_platform_wii")
#pragma comment(linker, "/include:_isolink_platform_ios")
#pragma comment(linker, "/include:_isolink_platform_pc")
#endif

isolink_host_enum *isolink_host_enum::first;

char *iso_itoa(int i, char *p, int base) {
	int	n = 1;
	for (int d = base; i > d; d *= base)
		n++;

	p[n] = 0;
	while (n--) {
		p[n] = (i % base) + '0';
		i /= base;
	}
	return p;
}

#ifdef PLAT_PC
void _isolink_set_error(const char *err) {
	static char buff[256];
	sprintf(buff, "isolink: %s: 0x%08x", err, errno);
	iso::socket_error(buff);
}
#elif defined(PLAT_MAC)
void _isolink_set_error(const char *err) {
	static char buff[256];
	sprintf(buff, "isolink: %s", err);
	iso::socket_error(buff);
}
#endif

bool _isolink_discover(isolink_handle_t h, char *target) {
	char		buffer[256];
	sockaddr	addr;
	socklen_t	addrlen	= sizeof(addr);
	int			read	= (int)recvfrom(h, buffer, sizeof(buffer), 0, &addr, &addrlen);
	if (read <= 0)
		return false;

	buffer[read]		= 0;

	target += sprintf(target, "%s@", buffer);
	if (getnameinfo(&addr, sizeof(addr), target, 256, NULL, 0, 0)) {
  		unsigned char*	ip	= (unsigned char*)&((sockaddr_in&)addr).sin_addr;
		sprintf(target, "%i.%i.%i.%i", ip[0], ip[1], ip[2], ip[3]);
	}
	return true;
}

bool isolink_enum_hosts(isolink_enum_host_callback *callback, isolink_enum_flags flags, void *param) {
	for (isolink_host_enum *i = isolink_host_enum::first; i; i = i->next) {
		if (i->enumerate(callback, flags, param))
			return true;
	}
	return false;
}

isolink_platform_t isolink_resolve(const char *target, const char *service, const addrinfo *hints, addrinfo **result) {
	for (isolink_host_enum *i = isolink_host_enum::first; i; i = i->next) {
		if (isolink_platform_t platform = i->resolve(target, service, hints, result))
			return platform;
	}
	return isolink_platform_invalid;
}

isolink_platform_t isolink_resolve(const char *target, unsigned short port) {
	addrinfo_wrap address_info(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	char _port[6];
	return isolink_resolve(target, iso_itoa(port, _port, 10), address_info, &address_info);
}

isolink_handle_t isolink_send(const char *target, unsigned short port, const void *buffer, size_t size) {
	// resolve
	addrinfo_wrap	address_info(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	char _port[6];
	iso_itoa(port, _port, 10);

	isolink_platform_t	platform = isolink_platform_invalid;
	if (const char *at = strchr(target, '@')) {
		int	ip[4], n;
		if (sscanf(at + 1, "%i.%i.%i.%i%n", ip+0, ip+1, ip+2, ip+3, &n) == 4 && at[n + 1] == 0) {
			static sockaddr	addr;
#ifdef PLAT_MAC
			addr.sa_len		= 16;
#endif
			addr.sa_family	= address_info.ai_family;
			addr.sa_data[0]	= port >> 8;
			addr.sa_data[1]	= port & 0xff;
			addr.sa_data[2]	= ip[0];
			addr.sa_data[3]	= ip[1];
			addr.sa_data[4]	= ip[2];
			addr.sa_data[5]	= ip[3];

			address_info.ai_flags	= AI_NUMERICHOST;
			address_info.ai_addrlen = 16;
			address_info.ai_addr	= &addr;

			static char	plat[16];
			memcpy(plat, target, at - target);
			plat[at - target]	= 0;
			platform			= plat;
		}
	}
	if (platform == isolink_platform_invalid)
		platform = isolink_resolve(target, _port, address_info, &address_info);

	if (platform == isolink_platform_invalid) {
		_isolink_set_error("getaddrinfo()");
		return isolink_invalid_handle;
	}

	// socket
	isolink_handle_t handle = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
	if (handle == -1) {
		_isolink_set_error("socket()");
		return isolink_invalid_handle;
	}

	// connect
	if (connect(handle, address_info->ai_addr, (int)address_info->ai_addrlen) < 0) {
		_isolink_set_error("connect()");
		isolink_close(handle);
		return isolink_invalid_handle;
	}

	// send
	if (send(handle, static_cast<const char*>(buffer), (int)size, 0) < 0) {
		_isolink_set_error("send()");
		isolink_close(handle);
		return isolink_invalid_handle;
	}
	return handle;
}
