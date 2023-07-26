#include "h265_internal.h"
#include "jobs.h"
#include "vector_string.h"

#ifdef HEVC_RANGE
#define IF_HAS_RANGE(x)	(x)
#else
#define IF_HAS_RANGE(x)	false
#endif

#ifdef HEVC_3D
#define ONLY_3D(x)	, x
#else
#define ONLY_3D(x)
#endif

#ifdef HEVC_SCC
#define IF_HAS_SCC(x)	(x)
#else
#define IF_HAS_SCC(x)	false
#endif


namespace h265 {

//-----------------------------------------------------------------------------
// model
//-----------------------------------------------------------------------------

enum CM_index {
	// SAO
	CM_SAO_MERGE								= 0,
	CM_SAO_TYPE_IDX								= CM_SAO_MERGE + 1,

	// CB-tree
	CM_SPLIT_CU									= CM_SAO_TYPE_IDX + 1,
	CM_CU_SKIP									= CM_SPLIT_CU + 3,

	// intra-prediction
	CM_PART_MODE								= CM_CU_SKIP + 3,
	CM_PREV_INTRA_LUMA_PRED						= CM_PART_MODE + 4,
	CM_INTRA_CHROMA_PRED_MODE					= CM_PREV_INTRA_LUMA_PRED + 1,

	// transform-tree
	CM_CBF_LUMA									= CM_INTRA_CHROMA_PRED_MODE + 1,
	CM_CBF_CHROMA								= CM_CBF_LUMA + 2,
	CM_SPLIT_TRANSFORM							= CM_CBF_CHROMA + 4,
	CM_CU_CHROMA_QP_OFFSET						= CM_SPLIT_TRANSFORM + 3,
	CM_CU_CHROMA_QP_OFFSET_IDX					= CM_CU_CHROMA_QP_OFFSET + 1,

	// residual
	CM_LAST_SIGNIFICANT_COEFFICIENT_X_PREFIX	= CM_CU_CHROMA_QP_OFFSET_IDX + 1,
	CM_LAST_SIGNIFICANT_COEFFICIENT_Y_PREFIX	= CM_LAST_SIGNIFICANT_COEFFICIENT_X_PREFIX + 18,
	CM_CODED_SUB_BLOCK							= CM_LAST_SIGNIFICANT_COEFFICIENT_Y_PREFIX + 18,
	CM_SIGNIFICANT_COEFF						= CM_CODED_SUB_BLOCK + 4,
	CM_COEFF_ABS_LEVEL_GREATER1					= CM_SIGNIFICANT_COEFF + 27 + 15 + 2,	//27 luma, 15 chroma, 2 skip
	CM_COEFF_ABS_LEVEL_GREATER2					= CM_COEFF_ABS_LEVEL_GREATER1 + 24,

	CM_CU_QP_DELTA_ABS							= CM_COEFF_ABS_LEVEL_GREATER2 + 6,
	CM_TRANSFORM_SKIP							= CM_CU_QP_DELTA_ABS + 2,
	CM_RDPCM									= CM_TRANSFORM_SKIP + 2,
	CM_RDPCM_DIR								= CM_RDPCM + 2,

	// motion
	CM_MERGE									= CM_RDPCM_DIR + 2,
	CM_MERGE_IDX								= CM_MERGE + 1,
	CM_PRED_MODE								= CM_MERGE_IDX + 1,
	CM_ABS_MVD_GREATER01						= CM_PRED_MODE + 1,
	CM_MVP_LX									= CM_ABS_MVD_GREATER01 + 2,
	CM_RQT_ROOT_CBF								= CM_MVP_LX + 1,
	CM_REF_IDX_LX								= CM_RQT_ROOT_CBF + 1,
	CM_INTER_PRED_IDC							= CM_REF_IDX_LX + 2,

	CM_CU_TRANSQUANT_BYPASS						= CM_INTER_PRED_IDC + 5,
	CM_LOG2_RES_SCALE_ABS_PLUS1					= CM_CU_TRANSQUANT_BYPASS + 1,
	CM_RES_SCALE_SIGN							= CM_LOG2_RES_SCALE_ABS_PLUS1 + 8,

	// 3D extension
	CM_SKIP_INTRA								= CM_RES_SCALE_SIGN			+ 2,
	CM_NO_DIM									= CM_SKIP_INTRA				+ 1,
	CM_DEPTH_INTRA_MODE_IDX						= CM_NO_DIM					+ 1,
	CM_SKIP_INTRA_MODE_IDX						= CM_DEPTH_INTRA_MODE_IDX	+ 1,
	CM_DBBP										= CM_SKIP_INTRA_MODE_IDX	+ 1,
	CM_DC_ONLY									= CM_DBBP					+ 1,
	CM_IV_RES_PRED_WEIGHT_IDX					= CM_DC_ONLY				+ 1,
	CM_ILLU_COMP								= CM_IV_RES_PRED_WEIGHT_IDX	+ 3,
	CM_DEPTH_DC_PRESENT							= CM_ILLU_COMP				+ 1,
	CM_DEPTH_DC_ABS								= CM_DEPTH_DC_PRESENT		+ 1,

	// SCC extension
	CM_TU_TU_RESIDUAL_ACT						= CM_DEPTH_DC_ABS			+ 1,
	CM_PALETTE_MODE								= CM_TU_TU_RESIDUAL_ACT		+ 1,
	CM_PALETTE_TRANSPOSE						= CM_PALETTE_MODE			+ 1,
	CM_PALETTE_COPY_ABOVE						= CM_PALETTE_TRANSPOSE		+ 1,
	CM_PALETTE_TRANSPOSE_RUN_PREFIX				= CM_PALETTE_COPY_ABOVE		+ 1,

	CM_TABLE_LENGTH								= CM_PALETTE_TRANSPOSE_RUN_PREFIX + 8,
};

constexpr auto operator+(CM_index a, int b) { return CM_index((int)a + b); }

static const uint8 initValue_split_cu[3][3]				= {
	{107, 139, 126},		//B
	{107, 139, 126},		//P
	{139, 141, 157},		//I
};
static const uint8 initValue_part_mode[3][4]			= {
	{154, 139, 154, 154}, 	//B
	{154, 139, 154, 154},	//P
	{184, 154, 139, 154},	//I
};
static const uint8 initValue_prev_intra_luma_pred[3]	= {
	183,					//B
	154,					//P
	184,					//I
};
static const uint8 initValue_intra_chroma_pred_mode[3]	= {
	152,					//B
	152,					//P
	63,						//I
};
static const uint8 initValue_cbf_luma[2][2]				= {
	{153,111}, 				//B/P
	{111,141},				//I
};
static const uint8 initValue_cbf_chroma[3][4]			= {
	{149, 92,167,154}, 		//B
	{149,107,167,154},		//P
	{ 94,138,182,154},		//I
};
static const uint8 initValue_split_transform[3][3]		= {
	{224,167,122}, 			//B
	{124,138,94}, 			//P
	{153,138,138},			//I
};
static const uint8 initValue_last_significant_coefficient_prefix[3][18] = {
	//luma 4x4		luma 8x8		luma 16x16			luma 32x32				chroma
	{125,110,124,	110, 95, 94,	125,111,111, 79,	125,126,111,111, 79,	108,123, 93}, 	//B
	{125,110, 94,	110, 95, 79,	125,111,110, 78,	110,111,111, 95, 94,	108,123,108},	//P
	{110,110,124,	125,140,153,	125,127,140,109,	111,143,127,111, 79,	108,123, 63},	//I
};
static const uint8 initValue_coded_sub_block[3][4]		= {
	{121,140,61,154},  		//B
	{121,140,61,154}, 		//P
	{ 91,171,134,141},		//I
};
static const uint8 initValue_significant_coeff[3][44]	= {
	//luma 4x4								luma 8x8 diag				luma 8x8 horiz/vert			luma 16x16+					chroma 4x4								chroma 8x8		chroma 16x16+	skipmode
	{170,154,139,153,139,123,123, 63,124,	166,183,140,136,153,154,	166,183,140,136,153,154,	166,183,140,136,153,154,	170,153,138,138,122,121,122,121,167,	151,183,140,	151,183,140,	140,140},	//B
	{155,154,139,153,139,123,123, 63,153,	166,183,140,136,153,154,	166,183,140,136,153,154,	166,183,140,136,153,154,	170,153,123,123,107,121,107,121,167,	151,183,140,	151,183,140,	140,140},	//P
	{111,111,125,110,110,94, 124,108,124,	107,125,141,179,153,125,	107,125,141,179,153,125,	107,125,141,179,153,125,	140,139,182,182,152,136,152,136,153,	136,139,111,	136,139,111,	141,111},	//I
};
static const uint8 initValue_coeff_abs_level_greater1[3][24] = {
	//luma block == 0					luma block > 0						chroma
	{154,196,167,167, 154,152,167,182,	182,134,149,136, 153,121,136,122,	169,208,166,167, 154,152,167,182}, 	//B
	{154,196,196,167, 154,152,167,182,	182,134,149,136, 153,121,136,137,	169,194,166,167, 154,167,137,182},	//P
	{140, 92,137,138, 140,152,138,139,	153, 74,149, 92, 139,107,122,152,	140,179,166,182, 140,227,122,197},	//I
};
static const uint8 initValue_coeff_abs_level_greater2[3][6] = {
	//luma block == 0					luma block > 0						chroma
	{107,167,							 91,107,							107,167}, 	//B
	{107,167,							 91,122,							107,167},	//P
	{138,153,							136,167,							152,152},	//I
};
static const uint8 initValue_sao_type_idx_lumaChroma[3]	= {
	160,			//B
	185,			//P
	200,			//I
};
static const uint8 initValue_cu_skip[2][3] = {
	{197,185,201},	//B
	{197,185,201},	//P
};
static const uint8 initValue_motion[2][5] = {
	//merge		merge_idx	pred_mode	abs_mvd_greater01
	{154,		137,		134,		169,198},	//B
	{110,		122,		149,		140,198},	//P
};
static const uint8 initValue_mvp_lx[1]					= { 168 };
static const uint8 initValue_rqt_root_cbf[1]			= { 79 };
static const uint8 initValue_ref_idx_lX[2]				= { 153,153 };
static const uint8 initValue_inter_pred_idc[5]			= { 95,79,63,31,31 };

#ifdef HEVC_3D
static const uint8 initValue_3d[3][12] = {
	//skip_intra	no_dim		depth_intra_mode_idx	skip_intra_mode_idx		dbbp	dc_only		iv_res_pred_weight_idx	illu_comp	depth_dc_present	depth_dc_abs
	{185,			154,		154,					137,					154,	154,		162,153,162,			154,		0,					154},	//B
	{185,			141,		154,					137,					154,	154,		162,153,162,			154,		0,					154},	//P
	{185,			155,		154,					137,					154,	154,		162,153,162,			0,			64,					154},	//I
};
#endif

void initialize_CABAC_models(context_model model[CM_TABLE_LENGTH], SliceType type, int QPY) {
	QPY = clamp(QPY, 0, 51);

	if (type != SLICE_TYPE_I) {
		init_contexts(QPY, model,
			CM_CU_SKIP,				initValue_cu_skip[type],
			CM_MERGE,				initValue_motion[type],
			CM_MVP_LX,				initValue_mvp_lx,
			CM_RQT_ROOT_CBF,		initValue_rqt_root_cbf,
			CM_REF_IDX_LX,			initValue_ref_idx_lX,
			CM_INTER_PRED_IDC,		initValue_inter_pred_idc,
			CM_RDPCM,				repeat(139, 2),
			CM_RDPCM_DIR,			repeat(139, 2)
		);
	}

	init_contexts(QPY, model,
		CM_SPLIT_CU,								initValue_split_cu[type],
		CM_PART_MODE,								initValue_part_mode[type],
		CM_PREV_INTRA_LUMA_PRED,					initValue_prev_intra_luma_pred[type],
		CM_INTRA_CHROMA_PRED_MODE,					initValue_intra_chroma_pred_mode[type],
		CM_CBF_LUMA,								initValue_cbf_luma[type == SLICE_TYPE_I],
		CM_CBF_CHROMA,								initValue_cbf_chroma[type],
		CM_SPLIT_TRANSFORM,							initValue_split_transform[type],
		CM_LAST_SIGNIFICANT_COEFFICIENT_X_PREFIX,	initValue_last_significant_coefficient_prefix[type],
		CM_LAST_SIGNIFICANT_COEFFICIENT_Y_PREFIX,	initValue_last_significant_coefficient_prefix[type],
		CM_CODED_SUB_BLOCK,							initValue_coded_sub_block[type],
		CM_SIGNIFICANT_COEFF,						initValue_significant_coeff[type],

		CM_COEFF_ABS_LEVEL_GREATER1,				initValue_coeff_abs_level_greater1[type],
		CM_COEFF_ABS_LEVEL_GREATER2,				initValue_coeff_abs_level_greater2[type],
		CM_SAO_MERGE,								uint8(153),
		CM_SAO_TYPE_IDX,							initValue_sao_type_idx_lumaChroma[type],
		CM_CU_QP_DELTA_ABS,							repeat(154, 2),
		CM_TRANSFORM_SKIP,							repeat(139, 2),
		CM_CU_TRANSQUANT_BYPASS,					uint8(154),
		CM_LOG2_RES_SCALE_ABS_PLUS1,				repeat(154, 8),
		CM_RES_SCALE_SIGN,							repeat(154, 2),
		CM_CU_CHROMA_QP_OFFSET,						repeat(154, 1),
		CM_CU_CHROMA_QP_OFFSET_IDX,					repeat(154, 1),

	#ifdef HEVC_3D
		CM_SKIP_INTRA,								initValue_3d[type],
	#endif
		CM_TU_TU_RESIDUAL_ACT,						repeat(154, 12)
	);
}

struct CM_table0 : public refs<CM_table0> {
	context_model	model[CM_TABLE_LENGTH];
};

class CM_table : public ref_ptr<CM_table0> {
public:
	using ref_ptr<CM_table0>::ref_ptr;
	void	init(SliceType type, int QPY) {
		emplace();
		initialize_CABAC_models(p->model, type, QPY);
	}
	void	decouple() {
		emplace(*p);
	}
	context_model& operator[](int i)	{ return p->model[i]; }
};

//-----------------------------------------------------------------------------
// SEI
//-----------------------------------------------------------------------------

SEI *SEI::read0(const_memory_block data) {
	const uint8	*p = data;

	uint64 v = 0;
	while (*p == 255)
		v += *p++;

	auto	type = (TYPE)(v + *p++);

	ISO_OUTPUTF("SEI=") << type << '\n';

	v = 0;
	while (*p == 255)
		v += *p++;
	v += *p++;

	memory_reader	r({p, v});

	if (auto funcs = functions::get(type))
		return funcs->load(r);

	auto	sei	= new T1<(TYPE)-1>;
	sei->type	= type;
	sei->read(r);
	return sei;
}

//-----------------------------------------------------------------------------
//	decoder_tables
//-----------------------------------------------------------------------------

struct decoder_tables {
	atomic<int>	refs	= 0;
	uint8		*idx_maps[4 /* 4-log2-32 */][2 /* !!cIdx */][2 /* !!scanIdx */][4 /* prev */];
	uint16		*scan[4][6];
	uint16		*scanpos[3][6];
	uint16		*wedge_patterns[3];
	uint16		num_wedge_bits[3];

	void init_significant_coeff_ctxIdx_lookupTable(uint8 *p);
	void init_scan_orders(uint16 *p);
	void init_scan_pos(uint16 *p);
	void init_wedge_patterns();

	void addref() {
		if (refs++ == 0) {
			int		tableSize = 4 * 4 * (2) + 8 * 8 * (2 * 2 * 4) + 16 * 16 * (2 * 4) + 32 * 32 * (2 * 4);
			auto	p = new uint8[tableSize];
			memset(p, 0xFF, tableSize);	// just for debugging
			init_significant_coeff_ctxIdx_lookupTable(p);

			init_scan_orders(new uint16[(2 * 2 + 4 * 4 + 8 * 8 + 16 * 16 + 32 * 32) * 4]);
			init_scan_pos(new uint16[(4 * 4 + 8 * 8 + 16 * 16 + 32 * 32) * 3]);

			init_wedge_patterns();
		}
	}

	void release() {
		if (--refs == 0) {
			delete[] idx_maps[0][0][0][0];
			delete[] scan[0][1];
			delete[] scanpos[0][0];
			delete[] wedge_patterns[0];
		}
	}

	const uint16*	get_scan_order(int log2BlockSize, ScanOrder scanIdx) const {
		return scan[scanIdx][log2BlockSize];
	}
	uint16			get_scan_position(int x, int y, ScanOrder scanIdx, int log2BlkSize) const {
		return scanpos[scanIdx][log2BlkSize][(y << log2BlkSize) + x];
	}

	const uint16*	get_wedge_pattern(int log2BlockSize, int wedge_idx) const {
		if (log2BlockSize < 2)
			return nullptr;
		log2BlockSize = min(log2BlockSize, 4);
		return wedge_patterns[log2BlockSize - 2] + (wedge_idx << log2BlockSize);
	}
	uint16	get_wedge_pattern_bits(int log2BlockSize) const {
		return log2BlockSize < 2 ? 0 : num_wedge_bits[min(log2BlockSize, 4) - 2];
	}
};


void decoder_tables::init_scan_orders(uint16 *p) {
	static uint16 scan0 = zero;

	scan[SCAN_DIAG][0]		= &scan0;
	scan[SCAN_HORIZ][0]		= &scan0;
	scan[SCAN_VERT][0]		= &scan0;
	scan[SCAN_TRAVERSE][0]	= &scan0;

	for (int log2size = 1; log2size < 6; log2size++) {
		uint32	size	= 1 << log2size;

		uint16* d	= scan[SCAN_DIAG][log2size] = p;
		p	+= size * size;
		uint16* h	= scan[SCAN_HORIZ][log2size] = p;
		p	+= size * size;
		uint16* v	= scan[SCAN_VERT][log2size] = p;
		p	+= size * size;
		uint16* t	= scan[SCAN_TRAVERSE][log2size] = p;
		p	+= size * size;

		for (uint8 y = 0; y < size; y++) {
			int	txor =  (y << log2size) | (y & 1 ? bits(log2size) : 0);
			for (uint8 x = 0; x < size; x++) {
				*h++ = x + (y << log2size);	//horiz
				*v++ = y + (x << log2size);	//vert
				*t++ = x ^ txor;			//traverse
			}
		}

		//diag
		int		x = 0, y = 0;
		for (auto end = d + size * size; d < end; ) {
			while (y >= 0) {
				if (x < size && y < size)
					*d++ = x + (y << log2size);
				y--;
				x++;
			}
			y = x;
			x = 0;
		}
	}
}

void decoder_tables::init_scan_pos(uint16 *p) {
	for (int scanIdx = 0; scanIdx < 3; scanIdx++) {
		for (int log2size = 2; log2size < 6; log2size++) {
			scanpos[scanIdx][log2size] = p;

			uint32	size	= 1 << (log2size * 2);
			for (int i = 0; i < size; i++) {
				int	pos	= scan[scanIdx][2][i & 15];
				int sub	= scan[scanIdx][log2size - 2][i >> 4];

				int	x	= (pos & 3) + ((sub & bits(log2size - 2)) << 2);
				int y	= (pos >> 2) + ((sub >> (log2size - 2)) << 2);

				p[x + (y << log2size)] = i;
			}
			p += size;
		}
	}
}

void decoder_tables::init_significant_coeff_ctxIdx_lookupTable(uint8 *p) {
	// --- Set pointers to memory areas. Note that some parameters share the same memory ---

	static const uint8 ctx4x4[16] = {
		0,1,4,5,
		2,3,4,5,
		6,6,8,8,
		7,7,8,99
	};

	// 4x4	- ignore scan & prev
	for (int c = 0; c < 2; c++) {
		for (int i = 0; i <16; i++)
			p[i] = ctx4x4[i] + c * 27;

		for (int s = 0; s < 2; s++)
			for (int prev = 0; prev < 4; prev++)
				idx_maps[0][c][s][prev] = p;
		p += 4 * 4;
	}

	// 8x8
	for (int c = 0; c < 2; c++)
		for (int s = 0; s < 2; s++)
			for (int prev = 0; prev < 4; prev++) {
				idx_maps[1][c][s][prev] = p;
				p += 8 * 8;
			}

	// 16x16 - ignore scan
	for (int c = 0; c < 2; c++)
		for (int prev = 0; prev < 4; prev++) {
			for (int s = 0; s < 2; s++)
				idx_maps[2][c][s][prev] = p;
			p += 16 * 16;
		}

	// 32x32 - ignore scan
	for (int c = 0; c < 2; c++)
		for (int prev = 0; prev < 4; prev++) {
			for (int s = 0; s < 2; s++)
				idx_maps[3][c][s][prev] = p;
			p += 32 * 32;
		}

	// --- precompute ctxIdx tables ---
	for (int log2w = 3; log2w <= 5 ; log2w++) {
		int		w	= 1 << log2w;
		for (int c = 0; c < 2; c++) {
			int	ctx_offset	= c == 0 ? 21 : 12;

			for (int s = 0; s < 2; s++) {
				if (log2w == 3)
					ctx_offset	= c || s == 0 ? 9 : 15;

				for (int prev = 0; prev < 4; prev++) {
					for (int yC = 0; yC < w; yC++) {
						for (int xC = 0; xC < w; xC++) {
							int xS = xC >> 2,	yS = yC >> 2,
								xP = xC & 3,	yP = yC & 3;

							int ctx = 0;

							if (xC + yC) {
								switch (prev) {
									default:	ctx = xP+yP >= 3 ? 0 : xP+yP > 0 ? 1 : 2;	break;
									case 1:		ctx = yP == 0 ? 2 : yP == 1 ? 1 : 0;		break;
									case 2:		ctx = xP == 0 ? 2 : xP == 1 ? 1 : 0;		break;
									case 3:		ctx = 2;									break;
								}

								ctx	+= (c == 0 && xS + yS > 0 ? 3 : 0) + ctx_offset;
							}

							if (c)
								ctx += 27;

							int	i = (xS + (yS << (log2w - 2))) * 16 + (xP + yP * 4);
							//int	i = xC + (yC << log2w);

							if (idx_maps[log2w - 2][c][s][prev][i] != 0xFF)
								ISO_ASSERT(idx_maps[log2w - 2][c][s][prev][i] == ctx);

							idx_maps[log2w - 2][c][s][prev][i] = ctx;
						}
					}
				}
			}
		}
	}
}

//I.6.6.1 Wedgelet partition pattern generation process
static void partition_pattern(int log2size, uint16 *pattern, int res_shift, int Sx, int Sy, int Ex, int Ey) {
	int	size = 1 << log2size;
	for (int y = 0; y < size; y++)
		pattern[y]	= y < (Sy >> res_shift) ? bits(size) : 0;

	int		deltax	= abs(Ex - Sx);
	int		deltay	= abs(Ey - Sy);

	if (deltay > deltax) {
		int		dx		= sign(Ex - Sx);
		int		error	= -deltay;
		while (Sy <= Ey) {
			pattern[Sy >> res_shift] |= bits((Sx >> res_shift) + 1);
			error += deltax << 1;
			if (error >= 0) {
				error	-= deltay << 1;
				Sx		+= dx;
			}
			++Sy;
		}

	} else {
		if (Sx > Ex) {
			swap(Sx, Ex);
			swap(Sy, Ey);
		}

		int		dy		= sign(Ey - Sy);
		int		error	= -deltax;
		while (Sx <= Ex) {
			pattern[Sy >> res_shift] |= bits((Sx >> res_shift) + 1);
			error += deltay << 1;
			if (error >= 0) {
				error	-= deltax << 1;
				Sy		+= dy;
			}
			++Sx;
		}
	}
}

static void rotate_pattern(int log2size, uint16 *out, const uint16 *in) {
	int	size = 1 << log2size;
	for (int y = 0; y < size; y++)
		out[y]	= 0;

	for (int y = 0; y < size; y++) {
		uint16	*p		= out;
		uint8	shift	= size - 1 - y;
		for (auto b = in[y] ^ bits(size); b; b >>= 1, ++p)
			*p |= (b & 1) << shift;
	}
}

//I.6.6.2 Wedgelet partition pattern table insertion process
static bool unique_pattern(int log2size, const uint16 *pattern, uint16 *patterns_start, uint16 *patterns_end) {
	int		size		= 1 << log2size;

	//all 1s or 0s?
	uint16	first	= pattern[0] ? bits(size) : 0;
	bool	valid	= false;
	for (int y = 0; y < size; y++) {
		if (pattern[y] != first)
			valid = true;
	}
	if (!valid)
		return false;

	for (auto k = patterns_start; k != patterns_end; k += size) {
		bool	identical		= true;
		bool	inv_identical	= true;
		for (int y = 0; y < size; y++) {
			if (pattern[y] != k[y])
				identical	= false;
			if (pattern[y] != (k[y] ^ bits(size)))
				inv_identical = false;
		}
		if (identical || inv_identical)
			return false;
	}

	return true;
}

void decoder_tables::init_wedge_patterns() {
	dynamic_array<uint16>	data;
	int	num_wedge_patterns[3];

	//I.6.6 Derivation process for a wedgelet partition pattern table
	for (int i = 0; i < 3; i++) {
		int		log2size	= i + 2;
		int		size		= 1 << log2size;
		int		res_shift	= log2size > 3 ? 0 : 1;
		int		sizeScaleS	= 2 - res_shift;
		int		wBlkSize	= 1 << (log2size + res_shift);

		auto	patterns_start	= data.size();
		auto	start			= patterns_start;
		auto	p				= data.expand(size);

		for (int ori = 0; ori < 6; ori++) {
			auto	end	= data.size() - size;

			if (ori == 0) {
				for (int m = 0; m < wBlkSize; m += sizeScaleS) {
					for (int n = 0; n < wBlkSize; n += sizeScaleS) {
						partition_pattern(log2size, p, res_shift, m, 0, 0, n);
						if (unique_pattern(log2size, p, data + patterns_start, p))
							p	= data.expand(size);
					}
				}

			} else if (ori == 4) {
				for (int m = 0; m < wBlkSize; m += sizeScaleS) {
					for (int n = 0; n < wBlkSize; n++) {
						partition_pattern(log2size, p, res_shift, m, 0, n, wBlkSize - 1);
						if (unique_pattern(log2size, p, data + patterns_start, p))
							p	= data.expand(size);
					}
				}

			} else {
				for (auto cur = start;  cur < end; cur += size) {
					rotate_pattern(log2size, p, data + cur);
					if (unique_pattern(log2size, p, data + patterns_start, p))
						p	= data.expand(size);
				}
			}
			start	= end;
		}

		data.resize(data.size() - size);

		num_wedge_patterns[i] = data.size() - patterns_start;
	}

	uint16	*p	= data.detach().begin();
	for (int i = 0; i < 3; i++) {
		wedge_patterns[i]	= p;
		num_wedge_bits[i]	= log2_ceil(num_wedge_patterns[i]) - (i + 2);
		p += num_wedge_patterns[i];
	}
}


decoder_tables	tables;

//-----------------------------------------------------------------------------
//	IntraPed
//-----------------------------------------------------------------------------

AVAIL	avail_quad(AVAIL avail, int i)	{
	switch (i) {
		case 0: return avail_quad0(avail);
		case 1: return avail_quad1(avail);
		case 2: return avail_quad2(avail);
		case 3: return avail_quad3(avail);
		default: return avail;
	}
}

void get_intrapred_candidates(IntraPredMode list[3], IntraPredMode mode_a, IntraPredMode mode_b) {
	if (mode_a == mode_b) {
		if (mode_a < 2) {
			list[0] = INTRA_PLANAR;
			list[1] = INTRA_DC;
			list[2] = INTRA_ANGLE_24;
		} else {
			list[0] = mode_a;
			list[1] = from_angle(to_angle(mode_a) - 1);
			list[2] = from_angle(to_angle(mode_a) + 1);
		}
	} else {
		list[0] = mode_a;
		list[1] = mode_b;
		list[2] = mode_a != INTRA_PLANAR	&& mode_b != INTRA_PLANAR	? INTRA_PLANAR
				: mode_a != INTRA_DC		&& mode_b != INTRA_DC		? INTRA_DC
				: INTRA_ANGLE_24;
	}
}

constexpr ScanOrder _get_intra_scan_idx(IntraPredMode intra_pred) {
	return	between(intra_pred, INTRA_ANGLE_4, INTRA_ANGLE_12) ? SCAN_VERT
		:	between(intra_pred, INTRA_ANGLE_20,INTRA_ANGLE_28) ? SCAN_HORIZ
		:	SCAN_DIAG;
}

constexpr ScanOrder get_intra_scan_idx(int log2TrSize, IntraPredMode intra_pred, int c, CHROMA ChromaArrayType) {
	return intra_pred != INTRA_UNKNOWN && (log2TrSize==2 || (log2TrSize==3 && (c==0 || ChromaArrayType==CHROMA_444))) ? _get_intra_scan_idx(intra_pred) : SCAN_DIAG;
}

IntraPredMode lumaPredMode_to_chromaPredMode(IntraPredMode luma, IntraChromaMode chroma) {
	switch (chroma) {
		case INTRA_CHROMA_LIKE_LUMA:		return luma;
		case INTRA_CHROMA_PLANAR_OR_34:		return luma == INTRA_PLANAR		? INTRA_ANGLE_32 : INTRA_PLANAR;
		case INTRA_CHROMA_ANGLE_24_OR_32:	return luma == INTRA_ANGLE_24	? INTRA_ANGLE_32 : INTRA_ANGLE_24;
		case INTRA_CHROMA_ANGLE_8_OR_32:	return luma == INTRA_ANGLE_8	? INTRA_ANGLE_32 : INTRA_ANGLE_8;
		case INTRA_CHROMA_DC_OR_34:			return luma == INTRA_DC			? INTRA_ANGLE_32 : INTRA_DC;
	}
	ISO_ASSERT(false);
	return INTRA_DC;
}

IntraPredMode map_chroma_pred_mode(IntraChromaMode intra_chroma, IntraPredMode intra_luma) {
	if (intra_chroma == INTRA_CHROMA_LIKE_LUMA)
		return intra_luma;

	static const IntraPredMode IntraPredModeCCand[4] = {
		INTRA_PLANAR,
		INTRA_ANGLE_24, // vertical
		INTRA_ANGLE_8,	// horizontal
		INTRA_DC
	};

	auto intra_c = IntraPredModeCCand[intra_chroma];
	return intra_c == intra_luma ? INTRA_ANGLE_32 : intra_c;
}

IntraPredMode map_chroma_pred_mode(IntraChromaMode intra_chroma, IntraPredMode intra_luma, CHROMA chroma) {
	// h.265-V2 Table 8-3
	static const uint8 map_chroma_422[35] = {
		INTRA_PLANAR,	INTRA_DC,
		INTRA_ANGLE_0,	INTRA_ANGLE_0,	INTRA_ANGLE_0,	INTRA_ANGLE_0,
		INTRA_ANGLE_1,	INTRA_ANGLE_3,	INTRA_ANGLE_5,	INTRA_ANGLE_6,
		INTRA_ANGLE_8,	INTRA_ANGLE_10,	INTRA_ANGLE_11,	INTRA_ANGLE_13,
		INTRA_ANGLE_15,	INTRA_ANGLE_16,	INTRA_ANGLE_17,	INTRA_ANGLE_18,
		INTRA_ANGLE_19,	INTRA_ANGLE_20,	INTRA_ANGLE_21,	INTRA_ANGLE_21,
		INTRA_ANGLE_22,	INTRA_ANGLE_22,	INTRA_ANGLE_23,	INTRA_ANGLE_23,
		INTRA_ANGLE_24,	INTRA_ANGLE_25,	INTRA_ANGLE_25,	INTRA_ANGLE_26,
		INTRA_ANGLE_26,	INTRA_ANGLE_27,	INTRA_ANGLE_27,	INTRA_ANGLE_28,
		INTRA_ANGLE_29,
	};

	auto	mode = map_chroma_pred_mode(intra_chroma, intra_luma);
	return chroma == CHROMA_422 ? (IntraPredMode)map_chroma_422[mode] : mode;

	//if (chroma != CHROMA_422)
	//	return map_chroma_pred_mode(intra_chroma, intra_luma);
	//return  (IntraPredMode)map_chroma_422[map_chroma_pred_mode(intra_chroma, intra_luma)];
}

//-----------------------------------------------------------------------------
//	Motion
//-----------------------------------------------------------------------------

MotionVector scale_mv(MotionVector mv, int16x2 scale) {
	auto	temp	= fullmul(mv, scale);
	return to_sat<int16>(sign(temp) * ((abs(temp) + 127) >> 8));
}

void scale_mv(MotionVector &mv, int coDist, int currDist) {
	if (coDist && currDist && coDist != currDist) {
	#if 1
		int		tb		= clamp(currDist, -128, 127);
		int		td		= clamp(coDist, -128, 127);
		int		x        = (0x4000 + abs(td / 2)) / td;
		int		scale    = clamp((tb * x + 32) >> 6, -4096, 4095);
	#else
		int		td		= clamp(coDist, -128, 127);
		int		scale	= clamp((clamp(currDist, -128, 127) * (16384 + (abs(td) >> 1)) / td + 32) >> 6, -4096, 4095);
	#endif
		mv	= scale_mv(mv, scale);
	}
}

bool check_merge(int16x2 a, int16x2 b, int merge_level) {
	return any(a >> merge_level != b >> merge_level);
}

string_accum &operator<<(string_accum &sa, const PB_info &vi) {
	if (vi.refIdx[0] >= 0)
		sa.format("a(%i):(%i,%i)%s", vi.refIdx[0], vi.mv[0].x, vi.mv[0].y, vi.refIdx[1] >= 0 ? " & " : "");

	if (vi.refIdx[1] >= 0)
		sa.format("b(%i):(%i,%i)", vi.refIdx[1], vi.mv[1].x, vi.mv[1].y);

	return sa << '\n';
}

struct PBMotionCoding {
	int				merge_index;
	int8			refIdx[2];	// index into RefPicList (-1 -> not used)
	MotionVector	mvd[2];		// motion vector difference
	bool			mvp[2];		// which of the two MVPs is used
	constexpr bool	is_merge() const { return merge_index >= 0; }
};

//-----------------------------------------------------------------------------
//	Weights
//-----------------------------------------------------------------------------

bool read_weights(bitreader &r, range<pred_weight(*)[3]> weights, uint8 bit_depth_luma, uint8 bit_depth_chroma, uint8 luma_log2_weight_denom, uint8 chroma_log2_weight_denom, bool high_prec) {
	auto	n			= weights.size();
	uint32	has			= r.get(n);
	if (bit_depth_chroma)
		has |= r.get(n) << 16;

	has <<= 16 - n;

	int	range_luma		= high_prec ? 1 << (bit_depth_luma - 1) : 128;
	int	luma_shift		= high_prec ? 0 : bit_depth_luma - 8;

	int	range_chroma	= high_prec ? 1 << (bit_depth_chroma - 1) : 128;
	int	chroma_shift	= high_prec ? 0 : bit_depth_chroma - 8;

	for (auto i : weights) {
		if (has & 0x8000) {
			auto	t = r.gets();
			if (!between(t, -128, 127))
				return false;
			i[0].weight = (1 << luma_log2_weight_denom) + t;

			t = r.gets();
			if (!between(t, -range_luma, range_luma - 1))
				return false;
			i[0].offset = t << luma_shift;

		} else {
			i[0].weight = 1 << luma_log2_weight_denom;
			i[0].offset = 0;
		}

		for (int j = 1; j < 3; j++) {
			if (has & 0x80000000) {
				auto	t = r.gets();
				if (!between(t, -128, 127))
					return false;
				i[j].weight = (1 << chroma_log2_weight_denom) + t;

				t = r.gets();
				if (!between(t, -range_chroma * 4, range_chroma * 4 - 1))
					return false;
				i[j].offset = clamp(range_chroma + t - ((range_chroma * i[j].weight) >> chroma_log2_weight_denom), -range_chroma, range_chroma - 1) << chroma_shift;

			} else {
				i[j].weight = 1 << chroma_log2_weight_denom;
				i[j].offset = 0;
			}
		}
		has	<<= 1;
	}
	return true;
}

//-----------------------------------------------------------------------------
// ReferenceSets
//-----------------------------------------------------------------------------

bool LongTermReferenceSet::read(bitreader& r, const POCreference *refs, int nrefs, int lsb_bits) {
	auto	num_long_term_sps	= nrefs ? r.getu() : 0;
	auto	num_long_term_pics	= r.getu();

	if (num_long_term_sps + num_long_term_pics > capacity())
		return false;

	clear();

	auto	delta_poc_msb	= 0;
	int		nBits			= log2_ceil(nrefs);
	while (num_long_term_sps--) {
		uint32	idx = r.get(nBits);
		if (idx >= nrefs)
			return false;

		auto	&i = push_back(refs[idx]);
		if (i.has_delta_msb = r.get_bit()) {
			delta_poc_msb	+= r.getu();
			i.poc -= delta_poc_msb << lsb_bits;
		}
	}

	delta_poc_msb	= 0;
	while (num_long_term_pics--) {
		auto	&i = push_back();
		i.read(r, lsb_bits);
		if (i.has_delta_msb = r.get_bit()) {
			delta_poc_msb	+= r.getu();
			i.poc -= delta_poc_msb << lsb_bits;
		}
	}

	return true;
}

bool ShortTermReferenceSet::read(bitreader &r, range<const ShortTermReferenceSet*> sets, bool slice) {
	clear(neg);
	clear(pos);

	// --- is this set coded in prediction mode (not possible for the first set)
	if (!sets.empty() && r.get_bit()) {
		// Only for the last ref_pic_set (that's the one coded in the slice header), we can specify relative to which reference set we code the set.
		int delta_idx = slice ? r.getu() + 1 : 1;
		if (delta_idx > sets.size())
			return false;

		auto&	src_set		= sets.end()[-delta_idx];// this is our source set
		bool	sign		= r.get_bit();
		int		delta_poc	= plus_minus(r.getu() + 1, sign);

		// bits are stored in this order:
		// - all bits for negative Pocs (forward),
		// - then all bits for positive Pocs (forward),
		// - then bits for '0', shifting of the current picture

		int		src_neg			= src_set.neg.size();
		int		src_pos			= src_set.pos.size();
		int		src_total		= src_neg + src_pos;

		auto used_by_curr_pic	= alloc_auto(bool, src_total + 1);
		auto use_delta			= alloc_auto(bool, src_total + 1);

		for (int j = 0; j <= src_total; j++) {
			used_by_curr_pic[j]	= r.get_bit();
			use_delta[j]		= used_by_curr_pic[j] || r.get_bit();
		}

		// --- update list 0 (negative Poc) ---
		// Iterate through all Pocs in decreasing value order (positive reverse, 0, negative forward).

		// positive list
		for (int j = src_pos - 1; j >= 0; j--) {
			int poc = src_set.pos[j].poc + delta_poc; // new delta
			if (poc < 0 && use_delta[src_neg + j]) {
				if (neg.full())
					return false;
				neg.push_back({poc, used_by_curr_pic[src_neg + j]});
			}
		}

		// frame 0
		if (delta_poc < 0 && use_delta[src_total]) {
			if (neg.full())
				return false;
			neg.push_back({delta_poc, used_by_curr_pic[src_total]});
		}

		// negative list
		for (int j = 0; j < src_neg; j++) {
			int poc = src_set.neg[j].poc + delta_poc;
			if (poc < 0 && use_delta[j]) {
				if (neg.full())
					return false;
				neg.push_back({poc, used_by_curr_pic[j]});
			}
		}

		// --- update list 1 (positive Poc) ---
		// Iterate through all Pocs in increasing value order (negative reverse, 0, positive forward)

		// negative list
		for (int j = src_neg - 1; j >= 0; j--) {
			int poc = src_set.neg[j].poc + delta_poc;
			if (poc > 0 && use_delta[j]) {
				if (pos.full())
					return false;
				pos.push_back({poc, used_by_curr_pic[j]});
			}
		}

		// frame 0
		if (delta_poc > 0 && use_delta[src_total]) {
			if (pos.full())
				return false;
			pos.push_back({delta_poc, used_by_curr_pic[src_total]});
		}

		// positive list
		for (int j = 0; j < src_pos; j++) {
			int poc = src_set.pos[j].poc + delta_poc;
			if (poc > 0 && use_delta[src_neg+j]) {
				if (pos.full())
					return false;
				pos.push_back({poc, used_by_curr_pic[src_neg + j]});
			}
		}

	} else {
		// --- first, read the number of past and future frames in this set ---
		uint32	num_neg = r.getu();
		uint32	num_pos = r.getu();

		// total number of reference pictures may not exceed buffer capacity
		if (num_neg + num_pos > MAX_NUM_REF_PICS)
			return false;

		// --- now, read the deltas between the reference frames to fill the lists ---
		int poc = 0;
		for (int i = 0; i < num_neg; i++) {
			poc -= r.getu() + 1;
			neg.push_back({poc, r.get_bit()});
		}

		poc = 0;
		for (int i = 0; i < num_pos; i++) {
			poc += r.getu() + 1;
			pos.push_back({poc, r.get_bit()});
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// scaling
//-----------------------------------------------------------------------------

uint8 ScalingList::default_4x4[16] = {
	16,16,16,16,
	16,16,16,16,
	16,16,16,16,
	16,16,16,16
};

uint8 ScalingList::default_8x8_intra[64] = {
	16,16,16,16,16,16,16,16,
	16,16,17,16,17,16,17,18,
	17,18,18,17,18,21,19,20,
	21,20,19,21,24,22,22,24,
	24,22,22,24,25,25,27,30,
	27,25,25,29,31,35,35,31,
	29,36,41,44,41,36,47,54,
	54,47,65,70,65,88,88,115
};

uint8 ScalingList::default_8x8_inter[64] = {
	16,16,16,16,16,16,16,16,
	16,16,17,17,17,17,17,18,
	18,18,18,18,18,20,20,20,
	20,20,20,20,24,24,24,24,
	24,24,24,24,25,25,25,25,
	25,25,25,28,28,28,28,28,
	28,33,33,33,33,33,41,41,
	41,41,54,54,54,71,71,91
};

template<int N> void fill_scaling_factor(uint8 *dest, const uint8* srce);

template<> void fill_scaling_factor<0>(uint8 *dest, const uint8* srce) {
	const uint16* scan = tables.get_scan_order(2, SCAN_DIAG);
	for (int i = 0; i < 4 * 4; i++)
		dest[scan[i]] = srce[i];
}

template<> void fill_scaling_factor<1>(uint8 *dest, const uint8* srce) {
	const uint16* scan = tables.get_scan_order(3, SCAN_DIAG);
	for (int i = 0; i < 8 * 8; i++)
		dest[scan[i]] = srce[i];
}

template<> void fill_scaling_factor<2>(uint8 *dest, const uint8* srce) {
	const uint16* scan = tables.get_scan_order(3, SCAN_DIAG);
	for (int i = 0; i < 8 * 8; i++) {
		int	s	= ((scan[i] & 0x07) << 1) + ((scan[i] & 0x38) << 2);
		for (int dy = 0; dy < 2; dy++, s += 16)
			for (int dx = 0; dx < 2; dx++)
				dest[s + dx] = srce[i];
	}
}

template<> void fill_scaling_factor<3>(uint8 *dest, const uint8* srce) {
	const uint16* scan = tables.get_scan_order(3, SCAN_DIAG);
	for (int i = 0; i < 8 * 8; i++) {
		int	s	= ((scan[i] & 0x07) << 2) + ((scan[i] & 0x38) << 4);
		for (int dy = 0; dy < 4; dy++, s += 32)
			for (int dx = 0; dx < 4; dx++)
				dest[s + dx] = srce[i];
	}
}

bool ScalingList::read(bitreader &r) {
	uint8	scaling_lists[6][64];
	int		dc_coeff[6];

	for (int sizeId = 0; sizeId < 4; sizeId++) {
		int		nCoeffs = sizeId == 0 ? 16 : 64;

		for (int m = 0; m < 6; m++) {
			uint8*	scaling_list = scaling_lists[m];

			if (sizeId == 3 && m % 3) {
				//don't read - previous size's result is in the same array
			} else {
				if (!r.get_bit()) {
					int delta = r.getu();
					if (sizeId == 3)
						delta *= 3;				// adapt to our changed matrixId for size 3

					if (delta > m)
						return false;

					if (delta == 0) {
						auto	default_scaling_list = sizeId == 0 ? default_4x4 : m < 3 ? default_8x8_intra : default_8x8_inter;
						copy_n(default_scaling_list, scaling_list, nCoeffs);
						dc_coeff[m] = 16;
					} else {
						memcpy(scaling_list, scaling_lists[m - delta], nCoeffs);
						dc_coeff[m] = dc_coeff[m - delta];
					}

				} else {
					uint8	nextCoef	= 8;
					int		dc			= 16;
					if (sizeId > 1) {
						dc = r.gets() + 8;
						if (!between(dc, 1, 255))
							return false;
						nextCoef = dc;
					}
					dc_coeff[m] = dc;

					for (int i = 0; i < nCoeffs; i++) {
						int delta = r.gets();
						if (!between(delta, -128, 127))
							return false;
						scaling_list[i] = (nextCoef += delta + 256);
					}
				}
			}

			// --- generate ScalingFactor arrays ---
			switch (sizeId) {
				case 0:	fill_scaling_factor<0>(size0[m], scaling_list); break;
				case 1: fill_scaling_factor<1>(size1[m], scaling_list); break;
				case 2: fill_scaling_factor<2>(size2[m], scaling_list); size2[m][0] = dc_coeff[m]; break;
				case 3: fill_scaling_factor<3>(size3[m], scaling_list); size3[m][0] = dc_coeff[m]; break;
			}
		}
	}

	return true;
}

void ScalingList::set_default() {
	// 4x4
	for (auto &i : size0)
		fill_scaling_factor<0>(i, default_4x4);

	for (int i = 0; i < 3; i++) {
		// 8x8
		fill_scaling_factor<1>(size1[i + 0], default_8x8_intra);
		fill_scaling_factor<1>(size1[i + 3], default_8x8_inter);
		// 16x16
		fill_scaling_factor<2>(size2[i + 0], default_8x8_intra);
		fill_scaling_factor<2>(size2[i + 3], default_8x8_inter);
		// 32x32
		fill_scaling_factor<3>(size3[i + 0], default_8x8_intra);
		fill_scaling_factor<3>(size3[i + 3], default_8x8_inter);
	}
}

//-----------------------------------------------------------------------------
// Deblocking
//-----------------------------------------------------------------------------

static uint8 table_8_23_beta[52] = {
	0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6,  7, 8, 9,10,11,12,13,14,15,16,17,18,20,22,24,
	26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,
	58,60,62,64
};

static uint8 table_8_23_tc[54] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3,
	3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 9,10,11,13,
	14,16,18,20,22,24
};

uint8	Deblocking::beta(int y)	const { return table_8_23_beta[clamp(y + beta_offset, 0, 51)]; }
uint8	Deblocking::tc(int y)	const { return table_8_23_tc[clamp(y + tc_offset, 0, 53)]; }

//-----------------------------------------------------------------------------
// HRD - hypothetical reference decoder
//-----------------------------------------------------------------------------

bool hrd_parameters::sub_pic::read(bitreader& r) { return r.read(tick_divisor, du_cpb_removal_delay_increment_length, sub_pic_cpb_params_in_pic_timing_sei, dpb_output_delay_du_length); }

void hrd_parameters::layer::nalvcl::read(bitreader& r, bool sub_pic) {
	r.read(bit_rate, cpb_size);
	if (sub_pic)
		r.read(cpb_size_du, bit_rate_du);
	cbr = r.get_bit();
}

void hrd_parameters::layer::read(bitreader& r) {
	fixed_pic_rate_general		= r.get_bit();
	fixed_pic_rate_within_cvs	= fixed_pic_rate_general || r.get_bit();

	low_delay_hrd	= false;
	cpb_cnt			= 1;

	if (fixed_pic_rate_within_cvs)
		elemental_duration_in_tc = r.getu() + 1;
	else
		low_delay_hrd = r.get_bit();

	if (!low_delay_hrd)
		cpb_cnt = r.getu() + 1;
}

bool hrd_parameters::read(bitreader& r, bool common, int max_sub_layers) {
	if (common) {
		r.read(nal_present, vcl_present);

		if (nal_present || vcl_present) {
			r.read(sub_pic, bit_rate_scale, cpb_size_scale);

			if (sub_pic.exists())
				sub_pic->cpb_size_du_scale = r.get(4);

			r.read(initial_cpb_removal_delay_length, au_cpb_removal_delay_length, dpb_output_delay_length);
		}
	}

	bool	has_sub_pic	= sub_pic.exists();
	for (int i = 0; i < max_sub_layers; i++) {
		layers[i].read(r);
		if (nal_present) {
			for (auto &j : make_range_n(layers[i].nal, layers[i].cpb_cnt))
				j.read(r, has_sub_pic);;
		}
		if (vcl_present) {
			for (auto &j : make_range_n(layers[i].vcl, layers[i].cpb_cnt))
				j.read(r, has_sub_pic);;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// VUI
//-----------------------------------------------------------------------------

bool video_signal_info::colour_description::read(bitreader &r)	{ return r.read(colour_primaries, transfer_characteristics, matrix_coeffs); }
bool video_signal_info::read(bitreader &r)						{ return r.read(format, full_range, colour_description); }
bool video_timing::read(bitreader& r)							{ return r.read(num_units_in_tick, time_scale, num_ticks_poc_diff_one); }

static const uint16x2 sar_presets[] = {
	{ 0,	0 },
	{ 1,	1 },
	{ 12,	11 },
	{ 10,	11 },
	{ 16,	11 },
	{ 40,	33 },
	{ 24,	11 },
	{ 20,	11 },
	{ 32,	11 },
	{ 80,	33 },
	{ 18,	11 },
	{ 15,	11 },
	{ 64,	33 },
	{ 160,	99 },
	{ 4,	3 },
	{ 3,	2 },
	{ 2,	1 }
};

bool video_usability_information::chroma_loc::read(bitreader& r)	{ return r.read(top_field, bottom_field); }
bool video_usability_information::restrictions::read(bitreader& r) {
	return r.read(
		tiles_fixed_structure, motion_vectors_over_pic_boundaries, restricted_ref_pic_lists,
		min_spatial_segmentation_idc, max_bytes_per_pic_denom, max_bits_per_min_cu_denom, log2_max_mv_length_horizontal, log2_max_mv_length_vertical
	);
}
bool video_usability_information::read(bitreader& r, int max_sub_layers) {
	// --- sample aspect ratio (SAR) ---
	if (r.get_bit()) {
		int aspect_ratio_idc = r.get(8);
		if (aspect_ratio_idc < num_elements(sar_presets))
			sar	= sar_presets[aspect_ratio_idc];
		else if (aspect_ratio_idc == 255)
			sar	= uint16x2{r.get(16), r.get(16)};
		else
			sar = uint16x2(zero);
	}

	r.read(overscan_appropriate, video_signal, chroma_loc, neutral_chroma_indication, field_seq, frame_field_info_present);

	if (r.get_bit())
		def_disp_offsets.put()	= {r.getu(), r.getu(), r.getu(), r.getu()};

	// --- timing ---
	if (r.read(timing) && timing.exists() && r.get_bit())
		hrd_parameters.put().read(r, true, max_sub_layers);

	r.read(restrictions);
	return true;
}

//-----------------------------------------------------------------------------
// Profile
//-----------------------------------------------------------------------------

void Profile::init(PROFILE _profile) {
	space			= 0;
	tier			= false;
	profile			= _profile;
	compatibility	= 1 << _profile;

	switch (profile) {
		case MAIN:		compatibility |= 1 << MAIN10; break;
		default:		break;
	}

	flags.clear_all();
}

bool Profile::read(bitreader &r) {
	r.read(space, tier, profile, compatibility);
	for (auto &&i : flags)
		i = r.get_bit();
	return true;
}

bool Profile::write(bitwriter &w) const {
	w.write(space, tier, profile, compatibility);
	for (auto i : flags)
		w.put_bit(i);
	return true;
}

void profile_tier_level::init(Profile::PROFILE _profile, uint8 level) {
	resize(1);
	(*this)[0].init(_profile);
	(*this)[0].level	= level;
}

bool profile_tier_level::read(bitreader &r, bool profile_present, uint32 max_sublayers) {
	resize(max_sublayers);
	(*this)[0].profile_present = profile_present;
	(*this)[0].level_present	= true;

	(*this)[0].read(r);

	for (auto &i : slice(1))
		r.read(i.profile_present, i.level_present);
	if (max_sublayers > 1)
		r.discard((9 - max_sublayers) * 2);

	return r.read(slice(1));
}

bool profile_tier_level::write(bitwriter &w) const {
	(*this)[0].write(w);

	for (auto &i : slice(1))
		w.write(i.profile_present, i.level_present);

	if (size() > 1)
		w.put(0, (9 - size()) * 2);

	return w.write(slice(1));
}

//-----------------------------------------------------------------------------
// Extensions
//-----------------------------------------------------------------------------

#ifdef HEVC_RANGE
//-----------------------------------------------------------------------------
// range
//-----------------------------------------------------------------------------

bool extension_range::sps::read(bitreader &r) {
	return r.read(
		transform_skip_rotation_enabled,
		transform_skip_context_enabled,
		implicit_rdpcm_enabled,
		explicit_rdpcm_enabled,
		extended_precision_processing,
		intra_smoothing_disabled,
		high_precision_offsets_enabled,
		persistent_rice_adaptation_enabled,
		cabac_bypass_alignment_enabled
	);
}

bool extension_range::pps::chroma_qp_offset_list::read(bitreader &r) {
	delta_depth = r.getu();

	auto	len = r.getu() + 1;
	if (len > 6)
		return false;

	resize(len);
	for (auto &i : *this) {
		i = {r.gets(), r.gets()};
		if (!all(between(i, -12, 12)))
			return false;
	}
	return true;
}

bool extension_range::pps::read(bitreader &r, const seq_parameter_set* sps, bool skip_enabled, int bit_depth_luma, int bit_depth_chroma) {
	log2_max_transform_skip_block_size = skip_enabled ? r.getu() + 2 : 2;

	return r.read(cross_component_prediction_enabled, chroma_qp_offsets, log2_sao_offset_scale)
		&& log2_sao_offset_scale[0] <= max(0, bit_depth_luma - 10)
		&& log2_sao_offset_scale[1] <= max(0, bit_depth_chroma - 10);
}
#endif	//HEVC_RANGE


#ifdef HEVC_ML
//-----------------------------------------------------------------------------
// ML
//-----------------------------------------------------------------------------

bool extension_multilayer::vui_layer_set_entry::read(bitreader &r, bool bit_rate_present, bool pic_rate_present) {
	bit_rate_present = bit_rate_present && r.get_bit();
	pic_rate_present = pic_rate_present && r.get_bit();
	if (bit_rate_present) {
		avg_bit_rate		= r.get(16);
		max_bit_rate		= r.get(16);
	}
	if (pic_rate_present) {
		constant_pic_rate	= r.get(2);
		avg_pic_rate		= r.get(16);
	}
	return true;
}

struct coeff_reader {
	uint8				octant_depth, y_part_num_log2;
	uint8				res_quant_bits;
	uint8				delta_flc_bits;
	uint8				rbits;

	coeff_reader(uint8 octant_depth, uint8 y_part_num_log2, int luma_bit_depth_input, int luma_bit_depth_output, uint8 res_quant_bits, uint8 delta_flc_bits)
		: octant_depth(octant_depth), y_part_num_log2(y_part_num_log2)
		, res_quant_bits(res_quant_bits)
		, delta_flc_bits(delta_flc_bits)
		, rbits(max(0, (10 + luma_bit_depth_input - luma_bit_depth_output - res_quant_bits - delta_flc_bits)))
	{}
	int		read_value(bitreader &r) const {
		uint32	v	= r.getu();
		if (rbits)
			v = (v << rbits) + r.get(rbits);
		return plus_minus(v << res_quant_bits, v && r.get_bit());
	}

	bool	read(bitreader &r, int *e, int depth) {
		if (depth < octant_depth && r.get_bit()) {
			auto	offset = (3 * 4) << ((octant_depth - depth - 1) * 3 + y_part_num_log2);
			for (int i = 0; i < 8; i++)
				read(r, e + offset * i, depth + 1);

		} else {
			int	a0	= 0, a1 = 0, a2 = 0;
			for (int i = 0; i < (1 << y_part_num_log2); i++) {
				for (int j = 0; j < 4; j++) {
					if (r.get_bit()) {
						a0 += read_value(r);
						a1 += read_value(r);
						a2 += read_value(r);
					}
					e[0] = a0 + (j == 0 ? 1024 : 0);
					e[4] = a1 + (j == 1 ? 1024 : 0);
					e[8] = a2 + (j == 2 ? 1024 : 0);
					++e;
				}
				e += 12 - 4;
			}
		}
		return true;
	}
};

bool extension_multilayer::colour_mapping::read(bitreader &r) {
	for (int i = 0, n = r.getu() + 1; i < n; i++)
		layer_enable |= bit64(r.get(6));

	u<2>				res_quant_bits;
	as_minus<u<2>,1>	delta_flc_bits;
	se					adapt_threshold_u_delta;
	se					adapt_threshold_v_delta;

	if (r.read(octant_depth, y_part_num_log2, luma_bit_depth_input, chroma_bit_depth_input, luma_bit_depth_output, chroma_bit_depth_output, res_quant_bits, delta_flc_bits)
		&& (octant_depth != 1 || r.read(adapt_threshold_u_delta, adapt_threshold_v_delta))
		) {
		yshift				= luma_bit_depth_input - octant_depth - y_part_num_log2;
		cshift				= chroma_bit_depth_input - octant_depth;
		mapping_shift		= 10 + luma_bit_depth_input - luma_bit_depth_output;
		adapt_threshold		= {(1 << (chroma_bit_depth_input - 1)) + adapt_threshold_u_delta, (1 << (chroma_bit_depth_input - 1)) + adapt_threshold_v_delta};

		coeffs.resize((3 * 4) << (octant_depth * 3 + y_part_num_log2));
		return coeff_reader(octant_depth, y_part_num_log2, luma_bit_depth_input, luma_bit_depth_output, res_quant_bits, delta_flc_bits).read(r, coeffs, 0);
	}
	return false;
}

bool extension_multilayer::pps::read(bitreader &r) {
	r.read(poc_reset_info_present, scaling_list_ref_layer_id);

	regions.resize(r.getu());
	for (auto &i : regions) {
		i.a = r.getu();
		i.b.read(r);
	}

	return r.read(colour_mapping);
}

bool extension_multilayer::shdr::read(bitreader &r, const pps &pps, uint8 log2_max_pic_order_cnt_lsb) {
	if (pps.poc_reset_info_present) {
		poc_reset_idc = r.get(2);

		if (poc_reset_idc != 0)
			poc_reset_period_id = r.get(6);

		if (poc_reset_idc == 3) {
			full_poc_reset = r.get_bit();
			poc_lsb_val = r.get(log2_max_pic_order_cnt_lsb);
		}
	}
	return true;
}

bool extension_multilayer::shdr::read(bitreader &r, const vps *vpsml, const layer *layer, int temporal_id) {
	auto	deps = layer->direct_dependency & (layer->depth_layer() ? vpsml->depth_layers : ~vpsml->depth_layers);

	if (deps) {
		if (vpsml->default_ref_layers_active || r.get_bit()) {
			int			layer_idx	= vpsml->layer_index(layer);
			auto		ndirect		= count_bits(deps);
			int			nbits		= log2_ceil(ndirect);

			int			j = 0;
			uint8		inter_layer_pred_layer[8];
			for (auto i : make_bit_container(deps)) {
				auto&	layer2 = vpsml->layers[i];
				if (layer2.max_sub_layers > temporal_id && (temporal_id == 0 || layer2.max_tid_il_ref_pics_plus1[layer_idx] > temporal_id))
					inter_layer_pred_layer[j++] = i;
			}

			int	inter_layer_refs	= j == 0			?	0
				:	vpsml->default_ref_layers_active	?	j
				:	vpsml->max_one_active_ref_layer || ndirect == 1	? 1
				:	r.get(nbits) + 1;

			if (inter_layer_refs != ndirect) {
				for (int i = 0; i < inter_layer_refs; i++)
					inter_layer_pred_layer[i] = r.get(nbits);
			}

			ref_layers.resize(inter_layer_refs);
			for (int i = 0; i < inter_layer_refs; i++)
				ref_layers[i] = inter_layer_pred_layer[i];
		}
	}
	return true;
}

bool extension_multilayer::vps::read(bitreader &r, bool base_layer_internal) {
	auto	max_sub_layers = layers[0].max_sub_layers;

	profile_tier_levels.resize(1);

	if (layers.size() > 1 && base_layer_internal)
		profile_tier_levels[0].read(r, false, max_sub_layers);

	bool	splitting			= r.get_bit();
	uint16	scalability_mask	= r.get_reverse(16);

	// --- dimension_id ---
	dynamic_array<as_minus<compact<uint8,3>, 1>>	dimension_id_len;
	dimension_id_len.read(r, count_bits(scalability_mask) - splitting);

	layers[0].id_in_nuh	= 0;
	bool id_present	= r.get_bit();
	for (auto &layer : slice(layers, 1)) {
		layer.id_in_nuh = id_present ? r.get(6) : layers.index_of(layer);
		auto	len = dimension_id_len.begin();
		int		bit	= 0;
		for (auto j : make_bit_container(scalability_mask)) {
			if (!splitting) {
				layer.dimension_id[j] = r.get(*len);
			} else {
				layer.dimension_id[j] = (layer.id_in_nuh >> bit) & bits(*len);
				bit += *len;
			}
			++len;
		}
	}

	// --- view ---
	depth_layers	= 0;
	uint64	views	= 0;
	for (auto &i : layers) {
		views			|= bit64(i.view_order_idx());
		depth_layers	|= uint64(i.depth_layer()) << layers.index_of(i);
	}

	view_ids.resize(count_bits(views), 0);
	if (auto nbits = r.get(4))
		for(auto &i : view_ids)
			i = r.get(nbits);

	//--- dependencies ---
	for (auto &i : slice(layers, 1))
		i.direct_dependency = r.get_reverse(layers.index_of(i));

	for (auto &i : layers) {
		i.dependency		= i.direct_dependency;
		for (auto j : make_bit_container(i.direct_dependency))
			i.dependency		|= layers[j].dependency;
		uint64	predicted	= 1 << layers.index_of(i);
		for (auto j : make_bit_container(i.dependency))
			layers[j].predicted |= predicted;
	}

	uint64	layer_in_list	= 0;
	for (auto &i : layers) {
		if (i.direct_dependency == 0) {
			TreePartitionList.push_back((1 << layers.index_of(i)) | (i.predicted & ~layer_in_list));
			layer_in_list	|= i.predicted;
		}
	}

	// --- additional layer sets ---
	auto num_independent_layers = TreePartitionList.size();
	if (num_independent_layers > 1) {
		uint32	num_original	= layer_sets.size32();
		uint32	num_additional	= r.getu();	//0...1023

		layer_sets.resize(num_original + num_additional);

		for (int i = num_original; i < num_original + num_additional; i++) {
			uint64	set = 0;
			for (int j = 1; j < num_independent_layers; j++) {
				auto	t = TreePartitionList[j];
				auto	n = r.get(log2_ceil(count_bits(t) + 1));
				set |= lowest_n_set(t, n);
			}
			layer_sets[i] = set;
		}
	}

	// --- max_sub_layers ---
	if (r.get_bit()) {
		for (auto &layer : layers)
			layer.max_sub_layers = r.get(3) + 1;
	}

	// --- max_tid_il_ref_pics ---
	if (r.get_bit()) {
	#if 1
		uint64	direct_predicted[64] = {0};

		for (auto &i : layers) {
			uint64	predicted = 1 << layers.index_of(i);
			for (auto j : make_bit_container(i.direct_dependency))
				direct_predicted[j] |= predicted;
		}
		
		for (auto &i : layers)
			readn(r, i.max_tid_il_ref_pics_plus1, count_bits(direct_predicted[layers.index_of(i)]));
	#else
		//read transpose
		for (auto &i : layers)
			readn(r, i.max_tid_il_ref_pics_plus1, count_bits(i.direct_dependency));
	#endif
	}

	default_ref_layers_active = r.get_bit();

	profile_tier_levels.resize(r.getu() + 1);
	for (auto &i : profile_tier_levels.slice(base_layer_internal ? 2 : 1))
		i.read(r, r.get_bit(), max_sub_layers);

	auto	NumLayerSets		= layer_sets.size();

	uint32	num_add_olss		= 0;
	default_output_layer_idc	= 0;
	if (NumLayerSets > 1) {
		num_add_olss				= r.getu();
		default_output_layer_idc	= r.get(2);
	}

	// --- output_layer_sets ---
	output_layer_sets.resize(NumLayerSets + num_add_olss);
	output_layer_sets[0].layer_set	= 0;

	for (int i = 1; i < output_layer_sets.size(); i++) {
		auto&	ols = output_layer_sets[i];
		ols.layer_set = NumLayerSets <= 2 || i < NumLayerSets ? i : r.get(log2_ceil(NumLayerSets - 1)) + 1;

		auto	ls			= ols.layer_set;
		auto	included	= layer_sets[ls];

		if (i >= layer_sets.size() || default_output_layer_idc >= 2) {
			ols.flags = spread_bits(r.get_reverse(count_bits(included)), included);

		} else if (default_output_layer_idc == 1) {
			ols.flags	= highest_set(included);

		} else if (default_output_layer_idc == 0) {
			ols.flags	= included;
		}

		ols.NecessaryLayerFlags = ols.flags;
		for (auto j : make_bit_container(ols.flags & bits(layers.size())))
			ols.NecessaryLayerFlags |= layers[j].dependency;

		if (!profile_tier_levels.empty()) {
			int	nbits = log2_ceil(profile_tier_levels.size());
			ols.profile_tier_level_idx.resize(count_bits(included), -1);
			int	j = 0;
			for (auto mask = included; mask; mask = clear_lowest(mask), j++) {
				if (ols.NecessaryLayerFlags & mask)
					ols.profile_tier_level_idx[j] = r.get(nbits);
			}
		}

		ols.alt_output_layer = count_bits(ols.flags) == 1 && layers[lowest_set_index(ols.flags)].direct_dependency != 0 && r.get_bit();
	}

	// --- rep_formats ---
	rep_formats.read(r, r.getu() + 1);

	if (rep_formats.size() > 1 && r.get_bit()) {
		auto	nbits = log2_ceil(rep_formats.size());
		for (auto &i : layers.slice(base_layer_internal ? 1 : 0))
			i.rep_format_idx = r.get(nbits);
	} else {
		for (auto &i : layers)
			i.rep_format_idx = min(layers.index_of(i), rep_formats.size() - 1);
	}

	r.read(max_one_active_ref_layer, poc_lsb_aligned);

	for (auto &i : layers.slice(1))
		i.poc_lsb_not_present = i.direct_dependency == 0 && r.get_bit();


	//	cross_layer_phase_alignment = r.get_bit();

	for (auto &ols : output_layer_sets.slice(1)) {
		auto	ls			= ols.layer_set;
		auto	included	= layer_sets[ls];

		int		max_sub		= 0;
		for (auto k : make_bit_container(included & bits(layers.size())))
			max_sub	= max(max_sub, layers[k].max_sub_layers);

		bool	sub_layer_flag_info_present = r.get_bit();

		ols.sub_layers.resize(max_sub);
		for (auto &s : ols.sub_layers) {
			s.sub_layer_dpb_info_present = &s == ols.sub_layers.begin() || (sub_layer_flag_info_present && r.get_bit());

			if (s.sub_layer_dpb_info_present) {
				s.max_vps_dec_pic_buffering.resize(count_bits(included), 0);
				int	k = 0;
				for (auto mask = included; mask; mask = clear_lowest(mask), k++) {
					if ((ols.NecessaryLayerFlags & mask) && (base_layer_internal || mask != 1))
						s.max_vps_dec_pic_buffering[k] = r.getu() + 1;
				}
				s.max_vps_num_reorder_pics			= r.getu();
				s.max_vps_latency_increase_plus1	= r.getu();
			}
		}
	}

	int	direct_dep_type_len					= r.getu() + 2;
	int direct_dependency_all_layers_type	= r.get_bit() ? r.get(direct_dep_type_len) + 1 : 0;

	for (auto &i : layers.slice(base_layer_internal ? 1 : 2)) {
		for (auto j : make_bit_container(i.direct_dependency))
			i.direct_dependency_type[j] = direct_dependency_all_layers_type ? direct_dependency_all_layers_type : r.get(direct_dep_type_len) + 1;
	}

	non_vui_extension.read(r, r.getu());
	if (r.get_bit()) {
		r.align(8);
		read_vui(r, base_layer_internal);
	}
	return true;
}

bool extension_multilayer::vps::read_vui(bitreader& r, bool base_layer_internal) {
	int		first_layer		= base_layer_internal ? 0 : 1;

	cross_layer_pic_type_aligned	= r.get_bit();
	cross_layer_irap_aligned		= !cross_layer_pic_type_aligned && r.get_bit();
	all_layers_idr_aligned			= cross_layer_irap_aligned && r.get_bit();

	bool	bit_rate_present = r.get_bit();
	bool	pic_rate_present = r.get_bit();
	if (bit_rate_present || pic_rate_present) {
		vui_layer_sets.resize(layer_sets.size());
		for (auto &i : vui_layer_sets.slice(first_layer)) {
			i.resize(count_bits(layer_sets[vui_layer_sets.index_of(i)]));
			for (auto &j : i)
				j.read(r, bit_rate_present, pic_rate_present);
		}
	}

	// video_signal_info
	if (r.get_bit()) {
		vsi.read(r, r.get(4) + 1);
		if (vsi.size() > 1) {
			for (auto &i : layers.slice(first_layer))
				i.video_signal_info_idx = r.get(4);
		}
	}

	// tile
	if (!r.get_bit()) {
		for (auto &i : layers.slice(first_layer)) {
			if (i.tiles_in_use = r.get_bit())
				i.loop_filter_not_across_tiles = r.get_bit();
		}
		for (auto &i : layers.slice(first_layer)) {
			for (auto j : make_bit_container(i.direct_dependency)) {
				if (i.tiles_in_use && layers[j].tiles_in_use)
					i.tile_boundaries_aligned |= r.get_bit() << j;
			}
		}
	}

	// wpp
	if (!r.get_bit()) {
		for (auto &i : layers.slice(first_layer))
			i.wpp_in_use = r.get_bit();
	}

	single_layer_for_non_irap	= r.get_bit();
	higher_layer_irap_skip		= r.get_bit();

	//ilp_restricted_ref_layers
	if (r.get_bit()) {
		for (auto &i : layers) {
			int	j = 0;
			for (auto k : make_bit_container(i.direct_dependency & ~int(base_layer_internal)))
				i.restrictions[j++].read(r);
		}
	}

	//	vps_vui_bsp_hrd
	if (r.get_bit()) {
		hrd_layer_sets2.resize(r.getu());
		for (auto &i : hrd_layer_sets2) {
			bool	common = hrd_layer_sets2.index_of(i) == 0 || r.get_bit();
			i.read(r, common, r.getu() + 1);
		}

		if (uint32 total_hrd_params = hrd_layer_sets.size32() + hrd_layer_sets2.size32()) {
			auto	nbits_hrd = log2_ceil(total_hrd_params);

			for (auto &h : output_layer_sets.slice(1)) {
				auto	included	= layer_sets[h.layer_set];
				auto	nschemes	= r.getu();

				h.signalled_partitioning_schemes.resize(nschemes);

				for (auto& j : h.signalled_partitioning_schemes) {
					j.resize(r.getu() + 1);
					for (auto& k : j)
						k = spread_bits(r.get_reverse(count_bits(included)), included);
				}

				for (auto &t : h.sub_layers)
					t.bsp_schedules.resize(nschemes);

				for (int i = 0; i < nschemes; i++) {
					for (auto &t : h.sub_layers) {
						t.bsp_schedules[i].resize(r.getu() + 1);
						for (auto& j : t.bsp_schedules[i]) {
							j.resize(h.signalled_partitioning_schemes[i].size());
							for (auto &k : j) {
								k.hrd_idx	= nbits_hrd ? r.get(nbits_hrd) : 0;
								k.sched_idx	= r.getu();
							}
						}

					}
				}
			}
		}

		for (auto &i : layers.slice(1)) {
			if (i.direct_dependency == 0)
				i.base_layer_parameter_set_compatibility = r.get_bit();
		}
	}
	return true;
}

#endif	//HEVC_ML


#ifdef HEVC_3D
//-----------------------------------------------------------------------------
// 3D
//-----------------------------------------------------------------------------
bool extension_3d::depth_lookup::read(bitreader &r, uint8 bit_depth, const depth_lookup *base) {
	if (!base) {
		if (r.get_bit()) {
			for (int i = 0, n = 1 << (bit_depth - 5); i < n; i++) {
				for (uint32 m = r.get(32); m; m = clear_lowest(m))
					delta_list.push_back(i * 32 + lowest_set_index(m));
			}
			return true;
		}
	}
	if (auto num_vals = r.get(bit_depth)) {
		delta_list.resize(num_vals);

		uint32	max_diff	= num_vals > 1 ? r.get(bit_depth) : 0;
		uint32	min_diff	= num_vals > 2 && max_diff > 0 ? r.get(log2_ceil(max_diff)) + 1 : max_diff;
		int		nbits		= max_diff > min_diff ? log2_ceil(max_diff - min_diff + 1) : 0;

		delta_list[0]	= r.get(bit_depth);
		for (int k = 1; k < num_vals; k++)
			delta_list[k] = delta_list[k - 1] + (nbits ? r.get(nbits) : 0) + min_diff;
	}

	if (base) {
		bool	m0[256] = {false};
		bool	m1[256] = {false};

		for (auto i : base->delta_list)
			m0[i] = true;

		for (auto i : delta_list)
			m1[i] = true;

		dynamic_array<uint32>	delta_list2;
		for (int i = 0; i < 256; i++) {
			if (m0[i] ^ m1[i])
				delta_list2.push_back(i);
		}
		swap(delta_list, delta_list2);
	}
	return true;
}

bool extension_3d::vps::read(bitreader &r, int num_views) {
	r.read(cp_precision);
	cps.resize(num_views);

	for (auto& i : cps.slice(1)) {
		i.resize(r.get(6));
		if (!i.empty()) {
			i.cp_in_slice_segment_header = r.get_bit();
			for (auto& j : i) {
				r.read(j.ref_voi);
				if (!i.cp_in_slice_segment_header)
					j.read(r);
			}
		}
	}
	return true;
}

bool extension_3d::pps::read(bitreader &r, const extension_multilayer::vps *vpsml) {
	if (r.get_bit()) {
		int		n			= r.get(6) + 1;
		auto	bit_depth	= r.get(4) + 8;
		auto	layer		= vpsml->layers.begin();

		for (int i = 0; i < n; i++) {
			while (!layer->depth_layer())
				++layer;

			if (r.get_bit()) {
				bool	pred = r.get_bit();
				dlts[layer->id_in_nuh].put().read(r, bit_depth, pred ? dlts[1].exists_ptr() : nullptr);
			}
			++layer;
		}
	}
	return true;
}

void extension_3d::shdr::init(const slice_segment_header* shdr, const extension_multilayer::vps *vpsml, const extension_multilayer::layer *layer, int poc) {
	//I.8.3.2 Derivation process for the default reference view order index for disparity derivation
	view_mask		= 0;
	default_view	= -1;

	auto	dependency = layer ? (layer->direct_dependency & (depth_layer ? vpsml->depth_layers : ~vpsml->depth_layers)) : 0;

	if (!dependency)
		iv.di_mc_enabled = false;

	for (int k = 0, nk = shdr->num_ref_lists(); k < nk; k++) {
		for (auto &i : shdr->get_ref_pics(k)) {
			if (i.poc == poc && (default_view < 0 || i.view_idx < default_view))
				default_view = i.view_idx;
			view_mask |= bit64(i.view_idx);
		}
	}

	if (depth_layer) {
		if (shdr->type == SLICE_TYPE_I)
			iv.cqt_cu_part_pred_enabled	= false;


		return;
	}

	if (default_view < 0 || shdr->type == SLICE_TYPE_I) {
		iv.dbbp_enabled		= false;
		iv.res_pred_enabled	= false;
		iv.di_mc_enabled	= false;
		iv.vsp_mc_enabled	= false;
	}

	bool	cp_available	= false;

	if (auto vps3d = shdr->pps->sps->vps->extension_3d.exists_ptr()) {
		if (default_view >= 0) {
			cp_available = true;
			for (auto i : make_bit_container(dependency)) {
				if (!vps3d->get_cp(layer->view_order_idx(), vpsml->layers[i].view_order_idx(), nullptr)) {
					cp_available = false;
					break;
				}
			}
		}
	}

	if (!cp_available) {
		iv.vsp_mc_enabled		= false;
		iv.depth_ref_enabled	= false;
	}


	if (iv.res_pred_enabled) {
		//I.8.3.5 Derivation process for the target reference index for residual prediction
		for (int k = 0; k < 2; k++) {
			rpref_idx[k]		= -1;
			ref_rpref_avail[k]	= 0;

			if (dependency && (k == 0 || shdr->type == SLICE_TYPE_B)) {
				int		min_diff	= 0x7fff;
				int		idx			= 0;
				for (auto i : shdr->get_ref_pics(k)) {
					int diff = abs(poc - i.poc);
					if (diff && diff < min_diff) {
						min_diff		= diff;
						rpref_idx[k]	= idx;
					}
					++idx;
				}
			}
		}

		if (rpref_idx[0] < 0 && rpref_idx[1] < 0)
			iv.res_pred_enabled = false;
	}
}


#endif	//HEVC_3D

#ifdef HEVC_SCC
//-----------------------------------------------------------------------------
// SCC
//-----------------------------------------------------------------------------

bool extension_scc::palette_predictor_initializers::read(bitreader &r, uint32 num, int bit_depth_luma, int bit_depth_chroma) {
	resize(num);
	for (int i = 0, n = bit_depth_chroma ? 3 : 1; i < n; i++) {
		uint32	nbits	= i == 0 ? bit_depth_luma : bit_depth_chroma;
		for (auto &e : *this)
			e[i] = r.get(nbits);
	}
	return true;
}

bool extension_scc::sps::read(bitreader &r, CHROMA chroma_format, int bit_depth_luma, int bit_depth_chroma) {
	r.read(curr_pic_ref, palette_mode);
	if (palette_mode) {
		r.read(palette_max_size, delta_palette_max_predictor_size);
		if (r.get_bit())
			ppi.put().read(r, r.getu() + 1, bit_depth_luma, chroma_format == CHROMA_MONO ? 0 : bit_depth_chroma);
	}
	return r.read(motion_vector_resolution_control, intra_boundary_filtering_disabled);
}

bool extension_scc::pps::read(bitreader &r, CHROMA chroma_format, int bit_depth_luma, int bit_depth_chroma) {
	r.read(curr_pic_ref, act_qp_offsets);
	if (r.get_bit()) {
		auto&	initialisers	= ppi.put();
		if (uint32	num	= r.getu())  {
			bool	mono				= r.get_bit();
			uint8	bit_depth_luma		= r.getu() + 8;
			uint8	bit_depth_chroma	= mono ? 0 : r.getu() + 8;
			initialisers.read(r, num, bit_depth_luma, bit_depth_chroma);
		}
	}
	return true;
}

#endif //HEVC_SCC

//-----------------------------------------------------------------------------
// sub_layer_ordering
//-----------------------------------------------------------------------------

bool sub_layer_ordering::read(bitreader &r, uint32 max_sublayers) {
	layers.resize(max_sublayers);

	ordering_info_present	= r.get_bit();
	if (ordering_info_present) {
		for (auto &i : layers)
			i.read(r);
	} else {
		layers[0].read(r);
		for (auto &i : layers.slice(1))
			i = layers[0];
	}
	return true;
}

bool sub_layer_ordering::write(bitwriter& w) const {
	w.put_bit(ordering_info_present);
	if (ordering_info_present)
		for (auto &i : layers)
			i.write(w);
	else
		layers[0].write(w);
	return true;
}


//-----------------------------------------------------------------------------
// VPS
//-----------------------------------------------------------------------------

bool video_parameter_set::read(bitreader &r) {
	r.tell_bit();
	r.read(id, base_layer_internal, base_layer_available, max_layers, max_sub_layers, temporal_id_nesting);
	r.discard(16);

	profile_tier_level.read(r, true, max_sub_layers);
	ordering.read(r, max_sub_layers);

	uint32	max_layer_id	= r.get(6);
	auto	num_layer_sets	= r.getu() + 1;
	if (num_layer_sets >= 1024)
		return false;

	dynamic_array<uint64>	layer_sets(num_layer_sets);
	for (auto &i : layer_sets.slice(1))
		i = r.get_reverse(max_layer_id + 1);

	dynamic_array<pair<uint16, hrd_parameters>>	hrd_layer_sets;

	if (r.read(timing) && timing.exists() && timing->num_ticks_poc_diff_one.exists()) {
		hrd_layer_sets.resize(r.getu());
		for (auto &i : hrd_layer_sets) {
			i.a		= r.getu();
			bool	common = hrd_layer_sets.index_of(i) == 0 || r.get_bit();
			i.b.read(r, common, max_sub_layers);
		}
	}

	if (r.get_bit()) {
	#ifdef HEVC_ML
		r.align(8);
		extension_multilayer.put(max_layers, max_sub_layers, move(layer_sets), move(hrd_layer_sets)).read(r, base_layer_internal);

		if (r.get_bit()) {
		#ifdef HEVC_3D
			if (r.get_bit()) {
				r.align(8);
				extension_3d.put().read(r, extension_multilayer->view_ids.size());
			}
			if (r.get_bit()) {
				//more extensions
			}
		#endif
		}
	#endif
	}

	return true;
}

//-----------------------------------------------------------------------------
// SPS
//-----------------------------------------------------------------------------

bool seq_parameter_set::read(bitreader &r, int layer_id) {
	auto	max_sub_layers_or_ext	= r.get(3);
	bool	multiLayer_ext			= layer_id != 0 && max_sub_layers_or_ext == 7;

	temporal_id_nesting = !multiLayer_ext && r.get_bit();

	if (layer_id == 0)
		max_sub_layers = max_sub_layers_or_ext + 1;

	if (!multiLayer_ext)
		profile_tier_level.read(r, true, max_sub_layers);

	id = r.getu();
	if (id >= MAX_SEQ_PARAMETER_SETS)
		return false;

#ifdef HEVC_ML
	if (multiLayer_ext) {
		uint8	rep_idx = r.get_bit() ? r.get(8) : vps->extension_multilayer->layer_by_id(layer_id)->rep_format_idx;
		auto&	rep		= vps->extension_multilayer->rep_formats[rep_idx];

		auto&	bitdepths		= get(rep.bitdepths);
		chroma_format			= bitdepths.chroma_format;
		separate_colour_plane	= bitdepths.separate_colour_plane;
		bit_depth_luma			= bitdepths.bit_depth_luma;
		bit_depth_chroma		= bitdepths.bit_depth_chroma;

		pic_width_luma			= rep.pic_width_luma;
		pic_height_luma			= rep.pic_height_luma;
		conformance_window		= rep.conformance_window;

	} else
#endif
	{
		// --- decode chroma type ---
		chroma_format		= (CHROMA)r.getu();
		if (chroma_format > 3)
			return false;
		separate_colour_plane = chroma_format == CHROMA_444 && r.get_bit();

		// --- picture size ---
		pic_width_luma		= r.getu();
		pic_height_luma		= r.getu();

		if (pic_width_luma == 0 || pic_height_luma == 0)
			return false;

		r.read(conformance_window);

		bit_depth_luma		= r.getu() + 8;
		bit_depth_chroma	= r.getu() + 8;
		if (bit_depth_luma > 16 || bit_depth_chroma > 16)
			return false;
	}

//	ChromaArrayType	= separate_colour_plane ? CHROMA_MONO : chroma_format;

	log2_max_pic_order_cnt_lsb = r.getu() + 4;
	if (log2_max_pic_order_cnt_lsb < 4 || log2_max_pic_order_cnt_lsb > 16)
		return false;

	// --- sub_layer_ordering_info ---
	if (!multiLayer_ext)
		ordering.read(r, max_sub_layers);

	log2_min_luma_coding_block_size				= r.getu() + 3;
	log2_diff_max_min_luma_coding_block_size	= r.getu();
	log2_min_transform_block_size				= r.getu() + 2;
	log2_diff_max_min_transform_block_size		= r.getu();

	max_transform_hierarchy_depth_inter			= r.getu();
	max_transform_hierarchy_depth_intra			= r.getu();

	if (log2_min_luma_coding_block_size + log2_diff_max_min_luma_coding_block_size > 6
	|| log2_min_transform_block_size + log2_diff_max_min_transform_block_size > 5
	|| log2_min_transform_block_size > log2_min_luma_coding_block_size
	|| max_transform_hierarchy_depth_inter > log2_ctu() - log2_min_transform_block_size
	|| max_transform_hierarchy_depth_intra > log2_ctu() - log2_min_transform_block_size
	|| ((pic_width_luma | pic_height_luma) & bits(log2_min_luma_coding_block_size))
	)
		return false;

	// --- scaling ---
	if (r.get_bit()) {
		if (multiLayer_ext && r.get_bit()) {
			auto sps_scaling_list_ref_layer_id = r.get(6);

		} else if (r.get_bit()) {
			if (!scaling_list.put().read(r))
				return false;
		} else {
			scaling_list.put().set_default();
		}
	}

	amp_enabled						= r.get_bit();
	sample_adaptive_offset_enabled	= r.get_bit();

	r.read(pcm);

	auto num_short_term_ref_pic_sets = r.getu();
	if (num_short_term_ref_pic_sets > 64)
		return false;

	shortterm_sets.resize(num_short_term_ref_pic_sets);
	for (uint32 i = 0; i < num_short_term_ref_pic_sets; i++) {
		if (!shortterm_sets[i].read(r, shortterm_sets.slice_to(i), false))
			return false;
	}

	if (long_term_refs_enabled = r.get_bit()) {
		auto num_long_term_ref_pics = r.getu();
		if (num_long_term_ref_pics > MAX_NUM_LT_REF_PICS_SPS)
			return false;

		longterm_refs.resize(num_long_term_ref_pics);
		for (auto &i : longterm_refs)
			i.read(r, log2_max_pic_order_cnt_lsb);
	}

	temporal_mvp_enabled			= r.get_bit();
	strong_intra_smoothing_enabled	= r.get_bit();

	if (r.get_bit()) {
		if (!vui.put().read(r, max_sub_layers))
			return false;
	}

	if (r.get_bit()) {
		extensions = r.get_reverse(8);
		return true
		#ifdef HEVC_RANGE
			&& (!(extensions & EXT_RANGE)		|| extension_range.read(r))
		#endif
		#ifdef HEVC_ML
			&& (!(extensions & EXT_MULTILAYER)	|| extension_multilayer.read(r))
		#ifdef HEVC_3D
			&& (!(extensions & EXT_3D)			|| extension_3d.read(r))
		#endif
		#endif
		#ifdef HEVC_SCC
			&& (!(extensions & EXT_SCC)			|| extension_scc.read(r, chroma_format, bit_depth_luma, bit_depth_chroma))
		#endif
			;
	}

	return true;
}

//-----------------------------------------------------------------------------
// PPS
//-----------------------------------------------------------------------------

bool pic_parameter_set::read(bitreader &r) {
	r.read(dependent_slice_segments_enabled, output_flag_present, num_extra_slice_header_bits, sign_data_hiding, cabac_init_present, num_ref_idx_l0_default_active, num_ref_idx_l1_default_active);

	r.read(init_qp, constrained_intra_pred, transform_skip_enabled);
	cu_qp_delta_depth	= r.get_bit() ? r.getu() : -1;
	qp_offset			= int32x2{r.gets(), r.gets()};

	bool	tiles_enabled;
	r.read(slice_chroma_qp_offsets_present, weighted_pred, weighted_bipred, transquant_bypass_enabled, tiles_enabled, entropy_coding_sync_enabled);

	if (tiles_enabled && entropy_coding_sync_enabled) {
		ISO_OUTPUT("both wpp + tiles!\n");
	}

	// --- tiles ---
	if (tiles_enabled) {
		auto	log2_ctu	= sps->log2_min_luma_coding_block_size + sps->log2_diff_max_min_luma_coding_block_size;
		tiles.put().read(r, shift_right_ceil(sps->pic_width_luma, log2_ctu), shift_right_ceil(sps->pic_height_luma, log2_ctu));
	}

	loop_filter_across_slices	= r.get_bit();

	r.read(deblocking);

	// --- scaling list ---
	bool	scaling_list_data_present = r.get_bit();
	if (sps->scaling_list.exists()) {
		if (scaling_list_data_present) {
			if (!scaling_list.read(r))
				return false;
		} else {
			scaling_list = sps->scaling_list;
		}
	} else if (scaling_list_data_present) {
		return false;
	}

	lists_modification_present	= r.get_bit();
	log2_parallel_merge_level	= r.getu() + 2;

	if (log2_parallel_merge_level > sps->log2_min_luma_coding_block_size + sps->log2_diff_max_min_luma_coding_block_size)
		return false;

	slice_segment_header_extension = r.get_bit();

	// --- extensions ---
	if (r.get_bit()) {
		extensions	= r.get_reverse(8);
		return true
		#ifdef HEVC_RANGE
			&& (!(extensions & EXT_RANGE)		|| extension_range.read(r, sps, transform_skip_enabled, sps->bit_depth_luma, sps->bit_depth_chroma))
		#endif
		#ifdef HEVC_ML
			&& (!(extensions & EXT_MULTILAYER)	|| extension_multilayer.read(r))
		#ifdef HEVC_3D
			&& (!(extensions & EXT_3D)			|| extension_3d.read(r, sps->vps->extension_multilayer.exists_ptr()))
		#endif
		#endif
		#ifdef HEVC_SCC
			&& (!(extensions & EXT_SCC)			|| extension_scc.read(r, sps->chroma_format, sps->bit_depth_luma, sps->bit_depth_chroma))
		#endif
			;
	}
	return true;
}

bool pic_parameter_set::Tiling::read(bitreader &r, uint32 pic_width_ctu, uint32 pic_height_ctu) {
	num_tile_columns	= r.getu() + 1;
	num_tile_rows		= r.getu() + 1;

	if (num_tile_columns > MAX_TILE_COLUMNS|| num_tile_rows > MAX_TILE_ROWS)
		return false;

	colBd[num_tile_columns] = pic_width_ctu;
	rowBd[num_tile_rows]	= pic_height_ctu;

	uniform_spacing			= r.get_bit();

	if (!uniform_spacing) {
		colBd[0]	= 0;
		for (int i = 0; i < num_tile_columns - 1; i++)
			colBd[i + 1]	= colBd[i] + r.getu() + 1;

		rowBd[0]	= 0;
		for (int i = 0; i < num_tile_rows - 1; i++)
			rowBd[i + 1]	= rowBd[i] + r.getu() + 1;

		if (colBd[num_tile_columns - 1] >= pic_width_ctu || rowBd[num_tile_rows - 1] >= pic_height_ctu)
			return false;
	} else {
		for (int i = 0; i < num_tile_columns; i++)
			colBd[i] = i * pic_width_ctu / num_tile_columns;

		for (int i = 0; i < num_tile_rows; i++)
			rowBd[i] = i * pic_height_ctu / num_tile_rows;
	}

	loop_filter_across_tiles = r.get_bit();

	// alloc raster scan arrays
	uint32	num_ctu	= pic_width_ctu * pic_height_ctu;
	RStoTile.resize(num_ctu);
	RStoTS.resize(num_ctu + 1);
	TStoRS.resize(num_ctu);

	// raster scan (RS) <-> tile scan (TS) conversion
	for (int ty = 0, ts = 0; ty < num_tile_rows; ty++) {
		for (int tx = 0; tx < num_tile_columns; tx++) {
			uint8	tile = ty * num_tile_columns + tx;
			for (int y = rowBd[ty]; y < rowBd[ty + 1]; y++) {
				for (int x = colBd[tx]; x < colBd[tx + 1]; x++) {
					int	rs	= y * pic_width_ctu + x;
					RStoTile[rs]	= tile;
					RStoTS[rs]	= ts;
					TStoRS[ts]	= rs;
					ts++;
				}
			}
		}
	}
	RStoTS[num_ctu] = num_ctu;
	return true;
}

//-----------------------------------------------------------------------------
//	slice
//-----------------------------------------------------------------------------

bool slice_segment_header::read(bitreader &r, int layer_id, int temporal_id, const slice_segment_header* prev_shdr, ShortTermReferenceSet &shortterm, LongTermReferenceSet &longterm) {
	const seq_parameter_set* sps = pps->sps;

	uint32	address = 0;

	if (!first_slice_segment_in_pic) {
		dependent	= pps->dependent_slice_segments_enabled && r.get_bit();
		address		= r.get(log2_ceil(sps->total_ctus()));

		if (address >= sps->total_ctus())
			return false;

		if (dependent) {
			if (address == 0 || !prev_shdr)
				return false;

			*this = *prev_shdr;
			first_slice_segment_in_pic	= false;
			dependent		= true;
		}
	}

	segment_address = address;

	auto	vps		= sps->vps;
#ifdef HEVC_ML
	auto*	vpsml	= vps->extension_multilayer.exists_ptr();
	auto	layer	= layer_id > 0 ? vpsml->layer_by_id(layer_id) : nullptr;
  #ifdef HEVC_3D
	auto	vps3d		= vps->extension_3d.exists_ptr();
	bool	depth_layer	= layer && layer->depth_layer();
  #endif
#endif

	if (!dependent) {
		slice_address	= address;
		header_bits		= r.get(pps->num_extra_slice_header_bits);
		type			= (SliceType)r.getu();
		if (type > 2)
			return false;

		pic_output		= !pps->output_flag_present || r.get_bit();

		if (sps->separate_colour_plane)
			colour_plane_id = r.get(2);

		NumPocTotalCurr	= 0;

	#ifdef HEVC_ML
		if ((layer && !layer->poc_lsb_not_present) || (unit_type != NAL::IDR_W_RADL && unit_type != NAL::IDR_N_LP))
			pic_order_cnt_lsb	= r.get(sps->log2_max_pic_order_cnt_lsb);
	#endif
		if (unit_type != NAL::IDR_W_RADL && unit_type != NAL::IDR_N_LP) {
		#ifndef HEVC_ML
			pic_order_cnt_lsb	= r.get(sps->log2_max_pic_order_cnt_lsb);
		#endif
			// --- short-term refs ---
			if (r.get_bit()) {
				int nbits	= log2_ceil(sps->shortterm_sets.size());
				int	idx		= nbits ? r.get(nbits) : 0;
				if (idx >= sps->shortterm_sets.size())
					return false;
				shortterm	= sps->shortterm_sets[idx];
			} else {
				shortterm.read(r, sps->shortterm_sets, true);
			}

			// --- long-term refs ---
			if (sps->long_term_refs_enabled)
				longterm.read(r, sps->longterm_refs.begin(), sps->longterm_refs.size(), sps->log2_max_pic_order_cnt_lsb);

			temporal_mvp_enabled = sps->temporal_mvp_enabled && r.get_bit();
		}

		// --- 3d_extension ---
	#ifdef HEVC_3D
		if (vps3d) {
			extension_3d.depth_layer= depth_layer;
			extension_3d.iv			= sps->extension_3d.iv[depth_layer];
		}
	#endif

		// --- extension_multilayer ---
	#ifdef HEVC_ML
		if (layer_id > 0) {
			extension_multilayer.read(r, vpsml, layer, temporal_id);
			NumPocTotalCurr += extension_multilayer.ref_layers.size();

			// --- 3d_extension ---
		#ifdef HEVC_3D
			if (vps3d) {
				bool	read_xc_pred	= extension_3d.has_inter_comp();

				if (read_xc_pred) {
					if (depth_layer) {
						if (!vpsml->RefCmpLayerAvail(layer, vpsml->find_layer(layer->view_order_idx(), false), temporal_id))
							read_xc_pred = false;

					} else {
						for (auto i : extension_multilayer.ref_layers) {
							if (auto layer2 = vpsml->find_layer(vpsml->layers[i].view_order_idx(), true)) {
								if (!vpsml->RefCmpLayerAvail(layer, layer2, temporal_id)) {
									read_xc_pred = false;
									break;
								}
							}
						}
					}

					if (!read_xc_pred || !r.get_bit())
						extension_3d.clear_inter_comp();
				}

				
				if (!layer->view_order_idx())
					extension_3d.iv.mv_scal_enabled	= false;

			}
		#endif
		}
	#endif

		// --- SAO ---
		if (sps->sample_adaptive_offset_enabled) {
			sao_luma	= r.get_bit();
			sao_chroma	= !sps->separate_colour_plane && sps->chroma_format != CHROMA_MONO && r.get_bit();
		}

		if (type != SLICE_TYPE_I) {
			int	n0 = pps->num_ref_idx_l0_default_active;
			int	n1 = pps->num_ref_idx_l1_default_active;
			if (r.get_bit()) {	//num_ref_idx_active_override
				n0 = r.getu() + 1;
				n1 = type == SLICE_TYPE_B ? r.getu() + 1 : 0;
			}
			if (n0 > MAX_NUM_REF_PICS || n1 > MAX_NUM_REF_PICS)
				return false;

			ref_list[0].init(n0);
			ref_list[1].init(n1);

			for (auto &i : longterm)
				NumPocTotalCurr += i.used;
			for (auto &i : shortterm.neg)
				NumPocTotalCurr += i.used;
			for (auto &i : shortterm.pos)
				NumPocTotalCurr += i.used;

			if (pps->lists_modification_present && NumPocTotalCurr > 1) {
				int nBits = log2_ceil(NumPocTotalCurr);
				if (r.get_bit())
					ref_list[0].read(r, nBits);
				if (type == SLICE_TYPE_B && r.get_bit())
					ref_list[1].read(r, nBits);
			}

			mvd_l1_zero	= type == SLICE_TYPE_B && r.get_bit();
			cabac_init	= pps->cabac_init_present && r.get_bit();

			if (temporal_mvp_enabled) {
				collocated.list		= type == SLICE_TYPE_B && !r.get_bit();
				collocated.idx		= ref_list[collocated.list].size() > 1 ? r.getu() : 0;
				if (collocated.idx >= ref_list[collocated.list].size())
					return false;
			}

			if (type == SLICE_TYPE_P ? pps->weighted_pred : pps->weighted_bipred) {
				luma_log2_weight_denom = r.getu();
				if (luma_log2_weight_denom > 7)
					return false;

				if (sps->chroma_format) {
					chroma_log2_weight_denom = luma_log2_weight_denom + r.gets();
					if (!between(chroma_log2_weight_denom, 0, 7))
						return false;
				}

				uint8	bit_depth_luma		= sps->bit_depth_luma;
				uint8	bit_depth_chroma	= sps->chroma_format ? sps->bit_depth_chroma : 0;
				bool	high_prec			= sps->extension_range.high_precision_offsets_enabled;

				if (!read_weights(r, make_range_n(weights[0], ref_list[0].size()), bit_depth_luma, bit_depth_chroma, luma_log2_weight_denom, chroma_log2_weight_denom, high_prec))
					return false;
				if (type == SLICE_TYPE_B && !read_weights(r, make_range_n(weights[1], ref_list[1].size()), bit_depth_luma, bit_depth_chroma, luma_log2_weight_denom, chroma_log2_weight_denom, high_prec))
					return false;

			} else {
			#ifdef HEVC_3D
				if (layer && vps->extension_3d.exists() && !depth_layer && layer->direct_dependency) {
					extension_3d.ic_enabled					= r.get_bit();
					extension_3d.ic_disabled_merge_zero_idx	= extension_3d.ic_enabled && r.get_bit();
				}
			#endif
			}

			reduce_merge_candidates = r.getu();

		#ifdef HEVC_SCC
			if (sps->extension_scc.motion_vector_resolution_control == 2)
				extension_scc.use_integer_mv = r.get_bit();
		#endif
		}

		qp_delta	= r.gets();
		qp_offset	= pps->slice_chroma_qp_offsets_present ? int32x2{r.gets(), r.gets()} : zero;

	#ifdef HEVC_SCC
		extension_scc.act_qp_offsets	= zero;
		if (auto *qp = pps->extension_scc.act_qp_offsets.exists_ptr()) {
			if (qp->present)
				extension_scc.act_qp_offsets = int32x3{qp->y + r.gets(), qp->cb + r.gets(), qp->cr + r.gets()};
		}
	#endif

	#ifdef HEVC_RANGE
		extension_range.cu_chroma_qp_offset_enabled = pps->extension_range.chroma_qp_offsets.exists() && r.get_bit();
	#endif

		deblocking	= pps->deblocking.or_default();
		if (deblocking.overridden = deblocking.overridden && r.get_bit())
			deblocking.read0(r);

		loop_filter_across_slices = pps->loop_filter_across_slices && ((!sao_luma && !sao_chroma && deblocking.disabled) || r.get_bit());

	#ifdef HEVC_3D
		if (vps3d && layer) {
			int		voi		= layer->view_order_idx();
			auto	&cps	= vps3d->cps[voi];
			if (cps.cp_in_slice_segment_header)
				extension_3d.cps.read(r, cps.size());
		}
	#endif
	}

	if (pps->tiles.exists() || pps->entropy_coding_sync_enabled) {
		auto	num_entry_points = r.getu();

//		if (pps->entropy_coding_sync_enabled && segment_address / sps->pic_width_ctu + num_entry_points >= sps->pic_height_ctu)
//			return false;

		if (pps->tiles.exists() && num_entry_points > pps->tiles->total())
			return false;

		entry_points.resize(num_entry_points);
		if (num_entry_points > 0) {
			auto	nbits = r.getu() + 1;
			if (nbits > 32)
				return false;

			uint32	offset	= 0;
			for (auto &i : entry_points)
				i = offset += r.get(nbits) + 1;
		}
	}

	if (pps->slice_segment_header_extension) {
		auto	ext_len = r.getu();
		auto	ext_end	= r.tell_bit() + ext_len * 8;

	#ifdef HEVC_ML
		if (pps->extensions & EXT_MULTILAYER)
			extension_multilayer.read(r, pps->extension_multilayer, sps->log2_max_pic_order_cnt_lsb);

		if (vpsml) {
			if (vpsml->poc_lsb_aligned && (layer->direct_dependency || (!isBLA(unit_type) && !isCRA(unit_type))) && r.get_bit())
				extension_multilayer.poc_msb_cycle_val = r.getu();
		}
	#endif
		//r.discard(ext_end - r.tell_bit());
		r.seek_bit(ext_end);

	}

	return true;
}

const PicReference *slice_segment_header::find_ref(bool list, int poc, int view) const {
	for (auto &i : get_ref_pics(list)) {
		if (i.poc == poc && i.view_idx == view)
			return &i;
	}
	return nullptr;
}

const PicReference* slice_segment_header::find_ref_any(int poc, int view) const {
	if (auto i = find_ref(false, poc, view))
		return i;
	return find_ref(true, poc, view);
}

//-----------------------------------------------------------------------------
// image
//-----------------------------------------------------------------------------

void image_base::plane::create(uint32 _width, uint32 _height, uint8 _bit_depth) {
	width			= _width;
	height			= _height;
	stride			= _width << (_bit_depth > 8);
	bit_depth		= _bit_depth;
	pixels.resize(stride * height);
}

void image_base::create(uint32 width,uint32 height, CHROMA _chroma_format, uint8 bit_depth_luma, uint8 bit_depth_choma) {
	chroma_format	= _chroma_format;
	planes[0].create(width, height, bit_depth_luma);

	if (chroma_format != CHROMA_MONO) {
		auto	chroma_width		= shift_right_ceil(width, get_shift_W(_chroma_format));
		auto	chroma_height		= shift_right_ceil(height, get_shift_H(_chroma_format));
		planes[1].create(chroma_width, chroma_height, bit_depth_choma);
		planes[2].create(chroma_width, chroma_height, bit_depth_choma);
	}
}

uint32 image::check_CTB_available(int x, int y) const {
	auto	ctu_mask	= bits(ctu_info.log2unitSize);
	uint32	mask		= (x & ctu_mask ? 1 : 0) | (y & ctu_mask ? 2 : 0);
	if (mask != 3)
		mask |= check_available(ctu_info.get(x, y).flags);
	return mask;
}

#if 0
uint32 image::available_pred_flags(int x, int y, int w, int h) const {
	auto	ctu_mask	= bits(ctu_info.log2unitSize);

	uint8	xoff		= x & ctu_mask;
	uint8	yoff		= y & ctu_mask;
	uint8	xp			= xoff / w;
	uint8	yp			= yoff / h;

	uint32	avail		= (xoff ? 1 : 0) | (yoff ? 2 : 0);
	if (avail != 3)
		avail |= check_available(ctu_info.get(x, y).flags);

	bool	left		= avail & 1;
	bool	above		= avail & 2;

	auto	a0			= left
		&& y + h < planes[0].height		//not off bottom of picture
		&& ((y + h) & ctu_mask) != 0;	//not in next ctb row


	//catch extra a0 cases ?

	if (a0 && xoff != 0) {
		uint16	xyp		= xp + yp * 0x100;
		auto	morton0	= interleave(xyp);
		auto	morton1	= interleave(uint16(xyp + 0xff));
		a0 = morton1 < morton0;
	}

	auto	b0			= above
		&& x + w < planes[0].width						//not off right of picture
		&& (yoff == 0 || ((x + w) & ctu_mask) != 0);	//not in next ctb (row above is ok)

	//catch extra b0 cases
	if (!above && y > 0 && ((x + w) & ctu_mask) == 0 && x + w < planes[0].width) {
		if (ctu_info.get(x, y).slice_address == ctu_info.get(x + w, y - 1).slice_address)
			b0 = true;
	}

	if (b0 & yoff != 0) {
		uint16	xyp		= xp + yp * 0x100;
		auto	morton0	= interleave(xyp);
		auto	morton1	= interleave(uint16(xyp - 0xff));
		b0 = morton1 < morton0;
	}

	//remove extra b2 cases ?

	return	left			* AVAIL_A1
		+	above			* AVAIL_B1
		+	(avail == 3)	* AVAIL_B2
		+	a0				* AVAIL_A0
		+	b0				* AVAIL_B0;
}
#endif

bool image::filter_top_right0(int x, int y) {
	if (x >= 0 && x < ctu_info.width_in_units - 1 && y > 0 && y < ctu_info.height_in_units) {
		auto&	ctb1 = ctu_info.get0(x, y);
		auto&	ctb2 = ctu_info.get0(x + 1, y - 1);
		return ctb1.slice_address == ctb2.slice_address || _get_SliceHeader(ctb1.shdr_index)->loop_filter_across_slices;
	}
	return false;
}

void image::release() {
	if (--refs == 0) {
		for (auto &i : slices)
			delete i;
		slices.clear();
		nal_unit	= NAL::UNDEFINED;

		//link to freelist
		if (auto p = (image**)free_ptr) {
			free_ptr	= *p;
			*p			= this;
		} else {
			delete this;
		}
	}
}

void image::alloc(const pic_parameter_set *pps, int64 _pts, void* _user_data) {
//	release();

	this->pps		= unconst(pps);
	pts				= _pts;
	user_data		= _user_data;

	auto	sps		= pps->sps.get();
	allow_filter_pcm = sps->pcm.exists() && !sps->pcm->loop_filter_disable;
	ChromaArrayType	= sps->separate_colour_plane ? CHROMA_MONO : sps->chroma_format;

	int		w		= sps->pic_width_luma;
	int		h		= sps->pic_height_luma;
	create(w, h, sps->chroma_format, sps->bit_depth_luma, sps->bit_depth_chroma);

	cu_info.alloc2(	w, h, sps->log2_min_luma_coding_block_size);
	pb_info.alloc2(	w, h, 2);
	tu_info.alloc2(	w, h, sps->log2_min_transform_block_size);
	ctu_info.alloc2(w, h, sps->log2_min_luma_coding_block_size + sps->log2_diff_max_min_luma_coding_block_size);
}

void image::clear_metadata() {
	fill(cu_info.all(), CU_info());
	fill(ctu_info.all(), CTU_info());
	fill(pb_info.all(), PB_info());
	fill(tu_info.all(), 0);

#if 0
	uint32	a = 0;
	for (auto i : ctu_info.all())
		for (auto &j : i)
			j.CtbAddrInRS	= a++;
#endif

	if (pps->tiles.exists()) {
		uint32	flags	= CTU_info::BOUNDARY_V | CTU_info::TILE_BOUNDARY_V | (pps->tiles->loop_filter_across_tiles ? 0 : CTU_info::FILTER_BOUNDARY_V);
		for (auto y : pps->tiles->rows()) {
			for (int x = 0; x < ctu_info.width_in_units; x++)
				ctu_info.get0(x, y).set_flags(flags);
		}

		flags	= CTU_info::BOUNDARY_H | CTU_info::TILE_BOUNDARY_H | (pps->tiles->loop_filter_across_tiles ? 0 : CTU_info::FILTER_BOUNDARY_H);
		for (auto x : pps->tiles->cols()) {
			for (int y = 0; y < ctu_info.height_in_units; y++)
				ctu_info.get0(x, y).set_flags(flags);
		}
	}
}

void CTU_info::wait(PROGRESS p, Event *_event) {
	for (;;) {
		PROGRESS	p0 = progress;
		if (p0 >= p)
			return;

		if (!(p0 & 1)) {
			ISO_ASSERT(!event);
			event	= _event;

			if (!progress.cas(p0, PROGRESS(p0 | 1)))
				continue;
		}
	#if 1
		event->wait();
	#else
		ISO_OUTPUTF("wait0 at 0x") << hex(CtbAddrInRS) << '\n';
		bool	ok = event.wait(1);
		ISO_OUTPUTF("%s at 0x", ok ? "done" : "timeout") << hex(CtbAddrInRS) << '\n';
	#endif
	}
}
void CTU_info::set_progress(PROGRESS p) {
	for (;;) {
		PROGRESS	p0 = progress;
		if (progress.cas(p0, p)) {
			if (p0 & 1)
				event->signal();
			return;
		}
	}
}

template<typename T> void print_checksum(blockptr<T, 2> p, int x, int y, int w, int h, int size, int minsize) {
	if (w <= 0 || h <= 0)
		return;

	int   total = 0;
	for (int y = 0; y < h; y++) {
		auto	p1 = p[y];
		for (int x = 0; x < w; x++)
			total += p1[x];
	}
	ISO_OUTPUTF("checksum x%i @%i, %i: %i\n", min(w, h), x, y, total);
	if (min(w, h) > minsize) {
		size >>= 1;
		int w0 = size, w1 = w - w0;
		int h0 = size, h1 = h - h0;
		print_checksum(p, x, y, w0, h0, size, minsize);
		print_checksum(p.slice(w0, 0), x + w0, y, w1, h0, size, minsize);
		print_checksum(p.slice(0, h0), x, y + h0, w0, h1, size, minsize);
		print_checksum(p.slice(w0, h0), x + w0, y + h0, w1, h1, size, minsize);
	}
}

void print_checksum(image* img, int c, int ctb, int minsize) {
	if (c && img->chroma_format == CHROMA_MONO)
		return;

	int	size	= 1 << img->ctu_info.log2unitSize;
	int	x		= (ctb % img->ctu_info.width_in_units) << img->ctu_info.log2unitSize, y = (ctb / img->ctu_info.width_in_units) << img->ctu_info.log2unitSize;
	int	w		= 1 << img->ctu_info.log2unitSize, h = w;

	const int shiftw	= get_shift_W(img->chroma_format, c);
	const int shifth	= get_shift_H(img->chroma_format, c);
	size	>>= shiftw;

	x >>= shiftw;
	y >>= shifth;
	w >>= shiftw;
	h >>= shifth;

	if (x + w > img->get_width(c))
		w = img->get_width(c) - x;
	if (y + h > img->get_height(c))
		h = img->get_height(c) - y;

	if (img->get_bit_depth(c) <= 8)
		print_checksum(img->get_plane_ptr<uint8>(c).slice(x, y), x, y, w, h, size, minsize);
	else
		print_checksum(img->get_plane_ptr<uint16>(c).slice(x, y), x, y, w, h, size, minsize);
}


//-----------------------------------------------------------------------------
//	Quant
//-----------------------------------------------------------------------------

static const uint8 chroma_scale[4][58] = {
	//0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, //CHROMA_400
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,29,30,31,32,33,33,34,34,35,35,36,36,37,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51 }, //CHROMA_420
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,51,51,51,51,51,51 }, //CHROMA_422
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,51,51,51,51,51,51 }  //CHROMA_444
};

static const int8 level_scale[] = { 40, 45, 51, 57, 64, 72 };

constexpr auto QuantFactor(int qP) { return level_scale[qP % 6] << (qP / 6); }

int get_QPpred(const image *img, int x, int y, int QPprev) {
	uint8	mask_ctu	= bits(img->ctu_info.log2unitSize);
	int		qPYA		= (x & mask_ctu) ? img->get_QPY(x - 1, y) : QPprev;
	int		qPYB		= (y & mask_ctu) ? img->get_QPY(x, y - 1) : QPprev;
	return (qPYA + qPYB + 1) >> 1;
}

int calc_QPY(int QPpred, int offset, int bit_depth) {
	int		bd_offset	= 6 * (bit_depth - 8);
	return (QPpred + offset + 52 + 2 * bd_offset) % (52 + bd_offset) - bd_offset;
}

int do_chroma_scale(int QPY, CHROMA chroma_format, int bit_depth) {
	int		bd_offset	= 6 * (bit_depth - 8);
	return QPY < 0
		? max(QPY + bd_offset, 0)
		: chroma_scale[chroma_format][min(QPY, num_elements(chroma_scale[0]) - 1)] + bd_offset;
}

int32x3 calc_QP(int QPY, int32x3 qp_offset, CHROMA chroma_format, int bit_depth_luma, int bit_depth_chroma) {
	return {
		max(QPY + qp_offset.x + 6 * (bit_depth_luma - 8), 0),
		do_chroma_scale(QPY + qp_offset.y, chroma_format, bit_depth_chroma),
		do_chroma_scale(QPY + qp_offset.z, chroma_format, bit_depth_chroma)
	};
}

//-----------------------------------------------------------------------------
// transforms
//-----------------------------------------------------------------------------

#define DEFINE_DST4x4_MATRIX(a,b,c,d) {\
  {  a,  b,  c,  d }, \
  {  c,  c,  0, -c }, \
  {  d, -a, -c,  b }, \
  {  b, -d,  c, -a }, \
}
#define DEFINE_DCT4x4_MATRIX2(b,c) {\
  { b,  c }, \
  { c, -b }  \
}
#define DEFINE_DCT8x8_MATRIX2(d,e,f,g) {\
/*	0   1   2   3*/\
  { d,  e,  f,  g }, /* 1*/\
  { e, -g, -d, -f }, /* 3*/\
  { f, -d,  g,  e }, /* 5*/\
  { g, -f,  e, -d }  /* 7*/\
}
#define DEFINE_DCT16x16_MATRIX2(h,i,j,k,l,m,n,o) {\
/*	0   1   2   3   4   5   6   7*/\
  { h,  i,  j,  k,  l,  m,  n,  o }, /* 1*/\
  { i,  l,  o, -m, -j, -h, -k, -n }, /* 3*/\
  { j,  o, -k, -i, -n,  l,  h,  m }, /* 5*/\
  { k, -m, -i,  o,  h,  n, -j, -l }, /* 7*/\
  { l, -j, -n,  h, -o, -i,  m,  k }, /* 9*/\
  { m, -h,  l,  n, -i,  k,  o, -j }, /*11*/\
  { n, -k,  h, -j,  m,  o, -l,  i }, /*13*/\
  { o, -n,  m, -l,  k, -j,  i, -h }  /*15*/\
}
#define DEFINE_DCT32x32_MATRIX2(p,q,r,s,t,u,v,w,x,y,z,A,B,C,D,E) {\
/*	0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15*/\
  { p,  q,  r,  s,  t,  u,  v,  w,  x,  y,  z,  A,  B,  C,  D,  E }, /* 1*/\
  { q,  t,  w,  z,  C, -E, -B, -y, -v, -s, -p, -r, -u, -x, -A, -D }, /* 3*/\
  { r,  w,  B, -D, -y, -t, -p, -u, -z, -E,  A,  v,  q,  s,  x,  C }, /* 5*/\
  { s,  z, -D, -w, -p, -v, -C,  A,  t,  r,  y, -E, -x, -q, -u, -B }, /* 7*/\
  { t,  C, -y, -p, -x,  D,  u,  s,  B, -z, -q, -w,  E,  v,  r,  A }, /* 9*/\
  { u, -E, -t, -v,  D,  s,  w, -C, -r, -x,  B,  q,  y, -A, -p, -z }, /*11*/\
  { v, -B, -p, -C,  u,  w, -A, -q, -D,  t,  x, -z, -r, -E,  s,  y }, /*13*/\
  { w, -y, -u,  A,  s, -C, -q,  E,  p,  D, -r, -B,  t,  z, -v, -x }, /*15*/\
  { x, -v, -z,  t,  B, -r, -D,  p, -E, -q,  C,  s, -A, -u,  y,  w }, /*17*/\
  { y, -s, -E,  r, -z, -x,  t,  D, -q,  A,  w, -u, -C,  p, -B, -v }, /*19*/\
  { z, -p,  A,  y, -q,  B,  x, -r,  C,  w, -s,  D,  v, -t,  E,  u }, /*21*/\
  { A, -r,  v, -E, -w,  q, -z, -B,  s, -u,  D,  x, -p,  y,  C, -t }, /*23*/\
  { B, -u,  q, -x,  E,  y, -r,  t, -A, -C,  v, -p,  w, -D, -z,  s }, /*25*/\
  { C, -x,  s, -q,  v, -A, -E,  z, -u,  p, -t,  y, -D, -B,  w, -r }, /*27*/\
  { D, -A,  x, -u,  r, -p,  s, -v,  y, -B,  E,  C, -z,  w, -t,  q }, /*29*/\
  { E, -D,  C, -B,  A, -z,  y, -x,  w, -v,  u, -t,  s, -r,  q, -p }  /*31*/\
}

const int16 idst_4[4][4]		= DEFINE_DST4x4_MATRIX	(   29,	55,	74,	84);

const int16 idct_2b [1][1]		= {{64}};																					//1/4				cos(x * pi) * sqrt2 * 64
const int16 idct_4b [2][2]		= DEFINE_DCT4x4_MATRIX2	(	83, 36);														//1/8,  3/8
const int16 idct_8b [4][4]		= DEFINE_DCT8x8_MATRIX2	(   89,	75,	50,	18);												//1/16, 3/16, 5/16, 7/16
const int16 idct_16b[8][8]		= DEFINE_DCT16x16_MATRIX2(	90,	87,	80,	70,	57,	43,	25,	 9);								//1/32, 3/32, 5/32, 7/32, 9/32, 11/32, 13/32, 15/32
const int16 idct_32b[16][16]	= DEFINE_DCT32x32_MATRIX2(	90,	90,	88,	85,	82,	78,	73,	67,	61,	54,	46,	38,	31,	22,	13,	 4);//1/64, 3/64, 5/64, 7/64, 9/64, 11/64, 13/64, 15/64, 17/64, 19/64, 21/64, 23/64, 25/64, 27/64, 29/64, 31/64

struct DST4 {
	template<typename S> static auto inverse(vec<S,4> in) {
		//return (const mat<int, 4, 4>&)idst_4 * to<int>(in);
		return to<int>((const mat<int16, 4, 4>&)idst_4) * to<int>(in);

	}
	template<typename S, typename FI, typename FO> static void inverse(FI in, FO out) {
		S	c[4];
		c[0] = in(0);
		c[1] = in(1);
		c[2] = in(2);
		c[3] = in(3);

		for (int k = 0; k < 4; k++) {
			S	result = 0;
			for (int row = 0; row < 4; row++)
				result += c[row] * idst_4[row][k];

			out(k, result);
		}
	}
};

template<int N> struct partial_butterfly;

template<> struct partial_butterfly<2> {
	template<typename S> static vec<S,2> inverse(vec<S,2> in) {
		auto	I0	= idct_2b[0][0] * in[0];
		auto	I1	= idct_2b[0][0] * in[1];
		return {I0 + I1, I0 - I1};
	}
	template<typename S, typename FI, typename FO> static void inverse(FI in, FO out) {
		auto	I0	= idct_2b[0][0] * in(0);
		auto	I1	= idct_2b[0][0] * in(1);
		out(0, I0 + I1);
		out(1, I0 - I1);
	}
};

template<> struct partial_butterfly<4> {
	template<typename S> static vec<S,4> inverse(vec<S,4> in) {
		vec<S,2>	O = {idct_4b[0][0] * in[1] + idct_4b[1][0] * in[3], idct_4b[0][1] * in[1] + idct_4b[1][1] * in[3]};
		vec<S,2>	E = partial_butterfly<2>::inverse<S>(in.even);
		return concat(E + O, swizzle<1,0>(E - O));
	}
	template<typename S, typename FI, typename FO> static void inverse(FI in, FO out) {
		S E[2], O[2];
		O[0] = idct_4b[0][0] * in(1) + idct_4b[1][0] * in(3);
		O[1] = idct_4b[0][1] * in(1) + idct_4b[1][1] * in(3);

		partial_butterfly<2>::inverse<S>(
			[&](int i)		{ return in(i * 2); },
			[&](int i, S v)	{ E[i] = v; }
		);

		out(0, E[0] + O[0]);
		out(1, E[1] + O[1]);
		out(2, E[1] - O[1]);
		out(3, E[0] - O[0]);
	}
};

template<> struct partial_butterfly<8> {
	template<typename S> static vec<S,8> inverse(vec<S,8> in) {
		vec<S,4>	O	= zero;
		for (int k = 0; k < 4; ++k)
			O	+= in[k * 2 + 1] * vec<S,4>{idct_8b[k][0], idct_8b[k][1], idct_8b[k][2], idct_8b[k][3]};

		vec<S,4>	E = partial_butterfly<4>::inverse<S>(in.even);
		return concat(E + O, swizzle<3,2,1,0>(E - O));
	}
	template<typename S, typename FI, typename FO> static void inverse(FI in, FO out) {
		S E[4], O[4];
		for (int k = 0; k < 4; k++)
			O[k] =	idct_8b[0][k] * in(1) + idct_8b[1][k] * in(3) + idct_8b[2][k] * in(5) + idct_8b[3][k] * in(7);

		partial_butterfly<4>::inverse<S>(
			[&](int i)		{ return in(i * 2); },
			[&](int i, S v)	{ E[i] = v; }
		);

		for (int k = 0; k < 4; k++) {
			out(k,		E[k]	+ O[k]	);
			out(k + 4,	E[3-k]	- O[3-k]);
		}
	}
};

template<> struct partial_butterfly<16> {
	template<typename S> static vec<S,16> inverse(vec<S,16> in) {
		vec<S,8>	O	= zero;
		for (int k = 0; k < 8; ++k)
			O	+= in[k * 2 + 1] * vec<S,8>{idct_16b[k][0], idct_16b[k][1], idct_16b[k][2], idct_16b[k][3], idct_16b[k][4], idct_16b[k][5], idct_16b[k][6], idct_16b[k][7]};

		vec<S,8>	E = partial_butterfly<8>::inverse<S>(in.even);
		return concat(E + O, swizzle<7,6,5,4,3,2,1,0>(E - O));
	}
	template<typename S, typename FI, typename FO> static void inverse(FI in, FO out) {
		S O[8], E[8];
		for (int k = 0; k < 8; k++)
			O[k] =	idct_16b[0][k] * in( 1) + idct_16b[1][k] * in( 3) + idct_16b[2][k] * in( 5) + idct_16b[3][k] * in( 7)
			+	idct_16b[4][k] * in( 9) + idct_16b[5][k] * in(11) + idct_16b[6][k] * in(13) + idct_16b[7][k] * in(15);

		partial_butterfly<8>::inverse<S>(
			[&](int i)		{ return in(i * 2); },
			[&](int i, S v)	{ E[i] = v; }
		);

		for (int k = 0; k < 8; k++) {
			out(k,		E[k]	+ O[k]	);
			out(k + 8,	E[7-k]	- O[7-k]);
		}
	}
};

template<> struct partial_butterfly<32> {
	template<typename S> static vec<S,32> inverse(vec<S,32> in) {
		vec<S,16>	O	= zero;
		for (int k = 0; k < 16; ++k)
			O	+= in[k * 2 + 1] * vec<S,16>{
			idct_32b[k][ 0], idct_32b[k][ 1], idct_32b[k][ 2], idct_32b[k][ 3], idct_32b[k][ 4], idct_32b[k][ 5], idct_32b[k][ 6], idct_32b[k][ 7],
				idct_32b[k][ 8], idct_32b[k][ 9], idct_32b[k][10], idct_32b[k][11], idct_32b[k][12], idct_32b[k][13], idct_32b[k][14], idct_32b[k][15]
		};
		vec<S,16>	E = partial_butterfly<16>::inverse<S>(in.even);
		return concat(E + O, swizzle<15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0>(E - O));
	}

	template<typename S, typename FI, typename FO> static void inverse(FI in, FO out) {
		S O[16],E[16];
		for (int k = 0; k< 16; k++) {
			O[k] =	idct_32b[ 0][k] * in( 1) + idct_32b[ 1][k] * in( 3) + idct_32b[ 2][k] * in( 5) + idct_32b[ 3][k] * in( 7)
				+	idct_32b[ 4][k] * in( 9) + idct_32b[ 5][k] * in(11) + idct_32b[ 6][k] * in(13) + idct_32b[ 7][k] * in(15)
				+	idct_32b[ 8][k] * in(17) + idct_32b[ 9][k] * in(19) + idct_32b[10][k] * in(21) + idct_32b[11][k] * in(23)
				+	idct_32b[12][k] * in(25) + idct_32b[13][k] * in(27) + idct_32b[14][k] * in(29) + idct_32b[15][k] * in(31);
		}

		partial_butterfly<16>::inverse<S>(
			[&](int i)		{ return in(i * 2); },
			[&](int i, S v)	{ E[i] = v; }
		);

		for (int k = 0; k < 16; k++) {
			out(k,		E[k]	+ O[k]	 );
			out(k + 16,	E[15-k]	- O[15-k]);
		}
	}
};

template<typename FT, int LN> static void transform(int32 *dst, int16 *coeffs, int bit_depth, int max_coeff_bits) {
	int		tmp[1 << (LN * 2)];

	const int TRANSFORM_MATRIX_SHIFT = 6;
	const int shift1	= TRANSFORM_MATRIX_SHIFT + 1;
	const int shift2	= (TRANSFORM_MATRIX_SHIFT + max_coeff_bits - 1) - bit_depth;
	static const int N	= 1 << LN;

	FT::template inverse<vec<int,N>>(
		[coeffs](int i)	{
			return to<int>(load<N>(coeffs + (i << LN)));
		},
		[&tmp, add = (1 << shift1) >> 1, min = -(1 << max_coeff_bits), max = (1 << max_coeff_bits) - 1](int i, vec<int, N> v) {
			v = clamp((v + add) >> shift1, min, max);
			//v = (v + add) >> shift1;
			store(v, tmp + (i << LN));
		}
	);

	FT::template inverse<vec<int,N>>(
		[tmp](int i) {
			return load<N>(fixed_stride_iterator2<const int, N>(tmp + i));
		},
		[&dst, shift2, add = (1 << shift2) >> 1](int i, vec<int,N> v) {
			v  = (v + add) >> shift2;
			store(v, fixed_stride_iterator2<int32, N>(dst + i));
		}
	);
}

void transform(int32 *dst, int log2TrSize, bool idst, int16 *coeffs, int bit_depth, int max_coeff_bits) {
	if (idst)
		transform<DST4, 2>(dst, coeffs, bit_depth, max_coeff_bits);
	else switch (log2TrSize) {
		case 2: transform<partial_butterfly<4 >, 2>(dst, coeffs, bit_depth, max_coeff_bits); break;
		case 3: transform<partial_butterfly<8 >, 3>(dst, coeffs, bit_depth, max_coeff_bits); break;
		case 4: transform<partial_butterfly<16>, 4>(dst, coeffs, bit_depth, max_coeff_bits); break;
		case 5: transform<partial_butterfly<32>, 5>(dst, coeffs, bit_depth, max_coeff_bits); break;
	}
}

template<typename FT, int LN, typename T> void transform_add(blockptr<T,2> dst, int16 *coeffs, int bit_depth, int max_coeff_bits) {
	int		tmp[1 << (LN * 2)];

	const int TRANSFORM_MATRIX_SHIFT = 6;
	const int shift1	= TRANSFORM_MATRIX_SHIFT + 1;
	const int shift2	= (TRANSFORM_MATRIX_SHIFT + max_coeff_bits - 1) - bit_depth;
	static const int N	= 1 << LN;

	FT::template inverse<vec<int,N>>(
		[coeffs](int i)	{
			return to<int>(load<N>(fixed_stride_iterator2<const int16, N>(coeffs + i)));
		},
		[&tmp, add = (1 << shift1) >> 1/*, min = -(1 << max_coeff_bits), max = (1 << max_coeff_bits) - 1*/](int i, vec<int, N> v) {
			//v = clamp((v + add) >> shift1, min, max);
			v = (v + add) >> shift1;
			store(v,  tmp + (i << LN));
		}
	);

	FT::template inverse<vec<int,N>>(
		[tmp](int i) {
			return load<N>(fixed_stride_iterator2<const int, N>(tmp + i));
		},
		[&dst, shift2, add = (1 << shift2) >> 1, max = (1 << bit_depth) - 1](int i, vec<int,N> v) {
			v  = clamp(((v + add) >> shift2) + to<int>(load<N>(dst[i])), 0, max);
			store(to<T>(v), dst[i]);
		}
	);
}

template<typename T> void transform_add(blockptr<T,2> dst, int log2TrSize, bool idst, int16 *coeffs, int bit_depth, int max_coeff_bits) {
	if (idst)
		transform_add<DST4, 2>(dst, coeffs, bit_depth, max_coeff_bits);
	else switch (log2TrSize) {
		case 2: transform_add<partial_butterfly<4 >, 2>(dst, coeffs, bit_depth, max_coeff_bits); break;
		case 3: transform_add<partial_butterfly<8 >, 3>(dst, coeffs, bit_depth, max_coeff_bits); break;
		case 4: transform_add<partial_butterfly<16>, 4>(dst, coeffs, bit_depth, max_coeff_bits); break;
		case 5: transform_add<partial_butterfly<32>, 5>(dst, coeffs, bit_depth, max_coeff_bits); break;
	}
}

template<int LN> void transform_bypass(int32* residual, int rdpcm_mode, const int16* coeffs) {
	static const int N	= 1 << LN;
	vec<int, N>	sum	= zero;
	switch (rdpcm_mode) {
		default:
			for (int y = 0; y < N; y++) {
				sum = to<int>(load<N>(coeffs + (y << LN)));
				store(sum, residual + (y << LN));
			}
			break;
		case 1:
			for (int x = 0; x < N; x++) {
				sum += to<int>(load<N>(fixed_stride_iterator2<const int16, N>(coeffs + x)));
				store(sum, fixed_stride_iterator2<int32, N>(residual + x));
			}
			break;
		case 2:
			for (int y = 0; y < N; y++) {
				sum += to<int>(load<N>(coeffs + (y << LN)));
				store(sum, residual + (y << LN));
			}
			break;
	}
}

void transform_bypass(int32 *dst, int log2TrSize, int rdpcm_mode, int16 *coeffs) {
	switch (log2TrSize) {
		case 2: transform_bypass<2>(dst, rdpcm_mode, coeffs); break;
		case 3: transform_bypass<3>(dst, rdpcm_mode, coeffs); break;
		case 4: transform_bypass<4>(dst, rdpcm_mode, coeffs); break;
		case 5: transform_bypass<5>(dst, rdpcm_mode, coeffs); break;
	}
}

template<int LN> void transform_skip(int32* residual, int rdpcm_mode, const int16* coeffs, int shift) {
	static const int N	= 1 << LN;
	const int	rnd = 1 << (shift - 1);
	vec<int, N>	sum	= zero;

	switch (rdpcm_mode) {
		default:
			for (int y = 0; y < N; y++) {
				auto	v = (to<int>(load<N>(coeffs + (y << LN))) + rnd) >> shift;
				store(v, residual + (y << LN));
			}
			break;
		case 1:
			for (int x = 0; x < N; x++) {
				sum += (to<int>(load<N>(fixed_stride_iterator2<const int16, N>(coeffs + x))) + rnd) >> shift;
				store(sum, fixed_stride_iterator2<int32, N>(residual + x));
			}
			break;
		case 2:
			for (int y = 0; y < N; y++) {
				sum += (to<int>(load<N>(coeffs + (y << LN))) + rnd) >> shift;
				store(sum, residual + (y << LN));
			}
			break;
	}
}

void transform_skip(int32 *dst, int log2TrSize, int rdpcm_mode, int16 *coeffs, int bit_depth, int max_coeff_bits) {
	int		shift	= max_coeff_bits - bit_depth - log2TrSize;
	ISO_ASSERT(shift > 0);
	switch (log2TrSize) {
		case 2: transform_skip<2>(dst, rdpcm_mode, coeffs, shift); break;
		case 3: transform_skip<3>(dst, rdpcm_mode, coeffs, shift); break;
		case 4: transform_skip<4>(dst, rdpcm_mode, coeffs, shift); break;
		case 5: transform_skip<5>(dst, rdpcm_mode, coeffs, shift); break;
	}
}

template<typename T> inline void add_residual(blockptr<T,2> dst, const int32* r, int nT, int bit_depth) {
	const int max	= bits(bit_depth);
	for (int y = 0; y < nT; y++)
		for (int x = 0; x < nT; x++)
			dst[y][x] = clamp(dst[y][x] + *r++, 0, max);
}


//-----------------------------------------------------------------------------
// intra-prediction
//-----------------------------------------------------------------------------

// Actually, the largest TB block can only be 32, but in some intra-pred-mode algorithms (e.g. min-residual), we may call intra prediction on the maximum CTB size (64).
static const int MAX_INTRA_PRED_BLOCK_SIZE = 64;

// (8.4.4.2.3)
template<typename T> void intra_prediction_sample_filtering(T* p, int log2TrSize, int smoothing_limit) {
	if (smoothing_limit
		&& abs((int)p[64] + p[64+64] - 2 * p[64+32]) < smoothing_limit
		&& abs((int)p[64] + p[64-64] - 2 * p[64-32]) < smoothing_limit
	) {
		int		c	= p[64], dx = p[64 + 64] - c, dy = p[64 - 64] - c;
		for (int i = 1; i <= 64; i++) {
			p[64 - i] = c + ((i * dy + 32) >> 6);
			p[64 + i] = c + ((i * dx + 32) >> 6);
		}
	} else {
		T		f[4 * 32 + 1];
		int		nT4	= 4 << log2TrSize;
		f[0]	= p[0];
		f[nT4]	= p[nT4];
		for (int i = 1; i < nT4; i++)
			f[i] = (p[i + 1] + 2 * p[i] + p[i - 1] + 2) >> 2;
		// copy back to original array
		copy_n(f, p, nT4 + 1);
	}
}

template<typename T> void intra_prediction_planar(blockptr<T, 2> dst, int log2TrSize, T* border) {
	int		nT	= 1 << log2TrSize;
	auto	bt	= border[1 + nT];
	for (int y = 0; y < nT; y++) {
		auto	row = dst[y];
		auto	by1 = border[-y - 1];
		auto	by2 = (y + 1) * border[-1 - nT];
		for (int x	= 0; x < nT; x++)
			row[x] = ((nT - x - 1) * by1 + (x + 1) * bt + (nT - y - 1) * border[x + 1] + by2 + nT) >> (log2TrSize + 1);
	}
}


template<typename T> void intra_prediction_DC(blockptr<T, 2> dst, int log2TrSize, T* border, bool enableIntraBoundaryFilter) {
	int		nT	= 1 << log2TrSize;
	int		dc	= 0;
	for (int i = 0; i < nT; i++)
		dc += border[i + 1] + border[-i - 1];
	dc = (dc + nT) >> (log2TrSize + 1);

	for (int y = 0; y < nT; y++)
		fill_n(dst[y], nT, dc);

	if (enableIntraBoundaryFilter) {
		dst[0][0] = (border[-1] + 2 * dc + border[1] + 2) >> 2;
		for (int x = 1; x < nT; x++) {
			dst[0][x] = (border[ x + 1] + 3 * dc + 2) >> 2;
			dst[x][0] = (border[-x - 1] + 3 * dc + 2) >> 2;
		}
	}
}

const int8 intraPredAngle_table[32 + 1] = {
	32,  26, 21, 17, 13,  9,  5,  2,
	0,	 -2, -5, -9,-13,-17,-21,-26,
	-32,-26,-21,-17,-13, -9, -5, -2,
	0,    2,  5,  9, 13, 17, 21, 26,
	32
};

template<int S, typename T> inline T lerp_shift(T a, T b, int frac) {
	return ((1 << S) - frac) * a + frac * b + (1 << (S - 1)) >> S;
}

// (8.4.4.2.6)
template<typename T> void intra_prediction_angular(blockptr<T, 2> dst, int log2TrSize, T* border, uint8 angle, int bit_depth) {
	int		nT		= 1 << log2TrSize;
	int		delta	= intraPredAngle_table[angle];

	if (angle >= 16) {
		// angle 16..32
		if (delta < 0) {
			int x = delta >> (5 - log2TrSize);
			if (x < -1) {
				int rdelta = div_round(8192, -delta);
				for (int i = -1; i >= x; i--)
					border[i] = border[(i * rdelta + 128) >> 8];
			}
		}

		int		a	= delta;
		for (int y = 0; y < nT; y++, a += delta) {
			auto	row		= dst[y];
			auto	p		= border + (a >> 5) + 1;
			int		frac	= a & 31;
			for (int x = 0; x < nT; x++)
				row[x]	= lerp_shift<5>(p[x], p[x + 1], frac);
		}

	} else {
		// angle 0..15
		if (delta < 0) {
			int x = delta >> (5 - log2TrSize);
			if (x < -1) {
				int rdelta = div_round(8192, -delta);
				for (int i = 1; i <= -x; i++)
					border[i] = border[(i * rdelta + 128) >> 8];
			}
		}

		auto	p	= border - 1;
		for (int y = 0; y < nT; y++, --p) {
			auto	row		= dst[y];
			int		a		= delta;
			for (int x = 0; x < nT; x++, a += delta)
				row[x] = lerp_shift<5>(p[-(a >> 5)], p[-(a >> 5) - 1], a & 31);
		}
	}
}

template<typename T> void get_border_pixels(T *border_pixels, const image *img, int xB0, int yB0, int c, int log2TrSize, int bit_depth, AVAIL avail_pred) {
	const int shiftw	= get_shift_W(img->chroma_format, c);
	const int shifth	= get_shift_H(img->chroma_format, c);

	int		nT			= 1 << log2TrSize;
	int		x0			= xB0 << shiftw;
	int		y0			= yB0 << shifth;
	bool	ignore_pred	= !img->pps->constrained_intra_pred;

	T		*first		= nullptr, *last = border_pixels;
	T		prev		= 1 << (bit_depth - 1);

	auto	dst			= img->get_plane_ptr<T>(c).slice(xB0, yB0);

	if (avail_pred & AVAIL_A1) {
		// copy pixels at left column
		int	n_bottom	= min(img->get_height(c) - yB0, 2 * nT);
		last			= &border_pixels[2 * nT - n_bottom];
		for (int y = n_bottom - 1; y >= 0; y -= 4) {
			if ((y < nT || (avail_pred & AVAIL_A0)) && (ignore_pred || img->get_pred_mode(x0 - 1, (yB0 + y) << shifth) == MODE_INTRA)) {
				if (!first)
					first = last;
				for (int i = 0; i < 4; i++)
					*last++	= prev = dst[y - i][-1];
			} else {
				for (int i = 0; i < 4; i++)
					*last++	= prev;
			}
		}

		// copy pixel at top-left position
		if ((avail_pred & AVAIL_B2) && (ignore_pred || img->get_pred_mode(x0 - 1, y0 - 1) == MODE_INTRA)) {
			if (!first)
				first = last;
			*last++	= prev = dst[-1][-1];
		} else {
			*last++	= prev;
		}
	}

	if (avail_pred & (AVAIL_B1 | AVAIL_B0)) {
		// copy pixels at top row
		int	n_right		= min(img->get_width(c) - xB0, 2 * nT);
		last			= &border_pixels[2 * nT + 1];
		for (int x = 0; x < n_right; x += 4) {
			if ((avail_pred & (x < nT ? AVAIL_B1 : AVAIL_B0)) && (ignore_pred || img->get_pred_mode((xB0 + x) << shiftw, y0 - 1) == MODE_INTRA)) {
				if (!first)
					first = last;
				for (int i = 0; i < 4; i++)
					*last++	= prev = dst[-1][x + i];
			} else {
				for (int i = 0; i < 4; i++)
					*last++	= prev;
			}
		}
	}

	// fill in missing samples
	if (first)
		fill_n(border_pixels, first - border_pixels, *first);
	else
		last = border_pixels;
	fill_n(last, border_pixels + 4 * nT + 1 - last, prev);
}


// (8.4.4.2.2)
template<typename T> void decode_intra_prediction_internal(const image *img, int xB0, int yB0, IntraPredMode intra_pred, blockptr<T,2> dst, int log2TrSize, int c, bool enableIntraBoundaryFilter, int bit_depth, AVAIL avail_pred) {
#ifdef HEVC_3D
	if (intra_pred == INTRA_SINGLE2 || intra_pred == INTRA_SINGLE3) {
		//I.8.4.4.2.4 Specification of intra prediction mode INTRA_SINGLE
		auto	avail	= img->check_CTB_available(xB0, yB0);
		int		nT		= 1 << log2TrSize;
		auto	val		= (avail & (intra_pred == INTRA_SINGLE2 ? 1 : 2))
			? (intra_pred == INTRA_SINGLE2 ? dst[nT / 2][-1] : dst[-1][nT / 2])
			: 1 << (bit_depth - 1);

		for (int y = 0; y < nT; y++)
			fill_n(dst[y], nT, val);

		return;
	}
#endif

	T		border_pixels[4 * MAX_INTRA_PRED_BLOCK_SIZE + 1];
	get_border_pixels(border_pixels, img, xB0, yB0, c, log2TrSize, bit_depth, avail_pred);

	T*		border_pixels2	= border_pixels + (2 << log2TrSize);

	if (intra_pred == INTRA_DC) {
		intra_prediction_DC(dst, log2TrSize, border_pixels2, c == 0 && log2TrSize < 5);
		return;
	}

	if (log2TrSize > 2 && (c == 0 || img->ChromaArrayType == CHROMA_444)) {
		auto	pps		= img->pps.get();
		auto	sps		= pps->sps.get();
		if (!IF_HAS_RANGE(sps->extension_range.intra_smoothing_disabled) && steepness(intra_pred) > (log2TrSize == 3 ? 7 : log2TrSize == 4 ? 1 : 0))
			intra_prediction_sample_filtering(border_pixels, log2TrSize, c == 0 && log2TrSize == 5 && sps->strong_intra_smoothing_enabled ? 1 << (bit_depth - 5) : 0);
	}

	if (intra_pred == INTRA_PLANAR) {
		intra_prediction_planar(dst, log2TrSize, border_pixels2);

	} else {
		intra_prediction_angular(dst, log2TrSize, border_pixels2, to_angle(intra_pred), bit_depth);

		if (enableIntraBoundaryFilter && (c == 0 && log2TrSize < 5)) {
			int	nT	= 1 << log2TrSize;
			if (intra_pred == INTRA_ANGLE_24) {
				for (int y = 0; y < nT; y++)
					dst[y][0] = clamp(dst[y][0] + ((border_pixels2[-1 - y] - border_pixels2[0])>>1), 0, bits(bit_depth));
			} else if (intra_pred == INTRA_ANGLE_8) {
				for (int x = 0; x < nT; x++)
					dst[0][x] = clamp(dst[0][x] + ((border_pixels2[1 + x] - border_pixels2[0])>>1), 0, bits(bit_depth));
			}
		}
	}
}

// (8.4.4.2.1)
void decode_intra_prediction(image* img, int xB0, int yB0, IntraPredMode intra_pred, int log2TrSize, int c, bool enableIntraBoundaryFilter, AVAIL avail) {
	int	bit_depth	= img->get_bit_depth(c);
	if (bit_depth <= 8)
		decode_intra_prediction_internal(img, xB0, yB0, intra_pred, img->get_plane_ptr<uint8 >(c).slice(xB0, yB0), log2TrSize, c, enableIntraBoundaryFilter, 8, avail);
	else
		decode_intra_prediction_internal(img, xB0, yB0, intra_pred, img->get_plane_ptr<uint16>(c).slice(xB0, yB0), log2TrSize, c, enableIntraBoundaryFilter, bit_depth, avail);
}

void decode_intra_prediction_recurse(image* img, int xB0, int yB0, IntraPredMode intra_pred, int log2TrSize, int c, bool enableIntraBoundaryFilter, AVAIL avail) {
	if (log2TrSize < 6) {
		decode_intra_prediction(img, xB0, yB0, intra_pred, log2TrSize, c, enableIntraBoundaryFilter, avail);
	} else {
		--log2TrSize;
		int	offset	= 1 << log2TrSize;
		decode_intra_prediction_recurse(img, xB0,			yB0,			intra_pred, log2TrSize, c, enableIntraBoundaryFilter, avail_quad0(avail));
		decode_intra_prediction_recurse(img, xB0 + offset,	yB0,			intra_pred, log2TrSize, c, enableIntraBoundaryFilter, avail_quad1(avail));
		decode_intra_prediction_recurse(img, xB0,			yB0 + offset,	intra_pred, log2TrSize, c, enableIntraBoundaryFilter, avail_quad2(avail));
		decode_intra_prediction_recurse(img, xB0 + offset,	yB0 + offset,	intra_pred, log2TrSize, c, enableIntraBoundaryFilter, avail_quad3(avail));
	}
}

#ifdef HEVC_3D

//I.8.4.4.2.5 Depth sub-block partition DC value derivation and assignment process
//I.8.4.4.2.2 Specification of intra prediction mode INTRA_WEDGE
template<typename T, typename M> void intra_prediction_apply_mask(blockptr<T, 2> dst, int log2TrSize, T* border, const M *pattern, int16x2 depth_off, const extension_3d::depth_lookup *dlt, int bit_depth) {
	int		nT			= 1 << log2TrSize;
	bool	pat0		= pattern[0] & 1;
	bool	ver_edge	= pat0 != (pattern[0] >> (nT - 1));
	bool	hor_edge	= pat0 != (pattern[nT - 1] & 1);

	int		val0, val1;

	if (ver_edge == hor_edge) {
		val0	= hor_edge
			? (border[-nT] + border[nT]) >> 1
			: abs(border[1] - border[nT * 2]) > abs(border[-1] - border[-nT * 2])
			? border[ nT * 2]
			: border[-nT * 2];
		val1	= (border[-1] + border[1]) >> 1;
	} else {
		val0	= hor_edge ? border[-nT]		: border[nT];
		val1	= hor_edge ? border[nT >> 1]	: border[-nT >> 1];
	}

	if (!pat0)
		swap(val0, val1);

	int		max	= bits(bit_depth);

	for (int y = 0; y < nT; y++) {
		auto	row		= dst[y];
		auto	pat		= pattern[y];
		for (int x = 0; x < nT; x++, pat >>= 1) {
			int	pred	= pat & 1 ? val1 : val0;
			int	offset	= depth_off[pat & 1];
			row[x]		= dlt ? dlt->fix(pred, offset, bit_depth) : clamp(pred + offset, 0, max);
		}
	}
}

//I.8.4.4.2.3 Specification of intra prediction mode INTRA_CONTOUR
template<typename T, typename M> void intra_prediction_generate_mask(block<T, 2> tex, int x0, int y0, int log2TrSize, M *pattern) {
	int		nT		= 1 << log2TrSize;
	int		width	= tex.template size<1>();
	int		height	= tex.template size<2>();

	x0	= clamp(x0, 0, width - 1);
	y0	= clamp(y0, 0, height - 1);
	int		x1		= min(x0 + nT - 1, width - 1);
	int		y1		= min(y0 + nT - 1, height - 1);

	auto	thresh = (tex[y0][x0] + tex[y1][x0] + tex[y0][x1] + tex[y1][x1]) >> 2;

	for (int y = 0; y < nT; y++) {
		auto	row		= tex[clamp(y0 + y, 0, height - 1)];
		M		pat		= 0;
		for (int x = 0; x < nT; x++)
			pat |= M(row[clamp(x0 + x, 0, width - 1)] > thresh) << x;
		pattern[y] = pat;
	}
}

void decode_depth_intra_prediction(image* img, const image *tex, int xB0, int yB0, const uint16 *wedge, int16x2 depth_off, const extension_3d::depth_lookup* dlt, int log2TrSize, AVAIL avail) {
	uint64			pattern[64];
	int	bit_depth	= img->get_bit_depth(0);

	if (!wedge) {
		if (bit_depth <= 8)
			intra_prediction_generate_mask(tex->get_plane_block<uint8 >(0), xB0, yB0, log2TrSize, pattern);
		else
			intra_prediction_generate_mask(tex->get_plane_block<uint16>(0), xB0, yB0, log2TrSize, pattern);

	} else if (log2TrSize == 5) {
		for (int i = 0; i < 16; i++)
			pattern[i * 2 + 0] = pattern[i * 2 + 1] = part_by_1(wedge[i]) * 3;

	} else {
		for (int i = 0; i < 1 << log2TrSize; i++)
			pattern[i] = wedge[i];
	}

	if (bit_depth <= 8) {
		uint8		border_pixels[4 * MAX_INTRA_PRED_BLOCK_SIZE + 1];
		get_border_pixels(border_pixels, img, xB0, yB0, 0, log2TrSize, bit_depth, avail);
		auto		dst	= img->get_plane_ptr<uint8 >(0).slice(xB0, yB0);
		intra_prediction_apply_mask(dst, log2TrSize, border_pixels + (2 << log2TrSize), pattern, depth_off, dlt, bit_depth);
	} else {
		uint16		border_pixels[4 * MAX_INTRA_PRED_BLOCK_SIZE + 1];
		get_border_pixels(border_pixels, img, xB0, yB0, 0, log2TrSize, bit_depth, avail);
		auto		dst = img->get_plane_ptr<uint16>(0).slice(xB0, yB0);
		intra_prediction_apply_mask(dst, log2TrSize, border_pixels + (2 << log2TrSize), pattern, depth_off, dlt, bit_depth);
	}
}


//I.8.4.4.3 Depth DC offset assignment process
template<typename T> void depth_offset_assignment(blockptr<T, 2> SL, int nT, int depth_off, int bit_depth, const extension_3d::depth_lookup* dlt) {
	if (dlt) {
		auto	pred	= (SL[0][0] + SL[0][nT - 1] + SL[nT - 1][0] + SL[nT - 1][nT - 1] + 2) >> 2;
		depth_off		= dlt->fix(pred, depth_off, bit_depth) - pred;
	}

	int		max	= bits(bit_depth);
	for (int y = 0; y < nT; y++)
		for (int x = 0; x < nT; x++)
			SL[y][x] = clamp(SL[y][x] + depth_off, 0, max);
}

void depth_offset_assignment(image* img, int x0, int y0, int depth_off, const extension_3d::depth_lookup* dlt, int log2TrSize) {
	int		nT			= 1 << log2TrSize;
	int		bit_depth	= img->get_bit_depth(0);
	if (bit_depth <= 8)
		depth_offset_assignment(img->get_plane_ptr<uint8 >(0).slice(x0, y0), nT, depth_off, bit_depth, dlt);
	else
		depth_offset_assignment(img->get_plane_ptr<uint16>(0).slice(x0, y0), nT, depth_off, bit_depth, dlt);
}

#endif

//-----------------------------------------------------------------------------
// inter-prediction
//-----------------------------------------------------------------------------

#define MAX_CU_SIZE 64

template<typename T, int N, int M, int F0> force_inline auto pel2(vec<T, N> src) {
	return shrink<N - M + 1>(src) * F0;
}
template<typename T, int N, int M, int F0, int F1, int... F> force_inline auto pel2(vec<T, N> src) {
	return shrink<N - M + 1>(src) * F0 + pel2<T, N, M, F1, F...>(rotate<1>(src));
}

#if 1
template<bool X> struct pel_row;
template<> struct pel_row<false> {
	template<int N, typename T>			static auto get(blockptr<T, 2> p, int i)			{ return load<N>(p[i]); }
	template<typename V, typename T>	static void put(V v, blockptr<T, 2> p, int i)		{ store(v, p[i]); }
};
template<> struct pel_row<true> {
	template<int N, typename T>			static auto get(blockptr<T, 2> p, int i)			{ return load<N>(strided(p[0] + i, p.pitch())); }
	template<typename V, typename T>	static void put(V v, blockptr<T, 2> p, int i)		{ store(v, strided(p[0] + i, p.pitch())); }
};

template<bool XD, bool XS, typename D, typename S, int N, int...F> force_inline void put_pel(blockptr<D, 2> dst, blockptr<S, 2> src, int n, int shift) {
	if (sizeof(S) == 1)
		shift = 0;
	const int M = sizeof...(F);
	typedef sint_t<sizeof(S) * 2>	I;
	for (auto i = 0; i < n; i++) {
		auto	p = to<I>(pel_row<XS>::template get<N + M - 1>(src, i));
		auto	o = pel2<I, N + M - 1, M, F...>(p) >> shift;
		pel_row<XD>::template put(to<D>(o), dst, i);
	}
}

template<int TAPS, int FRAC> struct pel {
	static int	extra_before(int frac)	{ return pel<TAPS, FRAC - 1>::extra_before(frac >> 1); }
	static int	extra_after(int frac)	{ return pel<TAPS, FRAC - 1>::extra_after(frac >> 1); }
	template<bool XD, bool XS, int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		pel<TAPS, FRAC - 1>::template f<XD, XS, N>(dst, src, frac >> 1, n, shift);
	}
};

template<int FRAC> struct pel<1, FRAC> {
	static int	extra_before(int frac)	{ return 0; }
	static int	extra_after(int frac)	{ return 0; }
	template<bool XD, bool XS, int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		put_pelx<XD, XS, int16, T, N, 64	>(dst, src, n, shift);
	}
};

template<> struct pel<2, 3> {
	static int	extra_before(int frac)	{ return 0; }
	static int	extra_after(int frac)	{ return frac ? 1 : 0; }
	template<bool XD, bool XS, int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		switch (frac) {
			default:
			case 0:		put_pel<XD, XS, int16, T, N, 64	>(dst, src, n, shift); break;
			case 1:		put_pel<XD, XS, int16, T, N, 56,  8>(dst, src, n, shift); break;
			case 2:		put_pel<XD, XS, int16, T, N, 48, 16>(dst, src, n, shift); break;
			case 3:		put_pel<XD, XS, int16, T, N, 40, 24>(dst, src, n, shift); break;
			case 4:		put_pel<XD, XS, int16, T, N, 32, 32>(dst, src, n, shift); break;
			case 5:		put_pel<XD, XS, int16, T, N, 24, 40>(dst, src, n, shift); break;
			case 6:		put_pel<XD, XS, int16, T, N, 16, 48>(dst, src, n, shift); break;
			case 7:		put_pel<XD, XS, int16, T, N,  8, 56>(dst, src, n, shift); break;
		}
	}
};

template<> struct pel<4, 3> {
	static int	extra_before(int frac)	{ return frac ? 1 : 0; }
	static int	extra_after(int frac)	{ return frac ? 2 : 0; }
	template<bool XD, bool XS, int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		switch (frac) {
			default:
			case 0:		put_pel<XD, XS, int16, T, N, 64					>(dst, src, n, shift); break;
			case 1:		put_pel<XD, XS, int16, T, N, -2, +58, +10, -2	>(dst, src, n, shift); break;
			case 2:		put_pel<XD, XS, int16, T, N, -4, +54, +16, -2	>(dst, src, n, shift); break;
			case 3:		put_pel<XD, XS, int16, T, N, -6, +46, +28, -4	>(dst, src, n, shift); break;
			case 4:		put_pel<XD, XS, int16, T, N, -4, +36, +36, -4	>(dst, src, n, shift); break;
			case 5:		put_pel<XD, XS, int16, T, N, -4, +28, +46, -6	>(dst, src, n, shift); break;
			case 6:		put_pel<XD, XS, int16, T, N, -2, +16, +54, -4	>(dst, src, n, shift); break;
			case 7:		put_pel<XD, XS, int16, T, N, -2, +10, +58, -2	>(dst, src, n, shift); break;
		}
	}
};

template<> struct pel<8, 2> {
	static int	extra_before(int frac)	{ static const int8 table[4] = { 0,3,3,2 }; return table[frac]; }
	static int	extra_after(int frac)	{ static const int8 table[4] = { 0,3,4,4 }; return table[frac]; }
	template<bool XD, bool XS, int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		switch (frac) {
			default:
			case 0:	put_pel<XD, XS, int16, T, N, 64									>(dst, src, n, shift); break;
			case 1:	put_pel<XD, XS, int16, T, N, -1, +4, -10, +58, +17, - 5, +1		>(dst, src, n, shift); break;
			case 2:	put_pel<XD, XS, int16, T, N, -1, +4, -11, +40, +40, -11, +4, -1	>(dst, src, n, shift); break;
			case 3:	put_pel<XD, XS, int16, T, N,  1, -5, +17, +58, -10, + 4, -1		>(dst, src, n, shift); break;
		}
	}
};

template<int TAPS, int FRAC, bool XD, bool XS, typename S> void mc2(blockptr<int16, 2> dst, blockptr<S, 2> src, int frac, int m, int n, int shift) {
	switch (m) {
		case 2:		pel<TAPS, FRAC>::template f<XD, XS, 2 , S>(dst, src, frac, n, shift); break;
		case 4:		pel<TAPS, FRAC>::template f<XD, XS, 4 , S>(dst, src, frac, n, shift); break;
		case 8:		pel<TAPS, FRAC>::template f<XD, XS, 8 , S>(dst, src, frac, n, shift); break;
		case 16:	pel<TAPS, FRAC>::template f<XD, XS, 16, S>(dst, src, frac, n, shift); break;
		case 32:	pel<TAPS, FRAC>::template f<XD, XS, 32, S>(dst, src, frac, n, shift); break;
		default:	ISO_ASSERT(0);
	}
}

template<int TAPS, int FRAC, bool XD, bool XS, typename S> void mc1(blockptr<int16, 2> dst, blockptr<S, 2> src, int frac, int m, int n, int shift) {
	int		m0			= min(lowest_set(m), 32);
	mc2<TAPS, FRAC, XD, XS, S>(dst, src, frac, m0, n, shift);
	if (m != m0)
		mc2<TAPS, FRAC, XD, XS, S>(dst.template slice<XD+1>(m0), src.template slice<XS+1>(m0), frac, m - m0, n, shift);
}

template<int TAPS, int FRAC, typename T> void mc(MotionVector mv, block<int16,2> out, block<const T, 2> ref, int bit_depth) {
	int		w		= out.template size<1>(), h = out.template size<2>();
	int		wL		= ref.template size<1>(), hL = ref.template size<2>();
	auto	frac	= mv & bits(FRAC);

	int		x0		= mv.x >> FRAC;
	int		y0		= mv.y >> FRAC;
	int		x1		= x0 - pel<TAPS, FRAC>::extra_before(frac.x);
	int		y1		= y0 - pel<TAPS, FRAC>::extra_before(frac.y);
	int		x2		= x0 + w + pel<TAPS, FRAC>::extra_after(frac.x);
	int		y2		= y0 + h + pel<TAPS, FRAC>::extra_after(frac.y);

	T	padbuf[MAX_CU_SIZE + 7][MAX_CU_SIZE + 16];
	blockptr<const T, 2>	src;

	if (x1 >= 0 && y1 >= 0 && x2 < wL && y2 < hL) {
		src = ref.template sub<1>(x1, x2 - x1).template sub<2>(y1, y2 - y1);

	} else {
		for (int y = y1; y < y2; y++) {
			for (int x = x1; x < x2; x++)
				padbuf[y - y1][x - x1] = ref[clamp(y, 0, hL - 1)][clamp(x, 0, wL - 1)];
		}
		src = make_blockptr(make_const(padbuf));
	}

#if 1
	int		hs		= y2 - y1;
	int16*	tempbuf	= alloc_auto(int16, w * hs);
	auto	tempptr = make_blockptr(tempbuf, w * 2);

	// H-filters
	mc1<TAPS, FRAC, false, false, const T>(tempptr, src, frac.x, w, hs, bit_depth - 8);
	// V-filters
	mc1<TAPS, FRAC, true, true, int16>(out, tempptr, frac.y, h, w, 6);
#elif 0
	int		ws		= x2 - x1;
	int16*	tempbuf	= alloc_auto(int16, ws * h);
	auto	tempptr = make_blockptr(tempbuf, ws * 2);

	// V-filters
	mc1<X, true, true, const T>(tempptr, src, frac.y, h, ws, bit_depth - 8);
	// H-filters
	mc1<X, false, false, int16>(out, tempptr, frac.x, w, h, 6);
#else
	int		ws		= x2 - x1;
	int16*	tempbuf	= alloc_auto(int16, ws * h);
	auto	tempptr = make_blockptr(tempbuf, h * 2);

	// V-filters
	mc1<TAPS, FRAC, false, true, const T>(tempptr, src, frac.y, h, ws, bit_depth - 8);
	// H-filters
	mc1<TAPS, FRAC, false, true, int16>(out, tempptr, frac.x, w, h, 6);
#endif
}

#else

template<typename D, typename S, int N, int...F> force_inline void put_pel(blockptr<D, 2> dst, blockptr<S, 2> src, int n, int shift) {
	if (sizeof(S) == 1)
		shift = 0;
	const int M = sizeof...(F);
	typedef sint_t<sizeof(S) * 2>	I;
	auto	offsets = to<int>(meta::make_value_sequence<N + M - 1, int>()) * (src.pitch() / sizeof(S));
	for (auto i = 0; i < n; i++) {
		auto	p = to<I>(gather(src[0] + i, offsets));
		auto	o = pel2<I, N + M - 1, M, F...>(p) >> shift;
		store(to<D>(o), dst[i]);
	}
}

template<int TAPS, int FRAC> struct pel {
	static int	extra_before(int frac)	{ return pel<TAPS, FRAC - 1>::extra_before(frac >> 1); }
	static int	extra_after(int frac)	{ return pel<TAPS, FRAC - 1>::extra_after(frac >> 1); }
	template<int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		pel<TAPS, FRAC - 1>::template f<N>(dst, src, frac >> 1, n, shift);
	}
};

template<int FRAC> struct pel<1, FRAC> {
	static int	extra_before(int frac)	{ return 0; }
	static int	extra_after(int frac)	{ return 0; }
	template<int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		put_pelx<int16, T, N, 64	>(dst, src, n, shift);
	}
};

template<> struct pel<2, 3> {
	static int	extra_before(int frac)	{ return 0; }
	static int	extra_after(int frac)	{ return frac ? 1 : 0; }
	template<int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		switch (frac) {
			default:
			case 0:		put_pel<int16, T, N, 64	>(dst, src, n, shift); break;
			case 1:		put_pel<int16, T, N, 56,  8>(dst, src, n, shift); break;
			case 2:		put_pel<int16, T, N, 48, 16>(dst, src, n, shift); break;
			case 3:		put_pel<int16, T, N, 40, 24>(dst, src, n, shift); break;
			case 4:		put_pel<int16, T, N, 32, 32>(dst, src, n, shift); break;
			case 5:		put_pel<int16, T, N, 24, 40>(dst, src, n, shift); break;
			case 6:		put_pel<int16, T, N, 16, 48>(dst, src, n, shift); break;
			case 7:		put_pel<int16, T, N,  8, 56>(dst, src, n, shift); break;
		}
	}
};

template<> struct pel<4, 3> {
	static int	extra_before(int frac)	{ return frac ? 1 : 0; }
	static int	extra_after(int frac)	{ return frac ? 2 : 0; }
	template<int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		switch (frac) {
			default:
			case 0:		put_pel<int16, T, N, 64					>(dst, src, n, shift); break;
			case 1:		put_pel<int16, T, N, -2, +58, +10, -2	>(dst, src, n, shift); break;
			case 2:		put_pel<int16, T, N, -4, +54, +16, -2	>(dst, src, n, shift); break;
			case 3:		put_pel<int16, T, N, -6, +46, +28, -4	>(dst, src, n, shift); break;
			case 4:		put_pel<int16, T, N, -4, +36, +36, -4	>(dst, src, n, shift); break;
			case 5:		put_pel<int16, T, N, -4, +28, +46, -6	>(dst, src, n, shift); break;
			case 6:		put_pel<int16, T, N, -2, +16, +54, -4	>(dst, src, n, shift); break;
			case 7:		put_pel<int16, T, N, -2, +10, +58, -2	>(dst, src, n, shift); break;
		}
	}
};

template<> struct pel<8, 2> {
	static int	extra_before(int frac)	{ static const int8 table[4] = { 0,3,3,2 }; return table[frac]; }
	static int	extra_after(int frac)	{ static const int8 table[4] = { 0,3,4,4 }; return table[frac]; }
	template<int N, typename T> static void f(blockptr<int16, 2> dst, blockptr<T, 2> src, int frac, int n, int shift) {
		switch (frac) {
			default:
			case 0:	put_pel<int16, T, N, 64									>(dst, src, n, shift); break;
			case 1:	put_pel<int16, T, N, -1, +4, -10, +58, +17, - 5, +1		>(dst, src, n, shift); break;
			case 2:	put_pel<int16, T, N, -1, +4, -11, +40, +40, -11, +4, -1	>(dst, src, n, shift); break;
			case 3:	put_pel<int16, T, N,  1, -5, +17, +58, -10, + 4, -1		>(dst, src, n, shift); break;
		}
	}
};

template<int TAPS, int FRAC, typename S> void mc2(blockptr<int16, 2> dst, blockptr<S, 2> src, int frac, int m, int n, int shift) {
	switch (m) {
		case 2:		pel<TAPS, FRAC>::template f<2 , S>(dst, src, frac, n, shift); break;
		case 4:		pel<TAPS, FRAC>::template f<4 , S>(dst, src, frac, n, shift); break;
		case 8:		pel<TAPS, FRAC>::template f<8 , S>(dst, src, frac, n, shift); break;
		case 16:	pel<TAPS, FRAC>::template f<16, S>(dst, src, frac, n, shift); break;
		case 32:	pel<TAPS, FRAC>::template f<32, S>(dst, src, frac, n, shift); break;
		default:	ISO_ASSERT(0);
	}
}

template<int TAPS, int FRAC, typename S> void mc1(blockptr<int16, 2> dst, blockptr<S, 2> src, int frac, int m, int n, int shift) {
	int		m0			= min(lowest_set(m), 32);
	mc2<TAPS, FRAC, S>(dst, src, frac, m0, n, shift);
	if (m != m0)
		mc2<TAPS, FRAC, S>(dst.template slice<1>(m0), src.template slice<2>(m0), frac, m - m0, n, shift);
}

template<int TAPS, int FRAC, typename T> void mc(MotionVector mv, block<int16,2> out, block<const T, 2> ref, int bit_depth) {
	int		w		= out.template size<1>(), h = out.template size<2>();
	int		wL		= ref.template size<1>(), hL = ref.template size<2>();
	auto	frac	= mv & bits(FRAC);

	int		x0		= mv.x >> FRAC;
	int		y0		= mv.y >> FRAC;
	int		x1		= x0 - pel<TAPS, FRAC>::extra_before(frac.x);
	int		y1		= y0 - pel<TAPS, FRAC>::extra_before(frac.y);
	int		x2		= x0 + w + pel<TAPS, FRAC>::extra_after(frac.x);
	int		y2		= y0 + h + pel<TAPS, FRAC>::extra_after(frac.y);

	T	padbuf[MAX_CU_SIZE + 7][MAX_CU_SIZE + 16];
	blockptr<const T, 2>	src;

	if (x1 >= 0 && y1 >= 0 && x2 < wL && y2 < hL) {
		src = ref.template sub<1>(x1, x2 - x1).template sub<2>(y1, y2 - y1);

	} else {
		for (int y = y1; y < y2; y++) {
			for (int x = x1; x < x2; x++)
				padbuf[y - y1][x - x1] = ref[clamp(y, 0, hL - 1)][clamp(x, 0, wL - 1)];
		}
		src = make_blockptr(make_const(padbuf));
	}
	int		ws		= x2 - x1;
	int16*	tempbuf	= alloc_auto(int16, ws * h);
	auto	tempptr = make_blockptr(tempbuf, h * 2);

	// V-filters
	mc1<TAPS, FRAC, const T>(tempptr, src, frac.y, h, ws, bit_depth - 8);
	// H-filters
	mc1<TAPS, FRAC, int16>(out, tempptr, frac.x, w, h, 6);
}
#endif

// 8.5.3.2.1
bool generate_inter_prediction_samples(image* img, const image *ref, int xP, int yP, int w, int h, MotionVector mv, int16 predSamples[3][MAX_CU_SIZE* MAX_CU_SIZE]) {
	if (!ref || !img->compatible(ref))
		return false;

	// 8.5.3.2.2
	//mv.x += xP << 2;
	//mv.y += yP << 2;

	mv.x = (mv.x << 1) + (xP << 3);
	mv.y = (mv.y << 1) + (yP << 3);

	auto		out				= make_block(predSamples[0], w, h);
	const int	bit_depth_luma	= img->planes[0].bit_depth;
	if (bit_depth_luma <= 8)
		mc<8,3>(mv, out, ref->get_plane_block<const uint8>(0), bit_depth_luma);
	else
		mc<8,3>(mv, out, ref->get_plane_block<const uint16>(0), bit_depth_luma);

	if (img->chroma_format != CHROMA_MONO) {
		const int	shiftw	= get_shift_W(img->chroma_format);
		const int	shifth	= get_shift_H(img->chroma_format);

		w	>>= shiftw;
		h	>>= shifth;
		mv.x >>= shiftw;
		mv.y >>= shifth;

		auto		out1				= make_block(predSamples[1], w, h);
		auto		out2				= make_block(predSamples[2], w, h);
		const int	bit_depth_chroma	= img->planes[1].bit_depth;
		if (bit_depth_chroma <= 8) {
			mc<4,3>(mv, out1, ref->get_plane_block<const uint8>(1), bit_depth_chroma);
			mc<4,3>(mv, out2, ref->get_plane_block<const uint8>(2), bit_depth_chroma);
		} else {
			mc<4,3>(mv, out1, ref->get_plane_block<const uint16>(1), bit_depth_chroma);
			mc<4,3>(mv, out2, ref->get_plane_block<const uint16>(2), bit_depth_chroma);
		}
	}
	return true;
}

#ifdef HEVC_3D

//-----------------------------------------------------------------------------
// disparity vectors
//-----------------------------------------------------------------------------

int	estimate_disparity(const image *dimg, int x, int y, int w, int h, const extension_3d::cp *cp, int cp_precision) {
	auto	width	= dimg->get_width();
	auto	height	= dimg->get_height();
	int		x0		= clamp(x, 0, width - 1);
	int		y0		= clamp(y, 0, height - 1);
	int		x1		= clamp(x + w - 1, 0, width - 1);
	int		y1		= clamp(y + h - 1, 0, height - 1);

	int		depth;
	auto	bit_depth = dimg->get_bit_depth(0);
	if (bit_depth <= 8) {
		auto	ref = dimg->get_plane_block<uint8 >(0);
		depth = max(max(max(ref[y0][x0], ref[y1][x0]), ref[y0][x1]), ref[y1][x1]);
	} else {
		auto	ref = dimg->get_plane_block<uint16>(0);
		depth = max(max(max(ref[y0][x0], ref[y1][x0]), ref[y0][x1]), ref[y1][x1]);
	}

	return cp ? cp->DepthToDisparityB(depth, bit_depth, cp_precision) : depth;
}

int	estimate_disparity(const image *dimg, int16x2 p, int w, int h, const extension_3d::cp *cp, int cp_precision) {
	return estimate_disparity(dimg, p.x, p.y, w, h, cp, cp_precision);
}

struct Disparity {
	MotionVector	raw			= zero;
	MotionVector	refined		= zero;
	int				ref_view	= -1;
};

struct DisparityDeriver {
	const image*	co2	= nullptr;
	edge_history2<Disparity, 6, 3>	history;
	int				refine	= 0;	//0: none, 1: y=0, 2: full

	static bool check(const image *img, int16x2 R, Disparity &dv, uint64 view_mask);

	void		init(slice_segment_header *shdr, int poc, int view, int _refine);
	Disparity	get(image *img, slice_segment_header *shdr, int x0, int y0, int log2CbSize, bool merge, AVAIL avail);
};

bool DisparityDeriver::check(const image *img, int16x2 R, Disparity &dv, uint64 view_mask) {
	if (check_image_ref(img, R)) {
		auto&	coPb	= img->get_mv_info(R);
		auto	coSh	= img->get_SliceHeader(R);

		for (int X = 0; X < 2; X++) {
			if (coPb.refIdx[X] >= 0) {
				int	v = coSh->ref(X, coPb.refIdx[X]).view_idx;
				if (v != img->view_idx && (view_mask & bit64(v))) {
					dv.raw		= coPb.mv[X];
					dv.ref_view	= v;
					return true;
				}
			}
		}
	}
	return false;
}

void DisparityDeriver::init(slice_segment_header *shdr, int poc, int view, int _refine) {
	refine = _refine;

	//I.8.3.1	Derivation process for the candidate picture list for disparity vector derivation
	if (shdr->temporal_mvp_enabled) {
		int		min_temporal	= 7;
		int		min_poc_diff	= 255;
		auto&	co				= shdr->ref(shdr->collocated);

		bool	found	= false;
		for (int k = 0, nk = shdr->num_ref_lists(); k < nk && !found; k++) {
			for (auto &i : shdr->get_ref_pics(k ^ shdr->collocated.list)) {
				if (view == i.view_idx && &i != &co) {
					if (isIRAP(i.img->slices[0]->unit_type)) {
						co2		= i.img;
						found	= true;
						break;
					} else {
						int	temporal = i.img->temporal_id;
						if (temporal < min_temporal) {
							min_temporal	= temporal;
							min_poc_diff	= 255;
						}
						if (temporal == min_temporal) {
							auto	diff = abs(poc - i.poc);
							if (diff < min_poc_diff) {
								min_poc_diff	= diff;
								co2				= i.img;
							}
						}
					}
				}
			}
		}
	}
}

//I.8.5.5 Derivation process for a disparity vector for texture layers
Disparity DisparityDeriver::get(image *img, slice_segment_header *shdr, int x0, int y0, int log2CbSize, bool merge, AVAIL avail) {
	Disparity	dv{zero, zero, shdr->extension_3d.default_view};
	if (dv.ref_view < 0)
		return dv;
		
	auto	pps		= img->pps.get();

	if (shdr->extension_3d.depth_layer) {
		auto	vps3d		= pps->sps->vps->extension_3d.exists_ptr();
		auto	cp			= vps3d->get_cp(img->view_idx, dv.ref_view, shdr->extension_3d.cps);
		int		bit_depth	= img->get_bit_depth(0);
		int		d			= 1 << (bit_depth - 1);
		dv.raw.x	= cp ? cp->DepthToDisparityB(d, bit_depth, vps3d->cp_precision) : d;
		dv.refined	= dv.raw;
		return dv;
	}

	bool	valid	= false;
	int		nC		= 1 << log2CbSize;
	auto	C		= int16x2{x0, y0};

	if (shdr->temporal_mvp_enabled) {
		//I.8.5.5.2 Derivation process for a disparity vector from temporal neighbouring blocks
		int16x2	R	= (C + nC / 2) & -16;
		valid		= check(shdr->ref(shdr->collocated).img, R, dv, shdr->extension_3d.view_mask)
				||	  check(co2, R, dv, shdr->extension_3d.view_mask);
	}

	if (!valid) {
		const int	poc			= img->picture_order_cnt;
		auto		pred_mode	= img->get_pred_mode(x0, y0);
		const int	merge_level = pred_mode == MODE_SKIP || (pred_mode == MODE_INTER && merge) ? pps->log2_parallel_merge_level : 0;

		int16x2		A1			= {x0 - 1, y0 + nC - 1};
		bool		avail_a1	= (avail & AVAIL_A1) && img->get_pred_mode(A1) != MODE_INTRA && (merge_level == 0 || check_merge(C, A1, merge_level));
		if (avail_a1) {
			auto&	vi	= img->get_mv_info(A1);
			int		ref	= vi.refIdx[0] >= 0 && shdr->ref(0, vi.refIdx[0]).poc == poc ? 0
				: vi.refIdx[1] >= 0 && shdr->ref(1, vi.refIdx[1]).poc == poc ? 1
				: -1;
			if (ref >= 0) {
				dv.raw		= vi.mv[ref];
				dv.ref_view	= shdr->ref(ref, vi.refIdx[ref]).view_idx;
				valid		= true;
			}
		}

		if (!valid) {
			int16x2		B1			= {x0 + nC - 1, y0 - 1};
			bool		avail_b1	= (avail & AVAIL_B1) && img->get_pred_mode(B1) != MODE_INTRA && (merge_level == 0 || check_merge(C, B1, merge_level));
			if (avail_b1) {
				auto&	vi	= img->get_mv_info(B1);
				int		ref	= vi.refIdx[0] >= 0 && shdr->ref(0, vi.refIdx[0]).poc == poc ? 0
					: vi.refIdx[1] >= 0 && shdr->ref(1, vi.refIdx[1]).poc == poc ? 1
					: -1;
				if (ref >= 0) {
					dv.raw		= vi.mv[ref];
					dv.ref_view	= shdr->ref(ref, vi.refIdx[ref]).view_idx;
					valid		= true;
				}
			}

			if (!valid) {
				if (avail_a1 && img->get_pred_mode(A1) == MODE_SKIP && (img->pb_info.get(A1).flags & PB_info::IV)) {
					dv		= history[0].get(A1.y);
					dv.raw	= dv.refined;

				} else if (avail_b1 && (y0 & bits(img->ctu_info.log2unitSize)) && img->get_pred_mode(B1) == MODE_SKIP && (img->pb_info.get(B1).flags & PB_info::IV)) {
					dv		= history[1].get(B1.x);
					dv.raw	= dv.refined;
				}
			}
		}
	}

	dv.refined = dv.raw;

	if (dv.ref_view >= 0 && refine) {
		dv.refined.y	= 0;
		if (refine > 1) {
			auto	vps3d	= pps->sps->vps->extension_3d.exists_ptr();
			auto	dimg	= shdr->extension_3d.depth_images[dv.ref_view];
			auto	cp		= vps3d->get_cp(img->view_idx, dv.ref_view, shdr->extension_3d.cps);
			dv.refined.x	= estimate_disparity(dimg, offset_pos(C, dv.raw), nC, nC, cp, vps3d->cp_precision);
		}
	}

	history.set(x0, y0, log2CbSize, dv);
	return dv;
}

//I.8.5.3.2.12: Derivation process for disparity information merging candidates
bool derive_disparity_merging_candidates(const slice_segment_header* shdr, int poc, const Disparity &d, PB_info &out) {
	bool	valid	= false;

	for (int X = 0, nX = shdr->num_ref_lists(); X < nX; X++) {
		if (auto i = shdr->find_ref(X, poc, d.ref_view)) {
			out.set(X, shdr->get_ref_pics(X).index_of(i), {d.refined.x, 0});
			valid	= true;
		}
	}
	out.flags = 0;
	return valid;
}

//I.8.5.3.2.13: Derivation process for a view synthesis prediction merging candidate
bool derive_viewsynthesis_merging_candidates(const slice_segment_header* shdr, const Disparity &d, int xP, int yP, int w, int h, PB_info &out) {
	for (int X = 0, nX = shdr->num_ref_lists(); X < nX; X++) {
		for (auto &i : shdr->get_ref_pics(X)) {
			if (i.view_idx == d.ref_view) {
				out.flags		= PB_info::VSP;
				out.refIdx[X]	= shdr->get_ref_pics(X).index_of(i);
				out.refIdx[!X]	= -1;
				return true;
			}
		}
	}
	return false;
}

bool derive_viewsynthesis_hsplit(const image* dimg, int16x2 R, int w, int h) {
	if (((w | h) & 7))
		return !!(h & 7);

	auto	R0		= clamp_to(dimg, R);
	auto	R1		= clamp_to(dimg, R + int16x2{w - 1, h - 1});
	if (dimg->get_bit_depth(0) <= 8) {
		auto	p	= dimg->get_plane_block<uint8 >(0);
		return ((p[R0.y][R0.x] < p[R1.y][R1.x]) == (p[R0.y][R1.x] < p[R1.y][R0.x]));
	} else {
		auto	p	= dimg->get_plane_block<uint16>(0);
		return ((p[R0.y][R0.x] < p[R1.y][R1.x]) == (p[R0.y][R1.x] < p[R1.y][R0.x]));
	}
}

//I.8.5.3.2.10 Derivation process for motion vectors for an inter-view predicted merging candidate
bool derive_interview_motionvectors(const slice_segment_header* shdr, const Disparity &d, int xP, int yP, int w, int h, bool shift, PB_info &out) {
	const image	*ivref	= nullptr;
	for (auto &i : shdr->get_ref_pics(0)) {
		if (i.view_idx == d.ref_view) {
			ivref = i.img;
			break;
		}
	}
	if (!ivref)
		return false;

	MotionVector disp_vec	= d.refined;
	if (shift)
		disp_vec += MotionVector{w * 2, h * 2};

	auto	sps		= shdr->pps->sps.get();
	int		xR		= clamp(xP + (w >> 1) + ((disp_vec.x + 2) >> 2), 0, sps->pic_width_luma  - 1) & ~7;
	int		yR		= clamp(yP + (h >> 1) + ((disp_vec.y + 2) >> 2), 0, sps->pic_height_luma - 1) & ~7;
	bool	valid	= false;

	out.flags		= PB_info::IV;
	out.refIdx[0]	= out.refIdx[1] = -1;

	if (ivref->get_pred_mode(xR, yR) != MODE_INTRA) {
		auto	ivrefpb	= ivref->get_mv_info(xR, yR);
		auto	ivrefsh	= ivref->get_SliceHeader(xR, yR);

		for (int X = 0, nX = shdr->num_ref_lists(); X < nX; X++) {
			for (int k = 0; k < 2; k++) {
				int	Y = X ^ k;
				if (ivrefpb.refIdx[Y] >= 0) {
					auto	refpoc	= ivrefsh->ref(Y, ivrefpb.refIdx[Y]).poc;
					int		idx		= 0;
					for (auto &i : shdr->get_ref_pics(X)) {
						if (refpoc == i.poc) {
							out.set(X, idx, ivrefpb.mv[Y]);
							valid	= true;
							break;
						}
						++idx;
					}
					if (out.refIdx[X] >= 0)
						break;
				}
			}
		}
	}

	return valid;
}

//I.8.5.3.2.11: Derivation process for motion vectors for the texture merge candidate
bool derive_texture_motionvectors(const slice_segment_header* shdr, int xP, int yP, int w, int h, PB_info &out) {
	int		xR		= (xP + (w >> 1)) & -8;
	int		yR		= (yP + (h >> 1)) & -8;
	bool	valid	= false;
	auto	tex		= shdr->extension_3d.reftex;

	out.flags		= 0;
	out.refIdx[0]	= out.refIdx[1] = -1;

	auto	texpb	= tex->get_mv_info(xR, yR);
	auto	texsh	= tex->get_SliceHeader(xR, yR);

	for (int X = 0, nX = shdr->num_ref_lists(); X < nX; X++) {
		int	refIdx = texpb.refIdx[X];
		if (refIdx >= 0) {
			auto	texref	= texsh->ref(X, refIdx);
			int		idx		= 0;
			for (auto &i : shdr->get_ref_pics(X)) {
				if (i.poc == texref.poc && i.view_idx == texref.view_idx) {
					out.set(X, idx, (texpb.mv[X] + 2) >> 2);
					valid	= true;
					break;
				}
				idx++;
			}
		}
	}
	return valid;
}

bool generate_inter_prediction_depthsamples(image* img, const image *ref, int xP, int yP, int w, int h, MotionVector mv, int16 predSamples[MAX_CU_SIZE* MAX_CU_SIZE]) {
	if (!ref || !img->compatible(ref))
		return false;

	int		wL	= img->get_width(), hL = img->get_height();
	int		x0	= xP + mv.x;
	int		y0	= yP + mv.y;
	int		x1	= x0 + w;
	int		y1	= y0 + h;
	int16*	d	= predSamples;

	if (img->planes[0].bit_depth <= 8) {
		auto	src = ref->get_plane_ptr<const uint8>(0);
		for (int y = y0; y < y1; y++) {
			auto	s = src[clamp(y, 0, hL - 1)];
			for (int x = x0; x < x1; x++)
				*d++ = s[clamp(x, 0, wL - 1)] << 6;
		}
	} else {
		auto	src = ref->get_plane_ptr<const uint16>(0);
		for (int y = y0; y < y1; y++) {
			auto	s = src[clamp(y, 0, hL - 1)];
			for (int x = x0; x < x1; x++)
				*d++ = s[clamp(x, 0, wL - 1)] << 6;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// depth predicted sub-block partitions	(dbbp)
//-----------------------------------------------------------------------------

template<typename T, typename S, typename M> void put_dbbp(block<T, 2> dst, iblockptr<S, 2> src0, iblockptr<S, 2> src1, const M *pattern, int bit_depth, bool vertical) {
	if (pattern[0] & 1)
		swap(src0, src1);

	T	temp[MAX_CU_SIZE* MAX_CU_SIZE];
	int	nC		= dst.template size<1>();
	int	shift	= 14 - bit_depth;
	int maxval	= bits(bit_depth);
	int	rnd		= (1 << shift) >> 1;

	for (int y = 0; y < nC; y++) {
		auto	in0 = src0[y];
		auto	in1 = src1[y];
		auto	pat	= pattern[y];
		for (int x = 0; x < nC; x++) {
			temp[y * nC + x] = clamp(((pat & 1 ? in1[x] : in0[x]) + rnd) >> shift, 0, maxval);
			//in0[x] = (in0[x] + rnd) >> shift;
			//in1[x] = (in1[x] + rnd) >> shift;
			pat >>= 1;
		}
	}

	int	offset = vertical ? 1 : nC;

	for (int y = 0; y < nC; y++) {
		auto	tmp	= temp + y * nC;
		auto	out	= dst[y].begin();
		auto	pat	= pattern[y];
		auto	dif	= vertical
			? ((pat << 1) | (pat & 1)) ^ ((pat >> 1) | (pat & bit<M>(nC - 1)))
			: pattern[max(y - 1, 0)] ^ pattern[min(y + 1, nC - 1)];

		for (int x = 0; x < nC; x++, tmp++, dif >>= 1) {
			*out++	= dif & 1
				?	(tmp[0] * 2
					+ tmp[(vertical ? x : y) > 0		? -offset : 0]
					+ tmp[(vertical ? x : y) < nC - 1	? +offset : 0]
					) >> 2
				:	tmp[0];
		}
	}
}

//I.8.5.3.5
bool decode_inter_prediction_dbbp(const slice_segment_header* shdr, image* img, int xP, int yP, int log2CbSize, const Disparity& dv, const PB_info *vi, bool vertical) {
	int16		predSamples[2][3][MAX_CU_SIZE* MAX_CU_SIZE];
	uint64		pattern[64];

	int			xR		= xP + ((dv.refined.x + 2) >> 2);
	int			yR		= yP + ((dv.refined.y + 2) >> 2);
	auto		dimg	= shdr->extension_3d.depth_images[dv.ref_view];
	if (dimg->get_bit_depth(0) <= 8)
		intra_prediction_generate_mask(dimg->get_plane_block<uint8 >(0), xR, yR, log2CbSize, pattern);
	else
		intra_prediction_generate_mask(dimg->get_plane_block<uint16>(0), xR, yR, log2CbSize, pattern);

	int			nC		= 1 << log2CbSize;
	int			i0		= vi[0].refIdx[0] < 0;
	int			i1		= vi[1].refIdx[0] < 0;
	if (!generate_inter_prediction_samples(img, shdr->ref(i0, vi[0].refIdx[i0]).img, xP, yP, nC, nC, vi[0].mv[i0], predSamples[0])
	||  !generate_inter_prediction_samples(img, shdr->ref(i1, vi[1].refIdx[i1]).img, xP, yP, nC, nC, vi[1].mv[i1], predSamples[1])
	)
		return false;

	int	bit_depth = img->planes[0].bit_depth;
	if (bit_depth <= 8)
		put_dbbp(img->get_plane_block<uint8 >(0, xP, yP, nC, nC), make_iblockptr(predSamples[0][0], nC), make_iblockptr(predSamples[1][0], nC), pattern, bit_depth, vertical);
	else
		put_dbbp(img->get_plane_block<uint16>(0, xP, yP, nC, nC), make_iblockptr(predSamples[0][0], nC), make_iblockptr(predSamples[1][0], nC), pattern, bit_depth, vertical);

	if (img->chroma_format != CHROMA_MONO) {
		const int	shiftw	= get_shift_W(img->chroma_format);
		const int	shifth	= get_shift_H(img->chroma_format);
		const int	xPc		= xP >> shiftw;
		const int	yPc		= yP >> shifth;
		const int	wc		= nC >> shiftw;
		const int	hc		= nC >> shifth;

		if (shifth) {
			for (int i = 0; i < hc; i++)
				pattern[i] = pattern[i * 2];
		}
		if (shiftw) {
			for (int i = 0; i < hc; i++)
				pattern[i] = even_bits(pattern[i]);
		}

		int	bit_depth = img->planes[1].bit_depth;
		if (bit_depth <= 8) {
			put_dbbp(img->get_plane_block<uint8 >(1, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][1], wc), make_iblockptr(predSamples[1][1], wc), pattern, bit_depth, vertical);
			put_dbbp(img->get_plane_block<uint8 >(2, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][2], wc), make_iblockptr(predSamples[1][2], wc), pattern, bit_depth, vertical);
		} else {
			put_dbbp(img->get_plane_block<uint16>(1, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][1], wc), make_iblockptr(predSamples[1][1], wc), pattern, bit_depth, vertical);
			put_dbbp(img->get_plane_block<uint16>(2, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][2], wc), make_iblockptr(predSamples[1][2], wc), pattern, bit_depth, vertical);
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// illum
//-----------------------------------------------------------------------------

//Table I.3 - Specification of divCoeff depending on sDenomDiv
static const int	divCoeff[64] = {
	0,		32768,	16384,	10923,	8192,	6554,	5461,	4681,
	4096,	3641,	3277,	2979,	2731,	2521,	2341,	2185,
	2048,	1928,	1820,	1725,	1638,	1560,	1489,	1425,
	1365,	1311,	1260,	1214,	1170,	1130,	1092,	1057,
	1024,	993,	964,	936,	910,	886,	862,	840,
	819,	799,	780,	762,	745,	728,	712,	697,
	683,	669,	655,	643,	630,	618,	607,	596,
	585,	575,	565,	555,	546,	537,	529,	520,	
};

//I.8.5.3.3.2 Illumination compensated sample prediction process
template<typename T> int edge_sum(blockptr<T, 2> plane, int w, int h, AVAIL avail, int step) {
	int		sum		= 0;

	if (avail & AVAIL_A1) {
		for (int i = 0; i < h; i += step)
			sum += plane[i][-1];
	}
	if (avail & AVAIL_B1) {
		for (int i = 0; i < w; i += step)
			sum += plane[-1][i];
	}
	return sum;
}

int32x3 edge_sums(const image *img, int xP, int yP, int w, int h, CHROMA chroma, AVAIL avail) {
	int32x3	sum;
	sum.x	= img->planes[0].bit_depth <= 8
			? edge_sum(img->get_plane_ptr<uint8 >(0).slice(xP, yP), w, h, avail, 2)
			: edge_sum(img->get_plane_ptr<uint16>(0).slice(xP, yP), w, h, avail, 2);

	if (chroma != CHROMA_MONO) {
		const int	shiftw				= get_shift_W(img->chroma_format);
		const int	shifth				= get_shift_H(img->chroma_format);
		xP	>>= shiftw;
		yP	>>= shifth;
		w	>>= shiftw;
		h	>>= shifth;
		if (img->planes[1].bit_depth <= 8) {
			sum.y	= edge_sum(img->get_plane_ptr<uint8 >(1).slice(xP, yP), w, h, avail, 2);
			sum.z	= edge_sum(img->get_plane_ptr<uint8 >(2).slice(xP, yP), w, h, avail, 2);
		} else {
			sum.y	= edge_sum(img->get_plane_ptr<uint16>(1).slice(xP, yP), w, h, avail, 2);
			sum.z	= edge_sum(img->get_plane_ptr<uint16>(2).slice(xP, yP), w, h, avail, 2);
		}
	}
	return sum;
}

//I.8.5.3.3.2.2 Derivation process for illumination compensation mode availability and parameters

template<typename T> int32x2 calculate_illum_weight_luma(const blockptr<T, 2> cur, const block<T, 2> ref, int16x2 R, int w, int h, int sum_cur, int bit_depth, AVAIL avail) {
	if (!(avail & (AVAIL_A1 | AVAIL_B1)))
		return {0, 32};

	int		sum_ref		= 0;
	int		width		= ref.template size<1>();
	int		height		= ref.template size<2>();

	int		num_samples	= (avail & AVAIL_A1 ? h : 0) + (avail & AVAIL_B1 ? w : 0);
	int		avg_shift	= log2_floor(num_samples) - 1;
	int		avg_offset	= 1 << (avg_shift - 1);

	int		precShift	= max(bit_depth - 12, 0);
	int		sum_ref2	= 0;
	int		sum_prod	= 0;

	if (avail & AVAIL_A1) {
		for (int i = 0; i < h; i += 2) {
			auto	v	= ref[clamp(R.y + i, 0, height - 1)][clamp(R.x - 1, 0, width - 1)];
			sum_ref		+= v;
			sum_ref2	+= square(v)		>> precShift;
			sum_prod	+= (v * cur[i][-1])	>> precShift;
		}
	}
	if (avail & AVAIL_B1) {
		for (int i = 0; i < w; i += 2) {
			auto	v	= ref[clamp(R.y - 1, 0, height - 1)][clamp(R.x + i, 0, width - 1)];
			sum_ref		+= v;
			sum_ref2	+= square(v)		>> precShift;
			sum_prod	+= (v * cur[-1][i])	>> precShift;
		}
	}

	int		denomDiv	= ((sum_ref2 + (sum_ref2 >> 7)) << avg_shift) - (square(sum_ref) >> precShift);
	int		numerDiv	= clamp(((sum_prod + (sum_ref2 >> 7)) << avg_shift) - ((sum_ref * sum_cur) >> precShift), 0, 2 * denomDiv);
	int		shiftDenom	= max((int)log2_floor(abs(denomDiv)) - 5, 0);
	int		shiftNumer	= max(shiftDenom - 12, 0);

	int		weight		= ((numerDiv >> shiftNumer) * divCoeff[denomDiv >> shiftDenom]) >> (shiftDenom - shiftNumer + 10);

	return {((sum_cur - ((weight * sum_ref) >> 5)) + avg_offset) >> avg_shift, weight};
}

template<typename T> int calculate_illum_weight_chroma(const block<T, 2> ref, int16x2 R, int w, int h, int sum_cur, int bit_depth, AVAIL avail) {
	if (!(avail & (AVAIL_A1 | AVAIL_B1)))
		return 0;

	int		sum_ref		= 0;
	int		width		= ref.template size<1>();
	int		height		= ref.template size<2>();

	int		num_samples	= (avail & AVAIL_A1 ? h : 0) + (avail & AVAIL_B1 ? w : 0);
	int		avg_shift	= log2_floor(num_samples) - 1;
	int		avg_offset	= 1 << (avg_shift - 1);

	if (avail & AVAIL_A1) {
		for (int i = 0; i < h; i += 2)
			sum_ref += ref[clamp(R.y + i, 0, height - 1)][clamp(R.x - 1, 0, width - 1)];
	}
	if (avail & AVAIL_B1) {
		for (int i = 0; i < w; i += 2)
			sum_ref += ref[clamp(R.y - 1, 0, height - 1)][clamp(R.x + i, 0, width - 1)];
	}

	return (sum_cur - sum_ref + avg_offset) >> avg_shift;
}

void apply_illum(block<int16, 2> dst, int ic_offset, int ic_weight, int bit_depth) {
	int	shift	= max(14 - bit_depth, 2);
	int	offset	= 1 << (shift - 1);
	int	maxval	= bits(bit_depth);

	for (auto y : dst) {
		for (auto &o : y) {
			auto	v = clamp((o + offset) >> shift, 0, maxval);
			o = clamp(((v * ic_weight) >> 5) + ic_offset, 0, maxval) << shift;
		}
	}
}

void apply_illum(image* img, const image *ref, int xP, int yP, int w, int h, CHROMA chroma, int32x3 sum_cur, MotionVector mv, AVAIL avail, int16 predSamples[3][MAX_CU_SIZE* MAX_CU_SIZE]) {
	auto	R			= offset_pos(int16x2{xP, yP}, mv);
	int		bit_depth	= img->planes[0].bit_depth;

	auto	weights		= bit_depth <= 8
		? calculate_illum_weight_luma(img->get_plane_ptr<uint8 >(0).slice(xP, yP), ref->get_plane_block<uint8 >(0), R, w, h, sum_cur.x, bit_depth, avail)
		: calculate_illum_weight_luma(img->get_plane_ptr<uint16>(0).slice(xP, yP), ref->get_plane_block<uint16>(0), R, w, h, sum_cur.x, bit_depth, avail);

	apply_illum(make_block(predSamples[0], w, h), weights.x, weights.y, bit_depth);

	if (chroma != CHROMA_MONO) {
		const int	shiftw		= get_shift_W(chroma);
		const int	shifth		= get_shift_H(chroma);
		xP	>>= shiftw;
		yP	>>= shifth;
		w	>>= shiftw;
		h	>>= shifth;

		R			= int16x2{xP, yP} + ((mv + 4) >> 3);
		bit_depth	= img->planes[1].bit_depth;

		auto	offset1	= bit_depth <= 8
			? calculate_illum_weight_chroma(ref->get_plane_block<uint8 >(1), R, w, h, sum_cur.y, bit_depth, avail)
			: calculate_illum_weight_chroma(ref->get_plane_block<uint16>(1), R, w, h, sum_cur.y, bit_depth, avail);
		
		apply_illum(make_block(predSamples[1], w, h), offset1, 32, bit_depth);

		auto	offset2 = bit_depth <= 8
			? calculate_illum_weight_chroma(ref->get_plane_block<uint8 >(2), R, w, h, sum_cur.z, bit_depth, avail)
			: calculate_illum_weight_chroma(ref->get_plane_block<uint16>(2), R, w, h, sum_cur.z, bit_depth, avail);

		apply_illum(make_block(predSamples[2], w, h), offset2, 32, bit_depth);
	}
}


//-----------------------------------------------------------------------------
// bilinear
//-----------------------------------------------------------------------------

//I.8.5.3.3.3 Bilinear sample interpolation and residual prediction process
void residual_prediction_samples(image* img, const slice_segment_header* shdr, int xP, int yP, int w, int h, const Disparity& dv, const PB_info &vi, bool X, int iv_shift, int16 predSamples[3][MAX_CU_SIZE* MAX_CU_SIZE]) {
	bool	Y			= !X;
	int		poc			= img->picture_order_cnt;
	auto	rpref_idx	= shdr->extension_3d.rpref_idx;

	int16x2			P		= {xP, yP};
	auto&			refX	= shdr->ref(X, vi.refIdx[X]);
	int				refidxY	= vi.refIdx[Y];
	MotionVector	mvX		= vi.mv[X];
	MotionVector	mvT, mvR;

	const image		*imX	= refX.img;
	const image		*rp		= nullptr, *rpref = nullptr;

	if (int diff_X = poc - refX.poc) {
		auto& refR	= shdr->ref(X, rpref_idx[X]);

		mvT			= dv.raw;
		scale_mv(mvX, diff_X, poc - refR.poc);
		mvR			= mvX + mvT;
		imX			= refR.img;

		int	view	= dv.ref_view;
		if (shdr->extension_3d.view_avail(X, view)) {
			auto	shR		= imX->get_SliceHeader(clamp_to(imX, offset_pos(P, mvR)));
			if (auto i = shR->find_ref_any(refR.poc, view)) {
				rpref	= i->img;
				rp		= shdr->find_ref_any(poc, view)->img;
			}
		}

	} else if (int diff_Y = refidxY >= 0 ? poc - shdr->ref(Y, refidxY).poc : 0) {
		auto& refR	= shdr->ref(Y, rpref_idx[Y]);

		mvT			= vi.mv[Y];
		scale_mv(mvT, diff_Y, poc - refR.poc);
		mvR			= mvX + mvT;

		int	view	= refX.view_idx;
		if (shdr->extension_3d.view_avail(Y, view)) {
			auto	shR		= refR.img->get_SliceHeader(clamp_to(refR.img, offset_pos(P, mvR)));
			if (auto i = shR->find_ref_any(refR.poc, view)) {
				rpref	= i->img;
				rp		= refR.img;
			}
		}

	} else {
		//I.8.5.3.3.3.5 Derivation process for a motion vector from a reference block for residual prediction
		bool	W		= X && refidxY < 0;
		auto	R		= clamp_to(img, offset_pos(P + (int16x2{w, h} >> 1), vi.mv[W])) & ~7;
		
		auto&	refW	= shdr->ref(W, vi.refIdx[W]);
		auto	shW		= refW.img->get_SliceHeader(R);
		int		view	= refX.view_idx;

		if (refW.img->get_pred_mode(R) != MODE_INTRA) {
			auto	&pbW	= refW.img->pb_info.get(R);
			int		Y		= pbW.refIdx[0] >= 0 && rpref_idx[0] >= 0 && shdr->extension_3d.view_avail(0, view) && refW.poc != shW->ref(0, pbW.refIdx[0]).poc ? 0
							: pbW.refIdx[1] >= 0 && rpref_idx[1] >= 0 && shdr->extension_3d.view_avail(1, view) && refW.poc != shW->ref(1, pbW.refIdx[1]).poc ? 1
							: -1;

			if (Y >= 0) {
				auto&	refY	= shdr->ref(Y, rpref_idx[Y]);

				if (auto i = shW->find_ref(Y, refY.poc, view)) {
					rpref	= i->img;
					rp		= refY.img;
					mvT		= pbW.mv[Y];
					scale_mv(mvT, refW.poc - shW->ref(Y, pbW.refIdx[Y]).poc, poc - refY.poc);
					mvR		= mvX + mvT;
				}
			}

		}

		if (!rpref && rpref_idx[W] >= 0 && shdr->extension_3d.view_avail(W, view)) {
			rpref	= shW->ref(W, rpref_idx[W]).img;
			rp		= shdr->ref(W, rpref_idx[W]).img;
			mvT		= zero;
			mvR		= mvX;
		}
	}

	auto		posX			= offset_pos3(P, mvX);
	auto		posT			= offset_pos3(P, mvT);
	auto		posR			= offset_pos3(P, mvR);

	auto		out				= make_block(predSamples[0], w, h);
	const int	bit_depth_luma	= img->planes[0].bit_depth;
	if (bit_depth_luma <= 8)
		mc<2,3>(posX, out, imX->get_plane_block<const uint8 >(0), bit_depth_luma);
	else
		mc<2,3>(posX, out, imX->get_plane_block<const uint16>(0), bit_depth_luma);

	if (rpref) {
		int16		temp[2][MAX_CU_SIZE* MAX_CU_SIZE];
		auto		out_rp		= make_block(temp[0], w, h);
		auto		out_rpref	= make_block(temp[1], w, h);
		if (bit_depth_luma <= 8) {
			mc<2,3>(posT, out_rp,		rp->get_plane_block<const uint8 >(0),		bit_depth_luma);
			mc<2,3>(posR, out_rpref,	rpref->get_plane_block<const uint8 >(0),	bit_depth_luma);
		} else {
			mc<2,3>(posT, out_rp,		rp->get_plane_block<const uint16>(0),		bit_depth_luma);
			mc<2,3>(posR, out_rpref,	rpref->get_plane_block<const uint16>(0),	bit_depth_luma);
		}

		for (int i = 0, n = w * h; i < n; i++)
			predSamples[0][i] += int16(temp[0][i] - temp[1][i]) >> iv_shift;
	}

	if (img->chroma_format != CHROMA_MONO) {
		const int	bit_depth_chroma	= img->planes[1].bit_depth;
		const int	shiftw				= get_shift_W(img->chroma_format);
		const int	shifth				= get_shift_H(img->chroma_format);

		w		>>= shiftw;
		h		>>= shifth;
		posX.x	>>= shiftw;
		posX.y	>>= shifth;

		auto		out1				= make_block(predSamples[1], w, h);
		auto		out2				= make_block(predSamples[2], w, h);
		if (bit_depth_chroma <= 8) {
			mc<2,3>(posX, out1, imX->get_plane_block<const uint8>(1), bit_depth_chroma);
			mc<2,3>(posX, out2, imX->get_plane_block<const uint8>(2), bit_depth_chroma);
		} else {
			mc<2,3>(posX, out1, imX->get_plane_block<const uint16>(1), bit_depth_chroma);
			mc<2,3>(posX, out2, imX->get_plane_block<const uint16>(2), bit_depth_chroma);
		}

		if (rpref && w > 4) {
			int16		temp[2][2][MAX_CU_SIZE* MAX_CU_SIZE];
			auto		out_rp1		= make_block(temp[0][0], w, h);
			auto		out_rpref1	= make_block(temp[0][1], w, h);
			auto		out_rp2		= make_block(temp[1][0], w, h);
			auto		out_rpref2	= make_block(temp[1][1], w, h);

			posT.x >>= shiftw;
			posT.y >>= shifth;
			posR.x >>= shiftw;
			posR.y >>= shifth;

			if (bit_depth_chroma <= 8) {
				mc<2,3>(posT, out_rp1,		rp->get_plane_block<const uint8 >(1),		bit_depth_chroma);
				mc<2,3>(posR, out_rpref1,	rpref->get_plane_block<const uint8 >(1),	bit_depth_chroma);
				mc<2,3>(posT, out_rp2,		rp->get_plane_block<const uint8 >(2),		bit_depth_chroma);
				mc<2,3>(posR, out_rpref2,	rpref->get_plane_block<const uint8 >(2),	bit_depth_chroma);
			} else {
				mc<2,3>(posT, out_rp1,		rp->get_plane_block<const uint16>(1),		bit_depth_chroma);
				mc<2,3>(posR, out_rpref1,	rpref->get_plane_block<const uint16>(1),	bit_depth_chroma);
				mc<2,3>(posT, out_rp2,		rp->get_plane_block<const uint16>(2),		bit_depth_chroma);
				mc<2,3>(posR, out_rpref2,	rpref->get_plane_block<const uint16>(2),	bit_depth_chroma);
			}

			for (int i = 0, n = w * h; i < n; i++) {
				predSamples[1][i] += int16(temp[0][0][i] - temp[0][1][i]) >> iv_shift;
				predSamples[2][i] += int16(temp[1][0][i] - temp[1][1][i]) >> iv_shift;
			}
		}
	}
}

#endif //HEVC_3D

// 8.5.3.2.8
// (refIdx is -1 for merge mode)
int derive_collocated_motion_vectors(const slice_segment_header* shdr, const image *img, const image *coImg, int16x2 coP, bool L, int refIdx, MotionVector& out) {
	if (coImg->get_pred_mode(coP) == MODE_INTRA)
		return -1;

	// get the collocated MV
	auto&	coPb		= coImg->get_mv_info(coP);
	bool	coL			= coPb.refIdx[0] < 0 || (coPb.refIdx[1] >= 0 && (shdr->has_future_refs ? !shdr->collocated.list : L));
	auto&	coRef		= coImg->get_SliceHeader(coP)->ref(coL, coPb.refIdx[coL]);
	bool	coLongterm	= coRef.longterm;

	bool	merge		= refIdx < 0;
	if (merge)
		refIdx = 0;
	
	merge = merge && img->layer_id;

	if (shdr->ref(L, refIdx).longterm != coLongterm) {
		bool	found = false;
		if (merge) {
			for (auto &i : shdr->get_ref_pics(L)) {
				if (found = i.longterm == coLongterm)
					break;
				++refIdx;
			}
		}
		if (!found)
			return -1;
	}

	out = coPb.mv[coL];

	if (!coLongterm) {
		scale_mv(out, coImg->picture_order_cnt - coRef.poc, img->picture_order_cnt - shdr->ref(L, refIdx).poc);

	#ifdef HEVC_3D
	} else if (merge && shdr->extension_3d.iv.mv_scal_enabled) {
		auto	vpsml		= img->pps->sps->vps->extension_multilayer.exists_ptr();
		scale_mv(out,
			vpsml->view_ids[coImg->view_idx]	- vpsml->view_ids[coRef.view_idx],
			vpsml->view_ids[img->view_idx]		- vpsml->view_ids[shdr->ref(L, refIdx).view_idx]
		);
	#endif
	}
	return refIdx;
}


// 8.5.3.1.7
int derive_temporal_luma_vector_prediction(const slice_segment_header* shdr, const image *img, int xP, int yP, int w, int h, bool L, int refIdx, MotionVector& out) {
	int16x2 coP		= {xP + w, yP + h};
	auto	coImg	= shdr->ref(shdr->collocated).img;

	if (check_image_pos(coImg, coP)) {
		// If neighboring pixel at bottom-right corner is in the same CTB-row and inside the image, use this (reduced down to 16 pixels resolution) as collocated MV position.
		if ((yP >> img->ctu_info.log2unitSize) == (coP.y >> img->ctu_info.log2unitSize)) {
			auto ret = derive_collocated_motion_vectors(shdr, img, coImg, coP & -16, L, refIdx, out);
			if (ret >= 0)
				return ret;
		}
	}
	// otherwise use centre of PB
	return derive_collocated_motion_vectors(shdr, img, coImg, int16x2{xP + (w >> 1), yP + (h >> 1)} & -16, L, refIdx, out);
}


// 8.5.3.1.1
void get_merge_candidate_list(const slice_segment_header* shdr, image* img, int xP, int yP, int w, int h, AVAIL avail, PB_info* out, int max_index ONLY_3D(const Disparity& dv)) {
	if (xP + w > img->get_width())
		avail -= AVAIL_B1;
	if (yP + h > img->get_height())
		avail -= AVAIL_A1;

	// --- spatial merge candidates
	auto	pps				= img->pps.get();
	auto	sps				= pps->sps.get();
	const int merge_level	= pps->log2_parallel_merge_level;

	auto	P				= int16x2{xP, yP};
	int		num_candidates	= 0;
	int		num_spatial		= 0;
	uint8	spatial_indices[4];


	int		idxA1	= -1;
	int16x2 A1		= {xP - 1, yP + h - 1};
	PB_info	a1		= (avail & AVAIL_A1) && check_merge(P, A1, merge_level) && img->get_pred_mode(A1) != MODE_INTRA ? img->get_mv_info(A1) : PB_info();

	int		idxB1	= -1;
	int16x2 B1		= {xP + w - 1, yP - 1};
	PB_info	b1		= (avail & AVAIL_B1) && check_merge(P, B1, merge_level) && img->get_pred_mode(B1) != MODE_INTRA ? img->get_mv_info(B1) : PB_info();


#ifdef HEVC_3D
	// --- T ---
	if (avail & AVAIL_T) {
		//I.8.5.3.2.9:
		int		log2min	= shdr->extension_3d.iv.log2_sub_pb_size;
		int		mask	= bits(log2min);
		bool	sub		= !((w | h) & mask);
		auto&	t		= out[num_candidates];
		if (sub
			? derive_texture_motionvectors(shdr, xP + (w >> 1) & ~mask, yP + (h >> 1) & ~mask, mask + 1, mask + 1, t)
			: derive_texture_motionvectors(shdr, xP, yP, w, h, t)
		) {
			if (sub)
				t.flags |= PB_info::SUB | ((log2min - 3) << 8);
			if (++num_candidates > max_index)
				return;

			if (a1.valid() && a1 == t)
				idxA1 = num_candidates - 1;
			if (b1.valid() && b1 == t)
				idxB1 = num_candidates - 1;
		}
	}

	// --- IV ---
	int		idxIV	= -1;
	if (avail & AVAIL_IV) {
		//I.8.5.3.2.8: Derivation process for inter-view predicted merging candidates
		int		log2min	= shdr->extension_3d.iv.log2_sub_pb_size;
		//int		log2min	= sps->extension_3d.iv[0].log2_sub_pb_size;
		int		mask	= bits(log2min);
		bool	sub		= (avail & AVAIL_IV_SUB) && !((w | h) & mask);
		if ((sub
			? derive_interview_motionvectors(shdr, dv, xP + (w >> 1) & ~mask, yP + (h >> 1) & ~mask, mask + 1, mask + 1, false, out[num_candidates])
			: derive_interview_motionvectors(shdr, dv, xP, yP, w, h, false, out[num_candidates])
		) && (num_candidates == 0 || out[num_candidates] != out[0])) {
			idxIV = num_candidates++;
			if (sub)
				out[idxIV].flags |= PB_info::SUB | ((log2min - 3) << 8);
			if (num_candidates > max_index)
				return;
			if (!shdr->extension_3d.depth_layer) {
				if (a1.valid() && a1 == out[idxIV])
					idxA1 = idxIV;
				if (b1.valid() && b1 == out[idxIV])
					idxB1 = idxIV;
			}
		}
	}
#endif

	// --- A1 ---
	if (a1.valid()) {
		if (idxA1 < 0) {
			out[idxA1 = num_candidates++]	= a1;
			out[idxA1].flags &= !(avail & (AVAIL_RESPRED | AVAIL_ILLU)) * PB_info::VSP;
			if (num_candidates > max_index)
				return;
		}
		if (a1.flags & PB_info::VSP)
			avail -= AVAIL_VSP;
		spatial_indices[num_spatial++] = idxA1;
	}

	// --- B1 ---
	if (b1.valid()) {
		if (idxB1 < 0) {
			if (idxA1 >= 0 && a1 == b1) {
				idxB1 = idxA1;
			} else {
				out[idxB1 = num_candidates++]	= b1.cleared_flags();
				if (num_candidates > max_index)
					return;
			}
		}
		if (idxB1 != idxA1)
			spatial_indices[num_spatial++] = idxB1;
	}

#ifdef HEVC_3D
	// --- VSP ---
	if ((avail & AVAIL_VSP) && derive_viewsynthesis_merging_candidates(shdr, dv, xP, yP, w, h, out[num_candidates])) {
		if (++num_candidates > max_index)
			return;
	}
#endif

	// --- B0 ---
	int16x2 B0		= {xP + w, yP - 1};
	if ((avail & AVAIL_B0) && check_merge(P, B0, merge_level) && img->get_pred_mode(B0) != MODE_INTRA) {
		auto& b0 = img->get_mv_info(B0);
		if (idxB1 < 0 || out[idxB1] != b0) {
			out[num_candidates++] = b0.cleared_flags();
			if (num_candidates > max_index)
				return;
			spatial_indices[num_spatial++] = num_candidates - 1;
		}
	}

#ifdef HEVC_3D
	// --- DI ---
	PB_info	di;
	bool	has_di = (avail & AVAIL_DI) && derive_disparity_merging_candidates(shdr, img->picture_order_cnt, dv, di);
	if (has_di && (idxA1 < 0 || out[idxA1] != di) && (idxB1 < 0 || out[idxB1] != di)) {
		out[num_candidates++] = di;
		if (num_candidates > max_index)
			return;
	}
#endif

	// --- A0 ---
	int16x2 A0		= {xP - 1, yP + h};
	if ((avail & AVAIL_A0) && check_merge(P, A0, merge_level) && img->get_pred_mode(A0) != MODE_INTRA) {
		auto& a0 = img->get_mv_info(A0);
		if (idxA1 < 0 || out[idxA1] != a0) {
			out[num_candidates++] = a0.cleared_flags();
			if (num_candidates > max_index)
				return;
			spatial_indices[num_spatial++] = num_candidates - 1;
		}
	}

	// --- B2 ---
	if (num_spatial < 4) {
		int16x2 B2		= {xP - 1, yP - 1};
		if ((avail & AVAIL_B2) && check_merge(P, B2, merge_level) && img->get_pred_mode(B2) != MODE_INTRA) {
			auto& b2 = img->get_mv_info(B2);
			if ((idxB1 < 0 || out[idxB1] != b2) && (idxA1 < 0 || out[idxA1] != b2)) {
				out[num_candidates++] = b2.cleared_flags();
				if (num_candidates > max_index)
					return;
				spatial_indices[num_spatial++] = num_candidates - 1;
			}
		}
	}

#ifdef HEVC_3D
	// --- IVshift or DIshift ---
	if ((avail & AVAIL_IV) && (avail & AVAIL_IV_SHIFT) && derive_interview_motionvectors(shdr, dv, xP, yP, w, h, true, out[num_candidates])) {
		if (idxIV < 0 || out[num_candidates] != out[idxIV])
			++num_candidates;

	} else if (has_di) {
		auto	&p	= out[num_candidates++];
		p			= di;
		p.mv[0].x += 4;
		p.mv[1].x += 4;
	}

	if (num_candidates > max_index)
		return;
#endif

	bool	btype	= shdr->type == SLICE_TYPE_B;

	// --- collocated merge candidate
	if (shdr->temporal_mvp_enabled) {
		MotionVector	mvCo[2];
		int				refIdx0	= derive_temporal_luma_vector_prediction(shdr, img, xP, yP, w, h, 0, -1, mvCo[0]);
		int				refIdx1	= btype ? derive_temporal_luma_vector_prediction(shdr, img, xP, yP, w, h, 1, -1, mvCo[1]) : -1;
		if (refIdx0 >= 0 || refIdx1 >= 0) {
			auto&	p	= out[num_candidates++];
			p.flags		= 0;
			p.set(0, refIdx0, mvCo[0]);
			p.set(1, refIdx1, mvCo[1]);
			if (num_candidates > max_index)
				return;
			spatial_indices[num_spatial++] = num_candidates - 1;
		}
	}

	// 8.5.3.1.3
	// --- bipredictive merge candidates ---
	if (btype && num_spatial < 5) {
		static const int table_8_19[2][12] = {
			{ 0,1,0,2,1,2,0,3,1,3,2,3 },
			{ 1,0,2,0,2,1,3,0,3,1,3,2 }
		};
		for (int i = 0, n = num_spatial * (num_spatial - 1); i < n; i++) {
			auto&	cand0		= out[spatial_indices[table_8_19[0][i]]];
			auto&	cand1		= out[spatial_indices[table_8_19[1][i]]];

			if (cand0.refIdx[0] >= 0 && cand1.refIdx[1] >= 0 && (shdr->ref(0, cand0.refIdx[0]).poc != shdr->ref(1, cand1.refIdx[1]).poc || any(cand0.mv[0] != cand1.mv[1]))) {
				auto&	p	= out[num_candidates++];
				p.flags		= 0;
				p.set(0, cand0.refIdx[0], cand0.mv[0]);
				p.set(1, cand1.refIdx[1], cand1.mv[1]);
				if (num_candidates > max_index)
					return;
			}
		}
	}

	// 8.5.3.1.4
	// --- zero-vector merge candidates ---
	int		num_refs	= btype ? min(shdr->ref_list[0].size(), shdr->ref_list[1].size()) : shdr->ref_list[0].size();
	for (int idx = 0; num_candidates <= max_index; num_candidates++, idx++) {
		auto&	p	= out[num_candidates];
		p.flags		= 0;
		p.set(0, idx < num_refs ? idx : 0, zero);
		p.set(1, btype ? p.refIdx[0] : -1, zero);
	}
}

// 8.5.3.1.6: derive two spatial vector predictors
int derive_spatial_luma_vector_prediction(const slice_segment_header* shdr, const image* img, int xP, int yP, int w, int h, bool X, int refIdxLX, AVAIL avail, MotionVector out_mv[2]) {
	const int	poc			= shdr->ref(X, refIdxLX).poc;
	const bool	longterm	= shdr->ref(X, refIdxLX).longterm;
	const bool	Y			= !X;
	int			num			= 0;

	// --- A ---

	int16x2	A[2] = {
		{xP - 1, yP + h},
		{xP - 1, yP + h - 1}
	};

	bool	availableA[2] = {
		(avail & AVAIL_A0) && img->get_pred_mode(A[0]) != MODE_INTRA,
		(avail & AVAIL_A1) && img->get_pred_mode(A[1]) != MODE_INTRA
	};

	for (int k = 0; k < 2; k++) {
		if (availableA[k]) {
			auto&	vi	= img->get_mv_info(A[k]);
			int		ref	= vi.refIdx[X] >= 0 && shdr->ref(X, vi.refIdx[X]).poc == poc ? X
						: vi.refIdx[Y] >= 0 && shdr->ref(Y, vi.refIdx[Y]).poc == poc ? Y
						: -1;

			if (ref >= 0) {
				// the predictor is available and references the same POC
				out_mv[0]	= vi.mv[ref];
				num			= 1;
				break;
			}
		}
	}

	// 7. If there is no predictor referencing the same POC, we take any other reference as long as it is the same type of reference (long-term / short-term)
	if (num == 0) {
		for (int k = 0; k < 2; k++) {
			if (availableA[k]) {
				auto&	vi	= img->get_mv_info(A[k]);
				int		ref	= vi.refIdx[X] >= 0 && shdr->ref(X, vi.refIdx[X]).longterm == longterm ? X
							: vi.refIdx[Y] >= 0 && shdr->ref(Y, vi.refIdx[Y]).longterm == longterm ? Y
							: -1;

				if (ref >= 0) {
					out_mv[0]	= vi.mv[ref];
					num			= 1;
					if (!longterm)
						scale_mv(out_mv[0], img->picture_order_cnt - shdr->ref(ref, vi.refIdx[ref]).poc, img->picture_order_cnt - poc);
					break;
				}
			}
		}
	}

	// --- B ---

	int16x2 B[3] = {
		{xP + w,		yP - 1},
		{xP + w - 1,	yP - 1},
		{xP - 1,		yP - 1},
	};

	bool	availableB[3] = {
		(avail & AVAIL_B0) && img->get_pred_mode(B[0]) != MODE_INTRA,
		(avail & AVAIL_B1) && img->get_pred_mode(B[1]) != MODE_INTRA,
		(avail & AVAIL_B2) && img->get_pred_mode(B[2]) != MODE_INTRA
	};

	for (int k = 0; k < 3; k++) {
		if (availableB[k]) {
			auto&	vi	= img->get_mv_info(B[k]);
			int		ref	= vi.refIdx[X] >= 0 && shdr->ref(X, vi.refIdx[X]).poc == poc ? X
						: vi.refIdx[Y] >= 0 && shdr->ref(Y, vi.refIdx[Y]).poc == poc ? Y
						: -1;

			if (ref >= 0) {
				out_mv[num++]	= vi.mv[ref];
				break;
			}
		}
	}

	if (!availableA[0] && !availableA[1]) {
		// If no A predictor, add a scaled B predictor
		for (int k = 0; k < 3; k++) {
			if (availableB[k]) {
				auto&	vi	= img->get_mv_info(B[k]);
				int		ref = vi.refIdx[X] >= 0 && shdr->ref(X, vi.refIdx[X]).longterm == longterm ? X
							: vi.refIdx[Y] >= 0 && shdr->ref(Y, vi.refIdx[Y]).longterm == longterm ? Y
							: -1;

				if (ref >= 0) {
					out_mv[num]	= vi.mv[ref];
					if (!longterm)
						scale_mv(out_mv[num], img->picture_order_cnt - shdr->ref(ref, vi.refIdx[ref]).poc, img->picture_order_cnt - poc);
					++num;
					break;
				}
			}
		}
	}

	if (num == 2 && all(out_mv[0] == out_mv[1]))
		--num;

	return num;
}

PB_info resolve_inter_prediction(const slice_segment_header* shdr, image* img, const PBMotionCoding& motion, int xP, int yP, int w, int h, PB_info *merge_candidates, bool shared_merge, AVAIL avail ONLY_3D(const Disparity& dv)) {
	if (motion.is_merge()) {
		if (!shared_merge)
			get_merge_candidate_list(shdr, img, xP, yP, w, h, avail, merge_candidates, motion.merge_index ONLY_3D(dv));

		auto	&vi = merge_candidates[motion.merge_index];
		if ((avail & AVAIL_NO_BIPRED) && vi.refIdx[0] >= 0)
			vi.refIdx[1] = -1;

		return vi;

	} else {
		PB_info	vi;
		vi.flags	= 0;

		for (int i = 0; i < 2; i++) {
			int		refIdx	= (avail & AVAIL_NO_BIPRED) && vi.refIdx[0] >= 0 ? -1 : motion.refIdx[i];
			vi.refIdx[i]	= refIdx;

			if (refIdx >= 0) {
				MotionVector	mv[2];
				int				n		= derive_spatial_luma_vector_prediction(shdr, img, xP, yP, w, h, i, refIdx, avail, mv);
				int				mvp		= motion.mvp[i];

				if (n <= mvp && shdr->temporal_mvp_enabled) {
					// if we only have one spatial vector or both spatial vectors are the same, derive a temporal predictor
					if (derive_temporal_luma_vector_prediction(shdr, img, xP,yP, w,h, i, refIdx, mv[n]) >= 0)
						++n;
				}

				vi.mv[i]	= motion.mvd[i];
				if (mvp < n)
					vi.mv[i] += mv[mvp];
			}
		}

		return vi;
	}
}

#if 0

template<int N, typename S> void put_pred2(block<uint8, 2> dst, iblockptr<S, 2> src, int shift, int max) {
	const int rnd	= (1 << shift) >> 1;
	for (auto y : dst) {
		auto	v = to_sat<uint8>((load<N>(src.begin()) + rnd) >> shift);
		store(v, y.begin());
		++src;
	}
}

template<int N, typename T, typename S> void put_pred2(block<T, 2> dst, iblockptr<S, 2> src, int shift, int max) {
	const int rnd	= (1 << shift) >> 1;
	for (auto y : dst) {
		auto	v = to<T>(clamp((load<N>(src.begin()) + rnd) >> shift, zero, max));
		store(v, y.begin());
		++src;
	}
}
template<typename T, typename S> void put_pred1(block<T, 2> dst, iblockptr<S, 2> src, int shift, int max) {
	switch (dst.template size<1>()) {
		case 2:		put_pred2<2 >(dst, src, shift, max); break;
		case 4:		put_pred2<4 >(dst, src, shift, max); break;
		case 8:		put_pred2<8 >(dst, src, shift, max); break;
		case 16:	put_pred2<16>(dst, src, shift, max); break;
		case 32:	put_pred2<32>(dst, src, shift, max); break;
		default:	ISO_ASSERT(0);
	}
}

template<typename T, typename S> inline void put_pred(block<T, 2> dst, iblockptr<S, 2> src, int shift, int max) {
	int		w	= dst.template size<1>();
	int		w0	= min(lowest_set(w), 32);
	if (w == w0) {
		put_pred1(dst, src, shift, max);
	} else {
		put_pred1(dst.template slice<1>(0, w0), src, shift, max);
		put_pred1(dst.template slice<1>(w0), src.slice(w0), shift, max);
	}
}

#else

template<typename T, typename S> void put_pred(block<T, 2> dst, iblockptr<S, 2> src, int shift, int max) {
	const int rnd	= (1 << shift) >> 1;
	for (auto y : dst) {
		auto	in  = src.begin();
		for (auto &o : y)
			o = clamp((*in++ + rnd) >> shift, 0, max);
		++src;
	}
}
#endif

#if 0

template<int N, typename S> void put_bipred2(block<uint8, 2> dst, iblockptr<S, 2> src1, iblockptr<S, 2> src2, int shift, int max) {
	const int rnd	= (1 << shift) >> 1;
	for (auto y : dst) {
		auto	v = to_sat<uint8>((load<N>(src1.begin()) + load<N>(src2.begin()) + rnd) >> (shift + 1));
		store(v, y.begin());
		++src1;
		++src2;
	}
}

template<int N, typename T, typename S> void put_bipred2(block<T, 2> dst, iblockptr<S, 2> src1, iblockptr<S, 2> src2, int shift, int max) {
	const int rnd	= (1 << shift) >> 1;
	for (auto y : dst) {
		auto	v = to<T>(clamp((load<N>(src1.begin()) + load<N>(src2.begin()) + rnd) >> (shift + 1), 0, max));
		store(v, y.begin());
		++src1;
		++src2;
	}
}

template<typename T, typename S> void put_bipred1(block<T, 2> dst, iblockptr<S, 2> src1, iblockptr<S, 2> src2, int shift, int max) {
	switch (dst.template size<1>()) {
		case 2:		put_bipred2<2 >(dst, src1, src2, shift, max); break;
		case 4:		put_bipred2<4 >(dst, src1, src2, shift, max); break;
		case 8:		put_bipred2<8 >(dst, src1, src2, shift, max); break;
		case 16:	put_bipred2<16>(dst, src1, src2, shift, max); break;
		case 32:	put_bipred2<32>(dst, src1, src2, shift, max); break;
		default:	ISO_ASSERT(0);
	}
}

template<typename T, typename S> inline void put_bipred(block<T, 2> dst, iblockptr<S, 2> src1, iblockptr<S, 2> src2, int shift, int max) {
	int		w	= dst.template size<1>();
	int		w0	= min(lowest_set(w), 32);
	if (w == w0) {
		put_bipred1(dst, src1, src2, shift, max);
	} else {
		put_bipred1(dst.template slice<1>(0, w0), src1, src2, shift, max);
		put_bipred1(dst.template slice<1>(w0), src1.slice(w0), src2.slice(w0), shift, max);
	}
}

#else

template<typename T, typename S> void put_bipred(block<T, 2> dst, iblockptr<S, 2> src1, iblockptr<S, 2> src2, int shift, int max) {
	const int rnd	= 1 << shift;
	for (auto y : dst) {
		auto in1 = src1.begin();
		auto in2 = src2.begin();
		for (auto &o : y)
			o = clamp((*in1++ + *in2++ + rnd) >> (shift + 1), 0, max);
		++src1;
		++src2;
	}
}
#endif

template<typename T, typename S> void put_weighted_pred(block<T, 2> dst, iblockptr<S, 2> src, int w, int o, int shift, int max) {
	const int rnd = (o << shift) + (1 << (shift - 1));
	for (auto y : dst) {
		auto in  = src.begin();
		for (auto &o : y)
			o = clamp((*in++ * w + rnd) >> shift, 0, max);
		++src;
	}
}

template<typename T, typename S> void put_weighted_bipred(block<T, 2> dst, iblockptr<S, 2> src1, iblockptr<S, 2> src2, int w1, int o1, int w2, int o2, int shift, int max) {
	const int rnd = (o1 + o2 + 1) << shift;
	for (auto y : dst) {
		auto in1 = src1.begin();
		auto in2 = src2.begin();
		for (auto &o : y)
			o = clamp((*in1++ * w1 + *in2++ * w2 + rnd) >> (shift + 1), 0, max);
		++src1;
		++src2;
	}
}

bool decode_inter_prediction2(const slice_segment_header* shdr, image* img, int xP, int yP, int w, int h, PB_info vi, AVAIL avail ONLY_3D(const Disparity& dv)) {
	int		enabled = vi.dir();
	if (shdr->type == SLICE_TYPE_P ? enabled != 1 : enabled == 0)
		return false;

	auto	pps			= shdr->pps.get();
	auto	sps			= pps->sps.get();
	bool	weighted	= !(avail & (AVAIL_ILLU | AVAIL_RESPRED)) && (shdr->type == SLICE_TYPE_P ? pps->weighted_pred : pps->weighted_bipred);

	// clip mvs
	int		ctu_size	= 1 << sps->log2_ctu();
	auto	C			= int16x2{xP, yP} & -ctu_size;
	auto	vmin		= (-C - ctu_size - 8 + 1) << 2;
	auto	vmax		= (MotionVector{sps->pic_width_luma, sps->pic_height_luma} - C + 8 - 1) << 2;
	if (enabled & 1)
		vi.mv[0]	= clamp(vi.mv[0], vmin, vmax);
	if (enabled & 2)
		vi.mv[1]	= clamp(vi.mv[1], vmin, vmax);

	// Some encoders use bi-prediction with two similar MVs; identify this case and use only one MV
	if (enabled == 3
		&& !weighted
		&& !(avail & AVAIL_RESPRED)
		&& all(vi.mv[0] == vi.mv[1])
		&& shdr->ref(0, vi.refIdx[0]).img == shdr->ref(1, vi.refIdx[1]).img
	)
		enabled = 1;

	int16	predSamples[2][3][MAX_CU_SIZE* MAX_CU_SIZE];

#ifdef HEVC_3D
	if (avail & AVAIL_RESPRED) {
		if (enabled & 1)
			residual_prediction_samples(img, shdr, xP, yP, w, h, dv, vi, false, avail & AVAIL_RESPRED2 ? 1 : 0, predSamples[0]);
		if (enabled & 2)
			residual_prediction_samples(img, shdr, xP, yP, w, h, dv, vi, true,  avail & AVAIL_RESPRED2 ? 1 : 0, predSamples[1]);

	} else if (shdr->extension_3d.depth_layer) {
		if (
			((enabled & 1) && !generate_inter_prediction_depthsamples(img, shdr->ref(0, vi.refIdx[0]).img, xP, yP, w, h, vi.mv[0], predSamples[0][0]))
		||	((enabled & 2) && !generate_inter_prediction_depthsamples(img, shdr->ref(1, vi.refIdx[1]).img, xP, yP, w, h, vi.mv[1], predSamples[1][0]))
		)
			return false;

	} else
#endif
	if (
		((enabled & 1) && !generate_inter_prediction_samples(img, shdr->ref(0, vi.refIdx[0]).img, xP, yP, w, h, vi.mv[0], predSamples[0]))
	||	((enabled & 2) && !generate_inter_prediction_samples(img, shdr->ref(1, vi.refIdx[1]).img, xP, yP, w, h, vi.mv[1], predSamples[1]))
	)
		return false;

#ifdef HEVC_3D
	//--- illu ---
	if (avail & AVAIL_ILLU) {
		auto	chroma	= w > 8 ? img->chroma_format : CHROMA_MONO;
		int32x3	sum_cur	= edge_sums(img, xP, yP, w, h, chroma, avail);
		if ((enabled & 1) && shdr->ref(0, vi.refIdx[0]).view_idx != img->view_idx)
			apply_illum(img, shdr->ref(0, vi.refIdx[0]).img, xP, yP, w, h, chroma, sum_cur, vi.mv[0], avail, predSamples[0]);
		if ((enabled & 2) && shdr->ref(1, vi.refIdx[1]).view_idx != img->view_idx)
			apply_illum(img, shdr->ref(1, vi.refIdx[1]).img, xP, yP, w, h, chroma, sum_cur, vi.mv[1], avail, predSamples[1]);

	}
#endif

	const int	shiftw				= get_shift_W(img->chroma_format);
	const int	shifth				= get_shift_H(img->chroma_format);
	const int	bit_depth_luma		= img->planes[0].bit_depth;
	const int	bit_depth_chroma	= img->planes[1].bit_depth;
	const bool	mono				= img->chroma_format == CHROMA_MONO;

	const int	xPc					= xP >> shiftw;
	const int	yPc					= yP >> shifth;
	const int	wc					= w >> shiftw;
	const int	hc					= h >> shifth;

	if (weighted) {
		// weighted sample prediction  (8.5.3.2.3)
		const int	luma_shift		= shdr->luma_log2_weight_denom		+ max(14 - bit_depth_luma, 2);
		const int	chroma_shift	= shdr->chroma_log2_weight_denom	+ max(14 - bit_depth_chroma, 2);

		if (enabled == 3) {
			auto	wts0 = shdr->weights[0][vi.refIdx[0]], wts1 = shdr->weights[1][vi.refIdx[1]];

			if (bit_depth_luma <= 8)
				put_weighted_bipred(img->get_plane_block<uint8 >(0, xP, yP, w, h), make_iblockptr(predSamples[0][0], w), make_iblockptr(predSamples[1][0], w), wts0[0].weight, wts0[0].offset, wts1[0].weight, wts1[0].offset, luma_shift, 255);
			else
				put_weighted_bipred(img->get_plane_block<uint16>(0, xP, yP, w, h), make_iblockptr(predSamples[0][0], w), make_iblockptr(predSamples[1][0], w), wts0[0].weight, wts0[0].offset, wts1[0].weight, wts1[0].offset, luma_shift, bits(bit_depth_chroma));

			if (!mono) {
				if (bit_depth_chroma <= 8) {
					put_weighted_bipred(img->get_plane_block<uint8 >(1, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][1], wc), make_iblockptr(predSamples[1][1], wc), wts0[1].weight, wts0[1].offset, wts1[1].weight, wts1[1].offset, chroma_shift, 255);
					put_weighted_bipred(img->get_plane_block<uint8 >(2, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][2], wc), make_iblockptr(predSamples[1][2], wc), wts0[2].weight, wts0[2].offset, wts1[2].weight, wts1[2].offset, chroma_shift, 255);
				} else {
					put_weighted_bipred(img->get_plane_block<uint16>(1, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][1], wc), make_iblockptr(predSamples[1][1], wc), wts0[1].weight, wts0[1].offset, wts1[1].weight, wts1[1].offset, chroma_shift, bits(bit_depth_chroma));
					put_weighted_bipred(img->get_plane_block<uint16>(2, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][2], wc), make_iblockptr(predSamples[1][2], wc), wts0[2].weight, wts0[2].offset, wts1[2].weight, wts1[2].offset, chroma_shift, bits(bit_depth_chroma));
				}
			}

		} else {
			int		i		= enabled == 2;
			auto	wts		= shdr->weights[i][vi.refIdx[i]];
			auto	samples	= predSamples[i];

			if (bit_depth_luma <= 8)
				put_weighted_pred(img->get_plane_block<uint8 >(0, xP, yP, w, h), make_iblockptr(samples[0], w), wts[0].weight, wts[0].offset, luma_shift, 255);
			else
				put_weighted_pred(img->get_plane_block<uint16>(0, xP, yP, w, h), make_iblockptr(samples[0], w), wts[0].weight, wts[0].offset, luma_shift, bits(bit_depth_luma));

			if (!mono) {
				if (bit_depth_chroma <= 8) {
					put_weighted_pred(img->get_plane_block<uint8 >(1, xPc, yPc, wc, hc), make_iblockptr(samples[1], wc), wts[1].weight, wts[1].offset, chroma_shift, 255);
					put_weighted_pred(img->get_plane_block<uint8 >(2, xPc, yPc, wc, hc), make_iblockptr(samples[2], wc), wts[2].weight, wts[2].offset, chroma_shift, 255);
				} else {
					put_weighted_pred(img->get_plane_block<uint16>(1, xPc, yPc, wc, hc), make_iblockptr(samples[1], wc), wts[1].weight, wts[1].offset, chroma_shift, bits(bit_depth_chroma));
					put_weighted_pred(img->get_plane_block<uint16>(2, xPc, yPc, wc, hc), make_iblockptr(samples[2], wc), wts[2].weight, wts[2].offset, chroma_shift, bits(bit_depth_chroma));
				}
			}
		}

	} else {
		// unweighted sample prediction
		if (enabled == 3) {
			if (bit_depth_luma <= 8)
				put_bipred(img->get_plane_block<uint8 >(0, xP, yP, w, h), make_iblockptr(predSamples[0][0], w), make_iblockptr(predSamples[1][0], w), 14 - 8, 255);
			else
				put_bipred(img->get_plane_block<uint16>(0, xP, yP, w, h), make_iblockptr(predSamples[0][0], w), make_iblockptr(predSamples[1][0], w), 14 - bit_depth_luma, bits(bit_depth_luma));

			if (!mono) {
				if (bit_depth_chroma <= 8) {
					put_bipred(img->get_plane_block<uint8 >(1, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][1], wc), make_iblockptr(predSamples[1][1], wc), 14 - 8, 255);
					put_bipred(img->get_plane_block<uint8 >(2, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][2], wc), make_iblockptr(predSamples[1][2], wc), 14 - 8, 255);
				} else {
					put_bipred(img->get_plane_block<uint16>(1, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][1], wc), make_iblockptr(predSamples[1][1], wc), 14 - bit_depth_chroma, bits(bit_depth_chroma));
					put_bipred(img->get_plane_block<uint16>(2, xPc, yPc, wc, hc), make_iblockptr(predSamples[0][2], wc), make_iblockptr(predSamples[1][2], wc), 14 - bit_depth_chroma, bits(bit_depth_chroma));
				}
			}

		} else {
			auto	samples	= predSamples[enabled == 2];

			if (bit_depth_luma <= 8)
				put_pred(img->get_plane_block<uint8 >(0, xP, yP, w, h), make_iblockptr(samples[0], w), 14 - 8, 255);
			else
				put_pred(img->get_plane_block<uint16>(0, xP, yP, w, h), make_iblockptr(samples[0], w), 14 - bit_depth_luma, bits(bit_depth_luma));

			if (!mono) {
				if (bit_depth_chroma <= 8) {
					put_pred(img->get_plane_block<uint8 >(1, xPc, yPc, wc, hc), make_iblockptr(samples[1], wc), 14 - 8, 255);
					put_pred(img->get_plane_block<uint8 >(2, xPc, yPc, wc, hc), make_iblockptr(samples[2], wc), 14 - 8, 255);
				} else {
					put_pred(img->get_plane_block<uint16>(1, xPc, yPc, wc, hc), make_iblockptr(samples[1], wc), 14 - bit_depth_chroma, bits(bit_depth_chroma));
					put_pred(img->get_plane_block<uint16>(2, xPc, yPc, wc, hc), make_iblockptr(samples[2], wc), 14 - bit_depth_chroma, bits(bit_depth_chroma));
				}
			}
		}
	}
	return true;
}

bool decode_inter_prediction1(const slice_segment_header* shdr, image* img, PB_info vi, int xP, int yP, int w, int h, AVAIL avail ONLY_3D(const Disparity& dv)) {
#ifdef HEVC_3D
	if (vi.flags & vi.VSP) {
		//I.8.5.7 Derivation process for a depth or disparity sample array from a depth picture
		auto	vps3d		= shdr->pps->sps->vps->extension_3d.exists_ptr();
		auto	cp			= vps3d->get_cp(img->view_idx, dv.ref_view, shdr->extension_3d.cps);
		auto	dimg		= shdr->extension_3d.depth_images[dv.ref_view];
		auto	R			= offset_pos(int16x2{xP, yP}, dv.raw);
		bool	hsplit		= derive_viewsynthesis_hsplit(dimg, R, w, h);
		int		subw		= hsplit ? 8 : 4;
		int		subh		= hsplit ? 4 : 8;
		int		X			= vi.refIdx[0] < 0;

		for (int y = 0; y < h; y += subh) {
			for (int x = 0; x < w; x += subw) {
				vi.mv[X]	= {estimate_disparity(dimg, R + int16x2{x, y}, subw, subh, cp, vps3d->cp_precision), 0};
				if (!decode_inter_prediction2(shdr, img, xP + x, yP + y, subw, subh, vi, avail, dv))
					return false;

				img->pb_info.get(xP + x, yP + y) = img->pb_info.get(xP + x + (hsplit ? 4 : 0), yP + y + (hsplit ? 0 : 4)) = vi;
			}
		}

	} else if (vi.flags & vi.SUB) {
		int		subsize		= 1 << ((vi.flags >> 8) + 3);
		bool	iv			= vi.flags & vi.IV;
		auto	sub			= vi;

		for (int yS = yP; yS < yP + h; yS += subsize) {
			for (int xS = xP; xS < xP + w; xS += subsize) {
				bool	valid	= iv
					? derive_interview_motionvectors(shdr, dv, xS, yS, subsize, subsize, false, sub)
					: derive_texture_motionvectors(shdr, xS, yS, subsize, subsize, sub);
				if (!valid)
					sub = vi;
				if (!decode_inter_prediction2(shdr, img, xS, yS, subsize, subsize, sub, avail, dv))
					return false;

				img->set_mv_info(xS, yS, subsize, subsize, sub);
			}
		}

	} else
#endif
	{
		//I.8.5.3.3.1
		if (!decode_inter_prediction2(shdr, img, xP, yP, w, h, vi, avail ONLY_3D(dv)))
			return false;

		img->set_mv_info(xP, yP, w, h, vi);
	}

	return true;
}

bool decode_inter_prediction(const slice_segment_header* shdr, image* img, const PBMotionCoding& motion, int xP, int yP, int w, int h, PB_info *merge_candidates, bool shared_merge, AVAIL avail ONLY_3D(const Disparity& dv)) {
	PB_info	vi = resolve_inter_prediction(shdr, img, motion, xP, yP, w, h, merge_candidates, shared_merge, avail ONLY_3D(dv));
	return decode_inter_prediction1(shdr, img, vi, xP, yP, w, h, avail ONLY_3D(dv));
};

//-----------------------------------------------------------------------------
// deblocking
//-----------------------------------------------------------------------------

enum DeblockFlags : uint8 {
	DEBLOCK_NONE			= 0,
	DEBLOCK_HORZ			= 1 << 0,
	DEBLOCK_VERT			= 1 << 1,
	DEBLOCK_HORZ_STRONG		= DEBLOCK_HORZ | (1 << 2),
	DEBLOCK_VERT_STRONG		= DEBLOCK_VERT | (1 << 3),
};

constexpr DeblockFlags operator|(DeblockFlags a, DeblockFlags b)	{ return DeblockFlags((uint8)a | (uint8)b); }
constexpr DeblockFlags& operator|=(DeblockFlags &a, DeblockFlags b)	{ return a = a | b; }

// 8.7.2.4
auto edge_QPY(const image* img, int x, int y, bool vertical) {
	return (img->get_QPY(x, y) + img->get_QPY(x - vertical, y - !vertical) + 1) >> 1;
}

template<typename T> vec<T, 4> edge_load(blockptr<T, 2> ptr, int i, bool vertical) {
	return vertical ? load<4>(strided(ptr[0] + i, ptr.pitch())) : load<4>(ptr[i]);
}

template<typename T, typename V> void edge_store(V v, blockptr<T, 2> ptr, int i, bool vertical) {
	if (vertical)
		store(v, strided(ptr[0] + i, ptr.pitch()));
	else
		store(v, ptr[i]);
}

template<typename T> void deblock_luma(image* img, int x0, int y0, blockptr<T, 2> plane, int bit_depth, bool vertical, block<DeblockFlags,2> deblk) {
	T		maxt		= bits(bit_depth);
	bool	allow_pcm	= img->allow_filter_pcm;
	int		mask		= 5 << vertical;

	for (int dy = 0; dy < deblk.template size<2>(); dy += 1 + !vertical) {
		for (int dx = 0; dx < deblk.template size<1>(); dx += 1 + vertical) {
			if (int bS = deblk[dy][dx] & mask) {
				int		x		= x0 + (dx << 2); // -> pixel
				int		y		= y0 + (dy << 2); // -> pixel
				int		QPY		= edge_QPY(img, x, y, vertical);
				auto&	filter	= img->get_SliceHeader(x, y)->deblocking;
				int		beta	= filter.beta(QPY)					<< (bit_depth - 8);
				int		tc		= filter.tc(QPY + (bS > 2 ? 2 : 0))	<< (bit_depth - 8);

				// 8.7.2.4.3
				auto	ptr		= plane.slice(x, y);

				vec<int16,4>	q[4], p[4];
				for (int i = 0; i < 4; i++) {
					q[i] = to<int16>(edge_load(ptr, i, vertical));
					p[i] = to<int16>(edge_load(ptr, -i - 1, vertical));
				}

				auto	dp		= abs(p[2].xw - 2 * p[1].xw + p[0].xw);
				auto	dq		= abs(q[2].xw - 2 * q[1].xw + q[0].xw);
				auto	dpq		= dp + dq;

				if (dpq.x + dpq.y < beta) {
					// 8.7.2.4.4
					bool	filterP = img->allow_filter(x - vertical, y - !vertical, allow_pcm);
					bool	filterQ = img->allow_filter(x, y, allow_pcm);
					bool	strong	= all(2 * dpq < (beta >> 2) & abs(p[3].xw - p[0].xw) + abs(q[0].xw - q[3].xw) < (beta >> 3) & abs(p[0].xw - q[0].xw) < ((5 * tc + 1) >> 1));

					if (strong) {
						// strong filtering
						if (filterP) {
							edge_store<T>(to<T>(clamp((p[2] + 2 * p[1] + 2 * p[0] + 2 * q[0] + q[1] + 4) >> 3,	p[0] - 2 * tc, p[0] + 2 * tc)), ptr, -1, vertical);
							edge_store<T>(to<T>(clamp((p[2] + p[1] + p[0] + q[0] + 2) >> 2,						p[1] - 2 * tc, p[1] + 2 * tc)), ptr, -2, vertical);
							edge_store<T>(to<T>(clamp((2 * p[3] + 3 * p[2] + p[1] + p[0] + q[0] + 4) >> 3,		p[2] - 2 * tc, p[2] + 2 * tc)), ptr, -3, vertical);
						}
						if (filterQ) {
							edge_store<T>(to<T>(clamp((p[1] + 2 * p[0] + 2 * q[0] + 2 * q[1] + q[2] + 4) >> 3,	q[0] - 2 * tc, q[0] + 2 * tc)), ptr, 0, vertical);
							edge_store<T>(to<T>(clamp((p[0] + q[0] + q[1] + q[2] + 2) >> 2,						q[1] - 2 * tc, q[1] + 2 * tc)), ptr, 1, vertical);
							edge_store<T>(to<T>(clamp((p[0] + q[0] + q[1] + 3 * q[2] + 2 * q[3] + 4) >> 3,		q[2] - 2 * tc, q[2] + 2 * tc)), ptr, 2, vertical);
						}

					} else {
						// weak filtering
						auto	delta	= (9 * (q[0] - p[0]) - 3 * (q[1] - p[1]) + 8) >> 4;
						auto	mask	= to<T>(abs(delta)) < tc * 10;
						delta = clamp(delta, -tc, tc);
						if (filterP) {
							edge_store<T>(masked(mask, to<T>(clamp(p[0] + delta, 0, maxt))), ptr, -1, vertical);
							if (dp.x + dp.y < ((beta + (beta >> 1)) >> 3))
								edge_store<T>(masked(mask, to<T>(clamp(p[1] + clamp((((p[2] + p[0] + 1) >> 1) - p[1] + delta) >> 1, -(tc >> 1), tc >> 1), 0, maxt))), ptr, -2, vertical);
						}
						if (filterQ) {
							edge_store<T>(masked(mask, to<T>(clamp(q[0] - delta, 0, maxt))), ptr, 0, vertical);
							if (dq.x + dq.y < ((beta + (beta >> 1)) >> 3))
								edge_store<T>(masked(mask, to<T>(clamp(q[1] + clamp((((q[2] + q[0] + 1) >> 1) - q[1] - delta) >> 1, -(tc >> 1), tc >> 1), 0, maxt))), ptr, +1, vertical);
						}
					}
				}
			}
		}
	}
}

template<typename T> void deblock_chroma(image* img, int x0, int y0, blockptr<T, 2> plane1, blockptr<T, 2> plane2, int bit_depth, bool vertical, block<DeblockFlags,2> deblk, int32x2 qp_offset) {
	auto chroma_format	= img->chroma_format;
	const int shiftw	= get_shift_W(chroma_format);
	const int shifth	= get_shift_H(chroma_format);

	int		xIncr		= (vertical ? 2 : 1) << shiftw;
	int		yIncr		= (vertical ? 1 : 2) << shifth;
	T		maxt		= bits(bit_depth);
	bool	allow_pcm	= img->allow_filter_pcm;
	int		mask		= 4 << vertical;

	for (int dy = 0; dy < deblk.template size<2>(); dy += yIncr) {
		for (int dx = 0; dx < deblk.template size<1>(); dx += xIncr) {
			if (deblk[dy][dx] & mask) {
				int		x		= x0 + (dx << 2);
				int		y		= y0 + (dy << 2);

				int		QPY		= edge_QPY(img, x, y, vertical);
				int		QPC1	= do_chroma_scale(QPY + qp_offset.x, chroma_format, 8);//bit_depth);
				int		QPC2	= do_chroma_scale(QPY + qp_offset.y, chroma_format, 8);//bit_depth);

				auto&	filter	= img->get_SliceHeader(x, y)->deblocking;
				int		tc1		= filter.tc(QPC1 + 2) << (bit_depth - 8);
				int		tc2		= filter.tc(QPC2 + 2) << (bit_depth - 8);
				auto	tc		= concat(vec<int16,4>(tc1), vec<int16,4>(tc2));

				bool	filterP = img->allow_filter(x - vertical, y - !vertical, allow_pcm);
				bool	filterQ = img->allow_filter(x, y, allow_pcm);

				// 8.7.2.4.5
				auto	ptr1	= plane1.slice(x >> shiftw, y >> shifth);
				auto	ptr2	= plane2.slice(x >> shiftw, y >> shifth);

				vec<int16,8>	p[2], q[2];
				for (int i = 0; i < 2; i++) {
					q[i] = to<int16>(concat(edge_load(ptr1, i, vertical),		edge_load(ptr2, i, vertical)));
					p[i] = to<int16>(concat(edge_load(ptr1, -i - 1, vertical),	edge_load(ptr2, -i - 1, vertical)));
				}

				auto delta = clamp(((q[0] - p[0]) * 4 + p[1] - q[1] + 4) >> 3, -tc, tc);
				if (filterP) {
					auto	r = to<T>(clamp(p[0] + delta, 0, maxt));
					edge_store(r.lo, ptr1, -1, vertical);
					edge_store(r.hi, ptr2, -1, vertical);
				}
				if (filterQ) {
					auto	r = to<T>(clamp(q[0] - delta, 0, maxt));
					edge_store(r.lo, ptr1, 0, vertical);
					edge_store(r.hi, ptr2, 0, vertical);
				}
			}
		}
	}
}

// 8.7.2.3 (both, EDGE_VER and EDGE_HOR)
bool deblock_inter_boundary(image* img, int xQ, int yQ, int xP, int yP) {
	auto&	mviP	= img->get_mv_info(xP, yP);
	auto&	mviQ	= img->get_mv_info(xQ, yQ);
	auto	shdrP	= img->get_SliceHeader(xP, yP);
	auto	shdrQ	= img->get_SliceHeader(xQ, yQ);

	auto	refPicP0 = mviP.refIdx[0] >= 0 ? shdrP->ref(0, mviP.refIdx[0]).img : nullptr;
	auto	refPicP1 = mviP.refIdx[1] >= 0 ? shdrP->ref(1, mviP.refIdx[1]).img : nullptr;
	auto	refPicQ0 = mviQ.refIdx[0] >= 0 ? shdrQ->ref(0, mviQ.refIdx[0]).img : nullptr;
	auto	refPicQ1 = mviQ.refIdx[1] >= 0 ? shdrQ->ref(1, mviQ.refIdx[1]).img : nullptr;

	bool	same1	= refPicP0 == refPicQ0 && refPicP1 == refPicQ1;
	bool	same2	= refPicP0 == refPicQ1 && refPicP1 == refPicQ0;
	if (!same1 && !same2)
		return true;

	MotionVector mvP0 = mviP.refIdx[0] >= 0 ? mviP.mv[0] : zero;
	MotionVector mvP1 = mviP.refIdx[1] >= 0 ? mviP.mv[1] : zero;
	MotionVector mvQ0 = mviQ.refIdx[0] >= 0 ? mviQ.mv[0] : zero;
	MotionVector mvQ1 = mviQ.refIdx[1] >= 0 ? mviQ.mv[1] : zero;

	return	(!same1 || any(abs(mvP0 - mvQ0) >= 4 | abs(mvP1 - mvQ1) >= 4))
		&&	(!same2 || any(abs(mvP0 - mvQ1) >= 4 | abs(mvP1 - mvQ0) >= 4));
}

inline bool deblock_trans_boundary(image* img, int xQ, int yQ, int xP, int yP) {
	return img->has_nonzero_coefficient(xP, yP) || deblock_inter_boundary(img, xQ, yQ, xP, yP);
}

inline int deblock_boundary_strength(image* img, int xQ, int yQ, int xP, int yP, PredMode pred_mode) {
	if (pred_mode == MODE_INTRA || img->cu_info.get(xP, yP).pred_mode == MODE_INTRA)
		return 2;

	return img->has_nonzero_coefficient(xQ, yQ) || deblock_trans_boundary(img, xQ, yQ, xP, yP);
}

// 8.7.2.2 for both EDGE_HOR and EDGE_VER at the same time
bool deblock_mark_prediction(image* img, blockptr<DeblockFlags,2> deblk, int x0, int y0, int log2CbSize, PartMode part_mode) {
	int		size		= 1 << log2CbSize;
	uint32	offset		= get_offset(part_mode, size);
	bool	ret			= false;

	if (has_vsplit(part_mode)) {
		auto	d		= deblk.slice((x0 + offset) >> 2, y0 >> 2);
		if (part_mode == PART_NxN || ((img->get_mv_info(x0, y0).flags | img->get_mv_info(x0 + offset, y0).flags) & (PB_info::VSP | PB_info::IV))) {
			for (int k = 0; k < size >> 2; ++k) {
				if (deblock_inter_boundary(img, x0 + offset, y0 + (k << 2), x0 + offset - 1, y0 + (k << 2))) {
					d[k][0] |= DEBLOCK_VERT;
					ret = true;
				}
			}
		} else if (deblock_inter_boundary(img, x0 + offset, y0, x0 + offset - 1, y0)) {
			for (int k = 0; k < size >> 2; ++k)
				d[k][0] |= DEBLOCK_VERT;
			ret = true;
		}
	}
	
	if (has_hsplit(part_mode)) {
		auto	d		= deblk.slice(x0 >> 2, (y0 + offset) >> 2);
		if (part_mode == PART_NxN || ((img->get_mv_info(x0, y0).flags | img->get_mv_info(x0, y0 + offset).flags) & (PB_info::VSP | PB_info::IV))) {
			for (int k = 0; k < size >> 2; ++k) {
				if (deblock_inter_boundary(img, x0 + (k << 2), y0 + offset, x0 + (k << 2), y0 + offset - 1)) {
					d[0][k] |= DEBLOCK_HORZ;
					ret = true;
				}
			}
		} else if (deblock_inter_boundary(img, x0, y0 + offset, x0, y0 + offset - 1)) {
			for (int k = 0; k < size >> 2; ++k)
				d[0][k] |= DEBLOCK_HORZ;
			ret = true;
		}
	}
	return ret;
}

// 8.7.2.1 for both EDGE_HOR and EDGE_VER at the same time
bool deblock_mark_transform(image* img, blockptr<DeblockFlags,2> deblk, int x0, int y0, int log2CbSize, PredMode pred_mode, int tr_depth, uint8 avail) {
	int		size	= 1 << (log2CbSize - tr_depth);
	if (img->get_split_transform(x0, y0, tr_depth)) {
		int x1 = x0 + size / 2;
		int y1 = y0 + size / 2;
		return	bool(0
			|	deblock_mark_transform(img, deblk, x0, y0, log2CbSize, pred_mode, tr_depth + 1, avail)
			|	deblock_mark_transform(img, deblk, x1, y0, log2CbSize, pred_mode, tr_depth + 1, avail | 0x11)
			|	deblock_mark_transform(img, deblk, x0, y1, log2CbSize, pred_mode, tr_depth + 1, avail | 0x22)
			|	deblock_mark_transform(img, deblk, x1, y1, log2CbSize, pred_mode, tr_depth + 1, 0x33)
		);

	} else {
		bool	ret			= false;
		auto	d			= deblk.slice(x0 >> 2, y0 >> 2);
		
		if (avail & AVAIL_A1) {
			if (auto vert_flags = pred_mode == MODE_INTRA ? DEBLOCK_VERT_STRONG : ((avail & 0x10) && img->has_nonzero_coefficient(x0, y0)) ? DEBLOCK_VERT : DEBLOCK_NONE) {
				for (int k = 0; k < size >> 2; ++k)
					d[k][0] |= vert_flags;
				ret = true;

			} else if (size > 4 || (x0 & 4) == 0) {
				for (int k = 0; k < size >> 2; ++k) {
					if (deblock_trans_boundary(img, x0, y0 + (k << 2), x0 - 1, y0 + (k << 2))) {
						d[k][0] |= DEBLOCK_VERT;
						ret = true;
					}
				}
			} else {
				d[0][0] |= DEBLOCK_VERT;
				ret = true;
			}
		}

		if (avail & AVAIL_B1) {
			if (auto horz_flags	= pred_mode == MODE_INTRA ? DEBLOCK_HORZ_STRONG : ((avail & 0x20) && img->has_nonzero_coefficient(x0, y0)) ? DEBLOCK_HORZ : DEBLOCK_NONE) {
				for (int k = 0; k < size >> 2; ++k)
					d[0][k] |= horz_flags;
				ret = true;

			} else if (size > 4 || (y0 & 4) == 0) {
				for (int k = 0; k < size >> 2; ++k) {
					if (deblock_trans_boundary(img, x0 + (k << 2), y0, x0 + (k << 2), y0 -1)) {
						d[0][k] |= DEBLOCK_HORZ;
						ret = true;
					}
				}
			} else {
				d[0][0] |= DEBLOCK_HORZ;
				ret = true;
			}
		}

		return ret;
	}
}

bool deblock_mark_tree(image* img, blockptr<DeblockFlags,2> deblk, int x0, int y0, int log2size, AVAIL avail) {
	auto	&cu = img->cu_info.get(x0, y0);
	if (cu.log2_size == 0)
		return false;

	if (cu.log2_size < log2size) {
		--log2size;
		int x1 = x0 + (1 << log2size);
		int y1 = y0 + (1 << log2size);
		int	w	= img->get_width();
		int	h	= img->get_height();
		return	bool(0
			|	deblock_mark_tree(img, deblk, x0, y0, log2size, avail)
			|	(x1 < w && deblock_mark_tree(img, deblk, x1, y0, log2size, avail | AVAIL_A1))
			|	(y1 < h && deblock_mark_tree(img, deblk, x0, y1, log2size, avail | AVAIL_B1))
			|	(x1 < w && y1 < h && deblock_mark_tree(img, deblk, x1, y1, log2size, AVAIL_A1 | AVAIL_B1))
		);

	} else {
		bool	ret			= false;
		auto	pred_mode	= (PredMode)cu.pred_mode;
		auto	d			= deblk.slice(x0 >> 2, y0 >> 2);

		//outer edges of prediction
		if ((avail & AVAIL_A1)) {
			for (int k = 0; k < 1 << (log2size - 2); ++k) {
				if (int bs = deblock_boundary_strength(img, x0, y0 + (k << 2), x0 - 1, y0 + (k << 2), pred_mode)) {
					d[k][0] |= bs > 1 ? DEBLOCK_VERT_STRONG : DEBLOCK_VERT;
					ret = true;
				}
			}
		}
		if (avail & AVAIL_B1) {
			for (int k = 0; k < 1 << (log2size - 2); ++k) {
				if (int bs = deblock_boundary_strength(img, x0 + (k << 2), y0, x0 + (k << 2), y0 - 1, pred_mode)) {
					d[0][k] |= bs > 1 ? DEBLOCK_HORZ_STRONG : DEBLOCK_HORZ;
					ret = true;
				}
			}
		}

		return	bool((int)ret
			|	deblock_mark_transform(img, deblk, x0, y0, log2size, pred_mode, 0, avail)
			|	deblock_mark_prediction(img, deblk, x0, y0, log2size, (PartMode)cu.part_mode)
		);
	}
}

void apply_deblocking_filter(image* img, bool parallel) {
	auto	deblk	= make_auto_block<DeblockFlags>(img->get_width() >> 2, img->get_height() >> 2);
	auto	enabled_array	= alloc_auto(bool, img->ctu_info.height_in_units);

	fill(deblk, (DeblockFlags)0);

	maybe_parallel_for(parallel, int_range(img->ctu_info.height_in_units), [img, &deblk, enabled_array](int ctby) {
		bool	enabled		= false;
		int		log2_ctu	= img->ctu_info.log2unitSize;

		for (int ctbx = 0; ctbx < img->ctu_info.width_in_units; ++ctbx) {
			auto	ctbflags = img->ctu_info.get0(ctbx, ctby).flags;
			if (!(ctbflags & CTU_info::NO_DEBLOCK))
				enabled |= deblock_mark_tree(img, deblk, ctbx << log2_ctu, ctby << log2_ctu, log2_ctu, soft_available(ctbflags));
		}

		enabled_array[ctby] = enabled;

		if (enabled) {
			int		y		= ctby << log2_ctu;
			auto	d		= deblk.slice<2>(y >> 2, 1 << (log2_ctu - 2));

			int	bit_depth	= img->planes[0].bit_depth;
			if (bit_depth <= 8) {
				deblock_luma(img, 0, y, img->get_plane_ptr<uint8 >(0), 8, true, d);
				//deblock_luma(img, 0, y, img->get_plane_ptr<uint8 >(0), 8, false, d);
			} else {
				deblock_luma(img, 0, y, img->get_plane_ptr<uint16>(0), bit_depth, true, d);
				//deblock_luma(img, 0, y, img->get_plane_ptr<uint16>(0), bit_depth, false, d);
			}

			if (img->ChromaArrayType != CHROMA_MONO) {
				int		bit_depth	= img->planes[1].bit_depth;
				auto	qp_offset	= img->pps->qp_offset;
				if (bit_depth <= 8) {
					deblock_chroma(img, 0, y, img->get_plane_ptr<uint8 >(1), img->get_plane_ptr<uint8 >(2), 8, true, d, qp_offset);
					//deblock_chroma(img, 0, y, img->get_plane_ptr<uint8 >(1), img->get_plane_ptr<uint8 >(2), 8, false, d, qp_offset);
				} else {
					deblock_chroma(img, 0, y, img->get_plane_ptr<uint16>(1), img->get_plane_ptr<uint16>(2), bit_depth, true, d, qp_offset);
					//deblock_chroma(img, 0, y, img->get_plane_ptr<uint16>(1), img->get_plane_ptr<uint16>(2), bit_depth, false, d, qp_offset);
				}
			}
		}
	});

	maybe_parallel_for(parallel, int_range(img->ctu_info.height_in_units), [img, &deblk, enabled_array](int ctby) {
		if (enabled_array[ctby]) {
			int		log2_ctu	= img->ctu_info.log2unitSize;
			int		y			= ctby << log2_ctu;
			auto	d			= deblk.slice<2>(y >> 2, 1 << (log2_ctu - 2));

			int	bit_depth	= img->planes[0].bit_depth;
			if (bit_depth <= 8) {
				//deblock_luma(img, 0, y, img->get_plane_ptr<uint8 >(0), 8, true, d);
				deblock_luma(img, 0, y, img->get_plane_ptr<uint8 >(0), 8, false, d);
			} else {
				//deblock_luma(img, 0, y, img->get_plane_ptr<uint16>(0), bit_depth, true, d);
				deblock_luma(img, 0, y, img->get_plane_ptr<uint16>(0), bit_depth, false, d);
			}

			if (img->ChromaArrayType != CHROMA_MONO) {
				int		bit_depth	= img->planes[1].bit_depth;
				auto	qp_offset	= img->pps->qp_offset;
				if (bit_depth <= 8) {
					//deblock_chroma(img, 0, y, img->get_plane_ptr<uint8 >(1), img->get_plane_ptr<uint8 >(2), 8, true, d, qp_offset);
					deblock_chroma(img, 0, y, img->get_plane_ptr<uint8 >(1), img->get_plane_ptr<uint8 >(2), 8, false, d, qp_offset);
				} else {
					//deblock_chroma(img, 0, y, img->get_plane_ptr<uint16>(1), img->get_plane_ptr<uint16>(2), bit_depth, true, d, qp_offset);
					deblock_chroma(img, 0, y, img->get_plane_ptr<uint16>(1), img->get_plane_ptr<uint16>(2), bit_depth, false, d, qp_offset);
				}
			}
		}
	});
}

//-----------------------------------------------------------------------------
// sample_adaptive_offset
//-----------------------------------------------------------------------------

template<class T> void apply_sao_internal(image* img, int xCtb, int yCtb, int log2_ctu, int shiftw, int shifth, blockptr<const T, 2> in_img, block<T, 2> out_img, const sao_info &info, int bit_depth) {
	if (info.type == 0)
		return;

	// top left position of CTB in pixels
	const int	x		= xCtb << log2_ctu;
	const int	y		= yCtb << log2_ctu;
	const int	w		= out_img.template size<1>();

	const int	max_pixel		= bits(bit_depth);
	const bool	extended_tests	= img->ctu_info.get0(xCtb, yCtb).flags & CTU_info::HAS_PCM_OR_BYPASS;
	const bool	allow_pcm		= img->allow_filter_pcm;

	if (info.type == 1) {
		int		table_shift	= bit_depth - 5;
		int8	offset[32]	= {0};
		for (int k = 0; k < 4; k++)
			offset[(k + info.pos) & 31] = info.offset[k];

		for (int j = 0, h = out_img.template size<2>(); j < h; j++) {
			auto	in_ptr	= in_img[j];
			auto	out_ptr	= out_img[j];

			for (int i = 0; i < w; i++) {
				if (!extended_tests || img->allow_filter(x + (i << shiftw), y + (j << shifth), allow_pcm)) {
					if (int v = offset[in_ptr[i] >> table_shift])
						out_ptr[i] = clamp(in_ptr[i] + v, 0, max_pixel);
				}
			}
		}

	} else {
		// 2..5 (clss = 0..3)
		int	dx, dy;
		switch (info.type - 2) {
			case 0: dx = -1; dy =  0; break;
			case 1: dx =  0; dy = -1; break;
			case 2: dx = -1; dy = -1; break;
			case 3: dx =  1; dy = -1; break;
		}

		// Reorder sao_info.offset[] array, so that we can index it directly with the sum of the two pixel-difference signs
		int8  offset[5] = {
			info.offset[0],
			info.offset[1],
			0,
			info.offset[2],
			info.offset[3],
		};

		// get adjacent CTB filter flags
		uint32		avail	= soft_available(img->ctu_info.get0(xCtb, yCtb).flags);			//left: bit0, top: bit1

		if (dx && xCtb + 1 < img->ctu_info.width_in_units)
			avail |= soft_available(img->ctu_info.get0(xCtb + 1, yCtb).flags) << 2;			//right: bit2

		if (dy && yCtb + 1 < img->ctu_info.height_in_units)
			avail |= soft_available(img->ctu_info.get0(xCtb, yCtb + 1).flags) << 4;			//bottom: bit5

		for (int j = 0, h = out_img.template size<2>(); j < h; j++) {
			int		row_avail	= avail | 2;	//left, middle, right

			if (dy && (j == 0 || j == h - 1)) {
				bool	skipv	= !(avail & (j == 0 ? 2 : 32));
				if (skipv)
					row_avail = 0;

				if (dx) {
					if (j == 0) {
						if (dx != dy)
							row_avail = (row_avail & ~4) | (img->filter_top_right0(xCtb, yCtb) << 2);		//right = topright
					} else if (dx == dy) {
						row_avail = (row_avail & ~4) | (((avail & 0x24) == 0x24) << 2);						//right = bottomright
					} else {
						row_avail = (row_avail & ~1) | (img->filter_top_right0(xCtb - 1, yCtb + 1) << 0);	//left = bottomleft
					}
				}
				if (row_avail == 0)
					continue;	//skip whole row
			}

			auto	in_ptr	= in_img[j];
			auto	in_ptr0	= in_img[j + dy] + dx;
			auto	in_ptr1	= in_img[j - dy] - dx;
			auto	out_ptr	= out_img[j];

			for (int i = 0; i < w; i++) {
				if (extended_tests && !img->allow_filter(x + (i << shiftw), y + (j << shifth), allow_pcm))
					continue;

				if (dx && !(row_avail & (1 << (i == 0 ? 0 : i == w - 1 ? 2 : 1))))
					continue;

				int edge	= sign(in_ptr[i] - in_ptr0[i]) + sign(in_ptr[i] - in_ptr1[i]);
				out_ptr[i]	= clamp(in_ptr[i] + offset[edge + 2], 0, max_pixel);
			}
		}
	}
}

void apply_sample_adaptive_offset(image* img, bool parallel) {
	for (int c = 0, n = num_channels(img->ChromaArrayType); c < n; c++) {
		malloc_block	inputCopy = img->planes[c].pixels;

		maybe_parallel_for_block(parallel, int_range(img->ctu_info.total()), [img, c, &inputCopy](int ctu) {
			int		yCtb	= ctu / img->ctu_info.width_in_units;
			int		xCtb	= ctu % img->ctu_info.width_in_units;
			auto&	ctb		= img->ctu_info.get0(xCtb, yCtb);

			if (ctb.has_sao(c)) {
				const int log2_ctu	= img->ctu_info.log2unitSize;
				const int stride	= img->get_width(c);
				const int shiftw	= get_shift_W(img->chroma_format, c);
				const int shifth	= get_shift_H(img->chroma_format, c);
				const int w			= 1 << (log2_ctu - shiftw);
				const int h			= 1 << (log2_ctu - shifth);
				auto	bit_depth	= img->get_bit_depth(c);

				// in pixels
				const int	x		= xCtb << (log2_ctu - shiftw);
				const int	y		= yCtb << (log2_ctu - shifth);

				if (bit_depth <= 8)
					apply_sao_internal(img, xCtb,yCtb, log2_ctu, shiftw, shifth, make_blockptr<const uint8 >(inputCopy, stride).slice(x, y), img->get_plane_block<uint8 >(c, x, y, w, h), ctb.sao_info[c], 8);
				else
					apply_sao_internal(img, xCtb,yCtb, log2_ctu, shiftw, shifth, make_blockptr<const uint16>(inputCopy, stride * 2).slice(x, y), img->get_plane_block<uint16>(c, x, y, w, h), ctb.sao_info[c], bit_depth);
			}
		});
	}
}

//-----------------------------------------------------------------------------
// scalability
//-----------------------------------------------------------------------------
#ifdef HEVC_ML

//Table H.1 - 16-phase luma resampling filter
const int8x8 luma_resample_filters[] = {
	{    0,   0,   0,  64,   0,   0,   0,   0},
	{    0,   1,  -3,  63,   4,  -2,   1,   0},
	{   -1,   2,  -5,  62,   8,  -3,   1,   0},
	{   -1,   3,  -8,  60,  13,  -4,   1,   0},
	{   -1,   4, -10,  58,  17,  -5,   1,   0},
	{   -1,   4, -11,  52,  26,  -8,   3,  -1},
	{   -1,   3,  -9,  47,  31,  10,   4,  -1},
	{   -1,   4, -11,  45,  34,  10,   4,  -1},
	{   -1,   4, -11,  40,  40,  11,   4,  -1},
	{   -1,   4, -10,  34,  45,  11,   4,  -1},
	{   -1,   4, -10,  31,  47,  -9,   3,  -1},
	{   -1,   3,  -8,  26,  52, -11,   4,  -1},
	{    0,   1,  -5,  17,  58,  10,   4,  -1},
	{    0,   1,  -4,  13,  60,  -8,   3,  -1},
	{    0,   1,  -3,   8,  62,  -5,   2,  -1},
	{    0,   1,  -2,   4,  63,  -3,   1,   0},
};

//Table H.2 - 16-phase chroma resampling filter
const int8x4 chroma_resample_filters[] = {
	{    0,  64,   0,   0},
	{   -2,  62,   4,   0},
	{   -2,  58,  10,  -2},
	{   -4,  56,  14,  -2},
	{   -4,  54,  16,  -2},
	{   -6,  52,  20,  -2},
	{   -6,  46,  28,  -4},
	{   -4,  42,  30,  -4},
	{   -4,  36,  36,  -4},
	{   -4,  30,  42,  -4},
	{   -4,  28,  46,  -6},
	{   -2,  20,  52,  -6},
	{   -2,  16,  54,  -4},
	{   -2,  14,  56,  -4},
	{   -2,  10,  58,  -2},
	{    0,   4,  62,  -2},
};

template<typename S, typename D> void scale_luma(block<S, 2> src, block<D, 2> dst, int phasew, int phaseh, int src_bit_depth, int dst_bit_depth, bool parallel) {
	auto	src_width	= src.template size<1>();
	auto	src_height	= src.template size<2>();
	auto	dst_width	= dst.template size<1>();
	auto	dst_height	= dst.template size<2>();

	auto	scalew	= ((src_width  << 16) + (dst_width  >> 1)) / dst_width;
	auto	scaleh	= ((src_height << 16) + (dst_height >> 1)) / dst_height;

	auto	addw	= -((scalew * phasew + 8) >> 4);
	auto	addh	= -((scaleh * phaseh + 8) >> 4);

	int		shift1	= src_bit_depth - 8;
	int		shift2	= 20 - dst_bit_depth;
	int		offset	= 1 << (shift2 - 1);
	int		maxt	= bits(dst_bit_depth);
	int		maxx	= src_width - 1;

	uint32	temp_stride	= (src_height + 15) & -16;
	auto	temp	= make_auto_block<int>(temp_stride, dst_width);

	auto	offsets = to<int>(meta::make_value_sequence<16, int>());

	for (int y = 0; y < src_height; y += 16) {
		auto	src_row	= src[y].begin();
		auto	off		= clamp(offsets - 3, -y, src_height - y) * src_width;
		for (int x = 0; x < dst_width; x++) {
			int		xref16	= (x * scalew + addw + (1 << 11)) >> 12;
			auto	xref	= xref16 >> 4;
			auto	filter	= luma_resample_filters[xref16 & 15];
			auto	r		= (
					filter[0] * to<int>(gather(src_row + clamp(xref - 3, 0, maxx), off))
				+	filter[1] * to<int>(gather(src_row + clamp(xref - 2, 0, maxx), off))
				+	filter[2] * to<int>(gather(src_row + clamp(xref - 1, 0, maxx), off))
				+	filter[3] * to<int>(gather(src_row + clamp(xref + 0, 0, maxx), off))
				+	filter[4] * to<int>(gather(src_row + clamp(xref + 1, 0, maxx), off))
				+	filter[5] * to<int>(gather(src_row + clamp(xref + 2, 0, maxx), off))
				+	filter[6] * to<int>(gather(src_row + clamp(xref + 3, 0, maxx), off))
				+	filter[7] * to<int>(gather(src_row + clamp(xref + 4, 0, maxx), off))
				) >> shift1;

			store(r, temp[x] + y);
		}
	}

	for (int x = 0; x < dst_width; x += 16) {
		auto	temp_row = temp[x].begin();
		auto	off		= min(offsets, dst_width - x) * temp_stride;
		for (int y = 0; y < dst_height; y++) {
			int		yref16	= (y * scaleh + addh + (1 << 11)) >> 12;
			auto	yref	= yref16 >> 4;
			auto	filter	= luma_resample_filters[yref16 & 15];
			auto	r		= clamp((
					filter[0] * gather(temp_row + yref + 0, off)
				+	filter[1] * gather(temp_row + yref + 1, off)
				+	filter[2] * gather(temp_row + yref + 2, off)
				+	filter[3] * gather(temp_row + yref + 3, off)
				+	filter[4] * gather(temp_row + yref + 4, off)
				+	filter[5] * gather(temp_row + yref + 5, off)
				+	filter[6] * gather(temp_row + yref + 6, off)
				+	filter[7] * gather(temp_row + yref + 7, off)
				+ offset) >> shift2, 0, maxt);

			store(to<D>(r), dst[y] + x);
		}
	}
}

template<typename S, typename D> void scale_chroma(block<S, 2> src, block<D, 2> dst, int phasew, int phaseh, int src_bit_depth, int dst_bit_depth, bool parallel) {
	auto	src_width	= src.template size<1>();
	auto	src_height	= src.template size<2>();
	auto	dst_width	= dst.template size<1>();
	auto	dst_height	= dst.template size<2>();

	auto	scalew	= ((src_width  << 16) + (dst_width  >> 1)) / dst_width;
	auto	scaleh	= ((src_height << 16) + (dst_height >> 1)) / dst_height;

	auto	addw	= -((scalew * phasew + 8) >> 4);
	auto	addh	= -((scaleh * phaseh + 8) >> 4);

	int		shift1	= src_bit_depth - 8;
	int		shift2	= 20 - dst_bit_depth;
	int		offset	= 1 << (shift2 - 1);
	int		maxt	= bits(dst_bit_depth);
	int		maxx	= src_width - 1;

	uint32	temp_stride	= (src_height + 15) & -16;
	auto	temp	= make_auto_block<int>(temp_stride, dst_width);

	auto	offsets = to<int>(meta::make_value_sequence<16, int>());

	for (int y = 0; y < src_height; y += 16) {
		auto	src_row	= src[y].begin();
		auto	off		= clamp(offsets - 1, -y, src_height - y) * src_width;
		for (int x = 0; x < dst_width; x++) {
			int		xref16	= (x * scalew + addw + (1 << 11)) >> 12;
			auto	xref	= xref16 >> 4;
			auto	filter	= chroma_resample_filters[xref16 & 15];
			auto	r = (
					filter[0] * to<int>(gather(src_row + clamp(xref - 1, 0, maxx), off))
				+	filter[1] * to<int>(gather(src_row + clamp(xref + 0, 0, maxx), off))
				+	filter[2] * to<int>(gather(src_row + clamp(xref + 1, 0, maxx), off))
				+	filter[3] * to<int>(gather(src_row + clamp(xref + 2, 0, maxx), off))
				) >> shift1;

			store(r, temp[x] + y);
		}
	}

	for (int x = 0; x < dst_width; x += 16) {
		auto	temp_row = temp[x].begin();
		auto	off		= min(offsets, dst_width - x) * temp_stride;
		for (int y = 0; y < dst_height; y++) {
			int		yref16	= (y * scaleh + addh + (1 << 11)) >> 12;
			auto	yref	= yref16 >> 4;
			auto	filter	= chroma_resample_filters[yref16 & 15];
			auto	r		= clamp((
					filter[0] * gather(temp_row + yref + 0, off)
				+	filter[1] * gather(temp_row + yref + 1, off)
				+	filter[2] * gather(temp_row + yref + 2, off)
				+	filter[3] * gather(temp_row + yref + 3, off)
				+ offset) >> shift2, 0, maxt);

			store(to<D>(r), dst[y] + x);
		}
	}
}

void scale_image(const extension_multilayer::region *region, const image *src, image *dst, bool parallel) {
	CHROMA	src_chroma_format	= src->chroma_format;
	int		src_bit_depth_luma	= src->planes[0].bit_depth;
	int		src_bit_depth_chroma= src->planes[1].bit_depth;
	auto	src_shiftw			= get_shift_W(src_chroma_format);
	auto	src_shifth			= get_shift_H(src_chroma_format);
	int		src_offsetx			= 0;
	int		src_offsety			= 0;
	int		src_width			= src->get_width();
	int		src_height			= src->get_height();

	if (region && region->ref_region.exists()) {
		auto	rr	= get(region->ref_region);
		src_offsetx	= rr[0] << src_shiftw;
		src_offsety	= rr[1] << src_shifth;
		src_width	-= (rr[0] + rr[2]) << src_shiftw;
		src_height	-= (rr[1] + rr[3]) << src_shifth;
	}

	CHROMA	dst_chroma_format	= dst->chroma_format;
	int		dst_bit_depth_luma	= dst->planes[0].bit_depth;
	int		dst_bit_depth_chroma= dst->planes[1].bit_depth;
	auto	dst_shiftw			= get_shift_W(dst_chroma_format);
	auto	dst_shifth			= get_shift_H(dst_chroma_format);
	int		dst_offsetx			= 0;
	int		dst_offsety			= 0;
	int		dst_width			= dst->get_width();
	int		dst_height			= dst->get_height();

	if (region && region->scaled_ref_layer.exists()) {
		auto	rr	= get(region->scaled_ref_layer);
		dst_offsetx	= rr[0] << dst_shiftw;
		dst_offsety	= rr[1] << dst_shifth;
		dst_width	-= (rr[0] + rr[2]) << dst_shiftw;
		dst_height	-= (rr[1] + rr[3]) << dst_shifth;
	}

	uint32	phaseh = 0, phasev = 0;
	if (region && region->phase.exists()) {
		phaseh = region->phase->hor_luma;
		phasev = region->phase->ver_luma;
	}

	if (src_bit_depth_luma <= 8) {
		if (dst_bit_depth_luma <= 8)
			scale_luma(
				src->get_plane_block<uint8 >(0, src_offsetx, src_offsety, src_width, src_height),
				dst->get_plane_block<uint8 >(0, dst_offsetx, dst_offsety, dst_width, dst_height),
				phaseh, phasev, 8, 8, parallel
			);
		else
			scale_luma(
				src->get_plane_block<uint8 >(0, src_offsetx, src_offsety, src_width, src_height),
				dst->get_plane_block<uint16>(0, dst_offsetx, dst_offsety, dst_width, dst_height),
				phaseh, phasev, 8, dst_bit_depth_luma, parallel
			);
	} else {
		if (dst_bit_depth_luma <= 8)
			scale_luma(
				src->get_plane_block<uint16>(0, src_offsetx, src_offsety, src_width, src_height),
				dst->get_plane_block<uint8 >(0, dst_offsetx, dst_offsety, dst_width, dst_height),
				phaseh, phasev, src_bit_depth_luma, 8, parallel
			);
		else
			scale_luma(
				src->get_plane_block<uint16>(0, src_offsetx, src_offsety, src_width, src_height),
				dst->get_plane_block<uint16>(0, dst_offsetx, dst_offsety, dst_width, dst_height),
				phaseh, phasev, src_bit_depth_luma, dst_bit_depth_luma, parallel
			);
	}

	if (dst_chroma_format != CHROMA_MONO) {
		src_offsetx >>= src_shiftw;
		src_offsety >>= src_shifth;
		src_width	>>= src_shiftw;
		src_height	>>= src_shifth;

		dst_offsetx >>= dst_shiftw;
		dst_offsety >>= dst_shifth;
		dst_width	>>= dst_shiftw;
		dst_height	>>= dst_shifth;

		if (region && region->phase.exists()) {
			phaseh = region->phase->hor_chroma;
			phasev = region->phase->ver_chroma;
		}

		if (src_chroma_format == CHROMA_MONO) {
			int	v = 1 << (dst_bit_depth_chroma - 1);
			if (dst_bit_depth_chroma <= 8) {
				fill(dst->get_plane_block<uint8 >(1, dst_offsetx, dst_offsety, dst_width, dst_height), v);
				fill(dst->get_plane_block<uint8 >(2, dst_offsetx, dst_offsety, dst_width, dst_height), v);
			} else {
				fill(dst->get_plane_block<uint16>(1, dst_offsetx, dst_offsety, dst_width, dst_height), v);
				fill(dst->get_plane_block<uint16>(2, dst_offsetx, dst_offsety, dst_width, dst_height), v);

			}

		} else if (src_bit_depth_chroma <= 8) {
			if (dst_bit_depth_chroma <= 8) {
				scale_chroma(
					src->get_plane_block<uint8 >(1, src_offsetx, src_offsety, src_width, src_height),
					dst->get_plane_block<uint8 >(1, dst_offsetx, dst_offsety, dst_width, dst_height),
					phaseh, phasev, 8, 8, parallel
				);
				scale_chroma(
					src->get_plane_block<uint8 >(2, src_offsetx, src_offsety, src_width, src_height),
					dst->get_plane_block<uint8 >(2, dst_offsetx, dst_offsety, dst_width, dst_height),
					phaseh, phasev, 8, 8, parallel
				);
			} else {
				scale_chroma(
					src->get_plane_block<uint8 >(1, src_offsetx, src_offsety, src_width, src_height),
					dst->get_plane_block<uint16>(1, dst_offsetx, dst_offsety, dst_width, dst_height),
					phaseh, phasev, 8, dst_bit_depth_chroma, parallel
				);
				scale_chroma(
					src->get_plane_block<uint8 >(2, src_offsetx, src_offsety, src_width, src_height),
					dst->get_plane_block<uint16>(2, dst_offsetx, dst_offsety, dst_width, dst_height),
					phaseh, phasev, 8, dst_bit_depth_chroma, parallel
				);
			}
		} else {
			if (dst_bit_depth_chroma <= 8) {
				scale_chroma(
					src->get_plane_block<uint16>(1, src_offsetx, src_offsety, src_width, src_height),
					dst->get_plane_block<uint8 >(1, dst_offsetx, dst_offsety, dst_width, dst_height),
					phaseh, phasev, src_bit_depth_chroma, 8, parallel
				);
				scale_chroma(
					src->get_plane_block<uint16>(2, src_offsetx, src_offsety, src_width, src_height),
					dst->get_plane_block<uint8 >(2, dst_offsetx, dst_offsety, dst_width, dst_height),
					phaseh, phasev, src_bit_depth_chroma, 8, parallel
				);
			} else {
				scale_chroma(
					src->get_plane_block<uint16>(1, src_offsetx, src_offsety, src_width, src_height),
					dst->get_plane_block<uint16>(1, dst_offsetx, dst_offsety, dst_width, dst_height),
					phaseh, phasev, src_bit_depth_chroma, dst_bit_depth_chroma, parallel
				);
				scale_chroma(
					src->get_plane_block<uint16>(2, src_offsetx, src_offsety, src_width, src_height),
					dst->get_plane_block<uint16>(2, dst_offsetx, dst_offsety, dst_width, dst_height),
					phaseh, phasev, src_bit_depth_chroma, dst_bit_depth_chroma, parallel
				);
			}
		}
	}
}


//	H.8.1.4.3 Resampling process of picture motion and mode parameters
void scale_mvs(const extension_multilayer::region *region, const image *src, image *dst, bool parallel) {
	CHROMA	chroma_format	= src->chroma_format;
	auto	shiftw			= get_shift_W(chroma_format);
	auto	shifth			= get_shift_H(chroma_format);

	int		src_width			= src->get_width();
	int		src_height			= src->get_height();
	int		src_region_offsetx	= 0;
	int		src_region_offsety	= 0;
	int		src_region_width	= src_width;
	int		src_region_height	= src_height;

	if (region && region->ref_region.exists()) {
		auto	rr	= get(region->ref_region);
		src_region_offsetx	= rr[0] << shiftw;
		src_region_offsety	= rr[1] << shifth;
		src_region_width	-= (rr[0] + rr[2]) << shiftw;
		src_region_height	-= (rr[1] + rr[3]) << shifth;
	}

	int		dst_width			= dst->get_width();
	int		dst_height			= dst->get_height();
	int		dst_region_offsetx	= 0;
	int		dst_region_offsety	= 0;
	int		dst_region_width	= dst_width;
	int		dst_region_height	= dst_height;

	if (region && region->scaled_ref_layer.exists()) {
		auto	rr	= get(region->scaled_ref_layer);
		dst_region_offsetx	= rr[0] << shiftw;
		dst_region_offsety	= rr[1] << shifth;
		dst_region_width	-= (rr[0] + rr[2]) << shiftw;
		dst_region_height	-= (rr[1] + rr[3]) << shifth;
	}

	auto	scalew	= ((src_region_width  << 16) + (dst_region_width  >> 1)) / dst_region_width;
	auto	scaleh	= ((src_region_height << 16) + (dst_region_height >> 1)) / dst_region_height;

	int16x2	mvscale = {
		src_region_width  != dst_region_width  ? clamp(((src_region_width  << 8) + (dst_region_width  >> 1)) / dst_region_width,  -4096, 4095) : 256,
		src_region_height != dst_region_height ? clamp(((src_region_height << 8) + (dst_region_height >> 1)) / dst_region_height, -4096, 4095) : 256
	};

	dst->cu_info.alloc2(dst_width, dst_height, 4);
	dst->pb_info.alloc2(dst_width, dst_height, 4);

	for (int xB = 0; xB < (dst_width + 15) >> 4; xB++) {
		for (int yB = 0; yB < (dst_height + 15) >> 4; yB++) {
			int		xP		= xB * 16, yP = yB * 16;
			auto	xref	= (((xP + 8 - dst_region_offsetx) * scalew + (1 << 15)) >> 16) + src_region_offsetx;
			auto	yref	= (((yP + 8 - dst_region_offsety) * scaleh + (1 << 15)) >> 16) + src_region_offsety;
			int		xRL		= (xref + 4) & -16;
			int		yRL		= (yref + 4) & -16;

			auto&	dst_cb	= dst->cu_info.get0(xB, yB);
			auto&	dst_pb	= dst->pb_info.get0(xB, yB);

			if (xRL < 0 || xRL >= src_width || yRL < 0 || yRL >= src_height)
				dst_cb.pred_mode = MODE_INTRA;
			else
				dst_cb.pred_mode = src->get_pred_mode(xRL, yRL);

			if (dst_cb.pred_mode == MODE_INTER) {
				auto	src_pb		= src->pb_info.get(xRL, yRL);
				for (int X = 0; X < 2; X++) {
					dst_pb.refIdx[X]	= src_pb.refIdx[X];
					if (src_pb.refIdx[X] >= 0)
						dst_pb.mv[X]	= scale_mv(src_pb.mv[X], mvscale);
				}

			} else {
				dst_pb.refIdx[0] = dst_pb.refIdx[1] = -1;
			}
		}
	}
}


//H.8.1.4.4.2 Colour mapping process of luma sample values
template<typename TY, typename TC, typename D> void map_luma_internal(const extension_multilayer::colour_mapping *cm, CHROMA chroma_format, block<TY, 2> src_y, block<TC, 2> src_cb, block<TC, 2> src_cr, D *dst_y, bool parallel) {
	auto	shiftw	= get_shift_W(chroma_format);
	auto	shifth	= get_shift_H(chroma_format);

	for (int yP = 0; yP < src_y.template size<2>(); yP++) {
		int		yPC		= yP >> shifth;
		auto	row_y	= src_y[yP].begin();
		auto	row_cb	= src_cb[yPC].begin();
		auto	row_cr	= src_cr[yPC].begin();
		int		offset	= yP & 1
			? (yPC < src_cb.template size<2>() - 1 ? src_cb.pitch() : 0)
			: (yPC > 0 ? -src_cb.pitch() : 0);

		for (int xP = 0; xP < src_y.template size<1>(); xP++) {
			int		xPC = xP >> shiftw;
			int		y	= row_y[xP];
			int		cb	= row_cb[xPC];
			int		cr	= row_cr[xPC];

			if (shifth) {
				int	cb2 = row_cb[offset + xPC];
				int	cr2 = row_cr[offset + xPC];
				if (xP & 1) {
					int		xP2C = min(xPC + 1, src_cb.template size<1>() - 1);
					cb	+= row_cb[xP2C]; cb2 += row_cb[offset + xP2C];
					cr	+= row_cr[xP2C]; cr2 += row_cr[offset + xP2C];

					if (yP & 1) {
						cb = (cb * 3 + cb2 + 4) >> 3;
						cr = (cr * 3 + cr2 + 4) >> 3;
					} else {
						cb = (cb2 + cb * 3 + 4) >> 3;
						cr = (cr2 + cr * 3 + 4) >> 3;
					}
				} else {
					cb = (cb * 3 + cb2 + 2) >> 2;
					cr = (cr * 3 + cr2 + 2) >> 2;
				}
			} else if (shiftw && (xP & 1)) {
				int	xP2C = min(xPC + 1, src_cb.template size<1>() - 1);
				cb = (cb + row_cb[xP2C] + 1) >> 1;
				cr = (cr + row_cr[xP2C] + 1) >> 1;
			}

			*dst_y++	= cm->get_Y(y, cb, cr);
		}
	}
}

//H.8.1.4.4.3 Colour mapping process of chroma sample values
template<typename TY, typename TC, typename D> void map_chroma_internal(const extension_multilayer::colour_mapping *cm, CHROMA chroma_format, block<TY, 2> src_y, block<TC, 2> src_cb, block<TC, 2> src_cr, D *dst_cb, D *dst_cr, bool parallel) {
	auto	shiftw	= get_shift_W(chroma_format);
	auto	shifth	= get_shift_H(chroma_format);

	for (int yPC = 0; yPC < src_cb.template size<2>(); yPC++) {
		int		yP		= yPC << shifth;
		int		offset	= src_y.pitch();
		auto	row_y	= src_y[yP].begin();
		auto	row_cb	= src_cb[yPC].begin();
		auto	row_cr	= src_cr[yPC].begin();

		for (int xPC = 0; xPC < src_cb.template size<1>(); xPC++) {
			int		xP = xPC << shiftw;
			int		y = row_y[xP];
			if (shifth)
				y = (y + row_y[offset + xP] + 1) >> 1;

			int		cb	= row_cb[xPC];
			int		cr	= row_cr[xPC];

			*dst_cb++ = cm->get_Cb(y, cb, cr);
			*dst_cr++ = cm->get_Cr(y, cb, cr);
		}
	}
}

//H.8.1.4.4 Colour mapping process of picture sample values
template<typename TY, typename TC> void map_colours_internal(const extension_multilayer::colour_mapping *cm, const image *img, image *dst, bool parallel) {
	CHROMA	chroma_format	= img->chroma_format;
	auto	src_y			= img->get_plane_block<TY>(0);
	auto	src_cb			= img->get_plane_block<TC>(1);
	auto	src_cr			= img->get_plane_block<TC>(2);

	if (cm->luma_bit_depth_output <= 8)
		map_luma_internal(cm, chroma_format, src_y, src_cb, src_cr, dst->get_plane_ptr<uint8>(0).begin(), parallel);
	else
		map_luma_internal(cm, chroma_format, src_y, src_cb, src_cr, dst->get_plane_ptr<uint16>(0).begin(), parallel);

	if (cm->chroma_bit_depth_output <= 8)
		map_chroma_internal(cm, chroma_format, src_y, src_cb, src_cr, dst->get_plane_ptr<uint8>(1).begin(), dst->get_plane_ptr<uint8>(2).begin(), parallel);
	else
		map_chroma_internal(cm, chroma_format, src_y, src_cb, src_cr, dst->get_plane_ptr<uint16>(1).begin(), dst->get_plane_ptr<uint16>(2).begin(), parallel);

}

image *map_colours(const extension_multilayer::colour_mapping *cm, const image *img, image *dst, bool parallel) {
	int		bit_depth_luma	= img->planes[0].bit_depth;
	int		bit_depth_chroma= img->planes[1].bit_depth;

	if (bit_depth_luma <= 8) {
		if (bit_depth_chroma <= 8)
			map_colours_internal<uint8,  uint8 >(cm, img, dst, parallel);
		else
			map_colours_internal<uint8,  uint16>(cm, img, dst, parallel);
	} else {
		if (bit_depth_chroma <= 8)
			map_colours_internal<uint16, uint8 >(cm, img, dst, parallel);
		else
			map_colours_internal<uint16, uint16>(cm, img, dst, parallel);
	}

	return dst;
}

image *interlayer_image(const pic_parameter_set *dst_pps, int dst_layer_id, image *src_img, bool parallel) {
	int src_layer_id	= src_img->layer_id;
	const extension_multilayer::region	*region = nullptr;

	for (auto& i : dst_pps->extension_multilayer.regions) {
		if (i.a == src_layer_id) {
			region = &i.b;
			break;
		}
	}

	auto	colour_mapping	= dst_pps->extension_multilayer.colour_mapping.exists_ptr();
	if (colour_mapping) {
		if (!(colour_mapping->layer_enable & bit64(src_layer_id)))
			colour_mapping = nullptr;
	}

	auto	src_pps	= src_img->pps.get();
	auto	src_sps	= src_pps->sps.get();
	auto	dst_sps	= dst_pps->sps.get();

	bool	equalSizeAndOffset = (!region || (!region->ref_region.exists() && !region->scaled_ref_layer.exists() && !region->phase.exists()))
		&&	dst_sps->pic_width_luma  == src_sps->pic_width_luma
		&&	dst_sps->pic_height_luma == src_sps->pic_height_luma;

	auto	dst_bit_depth_luma		= dst_sps->bit_depth_luma;
	auto	dst_bit_depth_chroma	= dst_sps->bit_depth_chroma;
	auto	dst_chroma_format		= dst_sps->chroma_format;
	auto	src_chroma_format		= src_sps->chroma_format;

	if (equalSizeAndOffset && dst_bit_depth_luma == src_sps->bit_depth_luma && dst_bit_depth_chroma == src_sps->bit_depth_chroma && dst_chroma_format == src_chroma_format && !colour_mapping)
		return src_img;

	auto	vps				= dst_sps->vps;
	auto	src_layer_idx	= vps->extension_multilayer->layer_index(src_layer_id);
	auto	dst_layer		= vps->extension_multilayer->layer_by_id(dst_layer_id);

	image	*dst_img		= nullptr;

	if (dst_layer->InterLayerSamplePredictionEnabled(src_layer_idx)) {
		auto	cm_img = src_img;

		if (colour_mapping) {
			cm_img	= new image;
			cm_img->create(src_img->get_width(), src_img->get_height(), src_img->chroma_format, colour_mapping->luma_bit_depth_output, colour_mapping->chroma_bit_depth_output);
			map_colours(colour_mapping, src_img, cm_img, parallel);
			if (equalSizeAndOffset && dst_bit_depth_luma == colour_mapping->luma_bit_depth_output && dst_bit_depth_chroma == colour_mapping->chroma_bit_depth_output && dst_chroma_format == src_chroma_format)
				dst_img = cm_img;
		}

		if (!dst_img) {
			dst_img = new image;
			dst_img->create(dst_sps->pic_width_luma, dst_sps->pic_height_luma, dst_chroma_format, dst_bit_depth_luma, dst_bit_depth_chroma);
			scale_image(region, cm_img, dst_img, parallel);
		}

		dst_img->layer_id			= dst_layer_id;
		dst_img->view_idx			= src_img->view_idx;
		dst_img->picture_order_cnt	= src_img->picture_order_cnt;
	}
	
	if (dst_layer->InterLayerMotionPredictionEnabled(src_layer_idx)) {
		if (!dst_img) {
			dst_img = new image;
			dst_img->create(dst_sps->pic_width_luma, dst_sps->pic_height_luma, dst_chroma_format, dst_bit_depth_luma, dst_bit_depth_chroma);
			dst_img->layer_id			= dst_layer_id;
			dst_img->view_idx			= src_img->view_idx;
			dst_img->picture_order_cnt	= src_img->picture_order_cnt;
		}

		auto	src_shdr	= src_img->slices[0];
		auto	dst_shdr	= dup(*src_shdr);
		dst_img->slices.push_back(dst_shdr);

		if (src_shdr->type != SLICE_TYPE_I) {
			if (equalSizeAndOffset) {
				dst_img->cu_info = src_img->cu_info;
				dst_img->pb_info = src_img->pb_info;
			} else {
				scale_mvs(region, src_img, dst_img, parallel);
			}
		}
	}
	if (!dst_img)
		dst_img = src_img;

	return dst_img;
}
#endif

//-----------------------------------------------------------------------------
// threading
//-----------------------------------------------------------------------------

typedef CABAC_decoder<memory_reader_0> cabac_reader;
static int _debug_layer = 1, _debug_poc = 0, _debug_chan = 0, _debug_ctu = 4 * 16 + 9;

void print_checksum(image* img) {
	if (_debug_ctu < 0) {
		for (int i = 0; i < img->ctu_info.total(); i++)
			print_checksum(img, _debug_chan, i, 64);
	} else if (img->layer_id == _debug_layer && img->picture_order_cnt == _debug_poc) {
		print_checksum(img, _debug_chan, _debug_ctu, 4);
	}
}

class thread_context {
	struct CoeffPos {
		int16	pos, val;
	};

	image*					img;
	slice_segment_header*	shdr;

	AVAIL				avail0						= AVAIL(0);

	cabac_reader		cabac;
	CM_table			model;

	// residual data
	bool				transquant_bypass;
	int16				coeff_scratch[32 * 32];
	int32				residual_luma[32 * 32]; // used for cross-comp-prediction
	uint8				StatCoeff[4];

	// quantization
	bool				need_cu_qp_delta			= false;
	bool				need_cu_qp_offset			= false;	//range
	int32x2				cu_qp_offset				= zero;		//range
	int					currentQPY;
	int32x3				QP;

	//intra history
	//edge_history2<IntraPredMode, 6, 2>	intra_pred_history;

	uint8				max_merge_candidates;
#ifdef HEVC_SCC
	dynamic_array<array<uint16, 3>>	predictor_palette;
#endif

#ifdef HEVC_3D
	DisparityDeriver	disparity_deriver;
	edge_history<bool, 6, 3>	ivs_history;
	bool				inter_dc_only_enabled		= false;
	bool				intra_contour_enabled		= false;
	bool				intra_dc_only_wedge_enabled	= false;
	bool				cqt_cu_part_pred_enabled	= false;
	bool				skip_intra_enabled			= false;

	bool				res_pred_enabled			= false;
	bool				dbbp_enabled				= false;
#endif

	bool	bit(CM_index i) {
		return cabac.bit(&model[i]);
	}
	int		read_merge_idx() {
		return	max_merge_candidates < 2 || !bit(CM_MERGE_IDX) ? 0
			:	cabac.count_ones(max_merge_candidates - 2) + 1;
	}
	int		read_ref_idx(int max) {
		return	max < 2 || !bit(CM_REF_IDX_LX + 0) ? 0
			:	max < 3 || !bit(CM_REF_IDX_LX + 1) ? 1
			:	cabac.count_ones(max - 3) + 2;
	}
	MotionVector read_mvd() {
		bool greater0[2] = {
			bit(CM_ABS_MVD_GREATER01),
			bit(CM_ABS_MVD_GREATER01),
		};
		bool greater1[2] = {
			greater0[0] && bit(CM_ABS_MVD_GREATER01 + 1),
			greater0[1] && bit(CM_ABS_MVD_GREATER01 + 1),
		};

		MotionVector	value = zero;
		for (int c = 0; c < 2; c++) {
			if (greater0[c]) {
				int	v	= greater1[c] ? cabac.EGk_bypass(1) + 2 : 1;
				value[c] = plus_minus(v, cabac.bypass());
			}
		}
		return value;
	}

#ifdef HEVC_3D
	int		read_depth_dc(int offset) {
		int	v = cabac.count_ones(3, &model[CM_DEPTH_DC_ABS]);
		if (v == 3)
			v += cabac.EGk_bypass(0);

		v -= offset;
		if (v)
			v = plus_minus(v, cabac.bypass());
		return v;
	}

	AVAIL	read_res_pred_illum(int x0, int y0, int log2CbSize, const PBMotionCoding &motion, AVAIL avail) {
		if (res_pred_enabled && bit(CM_IV_RES_PRED_WEIGHT_IDX + int(ivs_history.get(y0)))) {
			ivs_history.set(y0, log2CbSize);
			return (avail | AVAIL_RESPRED | (bit(CM_IV_RES_PRED_WEIGHT_IDX + 2) * AVAIL_RESPRED2)) - AVAIL_VSP;
		}

		ivs_history.clear(y0, log2CbSize);

		if (shdr->extension_3d.ic_enabled) {
			bool	icCuEnable = motion.is_merge()
				? (motion.merge_index > 0 || !shdr->extension_3d.ic_disabled_merge_zero_idx)
				: ((motion.refIdx[0] >= 0 && shdr->ref(0, motion.refIdx[0]).view_idx != img->view_idx)
				|| (motion.refIdx[1] >= 0 && shdr->ref(1, motion.refIdx[1]).view_idx != img->view_idx));

			if (icCuEnable && bit(CM_ILLU_COMP))
				return (avail | AVAIL_ILLU) - (AVAIL_IV | AVAIL_VSP);
		}
		return avail;
	}

#endif

	void	decode_transform_unit(int x0, int y0, int log2TrSize, int c, IntraPredMode intra_mode, range<const CoeffPos*> coeffs, int32 *residual, bool skip_transform, uint8 rdpcm_mode, int cross_scale, AVAIL avail);
	void	read_final_chroma_residuals(int x0, int y0, IntraPredMode intra_mode, uint8 cbf_chroma, AVAIL avail);

	void	read_cu_qp(int x0, int y0, int log2CbSize, bool cbf_chroma, bool tu_residual_act);
	int		read_residual(CoeffPos *coeffs, int log2TrSize, IntraPredMode intra_mode, int c, bool &skip_transform, uint8 &rdpcm_mode);
	void	read_transform_tree(int x0, int y0, int log2CbSize, int tr_depth, int max_tr_depth, IntraPredMode intra_luma, IntraPredMode intra_chroma, uint8 cbf_chroma, AVAIL avail);
	void	read_transform_tree_intra(int x0, int y0, int log2CbSize, int tr_depth, int max_tr_depth, IntraPredMode intra_luma, IntraPredMode intra_chroma, uint8 cbf_chroma ONLY_3D(uint16 wedge_index) ONLY_3D(int16x2 depth_offsets), AVAIL avail);
	PBMotionCoding	read_prediction_unit(bool merge, int ct_depth, bool allow_bi = true);

	void	read_cu_skip(int x0, int y0, int log2CbSize, AVAIL avail);
	void	read_cu_intra(int x0, int y0, int log2CbSize, bool can_split, AVAIL avail);
	void	read_cu_inter(int x0, int y0, int log2CbSize, int ct_depth, PartMode pred_part, AVAIL avail);

	void	read_coding_tree(int x0, int y0, int log2CbSize, AVAIL avail);
	void	read_sao(int xCtb, int yCtb);
	void	read_palette_coding(int x0, int y0, int log2CbSize);

public:
	enum DecodeResult {
		EndOfSliceSegment,
		EndOfSubstream,
		Error
	};

	edge_history2<IntraPredMode, 6, 2>	intra_pred_history;


	thread_context() : cabac(none) {}
	thread_context(image *img, slice_segment_header* shdr, const_memory_block data);
	DecodeResult	decode_substream(CM_table *wpp_models, Event *event, uint32 &ctb_addr, uint32 num_ctb);

	void	init_model(CM_table &&_model, int QPY) {
		currentQPY	= QPY;//shdr->get_qp();
		model		= move(_model);
	}
	void	init_model() {
		currentQPY	= shdr->get_qp();
		model.init(SliceType(shdr->type ^ (shdr->type != SLICE_TYPE_I && shdr->cabac_init)), currentQPY);
		clear(StatCoeff);
	}

	auto	get_model()			{ return move(model); }
	auto	get_QPY()	const	{ return currentQPY; }
};

thread_context::thread_context(image *img, slice_segment_header* shdr, const_memory_block data) : img(img), shdr(shdr), cabac(data) {
	clear(coeff_scratch);
	max_merge_candidates = 5 - shdr->reduce_merge_candidates;

	auto	pps = img->pps.get();

	avail0 |= AVAIL_CROSS_COMP * IF_HAS_RANGE(pps->extension_range.cross_component_prediction_enabled)
			| AVAIL_RESIDUAL_ACT * IF_HAS_SCC(pps->extension_scc.act_qp_offsets.exists());


#ifdef HEVC_ML
	auto	sps = pps->sps.get();
	auto	vps = sps->vps.get();

	if (auto vpsml = vps->extension_multilayer.exists_ptr()) {
		auto	layer			= vpsml->layer_by_id(img->layer_id);

	#ifdef HEVC_3D
		if (auto vps3d = vps->extension_3d.exists_ptr()) {
			auto&	iv	= shdr->extension_3d.iv;

			if (shdr->extension_3d.depth_layer) {
				intra_contour_enabled		= iv.intra_contour_enabled;
				intra_dc_only_wedge_enabled	= iv.intra_dc_only_wedge_enabled;
				cqt_cu_part_pred_enabled	= iv.cqt_cu_part_pred_enabled;
				inter_dc_only_enabled		= iv.inter_dc_only_enabled;
				skip_intra_enabled			= iv.skip_intra_enabled;
				avail0						|= iv.tex_mc_enabled * AVAIL_T | iv.di_mc_enabled * AVAIL_IV;

			} else {
				dbbp_enabled				= iv.dbbp_enabled;
				res_pred_enabled			= iv.res_pred_enabled;
				avail0						|= iv.di_mc_enabled * (AVAIL_DI | AVAIL_IV | AVAIL_IV_SUB | AVAIL_IV_SHIFT) | iv.vsp_mc_enabled * AVAIL_VSP;

				if (shdr->type != SLICE_TYPE_I && (iv.di_mc_enabled || iv.vsp_mc_enabled || dbbp_enabled || res_pred_enabled))
					disparity_deriver.init(shdr, img->picture_order_cnt, img->view_idx, iv.depth_ref_enabled ? (dbbp_enabled ? 2 : 1) : 0);
			}
		}

		max_merge_candidates	+= avail0 & (AVAIL_T | AVAIL_IV | AVAIL_VSP);

	#endif
	}
#endif
}

void thread_context::read_cu_qp(int x0, int y0, int log2CbSize, bool cbf_chroma, bool tu_residual_act) {
	if (need_cu_qp_delta || need_cu_qp_offset || tu_residual_act) {
		auto	pps		= img->pps.get();

		if (need_cu_qp_delta) {
			int	v	= 0;
			if (bit(CM_CU_QP_DELTA_ABS + 0)) {
				v	= cabac.count_ones(4, &model[CM_CU_QP_DELTA_ABS + 1]) + 1;
				if (v == 5)
					v += cabac.EGk_bypass(0);
				v	= plus_minus(v, cabac.bypass());
			}

			int	cb_mask			= -(1 << log2CbSize);
			currentQPY			= calc_QPY(currentQPY, v, img->planes[0].bit_depth);
			img->set_QPY(x0 & cb_mask, y0 & cb_mask, log2CbSize, currentQPY);
			need_cu_qp_delta	= false;
		}

	#ifdef HEVC_RANGE
		if (need_cu_qp_offset && cbf_chroma && !transquant_bypass) {
			if (bit(CM_CU_CHROMA_QP_OFFSET)) {
				auto&	offsets	= get(pps->extension_range.chroma_qp_offsets);
				cu_qp_offset	= offsets[offsets.size() > 1 && bit(CM_CU_CHROMA_QP_OFFSET_IDX)];
			}
			need_cu_qp_offset	= false;
		}
	#endif

	#ifdef HEVC_SCC
		int32x3	qp_offset	= tu_residual_act ? shdr->extension_scc.act_qp_offsets : concat(0, pps->qp_offset + shdr->qp_offset + cu_qp_offset);
	#else
		int32x3	qp_offset	= concat(0, pps->qp_offset + shdr->qp_offset + cu_qp_offset);
	#endif

		QP	= calc_QP(currentQPY, qp_offset, img->ChromaArrayType, img->planes[0].bit_depth, img->planes[1].bit_depth);
	}
}

int decode_last_significant_coeff_prefix(cabac_reader &decoder, int shift, int max, context_model* model) {
	for (int i = 0; i < max; i++) {
		if (!decoder.bit(&model[i >> shift]))
			return i;
	}
	return max;
}

int decode_last_significant_coeff(cabac_reader &decoder, int prefix) {
	if (prefix > 3) {
		int nbits = (prefix >> 1) - 1;
		return ((2 + (prefix & 1)) << nbits) + decoder.bypass(nbits);
	}
	return prefix;
}

int thread_context::read_residual(CoeffPos *coeffs, int log2TrSize, IntraPredMode intra_mode, int c, bool &skip_transform, uint8 &rdpcm_mode) {
	auto pps = img->pps.get();
	auto sps = pps->sps.get();

	bool	skip					= pps->transform_skip_enabled
		&& !transquant_bypass
		&& !IF_HAS_RANGE(log2TrSize > pps->extension_range.log2_max_transform_skip_block_size)
		&& bit(CM_TRANSFORM_SKIP + !!c);

	skip_transform = skip;

#ifdef HEVC_RANGE
	if (intra_mode == INTRA_UNKNOWN
		&& sps->extension_range.explicit_rdpcm_enabled
		&& (skip || transquant_bypass)
		&& bit(CM_RDPCM + !!c)
	)
		rdpcm_mode = bit(CM_RDPCM_DIR + !!c) + 1;
	else
		rdpcm_mode = sps->extension_range.implicit_rdpcm_enabled && (transquant_bypass || skip) && (intra_mode == INTRA_ANGLE_24 || intra_mode == INTRA_ANGLE_8)
			? 1 + (intra_mode == INTRA_ANGLE_24) : 0;
#endif

	// --- position of last coded coefficient ---
	int max		= (log2TrSize << 1) - 1;
	int offset, shift;
	if (c == 0) {
		offset	= 3 * (log2TrSize - 2) + (log2TrSize > 4);
		shift	= log2TrSize > 2;
	} else {
		offset	= 15;
		shift	= log2TrSize - 2;
	}
	int		last_significant_coeff_x_prefix = decode_last_significant_coeff_prefix(cabac, shift, max, &model[CM_LAST_SIGNIFICANT_COEFFICIENT_X_PREFIX + offset]);
	int		last_significant_coeff_y_prefix = decode_last_significant_coeff_prefix(cabac, shift, max, &model[CM_LAST_SIGNIFICANT_COEFFICIENT_Y_PREFIX + offset]);
	int		last_significant_coeff_x		= decode_last_significant_coeff(cabac, last_significant_coeff_x_prefix);
	int		last_significant_coeff_y		= decode_last_significant_coeff(cabac, last_significant_coeff_y_prefix);

	// --- scanIdx ---
	auto	scanIdx			= get_intra_scan_idx(log2TrSize, intra_mode, c, img->ChromaArrayType);

	if (scanIdx == SCAN_VERT)
		swap(last_significant_coeff_x, last_significant_coeff_y);

	// --- find last sub block and last scan pos ---
	auto	lastScanP		= tables.get_scan_position(last_significant_coeff_x, last_significant_coeff_y, scanIdx, log2TrSize);
	int		lastSubBlock	= lastScanP >> 4;
	int		lastScanPos		= lastScanP & 15;

	auto	log2sub			= log2TrSize - 2;
	auto	scan_sub		= tables.get_scan_order(log2sub, scanIdx);
	auto	scan_pos		= tables.get_scan_order(2, scanIdx);

	uint8	coded_sub_block_neighbors[64];
	memset(coded_sub_block_neighbors, 0, 1 << (log2sub * 2));

#ifdef HEVC_RANGE
	uint8	*stat_coeff		= sps->extension_range.persistent_rice_adaptation_enabled ? StatCoeff + (c == 0 ? 2 : 0) + (skip || transquant_bypass) : nullptr;
#endif

	int		nCoeff			= 0;
	int		c1				= 1;
	for (int i = lastSubBlock; i >= 0; i--) {
		auto S			= scan_sub[i];
		bool infer_dc	= i > 0 && i < lastSubBlock;	// first and last sub-block are always coded
		if (infer_dc && !bit(CM_CODED_SUB_BLOCK + (coded_sub_block_neighbors[S] ? 1 : 0) + (c ? 2 : 0)))
			continue;

		if (S & bits(log2sub))
			coded_sub_block_neighbors[S - 1] |= 1;
		if (S >> log2sub)
			coded_sub_block_neighbors[S - (1 << log2sub)] |= 2;


		// ----- find significant coefficients in this sub-block -----
		static_array<int8, 16>	pos;

		if (i == lastSubBlock)
			pos.push_back(lastScanPos);

		int		prev_coded	= coded_sub_block_neighbors[S];
		uint8	*idx_map	= tables.idx_maps[log2sub][!!c][scanIdx != SCAN_DIAG][prev_coded] + (S << 4);
		int		last_coeff	= i == lastSubBlock ? lastScanPos : 16;
		int		skip_ctx	= IF_HAS_RANGE(sps->extension_range.transform_skip_context_enabled && (transquant_bypass || skip)) ? 42 + !!c : 0;

		// --- AC coefficients' significance ---
		for (int n = last_coeff; --n > 0;) {
			if (bit(CM_SIGNIFICANT_COEFF + (skip_ctx ? skip_ctx : idx_map[scan_pos[n]]))) {
				pos.push_back(n);
				infer_dc	= false;	// since we have a coefficient in the sub-block, we cannot infer the DC coefficient anymore
			}
		}

		// --- DC coefficient significance ---
		if (last_coeff) {
			if (infer_dc || bit(CM_SIGNIFICANT_COEFF + (skip_ctx ? skip_ctx : idx_map[0])))
				pos.push_back(0);
		}

		if (int num_sig = pos.size()) {
			// --- greater1 ---
			int		ctxSet			= (c ? 4 : i ? 2 : 0) + (c1 == 0);
			auto	m				= &model[CM_COEFF_ABS_LEVEL_GREATER1 + ctxSet * 4];
			uint8	greater1		= 0;
			int		num_greater1	= min(8, num_sig);

			c1 = 1;
			for (int n = 0; n < num_greater1; n++) {
				bool	gt1	= cabac.bit(m + c1);
				greater1	= (greater1 << 1) | gt1;

				if (c1) {
					if (gt1)
						c1	= 0;
					else if (c1 < 3)
						++c1;
				}
			}
			// --- greater2 ---
			bool	greater2		= greater1 && bit(CM_COEFF_ABS_LEVEL_GREATER2 + ctxSet);

			// --- signs ---
			bool	signHidden		= pps->sign_data_hiding && !transquant_bypass && !IF_HAS_RANGE(rdpcm_mode) && pos[0] - pos[num_sig - 1] > 3;
			uint16	signs			= cabac.bypass(num_sig - signHidden) << 16 - (num_sig - signHidden);

			// --- values ---
		#ifdef HEVC_RANGE
			int		uiGoRiceParam	= stat_coeff ? *stat_coeff / 4 : 0;
			bool	first_remaining	= true;
		#else
			int		uiGoRiceParam	= 0;
		#endif

			int		sum_abs			= 0;
			uint32	remaining		= (greater1 << (16 - num_greater1)) | 0xff;
			int		S2				= ((S & bits(log2sub)) << 2) + ((S & ~bits(log2sub)) << 4);

			for (int n = 0; n < num_sig; n++, signs <<= 1, remaining <<= 1) {
				int		value		= 1;
				bool	has_remaining = !!(remaining & 0x8000);

				if (has_remaining && n < 8) {
					++value;
					if (remaining < 0x10000) {
						if (has_remaining = greater2)
							++value;
					}
				}

				if (has_remaining) {
					int	prefix			= cabac.count_ones(64);
					int abs_remaining	= prefix <= 3
						? (prefix << uiGoRiceParam) + cabac.bypass(uiGoRiceParam)								// when code only TR part (level < TRMax)
						: ((bits(prefix - 3) + 3) << uiGoRiceParam) + cabac.bypass(prefix - 3 + uiGoRiceParam);	// Suffix coded with EGk. Note that the unary part of EGk is already included in the 'prefix' counter above

					value	+= abs_remaining;
					if (value > 3 << uiGoRiceParam && (IF_HAS_RANGE(stat_coeff) || uiGoRiceParam < 4))
						++uiGoRiceParam;

				#ifdef HEVC_RANGE
					// persistent_rice_adaptation_enabled
					if (stat_coeff && first_remaining) {
						if (abs_remaining >= 3 << *stat_coeff / 4)
							++*stat_coeff;
						else if (2 * abs_remaining < 1 << *stat_coeff / 4 && *stat_coeff > 0)
							--*stat_coeff;
					}
					first_remaining = false;
				#endif
				}

				sum_abs += value;

				if ((signHidden && n == num_sig - 1 && (sum_abs & 1)) || (signs & 0x8000))
					value = -value;

				// put coefficient in list
				auto	p = scan_pos[pos[n]];
				coeffs[nCoeff].pos = S2 + (p & 3) + ((p >> 2) << log2TrSize);
				coeffs[nCoeff].val = value;
				++nCoeff;
			}

		}
	}
	ISO_ASSERT(nCoeff);
	return nCoeff;
}

#ifdef HEVC_SCC

int decode_palatte_run_prefix(cabac_reader &decoder, context_model* model, int ctx0, int ctx1, int max) {
	for (int i = 0; i < max; i++) {
		bool	bit = i < 5 ? decoder.bit(&model[i == 0 ? ctx0 : (ctx1 + i - 1) >> 1]) : decoder.bypass();
		if (!bit)
			return i;
	}
	return max;
}

//8.4.4.2.7 Decoding process for palette mode
template<typename T> void decode_palette_mode(block<T, 2> dst, int shiftw, int shifth, int bitdepth, int Q, bool transquant_bypass, bool transpose, int escape_index, blockptr<uint16,2> indices, blockptr<uint16,2> escape_vals, range<uint16*> palette) {
	auto	w = dst.template size<1>();
	auto	h = dst.template size<2>();
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int	xL = transpose ? y << shifth : x << shiftw;
			int	yL = transpose ? x << shiftw : y << shifth;

			int	index = indices[yL][xL];

			if (index != escape_index) {
				dst[y][x] = palette[index];
			} else {
				int	val = escape_vals[yL][xL];
				if (!transquant_bypass)
					val =  clamp((val * QuantFactor(Q) + 32) >> 6, 0, bits(bitdepth));
				dst[y][x] = val;
			}
		}
	}
}

//7.3.8.13 Palette Syntax
void thread_context::read_palette_coding(int x0, int y0, int log2CbSize) {
	auto	pps					= img->pps.get();
	auto	sps					= pps->sps.get();
	int		palette_max_size	= sps->extension_scc.palette_max_size;
	int		max_predictor		= palette_max_size + sps->extension_scc.delta_palette_max_predictor_size;

	int		numComps			= num_channels(img->chroma_format);

	bool	reuse[1024];
	int		index_idc[1024];

	clear(reuse);
	clear(index_idc);

	int		num_predicted = 0;
	for (int i = 0; i < predictor_palette.size() && num_predicted < palette_max_size; i++) {
		int	run = cabac.EGk_bypass(0);
		if (run != 1) {
			if (run > 1)
				i += run - 1;
			reuse[i] = true;
			num_predicted++;
		} else {
			break;
		}
	}

	dynamic_array<array<uint16, 3>>		new_palette_entries(num_predicted < palette_max_size ? cabac.EGk_bypass(0) : 0);
	for (int c = 0; c < numComps; c++ ) {
		int	nbits = img->planes[c].bit_depth;
		for (auto &i : new_palette_entries)
			i[c] = cabac.bypass(nbits);
	}

	int		palette_size		= num_predicted + new_palette_entries.size32();
	bool	has_escape			= palette_size && cabac.bypass();
	int		max_index			= palette_size - 1 + has_escape;

	bool	copy_above_final	= false;
	bool	transpose			= false;
	int		num_indices			= 0;

	if (max_index > 0) {
		int		prefix	= cabac.count_ones(4);
		int		rice	= ((max_index + 1) >> 3) + 3;
		num_indices		= (prefix < 4 ? prefix : cabac.bypass(rice) + (prefix << rice)) + 1;

		for (int i = 0; i < num_indices; i++) {
			int		r		= max_index - (i > 0);
			index_idc[i]	= r > 0 ? cabac.bypass(log2_ceil(r)) : 0;
		}
		copy_above_final	= bit(CM_PALETTE_COPY_ABOVE);
		transpose			= bit(CM_PALETTE_TRANSPOSE);
	}

	if (has_escape)
		read_cu_qp(x0, y0, log2CbSize, true, false);

	auto	scan	= tables.get_scan_order(log2CbSize, SCAN_TRAVERSE);
	int		nC		= 1 << log2CbSize;
	int		total	= nC << log2CbSize;

	bool	copy_above[64 * 64];
	uint16	indices[64 * 64];

	for (int i = 0, s = 0, remaining = num_indices; i < total;) {
		int	run		= total - i;
		int	sprev	= s;

		s			= scan[i];
		copy_above[s] = max_index > 0 && i >= nC && !copy_above[sprev] && remaining > 0 && i < total - 1 && bit(CM_PALETTE_COPY_ABOVE);

		int		curr = copy_above[s] ? 0 : index_idc[num_indices - remaining];

		if (max_index > 0) {
			if (!copy_above[s])
				--remaining;

			if (remaining > 0 || copy_above[s] != copy_above_final) {
				int	max_run_minus1 = total - i - 1 - remaining - copy_above_final;
				if (max_run_minus1 > 0) {
					int		max		= log2_floor(max_run_minus1) + 1;
					int		prefix	= copy_above[s]
						? decode_palatte_run_prefix(cabac, &model[CM_PALETTE_TRANSPOSE_RUN_PREFIX], 0, 6, max)
						: decode_palatte_run_prefix(cabac, &model[CM_PALETTE_TRANSPOSE_RUN_PREFIX], index_idc[i] < 1 ? 0 : index_idc[i] < 3 ? 1 : 2, 3, max);

					run = 1 << (prefix - 1);
					if (prefix > 1 && max_run_minus1 != run)
						run += cabac.bypass(run * 2 > max_run_minus1 ? log2_ceil(max_run_minus1 - run) : prefix - 1);

					++run;
				}
			}
		}
		while (run--) {
			int		sR	= scan[i++];
			indices[sR] = copy_above[sR] == copy_above[s] ? indices[sR - nC] : curr;
		}
	}

	uint16	escape_vals[3][64 * 64];

	if (has_escape) {
		auto	shiftw = get_shift_W(img->chroma_format);
		auto	shifth = get_shift_H(img->chroma_format);
		if (transpose)
			swap(shiftw, shifth);

		for (int c = 0; c < numComps; c++) {
			uint32	mask	= c == 0 ? 0 : (shiftw ? 1 : 0) | (shifth ? nC : 0);
			for (int i = 0; i < total; i++ ) {
				int	s = scan[i];
				if (indices[s] == max_index && !(s & mask)) {
					escape_vals[c][s] = transquant_bypass
						? cabac.bypass(img->planes[!!c].bit_depth)
						: cabac.EGk_bypass(3);
				}
			}
		}
	}

	dynamic_array<uint16>	palette[3] = {palette_size, palette_size, palette_size};
	num_predicted = 0;
	for (int i = 0; i < predictor_palette.size(); i++) {
		if (reuse[i]) {
			for (int c = 0; c < numComps; c++ )
				palette[c][num_predicted] = predictor_palette[i][c];
			num_predicted++;
		}
	}

	for (int c = 0; c < numComps; c++)
		for (int i = 0; i < new_palette_entries.size(); i++)
			palette[c][num_predicted + i] = new_palette_entries[c][i];


	int		escape_index	= has_escape ? max_index : -1;

	if (img->planes[0].bit_depth <= 8)
		decode_palette_mode(img->get_plane_block<uint8 >(0, x0, y0, nC, nC), 0, 0, img->planes[0].bit_depth, QP[0], transquant_bypass, transpose, escape_index, make_blockptr(indices, nC), make_blockptr(escape_vals[0], nC), palette[0]);
	else
		decode_palette_mode(img->get_plane_block<uint16>(0, x0, y0, nC, nC), 0, 0, img->planes[0].bit_depth, QP[0], transquant_bypass, transpose, escape_index, make_blockptr(indices, nC), make_blockptr(escape_vals[0], nC), palette[0]);

	auto	shiftw		= get_shift_W(img->chroma_format);
	auto	shifth		= get_shift_H(img->chroma_format);
	int		w			= nC >> shiftw;
	int		h			= nC >> shifth;
	if (img->planes[1].bit_depth  <= 8) {
		decode_palette_mode(img->get_plane_block<uint8 >(1, x0, y0, w, h), 0, 0, img->planes[1].bit_depth, QP[1], transquant_bypass, transpose, escape_index, make_blockptr(indices, nC), make_blockptr(escape_vals[1], nC), palette[1]);
		decode_palette_mode(img->get_plane_block<uint8 >(1, x0, y0, w, h), 0, 0, img->planes[2].bit_depth, QP[2], transquant_bypass, transpose, escape_index, make_blockptr(indices, nC), make_blockptr(escape_vals[2], nC), palette[2]);
	} else {
		decode_palette_mode(img->get_plane_block<uint16>(1, x0, y0, w, h), 0, 0, img->planes[1].bit_depth, QP[1], transquant_bypass, transpose, escape_index, make_blockptr(indices, nC), make_blockptr(escape_vals[1], nC), palette[1]);
		decode_palette_mode(img->get_plane_block<uint16>(1, x0, y0, w, h), 0, 0, img->planes[2].bit_depth, QP[2], transquant_bypass, transpose, escape_index, make_blockptr(indices, nC), make_blockptr(escape_vals[2], nC), palette[2]);
	}

	dynamic_array<array<uint16, 3>>	new_predictor_palette(max_predictor);

	for (int i = 0; i < palette_size; i++)
		for (int c = 0; c < numComps; c++)
			new_predictor_palette[i][c] = palette[c][i];

	for (int i = 0; i < predictor_palette.size() && palette_size < max_predictor; i++) {
		if (!reuse[i])
			new_predictor_palette[palette_size++] = predictor_palette[i];
	}

	predictor_palette	= move(new_predictor_palette);
}

#endif

void thread_context::decode_transform_unit(int x0, int y0, int log2TrSize, int c, IntraPredMode intra_pred, range<const CoeffPos*> coeffs, int32 *residual, bool skip_transform, uint8 rdpcm_mode, int cross_scale, AVAIL avail) {
	auto	pps			= img->pps.get();
	auto	sps			= pps->sps.get();

	if (is_per_tu(intra_pred))
		decode_intra_prediction(img, x0, y0, intra_pred, log2TrSize, c, !transquant_bypass || !IF_HAS_RANGE(sps->extension_range.implicit_rdpcm_enabled), avail);

	const bool	is_intra	= intra_pred != INTRA_UNKNOWN;
	const int	bit_depth	= img->planes[c].bit_depth;

	bool	has_coeffs	= !coeffs.empty();
	if (has_coeffs) {
		const int	xor_pos	= IF_HAS_RANGE(sps->extension_range.transform_skip_rotation_enabled && is_intra && log2TrSize == 2) ? bits(log2TrSize * 2) : 0;

		if (transquant_bypass) {
			for (auto &i : coeffs)
				coeff_scratch[i.pos ^ xor_pos] = i.val;

			transform_bypass(residual, log2TrSize, rdpcm_mode, coeff_scratch);

		} else {
			// (8.6.3)
			const int fact			= QuantFactor(QP[c]);
			const int coeff_bits	= IF_HAS_RANGE(sps->extension_range.extended_precision_processing) ? max(bit_depth + 6, 15) : 15;
			const int coeff_min		= -(1 << coeff_bits);
			const int coeff_max		= (1 << coeff_bits) - 1;

			// --- inverse quantization ---
			if (sps->scaling_list.exists()) {
				auto	sclist		= pps->scaling_list.get_matrix(log2TrSize, c + (is_intra ? 0 : 3));
				const int shift		= bit_depth + log2TrSize - 5;
				const int offset	= 1 << (shift - 1);
				for (auto &i : coeffs)
					coeff_scratch[i.pos ^ xor_pos]	= clamp((i.val * int64(sclist[i.pos] * fact) + offset) >> shift, coeff_min, coeff_max);

			} else {
				const int shift		= bit_depth + log2TrSize - 9;
				const int offset	= 1 << (shift - 1);
				for (auto &i : coeffs)
					coeff_scratch[i.pos ^ xor_pos] = clamp((i.val * fact + offset) >> shift, coeff_min, coeff_max);
			}

			if (skip_transform) {
				transform_skip(residual, log2TrSize, rdpcm_mode, coeff_scratch, bit_depth, coeff_bits);

			} else {
				bool	idst	= is_intra && c == 0 && log2TrSize ==2;
				if (false && cross_scale == 0) {
					//transform and add
					if (bit_depth <= 8)
						transform_add(img->get_plane_ptr<uint8 >(c).slice(x0, y0), log2TrSize, idst, coeff_scratch, 8, coeff_bits);
					else
						transform_add(img->get_plane_ptr<uint16>(c).slice(x0, y0), log2TrSize, idst, coeff_scratch, bit_depth, coeff_bits);
					has_coeffs	= false;	//prevent re-add

				} else {
					transform(residual, log2TrSize, idst, coeff_scratch, bit_depth, coeff_bits);
				}
			}
		}
		// zero out scratch buffer again
		for (auto &i : coeffs)
			coeff_scratch[i.pos ^ xor_pos] = 0;
	}

	if (cross_scale) {
		auto	bit_depth_luma = sps->bit_depth_luma;
		for (int i = 0, n = 1 << (log2TrSize * 2); i < n; i++)
			residual[i] += (cross_scale * ((residual_luma[i] << bit_depth) >> bit_depth_luma)) >> 3;
	}

	if (has_coeffs || cross_scale) {
		// --- cross-component-prediction when CBF==0 ---
		const int	nT		= 1 << log2TrSize;
		if (bit_depth <= 8)
			add_residual(img->get_plane_ptr<uint8 >(c).slice(x0, y0), residual, nT, 8);
		else
			add_residual(img->get_plane_ptr<uint16>(c).slice(x0, y0), residual, nT, bit_depth);
	}
}

int	read_cbf_chroma(cabac_reader &cabac, context_model* model, int prev_cbf_chroma, bool bottom) {
	// bit 0: cb
	// bit 1: cr
	// bit 2: cb in bottom block of 4:2:2 mode
	// bit 3: cr in bottom block of 4:2:2 mode

	int		cbf_chroma	= 0;

	if (prev_cbf_chroma & 5) {
		cbf_chroma |= cabac.bit(model);
		if (bottom)
			cbf_chroma |= cabac.bit(model) << 2;
	}

	if (prev_cbf_chroma & 10) {
		cbf_chroma |= cabac.bit(model) << 1;
		if (bottom)
			cbf_chroma |= cabac.bit(model) << 2;
	}
	return cbf_chroma;
}

int	read_cross_comp_pred(cabac_reader &decoder, context_model* model, context_model* model_sign) {
	int value = 0;
	while (value < 4 && decoder.bit(model + value))
		value++;
	return value ? plus_minus((1 << (value - 1)), decoder.bit(model_sign)) : 0;
}

void thread_context::read_final_chroma_residuals(int x0, int y0, IntraPredMode intra_mode, uint8 cbf_chroma, AVAIL avail) {
	CoeffPos			coeffs[32 * 32];
	int32				residual[32 * 32];
	bool				skip_transform	= false;
	uint8				rdpcm_mode	= 0;
	int					ncoeffs;

	int		xc	= x0 >> get_shift_W(img->chroma_format);
	int		yc	= y0 >> get_shift_H(img->chroma_format);

	ncoeffs = cbf_chroma & 1 ? read_residual(coeffs, 2, intra_mode, 1, skip_transform, rdpcm_mode) : 0;
	decode_transform_unit(xc, yc, 2, 1, intra_mode, make_range_n(coeffs, ncoeffs), residual, skip_transform, rdpcm_mode, 0, avail);

	if (img->chroma_format == CHROMA_422) {
		ncoeffs = cbf_chroma & 4 ? read_residual(coeffs, 2, intra_mode, 1, skip_transform, rdpcm_mode) : 0;
		decode_transform_unit(xc, yc + 4, 2, 1, intra_mode, make_range_n(coeffs, ncoeffs), residual, skip_transform, rdpcm_mode, 0, avail);
	}

	ncoeffs = cbf_chroma & 2 ? read_residual(coeffs, 2, intra_mode, 2, skip_transform, rdpcm_mode) : 0;
	decode_transform_unit(xc, yc, 2, 2, intra_mode, make_range_n(coeffs, ncoeffs), residual, skip_transform, rdpcm_mode, 0, avail);

	if (img->chroma_format == CHROMA_422) {
		ncoeffs = cbf_chroma & 8 ? read_residual(coeffs, 2, intra_mode, 2, skip_transform, rdpcm_mode) : 0;
		decode_transform_unit(xc, yc + 4, 2, 2, intra_mode, make_range_n(coeffs, ncoeffs), residual, skip_transform, rdpcm_mode, 0, avail);
	}
}

void thread_context::read_transform_tree(int x0, int y0, int log2CbSize, int tr_depth, int max_tr_depth, IntraPredMode intra_luma, IntraPredMode intra_chroma, uint8 cbf_chroma, AVAIL avail) {
	auto	pps				= img->pps.get();
	auto	sps				= pps->sps.get();
	int		log2TrSize		= log2CbSize - tr_depth;
	auto	ChromaArrayType	= img->ChromaArrayType;

	bool	split_transform	= log2TrSize > sps->log2_max_tu()
		|| (log2TrSize > sps->log2_min_transform_block_size && tr_depth < max_tr_depth && bit(CM_SPLIT_TRANSFORM + (5 - log2TrSize)));

	if (cbf_chroma && (log2TrSize > 2 || ChromaArrayType == CHROMA_444))
		cbf_chroma = read_cbf_chroma(cabac, &model[CM_CBF_CHROMA + tr_depth], cbf_chroma, ChromaArrayType == CHROMA_422 && (!split_transform || log2TrSize == 3));

	if (split_transform) {
		img->set_split_transform(x0, y0, tr_depth);

		int x1 = x0 + (1 << (log2TrSize - 1));
		int y1 = y0 + (1 << (log2TrSize - 1));
		read_transform_tree(x0,y0, log2CbSize, tr_depth + 1, max_tr_depth, intra_luma, intra_chroma, cbf_chroma, avail_quad0(avail));
		read_transform_tree(x1,y0, log2CbSize, tr_depth + 1, max_tr_depth, intra_luma, intra_chroma, cbf_chroma, avail_quad1(avail));
		read_transform_tree(x0,y1, log2CbSize, tr_depth + 1, max_tr_depth, intra_luma, intra_chroma, cbf_chroma, avail_quad2(avail));
		read_transform_tree(x1,y1, log2CbSize, tr_depth + 1, max_tr_depth, intra_luma, intra_chroma, cbf_chroma, avail_quad3(avail));

		if (log2TrSize == 3 && ChromaArrayType != CHROMA_444 && ChromaArrayType != CHROMA_MONO)
			read_final_chroma_residuals(x0, y0, intra_chroma, cbf_chroma, avail);

	} else {
		bool cbf_luma = (intra_luma == INTRA_UNKNOWN && tr_depth == 0 && !cbf_chroma) || bit(CM_CBF_LUMA + !tr_depth);//pred_mode != MODE_INTRA

		if (cbf_luma)
			img->set_nonzero_coefficient(x0, y0, log2TrSize);

		if (cbf_luma || cbf_chroma) {
			bool	residual_act = IF_HAS_SCC((avail & AVAIL_RESIDUAL_ACT) && bit(CM_TU_TU_RESIDUAL_ACT));
			read_cu_qp(x0, y0, log2CbSize, cbf_chroma, residual_act);
		}

		// --- luma ---
		CoeffPos	coeffs[32 * 32];
		//int32		residual_luma[32 * 32];
		bool		skip_transform	= false;
		uint8		rdpcm_mode		= 0;
		int			ncoeffs			= cbf_luma ? read_residual(coeffs, log2TrSize, intra_luma, 0, skip_transform, rdpcm_mode) : 0;
		decode_transform_unit(x0, y0, log2TrSize, 0, intra_luma, make_range_n(coeffs, ncoeffs), residual_luma, skip_transform, rdpcm_mode, 0, avail);

		// --- chroma ---
		if (ChromaArrayType != CHROMA_MONO) {
			const int	log2TrSizeC	= ChromaArrayType == CHROMA_444 ? log2TrSize : log2TrSize - 1;
			if (log2TrSizeC >= 2) {
				const int	nTC			= 1 << log2TrSizeC;
				//const bool	do_cross	= IF_HAS_RANGE(pps->extension_range.cross_component_prediction_enabled && cbf_luma && (intra_luma == INTRA_UNKNOWN || intra_chroma == INTRA_CHROMA_LIKE_LUMA));
				const bool	do_cross	= cbf_luma && (avail & AVAIL_CROSS_COMP) && (intra_luma == intra_chroma);
				const int	shiftw		= get_shift_W(img->chroma_format);
				const int	shifth		= get_shift_H(img->chroma_format);
				const int	xc			= x0 >> shiftw, yc = y0 >> shifth;
				int			cross_scale	= 0;
				int32		residual[32 * 32];

				if (do_cross)
					cross_scale = read_cross_comp_pred(cabac, &model[CM_LOG2_RES_SCALE_ABS_PLUS1], &model[CM_RES_SCALE_SIGN]);

				ncoeffs = cbf_chroma & 1 ? read_residual(coeffs, log2TrSizeC, intra_chroma, 1, skip_transform, rdpcm_mode) : 0;
				decode_transform_unit(xc, yc, log2TrSizeC, 1, intra_chroma, make_range_n(coeffs, ncoeffs), residual, skip_transform, rdpcm_mode, cross_scale, avail);

				if (ChromaArrayType == CHROMA_422) {
					ncoeffs = cbf_chroma & 4 ? read_residual(coeffs, log2TrSizeC, intra_chroma, 1, skip_transform, rdpcm_mode) : 0;
					decode_transform_unit(xc, yc + nTC, log2TrSizeC, 1, intra_chroma, make_range_n(coeffs, ncoeffs), residual, skip_transform, rdpcm_mode, cross_scale, avail);
				}

				if (do_cross)
					cross_scale = read_cross_comp_pred(cabac, &model[CM_LOG2_RES_SCALE_ABS_PLUS1 + 4], &model[CM_RES_SCALE_SIGN + 1]);

				ncoeffs = cbf_chroma & 2 ? read_residual(coeffs, log2TrSizeC, intra_chroma, 2, skip_transform, rdpcm_mode) : 0;
				decode_transform_unit(xc, yc, log2TrSizeC, 2, intra_chroma, make_range_n(coeffs, ncoeffs), residual, skip_transform, rdpcm_mode, cross_scale, avail);

				if (ChromaArrayType == CHROMA_422) {
					ncoeffs = cbf_chroma & 8 ? read_residual(coeffs, log2TrSizeC, intra_chroma, 2, skip_transform, rdpcm_mode) : 0;
					decode_transform_unit(xc, yc + nTC, log2TrSizeC, 2, intra_chroma, make_range_n(coeffs, ncoeffs), residual, skip_transform, rdpcm_mode, cross_scale, avail);
				}
			}
		}
	}
}

//-------------------------------------------
//	SKIP
//-------------------------------------------

void thread_context::read_cu_skip(int x0, int y0, int log2CbSize, AVAIL avail) {
	PBMotionCoding	motion;
	motion.merge_index	= read_merge_idx();

	img->set_pred_mode(x0, y0, log2CbSize, MODE_SKIP);
	intra_pred_history.set(x0, y0, log2CbSize, INTRA_DC);

#ifdef HEVC_3D
	Disparity	dv	= disparity_deriver.get(img, shdr, x0, y0, log2CbSize, true, avail);
	avail			= read_res_pred_illum(x0, y0, log2CbSize, motion, avail);
#endif
	
	PB_info	merge_candidates[6];
	int		nC	= 1 << log2CbSize;

	get_merge_candidate_list(shdr, img, x0, y0, nC, nC, avail, merge_candidates, motion.merge_index ONLY_3D(dv));
	if (!decode_inter_prediction1(shdr, img, merge_candidates[motion.merge_index], x0, y0, nC, nC, avail ONLY_3D(dv)))
		img->integrity = image::DECODING_ERRORS;
}

//-------------------------------------------
//	INTRA
//-------------------------------------------

template<typename T> void read_pcm_samples_internal(bitreader& r, block<T, 2> b, int pcm, int bit_depth) {
	int shift	= max(bit_depth - pcm, 0);
	for (auto y : b) {
		for (auto &x : y)
			x = r.get(pcm) << shift;
	}
}

void read_pcm_samples(bitreader& r, image *img, int x0, int y0, int log2CbSize) {
	auto&	pcm	= get(img->pps->sps->pcm);
	int		w	= 1 << log2CbSize, h = w;

	int	bit_depth	= img->get_bit_depth(0);
	if (bit_depth > 8)
		read_pcm_samples_internal(r, img->get_plane_block<uint16>(0, x0, y0, w, h), pcm.sample_bit_depth_luma, bit_depth);
	else
		read_pcm_samples_internal(r, img->get_plane_block<uint8 >(0, x0, y0, w, h), pcm.sample_bit_depth_luma, bit_depth);

	if (img->ChromaArrayType != CHROMA_MONO) {
		w	>>= get_shift_W(img->chroma_format);
		h	>>= get_shift_H(img->chroma_format);
		x0	>>= get_shift_W(img->chroma_format);
		y0	>>= get_shift_H(img->chroma_format);

		bit_depth	= img->get_bit_depth(1);
		if (bit_depth > 8) {
			read_pcm_samples_internal(r, img->get_plane_block<uint16>(1, x0, y0, w, h), pcm.sample_bit_depth_chroma, bit_depth);
			read_pcm_samples_internal(r, img->get_plane_block<uint16>(2, x0, y0, w, h), pcm.sample_bit_depth_chroma, bit_depth);
		} else {
			read_pcm_samples_internal(r, img->get_plane_block<uint8 >(1, x0, y0, w, h), pcm.sample_bit_depth_chroma, bit_depth);
			read_pcm_samples_internal(r, img->get_plane_block<uint8 >(2, x0, y0, w, h), pcm.sample_bit_depth_chroma, bit_depth);
		}
	}
}

void thread_context::read_transform_tree_intra(int x0, int y0, int log2CbSize, int tr_depth, int max_tr_depth, IntraPredMode intra_luma, IntraPredMode intra_chroma, uint8 cbf_chroma ONLY_3D(uint16 wedge_index) ONLY_3D(int16x2 depth_offsets), AVAIL avail) {
	int		log2PbSize		= log2CbSize - tr_depth;
#ifdef HEVC_3D
	if (is_dim(intra_luma)) {
		decode_depth_intra_prediction(img, shdr->extension_3d.reftex, x0, y0,
			intra_luma == INTRA_WEDGE ? tables.get_wedge_pattern(log2PbSize, wedge_index) : nullptr,
			depth_offsets, img->pps->extension_3d.get_dlt(img->layer_id), log2PbSize, avail
		);
	}
#endif
	read_transform_tree(x0, y0, log2CbSize, tr_depth, max_tr_depth, intra_luma, intra_chroma, cbf_chroma, avail);
}

void thread_context::read_cu_intra(int x0, int y0, int log2CbSize, bool can_split, AVAIL avail) {
	auto	pps = img->pps.get();
	auto	sps = pps->sps.get();

	int		cbf_chroma		= img->ChromaArrayType == CHROMA_MONO ? 0 : 3;
	int		nC				= 1 << log2CbSize;
	
	img->set_pred_mode(x0, y0, log2CbSize, MODE_INTRA);
	ivs_history.clear(y0, log2CbSize);

#ifdef HEVC_SCC
	if (sps->extension_scc.palette_mode && log2CbSize <= sps->log2_min_transform_block_size + sps->log2_diff_max_min_transform_block_size && bit(CM_PALETTE_MODE)) {
		read_palette_coding(x0, y0, log2CbSize);
		return;
	}
#endif
	
	bool	intra_split	= can_split && log2CbSize == sps->log2_min_luma_coding_block_size && !bit(CM_PART_MODE);
	if (intra_split)
		img->cu_info.get(x0, y0).part_mode = PART_NxN;

	if (!intra_split && sps->pcm.exists() && sps->pcm->valid_size(log2CbSize) && cabac.term_bit()) {
		img->set_pcm(x0, y0, log2CbSize);
		intra_pred_history.set(x0, y0, log2CbSize, INTRA_DC);

		bitreader r(cabac.get_stream().peek_block());
		read_pcm_samples(r, img, x0,y0, log2CbSize);
		r.align(8);
		r.restore_unused();
		cabac.init(r.get_stream());
		return;
	}

	int				num_parts		= intra_split ? 4 : 1;
	IntraPredMode	intra_luma[4]	= {INTRA_UNKNOWN, INTRA_UNKNOWN, INTRA_UNKNOWN, INTRA_UNKNOWN};
	IntraPredMode	intra_chroma[4];//	= {INTRA_CHROMA_LIKE_LUMA, INTRA_CHROMA_LIKE_LUMA, INTRA_CHROMA_LIKE_LUMA, INTRA_CHROMA_LIKE_LUMA};
	bool			prev_intra_luma_pred[4];
	int				log2PbSize		= log2CbSize - intra_split;

#ifdef HEVC_3D
	int				wedge_indices[4];
	int16x2			depth_offsets[4];
#endif

	for (int i = 0; i < num_parts; i++) {
	#ifdef HEVC_3D
		if ((intra_dc_only_wedge_enabled || intra_contour_enabled) && log2PbSize < 6 && !bit(CM_NO_DIM)) {
			bool	dim_idx	= intra_dc_only_wedge_enabled && intra_contour_enabled ? bit(CM_DEPTH_INTRA_MODE_IDX) : (!intra_dc_only_wedge_enabled || intra_contour_enabled);
			intra_luma[i]	= dim_idx ? INTRA_CONTOUR : INTRA_WEDGE;
			if (!dim_idx) {
				int	nbits = tables.get_wedge_pattern_bits(log2PbSize);
				wedge_indices[i] = reverse_bits(cabac.bypass(nbits), nbits);
			}

		} else
	#endif
		{
			prev_intra_luma_pred[i] = bit(CM_PREV_INTRA_LUMA_PRED);
		}
	}

	// --- find intra prediction modes ---

	for (int i = 0; i < num_parts; i++) {
		int		x = x0 + (i & 1 ? nC >> 1 : 0);
		int		y = y0 + (i & 2 ? nC >> 1 : 0);

		IntraPredMode	intra_pred = intra_luma[i];

		if (intra_pred == INTRA_UNKNOWN) {
			IntraPredMode	candidates[3];
			get_intrapred_candidates(candidates, intra_pred_history[0].get(y), intra_pred_history[1].get(x));

			intra_pred	= prev_intra_luma_pred[i]
						? candidates[cabac.count_ones(2)]
						: (IntraPredMode)nth_set_index(~(bit64(candidates[0]) | bit64(candidates[1]) | bit64(candidates[2])), cabac.bypass(5));
			intra_luma[i] = intra_pred;
		}
		intra_pred_history.set(x, y, log2PbSize, is_dim(intra_pred) ? INTRA_DC : intra_pred);
	}

	if (cbf_chroma) {
		// --- set chroma intra prediction mode ---
		bool	split_chroma		= intra_split && img->ChromaArrayType == CHROMA_444;

		for (int i = 0, n = split_chroma ? 4 : 1; i < n; i++) {
			auto chroma_mode	= bit(CM_INTRA_CHROMA_PRED_MODE) ? (IntraChromaMode)cabac.bypass(2) : INTRA_CHROMA_LIKE_LUMA;
			intra_chroma[i]		= map_chroma_pred_mode(chroma_mode, intra_luma[i], img->ChromaArrayType);
			if (chroma_mode != INTRA_CHROMA_LIKE_LUMA)
				avail -= AVAIL_RESIDUAL_ACT;
		}
		if (intra_split && !split_chroma)
			intra_chroma[1] = intra_chroma[2] = intra_chroma[3] = intra_chroma[0];
	}

#ifdef HEVC_3D
	// --- check for dc_only ---
	if (intra_dc_only_wedge_enabled && !intra_split && bit(CM_DC_ONLY)) {
		bool	dc	= bit(CM_DEPTH_DC_PRESENT);

		if (is_dim(intra_luma[0])) {
			int16x2		depth_offsets = dc ? int16x2{read_depth_dc(0), read_depth_dc(0)} : zero;
			decode_depth_intra_prediction(img, shdr->extension_3d.reftex, x0, y0,
				intra_luma[0] == INTRA_WEDGE ? tables.get_wedge_pattern(log2CbSize, wedge_indices[0]) : nullptr,
				depth_offsets, img->pps->extension_3d.get_dlt(img->layer_id), log2CbSize, avail
			);

		} else {
			decode_intra_prediction_recurse(img, x0, y0, intra_luma[0], log2CbSize, 0, !transquant_bypass || !IF_HAS_RANGE(sps->extension_range.implicit_rdpcm_enabled), avail);
			depth_offset_assignment(img, x0, y0, dc ? read_depth_dc(-1) : 0, img->pps->extension_3d.get_dlt(img->layer_id), log2CbSize);
		}
		return;
	}

	// --- read depth_offsets ---
	for (int i = 0; i < num_parts; i++) {
		if (is_dim(intra_luma[i]))
			depth_offsets[i] = {read_depth_dc(0), read_depth_dc(0)};
	}
#endif

	int max_tr_depth	= sps->max_transform_hierarchy_depth_intra + intra_split;

	if (intra_split) {
		// take care of first transform split here
		img->set_split_transform(x0, y0, 0);
		if (cbf_chroma)
			cbf_chroma = read_cbf_chroma(cabac, &model[CM_CBF_CHROMA], cbf_chroma, img->ChromaArrayType == CHROMA_422 && log2CbSize == 3);

		int		x1 = x0 + nC / 2;
		int		y1 = y0 + nC / 2;
		read_transform_tree_intra(x0, y0, log2CbSize, 1, max_tr_depth, intra_luma[0], intra_chroma[0], cbf_chroma ONLY_3D(wedge_indices[0]) ONLY_3D(depth_offsets[0]), avail_quad0(avail));
		read_transform_tree_intra(x1, y0, log2CbSize, 1, max_tr_depth, intra_luma[1], intra_chroma[1], cbf_chroma ONLY_3D(wedge_indices[1]) ONLY_3D(depth_offsets[1]), avail_quad1(avail));
		read_transform_tree_intra(x0, y1, log2CbSize, 1, max_tr_depth, intra_luma[2], intra_chroma[2], cbf_chroma ONLY_3D(wedge_indices[2]) ONLY_3D(depth_offsets[2]), avail_quad2(avail));
		read_transform_tree_intra(x1, y1, log2CbSize, 1, max_tr_depth, intra_luma[3], intra_chroma[3], cbf_chroma ONLY_3D(wedge_indices[3]) ONLY_3D(depth_offsets[3]), avail_quad3(avail));

		if (log2CbSize == 3 && img->ChromaArrayType != CHROMA_444 && img->ChromaArrayType != CHROMA_MONO)
			read_final_chroma_residuals(x0, y0, intra_chroma[0], cbf_chroma, avail);

	} else {
		read_transform_tree_intra(x0, y0, log2CbSize, 0, max_tr_depth, intra_luma[0], intra_chroma[0], cbf_chroma ONLY_3D(wedge_indices[0]) ONLY_3D(depth_offsets[0]), avail);
	}

}

//-------------------------------------------
//	INTER
//-------------------------------------------

PBMotionCoding thread_context::read_prediction_unit(bool merge, int ct_depth, bool allow_bi) {
	PBMotionCoding	motion;
	if (merge) {
		motion.merge_index	= read_merge_idx();

	} else {
		motion.merge_index	= -1;
		motion.refIdx[0]	= motion.refIdx[1]	= -1;

		auto enabled	= shdr->type != SLICE_TYPE_B		? 1
			: allow_bi && bit(CM_INTER_PRED_IDC + ct_depth)	? 3
			: (1 << bit(CM_INTER_PRED_IDC + 4));

		if (enabled & 1) {
			motion.refIdx[0]	= read_ref_idx(shdr->ref_list[0].size());
			motion.mvd[0]		= read_mvd();
			motion.mvp[0]		= bit(CM_MVP_LX);
		}

		if (enabled & 2) {
			motion.refIdx[1]	= read_ref_idx(shdr->ref_list[1].size());
			motion.mvd[1]		= shdr->mvd_l1_zero && (enabled & 1) ? zero : read_mvd();
			motion.mvp[1]		= bit(CM_MVP_LX);
		}
	}
	return motion;
}

void thread_context::read_cu_inter(int x0, int y0, int log2CbSize, int ct_depth, PartMode pred_part, AVAIL avail) {
	PB_info	merge_candidates[6];

	int		cbf_chroma	= img->ChromaArrayType == CHROMA_MONO ? 0 : 3;
	int		nC			= 1 << log2CbSize;
	auto	pps			= img->pps.get();
	auto	sps			= pps->sps.get();
	
	img->set_pred_mode(x0, y0, log2CbSize, MODE_INTER);
	intra_pred_history.set(x0, y0, log2CbSize, INTRA_DC);

	// --- PART_2Nx2N ---

	if (pred_part == PART_2Nx2N || bit(CM_PART_MODE)) {
		bool	merge0	= bit(CM_MERGE);
		auto	motion	= read_prediction_unit(merge0, ct_depth, true);

	#ifdef HEVC_3D
		Disparity	dv	= disparity_deriver.get(img, shdr, x0, y0, log2CbSize, merge0, avail);
		bool	dc_only	= inter_dc_only_enabled && bit(CM_DC_ONLY);
		avail			= read_res_pred_illum(x0, y0, log2CbSize, motion, avail);

		if (!decode_inter_prediction(shdr, img, motion, x0, y0, nC, nC, merge_candidates, false, avail, dv))
			img->integrity = image::DECODING_ERRORS;

		if (dc_only) {
			//depth_offset_assignment(img, x0, y0, read_depth_dc(-1), img->pps->extension_3d.get_dlt(img->layer_id), log2CbSize);
			depth_offset_assignment(img, x0, y0, read_depth_dc(-1), nullptr, log2CbSize);
			return;
		}
	#else
		if (!decode_inter_prediction(shdr, img, motion, x0, y0, nC, nC, merge_candidates, false, avail))
			img->integrity = image::DECODING_ERRORS;
	#endif

		// decode residual
		if (merge0 || bit(CM_RQT_ROOT_CBF))
			read_transform_tree(x0, y0, log2CbSize, 0, sps->max_transform_hierarchy_depth_inter, INTRA_UNKNOWN, INTRA_UNKNOWN, cbf_chroma, avail);
		return;
	}

	// --- not PART_2Nx2N ---

	PartMode	part_mode;

	if (pred_part != PART_UNKNOWN && pred_part != PART_NxN) {
		auto	hsplit	= !has_vsplit(pred_part);
		part_mode		= sps->amp_enabled && log2CbSize > sps->log2_min_luma_coding_block_size && !bit(CM_PART_MODE + 1)
			? (cabac.bypass()
				? (hsplit				? PART_2NxnD : PART_nRx2N)
				: (hsplit				? PART_2NxnU : PART_nLx2N)
				)
			: hsplit					? PART_2NxN : PART_Nx2N;

	} else {
		auto	hsplit	= bit(CM_PART_MODE + 1);
		part_mode = log2CbSize > sps->log2_min_luma_coding_block_size
			? (!sps->amp_enabled || bit(CM_PART_MODE + 3)
				? (hsplit				? PART_2NxN : PART_Nx2N)
				: cabac.bypass()
				? (hsplit				? PART_2NxnD : PART_nRx2N)
				: (hsplit				? PART_2NxnU : PART_nLx2N)
				)	: hsplit										? PART_2NxN
			: (log2CbSize == 3 || bit(CM_PART_MODE + 2))	? PART_Nx2N : PART_NxN;
	}

	img->cu_info.get(x0, y0).part_mode	= part_mode;

	bool	merge0	= bit(CM_MERGE);
	auto	motion0 = read_prediction_unit(merge0, ct_depth, log2CbSize > 3);
	bool	merge1	= bit(CM_MERGE);
	auto	motion1	= read_prediction_unit(merge1, ct_depth, log2CbSize > 3);

#ifdef HEVC_3D
	Disparity	dv	= disparity_deriver.get(img, shdr, x0, y0, log2CbSize, merge0, avail);
	ivs_history.clear(y0, log2CbSize);
#endif

	int		off		= get_offset(part_mode, nC);
	bool	ok		= false;

	if (part_mode == PART_NxN) {
		auto	motion2 = read_prediction_unit(bit(CM_MERGE), ct_depth, true);
		auto	motion3 = read_prediction_unit(bit(CM_MERGE), ct_depth, true);

		bool	shared_merge	= log2CbSize == 3 && pps->log2_parallel_merge_level > 2;
		if (shared_merge)
			get_merge_candidate_list(shdr, img, x0, y0, nC, nC, avail, merge_candidates, max(motion0.merge_index, motion1.merge_index, motion2.merge_index, motion3.merge_index) ONLY_3D(dv));

		ok	=	decode_inter_prediction(shdr, img, motion0,	x0,			y0,				off,		off,	merge_candidates, shared_merge, avail_quad0(avail) ONLY_3D(dv))
			&&	decode_inter_prediction(shdr, img, motion1,	x0 + off,	y0,				off,		off,	merge_candidates, shared_merge, avail_quad1(avail) ONLY_3D(dv))
			&&	decode_inter_prediction(shdr, img, motion2,	x0,			y0 + off,		off,		off,	merge_candidates, shared_merge, avail_quad2(avail) ONLY_3D(dv))
			&&	decode_inter_prediction(shdr, img, motion3,	x0 + off,	y0 + off,		off,		off,	merge_candidates, shared_merge, avail_quad3(avail) ONLY_3D(dv));

	} else {

		bool	shared_merge = log2CbSize == 3 && pps->log2_parallel_merge_level > 2;
		if (shared_merge)
			get_merge_candidate_list(shdr, img, x0, y0, nC, nC, avail, merge_candidates, max(motion0.merge_index, motion1.merge_index) ONLY_3D(dv));

		switch (part_mode) {
			case PART_2NxN:
			#ifdef HEVC_3D
				if (log2CbSize == 3) {
					avail = (avail - (AVAIL_T | AVAIL_IV | AVAIL_VSP | AVAIL_DI)) | AVAIL_NO_BIPRED;
				} else if (dbbp_enabled && bit(CM_DBBP)) {
					PB_info	vi[2];
					avail = (avail - (AVAIL_IV_SUB | AVAIL_VSP)) | AVAIL_NO_BIPRED;
					vi[0] = resolve_inter_prediction(shdr, img, motion0,	x0,		y0,				nC,		off,	merge_candidates, shared_merge, avail_top(avail), dv);
					vi[0].flags &= ~PB_info::VSP;
					img->set_mv_info(x0, y0,			nC, off, vi[0]);
					vi[1] = resolve_inter_prediction(shdr, img, motion1,	x0,		y0 + off,		nC,		off,	merge_candidates, shared_merge, avail_bot(avail, merge1), dv);
					vi[1].flags &= ~PB_info::VSP;
					img->set_mv_info(x0, y0 + off,		nC, off, vi[1]);
					ok	= decode_inter_prediction_dbbp(shdr, img, x0, y0, log2CbSize, dv, vi, false);
					break;
				}
			#endif
				//fallthrough
			case PART_2NxnU:
			case PART_2NxnD:
				ok	=	decode_inter_prediction(shdr, img, motion0,	x0,			y0,				nC,		off,		merge_candidates, shared_merge, avail_top(avail) ONLY_3D(dv))
					&&	decode_inter_prediction(shdr, img, motion1,	x0,			y0 + off,		nC,		nC - off,	merge_candidates, shared_merge, avail_bot(avail, merge1) ONLY_3D(dv));
				break;

			case PART_Nx2N:
			#ifdef HEVC_3D
				if (log2CbSize == 3) {
					avail = (avail - (AVAIL_T | AVAIL_IV | AVAIL_VSP | AVAIL_DI)) | AVAIL_NO_BIPRED;
				} else if (dbbp_enabled && bit(CM_DBBP)) {
					PB_info	vi[2];
					avail = (avail - (AVAIL_IV_SUB | AVAIL_VSP)) | AVAIL_NO_BIPRED;
					vi[0] = resolve_inter_prediction(shdr, img, motion0,	x0,			y0,			off,	nC,		merge_candidates, shared_merge, avail_left(avail), dv);
					vi[0].flags &= ~PB_info::VSP;
					img->set_mv_info(x0,		y0,		off,	nC, vi[0]);
					vi[1] = resolve_inter_prediction(shdr, img, motion1,	x0 + off,	y0,			off,	nC,		merge_candidates, shared_merge, avail_right(avail, merge1), dv);
					vi[1].flags &= ~PB_info::VSP;
					img->set_mv_info(x0 + off,	y0,		off,	nC, vi[1]);
					ok	= decode_inter_prediction_dbbp(shdr, img, x0, y0, log2CbSize, dv, vi, true);
					break;
				}
			#endif
				//fallthrough
			case PART_nLx2N:
			case PART_nRx2N:
				ok	=	decode_inter_prediction(shdr, img, motion0,	x0,			y0,				off,		nC,		merge_candidates, shared_merge, avail_left(avail) ONLY_3D(dv))
					&&	decode_inter_prediction(shdr, img, motion1,	x0 + off,	y0,				nC - off,	nC,		merge_candidates, shared_merge, avail_right(avail, merge1) ONLY_3D(dv));
				break;

			default:
				ISO_ASSERT(0); // undefined PartMode
		}
	}

	if (!ok)
		img->integrity = image::DECODING_ERRORS;

	// decode residual
	if (bit(CM_RQT_ROOT_CBF)) {
		if (sps->max_transform_hierarchy_depth_inter == 0) {
			// take care of first transform split here

			if (cbf_chroma)
				cbf_chroma = read_cbf_chroma(cabac, &model[CM_CBF_CHROMA], cbf_chroma, img->ChromaArrayType == CHROMA_422 && log2CbSize == 3);

			img->set_split_transform(x0, y0, 0);

			int x1 = x0 + nC / 2;
			int y1 = y0 + nC / 2;
			read_transform_tree(x0,y0, log2CbSize, 1, 0, INTRA_UNKNOWN, INTRA_UNKNOWN, cbf_chroma, avail_quad0(avail));
			read_transform_tree(x1,y0, log2CbSize, 1, 0, INTRA_UNKNOWN, INTRA_UNKNOWN, cbf_chroma, avail_quad1(avail));
			read_transform_tree(x0,y1, log2CbSize, 1, 0, INTRA_UNKNOWN, INTRA_UNKNOWN, cbf_chroma, avail_quad2(avail));
			read_transform_tree(x1,y1, log2CbSize, 1, 0, INTRA_UNKNOWN, INTRA_UNKNOWN, cbf_chroma, avail_quad3(avail));

			if (log2CbSize == 3 && img->ChromaArrayType != CHROMA_444 && img->ChromaArrayType != CHROMA_MONO)
				read_final_chroma_residuals(x0, y0, INTRA_UNKNOWN, cbf_chroma, avail);

		} else {
			read_transform_tree(x0, y0, log2CbSize, 0, sps->max_transform_hierarchy_depth_inter, INTRA_UNKNOWN, INTRA_UNKNOWN, cbf_chroma, avail);
		}
	}
}

void thread_context::read_coding_tree(int x0, int y0, int log2CbSize, AVAIL avail) {
	auto		pps			= img->pps.get();
	auto		sps			= pps->sps.get();
	int			ct_depth	= img->ctu_info.log2unitSize - log2CbSize;
	bool		split		= log2CbSize > sps->log2_min_luma_coding_block_size;
	PartMode	pred_part	= PART_UNKNOWN;

#ifdef HEVC_3D
	if (cqt_cu_part_pred_enabled) {
		auto	tex		= shdr->extension_3d.reftex;
		if (!isIRAP(tex->get_SliceHeader(x0, y0)) && log2CbSize <= tex->cu_info.get(x0, y0).log2_size) {
			split		= false;
			pred_part	= (PartMode)tex->cu_info.get(x0, y0).part_mode;
		}
	}
#endif

	if (split && x0 + (1 << log2CbSize) <= img->planes[0].width && y0 + (1 << log2CbSize) <= img->planes[0].height)
		split = bit(CM_SPLIT_CU + (((avail & AVAIL_A1) && img->get_ct_depth(x0 - 1, y0) > ct_depth) + ((avail & AVAIL_B1) && img->get_ct_depth(x0, y0 - 1) > ct_depth)));

	if (log2CbSize == img->ctu_info.log2unitSize - pps->cu_qp_delta_depth) {
		need_cu_qp_delta	= true;
		currentQPY			= get_QPpred(img, x0, y0, currentQPY);
	}

#ifdef HEVC_RANGE
	if (shdr->extension_range.cu_chroma_qp_offset_enabled && log2CbSize >= img->ctu_info.log2unitSize - pps->extension_range.chroma_qp_offsets->delta_depth) {
		need_cu_qp_offset	= true;
		cu_qp_offset		= zero;
	}
#endif

	if (split) {
		int x1 = x0 + (1 << (log2CbSize - 1));
		int y1 = y0 + (1 << (log2CbSize - 1));

		bool	right	= x1 < img->planes[0].width;
		bool	below	= y1 < img->planes[0].height;

		read_coding_tree(x0, y0, log2CbSize - 1, avail_quad0(avail, right, below));

		if (right)
			read_coding_tree(x1, y0, log2CbSize - 1, avail_quad1(avail));

		if (below)
			read_coding_tree(x0, y1, log2CbSize - 1, avail_quad2(avail, right));

		if (right && below)
			read_coding_tree(x1, y1, log2CbSize - 1, avail_quad3(avail));


		return;
	}

	if (img->layer_id == _debug_layer && img->picture_order_cnt == _debug_poc && img->ctu_info.index(x0, y0) == _debug_ctu) {
		(void)1;
	}

	if (log2CbSize > img->ctu_info.log2unitSize - pps->cu_qp_delta_depth) {
		need_cu_qp_delta	= true;
		currentQPY			= get_QPpred(img, x0, y0, currentQPY);
	}

	img->set_QPY(x0, y0, log2CbSize, currentQPY);

	if (transquant_bypass = pps->transquant_bypass_enabled && bit(CM_CU_TRANSQUANT_BYPASS))
		img->set_transquant_bypass(x0, y0, log2CbSize);

	// --- check for cu_skip ---
	if (shdr->type != SLICE_TYPE_I) {
		int	ctx = ((avail & AVAIL_A1) && img->get_pred_mode(x0 - 1, y0) == MODE_SKIP)
				+ ((avail & AVAIL_B1) && img->get_pred_mode(x0, y0 - 1) == MODE_SKIP);
		if (bit(CM_CU_SKIP + ctx)) {
			read_cu_skip(x0, y0, log2CbSize, avail);
			return;
		}
	}

#ifdef HEVC_3D
	// --- check for skip_intra ---
	if (skip_intra_enabled && bit(CM_SKIP_INTRA)) {
		auto	intra_pred	= !bit(CM_SKIP_INTRA_MODE_IDX) ? INTRA_ANGLE_24
							: !cabac.bypass() ? INTRA_ANGLE_8
							: !cabac.bypass() ? INTRA_SINGLE2
							: INTRA_SINGLE3;
		img->set_pred_mode(x0, y0, log2CbSize, MODE_INTRA);
		intra_pred_history.set(x0, y0, log2CbSize, INTRA_DC);
		ivs_history.clear(y0, log2CbSize);
		decode_intra_prediction(img, x0, y0, intra_pred, log2CbSize, 0, !transquant_bypass || !IF_HAS_RANGE(sps->extension_range.implicit_rdpcm_enabled), avail);
		return;
	}
#endif

	if (shdr->type == SLICE_TYPE_I || bit(CM_PRED_MODE))
		read_cu_intra(x0, y0, log2CbSize, pred_part != PART_2Nx2N, avail);
	else
		read_cu_inter(x0, y0, log2CbSize, ct_depth, pred_part, avail);

}


void thread_context::read_sao(int xCtb, int yCtb) {
	auto&	ctb			= img->ctu_info.get0(xCtb, yCtb);
	auto	avail		= check_available(ctb.flags);

	if ((avail & AVAIL_A1) && bit(CM_SAO_MERGE)) {
		ctb.sao_info = img->ctu_info.get0(xCtb - 1, yCtb).sao_info;		//merge left
		return;
	}
	if ((avail & AVAIL_B1) && bit(CM_SAO_MERGE)) {
		ctb.sao_info = img->ctu_info.get0(xCtb, yCtb - 1).sao_info;		//merge up
		return;
	}

	clear(ctb.sao_info);

	auto	pps		= img->pps.get();
	uint8	type	= 0;
	for (int c = 0, nC = num_channels(img->ChromaArrayType); c < nC; c++) {
		if (c ? shdr->sao_chroma : shdr->sao_luma) {
			if (c < 2)
				type = !bit(CM_SAO_TYPE_IDX) ? 0 : !cabac.bypass() ? 1 : 2;

			if (type) {
				int		offset[4];
				int		max = bits(min(img->get_bit_depth(c), 10) - 5);
				for (auto &i : offset)
					i = cabac.count_ones(max);

				if (type == 1) {
					for (auto &i : offset) {
						if (i)
							i = plus_minus(i, cabac.bypass());
					}
					ctb.sao_info[c].pos = cabac.bypass(5);

				} else {
					offset[2] = -offset[2];
					offset[3] = -offset[3];
					if (c < 2)
						type += cabac.bypass(2);
				}

			#ifdef HEVC_RANGE
				int shift = pps->extension_range.log2_sao_offset_scale[!!c];
			#else
				int shift = 0;
			#endif
				for (int i = 0; i < 4; i++)
					ctb.sao_info[c].offset[i] = offset[i] << shift;
			}
			ctb.sao_info[c].type	= type;
		}
	}
}

thread_context::DecodeResult thread_context::decode_substream(CM_table *wpp_models, Event *event, uint32 &ctb_addr, uint32 max_ctb) {
	auto	pps			= img->pps.get();
	const int ctb_stride= img->ctu_info.width_in_units;
	int		CtbX		= ctb_addr % ctb_stride;
	int		CtbY		= ctb_addr / ctb_stride;
	bool	wpp			= pps->entropy_coding_sync_enabled;


	ISO_ALWAYS_ASSERT(model);
	uint32	flags0	= (shdr->deblocking.disabled * CTU_info::NO_DEBLOCK) + shdr->sao_luma * CTU_info::SAO_LUMA + shdr->sao_chroma * CTU_info::SAO_CHROMA;

	do {
		if (wpp && event && CtbY > 0) {
			if (CtbX < ctb_stride - 1)
				img->ctu_info.get0(CtbX + 1, CtbY - 1).wait(CTU_info::PREFILTER, event);
		}

		if (img->layer_id == _debug_layer && img->picture_order_cnt == _debug_poc) {
			discard();
			if (_debug_ctu < 0 || ctb_addr == _debug_ctu) {
				discard();
			}
		}

		auto	&ctu	= img->ctu_info.get0(CtbX, CtbY);
		uint32	flags	= ctu.flags | flags0;
		if (CtbX == 0 || img->ctu_info.get0(CtbX - 1, CtbY).slice_address != shdr->slice_address)
			flags |= CTU_info::BOUNDARY_H | (CtbX == 0 || !shdr->loop_filter_across_slices ? CTU_info::FILTER_BOUNDARY_H : 0);
		if (CtbY == 0 || img->ctu_info.get0(CtbX, CtbY - 1).slice_address != shdr->slice_address)
			flags |= CTU_info::BOUNDARY_V | (CtbY == 0 || !shdr->loop_filter_across_slices ? CTU_info::FILTER_BOUNDARY_V : 0);

		ctu.init(shdr->slice_address, shdr->index, flags);

		if (shdr->sao_luma || shdr->sao_chroma)
			read_sao(CtbX, CtbY);

		auto	avail = check_available(CTU_info::FLAGS(flags));
		if (avail == (AVAIL_A1 | AVAIL_B1) && img->ctu_info.get0(CtbX - 1, CtbY - 1).slice_address == shdr->slice_address)
			avail |= AVAIL_B2;

		if (CtbY > 0 && CtbX < ctb_stride - 1) {
			if (img->ctu_info.get0(CtbX + 1, CtbY - 1).slice_address == shdr->slice_address
			&&	!(img->ctu_info.get0(CtbX + 1, CtbY).flags & (CTU_info::TILE_BOUNDARY_H | CTU_info::TILE_BOUNDARY_V))
			)
				avail |= AVAIL_B0;
		}

		avail |= avail0;

		if (!(avail & AVAIL_A1)) {
			intra_pred_history[0].reset(INTRA_DC);
			ivs_history.reset();
		}

		intra_pred_history[1].reset(INTRA_DC);


		QP	= calc_QP(currentQPY, 0, img->ChromaArrayType, img->planes[0].bit_depth, img->planes[1].bit_depth);

		read_coding_tree(CtbX << img->ctu_info.log2unitSize, CtbY << img->ctu_info.log2unitSize, img->ctu_info.log2unitSize, avail);
		// save CABAC-model for WPP (except in last CTB row)
		if (wpp && CtbX == 1 && CtbY < img->ctu_info.height_in_units - 1) {
			wpp_models[CtbY] = model;
			wpp_models[CtbY].decouple(); // store an independent copy
		}

		if (wpp)
			img->ctu_info.get0(CtbX, CtbY).set_progress(CTU_info::PREFILTER);

		// end of slice segment ?
		if (cabac.term_bit()) {
			if (wpp) {
				while (++CtbX < img->ctu_info.width_in_units)
					img->ctu_info.get0(CtbX, CtbY).set_progress(CTU_info::PREFILTER);
			}
			return EndOfSliceSegment;
		}

		if (pps->tiles.exists())
			ctb_addr	= pps->tiles->next_ctu(ctb_addr);
		else
			++ctb_addr;

		if (ctb_addr >= img->ctu_info.total())
			return Error;

		CtbX	= ctb_addr % ctb_stride;
		CtbY	= ctb_addr / ctb_stride;

	} while (--max_ctb);

	if (!cabac.term_bit())
		return Error;

	cabac.reset(); // byte alignment
	return EndOfSubstream;
}

//-----------------------------------------------------------------------------
// slice_unit / image_unit
//-----------------------------------------------------------------------------

PicReference::PicReference(image *img, bool longterm) : img(img), longterm(longterm), poc(img->picture_order_cnt), view_idx(img->view_idx) {}

struct slice_unit : e_link<slice_unit> {
	enum State {
		Unprocessed,
		InProgress,
		Decoded,
		Error,
	};
	volatile State	state = Unprocessed;

	NAL::unit*				nal;	// we are the owner
	slice_segment_header*	shdr;	// not the owner (image is owner)

	//CM_table				wpp_model;

	// for next dependent slice segment
	CM_table				model;
	int						QPY;
	edge_history<IntraPredMode, 6, 2>	intra_pred_history;

	slice_unit(NAL::unit* nal, slice_segment_header* shdr) : nal(nal), shdr(shdr) {}
	~slice_unit() {
		if (nal)
			nal->release();
	}

	void	save_state(thread_context& tctx) {
		model				= tctx.get_model();
		QPY					= tctx.get_QPY();
		intra_pred_history	= tctx.intra_pred_history[0];
	}
	void	restore_state(thread_context& tctx) {
		ISO_ALWAYS_ASSERT(state == slice_unit::Decoded && model);
		tctx.init_model(exchange(model, nullptr), QPY);
		tctx.intra_pred_history[0] = intra_pred_history;
	}
};


class image_unit {
public:
	ref_ptr<image>	img;
	uint64			dependency	= 0;
	bool			flush_reorder_buffer;

	dynamic_array<ref_ptr<image>>	shortterm_before;	// used for reference in current picture, smaller POC
	dynamic_array<ref_ptr<image>>	shortterm_after;	// used for reference in current picture, larger POC
	dynamic_array<ref_ptr<image>>	longterm_curr;		// used in current picture
	dynamic_array<ref_ptr<image>>	interlayer0, interlayer1;

	e_list<slice_unit>				slice_units;
	dynamic_array<CM_table>			wpp_models;		// Saved context models for WPP
	dynamic_array<SEI*>				suffix_seis;

	image_unit(image* img, bool flush_reorder_buffer) : img(img), flush_reorder_buffer(flush_reorder_buffer) {
		// alloc CABAC-model array if entropy_coding_sync is enabled
		if (img->pps->entropy_coding_sync_enabled)
			wpp_models.resize(img->ctu_info.height_in_units);
	}

	~image_unit() {
		slice_units.deleteall();
	}

	bool check_refs() const {
		bool	correct	= true;

		for (const image *i : longterm_curr)
			correct = correct && i->integrity == image::CORRECT;

		for (const image *i : shortterm_before)
			correct = correct && i->integrity == image::CORRECT;

		for (const image *i : shortterm_after)
			correct = correct && i->integrity == image::CORRECT;

		return correct;
	}

	uint64	ref_avail(int poc) const {
		uint64	views = 0;
		for (image* i : shortterm_before) {
			if (i->picture_order_cnt == poc)
				views |= bit64(i->view_idx);
		}
		for (image* i : shortterm_after) {
			if (i->picture_order_cnt == poc)
				views |= bit64(i->view_idx);
		}
		for (image* i : longterm_curr) {
			if (i->picture_order_cnt == poc)
				views |= bit64(i->view_idx);
		}
		return views;
	}

	slice_unit* get_next_unprocessed_slice_segment() {
		if (dependency)
			return nullptr;

		bool	error = false;

		for (auto &i : slice_units) {
			error = error && i.shdr->dependent;
			if (error)
				i.state = slice_unit::Error;

			if (i.state == slice_unit::Decoded || i.state == slice_unit::Error) {
				error = i.state == slice_unit::Error;
				if (i.nal)
					exchange(i.nal, nullptr)->release();

			} else if (i.state == slice_unit::Unprocessed) {
				if (!i.shdr->dependent || (i.prev->state == slice_unit::Decoded && i.state == slice_unit::Unprocessed))
					return &i;
			}
		}
		return nullptr;
	}
	bool	all_slice_segments_processed() const {
		for (auto &i : slice_units)
			if (i.state != slice_unit::Decoded && i.state != slice_unit::Error)
				return false;
		return true;
	}

	bool	get_refs(PicReference *out, range<const uint8*> ref_list, uint32 total_poc, bool bslice) const;
	bool	decode_serial(slice_unit *sliceunit)		const;
	void	decode_tiles(slice_unit *sliceunit)			const;
	void	decode_wpp(slice_unit *sliceunit)			const;
	void	finish(OPTIONS options)						const;
};


// 8.3.4	invoked at beginning of each slice
// Returns whether we can continue decoding (or whether there is a severe error).
bool image_unit::get_refs(PicReference *out, range<const uint8*> ref_list, uint32 total_poc, bool bslice) const {
	pair<image*, bool>	temp[3 * MAX_NUM_REF_PICS];

	// P: fill with reference pictures in this order:
	// 1) short term, past POC
	// 2) interlayer0
	// 3) short term, future POC
	// 4) long term
	// 3) interlayer1

	// B: Fill with reference pictures in this order:
	// 3) short term, future POC
	// 2) interlayer1
	// 1) short term, past POC
	// 4) long term
	// 3) interlayer0

	for (int r = 0, nr = max(ref_list.size(), total_poc); r < nr;) {
		for (auto i : bslice ? shortterm_after : shortterm_before) {
			if (r >= nr)
				break;
			temp[r++]	= {i, false};
		}
		for (auto i : bslice ? interlayer1 : interlayer0) {
			if (r >= nr)
				break;
			temp[r++]	= {i, true};
		}
		for (auto i : bslice ? shortterm_before : shortterm_after) {
			if (r >= nr)
				break;
			temp[r++]	= {i, false};
		}
		for (auto i : longterm_curr) {
			if (r >= nr)
				break;
			temp[r++]	= {i, true};
		}
		for (auto i : bslice ? interlayer0 : interlayer1) {
			if (r >= nr)
				break;
			temp[r++]	= {i, true};
		}

		// This check is to prevent an endless loop when no images are added above
		if (r == 0)
			return false;
	}

	for (auto i : ref_list) {
		out->img		= temp[i].a;
		out->longterm	= temp[i].b;
		out->poc		= out->img->picture_order_cnt;
		out->view_idx	= out->img->view_idx;
		++out;
	}

	return true;
}

bool image_unit::decode_serial(slice_unit *sliceunit) const {
	auto	shdr	= sliceunit->shdr;
	auto	pps		= img->pps.get();
	auto	tiles	= pps->tiles.exists_ptr();
	int		tile	= tiles ? tiles->RStoTile[shdr->segment_address] : 0;
	bool	wpp		= pps->entropy_coding_sync_enabled;

	thread_context tctx(img, shdr, shdr->stream_data);

	if (shdr->dependent)
		sliceunit->prev->restore_state(tctx);
	else
		tctx.init_model();

	auto	result		= thread_context::EndOfSubstream;
	uint32	ctb_addr	= shdr->segment_address;
	uint32	num_ctb		= 0;

	//--- handle partial tile/row of first substream ---
	if (tiles) {
		if (tiles->get_tile_start(tile) != shdr->segment_address) {
			auto	curr	= tiles->RStoTS[shdr->segment_address];
			auto	end		= tiles->RStoTS[tiles->get_tile_start(++tile)];
			result	= tctx.decode_substream(wpp_models, nullptr, ctb_addr, end - curr);
		}

	} else if (wpp) {
		num_ctb		= img->ctu_info.width_in_units;
		int	ctbx	= shdr->segment_address % num_ctb;
		if (!shdr->dependent || ctbx != 0)
			result = tctx.decode_substream(wpp_models, nullptr, ctb_addr, num_ctb - ctbx);
	}

	while (result == thread_context::EndOfSubstream) {
		if (tiles) {
			tctx.init_model();
			num_ctb	= tiles->get_tile_size(tile++);

		} else if (wpp) {
			if (int	ctby = ctb_addr / num_ctb) {
				if (wpp_models[ctby - 1])
					tctx.init_model(exchange(wpp_models[ctby - 1], nullptr), shdr->get_qp());//model_events[ctby - 1].QPY);
				else
					tctx.init_model();
			}
		}

		result = tctx.decode_substream(wpp_models, nullptr, ctb_addr, num_ctb);
	}

	if (result == thread_context::EndOfSliceSegment && pps->dependent_slice_segments_enabled)
		sliceunit->save_state(tctx);

	return result != thread_context::Error;
}

void image_unit::decode_tiles(slice_unit *sliceunit) const {
	auto	shdr = sliceunit->shdr;
	auto	pps		= img->pps.get();
	auto	tiles	= pps->tiles.exists_ptr();
	auto	tile	= tiles->RStoTile[shdr->segment_address];

	parallel_for(int_range(shdr->entry_points.size() + 1), [this, sliceunit, tile](int i) {
		auto	shdr	= sliceunit->shdr;

		thread_context tctx(img, shdr, shdr->get_substream_data(i));

		auto	tiles	= shdr->pps->tiles.exists_ptr();
		uint32	ctb_addr = shdr->segment_address;
		uint32	num_ctb	= tiles->get_tile_size(tile + i);

		if (i == 0)
			num_ctb		-= ctb_addr - tiles->get_tile_start(tile);
		else
			ctb_addr	= tiles->get_tile_start(tile + i);

		if (i == 0 && shdr->dependent && tiles->get_tile_start(tile) != ctb_addr)
			sliceunit->prev->restore_state(tctx);
		else
			tctx.init_model();

		auto	result = tctx.decode_substream(wpp_models, nullptr, ctb_addr, num_ctb);

		if (result == thread_context::Error) {
			img->integrity		= image::DECODING_ERRORS;
			sliceunit->state	= slice_unit::Error;
		} else if (result == thread_context::EndOfSliceSegment) {
			if (shdr->pps->dependent_slice_segments_enabled)
				sliceunit->save_state(tctx);
		}

	});
}

void image_unit::decode_wpp(slice_unit *sliceunit) const {
	auto	shdr = sliceunit->shdr;

	parallel_for(int_range(shdr->entry_points.size() + 1), [this, sliceunit](int i) {
		auto	shdr		= sliceunit->shdr;

		thread_context tctx(img, shdr, shdr->get_substream_data(i));

		uint32	ctb_addr	= shdr->segment_address;
		uint32	num_ctb		= img->ctu_info.width_in_units;
		auto	offset		= ctb_addr % num_ctb;
		int		y			= i > 0 || (shdr->dependent && offset == 0) ? ctb_addr / num_ctb + i : 0;
		Event	event;

		if (i == 0)
			num_ctb		-= offset;
		else
			ctb_addr	+= i * num_ctb - offset;

		if (y) {
			img->ctu_info.get0(1, y - 1).wait(CTU_info::PREFILTER, &event);
			tctx.init_model(exchange(wpp_models[y - 1], nullptr), shdr->get_qp());
		} else if (i == 0 && shdr->dependent) {
			sliceunit->prev->restore_state(tctx);
		} else {
			tctx.init_model();
		}

		auto	result = tctx.decode_substream(wpp_models, &event, ctb_addr, num_ctb);

		if (result == thread_context::Error) {
			img->integrity		= image::DECODING_ERRORS;
			sliceunit->state	= slice_unit::Error;
		} else if (result == thread_context::EndOfSliceSegment) {
			if (shdr->pps->dependent_slice_segments_enabled)
				sliceunit->save_state(tctx);
		}

	});
}


void image_unit::finish(OPTIONS options) const {
#if defined(ISO_DEBUG)
	print_checksum(img);
#endif

	if (!(options & OPT_disable_deblocking))
		apply_deblocking_filter(img, !(options & OPT_force_sequential));

	if (!(options & OPT_disable_sao) && img->pps->sps->sample_adaptive_offset_enabled)
		apply_sample_adaptive_offset(img, !(options & OPT_force_sequential));

#if 0
	print_checksum(img);
#endif

	// process suffix SEIs
	for (auto i : suffix_seis) {
		if (!i->process0(img)) {
			ISO_OUTPUT("SEI failed\n");
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Layer
//-----------------------------------------------------------------------------

void Layer::reset() {
	first_decoded_picture	= true;
	reorder_output_queue.clear();
	image_output_queue.clear();

	while (!image_units.empty()) {
		delete image_units.back();
		image_units.pop_back();
	}
}

image* Layer::new_image(const pic_parameter_set *pps, int64 pts, void* user_data) {
	image* img = free_images;

	if (img)
		free_images	= (image*)img->free_ptr;
	else
		img			= new image;

	img->free_ptr	= &free_images;

	img->alloc(pps, pts, user_data);
	img->integrity = image::CORRECT;
	if (auto con = pps->sps->conformance_window.exists_ptr())
		img->offsets = (*con);
	else
		img->offsets = 0;
	return img;
}

image* Layer::new_dummy(const pic_parameter_set* pps, int poc) {
	image	*img = new_image(pps, 0, nullptr);
	if (img) {
		uint16	fill_luma	= (img->planes[0].bit_depth <= 8 ? 0x0101 : 1) << (img->planes[0].bit_depth - 1);
		uint16	fill_chroma = (img->planes[1].bit_depth <= 8 ? 0x0101 : 1) << (img->planes[1].bit_depth - 1);
		img->planes[0].pixels.fill(fill_luma);
		img->planes[1].pixels.fill(fill_chroma);
		img->planes[2].pixels.fill(fill_chroma);

		over(img->cu_info.all(), [](CU_info& i) { i.pred_mode = MODE_INTRA; });

		img->picture_order_cnt		= poc;
		img->is_output				= false;
		img->integrity				= image::UNAVAILABLE_REFERENCE;
	}
	return img;
}

image *Layer::image_for_POC(int poc, uint32 poc_mask, bool preferLongTerm) const {
	if (preferLongTerm) {
		for (auto &i : long_term_images) {
			if (!((i->picture_order_cnt - poc) & poc_mask))
				return i;
		}
	}
	for (auto &i : short_term_images) {
		if (!((i->picture_order_cnt - poc) & poc_mask))
			return i;
	}
	return nullptr;
}

uint64 Layer::ref_avail(int poc) const {
	return image_units.empty() ? 0 : image_units.back()->ref_avail(poc);
}

image_unit* Layer::get_unit(int poc) const {
	for (auto i : image_units) {
		if (i->img->picture_order_cnt == poc)
			return i;
	}
	return nullptr;
}

image* Layer::current() const {
//	return image_units.empty() ? nullptr : image_units.back()->img;
	return short_term_images.empty() ? nullptr : (image*)short_term_images.back(); 
}

bool Layer::decode(bool parallel) {
	if (image_units.empty())
		return false;

	auto	imgunit		= image_units.front();
	auto	sliceunit	= imgunit->get_next_unprocessed_slice_segment();

	if (!sliceunit)
		return false;

	image*	img			= imgunit->img;

	if (sliceunit == &imgunit->slice_units.front() && imgunit->flush_reorder_buffer)
		flush_reorder_buffer();

	sliceunit->state	= slice_unit::InProgress;

	ISO_OUTPUTF("decode layer ") << int(img->layer_id) << " POC " << img->picture_order_cnt << " slice " << sliceunit->shdr->segment_address << onlyif(sliceunit->shdr->dependent, "(dependent)") << '\n';

	if (!parallel) {
		if (imgunit->decode_serial(sliceunit)) {
			sliceunit->state	= slice_unit::Decoded;
		} else {
			img->integrity		= image::DECODING_ERRORS;
			sliceunit->state	= slice_unit::Error;
		}

	} else {
		ConcurrentJobs::Get().add(
		//RunThread(
			[imgunit, sliceunit]() mutable {
			image*	img		= imgunit->img;
			auto	pps		= img->pps.get();
			auto	tiles	= pps->tiles.exists_ptr();

			for (;;sliceunit = sliceunit->next) {
				auto	shdr = sliceunit->shdr;

				if (pps->entropy_coding_sync_enabled && img->ctu_info.width_in_units > 1) {
					// WPP
				#if 1
					imgunit->decode_wpp(sliceunit);
				#else
					parallel_for(int_range(shdr->entry_points.size() + 1), [imgunit, sliceunit](int i) {
						auto	shdr		= sliceunit->shdr;
						image*	img			= imgunit->img;

						thread_context tctx(img, shdr, shdr->get_substream_data(i));

						uint32	ctb_addr	= shdr->segment_address;
						uint32	num_ctb		= img->ctu_info.width_in_units;
						auto	offset		= ctb_addr % num_ctb;
						int		y			= i > 0 || (shdr->dependent && offset == 0) ? ctb_addr / num_ctb + i : 0;
						Event	event;

						if (i == 0)
							num_ctb		-= offset;
						else
							ctb_addr	+= i * num_ctb - offset;

						if (y) {
							img->ctu_info.get0(1, y - 1).wait(CTU_info::PREFILTER, &event);
							tctx.init_model(exchange(imgunit->wpp_models[y - 1], nullptr), shdr->get_qp());
						} else if (i == 0 && shdr->dependent) {
							sliceunit->prev->restore_state(tctx);
						} else {
							tctx.init_model();
						}

						auto	result = tctx.decode_substream(imgunit->wpp_models, &event, ctb_addr, num_ctb);

						if (result == thread_context::Error) {
							img->integrity		= image::DECODING_ERRORS;
							sliceunit->state	= slice_unit::Error;
						} else {
							ISO_ASSERT(result == thread_context::EndOfSliceSegment);
							if (shdr->pps->dependent_slice_segments_enabled)
								sliceunit->save_state(tctx);
						}

					});
				#endif

				} else if (tiles && shdr->entry_points) {
					// TILED
				#if 1
					imgunit->decode_tiles(sliceunit);
				#else
					auto	tile = tiles->RStoTile[shdr->segment_address];
					parallel_for(int_range(shdr->entry_points.size() + 1), [imgunit, sliceunit, tile](int i) {
						auto	shdr	= sliceunit->shdr;
						image*	img		= imgunit->img;

						thread_context tctx(img, shdr, shdr->get_substream_data(i));

						auto	tiles	= shdr->pps->tiles.exists_ptr();
						uint32	ctb_addr = shdr->segment_address;
						uint32	num_ctb	= tiles->get_tile_size(tile + i);

						if (i == 0)
							num_ctb		-= ctb_addr - tiles->get_tile_start(tile);
						else
							ctb_addr	= tiles->get_tile_start(tile + i);

						if (i == 0 && shdr->dependent && tiles->get_tile_start(tile) != ctb_addr)
							sliceunit->prev->restore_state(tctx);
						else
							tctx.init_model();

						auto	result = tctx.decode_substream(imgunit->wpp_models, nullptr, ctb_addr, num_ctb);

						if (result == thread_context::Error) {
							img->integrity		= image::DECODING_ERRORS;
							sliceunit->state	= slice_unit::Error;
						} else {
							ISO_ASSERT(result == thread_context::EndOfSliceSegment);
							if (shdr->pps->dependent_slice_segments_enabled)
								sliceunit->save_state(tctx);
						}

					});
				#endif

				} else {
					// SINGLE SUBSTREAM
					if (!imgunit->decode_serial(sliceunit)) {
						img->integrity		= image::DECODING_ERRORS;
						sliceunit->state	= slice_unit::Error;
					}
				}

				if (sliceunit->state == slice_unit::Error)
					break;

				if (sliceunit == &imgunit->slice_units.back() || !sliceunit->next->shdr->dependent) {
					sliceunit->state = slice_unit::Decoded;
					break;
				}

				// do dependent slices on same thread
				sliceunit->next->state	= slice_unit::InProgress;
				sliceunit->state		= slice_unit::Decoded;

				ISO_OUTPUTF("+decode layer ") << int(img->layer_id) << " POC " << img->picture_order_cnt << " slice " << sliceunit->next->shdr->segment_address << '\n';
			}

		});
	}
	return true;
}

image* Layer::finish_pic(OPTIONS options) {
	auto	imgunit = image_units.front();
	if (!imgunit->all_slice_segments_processed())
		return nullptr;

	imgunit->finish(options);

	image *img = imgunit->img;
	if (img->is_output && img->integrity == image::CORRECT || !(options & OPT_suppress_faulty_pictures)) {
		if (options & OPT_output_immediately) {
			image_output_queue.push_back(img);

		} else {
			reorder_output_queue.push_back(img);
			// check for full reorder buffers
			if (reorder_output_queue.size() > img->pps->sps->vps->ordering.layers.back().max_num_reorder_pics)
				output_next_picture_in_reorder_buffer();
		}
	}

	delete imgunit;
	image_units.erase(image_units.begin());
	return img;
}

void Layer::start_pic(image *img, slice_segment_header* shdr, uint8 temporal_id, const ShortTermReferenceSet &shortterm, const LongTermReferenceSet &longterm) {
	auto	unit_type		= shdr->unit_type;
	bool	irap			= isIRAP(unit_type);

	if (irap) {
		if (isIDR(unit_type) || isBLA(unit_type) || first_decoded_picture || first_after_EOS) {
			NoRaslOutput		= true;
			first_after_EOS		= false;
		} else if (isCRA(unit_type)) {
			NoRaslOutput		= false;
		}
	}

	auto	pps		= shdr->pps.get();
	auto	sps		= pps->sps.get();
	img->is_output	= (!isRASL(unit_type) || !NoRaslOutput) && shdr->pic_output;

	// 8.3.1 process_picture_order_count
	int		order_msb	= 0;
	if (!irap || !NoRaslOutput) {
		int limit		= 1 << sps->log2_max_pic_order_cnt_lsb;
		int	prev_lsb	= prev_POC & (limit - 1);
		int	prev_msb	= prev_POC & -limit;
		order_msb		= shdr->pic_order_cnt_lsb <= prev_lsb - limit / 2 ? prev_msb + limit
						: shdr->pic_order_cnt_lsb >  prev_lsb + limit / 2 ? prev_msb - limit
						: prev_msb;
	}

	img->clear_metadata();
	img->picture_order_cnt	= order_msb + shdr->pic_order_cnt_lsb;
	img->temporal_id		= temporal_id;
	img->nal_unit			= unit_type;

	if (temporal_id == 0 && !isSublayerNonReference(unit_type) && !isRASL(unit_type) && !isRADL(unit_type))
		prev_POC = img->picture_order_cnt;

	// next image is not the first anymore
	first_decoded_picture = false;

	auto	imgunit	= new image_unit(img, irap && NoRaslOutput);
	swap(imgunit->suffix_seis, held_seis);
	image_units.push_back(imgunit);

	// reflists

	if (!isIDR(unit_type)) {
		dynamic_array<ref_ptr<image>>	next_short_term_images;
		dynamic_array<ref_ptr<image>>	next_long_term_images;

		// (8-98)
		uint32	poc_mask	= bits(sps->log2_max_pic_order_cnt_lsb);
		auto	poc_curr	= img->picture_order_cnt;

		// scan ref-pic-set for smaller POCs and fill into shortterm_before / shortterm_future
		for (auto &i : shortterm.neg) {
			auto	poc = poc_curr + i.poc;
			auto	k	= image_for_POC(poc);
			if (!k && i.used)
				k = new_dummy(pps, poc);

			if (k)
				next_short_term_images.push_back(k);
			if (i.used)
				imgunit->shortterm_before.push_back(k);
		}

		// scan ref-pic-set for larger POCs and fill into shortterm_after / shortterm_future
		for (auto &i : shortterm.pos) {
			auto	poc = poc_curr + i.poc;
			auto	k	= image_for_POC(poc);
			if (!k && i.used)
				k = new_dummy(pps, poc);

			if (k)
				next_short_term_images.push_back(k);
			if (i.used)
				imgunit->shortterm_after.push_back(k);
		}

		// find used / future long-term references
		for (auto &i : longterm) {
			int		poc	= i.has_delta_msb ? i.poc + (poc_curr & ~poc_mask) : i.poc;
			auto	k	= image_for_POC(poc, i.has_delta_msb ? ~0 : poc_mask, true);
			if (!k)
				k = new_dummy(pps, poc);

			next_long_term_images.push_back(k);
			if (i.used)
				imgunit->longterm_curr.push_back(k);
		}

		swap(short_term_images, next_short_term_images);
		swap(long_term_images, next_long_term_images);

		if (!imgunit->check_refs())
			img->integrity = image::DERIVED_FROM_FAULTY_REFERENCE;
	}
	short_term_images.push_back(img);

}

image_base*	Layer::get_next_picture_in_output_queue()	const	{
	return image_output_queue.empty() ? nullptr : image_output_queue.front();
}

void Layer::pop_next_picture_in_output_queue(bool hold) {
	if (hold)
		image_output_queue.front()->addref();
//	image_output_queue.pop_front();
	image_output_queue.erase(image_output_queue.begin());
}

void Layer::output_next_picture_in_reorder_buffer() {
	ref_ptr<image>	*p		= nullptr;
	int		minPOC	= maximum;
	for (auto &i : reorder_output_queue) {
		if (i->picture_order_cnt < minPOC) {
			minPOC	= i->picture_order_cnt;
			p		= &i;
		}
	}
	if (p) {
		image_output_queue.push_back(*p);			// put image into output queue
		reorder_output_queue.erase_unordered(p);	// remove image from reorder buffer
	}
}

void Layer::flush_reorder_buffer() {
	while (!reorder_output_queue.empty())
		output_next_picture_in_reorder_buffer();
}

Layer::~Layer() {
	while (!image_units.empty()) {
		delete image_units.back();
		image_units.pop_back();
	}
}

//-----------------------------------------------------------------------------
// decoder
//-----------------------------------------------------------------------------

tables_reference::tables_reference()	{ tables.addref(); }
tables_reference::~tables_reference()	{ tables.release(); }

decoder_context::decoder_context(OPTIONS options) :
#ifdef HEVC_ML
	layers(1),
#endif
	options(options) {
	compute_framedrop_table();
}

#ifdef HEVC_ML

image_base* decoder_context::get_next_picture_in_output_queue(uint64 layer_mask) const {
	for (auto i : make_bit_container(layer_mask & bits64(layers.size()))) {
		if (!layers[i].image_output_queue.empty())
			return layers[i].image_output_queue.front();
	}

	return nullptr;
}

void decoder_context::pop_next_picture_in_output_queue(int layer, bool hold) {
	if (auto img = layers[layer].get_next_picture_in_output_queue())
		ISO_TRACEF("pop layer ") << int(img->layer_id) << " POC " << img->picture_order_cnt << '\n';
	layers[layer].pop_next_picture_in_output_queue(hold);
}

void decoder_context::clear_output_queues(uint64 layer_mask) {
	for (auto i : make_bit_container(layer_mask & bits64(layers.size())))
		layers[i].image_output_queue.clear();
}

void decoder_context::finish_pic(Layer& layer) {
	if (auto img = layer.finish_pic(options)) {
		int		poc		= img->picture_order_cnt;
		auto	mask	= bit(img->layer_id);

		for (auto& i : layers) {
			if (auto unit = i.get_unit(poc))
				unit->dependency &= ~mask;
		}
	}
}

#endif

decoder_context::~decoder_context() {}


int decoder_context::decode_NAL(NAL::unit* nal) {
	auto	nal_hdr	= nal->get_header();

//	ISO_OUTPUTF("NAL:") << nal_hdr.type << " layer:" << nal_hdr.layer_id << " temporal:" << nal_hdr.temporal_id_plus1 - 1 << '\n';

	if (nal_hdr.layer_id > 0 && (options & OPT_disable_SHVC))
		return true;		// discard all NAL units with layer_id > 0 - these will have to be handled by an SHVC decoder

#ifdef HEVC_ML
	if (nal_hdr.layer_id >= layers.size())
		return false;

	Layer	&layer	= layers[nal_hdr.layer_id];
#else
	if (nal_hdr.layer_id)
		return true;

	Layer	&layer	= *this;
#endif

	int	temporal_id = nal_hdr.temporal_id_plus1 - 1;
	if (temporal_id > current_HighestTid)
		return true;

	if (temporal_id == current_HighestTid) {
		frame_accum += layer_framerate_ratio;
		if (frame_accum < 100)
			return true;

		frame_accum -= 100;
	}

	bitreader	r(nal->data + 2);

	auto	unit_type = (NAL::TYPE)nal_hdr.type;
	switch (unit_type) {
		case NAL::VPS_NUT:
			if (nal_hdr.layer_id == 0) {
				auto new_vps = new video_parameter_set;
				if (new_vps->read(r)) {
					vps[new_vps->id] = new_vps;
				#ifdef HEVC_ML
					if (!(options & OPT_disable_SHVC)) {
						//layers.resize(new_vps->max_sub_layers);
						layers.resize(new_vps->max_layers);
					}
				#endif
					return true;
				}
				delete new_vps;
			}
			return false;

		case NAL::SPS_NUT: {
			auto	vps_id	= r.get(4);
			if (auto v = vps[vps_id]) {
				auto	new_sps = new seq_parameter_set(v);

				if (new_sps->read(r, nal_hdr.layer_id)) {
					size	= {new_sps->pic_width_luma, new_sps->pic_height_luma};

					auto	old_sps = exchange(sps[new_sps->id], new_sps);
					// Remove the all PPS that referenced the old SPS
					for (auto& p : pps) {
						if (p && p->sps.get() == old_sps.get())
							p = nullptr;
					}
					return true;
				}
				delete new_sps;
			}
			return false;
		}

		case NAL::PPS_NUT: {
			auto	id		= r.getu();
			auto	sps_id	= r.getu();
			if (auto s = sps[sps_id]) {
				auto	new_pps = new pic_parameter_set(id, s);
				if (new_pps->read(r)) {
					pps[id] = new_pps;
					return true;
				}
				delete new_pps;
			}
			return false;
		}

		case NAL::PREFIX_SEI_NUT:
			if (auto sei = SEI::read0(nal->data + 2)) {
				if (sei->process0(nullptr))
					layer.held_seis.push_back(sei);
				else
					delete sei;
				return true;
			}
			return false;

		case NAL::SUFFIX_SEI_NUT:
			if (auto sei = SEI::read0(nal->data + 2)) {
				if (!layer.image_units.empty())
					layer.image_units.back()->suffix_seis.push_back(sei);
				else
					layer.held_seis.push_back(sei);
				return true;
			}
			return false;

		case NAL::AUD_NUT: {
			auto	pic_type = r.get(3);
			return true;
		}

		case NAL::EOS_NUT:
			layer.first_after_EOS = true;
			return true;

		default:
			if (unit_type < NAL::VPS_NUT) {
				// read slice header
				bool	first_slice_segment_in_pic	= r.get_bit();
				bool	no_output_of_prior_pics		= between(unit_type, NAL::BLA_W_LP, NAL::RESERVED_IRAP_VCL23) && r.get_bit();
				auto	pps							= get_pps(r.getu());
				if (!pps)
					return false;

			#if 0
				// Skip pictures due to random access
				if (isRASL(unit_type) && layer.NoRaslOutput)
					return true;
			#endif
				(void)no_output_of_prior_pics;

				slice_segment_header* shdr = new slice_segment_header(pps, unit_type, first_slice_segment_in_pic);
				LongTermReferenceSet	longterm;
				ShortTermReferenceSet	shortterm;

				if (shdr->read(r, nal_hdr.layer_id, temporal_id, layer.prev_shdr, shortterm, longterm)) {
					auto	sps			= pps->sps.get();
					auto	vps			= sps->vps.get();
				#ifdef HEVC_ML
					auto	vpsml		= vps->extension_multilayer.exists_ptr();
					auto	vpslayer	= nal_hdr.layer_id && vpsml ? vpsml->layer_by_id(nal_hdr.layer_id) : nullptr;
				#endif

					if (first_slice_segment_in_pic) {
						if (img = layer.new_image(pps, nal->pts, nal->user_data)) {
							layer.start_pic(img, shdr, temporal_id, shortterm, longterm);

						#ifdef HEVC_ML
							if (vpslayer) {
								// --- make inter-layer refs ---
								auto	view0		= vpsml->view_ids[0];
								auto	view1		= vpsml->view_ids[vpslayer->view_order_idx()];
								auto	imgunit		= layer.image_units.back();

								imgunit->dependency	= vpslayer->direct_dependency;
								img->layer_id		= nal_hdr.layer_id;
								img->view_idx		= vpslayer->view_order_idx();

								for (auto i : shdr->extension_multilayer.ref_layers) {
									auto	img2		= interlayer_image(pps, nal_hdr.layer_id, layers[i].current(), !(options & OPT_force_sequential));
									auto	vpslayer2	= vpsml->layer_by_id(i);
									auto	view2		= vpsml->view_ids[vpslayer2->view_order_idx()];
									(view1 <= min(view0, view2) || view1 >= max(view0, view1) ? imgunit->interlayer0 : imgunit->interlayer1).push_back(img2);
								}
							}
						#endif
						}
					}

					if (img) {
						auto	imgunit = layer.image_units.back();

						if ((shdr->type == SLICE_TYPE_I || imgunit->get_refs(shdr->ref_pics[0], shdr->ref_list[0], shdr->NumPocTotalCurr, false))
						&&  (shdr->type != SLICE_TYPE_B || imgunit->get_refs(shdr->ref_pics[1], shdr->ref_list[1], shdr->NumPocTotalCurr, true))
						) {
							shdr->has_future_refs = false;

							for (auto &i : shdr->get_ref_pics(true))
								if (shdr->has_future_refs = i.poc > img->picture_order_cnt)
									break;

							if (!shdr->has_future_refs) {
								for (auto &i : shdr->get_ref_pics(false))
									if (shdr->has_future_refs = i.poc > img->picture_order_cnt)
										break;
							}

						#ifdef HEVC_ML
							if (vpsml) {
							#ifdef HEVC_3D
								if (vps->extension_3d.exists())
									shdr->extension_3d.init(shdr, vpsml, vpslayer, img->picture_order_cnt);

								if (vpslayer) {
									if (shdr->extension_3d.needs_tex()) {
										auto	tex = layers[vpsml->find_layer(vpslayer->view_order_idx(), false)->id_in_nuh].current();
										ISO_ASSERT(!tex || tex->picture_order_cnt == img->picture_order_cnt);
										shdr->extension_3d.set_tex(tex);
									}

									if (!shdr->extension_3d.depth_layer) {
										for (int k = 0; k < 2; k++) {
											if (shdr->extension_3d.rpref_idx[k] >= 0) {
												for (auto i : shdr->extension_multilayer.ref_layers)
													shdr->extension_3d.ref_rpref_avail[k] |= layers[i].ref_avail(shdr->ref(k, shdr->extension_3d.rpref_idx[k]).poc);
											}
										}

										shdr->extension_3d.depth_images.resize(highest_set_index(shdr->extension_3d.view_mask) + 1);
										for (auto vm = shdr->extension_3d.view_mask; vm; vm = clear_lowest(vm)) {
											int		v	= lowest_set_index(vm);
											if (auto dlayer = vpsml->find_layer(v, true))
												shdr->extension_3d.depth_images[v] = layers[dlayer->id_in_nuh].current();
										}

									}
								}
							#endif
							}
						#endif

							layer.prev_shdr	= shdr;
							// if number of temporal layers changed, we have to recompute the framedrop table
							if (sps->max_sub_layers - 1 != highestTid) {
								highestTid = sps->max_sub_layers - 1;
								compute_framedrop_table();
							}

							calc_tid_and_framerate_ratio();

							r.discard(1);
							r.align(8);
							r.restore_unused();

							// modify entry_points
							int headerLength	= r.get_stream().tell() + 2;
							shdr->stream_data	= r.get_stream().get_block(maximum);

							for (auto &i : shdr->entry_points)
								i -= nal->num_skipped_bytes_before(i + headerLength);

							// add slice to current picture
							shdr->index = img->add_slice_segment_header(shdr);
							//imgunit->slice_units.emplace_back(nal, shdr, r.get_stream().get_block(maximum));
							imgunit->slice_units.push_back(new slice_unit(nal, shdr));

							return -1;	// we took ownership of nal

						} else {
							img->integrity = image::NOT_DECODED;
						}
					}
				}

				delete shdr;
				return img || !first_slice_segment_in_pic ? RES_error : RES_stall_out;
			}
			return true;
	}
}

decoder_context::DecodeResult decoder_context::decode(NAL::Parser &nal_parser) {

	bool	did_work	= nal_parser.queue_length();

	if (did_work) {
		auto	nal = nal_parser.pop();
		auto	ret = decode_NAL(nal);
		if (ret != -1) {
			nal_parser.dealloc(nal);
			if (ret != RES_ok)
				return (DecodeResult)ret;
		}
	}

#ifdef HEVC_ML
	for (auto& i : layers) {
		if (i.decode(!(options & OPT_force_sequential))) {
			did_work	= true;
		} else if (i.image_units.size() > 1) {
			finish_pic(i);
		}
	}
#else
	if (!image_units.empty()) {
		auto	imgunit		= image_units.front();
		if (auto sliceunit = imgunit->get_next_unprocessed_slice_segment()) {
			//is it first sliceunit in this imgunit?
			if (sliceunit == &imgunit->slice_units.front() && imgunit->flush_reorder_buffer)
				flush_reorder_buffer();

			imgunit->decode(sliceunit, !(options & OPT_force_sequential));
			did_work	= true;

		} else if (image_units.size() > 1) {
			finish_pic(options);
		}
	}
#endif

	if (!did_work) {
		if (nal_parser.end_of_stream || nal_parser.end_of_frame) {
		#ifdef HEVC_ML
			for (auto &i : layers) {
				if (!i.image_units.empty())
					finish_pic(i);
				if (nal_parser.end_of_stream)
					i.flush_reorder_buffer();
				if (!i.image_units.empty())
					return RES_ok;
			}
		#else
			if (!image_units.empty())
				finish_pic(options);
			if (nal_parser.end_of_stream)
				flush_reorder_buffer();
			if (!image_units.empty())
				return RES_ok;
		#endif
			for (auto& i : layers) {
				if (!i.image_output_queue.empty())
					return RES_ok;
			}
			return RES_error;
		}

		return RES_stall_in;	 // we need more data -> input stalled

	}

	return RES_ok;
}


/*
.     0     1     2       <- goal_HighestTid
+-----+-----+-----+
| -0->| -1->| -2->|
+-----+-----+-----+
0     33    66    100     <- framerate_ratio
*/

void decoder_context::compute_framedrop_table() {
	for (int tid = highestTid; tid >= 0; tid--) {
		int lower	= 100 * tid / (highestTid + 1);
		int higher	= 100 * (tid + 1) / (highestTid + 1);

		for (int i = lower; i <= higher; i++) {
			int ratio = 100 * (i - lower) / (higher - lower);

			// if we would exceed our TID limit, decode the highest TID at full frame-rate
			if (tid > limit_HighestTid) {
				tid		= limit_HighestTid;
				ratio	= 100;
			}

			framedrop_tab[i].tid	= tid;
			framedrop_tab[i].ratio	= ratio;
		}

		//framedrop_tid_index[tid] = higher;
	}
}

void decoder_context::calc_tid_and_framerate_ratio() {
	goal_HighestTid			= framedrop_tab[framerate_ratio].tid;
	layer_framerate_ratio	= framedrop_tab[framerate_ratio].ratio;

	// TODO: for now, we switch immediately
	current_HighestTid		= goal_HighestTid;
}

void decoder_context::change_framerate(int more) {
	ISO_ASSERT(between(more, -1, +1));

	goal_HighestTid = clamp(goal_HighestTid + more, 0, highestTid);

	set_framerate_ratio(100 * (goal_HighestTid + 1) / (highestTid + 1));
}

void decoder_context::set_limit_TID(int tid) {
	limit_HighestTid = tid;
	calc_tid_and_framerate_ratio();
}

void decoder_context::set_framerate_ratio(int percent)	{
	framerate_ratio = percent;
	calc_tid_and_framerate_ratio();
}

uint32x2 decoder_context::get_image_size(NAL::Parser &nal_parser)	{
	uint32x2	size	= zero;

	while (nal_parser.number_pending()) {
		auto	nal		= nal_parser.pop();
		auto	nal_hdr	= nal->get_header();
		if (nal_hdr.type == NAL::SPS_NUT) {
			seq_parameter_set		sps(nullptr);
			bitreader	r(nal->data + 2);
			if (sps.read(r, nal_hdr.layer_id))
				size = {sps.pic_width_luma, sps.pic_height_luma};
			nal_parser.dealloc(nal);
			break;
		}
		nal_parser.dealloc(nal);
	}
	return size;
}

} //namespace h265
