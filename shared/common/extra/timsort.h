#include "base/defs.h"
#include "base/algorithm.h"

// An adaptive, stable, natural mergesort

namespace iso {

enum {MIN_GALLOP = 7};

// Merge the elements [a, enda) with the elements [b, endb) in a stably, to dest
// Must have that enda[-1] belongs at the end of the merge

template<class D, class A, class B, class P> intptr_t gallop_merge(D dest, A a, A enda, B b, B endb, intptr_t min_gallop, P pred) {
	*dest++ = *b++;
	
	if (b == endb)
		goto done;
		
	if (a == enda - 1)
		goto copy_b;

	for (;;) {
		intptr_t counta = 0;	// # of times A won in a row
		intptr_t countb = 0;	// # of times B won in a row

		// straightforward merge until one run appears to win consistently
		for (;;) {
			if (pred(*b, *a)) {
				*dest++ = *b++;
				if (b == endb)
					goto done;
				counta = 0;
				if (++countb >= min_gallop)
					break;
			} else {
				*dest++ = *a++;
				if (a == enda - 1)
					goto copy_b;
				countb = 0;
				if (++counta >= min_gallop)
					break;
			}
		}

		// one run is winning so consistently that galloping may be a huge win
		// gallop until neither run appears to be winning consistently anymore
		do {
			A	pa	= gallop_find(*b, a, enda, a, !flip(pred));
			if (counta = pa - a) {
				copy(a, pa, dest);
				dest	+= counta;
				a		= pa;
				if (a == enda - 1)
					goto copy_b;
				// a == enda is impossible now if pred is consistent, but we can't assume that it is
				if (a == enda)
					goto done;
			}
			*dest++ = *b++;
			if (b == endb)
				goto done;

			B	pb	= gallop_find(*a, b, endb, b, pred);
			if (countb = pb - b) {
				copy_from_end(b, pb, dest);
				dest 	+= countb;
				b		= pb;
				if (b == endb)
					goto done;
			}
			*dest++ = *a++;
			if (a == enda - 1)
				goto copy_b;
				
			min_gallop -= min_gallop > 1;
				
		} while (counta >= MIN_GALLOP || countb >= MIN_GALLOP);
		
		min_gallop += 2;	// penalize it for leaving galloping mode
	}
done:
	copy(a, enda, dest);
	return min_gallop;

copy_b:
	// The last element of a belongs at the end of the merge
	copy_from_end(b, endb, dest);
	dest[endb - b] = *a;
	return min_gallop;
}

template<class I, class A, class B, class P> intptr_t gallop_merge(I dest, A &&a, B &&b, intptr_t min_gallop, P pred) {
	return gallop_merge(dest, a.begin(), a.end(), b.begin(), b.end(), min_gallop, pred);
}

template<class I, class P> size_t split_merge(I lo, I mid, I hi, P pred) {
	size_t	a	= 0;
	for (size_t n = min(mid - lo, hi - mid); n; n >>= 1) {
		size_t	c = a + (n >> 1);
		if (pred(*(mid - c), *(mid + c))) {
			a = c + 1;
			--n;
		}
	}
	
	swap_ranges(mid - a, mid, mid);
	return a;
}


template<class I, int N> struct tim_merger {
	dynamic_array<noref_t<decltype(*declval<I>())>, N>	temp;

	// Merge the two runs at [a, b) and [b, c)
	template<typename P> intptr_t merge(I a, I b, I c, intptr_t min_gallop, P pred) {
		// Where does b start in a? Elements in a before that can be ignored (already in place)
		a = gallop_find(b[0], a, b, a, !flip(pred));

		if (a < b) {
			// Where does a end in b? Elements in b after that can be ignored (already in place)
			c = gallop_find(b[-1], b, c, c - 1, pred);

			if (c > b) {
				// Merge what remains of the runs, using a temp array
				if (b - a <= c - b) {
					temp = make_range(a, b);
					return gallop_merge(
						a,
						temp, make_range(b, c),
						min_gallop, pred
					);
				} else {
					temp = make_range(b, c);
					return gallop_merge(
						make_reverse_iterator(c - 1),
						reversed(temp), reversed(make_range(a, b)),
						min_gallop, make_not(pred)
					);
				}
			}
		}
		return min_gallop;
	}

};

template<class I, typename P = less> void timsort(I begin, I end, P pred = P()) {
	intptr_t		size = end - begin;
				
	if (size < 64) {
		binary_sort(begin, end, pred);
		
	} else {
		const int 	s 		= highest_set_index(size) - 6;
		intptr_t 	minrun 	= (size >> s) + (size & bits64(s) ? 1 : 0);
		intptr_t 	min_gallop	= MIN_GALLOP;
		I			stack[85], *sp = stack;
		
		*sp	= begin;
		
		tim_merger<I, 256>	merger;
	
		// March over the array once, left to right, finding natural runs, and extending short natural runs to minrun elements
		for (I lo = begin, hi; lo != end; lo = hi) {
			// get next run
			hi = find_run(lo, end, pred);
			if (hi == lo + 1) {
				hi = find_run(lo, end, make_not(pred));
				reverse(lo, hi);
			}
			
			// extend to minrun
			if (hi - lo < minrun) {
				auto	unsorted = hi;
				hi = min(lo + minrun, end);
				binary_sort(lo, hi, unsorted, pred);
			}

			// Examine the stack of runs waiting to be merged, merging adjacent runs until the stack invariants are re-established:
			// 1. len[-3] > len[-2] + len[-1]
			// 2. len[-2] > len[-1]
			I	s0 = hi;
			while (sp > stack) {
				I	s1 = sp[0], s2 = sp[-1];
				
				if ((sp >= stack + 2 && s2 - sp[-2] <= s0 - s2)
				||	(sp >= stack + 3 && sp[-2] - sp[-3] <= s1 - sp[-2])
				) {
					if (s2 - sp[-2] < s0 - s1) {
						min_gallop = merger.merge(sp[-2], s2, s1, min_gallop, pred);
						sp[-1] = s1;
					} else {
						min_gallop = merger.merge(s2, s1, s0, min_gallop, pred);
					}
						
				} else if (s1 - s2 <= s0 - s1) {
					min_gallop = merger.merge(s2, s1, s0, min_gallop, pred);
						
				} else
					break;

				--sp;
			}
			// Push run end onto pending-runs stack
			*++sp = hi;
		}

		// Regardless of invariants, merge all runs on the stack until only one remains
		I	s0 = end;
		while (--sp > stack) {
			I	s1 = sp[0], s2 = sp[-1];
			if (sp >= stack + 2 && s2 - sp[-2] < s0 - s1) {
				min_gallop = merger.merge(sp[-2], s2, s1, min_gallop, pred);
				sp[-1] = s1;
			} else {
				min_gallop = merger.merge(s2, s1, s0, min_gallop, pred);
			}
		}
	}
}

template<class P, typename T> void timsort(T *begin, T *end) { timsort(begin, end, P()); }

} // namespace iso

#ifdef TEST
#include "base/array.h"
#include "utilities.h"

using namespace iso;

struct test_timsort {
	dynamic_array<int>	a;
	rng<simple_random> random;

	test_timsort() : random(42) {
		a = int_range(100);
		block_exchange2(a.begin(), a.begin() + 11, a.end());

	
		a = transformc(int_range(100000), [this](int) { return random.to(100000); });
		timsort(a.begin(), a.end());
//		a = transformc(int_range(1000), [this](int i) { return i / 10.f; });
//		timsort(a.begin(), a.end(), [](float a, float b) { return (int)a > int(b); });
//		binarysort<greater>(a.begin(), a.end(), a.begin());
		ISO_ASSERT(check_sorted(a.begin(), a.end()));
	}
	
} _test_timsort;
#endif
