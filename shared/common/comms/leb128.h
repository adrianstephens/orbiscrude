#ifndef LEB128_H
#define LEB128_H

#include "base/defs.h"

namespace iso {
template<typename T, bool sign = num_traits<T>::is_signed> struct leb128;

template<typename T> struct leb128<T, false> {
	typedef uint_for_t<T>	S;
	struct raw {
		operator T() const {
			const uint8	*p	= (const uint8*)this;
			S		t	= 0;
			for (int s = 0;; s += 7) {
				int	b = *p++;
				t |= (b & 0x7f) << s;
				if (!(b & 0x80))
					return T(t);
			}
		}
		friend void *get_after(const raw *t) {
			const uint8	*p	= (const uint8*)t;
			while (*p++ & 0x80);
			return (void*)p;
		}
	};
	T	t;

	leb128() {}
	leb128(const T &t) : t(t) {}
	operator T() const { return T(t); }

	template<class W> bool write(W &w) const {
		S	t2 = t;
		while (t2 > 127) {
			w.putc((t2 & 0x7f) | 0x80);
			t2 >>= 7;
		}
		w.putc(t2);
		return true;
	}
	template<class R> bool read(R &r) {
		S	x = 0;
		for (int s = 0;; s += 7) {
			int	b = r.getc();
			if (b == -1)
				return false;
			x |= S(b & 0x7f) << s;
			if (!(b & 0x80)) {
				t = (T&)x;
				return true;
			}
		}
	}
	bool read(const uint8 *&r) {
		S	x = 0;
		for (int s = 0;; s += 7) {
			int	b = *r++;
			if (b == -1)
				return false;
			x |= S(b & 0x7f) << s;
			if (!(b & 0x80)) {
				t = T(x);
				return true;
			}
		}
	}
};

template<typename T> struct leb128<T,true> {
	typedef uint_for_t<T>	S;
	struct raw {
		operator T() const {
			const uint8	*p	= (const uint8*)this;
			S		t	= 0;
			for (int s = 0;; s += 7) {
				int	b = *p++;
				t |= (b & 0x7f) << s;
				if (!(b & 0x80))
					return T(t - (t & (S(64) << s)) << 1);
			}
		}
		friend void *get_after(const raw *t) {
			const uint8	*p	= (const uint8*)t;
			while (*p++ & 0x80);
			return (void*)p;
		}
	};
	S	t;

	leb128() {}
	leb128(const T &_t) : t(_t) {}
	operator T() const { return T(t); }

	template<class W> bool write(W &w) const {
		T	t2 = t;
		while (t2 > 127 || t2 < -127) {
			w.putc((t2 & 0x7f) | 0x80);
			t2 /= 128;
		}
		w.putc(t2 & 0x7f);
		return true;
	}
	template<class R> bool read(R &r) {
		S	x = 0;
		for (int s = 0;; s += 7) {
			int	b = r.getc();
			if (b == -1)
				return false;
			x |= S(b & 0x7f) << s;
			if (!(b & 0x80)) {
				t = x - ((x & (S(64) << s)) << 1);
				return true;
			}
		}
	}
};

template<typename T> T get(const leb128<T> &t)	{ return t; }
template<typename T> leb128<T> make_leb128(T t)	{ return t; }

template<typename T, class W> bool write_leb128(W &w, const T &t) {
	return leb128<T>(t).write(w);
}

template<typename T, class R> bool read_leb128(R &r, T &t) {
	leb128<T>	x;
	if (!x.read(r))
		return false;
	t = x;
	return true;
}

template<typename T, class R> T get_leb128(R &r) {
	leb128<T>	x;
	if (!x.read(r))
		return 0;
	return x;
//	return r.template get<leb128<T> >();
}

template<class R> struct leb128_getter {
	R	&r;
	leb128_getter(R &r) : r(r) {}
	template<typename T> operator T() { return get_leb128<T>(r); }
};

template<class R> auto get_leb128(R &r) {
	return leb128_getter<R>(r);
}

} // namespace iso
#endif //LEB128_H
