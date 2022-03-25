#include "base/vector.h"
#include "etc.h"

using namespace iso;

static const uint8 codebookRGB[][2] = {
	{  2,   8 },
	{  5,  17 },
	{  9,  29 },
	{ 13,  42 },
	{ 18,  60 },
	{ 24,  80 },
	{ 33, 106 },
	{ 47, 183 },
};

static const uint8 codebookA[][4] = {
	{2, 5, 8, 14},	//0	
	{2, 6, 9, 12},	//1	
	{1, 4, 7, 12},	//2	
	{1, 3, 5, 12},	//3	
	{2, 5, 7, 11},	//4	
	{2, 6, 8, 10},	//5	
	{3, 6, 7, 10},	//6	
	{2, 4, 7, 10},	//7	
	{1, 5, 7,  9},	//8	
	{1, 4, 7,  9},	//9	
	{1, 3, 7,  9},	//10	
	{1, 4, 6,  9},	//11	
	{2, 3, 6,  9},	//12	
	{0, 1, 2,  9},	//13	
	{3, 5, 7,  8},	//14	
	{2, 4, 6,  8},	//15	
};

static const uint8 modifier_table[8] = {
	3, 6, 11, 16, 23, 32, 41, 64
};

template<typename P> static P MaskedAverage(const P *in, uint32 mask) {
	if (!mask)
		return zero;
	decltype(*in + *in)	t = 0;
	uint32	n = 0;
	for (; mask; mask >>= 1, ++in) {
		if (mask & 1) {
			t += *in;
			++n;
		}
	}
	return (t * 2 + n) / (n * 2);
}

static uint8x4 MaskedAverage(const uint8x4 *in, uint32 mask) {
	if (!mask)
		return 0;
	uint16x4	t = 0;
	uint16		n = 0;
	for (; mask; mask >>= 1, ++in) {
		if (mask & 1) {
			t += to<uint16>(*in);
			++n;
		}
	}
	return to<uint8>((t * 2 + n) / (n * 2));
}

uint32 DifferenceScore(uint8x4 a, uint8x4 b) {
	return dot(square(to<int>(a) - to<int>(b)), int32x4{ 3, 6, 1, 0 });
}

template<typename C, typename F> static uint32 ChooseBook(const uint8x4* in, uint32 mask, uint32 &pixels, uint32 &book, C &&c, F &&make_table, uint32 score = ~0) {
	int		x		= 0;

	for (auto& i : c) {
		uint32	temp_pixels = 0;
		uint32	temp_score	= 0;

		auto	table = make_table(i);

		for (uint32 i = 0, m = mask; m && temp_score < score; ++i, m >>= 1) {
			if (m & 1) {
				uint32	best_score = ~0;
				int		best_index = 0;
				int		x = 0;
				for (auto &entry : table) {
					uint32	score = DifferenceScore(in[i], entry);
					if (score < best_score) {
						best_score = score;
						best_index = x;
					}
					++x;
				}
				temp_score		+= best_score;
				temp_pixels		|= (((best_index & 2) << 15) | (best_index & 1)) << i;
			}
		}

		if (temp_score < score) {
			score	= temp_score;
			pixels	= temp_pixels;
			book	= x;
		}
		++x;
	}
	return score;
}

//-----------------------------------------------------------------------------
// ETC1 decode
//-----------------------------------------------------------------------------

uint8x4 rgb555_32(uint32 i) {
	auto t = as<uint8>(i) & 0x1f;
	return (t << 3) | (t >> 1);
}

uint8x4 rgb444_32(uint32 i) {
	return (as<uint8>(i) & 0xf) * 17;
}

template<typename A, typename B> auto add_sub(A&& a, B&& b, bool sub) {
	return sub ? a - b : a + b;
}

void ETC1::Decode(const block<ISO_rgba, 2>& block) const {
	uint8x4		cols[2];
	if (diff) {
		uint32	i0	= (rgbc >> 3) & 0x1f1f1f;
		uint32	i1	= (i0 + ((rgbc & 0x030303) | ((rgbc & 0x040404) * 7))) & 0x1f1f1f;
		cols[0]	= rgb555_32(i0);
		cols[1]	= rgb555_32(i1);
	} else {
		cols[0]	= rgb444_32(rgbc >> 4);
		cols[1]	= rgb444_32(rgbc);
	}

	const uint8	mods[4]	= {codebookRGB[cw0][0], codebookRGB[cw0][1], codebookRGB[cw1][0], codebookRGB[cw1][1]};
	int			flipbit	= flip ? 2 : 8;

	for (uint32 i = 0, bits = pixels; i < 16; i++, bits >>= 1) {
		int		j = (i & flipbit) ? 1 : 0;
		block[i & 3][i >> 2] = add_sub(saturated(cols[j]), mods[(j << 1) | (bits & 1)], bits & (1 << 16));
	}
}

//	diff encodes whether block has alpha
void ETC1::DecodePunch(const block<ISO_rgba, 2>& block) const {
	uint32	i0	= (rgbc >> 3) & 0x1f1f1f;
	uint32	i1	= (i0 + ((rgbc & 0x030303) | ((rgbc & 0x040404) * 7))) & 0x1f1f1f;

	uint8x4		cols[2]	= { rgb555_32(i0), rgb555_32(i1) };
	const uint8	mods[4]	= {codebookRGB[cw0][0], codebookRGB[cw0][1], codebookRGB[cw1][0], codebookRGB[cw1][1]};
	int			flipbit	= flip ? 2 : 8;

	for (uint32 i = 0, bits = pixels; i < 16; i++, bits >>= 1) {
		int		j = (i & flipbit) ? 1 : 0;
		auto	c = cols[j];

		if (!diff && (bits & 1)) {
			if (bits & (1 << 16))
				c.w = 0;
		} else {
			c = add_sub(saturated(c), mods[(j << 1) | (bits & 1)], bits & (1 << 16));
		}

		block[i & 3][i >> 2] = c;
	}
}

//-----------------------------------------------------------------------------
// ETC1 encode
//-----------------------------------------------------------------------------

static uint32 ChooseBook(const uint8x4& base, const uint8x4* cols, uint32 mask, uint32& pixels, uint32& book, bool has_punch = false, uint32 score = ~0) {
	uint8x4	table[4];
	if (has_punch) {
		return ChooseBook(cols, mask, pixels, book, codebookRGB, [base, &table](const uint8* mods) {
			table[0] = saturated(base) + mods[0];
			table[1] = base;
			table[2] = saturated(base) - mods[0];
			return make_range_n(table, 3);
		}, score);

	} else {
		return ChooseBook(cols, mask, pixels, book, codebookRGB, [base, &table](const uint8* mods) {
			table[0] = saturated(base) + mods[0];
			table[1] = saturated(base) + mods[1];
			table[2] = saturated(base) - mods[0];
			table[3] = saturated(base) - mods[1];
			return make_range_n(table, 4);
		}, score);
	}
}

uint32 FixColours(uint8x4* cols, bool force_diff = false) {
	auto	c0 = to<int16>(cols[0].xyz);
	auto	c1 = to<int16>(cols[1].xyz);

	auto	c50 = extend_bits<8, 5>(c0);
	auto	c51 = extend_bits<8, 5>(c1);

	auto	d	= c51 - c50;

	if (any(d < -4 | d > 3)) {
		if (!force_diff) {
			auto	c40 = extend_bits<8, 4>(c0);
			auto	c41 = extend_bits<8, 4>(c1);

			cols[0] = to<uint8>(extend_bits<4, 8>(c40), zero);
			cols[1] = to<uint8>(extend_bits<4, 8>(c41), zero);
			return as<uint32>(to<uint8>((c40 << 4) | c41, zero));
		}

		c50 = select(d < -4, c50 + (d + 4) / 2, c50);
		c50 = select(d >  3, c50 + (d - 3) / 2, c50);
		c51	= c50 + clamp(d, -4, 3);
	}

	cols[0] = to<uint8>(extend_bits<5, 8>(c50), zero);
	cols[1] = to<uint8>(extend_bits<5, 8>(c51), zero);
	return as<uint32>(to<uint8>((c50 << 3) | (d & 7), zero)) | (1 << 25);
}

void ETC1::Encode(const block<ISO_rgba, 2>& block) {
	uint8x4		cols[16], base[2];
	uint32		mask	= block_mask<4, 4>(block.size<2>(), block.size<1>());
	uint32		book0, book1, pixels0, pixels1, rgbc0;

	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1)
			cols[i] = (const uint8x4&)block[i & 3][i >> 2];
	}

	// try unflipped
	base[0] = MaskedAverage(cols + 0, mask & 0xff);
	base[1] = MaskedAverage(cols + 8, mask >> 8);
	rgbc0	= FixColours(base);

	uint32	best_score	= ChooseBook(base[0], cols, mask & 0xff, pixels0, book0)
						+ ChooseBook(base[1], cols + 8, mask >> 8, pixels1, book1);
	set(rgbc0, false, book0, book1, pixels0 | (pixels1 << 8));

	// try flipped
	base[0]	= MaskedAverage(cols, mask & 0x3333);
	base[1]	= MaskedAverage(cols, mask & 0xcccc);
	rgbc0	= FixColours(base);

	uint32	score =	ChooseBook(base[0], cols, mask & 0x3333, pixels0, book0, false, best_score);
	score	+=		ChooseBook(base[1], cols, mask & 0xcccc, pixels1, book1, false, best_score - score);
	if (score < best_score)
		set(rgbc0, true, book0, book1, pixels0 | pixels1);
}

void ETC1::EncodePunch(const block<ISO_rgba, 2>& block) {
	uint8x4		cols[16], base[2];
	uint32		mask		= block_mask<4, 4>(block.size<2>(), block.size<1>());
	uint32		punch_mask	= 0;
	uint32		book0, book1, pixels0, pixels1, rgbc0;

	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1) {
			cols[i] = (const uint8x4&)block[i & 3][i >> 2];
			punch_mask |= (cols[i].w == 0) << i;
		}
	}

	uint32	punch_pixels = punch_mask | (punch_mask << 16);
	uint32	mask2 = mask & ~punch_mask;

	// try unflipped
	base[0]	= MaskedAverage(cols + 0, mask2 & 0xff);
	base[1]	= MaskedAverage(cols + 8, mask2 >> 8);
	rgbc0	= FixColours(base, true);

	uint32	best_score	= ChooseBook(base[0], cols, mask2 & 0xff, pixels0, book0, !!punch_mask)
						+ ChooseBook(base[1], cols + 8, mask2 >> 8, pixels1, book1, !!punch_mask);
	set(rgbc0, false, book0, book1, pixels0 | (pixels1 << 8) | punch_pixels);

	// try flipped
	base[0]	= MaskedAverage(cols, mask & 0x3333);
	base[1]	= MaskedAverage(cols, mask & 0xcccc);
	rgbc0	= FixColours(base, true);

	uint32	score = ChooseBook(base[0], cols, mask2 & 0x3333, pixels0, book0, !!punch_mask, best_score);
	score	+=		ChooseBook(base[1], cols, mask2 & 0xcccc, pixels1, book1, !!punch_mask, best_score - score);
	if (score < best_score)
		set(rgbc0, true, book0, book1, pixels0 | pixels1 | punch_pixels);

	diff = !punch_mask;
}

//-----------------------------------------------------------------------------
// ETCA decode
//-----------------------------------------------------------------------------

void ETCA::Decode(uint8* alpha) const {
	uint8	mul		= book >> 4;
	auto	cb		= codebookA[book & 15];
	uint64	bits	= pixels;

	for (int i = 0; i < 16; i++, bits <<= 3) {
		int		t = bits >> 45;
		int		d = cb[t & 3];
		if (!(t & 4))
			d = ~d;
		alpha[i] = base + d * mul;
	}
}

void ETCA::Decode(uint16 *alpha) const {
	int		alpha0	= base * 8 + 4;
	uint8	mul		= book >= 16 ? (book >> 4) * 8 : 1;
	auto	cb		= codebookA[book & 15];
	uint64	bits	= pixels;

	for (int i = 0; i < 16; i++, bits <<= 3) {
		int		t = bits >> 45;
		int		d = cb[t & 3];
		if (!(t & 4))
			d = ~d;
		alpha[i] = clamp(alpha0 + d * mul, 0, 2047);
	}
}

void ETCA::Decode(int16 *alpha) const {
	int		alpha0	= (base == 128 ? -128 : (int8)base) * 8;
	auto	cb		= codebookA[book & 15];
	uint8	mul		= book >= 16 ? (book >> 4) * 8 : 1;
	uint64	bits	= pixels;

	for (int i = 0; i < 16; i++, bits <<= 3) {
		int		t = bits >> 45;
		int		d = cb[t & 3];
		if (!(t & 4))
			d = ~d;
		alpha[i] = clamp(alpha0 + d * mul, -1023, 1023);
	}
}

//-----------------------------------------------------------------------------
// ETCA encode
//-----------------------------------------------------------------------------

void MakeTableA(int16 *table, uint8 book, bool hdr) {
	uint8	mul	= hdr ? (book >= 16 ? (book >> 4) * 8 : 1) : book >> 4;
	auto	pos = table + 4, neg = pos;
	for (auto i : codebookA[book & 15]) {
		*pos++ = i * mul;
		*--neg = ~i * mul;
	}
}

template<typename P> static uint32 ChooseBook(const P* in, uint32 mask, int base, bool hdr) {
	int		sorted[16];
	int*	end = sorted;
	for (; mask; mask >>= 1, ++in) {
		if (mask & 1)
			*end++ = *in - base;
	}

	sort(make_range(&sorted[0], end));

	int		range = max(~sorted[0], end[-1]);
	uint32	best_book = 0, book = 0, max_book = 256;

	if (hdr) {
		if (range < 32) {
			max_book	= 16;
		} else {
			book		= min(range / 64 * 16, 256 - 32);
			max_book	= book + 32;
		}
	} else {
		max_book = (range / 16) * 16;
	}

	for (uint32	score = ~0; book < 256; ++book) {
		int16	table[8];
		MakeTableA(table, book, hdr);

		uint32		temp_score = 0;
		const int	*p = sorted;
		for (uint32 j = 0; p < end && j < 7; ++j) {
			int16	curr = table[j];
			int16	next = (table[j] + table[j + 1]) / 2;

			while (p < end && *p < next) {
				temp_score += square(*p - curr);
				++p;
			}
		}

		int16	curr = table[7];
		while (p < end) {
			temp_score += square(*p - curr);
			++p;
		}

		if (temp_score < score) {
			best_book	= book;
			score		= temp_score;
		}
	}
	return best_book;
}

template<typename P> static uint64 GetIndicesA(const P* in, int base, uint32 mask, uint8 book, bool hdr) {
	int16	table[8];
	MakeTableA(table, book, hdr);

	uint64	pixels = 0;
	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1) {
			int	d = in[i] - base;
			int	j = 0;
			while (j < 8 && d > table[j])
				j++;
			if (j == 8 || (j > 0 && table[j] - d > d - table[j - 1]))
				--j;

			j		^= ((~j >> 2) & 1) * 3;
			pixels	|= uint64(j) << ((15 - i) * 3);
		}

	}
	return pixels;
}

void ETCA::Encode(const uint8* alpha, uint32 mask) {
	uint8	base0	= MaskedAverage(alpha, mask);
	uint32	book	= ChooseBook(alpha, mask, base0, true);
	set(base0, book, GetIndicesA(alpha, base0, mask, book, false));
}

void ETCA::Encode(const uint16* alpha, uint32 mask) {
	uint32	base0	= ((MaskedAverage(alpha, mask) - 4) & ~7) + 4;
	uint32	book	= ChooseBook(alpha, mask, base0, true);
	set(base0 >> 3, book, GetIndicesA(alpha, base0, mask, book, true));
}

void ETCA::Encode(const int16* alpha, uint32 mask) {
	uint32	base0	= ((MaskedAverage(alpha, mask) - 4) & ~7) + 4;
	uint32	book	= ChooseBook(alpha, mask, base0, true);
	set(base0 >> 3, book, GetIndicesA(alpha, base0, mask, book, true));
}

//-----------------------------------------------------------------------------
// ETC2 decode
//-----------------------------------------------------------------------------

static const int MODE_TABLE = 7, MODE_T = 0, MODE_H = 8, MODE_ALPHA = 16;

inline void TableTH(uint8x4 *table, uint8x4 c0, uint8x4 c1, uint8 delta, int mode) {
	if (!(mode & MODE_H)) {
		// T mode
		table[0] = c0;
		table[1] = saturated(c1) + delta;
		table[2] = c1;
		table[3] = saturated(c1) - delta;

	} else {
		// H mode
		table[0] = saturated(c0) + delta;
		table[1] = saturated(c0) - delta;
		table[2] = saturated(c1) + delta;
		table[3] = saturated(c1) - delta;
	}

	if (mode & MODE_ALPHA)
		table[3] = 0;
}

uint8x4 rgb444_12(uint32 i) {
	return uint8x4{ uint8(i >> 8), uint8((i >> 4) & 0xf), uint8(i & 0xf), 0xf } * 17;
}

void DecodeTH(const block<ISO_rgba, 2>& block, uint32 i0, uint32 i1, int mode, uint32 bits) {
	uint8x4		table[4];
	TableTH(table, rgb444_12(i0), rgb444_12(i1), modifier_table[mode & MODE_TABLE], mode);

	for (uint32 i = 0; i < 16; i++, bits >>= 1)
		block[i & 3][i >> 2] = table[(bits & 1) | ((bits >> 15) & 2)];
}

int16x4 rgb676_19(uint32 i) {
	return {
		extend_bits<6, 8, int16>((i >> 13) & 0x3f),
		extend_bits<7, 8, int16>((i >> 6) & 0x7f),
		extend_bits<6, 8, int16>(i & 0x3f),
		0xff
	};
}

void DecodePlanar(const block<ISO_rgba, 2>& block, uint32 i0, uint32 ih, uint32 iv) {
	int16x4	c0 = rgb676_19(i0), ch = rgb676_19(ih), cv = rgb676_19(iv);

	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++)
			block[y][x] = to_sat<uint8>((x * ch + y * cv + (4 - x - y) * c0 + 2) >> 2);
	}
}

static inline uint32 get_overflow(uint32 rgbc) {
	uint32	i0		= (rgbc >> 3) & 0x1f1f1f;
	uint32	signs	= rgbc & 0x040404;
	uint32	i1		= i0 + ((rgbc & 0x030303) | (signs * 7));
	return (i1 & 0x202020) ^ (signs << 3);
}

void ETC2_RGB::Decode(const block<ISO_rgba, 2>& block) const {
	if (etc1.diff) {
		if (uint32 ov = get_overflow(etc1.rgbc)) {
			if (ov & 0x2020) {
				if (ov & 0x20) {
					// T mode
					DecodeTH(block, t.c0, t.c1, t.mod | MODE_T, etc1.pixels);
				} else {
					// H mode
					uint32	i0	= h.c0, i1 = h.c1;
					DecodeTH(block, i0, i1, (i0 > i1) | h.mod | MODE_H, etc1.pixels);
				}

			} else  {
				//Planar mode
				DecodePlanar(block, p.c0, p.ch, p.cv);
			}
			return;
		}
	}

	etc1.Decode(block);
}

//	diff encodes whether block has alpha
void ETC2_RGB::DecodePunch(const block<ISO_rgba, 2>& block) const {
	if (uint32 ov = get_overflow(etc1.rgbc)) {
		if (ov & 0x2020) {
			if (ov & 0x20) {
				// T mode
				DecodeTH(block, t.c0, t.c1, t.mod | (etc1.diff ? MODE_ALPHA : 0), etc1.pixels);
			} else {
				// H mode
				uint32	i0	= h.c0, i1 = h.c1;
				DecodeTH(block, i0, i1, (i0 > i1) | h.mod | (etc1.diff ? MODE_H | MODE_ALPHA : MODE_H), etc1.pixels);
			}

		} else  {
			//Planar mode
			DecodePlanar(block, p.c0, p.ch, p.cv);
		}

	} else {
		etc1.DecodePunch(block);
	}
}

void ETC2_RGBA::Decode(const block<ISO_rgba, 2>& block) const {
	uint8	alphas[16];
	alpha.Decode(alphas);
	rgb.Decode(block);
	for (uint32 i = 0; i < 16; i++)
		block[i & 3][i >> 2].a = alphas[i];
}

//-----------------------------------------------------------------------------
// ETC2 encode
//-----------------------------------------------------------------------------

uint32 rgb12(uint8x4 c) {
	auto	t = extend_bits<8, 4>(to<uint16>(c));
	return (t.x << 8) | (t.y << 4) | t.z;
}

static uint32 ScoreTH(const uint8x4 *base, const uint8x4* cols, uint32 mask, uint32& pixels, uint32& book, int mode, uint32 score = ~0) {
	uint8x4	table[4];
	return ChooseBook(cols, mask, pixels, book, modifier_table, [base, &table, mode](int delta) {
		TableTH(table, base[0], base[1], delta, mode);
		return make_range_n(table, mode & MODE_ALPHA ? 3 : 4);
	}, score);
}

inline uint32 CalcGreyDistance2(uint8x4 a, uint8x4 b) {
	auto	d = to<int>(a) - to<int>(b);
	return len2(d * 3 - reduce_add(d));
}

uint8x4 quantise444(const uint8x4& c) {
	return to<uint8>(extend_bits<4, 8>(extend_bits<8, 4>(to<uint16>(c))));
}

void CalculateBaseTH(uint8x4 *base, const uint8x4* in, uint32 mask) {
	// find pixel farthest from average gray line
	auto	avg		= MaskedAverage(in, mask);
	uint32	farthest			= 0;
	uint32	farthest_distance2	= 0;

	for (uint32 i = 0, m = mask; m; m >>= 1, ++i) {
		uint32 grey_distance2 = CalcGreyDistance2(in[i], avg);
		if (grey_distance2 > farthest_distance2) {
			farthest			= i;
			farthest_distance2	= grey_distance2;
		}
	}

	auto offset = (to<int>(in[farthest]) - to<int>(avg)) / 2;
	base[0] = quantise444(to_sat<uint8>(to<int>(avg) + offset));
	base[1] = quantise444(to_sat<uint8>(to<int>(avg) - offset));

	// move base colors to find best fit
	for (uint32 it = 0; it < 10; it++)	{
		uint32	part_mask = 0;
		for (uint32 i = 0, m = mask; m; m >>= 1, ++i) {
			if (m & 1)
				part_mask |= (CalcGreyDistance2(in[i], base[0]) > CalcGreyDistance2(in[i], base[1])) << i;
		}

		if (part_mask == 0 || part_mask == mask)
			break;

		uint8x4 new0 = quantise444(MaskedAverage(in, mask & ~part_mask));
		uint8x4 new1 = quantise444(MaskedAverage(in, mask &  part_mask));

		if (all(new0 == base[0] & new1 == base[1]))
			break;

		base[0] = new0;
		base[1] = new1;
	}
}

float3x2 ColourRegression(initializer_list<float3> pts) {
	float4	sum_y	= zero;	//w is sum_x
	float4	sum_xy	= zero;	//w is sum_x2

	uint32	n = 0;
	for (auto &p : pts) {
		float4	p1 = concat(p, n);
		sum_y	+= p1;
		sum_xy	+= p1 * n;
		++n;
	}

	float3	slope	= (sum_xy.xyz * n - sum_y.w * sum_y.xyz) / (sum_xy.w * n - square(sum_y.w));
	float3	offset	= (sum_y.xyz - slope * sum_y.w) / n;
	return { offset, slope };
}

template<int R, int G, int B> uint8x4 quantise_rgba(uint8x4 c) {
	return {
		extend_bits<R, 8>(extend_bits<8, R>(c.x)),
		extend_bits<G, 8>(extend_bits<8, G>(c.y)),
		extend_bits<B, 8>(extend_bits<8, B>(c.z)),
		0xff
	};
}

uint32 rgb19(uint8x4 c) {
	uint8x4	t = {
		extend_bits<8, 6>(c.x),
		extend_bits<8, 7>(c.y),
		extend_bits<8, 6>(c.z),
		0xff
	};
	return (t.r << 13) | (t.g << 6) | t.b;
}

uint32 CalculatePlanar(uint8x4 *base, const uint8x4 *cols, uint32 mask) {
	float3	base0, base1, base2;
	float3	fcols[16];
	for (int i = 0; i < 16; i++)
		fcols[i] = to<float>(cols[i]).xyz;

	// top edge
	auto	lines = ColourRegression({ fcols[0], fcols[4], fcols[8], fcols[12] });
	base0 = lines.x;
	base1 = lines.x + lines.y * 4;

	// left edge
	lines = ColourRegression({ fcols[0], fcols[1], fcols[2], fcols[3] });
	base0 = (base0 + lines.x) * half;		// average with top edge
	base2 = lines.x + lines.y * 4;

	// right edge
	lines = ColourRegression({ fcols[12], fcols[13], fcols[14], fcols[15] });
	base1 = (base1 + lines.x) * half;		// average with top edge

	// bottom edge
	lines = ColourRegression({ fcols[3], fcols[7], fcols[11], fcols[15] });
	base2 = (base2 + lines.x) * half;		// average with left edge

	base[0] = quantise_rgba<6, 7, 6>(to<uint8>(concat(base0, one)));
	base[1] = quantise_rgba<6, 7, 6>(to<uint8>(concat(base1, one)));
	base[2] = quantise_rgba<6, 7, 6>(to<uint8>(concat(base2, one)));

	uint32	score = 0;
	auto	c0 = to<int16>(base[0]), ch = to<int16>(base[1]), cv = to<int16>(base[2]);
	for (int i = 0; mask; i++, mask >>= 1) {
		if (mask & 1) {
			int	x = i >> 2, y = i & 3;
			score += DifferenceScore(
				to_sat<uint8>(((4 - x - y) * c0 + x * ch + y * cv + 2) >> 2),
				cols[i]
			);
		}
	}
	return score;
}

void ETC2_RGB::Encode(const block<ISO_rgba, 2>& block) {
	uint8x4	cols[16], base[3];
	uint32	mask = block_mask<4, 4>(block.size<2>(), block.size<1>());
	uint32	book0, book1, pixels0, pixels1, rgbc;

	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1)
			cols[i]	= (const uint8x4&)block[i & 3][i >> 2];
	}

	// try unflipped
	base[0] = MaskedAverage(cols + 0, mask & 0xff);
	base[1] = MaskedAverage(cols + 8, mask >> 8);
	rgbc	= FixColours(base);
	uint32	best_score	= ChooseBook(base[0], cols, mask & 0xff, pixels0, book0)
						+ ChooseBook(base[1], cols + 8, mask >> 8, pixels1, book1);
	etc1.set(rgbc, false, book0, book1, pixels0 | (pixels1 << 8));

	// try flipped
	base[0] = MaskedAverage(cols, mask & 0x3333);
	base[1] = MaskedAverage(cols, mask & 0xcccc);
	rgbc	= FixColours(base);
	uint32	score = ChooseBook(base[0], cols, mask & 0x3333, pixels0, book0, false, best_score);
	score	+=		ChooseBook(base[1], cols, mask & 0xcccc, pixels1, book1, false, best_score - score);
	if (score < best_score) {
		best_score	= score;
		etc1.set(rgbc, true, book0, book1, pixels0 | pixels1);
	}

	CalculateBaseTH(base, cols, mask);

	// try T mode
	score = ScoreTH(base, cols, mask, pixels0, book0, MODE_T, best_score);
	if (score < best_score) {
		best_score	= score;
		etc1.pixels	= pixels0;
		t.set(rgb12(base[0]), rgb12(base[1]), book0);
	}

	// try T with reversed base
	swap(base[0], base[1]);
	score = ScoreTH(base, cols, mask, pixels0, book0, MODE_T, best_score);
	if (score < best_score) {
		best_score	= score;
		etc1.pixels = pixels0;
		t.set(rgb12(base[0]), rgb12(base[1]), book0);
	}

	// try H mode
	score = ScoreTH(base, cols, mask, pixels0, book0, MODE_H, best_score);
	if (score < best_score) {
		best_score	= score;
		auto	i0	= rgb12(base[0]);
		auto	i1	= rgb12(base[1]);
		if ((book0 & 1) && i0 < i1) {
			swap(i0, i1);
			pixels0 ^= 0xffff0000;
		}
		etc1.pixels = pixels0;
		h.set(i0, i1, book0);
	}

	// try Planar mode
	score = CalculatePlanar(base, cols, mask);
	if (score < best_score) {
		best_score	= score;
		p.set(rgb19(base[0]), rgb19(base[1]), rgb19(base[2]));
	}
}

void ETC2_RGB::EncodePunch(const block<ISO_rgba, 2>& block) {
	uint8x4		cols[16], base[3];
	uint32		book0, book1, pixels0, pixels1, rgbc;
	uint32		mask = block_mask<4, 4>(block.size<2>(), block.size<1>());
	uint32		punch_mask	= 0;

	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1) {
			cols[i] = (const uint8x4&)block[i & 3][i >> 2];
			punch_mask |= (cols[i].w == 0) << i;
		}
	}

	uint32	punch_pixels = punch_mask | (punch_mask << 16);
	uint32	mask2 = mask & ~punch_mask;

	// try unflipped
	base[0] = MaskedAverage(cols + 0, mask2 & 0xff);
	base[1] = MaskedAverage(cols + 8, mask2 >> 8);
	rgbc	= FixColours(base);
	uint32	best_score	= ChooseBook(base[0], cols, mask2 & 0xff, pixels0, book0, !!punch_mask)
						+ ChooseBook(base[1], cols + 8, mask2 >> 8, pixels1, book1, !!punch_mask);
	etc1.set(rgbc, false, book0, book1, pixels0 | (pixels1 << 8) | punch_pixels);

	// try flipped
	base[0] = MaskedAverage(cols, mask2 & 0x3333);
	base[1] = MaskedAverage(cols, mask2 & 0xcccc);
	rgbc	= FixColours(base);
	uint32	score = ChooseBook(base[0], cols, mask2 & 0x3333, pixels0, book0, !!punch_mask, best_score);
	score	+=		ChooseBook(base[1], cols, mask2 & 0xcccc, pixels1, book1, !!punch_mask, best_score - score);
	if (score < best_score) {
		best_score	= score;
		etc1.set(rgbc, true, book0, book1, pixels0 | pixels1 | punch_pixels);
	}

	CalculateBaseTH(base, cols, mask2);

	// try T mode
	score = ScoreTH(base, cols, mask2, pixels0, book0, punch_mask ? MODE_T | MODE_ALPHA : MODE_T, best_score);
	if (score < best_score) {
		best_score	= score;
		etc1.pixels = pixels0 | punch_pixels;
		t.set(rgb12(base[0]), rgb12(base[1]), book0, !!punch_mask);
	}

	// try T with reversed base
	swap(base[0], base[1]);
	score = ScoreTH(base, cols, mask2, pixels0, book0, punch_mask ? MODE_T | MODE_ALPHA : MODE_T, best_score);
	if (score < best_score) {
		best_score	= score;
		etc1.pixels = pixels0 | punch_pixels;
		t.set(rgb12(base[0]), rgb12(base[1]), book0, !!punch_mask);
	}

	// try H mode
	score = ScoreTH(base, cols, mask2, pixels0, book0, punch_mask ? MODE_H | MODE_ALPHA : MODE_H, best_score);
	if (score < best_score) {
		best_score	= score;
		auto	i0	= rgb12(base[0]);
		auto	i1	= rgb12(base[1]);
		if ((book0 & 1) && i0 < i1) {
			swap(i0, i1);
			pixels0 ^= 0xffff0000;
		}
		etc1.pixels = pixels0 | punch_pixels;
		h.set(i0, i1, book0, !!punch_mask);
	}

	if (!punch_mask) {
		// try Planar mode
		score = CalculatePlanar(base, cols, mask);
		if (score < best_score) {
			best_score = score;
			p.set(rgb19(base[0]), rgb19(base[1]), rgb19(base[2]));
		}
	}
}

void ETC2_RGBA::Encode(const block<ISO_rgba, 2>& block) {
	uint8	alphas[16];
	uint32	mask = block_mask<4, 4>(block.size<2>(), block.size<1>());
	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1)
			alphas[i] = block[i & 3][i >> 2].a;
	}
	alpha.Encode(alphas, mask);
	rgb.Encode(block);
}

//-----------------------------------------------------------------------------
// 11-bit decode
//-----------------------------------------------------------------------------

void ETC2_R11::Decode(const block<HDRpixel, 2>& block) const {
	uint16	reds[16];
	red.Decode(reds);
	for (int i = 0; i < 16; i++)
		block[i & 3][i >> 2] = float(reds[i]) / bits(11);
}

void ETC2_RG11::Decode(const block<HDRpixel, 2>& block) const {
	uint16	reds[16], greens[16];
	red.Decode(reds);
	green.Decode(greens);
	for (int i = 0; i < 16; i++)
		block[i & 3][i >> 2] = float4{float(reds[i]) / bits(11), float(greens[i]) / bits(11), zero, one};
}

void ETC2_R11S::Decode(const block<HDRpixel, 2>& block) const {
	int16	reds[16];
	red.Decode(reds);
	for (int i = 0; i < 16; i++)
		block[i & 3][i >> 2] = float(reds[i]) / bits(10);
}

void ETC2_RG11S::Decode(const block<HDRpixel, 2>& block) const {
	int16	reds[16], greens[16];
	red.Decode(reds);
	green.Decode(greens);
	for (int i = 0; i < 16; i++)
		block[i & 3][i >> 2] = float4{float(reds[i]) / bits(10), float(greens[i]) / bits(10), zero, one};
}

//-----------------------------------------------------------------------------
// 11-bit encode
//-----------------------------------------------------------------------------

void ETC2_R11::Encode(const block<HDRpixel, 2>& block) {
	uint16	reds[16];
	uint32	mask = block_mask<4, 4>(block.size<2>(), block.size<1>());
	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1)
			reds[i] = block[i & 3][i >> 2].r * bits(11);
	}
	red.Encode(reds, mask);
}

void ETC2_RG11::Encode(const block<HDRpixel, 2>& block) {
	uint16	reds[16], greens[16];
	uint32	mask = block_mask<4, 4>(block.size<2>(), block.size<1>());
	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1) {
			reds[i]		= block[i & 3][i >> 2].r * bits(11);
			greens[i]	= block[i & 3][i >> 2].g * bits(11);
		}
	}
	red.Encode(reds, mask);
	green.Encode(greens, mask);
}

void ETC2_R11S::Encode(const block<HDRpixel, 2>& block) {
	int16	reds[16];
	uint32	mask = block_mask<4, 4>(block.size<2>(), block.size<1>());
	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1)
			reds[i] = block[i & 3][i >> 2].r * bits(10);
	}
	red.Encode(reds, mask);
}

void ETC2_RG11S::Encode(const block<HDRpixel, 2>& block) {
	int16	reds[16], greens[16];
	uint32	mask = block_mask<4, 4>(block.size<2>(), block.size<1>());
	for (uint32 i = 0, m = mask; m; ++i, m >>= 1) {
		if (m & 1) {
			reds[i]		= block[i & 3][i >> 2].r * bits(10);
			greens[i]	= block[i & 3][i >> 2].g * bits(10);
		}
	}
	red.Encode(reds, mask);
	green.Encode(greens, mask);
}
