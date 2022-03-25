#include "flash.h"
#include "stream.h"

#undef exception_info

namespace iso { namespace flash {

template<bool SIGN> struct fixsign	{ template<typename T> static signed_t<T> f(T t, int n) { return sign_extend(t, n); } };
template<> struct fixsign<false>	{ template<typename T> static T			f(T t, int n) { return t; } };

template<int BITS, bool SIGN> struct varenc {
	typedef	int_t<(BITS + 7) / 8, SIGN>	T;
	T	t;

	template<typename R> static T _read(R &r) {
		uint_bits_t<(BITS + 7) / 8>	v = 0;
		for (int i = 0; i < BITS / 7; i++) {
			uint8	c = r.getc();
			v |= (c & 0x7f) << i * 7;
			if (!(c & 0x80))
				return fixsign<SIGN>::f(v, i * 7);
		}
		return T(v | (r.getc() << (BITS / 7 * 7)));
	}

	template<typename R> bool read(R &r) { t = _read(r); return true; }
	operator T()	const		{ return t; }
	void			clear()		{ t = 0;	}
};

typedef uint8				u8;
typedef uint16le			u16;
typedef uintn<3>			s24;
typedef varenc<30, false>	u30;
typedef varenc<32, false>	u32;
typedef varenc<32, true>	s32;
typedef float64le			d64;

template<typename T> bool readn2(byte_reader &r, T *&values, int n) {
	return n == 0 || readn(r, values = new T[n], n);
}

template<typename T> struct abc_array {
	u30	count;
	T	*values;

	abc_array() : values(0)				{ count.clear();	}
	~abc_array()						{ delete[] values;	}
	operator T*()						{ return values;	}
	operator const T*() const			{ return values;	}
//	T		&operator[](int i)			{ return values[i]; }
//	const T	&operator[](int i) const	{ return values[i]; }
	bool read(byte_reader &r)			{ return readn2(r, values, count = r.get()); }
};

template<typename T> struct abc_array0 : abc_array<T> {
#if 0
	bool read(byte_reader &r)	{ return readn2(r, values, int(count = r.get()) - 1); }
#endif
};

struct string_info {
	u30			size;
	u8			*utf8;
	string_info()	: utf8(0)			{ size.clear(); }
	~string_info()						{ free(utf8); }
	bool		read(byte_reader &r)	{
		return r.read(size) && check_readbuff(r, utf8 = (u8*)malloc(size), size);
	}
	operator	string() const			{ string s(size); if (size) { memcpy(s.begin(), utf8, size); s.begin()[size] = 0; } return s; }
};

struct cpool_info {
	struct namespace_info {
		enum {
			Namespace			= 0x08,
			PackageNamespace	= 0x16,
			PackageInternalNs	= 0x17,
			ProtectedNamespace	= 0x18,
			ExplicitNamespace	= 0x19,
			StaticProtectedNs	= 0x1A,
			PrivateNs			= 0x05,
		};
		u8	kind;
		u30	name;
		bool read(byte_reader &r)		{
			if (r.read(kind)) switch (kind) {
				case Namespace:
				case PackageNamespace:
				case PackageInternalNs:
				case ProtectedNamespace:
				case ExplicitNamespace:
				case StaticProtectedNs:
				case PrivateNs:
					return r.read(name);
			}
			return false;
		}
	};
	struct ns_set_info : abc_array<u30> {
		bool read(byte_reader &r) {
			if (r.read<abc_array<u30> >(*this)) {
				for (int i = 0; i < count; i++) {
					if ((*this)[i] == 0)
						return false;
				}
				return true;
			}
			return false;
		}
	};

	struct multiname_info {
		enum {
			QName		= 0x07,
			QNameA		= 0x0D,
			RTQName		= 0x0F,
			RTQNameA	= 0x10,
			RTQNameL	= 0x11,
			RTQNameLA	= 0x12,
			NameL		= 0x13,
			NameLA		= 0x14,
			Multiname	= 0x09,
			MultinameA	= 0x0E,
			MultinameL	= 0x1B,
			MultinameLA	= 0x1C,
			undocumented= 0x1D,
		};

		u8		kind;
		union {
			struct {
				u30 ns;
				u30 name;
			} qname;
			struct {
				u30 name;
			} rtqname;
			struct {
				u30 name;
				u30 ns_set;
			} multiname;
			struct {
				u30 ns_set;
			} multinamel;
			struct {
				u30 multiname;
				u30	count;
				u30	*types;
			} undoc;
		};
		bool read(byte_reader &r) {
			switch (kind = r.get()) {
				case QName: case QNameA:
					return r.read(qname.ns) && r.read(qname.name);
				case RTQName: case RTQNameA:
					return r.read(rtqname.name);
				case NameL: case NameLA:
				case RTQNameL: case RTQNameLA:
					return true;
				case Multiname: case MultinameA:
					return r.read(multiname.name) && r.read(multiname.ns_set);
				case MultinameL: case MultinameLA:
					return r.read(multinamel.ns_set);
				case undocumented:
					return r.read(undoc.multiname)
						&& r.read(undoc.count)
						&& readn2(r, undoc.types, undoc.count);
			}
			return false;
		}
	};
	abc_array0<s32>				integers;
	abc_array0<u32>				uintegers;
	abc_array0<d64>				doubles;
	abc_array0<string_info>		strings;
	abc_array0<namespace_info>	namespaces;
	abc_array0<ns_set_info>		ns_sets;
	abc_array0<multiname_info>	multinames;

	bool read(byte_reader &r) {
		return	r.read(integers)
			&&	r.read(uintegers)
			&&	r.read(doubles)
			&&	r.read(strings)
			&&	r.read(namespaces)
			&&	r.read(ns_sets)
			&&	r.read(multinames);
	}
};

struct method_info {
	enum {
		NEED_ARGUMENTS	= 0x01,	// Suggests to the run-time that an “arguments” object (as specified by the ActionScript 3.0 Language Reference) be created. Must not be used together with NEED_REST. See Chapter 3.
		NEED_ACTIVATION	= 0x02,	// Must be set if this method uses the newactivation opcode.
		NEED_REST		= 0x04,	// This flag creates an ActionScript 3.0 rest arguments array. Must not be used with NEED_ARGUMENTS. See Chapter 3.
		HAS_OPTIONAL	= 0x08,	// Must be set if this method has optional parameters and the options field is present in this method_info structure.
		SET_DXNS		= 0x40,	// Must be set if this method uses the dxns or dxnslate opcodes.
		HAS_PARAM_NAMES	= 0x80,	// Must be set when the param_names field is present in this method_info structure
	};
	struct option_detail {
		enum {
			Int					= 0x03,	//		integer
			UInt				= 0x04,	//		uinteger
			Double				= 0x06,	//		double
			Utf8				= 0x01,	//		string
			True				= 0x0B,	//		-
			False				= 0x0A,	//		-
			Null				= 0x0C,	//		-
			Undefined			= 0x00,	//		-
			Namespace			= 0x08,	//		namespace
			PackageNamespace	= 0x16,	//		namespace
			PackageInternalNs	= 0x17,	//		Namespace
			ProtectedNamespace	= 0x18,	//		Namespace
			ExplicitNamespace	= 0x19,	//		Namespace
			StaticProtectedNs	= 0x1A,	//		Namespace
			PrivateNs			= 0x05,	//		namespace
		};
		u30		val;
		u8		kind;
		bool read(byte_reader &r) { return r.read(val) && r.read(kind); }
	};

	u30							param_count;
	u30							return_type;
	u30							*param_types;//[param_count];
	u30							name;
	u8							flags;
	abc_array<option_detail>	options;
	u30							*param_names;//[param_count]

	bool read(byte_reader &r) {
		return	r.read(param_count)
		&&		r.read(return_type)
		&&		readn2(r, param_types, param_count)
		&&		r.read(name)
		&&		r.read(flags)
		&&		(!(flags & HAS_OPTIONAL)	|| r.read(options))
		&&		(!(flags & HAS_PARAM_NAMES) || readn2(r, param_names, param_count));
	}
	method_info() : param_types(0), param_names(0)	{}
	~method_info()	{
		delete[] param_types;
		delete[] param_names;
	}
};

struct metadata_info {
	struct item_info {
		u30		key;
		u30		value;
	};
	u30						name;
	abc_array<item_info>	items;
	bool read(byte_reader &r)		{ return r.read(name) && r.read(items); }
};

struct traits_info {
	enum {
		Slot		= 0,
		Method		= 1,
		Getter		= 2,
		Setter		= 3,
		Class		= 4,
		Function	= 5,
		Const		= 6,
	};
	enum {
		ATTR_Final		= 0x10,	// Is used with Trait_Method, Trait_Getter and Trait_Setter. It marks a method that cannot be overridden by a sub-class
		ATTR_Override	= 0x20,	// Is used with Trait_Method, Trait_Getter and Trait_Setter. It marks a method that has been overridden in this class
		ATTR_Metadata	= 0x40,	// Is used to signal that the fields metadata_count and metadata follow the data field in the traits_info entry
	};
	u30		name;
	u8		kind;
	union {
		struct {
			u30		slot_id;
			u30		type_name;
			u30		vindex;
			u8		vkind;
		} slot;
		struct {
			u30		slot_id;
			u30		classi;
		} clss;
		struct {
			u30		slot_id;
			u30		function;
		} function;
		struct {
			u30		disp_id;
			u30		method;
		} method;
	};

	abc_array<u30>	metadata;

	bool read(byte_reader &r) {
		bool	ret = r.read(name);
		switch ((kind = r.get()) & 0xf) {
			case Const:
			case Slot:
				ret = ret
				&&	r.read(slot.slot_id)
				&&	r.read(slot.type_name)
				&&	r.read(slot.vindex)
				&&	(!slot.vindex || r.read(slot.vkind));
				break;
			case Getter:
			case Setter:
			case Method:
				ret = ret
				&&	r.read(method.disp_id)
				&&	r.read(method.method);
				break;
			case Class:
				ret = ret
				&&	r.read(clss.slot_id)
				&&	r.read(clss.classi);
				break;
			case Function:
				ret = ret
				&&	r.read(function.slot_id)
				&&	r.read(function.function);
				break;
			default:
				return false;
		}
		return ret && (!(kind & ATTR_Metadata) || r.read(metadata));
	}
};

struct instance_info {
	enum {
		Sealed		= 0x01,	// The class is sealed: properties can not be dynamically added to instances of the class.
		Final		= 0x02,	// The class is final: it cannot be a base class for any other class.
		Interface	= 0x04,	// The class is an interface.
		ProtectedNs	= 0x08,	// The class uses its protected namespace and the protectedNs field is present in the interface_info structure.
	};
	u30						name;
	u30						super_name;
	u8						flags;
	u30						protectedNs;
	abc_array<u30>			interfaces;	//[intrf_count]
	u30						iinit;
	abc_array<traits_info>	traits;

	bool read(byte_reader &r) {
		return	r.read(name)
		&&		r.read(super_name)
		&&		r.read(flags)
		&&		(!(flags & ProtectedNs) || r.read(protectedNs))
		&&		r.read(interfaces)
		&&		r.read(iinit)
		&&		r.read(traits);
	}
};

struct class_info {
	u30						cinit;
	abc_array<traits_info>	traits;
	bool read(byte_reader &r)		{ return r.read(cinit) && r.read(traits); }
};

struct script_info {
	u30						init;
	abc_array<traits_info>	traits;
	bool read(byte_reader &r)		{ return r.read(init) && r.read(traits); }
};

struct method_body_info {
	struct exception_info {
		u30		from;
		u30		to;
		u30		target;
		u30		exc_type;
		u30		var_name;
		bool read(byte_reader &r) {
			return	r.read(from)
			&&		r.read(to)
			&&		r.read(target)
			&&		r.read(exc_type)
			&&		r.read(var_name);
		}
	};
	u30							method;
	u30							max_stack;
	u30							local_count;
	u30							init_scope_depth;
	u30							max_scope_depth;
	abc_array<u8>				code;
	abc_array<exception_info>	exceptions;
	abc_array<traits_info>		traits;

	bool read(byte_reader &r) {
		return	r.read(method)
		&&		r.read(max_stack)
		&&		r.read(local_count)
		&&		r.read(init_scope_depth)
		&&		r.read(max_scope_depth)
		&&		r.read(code)
		&&		r.read(exceptions)
		&&		r.read(traits);
	}
};
struct ABC {
	u16							minor_version;
	u16							major_version;
	cpool_info					constant_pool;
	abc_array<method_info>		methods;
	abc_array<metadata_info>	metadata;
	abc_array<instance_info>	instances;
	class_info					*classes;		//[instances.count];
	abc_array<script_info>		scripts;
	abc_array<method_body_info>	method_bodys;

	bool read(byte_reader &r) {
		return	r.read(minor_version)
		&&		r.read(major_version)
		&&		r.read(constant_pool)
		&&		r.read(methods)
		&&		r.read(metadata)
		&&		r.read(instances)
		&&		readn2(r, classes, instances.count)
		&&		r.read(scripts)
		&&		r.read(method_bodys);
	}
	ABC() : classes(0)	{}
	~ABC() { delete[] classes; }
};

ABC *DecodeABC(void *p) {
	byte_reader	r(p);

	ABC	*abc	= new ABC;
	r.read(*abc);
	return abc;
};

//-----------------------------------------------------------------------------
//	ISO
//-----------------------------------------------------------------------------

template<typename T> struct MakeISO_s {
	typedef	T	type;
	static void	f(const T &in, type &out)	{ out = in; }
};

template<typename T> void MakeISO(const T &in, typename MakeISO_s<T>::type &out) { return MakeISO_s<T>::f(in, out); }
template<typename T> ISO_ptr<void> MakeISO(tag2 id, const T &in) {
	ISO_ptr<typename MakeISO_s<T>::type>	out(id);
	MakeISO(in, *out);
	return out;
}

template<typename T> ISO_ptr<void> MakeISO(tag2 id, const T *in, int n) {
	ISO_ptr<ISO_openarray<typename MakeISO_s<T>::type > >	out(id, n);
	for (int i = 0; i < n; i++)
		MakeISO(in[i], (*out)[i]);
	return out;
}

template<> struct MakeISO_s<s24> : MakeISO_s<int> {};
template<> struct MakeISO_s<s32> : MakeISO_s<int> {};
template<> struct MakeISO_s<u30> : MakeISO_s<uint32> {};
template<> struct MakeISO_s<u32> : MakeISO_s<uint32> {};
template<> struct MakeISO_s<string_info> : MakeISO_s<string> {};

template<typename T> struct MakeISO_s<abc_array<T> > {
	typedef	ISO_openarray<typename MakeISO_s<T>::type>	type;
	static void	f(const abc_array<T> &in, type &out) {
		out.Resize(in.count);
		for (int i = 0, n = in.count; i < n; i++)
			MakeISO(in[i], out[i]);
	}
};
template<typename T> struct MakeISO_s<abc_array0<T> > : MakeISO_s<abc_array<T> > {
#if 0
	static void	f(const abc_array<T> &in, type &out) {
		if (int n = in.count) {
			out.Resize(n - 1);
			for (int i = 0; i < n - 1; i++)
				MakeISO(in[i], out[i]);
		}
	}
#endif
};
template<> struct MakeISO_s<cpool_info> {
	typedef	anything	type;
	static void	f(const cpool_info &in, type &out) {
		out.Append(MakeISO("integers",	in.integers));
		out.Append(MakeISO("uintegers",	in.uintegers));
		out.Append(MakeISO("doubles",	in.doubles));
		out.Append(MakeISO("strings",	in.strings));
	//	out.Append(MakeISO("namespaces",in.namespaces));
	//	out.Append(MakeISO("ns_sets",	in.ns_sets));
	//	out.Append(MakeISO("multinames",in.multinames));
	}
};

template<> struct MakeISO_s<metadata_info::item_info> {
	typedef	pair<uint32,uint32>	type;
	static void	f(const metadata_info::item_info &in, type &out) {
		out.a = in.key;
		out.b = in.value;
	}
};
template<> struct MakeISO_s<metadata_info> {
	typedef	pair<uint32, typename MakeISO_s<abc_array<metadata_info::item_info> >::type> type;
	static void	f(const metadata_info &in, type &out) {
		out.a = in.name;
		MakeISO(in.items, out.b);
	}
};
template<> struct MakeISO_s<method_info::option_detail> {
	typedef	pair<uint32,uint8>	type;
	static void	f(const method_info::option_detail &in, type &out) {
		out.a = in.val;
		out.b = in.kind;
	}
};
template<> struct MakeISO_s<method_info> {
	typedef	anything	type;
	static void	f(const method_info &in, type &out) {
		out.Append(MakeISO("return_type",	in.return_type));
		out.Append(MakeISO("param_types",	in.param_types, in.param_count));
		out.Append(MakeISO("name",			in.name));
		out.Append(MakeISO("flags",			in.flags));
		out.Append(MakeISO("options",		in.options));
		if (in.param_names)
			out.Append(MakeISO("param_names",	in.param_names, in.param_count));
	}
};

template<> struct MakeISO_s<instance_info> {
	typedef	anything	type;
	static void	f(const instance_info &in, type &out) {
		out.Append(MakeISO("name",			in.name));
		out.Append(MakeISO("super_name",	in.super_name));
		out.Append(MakeISO("flags",			in.flags));
		out.Append(MakeISO("protectedNs",	in.protectedNs));
		out.Append(MakeISO("interfaces",	in.interfaces));
		out.Append(MakeISO("iinit",			in.iinit));
		out.Append(MakeISO("traits",		in.traits));
	};
};
template<> struct MakeISO_s<traits_info> {
	typedef	ISO_ptr<void>	type;
	static void	f(const traits_info &in, type &out) {
	}
};
template<> struct MakeISO_s<class_info> {
	typedef	pair<uint32, typename MakeISO_s<abc_array<traits_info> >::type> type;
	static void	f(const class_info &in, type &out) {
		out.a = in.cinit;
		MakeISO(in.traits, out.b);
	}
};
template<> struct MakeISO_s<script_info> {
	typedef	pair<uint32, typename MakeISO_s<abc_array<traits_info> >::type> type;
	static void	f(const script_info &in, type &out) {
		out.a = in.init;
		MakeISO(in.traits, out.b);
	}
};
template<> struct MakeISO_s<method_body_info> {
	typedef	anything	type;
	static void	f(const method_body_info &in, type &out) {
		out.Append(MakeISO("method",			in.method));
		out.Append(MakeISO("max_stack",			in.max_stack));
		out.Append(MakeISO("local_count",		in.local_count));
		out.Append(MakeISO("init_scope_depth",	in.init_scope_depth));
		out.Append(MakeISO("max_scope_depth",	in.max_scope_depth));
		out.Append(MakeISO("code",				in.code));
//		out.Append(MakeISO("exceptions",		in.exceptions));
		out.Append(MakeISO("traits",			in.traits));
	}
};

template<> struct MakeISO_s<ABC> {
	typedef	anything	type;
	static void	f(const ABC &in, type &out) {
		out.Append(MakeISO("minor_version",	in.minor_version));
		out.Append(MakeISO("major_version",	in.major_version));
		out.Append(MakeISO("constant_pool",	in.constant_pool));
		out.Append(MakeISO("methods",		in.methods));
		out.Append(MakeISO("metadata",		in.metadata));
		out.Append(MakeISO("instances",		in.instances));
		out.Append(MakeISO("classes",		in.classes,	in.instances.count));
		out.Append(MakeISO("scripts",		in.scripts));
		out.Append(MakeISO("method_bodys",	in.method_bodys));
	}
};

ISO_ptr<void> ISO_ABC(ABC *abc) {
	return MakeISO("script", *abc);
}
} } // namespace iso::flash

#if 0
//-----------------------------------------------------------------------------
//	FileHandler
//-----------------------------------------------------------------------------
#include "iso/iso_files.h"

class ASFileHandler : public FileHandler {
	const char*		GetExt() override { return "as";		}
	const char*		GetDescription() override { return "Action Script";	}
//	int				Check(istream_ref file) override {

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		flash::ABC	*abc = flash::DecodeABC(malloc_block(file, file.length()));
		return flash::ISO_ABC(abc);
	}
} as;
#endif

//-----------------------------------------------------------------------------
//	Disassembler
//-----------------------------------------------------------------------------

#ifdef ISO_EDITOR

#include "disassembler.h"

using namespace iso;
using namespace iso::flash;

class DisassemblerAS : public Disassembler {
public:
	const char*	GetDescription() override { return "ActionScript"; }
	State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) override;
	static uint8		GetNextOp(byte_reader &r, uint32 *params);
} actionscript;

enum opcodes {
	add 			= 0xA0,
	add_i			= 0xC5,
	astype			= 0x86,
	astypelate		= 0x87,
	_bitand			= 0xA8,
	bitnot			= 0x97,
	_bitor			= 0xA9,
	bitxor			= 0xAA,
	call			= 0x41,
	callmethod		= 0x43,
	callproperty	= 0x46,
	callproplex		= 0x4C,
	callpropvoid	= 0x4f,
	callstatic		= 0x44,
	callsuper		= 0x45,
	callsupervoid	= 0x4e,
	checkfilter		= 0x78,
	coerce			= 0x80,
	coerce_a		= 0x82,
	coerce_s		= 0x85,
	construct		= 0x42,
	constructprop	= 0x4a,
	constructsuper	= 0x49,
	convert_b		= 0x76,
	convert_i		= 0x73,
	convert_d		= 0x75,
	convert_o		= 0x77,
	convert_u		= 0x74,
	convert_s		= 0x70,
	debug			= 0xef,
	debugfile		= 0xf1,
	debugline		= 0xf0,
	declocal		= 0x94,
	declocal_i		= 0xc3,
	decrement		= 0x93,
	decrement_i		= 0xc1,
	deleteproperty	= 0x6a,
	divide			= 0xa3,
	dupop			= 0x2a,
	dxns			= 0x06,
	dxnslate		= 0x07,
	equals			= 0xab,
	esc_xattr		= 0x72,
	esc_xelem		= 0x71,
	findproperty	= 0x5e,
	findpropstrict	= 0x5d,
	getdescendants	= 0x59,
	getglobalscope	= 0x64,
	getglobalslot	= 0x6e,
	getlex			= 0x60,
	getlocal		= 0x62,
	getlocal_		= 0xd0,
	getproperty		= 0x66,
	getscopeobject	= 0x65,
	getslot			= 0x6c,
	getsuper		= 0x04,
	greaterequals	= 0xb0,
	greaterthan		= 0xaf,
	hasnext			= 0x1f,
	hasnext2		= 0x32,
	ifeq			= 0x13,
	iffalse			= 0x12,
	ifge			= 0x18,
	ifgt			= 0x17,
	ifle			= 0x16,
	iflt			= 0x15,
	ifnge			= 0x0f,
	ifngt			= 0x0e,
	ifnle			= 0x0d,
	ifnlt			= 0x0c,
	ifne			= 0x14,
	ifstricteq		= 0x19,
	ifstrictne		= 0x1a,
	iftrue			= 0x11,
	in				= 0xb4,
	inclocal		= 0x92,
	inclocal_i		= 0xc2,
	increment		= 0x91,
	increment_i		= 0xc0,
	initproperty	= 0x68,
	instanceof		= 0xb1,
	istype			= 0xb2,
	istypelate		= 0xb3,
	jump			= 0x10,
	kill			= 0x08,
	label			= 0x09,
	lessequals		= 0xae,
	lessthan		= 0xad,
	lookupswitch	= 0x1b,
	lshift			= 0xa5,
	modulo			= 0xa4,
	multiply		= 0xa2,
	multiply_i		= 0xc7,
	negate			= 0x90,
	negate_i		= 0xc4,
	newactivation	= 0x57,
	newarray		= 0x56,
	newcatch		= 0x5a,
	newclass		= 0x58,
	newfunction		= 0x40,
	newobject		= 0x55,
	nextname		= 0x1e,
	nextvalue		= 0x23,
	nop				= 0x02,
	_not			= 0x96,
	pop				= 0x29,
	popscope		= 0x1d,
	pushbyte		= 0x24,
	pushdouble		= 0x2f,
	pushfalse		= 0x27,
	pushint			= 0x2d,
	pushnamespace	= 0x31,
	pushnan			= 0x28,
	pushnull		= 0x20,
	pushscope		= 0x30,
	pushshort		= 0x25,
	pushstring		= 0x2c,
	pushtrue		= 0x26,
	pushuint		= 0x2e,
	pushundefined	= 0x21,
	pushwith		= 0x1c,
	returnvalue		= 0x48,
	returnvoid		= 0x47,
	rshift			= 0xa6,
	setlocal		= 0x63,
	setlocal_		= 0xd4,
	setglobalslot	= 0x6f,
	setproperty		= 0x61,
	setslot			= 0x6d,
	setsuper		= 0x05,
	strictequals	= 0xac,
	subtract		= 0xa1,
	subtract_i		= 0xc6,
	swap			= 0x2b,
	throws			= 0x03,
	_typeof			= 0x95,
	urshift			= 0xa7,
};

enum PARAMS {
	NONE	= 0,
	U30x1	= 1,
	U30x2	= 2,
	OFF24	= 3,
	SWITCH	= 4,
	ABYTE	= 5,
	XDEBUG	= 6,
	UINTx2	= 7,
	MULTINAME=8,
};

static struct {const char *op; PARAMS params;} ops[] = {
	"00",					NONE,
	"01",					NONE,
	"nop",					NONE,
	"throws",				NONE,
	"getsuper",				U30x1,
	"setsuper",				U30x1,
	"dxns",					U30x1,
	"dxnslate",				NONE,
	"kill",					U30x1,
	"label",				NONE,
	"0a",					NONE,
	"0b",					NONE,
	"ifnlt",				OFF24,
	"ifnle",				OFF24,
	"ifngt",				OFF24,
	"ifnge",				OFF24,
	"jump",					OFF24,
	"iftrue",				OFF24,
	"iffalse",				OFF24,
	"ifeq",					OFF24,
	"ifne",					OFF24,
	"iflt",					OFF24,
	"ifle",					OFF24,
	"ifgt",					OFF24,
	"ifge",					OFF24,
	"ifstricteq",			OFF24,
	"ifstrictne",			OFF24,
	"lookupswitch",			SWITCH,
	"pushwith",				NONE,
	"popscope",				NONE,
	"nextname",				NONE,
	"hasnext",				NONE,
	"pushnull",				NONE,
	"pushundefined",		NONE,
	"22",					NONE,
	"nextvalue",			NONE,
	"pushbyte",				ABYTE,
	"pushshort",			U30x1,
	"pushtrue",				NONE,
	"pushfalse",			NONE,
	"pushnan",				NONE,
	"pop",					NONE,
	"dup",					NONE,
	"swap",					NONE,
	"pushstring",			U30x1,
	"pushint",				U30x1,
	"pushuint",				U30x1,
	"pushdouble",			U30x1,
	"pushscope",			NONE,
	"pushnamespace",		U30x1,
	"hasnext2",				UINTx2,
	"33",					NONE,
	"34",					NONE,
	"35",					NONE,
	"36",					NONE,
	"37",					NONE,
	"38",					NONE,
	"39",					NONE,
	"3a",					NONE,
	"3b",					NONE,
	"3c",					NONE,
	"3d",					NONE,
	"3e",					NONE,
	"3f",					NONE,
	"newfunction",			U30x1,
	"call",					U30x1,
	"construct",			U30x1,
	"callmethod",			U30x2,
	"callstatic",			U30x2,
	"callsuper",			U30x2,
	"callproperty",			U30x2,
	"returnvoid",			NONE,
	"returnvalue",			NONE,
	"constructsuper",		U30x1,
	"constructprop",		U30x2,
	"4b",					NONE,
	"callproplex",			U30x2,
	"4d",					NONE,
	"callsupervoid",		U30x2,
	"callpropvoid",			U30x2,
	"50",					NONE,
	"51",					NONE,
	"52",					NONE,
	"53",					NONE,
	"54",					NONE,
	"newobject",			U30x1,
	"newarray",				U30x1,
	"newactivation",		NONE,
	"newclass",				U30x1,
	"getdescendants",		U30x1,
	"newcatch",				U30x1,
	"5b",					NONE,
	"5c",					NONE,
	"findpropstrict",		U30x1,
	"findproperty",			MULTINAME,
	"5f",					NONE,
	"getlex",				U30x1,
	"setproperty",			U30x1,
	"getlocal",				U30x1,
	"setlocal",				U30x1,
	"getglobalscope",		NONE,
	"getscopeobject",		U30x1,
	"getproperty",			U30x1,
	"67",					NONE,
	"initproperty",			U30x1,
	"69",					NONE,
	"deleteproperty",		U30x1,
	"6b",					NONE,
	"getslot",				U30x1,
	"setslot",				U30x1,
	"getglobalslot",		U30x1,
	"setglobalslot",		U30x1,
	"convert_s",			NONE,
	"esc_xelem",			NONE,
	"esc_xattr",			NONE,
	"convert_i",			NONE,
	"convert_u",			NONE,
	"convert_d",			NONE,
	"convert_b",			NONE,
	"convert_o",			NONE,
	"checkfilter",			NONE,
	"79",					NONE,
	"7a",					NONE,
	"7b",					NONE,
	"7c",					NONE,
	"7d",					NONE,
	"7e",					NONE,
	"7f",					NONE,
	"coerce",				U30x1,
	"81",					NONE,
	"coerce_a",				NONE,
	"83",					NONE,
	"84",					NONE,
	"coerce_s",				NONE,
	"astype",				NONE,
	"astypelate",			NONE,
	"88",					NONE,
	"89",					NONE,
	"8a",					NONE,
	"8b",					NONE,
	"8c",					NONE,
	"8d",					NONE,
	"8e",					NONE,
	"8f",					NONE,
	"negate",				NONE,
	"increment",			NONE,
	"inclocal",				U30x1,
	"decrement",			NONE,
	"declocal",				U30x1,
	"typeof",				NONE,
	"not",					NONE,
	"bitnot",				NONE,
	"98",					NONE,
	"99",					NONE,
	"9a",					NONE,
	"9b",					NONE,
	"9c",					NONE,
	"9d",					NONE,
	"9e",					NONE,
	"9f",					NONE,
	"add ",					NONE,
	"subtract",				NONE,
	"multiply",				NONE,
	"divide",				NONE,
	"modulo",				NONE,
	"lshift",				NONE,
	"rshift",				NONE,
	"urshift",				NONE,
	"bitand",				NONE,
	"bitor",				NONE,
	"bitxor",				NONE,
	"equals",				NONE,
	"strictequals",			NONE,
	"lessthan",				NONE,
	"lessequals",			NONE,
	"greaterthan",			NONE,
	"greaterequals",		NONE,
	"instanceof",			NONE,
	"istype",				U30x1,
	"istypelate",			NONE,
	"in",					NONE,
	"b5",					NONE,
	"b6",					NONE,
	"b7",					NONE,
	"b8",					NONE,
	"b9",					NONE,
	"ba",					NONE,
	"bb",					NONE,
	"bc",					NONE,
	"bd",					NONE,
	"be",					NONE,
	"bf",					NONE,
	"increment_i",			NONE,
	"decrement_i",			NONE,
	"inclocal_i",			U30x1,
	"declocal_i",			U30x1,
	"negate_i",				NONE,
	"add_i",				NONE,
	"subtract_i",			NONE,
	"multiply_i",			NONE,
	"c8",					NONE,
	"c9",					NONE,
	"ca",					NONE,
	"cb",					NONE,
	"cc",					NONE,
	"cd",					NONE,
	"ce",					NONE,
	"cf",					NONE,
	"getlocal_0",			NONE,
	"getlocal_1",			NONE,
	"getlocal_2",			NONE,
	"getlocal_3",			NONE,
	"setlocal_0",			NONE,
	"setlocal_1",			NONE,
	"setlocal_2",			NONE,
	"setlocal_3",			NONE,
	"d8",					NONE,
	"d9",					NONE,
	"da",					NONE,
	"db",					NONE,
	"dc",					NONE,
	"dd",					NONE,
	"de",					NONE,
	"df",					NONE,
	"e0",					NONE,
	"e1",					NONE,
	"e2",					NONE,
	"e3",					NONE,
	"e4",					NONE,
	"e5",					NONE,
	"e6",					NONE,
	"e7",					NONE,
	"e8",					NONE,
	"e9",					NONE,
	"ea",					NONE,
	"eb",					NONE,
	"ec",					NONE,
	"ed",					NONE,
	"ee",					NONE,
	"debug",				XDEBUG,
	"debugline",			U30x1,
	"debugfile",			U30x1,
	"f2",					NONE,
	"f3",					NONE,
	"f4",					NONE,
	"f5",					NONE,
	"f6",					NONE,
	"f7",					NONE,
	"f8",					NONE,
	"f9",					NONE,
	"fa",					NONE,
	"fb",					NONE,
	"fc",					NONE,
	"fd",					NONE,
	"fe",					NONE,
	"ff",					NONE,
};

Disassembler::State *DisassemblerAS::Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;
	byte_reader		r(block);
	while (r.p < (uint8*)block.end()) {
		const uint8	*start	= r.p;
		uint32	offset		= uint32(start - (uint8*)block);
		uint8	op			= r.getc();
		PARAMS	params		= ops[op].params;

		uint32	a, b;//, c, d;

		switch (params) {
			case MULTINAME:
			case U30x1:	a = r.get<u30>(); break;

			case U30x2:	a = r.get<u30>(); b = r.get<u30>(); break;

			case OFF24: a = r.get<s24>(); break;
			case SWITCH: break;
			case ABYTE:	a = r.getc(); break;
			case XDEBUG: break;
			default: break;
		}
		buffer_accum<1024>	ba("%08x ", offset);
		const uint8 *p = start;
		while (p < r.p)
			ba.format("%02x ", *p++);
		while (p < start + 8) {
			ba << "   ";
			p++;
		}

		ba << ops[op].op;

//		switch (params) {
//			case MULTINAME:
//		}

		state->lines.push_back((const char*)ba);
	}
	return state;
}

uint8 DisassemblerAS::GetNextOp(byte_reader &r, uint32 *params) {
	uint8	op	= r.getc();
	switch (ops[op].params) {
		case MULTINAME:
		case U30x1:	params[0] = r.get<u30>(); break;
		case U30x2:	params[0] = r.get<u30>(); params[1] = r.get<u30>(); break;
		case OFF24: params[0] = r.get<s24>(); break;
		case SWITCH: break;
		case ABYTE:	params[0] = r.getc(); break;
		case XDEBUG: break;
	}
	return op;
}
#endif
