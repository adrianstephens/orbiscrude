#ifndef _ATOMIC_H
#define _ATOMIC_H

#include "base/defs.h"

namespace iso {

constexpr size_t CACHELINE_SIZE = 64;

template<int B> struct _atomic_type;
template<> struct _atomic_type<1>	{ typedef int8 type;	};
template<> struct _atomic_type<2>	{ typedef int16 type;	};
template<> struct _atomic_type<4>	{ typedef int32 type;	};
template<> struct _atomic_type<8>	{ typedef int64 type;	};

inline void		_atomic_pause()		{}
inline void		_atomic_barrier()	{ __atomic_thread_fence(__ATOMIC_ACQUIRE); }
inline void		_atomic_fence()		{ __atomic_thread_fence(__ATOMIC_RELEASE); }

template<typename T> inline T		_atomic_load(T *p)						{ return __atomic_load_n(p, __ATOMIC_RELAXED); }
template<typename T> inline T		_atomic_load_acquire(T *p)				{ return __atomic_load_n(p, __ATOMIC_ACQUIRE); }

//template<typename T> inline void	_atomic_store(T *p, T a)				{ *p = a; }
template<typename T> inline void	_atomic_store(T *p, T a)				{ __atomic_store_n(p, a, __ATOMIC_RELAXED); }
template<typename T> inline void	_atomic_store_release(T *p, T a)		{ __atomic_store_n(p, a, __ATOMIC_RELEASE); }

template<typename T> inline bool 	_cas(volatile T *p, T a, T b)			{ return __atomic_compare_exchange_n(p, &a, b, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED); }
template<typename T> inline bool 	_cas_acquire(volatile T *p, T a, T b)	{ return __atomic_compare_exchange_n(p, &a, b, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED); }
template<typename T> inline bool 	_cas_release(volatile T *p, T a, T b)	{ return __atomic_compare_exchange_n(p, &a, b, true, __ATOMIC_RELEASE, __ATOMIC_RELAXED); }

template<typename T> inline T		_cas_val(T *p, T a, T b)	{ __atomic_compare_exchange_n(p, &a, b, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED); return a; }

} // namespace iso

#endif //_ATOMIC_H
