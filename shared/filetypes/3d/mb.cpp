#include "iso/iso_files.h"
#include "model_utils.h"
#include "extra/indexer.h"
#include "geometry.h"
#include "filetypes/iff.h"
#include "bitmap/bitmap.h"

using namespace iso;

template<> struct ISO_def<embedded_string> : TISO_virtual2<embedded_string> {
	static ISO_browser2	Deref(embedded_string &a)	{ return ISO_ptr<string>(0, a); }
};
template<typename T, typename P> struct ISO_def<after<T,P> > : TISO_virtual2<after<T,P> > {
	static ISO_browser2	Deref(after<T,P> &a)		{ return MakePtr(0, a.get()); }
};

template<> struct ISO_def<IFF_ID> : TISO_virtual2<IFF_ID> {
	static ISO_browser2	Deref(IFF_ID &a)	{ return ISO_ptr<string>(0, str(a.c, 4)); }
};

struct CREA {
	xint8	flags;
	string	parent;
};
ISO_DEFUSERCOMP2(CREA, flags, parent);

struct DBLE {
	xint8	flags;
	uint32	wtf;
	dynamic_array<float64> value;
};
ISO_DEFUSERCOMP3(DBLE, flags, wtf, value);

template<typename T> struct MAYA_VAL {
	xint8	flags;
	T		value;
};

template<typename T> struct MAYA_VALS : MAYA_VAL<dynamic_array<T> > {};

template<typename T> void ReadMaya(IFF_chunk &chunk, T &t) {
	read(chunk, t);
}
void ReadMaya(IFF_chunk &chunk, dynamic_array<string> &a) {
	while (chunk.remaining())
		read(chunk, a.push_back());
}
template<typename T> void ReadMaya(IFF_chunk &chunk, dynamic_array<T> &a) {
	uint32	rem	= chunk.remaining();
	uint32	n	= rem / sizeof(T);
	a.reserve(n);
	for (int i = 0; i < n; i++)
		a.push_back(chunk.get<T_swap_endian_type<T>::type>());
}

template<typename T> ISO_ptr<MAYA_VAL<T> > ReadMayaValue(IFF_chunk &chunk) {
	ISO_ptr<MAYA_VAL<T> >	r(chunk.get<string>());
	chunk.read(r->flags);
	ReadMaya(chunk, r->value);
	return r;
}

template<typename T> ISO_DEFCOMPT(MAYA_VAL, T, 2) {
	ISO_SETFIELDS2(0, flags, value);
}};

#if 0
struct ATTR {
	xint16	flags;
	uint32	value_count;
	dynamic_array<string> strings;
};
ISO_DEFUSERCOMP3(ATTR, flags, values, strings);

#else
struct ATTR {
	xint16	flags;
	uint32	value_count;
	dynamic_array<IFF_ID> type_ids; // some kind of supported ids for compound types ?
	string	name_a;
	string	name_b;
	string	parent;
	string	enum_name;
	IFF_ID	id_after_names;
	float64	min;
	float64	max;
	float64	unk1;
	float64	unk2;
	float64	def;

	enum flags {
		f_min			= 0x0002,
		f_max			= 0x0004,
		f_default		= 0x0008,
		f_parent		= 0x0010,
		f_parented		= 0x0020,
		f_cached		= 0x0040,
		f_internal_set	= 0x0080,
		f_enum			= 0x0200,
		f_colour		= 0x0400,
		f_unknown1		= 0x0800,
		f_unknown2		= 0x1000,
		f_keyable		= 0x4000
	};
};
ISO_DEFUSERCOMP(ATTR, 6) {
	ISO_SETFIELDS7(0, flags, value_count, type_ids, name_a, name_b, parent, enum_name);
	ISO_SETFIELDS6(7, id_after_names, min, max, unk1, unk2, def);
}};

#endif

struct CMPD {
	xint8	flags;
	dynamic_array<xint8> data;
};
ISO_DEFUSERCOMP2(CMPD, flags, data);

struct CMP_list {
	xint8	flags;
	dynamic_array<uint32>	types;
	dynamic_array<xint8>	data;
};
ISO_DEFUSERCOMP3(CMP_list, flags, types, data);

struct CWFL {
	enum Flags 	{
		f_next_available = 0x01,
		f_lock = 0x02 //seen from _LightRadius -> sx (and sy, sz)
		//unseen options that may need a flag
		//force(f)
		//referenceDest(rd)
	};
	xint8	flags;
	string	from, to;
};
ISO_DEFUSERCOMP3(CWFL, flags, from, to);

struct PLUG {
	string description;
};
ISO_DEFUSERCOMP1(PLUG, description);

class MBFileHandler : public FileHandler {
	static ISO_ptr<void>	ReadForm(IFF_chunk &iff, int alignment = 2);
	const char*		GetExt() override { return "mb";		}
	const char*		GetDescription() override { return "Maya binary (mb) container"; }

	int	Check(iso::istream &file) {
		if (file.length() < 16)
			return CHECK_DEFINITE_NO;

		file.seek(0);
		IFF_chunk	chunk(file);
		return chunk.is_ext('FORM') && chunk.remaining() == file.length() - 8 && file.get<uint32be>() == 'MAYA' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, iso::istream &file) override {
		IFF_chunk	iff(file);
		int			n = iff.is_ext('FORM');
		if (!n)
			return ISO_NULL;
		ISO_ptr<void>	p = ReadForm(iff, n);
		return p;
	}

} maya_mb;

ISO_ptr<void> MBFileHandler::ReadForm(IFF_chunk &iff, int alignment) {
	ISO_ptr<anything>	p(iff.get<IFF_ID>());

	for (;iff.remaining(); iff.istream().align(alignment)) {
		IFF_chunk	chunk(iff.istream());
		int			n;
		if ((n = chunk.is_ext('FORM')) || (n = chunk.is_ext('LIST')) || (n = chunk.is_ext('PROP'))) {
			p->Append(ReadForm(chunk, n));

		} else switch (chunk.id) {
			case 'VERS':
			case 'UVER':
			case 'MADE':
			case 'CHNG':
			case 'ICON':
			case 'INFO':
			case 'OBJN':
			case 'INCL':
			case 'LUNI':
			case 'TUNI':
			case 'AUNI':
			case 'SLCT':
			{
				ISO_ptr<string>	r(chunk.id);
				uint32	rem = chunk.remaining();
				chunk.readbuff(r->alloc(rem), rem);
				p->Append(r);
				break;
			}
			case 'CREA': {
				uint8	flags	= chunk.get();
				ISO_ptr<CREA>	r(chunk.get<string>());
				r->flags	= flags;
				chunk.read(r->parent);
				p->Append(r);
				break;
			}
			case 'FINF':
				p->Append(ISO_ptr<string>(chunk.get<string>(), chunk.get()));
				break;
			case 'STR ':
				p->Append(ISO_ptr<string>(chunk.get<string>(), chunk.get()));
				break;

			case 'STR#':	p->Append(ReadMayaValue<dynamic_array<string> >(chunk)); break;
			case 'DBLE': {
				ISO_ptr<DBLE>	r(chunk.get<string>());
				chunk.read(r->flags);
				if (chunk.remaining() & 4)
					chunk.read(r->wtf);
				else
					r->wtf = 0;
				ReadMaya(chunk, r->value);
				p->Append(r);
				break;
			}
			case 'DBL2':	p->Append(ReadMayaValue<dynamic_array<fixed_array<float64,2> > >(chunk)); break;
			case 'DBL3':	p->Append(ReadMayaValue<dynamic_array<fixed_array<float64,3> > >(chunk)); break;
			case 'FLT2':	p->Append(ReadMayaValue<dynamic_array<fixed_array<float32,2> > >(chunk)); break;
			case 'FLT3':	p->Append(ReadMayaValue<dynamic_array<fixed_array<float32,3> > >(chunk)); break;
			case 'LNG2':	p->Append(ReadMayaValue<dynamic_array<fixed_array<uint32,2>  > >(chunk)); break;
			case 'FLGS':	p->Append(ReadMayaValue<dynamic_array<xint8> >(chunk)); break;

			case 'MATR':	p->Append(ReadMayaValue<fixed_array<fixed_array<float64,4>,4> >(chunk)); break;

			case 'ATTR': {
				IFF_ID			id = chunk.get();
				ISO_ptr<ATTR>	r(id);

				r->flags		= chunk.get<uint16be>();
				r->value_count	= chunk.get<uint32be>();
#if 0
				iso::read(chunk, r->values.begin(), count);
				ReadMaya(chunk, r->strings);
#else
				if (!(r->flags & (ATTR::f_min|ATTR::f_max|ATTR::f_default)) && id != 'aFL3') {
					//1 type?
					chunk.read(r->type_ids.push_back());
				} else {
					size_t count = chunk.get<uint32be>();
					for(size_t i = 0 ; i < count; ++i)
						chunk.read(r->type_ids.push_back());
				}

				chunk.read(r->name_a);
				chunk.read(r->name_b);

				if (r->flags & ATTR::f_parented)
					chunk.read(r->parent);

				if (r->flags & ATTR::f_enum)
					chunk.read(r->enum_name);
				else if (id == 'aENM')
					ISO_TRACEF("Unexpected enum on attribute ") << r->name_a << "\n";

				//0, UNKN, lock, ATOL, ANIM and Wnnn have been seen
				chunk.read(r->id_after_names);

				if (r->flags & ATTR::f_min)
					r->min = chunk.get<float64be>();

				if (r->flags & ATTR::f_max)
					r->max = chunk.get<float64be>();

				if (r->flags & ATTR::f_unknown1)
					r->unk1 = chunk.get<float64be>();
				
				if (r->flags & ATTR::f_unknown2)
					r->unk2 = chunk.get<float64be>();

				if (r->flags & ATTR::f_default) {
					IFF_ID	defid = chunk.get();
					if (defid != 'DBLE')
						ISO_OUTPUTF("Unexpected default value on attribute ") << r->name_a << "\n";
					else 
						r->def = chunk.get<float64be>();
				}
#endif
				p->Append(r);
				break;
			}
			{
				string	name = chunk.get();
				ISO_TRACEF("MATR(")<<name<< "=" << chunk.remaining() << '\n';
			}

			case 'CMPD': {//compound object
				ISO_ptr<CMPD>	r(chunk.get<string>());
				chunk.read(r->flags);
				ReadMaya(chunk, r->data);
				p->Append(r);
				break;
			}
			case 'CMP#': { //compound object
				ISO_ptr<CMP_list>	r(chunk.get<string>());
				chunk.read(r->flags);
				uint32	count	= chunk.get<uint32be>();
				uint32	type	= chunk.get<uint32be>();
//				if (count > 1 || (type != 'CMDF' && type != 'CMDV' && type != 'CMDE'))
//					printf("oops\n");

				r->types.push_back(type);
				ReadMaya(chunk, r->data);
				p->Append(r);
				break;
			}

			case 'CWFL': { // maya node link (to find what shader is hooked to what material because the wads have some info about material name)
				ISO_ptr<CWFL>	r(0);
				chunk.read(r->flags);
				chunk.read(r->from);
				chunk.read(r->to);
				p->Append(r);
				break;
			}

			case 'PLUG': {//plugin data
				ISO_ptr<PLUG>	r(chunk.get<string>());
				chunk.read(r->description);
				p->Append(r);
				break;
			}

			default: {
				int32 remain =  chunk.remaining();
				ISO_ptr<ISO_openarray<uint8> >	r(chunk.id, remain);
				iff.readbuff(*r, chunk.remaining());
				p->Append(r);
				break;
			}
		}
	}
	return p;
}
