#ifndef COLOUR_H
#define COLOUR_H

#include "vector.h"

namespace iso {

//-----------------------------------------------------------------------------
//	consts
//-----------------------------------------------------------------------------

#ifdef PLAT_CLANG
template<typename E, int N> using const_vec = vec<E, N>;

template<typename E, int N> struct const_vec2 {
	vec<E, N> v;

	template<size_t...I> constexpr const_vec2(const E *p, index_list<I...>) : v{p[I]...} {}
	constexpr const_vec2(initializer_list<E> p): const_vec2(p.begin(), meta::make_index_list<N>()) {}

	constexpr operator vec<E, N>()	const { return v; }
	constexpr auto	get()			const { return v; }
};

template<typename E, int N, int M> struct const_mat : mat<E, N, M> {
	template<size_t...I> constexpr const_mat(const E (*p)[N], index_list<I...>) : mat<E, N, M>(const_vec2<E,N>(p[I], meta::make_index_list<N>())...) {}
	constexpr const_mat(initializer_list<E[N]> p): const_mat(p.begin(), meta::make_index_list<M>()) {}
	template<typename V> friend auto operator*(const V &v, const const_mat &c) { return v * (const mat<E, N, M>&)c; }
};

#else

template<typename E, int N, int M> struct const_mat {
	E	m[M][N];
	operator mat<E, N, M>() const { return load<mat<E, N, M>>(m); }
	auto	get()			const { return load<mat<E, N, M>>(m); }
	template<typename V> friend auto operator*(const V &v, const const_mat &c) { return v * c.get(); }
};
template<typename E, int N> struct const_vec {
	E v[N];
	operator vec<E, N>()	const { return load<vec<E,N>>(v); }
	auto	get()			const { return load<vec<E,N>>(v); }
};
#endif

typedef const_mat<float,3, 3> float3x3c;

//-----------------------------------------------------------------------------
// srgb
//-----------------------------------------------------------------------------

template<typename T> inline auto _srgb_to_linear(T r) { return select(r <= 0.04045f, r / 12.92f, select(r <= 1, pow((r + 0.055f) / 1.055f, 2.4f), r)); }
template<typename T> inline auto _linear_to_srgb(T r) { return select(r <= 0.0031308f, r * 12.92f, select(r <= 1, 1.055f * pow(r, (1.0f / 2.4f)) - 0.055f, r)); }

template<typename T>	auto srgb_to_linear(const T &r, enable_if_t<is_vec<T>>* =0)	{ return _srgb_to_linear(as_vec(r)); }
template<typename T>	auto linear_to_srgb(const T &r, enable_if_t<is_vec<T>>* =0)	{ return _srgb_to_linear(as_vec(r)); }
template<typename T>	auto srgb_to_linear(const T &r, enable_if_t<!is_vec<T>>* =0)	{ return _srgb_to_linear(r); }
template<typename T>	auto linear_to_srgb(const T &r, enable_if_t<!is_vec<T>>* =0)	{ return _srgb_to_linear(r); }

inline uint8	linear_to_srgb_u8(float c) {
	static constexpr uint32 tab4[104] = {
		0x00e6000d, 0x00f3000d, 0x0100000d, 0x010d000d, 0x011a000d, 0x0127000d, 0x0134000d, 0x0141000d,
		0x014d001a, 0x0167001a, 0x0181001a, 0x019b001a, 0x01b4001a, 0x01ce001a, 0x01e8001a, 0x0202001a,
		0x021b0033, 0x024f0033, 0x02820033, 0x02b60033, 0x02e90033, 0x031d0033, 0x03500033, 0x03840033,
		0x03b70067, 0x041e0067, 0x04850067, 0x04ec0067, 0x05530067, 0x05ba0067, 0x06210067, 0x06880067,
		0x06ef00ce, 0x07bd00ce, 0x088b00ce, 0x095900ce, 0x0a2600cd, 0x0af400c5, 0x0bb800bc, 0x0c7500b5,
		0x0d2a0158, 0x0e820142, 0x0fc40130, 0x10f30120, 0x12140112, 0x13260106, 0x142c00fc, 0x152800f2,
		0x161a01cb, 0x17e501ae, 0x19920195, 0x1b280181, 0x1ca9016e, 0x1e17015e, 0x1f750150, 0x20c50143,
		0x22080264, 0x246c023e, 0x26aa021d, 0x28c70201, 0x2ac901e9, 0x2cb101d3, 0x2e8501c0, 0x304501af,
		0x31f40331, 0x352602fe, 0x382402d3, 0x3af602ad, 0x3da3028d, 0x40300270, 0x42a00256, 0x44f60240,
		0x47360443, 0x4b7903ff, 0x4f7803c4, 0x533c0393, 0x56cf0367, 0x5a360341, 0x5d76031f, 0x60950300,
		0x639605b1, 0x69460555, 0x6e9b0507, 0x73a204c5, 0x7867048b, 0x7cf20458, 0x814a042a, 0x85740402,
		0x89750798, 0x910e071e, 0x982b06b6, 0x9ee2065e, 0xa53f0610, 0xab4f05cc, 0xb11b058f, 0xb6ab0559,
		0xbc040a23, 0xc6270980, 0xcfa708f6, 0xd89d087f, 0xe11c0818, 0xe93407bd, 0xf0f0076c, 0xf85c0723,
	};
	static const iorf maxval(0x7fffff, 127 - 1, 0), minval(0, 127 - 13, 0);

	iorf   f(clamp(c, minval.f(), maxval.f()));
	uint32 t   = tab4[(f.i() - minval.i()) >> 20];
	return (uint8)((((t & 0xffff0000) >> 8) + (t & 0xffff) * ((f.i() >> 12) & 0xff)) >> 16);
}

template<typename T> struct srgb {
	T	t;
	srgb()	{}
	constexpr srgb(const T &t) 	: t(t) {}
};

//-----------------------------------------------------------------------------
//	colour
//-----------------------------------------------------------------------------
struct colour_XYZ;

class colour {
public:
	static constexpr float3 luminance_factors = { 0.3086f, 0.6094f, 0.0820f };
	template<typename R, typename G, typename B> struct colour_const {};
	static constexpr colour_const<_zero,_zero,_zero>	black	= {};
	static constexpr colour_const<_one,_zero,_zero>		red		= {};
	static constexpr colour_const<_zero,_one,_zero>		green	= {};
	static constexpr colour_const<_one,_one,_zero>		yellow	= {};
	static constexpr colour_const<_zero,_zero,_one>		blue	= {};
	static constexpr colour_const<_one,_zero,_one>		magenta	= {};
	static constexpr colour_const<_zero,_one,_one>		cyan	= {};
	static constexpr colour_const<_one,_one,_one>		white	= {};
	union {
		struct {float r, g, b, a; };
		float3	rgb;
		float4	rgba;
	};

	colour()								{}
	template<typename K> explicit colour(const constant<K> &k)	: rgba(k) {}
	explicit constexpr colour(float f)		: rgba(f) {}
	explicit constexpr colour(uint32 c)		: rgba(float4{float((c >> 24) & 255), float((c >> 16) & 255), float((c >> 8) & 255), float(c & 255)} / 255.f) {}
	explicit constexpr colour(float4 rgba)	: rgba(rgba) {}
	explicit constexpr colour(float3 rgb)	: rgba(concat(rgb, one)) {}
	constexpr colour(float3 rgb, float a)	: rgba(concat(rgb, a)) {}
	constexpr colour(float r, float g, float b, float a = one)	: rgba{r, g, b, a} {}

	template<typename R, typename G, typename B> constexpr colour(const colour_const<R, G, B>&) : colour(R(), G(), B()) {}

	colour(const srgb<float3> &c, float a = one)	: colour(srgb_to_linear(c.t), a) {}
	colour(const colour_XYZ& c);

	constexpr float luminance() const	{ return dot(rgb, luminance_factors); }
	srgb<float3>	srgb()		const	{ return linear_to_srgb(rgb); }
	bool			monochrome()const	{ return r == g && r == b; }

	force_inline colour&	operator=(const colour &b)	{ rgba  = b.rgba; return *this; }
	force_inline colour&	operator+=(const colour &b)	{ rgba  += b.rgba; return *this; }
	force_inline bool		operator==(const colour &b)	{ return all(rgba == b.rgba); }
	template<typename B> friend IF_SCALAR(B,colour) operator*(const colour &a, const B &b)	{ return colour(a.rgba * b); }
	template<typename B> friend IF_SCALAR(B,colour) operator/(const colour &a, const B &b)	{ return a * reciprocal(b); }

	friend colour	blend(colour c)					{ return colour(c.rgb * c.a, c.a); }
	friend colour	additive(colour c)				{ return colour(c.rgb * c.a, zero); }
	friend colour	desaturate(colour c, float x)	{ return colour(lerp(c.rgb, c.luminance(), x)); }

};

//-----------------------------------------------------------------------------
// colour_HSV
//-----------------------------------------------------------------------------

inline float3 hsv_to_rgb(float h, float s, float v) {
	float	c = v * s;
	float	m = v - c;
	int		i = int(h * 6);
	float	x = c * (1 - abs(h * 6 - (i | 1))) + m;
	switch (i) {
		case 0:		return {v, x, m};
		case 1:		return {x, v, m};
		case 2:		return {m, v, x};
		case 3:		return {m, x, v};
		case 4:		return {x, m, v};
		default:	return {v, m, x};
	}
}
inline uint8x3 	hsv_to_rgb(uint8 h, uint8 s, uint8 v) {
	uint8	c = (v * s) >> 8;
	uint8	m = v - c, x = v, y = v;
	uint16	i = h * 3;

	if (i & 0x80)
		x -= (c * ( i & 0x7f)) >> 7;
	else
		y -= (c * (~i & 0x7f)) >> 7;

	switch (i >> 8) {
		default:return {x, y, m};
		case 1:	return {m, x, y};
		case 2:	return {y, m, x};
	}
}

inline float3 rgb_to_hsv(float r, float g, float b) {
	float	v = max(r, g, b);
	float	c = v - min(r, g, b);
	return	{
		(c == 0 ? 0.f
			:	v == r ? (g - b) / c
			:	v == g ? (b - r) / c + 2
			:	(r - g) / c + 4
			) / 6,
		v == 0 ? 0.f : c / v,
		v
	};
}

inline uint8x3	rgb_to_hsv(uint8 r, uint8 g, uint8 b) {
	uint8	v = max(r, g, b);
	uint8	c = v - min(r, g, b);
	return {
		uint8(c == 0 ? 0
		:	v == r ?	uint8(g - b) * 128u / (c * 3)
			:	v == g ?	uint8(b - r) * 128u / (c * 3) + 256u / 3
			:				uint8(r - g) * 128u / (c * 3) + 256u * 2 / 3
		),
		uint8(v == 0 ? 0 : c * 255u / v),
		v
	};
}

struct colour_HSV {
	union {
		struct {float h, s, v; };
		float3	x;
	};
	colour_HSV() {}
	constexpr colour_HSV(float h, float s, float v)	: x{h, s, v} {}
	explicit constexpr colour_HSV(float3 x)			: x(x) {}
	explicit colour_HSV(colour c)					: x(rgb_to_hsv(c.r, c.g, c.b)) {}
	operator colour() const { return colour(hsv_to_rgb(h, s, v)); }
};

//-----------------------------------------------------------------------------
// chromaticity
//-----------------------------------------------------------------------------

struct chromaticity {
	enum white {
		A,			// Incandescent / Tungsten
		B,			// {obsolete} Direct sunlight at noon
		C,			// {obsolete} Average / North sky Daylight
		D50,		// Horizon Light. ICC profile PCS
		D55,		// Mid-morning / Mid-afternoon Daylight
		D65,		// Noon Daylight: Television, sRGB color space
		D75,		// North sky Daylight
		E,			// Equal energy
		F1,			// Daylight Fluorescent
		F2,			// Cool White Fluorescent
		F3,			// White Fluorescent
		F4,			// Warm White Fluorescent
		F5,			// Daylight Fluorescent
		F6,			// Lite White Fluorescent
		F7,			// D65 simulator, Daylight simulator
		F8,			// D50 simulator, Sylvania F40 Design 50
		F9,			// Cool White Deluxe Fluorescent
		F10,		// Philips TL85, Ultralume 50
		F11,		// Philips TL84, Ultralume 40
		F12,		// Philips TL83, Ultralume 30
		LED_B1,		// phosphor-converted blue
		LED_B2,		// phosphor-converted blue
		LED_B3,		// phosphor-converted blue
		LED_B4,		// phosphor-converted blue
		LED_B5,		// phosphor-converted blue
		LED_BH1,	// mixing of phosphor-converted blue LED and red LED (blue-hybrid)
		LED_RGB1,	// mixing of red, green, and blue LEDs
		LED_V1,		// phosphor-converted violet
		LED_V2,		// phosphor-converted violet
	};
	static constexpr const_vec<float,2> ref_white[] = {//	temp(K)
		{0.44757f,	0.40745f},	//	A			2856
		{0.34842f,	0.35161f},	//	B			4874
		{0.31006f,	0.31616f},	//	C			6774
		{0.34567f,	0.35850f},	//	D50			5003
		{0.33242f,	0.34743f},	//	D55			5503
		{0.31271f,	0.32902f},	//	D65			6504
		{0.29902f,	0.31485f},	//	D75			7504
		{0.33333f,	0.33333f},	//	E			5454
		{0.31310f,	0.33727f},	//	F1			6430
		{0.37208f,	0.37529f},	//	F2			4230
		{0.40910f,	0.39430f},	//	F3			3450
		{0.44018f,	0.40329f},	//	F4			2940
		{0.31379f,	0.34531f},	//	F5			6350
		{0.37790f,	0.38835f},	//	F6			4150
		{0.31292f,	0.32933f},	//	F7			6500
		{0.34588f,	0.35875f},	//	F8			5000
		{0.37417f,	0.37281f},	//	F9			4150
		{0.34609f,	0.35986f},	//	F10			5000
		{0.38052f,	0.37713f},	//	F11			4000
		{0.43695f,	0.40441f},	//	F12			3000
		{0.4560f,	0.4078f},	//	LED-B1		2733
		{0.4357f,	0.4012f},	//	LED-B2		2998
		{0.3756f,	0.3723f},	//	LED-B3		4103
		{0.3422f,	0.3502f},	//	LED-B4		5109
		{0.3118f,	0.3236f},	//	LED-B5		6598
		{0.4474f,	0.4066f},	//	LED-BH1		2851
		{0.4557f,	0.4211f},	//	LED-RGB1	2840
		{0.4560f,	0.4548f},	//	LED-V1		2724
		{0.3781f,	0.3775f},	//	LED-V2		4070
	};
	static constexpr float c = 299792458.0f;	// speed of light in a vacuum
	static constexpr float h = 6.62607015e-34f;	// planck's constant
	static constexpr float k = 1.380649e-23f;	// boltzmann's constant

	static float blackbody_rel(float nm, float temp) {
		static const float c2 = h * c / k * 1e9f;
		return  1e15f / (pow(nm, 5) * (exp(c2 / (temp * nm)) - one));
	}
	static float blackbody_spectral_radiant_exitance(float nm, float temp) {
		return blackbody_rel(nm, temp) * (pi * two * h * c * c * 1e30);	//x 1e45 to counter pow5(nanometers)
	}
	static float blackbody_spectral_radiance(float nm, float temp) {
		return blackbody_rel(nm, temp) * (two * h * c * c * 1e30);		//x 1e45 to counter pow5(nanometers)
	}
	template<typename T> static float cct_approx(float2 xy, float2 epi, T t, T A) {
		float	n = (xy.x - epi.x) / (xy.y - epi.y);
		return dot(exp(-n / t), A);
	}
	static chromaticity from_cct(float temp);
	static chromaticity from_cct_approx(float T) {
		float x	= 	T < 4000 	?	horner(1000 / T, +0.179910, +0.8776956, -0.2343589, -0.2661239)
								:	horner(1000 / T, +0.240390, +0.2226347, +2.1070379, -3.0258469);
		float y	= 	T < 222		? 	horner(x, -0.20219683, +2.18555832, -1.34811020, -1.1063814)
			:		T < 4000 	? 	horner(x, -0.16748867, +2.09137015, -1.37418593, -0.9549476)
			:						horner(x, -0.37001483, +3.75112997, -5.87338670, +3.0817580);
		return {x, y};
	}
	
	float2 v;
	chromaticity(float x, float y) 	: v{x, y} {}
	chromaticity(white r) 			: v(ref_white[(int)r]) {}
	chromaticity(colour_XYZ c);


	float cct() {
		float	temp = cct_approx(v, float2{0.3366f, 0.1735f}, float3{0.92159f, 0.20039f, 0.07125f}, float3{6253.80338f, 28.70599f, 0.00004f}) - 949.86315f;
		return temp < 50000 ? temp : cct_approx(v, float2{0.3356f, 0.1691f}, float2{0.07861f, 0.01543f}, float2{0.00228f, 5.4535e-36f}) + 36284.48953f;
	}
};

force_inline float cct(chromaticity c) {
	return c.cct();
}

//-----------------------------------------------------------------------------
// colour_XYZ
//-----------------------------------------------------------------------------

struct colour_XYZ {
	static constexpr float3x3c	to_rgb = {{
		{+3.2406255f, -1.537208f,  -0.4986286f},
		{-0.9689307f, +1.8757561f, +0.0415175f},
		{+0.0557101f, -0.2040211f, +1.0569959f}
	}};
	static constexpr float3x3c	from_rgb = {{
		{0.4124f, 0.3576f, 0.1805f},
		{0.2126f, 0.7152f, 0.0722f},
		{0.0193f, 0.1192f, 0.9505f}
	}};

	template<typename T, typename C>	static auto gaussian( T x, C mu, C sigma) 	{ return exp(square((x - mu) / sigma) / -2); }
	template<typename T, typename C>	static auto dgaussian(T x, C mu, C sigma) 	{ return -gaussian(x, mu, sigma) * (x - mu) / square(sigma); }
	template<typename C> struct gparams {
		C	weights, mu, sigma1, sigma2;
		template<typename T>	auto get(T x) 	const	{ return dot(weights,  gaussian(x, mu, select(x < mu, sigma1, sigma2))); }
		template<typename T>	auto dget(T x) const	{ return dot(weights, dgaussian(x, mu, select(x < mu, sigma1, sigma2))); }
	};
	static constexpr	gparams<const_vec<float, 3>> gp_x = {{1.056f, 0.362f, -0.065f}, {599.8f, 442.0f, 501.1f}, 	{37.9f, 16.0f, 20.4f}, 	{31.0f, 26.7f, 26.2f}};
	static constexpr	gparams<const_vec<float, 2>> gp_y = {{0.821f, 0.286f}, 			{568.8f, 530.9f}, 			{46.9f, 16.3f}, 		{40.5f, 31.1f}};
	static constexpr	gparams<const_vec<float, 2>> gp_z = {{1.217f, 0.681f}, 			{437.0f, 459.0f}, 			{11.8f, 26.0f},			{36.0f, 13.8f}};

	float3	v;
	
	static colour_XYZ from_wavelength(float nm) {
		return colour_XYZ(gp_x.get(nm), gp_y.get(nm), gp_z.get(nm));
	}
	static float3 dXYZ(float nm) {
		return {gp_x.dget(nm), gp_y.dget(nm), gp_z.dget(nm)};
	}

	friend float dominant_wavelength(chromaticity col, chromaticity ref) {
		float2	rel = col.v - ref.v;
		float 	t 	= 520, dt;
		do {
			auto	x 	= from_wavelength(t);
			auto	dx	= dXYZ(t);
		
			float	b	= reduce_add(x.v);
			float	db 	= reduce_add(dx);
			float2	dc 	= (dx.xy * b - x.v.xy * db) / square(b);
			
			auto 	d0 	= x.v.xy / b - ref.v;
			dt 	= cross(d0, rel) / cross(d0, dc);
			t 	+= dt;
		} while (dt > 0.1f && between(t, 440, 640));
		
		return t;
	}

	constexpr colour_XYZ(float x, float y, float z)	: v{x, y, z} {}
	explicit constexpr colour_XYZ(float3 v)			: v(v) {}
	explicit colour_XYZ(colour c)					: v(c.rgb * from_rgb) {}
	colour_XYZ(chromaticity c)						: v(concat(c.v, one - c.v.x - c.v.y)) {}
};

inline colour::colour(const colour_XYZ& c) : colour(c.v * colour_XYZ::to_rgb) {}

inline chromaticity::chromaticity(colour_XYZ c) : v(c.v.xy / reduce_add(c.v)) {}

inline chromaticity chromaticity::from_cct(float temp) {
	float3	xyz(zero);
	for (float nm = 360; nm <= 830; nm += 5)
		xyz += blackbody_rel(nm, temp) * colour_XYZ::from_wavelength(nm).v;
	return colour_XYZ(xyz);
}

//-----------------------------------------------------------------------------
// colour_xyY
//-----------------------------------------------------------------------------

struct colour_xyY {
	union {
		struct {float x, y, Y;};
		float3	v;
	};
	constexpr colour_xyY(float x, float y, float Y)		: v{x, y, Y} {}
	explicit constexpr colour_xyY(float3 v)				: v(v) {}
	explicit colour_xyY(const colour_XYZ &c)			: v(concat(c.v.xy / reduce_add(c.v), c.v.y)) {}
	operator colour_XYZ() const { return colour_XYZ(float3{x * Y / y, Y, (1 - x - y) * Y / y}); }
};

//-----------------------------------------------------------------------------
// colour_Lab
//-----------------------------------------------------------------------------

struct colour_Lab {
	union {
		struct {float L, a, b; };
		float3	v;
	};
	static float3 f(float3 t)	{ return select(t > 216.f / 24389, 	pow(t, third), (24389 * t + 432) / 3132); }
	static float3 r(float3 t)	{ return select(t > 6.f / 29, 		cube(t), (t * 3132 - 432) / 24389); }
	
	constexpr colour_Lab(float L, float a, float b)	: v{L, a, b}	{}
	explicit constexpr colour_Lab(float3 v)			: v(v)			{}
	colour_Lab(const colour_XYZ &c) {
		float3	t = f(c.v);
		v = float3{116 * t.y - 16, 500 * (t.x - t.y), 200 * (t.y - t.z)};
	}
	operator colour_XYZ() const { return colour_XYZ(r(float3{a / 500, zero, b / 200} + (L + 16) / 116)); }
};

//-----------------------------------------------------------------------------
// colour_YCbCr
//-----------------------------------------------------------------------------

struct colour_YCbCr {
	static constexpr float3x3c	from_srgb = {{
		{+0.299f, +0.587f, +0.114f },
		{-0.169f, -0.331f, +0.499f },
		{+0.499f, -0.418f, -0.0813f}
	}};
	static constexpr float3x3c	to_srgb = {{
		{one,	zero,		+1.40200f},
		{one, -0.34414f,	-0.71414f},
		{one, +1.77200f,	zero}
	}};
	union {
		struct {float Y, Cb, Cr; };
		float3	v;
	};
	constexpr colour_YCbCr(float Y, float Cb, float Cr)	: v{Y, Cb, Cr}	{}
	explicit constexpr colour_YCbCr(float3 v) 			: v(v)			{}
	explicit colour_YCbCr(const srgb<float3> &c) 		: colour_YCbCr(c.t * from_srgb) {}
	operator srgb<float3>() const { return v * to_srgb; }
};

//-----------------------------------------------------------------------------
// colour_YUV
//-----------------------------------------------------------------------------

struct colour_YUV {
	static constexpr float3x3c	from_srgb = {{
		{+0.299f, +0.587f, +0.114f},
		{-0.147f, -0.289f, +0.436f},
		{+0.615f, -0.515f, -0.100f}
	}};
	static constexpr float3x3c	to_srgb = {{
		{one,	zero,		+1.140f},
		{one, -0.395f,	-0.581f},
		{one, +2.032f,	zero}
	}};
	union {
		struct {float Y, U, V;};
		float3	v;
	};
	constexpr colour_YUV(float Y, float U, float V)	: v{Y, U, V}	{}
	explicit constexpr colour_YUV(float3 v)			: v(v)			{}
	explicit colour_YUV(const srgb<float3> &c)		: v(c.t * from_srgb) {}
	operator srgb<float3>() const { return v * to_srgb; }
};

//-----------------------------------------------------------------------------
// colour_AdobeRGB
//-----------------------------------------------------------------------------

struct colour_AdobeRGB {
	static constexpr float3x3c	from_xyz = {{
		{+2.04159, -0.56501, -0.34473},
		{-0.96924, +1.87597, +0.04156},
		{+0.01344, -0.11836, +1.01517}
	}};
	static constexpr float3x3c	to_xyz = {{
		{0.57667, 0.18556, 0.18823},
		{0.29734, 0.62736, 0.07529},
		{0.02703, 0.07069, 0.99134}
	}};
	union {
		struct { float R, G, B; };
		float3	v;
	};
	constexpr colour_AdobeRGB(float R, float G, float B)	: v{R, G, B}	{}
	explicit constexpr colour_AdobeRGB(float3 v)			: v(v)			{}
	colour_AdobeRGB(const colour_XYZ& c)					: v(pow(c.v * from_xyz, 563 / 256.f)) {}
	operator colour_XYZ() const { return colour_XYZ(pow(v, 256 / 563.f) * to_xyz); }
};

//-----------------------------------------------------------------------------
// ColourAdjust
//-----------------------------------------------------------------------------

struct ColourAdjust {
	float3	v;
	ColourAdjust() : v(one)	{}
	ColourAdjust(float saturation, float contrast, float brightness) : v{saturation, contrast, brightness}	{}

	void		Clear()								{ v = float3(one);	}
	void		operator*=(const ColourAdjust &b)	{ v *= b.v;	}
	float3x4	GetMatrix() const {
		float3		lum			= colour::luminance_factors * (one - v.x);
		float3x3	saturation	= (float3x3)transpose(float3x3(lum, lum, lum)) + (float3x3)scale(v.x);
		float3x4	contrast	= translate(position3(half)) * scale(v.y) * translate(position3(-half));
		return saturation * contrast * scale(v.z);
	}
	float		GetSaturation()		const			{ return v.x; }
	float		GetContrast()		const			{ return v.y; }
	float		GetBrightness()		const			{ return v.z; }
};


} // namespace iso

#endif // COLOUR_H
