#ifndef VPX_DECODE_H
#define VPX_DECODE_H

#include "codec/prob_coder.h"
#include "codec/vlc.h"
#include "base/bits.h"
#include "base/array.h"
#include "base/atomic.h"
#include "stream.h"

#if defined __GNUC__ || defined __MWERKS__
#define DECLARE_ALIGNED(n,t,v)      t __attribute__((__aligned__(n))) v
#else
#define DECLARE_ALIGNED(n,t,v)      __declspec(align(n)) t v
#endif

namespace vp9 {
using namespace iso;

typedef prob_code::prob			prob;
typedef prob_code::tree_index	tree_index;
typedef int32	tran_high_t;
typedef int16	tran_low_t;
typedef char	ENTROPY_CONTEXT;
typedef char	PARTITION_CONTEXT;

typedef vlc_in<uint32, true, memory_reader&>	bit_reader;
typedef prob_decoder<uint64, memory_reader>		reader;

struct GPU;

//-------------------------------------
//	enums
//-------------------------------------
enum RETURN {
	RETURN_OK				= 0,
	RETURN_ERROR			= -1,
	RETURN_MEM_ERROR		= -2,
	RETURN_ABI_MISMATCH		= -3,
	RETURN_INCAPABLE		= -4,
	RETURN_UNSUP_BITSTREAM	= -5,
	RETURN_UNSUP_FEATURE	= -6,
	RETURN_CORRUPT_FRAME	= -7,
	RETURN_INVALID_PARAM	= -8,
};

enum GLOBALS {
	MI_SIZE_LOG2		= 3,
	MI_BLOCK_SIZE_LOG2	= 6 - MI_SIZE_LOG2,			// 64 = 2^6
	MI_SIZE				= 1 << MI_SIZE_LOG2,		// pixels per mi-unit
	MI_BLOCK_SIZE		= 1 << MI_BLOCK_SIZE_LOG2,	// mi-units per max block
	MI_MASK				= MI_BLOCK_SIZE - 1,

	MINQ				= 0,
	MAXQ				= 255,
	QINDEX_RANGE		= MAXQ - MINQ + 1,
	QINDEX_BITS			= 8,

	MAX_SEGMENTS		= 8,
	MAX_PLANES			= 3,
	PREDICTION_PROBS	= 3,

	DEC_BORDER			= 32,
	ENC_BORDER			= 160,
};

enum BITSTREAM_PROFILE {
	PROFILE_0,		// 00: Profile 0.  8-bit 4:2:0 only.
	PROFILE_1,		// 10: Profile 1.  8-bit 4:4:4, 4:2:2, and 4:4:0.
	PROFILE_2,		// 01: Profile 2.  10-bit and 12-bit color only, with 4:2:0 sampling.
	PROFILE_3,		// 110: Profile 3. 10-bit and 12-bit color only, with 4:2:2/4:4:4/4:4:0
};

enum BLOCK_SIZE {
	BLOCK_4X4			= 0,
	BLOCK_4X8			= 1,
	BLOCK_8X4			= 2,
	BLOCK_8X8			= 3,
	BLOCK_8X16			= 4,
	BLOCK_16X8			= 5,
	BLOCK_16X16			= 6,
	BLOCK_16X32			= 7,
	BLOCK_32X16			= 8,
	BLOCK_32X32			= 9,
	BLOCK_32X64			= 10,
	BLOCK_64X32			= 11,
	BLOCK_64X64			= 12,
	BLOCK_SIZES			= 13,
	BLOCK_INVALID		= BLOCK_SIZES,
};

BLOCK_SIZE get_plane_block_size(BLOCK_SIZE bsize, int xss, int yss);

enum FRAME_TYPE {
	KEY_FRAME			= 0,
	INTER_FRAME			= 1,
	FRAME_TYPES			= 2,
	// dummy types for encoding
	KEYLIKE_FRAME		= FRAME_TYPES,
	ALTREF_FRAME,
	INTRA_FRAME,
};

enum REFERENCE_MODE {
	REFMODE_SINGLE		= 0,
	REFMODE_COMPOUND	= 1,
	REFMODE_SELECT		= 2,
	REFMODES			= 3,
};

enum REFERENCE_FRAME {
//	REFFRAME_NONE		= -1,
	REFFRAME_INTRA		=  0,
	REFFRAME_LAST		=  1,
	REFFRAME_GOLDEN		=  2,
	REFFRAME_ALTREF		=  3,
	REFFRAMES			=  4,
};

enum PARTITION_TYPE {
	PARTITION_NONE,
	PARTITION_HORZ,
	PARTITION_VERT,
	PARTITION_SPLIT,
	PARTITION_TYPES,
	PARTITION_INVALID	= PARTITION_TYPES
};

// block transform size
enum TX_SIZE {
	TX_4X4				= 0,	// 4x4 transform
	TX_8X8				= 1,	// 8x8 transform
	TX_16X16			= 2,	// 16x16 transform
	TX_32X32			= 3,	// 32x32 transform

	TX_64X64			= 4,	// not a valid transform
	TX_SIZES			= 4,
	TX_INVALID			= TX_SIZES,
};

// frame transform mode
enum TX_MODE {
	ONLY_4X4			= 0,	// only 4x4 transform used
	ALLOW_8X8			= 1,	// allow block transform size up to 8x8
	ALLOW_16X16			= 2,	// allow block transform size up to 16x16
	ALLOW_32X32			= 3,	// allow block transform size up to 32x32
	TX_MODE_SELECT		= 4,	// transform specified for each block
	TX_MODES			= 5,
};

enum TX_TYPE {
	DCT_DCT				= 0,	// DCT  in both horizontal and vertical
	ADST_DCT			= 1,	// ADST in vertical, DCT in horizontal
	DCT_ADST			= 2,	// DCT  in vertical, ADST in horizontal
	ADST_ADST			= 3,	// ADST in both directions
	TX_TYPES			= 4
};

enum PLANE_TYPE {
	PLANE_TYPE_Y		= 0,
	PLANE_TYPE_UV		= 1,
	PLANE_TYPES
};

enum PREDICTION_MODE {
	DC_PRED				= 0,	// Average of above and left pixels
	V_PRED				= 1,	// Vertical
	H_PRED				= 2,	// Horizontal
	D45_PRED			= 3,	// Directional 45  deg = round(arctan(1/1) * 180/pi)
	D135_PRED			= 4,	// Directional 135 deg = 180 - 45
	D117_PRED			= 5,	// Directional 117 deg = 180 - 63
	D153_PRED			= 6,	// Directional 153 deg = 180 - 27
	D207_PRED			= 7,	// Directional 207 deg = 180 + 27
	D63_PRED			= 8,	// Directional 63  deg = round(arctan(2/1) * 180/pi)
	TM_PRED				= 9,	// True-motion

	NEARESTMV			= 10,
	NEARMV				= 11,
	ZEROMV				= 12,
	NEWMV				= 13,
	MB_MODE_COUNT		= 14,

	INTRA_MODES			= TM_PRED + 1,
	INTER_MODES			= 1 + NEWMV - NEARESTMV,

	DC_NO_L				= INTRA_MODES,
	DC_NO_A,
	DC_128,
};

// The codec can operate in four possible inter prediction filter modes: 8-tap, 8-tap-smooth, 8-tap-sharp, and switching between the three.
enum INTERP_FILTER {
	INTERP_8TAP			= 0,
	INTERP_8TAP_SMOOTH	= 1,
	INTERP_8TAP_SHARP	= 2,
	INTERP_BILINEAR		= 3,
	INTERP_SWITCHABLE	= 4,

	SWITCHABLE_FILTERS	= 3, // Number of switchable filters
};

// Coefficient token alphabet
enum TOKEN {
	ZERO_TOKEN			= 0,	// 0		Extra Bits 0+0
	ONE_TOKEN			= 1,	// 1		Extra Bits 0+1
	TWO_TOKEN			= 2,	// 2		Extra Bits 0+1
	THREE_TOKEN			= 3,	// 3		Extra Bits 0+1
	FOUR_TOKEN			= 4,	// 4		Extra Bits 0+1
	CATEGORY1_TOKEN		= 5,	// 5-6		Extra Bits 1+1
	CATEGORY2_TOKEN		= 6,	// 7-10		Extra Bits 2+1
	CATEGORY3_TOKEN		= 7,	// 11-18	Extra Bits 3+1
	CATEGORY4_TOKEN		= 8,	// 19-34	Extra Bits 4+1
	CATEGORY5_TOKEN		= 9,	// 35-66	Extra Bits 5+1
	CATEGORY6_TOKEN		= 10,	// 67+		Extra Bits 14+1
	EOB_TOKEN			= 11,	// EOB		Extra Bits 0+0
	ENTROPY_TOKENS		= 12,
	ENTROPY_NODES		= 11,

	EOB_CONTEXT_NODE	= 0,
	ZERO_CONTEXT_NODE	= 1,
	ONE_CONTEXT_NODE	= 2,
	UNCONSTRAINED_NODES	= 3,

	EOB_MODEL_TOKEN		= 3,

	COEFF_PROB_MODELS	= 255,

	PIVOT_NODE			= 2,	// which node is pivot
	MODEL_NODES			= ENTROPY_NODES - UNCONSTRAINED_NODES,
};

extern const uint8	b_width_log2_lookup[BLOCK_SIZES];
extern const uint8	b_height_log2_lookup[BLOCK_SIZES];

extern const prob kf_y_mode_prob[INTRA_MODES][INTRA_MODES][INTRA_MODES - 1];
extern const prob kf_uv_mode_prob[INTRA_MODES][INTRA_MODES - 1];
extern const prob pareto8_full[COEFF_PROB_MODELS][MODEL_NODES];

struct ScaleFactors {
	enum {
		FILTER_BITS		= 7,
		SUBPEL_BITS		= 4,
		SUBPEL_MASK		= (1 << SUBPEL_BITS) - 1,
		SUBPEL_SHIFTS	= 1 << SUBPEL_BITS,
		SUBPEL_TAPS		= 8,

		SHIFT			= 14,
		INVALID			= -1
	};
	typedef int16	kernel[SUBPEL_TAPS];
	typedef void (*convolve_fn_t)(
		const uint8 *src, ptrdiff_t src_stride,
		uint8 *dst, ptrdiff_t dst_stride,
		const kernel *kern,
		int x0_q4, int x_step_q4,
		int y0_q4, int y_step_q4,
		int w, int h
	);

	int		x_scale_fp;	// horizontal fixed point scale factor
	int		y_scale_fp;	// vertical fixed point scale factor

	convolve_fn_t predict[2][2][2];  // horiz, vert, avg

	static int get_fixed_point_scale_factor(int other_size, int this_size) {
		return (other_size << SHIFT) / this_size;
	}
	static bool valid_ref_frame_size(int ref_width, int ref_height, int this_width, int this_height) {
		return 2 * this_width >= ref_width && 2 * this_height >= ref_height && this_width <= 16 * ref_width && this_height <= 16 * ref_height;
	}

	int		x_step_q4()			const { return x_scale_fp >> (SHIFT - SUBPEL_BITS); }
	int		y_step_q4()			const { return x_scale_fp >> (SHIFT - SUBPEL_BITS); }
	int		scaled_x(int val)	const { return int((int64)val * x_scale_fp >> SHIFT); }
	int		scaled_y(int val)	const { return int((int64)val * y_scale_fp >> SHIFT); }
	bool	is_valid()			const { return x_scale_fp != INVALID && y_scale_fp != INVALID; }
	bool	is_scaled()			const { return is_valid() && (x_scale_fp != 1 << SHIFT || y_scale_fp != 1 << SHIFT); }

	void	init(int other_w, int other_h, int this_w, int this_h);

	void inter_predictor(const uint8 *src, int src_stride, uint8 *dst, int dst_stride,
		const int subpel_x, const int subpel_y,
		int w, int h, int ref,
		const kernel *kern
	) const {
		predict[subpel_x != 0][subpel_y != 0][ref](
			src, src_stride, dst, dst_stride,
			kern,
			subpel_x, x_step_q4(), subpel_y, y_step_q4(),
			w, h
		);
	}

	friend int scaled_buffer_offset(const ScaleFactors *sf, int x_offset, int y_offset, int stride) {
		const int x = sf ? sf->scaled_x(x_offset) : x_offset;
		const int y = sf ? sf->scaled_y(y_offset) : y_offset;
		return y * stride + x;
	}
};

struct Position {
	int32 row, col;
};

struct MotionVector32 {
	int32 row, col;
	MotionVector32() {}
	MotionVector32(int32 _row, int32 _col) : row(_row), col(_col) {}
	MotionVector32&	clamp(int min_col, int max_col, int min_row, int max_row) {
		col = iso::clamp(col, min_col, max_col);
		row = iso::clamp(row, min_row, max_row);
		return *this;
	}
	MotionVector32	scale(const ScaleFactors &sf) const {
		return MotionVector32(sf.scaled_y(row), sf.scaled_x(col));
	}
};

struct MotionVector {
	enum PRECISION {
		PRECISION_Q3,
		PRECISION_Q4
	};
	enum JOINT_TYPE {
		JOINT_ZERO		= 0,	// Zero vector
		JOINT_HNZVZ		= 1,	// Vert zero, hor nonzero
		JOINT_HZVNZ		= 2,	// Hor zero, vert nonzero
		JOINT_HNZVNZ	= 3,	// Both components nonzero
		JOINTS			= 4,
	};
	// Symbols for coding magnitude class of nonzero components
	enum CLASS_TYPE {
		CLASS_0			= 0,	// (0, 2]		integer pel
		CLASS_1			= 1,	// (2, 4]		integer pel
		CLASS_2			= 2,	// (4, 8]		integer pel
		CLASS_3			= 3,	// (8, 16]		integer pel
		CLASS_4			= 4,	// (16, 32]		integer pel
		CLASS_5			= 5,	// (32, 64]		integer pel
		CLASS_6			= 6,	// (64, 128]	integer pel
		CLASS_7			= 7,	// (128, 256]	integer pel
		CLASS_8			= 8,	// (256, 512]	integer pel
		CLASS_9			= 9,	// (512, 1024]	integer pel
		CLASS_10		= 10,	// (1024,2048]	integer pel
		CLASSES			= 11,
	};
	enum {
		CLASS0_BITS		= 1,  // bits at integer precision for class 0
		CLASS0_SIZE		= 1 << CLASS0_BITS,
		OFFSET_BITS		= CLASSES + CLASS0_BITS - 2,
		MAX_BITS		= CLASSES + CLASS0_BITS + 2,
		MAX				= (1 << MAX_BITS) - 1,
		VALS			= ((MAX << 1) + 1),
		IN_USE_BITS		= 14,
		UPP				= (1 << IN_USE_BITS) - 1,
		LOW				= -(1 << IN_USE_BITS),
		FP_SIZE			= 4,
	};
	union {
		struct { int16 row, col; };
		uint32	u;
	};

	friend bool			has_vertical(JOINT_TYPE type)	{ return !!(type & 2); }
	friend bool			has_horizontal(JOINT_TYPE type)	{ return !!(type & 1); }
	friend int			class_base(CLASS_TYPE c)		{ return c ? CLASS0_SIZE << (c + 2) : 0; }

	static inline int	round_q4(int value)				{ return (value < 0 ? value - 2 : value + 2) / 4; }
	static inline int	round_q2(int value)				{ return (value < 0 ? value - 1 : value + 1) / 2; }
	static CLASS_TYPE	get_class(int z, int *offset) {
		const CLASS_TYPE c = (z >= CLASS0_SIZE * 4096) ? CLASS_10 : (CLASS_TYPE)log2_floor(z >> 3);
		if (offset)
			*offset = z - class_base(c);
		return c;
	}

	MotionVector()	{}
	MotionVector(int16 _row, int16 _col) : row(_row), col(_col) {}

	void			clear()										{ u = 0; }
	bool			is_zero()							const	{ return u == 0; }
	bool			use_hp()							const	{ return max(abs(row), abs(col)) < (8 << 3); }
	bool			operator==(const MotionVector &b)	const	{ return u == b.u; }
	bool			operator!=(const MotionVector &b)	const	{ return u != b.u; }
	MotionVector	operator-()							const	{ return MotionVector(-row, -col); }
	MotionVector&	operator+=(const MotionVector &b)			{ row += b.row; col += b.col; return *this; }
	MotionVector&	operator*=(int s)							{ row *= s; col *= s; return *this; }

	MotionVector&	clamp(int min_col, int max_col, int min_row, int max_row) {
		col = iso::clamp(col, min_col, max_col);
		row = iso::clamp(row, min_row, max_row);
		return *this;
	}
	MotionVector&	lower_precision(bool allow_hp) {
		if (!allow_hp || !use_hp()) {
			if (row & 1)
				row += row > 0 ? -1 : 1;
			if (col & 1)
				col += col > 0 ? -1 : 1;
		}
		return *this;
	}
	JOINT_TYPE		get_joint() const {
		return row == 0
			? (col == 0 ? JOINT_ZERO : JOINT_HNZVZ)
			: (col == 0 ? JOINT_HZVNZ : JOINT_HNZVNZ);
	}
	friend MotionVector		operator+(const MotionVector &a, const MotionVector &b)	{ return MotionVector(a.row + b.row, a.col + b.col); }
	friend MotionVector		operator-(const MotionVector &a, const MotionVector &b)	{ return MotionVector(a.row - b.row, a.col - b.col); }
	friend MotionVector		operator*(const MotionVector &a, int s)					{ return MotionVector(a.row * s, a.col * s); }
	friend MotionVector32	shift(const MotionVector &a, int sx, int sy)			{ return MotionVector32(a.row << sy, a.col << sx); }
	friend bool				is_valid(const MotionVector &a)							{ return a.row > LOW && a.row < UPP && a.col > LOW && a.col < UPP; }
	friend MotionVector	average(const MotionVector &a, const MotionVector &b)	{
		return MotionVector(round_q2(a.row + b.row), round_q2(a.col + b.col));
	}
	friend MotionVector		average(const MotionVector &a, const MotionVector &b, const MotionVector &c, const MotionVector &d) {
		return MotionVector(round_q4(a.row + b.row + c.row + d.row), round_q2(a.col + b.col + c.col + d.col));
	}
	friend MotionVector		flip(const MotionVector &a, bool doflip) {
		return doflip ? -a : a;
	}
};

struct MotionVectorPair {
	MotionVector	mv[2];
	void clear() { mv[0].clear(); mv[1].clear(); }
};

struct MotionVectorRef {
	enum {
		NEIGHBOURS		= 8,
		MAX_CANDIDATES	= 2,
		BORDER			= 16 << 3,  // Allow 16 pels in 1/8th pel units
	};
	MotionVector		mv[2];
	REFERENCE_FRAME		ref_frame[2];
};

template<typename T> struct Bands {
	enum {
		BANDS		= 6,
		CONTEXTS0	= 3,
		CONTEXTS	= 6,
		TOTAL		= CONTEXTS0 + (BANDS - 1) * CONTEXTS,
	};
	T	band0[CONTEXTS0];
	T	bands[BANDS - 1][CONTEXTS];
	T*				operator[](int i)			{ return i == 0 ? band0 : bands[i - 1]; }
	const T*		operator[](int i)	const	{ return i == 0 ? band0 : bands[i - 1]; }
	T*				all()						{ return band0; }
	const T*		all()				const	{ return band0; }
	T&				all(int i)					{ return band0[i]; }
	const T&		all(int i)			const	{ return band0[i]; }
};

// 128 lists of probabilities are stored for the following ONE node probs:
// 1, 3, 5, 7, ..., 253, 255
// In between probabilities are interpolated linearly

struct ScanOrder {
	const int16 *scan;
	const int16 *iscan;
	const int16 *neighbors;
};

struct ColorSpace {
	enum Space {
		UNKNOWN		= 0,  // Unknown
		BT_601		= 1,  // BT.601
		BT_709		= 2,  // BT.709
		SMPTE_170	= 3,  // SMPTE.170
		SMPTE_240	= 4,  // SMPTE.240
		BT_2020		= 5,  // BT.2020
		RESERVED	= 6,  // Reserved
		SRGB		= 7   // sRGB
	};
	enum Range {
		STUDIO_RANGE = 0,    // Y [16..235], UV [16..240]
		FULL_RANGE   = 1     // YUV/RGB [0..255]
	};
	Space		space;
	Range		range;
	uint8		bit_depth;
	uint8		subsampling_x, subsampling_y;

	void		init() {
		space			= BT_601;
		range			= STUDIO_RANGE;
		subsampling_y	= subsampling_x = 1;
		bit_depth		= 8;
	}
};

struct Buffer2D {
	uint8		*buffer;
	int			stride;
	uint32		texture;

	Buffer2D() : buffer(0), stride(0), texture(0) {}
	void	init(uint8 *_buffer, int _stride) {
		buffer	= _buffer;
		stride	= _stride;
	}
	uint8	*row(int y) const {
		return buffer + stride * y;
	}
	friend void copy_buffer(const Buffer2D &dest, const Buffer2D &srce, int w, int h);
};

struct FrameBuffer : refs<FrameBuffer>, ColorSpace {
friend class ref_ptr<FrameBuffer>;
protected:
	void	release()	{
		ISO_ASSERT(nrefs != 0);
		--nrefs;
	}
	static int next_id() { static int id = 0; return id++; }
public:
	uint32	texture;

	struct Plane {
		int	width, height;
		int	crop_x, crop_y, crop_width, crop_height;

		Plane() : width(0), height(0) {}

		void		init(int _width, int _height, int xborder, int yborder) {
			crop_width	= _width;
			crop_height	= _height;
			crop_x		= xborder;
			crop_y		= yborder;
			width		= _width  + xborder * 2;
			height		= _height + yborder * 2;
		}
		ptrdiff_t	offset(int stride)	const { return stride * crop_y + crop_x; }
		void		extend(const Buffer2D &src, int extend_top, int extend_left, int extend_bottom, int extend_right) const;
		void		extend(const Buffer2D &src)  const {
			extend(src, crop_y, crop_x, height - crop_height - crop_y, width - crop_width - crop_x);
		}
		void		copy_extend(const Buffer2D &src, const Buffer2D &dst, int extend_top, int extend_left, int extend_bottom, int extend_right) const;
		void		copy_extend(const Buffer2D &src, const Buffer2D &dst) const {
			copy_extend(src, dst, crop_y, crop_x, height - crop_height - crop_y, width - crop_width - crop_x);
		}
		friend bool equal_dimensions(const Plane &a, const Plane &b) { return a.height == b.height && a.width == b.width; }
	};
	int				id;
	Plane			y, uv;
	Buffer2D		y_buffer, u_buffer, v_buffer;
	int				render_width, render_height;
	void			*mem_buffer;
	size_t			mem_size;

	FrameBuffer() : texture(0), id(next_id()), mem_buffer(0), mem_size(0) {}
	~FrameBuffer() { aligned_free(mem_buffer); }


	const Plane&	plane(int i)	const { return i == 0 ? y : uv; }
	const Buffer2D&	buffer(int i)	const { return (&y_buffer)[i]; }

	int			resize(int width, int height, const ColorSpace &cs, int border, int buffer_alignment, int stride_alignment, bool gpu);

	void		extend() const;
	void		copy_extend(const FrameBuffer &src) const;

	friend void	scale_and_extend_frame(const FrameBuffer &src, FrameBuffer &dst);
	friend void	scale_and_extend_frame_nonnormative(const FrameBuffer &src, FrameBuffer &dst);

	friend bool equal_dimensions(const FrameBuffer &a, const FrameBuffer &b) {
		return equal_dimensions(a.y, b.y) && equal_dimensions(a.uv, b.uv);
	}
	bool		in_use() const	{ return nrefs > 0; }
};

struct ModeInfo {

	// Common for both INTER and INTRA blocks
	BLOCK_SIZE			sb_type;
	PREDICTION_MODE		mode;
	TX_SIZE				tx_size;
	bool				skip;
	int8				segment_id;
	int8				seg_id_predicted;  // valid only when temporal_update is enabled
	REFERENCE_FRAME		ref_frame[2];

	union {
		struct {	// Only for INTRA blocks
			PREDICTION_MODE		uv_mode;
			PREDICTION_MODE		sub_mode[4];
		};
		struct {	// Only for INTER blocks
			INTERP_FILTER		interp_filter;
			MotionVectorPair	sub_mv[4];  // first, second inter predictor motion vectors
		};
	};

	static TX_SIZE	get_max_txsize(BLOCK_SIZE bsize) {
		static const TX_SIZE max_txsize_lookup[] = {
			TX_4X4, TX_4X4, TX_4X4,	TX_8X8, TX_8X8, TX_8X8,	TX_16X16, TX_16X16, TX_16X16, TX_32X32, TX_32X32, TX_32X32, TX_32X32, TX_INVALID
		};
		return max_txsize_lookup[bsize];
	}

	ModeInfo()	{}
	int		width()				const { return 1 << b_width_log2_lookup[sb_type]; }
	int		height()			const { return 1 << b_width_log2_lookup[sb_type]; }
	bool	is_inter_block()	const { return ref_frame[0] > REFFRAME_INTRA; }
	bool	has_second_ref()	const { return ref_frame[1] > REFFRAME_INTRA; }

	TX_SIZE	get_uv_tx_size(int xss, int yss) const {
		return sb_type < BLOCK_8X8 ? TX_4X4 : (TX_SIZE)min(tx_size, get_max_txsize(get_plane_block_size(sb_type, xss, yss)));
	}

	// Only for INTRA blocks
	PREDICTION_MODE get_y_mode(int block)	const {
		return sb_type < BLOCK_8X8 ? sub_mode[block] : mode;
	}
	PREDICTION_MODE left_block_mode(const ModeInfo *left_mi, int b) const {
		return !(b & 1)
			? (!left_mi || left_mi->is_inter_block() ? DC_PRED : left_mi->get_y_mode(b + 1))
			: sub_mode[b - 1];
	}
	PREDICTION_MODE above_block_mode(const ModeInfo *above_mi, int b) const {
		return !(b & 2)
			? (!above_mi || above_mi->is_inter_block() ? DC_PRED : above_mi->get_y_mode(b + 2))
			: sub_mode[b - 2];
	}
	const prob*		get_y_mode_probs(const ModeInfo *above_mi, const ModeInfo *left_mi, int block) const {
		return kf_y_mode_prob[above_block_mode(above_mi, block)][left_block_mode(left_mi, block)];
	}

	// Only for INTER blocks
	MotionVector	average_mvs(int ref, int block, int ss) const {
		switch (ss) {
			default:	return sub_mv[block].mv[ref];
			case 1:		return average(sub_mv[block].mv[ref], sub_mv[block + 2].mv[ref]);
			case 2:		return average(sub_mv[block].mv[ref], sub_mv[block + 1].mv[ref]);
			case 3:		return average(sub_mv[0].mv[ref], sub_mv[1].mv[ref], sub_mv[2].mv[ref], sub_mv[3].mv[ref]);
		}
	}
	MotionVector	get_sub_mv(int ref, int search_col, int block) const {
		static const uint8 idx_n_column_to_subblock[4][2] = {{2, 1},{3, 1},{2, 3},{3, 3}};
		return sub_mv[idx_n_column_to_subblock[block][search_col != 0]].mv[ref];
	}
	// Performs mv sign inversion if indicated by the reference frame combination.
	MotionVector	scale_mv(int ref, const REFERENCE_FRAME this_ref_frame, const bool *ref_sign_bias) const {
		return flip(sub_mv[3].mv[ref], ref_sign_bias[ref_frame[ref]] != ref_sign_bias[this_ref_frame]);
	}
};

struct LoopFilterMasks {
	uint64		left_y[TX_SIZES];
	uint64		above_y[TX_SIZES];
	uint64		int_4x4_y;
	uint16		left_uv[TX_SIZES];
	uint16		above_uv[TX_SIZES];
	uint16		int_4x4_uv;
	uint8		level_y[64];

	void	adjust_mask(const int mi_row, const int mi_col, const int mi_rows, const int mi_cols);
	void	build_mask(const int filter_level, const ModeInfo *mi, int row_in_sb, int col_in_sb, int bw, int bh);
	void	clear()	{ iso::clear(*this); }
};

struct LoopFilter {
	enum {
		MAX_LOOP_FILTER			= 63,
		MAX_SHARPNESS			= 7,
		MAX_REF_LF_DELTAS		= 4,
		MAX_MODE_LF_DELTAS		= 2,
	};

	int		filter_level;
	int		sharpness_level,	last_sharpness_level;
	bool	mode_ref_delta_enabled;
	bool	mode_ref_delta_update;

	// 0 = Intra, Last, GF, ARF
	int8	ref_deltas[MAX_REF_LF_DELTAS];
	int8	last_ref_deltas[MAX_REF_LF_DELTAS];

	// 0 = ZERO_MV, MV
	int8	mode_deltas[MAX_MODE_LF_DELTAS];
	int8	last_mode_deltas[MAX_MODE_LF_DELTAS];

	LoopFilter() : filter_level(0), sharpness_level(0), last_sharpness_level(0) {}

	void reset() {
		mode_ref_delta_enabled		= true;
		mode_ref_delta_update		= true;

		ref_deltas[REFFRAME_INTRA]	= 1;
		ref_deltas[REFFRAME_LAST]	= 0;
		ref_deltas[REFFRAME_GOLDEN]	= -1;
		ref_deltas[REFFRAME_ALTREF]	= -1;

		mode_deltas[0]	= 0;
		mode_deltas[1]	= 0;

		clear(last_ref_deltas);
		clear(last_mode_deltas);

		last_sharpness_level = -1;
	}
};

struct Segmentation {
	enum FEATURE {
		FEATURE_ALT_Q		= 0,		// Use alternate Quantizer ....
		FEATURE_ALT_LF		= 1,		// Use alternate loop filter value...
		FEATURE_REF_FRAME	= 2,		// Optional Segment reference frame
		FEATURE_SKIP		= 3,		// Optional Segment (0,0) + skip mode
		FEATURES			= 4			// Number of features supported
	};

	template<FEATURE id> struct traits;

	bool		enabled;
	bool		update_map;
	bool		update_data;
	bool		abs_delta;
	bool		temporal_update;

	prob		tree_probs[MAX_SEGMENTS - 1];
	prob		pred_probs[PREDICTION_PROBS];
	int16		data[MAX_SEGMENTS][FEATURES];
	uint32		mask[MAX_SEGMENTS];

	static int data_max(FEATURE id) {
		static const int table[] = { MAXQ, LoopFilter::MAX_LOOP_FILTER, 3, 0 };
		return table[id];
	}
	static bool is_signed(FEATURE id) {
		static const bool table[] = { true, true, false, false };
		return table[id];
	}
	void	reset()												{ clearall(); abs_delta = false; }
	void	clearall()											{ clear(data); clear(mask); }
	void	enable()											{ enabled = update_map = update_data = true; }
	void	disable()											{ enabled = update_map = update_data = false; }
	void	enable(int segment_id, FEATURE id)					{ mask[segment_id] |= 1 << id;	}
	void	disable(int segment_id, FEATURE id)					{ mask[segment_id] &= ~(1 << id);	}
	int		active(int segment_id, FEATURE id) const			{ return enabled && (mask[segment_id] & (1 << id)); }
	void	set_data(int segment_id, FEATURE id, int seg_data)	{ data[segment_id][id] = seg_data; }
	void	clear_data(int segment_id, FEATURE id)				{ data[segment_id][id] = 0; }
	int		get_data(int segment_id, FEATURE id) const			{ return data[segment_id][id]; }

	int		get_data(int segment_id, FEATURE id, int base) const {
		return active(segment_id, id)
			? clamp(get_data(segment_id, id) + (abs_delta ? 0 : base), 0, data_max(id))
			: base;
	}
};


struct LoopFilterInfo {
	struct Thresh {
		uint8		blimit[16];
		uint8		limit[16];
		uint8		hev_thr[16];
	};
	Thresh	thresh[LoopFilter::MAX_LOOP_FILTER + 1];
	uint8	level[MAX_SEGMENTS][REFFRAMES][LoopFilter::MAX_MODE_LF_DELTAS];

	LoopFilterInfo() {
		// init hev threshold const vectors
		for (int level = 0; level <= LoopFilter::MAX_LOOP_FILTER; level++)
			memset(thresh[level].hev_thr, level >> 4, 16);
		update_sharpness(0);
	}

	void update_sharpness(int sharpness_lvl) {
		// For each possible value for the loop filter fill out limits
		for (int level = 0; level <= LoopFilter::MAX_LOOP_FILTER; level++) {
			int block_inside_limit = max(level >> ((sharpness_lvl > 0) + (sharpness_lvl > 4)), 1);
			if (sharpness_lvl > 0 && block_inside_limit > (9 - sharpness_lvl))
				block_inside_limit = 9 - sharpness_lvl;
			memset(thresh[level].limit, block_inside_limit, 16);
			memset(thresh[level].blimit, (2 * (level + 2) + block_inside_limit), 16);
		}
	}

	uint8 get_filter_level(const ModeInfo *mi) const {
		static const int table[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1};
		return level[mi->segment_id][mi->ref_frame[0]][table[mi->mode]];
	}

	void filter_selectively_vert_row2(uint8 *s, int pitch, int lfl_forward, uint32 mask_16x16, uint32 mask_8x8, uint32 mask_4x4, uint32 mask_4x4_int, const uint8 *lfl) const;
	void filter_selectively_horiz(uint8 *s, int pitch, uint32 mask_16x16, uint32 mask_8x8, uint32 mask_4x4, uint32 mask_4x4_int, const uint8 *lfl) const;
	void filter_selectively_vert(uint8 *s, int pitch, uint32 mask_16x16, uint32 mask_8x8, uint32 mask_4x4, uint32 mask_4x4_int, const uint8 *lfl) const;

	void filter_block_plane_non420(const Buffer2D dst, int ssx, int ssy, ModeInfo **mi_8x8, int mi_row, int mi_col, const int mi_rows, const int mi_cols, const int mi_stride) const;
	void filter_block_plane_ss00(const Buffer2D dst, int mi_row, LoopFilterMasks *lfm, const int mi_rows, const int mi_cols, const int mi_stride) const;
	void filter_block_plane_ss11(const Buffer2D dst, int mi_row, LoopFilterMasks *lfm, const int mi_rows, const int mi_cols, const int mi_stride) const;
};

struct Quantisation {
	int		base_index;
	int		y_dc_delta;
	int		uv_dc_delta;
	int		uv_ac_delta;
	int16	dequant[PLANE_TYPES][MAX_SEGMENTS][2];
	uint8	bit_depth;

	bool	is_lossless() {
		return base_index == 0
			&& y_dc_delta == 0
			&& uv_dc_delta == 0
			&& uv_ac_delta == 0;
	}
	static const int16*	dc_quant_table(int bit_depth) {
		static const int16 dc_qlookup[] = {
			   4,    8,    8,    9,   10,   11,   12,   12,
			  13,   14,   15,   16,   17,   18,   19,   19,
			  20,   21,   22,   23,   24,   25,   26,   26,
			  27,   28,   29,   30,   31,   32,   32,   33,
			  34,   35,   36,   37,   38,   38,   39,   40,
			  41,   42,   43,   43,   44,   45,   46,   47,
			  48,   48,   49,   50,   51,   52,   53,   53,
			  54,   55,   56,   57,   57,   58,   59,   60,
			  61,   62,   62,   63,   64,   65,   66,   66,
			  67,   68,   69,   70,   70,   71,   72,   73,
			  74,   74,   75,   76,   77,   78,   78,   79,
			  80,   81,   81,   82,   83,   84,   85,   85,
			  87,   88,   90,   92,   93,   95,   96,   98,
			  99,  101,  102,  104,  105,  107,  108,  110,
			 111,  113,  114,  116,  117,  118,  120,  121,
			 123,  125,  127,  129,  131,  134,  136,  138,
			 140,  142,  144,  146,  148,  150,  152,  154,
			 156,  158,  161,  164,  166,  169,  172,  174,
			 177,  180,  182,  185,  187,  190,  192,  195,
			 199,  202,  205,  208,  211,  214,  217,  220,
			 223,  226,  230,  233,  237,  240,  243,  247,
			 250,  253,  257,  261,  265,  269,  272,  276,
			 280,  284,  288,  292,  296,  300,  304,  309,
			 313,  317,  322,  326,  330,  335,  340,  344,
			 349,  354,  359,  364,  369,  374,  379,  384,
			 389,  395,  400,  406,  411,  417,  423,  429,
			 435,  441,  447,  454,  461,  467,  475,  482,
			 489,  497,  505,  513,  522,  530,  539,  549,
			 559,  569,  579,  590,  602,  614,  626,  640,
			 654,  668,  684,  700,  717,  736,  755,  775,
			 796,  819,  843,  869,  896,  925,  955,  988,
			1022, 1058, 1098, 1139, 1184, 1232, 1282, 1336,
		};
		static const int16 dc_qlookup_10[] = {
			    4,     9,    10,    13,    15,    17,    20,    22,
			   25,    28,    31,    34,    37,    40,    43,    47,
			   50,    53,    57,    60,    64,    68,    71,    75,
			   78,    82,    86,    90,    93,    97,   101,   105,
			  109,   113,   116,   120,   124,   128,   132,   136,
			  140,   143,   147,   151,   155,   159,   163,   166,
			  170,   174,   178,   182,   185,   189,   193,   197,
			  200,   204,   208,   212,   215,   219,   223,   226,
			  230,   233,   237,   241,   244,   248,   251,   255,
			  259,   262,   266,   269,   273,   276,   280,   283,
			  287,   290,   293,   297,   300,   304,   307,   310,
			  314,   317,   321,   324,   327,   331,   334,   337,
			  343,   350,   356,   362,   369,   375,   381,   387,
			  394,   400,   406,   412,   418,   424,   430,   436,
			  442,   448,   454,   460,   466,   472,   478,   484,
			  490,   499,   507,   516,   525,   533,   542,   550,
			  559,   567,   576,   584,   592,   601,   609,   617,
			  625,   634,   644,   655,   666,   676,   687,   698,
			  708,   718,   729,   739,   749,   759,   770,   782,
			  795,   807,   819,   831,   844,   856,   868,   880,
			  891,   906,   920,   933,   947,   961,   975,   988,
			 1001,  1015,  1030,  1045,  1061,  1076,  1090,  1105,
			 1120,  1137,  1153,  1170,  1186,  1202,  1218,  1236,
			 1253,  1271,  1288,  1306,  1323,  1342,  1361,  1379,
			 1398,  1416,  1436,  1456,  1476,  1496,  1516,  1537,
			 1559,  1580,  1601,  1624,  1647,  1670,  1692,  1717,
			 1741,  1766,  1791,  1817,  1844,  1871,  1900,  1929,
			 1958,  1990,  2021,  2054,  2088,  2123,  2159,  2197,
			 2236,  2276,  2319,  2363,  2410,  2458,  2508,  2561,
			 2616,  2675,  2737,  2802,  2871,  2944,  3020,  3102,
			 3188,  3280,  3375,  3478,  3586,  3702,  3823,  3953,
			 4089,  4236,  4394,  4559,  4737,  4929,  5130,  5347,
		};
		static const int16 dc_qlookup_12[] = {
			    4,    12,    18,    25,    33,    41,    50,    60,
			   70,    80,    91,   103,   115,   127,   140,   153,
			  166,   180,   194,   208,   222,   237,   251,   266,
			  281,   296,   312,   327,   343,   358,   374,   390,
			  405,   421,   437,   453,   469,   484,   500,   516,
			  532,   548,   564,   580,   596,   611,   627,   643,
			  659,   674,   690,   706,   721,   737,   752,   768,
			  783,   798,   814,   829,   844,   859,   874,   889,
			  904,   919,   934,   949,   964,   978,   993,  1008,
			 1022,  1037,  1051,  1065,  1080,  1094,  1108,  1122,
			 1136,  1151,  1165,  1179,  1192,  1206,  1220,  1234,
			 1248,  1261,  1275,  1288,  1302,  1315,  1329,  1342,
			 1368,  1393,  1419,  1444,  1469,  1494,  1519,  1544,
			 1569,  1594,  1618,  1643,  1668,  1692,  1717,  1741,
			 1765,  1789,  1814,  1838,  1862,  1885,  1909,  1933,
			 1957,  1992,  2027,  2061,  2096,  2130,  2165,  2199,
			 2233,  2267,  2300,  2334,  2367,  2400,  2434,  2467,
			 2499,  2532,  2575,  2618,  2661,  2704,  2746,  2788,
			 2830,  2872,  2913,  2954,  2995,  3036,  3076,  3127,
			 3177,  3226,  3275,  3324,  3373,  3421,  3469,  3517,
			 3565,  3621,  3677,  3733,  3788,  3843,  3897,  3951,
			 4005,  4058,  4119,  4181,  4241,  4301,  4361,  4420,
			 4479,  4546,  4612,  4677,  4742,  4807,  4871,  4942,
			 5013,  5083,  5153,  5222,  5291,  5367,  5442,  5517,
			 5591,  5665,  5745,  5825,  5905,  5984,  6063,  6149,
			 6234,  6319,  6404,  6495,  6587,  6678,  6769,  6867,
			 6966,  7064,  7163,  7269,  7376,  7483,  7599,  7715,
			 7832,  7958,  8085,  8214,  8352,  8492,  8635,  8788,
			 8945,  9104,  9275,  9450,  9639,  9832, 10031, 10245,
			10465, 10702, 10946, 11210, 11482, 11776, 12081, 12409,
			12750, 13118, 13501, 13913, 14343, 14807, 15290, 15812,
			16356, 16943, 17575, 18237, 18949, 19718, 20521, 21387,
		};
		return bit_depth == 8 ? dc_qlookup : bit_depth == 10 ? dc_qlookup_10 : dc_qlookup_12;
	}
	static const int16*	ac_quant_table(int bit_depth) {
		static const int16 ac_qlookup[] = {
			   4,    8,    9,   10,   11,   12,   13,   14,
			  15,   16,   17,   18,   19,   20,   21,   22,
			  23,   24,   25,   26,   27,   28,   29,   30,
			  31,   32,   33,   34,   35,   36,   37,   38,
			  39,   40,   41,   42,   43,   44,   45,   46,
			  47,   48,   49,   50,   51,   52,   53,   54,
			  55,   56,   57,   58,   59,   60,   61,   62,
			  63,   64,   65,   66,   67,   68,   69,   70,
			  71,   72,   73,   74,   75,   76,   77,   78,
			  79,   80,   81,   82,   83,   84,   85,   86,
			  87,   88,   89,   90,   91,   92,   93,   94,
			  95,   96,   97,   98,   99,  100,  101,  102,
			 104,  106,  108,  110,  112,  114,  116,  118,
			 120,  122,  124,  126,  128,  130,  132,  134,
			 136,  138,  140,  142,  144,  146,  148,  150,
			 152,  155,  158,  161,  164,  167,  170,  173,
			 176,  179,  182,  185,  188,  191,  194,  197,
			 200,  203,  207,  211,  215,  219,  223,  227,
			 231,  235,  239,  243,  247,  251,  255,  260,
			 265,  270,  275,  280,  285,  290,  295,  300,
			 305,  311,  317,  323,  329,  335,  341,  347,
			 353,  359,  366,  373,  380,  387,  394,  401,
			 408,  416,  424,  432,  440,  448,  456,  465,
			 474,  483,  492,  501,  510,  520,  530,  540,
			 550,  560,  571,  582,  593,  604,  615,  627,
			 639,  651,  663,  676,  689,  702,  715,  729,
			 743,  757,  771,  786,  801,  816,  832,  848,
			 864,  881,  898,  915,  933,  951,  969,  988,
			1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151,
			1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
			1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567,
			1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
		};
		static const int16 ac_qlookup_10[] = {
			    4,     9,    11,    13,    16,    18,    21,    24,
			   27,    30,    33,    37,    40,    44,    48,    51,
			   55,    59,    63,    67,    71,    75,    79,    83,
			   88,    92,    96,   100,   105,   109,   114,   118,
			  122,   127,   131,   136,   140,   145,   149,   154,
			  158,   163,   168,   172,   177,   181,   186,   190,
			  195,   199,   204,   208,   213,   217,   222,   226,
			  231,   235,   240,   244,   249,   253,   258,   262,
			  267,   271,   275,   280,   284,   289,   293,   297,
			  302,   306,   311,   315,   319,   324,   328,   332,
			  337,   341,   345,   349,   354,   358,   362,   367,
			  371,   375,   379,   384,   388,   392,   396,   401,
			  409,   417,   425,   433,   441,   449,   458,   466,
			  474,   482,   490,   498,   506,   514,   523,   531,
			  539,   547,   555,   563,   571,   579,   588,   596,
			  604,   616,   628,   640,   652,   664,   676,   688,
			  700,   713,   725,   737,   749,   761,   773,   785,
			  797,   809,   825,   841,   857,   873,   889,   905,
			  922,   938,   954,   970,   986,  1002,  1018,  1038,
			 1058,  1078,  1098,  1118,  1138,  1158,  1178,  1198,
			 1218,  1242,  1266,  1290,  1314,  1338,  1362,  1386,
			 1411,  1435,  1463,  1491,  1519,  1547,  1575,  1603,
			 1631,  1663,  1695,  1727,  1759,  1791,  1823,  1859,
			 1895,  1931,  1967,  2003,  2039,  2079,  2119,  2159,
			 2199,  2239,  2283,  2327,  2371,  2415,  2459,  2507,
			 2555,  2603,  2651,  2703,  2755,  2807,  2859,  2915,
			 2971,  3027,  3083,  3143,  3203,  3263,  3327,  3391,
			 3455,  3523,  3591,  3659,  3731,  3803,  3876,  3952,
			 4028,  4104,  4184,  4264,  4348,  4432,  4516,  4604,
			 4692,  4784,  4876,  4972,  5068,  5168,  5268,  5372,
			 5476,  5584,  5692,  5804,  5916,  6032,  6148,  6268,
			 6388,  6512,  6640,  6768,  6900,  7036,  7172,  7312,
		};
		static const int16 ac_qlookup_12[] = {
			    4,    13,    19,    27,    35,    44,    54,    64,
			   75,    87,    99,   112,   126,   139,   154,   168,
			  183,   199,   214,   230,   247,   263,   280,   297,
			  314,   331,   349,   366,   384,   402,   420,   438,
			  456,   475,   493,   511,   530,   548,   567,   586,
			  604,   623,   642,   660,   679,   698,   716,   735,
			  753,   772,   791,   809,   828,   846,   865,   884,
			  902,   920,   939,   957,   976,   994,  1012,  1030,
			 1049,  1067,  1085,  1103,  1121,  1139,  1157,  1175,
			 1193,  1211,  1229,  1246,  1264,  1282,  1299,  1317,
			 1335,  1352,  1370,  1387,  1405,  1422,  1440,  1457,
			 1474,  1491,  1509,  1526,  1543,  1560,  1577,  1595,
			 1627,  1660,  1693,  1725,  1758,  1791,  1824,  1856,
			 1889,  1922,  1954,  1987,  2020,  2052,  2085,  2118,
			 2150,  2183,  2216,  2248,  2281,  2313,  2346,  2378,
			 2411,  2459,  2508,  2556,  2605,  2653,  2701,  2750,
			 2798,  2847,  2895,  2943,  2992,  3040,  3088,  3137,
			 3185,  3234,  3298,  3362,  3426,  3491,  3555,  3619,
			 3684,  3748,  3812,  3876,  3941,  4005,  4069,  4149,
			 4230,  4310,  4390,  4470,  4550,  4631,  4711,  4791,
			 4871,  4967,  5064,  5160,  5256,  5352,  5448,  5544,
			 5641,  5737,  5849,  5961,  6073,  6185,  6297,  6410,
			 6522,  6650,  6778,  6906,  7034,  7162,  7290,  7435,
			 7579,  7723,  7867,  8011,  8155,  8315,  8475,  8635,
			 8795,  8956,  9132,  9308,  9484,  9660,  9836, 10028,
			10220, 10412, 10604, 10812, 11020, 11228, 11437, 11661,
			11885, 12109, 12333, 12573, 12813, 13053, 13309, 13565,
			13821, 14093, 14365, 14637, 14925, 15213, 15502, 15806,
			16110, 16414, 16734, 17054, 17390, 17726, 18062, 18414,
			18766, 19134, 19502, 19886, 20270, 20670, 21070, 21486,
			21902, 22334, 22766, 23214, 23662, 24126, 24590, 25070,
			25551, 26047, 26559, 27071, 27599, 28143, 28687, 29247,
		};
		return bit_depth == 8 ? ac_qlookup : bit_depth == 10 ? ac_qlookup_10 : ac_qlookup_12;
	}

	static int16	dc_quant(int qindex, int delta, int bit_depth) {
		return dc_quant_table(bit_depth)[clamp(qindex + delta, 0, 255)];
	}
	static int16	ac_quant(int qindex, int delta, int bit_depth) {
		return ac_quant_table(bit_depth)[clamp(qindex + delta, 0, 255)];
	}

	void	set(int i, int index, int bit_depth) {
		dequant[PLANE_TYPE_Y][i][0]		= dc_quant(index, y_dc_delta,	bit_depth);
		dequant[PLANE_TYPE_Y][i][1]		= ac_quant(index, 0,			bit_depth);
		dequant[PLANE_TYPE_UV][i][0]	= dc_quant(index, uv_dc_delta,	bit_depth);
		dequant[PLANE_TYPE_UV][i][1]	= ac_quant(index, uv_ac_delta,	bit_depth);
	}
	// convert 0-63 Q-range value to the Qindex range used internally
	static int quantizer_to_qindex(int quantizer) {
		static const int quantizer_to_qindex[] = {
			0,    4,   8,  12,  16,  20,  24,  28,
			32,   36,  40,  44,  48,  52,  56,  60,
			64,   68,  72,  76,  80,  84,  88,  92,
			96,  100, 104, 108, 112, 116, 120, 124,
			128, 132, 136, 140, 144, 148, 152, 156,
			160, 164, 168, 172, 176, 180, 184, 188,
			192, 196, 200, 204, 208, 212, 216, 220,
			224, 228, 232, 236, 240, 244, 249, 255,
		};
		return quantizer_to_qindex[quantizer];
	}
	static int	qindex_to_quantizer(int qindex) {
		for (int q = 0; q < 64; ++q)
			if (quantizer_to_qindex(q) >= qindex)
				return q;
		return 63;
	}
	// Convert the index to a real Q value (scaled down to match old Q values)
	static int q_scale(int bit_depth) {
		return 1 << (bit_depth - 6);
	}
	static double qindex_to_q(int qindex, int bit_depth) {
		return ac_quant(qindex, 0, bit_depth) / q_scale(bit_depth);
	}
};

enum CONTEXT_SIZES {
	SKIP_CONTEXTS				= 3,
	INTER_MODE_CONTEXTS			= 7,
	INTRA_INTER_CONTEXTS		= 4,
	COMP_INTER_CONTEXTS			= 5,
	REF_CONTEXTS				= 5,
	TX_SIZE_CONTEXTS			= 2,
	PARTITION_CONTEXTS			= TX_SIZES * 4,
	SWITCHABLE_FILTER_CONTEXTS	= SWITCHABLE_FILTERS + 1,

	BLOCK_SIZE_GROUPS			= 4,
	REF_TYPES					= 2,  // intra=0, inter=1
};

struct FrameCounts {
	typedef Bands<uint32[UNCONSTRAINED_NODES + 1]>	coeffs[REF_TYPES];

	struct mvs {
		struct component {
			uint32	sign[2];
			uint32	classes[MotionVector::CLASSES];
			uint32	class0[MotionVector::CLASS0_SIZE];
			uint32	bits[MotionVector::OFFSET_BITS][2];
			uint32	class0_fp[MotionVector::CLASS0_SIZE][MotionVector::FP_SIZE];
			uint32	fp[MotionVector::FP_SIZE];
			uint32	class0_hp[2];
			uint32	hp[2];
		};
		uint32		joints[MotionVector::JOINTS - 1];
		component	comps[2];
	};

	uint32				y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES];
	uint32				uv_mode[INTRA_MODES][INTRA_MODES];
	uint32				partition[PARTITION_CONTEXTS][PARTITION_TYPES];
	uint32				switchable_interp[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
	uint32				inter_mode[INTER_MODE_CONTEXTS][INTER_MODES];
	uint32				intra_inter[INTRA_INTER_CONTEXTS][2];
	uint32				comp_inter[COMP_INTER_CONTEXTS][2];
	uint32				single_ref[REF_CONTEXTS][2][2];
	uint32				comp_ref[REF_CONTEXTS][2];
	uint32				skip[SKIP_CONTEXTS][2];
	uint32				tx_32x32[TX_SIZE_CONTEXTS][TX_SIZES];
	uint32				tx_16x16[TX_SIZE_CONTEXTS][TX_SIZES - 1];
	uint32				tx_8x8[TX_SIZE_CONTEXTS][TX_SIZES - 2];
	uint32				tx_totals[TX_SIZES];

	coeffs				coef[TX_SIZES][PLANE_TYPES];
	Bands<uint32>		eob_branch[TX_SIZES][PLANE_TYPES][REF_TYPES];
	mvs					mv;

	uint32 *get_tx_counts(TX_SIZE max_tx_size, int ctx) {
		switch (max_tx_size) {
			case TX_8X8:	return tx_8x8[ctx];
			case TX_16X16:	return tx_16x16[ctx];
			case TX_32X32:	return tx_32x32[ctx];
			default:		return NULL;
		}
	}
};

struct FrameContext {
	enum {
		DIFF_UPDATE_PROB	= 252,
		MV_UPDATE_PROB		= 252,

		COEF_COUNT_SAT						= 24,
		COEF_MAX_UPDATE_FACTOR				= 112,
		COEF_COUNT_SAT_KEY					= 24,
		COEF_MAX_UPDATE_FACTOR_KEY			= 112,
		COEF_COUNT_SAT_AFTER_KEY			= 24,
		COEF_MAX_UPDATE_FACTOR_AFTER_KEY	= 128,
	};
	typedef Bands<prob[UNCONSTRAINED_NODES]>	coeffs[REF_TYPES];
	typedef prob (partition_probs_t)[PARTITION_TYPES - 1];

	struct mvs {
		struct component {
			prob	sign;
			prob	classes[MotionVector::CLASSES - 1];
			prob	class0[MotionVector::CLASS0_SIZE - 1];
			prob	bits[MotionVector::OFFSET_BITS];
			prob	class0_fp[MotionVector::CLASS0_SIZE][MotionVector::FP_SIZE - 1];
			prob	fp[MotionVector::FP_SIZE - 1];
			prob	class0_hp;
			prob	hp;
		};
		prob		joints[MotionVector::JOINTS - 1];
		component	comps[2];
	};

	prob				y_mode_prob[BLOCK_SIZE_GROUPS][INTRA_MODES - 1];
	prob				uv_mode_prob[INTRA_MODES][INTRA_MODES - 1];
	partition_probs_t	partition_prob[PARTITION_CONTEXTS];
	prob				inter_mode_probs[INTER_MODE_CONTEXTS][INTER_MODES - 1];
	prob				switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1];
	prob				intra_inter_prob[INTRA_INTER_CONTEXTS];
	prob				comp_inter_prob[COMP_INTER_CONTEXTS];
	prob				single_ref_prob[REF_CONTEXTS][2];
	prob				comp_ref_prob[REF_CONTEXTS];
	prob				skip_probs[SKIP_CONTEXTS];
	prob				tx_32x32[TX_SIZE_CONTEXTS][TX_SIZES - 1];
	prob				tx_16x16[TX_SIZE_CONTEXTS][TX_SIZES - 2];
	prob				tx_8x8[TX_SIZE_CONTEXTS][TX_SIZES - 3];

	coeffs				coef_probs[TX_SIZES][PLANE_TYPES];
	mvs					mv;
	bool				initialized;

	const prob *get_tx_probs(TX_SIZE max_tx_size, int ctx) const {
		switch (max_tx_size) {
			case TX_8X8:	return tx_8x8[ctx];
			case TX_16X16:	return tx_16x16[ctx];
			case TX_32X32:	return tx_32x32[ctx];
			default:		return 0;
		}
	}

	const partition_probs_t	*get_partition_probs(bool intra_only) {
		static const prob kf_partition_probs[PARTITION_CONTEXTS][PARTITION_TYPES - 1] = {
			// 8x8 -> 4x4
			{ 158,  97,  94 },	// a/l both not split
			{  93,  24,  99 },	// a split, l not split
			{  85, 119,  44 },	// l split, a not split
			{  62,  59,  67 },	// a/l both split
			// 16x16 -> 8x8
			{ 149,  53,  53 },	// a/l both not split
			{  94,  20,  48 },	// a split, l not split
			{  83,  53,  24 },	// l split, a not split
			{  52,  18,  18 },	// a/l both split
			// 32x32 -> 16x16
			{ 150,  40,  39 },	// a/l both not split
			{  78,  12,  26 },	// a split, l not split
			{  67,  33,  11 },	// l split, a not split
			{  24,   7,   5 },	// a/l both split
			// 64x64 -> 32x32
			{ 174,  35,  49 },	// a/l both not split
			{  68,  11,  27 },	// a split, l not split
			{  57,  15,   9 },	// l split, a not split
			{  12,   3,   3 },	// a/l both split
		};
		return intra_only ? kf_partition_probs : partition_prob;
	}

	void adapt_mode_probs(FrameContext &prev, FrameCounts &counts, bool interp_switchable, bool tx_select);
	void adapt_mv_probs(FrameContext &prev, FrameCounts &counts, bool allow_high_precision_mv);
	void adapt_coef_probs(FrameContext &prev, FrameCounts &counts, uint32 count_sat, uint32 update_factor);
};

struct TILE_INFO {
	int mi_row_start, mi_row_end;
	int mi_col_start, mi_col_end;

	bool	is_inside(int col, int row) const {
		return row >= mi_row_start && col >= mi_col_start && row < mi_row_end && col < mi_col_end;
	}
	bool	is_inside(int mi_col, int mi_row, const Position &offset) const {
		return is_inside(mi_col + offset.col, mi_row + offset.row);
	}

	static int get_token_alloc(int mb_rows, int mb_cols) {
		// TODO(JBB): double check we can't exceed this token count if we have a 32x32 transform crossing a boundary at a multiple of 16.
		// mb_rows, cols are in units of 16 pixels. We assume 3 planes all at full resolution. We assume up to 1 token per pixel, and then allow a head room of 4.
		return mb_rows * mb_cols * (16 * 16 * 3 + 4);
	}

	// Get the allocated token size for a tile. It does the same calculation as in the frame token allocation.
	int allocated_tokens() {
		return get_token_alloc((mi_row_end - mi_row_start + 1) >> 1, (mi_col_end - mi_col_start + 1) >> 1);
	}
};

struct FrameBufferRef {
	ref_ptr<FrameBuffer>	buf;
	ScaleFactors			sf;

	bool valid_size(int this_width, int this_height) {
		return ScaleFactors::valid_ref_frame_size(buf->y.crop_width, buf->y.crop_height, this_width, this_height);
	}
	bool valid_fmt(ColorSpace &cs) {
		return buf->bit_depth == cs.bit_depth && buf->subsampling_x == cs.subsampling_x && buf->subsampling_y == cs.subsampling_y;
	}
};

struct TileDecoder {
	struct Plane {
		ENTROPY_CONTEXT *above_context, *left_context;
		uint8			subsampling_x, subsampling_y;

		TX_SIZE		get_uv_tx_size(const ModeInfo *mi) const {
			return mi->get_uv_tx_size(subsampling_x, subsampling_y);
		}
		void		set_sampling(int ssx, int ssy) {
			subsampling_x = ssx;
			subsampling_y = ssy;
		}
		void		reset_skip_context(int bwl, int bhl) const {
			memset(above_context, 0, sizeof(ENTROPY_CONTEXT) << (bwl - subsampling_x));//n4_wl);
			memset(left_context, 0, sizeof(ENTROPY_CONTEXT) << (bhl - subsampling_y));//n4_hl);
		}
		force_inline int get_entropy_context(TX_SIZE tx_size, int col, int row) const {
			const ENTROPY_CONTEXT *a = above_context + col;
			const ENTROPY_CONTEXT *l = left_context + row;
			switch (tx_size) {
				default:
				case TX_4X4:	return !!*a + !!*l;
				case TX_8X8:	return !!*(const uint16*)a + !!*(const uint16*)l;
				case TX_16X16:	return !!*(const uint32*)a + !!*(const uint32*)l;
				case TX_32X32:	return !!*(const uint64*)a + !!*(const uint64*)l;
			}
		}
		force_inline void get_entropy_contexts(BLOCK_SIZE bsize, TX_SIZE tx_size, ENTROPY_CONTEXT t_above[16], ENTROPY_CONTEXT t_left[16]) const {
			const int n4_w	= 1 << (b_width_log2_lookup[bsize]  + 1 - subsampling_x);//1 << n4_wl;
			const int n4_h	= 1 << (b_height_log2_lookup[bsize] + 1 - subsampling_y);//1 << n4_hl;
			switch (tx_size) {
				default:
				case TX_4X4:
					memcpy(t_above, above_context, n4_w);
					memcpy(t_left, left_context, n4_h);
					break;
				case TX_8X8:
					for (int i = 0; i < n4_w; i += 2)
						t_above[i] = !!*(const uint16*)(above_context + i);
					for (int i = 0; i < n4_h; i += 2)
						t_left[i] = !!*(const uint16*)(left_context + i);
					break;
				case TX_16X16:
					for (int i = 0; i < n4_w; i += 4)
						t_above[i] = !!*(const uint32*)(above_context + i);
					for (int i = 0; i < n4_h; i += 4)
						t_left[i] = !!*(const uint32*)(left_context + i);
					break;
				case TX_32X32:
					for (int i = 0; i < n4_w; i += 8)
						t_above[i] = !!*(const uint64*)(above_context + i);
					for (int i = 0; i < n4_h; i += 8)
						t_left[i] = !!*(const uint64*)(left_context + i);
					break;
			}
		}
		force_inline void set_entropy_context(TX_SIZE tx_size, int col, int row, bool has_eob) const {
			ENTROPY_CONTEXT *const a = above_context + col;
			ENTROPY_CONTEXT *const l = left_context + row;
			switch (tx_size) {
				default:
				case TX_4X4:	*a = *l = has_eob; break;
				case TX_8X8:	*(uint16*)a = *(uint16*)l = has_eob ? 0x0101 : 0; break;
				case TX_16X16:	*(uint32*)a = *(uint32*)l = has_eob ? 0x01010101 : 0; break;
				case TX_32X32:	*(uint64*)a = *(uint64*)l = has_eob ? 0x0101010101010101ul : 0; break;
			}
		}
	};

	Plane			plane[MAX_PLANES];
	FrameCounts		*counts;
	TILE_INFO		tile;

	ModeInfo		**mi;
	ModeInfo		*left_mi;
	ModeInfo		*above_mi;

	ENTROPY_CONTEXT		*above_context[MAX_PLANES];
	ENTROPY_CONTEXT		left_context[MAX_PLANES][16];

	PARTITION_CONTEXT	*above_seg_context;
	PARTITION_CONTEXT	left_seg_context[8];

	int					*above_dependency[MAX_PLANES];
	int					left_dependency[MAX_PLANES][16];

	int16				seg_dequant[2][MAX_SEGMENTS][2];

	bool				up_available, left_available;
	tran_low_t			*dqcoeff, *dqcoeff_end;

	TileDecoder() : dqcoeff(0), dqcoeff_end(0) {}

	int		get_tx_size_context(TX_SIZE max_tx_size) const;
	int		get_intra_inter_context() const;
	int		get_reference_mode_context(const FrameContext &fc, REFERENCE_FRAME comp_ref) const;
	int		get_pred_context_switchable_interp() const;
	int		get_pred_context_single_ref_p1() const;
	int		get_pred_context_single_ref_p2() const;
	int		get_pred_context_comp_ref_p(const FrameContext &fc, const REFERENCE_FRAME comp_ref[3], bool var_ref_idx) const;

	int		get_partition_plane_context(int mi_row, int mi_col, TX_SIZE bsl) const {
		int		above	= (above_seg_context[mi_col] >> bsl) & 1;
		int		left	= (left_seg_context[mi_row & MI_MASK] >> bsl) & 1;
		return (left * 2 + above) + bsl * 4;
	}

	int		get_pred_context_seg_id() const {
		return (up_available ? above_mi->seg_id_predicted : 0) + (left_available ? left_mi->seg_id_predicted : 0);
	}
	int		get_skip_context() const {
		return (up_available ? above_mi->skip : 0) + (left_available ? left_mi->skip : 0);
	}

	void	reset_skip_context(int bwl, int bhl) const {
		for (int i = 0; i < MAX_PLANES; i++)
			plane[i].reset_skip_context(bwl, bhl);
	}

	void	set_mi_info(ModeInfo **_mi, int mi_stride, int mi_row, int mi_col, int bwl, int bhl) {
		mi					= _mi;
		// Are edges available for intra prediction?
		up_available		= mi_row != 0;
		left_available		= mi_col > tile.mi_col_start;

		above_mi			= up_available		? mi[-mi_stride] : NULL;
		left_mi				= left_available	? mi[-1] : NULL;

		const int above_idx = mi_col * 2;
		const int left_idx	= (mi_row * 2) & 15;
		for (int i = 0; i < MAX_PLANES; ++i) {
			Plane *const pd = &plane[i];
//			pd->n4_wl			= bwl - pd->subsampling_x;
//			pd->n4_hl			= bhl - pd->subsampling_y;
			pd->above_context	= above_context[i]	+ (above_idx >> pd->subsampling_x);
			pd->left_context	= left_context[i]	+ (left_idx >> pd->subsampling_y);
		}
	}

	void	update_partition_context(int mi_row, int mi_col, BLOCK_SIZE subsize, int n) {
		// Generates 4 bit field in which each bit set to 1 represents a blocksize partition
		// 1111 means we split 64x64, 32x32, 16x16 and 8x8
		// 1000 means we just split the 64x64 to 32x32
		static const struct {
			PARTITION_CONTEXT above, left;
		} partition_context_lookup[BLOCK_SIZES] = {
			{15, 15},	// BLOCK_4X4	- {1111, 1111}
			{15, 14},	// BLOCK_4X8	- {1111, 1110}
			{14, 15},	// BLOCK_8X4	- {1110, 1111}
			{14, 14},	// BLOCK_8X8	- {1110, 1110}
			{14, 12},	// BLOCK_8X16	- {1110, 1100}
			{12, 14},	// BLOCK_16X8	- {1100, 1110}
			{12, 12},	// BLOCK_16X16	- {1100, 1100}
			{12, 8 },	// BLOCK_16X32	- {1100, 1000}
			{8,  12},	// BLOCK_32X16	- {1000, 1100}
			{8,  8 },	// BLOCK_32X32	- {1000, 1000}
			{8,  0 },	// BLOCK_32X64	- {1000, 0000}
			{0,  8 },	// BLOCK_64X32	- {0000, 1000}
			{0,  0 },	// BLOCK_64X64	- {0000, 0000}
		};
		PARTITION_CONTEXT *const above_ctx	= above_seg_context + mi_col;
		PARTITION_CONTEXT *const left_ctx	= left_seg_context + (mi_row & MI_MASK);
		// update the partition context at the end
		// set partition bits of block sizes larger than the current one to be one, and partition bits of smaller block sizes to be zero
		memset(above_ctx, partition_context_lookup[subsize].above, n);
		memset(left_ctx, partition_context_lookup[subsize].left, n);
	}

	void	update_partition_context(int mi_row, int mi_col, int bwl, int bhl, int n) {
		static const uint8 table[] = {15,14,12,8,0};
		memset(above_seg_context + mi_col, table[bwl], n);
		memset(left_seg_context + (mi_row & MI_MASK), table[bhl], n);
	}
	void	set_contexts(const Plane *pd, TX_SIZE tx_size, bool has_eob, int col, int row, int max_cols, int max_rows) const;
	int		decode_block_tokens(reader &r, int plane, int entropy_ctx, TX_SIZE tx_size, TX_TYPE tx_type, int seg_id, const FrameContext &fc, GPU *gpu);
};

struct Common {
	enum RESET {
		RESET_NO	= 0,
		RESET_NO2	= 1,
		RESET_THIS	= 2,	//reset just the context specified in the frame header
		RESET_ALL	= 3,	//reset all contexts.
	};
	enum {
		REFS_PER_FRAME		= 3,
		REFERENCE_FRAMES	= 8,
		FRAME_BUFFERS		= REFERENCE_FRAMES + 7,

		FRAME_CONTEXTS_LOG2 = 2,
		FRAME_CONTEXTS		= 1 << FRAME_CONTEXTS_LOG2,
	};

	int							buffer_alignment	= 16;
	int							stride_alignment	= 16;
	int							threads		= 0;
	int							width		= 0, height		= 0;
	int							mi_rows		= 0, mi_cols	= 0;			// in ModeInfo (8-pixel) units
	FRAME_TYPE					frame_type	= KEY_FRAME;
	bool						intra_only	= false;
	bool						show_frame	= false;

	ColorSpace						cs;
	ref_ptr<FrameBuffer>			cur_frame;
	dynamic_array<MotionVectorRef>	mvs;
	dynamic_array<MotionVectorRef>	prev_mvs;

	FrameBufferRef				frame_refs[REFS_PER_FRAME];
	FrameBuffer					frame_pool[FRAME_BUFFERS * 2];
	ref_ptr<FrameBuffer>		ref_frame_map[REFERENCE_FRAMES]; // maps fb_idx to reference slot


	bool						allow_high_precision_mv;
	bool						error_resilient_mode;
	bool						refresh_frame_context;
	bool						frame_parallel_decoding_mode;
	bool						use_prev_frame_mvs;
	bool						lossless;

	int							log2_tile_cols, log2_tile_rows;

	BITSTREAM_PROFILE			profile;
	TX_MODE						tx_mode;
	INTERP_FILTER				interp_filter;
	Quantisation				quant;

	int							mi_stride;
	int							lfm_stride;
	dynamic_array<ModeInfo>		mi_array;
	dynamic_array<ModeInfo*>	mi_grid;
	dynamic_array<LoopFilterMasks>	lfm;

	LoopFilterInfo				lf_info;
	LoopFilter					lf;
	Segmentation				seg;
	malloc_block				seg_map[2];

	bool						ref_frame_sign_bias[REFFRAMES];
	REFERENCE_FRAME				comp_ref[3];			//was fixed_ref, var_ref[2];
	REFERENCE_MODE				reference_mode;

	FrameContext				fc;						// this frame entropy
	FrameContext				*pre_fc;				// this frame entropy prev value
	int							frame_context_idx;
	FrameContext				frame_contexts[FRAME_CONTEXTS];
	FrameCounts					counts;

	dynamic_array<ENTROPY_CONTEXT>		above_context;
	dynamic_array<PARTITION_CONTEXT>	above_seg_context;
	dynamic_array<int>					above_dependency;

	GPU							*gpu = nullptr;

	Common();
	~Common();

	bool			frame_is_intra_only() const {
		return frame_type == KEY_FRAME || intra_only;
	}
	void			reset_frame_map() {
		for (int i = 0; i < REFERENCE_FRAMES; i++)
			ref_frame_map[i] = 0;
    }
	FrameBuffer*	get_free_fb() {
		for (FrameBuffer *i = frame_pool; i != end(frame_pool); ++i) {
			if (!i->in_use())
				return i;
		}
		return 0;
	}

	LoopFilterMasks *get_masks(const int mi_row, const int mi_col) const {
		return &lfm[(mi_col >> MI_BLOCK_SIZE_LOG2) + ((mi_row >> MI_BLOCK_SIZE_LOG2) * lfm_stride)];
	}

	void	get_tile_offsets(int tile_col, int tile_row, int *mi_col, int *mi_row) const;
	void	setup_past_independence(RESET reset_frame_context);
	void	loop_filter_frame_init();
	void	loop_filter_rows(int start, int stop, bool y_only);
	void	set_quantizer(int q);

	void	set_high_precision_mv(bool hp) {
		allow_high_precision_mv = hp;
	}

	FrameBuffer &scale_if_required(FrameBuffer &unscaled, FrameBuffer &scaled, int use_normative_scaler);
};


struct Decoder : Common {
	bool		need_resync;
	int			refresh_frame_flags;
	const FrameContext::partition_probs_t	*partition_probs;

	struct TileWorkerData {
		TileDecoder			mb;
		reader				r;
		FrameCounts			counts;
		tran_low_t			dqcoeff[32 * 32];	// dqcoeff are shared by all the planes. So planes must be decoded serially
		TileWorkerData() : r(empty)  {}
		int		init(Decoder *const dec, const uint8 *data, const uint8 *data_end, int tile_row, int tile_col);
		int		process(Decoder *const dec);
		int		process1row(Decoder *const dec, int mi_row);
		const uint8 *end() {
			r.restore_unused();
			return r.reader().peek_block(0);
		}
	};

	dynamic_array<TileWorkerData>		tile_data;

	Decoder();

	ptrdiff_t		read_uncompressed_header(bit_reader &r);
	void			read_compressed_header(reader &r);
	void			read_ref_frames(reader &r, TileDecoder *const xd, int segment_id, REFERENCE_FRAME ref_frame[2]) const;

	int				read_intra_segment_id(reader &r, int mi_offset, int x_mis, int y_mis) const;
	void			read_intra_frame_mode_info(reader &r, TileDecoder *const xd, int mi_row, int mi_col) const;

	int				read_inter_segment_id(reader &r, TileDecoder *const xd, int mi_row, int mi_col) const;
	RETURN			read_inter_block_mode_info(reader &r, TileDecoder *const xd, ModeInfo *const mi, int mi_row, int mi_col) const;
	RETURN			read_inter_frame_mode_info(reader &r, TileDecoder *const xd, int mi_row, int mi_col) const;

	RETURN			decode_block(reader &r, TileDecoder *const xd, int mi_row, int mi_col, BLOCK_SIZE bsize, int bwl, int bhl) const;
	RETURN			decode_partition(reader &r, TileDecoder *const xd, int mi_row, int mi_col, TX_SIZE n4x4_l2);
	ptrdiff_t		decode_subframe(const uint8 *data, const uint8 *data_end);
	ptrdiff_t		decode_frame(const uint8 *data, const uint8 *data_end);
	FrameBuffer*	get_frame() const;
};

} // namespace vp9

#endif	// VPX_DECODE_H
