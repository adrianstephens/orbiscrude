#undef min
#undef max

#ifndef DEFS_H
#define DEFS_H

#include "defs_base.h"
#include "defs_int.h"
#include "defs_endian.h"
#include "iterator.h"
#include "stdarg.h"
#include <initializer_list>

#undef NAN

const void *memmem(const void *l, size_t l_len, const void *s, size_t s_len);

#define alloc_auto(T,N)	(T*)alloca((N) * sizeof(T))

#ifndef _WIN32
struct GUID {
	iso::uint32	Data1;
	iso::uint16	Data2;
	iso::uint16	Data3;
	iso::uint8	Data4[8];
};
inline int compare(const GUID &a, const GUID &b)		{ return memcmp(&a, &b, sizeof(GUID)); }
inline bool operator==(const GUID &a, const GUID &b)	{ return compare(a, b) == 0; }
inline bool operator!=(const GUID &a, const GUID &b)	{ return compare(a, b) != 0; }
#endif

namespace iso {

using std::initializer_list;

template<typename T> using is_reader_t		= exists_t<decltype(&noref_t<T>::readbuff), bool>;
template<typename T> using is_writer_t		= exists_t<decltype(&noref_t<T>::writebuff), bool>;
template<typename T> using is_allocator_t	= exists_t<decltype(declval<T>().alloc(1,1)), void*>;

//-----------------------------------------------------------------------------
//	alignment
//-----------------------------------------------------------------------------

template<int N> struct _aligner {
	inline void*	operator new(size_t size)			{ return aligned_alloc(size, N);	}
	inline void		operator delete(void *p)			{ aligned_free(p);					}
	inline void*	operator new[](size_t size)			{ return aligned_alloc(size, N);	}
	inline void		operator delete[](void *p)			{ aligned_free(p);					}
	inline void*	operator new(size_t size, const placement& p)	{ return p.p; }
	inline void*	operator new[](size_t size, const placement& p)	{ return p.p; }
	template<typename A> void*	operator new(size_t size, A &a, is_allocator_t<A> = 0)		{ return a.alloc(size, N); }
	template<typename A> void*	operator new[](size_t size, A &a, is_allocator_t<A> = 0)	{ return a.alloc(size, N); }
};

template<int N> struct aligner	: _aligner<N> {};

#if defined __GNUC__ || defined __MWERKS__
template<> struct aligner<16>	: _aligner<16> {} __attribute__((__aligned__(16)));
template<> struct aligner<128>	: _aligner<128> {} __attribute__((__aligned__(128)));
#else
template<> struct __declspec(align(16))		aligner<16>		: _aligner<16> {};
template<> struct __declspec(align(128))	aligner<128>	: _aligner<128> {};
#endif

template<typename T, int N> struct aligned : public T, public aligner<N> {
	template<typename T2> T	&operator=(const T2 &t)	{ return *(T*)this = t; }
};

//-----------------------------------------------------------------------------
//	numeric traits
//-----------------------------------------------------------------------------

template<> struct num_traits<float> : float_traits<23, 8, 1> {
	static constexpr float	min()		{ return -max(); }
	static constexpr float	max()		{ return 3.40282347e+38f; }//force_cast<float>(0x7f7fffffu); }
	template<typename T2> static constexpr float cast(const T2 &t)					{ return float(t); }
	template<typename T2> static constexpr float cast(const T_swap_endian<T2> &t)	{ return float(T2(t)); }
};

template<> struct num_traits<double> : float_traits<52, 11, 1> {
	static constexpr double	min()		{ return -max(); }
	static constexpr double	max()		{ return 1.7976931348623158e+308; }//force_cast<double>(0x7fefffffffffffffull); }
	template<typename T2> static constexpr double cast(const T2 &t)	{ return double(t); }
};
template<> struct num_traits<long double> : num_traits<double> {};

template<typename TR, typename V = void>	struct expanded_num_traits : TR {
	static const int mantissa_bits = TR::bits, exponent_bits = 0;
};
template<typename TR>	struct expanded_num_traits<TR, enable_if_t<TR::has_frac>> : TR {
	static const int mantissa_bits = TR::bits, exponent_bits = 0;
	static const bool is_float = true;
};
template<typename TR>	struct expanded_num_traits<TR, enable_if_t<TR::is_float>> : TR {};

template<typename T0, typename...T> struct max_num_traits {
	typedef max_num_traits<T0>		head;
	typedef max_num_traits<T...>	tail;
	static const int	exponent_bits	= max(head::exponent_bits, tail::exponent_bits);
	static const int	mantissa_bits	= max(head::mantissa_bits, tail::mantissa_bits);
	static const bool	is_signed		= head::is_signed || tail::is_signed;
	static const bool	is_float		= head::is_float || tail::is_float;
};
template<typename T> struct max_num_traits<T> : expanded_num_traits<num_traits<T>> {};

template<bool is_float, bool is_signed, int mantissa_bits, int exponent_bits> using type_from_bits
= if_t<is_float,
	if_t<(mantissa_bits > 23 || exponent_bits > 8), double, float>,
	int_t<(mantissa_bits + 7) / 8, is_signed>
>;

template<typename TR>	using type_from_traits	= type_from_bits<TR::is_float, TR::is_signed, TR::mantissa_bits, TR::exponent_bits>;
template<typename...T>	using promoted_type		= type_from_traits<max_num_traits<T...>>;

//-----------------------------------------------------------------------------
//	holders
//-----------------------------------------------------------------------------

template<typename T> struct holder {
	T	t;
	holder()								: t()	{}
	template<typename...P> holder(P&&...p)	: t(forward<P>(p)...) {}

	operator	const T&()			const	{ return t; }
	operator	T&()						{ return t; }
	T&			operator->()				{ return t; }
	const T&	operator->()		const	{ return t; }
	T*			operator&()					{ return &t; }
	const T*	operator&()			const	{ return &t; }
	template<typename T2> holder&	operator= (T2 &&b)	{ t  = forward<T2>(b); return *this; }
	template<typename T2> holder&	operator+=(T2 &&b)	{ t += forward<T2>(b); return *this; }
	template<typename T2> holder&	operator-=(T2 &&b)	{ t -= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator*=(T2 &&b)	{ t *= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator/=(T2 &&b)	{ t /= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator|=(T2 &&b)	{ t |= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator&=(T2 &&b)	{ t &= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator^=(T2 &&b)	{ t ^= forward<T2>(b); return *this; }
	T&			get()						{ return t; }
	const T&	get()				const	{ return t; }
	friend T&		put(holder &p)			{ return p; }
	friend const T&	get(const holder &p)	{ return p; }
};

template<typename T> struct holder<T&> {
	T	*t;
	holder()								: t(0)	{}
	holder(T& t)							: t(&t) {}

	operator	const T&()			const	{ return *t; }
	operator	T&()						{ return *t; }
	T*			operator->()				{ return t; }
	const T&	operator->()		const	{ return *t; }
	T*			operator&()					{ return t; }
	const T*	operator&()			const	{ return t; }
	template<typename T2> holder&	operator= (T2 &&b)	{ *t  = forward<T2>(b); return *this; }
	template<typename T2> holder&	operator+=(T2 &&b)	{ *t += forward<T2>(b); return *this; }
	template<typename T2> holder&	operator-=(T2 &&b)	{ *t -= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator*=(T2 &&b)	{ *t *= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator/=(T2 &&b)	{ *t /= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator|=(T2 &&b)	{ *t |= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator&=(T2 &&b)	{ *t &= forward<T2>(b); return *this; }
	template<typename T2> holder&	operator^=(T2 &&b)	{ *t ^= forward<T2>(b); return *this; }
	T&			get()						{ return *t; }
	const T&	get()				const	{ return *t; }
	friend T&		put(holder &p)			{ return p; }
	friend const T&	get(const holder &p)	{ return p; }
};

template<typename T, int N> struct holder<T[N]> {
	typedef T	A[N];
	A			t;
	holder()								{}
	holder(const A &_t)						{ memcpy(t, &t, sizeof(A)); }
	operator	const A&()			const	{ return t; }
	operator	A&()						{ return t; }
	A*			operator&()					{ return &t; }
	A&			get()						{ return t; }
	const A&	get()				const	{ return t; }
};

template<typename T> struct holder<T*> {
	T			*t;
	holder(T *t = 0) : t(t)	{}
	operator	const T*()			const	{ return t; }
	operator	T*&()						{ return t; }
	T*			operator->()				{ return t; }
	const T*	operator->()		const	{ return t; }
	T**			operator&()					{ return &t; }
	T* const*	operator&()			const	{ return &t; }
	T&			operator*()			const	{ return *t; }
	holder&		operator=(T *t2)			{ t = t2; return *this; }
	holder&		operator+=(intptr_t b)		{ t += b; return *this;	}
	holder&		operator-=(intptr_t b)		{ t -= b; return *this;	}
	T*&			get()						{ return t; }
	T*			get()				const	{ return t; }
	friend T*		put(holder &p)			{ return p; }
	friend const T*	get(const holder &p)	{ return p; }
};

template<> struct holder<void*> {
	void		*t;
	holder(void *t = 0) : t(t)	{}
	void		operator=(void *t2)			{ t = t2; }
	operator	void*()				const	{ return t; }
	operator	void*&()					{ return t; }
	void**		operator&()					{ return &t; }
	void* const* operator&()		const	{ return &t; }
	void*&		get()						{ return t; }
	void*		get()				const	{ return t; }
	friend void*		put(holder &p)		{ return p; }
	friend const void*	get(const holder &p){ return p; }
};

template<> struct holder<const void*> {
	const void		*t;
	holder(const void *t = 0) : t(t)	{}
	void		operator=(const void *t2)	{ t = t2; }
	operator	const void*()		const	{ return t; }
	operator	const void*&()				{ return t; }
	const void**		operator&()			{ return &t; }
	const void* const*	operator&()	const	{ return &t; }
	const void*&		get()				{ return t; }
	const void*			get()		const	{ return t; }
	friend const void*	put(holder &p)		{ return p; }
	friend const void*	get(const holder &p){ return p; }
};

template<> struct holder<void> {
	char		_;
	void*		operator&()					{ return this; }
	const void*	operator&()			const	{ return this; }
};

template<typename T> struct num_traits<holder<T> > : num_traits<T> {};
template<typename T> struct T_swap_endian_type<holder<T> >	: T_type<T_swap_endian<T>> {};

template<typename T>		struct T_inheritable			{ typedef T					type; };
template<typename T, int N>	struct T_inheritable<T[N]>		{ typedef holder<T[N]>		type; };
template<typename T>		struct T_inheritable<T*>		{ typedef holder<T*>		type; };
template<>					struct T_inheritable<void>		{ typedef holder<void>		type; };
template<>					struct T_inheritable<bool>		{ typedef holder<bool>		type; };
template<>					struct T_inheritable<uint8>		{ typedef holder<uint8>		type; };
template<>					struct T_inheritable<uint16>	{ typedef holder<uint16>	type; };
template<>					struct T_inheritable<uint32>	{ typedef holder<uint32>	type; };
template<>					struct T_inheritable<int8>		{ typedef holder<int8>		type; };
template<>					struct T_inheritable<int16>		{ typedef holder<int16>		type; };
template<>					struct T_inheritable<int32>		{ typedef holder<int32>		type; };
template<>					struct T_inheritable<float>		{ typedef holder<float>		type; };
template<>					struct T_inheritable<double>	{ typedef holder<double>	type; };
#if USE_LONG
template<>					struct T_inheritable<ulong>		{ typedef holder<ulong>		type; };
template<>					struct T_inheritable<long>		{ typedef holder<long>		type; };
#endif
#if USE_64BITREGS
template<>					struct T_inheritable<uint64>	{ typedef holder<uint64>	type; };
template<>					struct T_inheritable<int64>		{ typedef holder<int64>		type; };
#endif

template<typename T>	using inheritable_t	= typename T_inheritable<T>::type;

template<typename T, typename TAG> struct tagged : holder<T> {
	tagged() {}
	tagged(const T& t) : holder<T>(t) {}
};

template<typename T> struct T_hold_ref		: T_type<T> {};
template<typename T> struct T_hold_ref<T&>	: T_type<holder<T&>> {};
template<typename T>	using hold_ref_t	= typename T_hold_ref<T>::type;

template<typename R, typename T> struct _read_as : holder<T> {
	using holder<T>::holder;
	friend const T&	get(const _read_as &p)	{ return p; }
};

//-----------------------------------------------------------------------------
//	constexpr
//-----------------------------------------------------------------------------

template<size_t N> using byte_array = meta::array<uint8, N>;

constexpr auto		le_byte_array(uint16 v)	{ return byte_array<2>{{ uint8(v), uint8(v >> 8) }}; }
constexpr auto		le_byte_array(uint32 v)	{ return byte_array<4>{{ uint8(v), uint8(v >> 8), uint8(v >> 16), uint8(v >> 24) }}; }
constexpr auto		le_byte_array(uint64 v)	{ return byte_array<8>{{ uint8(v), uint8(v >> 8), uint8(v >> 16), uint8(v >> 24), uint8(v >> 32), uint8(v >> 40), uint8(v >> 48), uint8(v >> 56) }}; }

constexpr auto		be_byte_array(uint16 v)	{ return byte_array<2>{{ uint8(v >> 8), uint8(v) }}; }
constexpr auto		be_byte_array(uint32 v)	{ return byte_array<4>{{ uint8(v >> 24), uint8(v >> 16), uint8(v >> 8), uint8(v) }}; }
constexpr auto		be_byte_array(uint64 v)	{ return byte_array<8>{{ uint8(v >> 56), uint8(v >> 48), uint8(v >> 40), uint8(v >> 32), uint8(v >> 24), uint8(v >> 16), uint8(v >> 8), uint8(v) }}; }

#if USE_LONG
constexpr auto		le_byte_array(ulong v)	{ return byte_array<4>{{ uint8(v), uint8(v >> 8), uint8(v >> 16), uint8(v >> 24) }}; }
constexpr auto		be_byte_array(ulong v)	{ return byte_array<4>{{ uint8(v >> 24), uint8(v >> 16), uint8(v >> 8), uint8(v) }}; }
#endif

constexpr uint16	bytes_to_u2(uint8 a, uint8 b)					{ return (uint32(b) << 8) | uint32(a); }
constexpr uint32	bytes_to_u4(uint8 a, uint8 b, uint8 c, uint8 d)	{ return (uint32(d) << 24) | (uint32(c) << 16) | (uint32(b) << 8) | uint32(a); }
constexpr uint64	bytes_to_u8(uint8 a, uint8 b, uint8 c, uint8 d, uint8 e, uint8 f, uint8 g, uint8 h)		{ return bytes_to_u4(a, b, c, d) | (uint64(bytes_to_u4(e, f, g, h)) << 32); }

constexpr auto		to_byte_array(const GUID &v)					{ return be_byte_array(v.Data1) + be_byte_array(v.Data2) + be_byte_array(v.Data3) + meta::make_array(v.Data4); }
//template<typename T> constexpr auto	to_byte_array(const T &v)	{ return meta::make_array<sizeof(T)>((const uint8*)&v); }

//implemented in platform.cpp
uint64	random_seed();

template<template<typename> class T> struct num_types {
	template<typename U> using type = typename T<U>::type;
	typedef	type<iso::uint8>	uint8;
	typedef	type<iso::uint16>	uint16;
	typedef	type<iso::uint32>	uint32;
	typedef	type<iso::uint64>	uint64;
	typedef	type<iso::int8>		int8;
	typedef	type<iso::int16>	int16;
	typedef	type<iso::int32>	int32;
	typedef	type<iso::int64>	int64;
	typedef	type<iso::float32>	float32;
	typedef	type<iso::float64>	float64;
	typedef	type<iso::xint16>	xint16;
	typedef	type<iso::xint32>	xint32;
	typedef	type<iso::xint64>	xint64;
};

#if 1
template<template<typename> class T, class B> struct num_types2 {
	template<typename U> using type = typename T<typename B::template type<U>>::type;
	typedef	type<iso::uint8>	uint8;
	typedef	type<iso::uint16>	uint16;
	typedef	type<iso::uint32>	uint32;
	typedef	type<iso::uint64>	uint64;
	typedef	type<iso::int8>		int8;
	typedef	type<iso::int16>	int16;
	typedef	type<iso::int32>	int32;
	typedef	type<iso::int64>	int64;
	typedef	type<iso::float32>	float32;
	typedef	type<iso::float64>	float64;
	typedef	type<iso::xint16>	xint16;
	typedef	type<iso::xint32>	xint32;
	typedef	type<iso::xint64>	xint64;
};

template<typename T, size_t A> struct _packed_type	: T_type<packed<T>> {};
template<typename T> struct _packed_type<T, 1>		: T_type<T> {};
template<typename T> struct packed_type	: _packed_type<T, alignof(T)> {};
template<typename T> struct packed_types : num_types2<packed_type, T> {};

#else

template<template<typename> class T, template<typename> class U> struct T_type_T {
	template<typename V> struct type : T<U<V> > {};
};

template<typename T> struct packed_type : T_type<packed<T>> {};
template<template<typename> class T> struct packed_types : num_types<T_type_T<packed_type,T> >::type> {};

#endif

typedef num_types<T_type>	native;

#ifdef	ISO_BIGENDIAN
template<typename T>	using	bigendian		= T;
template<typename T>	using	littleendian	= typename T_constructable_swap_endian_type<T>::type;
typedef native							bigendian_types, bigendian0;
typedef num_types<T_constructable_swap_endian_type>	littleendian_types;
typedef num_types<T_swap_endian_type>	littleendian0;
#else
template<typename T>	using	littleendian	= T;
template<typename T>	using	bigendian		= typename T_constructable_swap_endian_type<T>::type;
typedef native							littleendian_types, littleendian0;
typedef num_types<T_constructable_swap_endian_type>	bigendian_types;
typedef num_types<T_swap_endian_type>	bigendian0;
#endif

template<bool be> struct endian_types;
template<> struct endian_types<false>	: littleendian_types	{};
template<> struct endian_types<true>	: bigendian_types		{};

template<bool be> struct endian_types0;
template<> struct endian_types0<false>	: littleendian0	{};
template<> struct endian_types0<true>	: bigendian0	{};

template<int B, typename T> class T_swap_endian<baseint<B, T> > : public T_swap_endian<T> {
public:
	using T_swap_endian<T>::operator=;
	using T_swap_endian<T>::T_swap_endian;
	friend T	put(T_swap_endian &a)		{ return a; }
	friend T	get(const T_swap_endian &a)	{ return a; }
};

template<typename T, bool S> struct T_signed_native	: T_signed<native_endian_t<T>, S> {};

//-----------------------------------------------------------------------------
//	calc_size - dynamic sizeof (for, e.g. trailing_array)
//-----------------------------------------------------------------------------

template<typename T> struct calc_size_s {
	template<typename...P> static constexpr size_t f(P&&...) { return sizeof(T); }
};
template<typename T, typename...P> constexpr size_t calc_size(P&&...p) {
	return calc_size_s<T>::f(forward<P>(p)...);
}
template<typename T> constexpr size_t calc_size() {
	return sizeof(T);
}

//-----------------------------------------------------------------------------
//	operations
//-----------------------------------------------------------------------------

// arthmetic
struct op_add			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) + forward<B>(b); } };
struct op_sub			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) - forward<B>(b); } };
struct op_mul			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) * forward<B>(b); } };
struct op_div			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) / forward<B>(b); } };
struct op_rdiv			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(b) / forward<B>(a); } };
struct op_mod			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) % forward<B>(b); } };
struct op_and			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) & forward<B>(b); } };
struct op_or			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) | forward<B>(b); } };
struct op_xor			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) ^ forward<B>(b); } };
struct op_shl			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) << forward<B>(b); } };
struct op_shr			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return forward<A>(a) >> forward<B>(b); } };
struct op_rot			{ template<typename A, typename B> auto operator()(A &&a, B &&b) const { return rotate_bits(a, b); } };
struct op_neg			{ template<typename A> auto operator()(A &&a) const { return -forward<A>(a); } };
struct op_not			{ template<typename A> auto operator()(A &&a) const { return ~forward<A>(a); } };
//struct op_recip		{ template<typename A> A operator()(const A &a) const { return reciprocal(a); } };

struct op_min			{ template<typename A, typename B>	A operator()(const A &a, const B &b) const { return min(a, b); } };
struct op_max			{ template<typename A, typename B>	A operator()(const A &a, const B &b) const { return max(a, b); } };

// assignment
struct op_add_eq		{ template<typename A, typename B> A& operator()(A &a, B &&b) const { return a += forward<B>(b); } };
struct op_sub_eq		{ template<typename A, typename B> A& operator()(A &a, B &&b) const { return a -= forward<B>(b); } };
struct op_mul_eq		{ template<typename A, typename B> A& operator()(A &a, B &&b) const { return a *= forward<B>(b); } };
struct op_div_eq		{ template<typename A, typename B> A& operator()(A &a, B &&b) const { return a /= forward<B>(b); } };
struct op_mod_eq		{ template<typename A, typename B> A& operator()(A &a, B &&b) const { return a %= forward<B>(b); } };
struct op_and_eq		{ template<typename A, typename B> A& operator()(A &a, B &&b) const { return a &= forward<B>(b); } };
struct op_or_eq			{ template<typename A, typename B> A& operator()(A &a, B &&b) const { return a |= forward<B>(b); } };
struct op_xor_eq		{ template<typename A, typename B> A& operator()(A &a, B &&b) const { return a ^= forward<B>(b); } };

// predicates
struct equal_to			{ template<typename A, typename B> bool operator()(A &&a, B &&b) const { return forward<A>(a) == forward<B>(b); } typedef struct not_equal_to	not_t; };
struct not_equal_to		{ template<typename A, typename B> bool operator()(A &&a, B &&b) const { return forward<A>(a) != forward<B>(b); } typedef struct equal_to		not_t; };
struct less_equal		{ template<typename A, typename B> bool operator()(A &&a, B &&b) const { return forward<A>(a) <= forward<B>(b); } typedef struct greater		not_t; };
struct greater			{ template<typename A, typename B> bool operator()(A &&a, B &&b) const { return forward<A>(a)  > forward<B>(b); } typedef struct less_equal		not_t; };
struct less				{ template<typename A, typename B> bool operator()(A &&a, B &&b) const { return forward<A>(a)  < forward<B>(b); } typedef struct greater_equal	not_t; };
struct greater_equal	{ template<typename A, typename B> bool operator()(A &&a, B &&b) const { return forward<A>(a) >= forward<B>(b); } typedef struct less			not_t; };
struct equal_vec		{ template<typename T> inline bool operator()(const T &a, const T &b) const { return all(a == b); } typedef struct not_equal_vec		not_t; };
struct not_equal_vec	{ template<typename T> inline bool operator()(const T &a, const T &b) const { return any(a != b); } typedef struct equal_vec			not_t; };

struct op_deref			{ template<typename A> decltype(auto) operator()(A &&a) const { return *a; } };
struct op_set			{ template<typename A, typename B>	void operator()(const A &a, B &&b) const { assign(b, a); } };
struct op_rset			{ template<typename A, typename B>	void operator()(A &&a, const B &b) const { assign(a, b); } };
struct op_swap			{ template<typename A, typename B>	void operator()(A &a, B &b)		const { swap(a, b); } };

struct op_construct		{ template<typename A, typename...B>void operator()(A &a, B&&...b)	const { construct(a, forward<B>(b)...); } };
struct op_destruct		{ template<typename A>				void operator()(A &a)			const { destruct(a); } };
struct op_move			{ template<typename A, typename B>	void operator()(A &a, B &&b)	const { a = forward<B>(b); } };

// op_param - pick one parameter
template<int I> struct op_param { template<typename...P> auto	operator()(P&&... p) const { return PP_index<I>(forward<P>(p)...); } };

// op_field - read a structure field
template<typename S, typename T> struct op_field {
	T	S::*f;
	T&	operator()(S &s) const { return s.*f; }
	op_field(T S::*f) : f(f) {}
};
template<typename S, typename T> op_field<S, T> make_deferred_field(T S::*f) { return f; }

template<typename O1, typename O2> struct op_chain12 : O1, O2 {
	template<typename A, typename B> auto operator()(A &&a, B &&b) const { return O2::operator()((*(O1*)this)(forward<A>(a)), (*(O1*)this)(forward<B>(b))); }
};
template<typename O1, typename O2> struct op_chain21 : O1, O2 {
	template<typename A, typename B> auto operator()(A &&a, B &&b) const { return O2::operator()((*(O1*)this)(forward<A>(a)), forward<B>(b)); }
};

template<typename T> struct _not {
	T	t;
	_not(T &&t) : t(forward<T>(t)) {}
	template<typename...P> bool operator()(P&&... p) const { return !t(forward<P>(p)...); }
};

template<> struct _not<bool> {
	bool	t;
	_not(bool t = false) : t(t) {}
	operator bool() const { return !t; }
};

template<class T> _not<T>	make_not(T &&t)				{ return forward<T>(t); }
template<class T> const T&	make_not(const _not<T> &t)	{ return t.t; }

template<class T> const T&	operator!(const _not<T> &t)	{ return t.t; }
template<class T> const T&	operator~(const _not<T> &t)	{ return t.t; }

template<typename A, typename B> auto	operator&(A&& a, const _not<B>& b)				{ return forward<A>(a) - b; }
template<typename A, typename B> auto	operator&(const _not<A>& a, B&& b)				{ return forward<B>(b) - a; }

template<typename A, typename B> auto	operator&(const _not<A>& a, const _not<B>& b)	{ return ~(a | b); }
template<typename A, typename B> auto	operator|(const _not<A>& a, const _not<B>& b)	{ return ~(a & b); }
template<typename A, typename B> auto	operator^(const _not<A>& a, const _not<B>& b)	{ return a ^ b; }

template<class T> struct flipped {
	T	t;
	template<typename A, typename B> bool operator()(A &&a, B &&b) const { return t(forward<B>(b), forward<A>(a)); }
	flipped(T &&t) : t(forward<T>(t)) {}
};

template<class T> flipped<T> 		flip(T &&t) 					{ return forward<T>(t); }
template<class T> T 				flip(flipped<T> t)				{ return t.t; }
template<class T> _not<flipped<T>>	flip(_not<T> t)					{ return flip(t.t); }
template<class T> _not<T>			flip(_not<flipped<T>> t)		{ return t.t.t; }
template<class T> _not<flipped<T>>	operator!(flipped<T> t)			{ return t; }

template<typename F, typename A> struct op_bind_first {
	F	f;
	A	a;
	template<typename B> auto	operator()(B &&b) const { return f(a, forward<B>(b)); }
	op_bind_first(A &&a) : a(forward<A>(a))	{}
};

template<typename F, typename B> struct op_bind_second {
	F	f;
	B	b;
	template<typename A> auto	operator()(A &&a) const { return f(forward<A>(a), b); }
	op_bind_second(B &&b) : b(forward<B>(b))	{}
};


template<typename T, int S> struct shifted {
	typedef uint_t<sizeof(T) * 2>	T2;
	T	raw;
	constexpr operator T2() const	{ return T2(raw) << S; }
	T2	operator=(T2 t)				{ raw = T(t >> S); return t; }
};

template<typename OP, typename T, typename T2, T2 B, typename R = decltype(OP()(declval<T>(), B))> struct with_op {
	T	raw;
	constexpr operator R() const	{ return OP()(raw, B); }
};

//-----------------------------------------------------------------------------
//	compact - cram a type into less bits
//-----------------------------------------------------------------------------

template<typename T, int N> class compact {
	typedef native_endian_t<T>		T0;
	uintn<(N + 7) / 8, is_bigendian<T>()>	t;
public:
	compact()							{}
	constexpr compact(const T0 &t2)	: t(t2) {}
	constexpr T0		get()	const	{ return T0((uint_bits_t<N>)t); }
	constexpr operator	T0()	const	{ return get(); }
	T0			operator=(const T0 &t2)	{ t = t2; return t2; }
	T0			operator+=(const T0 &b)	{ return operator=(get() + b); }
	T0			operator-=(const T0 &b)	{ return operator=(get() - b); }
	T0			operator*=(const T0 &b)	{ return operator=(get() * b); }
	T0			operator/=(const T0 &b)	{ return operator=(get() / b); }
	T0			operator%=(const T0 &b)	{ return operator=(get() % b); }
	T0			operator&=(const T0 &b)	{ return operator=(get() & b); }
	T0			operator|=(const T0 &b)	{ return operator=(get() | b); }
	T0			operator<<=(int b)		{ return operator=(get() << b); }
	T0			operator>>=(int b)		{ return operator=(get() >> b); }

	template<class R, typename = is_reader_t<R>> compact(R &&r) { r.read(*this); }
};

typedef compact<bool, 8>	bool8;

template<typename T, int N> struct num_traits<compact<T, N> > : num_traits<T> {
	enum {bits = N};
};

template<typename T, int N>	constexpr uint32 BIT_COUNT<compact<T, N>>	= N;

//-----------------------------------------------------------------------------
//	spacer/space_for
//-----------------------------------------------------------------------------

template<int N, int A = 1>	struct spacer		{ uint_t<A> _[N]; };
template<int A>				struct spacer<0,A>	{};

template<typename T, bool OBJC = is_objc<T>> struct space_for : spacer<sizeof(T) / alignof(T), alignof(T)> {
	force_inline constexpr operator sized_placement() 	{ return {this, sizeof(T)}; }
	force_inline constexpr operator T*()				{ return (T*)this; }
	force_inline constexpr operator const T*() const	{ return (const T*)this; }
	force_inline constexpr T* operator->()				{ return (T*)this; }
	force_inline constexpr const T* operator->() const	{ return (const T*)this; }
};

#ifdef __OBJC__
template<typename T> struct space_for<T, true> {
	T	val;
	force_inline constexpr operator T*()	{ return &val; }
};
#endif

template<typename T, int N> struct space_for<T[N], false> : spacer<sizeof(T[N]) / alignof(T), alignof(T)> {
	constexpr operator const T*()	const			{ return (const T*)this; }
	constexpr operator T*()							{ return (T*)this; }
};

template<typename R, typename T = noref_t<decltype(get(declval<R>()))>> struct placement_helper : sized_placement {
	R	r;
	space_for<T>	t;
	constexpr placement_helper(R&& r) : sized_placement(&t, sizeof(T)), r(forward<R>(r))	{}
	~placement_helper()					{ r = *(T*)t; }
};
template<typename R, typename T> struct placement_helper<R, const T> {
	constexpr placement_helper(R&& r) {}
};
template<typename P> placement_helper<P> make_placement_helper(P&& p) {
	return forward<P>(p);
}

//-----------------------------------------------------------------------------
//	optional
//-----------------------------------------------------------------------------

template<typename T> struct optional {
	typedef typename T_underlying<T>::type T0;
	space_for<T>	t;
	bool			_exists;
	optional()				: _exists(false)		{}
	optional(const _none&)	: _exists(false)		{}
	optional(const T0 &x)	: _exists(true)			{ new(placement(t)) T(x); }
	optional(T0 &&x)		: _exists(true)			{ new(placement(t)) T(move(x)); }
	optional(const optional<T&> &x)	: _exists(x.exists())	{ if (_exists) new(t) T(get(x)); }
	T*				operator->()					{ ISO_ASSERT(exists()); return t; }
	const T*		operator->()	const			{ ISO_ASSERT(exists()); return t; }
	operator const	T&()			const			{ ISO_ASSERT(exists()); return *t; }
	operator		T&()							{ ISO_ASSERT(exists()); return *t; }
	bool			exists()		const			{ return _exists; }
	const T&		or_default(const T &def = T()) const	{ return _exists ? *t : def; }
	friend const T&	get(const optional &t)			{ return t; }
	friend T&		put(optional &t)				{ t._exists = true; return *new(t.t) T; }
};
template<typename T> struct T_underlying<optional<T> > : T_type<T> {};

template<> struct optional<bool> {
	uint_t<sizeof(bool)>	t;
	optional()				: t(2)					{}
	optional(const _none&)	: t(2)					{}
	optional(bool  x)		: t(x)					{}
	operator		bool()		const				{ ISO_ASSERT(exists()); return t == 1; }
	bool			exists()	const				{ return t < 2; }
	bool			or_default(bool def = false) const	{ return t < 2 ? t == 1 : def; }
	friend bool		get(const optional &t)			{ return t; }
	friend bool&	put(optional &t)				{ return (bool&)t.t; }
};

template<typename T> struct optional<T*> {
	T				*t;
	optional()				: t((T*)~uintptr_t(0))	{}
	optional(const _none&)	: t((T*)~uintptr_t(0))	{}
	optional(T *x)			: t(x)					{}
	T*				operator->() const				{ ISO_ASSERT(exists()); return t; }
	operator		T*()		const				{ ISO_ASSERT(exists()); return t; }
	bool			exists()	const				{ return !!~uintptr_t(t); }
	T*				or_default(T *def = 0) const	{ return exists() ? t : def; }
	friend T*		get(const optional &t)			{ return t; }
	friend T*&		put(optional &t)				{ return t.t; }
};

template<typename T> struct optional_temp {
	T	t;
	T	*p;
	optional_temp(T &t)		: p(&t)					{}
	optional_temp(T &&t)	: t(move(t)), p(&this->t)	{}
	operator	T&()			const				{ return *p; }
	auto		operator*()		const				{ return *p; }
	auto		operator->()	const				{ return p; }
	auto		operator!()		const				{ return !*p; }

	template<typename U> friend auto operator+(const optional_temp &a, const U &b)	{ return get(a) + b; }

	friend T&	get(const optional_temp &t)			{ return t; }
	friend T&	put(optional_temp &t)				{ return t; }
	friend auto	begin(const optional_temp &t)		{ return t->begin(); }
	friend auto	end(const optional_temp &t)			{ return t->end(); }
};
template<typename T> struct optional<T&> {
	typedef if_t<(sizeof(T) > sizeof(void*)), optional_temp<T>, const T&> T1;
	T		*t;
	optional()				: t((T*)0)				{}
	optional(const _none&)	: t((T*)0)				{}
	optional(T &x)			: t(__builtin_addressof(x)) {}
	explicit optional(T *x)	: t(x)					{}
	T&				operator=(const T &x)			{ ISO_ASSERT(exists()); return *t = x; }
	T*				operator->()					{ ISO_ASSERT(exists()); return t; }
	T*				operator&()						{ ISO_ASSERT(exists()); return t; }
	operator		T&()		const				{ ISO_ASSERT(exists()); return *t; }
	bool			exists()	const				{ return !!t; }
	bool			operator!()	const				{ return !t || !*t; }
	T1				or_default(T &&def = T()) const { if (exists()) return *t; return move(def); }
	T1				or_default(T &def)		const	{ if (exists()) return *t; return def; }
	friend T&		get(const optional &t)			{ return t; }
	friend T&		put(optional &t)				{ return t; }
};
template<typename T> struct T_underlying<optional_temp<T>> : T_type<T> {};

template<typename T> struct optional<return_holder<T> > : optional<T&> {
	using optional<T&>::optional;
};

template<typename T> struct optional<return_holder<T>&> : optional<T&> {
	using optional<T&>::optional;
};

template<typename T> optional<T> onlyif(bool b, T&& t) {
	return b ? optional<T>(forward<T>(t)) : none;
}

//template<typename C> constexpr decltype(begin(declval<C>()))		begin(optional<C> &c)		{ return begin(c.or_default());	}
//template<typename C> constexpr decltype(end(declval<C>()))			end(optional<C> &c)			{ return end(c.or_default());	}
template<typename C> constexpr decltype(begin(declval<const C>()))	begin(const optional<C> &c)	{ return begin(c.or_default());	}
template<typename C> constexpr decltype(end(declval<const C>()))	end(const optional<C> &c)	{ return end(c.or_default());	}

//-----------------------------------------------------------------------------
//	more templated functions
//-----------------------------------------------------------------------------

template<typename T>				constexpr	const T*	get_ptr(const T &t)				{ return __builtin_addressof(t); }
template<typename T>				constexpr	T*			get_ptr(T &t)					{ return __builtin_addressof(t); }
template<typename T>				constexpr	T*			get_ptr(T &&t)					{ return __builtin_addressof(t); }
template<typename T>				constexpr	T*			get_ptr(T *t)					{ return t; }

template<typename P, typename=enable_if_t< T_has_arrow<P>::value>>	constexpr	decltype(auto)	get_ptr1(P &t)		{ return t; }
template<typename P, typename=enable_if_t<!T_has_arrow<P>::value>>	constexpr	auto			get_ptr1(P &t)		{ return &t; }

template<typename T, int N>			constexpr	size_t		num_elements(const T (&t)[N])	{ return N; }
template<typename T, int N>			constexpr	uint32		num_elements32(const T (&t)[N])	{ return N; }
template<typename T>				constexpr	uint32		num_elements32(const T &t)		{ return uint32(num_elements(t)); }

template<typename T, typename=void>	constexpr	int		num_elements_v			= 1;
template<typename T, int N>			constexpr	int		num_elements_v<T[N]>	= N;
template<typename T>				constexpr	int		num_elements_v<T&>		= num_elements_v<T>;


template<typename A, typename B>	constexpr	bool	test_all(A a, B b)						{ return (as_unsigned(a) & as_unsigned(b)) == as_unsigned(b); }
template<typename T>				inline		bool	test_set(T &a, T b)						{ T t = a & b; a |= b; return !!t; }
template<typename T>				inline		bool	test_set(T &a, T b, bool set)			{ T t = a & b; a ^= t ^ (-int(set) & b); return !!t; }
template<typename T>				inline		bool	test_clear(T &a, T b)					{ T t = a & b; a ^= t; return !!t; }
template<typename T>				inline		bool	test_flip(T &a, T b)					{ return !((a ^= b) & b); }
template<typename T>				inline		bool	test_set_bit(T &a, int bit)				{ return test_set(a, T(1) << bit); }
template<typename T>				inline		bool	test_set_bit(T &a, int bit, bool set)	{ return test_set(a, T(1) << bit, set); }
template<typename T>				inline		bool	test_clear_bit(T &a, int bit)			{ return test_clear(a, T(1) << bit); }
template<typename T>				inline		bool	test_flip_bit(T &a, int bit)			{ return test_flip(a, T(1) << bit); }

template<typename B, typename T, int N> inline T B::* member_element(T (B::*array)[N], int i) {
	uintptr_t p = uintptr_t(&(((B*)0)->*array)[i]);
	return *(T B::**)(&p);
}

template<typename D, typename S>	struct cast_s		{ static inline D f(S s) { return num_traits<D>::cast(s); } };
template<typename T>				struct cast_s<T, T>	{ static inline T f(T t) { return t; } };
template<typename D, typename S>	inline D	cast(S s)	{ return cast_s<D, S>::f(s); }
template<typename D, typename S>	inline void cast_assign(D &d, const S &s) { d = num_traits<D>::cast(get(s)); }

template<typename T> struct				deleter			{ void operator()(void *p) const { delete (T*)p; } };
template<typename T> struct				deleter<T[]>	{ void operator()(void *p) const { delete[] (T*)p; } };
template<typename T> struct				destructor		{ void operator()(void *p) const { ((T*)p)->~T(); } };

template<typename T> void				deleter_fn(void *p)		{ iso::deleter<T>()(p); }
template<typename T> void				destructor_fn(void *p)	{ iso::destructor<T>()(p); }
template<typename T> void				create(void *p)			{ new(p) T(); }
template<typename T> auto				create(void *p, T&& t)	{ return new(p) noref_t<T>(forward<T>(t)); }
template<typename T, typename...P> T*	create(T *t, P&&...p)	{ return new(t) T(forward<P>(p)...); }
template<typename T> auto				dup(T&& t)				{ return new noref_t<T>(forward<T>(t)); }

template<typename T> void negate(T &t) { t = -t; }

template<int N> struct _over_s {
	template<typename I> static constexpr auto sum(I i)		{ return *i + _over_s<N - 1>::sum(i + 1); }
	template<typename I> static constexpr auto prod(I i)	{ return *i * _over_s<N - 1>::prod(i + 1); }
};
template<> struct _over_s<1> {
	template<typename I> static constexpr auto sum(I i)		{ return *i; }
	template<typename I> static constexpr auto prod(I i)	{ return *i; }
};

template<typename A, typename B>				constexpr auto	sum(A a, B b)			{ return a + b; }
template<typename A, typename B, typename... C>	constexpr auto	sum(A a, B b, C... c)	{ return a + sum(b, c...); }
template<int N, typename I>						constexpr auto	sum(I i)				{ return _over_s<N>::sum(i); }

constexpr auto													prod()					{ return 1; }
template<typename A>							constexpr auto	prod(A a)				{ return a; }
template<typename A, typename... TT>			constexpr auto	prod(A a, TT... tt)		{ return a * prod(tt...); }
template<int N, typename I>						constexpr auto	prod(I i)				{ return _over_s<N>::prod(i); }


template<typename T>							constexpr bool is_any(T &&t) { return false; }
template<typename T, typename P0, typename...P>	constexpr bool is_any(T &&t, P0 &&p0, P&&...p) {
	return t == p0 || is_any(forward<T>(t), forward<P>(p)...);
}

template<typename P0> static auto select_n(int i, P0&& p0) {
	return p0;
}
template<typename P0, typename...P> static auto select_n(int i, P0&& p0, P&&...p) {
	return i == 0 ? p0 : select_n<P0>(i - 1, forward<P>(p)...);
}
template<typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename...P> static auto select_n(int i, P0&& p0, P1&& p1, P2&& p2, P3&& p3, P4&& p4, P5&& p5, P6&& p6, P7&& p7, P8&& p8, P&&...p) {
	return i < 8 ? select_n<P0>(i, forward<P0>(p0), forward<P1>(p1), forward<P2>(p2), forward<P3>(p3), forward<P4>(p4), forward<P5>(p5), forward<P6>(p6), forward<P7>(p7)) : select_n<P0>(i - 8, forward<P8>(p8), forward<P>(p)...);
}

template<typename T, typename C0> 					constexpr auto horner(T x, C0 c0) 				{ return c0; }
template<typename T, typename C0, typename... C> 	constexpr auto horner(T x, C0 c0, C... c) 		{ return madd(horner(x, c...), x, c0); }

template<typename T> constexpr T pow(T v, uint32 n) {
	T	r = n & 1 ? v : T(1);
	while (n >>= 1) {
		v = square(v);
		if (n & 1)
			r *= v;
	}
	return r;
}

template<typename T, typename B> enable_if_t<(!is_int<B>), T> pow_mul(T a, B b, T c) {
	return mul(pow(a, b), c);
}

template<typename T, typename B> enable_if_t<(is_int<B> && !is_signed<B>), T> pow_mul(T a, B b, T c) {
	// split to avoid overflow
	auto	t = pow(a, b / 2);
	return mul(mul(c, t), b & 1 ? t * a : t);
}
template<typename T, typename B> auto rpow_mul(T a, B b, T c) {
	// split to avoid overflow
	auto	t = pow(a, b / 2);
	return div(div(c, t), b & 1 ? t * a : t);
}
template<typename T, typename B> enable_if_t<(is_int<B> && is_signed<B>), T> pow_mul(T a, B b, T c) {
	return b < 0 ? rpow_mul(a, -b, c) : pow_mul(a, uint32(b), c);
}


//-----------------------------------------------------------------------------
//	getter
//-----------------------------------------------------------------------------

template<class C> struct getter {
	C	&c;
	getter(C &c) : c(c) {}
	template<typename T> constexpr operator T() const	{
		return c.template get<T>();
	}
};

template<class C, class K, class T = decltype(declval<C>().get(declval<K>()))> struct putter {
	typedef typename T_noref<T>::type T0;
	C		&c;
	K		k;

	putter(C &c, const K k)	: c(c), k(k) {}
	putter(C &c, K &&k)		: c(c), k(move(k)) {}
	T			get()					const	{ return c.get(k); }
	T0&			put()					const	{ return c.put(k); }
	auto		remove()				const	{ return c.remove(k); }
	T			operator->()			const	{ return get(); }
	operator	T()						const	{ return get(); }
	template<typename U> auto operator=(U &&u)	{ return c.put(k, forward<U>(u)); }

	friend T	get(const putter &m)			{ return m.get(); }
	friend T	put(putter &m)					{ return m.get(); }
};

template<class C, class K, class T> struct putter<C, K, optional<T> > {
	typedef typename T_noref<T>::type T0;
	C		&c;
	K		k;

	putter(C &c, const K &k): c(c), k(k) {}
	putter(C &c, K &&k)		: c(c), k(move(k)) {}
	optional<T>	get()					const	{ return c.get(k); }
	T0&			put()					const	{ return c.put(k); }
	auto		remove()				const	{ return c.remove(k); }
	auto		operator&()				const	{ return get_ptr(c.put(k)); }
	decltype(auto)	operator->()		const	{ return get_ptr1(c.put(k)); }
	decltype(auto)	operator*()			const	{ return *get_ptr(c.put(k)); }
	auto		operator!()				const	{ return !get(); }
	operator	optional<T>()			const	{ return get(); }
	operator	T0&()					const	{ return put(); }
	template<typename U> decltype(auto) operator=(U &&u)	{ return c.put(k, forward<U>(u)); }

	bool		exists()				const	{ return get().exists(); }
	decltype(auto)	or_default(T0 &&def = T0()) const	{ return get().or_default(move(def)); }

	friend optional<T>	get(const putter &m)	{ return m.get(); }
	friend optional<T>	put(putter &m)			{ return m.get(); }
};

template<class C, class K, class T> struct T_underlying<putter<C, K, T>> : T_underlying<T> {};

//-----------------------------------------------------------------------------
//	struct consts (enum in specified type)
//-----------------------------------------------------------------------------

template<typename E, typename S> struct consts {
	S		v;
public:
	consts()							{}
	consts(const E &e)	: v(e)			{}
	operator	E()		const			{ return E(v); }
	const E&	operator=(const E &e)	{ v = e; return e; }
};

//-----------------------------------------------------------------------------
//	struct flags
//-----------------------------------------------------------------------------

template<typename T> inline auto enum_int(T t) { return underlying_type<T>(t); }

#define ENUM_FLAGOPS(X)\
	inline X operator&(X a, X b)	{ return X(enum_int(a) &  enum_int(b)); }\
	inline X operator|(X a, X b)	{ return X(enum_int(a) |  enum_int(b)); }\
	inline X operator^(X a, X b)	{ return X(enum_int(a) ^  enum_int(b)); }\
	inline X operator-(X a, X b)	{ return X(enum_int(a) & ~enum_int(b)); }\
	inline X operator*(X a, bool b)	{ return b ? a : X(0); }\
	inline X& operator&=(X &a, X b)	{ return a = a & b; }\
	inline X& operator|=(X &a, X b)	{ return a = a | b; }\
	inline X& operator^=(X &a, X b)	{ return a = a ^ b; }\
	inline X& operator-=(X &a, X b)	{ return a = a - b; }

template<typename E, typename S = uint_for_t<E>> struct flags {
	typedef uint_for_t<E>	T;
	S		v;
	flags()		: v(0)		{}
	flags(S e)	: v(T(e))	{}

	constexpr operator	S()					const	{ return v; }

	constexpr flags	with_set(E e)			const	{ return v | T(e);	 }
	constexpr flags	with_set(E e, bool b)	const	{ return (v & ~T(e)) | (-int(b) & T(e)); }
	constexpr flags	with_clear(E e)			const	{ return v & ~T(e); }
	constexpr flags	with_flip(E e)			const	{ return v ^ T(e); }

	constexpr flags	with_set_all(S e)		const	{ return v | T(e); }
	constexpr flags	with_clear_all(S e)		const	{ return v & ~T(e); }
	constexpr flags	with_flip_all(S e)		const	{ return v ^ T(e); }

	constexpr bool	test(E e)				const	{ return !!(v & T(e)); }
	constexpr bool	test_any(S e)			const	{ return !!(v & T(e)); }
	constexpr bool	test_all(S e)			const	{ return (v & T(e)) == e; }

	constexpr E		get_field(E m)			const	{ return E(v & T(m)); }

	flags&	set(E e)				{ v |= T(e);	return *this; }
	flags&	set(E e, bool b)		{ v = (v & ~T(e)) | (-int(b) & T(e)); return *this; }
	flags&	clear(E e)				{ v &= ~T(e);	return *this; }
	flags&	flip(E e)				{ v ^= T(e);	return *this; }
	flags&	swap(E e1, E e2)		{ bool b1 = test(e1); set(e1, test(e2)); set(e2, b1); return *this; }

	flags&	set_all(S e)			{ v |= T(e);	return *this; }
	flags&	clear_all(S e)			{ v &= ~T(e);	return *this; }
	flags&	flip_all(S e)			{ v ^= T(e);	return *this; }

	flags&	set_field(E m, E e)		{ v = (v & ~T(m)) | (T(e) & T(m)); return *this; }
	flags&	inc_field(E m)			{ return set_field(m, E((v | ~T(m)) + 1)); }
	flags&	dec_field(E m)			{ return set_field(m, E((v &  T(m)) - 1)); }

	bool	test_set(E e)			{ S t = v & T(e); v |= T(e); return !!t; }
	bool	test_set(E e, bool b)	{ S t = v & T(e); v ^= t ^ (-int(b) & T(e)); return !!t; }
	bool	test_clear(E e)			{ S t = v & T(e); v ^= t; return !!t; }
	bool	test_flip(E e)			{ S t = v & T(e); v ^= T(e); return !!t; }
	bool	flip_test(E e)			{ v ^= T(e); return !!(v & T(e)); }
};

//-----------------------------------------------------------------------------
//	pointer_pair - store auxilliary information in unused (lower) pointer bits
//-----------------------------------------------------------------------------

inline bool is_marked_ptr(const void *p)			{ return !!(uintptr_t(p) & 1); }
template<typename T> inline T *marked_ptr(T *p)		{ return (T*)(uintptr_t(p) | 1); }
template<typename T> inline T *unmarked_ptr(T *p)	{ return (T*)(uintptr_t(p) & ~1); }

template<typename T, typename B, unsigned M = alignof(T), unsigned A = alignof(T)> struct pointer_pair {
	static const uint32		S	= max(M, A) - A;
	union {
		uintptr_t	v;
		struct {
			uintptr_t	v;
			constexpr T*	get()		const	{ return (T*)((v & -intptr_t(M)) >> S); }
			constexpr operator T*()		const	{ return get(); }
			constexpr T* operator->()	const	{ return get(); }
			T*		operator=(T *p)				{ ISO_CHEAPASSERT((uintptr_t(p) & ((M >> S) - 1)) == 0); v = (v & (M - 1)) | (uintptr_t(p) << S); return p; }
		} a;
		struct {
			uintptr_t	v;
			constexpr B		get()		const	{ return B(v & (M - 1)); }
			constexpr operator B()		const	{ return get(); }
			B		operator=(B b)				{ v = (v & -intptr_t(M)) | (uint32(b) & (M - 1)); return b; }
		} b;
	};

	pointer_pair() {}
	pointer_pair(T *p, B b = B()) : v((uintptr_t(p) << S) | (uint32(b) & (M - 1))) {
		ISO_CHEAPASSERT((uintptr_t(p) & ((M >> S) - 1)) == 0);
	}

	T*				operator=(T *p)						{ return a = p; }
	constexpr		operator T*()				const	{ return a; }
	constexpr T*	operator->()				const	{ return a; }
//	bool	operator==(const pointer_pair &b)	const	{ return v == b.v; }
//	bool	operator!=(const pointer_pair &b)	const	{ return v != b.v; }
//	bool	operator==(const T *p)				const	{ return a == p; }

	friend constexpr T*	begin(pointer_pair &s)			{ return s;	}
	friend constexpr T*	begin(const pointer_pair &s)	{ return s;	}
};

template<typename T, typename B, unsigned M, unsigned A> struct container_traits<pointer_pair<T, B, M, A>> : container_traits<T*> {};

//-----------------------------------------------------------------------------
//	tagged_pointer - store tag in unused (upper) pointer bits
//-----------------------------------------------------------------------------

template<typename T> struct tagged_pointer {
	static const uint32		N	= 48;
	static const uintptr_t	M	= (uintptr_t(1) << N) - 1;

	union {
		uintptr_t	v;
		struct {
			uintptr_t	a:48, :16;
			constexpr T*	get()		const	{ return (T*)a; }
			constexpr operator T*()		const	{ return get(); }
			constexpr T* operator->()	const	{ return get(); }
			T*		operator=(T *p)				{ a = uintptr_t(p); return p; }
		} a;
		struct {
			int		alo, ahi:16, b:16;
		};
	};

	tagged_pointer() {}
	tagged_pointer(T *p, int b = 0) : v(uintptr_t(p) | (uintptr_t(b) << N)) {
//		ISO_CHEAPASSERT(((uintptr_t)p >> N) == 0);
	}

	T*				operator=(T *p)						{ return a = p; }
	constexpr		operator T*()				const	{ return a; }
	constexpr T*	operator->()				const	{ return a; }
	bool	operator==(const tagged_pointer &b) const	{ return v == b.v; }
	bool	operator!=(const tagged_pointer &b) const	{ return v != b.v; }

	friend constexpr T*	begin(tagged_pointer &s)		{ return s;	}
	friend constexpr T*	begin(const tagged_pointer &s)	{ return s;	}
};

template<typename T> struct container_traits<tagged_pointer<T>> : container_traits<T*> {};

//-----------------------------------------------------------------------------
//	arbitrary
//-----------------------------------------------------------------------------

class arbitrary {
	uintptr_t	p;
public:
	arbitrary()			{}
	template<typename T> arbitrary(const T &t)			{ ISO_COMPILEASSERT(sizeof(T) <= sizeof(p)); p = 0; new((void*)this) T(t); }
	arbitrary			&me()							{ return *this; }

	constexpr bool		operator!()				const	{ return !p; }
	constexpr uintptr_t	operator&(uintptr_t b)	const	{ return p & b; }
	constexpr uintptr_t	operator|(uintptr_t b)	const	{ return p | b; }
	constexpr uintptr_t	operator>>(int b)		const	{ return p >> b; }
	constexpr uintptr_t	operator<<(int b)		const	{ return p << b; }
	operator			const void*()			const	{ return (const void*)p; }
	template<typename T> constexpr const T&		as()	const	{ ISO_COMPILEASSERT(sizeof(T) <= sizeof(p)); return *(T*)&p; }
	template<typename T> constexpr operator const T&()	const	{ return as<T>(); }

	template<typename T> T&	as()					{ ISO_COMPILEASSERT(sizeof(T) <= sizeof(p)); return *(T*)&p; }
	template<typename T> operator T&()				{ return as<T>(); }
	template<typename T> T&	operator=(const T &t)	{ p = 0; return as<T>() = t; }
};

class arbitrary_ptr {
	uintptr_t	p;
	struct deref {
		uintptr_t	p;
		constexpr deref(uintptr_t p) : p(p) {}
		template<typename T> constexpr operator T&()	{ return *(T*)p; }
		template<typename T> T& operator=(const T &t)	{ return *(T*)p = t; }
	};
public:
	constexpr			arbitrary_ptr()								: p(0)				{}
	explicit constexpr	arbitrary_ptr(uintptr_t p)					: p(p)				{}
	constexpr arbitrary_ptr(nullptr_t)								: p(0)				{}
	template<typename T> constexpr arbitrary_ptr(T *t)				: p(uintptr_t(t))	{}
	template<typename T> constexpr arbitrary_ptr(const T *t)		: p(uintptr_t(t))	{}
	constexpr bool					operator!()				const	{ return !p; }
	constexpr explicit				operator bool()			const	{ return !!p; }
	constexpr explicit				operator uintptr_t()	const	{ return p; }
	constexpr arbitrary_ptr			operator+(intptr_t a)	const	{ return arbitrary_ptr(p + a); }
	constexpr arbitrary_ptr			operator-(intptr_t a)	const	{ return arbitrary_ptr(p - a); }
	constexpr deref					operator*()				const	{ return p; }
	template<typename T> constexpr bool	 operator==(T *p2)	const	{ return p == (uintptr_t)p2; }
	template<typename T> constexpr bool	 operator!=(T *p2)	const	{ return p != (uintptr_t)p2; }
	template<typename T> constexpr operator T*()			const	{ return (T*)p; }
	template<typename T> T *operator=(T *t)							{ p = uintptr_t(t); return t; }
	friend auto align(arbitrary_ptr x, uint32 a)		{ return arbitrary_ptr(align(x.p, a)); }
	friend auto align_down(arbitrary_ptr x, uint32 a)	{ return arbitrary_ptr(align_down(x.p, a)); }
};

template<> struct optional<arbitrary_ptr> {
	arbitrary_ptr	t;
	optional()													: t((void*)~uintptr_t(0)) {}
	optional(const _none&)										: t((void*)~uintptr_t(0)) {}
	optional(arbitrary_ptr x)			: t(x)					{}
	arbitrary_ptr			operator=(arbitrary_ptr x)			{ return t = x; }
	arbitrary_ptr			operator->() const					{ ISO_ASSERT(exists()); return t; }
	operator		arbitrary_ptr()		const					{ ISO_ASSERT(exists()); return t; }
	bool			exists()	const							{ return !!~uintptr_t((void*)t); }
	arbitrary_ptr	or_default(arbitrary_ptr def = 0) const		{ return exists() ? t : def; }
	friend arbitrary_ptr		get(const optional &t)			{ return t; }
	friend arbitrary_ptr&		put(optional &t)				{ return t.t; }
};

class arbitrary_const_ptr {
	uintptr_t	p;
	struct deref {
		uintptr_t	p;
		constexpr deref(uintptr_t p) : p(p) {}
		template<typename T> constexpr operator const T&() { return *(const T*)p; }
	};
public:
	arbitrary_const_ptr()	{}
	explicit constexpr arbitrary_const_ptr(uintptr_t p)				: p(p)				{}
	constexpr arbitrary_const_ptr(nullptr_t)						: p(0)				{}
	template<typename T> constexpr arbitrary_const_ptr(const T *t)	: p(uintptr_t(t))	{}
	constexpr bool					operator!()				const	{ return !p; }
	constexpr explicit				operator bool()			const	{ return !!p; }
	constexpr explicit				operator uintptr_t()	const	{ return p; }
	constexpr arbitrary_const_ptr	operator+(intptr_t a)	const	{ return arbitrary_const_ptr(p + a); }
	constexpr arbitrary_const_ptr	operator-(intptr_t a)	const	{ return arbitrary_const_ptr(p - a); }
	constexpr deref					operator*()				const	{ return p; }
	template<typename T> constexpr bool	 operator==(T *p2)	const	{ return p == (uintptr_t)p2; }
	template<typename T> constexpr bool	 operator!=(T *p2)	const	{ return p != (uintptr_t)p2; }
	template<typename T> constexpr operator const T*()		const	{ return (const T*)p; }
	template<typename T> const T *operator=(const T *t)				{ p = uintptr_t(t); return t; }
	friend auto align(arbitrary_const_ptr x, uint32 a)		{ return arbitrary_const_ptr(align(x.p, a)); }
	friend auto align_down(arbitrary_const_ptr x, uint32 a)	{ return arbitrary_const_ptr(align_down(x.p, a)); }
};

//-----------------------------------------------------------------------------
//	memory_block
//-----------------------------------------------------------------------------

struct memory_block_copy : comparisons<memory_block_copy> {
	void	*p;
	size_t	n;
	memory_block_copy(void *p, size_t n) : p(p), n(n)	{}
	template<typename T>	operator const T&()				const	{ return *(const T*)p; }
	template<typename T>	operator T&()							{ return *(T*)p; }
	memory_block_copy&		operator=(const memory_block_copy &c)	{ memcpy(p, c.p, n); return *this; }
	template<typename T> T&	operator=(const T &t)					{ return *(T*)p = t; }

	template<typename T> friend int compare(const memory_block_copy &a, const T &b) { return memcmp(a.p, &b, min(a.n, sizeof(T))); }
	template<typename T> friend int compare(const T &a, const memory_block_copy &b) { return memcmp(&a, &b, min(b.n, sizeof(T))); }

	friend int compare(const memory_block_copy &a, const memory_block_copy &b) {
		int r = memcmp(a.p, b.p, min(a.n, b.n));
		return r ? r : int(intptr_t(a.n) - intptr_t(b.n));
	}
};

struct memory_block {
	void	*p;
	size_t	n;

	constexpr memory_block()						: p(0), n(0)			{}
	constexpr memory_block(const _none&)			: p(0), n(0)			{}
	constexpr memory_block(void *p, size_t n)		: p(p), n(n)			{}
	constexpr memory_block(void *p, void *e)		: p(p), n((uint8*)e - (uint8*)p) {}
	constexpr memory_block(const memory_block &t)	: p(t.p), n(t.n)		{}
	template<typename T> explicit	constexpr memory_block(T *t)		: p(t), n(sizeof(T))		{}
	template<typename T, int N>		constexpr memory_block(T (&t)[N])	: p(&t), n(sizeof(T) * N)	{}
	template<typename T> operator T*()					const	{ return (T*)p; }
	operator arbitrary_ptr()							const	{ return p; }
	operator arbitrary_const_ptr()						const	{ return p; }

	memory_block_copy	operator*()						const	{ return memory_block_copy(p, n); }
	explicit operator bool()							const	{ return !!p; }
	size_t				size()							const	{ return n; }
	uint32				size32()						const	{ return uint32(n); }
	size_t				length()						const	{ return n; }			// DEPRECATE
	arbitrary_ptr		begin()							const	{ return p; }
	arbitrary_ptr		end()							const	{ return (char*)p + n; }
	bool				contains(const void *x)			const	{ return between(x, p, end()); }

	void				clear_contents()				const	{ memset(p, 0, n); }
	size_t				copy_to(void *dest)				const	{ memcpy(dest, p, n); return n; }
	size_t				move_to(void *dest)				const	{ memmove(dest, p, n); return n; }
	void*				find(struct const_memory_block data)	const;

	size_t				copy_from(const void *srce)		const	{ memcpy(p, srce, n); return n; }
	void				shift_down(size_t i)			const	{ if (i && i < n) memmove(p, (uint8*)p + i, n - i); }
	void				shift_up(size_t i)				const	{ if (i && i < n) memmove((uint8*)p + i, p, n - i); }

	memory_block		begin(size_t i)					const	{ return i < n ? memory_block((char*)p, i) : *this; }
	memory_block		end(size_t i)					const	{ return i < n ? memory_block((char*)p + n - i, i) : *this; }
	memory_block		slice(void *a, void *b)			const	{ ISO_ASSERT(a >= p && b >= a && b <= end()); return memory_block(a, size_t(b) - size_t(a)); }
	memory_block		slice(void *a)					const	{ return slice(a, (void*)end()); }
	memory_block		slice(void *a, intptr_t b)		const	{ return slice(a, (b < 0 ? (char*)end() : (char*)a) + b); }
	memory_block		slice(intptr_t a, intptr_t b)	const	{ return slice((char*)p + (a < 0 ? n + a : a), b); }
	memory_block		slice(intptr_t a)				const	{ return slice((char*)p + (a < 0 ? n + a : a)); }
	memory_block		slice_to(intptr_t b)			const	{ return slice(p, min(b, n)); }
	memory_block		slice_to(void *b)				const	{ return slice(p, b); }

	template<typename T> memory_block operator+(T i)	const	{ return p && i <= n ? memory_block((char*)p + i, n - i) : none; }
	template<typename T> void	fill(const T &t)		const	{ for (T *i = (T*)p, *e = end(); i < e; ++i) *i = t; }

	template<typename R> bool	read(R &r)				const	{ return r.readbuff(p, n) == n; }
	template<typename W> bool	write(W &w)				const	{ return w.writebuff(p, n) == n; }
};

struct const_memory_block {
	const void	*p;
	size_t		n;

	constexpr const_memory_block()								: p(0), n(0)			{}
	constexpr const_memory_block(_none)							: p(0), n(0)			{}
	constexpr const_memory_block(const void *p, size_t n)		: p(p), n(n)			{}
	constexpr const_memory_block(const void *p, const void *e)	: p(p), n((uint8*)e - (uint8*)p) {}
	constexpr const_memory_block(const memory_block &m)			: p(m.p), n(m.n)		{}
	const_memory_block(const char *t)							: p(t), n(t ? strlen(t) : 0)	{}
	const_memory_block(char *t)									: p(t), n(t ? strlen(t) : 0)	{}
	template<typename T, typename = enable_if_t<is_pointer_t<T>>> constexpr	const_memory_block(T t)	: p(t), n(sizeof(deref_t<T>)){}
	template<typename T, size_t N>		constexpr	const_memory_block(const T (&t)[N])		: p(&t), n(sizeof(T) * N)	{}
	template<size_t N>					constexpr	const_memory_block(const char (&t)[N])	: p(&t), n(N - 1)			{}
	template<typename T> operator const T*()	const	{ return (const T*)p; }
	constexpr operator arbitrary_const_ptr()	const	{ return p; }

	template<typename T> auto&	operator=(const T *t)	{ p = t; n = sizeof(T); return *this; }

	const memory_block_copy	operator*()						const	{ return memory_block_copy(const_cast<void*>(p), n); }
	explicit operator bool()								const	{ return !!p; }
	size_t				size()								const	{ return n; }
	uint32				size32()							const	{ return uint32(n); }
	size_t				length()							const	{ return n; }			// DEPRECATE
	arbitrary_const_ptr	begin()								const	{ return p; }
	arbitrary_const_ptr	end()								const	{ return (const char*)p + n; }
	bool				contains(const void *x)				const	{ return between(x, p, end()); }

	size_t				copy_to(void *dest)					const	{ memcpy(dest, p, n); return n; }
	size_t				move_to(void *dest)					const	{ memmove(dest, p, n); return n; }
	const void*			find(const_memory_block data)		const	{ return memmem(p, n, data.p, data.n); }

	const_memory_block	begin(size_t i)						const	{ return i < n ? const_memory_block((const char*)p, i) : *this; }
	const_memory_block	end(size_t i)						const	{ return i < n ? const_memory_block((const char*)p + n - i, i) : *this; }
	const_memory_block	slice(const void *a, const void *b)	const	{ ISO_ASSERT(a >= p && b >= a && b <= end()); return const_memory_block((void*)a, size_t(b) - size_t(a)); }
	const_memory_block	slice(const void *a)				const	{ return slice(a, (const void*)end()); }
	const_memory_block	slice(const void *a, intptr_t b)	const	{ return slice(a, (b < 0 ? (const char*)end() : (const char*)a) + b); }
	const_memory_block	slice(intptr_t a, intptr_t b)		const	{ return slice((const char*)p + (a < 0 ? n + a : a), b); }
	const_memory_block	slice(intptr_t a)					const	{ return slice((const char*)p + (a < 0 ? n + a : a)); }
	const_memory_block	slice_to(intptr_t b)				const	{ return slice(p, b); }
	const_memory_block	slice_to(const void *b)				const	{ return slice(p, b); }

	template<typename T> const_memory_block operator+(T i)	const	{ return p && i < n ? const_memory_block((const char*)p + i, n - i) : none; }
	template<typename W> bool	write(W &w)					const	{ return !n || w.writebuff(p, n) == n; }

	friend constexpr memory_block unconst(const const_memory_block &b)	{ return memory_block((void*)b.p, b.n); }
};

inline void* memory_block::find(struct const_memory_block data)	const	{ return (void*)memmem(p, n, data.p, data.n); }


template<typename T> inline void	write_bytes(const memory_block &b, T x)		{ write_bytes<T>(b, x, b.size32()); }
template<typename T> inline T		read_bytes(const const_memory_block &b)		{ return read_bytes<T>(b, b.size32()); }

template<typename T> constexpr const T* addr(const T&t) { return &t; }

template<typename T> constexpr range<T*>						make_range(const memory_block &b) {
	return make_range_n<T*>(b, b.size() / sizeof(T));
}
template<typename T> constexpr range<const T*>					make_range(const const_memory_block &b) {
	return make_range_n<const T*>(b, b.size() / sizeof(T));
}
template<typename T> force_inline range<next_iterator<T> >		make_next_range(const memory_block &b) {
	return {next_iterator<T>(b), next_iterator<T>(b.end(), -1)};
}
template<typename T> force_inline range<next_iterator<const T> >	make_next_range(const const_memory_block &b) {
	return {next_iterator<const T>(b), next_iterator<const T>(b.end(), -1)};
}
template<typename T> inline range<stride_iterator<T> >			make_strided(const memory_block &b, int s) {
	return make_range_n(strided((T*)b, s), b.n / s);
}
template<typename T> inline range<stride_iterator<const T> >	make_strided(const const_memory_block &b, int s) {
	return make_range_n(strided((const T*)b, s), b.n / s);
}

//-----------------------------------------------------------------------------
//	malloc_block
//-----------------------------------------------------------------------------

struct malloc_block : memory_block {
protected:
	constexpr malloc_block(void *p, size_t n)				: memory_block(p, n)			{}
public:
	static malloc_block own(void *p, size_t n) {
		return {p, n};
	}
	template<typename R> static malloc_block unterminated(R &&r) {
		size_t			n = r.remaining();
		malloc_block	mb(n);
		r.readbuff(mb, n);
		return mb;
	}
	template<typename R> static malloc_block zero_terminated(R &r) {
		size_t			n = r.remaining();
		malloc_block	mb(n + 1);
		r.readbuff(mb, n);
		((char*)mb)[n] = 0;
		return mb;
	}

	constexpr malloc_block()								{}
	constexpr malloc_block(const _none&)					{}
	malloc_block(size_t n)									: memory_block(malloc(n), n)	{}
	malloc_block(malloc_block &&b)							: memory_block(b.p, b.n)		{ b.p = 0; }
	malloc_block(const malloc_block &b)						: malloc_block(b.n)				{ copy_from(b); }
	malloc_block(const memory_block &b)						: malloc_block(b.n)				{ copy_from(b); }
	malloc_block(const const_memory_block &b)				: malloc_block(b.n)				{ copy_from(b); }
	template<typename R> malloc_block(R &&r, size_t n)		: malloc_block(n)				{ resize(r.readbuff(p, n)); }
	~malloc_block()											{ if (p) free(p); }

	malloc_block&	clear()									{ free(p); p = nullptr; n = 0; return *this; }
	memory_block	detach()								{ memory_block r = *this; p = 0; return r; }
	malloc_block&	create(size_t n2)						{ if (!p || n2 > n) { free(p); p = malloc(n2); } n = n2; return *this; }
	malloc_block&	resize(size_t n2)						{ p = realloc(p, n2); n = n2; return *this; }
	malloc_block&	resize(size_t n2, uint8 fill)			{ p = realloc(p, n2); if (n2 > n) memset((uint8*)p + n, fill, n2 - n); n = n2; return *this; }
	memory_block	extend(size_t n2)						{ return resize(n + n2).end(n2); }

	malloc_block&	operator=(const _none&)					{ clear(); return *this; }
	malloc_block&	operator=(malloc_block &&b)				{ swap(*this, b); return *this; }
	malloc_block&	operator=(const malloc_block &b)		{ create(b.n).copy_from(b); return *this; }
	malloc_block&	operator=(const memory_block &b)		{ create(b.n).copy_from(b); return *this; }
	malloc_block&	operator=(const const_memory_block &b)	{ create(b.n).copy_from(b); return *this; }

	malloc_block&	operator+=(const const_memory_block &b) { extend(b.n).copy_from(b); return *this; }
	malloc_block&	operator+=(const memory_block &b)		{ extend(b.n).copy_from(b); return *this; }
	malloc_block&	operator+=(malloc_block &&b)			{ if (n == 0) swap(*this, b); else extend(b.n).copy_from(b); return *this; }

	template<typename R> bool read(R &r, size_t n2)			{ return check_readbuff(r, create(n2), n2); }
	template<typename R> bool read(R &r)					{ return check_readbuff(r, p, n); }
	friend void swap(malloc_block &a, malloc_block &b)		{ swap(a.p, b.p); swap(a.n, b.n); }
};

struct memory_block_own : memory_block {
	bool	owned	= false;
	using memory_block::memory_block;
	memory_block_own(memory_block b)					: memory_block(b), owned(false) {}
	memory_block_own(malloc_block &&b)					: memory_block(b.detach()), owned(true) {}
	memory_block_own(memory_block_own &&b)				: memory_block(b), owned(b.owned) { b.owned = false; }
	memory_block_own&	operator=(memory_block_own&& b)	{ swap(*this, b); return *this; }
	~memory_block_own()			{ if (owned) free(p); }
	void	clear()				{ if (owned) { free(p); owned = false; } p = nullptr; }
	friend void swap(memory_block_own &a, memory_block_own &b)	{ swap(a.p, b.p); swap(a.n, b.n); swap(a.owned, b.owned); }
};

struct const_memory_block_own : const_memory_block {
	bool	owned	= false;
	using const_memory_block::const_memory_block;
	const_memory_block_own(const_memory_block b)		: const_memory_block(b) {}
	const_memory_block_own(malloc_block &&b)			: const_memory_block(b.detach()), owned(true) {}
	const_memory_block_own(const_memory_block_own &&b)	: const_memory_block(b), owned(b.owned) { b.owned = false; }
	const_memory_block_own&	operator=(const_memory_block_own&& b)	{ swap(*this, b); return *this; }
	~const_memory_block_own()	{ if (owned) free((void*)p); }
	void	clear()				{ if (owned) { free((void*)p); owned = false; } p = nullptr; }
	friend void swap(const_memory_block_own &a, const_memory_block_own &b)	{ swap(a.p, b.p); swap(a.n, b.n); swap(a.owned, b.owned); }
};

struct malloc_block2 : malloc_block {
	size_t	max_size;

	constexpr malloc_block2()								: max_size(0) {}
	constexpr malloc_block2(const _none&)					: max_size(0) {}
	malloc_block2(size_t n)									: malloc_block(n), max_size(n)	{}
	malloc_block2(malloc_block2 &&b)						: malloc_block(move(b))			{ max_size = b.max_size; }
	malloc_block2(const malloc_block2 &b)					: malloc_block2(b.n)			{ copy_from(b); }
	malloc_block2(const memory_block &b)					: malloc_block2(b.n)			{ copy_from(b); }
	malloc_block2(const const_memory_block &b)				: malloc_block2(b.n)			{ copy_from(b); }
	template<typename R> malloc_block2(R &&r, size_t n)		: malloc_block2(n)				{ resize(r.readbuff(p, n)); }

	malloc_block2&	create(size_t n2)						{ if (n2 > max_size) malloc_block::create(max_size = n2); else n = n2; return *this; }
	malloc_block2&	reserve(size_t n2)						{ if (n2 > max_size) p = realloc(p, max_size = max(max_size * 2, n2)); return *this; }
	malloc_block2&	resize(size_t n2)						{ reserve(n2); n = n2; return *this; }
	memory_block	extend(size_t n2)						{ return resize(n + n2).end(n2); }

	malloc_block2&	operator=(malloc_block2 &&b)			{ swap(*this, b); return *this; }
	malloc_block2&	operator=(const malloc_block2 &b)		{ create(b.n).copy_from(b); return *this; }
	malloc_block2&	operator=(const memory_block &b)		{ create(b.n).copy_from(b); return *this; }
	malloc_block2&	operator=(const const_memory_block &b)	{ create(b.n).copy_from(b); return *this; }

	malloc_block2&	operator+=(const const_memory_block &b) { extend(b.n).copy_from(b); return *this; }
	malloc_block2&	operator+=(const memory_block &b)		{ extend(b.n).copy_from(b); return *this; }
	malloc_block2&	operator+=(malloc_block2 &&b)			{ if (n == 0) swap(*this, b); else extend(b.n).copy_from(b); return *this; }

	template<typename R> bool read(R &r, size_t n2)			{ return check_readbuff(r, create(n2), n2); }
	friend void swap(malloc_block2 &a, malloc_block2 &b)	{ swap(a.p, b.p); swap(a.n, b.n); swap(a.max_size, b.max_size); }
};


inline void	duplicate(memory_block &m) { m = malloc_block(m).detach(); }

struct malloc_chain {
	struct link {
		link	*next;
		size_t	n;
		uint8	data[];
		link(size_t n) : next(0), n(n) {}
		operator memory_block() { return { data, n}; }
	};
	struct iterator {
		link	*p;
		iterator(link *p) : p(p)	{}
		iterator& operator++()			{ p = p->next; return *this; }
		operator link*()		const	{ return p;	}
		link	*operator->()	const	{ return p;	}
	};
	link		*first, *last;

	malloc_chain() : first(0), last((link*)&first) {}
	~malloc_chain() {
		for (link *p = first, *n; p; p = n) {
			n = p->next;
			free(p);
		}
	}
	memory_block	_push_back(link *link) {
		last->next = link;
		last = link;
		return *link;
	}
	memory_block	push_back(size_t n) {
		return _push_back(new(placement(malloc(sizeof(link) + n))) link(n));
	}
	void		push_back(const const_memory_block &m) {
		m.copy_to(_push_back(new(placement(malloc(sizeof(link) + m.size()))) link(m.size())));
	}
	iterator	begin()	const	{ return first;		}
	iterator	end()	const	{ return (link*)0;	}

	size_t		total()	const	{
		size_t	t = 0;
		for (iterator i = begin(); i != end(); ++i)
			t += i->n;
		return t;
	}
	void		copy_to(void *dest) const {
		for (iterator i = begin(); i != end(); ++i) {
			memcpy(dest, i->data, i->n);
			dest = (uint8*)dest + i->n;
		}
	}
	operator malloc_block() const {
		malloc_block	b(total());
		copy_to(b);
		return b;
	}
};

//-----------------------------------------------------------------------------
//	class watched - defined in functions.h
//-----------------------------------------------------------------------------

template<typename T> struct watched;
template<typename T, typename K> struct watched_base;

//-----------------------------------------------------------------------------
//	class pair + triple
//-----------------------------------------------------------------------------

template<class A, class B, int use_union = (int(T_isempty<A>::value) + int(T_isempty<B>::value) * 2)> struct pair_helper {
#if 0
	typedef hold_ref_t<A>	A1;
	typedef hold_ref_t<B>	B1;
	A1	a;
	B1	b;
#else
	A	a;
	B	b;
#endif
	pair_helper() : a(), b() {}
	template<typename A2, typename B2> pair_helper(A2 &&a, B2 &&b) : a(forward<A2>(a)), b(forward<B2>(b))	{}
};

template<class A, class B> struct pair_helper<A, B, 1> {
	typedef hold_ref_t<B>	B1;
	union {
		A	a;
		B1	b;
	};
	pair_helper() : b() {}
	~pair_helper() { a.~A(); b.~B1(); }
	template<typename A2, typename B2> pair_helper(A2 &&a, B2 &&b) : b(forward<B2>(b))	{}
};

template<class A, class B> struct pair_helper<A, B, 2> {
	typedef hold_ref_t<A>	A1;
	union {
		A1	a;
		B	b;
	};
	pair_helper() : a() {}
	~pair_helper() { a.~A1(); b.~B(); }
	template<typename A2, typename B2> pair_helper(A2 &&a, B2 &&b) : a(forward<A2>(a))	{}
};

template<class A, class B> struct pair_helper<A, B, 3> {
	union {
		A	a;
		B	b;
	};
	pair_helper() {}
	~pair_helper() { a.~A(); b.~B(); }
	template<typename A2, typename B2> pair_helper(A2 &&a, B2 &&b)	{}
};

template<typename _A, typename _B> struct pair : comparisons<pair<_A, _B> >, pair_helper<_A, _B> {
	typedef _A	A;
	typedef _B	B;
	typedef pair_helper<A, B> base;
	using base::a; using base::b;
	pair() {}
	template<typename A2, typename B2> pair(const pair<A2,B2> &p) : base(p.a, p.b) {}
#ifdef USE_RVALUE_REFS
	pair(const pair &p)					= default;
	pair(pair &&p)						= default;
	pair&	operator=(const pair &p)	= default;
	pair&	operator=(pair &&p)			= default;
	pair(const A &a)															: base(a, B())	{}
	pair(const A &a, const B &b)												: base(a, b)	{}
//	template<typename A2>				pair(A2 &&a2)							: base(forward<A2>(a2), B()) {}
	template<typename A2, typename B2>	pair(pair<A2,B2> &&p)					: base(move_nonref<A2>(p.a), move_nonref<B2>(p.b)) {}
	template<typename A2, typename B2>	pair(A2 &&a2, B2 &&b2)					: base(forward<A2>(a2), forward<B2>(b2)) {}
	template<typename A2, typename B2>	pair& operator=(const pair<A2,B2> &p)	{ a = p.a; b = p.b; return *this; }
	template<typename A2, typename B2>	pair& operator=(pair<A2,B2> &&p)		{ a = move_nonref<A2>(p.a); b = move_nonref<B2>(p.b); return *this; }
#else
	pair(typename T_param<_A>::type a, typename T_param<_B>::type b) : base(a, b)	{}
#endif
	friend void swap(pair &a, pair &b) {
		swap(a.a, b.a);
		swap(a.b, b.b);
	}
	template<typename T> friend int compare(const pair &a, const T &b) {
		if (int	v = simple_compare(a.a, b.a))
			return v;
		return simple_compare(a.b, b.b);
	}
};

#ifdef USE_RVALUE_REFS
template<typename A, typename B> force_inline auto make_pair(A &&a, B &&b) {
	return pair<A, B>(forward<A>(a), forward<B>(b));
}
#else
template<typename A, typename B> force_inline auto make_pair(const A &a, const B &b) {
	return pair<typename T_noarray<A>::type, typename T_noarray<B>::type>(a, b);
}
#endif

template<typename A, typename B> force_inline auto make_pair_noref(const A &a, const B &b) {
	return pair<A, B>(a, b);
}

#define PAIR(a,b)		pair<a,b>

template<typename A, typename B> struct pair_iterator {
	A	a;
	B	b;
	pair_iterator(A &&a, B &&b) : a(forward<A>(a)), b(forward<B>(b)) {}
	auto	operator*()	const { return make_pair(*a, *b); }
	bool	operator==(const pair_iterator &i) const { return a == i.a; }
	bool	operator!=(const pair_iterator &i) const { return a != i.a; }
	pair_iterator& operator++() { ++a; ++b; return *this; }
};

template<typename A, typename B> auto	begin(const pair<A,B> &p)	->pair_iterator<decltype(begin(p.a)), decltype(begin(p.b))>	{ return {begin(p.a), begin(p.b)}; }
template<typename A, typename B> auto	end(const pair<A,B> &p)		->pair_iterator<decltype(end(p.a)), decltype(end(p.b))>		{ return {end(p.a), end(p.b)}; }

template<typename _A, typename _B, typename _C> struct triple : comparisons<triple<_A, _B, _C> > {
	typedef _A	A;
	typedef _B	B;
	typedef _C	C;
	A			a;
	B			b;
	C			c;
	force_inline triple() : a(), b()	{}
	template<typename A2, typename B2, typename C2> force_inline triple(const triple<A2,B2,C2> &t) : a(t.a), b(t.b), c(t.c) {}
#ifdef USE_RVALUE_REFS
	triple(const triple &p)				= default;
	triple(triple &&p)					= default;
	triple&	operator=(const triple &p)	= default;
	triple&	operator=(triple &&p)		= default;
	template<typename A2, typename B2, typename C2> triple(triple<A2,B2,C2> &&p)					: a(move(p.a)), b(move(p.b)), c(move(p.c))					{}
	template<typename A2, typename B2, typename C2> triple(A2 &&a, B2 &&b, C2 &&c)					: a(forward<A2>(a)), b(forward<B2>(b)), c(forward<C2>(c))	{}
	template<typename A2, typename B2, typename C2> triple& operator=(const triple<A2,B2,C2> &p)	{ a = p.a; b = p.b; c = p.c; return *this; }
	template<typename A2, typename B2, typename C2> triple& operator=(triple<A2,B2,C2> &&p)			{ a = move(p.a); b = move(p.b); c = move(p.c); return *this; }
#else
	force_inline triple(typename T_param<_A>::type a, typename T_param<_B>::type b, typename T_param<_C>::type c) : a(a), b(b), c(c)	{}
#endif
	friend void swap(triple &a, triple &b) {
		swap(a.a, b.a);
		swap(a.b, b.b);
		swap(a.c, b.c);
	}
	template<typename A2, typename B2, typename C2> friend int compare(const triple &a, const triple<A2,B2,C2> &b) {
		if (int	v = compare(a.a, b.a))
			return v;
		if (int	v = compare(a.b, b.b))
			return v;
		return compare(a.c, b.c);
	}
};

#ifdef USE_RVALUE_REFS
template<typename A, typename B, typename C> force_inline triple<typename T_noarray<typename T_noref<A>::type>::type, typename T_noarray<typename T_noref<B>::type>::type, typename T_noarray<typename T_noref<C>::type>::type> make_triple(A &&a, B &&b, C &&c) {
	return triple<typename T_noarray<typename T_noref<A>::type>::type, typename T_noarray<typename T_noref<B>::type>::type, typename T_noarray<typename T_noref<C>::type>::type>(forward<A>(a), forward<B>(b), forward<C>(c));
}
#else
template<typename A, typename B, typename C> force_inline triple<A, B, C> make_triple(const A &a, const B &b, const C &c) {
	return triple<A, B, C>(a, b, c);
}
#endif

template<typename A, typename B, typename C> force_inline triple<const A&, const B&, const C&> maketriple_ref(const A &a, const B &b, const C &c) {
	return triple<const A&, const B&, const C&>(a, b, c);
}
#define TRIPLE(a,b,c)	triple<a,b,c>

//-----------------------------------------------------------------------------
//	class unique_ptr
//-----------------------------------------------------------------------------

template<typename T, typename D = iso::deleter<T>> class unique_ptr : pair<T*, D> {
protected:
	typedef pair<T*,D>	B;
	using	B::a;
	void	set(T* p2)	{ if (T *p1 = exchange(a, p2)) if (p1 != p2) B::b(p1); }
public:
	template<typename...P> unique_ptr&	emplace(P&&...pp)	{ set(new T(forward<P>(pp)...)); return *this; }
	constexpr unique_ptr()					: B(nullptr)	{}
	constexpr unique_ptr(_none)				: B(nullptr)	{}
	constexpr unique_ptr(nullptr_t)			: B(nullptr)	{}
	/*explicit*/ constexpr unique_ptr(T *p)		: B(p)			{}
	template<typename D1> constexpr unique_ptr(T *p, D1&& d)	: B(p)	{}
	unique_ptr(unique_ptr &&b)				: B(b.detach())	{}
	unique_ptr(malloc_block &&b)			: B(b.detach())	{}
	~unique_ptr()											{ B::b(a); }

	unique_ptr&				operator=(T *b)					{ set(b); return *this; }
	unique_ptr&				operator=(unique_ptr &&b)		{ swap(a, b.a); return *this; }
	unique_ptr&				operator=(malloc_block &&b)		{ set(b.detach()); return *this; }
	template<class T2>	unique_ptr& operator=(unique_ptr<T2,D> &&b) { set(b.detach()); return *this; }

	constexpr operator T*()			const			{ return a; }
	constexpr T*	operator->()	const			{ return a; }
	constexpr T*	get()			const			{ return a; }
	void			clear()							{ if (a) { B::b(a); a = 0; } }
	T*				detach()						{ return exchange(a, nullptr); }

	friend void	swap(unique_ptr &a, unique_ptr &b)	{ swap((B&)a, (B&)b); }
	friend constexpr T*	get(const unique_ptr &a)	{ return a; }
	friend constexpr T*	put(unique_ptr &a)			{ return a; }
};

template<typename T, typename D> class unique_ptr<T[],D> : public unique_ptr<T,D> {
	using unique_ptr<T,D>::a;
public:
	using unique_ptr<T,D>::unique_ptr;
	T&	operator[](intptr_t i)	const	{ return a[i]; }
};

template<typename T, size_t N, typename D> class unique_ptr<T[N],D> : public unique_ptr<T,D> {
	using unique_ptr<T,D>::a;
public:
	using unique_ptr<T,D>::unique_ptr;
	T&	operator[](intptr_t i)	const	{ return a[i]; }
	T*	begin()					const	{ return a; }
	T*	end()					const	{ return a + N; }
};

template <typename T, typename... A> force_inline unique_ptr<T> make_unique(A&&... a)	{ return new T(forward<A>(a)...); }

//-----------------------------------------------------------------------------
//	class ref_ptr
//-----------------------------------------------------------------------------

template<typename T> class ref_ptr {
protected:
	T	*p;
	void	set(T* p2 = 0)						{ if (p2) p2->addref(); if (p) p->release(); p = p2; }
public:
	static ref_ptr make_noref(T *p)				{ ref_ptr r; r.p = p; return r; }
	template<typename...P> ref_ptr&			emplace(P&&...pp)	{ set(new T(forward<P>(pp)...)); return *this; }
	template<typename...P> static ref_ptr	make(P&&...pp)		{ return new T(forward<P>(pp)...); }

	ref_ptr(const ref_ptr &b)					{ if (p = b.p) p->addref(); }
	ref_ptr(T *b = 0)							{ if (p = b) p->addref(); }
	~ref_ptr()									{ if (p) p->release(); 	}
	ref_ptr&	operator=(T *b)					{ set(b); return *this; }
	ref_ptr&	operator=(const ref_ptr &b)		{ set(b.p); return *this; }

#ifdef USE_RVALUE_REFS
	template<typename U> ref_ptr(ref_ptr<U> &&b)	: p(b.detach())		{}
	ref_ptr(ref_ptr &&b)	: p(b.detach())		{}
	ref_ptr(nullptr_t)		: p(0)				{}
	ref_ptr&	operator=(ref_ptr &&b)			{ swap(p, b.p); return *this; }
#endif

	operator T*()			const				{ return p; }
	T*		operator->()	const				{ return p; }
	T*		get()			const				{ return p; }
	void	clear()								{ if (p) { p->release(); p = 0; } }
	T*		detach()							{ T *t = p;	p = 0; return t; }

	friend void	swap(ref_ptr &a, ref_ptr &b)	{ swap(a.p, b.p); }
	friend T*	get(const ref_ptr &a)			{ return a; }
	friend T*	put(ref_ptr &a)					{ return a; }
};

template<typename T> bool operator==(const ref_ptr<T> &a, const ref_ptr<T> &b) { return a ? (b && *a == *b) : !b; }

template<typename T, typename R=uint32> class refs {
friend class ref_ptr<T>;
protected:
	R		nrefs;
	constexpr refs() : nrefs(0)		{}
	~refs()					{ ISO_ASSERT(nrefs == 0); }
public:
	T*		addref()		{ ++nrefs; return static_cast<T*>(this); }
	void	release()		{ ISO_ASSERT(nrefs); if (!--nrefs) delete static_cast<T*>(this); }
	void	addref(int n)	{ nrefs += n; }
	void	release(int n)	{ ISO_ASSERT(nrefs >= n); if (!(nrefs -= n)) delete static_cast<T*>(this); }
	bool	shared() const	{ return nrefs > 1; }
};

template<typename T, typename R=uint32> class with_refs : public refs<with_refs<T,R>, R> {
	T	t;
public:
	constexpr with_refs()				{}
	template<typename...P> constexpr with_refs(P&&...p) : t(forward<P>(p)...) {}
	template<typename U> T &operator=(U &&u)		{ return t = forward<U>(u); }
	constexpr T&			operator->()			{ return t; }
	constexpr const T&		operator->()	const	{ return t; }
	constexpr operator T&()							{ return t; }
	constexpr operator const T&()			const	{ return t; }
};

template<typename T> with_refs<noref_t<T>> make_with_refs(T &&t)	{ return forward<T>(t); }

template<typename T, typename B> class inherit_refs : public B {
friend class ref_ptr<T>;
protected:
	inherit_refs()			{}
	template<typename P> inherit_refs(P p)	: B(p) {}
	void	release()		{ if (!--this->nrefs) delete static_cast<T*>(this); }
};

template<typename T> struct	deleter<refs<T>>	{ void operator()(refs<T> *p)	{ p->release(); }};
template<typename T> auto	dup(refs<T> &t)		{ return t->addref(); }

//-----------------------------------------------------------------------------
//	saver
//-----------------------------------------------------------------------------

template<typename T> class saver {
	T	o, &r;
public:
	saver(T &t) : o(t), r(t)				{}
	template<typename N> saver(T &t, N &&n) : o(move(t)), r(t) { t = forward<N>(n); }
	saver(saver &&) = default;
	~saver()								{ r = move(o); }
	operator const T&()		const			{ return o; }
	const T&	original()	const			{ return o; }
};

template<typename T> force_inline saver<T> save(T &t)					{ return saver<T>(t); }
template<typename T, typename N> force_inline saver<T> save(T &t, N &&n)	{ return saver<T>(t, forward<N>(n)); }

//-----------------------------------------------------------------------------
//	recursion_checker
//-----------------------------------------------------------------------------

template<typename T> struct recursion_checker {
	struct entry {
		T		t;
		entry	*prev;
		recursion_checker *checker;
		entry(recursion_checker *_checker, const T &t) : t(t), checker(0) {
			if (!_checker->contains(t)) {
				checker = _checker;
				prev	= exchange(_checker->top, this);
			}
		}
		~entry() {
			if (checker)
				checker->top = prev;
		}
		operator bool() const { return !!checker; }
	};
	entry	*top;

	recursion_checker() : top(0) {}
	~recursion_checker() {
		for (auto *i = top; i; i = i->prev)
			i->checker = 0;
	}
	bool contains(const T &t) {
		for (auto *i = top; i; i = i->prev) {
			if (i->t == t)
				return true;
		}
		return false;
	}
	entry check(const T &t) {
		return entry(this, t);
	}
};

struct stack_depth {
	thread_local static int	depth;
	stack_depth(int max)	{ ISO_ALWAYS_ASSERT(++depth < max); }
	~stack_depth()			{ --depth; }
};


//-----------------------------------------------------------------------------
//	singleton
//-----------------------------------------------------------------------------

template<class T> struct singleton {
	static iso_export T&	single()			{ static T t; return t; }
	operator 	T&()					const	{ return single(); }
	T&			operator()()			const	{ return single(); }
//	T*			operator&()				const	{ return &single(); }
	T*			operator->()			const	{ return &single(); }
	T&			operator=(const T &t)	const	{ return single() = t; }
};

//-----------------------------------------------------------------------------
//	static_list
//-----------------------------------------------------------------------------

template<typename T> class static_list : protected singleton<T*> {
protected:
	using	singleton<T*>::single;
	T*		next;
public:
	static_list() {
		next		= single();
		single()	= static_cast<T*>(this);
	}
	~static_list() {
		T *p = single();
		if (this == p) {
			single() = next;
		} else while (p) {
			T *n = p->next;
			if (n == this) {
				p->next = next;
				return;
			}
			p = n;
		}
	}

	class iterator {
		T			*p;
	public:
		typedef struct forward_iterator_t iterator_category;
		typedef T	element, *pointer, &reference;
		iterator(T *p) : p(p)	{}
		operator T*()		const	{ return p; }
		T&			operator*()			const	{ return *p; }
		T*			operator->()		const	{ return p; }
		bool		operator==(T *p2)	const	{ return p == p2; }
		bool		operator!=(T *p2)	const	{ return p != p2; }
		iterator	operator++()				{ p = p->static_list<T>::next; return *this; }
		iterator	operator++(int)				{ iterator i = *this; p = p->next; return i; }
	};

	static range<iterator>	all()	{ return range<iterator>(single(), NULL); }
	static iterator	begin()			{ return single(); }
	static iterator	end()			{ return NULL; }
	static int		find(T *t)		{ int i = 0; for (T *j = single(); j && j != t; j = j->next) ++i; return i; }
	static T*		index(int i)	{ T *j = single(); while (j && i--) j = j->next; return j; }

	template<typename U> static T*	find_by(U &&u) {
		for (T *j = single(); j; j = j->next) {
			if (*j == u)
				return j;
		}
		return 0;
	}
};

template<typename T> class static_list_priority : public static_list<T> {
	int PRI;
public:
	static_list_priority(int PRI) : PRI(PRI) {
		auto	me	= static_cast<T*>(this);
		auto	p	= this->next, p0 = (decltype(p))nullptr;
		while (p && me->PRI < p->PRI) {
			p0	= p;
			p	= p->next;
		}
		if (p0) {
			p0->next = me;
			single() = exchange(this->next, p);
		}
	}
};

template<typename T, int _PRI> class static_list_priorityT : public T {
public:
	static_list_priorityT() : T(_PRI) {}
};

//-----------------------------------------------------------------------------
//	float types
//-----------------------------------------------------------------------------

enum float_category {
	FLOAT_NORMAL,
	FLOAT_QNAN,
	FLOAT_SNAN,
	FLOAT_INF,
	FLOAT_NEGINF,
};

template<
	typename F,
	uint32	_M	= num_traits<F>::mantissa_bits,
	uint32	_E	= num_traits<F>::exponent_bits,
	bool	S	= true,
	uint32	B	= _M + _E + int(S),
	typename I = uint_bits_t<B>
> struct float_storage {
	typedef	I	mant_t;
	static const int	E	= _E, E_OFF = (1 << (E - 1)) - 1,
						M	= _M, M64 = M, MB = sizeof(I) * 8;
	static const I
		E_MASK		= (1 << E) - 1,
		M_MASK		= (I(1) << M) - 1,
		S_MASK		= I(1) << (M + E),
		SIGNAL_MASK = I(1) << (M - 1)
	;

	union {
		struct { I m:M, e:E, s:1; };
		I	_i;
		F	_f;
	};
	static constexpr float_storage	max()			{ return {M_MASK, E_MASK - 1, 0}; }
	static constexpr float_storage	min()			{ return {0, 1, 0}; }
	static constexpr float_storage	eps()			{ return {0, E_OFF - M, 0}; }
	static constexpr float_storage	nan()			{ return {M_MASK, E_MASK, 0}; }
	static constexpr float_storage	inf()			{ return {0, E_MASK, 0}; }
	static constexpr float_storage	neg_inf()		{ return {0, E_MASK, 1}; }
	static constexpr float_storage	inf(bool neg)	{ return {0, E_MASK, neg}; }
	static constexpr float_storage	exp2(int e)		{ return {0, e < -E_OFF ? 0 : e + E_OFF, 0}; }

	float_storage() 		{}
	constexpr float_storage(F f)	: float_storage(reinterpret_cast<const float_storage&>(f))	{}
	constexpr float_storage(I i)	: float_storage(reinterpret_cast<const float_storage&>(i))	{}

#ifdef	ISO_BIGENDIAN
	constexpr float_storage(I m, int e, int s) : s(s), e(e), m(m) {}
#else
	constexpr float_storage(I m, int e, int s) : m(m), e(e), s(s) {}
#endif
	
	float_storage&		set(bool sign, int exp, I mant)	{ m = mant; e = exp + E_OFF; s = sign; return *this; }
	float_storage&		set_sign(bool _s)			{ s = int(_s);				return *this; }
	float_storage&		set_exp(int _e)				{ e = _e + E_OFF;			return *this; }
	float_storage&		set_mant(F _m)				{ m = float_storage(_m).m;	return *this; }
	float_storage&		add_exp(int _e)				{ e += _e;					return *this; }
	float_storage&		add_mant(int _m)			{ reinterpret_cast<I&>(*this) += _m; return *this; }

	constexpr I			i()					const	{ return _i; }
	constexpr F			f()					const	{ return _f; }

	constexpr bool		get_sign()			const	{ return s; }
	constexpr int		get_exp()			const	{ return e - E_OFF; }
	constexpr int		get_dexp()			const	{ return (e ? e : highest_set_index(m) - M) - E_OFF; }
	constexpr I			get_mant()			const	{ return m | (e ? (I(1) << M) : 0); }
	constexpr uint64	get_mant64()		const	{ return m; }
	constexpr F			get_mantf()			const	{ return float_storage(m, E_OFF, s).f(); }

	constexpr bool		is_special()		const	{ return e == E_MASK; }
	constexpr bool		is_nan()			const	{ return e == E_MASK && m; }
	constexpr bool		is_signalling_nan()	const	{ return is_nan() &&  (m & SIGNAL_MASK); }
	constexpr bool		is_quiet_nan()		const	{ return is_nan() && !(m & SIGNAL_MASK); }
	constexpr bool		is_inf()			const	{ return e == E_MASK && !m; }
	constexpr bool		is_denorm()			const	{ return e == 0 && m; }
	constexpr bool		is_zero()			const	{ return e == 0 && m == 0; }

	constexpr float_category get_category()	const	{ return e == E_MASK ? (m ? (m & SIGNAL_MASK ? FLOAT_SNAN : FLOAT_QNAN) : (s ? FLOAT_NEGINF : FLOAT_INF)) : FLOAT_NORMAL; }

	constexpr I				_fracmask()		const	{ return bits<I>(e < E_OFF ? M + E : e > M + E_OFF ? 0 : M - e + E_OFF); }
	static constexpr float_storage	_floor_helper(I i, I x, bool s)	{ return (i + (s ? x : 0)) & ~x; }
	static constexpr float_storage	_round_ne_helper(I i, I x)		{ return (i + ((i & (x * 2 + 1)) == (x + 1) / 2 ? 0 : (x + 1) / 2)) & ~x; }

	constexpr float_storage	trunc()			const	{ return i() & ~_fracmask(); }
	constexpr float_storage	floor()			const	{ return floor_helper(i(), _fracmask(), s); }
	constexpr float_storage	ceil()			const	{ return floor_helper(i(), _fracmask(), !s); }

	constexpr float_storage	round_ne()		const	{ return e < E_OFF ? float_storage(0, e == E_OFF - 1 && m ? E_OFF : 0, s) : _round_ne_helper(i(), _fracmask()); }
	constexpr I				monotonic()		const	{ return i() ^ (S_MASK - s) ^ S_MASK; }

	constexpr float_storage operator-()		const { return i() ^ S_MASK; }
};

template<typename F> struct float_components : float_storage<F> {
	typedef float_storage<F> B;
	typedef uint_for_t<F>	I;
	typedef sint_for_t<F>	S;

	static constexpr I	to_uint(F f)			{ return float_components(f + F(1<<B::M)).i() & B::M_MASK; }
	static constexpr S	to_int(F f)				{ S i = S(to_uint(f)); return f < 0 ? -i : i; }
	static constexpr F	to_float(I i)			{ return float_components(i | (B::M + B::E_OFF) << B::M).f() - F(1<<B::M); }
	static constexpr F	to_float(S i)			{ F f = to_float(I(i < 0 ? -i : i)); return i < 0 ? -f : f; }

	float_components() 		{}
	constexpr float_components(I m, int e, int s) : B(m, e, s) {}
	constexpr float_components(F f)	: B(reinterpret_cast<const B&>(f))	{}
	constexpr float_components(I i)	: B(reinterpret_cast<const B&>(i))	{}
};

template<typename F> float_components<F>	get_components(F f) { return f; }
template<typename F> constexpr auto			get_category(F f)	{ return get_components(f).get_category(); }

typedef float_components<float> iorf;
#if USE_64BITREGS
typedef float_components<double> iord;
#endif

force_inline	uint32	float2uint(float f)		{ return iorf::to_uint(f); }
force_inline	float	uint2float(uint32 i)	{ return iorf::to_float(i); }
force_inline	int		float2int(float f)		{ return iorf::to_int(f); }
force_inline	float	int2float(int32 i)		{ return iorf::to_float(i); }

// printing helper
template<typename T> struct float_info {
	float_category	cat;
	T				mant;
	int				frac;
	int				exp;
	bool			sign;
	float_info(float_category cat) : cat(cat) {}
	template<typename F> float_info(const F &comp, int exp) : cat(FLOAT_NORMAL), mant(comp.get_mant()), frac(comp.M - comp.get_exp()), exp(exp), sign(comp.get_sign()) {}
	float_info&	fix(int B) {
		if ((mant >> frac) >= B) {
			mant /= B;
			++exp;
		}
		return *this;
	}
};

template<int B, typename F> int get_exponent(F f) {
	auto	comps	= get_components(f);
	constexpr int ES = 31 - num_traits<F>::exponent_bits;
	return f ? (comps.get_dexp() * FLOG_BASE(B, 2, ES)) >> ES : 0;
}

template<int B, typename F> int get_scientific(F &f) {
	int	exp	= get_exponent<B>(f);
	f		= pow_mul(F(B), -exp, f);
	if (f >= B) {
		f /= B;
		++exp;
	}
	return exp;
}

template<int B, typename F> auto get_print_info(F f, int exp) -> float_info<decltype(get_components(f).get_mant())> {
	return {get_components(pow_mul(F(B), -exp, f)), exp};
}

template<int B, typename F> auto get_print_info(F f) -> float_info<decltype(get_components(f).get_mant())> {
	if (auto cat = get_category(f))
		return cat;
	int	exp		= get_exponent<B>(f);
	return {get_components(pow_mul(F(B), -exp, f)), exp};
}

//template<typename T> constexpr uint32	log2_floor(T x) { return float_components(if_t<sizeof(T) <= 4, float, double>(x)).get_exp(); }

//-----------------------------------------------------------------------------
//	number
//-----------------------------------------------------------------------------

struct number {
	enum TYPE : uint8 {	NAN, UINT, INT, FLT, DEC, TYPE_COUNT };
	enum SIZE : uint8 { SIZE8, SIZE16, SIZE32, SIZE64, SIZE80, SIZE_COUNT };
	int64	m;
	int		e:26;
	TYPE	type:3;
	SIZE	size:3;

	number	to_binary() const;

	constexpr number()			: m(0), e(0), type(NAN), size(SIZE8)	{}
	constexpr number(int64 m)	: m(m), e(0), type(INT), size(SIZE64)	{}
	constexpr number(int m)		: m(m), e(0), type(INT), size(SIZE32)	{}
	constexpr number(uint64 m)	: m(m), e(0), type(UINT), size(SIZE64)	{}
	constexpr number(uint32 m)	: m(m), e(0), type(UINT), size(SIZE32)	{}
	constexpr number(int64 m, int e, TYPE type, SIZE size = SIZE64)		: m(m), e(e), type(type), size(size) {}
	number(float f)		: type(FLT), size(SIZE32) { iorf t(f); m = t.get_mant() * (t.s ? -1 : 1); e = t.get_exp() - iorf::M; }
#ifdef USE_DOUBLE
	number(double f)	: type(FLT), size(SIZE64) { iord t(f); m = t.get_mant() * (t.s ? -1 : 1); e = t.get_exp() - iord::M; }
#endif

	constexpr bool		valid()		const { return type != NAN || m != 0; }
	constexpr bool		isfloat()	const { return type >= FLT; }
	constexpr operator	int64()		const { return type == DEC ? (int64)to_binary() : e < 0 ? m >> -e : m << e; }

	int64	fixed(uint32 frac, bool adj = false) const;

	template<typename F> float_components<F> get_comps() const {
		if (type == DEC)
			return to_binary().get_comps<F>();
		typedef float_components<F>	C;
		uint64	m2	= abs(m);
		int		i	= leading_zeros(m2);
		return C((((m2 << i) >> (62 - C::M)) + 1) >> 1, e - i + C::E_OFF + 63, m < 0).f();
	}
	operator float()			const	{ return get_comps<float>().f(); }
#ifdef USE_DOUBLE
	operator double()			const	{ return get_comps<double>().f(); }
#endif
	explicit operator bool()	const	{ return !!m; }
	bool	operator!()			const	{ return !m; }
};


//-----------------------------------------------------------------------------
//	table generation
//-----------------------------------------------------------------------------

template<typename A, typename B> struct table2 {
	constexpr static int	size()			{ return A::size() + B::size(); }
	constexpr static uint32	index(int i)	{ return i < A::size() ? A::index(i) : B::index(i - A::size()); }
};
template<uint32 A, uint32 B> struct table2<meta::constant<uint32, A>, meta::constant<uint32, B> > {
	constexpr static int	size()			{ return 2; }
	constexpr static uint32 index(int i)	{ return i == 0 ? A : B; }
};

template<typename T, class G, int N, int I> struct gen_table : table2<gen_table<T, G, N / 2, I>, gen_table<T, G, N - N / 2, I + N / 2> > {};

template<typename T, class G, int I> struct gen_table<T, G, 1, I> {
	constexpr static int	size()			{ return 1; }
	constexpr static uint32 index(int i)	{ return G::f(I); }
};

//-----------------------------------------------------------------------------
//	data layout
//-----------------------------------------------------------------------------
struct initialise { initialise(...); };

template<typename T> struct elided {
	elided() {}
	~elided() 										{ ((T*)this)->~T(); }
	constexpr elided(const elided &t)				{ new(this) T(*(T*)&t); }
	constexpr elided(elided &&t)					{ new(this) T(move(*(T*)&t)); }
	template<typename T2> constexpr elided(T2 &&t)	{ new(this) T(forward<T2>(t)); }
	template<typename T2> void operator=(T2 &&t)	{ *(T*)this = forward<T2>(t); }
};

template<typename T, int O>	struct offset_type : spacer<O> {
	typedef T			type;
	typedef noref_t<T>	T0;
	T			t;
	offset_type()		{}
	template<typename T2> constexpr offset_type(T2&& t2)		: t(forward<T2>(t2)) {}
	template<typename T2> constexpr offset_type(elided<T2> t2)	: offset_type((T2&)t2) {}
	template<typename T2> T& operator=(T2 &&t2)		{ return t = get(forward<T2>(t2)); }
	operator			const T0&()		const		{ return t; }
	operator			T0&()						{ return t; }
	T0&					operator->()				{ return t; }
	const T0&			operator->()	const		{ return t; }
	T0*					operator&()					{ return &t; }
	const T0*			operator&()		const		{ return &t; }

	template<typename B> auto&	operator+=(B b)		{ return t += b; }
	template<typename B> auto&	operator-=(B b)		{ return t -= b; }
	template<typename B> auto&	operator*=(B b)		{ return t *= b; }
	template<typename B> auto&	operator/=(B b)		{ return t /= b; }
	template<typename B> auto&	operator|=(B b)		{ return t |= b; }
	template<typename B> auto&	operator&=(B b)		{ return t &= b; }
	template<typename B> auto&	operator^=(B b)		{ return t ^= b; }

	friend const T0&	get(const offset_type &p)	{ return p; }
	friend T0&			put(offset_type &p)			{ return p; }
};

template<typename T, int O> struct num_traits<offset_type<T, O>> : num_traits<T> {};
template<typename T, int O> constexpr bool use_constants<offset_type<T,O>> = use_constants<T>;

template<typename T, int O>	struct offset_type_type 		: T_type<offset_type<T, O>> {};
template<typename T>		struct offset_type_type<T, 0> 	: T_type<T> {};
template<typename T, int O>	using offset_type_t = typename offset_type_type<T, O>::type;

template<int N, int O, typename... T>				struct tuple_offset_type;
template<int N, int O, typename T0, typename... T>	struct tuple_offset_type<N, O, T0, T...>	: tuple_offset_type<N - 1, sizeof(offset_type<T0, O>), T...> {};
template<int O, typename T0, typename... T>			struct tuple_offset_type<0, O, T0, T...>	: T_type<offset_type<T0, O>> {};
template<int N, typename... T>	using tuple_offset_t = typename tuple_offset_type<N, 0, T...>::type;

template<typename T, int N>	struct padded_type {
	typedef T			type;
	typedef noref_t<T>	T0;
	union {
		T			t;
		spacer<N>	_;
	};
	padded_type()		{}
	template<typename T2> padded_type(T2&& t2)		: t(forward<T2>(t2)) {}
	template<typename T2> void operator=(T2 &&t2)	{ t = forward<T2>(t2); }
	template<typename T2> auto operator*(const T2 &t2)	const { return t * t2; }
	operator			const T0&()		const		{ return t; }
	operator			T0&()						{ return t; }
	T0&					operator->()				{ return t; }
	const T0&			operator->()	const		{ return t; }
	T0*					operator&()					{ return &t; }
	const T0*			operator&()		const		{ return &t; }
	friend const T0&	get(const padded_type &p)	{ return p; }
	friend T0&			put(padded_type &p)			{ return p; }
};

template<typename T> using def_init = holder<T>;

template<typename T, int N> struct num_init : holder<T> {
	using holder<T>::holder;
	num_init() : holder<T>(N)	{}
};

template<typename T, typename... TT> union union_of;
template<typename T>	struct union_of_type;
template<typename...T>	struct union_of_type<type_list<T...>> : T_type<union_of<T...>> {};

template<typename T, typename... TT> union union_of {
	char			dummy;
	T				t;
	union_of<TT...>	u;

	union_of()	: dummy{} {}
	~union_of()	{}
	template<int N>	using			type = meta::VT_index_t<N, T, TT...>;
	template<int N>	auto&			get()				{ return ((typename union_of_type<meta::VT_right_t<sizeof...(TT) + 1 - N, T, TT...>>::type*)this)->t; }
	template<int N>	constexpr auto&	get()		const	{ return ((const typename union_of_type<meta::VT_right_t<sizeof...(TT) + 1 - N, T, TT...>>::type*)this)->t; }
	constexpr size_t				size(int i)	const	{ return i == 0 ? sizeof(T) : u.size(i - 1); }
};
template<typename T> union union_of<T> {
	T				t;
	constexpr size_t				size(int i)	const	{ return sizeof(T); }
};
template<template<class> class M, typename...TT> struct meta::map<M, union_of<TT...> > : T_type<union_of<map_t<M, TT>...>> {};

//-----------------------------------------------------------------------------
//	Time
//-----------------------------------------------------------------------------

class time : public _time {
public:
	type	t;
	time(type t) : t(t)	{}
public:
	static time		now()	{ return _time::now(); }
	time()			{}
	time(float f) : t(from_secs(f))	{}
	operator	float()	const				{ return to_secs(t); }
	time&		operator=(float f)			{ t = from_secs(f); return *this; }
	time&		operator+=(time b)			{ t = t + b.t; return *this; }

	friend time	operator+ (time a, time b)	{ return a.t + b.t; }
	friend time	operator- (time a, time b)	{ return a.t - b.t; }
	friend bool	operator==(time a, time b)	{ return a.t == b.t; }
	friend bool	operator!=(time a, time b)	{ return !(a.t == b.t); }
	friend bool	operator< (time a, time b)	{ return a.t <  b.t; }
	friend bool	operator<=(time a, time b)	{ return a.t <= b.t; }
	friend bool	operator> (time a, time b)	{ return b.t <  a.t; }
	friend bool	operator>=(time a, time b)	{ return b.t <= a.t; }
};

class timer : public _time {
	type	t;
public:
	timer() : t(now())			{}
	void		reset()				{ t = now(); }
	operator	float()	const		{ return to_secs(now() - t); }
	void		operator=(float f)	{ t = now() + from_secs(f); }
};

//-----------------------------------------------------------------------------

}//namespace iso

//-----------------------------------------------------------------------------
//	global functions
//-----------------------------------------------------------------------------

// need to be global for some reason

//template<int N> inline void *operator new(size_t size, iso::spacer<N> &a)	{ ISO_ASSERT(size <= N); return &a; }
//template<int N> inline void *operator new[](size_t size, iso::spacer<N> &a)	{ ISO_ASSERT(size <= N); return &a; }

#endif // DEFS_H
