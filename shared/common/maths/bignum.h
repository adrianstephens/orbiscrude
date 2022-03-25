#ifndef BIGNUM_H
#define BIGNUM_H

#include "base/defs.h"
#include "base/bits.h"
#include "base/strings.h"
#include "base/array.h"
#include "base/maths.h"

namespace iso {

template<typename T> int cmpn(const T *a, const T *b, int n) {
	if (n == 0)
		return 0;
	while (--n && a[n] == b[n]);
	return a[n] < b[n] ? -1 : a[n] > b[n] ? 1 : 0;
}
template<typename T> int cmpn(const T *a, int na, const T *b, int nb) {
	return na != nb ? na - nb : cmpn(a, b, na);
}

// Bits of security, see SP800-57
inline int security_bits(int L, int N) {
	int	secbits	= L >= 15360	? 256
				: L >= 7690		? 192
				: L >= 3072		? 128
				: L >= 2048		? 112
				: L >= 1024		? 80
				: 0;
	return N == 0 ? secbits : N < 160 ? 0 : min(N / 2, secbits);
}


//-----------------------------------------------------------------------------
//	mpi_const
//-----------------------------------------------------------------------------

#ifdef USE_VARIADIC_TEMPLATES

template<typename T, T... args> struct mpi_const;

template<typename IL, typename VL> struct mpi_u8;
template<int...II, typename VL> struct mpi_u8<index_list<II...>, VL> : static_ref_array<uint32, uint32(meta::VL_index<VL::count - 1 - II*4, VL>::value + (meta::VL_index<VL::count - 2 - II*4, VL>::value<<8) + (meta::VL_index<VL::count - 3 - II*4, VL>::value<<16) + (meta::VL_index<VL::count - 4 - II*4, VL>::value<<24))...> {};
template<uint8... args> struct mpi_const<uint8, args...> : mpi_u8<meta::make_index_list<sizeof...(args) / 4>, meta::value_list<uint8, args...> > {};


template<typename IL, typename VL> struct mpi_u32;
template<int...II, typename VL> struct mpi_u32<index_list<II...>, VL> : static_ref_array<uint32, (meta::VL_index<VL::count - 1 - II, VL>::value)...> {};
template<uint32... args> struct mpi_const<uint32, args...> : mpi_u32<meta::make_index_list<sizeof...(args)>, meta::value_list<uint32, args...> > {};

template<typename IL, typename VL> struct mpi_u64;
template<int...II, typename VL> struct mpi_u64<index_list<II...>, VL> : static_ref_array<uint32, uint32(meta::VL_index<VL::count - 1 - II / 2, VL>::value >> ((II & 1) * 32))...> {};
template<uint64... args> struct mpi_const<uint64, args...> : mpi_u64<meta::make_index_list<sizeof...(args) * 2>, meta::value_list<uint64, args...> > {};

template<int I> struct mpi_const1 : static_ref_array<uint32, (I < 0 ? -I : I)>	{};

#endif

//-----------------------------------------------------------------------------
//	mpi
//-----------------------------------------------------------------------------

struct mpi : comparisons<mpi> {
	typedef uint32			element;
	typedef uint64			element2;
	enum {
		bytes_per_element	= sizeof(element),
		bits_per_element	= sizeof(element) * 8,
	};

protected:
	ref_array<element>	p;
	uint32				max_size;
	union {
		uint32	u;
		struct { uint32	size:31, sign:1; };
	};

	static inline uint32	nbits(uint32 n)		{ return div_round_up(n, bits_per_element);		}
	static inline uint32	nbytes(uint32 n)	{ return div_round_up(n, bytes_per_element);	}

	uint32	fix_size() {
		uint32	i	= size;
		element	*pp	= p;
		while (i && !pp[i - 1])
			--i;
		return size = i;
	}
	friend int 	compare(param(mpi) x, param(mpi) y) {
		int	c = x.sign == y.sign ? compare_abs(x, y) : 1;
		return x.sign ? -c : c;
	}
	friend int	compare(param(mpi) x, int y) {
		int	c = x.sign == int(y < 0) && x.size <= 1 ? (x.p ? int(*x.p) : 0) - abs(y) : 1;
		return x.sign ? -c : c;
	}
	friend int	compare(param(mpi) x, uint32 y) {
		return x.sign ? -1 : x.size <= 1 ? (x.p ? int(*x.p) : 0) - y : 1;
	}
	friend int	compare(param(mpi) x, const _zero&) {
		return x.sign ? -1 : x.size;
	}
	template<typename C> friend int	compare(param(mpi) x, const constant<C>& c) {
		return compare(x, int(c));
	}

	element	*unique();
	element	*unique(int n);

public:
	static inline mpi		error()				{ mpi e; e.sign = true; return e; }

	mpi()									: max_size(0), u(0)							{}
	mpi(const _zero&)						: max_size(0), u(0)							{}
	explicit mpi(int i)						: p(1), max_size(1), size(1), sign(i < 0)	{ p[0]	= abs(i); }
	explicit mpi(uint32 i)					: p(1), max_size(1), size(1), sign(0)		{ p[0]	= i; }
	mpi(uint32 n, bool sign)				: p(n), max_size(n), size(n), sign(sign)	{}// memset(p, 0, n * sizeof(element));
	mpi(const element *buffer, uint32 len)	: p(len), max_size(len), u(len)				{ memcpy(p, buffer, len * sizeof(element)); }
	mpi(const uint8 *buffer, uint32 len, bool bigendian = true)		: max_size(0), u(0)	{ load(buffer, len, bigendian); }
	mpi(const const_memory_block &buffer, bool bigendian = true)	: max_size(0), u(0)	{ load(buffer, buffer.size32(), bigendian); }
	mpi(const memory_block &buffer, bool bigendian = true)			: max_size(0), u(0)	{ load(buffer, buffer.size32(), bigendian); }
	template<typename T, int N>		mpi(const T (&t)[N])			: max_size(0), u(0)	{ load((const uint8*)&t, N * sizeof(T)); }
	template<int N>					mpi(const constant<__int<N> >&)	: p(mpi_const1<N>::x), max_size(1), size(1), sign(N < 0) {}
	template<int N>					mpi(const mpi_const1<N> &c)		: p(c.x), max_size(1), size(1), sign(N < 0) {}
	template<typename T, T... t>	mpi(const mpi_const<T, t...> &c): p(c.x), max_size(c.size), u(c.size) 		{}
	mpi(const char *s)																	{ get_signed_num(s, *this); }

	mpi&			load(const uint8 *buffer, uint32 len, bool bigendian = true);
	bool			save(uint8 *buffer, uint32 len, bool bigendian = true) const;
	uint32			save_all(uint8 *buffer, bool bigendian = true) const;
	const char*		read_base(const char *p, int B);

	mpi&			load(const const_memory_block &buffer, bool bigendian = true)	{ return load(buffer, buffer.size32(), bigendian); }
	bool			save(const memory_block &buffer, bool bigendian = true) const	{ return save(buffer, buffer.size32(), bigendian); }
	malloc_block	save_all(bool bigendian = true) const							{ malloc_block b(num_bytes()); save_all(b, bigendian); return b; }
	void			set(const element *buffer, uint32 len)							{ memcpy(unique(len), buffer, len * sizeof(element)); }
	void			reset()															{ p.clear(); u = 0; }
	void			grow(int n)														{ if (size < n) unique(n); }

	element*				elements_ptr()									{ return unique(); }
	const element*			elements_ptr()			const					{ return get(p); }
	range<element*>			elements()										{ return make_range_n(unique(), size); }
	range<const element*>	elements()				const					{ return make_range_n((const element*)get(p), size); }
	void	clear_elements(uint32 i, uint32 n)								{ memset(unique() + i, 0, n * sizeof(element)); }
	void	copy_elements(const mpi &b, uint32 d, uint32 s, uint32 n)		{ memcpy(unique() + d, b.p + s, n * sizeof(element)); }

	int		num_bits()			const	{ return highest_set_index(*this) + 1; }
	int		num_bytes()			const	{ return (highest_set_index(*this) + 8) / 8; }
	int		num_elements()		const	{ return size; }
	element	lowest_bits()		const	{ return p ? p[0] : 0; }
	element	&top_element()				{ return p[size - 1]; }
	element	top_element()		const	{ return p[size - 1]; }

	bool	test_bit(uint32 b)	const	{ uint32 i = b / bits_per_element; return i < size && (p[i] & bit<element>(b % bits_per_element)); }
	void	set_bit(uint32 b)			{ uint32 i = b / bits_per_element; unique(i + 1)[i] |= bit<element>(b % bits_per_element); }
	void	clear_bit(uint32 b)			{ uint32 i = b / bits_per_element; if (i < size) p[i] &= ~bit<element>(b % bits_per_element); }

	mpi&	operator<<=(int count);
	mpi&	operator>>=(int count);

	template<int N>		mpi& operator=(const constant<__int<N> >&)	{
		p		= mpi_const1<N>::x;
		sign	= N < 0;
		size	= max_size = 1;
		return *this;
	}
	mpi&	operator=(element b);
	mpi&	operator+=(element b);
	mpi&	operator-=(element b);
	mpi&	operator*=(element b);
	mpi&	operator/=(element b);
	mpi&	operator%=(element b);

	mpi&	operator+=(param(mpi) b);
	mpi&	operator-=(param(mpi) b);
	mpi&	operator*=(param(mpi) b);
	mpi&	operator/=(param(mpi) b);
	mpi&	operator%=(param(mpi) b);

	mpi&	operator&=(element b);
	mpi&	operator|=(element b);
	mpi&	operator^=(element b);

	mpi&	operator&=(param(mpi) b);
	mpi&	operator|=(param(mpi) b);
	mpi&	operator^=(param(mpi) b);

	mpi&	operator++()	{ return *this += 1; }
	mpi&	operator--()	{ return *this -= 1; }
	void	operator++(int) { *this += 1; }
	void	operator--(int) { *this -= 1; }

	bool	operator!()			const { return size == 0; }
	bool	is_odd()			const { return !!(lowest_bits() & 1); }
	bool	is_error()			const { return u == 0x80000000;	}

	friend mpi			operator-(param(mpi) a);
	friend mpi			operator<<(param(mpi) a, int count);
	friend mpi			operator>>(param(mpi) a, int count);

	friend mpi			operator+(param(mpi) a, element b);
	friend mpi			operator-(param(mpi) a, element b);
	friend mpi			operator*(param(mpi) a, element b);
	friend mpi			operator/(param(mpi) a, element b);
	friend element		operator%(param(mpi) a, element b);

	friend mpi			operator+(param(mpi) a, param(mpi) b);
	friend mpi			operator-(param(mpi) a, param(mpi) b);
	friend mpi			operator*(param(mpi) a, param(mpi) b);
	friend mpi			operator/(param(mpi) a, param(mpi) b);
	friend mpi			operator%(param(mpi) a, param(mpi) b);

	friend element		operator&(param(mpi) a, element b)	{ return a.p ? a.p[0] & b : 0; }
	friend mpi			operator|(param(mpi) a, element b)	{ return mpi(a) |= b;}
	friend mpi			operator^(param(mpi) a, element b)	{ return mpi(a) ^= b;}

	friend mpi			operator&(param(mpi) a, param(mpi) b);
	friend mpi			operator|(param(mpi) a, param(mpi) b);
	friend mpi			operator^(param(mpi) a, param(mpi) b);

	template<int N>	mpi operator+(const constant<__int<N> >&) { return *this + N; }
	template<int N>	mpi operator-(const constant<__int<N> >&) { return *this - N; }

	friend int		compare_abs(param(mpi) x, param(mpi) y) {
		return cmpn(get(x.p), x.size, get(y.p), y.size);
	}

	friend void		swap(mpi &a, mpi &b) {
		swap(a.p, b.p);
		swap(a.u, b.u);
	}

	friend int		lowest_set_index(param(mpi) x) {
		element	*p	= x.p;
		for (int i = 0, n = x.size; i < n; i++) {
			if (element v = p[i])
				return i * bits_per_element + lowest_set_index(v);
		}
		return -1;
	}
	friend int		lowest_clear_index(param(mpi) x) {
		element	*p	= x.p;
		for (int i = 0, n = x.size; i < n; i++) {
			if (element v = ~p[i])
				return i * bits_per_element + lowest_set_index(v);
		}
		return -1;
	}
	friend int		highest_set_index(param(mpi) x) {
		return x.size ? (x.size  - 1) * bits_per_element + highest_set_index(x.top_element()) : -1;
	}

	friend bool		get_sign(param(mpi) a)	{ return !!a.sign; }

	friend mpi		abs(param(mpi) a);
	friend mpi		divmod(mpi &a, mpi::element b);		// remainder in a
	friend mpi		divmod(mpi &a, param(mpi) b);		// remainder in a
	friend mpi		divmod(param(mpi) a, mpi::element b, mpi::element &mod) { mpi t	= a; mpi r = divmod(t, b); mod = t.lowest_bits(); return r; }
	friend mpi		divmod(param(mpi) a, param(mpi) b, mpi &mod)			{ return divmod(mod = a, b); }
	friend mpi		pow(param(mpi) a, param(mpi) b);
	friend mpi		gcd(param(mpi) a, param(mpi) b);
	friend mpi		square(param(mpi) a);
	friend mpi		sqrt(param(mpi) a);
	friend mpi		root_n(param(mpi) a, element b);
	friend bool		is_prime(param(mpi) a);
	friend int		jacobi(param(mpi) a, param(mpi) b);
	friend int		kronecker(param(mpi) a, param(mpi) b);

	friend mpi		inv_mod(param(mpi) a, param(mpi) n);
	friend mpi		exp_mod(param(mpi) a, param(mpi) b, param(mpi) p);
	friend mpi		exp_mod(param(mpi) a, param(mpi) b, param(mpi) p);
	friend mpi		exp_mod(param(mpi) a, param(mpi) e, param(mpi) p, mpi &rr);
	friend mpi		mul_mod(param(mpi) a, param(mpi) b, param(mpi) p)		{ return (a * b) % p; }
	friend mpi		square_mod(param(mpi) a, param(mpi) p)					{ return (a * a) % p; }

	// assume 0 < a, b < p
	friend mpi		adjust_mod(param(mpi) a, param(mpi) p)					{ return a < 0 ? a + p : a >= p ? a - p : a; }
	friend mpi		add_mod(param(mpi) a, param(mpi) b, param(mpi) p)		{ return adjust_mod(a + b, p); }
	friend mpi		sub_mod(param(mpi) a, param(mpi) b, param(mpi) p)		{ return adjust_mod(a - b, p); }

	template<typename R> static mpi		random(R &r, int bits);
	template<typename R> static mpi		random_to(R &r, param(mpi) range);
	static mpi		random(int bits);
	static mpi		random_to(param(mpi) range);
	static mpi		generate_prime(int bits);

	// string
	friend size_t						put_signed_num_base(int B, char *s, param(mpi) i, char ten);
	template<int B> friend size_t		put_signed_num_base(char *s, param(mpi) i, char ten = 'A') { return put_signed_num_base(B, s, i, ten); }
	friend string						_to_string_base(uint32 B, param(mpi) a);
	friend string						to_string(param(mpi) i)					{ return _to_string_base(10, i); }
	template<int B> friend string		to_string(const baseint<B,mpi> &v)		{ return _to_string_base(B, v.i); }
	template<int B> friend const char*	get_num_base(const char *p, mpi &i)		{ return i.read_base(p, B); }
	friend inline size_t				from_string(const char *s, mpi &i)		{ return get_signed_num(s, i) - s; }
};


template<typename R> mpi mpi::random(R &r, int bits) {
	mpi	i(nbits(bits), false);
//	r.fill(make_range_n(get(i.p), i.size));
	r.fill(range<uint8*>((uint8*)get(i.p), (uint8*)get(i.p) + (bits + 7) / 8));
	i.p[i.size - 1] &= iso::bits<element>(((bits - 1) & (bits_per_element - 1)) + 1);
	return i;
}

template<typename R> mpi mpi::random_to(R &r, param(mpi) range) {
	int	n = highest_set_index(range);
	mpi	i;

	if (!range.test_bit(n - 1) && !range.test_bit(n - 2)) {
		// range = 100..., so 3*range (= 11...) is exactly one bit longer than range
		do {
			i = random(r, n + 2);
			// if i < 3*range, use i := i MOD range (which is either i, i - range, or i - 2*range), else iterate
			if (i >= range) {
				i -= range;
				if (i >= range)
					i -= range;
			}
		} while (i >= range);
	} else {
		do {
			// range = 11... or 101...
			i = random(r, n + 1);
		} while (i >= range);
	}
	return i;
}

template<> struct num_traits<mpi> {
	static const bool	is_signed	= true;
	static const bool	is_float	= false;
};

struct mod_context {
	mpi				p;		// The modulus
	mod_context()	{}
	mod_context(param(mpi) _p)	: p(_p) {}
	void	init(param(mpi) _p)	{ p = _p; }
	mpi		check(param(mpi) a) const {
		ISO_ASSERT(a >= zero && a < p);
		return a;
	}

	mpi		mul(param(mpi) a, param(mpi) b)		const	{ return check(mul_mod(a, b, p)); }
	mpi		exp(param(mpi) a, param(mpi) b)		const	{ return check(exp_mod(a, b, p)); }

	mpi		add(param(mpi) a, param(mpi) b)		const	{ mpi t = a + b; if (t >= p) t -= p; return check(t); }
	mpi		twice(param(mpi) a)					const	{ return add(a, a); }
	mpi		sub(param(mpi) a, param(mpi) b)		const	{ mpi t = a - b; if (t < 0) t += p; return check(t); }
	mpi		neg(param(mpi) a)					const	{ return check(p - a); }
	mpi		div(param(mpi) a, param(mpi) b)		const	{ return mul(a, reciprocal(b)); }
	mpi		reciprocal(param(mpi) a)			const	{ return inv_mod(a, p); }
	mpi		square(param(mpi) a)				const	{ return mul(a, a); }
};

struct montgomery_context {
	mpi				p;		// The modulus
protected:
	mpi				rr;		// used to convert to montgomery form
	mpi::element	mm;
public:
	montgomery_context()	{}
	montgomery_context(param(mpi) _p)	{ init(_p); }
	void	init(param(mpi) _p);
	mpi		from(param(mpi) a)					const;
	mpi		mul(param(mpi) a, param(mpi) b)		const;
	mpi		exp(param(mpi) a, param(mpi) b)		const;
	mpi		sqrt(param(mpi) a)					const;

	mpi		to(param(mpi) a)					const	{ return mul(a, rr); }
	mpi		add(param(mpi) a, param(mpi) b)		const	{ mpi t = a + b; if (t >= p) t -= p; return t; }
	mpi		twice(param(mpi) a)					const	{ return add(a, a); }
	mpi		sub(param(mpi) a, param(mpi) b)		const	{ mpi t = a - b; if (t < 0) t += p; return t; }
	mpi		neg(param(mpi) a)					const	{ return p - a; }
	mpi		div(param(mpi) a, param(mpi) b)		const	{ return mul(a, reciprocal(b)); }
	mpi		reciprocal(param(mpi) a)			const	{ return inv_mod(a, p); }
	mpi		square(param(mpi) a)				const	{ return mul(a, a); }
};

//Miller-Rabin primality tester
struct prime_checker : montgomery_context {
	mpi		N1, N1_odd;
	int		k;

	//Number of Miller-Rabin iterations for an error rate of less than 2^-80 for random 'b'-bit input, b >= 100
	static constexpr int num_checks(int b) {
		return	b >= 1300 ?  2
			:	b >=  850 ?  3
			:	b >=  650 ?  4
			:	b >=  550 ?  5
			:	b >=  450 ?  6
			:	b >=  400 ?  7
			:	b >=  350 ?  8
			:	b >=  300 ?  9
			:	b >=  250 ? 12
			:	b >=  200 ? 15
			:	b >=  150 ? 18
			:/* b >=  100 */ 27;
	}

	prime_checker(param(mpi) a) : montgomery_context(abs(a)), N1(p - 1) {
		k = lowest_set_index(N1);
		N1_odd = N1 >> k;
	}

	bool	is_witness(param(mpi) x);
	template<typename R> bool check(R &r) { return is_witness(mpi::random_to(r, N1) + 1); }
};

//-----------------------------------------------------------------------------
//	mpf
//-----------------------------------------------------------------------------

class mpf : mpi, comparisons<mpf> {
	int		exp;
protected:

	friend int	compare_abs(param(mpf) x, param(mpf) y) {
		return x.exp > y.exp
			? compare_abs(x << (x.exp - y.exp), (const mpi&)y)
			: compare_abs((const mpi&)x, y << (y.exp - x.exp));
	}
	friend int 	compare(param(mpf) x, param(mpf) y) {
		int	c = x.sign == y.sign ? compare_abs(x, y) : 1;
		return x.sign ? -c : c;
	}
public:
	using mpi::bits_per_element;
	mpf()						: exp(0)				{}
	mpf(const _zero&)			: exp(0)				{}
	explicit mpf(int i)			: mpi(i), exp(0)		{}
	explicit mpf(uint32 i)		: mpi(i), exp(0)		{}
	explicit mpf(float f)		: mpi(1, f < 0)			{ iorf i(f); p[0] = element(i.m | (1 << 23)); exp = i.e - 127 - 23; }
	explicit mpf(double f)		: mpi(2, f < 0)			{ iord i(f); p[0] = element(i.m); p[1] = element((i.m >> 32) | (1 << (52-32))); exp = i.e - 1023 - 52; }
	mpf(param(mpi) b, int exp = 0)	: mpi(b), exp(exp)	{}

	mpf&			operator+=(const mpf &b)			{ return *this = *this + b; }
	mpf&			operator-=(const mpf &b)			{ return *this = *this - b; }
	mpf&			operator*=(const mpf &b)			{ return *this = *this * b; }
	mpf&			operator/=(const mpf &b)			{ return *this = *this / b; }

	friend mpf		operator-(const mpf &a);
	friend mpf		operator+(const mpf &a, const mpf &b);
	friend mpf		operator-(const mpf &a, const mpf &b);
	friend mpf		operator*(const mpf &a, const mpf &b);
	friend mpf		operator/(const mpf &a, const mpf &b);

	template<typename T> friend mpf	operator+(const mpf &a, const T &b)		{ return a + mpf(b); }
	template<typename T> friend mpf	operator-(const mpf &a, const T &b)		{ return a - mpf(b); }
	template<typename T> friend mpf	operator*(const mpf &a, const T &b)		{ return a * mpf(b); }
	template<typename T> friend mpf	operator/(const mpf &a, const T &b)		{ return a / mpf(b); }

	friend void		swap(mpf &a, mpf &b) {
		swap((mpi&)a, (mpi&)b);
		swap(a.exp, b.exp);
	}
	friend bool		get_sign(const mpf& a)	{ return !!a.sign; }

	friend mpf		abs(const mpf &a);
	friend mpi		trunc(const mpf &a);
	friend mpi		floor(const mpf &a);
	friend mpi		ceil(const mpf &a);
	friend mpi		round(const mpf &a);
	friend mpf		frac(const mpf &a);
	friend mpf		sfrac(const mpf &a);
	friend mpf		sqrt(const mpf &a);

	friend string	to_string(const mpf &a);
	friend size_t	from_string(const char *s, mpf &a);
};

//-----------------------------------------------------------------------------
//	string
//-----------------------------------------------------------------------------

inline auto		operator"" _mpi(const char* s)				{ mpi i; from_string(s, i); return i; }
inline auto		operator"" _mpf(const char* s)				{ mpf f; from_string(s, f); return f; }

} //namespace iso

#endif
