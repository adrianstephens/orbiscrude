#include "stream.h"

//-----------------------------------------------------------------------------
//	CFF
//-----------------------------------------------------------------------------

namespace cff {
using namespace iso;

static const int nStdStrings = 391;

enum prop {
	version				= 0x00,
	Notice				= 0x01,
	FullName			= 0x02,
	FamilyName			= 0x03,
	Weight				= 0x04,
	FontBBox			= 0x05,
	BlueValues			= 0x06,
	OtherBlues			= 0x07,
	FamilyBlues			= 0x08,
	FamilyOtherBlues	= 0x09,
	StdHW				= 0x0a,
	StdVW				= 0x0b,
	escape				= 0x0c,
	UniqueID			= 0x0d,
	XUID				= 0x0e,
	charset				= 0x0f,
	Encoding			= 0x10,
	CharStrings			= 0x11,
	Private				= 0x12,
	Subrs				= 0x13,
	defaultWidthX		= 0x14,
	nominalWidthX		= 0x15,
	//-Reserved-		= 0x16-0x1b,
	shortint			= 0x1c,
	longint				= 0x1d,
	BCD					= 0x1e,
	//-Reserved-		= 0x1f,

//	prefixed with 0xc (escape)
	Copyright			= 0x20,
	isFixedPitch		= 0x21,
	ItalicAngle			= 0x22,
	UnderlinePosition	= 0x23,
	UnderlineThickness	= 0x24,
	PaintType			= 0x25,
	CharstringType		= 0x26,
	FontMatrix			= 0x27,
	StrokeWidth			= 0x28,
	BlueScale			= 0x29,
	BlueShift			= 0x2a,
	BlueFuzz			= 0x2b,
	StemSnapH			= 0x2c,
	StemSnapV			= 0x2d,
	ForceBold			= 0x2e,
	//-Reserved-		= 0x2f-0x10,
	LanguageGroup		= 0x31,
	ExpansionFactor		= 0x32,
	initialRandomSeed	= 0x33,
	SyntheticBase		= 0x34,
	PostScript			= 0x35,
	BaseFontName		= 0x36,
	BaseFontBlend		= 0x37,
	//-Reserved-		= 0x38-0x1d,
	ROS					= 0x3e,
	CIDFontVersion		= 0x3f,
	CIDFontRevision		= 0x40,
	CIDFontType			= 0x41,
	CIDCount			= 0x42,
	UIDBase				= 0x43,
	FDArray				= 0x44,
	FDSelect			= 0x45,
	FontName			= 0x46,
};

enum op {
	op_hstem			= 0x01,
	op_vstem			= 0x03,
	op_vmoveto			= 0x04,
	op_rlineto			= 0x05,
	op_hlineto			= 0x06,
	op_vlineto			= 0x07,
	op_rrcurveto		= 0x08,
	op_callsubr			= 0x0a,
	op_return			= 0x0b,
	op_endchar			= 0x0e,
	op_hstemhm			= 0x12,
	op_hintmask			= 0x13,
	op_cntrmask			= 0x14,
	op_rmoveto			= 0x15,
	op_hmoveto			= 0x16,
	op_vstemhm			= 0x17,
	op_rcurveline		= 0x18,
	op_rlinecurve		= 0x19,
	op_vvcurveto		= 0x1a,
	op_hhcurveto		= 0x1b,
	op_shortint			= 0x1c,
	op_callgsubr		= 0x1d,
	op_vhcurveto		= 0x1e,
	op_hvcurveto		= 0x1f,

//	prefixed with 0xc (escape)
	op_dotsection		= 0x20,	// (deprecated)
	op_and				= 0x23,
	op_or				= 0x24,
	op_not				= 0x25,
	op_abs				= 0x29,
	op_add				= 0x2a,
	op_sub				= 0x2b,
	op_div				= 0x2c,
	op_neg				= 0x2e,
	op_eq				= 0x2f,
	op_drop				= 0x32,
	op_put				= 0x34,
	op_get				= 0x35,
	op_ifelse			= 0x36,
	op_random			= 0x37,
	op_mul				= 0x38,
	op_sqrt				= 0x3a,
	op_dup				= 0x3b,
	op_exch				= 0x3c,
	op_index			= 0x3d,
	op_roll				= 0x3e,
	op_hflex			= 0x42,
	op_flex				= 0x43,
	op_hflex1			= 0x44,
	op_flex1			= 0x45,

	op_fixed16_16		= 0xff,
};

enum op1 {
	op1_hstem			= 0x01,
	op1_vstem			= 0x03,
	op1_vmoveto			= 0x04,
	op1_rlineto			= 0x05,
	op1_hlineto			= 0x06,
	op1_vlineto			= 0x07,
	op1_rrcurveto		= 0x08,
	op1_closepath		= 0x09,
	op1_callsubr		= 0x0a,
	op1_return			= 0x0b,
	op1_hsbw			= 0x0d,
	op1_endchar			= 0x0e,
	op1_rmoveto			= 0x15,
	op1_hmoveto			= 0x16,
	op1_vhcurveto		= 0x1e,
	op1_hvcurveto		= 0x1f,
//	prefixed with 0xc (escape)
	op1_dotsection		= 0x20,
	op1_vstem3			= 0x21,
	op1_hstem3			= 0x22,
	op1_seac			= 0x26,
	op1_sbw				= 0x27,
	op1_div				= 0x2c,
	op1_callothersubr	= 0x30,
	op1_pop				= 0x31,
	op1_setcurrentpoint	= 0x41,
	op1_longint			= 0xff,
};

struct packed_int : holder<int32> {
	template<typename R> bool	read(R &r, uint8 b0) {
		if (b0 < 32) {
			if (b0 == shortint)		//	b0==28:		bytes:3; range:�32768..+32767
				t = r.get<int16be>();
			else if (b0 == longint)	//	b0==29:		bytes:5; range:�2^31..+2^31-1
				t = r.get<int32be>();
			else
				return false;
		} else {
			if (b0 < 247)		// 32<b0<246:	bytes:1; range:�107..+107
				t = int(b0) - 139;
			else if (b0 < 251)	// 247<b0<250:	bytes:2; range:+108..+1131
				t = (int(b0) - 247) * +256 + r.getc() + 108;
			else if (b0 < 255)	// 251<b0<254:	bytes:2; range:�1131..�108
				t = (int(b0) - 251) * -256 - r.getc() - 108;
			else
				return false;
		}
		return true;
	}
	template<typename R> bool	read(R &r) {
		return read(r, r.getc());
	}

	template<typename W> bool	write(W &w) {
		if (abs(t) <= 107)
			w.putc(t + 139);
		else if (abs(t) <= 1131) {
			if (t < 0) {
				w.putc((t + 108) / -256 + 251);
				w.putc(t + 108);
			} else {
				w.putc((t - 108) / 256 + 247);
				w.putc(t - 108);
			}
		} else if (t <= 32767 && t >= -32768) {
			w.putc(shortint);
			w.write(int16be(t));
		} else {
			w.putc(longint);
			w.write(int32be(t));
		}
		return true;
	}
};
struct packed_float : holder<float> {
	template<typename R> bool	read(R &r, uint8 b0) {
		static const char *trans = "0123456789.EE?-";
		if (b0 != BCD)
			return false;
		char	s[16], *p = s;
		do {
			uint8	b = r.getc();
			*p++ = trans[b >> 4];
			if ((b >> 4) == 12)
				*p++ = '-';
			*p = trans[b & 15];
			if ((b & 15) == 12)
				*++p = '-';
		} while (*p++);
		return !!from_string(s, t);
	}
	template<typename R> bool	read(R &r) {
		return read(r, r.getc());
	}
	static char *get_nibble(char *p, uint8 &n) {
		char	c = *p++;
		if (c >= '0' && c <= '9')
			n = c - '0';
		else if (c == '.')
			n = 10;
		else if (c == '-')
			n = 14;
		else if (c == 'e' || c == 'E') {
			if (*p == '-') {
				n = 12;
				p++;
			} else {
				n = 11;
			}
		}
		return p;
	}
	template<typename W> bool	write(W &w) {
		file.putc(BCD);
		char	s[16], *p = s, *e = p + to_string(p, t);
		uint8	n1, n2;
		while (p < s) {
			p = get_nibble(p, n1);
			if (p < s) {
				p = get_nibble(p, n2);
				w.putc((n1 << 4) | n2);
				n1 = 0xf;
			}
		}
		w.putc((n1 << 4) | 0xf);
		return true;
	}
};

enum Type {
	t_int	= 0,
	t_float	= 1,
	t_prop	= 2,
	t_sid	= t_int,
};

struct value {
	Type	type;
	uint32	data;

	template<typename R> inline int get_num(uint8 b0, R &r) {
		return b0 < 247	?  int(b0) - 139							// 32  < b0 < 246:	bytes:1; range:�107..+107
			 : b0 < 251	? (int(b0) - 247) * +256 + r.getc() + 108	// 247 < b0 < 250:	bytes:2; range:+108..+1131
						: (int(b0) - 251) * -256 - r.getc() - 108;	// 251 < b0 < 254:	bytes:2; range:�1131..�108
	}

	operator float()	const { return type == t_float ? (float&)data : (int&)data; }
	operator int()		const { return type == t_float ? (float&)data : (int&)data; }
};

struct value_prop : value {
	template<typename R> bool	read(R &r) {
		uint8	b0 = r.getc();
		if (b0 < 0x20) {
			switch (b0) {
				case shortint:
					type = t_int;
					data = r.template get<int16be>();
					return true;
				case longint:
					type = t_int;
					data = r.template get<int32be>();
					return true;
				case escape:
					type = t_prop;
					data = r.getc() + 0x20;
					return true;
				case BCD:
					type = t_float;
					return ((packed_float&)data).read(r, b0);
				default:
					type = t_prop;
					data = b0;
					return true;
			}
		} else if (b0 != 0xff) {
			type = t_int;
			data = get_num(b0, r);
			return true;
		}
		return false;
	}
};

struct value_op : value {
	template<typename R> bool	read(R &r) {
		uint8	b0 = r.getc();
		if (b0 < 0x20) {
			switch (b0) {
				case op_shortint:
					type = t_prop;
					data = r.template get<int16be>();
					break;
				case escape:
					type = t_prop;
					data = r.getc() + 0x20;
					break;
				default:
					type = t_prop;
					data = b0;
					break;
			}
		} else if (b0 == op_fixed16_16) {
			type = t_float;
			data = r.template get<int32be>();
		} else {
			type = t_int;
			data = get_num(b0, r);
		}
		return true;
	}
};

struct value_op1 : value {
	template<typename R> bool	read(R &r) {
		uint8	b0 = r.getc();
		if (b0 < 0x20) {
			switch (b0) {
				case escape:
					type = t_prop;
					data = r.getc() + 0x20;
					break;
				default:
					type = t_prop;
					data = b0;
					break;
			}
		} else if (b0 == op1_longint) {
			type = t_int;
			data = r.get<int32be>();
		} else {
			type = t_int;
			data = get_num(b0, r);
		}
		return true;
	}
};

struct index {
	uint16be	count;
	uint8		offset_size;
	uint8		offsets;

	template<typename T> const_memory_block get(uint32 i) const {
		T		*table	= (T*)&offsets;
		uint8	*data	= (uint8*)(table + count + 1) - 1;
		return const_memory_block(data + uint32(table[i]), uint32(table[i + 1]) - uint32(table[i]));
	}

	const_memory_block operator[](uint32 i) const {
		switch (offset_size) {
			case 1: return get<uint8>			(i);
			case 2: return get<uint16be>		(i);
			case 3: return get<uintn<3,true> >	(i);
			case 4: return get<uint32be>		(i);
		}
		return empty;
	};

	const void*	end()		const { return count == 0 ? (void*)&offset_size : (*this)[count - 1].end(); }
	size_t		length()	const { return (uint8*)end() - (uint8*)this; }
};

struct charset0 {
	uint16be			sid;
};
struct charset1 {
	packed<uint16be>	sid;
	uint8				left;
};
struct charset2 {
	uint16be			sid;
	uint16be			left;
};

struct header {
	uint8	major_version;	// Format major version (starting at 1)
	uint8	minor_version;	// Format minor version (starting at 0)
	uint8	hdrSize;		// Header size (bytes)
	uint8	offset_size;	// Absolute offset (0) size
};

struct dict_entry {
	prop		p;
	uint16		vals;
};

inline dict_entry* dictionary_lookup(prop p, dict_entry *entries) {
	for (dict_entry *i = entries; i->p != (prop)-1; ++i) {
		if (i->p == p)
			return i;
	}
	return 0;
}

inline range<value*> dictionary_lookup(prop p, dict_entry *entries, value *vals) {
	if (auto *i = dictionary_lookup(p, entries))
		return range<value*>(vals + i->vals, vals + i[1].vals);
	return none;
}

struct dictionary : dynamic_array<dict_entry> {
	dynamic_array<value_prop>	vals;

	bool	add(istream_ref file) {
		uint32	prev	= 0;
		while (!file.eof()) {
			value_prop	v = file.get();
			if (v.type == t_prop) {
				dict_entry	*e = new(expand()) dict_entry;
				e->p		= (prop&)v.data;
				e->vals		= prev;
				prev		= vals.size32();
			} else {
				vals.push_back(v);
			}
		}
		dict_entry	*e = new(expand()) dict_entry;
		e->p		= (prop)-1;
		e->vals		= prev;
		return true;
	}

	range<cff::value*> lookup(prop p) {
		return dictionary_lookup(p, *this, vals);
	}

	dictionary()	{}
	dictionary(index *ind) {
		for (int i = 0, n = ind->count; i < n; i++)
			add(memory_reader((*ind)[i]));
	}

	auto	end() const { return dynamic_array<dict_entry>::end() - 1; }

	range<const cff::value*>	get_values(const dict_entry *i) const {
		return {vals + i->vals, vals + i[1].vals};
	}
};

struct dict_top_defaults {
	static auto	*values() {
		static value values[] = {
			{t_sid,	0},
			{t_sid,	1},
			{t_sid,	2},
			{t_sid,	3},
			{t_sid,	4},
			{t_sid,	0x20},
			{t_sid,	0x34},
			{t_sid,	0x35},
			{t_int,	0},
			{t_int,	0},
			{t_int,	uint32(-100)},
			{t_int,	50},
			{t_int,	0},
			{t_int,	2},
			{t_int,	0},
			{t_int,	0},
			{t_int,	0},
			{t_float, iso::iorf(0.001f).i()}, {t_float, 0}, {t_float, 0}, {t_float, iso::iorf(0.001f).i()}, {t_float, 0}, {t_float, 0},
			{t_int,	0},  {t_int, 0},  {t_int, 0},  {t_int, 0},
		};
		return values;
	}

	static auto	*entries() {
		static dict_entry entries[] = {
			{version,			0},		//sid(0)
			{Notice,			1},		//sid(1)
			{FullName,			2},		//sid(2)
			{FamilyName,		3},		//sid(3)
			{Weight,			4},		//sid(4)
			{Copyright,			5},		//sid(0x20)
			{BaseFontName,		6},		//sid(0x34)
			{PostScript,		7},		//sid(0x35)
			{isFixedPitch,		8},		//int(0},
			{ItalicAngle,		9},		//int(0},
			{UnderlinePosition,	10},	//int(uint32(-100)},
			{UnderlineThickness,11},	//int(50},
			{PaintType,			12},	//int(0},
			{CharstringType,	13},	//int(2},
			{StrokeWidth,		14},	//int(0},
			{charset,			15},	//int(0},
			{Encoding,			16},	//int(0},
			{FontMatrix,		17},	//float(0.001), float(0), float(0), float(0.001), float(0}, float(0)
			{FontBBox,			23},	//int(0), int(0), int(0), int(0)
			{(prop)-1,			27},
		};
		return entries;
	}
};

struct dict_pvr_defaults {
	static auto	*values() {
		static value values[] = {
			{t_int,	0},
			{t_int,	1},
			{t_int,	7},
			{t_float,	iso::iorf(0.039625f).i()},
			{t_float,	iso::iorf(0.06f).i()},
		};
		return values;
	}
	static auto	*entries() {
		static dict_entry entries[] = {
			{BlueScale,			3},	//floatf(0.039625)
			{BlueShift,			2},	//int(7)
			{BlueFuzz,			1},	//int(1)
			{ForceBold,			0},	//int(0)
			{LanguageGroup,		0},	//int(0)
			{ExpansionFactor,	4},	//float(0.06)
			{initialRandomSeed,	0},	//int(0)
			{defaultWidthX,		0},	//int(0)
			{nominalWidthX,		0},	//int(0)
			{(prop)-1,			1},
		};
		return entries;
	}
};

struct dictionary_with_defaults {
	dictionary	&dict;
	dict_entry	*def_entries;
	value		*def_vals;

	template<typename T> dictionary_with_defaults(dictionary &dict, const T &def) : dict(dict), def_entries(def.entries()), def_vals(def.values()) {}

	range<value*> lookup(prop p) {
		auto e = dict.lookup(p);
		return !e.empty() ? e : dictionary_lookup(p, def_entries, def_vals);
	}
};

}// namespace ccf;

namespace postscript {
using namespace iso;

struct encryption {
	enum {c1 = 52845, c2 = 22719, eexec_key = 55665, charstring_key = 4330};
	uint16	r;

	uint8 encrypt(uint8 b) {
		uint8 x = b ^ (r >> 8);
		r = (x + r) * c1 + c2;
		return x;
	}
	uint8 decrypt(uint8 x) {
		uint8 b = x ^ (r >> 8);
		r = (x + r) * c1 + c2;
		return b;
	}
	void encrypt(const void *src, void *dst, size_t len) {
		uint8 *d = (uint8*)dst;
		for (const uint8 *s = (const uint8*)src; len--; s++, d++)
			*d = encrypt(*s);
	}
	void decrypt(const void *src, void *dst, size_t len) {
		uint8 *d = (uint8*)dst;
		for (const uint8 *s = (const uint8*)src; len--; s++, d++)
			*d = decrypt(*s);
	}
	const char *decrypt_ascii(const char *src, void *dst, size_t len) {
		uint8	b	= 0xff;
		uint8	*d	= (uint8*)dst;
		uint8	*e	= d + len;
		for (;;) {
			char	c = *src++;
			if (is_hex(c)) {
				uint8	n = c > '9' ? c + 9 : c;
				if (b & 0xf) {
					b	= n << 4;
				} else {
					*d++= decrypt(b | (n & 0xf));
					if (d == e)
						return src;
					b	= 0xff;
				}
			}
		}
	}

	size_t	eexec_decrypt(const void *src, void *dst, size_t len) {
		r = eexec_key;
		const char *s = (const char*)src;
		if (is_hex(s[0]) && is_hex(s[1]) && is_hex(s[2]) && is_hex(s[3])) {
			s = decrypt_ascii(s, dst, 4);
			return decrypt_ascii(s, dst, len) - s;
		} else {
			decrypt(src, dst, 4);
			decrypt(s + 4, dst, len - 4);
			return len -4;
		}
	}

	void	charstring_decrypt(const void *src, void *dst, size_t len) {
		//skip might be prvt.lenIV (Version 23.0 requires n to be 4.)
		r = charstring_key;
		decrypt(src, dst, 4);
		decrypt((uint8*)src + 4, dst, len - 4);
	}
};

}// namespace postscript;
