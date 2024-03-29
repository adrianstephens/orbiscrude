#ifndef COM_H
#define COM_H

#include "base/defs.h"
#include "base/tuple.h"
#include "allocators/allocator.h"

typedef struct _GUID GUID;
typedef long	HRESULT;
struct IUnknown;

#ifdef PLAT_PC

#undef NONAMELESSUNION
#define _FORCENAMELESSUNION

#include <Objbase.h>
#undef small
#undef min
#undef max

#undef FAILED
#undef SUCCEEDED

namespace iso {
extern void com_error_handler(HRESULT hr);
}

inline bool FAILED(HRESULT hr) {
	if (hr < 0)
		iso::com_error_handler(hr);
	return hr < 0;
}
inline bool SUCCEEDED(HRESULT hr) {
	return !FAILED(hr);
}

#endif

namespace iso {

//-----------------------------------------------------------------------------
//	query helpers
//-----------------------------------------------------------------------------

template<typename T> inline constexpr exists_t<decltype(__uuidof(T)), const GUID> uuidof(T*)	{ return __uuidof(T); }
template<typename T> inline constexpr const GUID uuidof()		{ return uuidof((T*)0); }

template<typename T> struct is_com : meta::bool_constant<T_is_base_of<IUnknown, T>::value> {};

template<typename T2> inline HRESULT	query(IUnknown *t, T2 **p, const GUID &guid)	{ return t->QueryInterface(guid, (void**)p); }
template<typename T2> inline HRESULT	query(IUnknown *t, T2 **p)						{ return query(t, p, uuidof<T2>()); }
template<typename T2> inline T2*		_query(IUnknown *t)								{ T2 *p = 0; query(t, &p); return p; }

struct querier {
	IUnknown	*p;
	querier(IUnknown *p) : p(p) {}
	template<typename T> operator T*()	const	{ return _query<T>(p); }
};

#define COM_CREATE(pp)		__uuidof(**(pp)), (void**)(pp)
#define COM_CREATE2(T, pp)	__uuidof(T), (void**)(pp)

//-----------------------------------------------------------------------------
//	_com_ptr
//-----------------------------------------------------------------------------

template<typename T> class _com_ptr {
protected:
	T	*t;
	T**			get_addr()					{ ISO_ASSERT(!t); return &t; }
	T* const*	get_addr()		const		{ return &t; }
	void		add_ref()					{ if (t) t->AddRef(); }
	void		release()					{ if (t) t->Release(); }
	void		set(T *p)					{ if (p && p != t) { p->AddRef(); release(); } t = p; }
public:
	_com_ptr(T *t = nullptr) : t(t) {}
	~_com_ptr()								{ this->release();	}
	T*			get()			const		{ return t;	 }
	void		attach(T *p)				{ release(); t = p; }
	T*			detach()					{ return exchange(t, nullptr); }
	T**			operator&()					{ ISO_ASSERT(!t); return &t; }
	T* const*	operator&()		const		{ return &t; }
	explicit	operator bool()	const		{ return !!t; }
				operator T*()	const		{ return t; }
    T*			operator->()	const		{ return t; }
   	bool		operator!()		const		{ return !t; }
	void		clear()						{ release(); t = nullptr; }
	static const GUID	uuid()				{ return uuidof<T>(); }

	template<typename T2> HRESULT	query(T2 **p, const GUID &guid)	const	{ return t->QueryInterface(guid, (void**)p); }
	template<typename T2> HRESULT	query(T2 **p)					const	{ return query(p, uuidof<T2>()); }
	querier							query()							const	{ return t; }

#ifdef _OBJBASE_H_
	_com_ptr<T>&		create(const CLSID &clsid, uint32 context = CLSCTX_INPROC_SERVER) {
		ISO_ASSERT(!t);
		CoCreateInstance(clsid, nullptr, context, uuidof<T>(), (void**)&t);
		return *this;
	}
	template<typename C> _com_ptr<T>& create(uint32 context = CLSCTX_INPROC_SERVER) {
		return create(uuidof<C>(), context);
	}
#endif

	friend T*	get(const _com_ptr &a)	{ return a; }
};

//-----------------------------------------------------------------------------
//	com_ptr
//	prevents copy, and no addref on construction
//-----------------------------------------------------------------------------

template<typename T> class com_ptr : public _com_ptr<T> {
public:
	com_ptr()									{}
	com_ptr(nullptr_t)							{}
	com_ptr(T *t)			: _com_ptr<T>(t)	{}
	com_ptr(com_ptr &&p)	: _com_ptr<T>(p.detach())	{}
	com_ptr(querier q)		: _com_ptr<T>(q)	{}
	com_ptr(const CLSID &clsid, uint32 context = CLSCTX_INPROC_SERVER) { CoCreateInstance(clsid, NULL, context, uuidof<T>(), (void**)&t); }
	com_ptr&	operator=(com_ptr &&p)			{ swap(this->t, p.t); return *this; }

	using _com_ptr<T>::query;
	template<typename T2> com_ptr<T2>	query()	const	{ T2 *p = nullptr; query(&p); return p; }
	template<typename T2> com_ptr<T2>	as()	const	{ T2 *p = nullptr; query(&p); return p; }

	friend void	swap(com_ptr &a, com_ptr &b)	{ swap(a.t, b.t); }
};

template<typename T> inline com_ptr<T>	query(IUnknown *p)			{ return _query<T>(p); }
template<typename T> inline com_ptr<T>	temp_com_cast(IUnknown *p)	{ return _query<T>(p); }

template<typename B, typename A> inline enable_if_t<T_conversion<A*,B*>::exists, B*>			com_cast(A *a) { return a; }
template<typename B, typename A> inline enable_if_t<!T_conversion<A*,B*>::exists, com_ptr<B>>	com_cast(A *a) { return _query<B>(a); }

//-----------------------------------------------------------------------------
//	com_ptr2
//	copyable
//-----------------------------------------------------------------------------

template<typename T> class com_ptr2 : public _com_ptr<T> {
public:
	com_ptr2()										{}
	com_ptr2(nullptr_t)								{}
	com_ptr2(T *t)				: _com_ptr<T>(t)	{ this->add_ref(); }
	com_ptr2(com_ptr<T> &&p)	: _com_ptr<T>(p.detach())	{}
	com_ptr2(const com_ptr2 &p)	: _com_ptr<T>(p)	{ this->add_ref(); }
	com_ptr2(com_ptr2 &&p)		: _com_ptr<T>(p.detach())	{}
	com_ptr2(const CLSID &clsid, uint32 context = CLSCTX_INPROC_SERVER) { CoCreateInstance(clsid, NULL, context, uuidof<T>(), (void**)&t); }
	T**			operator&()							{ this->clear(); return &this->t; }
	com_ptr2&	operator=(T *p)						{ this->set(p); return *this; }
	com_ptr2&	operator=(const com_ptr2 &p)		{ this->set(p.t); return *this; }
	com_ptr2&	operator=(com_ptr<T> &&p)			{ this->release(); this->t = p.detach(); return *this; }

	using _com_ptr<T>::query;
	template<typename T2> com_ptr<T2> query() const	{ T2 *p = 0; query(&p); return p; }

	friend void		swap(com_ptr2 &a, com_ptr2 &b)	{ swap(a.t, b.t); }
};

//-----------------------------------------------------------------------------
//	com_iterator
//	for com objects with a Next(ULONG, T**, ULONG*) method
//-----------------------------------------------------------------------------

template<typename E, typename T> class com_iterator : public com_ptr<E> {
	com_ptr<T>	t;
public:
	com_iterator&	operator++() {
		ULONG	fetched;
		t.clear();
		(*this)->Next(1, &t, &fetched);
		return *this;
	}
	operator T*()	const	{ return t; }
	operator com_ptr<T>&()	{ return t; }
};

#ifdef PLAT_PC

//-----------------------------------------------------------------------------
//	com functions
//-----------------------------------------------------------------------------

HRESULT CoCreateInstanceFromDLL(void **unknown, const char *fn, const CLSID &clsid, const IID &iid);

inline void init_com() {
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

inline uint32 get_refcount(IUnknown *p) {
	p->AddRef();
	return p->Release();
}

template<typename D, typename T> inline bool check_interface(T *t, REFIID riid, void **ppv) {
	if (riid == uuidof<D>()) {
		*ppv = static_cast<D*>(t);
		t->AddRef();
		return true;
	}
	return false;
}

template<typename T> inline bool check_interfaces2(T *t, REFIID riid, void **ppv) {
	return false;
}

template<typename T, typename D, typename...DD> inline bool check_interfaces2(T *t, REFIID riid, void **ppv) {
	return check_interface<D>(t, riid, ppv) || check_interfaces2<T, DD...>(t, riid, ppv);
}

template<typename...DD, typename T> inline bool check_interfaces(T *t, REFIID riid, void **ppv) {
	return check_interfaces2<T, DD...>(t, riid, ppv);
}

//-----------------------------------------------------------------------------
//	com
//	base class for com classes
//-----------------------------------------------------------------------------

class com_base {
	LONG	refs;
protected:
	com_base(const com_base&) : refs(1)	{}
	com_base() : refs(1)	{}
	virtual ~com_base()		{}

	ULONG	AddRef()	{ return InterlockedIncrement(&refs); }
	ULONG	Release()	{ return InterlockedDecrement(&refs); }
};

template<typename T, typename B = com_base> struct com : public T, public B {
	ULONG	STDMETHODCALLTYPE	AddRef()	{ return B::AddRef(); }
	ULONG	STDMETHODCALLTYPE	Release()	{ return B::Release();}
	HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID riid, void **ppv) {
		return check_interface<T>(this, riid, ppv) ? S_OK : B::QueryInterface(riid, ppv);
	}
};

template<typename T> struct com<T, com_base> : public T, public com_base {
	ULONG	STDMETHODCALLTYPE	AddRef() {
		return com_base::AddRef();
	}
	ULONG	STDMETHODCALLTYPE	Release() { 
		auto	n = com_base::Release();
		if (!n)
			delete this;
		return n;
	}
	HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID riid, void **ppv) {
		return check_interface<T>(this, riid, ppv) || check_interface<IUnknown>(this, riid, ppv) ? S_OK : E_NOINTERFACE;
	}
};

template<typename T0, typename...T> class com_list : public com<T0, com_list<T...>> {};
template<typename T> struct com_list<T> : public com<T> {};
template<typename T0, typename...T> struct com_list<type_list<T0, T...>> : public com_list<T0, T...> {};

// com_inherit: check for additional types in QueryInterface
template<typename T, typename X> struct com_inherit : public com_inherit<meta::TL_tail_t<T>, X> {
	typedef meta::TL_head_t<T>					H;
	typedef com_inherit<meta::TL_tail_t<T>, X>	B;
	ULONG	STDMETHODCALLTYPE	AddRef()	{ return X::AddRef(); }
	ULONG	STDMETHODCALLTYPE	Release()	{ return X::Release();}
	HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID riid, void **ppv) {
		return check_interface<H>(this, riid, ppv) ? S_OK : B::QueryInterface(riid, ppv);
	}
};

template<typename X> struct com_inherit<type_list<>, X> : public X {};

#endif	//PLAT_PC

}//namespace iso

#ifdef _OLEAUTO_H_
//-----------------------------------------------------------------------------
//	COM helpers
//-----------------------------------------------------------------------------

#include "base/strings.h"

extern const IID GUID_NULL;

namespace iso {

struct com_allocator {
	static void		*alloc(size_t size)				{ return CoTaskMemAlloc(size); }
	static void		*realloc(void *p, size_t size)	{ return CoTaskMemRealloc(p, size); }
	static void		free(void *p)					{ CoTaskMemFree(p); }
	template<typename T, typename...P>	static T*	make(P&&...p)		{ return new(alloc(sizeof(T))) T(forward<P>(p)...); }
	template<typename C>				static C*	strdup(const C *p)	{ auto n = string_len(p); C *p2 = (C*)alloc((n + 1) * sizeof(C)); string_copy(p2, p, n); return p2; }
	template<typename T>				static void	del(T *t)			{ t->~T(); free(t); }
};

template<typename T> class unique_com {
protected:
	T		*a;
	void	set(T* p2)	{
		if (T *p1 = exchange(a, p2))
			if (p1 != p2)
				com_allocator::del(p1);
	}
public:
	template<typename...P> unique_com&	emplace(P&&...pp)	{ set(com_allocator::make<T>(forward<P>(pp)...)); return *this; }
	template<typename...P> unique_com(P&&...pp)		: a(com_allocator::make<T>(forward<P>(pp)...)) {}
	constexpr unique_com()							: a(nullptr)	{}
	constexpr unique_com(const _none&)				: a(nullptr)	{}
	constexpr unique_com(nullptr_t)					: a(nullptr)	{}
	/*explicit*/ constexpr unique_com(T *p)			: a(p)			{}
	unique_com(unique_com &&b)						: a(b.detach())	{}
	~unique_com()									{ com_allocator::del(a); }

	unique_com&		operator=(T *b)					{ set(b); return *this; }
	unique_com&		operator=(unique_com &&b)		{ swap(a, b.a); return *this; }
	template<class T2>	unique_com& operator=(unique_com<T2> &&b) { set(b.detach()); return *this; }

	T**				operator&()						{ return &a; }
	constexpr operator T*()			const			{ return a; }
	constexpr T*	operator->()	const			{ return a; }
	constexpr T*	get()			const			{ return a; }
	void			clear()							{ if (a) { com_allocator::del(a); a = 0; } }
	T*				detach()						{ return exchange(a, nullptr); }

	friend void	swap(unique_com &a, unique_com &b)	{ swap(a.a, b.a); }
	friend constexpr T*	get(const unique_com &a)	{ return a; }
	friend constexpr T*	put(unique_com &a)			{ return a; }
};

class com_string : public string_base<BSTR> {
public:
	com_string() {
		p = 0;
	}
	com_string(const wchar_t *s) {
		p = SysAllocString(s);
	}
	com_string(const char *s) {
		int		len	= MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
		wchar_t	*s2	= alloc_auto(wchar_t, len);
		MultiByteToWideChar(CP_ACP, 0, s, len, s2, len);
		p = SysAllocStringLen(s2, len - 1);
	}
	~com_string() {
		SysFreeString(p);
	}
	void operator=(const char *s) {
		int		len	= MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
		wchar_t	*s2	= alloc_auto(wchar_t, len);
		MultiByteToWideChar(CP_ACP, 0, s, -1, s2, len);
		if (p)
			SysReAllocStringLen(&p, s2, len - 1);
		else
			p = SysAllocStringLen(s2, len - 1);
	}
	void operator=(const wchar_t *s) {
		if (p)
			SysReAllocString(&p, s);
		else
			p = SysAllocString(s);
	}

	size_t		length()		const	{ return SysStringLen(p); }
	BSTR*		operator&()				{ SysFreeString(p); return &p; }

	template<int N> operator fixed_string<N>() const {
		fixed_string<N>	s;
		if (p)
			WideCharToMultiByte(CP_ACP, 0, p, length(), s, N, NULL, NULL);
		return s;
	}
	operator string() const {
		if (!p)
			return string();
		int		n = WideCharToMultiByte(CP_ACP, 0, p, -1, NULL, 0, NULL, NULL);
		string	s(n - 1);
		WideCharToMultiByte(CP_ACP, 0, p, -1, s.begin(), n, NULL, NULL);
		return s;
	}
	friend size_t to_string(char *s, const com_string &v)	{ return string_copy(s, v.begin(), v.length()); }
};


class ole_string : public string_base<LPOLESTR> {
	typedef string_base<LPOLESTR> B;
	OLECHAR			*_alloc(size_t n)						{ return p = n ? (OLECHAR*)com_allocator::alloc((n + 1) * sizeof(OLECHAR)) : 0; }
	template<typename S> void init(const S *s, size_t n)	{ string_copy(_alloc(n), s, n); }
	void			init(const char *s)						{
		int	len	= MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
		MultiByteToWideChar(CP_ACP, 0, s, len, _alloc(len - 1), len);
	}
public:
	ole_string()	: B(0) {}
	ole_string(const OLECHAR *s)		{ init(s, string_len(s)); }
	ole_string(const char *s)			{ init(s); }
	~ole_string()						{ com_allocator::free(p); }
	void operator=(const char *s)		{ com_allocator::free(p); init(s); }
	void operator=(const OLECHAR *s)	{ com_allocator::free(p); init(s, string_len(s)); }

	operator	LPOLESTR()		const&	{ return p; }
	operator	LPOLESTR()		&&		{ return exchange(p, nullptr); }
	LPOLESTR*	operator&()				{ com_allocator::free(p); return &p; }

	template<int N> operator fixed_string<N>() const {
		fixed_string<N>	s;
		if (p)
			WideCharToMultiByte(CP_ACP, 0, p, length(), s, N, NULL, NULL);
		return s;
	}
	operator string() const {
		if (!p)
			return string();
		int		n = WideCharToMultiByte(CP_ACP, 0, p, -1, NULL, 0, NULL, NULL);
		string	s(n - 1);
		WideCharToMultiByte(CP_ACP, 0, p, -1, s.begin(), n, NULL, NULL);
		return s;
	}
};

inline size_t to_string(char *s, const ole_string &v)	{ return string_copy(s, v.begin(), v.length()); }

//-----------------------------------------------------------------------------
//	class Variant
//-----------------------------------------------------------------------------

template<typename T> class com_array;

constexpr VARENUM operator|(VARENUM a, int b) { return VARENUM(int(a) | b); }

template<typename T> struct com_vartype;
template<VARENUM E> using com_enum = meta::constant<VARENUM, E>;
template<> struct com_vartype<bool>			: com_enum<VT_BOOL>					{};
template<> struct com_vartype<int8>			: com_enum<VT_I1>					{};
template<> struct com_vartype<uint8>		: com_enum<VT_UI1>					{};
template<> struct com_vartype<int16>		: com_enum<VT_I2>					{};
template<> struct com_vartype<uint16>		: com_enum<VT_UI2>					{};
template<> struct com_vartype<int32>		: com_enum<VT_I4>					{};
template<> struct com_vartype<uint32>		: com_enum<VT_UI4>					{};
template<> struct com_vartype<int64>		: com_enum<VT_I8>					{};
template<> struct com_vartype<uint64>		: com_enum<VT_UI8>					{};
template<> struct com_vartype<float>		: com_enum<VT_R4>					{};
template<> struct com_vartype<double>		: com_enum<VT_R8>					{};
template<> struct com_vartype<CY>			: com_enum<VT_CY>					{};
template<> struct com_vartype<DECIMAL>		: com_enum<VT_BSTR>					{};
template<> struct com_vartype<IUnknown*>	: com_enum<VT_UNKNOWN>				{};
template<> struct com_vartype<IDispatch*>	: com_enum<VT_DISPATCH>				{};
template<> struct com_vartype<SAFEARRAY*>	: com_enum<VT_VARIANT | VT_ARRAY>	{};
template<> struct com_vartype<BSTR>			: com_enum<VT_BSTR>					{};
template<> struct com_vartype<com_string>	: com_enum<VT_BSTR>					{};
template<> struct com_vartype<char*>		: com_enum<VT_BSTR>					{};

//#ifndef PLAT_CLANG

class com_variant : public PROPVARIANT {
	struct either {
		com_variant	*p;
		constexpr operator PROPVARIANT*() const { return p; }
		constexpr operator VARIANT*() const { return (VARIANT*)p; }
	};

	PROPVARIANT	*as(VARENUM _vt)	{ vt = _vt; return this; }
public:
	com_variant()					{ PropVariantInit(this); }
	~com_variant()					{ PropVariantClear(this); }
	com_variant(VARIANT &&v)		{ *(VARIANT*)this = v; v.vt = VT_EMPTY; }

	explicit com_variant(bool v)	{ V_BOOL(as(VT_BOOL))	= v ? VARIANT_TRUE : VARIANT_FALSE; }
	com_variant(int8 v)				{ V_I1	(as(VT_I1)	)	= v; }
	com_variant(uint8 v)			{ V_UI1	(as(VT_UI1)	)	= v; }
	com_variant(int16 v)			{ V_I2	(as(VT_I2)	)	= v; }
	com_variant(uint16 v)			{ V_UI2	(as(VT_UI2)	)	= v; }
	com_variant(int32 v)			{ V_I4	(as(VT_I4)	)	= v; }
	com_variant(uint32 v)			{ V_UI4	(as(VT_UI4)	)	= v; }
	com_variant(long v)				{ V_I4	(as(VT_I4)	)	= v; }
	com_variant(ulong v)			{ V_I4	(as(VT_UI4)	)	= v; }
	com_variant(int64 v)			{ as(VT_I8)->hVal.QuadPart		= v; }
	com_variant(uint64 v)			{ as(VT_UI8)->uhVal.QuadPart	= v; }
	com_variant(float v)			{ V_R4	(as(VT_R4)	)	= v; }
	com_variant(double v)			{ V_R8	(as(VT_R8)	)	= v; }
	com_variant(const CY &v)		{ V_CY	(as(VT_CY)	)	= v; }
	com_variant(BSTR v)				{ V_BSTR(as(VT_BSTR))	= v; }
	com_variant(const com_string &v){ V_BSTR(as(VT_BSTR))	= unconst(v); }
	//com_variant(const char *v)		{ as(VT_BSTR); new(&bstrVal) com_string(v); }
	//com_variant(const wchar_t *v)	{ as(VT_BSTR); new(&bstrVal) com_string(v); }
	com_variant(const char *v)		{ as(VT_LPSTR)->pszVal = com_allocator::strdup(v);  }
	com_variant(const wchar_t *v)	{ as(VT_LPWSTR)->pwszVal = com_allocator::strdup(v);  }
	com_variant(ole_string &&v)		{ as(VT_LPWSTR)->pwszVal = move(v);  }
	com_variant(const DECIMAL &v)	{ V_DECIMAL(this) = v; as(VT_DECIMAL);	}//order matters
	com_variant(const FILETIME &v)	{ as(VT_FILETIME)->filetime		= v; }
	com_variant(IUnknown *v)		{ V_UNKNOWN (as(VT_UNKNOWN))	= v; }
	com_variant(IDispatch *v)		{ V_DISPATCH(as(VT_DISPATCH))	= v; }
	com_variant(SAFEARRAY *v)		{ SafeArrayGetVartype(v, &vt); vt |= VT_ARRAY; V_ARRAY(this) = v; }
	template<typename T> com_variant(const T *&t)				{ as(VT_BYREF | com_vartype<T>::value); pcVal = (char*)t; }
	template<typename T> com_variant(const T *p, int N)			{ V_ARRAY(as(VT_ARRAY | com_vartype<T>::value)) = com_array<T>(p, N).detach(); }
	template<typename T, int N> com_variant(const T (&v)[N])	{ V_ARRAY(as(VT_ARRAY | com_vartype<T>::value)) = com_array<T>(v).detach(); }

	int		type()			const	{ return vt;			}
	void	clear()					{ PropVariantClear(this);	}
	PROPVARIANT	detach()			{ PROPVARIANT p = *this; vt = VT_EMPTY; return p; }

	void	operator=(SAFEARRAY *sa){ vt = VT_VARIANT | VT_ARRAY; V_ARRAY(this) = sa; }
	com_variant& operator*() const	{ return *(com_variant*)pvarVal; }
	either	operator&()				{ return {this}; }
	operator const VARIANT&()const	{ return *(VARIANT*)this; }

	operator SAFEARRAY*()	const	{ return V_ARRAY(this);	}
	operator BSTR()			const	{ return V_BSTR(this);	}
//	operator com_string&()	const	{ return (com_string&)bstrVal;	}
	operator int()			const	{ return V_I4(this);	}
	operator unsigned int()	const	{ return V_UI4(this);	}
	operator int64()		const	{ return hVal.QuadPart;	}
	operator uint64()		const	{ return uhVal.QuadPart;}
	operator float()		const	{ return V_R4(this);	}
	operator double()		const	{ return V_R8(this);	}
	operator PROPVARIANT*()	const	{ return pvarVal;		}
	operator VARIANT*()		const	{ return (VARIANT*)pvarVal; }
	operator IDispatch*()	const	{ return pdispVal;		}
	operator IUnknown*()	const	{ return punkVal;		}

	template<typename T> bool is()	const { return com_vartype<T>::value == vt; }

	template<typename T2> HRESULT	query(com_ptr<T2> &p, const GUID &guid)	const	{ return iso::query(punkVal, &p, guid); }
	template<typename T2> HRESULT	query(com_ptr<T2> &p)					const	{ return iso::query(punkVal, &p); }
	template<typename T2> com_ptr<T2>	query()								const	{ return _query<T2>(punkVal); }

	template<typename T> T	get_number(const T &def = T())	const;
	template<typename T> T	get(const T &def)	const	{ if (vt == com_vartype<T>::value) return *this; return def; }
	template<typename T> T	get()				const	{ return get(T()); }

	template<>	bool		get<bool>()			const	{ return V_BOOL	(this) == VARIANT_TRUE; }
	template<>	int8		get<int8>()			const	{ return V_I1	(this); }
	template<>	uint8		get<uint8>()		const	{ return V_UI1	(this); }
	template<>	int16		get<int16>()		const	{ return V_I2	(this); }
	template<>	uint16		get<uint16>()		const	{ return V_UI2	(this); }
	template<>	CY			get<CY>()			const	{ return V_CY	(this); }
	template<>	DECIMAL		get<DECIMAL>()		const	{ return V_DECIMAL(this); }

	template<typename C> friend accum<C> &operator<<(accum<C> &a, const com_variant &v);
};

template<typename T> T com_variant::get_number(const T &def) const {
	switch (type()) {
		case VT_BOOL:	return T(V_BOOL(this));
		case VT_I1:		return T(V_I1(this));
		case VT_UI1:	return T(V_UI1(this));
		case VT_I2:		return T(V_I2(this));
		case VT_UI2:	return T(V_UI2(this));
		case VT_I4:		return T(V_I4(this));
		case VT_UI4:	return T(V_UI4(this));
		case VT_I8:		return T(hVal.QuadPart);
		case VT_UI8:	return T(uhVal.QuadPart);
		case VT_R4:		return T(V_R4(this));
		case VT_R8:		return T(V_R8(this));
		default:		return def;
	}
};

//-----------------------------------------------------------------------------
//	class com_safearray
//-----------------------------------------------------------------------------
template<typename T> class com_quickarray {
protected:
	SAFEARRAY	*sa;
public:
	com_quickarray(SAFEARRAY *sa)	: sa(sa)	{}
	com_quickarray(VARIANT &&v)		: sa(v)		{ v.vt = VT_EMPTY; }
	~com_quickarray()					{ SafeArrayDestroy(sa); }

	typedef	T		*iterator;
	operator T*()				const	{ return (T*)sa->pvData;	}
	int		dim()				const	{ return sa->cDims; }
	int		ubound(int d = 0)	const	{ return sa->rgsabound[d].lLbound; }
	int		lbound(int d = 0)	const	{ return sa->rgsabound[d].lLbound + sa->rgsabound[d].cElements - 1; }
	int		count(int d = 0)	const	{ return sa->rgsabound[d].cElements; }

	iterator	begin()			const	{ return (T*)sa->pvData;	}
	iterator	end()			const	{ return begin() + count(); }
};

class com_array_base {
protected:
	SAFEARRAY	*sa;

	template<typename T> struct _element {
		SAFEARRAY	*sa;
		LONG		i;
		_element(SAFEARRAY *sa, int i) : sa(sa), i(i)	{}
		void		put(const T &v)	{ SafeArrayPutElement(sa, &i, (void*)&v); }
		T			get() const		{ T v; SafeArrayGetElement(sa, const_cast<LONG*>(&i), get_ptr(v)); return v; }
	};

	template<typename E> struct _iterator {
		SAFEARRAY	*sa;
		LONG		i;
		_iterator(SAFEARRAY *_sa, int _i) : sa(_sa), i(_i)	{}
		_iterator&	operator++()							{ ++i; return *this; }
		_iterator&	operator--()							{ --i; return *this; }
		_iterator	operator++(int)							{ ++i; return _iterator(sa, i - 1); }
		_iterator	operator--(int)							{ --i; return _iterator(sa, i + 1); }
		bool		operator==(const _iterator &b)	const	{ return i == b.i; }
		bool		operator!=(const _iterator &b)	const	{ return i != b.i; }
		E			operator*()						const	{ return E(sa, i); }
		E			operator->()					const	{ return E(sa, i); }
	};

	template<typename T> class _data {
		SAFEARRAY	*sa;
		void		*p;
	public:
		_data(SAFEARRAY *_sa) : sa(_sa)	{ SafeArrayAccessData(sa, &p);	}
		~_data()						{ SafeArrayUnaccessData(sa);	}
		operator T*() const				{ return (T*)p; }
	};

	static SAFEARRAY *create(VARTYPE vt, int d0) {
		SAFEARRAYBOUND	bounds[1] = {
			{d0, 0}
		};
		return SafeArrayCreate(vt, 1, bounds);
	}
	static SAFEARRAY *create(VARTYPE vt, int d0, int d1) {
		SAFEARRAYBOUND	bounds[2];
		bounds[0].lLbound	= 0;
		bounds[0].cElements	= d0;
		bounds[1].lLbound	= 0;
		bounds[1].cElements	= d1;
		return SafeArrayCreate(vt, 2, bounds);
	}

	void	set(SAFEARRAY *_sa) {
		if (sa)
			SafeArrayDestroy(sa);
		sa = _sa;
	}
	void	set(const VARIANT &v) {
		set(V_ARRAY(&v));
		const_cast<VARIANT&>(v).vt = VT_EMPTY;
	}
	void	set(const PROPVARIANT &v) {
		set(V_ARRAY(&v));
		const_cast<PROPVARIANT&>(v).vt = VT_EMPTY;
	}

	~com_array_base()					{ SafeArrayDestroy(sa); }
public:
	com_array_base()					: sa(0)		{}
	com_array_base(SAFEARRAY *sa)		: sa(sa)	{}
	com_array_base(VARIANT &&v)			: sa(V_ARRAY(&v))	{ v.vt = VT_EMPTY; }
	com_array_base(PROPVARIANT &&v)		: sa(V_ARRAY(&v))	{ v.vt = VT_EMPTY; }

	void	operator=(SAFEARRAY *_sa)		{ set(_sa); }
	void	operator=(const VARIANT &v)		{ set(v);	}
	void	operator=(const PROPVARIANT &v) { set(v);	}

	SAFEARRAY*	detach()				{ return exchange(sa, nullptr); }
	operator SAFEARRAY*()		const	{ return sa; }
	int		dim()				const	{ return SafeArrayGetDim(sa); }
	int		ubound(int d = 0)	const	{ LONG v; return SUCCEEDED(SafeArrayGetUBound(sa, d + 1, &v)) ? v : 0; }
	int		lbound(int d = 0)	const	{ LONG v; return SUCCEEDED(SafeArrayGetLBound(sa, d + 1, &v)) ? v : 0; }
	int		count(int d = 0)	const	{ return ubound(d) - lbound(d) + 1;			}
	bool	lock()				const	{ return SUCCEEDED(SafeArrayLock(sa));		}
	bool	unlock()			const	{ return SUCCEEDED(SafeArrayUnlock(sa));	}
};

class com_safearray : public com_array_base {
	class element : _element<com_variant> {
	public:
		element(SAFEARRAY *sa, int i) : _element<com_variant>(sa, i)	{}
		template<typename T> void operator=(const T &t)	{ this->put(t); }
		template<typename T> operator T() const			{ return this->get(); }
	};
public:
	typedef _iterator<element> iterator;
	using com_array_base::com_array_base;
	using com_array_base::operator=;

	com_safearray(VARTYPE t, int d0)			: com_array_base(create(t, d0))		{}
	com_safearray(VARTYPE t, int d0, int d1)	: com_array_base(create(t, d0, d1))	{}

	element	operator[](int i)	const	{ return element(sa, i); }
	iterator	begin()			const	{ return iterator(sa, lbound()); }
	iterator	end()			const	{ return iterator(sa, ubound() + 1); }
	_data<com_variant>	data()			{ return sa; }
};

template<typename I> class com_interface_array : public com_array_base {
public:
	struct element : _element<com_variant> {
		element(SAFEARRAY *sa, int i) : _element<com_variant>(sa, i)	{}
		void operator=(const I *t)	{ this->put(t); }
		operator I*()		const	{ return this->get().template query<I>(); }
		I*	operator->()	const	{ return *this; }
	};
	typedef _iterator<element> iterator;
	using com_array_base::com_array_base;
	using com_array_base::operator=;

	I*		operator[](int i)	const	{ i += lbound(); return i <= ubound() ? (I*)element(sa, i) : 0; }
	iterator	begin()			const	{ return iterator(sa, lbound()); }
	iterator	end()			const	{ return iterator(sa, ubound() + 1); }
	_data<com_variant>	data()			{ return sa; }
};

template<typename T> class com_array : public com_array_base {
	struct element : _element<T> {
		element(SAFEARRAY *sa, int i) : _element<T>(sa, i)	{}
		void operator=(const T &t)	{ this->put(t); }
		operator T() const			{ return this->get(); }
	};
public:
	typedef _iterator<element> iterator;
	using com_array_base::com_array_base;
	using com_array_base::operator=;

	com_array(const T *p, int N)	: com_array_base(create(com_vartype<T>::value, N))				{ memcpy(data(), p, N * sizeof(T)); }
	template<int N> com_array(const T (&v)[N]) : com_array_base(create(com_vartype<T>::value, N))	{ memcpy(data(), v, sizeof(v)); }

	element	operator[](int i)	const	{ return element(sa, i); }
	iterator	begin()			const	{ return iterator(sa, lbound()); }
	iterator	end()			const	{ return iterator(sa, ubound() + 1); }
	_data<T>	data()					{ return sa; }
};

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

//-----------------------------------------------------------------------------
//	IDispatch
//-----------------------------------------------------------------------------

class _IDispatch {
protected:
	const GUID& iid;
	const GUID* libid;
	WORD		major;
	WORD		minor;

	com_ptr<ITypeInfo>	info;

	struct entry {
		com_string	name;
		size_t		len;
		DISPID		id;
		entry() : len(0), id(DISPID_UNKNOWN){}
	};
	entry	*map;
	int		count;

	_IDispatch(const IID &iid, WORD major, WORD minor) : iid(iid), libid(0), major(major), minor(minor), map(0), count(0) {}
	bool	Init(LCID loc);
	bool	Check(LCID loc) { return (info && map) || Init(loc); }

	HRESULT	GetTypeInfoCount(UINT *n) {
		if (!n)
			return E_POINTER;
		*n = 1;
		return S_OK;
	}
	HRESULT	GetTypeInfo(UINT i, LCID loc, ITypeInfo **_info) {
		if (i != 0)
			return DISP_E_BADINDEX;
		*_info = info;
		info->AddRef();
		return S_OK;
	}
	HRESULT	GetIDsOfNames(REFIID riid, BSTR *names, UINT num, LCID loc, DISPID *dispid);
};

template<class T, WORD _major = 1, WORD _minor = 0> class TDispatch : public com<T>, _IDispatch {
public:
	TDispatch() : _IDispatch(uuidof<T>(), _major, _minor) {}
	HRESULT	STDMETHODCALLTYPE GetTypeInfoCount(UINT *n) {
		return _IDispatch::GetTypeInfoCount(n);
	}
	HRESULT	STDMETHODCALLTYPE GetTypeInfo(UINT i, LCID loc, ITypeInfo **info) {
		return _IDispatch::GetTypeInfo(i, loc, info);
	}
	HRESULT	STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, BSTR *names, UINT num, LCID loc, DISPID *dispid) {
		return _IDispatch::GetIDsOfNames(riid, names, num, loc, dispid);
	}
	HRESULT	STDMETHODCALLTYPE Invoke(DISPID dispid, REFIID riid, LCID loc, WORD flags, DISPPARAMS* params, VARIANT* result, EXCEPINFO* excepinfo, UINT* err) {
		return Check(loc)
			? info->Invoke(this, dispid, flags, params, result, excepinfo, err)
			: E_FAIL;
	}
};

//-----------------------------------------------------------------------------
//	com_error
//-----------------------------------------------------------------------------

class com_error {
	enum {
		WCODE_HRESULT_FIRST = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x200),
		WCODE_HRESULT_LAST	= MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF + 1, 0) - 1
	};

	HRESULT					hr;
	com_ptr2<IErrorInfo>	info;
	mutable char			*msg;
public:
	static HRESULT	WCodeToHRESULT(WORD w)		{ return w >= 0xFE00 ? WCODE_HRESULT_LAST : WCODE_HRESULT_FIRST + w; }
	static WORD		HRESULTToWCode(HRESULT hr)	{ return hr >= WCODE_HRESULT_FIRST && hr <= WCODE_HRESULT_LAST ? WORD(hr - WCODE_HRESULT_FIRST)	: 0; }

	com_error(HRESULT hr, IErrorInfo *info = 0)	: hr(hr), info(info), msg(0) {}
	com_error(const com_error &b)				: hr(b.hr), info(b.info), msg(0) {}
	~com_error()								{ if (msg) LocalFree((HLOCAL)msg); }

	operator	HRESULT()	const	{ return hr; }
	WORD		WCode()		const	{ return HRESULTToWCode(hr); }
	IErrorInfo*	Info()		const	{ return info;	}

	// IErrorInfo accessors
	com_string	Description()	const { BSTR b = 0; if (info) info->GetDescription(&b); return b; }
	DWORD		HelpContext()	const { DWORD c = 0; if (info) info->GetHelpContext(&c); return c; }
	com_string	HelpFile()		const { BSTR b = 0; if (info) info->GetHelpFile(&b); return b; }
	com_string	Source()		const { BSTR b = 0; if (info) info->GetSource(&b); return b; }
	GUID		GUID()			const { ::GUID g = GUID_NULL; if (info) info->GetGUID(&g); return g; }

	// FormatMessage accessors
	const char* Message() {
		if (!msg) {
			FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
				0, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&msg, 0, NULL
			);
			if (msg) {
				size_t len = strlen(msg);
				if (len > 1 && msg[len - 1] == '\n') {
					msg[len - 1] = 0;
					if (msg[len - 2] == '\r')
						msg[len - 2] = 0;
				}
			} else if (msg = (char*)LocalAlloc(0, 32)) {
				if (WORD w = WCode())
					_format(msg, 32, "IDispatch error #%d", w);
				else
					_format(msg, 32, "Unknown error 0x%0lX", hr);
			}
		}
		return msg;
	}
};
#endif
//#endif // CLANG

}//namespace iso

#endif	//_OLEAUTO_H_
#endif // COM_H
