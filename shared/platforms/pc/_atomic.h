#ifndef _ATOMIC_H
#define _ATOMIC_H

#include "base\defs.h"

namespace iso {

constexpr size_t CACHELINE_SIZE = 64;

template<int B> struct _atomic_type;
template<> struct _atomic_type<1>	{ typedef int8 type;	};
template<> struct _atomic_type<2>	{ typedef int16 type;	};
template<> struct _atomic_type<4>	{ typedef int32 type;	};
template<> struct _atomic_type<8>	{ typedef int64 type;	};
template<> struct _atomic_type<16>	{ typedef uint128 type;	};

#if defined(_M_IX86) || defined(_M_X64)
inline void		_atomic_pause()		{ _mm_pause(); }
#else
inline void		_atomic_pause()		{}
#endif

#ifdef __clang__
inline void		_atomic_barrier()	{ __atomic_thread_fence(__ATOMIC_ACQUIRE); }
inline void		_atomic_fence()		{ __atomic_thread_fence(__ATOMIC_RELEASE); }
#else
inline void		_atomic_barrier()	{ _ReadBarrier(); }
inline void		_atomic_fence()		{ long dummy; InterlockedAdd(&dummy, 0); }
#endif

template<typename T> inline T		_atomic_load(T *p)					{ return *p; }
template<typename T> inline T		_atomic_load_acquire(T *p)			{ return *p; }
template<typename T> inline void	_atomic_store_release(T *p, T a)	{ *p = a; }

// 8 bit
inline int8		_cas_val(int8 *p, int8 a, int8 b)				{ return _InterlockedCompareExchange8((char*)p, b, a); }
inline int8		_cas_acquire_val(int8 *p, int8 a, int8 b)		{ return _cas_val(p, a, b); }
inline int8		_cas_release_val(int8 *p, int8 a, int8 b)		{ return _cas_val(p, a, b); }
#if defined _M_ARM64 || defined _M_ARM
inline int16	_atomic_exch(int8 *p, int8 a)					{ return _InterlockedExchange8_acq((char*)p, a); }
inline void		_atomic_store(int8 *p, int8 a)					{ _InterlockedExchange8_rel((char*)p, a); }
#else
inline int16	_atomic_exch(int8 *p, int8 a)					{ return InterlockedExchange8((char*)p, a); }
inline void		_atomic_store(int8 *p, int8 a)					{ InterlockedExchange8((char*)p, a); }
#endif
//inline int16	_atomic_inc(int8 *p)							{ return InterlockedIncrement8(p) - 1; }
//inline int16	_atomic_dec(int8 *p)							{ return InterlockedDecrement8(p) + 1; }
//inline int16	_atomic_add(int8 *p, int8 a)					{ return InterlockedAdd8(p, a) - a; }
inline int8		_atomic_and(int8 *p, int8 a)					{ return InterlockedAnd8((char*)p, a); }
inline int8		_atomic_or(int8	*p, int8 a)						{ return InterlockedOr8((char*)p, a); }
inline int8		_atomic_xor(int8 *p, int8 a)					{ return InterlockedXor8((char*)p, a); }

// 16 bit
inline int16	_cas_val(int16 *p, int16 a, int16 b)			{ return InterlockedCompareExchange16(p, b, a); }
inline int16	_cas_acquire_val(int16 *p, int16 a, int16 b)	{ return InterlockedCompareExchangeAcquire16(p, b, a); }
inline int16	_cas_release_val(int16 *p, int16 a, int16 b)	{ return InterlockedCompareExchangeRelease16(p, b, a); }
inline void		_atomic_store(int16 *p, int32 a)				{ InterlockedExchange16(p, a); }
inline int16	_atomic_exch(int16 *p, int32 a)					{ return InterlockedExchange16(p, a); }
inline int16	_atomic_inc(int16 *p)							{ return InterlockedIncrement16(p) - 1; }
inline int16	_atomic_dec(int16 *p)							{ return InterlockedDecrement16(p) + 1; }
//inline int16	_atomic_add(int16 *p, int16 a)					{ return InterlockedAdd16(p, a) - a; }
inline int16	_atomic_and(int16 *p, int16 a)					{ return InterlockedAnd16(p, a); }
inline int16	_atomic_or(int16 *p, int16 a)					{ return InterlockedOr16(p, a); }
inline int16	_atomic_xor(int16 *p, int16 a)					{ return InterlockedXor16(p, a); }

// 32 bit
inline int32	_cas_val(int32 *p, int32 a, int32 b)			{ return InterlockedCompareExchange((long*)p, b, a); }
inline int32	_cas_acquire_val(int32 *p, int32 a, int32 b)	{ return InterlockedCompareExchangeAcquire((long*)p, b, a); }
inline int32	_cas_release_val(int32 *p, int32 a, int32 b)	{ return InterlockedCompareExchangeRelease((long*)p, b, a); }
inline void		_atomic_store(int32 *p, int32 a)				{ InterlockedExchange((long*)p, a); }
inline int32	_atomic_exch(int32 *p, int32 a)					{ return InterlockedExchange((long*)p, a); }
inline int32	_atomic_inc(int32 *p)							{ return InterlockedIncrement((long*)p) - 1; }
inline int32	_atomic_dec(int32 *p)							{ return InterlockedDecrement((long*)p) + 1; }
inline int32	_atomic_add(int32 *p, int32 a)					{ return InterlockedAdd((long*)p, a) - a; }
inline int32	_atomic_and(int32 *p, int32 a)					{ return InterlockedAnd((long*)p, a); }
inline int32	_atomic_or(int32	*p, int32 a)				{ return InterlockedOr((long*)p, a); }
inline int32	_atomic_xor(int32 *p, int32 a)					{ return InterlockedXor((long*)p, a); }

// 64 bit
inline int64	_cas_val(int64 *p, int64 a, int64 b)			{ return InterlockedCompareExchange64(p, b, a); }
inline int64	_cas_acquire_val(int64 *p, int64 a, int64 b)	{ return InterlockedCompareExchangeAcquire64(p, b, a); }
inline int64	_cas_release_val(int64 *p, int64 a, int64 b)	{ return InterlockedCompareExchangeRelease64(p, b, a); }
inline void		_atomic_store(int64 *p, int64 a)				{ InterlockedExchange64(p, a); }
inline int64	_atomic_exch(int64 *p, int64 a)					{ return InterlockedExchange64(p, a); }
inline int64	_atomic_inc(int64 *p)							{ return InterlockedIncrement64(p) - 1; }
inline int64	_atomic_dec(int64 *p)							{ return InterlockedDecrement64(p) + 1; }
inline int64	_atomic_add(int64 *p, int64 a)					{ return InterlockedAdd64(p, a) - a; }
inline int64	_atomic_and(int64 *p, int64 a)					{ return InterlockedAnd64(p, a); }
inline int64	_atomic_or(int64 *p, int64 a)					{ return InterlockedOr64(p, a); }
inline int64	_atomic_xor(int64 *p, int64 a)					{ return InterlockedXor64(p, a); }

// 128 bit
inline bool		_cas_exch(uint128 *p, uint128 &a, const uint128 &b)			{ return InterlockedCompareExchange128((__int64*)p, ((__int64*)&b)[1], ((__int64*)&b)[0], (int64*)&a); }
inline uint128	_cas_val(uint128 *p, const uint128 &a, const uint128 &b)	{ uint128 t = a; _cas_exch(p, t, b); return t; }
inline bool		_cas(uint128 *p, const uint128 &a, const uint128 &b)		{ uint128 t = a; return _cas_exch(p, t, b); }
inline void		_atomic_store(uint128 *p, uint128 a)						{ uint128 b = *p; while (!_cas_exch(p, b, a)); }//	*p = a; _atomic_fence(); }

inline bool		_atomic_test_set_bit(int32 *p, int bit)				{ return !!_interlockedbittestandset((long*)p, bit); }
inline bool		_atomic_test_set_bit(int64 *p, int bit)				{ return !!_interlockedbittestandset64(p, bit); }

inline bool		_atomic_test_clear_bit(int32 *p, int bit)			{ return !!_interlockedbittestandreset((long*)p, bit); }
inline bool		_atomic_test_clear_bit(int64 *p, int bit)			{ return !!_interlockedbittestandreset64(p, bit); }

template<typename T> inline T		_cas_val(T *p, T a, T b) {
	typedef typename _atomic_type<sizeof(T)>::type	T2;
	return _cas_val((T2*)p, (T2&)a, (T2&)b);
}

template<typename T> inline bool	_cas(T *p, T a, T b)			{ return _cas_val(p, a, b) == a; }
template<typename T> inline bool	_cas_acquire(T *p, T a, T b)	{ return _cas_acquire_val(p, a, b) == a; }
template<typename T> inline bool	_cas_release(T *p, T a, T b)	{ return _cas_release_val(p, a, b) == a; }

} // namespace iso

#endif //_ATOMIC_H
