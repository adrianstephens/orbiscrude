#include "base/defs.h"
#include "base/atomic.h"
#include "base/functions.h"

namespace iso {

template<typename K> struct atomic_hash_key {
	K		key;

	static K tombstone(int i)	{ return i + 2; }
	static K unset(int i)		{ return i + 1; }

	atomic_hash_key()					{}
	atomic_hash_key(K key)	: key(key)	{}
	operator const K()			const	{ return key; }
	bool	is_unset(int i)		const	{ return key == i + 1; }
	bool	is_tombstone(int i)	const	{ return key == i + 2; }
};

template<typename V> struct atomic_hash_val {
	enum {
		PRIME			= 1,
		TOMBSTONE		= 2,
		SET_VAL			= 4,
	};
	V		val;
	uint32	flags;

	static atomic_hash_val tombstone(const atomic_hash_val &v)	{ return atomic_hash_val(v.val, v.flags | TOMBSTONE); }
	static atomic_hash_val prime(const atomic_hash_val &v)		{ return atomic_hash_val(v.val, v.flags | PRIME); }
	static atomic_hash_val tombstone_prime()					{ return atomic_hash_val(V(), TOMBSTONE | PRIME); }

	atomic_hash_val() : val(), flags(TOMBSTONE) {}
	atomic_hash_val(const V &val, uint32 flags = SET_VAL) : val(val), flags(flags) {}
	operator const V()		const	{ return val; }
	bool	is_prime()		const	{ return flags & PRIME; }
	bool	is_tombstone()	const	{ return flags & TOMBSTONE; }
};

 struct atomic_hash_ptrval {
	enum {
		PRIME			= 1,
		TOMBSTONE		= 2,
	};
	intptr_t			val;

	static intptr_t tombstone(intptr_t v)	{ return v | TOMBSTONE; }
	static intptr_t prime(intptr_t v)		{ return v | PRIME; }
	static intptr_t tombstone_prime()		{ return TOMBSTONE | PRIME; }

	atomic_hash_ptrval()			: val(TOMBSTONE) {}
	atomic_hash_ptrval(intptr_t v)	: val(v) {}
	bool	is_prime()		const	{ return val & PRIME; }
	bool	is_tombstone()	const	{ return val & TOMBSTONE; }
};

template<typename V> struct atomic_hash_val<V*> : atomic_hash_ptrval {
	atomic_hash_val() {}
	atomic_hash_val(const V* _val)	: atomic_hash_ptrval(intptr_t(_val)) {}
	atomic_hash_val(intptr_t v)		: atomic_hash_ptrval(v) {}
	operator intptr_t()		const	{ return val; }
	operator V*()			const	{ return (V*)(val & ~3); }
};

template<typename K, typename V> class atomic_hash_map {
	enum { MIN_SIZE_LOG = 3 };
	enum RESULT {
		RETRY,
		FAIL,
		SUCCESS,
		SUCCESS_INC,
		SUCCESS_DEC,
	};

	typedef	atomic_hash_key<K>	AK;
	typedef	atomic_hash_val<V>	AV;

	// --- CHM -----------------------------------------------------------------
	// The control structure for the atomic_hash_map

	struct CHM : atomic<refs<CHM>> {
		enum { REPROBE_LIMIT = 10 };

		const uint32	len;		// size of array
		atomic<uint32>	slots;		// total used slots, to tell when table is full of dead unusable slots
		atomic<CHM*>	next;		// new mappings, used during resizing.
		atomic<uint32>	copy_idx;	// next part of the table to copy
		atomic<uint32>	copy_done;	// work-done reporting

		void*	operator new(size_t, uint32 n)	{ return malloc(sizeof(CHM) + n * (sizeof(AK) + sizeof(AV))); }

		CHM(uint32 len) : len(len), slots(0), next(0), copy_idx(0), copy_done(0) {
			copy_new_n(make_int_iterator(AK::unset(0)), keys(), len);
			fill_new_n(vals(), len);
		}
		~CHM() {
			if (CHM *p = next)
				p->release();
		}
		atomic<AK>* keys() {
			return (atomic<AK>*)(this + 1);
		}
		atomic<AV>* vals() {
			return (atomic<AV>*)(keys() + len);
		}
		constexpr int	reprobe_limit() const {
			return REPROBE_LIMIT + (len >> 2);
		}
		constexpr bool	table_full(int reprobes) const {
			return reprobes >= REPROBE_LIMIT && slots >= reprobe_limit();
		}

		// callers MUST help_copy lest we have a path which forever runs through 'resize' only to discover a copy-in-progress which never progresses
		CHM	*resize(uint32 size) {
			// check for resize already in progress, probably triggered by another thread
			if (CHM *next1 = next)
				return next1;

			// otherwise (try to) start one
			uint32	newlen	= size >= len >> 2 ? (size >= (len >> 1) ? (len << 2) : (len << 1)) : len;
			CHM		*next1	= new(newlen) CHM(newlen);

			next1->addref();

			if (next.cas(0, next1))
				return next1;

			next1->release();
			return next;
		}
	};

	struct _snapshot {
		atomic_hash_map*	map;
		ref_ptr<CHM>		chm;

		struct iterator_base {
			const _snapshot	&s;
			pair<K, V>		entry;
			uint32			avflags;
			int				i;

			iterator_base(const _snapshot &s, bool end) : s(s), i(end ? s.chm->len : next_valid(0)) {}

			int		get(int i) {
				AK	ak = s.chm->keys()[i];
				AV	av = s.chm->vals()[i];
				if (!ak.is_unset(i) && !ak.is_tombstone(i) && !av.is_tombstone()) {
					entry.a	= ak;
					entry.b	= av;
					avflags	= av.flags;
					return true;
				}
				return false;
			}
			int		next_valid(int i) {
				while (i < s.chm->len && !get(i))
					++i;
				return i;
			}
			int		prev_valid(int i) {
				while (i >= 0 && !get(i))
					--i;
				return i;
			}
			void	next() { i = next_valid(i + 1); }
			void	prev() { i = prev_valid(i + 1); }

			void	remove() {
				s.map->put_impl(s.chm, next.key, true, true, [oldav = AV(entry.b, avflags)](atomic<AV> &av) {
					return av.cas(oldav, AV::tombstone(oldav)) ? SUCCESS_DEC : RETRY;
				});
			}
			bool	operator==(const iterator_base &b) const	{ return i == b.i; }
			bool	operator!=(const iterator_base &b) const	{ return i != b.i; }
		};

		struct iterator : iterator_base {
			typedef iterator_base	B;
			iterator(const _snapshot &s, bool end) : B(s, end) {}
			V			operator*()			const	{ return B::entry.b; }
			V			operator->()		const	{ return B::entry.b; }
			iterator&	operator++()				{ B::next(); return *this; }
			iterator&	operator--()				{ B::prev(); return *this; }
		};

		struct _with_keys {
			const _snapshot	&s;

			struct iterator : iterator_base {
				typedef iterator_base	B;
				iterator(const _snapshot &s, bool end) : B(s, end) {}
				auto&		operator*()		const	{ return B::entry; }
				iterator&	operator++()			{ B::next(); return *this; }
				iterator&	operator--()			{ B::prev(); return *this; }
			};

			_with_keys(const _snapshot &s) : s(s) {}
			iterator	begin() { return iterator(s, false); }
			iterator	end()	{ return iterator(s, true); }
		};

		_snapshot(atomic_hash_map *map) : map(map) {
			for (;;) {
				ref_ptr<CHM> top = map->top;
				if (CHM *next = top->next) {
					// copy in-progress; must help finish the table copy
					map->help_copy(top, next, true);
				} else {
					chm = top;
					break;
				}
			}
		}
		~_snapshot() {}
		_with_keys	with_keys()	const { return *this; }
		iterator	begin()		const { return iterator(*this, false); }
		iterator	end()		const { return iterator(*this, true); }
	};

	atomic<ref_ptr<CHM> >	top;
	atomic<uint32>			total;

	bool	copy_slot(CHM *next, atomic<AK> &rk, atomic<AV> &rv) {
		// prevent new values from appearing in the old table by setting PRIME
		AV	v = rv;
		while (!v.is_prime()) {
			if (rv.cas(v, AV::prime(v))) {
				if (v.is_tombstone())
					return true;	// don't copy dead keys
				break;
			}
			v	= rv;
		}

		if (v.is_tombstone())
			return false;			// copy already complete

		// copy the value into the new table, but only if we overwrite a null
		bool copied = put_impl(next, rk.get(), false, false, [&v](atomic<AV> &av) {
			return av.cas(AV(), (V)v) ? SUCCESS : FAIL;
		});

		// hide the old-table value by slapping a TOMBPRIME down; this will stop other threads from uselessly attempting to copy this slot (speed optimization not a correctness issue)
		rv = AV::tombstone_prime();
		return copied;
	}

	void	copy_slot_promote(CHM *chm, CHM *next, int idx) {
		if (copy_slot(next, chm->keys()[idx], chm->vals()[idx])) {
			if (++chm->copy_done == chm->len)
				top.cas(chm, next);	// attempt to promote
		}
	}

	// Help along an existing resize operation
	bool	help_copy(CHM *chm, CHM *next, bool copy_all) {
		int		len			= chm->len;
		int		min_copies	= min(len, 1024);
		bool	panic		= false;		// panic if we have tried TWICE to copy every slot and it still has not happened
		int		idx			= 0;
		auto	keys		= chm->keys();
		auto	vals		= chm->vals();

		while (chm->copy_done < len) {
			if (panic) {
				idx		+= min_copies;
			} else {
				idx		= chm->copy_idx.post() += min_copies;
				panic	= idx >= (len << 1);
			}

			for (int i = 0; i < min_copies; i++) {
				int	j	= (idx + i) & (len - 1);
				atomic<AK>	&ak	= keys[j];
				// blindly set the key slot from unset to tombstone to eagerly stop fresh put's from inserting new values in the old table when it is mid-resize
				ak.cas(AK::unset(j), AK::tombstone(j));
				if (copy_slot(next, ak, vals[j]))
					++chm->copy_done;
			}

			if (!copy_all && !panic)
				return chm->copy_done == len;
		}
		return true;
	}

	void	help_copy() {
		ref_ptr<CHM> chm	= top;
		if (CHM *next = chm->next) {
			if (help_copy(chm, next, false))
				top.cas(chm, next);	// attempt to promote
		}
	}

	bool	get_impl(CHM *chm, K key, V &val) {
		for (;;) {
			int		reprobes	= 0;
			int		mask		= chm->len - 1;
			int		idx;

			for (idx = key & mask; ; idx = (idx + 1) & mask) {
				AK	k = chm->keys()[idx];
				if (k.is_unset(idx))
					return false;				// a clear miss

				if (k.key == key) {
					AV	v	= chm->vals()[idx];
					if (!v.is_prime()) {
						val = v;
						return !v.is_tombstone();
					}
					// slot is (possibly partially) copied to the new table; finish the copy & retry in the new table
					CHM	*next	= chm->next;
					copy_slot_promote(chm, next, idx);
					chm = next;
					break;
				}

				// get and put must have the same key lookup logic but only 'put' needs to force a table-resize for a too-long key-reprobe sequence
				if (++reprobes >= chm->reprobe_limit() || k.is_tombstone(idx)) {
					if (chm = chm->next)
						break;
					return false;
				}
			}

			help_copy();
			//loop for next table
		}
	}

	bool	put_impl(CHM *chm, K key, bool should_help, bool removing, callback_ref<RESULT(atomic<AV>&)> update) {
		for (;;) {
			CHM		*next		= chm->next;
			int		reprobes	= 0;
			bool	found		= false;
			int		mask		= chm->len - 1;
			int		idx;

			for (idx = key & mask; ; idx = (idx + 1) & mask) {
				auto	&ak = chm->keys()[idx];
				AK		k	= ak;

				if (k.is_unset(idx)) {				// if slot free?
					if (removing)
						return false;				// not-now & never-been in this table

					// claim the null key-slot
					if (ak.cas(AK::unset(idx), key)) {
						++chm->slots;
						if (!next && chm->table_full(reprobes))
							next = chm->resize(total);	// force the new table copy to start
						break;						// got it
					}
					k = ak;							// CAS failed, get updated value
				}

				if (found = k.key == key)
					break;							// got it

				// get and put must have the same key lookup logic lest 'get' give up looking too soon
				if (++reprobes >= chm->reprobe_limit() || k.is_tombstone(idx)) {
					next = chm->resize(total);
					break;
				}
			}

			if (!next) {
				auto	&av	= chm->vals()[idx];
				for (;;) {
					switch (update(av)) {
						case FAIL:			return false;
						case SUCCESS:		return true;
						case SUCCESS_INC:	++total; return true;
						case SUCCESS_DEC:	--total; return true;
						default:			break;
					}
					// if a prime'd value got installed, we need to re-run the put on the new table
					if (av.get().is_prime()) {
						next = chm->next;
						break;
					}
				}
			}

			if (found)
				copy_slot_promote(chm, next, idx);

			if (should_help)
				help_copy();

			//loop for next table
			chm = next;
		}
	}

public:
	atomic_hash_map(int n = 1 << MIN_SIZE_LOG) {
		n	= 1 << log2_ceil(n);
		top = new(n) CHM(n);
	}

	int     size()	const		{ return total; }
	bool	empty()	const		{ return total == 0; }

	void	clear() {
		CHM	*chm	= new(1 << MIN_SIZE_LOG) CHM(1 << MIN_SIZE_LOG);
		ref_ptr<CHM> old;
		do
			old = top;
		while (!top.cas(old, chm));
		total = 0;
	}

	// map the specified key to the specified value in the table
	void	put(K key, V val = V()) {
		put_impl(top.get(), key, true, false, [val](atomic<AV> &av) {
			AV	v = av;
			return v.is_prime() || !av.cas(v, val) ? RETRY : v.is_tombstone() ? SUCCESS_INC : SUCCESS;
		});
	}

	// put if-and-only-if the key is not mapped
	bool	put_absent(K  key, V val) {
		return put_impl(top.get(), key, true, false, [val](atomic<AV> &av) {
			AV	v = av;
			if (!v.is_tombstone())
				return FAIL;
			return !v.is_prime() && av.cas(v, val) ? SUCCESS_INC : RETRY;
		});
	}

	// remove the key from this map
	bool	remove(K key) {
		return put_impl(top.get(), key, true, true, [](atomic<AV> &av) {
			AV	v = av;
			if (v.is_tombstone())
				return FAIL;
			return !v.is_prime() && av.cas(v, AV::tombstone(v)) ? SUCCESS_DEC : RETRY;
		});
	}

	// remove if-and-only-if the key is mapped to a value which is equal to the given value
	bool	remove(K key, V val) {
		return put_impl(top.get(), key, true, true, [val](atomic<AV> &av) {
			return av.cas(val, AV::tombstone(val)) ? SUCCESS_DEC : RETRY;
		});
	}

	// put(key,val) if-and-only-if the key is mapped
	bool	replace(K key, V val) {
		return put_impl(top.get(), key, true, false, [val](atomic<AV> &av) {
			AV	v = av;
			if (v.is_tombstone())
				return FAIL;
			return !v.is_prime() && av.cas(v, val) ? SUCCESS : RETRY;
		});
	}

	// put(key,val) if-and-only-if the key is mapped to old
	bool	replace(K key, V old, V val) {
		return put_impl(top.get(), key, true, false, [old, val](atomic<AV> &av) {
			return av.cas(old, val) ? SUCCESS : FAIL;
		});
	}

	// return the value to which the specified key is mapped
	optional<V>	get(K key) {
		V	val;
		if (get_impl(top.get(), key, val))
			return val;
		return none;
	}

	_snapshot	snapshot()		{ return this; }
	auto		with_keys()		{ return snapshot().with_keys(); }

	optional<V>									operator[](const K &k)	const	{ return get(k); }
	putter<atomic_hash_map, K, optional<V>>		operator[](K k)					{ return {*this, k}; }
};


} // namespace iso