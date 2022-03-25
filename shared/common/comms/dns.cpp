#include "ip.h"

using namespace iso;

template<int MASK> inline uint32 get_field(uint32 x) {
	return (x & MASK) / (MASK & -MASK);
}
template<int MASK> inline uint32 set_field(uint32 x, uint32 y) {
	return (x & ~MASK) | ((MASK & -MASK) * y);
}
template<int MASK> inline void set_field(uint32 &x, uint32 y) {
	x = set_field<MASK>(uint32(x), y);
}

struct DNS	{
	enum {PORT = 53};
	enum opcode {
		QUERY		= 0,
	};
	enum question_class {
		C_IN		= 1,		// the internet
		C_CHAOS		= 3,		// for chaos net (MIT)
		C_ANY		= 255,		// wildcard	match
	};
	enum question_type {
		T_A			= 1,
		T_NS		= 2,
		T_CNAME		= 5,
		T_SOA		= 6,
		T_PTR		= 12,
		T_MX		= 15,
		T_TXT		= 16,
		T_SIG		= 24,
		T_AAAA		= 28,
		T_SRV		= 33,
		T_NAPTR		= 35,
		T_OPT		= 41,
		T_TKEY		= 249,
		T_TSIG		= 250,
		T_MAILB		= 253,
		T_ANY		= 255,
	};
	enum {
		OK			= 0,		// no error
		FORMAT_ERR	= 1,		// format error
		SERVER_FAIL	= 2,		// server failure
		NX_DOMAIN	= 3,		// non existent	domain
		NOT_IMP		= 4,		// not implemented
		REFUSED		= 5,		// query refused
	};
	enum {
		QR		= 0x8000,
		OPCODE	= 0x7800,
		AA		= 0x0400,
		TC		= 0x0200,
		RD		= 0x0100,
		RA		= 0x0080,
		AD		= 0x0020,
		CD		= 0x0010,
		RCODE	= 0x000f,
	};
	uint16be	id;
	uint16be	flags;
	uint16be	n_query, n_answer, n_authority, n_additional;

	uint8		*put_name(uint8 *dest, const char *name);
	const uint8	*get_name(const uint8 *srce, char *name) const;

	int			opcode()	const	{ return get_field<OPCODE>(flags);	}
	int			rcode()		const	{ return get_field<RCODE>(flags);	}
	void		set_rcode(int code)	{ set_field<RCODE>(flags, code);	}

	DNS() {}
	DNS(uint16 _id) : id(_id), flags(0), n_query(0), n_answer(0), n_authority(0), n_additional(0) {}
};

uint8 *DNS::put_name(uint8 *dest, const char *name) {
	while (const char *dot = strchr(name, '.')) {
		int	len = int(dot - name);
		dest[0] = len;
		memcpy(dest + 1, name, len);
		dest	+= len + 1;
		name	= dot + 1;
	}
	size_t	len = strlen(name);
	dest[0] = uint8(len);
	memcpy(dest + 1, name, len + 1);
	return dest + len + 2;
}

const uint8 *DNS::get_name(const uint8 *srce, char *name) const {
	const uint8	*end = 0;
	while (uint8 len = srce[0]) {
		if (len >= 0xc0) {
			if (!end)
				end = srce + 2;
			srce = (uint8*)this + srce[1] + (srce[0] & 0x3f) * 0x100;
			continue;
		}
		memcpy(name, srce + 1, len);
		name[len] = '.';
		name += len + 1;
		srce += len + 1;
	}
	name[-1] = 0;
	return end ? end : srce + 1;
}

struct DNSpacket : DNS {
	uint32	data[64];
	void	*payload()	{ return data; }
	DNSpacket() {}
	DNSpacket(uint16 id) : DNS(id)	{}
};

IP4::addr DNSQuery(const char *name) {
	DNSpacket	dns(42);
	dns.n_query = 1;
	dns.flags	= DNS::RD;

	byte_writer	data(dns.payload());
	data.p		= dns.put_name(data.p, name);
	data.write(uint16be(DNS::T_A));
	data.write(uint16be(DNS::C_IN));

	socket_address	addr = socket_address::UDP();
	addr.resolve("192.168.0.201", DNS::PORT);
	SOCKET	sock = addr.socket();
	addr.send(sock, &dns, data.p - (uint8*)&dns);

	DNSpacket	dns2;
	auto		r = recv(sock, (char*)&dns2, sizeof(dns2), 0);
	socket_close(sock);

	char		temp[256];
	byte_reader	read(dns2.payload());
	read.p		= dns.get_name(read.p, temp);	// question
	uint16	typ	= read.get<uint16be>();
	uint16	cls	= read.get<uint16be>();

	read.p		= dns.get_name(read.p, temp);	// answer
	typ			= read.get<uint16be>();
	cls			= read.get<uint16be>();

	uint32	ttl	= read.get<uint32be>();
	uint16	len	= read.get<uint16be>();

	return *(IP4::addr*)read.p;
}

fixed_string<256> DNSQuery(const IP4::addr &ip) {
	DNSpacket	dns(42);
	dns.n_query = 1;
	dns.flags	= DNS::RD;

	buffer_accum<256>	ba;
	ba << ip.flip() << ".in-addr.arpa";

	byte_writer	data(dns.payload());
	data.p		= dns.put_name(data.p, ba.term());
	data.write(uint16be(DNS::T_PTR));
	data.write(uint16be(DNS::C_IN));

	socket_address	addr = socket_address::UDP();
	addr.resolve("192.168.0.201", DNS::PORT);
	SOCKET	sock = addr.socket();
	addr.send(sock, &dns, data.p - (uint8*)&dns);

	DNSpacket	dns2;
	int		r = recv(sock, (char*)&dns2, sizeof(dns2), 0);
	socket_close(sock);

	fixed_string<256>	result;

	byte_reader	read(dns2.payload());
	read.p		= dns.get_name(read.p, result);	// question
	uint16	typ	= read.get<uint16be>();
	uint16	cls	= read.get<uint16be>();

	read.p		= dns.get_name(read.p, result);	// answer
	typ			= read.get<uint16be>();
	cls			= read.get<uint16be>();

	uint32	ttl	= read.get<uint32be>();
	uint16	len	= read.get<uint16be>();
	dns.get_name(read.p, result);
	return result;
}
