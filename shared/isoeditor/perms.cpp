#include "base/defs.h"
#include "base/strings.h"
#include "base/algorithm.h"
#include "stream.h"

using namespace iso;

template<int N> struct perm {
	uint8	p[N];

	operator uint8*()	{ return p; }

	perm<N> apply_disable(const uint8 *a) {
		perm<N>	b;
		clear(b);
		return b;
	}

	perm<N> apply_shuffle(const uint8 *a) {
		perm<N>	b;
		for (int i = 0; i < N; i++) {
			b.p[i] = a[p[i]];
		}
		return b;
	}

	perm<N> apply_add(const uint8 *a) {
		perm<N>	b;
		for (int i = 0; i < N; i++) {
			int	r = 0;
			for (int j = p[i]; j; j = clear_lowest(j)) {
				uint8	m = a[lowest_set_index(j)];
				if (r & m) {
					r = 0xff;
					break;
				}
				r |= m;
			}
			b.p[i] = r;
		}
		return b;
	}
};

enum MODE { DISABLE, SHUFFLE, ADD};

struct Perm {
	uint8		mode;
	perm<4>		p;
	const char *name;
	int latency, throughput;

	perm<4> apply(const uint8 *a) {
		return	mode == DISABLE ? p.apply_disable(a)
			//:	mode == SHUFFLE	? p.apply_shuffle(a)
			: p.apply_add(a);
	}

};

template<int N> struct ops {
	uint8	p[N];
	ops()  { p[0] = 0xff; }
	template<typename...T> ops(T... t) : p{uint8(t)...} {}

	bool	operator!() const { return p[0] == 0xff; }

	uint32	cost(const Perm *perms) const;
	void	print(string_accum &sa, const Perm *perms) const;
};


template<> 	uint32	ops<3>::cost(const Perm *perms) const {
	if (perms[p[2]].throughput) {
		if (p[0] == p[1])
			return perms[p[0]].latency + perms[p[2]].throughput;
		return perms[p[0]].throughput + max(perms[p[0]].latency, perms[p[1]].latency) + perms[p[2]].throughput;
	}
	return perms[p[0]].throughput + perms[p[1]].throughput;
}

template<> 	void ops<3>::print(string_accum &sa, const Perm *perms) const {
	if (p[0] == p[1] && p[0] > 1) {
		((sa << "v8 t = ").formati(perms[p[0]].name, "a", "b") << "; return ").formati(perms[p[2]].name, "t", "t");
	} else {
		(sa << "return ").formati(perms[p[2]].name, formati_string(perms[p[0]].name, "a", "b"), formati_string(perms[p[1]].name, "a", "b"));
	}
}

template<> 	uint32	ops<4>::cost(const Perm *perms) const {
	uint32	cost3 = ((ops<3>*)this)->cost(perms);
	return cost3 + perms[p[2]].throughput;
}

template<> 	void ops<4>::print(string_accum &sa, const Perm *perms) const {
	const char *t0;
	if (p[0] == 0) {
		t0 = "a";
	} else if (p[0] == 1) {
		t0 = "b";
	} else {
		(sa << "v8 t0 = ").formati(perms[p[0]].name, "a", "b") << "; ";
		t0 = "t0";
	}
	if (p[0] == p[1]) {
		(sa << "return ").formati(perms[p[3]].name, t0, formati_string(perms[p[2]].name, t0, t0));
	} else {
		(sa << "return ").formati(perms[p[3]].name, t0, formati_string(perms[p[2]].name, t0, formati_string(perms[p[1]].name, "a", "b")));
	}
}

#define __	0

#define AX	1<<0
#define AY	1<<1
#define AZ	1<<2
#define AW	1<<3

#define BX	0x10<<0
#define BY	0x10<<1
#define BZ	0x10<<2
#define BW	0x10<<3

Perm perms256[] = {
	{SHUFFLE,	{AX,AY,AZ,AW},	"%0",						0,	0},	//0
	{SHUFFLE,	{BX,BY,BZ,BW},	"%1",						0,	0},

	//blend_pd
//	{SHUFFLE,	{AX,AY,AZ,AW},	"vblend<0>(%0, %1)",		3,	1},
	{SHUFFLE,	{BX,AY,AZ,AW},	"vblend<1>(%0, %1)",		3,	1},	//2
	{SHUFFLE,	{AX,BY,AZ,AW},	"vblend<2>(%0, %1)",		3,	1},
	{SHUFFLE,	{BX,BY,AZ,AW},	"vblend<3>(%0, %1)",		3,	1},
	{SHUFFLE,	{AX,AY,BZ,AW},	"vblend<4>(%0, %1)",		3,	1},
	{SHUFFLE,	{BX,AY,BZ,AW},	"vblend<5>(%0, %1)",		3,	1},
	{SHUFFLE,	{AX,BY,BZ,AW},	"vblend<6>(%0, %1)",		3,	1},
	{SHUFFLE,	{BX,BY,BZ,AW},	"vblend<7>(%0, %1)",		3,	1},
	{SHUFFLE,	{AX,AY,AZ,BW},	"vblend<8>(%0, %1)",		3,	1},
	{SHUFFLE,	{BX,AY,AZ,BW},	"vblend<9>(%0, %1)",		3,	1},
	{SHUFFLE,	{AX,BY,AZ,BW},	"vblend<10>(%0, %1)",		3,	1},
	{SHUFFLE,	{BX,BY,AZ,BW},	"vblend<11>(%0, %1)",		3,	1},
	{SHUFFLE,	{AX,AY,BZ,BW},	"vblend<12>(%0, %1)",		3,	1},
	{SHUFFLE,	{BX,AY,BZ,BW},	"vblend<13>(%0, %1)",		3,	1},
	{SHUFFLE,	{AX,BY,BZ,BW},	"vblend<14>(%0, %1)",		3,	1},	//15
//	{SHUFFLE,	{BX,BY,BZ,BW},	"vblend<15>(%0, %1)",		3,	1},

//permute_pd
	{SHUFFLE,	{AX,AX,AZ,AZ},	"vpermute<0>(%0)",			3,	3},	//16
	{SHUFFLE,	{AY,AX,AZ,AZ},	"vpermute<1>(%0)",			3,	3},
	{SHUFFLE,	{AX,AY,AZ,AZ},	"vpermute<2>(%0)",			3,	3},
	{SHUFFLE,	{AY,AY,AZ,AZ},	"vpermute<3>(%0)",			3,	3},
	{SHUFFLE,	{AX,AX,AW,AZ},	"vpermute<4>(%0)",			3,	3},
	{SHUFFLE,	{AY,AX,AW,AZ},	"vpermute<5>(%0)",			3,	3},
	{SHUFFLE,	{AX,AY,AW,AZ},	"vpermute<6>(%0)",			3,	3},
	{SHUFFLE,	{AY,AY,AW,AZ},	"vpermute<7>(%0)",			3,	3},
	{SHUFFLE,	{AX,AX,AZ,AW},	"vpermute<8>(%0)",			3,	3},
	{SHUFFLE,	{AY,AX,AZ,AW},	"vpermute<9>(%0)",			3,	3},
//	{SHUFFLE,	{AX,AY,AZ,AW},	"vpermute<10>(%0)",			3,	3},
	{SHUFFLE,	{AY,AY,AZ,AW},	"vpermute<11>(%0)",			3,	3},
	{SHUFFLE,	{AX,AX,AW,AW},	"vpermute<12>(%0)",			3,	3},
	{SHUFFLE,	{AY,AX,AW,AW},	"vpermute<13>(%0)",			3,	3},
	{SHUFFLE,	{AX,AY,AW,AW},	"vpermute<14>(%0)",			3,	3},
	{SHUFFLE,	{AY,AY,AW,AW},	"vpermute<15>(%0)",			3,	3},	//30
//permute_pd(b)
	{SHUFFLE,	{BX,BX,BZ,BZ},	"vpermute<0>(%1)",			3,	3},	//31
	{SHUFFLE,	{BY,BX,BZ,BZ},	"vpermute<1>(%1)",			3,	3},
	{SHUFFLE,	{BX,BY,BZ,BZ},	"vpermute<2>(%1)",			3,	3},
	{SHUFFLE,	{BY,BY,BZ,BZ},	"vpermute<3>(%1)",			3,	3},
	{SHUFFLE,	{BX,BX,BW,BZ},	"vpermute<4>(%1)",			3,	3},
	{SHUFFLE,	{BY,BX,BW,BZ},	"vpermute<5>(%1)",			3,	3},
	{SHUFFLE,	{BX,BY,BW,BZ},	"vpermute<6>(%1)",			3,	3},
	{SHUFFLE,	{BY,BY,BW,BZ},	"vpermute<7>(%1)",			3,	3},
	{SHUFFLE,	{BX,BX,BZ,BW},	"vpermute<8>(%1)",			3,	3},
	{SHUFFLE,	{BY,BX,BZ,BW},	"vpermute<9>(%1)",			3,	3},
//	{SHUFFLE,	{BX,BY,BZ,BW},	"vpermute<10>(%1)",			3,	3},
	{SHUFFLE,	{BY,BY,BZ,BW},	"vpermute<11>(%1)",			3,	3},
	{SHUFFLE,	{BX,BX,BW,BW},	"vpermute<12>(%1)",			3,	3},
	{SHUFFLE,	{BY,BX,BW,BW},	"vpermute<13>(%1)",			3,	3},
	{SHUFFLE,	{BX,BY,BW,BW},	"vpermute<14>(%1)",			3,	3},
	{SHUFFLE,	{BY,BY,BW,BW},	"vpermute<15>(%1)",			3,	3},	//45

	//movedup_pd	(covered by permute_pd)
//	{{0,0,2,2}, "movedup_pd(a)",			3,	3},
	//movedup_pd(b)	(covered by permute_pd(b))
//	{{4,4,6,6}, "movedup_pd(b)",			3,	3},

	//shuffle_pd
	{SHUFFLE,	{AX,BX,AZ,BZ},	"vshuffle<0>(%0, %1)",		3,	3},	//46
	{SHUFFLE,	{AY,BX,AZ,BZ},	"vshuffle<1>(%0, %1)",		3,	3},
	{SHUFFLE,	{AX,BY,AZ,BZ},	"vshuffle<2>(%0, %1)",		3,	3},
	{SHUFFLE,	{AY,BY,AZ,BZ},	"vshuffle<3>(%0, %1)",		3,	3},
	{SHUFFLE,	{AX,BX,AW,BZ},	"vshuffle<4>(%0, %1)",		3,	3},
	{SHUFFLE,	{AY,BX,AW,BZ},	"vshuffle<5>(%0, %1)",		3,	3},
	{SHUFFLE,	{AX,BY,AW,BZ},	"vshuffle<6>(%0, %1)",		3,	3},
	{SHUFFLE,	{AY,BY,AW,BZ},	"vshuffle<7>(%0, %1)",		3,	3},
	{SHUFFLE,	{AX,BX,AZ,BW},	"vshuffle<8>(%0, %1)",		3,	3},
	{SHUFFLE,	{AY,BX,AZ,BW},	"vshuffle<9>(%0, %1)",		3,	3},
	{SHUFFLE,	{AX,BY,AZ,BW},	"vshuffle<10>(%0, %1)",		3,	3},
	{SHUFFLE,	{AY,BY,AZ,BW},	"vshuffle<11>(%0, %1)",		3,	3},
	{SHUFFLE,	{AX,BX,AW,BW},	"vshuffle<12>(%0, %1)",		3,	3},
	{SHUFFLE,	{AY,BX,AW,BW},	"vshuffle<13>(%0, %1)",		3,	3},
	{SHUFFLE,	{AX,BY,AW,BW},	"vshuffle<14>(%0, %1)",		3,	3},
	{SHUFFLE,	{AY,BY,AW,BW},	"vshuffle<15>(%0, %1)",		3,	3},	//61

	//alignr_epiBZBX (avxBYAYAZ only)
	{DISABLE,	{AY,AZ,AW,BX},	"vshift_by64<1>(%0, %1)",	3,	3},	//62
	{DISABLE,	{AZ,AW,BX,BY},	"vshift_by64<2>(%0, %1)",	3,	3},
	{DISABLE,	{AW,BX,BY,BZ},	"vshift_by64<3>(%0, %1)",	3,	3},

	//permute2_pd
	{SHUFFLE,	{AX,AY,AX,AY},	"vpermute<0x00>(%0, %1)",	9,	3},	//65
	{SHUFFLE,	{AZ,AW,AX,AY},	"vpermute<0x01>(%0, %1)",	9,	3},
	{SHUFFLE,	{BX,BY,AX,AY},	"vpermute<0x02>(%0, %1)",	9,	3},
	{SHUFFLE,	{BZ,BW,AX,AY},	"vpermute<0x03>(%0, %1)",	9,	3},
	{SHUFFLE,	{AX,AY,AZ,AW},	"vpermute<0x10>(%0, %1)",	9,	3},
	{SHUFFLE,	{AZ,AW,AZ,AW},	"vpermute<0x11>(%0, %1)",	9,	3},
	{SHUFFLE,	{BX,BY,AZ,AW},	"vpermute<0x12>(%0, %1)",	9,	3},
	{SHUFFLE,	{BZ,BW,AZ,AW},	"vpermute<0x13>(%0, %1)",	9,	3},
	{SHUFFLE,	{AX,AY,BX,BY},	"vpermute<0x20>(%0, %1)",	9,	3},
	{SHUFFLE,	{AZ,AW,BX,BY},	"vpermute<0x21>(%0, %1)",	9,	3},
	{SHUFFLE,	{BX,BY,BX,BY},	"vpermute<0x22>(%0, %1)",	9,	3},
	{SHUFFLE,	{BZ,BW,BX,BY},	"vpermute<0x23>(%0, %1)",	9,	3},
	{SHUFFLE,	{AX,AY,BZ,BW},	"vpermute<0x30>(%0, %1)",	9,	3},
	{SHUFFLE,	{AZ,AW,BZ,BW},	"vpermute<0x31>(%0, %1)",	9,	3},
	{SHUFFLE,	{BX,BY,BZ,BW},	"vpermute<0x32>(%0, %1)",	9,	3},
	{SHUFFLE,	{BZ,BW,BZ,BW},	"vpermute<0x33>(%0, %1)",	9,	3},	//80

	//_unpackhi (covered by shuffle_pd)
//	{SHUFFLE,	{1,5,3,7},		"_mm256_unpackhi_pd(%0, %1)",3,	3},
//	{SHUFFLE,	{0,4,2,6},		"_mm256_unpacklo_pd(%0, %1)",3,	3},

//	{SHUFFLE,	{0,0,0,0},		"broadcast(3)",             3,	3},

	{SHUFFLE,	{AZ,AW,__,__},	"_mm256_extractf128_pd(%0, 1)",		9, 3},
	{SHUFFLE,	{BZ,BW,__,__},	"_mm256_extractf128_pd(%1, 1)",		9, 3},
//};
//
//
//Perm adds256[] = {
	{ADD,		{0x11,0x22,0x44,0x88}, "_mm256_add_pd(%0, %1)",				9,	2},	//83
	{ADD,		{0x03,0x30,0x0c,0xc0}, "_mm256_hadd_pd(%0, %1)",			9,	2},

	{ADD,		{0x01,0x02,0x04,0x08}, "_mm256_mask_add_pd(0x0, %0, %1)",	9,	2},
	{ADD,		{0x11,0x02,0x04,0x08}, "_mm256_mask_add_pd(0x1, %0, %1)",	9,	2},
	{ADD,		{0x01,0x22,0x04,0x08}, "_mm256_mask_add_pd(0x2, %0, %1)",	9,	2},
	{ADD,		{0x11,0x22,0x04,0x08}, "_mm256_mask_add_pd(0x3, %0, %1)",	9,	2},
	{ADD,		{0x01,0x02,0x44,0x08}, "_mm256_mask_add_pd(0x4, %0, %1)",	9,	2},
	{ADD,		{0x11,0x02,0x44,0x08}, "_mm256_mask_add_pd(0x5, %0, %1)",	9,	2},
	{ADD,		{0x01,0x22,0x44,0x08}, "_mm256_mask_add_pd(0x6, %0, %1)",	9,	2},
	{ADD,		{0x11,0x22,0x44,0x08}, "_mm256_mask_add_pd(0x7, %0, %1)",	9,	2},
	{ADD,		{0x01,0x02,0x04,0x88}, "_mm256_mask_add_pd(0x8, %0, %1)",	9,	2},
	{ADD,		{0x11,0x02,0x04,0x88}, "_mm256_mask_add_pd(0x9, %0, %1)",	9,	2},
	{ADD,		{0x01,0x22,0x04,0x88}, "_mm256_mask_add_pd(0xa, %0, %1)",	9,	2},
	{ADD,		{0x11,0x22,0x04,0x88}, "_mm256_mask_add_pd(0xb, %0, %1)",	9,	2},
	{ADD,		{0x01,0x02,0x44,0x88}, "_mm256_mask_add_pd(0xc, %0, %1)",	9,	2},
	{ADD,		{0x11,0x02,0x44,0x88}, "_mm256_mask_add_pd(0xd, %0, %1)",	9,	2},
	{ADD,		{0x01,0x22,0x44,0x88}, "_mm256_mask_add_pd(0xe, %0, %1)",	9,	2},
	{ADD,		{0x11,0x22,0x44,0x88}, "_mm256_mask_add_pd(0xf, %0, %1)",	9,	2},

	{ADD,		{0x00,0x00,0x00,0x00}, "_mm256_maskz_add_pd(0x0, %0, %1)",	9,	2},
	{ADD,		{0x11,0x00,0x00,0x00}, "_mm256_maskz_add_pd(0x1, %0, %1)",	9,	2},
	{ADD,		{0x00,0x22,0x00,0x00}, "_mm256_maskz_add_pd(0x2, %0, %1)",	9,	2},
	{ADD,		{0x11,0x22,0x00,0x00}, "_mm256_maskz_add_pd(0x3, %0, %1)",	9,	2},
	{ADD,		{0x00,0x00,0x44,0x00}, "_mm256_maskz_add_pd(0x4, %0, %1)",	9,	2},
	{ADD,		{0x11,0x00,0x44,0x00}, "_mm256_maskz_add_pd(0x5, %0, %1)",	9,	2},
	{ADD,		{0x00,0x22,0x44,0x00}, "_mm256_maskz_add_pd(0x6, %0, %1)",	9,	2},
	{ADD,		{0x11,0x22,0x44,0x00}, "_mm256_maskz_add_pd(0x7, %0, %1)",	9,	2},
	{ADD,		{0x00,0x00,0x00,0x88}, "_mm256_maskz_add_pd(0x8, %0, %1)",	9,	2},
	{ADD,		{0x11,0x00,0x00,0x88}, "_mm256_maskz_add_pd(0x9, %0, %1)",	9,	2},
	{ADD,		{0x00,0x22,0x00,0x88}, "_mm256_maskz_add_pd(0xa, %0, %1)",	9,	2},
	{ADD,		{0x11,0x22,0x00,0x88}, "_mm256_maskz_add_pd(0xb, %0, %1)",	9,	2},
	{ADD,		{0x00,0x00,0x44,0x88}, "_mm256_maskz_add_pd(0xc, %0, %1)",	9,	2},
	{ADD,		{0x11,0x00,0x44,0x88}, "_mm256_maskz_add_pd(0xd, %0, %1)",	9,	2},
	{ADD,		{0x00,0x22,0x44,0x88}, "_mm256_maskz_add_pd(0xe, %0, %1)",	9,	2},
	{ADD,		{0x11,0x22,0x44,0x88}, "_mm256_maskz_add_pd(0xf, %0, %1)",	9,	2},
};


string_accum &open_swizzle(string_accum &ba, int x, int y, int z, int w) {
	return ba << "template<> v8 swizzle<" << x << "," << y << "," << z << "," << w << ">(v8 a v8 b) { ";
}

string_accum &close_function(string_accum &ba, int cost) {
	return ba << "; }\t// cost = " << cost << "\n";
}


struct generate {

	ops<3>	map[9*9*9*9];

	int	find_cheapest(const Perm *perms, int x, int y, int z, int w) {
		int	best_index	= -1;
		int	best_cost	= maximum;

		for (int i = max(x, 0) + 9 * (max(y,0) + 9 * (max(z,0) + 9 * max(w, 0))), endx	= i + 9, endy = i + 9 * 9, endz = i + 9 * 9 * 9; i < 9 * 9 * 9 * 9; ) {
			if (!!map[i]) {
				int	cost = map[i].cost(perms);

				if (cost < best_cost) {
					best_cost	= cost;
					best_index	= i;
				}
			}

			if (x < 0) {
				++i;
				if (i < endx)
					continue;
			} else {
				i = endx;
			}
			endx += 9;

			if (y < 0) {
				if (i < endy)
					continue;
			} else {
				i = endy;
			}
			endy += 9 * 9;

			if (z < 0) {
				if (i < endz)
					continue;
			} else {
				i = endz;
			}
			endz += 9 * 9 * 9;

			if (w >= 0)
				break;

		}

		return best_index;
	}

	static int index(uint8 i) { return i ? lowest_set_index(i) : 8; }

	generate() {
		FileOutput	f("e:\\perms256.h");
		stream_accum<ostream_ref, 256>	ba(f);

		auto	onlyperms = make_range_n(perms256, 83);

		for (auto &i : onlyperms) {
		//for (auto _i = perms256 + 51; _i < end(perms256); _i++) { auto &i = *_i;
			perm<8>	a = {AX,AY,AZ,AW,BX,BY,BZ,BW};
			auto	b0 = i.apply(a);

			for (auto &j : onlyperms) {
			//for (auto _j = perms256 + 51; _j < end(perms256); _j++) { auto &j = *_j;
				auto	b1	= j.apply(a);
				perm<8>	b;
				memcpy(b.p, b0, 4);
				memcpy(b.p + 4, b1, 4);

				for (auto &k : onlyperms) {
				//for (auto _k = perms256 + 32; _k < end(perms256); _k++) { auto &k = *_k;
					auto	c		= k.apply(b);
					ops<3>	o(&i - perms256, &j - perms256, &k - perms256);
					int		cost	= o.cost(perms256);
					int		x		= index(c.p[0]) + 9 * (index(c.p[1]) + 9 * (index(c.p[2]) + 9 * index(c.p[3])));
					if (!map[x] || cost < map[x].cost(perms256))
						map[x] = o;
				}
			}
		}

		int	n = 0;
		for (int x = 0; x < 8; x++) {
			for (int y = 0; y < 8; y++) {
				for (int z = 0; z < 8; z++) {
					for (int w = 0; w < 8; w++) {
						int	i	= x + 9 * (y + 9 * (z + 9 * w));
						n += int(!!map[i]);

						open_swizzle(ba, x, y, z, w);
						map[i].print(ba, perms256);
						close_function(ba, map[i].cost(perms256));
					}
				}
			}
		}
		ISO_TRACEF(n * 100 / num_elements(map)) << "%\n";

		for (int x = -1; x < 8; x++) {
			for (int y = -1; y < 8; y++) {
				for (int z = -1; z < 8; z++) {
					for (int w = -1; w < 8; w++) {
						int	i = find_cheapest(perms256, x, y, z, w);
						open_swizzle(ba, x, y, z, w);
					#if 1
						map[i].print(ba, perms256);
					#else
						ba << "return swizzle<" << (i & 7) << ", " << ((i >> 3) & 7) << ", " << ((i >> 6) & 7) << ", " << ((i >> 9) & 7) << ">(a, b)";
					#endif
						close_function(ba, map[i].cost(perms256));
					}
				}
			}
		}

		ops<4>		adds[16];

		for (auto &op0 : perms256) {
			perm<8>	a = {AX,AY,AX,AW,__,__,__,__};
			auto	b0 = op0.apply(a);

			for (auto &op1 : perms256) {
				auto	b1	= op1.apply(a);
				perm<8>	b;
				memcpy(b.p, b0, 4);
				memcpy(b.p + 4, b1, 4);

				for (auto &op2 : perms256) {
					auto	b2 = op2.apply(b);
					perm<8>	c;
					memcpy(c.p, b0, 4);
					memcpy(c.p + 4, b2, 4);

					for (auto &op3 : perms256) {
						auto	r	= op3.apply(c);
					#if 0	// require result in first slot
						int	v = r.p[0];
						if (v != 0xff) {
							ops<4>	o(&op0 - perms256, &op1 - perms256, &op2 - perms256, &op3 - perms256);
							if (!adds[v] || o.cost(perms256) < adds[v].cost(perms256))
								adds[v] = o;
						}
					#elif 1	// require result in lowest slot of input
						for (int i = 0; i < 4; i++) {
							int	v = r.p[i];
							if (lowest_set_index(v) == i) {
								ISO_ASSERT(v!=7);
								ops<4>	o(&op0 - perms256, &op1 - perms256, &op2 - perms256, &op3 - perms256);
								if (!adds[v] || o.cost(perms256) < adds[v].cost(perms256))
									adds[v] = o;
							}
						}
					#else
						for (auto v : r.p) {
							if (v != 0xff) {
								ops<4>	o(&op0 - perms256, &op1 - perms256, &op2 - perms256, &op3 - perms256);
								if (!adds[v] || o.cost(perms256) < adds[v].cost(perms256))
									adds[v] = o;
							}
						}
					#endif
					}
				}
			}
		}

		for (int i = 0; i < 16; i++) {
			auto	&op = adds[i];
			if (!op)
				continue;

			ba << "template<> cvd4 vdot<" << i << ">(vd4 a, vd4 b) { a = mul(a, b); ";
			adds[i].print(ba, perms256);
			close_function(ba, 12 + adds[i].cost(perms256));
		}

	}

} _generate;