#include "oodle.h"
#include "codec/window.h"
#include "base/bits.h"
#include "utilities.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Bitknit
//-----------------------------------------------------------------------------

struct BitknitBitReader {
	const uint16* src;
	uint32 bits, bits2;

	BitknitBitReader(const uint16* p) {
		uint32	a = *(uint32*)p;
		p		+= 2;
		ISO_ASSERT(a >= 0x10000);

		uint32	n = a & 0xF;
		a		>>= 4;
		if (a < 0x10000)
			a = (a << 16) | *p++;

		bits = a >> n;
		if (bits < 0x10000)
			bits = (bits << 16) | *p++;

		a		= (a << 16) | *p++;
		src		= p;
		bits2	= iso::bit(n + 16) | (a & iso::bits(n + 16));
	}

	void	renormalize() {
		if (bits < 0x10000)
			bits = (bits << 16) | *src++;
		swap(bits, bits2);
	}

	uint32	get(int n) {
		uint32	r = bits & iso::bits(n);
		bits >>= n;
		renormalize();
		return r;
	}

	void	adjust(uint16 a0, uint16 a1) {
		bits = (bits & 0x7fff) + (bits >> 15) * (a1 - a0) - a0;
		renormalize();
	}

	uint32	raw16() {
		return *src++;
	}
};

template<int L, int A, int S> struct BitknitT {
	uint16 lookup[L + 4];
	uint16 a[A + 1];
	uint16 freq[A];
	uint32 adapt_interval;

	void init_lookup() {
		adapt_interval = 1024;

		uint16* p = lookup;
		for (int i = 0; i < A; i++) {
			freq[i]		= 1;

			uint16* p_end = &lookup[(a[i + 1] - 1) >> S];
			do {
				p[0] = p[1] = p[2] = p[3] = i;
				p += 4;
			} while (p <= p_end);
			p = p_end + 1;
		}
	}

	void adaptive(uint32 sym) {
		freq[sym]		+= 1024 - A + 1;

		uint32 sum = 0;
		for (int i = 0; i < A; i++) {
			sum			+= freq[i];
			a[i + 1]	+= (sum - a[i + 1]) >> 1;
		}
		init_lookup();
	}

	uint32 dolookup(BitknitBitReader& bits) {
		uint32 masked	= bits.bits & 0x7FFF;
		size_t sym		= lookup[masked >> S];

		sym += masked > a[sym + 1];
		while (masked >= a[sym + 1])
			++sym;

		bits.adjust(a[sym], a[sym + 1]);

		freq[sym] += 31;
		if (--adapt_interval == 0)
			adaptive(sym);
		return sym;
	}
};

struct BitknitState {
	struct Literal : BitknitT<512, 300, 6> {
		Literal() {
			for (int i = 0; i < 264; i++)
				a[i] = (0x8000 - 300 + 264) * i / 264;
			for (int i = 264; i <= 300; i++)
				a[i] = (0x8000 - 300) + i;
			init_lookup();
		}
	};
	struct DistanceLsb : BitknitT<64, 40, 9> {
		DistanceLsb() {
			for (int i = 0; i <= 40; i++)
				a[i] = 0x8000 * i / 40;
			init_lookup();
		}
	};
	struct DistanceBits : BitknitT<64, 21, 9> {
		DistanceBits() {
			for (int i = 0; i <= 21; i++)
				a[i] = 0x8000 * i / 21;
			init_lookup();
		}
	};

	Literal			literals[4];
	DistanceLsb		distance_lsb[4];
	DistanceBits	distance_bits;

	uint32 recent_dist[8]	= {1,1,1,1,1,1,1,1};
	uint32 last_match_dist	= 1;
	uint32 recent_dist_mask	=
		(7 << (7 * 3)) | (6 << (6 * 3)) |
		(5 << (5 * 3)) | (4 << (4 * 3)) |
		(3 << (3 * 3)) | (2 << (2 * 3)) |
		(1 << (1 * 3)) | (0 << (0 * 3));

	size_t DecodeQuantum(const uint8* src, const uint8* src_end, uint8* dst, uint8* dst_end, uint8* dst_start);
};

size_t BitknitState::DecodeQuantum(const uint8* src, const uint8* src_end, uint8* dst, uint8* dst_end, uint8* dst_start) {
	Literal*		literals[4];
	DistanceLsb*	distance_lsb[4];

	for (int i = 0; i < 4; i++) {
		int	j = (i - (intptr_t)dst_start) & 3;
		literals[i]		= &this->literals[j];
		distance_lsb[i]	= &this->distance_lsb[j];
	}

	BitknitBitReader	bits((uint16*)src);
	
	uint32		recent_dist_mask	= this->recent_dist_mask;
	uint32		match_dist			= this->last_match_dist;

	if (dst == dst_start)
		*dst++ = bits.get(8);

	while (dst + 4 < dst_end) {
		uint32 sym = literals[(intptr_t)dst & 3]->dolookup(bits);

		if (sym < 256) {
			*dst = sym + *(dst - match_dist);
			++dst;

			if (dst + 4 >= dst_end)
				break;

			sym = literals[(intptr_t)dst & 3]->dolookup(bits);

			if (sym < 256) {
				*dst = sym + *(dst - match_dist);
				++dst;
				continue;
			}
		}

		if (sym >= 288) {
			uint32 nb = sym - 287;
			sym = bits.get(nb) + iso::bit(nb) + 286;
		}

		uint32	copy_length = sym - 254;

		sym = distance_lsb[(intptr_t)dst & 3]->dolookup(bits);

		if (sym >= 8) {
			uint32 nb = distance_bits.dolookup(bits);

			match_dist = bits.get(nb & 0xf);
			if (nb >= 0x10)
				match_dist = (match_dist << 16) | bits.raw16();

			match_dist = (32 << nb) + (match_dist << 5) + sym - 39;
			recent_dist[(recent_dist_mask >> 21) & 7] = exchange(recent_dist[(recent_dist_mask >> 18) & 7], match_dist);

		} else {
			size_t idx	= (recent_dist_mask >> (3 * sym)) & 7;
			uint32 mask	= ~7 << (3 * sym);
			match_dist			= recent_dist[idx];
			recent_dist_mask	= (recent_dist_mask & mask) | (idx + 8 * recent_dist_mask) & ~mask;
		}

		window::clipped_copy(dst, dst - match_dist, dst + copy_length, dst_end);

		dst					+= copy_length;
	}
	*(uint32*)dst = (uint16)bits.bits | bits.bits2 << 16;

	this->last_match_dist	= match_dist;
	this->recent_dist_mask	= recent_dist_mask;
	return (uint8*)bits.src - src;
}

//-----------------------------------------------------------------------------
//	LZNA
//-----------------------------------------------------------------------------

// Complete LZNA state
struct LznaState {

	struct BitReader {
		uint64			bits_a, bits_b;
		const uint32	*src;

		// Initialize bit reader with 2 parallel streams. Every decode operation swaps the two streams.
		BitReader(const uint8* p) {
			int		d = *p++;
			int		n = d >> 4;
			uint64	v = 0;
			ISO_ASSERT(n <= 8);
			for (int i = 0; i < n; i++)
				v = (v << 8) | *p++;
			bits_a = (v << 4) | (d & 0xF);

			d	= *p++;
			n	= d >> 4;
			v	= 0;
			ISO_ASSERT(n <= 8);
			for (int i = 0; i < n; i++)
				v = (v << 8) | *p++;
			bits_b = (v << 4) | (d & 0xF);

			src = (uint32*)p;
		}

		// renormalize by filling up the RANS state and swapping the two streams
		void renormalize() {
			uint64 x = bits_a;
			if (x < 0x80000000)
				x = (x << 32) | *src++;
			bits_a = bits_b;
			bits_b = x;
		}

		// Read a single bit with a uniform distribution
		uint32 bit() {
			uint32 r = bits_a & 1;
			bits_a >>= 1;
			renormalize();
			return r;
		}

		// Read a number of bits with a uniform distribution
		uint32 get(int n) {
			uint32 r = bits_a & ((1 << n) - 1);
			bits_a >>= n;
			renormalize();
			return r;
		}

	};

	struct BitModel {
		uint16	v = 0x2000;

		uint32 read(BitReader &tab, int nbits, int shift) {
			int		magn	= 1 << nbits;
			uint64	q		= v * (tab.bits_a >> nbits);
			if ((tab.bits_a & (magn - 1)) >= v) {
				tab.bits_a	-= q + v;
				v = v - (v >> shift);
				tab.renormalize();
				return 1;
			} else {
				tab.bits_a	= (tab.bits_a & (magn - 1)) + q;
				v = v + ((magn - v) >> shift);
				tab.renormalize();
				return 0;
			}
		}
	};

	// State for a 4-bit value RANS model
	struct NibbleModel {
		uint16 prob[17] = {
			0x0, 0x800, 0x1000, 0x1800, 0x2000, 0x2800, 0x3000, 0x3800, 0x4000, 0x4800, 0x5000, 0x5800, 0x6000, 0x6800, 0x7000, 0x7800, 0x8000,
		};

		uint32 read(BitReader &tab) {
			uint64	x	= tab.bits_a;
			__m128i	t0	= _mm_loadu_si128((const __m128i*)&prob[0]);
			__m128i	t1	= _mm_loadu_si128((const __m128i*)&prob[8]);
			__m128i	t	= _mm_cvtsi32_si128((int16)x);
			t = _mm_and_si128(_mm_shuffle_epi32(_mm_unpacklo_epi16(t, t), 0), _mm_set1_epi16(0x7FFF));
			__m128i	c0	= _mm_cmpgt_epi16(t0, t);
			__m128i	c1	= _mm_cmpgt_epi16(t1, t);

			uint32	bitindex = lowest_set_index(_mm_movemask_epi8(_mm_packs_epi16(c0, c1)) | 0x10000);
			uint32	start	= prob[bitindex - 1];
			uint32	end		= prob[bitindex];

			c0 = _mm_and_si128(_mm_set1_epi16(0x7FD9), c0);
			c1 = _mm_and_si128(_mm_set1_epi16(0x7FD9), c1);

			c0 = _mm_add_epi16(c0, _mm_set_epi16(56, 48, 40, 32, 24, 16, 8, 0));
			c1 = _mm_add_epi16(c1, _mm_set_epi16(120, 112, 104, 96, 88, 80, 72, 64));

			t0 = _mm_add_epi16(_mm_srai_epi16(_mm_sub_epi16(c0, t0), 7), t0);
			t1 = _mm_add_epi16(_mm_srai_epi16(_mm_sub_epi16(c1, t1), 7), t1);

			_mm_storeu_si128((__m128i*)&prob[0], t0);
			_mm_storeu_si128((__m128i*)&prob[8], t1);

			tab.bits_a = (end - start) * (x >> 15) + (x & 0x7FFF) - start;
			tab.renormalize();
			return bitindex - 1;
		}
	};

	// State for a 3-bit value RANS model
	struct TriModel {
		uint16 prob[9] = {
			0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000
		};

		uint32 read(BitReader &tab) {
			uint64	x	= tab.bits_a;
			__m128i	t0	= _mm_loadu_si128((const __m128i*) & prob[0]);
			__m128i	t	= _mm_cvtsi32_si128(x & 0x7FFF);
			t = _mm_shuffle_epi32(_mm_unpacklo_epi16(t, t), 0);
			__m128i	c0	= _mm_cmpgt_epi16(t0, t);

			uint32	bitindex = lowest_set_index(_mm_movemask_epi8(c0) | 0x10000) >> 1;
			uint32	start	= prob[bitindex - 1];
			uint32	end		= prob[bitindex];

			c0 = _mm_and_si128(_mm_set1_epi16(0x7FE5), c0);
			c0 = _mm_add_epi16(c0, _mm_set_epi16(56, 48, 40, 32, 24, 16, 8, 0));
			t0 = _mm_add_epi16(_mm_srai_epi16(_mm_sub_epi16(c0, t0), 7), t0);
			_mm_storeu_si128((__m128i*) & prob[0], t0);

			tab.bits_a = (end - start) * (x >> 15) + (x & 0x7FFF) - start;
			tab.renormalize();
			return bitindex - 1;
		}
	};

	// State for the literal model
	struct LiteralModel {
		NibbleModel		upper[16];
		NibbleModel		lower[16];
		NibbleModel		nomatch[16];

		uint32	read(BitReader& tab, uint8 match_val) {
			uint32	x = upper[match_val >> 4].read(tab);
			return (x << 4) + ((match_val >> 4) != x ? nomatch[x] : lower[match_val & 0xF]).read(tab);
		}
	};

	// State for model representing the low bits of a distance
	struct LowBitsDistModel {
		NibbleModel		d[2];
		BitModel		v;

		uint32	read(BitReader& tab) {
			uint32 low_bit		= v.read(tab, 14, 6);
			uint32 low_nibble	= d[low_bit].read(tab);
			return low_bit + (2 * low_nibble) + 1;
		}
	};

	// State for a model representing a far distance
	struct FarDistModel {
		NibbleModel		first_lo, first_hi;
		BitModel		second[31];
		BitModel		third[2][30];

		uint32	read(BitReader& tab, LowBitsDistModel *low) {
			uint32 n	= first_lo.read(tab);
			if (n == 15)
				n += first_hi.read(tab);

			uint32 hi	= 0;
			if (n > 0) {
				hi = second[n - 1].read(tab, 14, 6) + 2;
				if (n > 1) {
					hi = (hi << 1) + third[hi - 2][n - 2].read(tab, 14, 6);
					if (n > 2)
						hi = (hi << (n - 2)) + tab.get(n - 2);
				}
				hi -= 1;
			}
			return low[hi == 0].read(tab) + hi * 32;
		}
	};

	// State for a model representing a near distance
	struct NearDistModel {
		NibbleModel		first;
		BitModel		second[16];
		BitModel		third[2][14];

		uint32	read(BitReader& tab, LowBitsDistModel *low) {
			uint32 n	= first.read(tab);
			uint32 hi	= 0;
			if (n > 0) {
				hi = second[n - 1].read(tab, 14, 6) + 2;
				if (n > 1) {
					hi = (hi << 1) + third[hi - 2][n - 2].read(tab, 14, 6);
					if (n > 2)
						hi = (hi << (n - 2)) + tab.get(n - 2);
				}
				hi -= 1;
			}
			return low[hi == 0].read(tab) + hi * 32;
		}
	};

	// State for model for long lengths
	struct LongLengthModel {
		NibbleModel		first[4];
		NibbleModel		second;
		NibbleModel		third;

		// Read a length using the length model.
		uint32 read(BitReader &tab, int64 dst_offs) {
			uint32 length = first[dst_offs & 3].read(tab);
			if (length >= 12) {
				uint32 b = second.read(tab);
				if (b == 15)
					b += third.read(tab);

				uint32 n	= 0;
				uint32 base	= 0;
				if (b) {
					n		= (b - 1) >> 1;
					base	= ((((b - 1) & 1) + 2) << n) - 1;
				}
				length += (tab.get(n) + base) * 4;
			}
			return length;
		}
	};

	uint32				match_history[8];
	LiteralModel		literal[4];
	BitModel			is_literal[12 * 8] = {0x1000};
	NibbleModel			type[12 * 8];
	TriModel			short_length_recent[4][4];
	LongLengthModel		long_length_recent;
	LowBitsDistModel	low_bits_of_distance[2];
	BitModel			short_length[12][4];
	NearDistModel		near_dist[2];
	TriModel			medium_length;
	LongLengthModel		long_length;
	FarDistModel		far_distance;

	LznaState() {
		for (int i = 0; i < 4; i++)
			match_history[i + 4] = 1;

		for (auto &i : is_literal)
			i.v = 0x1000;
	}

	void	PreprocessMatchHistory();
	int		DecodeQuantum(uint8* dst, uint8* dst_end, uint8* dst_start, const uint8* src_in, const uint8* src_end);
};

static void LznaCopy4to12(uint8* dst, size_t dist, size_t length) {
	const uint8* src = dst - dist;
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
	if (length > 4) {
		dst[4] = src[4];
		dst[5] = src[5];
		dst[6] = src[6];
		dst[7] = src[7];
		if (length > 8) {
			dst[8] = src[8];
			dst[9] = src[9];
			dst[10] = src[10];
			dst[11] = src[11];
		}
	}
}

void LznaState::PreprocessMatchHistory() {
	if (match_history[4] >= 0xc000) {
		size_t i = 0;
		while (match_history[4 + i] >= 0xC000) {
			++i;
			if (i >= 4) {
				match_history[7] = match_history[6];
				match_history[6] = match_history[5];
				match_history[5] = match_history[4];
				match_history[4] = 4;
				return;
			}
		}
		uint32 t = match_history[i + 4];
		match_history[i + 4] = match_history[i + 3];
		match_history[i + 3] = match_history[i + 2];
		match_history[i + 2] = match_history[i + 1];
		match_history[4] = t;
	}
}

int LznaState::DecodeQuantum(uint8* dst, uint8* dst_end, uint8* dst_start, const uint8* src_in, const uint8* src_end) {
	static const uint8 next_state_lit[12] = {0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5};
	
	PreprocessMatchHistory();

	BitReader	tab(src_in);
	uint32		dist	= match_history[4];
	uint32		state	= 5;
	dst_end -= 8;

	if (dst == dst_start)
		*dst++	= tab.bit() ? 0 : literal[0].read(tab, 0);

	while (dst < dst_end) {
		uint32	dst_offs	= dst - dst_start;
		uint8	match_val	= *(dst - dist);
		uint32	length;

		if (is_literal[(dst_offs & 7) + 8 * state].read(tab, 13, 5)) {
			uint32	x = type[(dst_offs & 7) + 8 * state].read(tab);
			if (x == 0) {
				// Copy 1 uint8 from most recent distance
				*dst++	= match_val;
				state	= (state >= 7) ? 11 : 9;

			} else if (x < 4) {
				if (x == 1) {
					// Copy count 3-4
					length = 3 + short_length[state][dst_offs & 3].read(tab, 14, 4);
					dist = near_dist[length - 3].read(tab, low_bits_of_distance);
					dst[0] = (dst - dist)[0];
					dst[1] = (dst - dist)[1];
					dst[2] = (dst - dist)[2];
					dst[3] = (dst - dist)[3];

				} else if (x == 2) {
					// Copy count 5-12
					length = 5 + medium_length.read(tab);
					dist = far_distance.read(tab, low_bits_of_distance);
					if (dist >= 8) {
						((uint64*)dst)[0] = ((uint64*)(dst - dist))[0];
						((uint64*)dst)[1] = ((uint64*)(dst - dist))[1];
					} else {
						LznaCopy4to12(dst, dist, length);
					}

				} else {
					// Copy count 13-
					length = long_length.read(tab, dst_offs) + 13;
					dist = far_distance.read(tab, low_bits_of_distance);
					window::clipped_copy(dst, dst - dist, dst + length, dst_end);
				}
				state	= state >= 7 ? 10 : 7;
				match_history[7] = match_history[6];
				match_history[6] = match_history[5];
				match_history[5] = match_history[4];
				match_history[4] = dist;
				dst		+= length;

			} else if (x >= 12) {
				// Copy 2 bytes from a recent distance
				size_t idx = x - 12;
				dist	= match_history[4 + idx];
				match_history[4 + idx] = match_history[3 + idx];
				match_history[3 + idx] = match_history[2 + idx];
				match_history[2 + idx] = match_history[1 + idx];
				match_history[4] = dist;
				dst[0]	= *(dst - dist + 0);
				dst[1]	= *(dst - dist + 1);
				state	= (state >= 7) ? 11 : 8;
				dst		+= 2;

			} else {
				size_t idx = (x - 4) >> 1;
				dist = match_history[4 + idx];
				match_history[4 + idx] = match_history[3 + idx];
				match_history[3 + idx] = match_history[2 + idx];
				match_history[2 + idx] = match_history[1 + idx];
				match_history[4] = dist;
				if (x & 1) {
					// Copy 11- bytes from recent distance
					length = 11 + long_length_recent.read(tab, dst_offs);
					window::clipped_copy(dst, dst - dist, dst + length, dst_end);

				} else {
					// Copy 3-10 bytes from recent distance
					length = 3 + short_length_recent[idx][dst_offs & 3].read(tab);
					if (dist >= 8) {
						((uint64*)dst)[0] = ((uint64*)(dst - dist))[0];
						((uint64*)dst)[1] = ((uint64*)(dst - dist))[1];
					} else {
						LznaCopy4to12(dst, dist, length);
					}
				}
				state	= (state >= 7) ? 11 : 8;
				dst		+= length;
			}
		} else {
			// Output a literal
			*dst++		= literal[dst_offs & 3].read(tab, match_val);
			state		= next_state_lit[state];
		}
	}

	if (dst != dst_end)
		return -1;

	*(uint64*)dst = (uint32)tab.bits_a | (tab.bits_b << 32);
	return (uint8*)tab.src - src_in;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

struct BitReader {
	const uint8	*p;
	const uint8	*p_end;
	uint32		bits;
	int			bitpos;		// Next uint8 will end up in the |bitpos| position in |bits|

	operator bit_address() const {
		return bit_address(p) + bitpos - 24;
	}

	void operator=(bit_address b) {
		// Reset the bits decoder.
		bitpos		= 24;
		p			= b.ptr<uint8>();
		bits		= 0;
		Refill();
		bits		<<= b.shift<uint8>();
		bitpos		+= b.shift<uint8>();
	}

	// Read more bytes to make sure we always have at least 24 bits in |bits|
	void Refill() {
		ISO_ASSERT(bitpos <= 24);
		while (bitpos > 0) {
			bits |= (p < p_end ? *p : 0) << bitpos;
			bitpos -= 8;
			p++;
		}
	}
	// Read more bytes to make sure we always have at least 24 bits in |bits| when reading backwards
	void RefillBackwards() {
		ISO_ASSERT(bitpos <= 24);
		while (bitpos > 0) {
			p--;
			bits |= (p >= p_end ? *p : 0) << bitpos;
			bitpos -= 8;
		}
	}
	// Refill bits then read a single bit
	int bit() {
		Refill();
		int		r = bits >> 31;
		bits	<<= 1;
		bitpos	+= 1;
		return r;
	}
	int bit_norefill() {
		int		r = bits >> 31;
		bits	<<= 1;
		bitpos	+= 1;
		return r;
	}
	// Read |n| bits without refilling.
	int get_norefill(int n) {
		int		r = bits >> (32 - n);
		bits	<<= n;
		bitpos	+= n;
		return r;
	}
	// Read |n| bits without refilling, n may be zero.
	int get_norefill_zero(int n) {
		int		r = (bits >> 1 >> (31 - n));
		bits	<<= n;
		bitpos	+= n;
		return r;
	}
	uint32 get(int n) {
		uint32 r;
		if (n <= 24) {
			r = get_norefill_zero(n);
		} else {
			r = get_norefill(24) << (n - 24);
			Refill();
			r += get_norefill(n - 24);
		}
		Refill();
		return r;
	}
	uint32 get_backwards(int n) {
		uint32 r;
		if (n <= 24) {
			r = get_norefill_zero(n);
		} else {
			r = get_norefill(24) << (n - 24);
			RefillBackwards();
			r += get_norefill(n - 24);
		}
		RefillBackwards();
		return r;
	}
};

// Reads a gamma value.
// Assumes bitreader is already filled with at least 23 bits
int ReadGamma(BitReader& bits) {
	int n = leading_zeros(bits.bits) * 2 + 2;
	ISO_ASSERT(n < 24);
	return bits.get_norefill(n) - 2;
}

// Reads a gamma value with |forced| number of forced bits.
int ReadGammaX(BitReader& bits, int forced) {
	if (bits.bits != 0) {
		int lz = leading_zeros(bits.bits);
		ISO_ASSERT(lz < 24);
		return bits.get_norefill(lz + forced + 1) + ((lz - 1) << forced);
	}
	return 0;
}

// Reads a offset code parametrized by |v|.
uint32 ReadDistance(BitReader& bits, uint32 v) {
	uint32 rv;
	if (v < 0xF0) {
		uint32	n = (v >> 4) + 4;
		uint32	w = rotate_left(bits.bits | 1, n);
		bits.bitpos += n;
		uint32	m = (2 << n) - 1;
		bits.bits = w & ~m;
		rv = ((w & m) << 4) + (v & 0xF) - 248;
	} else {
		uint32	n = v - 0xF0 + 4;
		uint32	w = rotate_left(bits.bits | 1, n);
		bits.bitpos += n;
		uint32	m = (2 << n) - 1;
		bits.bits = w & ~m;
		rv = 8322816 + ((w & m) << 12);
		bits.Refill();
		rv += (bits.bits >> 20);
		bits.bitpos += 12;
		bits.bits <<= 12;
	}
	bits.Refill();
	return rv;
}


// Reads a offset code parametrized by |v|, backwards.
uint32 ReadDistanceB(BitReader& bits, uint32 v) {
	uint32 w, m, n, rv;
	if (v < 0xF0) {
		n = (v >> 4) + 4;
		w = rotate_left(bits.bits | 1, n);
		bits.bitpos += n;
		m = (2 << n) - 1;
		bits.bits = w & ~m;
		rv = ((w & m) << 4) + (v & 0xF) - 248;
	} else {
		n = v - 0xF0 + 4;
		w = rotate_left(bits.bits | 1, n);
		bits.bitpos += n;
		m = (2 << n) - 1;
		bits.bits = w & ~m;
		rv = 8322816 + ((w & m) << 12);
		bits.RefillBackwards();
		rv += (bits.bits >> (32 - 12));
		bits.bitpos += 12;
		bits.bits <<= 12;
	}
	bits.RefillBackwards();
	return rv;
}

// Reads a length code.
bool ReadLength(BitReader& bits, uint32* v) {
	unsigned long bitresult;
	uint32 rv;
	int	n = leading_zeros(bits.bits);
	if (n > 12)
		return false;
	bits.bitpos += n;
	bits.bits <<= n;
	bits.Refill();
	n += 7;
	bits.bitpos += n;
	rv = (bits.bits >> (32 - n)) - 64;
	bits.bits <<= n;
	*v = rv;
	bits.Refill();
	return true;
}

// Reads a length code, backwards.
bool ReadLengthB(BitReader& bits, uint32* v) {
	unsigned long bitresult;
	uint32 rv;
	int	n = leading_zeros(bits.bits);
	if (n > 12)
		return false;
	bits.bitpos += n;
	bits.bits <<= n;
	bits.RefillBackwards();
	n += 7;
	bits.bitpos += n;
	rv = (bits.bits >> (32 - n)) - 64;
	bits.bits <<= n;
	*v = rv;
	bits.RefillBackwards();
	return true;
}

int Log2RoundUp(uint32 v) {
	return highest_set_index(v - 1) + 1;
}

#define ALIGN_16(x) (((x)+15)&~15)
#define COPY_64(d, s) {*(uint64*)(d) = *(uint64*)(s); }
#define COPY_64_BYTES(d, s) {                                                 \
        _mm_storeu_si128((__m128i*)d + 0, _mm_loadu_si128((__m128i*)s + 0));  \
        _mm_storeu_si128((__m128i*)d + 1, _mm_loadu_si128((__m128i*)s + 1));  \
        _mm_storeu_si128((__m128i*)d + 2, _mm_loadu_si128((__m128i*)s + 2));  \
        _mm_storeu_si128((__m128i*)d + 3, _mm_loadu_si128((__m128i*)s + 3));  \
}

#define COPY_64_ADD(d, s, t) _mm_storel_epi64((__m128i *)(d), _mm_add_epi8(_mm_loadl_epi64((__m128i *)(s)), _mm_loadl_epi64((__m128i *)(t))))

uint32 Kraken_GetCrc(const uint8* p, size_t p_size) {
	// TODO: implement
	return 0;
}

// Rearranges elements in the input array so that bits in the index get flipped.
static void ReverseBitsArray2048(const uint8* input, uint8* output) {
	static const uint8 offsets[32] = {
		0,    0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
		0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8
	};
	for (int i = 0; i != 32; i++) {
		int j = offsets[i];
		__m128i	t0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*) & input[j]), _mm_loadl_epi64((const __m128i*) & input[j + 256]));
		__m128i	t1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*) & input[j + 512]), _mm_loadl_epi64((const __m128i*) & input[j + 768]));
		__m128i	t2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*) & input[j + 1024]), _mm_loadl_epi64((const __m128i*) & input[j + 1280]));
		__m128i	t3 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*) & input[j + 1536]), _mm_loadl_epi64((const __m128i*) & input[j + 1792]));

		__m128i	s0 = _mm_unpacklo_epi8(t0, t1);
		__m128i	s1 = _mm_unpacklo_epi8(t2, t3);
		__m128i	s2 = _mm_unpackhi_epi8(t0, t1);
		__m128i	s3 = _mm_unpackhi_epi8(t2, t3);

		t0 = _mm_unpacklo_epi8(s0, s1);
		t1 = _mm_unpacklo_epi8(s2, s3);
		t2 = _mm_unpackhi_epi8(s0, s1);
		t3 = _mm_unpackhi_epi8(s2, s3);

		_mm_storel_epi64((__m128i*) & output[0], t0);
		_mm_storeh_pi((__m64*) & output[1024], _mm_castsi128_ps(t0));
		_mm_storel_epi64((__m128i*) & output[256], t1);
		_mm_storeh_pi((__m64*) & output[1280], _mm_castsi128_ps(t1));
		_mm_storel_epi64((__m128i*) & output[512], t2);
		_mm_storeh_pi((__m64*) & output[1536], _mm_castsi128_ps(t2));
		_mm_storel_epi64((__m128i*) & output[768], t3);
		_mm_storeh_pi((__m64*) & output[1792], _mm_castsi128_ps(t3));
		output += 8;
	}
}

struct HuffRevLut {
	uint8 bits2len[2048];
	uint8 bits2sym[2048];
};

struct HuffReader {
	// Array to hold the output of the huffman read array operation
	uint8		*output, *output_end;
	// We decode three parallel streams, two forwards, |src| and |src_mid| while |src_end| is decoded backwards. 
	const uint8	*src,		*src_mid,		*src_end,		*src_mid_org;
	int			src_bitpos, src_mid_bitpos, src_end_bitpos;
	uint32		src_bits,	src_mid_bits,	src_end_bits;
};

struct HuffRange {
	uint16 symbol;
	uint16 num;
};

bool Kraken_DecodeBytesCore(HuffReader* hr, HuffRevLut* lut) {
	const uint8*	src				= hr->src;
	uint32			src_bits		= hr->src_bits;
	int				src_bitpos		= hr->src_bitpos;

	const uint8*	src_mid			= hr->src_mid;
	uint32			src_mid_bits	= hr->src_mid_bits;
	int				src_mid_bitpos	= hr->src_mid_bitpos;

	const uint8*	src_end			= hr->src_end;
	uint32			src_end_bits	= hr->src_end_bits;
	int				src_end_bitpos	= hr->src_end_bitpos;

	uint8*			dst				= hr->output;
	uint8*			dst_end			= hr->output_end;

	if (src > src_mid)
		return false;

	if (hr->src_end - src_mid >= 4 && dst_end - dst >= 6) {
		dst_end -= 5;
		src_end -= 4;

		while (dst < dst_end && src <= src_mid && src_mid <= src_end) {
			src_bits		|= *(uint32*)src << src_bitpos;
			src				+= (31 - src_bitpos) >> 3;

			src_end_bits	|= *(uint32be*)src_end << src_end_bitpos;
			src_end			-= (31 - src_end_bitpos) >> 3;

			src_mid_bits	|= *(uint32*)src_mid << src_mid_bitpos;
			src_mid			+= (31 - src_mid_bitpos) >> 3;

			src_bitpos		|= 0x18;
			src_end_bitpos	|= 0x18;
			src_mid_bitpos	|= 0x18;

			int	k = src_bits & 0x7FF;
			int	n = lut->bits2len[k];
			src_bits >>= n;
			src_bitpos -= n;
			dst[0] = lut->bits2sym[k];

			k = src_end_bits & 0x7FF;
			n = lut->bits2len[k];
			src_end_bits >>= n;
			src_end_bitpos -= n;
			dst[1] = lut->bits2sym[k];

			k = src_mid_bits & 0x7FF;
			n = lut->bits2len[k];
			src_mid_bits >>= n;
			src_mid_bitpos -= n;
			dst[2] = lut->bits2sym[k];

			k = src_bits & 0x7FF;
			n = lut->bits2len[k];
			src_bits >>= n;
			src_bitpos -= n;
			dst[3] = lut->bits2sym[k];

			k = src_end_bits & 0x7FF;
			n = lut->bits2len[k];
			src_end_bits >>= n;
			src_end_bitpos -= n;
			dst[4] = lut->bits2sym[k];

			k = src_mid_bits & 0x7FF;
			n = lut->bits2len[k];
			src_mid_bits >>= n;
			src_mid_bitpos -= n;
			dst[5] = lut->bits2sym[k];
			dst += 6;
		}
		dst_end += 5;

		src -= src_bitpos >> 3;
		src_bitpos &= 7;

		src_end += 4 + (src_end_bitpos >> 3);
		src_end_bitpos &= 7;

		src_mid -= src_mid_bitpos >> 3;
		src_mid_bitpos &= 7;
	}

	for (;;) {
		if (dst >= dst_end)
			break;

		if (src_mid - src <= 1) {
			if (src_mid - src == 1)
				src_bits |= *src << src_bitpos;
		} else {
			src_bits |= *(uint16*)src << src_bitpos;
		}
		int	k = src_bits & 0x7FF;
		int	n = lut->bits2len[k];
		src_bitpos	-= n;
		src_bits	>>= n;
		*dst++		= lut->bits2sym[k];
		src			+= (7 - src_bitpos) >> 3;
		src_bitpos	&= 7;

		if (dst < dst_end) {
			if (src_end - src_mid <= 1) {
				if (src_end - src_mid == 1) {
					src_end_bits |= *src_mid << src_end_bitpos;
					src_mid_bits |= *src_mid << src_mid_bitpos;
				}
			} else {
				src_end_bits |= *(uint16be*)(src_end - 2) << src_end_bitpos;
				src_mid_bits |= *(uint16*)src_mid << src_mid_bitpos;
			}
			n		= lut->bits2len[src_end_bits & 0x7FF];
			*dst++	= lut->bits2sym[src_end_bits & 0x7FF];
			src_end_bitpos	-= n;
			src_end_bits	>>= n;
			src_end			-= (7 - src_end_bitpos) >> 3;
			src_end_bitpos	&= 7;

			if (dst < dst_end) {
				n		= lut->bits2len[src_mid_bits & 0x7FF];
				*dst++	= lut->bits2sym[src_mid_bits & 0x7FF];
				src_mid_bitpos	-= n;
				src_mid_bits	>>= n;
				src_mid			+= (7 - src_mid_bitpos) >> 3;
				src_mid_bitpos	&= 7;
			}
		}
		if (src > src_mid || src_mid > src_end)
			return false;
	}
	return src == hr->src_mid_org && src_end == src_mid;
}

int Huff_ReadCodeLengthsOld(BitReader& bits, uint8* syms, uint32* code_prefix) {
	if (bits.bit_norefill()) {
		int sym = 0, codelen, num_symbols = 0;
		int avg_bits_x4 = 32;
		int forced_bits = bits.get_norefill(2);

		uint32 thres_for_valid_gamma_bits = 1 << (31 - (20u >> forced_bits));
		if (bits.bit())
			goto SKIP_INITIAL_ZEROS;

		do {
			// Run of zeros
			if (!(bits.bits & 0xff000000))
				return -1;
			sym += bits.get_norefill(2 * (leading_zeros(bits.bits) + 1)) - 2 + 1;
			if (sym >= 256)
				break;

		SKIP_INITIAL_ZEROS:
			bits.Refill();
			// Read out the gamma value for the # of symbols
			if (!(bits.bits & 0xff000000))
				return -1;

			int	n = bits.get_norefill(2 * (leading_zeros(bits.bits) + 1)) - 2 + 1;
			// Overflow?
			if (sym + n > 256)
				return -1;

			bits.Refill();
			num_symbols += n;
			do {
				if (bits.bits < thres_for_valid_gamma_bits)
					return -1; // too big gamma value?

				int lz = leading_zeros(bits.bits);
				int v = bits.get_norefill(lz + forced_bits + 1) + ((lz - 1) << forced_bits);
				codelen = (-(int)(v & 1) ^ (v >> 1)) + ((avg_bits_x4 + 2) >> 2);
				if (codelen < 1 || codelen > 11)
					return -1;
				avg_bits_x4 = codelen + ((3 * avg_bits_x4 + 2) >> 2);
				bits.Refill();
				syms[code_prefix[codelen]++] = sym++;
			} while (--n);
		} while (sym != 256);

		return (sym == 256) && (num_symbols >= 2) ? num_symbols : -1;

	} else {
		// Sparse symbol encoding
		int num_symbols = bits.get_norefill(8);
		if (num_symbols == 0)
			return -1;
		if (num_symbols == 1) {
			syms[0] = bits.get_norefill(8);
		} else {
			int codelen_bits = bits.get_norefill(3);
			if (codelen_bits > 4)
				return -1;
			for (int i = 0; i < num_symbols; i++) {
				bits.Refill();
				int sym = bits.get_norefill(8);
				int codelen = bits.get_norefill_zero(codelen_bits) + 1;
				if (codelen > 11)
					return -1;
				syms[code_prefix[codelen]++] = sym;
			}
		}
		return num_symbols;
	}
}

int ReadFluff(BitReader& bits, int num_symbols) {
	unsigned long y;

	if (num_symbols == 256)
		return 0;

	int x = 257 - num_symbols;
	if (x > num_symbols)
		x = num_symbols;

	x *= 2;

	y = highest_set_index(x - 1) + 1;

	uint32 v = bits.bits >> (32 - y);
	uint32 z = (1 << y) - x;

	if ((v >> 1) >= z) {
		bits.bits <<= y;
		bits.bitpos += y;
		return v - z;
	} else {
		bits.bits <<= (y - 1);
		bits.bitpos += (y - 1);
		return (v >> 1);
	}
}

struct BitReader2 {
	const uint8*	p;
	const uint8*	p_end;
	uint32			bitpos;

	bool DecodeGolombRiceLengths(uint8* dst, size_t size);
	bool DecodeGolombRiceBits(uint8* dst, uint32 size, uint32 bitcount);
};

static const uint32 kRiceCodeBits2Value[256] = {
	0x80000000, 0x00000007, 0x10000006, 0x00000006, 0x20000005, 0x00000105, 0x10000005, 0x00000005,
	0x30000004, 0x00000204, 0x10000104, 0x00000104, 0x20000004, 0x00010004, 0x10000004, 0x00000004,
	0x40000003, 0x00000303, 0x10000203, 0x00000203, 0x20000103, 0x00010103, 0x10000103, 0x00000103,
	0x30000003, 0x00020003, 0x10010003, 0x00010003, 0x20000003, 0x01000003, 0x10000003, 0x00000003,
	0x50000002, 0x00000402, 0x10000302, 0x00000302, 0x20000202, 0x00010202, 0x10000202, 0x00000202,
	0x30000102, 0x00020102, 0x10010102, 0x00010102, 0x20000102, 0x01000102, 0x10000102, 0x00000102,
	0x40000002, 0x00030002, 0x10020002, 0x00020002, 0x20010002, 0x01010002, 0x10010002, 0x00010002,
	0x30000002, 0x02000002, 0x11000002, 0x01000002, 0x20000002, 0x00000012, 0x10000002, 0x00000002,
	0x60000001, 0x00000501, 0x10000401, 0x00000401, 0x20000301, 0x00010301, 0x10000301, 0x00000301,
	0x30000201, 0x00020201, 0x10010201, 0x00010201, 0x20000201, 0x01000201, 0x10000201, 0x00000201,
	0x40000101, 0x00030101, 0x10020101, 0x00020101, 0x20010101, 0x01010101, 0x10010101, 0x00010101,
	0x30000101, 0x02000101, 0x11000101, 0x01000101, 0x20000101, 0x00000111, 0x10000101, 0x00000101,
	0x50000001, 0x00040001, 0x10030001, 0x00030001, 0x20020001, 0x01020001, 0x10020001, 0x00020001,
	0x30010001, 0x02010001, 0x11010001, 0x01010001, 0x20010001, 0x00010011, 0x10010001, 0x00010001,
	0x40000001, 0x03000001, 0x12000001, 0x02000001, 0x21000001, 0x01000011, 0x11000001, 0x01000001,
	0x30000001, 0x00000021, 0x10000011, 0x00000011, 0x20000001, 0x00001001, 0x10000001, 0x00000001,
	0x70000000, 0x00000600, 0x10000500, 0x00000500, 0x20000400, 0x00010400, 0x10000400, 0x00000400,
	0x30000300, 0x00020300, 0x10010300, 0x00010300, 0x20000300, 0x01000300, 0x10000300, 0x00000300,
	0x40000200, 0x00030200, 0x10020200, 0x00020200, 0x20010200, 0x01010200, 0x10010200, 0x00010200,
	0x30000200, 0x02000200, 0x11000200, 0x01000200, 0x20000200, 0x00000210, 0x10000200, 0x00000200,
	0x50000100, 0x00040100, 0x10030100, 0x00030100, 0x20020100, 0x01020100, 0x10020100, 0x00020100,
	0x30010100, 0x02010100, 0x11010100, 0x01010100, 0x20010100, 0x00010110, 0x10010100, 0x00010100,
	0x40000100, 0x03000100, 0x12000100, 0x02000100, 0x21000100, 0x01000110, 0x11000100, 0x01000100,
	0x30000100, 0x00000120, 0x10000110, 0x00000110, 0x20000100, 0x00001100, 0x10000100, 0x00000100,
	0x60000000, 0x00050000, 0x10040000, 0x00040000, 0x20030000, 0x01030000, 0x10030000, 0x00030000,
	0x30020000, 0x02020000, 0x11020000, 0x01020000, 0x20020000, 0x00020010, 0x10020000, 0x00020000,
	0x40010000, 0x03010000, 0x12010000, 0x02010000, 0x21010000, 0x01010010, 0x11010000, 0x01010000,
	0x30010000, 0x00010020, 0x10010010, 0x00010010, 0x20010000, 0x00011000, 0x10010000, 0x00010000,
	0x50000000, 0x04000000, 0x13000000, 0x03000000, 0x22000000, 0x02000010, 0x12000000, 0x02000000,
	0x31000000, 0x01000020, 0x11000010, 0x01000010, 0x21000000, 0x01001000, 0x11000000, 0x01000000,
	0x40000000, 0x00000030, 0x10000020, 0x00000020, 0x20000010, 0x00001010, 0x10000010, 0x00000010,
	0x30000000, 0x00002000, 0x10001000, 0x00001000, 0x20000000, 0x00100000, 0x10000000, 0x00000000,
};

static const uint8 kRiceCodeBits2Len[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};


bool BitReader2::DecodeGolombRiceLengths(uint8* dst, size_t size) {
	const uint8*	p		= this->p;
	uint8*			dst_end = dst + size;
	if (p >= p_end)
		return false;

	int count	= -(int)bitpos;
	uint32 v	= *p++ & (255 >> bitpos);
	for (;;) {
		if (v == 0) {
			count += 8;
		} else {
			uint32 x = kRiceCodeBits2Value[v];
			*(uint32*)&dst[0] = count + (x & 0x0f0f0f0f);
			*(uint32*)&dst[4] = (x >> 4) & 0x0f0f0f0f;
			dst += count_bits(v);//kRiceCodeBits2Len[v];
			if (dst >= dst_end)
				break;
			count = x >> 28;
		}
		if (p >= p_end)
			return false;
		v = *p++;
	}
	
	// went too far, step back
	while (dst > dst_end) {
		v &= (v - 1);
		--dst;
	}
	
	// step back if uint8 not finished
	if (!(v & 1)) {
		--p;
		bitpos = 8 - lowest_set_index(v);
	} else {
		bitpos = 0;
	}
	this->p	= p;
	return true;
}

bool BitReader2::DecodeGolombRiceBits(uint8* dst, uint32 size, uint32 bitcount) {
	if (bitcount == 0)
		return true;

	uint8*			dst_end = dst + size;
	const uint8*	p		= this->p;
	int				bitpos	= this->bitpos;
	uint32			bits_required = bitpos + bitcount * size;

	if (p + (bits_required + 7) / 8 > p_end)
		return false;

	this->p			= p + (bits_required >> 3);
	this->bitpos	= bits_required & 7;

	// todo. handle r/w outside of range
	uint64 bak		= *(uint64*)dst_end;

	switch (bitcount) {
		case 1:
			do {
				// Read the next byte
				uint64 bits = part_bits<1, 7, 8>(*(uint32be*)p >> (24 - bitpos));
				p += 1;
				*(uint64*)dst = *(uint64*)dst * 2 + _byteswap_uint64(bits);
				dst += 8;
			} while (dst < dst_end);
		break;

		case 2:
			do {
				// Read the next 2 bytes
				uint64 bits = part_bits<2, 6, 16>(*(uint32be*)p >> (16 - bitpos));
				p += 2;
				*(uint64*)dst = *(uint64*)dst * 4 + _byteswap_uint64(bits);
				dst += 8;
			} while (dst < dst_end);
		break;

		case 3:
			do {
				// Read the next 3 bytes
				uint64 bits = part_bits<3, 5, 24>(*(uint32be*)p >> (8 - bitpos));
				p += 3;
				*(uint64*)dst = *(uint64*)dst * 8 + _byteswap_uint64(bits);
				dst += 8;
			} while (dst < dst_end);
			break;
	}
	*(uint64*)dst_end = bak;
	return true;
}

bit_address DecodeGolombRiceLengths(bit_address b, uint8* dst, size_t size) {
	uint8*	dst_end = dst + size;
	uint32	shift	= b.shift<uint8>();
	int		count	= -(int)shift;
	uint8	*p		= b.ptr<uint8>();
	uint32	v		= *p++ & (255 >> shift);

	for (;;) {
		if (v == 0) {
			count += 8;
		} else {
			uint32 x = kRiceCodeBits2Value[v];
			*(uint32*)&dst[0] = count + (x & 0x0f0f0f0f);
			*(uint32*)&dst[4] = (x >> 4) & 0x0f0f0f0f;
			dst += count_bits(v);//kRiceCodeBits2Len[v];
			if (dst >= dst_end)
				break;
			count = x >> 28;
		}
		v = *p++;
	}

	// went too far, step back
	while (dst > dst_end) {
		v &= (v - 1);
		--dst;
	}

	// step back if uint8 not finished
	return bit_address(p) - lowest_set_index(v);
}

bit_address DecodeGolombRiceBits(bit_address b, uint8* dst, uint32 size, uint32 bitcount) {
	if (bitcount == 0)
		return b;

	uint8*			dst_end = dst + size;
	const uint8*	p		= b.ptr<uint8>();
	int				bitpos	= b.shift<uint8>();
	uint32			bits_required = bitpos + bitcount * size;

	// todo. handle r/w outside of range
	uint64 bak		= *(uint64*)dst_end;

	switch (bitcount) {
		case 1:
			do {
				// Read the next byte
				uint64 bits = part_bits<1, 7, 8>(*(uint32be*)p >> (24 - bitpos));
				p += 1;
				*(uint64*)dst = *(uint64*)dst * 2 + _byteswap_uint64(bits);
				dst += 8;
			} while (dst < dst_end);
			break;

		case 2:
			do {
				// Read the next 2 bytes
				uint64 bits = part_bits<2, 6, 16>(*(uint32be*)p >> (16 - bitpos));
				p += 2;
				*(uint64*)dst = *(uint64*)dst * 4 + _byteswap_uint64(bits);
				dst += 8;
			} while (dst < dst_end);
			break;

		case 3:
			do {
				// Read the next 3 bytes
				uint64 bits = part_bits<3, 5, 24>(*(uint32be*)p >> (8 - bitpos));
				p += 3;
				*(uint64*)dst = *(uint64*)dst * 8 + _byteswap_uint64(bits);
				dst += 8;
			} while (dst < dst_end);
			break;
	}
	*(uint64*)dst_end = bak;
	return b + bits_required;
}

int Huff_ConvertToRanges(HuffRange* range, int num_symbols, int P, const uint8* symlen, BitReader& bits) {
	int num_ranges = P >> 1, v, sym_idx = 0;

	// Start with space?
	if (P & 1) {
		bits.Refill();
		v = *symlen++;
		if (v >= 8)
			return -1;
		sym_idx = bits.get_norefill(v + 1) + (1 << (v + 1)) - 1;
	}
	int syms_used = 0;

	for (int i = 0; i < num_ranges; i++) {
		bits.Refill();
		v = symlen[0];
		if (v >= 9)
			return -1;

		int num = bits.get_norefill_zero(v) + (1 << v);
		v = symlen[1];
		if (v >= 8)
			return -1;

		int space = bits.get_norefill(v + 1) + (1 << (v + 1)) - 1;
		range[i].symbol = sym_idx;
		range[i].num	= num;
		syms_used	+= num;
		sym_idx		+= num + space;
		symlen		+= 2;
	}

	if (sym_idx >= 256 || syms_used >= num_symbols || sym_idx + num_symbols - syms_used > 256)
		return -1;

	range[num_ranges].symbol = sym_idx;
	range[num_ranges].num = num_symbols - syms_used;

	return num_ranges + 1;
}

int Huff_ReadCodeLengthsNew(BitReader& bits, uint8* syms, uint32* code_prefix) {
	int		forced_bits		= bits.get_norefill(2);
	int		num_symbols		= bits.get_norefill(8) + 1;
	int		fluff			= ReadFluff(bits, num_symbols);
	uint8	code_len[512];

#if 0
	bit_address	b	= bits;
	b	= DecodeGolombRiceLengths(b, code_len, num_symbols + fluff);
	memset(code_len + (num_symbols + fluff), 0, 16);
	b	= DecodeGolombRiceBits(b, code_len, num_symbols, forced_bits);
	bits	= b;
#else
	BitReader2 br2;
	br2.bitpos	= (bits.bitpos - 24) & 7;
	br2.p_end	= bits.p_end;
	br2.p		= bits.p - (unsigned)((24 - bits.bitpos + 7) >> 3);
	if (!br2.DecodeGolombRiceLengths(code_len, num_symbols + fluff))
		return -1;

	memset(code_len + (num_symbols + fluff), 0, 16);
	if (!br2.DecodeGolombRiceBits(code_len, num_symbols, forced_bits))
		return -1;

	// Reset the bits decoder.
	bits.bitpos	= 24;
	bits.p			= br2.p;
	bits.bits		= 0;
	bits.Refill();
	bits.bits		<<= br2.bitpos;
	bits.bitpos	+= br2.bitpos;
#endif

	if (1) {
		uint32 running_sum = 0x1e;
		int maxlen = 11;
		for (int i = 0; i < num_symbols; i++) {
			int v = code_len[i];
			v = -(int)(v & 1) ^ (v >> 1);
			code_len[i] = v + (running_sum >> 2) + 1;
			if (code_len[i] < 1 || code_len[i] > 11)
				return -1;
			running_sum += v;
		}

	} else {
		// Ensure we don't read unknown data that could contaminate max_codeword_len
		__m128i bak = _mm_loadu_si128((__m128i*) & code_len[num_symbols]);
		_mm_storeu_si128((__m128i*) & code_len[num_symbols], _mm_set1_epi32(0));
		// apply a filter
		__m128i avg = _mm_set1_epi8(0x1e);
		__m128i ones = _mm_set1_epi8(1);
		__m128i max_codeword_len = _mm_set1_epi8(10);
		for (uint32 i = 0; i < num_symbols; i += 16) {
			__m128i v = _mm_loadu_si128((__m128i*) & code_len[i]), t;
			// avg[0..15] = avg[15]
			avg = _mm_unpackhi_epi8(avg, avg);
			avg = _mm_unpackhi_epi8(avg, avg);
			avg = _mm_shuffle_epi32(avg, 255);
			// v = -(int)(v & 1) ^ (v >> 1)
			v = _mm_xor_si128(_mm_sub_epi8(_mm_set1_epi8(0), _mm_and_si128(v, ones)),
				_mm_and_si128(_mm_srli_epi16(v, 1), _mm_set1_epi8(0x7f)));
			// create all the sums. v[n] = v[0] + ... + v[n]
			t = _mm_add_epi8(_mm_slli_si128(v, 1), v);
			t = _mm_add_epi8(_mm_slli_si128(t, 2), t);
			t = _mm_add_epi8(_mm_slli_si128(t, 4), t);
			t = _mm_add_epi8(_mm_slli_si128(t, 8), t);
			// u[x] = (avg + t[x-1]) >> 2
			__m128i u = _mm_and_si128(_mm_srli_epi16(_mm_add_epi8(_mm_slli_si128(t, 1), avg), 2u), _mm_set1_epi8(0x3f));
			// v += u
			v = _mm_add_epi8(v, u);
			// avg += t
			avg = _mm_add_epi8(avg, t);
			// max_codeword_len = max(max_codeword_len, v)
			max_codeword_len = _mm_max_epu8(max_codeword_len, v);
			// mem[] = v+1
			_mm_storeu_si128((__m128i*) & code_len[i], _mm_add_epi8(v, _mm_set1_epi8(1)));
		}
		_mm_storeu_si128((__m128i*) & code_len[num_symbols], bak);
		if (_mm_movemask_epi8(_mm_cmpeq_epi8(max_codeword_len, _mm_set1_epi8(10))) != 0xffff)
			return -1; // codeword too big?
	}

	HuffRange range[128];
	int ranges = Huff_ConvertToRanges(range, num_symbols, fluff, &code_len[num_symbols], bits);
	if (ranges <= 0)
		return -1;

	uint8* cp = code_len;
	for (int i = 0; i < ranges; i++) {
		int sym = range[i].symbol;
		int n = range[i].num;
		do {
			syms[code_prefix[*cp++]++] = sym++;
		} while (--n);
	}

	return num_symbols;
}

struct NewHuffLut {
	uint8 bits2len[2048 + 16];	// maps a bit pattern to a code length.
	uint8 bits2sym[2048 + 16];	// maps a bit pattern to a symbol.
};

// May overflow 16 bytes past the end
void FillByteOverflow16(uint8* dst, uint8 v, size_t n) {
	memset(dst, v, n);
}

bool Huff_MakeLut(const uint32* prefix_org, const uint32* prefix_cur, NewHuffLut* hufflut, uint8* syms) {
	uint32 currslot = 0;
	for (uint32 i = 1; i < 11; i++) {
		uint32 start = prefix_org[i];
		uint32 count = prefix_cur[i] - start;
		if (count) {
			uint32 stepsize = 1 << (11 - i);
			uint32 num_to_set = count << (11 - i);
			if (currslot + num_to_set > 2048)
				return false;
			FillByteOverflow16(&hufflut->bits2len[currslot], i, num_to_set);

			uint8* p = &hufflut->bits2sym[currslot];
			for (uint32 j = 0; j != count; j++, p += stepsize)
				FillByteOverflow16(p, syms[start + j], stepsize);
			currslot += num_to_set;
		}
	}
	if (prefix_cur[11] - prefix_org[11] != 0) {
		uint32 num_to_set = prefix_cur[11] - prefix_org[11];
		if (currslot + num_to_set > 2048)
			return false;
		FillByteOverflow16(&hufflut->bits2len[currslot], 11, num_to_set);
		memcpy(&hufflut->bits2sym[currslot], &syms[prefix_org[11]], num_to_set);
		currslot += num_to_set;
	}
	return currslot == 2048;
}

//-----------------------------------------------------------------------------
//	Tans
//-----------------------------------------------------------------------------

struct TansData {
	uint32	A_used;
	uint32	B_used;
	uint8	A[256];
	uint32	B[256];
};

template<typename T> void SimpleSort(T* p, T* pend) {
	if (p != pend) {
		for (T* lp = p + 1, *rp; lp != pend; lp++) {
			T t = lp[0];
			for (rp = lp; rp > p && t < rp[-1]; rp--)
				rp[0] = rp[-1];
			rp[0] = t;
		}
	}
}

bool Tans_DecodeTable(BitReader& bits, int L_bits, TansData* tans_data) {
	bits.Refill();
	if (bits.bit_norefill()) {
		int Q			= bits.get_norefill(3);
		int num_symbols	= bits.get_norefill(8) + 1;
		if (num_symbols < 2)
			return false;

		int fluff		= ReadFluff(bits, num_symbols);
		int total_rice_values = fluff + num_symbols;
		uint8 rice[512 + 16];

	#if 0
		bit_address	b	= bits;
		b		= DecodeGolombRiceLengths(b, rice, total_rice_values);
		memset(rice + total_rice_values, 0, 16);
		bits	= b;
	#else
		// another bit reader...
		BitReader2 br2;
		br2.p		= bits.p - ((uint32)(24 - bits.bitpos + 7) >> 3);
		br2.p_end	= bits.p_end;
		br2.bitpos	= (bits.bitpos - 24) & 7;

		if (!br2.DecodeGolombRiceLengths(rice, total_rice_values))
			return false;

		memset(rice + total_rice_values, 0, 16);

		// Switch back to other bitreader impl
		bits.bitpos	= 24;
		bits.p			= br2.p;
		bits.bits		= 0;
		bits.Refill();
		bits.bits		<<= br2.bitpos;
		bits.bitpos	+= br2.bitpos;
	#endif

		HuffRange range[133];
		fluff = Huff_ConvertToRanges(range, num_symbols, fluff, &rice[num_symbols], bits);
		if (fluff < 0)
			return false;

		bits.Refill();

		uint32	L = 1 << L_bits;
		uint8*	cur_rice_ptr = rice;
		int		average = 6;
		int		somesum = 0;
		uint8*	tanstable_A = tans_data->A;
		uint32*	tanstable_B = tans_data->B;

		for (int ri = 0; ri < fluff; ri++) {
			int symbol	= range[ri].symbol;
			int num		= range[ri].num;
			do {
				bits.Refill();

				int nextra = Q + *cur_rice_ptr++;
				if (nextra > 15)
					return false;

				int v				= bits.get_norefill_zero(nextra) + (1 << nextra) - (1 << Q);
				int average_div4	= average >> 2;
				int limit			= 2 * average_div4;
				if (v <= limit)
					v = average_div4 + (-(v & 1) ^ ((uint32)v >> 1));
				if (limit > v)
					limit = v;
				v += 1;
				average			+= limit - average_div4;
				*tanstable_A	= symbol;
				*tanstable_B	= (symbol << 16) + v;
				tanstable_A		+= v == 1;
				tanstable_B		+= v >= 2;
				somesum			+= v;
				symbol			+= 1;
			} while (--num);
		}
		tans_data->A_used = tanstable_A - tans_data->A;
		tans_data->B_used = tanstable_B - tans_data->B;
		return somesum == L;

	} else {
		bool seen[256];
		memset(seen, 0, sizeof(seen));
		uint32 L = 1 << L_bits;

		int		count			= bits.get_norefill(3) + 1;
		int		bits_per_sym	= highest_set_index(L_bits) + 1;
		int		max_delta_bits	= bits.get_norefill(bits_per_sym);

		if (max_delta_bits == 0 || max_delta_bits > L_bits)
			return false;

		uint8*	tanstable_A		= tans_data->A;
		uint32* tanstable_B		= tans_data->B;
		int		weight			= 0;
		int		total_weights	= 0;

		do {
			bits.Refill();

			int sym = bits.get_norefill(8);
			if (seen[sym])
				return false;

			weight += bits.get_norefill(max_delta_bits);

			if (weight == 0)
				return false;

			seen[sym] = true;
			if (weight == 1)
				*tanstable_A++ = sym;
			else
				*tanstable_B++ = (sym << 16) + weight;

			total_weights += weight;
		} while (--count);

		bits.Refill();

		int sym = bits.get_norefill(8);
		if (seen[sym])
			return false;

		if (L - total_weights < weight || L - total_weights <= 1)
			return false;

		*tanstable_B++ = (sym << 16) + (L - total_weights);

		tans_data->A_used = tanstable_A - tans_data->A;
		tans_data->B_used = tanstable_B - tans_data->B;

		SimpleSort(tans_data->A, tanstable_A);
		SimpleSort(tans_data->B, tanstable_B);
		return true;
	}
}

struct TansLutEnt {
	uint32	x;
	uint8	bits_x;
	uint8	symbol;
	uint16	w;
};

void Tans_InitLut(TansData* tans_data, int L_bits, TansLutEnt* lut) {
	TansLutEnt* pointers[4];

	int L = 1 << L_bits;
	int a_used = tans_data->A_used;

	uint32 slots_left_to_alloc = L - a_used;

	uint32 sa = slots_left_to_alloc >> 2;
	pointers[0] = lut;
	uint32 sb = sa + ((slots_left_to_alloc & 3) > 0);
	pointers[1] = lut + sb;
	sb += sa + ((slots_left_to_alloc & 3) > 1);
	pointers[2] = lut + sb;
	sb += sa + ((slots_left_to_alloc & 3) > 2);
	pointers[3] = lut + sb;

	// Setup the single entrys with weight=1
	{
		TansLutEnt* lut_singles = lut + slots_left_to_alloc, le;
		le.w = 0;
		le.bits_x = L_bits;
		le.x = (1 << L_bits) - 1;
		for (int i = 0; i < a_used; i++) {
			lut_singles[i] = le;
			lut_singles[i].symbol = tans_data->A[i];
		}
	}

	// Setup the entrys with weight >= 2
	int weights_sum = 0;
	for (int i = 0; i < tans_data->B_used; i++) {
		int weight = tans_data->B[i] & 0xffff;
		int symbol = tans_data->B[i] >> 16;
		if (weight > 4) {
			uint32 sym_bits = highest_set_index(weight);
			int Z = L_bits - sym_bits;
			TansLutEnt le;
			le.symbol = symbol;
			le.bits_x = Z;
			le.x = (1 << Z) - 1;
			le.w = (L - 1) & (weight << Z);
			int what_to_add = 1 << Z;
			int X = (1 << (sym_bits + 1)) - weight;

			for (int j = 0; j < 4; j++) {
				TansLutEnt* dst = pointers[j];

				int Y = (weight + ((weights_sum - j - 1) & 3)) >> 2;
				if (X >= Y) {
					for (int n = Y; n; n--) {
						*dst++ = le;
						le.w += what_to_add;
					}
					X -= Y;
				} else {
					for (int n = X; n; n--) {
						*dst++ = le;
						le.w += what_to_add;
					}
					Z--;

					what_to_add >>= 1;
					le.bits_x = Z;
					le.w = 0;
					le.x >>= 1;
					for (int n = Y - X; n; n--) {
						*dst++ = le;
						le.w += what_to_add;
					}
					X = weight;
				}
				pointers[j] = dst;
			}
		} else {
			ISO_ASSERT(weight > 0);
			uint32 bits = ((1 << weight) - 1) << (weights_sum & 3);
			bits |= (bits >> 4);
			int n = weight, ww = weight;
			do {
				uint32 idx = lowest_set_index(bits);
				bits &= bits - 1;
				TansLutEnt* dst = pointers[idx]++;
				dst->symbol = symbol;
				uint32 weight_bits = highest_set_index(ww);
				dst->bits_x = L_bits - weight_bits;
				dst->x = (1 << (L_bits - weight_bits)) - 1;
				dst->w = (L - 1) & (ww++ << (L_bits - weight_bits));
			} while (--n);
		}
		weights_sum += weight;
	}
}

struct TansDecoderParams {
	TansLutEnt*		lut;
	uint8*			dst,		*dst_end;
	const uint8*	ptr_f,		*ptr_b;
	uint32			bits_f,		bits_b;
	int				bitpos_f,	bitpos_b;
	uint32			state_0, state_1, state_2, state_3, state_4;
};

bool Tans_Decode(TansDecoderParams* params) {
	TansLutEnt* lut			= params->lut,		*e;
	uint8*		dst			= params->dst,		*dst_end	= params->dst_end;
	const uint8* ptr_f		= params->ptr_f,	*ptr_b		= params->ptr_b;
	uint32		bits_f		= params->bits_f,	bits_b		= params->bits_b;
	int			bitpos_f	= params->bitpos_f,	bitpos_b	= params->bitpos_b;
	uint32		state_0		= params->state_0,	state_1		= params->state_1, state_2 = params->state_2, state_3 = params->state_3, state_4 = params->state_4;

	if (ptr_f > ptr_b)
		return false;

#define TANS_FORWARD_BITS()						\
    bits_f		|= *(uint32 *)ptr_f << bitpos_f;\
    ptr_f		+= (31 - bitpos_f) >> 3;		\
    bitpos_f	|= 24;

#define TANS_FORWARD_ROUND(state)				\
    e			= &lut[state];					\
    *dst++		= e->symbol;					\
    bitpos_f	-= e->bits_x;					\
    state		= (bits_f & e->x) + e->w;		\
    bits_f		>>= e->bits_x;					\
    if (dst >= dst_end)							\
      break;

#define TANS_BACKWARD_BITS()							\
    bits_b		|= ((uint32be*)ptr_b)[-1] << bitpos_b;	\
    ptr_b		-= (31 - bitpos_b) >> 3;				\
    bitpos_b	|= 24;

#define TANS_BACKWARD_ROUND(state)				\
    e			= &lut[state];					\
    *dst++		= e->symbol;					\
    bitpos_b	-= e->bits_x;					\
    state		= (bits_b & e->x) + e->w;		\
    bits_b		>>= e->bits_x;					\
    if (dst >= dst_end)							\
      break;

	if (dst < dst_end) {
		for (;;) {
			TANS_FORWARD_BITS();
			TANS_FORWARD_ROUND(state_0);
			TANS_FORWARD_ROUND(state_1);
			TANS_FORWARD_BITS();
			TANS_FORWARD_ROUND(state_2);
			TANS_FORWARD_ROUND(state_3);
			TANS_FORWARD_BITS();
			TANS_FORWARD_ROUND(state_4);
			TANS_BACKWARD_BITS();
			TANS_BACKWARD_ROUND(state_0);
			TANS_BACKWARD_ROUND(state_1);
			TANS_BACKWARD_BITS();
			TANS_BACKWARD_ROUND(state_2);
			TANS_BACKWARD_ROUND(state_3);
			TANS_BACKWARD_BITS();
			TANS_BACKWARD_ROUND(state_4);
		}
	}

	if (ptr_b - ptr_f + (bitpos_f >> 3) + (bitpos_b >> 3) != 0)
		return false;

	uint32 states_or = state_0 | state_1 | state_2 | state_3 | state_4;
	if (states_or & ~0xFF)
		return false;

	dst_end[0] = (uint8)state_0;
	dst_end[1] = (uint8)state_1;
	dst_end[2] = (uint8)state_2;
	dst_end[3] = (uint8)state_3;
	dst_end[4] = (uint8)state_4;
	return true;
}

//-----------------------------------------------------------------------------
//	Kraken (shared w/Leviathan,Mermain,Selkie)
//-----------------------------------------------------------------------------

int Kraken_GetBlockSize(const uint8* src, const uint8* src_end, int* dest_size, int dest_capacity) {
	const uint8* src_org = src;
	int src_size, dst_size;

	if (src_end - src < 2)
		return -1; // too few bytes

	int chunk_type = (src[0] >> 4) & 0x7;
	if (chunk_type == 0) {
		if (src[0] >= 0x80) {
			// In this mode, memcopy stores the length in the bottom 12 bits.
			src_size = ((src[0] << 8) | src[1]) & 0xFFF;
			src += 2;
		} else {
			if (src_end - src < 3)
				return -1; // too few bytes
			src_size = ((src[0] << 16) | (src[1] << 8) | src[2]);
			if (src_size & ~0x3ffff)
				return -1; // reserved bits must not be set
			src += 3;
		}
		if (src_size > dest_capacity || src_end - src < src_size)
			return -1;
		*dest_size = src_size;
		return src + src_size - src_org;
	}

	if (chunk_type >= 6)
		return -1;

	// In all the other modes, the initial bytes encode the src_size and the dst_size
	if (src[0] >= 0x80) {
		if (src_end - src < 3)
			return -1; // too few bytes

		// short mode, 10 bit sizes
		uint32 bits = ((src[0] << 16) | (src[1] << 8) | src[2]);
		src_size = bits & 0x3ff;
		dst_size = src_size + ((bits >> 10) & 0x3ff) + 1;
		src += 3;
	} else {
		// long mode, 18 bit sizes
		if (src_end - src < 5)
			return -1; // too few bytes
		uint32 bits = ((src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4]);
		src_size = bits & 0x3ffff;
		dst_size = (((bits >> 18) | (src[0] << 14)) & 0x3FFFF) + 1;
		if (src_size >= dst_size)
			return -1;
		src += 5;
	}
	if (src_end - src < src_size || dst_size > dest_capacity)
		return -1;
	*dest_size = dst_size;
	return src_size;
}

int Kraken_DecodeBytes_Type12(const uint8* src, size_t src_size, uint8* output, int output_size, int type) {
	BitReader	bits;
	NewHuffLut	huff_lut;
	HuffRevLut	rev_lut;
	const uint8* src_end = src + src_size;

	bits.bitpos = 24;
	bits.bits	= 0;
	bits.p		= src;
	bits.p_end	= src_end;
	bits.Refill();

	static const uint32 code_prefix_org[12] = { 0x0, 0x0, 0x2, 0x6, 0xE, 0x1E, 0x3E, 0x7E, 0xFE, 0x1FE, 0x2FE, 0x3FE };
	uint32 code_prefix[12] = { 0x0, 0x0, 0x2, 0x6, 0xE, 0x1E, 0x3E, 0x7E, 0xFE, 0x1FE, 0x2FE, 0x3FE };
	uint8 syms[1280];

	int num_syms;
	if (!bits.bit_norefill()) {
		num_syms = Huff_ReadCodeLengthsOld(bits, syms, code_prefix);
	} else if (!bits.bit_norefill()) {
		num_syms = Huff_ReadCodeLengthsNew(bits, syms, code_prefix);
	} else {
		return -1;
	}

	if (num_syms < 1)
		return -1;

	src = bits.p - ((24 - bits.bitpos) / 8);

	if (num_syms == 1) {
		memset(output, syms[0], output_size);
		return src - src_end;
	}

	if (!Huff_MakeLut(code_prefix_org, code_prefix, &huff_lut, syms))
		return -1;

	ReverseBitsArray2048(huff_lut.bits2len, rev_lut.bits2len);
	ReverseBitsArray2048(huff_lut.bits2sym, rev_lut.bits2sym);

	if (type == 1) {
		if (src + 3 > src_end)
			return -1;

		uint32	split_mid	= *(uint16*)src;
		src			+= 2;

		HuffReader	hr;
		hr.output			= output;
		hr.output_end		= output + output_size;
		hr.src				= src;
		hr.src_end			= src_end;
		hr.src_mid_org		= hr.src_mid = src + split_mid;
		hr.src_bitpos		= 0;
		hr.src_bits			= 0;
		hr.src_mid_bitpos	= 0;
		hr.src_mid_bits		= 0;
		hr.src_end_bitpos	= 0;
		hr.src_end_bits		= 0;
		if (!Kraken_DecodeBytesCore(&hr, &rev_lut))
			return -1;

	} else {
		if (src + 6 > src_end)
			return -1;

		int	half_output_size	= (output_size + 1) >> 1;
		uint32	split_mid		= *(uint32*)src & 0xFFFFFF;
		src	+= 3;
		if (split_mid > (src_end - src))
			return -1;
		
		const uint8* src_mid	= src + split_mid;
		uint32	split_left	= *(uint16*)src;
		src	+= 2;
		if (src_mid - src < split_left + 2 || src_end - src_mid < 3)
			return -1;

		uint32	split_right	= *(uint16*)src_mid;
		if (src_end - (src_mid + 2) < split_right + 2)
			return -1;

		HuffReader	hr;
		hr.output			= output;
		hr.output_end		= output + half_output_size;
		hr.src				= src;
		hr.src_end			= src_mid;
		hr.src_mid_org		= hr.src_mid = src + split_left;
		hr.src_bitpos		= 0;
		hr.src_bits			= 0;
		hr.src_mid_bitpos	= 0;
		hr.src_mid_bits		= 0;
		hr.src_end_bitpos	= 0;
		hr.src_end_bits		= 0;
		if (!Kraken_DecodeBytesCore(&hr, &rev_lut))
			return -1;

		hr.output			= output + half_output_size;
		hr.output_end		= output + output_size;
		hr.src				= src_mid + 2;
		hr.src_end			= src_end;
		hr.src_mid_org		= hr.src_mid = src_mid + 2 + split_right;
		hr.src_bitpos		= 0;
		hr.src_bits			= 0;
		hr.src_mid_bitpos	= 0;
		hr.src_mid_bits		= 0;
		hr.src_end_bitpos	= 0;
		hr.src_end_bits		= 0;
		if (!Kraken_DecodeBytesCore(&hr, &rev_lut))
			return -1;
	}
	return (int)src_size;
}

int Kraken_DecodeBytes(uint8** output, const uint8* src, const uint8* src_end, int* decoded_size, size_t output_size, bool force_memmove, uint8* scratch, uint8* scratch_end);

int Kraken_DecodeMultiArray(const uint8* src, const uint8* src_end, uint8* dst, uint8* dst_end, uint8** array_data, int* array_lens, int array_count, int* total_size_out, bool force_memmove, uint8* scratch, uint8* scratch_end) {
	const uint8* src_org = src;

	if (src_end - src < 4)
		return -1;

	int decoded_size;
	int num_arrays_in_file = *src++;
	if (!(num_arrays_in_file & 0x80))
		return -1;
	num_arrays_in_file &= 0x3f;

	if (dst == scratch) {
		// todo: ensure scratch space first?
		scratch += (scratch_end - scratch - 0xc000) >> 1;
		dst_end = scratch;
	}

	int total_size = 0;

	if (num_arrays_in_file == 0) {
		for (int i = 0; i < array_count; i++) {
			uint8* chunk_dst = dst;
			int dec = Kraken_DecodeBytes(&chunk_dst, src, src_end, &decoded_size, dst_end - dst, force_memmove, scratch, scratch_end);
			if (dec < 0)
				return -1;
			dst += decoded_size;
			array_lens[i] = decoded_size;
			array_data[i] = chunk_dst;
			src += dec;
			total_size += decoded_size;
		}
		*total_size_out = total_size;
		return src - src_org; // not supported yet
	}

	uint8* entropy_array_data[32];
	uint32 entropy_array_size[32];

	// First loop just decodes everything to scratch
	uint8* scratch_cur = scratch;

	for (int i = 0; i < num_arrays_in_file; i++) {
		uint8* chunk_dst = scratch_cur;
		int dec = Kraken_DecodeBytes(&chunk_dst, src, src_end, &decoded_size, scratch_end - scratch_cur, force_memmove, scratch_cur, scratch_end);
		if (dec < 0)
			return -1;
		entropy_array_data[i] = chunk_dst;
		entropy_array_size[i] = decoded_size;
		scratch_cur += decoded_size;
		total_size += decoded_size;
		src += dec;
	}
	*total_size_out = total_size;

	if (src_end - src < 3)
		return -1;

	int Q = *(uint16*)src;
	src += 2;

	int out_size;
	if (Kraken_GetBlockSize(src, src_end, &out_size, total_size) < 0)
		return -1;
	int num_indexes = out_size;

	int num_lens = num_indexes - array_count;
	if (num_lens < 1)
		return -1;

	if (scratch_end - scratch_cur < num_indexes)
		return -1;

	uint8* interval_lenlog2 = scratch_cur;
	scratch_cur += num_indexes;

	if (scratch_end - scratch_cur < num_indexes)
		return -1;

	uint8* interval_indexes = scratch_cur;
	scratch_cur += num_indexes;


	if (Q & 0x8000) {
		int size_out;
		int n = Kraken_DecodeBytes(&interval_indexes, src, src_end, &size_out, num_indexes, false, scratch_cur, scratch_end);
		if (n < 0 || size_out != num_indexes)
			return -1;
		src += n;

		for (int i = 0; i < num_indexes; i++) {
			int t = interval_indexes[i];
			interval_lenlog2[i] = t >> 4;
			interval_indexes[i] = t & 0xF;
		}

		num_lens = num_indexes;

	} else {
		int lenlog2_chunksize = num_indexes - array_count;

		int size_out;
		int n = Kraken_DecodeBytes(&interval_indexes, src, src_end, &size_out, num_indexes, false, scratch_cur, scratch_end);
		if (n < 0 || size_out != num_indexes)
			return -1;
		src += n;

		n = Kraken_DecodeBytes(&interval_lenlog2, src, src_end, &size_out, lenlog2_chunksize, false, scratch_cur, scratch_end);
		if (n < 0 || size_out != lenlog2_chunksize)
			return -1;
		src += n;

		for (int i = 0; i < lenlog2_chunksize; i++)
			if (interval_lenlog2[i] > 16)
				return -1;
	}

	if (scratch_end - scratch_cur < 4)
		return -1;

	scratch_cur = align(scratch_cur, 4);
	if (scratch_end - scratch_cur < num_lens * 4)
		return -1;
	uint32* decoded_intervals = (uint32*)scratch_cur;

	int varbits_complen = Q & 0x3FFF;
	if (src_end - src < varbits_complen)
		return -1;

	const uint8* f = src;
	uint32 bits_f = 0;
	int bitpos_f = 24;

	const uint8* src_end_actual = src + varbits_complen;

	const uint8* b = src_end_actual;
	uint32 bits_b = 0;
	int bitpos_b = 24;


	static const uint32 bitmasks[32] = {
		0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff,
		0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff,
		0x1ffff, 0x3ffff, 0x7ffff, 0xfffff, 0x1fffff, 0x3fffff, 0x7fffff,
		0xffffff, 0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff
	};

	int i;
	for (i = 0; i + 2 <= num_lens; i += 2) {
		bits_f |= *(uint32be*)f >> (24 - bitpos_f);
		f += (bitpos_f + 7) >> 3;

		bits_b |= ((uint32*)b)[-1] >> (24 - bitpos_b);
		b -= (bitpos_b + 7) >> 3;

		int numbits_f = interval_lenlog2[i + 0];
		int numbits_b = interval_lenlog2[i + 1];

		bits_f = rotate_left(bits_f | 1, numbits_f);
		bitpos_f += numbits_f - 8 * ((bitpos_f + 7) >> 3);

		bits_b = rotate_left(bits_b | 1, numbits_b);
		bitpos_b += numbits_b - 8 * ((bitpos_b + 7) >> 3);

		int value_f = bits_f & bitmasks[numbits_f];
		bits_f &= ~bitmasks[numbits_f];

		int value_b = bits_b & bitmasks[numbits_b];
		bits_b &= ~bitmasks[numbits_b];

		decoded_intervals[i + 0] = value_f;
		decoded_intervals[i + 1] = value_b;
	}

	// read final one since above loop reads 2
	if (i < num_lens) {
		bits_f |= *(uint32be*)f >> (24 - bitpos_f);
		int numbits_f = interval_lenlog2[i];
		bits_f = rotate_left(bits_f | 1, numbits_f);
		int value_f = bits_f & bitmasks[numbits_f];
		decoded_intervals[i + 0] = value_f;
	}

	if (interval_indexes[num_indexes - 1])
		return -1;

	int indi = 0, leni = 0, source;
	int increment_leni = (Q & 0x8000) != 0;

	for (int arri = 0; arri < array_count; arri++) {
		array_data[arri] = dst;
		if (indi >= num_indexes)
			return -1;

		while ((source = interval_indexes[indi++]) != 0) {
			if (source > num_arrays_in_file)
				return -1;
			if (leni >= num_lens)
				return -1;
			int cur_len = decoded_intervals[leni++];
			int bytes_left = entropy_array_size[source - 1];
			if (cur_len > bytes_left || cur_len > dst_end - dst)
				return -1;
			uint8* blksrc = entropy_array_data[source - 1];
			entropy_array_size[source - 1] -= cur_len;
			entropy_array_data[source - 1] += cur_len;
			uint8* dstx = dst;
			dst += cur_len;
			memcpy(dstx, blksrc, cur_len);
		}
		leni += increment_leni;
		array_lens[arri] = dst - array_data[arri];
	}

	if (indi != num_indexes || leni != num_lens)
		return -1;

	for (int i = 0; i < num_arrays_in_file; i++) {
		if (entropy_array_size[i])
			return -1;
	}
	return src_end_actual - src_org;
}

int Kraken_DecodeRecursive(const uint8* src, size_t src_size, uint8* output, int output_size, uint8* scratch, uint8* scratch_end) {
	const uint8* src_org = src;
	uint8* output_end = output + output_size;
	const uint8* src_end = src + src_size;

	if (src_size < 6)
		return -1;

	int n = src[0] & 0x7f;
	if (n < 2)
		return -1;

	if (!(src[0] & 0x80)) {
		src++;
		do {
			int decoded_size;
			int dec = Kraken_DecodeBytes(&output, src, src_end, &decoded_size, output_end - output, true, scratch, scratch_end);
			if (dec < 0)
				return -1;
			output += decoded_size;
			src += dec;
		} while (--n);

		return output != output_end ? -1 : src - src_org;

	} else {
		uint8* array_data;
		int array_len, decoded_size;
		int dec = Kraken_DecodeMultiArray(src, src_end, output, output_end, &array_data, &array_len, 1, &decoded_size, true, scratch, scratch_end);
		if (dec < 0)
			return -1;
		output += decoded_size;
		return output != output_end ? -1 : dec;
	}
}

int Kraken_DecodeRLE(const uint8* src, size_t src_size, uint8* dst, int dst_size, uint8* scratch, uint8* scratch_end) {
	if (src_size <= 1) {
		if (src_size != 1)
			return -1;
		memset(dst, src[0], dst_size);
		return 1;
	}
	uint8* dst_end = dst + dst_size;
	const uint8* cmd_ptr = src + 1, * cmd_ptr_end = src + src_size;

	// Unpack the first X bytes of the command buffer?
	if (src[0]) {
		uint8* dst_ptr = scratch;
		int dec_size;
		int n = Kraken_DecodeBytes(&dst_ptr, src, src + src_size, &dec_size, scratch_end - scratch, true, scratch, scratch_end);
		if (n <= 0)
			return -1;
		int cmd_len = src_size - n + dec_size;
		if (cmd_len > scratch_end - scratch)
			return -1;
		memcpy(dst_ptr + dec_size, src + n, src_size - n);
		cmd_ptr = dst_ptr;
		cmd_ptr_end = &dst_ptr[cmd_len];
	}

	int rle_byte = 0;

	while (cmd_ptr < cmd_ptr_end) {
		uint32 cmd = cmd_ptr_end[-1];
		if (cmd - 1 >= 0x2f) {
			cmd_ptr_end--;
			uint32 bytes_to_copy = (-1 - cmd) & 0xF;
			uint32 bytes_to_rle = cmd >> 4;
			if (dst_end - dst < bytes_to_copy + bytes_to_rle || cmd_ptr_end - cmd_ptr < bytes_to_copy)
				return -1;
			memcpy(dst, cmd_ptr, bytes_to_copy);
			cmd_ptr += bytes_to_copy;
			dst += bytes_to_copy;
			memset(dst, rle_byte, bytes_to_rle);
			dst += bytes_to_rle;

		} else if (cmd >= 0x10) {
			uint32 data = *(uint16*)(cmd_ptr_end - 2) - 4096;
			cmd_ptr_end -= 2;
			uint32 bytes_to_copy = data & 0x3F;
			uint32 bytes_to_rle = data >> 6;
			if (dst_end - dst < bytes_to_copy + bytes_to_rle || cmd_ptr_end - cmd_ptr < bytes_to_copy)
				return -1;
			memcpy(dst, cmd_ptr, bytes_to_copy);
			cmd_ptr += bytes_to_copy;
			dst += bytes_to_copy;
			memset(dst, rle_byte, bytes_to_rle);
			dst += bytes_to_rle;

		} else if (cmd == 1) {
			rle_byte = *cmd_ptr++;
			cmd_ptr_end--;

		} else if (cmd >= 9) {
			uint32 bytes_to_rle = (*(uint16*)(cmd_ptr_end - 2) - 0x8ff) * 128;
			cmd_ptr_end -= 2;
			if (dst_end - dst < bytes_to_rle)
				return -1;
			memset(dst, rle_byte, bytes_to_rle);
			dst += bytes_to_rle;

		} else {
			uint32 bytes_to_copy = (*(uint16*)(cmd_ptr_end - 2) - 511) * 64;
			cmd_ptr_end -= 2;
			if (cmd_ptr_end - cmd_ptr < bytes_to_copy || dst_end - dst < bytes_to_copy)
				return -1;
			memcpy(dst, cmd_ptr, bytes_to_copy);
			dst += bytes_to_copy;
			cmd_ptr += bytes_to_copy;
		}
	}
	if (cmd_ptr_end != cmd_ptr)
		return -1;

	if (dst != dst_end)
		return -1;

	return src_size;
}

int Kraken_DecodeTans(const uint8* src, size_t src_size, uint8* dst, int dst_size, uint8* scratch, uint8* scratch_end) {
	if (src_size < 8 || dst_size < 5)
		return -1;

	const uint8* src_end = src + src_size;

	BitReader br;
	TansData tans_data;

	br.bitpos = 24;
	br.bits = 0;
	br.p = src;
	br.p_end = src_end;
	br.Refill();

	// reserved bit
	if (br.bit_norefill())
		return -1;

	int L_bits = br.get_norefill(2) + 8;

	if (!Tans_DecodeTable(br, L_bits, &tans_data))
		return -1;

	src = br.p - (24 - br.bitpos) / 8;

	if (src >= src_end)
		return -1;

	uint32 lut_space_required = ((sizeof(TansLutEnt) << L_bits) + 15) & ~15;
	if (lut_space_required > (scratch_end - scratch))
		return -1;

	TansDecoderParams params;
	params.dst		= dst;
	params.dst_end	= dst + dst_size - 5;
	params.lut		= (TansLutEnt*)align(scratch, 16);
	Tans_InitLut(&tans_data, L_bits, params.lut);

	// Read out the initial state
	uint32 L_mask = (1 << L_bits) - 1;
	uint32 bits_f = *(uint32*)src;
	src += 4;
	uint32 bits_b = *(uint32be*)(src_end - 4);
	src_end -= 4;
	uint32 bitpos_f = 32, bitpos_b = 32;

	// Read first two.
	params.state_0 = bits_f & L_mask;
	params.state_1 = bits_b & L_mask;
	bits_f >>= L_bits, bitpos_f -= L_bits;
	bits_b >>= L_bits, bitpos_b -= L_bits;

	// Read next two.
	params.state_2 = bits_f & L_mask;
	params.state_3 = bits_b & L_mask;
	bits_f >>= L_bits, bitpos_f -= L_bits;
	bits_b >>= L_bits, bitpos_b -= L_bits;

	// Refill more bits
	bits_f |= *(uint32*)src << bitpos_f;
	src += (31 - bitpos_f) >> 3;
	bitpos_f |= 24;

	// Read final state variable
	params.state_4 = bits_f & L_mask;
	bits_f >>= L_bits, bitpos_f -= L_bits;

	params.bits_f = bits_f;
	params.ptr_f = src - (bitpos_f >> 3);
	params.bitpos_f = bitpos_f & 7;

	params.bits_b = bits_b;
	params.ptr_b = src_end + (bitpos_b >> 3);
	params.bitpos_b = bitpos_b & 7;

	if (!Tans_Decode(&params))
		return -1;

	return src_size;
}

int Kraken_DecodeBytes(uint8** output, const uint8* src, const uint8* src_end, int* decoded_size, size_t output_size, bool force_memmove, uint8* scratch, uint8* scratch_end) {
	const uint8* src_org = src;
	int src_size, dst_size;

	if (src_end - src < 2)
		return -1; // too few bytes

	int chunk_type = (src[0] >> 4) & 0x7;
	if (chunk_type == 0) {
		if (src[0] >= 0x80) {
			// In this mode, memcopy stores the length in the bottom 12 bits.
			src_size = ((src[0] << 8) | src[1]) & 0xFFF;
			src += 2;
		} else {
			if (src_end - src < 3)
				return -1; // too few bytes
			src_size = ((src[0] << 16) | (src[1] << 8) | src[2]);
			if (src_size & ~0x3ffff)
				return -1; // reserved bits must not be set
			src += 3;
		}
		if (src_size > output_size || src_end - src < src_size)
			return -1;
		*decoded_size = src_size;
		if (force_memmove)
			memmove(*output, src, src_size);
		else
			*output = (uint8*)src;
		return src + src_size - src_org;
	}

	// In all the other modes, the initial bytes encode the src_size and the dst_size
	if (src[0] >= 0x80) {
		if (src_end - src < 3)
			return -1; // too few bytes

		// short mode, 10 bit sizes
		uint32 bits = ((src[0] << 16) | (src[1] << 8) | src[2]);
		src_size = bits & 0x3ff;
		dst_size = src_size + ((bits >> 10) & 0x3ff) + 1;
		src += 3;
	} else {
		// long mode, 18 bit sizes
		if (src_end - src < 5)
			return -1; // too few bytes
		uint32 bits = ((src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4]);
		src_size = bits & 0x3ffff;
		dst_size = (((bits >> 18) | (src[0] << 14)) & 0x3FFFF) + 1;
		if (src_size >= dst_size)
			return -1;
		src += 5;
	}
	if (src_end - src < src_size || dst_size > output_size)
		return -1;

	uint8* dst = *output;
	if (dst == scratch) {
		if (scratch_end - scratch < dst_size)
			return -1;
		scratch += dst_size;
	}

	//  printf("%d -> %d (%d)\n", src_size, dst_size, chunk_type);

	int src_used = -1;
	switch (chunk_type) {
		case 2:
		case 4:
			src_used = Kraken_DecodeBytes_Type12(src, src_size, dst, dst_size, chunk_type >> 1);
			break;
		case 5:
			src_used = Kraken_DecodeRecursive(src, src_size, dst, dst_size, scratch, scratch_end);
			break;
		case 3:
			src_used = Kraken_DecodeRLE(src, src_size, dst, dst_size, scratch, scratch_end);
			break;
		case 1:
			src_used = Kraken_DecodeTans(src, src_size, dst, dst_size, scratch, scratch_end);
			break;
	}
	if (src_used != src_size)
		return -1;
	*decoded_size = dst_size;
	return src + src_size - src_org;
}

void CombineScaledOffsetArrays(int* offs_stream, size_t offs_stream_size, int scale, const uint8* low_bits) {
	for (size_t i = 0; i != offs_stream_size; i++)
		offs_stream[i] = scale * offs_stream[i] - low_bits[i];
}

// Unpacks the packed 8 bit offset and lengths into 32 bit.
bool Kraken_UnpackOffsets(const uint8* src, const uint8* src_end,
	const uint8* packed_offs_stream, const uint8* packed_offs_stream_extra, int packed_offs_stream_size,
	int multi_dist_scale,
	const uint8* packed_litlen_stream, int packed_litlen_stream_size,
	int* offs_stream, int* len_stream,
	bool excess_flag, int excess_bytes) {


	BitReader bits_a, bits_b;
	int n, i;
	int u32_len_stream_size = 0;

	bits_a.bitpos	= 24;
	bits_a.bits		= 0;
	bits_a.p		= src;
	bits_a.p_end	= src_end;
	bits_a.Refill();

	bits_b.bitpos	= 24;
	bits_b.bits		= 0;
	bits_b.p		= src_end;
	bits_b.p_end	= src;
	bits_b.RefillBackwards();

	if (!excess_flag) {
		if (bits_b.bits < 0x2000)
			return false;
		int	n = leading_zeros(bits_b.bits);
		bits_b.bitpos += n;
		bits_b.bits <<= n;
		bits_b.RefillBackwards();
		n++;
		u32_len_stream_size = (bits_b.bits >> (32 - n)) - 1;
		bits_b.bitpos += n;
		bits_b.bits <<= n;
		bits_b.RefillBackwards();
	}

	if (multi_dist_scale == 0) {
		// Traditional way of coding offsets
		const uint8* packed_offs_stream_end = packed_offs_stream + packed_offs_stream_size;
		while (packed_offs_stream != packed_offs_stream_end) {
			*offs_stream++ = -(int32)ReadDistance(bits_a, *packed_offs_stream++);
			if (packed_offs_stream == packed_offs_stream_end)
				break;
			*offs_stream++ = -(int32)ReadDistanceB(bits_b, *packed_offs_stream++);
		}

	} else {
		// New way of coding offsets 
		int* offs_stream_org = offs_stream;
		const uint8* packed_offs_stream_end = packed_offs_stream + packed_offs_stream_size;
		while (packed_offs_stream != packed_offs_stream_end) {
			uint32	cmd = *packed_offs_stream++;
			if ((cmd >> 3) > 26)
				return 0;
			uint32	offs = ((8 + (cmd & 7)) << (cmd >> 3)) | bits_a.get((cmd >> 3));
			*offs_stream++ = 8 - (int32)offs;
			if (packed_offs_stream == packed_offs_stream_end)
				break;
			cmd = *packed_offs_stream++;
			if ((cmd >> 3) > 26)
				return 0;
			offs = ((8 + (cmd & 7)) << (cmd >> 3)) | bits_b.get_backwards((cmd >> 3));
			*offs_stream++ = 8 - (int32)offs;
		}
		if (multi_dist_scale != 1) {
			CombineScaledOffsetArrays(offs_stream_org, offs_stream - offs_stream_org, multi_dist_scale, packed_offs_stream_extra);
		}
	}
	uint32 u32_len_stream_buf[512]; // max count is 128kb / 256 = 512
	if (u32_len_stream_size > 512)
		return false;

	uint32	*u32_len_stream = u32_len_stream_buf,
			*u32_len_stream_end = u32_len_stream_buf + u32_len_stream_size;
	for (i = 0; i + 1 < u32_len_stream_size; i += 2) {
		if (!ReadLength(bits_a, &u32_len_stream[i + 0]))
			return false;
		if (!ReadLengthB(bits_b, &u32_len_stream[i + 1]))
			return false;
	}
	if (i < u32_len_stream_size) {
		if (!ReadLength(bits_a, &u32_len_stream[i + 0]))
			return false;
	}

	bits_a.p -= (24 - bits_a.bitpos) >> 3;
	bits_b.p += (24 - bits_b.bitpos) >> 3;

	if (bits_a.p != bits_b.p)
		return false;

	for (i = 0; i < packed_litlen_stream_size; i++) {
		uint32 v = packed_litlen_stream[i];
		if (v == 255)
			v = *u32_len_stream++ + 255;
		len_stream[i] = v + 3;
	}
	if (u32_len_stream != u32_len_stream_end)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//	Kraken
//-----------------------------------------------------------------------------

// Kraken decompression happens in two phases, first one decodes all the literals and copy lengths using huffman and second phase runs the copy loop. This holds the tables needed by stage 2.
struct KrakenLzTable {
	// Stream of (literal, match) pairs. The flag uint8 contains the length of the match, the length of the literal and whether to use a recent offset.
	uint8*	cmd_stream;
	int		cmd_stream_size;

	// Holds the actual distances in case we're not using a recent offset.
	int*	offs_stream;
	int		offs_stream_size;

	// Holds the sequence of literals. All literal copying happens from here.
	uint8*	lit_stream;
	int		lit_stream_size;

	// Holds the lengths that do not fit in the flag stream. Both literal lengths and match length are stored in the same array.
	int*	len_stream;
	int		len_stream_size;

	bool	Read(int mode, const uint8* src, const uint8* src_end, uint8* dst, int dst_size, int offset, uint8* scratch, uint8* scratch_end);
	bool	ProcessLzRuns_Type0(uint8* dst, uint8* dst_end, uint8* dst_start);
	bool	ProcessLzRuns_Type1(uint8* dst, uint8* dst_end, uint8* dst_start);

	bool ProcessLzRuns(int mode, uint8* dst, int dst_size, int offset) {
		return	mode == 1 ? ProcessLzRuns_Type1(dst + (offset == 0 ? 8 : 0), dst + dst_size, dst - offset)
			:	mode == 0 ? ProcessLzRuns_Type0(dst + (offset == 0 ? 8 : 0), dst + dst_size, dst - offset)
			:	false;
	}
};

bool KrakenLzTable::Read(int mode, const uint8* src, const uint8* src_end, uint8* dst, int dst_size, int offset, uint8* scratch, uint8* scratch_end) {
	int		decode_count, n;

	if (mode > 1)
		return false;

	if (src_end - src < 13)
		return false;

	if (offset == 0) {
		COPY_64(dst, src);
		dst += 8;
		src += 8;
	}

	if (*src & 0x80) {
		uint8 flag = *src++;
		if ((flag & 0xc0) != 0x80)
			return false; // reserved flag set

		return false; // excess bytes not supported
	}

	// Disable no copy optimization if source and dest overlap
	bool force_copy = dst <= src_end && src <= dst + dst_size;

	// Decode lit stream, bounded by dst_size
	uint8	*out = scratch;
	n = Kraken_DecodeBytes(&out, src, src_end, &decode_count, min(scratch_end - scratch, dst_size), force_copy, scratch, scratch_end);
	if (n < 0)
		return false;
	src		+= n;
	lit_stream = out;
	lit_stream_size = decode_count;
	scratch += decode_count;

	// Decode command stream, bounded by dst_size
	out	= scratch;
	n	= Kraken_DecodeBytes(&out, src, src_end, &decode_count, min(scratch_end - scratch, dst_size), force_copy, scratch, scratch_end);
	if (n < 0)
		return false;
	src		+= n;
	cmd_stream = out;
	cmd_stream_size = decode_count;
	scratch += decode_count;

	// Check if to decode the multistuff crap
	if (src_end - src < 3)
		return false;

	int		offs_scaling = 0;
	uint8*	packed_offs_stream_extra = NULL;
	uint8*	packed_offs_stream = scratch;

	if (src[0] & 0x80) {
		// uses the mode where distances are coded with 2 tables
		offs_scaling = src[0] - 127;
		src++;

		n = Kraken_DecodeBytes(&packed_offs_stream, src, src_end, &offs_stream_size, min(scratch_end - scratch, cmd_stream_size), false, scratch, scratch_end);
		if (n < 0)
			return false;
		src		+= n;
		scratch += offs_stream_size;

		if (offs_scaling != 1) {
			packed_offs_stream_extra = scratch;
			n = Kraken_DecodeBytes(&packed_offs_stream_extra, src, src_end, &decode_count, min(scratch_end - scratch, offs_stream_size), false, scratch, scratch_end);
			if (n < 0 || decode_count != offs_stream_size)
				return false;
			src		+= n;
			scratch += decode_count;
		}
	} else {
		// Decode packed offset stream, it's bounded by the command length.
		n = Kraken_DecodeBytes(&packed_offs_stream, src, src_end, &offs_stream_size, min(scratch_end - scratch, cmd_stream_size), false, scratch, scratch_end);
		if (n < 0)
			return false;
		src		+= n;
		scratch += offs_stream_size;
	}

	// Decode packed litlen stream. It's bounded by 1/4 of dst_size.
	uint8	*packed_len_stream = scratch;
	n = Kraken_DecodeBytes(&packed_len_stream, src, src_end, &len_stream_size, min(scratch_end - scratch, dst_size >> 2), false, scratch, scratch_end);
	if (n < 0)
		return false;
	src		+= n;
	scratch += len_stream_size;

	// Reserve memory for final dist stream
	scratch = align(scratch, 16);
	offs_stream = (int*)scratch;
	scratch += offs_stream_size * 4;

	// Reserve memory for final len stream
	scratch = align(scratch, 16);
	len_stream = (int*)scratch;
	scratch += len_stream_size * 4;

	if (scratch + 64 > scratch_end)
		return false;

	return Kraken_UnpackOffsets(src, src_end, packed_offs_stream, packed_offs_stream_extra, offs_stream_size, offs_scaling, packed_len_stream, len_stream_size, offs_stream, len_stream, 0, 0);
}


// Note: may access memory out of bounds on invalid input.
bool KrakenLzTable::ProcessLzRuns_Type0(uint8* dst, uint8* dst_end, uint8* dst_start) {
	const uint8*	cmd_stream		= this->cmd_stream;
	const uint8*	cmd_stream_end	= cmd_stream + this->cmd_stream_size;
	const int*		len_stream		= this->len_stream;
	const int*		len_stream_end	= len_stream + this->len_stream_size;
	const uint8*	lit_stream		= this->lit_stream;
	const uint8*	lit_stream_end	= lit_stream + this->lit_stream_size;
	const int*		offs_stream		= this->offs_stream;
	const int*		offs_stream_end	= offs_stream + this->offs_stream_size;
	int				last_offset = -8;
	int32			recent_offs[7];

	recent_offs[3] = -8;
	recent_offs[4] = -8;
	recent_offs[5] = -8;

	while (cmd_stream < cmd_stream_end) {
		uint32 f = *cmd_stream++;
		uint32 litlen = f & 3;
		uint32 offs_index = f >> 6;
		uint32 matchlen = (f >> 2) & 0xF;

		// use cmov
		uint32 next_long_length = *len_stream;
		const int* next_len_stream = len_stream + 1;

		len_stream = (litlen == 3) ? next_len_stream : len_stream;
		litlen = (litlen == 3) ? next_long_length : litlen;
		recent_offs[6] = *offs_stream;

		COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
		if (litlen > 8) {
			COPY_64_ADD(dst + 8, lit_stream + 8, &dst[last_offset + 8]);
			if (litlen > 16) {
				COPY_64_ADD(dst + 16, lit_stream + 16, &dst[last_offset + 16]);
				if (litlen > 24) {
					do {
						COPY_64_ADD(dst + 24, lit_stream + 24, &dst[last_offset + 24]);
						litlen -= 8;
						dst += 8;
						lit_stream += 8;
					} while (litlen > 24);
				}
			}
		}
		dst			+= litlen;
		lit_stream	+= litlen;

		int	offset = recent_offs[offs_index + 3];
		recent_offs[offs_index + 3] = recent_offs[offs_index + 2];
		recent_offs[offs_index + 2] = recent_offs[offs_index + 1];
		recent_offs[offs_index + 1] = recent_offs[offs_index + 0];
		recent_offs[3] = offset;
		last_offset = offset;

		offs_stream = (int*)((intptr_t)offs_stream + ((offs_index + 1) & 4));

		if ((uintptr_t)offset < (uintptr_t)(dst_start - dst))
			return false; // offset out of bounds

		const uint8* copyfrom = dst + offset;
		if (matchlen != 15) {
			COPY_64(dst, copyfrom);
			COPY_64(dst + 8, copyfrom + 8);
			dst += matchlen + 2;
		} else {
			matchlen = 14 + *len_stream++; // why is the value not 16 here, the above case copies up to 16 bytes.
			if ((uintptr_t)matchlen > (uintptr_t)(dst_end - dst))
				return false; // copy length out of bounds
			COPY_64(dst, copyfrom);
			COPY_64(dst + 8, copyfrom + 8);
			COPY_64(dst + 16, copyfrom + 16);
			do {
				COPY_64(dst + 24, copyfrom + 24);
				matchlen -= 8;
				dst += 8;
				copyfrom += 8;
			} while (matchlen > 24);
			dst += matchlen;
		}
	}

	// check for incorrect input
	if (offs_stream != offs_stream_end || len_stream != len_stream_end)
		return false;

	uint32	final_len = dst_end - dst;
	if (final_len != lit_stream_end - lit_stream)
		return false;

	if (final_len >= 8) {
		do {
			COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
			dst += 8, lit_stream += 8, final_len -= 8;
		} while (final_len >= 8);
	}
	if (final_len > 0) {
		do {
			*dst = *lit_stream++ + dst[last_offset];
		} while (dst++, --final_len);
	}
	return true;
}


// Note: may access memory out of bounds on invalid input.
bool KrakenLzTable::ProcessLzRuns_Type1(uint8* dst, uint8* dst_end, uint8* dst_start) {
	const uint8*	cmd_stream		= this->cmd_stream;
	const uint8*	cmd_stream_end	= cmd_stream + this->cmd_stream_size;
	const int*		len_stream		= this->len_stream;
	const int*		len_stream_end	= len_stream + this->len_stream_size;
	const uint8*	lit_stream		= this->lit_stream;
	const uint8*	lit_stream_end	= lit_stream + this->lit_stream_size;
	const int*		offs_stream		= this->offs_stream;
	const int*		offs_stream_end	= offs_stream + this->offs_stream_size;
	int32 recent_offs[7];

	recent_offs[3] = -8;
	recent_offs[4] = -8;
	recent_offs[5] = -8;

	while (cmd_stream < cmd_stream_end) {
		uint32 f = *cmd_stream++;
		uint32 litlen = f & 3;
		uint32 offs_index = f >> 6;
		uint32 matchlen = (f >> 2) & 0xF;

		// use cmov
		uint32 next_long_length = *len_stream;
		const int* next_len_stream = len_stream + 1;

		len_stream = (litlen == 3) ? next_len_stream : len_stream;
		litlen = (litlen == 3) ? next_long_length : litlen;
		recent_offs[6] = *offs_stream;

		COPY_64(dst, lit_stream);
		if (litlen > 8) {
			COPY_64(dst + 8, lit_stream + 8);
			if (litlen > 16) {
				COPY_64(dst + 16, lit_stream + 16);
				if (litlen > 24) {
					do {
						COPY_64(dst + 24, lit_stream + 24);
						litlen -= 8;
						dst += 8;
						lit_stream += 8;
					} while (litlen > 24);
				}
			}
		}
		dst += litlen;
		lit_stream += litlen;

		int	offset = recent_offs[offs_index + 3];
		recent_offs[offs_index + 3] = recent_offs[offs_index + 2];
		recent_offs[offs_index + 2] = recent_offs[offs_index + 1];
		recent_offs[offs_index + 1] = recent_offs[offs_index + 0];
		recent_offs[3] = offset;

		offs_stream = (int*)((intptr_t)offs_stream + ((offs_index + 1) & 4));

		if ((uintptr_t)offset < (uintptr_t)(dst_start - dst))
			return false; // offset out of bounds

		const uint8*	copyfrom = dst + offset;
		if (matchlen != 15) {
			COPY_64(dst, copyfrom);
			COPY_64(dst + 8, copyfrom + 8);
			dst += matchlen + 2;
		} else {
			matchlen = 14 + *len_stream++; // why is the value not 16 here, the above case copies up to 16 bytes.
			if ((uintptr_t)matchlen > (uintptr_t)(dst_end - dst))
				return false; // copy length out of bounds
			COPY_64(dst, copyfrom);
			COPY_64(dst + 8, copyfrom + 8);
			COPY_64(dst + 16, copyfrom + 16);
			do {
				COPY_64(dst + 24, copyfrom + 24);
				matchlen -= 8;
				dst += 8;
				copyfrom += 8;
			} while (matchlen > 24);
			dst += matchlen;
		}
	}

	// check for incorrect input
	if (offs_stream != offs_stream_end || len_stream != len_stream_end)
		return false;

	uint32	final_len = dst_end - dst;
	if (final_len != lit_stream_end - lit_stream)
		return false;

	if (final_len >= 64) {
		do {
			COPY_64_BYTES(dst, lit_stream);
			dst += 64, lit_stream += 64, final_len -= 64;
		} while (final_len >= 64);
	}
	if (final_len >= 8) {
		do {
			COPY_64(dst, lit_stream);
			dst += 8, lit_stream += 8, final_len -= 8;
		} while (final_len >= 8);
	}
	if (final_len > 0) {
		do {
			*dst++ = *lit_stream++;
		} while (--final_len);
	}
	return true;
}

//-----------------------------------------------------------------------------
//	Leviathan
//-----------------------------------------------------------------------------

struct LeviathanLzTable {
	int*	offs_stream;
	int		offs_stream_size;
	int*	len_stream;
	int		len_stream_size;
	uint8*	lit_stream[16];
	int		lit_stream_size[16];
	int		lit_stream_total;
	uint8*	multi_cmd_ptr[8];
	uint8*	multi_cmd_end[8];
	uint8*	cmd_stream;
	int		cmd_stream_size;

	bool Read(int chunk_type, const uint8* src, const uint8* src_end, uint8* dst, int dst_size, int offset, uint8* scratch, uint8* scratch_end);
	template<typename Mode, bool MultiCmd> bool ProcessLz(uint8* dst, uint8* dst_start, uint8* dst_end, uint8* window_base);
	bool ProcessLzRuns(int chunk_type, uint8* dst, int dst_size, int offset);
};

bool LeviathanLzTable::Read(int chunk_type, const uint8* src, const uint8* src_end, uint8* dst, int dst_size, int offset, uint8* scratch, uint8* scratch_end) {
	int decode_count, n;

	if (chunk_type > 5)
		return false;

	if (src_end - src < 13)
		return false;

	if (offset == 0) {
		COPY_64(dst, src);
		dst += 8;
		src += 8;
	}

	int		offs_scaling			= 0;
	uint8*	packed_offs_stream_extra = NULL;
	int		offs_stream_limit		= dst_size / 3;
	uint8*	packed_offs_stream		= scratch;

	if (!(src[0] & 0x80)) {
		// Decode packed offset stream, it's bounded by the command length.
		n	= Kraken_DecodeBytes(&packed_offs_stream, src, src_end, &offs_stream_size, min(scratch_end - scratch, offs_stream_limit), false, scratch, scratch_end);
		if (n < 0)
			return false;
		src		+= n;
		scratch += offs_stream_size;

	} else {
		// uses the mode where distances are coded with 2 tables and the transformation offs * scaling + low_bits
		offs_scaling = src[0] - 127;
		src++;
		n	= Kraken_DecodeBytes(&packed_offs_stream, src, src_end, &offs_stream_size, min(scratch_end - scratch, offs_stream_limit), false, scratch, scratch_end);
		if (n < 0)
			return false;
		src		+= n;
		scratch	+= offs_stream_size;

		if (offs_scaling != 1) {
			packed_offs_stream_extra = scratch;
			n = Kraken_DecodeBytes(&packed_offs_stream_extra, src, src_end, &decode_count, min(scratch_end - scratch, offs_stream_limit), false, scratch, scratch_end);
			if (n < 0 || decode_count != offs_stream_size)
				return false;
			src += n;
			scratch += decode_count;
		}
	}

	// Decode packed litlen stream. It's bounded by 1/5 of dst_size.
	uint8	*packed_len_stream = scratch;
	n = Kraken_DecodeBytes(&packed_len_stream, src, src_end, &len_stream_size, min(scratch_end - scratch, dst_size / 5), false, scratch, scratch_end);
	if (n < 0)
		return false;

	src		+= n;
	scratch	+= len_stream_size;

	// Reserve memory for final dist stream
	scratch = align(scratch, 16);
	offs_stream = (int*)scratch;
	scratch += offs_stream_size * 4;

	// Reserve memory for final len stream
	scratch = align(scratch, 16);
	len_stream = (int*)scratch;
	scratch += len_stream_size * 4;

	if (scratch > scratch_end)
		return false;

	if (chunk_type <= 1) {
		// Decode lit stream, bounded by dst_size
		uint8	*out = scratch;
		n = Kraken_DecodeBytes(&out, src, src_end, &decode_count, min(scratch_end - scratch, dst_size), true, scratch, scratch_end);
		if (n < 0)
			return false;
		src		+= n;
		lit_stream[0] = out;
		lit_stream_size[0] = decode_count;
	} else {
		int array_count = (chunk_type == 2) ? 2 :
			(chunk_type == 3) ? 4 : 16;
		n = Kraken_DecodeMultiArray(src, src_end, scratch, scratch_end, lit_stream, lit_stream_size, array_count, &decode_count, true, scratch, scratch_end);
		if (n < 0)
			return false;
		src		+= n;
	}
	scratch		+= decode_count;
	lit_stream_total = decode_count;

	if (src >= src_end)
		return false;

	if (!(src[0] & 0x80)) {
		// Decode command stream, bounded by dst_size
		uint8	*out = scratch;
		n = Kraken_DecodeBytes(&out, src, src_end, &decode_count, min(scratch_end - scratch, dst_size), true, scratch, scratch_end);
		if (n < 0)
			return false;
		src		+= n;
		cmd_stream = out;
		cmd_stream_size = decode_count;
		scratch += decode_count;

	} else {
		if (src[0] != 0x83)
			return false;
		src++;
		int multi_cmd_lens[8];
		n = Kraken_DecodeMultiArray(src, src_end, scratch, scratch_end, multi_cmd_ptr, multi_cmd_lens, 8, &decode_count, true, scratch, scratch_end);
		if (n < 0)
			return false;
		src		+= n;
		for (size_t i = 0; i < 8; i++)
			multi_cmd_end[i] = multi_cmd_ptr[i] + multi_cmd_lens[i];

		cmd_stream = NULL;
		cmd_stream_size = decode_count;
		scratch += decode_count;
	}

	if (dst_size > scratch_end - scratch)
		return false;


	return Kraken_UnpackOffsets(src, src_end, packed_offs_stream, packed_offs_stream_extra, offs_stream_size, offs_scaling, packed_len_stream, len_stream_size, offs_stream, len_stream, 0, 0);
}

struct LeviathanModeRaw {
	const uint8* lit_stream;

	force_inline LeviathanModeRaw(LeviathanLzTable* lzt, uint8* dst_start) : lit_stream(lzt->lit_stream[0]) {}

	force_inline bool CopyLiterals(uint32 cmd, uint8*& dst, const int*& len_stream, uint8* match_zone_end, size_t last_offset) {
		uint32 litlen = (cmd >> 3) & 3;
		// use cmov
		uint32 len_stream_value = *len_stream & 0xffffff;
		const int* next_len_stream = len_stream + 1;
		len_stream = (litlen == 3) ? next_len_stream : len_stream;
		litlen = (litlen == 3) ? len_stream_value : litlen;
		COPY_64(dst, lit_stream);
		if (litlen > 8) {
			COPY_64(dst + 8, lit_stream + 8);
			if (litlen > 16) {
				COPY_64(dst + 16, lit_stream + 16);
				if (litlen > 24) {
					if (litlen > match_zone_end - dst)
						return false;  // out of bounds
					do {
						COPY_64(dst + 24, lit_stream + 24);
						litlen -= 8, dst += 8, lit_stream += 8;
					} while (litlen > 24);
				}
			}
		}
		dst += litlen;
		lit_stream += litlen;
		return true;
	}

	force_inline void CopyFinalLiterals(uint32 final_len, uint8*& dst, size_t last_offset) {
		if (final_len >= 64) {
			do {
				COPY_64_BYTES(dst, lit_stream);
				dst += 64, lit_stream += 64, final_len -= 64;
			} while (final_len >= 64);
		}
		if (final_len >= 8) {
			do {
				COPY_64(dst, lit_stream);
				dst += 8, lit_stream += 8, final_len -= 8;
			} while (final_len >= 8);
		}
		if (final_len > 0) {
			do {
				*dst++ = *lit_stream++;
			} while (--final_len);
		}
	}
};

struct LeviathanModeSub {
	const uint8* lit_stream;

	force_inline LeviathanModeSub(LeviathanLzTable* lzt, uint8* dst_start) : lit_stream(lzt->lit_stream[0]) {}

	force_inline bool CopyLiterals(uint32 cmd, uint8*& dst, const int*& len_stream, uint8* match_zone_end, size_t last_offset) {
		uint32 litlen = (cmd >> 3) & 3;
		// use cmov
		uint32 len_stream_value = *len_stream & 0xffffff;
		const int* next_len_stream = len_stream + 1;
		len_stream = (litlen == 3) ? next_len_stream : len_stream;
		litlen = (litlen == 3) ? len_stream_value : litlen;
		COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
		if (litlen > 8) {
			COPY_64_ADD(dst + 8, lit_stream + 8, &dst[last_offset + 8]);
			if (litlen > 16) {
				COPY_64_ADD(dst + 16, lit_stream + 16, &dst[last_offset + 16]);
				if (litlen > 24) {
					if (litlen > match_zone_end - dst)
						return false;  // out of bounds
					do {
						COPY_64_ADD(dst + 24, lit_stream + 24, &dst[last_offset + 24]);
						litlen -= 8, dst += 8, lit_stream += 8;
					} while (litlen > 24);
				}
			}
		}
		dst += litlen;
		lit_stream += litlen;
		return true;
	}

	force_inline void CopyFinalLiterals(uint32 final_len, uint8*& dst, size_t last_offset) {
		if (final_len >= 8) {
			do {
				COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
				dst += 8, lit_stream += 8, final_len -= 8;
			} while (final_len >= 8);
		}
		if (final_len > 0) {
			do {
				*dst = *lit_stream++ + dst[last_offset];
			} while (dst++, --final_len);
		}
	}
};

struct LeviathanModeLamSub {
	const uint8* lit_stream, * lam_lit_stream;

	force_inline LeviathanModeLamSub(LeviathanLzTable* lzt, uint8* dst_start) : lit_stream(lzt->lit_stream[0]), lam_lit_stream(lzt->lit_stream[1]) {}

	force_inline bool CopyLiterals(uint32 cmd, uint8*& dst, const int*& len_stream, uint8* match_zone_end, size_t last_offset) {
		uint32 lit_cmd = cmd & 0x18;
		if (!lit_cmd)
			return true;

		uint32 litlen = lit_cmd >> 3;
		// use cmov
		uint32 len_stream_value = *len_stream & 0xffffff;
		const int* next_len_stream = len_stream + 1;
		len_stream = (litlen == 3) ? next_len_stream : len_stream;
		litlen = (litlen == 3) ? len_stream_value : litlen;

		if (litlen-- == 0)
			return false; // lamsub mode requires one literal

		dst[0] = *lam_lit_stream++ + dst[last_offset], dst++;

		COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
		if (litlen > 8) {
			COPY_64_ADD(dst + 8, lit_stream + 8, &dst[last_offset + 8]);
			if (litlen > 16) {
				COPY_64_ADD(dst + 16, lit_stream + 16, &dst[last_offset + 16]);
				if (litlen > 24) {
					if (litlen > match_zone_end - dst)
						return false;  // out of bounds
					do {
						COPY_64_ADD(dst + 24, lit_stream + 24, &dst[last_offset + 24]);
						litlen -= 8, dst += 8, lit_stream += 8;
					} while (litlen > 24);
				}
			}
		}
		dst += litlen;
		lit_stream += litlen;
		return true;
	}

	force_inline void CopyFinalLiterals(uint32 final_len, uint8*& dst, size_t last_offset) {
		dst[0] = *lam_lit_stream++ + dst[last_offset], dst++;
		final_len -= 1;

		if (final_len >= 8) {
			do {
				COPY_64_ADD(dst, lit_stream, &dst[last_offset]);
				dst += 8, lit_stream += 8, final_len -= 8;
			} while (final_len >= 8);
		}
		if (final_len > 0) {
			do {
				*dst = *lit_stream++ + dst[last_offset];
			} while (dst++, --final_len);
		}
	}
};

struct LeviathanModeSubAnd3 {
	enum { NUM = 4, MASK = NUM - 1 };
	const uint8* lit_stream[NUM];

	force_inline LeviathanModeSubAnd3(LeviathanLzTable* lzt, uint8* dst_start) {
		for (size_t i = 0; i != NUM; i++)
			lit_stream[i] = lzt->lit_stream[(-(intptr_t)dst_start + i) & MASK];
	}
	force_inline bool CopyLiterals(uint32 cmd, uint8*& dst, const int*& len_stream, uint8* match_zone_end, size_t last_offset) {
		uint32 lit_cmd = cmd & 0x18;

		if (lit_cmd == 0x18) {
			uint32 litlen = *len_stream++ & 0xffffff;
			if (litlen > match_zone_end - dst)
				return false;
			while (litlen) {
				*dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
				dst++, litlen--;
			}
		} else if (lit_cmd) {
			*dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
			dst++;
			if (lit_cmd == 0x10) {
				*dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
				dst++;
			}
		}
		return true;
	}

	force_inline void CopyFinalLiterals(uint32 final_len, uint8*& dst, size_t last_offset) {
		if (final_len > 0) {
			do {
				*dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
			} while (dst++, --final_len);
		}
	}
};

struct LeviathanModeSubAndF {
	enum { NUM = 16, MASK = NUM - 1 };
	const uint8* lit_stream[NUM];

	force_inline LeviathanModeSubAndF(LeviathanLzTable* lzt, uint8* dst_start) {
		for (size_t i = 0; i != NUM; i++)
			lit_stream[i] = lzt->lit_stream[(-(intptr_t)dst_start + i) & MASK];
	}
	force_inline bool CopyLiterals(uint32 cmd, uint8*& dst, const int*& len_stream, uint8* match_zone_end, size_t last_offset) {
		uint32 lit_cmd = cmd & 0x18;

		if (lit_cmd == 0x18) {
			uint32 litlen = *len_stream++ & 0xffffff;
			if (litlen > match_zone_end - dst)
				return false;
			while (litlen) {
				*dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
				dst++, litlen--;
			}
		} else if (lit_cmd) {
			*dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
			dst++;
			if (lit_cmd == 0x10) {
				*dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
				dst++;
			}
		}
		return true;
	}

	force_inline void CopyFinalLiterals(uint32 final_len, uint8*& dst, size_t last_offset) {
		if (final_len > 0) {
			do {
				*dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
			} while (dst++, --final_len);
		}
	}
};

struct LeviathanModeO1 {
	const uint8* lit_streams[16];
	uint8 next_lit[16];

	force_inline LeviathanModeO1(LeviathanLzTable* lzt, uint8* dst_start) {
		for (size_t i = 0; i != 16; i++) {
			uint8* p = lzt->lit_stream[i];
			next_lit[i] = *p;
			lit_streams[i] = p + 1;
		}
	}

	force_inline bool CopyLiterals(uint32 cmd, uint8*& dst, const int*& len_stream, uint8* match_zone_end, size_t last_offset) {
		uint32 lit_cmd = cmd & 0x18;

		if (lit_cmd == 0x18) {
			uint32 litlen = *len_stream++;
			if ((int32)litlen <= 0)
				return false;
			uint32 context = dst[-1];
			do {
				size_t slot = context >> 4;
				*dst++ = (context = next_lit[slot]);
				next_lit[slot] = *lit_streams[slot]++;
			} while (--litlen);
		} else if (lit_cmd) {
			// either 1 or 2
			uint32 context = dst[-1];
			size_t slot = context >> 4;
			*dst++ = (context = next_lit[slot]);
			next_lit[slot] = *lit_streams[slot]++;
			if (lit_cmd == 0x10) {
				slot = context >> 4;
				*dst++ = (context = next_lit[slot]);
				next_lit[slot] = *lit_streams[slot]++;
			}
		}
		return true;
	}

	force_inline void CopyFinalLiterals(uint32 final_len, uint8*& dst, size_t last_offset) {
		uint32 context = dst[-1];
		while (final_len) {
			size_t slot = context >> 4;
			*dst++ = (context = next_lit[slot]);
			next_lit[slot] = *lit_streams[slot]++;
			final_len--;
		}
	}
};

template<typename Mode, bool MultiCmd> bool LeviathanLzTable::ProcessLz(uint8* dst, uint8* dst_start, uint8* dst_end, uint8* window_base) {
	const uint8*	cmd_stream		= this->cmd_stream;
	const uint8*	cmd_stream_end	= cmd_stream + this->cmd_stream_size;
	const int*		len_stream		= this->len_stream;
	const int*		len_stream_end	= len_stream + this->len_stream_size;
	const int*		offs_stream		= this->offs_stream;
	const int*		offs_stream_end	= offs_stream + this->offs_stream_size;

	uint8*	match_zone_end = (dst_end - dst_start >= 16) ? dst_end - 16 : dst_start;
	size_t	offset = -8;

	int32	recent_offs[16];
	recent_offs[8] = recent_offs[9] = recent_offs[10] = recent_offs[11] = -8;
	recent_offs[12] = recent_offs[13] = recent_offs[14] = -8;

	Mode	mode(this, dst_start);

	uint32 cmd_stream_left;
	const uint8* multi_cmd_stream[8], **cmd_stream_ptr;
	if (MultiCmd) {
		for (size_t i = 0; i != 8; i++)
			multi_cmd_stream[i] = multi_cmd_ptr[(i - (uintptr_t)dst_start) & 7];
		cmd_stream_left = cmd_stream_size;
		cmd_stream_ptr	= &multi_cmd_stream[(uintptr_t)dst & 7];
		cmd_stream		= *cmd_stream_ptr;
	}

	for (;;) {
		uint32 cmd;

		if (!MultiCmd) {
			if (cmd_stream >= cmd_stream_end)
				break;
			cmd = *cmd_stream++;
		} else {
			if (cmd_stream_left == 0)
				break;
			cmd_stream_left--;
			cmd = *cmd_stream;
			*cmd_stream_ptr = cmd_stream + 1;
		}

		uint32 offs_index	= cmd >> 5;
		uint32 matchlen		= (cmd & 7) + 2;

		recent_offs[15] = *offs_stream;

		if (!mode.CopyLiterals(cmd, dst, len_stream, match_zone_end, offset))
			return false;

		offset = recent_offs[(size_t)offs_index + 8];

		// Permute the recent offsets table
		__m128i temp = _mm_loadu_si128((const __m128i*) & recent_offs[(size_t)offs_index + 4]);
		_mm_storeu_si128((__m128i*) & recent_offs[(size_t)offs_index + 1], _mm_loadu_si128((const __m128i*) & recent_offs[offs_index]));
		_mm_storeu_si128((__m128i*) & recent_offs[(size_t)offs_index + 5], temp);
		recent_offs[8] = (int32)offset;
		offs_stream += offs_index == 7;

		if ((uintptr_t)offset < (uintptr_t)(window_base - dst))
			return false;  // offset out of bounds

		const uint8* copyfrom = dst + offset;
		if (matchlen == 9) {
			if (len_stream >= len_stream_end)
				return false;  // len stream empty
			matchlen = *--len_stream_end + 6;
			COPY_64(dst, copyfrom);
			COPY_64(dst + 8, copyfrom + 8);
			uint8* next_dst = dst + matchlen;
			if (MultiCmd)
				cmd_stream = *(cmd_stream_ptr = &multi_cmd_stream[(uintptr_t)next_dst & 7]);
			if (matchlen > 16) {
				if (matchlen > (uintptr_t)(dst_end - 8 - dst))
					return false;  // no space in buf
				COPY_64(dst + 16, copyfrom + 16);
				do {
					COPY_64(dst + 24, copyfrom + 24);
					matchlen	-= 8;
					dst			+= 8;
					copyfrom	+= 8;
				} while (matchlen > 24);
			}
			dst = next_dst;

		} else {
			COPY_64(dst, copyfrom);
			dst += matchlen;
			if (MultiCmd)
				cmd_stream = *(cmd_stream_ptr = &multi_cmd_stream[(uintptr_t)dst & 7]);
		}
	}

	// check for incorrect input
	if (offs_stream != offs_stream_end || len_stream != len_stream_end)
		return false;

	// copy final literals
	if (dst < dst_end) {
		mode.CopyFinalLiterals(dst_end - dst, dst, offset);
	} else if (dst != dst_end) {
		return false;
	}
	return true;
}

bool LeviathanLzTable::ProcessLzRuns(int chunk_type, uint8* dst, int dst_size, int offset) {
	uint8* dst_cur		= dst + (offset == 0 ? 8 : 0);
	uint8* dst_end		= dst + dst_size;
	uint8* dst_start	= dst - offset;

	if (cmd_stream != NULL) {
		// single cmd mode
		switch (chunk_type) {
			case 0:	return ProcessLz<LeviathanModeSub, false>(dst_cur, dst, dst_end, dst_start);
			case 1:	return ProcessLz<LeviathanModeRaw, false>(dst_cur, dst, dst_end, dst_start);
			case 2:	return ProcessLz<LeviathanModeLamSub, false>(dst_cur, dst, dst_end, dst_start);
			case 3:	return ProcessLz<LeviathanModeSubAnd3, false>(dst_cur, dst, dst_end, dst_start);
			case 4:	return ProcessLz<LeviathanModeO1, false>(dst_cur, dst, dst_end, dst_start);
			case 5:	return ProcessLz<LeviathanModeSubAndF, false>(dst_cur, dst, dst_end, dst_start);
		}
	} else {
		// multi cmd mode
		switch (chunk_type) {
			case 0:	return ProcessLz<LeviathanModeSub, true>(dst_cur, dst, dst_end, dst_start);
			case 1:	return ProcessLz<LeviathanModeRaw, true>(dst_cur, dst, dst_end, dst_start);
			case 2:	return ProcessLz<LeviathanModeLamSub, true>(dst_cur, dst, dst_end, dst_start);
			case 3:	return ProcessLz<LeviathanModeSubAnd3, true>(dst_cur, dst, dst_end, dst_start);
			case 4:	return ProcessLz<LeviathanModeO1, true>(dst_cur, dst, dst_end, dst_start);
			case 5:	return ProcessLz<LeviathanModeSubAndF, true>(dst_cur, dst, dst_end, dst_start);
		}

	}
	return false;
}

//-----------------------------------------------------------------------------
//	Mermaid
//-----------------------------------------------------------------------------
// Mermaid/Selkie decompression also happens in two phases, just like in Kraken, but the match copier works differently.
// Both Mermaid and Selkie use the same on-disk format, only the compressor differs.
struct MermaidLzTable {
	// Flag stream. Format of flags:
	// Read flagbyte from |cmd_stream|
	// If flagbyte >= 24:
	//   flagbyte & 0x80 == 0 : Read from |off16_stream| into |recent_offs|.
	//                   != 0 : Don't read offset.
	//   flagbyte & 7 = Number of literals to copy first from |lit_stream|.
	//   (flagbyte >> 3) & 0xF = Number of bytes to copy from |recent_offs|.
	//
	//  If flagbyte == 0 :
	//    Read uint8 L from |length_stream|
	//    If L > 251: L += 4 * Read word from |length_stream|
	//    L += 64
	//    Copy L bytes from |lit_stream|.
	//
	//  If flagbyte == 1 :
	//    Read uint8 L from |length_stream|
	//    If L > 251: L += 4 * Read word from |length_stream|
	//    L += 91
	//    Copy L bytes from match pointed by next offset from |off16_stream|
	//
	//  If flagbyte == 2 :
	//    Read uint8 L from |length_stream|
	//    If L > 251: L += 4 * Read word from |length_stream|
	//    L += 29
	//    Copy L bytes from match pointed by next offset from |off32_stream|, 
	//    relative to start of block.
	//    Then prefetch |off32_stream[3]|
	//
	//  If flagbyte > 2:
	//    L = flagbyte + 5
	//    Copy L bytes from match pointed by next offset from |off32_stream|,
	//    relative to start of block.
	//    Then prefetch |off32_stream[3]|

	const uint8*	cmd_stream,		*cmd_stream_end;
	const uint8*	length_stream;
	const uint8*	lit_stream,		*lit_stream_end;
	const uint16*	off16_stream,	*off16_stream_end;
	uint32*			off32_stream,	*off32_stream_end;

	// Holds the offsets for the two chunks
	uint32*			off32_stream_1,	*off32_stream_2;
	uint32			off32_size_1,	off32_size_2;

	// Flag offsets for next 64k chunk.
	uint32			cmd_stream_2_offs, cmd_stream_2_offs_end;

	bool Read(int mode, const uint8* src, const uint8* src_end, uint8* dst, int dst_size, int64 offset, uint8* scratch, uint8* scratch_end);
	const uint8* Mode0(uint8* dst, size_t dst_size, uint8* dst_ptr_end, uint8* dst_start, const uint8* src_end, int32* saved_dist, size_t startoff);
	const uint8* Mode1(uint8* dst, size_t dst_size, uint8* dst_ptr_end, uint8* dst_start, const uint8* src_end, int32* saved_dist, size_t startoff);
	bool		ProcessLzRuns(int mode, uint8* dst, size_t dst_size, uint64 offset, const uint8* src, const uint8* src_end, uint8* dst_end);
};

int Mermaid_DecodeFarOffsets(const uint8* src, const uint8* src_end, uint32* output, size_t output_size, int64 offset) {
	const uint8* src_cur = src;

	if (offset < (0xC00000 - 1)) {
		for (int i = 0; i != output_size; i++) {
			if (src_end - src_cur < 3)
				return -1;
			uint32 off = src_cur[0] | src_cur[1] << 8 | src_cur[2] << 16;
			src_cur += 3;
			output[i] = off;
			if (off > offset)
				return -1;
		}
		return src_cur - src;
	}

	for (int i = 0; i != output_size; i++) {
		if (src_end - src_cur < 3)
			return -1;
		uint32 off = src_cur[0] | src_cur[1] << 8 | src_cur[2] << 16;
		src_cur += 3;

		if (off >= 0xc00000) {
			if (src_cur == src_end)
				return -1;
			off += *src_cur++ << 22;
		}
		output[i] = off;
		if (off > offset)
			return -1;
	}
	return src_cur - src;
}

void Mermaid_CombineOffs16(uint16* dst, size_t size, const uint8* lo, const uint8* hi) {
	for (size_t i = 0; i != size; i++)
		dst[i] = lo[i] + hi[i] * 256;
}

bool MermaidLzTable::Read(int mode, const uint8* src, const uint8* src_end, uint8* dst, int dst_size, int64 offset, uint8* scratch, uint8* scratch_end) {
	int decode_count, n;

	if (mode > 1)
		return false;

	if (src_end - src < 10)
		return false;

	if (offset == 0) {
		COPY_64(dst, src);
		dst += 8;
		src += 8;
	}

	// Decode lit stream
	uint8* out = scratch;
	n = Kraken_DecodeBytes(&out, src, src_end, &decode_count, min(scratch_end - scratch, dst_size), false, scratch, scratch_end);
	if (n < 0)
		return false;

	src += n;
	lit_stream		= out;
	lit_stream_end	= out + decode_count;
	scratch += decode_count;

	// Decode flag stream
	out = scratch;
	n = Kraken_DecodeBytes(&out, src, src_end, &decode_count, min(scratch_end - scratch, dst_size), false, scratch, scratch_end);
	if (n < 0)
		return false;
	src += n;
	cmd_stream		= out;
	cmd_stream_end	= out + decode_count;
	scratch += decode_count;

	cmd_stream_2_offs_end = decode_count;
	if (dst_size <= 0x10000) {
		cmd_stream_2_offs = decode_count;
	} else {
		if (src_end - src < 2)
			return false;
		cmd_stream_2_offs = *(uint16*)src;
		src += 2;
		if (cmd_stream_2_offs > cmd_stream_2_offs_end)
			return false;
	}

	if (src_end - src < 2)
		return false;

	int off16_count = *(uint16*)src;
	if (off16_count == 0xffff) {
		// off16 is entropy coded
		int off16_lo_count, off16_hi_count;
		src += 2;
		uint8* off16_hi = scratch;
		n = Kraken_DecodeBytes(&off16_hi, src, src_end, &off16_hi_count, min(scratch_end - scratch, dst_size >> 1), false, scratch, scratch_end);
		if (n < 0)
			return false;
		src += n;
		scratch += off16_hi_count;

		uint8	*off16_lo = scratch;
		n = Kraken_DecodeBytes(&off16_lo, src, src_end, &off16_lo_count, min(scratch_end - scratch, dst_size >> 1), false, scratch, scratch_end);
		if (n < 0)
			return false;
		src += n;
		scratch += off16_lo_count;

		if (off16_lo_count != off16_hi_count)
			return false;
		scratch = align(scratch, 2);
		off16_stream = (uint16*)scratch;
		if (scratch + off16_lo_count * 2 > scratch_end)
			return false;
		scratch += off16_lo_count * 2;
		off16_stream_end = (uint16*)scratch;
		Mermaid_CombineOffs16((uint16*)off16_stream, off16_lo_count, off16_lo, off16_hi);

	} else {
		off16_stream = (uint16*)(src + 2);
		src += 2 + off16_count * 2;
		off16_stream_end = (uint16*)src;
	}

	if (src_end - src < 3)
		return false;

	uint32	tmp = src[0] | src[1] << 8 | src[2] << 16;
	src += 3;

	if (tmp != 0) {
		off32_size_1 = tmp >> 12;
		off32_size_2 = tmp & 0xFFF;
		if (off32_size_1 == 4095) {
			if (src_end - src < 2)
				return false;
			off32_size_1 = *(uint16*)src;
			src += 2;
		}
		if (off32_size_2 == 4095) {
			if (src_end - src < 2)
				return false;
			off32_size_2 = *(uint16*)src;
			src += 2;
		}
		if (scratch + 4 * (off32_size_2 + off32_size_1) + 64 > scratch_end)
			return false;

		scratch = align(scratch, 4);

		off32_stream_1 = (uint32*)scratch;
		scratch += off32_size_1 * 4;
		// store dummy bytes after for prefetcher.
		((uint64*)scratch)[0] = 0;
		((uint64*)scratch)[1] = 0;
		((uint64*)scratch)[2] = 0;
		((uint64*)scratch)[3] = 0;
		scratch += 32;

		off32_stream_2 = (uint32*)scratch;
		scratch += off32_size_2 * 4;
		// store dummy bytes after for prefetcher.
		((uint64*)scratch)[0] = 0;
		((uint64*)scratch)[1] = 0;
		((uint64*)scratch)[2] = 0;
		((uint64*)scratch)[3] = 0;
		scratch += 32;

		n = Mermaid_DecodeFarOffsets(src, src_end, off32_stream_1, off32_size_1, offset);
		if (n < 0)
			return false;
		src += n;

		n = Mermaid_DecodeFarOffsets(src, src_end, off32_stream_2, off32_size_2, offset + 0x10000);
		if (n < 0)
			return false;
		src += n;

	} else {
		if (scratch_end - scratch < 32)
			return false;
		off32_size_1 = 0;
		off32_size_2 = 0;
		off32_stream_1 = (uint32*)scratch;
		off32_stream_2 = (uint32*)scratch;
		// store dummy bytes after for prefetcher.
		((uint64*)scratch)[0] = 0;
		((uint64*)scratch)[1] = 0;
		((uint64*)scratch)[2] = 0;
		((uint64*)scratch)[3] = 0;
	}
	length_stream = src;
	return true;
}

const uint8* MermaidLzTable::Mode0(uint8* dst, size_t dst_size, uint8* dst_ptr_end, uint8* dst_start, const uint8* src_end, int32* saved_dist, size_t startoff) {
	const uint8*	cmd_stream			= this->cmd_stream;
	const uint8*	cmd_stream_end		= this->cmd_stream_end;
	const uint8*	length_stream		= this->length_stream;
	const uint8*	lit_stream			= this->lit_stream;
	const uint8*	lit_stream_end		= this->lit_stream_end;
	const uint16*	off16_stream		= this->off16_stream;
	const uint16*	off16_stream_end	= this->off16_stream_end;
	const uint32*	off32_stream		= this->off32_stream;
	const uint32*	off32_stream_end	= this->off32_stream_end;

	intptr_t		recent_offs	= *saved_dist;
	const uint8*	dst_begin	= dst;
	const uint8*	dst_end		= dst + dst_size;

	dst += startoff;

	while (cmd_stream < cmd_stream_end) {
		const uint8* match;
		intptr_t length;
		uintptr_t cmd = *cmd_stream++;
		if (cmd >= 24) {
			intptr_t new_dist = *off16_stream;
			uintptr_t use_distance = (uintptr_t)(cmd >> 7) - 1;
			uintptr_t litlen = (cmd & 7);
			COPY_64_ADD(dst, lit_stream, &dst[recent_offs]);
			dst += litlen;
			lit_stream += litlen;
			recent_offs ^= use_distance & (recent_offs ^ -new_dist);
			off16_stream = (uint16*)((uintptr_t)off16_stream + (use_distance & 2));
			match = dst + recent_offs;
			COPY_64(dst, match);
			COPY_64(dst + 8, match + 8);
			dst += (cmd >> 3) & 0xF;

		} else if (cmd > 2) {
			length = cmd + 5;
			if (off32_stream == off32_stream_end)
				return NULL;

			match		= dst_begin - *off32_stream++;
			recent_offs	= (match - dst);
			if (dst_end - dst < length)
				return NULL;

			COPY_64(dst, match);
			COPY_64(dst + 8, match + 8);
			COPY_64(dst + 16, match + 16);
			COPY_64(dst + 24, match + 24);
			dst += length;
			_mm_prefetch((char*)dst_begin - off32_stream[3], _MM_HINT_T0);

		} else if (cmd == 0) {
			if (src_end - length_stream == 0)
				return NULL;

			length = *length_stream;
			if (length > 251) {
				if (src_end - length_stream < 3)
					return NULL;

				length			+= (size_t) * (uint16*)(length_stream + 1) * 4;
				length_stream	+= 2;
			}
			length_stream += 1;

			length += 64;
			if (dst_end - dst < length ||
				lit_stream_end - lit_stream < length)
				return NULL;

			do {
				COPY_64_ADD(dst, lit_stream, &dst[recent_offs]);
				COPY_64_ADD(dst + 8, lit_stream + 8, &dst[recent_offs + 8]);
				dst			+= 16;
				lit_stream	+= 16;
				length		-= 16;
			} while (length > 0);

			dst			+= length;
			lit_stream	+= length;

		} else if (cmd == 1) {
			if (src_end - length_stream == 0)
				return NULL;

			length = *length_stream;
			if (length > 251) {
				if (src_end - length_stream < 3)
					return NULL;

				length			+= (size_t) * (uint16*)(length_stream + 1) * 4;
				length_stream	+= 2;
			}
			length_stream	+= 1;
			length			+= 91;
			if (off16_stream == off16_stream_end)
				return NULL;

			match = dst - *off16_stream++;
			recent_offs = (match - dst);
			do {
				COPY_64(dst, match);
				COPY_64(dst + 8, match + 8);
				dst		+= 16;
				match	+= 16;
				length	-= 16;
			} while (length > 0);
			dst += length;

		} else /* flag == 2 */ {
			if (src_end - length_stream == 0)
				return NULL;

			length = *length_stream;
			if (length > 251) {
				if (src_end - length_stream < 3)
					return NULL;

				length			+= (size_t) * (uint16*)(length_stream + 1) * 4;
				length_stream	+= 2;
			}
			length_stream	+= 1;
			length			+= 29;
			if (off32_stream == off32_stream_end)
				return NULL;

			match = dst_begin - *off32_stream++;
			recent_offs = (match - dst);
			do {
				COPY_64(dst, match);
				COPY_64(dst + 8, match + 8);
				dst		+= 16;
				match	+= 16;
				length	-= 16;
			} while (length > 0);

			dst += length;
			_mm_prefetch((char*)dst_begin - off32_stream[3], _MM_HINT_T0);
		}
	}

	intptr_t	length = dst_end - dst;
	if (length >= 8) {
		do {
			COPY_64_ADD(dst, lit_stream, &dst[recent_offs]);
			dst			+= 8;
			lit_stream	+= 8;
			length		-= 8;
		} while (length >= 8);
	}
	if (length > 0) {
		do {
			*dst = *lit_stream++ + dst[recent_offs];
			dst++;
		} while (--length);
	}

	*saved_dist			= (int32)recent_offs;
	this->length_stream	= length_stream;
	this->off16_stream	= off16_stream;
	this->lit_stream	= lit_stream;
	return length_stream;
}

const uint8* MermaidLzTable::Mode1(uint8* dst, size_t dst_size, uint8* dst_ptr_end, uint8* dst_start, const uint8* src_end, int32* saved_dist, size_t startoff) {
	const uint8*	cmd_stream			= this->cmd_stream;
	const uint8*	cmd_stream_end		= this->cmd_stream_end;
	const uint8*	length_stream		= this->length_stream;
	const uint8*	lit_stream			= this->lit_stream;
	const uint8*	lit_stream_end		= this->lit_stream_end;
	const uint16*	off16_stream		= this->off16_stream;
	const uint16*	off16_stream_end	= this->off16_stream_end;
	const uint32*	off32_stream		= this->off32_stream;
	const uint32*	off32_stream_end	= this->off32_stream_end;

	intptr_t		recent_offs		= *saved_dist;
	const uint8*	dst_begin		= dst;
	const uint8*	dst_end			= dst + dst_size;

	dst += startoff;

	while (cmd_stream < cmd_stream_end) {
		const uint8*	match;
		intptr_t		length;
		uintptr_t		flag = *cmd_stream++;

		if (flag >= 24) {
			intptr_t	new_dist = *off16_stream;
			uintptr_t	use_distance = (uintptr_t)(flag >> 7) - 1;
			uintptr_t	litlen = (flag & 7);
			COPY_64(dst, lit_stream);
			dst			+= litlen;
			lit_stream	+= litlen;
			recent_offs ^= use_distance & (recent_offs ^ -new_dist);
			off16_stream = (uint16*)((uintptr_t)off16_stream + (use_distance & 2));
			match = dst + recent_offs;
			COPY_64(dst, match);
			COPY_64(dst + 8, match + 8);
			dst += (flag >> 3) & 0xF;

		} else if (flag > 2) {
			length = flag + 5;
			if (off32_stream == off32_stream_end)
				return NULL;

			match		= dst_begin - *off32_stream++;
			recent_offs	= (match - dst);
			if (dst_end - dst < length)
				return NULL;

			COPY_64(dst, match);
			COPY_64(dst + 8, match + 8);
			COPY_64(dst + 16, match + 16);
			COPY_64(dst + 24, match + 24);
			dst += length;
			_mm_prefetch((char*)dst_begin - off32_stream[3], _MM_HINT_T0);

		} else if (flag == 0) {
			if (src_end - length_stream == 0)
				return NULL;

			length = *length_stream;
			if (length > 251) {
				if (src_end - length_stream < 3)
					return NULL;

				length			+= (size_t) * (uint16*)(length_stream + 1) * 4;
				length_stream	+= 2;
			}
			length_stream += 1;

			length += 64;
			if (dst_end - dst < length ||
				lit_stream_end - lit_stream < length)
				return NULL;

			do {
				COPY_64(dst, lit_stream);
				COPY_64(dst + 8, lit_stream + 8);
				dst			+= 16;
				lit_stream	+= 16;
				length		-= 16;
			} while (length > 0);

			dst			+= length;
			lit_stream	+= length;

		} else if (flag == 1) {
			if (src_end - length_stream == 0)
				return NULL;

			length = *length_stream;
			if (length > 251) {
				if (src_end - length_stream < 3)
					return NULL;

				length			+= (size_t) * (uint16*)(length_stream + 1) * 4;
				length_stream	+= 2;
			}
			length_stream	+= 1;
			length			+= 91;
			if (off16_stream == off16_stream_end)
				return NULL;

			match		= dst - *off16_stream++;
			recent_offs	= (match - dst);
			do {
				COPY_64(dst, match);
				COPY_64(dst + 8, match + 8);
				dst		+= 16;
				match	+= 16;
				length	-= 16;
			} while (length > 0);

			dst += length;

		} else /* flag == 2 */ {
			if (src_end - length_stream == 0)
				return NULL;

			length = *length_stream;
			if (length > 251) {
				if (src_end - length_stream < 3)
					return NULL;

				length			+= (size_t) * (uint16*)(length_stream + 1) * 4;
				length_stream	+= 2;
			}
			length_stream	+= 1;
			length			+= 29;

			if (off32_stream == off32_stream_end)
				return NULL;

			match		= dst_begin - *off32_stream++;
			recent_offs	= (match - dst);

			do {
				COPY_64(dst, match);
				COPY_64(dst + 8, match + 8);
				dst		+= 16;
				match	+= 16;
				length	-= 16;
			} while (length > 0);
			dst += length;

			_mm_prefetch((char*)dst_begin - off32_stream[3], _MM_HINT_T0);
		}
	}

	intptr_t	length = dst_end - dst;
	if (length >= 8) {
		do {
			COPY_64(dst, lit_stream);
			dst			+= 8;
			lit_stream	+= 8;
			length		-= 8;
		} while (length >= 8);
	}
	if (length > 0) {
		do {
			*dst++ = *lit_stream++;
		} while (--length);
	}

	*saved_dist			= (int32)recent_offs;
	this->length_stream	= length_stream;
	this->off16_stream	= off16_stream;
	this->lit_stream	= lit_stream;
	return length_stream;
}

bool MermaidLzTable::ProcessLzRuns(int mode, uint8* dst, size_t dst_size, uint64 offset, const uint8* src, const uint8* src_end, uint8* dst_end) {
	uint8* dst_start = dst - offset;
	int32 saved_dist = -8;
	const uint8* src_cur;

	for (int iteration = 0; iteration != 2; iteration++) {
		size_t dst_size_cur = dst_size;
		if (dst_size_cur > 0x10000) dst_size_cur = 0x10000;

		if (iteration == 0) {
			off32_stream		= off32_stream_1;
			off32_stream_end	= off32_stream_1 + off32_size_1 * 4;
			cmd_stream_end		= cmd_stream + cmd_stream_2_offs;
		} else {
			off32_stream		= off32_stream_2;
			off32_stream_end	= off32_stream_2 + off32_size_2 * 4;
			cmd_stream_end		= cmd_stream + cmd_stream_2_offs_end;
			cmd_stream			+= cmd_stream_2_offs;
		}

		src_cur = mode == 0
			? Mode0(dst, dst_size_cur, dst_end, dst_start, src_end, &saved_dist, offset == 0 && iteration == 0 ? 8 : 0)
			: Mode1(dst, dst_size_cur, dst_end, dst_start, src_end, &saved_dist, offset == 0 && iteration == 0 ? 8 : 0);

		if (!src_cur)
			return false;

		dst += dst_size_cur;
		dst_size -= dst_size_cur;
		if (dst_size == 0)
			break;
	}

	return src_cur == src_end;
}

//-----------------------------------------------------------------------------
//	OodleDecoder
//-----------------------------------------------------------------------------

// Header in front of each 256k block
struct OodleHeader {
	enum TYPE {
		LZNA		= 5,
		Kraken		= 6,
		Mermaid		= 10,
		Bitknit		= 11,
		Leviathan	= 12,
	};
	TYPE	decoder_type;
	bool	restart_decoder;
	bool	uncompressed;
	bool	use_checksums;

	bool	is_kraken() const {
		return decoder_type == Kraken || decoder_type == Mermaid || decoder_type == Leviathan;
	}

	const uint8* parse(const uint8* p) {
		int b = p[0];
		if ((b & 0xF) != 0xC || (b & 0x30) != 0)
			return nullptr;

		restart_decoder = !!(b & (1 << 7));
		uncompressed	= !!(b & (1 << 6));
		b = p[1];
		decoder_type	= TYPE(b & 0x7F);
		use_checksums	= !!(b & (1 << 7));
		if (decoder_type != 6 && decoder_type != 10 && decoder_type != 5 && decoder_type != 11 && decoder_type != 12)
			return nullptr;
		return p + 2;
	}
};

// Additional header in front of each 256k block ("quantum").
struct QuantumHeader {
	uint32	compressed_size;		// The compressed size of this quantum. If this value is 0 it means the quantum is a special quantum such as memset.
	uint32	checksum;				// If checksums are enabled, holds the checksum.
	uint8	flags;
	uint32	whole_match_distance;	// Whether the whole block matched a previous block

	const uint8* parseKraken(const uint8* p, bool use_checksum) {
		uint32 v	= (p[0] << 16) | (p[1] << 8) | p[2];
		uint32 size	= v & 0x3FFFF;

		if (size != 0x3ffff) {
			compressed_size = size + 1;
			flags = v >> 18;
			if (use_checksum) {
				checksum = (p[3] << 16) | (p[4] << 8) | p[5];
				return p + 6;
			}
			return p + 3;
		}
		switch (v >> 18) {
			case 1:		// memset
				checksum				= p[3];
				compressed_size			= 0;
				whole_match_distance	= 0;
				return p + 4;

			default:
				return nullptr;
		}
	}

	const uint8* parseLZNA(const uint8* p, bool use_checksum, int raw_len) {
		uint32 v	= (p[0] << 8) | p[1];
		uint32 size	= v & 0x3FFF;
		
		if (size != 0x3fff) {
			compressed_size = size + 1;
			flags			= v >> 14;
			if (use_checksum) {
				checksum = (p[2] << 16) | (p[3] << 8) | p[4];
				return p + 5;
			}
			return p + 2;
		}

		switch (v >> 14) {
			case 0: {
				compressed_size = 0;
				p		+= 2;
				uint32	v = *(uint16be*)p;
				if (v < 0x8000) {
					uint32 x = 0, b, pos = 0;
					for (;;) {
						b = p[2];
						p += 1;
						if (b & 0x80)
							break;
						x	+= (b + 0x80) << pos;
						pos += 7;
					}
					x += (b - 128) << pos;
					whole_match_distance	= 0x8000 + v + (x << 15) + 1;
				} else {
					whole_match_distance	= v - 0x8000 + 1;
				}
				return p + 2;
			}
			case 1:			// memset
				checksum		= p[2];
				compressed_size	= 0;
				whole_match_distance = 0;
				return p + 3;

			case 2:			// uncompressed
				compressed_size = raw_len;
				return p + 2;

			default:
				return nullptr;
		}
	}
};

struct OodleDecoder {
	int			src_used, dst_used;	// Updated after the |*_DecodeStep| function completes to hold the number of bytes read and written
	temp_block	scratch;			// Pointer to a 256k buffer that holds the intermediate state in between decode phase 1 and 2
	OodleHeader hdr;

	OodleDecoder() : scratch(0x6C000) {}
	bool DecodeStep(uint8* dst_start, int offset, size_t dst_bytes_left_in, const uint8* src, size_t src_bytes_left);

};

bool OodleDecoder::DecodeStep(uint8* dst_start, int offset, size_t dst_bytes_left_in, const uint8* src, size_t src_bytes_left) {
	const uint8* src_in = src;
	const uint8* src_end = src + src_bytes_left;

	if ((offset & 0x3FFFF) == 0) {
		src = hdr.parse(src);
		if (!src)
			return false;
	}

	bool	is_kraken_decoder	= hdr.is_kraken();
	int		dst_bytes_left		= (int)min(is_kraken_decoder ? 0x40000 : 0x4000, dst_bytes_left_in);

	if (hdr.uncompressed) {
		if (src_end - src < dst_bytes_left) {
			src_used = dst_used = 0;
			return true;
		}
		memmove(dst_start + offset, src, dst_bytes_left);
		src_used = (src - src_in) + dst_bytes_left;
		dst_used = dst_bytes_left;
		return true;
	}

	QuantumHeader qhdr;
	src = is_kraken_decoder
		? qhdr.parseKraken(src, hdr.use_checksums)
		: qhdr.parseLZNA(src, hdr.use_checksums, dst_bytes_left);

	if (!src || src > src_end)
		return false;

	// Too few bytes in buffer to make any progress?
	if (src_end - src < qhdr.compressed_size) {
		src_used = dst_used = 0;
		return true;
	}

	if (qhdr.compressed_size > (uint32)dst_bytes_left)
		return false;

	if (qhdr.compressed_size == 0) {
		if (qhdr.whole_match_distance) {
			if (qhdr.whole_match_distance > (uint32)offset)
				return false;

			uint8* d	= dst_start + offset;
			size_t i	= 0;
			uint8* s	= d - qhdr.whole_match_distance;
			if (qhdr.whole_match_distance >= 8) {
				for (; i + 8 <= dst_bytes_left; i += 8)
					*(uint64*)(d + i) = *(uint64*)(s + i);
			}
			for (; i < dst_bytes_left; i++)
				d[i] = s[i];

		} else {
			memset(dst_start + offset, qhdr.checksum, dst_bytes_left);
		}

		src_used = src - src_in;
		dst_used = dst_bytes_left;
		return true;
	}

	if (hdr.use_checksums && (Kraken_GetCrc(src, qhdr.compressed_size) & 0xFFFFFF) != qhdr.checksum)
		return false;

	if (qhdr.compressed_size == dst_bytes_left) {
		memmove(dst_start + offset, src, dst_bytes_left);
		src_used = src - src_in + dst_bytes_left;
		dst_used = dst_bytes_left;
		return true;
	}

	uint8* dst		= dst_start + offset;
	uint8* dst_end	= dst + dst_bytes_left;
	src_end = src + qhdr.compressed_size;

	switch (hdr.decoder_type) {
		case OodleHeader::LZNA: {
			if (hdr.restart_decoder) {
				hdr.restart_decoder = false;
				new(placement(scratch)) LznaState;
			}
			int	n = ((LznaState*)scratch)->DecodeQuantum(dst, dst_end, dst_start, src, src_end);
			if (n != qhdr.compressed_size)
				return false;
			src += n;
			break;
		}

		case OodleHeader::Bitknit: {
			if (hdr.restart_decoder) {
				hdr.restart_decoder = false;
				new(placement(scratch)) BitknitState;
			}
			int	n = (int)((BitknitState*)scratch)->DecodeQuantum(src, src_end, dst, dst_end, dst_start);
			if (n != qhdr.compressed_size)
				return false;
			src += n;
			break;
		}

		default: {
			while (dst_end != dst) {
				int	dst_count = min(dst_end - dst, 0x20000);
				if (src_end - src < 4)
					return false;

				int	chunkhdr = src[2] | src[1] << 8 | src[0] << 16;

				if (!(chunkhdr & 0x800000)) {
					// Stored as entropy without any match copying
					uint8*	out = dst;
					int		written_bytes;
					int		src_used = Kraken_DecodeBytes(&out, src, src_end, &written_bytes, dst_count, false, scratch, scratch.end());
					if (src_used < 0 || written_bytes != dst_count)
						return false;
					
					src += src_used;

				} else {
					src			+= 3;
					int	src_used	= chunkhdr & 0x7FFFF;
					int	mode		= (chunkhdr >> 19) & 0xF;
					const uint8	*src_end_chunk	= src + src_used;
					if (src_end_chunk > src_end)
						return false;

					if (src_used < dst_count) {
						uint64		offset	= dst - dst_start;
						switch (hdr.decoder_type) {
							case OodleHeader::Kraken: {
								size_t	scratch_usage = min(min(3 * dst_count + 32 + 0xd000, 0x6C000), scratch.size());
								if (scratch_usage < sizeof(KrakenLzTable)
									|| !((KrakenLzTable*)scratch)->Read(mode, src, src_end_chunk, dst, dst_count, offset, scratch + sizeof(KrakenLzTable), scratch + scratch_usage)
									|| !((KrakenLzTable*)scratch)->ProcessLzRuns(mode, dst, dst_count, offset)
									)
									return false;
								break;
							}
							case OodleHeader::Leviathan: {
								size_t	scratch_usage = min(min(3 * dst_count + 32 + 0xd000, 0x6C000), scratch.size());
								if (scratch_usage < sizeof(LeviathanLzTable)
									|| !((LeviathanLzTable*)scratch)->Read(mode, src, src_end_chunk, dst, dst_count, offset, scratch + sizeof(LeviathanLzTable), scratch + scratch_usage)
									|| !((LeviathanLzTable*)scratch)->ProcessLzRuns(mode, dst, dst_count, offset)
									)
									return false;
								break;
							}
							case OodleHeader::Mermaid: {
								size_t	scratch_usage = min(2 * dst_count + 32, 0x40000);
								auto	lz = (MermaidLzTable*)scratch;
								if (!lz->Read(mode, src, src_end_chunk, dst, dst_count, offset, scratch + sizeof(MermaidLzTable), scratch + scratch_usage)
									||  !lz->ProcessLzRuns(mode, dst, dst_count, offset, src, src_end_chunk, dst_end)
									)
									return false;
								break;
							}
							default:
								return false;
						}


					} else if (src_used > dst_count || mode != 0) {
						return false;

					} else {
						memmove(dst, src, dst_count);
					}
					src = src_end_chunk;
				}
				dst += dst_count;
			}
			break;
		}
	}

	src_used = src - src_in;
	dst_used = dst_bytes_left;
	return true;
}

int oodle::decompress(const uint8* src, size_t src_len, uint8* dst, size_t dst_len) {
	OodleDecoder dec;
	int offset = 0;
	while (dst_len != 0) {
		if (!dec.DecodeStep(dst, offset, dst_len, src, src_len))
			return -1;

		if (dec.src_used == 0)
			return -1;

		src		+= dec.src_used;
		src_len	-= dec.src_used;
		dst_len	-= dec.dst_used;
		offset	+= dec.dst_used;
	}
	if (src_len != 0)
		return -1;

	return offset;
}
