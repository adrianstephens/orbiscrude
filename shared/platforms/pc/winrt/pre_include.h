//#ifndef WINRT_PREINCLUDE_H
//#define WINRT_PREINCLUDE_H
#pragma once

#include "combaseapi.h"
#include "base/array.h"
#include "base/strings.h"
#include "hashes/_sha.h"

#define COM_NO_WINDOWS_H
#include <winstring.h>
#include <inspectable.h>
#include <activation.h>
#include <weakreference.h>
#include <RestrictedErrorInfo.h>
//#include <Roerrorapi.h>
#include <ctxtcall.h>

#define GUID_CONSTEXPR

#ifdef GUID_CONSTEXPR

#define define_guid(a,b,c,d,e,f,g,h,i,j,k)	static const constexpr GUID value {a, b, c, {d, e, f, g, h, i, j, k}}
#define define_guid_s(s)					static const constexpr GUID value = string_to_guid(s)
#define generate_guid(g)					static constexpr GUID value = g;
#define get_guid(G)							G::value

#else

#define define_guid(a,b,c,d,e,f,g,h,i,j,k)	static const GUID value() { return GUID {a, b, c, {d, e, f, g, h, i, j, k}}; }
#define define_guid_s(s)					static const GUID value() { return string_to_guid(s); }
#define generate_guid(g)					static const GUID value() { static GUID guid = g; return guid; }
#define get_guid(G)							G::value()

#endif

#undef GetCurrentTime
#define internal	public

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

enum RO_INIT_TYPE {
	RO_INIT_SINGLETHREADED	= 0,	// Single-threaded application
	RO_INIT_MULTITHREADED	= 1,	// COM calls objects on any thread.
};

typedef HRESULT(STDAPICALLTYPE * PFNGETACTIVATIONFACTORY)(HSTRING, IActivationFactory **);
struct IApartmentShutdown;
DECLARE_HANDLE(APARTMENT_SHUTDOWN_REGISTRATION_COOKIE);

extern "C" {
	DECLSPEC_IMPORT HRESULT	WINAPI	RoInitialize(RO_INIT_TYPE initType);
	DECLSPEC_IMPORT void	WINAPI	RoUninitialize();
	DECLSPEC_IMPORT HRESULT	WINAPI	RoActivateInstance(HSTRING activatableClassId, IInspectable **instance);
	DECLSPEC_IMPORT HRESULT	WINAPI	RoRegisterActivationFactories(HSTRING *activatableClassIds, PFNGETACTIVATIONFACTORY *activationFactoryCallbacks, UINT32 count, struct RO_REGISTRATION_COOKIE **cookie);
	DECLSPEC_IMPORT void	WINAPI	RoRevokeActivationFactories(struct RO_REGISTRATION_COOKIE *cookie);
	DECLSPEC_IMPORT HRESULT	WINAPI	RoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void ** factory);
	DECLSPEC_IMPORT HRESULT	WINAPI	RoRegisterForApartmentShutdown(IApartmentShutdown *callbackObject, UINT64 *apartmentIdentifier, APARTMENT_SHUTDOWN_REGISTRATION_COOKIE *regCookie);
	DECLSPEC_IMPORT HRESULT	WINAPI	RoUnregisterForApartmentShutdown(APARTMENT_SHUTDOWN_REGISTRATION_COOKIE regCookie);
	DECLSPEC_IMPORT HRESULT	WINAPI	RoGetApartmentIdentifier(UINT64 *apartmentIdentifier);

	STDAPI_(BOOL)					RoOriginateError(HRESULT error, HSTRING message);
	STDAPI							GetRestrictedErrorInfo(IRestrictedErrorInfo **ppRestrictedErrorInfo);
}

namespace iso_winrt {
using namespace iso;

//-----------------------------------------------------------------------------
//	generate GUID
//-----------------------------------------------------------------------------

template<size_t N> constexpr GUID bytes_to_guid(const meta::array<uint8, N> &a) {
	return {
		bytes_to_u4(a[3], a[2], a[1], a[0]),
		bytes_to_u2(a[5], a[4]),
		bytes_to_u2(a[7], a[6]),
		{a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]}
	};
}

template<size_t N> constexpr GUID string_to_guid(const meta::array<char, N> &a) {
	return GUID{
		from_hex_string<uint32>(a, 0),
		from_hex_string<uint16>(a, 9),
		from_hex_string<uint16>(a, 14),
		{
			from_hex_string<uint8>(a, 19),
			from_hex_string<uint8>(a, 21),
			from_hex_string<uint8>(a, 24),
			from_hex_string<uint8>(a, 26),
			from_hex_string<uint8>(a, 28),
			from_hex_string<uint8>(a, 30),
			from_hex_string<uint8>(a, 32),
			from_hex_string<uint8>(a, 34)
		}
	};
}
template<size_t N> constexpr GUID string_to_guid(const char (&a)[N]) {
	return string_to_guid(meta::make_array(a));
}

constexpr GUID operator"" _guid(const char *p, size_t len) {
	return string_to_guid(*(meta::array<char,36>*)(p));
}

constexpr meta::array<char, 38> guid_to_string(const GUID &id) {
	return to_constexpr_string("{")
		+ to_hex_string(id.Data1)
		+ "-" + to_hex_string(id.Data2)
		+ "-" + to_hex_string(id.Data3)
		+ "-" + to_hex_string(id.Data4[0]) + to_hex_string(id.Data4[1])
		+ "-" + to_hex_string(id.Data4[2]) + to_hex_string(id.Data4[3])
		+ to_hex_string(id.Data4[4]) + to_hex_string(id.Data4[5])
		+ to_hex_string(id.Data4[6]) + to_hex_string(id.Data4[7])
		+ "}";
}

template<size_t N> constexpr meta::array<uint8, N + 16> create_guid_gen_buffer(const GUID &guid, const meta::array<char, N> &str) {
	return to_byte_array(guid) + meta::make_array<uint8>(str);
}
inline malloc_block create_guid_gen_buffer(const GUID &guid, const char *str) {
	malloc_block	buffer(sizeof(GUID) + strlen(str));
	*buffer = to_byte_array(guid);
	memcpy(buffer + sizeof(GUID), str, strlen(str));
	return buffer;
}

constexpr GUID set_named_guid_fields(const GUID &id) {
	return {
		id.Data1, id.Data2, uint16((id.Data3 & 0x0fff) | (5 << 12)),
		{uint8((id.Data4[0] & 0x3f) | 0x80), id.Data4[1], id.Data4[2], id.Data4[3], id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]}
	};
}

template<typename S> constexpr GUID generate_interface_guid(const S &sig) {
	constexpr GUID namespace_guid = {0x11f47ad5, 0x7b73, 0x42c0, {0xab, 0xae, 0x87, 0x8b, 0x1e, 0x16, 0xad, 0xee}};
	return set_named_guid_fields(bytes_to_guid(SHA1_const::calculate(create_guid_gen_buffer(namespace_guid, sig))));
}

//-----------------------------------------------------------------------------
//	generate name
//-----------------------------------------------------------------------------

template<typename U, typename V = meta::value_list<char> >	struct fix_constexpr_name;

template<char... vv>		struct fix_constexpr_name2													: meta::value_list<char, vv...> {};
template<char... vv>		struct fix_constexpr_name2<'c','l','a','s','s',' ',					vv...>	: fix_constexpr_name2<vv...> {};
template<char... vv>		struct fix_constexpr_name2<'s','t','r','u','c','t', ' ',			vv...>	: fix_constexpr_name2<vv...> {};
template<char... vv>		struct fix_constexpr_name2<'i','s','o','_','w','i','n','r','t','.',	vv...>	: meta::value_list<char, vv...> {};

template<char u, char... uu, char... vv>	struct fix_constexpr_name<meta::value_list<char, u, uu...>,			meta::value_list<char, vv...> >	: fix_constexpr_name<meta::value_list<char, uu...>, meta::value_list<char, vv..., u> >		{};
template<char... uu, char... vv>			struct fix_constexpr_name<meta::value_list<char, ':', ':', uu...>,	meta::value_list<char, vv...> >	: fix_constexpr_name<meta::value_list<char, uu...>, meta::value_list<char, vv..., '.'> >	{};
template<char u0, char u1, char... vv>		struct fix_constexpr_name<meta::value_list<char, u0, u1>,			meta::value_list<char, vv...> >	: fix_constexpr_name2<vv..., u0, u1>		{};

//auto __cdecl iso_winrt::get_constexpr_name_s<class <namespace>::<id>>::f(void)
template<typename T> struct get_constexpr_name_s {
//	constexpr static auto f() { return meta::trim_back<11>(meta::trim_front<45>(meta::make_array(__FUNCSIG__))); };
	constexpr static auto array{meta::name<T>()};
};
template<class T> constexpr auto get_constexpr_name() {
	return meta::make_array(
		fix_constexpr_name<meta::explode<char, get_constexpr_name_s<T>>>()
	);
}

//-----------------------------------------------------------------------------
//	meta
//-----------------------------------------------------------------------------

struct yesno {
	typedef char yes;
	struct no { yes c[2]; };
};

template<typename A, typename B> struct T_inherits : yesno {
	static no	test(...);
	static yes	test(B *);
	enum { value = sizeof(test((A*)0)) == sizeof(yes) };
};

template<bool b, typename, typename F>	struct T_if					{ typedef F type; };
template<typename T, typename F>		struct T_if<true, T, F>		{ typedef T type; };

template<typename... TT>				struct inherit : TT... {};
template<typename T>					struct dummy_wrapper {};

//list of types
template<typename...T>					struct types {};

//concatenate types A... and B...
template<typename...T>					struct types_cat;
template<typename A, typename...B>		struct types_cat<A, B...>			: types_cat<types<A>, typename types_cat<B...>::type> {};
template<typename...A, typename...B>	struct types_cat<types<A...>, B...> : types_cat<types<A...>, typename types_cat<B...>::type> {};
template<typename...A, typename...B>	struct types_cat<types<A...>, types<B...> > { typedef types<A..., B...> type; };
template<typename...A>					struct types_cat<types<A...> >				{ typedef types<A...> type; };
template<typename A>					struct types_cat<A>							{ typedef types<A> type; };
template<>								struct types_cat<>							{ typedef types<> type; };

//add type A to types B if not already present
template<typename T, typename B>		struct types_add_unique;
template<typename T, typename...B>		struct types_add_unique<T, types<B...> > : T_if<T_inherits<inherit<dummy_wrapper<B>...>, dummy_wrapper<T>>::value, types<B...>, types<T, B...>> {};

//strip duplicate types in types
template<typename A, typename...B>		struct types_unique					: types_add_unique<A, typename types_unique<B...>::type> {};
template<typename... T>					struct types_unique<types<T...>>	: types_unique<T...>	{};
template<typename T>					struct types_unique<T>			{ typedef types<T> type; };
template<>								struct types_unique<types<>>	{ typedef types<> type; };

//get union of types A and types B
template<typename A, typename B>		struct types_union;
template<typename...A, typename...B>	struct types_union<types<A...>, types<B...>> : types_unique<A..., B...> {};

//remove type T from types A (if present)
template<typename T, typename A, typename B = types<> >			struct types_remove;
template<typename T, typename A0, typename...A, typename...B>	struct types_remove<T, types<A0, A...>, types<B...> >	: types_remove<T, types<A...>, types<B..., A0> > {};
template<typename T, typename...A, typename...B>				struct types_remove<T, types<T, A...>, types<B...> >	: types_remove<T, types<A...>, types<B...> > {};
template<typename T, typename...B>								struct types_remove<T, types<>, types<B...> >	{ typedef types<B...> type; };

//get diff of types A and types B
template<typename A, typename B>								struct types_diff;
template<typename A>											struct types_diff<A, types<>>			{ typedef A type; };
//template<typename A, typename B0>								struct types_diff<A, types<B0>>			: types_remove<B0, A> {};
template<typename A, typename B0, typename...B>					struct types_diff<A, types<B0, B...>>	: types_remove<B0, typename types_diff<A, types<B...>>::type> {};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

namespace Windows { namespace Foundation {
	struct DateTime;
	struct TimeSpan;
	struct Point;
	struct Size;
	struct Rect;

	typedef GUID			Guid;
	typedef IInspectable	Object;
	typedef IInspectable	Attribute;
	typedef HSTRING__		String;
	template<typename T> struct Array;

	enum class TypeCode : int {
		Empty = 0,
		Object = 1,
		Boolean = 3,
		Char16 = 4,
		Int8 = 5,
		UInt8 = 6,
		Int16 = 7,
		UInt16 = 8,
		Int32 = 9,
		UInt32 = 10,
		Int64 = 11,
		UInt64 = 12,
		Single = 13,
		Double = 14,
		DateTime = 16,
		String = 18,
		TimeSpan = 19,
		Point = 20,
		Size = 21,
		Rect = 22,
		Guid = 23,
		Custom = 24,
	};
	namespace Collections {}
} }

namespace Platform = Windows::Foundation;

template<typename T> class ptr;
template<typename T> class pptr;

typedef ptr<Platform::String>	hstring;
typedef pptr<Platform::String>	hstring_ref;
typedef ptr<Platform::Object>	object;
typedef pptr<Platform::Object>	object_ref;

//-----------------------------------------------------------------------------
//	szarray
//-----------------------------------------------------------------------------

template<typename T> struct szarray {
	T			*p;
	unsigned	size;
	szarray()	{}
	constexpr szarray(T *p, size_t size)			: p(p), size(size)	{}
	template<size_t N> constexpr szarray(T (&a)[N])	: p(a), size(N)		{}

	T*	detach(unsigned *_size) const {
		*_size	= size;
		return p;
	}
	T*	begin()					const { return p; }
	T*	end()					const { return p + size; }
	T&	operator[](unsigned i)	const { return p[i]; }
	friend const T* begin(const szarray &a)	{ return a.p; }
	friend const T* end(const szarray &a)	{ return a.p + a.size; }
};

template<typename T> class pptr<Platform::Array<T>> : szarray<T> {
public:
	constexpr pptr()								: szarray<T>(0, 0)	{}
	constexpr pptr(T *a, size_t n)					: szarray<T>(a, n)	{}
	template<size_t N> constexpr pptr(T (&a)[N])	: szarray<T>(a, N)	{}
};

template<typename T> class ptr<Platform::Array<T>> : szarray<T> {
public:
	T *alloc(unsigned size) {
		if (this->size	= size)
			return this->p = static_cast<T*>(CoTaskMemAlloc(size * sizeof(T)));
		return 0;
	}
	void alloc_copy(const T *a, unsigned size) {
		for (T *b = alloc(size); size--;)
			new(b++) T(*a++);
	}
	ptr()										: szarray<T>(0, 0) {}
	ptr(ptr &&b)								: szarray<T>(b.p, exchange(b.size, 0)) {}
	~ptr()										{ if (size) CoTaskMemFree(p); }
	ptr(size_t n)								{ alloc(n); }
	ptr(const T *a, unsigned n)					{ alloc_copy(a, n); }
	template<unsigned N> ptr(const T (&a)[N])	{ alloc_copy(a, N); }

	T	*detach(unsigned *_size) {
		*_size	= size;
		size	= 0;
		return p;
	}
};

//-----------------------------------------------------------------------------
//	def
//-----------------------------------------------------------------------------

struct								basic_type {};
template<typename U>		struct	enum_type {};
template<typename... TT>	struct	value_type {};
struct								delegate_type {};

template<typename... I>		struct	interface_type {
	typedef interface_type		type;
};
template<typename... I>		struct	overridable_type : interface_type<I...> {
	typedef overridable_type		type;
};
template<typename E, typename I0, typename... I> struct class_type {
	typedef I0				default_interface;
	typedef class_type		type;
};

template<typename C> struct composer_type {
	typedef	C	composer;
};

struct custom_activators {};

template<typename T, typename V=void>	struct def;
template<typename T, typename V=void>	struct def_runtime : def<T> {};

template<typename T> struct def<T*>		: def_runtime<T> {};
template<typename T> struct def<ptr<T>>	: def_runtime<T> {};

template<typename T> struct name { static constexpr auto value = meta::make_array<wchar_t>(get_constexpr_name<T>()); };// + wchar_t(0); };
template<typename T> constexpr auto name_of() { return name<T>::value; };

//-----------------------------------------------------------------------------
//	uuid
//-----------------------------------------------------------------------------

template<typename T> struct uuid_gen;
template<typename T, typename V=void> struct uuid;//	{ generate_guid(generate_interface_guid(sigof<T>())); };
template<typename T> struct uuid<T, void_t<typename def<T>::default_interface>> : uuid<typename def<T>::default_interface> {};
template<typename T> constexpr auto uuidof()	{ return get_guid(uuid<T>); }
template<typename T> constexpr auto uuidof(T*)	{ return get_guid(uuid<T>); }

//#ifdef PLAT_CLANG
template<> struct uuid<IInspectable>			{ define_guid(0xAF86E2E0,0xB12D,0x4c6a,0x9C,0x5A,0xD7,0xAA,0x65,0x10,0x1E,0x90); };
template<> struct uuid<IActivationFactory>		{ define_guid(0x00000035,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46); };
template<> struct uuid<IAgileObject>			{ define_guid(0x94ea2b94,0xe9cc,0x49e0,0xc0,0xff,0xee,0x64,0xca,0x8f,0x5b,0x90); };
template<> struct uuid<IWeakReferenceSource>	{ define_guid(0x00000038,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46); };
template<> struct uuid<IWeakReference>			{ define_guid(0x00000037,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46); };

//#else
//template<typename T>	struct uuid<T, typename T_void<decltype(__uuidof(T))>::type> { generate_guid(__uuidof(T)); };
//#endif

//-----------------------------------------------------------------------------
//	signatures
//-----------------------------------------------------------------------------

template<typename T> struct signature;

#ifdef GUID_CONSTEXPR
#define SIG_CONSTEXPR	constexpr
#define define_sig(s)	constexpr static auto sig{s};
#define const_sig(s)	constexpr static auto &sig{s};
#define get_sig(S)		S::sig
#else
#define SIG_CONSTEXPR
#define define_sig(s)	static const auto& sig() { static auto sig = s; return sig; }
#define const_sig(s)	static const auto& sig() { static auto &sig{s}; return sig; }//meta::make_array(sig); }
#define get_sig(S)		S::sig()
#endif

template<typename T>	constexpr auto &sigof()	{ return get_sig(signature<T>); }

template<typename T, typename... TT> struct signature<types<T, TT...> > {
	define_sig(to_constexpr_string(sigof<T>()) + ";" + sigof<types<TT...>>())
};
template<typename T> struct signature<types<T>> : signature<T> {};

template<typename T> struct delegate_signature {
	define_sig(to_constexpr_string("delegate(") + guid_to_string(uuidof<T>()) + ")");
};
template<template<typename...> class T, typename...P> struct delegate_signature<T<P...> > {
	define_sig(to_constexpr_string("pinterface(") + guid_to_string(get_guid(uuid_gen<T<P...>>)) + ";" + sigof<types<P...>>() + ")");
};

template<typename T> struct interface_signature {
	define_sig(guid_to_string(uuidof<T>()));
};
template<template<typename...> class T, typename...P> struct interface_signature<T<P...> > {
	define_sig(to_constexpr_string("pinterface(") + guid_to_string(get_guid(uuid_gen<T<P...>>)) + ";" + sigof<types<P...>>() + ")");
};

template<typename T> struct signature {
	SIG_CONSTEXPR static auto get(void*) {
		return get_constexpr_name<T>();
	};
	SIG_CONSTEXPR static auto get(basic_type*) {
		return meta::make_array(def_runtime<T>::sig);
	};
	template<typename U> SIG_CONSTEXPR static auto get(enum_type<U>*) {
		return to_constexpr_string("enum(") + name<T>::value + ";" + sigof<U>() + ")";
	};
	template<typename... TT> SIG_CONSTEXPR static auto get(value_type<TT...> *) {
		return to_constexpr_string("struct(") + meta::make_array<char>(name<T>::value) + ";" + sigof<types<TT...>>() + ")";
	}
	template<typename E, typename I0, typename... I> SIG_CONSTEXPR static auto get(class_type<E, I0, I...> *) {
		return to_constexpr_string("rc(") + meta::make_array<char>(name<T>::value) + ";" + guid_to_string(uuidof<I0>()) + ")";
	}
	template<typename... I>	SIG_CONSTEXPR static auto get(interface_type<I...> *) {
		return get_sig(interface_signature<T>);
	}
	SIG_CONSTEXPR static auto get(delegate_type*) {
		return get_sig(delegate_signature<T>);
	};
	define_sig(get((def<T>*)0))
};

template<> struct signature<bool>		{ const_sig("b1"); };
template<> struct signature<int16>		{ const_sig("i2"); };
template<> struct signature<int32>		{ const_sig("i4"); };
template<> struct signature<int64>		{ const_sig("i8"); };
template<> struct signature<uint8>		{ const_sig("u1"); };
template<> struct signature<uint16>		{ const_sig("u2"); };
template<> struct signature<uint32>		{ const_sig("u4"); };
template<> struct signature<uint64>		{ const_sig("u8"); };
template<> struct signature<float>		{ const_sig("f4"); };
template<> struct signature<double>		{ const_sig("f8"); };
template<> struct signature<char16_t>	{ const_sig("c2"); };
template<> struct signature<GUID>		{ const_sig("g16"); };
template<> struct signature<HSTRING>	{ const_sig("string");};
template<> struct signature<IInspectable> { const_sig("cinterface(IInspectable)"); };
template<> struct signature<hstring>	: signature<HSTRING> {};

template<> struct name<bool>			{	static constexpr auto &value{L"Boolean"};	};
template<> struct name<int16>			{	static constexpr auto &value{L"Int16"};		};
template<> struct name<int32>			{	static constexpr auto &value{L"Int32"};		};
template<> struct name<int64>			{	static constexpr auto &value{L"Int64"};		};
template<> struct name<uint8>			{	static constexpr auto &value{L"UInt8"};		};
template<> struct name<uint16>			{	static constexpr auto &value{L"UInt16"};	};
template<> struct name<uint32>			{	static constexpr auto &value{L"UInt32"};	};
template<> struct name<uint64>			{	static constexpr auto &value{L"UInt64"};	};
template<> struct name<float>			{	static constexpr auto &value{L"Single"};	};
template<> struct name<double>			{	static constexpr auto &value{L"Double"};	};
template<> struct name<char16_t>		{	static constexpr auto &value{L"Char16"};	};
template<> struct name<GUID>			{	static constexpr auto &value{L"Guid"};		};
template<> struct name<HSTRING>			{	static constexpr auto &value{L"String"};	};
template<> struct name<IInspectable>	{	static constexpr auto &value{L"Object"};	};
template<> struct name<hstring>			: name<HSTRING> {};

template<> struct def<IWeakReferenceSource> : overridable_type<> {};

template<typename T> struct signature<T*>		: signature<T> {};
template<typename T> struct signature<ptr<T>>	: signature<T> {};

#undef define_sig
#undef const_sig
#undef get_sig

template<typename T, typename V> struct uuid	{ generate_guid(generate_interface_guid(sigof<T>())); };

//-----------------------------------------------------------------------------
//	ptr helpers
//-----------------------------------------------------------------------------

template<typename A> class T_inherits<A, IInspectable>	: public T_true {};
template<typename A> class T_inherits<A, IUnknown>		: public T_true {};

template<typename T> ptr<T> query(IUnknown *f, GUID guid) {
	T *t;
	return f && SUCCEEDED(f->QueryInterface(guid, (void**)&t)) ? ptr<T>(t, true) : nullptr;
};

template<typename F, typename T, bool Q> struct query_s;

template<typename F, typename T> struct query_s<F, T, true> {
	static T* ref(F *f)		{ return f; }
};
template<typename F, typename T> struct query_s<F, T, false> {
	static ptr<T> ref(F *f) {
		T *t;
		return f && SUCCEEDED(f->QueryInterface(uuidof<T>(), (void**)&t)) ? ptr<T>(t, true) : nullptr;
	}
};

template<typename B, typename A> inline auto query(A *a) {
	return query_s<A, B, T_inherits<A, B>::value>::ref(a);
}

template<typename B, typename A> inline auto query(pptr<A> a) {
	return query_s<A, B, T_inherits<A, B>::value>::ref((A*)a);
}

template<typename F, typename T> struct caster_s;
template<typename F, typename T> struct caster_s<ptr<F>, ptr<T>> : query_s<F, T, T_inherits<F,T>::value> {
	static auto safe(F *f) {
		auto t = ref(f);
		if (f && !t)
			throw "InvalidCastException";
		return t;
	}
};

template<typename T, typename F> inline T ref_cast(const pptr<F> &a)	{ return caster_s<ptr<F>, T>::ref(a); }
template<typename T, typename F> inline T ref_cast(const ptr<F> &a)		{ return caster_s<ptr<F>, T>::ref(a); }
template<typename T, typename F> inline T ref_cast(ptr<F> &&a)			{ return caster_s<ptr<F>, T>::ref(a.detach()); }

template<typename T, typename F> inline T safe_cast(const pptr<F> &a)	{ return caster_s<ptr<F>, T>::safe(a); }
template<typename T, typename F> inline T safe_cast(const ptr<F> &a)	{ return caster_s<ptr<F>, T>::safe(a); }
template<typename T, typename F> inline T safe_cast(ptr<F> &&a)			{ return caster_s<ptr<F>, T>::safe(a.detach()); }

struct _ptr_base {
	IInspectable	*get0()	const	{ return *(IInspectable**)this; }
};

inline bool	operator==(const _ptr_base &a, nullptr_t)	{ return !a.get0(); }
inline bool	operator!=(const _ptr_base &a, nullptr_t)	{ return !!a.get0(); }
inline bool	operator==(const _ptr_base &a, const _ptr_base &b)	{ return a.get0() == b.get0(); }
inline bool	operator!=(const _ptr_base &a, const _ptr_base &b)	{ return a.get0() != b.get0(); }

template<typename T> class ptr_base : public _ptr_base {
protected:
	T		*t;
//	void	release()	const			{ if (t) t->Release(); }
	void	release()	const			{ if (t) ((IUnknown*)(void*)t)->Release(); }
	void	add_ref()	const			{ if (t) t->AddRef(); }
//	void	add_ref()	const			{ if (t) ((IUnknown*)(void*)t)->AddRef(); }
	void	set(T *p)					{ if (p != t) { if (p) p->AddRef(); release(); } t = p; }
	void	set(ptr<T> &&p)				{ swap(t, p.t); }
	void	set(const pptr<T> &p)		{ set((T*)p); }

public:
	ptr_base(T *p = 0)				: t(p)			{ add_ref(); }
	ptr_base(ptr_base<T> &&p)		: t(p.detach())	{}
	ptr_base(T *p, bool)			: t(p)			{}
	ptr_base(ptr_base<T> &&p, bool)	: t(p.detach())	{}
	~ptr_base()							{ release(); }
    T*		operator->()	const		{ return t;	}
	operator T*()			const		{ return t; }
	T*		get()			const		{ return t; }
	T*		get_addref()	const		{ add_ref(); return t; }
	T*		detach()					{ T *p = t; t = nullptr; return p;	}
};

// for passing parameters

template<typename T> class pptr_base {
	T		*t;
	bool	release;
public:
	pptr_base(T *p, bool release)	: t(p), release(release)		{}
	pptr_base(const pptr_base &p)	: t(p.t), release(false)		{}
	~pptr_base()				{
		if (t && release)
			((IUnknown*)(void*)t)->Release();
	}

	operator T*()		const	{ return t; }
	T*	operator->()	const	{ return t; }
	T**	operator&()				{ return &t; }

	T*	get_addref()			{ if (t && !release) ((IUnknown*)(void*)t)->AddRef(); release = false; return t; }
};

//-----------------------------------------------------------------------------
//	general ptr, pptr
//-----------------------------------------------------------------------------

template<typename T> class ptr : public ptr_base<T> {
	typedef ptr_base<T>	B;
public:
	ptr(nullptr_t=nullptr)	{}
	ptr(T *p, bool)			: B(p, true)				{}	//no addref
	ptr(T *p)				: B(p)						{}
	ptr(ptr &&p)			: B(p.detach(), true)		{}
	ptr(const ptr &p)		: B(p.get())				{}
	ptr(pptr<T> &p)			: B(p.get_addref(), true)	{}
	ptr(pptr<T> &&p)		: B(p.get_addref(), true)	{}
	ptr(const _ptr_base &p)	: B(query<T>(p.get0()))		{}
	ptr(_ptr_base &&p)		: B(query<T>(p.get0()))		{}
	template<typename A> ptr(const ptr<A> &p)	: B(query<T>(p.get()))	{}

	ptr&						operator=(ptr &&p)			{ swap(this->t, p.t); return *this; }
	ptr&						operator=(const ptr &p)		{ this->set(p.t); return *this; }
	template<typename A> ptr&	operator=(const ptr<A> &p)	{ B::set(query<T>(p.get())); return *this; }
	template<typename A> ptr&	operator=(ptr<A> &&p)		{ B::set(query<T>(p.detach())); return *this; }

	template<typename A> ptr<A>	as()	const				{ return query<A>(t); }
	template<typename A> bool	is()	const				{ return !!query<A>(t); }
};

template<typename T> class pptr : public pptr_base<T> {
	typedef pptr_base<T>	B;
public:
	pptr(T *p)				: B(p, false)					{}
	pptr(const ptr<T> &p)	: B(p.get(), false)				{}
	pptr(ptr<T> &&p)		: B(p.detach(), true)			{}
	template<typename A> pptr(A *a)				: pptr(query<T>(a))		{}
	template<typename A> pptr(const ptr<A> &a)	: pptr(query<T>(a.get()))	{}
};

//-----------------------------------------------------------------------------
//	IInspectable specialisations
//-----------------------------------------------------------------------------

template<> class pptr<IInspectable> : public pptr_base<IInspectable> {
	typedef IInspectable	T;
	typedef pptr_base<T>	B;
public:
	pptr(T *p)				: B(p, false)					{}
	pptr(const ptr<T> &p);
	pptr(ptr<T> &&p);
	template<typename A>	pptr(const ptr<A> &a)		: pptr(query<T>(a.get()))	{}
	template<typename A>	explicit pptr(A *a)			: pptr(query<T>(a))		{}
	template<typename A>	explicit pptr(A value);
	template<size_t N>		pptr(const wchar_t (&value)[N]);
	pptr(hstring value);
	pptr(hstring_ref value);
};


template<> class ptr<IInspectable> : public ptr_base<IInspectable> {
	typedef IInspectable	T;
	typedef ptr_base<T>		B;
public:
	ptr(nullptr_t=nullptr)	{}
	ptr(T *p, bool)			: B(p, true)			{}	//no addref
	ptr(T *p)				: B(p)					{}
	ptr(ptr &&p)			: B(p.detach(), true)	{}
	ptr(const ptr &p)		: B(p.get())			{}
	ptr(_ptr_base &&a)		: B(a.get0())			{}
	ptr(pptr<T> &p)			: B(p.get_addref(), true)	{}
	ptr(pptr<T> &&p)		: B(p.get_addref(), true)	{}
	ptr(const pptr<T> &p)	: B(query<T>((T*)p))		{}
	template<typename A> ptr(const ptr<A> &p)	: B(query<T>(p.get()))		{}
//	template<typename A> ptr(ptr<A> &&p)		: B(query<T>(p.detach()))	{}//prob should be get because ref holder can change
	template<typename A> ptr(const pptr<A> &p)	: B(query<T>((A*)p))		{}
	template<typename A> explicit ptr(A *p)		: B(query<T>(p))			{}
	template<typename A> explicit ptr(A value);
	template<size_t N>	ptr(const wchar_t (&value)[N]);
	template<>			ptr(const wchar_t *value);
	ptr(hstring value);
	ptr(hstring_ref value);

	ptr&						operator=(ptr &&p)			{ swap(t, p.t); return *this; }
	ptr&						operator=(const ptr &p)		{ B::set(p.t); return *this; }
	template<typename A> ptr&	operator=(A *p)				{ B::set(query<T>(p)); return *this; }
	template<typename A> ptr&	operator=(const ptr<A> &p)	{ B::set(query<T>(p.get())); return *this; }
	template<typename A> ptr&	operator=(ptr<A> &&p)		{ B::set(query<T>(p.detach())); return *this; }

	template<typename A> ptr<A>	as()						{ return query<A>(t); }
	template<typename A> bool	is()						{ return !!query<A>(t); }

	friend bool	operator==(const ptr &a, nullptr_t)		{ return !a.t; }
	friend bool	operator!=(const ptr &a, nullptr_t)		{ return !!a.t; }
	friend bool	operator==(const ptr &a, const T *b)	{ return a.t == b; }
	friend bool	operator!=(const ptr &a, const T *b)	{ return a.t != b; }
};

inline pptr<IInspectable>::pptr(const ptr<T> &p)	: B(p.get(), false)			{}
inline pptr<IInspectable>::pptr(ptr<T> &&p)			: B(p.detach(), true)		{}

//-----------------------------------------------------------------------------
//	hstring specialisations
//-----------------------------------------------------------------------------

class hstring_base {
protected:
	HSTRING h;

	template<typename I> bool init(I s, I e) {
		if (s == e)
			return true;
		wchar_t			*buffer;
		HSTRING_BUFFER	handle;
		if (SUCCEEDED(WindowsPreallocateStringBuffer((UINT32)chars_count<wchar_t>(s, e), &buffer, &handle))) {
			string_copy(buffer, s, e);
			return SUCCEEDED(WindowsPromoteStringBuffer(handle, &h));
		}
		return false;
	}
	void set(const HSTRING &h2) {
		if (h2 != h) {
			WindowsDeleteString(h);
			WindowsDuplicateString(h2, &h);
		}
	}

	hstring_base()					: h(nullptr)	{}
	hstring_base(HSTRING _h)		: h(_h)			{}
public:
	struct HSTRING_imp {
		unsigned	unknown1;
		unsigned	Length;
		unsigned	unknown2[2];
		wchar_t*	Pointer;
		unsigned	refs;
	};

	operator HSTRING()							const	{ return h; }
	HSTRING			get()						const	{ return h; }
	HSTRING			get_addref()				const	{ HSTRING h2; WindowsDuplicateString(h, &h2); return h2; }
	unsigned		length()					const	{ return WindowsGetStringLen(h); }
	bool			blank()						const	{ return WindowsIsStringEmpty(h); }
	bool			contains_null()				const	{ BOOL b; return SUCCEEDED(WindowsStringHasEmbeddedNull(h, &b)) && b; }
	const wchar_t*	raw(unsigned *length = 0)	const	{ return WindowsGetStringRawBuffer(h, length); }

	hstring			copy()									const;
	hstring			slice(int i)							const;
	hstring			slice(int i, int n)					const;
	hstring			trim_start(const hstring_base &trim)	const;
	hstring			trim_end(const hstring_base &trim)		const;
	HSTRING_imp*	imp()	const	{ return (HSTRING_imp*)h; }

	friend int		_compare(const hstring_base &a, const hstring_base &b) {
		int r;
		WindowsCompareStringOrdinal(a, b, &r);
		return r;
	}

	friend hstring	add(const hstring_base &a, const hstring_base &b);

	friend count_string16	to_string(const hstring_base &s) {
		unsigned length;
		auto	w =  s.raw(&length);
		return str(w, length);
	}
};

template<> class ptr<Platform::String> : public hstring_base {
public:
	ptr()												{}
	ptr(nullptr_t)										{}
	ptr(HSTRING _h)			: hstring_base(_h)			{}
	ptr(HSTRING _h, bool)	: hstring_base(_h)			{}	// no addref
	ptr(ptr &&b)			: hstring_base(b.detach())	{}
	ptr(const hstring_ref &b);
	ptr(const ptr &b)									{ WindowsDuplicateString(b.h, &h); }
	ptr(const wchar_t *s)								{ WindowsCreateString(s, string_len32(s), &h); }
	template<size_t N> ptr(const wchar_t (&s)[N])		{ WindowsCreateString(s, N - 1, &h); }
	template<typename B> ptr(const string_base<B> &s)	{ init(s.begin(), s.end()); }
	~ptr()												{ if (h) WindowsDeleteString(h); }

	HSTRING			detach()							{ HSTRING t = h; h = nullptr; return t; }
	void			attach(HSTRING hstr)				{ WindowsDeleteString(h); h = hstr; }

	ptr&			operator=(ptr&& b)					{ swap(h, b.h); return *this; }
	ptr&			operator=(const ptr& b)				{ set(b.h); return *this; }
	ptr&			operator=(const HSTRING &h2)		{ set(h2); return *this; }
	template<typename B>	ptr&	operator=(const string_base<B> &s)	{ WindowsDeleteString(h); init(s.begin(), s.end()); return *this; }
	template<size_t N>		ptr&	operator=(const wchar_t (&s)[N])	{ WindowsDeleteString(h); WindowsCreateString(s, N - 1, &h); return *this; }

	friend uint32	hash(const ptr &s)		{
		unsigned length;
		auto	w =  s.raw(&length);
		return string_hash(w, length);
	}
};

template<> class pptr<Platform::String> : public hstring_base {
	HSTRING_HEADER	header;
private:
	HRESULT CreateReference(const wchar_t* s, unsigned len) {
		return WindowsCreateStringReference(s, len, &header, &h);
	}
public:
	pptr(nullptr_t)										{}
	pptr(HSTRING h)		: hstring_base(h)				{}
	pptr(hstring h)		: hstring_base(h.detach())		{}
	pptr(const wchar_t* s, unsigned len)				{ CreateReference(s, len); }
	template<size_t N> pptr(const wchar_t (&s)[N])		{ CreateReference(s, N - 1); }
//	template<size_t N> pptr(wchar_t (&s)[N])			{ CreateReference(s, string_len(s)); }
	pptr(wchar_t *s)		{ CreateReference(s, (unsigned)string_len(s)); }
	pptr(const wchar_t *&s)	{ CreateReference(s, (unsigned)string_len(s)); }
//	template<typename V> pptr(wchar_t const *s, V *v = (void*)0)	{ CreateReference(s, (unsigned)string_len(s)); }
//	explicit			pptr(wchar_t const *s)			{ CreateReference(s, (unsigned)string_len(s)); }
//	template<size_t N> pptr(const meta::array<wchar_t, N> &s)	{ CreateReference(s.begin(), N - 1); }//must be zero-terminated
	template<typename C, size_t N> pptr(const meta::array<C, N> &s)	{ init(s.begin(), s.end()); }//CreateReference(s.begin(), N, N - 1); }//not zero-terminated
	template<typename B> pptr(const string_base<B> &s)	{ init(s.begin(), s.end()); }
};

inline ptr<Platform::String>::ptr(const hstring_ref &b) { WindowsDuplicateString(b, &h); }

//-----------------------------------------------------------------------------
//	abi type conversions
//-----------------------------------------------------------------------------

template<typename T> struct from_abi_s			{ typedef T type; };
template<typename T> using from_abi_t			= typename from_abi_s<T>::type;
template<typename T> struct from_abi_s<T&>		: from_abi_s<T> {};
template<typename T> struct from_abi_s<T*>		{ typedef ptr<from_abi_t<T> > type; };
template<template<typename...> class T, typename...U> struct from_abi_s<T<U...>> { typedef T<from_abi_t<U>...> type; };

template<typename T> inline auto	from_abi(T &&t)				{ return forward<T>(t); }
template<typename T> inline ptr<T>	from_abi(T *t)				{ return ptr<T>(t, true); }
template<typename T> inline	szarray<ptr<T>> from_abi(szarray<T*> &a) { return {reinterpret_cast<ptr<T>*>(a.p), a.size}; }
template<template<typename...> class T, typename...U> inline auto	from_abi(T<U...> *t) { return pptr<T<from_abi_t<U>...>>(t); }

template<typename T> struct to_abi_s			{ typedef T type; };
template<typename T> using to_abi_t				= typename to_abi_s<T>::type;
template<typename T> struct to_abi_s<T&>		: to_abi_s<T> {};
template<typename T> struct to_abi_s<ptr<T>>	{ typedef to_abi_t<T> *type; };
template<template<typename...> class T, typename...U> struct to_abi_s<T<U...>> { typedef T<to_abi_t<U>...> type; };

template<typename T> inline auto	to_abi(T &&t)		{ return forward<T>(t); }
template<typename T> inline auto	to_abi(ptr<T> &&t)	{ return to_abi(t.detach()); }
template<typename T> inline auto	to_abi(ptr<T> &t)	{ return to_abi(t.get_addref()); }
template<typename T> inline auto	to_abi(ptr<T> *t)	{ return (T**)t; }
template<typename T> inline auto	to_abi(T *t)		{ return t; }
template<typename T> inline auto	to_abi(pptr<T> &&t)	{ return to_abi((T*)t); }
template<typename T> inline auto	to_abi(pptr<T> &t)	{ return to_abi((T*)t); }
template<template<typename...> class T, typename...U> inline auto	to_abi(T<U...> *t) { return (T<to_abi_t<U>...>*)(t); }

//-----------------------------------------------------------------------------
//	property helpers
//-----------------------------------------------------------------------------

template<typename S, typename T>	inline S*	encloser(T *p, T S::*t)	{ return (S*)((uint8*)p - uintptr_t(&(((S*)0)->*t))); }

struct property {
	property()	{}
	property(const property&)			= delete;
	property& operator=(const property&)= delete;
	property& operator=(property&&)		= delete;
	template<typename S, typename T>	inline auto	enc(T S::*t)	{ return encloser(static_cast<T*>(this), t)->get(); }
};

template<typename T>				struct prop_getter;
template<typename T>				struct prop_getter<T*>			{ template<T *p, typename R>	static void	get(void*, R *r)	{ *r = to_abi((from_abi_t<R>)(*p)); } };
template<typename X, typename T>	struct prop_getter<T X::*>		{ template<T X::*p, typename R>	static void	get(X *x, R *r)		{ *r = to_abi((from_abi_t<R>)(x->*p)); } };
template<typename X, typename T>	struct prop_getter<T (X::*)()>	{
//	template<T (X::*p)()>				static auto get(X *x, R *r)		{ return to_abi(x->*p)()); }// causes compiler crash
	template<typename V> static V&&		get2(V &&v)						{ return forward<V>(v); }
	template<typename V> static auto	get2(ptr<V> &&v)				{ return v.detach(); }
	template<typename V> static auto	get2(ptr<V> &v)					{ return v.get_addref(); }
	template<T (X::*p)(), typename R>	static void	get(X *x, R *r)		{ *r = get2((x->*p)()); }
};

template<typename T> prop_getter<T*>							get_prop_getter(T *p)				{ return prop_getter<T*>(); }
template<typename X, typename T> prop_getter<T X::*>			get_prop_getter(T X::*p)			{ return prop_getter<T X::*>(); }
template<typename X, typename T> prop_getter<T (X::*)()>		get_prop_getter(T (X::*p)())		{ return prop_getter<T (X::*)()>(); }
#define get_prop(rr, p)	{ typedef typename X::type T; auto t = X::get(); auto r = rr; get_prop_getter(&T::p).template get<&T::p>(t, r); }

template<typename T>				struct prop_putter;
template<typename T>				struct prop_putter<T*>				{ template<T *p, typename V>			static void put(void*, V&& v)	{ *p = forward<V>(v); } };
template<typename X, typename T>	struct prop_putter<T X::*>			{ template<T X::*p, typename V>			static void put(X *x, V&& v)	{ x->*p = forward<V>(v); } };
template<typename X, typename T>	struct prop_putter<void (X::*)(T)>	{ template<void (X::*p)(T), typename V>	static void put(X *x, V&& v)	{ (x->*p)(forward<V>(v)); } };

template<typename T> prop_putter<T*>							get_prop_putter(T *p)				{ return prop_putter<T*>(); }
template<typename X, typename T> prop_putter<T X::*>			get_prop_putter(T X::*p)			{ return prop_putter<T X::*>(); }
template<typename X, typename T> prop_putter<void (X::*)(T)>	get_prop_putter(void (X::*p)(T))	{ return prop_putter<void (X::*)(T)>(); }
#define put_prop(rr, p)	{ typedef X::type T; auto t = X::get(); auto r = rr; get_prop_putter(&T::p).template put<&T::p>(t, r); }

//-----------------------------------------------------------------------------
//	compositors
//-----------------------------------------------------------------------------

template<typename I, typename X>		struct unadapt	: X		{};
template<typename I, typename X>		struct adapt	: X		{};
template<typename I, typename X>		struct statics	: X		{};

//collect all interfaces
template<typename T, typename D>		struct collect_intfs1;
template<typename T>					struct collect_intfs								: collect_intfs1<T, typename def<T>::type> {};
//template<>								struct collect_intfs<Platform::Object>				{ typedef types<Platform::Object> type; };
template<>								struct collect_intfs<Platform::Object>				{ typedef types<> type; };
template<typename T, typename... I>		struct collect_intfs1<T, class_type<I...>>			: types_cat<typename collect_intfs<I>::type...>		{};
template<typename T, typename... I>		struct collect_intfs1<T, interface_type<I...>>		: types_cat<T, typename collect_intfs<I>::type...>	{};
template<typename T, typename... I>		struct collect_intfs1<T, overridable_type<I...>>	: types_cat<T, typename collect_intfs<I>::type...>	{};

//collect only implemented interfaces
template<typename T, typename D>		struct collect_imp1									{ typedef types<> type; };
template<typename T>					struct collect_imp									: collect_imp1<T, typename def<T>::type> {};
template<>								struct collect_imp<Platform::Object>				{ typedef types<> type; };
template<typename T, typename... I>		struct collect_imp1<T, class_type<I...>>			: types_cat<typename collect_imp<I>::type...> {};
template<typename T, typename... I>		struct collect_imp1<T, overridable_type<I...>>		: types_cat<T, typename collect_intfs<I>::type...> {};

template<typename...I>					struct unique_interfaces : types_unique<typename types_cat<typename collect_intfs<I>::type...>::type> {};
template<>								struct unique_interfaces<Platform::Object>			{ typedef types<Platform::Object> type; };

template<typename X, typename I>		struct querier { typedef I type; auto get() { return query<I>(static_cast<X*>(this)); } };

template<typename A, typename... I> struct adapt_list : adapt<I, querier<A, I>>... {};
template<typename A, typename... I> struct adapt_list<A, types<I...>> : adapt<I, querier<A, I>>... {};

template<typename A, typename E, typename I0, typename...I>	struct adapt_list2 : I0, adapt_list<A, typename types_diff<typename unique_interfaces<E, I...>::type, typename collect_intfs<I0>::type>::type> {
	typedef A root_type;
};

template<typename A, typename D>				struct generate1;
template<typename A, typename E, typename... I>	struct generate1<A, class_type<E, I...>> 		: statics<A, adapt_list2<A, E, I...> > {};
template<typename A, typename... I>				struct generate1<A, interface_type<I...>> 		: adapt_list<A, typename unique_interfaces<A>::type> {};
template<typename A, typename... I>				struct generate1<A, overridable_type<I...>> 	: adapt_list<A, typename unique_interfaces<A>::type> {};
template<typename T>							struct generate : generate1<T, typename def<T>::type> {};

//-----------------------------------------------------------------------------
//	ref_new
//-----------------------------------------------------------------------------

template<typename T, typename F = IActivationFactory> F *get_activation_factory() {
	struct factory : ptr<F> {
		factory() { hrcheck(RoGetActivationFactory(hstring_ref(name<T>::value), uuidof<F>(), (void**)this)); }
	};
	static factory f;
	return f;
}

template<typename T, typename V=void> struct ref_new : ptr<T> {
	ref_new() {
		IInspectable	*i;
		hrcheck(get_activation_factory<T>()->ActivateInstance(&i));
		i->QueryInterface(uuidof<T>(), (void**)&this->t);
	}
};

template<typename T> struct ref_new<T, void_t<typename def<T>::composer>> : ptr<T> {
	template<typename...P> ref_new(P&&... p) {
		IInspectable* inner;
		this->t = T::activate(forward<P>(p)..., nullptr, &inner);
	}
};

template<typename T> struct ref_new<T, typename T_enable_if<T_inherits<def<T>, custom_activators>::value>::type> : ptr<T> {
	template<typename...P> ref_new(P&&... p) {
		this->t = T::activate(forward<P>(p)...);
	}
};

template<typename T> struct ref_new<Platform::Array<T>, void> : ptr<Platform::Array<T>> {
	ref_new()	{}
	ref_new(unsigned n)								: ptr<Platform::Array<T>>(n) {}
	ref_new(const T *a, unsigned n)					: ptr<Platform::Array<T>>(a, n) {}
	template<unsigned N> ref_new(const T (&a)[N])	: ptr<Platform::Array<T>>(a, N) {}
};

//-----------------------------------------------------------------------------
//	error reporting
//-----------------------------------------------------------------------------

namespace Windows { namespace Foundation {
	struct Exception {
		hstring	message;
		Exception()	{}
		Exception(hstring_ref _message)	: message(_message) {}
	};
	struct COMException : Exception {
		HRESULT hr;
		COMException(HRESULT _hr) : hr(_hr) {}
		COMException(HRESULT _hr, hstring_ref _message) : Exception(_message), hr(_hr) {}
	};

	struct OutOfMemoryException			: COMException { OutOfMemoryException()			: COMException(E_OUTOFMEMORY) {} OutOfMemoryException(hstring_ref _message)				: COMException(E_OUTOFMEMORY, _message) {} };
	struct ClassNotRegisteredException	: COMException { ClassNotRegisteredException()	: COMException(REGDB_E_CLASSNOTREG) {} ClassNotRegisteredException(hstring_ref _message): COMException(REGDB_E_CLASSNOTREG, _message) {} };
	struct DisconnectedException		: COMException { DisconnectedException()		: COMException(RPC_E_DISCONNECTED) {} DisconnectedException(hstring_ref _message)		: COMException(RPC_E_DISCONNECTED, _message) {} };
	struct InvalidArgumentException		: COMException { InvalidArgumentException()		: COMException(E_INVALIDARG) {} InvalidArgumentException(hstring_ref _message)			: COMException(E_INVALIDARG, _message) {} };
	struct InvalidCastException			: COMException { InvalidCastException()			: COMException(E_NOINTERFACE) {} InvalidCastException(hstring_ref _message)				: COMException(E_NOINTERFACE, _message) {} };
	struct NullReferenceException		: COMException { NullReferenceException()		: COMException(E_POINTER) {} NullReferenceException(hstring_ref _message)				: COMException(E_POINTER, _message) {} };
	struct NotImplementedException		: COMException { NotImplementedException()		: COMException(E_NOTIMPL) {} NotImplementedException(hstring_ref _message)				: COMException(E_NOTIMPL, _message) {} };
	struct AccessDeniedException		: COMException { AccessDeniedException()		: COMException(E_ACCESSDENIED) {} AccessDeniedException(hstring_ref _message)			: COMException(E_ACCESSDENIED, _message) {} };
	struct FailureException				: COMException { FailureException()				: COMException(E_FAIL) {} FailureException(hstring_ref _message)						: COMException(E_FAIL, _message) {} };
	struct OutOfBoundsException			: COMException { OutOfBoundsException()			: COMException(E_BOUNDS) {} OutOfBoundsException(hstring_ref _message)					: COMException(E_BOUNDS, _message) {} };
	struct ChangedStateException		: COMException { ChangedStateException()		: COMException(E_CHANGED_STATE) {} ChangedStateException(hstring_ref _message)			: COMException(E_CHANGED_STATE, _message) {} };
	struct OperationCanceledException	: COMException { OperationCanceledException()	: COMException(E_ABORT) {} OperationCanceledException(hstring_ref _message)				: COMException(E_ABORT, _message) {} };
	struct WrongThreadException			: COMException { WrongThreadException()			: COMException(RPC_E_WRONG_THREAD) {} WrongThreadException(hstring_ref _message)		: COMException(RPC_E_WRONG_THREAD, _message) {} };
	struct ObjectDisposedException		: COMException { ObjectDisposedException()		: COMException(RO_E_CLOSED) {} ObjectDisposedException(hstring_ref _message)			: COMException(RO_E_CLOSED, _message) {} };
} }

class com_string : public string_base<BSTR> {
public:
	com_string()				{ p = 0; }
	~com_string()				{ SysFreeString(p); }
	BSTR*	operator&()			{ SysFreeString(p); return &p; }
	size_t	length()	const	{ return SysStringLen(p); }
	void	clear()				{ SysFreeString(p); p = 0; }
};

template<typename> struct hrerror_s {
	static void	display(HRESULT hr);
};

inline void hrcheck(HRESULT hr) {
	if (hr != S_OK) {
		if (FAILED(hr))
			hrerror_s<void>::display(hr);
		throw Platform::COMException(hr);
	}
}

template<typename T> void hrerror_s<T>::display(HRESULT hr) {
	ptr<IRestrictedErrorInfo>	info;
	HRESULT						hrErr;
	com_string					desc, restricted, sid;
	if (SUCCEEDED(GetRestrictedErrorInfo((IRestrictedErrorInfo**)&info)) && info && SUCCEEDED(info->GetErrorDetails(&desc, &hrErr, &restricted, &sid))) {
		//Get empty error message text to remove "The text associated with this error code could not be found" messages
		ptr<IRestrictedErrorInfo>	empty;
		HRESULT						hrDummy;
		com_string					empty_desc, dummy1, dummy2;
		RoOriginateError(-1, NULL);
		if (SUCCEEDED(GetRestrictedErrorInfo((IRestrictedErrorInfo**)&empty)) && empty && SUCCEEDED(empty->GetErrorDetails(&empty_desc, &hrDummy, &dummy1, &dummy2))) {
			if (desc == empty_desc)
				desc.clear();
			if (restricted == empty_desc)
				restricted.clear();
		}
		ISO_TRACEF("Failed with HRESULT = 0x%08x: ", hr) << desc << "; " << restricted << '\n';
	} else {
		ISO_TRACEF("Failed with HRESULT = 0x%08x\n", hr);
	}
}

template<typename L> inline HRESULT hrtry(L &&lambda) {
	try {
		lambda();
	} catch (const Platform::COMException &e) {
		return e.hr;
	}
	return S_OK;
}

//-----------------------------------------------------------------------------
//	handler
//-----------------------------------------------------------------------------

template<typename T, typename L> struct handler;

template<typename T> struct handler_ref : ptr<to_abi_t<T> > {
	typedef to_abi_t<T>	TR;

	template<typename L> static TR *make_handler(L &&lambda) {
		return new handler<TR,L>(forward<L>(lambda));
	}

	template<typename L>				handler_ref(L &&lambda) : ptr<TR>(new handler<TR, L>(forward<L>(lambda))) {}
	template<typename C, typename...P>	handler_ref(C *c, void(C::*f)(P...)) : ptr<TR>(make_handler([c, f](P&&... p) { (c->*f)(forward<P>(p)...); })) {}

	handler_ref(handler_ref &&h)				= default;
	handler_ref&	operator=(handler_ref &&h)	= default;
	handler_ref(TR *t) : ptr<TR>(t) {}
};

template<typename T> inline auto	to_abi(handler_ref<T> &t)	{ return t.get(); }

} // namespace iso_winrt

//#endif // WINRT_PREINCLUDE_H
