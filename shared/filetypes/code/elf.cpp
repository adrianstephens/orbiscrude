#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "base/hash.h"
#include "obj.h"
#include "elf.h"
#include "dwarf.h"
#include "vm.h"

using namespace iso;

template<int bits> using iso_mem = ISO::VStartBin<ISO_openarray<xint8>, constructable<baseint<16, uint_bits_t<bits>>>>;

template<int bits> memory_block	GetSection(ISO_openarray<ISO_ptr<iso_mem<bits>>> &array, const char *name) {
	auto p = array[name];
	if (p)
		return p->b;
	p = array[string("__") + (name + 1)];
	if (p)
		return p->b;
	return none;
}

template<int N> struct depth_tester {
	uint32		&i;
	depth_tester(uint32 &_i, uint32 &m)	: i(_i) { m = max(m, ++i); }
	~depth_tester()			{ --i; }
	operator bool()	const	{ return i < N; }
};

template<bool be, int bits> struct ISO::def<Elf_Sym<be, bits> > : public TypeUserCompN<6> { typedef Elf_Sym<be, bits> _S, _T; def() : TypeUserCompN<6>("Elf_Sym") {
	ISO_SETFIELDS5(0, st_value, st_size, st_info, st_other,st_shndx);
} };

//-----------------------------------------------------------------------------
//	DWARF
//-----------------------------------------------------------------------------

struct Dwarf_file {
	string				dir;
};
struct Dwarf_lineinfo {
	xint64				address;
	ISO_ptr<Dwarf_file>	file;
	uint32				line;
	uint8				flags;
};
struct Dwarf_AddressRange {
	xint64				segment;
	xint64				address;
	xint64				length;
};
struct Dwarf_Directive {
	uint32				line;
	string				text;
};
struct Dwarf_Include {
	uint32				line;
	ISO_ptr<Dwarf_file>	file;
	anything			children;
};
struct Dwarf_range {
	xint64				start, end;
};
struct Dwarf_loc {
	xint64				start, end;
	ISO_openarray<xint8> expr;
};
ISO_DEFUSERCOMPV(Dwarf_file, dir);
ISO_DEFUSERCOMPV(Dwarf_lineinfo, address, file, line, flags);
ISO_DEFUSERCOMPV(Dwarf_AddressRange, segment, address, length);
ISO_DEFUSERCOMPV(Dwarf_Directive, line, text);
ISO_DEFUSERCOMPV(Dwarf_Include, line, file, children);
ISO_DEFUSERCOMPV(Dwarf_range, start, end);
ISO_DEFUSERCOMPV(Dwarf_loc, start, end, expr);

namespace dwarf {

struct ISO_type_userint : ISO::TypeUser, ISO::TypeInt {
	ISO_type_userint(tag2 _id, int bits) : ISO::TypeUser(_id, (ISO::TypeInt*)this), ISO::TypeInt(bits, 0, ISO::TypeInt::ENUM)  {}
};
struct ISO_type_userenum {ISO_type_userint i; ISO::Enum e[1]; };
template<int N> struct TISO_type_userenum {ISO_type_userint i; ISO::Enum e[N]; ISO_type_userenum *operator&() { return (ISO_type_userenum*)this;} };
#define ENUMH(n,b,c)	TISO_type_userenum<c+1> dw_##n = {ISO_type_userint("Dwarf_" #n, b), {
#define ENUMT		{tag(),0} } }
#define ENUM(p,x)	{tag(#x), p##_##x}
ENUMH(encoding,8,18)
	ENUM(ATE, address),				ENUM(ATE, boolean),				ENUM(ATE, complex_float),		ENUM(ATE, float),				ENUM(ATE, signed),				ENUM(ATE, signed_char),			ENUM(ATE, unsigned),			ENUM(ATE, unsigned_char),
	ENUM(ATE, imaginary_float),		ENUM(ATE, packed_decimal),		ENUM(ATE, numeric_string),		ENUM(ATE, edited),				ENUM(ATE, signed_fixed),		ENUM(ATE, unsigned_fixed),		ENUM(ATE, decimal_float),		ENUM(ATE, UTF),
	ENUM(ATE, lo_user),				ENUM(ATE, hi_user),
ENUMT;
ENUMH(decimal_sign,8,5)				ENUM(DS, unsigned),				ENUM(DS, leading_overpunch),	ENUM(DS, trailing_overpunch),	ENUM(DS, leading_separate),		ENUM(DS, trailing_separate),	ENUMT;
ENUMH(endianity,8,5)				ENUM(END, default),				ENUM(END, big),					ENUM(END, little),				ENUM(END, lo_user),				ENUM(END, hi_user),				ENUMT;
ENUMH(accessibility,8,3)			ENUM(ACCESS, public),			ENUM(ACCESS, protected),		ENUM(ACCESS, private),			ENUMT;
ENUMH(visibility,8,3)				ENUM(VIS, local),				ENUM(VIS, exported),			ENUM(VIS, qualified),			ENUMT;
ENUMH(virtuality,8,3)				ENUM(VIRTUALITY, none),			ENUM(VIRTUALITY, virtual),		ENUM(VIRTUALITY, pure_virtual),	ENUMT;
ENUMH(language,16,39)
	ENUM(LANG, C89),				ENUM(LANG, C),					ENUM(LANG, Ada83),				ENUM(LANG, C_plus_plus),		ENUM(LANG, Cobol74),			ENUM(LANG, Cobol85),			ENUM(LANG, Fortran77),			ENUM(LANG, Fortran90),
	ENUM(LANG, Pascal83),			ENUM(LANG, Modula2),			ENUM(LANG, Java),				ENUM(LANG, C99),				ENUM(LANG, Ada95),				ENUM(LANG, Fortran95),			ENUM(LANG, PLI),				ENUM(LANG, ObjC),
	ENUM(LANG, ObjC_plus_plus),		ENUM(LANG, UPC),				ENUM(LANG, D),					ENUM(LANG, Python),				ENUM(LANG, OpenCL),				ENUM(LANG, Go),					ENUM(LANG, Modula3),			ENUM(LANG, Haskell),
	ENUM(LANG, C_plus_plus_03),		ENUM(LANG, C_plus_plus_11),		ENUM(LANG, OCaml),				ENUM(LANG, Rust),				ENUM(LANG, C11),				ENUM(LANG, Swift),				ENUM(LANG, Julia),				ENUM(LANG, Dylan),
	ENUM(LANG, C_plus_plus_14),		ENUM(LANG, Fortran03),			ENUM(LANG, Fortran08),			ENUM(LANG, RenderScript),		ENUM(LANG, BLISS),
	ENUM(LANG, lo_user),			ENUM(LANG, hi_user),			ENUMT;
ENUMH(identifier_case,8,4)			ENUM(ID, case_sensitive),		ENUM(ID, up_case),				ENUM(ID, down_case),			ENUM(ID, istr),					ENUMT;
ENUMH(calling_convention,8,5)		ENUM(CC, normal),				ENUM(CC, program),				ENUM(CC, nocall),				ENUM(CC, lo_user),				ENUM(CC, hi_user),				ENUMT;
ENUMH(inline,8,4)					ENUM(INL, not_inlined),			ENUM(INL, inlined),				ENUM(INL, declared_not_inlined),ENUM(INL, declared_inlined),	ENUMT;
ENUMH(ordering,8,2)					ENUM(ORD, row_major),			ENUM(ORD, col_major),			ENUMT;
//ENUMH(dw_DSC,8,2)					ENUM(DSC, label),				ENUM(DSC, range),				ENUMT;
#undef 	ENUM
#undef 	ENUMH
#undef 	ENUMT

tag GetID(ATTRIBUTE attr) {
	if (attr < _AT_table_size && attr_ids[attr])
		return attr_ids[attr];

	if (attr >= AT_lo_user && attr <= AT_hi_user)
		return format_string("user_attr_0x%04x", attr);

	return format_string("attr_0x%04x", attr);
}

tag GetID(TAG name) {
	if (name < _TAG_table_size && tag_ids[name])
		return tag_ids[name];
	return format_string("Dwarf_0x%04x", name);
}

const ISO::Type *GetType(FORM form, uint8 addr_size, bool big) {
	switch (form) {
		case FORM_addr:			return addr_size > 4 ? ISO::getdef<uint64>() : ISO::getdef<uint32>();

		case FORM_block1:
		case FORM_block2:
		case FORM_block4:
		case FORM_block:		return ISO::getdef<ISO_openarray<xint8> >();

		case FORM_data1:		return ISO::getdef<uint8>();
		case FORM_data2:		return ISO::getdef<uint16>();
		case FORM_data4:		return ISO::getdef<uint32>();
		case FORM_data8:		return ISO::getdef<uint64>();
		case FORM_flag:			return ISO::getdef<bool8>();
		case FORM_sdata:		return ISO::getdef<int64>();
		case FORM_udata:		return ISO::getdef<uint64>();

		case FORM_strp:
		case FORM_string:		return ISO::getdef<string>();

		case FORM_indirect:
		case FORM_ref_addr:
		case FORM_ref_udata:
		case FORM_ref1:
		case FORM_ref2:
		case FORM_ref4:
		case FORM_ref8:			return ISO::getdef<ISO_ptr<void> >();

		case FORM_sec_offset:	return big ? ISO::getdef<uint64>() : ISO::getdef<uint32>();

		case FORM_exprloc:		return ISO::getdef<ISO_openarray<xint8> >();
		case FORM_flag_present:	return ISO::getdef<bool8>();
		case FORM_ref_sig8:		return ISO::getdef<uint64>();
		default: return 0;
	}
}
const ISO::Type *GetType(ATTRIBUTE attr, FORM form, uint8 addr_size, bool big) {
	const ISO_type_userenum	*e = 0;
	switch (attr) {
		case AT_encoding:			e = &dw_encoding;			break;
		case AT_decimal_sign:		e = &dw_decimal_sign;		break;
		case AT_endianity:			e = &dw_endianity;			break;
		case AT_accessibility:		e = &dw_accessibility;		break;
		case AT_visibility:			e = &dw_visibility;			break;
		case AT_virtuality:			e = &dw_virtuality;			break;
		case AT_language:			e = &dw_language;			break;
		case AT_identifier_case:	e = &dw_identifier_case;	break;
		case AT_calling_convention:	e = &dw_calling_convention;	break;
		case AT_inline:				e = &dw_inline;				break;
		case AT_ordering:			e = &dw_ordering;			break;
//		case AT_discr_list:			e = &dw_DSC;				break;

		case AT_name:				return 0;
		case AT_decl_file:			return ISO::getdef<ISO_ptr<Dwarf_file> >();
	}
	if (e && form == (e->i.num_bits() == 8 ? FORM_data1 : e->i.num_bits() == 16 ? FORM_data2 : 0))
		return (const ISO::Type*)e;

	if (form == FORM_data4 || form == FORM_data8 || form == FORM_sec_offset) {
		switch (GetClass(attr) & _ATC_refmask) {
			case ATC_loclistptr:	return ISO::getdef<ISO_openarray<Dwarf_loc> >();
			case ATC_rangelistptr:	return ISO::getdef<ISO_openarray<Dwarf_range> >();
			case ATC_lineptr:		return ISO::getdef<ISO_openarray<Dwarf_lineinfo> >();
			case ATC_macptr:		return ISO::getdef<ISO_openarray<anything> >();
		}
	}

	return GetType(form, addr_size, big);
}

class Reader : Sections, endian_reader, public refs<Reader> {
	struct Attribute {
		ATTRIBUTE	attr;
		FORM		form;
		Attribute(ATTRIBUTE _attr, FORM _form) : attr(_attr), form(_form)	{}
	};
	struct Abbreviation {
		bool		children;
		dynamic_array<Attribute> attr;
		ISO::TypeUser	*type;
		Abbreviation() : type(0)	{}
	};

	ISO_ptr<Dwarf_file>	*files;
	size_t				num_files;
	uint32				recursion_depth, recursion_max;

	hash_map<const uint8*, ISO_ptr<void> >	refs;

public:
	struct info_unit : dwarf::info_unit, iso::refs<info_unit> {
		ref_ptr<Reader>					reader;
		hash_map<uint16, Abbreviation>	abbrs;
		info_unit(Reader *_reader, byte_reader &b, bool bigendian, const uint8 *start);
	};

	ISO_ptr<Dwarf_file>	GetFile(int i) {
		return i && i <= num_files ? files[i - 1] : ISO_ptr<Dwarf_file>();
	}
	void			ReadBlock(void *p, byte_reader &b, uint32 size) {
		ISO_openarray<uint8>	*a = new(p) ISO_openarray<uint8>;
		b.readbuff(a->Create(size), size);
	}

	ISO_ptr<void>	ReadRef(const uint8 *ref, info_unit *unit, bool deferred);
	void			ReadForm(void *p, byte_reader &b, info_unit *unit, FORM form, bool deferred);
	ISO_ptr<void>	ReadDIE(byte_reader &b, info_unit *unit, bool always_read, bool deferred);

	void			Read_loclistptr(const const_memory_block &m, ISO_openarray<Dwarf_loc> &a, uint8 addr_size);
	void			Read_rangelistptr(const const_memory_block &m, ISO_openarray<Dwarf_range> &a, uint8 addr_size);
	void			Read_lineptr(const const_memory_block &m, ISO_openarray<Dwarf_lineinfo> &a, uint8 addr_size);
	void			Read_macptr(const const_memory_block &m, anything &a);


	Reader(Sections &_sections, bool _bigendian) : Sections(_sections), endian_reader(_bigendian), files(0) {}

	ISO_ptr<void>	ReadInfo(tag id, bool deferred);
	ISO_ptr<void>	ReadPubNames(tag id);
	ISO_ptr<void>	ReadAddressRanges(tag id);
};

ISO_ptr<void> ReadDwarf(Sections &_sections, bool be) {
	ref_ptr<dwarf::Reader>	dr = new dwarf::Reader(_sections, be);
	return dr->ReadInfo("DWARF", true);
}

Reader::info_unit::info_unit(Reader *_reader, byte_reader &b, bool bigendian, const uint8 *start) : reader(_reader) {
	read(b, bigendian);
	byte_reader ba(start + abbr_off);
	while (uint64 code = ba.get<leb128<uint32> >()) {
		Abbreviation	&a	= abbrs[code];
		tag				id	= GetID(TAG(get_leb128<uint16>(ba)));
		a.children			= ba.getc() != 0;

		ISO::TypeCompositeN<256>	builder(0);
		for (;;) {
			ATTRIBUTE	attr	= ATTRIBUTE(get_leb128<uint32>(ba));
			FORM		form	= FORM(get_leb128<uint32>(ba));
			if (attr == 0 && form == 0)
				break;

			a.attr.emplace_back(attr, form);
			if (const ISO::Type *type = GetType(attr, form, addr_size, large))
				builder.Add(type, GetID(attr));
		}

		if (a.children)
			builder.Add<anything>("children");

//		a.type	= new ISO::TypeUserSave(id, builder.Duplicate());
		a.type	= new ISO::TypeUser(id, builder.Duplicate());
	}
}

void Reader::Read_loclistptr(const const_memory_block &m, ISO_openarray<Dwarf_loc> &a, uint8 addr_size) {
	for (byte_reader b(m); b.p < m.end();) {
		uint64	start	= get_val(b, addr_size);
		uint64	end		= get_val(b, addr_size);
		if (start == 0 && end == 0)
			break;
		uint16	len		= get<uint16>(b);
		Dwarf_loc	&ar = a.Append();
		ar.start		= start;
		ar.end			= end;
		b.readbuff(ar.expr.Create(len), len);
	}
}
void Reader::Read_rangelistptr(const const_memory_block &m, ISO_openarray<Dwarf_range> &a, uint8 addr_size) {
	for (byte_reader b(m); b.p < m.end();) {
		uint64	start	= get_val(b, addr_size);
		uint64	end		= get_val(b, addr_size);
		if (start == 0 && end == 0)
			break;
		Dwarf_range	&ar = a.Append();
		ar.start		= start;
		ar.end			= end;
	}
}
void Reader::Read_lineptr(const const_memory_block &m, ISO_openarray<Dwarf_lineinfo> &a, uint8 addr_size) {
	byte_reader		b(m);
	line_machine	dlm(bigendian, addr_size);
	line_unit		unit;
	unit.read(b, bigendian);

	dlm.flags.set(line_machine::is_stmt, !!unit.h->default_is_stmt);
	dlm.header(unit.h, b);
	while (b.p < unit.end)
		dlm.op(unit.h, b);

	num_files	= dlm.files.size();
	ISO_ptr<Dwarf_file>	*fd	= files	= new ISO_ptr<Dwarf_file>[num_files];
	for (file_info *i = dlm.files.begin(), *e = dlm.files.end(); i != e; ++i, ++fd) {
		Dwarf_file	*f	= fd->Create(i->name);
		if (i->dir)
			f->dir = dlm.dirs[i->dir - 1];
	}

	Dwarf_lineinfo		*line	= a.Create(uint32(dlm.lines.size()));
	for (line_info *i = dlm.lines.begin(), *e = dlm.lines.end(); i != e; ++i, ++line) {
		line->address	= i->address;
		if (i->file)
			line->file = files[i->file - 1];
		line->line		= i->line;
		line->flags		= i->flags;
	}
}

void Reader::Read_macptr(const const_memory_block &m, anything &a) {
	anything*	stack[64], **sp = stack;
	*sp = &a;

	for (byte_reader b(m); b.p < m.end() && b.peekc();) {
		switch (b.getc()) {
			case MACINFO_define: {
				ISO_ptr<Dwarf_Directive>	d("define");
				d->line	= b.get<leb128<uint32> >();
				read(b, d->text);
				(*sp)->Append(d);
				break;
			}
			case MACINFO_undef: {
				ISO_ptr<Dwarf_Directive>	d("undefine");
				d->line	= b.get<leb128<uint32> >();
				read(b, d->text);
				(*sp)->Append(d);
				break;
			}
			case MACINFO_start_file: {
				ISO_ptr<Dwarf_Include>		d("include");
				d->line	= b.get<leb128<uint32> >();
				d->file = GetFile(b.get<leb128<uint32> >());
				(*sp)++->Append(d);
				*sp		= &d->children;
				break;
			}
			case MACINFO_end_file:
				--sp;
				break;
			case MACINFO_vendor_ext: {
				ISO_ptr<Dwarf_Directive>	d("vendor");
				d->line	= b.get<leb128<uint32> >();
				read(b, d->text);
				(*sp)->Append(d);
				break;
			}
		}
	}
}

struct DWARF_die : ISO::VirtualDefaults {
	ref_ptr<Reader::info_unit>	unit;
	const uint8					*data;

	DWARF_die(Reader::info_unit *_unit, const uint8 *_data) : unit(_unit), data(_data) {
	}

	ISO_ptr<void>	Deref() {
		byte_reader	b(data);
		return unit->reader->ReadDIE(b, unit, false, true);
	}
};

ISO_ptr<void> Reader::ReadRef(const uint8 *ref, info_unit *unit, bool deferred) {
	if (deferred) {
		return ISO_ptr<DWARF_die>(tag(), DWARF_die(unit, ref));
	} else {
		byte_reader	b(ref);
		return depth_tester<100>(recursion_depth, recursion_max) ? ReadDIE(b, unit, false, false) : ISO_NULL;
	}
}

void Reader::ReadForm(void *p, byte_reader &b, info_unit *unit, FORM form, bool deferred) {
	if (form == FORM_indirect) {
		p		= new(p) ISO_ptr<void>(0);
		form	= FORM(get_leb128<uint32>(b));
	}

	switch (form) {
		case FORM_addr:
			if (unit->addr_size > 4)
				new(p) uint64(get<uint64>(b));
			else
				new(p) uint32(get<uint32>(b));
			break;
		case FORM_block1:		ReadBlock(p, b, b.get<uint8>()); break;
		case FORM_block2:		ReadBlock(p, b, get<uint16>(b)); break;
		case FORM_block4:		ReadBlock(p, b, get<uint32>(b)); break;
		case FORM_block:		ReadBlock(p, b, b.get<leb128<uint32> >()); break;

		case FORM_flag:			new(p) bool8 (b.getc() != 0); break;
		case FORM_data1:		new(p) uint8 (b.get<uint8>()); break;
		case FORM_data2:		new(p) uint16(get<uint16>(b)); break;
		case FORM_data4:		new(p) uint32(get<uint32>(b)); break;
		case FORM_data8:		new(p) uint64(get<uint64>(b)); break;
		case FORM_sdata:		new(p) int64 (b.get<leb128<int64> >()); break;
		case FORM_udata:		new(p) uint64(b.get<leb128<uint64> >()); break;

		case FORM_string: {
			auto bs = build(*new(p) string);
			while (char c = b.getc())
				bs.putc(c);
			break;
		}
		case FORM_strp: {
			uint32	off = get<uint32>(b);
			if (off < str.size())
				new(p) string((const char*)str.p + off);
			break;
		}
		case FORM_ref1:			new(p) ISO_ptr<void>(ReadRef(unit->start + b.get<uint8>(), unit, deferred)); break;
		case FORM_ref2:			new(p) ISO_ptr<void>(ReadRef(unit->start + get<uint16>(b), unit, deferred)); break;
		case FORM_ref4:			new(p) ISO_ptr<void>(ReadRef(unit->start + get<uint32>(b), unit, deferred)); break;
		case FORM_ref8:			new(p) ISO_ptr<void>(ReadRef(unit->start + get<uint64>(b), unit, deferred)); break;
		case FORM_ref_udata:	new(p) ISO_ptr<void>(ReadRef(unit->start + b.get<leb128<uint64> >(), unit, deferred)); break;
		case FORM_ref_addr:		new(p) ISO_ptr<void>(ReadRef(info + b.get<leb128<uint64> >(), unit, deferred)); break;

		case FORM_sec_offset:
			if (unit->large)
				new(p) uint64(get<uint64>(b));
			else
				new(p) uint32(get<uint32>(b));
			break;

		case FORM_exprloc:		ReadBlock(p, b, b.get<leb128<uint32> >()); break;
		case FORM_flag_present:	new(p) bool8(true); break;
		case FORM_ref_sig8:		new(p) xint64(get<uint64>(b));
		default:				ISO_TRACE("Not implemented\n"); break;
	}
}

ISO_ptr<void> Reader::ReadDIE(byte_reader &b, info_unit *unit, bool always_read, bool deferred) {

	dynamic_array<anything*> stack;
	anything	*children	= 0;
	const uint8	*x0			= b.p;

	for (;;) {
		const uint8		*x	= b.p;
		ISO_ptr<void>	&rp	= refs[x];
		ISO_ptr<void>	p;

		if (!p || always_read) {
			uint32				ai	= b.get<leb128<uint32> >();
			const Abbreviation	*a	= unit->abbrs.check(ai);
			if (a && !a->type) {
				ISO_TRACEF("DIE stopped at ") << ai << '\n';
				break;
			}

			if (a && a->type) {
				p = ISO::MakePtr(a->type);

				if (!rp)
					rp = p;

				uint8				*data		= p;
				ISO::TypeComposite	*comp		= (ISO::TypeComposite*)a->type->subtype.get();
				const ISO::Element	*element	= comp->begin();
				for (const Attribute *i = a->attr.begin(), *e = a->attr.end(); i != e; ++i, ++element) {
					switch (i->attr) {
						case AT_name: {
							string	s;
							ReadForm(&s, b, unit, i->form, deferred);
							p.SetID(s);
							--element;
							break;
						}
						case AT_decl_file: {
							uint64	v = 0;
							ReadForm(&v, b, unit, i->form, deferred);
							new(data + element->offset) ISO_ptr<Dwarf_file>(GetFile(int(v)));
							break;
						}
						default: {
							uint8	ref = GetClass(i->attr) & _ATC_refmask;
							if (ref >= ATC_loclistptr && (i->form == FORM_data4 || i->form == FORM_data8 || i->form == FORM_sec_offset)) {
								uint64	v = i->form == FORM_data8 || (i->form == FORM_sec_offset && unit->large) ? get<uint64>(b) : get<uint32>(b);
								switch (ref) {
									case ATC_loclistptr:
										if (v < loc.size())
											Read_loclistptr(loc + v, *new(data + element->offset) ISO_openarray<Dwarf_loc>, unit->addr_size);
										break;
									case ATC_rangelistptr:
										if (v < ranges.size())
											Read_rangelistptr(ranges + v, *new(data + element->offset) ISO_openarray<Dwarf_range>, unit->addr_size);
										break;
									case ATC_lineptr:
										if (v == 0)
										//if (v < line.size)
											Read_lineptr(line + v, *new(data + element->offset) ISO_openarray<Dwarf_lineinfo>, unit->addr_size);
										break;
									case ATC_macptr:
										if (v < macinfo.size())
											Read_macptr(macinfo + v, *new(data + element->offset) anything);
										break;
								}
							} else {
								ReadForm(data + element->offset, b, unit, i->form, deferred);
							}
							break;
						}
					}
				}

				if (children)
					children->Append(p);

				if (a->children) {
					if (stack.size() > 1024) {
						ISO_TRACEF("DIE overflowed at ") << refs[x0]->ID() << '\n';
						break;
					}
					stack.push_back(children);
					children	= (anything*)(data + element->offset);
					always_read	= true;
				}
				if (b.peekc() == 0) {
					b.getc();
					if (stack.empty())
						break;
					children = stack.pop_back_value();
				}
			} else {
				break;
			}
		} else {
			if (children)
				children->Append(p);
		}
	}

	return refs[x0];
}

ISO_ptr<void> Reader::ReadInfo(tag id, bool deferred) {
	if (!info.p)
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	for (byte_reader b(info.p); b.p < info.end(); ) {
		ref_ptr<info_unit>	unit = new info_unit(this, b, bigendian, abbrev);

		recursion_depth	= recursion_max = 0;
		p->Append(ReadDIE(b, unit, true, deferred));

		if (files) {
			delete[] files;
			files		= 0;
			num_files	= 0;
		}

		b.p = unit->end;

		if (refs.size() > 0x10000)
			break;
	}
	return p;
}

ISO_ptr<void> Reader::ReadPubNames(tag id) {
	if (!pubnames.p)
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	names_unit	unit;
	for (byte_reader b(pubnames.p); b.p < pubnames.end(); b.p = unit.end) {
		unit.read(b, bigendian);
		ISO_ptr<anything>	p2(0);
		p->Append(p2);

		while (uint64 offset = unit.read_offset(b, bigendian)) {
			tag	id = b.get<string>();
			if (unit.large)
				p2->Append(ISO_ptr<xint64>(id, offset));
			else
				p2->Append(ISO_ptr<xint32>(id, uint32(offset)));
		}
	}
	return p;
}

ISO_ptr<void> Reader::ReadAddressRanges(tag id) {
	if (!aranges.p)
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	arange_unit	unit;
	for (byte_reader b(aranges.p); b.p < aranges.end(); b.p = unit.end) {
		unit.read(b, bigendian);
		b.p	= aranges + align(b.p - aranges, unit.entry_size());

		ISO_ptr<ISO_openarray<Dwarf_AddressRange> >	p2(0);
		p->Append(p2);

		for (;;) {
			uint64	seg	= get_val(b, unit.seg_size);
			uint64	off	= get_val(b, unit.addr_size);
			uint64	len	= get_val(b, unit.addr_size);
			if (seg == 0 && off == 0 && len == 0)
				break;
			Dwarf_AddressRange	&ar = p2->Append();
			ar.segment	= seg;
			ar.address	= off;
			ar.length	= len;
		}
	}
	return p;
}

ISO_ptr<void> ReadFrameInfo(tag id, uint8 *start, uint8 *end, bool bigendian, bool eh) {
	ISO_ptr<ISO_openarray<Dwarf_AddressRange> >	p(id);

	endian_reader	r(bigendian);
	frame_unit		unit;

	for (byte_reader b(start); b.p < end; b.p = unit.end) {
		unit.read(b, bigendian);

		if (eh) {
			if (unit.is_cie_eh())
				continue;
			byte_reader	b2(unit.start + 4 - unit.cie_offset);
			frame_unit_cie_eh	cie;
			cie.read(b2, bigendian);

			uint64	off		= cie.read_encoded(b, cie.fde_enc, bigendian);
			uint64	len		= cie.read_encoded(b, cie.fde_enc, bigendian);
//			uint32	aug		= cie.aug ? b.get<leb128<uint32> >() : 0;
//			uint64	lsda	= cie.read_encoded(b, cie.lsda_enc, bigendian);

			Dwarf_AddressRange	&ar = p->Append();
			ar.segment	= 0;
			ar.address	= off;
			ar.length	= len;
		} else {
			if (unit.is_cie())
				continue;

			frame_unit_cie	cie;
			
			byte_reader	b2(start + unit.cie_offset);
			cie.read(b2, bigendian);

			uint64	seg = 0, off, len;
			if (cie.version < 3) {
				off	= r.get_val(b, 4);
				len	= r.get_val(b, 4);
			} else {
				if (cie.seg_size <= 8)
					seg	= r.get_val(b, cie.seg_size);
				else
					b.skip(cie.seg_size);
				off	= r.get_val(b, cie.addr_size);
				len	= r.get_val(b, cie.addr_size);
			}

			Dwarf_AddressRange	&ar = p->Append();
			ar.segment	= seg;
			ar.address	= off;
			ar.length	= len;
		}
	}
	return p;
}

}//namespace dwarf

ISO_DEFVIRT(dwarf::DWARF_die);

//-----------------------------------------------------------------------------
//	ELF
//-----------------------------------------------------------------------------

template<bool be, int bits> class ELF {
	typedef Elf_types<iso_bigendian, bits>	types;

	typedef	Elf_Ehdr	<be,bits>	Ehdr;
	typedef	Elf_Shdr	<be,bits>	Shdr;
	typedef	Elf_Phdr	<be,bits>	Phdr;
	typedef	Elf_Sym		<be,bits>	Sym;
	typedef	Elf_Rel		<be,bits>	Rel;
	typedef	Elf_Rela	<be,bits>	Rela;
	typedef unsigned_t<Elf_Addr>	Addr;

	static	Sym		*PutSymbol(Sym *p, STRINGTABLE &st, const char *name,
		uint32			shndx,
		intptr_t		value,
		intptr_t		size,
		ST_BINDING		b,
		ST_TYPE			t,
		ST_VISIBILITY	v = STV_DEFAULT
	) {
		p->st_name	= st.add(name);
		p->st_shndx	= shndx;
		p->st_value	= value;
		p->st_size	= size;
		p->set(b, t, v);
		return p + 1;
	}
	static	Shdr	*PutSection(Shdr *p, STRINGTABLE &st, const char *name,
		typename types::Word	type,
		typename types::Xword	flags,
		intptr_t		addr,
		intptr_t		offset,
		intptr_t		size,
		intptr_t		addralign,
		typename types::Word	link	= 0,
		typename types::Word	info	= 0,
		typename types::Off		entsize	= 0
	) {
		p->sh_name		= st.add(name);
		p->sh_type		= type;
		p->sh_flags		= flags;
		p->sh_addr		= addr;
		p->sh_offset	= offset;
		p->sh_size		= size;
		p->sh_link		= link;
		p->sh_info		= info;
		p->sh_addralign = addralign;
		p->sh_entsize	= entsize;
		return p + 1;
	}
public:
	ISO_ptr<void>  Read(tag id, istream_ref file);
	bool	Write(ISO::Browser b, ostream_ref file, FileHandler *fh, const char *fn, int machine, int alignment = 4);
//	bool	Write(ISO::Browser b, ostream_ref file, FileHandler *fh);
//	void	DumpType(const ISO::TypeUser *type);
};

struct ISO_ELF : anything {};
ISO_DEFUSERX(ISO_ELF, anything, "ELF");
//-----------------------------------------------------------------------------
//	Read
//-----------------------------------------------------------------------------

template<bool be> void Relocate(int machine, int type, void *p, uint32 addr, uint32 val) {
	typedef typename endian_types<be>::uint32	uint32_e;
	typedef typename endian_types<be>::uint16	uint16_e;

	switch (machine) {
		case EM_PPC:
			switch (type) {
				case R_PPC_ADDR16_LO:	*(uint16_e*)p	= val;			break;
				case R_PPC_ADDR16_HA:	*(uint16_e*)p	= val >> 16;	break;
				case R_PPC_REL24:		*(uint32_e*)p	= (*(uint32_e*)p & 0xfc000003) | ((val - addr) & 0x03fffffc); break;
			}
			break;
	}
}

template<bool be> void Relocate(int machine, int type, void *p, uint64 addr, uint64 val) {
	typedef typename endian_types<be>::uint64	uint64_e;
	typedef typename endian_types<be>::uint32	uint32_e;
	typedef typename endian_types<be>::uint16	uint16_e;

	switch (machine) {
		case EM_AMD64: switch (type) {
			case R_AMD64_64:			*(uint64_e*)p	= val;		break;
			case R_AMD64_PC32:
			case R_AMD64_PLT32:
			case R_AMD64_GOTPCREL:		*(uint32_e*)p	+= val;		break;
			case R_AMD64_GOT32:			*(uint32_e*)p	= uint32(val);	break;
//			case R_AMD64_COPY:
//			case R_AMD64_GLOB_DAT:
//			case R_AMD64_JUMP_SLOT:
//			case R_AMD64_RELATIVE:
			case R_AMD64_32:			*(uint32_e*)p	= uint32(val);	break;
			case R_AMD64_32S:			*(uint32_e*)p	= uint32(val);	break;
			case R_AMD64_16:			*(uint16_e*)p	= val;		break;
			case R_AMD64_PC16:			*(uint16_e*)p	+= val;		break;
			case R_AMD64_8:				*(uint8*)p		= val;		break;
			case R_AMD64_PC8:			*(uint8*)p		+= val;		break;
			default:
				ISO_TRACE("unsupported reloc\n");
				break;
		}	break;

		case EM_PPC64: switch (type) {
			case R_PPC64_REL32:
			case R_PPC64_PLTREL32:
				val	-= addr;
			case R_PPC64_ADDR32:
			case R_PPC64_UADDR32:
			case R_PPC64_PLT32:
				*(uint32_e*)p	= uint32(val);
				break;
			case R_PPC64_REL24:
				val	-= addr;
			case R_PPC64_ADDR24:
				*(uint32_e*)p	= masked_write<uint32>(*(uint32_e*)p, uint32(val), 0x03fffffc);
				break;
			case R_PPC64_ADDR16:
			case R_PPC64_UADDR16:
			case R_PPC64_GOT16:
				*(uint16_e*)p	= val;
				break;
			case R_PPC64_ADDR16_LO:
			case R_PPC64_GOT16_LO:
			case R_PPC64_PLT16_LO:
				*(uint16_e*)p	= val;
				break;
			case R_PPC64_ADDR16_HIGHESTA:
				val >>= 16;
			case R_PPC64_ADDR16_HIGHERA:
				val >>= 16;
			case R_PPC64_ADDR16_HA:
			case R_PPC64_PLT16_HA:
			case R_PPC64_GOT16_HA:
				val += 0x8000;
			case R_PPC64_ADDR16_HI:
			case R_PPC64_PLT16_HI:
			case R_PPC64_GOT16_HI:
				*(uint16_e*)p	= val >> 16;
				break;
			case R_PPC64_REL14:
				val	-= addr;
			case R_PPC64_ADDR14:
				*(uint32_e*)p	= masked_write<uint32>(*(uint32_e*)p, uint32(val), 0xfffc);
				break;
			case R_PPC64_REL14_BRTAKEN:
				val	-= addr;
			case R_PPC64_ADDR14_BRTAKEN:
				*(uint32_e*)p	= masked_write<uint32>(*(uint32_e*)p |  (1<<21), uint32(val), 0xfffc);
				break;
			case R_PPC64_REL14_BRNTAKEN:
				val	-= addr;
			case R_PPC64_ADDR14_BRNTAKEN:
				*(uint32_e*)p	= masked_write<uint32>(*(uint32_e*)p & ~(1<<21), uint32(val), 0xfffc);
				break;
//			case R_PPC64_COPY:
//			case R_PPC64_GLOB_DAT:
//			case R_PPC64_JMP_SLOT:
//			case R_PPC64_RELATIVE:
//			case R_PPC64_SECTOFF:
//			case R_PPC64_SECTOFF_LO:
//			case R_PPC64_SECTOFF_HI:
//			case R_PPC64_SECTOFF_HA:
			case R_PPC64_REL30:
				*(uint32_e*)p	= masked_write<uint32>(*(uint32_e*)p, uint32(val), 0xfffffffc);
				break;
			case R_PPC64_REL64:
			case R_PPC64_PLTREL64:
				val	-= addr;
			case R_PPC64_ADDR64:
			case R_PPC64_UADDR64:
			case R_PPC64_PLT64:
				*(uint64_e*)p	= val;
				break;
			case R_PPC64_ADDR16_HIGHER:
				*(uint16_e*)p	= val >> 32;
				break;
			case R_PPC64_ADDR16_HIGHEST:
				*(uint16_e*)p	= val >> 48;
				break;
//			case R_PPC64_TOC16:
//			case R_PPC64_TOC16_LO:
//			case R_PPC64_TOC16_HI:
//			case R_PPC64_TOC16_HA:
//			case R_PPC64_TOC:
//			case R_PPC64_PLTGOT16:
//			case R_PPC64_PLTGOT16_LO:
//			case R_PPC64_PLTGOT16_HI:
//			case R_PPC64_PLTGOT16_HA:
			case R_PPC64_ADDR16_DS:
			case R_PPC64_ADDR16_LO_DS:
			case R_PPC64_GOT16_DS:
			case R_PPC64_GOT16_LO_DS:
			case R_PPC64_PLT16_LO_DS:
				*(uint16_e*)p	= masked_write<uint16>(*(uint16_e*)p, uint16(val), 0xfffc);
				break;
//			case R_PPC64_SECTOFF_DS:
//			case R_PPC64_SECTOFF_LO_DS:
//			case R_PPC64_TOC16_DS:
//			case R_PPC64_TOC16_LO_DS:
//			case R_PPC64_PLTGOT16_DS:
//			case R_PPC64_PLTGOT16_LO_DS:
				break;
		}	break;
	}
}

template<bool be, int bits> ISO_ptr<void> ELF<be,bits>::Read(tag id, istream_ref file) {
	ISO_ptr<ISO_ELF>	iso_elf(id);

	Ehdr		h	= file.get();
//	int			m	= h.e_machine;
	int			np	= h.e_phnum;
	int			ns	= h.e_shnum;
	Phdr		*ph	= alloc_auto(Phdr, np);
	Shdr		*sh	= alloc_auto(Shdr, ns);

	file.seek(h.e_phoff);
	readn(file, ph, np);

	file.seek(h.e_shoff);
	readn(file, sh, ns);

	if (np) {
		static const struct {uint32 id; const char *name; } pnames[] = {
			{PT_NULL,			0								},
			{PT_LOAD,			"loadable segment"				},
			{PT_DYNAMIC,		"dynamic linking information"	},
			{PT_INTERP,			"interpreter"					},
			{PT_NOTE,			"note"							},
			{PT_SHLIB,			"unspecified"					},
			{PT_PHDR,			"program header table"			},
			{PT_TLS,			"thread-local"					},
			{PT_LOOS,			"OS-specific"					},
			{PT_UNWIND,			"stack unwind tables"			},
			{PT_EH_FRAME,		"stack unwind table"			},
			{PT_GNU_STACK,		"stack flags"					},
			{PT_GNU_RELRO,		"relocated read only"			},
			{PT_OS_SCE,			"SCE specific"					},
			{PT_LOPROC,			"Processor-specific"			},
		};

		auto	pb	= new_auto(iso_mem<bits>, np);
		for (int i = 0; i < np; i++) {
			file.seek(ph[i].p_offset);
			pb[i].a = ph[i].p_vaddr;
			file.readbuff(pb[i].b.Create(uint32(ph[i].p_filesz), false), ph[i].p_filesz);
		}
		auto	iso_segs	= ISO::MakePtr<ISO_openarray<ISO_ptr<iso_mem<bits> > > >("segments", np);
		iso_elf->Append(iso_segs);
//		ISO_openarray<ISO_ptr<iso_mem<bits> > >	iso_segs(np);
//		iso_elf->Append(ISO::MakePtr("segments", iso_segs));

		for (int i = 0; i < np; i++) {
			Phdr	&p		= ph[i];
			int		j;
			for (j = 0; j < num_elements(pnames) - 1 && p.p_type >= pnames[j + 1].id; j++);
			const char *id	= pnames[j].name;
			if (p.p_type > pnames[j].id)
				id = (buffer_accum<256>(id) << "+0x" << hex(p.p_type - pnames[j].id));
			(*iso_segs)[i]		= ISO::MakePtr(id, move(pb[i]));
		}
	}

	auto	sb	= new_auto(iso_mem<bits>, ns);

	for (int i = 1; i < ns; i++) {
		file.seek(sh[i].sh_offset);
		sb[i].a = sh[i].sh_addr;
		file.readbuff(sb[i].b.Create(uint32(sh[i].sh_size), false), sh[i].sh_size);
	}

	auto	iso_sects	= ISO::MakePtr<ISO_openarray<ISO_ptr<iso_mem<bits> > > >("sections", ns);
	iso_elf->Append(iso_sects);

	const char	*shnames	= (const char*)(xint8*)sb[h.e_shstrndx].b;
	bool		nodemangle	= !!ISO::root("variables")["nodemangle"].GetInt();
	for (int i = 1; i < ns; i++) {
		Shdr	&s		= sh[i];
		(*iso_sects)[i]	= ISO::MakePtr(shnames + int(s.sh_name), move(sb[i]));
#if 0
		switch (s.sh_type) {
			case SHT_SYMTAB:
			case SHT_DYNSYM: {
				const char	*names	= (const char*)(xint8*)sb[s.sh_link].bin;
				Sym		*sym		= (Sym*)(xint8*)sb[i].bin;
				int		n			= int(s.sh_size / sizeof(Sym));

				ISO_ptr<anything>	p(s.sh_type == SHT_DYNSYM ? "dynamic_symbols" : "symbols", n);
				for (int j = 0; j < n; j++) {
					int	si	= sym[j].st_shndx;
					if (si >= ns)
						si = 0;

					ISO_ptr<void>	section	= si > 0	? (ISO_ptr<void>)(*iso_sects)[si] : ISO_NULL;
					string			name	= names		+ int(sym[j].st_name);

					if (name && !nodemangle)
						name = demangle(name);
					//(*p)[j] = ISO::MakePtr(name, make_pair(section, hex(native_endian(sym[j].st_value))));
					//(*p)[j] = ISO::MakePtr(name, hex(native_endian(sym[j].st_value)));
					(*p)[j] = ISO::MakePtr(name, sym[j]);
				}
				iso_elf->Append(p);
				break;
			}

			case SHT_RELA:
				if (s.sh_info) {
					xint8	*dest	= sb[s.sh_info].bin;
					Sym		*sym	= (Sym*)(xint8*)sb[s.sh_link].bin;
					Rela	*rel	= (Rela*)(xint8*)sb[i].bin;
					Addr	start	= sh[s.sh_info].sh_addr;
					for (size_t	n = s.sh_size / sizeof(Rela); n--; rel++) {
						Addr	off	= rel->r_offset - start;
						Relocate<be>(m, rel->type(), dest + off, off, sym[rel->symbol()].st_value + rel->r_addend);
					}
				}
				break;

			case SHT_REL:
			#if 0
				if (s.sh_info) {
					xint8	*dest	= sb[s.sh_info].bin;
					Sym		*sym	= (Sym*)(xint8*)sb[s.sh_link].bin;
					Rela	*rel	= (Rela*)(xint8*)sb[i].bin;
					Addr	start	= sh[s.sh_info].sh_addr;
					for (size_t	n = s.sh_size / sizeof(Rela); n--; rel++) {
						Addr	off	= rel->r_offset - start;
						Relocate<be>(m, rel->type(), dest + off, off, sym[rel->symbol()].st_value);
					}
				}
			#endif
				break;

			case SHT_PS3_RELA:
				if (s.sh_info) {
					xint8	*dest	= (xint8*)sb[s.sh_info].bin;
					Rela	*rel	= (Rela*)(xint8*)sb[i].bin;
					Addr	start	= sh[s.sh_info].sh_addr;
					for (size_t	n = s.sh_size / sizeof(Rela); n--; rel++) {
						Addr	off	= rel->r_offset - start;
						Relocate<be>(m, rel->type(), dest + off, off, rel->r_addend);
					}
				}
				break;

		}
#endif
	}

#if 1
	if (true) {
		static const char *dwarf_sects[] = {
			".debug_abbrev",	".debug_aranges",	".debug_frame",		".debug_info",
			".debug_line",		".debug_loc",		".debug_macinfo",	".debug_pubnames",
			".debug_pubtypes",	".debug_ranges",	".debug_str",		".debug_types",
		};

		dwarf::Sections	sects;
		for (int i = 0; i < num_elements(dwarf_sects); i++) {
			if (auto p = GetSection<bits>(*iso_sects, dwarf_sects[i]))
				sects[i]	= p;
		}

		ref_ptr<dwarf::Reader>	dr = new dwarf::Reader(sects, be);
		if (ISO_ptr<void> d = dr->ReadInfo("DWARF", true))
			iso_elf->Append(d);

//		if (ISO_ptr<void> d = dr.ReadPubNames("pub_names"))
//			iso_elf->Append(d);
	}

//	if (auto p = GetSection<bits>(*iso_sects, ".debug_frame"))
//		iso_elf->Append(dwarf::ReadFrameInfo("frames", (uint8*)p.begin(), (uint8*)p.end(), be, false));

	if (auto p = GetSection<bits>(*iso_sects, ".eh_frame"))
		iso_elf->Append(dwarf::ReadFrameInfo("frames", (uint8*)p.begin(), (uint8*)p.end(), be, true));

#endif
	return iso_elf;
}

//-----------------------------------------------------------------------------
//	Write
//-----------------------------------------------------------------------------

#if 0
bool ELF::Write(ISO::Browser b, ostream_ref file, FileHandler *fh) {
	bool		be		= true;//p.Header()->flags & ISO::Value::ISBIGENDIAN;
	int			bits	= 64;

	return be
		? (bits == 32 ? Write<true, 32> (b, file, fh) : Write<true, 64> (b, file, fh))
		: (bits == 32 ? Write<false, 32>(b, file, fh) : Write<false, 64>(b, file, fh));
}

void ELF::DumpType(const ISO::TypeUser *type) {

	offset	= (offset + 3) & ~3;
	streamptr	here	= file.tell();
	file.seek(offset);

	switch (type->GetType()) {
		default:
			file.write(type2);
			offset = uint32(file.tell());
			break;

		case ISO::COMPOSITE: {
			ISO::TypeComposite	&comp	= *(ISO::TypeComposite*)type;
			bool				crc_ids	= !!(type->flags & ISO::TypeComposite::CRCIDS);
			offset += sizeof(_ISO_type_composite) + sizeof(ISO::Element) * comp.Count();
			if (!string_ids)
				type2.flags |= ISO::TypeComposite::CRCIDS;
			file.write(type2);
			file.write(comp.Count());
			for (auto e : comp) {
				if (e.id && !crc_ids && string_ids) {
					streamptr	here	= file.tell();
					uint32		len		= strlen(e.id) + 1;
					file.seek(offset);
					file.writebuff(e.id, len);
					file.seek(here);
					e.id		= (tag&)offset;
					offset		+= len;
				} else {
					e.id		= GetCRCID(e.id, crc_ids);//(tag&)crc32(e.id);
				}
				e.type			= DumpType(e.type);
				file.write(e);
			}
			break;
		}
		case ISO::ARRAY: {
			offset += sizeof(ISO::TypeArray);
			ISO::TypeArray	*a = (ISO::TypeArray*)type;
			file.write(type2);
			file.write(a->count);
			file.write(DumpType(a->subtype));
			file.write(a->subsize);
			break;
		}
		case ISO::OPENARRAY: {
			offset += sizeof(ISO::TypeOpenArray);
			ISO::TypeOpenArray	*a = (ISO::TypeOpenArray*)type;
			file.write(type2);
			file.write(DumpType(a->subtype));
			file.write(a->subsize);
			break;
		}
		case ISO::REFERENCE: {
			offset += sizeof(ISO::TypeReference);
			ISO::TypeReference	*a = (ISO::TypeReference*)type;
			file.write(type2);
			file.write(DumpType(a->subtype));
			break;
		}
		case ISO::USER: {
			offset += sizeof(ISO::TypeUser);
			ISO::TypeUser	*a = (ISO::TypeUser*)type;
			file.write(type2);
			file.write(crc32(a->id));
			file.write(0);
			break;
		}
	}

	file.seek(here);
}
#endif

template<bool be, int bits> bool ELF<be,bits>::Write(ISO::Browser b, ostream_ref file, FileHandler *fh, const char *fn, int machine, int alignment) {
	//-------------------------------- header

	const char	*section	= ISO::root("variables")["section"].GetString(".data");
	bool		putsize		= !!ISO::root("variables")["data_size"].GetInt(1);
	bool		putend		= !!ISO::root("variables")["data_end"].GetInt(0);
	bool		has_bin		= fn && ISO::binary_data.Size() != 0 && str(fh->GetExt()) != "ib" && str(fh->GetExt()) != "ibz";

	Ehdr	h;
	clear(h);

	h.e_ident[EI_MAG0]	= 0x7f;
	h.e_ident[EI_MAG1]	= 'E';
	h.e_ident[EI_MAG2]	= 'L';
	h.e_ident[EI_MAG3]	= 'F';
	h.e_ident[EI_CLASS]	= bits == 32 ? ELFCLASS32 : ELFCLASS64;
	h.e_ident[EI_DATA]	= be ? ELFDATA2MSB : ELFDATA2LSB;
	h.e_ident[EI_VERSION]= EV_CURRENT;

	if (bits == 64) {
		h.e_ident[EI64_OSABI] = 0x66;
		h.e_ident[EI64_ABIVERSION] = 0;
	}

	h.e_type		= ET_REL;
	h.e_machine		= machine;
	h.e_version		= EV_CURRENT;
	h.e_shoff		= (typename Elf_types<iso_bigendian,bits>::Off)sizeof(h);
	h.e_flags		= 0;//nintendo ? 0 : 0x20924001;
	h.e_ehsize		= (uint16)sizeof(h);
	h.e_shentsize	= (uint16)sizeof(Elf_Shdr<be,bits>);
	h.e_shstrndx	= 4 + int(has_bin);
	h.e_shnum		= h.e_shstrndx + 1;

	size_t	offset	= sizeof(Ehdr) + sizeof(Shdr) * h.e_shnum;
	file.seek(offset);

	//-------------------------------- data + build symbols

	STRINGTABLE	symbol_st;
	Sym			sym[256], *psym = sym;
	size_t		total	= 0;

	symbol_st.add("");
	clear(*psym++);

	if (fn)
		psym = PutSymbol(psym, symbol_st, fn, -15, 0, 0, STB_LOCAL, STT_FILE);

	if (ISO::root("variables")["isodefs"].GetInt()) {
		for (auto &i : ISO::user_types) {
			if ((i->flags & ISO::TypeUser::FROMFILE) && i->subtype) {
				while (total % alignment) {
					file.putc(0);
					total++;
				}
				psym = PutSymbol(psym, symbol_st, i->ID().get_tag(), 1, total, alignment, STB_GLOBAL, STT_OBJECT);
//				DumpType(i);
				total			= file.tell() - offset;
			}
		}
	} else {
		for (int i = 0, count = b.Count(); i < count; i++) {
			ISO_ptr<void>	p		= *b[i];
			const char		*label	= p.ID().get_tag();
			if (!label)
				label = "";

			while (total % alignment) {
				file.putc(0);
				total++;
			}

			size_t	start	= total;
			psym		= PutSymbol(psym, symbol_st, format_string("%s_bin", label), 1, start, 0, STB_GLOBAL, STT_OBJECT);

			fh->Write(p, ostream_offset(file).me());
			file.seek_end(0);
			total		= file.tell() - offset;

			if (putend)
				psym = PutSymbol(psym, symbol_st, format_string("%s_end", label), 1, total, alignment, STB_GLOBAL, STT_OBJECT);
			if (putsize)
				psym = PutSymbol(psym, symbol_st, format_string("%s_size", label), SHN_ABS, total - start, 0, STB_GLOBAL, STT_NOTYPE);
		}
		if (has_bin) {
			size_t		size = ISO::binary_data.Size();
			filename	name = filename(fn).name();

			psym = PutSymbol(psym, symbol_st, format_string("%s_binary_bin", (const char*)name), 2, 0, alignment, STB_GLOBAL, STT_OBJECT);
			ISO::binary_data.Write(file);

			if (putend)
				psym = PutSymbol(psym, symbol_st, format_string("%s_binary_end", (const char*)name), 2, size, 256, STB_GLOBAL, STT_OBJECT);
			if (putsize)
				psym = PutSymbol(psym, symbol_st, format_string("%s_binary_size", (const char*)name), SHN_ABS, size, 0, STB_GLOBAL, STT_NOTYPE);
		}
	}

	int	nsyms	= int(psym - sym);

//-------------------------------- sections

	STRINGTABLE	section_st;
	Shdr		sh[16], *psh = sh;

	// null section
	section_st.add("");
	clear(*psh++);

	// .data section
	psh		= PutSection(psh, section_st, section, SHT_PROGBITS, SHF_WRITE | SHF_ALLOC, 0, offset, total, alignment);
	offset	+= total;

	// .bin section
	if (has_bin) {
		psh		= PutSection(psh, section_st, ".bin", SHT_PROGBITS, SHF_WRITE | SHF_ALLOC, 0, offset, ISO::binary_data.Size(), 256);
		offset	+= ISO::binary_data.Size();
	}

	// .symtab section
	psh		= PutSection(psh, section_st, ".symtab", SHT_SYMTAB, 0, 0, offset, sizeof(Sym) * nsyms, 4, psh - sh + 1, 1, uint32(sizeof(Sym)));
	offset	+= sizeof(Sym) * nsyms;

	// .strtab section
	psh		= PutSection(psh, section_st, ".strtab", SHT_STRTAB, 0, 0, offset, symbol_st.size(), 1);
	offset	+= symbol_st.size();

	// .shstrtab section
	section_st.add(".shstrtab");
	psh		= PutSection(psh, section_st, ".shstrtab", SHT_STRTAB, 0, 0, offset, section_st.size(), 1);
	offset	+= section_st.size();

//-------------------------------- write

	// Headers
	file.seek(0);
	file.write(h);
	writen(file, sh, psh - sh);

	// Symbols
	file.seek_end(0);
	file.writebuff(sym, sizeof(Sym) * nsyms);

	// Symbol strtab
	file.write(symbol_st);

	// Section strtab
	file.write(section_st);

	return true;
}

//-----------------------------------------------------------------------------
//	FileHandlers
//-----------------------------------------------------------------------------

class ELFFileHandler : public FileHandler {
public:
	const char*		GetExt() override { return "elf";			}
	const char*		GetDescription() override { return "ELF file";	}

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32be>() == Elf_Ident::MAGIC ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		filename::ext_t		ext = filename(fn.name()).ext();
		FileHandler			*fh	= ext.blank() ? NULL : FileHandler::Get(ext);
		if (!fh)
			fh = FileHandler::Get("bin");
		return ELF<true,  64>().Write(GetItems(p), FileOutput(fn).me(), fh, fn, EM_PPC64);
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return ELF<true,  64>().Write(GetItems(p), file, FileHandler::Get("bin"), 0, EM_PPC64);
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
#if 1
		Elf_Ident		ident;
		file.read(ident);
		if (ident.magic == ident.MAGIC) {
			bool	be		= ident.encoding	== ELFDATA2MSB;
			int		bits	= ident.file_class	== ELFCLASS32 ? 32 : 64;
			file.seek(0);
			return be
				? (bits == 32 ? ELF<true,  32>().Read(id, file) : ELF<true,  64>().Read(id, file))
				: (bits == 32 ? ELF<false, 32>().Read(id, file) : ELF<false, 64>().Read(id, file));
		}
		return ISO_NULL;
#else
		unsigned char	e_ident[EI_NIDENT];
		file.readbuff(e_ident, sizeof(e_ident));

		if (e_ident[EI_MAG0]== 0x7f
		&& e_ident[EI_MAG1]	== 'E'
		&& e_ident[EI_MAG2]	== 'L'
		&& e_ident[EI_MAG3]	== 'F') {
			bool	be		= e_ident[EI_DATA]	== ELFDATA2MSB;
			int		bits	= e_ident[EI_CLASS] == ELFCLASS32 ? 32 : 64;
			file.seek(0);
			return be
				? (bits == 32 ? ELF<true,  32>().Read(id, file) : ELF<true,  64>().Read(id, file))
				: (bits == 32 ? ELF<false, 32>().Read(id, file) : ELF<false, 64>().Read(id, file));
		}
		return ISO_NULL;
#endif
	}
} elf;

class SELFFileHandler : public FileHandler {
	struct SCE_header {
		enum {MAGIC = 'SCE\0'};
		uint32be	magic;
		uint32be	u04;	//00000002
		uint32be	u08;	//80000001
		uint32be	u0c;	//00000410

		uint64be	elf_offset;

//		uint64be	unknown_offset;
//		uint64be	unknown_count;
//		uint64be	unknown_offsets[];
	};
	const char*		GetExt() override { return "self";			}
	const char*		GetDescription() override { return "Signed ELF file";	}

	int				Check(istream_ref file) override {
		file.seek(0);
		SCE_header	h;
		if (file.read(h) && h.magic == h.MAGIC) {
			file.seek(h.elf_offset);
			if (file.get<uint32be>() == Elf_Ident::MAGIC)
				return CHECK_PROBABLE;
		}
		return CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		SCE_header		h;
		if (file.read(h) && h.magic == h.MAGIC) {
			file.seek(h.elf_offset);
			return elf.Read(id, istream_offset(file));
		}
		return ISO_NULL;
	}
} self;

class ELFOFileHandler : ELFFileHandler {
	const char*		GetExt() override { return "o"; }
	int				Check(istream_ref file) override { return CHECK_DEFINITE_NO; }

	static int RightPlatform() {
		static const char *platforms[] = {
			"ps3",
			"ps4",
			"wii",
		};
		const char *exportfor = ISO::root("variables")["exportfor"].GetString();
		for (int i = 0; i < num_elements(platforms); i++) {
			if (str(exportfor) == platforms[i])
				return i + 1;
		}
		return 0;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file, const char *fn, FileHandler *fh, int plat) {
		p	= ISO_conversion::convert(p, 0, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);
		ISO::Browser2	b	= GetItems(p);
		switch (plat) {
			case 1: return ELF<true,  64>().Write(b, file, fh, fn, EM_PPC64);
			case 2: return ELF<false, 64>().Write(b, file, fh, fn, EM_AMD64);
			case 3:	return ELF<true,  32>().Write(b, file, fh, fn, EM_PPC, 32);
			default: return false;
		}
	}
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		if (int plat = RightPlatform()) {
			filename::ext_t	ext = filename(fn.name()).ext();
			FileHandler		*fh	= ext.blank() ? NULL : FileHandler::Get(ext);
			if (!fh)
				fh = FileHandler::Get("bin");
			return Write(p, FileOutput(fn).me(), fn, fh, plat);
		}
		return false;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		if (int plat = RightPlatform())
			return Write(p, file, 0, FileHandler::Get("bin"), plat);
		return false;
	}
} elfo;

class ELFOBJFileHandler : ELFOFileHandler {
	const char*		GetExt() override { return "obj"; }
} elfobj;
