#include "ip.h"

using namespace iso;

template<typename T> static size_t read_dotted(const char *s, T *p, int n, char sep) {
	string_scan	ss(s);
	for (int i = 0; i < n; i++) {
		if (i) {
			ISO_VERIFY(ss.getc() == sep);
		}
		ss.get(p[i]);
	}
	return ss.getp() - s;
}

template<typename T> static size_t write_dotted(char *s, const T *p, int n, char sep) {
	char	*s0 = s;
	for (int i = 0; i < n; i++) {
		if (i)
			*s++ = sep;
		s += to_string(s, p[i]);
	}
	return s - s0;
}

size_t iso::from_string(const char *s, MAC &a) {
	return read_dotted(s, (xint8*)&a, 6, ':');
}
size_t iso::to_string(char *s, const MAC &a) {
	return write_dotted(s, (const xint8*)&a, 6, ':');
}
size_t IP4::from_string(const char *s, addr &a) {
	return read_dotted(s, (uint8*)&a, 4, '.');
}
size_t IP4::to_string(char *s, const addr &a) {
	return write_dotted(s, (const uint8*)&a, 4, '.');
}

size_t IP6::from_string(const char *s, addr &a)	{
	string_scan	ss(s);
	for (int i = 0; i < 8; i++) {
		while (i < 8 && ss.peekc() == ':') {
			ss.move(1);
			a[i++] = 0;
		}
		ss.get(unconst(hex(a[i])));
	}
	return ss.getp() - s;
}
size_t IP6::to_string(char *s, const addr &a) {
	int		bz	= 0;
	int		iz	= 0;
	for (int i = 0; i < 8; i++) {
		int	i0 = i;
		while (i < 8 && a[i] == 0)
			++i;
		int	nz	= i - i0;
		if (nz > bz) {
			bz = nz;
			iz = i0;
		}
	}

	char	*s0 = s;
	for (int i = 0; i < 8; i++) {
		if (i)
			*s++ = ':';
		if (i == iz) {
			*s++ = ':';
			i += bz;
		}
		if (i < 8)
			s += to_string(s, hex(a[i]));
	}
	return s - s0;
}

uint16 IP4::calc_checksum(void *buffer, uint32 size) {
	uint32	total	= 0;
	uint16	*p		= (uint16*)buffer;
	while (size > 1) {
		total	+= *p++;
		size	-= sizeof(uint16);
	}
	if (size)
		total += *(uint8*)p;

	total = (total >> 16) + (total & 0xffff);
	total += total >> 16;

	return chksum(~total);
}

#define MAX_PING_DATA_SIZE	1024

struct PingPacket : ICMP {
	uint32	timestamp;
	held_constant<uint32, 0xDEADBEEF>	data[(MAX_PING_DATA_SIZE - sizeof(ICMP)) / sizeof(uint32)];

	PingPacket(uint16 _id, uint16 _seq, uint32 size, uint32 _timestamp) : ICMP(ECHO_REQUEST, _id, _seq), timestamp(_timestamp) {
		checksum = IP4::calc_checksum(this, size);
	}
};

struct IPResponse : IP4::header {
	uint8	payload[MAX_PING_DATA_SIZE];
};

#ifndef PLAT_PC
int GetCurrentProcessId()	{ return 0; }
int GetTickCount()			{ return 0; }
#endif

int iso::ping(const char *host, int packet_size, int ttl) {

	socket_address	addr(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (!addr.resolve(host, uint16(0)))
		return -1;

	SOCKET			sock	= addr.socket();
	if (setsockopt(sock, IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl)) != 0) {
		socket_close(sock);
		return -1;
	}

	int				seq_no	= 0;
	PingPacket		send_buf((uint16)GetCurrentProcessId(), seq_no, packet_size, GetTickCount());
	IPResponse		recv_buf;

	int	ret	= -1;
	if (addr.send(sock, &send_buf, packet_size)) {
		for (;;) {
			// Receive replies until we either get a successful read, or a fatal error occurs.
			int	recv_size = addr.recv(sock, &recv_buf, MAX_PING_DATA_SIZE + sizeof(IP4::header));
			if (recv_size < 0) {
				// Pull the sequence number out of the ICMP header.
				// If it's bad, we just complain, but otherwise we take off, because the read failed for some reason.
				ICMP* icmp		= (ICMP*)recv_buf.payload;
				if (icmp->seq != seq_no)
					continue;
			} else {
				ICMP* icmp		= (ICMP*)recv_buf.payload;
				if (icmp->type == ICMP::ECHO_REPLY) {
					if (icmp->id != (uint16)GetCurrentProcessId())
						continue;		// Must be a reply for another pinger running locally, so just ignore it.

					// Figure out how far the packet travelled
					int hops = int(256 - recv_buf.ttl);
					ret = hops == 192 ? 1 : hops == 128 ? 0 : hops;
				}
			}
			break;
		}
	}
	socket_close(sock);
	return ret;
}


