#ifndef FNV_H
#define FNV_H

#include "base/bits.h"
//#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	FNV - Fowler–Noll–Vo hash function
//-----------------------------------------------------------------------------

template<typename T> struct FNV_vals;
template<> struct FNV_vals<uint32>	{ static const uint32 prime = 0x01000193U,			basis = 0x811C9DC5U; };
template<> struct FNV_vals<uint64>	{ static const uint64 prime = 0x0100000001B3ULL,	basis = 0xcbf29ce484222325ULL; };
//template<> struct FNV_vals<uint128>	{ static const uint128 prime = 0x0100000001B3_u128,	basis = 0x6c62272e07bb014262b821756295c58d_u128; };

template<typename T, typename W> inline T _FNV(const W *buffer, size_t count, T hash = FNV_vals<T>::basis, T prime = FNV_vals<T>::prime) {
	for (const W *p = buffer, *e = p + count; p < e; ++p)
		hash = T((hash * prime) ^ *p);
	return hash;
}
template<typename T, typename W> inline T _FNV_zt(const W *buffer, T hash = FNV_vals<T>::basis, T prime = FNV_vals<T>::prime) {
	while (W c = *buffer++)
		hash = T(hash * prime) ^ c;
	return hash;
}
template<typename T, typename W> inline T _FNVa(const W *buffer, size_t count, T hash = FNV_vals<T>::basis, T prime = FNV_vals<T>::prime) {
	for (const W *p = buffer, *e = p + count; p < e; ++p)
		hash = T((hash ^ *p) * prime);
	return hash;
}
template<typename T, typename W> inline T _FNVa_zt(const W *buffer, T hash = FNV_vals<T>::basis, T prime = FNV_vals<T>::prime) {
	while (W c = *buffer++)
		hash = T((hash ^ c) * prime);
	return hash;
}

template<typename T, typename W> constexpr T FNV_const(const W *buffer, size_t count, size_t i = 0, T hash = FNV_vals<T>::basis, T prime = FNV_vals<T>::prime) {
	return i == count ? hash : FNV_const(buffer, count, i + 1, T((hash * prime) ^ buffer[i]), prime);
}
template<typename T, typename W> constexpr T FNVa_const(const W *buffer, size_t count, size_t i = 0, T hash = FNV_vals<T>::basis, T prime = FNV_vals<T>::prime) {
	return i == count ? hash : FNVa_const(buffer, count, i + 1, T((hash ^  buffer[i]) * prime),  prime);
}
template<typename T, typename W, size_t N> constexpr T FNV_const(const W (&buffer)[N], T hash = FNV_vals<uint32>::basis, T prime = FNV_vals<T>::prime) {
	return FNV_const(buffer, N - 1, 0, hash, prime);
}
template<typename T, typename W, size_t N> constexpr T FNVa_const(const W (&buffer)[N], T hash = FNV_vals<uint32>::basis, T prime = FNV_vals<T>::prime) {
	return FNVa_const(buffer, N - 1, 0, hash, prime);
}

uint32 constexpr operator"" _fnv(const char* s, size_t len)					{ return FNV_const<uint32>(s, len); }
uint32 constexpr operator"" _hash(const char* s, size_t len)				{ return FNV_const<uint32>(s, len); }

template<typename T> constexpr T FNV_basis(T prime = FNV_vals<T>::prime)	{ return FNV_const<T>("chongo <Landon Curt Noll> /\\../\\", 0, prime);}

// FNV0 - no basis
template<typename T> T FNV0(const void *buffer, size_t len)					{ return buffer && len ? _FNV((const uint8*)buffer, len, T(0)) : 0;}
template<typename T> T FNV0(const char *buffer)								{ return buffer ? _FNV(buffer, T(0)) : 0;}
template<typename T, typename X> T FNV0(const X &x)							{ return FNV0<T>(&x, sizeof(X)); }
template<typename T, typename W> T FNV0_str(const W *buffer, size_t len)	{ return buffer && len ? _FNV(buffer, len, T(0)) : 0; }

// FNV1 - start with basis
template<typename T> T FNV1(const void *buffer, size_t len)					{ return buffer && len ? _FNV((const uint8*)buffer, len, FNV_vals<T>::basis) : 0; }
template<typename T, typename W> T FNV1(const W *buffer)					{ return buffer ? _FNV_zt(buffer, FNV_vals<T>::basis) : 0; }
template<typename T, typename X> T FNV1(const X &x)							{ return FNV1<T>((uint8*)&x, sizeof(X)); }
template<typename T, typename W> T FNV1(const W *buffer, size_t len)		{ return buffer && len ? _FNV(buffer, len, FNV_vals<T>::basis) : 0; }

// FNV1a - ^, then *
template<typename T> T FNV1a(const void *buffer, size_t len)				{ return buffer && len ? _FNVa((const uint8*)buffer, len, FNV_vals<T>::basis) : 0; }
template<typename T, typename W> T FNV1a(const W *buffer)					{ return buffer ? _FNVa_zt(buffer, FNV_vals<T>::basis) : 0; }
template<typename T, typename X> T FNV1a(const X &x)						{ return FNV1a<T>((uint8*)&x, sizeof(X)); }
template<typename T, typename W> T FNV1a(const W *buffer, size_t len)		{ return buffer && len ? _FNVa(buffer, len, FNV_vals<T>::basis) : 0; }

// non power of 2 bits
template<int N, typename T> constexpr T	xor_fold(T i)			{ return (i ^ (i >> N)) & bits(N); }
template<> constexpr uint32				xor_fold<32>(uint32 i)	{ return i; }
template<> constexpr uint64				xor_fold<64>(uint64 i)	{ return i; }

template<int N> using fnv_type = typename T_if<(N<=32), uint32, uint64>::type;
template<int N, typename W>	fnv_type<N> FNV0(const W *buffer, size_t len)	{ return buffer ? xor_fold<N>(_FNV(buffer, len, fnv_type<N>(0))) : 0; }
template<int N, typename W>	fnv_type<N> FNV0(const W *buffer)				{ return buffer ? xor_fold<N>(_FNV(buffer, fnv_type<N>(0))) : 0; }
template<int N, typename W>	fnv_type<N> FNV1(const W *buffer, size_t len)	{ return buffer && len ? xor_fold<N>(_FNV(buffer, len, FNV_vals<fnv_type<N>>::basis)) : 0; }
template<int N, typename W>	fnv_type<N> FNV1(const W *buffer)				{ return buffer ? xor_fold<N>(_FNV(buffer, FNV_vals<fnv_type<N>>::basis)) : 0; }
template<int N, typename W>	fnv_type<N> FNV1a(const W *buffer, size_t len)	{ return buffer && len ? xor_fold<N>(_FNVa(buffer, len, FNV_vals<fnv_type<N>>::basis)) : 0; }
template<int N, typename W>	fnv_type<N> FNV1a(const W *buffer)				{ return buffer ? xor_fold<N>(_FNVa(buffer, FNV_vals<fnv_type<N>>::basis)) : 0; }

template<int N, bool BASIS = true, bool A = false> struct FNV {
	typedef fnv_type<N>	CODE;
	CODE	state;

	FNV() : state(BASIS ? FNV_vals<CODE>::basis : 0)	{}
	void	reset()					{ state = BASIS ? FNV_vals<CODE>::basis : 0; }
	size_t	operator()(const void *data, size_t size) {
		state = A ? _FNVa((const uint8*)data, size, state) : _FNV((const uint8*)data, size, state);
		return (int)size;
	}
	CODE	terminate()	const		{ return state; }
	CODE	digest()	const		{ return terminate(); }
};

/*
template<int N, bool BASIS = true, bool A = false> struct FNV : writer_mixin<FNV<N>> {
	typedef fnv_type<N>	CODE;
	CODE	state;

	FNV() : state(BASIS ? FNV_vals<CODE>::basis : 0)	{}
	void			reset()					{ state = BASIS ? FNV_vals<CODE>::basis : 0; }
	size_t			writebuff(const void *data, size_t size) {
		state = A ? _FNVa((const uint8*)data, size, state) : _FNV((const uint8*)data, size, state);
		return (int)size;
	}
	CODE			terminate()	const		{ return state; }
	CODE			digest()	const		{ return terminate(); }
	operator CODE()				const		{ return terminate(); }
};
*/
} // namespace iso

#endif	// FNV_H
