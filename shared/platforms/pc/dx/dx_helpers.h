#ifndef DX_H
#define DX_H

#include "base/defs.h"
#include "base/pointer.h"
#include "com.h"
#include "allocators/allocator.h"

namespace iso {

struct sized_data {
	const void	*p;
	sized_data(const void *_p) : p(_p) {}
	uint32		length()			const { return p ? ((uint32*)p)[-1] & 0x7fffffff : 0; }
	operator const_memory_block()	const { return const_memory_block(p, length()); }
	operator const void*()			const { return p; }
	template<typename T> operator const T*() const { return (const T*)p; }
};
iso_export vallocator&	allocator32();

#ifdef ISO_PTR64
//-----------------------------------------------------------------------------
//	64 bit
//-----------------------------------------------------------------------------

ISO_COMPILEASSERT(sizeof(void*) == 8);

template<typename T> struct _indirect32 {
	pointer32<T>	p;
	T&	get() {
		if (!p)
			p = new(allocator32()) T();
		return *p;
	}
	T&	get() const {
		ISO_ASSERT(!!p);
		return *p;
	}
	_indirect32()										{}
	~_indirect32()										{ allocator32().del(p.get());		}
	template<typename T2> _indirect32(const T2 &t2)		{ p = new(allocator32()) T(t2);	}
	template<typename T2> void operator=(const T2 &t2)	{ get() = t2;		}
	template<typename T2> void operator=(T2 &t2)		{ get() = t2;		}
	operator	T&()									{ return get();		}
	T&			operator->()							{ return get();		}
	T*			operator&()								{ return &get();	}
};

template<typename T> struct indirect32 : _indirect32<T> {
	indirect32()										{}
	template<typename T2> indirect32(const T2 &t2) : _indirect32<T>(t2) {}
};

template<typename T> struct indirect32<com_ptr<T> > : _indirect32<com_ptr<T> > {
	indirect32()										{}
	template<typename T2> indirect32(const T2 &t2) : _indirect32<com_ptr<T> >(t2) {}
	operator	T*()			const					{ return this->get();		}
	T*			operator->()	const					{ return this->get();		}
	T**			operator&()								{ return &this->get();	}
	T* const*	operator&()		const					{ return &this->get();	}
};

// overwrite original data
struct _DXholder {
	void		*p;

	_DXholder() : p(0) {}

	const void	*raw()		const	{ return this; }
	void		*get()		const	{ return p; }
	void		**write()			{ return &p; }
	const void	*safe()		const	{ return p; }
	void		**iso_write()		{ return &p; }

	template<typename T> void	clear() {
		if (p && ((T*)p)->Release() == 0)
			p = 0;
	}
	template<typename T> void	set(T *t) {
		if (t)
			t->AddRef();
		if (p)
			((T*)p)->Release();
		p = t;
	}
	template<typename T> T*		get()	const	{ return (T*)get(); }
	template<typename T> T**	write()			{ return (T**)write(); }
};

// wrap an ISO_ptr - keep original data
struct _DXholderKeep {
	pointer32<void*>&	_get()		const { return ((pointer32<void*>*)this)[-2]; }
	const void			*raw()		const { return this; }
	void				*get()		const {
		if (void **p2 = _get())
			return *p2;
		return 0;
	}
	void	**write() const {
		auto	&p2 = _get();
		if (!p2)
			p2 = new(allocator32()) void*(0);
		return p2;
	}
	template<typename T> void	clear() {
		if (void **p2 = _get()) {
			if (*p2 && ((T*)*p2)->Release() == 0)
				allocator32().free(p2);
		}
	}
	template<typename T> T*		get()	const	{ return (T*)get(); }
	template<typename T> T**	write()	const	{ return (T**)write(); }
};

// wrap an ISO_ptr (optionally) - overwrite original data

template<typename T, int BITS>	struct PtrType			{ typedef T*			type; };
template<typename T>			struct PtrType<T, 32>	{ typedef pointer32<T>	type; };

template<int BITS> struct _DXwrapper {
	typename PtrType<_DXholder, BITS>::type	p;

	_DXwrapper() : p(nullptr) {}
	_DXwrapper(const _DXwrapper &b) : p(b.p) {}
	_DXwrapper(_DXwrapper &&b)		: p(b.p) { b.p = nullptr; }
	_DXwrapper(_DXholder *t)		: p(new(allocator32()) _DXholder(*t)) {}
	void operator=(const _DXwrapper &b)		{ p = b.p; }
	_DXwrapper& operator=(_DXwrapper &&b)	{ swap(p, b.p); return *this; }

	const void	*raw()		const	{ return p->raw(); }
	void		*get()		const	{ return p->get(); }
	void		**write()			{ if (!p) p = new(allocator32()) _DXholder; return p->write(); }
	const void	*safe()		const	{ return p ? p->safe() : 0; }
	void		**iso_write()		{ return p->iso_write(); }

	template<typename T> void	clear()			{ if (p) p->template clear<T>(); }
	template<typename T> void	set(T *t)		{ p->set(t); }
	template<typename T> T*		get()	const	{ return (T*)get(); }
	template<typename T> T**	write()			{ return (T**)write(); }
};

// wrap an ISO_ptr - keep original data
template<int BITS> struct _DXwrapperKeep {
	typename PtrType<_DXholderKeep, BITS>::type	p;

	_DXwrapperKeep()					: p(0)		{}
	_DXwrapperKeep(_DXwrapperKeep &&b)	: p(b.p)	{ b.p = nullptr; }
	_DXwrapperKeep& operator=(_DXwrapperKeep &&b)	{ swap(p, b.p); return *this; }

	bool		exists()	const	{ return !!p;	}
	const void	*raw()		const	{ return p ? p->raw() : 0; }
	void		*get()		const	{ return p ? p->get() : 0; }
	void		**write()	const	{ return p ? p->write() : 0; }

	template<typename T> void	clear()			{ if (p) p->template clear<T>(); }
	template<typename T> T*		get()	const	{ return (T*)get(); }
	template<typename T> T**	write()	const	{ return (T**)write(); }
};

#ifdef CROSS_PLATFORM

template<typename T, int BITS> struct DXwrapper : _DXwrapperKeep<BITS> {
	typedef _DXwrapperKeep<BITS>	B;
	DXwrapper()									= default;
	DXwrapper(DXwrapper &&b)					= default;
	DXwrapper& operator=(DXwrapper &&b)			= default;
	DXwrapper(const DXwrapper &b) {
		T *p = b.safe();
		if (p)
			p->AddRef();
		*write() = p;
	}
	DXwrapper& operator=(const DXwrapper &b) {
		T *p = b.safe();
		if (p)
			p->AddRef();
		clear<T>();
		*write() = p;
		return *this;
	}
	~DXwrapper() {
		B::template clear<T>();
		if (auto	dummy = (uint16*)B::raw()) {
			if (dummy[-1] == 0)
				free(dummy - 8);
		}
	}
	T	*get()		const {
		return B::template get<T>();
	}
	T	**write()	const {
		if (!B::exists()) {
			void *dummy = malloc(16);
			memset(dummy, 0, 16);
			unconst(B::p) = (_DXholderKeep*)((uint8*)dummy + 16);
		}
		return B::template write<T>();
	}
	T	*safe()		const {
		return get();
	}
};

#define DXwrapperKeep DXwrapper

#else

// wrap an ISO_ptr (optionally) - overwrite original data
template<typename T, int BITS> struct DXwrapper : _DXwrapper<BITS> {
	typedef _DXwrapper<BITS>	B;
	DXwrapper()	{}
	DXwrapper(const DXwrapper &b) : B(b) {
		if (T *p = get())
			p->AddRef();
	}
	DXwrapper& operator=(const DXwrapper &b) {
		if (T *p = b.safe())
			p->AddRef();
		B::template clear<T>();
		B::operator=(b);
		return *this;
	}
	DXwrapper(DXwrapper &&b)			= default;
	DXwrapper& operator=(DXwrapper &&b) = default;

	~DXwrapper()						{ B::template clear<T>(); }
	T		*safe()		const			{ return (T*)B::safe(); }
	T		**write()					{ return B::template write<T>(); }
	T		**iso_write()				{ return (T**)B::iso_write(); }
	T		*get()		const			{ return B::template get<T>(); }
};

// wrap an ISO_ptr - keep original data
template<typename T, int BITS> struct DXwrapperKeep : _DXwrapperKeep<BITS> {
	typedef _DXwrapperKeep<BITS>	B;
	~DXwrapperKeep()					{ clear(); }
	void	clear()						{ B::template clear<T>(); }
	T		**write()					{ return (T**)B::write(); }
	T		*get()				const	{ return (T*)B::get(); }
};

#endif

#else
//-----------------------------------------------------------------------------
//	32 bit
//-----------------------------------------------------------------------------

ISO_COMPILEASSERT(sizeof(void*) == 4);

template<typename T> struct indirect32 : T_inheritable<T>::type {
	indirect32()													{}
	template<typename T2> indirect32(const T2 &t2) :  T_inheritable<T>::type(t2)	{}
};

struct _DXwrapper {
	void	*p;
	_DXwrapper()	: p(0)	{}
	template<typename T> void	clear()		{ if (T *t = (T*)get()) t->Release(); p = 0; }
	void	*safe()		const	{ return p;		}
	void	**write()			{ return &p;	}
	void	*get()		const	{ return p;		}
	void	*raw()		const	{ return p;		}
};

// wrap an ISO_ptr - keep original data
struct _DXwrapperKeep {
	void	**p;
	_DXwrapperKeep() : p(0)	{}
	template<typename T> void	clear()		{ if (T *t = (T*)get()) t->Release(); p = 0; }
	void	**write()		 	{ return p - 2;			}
	void	*get()		const	{ return p ? p[-2] : 0;	}
	void	*raw()		const	{ return p;				}
	bool	exists()	const	{ return !!p;			}
};

#ifdef CROSS_PLATFORM

template<typename T> struct DXwrapper : _DXwrapperKeep {
	T		*safe()		const;
	T		**write()			{ return (T**)_DXwrapperKeep::write();	}
	T		*get()		const	{ return (T*)_DXwrapperKeep::get();	}
};

#define DXwrapperKeep DXwrapper

#else

template<typename T> struct DXwrapper : _DXwrapper {
	~DXwrapper()				{ clear();							}
	void	clear()				{ _DXwrapper::clear<T>();			}
	T		*get()		const	{ return (T*)_DXwrapper::get();		}
	T		**write()		 	{ return (T**)_DXwrapper::write();	}
	T		*safe()		const	{ return (T*)_DXwrapper::safe();	}
	T		**iso_write()		{ return (T**)p - 2;				}
};

template<typename T> struct DXwrapperKeep : _DXwrapperKeep {
	DXwrapperKeep() : p(0)	{}
	~DXwrapperKeep()			{ clear();						}
	void	clear()				{ _DXwrapperKeep::clear<T>();	}
	T		**write()		 	{ return (T**)_DXwrapperKeep::write();	}
	T		*get()		const	{ return (T*)_DXwrapperKeep::get();		}
};

#endif
#endif

// wrap an ISO_ptr<ISO_openarray> - keep original data
template<int BITS> struct _DXwrapperOpenArray : _DXwrapperKeep<BITS> {
	operator sized_data()	const	{ return raw(); }
	const void	*raw()		const	{ const void *p = _DXwrapperKeep<BITS>::raw(); return p ? *(typename PtrType<void, BITS>::type*)p : 0; }
};

} // namespace iso
#endif // DX_H

