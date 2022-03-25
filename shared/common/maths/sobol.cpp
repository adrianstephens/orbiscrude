#include "sobol.h"
#include "base/tuple.h"
#include "base/strings.h"
#include "base/simd.h"
#include "base/algorithm.h"
#include "utilities.h"

namespace iso {
//template<typename D, typename S> constexpr if_smaller_t<D,double_int<S>> shift_right(const double_int<S> &x, int y) {
//	return y < BIT_COUNT<S> ? shift_right<D, S>(x.lo, y) : shift_right<D, S>(x.hi, y - BIT_COUNT<S>);
//}
}

using namespace iso;

uint64 _Sobol::cjn[MAX_DIMS][63];

void _Sobol::GeneratePolynomials(uint32 *buffer, int dims, bool primitive) {
	uint32	p1	= buffer[0]	= 2;

	for (int n = 1; n < dims; ++n) {
		// search for the next irreducable polynomial
		for (;;) {
			++p1;
			gf2_poly<32>	a(p1);
			
			// try to divide p1 by all polynomials in buffer
			bool	irreducible	= true;
			for (int i = 0; i < n && irreducible; ++i)
				irreducible = !!(a % gf2_poly<32>(buffer[i]));

			if (irreducible && (!primitive || a.is_primitive()))
				break;
		}

		buffer[n] = p1;
	}
}

//  @misc{Bratley92:LDS,
//    author = "B. Fox and P. Bratley and H. Niederreiter",
//    title = "Implementation and test of low discrepancy sequences",
//    text = "B. L. Fox, P. Bratley, and H. Niederreiter. Implementation and test of low discrepancy sequences. ACM Trans. Model. Comput. Simul., 2(3):195--213, July 1992.",
//    year = "1992"
//	}

template<int N> void read_gf2(gf2_vec<N> &r, const uint8* p, int n) {
	r.clear();
	int		nc	= n / 64;
	auto	rp	= (uint64*)&r.u + nc;

	if (int n1 = n & 63) {
		uint64	i	= 0;
		while (n1--)
			i = (i << 1) | *p++;

		write_bytes(rp, i, ((n & 63) + 7) / 8);
	}

	while (nc--) {
		uint64	i	= 0;
		for (int n1 = 64; n1--;)
			i = (i << 1) | *p++;

		*--rp = i;
	}
}

void _Sobol::GenerateCJ() {
	uint32	buffer[MAX_DIMS];

	// Niederreiter (in contrast to Sobol) allows to use not primitive, but just irreducable polynomials
	GeneratePolynomials(buffer, MAX_DIMS, false);

	uint8 b_arr[1024];	// polynomial b
	uint8 v_arr[1024];	// v array
	uint8 t_arr[1024];	// temporary polynomial, required to do multiplication of p and b

	int		max_m	= 0;
	// cycle over (maybe monic) irreducible polynomials
	for (int d = 0; d != MAX_DIMS; ++d) {
		clear(cjn[d]);

		uint8	p[32];
		uint32	p1	= buffer[d];

		const uint32	e		= highest_set_index(p1);

		// fill polynomials table with values for this polynomial
		for (int i = e; i >= 0; --i, p1 >>= 1)
			p[i] = p1 & 1;

		//for (int i = 0; i <= e; i++, p1 <<= 1)
		//	p[i] = (p1 >> e) & 1;

		// polynomial b in the beginning is just '1'
		uint8	*b	= b_arr + 1023;
		int		m	= 0;
		b[0]	= 1;

		// v array needs only (63 + e - 2) length
		uint8	*v = v_arr + 1023 - (63 + e - 2);

		// cycle over all coefficients
		for (int j = 63, u = e; j--; ++u) {
			if (u == e) {
				u = 0;
				// b = b * p (polynomials multiplication)

				gf2_poly<1024>	b2;
				gf2_poly<32>	p2;
				read_gf2(b2, b, m + 1);
				read_gf2(p2, p, e + 1);
				auto	b3	= b2 * p2;

				bool	x = b2[0];

				// t = b
				uint8	*t = t_arr + 1023 - m;
				for (int i = 0; i <= m; ++i)
					t[i] = b[i];

				int	m1 = m;
				m += e;
				max_m	= max(max_m, m);
				b = b_arr + 1023 - m;
				for (int i = 0; i <= m; ++i) {
					b[i] = 0;
					for (int ip = e - (m - i), it = m1; ip <= e && it >= 0; ++ip, --it) {
						if (ip >= 0)
							b[i] ^= p[ip] & t[it];
					}
				}

				gf2_poly<1024>	b4;
				read_gf2(b4, b, m + 1);
//				ISO_ASSERT(b4 == b3);
				// multiplication of polynomials finished

				// calculate v
				for (int i = 0; i < m1; ++i)
					v[i] = 0;

				for (int i = m1; i < m; ++i)
					v[i] = 1;

				for (int i = m; i <= 63 + e - 2; ++i) {
					v[i] = 0;
					for (int it = 1; it <= m; ++it)
						v[i] ^= v[i - it] & b[it];
				}
			}

			//cjn[d] += v << j

			// copy calculated v to cj
			for (int i = 0; i < 63; ++i)
				cjn[d][i] |= (int64)v[i + u] << j;
		}
	}
}

template<size_t I, typename T> struct union_entry {
	uint8	i;
	T		t;
	template<typename A>	T&	operator=(A &&a)	{ i = I; return t = forward<A>(a); }
	operator T()	const	{ ISO_ASSERT(i == I); return t; }
};


union tagged_union {
	union_entry<0, int>		my_int;
	union_entry<1, float>	my_float;
};

typedef uint_bits_t<1024>	uint1024;
uint1024	x;

no_inline size_t my_strlen(const char* p) {
	return strlen(p);
}
no_inline size_t my_strlen(const char16* p) {
	return wcslen(p);
}

template<int B, int S, int I> constexpr uint64 digits_entry() {
	const uint64 smallest	= klog_ceil<B, kbit<uint64, I>>;
	const uint64 biggest	= klog_ceil<B, kbit<uint64, I + 1>>;
//	return (I < 60 ? (1ull << S) - kpow<B, smallest> : 0) + (smallest << S);
	return (smallest == biggest ? 0 : (kbit<uint64, S> - (kpow<B, smallest> & kbits<uint64, S>))) + (smallest << S);
}
template<int B, int S, int...I> constexpr meta::array<uint64, sizeof...(I)> digits_table(meta::value_list<int, I...>) {
	return {digits_entry<B, S, I>()...};
}

template<int B> int digit_count(uint32 x) {
	static constexpr auto table = digits_table<B,32>(meta::make_value_sequence<32, int>());
	return (x + table[highest_set_index(x)]) >> 32;
}

template<int B> int digit_count(uint64 x) {
	static constexpr auto table = digits_table<B,59>(meta::make_value_sequence<64, int>());
	return ((x & kbits<uint64, 59>) + table[highest_set_index(x)]) >> 59;
}

static struct test {
	test() {
#if 0
		for (uint64 i = kbit<uint64, 59>; i; i += kbit<uint64, 55>) {
			auto	j = digit_count<10>(i);
			ISO_OUTPUTF("i=") << i << " has " << j << '\n';
		}

		int	t10 = digit_count<10>(12345u);
		int	t16 = digit_count<16>(12345u);

		ISO_OUTPUTF("x=") << my_strlen("hello goodbye whatever");
		ISO_OUTPUTF("x=") << my_strlen(L"hello goodbye whatever");
#endif
		variant<int, float, string, int>	v(none);
		auto	v2 = v;

		bool	same = v == v2;

		v.emplace<0>(1);
		v.emplace<3>(2);
		auto	vi	= v.get<int>();
		vi	= v.get<3>();
		auto	vf	= v.get<float>();
		//v.get<float>() = 42;
		v = 1.f;
		v = "hello";
		//auto	vd	= v.get<double>();

		v2	= v;
		v = none;
		Sobol<2>	sobol;
	}
} _test;
