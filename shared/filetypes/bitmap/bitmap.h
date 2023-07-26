#ifndef BITMAP_H
#define BITMAP_H

#include "vector_iso.h"
#include "base/algorithm.h"
#include "base/bits.h"
#include "base/block.h"
#include "base/maths.h"
#include "extra/filters.h"
#include "iso/iso.h"

namespace iso {

//-----------------------------------------------------------------------------
//	HDRpixel
//-----------------------------------------------------------------------------

struct HDRpixel : array_vec<float, 4> {
	typedef array_vec<float, 4>	B;

	HDRpixel() {}
	HDRpixel(const B &b)	: B(b) {}
	HDRpixel(unp b)			: B(b) {}
	HDRpixel(float i)						: B{i, i, i, i} {}
	HDRpixel(float i, float a)				: B{i, i, i, a} {}
	HDRpixel(float r, float g, float b, float a = 1) : B(r, g, b, a) {}
	HDRpixel	WithAlpha(float a)	const	{ return HDRpixel(r, g, b, a); }
	bool		HasAlpha()			const	{ return a != 1; }
	bool		IsGrey()			const	{ return r == g && r == b; }

	template<typename P> friend enable_if_t<num_elements_v<P> == 1> assign(HDRpixel& v, const P& p) { v = float4(p); }//concat(float3(p), one); }
	template<typename P> friend enable_if_t<num_elements_v<P> == 2> assign(HDRpixel& v, const P& p) { v = vec<float,4>(concat(to<float>(p), zero).xzzy); }
	template<typename P> friend enable_if_t<num_elements_v<P> == 3> assign(HDRpixel& v, const P& p) { v = to<float>(p, one); }
	template<typename P> friend enable_if_t<num_elements_v<P> == 4> assign(HDRpixel& v, const P& p) { v = to<float>(p); }

	template<typename P> friend enable_if_t<num_elements_v<P> == 2> assign(P& p, const HDRpixel& v) { p = to<element_type<P>>(v.xy); }
	template<typename P> friend enable_if_t<num_elements_v<P> == 3> assign(P& p, const HDRpixel& v) { p = to<element_type<P>>(v.xyz); }
	template<typename P> friend enable_if_t<num_elements_v<P> == 4> assign(P& p, const HDRpixel& v) { p = to<element_type<P>>(v); }

	friend void assign(HDRpixel& p, const HDRpixel& v) { p = v; }

	friend HDRpixel srgb_to_linear(const HDRpixel& c) { return HDRpixel(concat(srgb_to_linear(c.rgb), c.a)); }
	friend HDRpixel linear_to_srgb(const HDRpixel& c) { return HDRpixel(concat(linear_to_srgb(c.rgb), c.a)); }
};
template<> static constexpr int num_elements_v<HDRpixel> = 4;

inline HDRpixel PreMultiplied(const HDRpixel& a) { return masked.xyz(a) * (float)a.a; }

//-----------------------------------------------------------------------------
//	ISO_rgba
//-----------------------------------------------------------------------------

struct ISO_rgba {
	uint8 r, g, b, a;

	static ISO_rgba YCbCr(int Y, int Cb, int Cr) { 
		return ISO_rgba(clamp(int(Y + 1.40200 * Cr + .5), 0, 255), clamp(int(Y - 0.34414 * Cb - 0.71414 * Cr + .5), 0, 255), clamp(int(Y + 1.77200 * Cb + .5), 0, 255)); 
	}

	static ISO_rgba YUV(int Y, int U, int V) {
		int y = Y - 16, u = U - 128, v = V - 128;
		return ISO_rgba(clamp((298 * y + 409 * v + 128) >> 8, 0, 255), clamp((298 * y - 100 * u - 208 * v + 128) >> 8, 0, 255), clamp((298 * y + 516 * u + 128) >> 8, 0, 255));
	}

	ISO_rgba() {}
	ISO_rgba(int r, int g, int b, int a = 255)	: r(r), g(g), b(b), a(a) {}
	ISO_rgba(int i)								: r(i), g(i), b(i), a(i) {}
	ISO_rgba(int i, int a)						: r(i), g(i), b(i), a(a) {}
	ISO_rgba(const ISO_rgba& rgb, int _a) : r(rgb.r), g(rgb.g), b(rgb.b), a(_a) {}
	ISO_rgba(const HDRpixel& h) : r(uint8(clamp(h.r, 0.f, 1.f) * 255)), g(uint8(clamp(h.g, 0.f, 1.f) * 255)), b(uint8(clamp(h.b, 0.f, 1.f) * 255)), a(uint8(clamp(h.a, 0.f, 1.f) * 255)) {}
	ISO_rgba(uint8x4 v) : ISO_rgba(reinterpret_cast<const ISO_rgba&>(v)) {}
	uint8	operator[](int i)				const	{ return (&r)[i]; }
	uint8&   operator[](int i)						{ return (&r)[i]; }
	bool	 HasAlpha()						const	{ return a != 0xff; }
	bool	 IsGrey()						const	{ return r == g && r == b; }
	int		 MaxComponent()					const	{ return r > g ? (r > b ? 0 : 2) : (g > b ? 1 : 2); }
	int		 MaxComponentA()				const	{ return max(r, g) > max(b, a) ? (r > g ? 0 : 1) : (b > a ? 2 : 3); }
	bool	 operator==(const ISO_rgba& c2)	const	{ return r == c2.r && g == c2.g && b == c2.b && a == c2.a; }
	bool	 operator!=(const ISO_rgba& c2)	const	{ return r != c2.r || g != c2.g || b != c2.b || a != c2.a; }
	ISO_rgba operator+=(const ISO_rgba& c2) {
		r = min(r + c2.r, 255);
		g = min(g + c2.g, 255);
		b = min(b + c2.b, 255);
		a = min(a + c2.a, 255);
		return *this;
	}
	ISO_rgba operator-=(const ISO_rgba& c2) {
		r = max(r + c2.r, 0);
		g = max(g + c2.g, 0);
		b = max(b + c2.b, 0);
		a = max(a + c2.a, 0);
		return *this;
	}
	ISO_rgba operator+=(int t) {
		r = clamp(r + t, 0, 255);
		g = clamp(g + t, 0, 255);
		b = clamp(b + t, 0, 255);
		return *this;
	}
	ISO_rgba operator-=(int t) {
		return *this += -t;
	}
	ISO_rgba operator*=(const ISO_rgba& c2) {
		r = r * c2.r / 255;
		g = g * c2.g / 255;
		b = b * c2.b / 255;
		return *this;
	}
	ISO_rgba operator*=(uint8 t) {
		r = r * t / 255;
		g = g * t / 255;
		b = b * t / 255;
		return *this;
	}
	ISO_rgba operator/=(uint8 t) {
		r = r * 255 / t;
		g = g * 255 / t;
		b = b * 255 / t;
		return *this;
	}
	operator HDRpixel() const { return HDRpixel(r / 255.f, g / 255.f, b / 255.f, a / 255.f); }

	friend HDRpixel operator+(const ISO_rgba& a, const ISO_rgba& b) { return HDRpixel(a) + HDRpixel(b); }
	friend HDRpixel operator-(const ISO_rgba& a, const ISO_rgba& b) { return HDRpixel(a) - HDRpixel(b); }
	friend ISO_rgba operator+(const ISO_rgba& a, int b)		{ return ISO_rgba(a) += b; }
	friend ISO_rgba operator-(const ISO_rgba& a, int b)		{ return ISO_rgba(a) -= b; }
	friend ISO_rgba operator*(const ISO_rgba& a, uint8 b)	{ return ISO_rgba(a) *= b; }
	friend HDRpixel operator*(const ISO_rgba& a, float b) {
		b /= 255;
		return HDRpixel(a.r * b, a.g * b, a.b * b, a.a * b);
	}

	friend HDRpixel operator+(const ISO_rgba& a, const HDRpixel& b) { return HDRpixel(a) += b; }
	friend HDRpixel operator-(const ISO_rgba& a, const HDRpixel& b) { return HDRpixel(a) -= b; }

	friend ISO_rgba PreMultiplied(const ISO_rgba& a)							{ return a * a.a; }
	friend ISO_rgba Blend(const ISO_rgba& a, const ISO_rgba& b)					{ return ISO_rgba((a.r * (255 - b.a) + b.r * b.a) / 255, (a.g * (255 - b.a) + b.g * b.a) / 255, (a.b * (255 - b.a) + b.b * b.a) / 255, max(a.a, b.a)); }
	friend ISO_rgba PreMultipliedBlend(const ISO_rgba& a, const ISO_rgba& b)	{ return b.a ? ISO_rgba((a.r * (255 - b.a)) / 255 + b.r, (a.g * (255 - b.a)) / 255 + b.g, (a.b * (255 - b.a)) / 255 + b.b, max(a.a, b.a)) : a; }
	friend void assign(ISO_rgba& p, const HDRpixel& v)							{ p = v; }
	friend void assign(HDRpixel& p, const ISO_rgba& v)							{ p = v; }
	template<typename P> friend enable_if_t<num_elements_v<P> == 4> assign(ISO_rgba& v, const P& p) { v = HDRpixel(to<float>(p)); }

	friend ISO_rgba InterpCol(const ISO_rgba &ca, const ISO_rgba &cb, int sa, int sb) {
		int	t	= sa + sb;
		int	t2	= t / 2;
		return ISO_rgba(
			(sa * ca.r + sb * cb.r + t2) / t,
			(sa * ca.g + sb * cb.g + t2) / t,
			(sa * ca.b + sb * cb.b + t2) / t
		);
	}

};

inline ISO_rgba min(const ISO_rgba& h1, const ISO_rgba& h2) { return ISO_rgba(min(h1.r, h2.r), min(h1.g, h2.g), min(h1.b, h2.b), min(h1.a, h2.a)); }
inline ISO_rgba max(const ISO_rgba& h1, const ISO_rgba& h2) { return ISO_rgba(max(h1.r, h2.r), max(h1.g, h2.g), max(h1.b, h2.b), max(h1.a, h2.a)); }

//-----------------------------------------------------------------------------
//	Colour spaces
//-----------------------------------------------------------------------------

struct YUYV {
	uint8 y0, u, y1, v;
	YUYV(uint8 y0, uint8 u, uint8 y1, uint8 v) : y0(y0), u(u), y1(y1), v(v) {}
	YUYV() {}
};

// Convert from float to RGBE as outlined in Gregory Ward's Real Pixels article, Graphics Gems II, page 80.
struct rgbe {
	uint8x4	v;

	rgbe() {}
	rgbe(param(float3) col) {
		const float	primary = reduce_max(col);
		if (primary < 1e-32f) {
			v = 0;
		} else {
			iorf	primary2(primary);
			const float scale = primary2.get_mantf() / primary * 255;
			v = to<uint8>(min(max(col * scale, 0.f), 255.f), primary2.e);
		}
	}
	friend void assign(HDRpixel& p, const rgbe& v) {
		if (v.v.w == 0) {
			p = 0;
		} else {
			const float scale = iorf::exp2(v.v.w - 128).f() / 255.f;
			p = concat(to<float>(v.v.xyz) * scale, one);
		}
	}
};


//-----------------------------------------------------------------------------
//	TexelFormat
//-----------------------------------------------------------------------------

template<int B> struct TexelComponent {
	static uint8  to8(uint32 x, uint8 def)	{ return extend_bits<B, 8>(x & kbits<uint32, B>); }
	static float  toF(uint32 x, float def)	{ return float(x & kbits<uint32, B>) / kbits<uint32, B>; }
	static uint32 from8(uint8 x)			{ return extend_bits<8, B>(x); }
	static uint32 fromF(float x)			{ return uint32(x * bits(B)); }
};
template<> struct TexelComponent<0> {
	static uint8  to8(uint32 x, uint8 def)	{ return def; }
	static float  toF(uint32 x, float def)	{ return def; }
	static uint32 from8(uint8 x)			{ return 0; }
	static uint32 fromF(float x)			{ return 0; }
};

template<int bits, int rpos, int rbits, int gpos, int gbits, int bpos, int bbits, int apos = 0, int abits = 0> class TexelFormat {
public:
	typedef uint_bits_t<bits> T;
	enum { BITS = bits, RBITS = rbits, GBITS = gbits, BBITS = bbits, ABITS = abits };

	static inline uint32 Get(uint8 r, uint8 g, uint8 b, uint8 a) {
		return (TexelComponent<rbits>::from8(r) << rpos) | (TexelComponent<gbits>::from8(g) << gpos) | (TexelComponent<bbits>::from8(b) << bpos) | (TexelComponent<abits>::from8(a) << apos);
	}
	static inline T		 Get(const ISO_rgba& p) {
		return Get(p.r, p.g, p.b, p.a);
	}
	static inline T		 Get(const HDRpixel& p) {
		return (TexelComponent<rbits>::fromF(p.r) << rpos) | (TexelComponent<gbits>::fromF(p.g) << gpos) | (TexelComponent<bbits>::fromF(p.b) << bpos) | (TexelComponent<abits>::fromF(p.a) << apos);
	}
	static inline ISO_rgba Put(T p) {
		return ISO_rgba(TexelComponent<rbits>::to8(p >> rpos, 0), TexelComponent<gbits>::to8(p >> gpos, 0), TexelComponent<bbits>::to8(p >> bpos, 0), TexelComponent<abits>::to8(p >> apos, 0xff));
	}
	static inline HDRpixel PutHDR(T p) {
		return HDRpixel(TexelComponent<rbits>::toF(p >> rpos, 0), TexelComponent<gbits>::toF(p >> gpos, 0), TexelComponent<bbits>::toF(p >> bpos, 0), TexelComponent<abits>::toF(p >> apos, 1));
	}
};

typedef TexelFormat<32, 0, 8, 8, 8, 16, 8, 24, 8>	R8G8B8A8;
typedef TexelFormat<32, 8, 8, 16, 8, 24, 8, 0, 8>	A8R8G8B8;
typedef TexelFormat<32, 16, 8, 8, 8, 0, 8, 24, 8>	B8G8R8A8;
typedef TexelFormat<32, 16, 8, 8, 8, 0, 8>			B8G8R8_8;

typedef TexelFormat<24, 0, 8, 8, 8, 16, 8>			R8G8B8;
typedef TexelFormat<24, 16, 8, 8, 8, 0, 8>			B8G8R8;

typedef TexelFormat<16, 0, 5, 5, 5, 10, 5>			R5G5B5;
typedef TexelFormat<16, 0, 5, 5, 5, 10, 5, 15, 1>	R5G5B5A1;
typedef TexelFormat<16, 10, 5, 5, 5, 0, 5, 15, 1>	B5G5R5A1;
typedef TexelFormat<16, 0, 5, 5, 6, 11, 5>			R5G6B5;
typedef TexelFormat<16, 11, 5, 5, 6, 0, 5>			B5G6R5;
typedef TexelFormat<16, 0, 4, 4, 4, 8, 4, 12, 4>	R4G4B4A4;
typedef TexelFormat<16, 8, 4, 4, 4, 0, 4, 12, 4>	B4G4R4A4;
typedef TexelFormat<16, 4, 4, 8, 4, 12, 4, 0, 4>	A4R4G4B4;

typedef TexelFormat<1, 0, 0, 0, 0, 0, 0, 0, 1>		A1;
typedef TexelFormat<8, 0, 0, 0, 0, 0, 0, 0, 8>		A8;
typedef TexelFormat<8, 0, 8, 0, 0, 0, 0, 0, 0>		R8;
typedef TexelFormat<16, 0, 8, 8, 8, 0, 0, 0, 0>		R8G8;

template<typename FORMAT> class Texel {
protected:
	uintn<(FORMAT::BITS + 7) / 8> v;

public:
	Texel() {}
	Texel(uint32 v) : v(v) {}
	Texel(const ISO_rgba& c)					{ v = FORMAT::Get(c); }
	//Texel(const HDRpixel &c)					{ v = FORMAT::Get(c);	}
	void		operator=(const ISO_rgba& c)	{ v = FORMAT::Get(c); }
	operator ISO_rgba()				const		{ return FORMAT::Put(v); }
	operator HDRpixel()				const		{ return FORMAT::PutHDR(v); }
	HDRpixel	operator*(float f)	const		{ return operator HDRpixel() * f; }
	auto		representation()	const		{ return v; }

	friend void assign(HDRpixel& p, const Texel& v) { p = v; }
};

template<typename D, typename S, int N> void copy(const block<Texel<S>, 1> &s, D &d) {
	auto	j = d.begin();
//	for (auto i = s.begin(), ie = i + min(s.size(), d.size()); i != ie; ++i, ++j)
//		copy(*i, *j);
}


template<typename FORMAT> class BlendedTexel : public Texel<FORMAT> {
	using Texel<FORMAT>::v;
public:
	void operator=(const ISO_rgba& c) { v = FORMAT::Get(Blend(FORMAT::Put(v), c)); }
	friend void assign(HDRpixel& p, const BlendedTexel& v) { p = v; }
};

template<typename FORMAT> class PreMultipliedTexel : public Texel<FORMAT> {
	using Texel<FORMAT>::v;
public:
	void operator=(const HDRpixel& c) { v = FORMAT::Get(PreMultiplied(c)); }
	void operator=(const ISO_rgba& c) { v = FORMAT::Get(PreMultiplied(c)); }
	friend void assign(HDRpixel& p, const PreMultipliedTexel& v) { p = v; }
};

//-----------------------------------------------------------------------------
//	mipping
//-----------------------------------------------------------------------------

template<typename T, int N> uint32 MaxMips(const block<T, N>& b) { return log2_ceil(b.max_size()); }

template<typename T, int N> void _GetMip(block<T, N>& b, int n) {
	b.cut(0, max(b.size() >> n, 1));
	_GetMip((block<T, N - 1>&)b, n);
}
template<typename T> void _GetMip(block<T, 1>& b, int n) {
	b.cut((((b.size() << n) - b.size()) >> n), max(b.size() >> (n + 1), 1));
}
template<typename T, int N> block<T, N> GetMip(const block<T, N>& b, int n) {
	block<T, N> b2 = b;
	_GetMip(b2, n);
	return b2;
}

// 2D mips (mipped mips)
template<typename T, int N> void _GetMip2(block<T, N>& b, int n1, int n2) {
	b.cut(0, max(b.size() >> (n1 + n2), 1));
	_GetMip2((block<T, N - 1>&)b, n1, n2);
}

template<typename T> void _GetMip2(block<T, 2>& b, int n1, int n2) {
	if (n1 == 0) {
		int s = b.size();
		b.cut(s >> 1, max(s >> (n1 + n2), 1));
		_GetMip((block<T, 1>&)b, n2);
	} else {
		int s = (b.size() * 2) >> n1;
		b.cut((((s << n2) - s) >> n2), max(s >> (n2 + 1), 1));
		_GetMip2((block<T, 1>&)b, n1, n2);
	}
}
template<typename T> void _GetMip2(block<T, 1>& b, int n1, int n2) {
	b.cut((((b.size() << n1) - b.size()) >> n1), max(b.size() >> (n1 + n2 + 1), 1));
}
template<typename T, int N> block<T, N> GetMip2(const block<T, N>& b, int n1, int n2) {
	block<T, N> b2 = b;
	_GetMip2(b2, n1, n2);
	return b2;
}

void BoxFilter(const block<ISO_rgba, 2>& srce, const block<ISO_rgba, 2>& dest, bool alpha1bit = false);
void BoxFilter(const block<ISO_rgba, 3>& srce, const block<ISO_rgba, 3>& dest, bool alpha1bit = false);
void BoxFilter(const block<HDRpixel, 2>& srce, const block<HDRpixel, 2>& dest, bool alpha1bit = false);
void BoxFilter(const block<HDRpixel, 3>& srce, const block<HDRpixel, 3>& dest, bool alpha1bit = false);

//-----------------------------------------------------------------------------
//	bitmap
//-----------------------------------------------------------------------------

enum {
	BMF_SEPALPHA	 = 1 << 0,
	BMF_NOCOMPRESS   = 1 << 1,
	BMF_UNNORMALISED = 1 << 2,
	BMF_VOLUME		 = 1 << 3,
	BMF_CUBE		 = 1 << 4,
	BMF_UVCLAMP		 = 1 << 5,
	BMF_NOMIP		 = 1 << 6,

	// actually in mips
	BMF_SCANNED		= 1 << 8,
	BMF_ALPHA		= 1 << 9,
	BMF_GREY		= 1 << 10,

	// passed in to Create
	BMF_MIPS		= 1 << 14,
	BMF_CLEAR		= 1 << 15,

	// stored in mips
	BMF2_MIPMASK	= 15,
	BMF2_SCANNED	= 1 << 4,
	BMF2_ALPHA		= 1 << 5,
	BMF2_GREY		= 1 << 6,
};

enum TextureType {
	TT_NORMAL,
	TT_ARRAY,
	TT_CUBE,
	TT_VOLUME,
};
inline int MaxMips(uint32 width, uint32 height) { return log2_ceil(max(width, height)); }

iso_export uint16 Scan(const block<ISO_rgba, 1>& block, uint16 flags);
iso_export uint16 Scan(const block<ISO_rgba, 2>& block, uint16 flags);
iso_export void   Unpalette(ISO_rgba* p, size_t n, const ISO_rgba* clut, uint32 flags);
iso_export uint32 GetBitmapFlags();

template<int B> class _bitmap {
	friend struct ISO::def<_bitmap>;
	template<int B2> friend class iso::_bitmap;
	uint16						width, height, depth;
	uint8						flags;
	mutable uint8				mips;
	ISO_openarray<ISO_rgba, B>	clut, texels;
	
	void		_Scan() const;

public:
	_bitmap() : width(0), height(0), depth(0), flags(0), mips(0) {}
	_bitmap(int _width, int _height, uint32 _flags = 0, int _depth = 1) { Create(_width, _height, _flags, _depth); }
	template<int B2> _bitmap(const _bitmap<B2>& b)	: width(b.width), height(b.height), depth(b.depth), flags(b.flags), mips(b.mips), clut(b.clut), texels(b.texels) {}
	template<int B2> _bitmap(_bitmap<B2>&& b)		: width(b.width), height(b.height), depth(b.depth), flags(b.flags), mips(b.mips), clut(move(b.clut)), texels(move(b.texels)) {}

	ISO_rgba*		Create(int _width, int _height, uint32 _flags = 0, int _depth = 1);
	block<ISO_rgba, 1>	CreateClut(uint32 _clutsize) { return {clut.Create(_clutsize).begin(), _clutsize}; }
	void			SetDepth(int _depth)	{ height = Height() / _depth; depth  = _depth; }
	void			SetMips(int _mips)		{ mips = clamp(_mips - 1, 0, BMF2_MIPMASK);	}
	int				Scan()			const	{ if (!(mips & BMF2_SCANNED)) _Scan(); return Flags(); }
	int				Flags()			const	{ return flags | ((mips & ~BMF2_MIPMASK) << 4); }
	void			SetFlags(uint32 i)		{ flags |= i; }
	void			ClearFlags(uint32 i)	{ flags &= ~i; }

	int				Width()			const	{ return width; }
	int				Height()		const	{ return height * depth; }
	int				Depth()			const	{ return depth; }
	int				Mips()			const	{ return mips & BMF2_MIPMASK; }
	const ISO_rgba* Clut()			const	{ return clut; }
	const ISO_rgba& Clut(int i)		const	{ return clut[i]; }
	uint32			ClutSize()		const	{ return clut.Count(); }
	const ISO_rgba* ScanLine(int y) const	{ return texels + y * width; }

	ISO_rgba*		Clut()					{ return clut; }
	ISO_rgba&		Clut(int i)				{ return clut[i]; }
	ISO_rgba*		ScanLine(int y)			{ return texels + y * width; }

	int				BaseWidth()		const	{ return Mips() ? width / 2 : width; }
	int				BaseHeight()	const	{ return height; }
	int				MaxMips()		const	{ return iso::MaxMips(BaseWidth(), BaseHeight()); }

	bool			IsPaletted()	const	{ return Clut() != NULL; }
	bool			IsCube()		const	{ return !!(flags & BMF_CUBE); }
	bool			IsVolume()		const	{ return !!(flags & BMF_VOLUME); }
	bool			IsIntensity()	const	{ return !!(Scan() & BMF_GREY); }
	bool			HasAlpha()		const	{ return !!(Scan() & BMF_ALPHA); }
	TextureType		Type()			const	{ return IsVolume() ? TT_VOLUME : IsCube() ? TT_CUBE : Depth() == 1 ? TT_NORMAL : TT_ARRAY; }

	void Unpalette() {
		iso::Unpalette(texels.begin(), texels.size(), clut.begin(), flags);
		clut.Clear();
	}
	iso_export void Crop(int x, int y, int w, int h, ISO_rgba c = ISO_rgba(0, 0, 0));

	ISO_rgba GetTexel(const ISO_rgba* p)	const { return clut ? (flags & BMF_SEPALPHA ? ISO_rgba(clut[p->r], p->a) : clut[p->r]) : *p; }
	ISO_rgba GetTexel(int x, int y)			const { return GetTexel(ScanLine(y) + x); }

	operator block<ISO_rgba, 2>()			const { return make_block(unconst(texels.begin()), width, Height()); }
	block<ISO_rgba, 2> All()				const { return *this; }
	block<ISO_rgba, 2> Mip(int n)			const { return GetMip(All(), n); }
	block<ISO_rgba, 2> Base()				const { return Mips() ? Mip(0) : All(); }
	block<ISO_rgba, 2> Slice(int n)			const { return Block(0, BaseHeight() * n, width, BaseHeight()); }
	block<ISO_rgba, 3> All3D()				const { return make_block(unconst(texels.begin()), width, BaseHeight(), depth); }
	block<ISO_rgba, 2> Block(int x, int y, int w, int h) const {
		return All().template sub<1>(x, w).template sub<2>(y, h);
	}
	block<ISO_rgba, 3> Mip3D(int n) const {
		block<ISO_rgba, 3> b = All3D();
		if (IsVolume())
			return GetMip(b, n);
		_GetMip(b.template get<2>(), n);
		return b;
	}
	block<ISO_rgba, 3> MipArray(int n) const {
		block<ISO_rgba, 3> b = All3D();
		_GetMip(b.template get<2>(), n);
		return b;
	}
	block<ISO_rgba, 1> ClutBlock()			const	{ return {unconst(clut.begin()), clut.size32()}; }
	block<ISO_rgba, 1> ScanLineBlock(int y) const	{ return {unconst(texels.begin()) + y * width, width}; }
};

template<int B> ISO_rgba* _bitmap<B>::Create(int _width, int _height, uint32 _flags, int _depth) {
	uint64	total	= _width * _height;
	if (_flags & BMF_MIPS) {
		mips = iso::MaxMips(_width, _height);
		_width *= 2;
	} else {
		mips = 0;
	}

	width  = _width;
	height = _height / _depth;
	depth  = _depth;
	flags  = _flags;

	ISO_rgba* p = texels.Create(_width * _height, false);
	if (_flags & BMF_CLEAR)
		memset(p, 0, _width * _height * sizeof(ISO_rgba));
	return p;
}

template<int B> void _bitmap<B>::_Scan() const {
	uint16 f = BMF_GREY;
	if (clut) {
		f = iso::Scan(ClutBlock(), f);
	} else if (int m = Mips()) {
		for (int i = 0; i < m; i++)
			f = iso::Scan(Mip(i), f);
	} else {
		f = iso::Scan(All(), f);
	}
	mips = (mips & BMF2_MIPMASK) | ((f | BMF_SCANNED) >> 4);
}

template<int B> void _bitmap<B>::Crop(int x, int y, int w, int h, ISO_rgba c) {
	if (x || y || w != width || h != height) {
		ISO_openarray<ISO_rgba, B> newtexels(w * h);
		for (int yd = 0; yd < h; yd++) {
			ISO_rgba* dest = newtexels + w * yd;
			int		  ys   = yd + y;
			if (ys < 0 || ys >= height) {
				for (int xd = 0; xd < w; xd++)
					*dest++ = c;
			} else {
				int xd;
				for (xd = 0; xd + x < 0; xd++)
					*dest++ = c;
				for (int xs = max(x, 0); xd < w && xs < width; xs++, xd++)
					*dest++ = ScanLine(ys)[xs];
				for (; xd < w; xd++)
					*dest++ = c;
			}
		}
		texels.Clear();
		texels = move(newtexels);
		width  = w;
		height = h;
		depth  = 1;
	}
}

template<int B> void FillMips(_bitmap<B>* bm, int from, int total) {
	total = min(total ? total : 1000, bm->MaxMips());
	for (int m = max(from, 1); m < total; ++m)
		BoxFilter(bm->Mip(m - 1), bm->Mip(m), false);
}

typedef _bitmap<32>					 bitmap;
typedef _bitmap<64>					 bitmap64;
typedef pair<ISO_ptr<bitmap>, float> bitmap_frame;
typedef ISO_openarray<bitmap_frame>  bitmap_anim;

template<int B> void SetBitmapFlags(_bitmap<B>* bm) { bm->SetFlags(GetBitmapFlags()); }

//-----------------------------------------------------------------------------
//	HDRbitmap
//-----------------------------------------------------------------------------
iso_export uint16 Scan(const block<HDRpixel, 1>& block, uint16 flags);
iso_export uint16 Scan(const block<HDRpixel, 2>& block, uint16 flags);

template<int B> class _HDRbitmap {
	friend struct ISO::def<_HDRbitmap>;
	template<int B2> friend class iso::_HDRbitmap;
	uint16					   width, height, depth;
	uint8					   flags, mips;
	ISO_openarray<HDRpixel, B> texels;

public:
	_HDRbitmap() {}
	_HDRbitmap(int _width, int _height, uint32 _flags = 0, int _depth = 1) { Create(_width, _height, _flags, _depth); }
	template<int B2> _HDRbitmap(const _HDRbitmap<B2>& b)	: width(b.width), height(b.height), depth(b.depth), flags(b.flags), mips(b.mips), texels(b.texels) {}
	template<int B2> _HDRbitmap(_HDRbitmap<B2>&& b)			: width(b.width), height(b.height), depth(b.depth), flags(b.flags), mips(b.mips), texels(move(b.texels)) {}

	iso_export HDRpixel*	Create(int _width, int _height, uint32 _flags = 0, int _depth = 1);

	void		SetDepth(int _depth) {
		height = Height() / _depth;
		depth  = _depth;
	}
	void		SetMips(int _mips) {
		mips = clamp(_mips - 1, 0, BMF2_MIPMASK);
	}

	void		_Scan();
	int			Scan() {
		 if (!(mips & BMF2_SCANNED))
			 _Scan();
		 return Flags();
	}
	int			Flags()					const	{ return flags | ((mips & ~BMF2_MIPMASK) << 4); }
	void		SetFlags(uint32 i)				{ flags |= i; }
	void		ClearFlags(uint32 i)			{ flags &= ~i; }

	int			Width()					const	{ return width; }
	int			Height()				const	{ return height * depth; }
	int			Depth()					const	{ return depth; }
	int			Mips()					const	{ return mips & BMF2_MIPMASK; }
	int			MaxMips()				const	{ return iso::MaxMips(BaseWidth(), BaseHeight()); }
	HDRpixel*	ScanLine(int y)			const	{ return texels + y * width; }

	int			BaseWidth()				const	{ return mips ? width / 2 : width; }
	int			BaseHeight()			const	{ return height; }

	bool		IsCube()				const	{ return !!(flags & BMF_CUBE); }
	bool		IsVolume()				const	{ return !!(flags & BMF_VOLUME); }
	bool		IsIntensity()					{ return !!(Scan() & BMF_GREY); }
	bool		HasAlpha()						{ return !!(Scan() & BMF_ALPHA); }
	TextureType Type()					const	{ return IsVolume() ? TT_VOLUME : IsCube() ? TT_CUBE : Depth() == 1 ? TT_NORMAL : TT_ARRAY; }

	operator block<HDRpixel, 2>()		const	{ return make_block(unconst(texels.begin()), width, Height()); }
	block<HDRpixel, 2>	All()			const	{ return *this; }
	block<HDRpixel, 2>	Mip(int n)		const	{ return GetMip(All(), n); }
	block<HDRpixel, 2>	Base()			const	{ return Mips() ? Mip(0) : All(); }
	block<HDRpixel, 2>	Slice(int n)	const	{ return Block(0, BaseHeight() * n, width, BaseHeight()); }
	block<HDRpixel, 3>	All3D()			const	{ return make_block(unconst(texels.begin()), width, BaseHeight(), depth); }

	block<HDRpixel, 2>	Block(int x, int y, int w, int h) const {
		return All().template sub<1>(x, w).template sub<2>(y, h);
	}
	block<HDRpixel, 3>	Mip3D(int n) const {
		block<HDRpixel, 3> b = All3D();
		if (IsVolume())
			return GetMip(b, n);
		_GetMip(b.template get<2>(), n);
		return b;
	}
	block<HDRpixel, 3> MipArray(int n) const {
		block<HDRpixel, 3> b = All3D();
		_GetMip(b.template get<2>(), n);
		return b;
	}
};

template<int B> HDRpixel* _HDRbitmap<B>::Create(int _width, int _height, uint32 _flags, int _depth) {
	if (_flags & BMF_MIPS) {
		mips = iso::MaxMips(_width, _height);
		_width *= 2;
	} else {
		mips = 0;
	}
	width  = _width;
	height = _height / _depth;
	depth  = _depth;
	flags  = _flags;

	HDRpixel* p = texels.Create(_width * _height, false);
	if (_flags & BMF_CLEAR)
		memset(p, 0, _width * _height * sizeof(HDRpixel));
	return p;
}

template<int B> void _HDRbitmap<B>::_Scan() {
	uint16 f = BMF_GREY;
	if (int m = Mips()) {
		for (int i = 0; i < m; i++)
			f = iso::Scan(Mip(i), f);
	} else {
		f = iso::Scan(All(), f);
	}
	mips = (mips & BMF2_MIPMASK) | ((f | BMF_SCANNED) >> 4);
}

typedef _HDRbitmap<32> HDRbitmap;
typedef _HDRbitmap<64> HDRbitmap64;

struct bitmap2 : ISO_ptr<void> {
	enum TYPE { NONE, BITMAP, HDRBITMAP, VBITMAP };
	bitmap2(const ISO_ptr<void>& p) : ISO_ptr<void>(p) {}
	TYPE BitmapType() const { return !*this ? NONE : IsType<bitmap>() ? BITMAP : IsType<HDRbitmap>() ? HDRBITMAP : VBITMAP; }
};

ISO_ptr<bitmap>	HDR2bitmap(HDRbitmap& hdr);
ISO_ptr<HDRbitmap> bitmap2HDR(bitmap& bm);

//-----------------------------------------------------------------------------
//	vbitmap
//-----------------------------------------------------------------------------

#define CHANS(R, G, B, A) uint32(R | (G << 8) | (B << 16) | (A << 24))

struct channels {
	static const uint32
		SIZE_MASK	= 0x3f,			// per channel
		COMPRESSED	= 0x80,			// per channel
		R_COMP		= 0x00000080, RG_COMP = 0x00008080, RGB_COMP = 0x00808080, ALL_COMP = 0x80808080,
		// special - apply for all channels
		GREY		= 0x00000040,
		SIGNED		= 0x00004000,
		UNNORM		= 0x00400000,	// not 0..1 (or -1..+1)
		FLOAT		= 0x40000000;

	union {
		uint8 array[4];
		struct {
			uint8 r, g, b, a;
		};
		uint32 i;
	};
	constexpr channels(uint32 i = 0) : i(i) {}
	constexpr channels(uint8 r, uint8 g, uint8 b, uint8 a, uint32 f = 0) : i(r | (g << 8) | (b << 16) | (a << 24) | f) {}
	constexpr operator uint32() const { return i; }
	constexpr int	bitdepth(int i) const { return array[i] & SIZE_MASK; }
};

struct vbitmap_format : channels {
	static const uint32
		BC1		= CHANS(5, 6, 5, 1) | ALL_COMP,	DXT1 = BC1,
		BC2		= CHANS(5, 6, 5, 4) | RGB_COMP, DXT3 = BC2,
		BC3		= CHANS(5, 6, 5, 8) | ALL_COMP, DXT5 = BC3,
		BC4		= CHANS(8, 0, 0, 0) | R_COMP,
		BC5		= CHANS(8, 8, 0, 0) | RG_COMP,
		BC6U	= CHANS(16, 16, 16, 0) | RGB_COMP | FLOAT,
		BC6S	= CHANS(16, 16, 16, 0) | RGB_COMP | FLOAT | SIGNED,
		BC7		= CHANS(8, 8, 8, 8) | ALL_COMP;

	template<typename T> struct get_s;
	using channels::channels;

	constexpr uint8	channel_bits(int i)	const { return array[i] & SIZE_MASK; }
	constexpr uint8	max_channel_bits()	const { return max(channel_bits(0), channel_bits(1), channel_bits(2), channel_bits(3)); }
	constexpr bool	is_compressed()		const { return !!(i & ALL_COMP); }
	constexpr bool	is_hdr()			const { return (i & FLOAT) || max_channel_bits() > 8; }

	uint32 bits() const {
		if (is_compressed())
			return i == DXT1 ? 64 : 128;
		uint32 t = i & 0x3f3f3f3f;
		t += t >> 16;
		return (t + (t >> 8)) & 0xff;
	}
	
	uint32 channels() const {
		uint32 t = ((0x80808080 - (i & 0x3f3f3f3f)) & 0x40404040) >> 6;
		t		 = t + (t >> 16);
		return (t + (t >> 8)) & 7;
	}

	template<typename T> static inline vbitmap_format get()			{ return get_s<T>::get(); }
	template<typename T> bool						  is() const	{ return i == get<T>().i; }
};

template<> inline vbitmap_format vbitmap_format::get<ISO_rgba>()	{ return vbitmap_format(8, 8, 8, 8); }
template<> inline vbitmap_format vbitmap_format::get<uint8>()		{ return vbitmap_format(); }
template<> inline vbitmap_format vbitmap_format::get<HDRpixel>()	{ return vbitmap_format(32, 32, 32, 32, FLOAT); }
template<> inline vbitmap_format vbitmap_format::get<YUYV>()		{ return vbitmap_format(16 | COMPRESSED, 8, 8, 0); }

template<typename FORMAT> struct vbitmap_format::get_s<Texel<FORMAT> > {
	static inline vbitmap_format get() { return vbitmap_format(FORMAT::RBITS, FORMAT::GBITS, FORMAT::BBITS, FORMAT::ABITS); }
};

struct vbitmap_loc;

struct vbitmap {
	bool (*vget)(void* data, const vbitmap_loc& in, vbitmap_format fmt, void* dest, uint32 stride, uint32 width, uint32 height);
	void* (*vget_raw)(void* data, uint32 plane, vbitmap_format* fmt, uint32* stride, uint32* width, uint32* height);
	uint16		   width, height, depth;
	uint8		   mips, flags;
	vbitmap_format format;

	template<typename T> struct thunk {
		static bool  get(void* data, const vbitmap_loc& in, vbitmap_format fmt, void* dest, uint32 stride, uint32 width, uint32 height) { return ((T*)data)->get(in, fmt, dest, stride, width, height); }
		static void* get_raw(void* data, uint32 plane, vbitmap_format* fmt, uint32* stride, uint32* width, uint32* height) { return ((T*)data)->get_raw(plane, fmt, stride, width, height); }
	};
	template<typename T> vbitmap(T* t, vbitmap_format format = 0) : vget(thunk<T>::get), vget_raw(thunk<T>::get_raw), width(0), height(0), depth(1), mips(0), flags(0), format(format) {}
	template<typename T> vbitmap(T* t, vbitmap_format format, uint32 width, uint32 height, uint32 depth = 1) : vget(thunk<T>::get), vget_raw(thunk<T>::get_raw), width(width), height(height), depth(depth), mips(0), flags(0), format(format) {}

	void		SetDepth(int _depth) { depth = _depth; }
	void		SetMips(int _mips) { mips = _mips; }

	int			Flags()		const	{ return flags | ((mips & ~BMF2_MIPMASK) << 4); }
	void		SetFlags(uint32 i)	{ flags |= i; }
	void		ClearFlags(uint32 i) { flags &= ~i; }

	int			Width()		const	{ return width; }
	int			Height()	const	{ return height; }
	int			Depth()		const	{ return depth; }
	int			Mips()		const	{ return mips & BMF2_MIPMASK; }
	bool		IsCube()	const	{ return !!(flags & BMF_CUBE); }
	bool		IsVolume()	const	{ return !!(flags & BMF_VOLUME); }
	TextureType Type()		const	{ return IsVolume() ? TT_VOLUME : IsCube() ? TT_CUBE : Depth() == 1 ? TT_NORMAL : TT_ARRAY; }

	void* GetRaw(uint32 plane, vbitmap_format* fmt, uint32* stride, uint32* width, uint32* height) { return (*vget_raw)(this, plane, fmt, stride, width, height); }
};

struct vbitmap_loc {
	vbitmap& v;
	uint32   x, y, z, m;

	vbitmap_loc(vbitmap& v) : v(v), x(0), y(0), z(0), m(0) {}
	vbitmap_loc set_x(int i)	{ x += i; return *this; }
	vbitmap_loc set_y(int i)	{ y += i; return *this; }
	vbitmap_loc set_z(int i)	{ z += i; return *this; }
	vbitmap_loc set_mip(int i)	{ m += i; return *this; }

	template<typename T> bool get(const block<T, 2>& out) const {
		return v.vget(&v, *this, vbitmap_format::get<T>(), out[0].begin(), out.template pitch<2>(), out.template size<1>(), out.template size<2>());
	}
};

ISO_ptr<vbitmap> MakeVBitmap(ISO_ptr<void> p, vbitmap_format fmt);

}  // namespace iso

namespace ISO {
ISO_DEFUSERCOMPV(ISO_rgba, r, g, b, a);
ISO_DEFUSERCOMPV(bitmap, width, height, depth, flags, mips, clut, texels);
ISO_DEFUSERCOMPV(bitmap64, width, height, depth, flags, mips, clut, texels);

ISO_DEFUSER(HDRpixel, float[4]);
ISO_DEFUSERCOMPV(HDRbitmap, width, height, depth, flags, mips, texels);
ISO_DEFUSERCOMPV(HDRbitmap64, width, height, depth, flags, mips, texels);

ISO_DEFUSER(bitmap2, ISO_ptr<void>);

ISO_DEFSAME(vbitmap_format, xint32);
ISO_DEFUSERCOMPV(vbitmap, width, height, depth, mips, flags, format);

}

#endif
