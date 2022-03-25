#ifndef PVRTC_H
#define PVRTC_H

#include "base/defs.h"
#include "base/bits.h"
#include "base/interval.h"

namespace iso {

struct pixel8 {
	uint8 r,g,b,a;
	pixel8() {}
	pixel8(uint8 _r, uint8 _g, uint8 _b, uint8 _a) : r(_r), g(_g), b(_b), a(_a) {}
	void	operator=(struct pixel32 &p);
};

struct pixel32 {
	int r,g,b,a;
	pixel32()	{}
	pixel32(int32 _r, int32 _g, int32 _b, int32 _a) : r(_r), g(_g), b(_b), a(_a) {}
	pixel32(const pixel8 &p) : r(p.r), g(p.g), b(p.b), a(p.a)	{}
	pixel32& operator+=(const pixel32 &y) { r += y.r; g += y.g; b += y.b; a += y.a; return *this; }
	pixel32& operator-=(const pixel32 &y) { r -= y.r; g -= y.g; b -= y.b; a -= y.a; return *this; }

	pixel32& operator*=(int y) { r *= y; g *= y; b *= y; a *= y; return *this; }
	pixel32& operator/=(int y) { r /= y; g /= y; b /= y; a /= y; return *this; }
};

force_inline pixel32 operator-(const pixel32 &x, const pixel32 &y) {
	return pixel32(x.r - y.r, x.g - y.g, x.b - y.b, x.a - y.a);
}
force_inline pixel32 operator+(const pixel32 &x, const pixel32 &y) {
	return pixel32(x.r + y.r, x.g + y.g, x.b + y.b, x.a + y.a);
}
force_inline pixel32 operator*(const pixel32 &x, int y) {
	return pixel32(x.r * y, x.g * y, x.b * y, x.a * y);
}
force_inline pixel32 operator/(const pixel32 &x, int y) {
	return pixel32(x.r / y, x.g / y, x.b / y, x.a / y);
}
force_inline pixel32 abs(const pixel32 &x) {
	return pixel32(abs(x.r), abs(x.g), abs(x.b), abs(x.a));
}
force_inline int dotp(const pixel32 &x, const pixel32 &y) {
	return x.r * y.r + x.g * y.g + x.b * y.b + x.a * y.a;
}
force_inline pixel32 clamp(const pixel32 &x) {
	return pixel32(clamp(x.r, 0, 255), clamp(x.g, 0, 255), clamp(x.b, 0, 255), clamp(x.a, 0, 255));
}

force_inline void pixel8::operator=(pixel32 &p)		{ r = p.r; g = p.g; b = p.b; a = p.a; }
force_inline pixel8 clamped_add(const pixel32 &p, int i)	{
	return pixel8(clamp(p.r + i, 0, 255), clamp(p.g + i, 0, 255), clamp(p.b + i, 0, 255), clamp(p.a + i, 0, 255));
}

struct PVRTCrec {
	union {
		uint32 mod_data;
	};
	union {
		uint32 col_data;
		struct {
			uint32	:1, b1 : 4, g1 : 5, r1 : 5, has_a1 : 1, b2 : 5, g2 : 5, r2 : 5, has_a2 : 1;
		} col5;
		struct {
			uint32	:1, b1 : 3, g1 : 4, r1 : 4, a1 : 3, : 1, b2 : 4, g2 : 4, r2 : 4, a2 : 7, : 1;
		} col4;
	};

	pixel8 ColourA() const {
		return col_data & 0x8000 ? pixel8(
			(col_data >> 10) & 0x1f,
			(col_data >>  5) & 0x1f,
			extend_bits<4,5>((col_data >> 1) & 0xf),
			0xf
		) : pixel8(
			extend_bits<4,5>((col_data >> 8) & 0xf),
			extend_bits<4,5>((col_data >> 4) & 0xf),
			extend_bits<3,5>((col_data >> 1) & 0x7),
			(col_data >> 11) & 0xe
		);
	}

	pixel8 ColourB() const {
		return col_data & 0x80000000 ? pixel8(
			(col_data >> 26) & 0x1f,
			(col_data >> 21) & 0x1f,
			(col_data >> 16) & 0x1f,
			0xf
		) : pixel8(
			extend_bits<4,5>((col_data >> 24) & 0xf),
			extend_bits<4,5>((col_data >> 20) & 0xf),
			extend_bits<4,5>((col_data >> 16) & 0xf),
			(col_data >> 27) & 0xe
		);
	}
};
int PVRTCDecompress(const PVRTCrec *srce, pixel8* dest, uint32 width, uint32 height, uint32 pitch, bool bpp2);
int PVRTCompress(const pixel8 *src, PVRTCrec *dest, uint32 width, uint32 height, uint32 pitch, bool bpp2);

} // namespace iso

#endif// PVRTC_H
