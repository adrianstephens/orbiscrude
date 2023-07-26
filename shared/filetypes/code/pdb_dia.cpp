#include "iso/iso_files.h"
#include "hashes/fnv.h"
#include "base/algorithm.h"
#include "extra/text_stream.h"
#include "dia2.h"
#include "com.h"
#include "windows/registry.h"
#include "windows/iso_istream.h"

//-----------------------------------------------------------------------------
//	Mircosoft debug information using DIA interface
//-----------------------------------------------------------------------------

using namespace iso;
enum {
	UNDNAME_COMPLETE				= 0x0000,		// Enables full undecoration.
	UNDNAME_NO_LEADING_UNDERSCORES	= 0x0001,		// Removes leading underscores from Microsoft extended keywords.
	UNDNAME_NO_MS_KEYWORDS			= 0x0002,		// Disables expansion of Microsoft extended keywords.
	UNDNAME_NO_FUNCTION_RETURNS		= 0x0004,		// Disables expansion of return type for primary declaration.
	UNDNAME_NO_ALLOCATION_MODEL		= 0x0008,		// Disables expansion of the declaration model.
	UNDNAME_NO_ALLOCATION_LANGUAGE	= 0x0010,		// Disables expansion of the declaration language specifier.
	UNDNAME_NO_THISTYPE				= 0x0060,		// Disables all modifiers on the this type.
	UNDNAME_NO_ACCESS_SPECIFIERS	= 0x0080,		// Disables expansion of access specifiers for members.
	UNDNAME_NO_THROW_SIGNATURES		= 0x0100,		// Disables expansion of "throw-signatures" for functions and pointers to functions.
	UNDNAME_NO_MEMBER_TYPE			= 0x0200,		// Disables expansion of static or virtual members.
	UNDNAME_NO_RETURN_UDT_MODEL		= 0x0400,		// Disables expansion of the Microsoft model for UDT returns.
	UNDNAME_32_BIT_DECODE			= 0x0800,		// Undecorates 32-bit decorated names.
	UNDNAME_NAME_ONLY				= 0x1000,		// Gets only the name for primary declaration; returns just [scope::]name. Expands template params.
	UNDNAME_TYPE_ONLY				= 0x2000,		// Input is just a type encoding; composes an abstract declarator.
	UNDNAME_HAVE_PARAMETERS			= 0x4000,		// The real template parameters are available.
	UNDNAME_NO_ECSU					= 0x8000,		// Suppresses enum/class/struct/union.
	UNDNAME_NO_IDENT_CHAR_CHECK		= 0x10000,		// Suppresses check for valid identifier characters.
	UNDNAME_NO_PTR64				= 0x20000,		// Does not include ptr64 in output.
	UNDNAME_NO_ELLIPSIS				= 0x40000,
};
//-----------------------------------------------------------------------------
//	DIA helpers
//-----------------------------------------------------------------------------

template<typename E> union enum_dword {
	E		e;
	DWORD	d;
	DWORD*	operator&()		{ return &d; }
	operator E()	const	{ return e; }
	enum_dword()			{}
	enum_dword(E _e) : e(_e){}
	friend E	get(enum_dword x)	{ return x; }
};

template<typename E> enum_dword<E> get_enum(E e) { return e; }

static const char* tags[] = {
	"",
	"Executable (Global)",
	"Compiland",
	"CompilandDetails",
	"CompilandEnv",
	"Function",
	"Block",
	"Data",
	"Unused",
	"Label",
	"PublicSymbol",
	"UDT",
	"Enum",
	"FunctionType",
	"PointerType",
	"ArrayType",
	"BaseType",
	"Typedef",
	"BaseClass",
	"Friend",
	"FunctionArgType",
	"FuncDebugStart",
	"FuncDebugEnd",
	"UsingNamespace",
	"VTableShape",
	"VTable",
	"Custom",
	"Thunk",
	"CustomType",
	""
};

static const char *kinds[] = {
	"Unknown",
	"Local",
	"Static Local",
	"Parameter",
	"Object Pointer",
	"File Static",
	"Global",
	"Member",
	"Static Member",
	"Constant"
};

ISO::TypeUser	bt_currency("currency");
ISO::TypeUser	bt_date		("date");
ISO::TypeUser	bt_variant	("variant");
ISO::TypeUser	bt_complex	("complex");
ISO::TypeUser	bt_bit		("bit");
ISO::TypeUser	bt_bstr		("bstr");
ISO::TypeUser	bt_hresult	("hresult");

static const ISO::Type* basic_type_table[] = {
	0,
	0,						// 1
	ISO::getdef<char>(),		// 2
	ISO::getdef<wchar_t>(),	// 3
	0,
	0,
	ISO::getdef<int>(),		// 6
	ISO::getdef<uint32>(),	// 7
	ISO::getdef<float>(),	// 8
	0,						// 9
	ISO::getdef<bool>(),		// 10
	0,
	0,
	ISO::getdef<int32>(),	// 13
	ISO::getdef<uint32>(),	// 14
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	&bt_currency,			// 25
	&bt_date,				// 26
	&bt_variant,			// 27
	&bt_complex,			// 28
	&bt_bit,				// 29
	&bt_bstr,				// 30
	&bt_hresult,			// 31
};
static const ISO::Type* int_type_table[][2] = {
	{ISO::TypeInt::make<8,	0,ISO::TypeInt::NONE>(), ISO::TypeInt::make<8,	0,ISO::TypeInt::SIGN>()},
	{ISO::TypeInt::make<16,	0,ISO::TypeInt::NONE>(), ISO::TypeInt::make<16,	0,ISO::TypeInt::SIGN>()},
	{ISO::TypeInt::make<32,	0,ISO::TypeInt::NONE>(), ISO::TypeInt::make<32,	0,ISO::TypeInt::SIGN>()},
	{ISO::TypeInt::make<64,	0,ISO::TypeInt::NONE>(), ISO::TypeInt::make<64,	0,ISO::TypeInt::SIGN>()},
	{ISO::TypeInt::make<128,0,ISO::TypeInt::NONE>(), ISO::TypeInt::make<128,0,ISO::TypeInt::SIGN>()},
};

static const char* data_kinds[] = {
	"Unknown",
	"Local",
	"Static Local",
	"Parameter",
	"Object Pointer",
	"File Static",
	"Global",
	"Member",
	"Static Member",
	"Constant"
};

static const char* basic_types[] = {
	0,
	"void",		// 1
	"char",		// 2
	"wchar_t",	// 3
	0,
	0,
	"int",		// 6
	"uint32",	// 7
	"float",	// 8
	0,			// 9
	"bool",		// 10
	0,
	0,
	"long",		// 13
	"unsigned long",	// 14
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	"CURRENCY",	// 25
	"DATE",		// 26
	"VARIANT",	// 27
	"COMPLEX",	// 28
	"BIT",		// 29
	"BSTR",		// 30
	"HRESULT",	// 31
};
static const char* integer_types[] = {
	"char",
	"short",
	"int",
	"__int64",
};
static const char *struct_kinds[] = {
	"struct",
	"class",
	"union",
	"interface",
};

uint32 GetBound(IDiaSymbol *bound) {
	auto	tag = get_enum(SymTagNull);
	//enum_dword<SymTagEnum>		tag;
	enum_dword<LocationType>	loc;
	com_string	name;
	bound->get_symTag(&tag);
	bound->get_locationType(&loc);
	if (tag == SymTagData && loc == LocIsConstant) {
		com_variant v;
		bound->get_value(&v);
		return v;
//	} else if (bound->get_name(&name) == S_OK) {
//		printf("%ws", name);
	}
	return 0;
}

using ISO::MakePtr;

ISO_ptr<void> MakePtr(tag id, com_variant v) {
	switch (v.type()) {
		case VT_BOOL:	return ISO::MakePtr(id, v.boolVal == VARIANT_TRUE);
		case VT_I1:		return ISO::MakePtr(id, v.cVal);
		case VT_UI1:	return ISO::MakePtr(id, v.bVal);
		case VT_I2:		return ISO::MakePtr(id, v.iVal);
		case VT_UI2:	return ISO::MakePtr(id, v.uiVal);
		case VT_I4:		return ISO::MakePtr(id, (int32)	v);
		case VT_UI4:	return ISO::MakePtr(id, (uint32)v);
		case VT_I8:		return ISO::MakePtr(id, (int64)	v);
		case VT_UI8:	return ISO::MakePtr(id, (uint64)v);
		case VT_R4:		return ISO::MakePtr(id, (float)	v);
		case VT_R8:		return ISO::MakePtr(id, (double)v);
		case VT_BSTR:	return ISO::MakePtr(id, (BSTR)	v);
	}
	return ISO_NULL;
}

template<typename T, typename R, typename E> R *FindItem(const com_ptr<E> &enumerator) {
	com_ptr<T>	item;
	R			*result;
	ULONG		n = 0;
	while (SUCCEEDED(enumerator->Next(1, &item, &n)) && n == 1) {
		if (item.query(&result) == S_OK)
			return result;
		item.clear();
	}
	return 0;
}

template<typename T, typename E> bool FindItemByRVA(const com_ptr<E> &enumerator, DWORD rva) {
	long	first = 0, last;
	enumerator->get_Count(&last);

	for (long count = last - first; count; count >>= 1) {
		long middle = first + (count >> 1);

		com_ptr<T>	item;
		DWORD		item_rva;
		if (FAILED(enumerator->Item(middle, &item)) || FAILED(item->get_relativeVirtualAddress(&item_rva)))
			return false;

		if (item_rva < rva) {
			first = ++middle;
			--count;
		}
	}
	return SUCCEEDED(enumerator->Reset()) && SUCCEEDED(enumerator->Skip(first));
}

template<> bool FindItemByRVA<IDiaSymbol,IDiaEnumSymbolsByAddr>(const com_ptr<IDiaEnumSymbolsByAddr> &enumerator, DWORD rva) {
	com_ptr<IDiaSymbol>	item, item2;
	ULONG		n = 0;
	return SUCCEEDED(enumerator->symbolByRVA(rva, &item)) && SUCCEEDED(enumerator->Prev(1, &item2, &n));
}

//-----------------------------------------------------------------------------
//	Type stuff
//-----------------------------------------------------------------------------

uint64 GetTypeAlignment(IDiaSymbol *type) {
	auto	tag	= get_enum(SymTagNull);
	type->get_symTag(&tag);

	switch (tag) {
		case SymTagBaseType:
		case SymTagPointerType:
		case SymTagEnum: {
			uint64	size;
			type->get_length(&size);
			return size;
		}

		case SymTagArrayType: {
			com_ptr<IDiaSymbol> sub;
			type->get_type(&sub);
			return GetTypeAlignment(sub);
		}

		case SymTagUDT: {
			// bases
			uint64	alignment = 1;
			for (int i = 0; i < 2; i++) {
				com_ptr<IDiaEnumSymbols>	children;
				long	num = 0;
				DWORD	num2;
				if (type->findChildren(i ? SymTagData : SymTagBaseClass, NULL, nsNone, &children) == S_OK && children && SUCCEEDED(children->get_Count(&num)) && num) {
					auto	elements	= new_auto(com_ptr<IDiaSymbol>, num);
					if (SUCCEEDED(children->Next(num, &elements[0], &num2)) && num2 == num) {
						for (int i = 0; i < num; i++) {
							com_ptr<IDiaSymbol>	type;
							if (elements[i]->get_type(&type) == S_OK)
								alignment	= max(alignment, GetTypeAlignment(type));
						}
					}
				}
			}
			return alignment;
		}

	}
	return 1;
}

struct BasicTypeInfo {
	enum_dword<BasicType>	bt;
	uint64					size;
	BasicTypeInfo() : bt(btNoType), size(0) {}
	BasicTypeInfo(const com_ptr<IDiaSymbol> &type) : bt(btNoType), size(0) {
		DWORD	tag;
		type->get_symTag(&tag);
		if (tag == SymTagBaseType) {
			type->get_baseType(&bt);
			type->get_length(&size);
		}
	}
	bool operator==(const BasicTypeInfo &b) const {
		return bt == b.bt && size == b.size;
	}
	bool operator!=(const BasicTypeInfo &b) const {
		return bt != b.bt || size != b.size;
	}
};

//-----------------------------------------------------------------------------
//	ISODiaSession
//-----------------------------------------------------------------------------

struct ISODiaSession : com_ptr<IDiaSession> {
	hash_map<uint32, const ISO::Type*>	type_hash;
	hash_map<uint32, ISO_ptr<void> >	symbol_hash;
	dynamic_array<com_ptr<IUnknown> >		tables;

	const ISO::Type	*GetType(IDiaSymbol *type);

	template<typename T, typename E> void Enumerate(anything &a, const com_ptr<E> &enumerator);
	template<typename T, typename E> ISO_ptr<anything> Enumerate(tag id, const com_ptr<E> &enumerator) {
		ISO_ptr<anything>	p(id);
		Enumerate<T>(*p, enumerator);
		return p;
	};
	template<typename T, typename E> void EnumerateByRVA(anything &a, DWORD rva, DWORD length);
	template<typename T, typename E> ISO_ptr<anything> EnumerateByRVA(tag id, DWORD rva, DWORD length) {
		ISO_ptr<anything>	p(id);
		EnumerateByRVA<T, E>(*p, rva, length);
		return p;
	};

	template<typename T> ISO_ptr<void> GetItem(T *item);
	template<typename T> ISO_ptr<void> GetItem(const com_ptr<T> &item) { return GetItem((T*)item); }

	void GetChildren(anything &a, IDiaSymbol *sym) {
		com_ptr<IDiaEnumSymbols> children;
		if (SUCCEEDED(sym->findChildren(SymTagNull, NULL, nsNone, &children)) && children)
			Enumerate<IDiaSymbol>(a, children);
	}
	ISO_ptr<anything> GetChildren(tag id, IDiaSymbol *sym) {
		ISO_ptr<anything>	p(id);
		GetChildren(*p, sym);
		return p;
	}
	template<typename T> T *GetTable();

	ISODiaSession(IDiaDataSource *source) {
		source->openSession(&*this);
	}
};

template<typename T> T *ISODiaSession::GetTable() {
#if 1
	T	*t = 0;
	for (auto i = tables.begin(), e = tables.end(); i != e; ++i) {
		if (SUCCEEDED(i->query(&t)))
			break;
	}
	if (!t) {
		com_ptr<IDiaEnumTables> tables2;
		if (SUCCEEDED((*this)->getEnumTables(&tables2)) && (t = FindItem<IDiaTable, T>(tables2)))
			tables.push_back(t);
	}
	if (t)
		t->AddRef();

	return t;
#else
	com_ptr<IDiaEnumTables> tables2;
	return SUCCEEDED((*this)->getEnumTables(&tables2)) ? FindItem<IDiaTable, T>(tables2) : 0;
#endif
}

template<> IDiaEnumSymbolsByAddr *ISODiaSession::GetTable<IDiaEnumSymbolsByAddr>() {
	IDiaEnumSymbolsByAddr	*table;
	return SUCCEEDED((*this)->getSymbolsByAddr(&table)) ? table : 0;
}

template<typename T, typename E> void ISODiaSession::Enumerate(anything &a, const com_ptr<E> &enumerator) {
	com_ptr<T>	item;
	ULONG		n = 0;
	while (SUCCEEDED(enumerator->Next(1, &item, &n)) && n == 1) {
		if (auto p = GetItem(item))
			a.Append(p);
		item.clear();
	}
}

template<typename T, typename E> void ISODiaSession::EnumerateByRVA(anything &a, DWORD rva, DWORD length) {
	com_ptr<E> enumerator	= GetTable<E>();
	if (FindItemByRVA<T>(enumerator, rva)) {
		com_ptr<T>	item;
		ULONG		n = 0;
		DWORD		item_rva;
		while (SUCCEEDED(enumerator->Next(1, &item, &n)) && n == 1 && SUCCEEDED(item->get_relativeVirtualAddress(&item_rva)) && item_rva < rva + length) {
			if (auto p = GetItem(item))
				a.Append(p);
			item.clear();
		}
	}
}

template<> void ISODiaSession::EnumerateByRVA<IDiaLineNumber,IDiaEnumLineNumbers>(anything &a, DWORD rva, DWORD length) {
	com_ptr<IDiaEnumLineNumbers> enumerator;
	if (SUCCEEDED((*this)->findLinesByRVA(rva, length, &enumerator))) {
		Enumerate<IDiaLineNumber>(a, enumerator);
	}
}

const ISO::Type *ISODiaSession::GetType(IDiaSymbol *type) {
	DWORD	id;
	if (type->get_symIndexId(&id) != S_OK)
		return 0;

	const ISO::Type *&p = type_hash[id];
	if (p)
		return p;

	auto	tag	= get_enum(SymTagNull);
	DWORD	n;

	type->get_symTag(&tag);

	switch (tag) {
		case SymTagBaseType: {
			enum_dword<BasicType>	bt;
			uint64					size;
			type->get_baseType(&bt);
			type->get_length(&size);

			if (bt == btInt || bt == btUInt) {
				int	i = size <= 1 ? 0 : size <= 2 ? 1 : size <= 4 ? 2 : size <= 8 ? 3 : 4;
				return int_type_table[i][bt == btInt];
			}
			return p = basic_type_table[bt];
		}

		case SymTagPointerType: {
			com_ptr<IDiaSymbol>		sub;
			type->get_type(&sub);
			ISO::TypeReference *t = new ISO::TypeReference(0);
			p			= t;
			t->subtype	= GetType(sub);
			return t;
		}

		case SymTagArrayType: {
			com_ptr<IDiaSymbol> sub;
			type->get_type(&sub);
			const ISO::Type	*t = GetType(sub);
			DWORD			rank;
			LONG			count;
			com_ptr<IDiaEnumSymbols> i;

			if (type->get_rank(&rank) == S_OK) {
				com_ptr<IDiaEnumSymbols> i;
				if (type->findChildren(SymTagDimension, NULL, nsNone, &i) == S_OK && i) {
					com_ptr<IDiaSymbol> sym;
					while (i->Next(1, &sym, &n) == S_OK && n == 1) {
						com_ptr<IDiaSymbol>	bound;
						uint32	lo = 0, hi = 0;
						if (sym->get_lowerBound(&bound) == S_OK)
							lo = GetBound(bound);
						bound.clear();
						if (sym->get_upperBound(&bound) == S_OK)
							hi = GetBound(bound);
						bound.clear();
						t = new ISO::TypeArray(t, hi - lo);
					}
				}

			} else if (type->findChildren(SymTagCustomType, NULL, nsNone, &i) == S_OK && i && SUCCEEDED(i->get_Count(&count)) && count > 0){
				com_ptr<IDiaSymbol>	sym;
				while (i->Next(1, &sym, &n) == S_OK && n == 1) {
					t = new ISO::TypeArray(t, 1);
					sym.clear();
				}
			} else {
				uint64 lenArray;
				uint64 lenElem;
				if (type->get_length(&lenArray) == S_OK && sub->get_length(&lenElem) == S_OK)
					t = new ISO::TypeArray(t, lenArray / max(lenElem, 1));
			}
			return p = t;
		}

		case SymTagUDT: {
			com_ptr<IDiaEnumSymbols>	bases, data, funcs;
			long						num_bases = 0, num_data = 0, num_funcs = 0;

			if (type->findChildren(SymTagBaseClass, NULL, nsNone, &bases) == S_OK && bases)
				bases->get_Count(&num_bases);
			if (type->findChildren(SymTagData, NULL, nsNone, &data) == S_OK && data)
				data->get_Count(&num_data);
			if (type->findChildren(SymTagFunction, NULL, nsNone, &funcs) == S_OK && funcs)
				funcs->get_Count(&num_funcs);

			ISO::TypeComposite	*comp = new(num_bases + num_data + num_funcs) ISO::TypeComposite;
			p = comp;

			// bases
			com_ptr<IDiaSymbol>	base;
			while (SUCCEEDED(bases->Next(1, &base, &n)) && n == 1) {
				com_ptr<IDiaSymbol>	type;
				if (base->get_type(&type) == S_OK) {
					long	offset;
					uint64	size;
					base->get_offset(&offset);
					type->get_length(&size);
					comp->Add(ISO::Element(none, GetType(type), offset, size));
				}
				base.clear();
			}

			if (num_data) {	// elements
				com_ptr<IDiaSymbol>	*elements = new com_ptr<IDiaSymbol>[num_data];
				if (SUCCEEDED(data->Next(num_data, &elements[0], &n)) && n == num_data) {
					auto		loc	= get_enum(LocIsNull);
					long		offset;
					for (com_ptr<IDiaSymbol> *i = elements, *ie = elements + n; i < ie; i++) {
						(*i)->get_locationType(&loc);
						(*i)->get_offset(&offset);
						if (loc == LocIsBitField) {
							com_ptr<IDiaSymbol> *i2 = i;
							while (++i2 < ie) {
								(*i2)->get_locationType(&loc);
								if (loc != LocIsBitField)
									break;
							}
							ISO::BitPacked	*bits = new(i2 - i) ISO::BitPacked;
							for (;i < i2; ++i) {
								com_string			name;
								com_ptr<IDiaSymbol>	type;
								DWORD				bit;
								uint64				size;
								(*i)->get_name(&name);
								(*i)->get_type(&type);
								(*i)->get_bitPosition(&bit);
								(*i)->get_length(&size);
								bits->Add(GetType(type), name, bit, size);
							}
							comp->Add(ISO::Element(none, bits, offset, 0));
						} else {
							com_string			name;
							com_ptr<IDiaSymbol>	type;
							uint64				size;
							(*i)->get_name(&name);
							(*i)->get_type(&type);
							type->get_length(&size);
							comp->Add(ISO::Element(name, GetType(type), offset, size));
						}
					}
				}
				delete[] elements;
			}

			// functions
			com_ptr<IDiaSymbol>	func;
			while (SUCCEEDED(funcs->Next(1, &func, &n)) && n == 1) {
				com_ptr<IDiaSymbol>	type;
				if (func->get_type(&type) == S_OK) {
					com_string		name;
					BOOL			c = FALSE, s;
					func->get_name(&name);
					func->get_isStatic(&s);

					if (!s) {
						com_ptr<IDiaSymbol>	obj, this_type;
						type->get_objectPointerType(&obj);
						obj->get_type(&this_type);
						this_type->get_constType(&c);
					}

					comp->Add(ISO::Element(name, GetType(type), 0, 0));
				}
				func.clear();
			}

			ISO_ASSERT(comp->Count() <= num_bases + num_data + num_funcs);

			return comp;
		}

		case SymTagEnum: {
			com_ptr<IDiaEnumSymbols> i;
			long			count	= 0;
			if (type->findChildren(SymTagData, NULL, nsNone, &i) == S_OK && SUCCEEDED(i->get_Count(&count))) {
				ISO::TypeEnum		*t	= new(count + 1) ISO::TypeEnum(32);
				ISO::Enum			*e	= t->begin();
				com_ptr<IDiaSymbol>	v;
				while (SUCCEEDED(i->Next(1, &v, &n)) && n == 1) {
					com_string	name;
					com_variant val;
					v->get_name(&name);
					v->get_value(&val);
					e++->set(name, val);
					v.clear();
				}
				iso::clear(*e);
				return p = t;
			}
			break;
		}

		case SymTagFunctionType: {
			com_ptr<IDiaEnumSymbols> args;
			long			num_args	= 0;
			if (SUCCEEDED(type->findChildren(SymTagFunctionArgType, NULL, nsNone, &args)) && args && SUCCEEDED(args->get_Count(&num_args))) {
				// params
				ISO::TypeComposite	*params	= new(num_args) ISO::TypeComposite;
				ISO::TypeFunction	*func	= new ISO::TypeFunction(0, params);
				p = func;

				com_ptr<IDiaSymbol>	ret;
				if (type->get_type(&ret) == S_OK)
					func->rettype = GetType(ret);

				com_ptr<IDiaSymbol>	arg;
				while (SUCCEEDED(args->Next(1, &arg, &n)) && n == 1) {
					com_ptr<IDiaSymbol>	type;
					if (arg->get_type(&type) == S_OK) {
						com_string	name;
						arg->get_name(&name);
						params->Add(ISO::Element(name, GetType(type), 0, 0));
					}
					arg.clear();
				}
				return func;
			}
			break;
		}
	}
	return 0;
}

template<> ISO_ptr<void> ISODiaSession::GetItem(IDiaSymbol *sym) {
	DWORD	id;
	if (sym->get_symIndexId(&id) == S_OK) {
		ISO_ptr<void>	&p2 = symbol_hash[id];
		if (p2)
			return p2;

		com_string	name;
		auto		tag	= get_enum(SymTagNull);
		//sym->get_name(&name);
		sym->get_undecoratedNameEx(UNDNAME_COMPLETE, &name);
		sym->get_symTag(&tag);

		switch (tag) {
			case SymTagPublicSymbol: {
				com_ptr<IDiaSymbol>	type;
				auto	loc		= get_enum(LocIsNull);
				auto	data	= get_enum(DataIsUnknown);

				sym->get_locationType(&loc);
				sym->get_dataKind(&data);
				if (loc == LocIsConstant && data == DataIsConstant) {
					com_variant value;
					if (sym->get_value(&value) == S_OK)
						return p2 = MakePtr(name, value);

				}
				//if (loc == LocIsStatic) {
					DWORD		rva;
					if (SUCCEEDED(sym->get_relativeVirtualAddress(&rva)))
						return ISO_ptr<xint32>(name, rva);
				//}
				break;
			}
			case SymTagData: {
				com_ptr<IDiaSymbol>	type;
				auto	loc		= get_enum(LocIsNull);
				auto	data	= get_enum(DataIsUnknown);

				sym->get_locationType(&loc);
				sym->get_dataKind(&data);
				if (loc == LocIsConstant && data == DataIsConstant) {
					com_variant value;
					if (sym->get_value(&value) == S_OK)
						return p2 = MakePtr(name, value);

				} else if (sym->get_type(&type) == S_OK) {
					ISO_ptr<anything>	p(name);
					ULONGLONG			va;
					p2	= p;
					p->Append(MakePtr("type", GetType(type)));
					if (SUCCEEDED(sym->get_virtualAddress(&va)))
						p->Append(MakePtr("va", xint64(va)));

					return p;
				}
				break;
			}
			case SymTagTypedef: {
				com_ptr<IDiaSymbol>	type;
				if (sym->get_type(&type) == S_OK) {
					auto	p = MakePtr(name, (const ISO::Type*)0);
					p2	= p;
					*p	= GetType(type);
					return p;
				}
				break;
			}
			case SymTagUDT:
			case SymTagEnum: {
				auto	p = MakePtr(name, (const ISO::Type*)0);
				p2	= p;
				*p	= GetType(sym);
				return p;
			}

			case SymTagFunction: {
				ISO_ptr<anything>	p(name);
				p2	= p;
				DWORD		rva;
				ULONGLONG	length;

				if (SUCCEEDED(sym->get_relativeVirtualAddress(&rva)))
					p->Append(MakePtr("rva", xint32(rva)));
				if (SUCCEEDED(sym->get_length(&length)))
					p->Append(MakePtr("length", xint32(length)));

				auto		loc	= get_enum(LocIsNull);
				sym->get_locationType(&loc);// == S_OK && loc == LocIsStatic) {

				com_ptr<IDiaSymbol>	type;
				if (sym->get_type(&type) == S_OK) {
					p->Append(MakePtr("type", GetType(type)));
					com_ptr<IDiaSymbol> block;
					if (SUCCEEDED((*this)->findSymbolByRVA(rva, SymTagBlock, &block)) && block)
						p->Append(GetChildren("block", block));
				}
				p->Append(EnumerateByRVA<IDiaLineNumber, IDiaEnumLineNumbers>("lines", rva, length));

				return p;
			}

			case SymTagAnnotation:
			case SymTagBlock:
			case SymTagCompiland:
			{
				ISO_ptr<anything>	p(name);
				p2	= p;
				GetChildren(*p, sym);
				return p;
			}

			case SymTagCompilandEnv: {
				com_variant value;
				if (sym->get_value(&value) == S_OK)
					return p2 = MakePtr(name, value);
				break;
			}

			case SymTagFuncDebugStart:
			case SymTagFuncDebugEnd:
			case SymTagLabel:
			case SymTagCompilandDetails:
				break;

			default:
				ISO_TRACEF("Unhandled ") << (uint32)tag << "\n";
				break;
		}
	}
	return ISO_NULL;
}

template<> ISO_ptr<void> ISODiaSession::GetItem(IDiaSourceFile *source_file) {
	com_string			filename;
	source_file->get_fileName(&filename);
	ISO_ptr<anything>	p(filename);

	com_ptr<IDiaEnumSymbols>	compilands;
	if (source_file->get_compilands(&compilands) == S_OK)
		p->Append(Enumerate<IDiaSymbol>("compilands", compilands));
	return p;
}

template<> ISO_ptr<void> ISODiaSession::GetItem(IDiaSegment *segment) {
	DWORD		rva, section, length;
	ULONGLONG	va;

	ISO_ptr<anything>	p(0);

	if (SUCCEEDED(segment->get_relativeVirtualAddress(&rva)))
		p->Append(MakePtr("rva", xint32(rva)));
	if (SUCCEEDED(segment->get_addressSection(&section)))
		p->Append(MakePtr("section", xint32(section)));
	if (SUCCEEDED(segment->get_virtualAddress(&va)))
		p->Append(MakePtr("va", xint64(va)));
	if (SUCCEEDED(segment->get_length(&length)))
		p->Append(MakePtr("length", xint32(length)));

	if (length != 0xffffffff)
		p->Append(EnumerateByRVA<IDiaSectionContrib, IDiaEnumSectionContribs>("Contribs", rva, length));

	return p;
}

struct DiaSectionContrib {
	xint32				rva, length;
	ISO_ptr<anything>	symbols;
};
ISO_DEFUSERCOMPV(DiaSectionContrib, rva, length, symbols);

template<> ISO_ptr<void> ISODiaSession::GetItem(IDiaSectionContrib *contrib) {
	DWORD				rva, length;
	com_ptr<IDiaSymbol>	sym;
	com_string			name;

	contrib->get_relativeVirtualAddress(&rva);
	contrib->get_length(&length);

	if (SUCCEEDED((*this)->findSymbolByRVA(rva, SymTagNull, &sym)))
		sym->get_name(&name);

#if 1
	ISO_ptr<DiaSectionContrib>	p(name);
	p->rva		= rva;
	p->length	= length;
	p->symbols	= EnumerateByRVA<IDiaSymbol, IDiaEnumSymbolsByAddr>("symbols", rva, length);
#else
	ISO_ptr<anything>	p(name);
	p->Append(MakePtr("rva", xint32(rva)));
	p->Append(MakePtr("length", xint32(length)));
	p->Append(EnumerateByRVA<IDiaSymbol, IDiaEnumSymbolsByAddr>("symbols", rva, length));
#endif
	return p;
}

template<> ISO_ptr<void> ISODiaSession::GetItem(IDiaFrameData *frame) {
	DWORD		len_block, len_locals, len_params;
	ULONGLONG	va;

	ISO_ptr<anything>	p(0);

	if (SUCCEEDED(frame->get_virtualAddress(&va)))
		p->Append(MakePtr("va", xint64(va)));
	if (SUCCEEDED(frame->get_lengthBlock( &len_block)))
		p->Append(MakePtr("len_block", xint32(len_block)));
	if (SUCCEEDED(frame->get_lengthLocals(&len_locals)))
		p->Append(MakePtr("len_locals", xint32(len_locals)));
	if (SUCCEEDED(frame->get_lengthParams(&len_params)))
		p->Append(MakePtr("len_params", xint32(len_params)));

	return p;
}

template<> ISO_ptr<void> ISODiaSession::GetItem(IDiaStackFrame *frame) {
	DWORD		size, len_locals, len_params;
	ULONGLONG	base;

	ISO_ptr<anything>	p(0);

	if (SUCCEEDED(frame->get_base(&base)))
		p->Append(MakePtr("base", xint64(base)));
	if (SUCCEEDED(frame->get_size( &size)))
		p->Append(MakePtr("size", xint32(size)));
	if (SUCCEEDED(frame->get_lengthLocals(&len_locals)))
		p->Append(MakePtr("len_locals", xint32(len_locals)));
	if (SUCCEEDED(frame->get_lengthParams(&len_params)))
		p->Append(MakePtr("len_params", xint32(len_params)));

	return p;
}

template<> ISO_ptr<void> ISODiaSession::GetItem(IDiaInjectedSource*source) {
	com_string	fn;
	source->get_filename(&fn);
	return ISO_ptr<string>("filename", fn);
}

template<> ISO_ptr<void> ISODiaSession::GetItem(IDiaLineNumber *line) {
	DWORD	linen, rva;
	line->get_lineNumber(&linen);
	line->get_relativeVirtualAddress(&rva);
	return MakePtr(0, make_pair(uint32(linen), xint32(rva)));
}

//-----------------------------------------------------------------------------
//	DiaSession - dumper
//-----------------------------------------------------------------------------

//template<> struct T_alignment<IDiaSymbol>	{
//	enum { value = 8 };
//};

uint32 hash(IDiaSymbol *k) {
	com_string	name;
	k->get_name(&name);
	return FNV1a<uint32>(string(name));
}

struct Depends {
	enum STATE {
		NONE, PROCESSING, FORWARD, PROCESSING_FORWARD, DUMPED,
	};
	com_ptr2<IDiaSymbol>		sym;
	dynamic_array<com_ptr2<IDiaSymbol> >	dependencies[2];
	STATE						state;
	Depends() : state(NONE)	{}
	void	Add(IDiaSymbol *type, bool ref);
	void	AddTypes(IDiaEnumSymbols *symbols, bool ref);
	void	AddSymbols(IDiaEnumSymbols *symbols, bool ref);
};

struct DiaSession : com_ptr<IDiaSession> {
	text_writer<line_counter<ostream_ref> >	w;
	hash_map<uint32, string>		name_hash;
	hash_map<IDiaSymbol*, Depends>	depends_hash;
	uint32							clashes;

	bool Check(string name) {
		string	&s = name_hash[FNV1a<uint32>(name)];
		if (s) {
			if (name != s)
				++clashes;
			return true;
		}
		s = name;
		return false;
	}

	IDiaSymbol*				Decorate(IDiaSymbol *type, string &name);
	void					Dump(IDiaSymbol *sym, int indent);
	void					DumpElements(com_ptr<IDiaSymbol> *begin, com_ptr<IDiaSymbol> *end, UdtKind kind, int indent);
	void					DumpType(IDiaSymbol *type, const string &name, int indent, bool def = false);
	void					DumpFunction(IDiaSymbol *func);
	void					DumpBlock(IDiaSymbol *block, int indent);

	void					GetDepends(IDiaSymbol *sym);
	void					GetDepends(IDiaEnumSymbols *symbols);

	DiaSession(IDiaDataSource *source, ostream_ref out) : w(out), clashes(0) {
		source->openSession(&*this);
	}
};

void DiaSession::DumpBlock(IDiaSymbol *block, int indent) {
	com_ptr<IDiaEnumSymbols> i;
	if (FAILED(block->findChildren(SymTagNull, NULL, nsNone, &i)) || !i)
		return;

	w << "{\n";

	com_ptr<IDiaSymbol>	symbol;
	auto				tag		= get_enum(SymTagNull);
	auto				kind	= get_enum(DataIsUnknown);
	DWORD				n;

	while (SUCCEEDED(i->Next(1, &symbol, &n)) && n == 1) {
		symbol->get_symTag(&tag);
		switch (tag) {
			case SymTagData: {
				com_ptr<IDiaSymbol>	type;
				com_string			name;
				if (symbol->get_type(&type) == S_OK && symbol->get_name(&name) == S_OK && name.length()) {
					w << repeat('\t', indent + 1);
					symbol->get_dataKind(&kind);
					switch (kind) {
						case DataIsStaticLocal:	w << "static ";
						case DataIsLocal:		break;
						default:				w << "//";
					}
					DumpType(type, name, indent + 1);
					w << ";\t//" << data_kinds[kind] << "\n";
				}
				break;
			}
			case SymTagAnnotation: {
				com_ptr<IDiaEnumSymbols> values;
				if (SUCCEEDED(symbol->findChildren(SymTagNull, NULL, nsNone, &values)) && values) {
					symbol.clear();
					while (values != NULL && SUCCEEDED(values->Next(1, &symbol, &n)) && n == 1) {
						com_variant value;
						if (symbol->get_value(&value) == S_OK)
							w << " <" << value.bstrVal << ">\n";
						symbol.clear();
					}
				}
				break;
			}
			case SymTagBlock:
				w << repeat('\t', indent + 1);
				DumpBlock(symbol, indent + 1);
				break;
		}
		symbol.clear();
	}
	w << repeat('\t', indent) << "}\n";
}

void DiaSession::DumpFunction(IDiaSymbol *func) {
	auto	loc	= get_enum(LocIsNull);
	uint64	length	= 0;
	DWORD	isect	= 0;
	DWORD	offset	= 0;
	DWORD	rva;

	func->get_locationType(&loc);// == S_OK && loc == LocIsStatic) {

	func->get_addressSection(&isect);
	func->get_addressOffset(&offset);
	func->get_length(&length);
	func->get_relativeVirtualAddress(&rva);

	com_ptr<IDiaSymbol>	type;
	com_string			name;
	if (func->get_type(&type) == S_OK && func->get_name(&name) == S_OK) {
		DumpType(type, name, 0);
		com_ptr<IDiaSymbol> block;
	//	if (FAILED((*this)->findSymbolByRVA(rva, SymTagBlock, &block)))
		if (SUCCEEDED((*this)->findSymbolByAddr(isect, offset, SymTagNull, &block)) && block)
			DumpBlock(block, 0);
		else
			w << ";\n";
	}

#if 0

	if (isect != 0 && length > 0) {
		com_ptr<IDiaEnumLineNumbers> lines;
		if (SUCCEEDED((*this)->findLinesByAddr(isect, offset, static_cast<DWORD>(length), &lines))) {
			com_ptr<IDiaLineNumber>	line;
			bool					first = true;
			while (SUCCEEDED(lines->Next(1, &line, &n)) && n == 1){
				DWORD	offset, seg, linenum;
				com_ptr<IDiaSymbol>		comp;
				com_ptr<IDiaSourceFile>	src;

				line->get_compiland(&comp);
				line->get_sourceFile(&src);
				line->get_addressSection(&seg);
				line->get_addressOffset(&offset);
				line->get_lineNumber(&linenum);
				w << "\tline " << linenum << " at 0x" << hex(seg) << ':' << hex(offset) << '\n';
				line.clear();
				if (first) {
					// sanity check
					com_ptr<IDiaEnumLineNumbers>	lines2;
					if (SUCCEEDED((*this)->findLinesByLinenum(comp, src, linenum, 0, &lines2))) {
						com_ptr<IDiaLineNumber> line;
						while (SUCCEEDED(lines2->Next(1, &line, &n)) && n == 1){
							DWORD offset, seg, linenum;
							line->get_addressSection(&seg);
							line->get_addressOffset(&offset);
							line->get_lineNumber(&linenum);
							w << "\t\tfound line " << linenum << " at 0x" << hex(seg) << ':' << hex(offset) << '\n';
							line.clear();
						}
					}
					first = false;
				}
			}
		}
	}
#endif
}

void DiaSession::DumpElements(com_ptr<IDiaSymbol> *begin, com_ptr<IDiaSymbol> *end, UdtKind kind, int indent) {
	if (indent > 16) {
		w << "// recursion too deep!\n";
		return;
	}

	enum_dword<LocationType>	loc;

	if (kind == UdtUnion) {
		long	start_offset, offset;
		(*begin)->get_offset(&start_offset);

		for (com_ptr<IDiaSymbol> *i = begin; i <= end;) {
			if (i < end) {
				(*i)->get_offset(&offset);
				(*i)->get_locationType(&loc);
				if (loc == LocIsBitField) {
					DWORD		bit;
					(*i)->get_bitPosition(&bit);
					offset += int(bit != 0);
				}
			}
			if (i > begin && (i == end || offset == start_offset)) {
				if (i > begin + 1) {
					w << repeat('\t', indent + 1) << "struct {\n";
					DumpElements(begin, i, UdtStruct, indent + 1);
					w << repeat('\t', indent + 1) << "};\n";
				} else {
					com_ptr<IDiaSymbol>	type;
					com_string			name;
					(*begin)->get_type(&type);
					(*begin)->get_name(&name);
 					(*begin)->get_locationType(&loc);
					w << repeat('\t', indent + 1);
					if (loc == LocIsBitField) {
						uint64		num_bits;
						(*begin)->get_length(&num_bits);
						DumpType(type, "", indent + 1);
						w << '\t' << name << ':' << num_bits;
					} else {
						DumpType(type, name, indent + 1);
					}
					w << ";\n";
				}
				begin = i;
			} else {
				++i;
			}
		}
	} else {
		int		padding		= 0;
		while (begin < end) {
			long	start_offset, prev_offset, offset;
			long	union_offset = maximum, union_max = 0;
			com_ptr<IDiaSymbol> *union_end = 0;

			(*begin)->get_offset(&start_offset);
			prev_offset = start_offset;
			for (com_ptr<IDiaSymbol> *i = begin; i < end;) {
				com_ptr<IDiaSymbol>	type;
				uint64				size = 0;

				(*i)->get_offset(&offset);
				(*i)->get_type(&type);
				type->get_length(&size);

				if (offset < prev_offset) {
					if (offset < union_offset) {
						union_offset = offset;
						union_max	= max(union_max, align(prev_offset, GetTypeAlignment(type)));
						union_end	= end;
					}
				}

				(*i++)->get_locationType(&loc);
				if (loc == LocIsBitField) {
					while (i < end && SUCCEEDED((*i)->get_locationType(&loc)) && loc == LocIsBitField)
						i++;
				}

				if (offset < union_max)
					union_end = i;

				prev_offset	= offset + size;
			}

			com_ptr<IDiaSymbol> *i = begin;
			prev_offset = start_offset;
			while (i < end) {
				(*i)->get_offset(&offset);
				if (offset >= union_offset)
					break;

				com_ptr<IDiaSymbol>	type;
				uint64				size;
 				(*i)->get_locationType(&loc);
				(*i)->get_type(&type);
				type->get_length(&size);

				if (offset != prev_offset) {
					if (offset != align(prev_offset, GetTypeAlignment(type)))
						w << repeat('\t', indent + 1) << "char\t_pdb_padding" << padding++ << "[" << offset - prev_offset << "];\n";
				}

				if (loc == LocIsBitField) {
					BasicTypeInfo	basictype;
					DWORD			prev_bit = 0;
					for (; i != end; ++i) {
						com_string	name;
						DWORD		bit;
						uint64		num_bits;

						(*i)->get_locationType(&loc);
						if (loc != LocIsBitField)
							break;
						(*i)->get_bitPosition(&bit);
						if (bit < prev_bit)
							break;

						type.clear();
						(*i)->get_name(&name);
						(*i)->get_length(&num_bits);
						(*i)->get_type(&type);

						if (basictype != type) {
							if (prev_bit > 0)
								w << ";\n";
							w << repeat('\t', indent + 1);
							DumpType(type, "", indent + 1);
							basictype = type;
							w << '\t';
						} else {
							w << ", ";
						}
						if (bit > prev_bit)
							w << ':' << bit - prev_bit << ", ";
						w << name << ':' << num_bits;
						prev_bit	= bit + num_bits;
					}
				} else {
					com_string			name;
					(*i)->get_name(&name);
					w << repeat('\t', indent + 1);
					DumpType(type, name, indent + 1);
					++i;
				}
				w << ";\n";

				prev_offset	= offset + size;
			}
			if (union_end > i) {
				w << repeat('\t', indent + 1) << "union {\n";
				DumpElements(i, union_end, UdtUnion, indent + 1);
				w << repeat('\t', indent + 1) << "};\n";
				i = union_end;
			}
			begin = i;
		}
	}
}

IDiaSymbol* DiaSession::Decorate(IDiaSymbol *type, string &name) {
	auto		tag		= get_enum(SymTagNull);
	type->get_symTag(&tag);

	switch (tag) {
		default:
			type->AddRef();
			return type;

		case SymTagPointerType: {
			com_ptr<IDiaSymbol>		sub;
			type->get_type(&sub);
			IDiaSymbol	*r = Decorate(sub, name);

			BOOL					ref;
			type->get_reference(&ref);
			name = str(ref ? "&" : "*") + name;
			return r;
		}

		case SymTagArrayType: {
			com_ptr<IDiaSymbol> sub;
			type->get_type(&sub);
			IDiaSymbol	*r = Decorate(sub, name);

			DWORD			rank;
			LONG			count;
			DWORD			n;
			com_ptr<IDiaEnumSymbols> i;

			if (type->get_rank(&rank) == S_OK) {
				if (type->findChildren(SymTagDimension, NULL, nsNone, &i) == S_OK && i) {
					com_ptr<IDiaSymbol> sym;
					while (i->Next(1, &sym, &n) == S_OK && n == 1) {
						com_ptr<IDiaSymbol>	bound;
						uint32	lo = 0, hi = 0;
						if (sym->get_lowerBound(&bound) == S_OK)
							lo = GetBound(bound);
						bound.clear();
						if (sym->get_upperBound(&bound) == S_OK)
							hi = GetBound(bound);
						bound.clear();
						name << '[' << hi - lo << ']';
					}
				}

			} else if (type->findChildren(SymTagCustomType, NULL, nsNone, &i) == S_OK && i && SUCCEEDED(i->get_Count(&count)) && count > 0){
				com_ptr<IDiaSymbol>	sym;
				while (i->Next(1, &sym, &n) == S_OK && n == 1) {
					name << "[]";
					sym.clear();
				}

			} else {
				uint64 lenArray;
				uint64 lenElem;
				if (type->get_length(&lenArray) == S_OK && sub->get_length(&lenElem) == S_OK)
					name << '[' << lenArray / max(lenElem, 1) << ']';
			}
			return r;
		}
	}
}

void DiaSession::DumpType(IDiaSymbol *type, const string &name, int indent, bool def) {
	com_string	type_name;
	bool		has_name = type->get_name(&type_name) == S_OK && type_name.length() > 0 && type_name != "<unnamed-tag>";
	if (def && !has_name)
		return;

	auto		tag		= get_enum(SymTagNull);
	type->get_symTag(&tag);
	DWORD		n;

	if (has_name && (!def|| Check(type_name))) {
		w << type_name;

	} else switch (tag) {
		case SymTagBaseType: {
			enum_dword<BasicType>	bt;
			uint64					size;
			type->get_baseType(&bt);
			type->get_length(&size);

			if (bt == btInt || bt == btUInt) {
				int	i = size <= 1 ? 0 : size <= 2 ? 1 : size <= 4 ? 2 : 3;
				if (bt == btUInt)
					w << "unsigned ";
				else if (i == 0)
					w << "signed ";
				w << integer_types[i];
			} else {
				w << basic_types[bt];
			}
			break;
		}

		case SymTagPointerType:
		case SymTagArrayType: {
			string	name2(name);
			com_ptr<IDiaSymbol>	sub = Decorate(type, name2);
			DumpType(sub, name2, indent);
			return;
		}

		case SymTagUDT: {
			enum_dword<UdtKind>	kind;
			type->get_udtKind(&kind);
			w << struct_kinds[kind];
			if (has_name)
				w << ' ' << type_name;

			// bases
			com_ptr<IDiaEnumSymbols>	bases;
			if (type->findChildren(SymTagBaseClass, NULL, nsNone, &bases) == S_OK && bases) {
				com_ptr<IDiaSymbol>	base;
				bool				first = true;
				while (SUCCEEDED(bases->Next(1, &base, &n)) && n == 1) {
					com_ptr<IDiaSymbol>	type;
					if (base->get_type(&type) == S_OK) {
						com_string		name;
						type->get_name(&name);
						w << (first ? " : " : ", ") << name;
						first = false;
					}
					base.clear();
				}
			}

			w << " {\n";

			com_ptr<IDiaEnumSymbols>	children;
			if (SUCCEEDED(type->findChildren(SymTagNull, NULL, nsfCaseInsensitive | nsfUndecoratedName, &children))) {
				com_ptr<IDiaSymbol>		sym;
				while (SUCCEEDED(children->Next(1, &sym, &n)) && n == 1) {
					Dump(sym, indent);
					sym.clear();
				}
			}

			// elements
			com_ptr<IDiaEnumSymbols>	data;
			long						num_data;
			if (type->findChildren(SymTagData, NULL, nsNone, &data) == S_OK && data && SUCCEEDED(data->get_Count(&num_data)) && num_data) {
				auto	elements	= new_auto(com_ptr<IDiaSymbol>, num_data);
				if (SUCCEEDED(data->Next(num_data, &elements[0], &n)) && n == num_data)
					DumpElements(elements, elements + n, kind, indent);
			}

			// functions
			com_ptr<IDiaEnumSymbols>	funcs;
			if (type->findChildren(SymTagFunction, NULL, nsNone, &funcs) == S_OK && funcs) {
				com_ptr<IDiaSymbol>	func;
				BOOL	c, s;
				while (SUCCEEDED(funcs->Next(1, &func, &n)) && n == 1) {
					com_ptr<IDiaSymbol>	type;
					if (func->get_type(&type) == S_OK) {
						com_string		name;
						func->get_name(&name);
						func->get_isStatic(&s);

						if (!s) {
							com_ptr<IDiaSymbol>	obj, this_type;
							type->get_objectPointerType(&obj);
							obj->get_type(&this_type);
							this_type->get_constType(&c);
						}

						w << repeat('\t', indent + 1);
						if (s)
							w << "static ";
						DumpType(type, name, indent + 1);
						if (!s && c)
							w << " const";
						w << ";\n";
					}
					func.clear();
				}
			}

			w << repeat('\t', indent) << "}";
			break;
		}

		case SymTagEnum: {
			com_ptr<IDiaEnumSymbols> i;
			long			count	= 0;
			if (type->findChildren(SymTagData, NULL, nsNone, &i) == S_OK && SUCCEEDED(i->get_Count(&count))) {
				w << "enum " << type_name << " {\n";
				com_ptr<IDiaSymbol>	v;
				while (SUCCEEDED(i->Next(1, &v, &n)) && n == 1) {
					com_string	name;
					com_variant val;
					v->get_name(&name);
					v->get_value(&val);
					w << repeat('\t', indent + 1) << name << "\t= " << val.get_number(0) << ",\n";
					v.clear();
				}
				w << repeat('\t', indent) << "}";
			}
			break;
		}

		case SymTagFunctionType: {
			com_ptr<IDiaSymbol>			ret;
			type->get_type(&ret);
			DumpType(ret, name.find(~(char_set::alphanum+'_')) ? string("(") + name + ")" : name, indent);
			w << '(';

			com_ptr<IDiaEnumSymbols>	args;
			long	num_args	= 0;
			bool	first		= true;
			if (SUCCEEDED(type->findChildren(SymTagFunctionArgType, NULL, nsNone, &args)) && args && SUCCEEDED(args->get_Count(&num_args))) {
				// params
				com_ptr<IDiaSymbol>	arg;
				while (SUCCEEDED(args->Next(1, &arg, &n)) && n == 1) {
					com_ptr<IDiaSymbol>	type;
					if (arg->get_type(&type) == S_OK) {
						com_string	name;
						arg->get_name(&name);
						if (!first)
							w << ", ";
						DumpType(type, name, -1);
						first = false;
					}
					arg.clear();
				}
				w << ')';
			}
			return;
		}
	}

	if (name.length()) {
		size_t	len = name.length();
		w << (indent < 0 ? ' ' : '\t') << name;
	}
}

string_accum operator<<(string_accum &a, com_variant value) {
	return a << "value";
}

void DiaSession::Dump(IDiaSymbol *sym, int indent) {
	com_string	name;
	auto		tag	= get_enum(SymTagNull);

	sym->get_name(&name);
	sym->get_symTag(&tag);

	switch (tag) {
		case SymTagTypedef: {
			com_ptr<IDiaSymbol>	type;
			if (sym->get_type(&type) == S_OK) {
				w << repeat('\t', indent) << "typedef ";
				DumpType(type, name, indent, true);
				w << ";\n";
			}
			break;
		}
		case SymTagUDT:
		case SymTagEnum:
			w << repeat('\t', indent);
			DumpType(sym, "", indent, true);
			w << ";\n";
			break;

		case SymTagPublicSymbol: {
			com_ptr<IDiaSymbol>	type;
			if (sym->get_type(&type) == S_OK) {
				DumpType(type, name, indent, false);
				w << ";\n";

				auto	loc		= get_enum(LocIsNull);
				auto	data	= get_enum(DataIsUnknown);

				sym->get_locationType(&loc);
				sym->get_dataKind(&data);

				if (loc == LocIsConstant && data == DataIsConstant) {
					com_variant value;
					if (sym->get_value(&value) == S_OK)
						lvalue(w << " = ") << value;
				}
				w << ';\n';
			}
			break;
		}
	}
}

void Depends::AddTypes(IDiaEnumSymbols *symbols, bool ref) {
	long	num	= 0;
	DWORD	num2;
	if (symbols && SUCCEEDED(symbols->get_Count(&num)) && num) {
		auto	elements	= new_auto(com_ptr<IDiaSymbol>, num);
		if (SUCCEEDED(symbols->Next(num, &elements[0], &num2)) && num2 == num) {
			for (auto i = elements.begin(), e = elements.end(); i != e; ++i)
				Add(*i, false);
		}
	}
}

void Depends::AddSymbols(IDiaEnumSymbols *symbols, bool ref) {
	long	num	= 0;
	DWORD	num2;
	if (symbols && SUCCEEDED(symbols->get_Count(&num)) && num) {
		auto	elements	= new_auto(com_ptr<IDiaSymbol>, num);
		if (SUCCEEDED(symbols->Next(num, &elements[0], &num2)) && num2 == num) {
			for (auto i = elements.begin(), e = elements.end(); i != e; ++i) {
				com_ptr<IDiaSymbol>	type;
				if ((*i)->get_type(&type) == S_OK)
					Add(type, ref);
			}
		}
	}
}

void Depends::Add(IDiaSymbol *type, bool ref) {
	auto	tag	= get_enum(SymTagNull);
	type->get_symTag(&tag);

	switch (tag) {
		case SymTagPointerType: {
			com_ptr<IDiaSymbol>	sub;
			type->get_type(&sub);
			Add(sub, true);
			break;
		}

		case SymTagArrayType: {
			com_ptr<IDiaSymbol> sub;
			type->get_type(&sub);
			Add(sub, ref);
			break;
		}

		case SymTagUDT: {
			com_string	type_name;
			if (type->get_name(&type_name) != S_OK || type_name.length() == 0 || type_name == "<unnamed-tag>") {
				// bases
				// elements
				com_ptr<IDiaEnumSymbols>	data;
				if (type->findChildren(SymTagData, NULL, nsNone, &data) == S_OK)
					AddSymbols(data, ref);

			} else {
				com_string	my_name;
				if (ref && sym->get_name(&my_name) == S_OK && my_name == type_name)
					break;
				dependencies[ref].push_back(type);
			}
			break;
		}

		case SymTagEnum:
			dependencies[ref].push_back(type);
			break;

		case SymTagFunctionType: {
			com_ptr<IDiaSymbol>			ret;
			type->get_type(&ret);
			Add(ret, false);

			com_ptr<IDiaEnumSymbols>	args;
			if (type->findChildren(SymTagFunctionArgType, NULL, nsNone, &args) == S_OK)
				AddSymbols(args, ref);
			break;
		}
	}
}

void DiaSession::GetDepends(IDiaSymbol *sym) {
	com_string	name;
	sym->get_name(&name);

	auto	tag	= get_enum(SymTagNull);
	sym->get_symTag(&tag);

	switch (tag) {
		case SymTagCompiland: {
			com_ptr<IDiaEnumSymbols>	children;
			if (sym->findChildren(SymTagNull, NULL, nsNone, &children) == S_OK)
				GetDepends(children);
			break;
		}
		case SymTagTypedef: {
			com_ptr<IDiaSymbol>	type;
			Depends &d	= depends_hash[sym];
			d.sym = sym;
			if (sym->get_type(&type) == S_OK)
				d.Add(type, false);
			break;
		}
		case SymTagUDT: {
			Depends &d	= depends_hash[sym];
			d.sym = sym;

			// bases
			com_ptr<IDiaEnumSymbols>	bases;
			if (sym->findChildren(SymTagBaseClass, NULL, nsNone, &bases) == S_OK)
				d.AddTypes(bases, false);

			// elements
			com_ptr<IDiaEnumSymbols>	data;
			if (sym->findChildren(SymTagData, NULL, nsNone, &data) == S_OK)
				d.AddSymbols(data, false);

			// functions
			com_ptr<IDiaEnumSymbols>	funcs;
			if (sym->findChildren(SymTagFunction, NULL, nsNone, &funcs) == S_OK)
				d.AddSymbols(funcs, false);
			break;
		}
		case SymTagEnum: {
			Depends &d	= depends_hash[sym];
			d.sym = sym;
			break;
		}
	}
}

void DiaSession::GetDepends(IDiaEnumSymbols *symbols) {
	com_ptr<IDiaSymbol>		sym;
	ULONG					n = 0;
	while (SUCCEEDED(symbols->Next(1, &sym, &n)) && n == 1) {
		GetDepends(sym);
		sym.clear();
	}
}

bool OrderedDump(DiaSession &sess, Depends &d) {
	com_string	name;
	d.sym->get_name(&name);

	if (d.state == Depends::DUMPED)
		return true;

	if (d.state == Depends::NONE)
		d.state = Depends::PROCESSING;
	else if (d.state == Depends::FORWARD)
		d.state = Depends::PROCESSING_FORWARD;
	else
		return false;

	for (auto i = d.dependencies[0].begin(), e = d.dependencies[0].end(); i != e; ++i) {
		if (auto *p = sess.depends_hash.check(i->get())) {
			if (!OrderedDump(sess, *p))
				return false;
		}
	}

	for (auto i = d.dependencies[1].begin(), e = d.dependencies[1].end(); i != e; ++i) {
		IDiaSymbol	*sym = *i;
		if (auto *p = sess.depends_hash.check(sym)) {
			if (!OrderedDump(sess, *p)) {
				if (p->state == Depends::PROCESSING) {
					auto		tag		= get_enum(SymTagNull);
					p->sym->get_symTag(&tag);
					switch (tag) {
						case SymTagUDT: {
							enum_dword<UdtKind>	kind;
							com_string			name2;
							p->sym->get_udtKind(&kind);
							p->sym->get_name(&name2);
							sess.w << struct_kinds[kind] << ' ' << name2 << ";\n";
							p->state = Depends::FORWARD;
						}
						default:
							break;
					}
				}
				if (p->state != Depends::FORWARD)
					return false;
			}
		}
	}

	sess.Dump(d.sym, 0);
	d.state = Depends::DUMPED;
	return true;
}

void DumpPDB(IDiaDataSource *source, filename fn) {
	FileOutput	out(fn);
	DiaSession	sess(source, out);

	com_ptr<IDiaSymbol>		global;
	if (SUCCEEDED(sess->get_globalScope(&global))) {
		com_ptr<IDiaEnumSymbols>	symbols;
		if (SUCCEEDED(global->findChildren(SymTagNull, NULL, nsfCaseInsensitive | nsfUndecoratedName, &symbols))) {
			sess.GetDepends(symbols);
			for (auto i = sess.depends_hash.begin(), e = sess.depends_hash.end(); i != e; ++i) {
				OrderedDump(sess, *i);
			}
		}

		symbols.clear();
		if (SUCCEEDED(global->findChildren(SymTagData, NULL, nsfCaseInsensitive | nsfUndecoratedName, &symbols))) {
			com_ptr<IDiaSymbol>		sym;
			ULONG					n = 0;
			while (SUCCEEDED(symbols->Next(1, &sym, &n)) && n == 1) {
				com_string			name;
				com_ptr<IDiaSymbol>	type;
				sym->get_name(&name);
				sym->get_type(&type);
				sess.DumpType(type, name, 0, false);
				sess.w << ";\n";
				sym.clear();
			}
		}
#if 0
		com_ptr<IDiaEnumSymbols>	funcs;
		if (SUCCEEDED(global->findChildren(SymTagFunction, NULL, nsfCaseInsensitive | nsfUndecoratedName, &funcs))) {
			com_ptr<IDiaSymbol>		func;
			ULONG					n = 0;
			while (SUCCEEDED(funcs->Next(1, &func, &n)) && n == 1) {
				sess.DumpFunction(func);
				func.clear();
			}
		}
#endif
		com_ptr<IDiaEnumSymbolsByAddr>	byaddr;
		if (SUCCEEDED(sess->getSymbolsByAddr(&byaddr))) {
			com_ptr<IDiaSymbol>		sym;
			if (SUCCEEDED(byaddr->symbolByAddr(1, 0, &sym))) {
				ULONG					n = 0;
				while ((sym.clear(), SUCCEEDED(byaddr->Next(1, &sym, &n))) && n == 1) {
					sess.Dump(sym, 0);
				}
			}
		}

		ISO_TRACEF("CRC Clashes: ") << sess.clashes << " out of " << sess.name_hash.size() << '\n';
	}
}

//-----------------------------------------------------------------------------
//	PDB
//-----------------------------------------------------------------------------

class PDBDIAFileHandler : public FileHandler {
	ISO_ptr<void>	ReadSource(tag id, IDiaDataSource *source);

	const char*		GetExt() override { return "pdb"; }
	const char*		GetDescription() override { return "Visual Studio Debug using DIA";	}

	bool	MakeSource(com_ptr<IDiaDataSource> &source) {
		return	source.create<DiaSource>()
			||	SUCCEEDED(CoCreateInstanceFromDLL(
				(void**)&source,
				win::RegKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\14.0\\").value("ShellFolder").get<filename>().add_dir("DIA SDK\\bin\\amd64\\msdia140.dll"),
				__uuidof(DiaSource), __uuidof(IDiaDataSource)
			));
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		com_ptr<IDiaDataSource>	source;
		if (!MakeSource(source) || FAILED(source->loadDataFromIStream(ISO_IStreamI(file))))
			return ISO_NULL;
		return ReadSource(id, source);
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		com_ptr<IDiaDataSource>	source;
		if (!MakeSource(source) || FAILED(source->loadDataFromPdb(str16(fn))))
			return ISO_NULL;
		DumpPDB(source, filename(fn).set_ext("h"));
		return ReadSource(id, source);
	}
} pdb_dia;

ISO_ptr<void> PDBDIAFileHandler::ReadSource(tag id, IDiaDataSource *source) {
	ISODiaSession			sess(source);
	ISO_ptr<anything>		p(id);

	if (com_ptr<IDiaEnumSegments> segments = sess.GetTable<IDiaEnumSegments>())
		p->Append(sess.Enumerate<IDiaSegment>("Segments", segments));

	com_ptr<IDiaEnumSymbolsByAddr>	byaddr;
	com_ptr<IDiaSymbol>		sym;
	if (SUCCEEDED(sess->getSymbolsByAddr(&byaddr)) && SUCCEEDED(byaddr->symbolByAddr(1, 0, &sym)))
		p->Append(sess.Enumerate<IDiaSymbol>("symbols", byaddr));

	com_ptr<IDiaSymbol>		global;
	if (SUCCEEDED(sess->get_globalScope(&global))) {
		GUID	guid;
		global->get_guid(&guid);
		p->Append(MakePtr("guid", guid));

		com_ptr<IDiaEnumSymbols>	symbols;
		if (SUCCEEDED(global->findChildren(SymTagCompiland, NULL, nsfCaseInsensitive | nsfUndecoratedName, &symbols)))
			p->Append(sess.Enumerate<IDiaSymbol>("compilands", symbols));
		symbols.clear();

		if (SUCCEEDED(global->findChildren(SymTagData, NULL, nsfCaseInsensitive | nsfUndecoratedName, &symbols)))
			p->Append(sess.Enumerate<IDiaSymbol>("globals", symbols));
		symbols.clear();

		if (SUCCEEDED(global->findChildren(SymTagFunction, NULL, nsfCaseInsensitive | nsfUndecoratedName, &symbols)))
			p->Append(sess.Enumerate<IDiaSymbol>("functions", symbols));
		symbols.clear();
	}
	DWORD					n;
	com_ptr<IDiaEnumTables> tables;
	com_ptr<IDiaTable>		table;
	sess->getEnumTables(&tables);

	while (SUCCEEDED(tables->Next(1, &table, &n)) && n == 1) {
		com_string	name;
		table->get_name(&name);

		if (com_ptr<IDiaEnumSourceFiles>	source_files = table.query()) {
			p->Append(sess.Enumerate<IDiaSourceFile>(name, source_files));

//		} else if (com_ptr<IDiaEnumSegments> segments = table.query()) {
//			p->Append(sess.Enumerate<IDiaSegment>(name, segments));

//		} else if (com_ptr<IDiaEnumSectionContribs> sec_contribs = table.query()) {
//			p->Append(sess.Enumerate<IDiaSectionContrib>(name, sec_contribs));

		} else if (com_ptr<IDiaEnumFrameData> frames = table.query()) {
			p->Append(sess.Enumerate<IDiaFrameData>(name, frames));

		} else if (com_ptr<IDiaEnumStackFrames> frames = table.query()) {
			p->Append(sess.Enumerate<IDiaStackFrame>(name, frames));

		} else if (com_ptr<IDiaEnumInjectedSources> injected = table.query()) {
			p->Append(sess.Enumerate<IDiaInjectedSource>(name, injected));

//		} else if (com_ptr<IDiaEnumLineNumbers> lines = table.query()) {
//			p->Append(sess.Enumerate<IDiaLineNumber>(name, lines));

		}

		table.clear();
	}
	return p;
}
