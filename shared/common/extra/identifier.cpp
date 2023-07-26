#include "identifier.h"
#include "base/soft_float.h"
#include "base/bits.h"

namespace iso {

typed_nullptr<field>		fields<_none>::f;

string_accum& FormatIdentifier(string_accum &sa, const char *p, IDFMT format, IDTYPE type) {
	bool	prefixes = !!(format & (IDFMT_SEMANTIC_PREFIX|IDFMT_TYPE_PREFIX));
	if (format & IDFMT_STORAGE_PREFIX) {
		switch (type & IDTYPE_STORAGE_MASK) {
			case IDTYPE_MEMBER:	sa << "m_"; break;
			case IDTYPE_STATIC:	sa << "s_"; break;
			case IDTYPE_GLOBAL:	sa << "g_"; break;
		}
	}
	if (prefixes) {
		for (int n = type & IDTYPE_POINTER_MASK; --n;)
			sa << 'p';
	}
	if (format & IDFMT_SEMANTIC_PREFIX) {
		switch (type & IDTYPE_SEMANTIC_MASK) {
			case IDTYPE_DIFFERENCE:	sa << 'd'; break;
			case IDTYPE_COUNT:		sa << 'c'; break;
			case IDTYPE_RANGE:		sa << "rg"; break;
			case IDTYPE_INDEX:		sa << 'i'; break;
			case IDTYPE_BYTECOUNT:	sa << "cb"; break;
			case IDTYPE_WORDCOUNT:	sa << "cw"; break;
			case IDTYPE_DWORDCOUNT:	sa << "cdw"; break;
			default:				prefixes = false; break;
		}
	}
	switch (format & IDFMT_MASK) {
		case IDFMT_LEAVE:
			sa << p;
			break;

		case IDFMT_LOWER:
			if (prefixes)
				sa << '_';
			while (char c = *p++)
				sa << to_lower(c);
			break;

		case IDFMT_LOWER_NOUL:
			if (prefixes)
				sa << '_';
			while (char c = *p++) {
				if (c != '_')
					sa << to_lower(c);
			}
			break;

		case IDFMT_UPPER:
			while (char c = *p++)
				sa << to_upper(c);
			break;

		case IDFMT_UPPER_NOUL:
			while (char c = *p++) {
				if (c != '_')
					sa << to_upper(c);
			}
			break;

		case IDFMT_CAMEL:
			if (prefixes)
				sa << *p++;
			while (char c = *p++) {
				if (c == '_')
					c = to_upper(*p++);
				else
					c = to_lower(c);
				sa << c;
			}
			break;
	}
	return sa;
}

//-----------------------------------------------------------------------------

//bool

template<> const char *field_names<bool>::s[]		= {"false", "true"};
template<> const char *field_names<_not<bool>>::s[]	= {"true", "false"};
template<> field_value field_values<bool>::s[] = {
	"false",	false,
	"true",		true,
	0
};

//special

const char **field_names_none::s	= nullptr;
const char *field_names_hex::s[]	= {nullptr};
const char *field_names_signed::s[]	= {nullptr};
template<> const char *field_names<float>::s[]		= {nullptr};

const char *sString[]		= {0};
const char *sString16[]		= {0};
const char *sRelString[]	= {0};
const char *sDec[]			= {0};
const char *sEnable[] 		= {"disable", "enable"};
const char *sOn[] 			= {"off", "on"};
const char *sPowersOfTwo[] 	= {"1","2","4","8","16","32","64","128","256"};
const char *sCustom[]		= {"<custom>"};
field		float_field[]	= {{ 0, 0, 32, 0,0, sFloat}, 0};
field		empty_field[]	= {{0}};

template<> field value_field<void*>::f[] = {
	{0, 0, 64, 0, 0, sHex},
	{0, sizeof(void*) * 8}
};

field custom_ptr_field[] = {
	{0, 0, 64, field::MODE_CUSTOM, 0, sCustom},
	{0, sizeof(void*) * 8}
};

//-----------------------------------------------------------------------------

uint32 BiggestValue(const field_value *vals) {
	uint32 biggest = 0;
	for (const field_value *i = vals; i->name; i++) {
		if (i->value >= biggest)
			biggest = i->value;
	}
	return biggest;
}

const field_bit *BiggestFactor(const field_bit *vals, uint32 v) {
	const field_bit *biggest = 0;
	for (const field_bit *i = vals; i->name; i++) {
		if (i->value <= v && (!biggest || i->value >= biggest->value))
			biggest = i;
	}
	return biggest;
}

string_accum& PutConst(string_accum &a, IDFMT fmt, const char *const *p, int val, const char *prefix) {
	if (!p[val])
		return a << val;
	a << prefix << p[val];
	if (fmt & IDFMT_CONSTNUMS)
		a << '(' << val << ')';
	return a;
}

string_accum& PutConst(string_accum &a, IDFMT fmt, const field_value *p, int val, const char *prefix) {
	while (p->name) {
		if (val == p->value) {
			a << prefix << p->name;
			if (fmt & IDFMT_CONSTNUMS)
				a << "(0x" << hex(val) << ')';
			return a;
		}
		++p;
	}
	return a << "0x" << hex(val);
}

string_accum& PutConst(string_accum &a, IDFMT fmt, const field_bit *p, int val, const char *prefix, char sep) {
	bool	any		= false;
	int		val0	= val;
	while (const field_bit *i = BiggestFactor(p, val)) {
		if (any)
			a << sep;
		any = true;

		a << prefix << i->name;
		if (i->value == 0)
			break;

		int	mult = val / i->value;
		if (mult > 1)
			a << '*' << mult;

		val -= i->value * mult;
		if (val == 0)
			break;
	}
	if (any) {
		if (val)
			a << sep << "0x" << hex(val);
		if (fmt & IDFMT_CONSTNUMS)
			a << "(0x" << hex(val) << ')';
		return a;
	}

	return a << "0x" << hex(val);
}

string_accum &PutConst(string_accum &a, IDFMT fmt, const char *const *names, uint32 val, uint8 mode) {
	if (fmt & IDFMT_NONAMES)
		return a << val;

	const char *prefix = "";
	if (mode & field::MODE_PREFIX) {
		field_prefix<const char*>	*p = (field_prefix<const char*>*)names;
		if (!(fmt & IDFMT_NOPREFIX))
			prefix	= p->prefix;

		names	= p->names;
		mode	&= ~field::MODE_PREFIX;
	}

	if (!names)
		return a << val;

	switch (mode) {
		case field::MODE_NONE:		return PutConst(a, fmt, names, val, prefix);
		case field::MODE_VALUES:	return PutConst(a, fmt, (field_value*)names, val, prefix);
		case field::MODE_BITS:		return PutConst(a, fmt, (field_bit*)names, val, prefix);
	}
	return a;
}

string_accum &PutConst(string_accum &a, IDFMT fmt, const field *pf, uint64 val) {
	if (pf->values == sFloat) {
		switch (pf->num) {
			case 64: return a << (float64&)val;
			case 32: return a << (float32&)val;
			case 16: return a << float((float16&)val);
		}
	}
	if (pf->values == sString) {
		if (val)
			return a << '"' << (const char*)val << '"';//"<string>";//(const char*)val;
		return a << "<nil>";
	}
	if (pf->values == sString16) {
		if (val)
			return a << '"' << (const char16*)val << '"';//"<string>";//(const char*)val;
		return a << "<nil>";
	}
	if ((pf->num > 32 && pf->values == 0) || pf->values == sHex || pf->values == sCustom)
		return a << "0x" << hex(val);
	if (pf->values == sSigned)
		return a << int64(val);
	if (pf->values == sDec)
		return a << uint64(val);
	if (pf->num == 0)
		return a << "???";
//	if (pf->offset == 0 && pf->shift && val >= pf->shift)
//		return a << "<bad>";
	return PutConst(a, fmt, pf->values, val, pf->has_values() ? pf->offset : 0);
}

string_accum &PutConst(string_accum &a, IDFMT fmt, const field *pf, const uint32 *p, uint32 offset) {
	uint64	val	= pf->get_value(p, offset);
	if (pf->values == sRelString) {
		if (val) {
			const char *s = (const char*)p + (pf->start + offset) / 8 + val;
			if (pf->offset == 1)
				return a << (const wchar_t*)(s - 1);
			return a << s;
		}
		return a << "<nil>";
	}
	return PutConst(a, fmt, pf, val);
}

string_accum &PutFieldName(string_accum &sa, IDFMT fmt, const field *pf, const char *prefix) {
	if (pf->name)
		sa << Identifier(prefix, pf->name, fmt) << (fmt & IDFMT_NOSPACES ? "=" : " = ");
	return sa;
}

string_accum &PutField(string_accum &sa, IDFMT fmt, const field *pf, uint64 val, const char *prefix) {
	return PutConst(PutFieldName(sa, fmt, pf, prefix), fmt, pf, val);
}

string_accum &PutField(string_accum &sa, IDFMT fmt, const field *pf, const uint32 *p, uint32 offset, const char *prefix) {
	return PutConst(PutFieldName(sa, fmt, pf, prefix), fmt, pf, p, offset);
}

string make_prefix(const char *prefix1, const char *prefix2, char sep) {
	return !prefix1 ? str(prefix2) : sep ? str(prefix1) + str(sep) + prefix2 : str(prefix1) + prefix2;
}

string_accum &PutFields(string_accum &sa, IDFMT fmt, const field *pf, uint64 val, const char* separator, uint64 mask, const char *prefix) {
	if (pf == float_field) {
		sa << onlyif(!(fmt & IDFMT_NOSPACES), '\t') << (float&)val;

	} else while (!pf->is_terminator()) {
		if (pf->num == 0)
			PutFields(sa, fmt, (const field*)pf->values, val >> pf->start, separator, mask, make_prefix(prefix, pf->name, pf->offset & field::MODE_DOT ? '.' : 0));
		else if (mask & bits(pf->num, pf->start))
			PutField(sa << onlyif(!(fmt & IDFMT_NOSPACES), '\t'), fmt, pf, pf->get_value(val), prefix);
		++pf;
		if (pf->name || pf->num)
			sa << separator;
	}
	return sa;
}

string_accum &PutFields(string_accum &sa, IDFMT fmt, const field *pf, const uint32 *p, uint32 offset, const char* separator, const char *prefix) {
	while (!pf->is_terminator()) {
		if (pf->num == 0) {
			PutFields(sa, fmt, pf->get_call(p, offset), pf->get_ptr(p, offset), pf->get_offset(offset), separator, make_prefix(prefix, pf->name, pf->offset & field::MODE_DOT ? '.' : 0));
		} else {
			PutField(sa, fmt, pf, p, offset, prefix);
		}
		++pf;
		if (pf->name || pf->num)
			sa << separator;
	}
	return sa;
}

//-----------------------------------------------------------------------------
//FieldPutter
//-----------------------------------------------------------------------------

void FieldPutter::AddHex(const uint32 *vals, uint32 n, uint32 addr) {
	while (n--) {
		Line(0, format_string("0x%08x", *vals++), addr);
		addr += 4;
	}
}

void FieldPutter::AddArray(const field *pf, const uint32le *p, uint32 stride, uint32 n, uint32 addr) {
	if (stride == 0) {
		uint32	s = Terminator(pf)->start;
		ISO_ASSERT(s);
		if (s % 32) {
			AddHex(p, n * s / 4, addr);
			return;
		}
		stride = s / 32;
	}

	for (uint32 i = 0; i < n; i++) {
		Open(format_string("[%i]", i), addr + stride * i * 4);
		AddFields(pf, p + stride * i, addr + stride * i * 4, 0);
		Close();
	}
}

void FieldPutter::AddPointer(const field *pf, const uint32 *p, uint32 addr, uint32 offset, const field *pf2, const uint32 *p2) {
	buffer_accum<64>	ba(FieldName(pf));
	if (p2) {
		if (pf->shift)
			ba << '[' << pf->get_companion_value(p, offset) << ']';
		else
			ba << "->";
		Open(ba, addr);
		if (format & IDFMT_FOLLOWPTR) {
			if (pf->shift)
				AddArray(pf2, p2, 0, pf->get_companion_value(p, offset), addr);
			else
				AddFields(pf2, p2, addr, 0);
		}
		Close();
	} else {
		Line(ba, "0", addr);
	}
}

void FieldPutter::AddField(const field *pf, const uint32 *p, uint32 addr, uint32 offset) {
	if (pf->num == 0) {
		switch (pf->offset) {
			case field::MODE_CALL:
				if (pf->name)
					Open(FieldName(pf), addr);
				AddFields(pf->get_call(p, offset), p, addr, offset + pf->start);
				if (pf->name)
					Close();
				break;

			case field::MODE_POINTER:
				AddPointer(pf, p, addr, offset, (const field*)pf->values, ((uint32le**)p)[(offset + pf->start) / 64]);
				break;

			case field::MODE_RELPTR: {
				int32le		*p0 = (int32le*)((char*)p + (offset + pf->start) / 8);
				AddPointer(pf, p, addr, offset, (const field*)pf->values, *p0 ? (uint32le*)((char*)p0 + *p0) : nullptr);
				break;
			}
			case field::MODE_CUSTOM_PTR: {
				auto ret = make_unique(((field_follow_func)pf->values)(pf, p, offset));
				AddPointer(pf, p, addr, offset, ret ? ret->pf : nullptr, ret ? ret->p : nullptr);
				break;
			}
		}

	} else if (pf->offset == field::MODE_CUSTOM) {
		if (pf->shift)
			Line(FieldName(pf), ((field_callback_func*)pf->values)(lvalue(buffer_accum<256>()), pf, p, offset, addr + offset), addr + offset);
		else
			Callback(pf, p, offset, addr);
	} else {
		Line(FieldName(pf), PutConst(lvalue(buffer_accum<256>()), format, pf, p, offset), addr + offset);
	}
}

void FieldPutter::AddFields(const field* pf, const uint32* p, uint32 addr, uint32 offset) {
	if (pf == float_field) {
		Line(0, buffer_accum<256>() << (float&)p[offset / 32], addr);

	} else if (pf) {
		while (!pf->is_terminator()) {
			AddField(pf, p, addr, offset);
			pf++;
		}
	}
}
/*
struct StringPutter {
	string_accum	&sa;
	const char* separator;
	void	Open(const char* title, uint32 addr) {}
	void	Close() {}
	void	Line(const char* name, const char* value, uint32 addr) {
		if (name)
			sa << name << "=";
		sa << value << separator;
	}
	void	Callback(const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
		Line(pf->name, format_string("0x") << hex(pf->get_raw_value(p, offset)), addr);
	}
	StringPutter(string_accum &sa, const char* separator) : sa(sa), separator(separator) {}
};

string_accum &PutFields(string_accum &sa, IDFMT fmt, const field *pf, uint64 val, const char* separator, uint32 mask, const char *prefix) {
	FieldPutter(StringPutter(sa, separator), fmt).AddFields(pf, &val);
	return sa;
}

string_accum &PutFields(string_accum &sa, IDFMT fmt, const field *pf, const uint32 *p, uint32 offset, const char* separator, const char *prefix) {
	FieldPutter(StringPutter(sa, separator), fmt).AddFields(pf, p);
	return sa;
}
*/
//-----------------------------------------------------------------------------

uint64 GetConst(string_scan &ss, const char *const *names, int bits, uint8 mode) {
	if (is_digit(ss.peekc()) ||ss.peekc() == '-')
		return ss.get<int64>();

	if (mode & field::MODE_PREFIX) {
		field_prefix<const char*>	*p = (field_prefix<const char*>*)names;
		if (!ss.scan(make_case_insensitive(p->prefix)))
			return false;
		names	= p->names;
		mode	&= ~field::MODE_PREFIX;
	}

	switch (mode) {
		case field::MODE_NONE:// direct index
			if (names) {
				int	n = 1 << bits;
				for (int i = 0; i < n; i++) {
					if (names[i] && ss.scan(make_case_insensitive(names[i])))
						return i;
				}
			}
			break;

		case field::MODE_VALUES:// table of field_value
			for (const field_value *values = (field_value*)names; values->name; values++) {
				if (ss.scan(make_case_insensitive(values->name)))
					return values->value;
			}
			break;
	}
	return 0;
}

uint64 GetField(string_scan &ss, const field *pf) {
	if (pf->values == sFloat)
		return force_cast<uint32>(ss.get<float>());

	if (pf->values == sString || pf->values == sString16)
		return 0;

	return GetConst(ss, pf->values, pf->num, pf->has_values() ? pf->offset : 0);
}

void GetField(string_scan &ss, const field *pf, uint32 *p, uint32 offset) {
	pf->set_value(p, GetField(ss, pf), offset);
}

//-----------------------------------------------------------------------------

const field *_Index(const field *pf, int &i, uint32 &offset) {
	if (pf) {
		while (!pf->is_terminator()) {
			if (pf->num == 0) {
				if (const field *ret = _Index(pf->get_call(), i, offset)) {
					offset += pf->start;
					return ret;
				}
			} else {
				if (i-- == 0)
					return pf;
			}
			pf++;
		}
	}
	return 0;
}

field FieldIndex(const field *pf, int i) {
	field	f;
	clear(f);

	uint32 offset = 0;
	if (const field *ret = _Index(pf, i, offset)) {
		f = *ret;
		f.start += offset;
	}
	return f;
}

const field *_Index(const field *pf, int &i, const uint32 *&p, uint32 &offset, bool follow_ptr);

const field *_IndexArray(const field *pf, int &i, const uint32 *&p, uint32 &offset, uint32 stride, uint32 n) {
	if (stride == 0) {
		stride = Terminator(pf)->start;
		ISO_ASSERT(stride);
	}

	for (uint32 x = 0; x < n; x++) {
		uint32	offset2 = offset + stride * x;
		if (auto *f = _Index(pf, i, p, offset2, true)) {
			offset = offset2;
			return f;
		}
	}
	return 0;
}

const field *_Index(const field *pf, int &i, const uint32 *&p, uint32 &offset, bool follow_ptr) {
	if (pf) {
		while (!pf->is_terminator()) {
			if (pf->num == 0) {
				switch (pf->offset) {
					case field::MODE_NONE: {
						uint32	offset2 = offset + pf->start;
						if (const field	*ret = _Index(pf->get_call(p, offset), i, p, offset2, follow_ptr)) {
							offset	= offset2;
							return ret;
						}
						break;
					}
					case field::MODE_POINTER: {
						const uint32	*p2 = ((uint32le**)p)[(offset + pf->start) / 64];
						if (p2 && follow_ptr) {
							uint32	offset2	= 0;
							const field	*ret = pf->shift
								? _IndexArray((const field*)pf->values, i, p2, offset2, 0, pf->get_companion_value(p, offset))
								: _Index((const field*)pf->values, i, p2, offset2, follow_ptr);
							if (ret) {
								p		= p2;
								offset	= offset2;
								return ret;
							}
						}
						break;
					}
					case field::MODE_RELPTR: {
						const int32le	*p0 = (int32le*)((char*)p + (offset + pf->start) / 8);
						if (*p0 && follow_ptr) {
							uint32	offset2	= 0;
							const uint32	*p2	= (uint32le*)((char*)p0 + *p0);
							const field		*ret = pf->shift
								? _IndexArray((const field*)pf->values, i, p2, offset2, 0, pf->get_companion_value(p, offset))
								: _Index((const field*)pf->values, i, p2, offset2, follow_ptr);
							if (ret) {
								p		= p2;
								offset	= offset2;
								return ret;
							}
						}
						break;
					}
					case field::MODE_CUSTOM_PTR:
						if (follow_ptr) {
							if (auto ret = make_unique(((field_follow_func)pf->values)(pf, p, offset)))
								return ret->pf;
						}
						break;
				}

			} else {
				if (i-- == 0)
					return pf;
			}
			pf++;
		}
	}
	return 0;
}

const field *FieldIndex(const field *pf, int i, const uint32 *&p, uint32 &offset, bool follow_ptr) {
	return _Index(pf, i, p, offset, follow_ptr);
}

const field *_FindField(const field *pf, int bit, uint32 &offset) {
	while (!pf->is_terminator()) {
		if (pf->num == 0) {
			if (const field *ret = _FindField(pf->get_call(), bit - pf->start, offset)) {
				offset += pf->start;
				return ret;
			}
		} else {
			if (bit <= offset + pf->start)
				return pf;
		}
		pf++;
	}
	return 0;
}

field FindField(const field *pf, uint32 bit) {
	field	f;
	clear(f);

	if (pf) {
		uint32 offset = 0;
		if (const field *ret = _FindField(pf, bit, offset)) {
			f = *ret;
			f.start += offset;
		}
	} else {
		f.start	= bit;
		f.num	= 32 - bit;
	}
	return f;
}

uint32 TotalBits(const field *pf) {
	uint32	end = 0;
	while (!pf->is_terminator()) {
		uint32	n = pf->num == 0 ? TotalBits(pf->get_call()) : pf->num;
		end = max(end, pf->start + n);
		pf++;
	}
	return end;
}

const field *Terminator(const field *pf) {
	while (!pf->is_terminator())
		++pf;
	return pf;
}

}