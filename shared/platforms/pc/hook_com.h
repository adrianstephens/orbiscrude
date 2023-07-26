#ifndef HOOK_COM_H
#define HOOK_COM_H

#include "hook.h"
#include "com.h"
#include "extra/marshal.h"
#include <comdef.h>

namespace iso {

//-----------------------------------------------------------------------------
//	FM - fix for call to original functions
//-----------------------------------------------------------------------------

template<typename T>		struct FM						: T_type<T> {};
template<typename T>		struct FM<T&>					: T_type<T> {};
template<typename T>		struct FM<T*>					: T_if<is_com<T>::value, unwrapped<T*>, typename FM<T>::type*> {};
template<typename T>		struct FM<T**>					: T_type<T**> {};
template<typename T>		struct FM<T* const*>			: T_type<const typename FM<T*>::type*> {};
template<typename T, int I> struct FM<counted<T, I, T*> >	: T_type<counted<map_t<iso::FM, T>, I>> {};

//template<typename T>		struct meta::map<FM, T**>		: T_type<T**> {};

//-----------------------------------------------------------------------------
//	RTM - convert to recorded format
//-----------------------------------------------------------------------------

template<typename T>		struct RTM						: T_type<T> {};
template<typename T>		struct RTM<T&>					: meta::map<RTM, T> {};
template<typename T>		struct RTM<T*>					: T_if<is_com<T>::value, T*,		rel_ptr<typename RTM<T>::type>>			{};
template<typename T>		struct RTM<const T*>			: T_if<is_com<T>::value, const T*,	rel_ptr<const typename RTM<T>::type>>	{};
template<>					struct RTM<void*>				: T_type<void*> {};
template<>					struct RTM<const void*>			: T_type<const void*> {};
template<typename T, int I> struct RTM<counted<T, I, T*> >	: T_type<rel_counted<map_t<iso::RTM, T>, I>> {};
template<>					struct RTM<const_memory_block>	: T_type<const_memory_block_rel> {};
template<>					struct RTM<malloc_block>		: T_type<const_memory_block_rel> {};

template<typename K, typename V> struct RTM<hash_map<K, V>>	: T_type<flat_hash<hash_t<K>, map_t<iso::RTM, V>>> {};

//-----------------------------------------------------------------------------
//	KM - as RTM, but dup strings
//-----------------------------------------------------------------------------

template<typename T>		struct KM						: T_type<T> {};
template<typename T>		struct KM<T&>					: T_type<T> {};
template<typename T>		struct KM<T*>					: T_if<is_com<T>::value, T*,		rel_ptr<typename KM<T>::type> >			{};
template<typename T>		struct KM<const T*>				: T_if<is_com<T>::value, const T*,	rel_ptr<const typename KM<T>::type> >	{};
template<>					struct KM<void*>				: T_type<void*> {};
template<>					struct KM<const void*>			: T_type<const void*> {};
template<>					struct KM<const char*>			: T_type<dup_pointer<rel_ptr<const char> >>	{};
template<>					struct KM<const char16*>		: T_type<dup_pointer<rel_ptr<const char16>>>{};
template<typename T, int I> struct KM<counted<T, I, T*> >	: T_type<rel_counted<map_t<iso::KM, T>, I>>	{};
template<>					struct KM<const_memory_block>	: T_type<const_memory_block_rel>			{};

template<typename K, typename V> struct KM<hash_map<K, V>>	: T_type<flat_hash<hash_t<K>, map_t<iso::KM, V>>> {};

//-----------------------------------------------------------------------------
//	PM - convert to playback format
//-----------------------------------------------------------------------------

template<typename T>		struct PM						: T_type<T> {};
template<typename T>		struct PM<T&>					: T_type<T> {};
template<typename T>		struct PM<T*>					: T_if<is_com<T>::value, lookup<T*>,		rel_ptr<typename PM<T>::type> > {};
template<typename T>		struct PM<const T*>				: T_if<is_com<T>::value, lookup<const T*>,	rel_ptr<const typename PM<T>::type> > {};
template<>					struct PM<void*>				: T_type<lookup<handle>> {};
template<>					struct PM<const void*>			: T_type<const void*> {};
template<typename T, int I> struct PM<counted<T, I, T*> >	: T_type<rel_counted<map_t<iso::PM, T>, I>> {};
template<typename T>		struct PM<lookedup<T> >			: T_type<lookup<T>> {};
template<>					struct meta::map<PM, void**>	: T_type<rel_ptr<void*>> {};

//-----------------------------------------------------------------------------
//	COMParse
//-----------------------------------------------------------------------------

template<typename F> static typename function<F>::R COMParse(const uint16 *p, F f) {
	return call<0>(f, *(TL_tuple<map_t<RTM, typename function<F>::P>>*)p);
}

template<typename F> static typename function<F>::R COMParse2(const uint16 *p, F f) {
	typedef TL_tuple<typename function<F>::P>	T0;
	typedef	map_t<RTM, T0>		T1;

	offset_allocator	a;

	T1&	t1 = *(T1*)p;
	allocate(a, t1, a.alloc<T0>());

	size_t	size = a.size();
	a.init(alloca(size));

	T0*	t0	= a.alloc<T0>();
	transfer(a, t1, *t0);
	ISO_ASSERT(a.size() == size);

	return call<0>(f, *t0);
}

//-----------------------------------------------------------------------------
//	COMReplay
//-----------------------------------------------------------------------------

template<typename...T>	struct T_noref<type_list<T...> >	{ typedef type_list<typename T_noref<T>::type...> type; };

void throw_hresult(HRESULT hr);

template<typename R>	struct COMcall {
	template<typename T, typename F, typename P> static void call(T &t, F f, const P &p) { iso::call(t, f, p); }
};
template<>	struct COMcall<HRESULT> {
	template<typename T, typename F, typename P> static void call(T &t, F f, const P &p) {
		HRESULT	hr = iso::call(t, f, p);
		if (!SUCCEEDED(hr))
			throw_hresult(hr);
	}
};

template<typename B> struct COMReplay {
	bool	abort;
	bool	always = false;

	auto&	Always()	{ always = true; return *this; }

	template<typename T> unique_ptr<T> Remap(const void *p) {
		typedef	TL_fields_t<T>		RTL0;
		typedef map_t<PM, RTL0>		RTL1;

		abort = false;
		lookup_allocator<offset_allocator, B>	a(*static_cast<B*>(this));
		TL_tuple<RTL1>	*t0 = (TL_tuple<RTL1>*)p;
		allocate(a, *t0, a.template alloc<TL_tuple<RTL0> >());

		size_t	size = a.size();
		malloc_block	m(a.size());
		a.init(m);
		TL_tuple<RTL0>	*t1 = a.template alloc<TL_tuple<RTL0> >();
		transfer(a, *t0, *t1);

		ISO_ASSERT(a.size() == size);
//		if (abort)
//			return none;
		return (T*)m.detach();
	}

	template<typename F1, typename C, typename F2, typename C2> void Replay2(C2 *obj, const void *p, F2 C::*f) {
		typedef	typename function<F1>::P	P;
		typedef	noref_t<P>		RTL0;
		typedef map_t<PM, P>	RTL1;

		abort = false;
		lookup_allocator<offset_allocator, B>	a(*static_cast<B*>(this));
		TL_tuple<RTL1>	*t0 = (TL_tuple<RTL1>*)p;
		allocate(a, *t0, a.template alloc<TL_tuple<RTL0> >());

		size_t	size = a.size();
		a.init(alloca(size));
		TL_tuple<RTL0>	*t1 = a.template alloc<TL_tuple<RTL0> >();
		transfer(a, *t0, *t1);

		ISO_ASSERT(a.size() == size);
		if (always || !abort)
			COMcall<typename function<F2>::R>::call(*static_cast<C*>(obj), f, *t1);
		always = false;
	}
	template<typename F1, typename C, typename F2, typename C2> void Replay2(const com_ptr<C2> &obj, const void *p, F2 C::*f) {
		Replay2<F1>(obj.get(), p, f);
	}
	template<typename C, typename F, typename C2> void Replay(C2 *obj, const void *p, F C::*f) {
		Replay2<F>(obj, p, f);
	}
	template<typename C, typename F, typename C2> void Replay(const com_ptr<C2> &obj, const void *p, F C::*f) {
		Replay2<F>(obj.get(), p, f);
	}

	template<typename F> void Replay(const void *p, F f) {
		abort = false;
		typedef	typename function<F>::P		P;
		typedef	noref_t<P>		RTL0;
		typedef map_t<PM, RTL0>	RTL1;

		lookup_allocator<offset_allocator, B>	a(*static_cast<B*>(this));
		auto	*t0		= (TL_tuple<RTL1>*)p;
		allocate(a, *t0, a.template alloc<TL_tuple<RTL0> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.template alloc<TL_tuple<RTL0> >();
		transfer(a, *t0, *t1);

		ISO_ASSERT(a.size() == size);
		if (always || !abort)
			call(f, *t1);
		always = false;
	}

};

//-----------------------------------------------------------------------------
//	COMRecording
//-----------------------------------------------------------------------------

template<typename T> T *mem_find(T *a, T *b, T v) {
	while (a < b) {
		if (*a == v)
			return a;
		++a;
	}
	return 0;
}

struct memory_cube {
	void	*data;
	uint32	width, rows, depth;
	uint32	row_pitch, depth_pitch;

	memory_cube(void *data, uint32 width, uint32 rows, uint32 depth, uint32 row_pitch, uint32 depth_pitch) :
		data(data), width(width), rows(rows), depth(depth), row_pitch(row_pitch), depth_pitch(depth_pitch) {}

	size_t	size()			const { return width * rows * depth; }
	size_t	strided_size()	const { return (depth - 1) * depth_pitch + (rows - 1) * row_pitch + width; }

	void	copy_to(void *dest)	const {
		const uint8	*s0	= (const uint8*)data;
		uint8		*d	= (uint8*)dest;
		for (int z = 0; z < depth; z++) {
			const uint8	*s	= s0;
			for (int y = 0; y < rows; y++) {
				memcpy(d, s, width);
				d += width;
				s += row_pitch;
			}
			s0 += depth_pitch;
		}
	}
	size_t	copy_to(void *dest, uint32 skip_val) const {
		bool	prev_skip	= true;
		uint32	*d			= (uint32*)dest;
		uint32	*token		= 0;
		uint32	run			= 0;

		for (int z = 0; z < depth; z++) {
			uint32	offset = z * depth_pitch;

			for (int y = 0; y < rows; y++) {
				for (uint32	*s = (uint32*)((char*)data + offset), *e = (uint32*)((char*)data + offset + width); s < e; ++s) {
					bool skip = *s == skip_val;

					if (skip != prev_skip) {
						if (prev_skip) {
							while (run > 0xffff) {
								*d++ = 0xffff;
								run -= 0xffff;
							}
							token	= d++;
							*token	= run;
						} else {
							*token	|= run << 16;
						}
						prev_skip	= skip;
						run			= 0;
					}

					++run;

					if (!skip) {
						*d++ = *s;
						if (run == 0xffff) {
							*token		|= 0xffff0000;
							token		= d++;
							*token		= 0;
							run			= 0;
						}
					}
				}
				offset += row_pitch;
			}
		}
		if (!prev_skip)
			*token	|= run << 16;

		return (uint8*)d - (uint8*)dest;
	}

	void	strided_copy_to(void *dest)	const {
		memcpy(dest, data, strided_size());
	}
	size_t	strided_copy_to(void *dest, uint32 skip_val) const {
		size_t	total		= 0;
		bool	prev_skip	= true;
		uint32	run			= 0;

		for (int z = 0; z < depth; z++) {
			uint32	offset = z * depth_pitch;

			for (int y = 0; y < rows; y++) {
				uint32	*d = (uint32*)((char*)dest + offset);

				for (uint32	*s = (uint32*)((char*)data + offset), *e = (uint32*)((char*)data + offset + width); s < e; ++s, ++d) {
					bool skip	= *s == skip_val;
					
					if (skip != prev_skip) {
						total		+= int(prev_skip) + (run ? (run - 1) / 0xffff : 0);
						prev_skip	= skip;
						run			= 0;
					}

					if (!skip) {
						*d = *s;
						++total;
					}
					++run;
				}

				offset += row_pitch;
			}
		}

		return total * sizeof(uint32);
	}

	void	copy_from(const void *srce) {
		uint8			*d0	= (uint8*)data;
		const uint8		*s	= (const uint8*)srce;
		for (int z = 0; z < depth; z++) {
			uint8	*d	= d0;
			for (int y = 0; y < rows; y++) {
				memcpy(d, s, width);
				s += width;
				d += row_pitch;
			}
			d0 += depth_pitch;
		}
	}

	void	copy_from_rle(const const_memory_block &rle) {
		uint32	offset	= 0;
		for (const uint32 *p = rle; p != rle.end();) {
			uint32	token	= *p++;
			uint32	len		= token >> 16;

			offset += (token & 0xffff) * 4;

			uint8	*s		= (uint8*)p;
			uint32	x		= offset % width;
			uint32	y		= (offset / width) % rows;
			uint32	z		= offset / (width * rows);

			p		+= len;
			len		*= 4;
			offset	+= len;

			while (len) {
				uint32	len1	= min(width - x, len);
				uint8	*dest	= (uint8*)data + z * depth_pitch + y * row_pitch + x;
				memcpy(dest, s, len1);

				s	+= len1;
				len	-= len1;
				x	= 0;
				if (++y == rows) {
					y = 0;
					++z;
				}
			}

			ISO_ASSERT((uint8*)p == s);
		}
	}

};

/*
template<typename F, typename W, typename... P> typename function<F>::R COMRun(W *wrap, F f, P... p) {
	return (wrap->orig->*f)(p...);
}

template<typename F, typename W, typename... P> typename function<F>::R COMRun2(W *wrap, F f, P... p) {
	typedef type_list<P...>					TL0;
	typedef map_t<FM, TL0>	TL1;

	offset_allocator	a;
	auto		t0	= TL_tuple<TL0>(p...);
	allocate(a, t0, a.alloc<TL_tuple<TL1> >());

	size_t	size	= a.size();
	a.init(alloca(size));
	TL_tuple<TL1>	*t1 = a.alloc<TL_tuple<TL1> >();
	transfer(a, t0, *t1);
	ISO_ASSERT(a.size() == size);

	return call(*wrap->orig, f, *(TL_tuple<TL0>*)t1);
}
*/
struct COMRecording {
	malloc_block	recording;
	size_t			total;
	bool			enable;
	Mutex			recording_mutex;

	class header {
		uint16			size;
	public:
		uint16			id;
		header(uint16 _id, size_t _size) : size((uint16)_size), id(_id) {
			if (_size >= 0x10000) {
				size |= 1;
				(&id)[1] = uint16(_size >> 16);
			}
		}
		uint16				*data()			{ return &id + 1 + (size & 1); }
		const uint16		*data()	const	{ return &id + 1 + (size & 1); }
		const_memory_block	block()	const	{ return const_memory_block(data(), next()); }
		const header		*next()	const	{ return (header*)((uint8*)this + (size & 1 ? ((&id)[1] << 16) | (size - 1) : size)); }
	};

	memory_block	get_buffer() const {
		return memory_block(recording, total);
	}
	memory_block	get_buffer_reset() {
		size_t t = total;
		total = 0;
		return memory_block(recording.p, t);
	}

	void*	get_space(uint16 id, size_t size) {
		size += sizeof(header);
		if (size >= 0x10000)
			size += 2;

		if (total + size > recording.length())
			recording.resize(max(recording.length() * 2, total + size * 2));

		header	*p	= new((uint8*)recording + total) header(id, size);
		total		+= size;

		return p->data();
	}
	void add(uint16 id, size_t size, const void *p) {
		with(recording_mutex),
			memcpy(get_space(id, size), p, size);
	}
	template<typename T> void add(uint16 id, const T &t) {
		add(id, sizeof(T), &t);
	}
	
	COMRecording() : total(0), enable(false) {}
	
	void Reset() {
		total = 0;
	}

	void Record(uint16 id) {
		with(recording_mutex),
			get_space(id, 0);
	}

	void Record(uint16 id, const const_memory_block &t) {
		with(recording_mutex),
			t.copy_to(get_space(id, t.length()));
	}

	void Record(uint16 id, const memory_cube &t) {
		with(recording_mutex),
			t.copy_to(get_space(id, t.size()));
	}

	// record single item (usually a tuple)
	template<typename T0> void Record(uint16 id, const T0 &t0) {
		typedef map_t<RTM, T0>	T1;

		offset_allocator	a;
		allocate(a, t0, a.alloc<T1>());

		size_t	size	= a.size();
		with_lambda(recording_mutex, [&]() {
			a.init(get_space(id, align(size, 4)));
			transfer(a, t0, *a.alloc<T1>());
		});
		ISO_ASSERT(a.size() == size);
	}

	// record parameters
	template<typename... P> void Record(uint16 id, const P&... p) {
		Record(id, tuple<P...>(p...));
	}

	// helpers for passing return value back
	template<typename R> struct RunRecord_s {
		template<typename F, typename W, typename... P> static R f(COMRecording *rec, W *wrap, F f, int id, P... p) {
			R	r = (wrap->orig->*f)(p...);
			if (rec)
				rec->Record(uint16(id), p...);
			return r;
		}
		template<typename F, typename W, typename T0, typename T1> static R f2(COMRecording *rec, W *wrap, F f, int id, T0 &t0, T1 &t1) {
			R	r = call(*wrap->orig, f, t1);
			if (rec)
				rec->Record(uint16(id), t0);
			return r;
		}
		template<typename F, typename W, typename T, typename T0, typename T1, typename...P> static R fwrap(COMRecording *rec, W *wrap, F f, int id, T **pp, T0 &t0, T1 &t1, P... p) {
			R	r = com_wrap_system->make_wrap_check(call(*wrap->orig, f, t1), pp, wrap, p...);
			if (rec)
				rec->Record(uint16(id), t0);
			return r;
		}
	};

	template<> struct RunRecord_s<void> {
		template<typename F, typename W, typename... P> static void f(COMRecording *rec, W *wrap, F f, int id, P... p) {
			(wrap->orig->*f)(p...);
			if (rec)
				rec->Record(uint16(id), p...);
		}
		template<typename F, typename W, typename T0, typename T1> static void f2(COMRecording *rec, W *wrap, F f, int id, T0 &t0, T1 &t1) {
			call(*wrap->orig, f, t1);
			if (rec)
				rec->Record(uint16(id), t0);
		}
		template<typename F, typename W, typename T, typename T0, typename T1, typename...P> static void fwrap(COMRecording *rec, W *wrap, F f, int id, T **pp, T0 &t0, T1 &t1, P... p) {
			call(*wrap->orig, f, t1);
			if (pp)
				*pp = com_wrap_system->make_wrap_check(*pp, wrap, p...);
			if (rec)
				rec->Record(uint16(id), t0);
		}
	};

	// run function and record parameters
	template<typename F, typename W, typename... P> typename function<F>::R RunRecord(W *wrap, F f, int id, P... p) {
		return RunRecord_s<typename function<F>::R>::f(enable ? this : nullptr, wrap, f, id, p...);
	}
	/*
	// fix parameters and run function
	template<typename F, typename W, typename... P> typename function<F>::R Run2(W *wrap, F f, P... p) {
		typedef type_list<P...>						TL0;
		typedef map_t<FM, TL0>	TL1;

		offset_allocator	a;
		auto		t0	= TL_tuple<TL0>(p...);
		allocate(a, t0, a.alloc<TL_tuple<TL1> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.alloc<TL_tuple<TL1> >();
		transfer(a, t0, *t1);
		ISO_ASSERT(a.size() == size);

		return call(*wrap->orig, f, *(TL_tuple<TL0>*)t1);
	}
	*/
	// fix parameters, run function, and record
	template<typename F, typename W, typename... P> typename function<F>::R RunRecord2(W *wrap, F f, int id, P... p) {
		typedef type_list<P...>						TL0;
		typedef map_t<FM, TL0>	TL1;

		offset_allocator	a;
		auto		t0	= TL_tuple<TL0>(p...);
		allocate(a, t0, a.alloc<TL_tuple<TL1> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.alloc<TL_tuple<TL1> >();
		transfer(a, t0, *t1);
		ISO_ASSERT(a.size() == size);

		return RunRecord_s<typename function<F>::R>::f2(enable ? this : nullptr, wrap, f, id, t0, *(TL_tuple<TL0>*)t1);
	}

	// fix parameters, run function, record, and wrap result using IID
	template<typename F, typename W, typename T, typename... P> typename function<F>::R RunRecord2Wrap(W *wrap, F f, int id, REFIID riid, T **pp, P... p) {
		typedef type_list<P..., IID, void**>		TL0;
		typedef map_t<FM, TL0>	TL1;

		offset_allocator	a;
		auto		t0	= TL_tuple<TL0>(p..., riid, (void**)pp);
		allocate(a, t0, a.alloc<TL_tuple<TL1> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.alloc<TL_tuple<TL1> >();
		transfer(a, t0, *t1);
		ISO_ASSERT(a.size() == size);

		return RunRecord_s<typename function<F>::R>::fwrap(enable ? this : nullptr, wrap, f, id, pp, t0, *(TL_tuple<TL0>*)t1, p...);
	}

	// fix parameters, run function, record, and wrap result
	template<typename F, typename W, typename T, typename... P> typename function<F>::R RunRecord2Wrap(W *wrap, F f, int id, T **pp, P... p) {
		typedef type_list<P..., T**>				TL0;
		typedef map_t<FM, TL0>	TL1;

		offset_allocator	a;
		auto		t0	= TL_tuple<TL0>(p..., pp);
		allocate(a, t0, a.alloc<TL_tuple<TL1> >());

		size_t	size	= a.size();
		a.init(alloca(size));
		auto	*t1		= a.alloc<TL_tuple<TL1> >();
		transfer(a, t0, *t1);
		ISO_ASSERT(a.size() == size);

		return RunRecord_s<typename function<F>::R>::fwrap(enable ? this : nullptr, wrap, f, id, pp, t0, *(TL_tuple<TL0>*)t1, p...);
	}

	struct SafeRecorder {
		COMRecording*	rec;
		bool			enable;
		SafeRecorder(COMRecording *rec) : rec(rec), enable(rec->enable) {
			if (enable)
				rec->recording_mutex.lock();
		}
		~SafeRecorder() {
			if (enable)
				rec->recording_mutex.unlock();
		}
		explicit		operator bool() const { return enable; }
		COMRecording*	operator->()	const { return rec; }

		SafeRecorder& Add(uint16 id, size_t size, const void *p) {
			memcpy(rec->get_space(id, size), p, size);
			return *this;
		}
		template<typename T> SafeRecorder& Add(uint16 id, const T &t) {
			return Add(id, sizeof(T), &t);
		}

	};
	SafeRecorder Safe() {
		return this;
	}
};

//-----------------------------------------------------------------------------
//	Wrapping
//-----------------------------------------------------------------------------

struct ReWrapper : static_list<ReWrapper>, virtfunc<void*(REFIID,void*)> {
	template<typename T> ReWrapper(const T *t) : virtfunc<void*(REFIID,void*)>(t) {}
};
template<typename T> struct ReWrapperT : ReWrapper {
	void	*operator()(REFIID riid, void *p) const {
		if (riid == __uuidof(T))
			return com_wrap_system->find_wrap_carefully((T*)p);
		return 0;
	}
	ReWrapperT() : ReWrapper(this) {}
};

template<class T> class Wrap;
template<class T, class B> class Wrap2;

template<class I> inline void block_shuffle_down(I a, I b) {
	--b;
	while (a != b) {
		I	a0	= a;
		swap(*a0, *++a);
	}
}

template<class I> inline void block_shuffle_up(I a, I b) {
	--b;
	while (a != b) {
		I	b0	= b;
		swap(*b0, *--b);
	}
}

//override this when necessary
template<typename T> struct	Wrappable { typedef T type; };

template<typename T> static typename Wrappable<T>::type	*GetWrappable(T *p)	{
	return static_cast<typename Wrappable<T>::type*>(p);
}

//---------------------------------
// _com_wrap
//---------------------------------
struct _com_wrap {
//	LONG	refs	= 1;
	_com_wrap();
	virtual ~_com_wrap();
};

template<typename T> Wrap<T>* make_wrap_orphan(T* p) {
	ISO_ASSERT(p);
	return com_wrap_system->make_wrap(p);
}

struct _com_wrap_system {
	hash_map<void*,_com_wrap*, true>	to_wrap;
	hash_set<_com_wrap*, true>			is_wrap;
	dynamic_array<deferred_deletes>		dd;

	_com_wrap_system() : to_wrap(1024), is_wrap(1024)	{}
	~_com_wrap_system() {
		//for (auto &i : dd)
		//	i.reset();
	}

	void	defer_deletes(int num_frames)	{
		dd.resize(num_frames);
	}
	void	end_frame()	{
		if (!dd.empty()) {
			dd.back().process();
			block_shuffle_up(dd.begin(), dd.end());
		}
	}

	void	remove(_com_wrap *p, void *orig) {
		ISO_VERIFY(is_wrap.remove(p));
		ISO_VERIFY(to_wrap.remove(orig));
		if (!dd.empty())
			dd.front().push_back(p);
		else
			delete p;
	}
	bool	add(_com_wrap *p, void *orig) {
		if (dd.empty())
			return false;
		ISO_VERIFY(dd.front().undelete(p));
		ISO_ASSERT(!is_wrap.count(p));
		is_wrap.insert(p);
		to_wrap[orig]	= p;
		return true;
	}

	//---------------------------------
	// make_wrap:
	//---------------------------------

	template<typename T> Wrap<T>	*_make_wrap_nocheck(T *p) {
		ISO_ASSERT(p);
		auto *w		= new Wrap<T>;
		w->orig		= p;
		to_wrap[p]	= w;
		return w;
	}
	template<typename T> Wrap<T>	*_make_wrap(T *p) {
		ISO_ASSERT(p);
		ISO_ASSERT(!to_wrap[p].exists());
		auto *w		= new Wrap<T>;
		w->orig		= p;
		to_wrap[p]	= w;
		return w;
	}

	template<typename T> auto	*make_wrap(T *p) {
		return _make_wrap(GetWrappable(p));
	}
	template<typename T, typename... X> auto *make_wrap(T *p, const X&... x) {
		auto	*w = make_wrap(p);
		w->init(x...);
		return w;
	}
	template<typename T> auto *make_wrap_check(T *p) {
		if (auto *w = find_wrap_check(p))
			return w;
		return make_wrap(p);
	}
	template<typename T, typename... X> auto *make_wrap_check(T *p, const X&... x) {
		auto *w = make_wrap_check(p);
		w->init(x...);
		return w;
	}
	template<typename T> auto *make_wrap_always(T *p) {
		return _make_wrap_nocheck(GetWrappable(p));
	}
	template<typename T, typename... X> auto *make_wrap_always(T *p, const X&... x) {
		auto *w = make_wrap_always(p);
		w->init(x...);
		return w;
	}

	// pass through HRESULT
	template<typename T, typename... X> HRESULT make_wrap(HRESULT h, T **pp, const X&... x) {
		if (h == S_OK)
			*pp = make_wrap(*pp, x...);
		return h;
	}
	template<typename T, typename... X> HRESULT make_wrap_check(HRESULT h, T **pp, const X&... x) {
		if (h == S_OK)
			*pp = make_wrap_check(*pp, x...);
		return h;
	}

	// need QueryInterface
	template<typename T, typename T0, typename... X> HRESULT make_wrap_qi(HRESULT h, T0 **pp, const X&... x) {
		if (h == S_OK) {
			T	*t;
			h = (*pp)->QueryInterface(__uuidof(T), (void**)&t);
			if (SUCCEEDED(h)) {
				(*pp)->Release();
				*pp = make_wrap_always(t, x...);
			}
		}
		return h;
	}
	
	//---------------------------------
	// find_wrap: (expects to be passed original)
	//---------------------------------
	template<typename T> Wrap<T>	*_find_wrap(T *p) {
		_com_wrap	*w	= to_wrap[p];
		return ISO_VERIFY(static_cast<Wrap<T>*>(w));
	}
	template<typename T> auto		*find_wrap(T *p) {
		return p ? _find_wrap(GetWrappable(p)) : 0;
	}

	template<typename T> Wrap<T>	*_find_wrap_carefully(T *p) {
		if (p) {
			auto w = to_wrap[p];
			if (w.exists())
				return static_cast<Wrap<T>*>(w.or_default());

			if (!is_wrap.count(static_cast<Wrap<T>*>(p)))
				return (Wrap<T>*)make_wrap_orphan(p);
		}
		return static_cast<Wrap<T>*>(p);
	}
	template<typename T> auto		*find_wrap_carefully(T *p) {
		return p ? _find_wrap_carefully(GetWrappable(p)) : 0;
	}
	template<typename T> Wrap<T>	*_find_wrap_check(T *p) {
		auto w = to_wrap[p];
		if (w.exists())
			return static_cast<Wrap<T>*>(w.or_default());
		return 0;
	}
	template<typename T> auto		*find_wrap_check(T *p) {
		return _find_wrap_check(GetWrappable(p));
	}

	//---------------------------------
	// ReWrap - FindWrap in place (and addref)
	//---------------------------------
	template<typename T> void rewrap(T **pp) {
		if (pp && *pp) {
			*pp = find_wrap(*pp);
			//(*pp)->AddRef();
		}
	}
	template<typename T> HRESULT rewrap(HRESULT h, T **pp) {
		if (h == S_OK)
			rewrap(pp);
		return h;
	}
	template<typename T> void rewrap(T **pp, UINT num) {
		if (pp) {
			while (num--)
				rewrap(pp++);
		}
	}
	template<typename T> void rewrap_carefully(T **pp) {
		if (pp && (*pp = find_wrap_carefully(*pp)))
			;//(*pp)->AddRef();
	}
	template<typename T> HRESULT rewrap_carefully(HRESULT h, T **pp) {
		if (h == S_OK)
			rewrap_carefully(pp);
		return h;
	}
	template<typename T> void rewrap_carefully(T **pp, UINT num) {
		if (pp) {
			while (num--)
				rewrap_carefully(pp++);
		}
	}
	//HRESULT	rewrap_carefully(HRESULT h, void **pp) {
	//	if (SUCCEEDED(h))
	//		*pp = find_wrap_carefully((IUnknown*)*pp);
	//	return h;
	//}

	//---------------------------------
	// get_wrap: (expects to be passed wrapped)
	//---------------------------------
	template<typename T> Wrap<T>	*_get_wrap(T *p) {
		Wrap<T>	*w = static_cast<Wrap<T>*>(p);
		return is_wrap.count(w) ? w : find_wrap_carefully(p);
	}
	template<typename T> Wrap<T>	*get_wrap(T *p) {
		return _get_wrap(GetWrappable(p));
	}
	// need QueryInterface
	template<typename T, typename T0> HRESULT get_wrap_qi(HRESULT h, T0 **pp) {
		if (h == S_OK) {
			T	*t;
			h = (*pp)->QueryInterface(__uuidof(T), (void**)&t);
			if (SUCCEEDED(h)) {
				(*pp)->Release();
				*pp = _get_wrap(t);
			}
		}
		return h;
	}
	// from hash
	IUnknown						*get_wrap(REFIID riid, void *p);

//	static HRESULT GetWrap(HRESULT h, REFIID riid, void **pp) {
//		if (h == S_OK)
//			*pp = static_cast<IUnknown*>(static_cast<com_wrap<IUnknown>*>(GetWrap(riid, *pp)));
//		return h;
//	}
	
	//---------------------------------
	// unwrap: - get orig from wrapped
	//---------------------------------
	template<typename T> T		*_unwrap(T *p) {
		if (!p)
			return p;
		Wrap<T>	*w = static_cast<Wrap<T>*>(p);
		ISO_ASSERT(is_wrap.count(w));
		return w->orig;
	}
	template<typename T> T		*unwrap(T *p) {
		return _unwrap(GetWrappable(p));
	}
	// if not wrapped, make wrap
	template<typename T> T		*_unwrap_carefully(T *p) {
		if (p) {
			Wrap<T>	*w = static_cast<Wrap<T>*>(p);
			if (is_wrap.count(w))
				return w->orig;
			if (!to_wrap.check(p))
				make_wrap_orphan(p);
		}
		return p;
	}
	template<typename T> auto *unwrap_carefully(T *p) {
		return _unwrap_carefully(GetWrappable(p));
	}

	// if not wrapped, leave
	template<typename T> T		*_unwrap_safe(T *p) {
		if (p) {
			Wrap<T>	*w = static_cast<Wrap<T>*>(p);
			if (is_wrap.count(w))
				return w->orig;
		}
		return p;
	}
	template<typename T> auto *unwrap_safe(T *p) {
		return _unwrap_safe(GetWrappable(p));
	}
	// if not wrapped, return 0
	template<typename T> T		*_unwrap_test(T *p) {
		if (p) {
			Wrap<T>	*w = static_cast<Wrap<T>*>(p);
			if (is_wrap.count(w))
				return w->orig;
		}
		return 0;
	}
	template<typename T> auto *unwrap_test(T *p) {
		return _unwrap_test(GetWrappable(p));
	}

	template<typename T> T *const *unwrap(T **unwrapped, T *const *pp, UINT num) {
		for (int i = 0; i < num; i++)
			unwrapped[i] = unwrap(pp[i]);
		return (T *const *)unwrapped;
	}

};

static singleton<_com_wrap_system> com_wrap_system;

inline _com_wrap::_com_wrap() {
	ISO_ASSERT(com_wrap_system->is_wrap.count(this) == 0);
	com_wrap_system->is_wrap.insert(this);
}
inline _com_wrap::~_com_wrap() {
	com_wrap_system->is_wrap.remove(this);
}

template<class T> class com_wrap : public T, public _com_wrap {
public:
	T		*orig;

	~com_wrap() {
		//if (orig)
		//	orig->Release();
	}

	void	mark_dead() {
		T	*p = orig;
		orig = (T*)(uintptr_t(p) | 1);
		com_wrap_system->remove(this, p);
	}
	void	mark_undead() {
		orig = (T*)(uintptr_t(orig) & ~1);
		ISO_VERIFY(com_wrap_system->add(this, orig));
	}
	constexpr bool	is_orig_dead()	const {
		return uintptr_t(orig) & 1;
	}

	ULONG STDMETHODCALLTYPE	AddRef() {
		if (is_orig_dead())
			mark_undead();
		return orig->AddRef();// - 1;
	}
	ULONG STDMETHODCALLTYPE	Release() {
		ULONG n = orig->Release();
		if (n == 0)
			mark_dead();
		return n;// - 1;
	}
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID riid, void **pp) {
		if (check_interface<IUnknown>	(this, riid, pp)
		||	check_interface<T>			(this, riid, pp)
		)	return S_OK;

		auto	h = orig->QueryInterface(riid, pp);
		if (h == S_OK) {
			auto	p = com_wrap_system->get_wrap(riid, *pp);
			p->AddRef();
			((IUnknown*)(*pp))->Release();
			*pp = p;//static_cast<IUnknown*>(static_cast<com_wrap<IUnknown>*>(p));
		}
		return h;
	}
};

inline IUnknown	*_com_wrap_system::get_wrap(REFIID riid, void *p) {
	auto w = to_wrap[p];
	if (auto w2 = w.or_default())
		return static_cast<IUnknown*>(static_cast<com_wrap<IUnknown>*>(w2));

	return (IUnknown*)p;
}

template<> class Wrap<IUnknown> : public com_wrap<IUnknown> {};

template<typename T> void unwrapped<T>::operator=(const T _t)	{
	t = com_wrap_system->unwrap_carefully(_t);
}


} // namespace iso

#endif // HOOK_COM_H
