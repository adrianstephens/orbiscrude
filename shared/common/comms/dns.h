#ifndef DNS_H
#define DNS_H

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

#endif // DNS_H
