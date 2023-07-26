#ifndef SHARED_PTR_H
#define SHARED_PTR_H

#include "base/hash.h"
#include "base/atomic.h"
#include "base/functions.h"

namespace iso {

//-----------------------------------------------------------------------------
//	shared_ptr
//-----------------------------------------------------------------------------

#if 0
struct sharedptr_control_base {
	static auto	&hash() {
		static hash_map<void*, sharedptr_control_base*>	hash;
		return hash;
	}

	atomic<uint32>	strong, weak;
	sharedptr_control_base() : strong(0), weak(1) {}

	void	add_strong()		{ ++strong; }
	void	add_strong(int n)	{ strong += n; }
	void	add_weak()			{ ++weak; }
};

template<typename T, typename D> struct sharedptr_control : sharedptr_control_base, D {
	T	*p;

	static sharedptr_control	*find(T *p) {
		if (p) {
			sharedptr_control_base *&c = hash()[unconst(p)].put();
			if (!c)
				c = new sharedptr_control(unconst(p), deleter<T>());
			return static_cast<sharedptr_control*>(c);
		}
		return nullptr;
	}

	sharedptr_control(T *p, D d) : D(d), p(p) {}

	void	destroy() {
		D::operator()(p);
		hash().remove(p);
		p = nullptr;
		release_weak();
	}
	void	release_strong() {
		if (!--strong)
			destroy();
	}
	void	release_strong(int n) {
		if (!(strong -= n))
			destroy();
	}
	void	release_weak() {
		if (!--weak)
			delete this;
	}
};

template<typename T, typename D, bool strong> class shared_store;

template<typename T, typename D = deleter<T>> class shared_ptr {
protected:
	typedef sharedptr_control<T,D>	C;
	friend class shared_store<T, D, false>;
	friend class shared_store<T, D, true>;
	C	*c;
	T	*p;
	void	set(T *_p, C *_c) {
		if (_c && _p)
			_c->add_strong();
		if (c)
			c->release_strong();
		p = _p;
		c = _c;
	}
public:
	shared_ptr()					: c(nullptr), p(nullptr)		{}
	explicit shared_ptr(C *c)		: c(c), p(c ? c->p : nullptr)	{}
	shared_ptr(T *p, D d)			: c(new C(p, d)), p(p)			{ c->add_strong(); }
	shared_ptr(const shared_ptr &b)	: c(b.c), p(b.p) 				{ if (c && p) c->add_strong(); }
	shared_ptr(shared_ptr &&b)		: c(b.c), p(b.detach())			{}
	shared_ptr(T *p)				: c(C::find(p)), p(p)			{ if (c && p) c->add_strong(); }
	template<typename U> shared_ptr(const shared_ptr<U> &b, enable_if_t<T_is_base_of<T, U>::value>* = nullptr)	: c(*(C**)&b), p(b.get()) 		{ if (c && p) c->add_strong(); }
	template<typename U> shared_ptr(shared_ptr<U> &&b, enable_if_t<T_is_base_of<T, U>::value>* = nullptr)		: c(*(C**)&b), p(b.detach())	{}

	shared_ptr&	operator=(const shared_ptr &b)		{ set(b.p, b.c); return *this; }
	shared_ptr&	operator=(shared_ptr &&b)			{ swap(*this, b); return *this; }
	shared_ptr&	operator=(T *_p)					{ set(_p, C::find(_p)); return *this; }

	~shared_ptr()									{ if (c) c->release_strong(); }
	operator T*()					const			{ return p; }
	T*			operator->()		const			{ return p; }
	T*			get()				const			{ return p; }
	void		clear()								{ if (c) { c->release_strong(); c = nullptr; p = nullptr; } }
	T*			detach()							{ c = nullptr; return p; }
	friend void swap(shared_ptr &a, shared_ptr &b)	{ swap(a.p, b.p); swap(a.c, b.c); }
	friend T*	get(const shared_ptr &a)			{ return a; }
};

// shared_store - for strong/weak storage

template<typename T, typename D, bool strong> struct shared_access;
template<typename T, typename D> struct shared_access<T, D, false> {
	static void	add(sharedptr_control<T, D> *c)		{ c->add_weak(); }
	static void	release(sharedptr_control<T, D> *c)	{ c->release_weak(); }
};
template<typename T, typename D> struct shared_access<T, D, true> {
	static void	add(sharedptr_control<T, D> *c)		{ c->add_strong(); }
	static void	release(sharedptr_control<T, D> *c)	{ c->release_strong(); }
};

template<typename T, typename D, bool strong> class shared_store {
protected:
	typedef sharedptr_control<T, D>		C;
	typedef shared_access<T, D, strong>	A;

	C	*c;

	void	set(C *_c) {
		if (_c)
			A::add(_c);
		if (c)
			A::release(c);
		c = _c;
	}
public:
	shared_store() : c(nullptr)								{}
	shared_store(const shared_ptr<T,D> &b)	: c(b.c)		{ if (c) A::add(c); }
	shared_store(const shared_store &b)		: c(b.c)		{ if (c) A::add(c); }
	shared_store(shared_store &&b)			: c(b.c)		{ b.c = nullptr; }
	shared_store(T *p)						: c(C::find(p)) { if (c) A::add(c); }
	~shared_store()											{ if (c) A::release(c); }
	shared_store&	operator=(const shared_store &b)		{ set(b.c); return *this; }
	shared_store&	operator=(shared_store &&b)				{ swap(*this, b); return *this; }
	shared_store&	operator=(T *p)							{ set(C::find(p)); return *this; }
	operator shared_ptr<T,D>()		const					{ if (c) c->add_strong(); return shared_ptr<T,D>(c); }
	shared_ptr<T,D>	get()			const					{ if (c) c->add_strong(); return shared_ptr<T,D>(c); }
	void			clear()									{ if (c) { A::release(c); c = nullptr; } }
	T*				detach()								{ T *p = c ? c->p : nullptr; c = nullptr; return p; }
	friend void		swap(shared_store &a, shared_store &b)	{ swap(a.c, b.c); }
	friend auto		get(const shared_store &a)				{ return a.get(); }
};

template<typename T, typename D = deleter<T>> using weak_ptr	= shared_store<T, D, false>;
template<typename T, typename D = deleter<T>> using strong_ptr	= shared_store<T, D, true>;

template<typename T, typename...U> shared_ptr<T> make_shared(U&&...u) {
	return new T(forward<U>(u)...);
}

//-----------------------------------------------------------------------------
//	atomic shared_ptr
//-----------------------------------------------------------------------------

template<typename T, typename D> class atomic<shared_ptr<T, D> > {
protected:
	typedef sharedptr_control<T, D>	C;
	typedef external_refs<C>	external;
	atomic<external>	c;

	void	set(C *_c) {
		external c0, c1(_c);
		do
			c0 = c;
		while (!c.cas(c0, c1));
		c0.release_strong();
	}
	C*		acquire() {
		external c0, c1;
		do {
			c0	= c;
			c1	= c0;
			c1.add_strong();
		} while (!c.cas(c0, c1));

		if (c1.fix()) {
			c0.ext_refs = 0;
			if (!c.cas(c1, c0))
				c1.unfix();
		}
		return c1.p;
	}
public:
	atomic()				: c(0)				{}
	atomic(T *p, D d)		: c(new C(p, d))	{}
	explicit atomic(C *c)	: c(c)				{}
	atomic(atomic &b)		: c(b.acquire())	{}
	atomic(T *p)			: c(C::find(p))		{}
	~atomic()									{ external c0 = c; c0.release_strong(); }
	atomic&			operator=(atomic &b)		{ set(b.acquire()); return *this; }
	atomic&			operator=(T *p)				{ set(C::find(p)); return *this; }
	shared_ptr<T,D>	operator->()	const		{ return acquire(); }
	shared_ptr<T,D>	get()			const		{ return acquire(); }
	void			clear()						{ set(0); }
	friend shared_ptr<T,D> get(const atomic &a)	{ return a.acquire(); }
};

#else

struct sharedptr_control_base {
	static auto	&hash() {
		static hash_map<void*, sharedptr_control_base*>	hash;
		return hash;
	}

	atomic<uint32>	strong, weak;
	void			*p;
	void			(*deleter)(void*);

	sharedptr_control_base(void *p, void(*deleter)(void*)) : strong(0), weak(1), p(p), deleter(deleter) {}

	void	add_strong()		{ ++strong; }
	void	add_strong(int n)	{ strong += n; }
	void	add_weak()			{ ++weak; }

	void	destroy() {
		deleter(this);
		hash().remove(p);
		p = nullptr;
		release_weak();
	}
	void	release_strong() {
		if (!--strong)
			destroy();
	}
	void	release_strong(int n) {
		if (!(strong -= n))
			destroy();
	}
	void	release_weak() {
		if (!--weak)
			delete this;
	}
};

template<typename T, typename D = deleter<T>> struct sharedptr_control : sharedptr_control_base, D {
	static sharedptr_control_base	*find(T *p) {
		if (p) {
			sharedptr_control_base *&c = hash()[unconst(p)].put();
			if (!c)
				c = new sharedptr_control(unconst(p), D());
			return c;
		}
		return nullptr;
	}
	static void deleter(sharedptr_control* me) {
		(*(D*)me)((T*)me->p);
	}

	sharedptr_control(T *p, D d) : sharedptr_control_base(p, (void(*)(void*))deleter), D(d) {}
};

template<typename T, bool strong> class shared_store;

template<typename T> class shared_ptr {
protected:
	typedef sharedptr_control_base	C;
	friend class shared_store<T, false>;
	friend class shared_store<T, true>;
	C	*c;
	T	*p;
	void	set(T *_p, C *_c) {
		if (_c && _p)
			_c->add_strong();
		if (c)
			c->release_strong();
		p = _p;
		c = _c;
	}

public:
	shared_ptr()					: c(nullptr), p(nullptr)		{}
	explicit shared_ptr(C *c)		: c(c), p(c ? c->p : nullptr)	{}
	template<typename D> shared_ptr(T *p, D d)	: c(new sharedptr_control<T, D>(p, d)), p(p)			{ c->add_strong(); }
	shared_ptr(const shared_ptr &b)	: c(b.c), p(b.p) 				{ if (c && p) c->add_strong(); }
	shared_ptr(shared_ptr &&b)		: c(b.c), p(b.detach())			{}
	shared_ptr(T *p)				: c(sharedptr_control<T>::find(p)), p(p)				{ if (c && p) c->add_strong(); }
	template<typename U> shared_ptr(const shared_ptr<U> &b, enable_if_t<T_is_base_of<T, U>::value>* = nullptr)	: c(*(C**)&b), p(b.get()) 		{ if (c && p) c->add_strong(); }
	template<typename U> shared_ptr(shared_ptr<U> &&b, enable_if_t<T_is_base_of<T, U>::value>* = nullptr)		: c(*(C**)&b), p(b.detach())	{}

	shared_ptr&	operator=(const shared_ptr &b)		{ set(b.p, b.c); return *this; }
	shared_ptr&	operator=(shared_ptr &&b)			{ swap(*this, b); return *this; }
	shared_ptr&	operator=(T *_p)					{ set(_p, sharedptr_control<T>::find(_p)); return *this; }

	~shared_ptr()									{ if (c) c->release_strong(); }
	operator T*()					const			{ return p; }
	T*			operator->()		const			{ return p; }
	T*			get()				const			{ return p; }
	void		clear()								{ if (c) { c->release_strong(); c = nullptr; p = nullptr; } }
	T*			detach()							{ c = nullptr; return p; }
	friend void swap(shared_ptr &a, shared_ptr &b)	{ swap(a.p, b.p); swap(a.c, b.c); }
	friend T*	get(const shared_ptr &a)			{ return a; }
};


// shared_store - for strong/weak storage

template<typename T, bool strong> struct shared_access;
template<typename T> struct shared_access<T, false> {
	static void	add(sharedptr_control_base *c)		{ c->add_weak(); }
	static void	release(sharedptr_control_base *c)	{ c->release_weak(); }
};
template<typename T> struct shared_access<T, true> {
	static void	add(sharedptr_control_base *c)		{ c->add_strong(); }
	static void	release(sharedptr_control_base *c)	{ c->release_strong(); }
};

template<typename T, bool strong> class shared_store {
protected:
	typedef sharedptr_control_base		C;
	typedef shared_access<T, strong>	A;

	C	*c;

	void	set(C *_c) {
		if (_c)
			A::add(_c);
		if (c)
			A::release(c);
		c = _c;
	}
public:
	shared_store() : c(nullptr)								{}
	shared_store(const shared_ptr<T> &b)	: c(b.c)		{ if (c) A::add(c); }
	shared_store(const shared_store &b)		: c(b.c)		{ if (c) A::add(c); }
	shared_store(shared_store &&b)			: c(b.c)		{ b.c = nullptr; }
	shared_store(T *p)						: c(sharedptr_control<T>::find(p)) { if (c) A::add(c); }
	~shared_store()											{ if (c) A::release(c); }
	shared_store&	operator=(const shared_store &b)		{ set(b.c); return *this; }
	shared_store&	operator=(shared_store &&b)				{ swap(*this, b); return *this; }
	shared_store&	operator=(T *p)							{ set(sharedptr_control<T>::find(p)); return *this; }
	operator shared_ptr<T>()		const					{ if (c) c->add_strong(); return shared_ptr<T>(c); }
	shared_ptr<T>	get()			const					{ if (c) c->add_strong(); return shared_ptr<T>(c); }
	void			clear()									{ if (c) { A::release(c); c = nullptr; } }
	T*				detach()								{ T *p = c ? c->p : nullptr; c = nullptr; return p; }
	friend void		swap(shared_store &a, shared_store &b)	{ swap(a.c, b.c); }
	friend auto		get(const shared_store &a)				{ return a.get(); }
};

template<typename T, typename D = deleter<T>> using weak_ptr	= shared_store<T, false>;
template<typename T, typename D = deleter<T>> using strong_ptr	= shared_store<T, true>;

template<typename T, typename...U> shared_ptr<T> make_shared(U&&...u) {
	return new T(forward<U>(u)...);
}

//-----------------------------------------------------------------------------
//	atomic shared_ptr
//-----------------------------------------------------------------------------

template<typename T> class atomic<shared_ptr<T> > {
protected:
	typedef sharedptr_control_base	C;
	typedef external_refs<C>	external;
	atomic<external>	c;

	void	set(C *_c) {
		external c0, c1(_c);
		do
			c0 = c;
		while (!c.cas(c0, c1));
		c0.release_strong();
	}
	C*		acquire() {
		external c0, c1;
		do {
			c0	= c;
			c1	= c0;
			c1.add_strong();
		} while (!c.cas(c0, c1));

		if (c1.fix()) {
			c0.ext_refs = 0;
			if (!c.cas(c1, c0))
				c1.unfix();
		}
		return c1.p;
	}
public:
	atomic()				: c(0)				{}
	template<typename D> atomic(T *p, D d)		: c(new sharedptr_control<T,D>(p, d))	{}
	explicit atomic(C *c)	: c(c)				{}
	atomic(atomic &b)		: c(b.acquire())	{}
	atomic(T *p)			: c(sharedptr_control<T>::find(p))		{}
	~atomic()									{ external c0 = c; c0.release_strong(); }
	atomic&			operator=(atomic &b)		{ set(b.acquire()); return *this; }
	atomic&			operator=(T *p)				{ set(sharedptr_control<T>::find(p)); return *this; }
	shared_ptr<T>	operator->()	const		{ return acquire(); }
	shared_ptr<T>	get()			const		{ return acquire(); }
	void			clear()						{ set(0); }
	friend shared_ptr<T> get(const atomic &a)	{ return a.acquire(); }
};

#endif


} // namespace iso

#endif // SHARED_PTR_H
