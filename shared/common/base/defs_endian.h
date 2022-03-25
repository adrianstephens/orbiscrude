#ifndef DEFS_ENDIAN_H
#define DEFS_ENDIAN_H

#include "defs_base.h"

namespace iso {

//-----------------------------------------------------------------------------
//	endian conversion
//-----------------------------------------------------------------------------

template<typename T, int N> struct s_reverse_bytes {
	static const T mask = T(~T(0)) / ((T(1) << (N * 4)) + 1);	// N bits every 2N
	static constexpr T f(T x) { return s_reverse_bytes<T, N / 2>::f(((x & mask) << (N*4)) | ((x >> (N*4)) & mask)); }
};
template<typename T> struct s_reverse_bytes<T,1> { static constexpr T f(T x) { return x; } };
template<typename T> constexpr T swap_endian_const(T x)		{ return s_reverse_bytes<uint_for_t<T>, sizeof(T)>::f(x); }
template<typename T> constexpr T endian_const(T x, bool be)	{ return be == iso_bigendian ? x : swap_endian_const(x); }

constexpr uint8	swap_endian(uint8 i) {
	return i;
}

#if defined(__GNUC__)
constexpr uint16	swap_endian(uint16 i) {	return __builtin_bswap16(i);}
constexpr uint32	swap_endian(uint32 i) {	return __builtin_bswap32(i);}
constexpr uint64	swap_endian(uint64 i) {	return __builtin_bswap64(i);}
#elif 0//defined(PLAT_PC)
constexpr uint16	swap_endian(uint16 i) {	return _byteswap_ushort(i);}
constexpr uint32	swap_endian(uint32 i) {	return _byteswap_ulong(i);}
constexpr uint64	swap_endian(uint64 i) {	return _byteswap_uint64(i);}
#else
constexpr uint16	swap_endian(uint16 i) {
	return (i >> 8) | (i << 8);
}
constexpr uint32	swap_endian(uint32 i) {
	uint32	m = 0x0000ffffL;
	i	= ((i & m) << 16) | ((i & ~m) >> 16);
	m	= m ^ (m << 8);
	return((i & m) <<  8) | ((i & ~m) >>  8);
}
constexpr uint64	swap_endian(uint64 i) {
	uint64	m = 0x00000000ffffffffL;
	i	= ((i & m) << 32) | ((i & ~m) >> 32);
	m	= m ^ (m << 16);
	i	= ((i & m) << 16) | ((i & ~m) >> 16);
	m	= m ^ (m << 8);
	return((i & m) <<  8) | ((i & ~m) >>  8);
}
#endif

constexpr uint128		swap_endian(uint128 i)		{ return lo_hi(swap_endian(hi(i)), swap_endian(lo(i))); }

template<typename T> constexpr T	swap_endian(T t)				{ return force_cast<T>(swap_endian((uint_for_t<T>&)t)); }
template<typename T> constexpr T	endian(T t, bool be)			{ return be == iso_bigendian ? t : swap_endian(t); }
//template<typename T> constexpr void	swap_endian_inplace(T &t)		{ (uint_for_t<T>&)t = swap_endian((uint_for_t<T>&)t); }
template<typename...T> inline void	swap_endian_inplace(T&...t)		{ discard(((uint_for_t<T>&)t = swap_endian((uint_for_t<T>&)t))...); }
template<typename...T> inline void	endian_inplace(bool be, T&...t)	{ if (be != iso_bigendian) swap_endian_inplace(t...); }

//template<typename T> constexpr	auto	native_endian(const constructable<T> &t)	{ return t; };

template<typename T> class T_swap_endian {
protected:
	typedef	uint_for_t<T>	S;
	S	i;
	void				set(const T &b)					{ i = swap_endian((S&)b); }
public:
	constexpr operator	T()		const					{ return force_cast<T>(swap_endian(i)); }
	T_swap_endian		&operator=(const T &b)			{ set(b); return *this; }
	constexpr T			get()	const					{ return force_cast<T>(swap_endian(i)); }
	constexpr S			raw()	const					{ return i; }
	friend constexpr T	get(const T_swap_endian &a)		{ return a; }

	template<typename B> friend T_swap_endian& operator+=(T_swap_endian &a, const B &b)	{ return a = a + b;	}
	template<typename B> friend T_swap_endian& operator-=(T_swap_endian &a, const B &b)	{ return a = a - b;	}
	template<typename B> friend T_swap_endian& operator*=(T_swap_endian &a, const B &b)	{ return a = a * b;	}
	template<typename B> friend T_swap_endian& operator/=(T_swap_endian &a, const B &b)	{ return a = a / b;	}
	template<typename B> friend T_swap_endian& operator%=(T_swap_endian &a, const B &b)	{ return a = a % b;	}
	template<typename B> friend T_swap_endian& operator&=(T_swap_endian &a, const B &b)	{ return a = a ^ b; }
	template<typename B> friend T_swap_endian& operator|=(T_swap_endian &a, const B &b)	{ return a = a ^ b; }
	template<typename B> friend T_swap_endian& operator^=(T_swap_endian &a, const B &b) { return a = a ^ b; }
	template<typename B> friend T_swap_endian& operator++(T_swap_endian &a)				{ return a = a + 1; }
	template<typename B> friend T_swap_endian& operator--(T_swap_endian &a)				{ return a = a - 1; }
	template<typename B> friend T operator++(T_swap_endian &a, int)						{ T t = a; a = t + 1; return t; }
	template<typename B> friend T operator--(T_swap_endian &a, int)						{ T t = a; a = t - 1; return t; }

	template<typename P> friend P* operator+(P *p, const T_swap_endian &i)				{ return p + i.get();  }
};

template<typename T> struct packed<constructable<T_swap_endian<T> > > : _packed<T_swap_endian<T> > {
	typedef	_packed<T_swap_endian<T> >	B;
	packed()									{}
	packed(const T &_t)			{ B::set(_t); }
	operator T_swap_endian<T>()	const			{ return B::get(); }
	operator T()				const			{ return B::get(); }
	template<typename T2> void operator=(const T2 &_t)	{ B::operator=(_t); }
	friend T	get(const packed &a)			{ return a; }
};

template<>					struct T_swap_endian_type<uint8>				: T_type<uint8> {};
template<typename T>		struct T_swap_endian_type<constructable<T> >	: T_swap_endian_type<T> {};
template<typename T, int N>	struct T_swap_endian_type<T[N]>					{ typedef typename T_swap_endian_type<T>::type type[N]; };

template<typename T>		struct T_native_endian							: T_type<T> {};
template<typename T>		struct T_native_endian<constructable<T>>		: T_native_endian<T> {};
template<typename T>		struct T_native_endian<T_swap_endian<T>>		: T_type<T> {};
template<typename T>		using native_endian_t	= typename T_native_endian<T>::type;

template<typename T>		struct T_underlying<T_swap_endian<T>>			: T_underlying<T>	{};
template<typename T>		struct T_swap_endian<constructable<T>>			: T_swap_endian<T>	{};
template<typename T>		struct T_isint<T_swap_endian<T>>				: T_isint<T>		{};
template<typename T>		struct num_traits<T_swap_endian<T>>				: num_traits<T>		{};

template<typename T>		struct T_constructable_swap_endian_type			: T_type<constructable<typename T_swap_endian_type<T>::type>> {};
template<>					struct T_constructable_swap_endian_type<uint8>	: T_type<uint8> {};

template<typename T>		struct T_is_native_endian						: T_true				{};
template<typename T>		struct T_is_native_endian<constructable<T>>		: T_is_native_endian<T> {};
template<typename T>		struct T_is_native_endian<T_swap_endian<T>>		: T_false				{};

template<typename T> constexpr const T& native_endian(const T &t)				{ return t; }
template<typename T> constexpr T		native_endian(T_swap_endian<T> t)		{ return t; }
template<typename T> constexpr auto		native_endian(constructable<T> t)		{ return native_endian((T)t); }
template<typename T> constexpr bool		is_native_endian(T t)					{ return true; }
template<typename T> constexpr bool		is_native_endian(T_swap_endian<T> t)	{ return false; }
template<typename T> constexpr bool		is_native_endian()						{ return T_is_native_endian<T>::value; }
template<typename T> constexpr bool		is_bigendian()							{ return is_native_endian<T>() == iso_bigendian; }

template<typename T, bool be>	using endian_t	= typename T_if<is_bigendian<T>() == be, T, swap_endian_t<T>>::type;


#ifdef	ISO_BIGENDIAN
#define BE(x)		x
#define LE(x)		iso::T_constructable_swap_endian_type<x>::type
#else
#define LE(x)		x
#define BE(x)		iso::T_constructable_swap_endian_type<x>::type
#endif

typedef BE(uint16)	uint16be;
typedef BE(uint32)	uint32be;
typedef BE(uint64)	uint64be;
typedef BE(int16)	int16be;
typedef BE(int32)	int32be;
typedef BE(int64)	int64be;
typedef BE(float32)	float32be;
typedef BE(float64)	float64be;
typedef BE(float)	floatbe;
typedef BE(double)	doublebe;
typedef BE(xint16)	xint16be;
typedef BE(xint32)	xint32be;
typedef BE(xint64)	xint64be;

typedef LE(uint16)	uint16le;
typedef LE(uint32)	uint32le;
typedef LE(uint64)	uint64le;
typedef LE(int16)	int16le;
typedef LE(int32)	int32le;
typedef LE(int64)	int64le;
typedef LE(float32)	float32le;
typedef LE(float64)	float64le;
typedef LE(float)	floatle;
typedef LE(double)	doublele;
typedef LE(xint16)	xint16le;
typedef LE(xint32)	xint32le;
typedef LE(xint64)	xint64le;

namespace bigendian_ns {
	using iso::int8;
	using iso::uint8;
	typedef	iso::uint16be	uint16;
	typedef	iso::uint32be	uint32;
	typedef	iso::uint64be	uint64;
	typedef	iso::int16be	int16;
	typedef	iso::int32be	int32;
	typedef	iso::int64be	int64;
	typedef	iso::float32be	float32;
	typedef	iso::float64be	float64;
}

namespace littleendian_ns {
	using iso::int8;
	using iso::uint8;
	typedef	iso::uint16le	uint16;
	typedef	iso::uint32le	uint32;
	typedef	iso::uint64le	uint64;
	typedef	iso::int16le	int16;
	typedef	iso::int32le	int32;
	typedef	iso::int64le	int64;
	typedef	iso::float32le	float32;
	typedef	iso::float64le	float64;
}

//-----------------------------------------------------------------------------
//	bytefields
//-----------------------------------------------------------------------------

// read/write
template<bool be>	inline void		_write_bytes(void *p, const void *x, int n, int sx)			{ n = min(n, sx); memcpy(p, (const char*)x + sx - n, n); }
template<>			inline void		_write_bytes<false>(void *p, const void *x, int n, int sx)	{ n = min(n, sx); memcpy(p, x, n); }
template<typename T> inline void	write_bytes(void *p, T x, int n)							{ _write_bytes<is_bigendian<T>()>(p, &x, n, sizeof(x)); }

template<bool be>	inline void		_read_bytes(const void *p, int n, void *x, int sx)			{ n = min(n, sx); memcpy((char*)x + sx - n, p, n); }
template<>			inline void		_read_bytes<false>(const void *p, int n, void *x, int sx)	{ n = min(n, sx); memcpy(x, p, n); }
template<typename T> inline T		read_bytes(const void *p, int n)							{ T x = 0; _read_bytes<is_bigendian<T>()>(p, n, &x, sizeof(x)); return s_extend<num_traits<T>::is_signed>::f(x, n * 8); }

//with fixed n
template<typename T, int N, bool S = (BIT_COUNT<T> == N)> struct read_write_bytes_s;
template<typename T, int N> struct read_write_bytes_s<T, N, false> {
	static inline T		read(const void *p)		{ return read_bytes<T>(p, N); }
	static inline void	write(void *p, T x)		{ write_bytes<T>(p, x, N); }
};
template<typename T, int N> struct read_write_bytes_s<T, N, true> {
	static inline T		read(const void *p)		{ return *(const T*)p; }
	static inline void	write(void *p, T x)		{ *(T*)p = x; }
};
template<typename T, int N>			inline T	read_bytes(const void *p)		{ return read_write_bytes_s<T, N>::read(p); }
template<typename T, typename U>	inline T	read_bytes(const U &u)			{ return read_bytes<T, sizeof(U)>(&u); }
template<typename T, int N>			inline void	write_bytes(void *p, T x)		{ return read_write_bytes_s<T, N>::write(p, x); }
template<typename T, typename U>	inline void	write_bytes(const U &u, T x)	{ return read_write_bytes_s<T, sizeof(U)>(&u, x); }

}//namespace iso

#endif // DEFS_ENDIAN_H
