#include "base/vector.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "filetypes/bitmap/bitmap.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Distance Fields
//-----------------------------------------------------------------------------

struct DistPoint {
	int x, y;
	DistPoint(int x = 0x7fff, int y = 0x7fff) : x(x), y(y)	{}
	int		DistSq()			const	{ return square(x) + square(y); }
	float2	Deriv()				const	{ return {float(x), float(y)}; }
};

void Compare(block<DistPoint, 2> &g, DistPoint &p, int x, int y, int dx, int dy) {
	DistPoint other = g[y + dy][x + dx];
	other.x += dx;
	other.y += dy;

	if (other.DistSq() < p.DistSq())
		p = other;
}

void GenerateSDF(block<DistPoint, 2> &g) {
	int			w = g.size<1>(), h = g.size<2>();

	// Pass 0
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			DistPoint &p = g[y][x];
			if (x > 0)
				Compare(g, p, x, y, -1,  0);
			if (y > 0) {
				Compare(g, p, x, y,  0, -1);
				if (x > 0)
					Compare(g, p, x, y, -1, -1);
				if (x < w - 1)
					Compare(g, p, x, y,  1, -1);
			}
		}

		for (int x = w - 1; --x; )
			Compare(g, g[y][x], x, y, 1, 0);
	}

	// Pass 1
	for (int y = h; y--;) {
		for (int x = w; x--;) {
			DistPoint &p = g[y][x];
			if (x < w - 1)
				Compare(g, p, x, y,  1,  0);
			if (y < h - 1) {
				Compare(g, p, x, y,  0,  1);
				if (x < w - 1)
					Compare(g, p, x, y,  1,  1);
				if (x > 0)
					Compare(g, p, x, y, -1,  1);
			}
		}

		for (int x = 1; x < w - 1; x++)
			Compare(g, g[y][x], x, y, -1, 0);
	}
}

void MakeDistanceField(const block<ISO_rgba, 2> &drect, const block<uint8, 2> &srect, int xoffset, int yoffset, int scale, int samples) {
	int	samples2= samples * scale;
	int	swidth	= srect.size<1>();
	int	sheight	= srect.size<2>();
	int	width	= swidth	+ samples2 * 2;
	int	height	= sheight	+ samples2 * 2;

	auto grid1 = make_auto_block<DistPoint>(width, height);
	auto grid2 = make_auto_block<DistPoint>(width, height);

	fill(grid2.sub<1>(0, width).sub<2>(0, samples2),						DistPoint(0, 0));
	fill(grid2.sub<1>(0, width).sub<2>(sheight + samples2, samples2),		DistPoint(0, 0));
	fill(grid2.sub<1>(0, samples2).sub<2>(samples2, sheight),				DistPoint(0, 0));
	fill(grid2.sub<1>(swidth + samples2, samples).sub<2>(samples2, sheight), DistPoint(0, 0));

	for (int y = 0; y < sheight; y++) {
		uint8		*src	= srect[y].begin();
		DistPoint	*dst1	= grid1[y + samples2].begin() + samples2;
		DistPoint	*dst2	= grid2[y + samples2].begin() + samples2;
		for (int x = swidth; x--; src++, dst1++, dst2++)
			*(*src ? dst1 : dst2) = DistPoint(0, 0);
	}

	GenerateSDF(grid1);
	GenerateSDF(grid2);

#if 1
	for (int y = 0, ys = scale / 2 + yoffset; y < drect.size<2>(); y++, ys += scale) {
		if (ys < 0)
			continue;
		if (ys >= height)
			break;
		block<DistPoint, 1>	src1	= grid1[ys];
		block<DistPoint, 1>	src2	= grid2[ys];
		block<ISO_rgba, 1>	dst		= drect[y];
		for (int x = 0, xs = scale / 2 + xoffset; xs < width; x++, xs += scale) {
			if (xs < 0)
				continue;
			int		d1	= src1[xs].DistSq(), d2 = src2[xs].DistSq();
			float	d	= (d1 ? -sqrt(d1) : sqrt(d2)) / (sqrt2 * scale * samples);
			dst[x]		= ISO_rgba(255,255,255, clamp(int((1 + d) * 128), 0, 255));
		}
	}

#else

	Tempblock<float, 2> grid3(width, height);

	for (int y = 0; y < height; y++) {
		DistPoint	*src1	= grid1[y];
		DistPoint	*src2	= grid2[y];
		float		*dst	= grid3[y];
		for (int x = width; x--; dst++, src1++, src2++) {
			int	d1 = src1->DistSq(), d2 = src2->DistSq();
//			*dst = sqrt(src1->DistSq()) / samples2;
			*dst = (d1 ? -sqrt(d1) : sqrt(d2)) / samples2;
		}
	}

	Tempblock<float, 2> grid4(dwidth, dheight);
	resample(grid4, grid3);

	for (int y = 0; y < dheight; y++) {
		float		*src	= grid4[y];
		ISO_rgba	*dst	= drect[y - yoffset / scale] - xoffset / scale;
		for (int x = dwidth; x--; dst++, src++)
			*dst = ISO_rgba(255,255,255, clamp(int((1 + *src) * 128), 0, 255));
	}
#endif
}

int RGBdist(const ISO_rgba &c1, const ISO_rgba &c2) {
	return square(c1.r - c2.r) + square(c1.g - c2.g) + square(c1.b - c2.b);
}

ISO_ptr<bitmap> DistanceBitmap2(ISO_ptr<bitmap> bm, float scale, float fdist)
{
	if (scale == 0)	scale	= 16;
	if (fdist == 0)	fdist	= 4;
	fdist *= scale;

	int		dist	= int(fdist);
	int		width	= bm->Width(), height = bm->Height();
	int		width1	= width + dist * 2, height1 = height + dist * 2;

	auto	grid1	= make_auto_block<DistPoint>(width1, height1);
	auto	grid2	= make_auto_block<DistPoint>(width1, height1);

	for (int y = 0; y < height; y++) {
		DistPoint	*dst1	= grid1[y + dist].begin() + dist;
		DistPoint	*dst2	= grid2[y + dist].begin() + dist;
		ISO_rgba	*src	= bm->ScanLine(y);
		for (int x = width; x--; src++, dst1++, dst2++)
			*(src->a ? dst1 : dst2) = DistPoint(0, 0);
	}

	GenerateSDF(grid1);
	GenerateSDF(grid2);

	auto	gridc1	= make_auto_block<DistPoint>(width1, height1);
	auto	gridc2	= make_auto_block<DistPoint>(width1, height1);
	auto	diffs	= make_auto_block<int>(width1, height1);

	for (int y = 0; y < height - 1; y++) {
		int			*diff	= diffs[y + dist].begin() + dist;
		DistPoint	*dst1	= gridc1[y + dist].begin() + dist;
		DistPoint	*dst2	= gridc2[y + dist].begin() + dist;
		ISO_rgba	*src1	= bm->ScanLine(y);
		ISO_rgba	*src2	= bm->ScanLine(y + 1);
		for (int x = 0; x < width - 1; x++) {
			int			d	= max(RGBdist(src1[x], src1[x + 1]), RGBdist(src1[x], src2[x]));
			diff[x] = d;
			(d > 0x100 ? dst1 : dst2)[x] = DistPoint(0, 0);
		}
		diff[width - 1] = 0;
		dst2[width - 1] = DistPoint(0, 0);
	}
	for (int x = 0; x < width; x++)
		gridc2[height - 1 + dist][x + dist] = DistPoint(0, 0);

	ISO_ptr<bitmap>	bm1(NULL);
	bm1->Create(width1, height1);
	for (int y = 0; y < height1; y++) {
		ISO_rgba	*dst	= bm1->ScanLine(y);
		int			*src	= diffs[y].begin();
		for (int x = 0; x < width1; x++)
			dst[x] = src[x] > 0x100 ? ISO_rgba(255,255,255) : ISO_rgba(0,0,0);
	}
//	return bm1;

	GenerateSDF(gridc1);
	GenerateSDF(gridc2);

	ISO_ptr<bitmap>	bm3(NULL);
	bm3->Create(width1, height1 * 2, bm->Flags() | BMF_NOMIP | BMF_NOCOMPRESS);

	for (int y = 0; y < height1; y++) {
		ISO_rgba	*dst1	= bm3->ScanLine(y);
		ISO_rgba	*dst2	= bm3->ScanLine(y + height1);
		DistPoint	*src1	= grid1[y].begin();
		DistPoint	*src2	= grid2[y].begin();
		DistPoint	*srcc1	= gridc1[y].begin();
		DistPoint	*srcc2	= gridc2[y].begin();
		for (int x = 0; x < width1; x++) {
			DistPoint	d	= srcc1[x];
			float		d0	= d.DistSq(), dx = gridc1[y][x + 1].DistSq() - d0, dy = gridc1[y + 1][x].DistSq() - d0;
			ISO_rgba	c1	= bm->ScanLine(clamp(y - dist, 0, height - 1))[clamp(x - dist, 0, width - 1)];
			ISO_rgba	c2	= bm->ScanLine(clamp(y + d.y, 0, height - 1))[clamp(x + d.x, 0, width - 1)];
			int			am	= 128;
			if (dx < 0) {
				swap(c1, c2);
				am	= -128;
			}
			c1.a = clamp(int(sqrt(src2[x].DistSq()) - sqrt(src1[x].DistSq()) / dist * 128 + 128), 0, 255);
			c2.a = clamp(int(sqrt(srcc1[x].DistSq()) / dist * 128 + 128), 0, 255);
//			c2.a = clamp(int(float(d.x) / dist * 128 + 128), 0, 255);
			dst1[x] = c1;
			dst2[x] = c2;
		}
	}
	return bm3;

	ISO_ptr<bitmap>	bm2(NULL);
	int		width2	= width1 / scale,	height2	= height1 / scale;
	bm2->Create(width2, height2 * 2, bm->Flags() | BMF_NOMIP | BMF_NOCOMPRESS);

#if 0
	HDRpixel	*hpix = new HDRpixel[width2 * height2];
	block<HDRpixel, 2>	hblock(hpix, width2, width2, height2);
	hblock.Fill(HDRpixel(0,0,0,0));
	DownsampleT(hblock, block<ISO_rgba, 2>(*bm3));
	copy(block<ISO_rgba, 2>(*bm2), hblock);
#else
	for (int y = 0; y < height2 * 2; y++) {
		ISO_rgba	*src	= bm3->ScanLine(y * scale);
		ISO_rgba	*dst	= bm2->ScanLine(y);
		for (int x = 0; x < width2; x++)
			dst[x] = src[int((x + 0.5f)* scale)];
	}
#endif

	return bm2;


	for (int y = 0; y < height2; y++) {
		ISO_rgba	*dst	= bm2->ScanLine(y);
		DistPoint	*src1	= grid1[y + dist].begin() + dist;
		DistPoint	*src2	= grid2[y + dist].begin() + dist;
		for (int x = width2; x--; dst++, src1++, src2++)
			dst->a = clamp(int(sqrt(src2->DistSq()) - sqrt(src1->DistSq()) / dist * 128 + 128), 0, 255);
	}

	return bm2;
}

float SolveEikonal2D(float h, float v) {
	//solve eikonal equation in 2D if or can, or revert to 1D if |h-v| >= 1
	if (abs(h - v) < 1) {
		float sum = h + v;
		float dist = sum * sum - 2 * (h * h + v * v - 1);
		return (sum + sqrt(dist)) * half;
	}
	//1D
	return min(h, v) + 1;
}

ISO_ptr<bitmap> DistanceBitmap(ISO_ptr<bitmap> bm, float scale, float dist) {
	if (!bm)
		return bm;

	if (scale == 0)
		scale = 16;

	if (dist == 0)
		dist = 4;

	int			width	= bm->Width(),		height	= bm->Height();
	int			width2	= width / scale,	height2	= height / scale;
	ISO_ptr<bitmap>	bm2(NULL);
	bm2->Create(width2, height2, bm->Flags() | BMF_NOMIP | BMF_NOCOMPRESS);

	dist *= scale;

#if 1
	auto	grid1	= make_auto_block<DistPoint>(width, height);
	auto	grid2	= make_auto_block<DistPoint>(width, height);

	for (int y = 0; y < height; y++) {
		ISO_rgba	*src	= bm->ScanLine(y);
		DistPoint	*dst1	= grid1[y].begin();
		DistPoint	*dst2	= grid2[y].begin();
		for (int x = width; x--; src++, dst1++, dst2++)
			*(src->a ? dst1 : dst2) = DistPoint(0, 0);
	}

	GenerateSDF(grid1);
	GenerateSDF(grid2);

	float		rdist	= reciprocal(dist);
	for (int y = 0, ys = scale / 2; ys < height; y++, ys += scale) {
		ISO_rgba	*src	= bm->ScanLine(ys);
		DistPoint	*src1	= grid1[ys].begin();
		DistPoint	*src2	= grid2[ys].begin();
		ISO_rgba	*dst	= bm2->ScanLine(y);
		for (int x = 0, xs = scale / 2; xs < width; x++, xs += scale) {
			int		d1	= src1[xs].DistSq(), d2 = max(src2[xs].DistSq() - 1, 0);
			float	d	= (d1 ? -sqrt(d1 * 0.5f) : sqrt(d2 * 0.5f)) * rdist;
#if 0
			auto	dxy		= d1 ? -src1[xs].Deriv() : src2[xs].Deriv();
			//if (len2(dxy) <= one) {
				for (int i = 0; i < 9; i++) {
					int	dx = i % 3 - 1, dy = i / 3 - 1;
					if (grid1[ys+dy][xs+dx].DistSq())
						dxy -= grid1[ys+dy][xs+dx].Deriv();
					else
						dxy += grid2[ys+dy][xs+dx].Deriv();
				}
			//}
			auto	dxyi	= clamp(to<int>(((normalise(dxy) + 1) * 128)), 0, 255);
			dst[x].r	= clamp(int((1 + d) * 128), 0, 255);
			dst[x].g	= dxyi.x;
			dst[x].b	= dxyi.y;
			dst[x].a	= 255;
#else
			dst[x]		= src[xs];
			dst[x].a	= clamp(int((1 + d) * 128), 0, 255);
#endif
		}
	}

#else

	for (int y = 0; y < height2; y++ ) {
		ISO_rgba	*dest	= bm2->ScanLine(y);
		for (int x = 0; x < width2; x++, dest++) {
			int		x1	= x * scale,//x - dist,
					y1	= y * scale;//y - dist;
			ISO_rgba c	= x1 >= 0 && y1 >= 0 && x1 < width && y1 < height ? bm->ScanLine(y1)[x1] : ISO_rgba(0,0,0,0);
			int		set	= !!c.a;
			float	r	= 1;
			for (int d = 1; d < dist && r > d / (sqrt2 * dist); d++) {
				for (int t0 = 0; t0 <= d * 2; t0++) {
					int	t = t0 & 1 ? -(t0 + 1) / 2 : t0 / 2;
					int	x2, y2;
					int	s0, s1, s2, s3;

					if ((x2 = x1 + t) >= 0 && x2 < width) {
						s0 = (y2 = y1 + d) >= 0 && y2 < height ? !!bm->ScanLine(y2)[x2].a : 0;
						s1 = (y2 = y1 - d) >= 0 && y2 < height ? !!bm->ScanLine(y2)[x2].a : 0;
					} else {
						s0 = s1 = 0;
					}

					if ((y2 = y1 + t) >= 0 && y2 < height) {
						s2 = (x2 = x1 + d) >= 0 && x2 < width ? !!bm->ScanLine(y2)[x2].a : 0;
						s3 = (x2 = x1 - d) >= 0 && x2 < width ? !!bm->ScanLine(y2)[x2].a : 0;
					} else {
						s2 = s3 = 0;
					}

					if (s0 != set || s1 != set || s2 != set || s3 != set) {
						if (!set)
							c = s0 || s1
								? bm->ScanLine(s0 ? y1 + d : y1 - d)[x1 + t]
								: bm->ScanLine(y1 + t)[s2 ? x1 + d : x1 - d];
						r = min(r, sqrt(t * t + d * d) / (sqrt2 * dist));
						break;
					}
				}
			}
			c.a = 255 * (set ? 1 + r : 1 - r) / 2;
			*dest = c;
//			dest->r = dest->g = dest->b = 255;
//			dest->a = 255 * (set ? (r + 1) / 2 : (1 - r) / 2);
		}
	}
#endif
	return bm2;
}

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation(DistanceBitmap),
	ISO_get_operation(DistanceBitmap2)
);
