#include "bignum.h"
#include "base/bits.h"
#include "base/maths.h"
#include "extra/random.h"
#include "utilities.h"

namespace iso {

template<typename T> T* copy_n(const T *a, T *r, int n) {
	while (n--)
		*r++ = *a++;
	return r;
}
template<typename T> void copy_rest(const T *a, T *r0, T *r1, int n) {
	int		n0 = int(r1 - r0);
	if (n > n0)
		copy_n(a + n0, r1, n - n0);
}

template<typename T> T* addn(const T *a, T b, T *r) {
	bool	c = addc(*a, b, false, *r++);
	while (c) {
		T	t = *++a + 1;
		c = t == 0;
		*r++ = t;
	}
	return r;
}

// bn <= an
template<typename T> T* addn(const T *a, const T *b, T *r, int an, int bn) {
	bool	c	= false;
	const T *ae = a + an;
	const T *be = b + bn;

	while (b != be)
		c = addc(*a++, *b++, c, *r++);

	while (c && a != ae) {
		T	t = *a++ + 1;
		c = t == 0;
		*r++ = t;
	}
	while (a != ae)
		*r++ = *a++;
	*r++ = int(c);
	return r;
}
template<typename T> T* addn_to(const T *a, T *r, int n) {
	bool	c = false;

	while (n--) {
		c = addc(*r, *a++, c, *r);
		++r;
	}

	while (c)
		c = ++(*r++) == 0;
	return r;
}

template<typename T> T* subn(const T *a, T b, T *r) {
	bool	c = subc(*a, b, false, *r++);
	while (c) {
		T	t = *++a;
		c = t == 0;
		*r++ = t - 1;
	}
	return r;
}
template<typename T> T* subn(const T *a, const T *b, T *r, int an, int bn) {
	bool	c	= false;
	const T *ae = a + an;
	const T *be = b + bn;

	while (b != be)
		c = subc(*a++, *b++, c, *r++);

	while (c) {
		T	t = *a++;
		c = t == 0;
		*r++ = t - 1;
	}
	while (a != ae)
		*r++ = *a++;
	return r;
}
template<typename T> T* subn_from(const T *a, T *r, int n) {
	bool	c = false;
	while (n--) {
		c = subc(*r, *a++, c, *r);
		++r;
	}
	while (c)
		c = (*r++)-- == 0;
	return r;
}

template<typename T> T *muln(const T *a, T b, T *r, int n) {
	T	c = 0;
	while (n--)
		c = maddc(*a++, b, c, *r++);
	if (c)
		*r++ = c;
	return r;
}

template<typename T> T *maddn(const T *a, T b, T *r, int n) {
	T	c = 0;
	T	lo;
	while (n--) {
		c = maddc(*a++, b, c, lo);
		c += int(addc(lo, *r, false, *r));
		++r;
	}
	while (c) {
		T	x = *r + c;
		c = int(x < c);
		*r++ = x;
	};
	return r;
}

//-----------------------------------------------------------------------------
//	LOAD/SAVE
//-----------------------------------------------------------------------------

// Import from unsigned binary data
mpi &mpi::load(const uint8 *buffer, uint32 len, bool bigendian) {
	if (bigendian) {
		while (len && *buffer == 0) {
			buffer++;
			len--;
		}
		element	*pp = unique(nbytes(len));
		memset(pp, 0, size * bytes_per_element);
		sign = 0;
		for (int j = len; j--; )
			pp[j / bytes_per_element] |= element(*buffer++) << ((j % bytes_per_element) << 3);
	} else {
		while (len && buffer[len - 1] == 0)
			len--;
		element	*pp = unique(nbytes(len));
		memset(pp, 0, size * bytes_per_element);
		sign = 0;
		for (int j = 0; len--; j++)
			pp[j / bytes_per_element] |= element(*buffer++) << ((j % bytes_per_element) << 3);
	}
	return *this;
}

// Export into unsigned binary data
bool mpi::save(uint8 *buffer, uint32 len, bool bigendian) const {
	uint32	n = num_bytes();
	if (len < n)
		return false;

	memset(buffer, 0, len);
	if (bigendian) {
		for (int i = len - 1, j = 0; n > 0; i--, j++, n--)
			buffer[i] = uint8(p[j / bytes_per_element] >> ((j % bytes_per_element) << 3));
	} else {
		for (int i = 0; n > 0; i++, n--)
			buffer[i] = uint8(p[i / bytes_per_element] >> ((i % bytes_per_element) << 3));
	}
	return true;
}

// Export into unsigned binary data
uint32 mpi::save_all(uint8 *buffer, bool bigendian) const {
	uint32	len	= num_bytes();
	save(buffer, len, bigendian);
	return len;
}

//-----------------------------------------------------------------------------
//	ASSIGN
//-----------------------------------------------------------------------------
mpi::element *mpi::unique() {
	if (p.shared()) {
		element	*oldp = p;
		p.create(max_size = size);
		element	*newp = p;
		memcpy(newp, oldp, size * sizeof(element));
		return newp;
	}
	return p;
}

mpi::element *mpi::unique(int n) {
	if (p.shared()) {
		element	*oldp = p;
		p.create(n);
		element	*newp = p;
		if (n > size) {
			memcpy(newp, oldp, size * sizeof(element));
			memset(newp + size, 0, (n - size) * sizeof(element));
		} else {
			memcpy(newp, oldp, n * sizeof(element));
		}
		size = max_size = n;
		return newp;

	} else if (n > max_size) {
		p.grow(n);
		element	*newp = p;
		if (n > size)
			memset(newp + size, 0, (n - size) * sizeof(element));
		size = max_size = n;
		return newp;

	} else {
		size = n;
		return p;
	}
}

mpi &mpi::operator=(element b) {
	element	*ap = unique(1);
	sign = 0;
	ap[0] = b;
	return *this;
}

//-----------------------------------------------------------------------------
//	SHIFT
//-----------------------------------------------------------------------------

mpi& mpi::operator<<=(int count) {
	if (count) {
		element *ap = unique(nbits(highest_set_index(*this) + count + 1));

		int	v0 = count / bits_per_element;
		if (v0) {
			int	i = size;
			while (i-- > v0)
				ap[i] = ap[i - v0];
			while (i--)
				ap[i] = 0;
		}

		if (int v1 = count & (bits_per_element - 1)) {
			element	r0 = 0, r1;
			for (int i = v0; i < size; i++, r0 = r1) {
				r1 = ap[i] >> (bits_per_element - v1);
				ap[i] = (ap[i] << v1) | r0;
			}
		}
	}
	return *this;
}

mpi operator<<(param(mpi) a, int count) {
	uint32				b = highest_set_index(a);
	uint32				i = count / mpi::bits_per_element;
	uint32				rn = mpi::nbits(b + count + 1);
	uint32				an = mpi::nbits(b + 1);

	mpi					r(rn, !!a.sign);
	const mpi::element	*ap = a.p;
	mpi::element		*rp = r.p;

	memset(rp, 0, i * mpi::bytes_per_element);
	rp += i;

	if (int shift = count & (mpi::bits_per_element - 1)) {
		int	shiftr = mpi::bits_per_element - shift;
		mpi::element	r0 = 0;
		for (const mpi::element *ae = ap + an; ap < ae; ap++) {
			*rp++ = (*ap << shift) | r0;
			r0 = *ap >> shiftr;
		}
		if (rp < r.p + rn)
			*rp++ = r0;
	} else {
		memcpy(rp, ap, an * mpi::bytes_per_element);
	}
	return r;
}

mpi& mpi::operator>>=(int count) {
	if (count) {
		element *ap = unique();
		if (int v0 = count / bits_per_element) {
			int	i = 0;
			while (i < size - v0) {
				ap[i] = ap[i + v0];
				i++;
			}
			while (i < size)
				ap[i++] = 0;
		}

		if (int v1 = count & (bits_per_element - 1)) {
			element	r0 = 0, r1;
			for (int i = size; i--; r0 = r1) {
				r1 = ap[i] << (bits_per_element - v1);
				ap[i] = (ap[i] >> v1) | r0;
			}
		}
		fix_size();
	}
	return *this;
}

mpi operator>>(param(mpi) a, int count) {
	int		b = highest_set_index(a) - count + 1;
	if (b <= 0)
		return zero;

	uint32				rn = mpi::nbits(b);
	mpi					r(rn, !!a.sign);
	mpi::element		*rp = r.p;
	const mpi::element	*ap = a.p + count / mpi::bits_per_element;

	if (int shift = count & (mpi::bits_per_element - 1)) {
		int	shiftl = mpi::bits_per_element - shift;
		mpi::element	r0 = count / mpi::bits_per_element + rn < a.size ? ap[rn] << shiftl : 0;
		for (int i = rn; i--;) {
			rp[i] = (ap[i] >> shift) | r0;
			r0 = ap[i] << shiftl;
		}
	} else {
		memcpy(rp, ap, rn * mpi::bytes_per_element);
	}
	return r;
}

//-----------------------------------------------------------------------------
//	ADD/SUB
//-----------------------------------------------------------------------------

template<typename T> uint32 add_size(const T *a, uint32 an, const T *b, uint32 bn) {
	static const T	maxt = T(~T(0));
	if (!bn)
		return an;

	if (!an)
		return bn;

	T	at = a[an - 1];
	T	bt = b[bn - 1];

	if (bn > an)
		return bn + int(bt == maxt && b[an - 1] >= ~at);

	if (bn < an)
		return an + int(at == maxt && a[bn - 1] >= ~bt);

	return an + int(at >= ~bt);
}

template<typename T> uint32 add_size(const T *a, uint32 an, T b) {
	static const T	maxt = T(~T(0));
	T	at = a[an - 1];
	return an + int(at == maxt && a[0] >= ~b);
}

mpi abs(param(mpi) a) {
	mpi	r = a;
	r.sign = false;
	return r;
}

mpi operator-(param(mpi) a) {
	mpi	r = a;
	r.sign = !a.sign;
	return r;
}

mpi &mpi::operator+=(element b) {
	if (uint32 an = size) {
		if (sign) {
			element *ap = unique();
			if (an == 1 && ap[0] < b) {
				ap[0]	= b - ap[0];
				sign	= 0;
			} else {
				subn(ap, b, ap);
			}
		} else {
			element *ap = unique(add_size(get(p), an, b));
			addn(ap, b, ap);
		}
	} else {
		p.create(size = 1);
		*p = b;
	}
	return *this;
}

mpi &mpi::operator+=(param(mpi) b) {
	uint32			an = size;
	uint32			bn = b.size;
	const element	*bp = b.p;

	if (sign != b.sign) {
		element			*ap = unique(max(an, bn));
		if (cmpn(ap, an, bp, bn) >= 0) {
			subn_from(bp, ap, bn);
		} else {
			subn(bp, ap, ap, bn, an);
			sign = !sign;
		}
	} else {
		element *ap = unique(max(an, bn) + 1);//add_size(get(p), an, bp, bn));
		addn_to(bp, ap, bn);
	}
	fix_size();
	return *this;
}

mpi operator+(param(mpi) a, mpi::element b) {
	const mpi::element	*ap = a.p;
	if (uint32 an = a.size) {
		if (a.sign) {
			mpi				r(an, true);
			mpi::element	*rp = r.p;
			if (an == 1 && ap[0] < b) {
				rp[0]	= b - ap[0];
				r.sign	= false;
			} else {
				copy_rest(ap, rp, subn(ap, b, rp), an);
			}
			return r;
		} else {
			mpi				r(add_size(ap, an, b), false);
			mpi::element	*rp = r.p;
			copy_rest(ap, rp, addn(ap, b, rp), an);
			return r;
		}
	} else {
		return mpi(b);
	}
}

mpi operator+(param(mpi) a, param(mpi) b) {
	bool		sign	= !!a.sign;
	uint32		an		= a.size;
	uint32		bn		= b.size;
	const mpi::element *ap = a.p, *bp = b.p;

	if (a.sign != b.sign) {
		if (cmpn(bp, bn, ap, an) > 0) {
			swap(an, bn);
			swap(ap, bp);
			sign = !sign;
		}
		mpi				r(an, sign);
		mpi::element	*rp = r.p;
		subn(ap, bp, rp, an, bn);
		r.fix_size();
		return r;

	} else {
		if (bn > an) {
			swap(an, bn);
			swap(ap, bp);
		}
		uint32			rn = an + 1;//add_size(ap, an, bp, bn);
		mpi				r(rn, sign);
		mpi::element	*rp = r.p;
		addn(ap, bp, rp, an, bn);
		r.fix_size();
		return r;
	}
}

mpi &mpi::operator-=(element b) {
	if (uint32 an = size) {
		if (!sign) {
			element *ap = unique();
			if (an == 1 && ap[0] < b) {
				ap[0]	= b - ap[0];
				sign	= 1;
			} else {
				subn(ap, b, ap);
			}
		} else {
			element *ap = unique(add_size(get(p), an, b));
			addn(ap, b, ap);
		}
	} else {
		p.create(size = 1);
		*p = b;
		sign = true;
	}
	return *this;
}

mpi &mpi::operator-=(param(mpi) b) {
	uint32			an = size;
	uint32			bn = b.size;
	const element	*bp = b.p;

	if (sign == b.sign) {
		element			*ap = unique(max(an, bn));
		if (cmpn(ap, an, bp, bn) >= 0) {
			subn_from(bp, ap, bn);
		} else {
			subn(bp, ap, ap, bn, an);
			sign = !sign;
		}
	} else {
		element *ap = unique(max(an, bn) + 1);//add_size(get(p), an, bp, bn));
		addn_to(bp, ap, bn);
	}
	fix_size();
	return *this;
}

mpi operator-(param(mpi) a, mpi::element b) {
	const mpi::element	*ap = a.p;
	if (uint32 an = a.size) {
		if (!a.sign) {
			mpi				r(an, false);
			mpi::element	*rp = r.p;
			if (an == 1 && ap[0] < b) {
				rp[0]	= b - ap[0];
				r.sign	= true;
			} else {
				copy_rest(ap, rp, subn(ap, b, rp), an);
			}
			return r;
		} else {
			mpi				r(add_size(ap, an, b), true);
			mpi::element	*rp = r.p;
			copy_rest(ap, rp, addn(ap, b, rp), an);
			return r;
		}
	} else {
		return -mpi(b);
	}
}

mpi operator-(param(mpi) a, param(mpi) b) {
	bool		sign = !!a.sign;
	uint32		an = a.size;
	uint32		bn = b.size;
	const mpi::element *ap = a.p, *bp = b.p;

	if (a.sign == b.sign) {
		if (cmpn(bp, bn, ap, an) > 0) {
			swap(an, bn);
			swap(ap, bp);
			sign = !sign;
		}
		mpi	r(an, sign);
		mpi::element	*rp = r.p;
		subn(ap, bp, rp, an, bn);
		r.fix_size();
		return r;

	} else {
		if (bn > an) {
			swap(an, bn);
			swap(ap, bp);
		}
		uint32			rn = an + 1;//add_size(ap, an, bp, bn);
		mpi				r(rn, sign);
		mpi::element	*rp = r.p;
		addn(ap, bp, rp, an, bn);
		r.fix_size();
		return r;
	}
}

//-----------------------------------------------------------------------------
//	MUL
//-----------------------------------------------------------------------------

mpi &mpi::operator*=(element b) {
	if (size) {
		element *ap = unique(size + int(p[size - 1] >= ~element(0) / b));
		muln(ap, b, ap, size);
	}
	return *this;
}

mpi &mpi::operator*=(param(mpi) b) {
	return *this = *this * b;
}

mpi operator*(param(mpi) a, mpi::element b) {
	uint32				an = a.size;
	mpi					r(an + 1, a.sign);
	mpi::element		*rp = r.p;
	const mpi::element	*ap = a.p;
	mpi::element		*ep = muln(ap, b, rp, an);
	r.size = int(ep - rp);
	return r;
}

mpi operator*(param(mpi) a, param(mpi) b) {
	uint32	an = a.size;
	uint32	bn = b.size;
	uint32	rn = an + bn;

	mpi		r(rn, !!(a.sign ^ b.sign));
	const mpi::element	*ap = a.p, *bp = b.p;

	memset(r.p, 0, rn * sizeof(mpi::element));
	for (mpi::element *i = r.p; bn--;)
		maddn(ap, *bp++, i++, an);

	r.fix_size();
	return r;
}

mpi square(param(mpi) a) {
#if 1
	return a * a;
#else
	uint32	an	= a.size;
	uint32	tn	= 2 * an + 1;
	mpi		t(tn, false);

	const mpi::element	*ap = a.p;
	mpi::element		*tp = t.p;
	memset(tp, 0, tn * sizeof(mpi::element));

	for (uint32 i = 0; i < an; i++) {
		// first calculate the digit at 2*i
		mpi::element	hi;
		tp[i + i] = maddc(ap[i], ap[i], tp[2 * i], hi);

		mpi::element	x	= ap[i];
		mpi::element	*p	= tp + (2 * i + 1);

		for (uint32 j = i + 1; j < an; j++) {
			mpi::element2	r = (mpi::element2)x * ap[j];
			r		= r + r + hi + *p;
			*p++	= (mpi::element)r;
			hi		= (mpi::element)(r >> mpi::bits_per_element);
		}

		// propagate upwards
		addn(p, hi, p);
	}
	t.fix_size();
	return t;
#endif
}

//-----------------------------------------------------------------------------
//	DIV/MOD
//-----------------------------------------------------------------------------

mpi divmod(mpi &a, mpi::element b) {
	bool	neg = !!a.sign;

	a.sign = 0;
	if (a < b) {
		a.sign = neg;
		return zero;
	}

	int		k = ~highest_set_index(b) & (mpi::bits_per_element - 1);
	b <<= k;
	a <<= k;

	uint32		qn	= a.size - 1;
	auto		*ap	= a.unique();
	mpi			q(ap[qn] >= b ? qn + 1 : qn, neg);
	if (ap[qn] >= b) {
		q.p[qn] = 1;
		subn(ap + qn, b, ap + qn);
	}

	while (qn--) {
		mpi::element	*rt = ap + qn;
		mpi::element	x	= divc(rt[0], rt[1], b);
		mpi::element	t0, t1 = mulc(x, b, t0);
		subc(rt[1], t1, subc(rt[0], t0, false, rt[0]), rt[1]);
		q.p[qn] = x;
	}

	a.fix_size();
	a >>= k;
	if (a != 0)
		a.sign = neg;

	return q;
}

mpi divmod(mpi &a, param(mpi) b) {
	if (compare_abs(a, b) < 0)
		return zero;

//	mpi	a0 = a;

	uint32			bn	= b.size;
	int				k	= ~highest_set_index(b.p[bn - 1]) & (mpi::bits_per_element - 1);
	mpi				s	= b << k;
	mpi::element	*sp	= s.p;
	a <<= k;

	uint32			an	= a.size;
	mpi::element	*ap	= a.unique();
	uint32			qn	= an - bn;
	bool			neg	= !!a.sign;
	mpi::element	top	= sp[bn - 1];

	mpi				q(ap[an - 1] >= top ? qn + 1 : qn, neg ^ !!b.sign);
	if (ap[an - 1] >= top) {
		q.p[qn] = 1;
		subn_from(sp, ap + qn, bn);
	}

	a.sign = s.sign = false;

	mpi				t(bn + 1, false);
	mpi::element	*tp = t.p, *te = tp + bn + 1;

	while (qn--) {
		mpi::element	*rt = ap + bn + qn;
		mpi::element	x = divc(rt[-1], rt[0], top);

//		mpi				t = s * x;
		mpi::element	*tt = muln(sp, x, tp, bn);
		if (tt < te)
			*tt = 0;

		while (cmpn(ap + qn, tp, bn + 1) < 0) {
			subn_from(sp, tp, bn);
			x--;
		}
		subn_from(tp, ap + qn, bn + 1);
		q.p[qn] = x;
	}

	a.fix_size();
	a >>= k;
	if (a != 0)
		a.sign = neg;

//	ISO_ASSERT(q * b + a == a0);

	return q;
}

mpi divmod_close(mpi &a, param(mpi) b) {
	if (compare_abs(a, b) < 0)
		return zero;

	int		an = a.num_bits(), bn = b.num_bits();
	if (an == bn) {
		a	-= b;
		return mpi(1);
	}

	if (an == bn + 1) {
		a -= b;
		if (a < b)
			return mpi(1);
		a -= b;
		if (a < b)
			return mpi(2);
		a -= b;
		return mpi(3);
	}

	return divmod(a, b);
}

mpi operator/(param(mpi) a, mpi::element b) {
	mpi	r;
	return divmod(r = a, b);
}

mpi	operator/(param(mpi) a, param(mpi) b) {
	mpi	r;
	return divmod(r = a, b);
}

mpi &mpi::operator/=(element b) {
	return *this = divmod(*this, b);
}

mpi &mpi::operator/=(param(mpi) b) {
	return *this = divmod(*this, b);
}

mpi::element operator%(param(mpi) a, mpi::element b) {
	if (is_pow2(b))
		return a.p[0] & (b - 1);
	mpi	r;
	divmod(r = a, b);
	return r.sign ? b - r.p[0] : r.p ? r.p[0] : 0;
}

mpi operator%(param(mpi) a, param(mpi) b) {
	mpi	r;
	divmod(r = a, b);
	return r;
}

mpi &mpi::operator%=(element b) {
	divmod(*this, b);
	return *this;
}

mpi &mpi::operator%=(param(mpi) b) {
	divmod(*this, b);
	return *this;
}

mpi pow(param(mpi) a, param(mpi) b) {
	mpi		r;
	if (b.is_odd())
		r = a;
	else
		r = 1;

	mpi		v = a;
	for (int i = 1, n = b.num_bits(); i < n; i++) {
		v = square(v);
		if (b.test_bit(i))
			r *= v;
	}
	return r;
}

mpi exp_mod(param(mpi) a, param(mpi) b, param(mpi) p) {
	mpi		r;
	mpi		v = a % p;

	if (b.is_odd())
		r = v;
	else
		r = 1;

	for (int i = 1, n = b.num_bits(); i < n; i++) {
		v = square(v) % p;
		if (b.test_bit(i)) {
			r *= v;
			r %= p;
		}
	}
	return r;
}

mpi sqrt(param(mpi) a) {
	if (get_sign(a))
		return mpi::error();

	if (!a)
		return a;

	// First approx
	int	n = highest_set_index(a) / 2;
	mpi	t2 = mpi(1) << n;
	mpi	t1 = a >> n;

	t1 += t2;
	t1 >>= 1;

	do {
		t2 = a / t1;
		t1 += t2;
		t1 >>= 1;
	} while (compare_abs(t1, t2) > 0);
	return t1;
}

mpi root_n(param(mpi) a, mpi::element b) {
	bool	neg = get_sign(a);

	// input must be positive if b is even
	if ((b & 1) == 0 && neg)
		return mpi::error();

	const_cast<mpi&>(a).sign = false;

	mpi		t1, t2(2);
	do {
		t1 = t2;
		mpi	t3 = pow(t1, b - 1);
		t2 = t1 - (t3 * t1 - a) / (t3 * b);
	} while (t1 != t2);

	// result can be off by a few so check
	while (pow(t1, b) > a)
		t1 -= 1;

	// reset the sign of a first
	const_cast<mpi&>(a).sign	= neg;
	t1.sign = neg;
	return t1;
}

//-----------------------------------------------------------------------------
//	LOGIC OPS
//-----------------------------------------------------------------------------

mpi &mpi::operator&=(element b) {
	if (p) {
		element *ap = unique(1);
		ap[0] &= b;
	}
	return *this;
}

mpi &mpi::operator|=(element b) {
	if (p) {
		element *ap = unique();
		ap[0] |= b;
	} else {
		p.create(size = 1);
		*p = b;
	}
	return *this;
}

mpi &mpi::operator^=(element b) {
	if (p) {
		element *ap = unique();
		ap[0] ^= b;
	} else {
		p.create(size = 1);
		*p = b;
	}
	return *this;
}

mpi &mpi::operator&=(param(mpi) b) {
	uint32			rn	= min(size, b.size);
	element			*ap = unique(rn);
	const element	*bp = b.p;
	while (rn)
		*ap++ &= *bp++;
	return *this;
}

mpi &mpi::operator|=(param(mpi) b) {
	uint32			rn	= max(size, b.size);
	element			*ap = unique(rn);
	const element	*bp = b.p;
	for (uint32 n = min(size, b.size); n; --n)
		*ap++ |= *bp++;
	return *this;
}

mpi &mpi::operator^=(param(mpi) b) {
	uint32			rn	= max(size, b.size);
	element			*ap	= unique(rn);
	const element	*bp	= b.p;
	for (uint32 n = min(size, b.size); n; --n)
		*ap++ ^= *bp++;
	return *this;
}

mpi operator&(param(mpi) a, param(mpi) b) {
	uint32	rn = min(a.size, b.size);
	mpi		r(rn, false);

	const mpi::element	*ap = a.p, *bp = b.p;
	mpi::element		*rp = r.p;
	while (rn--)
		*rp++ = *ap++ & *bp++;
	return r;
}

mpi operator|(param(mpi) a, param(mpi) b) {
	uint32	rn = max(a.size, b.size);
	mpi		r(rn, false);

	const mpi::element	*ap = a.p, *bp = b.p;
	mpi::element		*rp = r.p;
	for (uint32 n = min(a.size, b.size); n; --n)
		*rp++ = *ap++ | *bp++;

	int	n2 = a.size - b.size;
	if (n2 < 0) {
		ap = bp;
		n2 = -n2;
	}
	while (n2--)
		*rp++ = *ap++;
	return r;
}

mpi operator^(param(mpi) a, const mpi & b) {
	uint32	rn = max(a.size, b.size);
	mpi		r(rn, false);

	const mpi::element	*ap = a.p, *bp = b.p;
	mpi::element		*rp = r.p;
	for (uint32 n = min(a.size, b.size); n; --n)
		*rp++ = *ap++ | *bp++;

	int	n2 = a.size - b.size;
	if (n2 < 0) {
		ap = bp;
		n2 = -n2;
	}
	while (n2--)
		*rp++ = *ap++;
	return r;
}

mpi		mpi::random(int bits)				{ return random(random2, bits); }
mpi		mpi::random_to(param(mpi) range)	{ return random_to(random2, range); }

//-----------------------------------------------------------------------------
//	Jabobi symbol
//-----------------------------------------------------------------------------

int jacobi(param(mpi) a, param(mpi) b) {
	static const int8 table[8] = { 0, 1, 0, -1, 0, -1, 0, 1 };

	// if B <= 0 return error
	if (b <= zero)
		return 2;

	int	product = 1;
	for (mpi A = a, B = b;;) {
		// step 1. if A == 0, return 0
		if (A == zero)
			return 0;

		// step 2. if A == 1, return 1
		if (A == 1)
			break;

		// step 3. write A = A1 * 2**k
		int		k	= lowest_set_index(A);
		mpi		A1	= A >> k;

		// step 4. if e is even set s=1, else set s=1 if B = 1/7 (mod 8) or s=-1 if B = 3/5 (mod 8)
		product	*= k & 1 ? table[B & 7] : 1;

		// step 5. if B == 3 (mod 4) *and* A1 == 3 (mod 4) then s = -s
		if ((B & 3) == 3 && (A1 & 3) == 3)
			product = -product;

		// if A1 == 1 we're done
		if (A1 == 1)
			break;

		A = B % A1;
		B = A1;
	}
	return product;
}

//-----------------------------------------------------------------------------
//	Kronecker symbol
//-----------------------------------------------------------------------------

int kronecker(param(mpi) a, param(mpi) b) {
	// In 'tab', only odd-indexed entries are relevant: For any odd n, tab[n & 7] is (-1) ^ ((n^2 - 1) / 8)
	static const int tab[8] = { 0, 1, 0, -1, 0, -1, 0, 1 };

	if (b == zero)
		return int(a == mpi(one));

	if (!a.is_odd() && !b.is_odd())
		return 0;

	// now B is non-zero
	int	i = lowest_set_index(b);
	mpi	B = b >> i;

	int	ret = i & 1 ? tab[a & 7] : 1;
	if (B < zero && a < zero)
		ret = -ret;

	mpi	A	= a;
	B		= abs(B);
	// now B is positive and odd, so what remains to be done is to compute the Jacobi symbol (A/B) and multiply it by ret
	for (;;) {
		//  B  is positive and odd
		if (A == zero)
			return B == mpi(one) ? ret : 0;

		// now  A  is non-zero
		i = lowest_set_index(A);
		A >>= i;
		if (i & 1)
			ret *= tab[B & 7];			// i is odd - multiply table entry

		// multiply ret by  (-1)^(A-1)(B-1)/4}
		if ((A < zero ? ~A.lowest_bits() : A.lowest_bits()) & B.lowest_bits() & 2)
			ret = -ret;

		// (A, B) := (B mod |A|, |A|)
		mpi	t = nnmod(B, A);
		B	= abs(A);
		A	= t;
	}
}

//-----------------------------------------------------------------------------
//	MONTGOMERY
//-----------------------------------------------------------------------------

// Montgomery initialisation
static mpi::element montgomery_invmod(param(mpi) n) {
	mpi::element m0 = n.lowest_bits();
	mpi::element x = m0 + (((m0 + 2) & 4) << 1);

	for (int i = mpi::bytes_per_element; i; i >>= 1)
		x *= 2 - (m0 * x);

	return ~x + 1;
}

// Montgomery multiplication: A = A * B * R^-1 mod p (HAC 14.36)
static mpi::element *montgomery_mul(const mpi::element *ap, const mpi::element *bp, const mpi::element *np, uint32 size1, uint32 size2, mpi::element mm, mpi::element *temp) {
	size2 = min(size1, size2);

	memset(temp, 0, (size1 * 2 + 1) * sizeof(mpi::element));
	for (uint32 i = 0; i < size1; i++) {
		// T = (T + u0 * B + u1 * p) / (2 ^ bits_per_element)
		mpi::element	u0 = ap[i];
		mpi::element	u1 = (*temp + u0 * bp[0]) * mm;

		maddn(bp, u0, temp, size2);
		maddn(np, u1, temp, size1);

		*temp++ = u0;
	}

	if (temp[size1] || cmpn(temp, np, size1) >= 0)
		subn_from(np, temp, size1);
	return temp;
}


// Montgomery reduction: A = A * R^-1 mod p
static mpi::element *montgomery_reduction(const mpi::element *ap, const mpi::element *np, uint32 size, mpi::element mm, mpi::element *temp) {
	memset(temp, 0, (size * 2 + 1) * sizeof(mpi::element));
	for (uint32 i = 0; i < size; i++) {
		// T = (T + u0 + u1 * p) / (2 ^ bits_per_element)
		mpi::element	u0 = ap[i];
		mpi::element	u1 = (*temp + u0) * mm;

		addn(temp, u0, temp);
		maddn(np, u1, temp, size);

		*temp++ = u0;
	}

	if (temp[size] || cmpn(temp, np, size) >= 0)
		subn_from(np, temp, size);
	return temp;
}

static void montgomery_mul(mpi &a, param(mpi) b, param(mpi) n, uint32 size1, mpi::element mm, mpi::element *temp) {
	mpi::element *x = montgomery_mul(a.elements_ptr(), b.elements_ptr(), n.elements_ptr(), size1, b.num_elements(), mm, temp);
	a.set(x, size1);
}

static void montgomery_reduction(mpi &a, param(mpi) n, uint32 size, mpi::element mm, mpi::element *temp) {
	mpi::element *x = montgomery_reduction(a.elements_ptr(), n.elements_ptr(), size, mm, temp);
	a.set(x, size);
}


// Sliding-window exponentiation: X = A^E mod p (HAC 14.85)
static mpi montgomery_exp_mod(param(mpi) a, param(mpi) e, param(mpi) n, uint32 nsize, param(mpi) rr, uint32 mm) {
	int				bits	= e.num_bits();
	int				wsize	= bits > 671 ? 6 : bits > 239 ? 5 : bits > 79 ? 4 : bits > 23 ? 3 : 1;
	mpi::element	*temp	= alloc_auto(mpi::element, nsize * 2 + 1);
	mpi				x, w[64];

	// W[1] = A * R^2 * R^-1 mod p = A * R mod p
	w[1] = a % n;
	montgomery_mul(w[1], rr, n, nsize, mm, temp);

	// X = R^2 * R^-1 mod p = R mod p
	x = rr;
	montgomery_reduction(x, n, nsize, mm, temp);

	if (wsize > 1) {
		// W[1 << (wsize - 1)] = W[1] ^ (wsize - 1)
		int	j = 1 << (wsize - 1);
		w[j] = w[1];

		for (int i = 0; i < wsize - 1; i++)
			montgomery_mul(w[j], w[j], n, nsize, mm, temp);

		// W[i] = W[i - 1] * W[1]
		for (int i = j + 1; i < (1 << wsize); i++) {
			w[i] = w[i - 1];
			montgomery_mul(w[i], w[1], n, nsize, mm, temp);
		}
	}

	int		setbits = 0;
	while (bits--) {
		bool	ei = e.test_bit(bits);

		// skip leading 0s
		if (!ei && setbits == 0) {
			montgomery_mul(x, x, n, nsize, mm, temp);		// out of window, square X
			continue;
		}

		// add ei to current window
		setbits = (setbits << 1) | int(ei);

		if (setbits & (1 << (wsize - 1))) {
			// X = X ^ wsize R^-1 mod p
			for (int i = 0; i < wsize; i++)
				montgomery_mul(x, x, n, nsize, mm, temp);

			// X = X * W[wbits] R^-1 mod p
			montgomery_mul(x, w[setbits], n, nsize, mm, temp);
			setbits = 0;
		}
	}

	// process the remaining bits
	while (setbits) {
		montgomery_mul(x, x, n, nsize, mm, temp);
		if (setbits & 1)
			montgomery_mul(x, w[1], n, nsize, mm, temp);
		setbits >>= 1;
	}

	// X = A^E * R * R^-1 mod p = A^E mod p
	montgomery_reduction(x, n, nsize, mm, temp);
	return x;
}

mpi exp_mod(param(mpi) a, param(mpi) e, param(mpi) p, mpi &rr) {
	ISO_ASSERT(!get_sign(p) && p.is_odd());

	// If 1st call, pre-compute R^2 mod p
	if (!rr) {
		rr.set_bit(p.size * 2 * mpi::bits_per_element);
		rr %= p;
	}

	return montgomery_exp_mod(a, e, p, p.num_elements(), rr, montgomery_invmod(p));
}

void montgomery_context::init(param(mpi) _p) {
	p		= abs(_p);
	rr		= 0;
	rr.set_bit(p.num_elements() * mpi::bits_per_element * 2);
	rr		%= p;

	mm		= montgomery_invmod(p);
}

mpi montgomery_context::from(param(mpi) a) const {
	mpi::element	*temp	= alloc_auto(mpi::element, p.num_elements() * 2 + 1);
	mpi::element	*x		= montgomery_reduction(a.elements_ptr(), p.elements_ptr(), p.num_elements(), mm, temp);
	return mpi(x, p.num_elements());
}

mpi montgomery_context::exp(param(mpi) a, param(mpi) e) const {
	return montgomery_exp_mod(a, e, p, p.num_elements(), rr, mm);
}

mpi montgomery_context::mul(param(mpi) a, param(mpi) b) const {
	mpi::element	*temp	= alloc_auto(mpi::element, p.num_elements() * 2 + 1);
	mpi::element	*x		= montgomery_mul(a.elements_ptr(), b.elements_ptr(), p.elements_ptr(), p.num_elements(), b.num_elements(), mm, temp);
	return mpi(x, p.num_elements());
}

// Returns 'ret' such that ret^2 == a (mod p), using the Tonelli/Shanks algorithm (cf. Henri Cohen, "A Course in Algebraic Computational Number Theory", algorithm 1.5.1). 'p' must be prime!
mpi montgomery_context::sqrt(param(mpi) a) const {
	if (!p.is_odd() || abs(p) == mpi(one)) {
		ISO_ASSERT(p == 2);
		return mpi(a & 1);
	}

	if (a == zero || a == mpi(one))
		return a;

	mpi	A = nnmod(a, p);

	int e = 1;
	while (!p.test_bit(e))
		e++;

	if (e == 1) {
		// (|p| - 1) / 2  is odd, so 2 has an inverse modulo (|p| - 1) / 2, and square roots can be computed directly by modular exponentiation
		// We have
		//     2 * (|p| + 1) / 4 == 1  (mod (|p| - 1) / 2),
		// so we can use exponent  (|p| + 1) / 4,  i.e.  (|p| - 3) / 4 + 1
		return exp(A, abs(p >> 2) + one);
	}

	if (e == 2) {
		// |p| == 5  (mod 8)
		// In this case 2 is always a non-square since Legendre(2,p) = (-1) ^ ((p ^ 2 - 1) / 8)  for any odd prime; so if a really is a square, then 2*a is a non-square
		// Thus for
		//      b := (2*a)^((|p|-5)/8),
		//      i := (2*a)*b^2
		// we have
		//     i^2 = (2*a)^((1 + (|p|-5)/4)*2)
		//         = (2*a)^((p-1)/2)
		//         = -1;
		// so if we set
		//      x := a*b*(i-1),
		// then
		//     x^2 = a^2 * b^2 * (i^2 - 2*i + 1)
		//         = a^2 * b^2 * (-2*i)
		//         = a*(-i)*(2*a*b^2)
		//         = a*(-i)*i
		//         = a

		mpi	t = a << 1;
		mpi	b = exp(t, abs(p) >> 3);
		mpi	y = square(b);
		t = mul(t, y) - one;
		return mul(mul(A, b), t);
	}

	// find some y that is not a square
	mpi	q = abs(p);
	mpi	y;
	for (int i = 2, r = 0; r != -1 && i < 82; i++) {
		// for efficiency, try small numbers first; if this fails, try random numbers
		if (i < 22) {
			y = i;
		} else {
			y = mpi::random(random2, p.num_bits());
			if (y >= p)
				y -= p;
			if (y == zero)
				y = i;
		}

		r = kronecker(y, q); // here 'q' is |p|
		ISO_ASSERT(r != 0);
	}

	q >>= e;

	// Now that we have some non-square, we can find an element of order 2^e by computing its q'th power.
	y = exp(y, q);
	ISO_ASSERT(y != mpi(one));

	// Now we know that (if p is indeed prime) there is an integer k,  0 <= k < 2^e,  such that
	//      a^q * y^k == 1   (mod p)
	//
	// As a^q is a square and y is not, k  must be even. q+1 is even, too, so there is an element
	//     X := a^((q+1)/2) * y^(k/2),
	// and it satisfies
	//     X^2 = a^q * a * y^k
	//         = a,
	// so it is the square root that we are looking for

	mpi	t	= q >> 1;
	mpi	x;
	if (t == zero) {        // special case: p = 2^e + 1
		if (A % p == zero)
			x = zero;		// special case: a == 0  (mod p)
		else
			x = one;
	} else {
		x = exp(A, t);
		if (x == zero)
			return zero;	// special case: a == 0  (mod p)
	}

	mpi	b = mul(square(x), A);
	x = mul(x, A);

	for (;;) {
		// Now b is  a^q * y^k  for some even  k, and x is a ^ ((q + 1) / 2) * y ^ (k / 2).
		// We have  a*b = x ^ 2,
		//    y^2^(e-1) = -1, b^2^(e-1) = 1

		if (b == mpi(one))
			break;

		// find smallest  i  such that  b^(2^i) = 1
		int	i = 1;
		t = square(b);
		while (t != mpi(one)) {
			i++;
			ISO_ASSERT(i != e);
			t = square(t);
		}

		// t = y^2^(e - i - 1)
		t = y;
		for (int j = e - i - 1; j > 0; j--)
			t = square(t);

		y = square(t);
		x = mul(x, t);
		b = mul(b, y);
		e = i;
	}

	// verify the result -- the input might have been not a square
	ISO_ASSERT(square(x) == A);
	return x;
}


//-----------------------------------------------------------------------------
//	GCD
//-----------------------------------------------------------------------------

// Greatest common divisor: G = gcd(A, B) (HAC 14.54)
mpi gcd(param(mpi) a, param(mpi) b) {
	int	lz = min(lowest_set_index(a), lowest_set_index(b));
	mpi	ta = a >> lz;
	mpi	tb = b >> lz;
	ta.sign = tb.sign = 0;

	while (ta != 0) {
		ta >>= lowest_set_index(ta);
		tb >>= lowest_set_index(tb);
		mpi::element	*pa = ta.p;
		mpi::element	*pb = tb.p;
		if (ta >= tb) {
			subn_from(pb, pa, tb.num_elements());
			ta >>= 1;
		} else {
			subn_from(pa, pb, ta.num_elements());
			tb >>= 1;
		}
	}

	return tb << lz;
}

//	Extended euclidean algorithm:
//	returns gcd(a, b)
//	s, t are bezout coefficients: a.s + b.t = gcd(a,b)
mpi extended_euclid(param(mpi) a, param(mpi) b, mpi &s, mpi &t) {
	mpi	r0	= a,		r1 = b;
	mpi	s0	= mpi(1),	s1 = zero;
	mpi	t0	= zero,		t1 = mpi(1);
	mpi	q;

	for (;;) {
		q	= divmod(r0, r1);
		if (!r0) {
			s = s1;
			t = t1;
			return r1;
		}
		s0	-= q * s1;
		t0	-= q * t1;

		q	= divmod(r1, r0);
		if (!r1) {
			s = s0;
			t = t0;
			return r0;//break;	//gcd is r0, inv_mod(a, n) is s0
		}
		s1	-= q * s0;
		t1	-= q * t0;
	}
}

// Modular inverse: X = A^-1 mod p (HAC 14.61 / 14.64)
mpi inv_mod(param(mpi) a, param(mpi) n) {
	//extended euclidean algorithm
	mpi	r0	= a,		r1 = n;
	mpi	s0	= mpi(1),	s1 = zero;
	mpi	q;

	for (;;) {
		q	= divmod(r0, r1);
		if (!r0)
			return s1 < zero ? s1 + n : s1;
		s0	-= q * s1;

		q	= divmod(r1, r0);
		if (!r1)
			return s0 < zero ? s0 + n : s0;
		s1	-= q * s0;
	}
}


//-----------------------------------------------------------------------------
//	PRIME
//-----------------------------------------------------------------------------

bool prime_checker::is_witness(param(mpi) x) {
	mpi	w = exp(x, N1_odd);

	if (w == 1 || w == N1)
		return false;				// probably prime

	for (int i = k; --i;) {
		w = square(w);

		if (w == 1)
			return true;			// A is composite, otherwise a previous w would have been == -1 (mod A)

		if (w == N1)
			return false;			// w == -1 (mod A), probably prime
	}
	// If we get here, w is the (A-1)/2-th power of the original w, and is neither -1 nor +1 -- so A cannot be prime
	return true;
}

bool is_prime(param(mpi) a) {
	if (a <= 1)
		return false;

	for (auto i : primes) {
		if (a % i == 0)
			return a == i;
	}

	prime_checker	checker(a);
	for (int n = prime_checker::num_checks(a.num_bits()); n--;) {
		if (checker.check(random2))
			return false;
	}

	return true;
}

mpi mpi::generate_prime(int bits) {
	int				checks			= prime_checker::num_checks(bits);
	bool			is_single_word	= bits <= bits_per_element;
	element			maxdelta		= iso::bits<element>(bits) - end(primes)[-1];
	
	temp_array<uint16>	mods(iso::num_elements(primes));

	mpi		r;

	for (bool is_prime = false; !is_prime;) {
		element	delta;
		do {
			r = mpi::random(random2, bits);
			r.set_bit(0);
			r.set_bit(bits - 1);
			r.set_bit(bits - 2);

			for (int i = 1; i < iso::num_elements(primes); i++)
				mods[i] = r % primes[i];

			// If bits is so small that it fits into a single word then we additionally don't want to exceed that many bits
			if (is_single_word)
				maxdelta = iso::bits<mpi::element>(bits) - max(r.lowest_bits(), end(primes)[-1]);

			for (delta = 0; delta <= maxdelta; delta += 2) {
				is_prime = true;
				if (is_single_word) {
					// In the case that the candidate prime is a single word then we check that:
					// 1) It's greater than primes[i] because we shouldn't reject 3 as being a prime number because it's a multiple of three
					// 2) That it's not a multiple of a known prime. We don't check that r-1 is also coprime to all the known primes because there aren't many small primes where that's true
					mpi::element r0 = r.lowest_bits();
					for (int i = 1; is_prime && i < iso::num_elements(primes) && primes[i] < r0; i++)
						is_prime = (mods[i] + delta) % primes[i] != 0;

				} else {
					// check that r is not a prime and also that gcd(r-1,primes) == 1 (except for 2)
					for (int i = 1; is_prime && i < iso::num_elements(primes); i++)
						is_prime = (mods[i] + delta) % primes[i] > 1;
				}
				if (is_prime)
					break;
			}

		} while (!is_prime && r.num_bits() != bits);

		r += delta;

		prime_checker	checker(r);
		for (int i = 0; is_prime && i < checks; i++)
			is_prime = !checker.check(random2);
	}

	return r;
}

//-----------------------------------------------------------------------------
//	STRING
//-----------------------------------------------------------------------------

const char *mpi::read_base(const char *p, int B) {
	reset();
	for (uint32 d; is_alphanum(*p) && (d = from_digit(*p)) < B; ++p)
		(*this *= B) += d;
	return p;
}

size_t put_signed_num_base(int B, char *s, param(mpi) i, char ten) {
	if (!i) {
		*s = '0';
		return 1;
	}

	int			log2x4	= log2_floor(square(square(B)));
	int			digits	= (highest_set_index(i) + 1) * 4 / log2x4 + 1;

	uint32			digits_per_chunk = (sizeof(mpi::element) * 32 - 1) / log2x4;
//	uint32			digits_per_chunk = sizeof(baseint<10, mpi::element>::digits - 1;
	mpi::element	chunk_const	= pow(B, digits_per_chunk);
	uint32			nchunks		= div_round_up(digits, digits_per_chunk);
	mpi::element	*chunks		= alloc_auto(mpi::element, nchunks), *p = chunks;

	for (mpi t = i; !!t;) {
		mpi	x	= t;
		mpi	t2 = divmod(t, chunk_const);
		ISO_ASSERT(t2 * chunk_const + t == x);
		*p++ = t.lowest_bits();
		swap(t, t2);
	}

	char	*s0		= s;

	if (get_sign(i))
		*s++ = '-';

	s += put_unsigned_num_base(B, s, *--p, ten);
	while (p != chunks) {
		put_num_base(B, s, digits_per_chunk, *--p, ten);
		s += digits_per_chunk;
	}

	return s - s0;
}

string _to_string_base(uint32 B, param(mpi) a) {
	int		digits = int((highest_set_index(a) + 1) * 4 / log2_floor(square(square(B))) + 1);
	string	s(digits);
	return s.resize(put_signed_num_base(B, s, a, 'A'));
}


//-----------------------------------------------------------------------------
//	mpf
//-----------------------------------------------------------------------------

// abs
mpf abs(const mpf &a) {
	mpf	r = a;
	r.sign = false;
	return r;
}

mpi trunc(const mpf &a) {
	if (a.exp >= 0)
		return a;
	return a >> -a.exp;
}

mpi round(const mpf &a) {
	if (a.exp >= 0)
		return a;
	mpi	r = a >> -a.exp;
	int	e = -a.exp - 1;
	if ((a.p[e / mpf::bits_per_element] >> (e % mpf::bits_per_element)) & 1) {
		auto	rp = r.elements_ptr();
		addn(rp, 1u, rp);
	}
	return r;
}

bool any_frac(const mpi::element *p, int e) {
	if (e < 0) {
		uint32	m = bits(-e % mpf::bits_per_element);
		int		n = -e / mpf::bits_per_element;
		if (p[n] & m)
			return true;
		while (n--) {
			if (p[n])
				return true;
		}
	}
	return false;
}

mpi ceil(const mpf &a) {
	if (a.exp >= 0)
		return a;
	mpi	r = a >> -a.exp;
	if (!get_sign(r) && any_frac(a.p, a.exp))
		r += 1;
	return r;
}

mpi floor(const mpf &a) {
	if (a.exp >= 0)
		return a;
	mpi	r = a >> -a.exp;
	if (get_sign(r) && any_frac(a.p, a.exp))
		r -= 1;
	return r;
}

mpf frac(const mpf &a) {
	mpf		r	= sfrac(a);
	return get_sign(r) ? r + mpf(1) : r;
}

mpf sfrac(const mpf &a) {
	if (a.exp >= 0)
		return zero;
	uint32	n = mpf::nbits(-a.exp);
	mpi		r(n, a.sign);
	r.copy_elements(a, 0, 0, n);
	if (int m = -a.exp % mpf::bits_per_element)
		r.top_element() &= bits(m);
	return mpf(r, a.exp);
}


// negate
mpf operator-(const mpf &a) {
	mpf	r = a;
	r.sign = !a.sign;
	return r;
}

mpf operator+(const mpf &a, const mpf &b) {
	if (a.exp > b.exp) {
		mpi	c = a << (a.exp - b.exp);
		mpi	r = c + (mpi&)b;
		return mpf(r, b.exp);
	} else {
		mpi	c = b << (b.exp - a.exp);
		mpi	r = (mpi&)a + c;
		return mpf(r, a.exp);
	}
}

mpf operator-(const mpf &a, const mpf &b) {
	if (a.exp > b.exp) {
		mpi	c = a << (a.exp - b.exp);
		mpi	r = c - (mpi&)b;
		return mpf(r, b.exp);
	} else {
		mpi	c = b << (b.exp - a.exp);
		mpi	r = (mpi&)a - c;
		return mpf(r, a.exp);
	}
}
mpf operator*(const mpf &a, const mpf &b) {
	return mpf((mpi&)a * (mpi&)b, a.exp + b.exp);
}
mpf operator/(const mpf &a, const mpf &b) {
	int	shift = b.size * mpf::bits_per_element;
	return mpf(((mpi&)a << shift) / (mpi&)b, a.exp - shift - b.exp);
}

mpf sqrt(param(mpf) a) {
	if (get_sign(a))
		return mpi::error();

	if (!a)
		return a;

	int	n = highest_set_index((const mpi&)a) & ~1;
	return { sqrt((const mpi&)a << n), (a.exp - n) / 2 };
}

string to_string(const mpf &f) {
	if (f.exp > 0)
		return to_string(f << f.exp);

	string		s;
	if (f.size * mpf::bits_per_element + f.exp > 0)
		s = to_string(f >> -f.exp);

	s << '.';

	mpf	t = f;
	for (int digits	= int(-f.exp * 0.30103f + 1); digits--;) {
		t = frac(t);
		t = t * 10;
		mpi	i = trunc(t);
		s << char(i.lowest_bits() + '0');
	}

	return s;
}

size_t	from_string(const char* s, mpf &f) {
	const char*	s0	= s;
	bool		neg	= *s == '-';
	if (*s == '-' || *s == '+')
		++s;

	s = f.read_base(s, 10);
	
	const char *sf = s;
	if (*s == '.') {
		++sf;
		while (is_digit(*++s))
			((mpi&)f *= 10) += from_digit(*s);
	}

	int	exp = sf - s;

	if (*s == 'e' || *s == 'E') {
		int	exp2 = 0;
		s += from_string(s, exp2);
		exp += exp2;
	}
	if (exp > 0)
		f *= pow(mpi(10), uint32(exp));
	else if (exp < 0)
		f /= pow(mpi(10), uint32(-exp));

	f.sign	= neg;
	return s - s0;
}

//-----------------------------------------------------------------------------
//	TEST
//-----------------------------------------------------------------------------

struct test_mpi {
	test_mpi() {
		static const char* integers[] = {
			"38970864338945432122540493330",
			"1",
			"613069000200208324900377821045138855",
			"676",
			"-162252177270612080800354289",
			"-9085629961520001373682172816342224017978774",
			"-78832788539884199880097686237319433598035440724957",
			"6614089584408388605941540689825227882893",
			"-805251341866299612827898838543404817740298",
			"-9876406314976166168875536467247",
			"7476643748824994962126127587936517",
			"-3",
			"-137974477373101658403",
			"-598846631230248370921793389953703365435",
			"-32295598986080472414510811096",
			"8172648714775618865849571",
			"-552029132928764298233426403371814503980045782375",
			"-472472544437185617122410460268845698991861050",
			"-2411726037",
			"43670541682445",
			"-7101362632712160919923101928537876493",
			"65561025690135",
			"874303725732587323012791",
			"504009535909137145725532454473",
			"-5451341870425543",
			"-5324689203421",
			"45523",
			"-43310338",
			"19177882961396749019027701993249",
			"937650160918346267351092413",
			"-8690344505230602695",
			"-54256923348048786",
			"7925915119941850467401172406441954970102917551483",
			"1644912809509866030609261820301955416046966",
			"412657195378484435042062947629",
			"7569",
			"460682020424125194170487548466099365021",
			"225441526405993183338592697959442037939",
			"6",
			"29314775465433278633518516548926817",
			"-289956",
			"-815556752954289684",
			"82623051543",
			"-161143164147146728078412346990",
			"-14015458451456976295645175980553637308035589226813",
			"69226",
			"90739",
			"-79790564386954841206288499604417567651071290264",
			"-295204784952481987774288523093540",
			"9493853961518296192970396407960488388979634325253",
			"-5058219",
			"1",
			"2"
		};

		ISO_TRACEF("Check:") << sqrt(1000_mpi) << '\n';

		//		mpf	f(3.141592653589793);
		auto	f = 3.1415926535897932384626433832795_mpf;
		ISO_TRACEF("Check:") << f << '\n';
		ISO_TRACEF("Check:") << sqrt(f) << '\n';

		mpi_const<uint64,
			0xFFFFFFFFFFFFFFFF, 0xC90FDAA22168C234, 0xC4C6628B80DC1CD1, 0x29024E088A67CC74,
			0x020BBEA63B139B22, 0x514A08798E3404DD, 0xEF9519B3CD3A431B, 0x302B0A6DF25F1437,
			0x4FE1356D6D51C245, 0xE485B576625E7EC6, 0xF44C42E9A63A3620, 0xFFFFFFFFFFFFFFFF
		>	MODP768;

		mpi	x(MODP768);
		ISO_TRACEF("MODP768=") << hex(x) << '\n';

		for (int i = 0, m = int(num_elements(integers)); i < m; i++) {
			mpi	a, b, c;
			from_string(integers[i], a);
			from_string(integers[(i + 1) % m], b);
			//		ISO_TRACEF("") << a << '\n';

			c = a + b; ISO_TRACEF("") << a << " + " << b << " = " << c << '\n';
			c = a - b; ISO_TRACEF("") << a << " - " << b << " = " << c << '\n';
			c = a * b; ISO_TRACEF("") << a << " * " << b << " = " << c << '\n';
			c = a / b; ISO_TRACEF("") << a << " / " << b << " = " << c << '\n';
			c = a % b; ISO_TRACEF("") << a << " % " << b << " = " << c << '\n';
			ISO_TRACEF("Check:") << ((a / b) * b + a % b == a) << '\n';
		}
	};
};// _test_mpi;

} // namespace iso
