#ifndef THUNK64_H
#define THUNK64_H

#include "base/pointer.h"
#include "extra/type_tree.h"
#include "filename.h"
#include <thread.h>
#include "crc32.h"

namespace iso {
	// mapped_mem
	struct mapped_mem {
		static void*		base;
		static uint32		next;

		static void			init(void *p)					{ base = p; next = 0x8000; }
		static uint32		alloc(size_t s)					{ uint32 p = next; next = p + uint32(s); return p; }
		static uint32		alloc(size_t s, size_t a)		{ uint32 p = align(next, uint32(a)); next = p + uint32(s); return p; }

		template<typename T> static	T*	ptr(uint32 o) 		{ return o ? (T*)((char*)base + o) : 0; }
		static uint32		offset(const void *p)			{ return p ? (char*)p - (char*)base : 0; }

		static uint32		dup(const void *p, size_t s)	{ uint32 o = alloc(s); memcpy(ptr<char>(o), p, s); return o; }
		static uint32		dup(const char *p)				{ return p ? offset(p) < 0x8000 ? offset(p) : dup(p, strlen(p) + 1) : 0; }
		template<typename T> static uint32	dup(const T &t)	{ uint32 o = alloc(sizeof(T), alignof(T)); *ptr<T>(o) = t; return o; }

		static inline void	reset();
	};
}

void *operator new(size_t s, iso::mapped_mem &m)	{ return m.ptr<void>(m.alloc(s)); }

namespace iso {

struct base_mapped {
	int32	offset;
	void	set(const void *p)	{ offset = mapped_mem::offset(p); }
	void*	get() const			{ return mapped_mem::ptr<void>(offset); }
};

struct map_list : static_list<map_list> {
	struct record;
	typedef soft_pointer<record, base_mapped>	ptr;
	struct record {
		ptr			next;
		uint32		offset;
		const void	*p;
	};
	ptr	root;

	uint32	find(const void *p) const {
		for (record *r = root; r; r = r->next) {
			if (r->p == p)
				return r->offset;
		}
		return 0;
	}
	
	uint32	add(const void *p, uint32 offset) {
		record	*r = new(*(mapped_mem*)0) record;
		r->next		= root;
		r->offset	= offset;
		r->p		= p;
		root		= r;
		return offset;
	}
	void reset() {
		root = 0;
	}

	static void reset_all() {
		for (iterator i = begin(), e = end(); i != e; ++i)
			i->reset();
	}
};

void mapped_mem::reset() {
	map_list::reset_all();
	next	= 0x8000;
}

template<typename T, bool to64> struct mapped_pointer;

template<typename T> struct mapped_pointer<T, true> {
	typedef typename T_noconst<T>::type TM;
	uint32	p;
	void	operator=(const T *t)	{	//on 32
		p = mapped_mem::offset(t);
	}
	template<typename C> void operator=(const counted_data<C> &t) {	//on 32
		if (int n = t.n) {
			p			= mapped_mem::alloc(sizeof(T) * n, alignof(T));
			TM		*d	= mapped_mem::ptr<TM>(p);
			const C	*s	= t.t;
			while (n--)
				*d++ = *s++;
		} else {
			p = 0;
		}
	}
	operator T*() const	{		//on 64
		return mapped_mem::ptr<T>(p);
	}
};

template<typename T> struct mapped_pointer<T, false> {
	typedef typename T_noconst<T>::type TM;
	uint32	p;
	void	operator=(const T *t) {	//on 64
		p = t ? mapped_mem::dup(t) : 0;
	}
	template<typename C> void operator=(const counted_data<C> &t) {	//on 64
		if (int n = t.n) {
			p			= mapped_mem::alloc(sizeof(T) * n, alignof(T));
			TM		*d	= mapped_mem::ptr<TM>(p);
			const C	*s	= t.t;
			while (n--)
				*d++ = *s++;
		} else {
			p = 0;
		}
	}
	operator T*() const {		//on 32
		return mapped_mem::ptr<T>(p);
	}
};

template<typename T> struct mapped_pointer<struct_tree<T>, true> {
	typedef struct_tree<T> S;
	uint32	p;
	template<typename P> void	operator=(const P *t) {	//on 32
		if (t) {
			p = mapped_mem::alloc(sizeof(S), alignof(T));
			*mapped_mem::ptr<S>(p)	= *t;
		} else {
			p = 0;
		}
	}
	template<typename C> void operator=(const counted_data<C> &t) {	//on 32
		if (int n = t.n) {
			p			= mapped_mem::alloc(sizeof(S) * n, alignof(T));
			S		*d	= mapped_mem::ptr<S>(p);
			const C	*s	= t.t;
			while (n--)
				*d++ = *s++;
		} else {
			p = 0;
		}
	}
	operator S*() const {		//on 64
		return mapped_mem::ptr<S>(p);
	}
};

template<typename T> struct mapped_pointer<struct_tree<T>, false> {
	typedef struct_tree<T> S;
	uint32	p;
	template<typename P> void operator=(const P *t) {	//on 64
		if (t) {
			p = mapped_mem::alloc(sizeof(S), alignof(T));
			*mapped_mem::ptr<S>(p)	= *t;
		} else {
			p = 0;
		}
	}
	template<typename C> void operator=(const counted_data<C> &t) {	//on 64
		if (int n = t.n) {
			p			= mapped_mem::alloc(sizeof(S) * n, alignof(T));
			S		*d	= mapped_mem::ptr<S>(p);
			const C	*s	= t.t;
			while (n--)
				*d++ = *s++;
		} else {
			p = 0;
		}
	}
	operator S*() const {		//on 32
		return mapped_mem::ptr<S>(p);
	}
};

template<> struct mapped_pointer<char, true> {
	static map_list recs;
	uint32	p;
	void	operator=(const char *t)						{ p = t ? mapped_mem::dup(t) : 0; }				//on 32
	operator const char*() const							{ return mapped_mem::ptr<const char>(p); }		//on 64
};
template<> struct mapped_pointer<char, false> {
	static map_list recs;
	uint32	p;
	void	operator=(const char *t)						{ p = recs.find(t); if (t && !p) p = recs.add(t, mapped_mem::dup(t)); }	//on 64
	void	operator=(const counted_data<const char> &t)	{ p = t.n ? mapped_mem::dup(t.t, t.n) : 0; }	//on 64
	operator const char*() const							{ return mapped_mem::ptr<const char>(p); }		//on 32
};

map_list mapped_pointer<char, true>::recs;
map_list mapped_pointer<char, false>::recs;

template<typename T> struct mapped_to_32								{ typedef T type; };
template<typename T> struct mapped_to_32<T*>							{ typedef mapped_pointer<T, false> type; };
template<typename T, int I> struct mapped_to_32<counted_type<T, I> >	{ typedef mapped_pointer<T, false> type; };
template<typename T> struct mapped_to_32<ptr_type<T> >					{ typedef mapped_pointer<T, false> type; };

template<typename T> struct mapped_to_64								{ typedef T type; };
template<typename T> struct mapped_to_64<T*>							{ typedef mapped_pointer<T, true> type; };
template<typename T, int I> struct mapped_to_64<counted_type<T, I> >	{ typedef mapped_pointer<T, true> type; };
template<typename T> struct mapped_to_64<ptr_type<T> >					{ typedef mapped_pointer<T, true> type; };

struct call64 {
	crc32	fn;
	uint32	next;

	call64(crc32 _fn) : fn(_fn) {}
	void *return_value()			{ return this + 1;			}
	void *operator new(size_t size)	{ return mapped_mem::base;	}
};

struct call64_exit : call64 {
	call64_exit() : call64("exit") {}
};

struct call64_reset : call64 {
	call64_reset() : call64("reset") {}
};

struct Thunk64 {
	Semaphore	sem1, sem2;
	HANDLE		hProc;
	HANDLE		hPage;
	void		*page;

	Thunk64(const char *name, uint32 size = 0x1000000) : sem1(Semaphore::Shared(0)), sem2(Semaphore::Shared(0)), hProc(0) {
		SECURITY_ATTRIBUTES	sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE}; // inheritable
		hPage	= CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, size, name);
		page	= MapViewOfFile(hPage, FILE_MAP_ALL_ACCESS, 0, 0, size);
		mapped_mem::init(page);

		memcpy(page, &sem1, sizeof(Semaphore[2]));

		filename	fn;
		GetModuleFileName(NULL, fn, sizeof(fn));

		PROCESS_INFORMATION	pi;
		STARTUPINFO			si;
		clear(pi);
		clear(si);
		si.cb			= sizeof(si);
		si.dwFlags		= STARTF_USESHOWWINDOW;
		si.wShowWindow	= SW_HIDE;

		if (CreateProcess(NULL,
				buffer_accum<256>() << fn.rem_dir().add_dir(name).set_ext("exe") << ' ' << hPage << ' ' << size,
				NULL, NULL, TRUE, 0/*DETACHED_PROCESS*/, NULL, NULL, &si, &pi
			)) {
			hProc	= pi.hProcess;
			sem2.lock();
		}
	}
	bool	Running() const {
		return hProc != 0;
	}

	~Thunk64() {
		_Call(new call64_exit);
	}

	void	Reset() {
		mapped_mem::reset();
		_Call(new call64_reset);
	}

	void	_Call(call64 *c) {
		c->next	= mapped_mem::next;
		sem1.unlock();
		sem2.lock();
		mapped_mem::next = c->next;
	}

	template<typename T> typename T::ret *Call(T *c) {
		_Call(c);
		return (typename T::ret*)c->return_value();
	}
};

}

#endif	// THUNK64_H
