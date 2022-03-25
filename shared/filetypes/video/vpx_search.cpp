#include "vpx_encode.h"

namespace vp9 {

// Evaluates the Mahalanobis distance measure for the input CbCr values.
static int evaluate_color_difference(int cb, int cr, const int mean[2]) {
	static const int inv_cov[3]	= {4107, 1663 * 2, 2157};

	const int cb_diff		= (cb << 6) - mean[0];
	const int cr_diff		= (cr << 6) - mean[1];

	const int cb_diff_q2	= (cb_diff * cb_diff + (1 << 9)) >> 10;
	const int cbcr_diff_q2	= (cb_diff * cr_diff + (1 << 9)) >> 10;
	const int cr_diff_q2	= (cr_diff * cr_diff + (1 << 9)) >> 10;

	return inv_cov[0] * cb_diff_q2 + inv_cov[1] * cbcr_diff_q2 + inv_cov[2] * cr_diff_q2;
}

bool skin_pixel(const uint8 y, const uint8 cb, const uint8 cr) {
	static const int skin_mean[5][2]	= { {7463, 9614}, {6400, 10240}, {7040, 10240}, {8320, 9280}, {6800, 9614}};
	static const int skin_threshold[6]	= {1570636, 1400000, 800000, 800000, 800000, 800000};

	// Thresholds on luminance.
	static const int y_low = 40, y_high = 220;
	
	if (y < y_low || y > y_high)
		return 0;

#if 1
	return evaluate_color_difference(cb, cr, skin_mean[0]) < skin_threshold[0];
#else
	// Exit on grey.
	if (cb == 128 && cr == 128)
		return false;
	// Exit on very strong cb.
	if (cb > 150 && cr < 110)
		return false;
	// Exit on (another) low luminance threshold if either color is high.
	if (y < 50 && (cb > 140 || cr > 140))
		return false;

	for (int i = 0; i < 5; i++) {
		int	val = evaluate_color_difference(cb, cr, skin_mean[i]);
		if (val < skin_threshold[i + 1])
			return true;
		if (val > skin_threshold[i + 1] << 3)
			return false;
	}
	return false;
#endif
}

bool compute_skin_block(const Buffer2D &yb, const Buffer2D &ub, const Buffer2D &vb, int x, int y, int w, int h) {
	// Take center pixel in block to determine is_skin.
	y += h / 2;
	x += w / 2;
	return skin_pixel(
		yb.row(y)[x],
		ub.row(y / 2)[x / 2],
		vb.row(y / 2)[x / 2]
	);
}

// Applies a 1-D 2-tap bilinear filter to the source block in either horizontal or vertical direction to produce the filtered output block.
template<typename A, typename B> void bilinear_filter_block2d(const A *a, B *b, uint32 a_stride, int pixel_step, uint32 output_height, uint32 output_width, int offset) {
	int		f0 = (7 - offset), f1 = offset;
	for (int i = 0; i < output_height; ++i) {
		for (int j = 0; j < output_width; ++j)
			b[j] = round_pow2((int)a[j] * f0 + (int)a[j + pixel_step] * f1, 3);
		a += a_stride;
		b += output_width;
	}
}

// Sum the difference between every corresponding element of the buffers
uint32 sad(const uint8 *a, int a_stride, const uint8 *b, int b_stride, int width, int height) {
	uint32 sad = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++)
			sad += abs(a[x] - b[x]);
		a += a_stride;
		b += b_stride;
	}
	return sad;
}

 // Averages every corresponding element of the buffers and store the value in comp_pred. (pred and comp_pred are assumed to have stride = width)
void avg_pred(uint8 *comp_pred, const uint8 *pred, int width, int height, const uint8 *ref, int ref_stride) {
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++)
			comp_pred[j] = round_pow2(pred[j] + ref[j], 1);
		comp_pred	+= width;
		pred		+= width;
		ref			+= ref_stride;
	}
}
template<int W, int H> uint32 sad(const uint8 *a, int a_stride, const uint8 *b, int b_stride) {
	uint32 sad = 0;
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++)
			sad += abs(a[x] - b[x]);
		a += a_stride;
		b += b_stride;
	}
	return sad;
}
template<int W, int H> uint32 sad_avg(const uint8 *src, int src_stride, const uint8 *ref, int ref_stride, const uint8 *second_pred) {
	uint8 comp_pred[W * H];
	avg_pred(comp_pred, second_pred, m, n, ref, ref_stride);
	return sad(src, src_stride, comp_pred, m, m, n);
}
template<int W, int H, int D> uint32 sad(const uint8 *src, int src_stride, const uint8 *ref_array, int ref_stride, uint32 *sad_array) {
	for (int i = 0; i < D; ++i)
		sad_array[i] = sad<W, H>(src, src_stride, &ref_array[i], ref_stride);
}
template<int W, int H> uint32 sad4d(const uint8 *src, int src_stride, const uint8 *const ref_array[], int ref_stride, uint32 *sad_array) {
	for (int i = 0; i < 4; ++i)
		sad_array[i] = sad<W, H>(src, src_stride, ref_array[i], ref_stride);
}

void comp_avg_pred(uint8 *comp_pred, const uint8 *pred, int width, int height, const uint8 *ref, int ref_stride) {
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j)
			comp_pred[j] = round_pow2(pred[j] + ref[j], 1);
		comp_pred	+= width;
		pred		+= width;
		ref			+= ref_stride;
	}
}
uint32 variance(const uint8 *a, int  a_stride, const uint8 *b, int  b_stride, int  w, int  h) {
	uint32	sse = 0;
	for (int r = 0; r < h; r++) {
		for (int c = 0; c < w; c++)
			sse += square(a[c] - b[c]);
		a += a_stride;
		b += b_stride;
	}
	return sse;
}
uint32 variance(const uint8 *a, int  a_stride, const uint8 *b, int  b_stride, int  w, int  h, int *psum) {
	uint32	sum = 0;
	uint32	sse = 0;
	for (int r = 0; r < h; r++) {
		for (int c = 0; c < w; c++) {
			const int diff = a[c] - b[c];
			sum	+= diff;
			sse	+= diff * diff;
		}
		a += a_stride;
		b += b_stride;
	}
	*psum += sum;
	return sse;
}
template<int W, int H> uint32 _variance(const uint8 *a, int a_stride, const uint8 *b, int b_stride) {
	uint32	sse = 0;
	for (int r = 0; r < H; ++r) {
		for (int c = 0; c < W; ++c)
			sse += square(a[c] - b[c]);
		a += a_stride;
		b += b_stride;
	}
	return sse;
}
template<int W, int H> uint32 _variance(const uint8 *a, int a_stride, const uint8 *b, int b_stride, uint32 *psum) {
	uint32	sum = 0;
	uint32	sse = 0;
	for (int r = 0; r < H; ++r) {
		for (int c = 0; c < W; ++c) {
			const int diff = a[j] - b[j];
			sum += diff;
			sse += diff * diff;
		}
		a += a_stride;
		b += b_stride;
	}
	*psum += sum;
	return sse;
}
template<int W, int H> uint32 variance(const uint8 *a, int a_stride, const uint8 *b, int b_stride, uint32 *sse) {
	int		sum;
	*sse = variance<W, H>(a, a_stride, b, b_stride, &sum);
	return *sse - (((int64)sum * sum) / (W * H));
}
template<int W, int H> uint32 subpix_variance(const uint8 *a, int a_stride, int xoffset, int  yoffset, const uint8 *b, int b_stride, uint32 *sse) {
	uint16	temp1[(H + 1) * W];
	uint8	temp2[H * W];
	bilinear_filter_block2d(a, temp1, a_stride, 1, H + 1, W, xoffset);
	bilinear_filter_block2d(temp1, temp2, W, W, H, W, yoffset);
	return variance<W, H>(temp2, W, b, b_stride, sse);
}
template<int W, int H> uint32 subpix_avg_variance(const uint8 *a, int a_stride, int xoffset, int  yoffset, const uint8 *b, int b_stride, uint32 *sse, const uint8 *second_pred) {
	uint16	temp1[(H + 1) * W];
	uint8	temp2[H * W];
	uint8	temp3[H * W];
	bilinear_filter_block2d(a, temp1, a_stride, 1, H + 1, W, xoffset);
	bilinear_filter_block2d(temp1, temp2, W, W, H, W, yoffset);
	comp_avg_pred(temp3, second_pred, W, H, temp2, W);
	return variance<W, H>(temp3, W, b, b_stride, sse);
}

variance_funcs	vp9::variance_table[] = {
	{sad< 4, 4>,	sad_avg< 4, 4>,	variance< 4, 4>,	subpix_variance< 4, 4>,	subpix_avg_variance< 4, 4>,	sad< 4, 4, 3>,	sad< 4, 4, 8>,	sad4d< 4, 4>},	//BLOCK_4X4	
	{sad< 4, 8>,	sad_avg< 4, 8>,	variance< 4, 8>,	subpix_variance< 4, 8>,	subpix_avg_variance< 4, 8>,	sad< 4, 8, 3>,	sad< 4, 8, 8>,	sad4d< 4, 8>},	//BLOCK_4X8	
	{sad< 8, 4>,	sad_avg< 8, 4>,	variance< 8, 4>,	subpix_variance< 8, 4>,	subpix_avg_variance< 8, 4>,	sad< 8, 4, 3>,	sad< 8, 4, 8>,	sad4d< 8, 4>},	//BLOCK_8X4	
	{sad< 8, 8>,	sad_avg< 8, 8>,	variance< 8, 8>,	subpix_variance< 8, 8>,	subpix_avg_variance< 8, 8>,	sad< 8, 8, 3>,	sad< 8, 8, 8>,	sad4d< 8, 8>},	//BLOCK_8X8	
	{sad< 8,16>,	sad_avg< 8,16>,	variance< 8,16>,	subpix_variance< 8,16>,	subpix_avg_variance< 8,16>,	sad< 8,16, 3>,	sad< 8,16, 8>,	sad4d< 8,16>},	//BLOCK_8X16	
	{sad<16, 8>,	sad_avg<16, 8>,	variance<16, 8>,	subpix_variance<16, 8>,	subpix_avg_variance<16, 8>,	sad<16, 8, 3>,	sad<16, 8, 8>,	sad4d<16, 8>},	//BLOCK_16X8	
	{sad<16,16>,	sad_avg<16,16>,	variance<16,16>,	subpix_variance<16,16>,	subpix_avg_variance<16,16>,	sad<16,16, 3>,	sad<16,16, 8>,	sad4d<16,16>},	//BLOCK_16X16	
	{sad<16,32>,	sad_avg<16,32>,	variance<16,32>,	subpix_variance<16,32>,	subpix_avg_variance<16,32>,	sad<16,32, 3>,	sad<16,32, 8>,	sad4d<16,32>},	//BLOCK_16X32	
	{sad<32,16>,	sad_avg<32,16>,	variance<32,16>,	subpix_variance<32,16>,	subpix_avg_variance<32,16>,	sad<32,16, 3>,	sad<32,16, 8>,	sad4d<32,16>},	//BLOCK_32X16	
	{sad<32,32>,	sad_avg<32,32>,	variance<32,32>,	subpix_variance<32,32>,	subpix_avg_variance<32,32>,	sad<32,32, 3>,	sad<32,32, 8>,	sad4d<32,32>},	//BLOCK_32X32	
	{sad<32,64>,	sad_avg<32,64>,	variance<32,64>,	subpix_variance<32,64>,	subpix_avg_variance<32,64>,	sad<32,64, 3>,	sad<32,64, 8>,	sad4d<32,64>},	//BLOCK_32X64	
	{sad<64,32>,	sad_avg<64,32>,	variance<64,32>,	subpix_variance<64,32>,	subpix_avg_variance<64,32>,	sad<64,32, 3>,	sad<64,32, 8>,	sad4d<64,32>},	//BLOCK_64X32	
	{sad<64,64>,	sad_avg<64,64>,	variance<64,64>,	subpix_variance<64,64>,	subpix_avg_variance<64,64>,	sad<64,64, 3>,	sad<64,64, 8>,	sad4d<64,64>},	//BLOCK_64X64	
};

uint32 get_mb_ss(const int16 *a) {
	uint32	sum = 0;
	for (int i = 0; i < 256; ++i)
		sum += square(a[i]);
	return sum;
}

uint32 variance_halfpixvar16x16_h(const uint8 *a, int a_stride, const uint8 *b, int b_stride, uint32 *sse) {
	return subpix_variance<16,16>(a, a_stride, 4, 0, b, b_stride, sse);
}

uint32 variance_halfpixvar16x16_v_c(const uint8 *a, int a_stride, const uint8 *b, int b_stride, uint32 *sse) {
	return subpix_variance<16,16>(a, a_stride, 0, 4, b, b_stride, sse);
}

uint32 variance_halfpixvar16x16_hv_c(const uint8 *a, int a_stride, const uint8 *b, int b_stride, uint32 *sse) {
	return subpix_variance<16,16>(a, a_stride, 4, 4, b, b_stride, sse);
}

template<int W, int H> uint32 mse(const uint8 *a, int a_stride, const uint8 *b, int b_stride) {
	return _variance(a, a_stride, b, b_stride, W, H);
}

int64 get_sse(const uint8 *a, int a_stride, const uint8 *b, int b_stride, int width, int height) {
	int64	total_sse = 0;

	if (const int dw = width & 15)
		total_sse += variance(&a[width - dw], a_stride, &b[width - dw], b_stride, dw, height);

	if (const int dh = height & 15)
		total_sse += variance(&a[(height - dh) * a_stride], a_stride, &b[(height - dh) * b_stride], b_stride, width & -16, dh);

	for (int y = 0; y < height / 16; ++y) {
		const uint8 *pa = a;
		const uint8 *pb = b;
		for (int x = 0; x < width / 16; ++x) {
			total_sse += _variance<16,16>(pa, a_stride, pb, b_stride);
			pa += 16;
			pb += 16;
		}
		a += 16 * a_stride;
		b += 16 * b_stride;
	}

	return total_sse;
}

//-----------------------------------------------------------------------------
//	MOTION
//-----------------------------------------------------------------------------

// #define NEW_DIAMOND_SEARCH

inline const uint8 *get_buf_from_mv(const Buffer2D &buf, const MotionVector *mv) {
	return buf.row(mv->row) + mv->col;
}

uint32 mv_cost(const MotionVector &mv, const uint32 *joint_cost, uint32 *const comp_cost[2]) {
	return joint_cost[mv.get_joint()] + comp_cost[0][mv.row] + comp_cost[1][mv.col];
}

uint32 mv_bit_cost(const MotionVector &mv, const MotionVector &ref, const uint32 *mvjcost, uint32 *mvcost[2], int weight) {
	return round_pow2(mv_cost(mv - ref, mvjcost, mvcost) * weight, 7);
}

uint32 mv_err_cost(const MotionVector &mv, const MotionVector &ref, const uint32 *mvjcost, uint32 *mvcost[2], int error_per_bit) {
	return mvcost ? round_pow2((unsigned)mv_cost(mv - ref, mvjcost, mvcost) * error_per_bit, 13) : 0;
}

uint32 mvsad_err_cost(const TileEncoder *x, const MotionVector &mv, const MotionVector &ref, int error_per_bit) {
	return round_pow2(mv_cost(mv - ref, x->nmvjointsadcost, x->nmvsadcost) * error_per_bit, 8);
}


// To avoid the penalty for crossing cache-line read, preload the reference area in a small buffer, which is aligned to make sure there won't be crossing cache-line read while reading from this buffer.
// This reduced the cpu cycles spent on reading ref data in sub-pixel filter functions.
// TODO: Currently, since sub-pixel search range here is -3 ~ 3, copy 22 rows x 32 cols area that is enough for 16x16 macroblock. Later, for SPLITMV, we could reduce the area.

struct SUBPEL_SEARCH {
	const TileDecoder	*xd;
	const variance_funcs *vfp;
	const uint8			*second_pred;
	const uint32		*mvjcost, **mvcost;
	const uint32		halfiters;
	const uint32		quarteriters;
	const uint32		eighthiters;
	const Buffer2D		&pre, &src;
	const int			pre_stride;
	const int			src_stride;
	const int			error_per_bit;
	const int			minc, maxc, minr, maxr;
	const int			offset;

	uint32			besterr;
	uint32			sse;
	MotionVector	refmv, bestmv;

	uint32			sse1;
	int				distortion;
	int				whichdir;

	SUBPEL_SEARCH(const TileEncoder *x, MotionVector *_bestmv, const MotionVector *_refmv, const variance_funcs *_vfp, int iters_per_step, int _error_per_bit, const uint8 *_second_pred, uint32 *_mvjcost, uint32 **_mvcost)
		: xd(x), refmv(*_refmv), bestmv(*_bestmv), vfp(_vfp), second_pred(_second_pred), mvjcost(_mvjcost), mvcost(_mvcost)
		, halfiters(iters_per_step), quarteriters(iters_per_step), eighthiters(iters_per_step), error_per_bit(_error_per_bit)
		, pre(x->plane[0].pre[0])
		, src(x->plane[0].src)
		, minc(max(x->mv_col_min * 8, refmv.col - MotionVector::MAX)), maxc(min(x->mv_col_max * 8, refmv.col + MotionVector::MAX))
		, minr(max(x->mv_row_min * 8, refmv.row - MotionVector::MAX)), maxr(min(x->mv_row_max * 8, refmv.row + MotionVector::MAX))
		, offset(bestmv.row * pre.stride + bestmv.col)
		, besterr(INT_MAX)
	{
		_bestmv->row *= 8;
		_bestmv->col *= 8;
	}

	int	check(int r, int c) {
		if (c < minc || c > maxc || r < minr || r > maxr)
			return INT_MAX;

		int	thismse = second_pred
			? vfp->svaf(pre.row(r) + c, pre_stride, c & 7, r & 7, src.buffer, src.stride, &sse, second_pred)
			: vfp->svf(pre.row(r) + c, pre_stride,c & 7, r & 7, src.buffer, src.stride, &sse);
		int	v = mvcost ? ((mvjcost[((r) != refmv.row) * 2 + ((c) != refmv.col)] + mvcost[0][((r) - refmv.row)] + mvcost[1][((c) - refmv.col)]) * error_per_bit + 4096) >> 13 : 0;
		if (v + thismse < besterr) {
			besterr		= v;
			bestmv.row	= r;
			bestmv.col	= c;
			distortion = thismse;
			sse1		= sse;
		}
		return v;
	}

	int first_level_checks(const MotionVector &from, int hstep) {
		uint32	left	= check(from.row, from.col - hstep);
		uint32	right	= check(from.row, from.col + hstep);
		uint32	up		= check(from.row - hstep, from.col);
		uint32	down	= check(from.row + hstep, from.col);

		whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);
		switch (whichdir) {
			case 0:	return check(from.row - hstep, from.col - hstep);
			case 1:	return check(from.row - hstep, from.col + hstep);                                   
			case 2:	return check(from.row + hstep, from.col - hstep);                                   
			case 3:	return check(from.row + hstep, from.col + hstep);                                   
		}                                               
	}

	void second_level_checks(const MotionVector &from, int hstep) {
		if (from.row != bestmv.row && from.col != bestmv.col) {
			int	kr = bestmv.row - from.row;
			int	kc = bestmv.col - from.col;
			check(from.row + kr, from.col + 2 * kc);
			check(from.row + 2 * kr, from.col + kc);
		} else if (from.row == bestmv.row && from.col != bestmv.col) {
			int	kc = bestmv.col - from.col;
			check(from.row + hstep, from.col + 2 * kc);
			check(from.row - hstep, from.col + 2 * kc);
			if (whichdir & 2)
				check(from.row - hstep, from.col + kc);
			else
				check(from.row + hstep, from.col + kc);
		} else if (from.row != bestmv.row && from.col == bestmv.col) {
			int	kr = bestmv.row - from.row;
			check(from.row + 2 * kr, from.col + hstep);
			check(from.row + 2 * kr, from.col - hstep);
			if (whichdir & 1)
				check( from.row + kr, from.col - hstep);
			else
				check(from.row + kr, from.col + hstep);
		}
	}

	// TODO(yunqingwang): SECOND_LEVEL_CHECKS_BEST was a rewrote of SECOND_LEVEL_CHECKS, and SECOND_LEVEL_CHECKS should be rewritten later in the same way.
	void second_level_checks_best() {
		int		br0 = bestmv.row;
		int		bc0 = bestmv.col;
		//assert(from.row == br || from.col == bc);
		if (from.row == br0 && from.col != bc0) {
			kc = bc0 - from.col;
		} else if (from.row != br0 && from.col == bc0) {
			kr = br0 - from.row;
		}
		check(br0 + kr, bc0);
		check(br0, bc0 + kc);
		if (br0 != br || bc0 != bc)
			check(br0 + kr, bc0 + kc);
	}

	void setup_center_error(int w, int h) {
		if (second_pred) {
			if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
				DECLARE_ALIGNED(16, uint16, comp_pred16[64 * 64]);
				vpx_highbd_comp_avg_pred(comp_pred16, second_pred, w, h, pre + offset, pre_stride);
				besterr = vfp->vf(CONVERT_TO_BYTEPTR(comp_pred16), w, src, src_stride, &sse1);
			} else {
				DECLARE_ALIGNED(16, uint8, comp_pred[64 * 64]);
				vpx_comp_avg_pred(comp_pred, second_pred, w, h, pre + offset, pre_stride);
				besterr = vfp->vf(comp_pred, w, src, src_stride, &sse1);
			}
		} else {
			besterr = vfp->vf(pre + offset, pre_stride, src, src_stride, &sse1);
		}
		distortion	= besterr;
		besterr		+= mv_err_cost(bestmv, refmv, mvjcost, mvcost, error_per_bit);
	}
};

static inline int divide_and_round(const int n, const int d) {
	return ((n < 0) ^ (d < 0)) ? ((n - d / 2) / d) : ((n + d / 2) / d);
}
static inline int is_cost_list_wellbehaved(int *cost_list) {
	return cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX && cost_list[2] != INT_MAX && cost_list[3] != INT_MAX && cost_list[4] != INT_MAX
		&& cost_list[0] < cost_list[1] && cost_list[0] < cost_list[2] && cost_list[0] < cost_list[3] && cost_list[0] < cost_list[4];
}

// Returns surface minima estimate at given precision in 1/2^n bits.
// Assume a model for the cost surface: S = A(x - x0)^2 + B(y - y0)^2 + C
// For a given set of costs S0, S1, S2, S3, S4 at points (y, x) = (0, 0), (0, -1), (1, 0), (0, 1) and (-1, 0) respectively, the solution for the location of the minima (x0, y0) is given by:
// x0 = 1/2 (S1 - S3)/(S1 + S3 - 2*S0), y0 = 1/2 (S4 - S2)/(S4 + S2 - 2*S0)
static void get_cost_surf_min(int *cost_list, int *ir, int *ic, int bits) {
	*ic = divide_and_round((cost_list[1] - cost_list[3]) * (1 << (bits - 1)), (cost_list[1] - 2 * cost_list[0] + cost_list[3]));
	*ir = divide_and_round((cost_list[4] - cost_list[2]) * (1 << (bits - 1)),(cost_list[4] - 2 * cost_list[0] + cost_list[2]));
}

int find_best_sub_pixel_tree_pruned_evenmore(const TileEncoder *x, MotionVector *bestmv, const MotionVector *ref_mv, bool allow_hp, int error_per_bit, const variance_funcs *vfp,
	int forced_stop, int iters_per_step, int *cost_list,
	int *mvjcost, int *mvcost[2],
	int *distortion, uint32 *sse1,
	const uint8 *second_pred,
	int w, int h
) {
	SUBPEL_SEARCH	search(x, bestmv, ref_mv, vfp, iters_per_step, error_per_bit, second_pred, mvjcost, mvcost);
	search.setup_center_error(w, h);

	if (is_cost_list_wellbehaved(cost_list)) {
		int ir, ic;
		get_cost_surf_min(cost_list, &ir, &ic, 2);
		if (ir != 0 || ic != 0)
			search.check(search.bestmv.row + 2 * ir, search.bestmv.col + 2 * ic);

	} else {
		MotionVector	t = search.bestmv;
		search.first_level_checks(t, 4);
		if (search.halfiters > 1)
			search.second_level_checks(t, 4);

		// Each subsequent iteration checks at least one point in common with the last iteration could be 2 ( if diag selected) 1/4 pel
		if (forced_stop != 2) {
			MotionVector	t = search.bestmv;
			search.first_level_checks(t, 2);
			if (search.quarteriters > 1)
				search.second_level_checks(t, 2);
		}
	}

	if (allow_hp && ref_mv->use_hp() && !forced_stop) {
		MotionVector	t = search.bestmv;
		search.first_level_checks(t, 1);
		if (search.eighthiters > 1)
			search.second_level_checks(t, 1);
	}

	*bestmv = search.bestmv;

	return abs(bestmv->col - ref_mv->col) > (SEARCH_CONFIG::MAX_FULL_PEL_VAL << 3) || abs(bestmv->row - ref_mv->row) > (SEARCH_CONFIG::MAX_FULL_PEL_VAL << 3)
		? INT_MAX
		: search.besterr;
}

int find_best_sub_pixel_tree_pruned_more(const TileEncoder *x, MotionVector *bestmv, const MotionVector *ref_mv, bool allow_hp, int error_per_bit, const variance_funcs *vfp,
	int forced_stop, int iters_per_step, int *cost_list,
	int *mvjcost, int *mvcost[2],
	int *distortion, uint32 *sse1,
	const uint8 *second_pred,
	int w, int h
) {
	SUBPEL_SEARCH	search(x, bestmv, ref_mv, vfp, iters_per_step, error_per_bit, second_pred, mvjcost, mvcost);
	search.setup_center_error(w, h);
	if (is_cost_list_wellbehaved(cost_list)) {
		int		ir, ic;
		get_cost_surf_min(cost_list, &ir, &ic, 1);
		if (ir != 0 || ic != 0) {
			search.check(search.bestmv.row + ir * 4, search.bestmv.col + ic * 4);
		}
	} else {
		MotionVector	t = search.bestmv;
		search.first_level_checks(t, 4);
		if (search.halfiters > 1)
			search.second_level_checks(t, 4);
	}

	// Each subsequent iteration checks at least one point in common with the last iteration could be 2 (if diag selected) 1/4 pel
	// Note forced_stop: 0 - full, 1 - qtr only, 2 - half only
	if (forced_stop != 2) {
		MotionVector	t = search.bestmv;
		search.first_level_checks(t, 2);
		if (search.quarteriters > 1)
			search.second_level_checks(t, 2);
	}

	if (allow_hp && ref_mv->use_hp() && forced_stop == 0) {
		MotionVector	t = search.bestmv;
		search.first_level_checks(t, 1);
		if (search.eighthiters > 1)
			search.second_level_checks(t, 1);
	}

	*bestmv = search.bestmv;

	return abs(bestmv->col - ref_mv->col) > (SEARCH_CONFIG::MAX_FULL_PEL_VAL << 3) || abs(bestmv->row - ref_mv->row) > (SEARCH_CONFIG::MAX_FULL_PEL_VAL << 3)
		? INT_MAX
		: search.besterr;
}

int find_best_sub_pixel_tree_pruned(const TileEncoder *x, MotionVector *bestmv, const MotionVector *ref_mv, bool allow_hp, int error_per_bit, const variance_funcs *vfp,
	int forced_stop, int iters_per_step, int *cost_list,
	int *mvjcost, int *mvcost[2],
	int *distortion, uint32 *sse1,
	const uint8 *second_pred,
	int w, int h
) {
	SUBPEL_SEARCH	search(x, bestmv, ref_mv, vfp, iters_per_step, error_per_bit, second_pred, mvjcost, mvcost);
	search.setup_center_error(w, h);
	if (cost_list && cost_list[0] != INT_MAX && cost_list[1] != INT_MAX && cost_list[2] != INT_MAX && cost_list[3] != INT_MAX && cost_list[4] != INT_MAX) {
		uint32 left, right, up, down, diag;
		whichdir = (cost_list[1] < cost_list[3] ? 0 : 1) + (cost_list[2] < cost_list[4] ? 0 : 2);
		switch (whichdir) {
			case 0:
				CHECK_BETTER(left, tr, tc - hstep);
				CHECK_BETTER(down, tr + hstep, tc);
				CHECK_BETTER(diag, tr + hstep, tc - hstep);
				break;
			case 1:
				CHECK_BETTER(right, tr, tc + hstep);
				CHECK_BETTER(down, tr + hstep, tc);
				CHECK_BETTER(diag, tr + hstep, tc + hstep);
				break;
			case 2:
				CHECK_BETTER(left, tr, tc - hstep);
				CHECK_BETTER(up, tr - hstep, tc);
				CHECK_BETTER(diag, tr - hstep, tc - hstep);
				break;
			case 3:
				CHECK_BETTER(right, tr, tc + hstep);
				CHECK_BETTER(up, tr - hstep, tc);
				CHECK_BETTER(diag, tr - hstep, tc + hstep);
				break;
		}
	} else {
		FIRST_LEVEL_CHECKS;
		if (halfiters > 1)
			SECOND_LEVEL_CHECKS;
	}

	tr = br;
	tc = bc;

	// Each subsequent iteration checks at least one point in common with
	// the last iteration could be 2 ( if diag selected) 1/4 pel

	// Note forced_stop: 0 - full, 1 - qtr only, 2 - half only
	if (forced_stop != 2) {
		hstep >>= 1;
		FIRST_LEVEL_CHECKS;
		if (quarteriters > 1) {
			SECOND_LEVEL_CHECKS;
		}
		tr = br;
		tc = bc;
	}

	if (allow_hp && use_mv_hp(ref_mv) && forced_stop == 0) {
		hstep >>= 1;
		FIRST_LEVEL_CHECKS;
		if (eighthiters > 1) {
			SECOND_LEVEL_CHECKS;
		}
		tr = br;
		tc = bc;
	}
	// These lines insure static analysis doesn't warn that
	// tr and tc aren't used after the above point.
	(void)tr;
	(void)tc;

	bestmv->row = br;
	bestmv->col = bc;

	if ((abs(bestmv->col - ref_mv->col) > (MAX_FULL_PEL_VAL << 3)) || (abs(bestmv->row - ref_mv->row) > (MAX_FULL_PEL_VAL << 3)))
		return INT_MAX;

	return besterr;
}

static const MotionVector search_step_table[12] = {
	// left, right, up, down
	{0, -4}, {0, 4}, {-4, 0}, {4, 0},
	{0, -2}, {0, 2}, {-2, 0}, {2, 0},
	{0, -1}, {0, 1}, {-1, 0}, {1, 0}
};

int find_best_sub_pixel_tree(const TileEncoder *x, MotionVector *bestmv, const MotionVector *ref_mv, bool allow_hp, int error_per_bit, const variance_funcs *vfp,
	int forced_stop, int iters_per_step, int *cost_list,
	int *mvjcost, int *mvcost[2],
	int *distortion, uint32 *sse1,
	const uint8 *second_pred,
	int w, int h
) {
	const uint8 *const z = x->plane[0].src.buf;
	const uint8 *const src_address = z;
	const int src_stride = x->plane[0].src.stride;
	const MACROBLOCKD *xd = &x->e_mbd;
	uint32 besterr = INT_MAX;
	uint32 sse;
	int thismse;
	const int y_stride = xd->plane[0].pre[0].stride;
	const int offset = bestmv->row * y_stride + bestmv->col;
	const uint8 *const y = xd->plane[0].pre[0].buf;

	int rr = ref_mv->row;
	int rc = ref_mv->col;
	int br = bestmv->row * 8;
	int bc = bestmv->col * 8;
	int hstep = 4;
	int iter, round = 3 - forced_stop;
	const int minc = max(x->mv_col_min * 8, ref_mv->col - MV_MAX);
	const int maxc = min(x->mv_col_max * 8, ref_mv->col + MV_MAX);
	const int minr = max(x->mv_row_min * 8, ref_mv->row - MV_MAX);
	const int maxr = min(x->mv_row_max * 8, ref_mv->row + MV_MAX);
	int tr = br;
	int tc = bc;
	const MotionVector *search_step = search_step_table;
	int idx, best_idx = -1;
	uint32 cost_array[5];
	int kr, kc;

	if (!(allow_hp && use_mv_hp(ref_mv)))
		if (round == 3)
			round = 2;

	bestmv->row *= 8;
	bestmv->col *= 8;

	besterr = setup_center_error(xd, bestmv, ref_mv, error_per_bit, vfp,
		z, src_stride, y, y_stride, second_pred,
		w, h, offset, mvjcost, mvcost,
		sse1, distortion);

	(void)cost_list;  // to silence compiler warning

	for (iter = 0; iter < round; ++iter) {
		// Check vertical and horizontal sub-pixel positions.
		for (idx = 0; idx < 4; ++idx) {
			tr = br + search_step[idx].row;
			tc = bc + search_step[idx].col;
			if (tc >= minc && tc <= maxc && tr >= minr && tr <= maxr) {
				const uint8 *const pre_address = y + (tr >> 3) * y_stride + (tc >> 3);
				MotionVector this_mv;
				this_mv.row = tr;
				this_mv.col = tc;
				if (second_pred == NULL)
					thismse = vfp->svf(pre_address, y_stride, tc & 7, tr & 7,
					src_address, src_stride, &sse);
				else
					thismse = vfp->svaf(pre_address, y_stride, tc & 7, tr & 7,
					src_address, src_stride, &sse, second_pred);
				cost_array[idx] = thismse +
					mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost, error_per_bit);

				if (cost_array[idx] < besterr) {
					best_idx = idx;
					besterr = cost_array[idx];
					*distortion = thismse;
					*sse1 = sse;
				}
			} else {
				cost_array[idx] = INT_MAX;
			}
		}

		// Check diagonal sub-pixel position
		kc = (cost_array[0] <= cost_array[1] ? -hstep : hstep);
		kr = (cost_array[2] <= cost_array[3] ? -hstep : hstep);

		tc = bc + kc;
		tr = br + kr;
		if (tc >= minc && tc <= maxc && tr >= minr && tr <= maxr) {
			const uint8 *const pre_address = y + (tr >> 3) * y_stride + (tc >> 3);
			MotionVector this_mv = { tr, tc };
			if (second_pred == NULL)
				thismse = vfp->svf(pre_address, y_stride, tc & 7, tr & 7,
				src_address, src_stride, &sse);
			else
				thismse = vfp->svaf(pre_address, y_stride, tc & 7, tr & 7,
				src_address, src_stride, &sse, second_pred);
			cost_array[4] = thismse +
				mv_err_cost(&this_mv, ref_mv, mvjcost, mvcost, error_per_bit);

			if (cost_array[4] < besterr) {
				best_idx = 4;
				besterr = cost_array[4];
				*distortion = thismse;
				*sse1 = sse;
			}
		} else {
			cost_array[idx] = INT_MAX;
		}

		if (best_idx < 4 && best_idx >= 0) {
			br += search_step[best_idx].row;
			bc += search_step[best_idx].col;
		} else if (best_idx == 4) {
			br = tr;
			bc = tc;
		}

		if (iters_per_step > 1 && best_idx != -1)
			SECOND_LEVEL_CHECKS_BEST;

		tr = br;
		tc = bc;

		search_step += 4;
		hstep >>= 1;
		best_idx = -1;
	}

	// Each subsequent iteration checks at least one point in common with
	// the last iteration could be 2 ( if diag selected) 1/4 pel

	// These lines insure static analysis doesn't warn that
	// tr and tc aren't used after the above point.
	(void)tr;
	(void)tc;

	bestmv->row = br;
	bestmv->col = bc;

	return abs(bestmv->col - ref_mv->col) > (MAX_FULL_PEL_VAL << 3)) || abs(bestmv->row - ref_mv->row) > (MAX_FULL_PEL_VAL << 3)
		? INT_MAX
		: besterr;
}


#define CHECK_BETTER \
  {\
    if (thissad < bestsad) {\
      if (use_mvcost) \
        thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);\
      if (thissad < bestsad) {\
        bestsad = thissad;\
        best_site = i;\
      }\
    }\
  }

#define MAX_PATTERN_SCALES         11
#define MAX_PATTERN_CANDIDATES      8  // max number of canddiates per scale
#define PATTERN_CANDIDATES_REF      3  // number of refinement candidates

// Calculate and return a sad+mvcost list around an integer best pel.
static inline void calc_int_cost_list(const TileEncoder *x, const MotionVector *ref_mv, int sadpb, const variance_funcs *fn_ptr, const MotionVector *best_mv, int *cost_list) {
	static const MotionVector neighbors[4] = { {0, -1}, {1, 0}, {0, 1}, {-1, 0} };
	const Buffer2D &what	= x->plane[0].src;
	const Buffer2D &in_what = x->plane[0].pre[0];
	const MotionVector fcenter_mv(ref_mv->row >> 3, ref_mv->col >> 3);
	int		br = best_mv->row;
	int		bc = best_mv->col;
	MotionVector this_mv;
	uint32 sse;

	this_mv.row = br;
	this_mv.col = bc;
	cost_list[0] = fn_ptr->vf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride, &sse) + mvsad_err_cost(x, &this_mv, &fcenter_mv, sadpb);
	if (x->check_bounds(br, bc, 1)) {
		for (int i = 0; i < 4; i++) {
			const MotionVector this_mv(br + neighbors[i].row,  bc + neighbors[i].col);
			cost_list[i + 1] = fn_ptr->vf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride, &sse) + mv_err_cost(&this_mv, &fcenter_mv, x->nmvjointcost, x->mvcost, x->errorperbit);
		}
	} else {
		for (int i = 0; i < 4; i++) {
			const MotionVector this_mv(br + neighbors[i].row,  bc + neighbors[i].col);
			cost_list[i + 1] = x->is_mv_in(this_mv)
				? cost_list[i + 1] = fn_ptr->vf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride, &sse) + mv_err_cost(&this_mv, &fcenter_mv, x->nmvjointcost, x->mvcost, x->errorperbit)
				: INT_MAX;
		}
	}
}

// Generic pattern search function that searches over multiple scales.
// Each scale can have a different number of candidates and shape of candidates as indicated in the num_candidates and candidates arrays passed into this function
static int vp9_pattern_search(const TileEncoder *x, MotionVector *ref_mv, int search_param, int sad_per_bit, int do_init_search, int *cost_list, const variance_funcs *vfp, bool use_mvcost,
	const MotionVector *center_mv, MotionVector *best_mv,
	const int num_candidates[MAX_PATTERN_SCALES],
	const MotionVector candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES]
) {
	static const int search_param_to_steps[MAX_MVSEARCH_STEPS] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,};
	const Buffer2D &what	= x->plane[0].src;
	const Buffer2D &in_what = x->plane[0].pre[0];
	int br, bc;
	int bestsad = INT_MAX;
	int thissad;
	int k = -1;
	const MotionVector fcenter_mv(center_mv->row >> 3, center_mv->col >> 3);
	int best_init_s = search_param_to_steps[search_param];
	// adjust ref_mv to make sure it is within MotionVector range
	clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
	br = ref_mv->row;
	bc = ref_mv->col;

	// Work out the start point for the search
	bestsad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv), in_what->stride) + mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);

	// Search all possible scales upto the search param around the center point
	// pick the scale of the point that is best as the starting scale of
	// further steps around it.
	if (do_init_search) {
		int s = best_init_s;
		best_init_s = -1;
		for (int t = 0; t <= s; ++t) {
			int best_site = -1;
			if (check_bounds(x, br, bc, 1 << t)) {
				for (int i = 0; i < num_candidates[t]; i++) {
					const MotionVector this_mv(br + candidates[t][i].row, bc + candidates[t][i].col);
					thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
					CHECK_BETTER
				}
			} else {
				for (int i = 0; i < num_candidates[t]; i++) {
					const MotionVector this_mv(br + candidates[t][i].row, bc + candidates[t][i].col);
					if (x->is_mv_in(this_mv)) {
						thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
						CHECK_BETTER
					}
				}
			}
			if (best_site != -1) {
				best_init_s = t;
				k = best_site;
			}
		}
		if (best_init_s != -1) {
			br += candidates[best_init_s][k].row;
			bc += candidates[best_init_s][k].col;
		}
	}

	// If the center point is still the best, just skip this and move to
	// the refinement step.
	if (best_init_s != -1) {
		int best_site	= -1;
		int	s			= best_init_s;

		do {
			// No need to search all 6 points the 1st time if initial search was used
			if (!do_init_search || s != best_init_s) {
				if (check_bounds(x, br, bc, 1 << s)) {
					for (int i = 0; i < num_candidates[s]; i++) {
						const MotionVector this_mv(br + candidates[s][i].row, bc + candidates[s][i].col);
						thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
						CHECK_BETTER
					}
				} else {
					for (int i = 0; i < num_candidates[s]; i++) {
						const MotionVector this_mv(br + candidates[s][i].row, bc + candidates[s][i].col);
						if (x->is_mv_in(this_mv)) {
							thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
							CHECK_BETTER
						}
					}
				}

				if (best_site != -1) {
					br += candidates[s][best_site].row;
					bc += candidates[s][best_site].col;
					k = best_site;
				}
			}

			do {
				int next_chkpts_indices[PATTERN_CANDIDATES_REF];
				best_site = -1;
				next_chkpts_indices[0] = k == 0 ? num_candidates[s] - 1 : k - 1;
				next_chkpts_indices[1] = k;
				next_chkpts_indices[2] = k == num_candidates[s] - 1 ? 0 : k + 1;

				if (x->check_bounds(br, bc, 1 << s)) {
					for (int i = 0; i < PATTERN_CANDIDATES_REF; i++) {
						const MotionVector this_mv(br + candidates[s][next_chkpts_indices[i]].row, bc + candidates[s][next_chkpts_indices[i]].col);
						thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
						CHECK_BETTER
					}
				} else {
					for (int i = 0; i < PATTERN_CANDIDATES_REF; i++) {
						const MotionVector this_mv(br + candidates[s][next_chkpts_indices[i]].row, bc + candidates[s][next_chkpts_indices[i]].col);
						if (x->is_mv_in(this_mv)) {
							thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
							CHECK_BETTER
						}
					}
				}

				if (best_site != -1) {
					k = next_chkpts_indices[best_site];
					br += candidates[s][k].row;
					bc += candidates[s][k].col;
				}
			} while (best_site != -1);
		} while (s--);
	}

	// Returns the one-away integer pel sad values around the best as follows:
	// cost_list[0]: cost at the best integer pel
	// cost_list[1]: cost at delta {0, -1} (left)   from the best integer pel
	// cost_list[2]: cost at delta { 1, 0} (bottom) from the best integer pel
	// cost_list[3]: cost at delta { 0, 1} (right)  from the best integer pel
	// cost_list[4]: cost at delta {-1, 0} (top)    from the best integer pel
	if (cost_list) {
		const MotionVector best_mv(br, bc);
		calc_int_cost_list(x, &fcenter_mv, sad_per_bit, vfp, &best_mv, cost_list);
	}
	best_mv->row = br;
	best_mv->col = bc;
	return bestsad;
}

// A specialized function where the smallest scale search candidates
// are 4 1-away neighbors, and cost_list is non-null
// TODO(debargha): Merge this function with the one above. Also remove
// use_mvcost option since it is always 1, to save unnecessary branches.
static int vp9_pattern_search_sad(const TileEncoder *x, MotionVector *ref_mv, int search_param, int sad_per_bit, int do_init_search, int *cost_list, const variance_funcs *vfp, bool use_mvcost,
	const MotionVector *center_mv, MotionVector *best_mv,
	const int num_candidates[MAX_PATTERN_SCALES],
	const MotionVector candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES]
) {
	static const int search_param_to_steps[MAX_MVSEARCH_STEPS] = { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,};
	const Buffer2D &what	= x->plane[0].src;
	const Buffer2D &in_what = xd->plane[0].pre[0];
	int br, bc;
	int bestsad = INT_MAX;
	int thissad;
	int k = -1;
	const MotionVector fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
	int best_init_s = search_param_to_steps[search_param];
	// adjust ref_mv to make sure it is within MotionVector range
	clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
	br = ref_mv->row;
	bc = ref_mv->col;
	if (cost_list)
		cost_list[0] = cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] = INT_MAX;

	// Work out the start point for the search
	bestsad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv), in_what->stride) + mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);

	// Search all possible scales upto the search param around the center point pick the scale of the point that is best as the starting scale of further steps around it.
	if (do_init_search) {
		int	s = best_init_s;
		best_init_s = -1;
		for (int t = 0; t <= s; ++t) {
			int best_site = -1;
			if (x->check_bounds(br, bc, 1 << t)) {
				for (int i = 0; i < num_candidates[t]; i++) {
					const MotionVector this_mv(br + candidates[t][i].row, bc + candidates[t][i].col);
					thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
					CHECK_BETTER
				}
			} else {
				for (int i = 0; i < num_candidates[t]; i++) {
					const MotionVector this_mv(br + candidates[t][i].row, bc + candidates[t][i].col);
					if (x->is_mv_in(this_mv)) {
						thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
						CHECK_BETTER
					}
				}
			}
			if (best_site != -1) {
				best_init_s = t;
				k = best_site;
			}
		}
		if (best_init_s != -1) {
			br += candidates[best_init_s][k].row;
			bc += candidates[best_init_s][k].col;
		}
	}

	// If the center point is still the best, just skip this and move to
	// the refinement step.
	if (best_init_s != -1) {
		int do_sad		= (num_candidates[0] == 4 && cost_list != NULL);
		int best_site	= -1;
		int	s			= best_init_s;

		for (; s >= do_sad; s--) {
			if (!do_init_search || s != best_init_s) {
				if (x->check_bounds(br, bc, 1 << s)) {
					for (int i = 0; i < num_candidates[s]; i++) {
						const MotionVector this_mv(br + candidates[s][i].row, bc + candidates[s][i].col);
						thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
						CHECK_BETTER
					}
				} else {
					for (int i = 0; i < num_candidates[s]; i++) {
						const MotionVector this_mv(br + candidates[s][i].row, bc + candidates[s][i].col);
						if (x->is_mv_in(this_mv)) {
							thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
							CHECK_BETTER
						}
					}
				}

				if (best_site != -1) {
					br += candidates[s][best_site].row;
					bc += candidates[s][best_site].col;
					k = best_site;
				}
			}

			do {
				int next_chkpts_indices[PATTERN_CANDIDATES_REF];
				best_site = -1;
				next_chkpts_indices[0] = (k == 0) ? num_candidates[s] - 1 : k - 1;
				next_chkpts_indices[1] = k;
				next_chkpts_indices[2] = (k == num_candidates[s] - 1) ? 0 : k + 1;

				if (x->check_bounds(x, br, bc, 1 << s)) {
					for (int i = 0; i < PATTERN_CANDIDATES_REF; i++) {
						const MotionVector this_mv(br + candidates[s][next_chkpts_indices[i]].row, bc + candidates[s][next_chkpts_indices[i]].col);
						thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
						CHECK_BETTER
					}
				} else {
					for (int i = 0; i < PATTERN_CANDIDATES_REF; i++) {
						const MotionVector this_mv(br + candidates[s][next_chkpts_indices[i]].row, bc + candidates[s][next_chkpts_indices[i]].col);
						if (x->is_mv_in(this_mv)) {
							thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
							CHECK_BETTER
						}
					}
				}

				if (best_site != -1) {
					k = next_chkpts_indices[best_site];
					br += candidates[s][k].row;
					bc += candidates[s][k].col;
				}
			} while (best_site != -1);
		}

		// Note: If we enter the if below, then cost_list must be non-NULL.
		if (s == 0) {
			cost_list[0] = bestsad;
			if (!do_init_search || s != best_init_s) {
				if (x->check_bounds(br, bc, 1 << s)) {
					for (int i = 0; i < num_candidates[s]; i++) {
						const MotionVector this_mv(br + candidates[s][i].row, bc + candidates[s][i].col);
						cost_list[i + 1] = thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
						CHECK_BETTER
					}
				} else {
					for (int i = 0; i < num_candidates[s]; i++) {
						const MotionVector this_mv(br + candidates[s][i].row, bc + candidates[s][i].col);
						if (x->is_mv_in(this_mv)) {
							cost_list[i + 1] = thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
							CHECK_BETTER
						}
					}
				}

				if (best_site != -1) {
					br += candidates[s][best_site].row;
					bc += candidates[s][best_site].col;
					k = best_site;
				}
			}
			while (best_site != -1) {
				int next_chkpts_indices[PATTERN_CANDIDATES_REF];
				best_site = -1;
				next_chkpts_indices[0] = k == 0 ? num_candidates[s] - 1 : k - 1;
				next_chkpts_indices[1] = k;
				next_chkpts_indices[2] = k == num_candidates[s] - 1 ? 0 : k + 1;
				cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] = INT_MAX;
				cost_list[((k + 2) % 4) + 1] = cost_list[0];
				cost_list[0] = bestsad;

				if (x->check_bounds(br, bc, 1 << s)) {
					for (int i = 0; i < PATTERN_CANDIDATES_REF; i++) {
						const MotionVector this_mv(br + candidates[s][next_chkpts_indices[i]].row, bc + candidates[s][next_chkpts_indices[i]].col);
						cost_list[next_chkpts_indices[i] + 1] = thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
						CHECK_BETTER
					}
				} else {
					for (int i = 0; i < PATTERN_CANDIDATES_REF; i++) {
						const MotionVector this_mv(br + candidates[s][next_chkpts_indices[i]].row, bc + candidates[s][next_chkpts_indices[i]].col);
						if (x->is_mv_in(this_mv)) {
							cost_list[next_chkpts_indices[i] + 1] = thissad = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
							CHECK_BETTER
						} else {
							cost_list[next_chkpts_indices[i] + 1] = INT_MAX;
						}
					}
				}

				if (best_site != -1) {
					k = next_chkpts_indices[best_site];
					br += candidates[s][k].row;
					bc += candidates[s][k].col;
				}
			}
		}
	}

	// Returns the one-away integer pel sad values around the best as follows:
	// cost_list[0]: sad at the best integer pel
	// cost_list[1]: sad at delta {0, -1} (left)   from the best integer pel
	// cost_list[2]: sad at delta { 1, 0} (bottom) from the best integer pel
	// cost_list[3]: sad at delta { 0, 1} (right)  from the best integer pel
	// cost_list[4]: sad at delta {-1, 0} (top)    from the best integer pel
	if (cost_list) {
		static const MotionVector neighbors[4] = { {0, -1}, {1, 0}, {0, 1}, {-1, 0} };
		if (cost_list[0] == INT_MAX) {
			cost_list[0] = bestsad;
			if (x->check_bounds(br, bc, 1)) {
				for (int i = 0; i < 4; i++) {
					const MotionVector this_mv(br + neighbors[i].row, bc + neighbors[i].col);
					cost_list[i + 1] = vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride);
				}
			} else {
				for (int i = 0; i < 4; i++) {
					const MotionVector this_mv(br + neighbors[i].row,  bc + neighbors[i].col);
					cost_list[i + 1] = x->is_mv_in(this_mv) ? vfp->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &this_mv), in_what->stride) : INT_MAX;
				}
			}
		} else {
			if (use_mvcost) {
				for (int i = 0; i < 4; i++) {
					const MotionVector this_mv(br + neighbors[i].row, bc + neighbors[i].col);
					if (cost_list[i + 1] != INT_MAX)
						cost_list[i + 1] += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
				}
			}
		}
	}
	best_mv->row = br;
	best_mv->col = bc;
	return bestsad;
}

int get_mvpred_var(const TileEncoder *x, const MotionVector *best_mv, const MotionVector *center_mv, const variance_funcs *vfp, bool use_mvcost) {
	const Buffer2D &what = &x->plane[0].src;
	const Buffer2D &in_what = &xd->plane[0].pre[0];
	const MotionVector mv(best_mv->row * 8, best_mv->col * 8);
	uint32 unused;

	return vfp->vf(what->buf, what->stride, get_buf_from_mv(in_what, best_mv), in_what->stride, &unused) + (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost, x->errorperbit) : 0);
}

int get_mvpred_av_var(const TileEncoder *x, const MotionVector *best_mv, const MotionVector *center_mv, const uint8 *second_pred, const variance_funcs *vfp, bool use_mvcost) {
	const Buffer2D &what = &x->plane[0].src;
	const Buffer2D &in_what = &xd->plane[0].pre[0];
	const MotionVector mv(best_mv->row * 8, best_mv->col * 8);
	uint32 unused;

	return vfp->svaf(get_buf_from_mv(in_what, best_mv), in_what->stride, 0, 0, what->buf, what->stride, &unused, second_pred) + (use_mvcost ? mv_err_cost(&mv, center_mv, x->nmvjointcost, x->mvcost, x->errorperbit) : 0);
}

static int hex_search(const TileEncoder *x, MotionVector *ref_mv, int search_param, int sad_per_bit, int do_init_search, int *cost_list, const variance_funcs *vfp, bool use_mvcost,
	const MotionVector *center_mv, MotionVector *best_mv
) {
	// First scale has 8-closest points, the rest have 6 points in hex shape at increasing scales
	static const int hex_num_candidates[MAX_PATTERN_SCALES] = { 8, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 };
	// Note that the largest candidate step at each scale is 2^scale
	static const MotionVector hex_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
	  {{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, { 0, 1}, { -1, 1}, {-1, 0}},
	  {{-1, -2}, {1, -2}, {2, 0}, {1, 2}, { -1, 2}, { -2, 0}},
	  {{-2, -4}, {2, -4}, {4, 0}, {2, 4}, { -2, 4}, { -4, 0}},
	  {{-4, -8}, {4, -8}, {8, 0}, {4, 8}, { -4, 8}, { -8, 0}},
	  {{-8, -16}, {8, -16}, {16, 0}, {8, 16}, { -8, 16}, { -16, 0}},
	  {{-16, -32}, {16, -32}, {32, 0}, {16, 32}, { -16, 32}, { -32, 0}},
	  {{-32, -64}, {32, -64}, {64, 0}, {32, 64}, { -32, 64}, { -64, 0}},
	  {{-64, -128}, {64, -128}, {128, 0}, {64, 128}, { -64, 128}, { -128, 0}},
	  {{-128, -256}, {128, -256}, {256, 0}, {128, 256}, { -128, 256}, { -256, 0}},
	  {{-256, -512}, {256, -512}, {512, 0}, {256, 512}, { -256, 512}, { -512, 0}},
	  {{-512, -1024}, {512, -1024}, {1024, 0}, {512, 1024}, { -512, 1024},
		{ -1024, 0}},
	};
	return pattern_search(x, ref_mv, search_param, sad_per_bit, do_init_search, cost_list, vfp, use_mvcost, center_mv, best_mv, hex_num_candidates, hex_candidates);
}

static int bigdia_search(const TileEncoder *x, MotionVector *ref_mv, int search_param, int sad_per_bit, int do_init_search, int *cost_list, const variance_funcs *vfp, bool use_mvcost,
	const MotionVector *center_mv, MotionVector *best_mv
) {
	// First scale has 4-closest points, the rest have 8 points in diamond shape at increasing scales
	static const int bigdia_num_candidates[MAX_PATTERN_SCALES] = { 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, };
	// Note that the largest candidate step at each scale is 2^scale
	static const MotionVector bigdia_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
		{{0, -1}, {1, 0}, { 0, 1}, {-1, 0}},
		{{-1, -1}, {0, -2}, {1, -1}, {2, 0}, {1, 1}, {0, 2}, {-1, 1}, {-2, 0}},
		{{-2, -2}, {0, -4}, {2, -2}, {4, 0}, {2, 2}, {0, 4}, {-2, 2}, {-4, 0}},
		{{-4, -4}, {0, -8}, {4, -4}, {8, 0}, {4, 4}, {0, 8}, {-4, 4}, {-8, 0}},
		{{-8, -8}, {0, -16}, {8, -8}, {16, 0}, {8, 8}, {0, 16}, {-8, 8}, {-16, 0}},
		{{-16, -16}, {0, -32}, {16, -16}, {32, 0}, {16, 16}, {0, 32},
		  {-16, 16}, {-32, 0}},
		{{-32, -32}, {0, -64}, {32, -32}, {64, 0}, {32, 32}, {0, 64},
		  {-32, 32}, {-64, 0}},
		{{-64, -64}, {0, -128}, {64, -64}, {128, 0}, {64, 64}, {0, 128},
		  {-64, 64}, {-128, 0}},
		{{-128, -128}, {0, -256}, {128, -128}, {256, 0}, {128, 128}, {0, 256},
		  {-128, 128}, {-256, 0}},
		{{-256, -256}, {0, -512}, {256, -256}, {512, 0}, {256, 256}, {0, 512},
		  {-256, 256}, {-512, 0}},
		{{-512, -512}, {0, -1024}, {512, -512}, {1024, 0}, {512, 512}, {0, 1024},
		  {-512, 512}, {-1024, 0}},
	};
	return pattern_search_sad(x, ref_mv, search_param, sad_per_bit, do_init_search, cost_list, vfp, use_mvcost, center_mv, best_mv, bigdia_num_candidates, bigdia_candidates);
}

static int square_search(const TileEncoder *x, MotionVector *ref_mv, int search_param, int sad_per_bit, int do_init_search, int *cost_list, const variance_funcs *vfp, bool use_mvcost,
	const MotionVector *center_mv, MotionVector *best_mv
) {
	// All scales have 8 closest points in square shape
	static const int square_num_candidates[MAX_PATTERN_SCALES] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, };
	// Note that the largest candidate step at each scale is 2^scale
	static const MotionVector square_candidates[MAX_PATTERN_SCALES][MAX_PATTERN_CANDIDATES] = {
		{{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}},
		{{-2, -2}, {0, -2}, {2, -2}, {2, 0}, {2, 2}, {0, 2}, {-2, 2}, {-2, 0}},
		{{-4, -4}, {0, -4}, {4, -4}, {4, 0}, {4, 4}, {0, 4}, {-4, 4}, {-4, 0}},
		{{-8, -8}, {0, -8}, {8, -8}, {8, 0}, {8, 8}, {0, 8}, {-8, 8}, {-8, 0}},
		{{-16, -16}, {0, -16}, {16, -16}, {16, 0}, {16, 16}, {0, 16},
		  {-16, 16}, {-16, 0}},
		{{-32, -32}, {0, -32}, {32, -32}, {32, 0}, {32, 32}, {0, 32},
		  {-32, 32}, {-32, 0}},
		{{-64, -64}, {0, -64}, {64, -64}, {64, 0}, {64, 64}, {0, 64},
		  {-64, 64}, {-64, 0}},
		{{-128, -128}, {0, -128}, {128, -128}, {128, 0}, {128, 128}, {0, 128},
		  {-128, 128}, {-128, 0}},
		{{-256, -256}, {0, -256}, {256, -256}, {256, 0}, {256, 256}, {0, 256},
		  {-256, 256}, {-256, 0}},
		{{-512, -512}, {0, -512}, {512, -512}, {512, 0}, {512, 512}, {0, 512},
		  {-512, 512}, {-512, 0}},
		{{-1024, -1024}, {0, -1024}, {1024, -1024}, {1024, 0}, {1024, 1024},
		  {0, 1024}, {-1024, 1024}, {-1024, 0}},
	};
	return pattern_search(x, ref_mv, search_param, sad_per_bit, do_init_search, cost_list, vfp, use_mvcost, center_mv, best_mv, square_num_candidates, square_candidates);
}

static int fast_hex_search(const TileEncoder *x, MotionVector *ref_mv, int search_param, int sad_per_bit,
	int do_init_search,  // must be zero for fast_hex
	int *cost_list,
	const variance_funcs *vfp, bool use_mvcost,
	const MotionVector *center_mv, MotionVector *best_mv
) {
	return hex_search(x, ref_mv, max(MAX_MVSEARCH_STEPS - 2, search_param), sad_per_bit, do_init_search, cost_list, vfp, use_mvcost, center_mv, best_mv);
}

static int fast_dia_search(const TileEncoder *x, MotionVector *ref_mv, int search_param, int sad_per_bit, int do_init_search, int *cost_list, const variance_funcs *vfp, bool use_mvcost,
	const MotionVector *center_mv,
	MotionVector *best_mv) {
	return bigdia_search(
		x, ref_mv, max(MAX_MVSEARCH_STEPS - 2, search_param), sad_per_bit,
		do_init_search, cost_list, vfp, use_mvcost, center_mv, best_mv
	);
}

#undef CHECK_BETTER

// Exhuastive motion search around a given centre position with a given step size.
static int exhuastive_mesh_search(const TileEncoder *x,
	MotionVector *ref_mv, MotionVector *best_mv,
	int range, int step, int sad_per_bit,
	const variance_funcs *fn_ptr,
	const MotionVector *center_mv
) {
	const Buffer2D &what	= &x->plane[0].src;
	const Buffer2D &in_what = &xd->plane[0].pre[0];
	MotionVector fcenter_mv(center_mv->row, center_mv->col);
	uint32 best_sad = INT_MAX;
	int start_col, end_col, start_row, end_row;
	int col_step = (step > 1) ? step : 4;

	assert(step >= 1);

	clamp_mv(&fcenter_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
	*best_mv = fcenter_mv;
	best_sad = fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &fcenter_mv), in_what->stride) + mvsad_err_cost(x, &fcenter_mv, ref_mv, sad_per_bit);
	int	start_row	= max(-range, x->mv_row_min - fcenter_mv.row);
	int	start_col	= max(-range, x->mv_col_min - fcenter_mv.col);
	int	end_row		= min(range, x->mv_row_max - fcenter_mv.row);
	int	end_col		= min(range, x->mv_col_max - fcenter_mv.col);

	for (int r = start_row; r <= end_row; r += step) {
		for (int c = start_col; c <= end_col; c += col_step) {
			// Step > 1 means we are not checking every location in this pass.
			if (step > 1) {
				const MotionVector mv(fcenter_mv.row + r, fcenter_mv.col + c);
				uint32 sad = fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &mv), in_what->stride);
				if (sad < best_sad) {
					sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
					if (sad < best_sad) {
						best_sad = sad;
						*best_mv = mv;
					}
				}
			} else {
				// 4 sads in a single call if we are checking every location
				if (c + 3 <= end_col) {
					uint32 sads[4];
					const uint8 *addrs[4];
					for (int i = 0; i < 4; ++i) {
						const MotionVector mv(fcenter_mv.row + r, fcenter_mv.col + c + i);
						addrs[i] = get_buf_from_mv(in_what, &mv);
					}
					fn_ptr->sdx4df(what->buf, what->stride, addrs, in_what->stride, sads);

					for (int i = 0; i < 4; ++i) {
						if (sads[i] < best_sad) {
							const MotionVector mv(fcenter_mv.row + r, fcenter_mv.col + c + i);
							const uint32 sad = sads[i] + mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
							if (sad < best_sad) {
								best_sad = sad;
								*best_mv = mv;
							}
						}
					}
				} else {
					for (int i = 0; i < end_col - c; ++i) {
						const MotionVector mv(fcenter_mv.row + r, fcenter_mv.col + c + i);
						uint32 sad = fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &mv), in_what->stride);
						if (sad < best_sad) {
							sad += mvsad_err_cost(x, &mv, ref_mv, sad_per_bit);
							if (sad < best_sad) {
								best_sad = sad;
								*best_mv = mv;
							}
						}
					}
				}
			}
		}
	}

	return best_sad;
}

int diamond_search_sad_c(const TileEncoder *x, const SEARCH_CONFIG *cfg, MotionVector *ref_mv, MotionVector *best_mv, int search_param, int sad_per_bit, int *num00, const variance_funcs *fn_ptr, const MotionVector *center_mv) {
	uint8 *what = x->plane[0].src.buf;
	const int what_stride = x->plane[0].src.stride;
	const uint8 *in_what;
	const int in_what_stride = xd->plane[0].pre[0].stride;
	const uint8 *best_address;

	uint32	bestsad = INT_MAX;
	int		best_site = -1;
	int		last_site = -1;

	// search_param determines the length of the initial step and hence the number
	// of iterations.
	// 0 = initial step (MAX_FIRST_STEP) pel
	// 1 = (MAX_FIRST_STEP/2) pel,
	// 2 = (MAX_FIRST_STEP/4) pel...
  //  const search_site *ss = &cfg->ss[search_param * cfg->searches_per_step];
	const MotionVector	*ss_mv = &cfg->ss_mv[search_param * cfg->searches_per_step];
	const intptr_t		*ss_os = &cfg->ss_os[search_param * cfg->searches_per_step];
	const int			tot_steps = cfg->total_steps - search_param;

	const MotionVector fcenter_mv(center_mv->row >> 3, center_mv->col >> 3);
	clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
	int	ref_row = ref_mv->row;
	int	ref_col = ref_mv->col;
	*num00 = 0;
	best_mv->row = ref_row;
	best_mv->col = ref_col;

	// Work out the start point for the search
	in_what = xd->plane[0].pre[0].buf + ref_row * in_what_stride + ref_col;
	best_address = in_what;

	// Check the starting position
	bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride) + mvsad_err_cost(x, best_mv, &fcenter_mv, sad_per_bit);

	int	i = 0;

	for (int step = 0; step < tot_steps; step++) {
		int all_in = 1;

		// All_in is true if every one of the points we are checking are within
		// the bounds of the image.
		all_in &= ((best_mv->row + ss_mv[i].row) > x->mv_row_min);
		all_in &= ((best_mv->row + ss_mv[i + 1].row) < x->mv_row_max);
		all_in &= ((best_mv->col + ss_mv[i + 2].col) > x->mv_col_min);
		all_in &= ((best_mv->col + ss_mv[i + 3].col) < x->mv_col_max);

		// If all the pixels are within the bounds we don't check whether the
		// search point is valid in this loop,  otherwise we check each point
		// for validity..
		if (all_in) {
			uint32 sad_array[4];

			for (int j = 0; j < cfg->searches_per_step; j += 4) {
				unsigned char const *block_offset[4];

				for (int t = 0; t < 4; t++)
					block_offset[t] = ss_os[i + t] + best_address;

				fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride, sad_array);

				for (int t = 0; t < 4; t++, i++) {
					if (sad_array[t] < bestsad) {
						const MotionVector this_mv(best_mv->row + ss_mv[i].row, best_mv->col + ss_mv[i].col);
						sad_array[t] += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
						if (sad_array[t] < bestsad) {
							bestsad = sad_array[t];
							best_site = i;
						}
					}
				}
			}
		} else {
			for (int j = 0; j < cfg->searches_per_step; j++) {
				// Trap illegal vectors
				const MotionVector this_mv(best_mv->row + ss_mv[i].row, best_mv->col + ss_mv[i].col);
				if (x->is_mv_in(this_mv)) {
					const uint8 *const check_here = ss_os[i] + best_address;
					uint32 thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride);
					if (thissad < bestsad) {
						thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
						if (thissad < bestsad) {
							bestsad = thissad;
							best_site = i;
						}
					}
				}
				i++;
			}
		}
		if (best_site != last_site) {
			best_mv->row += ss_mv[best_site].row;
			best_mv->col += ss_mv[best_site].col;
			best_address += ss_os[best_site];
			last_site = best_site;
		#if defined(NEW_DIAMOND_SEARCH)
			while (1) {
				const MotionVector this_mv(best_mv->row + ss_mv[best_site].row, best_mv->col + ss_mv[best_site].col);
				if (x->is_mv_in(this_mv)) {
					const uint8 *const check_here = ss_os[best_site] + best_address;
					uint32 thissad = fn_ptr->sdf(what, what_stride, check_here, in_what_stride);
					if (thissad < bestsad) {
						thissad += mvsad_err_cost(x, &this_mv, &fcenter_mv, sad_per_bit);
						if (thissad < bestsad) {
							bestsad = thissad;
							best_mv->row += ss_mv[best_site].row;
							best_mv->col += ss_mv[best_site].col;
							best_address += ss_os[best_site];
							continue;
						}
					}
				}
				break;
			}
		#endif
		} else if (best_address == in_what) {
			(*num00)++;
		}
	}
	return bestsad;
}

static int vector_match(int16 *ref, int16 *src, int bwl) {
	int best_sad = INT_MAX;
	int center, offset = 0;
	int bw = 4 << bwl;  // redundant variable, to be changed in the experiments.
	for (int d = 0; d <= bw; d += 16) {
		int	this_sad = vpx_vector_var(&ref[d], src, bwl);
		if (this_sad < best_sad) {
			best_sad = this_sad;
			offset = d;
		}
	}
	center = offset;

	for (int d = -8; d <= 8; d += 16) {
		int this_pos = offset + d;
		// check limit
		if (this_pos < 0 || this_pos > bw)
			continue;
		int	this_sad = vpx_vector_var(&ref[this_pos], src, bwl);
		if (this_sad < best_sad) {
			best_sad = this_sad;
			center = this_pos;
		}
	}
	offset = center;

	for (int d = -4; d <= 4; d += 8) {
		int this_pos = offset + d;
		// check limit
		if (this_pos < 0 || this_pos > bw)
			continue;
		int	this_sad = vpx_vector_var(&ref[this_pos], src, bwl);
		if (this_sad < best_sad) {
			best_sad = this_sad;
			center = this_pos;
		}
	}
	offset = center;

	for (int d = -2; d <= 2; d += 4) {
		int this_pos = offset + d;
		// check limit
		if (this_pos < 0 || this_pos > bw)
			continue;
		int this_sad = vpx_vector_var(&ref[this_pos], src, bwl);
		if (this_sad < best_sad) {
			best_sad = this_sad;
			center = this_pos;
		}
	}
	offset = center;

	for (int d = -1; d <= 1; d += 2) {
		int this_pos = offset + d;
		// check limit
		if (this_pos < 0 || this_pos > bw)
			continue;
		int this_sad = vpx_vector_var(&ref[this_pos], src, bwl);
		if (this_sad < best_sad) {
			best_sad = this_sad;
			center = this_pos;
		}
	}

	return (center - (bw >> 1));
}

static const MotionVector search_pos[4] = {
	{-1, 0}, {0, -1}, {0, 1}, {1, 0},
};

uint32 Encoder::int_pro_motion_estimation(TileEncoder *x, BLOCK_SIZE bsize, int mi_row, int mi_col) {
	ModeInfo *mi = x->mi[0];
	Buffer2D backup_yv12[MAX_MB_PLANE] = { {0, 0} };
	DECLARE_ALIGNED(16, int16, hbuf[128]);
	DECLARE_ALIGNED(16, int16, vbuf[128]);
	DECLARE_ALIGNED(16, int16, src_hbuf[64]);
	DECLARE_ALIGNED(16, int16, src_vbuf[64]);
	const int		bw = 4 << b_width_log2_lookup[bsize];
	const int		bh = 4 << b_height_log2_lookup[bsize];
	const int		search_width = bw << 1;
	const int		search_height = bh << 1;
	const int		src_stride = x->plane[0].src.stride;
	const int		ref_stride = xd->plane[0].pre[0].stride;
	uint8 const		*ref_buf, *src_buf;
	MotionVector	*tmp_mv = &xd->mi[0]->mv[0].as_mv;
	uint32			best_sad, tmp_sad, this_sad[4];
	MotionVector	this_mv;
	const int		norm_factor = 3 + (bw >> 5);
	const FrameBuffer *scaled_ref_frame = get_scaled_ref_frame(mi->ref_frame[0]);

	if (scaled_ref_frame) {
		// Swap out the reference frame for a version that's been scaled to/ match the resolution of the current frame, allowing the existing motion search code to be used without additional modifications.
		for (int i = 0; i < MAX_MB_PLANE; i++)
			backup_yv12[i] = xd->plane[i].pre[0];
		vp9_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL);
	}

#if CONFIG_VP9_HIGHBITDEPTH
	{
		uint32 this_sad;
		tmp_mv->row = 0;
		tmp_mv->col = 0;
		this_sad = cpi->fn_ptr[bsize].sdf(x->plane[0].src.buf, src_stride, xd->plane[0].pre[0].buf, ref_stride);

		if (scaled_ref_frame) {
			for (int i = 0; i < MAX_MB_PLANE; i++)
				xd->plane[i].pre[0] = backup_yv12[i];
		}
		return this_sad;
	}
#endif

	// Set up prediction 1-D reference set
	ref_buf = xd->plane[0].pre[0].buf - (bw >> 1);
	for (int idx = 0; idx < search_width; idx += 16) {
		vpx_int_pro_row(&hbuf[idx], ref_buf, ref_stride, bh);
		ref_buf += 16;
	}

	ref_buf = xd->plane[0].pre[0].buf - (bh >> 1) * ref_stride;
	for (int idx = 0; idx < search_height; ++idx) {
		vbuf[idx] = vpx_int_pro_col(ref_buf, bw) >> norm_factor;
		ref_buf += ref_stride;
	}

	// Set up src 1-D reference set
	for (int idx = 0; idx < bw; idx += 16) {
		src_buf = x->plane[0].src.buf + idx;
		vpx_int_pro_row(&src_hbuf[idx], src_buf, src_stride, bh);
	}

	src_buf = x->plane[0].src.buf;
	for (int idx = 0; idx < bh; ++idx) {
		src_vbuf[idx] = vpx_int_pro_col(src_buf, bw) >> norm_factor;
		src_buf += src_stride;
	}

	// Find the best match per 1-D search
	tmp_mv->col = vector_match(hbuf, src_hbuf, b_width_log2_lookup[bsize]);
	tmp_mv->row = vector_match(vbuf, src_vbuf, b_height_log2_lookup[bsize]);

	this_mv = *tmp_mv;
	src_buf = x->plane[0].src.buf;
	ref_buf = xd->plane[0].pre[0].buf + this_mv.row * ref_stride + this_mv.col;
	best_sad = cpi->fn_ptr[bsize].sdf(src_buf, src_stride, ref_buf, ref_stride);

	const uint8 * const pos[4] = {ref_buf - ref_stride, ref_buf - 1, ref_buf + 1, ref_buf + ref_stride };
	cpi->fn_ptr[bsize].sdx4df(src_buf, src_stride, pos, ref_stride, this_sad);

	for (int idx = 0; idx < 4; ++idx) {
		if (this_sad[idx] < best_sad) {
			best_sad = this_sad[idx];
			tmp_mv->row = search_pos[idx].row + this_mv.row;
			tmp_mv->col = search_pos[idx].col + this_mv.col;
		}
	}

	if (this_sad[0] < this_sad[3])
		this_mv.row -= 1;
	else
		this_mv.row += 1;

	if (this_sad[1] < this_sad[2])
		this_mv.col -= 1;
	else
		this_mv.col += 1;

	ref_buf = xd->plane[0].pre[0].buf + this_mv.row * ref_stride + this_mv.col;

	tmp_sad = cpi->fn_ptr[bsize].sdf(src_buf, src_stride, ref_buf, ref_stride);
	if (best_sad > tmp_sad) {
		*tmp_mv = this_mv;
		best_sad = tmp_sad;
	}

	tmp_mv->row *= 8;
	tmp_mv->col *= 8;

	if (scaled_ref_frame) {
		for (int i = 0; i < MAX_MB_PLANE; i++)
			xd->plane[i].pre[0] = backup_yv12[i];
	}

	return best_sad;
}

// Runs sequence of diamond searches in smaller steps for RD.
// do_refine: If last step (1-away) of n-step search doesn't pick the center point as the best match, we will do a final 1-away diamond refining search
int Encoder::full_pixel_diamond(TileEncoder *x, MotionVector *mvp_full, int step_param, int sadpb, int further_steps, int do_refine, int *cost_list, const variance_funcs *fn_ptr,
	const MotionVector *ref_mv, MotionVector *dst_mv
) {
	MotionVector temp_mv;
	int thissme, n, num00 = 0;
	int bestsme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv, step_param, sadpb, &n, fn_ptr, ref_mv);
	if (bestsme < INT_MAX)
		bestsme = x->get_mvpred_var(&temp_mv, ref_mv, fn_ptr, 1);
	*dst_mv = temp_mv;

	// If there won't be more n-step search, check to see if refining search is
	// needed.
	if (n > further_steps)
		do_refine = 0;

	while (n++ < further_steps) {
		if (num00) {
			num00--;
		} else {
			thissme = cpi->diamond_search_sad(x, &cpi->ss_cfg, mvp_full, &temp_mv, step_param + n, sadpb, &num00, fn_ptr, ref_mv);
			if (thissme < INT_MAX)
				thissme = x->get_mvpred_var(&temp_mv, ref_mv, fn_ptr, 1);

			// check to see if refining search is needed.
			if (num00 > further_steps - n)
				do_refine = 0;

			if (thissme < bestsme) {
				bestsme = thissme;
				*dst_mv = temp_mv;
			}
		}
	}

	// final 1-away diamond refining search
	if (do_refine) {
		const int search_range = 8;
		MotionVector best_mv = *dst_mv;
		thissme = vp9_refining_search_sad(x, &best_mv, sadpb, search_range, fn_ptr, ref_mv);
		if (thissme < INT_MAX)
			thissme = x->get_mvpred_var(&best_mv, ref_mv, fn_ptr, 1);
		if (thissme < bestsme) {
			bestsme = thissme;
			*dst_mv = best_mv;
		}
	}

	// Return cost list.
	if (cost_list)
		calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, dst_mv, cost_list);
	return bestsme;
}

#define MIN_RANGE 7
#define MAX_RANGE 256
#define MIN_INTERVAL 1
// Runs an limited range exhaustive mesh search using a pattern set
// according to the encode speed profile.
static int Encoder::full_pixel_exhaustive(TileEncoder *x,
	MotionVector *centre_mv_full, int sadpb, int *cost_list,
	const variance_funcs *fn_ptr,
	const MotionVector *ref_mv, MotionVector *dst_mv
) {
	const SPEED_FEATURES *const sf = &cpi->sf;
	MotionVector temp_mv = { centre_mv_full->row, centre_mv_full->col };
	MotionVector f_ref_mv = { ref_mv->row >> 3, ref_mv->col >> 3 };
	int interval	= sf->mesh_patterns[0].interval;
	int range		= sf->mesh_patterns[0].range;

	// Keep track of number of exhaustive calls (this frame in this thread).
	++(*x->ex_search_count_ptr);

	// Trap illegal values for interval and range for this function.
	if (range < MIN_RANGE || range > MAX_RANGE || interval < MIN_INTERVAL || interval > range)
		return INT_MAX;

	int	baseline_interval_divisor = range / interval;

	// Check size of proposed first range against magnitude of the centre
	// value used as a starting point.
	range		= clamp(range, (5 * max(abs(temp_mv.row), abs(temp_mv.col))) / 4, MAX_RANGE);
	interval	= max(interval, range / baseline_interval_divisor);

	// initial search
	int	bestsme = exhuastive_mesh_search(x, &f_ref_mv, &temp_mv, range, interval, sadpb, fn_ptr, &temp_mv);

	if ((interval > MIN_INTERVAL) && (range > MIN_RANGE)) {
		// Progressive searches with range and step size decreasing each time
		// till we reach a step size of 1. Then break out.
		for (int i = 1; i < MAX_MESH_STEP; ++i) {
			// First pass with coarser step and longer range
			bestsme = exhuastive_mesh_search(x, &f_ref_mv, &temp_mv, sf->mesh_patterns[i].range, sf->mesh_patterns[i].interval, sadpb, fn_ptr, &temp_mv);
			if (sf->mesh_patterns[i].interval == 1)
				break;
		}
	}

	if (bestsme < INT_MAX)
		bestsme = x->get_mvpred_var(&temp_mv, ref_mv, fn_ptr, 1);
	*dst_mv = temp_mv;

	// Return cost list.
	if (cost_list)
		calc_int_cost_list(x, ref_mv, sadpb, fn_ptr, dst_mv, cost_list);
	return bestsme;
}

int full_search_sad_c(const TileEncoder *x, const MotionVector *ref_mv, int sad_per_bit, int distance, const variance_funcs *fn_ptr, const MotionVector *center_mv, MotionVector *best_mv) {
	const Buffer2D &what = &x->plane[0].src;
	const Buffer2D &in_what = &xd->plane[0].pre[0];
	const int row_min = max(ref_mv->row - distance, x->mv_row_min);
	const int row_max = min(ref_mv->row + distance, x->mv_row_max);
	const int col_min = max(ref_mv->col - distance, x->mv_col_min);
	const int col_max = min(ref_mv->col + distance, x->mv_col_max);
	const MotionVector fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
	int best_sad = fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv), in_what->stride) + mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);
	*best_mv = *ref_mv;

	for (int r = row_min; r < row_max; ++r) {
		for (int c = col_min; c < col_max; ++c) {
			const MotionVector mv(r, c);
			const int sad = fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &mv), in_what->stride) + mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
			if (sad < best_sad) {
				best_sad = sad;
				*best_mv = mv;
			}
		}
	}
	return best_sad;
}

int full_search_sadx3(const TileEncoder *x, const MotionVector *ref_mv, int sad_per_bit, int distance, const variance_funcs *fn_ptr, const MotionVector *center_mv, MotionVector *best_mv) {
	const Buffer2D &what = &x->plane[0].src;
	const Buffer2D &in_what = &xd->plane[0].pre[0];
	const int row_min = max(ref_mv->row - distance, x->mv_row_min);
	const int row_max = min(ref_mv->row + distance, x->mv_row_max);
	const int col_min = max(ref_mv->col - distance, x->mv_col_min);
	const int col_max = min(ref_mv->col + distance, x->mv_col_max);
	const MotionVector fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
	uint32 best_sad = fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv), in_what->stride) + mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);
	*best_mv = *ref_mv;

	for (int r = row_min; r < row_max; ++r) {
		int c = col_min;
		const uint8 *check_here = &in_what->buf[r * in_what->stride + c];

		if (fn_ptr->sdx3f != NULL) {
			while ((c + 2) < col_max) {
				DECLARE_ALIGNED(16, uint32_t, sads[3]);

				fn_ptr->sdx3f(what->buf, what->stride, check_here, in_what->stride, sads);

				for (int i = 0; i < 3; ++i) {
					uint32 sad = sads[i];
					if (sad < best_sad) {
						const MotionVector mv(r, c);
						sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
						if (sad < best_sad) {
							best_sad = sad;
							*best_mv = mv;
						}
					}
					++check_here;
					++c;
				}
			}
		}

		while (c < col_max) {
			uint32 sad = fn_ptr->sdf(what->buf, what->stride, check_here, in_what->stride);
			if (sad < best_sad) {
				const MotionVector mv(r, c);
				sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
				if (sad < best_sad) {
					best_sad = sad;
					*best_mv = mv;
				}
			}
			++check_here;
			++c;
		}
	}

	return best_sad;
}

int full_search_sadx8(const TileEncoder *x, const MotionVector *ref_mv, int sad_per_bit, int distance, const variance_funcs *fn_ptr, const MotionVector *center_mv, MotionVector *best_mv) {
	const Buffer2D &what = &x->plane[0].src;
	const Buffer2D &in_what = &xd->plane[0].pre[0];
	const int row_min = max(ref_mv->row - distance, x->mv_row_min);
	const int row_max = min(ref_mv->row + distance, x->mv_row_max);
	const int col_min = max(ref_mv->col - distance, x->mv_col_min);
	const int col_max = min(ref_mv->col + distance, x->mv_col_max);
	const MotionVector fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
	uint32 best_sad = fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv), in_what->stride) + mvsad_err_cost(x, ref_mv, &fcenter_mv, sad_per_bit);
	*best_mv = *ref_mv;

	for (int r = row_min; r < row_max; ++r) {
		int c = col_min;
		const uint8 *check_here = &in_what->buf[r * in_what->stride + c];

		if (fn_ptr->sdx8f != NULL) {
			while ((c + 7) < col_max) {
				DECLARE_ALIGNED(16, uint32_t, sads[8]);
				fn_ptr->sdx8f(what->buf, what->stride, check_here, in_what->stride, sads);

				for (int i = 0; i < 8; ++i) {
					uint32 sad = sads[i];
					if (sad < best_sad) {
						const MotionVector mv(r, c);
						sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
						if (sad < best_sad) {
							best_sad = sad;
							*best_mv = mv;
						}
					}
					++check_here;
					++c;
				}
			}
		}

		if (fn_ptr->sdx3f != NULL) {
			while ((c + 2) < col_max) {
				DECLARE_ALIGNED(16, uint32_t, sads[3]);
				fn_ptr->sdx3f(what->buf, what->stride, check_here, in_what->stride, sads);

				for (int i = 0; i < 3; ++i) {
					uint32 sad = sads[i];
					if (sad < best_sad) {
						const MotionVector mv(r, c);
						sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
						if (sad < best_sad) {
							best_sad = sad;
							*best_mv = mv;
						}
					}
					++check_here;
					++c;
				}
			}
		}

		while (c < col_max) {
			uint32 sad = fn_ptr->sdf(what->buf, what->stride, check_here, in_what->stride);
			if (sad < best_sad) {
				const MotionVector mv(r, c);
				sad += mvsad_err_cost(x, &mv, &fcenter_mv, sad_per_bit);
				if (sad < best_sad) {
					best_sad = sad;
					*best_mv = mv;
				}
			}
			++check_here;
			++c;
		}
	}

	return best_sad;
}

int refining_search_sad(const TileEncoder *x, MotionVector *ref_mv, int error_per_bit, int search_range, const variance_funcs *fn_ptr, const MotionVector *center_mv) {
	const MotionVector neighbors[4] = { { -1, 0}, {0, -1}, {0, 1}, {1, 0} };
	const Buffer2D &what = &x->plane[0].src;
	const Buffer2D &in_what = &xd->plane[0].pre[0];
	const MotionVector fcenter_mv = { center_mv->row >> 3, center_mv->col >> 3 };
	const uint8 *best_address = get_buf_from_mv(in_what, ref_mv);
	uint32 best_sad = fn_ptr->sdf(what->buf, what->stride, best_address, in_what->stride) + mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);
	
	for (int i = 0; i < search_range; i++) {
		int best_site = -1;
		const int all_in = ref_mv->row - 1 > x->mv_row_min && ref_mv->row + 1 < x->mv_row_max && ref_mv->col - 1 > x->mv_col_min && ref_mv->col + 1 < x->mv_col_max;

		if (all_in) {
			uint32 sads[4];
			const uint8 *const positions[4] = { best_address - in_what->stride,  best_address - 1, best_address + 1, best_address + in_what->stride };

			fn_ptr->sdx4df(what->buf, what->stride, positions, in_what->stride, sads);

			for (int j = 0; j < 4; ++j) {
				if (sads[j] < best_sad) {
					const MotionVector mv(ref_mv->row + neighbors[j].row, ref_mv->col + neighbors[j].col);
					sads[j] += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
					if (sads[j] < best_sad) {
						best_sad = sads[j];
						best_site = j;
					}
				}
			}
		} else {
			for (int j = 0; j < 4; ++j) {
				const MotionVector mv(ref_mv->row + neighbors[j].row, ref_mv->col + neighbors[j].col);

				if (x->is_mv_in(mv)) {
					uint32 sad = fn_ptr->sdf(what->buf, what->stride, get_buf_from_mv(in_what, &mv), in_what->stride);
					if (sad < best_sad) {
						sad += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
						if (sad < best_sad) {
							best_sad = sad;
							best_site = j;
						}
					}
				}
			}
		}

		if (best_site == -1) {
			break;
		} else {
			ref_mv->row += neighbors[best_site].row;
			ref_mv->col += neighbors[best_site].col;
			best_address = get_buf_from_mv(in_what, ref_mv);
		}
	}

	return best_sad;
}

// This function is called when we do joint motion search in comp_inter_inter mode.
int refining_search_8p_c(const TileEncoder *x, MotionVector *ref_mv, int error_per_bit, int search_range, const variance_funcs *fn_ptr, const MotionVector *center_mv, const uint8 *second_pred) {
	const MotionVector neighbors[8] = { {-1, 0}, {0, -1}, {0, 1}, {1, 0}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1} };
	const Buffer2D		&what = &x->plane[0].src;
	const Buffer2D		&in_what = &xd->plane[0].pre[0];
	const MotionVector	fcenter_mv(center_mv->row >> 3, center_mv->col >> 3);
	uint32 best_sad = fn_ptr->sdaf(what->buf, what->stride, get_buf_from_mv(in_what, ref_mv), in_what->stride, second_pred) + mvsad_err_cost(x, ref_mv, &fcenter_mv, error_per_bit);
	
	for (int i = 0; i < search_range; ++i) {
		int best_site = -1;

		for (int j = 0; j < 8; ++j) {
			const MotionVector mv(ref_mv->row + neighbors[j].row, ref_mv->col + neighbors[j].col);

			if (x->is_mv_in(mv)) {
				uint32 sad = fn_ptr->sdaf(what->buf, what->stride, get_buf_from_mv(in_what, &mv), in_what->stride, second_pred);
				if (sad < best_sad) {
					sad += mvsad_err_cost(x, &mv, &fcenter_mv, error_per_bit);
					if (sad < best_sad) {
						best_sad = sad;
						best_site = j;
					}
				}
			}
		}

		if (best_site == -1) {
			break;
		} else {
			ref_mv->row += neighbors[best_site].row;
			ref_mv->col += neighbors[best_site].col;
		}
	}
	return best_sad;
}

#define MIN_EX_SEARCH_LIMIT 128
bool Encoder::is_exhaustive_allowed(VP9_COMP *cpi, TileEncoder *x) {
	const int max_ex = max(MIN_EX_SEARCH_LIMIT, (*x->m_search_count_ptr * sf->max_exaustive_pct) / 100);

	return sf.allow_exhaustive_searches
		&& sf.exhaustive_searches_thresh < INT_MAX
		&& *x->ex_search_count_ptr <= max_ex
		&& !rc.is_src_frame_alt_ref;
}

int full_pixel_search(TileEncoder *x, BLOCK_SIZE bsize, MotionVector *mvp_full, int step_param, int error_per_bit, int *cost_list, const MotionVector *ref_mv, MotionVector *tmp_mv, int var_max, int rd) {
	const SEARCH_METHODS method = sf->mv.search_method;
	variance_funcs *fn_ptr = &variance_table[bsize];
	int var = 0;
	if (cost_list)
		cost_list[0] = cost_list[1] = cost_list[2] = cost_list[3] = cost_list[4] = INT_MAX;

	// Keep track of number of searches (this frame in this thread).
	++(*x->m_search_count_ptr);

	switch (method) {
		case FAST_DIAMOND:
			var = fast_dia_search(x, mvp_full, step_param, error_per_bit, 0, cost_list, fn_ptr, 1, ref_mv, tmp_mv);
			break;
		case FAST_HEX:
			var = fast_hex_search(x, mvp_full, step_param, error_per_bit, 0, cost_list, fn_ptr, 1, ref_mv, tmp_mv);
			break;
		case HEX:
			var = hex_search(x, mvp_full, step_param, error_per_bit, 1, cost_list, fn_ptr, 1, ref_mv, tmp_mv);
			break;
		case SQUARE:
			var = square_search(x, mvp_full, step_param, error_per_bit, 1, cost_list, fn_ptr, 1, ref_mv, tmp_mv);
			break;
		case BIGDIA:
			var = bigdia_search(x, mvp_full, step_param, error_per_bit, 1, cost_list, fn_ptr, 1, ref_mv, tmp_mv);
			break;
		case NSTEP:
			var = full_pixel_diamond(x, mvp_full, step_param, error_per_bit, MAX_MVSEARCH_STEPS - 1 - step_param, 1, cost_list, fn_ptr, ref_mv, tmp_mv);

			// Should we allow a follow on exhaustive search?
			if (is_exhaustive_allowed(x)) {
				int64_t exhuastive_thr = sf->exhaustive_searches_thresh;
				exhuastive_thr >>= 8 - (b_width_log2_lookup[bsize] + b_height_log2_lookup[bsize]);

				// Threshold variance for an exhaustive full search.
				if (var > exhuastive_thr) {
					int var_ex;
					MotionVector tmp_mv_ex;
					var_ex = full_pixel_exhaustive(x, tmp_mv, error_per_bit, cost_list, fn_ptr, ref_mv, &tmp_mv_ex);

					if (var_ex < var) {
						var = var_ex;
						*tmp_mv = tmp_mv_ex;
					}
				}
			}
			break;
	}

	if (method != NSTEP && rd && var < var_max)
		var = vp9_get_mvpred_var(x, tmp_mv, ref_mv, fn_ptr, 1);

	return var;
}


uint32 do_16x16_motion_iteration(const Buffer2D &src, const Buffer2D &dst, const MotionVector *ref_mv, MotionVector *dst_mv, int mb_row, int mb_col, SPEED_FEATURES &sf) {
	const variance_funcs v_fn_ptr = &variance_table[bsize];

	const int tmp_col_min = x->mv_col_min;
	const int tmp_col_max = x->mv_col_max;
	const int tmp_row_min = x->mv_row_min;
	const int tmp_row_max = x->mv_row_max;
	MotionVector	ref_full;
	int				cost_list[5];

	// Further step/diamond searches as necessary
	int step_param = min(sf.mv->reduce_first_step_size, MAX_MVSEARCH_STEPS - 2);

	vp9_set_mv_search_range(x, ref_mv);

	ref_full.col = ref_mv->col >> 3;
	ref_full.row = ref_mv->row >> 3;

	full_pixel_search(x, BLOCK_16X16, &ref_full, step_param, x->errorperbit, sf.mv.cond_cost_list(cost_list), ref_mv, dst_mv, 0, 0, HEX);

	// Try sub-pixel MC: if (bestsme > error_thresh && bestsme < INT_MAX)
	int		distortion;
	uint32	sse;
	find_fractional_mv_step(x, dst_mv, ref_mv, allow_high_precision_mv, x->errorperbit, &v_fn_ptr, 0, sf.mv->subpel_iters_per_step, sf.mv.cond_cost_list(cost_list),
		NULL, NULL,
		&distortion, &sse, NULL, 0, 0
	);

	xd->mi[0]->mode			= NEWMV;
	xd->mi[0]->mv[0].as_mv	= *dst_mv;

	build_inter_predictors_sby(xd, mb_row, mb_col, BLOCK_16X16);

	// restore UMV window
	x->mv_col_min = tmp_col_min;
	x->mv_col_max = tmp_col_max;
	x->mv_row_min = tmp_row_min;
	x->mv_row_max = tmp_row_max;

	return sad<16,16>(src.buffer, src.stride, dst.buffer, dst.stride);
}

int do_16x16_motion_search(const Buffer2D &src, const Buffer2D &dst, int mb_row, int mb_col, const MotionVector *ref_mv, MotionVector *dst_mv) {
	uint8	*sptr	= src.row(mb_row * 16) + mb_col * 16;
	uint8	*dptr	= dst.row(mb_row * 16) + mb_col * 16;

	// Try zero MotionVector first
	// FIXME should really use something like near/nearest MotionVector and/or MotionVector prediction
	uint32	err = sad<16,16>(sptr, src.stride, dptr, dst.stride);
	dst_mv->clear();

	// Test last reference frame using the previous best mv as the starting point (best reference) for the search
	MotionVector	tmp_mv;
	uint32			tmp_err = do_16x16_motion_iteration(ref_mv, tmp_mv, mb_row, mb_col);
	if (tmp_err < err) {
		err		= tmp_err;
		dst_mv	= tmp_mv;
	}

	// If the current best reference mv is not centered on 0,0 then do a 0,0
	// based search as well.
	if (ref_mv->row != 0 || ref_mv->col != 0) {
		uint32 tmp_err = do_16x16_motion_iteration(MotionVector(0, 0), tmp_mv, mb_row, mb_col);
		if (tmp_err < err) {
			dst_m	= tmp_mv;
			err		= tmp_err;
		}
	}
	return err;
}

int do_16x16_zerozero_search(const Buffer2D &src, const Buffer2D &dst, int mb_row, int mb_col, MotionVector *dst_mv) {
	dst_mv->clear();
	return sad<16,16>(src.row(mb_row * 16) + mb_col * 16, src.stride, dst.row(mb_row * 16) + mb_col * 16, dst.stride);
}

int find_best_16x16_intra(const Buffer2D &src, const Buffer2D &dst, int mb_row, int mb_col, PREDICTION_MODE *pbest_mode) {
	int		best_mode	= -1;
	uint32	best_err	= INT_MAX;

	uint8	*sptr	= src.row(mb_row * 16) + mb_col * 16;
	uint8	*dptr	= dst.row(mb_row * 16) + mb_col * 16;

	// calculate SATD for each intra prediction mode; we're intentionally not doing 4x4, we just want a rough estimate
	for (int mode = DC_PRED; mode <= TM_PRED; mode++) {
		predict_intra_block(xd, 2, TX_16X16, mode, src, dst, 0, 0, 0);
		uint32 err = sad<16,16>(sptr, src.stride, dptr, dst.stride);
		if (err < best_err) {
			best_err	= err;
			best_mode	= mode;
		}
	}
	if (pbest_mode)
		*pbest_mode = (PREDICTION_MODE)best_mode;

	return best_err;
}


} // namespace vp9
