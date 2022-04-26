#ifndef HASH_H
#define HASH_H

#include "base/defs.h"
#include "base/bits.h"
#include "base/atomic.h"
#include "hashes/fnv.h"
#include "thread.h"

namespace iso {

//-----------------------------------------------------------------------------
//	hash_key
//-----------------------------------------------------------------------------
iso_export uint32 string_hash(const char *s);

template<typename T, int N> struct xor_together	{ static inline T f(const T *p) { return xor_together<T, N/2>::f(p) ^ xor_together<T, (N + 1)/2>::f(p + N/2); } };
template<typename T> struct xor_together<T, 1>	{ static inline T f(const T *p) { return *p; } };

template<typename T1, typename T2> inline auto hash_combine(T1 seed, T2 value) { return seed ^ value + 0x9e3779b9 + (seed<<6) + (seed>>2); }

template<typename T>	enable_if_t<(sizeof(T) <= 4),					uint32> hash(const T &t)	{ return read_bytes<uint32>(t); }
template<typename T>	enable_if_t<(sizeof(T) > 4 && sizeof(T) <= 8),	uint64> hash(const T &t)	{ return read_bytes<uint64>(t); }
//template<typename T>	enable_if_t<(sizeof(T) >  8),					uint64> hash(const T &t)	{ typedef uint_t<min(alignof(T), 8)> I; return xor_together<I, sizeof(T) / sizeof(I)>::f((const I*)&t); }
template<typename T>	enable_if_t<(sizeof(T) >  8),					uint64> hash(const T &t)	{ typedef uint_t<lowest_set(sizeof(T)|8)> I; return _FNVa<uint64>((I*)&t, sizeof(T) / sizeof(I)); }

template<typename T>	auto hash(T *t)				{ return (uintptr_t)t / alignof(T); }
inline					auto hash(void *t)			{ return (uintptr_t)t; }
inline					auto hash(const void *t)	{ return (uintptr_t)t; }
inline					auto hash(const char *t)	{ return string_hash(t); }


template<typename T> struct hash_type : T_type<decltype(hash(declval<T>()))> {};
template<typename T> using hash_t = typename hash_type<T>::type;


template<typename T> struct hash_s {
	static auto f(const T &t)		{ return hash(t); }
};

template<typename T> struct hash_s<T&> {
	static auto f(const T &t)		{ return hash(&t); }
};
template<typename K, typename T> auto hashk(const T& t) {
	return hash_s<K>::f(t);
}

//-----------------------------------------------------------------------------
//	hash_helpers
//-----------------------------------------------------------------------------

// in defs.cpp
extern int64	hash_put_misses, hash_put_count;
extern int64	hash_get_misses, hash_get_count;

template<typename H> struct hash_helpers {
	static H	UNUSED(H i)			{ return i + 1; }
	static H	FREED(H i)			{ return i + 2; }
	static bool	IS_USED(H i, H k)	{ return k != UNUSED(i) && k != FREED(i); }

	static const H	*next_valid(const H *b, const H *e, const H *i) {
		while (i < e && !IS_USED(H(i - b), *i))
			++i;
		return i;
	}
	static const H	*prev_valid(const H *b, const H *i) {
		while (i >= b && !IS_USED(H(i - b), *i))
			--i;
		return i;
	}

	class iterator {
	protected:
		const H		*b, *e, *i;
		void		next()	{ i = next_valid(b, e, i + 1); }
		void		prev()	{ i = prev_valid(b, i - 1); }
	public:
		iterator(const H *b, const H *e)				: b(b), e(e), i(e) {}
		iterator(const H *b, const H *e, const H *i)	: b(b), e(e), i(next_valid(b, e, i)) {}
		H			hash()					const	{ return *i; }
		const H*	get()					const	{ return i; }
		bool operator==(const iterator &b)	const 	{ return i == b.i; }
		bool operator!=(const iterator &b)	const	{ return i != b.i; }
		iterator&	operator++()					{ next(); return *this; }
		iterator&	operator--()					{ prev(); return *this; }
		iterator&	operator+=(uint32 i)			{ while (i--) next(); return *this; }
		iterator	operator+(uint32 i)		const	{ return iterator(*this) += i; }
	};

	template<typename T> struct iteratorT : iterator {
		iteratorT(const iterator &i) : iterator(i) {}
		iteratorT&	operator++()					{ iterator::next(); return *this; }
		iteratorT&	operator--()					{ iterator::prev(); return *this; }
		iteratorT&	operator+=(uint32 i)			{ iterator::operator+=(i); return *this; }
		iteratorT	operator+(uint32 i)		const	{ return iterator::operator+(i); }
		T*			get()					const	{ return (T*)this->e + (this->i - this->b); }
		T&			operator*()				const	{ return *get(); }
		T*			operator->()			const	{ return get(); }
	};

	// returns entry where key is, or 0
	static const H *check(H h, const H *b, uint32 mask) {
		ISO_ON_DEBUG(++hash_get_count);
		for (H k = h; ; ++k) {
			H		i	= k & mask;
			auto	p	= b + i;
			if (*p == h)
				return p;
			if (*p == UNUSED(i))
				return 0;
			ISO_ON_DEBUG(++hash_get_misses);
		}
	}

	static void	reset(H *b, uint32 n) {
		for (int i = 0; i < n; i++)
			b[i] = UNUSED(i);
	}

	static int check_index(int i, H *b, size_t max) {
		return i >= 0 && i < max && IS_USED(i, b[i]) ? i : -1;
	}
};

//-----------------------------------------------------------------------------
//	hash_access - single threaded
//-----------------------------------------------------------------------------

template<typename H, bool MT> struct hash_access;

template<typename H> struct hash_access<H, false> : hash_helpers<H> {
	typedef hash_helpers<H>	B;
	uint32		curr_size;
	
	static bool access()				{ return true; }
	static bool try_exclusive()			{ return true; }
	static void release_exclusive()		{}
	static void add_pending()			{}
	static void sub_pending()			{}

	template<typename L> auto put(H h, H *b, uint32 mask, L&& lambda) {
		H	*freed = 0;

		for (H k = h; ; ++k) {
			H	i	= k & mask;
			H*	p	= b + i;

			if (*p == h)
				return lambda(p, false);

			if (*p == B::UNUSED(i)) {
				if (freed)
					p = freed;
				*p = h;
				++curr_size;
				ISO_ON_DEBUG(hash_put_misses += k - h;++hash_put_count);
				return lambda(p, true);
			}
			if (!freed && *p == B::FREED(i))
				freed = p;
		}
	}

	H* quick_remove(H *p, H h, H *b) {
		*p = B::FREED(H(p - b));
		--curr_size;
		return p;
	}
	int	mark_unused(H *h, H *b, uint32 mask);

	hash_access() : curr_size(0) {}
	bool			empty()				const	{ return curr_size == 0; }
	uint32			size()				const	{ return curr_size;	}
	uint32			potential_size()	const	{ return curr_size;	}
};

template<typename H> int hash_access<H, false>::mark_unused(H *h, H *b, uint32 mask) {
	int		num_removed = 0;
	uint32	i	= uint32(h - b);
	uint32	i1	= (i + 1) & mask;

	*h = B::FREED(i);

	if (b[i1] == B::UNUSED(i1)) {
		while (*h == B::FREED(i)) {
			*h = B::UNUSED(i);
			++num_removed;
			i = (i - 1) & mask;
			h = b + i;
		}
	}

	return num_removed;
}
//-----------------------------------------------------------------------------
//	hash_access - multi threaded
//-----------------------------------------------------------------------------

template<typename L> struct lambda_destruct {
	L	lambda;
	lambda_destruct(lambda_destruct &&) = default;
	lambda_destruct(L &&lambda) : lambda(forward<L>(lambda)) {}
	~lambda_destruct()	{ lambda(); }
};
template<typename L> lambda_destruct<L> make_lambda_destruct(L&& lambda) { return forward<L>(lambda); }


template<typename H> struct hash_access<H, true> : hash_helpers<H> {
	typedef hash_helpers<H>	B;
	atomic<uint32>		curr_size;

	mutable atomic<int>	refs, pending;
	mutable Event		event;

	auto access() const {
		for (;;) {
			int	r = refs;
			if (r < 0)
				event.wait();
			else if (refs.cas(r, r + 1))
				return make_lambda_destruct([this]() {--refs;});
		}
	}
	void add_pending() const {
		++pending;
	}
	void sub_pending() const {
		--pending;
	}

	bool try_exclusive() {
		if (refs.cas(0, -1)) {
			event.clear();
			return true;
		}
		return false;
	}
	void release_exclusive() {
		refs		= 0;
		event.signal();
	}

	template<typename L> auto put(H h, H *b, uint32 mask, L&& lambda) {
		H	*freed = 0;

		for (uint32 k = h; ; ++k) {
			H	i	= k & mask;
			H*	p	= b + i;
			H	old	= *p;
			if (old == h)
				return lambda(p, false);

			if (old == B::UNUSED(i)) {
				if (freed) {
					if (_cas(freed, B::FREED(freed - b), h)) {
						++curr_size;
						return lambda(freed, true);
					}
				} else {
					if (_cas(p, old, h)) {
						++curr_size;
						return lambda(p, true);
					}
				}
				// restart from beginning
				k = h - 1;
			}
			if (!freed && old == B::FREED(i))
				freed = p;
		}
	}

	H* quick_remove(H *h, H key, H *b) {
		if (_cas(h, key, FREED(h - b))) {
			--curr_size;
			return h;
		}
		return nullptr;
	}
	int	mark_unused(H *h, H *b, uint32 mask);

	hash_access() : curr_size(0), refs(0), event(true) {}
	hash_access(hash_access&&) = default;
	bool			empty()				const	{ return curr_size == 0; }
	uint32			size()				const	{ return curr_size;	}
	uint32			potential_size()	const	{ return curr_size + pending;	}
};

template<typename H> int hash_access<H, true>::mark_unused(H *h, H *b, uint32 mask) {
	int		num_removed = 0;
	uint32	i	= uint32(h - b);
	uint32	i1	= (i + 1) & mask;

	if (b[i1] == B::UNUSED(i1)) {
		while (_cas(h, B::FREED(i), B::UNUSED(i))) {
			++num_removed;
			i = (i - 1) & mask;
			h = b + i;
		}
	}

	return num_removed;
}

//-----------------------------------------------------------------------------
//	hash_storage - static
//-----------------------------------------------------------------------------

template<typename H, bool MT, int HASH_SIZE> class hash_static : public hash_access<H, MT> {
	typedef	hash_access<H, MT>	B;
protected:
	H			hashes[HASH_SIZE];

	const H*	check0(H h) const {
		return B::check(h, hashes, HASH_SIZE - 1);
	}

	template<typename L> auto put0(H h, L&& lambda) {
		return B::put(h, hashes, HASH_SIZE - 1, forward<L>(lambda));
	}

	H*	remove0(H *p) {
		H	h	= *p;
		if (IS_USED(p - hashes, h) && B::quick_remove(p, h, hashes)) {
			B::mark_unused(p, hashes, HASH_SIZE - 1);
			return p;
		}
		return nullptr;
	}

	H*	remove0(H h) {
		auto	a = B::access();
		if (auto p = uconst(check0(h))) {
			if (B::quick_remove(p, h, hashes)) {
				B::mark_unused(p, hashes, HASH_SIZE - 1);
				return p;
			}
		}
		return nullptr;
	}
public:
	using typename B::iterator;
	void		clear()				{ B::reset(hashes, HASH_SIZE); B::curr_size = 0; }
	bool		count(H h)	const	{ return !!check0(h); }
	iterator	begin()		const	{ return iterator(hashes, hashes + HASH_SIZE, hashes); }
	iterator	end()		const	{ return iterator(hashes, hashes + HASH_SIZE); }
	iterator	find(H h)	const	{ H *p = check0(h); return p ? iterator(hashes, hashes + HASH_SIZE, p) : end(); }
};

//	static<T>

template<typename H, typename T, bool MT, int HASH_SIZE> class hash_storage : hash_static<H, MT, HASH_SIZE> {
	typedef	hash_static<H, MT, HASH_SIZE> B;
	T			values[HASH_SIZE];

	T*			getval(const H *h)			{ return values + (h - B::hashes); }
	const T*	getval(const H *h)	const	{ return values + (h - B::hashes); }
public:
	using iterator			= typename B::template iteratorT<T>;
	using const_iterator	= typename B::template iteratorT<const T>;

	hash_storage(int = 8)	{ B::reset(B::hashes, HASH_SIZE); }

	int index_of(const T *t) const {
		return t ? B::check_index(t - values, B::hashes, HASH_SIZE) : -1;
	}
	T*	by_index(int i) const {
		return B::check_index(i, B::hashes, HASH_SIZE) >= 0 ? values + i : nullptr;
	}
	const T* check(H h) const {
		if (auto p = (B::access(), check0(h)))
			return getval(p);
		return 0;
	}
	T*	remove(H h) {
		if (auto p = B::remove0(h))
			return getval(p);
		return nullptr;
	}
	// single-threaded only!
	T*	remove(T *t) {
		int	i = t - values;
		return i >= 0 && i < HASH_SIZE && B::remove0(B::hashes + i) ? t : nullptr;
	}
	optional<const T&>	get(H h) const {
		if (auto p = check(h))
			return *p;
		return none;
	}
	optional<T&>		get(H h) {
		if (auto p = check(h))
			return *unconst(p);
		return none;
	}
	T&					put(H h) {
		return B::access(), *getval(B::put0(h, [](H *p, bool) {
			return p;
		}));
	}
	template<typename U> T& put(const H h, U &&u) {
		return B::access(), *B::put0(h, [this, &u](H *p, bool) {
			auto r = getval(p); *r = forward<U>(u); return r;
		});
	}
	const_iterator	begin()		const	{ return B::begin(); }
	const_iterator	end()		const	{ return B::end(); }
	const_iterator	find(H h)	const	{ return B::find(h); }
	iterator		begin()				{ return B::begin(); }
	iterator		end()				{ return B::end(); }
	iterator		find(H h)			{ return B::find(h); }
};

//	static<void>

template<typename H, bool MT, int HASH_SIZE> class hash_storage<H, void, MT, HASH_SIZE> : hash_static<H, MT, HASH_SIZE> {
	typedef	hash_static<H, MT, HASH_SIZE> B;
public:
	hash_storage(int = 8)	{ B::reset(B::hashes, HASH_SIZE); }

	bool	check(H h) const {
		return !!(B::access(), B::check0(h));
	}
	bool	remove(H h) {
		return !!B::remove0(h);
	}
	void	put(H h) {
		B::put0(h, [this](H *p, bool) {});
	}
};

//-----------------------------------------------------------------------------
//	hash_storage - dynamic
//-----------------------------------------------------------------------------

template<typename H, bool MT> class hash_dynamic : public hash_access<H, MT> {
	typedef	hash_access<H, MT>	B;
protected:
	decltype(B::curr_size)	max_size, num_free;
	H		*hashes;

	const H* check0(H h) const {
		return B::check(h, hashes, max_size - 1);
	}

	template<typename L> auto put0(H h, L&& lambda) {
		return B::put(h, hashes, max_size - 1, forward<L>(lambda));
	}
	
	// single-threaded only!
	H*	safe_remove(H *p) {
		H	h	= *p;
		if (B::IS_USED(p - hashes, h) && B::quick_remove(p, h, hashes)) {
			num_free += 1 - B::mark_unused(p, hashes, max_size - 1);
			return p;
		}
		return nullptr;
	}
	H*	safe_remove(H h) {
		auto	a = B::access();
		if (auto *p = unconst(check0(h))) {
			if (B::quick_remove(p, h, hashes)) {
				num_free += 1 - B::mark_unused(p, hashes, max_size - 1);
				return p;
			}
		}
		return nullptr;
	}
	void create_entries(uint32 n, size_t entry_size) {
		num_free	= 0;
		max_size	= 1 << log2_ceil(n);
		hashes		= (H*)malloc((sizeof(H) + entry_size) * max_size);
		B::reset(hashes, max_size);
	}

	bool maybe_expand() {
		if (B::potential_size() * 2 > max_size) {
			do {
				if (B::try_exclusive())
					return true;
			} while (B::potential_size() * 4 >= max_size * 3);
		}
		return false;
	}
	~hash_dynamic()			{ free(hashes); }

public:
	using typename B::iterator;
	void		clear()				{ B::reset(hashes, max_size); B::curr_size = 0; }
	bool		count(H h)	const	{ return !!check0(h); }
	iterator	begin()		const	{ return {hashes, hashes + max_size, hashes}; }
	iterator	end()		const	{ return {hashes, hashes + max_size}; }
	iterator	find(H h)	const	{ auto p = check0(h); return p ? iterator(hashes, hashes + max_size, p) : end(); }
};

//-------------------------------------
//	dynamic<T>
//-------------------------------------

template<typename H, typename T, bool MT> class hash_storage<H, T, MT, 0> : public hash_dynamic<H, MT> {
protected:
	typedef hash_dynamic<H, MT>	B;
	T*	values()			const		{ return (T*)(void*)(this->hashes + this->max_size); }
	T*	getval(const H *h)	const		{ return h ? values() + (h - this->hashes) : nullptr; }

	void erase_entries() {
		T	*v = values();
		for (int i = 0; i < B::max_size; i++) {
			if (B::IS_USED(i, B::hashes[i]))
				v[i].~T();
		}
	}
	void copy_entries(H *hashes2, uint32 max_size2) {
		T	*v2 = (T*)(hashes2 + max_size2);
		for (int i = 0; i < max_size2; i++) {
			H	h = hashes2[i];
			if (B::IS_USED(i, h))
				B::put0(h, [this, v = v2[i]](H *p, bool) { new(getval(p)) T(v); });
		}
	}
	void resize(uint32 newsize);

public:
	using B::hashes; using B::max_size; using B::curr_size; using B::num_free;
	using iterator			= typename B::template iteratorT<T>;
	using const_iterator	= typename B::template iteratorT<const T>;

	hash_storage(int n = 8)	{ B::create_entries(n, sizeof(T)); }
	~hash_storage()			{ erase_entries(); }

	hash_storage(const hash_storage &b) {
		B::create_entries(b.curr_size, sizeof(T));
		copy_entries(b.hashes, b.max_size);
	}
	hash_storage(hash_storage &&b) : B(move(b)) {
		b.hashes	= 0;
		b.curr_size	= 0;
		b.max_size	= 0;
	}
	hash_storage& operator=(const hash_storage &b) {
		if (b.curr_size * 2 > B::max_size) {
			operator=(hash_storage(b.curr_size));
		} else {
			clear();
		}
		copy_entries(b.hashes, b.max_size);
		return *this;
	}
	hash_storage& operator=(hash_storage &&b) {
		raw_swap(*this, b);
		return *this;
	}
	void	clear()	{
		erase_entries(); B::clear();
	}
	
	int index_of(const T *t) const {
		return B::check_index(t - values(), hashes, max_size);
	}
	T*	by_index(int i) const {
		return B::check_index(i, hashes, max_size) ? values() + i : nullptr;
	}

	T*	check(H h) const {
		if (auto p = (B::access(), B::check0(h)))
			return getval(p);
		return nullptr;
	}
	T*	remove(H h) {
		if (B::num_free >= B::max_size / 2 && B::try_exclusive()) {
			resize(B::potential_size() * 2);
			B::release_exclusive();
		}
		return getval(B::safe_remove(h));
	}

	// single-threaded only!
	T*	remove(T *t) {
		if (B::num_free >= B::max_size / 2)
			resize(B::potential_size() * 2);
		int	i = t - values();
		return i >= 0 && i < max_size && B::safe_remove(hashes + i) ? t : nullptr;
	}

	optional<T&>	get(H h) const {
		if (auto t = check(h))
			return *t;
		return none;
	}
	T&	put(H h) {
		B::add_pending();
		if (B::maybe_expand()) {
			resize(B::potential_size() * 2);
			B::release_exclusive();
		}
		T	*p = (B::access(), B::put0(h, [this](H *p, bool alloc) {
			auto	v = getval(p);
			if (alloc)
				new(v) T();
			return v;
		}));
		B::sub_pending();
		return *p;
	}
	template<typename U> T& put(H h, U &&u) {
		B::add_pending();
		if (B::maybe_expand()) {
			resize(B::potential_size() * 2);
			B::release_exclusive();
		}
		T	*p = (B::access(), B::put0(h, [this, &u](H *p, bool alloc) {
			auto	v = getval(p);
			if (alloc)
				new(v) T(forward<U>(u));
			else
				*v = forward<U>(u);
			return v;
		}));
		B::sub_pending();
		return *p;
	}

	const_iterator	begin()		const	{ return B::begin(); }
	const_iterator	end()		const	{ return B::end(); }
	const_iterator	find(H h)	const	{ return B::find(h); }
	iterator		begin()				{ return B::begin(); }
	iterator		end()				{ return B::end(); }
	iterator		find(H h)			{ return B::find(h); }
};

template<typename H, typename T, bool MT> void hash_storage<H, T, MT, 0>::resize(uint32 newsize) {
	hash_storage	new_map(newsize);
	T		*vals	= values();
	for (uint32 i = 0; i < max_size; i++) {
		auto h = hashes[i];
		if (B::IS_USED(i, h))
			new_map.put0(h, [&new_map, &v = vals[i]](H *p, bool) { new(new_map.getval(p)) T(move(v)); });
	}
	ISO_ASSERT(new_map.size() == B::size());
	swap(max_size, new_map.max_size);
	swap(hashes, new_map.hashes);
	B::num_free	= 0;
}

//-------------------------------------
//	dynamic<void>
//-------------------------------------

template<typename H, bool MT> class hash_storage<H, void, MT, 0> : public hash_dynamic<H, MT>  {
	typedef hash_dynamic<H, MT>	B;

	void copy_entries(H *hashes2, uint32 max_size2) {
		for (int i = 0; i < max_size2; i++) {
			H	h = hashes2[i];
			if (B::IS_USED(i, h))
				B::put0(h, [](H *p, bool) {});
		}
	}

	void resize(uint32 newsize);

public:
	typedef typename B::iterator	iterator, const_iterator;

	hash_storage(int n = 8)	{ B::create_entries(n, 0); }

	hash_storage(const hash_storage &b) {
		B::create_entries(b.curr_size, 0);
		copy_entries(b.hashes, b.max_size);
	}
	hash_storage(hash_storage &&b) : B(move(b)) {
		b.hashes	= 0;
		b.curr_size	= 0;
		b.max_size	= 0;
	}
	hash_storage& operator=(const hash_storage &b) {
		if (b.curr_size * 2 > B::max_size) {
			operator=(hash_storage(b.curr_size));
		} else {
			B::clear();
		}
		copy_entries(b.hashes, b.max_size);
		return *this;
	}
	hash_storage& operator=(hash_storage &&b) {
		raw_swap(*this, b);
		return *this;
	}

	bool	check(H h) const {
		return !!(B::access(), B::check0(h));
	}
	bool	remove(H h) {
		if (B::num_free >= B::max_size / 2 && B::try_exclusive()) {
			resize(B::curr_size);
			B::release_exclusive();
		}
		return !!B::safe_remove(h);
	}
	void	put(H h) {
		if (B::maybe_expand()) {
			resize(B::potential_size() * 2);
			B::release_exclusive();
		}
		return B::put0(h, [](H *p, bool alloc) {});
	}
};

template<typename H, bool MT> void hash_storage<H, void, MT, 0>::resize(uint32 newsize) {
	hash_storage	new_map(newsize);
	for (uint32 i = 0; i < B::max_size; i++) {
		auto h = B::hashes[i];
		if (B::IS_USED(i, h))
			new_map.put0(h, [](H *p, bool) {});
	}

	swap(B::max_size, new_map.max_size);
	swap(B::hashes, new_map.hashes);
	B::num_free	= 0;
}

template<typename H, typename T, bool MT = false, int HASH_BITS = 0> using hash_map0 = hash_storage<H, T, MT, (HASH_BITS ? (1<<HASH_BITS) : 0)>;

//-----------------------------------------------------------------------------
//	hash_map
//-----------------------------------------------------------------------------

template<typename K, typename T, bool MT = false, int HASH_BITS = 0> class hash_map : public hash_map0<hash_t<K>, T, MT, HASH_BITS> {
	typedef hash_t<K>						H;
	typedef hash_map0<H,T,MT,HASH_BITS>		B;
public:
	hash_map(int init = 8)			: B(init)	{}
	hash_map(initializer_list<pair<K, T>> vals)	{ for (auto &i : vals) put(i.a, i.b); }

	auto			check(const K &key) const			{ return B::check(hashk<K>(key)); }
	T*				remove(const K &key)				{ return B::remove(hashk<K>(key)); }
	T*				remove(T *t)						{ return B::remove(t);	}
	auto			get(const K &k)		const			{ return B::get(hashk<K>(k)); }
	auto&			put(const K &k)						{ return B::put(hashk<K>(k)); }
	template<typename U> auto& put(const K &k, U &&u)	{ return B::put(hashk<K>(k), forward<U>(u)); }
	auto			find(const K &k)	const			{ return B::find(hashk<K>(k)); }
	bool			count(const K &k)	const			{ return !!check(k); }

	auto	operator[](const K &k)		const			{ return get(k); }
	auto	operator[](const K &k)						{ return putter<B, H, optional<T&>>(*this, hashk<K>(k)); }

	friend void swap(hash_map &a, hash_map &b)	{ raw_swap(a, b); }
};

//-------------------------------------
//	hash_map_with_key
//-------------------------------------

template<typename K, typename T, bool MT = false, int HASH_BITS = 0> class hash_map_with_key : public hash_map0<hash_t<K>, pair<K,T>, MT, HASH_BITS> {
	typedef hash_t<K>						H;
	typedef	pair<K,T>						P;
	typedef hash_map0<H, P, MT, HASH_BITS>	B;
	using B::max_size; using B::curr_size; using B::num_free;

	static const pair<K,T>	*as_pair(const T *t) {
		return (const pair<K,T>*)T_get_enclosing<pair_helper<K,T>>(t, &pair<K,T>::b);
	}
public:
	hash_map_with_key(int init = 8)							: B(init)			{}
	hash_map_with_key(initializer_list<pair<K, T>> vals)	: B(vals.size())	{ for (auto &i : vals) put(i.a, i.b); }

	T* check(const K &k) const {
		if (auto *e = B::check(hashk<K>(k)))
			return &e->b;
		return 0;
	}
	int index_of(const T *t) const {
		return t ? B::index_of(as_pair(t)) : -1;
	}
	T*	by_index(int i) const {
		return &B::by_index(i)->b;
	}
	optional<K&>	key(const T *t) const {
		auto	p = as_pair(t);
		if (B::index_of(p) >= 0)
			return p->a;
		return none;
	}
	T*	remove(const K &key) {
		if (auto *e = B::remove(hashk<K>(key)))
			return &e->b;
		return 0;
	}
	T*	remove(T *t) {
		if (auto *e = B::remove(unconst(as_pair(t))))
			return t;
		return 0;
	}
	optional<T&>	get(const K &k) const {
		if (auto *e = B::check(hashk<K>(k))) {
			ISO_ASSERT(e->a == k);
			return e->b;
		}
		return none;
	}
	template<typename K1> T&	put(K1 &&k) {
		if (curr_size >= max_size / 2)
			B::resize(max_size * 2);

		return B::put0(hashk<K>(k), [this, &k](H *p, bool alloc) {
			auto *v	= B::getval(p);
			if (alloc)
				new(v) P(forward<K1>(k));
			return v;
		})->b;
	}
	template<typename K1, typename T1> T&	put(K1 &&k, T1 &&t) {
		if (B::curr_size >= B::max_size / 2)
			resize(B::max_size * 2);

		return B::put0(hashk<K>(forward<K1>(k)), [this, &k, &t](H *p, bool alloc) {
			auto *v	= getval(p);
			if (alloc)
				new(v) P(forward<K1>(k), forward<T1>(t));
			else
				v->b = forward<T1>(t);
			return v;
		})->b;
	}

	optional<T&>				operator[](const K &k)	const			{ return get(k); }
	putter<hash_map_with_key, K, optional<T&>>	operator[](const K &k)	{ return {*this, k}; }

	auto		find(const K &k)	const	{ return B::find(hashk<K>(k)); }
	bool		count(const K &k)	const	{ return !!B::check0(hashk<K>(k)); }
	
	const B&	base()				const	{ return *this; }
	friend void swap(hash_map_with_key &a, hash_map_with_key &b)	{ raw_swap(a, b); }
};

//-----------------------------------------------------------------------------
//	sets
//-----------------------------------------------------------------------------

template<typename H, typename T, bool MT = false, int HASH_BITS = 0> class hash_set0 : public hash_map0<H, T, MT, HASH_BITS> {
protected:
	typedef hash_map0<H, T, MT, HASH_BITS>		B;

	bool check_all(const hash_set0 &b) const	{
		for (auto i = B::begin(), e = B::end(); i != e; ++i) {
			if (!b.check(i.hash()))
				return false;
		}
		return true;
	}
	void intersect(const hash_set0 &b) {
		for (auto i = B::begin(), e = B::end(); i != e; ++i) {
			if (!b.check(i.hash()))
				B::safe_remove(unconst(i.hash()));
		}
	}
	void difference(const hash_set0 &b) {
		for (auto i = B::begin(), e = B::end(); i != e; ++i) {
			if (b.check(i.hash()))
				B::safe_remove(unconst(i.hash()));
		}
	}
	void onion(const hash_set0 &b) {
		for (auto i = b.begin(), e = b.end(); i != e; ++i)
			B::put(i.hash(), *i);
	}
	void exclusive(const hash_set0 &b) {
		for (auto i = b.begin(), e = b.end(); i != e; ++i) {
			auto	k = i.hash();
			if (auto j = B::check(k))
				B::safe_remove(j);
			else
				B::put(k, *i);
		}
	}
public:
	hash_set0(int init = 8)		: B(init)		{}

	bool operator==(const hash_set0 &b)	const	{ return B::size() == b.size() && check_all(b); }
	bool operator< (const hash_set0 &b)	const	{ return B::size() <  b.size() && check_all(b); }
	bool operator<=(const hash_set0 &b)	const	{ return B::size() <= b.size() && check_all(b); }
	bool operator> (const hash_set0 &b)	const	{ return b <  *this; }
	bool operator>=(const hash_set0 &b)	const	{ return b <= *this; }
};

//-------------------------------------
//	hash_set
//-------------------------------------

template<typename K, bool MT = false, int HASH_BITS = 0> class hash_set : public hash_set0<hash_t<K>, void, MT, HASH_BITS> {
	typedef hash_t<K>			H;
	typedef hash_set0<H, void, MT, HASH_BITS>	B;
public:
	hash_set(int init = 8)				: B(init)		{}
	hash_set(initializer_list<K> keys)	: B(keys.size()) { for (auto &i : keys) insert(i); }

	bool		count(const K &key) const			{ return !!B::check(hashk<K>(key)); }
	void		insert(const K &key)				{ B::put(hashk<K>(key)); }
	void		insert(initializer_list<K> keys)	{ for (auto &i : keys) insert(i); }
	bool		remove(const K &key)				{ return !!B::remove(hashk<K>(key)); }
	bool		check_insert(const K &key)			{ H h = hashk<K>(key); return B::check(h) || (B::put(h), false); }
	template<typename I> void insert(I a, I b)		{ while (a != b) insert(*a++); }
	template<typename I> void remove(I a, I b)		{ while (a != b) remove(*a++); }

	_not<hash_set>	operator~() const				{ return *this; }
	auto& operator&=(const hash_set &b)				{ B::intersect(b); return *this; }
	auto& operator&=(const _not<hash_set> &b)		{ B::difference(b.t); return *this; }
	auto& operator|=(const hash_set &b)				{ B::onion(b); return *this; }
	auto& operator^=(const hash_set &b)				{ B::exclusive(b); return *this; }
	auto& operator*=(const hash_set &b)				{ return operator&=(b); }
	auto& operator+=(const hash_set &b)				{ return operator|=(b); }
	auto& operator-=(const hash_set &b)				{ return operator&=(~b); }

	friend auto operator&(const hash_set &a, const hash_set &b)			{ hash_set t(a); t &= b; return t; }
	friend auto operator&(const hash_set &a, const _not<hash_set> &b)	{ hash_set t(a); t &= b; return t; }
	friend auto operator|(const hash_set &a, const hash_set &b)			{ hash_set t(a); t &= b; return t; }
	friend auto operator^(const hash_set &a, const hash_set &b)			{ hash_set t(a); t ^= b; return t; }
	friend void swap(hash_set &a, hash_set &b)	{ raw_swap(a, b); }
};

template<typename K> struct _not<hash_set<K> > {
	const hash_set<K>	&t;
	_not(const hash_set<K> &t) : t(t) {}
	bool	count(const K &k) const		{ return !t.count(k); }
};

//-------------------------------------
//	hash_set_with_key
//-------------------------------------

template<typename K, bool MT = false, int HASH_BITS = 0> class hash_set_with_key : public hash_set0<hash_t<K>, K, MT, HASH_BITS> {
	typedef hash_set0<hash_t<K>, K, MT, HASH_BITS>	B;
public:
	hash_set_with_key(int init = 8)				: B(init)		{}
	hash_set_with_key(initializer_list<K> keys)	: B(keys.size()) { for (auto &i : keys) insert(i); }

	auto		find(const K &key) const				{ return B::find(hashk<K>(key)); }
	bool		count(const K &key) const				{ return !!B::check(hashk<K>(key)); }
	void		insert(const K &key)					{ B::put(hashk<K>(key), key); }
	void		insert(initializer_list<K> keys)		{ for (auto &i : keys) insert(i); }
	bool		remove(const K &key)					{ return !!B::remove(hashk<K>(key)); }
	bool		check_insert(const K &key)				{ auto h = hashk<K>(key); return B::check(h) || (B::put(h, key), false); }
	template<typename I> void insert(I a, I b)			{ while (a != b) insert(*a++); }
	template<typename I> void remove(I a, I b)			{ while (a != b) remove(*a++); }

	_not<hash_set_with_key>	operator~() const			{ return *this; }
	auto& operator&=(const hash_set_with_key &b)		{ B::intersect(b); return *this; }
	auto& operator&=(const _not<hash_set_with_key> &b)	{ B::difference(b.t); return *this; }
	auto& operator|=(const hash_set_with_key &b)		{ B::onion(b); return *this; }
	auto& operator^=(const hash_set_with_key &b)		{ B::exclusive(b); return *this; }
	auto& operator*=(const hash_set_with_key &b)		{ return operator&=(b); }
	auto& operator+=(const hash_set_with_key &b)		{ return operator|=(b); }
	auto& operator-=(const hash_set_with_key &b)		{ return operator&=(~b); }

	friend auto operator&(const hash_set_with_key &a, const hash_set_with_key &b)			{ hash_set_with_key t(a); t &= b; return t; }
	friend auto operator&(const hash_set_with_key &a, const _not<hash_set_with_key> &b)		{ hash_set_with_key t(a); t &= b; return t; }
	friend auto operator|(const hash_set_with_key &a, const hash_set_with_key &b)			{ hash_set_with_key t(a); t &= b; return t; }
	friend auto operator^(const hash_set_with_key &a, const hash_set_with_key &b)			{ hash_set_with_key t(a); t ^= b; return t; }
	friend void swap(hash_set_with_key &a, hash_set_with_key &b)	{ raw_swap(a, b); }
};

template<typename K> struct _not<hash_set_with_key<K> > {
	const hash_set_with_key<K>	&t;
	_not(const hash_set_with_key<K> &t) : t(t) {}
	bool	count(const K &k) const		{ return !t.count(k); }
};

//-----------------------------------------------------------------------------
//	multisets
//-----------------------------------------------------------------------------

template<typename H, typename T, bool MT = false, int HASH_BITS = 0> class hash_multiset0 : public hash_map0<H, T, MT, HASH_BITS> {
protected:
	typedef hash_map0<H, T, MT, HASH_BITS>		B;

	bool check_all(const hash_multiset0 &b) const	{
		for (auto i = B::begin(), e = B::end(); i != e; ++i) {
			T	*x = b.check(i.key());
			if (!x || *x < *i)
				return false;
		}
		return true;
	}
	void add(const hash_multiset0 &b) {
		for (auto i = b.begin(), e = b.end(); i != e; ++i)
			B::put(i.hash(), *i);
	}
	void sub(const hash_multiset0 &b) {
		for (auto i = B::begin(), e = B::end(); i != e; ++i) {
			H	k = i.hash();
			if (T *x = b.check(k)) {
				if (*i <= *x)
					B::safe_remove(i.get());
				else
					*i -= *x;
			}
		}
	}
	void min(const hash_multiset0 &b) {
		for (auto i = B::begin(), e = B::end(); i != e; ++i) {
			if (T *x = b.check(i.hash()))
				i->min(*x);
			else
				B::safe_remove(i.get());
		}
	}
	void max(const hash_multiset0 &b) {
		for (auto i = b.begin(), e = b.end(); i != e; ++i) {
			if (T *x = B::check(i.hash()))
				x->max(*i);
			else
				B::put(i.hash(), *i);
		}
	}
public:
	uint32		count(H key) const {
		T *x = B::check(key);
		return x ? *x : 0;
	}
	bool		remove(H key) {
		if (T *x = B::check(key)) {
			if (--*x == 0)
				B::remove(x);
			return true;
		}
		return false;
	}
	template<typename I> void insert(I a, I b)	{ while (a != b) insert(*a++); }
	template<typename I> void remove(I a, I b)	{ while (a != b) remove(*a++); }

	bool operator==(const hash_multiset0 &b)	const	{ return B::size() == b.size() && check_all(b); }
	bool operator< (const hash_multiset0 &b)	const	{ return B::size() <  b.size() && check_all(b); }
	bool operator<=(const hash_multiset0 &b)	const	{ return B::size() <= b.size() && check_all(b); }
	bool operator> (const hash_multiset0 &b)	const	{ return b <  *this; }
	bool operator>=(const hash_multiset0 &b)	const	{ return b <= *this; }
};

//-------------------------------------
//	hash_multiset
//-------------------------------------

struct count_entry {
	uint32	count	= 0;
	operator uint32() const			{ return count; }
	uint32	operator++()			{ return ++count; }
	uint32	operator--()			{ return --count; }
	uint32	operator++(int)			{ return count++; }
	uint32	operator--(int)			{ return count--; }
	void	operator+=(uint32 b)	{ count += b; }
	void	operator-=(uint32 b)	{ count -= b; }
	bool	operator<(uint32 b)		{ return count < b; }
	void	min(int32 b)			{ if (b < count) count = b; }
	void	max(int32 b)			{ if (b > count) count = b; }
};

template<typename K, bool MT = false, int HASH_BITS = 0> class hash_multiset : public hash_multiset0<hash_t<K>, count_entry, MT, HASH_BITS> {
	typedef	hash_t<K>										H;
	typedef hash_multiset0<H, count_entry, MT, HASH_BITS>	B;
public:
	uint32		insert(const K &key)			{ return B::put(hashk<K>(key))++; }
	uint32		count(const K &key) const		{ return B::count(hashk<K>(key)); }
	bool		remove(const K &key)			{ return B::remove(hashk<K>(key)); }

	auto& operator+=(const hash_multiset &b) { B::add(b); return *this; }
	auto& operator-=(const hash_multiset &b) { B::sub(b); return *this; }
	auto& operator*=(const hash_multiset &b) { B::min(b); return *this; }
	auto& operator|=(const hash_multiset &b) { B::max(b); return *this; }

	friend auto operator+(const hash_multiset &a, const hash_multiset &b)	{ auto t = a; t += b; return t; }
	friend auto operator-(const hash_multiset &a, const hash_multiset &b)	{ auto t = a; t -= b; return t; }
	friend auto operator*(const hash_multiset &a, const hash_multiset &b)	{ auto t = a; t *= b; return t; }
};

//-------------------------------------
//	hash_multiset_with_key
//-------------------------------------

template<typename K> struct count_with_key_entry : count_entry {
	K		k;
	count_with_key_entry(const K &k) : k(k) {}
};

template<typename K, bool MT = false, int HASH_BITS = 0> class hash_multiset_with_key : public hash_multiset0<hash_t<K>, count_with_key_entry<K>, MT, HASH_BITS> {
	typedef	hash_t<K>													H;
	typedef hash_multiset0<H, count_with_key_entry<K>, MT, HASH_BITS>	B;
public:
	struct iterator : B::iterator {
		iterator(const typename B::iterator &i) : B::iterator(i)	{}
		iterator&	operator++()			{ this->next(); return *this; }
		K*			get()			const	{ return &B::iterator::get()->k; }
		K&			operator*()		const	{ return *get(); }
		K*			operator->()	const	{ return get(); }
		uint32		count()			const	{ return B::iterator::get()->count; }
	};
	iterator	begin()	const	{ return B::begin(); }
	iterator	end()	const	{ return B::end(); }

	uint32		insert(const K &key)			{ return B::put(hashk<K>(key))++; }
	uint32		count(const K &key) const		{ return B::count(hashk<K>(key)); }
	bool		remove(const K &key)			{ return B::remove(hashk<K>(key)); }

	auto& operator+=(const hash_multiset_with_key &b) { B::add(b); return *this; }
	auto& operator-=(const hash_multiset_with_key &b) { B::sub(b); return *this; }
	auto& operator*=(const hash_multiset_with_key &b) { B::min(b); return *this; }
	auto& operator|=(const hash_multiset_with_key &b) { B::max(b); return *this; }

	friend auto operator+(const hash_multiset_with_key &a, const hash_multiset_with_key &b)	{ auto t = a; t += b; return t; }
	friend auto operator-(const hash_multiset_with_key &a, const hash_multiset_with_key &b)	{ auto t = a; t -= b; return t; }
	friend auto operator*(const hash_multiset_with_key &a, const hash_multiset_with_key &b)	{ auto t = a; t *= b; return t; }
};

//-----------------------------------------------------------------------------
//	small_set - uses array of N before switching to hash_map
//-----------------------------------------------------------------------------

template<typename T> class small_set_base {
	hash_set<T>	hash;
	int			num_small, max_small;

	bool		small_full()			{ return num_small == max_small; }
	const T*	small_begin()	const	{ return (const T*)(this + 1); }
	T*			small_begin()			{ return (T*)(this + 1); }

	const T*	in_small(const T&t) const {
		for (auto &i : make_range_n(small_begin(), num_small)) {
			if (i == t)
				return &i;
		}
		return 0;
	}

public:
	small_set_base(int n) : num_small(0), max_small(n) {}

	void		clear() {
		num_small = 0;
		hash.clear();
	}

	bool		count(const T&t) const	{
		return hash.empty() ? in_small(t) : hash.count(t);
	}
	void		insert(const T&t) {
		if (hash.empty()) {
			if (in_small(t))
				return;
			if (!small_full()) {
				new(small_begin() + num_small++) T(t);
				return;
			}
			hash.insert(small_begin(), small_begin() + num_small);
		}
		hash.insert(t);
	}
	bool		remove(const T&t) {
		if (hash.empty()) {
			if (auto *i = in_small(t)) {
				auto	*back = small_begin()[--num_small];
				*i = move(*back);
				back->~T();
				return true;
			}
			return false;
		}
		return hash.remove(t);
	}
	bool		check_insert(const T&t)	{
		if (hash.empty()) {
			if (in_small(t))
				return true;
			if (!small_full()) {
				new(small_begin() + num_small++) T(t);
				return false;
			}
			hash.insert(small_begin(), small_begin() + num_small);
		}
		return hash.check_insert(t);
	}

	//dodgy!
	typedef	T*	iterator;
	iterator	begin() { return small_begin(); }
	iterator	end()	{ return small_begin() + num_small; }
};

template<typename T, int N = 8> struct small_set : public small_set_base<T> {
	space_for<T[N]>	space;

	small_set()	: small_set_base<T>(N) {}
#ifdef USE_RVALUE_REFS
	//	small_set(const small_set &b)	= default;
	//	small_set(small_set &&b)		= default;
#endif
};


//-----------------------------------------------------------------------------
//	static_hash
//-----------------------------------------------------------------------------

template<typename T, typename K> struct static_hash : singleton<hash_map<K, T*>> {
	typedef static_hash	base;
	using singleton<hash_map<K, T*>>::single;
	template<typename K2> static_hash(K2 &&k)	{ single().put(k, static_cast<T*>(this)); }
	static auto		begin()						{ return single().begin(); }
	static auto		end()						{ return single().end(); }
	template<typename K2> static T*	get(K2&& k)	{ return make_const(single()).get(forward<K2>(k)).or_default(); }
};

} //namespace iso


#endif // HASH_H
