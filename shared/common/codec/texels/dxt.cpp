#define USE_FP16

#include "base/vector.h"
#include "maths/polynomial.h"
#include "maths/geometry.h"
#include "dxt.h"

using namespace iso;

template<int S, int...D, typename T> T reduce_bits_vec(T c) {
	constexpr T		mul = { bits(D)... };
	return to<element_type<T>>(div_round_bits<S>(fullmul(c, mul)));
}

template<int D, int...S, typename T> T extend_bits_vec(T c) {
	constexpr int	M	= max((D * (S - 1)) % S...);
	constexpr T		mul = { (bits(D) << M) / bits(S)... };
	return to<element_type<T>>(fullmul(c, mul) >> M);
}

template<int S, int...D, typename T> T quantise_bits_vec(T c) {
	return extend_bits_vec<S, D...>(reduce_bits<S, D...>(c));
}

template<typename T> inline T BCinterp(T e0, T e1, int Q, int i) {
	return div_round(e0 * (Q - i) + e1 * i, Q);
}
inline uint8x4 BCinterp(uint8x4 e0, uint8x4 e1, int Q, int i) {
	return to<uint8>(BCinterp(to<uint16>(e0), to<uint16>(e1), Q, i));
}
inline uint8x3 BCinterp(uint8x3 e0, uint8x3 e1, int Q, int i) {
	return to<uint8>(BCinterp(to<uint16>(e0), to<uint16>(e1), Q, i));
}
template<typename T> inline void BCinterp(T e0, T e1, T *interp, int Q) {
	for (int i = 0; i < Q; i++)
		*interp++ = BCinterp(e0, e1, Q, i);
}
template<typename T> inline void BCinterp(T e0, T e1, T *interp, int P, int Q) {
	for (int i = 0; i < Q; i++)
		*interp++ = BCinterp(e0, e1, P, div_round(i * P, Q - 1));
}

template<int N> struct BCaxis_s;

template<> struct BCaxis_s<3> {
	template<typename T, typename W> static float3 f(const T* pixels, uint32 n, float3 mean, W weights) {
		symmetrical3 covariance(zero, zero);
		for (int i = 0; i < n; i++) {
			float3 a = (to<float>(pixels[i]) - mean) * weights;
			covariance.d += a * a;
			covariance.o += a.xyx * a.yzz;
		}

		float3 vf = { .9f, 1.0f, .7f };
		for (uint32 i = 0; i < 3; ++i)
			vf = covariance * vf;

		return safe_normalise(vf);
	}
};

template<> struct BCaxis_s<4> {
	template<typename T, typename W> static float4 f(const T* pixels, uint32 n, float4 mean, W weights) {
		// Use incremental PCA for RGBA PCA, because it's simple
		float4	axis = safe_normalise(to<float>(pixels[0]) - mean);
		for (int i = 1; i < n; ++i) {
			float4 a = (to<float>(pixels[i]) - mean) * weights;
			axis = safe_normalise(axis + dot(a, axis) * a);
		}
		return axis;
	}
};

template<typename T, int N = num_elements_v<T>> vec<float, N> BCaxis(const T* pixels, uint32 n, vec<float, N> mean) {
	return BCaxis_s<N>::template f(pixels, n, mean, one);
}

template<typename T, int N = num_elements_v<T>, typename W> vec<float, N> BCaxis(const T* pixels, uint32 n, vec<float, N> mean, W weights = one) {
	return BCaxis_s<N>::template f(pixels, n, mean, weights);
}

template<typename T, int N = num_elements_v<T>> void BCpca_ends(const T *pixels, uint32 n, vec<float, N> &e0, vec<float, N> &e1) {
	// get mean color
	vec<float, N>	mean = zero;
	for (int i = 0; i < n; i++)
		mean += to<float>(pixels[i]);

	mean /= n;

	auto	axis = BCaxis(pixels, n, mean);

	interval<float>	d(none);
	for (int i = 0; i < n; ++i)
		d |= dot(to<float>(pixels[i]) - mean, axis);

	e0 = axis * d.a + mean;
	e1 = axis * d.b + mean;
}

template<typename T, int N = num_elements_v<T>> static void BCleast_squares_ends(const T *pixels, const uint8 *selectors, int P, int Q, uint32 n, vec<float, N> &e0, vec<float, N> &e1) {
	float3	z = zero;
	vec<float, N>	q = zero, t = zero;

	for (uint32 i = 0; i < n; i++) {
		auto	col = to<float>(pixels[i]);
		float2	w2	= barycentric(float(div_round(selectors[i] * P, Q - 1)) / P);
		//float2	w2	= { w, 1 - w };
		z += w2.xyy * w2.xxy;// float3{ w * w, (1 - w) * w, (1 - w) * (1 - w) };
		q += w2.x * col;
		t += col;
	}

	if (float det = z.x * z.z - z.y * z.y)
		z /= det;

	e0 =  (z.z + z.y) * q - z.y * t;
	e1 = -(z.y + z.x) * q + z.x * t;

	auto	same = e0 == e1;
	if (any(same)) {
		interval<vec<float, N>> v(none);
		for (uint32 i = 0; i < n; i++)
			v |= to<float>(pixels[i]);

		auto	sel = same & v.a == v.b;
		e0 = select(sel, v.a, e0);
		e1 = select(sel, v.b, e1);
	}
}

//-----------------------------------------------------------------------------
//	DXT decode
//-----------------------------------------------------------------------------

void DXTrgb::Decode(ISO_rgba *color) const {
	color[0] = Texel<B5G6R5>(v0);
	color[1] = Texel<B5G6R5>(v1);
	color[2] = InterpCol(color[0], color[1], 2, 1);
	color[3] = InterpCol(color[0], color[1], 1, 2);
}

void DXTa::Decode(uint8 *vals) const {
	vals[0] = v0;
	vals[1] = v1;
	if (v0 > v1) { // 8-vals block
		vals[2] = (6 * v0 + 1 * v1 + 3) / 7;
		vals[3] = (5 * v0 + 2 * v1 + 3) / 7;
		vals[4] = (4 * v0 + 3 * v1 + 3) / 7;
		vals[5] = (3 * v0 + 4 * v1 + 3) / 7;
		vals[6] = (2 * v0 + 5 * v1 + 3) / 7;
		vals[7] = (1 * v0 + 6 * v1 + 3) / 7;
	} else {		// 6-vals block.
		vals[2] = (4 * v0 + 1 * v1 + 2) / 5;
		vals[3] = (3 * v0 + 2 * v1 + 2) / 5;
		vals[4] = (2 * v0 + 3 * v1 + 2) / 5;
		vals[5] = (1 * v0 + 4 * v1 + 2) / 5;
		vals[6] = 0;
		vals[7] = 255;
	}
}

bool DXT1rec::Decode(const block<ISO_rgba, 2> &block, bool wii) const {
	ISO_rgba	color[4];
	color[0] = Texel<B5G6R5>(v0);
	color[1] = Texel<B5G6R5>(v1);
	if (wii) {
		if (v0 > v1)  {		// Four-colour block - opaque
			color[2] = InterpCol(color[0], color[1], 5, 3);
			color[3] = InterpCol(color[0], color[1], 3, 5);
		} else {			// Three-colour block + transparency
			color[2] = InterpCol(color[0], color[1], 1, 1);
			color[3] = color[2];
			color[3].a = 0;
		}
	} else {
		if (v0 > v1)  {		// Four-colour block - opaque
			color[2] = InterpCol(color[0], color[1], 2, 1);
			color[3] = InterpCol(color[0], color[1], 1, 2);
		} else {			// Three-colour block + transparency
			color[2] = InterpCol(color[0], color[1], 1, 1);
			color[3] = ISO_rgba(0,0,0,0);
		}
	}
	bool	three	= false;
	for (uint32 i = 0, d = bits, bmask = block_mask<4, 4>(block.size<1>(), block.size<2>()); bmask; i++, bmask >>= 1, d >>= 2) {
		if (bmask & 1) {
			three = three || ((d & 3) == 3);
			block[i >> 2][i & 3] = color[d & 3];
		}
	}
	return three && v0 <= v1;
}

void DXT23rec::Decode(const block<ISO_rgba, 2> &block) const {
	ISO_rgba	color[4];
	rgb.Decode(color);

	uint64	a	= alpha;
	for (uint32 i = 0, d = rgb.bits, bmask = block_mask<4, 4>(block.size<1>(), block.size<2>()); bmask; i++, bmask >>= 1, d >>= 2, a >>= 4) {
		if (bmask & 1)
			block[i >> 2][i & 3] = ISO_rgba(color[d & 3], (a & 15) * 17);
	}
}

void DXT45rec::Decode(const block<ISO_rgba, 2> &block) const {
	ISO_rgba		color[4];
	uint8			alphas[8];
	rgb.Decode(color);
	alpha.Decode(alphas);

	uint64	a	= alpha.bits48;
	for (uint32 i = 0, d = rgb.bits, bmask = block_mask<4, 4>(block.size<1>(), block.size<2>()); bmask; i++, bmask >>= 1, d >>= 2, a >>= 3) {
		if (bmask & 1)
			block[i >> 2][i & 3] = ISO_rgba(color[d & 3], alphas[a & 7]);
	}
}

void BC<4>::Decode(const block<ISO_rgba, 2> &block) const {
	uint8	reds[8];
	red.Decode(reds);

	uint64	r	= red.bits48;
	for (uint32 i = 0, bmask = block_mask<4, 4>(block.size<1>(), block.size<2>()); bmask; i++, bmask >>= 1, r >>= 3) {
		if (bmask & 1)
			block[i >> 2][i & 3] = ISO_rgba(reds[r & 7]);
	}
}

void BC<5>::Decode(const block<ISO_rgba, 2> &block) const {
	uint8	reds[8], greens[8];
	red.Decode(reds);
	green.Decode(greens);

	uint64	r	= red.bits48;
	uint64	g	= green.bits48;
	for (uint32 i = 0, bmask = block_mask<4, 4>(block.size<1>(), block.size<2>()); bmask; i++, bmask >>= 1, r >>= 3, g >>= 3) {
		if (bmask & 1)
			block[i >> 2][i & 3] =ISO_rgba(reds[r & 7], greens[g & 7], 0);
	}
}

//-----------------------------------------------------------------------------
//	DXT encode
//-----------------------------------------------------------------------------

template<int N> struct BCindexer {
	vec<float, N>	dir;
	float			off;
	int				max;
	template<typename T> BCindexer(T e0, T e1, int max) : max(max) {
		auto	d = to<float>(e1) - to<float>(e0);
		if (auto d2 = len2(d))
			dir = d / len2(d) * max;
		else
			dir = vec<float, N>(zero);
		off = -dot(dir, to<float>(e0));
	}
	template<typename T> int operator()(T x) {
		return clamp(int(dot(dir, to<float>(x)) + off), 0, max);
	}
};
template<> struct BCindexer<1> {
	float			dir, off;
	int				max;
	template<typename T> BCindexer(T e0, T e1, int max) : max(max) {
		dir = e0 == e1 ? 0 : max / float(e1 - e0);
		off = -dir * e0;
	}
	template<typename T> int operator()(T x) {
		return clamp(int(dir * x + off), 0, max);
	}
};

template<typename A, typename B> static inline auto weighted_distance(A a, B b, const int32x4 weights) {
	return dot(square(to<int>(a) - to<int>(b)), weights);
}

template<typename T, typename W> uint64 BCevaluate(T e0, T e1, int P, int Q, uint8 *selectors, const T* pixels, uint32 n, const W weights, uint32 best_err = ~0) {
	BCindexer<num_elements_v<T>>	ix(e0, e1, Q - 1);
	T				interp[Q];

	for (int i = 0; i < Q; i++)
		interp[i] = BCinterp(e0, e1, P, div_round(i * P, Q - 1));

	uint64 total_err = 0;
	while (n--) {
		auto	pixel = *pixels++;
		int		sel = ix(pixel);
		auto	err	= weighted_distance(interp[sel], pixel, weights);
		if (sel != Q - 1) {
			auto	err1 = weighted_distance(interp[sel + 1], pixel, weights);
			if (err1 < err) {
				++sel;
				err = err1;
			}
		}
		*selectors++ = sel;
		total_err += err;
		if (total_err >= best_err)
			break;
	}
	return total_err;
}

void DXTa::Encode(const uint8 *srce) {
	bool	got0 = false, got1 = false;

	int		mina = 255, maxa = 0;
	for (int i = 0; i < 16; i++) {
		mina = min(mina, srce[i]);
		maxa = max(maxa, srce[i]);
	}

	if (mina == 0) {
		mina = 255;
		for (int i = 0; i < 16; i++) {
			if (srce[i] > maxa / 8)
				mina = min(mina, srce[i]);
		}
		if (mina != 255 && (maxa == 255 || mina > maxa / 6))
			got0 = true;
		else
			mina = 0;
	}
	if (maxa == 255) {
		maxa = 0;
		for (int i = 0; i < 16; i++) {
			if (srce[i] < 255 - mina / 8)
				maxa = max(maxa, srce[i]);
		}
		if (maxa != 0 && (mina == 0 || maxa < 255 - mina / 6))
			got1 = true;
		else
			maxa = 255;
	}

	if (mina == 255) {
		mina = 0;
		maxa = 255;
	} else if (maxa == mina) {
		maxa = mina + 1;
	}

	uint64	d = 0;
	if (got0 || got1) {
		static uint8 xlat[] = {0,2,3,4,5,1};
		d	= mina | (maxa << 8);
		for (int i = 0; i < 16; i++) {
			int	a = srce[i];
			int	b = a < mina ? 6 : a > maxa ? 7 : xlat[(a - mina) * 5 / (maxa - mina)];
			d |= (int64)b << (i * 3 + 16);
		}
	} else {
		static uint8 xlat[] = {1,7,6,5,4,3,2,0};
		if (maxa - mina < 8) {
			if (mina < 256 - 7)
				maxa = mina + 7;
			else
				mina = maxa - 7;
		}
		d	= maxa | (mina << 8);
		for (int i = 0; i < 16; i++) {
			int	a = srce[i];
			int	b = (a - mina) * 7 / (maxa - mina);
			d |= (int64)xlat[b] << (i * 3 + 16);
		}
	}
	*(uint64*)this = d;
}

void iso::FixDXT5Alpha(uint8 *srce, uint32 srce_pitch, void *dest, uint32 block_size, uint32 dest_pitch, int width, int height) {
	for (int y = 0; y < height; y += 4) {
		for (int x = 0; x < width; x += 4) {
			uint8	alpha[16];
			for (int i = 0; i < 16; i++)
				alpha[i] = srce[min(y + (i >> 2), height - 1) * srce_pitch + min(x + (i & 3), width - 1) * 4];
			DXTa		t;
			t.Encode(alpha);
			uint16be*	p	= (uint16be*)((uint8*)dest + (y / 4) * dest_pitch + (x / 4) * block_size);
			for (int i = 0; i < 4; i++)
				p[i] = ((uint16*)&t)[i];
		}
	}
}

struct ColourSet {
	uint32	count;
	uint8x4	values[16];
	float4	weighted[16];
	float4	sum;
	float3	mean;
	float3	axis;
	int8	remap[16];
	uint32	mask, trans_mask;

	ColourSet(const block<ISO_rgba, 2>& block, bool trans);
};

ColourSet::ColourSet(const block<ISO_rgba, 2> &block, bool trans) : mask(block_mask<4, 4>(block.size<1>(), block.size<2>())) {
	trans_mask	= 0;
	count		= 0;

	for (uint32 i = 0, m = mask; m; i++, m >>= 1) {
		int	j = -1;

		if (m & 1) {
			auto	c	= (const uint8x4&)block[i >> 2][i & 3];

			// check for transparent pixels when using dxt1
			if (trans && c.w < 128) {
				trans_mask |= 1 << i;

			} else {
				// loop over previous points for a match
				for (j = 0; j < count; ++j) {
					if (all(values[j].xyz == c.xyz)) {
						++values[j].w;
						break;
					}
				}
				if (j == count)
					values[count++] = concat(c.xyz, one);
			}
		}
		remap[i] = j;
	}

	sum = zero;
	for (int i = 0; i < count; i++) {
		auto	v	= to<float>(values[i].xyz, one) * values[i].w;
		weighted[i]	= concat(v.xyz, sqrt(v.w));
		sum			+= v;
	}

	mean = sum.xyz / sum.w;

	symmetrical3 covariance(zero, zero);
	for (int i = 0; i < count; i++) {
		float3 a		= to<float>(values[i].xyz) - mean;
		float3 b		= a * values[i].w;
		covariance.d	+= a * b;
		covariance.o	+= a.xyx * b.yzz;
	}

	float3 vf = { .9f, 1.0f, .7f };
	for (uint32 i = 0; i < 3; ++i)
		vf = covariance * vf;

	axis = safe_normalise(vf);
}

void SolidFit(uint8x3 col, uint8x3 *ends, uint8 *selectors, uint32 transmask, uint32 flags) {
	bool	wii		= false;//!!(flags & DXTENC_WIIFACTORS);
#if 1
	struct OptimalTable {
		uint8x2	table[256];
		OptimalTable(int n, int factor1, int factor2) {
			uint8	n2		= 1 << n;
			int		f1		= factor1 * 255;
			int		f2		= factor2 * 255;
			int		s		= (factor1 + factor2) << n;

			for (int i = 0; i < 256; i++) {
				int	beste = 256 * s;
				for (uint8 min = 0; min < n2; min++) {
					for (uint8 max = min; max < n2; max++) {
						int	e = abs(max * f1 + min * f2 - i * s);
						if (e < beste) {
							table[i] = { min, max };
							beste = e;
						}
					}
				}
			}
		}
		uint8x2	operator[](uint8 i) {
			return table[i];
		}
	};

	uint8x2	r, g, b;
	if (transmask) {
		static OptimalTable	optimal5_trans(5, 2, 2);
		static OptimalTable	optimal6_trans(6, 2, 2);
		r = optimal5_trans[col.x];
		g = optimal6_trans[col.y];
		b = optimal5_trans[col.z];
	} else if (wii) {
		static OptimalTable	optimal5_wii(5, 3, 5);
		static OptimalTable	optimal6_wii(6, 3, 5);
		r = optimal5_wii[col.x];
		g = optimal6_wii[col.y];
		b = optimal5_wii[col.z];
	} else {
		static OptimalTable	optimal5(5, 1, 2);
		static OptimalTable	optimal6(6, 1, 2);
		r = optimal5[col.x];
		g = optimal6[col.y];
		b = optimal5[col.z];
	}
	ends[0] = { r.x, g.x, b.x };
	ends[1] = { r.y, g.y, b.y };

#else

	start	= floor(col * grid) / grid;
	end		= ceil(col * grid) / grid;
	float3	codes[5] = {
		start,
		end,
		lerp(start, end, factor1.x),
		lerp(start, end, factor2.x),
		(start + end) * half
	};

	float	bestd;
	for (int i = 0; i < 5; i++) {
		float d = len2(metric * (col - codes[i]));
		if (i == 0 || d < bestd) {
			bestd	= d;
			besti	= i;
		}
	}
	if (besti == 4) {
		transparent = true;
		besti = 2;
	}
#endif

	//ends[0] = ends[1] = reduce_bits_vec<8, 5,6,5>(values[0].xyz);
	for (int i = 0; i < 16; i++)
		selectors[i] = 1;
}

uint32 RangeFit(const ColourSet &colours,  uint8x3 *ends, uint8 *selectors, uint32 flags) {
	// find range of colours along axis
	interval<float>	d(none);
	for (int i = 0; i < colours.count; ++i)
		d |= dot(to<float>(colours.values[i].xyz) - colours.mean, colours.axis);

	// clamp to the grid and save
	ends[0]	= reduce_bits_vec<8, 5,6,5>(to_sat<uint8>(colours.axis * d.a + colours.mean));
	ends[1]	= reduce_bits_vec<8, 5,6,5>(to_sat<uint8>(colours.axis * d.b + colours.mean));

	// map values to codes
	auto	weights = flags & DXTENC_PERCEPTUALMETRIC ? int32x4{ 3,6,1,0 } : int32x4{ 1,1,1,0 };
	auto	e0 = grow<4>(extend_bits_vec<8, 5, 6, 5>(ends[0]));
	auto	e1 = grow<4>(extend_bits_vec<8, 5, 6, 5>(ends[1]));

	return colours.trans_mask
		? BCevaluate(e0, e1, 2, 3, selectors, colours.values, colours.count, weights)
		: BCevaluate(e0, e1, 3, 4, selectors, colours.values, colours.count, weights);
}

void ClusterFit(const ColourSet &colours, uint8x3 *ends, uint8 *selectors, uint32 flags) {
	enum { MAX_ITERATIONS = 8 };
	uint8	order[MAX_ITERATIONS][16];

	const uint8x4*	values	= colours.values;
	int		count			= colours.count;
	bool	transparent		= !!colours.trans_mask;
	bool	wii				= false;//!!(flags & DXTENC_WIIFACTORS);

	const float3	metric	= flags & DXTENC_PERCEPTUALMETRIC ? float3{0.2126f, 0.7152f, 0.0722f} : float3(one);
	const float4	factor1	= transparent ? float4{.5f, 0.5f, 0.5f, 0.25f} : wii ? float4{3/8.f, 3/8.f, 3/8.f,  9/64.f} : float4{1/3.f, 1/3.f, 1/3.f, 1/9.f};
	const float4	factor2	= transparent ? float4{.0f, 1.0f, 1.0f, 1.0f}  : wii ? float4{5/8.f, 5/8.f, 5/8.f, 25/64.f} : float4{2/3.f, 2/3.f, 2/3.f, 4/9.f};
	const float		factorw	= factor2.w * half;//(wii ? 25 / 64.f : 4 / 9.f) / 2;
	const float3	grid	= float3{31, 63, 31} / 255;

	if (count < 2) {
		if (count == 0) {
			ends[0] = ends[1] = 0;
			for (int i = 0; i < 16; i++)
				selectors[i] = 0;
		} else {
			SolidFit(values[0].xyz, ends, selectors, colours.trans_mask, flags);
		}
		return;
	}

	// check all possible clusters and iterate on the total order
	float	best_error	= maximum;
	int		best_it		= 0, best_i = 0, best_j = 0, best_k = 0;
	float3	best_a, best_b;

	// loop over iterations (we avoid the case that all points in first or last cluster)
	float3	axis = colours.axis;
	for (int it = 0, nit = flags & DXTENC_ITERATIVE ? MAX_ITERATIONS : 1; it < nit; it++) {

		uint8*	o = order[it];
		float	d[16];
		for (int i = 0; i < count; i++) {
			d[i]	= dot(to<float>(values[i].xyz), axis);
			o[i]	= i;
		}

		// stable sort
		for (int i = 0; i < count; i++) {
			for (int j = i; j > 0 && d[j] < d[j - 1]; --j) {
				swap(d[j], d[j - 1]);
				swap(o[j], o[j - 1]);
			}
		}

		// check this ordering is unique
		bool	found = false;
		for (int i = 0; i < it && !found; i++)
			found = memcmp(o, order[i], count) == 0;

		if (found)
			break;

		// copy the ordering
		float4	weighted[16];
		for (int i = 0; i < count; i++)
			weighted[i] = colours.weighted[o[i]];

		// first cluster [0,i) is at the start
		float4 part0 = float4(zero);
		for (int i = 0; i < count; part0 += weighted[i++]) {

			// second cluster [i,j) is one third along (or skipped for trans)
			float4 part1 = float4(zero);
			for (int j = i; j < count; part1 += weighted[j++]) {

				// third cluster [j,k) is two thirds along (or half along for trans)
				float4	part2 = zero;
				for (int k = j; k < count; part2 += weighted[k++]) {
					if (k == 0)
						continue;

					// last cluster [k,count) is at the end
					float4 part3 = colours.sum - part2 - part1 - part0;

					// compute least squares terms directly
					const float4 alphax_sum		= part2 * factor1 + part1 * factor2 + part0;
					const float4 betax_sum		= part1 * factor1 + part2 * factor2 + part3;
					const float alphabeta_sum	= (part1.w + part2.w) * factorw;

					// compute the least-squares optimal points
					float	factor	= reciprocal(alphax_sum.w * betax_sum.w - square(alphabeta_sum));
					float3	a		= (alphax_sum.xyz * betax_sum.w - betax_sum.xyz  * alphabeta_sum) * factor;
					float3	b		= (betax_sum.xyz * alphax_sum.w - alphax_sum.xyz * alphabeta_sum) * factor;

					// clamp to the grid
					a	= trunc(clamp(a, zero, 255) * grid + half) / grid;
					b	= trunc(clamp(b, zero, 255) * grid + half) / grid;
#if 0
					if (a == b) {
						float3	d = b0 - b;
						int		c = max_component_index(abs(d));
						b[c] += (d[c] < 0.f ? -1.0f : 1.0f) / grid[c];
					}
#endif
					// compute the error (we skip the constant xxsum)
					float3 e	= square(a) * alphax_sum.w + square(b) * betax_sum.w
								+ (a * b * alphabeta_sum - a * alphax_sum.xyz - b * betax_sum.xyz) * 2;

					// apply the metric to the error term & keep the solution if it wins
					float error = dot(abs(e), metric);
					if (error < best_error) {
						best_error	= error;
						best_a		= a;
						best_b		= b;
						best_i		= i;
						best_j		= j;
						best_k		= k;
						best_it		= it;
					}
				}
			}
			if (transparent)
				break;
		}

		// stop if we didn't improve in this iteration
		if (best_it != it)
			break;

		// new axis to try
		axis = best_b - best_a;
	}

	ends[0] = reduce_bits_vec<8, 5,6,5>(to<uint8>(best_a));
	ends[1] = reduce_bits_vec<8, 5,6,5>(to<uint8>(best_b));

	const uint8 *o	= order[best_it];
	int			m	= 0;

	while (m < best_i)
		selectors[o[m++]] = 0;

	if (transparent) {
		while (m < best_k)
			selectors[o[m++]] = 2;
	} else {
		while (m < best_j)
			selectors[o[m++]] = 2;
		while (m < best_k)
			selectors[o[m++]] = 3;
	}

	while (m < count)
		selectors[o[m++]] = 1;
}

uint16 rgb565(uint8x3 c) {
	//	auto	c1 = reduce_bits<8, 5, 6, 5>(c);
	return (c.x << 11) | (c.y << 5) | c.z;
}

static const uint8 selmap[] = { 0, 2, 3, 1 };

uint32 DXT1bits(const uint8 *selectors, const int8 *pixmap, const uint8 *selmap = ::selmap) {
	uint32		bits = 0;
	for (int i = 16; i--; ) {
		auto	m = pixmap[i];
		bits = (bits << 2) | selmap[m < 0 ? 3 : selectors[m]];
	}
	return bits;
}

void DXT1rec::Encode(const block<ISO_rgba, 2> &block, uint32 flags) {
	uint8		selectors[16];
	uint8x3		ends[2];
	ColourSet	cs(block, true);

	if (cs.count == 1) {
		SolidFit(cs.values[0].xyz, ends, selectors, cs.trans_mask, flags);
	} else {
		RangeFit(cs, ends, selectors, flags);
		//ClusterFit(cs, ends, selectors, flags);
	}

	uint16		rgb0 = rgb565(ends[0]);
	uint16		rgb1 = rgb565(ends[1]);

	const uint8	*remap = selmap;

	if (cs.trans_mask) {
		if (rgb0 > rgb1) {
			static	uint8	remapa[] = {1,2,3,0};// {1, 0, 2, 3};
			swap(rgb0, rgb1);
			remap = remapa;
		}
	} else { // opaque
		static	uint8	remap0[]	= { 0,2,2,0 };//{ 0,0,2,2 };
		static	uint8	remap1[]	= { 1,3,3,1 };//{ 1,1,3,3 };
		static	uint8	remap01[]	= { 1,3,2,0 };//{ 1,0,3,2 };
		if (rgb0 == rgb1) { // have to flip one bit so they're not equal, but keep rgb0 > rgb1
			if (rgb0 & 0x1f) {
				--rgb0;
				remap = remap0;
			} else {
				++rgb0;
				remap = remap1;
			}
		} else if (rgb0 < rgb1) {
			swap(rgb0, rgb1);
			remap = remap01;
		}
	}
	for (auto &i : selectors)
		i = remap[i];

	set(rgb0, rgb1, DXT1bits(selectors, cs.remap, remap));

	ISO_rgba	test[16];
	Decode(make_block(test, 4, 4));
}

void DXT23rec::Encode(const block<ISO_rgba, 2> &block, uint32 flags) {
	uint8		selectors[16];
	uint8x3		ends[2];
	ColourSet	cs(block, false);

	if (cs.count == 1)
		SolidFit(cs.values[0].xyz, ends, selectors, cs.trans_mask, flags);
	else
		RangeFit(cs, ends, selectors, flags);

	rgb.set(rgb565(ends[0]), rgb565(ends[1]), DXT1bits(selectors, cs.remap));

	uint8	temp[16];
	for (uint32 i = 0, m = cs.mask; m; i++, m >>= 1)
		temp[i] = m & 1 ? block[i >> 2][i & 3].a : 0;

	uint64	d;
	for (int i = 16; i--;)
		d = (d << 4) | ((temp[i] * 15 + 128) / 255);
	alpha = d;
}

void DXT45rec::Encode(const block<ISO_rgba, 2> &block, uint32 flags) {
	ColourSet	cs(block, false);
	uint8		selectors[16];
	uint8x3		ends[2];

	if (cs.count == 1)
		SolidFit(cs.values[0].xyz, ends, selectors, cs.trans_mask, flags);
	else
		RangeFit(cs, ends, selectors, flags);

	rgb.set(rgb565(ends[0]), rgb565(ends[1]), DXT1bits(selectors, cs.remap));

	uint8	temp[16];
	for (uint32 i = 0, m = cs.mask; m; i++, m >>= 1)
		temp[i] = m & 1 ? block[i >> 2][i & 3].a : 0;

	alpha.Encode(temp);
}

void BC<4>::Encode(const block<ISO_rgba, 2> &block) {
	uint8	reds[16];
	clear(reds);
	int		w	= block.size<1>(), h = block.size<2>();
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++)
			reds[x + y * 4] = block[y][x].r;
	}
	red.Encode(reds);
}

void BC<5>::Encode(const block<ISO_rgba, 2> &block) {
	uint8	reds[16], greens[16];
	clear(reds);
	clear(greens);
	int		w	= block.size<1>(), h = block.size<2>();
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			reds[x + y * 4] = block[y][x].r;
			greens[x + y * 4] = block[y][x].g;
		}
	}
	red.Encode(reds);
	green.Encode(greens);
}

//-----------------------------------------------------------------------------
//	BC6 & 7
//-----------------------------------------------------------------------------

static const uint16 BCmask2[] = {
	0xcccc, 0x8888, 0xeeee, 0xecc8, 0xc880, 0xfeec, 0xfec8, 0xec80,
	0xc800, 0xffec, 0xfe80, 0xe800, 0xffe8, 0xff00, 0xfff0, 0xf000,
	0xf710, 0x008e, 0x7100, 0x08ce, 0x008c, 0x7310, 0x3100, 0x8cce,
	0x088c, 0x3110, 0x6666, 0x366c, 0x17e8, 0x0ff0, 0x718e, 0x399c,
	0xaaaa, 0xf0f0, 0x5a5a, 0x33cc, 0x3c3c, 0x55aa, 0x9696, 0xa55a,
	0x73ce, 0x13c8, 0x324c, 0x3bdc, 0x6996, 0xc33c, 0x9966, 0x0660,
	0x0272, 0x04e4, 0x4e40, 0x2720, 0xc936, 0x936c, 0x39c6, 0x639c,
	0x9336, 0x9cc6, 0x817e, 0xe718, 0xccf0, 0x0fcc, 0x7744, 0xee22,
};

static const uint16 BCmask3a[] = {
	0x08cc, 0x8cc8, 0xcc80, 0xec00, 0x3300, 0x00cc, 0xff00, 0xcccc,
	0x0f00, 0x0ff0, 0x00f0, 0x4444, 0x6666, 0x2222, 0x136c, 0x008c,
	0x36c8, 0x08ce, 0x3330, 0xf000, 0x00ee, 0x8888, 0x22c0, 0x4430,
	0x0c22, 0x0344, 0x6996, 0x9960, 0x0330, 0x0066, 0xc22c, 0x8c00,
	0x1300, 0xc400, 0x004c, 0x2222, 0x00f0, 0x2492, 0x2942, 0xc30c,
	0xc03c, 0x00aa, 0xaa00, 0x3030, 0xc0c0, 0x9090, 0xa00a, 0xaaa0,
	0x0aaa, 0xe0e0, 0x7070, 0x6660, 0x0ee0, 0x0770, 0x0666, 0x6600,
	0x0066, 0x0cc0, 0x0330, 0x6000, 0x8080, 0x1010, 0x000a, 0x08ce,
};
static const uint16 BCmask3b[] = {
	0xf600, 0x7300, 0x3310, 0x00ce, 0xcc00, 0xcc00, 0x00cc, 0x3300,
	0xf000, 0xf000, 0xff00, 0x8888, 0x8888, 0xcccc, 0xec80, 0x7310,
	0xc800, 0x3100, 0xccc0, 0x0ccc, 0xee00, 0x7700, 0xcc00, 0x3300,
	0x00cc, 0xfc88, 0x0660, 0x6600, 0xc88c, 0xf900, 0x0cc0, 0x7310,
	0xec80, 0x08ce, 0xec80, 0x4444, 0x0f00, 0x4924, 0x4294, 0x0c30,
	0x03c0, 0xff00, 0x5500, 0xcccc, 0x0c0c, 0x6666, 0x0ff0, 0x5550,
	0xf000, 0x0e0e, 0x8888, 0x9990, 0xe00e, 0x8888, 0xf000, 0x9900,
	0xff00, 0xc00c, 0xcccc, 0x9000, 0x0808, 0xeeee, 0xfff0, 0x7310,
};

uint8 BCanchor2[] = {
	15,15,15,15,15,15,15,15,
	15,15,15,15,15,15,15,15,
	15, 2, 8, 2, 2, 8, 8,15,
	 2, 8, 2, 2, 8, 8, 2, 2,
	15,15, 6, 8, 2, 8,15,15,
	 2, 8, 2, 2, 2,15,15, 6,
	 6, 2, 6, 8,15,15, 2, 2,
	15,15,15,15,15, 2, 2,15,
};
uint8 BCanchor3a[] = {
	 3, 3,15,15, 8, 3,15,15,
	 8, 8, 6, 6, 6, 5, 3, 3,
	 3, 3, 8,15, 3, 3, 6,10,
	 5, 8, 8, 6, 8, 5,15,15,
	 8,15, 3, 5, 6,10, 8,15,
	15, 3,15, 5,15,15,15,15,
	 3,15, 5, 5, 5, 8, 5,10,
	 5,10, 8,13,15,12, 3, 3,
};
uint8 BCanchor3b[] = {
	15, 8, 8, 3,15,15, 3, 8,
	15,15,15,15,15,15,15, 8,
	15, 8,15, 3,15, 8,15, 8,
	 3,15, 6,10,15,15,10, 8,
	15, 3,15,10,10, 8, 9,10,
	 6,15, 8,15, 3, 6, 6, 8,
	15, 3,15,15,15,15,15,15,
	15,15,15,15, 3,15,15, 8,
};

//template<int B> struct BCweights { static const uint8 weights[]; };
//template<> const uint8 BCweights<2>::weights[] = { 0, 21, 43, 64 };
//template<> const uint8 BCweights<3>::weights[] = { 0, 9, 18, 27, 37, 46, 55, 64 };
//template<> const uint8 BCweights<4>::weights[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

// Partition order sorted by usage frequency across a large test corpus
// Pattern 34 (checkerboard) must appear in slot 34
static const uint8 BCpartition_order2[64] = {
	 0, 13,  1,  2, 15, 14, 10, 16,
	 3, 23, 26,  6,  7, 21, 19, 29,
	 8,  4,  9, 20,  5, 31, 22, 17,
	18, 11, 12, 30, 24, 25, 28, 27,

	32, 33, 34, 45, 46, 51, 49, 50,
	48, 38, 39, 37, 53, 52, 54, 36,
	57, 58, 55, 41, 40, 42, 43, 59,
	44, 56, 47, 35, 60, 63, 62, 61
};

uint64 insert0(uint64 a, uint32 b) {
	uint64	m = bits64(b);
	return (a & m) | ((a & ~m) << 1);
}
uint64 remove0(uint64 a, uint32 b) {
	ISO_ASSERT(!(a & bit(b)));
	uint64	m = bits64(b);
	return (a & m) | ((a & ~m) >> 1);
}

template<int N>	inline uint64 BCput(uint64 b, uint64 x)		{ return (b >> N) | (x << (64 - N)); }
template<>		inline uint64 BCput<0>(uint64 b, uint64 x)	{ return b; }

template<int NS> struct BCindex;

template<> struct BCindex<1> {
	template<int B, typename T> static void	deindex(T block[16], T *interp, uint32 partition, uint64 indices) {
		indices	= insert0(indices, B - 1);
		for (int i = 0; i < 16; i++, indices >>= B)
			block[i] = interp[indices & bits(B)];
	}
	template<int B> static uint64 get_indices(const uint8 selectors[16], uint32 partition) {
		uint64	indices	= 0;
		for (int i = 0; i < 16; i++)
			indices |= uint64(selectors[i]) << (B * i);
		return remove0(indices, B - 1);
	}
};

template<> struct BCindex<2> {
	template<int B, typename T> static void	deindex(T block[16], T *interp, uint32 partition, uint64 indices) {
		indices = insert0(insert0(indices, B - 1), BCanchor2[partition] * B + B - 1);
		uint16	pmask	= BCmask2[partition];
		for (int i = 0; i < 16; i++, indices >>= B, pmask >>= 1)
			block[i] = interp[(indices & bits(B)) | ((pmask & 1) << B)];
	}
	template<int B> static uint64 get_indices(const uint8 selectors[16], uint32 partition) {
		uint64	indices	= 0;
		for (int i = 0; i < 16; i++)
			indices |= uint64(selectors[i]) << (B * i);
		return remove0(remove0(indices, BCanchor2[partition] * B + B - 1), B - 1);
	}
};

template<> struct BCindex<3> {
	template<int B, typename T> static void	deindex(T block[16], T *interp, uint32 partition, uint64 indices) {
		int		anchor1 = BCanchor3a[partition], anchor2 = BCanchor3b[partition];
		if (anchor1 > anchor2)
			swap(anchor1, anchor2);
		indices = insert0(insert0(insert0(indices, B - 1), anchor1 * B + B - 1), anchor2 * B + B - 1);
		uint32	pmask	= BCmask3a[partition] | (BCmask3b[partition] << 16);
		for (int i = 0; i < 16; i++, indices >>= B, pmask >>= 1)
			block[i] = interp[indices & bits(B) | (((pmask & 1) | ((pmask >> 15) & 2)) << B)];
	}
	template<int B> static uint64 get_indices(const uint8 selectors[16], uint32 partition) {
		uint64	indices	= 0;
		for (int i = 0; i < 16; i++)
			indices |= uint64(selectors[i]) << (B * i);
		int		anchor1 = BCanchor3a[partition], anchor2 = BCanchor3b[partition];
		if (anchor1 > anchor2)
			swap(anchor1, anchor2);
		return remove0(remove0(remove0(indices, anchor2 * B + B - 1), anchor1 * B + B - 1), B - 1);
	}
};

bool BCanchor(uint8* selectors, uint32 mask, uint8 anchor, int IB) {
	bool	has_anchor = mask & (1 << anchor);
	if (has_anchor && (selectors[anchor] & bit(IB - 1))) {
		uint8* d = selectors;
		for (uint32 m = mask; m; m >>= 1, ++d) {
			if (m & 1)
				*d ^= bits(IB);
		}
		return true;
	}
	if (!has_anchor)
		selectors[anchor] = 0;

	return false;
}

void BCadjust_selectors(const uint8* selectors, uint32 n, int maxsel, uint8 *selectors1, uint8 *selectors2, uint8 *selectors3) {
	// try varying the selectors a little, somewhat like cluster fit would
	interval<uint32>	sel(none);
	for (uint32 i = 0; i < n; i++)
		sel |= selectors[i];

	for (uint32 i = 0; i < n; i++) {
		uint32 j = selectors[i], j1 = j, j2 = j;
		if (j == sel.a && j < maxsel)
			j1 = ++j;
		else if (j == sel.b && j > 0)
			j2 = --j;
		selectors1[i] = j1;
		selectors2[i] = j2;
		selectors3[i] = j;
	}
}

//-----------------------------------------------------------------------------
//	BC6H
//-----------------------------------------------------------------------------
/*
MN		Q	R G B  delta	0-4								5-14								15-24								25-34								35-44							45-54							55-64							65-76									77-81	82-127
----	-----------------	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
1	0	10	5 5 5	Y		m[1:0],g2[4],b2[4],b3[4],		r0[9:0],							g0[9:0],							b0[9:0],							r1[4:0],g3[4],g2[3:0],			g1[4:0],b3[0],g3[3:0],			b1[4:0],b3[1],b2[3:0],			r2[4:0],b3[2],r3[4:0],b3[3],			d[4:0]	indices:46
2	1	7	6 6 6	Y		m[1:0],g2[5],g3[4],g3[5],		r0[6:0],b3[0],b3[1],b2[4],			g0[6:0],b2[5],b3[2],g2[4],			b0[6:0],b3[3],b3[5],b3[4],			r1[5:0],g2[3:0],				g1[5:0],g3[3:0],				b1[5:0],b2[3:0],				r2[5:0],r3[5:0],						d[4:0]	indices:46
3	2	11	5 4 4	Y		m[4:0],							r0[9:0],							g0[9:0],							b0[9:0],							r1[4:0],r0[10],g2[3:0],			g1[3:0],g0[10],b3[0],g3[3:0],	b1[3:0],b0[10],b3[1],b2[3:0],	r2[4:0],b3[2],r3[4:0],b3[3],			d[4:0]	indices:46
4	6	11	4 5 4	Y		m[4:0],							r0[9:0],							g0[9:0],							b0[9:0],							r1[3:0],r0[10],g3[4],g2[3:0],	g1[4:0],g0[10],g3[3:0],			b1[3:0],b0[10],b3[1],b2[3:0],	r2[3:0],b3[0],b3[2],r3[3:0],g2[4],b3[3],d[4:0]	indices:46
5	10	11	4 4 5	Y		m[4:0],							r0[9:0],							g0[9:0],							b0[9:0],							r1[3:0],r0[10],b2[4],g2[3:0],	g1[3:0],g0[10],b3[0],g3[3:0],	b1[4:0],b0[10],b2[3:0],			r2[3:0],b3[1],b3[2],r3[3:0],b3[4],b3[3],d[4:0]	indices:46
6	14	9	5 5 5	Y		m[4:0],							r0[8:0],b2[4],						g0[8:0],g2[4],						b0[8:0],b3[4],						r1[4:0],g3[4],g2[3:0],			g1[4:0],b3[0],g3[3:0],			b1[4:0],b3[1],b2[3:0],			r2[4:0],b3[2],r3[4:0],b3[3],			d[4:0]	indices:46
7	18	8	6 5 5	Y		m[4:0],							r0[7:0],g3[4],b2[4],				g0[7:0],b3[2],g2[4],				b0[7:0],b3[3],b3[4],				r1[5:0],g2[3:0],				g1[4:0],b3[0],g3[3:0],			b1[4:0],b3[1],b2[3:0],			r2[5:0],r3[5:0],						d[4:0]	indices:46
8	22	8	5 6 5	Y		m[4:0],							r0[7:0],b3[0],b2[4],				g0[7:0],g2[5],g2[4],				b0[7:0],g3[5],b3[4],				r1[4:0],g3[4],g2[3:0],			g1[5:0],g3[3:0],				b1[4:0],b3[1],b2[3:0],			r2[4:0],b3[2],r3[4:0],b3[3],			d[4:0]	indices:46
9	26	8	5 5 6	Y		m[4:0],							r0[7:0],b3[1],b2[4],				g0[7:0],b2[5],g2[4],				b0[7:0],b3[5],b3[4],				r1[4:0],g3[4],g2[3:0],			g1[4:0],b3[0],g3[3:0],			b1[5:0],b2[3:0],				r2[4:0],b3[2],r3[4:0],b3[3],			d[4:0]	indices:46
10	30	6	6 6 6	N		m[4:0],							r0[5:0],g3[4],b3[0],b3[1],b2[4],	g0[5:0],g2[5],b2[5],b3[2],g2[4],	b0[5:0],g3[5],b3[3],b3[5],b3[4],	r1[5:0],g2[3:0],				g1[5:0],g3[3:0],				b1[5:0],b2[3:0],				r2[5:0],r3[5:0],						d[4:0]	indices:46
11	3	10	10		N		m[4:0],							r0[9:0],							g0[9:0],							b0[9:0],							r1[9:0],						g1[9:0],						b1[9:0],						indices:63
12	7	11	9		Y		m[4:0],							r0[9:0],							g0[9:0],							b0[9:0],							r1[8:0],r0[10],					g1[8:0],g0[10],					b1[8:0],b0[10],					indices:63
13	11	12	8		Y		m[4:0],							r0[9:0],							g0[9:0],							b0[9:0],							r1[7:0],r0[10:11],				g1[7:0],g0[10:11],				b1[7:0],b0[10:11],				indices:63
14	15	16	4		Y		m[4:0],							r0[9:0],							g0[9:0],							b0[9:0],							r1[3:0],r0[10:15],				g1[3:0],g0[10:15],				b1[3:0],b0[10:15],				indices:63
*/

template<typename T> void BC6setcol(T& d, int32x3 c) {
	d.r = c.x;
	d.g = c.y;
	d.b = c.z;
}

inline int32x3 BC6int(float3 c, bool sign) {
	return to<int>(as<int16>(to<hfloat>(c))) * (sign ? 32 : 64) / 31;
}
inline float3 BC6float(int32x3 c, bool sign) {
	return to<float>(as<hfloat>(to<uint16>(
		sign
		? select(c < 0, ((c * -31) >> 5) | 0x8000, (c * 31) >> 5)
		: (c * 31) >> 6
	)));
}

template<int S, int D, typename T> T BC6unquant_helper(T c) {
	return S >= D ? c : select(c == 0, c, min((c << (D - S)) + bit(D - S - 1), bits(D)));
}
template<int S> int32x3 BC6unquant(int32x3 c, bool sign) {
	return sign
		? set_sign(BC6unquant_helper<S, 15>(abs(c)), c < 0)
		: BC6unquant_helper<S, 16>(c);
}

template<int S, int D, typename T> T BC6quant_helper(T c) {
	return D >= S ? c : min(max(c - bit(S - D - 1), zero) >> (S - D), bits(D));
}
template<int D> int32x3 BC6quant(int32x3 c, bool sign) {
	return sign
		? set_sign(BC6quant_helper<15, D>(abs(c)), c < 0)
		: BC6quant_helper<16, D>(c);
}

template<typename C> int32x3 BC6extend(const C &c, bool sign) {
	return sign
		? int32x3 {int(sign_extend<decltype(c.r)::BITS>(c.r.get())), int(sign_extend<decltype(c.g)::BITS>(c.g.get())), int(sign_extend<decltype(c.b)::BITS>(c.b.get()))}
		: int32x3 {int(c.r), int(c.g), int(c.b)};
}

void BC6common1(int32x3 block[16], int32x3 ends[2], uint64 indices, bool sign) {
	int32x3	interp[16];
	BCinterp(ends[0], ends[1], interp, 64, 16);
	BCindex<1>::template deindex<4>(block, interp, 0, indices);
}

void BC6common2(int32x3 block[16], int32x3 ends[4], uint8 partition, uint64 indices, bool sign) {
	int32x3	interp[16];
	BCinterp(ends[0], ends[1], interp + 0, 64, 8);
	BCinterp(ends[2], ends[3], interp + 8, 64, 8);
	BCindex<2>::template deindex<3>(block, interp, partition, indices);
}

/*
template<typename T, int S, int N, int...B>	constexpr T kbits_multi		= kbits<T, N, S> | kbits_multi<T, B...>;
template<typename T, int S, int N>		constexpr T kbits_multi<T,S,N>	= kbits<T, N, S>;

template<typename T> static			constexpr auto				get_mask = T::mask;// = kbits_multi<TN, B...>;
template<typename T, int...B>		static constexpr auto		get_mask<bitfield_multi<T, B...>> = kbits_multi<T, B...>;
template<typename T, int S, int N>	static constexpr auto		get_mask<bitfield<T, S, N>> = kbits<T, N, S>;

template<typename T0, typename...T> constexpr auto or_masks		= get_mask<T0> | or_masks<T...>;
template<typename T0, typename...T> constexpr auto xor_masks	= get_mask<T0> ^ xor_masks<T...>;
template<typename T>				constexpr auto or_masks<T>	= get_mask<T>;
template<typename T>				constexpr auto xor_masks<T>	= get_mask<T>;
template<typename...T> constexpr bool check_masks = or_masks<T...> == xor_masks<T...>;
*/
template<typename R, typename G, typename B> struct BC6_COL {
//	static constexpr auto mask = get_mask<R> | get_mask<G> | get_mask<B>;
//	static_assert(check_masks<R, G, B>, "ugh");
	union { R r; G g; B b; };
};


template<int Q> struct BC6_C01 : BC6_COL<bitfield<uint64, 4, Q>, bitfield<uint64, 14, Q>, bitfield<uint64, 24, Q>> {};
template<> struct BC6_C01<11> : BC6_COL<bitfield_multi<uint64, 4,10, 43,1>, bitfield_multi<uint64, 14,10, 53,1>, bitfield_multi<uint64, 24,10, 63,1>> {};
template<> struct BC6_C01<12> : BC6_COL<bitfield_multi<uint64, 4,10, 42,2>, bitfield_multi<uint64, 14,10, 52,2>, bitfield_multi<uint64, 24,10, 62,2>> {};
template<> struct BC6_C01<16> : BC6_COL<bitfield_multi<uint64, 4,10, 38,6>, bitfield_multi<uint64, 14,10, 48,6>, bitfield_multi<uint64, 24,10, 58,6>> {};

template<int Q, int R, int G,int B> struct BC6_C02 : BC6_C01<Q> {};
template<> struct BC6_C02<11,5,4,4> : BC6_COL<bitfield_multi<uint64, 4,10, 39,1>, bitfield_multi<uint64, 14,10, 48,1>, bitfield_multi<uint64, 24,10, 58,1>> {};
template<> struct BC6_C02<11,4,5,4> : BC6_COL<bitfield_multi<uint64, 4,10, 38,1>, bitfield_multi<uint64, 14,10, 49,1>, bitfield_multi<uint64, 24,10, 58,1>> {};
template<> struct BC6_C02<11,4,4,5> : BC6_COL<bitfield_multi<uint64, 4,10, 38,1>, bitfield_multi<uint64, 14,10, 48,1>, bitfield_multi<uint64, 24,10, 59,1>> {};

template<int R, int G, int B>			struct BC6_C1 : BC6_COL<bitfield<uint64, 34, R>, bitfield<uint64, 44, G>, bitfield<uint64, 54, B>> {};
template<int R, typename G, typename B> struct BC6_C2 : BC6_COL<bitfield<uint64, 64,R>, G, B> {};
template<int R, typename G, typename B> struct BC6_C3 : BC6_COL<bitfield<uint64, 70,R>, G, B> {};

template<int Q, int D, bool DELTA> struct BC6mode1 {
	union {
		BC6_C01<Q>			c0;
		BC6_C1<D, D, D>		c1;
		struct { uint64 _, ind; };
	};
//	static_assert(check_masks<BC6_C01<Q>, BC6_C1<D, D, D>>, "ugh");

	void Decode(int32x3 block[16], bool sign) const {
		int32x3	ends[2]{
			BC6extend(c0, sign),
			BC6extend(c1, DELTA || sign),
		};

		if (DELTA)
			ends[1] += ends[0];

		ends[0] = BC6unquant<Q>(ends[0], sign);
		ends[1] = BC6unquant<Q>(ends[1], sign);

		BC6common1(block, ends, ind, sign);
	}
	void Encode(const int32x3* ends, const uint8* selectors, bool sign) {
		clear(*this);

		int32x3	e0 = ends[0], e1 = ends[1];
		if (DELTA)
			e1 -= e0;

		BC6setcol(c0, e0);
		BC6setcol(c1, e1);

		ind = BCindex<1>::template get_indices<4>(selectors, 0);
	}

	static float Score(const float3* block, uint32 mask, bool sign, int32x3* ends, uint8* selectors, float best_err);
};

template<int Q, int R, int G, int B, bool DELTA, typename G2, typename B2, typename G3, typename B3> struct BC6mode2 {
	union {
		BC6_C02<Q,R,G,B>	c0;
		BC6_C1<R, G, B>		c1;
		BC6_C2<R, G2, B2>	c2;
		BC6_C3<R, G3, B3>	c3;
		struct { uint64:64, : 12, d : 5, ind : 46; };
	};
//	static_assert(check_masks<BC6_C02<Q,R,G,B>, BC6_C1<R, G, B>, BC6_C2<R, G2, B2>, BC6_C3<R, G3, B3>>, "ugh");

	void Decode(int32x3 block[16], bool sign) const {
		int32x3		ends[4] = {
			BC6extend(c0, sign),
			BC6extend(c1, DELTA || sign),
			BC6extend(c2, DELTA || sign),
			BC6extend(c3, DELTA || sign)
		};

		if (DELTA) {
			ends[1] += ends[0];
			ends[2] += ends[0];
			ends[3] += ends[0];
		}
		ends[0] = BC6unquant<Q>(ends[0], sign);
		ends[1] = BC6unquant<Q>(ends[1], sign);
		ends[2] = BC6unquant<Q>(ends[2], sign);
		ends[3] = BC6unquant<Q>(ends[3], sign);

		BC6common2(block, ends, d, ind, sign);
	}
	void Encode(const int32x3* ends, const uint8* selectors, uint8 partition, bool sign) {
		clear(*this);

		int32x3	e0 = ends[0], e1 = ends[1], e2 = ends[2], e3 = ends[3];
		if (DELTA) {
			e1 -= e0;
			e2 -= e0;
			e3 -= e0;
		}

		BC6setcol(c0, e0);
		BC6setcol(c1, e1);
		BC6setcol(c2, e2);
		BC6setcol(c3, e3);

		d	= partition;
		ind	= BCindex<2>::template get_indices<3>(selectors, partition);
	}

	static float Score(const float3* block, uint32 mask, bool sign, int32x3* ends, uint8* selectors, uint8 partition, float best_err);
};

// reused field definitions
typedef bitfield_multi<uint64, 40,4>							G2_4;
typedef bitfield_multi<uint64, 40,4, 23,1>						G2_5;
typedef bitfield_multi<uint64, 60,4>							B2_4;
typedef bitfield_multi<uint64, 60,4, 13,1>						B2_5;
typedef bitfield_multi<uint64, 60,4, 13,1, 21,1>				B2_6;
typedef bitfield_multi<uint64, 50,4>							G3_4;
typedef bitfield_multi<uint64, 50,4, 39,1>						G3_5;
typedef bitfield_multi<uint64, 11,2, 22,1, 31,1, 33,1, 32,1>	B3_6;

template<int MODE> struct BC6mode;
//										Q	R G B	delta	G2											B2											G3											B3
template<> struct BC6mode< 0> : BC6mode2<10, 5,5,5, true,	bitfield_multi<uint64, 40,4, 1,1>,			bitfield_multi<uint64, 60,4, 2,1>,			G3_5,										bitfield_multi<uint64, 49,1, 59,1, 69,1, 75,1, 3,1>			> {};
template<> struct BC6mode< 1> : BC6mode2< 7, 6,6,6, true,	bitfield_multi<uint64, 40,4, 23,1, 1,1>,	B2_6,										bitfield_multi<uint64, 50,4, 2,2>,			B3_6														> {};
template<> struct BC6mode< 2> : BC6mode2<11, 5,4,4, true,	G2_4,										B2_4,										G3_4,										bitfield_multi<uint64, 49,1, 59,1, 69,1, 75,1>				> {};
template<> struct BC6mode< 6> : BC6mode2<11, 4,5,4, true,	bitfield_multi<uint64, 40,4, 74,1>,			B2_4,										G3_5,										bitfield_multi<uint64, 68,1, 59,1, 69,1, 75,1>				> {};
template<> struct BC6mode<10> : BC6mode2<11, 4,4,5, true,	G2_4,										bitfield_multi<uint64, 60,4, 39,1>,			G3_4,										bitfield_multi<uint64, 49,1, 68,2, 75,1, 74,1>				> {};
template<> struct BC6mode<14> : BC6mode2< 9, 5,5,5, true,	G2_5,										bitfield_multi<uint64, 60,4, 13,1>,			G3_5,										bitfield_multi<uint64, 49,1, 59,1, 69,1, 75,1, 33,1>		> {};
template<> struct BC6mode<18> : BC6mode2< 8, 6,5,5, true,	G2_5,										B2_5,										bitfield_multi<uint64, 50,4, 12,1>,			bitfield_multi<uint64, 49,1, 59,1, 22,1, 32,2>				> {};
template<> struct BC6mode<22> : BC6mode2< 8, 5,6,5, true,	bitfield_multi<uint64, 40,4, 23,1, 22,1>,	B2_5,										bitfield_multi<uint64, 50,4, 39,1, 32,1>,	bitfield_multi<uint64, 12,1, 59,1, 69,1, 75,1, 33,1>		> {};
template<> struct BC6mode<26> : BC6mode2< 8, 5,5,6, true,	G2_5,										bitfield_multi<uint64, 60,4, 13,1, 22,1>,	G3_5,										bitfield_multi<uint64, 49,1, 12,1, 69,1, 75,1, 33,1, 32,1>	> {};
template<> struct BC6mode<30> : BC6mode2< 6, 6,6,6, false,	bitfield_multi<uint64, 40,4, 23,1, 20,1>,	B2_6,										bitfield_multi<uint64, 50,4, 10,1, 30,1>,	B3_6														> {};
template<> struct BC6mode< 3> : BC6mode1<10, 10,	false>	{};
template<> struct BC6mode< 7> : BC6mode1<11,  9,	true>	{};
template<> struct BC6mode<11> : BC6mode1<12,  8,	true>	{};
template<> struct BC6mode<15> : BC6mode1<16,  4,	true>	{};

void BC6::Decode(const block<HDRpixel, 2> &block, bool sign) const {
	int32x3	block2[16];
	uint64	shift1[2]	= { (b0 >> 1) | (b1 << 63), b1 >> 1 };

	switch (b0 & 0x1f) {
		case 0x00: case 0x04: case 0x08: case 0x0c: case 0x10: case 0x14: case 0x18: case 0x1c:
			((BC6mode<0>&)shift1).Decode(block2, sign);
			break;
		case 0x01: case 0x05: case 0x09: case 0x0d: case 0x11: case 0x15: case 0x19: case 0x1d:
			((BC6mode<1>&)shift1).Decode(block2, sign);
			break;
		case  2: ((BC6mode< 2>&)shift1).Decode(block2, sign); break;
		case  6: ((BC6mode< 6>&)shift1).Decode(block2, sign); break;
		case 10: ((BC6mode<10>&)shift1).Decode(block2, sign); break;
		case 14: ((BC6mode<14>&)shift1).Decode(block2, sign); break;
		case 18: ((BC6mode<18>&)shift1).Decode(block2, sign); break;
		case 22: ((BC6mode<22>&)shift1).Decode(block2, sign); break;
		case 26: ((BC6mode<26>&)shift1).Decode(block2, sign); break;
		case 30: ((BC6mode<30>&)shift1).Decode(block2, sign); break;
		case  3: ((BC6mode< 3>&)shift1).Decode(block2, sign); break;
		case  7: ((BC6mode< 7>&)shift1).Decode(block2, sign); break;
		case 11: ((BC6mode<11>&)shift1).Decode(block2, sign); break;
		case 15: ((BC6mode<15>&)shift1).Decode(block2, sign); break;
		default: //0x13, 0x17, 0x1b, and 0x1f
			clear(block2);
			break;
	}

	for (uint32 i = 0, bmask = block_mask<4, 4>(block.size<1>(), block.size<2>()); bmask; i++, bmask >>= 1) {
		if (bmask & 1)
			block[i >> 2][i & 3] = HDRpixel(concat(BC6float(block2[i], sign), one));
	}
}

//-----------------------------------------------------------------------------
//	BC6 encode
//-----------------------------------------------------------------------------

template<int B> void BC6interp(int32x3 e0, int32x3 e1, float3 *interp, bool sign) {
	for (int i = 0; i < 1 << B; i++)
		*interp++ = BC6float(BCinterp(e0, e1, 64, div_round(i * 64, bits(B))), sign);
}

static float BC6estimate(const float3* block, uint32 mask, bool sign, float best_err) {
	// use bounds as an approximation of the block's principle axis
	interval<float3>	bounds(none);
	for (uint32 m = mask, i = 0; m; m >>= 1, ++i) {
		if (m & 1)
			bounds |= block[i];
	}

	int32x3			e0 = BC6int(bounds.a, sign), e1 = BC6int(bounds.b, sign);
	float3			interp[1 << 3];
	BCindexer<3>	ix(e0, e1, bits(3));
	BC6interp<3>(e0, e1, interp, sign);

	float total_err = 0;
	for (uint32 m = mask, i = 0; m; m >>= 1, ++i) {
		if (m & 1) {
			auto	pixel = block[i];
			total_err += len2(interp[ix(BC6int(pixel, sign))] - pixel);
			if (total_err >= best_err)
				break;
		}
	}
	return total_err;
}

template<int R, int G, int B> int32x3 BC6clamp(int32x3 c, const int32x3* base) {
	static const int32x3 maxval = {bits(R - 1), bits(G - 1), bits(B - 1)};
	return base
		? *base + max(min(c - *base, maxval), ~maxval)
		: c;
}

template<int Q, int R, int G, int B, int IB> static float BC6fix_evaluate(int32x3 e0, int32x3 e1, bool sign, const int32x3 *base, bool delta, int32x3* ends, uint8* selectors, const float3* pixels, uint32 n, float best_err) {
	ends[0] = BC6clamp<R,G,B>(BC6quant<Q>(e0, sign), base);
	ends[1] = BC6clamp<R,G,B>(BC6quant<Q>(e1, sign), base ? base : delta ? ends + 0 : nullptr);

	float3			interp[1 << IB];
	BCindexer<3>	ix(e0, e1, bits(IB));
	BC6interp<IB>(BC6unquant<Q>(ends[0], sign), BC6unquant<Q>(ends[1], sign), interp, sign);

	float total_err = 0;
	while (n--) {
		auto	pixel	= *pixels++;
		int		sel		= ix(BC6int(pixel, sign));
		auto	err		= len2(interp[sel] - pixel);
		if (sel != bits(IB)) {
			auto	err1 = len2(interp[sel + 1] - pixel);
			if (err1 < err) {
				++sel;
				err = err1;
			}
		}
		*selectors++ = sel;
		total_err += err;
		if (total_err >= best_err)
			break;
	}
	return total_err;
}

template<int Q, int R, int G, int B, int IB> float BC6score_set(const float3* block, uint32 mask, uint8 anchor, bool sign, const int32x3 *base, bool delta, int32x3* ends, uint8* selectors, float best_err) {
	uint8	selectors0[2][16];
	int32x3	ends2[2][2];
	int		besti = 0;
	float3	e0, e1;

	// get mean color
	int32x3		ipixels[16];
	float3		pixels[16];
	float3*		p = pixels;
	int32x3*	ip = ipixels;

	for (uint32 m = mask; m; m >>= 1, ++block) {
		if (m & 1)
			*ip++ = BC6int(*p++ = *block, sign);
	}
	uint32	n = uint32(p - pixels);
	BCpca_ends(ipixels, n, e0, e1);

	if (!sign) {
		e0 = max(e0, zero);
		e1 = max(e1, zero);
	}
	best_err = BC6fix_evaluate<Q, R, G, B, IB>(to<int>(e0), to<int>(e1), sign, base, delta, ends2[besti], selectors0[besti], pixels, n, best_err);

	int	level = 0;

	if (level > 0) {
		// try to refine the solution using least squares by computing the optimal endpoints from the current selectors
		BCleast_squares_ends(ipixels, selectors0[0], 64, bit(IB), n, e0, e1);

		float	error = BC6fix_evaluate<Q, R, G, B, IB>(to<int>(e0), to<int>(e1), sign, base, delta, ends2[1 - besti], selectors0[1 - besti], pixels, n, best_err);
		if (error < best_err) {
			best_err = error;
			besti = 1 - besti;
		}

		if (level > 1) {
			// try varying the selectors a little, somewhat like cluster fit would
			uint8	selectors1[16], selectors2[16], selectors3[16];
			BCadjust_selectors(selectors0[besti], n, bits(IB), selectors1, selectors2, selectors3);

			//try incrementing the minimum selectors
			BCleast_squares_ends(ipixels, selectors1, 64, bit(IB), n, e0, e1);
			error = BC6fix_evaluate<Q, R, G, B, IB>(to<int>(e0), to<int>(e1), sign, base, delta, ends2[1 - besti], selectors0[1 - besti], pixels, n, best_err);
			if (error < best_err) {
				best_err = error;
				besti = 1 - besti;
			}

			//try decrementing the maximum selectors
			BCleast_squares_ends(ipixels, selectors2, 64, bit(IB), n, e0, e1);
			error = BC6fix_evaluate<Q, R, G, B, IB>(to<int>(e0), to<int>(e1), sign, base, delta, ends2[1 - besti], selectors0[1 - besti], pixels, n, best_err);
			if (error < best_err) {
				best_err = error;
				besti = 1 - besti;
			}

			//try both
			BCleast_squares_ends(ipixels, selectors3, 64, bit(IB), n, e0, e1);
			error = BC6fix_evaluate<Q, R, G, B, IB>(to<int>(e0), to<int>(e1), sign, base, delta, ends2[1 - besti], selectors0[1 - besti], pixels, n, best_err);
			if (error < best_err) {
				best_err = error;
				besti = 1 - besti;
			}
		}
	}

	// fixup selectors
	uint8	*sd = selectors, *ss = selectors0[besti];
	for (uint32 m = mask; m; m >>= 1, ++sd) {
		if (m & 1)
			*sd = *ss++;
	}

	bool	x = BCanchor(selectors, mask, anchor, IB);
	ends[0] = ends2[besti][x];
	ends[1] = ends2[besti][!x];

	return best_err;
}


template<int Q, int D, bool DELTA> float BC6mode1<Q, D, DELTA>::Score(const float3* block, uint32 mask, bool sign, int32x3* ends, uint8* selectors, float best_err) {
	return BC6score_set<Q, D, D, D, 4>(block, mask, 0, sign, nullptr, DELTA, ends, selectors, best_err);
}
	
template<int Q, int R, int G, int B, bool DELTA, typename G2, typename B2, typename G3, typename B3> float BC6mode2<Q,R,G,B,DELTA,G2,B2,G3,B3>::Score(const float3* block, uint32 mask, bool sign, int32x3* ends, uint8* selectors, uint8 partition, float best_err) {
	auto	error	= BC6score_set<Q, R, G, B, 3>(block, mask & ~BCmask2[partition], 0,						sign, nullptr,  DELTA, ends + 0, selectors, best_err);
	return	error	+ BC6score_set<Q, R, G, B, 3>(block, mask &  BCmask2[partition], BCanchor2[partition],	sign, ends + 0, DELTA, ends + 2, selectors, best_err - error);
}

float BC6score(int mode, const float3* block, uint32 mask, bool sign, int32x3 *ends, uint8 *selectors, uint8 partition, float best_err) {
	switch (mode) {
		default:
		case  0: return BC6mode< 0>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case  1: return BC6mode< 1>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case  2: return BC6mode< 2>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case  6: return BC6mode< 6>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case 10: return BC6mode<10>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case 14: return BC6mode<14>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case 18: return BC6mode<18>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case 22: return BC6mode<22>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case 26: return BC6mode<26>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case 30: return BC6mode<30>::Score(block, mask, sign, ends, selectors, partition,	best_err);
		case  3: return BC6mode< 3>::Score(block, mask, sign, ends, selectors,				best_err);
		case  7: return BC6mode< 7>::Score(block, mask, sign, ends, selectors,				best_err);
		case 11: return BC6mode<11>::Score(block, mask, sign, ends, selectors,				best_err);
		case 15: return BC6mode<15>::Score(block, mask, sign, ends, selectors,				best_err);
	}
}

void BC6::Encode(const block<HDRpixel, 2> &block, bool sign) {
	float3	block2[16];
	uint32	bmask	= block_mask<4, 4>(block.size<1>(), block.size<2>());

	for (uint32 i = 0, m = bmask; m; i++, m >>= 1) {
		if (m & 1)
			block2[i] = max((float3)block[i >> 2][i & 3].rgb, 1.f/ 256);
	}

	// find best partition (all partitioned modes use same criteria)
	uint8	best_part	= 0;
	float	best_err	= maximum;
	for (auto part : slice(BCpartition_order2, 0, 32)) {
		auto error	=	BC6estimate(block2, bmask &  BCmask2[part], sign, best_err);
		error		+=	BC6estimate(block2, bmask & ~BCmask2[part], sign, best_err - error);
		if (error < best_err) {
			best_err	= error;
			best_part	= part;
		}
	}
#if 0
	// get ends for each partition
	int32x3		ipixels[16];
	int32x3*	ip0	= ipixels, *ip1 = ipixels + 16;
	for (uint32 i = 0, m = bmask, m2 =BCmask2[best_part]; m; m >>= 1, m2 >>= 1, ++i) {
		if (m & 1)
			*(m2 & 1 ? ip0++ : --ip1) = BC6int(block2[i], sign);
	}
	float3		e0, e1, e2, e3;
	BCpca_ends(ipixels, uint32(ip0 - ipixels),  e0, e1);
	BCpca_ends(ip1, uint32(ipixels + 16 - ip1), e2, e3);
	if (!sign) {
		e0 = max(e0, zero);
		e1 = max(e1, zero);
		e2 = max(e2, zero);
		e3 = max(e3, zero);
	}
#endif

	// find best mode
	int32x3	ends[2][6];
	uint8	selectors[2][16];
	int		besti = 1;

	uint8	mode_order[] = {
		 0,
		 1,
		 2,
		 6,
		10,
		14,
		18,
		22,
		26,
		30,
		 3,
		 7,
		11,
		15,
		0xff,
	};
	int		best_mode	= 0;
	best_err	= maximum;

	for (uint8* pmode = mode_order; *pmode != 0xff; ++pmode) {
		auto	error = BC6score(*pmode, block2, bmask, sign, ends[besti], selectors[besti], best_part, best_err);
		if (error < best_err) {
			best_err	= error;
			best_mode	= *pmode;
			besti = 1 - besti;
		}
	}

	auto	best_sel	= selectors[1 - besti];
	auto	best_ends	= ends[1 - besti];
	uint64	shift1[2];
	switch (best_mode) {
		case  0: ((BC6mode< 0>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case  1: ((BC6mode< 1>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case  2: ((BC6mode< 2>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case  6: ((BC6mode< 6>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case 10: ((BC6mode<10>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case 14: ((BC6mode<14>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case 18: ((BC6mode<18>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case 22: ((BC6mode<22>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case 26: ((BC6mode<26>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case 30: ((BC6mode<30>&)shift1).Encode(best_ends, best_sel, best_part,	sign); break;
		case  3: ((BC6mode< 3>&)shift1).Encode(best_ends, best_sel,				sign); break;
		case  7: ((BC6mode< 7>&)shift1).Encode(best_ends, best_sel,				sign); break;
		case 11: ((BC6mode<11>&)shift1).Encode(best_ends, best_sel,				sign); break;
		case 15: ((BC6mode<15>&)shift1).Encode(best_ends, best_sel,				sign); break;
	}
	b0	= (shift1[0] << 1) | best_mode;
	b1	= (shift1[0] >> 63) | (shift1[1] << 1);

#if 0
	// to verify
	HDRpixel	block3[16];
	Decode(make_block(block3, 4, 4), sign);
	for (uint32 i = 0, m = bmask; m; i++, m >>= 1) {
		if (m & 1) {
			float	e = dist2(block2[i], (float3)block3[i].rgb);
			ISO_ASSERT(e < 3.f);
		}
	}
#endif
}

//-----------------------------------------------------------------------------
//	BC7
//-----------------------------------------------------------------------------
/*
NS:		Number of subsets
PB:		Partition bits (or rotation(+index) bits)
CB:		Color bits
AB:		Alpha bits
EPB:	Endpoint P-bits per set:	0, 1 or 2
IB:		Index bits per element
IB2:	Secondary index bits per element

MODE	NS PB CB AB EPB IB IB2	MODE+1+PB	CB*NS*2	AB*NS*2	ends	+MODE+1+PB
  0,	3  4  4  0  2   3		5			24		0		72		77
  1,	2  6  6  0  1   3		8			24		0		72		80
  2,	3  6  5  0  0   2		9			30		0		90		99
  3,	2  6  7  0  2   2		10			28		0		84		94
  4,	1  3  5  6      2	3	8			10		12		42		50
  5,	1  2  7  8      2	2	8			14		16		58		66
  6,	1  0  7  7  2   4		7			14		14		56		63
  7,	2  6  5  5  2   2		14			20		20		80		94

rotation:	0: no change, 1: swap(a,r), 2: swap(a,g), 3: swap(a,b)
 */

template<int B> inline uint8 BC7decode_comp(uint64 b)		{ return extend_bits<B, 8>(b & bits(B)); }// << (8 - B);
template<>		inline uint8 BC7decode_comp<0>(uint64 b)	{ return 0xff; }
template<int B, int X> inline uint64 BC7decode_cols(uint64 b, uint8 ends[X]) {
	for (int i = 0; i < X; i++, b >>= B)
		ends[i]	= BC7decode_comp<B>(b);
	return b;
}

template<int B> inline uint8 BC7encode_comp(uint8 c)		{ return c >> (8 - B); }
template<>		inline uint8 BC7encode_comp<0>(uint8 c)		{ return 0; }
template<int B, int X> inline uint64 BC7encode_cols(uint64 b, uint8 ends[X]) {
	for (int i = 0; i < X; i++)
		b = BCput<B>(b, BC7encode_comp<B>(ends[i]));
	return b;
}

template<typename T> T BC7rotate(T x, uint8 rotation) {
	switch (rotation) {
		default: return x;
		case 1: return x.wyzx;
		case 2: return x.xwzy;
		case 3: return x.xywz;
	}
}

template<int NS> struct BC7index1;

template<> struct BC7index1<1> {
	template<int B> static void	deindex(uint8x4 block[16], const uint8x4 *ends, uint32 partition, uint64 indices) {
		uint8x4	interp[1<<B];
		BCinterp(ends[0], ends[1], interp, 64, 1 << B);
		BCindex<1>::deindex<B>(block, interp, partition, indices);
	}
};

template<> struct BC7index1<2> {
	template<int B> static void	deindex(uint8x4 block[16], const uint8x4 *ends, uint32 partition, uint64 indices) {
		uint8x4	interp[2 << B];
		BCinterp(ends[0], ends[1], interp + (0 << B), 64, 1 << B);
		BCinterp(ends[2], ends[3], interp + (1 << B), 64, 1 << B);
		BCindex<2>::deindex<B>(block, interp, partition, indices);
	}
};

template<> struct BC7index1<3> {
	template<int B> static void	deindex(uint8x4 block[16], const uint8x4 *ends, uint32 partition, uint64 indices) {
		uint8x4	interp[3 << B];
		BCinterp(ends[0], ends[1], interp + (0 << B), 64, 1 << B);
		BCinterp(ends[2], ends[3], interp + (1 << B), 64, 1 << B);
		BCinterp(ends[4], ends[5], interp + (2 << B), 64, 1 << B);
		BCindex<3>::deindex<B>(block, interp, partition, indices);
	}
};

template<int IV, int IS> void BC7deindex2(uint8x3 v[16], uint8 s[16], uint8x4 e0, uint8x4 e1, uint64 vindices, uint64 sindices) {
	uint8x3	vinterp[1 << IV];
	BCinterp(e0.xyz, e1.xyz, vinterp, 64, 1 << IV);
	BCindex<1>::template deindex<IV>(v, vinterp, 0, vindices);

	uint8	sinterp[1 << IS];
	BCinterp(e0.w, e1.w, sinterp, 64, 1 << IS);
	BCindex<1>::template deindex<IS>(s, sinterp, 0, sindices);
}

template<int MODE, int NS, int PB, int CB, int AB, int EPB, int IB> struct BC7mode1 : BC<7> {
	void Decode(uint8x4 block[16]) const {
		uint64	b			= b0 >> (MODE + 1);
		uint32	partition	= b & bits(PB);
		b >>= PB;

		uint8	ends0[4][NS * 2];
		b = BC7decode_cols<CB, NS * 2>(b, ends0[0]);
		b |= b1 << (64 - (MODE + 1 + PB + CB * NS * 2));
		b = BC7decode_cols<CB, NS * 2>(b, ends0[1]);
		b = BC7decode_cols<CB, NS * 2>(b, ends0[2]);
		b = BC7decode_cols<AB, NS * 2>(b, ends0[3]);

		uint8x4	ends[NS * 2];
		for (int i = 0; i < NS * 2; i++)
			ends[i] = { ends0[0][i], ends0[1][i], ends0[2][i], ends0[3][i] };

		if (EPB) {
			auto	e = ends;
			for (int i = 0; i < NS; i++) {
				e->xyz	|= shift_bits<7-CB>(b & 1);
				e->w	|= shift_bits<7-AB>(b & 1);
				++e;

				if (EPB == 2)
					b <<= 1;

				e->xyz	|= shift_bits<7-CB>(b & 1);
				e->w	|= shift_bits<7-AB>(b & 1);
				++e;

				b <<= 1;
			}
		}

		BC7index1<NS>::template deindex<IB>(block, ends, partition, b1 >> (64 - (IB * 16 - NS)));
	}

	void Encode(const uint8x4 *block, uint32 mask, const uint8x4 *ends, const uint8 *selectors, uint8 partition) {
		uint8	ends0[4][NS * 2];
		for (int i = 0; i < NS * 2; i++) {
			ends0[0][i] = ends[i].x;
			ends0[1][i] = ends[i].y;
			ends0[2][i] = ends[i].z;
			ends0[3][i] = ends[i].w;
		}

		uint64	b0 = BCput<PB>(bit64(63), partition);
		b0 = BC7encode_cols<CB, NS * 2>(b0, ends0[0]);

		uint64	b1 = 0;
		b1 = BC7encode_cols<CB, NS * 2>(b1, ends0[1]);
		b1 = BC7encode_cols<CB, NS * 2>(b1, ends0[2]);
		b1 = BC7encode_cols<AB, NS * 2>(b1, ends0[3]);

		if (EPB) {
			for (int i = 0, step = EPB == 2 ? 1 : 2; i < NS * 2; i += step)
				b1 = BCput<1>(b1, ends0[0][i] & bit(7 - CB));
		}
		uint64	indices = BCindex<NS>::template get_indices<IB>(selectors, partition);
		this->b0 = BCput<64 - (MODE + 1 + PB + CB * NS * 2)>(b0, b1 >> (64 - (CB * NS * 4 + AB * NS * 2 + EPB * NS)));
		this->b1 = BCput<IB * 16 - NS>(b1, indices);
	}
};

template<int MODE, int CB, int AB, int IB2> struct BC7mode2 : BC<7> {
	void Decode(uint8x4 block[16]) const {
		static const int IB = 2, RB = 2 + IB2 - IB;

		uint64	b = b0 >> (MODE + 1);
		uint32	partition = b & bits(RB);
		b >>= RB;

		uint8	ends[4][2];
		b = BC7decode_cols<CB, 2>(b, ends[0]);
		b = BC7decode_cols<CB, 2>(b, ends[1]);
		b = BC7decode_cols<CB, 2>(b, ends[2]);

		b |= b1 << (64 - (MODE + 1 + RB + CB * 6));
		b = BC7decode_cols<AB, 2>(b, ends[3]);

		uint64	indices0 = b | (b1 << (IB2 * 16 - 1));
		uint64	indices1 = b1 >> (64 - (IB2 * 16 - 1));

		uint8x3	v[16];
		uint8	s[16];
		uint8x4	e0 = { ends[0][0], ends[1][0], ends[2][0], ends[3][0] },
			e1 = { ends[0][1], ends[1][1], ends[2][1], ends[3][1] };

		if (!(partition & 4))
			BC7deindex2<IB, IB2>(v, s, e0, e1, indices0, indices1);
		else
			BC7deindex2<IB2, IB>(v, s, e0, e1, indices1, indices0);

		int		rotation = partition & 3;
		for (int i = 0; i < 16; i++)
			block[i] = BC7rotate(concat(v[i], s[i]), rotation);
	}
	void Encode(const uint8x4 *block, uint32 mask, const uint8x4 *ends, const uint8 *selectors, uint8 partition) {
		static const int IB = 2, RB = 2 + IB2 - IB;

		uint8	ends0[4][2] = {
			{ends[0].x, ends[1].x},
			{ends[0].y, ends[1].y},
			{ends[0].z, ends[1].z},
			{ends[0].w, ends[1].w}
		};

		uint64	b0	= BCput<RB>(bit64(63), partition);
		b0 = BC7encode_cols<CB, 2>(b0, ends0[0]);
		b0 = BC7encode_cols<CB, 2>(b0, ends0[1]);
		b0 = BC7encode_cols<CB, 2>(b0, ends0[2]);

		uint64	b1	= BC7encode_cols<AB, 2>(0, ends0[3]);

		uint8x3	v[16];
		uint8	s[16];

		int		rotation = partition & 3;
		for (int i = 0; i < 16; i++) {
			auto	t = BC7rotate(block[i], rotation);
			v[i] = t.xyz;
			s[i] = t.w;
		}

		uint64	indices0 = 0, indices1 = 0;
		for (int i = 0; i < 16; i++) {
			indices0 |= uint64(selectors[i] & bits(IB))	<< (IB * i);
			indices1 |= uint64(selectors[i] >> IB)		<< (IB2 * i);
		}
		indices0 = remove0(indices0, IB - 1);
		indices1 = remove0(indices1, IB2 - 1);

		b1 = BCput<IB * 16 - 1>(b1, indices0);

		this->b0 = BCput<64 - (MODE + 1 + RB + CB * 6)>(b0, b1 >> (64 - (AB * 2 + IB * 16 - 1)));
		this->b1 = BCput<IB2 * 16 - 1>(b1, indices1);
	}
};

template<int MODE> struct BC7mode;

template<> struct BC7mode<0> : BC7mode1<0,	3, 4, 4, 0, 2,  3> {};
template<> struct BC7mode<1> : BC7mode1<1,	2, 6, 6, 0, 1,  3> {};
template<> struct BC7mode<2> : BC7mode1<2,	3, 6, 5, 0, 0,  2> {};
template<> struct BC7mode<3> : BC7mode1<3,	2, 6, 7, 0, 2,  2> {};
template<> struct BC7mode<4> : BC7mode2<4,	      5, 6,     3> {};
template<> struct BC7mode<5> : BC7mode2<5,	      7, 8,     2> {};
template<> struct BC7mode<6> : BC7mode1<6,	1, 0, 7, 7, 2,  4> {};
template<> struct BC7mode<7> : BC7mode1<7,	2, 6, 5, 5, 2,  2> {};


void BC<7>::Decode(const block<ISO_rgba, 2> &block) const {
	uint8x4	block2[16];
	uint8	b	= b0;		//	  MODE	NS PB CB AB EPB IB(2)
	if (b & 0x01)		((BC7mode<0>*)this)->Decode(block2);
	else if (b & 0x02)	((BC7mode<1>*)this)->Decode(block2);
	else if (b & 0x04)	((BC7mode<2>*)this)->Decode(block2);
	else if (b & 0x08)	((BC7mode<3>*)this)->Decode(block2);
	else if (b & 0x10)	((BC7mode<4>*)this)->Decode(block2);
	else if (b & 0x20)	((BC7mode<5>*)this)->Decode(block2);
	else if (b & 0x40)	((BC7mode<6>*)this)->Decode(block2);
	else				((BC7mode<7>*)this)->Decode(block2);

	uint32	bmask	= block_mask<4, 4>(block.size<1>(), block.size<2>());
	for (int i = 0; bmask; i++, bmask >>= 1) {
		if (bmask & 1)
			block[i >> 2][i & 3] = (ISO_rgba&)block2[i];
	}
}

//-----------------------------------------------------------------------------
//	BC7 encode
//-----------------------------------------------------------------------------

template<int B> static uint64 BC7estimate(const uint8x4* block, uint32 mask, const int32x4 weights, uint32 best_err) {
	// use bounds as an approximation of the block's principle axis
	interval<uint8x4>	bounds(none);
	for (uint32 m = mask, i = 0; m; m >>= 1, ++i) {
		if (m & 1)
			bounds |= block[i];
	}

	auto	e0 = to<uint16>(bounds.a);
	auto	e1 = to<uint16>(bounds.b);

	uint16x4		interp[1 << B];
	BCinterp(e0, e1, interp, 64, 1 << B);
	BCindexer<4>	ix(e0, e1, bits(B));

	uint64 total_err = 0;
	for (uint32 m = mask, i = 0; m; m >>= 1, ++i) {
		if (m & 1) {
			auto	pixel = block[i];
			total_err += weighted_distance(interp[ix(pixel)], pixel, weights);
			if (total_err >= best_err)
				break;
		}
	}
	return total_err;
}

template<int CB, int IB> uint32 BC7fix_evaluate(float4 e0, float4 e1, uint8x4 *ends, uint8* selectors, const uint8x4* pixels, uint32 n, int32x4 weights, int EPB, uint32 best_err) {
	auto		i0		= to<uint16>(e0);
	auto		i1		= to<uint16>(e1);
	uint16x4	e0a, e1a;

	if (EPB == 0) {
		e0a = extend_bits<CB, 8>(extend_bits<8, CB>(i0));
		e1a = extend_bits<CB, 8>(extend_bits<8, CB>(i1));

	} else {
		auto	t0		= extend_bits<8, CB>(i0) * 2;
		auto	t1		= extend_bits<8, CB>(i1) * 2;

		e0a				= extend_bits<CB + 1, 8>(t0);
		e1a				= extend_bits<CB + 1, 8>(t1);
		auto	err0a	= dist2(e0a, i0);
		auto	err1a	= dist2(e1a, i1);

		auto	e0b		= extend_bits<CB + 1, 8>(t0 + 1);
		auto	e1b		= extend_bits<CB + 1, 8>(t1 + 1);
		auto	err0b	= dist2(e0b, i0);
		auto	err1b	= dist2(e1b, i1);

		if (EPB == 1) {
			// Endpoints share pbits
			if (err0b + err1b < err0a + err1a) {
				e0a = e0b;
				e1a = e1b;
			}
		} else {
			if (err0b < err0a)
				e0a = e0b;
			if (err1b < err1a)
				e1a = e1b;
		}
	}

	ends[0] = to<uint8>(e0a);
	ends[1] = to<uint8>(e1a);

	return BCevaluate(ends[0], ends[1], 64, 1 << IB, selectors, pixels, n, weights, best_err);
}

template<int CB, int AB, int EPB, int IB> uint32 BC7score_set(const uint8x4* block, uint32 mask, uint8 anchor, uint8x4* ends, uint8 *selectors, uint32 best_err) {
	uint8x4	pixels[16];
	uint8	selectors0[2][16];
	uint8x4	ends2[2][2];
	int		besti	= 0;
	float4	e0, e1;

	uint8x4	*p	= pixels;
	for (uint32 m = mask; m; m >>= 1, ++block) {
		if (m & 1)
			*p++ = *block;
	}
	uint32	n = uint32(p - pixels);

	BCpca_ends(pixels, n, e0, e1);
	e0 = clamp(e0, 0, 255);
	e1 = clamp(e1, 0, 255);

	int32x4	weights = { 1,1,1,!!AB };
	best_err = BC7fix_evaluate<CB, IB>(e0, e1, ends2[besti], selectors0[besti], pixels, n, weights, EPB, best_err);

	int	level = 1;

	if (level > 0) {
		// try to refine the solution using least squares by computing the optimal endpoints from the current selectors
		BCleast_squares_ends(pixels, selectors0[0], 64, bit(IB), n, e0, e1);

		uint32	error = BC7fix_evaluate<CB, IB>(e0, e1, ends2[1 - besti], selectors0[1 - besti], pixels, n, weights, EPB, best_err);
		if (error < best_err) {
			best_err = error;
			besti = 1 - besti;
		}

		if (level > 1) {
			// try varying the selectors a little, somewhat like cluster fit would
			uint8	selectors1[16], selectors2[16], selectors3[16];
			BCadjust_selectors(selectors0[besti], n, bits(IB), selectors1, selectors2, selectors3);

			//try incrementing the minimum selectors
			BCleast_squares_ends(pixels, selectors1, 64, bit(IB), n, e0, e1);
			error = BC7fix_evaluate<CB, IB>(e0, e1, ends2[1 - besti], selectors0[1 - besti], pixels, n, weights, EPB, best_err);
			if (error < best_err) {
				best_err = error;
				besti = 1 - besti;
			}

			//try decrementing the maximum selectors
			BCleast_squares_ends(pixels, selectors2, 64, bit(IB), n, e0, e1);
			error = BC7fix_evaluate<CB, IB>(e0, e1, ends2[1 - besti], selectors0[1 - besti], pixels, n, weights, EPB, best_err);
			if (error < best_err) {
				best_err = error;
				besti = 1 - besti;
			}

			//try both
			BCleast_squares_ends(pixels, selectors3, 64, bit(IB), n, e0, e1);
			error = BC7fix_evaluate<CB, IB>(e0, e1, ends2[1 - besti], selectors0[1 - besti], pixels, n, weights, EPB, best_err);
			if (error < best_err) {
				best_err = error;
				besti = 1 - besti;
			}
		}
	}

	// fixup selectors
	uint8	*sd = selectors, *ss = selectors0[besti];
	for (uint32 m = mask; m; m >>= 1, ++sd) {
		if (m & 1)
			*sd = *ss++;
	}
	
	bool	x = BCanchor(selectors, mask, anchor, IB);
	ends[0] = ends2[besti][x];
	ends[1] = ends2[besti][!x];

	return best_err;
}

template<int MODE, int PB, int CB, int AB, int EPB, int IB> uint32 BC7score(BC7mode1<MODE, 1, PB, CB, AB, EPB, IB>*, const uint8x4* block, uint32 mask, uint8x4* ends, uint8* selectors, uint8& partition, uint32 best_err) {
	return BC7score_set<CB, AB, EPB, IB>(block, mask, 0, ends, selectors, best_err);
}

template<int MODE, int PB, int CB, int AB, int EPB, int IB> uint32 BC7score(BC7mode1<MODE, 2, PB, CB, AB, EPB, IB>*, const uint8x4* block, uint32 mask, uint8x4* ends, uint8* selectors, uint8& partition, uint32 best_err) {
	uint32	best_part		= ~0;
	uint32	best_part_err	= best_err;
	int32x4	weights			= { 1, 1, 1, AB != 0 };

	for (auto part : BCpartition_order2) {
		if (part > bits(PB))
			continue;

		auto	error = BC7estimate<IB>(block, mask &  BCmask2[part], weights, best_part_err);
		error +=		BC7estimate<IB>(block, mask & ~BCmask2[part], weights, best_part_err - error);
		if (error < best_part_err) {
			best_part_err	= error;
			best_part		= part;
			if (best_part_err == 0)
				break;
		}

		// If the checkerboard pattern doesn't get the highest ranking vs the previous (lower frequency) patterns,
		// then just stop now because statistically the subsequent patterns won't do well either
		if (part == 34 && best_part != 34)
			break;
	}

	if (!~best_part)
		return ~0;

	partition = best_part;
	auto	error	= BC7score_set<CB, AB, EPB, IB>(block, mask & ~BCmask2[best_part], 0,						ends + 0, selectors, best_err);
	return	error	+ BC7score_set<CB, AB, EPB, IB>(block, mask &  BCmask2[best_part], BCanchor2[best_part],	ends + 2, selectors, best_err - error);
}

template<int MODE, int PB, int CB, int EPB, int IB> uint32 BC7score(BC7mode1<MODE, 3, PB, CB, 0, EPB, IB>*, const uint8x4* block, uint32 mask, uint8x4* ends, uint8* selectors, uint8& partition, uint32 best_err) {
	uint32	best_part		= ~0;
	uint32	best_part_err	= best_err;
	int32x4	weights			= { 1, 1, 1, 0 };

	for (uint32 part = 0; part < (1 << PB); part++) {
		auto	error = BC7estimate<IB>(block, mask & ~BCmask3a[part] & ~BCmask3b[part], weights, best_part_err);
		error +=		BC7estimate<IB>(block, mask &  BCmask3a[part] & ~BCmask3b[part], weights, best_part_err - error);
		error +=		BC7estimate<IB>(block, mask &  BCmask3b[part], weights, best_part_err - error);
		if (error < best_part_err) {
			best_part_err = error;
			best_part = part;
			if (best_part_err == 0)
				break;
		}
	}

	if (!~best_part)
		return ~0;

	partition = best_part;
	auto	error	=	BC7score_set<CB, 0, EPB, IB>(block, mask & ~BCmask3a[best_part] & ~BCmask3b[best_part],	0,						ends + 0, selectors, best_err);
	error			+=	BC7score_set<CB, 0, EPB, IB>(block, mask &  BCmask3a[best_part] & ~BCmask3b[best_part],	BCanchor3a[best_part],	ends + 2, selectors, best_err - error);
	return error	+	BC7score_set<CB, 0, EPB, IB>(block, mask &  BCmask3b[best_part],						BCanchor3b[best_part],	ends + 4, selectors, best_err - error);
}

template<int MODE, int CB, int AB, int IB2> uint32 BC7score(BC7mode2<MODE, CB, AB, IB2>*, const uint8x4* block, uint32 mask, uint8x4* ends, uint8* selectors, uint8& partition, uint32 best_err) {
	static const int IB = 2;

	uint8x4	pixels[16];
	uint8	scalars[16];
	uint8x4	ends2[2][2];
	uint8	selectors0[16], selectors1[16];

	// get mean color
	float4	mean = zero;
	uint8x4	*p = pixels;
	for (uint32 m = mask; m; m >>= 1, ++block) {
		if (m & 1)
			mean += to<float>(*p++ = *block);
	}
	uint32	n = uint32(p - pixels);
	mean /= n;

	uint32	best_part	= ~0;
	int		besti		= 0;
	for (int rot = 0; rot < 4; ++rot) {
		int32x4	weights = { rot != 1, rot != 2, rot != 3, rot != 0 };
		auto	axis	= BCaxis(pixels, n, mean, to<float>(weights));
		uint32	error;

		interval<float>	d(none);
		interval<uint8>	a(none);
		for (int i = 0; i < n; ++i) {
			auto	p = pixels[i];
			d |= dot(to<float>(p) - mean, axis);
			a |= scalars[i] = p[(rot + 3) & 3];
		}

		// if have anchor pixel make sure it'll go to lowest half
		if (mask & 1) {
			if (dot(to<float>(pixels[0]) - mean, axis) * 2 > d.a + d.b)
				swap(d.a, d.b);

			if (scalars[0] * 2 > a.a + a.b)
				swap(a.a, a.b);
		}

		float4	v0	= clamp(axis * d.a + mean, 0, 255);
		float4	v1	= clamp(axis * d.b + mean, 0, 255);
		uint8	s0	= extend_bits<AB, 8>(extend_bits<8, AB>(a.a));
		uint8	s1	= extend_bits<AB, 8>(extend_bits<8, AB>(a.b));

		error =		BC7fix_evaluate<CB, IB>(v0, v1, ends2[1 - besti], selectors0, pixels, n, weights, 0, best_err);
		error +=	BCevaluate(s0, s1, 64, 1 << IB2, selectors1, scalars, n, one, best_err - error);
		if (error < best_err) {
			besti		= 1 - besti;
			best_err	= error;
			best_part	= rot;
			for (int i = 0; i < 16; i++)
				selectors[i] = selectors0[i] | (selectors1[i] << IB);
		}

		if (IB2 != IB) {
			error =		BC7fix_evaluate<CB, IB2>(v0, v1, ends2[1 - besti], selectors1, pixels, n, weights, 0, best_err);
			error +=	BCevaluate(s0, s1, 64, 1 << IB, selectors0, scalars, n, one, best_err - error);
			if (error < best_err) {
				besti		= 1 - besti;
				best_err	= error;
				best_part	= rot + 4;
				for (int i = 0; i < 16; i++)
					selectors[i] = selectors0[i] | (selectors1[i] << IB);
			}
		}
	}

	if (!~best_part)
		return ~0;

	ends[0] = BC7rotate(ends2[besti][0], best_part & 3);
	ends[1] = BC7rotate(ends2[besti][1], best_part & 3);
	partition = best_part;

	return best_err;
}

uint32 BC7score(int mode, const uint8x4* block, uint32 mask, uint8x4 *ends, uint8 *selectors, uint8 &partition, uint32 best_err) {
	switch (mode) {
		default:
		case 0: return BC7score((BC7mode<0>*)0, block, mask, ends, selectors, partition, best_err);
		case 1: return BC7score((BC7mode<1>*)0, block, mask, ends, selectors, partition, best_err);
		case 2: return BC7score((BC7mode<2>*)0, block, mask, ends, selectors, partition, best_err);
		case 3: return BC7score((BC7mode<3>*)0, block, mask, ends, selectors, partition, best_err);
		case 4: return BC7score((BC7mode<4>*)0, block, mask, ends, selectors, partition, best_err);
		case 5: return BC7score((BC7mode<5>*)0, block, mask, ends, selectors, partition, best_err);
		case 6: return BC7score((BC7mode<6>*)0, block, mask, ends, selectors, partition, best_err);
		case 7: return BC7score((BC7mode<7>*)0, block, mask, ends, selectors, partition, best_err);
	}
}

void BC<7>::Encode(const block<ISO_rgba, 2> &block) {
	uint8x4 block2[16];
	uint32	bmask	= block_mask<4, 4>(block.size<1>(), block.size<2>());
	uint8x4	mincol	= 255, maxcol = 0;

	for (uint32 i = 0, m = bmask; m; i++, m >>= 1) {
		if (m & 1) {
			auto	col = (const uint8x4&)block[i >> 2][i & 3];
			mincol = min(mincol, col);
			maxcol = max(maxcol, col);
			block2[i] = col;
		}
	}

	uint8 mode_order[8] = { 1, 0xff };
#if 1
	if (mincol.w == 0xff) {		// no alpha	0,1,2,3
		mode_order[0] = 0;
		mode_order[1] = 1;
		mode_order[2] = 2;
		mode_order[3] = 3;
		mode_order[4] = 0xff;
	} else {					// alpha	4,5,6,7
		mode_order[0] = 4;
		mode_order[1] = 5;
		mode_order[2] = 6;
		mode_order[3] = 7;
		mode_order[4] = 0xff;
	}
#endif

	uint8	partition[2];
	uint8x4	ends[2][6];
	uint8	selectors[2][16];
	int		besti		= 1;
	int		best_mode	= 0;
	auto	best_err	= ~0;

	for (uint8* pmode = mode_order; *pmode < 8; ++pmode) {
		auto	error = BC7score(*pmode, block2, bmask, ends[besti], selectors[besti], partition[besti], best_err);
		if (error < best_err) {
			best_err	= error;
			best_mode	= *pmode;
			besti		= 1 - besti;
		}
	}

	auto	best_sel	= selectors[1 - besti];
	auto	best_ends	= ends[1 - besti];
	auto	best_part	= partition[1 - besti];

	switch (best_mode) {
		default:
		case 0: ((BC7mode<0>*)this)->Encode(block2, bmask, best_ends, best_sel, best_part); break;
		case 1: ((BC7mode<1>*)this)->Encode(block2, bmask, best_ends, best_sel, best_part); break;
		case 2: ((BC7mode<2>*)this)->Encode(block2, bmask, best_ends, best_sel, best_part); break;
		case 3: ((BC7mode<3>*)this)->Encode(block2, bmask, best_ends, best_sel, best_part); break;
		case 4: ((BC7mode<4>*)this)->Encode(block2, bmask, best_ends, best_sel, best_part); break;
		case 5: ((BC7mode<5>*)this)->Encode(block2, bmask, best_ends, best_sel, best_part); break;
		case 6: ((BC7mode<6>*)this)->Encode(block2, bmask, best_ends, best_sel, best_part); break;
		case 7: ((BC7mode<7>*)this)->Encode(block2, bmask, best_ends, best_sel, best_part); break;
	}
#if 1
	// to verify
	ISO_rgba	block3[16];
	Decode(make_block(block3, 4, 4));
#endif
}
