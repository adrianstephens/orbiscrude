#include "asn1.h"
#include "base/bits.h"
#include "extra/date.h"
#include "maths/bignum.h"

using namespace iso;
using namespace ASN1;

struct trace_accum2 : buffered_accum<trace_accum2, char, 512> {
	trace_accum2() {}
	trace_accum2(const char *fmt, ...)	{ va_list valist; va_start(valist, fmt); vformat(fmt, valist); }
	template<typename T> trace_accum2(const T &t)	{ *this << t; }
	void		flush(const char *s, size_t n)		{ char temp[512]; memcpy(temp, s, n); temp[n] = 0; _iso_debug_print(temp); }
};
#define ISO_TRACEF2(...)	trace_accum2(__VA_ARGS__)


/*
INSTANCE_OF ::= [UNIVERSAL 8] IMPLICIT SEQUENCE {
	type-id		<DefinedObjectClass>.&id,
	value		[0] EXPLICIT <DefinedObjectClass>.&Type
}

EXTERNAL	::= [UNIVERSAL 8] IMPLICIT SEQUENCE {
	direct-reference		OBJECT IDENTIFIER OPTIONAL,
	indirect-reference		INTEGER OPTIONAL,
	data-value-descriptor	ObjectDescriptor OPTIONAL,
	encoding				CHOICE {
		single-ASN1-type	[0] ABSTRACT-SYNTAX.&Type,
		octet-aligned		[1] IMPLICIT OCTET STRING,
		arbitrary			[2] IMPLICIT BIT STRING
	}
}

EmbeddedPDV	::= [UNIVERSAL 11] IMPLICIT SEQUENCE {
	identification CHOICE {
		-- Abstract and transfer syntax object identifiers
		syntaxes SEQUENCE {
			abstract	OBJECT IDENTIFIER,
			transfer	OBJECT IDENTIFIER
		},
		syntax					OBJECT IDENTIFIER,		-- single object identifier for identification of the abstract and transfer syntaxes
		presentation-context-id INTEGER,				-- (Applicable only to OSI environments) negotiated OSI presentation context identifies the abstract and transfer syntaxes

		-- (Applicable only to OSI environments)
		-- Context-negotiation in progress, presentation-context-id identifies only the abstract syntax so the transfer syntax shall be specified
		context-negotiation SEQUENCE {
			presentation-context-id		INTEGER,
			transfer-syntax				OBJECT IDENTIFIER
		},
		transfer-syntax OBJECT IDENTIFIER,				-- provided primarily to support selective-field-encryption of an ASN.1 type
		fixed			NULL							-- data value is the value of a fixed ASN.1 type
	},
	data-value-descriptor	ObjectDescriptor OPTIONAL,	-- provides human-readable identification of the class of the value
	data-value				OCTET STRING
} ( WITH COMPONENTS {
	... ,
	data-value-descriptor ABSENT
} )
*/

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

FLOAT_FLAGS::FLOAT_FLAGS(iord f) 	{
	if (f.i() == 0) {
		u		= 0;
	} else if (f.is_inf()) {
		u		= POS_INF + f.s;
	} else {
		u		= BINARY;
		explen	= min(highest_set_index(abs(f.get_exp())) / 8, 3);
		sign	= f.s;
	}
}

static uint32 get_digits(const char *p, int n) {
	uint32	r = 0;
	while (n--) {
		if (!is_digit(*p))
			return uint32(-1);
		r = r * 10 + *p++ - '0';
	}
	return r;
};

DateTime ReadDate(const char *p, int year_digits) {
	uint32	year	= get_digits(p, year_digits);
	if (year_digits < 4)
		year += 2000;
	p += year_digits;
	uint32	month	= get_digits(p + 0, 2);
	uint32	day		= get_digits(p + 2, 2);
	uint32	hour	= get_digits(p + 4, 2);
	uint32	mins	= get_digits(p + 6, 2);
	float	secs	= from_string<float>(p + 8);
	return DateTime(year, month, day) + DateTime::Secs((hour * 60 + mins) * 60 + secs);
}

void WriteDate(string_accum &a, DateTime d) {
	Date		date(d.Days());
	TimeOfDay	tod(d.TimeOfDay());
	a.format("%02u%02u%02u%02u%02u%02uZ", date.year%100, date.month, date.day, tod.Hour(), tod.Min(), int(tod.Sec()));
}

const Field *Field::Find(const Value &v) const {
	for (const Field *f = this; f->type; ++f) {
		if (v.tag == f->tag)
			return f;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Value
//-----------------------------------------------------------------------------

uint32 Value::read_tag(istream_ref file, bool &constructed) {
	int		b	= file.getc();
	if (b == EOF)
		return 0;

	uint32	tag	= b & 31;

	constructed	= !!(b & TYPE_CONSTRUCTED);

	if (tag == 31) {
		tag = 0;
		for (uint8 x = 0x80; x & 0x80; ) {
			x = file.getc();
			tag = (tag << 7) | (x & 127);
		};
	}
	uint8	c	= b >> 6;
	return tag | (c << 30);
}

void Value::write_tag(ostream_ref file, uint32 tag, bool constructed) {
	uint32	bits	= ((tag >> 30) << 6) | (constructed ? TYPE_CONSTRUCTED : 0);
	tag &= 0x3fffffff;

	if (tag < 31) {
		file.putc(tag | bits);

	} else {
		file.putc(31 | bits);
		for (int n = highest_set_index(tag) / 7; n > 0; --n)
			file.putc(((tag >> (n * 7)) & 0x7f) | 0x80);
	}
}

uint64 Value::read_len(istream_ref file) {
	uint64	len = file.getc();

	if (len & 0x80) {
		if (len == 0x80) {
			len = ~uint64(0);	// indefinite
		} else {
			int	n = len & 0x7f;
			len = 0;
			while (n--)
				len = (len << 8) | file.getc();
		}
	}

	return len;
}

void Value::write_len(ostream_ref file, uint64 len) {
	if (len < 0x80) {
		file.putc(uint8(len));
	} else {
		int n = highest_set_index(len) / 8 + 1;
		file.putc(n + 0x80);

		while (n--)
			file.putc(uint8(len >> (n * 8)));
	}
}

double Value::get_float() const {
	uint32		len	= length;
	if (len == 0)
		return 0;

	const uint8	*p	= get_ptr();
	FLOAT_FLAGS	f	= *p++;

	if (f.binary()) {
		//binary
		int		explen	= f.explen;
		if (explen == 4) {
			explen = *p++;
			len--;
		}
		len	-= explen + 1;

		int64	exp = (int8)*p++;
		while (--explen)
			exp = (exp << 8) | *p++;

		switch (f.base) {	// base
			case FLOAT_FLAGS::BASE8:	exp *= 3; break;
			case FLOAT_FLAGS::BASE16:	exp *= 4; break;
		}
		uint64	mant	= 0;
		while (len--)
			mant = (mant << 8) | *p++;
		return iord(mant, int(exp + f.scale + iord::E_OFF), f.sign).f();

	}
	if (f.special()) {
		//special
		return f.special_value();
	}

	//decimal
	return from_string<double>((char*)p);
}

void Value::set_float(double x) {
	if (x == 0) {
		set_len(0);
		return;
	}

	FLOAT_FLAGS	f(x);
	if (f.special()) {
		*set_len(1) = f;

	} else {
		uint32	explen	= highest_set_index(abs(iord(x).get_exp())) / 8;
		uint64	mant	= iord(x).m;
		uint32	mantlen = highest_set_index(mant) / 8;
		uint8	*p		= set_len(explen + (explen >= 3) + mantlen + 1);

		*p++	= f;
		if (explen >= 3)
			*p++ = explen;

		while (mantlen--)
			*p++ = mant >> (mantlen * 8);
	}
}

size_t iso::to_string(char *s, const Value &v) {
	switch (v.type) {
		case TYPE_BOOLEAN:
			return to_string(s, !!v.u);

		case TYPE_INTEGER:
		case TYPE_ENUMERATED:
			if (v.length <= 8) {
				int64	x = sign_extend(*(uint64be*)&v.p, v.length * 8);
				return to_string(s, x);
			}
			return 0;

		case TYPE_REAL:
			return to_string(s, v.f);

		case TYPE_OCTET_STRING:
			return to_string(s, "bytes");

		case TYPE_OBJECT_DESCRIPTOR:
		case TYPE_UTF8_STRING:
		case TYPE_NUMERIC_STRING:
		case TYPE_PRINTABLE_STRING:
		case TYPE_T61_STRING:
		case TYPE_VIDEOTEX_STRING:
		case TYPE_IA5_STRING:
		case TYPE_GRAPHIC_STRING:
		case TYPE_ISO64_STRING:
		case TYPE_GENERAL_STRING:
		case TYPE_UNIVERSAL_STRING:
		case TYPE_CHARACTER_STRING:
		case TYPE_BMP_STRING:

		case TYPE_GENERALIZED_TIME:
		case TYPE_UTC_TIME:
			v.get_buffer().copy_to(s);
			return v.length;

		case TYPE_SEQUENCE:
			return to_string(s, "Sequence");

		case TYPE_SET:
			return to_string(s, "Set");

		case TYPE_OBJECT_ID:
			if (const OID *o = v.get_objectid())
				return to_string(s, o->name);
			return to_string(s, "<unknown OID>");

		default:
			return 0;
	}
}

string_accum& print(string_accum &a, const Value &v, int indent) {
	switch (v.type) {
		case TYPE_EOC:
			return a << "<eoc>";

		case TYPE_NULL:
			return a << "<null>";

		case TYPE_BOOLEAN:
			return a << !!v.u;

		case TYPE_INTEGER:
		case TYPE_ENUMERATED: {
			const_memory_block	b	= v.get_buffer();
			if (b.size32() <= 8)
				return a << read_bytes<uint64be>(b);

			a << "0x";
			int	d = 2;
			const uint8 *p = b, *e = (const uint8*)b.end();
			while (p != e)
				put_num_base<16>(a.getp(d), 2, *p++);
			return a;
		}

		case TYPE_REAL:
			return a << v.f;

		case TYPE_BIT_STRING: {
			const_memory_block	b	= v.get_buffer();
			int	x = v.length & 7;

			a << '\'';
			int	d = 8;
			const uint8 *p	= b, *e = (const uint8*)b.end() - (x != 0);
			while (p != e)
				put_num_base<2>(a.getp(d), 8, *p++);

			if (x)
				put_num_base<2>(a.getp(x), x, *p >> (8 - x));
			return a << "'B";
		}

		case TYPE_OCTET_STRING: {
		#if 1
			a << "bytes[";
			const_memory_block	b	= v.get_buffer();
			const uint8 *p	= b, *e = (const uint8*)b.end();
			while (p != e) {
				a << *p++;
				a << (p == e ? ']' : ',');
			}
			return a;
		#else
//			memory_reader		mi	= memory_reader(const_memory_block(v.get_buffer(), v.length));
//			Value			v2;
//			v2.read(mi);
//			return print(a, v2, indent);
		#endif
		}

		case TYPE_OBJECT_DESCRIPTOR:
		case TYPE_UTF8_STRING:
		case TYPE_NUMERIC_STRING:
		case TYPE_PRINTABLE_STRING:
		case TYPE_T61_STRING:
		case TYPE_VIDEOTEX_STRING:
		case TYPE_IA5_STRING:
		case TYPE_GRAPHIC_STRING:
		case TYPE_ISO64_STRING:
		case TYPE_GENERAL_STRING:
		case TYPE_UNIVERSAL_STRING:
		case TYPE_CHARACTER_STRING:
		case TYPE_BMP_STRING:
			return a << str((const char*)v.get_ptr(), v.length);

		case TYPE_GENERALIZED_TIME:
		case TYPE_UTC_TIME:
			return a << ReadDate(v.get_ptr(), v.type == TYPE_GENERALIZED_TIME ? 4 : 2);

		case TYPE_OBJECT_ID:
			if (const OID *o = v.get_objectid())
				return a << o->name;
			return a << "<unknown OID>";

		case TYPE_SEQUENCE:
		case TYPE_SET: {
			int			n	= v.length / sizeof(Value);
			const Value	*e	= v.get_ptr();
			a << (v.type == TYPE_SEQUENCE ? "Sequence" : "Set") << '[' << n << "] {\n";
			for (int i = 0; i < n; i++)
				print(a << repeat('\t', indent + 1) << '[' << i << "] ", e[i], indent + 1) << '\n';
			return a << repeat('\t', indent) << "}";
		}
		default:
			return a;
	}
}

string_accum& iso::operator<<(string_accum &a, const Value &v) {
	return print(a, v, 0);
}

void *Value::extend_buffer(uint32 len) {
	uint32	prev = length;
	length = len;

	if (type == TYPE_BIT_STRING) {
		prev	= (prev + 7) >> 3;
		len		= (len + 7) >> 3;
	}

	void	*r;
	if (prev > 8) {
		r = p = realloc(p, len);

	} else if (len > 8) {
		void	*p0 = malloc(len);
		memcpy(p0, buffer, prev);
		r = p = p0;

	} else {
		r = buffer;
	}
	return (uint8*)r + prev;
}

bool Value::read_octetstring(istream_ref file, uint64 len) {
	for (streamptr end = file.tell() + len; file.tell() < end; ) {
		Value	v;
		v.read_header(file);

		if (v.type == TYPE_EOC)
			break;

		if (v.is_constructed()) {
			if (!read_octetstring(file, v.length))
				return false;
		} else {
			file.readbuff(extend_buffer(v.length), v.length);
		}
	}
	return true;
}

bool Value::read_bitstring(istream_ref file, uint64 len) {
	for (streamptr end = file.tell() + len; file.tell() < end; ) {
		Value	v;
		v.read_header(file);

		if (v.type == TYPE_EOC)
			break;

		if (v.is_constructed()) {
			if (!read_bitstring(file, v.length))
				return false;
		} else {
			uint8	adjust	= file.getc();
			file.readbuff(extend_buffer(((v.length - 1) << 3) - adjust), v.length - 1);
		}
	}
	return true;
}

void Value::read_header(istream_ref file) {
	bool	constructed;
	tag		= read_tag(file, constructed);
	type	= (constructed ? TYPE_CONSTRUCTED : 0) | (get_class(tag) ? 0 : tag);
	length	= (uint32)read_len(file);
}

//exactly matches dynamic_array<T>
struct ans1_dynamic_array {
	void	*p;
	size_t	curr_size;
	size_t	max_size;

	ans1_dynamic_array() : p(0), curr_size(0), max_size(0) {}
	void	*append(uint32 size) {
		if (curr_size >= max_size) {
			max_size	= max(max_size * 2, 16);
			p			= aligned_realloc(p, size * max_size, 8);
		}
		void	*r = (uint8*)p + size * curr_size++;
		memset(r, 0, size);
		return r;
	}
	void	*trim(uint32 size) {
		max_size = curr_size;
		return p = aligned_realloc(p, size * curr_size, 8);
	}
	void	*get(uint32 size, int i) {
		return (uint8*)p + i * size;
	}
	const void	*get(uint32 size, int i) const {
		return (const uint8*)p + i * size;
	}
};

bool Value::read_contents(istream_ref file) {
	switch (type) {
	  	case TYPE_SEQUENCE | TYPE_CONSTRUCTED:
		case TYPE_SET | TYPE_CONSTRUCTED: {
			type &= ~TYPE_CONSTRUCTED;	//indicate 'processed'
			ans1_dynamic_array	a;
			for (streamptr end = file.tell() + length; file.tell() < end; ) {
				Value	*v = (Value*)a.append(sizeof(Value));
				if (!v->read(file))
					return false;
				if (v->tag == TYPE_EOC) {
					a.curr_size--;
					break;
				}
			}
			p		= a.trim(sizeof(Value));
			length	= uint32(a.curr_size * sizeof(Value));
			return true;
		}

		case TYPE_BIT_STRING | TYPE_CONSTRUCTED: {
			uint64	len	= length;
			length = 0;
			return read_bitstring(file, len);
		}

		case TYPE_BIT_STRING: {
			uint8	adjust	= file.getc();
			uint32	len		= length - 1;
			length = (len << 3) - adjust;
			return check_readbuff(file, alloc_buffer(), len);
		}
		default:
			if (type & TYPE_CONSTRUCTED) {
				uint64	len	= length;
				length = 0;
				return read_octetstring(file, len);
			}
			return check_readbuff(file, alloc_buffer(), length);
	}
}

bool Value::read(istream_ref file) {
	read_header(file);

	if (get_class()) {
		if (is_constructed()) {
			streamptr end = file.tell() + length;
			return read(file) && file.tell() == end;
		}
		//unknown type - just read raw data
		file.readbuff(alloc_buffer(), length);
		return true;
	}

	return read_contents(file);
}

bool Value::write_contents(ostream_ref file) const {
	switch (type & 0x1f) {
		case TYPE_EOC:
		case TYPE_NULL:
		case TYPE_INTEGER:
		case TYPE_ENUMERATED:
		case TYPE_OCTET_STRING:
		case TYPE_OBJECT_DESCRIPTOR:
		case TYPE_UTF8_STRING:
		case TYPE_NUMERIC_STRING:
		case TYPE_PRINTABLE_STRING:
		case TYPE_T61_STRING:
		case TYPE_VIDEOTEX_STRING:
		case TYPE_IA5_STRING:
		case TYPE_GRAPHIC_STRING:
		case TYPE_ISO64_STRING:
		case TYPE_GENERAL_STRING:
		case TYPE_UNIVERSAL_STRING:
		case TYPE_CHARACTER_STRING:
		case TYPE_BMP_STRING:
		case TYPE_GENERALIZED_TIME:
		case TYPE_UTC_TIME:
		case TYPE_OBJECT_ID:
		case TYPE_REL_OBJECT_ID:
			write_len(file, length);
			file.write(get_buffer());
			break;

		case TYPE_BIT_STRING:
			return write_bitstring(file, length, get_buffer());

		case TYPE_SEQUENCE:
		case TYPE_SET: {
			dynamic_memory_writer	mo;
			for (auto &i : make_range<Value>(get_buffer()))
				i.write(mo);
			write_len(file, mo.length());
			file.write(mo.data());
			break;
		}

		default:
			return false;
	}

	return true;
}

bool ASN1::Read(istream_ref file, void *p, const Type *type) {
	Value	v;
	v.read_header(file);
	return type->read(file, p, v);
}

bool ASN1::Write(ostream_ref file, const void *p, const Type *type) {
	return type->write(file, p);
}

//-----------------------------------------------------------------------------
// TypeArray
//-----------------------------------------------------------------------------

bool ASN1::TypeArray::read(istream_ref file, void *p, Value &v) const {
	ans1_dynamic_array	a;
	for (streamptr end = file.tell() + v.length; file.tell() < end;) {
		Value	v2;
		v2.read_header(file);
		void	*p = a.append(subtype->size);
		create(p);
		if (!subtype->read(file, p, v2))
			return false;
	}
	a.trim(subtype->size);
	*(ans1_dynamic_array*)p = a;
	return true;
}
bool ASN1::TypeArray::write(ostream_ref file, const void *p, uint32 tag) const {
	dynamic_memory_writer	mo;
	ans1_dynamic_array	&a = *(ans1_dynamic_array*)p;
	for (int i = 0; i < a.curr_size; i++) {
		if (!subtype->write(mo, a.get(subtype->size, i)))
			return false;
	}
	Value::write_tag(file, (tag >> 30) ? tag : type == TYPE_SET_OF ? TYPE_SET : TYPE_SEQUENCE, true);
	Value::write_len(file, mo.length());
	file.write(mo.data());
	return true;
}

size_t	ASN1::TypeArray::count(const void *p) const {
	const ans1_dynamic_array	&a = *(const ans1_dynamic_array*)p;
	return a.curr_size;
}

memory_block	ASN1::TypeArray::get_element(void *p, int i) const {
	ans1_dynamic_array	&a = *(ans1_dynamic_array*)p;
	return memory_block(a.get(subtype->size, i), subtype->size);
}

const_memory_block	ASN1::TypeArray::get_element(const void *p, int i) const {
	const ans1_dynamic_array	&a = *(const ans1_dynamic_array*)p;
	return const_memory_block(a.get(subtype->size, i), subtype->size);
}

//-----------------------------------------------------------------------------
// TypeCompositeBase
//-----------------------------------------------------------------------------

bool ASN1::TypeCompositeBase::read(istream_ref file, void *p, Value &v) const {
	switch (type) {
		case TYPE_CHOICE: {
			const Field *f	= find(v.tag);
			return f && f->type->read(file, p, v);
		}
		case TYPE_SEQUENCE: {
			const Field *f	= begin();
			for (streamptr end = file.tell() + v.length; file.tell() < end; ++f) {
				Value	v2;
				v2.read_header(file);
				while ((f->flags & Field::OPT) && f->tag && v2.tag != f->tag)
					++f;

				if (!f->type || !f->type->read(file, (uint8*)p + f->offset, v2, !!(f->flags & Field::EXPTAG)))
					return false;
			}
			break;
		}
		case TYPE_SET:
			for (streamptr end = file.tell() + v.length; file.tell() < end;) {
				Value	v2;
				if (!v2.read(file))
					return false;

				const Field *f	= find(v2.tag);
				if (!f || !f->type->read(file, p, v2))
					return false;
			}
			break;
	}
	return true;
}

bool ASN1::TypeCompositeBase::write(ostream_ref file, const void *p, uint32 tag) const {
	dynamic_memory_writer	mo;
	for (const Field *f = begin(); f->type; ++f) {
		const void *x = (uint8*)p + f->offset;
		//explicit tag
		if (f->flags & Field::EXPTAG) {
			dynamic_memory_writer	mo2;
			if (!f->type->write(mo2, x))
				return false;

			Value::write_tag(mo, f->tag, true);
			Value::write_len(mo, mo2.length());
			mo.write(mo2.data());
		} else {
			if (!f->type->write(mo, x, f->tag))
				return false;
		}
	}
	Value::write_tag(file, tag ? tag : type, true);
	Value::write_len(file, mo.length());
	file.write(mo.data());
	return true;
}

//-----------------------------------------------------------------------------
// TypeDef<DateTime>
//-----------------------------------------------------------------------------

TypeDef<DateTime>::TypeDef() : TypeBase(this, TYPE_UTC_TIME, sizeof(DateTime)) {}

bool TypeDef<DateTime>::read(istream_ref file, DateTime *p, Value &v) const {
	if (v.type != TYPE_UTC_TIME && v.type != TYPE_GENERALIZED_TIME)
		return false;

	v.read_contents(file);
	*p = ReadDate(v.get_ptr(), v.type == TYPE_GENERALIZED_TIME ? 4 : 2);
	return true;
}
bool TypeDef<DateTime>::write(ostream_ref file, const DateTime *p, uint32 tag) const {
	char	temp[64];
	WriteDate(lvalue(fixed_accum(temp)), *p);
	uint32	len = string_len32(temp);
	Value	v;
	v.set(TYPE_UTC_TIME, len).copy_from(temp);
	return v.write(file, tag);
}

//-----------------------------------------------------------------------------
// TypeDef<mpi>
//-----------------------------------------------------------------------------

TypeDef<mpi>::TypeDef() : TypeBase(this, TYPE_INTEGER, sizeof(mpi)) {}

bool TypeDef<mpi>::read(istream_ref file, mpi *p, Value &v) const {
	if (v.type != TYPE_INTEGER)
		return false;

	v.read_contents(file);
	p->load(v.get_buffer());
	return true;
}
bool TypeDef<mpi>::write(ostream_ref file, const mpi *p, uint32 tag) const {
	uint32	len	= (p->num_bits() + 8) / 8;
	Value	v;
	p->save(v.set(TYPE_INTEGER, len));
	return v.write(file, tag);
}

