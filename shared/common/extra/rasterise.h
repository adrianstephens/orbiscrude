#include "base/block.h"
#include "maths/geometry.h"

using namespace iso;

template<typename D, typename S> void RasteriseSlab(D &&dst, float y0, float y1, float x0, float x1, float dx0, float dx1, S &&src) {
	if (x0 > x1 || (x0 == x1 && dx0 > dx1)) {
		swap(x0, x1);
		swap(dx0, dx1);
	}

	float	yf = ceil(y0);
	x0 += (yf - y0) * dx0;
	x1 += (yf - y0) * dx1;

	for (int y = int(yf); y < y1; ++y, x0 += dx0, x1 += dx1) {
		for (int x = ceil(x0); x < x1; ++x)
			dst(x, y) = src(x, y);
	}
}

template<typename D, typename S> void RasteriseTriangle(D&& dst, const triangle& tri, S &&src) {
	auto	v		= tri.corners();
	int		i0		= v[0].v.y < v[1].v.y ? 0 : 1;
	if (v[2].v.y < v[i0].v.y)
		i0 = 2;

	int		i1 = i0 == 2 ? 0 : i0 + 1;
	int		i2 = i0 == 0 ? 2 : i0 - 1;

	float2	d0 = v[i1] - v[i0];
	float2	d1 = v[i2] - v[i0];
	float2	d2 = v[i2] - v[i1];
	float	dx0 = d0.x / d0.y;
	float	dx1 = d1.x / d1.y;
	float	dx2 = d2.x / d2.y;
	float	x0 = v[i0].v.x;
	float	y0 = v[i0].v.y;
	float	y1 = v[i1].v.y;
	float	y2 = v[i2].v.y;

	if (y1 < y2) {
		//rasterize from d0 to d1
		RasteriseSlab(dst, y0, y1, x0, x0, dx0, dx1, src);
		//rasterize from d1 to d2
		RasteriseSlab(dst, y1, y2, x0 + (y1 - y0) * dx1, v[i1].v.x, dx1, dx2, src);
	} else {
		//rasterize from d0 to d2
		RasteriseSlab(dst, y0, y2, x0, x0, dx0, dx1, src);
		//rasterize from d2 to d1
		RasteriseSlab(dst, y2, y1, x0 + (y2 - y0) * dx0, v[i2].v.x, dx0, dx2, src);
	}
}

template<typename D, typename S> void RasteriseCircle(D&& dst, const circle& c, S&& src) {
	auto	box = c.get_box();
	for (int y = ceil(box.a.v.y), y1 = floor(box.b.v.y); y < y1; ++y) {
		float	w = sqrt(c.radius2() - square(y));
		for (int x = ceil(c.centre().v.x - w), x1 = floor(c.centre().v.x - w); x < x1; ++x)
			dst(x, y) = src(x, y);
	}
}


template<typename D> auto RasteriseToBlock(const block<D, 2> &dst) {
	return [&](int x, int y)->D& { return dst[y][x]; };
}

template<typename S> auto RasteriseFromSolid(const S& col) {
	return [&](int x, int y) { return col; };
}

template<typename S> auto RasteriseFromTex(const block<S, 2> &tex, const float2x3 &uvmat) {
	return [&tex, uvmat](int x, int y) { return linear_sample(tex, uvmat * position2(x, y)); };
}

template<typename D, typename S> void RasteriseTriangle(const block<D, 2> &dst, const triangle &tri, const block<S, 2> &tex, const triangle &uvs) {
	RasteriseTriangle(RasteriseToBlock(dst), tri, RasteriseFromTex(tex, uvs.matrix() * tri.inv_matrix()));
}