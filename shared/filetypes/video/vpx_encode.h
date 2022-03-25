#include "vpx_decode.h"
#include "base/maths.h"

namespace vp9 {

#define VPX_MAX_LAYERS			12	// 3 temporal + 4 spatial layers are allowed.
#define VPX_SS_MAX_LAYERS		5	// Spatial Scalability: Maximum number of coding layers
#define VPX_SS_DEFAULT_LAYERS	1	// Spatial Scalability: Default number of coding layers
#define VPX_TS_MAX_LAYERS       5	// Temporal Scalability: Maximum number of coding layers
#define VLOW_MOTION_THRESHOLD	950

// Use this macro to turn on/off use of alt-refs in one-pass mode.
#define USE_ALTREF_FOR_ONE_PASS   1

enum {
	MAX_LAG_BUFFERS = 25,
};

struct variance_funcs {
	uint32	(*sdf	)(const uint8 *a, int as, const uint8 *b, int bs);
	uint32	(*sdaf	)(const uint8 *a, int as, const uint8 *b, int bs, const uint8 *second_pred);
	uint32	(*vf	)(const uint8 *a, int as, const uint8 *b, int bs, uint32 *sse);
	uint32	(*svf	)(const uint8 *a, int as, int x, int y, const uint8 *b, int bs, uint32 *sse);
	uint32	(*svaf	)(const uint8 *a, int as, int x, int y, const uint8 *b, int bs, uint32 *sse, const uint8 *second_pred);
	uint32	(*sdx3f	)(const uint8 *a, int as, const uint8 *b, int bs, uint32 *sad_array);
	uint32	(*sdx8f	)(const uint8 *a, int as, const uint8 *b, int bs, uint32 *sad_array);
	uint32	(*sdx4df)(const uint8 *a, int as, const uint8 *const b_array[], int bs, uint32 *sad_array);
};
extern variance_funcs	variance_table[];

bool compute_skin_block(const Buffer2D &yb, const Buffer2D &ub, const Buffer2D &vb, int x, int y, int w, int h);

enum ENCODE_MODE {
	// Good Quality Fast Encoding. The encoder balances quality with the amount of time it takes to encode the output. Speed setting controls how fast.
	GOOD,

	// The encoder places priority on the quality of the output over encoding speed.
	// The output is compressed at the highest possible quality. This option takes the longest amount of time to encode. Speed setting ignored.
	BEST,

	// Realtime/Live Encoding. This mode is optimized for realtime encoding (for example, capturing a television signal or feed from a live camera). Speed setting controls how fast.
	REALTIME
};
enum RC_MODE {
	RC_VBR,  // Variable Bit Rate (VBR) mode
	RC_CBR,  // Constant Bit Rate (CBR) mode
	RC_CQ,   // Constrained Quality (CQ)  mode
	RC_Q,    // Constant Quality (Q) mode
};
enum AQ_MODE {
	AQ_NO				= 0,
	AQ_VARIANCE			= 1,
	AQ_COMPLEXITY		= 2,
	AQ_CYCLIC_REFRESH	= 3,
	AQ_EQUATOR360		= 4,
	AQ_MODES
};
enum RESIZE_TYPE {
	RESIZE_NONE			= 0,	// No frame resizing allowed (except for SVC).
	RESIZE_FIXED		= 1,	// All frames are coded at the specified dimension.
	RESIZE_DYNAMIC		= 2		// Coded size of each frame is determined by the codec.
};
enum TUNING {
	TUNE_PSNR,
	TUNE_SSIM
};
enum TUNE_CONTENT {
	CONTENT_DEFAULT,
	CONTENT_SCREEN,
	CONTENT_INVALID
};
enum TEMPORAL_LAYERING_MODE {
	TEMPORAL_NOLAYERING   = 0,	// No temporal layering. Used when only spatial layering is used.
	TEMPORAL_BYPASS       = 1,	// Bypass mode. Used when application needs to control temporal layering. This will only work when the number of spatial layers equals 1.
	TEMPORAL_0101         = 2,	// 0-1-0-1... temporal layering scheme with two temporal layers.
	TEMPORAL_0212         = 3,	// 0-2-1-2... temporal layering scheme with three temporal layers.
};

struct NOISE_ESTIMATE {
	enum LEVEL {
		LowLow,
		Low,
		Medium,
		High
	};

	bool	enabled;
	LEVEL	level;
	int		value;
	int		thresh;
	int		count;
	int		last_w;
	int		last_h;
	int		num_frames_estimate;

	NOISE_ESTIMATE() {
		enabled = 0;
		level	= LowLow;
		value	= 0;
		count	= 0;
		thresh	= 90;
		last_w	= 0;
		last_h	= 0;
		thresh	= 0;
		num_frames_estimate = 20;
	}
	LEVEL	extract_level() const {
		return	value > thresh << 1 ? High
			:	value > thresh		? Medium
			:	value > thresh >> 1	? Low
			:	LowLow;
	}

	void	set_size(int width, int height) {
		last_w	= width;
		last_h	= height;
		thresh	= width * height >= 1920 * 1080 ? 200 : 130;
	}
	bool	update(uint64 avg_est) {
		value = (int)((15 * value + avg_est) >> 4);
		if (++count == num_frames_estimate) {
			// Reset counter and check noise level condition.
			num_frames_estimate = 30;
			count				= 0;
			level				= extract_level();
			return true;
		}
		return false;
	}

};

extern int64 get_sse(const uint8 *a, int a_stride, const uint8 *b, int b_stride, int width, int height);

inline int64 get_sse(const Buffer2D &a, const Buffer2D &b, int width, int height) {
	return get_sse(a.buffer, a.stride, b.buffer, b.stride, width, height);
}

struct PSNR {
	uint32		samples[4];  // Number of samples,	total/y/u/v
	uint64		sse[4];      // sum squared error,	total/y/u/v
	double		psnr[4];     // PSNR,				total/y/u/v
	
	static double sse_to_psnr(double samples, double peak, double sse) {
		static const double MAX_PSNR = 100.0;
		return sse > 0.0 ? min(10.0 * log10(samples * square(peak) / sse), MAX_PSNR) : MAX_PSNR;
	}

	void calc(const FrameBuffer *a, const FrameBuffer *b) {
		static const double peak	= 255.0;
		uint64		total_sse		= 0;
		uint32		total_samples	= 0;

		for (int i = 0; i < 3; ++i) {
			const FrameBuffer::Plane	&p	= a->plane(i);

			sse[1 + i]		= get_sse(a->buffer(i), b->buffer(i), p.width, p.height);
			samples[1 + i]	= p.width * p.height;
			psnr[1 + i]		= sse_to_psnr(samples[1 + i], peak, (double)sse[1 + i]);

			total_sse		+= sse[1 + i];
			total_samples	+= samples[1 + i];
		}

		sse[0]		= total_sse;
		samples[0]	= total_samples;
		psnr[0]		= sse_to_psnr((double)total_samples, peak, (double)total_sse);
	}

};

//-----------------------------------------------------------------------------
// vp9_tokenize.h
//-----------------------------------------------------------------------------

struct TOKENVALUE {
	int16		token;
	int16		extra;
};

struct TOKENEXTRA {
	const prob	*context_tree;
	int16		token;
	int16		extra;
};

struct PKT {
	enum PKT_KIND {
		PKT_CX_FRAME,					// Compressed video frame
		PKT_STATS,						// Two-pass statistics for this frame
		PKT_FPMB_STATS,					// first pass mb statistics for this frame
		PKT_PSNR,						// PSNR statistics for this frame
		PKT_SPATIAL_SVC_LAYER_SIZES,	// Sizes for each layer in this frame
		PKT_SPATIAL_SVC_LAYER_PSNR,		// PSNR for each layer in this frame
		PKT_CUSTOM	= 256				// Algorithm extensions
	};
	PKT_KIND  kind;
	union {
		struct {
			void		*buf;			// compressed data buffer
			size_t		sz;				// length of compressed data
			int64		pts;			// time stamp to show frame (in timebase units)
			uint32		duration;		// duration to show frame(in timebase units)
			uint32		flags;			// flags for this frame
			int			partition_id;	// defines the decoding order  of the partitions. Only applicable when "output partition" mode is enabled. First partition has id 0
		} frame;
		memory_block	twopass_stats;		// data for two-pass packet
		memory_block	firstpass_mb_stats; // first pass mb packet
		PSNR			psnr;				// data for PSNR packet
		memory_block	raw;				// data for arbitrary packets
	#if 0
		size_t			layer_sizes[VPX_SS_MAX_LAYERS];
		PSNR			layer_psnr[VPX_SS_MAX_LAYERS];
	#endif
		char			pad[128 - sizeof(PKT_KIND)];
	} data;
};

struct token {
	int value;
	int len;
};

struct EncoderConfig {
	BITSTREAM_PROFILE	profile;
	ColorSpace			cs;
	int					render_width;
	int					render_height;

	int					width;						// width of data passed to the compressor
	int					height;						// height of data passed to the compressor
	uint32				input_bit_depth;			// Input bit depth.
	double				init_framerate;				// set to passed in framerate
	int64				target_bandwidth;			// bandwidth to be used in kilobits per second

	int					noise_sensitivity;			// pre processing blur: recommendation 0
	int					sharpness;					// sharpening output: recommendation 0:
	int					speed;
	uint32				rc_max_intra_bitrate_pct;	// maximum allowed bitrate for any intra frame in % of bitrate target.
	uint32				rc_max_inter_bitrate_pct;	// maximum allowed bitrate for any inter frame in % of bitrate target.
	uint32				gf_cbr_boost_pct;			// percent of rate boost for golden frame in CBR mode.

	ENCODE_MODE			mode;
	int					pass;

	// Key Framing Operations
	int					auto_key;		// autodetect cut scenes and set the keyframes
	int					key_freq;		// maximum distance to key frame.
	int					lag_in_frames;  // how many frames lag before we start encoding

	// ----------------------------------------------------------------
	// DATARATE CONTROL OPTIONS

	// vbr, cbr, constrained quality or constant quality
	RC_MODE		rc_mode;

	// buffer targeting aggressiveness
	int			under_shoot_pct;
	int			over_shoot_pct;

	// buffering parameters
	int64		starting_buffer_level_ms;
	int64		optimal_buffer_level_ms;
	int64		maximum_buffer_size_ms;

	// Frame drop threshold.
	int			drop_frames_water_mark;

	// controlling quality
	int			fixed_q;
	int			worst_allowed_q;
	int			best_allowed_q;
	int			cq_level;
	AQ_MODE		aq_mode;  // Adaptive Quantization mode

	// Internal frame size scaling.
	RESIZE_TYPE resize_mode;
	int			scaled_frame_width;
	int			scaled_frame_height;

	// Enable feature to reduce the frame quantization every x frames.
	int			frame_periodic_boost;

	// two pass datarate control
	int			two_pass_vbrbias;        // two pass datarate control tweaks
	int			two_pass_vbrmin_section;
	int			two_pass_vbrmax_section;
	// ----------------------------------------------------------------

	// Spatial and temporal scalability.
	int			ss_number_layers;  // Number of spatial layers.
	int			ts_number_layers;  // Number of temporal layers.
	// Bitrate allocation for spatial layers.
	int			layer_target_bitrate[VPX_MAX_LAYERS];
	int			ss_target_bitrate[VPX_SS_MAX_LAYERS];
	int			ss_enable_auto_arf[VPX_SS_MAX_LAYERS];
	// Bitrate allocation (CBR mode) and framerate factor, for temporal layers.
	int			ts_rate_decimator[VPX_TS_MAX_LAYERS];

	int			enable_auto_arf;
	int			encode_breakout;  // early breakout : for video conf recommend 800

	// Bitfield defining the error resiliency features to enable. Can provide decodable frames after losses in previous frames and decodable partitions after losses in the same frame.
	uint32		error_resilient_mode;

	// Bitfield defining the parallel decoding mode where the decoding in successive frames may be conducted in parallel just by decoding the frame headers.
	uint32		frame_parallel_decoding_mode;

	int			arnr_max_frames;
	int			arnr_strength;

	int			min_gf_interval;
	int			max_gf_interval;

	int			tile_columns;
	int			tile_rows;

	int			max_threads;

	memory_block	two_pass_stats_in;
	dynamic_array<PKT>	output_pkt_list;

#if CONFIG_FP_MB_STATS
	memory_block	firstpass_mb_stats_in;
#endif

	TUNING			tuning;
	TUNE_CONTENT	content;
	TEMPORAL_LAYERING_MODE temporal_layering_mode;

	bool	is_lossless_requested() const {
		return best_allowed_q == 0 && worst_allowed_q == 0;
	}
};


//-----------------------------------------------------------------------------
// vp9_mcomp.h
//-----------------------------------------------------------------------------

struct SEARCH_CONFIG {
	enum {
		MAX_MVSEARCH_STEPS		= 11,								// The maximum number of steps in a step search given the largest allowed initial step
		MAX_FIRST_STEP			= 1 << (MAX_MVSEARCH_STEPS - 1),	// Maximum size of the first step in full pel units
		MAX_FULL_PEL_VAL		= MAX_FIRST_STEP - 1,				// Max full pel mv specified in the unit of full pixel Enable the use of motion vector in range [-1023, 1023].
		BORDER_MV_PIXELS_B16	= 16 + 4,							// Allowed motion vector pixel distance outside image border for Block_16x16
	};
	MotionVector	ss_mv[8 * MAX_MVSEARCH_STEPS];	// Motion vector
	intptr_t		ss_os[8 * MAX_MVSEARCH_STEPS];	// Offset
	int				searches_per_step;
	int				total_steps;

	static int init_search_range(int size) {
		int sr = 0;
		// Minimum search size no matter what the passed in value.
		size = max(16, size);
		while ((size << sr) < MAX_FULL_PEL_VAL)
			sr++;
		return min(sr, MAX_MVSEARCH_STEPS - 2);
	}

	void init_dsmotion_compensation(int stride) {
		static const int8 mvs[][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
		int s = 0;
		for (int len = MAX_MVSEARCH_STEPS; len--; ) {
			for (int i = 0; i < 4; ++i, ++s) {
				ss_mv[s].row = mvs[i][0] << len;
				ss_mv[s].col = mvs[i][1] << len;
				ss_os[s]	 = ss_mv[s].row * stride + ss_mv[s].col;
			}
		}
		searches_per_step	= 4;
		total_steps			= s / searches_per_step;
	}
	void init3smotion_compensation(int stride) {
		static const int8 mvs[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
		int s = 0;
		for (int len = MAX_MVSEARCH_STEPS; len--; ) {
			for (int i = 0; i < 8; ++i, ++s) {
				ss_mv[s].row = mvs[i][0] << len;
				ss_mv[s].col = mvs[i][1] << len;
				ss_os[s]	 = ss_mv[s].row * stride + ss_mv[s].col;
			}
		}
		searches_per_step	= 8;
		total_steps			= s / searches_per_step;
	}
};

//-----------------------------------------------------------------------------
//vp9_block.h
//-----------------------------------------------------------------------------

struct DIFF {
	uint32	sse;
	int		sum;
	uint32	var;
};

struct MB_ModeInfo_EXT {
	MotionVector	ref_mvs[REFFRAMES][MotionVectorRef::MAX_CANDIDATES];
	uint8			mode_context[REFFRAMES];
};

struct TileEncoder : TileDecoder {
	struct PlaneEncoder {
		DECLARE_ALIGNED(16, int16, src_diff[64 * 64]);
		tran_low_t	*qcoeff;
		tran_low_t	*coeff;
		uint16		*eobs;
		Buffer2D	src;

		// Quantizer setings
		int16		*quant_fp;
		int16		*round_fp;
		int16		*quant;
		int16		*quant_shift;
		int16		*zbin;
		int16		*round;

		int64		quant_thred[2];
	};

	PlaneEncoder	enc_plane[MAX_PLANES];

	MB_ModeInfo_EXT *mbmi_ext;
	MB_ModeInfo_EXT *mbmi_ext_base;
	int				skip_block;
	int				select_tx_size;
	int				skip_recode;
	int				skip_optimize;
	int				q_index;

	int				errorperbit;
	int				sadperbit16;
	int				sadperbit4;
	int				rddiv;
	int				rdmult;
	int				mb_energy;
	int				*m_search_count_ptr;
	int				*ex_search_count_ptr;

	// These are set to their default values at the beginning, and then adjustedfurther in the encoding process.
	BLOCK_SIZE		min_partition_size;
	BLOCK_SIZE		max_partition_size;

	int				mv_best_ref_index[REFFRAMES];
	uint32			max_mv_context[REFFRAMES];
	uint32			source_variance;
	uint32			pred_sse[REFFRAMES];
	int				pred_mv_sad[REFFRAMES];

	uint32			nmvjointcost[MotionVector::JOINTS];
	uint32			*nmvcost[2];
	uint32			*nmvcost_hp[2];

	uint32			nmvjointsadcost[MotionVector::JOINTS];
	uint32			*nmvsadcost[2];
	uint32			*nmvsadcost_hp[2];
	uint32			**mvsadcost;

	// These define limits to motion vector components to prevent them from extending outside the UMV borders
	int				mv_col_min;
	int				mv_col_max;
	int				mv_row_min;
	int				mv_row_max;

	// Notes transform blocks where no coefficents are coded. Set during mode selection. Read during block encoding.
	uint8		zcoeff_blk[TX_SIZES][256];

	int				skip;
	int				encode_breakout;

	// note that token_costs is the cost when eob node is skipped
	Bands<uint32[2][ENTROPY_TOKENS]> token_costs[TX_SIZES][PLANE_TYPES][REF_TYPES];

	int				optimize;

	// indicate if it is in the rd search loop or encoding process
	bool			use_lp32x32fdct;
	int				skip_encode;
	// use fast quantization process
	int				quant_fp;

	// skip forward transform and quantization
	enum {
		SKIP_TXFM_NONE		= 0,
		SKIP_TXFM_AC_DC		= 1,
		SKIP_TXFM_AC_ONLY	= 2,
	};
	uint8			skip_txfm[MAX_PLANES << 2];

	int64			bsse[MAX_PLANES << 2];

	// Used to store sub partition's choices.
	MotionVector	pred_mv[REFFRAMES];

	// Strong color activity detection. Used in RTC coding mode to enhance the visual quality at the boundary of moving color objects.
	uint8			color_sensitivity[2];
	uint8			sb_is_skin;

	void			(*fwd_txm4x4)(const int16 *input, tran_low_t *output, int stride);
	void			(*itxm_add)(const tran_low_t *input, uint8 *dest, int stride, int eob);

	void regular_quantize_b_4x4(int plane, int block, const int16 *scan, const int16 *iscan);
	void mv_pred(uint8 *src_y_buffer, int src_y_stride, uint8 *ref_y_buffer, int ref_y_stride, int ref_frame, BLOCK_SIZE block_size, bool extra_mv);

	void set_mv_search_range(const MotionVector *mv) {
		// Get intersection of UMV window and valid MotionVector window to reduce # of checks in diamond search.
		mv_col_min	= max(mv_col_min, max((mv->col >> 3) - SEARCH_CONFIG::MAX_FULL_PEL_VAL + (mv->col & 7 ? 1 : 0), (MotionVector::LOW >> 3) + 1));
		mv_col_max	= min(mv_col_max, min((mv->col >> 3) + SEARCH_CONFIG::MAX_FULL_PEL_VAL, (MotionVector::UPP >> 3) - 1));
		mv_row_min	= max(mv_row_min, max((mv->row >> 3) - SEARCH_CONFIG::MAX_FULL_PEL_VAL + (mv->row & 7 ? 1 : 0), (MotionVector::LOW >> 3) + 1));
		mv_row_max	= min(mv_row_max, min((mv->row >> 3) + SEARCH_CONFIG::MAX_FULL_PEL_VAL, (MotionVector::UPP >> 3) - 1));
	}

	bool check_bounds(int row, int col, int range) const {
		return row - range >= mv_row_min && row + range <= mv_row_max && col - range >= mv_col_min && col + range <= mv_col_max;
	}
	bool is_mv_in(const MotionVector &mv) const {
		return mv.col >= mv_col_min && mv.col <= mv_col_max && mv.row >= mv_row_min && mv.row <= mv_row_max;
	}

};

//-----------------------------------------------------------------------------
//vp9_context_tree.h
//-----------------------------------------------------------------------------

// Structure to hold snapshot of coding context during the mode picking process
struct PICK_MODE_CONTEXT {
	ModeInfo		mic;
	MB_ModeInfo_EXT mbmi_ext;
	uint8			*zcoeff_blk;
	tran_low_t		*coeff[MAX_PLANES][3];
	tran_low_t		*qcoeff[MAX_PLANES][3];
	tran_low_t		*dqcoeff[MAX_PLANES][3];
	uint16			*eobs[MAX_PLANES][3];

	// dual buffer pointers, 0: in use, 1: best in store
	tran_low_t		*coeff_pbuf[MAX_PLANES][3];
	tran_low_t		*qcoeff_pbuf[MAX_PLANES][3];
	tran_low_t		*dqcoeff_pbuf[MAX_PLANES][3];
	uint16			*eobs_pbuf[MAX_PLANES][3];

	int				is_coded;
	int				num_4x4_blk;
	int				skip;
	int				pred_pixel_ready;
	// For current partition, only if all Y, U, and V transform blocks' coefficients are quantized to 0, skippable is set to 0.
	int				skippable;
	uint8			skip_txfm[MAX_PLANES << 2];
	int				best_mode_index;
	int				hybrid_pred_diff;
	int				comp_pred_diff;
	int				single_pred_diff;
	int64			best_filter_diff[SWITCHABLE_FILTER_CONTEXTS];

	// TODO(jingning) Use RD_COST struct here instead. This involves a broader scope of refactoring.
	int				rate;
	int64			dist;

#if CONFIG_VP9_TEMPORAL_DENOISING
	uint32			newmv_sse;
	uint32			zeromv_sse;
	uint32			zeromv_lastref_sse;
	PREDICTION_MODE best_sse_inter_mode;
	MotionVector	best_sse_mv;
	REFERENCE_FRAME best_reference_frame;
	REFERENCE_FRAME best_zeromv_reference_frame;
#endif

	// motion vector cache for adaptive motion search control in partition search loop
	MotionVector	pred_mv[REFFRAMES];
	INTERP_FILTER	pred_interp_filter;
};

struct PC_TREE {
	int					index;
	PARTITION_TYPE		partitioning;
	BLOCK_SIZE			block_size;
	PICK_MODE_CONTEXT	none;
	PICK_MODE_CONTEXT	horizontal[2];
	PICK_MODE_CONTEXT	vertical[2];
	union {
		PC_TREE				*split[4];
		PICK_MODE_CONTEXT	*leaf_split[4];
	};
};

//-----------------------------------------------------------------------------
// vp9_lookahead.h
//-----------------------------------------------------------------------------


// The max of past frames we want to keep in the queue.

struct lookahead {
	enum { MAX_PRE_FRAMES = 1 };
	struct entry {
		FrameBuffer	img;
		int64		ts_start;
		int64		ts_end;
		uint32		flags;
	};
	uint32		max_sz;         // Absolute size of the queue
	uint32		sz;             // Number of buffers currently in the queue
	uint32		read_idx;       // Read index
	uint32		write_idx;      // Write index
	entry		*buf;			// Buffer list

	lookahead(uint32 width, uint32 height, ColorSpace &cs, uint32 depth) : max_sz(clamp(depth, 1, MAX_LAG_BUFFERS) + MAX_PRE_FRAMES), read_idx(0), write_idx(0) {
		buf		= new entry[max_sz];
		for (int i = 0; i < depth; i++)
			buf[i].img.resize(width, height, cs, ENC_BORDER, 0, 0, false);
	}
	~lookahead() {
		delete[] buf;
	}
	uint32 depth() const {
		return sz;
	}

	// Return the buffer at the given absolute index and increment the index 
	entry *pop(uint32 *idx) {
		uint32 index	= *idx;
		entry *e		= buf + index;
		if (++index >= max_sz)
			index -= max_sz;
		*idx = index;
		return e;
	}

	bool push(const FrameBuffer &src, int64 ts_start, int64 ts_end, bool use_highbitdepth, uint32 flags) {
		if (sz + 1 + MAX_PRE_FRAMES > max_sz)
			return true;

		sz++;
		entry	*e = pop(&write_idx);

		if (src.y.crop_width > e->img.y.width || src.y.crop_height > buf->img.y.height || src.uv.crop_width > buf->img.uv.width || src.uv.crop_height > buf->img.uv.height) {
			e->img.resize(src.y.crop_width, src.y.crop_height, src, ENC_BORDER, 0, 0, false);
		} else if (!equal_dimensions(src, e->img)) {
			e->img.y.crop_width		= src.y.crop_width;
			e->img.y.crop_height	= src.y.crop_height;
			e->img.uv.crop_width	= src.uv.crop_width;
			e->img.uv.crop_height	= src.uv.crop_height;
		}
		e->img.copy_extend(src);
		e->ts_start = ts_start;
		e->ts_end	= ts_end;
		e->flags	= flags;
		return false;
	}
	entry *pop(bool drain) {
		entry *e = NULL;
		if (sz && (drain || sz == max_sz - MAX_PRE_FRAMES)) {
			e = pop(&read_idx);
			sz--;
		}
		return e;
	}
	entry *peek(int index) {
		entry *e = NULL;
		if (index >= 0) {
			// Forward peek
			if (index < (int)sz) {
				index += read_idx;
				if (index >= (int)max_sz)
					index -= max_sz;
				e = buf + index;
			}
		} else if (index < 0) {
			// Backward peek
			if (-index <= MAX_PRE_FRAMES) {
				index += read_idx;
				if (index < 0)
					index += max_sz;
				e = buf + index;
			}
		}
		return e;
	}
};

//-----------------------------------------------------------------------------
// vp9_mbgraph.h
//-----------------------------------------------------------------------------

struct MBGRAPH_MB_STATS {
	struct {
		int		err;
		union {
			MotionVector	mv;
			PREDICTION_MODE mode;
		};
	} ref[REFFRAMES];
	void update(FrameBuffer *src, FrameBuffer *dst, FrameBuffer *golden_ref, const MotionVector *prev_golden_ref_mv, FrameBuffer *alt_ref, int mb_row, int mb_col);
};

struct MBGRAPH_FRAME_STATS {
	MBGRAPH_MB_STATS *mb_stats;
};

//-----------------------------------------------------------------------------
// vp9_firstpass.h
//-----------------------------------------------------------------------------

struct FIRSTPASS_STATS {
	double		frame;
	double		weight;
	double		intra_error;
	double		coded_error;
	double		sr_coded_error;
	double		pcnt_inter;
	double		pcnt_motion;
	double		pcnt_second_ref;
	double		pcnt_neutral;
	double		intra_skip_pct;
	double		inactive_zone_rows;  // Image mask rows top and bottom.
	double		inactive_zone_cols;  // Image mask columns at left and right edges.
	double		MVr;
	double		mvr_abs;
	double		MVc;
	double		mvc_abs;
	double		MVrv;
	double		MVcv;
	double		mv_in_out_count;
	double		new_mv_count;
	double		duration;
	double		count;
	int64		spatial_layer_id;
};

enum FRAME_UPDATE_TYPE {
	FU_KF				= 0,
	FU_LF				= 1,
	FU_GF				= 2,
	FU_ARF				= 3,
	FU_OVERLAY			= 4,
	FRAME_UPDATE_TYPES,
};
enum RATE_FACTOR_LEVEL {
	INTER_NORMAL		= 0,
	INTER_HIGH			= 1,
	GF_ARF_LOW			= 2,
	GF_ARF_STD			= 3,
	KF_STD				= 4,
	RATE_FACTOR_LEVELS	= 5
};
#define FC_ANIMATION_THRESH 0.15
enum FRAME_CONTENT_TYPE {
	FC_NORMAL				= 0,
	FC_GRAPHICS_ANIMATION	= 1,
	FRAME_CONTENT_TYPES		= 2
};

struct GF_GROUP {
	uint8				index;
	RATE_FACTOR_LEVEL	rf_level[MAX_LAG_BUFFERS * 2 + 1];
	FRAME_UPDATE_TYPE	update_type[MAX_LAG_BUFFERS * 2 + 1];
	uint8				arf_src_offset[MAX_LAG_BUFFERS * 2 + 1];
	uint8				arf_update_idx[MAX_LAG_BUFFERS * 2 + 1];
	uint8				arf_ref_idx[MAX_LAG_BUFFERS * 2 + 1];
	int					bit_allocation[MAX_LAG_BUFFERS * 2 + 1];
};

struct TWO_PASS {
	uint32				section_intra_rating;
	FIRSTPASS_STATS		total_stats;
	FIRSTPASS_STATS		this_frame_stats;
	const FIRSTPASS_STATS *stats_in;
	const FIRSTPASS_STATS *stats_in_start;
	const FIRSTPASS_STATS *stats_in_end;
	FIRSTPASS_STATS		total_left_stats;
	bool				first_pass_done;
	int64				bits_left;
	double				modified_error_min;
	double				modified_error_max;
	double				modified_error_left;
	double				mb_av_energy;

#if CONFIG_FP_MB_STATS
	uint8				*frame_mb_stats_buf;
	uint8				*this_frame_mb_stats;
	FIRSTPASS_MB_STATS	firstpass_mb_stats;
#endif
	// An indication of the content type of the current frame
	FRAME_CONTENT_TYPE	fr_content_type;

	// Projected total bits available for a key frame group of frames
	int64				kf_group_bits;

	// Error score of frames still to be coded in kf group
	int64				kf_group_error_left;

	// The fraction for a kf groups total bits allocated to the inter frames
	double				kfgroup_inter_fraction;

	int					sr_update_lag;

	int					kf_zeromotion_pct;
	int					last_kfgroup_zeromotion_pct;
	int					gf_zeromotion_pct;
	int					active_worst_quality;
	int					baseline_active_worst_quality;
	int					extend_minq;
	int					extend_maxq;
	int					extend_minq_fast;

	GF_GROUP			gf_group;
};

//-----------------------------------------------------------------------------
// vp9_ratectrl.h
//-----------------------------------------------------------------------------
enum RESIZE_ACTION {
	NO_RESIZE			= 0,
	DOWN_THREEFOUR		= 1,	// From orig to 3/4.
	DOWN_ONEHALF		= 2,	// From orig or 3/4 to 1/2.
	UP_THREEFOUR		= -1,	// From 1/2 to 3/4.
	UP_ORIG				= -2,	// From 1/2 or 3/4 to orig.
};
enum RESIZE_STATE {
	RESIZE_ORIG				= 0,
	RESIZE_THREE_QUARTER	= 1,
	RESIZE_ONE_HALF			= 2
};

#define LIMIT_QRANGE_FOR_ALTREF_AND_KEY	1

struct RATE_CONTROL {
	enum {
		BPER_MB_NORMBITS    = 9,	// Bits Per MB at different Q (Multiplied by 512)
		MIN_GF_INTERVAL     = 4,
		MAX_GF_INTERVAL     = 16,
		FIXED_GF_INTERVAL   = 8,	// Used in some testing modes only
		MAX_MB_RATE			= 250,
		MAXRATE_1080P		= 2025000,
		DEFAULT_KF_BOOST	= 2000,
		DEFAULT_GF_BOOST	= 2000,
		MAX_BPB_FACTOR		= 50,
		FRAME_OVERHEAD_BITS = 200,
	};

	enum FRAME_SCALE_LEVEL {
		UNSCALED			= 0,	// Frame is unscaled.
		SCALE_STEP1			= 1,	// First-level down-scaling.
		FRAME_SCALE_STEPS
	};

	// Tables relating active max Q to active min Q
	struct tables {
		int	kf_low[QINDEX_RANGE];
		int	kf_high[QINDEX_RANGE];
		int	arfgf_low[QINDEX_RANGE];
		int	arfgf_high[QINDEX_RANGE];
		int	inter[QINDEX_RANGE];
		int	rtc[QINDEX_RANGE];
		void init(int bit_depth);
	};
	static tables tables8;
	static tables tables10;
	static tables tables12;

	// Multiplier of the target rate to be used as threshold for triggering scaling.
	static const int8 rate_thresh_mult[FRAME_SCALE_STEPS];
	// Scale dependent Rate Correction Factor multipliers. Compensates for the greater number of bits per pixel generated in down-scaled frames.
	static const int8 rcf_mult[FRAME_SCALE_STEPS];

	static const int gf_high	= 2000;
	static const int gf_low		= 400;
	static const int kf_high	= 5000;
	static const int kf_low		= 400;
	static const float MIN_BPB_FACTOR;

	// Rate targetting variables
	int			base_frame_target;           // A baseline frame target before adjustment for previous under or over shoot.
	int			this_frame_target;           // Actual frame target after rc adjustment.
	int			projected_frame_size;
	int			sb64_target_rate;
	int			last_q[FRAME_TYPES];         // Separate values for Intra/Inter
	int			last_boosted_qindex;         // Last boosted GF/KF/ARF q
	int			last_kf_qindex;              // Q index of the last key frame coded.

	int			gfu_boost;
	int			last_boost;
	int			kf_boost;

	float		rate_correction_factors[RATE_FACTOR_LEVELS];

	int			frames_since_golden;
	int			frames_till_gf_update_due;
	int			min_gf_interval;
	int			max_gf_interval;
	int			static_scene_max_gf_interval;
	int			baseline_gf_interval;
	int			constrained_gf_group;
	int			frames_to_key;
	int			frames_since_key;

	bool		this_key_frame_forced;
	bool		next_key_frame_forced;
	bool		source_alt_ref_pending;
	bool		source_alt_ref_active;
	bool		is_src_frame_alt_ref;

	int			avg_frame_bandwidth;  // Average frame size target for clip
	int			min_frame_bandwidth;  // Minimum allocation used for any frame
	int			max_frame_bandwidth;  // Maximum burst rate allowed for a frame.

	int			ni_av_qi;
	int			ni_tot_qi;
	int			ni_frames;
	int			avg_frame_qindex[FRAME_TYPES];
	double		tot_q;
	double		avg_q;

	int64		buffer_level;
	int64		bits_off_target;
	int64		vbr_bits_off_target;
	int64		vbr_bits_off_target_fast;

	int			decimation_factor;
	int			decimation_count;

	int			rolling_target_bits;
	int			rolling_actual_bits;

	int			long_rolling_target_bits;
	int			long_rolling_actual_bits;

	int			rate_error_estimate;

	int64		total_actual_bits;
	int64		total_target_bits;
	int64		total_target_vs_actual;

	int			worst_quality;
	int			best_quality;

	int64		starting_buffer_level;
	int64		optimal_buffer_level;
	int64		maximum_buffer_size;

	// rate control history for last frame(1) and the frame before(2).
	// -1: undershot
	//  1: overshoot
	//  0: not initialized.
	int			rc_1_frame;
	int			rc_2_frame;
	int			q_1_frame;
	int			q_2_frame;

	// Auto frame-scaling variables.
	FRAME_SCALE_LEVEL	frame_size_selector;
	FRAME_SCALE_LEVEL	next_frame_size_selector;
	int					frame_width[FRAME_SCALE_STEPS];
	int					frame_height[FRAME_SCALE_STEPS];
	int					rf_level_maxq[RATE_FACTOR_LEVELS];

	uint64		avg_source_sad;
	bool		high_source_sad;

	static void static_init() {
		tables8.init(8);
		tables10.init(8);
		tables12.init(8);
	}

	static tables& get_tables(int bit_depth) {
		return bit_depth == 8 ? tables8 : bit_depth == 10 ? tables10 : tables12;
	}
	static int	bits_per_mb(FRAME_TYPE frame_type, int qindex, float correction_factor, int bit_depth) {
		const float			q					= Quantisation::qindex_to_q(qindex, bit_depth);
		int					enumerator			= frame_type == KEY_FRAME ? 2700000 : 1800000;
		// q based adjustment to baseline enumerator
		return int((enumerator + (int(enumerator * q) >> 12)) * correction_factor / q);
	}
	static int	estimate_bits_at_q(FRAME_TYPE frame_type, int q, int mbs, float correction_factor, int bit_depth) {
		const int			bpm					= bits_per_mb(frame_type, q, correction_factor, bit_depth);
		return max(FRAME_OVERHEAD_BITS, (int)((uint64)bpm * mbs) >> BPER_MB_NORMBITS);
	}
	static int	get_default_min_gf_interval(float framerate, int width, int height) {
		// Assume we do not need any constraint lower than 4K 20 fps
		static const float	factor_safe			= 3840 * 2160 * 20.0;
		const float			factor				= width * height * framerate;
		const int			default_interval	= clamp(int(framerate * 0.125f), MIN_GF_INTERVAL, MAX_GF_INTERVAL);
		return factor <= factor_safe ? default_interval : max(default_interval, int(MIN_GF_INTERVAL * factor / factor_safe + 0.5f));
	}
	static int	get_default_max_gf_interval(float framerate, int min_gf_interval) {
		int					interval			= min(MAX_GF_INTERVAL, int(framerate * 0.75f));
		return max(interval + (interval & 1), min_gf_interval);
	}
	static int	get_active_quality(int q, int gfu_boost, int low, int high, int *low_motion_minq, int *high_motion_minq) {
		return	gfu_boost > high	? low_motion_minq[q]
			:	gfu_boost < low		? high_motion_minq[q]
			:	low_motion_minq[q] + (((high - gfu_boost) * (high_motion_minq[q] - low_motion_minq[q])) + ((high - low) >> 1)) / (high - low);
	}
	int adjust_qindex(int qindex, float factor, int bit_depth) {
		float	q	= Quantisation::qindex_to_q(qindex, bit_depth);
		return qindex + compute_qdelta(q, q * factor, bit_depth);
	}
	int compute_qdelta_by_rate(FRAME_TYPE frame_type, int qindex, float rate_target_ratio, int bit_depth) const {
		const int			base_bits_per_mb	= bits_per_mb(frame_type, qindex, 1.0, bit_depth);
		const int			target_bits_per_mb	= int(rate_target_ratio * base_bits_per_mb);

		// Convert the q target to an index
		for (int i = best_quality; i < worst_quality; ++i) {
			if (bits_per_mb(frame_type, i, 1.0, bit_depth) <= target_bits_per_mb)
				return i - qindex;
		}
		return worst_quality - qindex;
	}
	int frame_type_qdelta(int rf_level, int q, int bit_depth) {
		static const struct {float factor; FRAME_TYPE type; } table[RATE_FACTOR_LEVELS] = {
			{1.00f,  INTER_FRAME	},	// INTER_NORMAL
			{1.00f,  INTER_FRAME	},	// INTER_HIGH
			{1.50f,  INTER_FRAME	},	// GF_ARF_LOW
			{1.75f,  INTER_FRAME	},	// GF_ARF_STD
			{2.00f,  KEY_FRAME		},	// KF_STD
		};
		return compute_qdelta_by_rate(table[rf_level].type, q, table[rf_level].factor, bit_depth);
	}
	void init_subsampling(int w, int h, int bit_depth) {
		// Frame dimensions multiplier wrt the native frame size, in 1/16ths, specified for the scale-up case.
		static const int8 frame_scale_factor[FRAME_SCALE_STEPS] = { 16, 24 };
		for (int i = 0; i < FRAME_SCALE_STEPS; ++i) {
			frame_width[i]	= w * 16 / frame_scale_factor[i];
			frame_height[i] = h * 16 / frame_scale_factor[i];
		}
		for (int i = INTER_NORMAL; i < RATE_FACTOR_LEVELS; ++i)
			rf_level_maxq[i] = max(worst_quality + frame_type_qdelta(i, worst_quality, bit_depth), best_quality);
	}
	float get_rate_correction_factor(RATE_FACTOR_LEVEL level) const {
		if (level == GF_ARF_STD && is_src_frame_alt_ref)
			level = INTER_NORMAL;
		return clamp(rate_correction_factors[level] * rcf_mult[frame_size_selector], MIN_BPB_FACTOR, MAX_BPB_FACTOR);
	}
	void set_rate_correction_factor(RATE_FACTOR_LEVEL level, float factor) {
		if (level == GF_ARF_STD && is_src_frame_alt_ref)
			level = INTER_NORMAL;
		factor = clamp(factor/ rcf_mult[frame_size_selector], MIN_BPB_FACTOR, MAX_BPB_FACTOR);
	}
	int		get_kf_active_quality(int q, int bit_depth) const {
		tables	&t = get_tables(bit_depth);
		return get_active_quality(q, kf_boost, kf_low, kf_high, t.kf_low, t.kf_high);
	}
	int		get_gf_active_quality(int q, int bit_depth) const {
		tables	&t = get_tables(bit_depth);
		return get_active_quality(q, gfu_boost, gf_low, gf_high, t.arfgf_low, t.arfgf_high);
	}
	int		calc_active_worst_quality_one_pass_vbr(FRAME_TYPE frame_type, int curr_frame) {
		int	active_worst_quality = frame_type == KEY_FRAME
			? (curr_frame == 0 ? worst_quality				: last_q[KEY_FRAME] * 2)
		: !is_src_frame_alt_ref && frame_type == INTER_FRAME
			? (curr_frame == 1 ? last_q[KEY_FRAME] * 5 / 4	: last_q[INTER_FRAME])
			: (curr_frame == 1 ? last_q[KEY_FRAME] * 2		: last_q[INTER_FRAME] * 2);
		return min(active_worst_quality, worst_quality);
	}
	int		calc_active_worst_quality_one_pass_cbr(FRAME_TYPE frame_type) {
		if (frame_type == KEY_FRAME)
			return worst_quality;

		// Adjust active_worst_quality: If buffer is above the optimal/target level, bring active_worst_quality down depending on fullness of buffer.
		// If buffer is below the optimal level, let the active_worst_quality go from ambient Q (at buffer = optimal level) to worst_quality level (at buffer = critical level).
		// Buffer level below which we push active_worst to worst_quality.
		int		ambient_qp				= frame_type == KEYLIKE_FRAME ? min(avg_frame_qindex[INTER_FRAME], avg_frame_qindex[KEY_FRAME]) : avg_frame_qindex[INTER_FRAME];
		int		active_worst_quality	= min(worst_quality, ambient_qp * 5 / 4);
		int64	critical_level			= optimal_buffer_level >> 3;

		if (buffer_level > optimal_buffer_level) {
			// Adjust down. Maximum limit for down adjustment, ~30%.
			if (int max_adjustment_down = active_worst_quality / 3) {
				if (int64 step = ((maximum_buffer_size - optimal_buffer_level) / max_adjustment_down))
					active_worst_quality -= int((buffer_level - optimal_buffer_level) / step);
			}
		} else if (buffer_level > critical_level) {
			// Adjust up from ambient Q.
			if (critical_level) {
				active_worst_quality = ambient_qp;
				if (int64 step = (optimal_buffer_level - critical_level))
					active_worst_quality += int((worst_quality - ambient_qp) * (optimal_buffer_level - buffer_level) / step);
			}
		} else {
			// Set to worst_quality if buffer is below critical level.
			active_worst_quality = worst_quality;
		}
		return active_worst_quality;
	}
	void	update_alt_ref_frame_stats() {
		frames_since_golden		= 0;		// this frame refreshes means next frames don't unless specified by user
		source_alt_ref_pending	= 0;		// Mark the alt ref as done (setting to 0 means no further alt refs pending).
		source_alt_ref_active	= true;		// Set the alternate reference frame active flag
	}
	void	update_golden_frame_stats(bool refresh_golden_frame, bool refresh_alt_ref_frame, bool multi_arf) {
		if (refresh_golden_frame) {
			// If we are not using alt ref in the up and coming group clear the arf active flag
			// In multi arf group case, if the index is not 0 then we are overlaying a mid group arf so should not reset the flag.
			if (!source_alt_ref_pending && !multi_arf)
				source_alt_ref_active = false;
			// this frame refreshes means next frames don't unless specified by user
			frames_since_golden = -1;
		}
		if (refresh_golden_frame || !refresh_alt_ref_frame) {
			// Decrement count down till next gf
			if (frames_till_gf_update_due > 0)
				frames_till_gf_update_due--;
			++frames_since_golden;
		}
	}
	bool	update_golden_frame_stats(int percent_refresh) {
		if (frames_till_gf_update_due != 0)
			return false;

		baseline_gf_interval		= percent_refresh > 0 ? min(4 * (100 / percent_refresh), 40) : (min_gf_interval + max_gf_interval) / 2;
		frames_till_gf_update_due	= baseline_gf_interval;
		// NOTE: frames_till_gf_update_due must be <= frames_to_key.
		if (constrained_gf_group = (frames_till_gf_update_due > frames_to_key))
			frames_till_gf_update_due = frames_to_key;

		gfu_boost				= RATE_CONTROL::DEFAULT_GF_BOOST;
		source_alt_ref_pending	= USE_ALTREF_FOR_ONE_PASS;
		return true;
	}
	int		boost_golden_frame(int boost_pct, bool add) {
		if (boost_pct) {
			return add && !is_src_frame_alt_ref
				? (avg_frame_bandwidth * baseline_gf_interval * (boost_pct + 100)) / (baseline_gf_interval * 100 + boost_pct)
				: (avg_frame_bandwidth * baseline_gf_interval * 100) / (baseline_gf_interval * 100 + boost_pct);
		} else {
			return avg_frame_bandwidth;
		}
	}
	int adjust_target_size(const EncoderConfig &oxcf, int target) {
		const int64 one_pct_bits	= 1 + optimal_buffer_level / 100;
		const int64 diff			= optimal_buffer_level - buffer_level;
		if (diff > 0) {
			// Lower the target bandwidth for this frame.
			target -= (target * min(diff / one_pct_bits, oxcf.under_shoot_pct)) / 200;
		} else if (diff < 0) {
			// Increase the target bandwidth for this frame.
			target += (target * min(-diff / one_pct_bits, oxcf.over_shoot_pct)) / 200;
		}
		return oxcf.rc_max_inter_bitrate_pct ? min(target, avg_frame_bandwidth * oxcf.rc_max_inter_bitrate_pct / 100) : target;
	}

	int		get_active_cq_level(RC_MODE rc_mode, int cq_level) {
		static const float cq_adjust_threshold = 0.1f;
		if (rc_mode == RC_CQ && total_target_bits > 0) {
			const float x = float(total_actual_bits) / total_target_bits;
			if (x < cq_adjust_threshold)
				return int(cq_level * x / cq_adjust_threshold);
		}
		return cq_level;
	}
	bool	update_drop_frame() {
		frames_since_key++;
		frames_to_key--;
		rc_2_frame = 0;
		rc_1_frame = 0;
	}
	bool	drop_frame(const EncoderConfig &oxcf);

	void	set_golden_update(int percent_refresh) {
		baseline_gf_interval = percent_refresh > 0 ? min(4 * (100 / percent_refresh), 40) : 40;
	}

	void	init(const EncoderConfig &oxcf, int pass);

	void RATE_CONTROL::set_keyframe(int key_freq, bool forced) {
		this_key_frame_forced	= forced;
		frames_to_key			= key_freq;
		kf_boost				= DEFAULT_KF_BOOST;
		source_alt_ref_active	= false;
	}

	void	update_rate_correction_factors(RATE_FACTOR_LEVEL level, int projected_size, int base_qindex);
	int		compute_qdelta(double qstart, double qtarget, int bit_depth) const;
	int		clamp_pframe_target_size(const EncoderConfig &oxcf, int target, bool refresh_golden_frame);
	int		clamp_iframe_target_size(const EncoderConfig &oxcf, int target);
	void	update_buffer_level(int encoded_frame_size, bool show_frame, bool can_drop);

	void	set_gf_interval_range(const EncoderConfig &oxcf, float framerate, bool clamp_static);
	void	update_framerate(const EncoderConfig &oxcf, float framerate, bool clamp_static, int mbs);
	int		adjust_target_rate(int target_rate, int max_delta, bool fast);

	void	set_frame_target(const EncoderConfig &oxcf, int target, int mbs);

	int		calc_iframe_target_size_temporal(float framerate) {
		if (framerate == 0)	// first frame
			return min(starting_buffer_level / 2, INT_MAX);
		kf_boost = max(kf_boost, int(2 * framerate - 16));
		if (frames_since_key < framerate / 2)
			kf_boost = int(kf_boost * frames_since_key / (framerate / 2));
		return ((16 + kf_boost) * avg_frame_bandwidth) >> 4;
	}

	int		pick_q_one_pass(RC_MODE rc_mode, int cq_level, FRAME_TYPE frame_type, int worst, int bit_depth, int curr_frame, float size_adj, bool refresh_alt_ref_frame);
	int		pick_q_two_pass(RC_MODE rc_mode, int cq_level, FRAME_TYPE frame_type, int worst, int bit_depth, float size_adj, bool motion, bool refresh_alt_ref_frame);
	float	encodedframe_overshoot(int frame_size, int *q, int qbase_index, int mbs, int bit_depth);
	void	postencode_update(uint64 bytes_used, RATE_FACTOR_LEVEL level, int qindex, FRAME_TYPE frame_type, int bit_depth, bool show_frame, bool can_drop);
};

//-----------------------------------------------------------------------------
//	vp9_aq_cyclicrefresh.h
//-----------------------------------------------------------------------------

struct CYCLIC_REFRESH {
	enum {
		SEGMENT_ID_BASE			= 0,	// The segment ids used in cyclic refresh: from base (no boost) to increasing boost (higher delta-qp).
		SEGMENT_ID_BOOST1		= 1,
		SEGMENT_ID_BOOST2		= 2,
		MAX_RATE_TARGET_RATIO	= 4,	// Maximum rate target ratio for setting segment delta-qp.
	};
	int			percent_refresh;		// Percentage of blocks per frame that are targeted as candidates for cyclic refresh.
	int			max_qdelta_perc;		// Maximum q-delta as percentage of base q.
	int			sb_index;				// Superblock starting index for cycling through the frame.
	int			time_for_refresh;		// Controls how long block will need to wait to be refreshed again, in excess of the cycle time, i.e., in the case of all zero motion, block will be refreshed every (100/percent_refresh + time_for_refresh) frames.
	int			target_num_seg_blocks;	// Target number of (8x8) blocks that are set for delta-q.
	int			actual_num_seg1_blocks;	// Actual number of (8x8) blocks that were applied delta-q.
	int			actual_num_seg2_blocks;
	int			rdmult;					// RD mult. parameters for segment 1.
	int8		*map;					// Cyclic refresh map.
	uint8		*last_coded_q_map;		// Map of the last q a block was coded at.
	uint8		*consec_zero_mv;		// Count on how many consecutive times a block uses ZER0MV for encoding.
	int64		thresh_rate_sb;			// Thresholds applied to the projected rate/distortion of the coding block, when deciding whether block should be refreshed.
	int64		thresh_dist_sb;
	int16		motion_thresh;			// Threshold applied to the motion vector (in units of 1/8 pel) of the coding block, when deciding whether block should be refreshed.
	double		rate_ratio_qdelta;		// Rate target ratio to set q delta.
	int			rate_boost_fac;			// Boost factor for rate target ratio, for segment CR_SEGMENT_ID_BOOST2.
	double		low_content_avg;
	int			qindex_delta[3];
	bool		reduce_refresh;

	CYCLIC_REFRESH() : map(0), last_coded_q_map(0), consec_zero_mv(0) {}
	~CYCLIC_REFRESH() {
		free(map);
		free(last_coded_q_map);
		free(consec_zero_mv);
	}

	void	init(size_t size) {
		map					= (int8*)malloc(size);
		last_coded_q_map	= (uint8*)malloc(size);
		consec_zero_mv		= (uint8*)malloc(size);
		reset_resize(size);
	}

	static bool	segment_id_boosted(int id)	{ return id == SEGMENT_ID_BOOST1 || id == SEGMENT_ID_BOOST2; }
	static int	segment_id(int id)			{ return id == SEGMENT_ID_BOOST1 || id == SEGMENT_ID_BOOST2 ? id : SEGMENT_ID_BASE; }

	// Check if this coding block, of size bsize, should be considered for refresh (lower-qp coding).
	// Decision can be based on various factors, such as size of the coding block (i.e., below min_block size rejected), coding mode, and rate/distortion.
	int candidate_refresh_aq(const ModeInfo *mi, int64 rate, int64 dist, int bsize) const {
		MotionVector mv = mi->sub_mv[3].mv[0];
		// Reject the block for lower-qp coding if projected distortion is above the threshold, and any of the following is true:
		// 1) mode uses large mv
		// 2) mode is an intra-mode
		// Otherwise accept for refresh.
		return dist > thresh_dist_sb && (mv.row > motion_thresh || mv.row < -motion_thresh || mv.col > motion_thresh || mv.col < -motion_thresh || !mi->is_inter_block())
			? SEGMENT_ID_BASE
		: bsize >= BLOCK_16X16 && rate < thresh_rate_sb && mi->is_inter_block() && mv.is_zero() && rate_boost_fac > 10
			? SEGMENT_ID_BOOST2		// More aggressive delta-q for bigger blocks with zero motion.
			: SEGMENT_ID_BOOST1;
	}

	// Update the actual number of blocks that were applied the segment delta q.
	void postencode(int mi_rows, int mi_cols, uint8 *seg_map) {
		actual_num_seg1_blocks = 0;
		actual_num_seg2_blocks = 0;
		for (int mi_row = 0; mi_row < mi_rows; mi_row++) {
			for (int mi_col = 0; mi_col < mi_cols; mi_col++) {
				if (segment_id(seg_map[mi_row * mi_cols + mi_col]) == SEGMENT_ID_BOOST1)
					actual_num_seg1_blocks++;
				else if (segment_id(seg_map[mi_row * mi_cols + mi_col]) == SEGMENT_ID_BOOST2)
					actual_num_seg2_blocks++;
			}
		}
	}

	// Set cyclic refresh parameters.
	void update_parameters(int percent, bool noisy) {
		percent_refresh		= 10;
		if (reduce_refresh)
			percent_refresh = 5;
		max_qdelta_perc		= 50;
		time_for_refresh	= 0;
		rate_ratio_qdelta	= percent_refresh > 0 && percent > percent_refresh ? 3 : noisy ? 1.5f : 2.0;
	}

	void set_motion_thresh(int _motion_thresh, int _rate_boost_fac) {
		motion_thresh = _motion_thresh;
		rate_boost_fac = _rate_boost_fac;
	}

	void reset_resize(size_t size) {
		memset(map,					0,		size);
		memset(last_coded_q_map,	MAXQ,	size);
		memset(consec_zero_mv,		0,		size);
		sb_index = 0;
	}

	int	estimate_bits_at_q(const RATE_CONTROL &rc, FRAME_TYPE frame_type, int base_qindex, float correction_factor, int mbs, int bit_depth) const {
		int	num8x8		= mbs << 2;
		return (
			(num8x8 - actual_num_seg1_blocks - actual_num_seg2_blocks) * rc.estimate_bits_at_q(frame_type, base_qindex, mbs, correction_factor, bit_depth)
			+ actual_num_seg1_blocks * rc.estimate_bits_at_q(frame_type, base_qindex + qindex_delta[1], mbs, correction_factor, bit_depth)
			+ actual_num_seg2_blocks * rc.estimate_bits_at_q(frame_type, base_qindex + qindex_delta[2], mbs, correction_factor, bit_depth)
		) / num8x8;
	}

	int	bits_per_mb(const RATE_CONTROL &rc, FRAME_TYPE frame_type, int base_qindex, float correction_factor, int mbs, int bit_depth, int i) const {
		int			num8x8			= mbs << 2;
		int			target_refresh	= percent_refresh * num8x8 / 100;
		int			weight_segment	= (target_refresh + actual_num_seg1_blocks + actual_num_seg2_blocks) >> 1;
		int			deltaq			= max(rc.compute_qdelta_by_rate(frame_type, i, rate_ratio_qdelta, bit_depth), -max_qdelta_perc * i / 100);
		return (
			(num8x8 - weight_segment)	* rc.bits_per_mb(frame_type, i, correction_factor, bit_depth)
			+ weight_segment			* rc.bits_per_mb(frame_type, i + deltaq, correction_factor, bit_depth)
		) / num8x8;
	}

};

//-----------------------------------------------------------------------------
// vp9_rd.h
//-----------------------------------------------------------------------------

#define INVALID_MV				0x80008000

#define RD_MULT_EPB_RATIO		64
#define SWITCHABLE_INTERP_RATE_FACTOR 1	// Factor to weigh the rate for switchable interp filters.

// This enumerator type needs to be kept aligned with the mode order in const MODE_DEFINITION vp9_mode_order used in the rd code.
enum THR_MODES {
	THR_NEARESTMV,
	THR_NEARESTA,
	THR_NEARESTG,

	THR_DC,

	THR_NEWMV,
	THR_NEWA,
	THR_NEWG,

	THR_NEARMV,
	THR_NEARA,
	THR_NEARG,

	THR_ZEROMV,
	THR_ZEROG,
	THR_ZEROA,

	THR_COMP_NEARESTLA,
	THR_COMP_NEARESTGA,

	THR_TM,

	THR_COMP_NEARLA,
	THR_COMP_NEWLA,
	THR_COMP_NEARGA,
	THR_COMP_NEWGA,

	THR_COMP_ZEROLA,
	THR_COMP_ZEROGA,

	THR_H_PRED,
	THR_V_PRED,
	THR_D135_PRED,
	THR_D207_PRED,
	THR_D153_PRED,
	THR_D63_PRED,
	THR_D117_PRED,
	THR_D45_PRED,
};

enum THR_MODES_SUB8X8 {
	THR_LAST,
	THR_GOLD,
	THR_ALTR,
	THR_COMP_LA,
	THR_COMP_GA,
	THR_INTRA,
};

struct RD_OPT {
	enum {
		MAX_MODES				= 30,
		MAX_REFS				= 6,
		RD_THRESH_MAX_FACT		= 64,
		RD_THRESH_INC			= 1,
		RD_DIVBITS				= 7,
		QIDX_SKIP_THRESH		= 115,
		MV_COST_WEIGHT			= 108,
		MV_COST_WEIGHT_SUB		= 120,
		PROB_COST_SHIFT			= 9,
	};
	// Thresh_mult is used to set a threshold for the rd score
	// A higher value means that we will accept the best mode so far more often
	// This number is used in combination with the current block size, and thresh_freq_fact to pick a threshold.
	int		thresh_mult[MAX_MODES];
	int		thresh_mult_sub8x8[MAX_REFS];
	int		threshes[MAX_SEGMENTS][BLOCK_SIZES][MAX_MODES];
	int64	prediction_type_threshes[REFFRAMES][REFMODES];
	int64	filter_threshes[REFFRAMES][SWITCHABLE_FILTER_CONTEXTS];

	int		rdmult;
	int		rddiv;

	static int	cost(int rdmult, int rddiv, int rate, int error) {
		return round_pow2((int64)rate * rdmult, PROB_COST_SHIFT) + (error << rddiv);
	}
	static int	intra_cost_penalty(int qindex, int qdelta, int bit_depth) {
		return round_pow2(Quantisation::dc_quant(qindex, qdelta, bit_depth) * 20, bit_depth - 8);
	}
	static int thresh_factor(int qindex, int bit_depth) {
		float q = Quantisation::dc_quant(qindex, 0, bit_depth) / float(Quantisation::q_scale(bit_depth));
		return max(int(pow(q, 1.25f) * 5.12f), 8);
	}
	static void from_var_lapndz(uint32 var, uint32 n_log2, uint32 qstep, int *rate, int64 *dist);
	static void update_rd_thresh_fact(int(*factor_buf)[MAX_MODES], int rd_thresh, int bsize, int best_mode_index);

	void	set_block_thresholds(Segmentation &seg, Quantisation &quant, int bit_depth);
	void	set_speed_thresholds(bool best, bool adaptive);
};

struct RD_COST {
	int		rate;
	int64	dist;
	int64	rdcost;

	void reset() {
		rate	= maximum;
		dist	= maximum;
		rdcost	= maximum;
	}

	void init() {
		rate	= 0;
		dist	= 0;
		rdcost	= 0;
	}
};

//-----------------------------------------------------------------------------
// vp9_quantize.h
//-----------------------------------------------------------------------------

struct QUANTS {
	DECLARE_ALIGNED(16, int16, y_quant[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, y_quant_shift[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, y_zbin[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, y_round[QINDEX_RANGE][8]);

	// TODO(jingning): in progress of re-working the quantization. will decide
	// if we want to deprecate the current use of y_quant.
	DECLARE_ALIGNED(16, int16, y_quant_fp[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, uv_quant_fp[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, y_round_fp[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, uv_round_fp[QINDEX_RANGE][8]);

	DECLARE_ALIGNED(16, int16, uv_quant[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, uv_quant_shift[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, uv_zbin[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, uv_round[QINDEX_RANGE][8]);
};

//-----------------------------------------------------------------------------
// vp9_speed_features.h
//-----------------------------------------------------------------------------

#define MAX_MESH_STEP 4

struct SPEED_FEATURES {
	enum {
		INTRA_ALL				= (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED) | (1 << D45_PRED) | (1 << D135_PRED) | (1 << D117_PRED) | (1 << D153_PRED) | (1 << D207_PRED) | (1 << D63_PRED) | (1 << TM_PRED),
		INTRA_DC				= (1 << DC_PRED),
		INTRA_DC_TM				= (1 << DC_PRED) | (1 << TM_PRED),
		INTRA_DC_H_V			= (1 << DC_PRED) | (1 << V_PRED) | (1 << H_PRED),
		INTRA_DC_TM_H_V			= (1 << DC_PRED) | (1 << TM_PRED) | (1 << V_PRED) | (1 << H_PRED),

		INTER_ALL				= (1 << NEARESTMV) | (1 << NEARMV) | (1 << ZEROMV) | (1 << NEWMV),
		INTER_NEAREST			= (1 << NEARESTMV),
		INTER_NEAREST_NEW		= (1 << NEARESTMV) | (1 << NEWMV),
		INTER_NEAREST_ZERO		= (1 << NEARESTMV) | (1 << ZEROMV),
		INTER_NEAREST_NEW_ZERO	= (1 << NEARESTMV) | (1 << ZEROMV) | (1 << NEWMV),
		INTER_NEAREST_NEAR_NEW	= (1 << NEARESTMV) | (1 << NEARMV) | (1 << NEWMV),
		INTER_NEAREST_NEAR_ZERO	= (1 << NEARESTMV) | (1 << NEARMV) | (1 << ZEROMV),

		DISABLE_ALL_INTER_SPLIT = (1 << THR_COMP_GA) | (1 << THR_COMP_LA) | (1 << THR_ALTR) | (1 << THR_GOLD) | (1 << THR_LAST),
		DISABLE_ALL_SPLIT		= (1 << THR_INTRA) | DISABLE_ALL_INTER_SPLIT,
		DISABLE_COMPOUND_SPLIT	= (1 << THR_COMP_GA) | (1 << THR_COMP_LA),
		LAST_AND_INTRA_SPLIT_ONLY = (1 << THR_COMP_GA) | (1 << THR_COMP_LA) | (1 << THR_ALTR) | (1 << THR_GOLD)
	};

	enum MOTION_THRESHOLD {
		NO_MOTION_THRESHOLD			= 0,
		LOW_MOTION_THRESHOLD		= 7
	};

	enum TX_SIZE_SEARCH_METHOD {
		USE_FULL_RD					= 0,
		USE_LARGESTALL,
		USE_TX_8X8
	};

	enum AUTO_MIN_MAX_MODE {
		NOT_IN_USE					= 0,
		RELAXED_NEIGHBORING_MIN_MAX = 1,
		STRICT_NEIGHBORING_MIN_MAX	= 2
	};

	enum LPF_PICK_METHOD {
		LPF_PICK_FROM_FULL_IMAGE,	// Try the full image with different values.
		LPF_PICK_FROM_SUBIMAGE,		// Try a small portion of the image with different values.
		LPF_PICK_FROM_Q,			// Estimate the level based on quantizer and frame type
		LPF_PICK_MINIMAL_LPF		// Pick 0 to disable LPF if LPF was enabled last frame
	};

	enum MODE_SEARCH_SKIP_LOGIC {
		FLAG_EARLY_TERMINATE		= 1 << 0,	// Terminate search early based on distortion so far compared to qp step, distortion in the neighborhood of the frame, etc.
		FLAG_SKIP_COMP_BESTINTRA	= 1 << 1,	// Skips comp inter modes if the best so far is an intra mode.
		FLAG_SKIP_INTRA_BESTINTER	= 1 << 3,	// Skips oblique intra modes if the best so far is an inter mode.
		FLAG_SKIP_INTRA_DIRMISMATCH	= 1 << 4,	// Skips oblique intra modes  at angles 27, 63, 117, 153 if the best intra so far is not one of the neighboring directions.
		FLAG_SKIP_INTRA_LOWVAR		= 1 << 5,	// Skips intra modes other than DC_PRED if the source variance is small
	};

	enum INTERP_FILTER_MASK {
		FLAG_SKIP_EIGHTTAP			= 1 << INTERP_8TAP,
		FLAG_SKIP_EIGHTTAP_SMOOTH	= 1 << INTERP_8TAP_SMOOTH,
		FLAG_SKIP_EIGHTTAP_SHARP	= 1 << INTERP_8TAP_SHARP,
	};

	enum PARTITION_SEARCH_TYPE {
		SEARCH_PARTITION,			// Search partitions using RD/NONRD criterion
		FIXED_PARTITION,			// Always use a fixed size partition
		REFERENCE_PARTITION,
		VAR_BASED_PARTITION,		// Use an arbitrary partitioning scheme based on source variance within a 64X64 SB
		SOURCE_VAR_BASED_PARTITION,	// Use non-fixed partitions based on source variance
	};
	enum RECODE_LOOP_TYPE {
		DISALLOW_RECODE,		// No recode.
		ALLOW_RECODE_KFMAXBW,	// Allow recode for KF and exceeding maximum frame bandwidth.
		ALLOW_RECODE_KFARFGF,	// Allow recode only for KF/ARF/GF frames.
		ALLOW_RECODE,			// Allow recode for all frames based on bitrate constraints.
	};
	enum FAST_COEFF_UPDATE {
		TWO_LOOP					= 0,	// Does a dry run to see if any of the contexts need to be updated or not, before the final run.
		ONE_LOOP_REDUCED			= 1,	// No dry run, also only half the coef contexts and bands are updated. The rest are not updated at all.
	};

	struct MV {
		enum SEARCH_METHODS {
			DIAMOND,
			NSTEP,
			HEX,
			BIGDIA,
			SQUARE,
			FAST_HEX,
			FAST_DIAMOND,
		};
		enum SUBPEL_SEARCH_METHODS {
			SUBPEL_TREE,
			SUBPEL_TREE_PRUNED,				// Prunes 1/2-pel searches
			SUBPEL_TREE_PRUNED_MORE,		// Prunes 1/2-pel searches more aggressively
			SUBPEL_TREE_PRUNED_EVENMORE,	// Prunes 1/2- and 1/4-pel searches
		};

		SEARCH_METHODS search_method;				// Motion search method (Diamond, NSTEP, Hex, Big Diamond, Square, etc).
		bool	reduce_first_step_size;				// This parameter controls which step in the n-step process we start at. It's changed adaptively based on circumstances.
		bool	auto_mv_step_size;					// If this is set to 1, we limit the motion search range to 2 times thelargest motion vector found in the last frame.

		SUBPEL_SEARCH_METHODS subpel_search_method;	// Subpel_search_method can only be subpel_tree which does a subpixel logarithmic search that keeps stepping at 1/2 pixel units until you stop getting a gain, and then goes on to 1/4 and repeats the same process. Along the way it skips many diagonals.
		int		subpel_iters_per_step;				// Maximum number of steps in logarithmic subpel search before giving up.
		int		subpel_force_stop;					// Control when to stop subpel search
		int		fullpel_search_step_param;			// This variable sets the step_param used in full pel motion search.

		int *cond_cost_list(int *cost_list)  const { return subpel_search_method != SUBPEL_TREE ? cost_list : NULL;	}
	} mv;

	struct MESH_PATTERN {
		int		range;
		int		interval;
	};

	RECODE_LOOP_TYPE recode_loop;

	bool	frame_parameter_update;	// Frame level coding parameter update
	bool	optimize_coefficients;	// Trellis (dynamic programming) optimization of quantized values (+1, 0).

	// Always set to 0. If on it enables 0 cost background transmission (except for the initial transmission of the segmentation).
	// The feature is disabled because the addition of very large block sizes make the backgrounds very to cheap to encode, and the segmentation we have adds overhead.
	bool	static_segmentation;

	// If 1 we iterate finding a best reference for 2 ref frames together - via a log search that iterates 4 times (check around mv for last for best
	// error of combined predictor then check around mv for alt). If 0 we we just use the best motion vector found for each frame by itself.
	BLOCK_SIZE	comp_inter_joint_search_thresh;

	int		adaptive_rd_thresh;						// This variable is used to cap the maximum number of times we skip testing a mode to be evaluated. A high value means we will be faster.
	bool	skip_encode_sb;							// Enables skipping the reconstruction step (idct, recon) in the intermediate steps assuming the last frame didn't have too many intra blocks and the q is less than a threshold.
	int		skip_encode_frame;
	bool	allow_skip_recode;						// Speed feature to allow or disallow skipping of recode at block level within a frame.
	int		coeff_prob_appx_step;					// Coefficient probability model approximation step size
	MOTION_THRESHOLD lf_motion_threshold;			// The threshold is to determine how slow the motino is, it is used when use_lastframe_partitioning is set to LAST_FRAME_PARTITION_LOW_MOTION
	TX_SIZE_SEARCH_METHOD tx_size_search_method;	// Determine which method we use to determine transform size. We can choose between options like full rd, largest for prediction size, largest for intra and model coefs for the rest.
	bool	use_lp32x32fdct;						// Low precision 32x32 fdct keeps everything in 16 bits and thus is less precise but significantly faster than the non lp version.
	int		mode_skip_start;						// After looking at the first set of modes (set by index here), skip checking modes for reference frames that don't match the reference frame of the best so far.
	bool	reference_masking;						// TODO(JBB): Remove this.

	PARTITION_SEARCH_TYPE partition_search_type;

	BLOCK_SIZE	always_this_block_size;				// Used if partition_search_type = FIXED_SIZE_PARTITION
	bool		less_rectangular_check;				// Skip rectangular partition test when partition type none gives better rd than partition type split.
	bool		use_square_partition_only;			// Disable testing non square partitions. (eg 16x32)
	BLOCK_SIZE	use_square_only_threshold;

	AUTO_MIN_MAX_MODE auto_min_max_partition_size;	// Sets min and max partition sizes for this 64x64 region based on the same 64x64 in last encoded frame, and the left and above neighbor.
	BLOCK_SIZE rd_auto_partition_min_limit;			// Ensures the rd based auto partition search will always go down at least to the specified level.

	// Min and max partition size we enable (block_size) as per automin max, but also used by adjust partitioning, and pick_partitioning.
	BLOCK_SIZE default_min_partition_size;
	BLOCK_SIZE default_max_partition_size;

	bool	adjust_partitioning_from_last_frame;	// Whether or not we allow partitions one smaller or one greater than the last frame's partitioning. Only used if use_lastframe_partitioning is set.
	int		last_partitioning_redo_frequency;		// How frequently we re do the partitioning from scratch. Only used if use_lastframe_partitioning is set.
	int		disable_split_mask;						// Disables sub 8x8 blocksizes in different scenarios: Choices are to disable it always, to allow it for only Last frame and Intra, disable it for all inter modes or to enable it always.
	bool	adaptive_motion_search;					// TODO(jingning): combine the related motion search speed features This allows us to use motion search at other sizes as a starting point for this motion search and limits the search range around it.
	bool	allow_exhaustive_searches;				// Flag for allowing some use of exhaustive searches;
	int		exhaustive_searches_thresh;				// Threshold for allowing exhaistive motion search.
	int		max_exaustive_pct;						// Maximum number of exhaustive searches for a frame.
	const MESH_PATTERN *mesh_patterns;				// Pattern to be used for any exhaustive mesh searches.
	bool	schedule_mode_search;
	int		adaptive_pred_interp_filter;			// Allows sub 8x8 modes to use the prediction filter that was determined best for 8x8 mode. If set to 0 we always re check all the filters for sizes less than 8x8, 1 means we check all filter modes if no 8x8 filter was selected, and 2 means we use 8 tap if no 8x8 filter mode was selected.
	bool	adaptive_mode_search;					// Adaptive prediction mode search

	// Chessboard pattern prediction filter type search
	bool	cb_pred_filter_search;
	bool	cb_partition_search;

	bool	motion_field_mode_search;
	bool	alt_ref_search_fp;
	bool	use_quant_fp;							// Fast quantization process path
	bool	force_frame_boost;						// Use finer quantizer in every other few frames that run variable block partition type search.
	int		max_delta_qindex;						// Maximally allowed base quantization index fluctuation.
	uint32 mode_search_skip_flags;					// Implements various heuristics to skip searching modes The heuristics selected are based on  flags defined in the MODE_SEARCH_SKIP_HEURISTICS enum

	uint32 disable_filter_search_var_thresh;		// A source variance threshold below which filter search is disabled Choose a very large value (UINT_MAX) to use 8-tap always

	// These bit masks allow you to enable or disable intra modes for each transform size separately.
	int		intra_y_mode_mask[TX_SIZES];
	int		intra_uv_mode_mask[TX_SIZES];

	// These bit masks allow you to enable or disable intra modes for each prediction block size separately.
	int		intra_y_mode_bsize_mask[BLOCK_SIZES];

	bool	use_rd_breakout;						// This variable enables an early break out of mode testing if the model for rd built from the prediction signal indicates a value that's much higher than the best rd we've seen so far.
	bool	use_uv_intra_rd_estimate;				// This enables us to use an estimate for intra rd based on dc mode rather than choosing an actual uv mode in the stage of encoding before the actual final encode.
	LPF_PICK_METHOD lpf_pick;						// This feature controls how the loop filter level is determined.
	FAST_COEFF_UPDATE use_fast_coef_updates;		// This feature limits the number of coefficients updates we actually do by only looking at counts from 1/2 the bands.
	bool	use_nonrd_pick_mode;					// This flag controls the use of non-RD mode decision.
	int		inter_mode_mask[BLOCK_SIZES];			// A binary mask indicating if NEARESTMV, NEARMV, ZEROMV, NEWMV modes are used in order from LSB to MSB for each BLOCK_SIZE.
	bool	use_fast_coef_costing;					// This feature controls whether we do the expensive context update and calculation in the rd coefficient costing loop.
	int		recode_tolerance;						// This feature controls the tolerence vs target used in deciding whether to recode a frame. It has no meaning if recode is disabled.

	BLOCK_SIZE max_intra_bsize;						// This variable controls the maximum block size where intra blocks can be used in inter frames. TODO(aconverse): Fold this into one of the other many mode skips
	int		search_type_check_frequency;			// The frequency that we check if SOURCE_VAR_BASED_PARTITION or FIXED_PARTITION search type should be used.
	bool	reuse_inter_pred_sby;					// When partition is pre-set, the inter prediction result from pick_inter_mode can be reused in final block encoding process. It is enabled only for real-time mode speed 6.

	int		encode_breakout_thresh;					// This variable sets the encode_breakout threshold. Currently, it is only enabled in real time mode.

	INTERP_FILTER default_interp_filter;			// default interp filter choice
	bool	tx_size_search_breakout;				// Early termination in transform size search, which only applies while tx_size_search_method is USE_FULL_RD.
	bool	adaptive_interp_filter_search;			// adaptive interp_filter search to allow skip of certain filter types.

	INTERP_FILTER_MASK interp_filter_search_mask;	// mask for skip evaluation of certain interp_filter type.

	// Partition search early breakout thresholds.
	int64	partition_search_breakout_dist_thr;
	int		partition_search_breakout_rate_thr;

	bool	allow_partition_search_skip;			// Allow skipping partition search for still image frame
	bool	simple_model_rd_from_var;				// Fast approximation of vp9_model_rd_from_var_lapndz
	bool	short_circuit_flat_blocks;				// Skip a number of expensive mode evaluations for blocks with zero source variance.

	void	set_realtime(int speed, TUNE_CONTENT content, int frames_since_key, bool inter);
	void	set_realtime_framesize_dependent(int width, int height, int speed, bool show_frame);

	void	set_good_framesize_dependent(int width, int height, int speed, int base_qindex, bool show_frame, bool animation);
	void	set_good(int speed, bool boosted, bool animation, bool inter);

	void	set_framesize_independent();
	void	set_meshes(int speed, bool animation);
};

//-----------------------------------------------------------------------------
// vp9_svc_layercontext.h
//-----------------------------------------------------------------------------

struct LAYER_CONTEXT {
	RATE_CONTROL	rc;
	int				target_bandwidth;
	int				spatial_layer_target_bandwidth;  // Target for the spatial layer.
	double			framerate;
	int				avg_frame_size;
	int				max_q;
	int				min_q;
	int				scaling_factor_num;
	int				scaling_factor_den;
	TWO_PASS		twopass;
	memory_block	rc_twopass_stats_in;
	uint32			current_video_frame_in_layer;
	int				is_key_frame;
	int				frames_from_key_frame;
	FRAME_TYPE		last_frame_type;
	lookahead::entry	*alt_ref_source;
	int				alt_ref_idx;
	int				gold_ref_idx;
	int				has_alt_frame;
	size_t			layer_size;
	PSNR			psnr;
	// Cyclic refresh parameters (aq-mode=3), that need to be updated per-frame.
	int				sb_index;
	int8			*map;
	uint8			*last_coded_q_map;
	uint8			*consec_zero_mv;
};

struct SVC {
	int		spatial_layer_id;
	int		temporal_layer_id;
	int		number_spatial_layers;
	int		number_temporal_layers;

	int		spatial_layer_to_encode;
	int		first_spatial_layer_to_encode;
	int		rc_drop_superframe;

	// Workaround for multiple frame contexts
	enum {
		ENCODED = 0,
		ENCODING,
		NEED_TO_ENCODE
	} encode_empty_frame_state;

	lookahead::entry empty_frame;
	int		encode_intra_empty_frame;

	// Store scaled source frames to be used for temporal filter to generate a alt ref frame.
	FrameBuffer	scaled_frames[MAX_LAG_BUFFERS];

	// Layer context used for rate control in one pass temporal CBR mode or two pass spatial mode.
	LAYER_CONTEXT layer_context[VPX_MAX_LAYERS];
	// Indicates what sort of temporal layering is used.
	// Currently, this only works for CBR mode.
	TEMPORAL_LAYERING_MODE temporal_layering_mode;
	// Frame flags and buffer indexes for each spatial layer, set by the application (external settings).
	int		ext_frame_flags[VPX_MAX_LAYERS];
	int		ext_lst_fb_idx[VPX_MAX_LAYERS];
	int		ext_gld_fb_idx[VPX_MAX_LAYERS];
	int		ext_alt_fb_idx[VPX_MAX_LAYERS];
	int		ref_frame_index[Common::REFERENCE_FRAMES];
	int		force_zero_mode_spatial_ref;
	int		current_superframe;
	int		use_base_mv;

	int		index(int i) const {
		return spatial_layer_id * number_temporal_layers + i;
	}

	// Update the buffer level for higher temporal layers, given the encoded current temporal layer.
	void update_layer_buffer_level(int encoded_frame_size) {
		for (int i = temporal_layer_id + 1; i < number_temporal_layers; ++i) {
			LAYER_CONTEXT	&lc		= layer_context[index(i)];
			RATE_CONTROL	&rc		= lc.rc;
			int bits_off_for_this_layer = (int)(lc.target_bandwidth / lc.framerate - encoded_frame_size);
			rc.bits_off_target += bits_off_for_this_layer;

			// Clip buffer level to maximum buffer size for the layer.
			rc.bits_off_target	= min(rc.bits_off_target, rc.maximum_buffer_size);
			rc.buffer_level		= rc.bits_off_target;
		}
	}
};

//-----------------------------------------------------------------------------
// vp9_encoder.h
//-----------------------------------------------------------------------------

struct CodingContext : FrameContext {
	int		nmvjointcost[MotionVector::JOINTS];
	int		nmvcosts[2][MotionVector::VALS];
	int		nmvcosts_hp[2][MotionVector::VALS];

	prob	segment_pred_probs[PREDICTION_PROBS];
	uint8	*seg_map;

	// 0 = Intra, Last, GF, ARF
	int8	last_ref_lf_deltas[LoopFilter::MAX_REF_LF_DELTAS];
	// 0 = ZERO_MV, MV
	int8	last_mode_lf_deltas[LoopFilter::MAX_MODE_LF_DELTAS];
};

enum SCALING {
	NORMAL		= 0,
	FOURFIVE	= 1,
	THREEFIVE	= 2,
	ONETWO		= 3
};

enum FRAMETYPE_FLAGS {
	FRAMEFLAGS_KEY		= 1 << 0,
	FRAMEFLAGS_GOLDEN	= 1 << 1,
	FRAMEFLAGS_ALTREF	= 1 << 2,
};

// TODO(jingning) All spatially adaptive variables should go to TileDataEnc.
struct TileDataEnc {
	TILE_INFO	tile_info;
	int			thresh_freq_fact[BLOCK_SIZES][RD_OPT::MAX_MODES];
	int			mode_map[BLOCK_SIZES][RD_OPT::MAX_MODES];
};

struct RD_COUNTS {
	typedef Bands<uint32[REF_TYPES]>  coeff_count[ENTROPY_TOKENS];

	coeff_count coef_counts[TX_SIZES][PLANE_TYPES];
	int64		comp_pred_diff[REFMODES];
	int64		filter_diff[SWITCHABLE_FILTER_CONTEXTS];
	int			m_search_count;
	int			ex_search_count;
};
/*
struct ThreadData {
	MACROBLOCK			mb;
	RD_COUNTS			rd_counts;
	FrameCounts			*counts;

	PICK_MODE_CONTEXT	*leaf_tree;
	PC_TREE				*pc_tree;
	PC_TREE				*pc_root;
};

struct EncWorkerData;
*/

struct ActiveMap {
	bool	enabled, update;
	uint8	*map;
};

struct IMAGE_STAT {
	enum TYPE {
		Y, U, V, ALL
	};
	double stat[ALL + 1];
	double worst;
};

struct POST_PROCESS {
	enum FLAGS {
		NOFILTERING				= 0,
		DEBLOCK					= 1 << 0,
		DEMACROBLOCK			= 1 << 1,
		ADDNOISE				= 1 << 2,
		DEBUG_TXT_FRAME_INFO	= 1 << 3,
		DEBUG_TXT_MBLK_MODES	= 1 << 4,
		DEBUG_TXT_DC_DIFF		= 1 << 5,
		DEBUG_TXT_RATE_INFO		= 1 << 6,
		DEBUG_DRAW_MV			= 1 << 7,
		DEBUG_CLR_BLK_MODES		= 1 << 8,
		DEBUG_CLR_FRM_REF_BLKS	= 1 << 9,
		MFQE					= 1 << 10
	};
	FLAGS	flags;
	int	deblocking_level;
	int	noise_level;
};

struct Encoder : Common {
	enum ENCODE_BREAKOUT_TYPE {
		ENCODE_BREAKOUT_DISABLED	= 0,	// encode_breakout is disabled.
		ENCODE_BREAKOUT_ENABLED		= 1,	// encode_breakout is enabled.
		ENCODE_BREAKOUT_LIMITED		= 2,	// encode_breakout is enabled with small max_thresh limit.
	};
	QUANTS				quants;
	MB_ModeInfo_EXT	*mbmi_ext_base;
	DECLARE_ALIGNED(16, int16, y_dequant[QINDEX_RANGE][8]);
	DECLARE_ALIGNED(16, int16, uv_dequant[QINDEX_RANGE][8]);
	EncoderConfig		oxcf;
	lookahead			*lookahead;
	lookahead::entry	*alt_ref_source;

	FrameBuffer			*Source;
	FrameBuffer			*Last_Source;  // NULL for first frame and alt_ref frames
	FrameBuffer			*un_scaled_source;
	FrameBuffer			scaled_source;
	FrameBuffer			*unscaled_last_source;
	FrameBuffer			scaled_last_source;

	TileDataEnc			*tile_data;
	int					allocated_tiles;  // Keep track of memory allocated for tiles.

	// For a still frame, this flag is set to 1 to skip partition search.
	int					partition_search_skippable_frame;

	ref_ptr<FrameBuffer>	scaled_ref_map[REFFRAMES];
	int					lst_fb_idx;
	int					gld_fb_idx;
	int					alt_fb_idx;

	bool				refresh_last_frame;
	bool				refresh_golden_frame;
	bool				refresh_alt_ref_frame;

	bool				ext_refresh_frame_flags_pending;
	bool				ext_refresh_last_frame;
	bool				ext_refresh_golden_frame;
	bool				ext_refresh_alt_ref_frame;

	bool				ext_refresh_frame_context_pending;
	bool				ext_refresh_frame_context;

	FrameBuffer			last_frame_uf;

	TOKENEXTRA			*tile_tok[4][1 << 6];
	uint32				tok_count[4][1 << 6];

	// Ambient reconstruction err target for force key frames
	int64				ambient_err;

	RD_OPT				rd;

	CodingContext		coding_context;

	int					*nmvcosts[2];
	int					*nmvcosts_hp[2];
	int					*nmvsadcosts[2];
	int					*nmvsadcosts_hp[2];

	int64				last_time_stamp_seen;
	int64				last_end_time_stamp_seen;
	int64				first_time_stamp_ever;

	RATE_CONTROL		rc;
	double				framerate;

	int					interp_filter_selected[REFFRAMES][INTERP_SWITCHABLE];

	dynamic_array<PKT>	output_pkt_list;

	MBGRAPH_FRAME_STATS mbgraph_stats[MAX_LAG_BUFFERS];
	int					mbgraph_n_frames;             // number of frames filled in the above
	int					static_mb_pct;                // % forced skip mbs by segmentation
	int					ref_frame_flags;

	SPEED_FEATURES		sf;

	uint32				max_mv_magnitude;
	int					mv_step_param;

	int					allow_comp_inter_inter;

	// Default value is 1. From first pass stats, encode_breakout may be disabled.
	ENCODE_BREAKOUT_TYPE allow_encode_breakout;

	// Get threshold from external input. A suggested threshold is 800 for HD clips, and 300 for < HD clips.
	int					encode_breakout;

	uint8				*segmentation_map;

	// segment threashold for encode breakout
	int					segment_encode_breakout[MAX_SEGMENTS];

	CYCLIC_REFRESH		cr;
	ActiveMap			active_map;
/*
	fractional_mv_step_fp	*find_fractional_mv_step;
	vp9_full_search_fn_t	full_search_sad;
	vp9_diamond_search_fn_t diamond_search_sad;
	*/

	uint64				time_receive_data;
	uint64				time_compress_data;
	uint64				time_pick_lpf;
	uint64				time_encode_sb_row;

#if CONFIG_FP_MB_STATS
	int					use_fp_mb_stats;
#endif

	TWO_PASS			twopass;

	FrameBuffer			alt_ref_buffer;

	int					b_calculate_psnr;

	int					droppable;

	int					initial_width;
	int					initial_height;
	int					initial_mbs;  // Number of MBs in the full-size frame; to be used to normalize the firstpass stats. This will differ from the number of MBs in the current frame when the frame is scaled.

	int					use_svc;
	SVC					svc;

	// Store frame variance info in SOURCE_VAR_BASED_PARTITION search type.
	DIFF				*source_diff_var;
	// The threshold used in SOURCE_VAR_BASED_PARTITION search type.
	uint32				source_var_thresh;
	int					frames_till_next_var_check;
	int					frame_flags;
	int					curr_frame;
	int					MBs;

	SEARCH_CONFIG		ss_cfg;

	uint32				mbmode_cost[INTRA_MODES];
	uint32				inter_mode_cost[INTER_MODE_CONTEXTS][INTER_MODES];
	uint32				intra_uv_mode_cost[FRAME_TYPES][INTRA_MODES][INTRA_MODES];
	uint32				y_mode_costs[INTRA_MODES][INTRA_MODES][INTRA_MODES];
	uint32				switchable_interp_costs[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
	uint32				partition_cost[PARTITION_CONTEXTS][PARTITION_TYPES];

	bool				multi_arf_allowed;
	bool				multi_arf_enabled;
	bool				multi_arf_last_grp_enabled;

#if CONFIG_VP9_TEMPORAL_DENOISING
	VP9_DENOISER		denoiser;
#endif

	RESIZE_STATE		resize_state;
	bool				resize_pending;
	int					resize_scale_num;
	int					resize_scale_den;
	int					resize_avg_qp;
	int					resize_buffer_underflow;
	int					resize_count;

	bool				use_skin_detection;

	NOISE_ESTIMATE		noise_estimate;

	// VAR_BASED_PARTITION thresholds
	// 0 - threshold_64x64; 1 - threshold_32x32;
	// 2 - threshold_16x16; 3 - vbp_threshold_8x8;
	int64				vbp_thresholds[4];
	int64				vbp_threshold_minmax;
	int64				vbp_threshold_sad;
	BLOCK_SIZE			vbp_bsize_min;

	static bool static_init();

	Encoder(EncoderConfig *oxcf);
	~Encoder();

	void	change_config(const EncoderConfig *oxcf);

	int		get_switchable_rate(const TileDecoder *xd) const {
		return SWITCHABLE_INTERP_RATE_FACTOR * switchable_interp_costs[xd->get_pred_context_switchable_interp()][xd->mi[0]->interp_filter];
	}

	bool	enable_noise_estimation() const {
		// Enable noise estimation if denoising is on (and cyclic refresh, since noise estimate is currently using a struct defined in cyclic refresh).
	#if CONFIG_VP9_TEMPORAL_DENOISING
		if (oxcf.noise_sensitivity > 0 && oxcf.aq_mode == AQ_CYCLIC_REFRESH)
			return 1;
	#endif
		// Only allow noise estimate under certain encoding mode.
		// Enabled for 1 pass CBR, speed >=5, and if resolution is same as original.
		// Not enabled for SVC mode and screen_content_mode.
		// Not enabled for low resolutions.
		return	oxcf.pass		== 0
			&&	oxcf.rc_mode	== RC_CBR
			&&	oxcf.aq_mode	== AQ_CYCLIC_REFRESH
			&&	oxcf.speed		>= 5
			&&	resize_state	== RESIZE_ORIG
			&&	resize_pending	== 0
			&&	!use_svc
			&&	oxcf.content	!= CONTENT_SCREEN
			&&	width			>= 640
			&&	height			>= 480;
	}

	// receive a frames worth of data. caller can assume that a copy of this frame is made and not just a copy of the pointer..
	int		receive_raw_frame(uint32 frame_flags, FrameBuffer *sd, int64 time_stamp, int64 end_time_stamp);
	int		get_compressed_data(uint32 *frame_flags, size_t *size, uint8 *dest, int64 *time_stamp, int64 *time_end, int flush);
	int		get_preview_raw_frame(FrameBuffer *dest, POST_PROCESS *pp);
	int		use_as_reference(int ref_frame_flags);
	void	update_reference(int ref_frame_flags);
	int		copy_reference_enc(uint32 ref_frame_flag, FrameBuffer *sd);
	int		set_reference_enc(uint32 ref_frame_flag, FrameBuffer *sd);
	int		update_entropy(bool update);
	int		set_active_map(uint8 *map, int rows, int cols);
	int		get_active_map(uint8 *map, int rows, int cols);
	int		set_internal_size(SCALING horiz_mode, SCALING vert_mode);
	int		set_size_literal(uint32 width, uint32 height);
	void	set_svc(int use_svc);
	int		get_quantizer();

	int		get_ref_frame_map_idx(REFERENCE_FRAME ref_frame) const {
		return	ref_frame == REFFRAME_LAST		? lst_fb_idx
			:	ref_frame == REFFRAME_GOLDEN	? gld_fb_idx
			:	alt_fb_idx;
	}
	FrameBuffer *get_ref_frame_buffer(REFERENCE_FRAME ref_frame) {
		const int map_idx = get_ref_frame_map_idx(ref_frame);
		return map_idx != -1 ? ref_frame_map[map_idx] : 0;
	}
	FrameBuffer *get_scaled_ref_frame(REFERENCE_FRAME ref_frame) const {
		return scaled_ref_map[ref_frame - 1];
	}
	void	scale_references();
	void	release_scaled_references();
	void	update_reference_frames();
	void	set_high_precision_mv(bool hp);
	void	apply_encoding_flags(uint32 flags);

	bool	frame_is_kf_gf_arf()	const { return frame_is_intra_only() || refresh_alt_ref_frame || (refresh_golden_frame && !rc.is_src_frame_alt_ref); }
	bool	is_two_pass_svc()		const { return use_svc && oxcf.pass != 0; }
	bool	is_one_pass_cbr_svc()	const { return use_svc && oxcf.pass == 0; }
	bool	is_upper_layer_key_frame()const {
		return is_two_pass_svc()
			&& svc.spatial_layer_id > 0 
			&& svc.layer_context[svc.spatial_layer_id * svc.number_temporal_layers + svc.temporal_layer_id].is_key_frame;
	}
	bool	is_altref_enabled() const {
		return oxcf.mode != REALTIME
			&& oxcf.lag_in_frames > 0
			&& oxcf.enable_auto_arf
			&& (!is_two_pass_svc() || oxcf.ss_enable_auto_arf[svc.spatial_layer_id]);
	}
	bool	frame_is_boosted()		const {
		return frame_is_kf_gf_arf()
			|| is_upper_layer_key_frame();
	}

	void frame_init_quantizer();
	void init_plane_quantizers(TileEncoder *x);
	void init_quantizer();

	void suppress_active_map();
	void apply_active_map();
	void setup_frame();
	void save_coding_context(TileEncoder *x);
	void restore_coding_context(TileEncoder *x);
	void configure_static_seg_features();
	void update_reference_segmentation_map();
	RETURN alloc_raw_frame_buffers();
	RETURN alloc_util_frame_buffers();
	void update_frame_size();
	void alloc_compressor_data();

	RATE_FACTOR_LEVEL get_rate_factor_level() const {
		return frame_type == KEY_FRAME ? KF_STD
			: oxcf.pass == 2 ? twopass.gf_group.rf_level[twopass.gf_group.index]
			: (refresh_alt_ref_frame || refresh_golden_frame) && !use_svc && (oxcf.rc_mode != RC_CBR || oxcf.gf_cbr_boost_pct > 20) ? GF_ARF_STD
			: INTER_NORMAL;
	}
	float get_rate_correction_factor() const {
		return rc.get_rate_correction_factor(get_rate_factor_level());
	}
	void set_rate_correction_factor(float factor) {
		rc.set_rate_correction_factor(get_rate_factor_level(), factor);
	}
	void update_rate_correction_factors();
	int regulate_q(int target_bits_per_frame, int active_best_quality, int active_worst_quality);

	int		calc_active_worst_quality_one_pass_vbr() {
		return rc.calc_active_worst_quality_one_pass_vbr(refresh_golden_frame || refresh_alt_ref_frame ? INTER_FRAME : ALTREF_FRAME, curr_frame);
	}
	void	postencode_update(uint64 bytes_used);
	void	compute_frame_size_bounds(int frame_target, int *frame_under_shoot_limit, int *frame_over_shoot_limit);
	void	set_size_dependent_vars(int *pq, int *bottom_index, int *top_index);

	int		compute_rd_mult(int qindex) const {
		int64	q		= Quantisation::dc_quant(qindex, 0, cs.bit_depth);
		int64	rdmult	= round_pow2(88 * q * q / 24, (cs.bit_depth - 8) * 2);

		if (oxcf.pass == 2 && frame_type != KEY_FRAME) {
			static const uint8 rd_boost_factor[]		= { 64, 32, 32, 32, 24, 16, 12, 12,  8, 8, 4, 4, 2, 2, 1, 0};
			static const uint8 rd_frame_type_factor[]	= { 128, 144, 128, 128, 144};
			const FRAME_UPDATE_TYPE frame_type	= twopass.gf_group.update_type[twopass.gf_group.index];
			const int				boost_index	= min(15, (rc.gfu_boost / 100));
			rdmult = ((rdmult * rd_frame_type_factor[frame_type]) >> 7) + ((rdmult * rd_boost_factor[boost_index]) >> 7);
		}
		return max(rdmult, 1);
	}
	
	// Compute delta-q for the segment.
	int		compute_deltaq(int q, double rate_factor) const {
		return max(rc.compute_qdelta_by_rate(frame_type, q, rate_factor, cs.bit_depth), -cr.max_qdelta_perc * q / 100);
	}
	int		cyclic_refresh_estimate_bits_at_q(float correction_factor) const {
		return cr.estimate_bits_at_q(rc, frame_type, quant.base_index, correction_factor, MBs, cs.bit_depth);
	}
	int		cyclic_refresh_rc_bits_per_mb(int i, float correction_factor) const {
		return cr.bits_per_mb(rc, frame_type, quant.base_index, correction_factor, MBs, cs.bit_depth, i);
	}
	// Set cyclic refresh parameters.
	void	cyclic_refresh_update_parameters() {
		cr.update_parameters(svc.number_temporal_layers * 400 / rc.frames_since_key, noise_estimate.enabled && noise_estimate.level >= NOISE_ESTIMATE::Medium);
	}
	void new_framerate(float framerate) {
		framerate = framerate < 0.1 ? 30 : framerate;
		rc.update_framerate(oxcf, framerate, is_altref_enabled(), MBs);
	}

	void	reset_temporal_layer_to_zero();
	void	initialize_rd_consts(TileEncoder *x);
	void	get_one_pass_params();
	void	update_noise_estimate();
	int		calc_iframe_target_size_one_pass_cbr();
	int		calc_pframe_target_size_one_pass_cbr();
	bool	resize_one_pass_cbr();
	void	avg_source_sad();
	void	get_svc_params();
	void	cyclic_refresh_setup();
	void	cyclic_refresh_update_segment(ModeInfo *mi, int mi_row, int mi_col, BLOCK_SIZE bsize, int64 rate, int64 dist, int skip, FrameBuffer *fb) const;
	void	cyclic_refresh_update_sb_postencode(const ModeInfo *const mi, int mi_row, int mi_col, BLOCK_SIZE bsize);
	void	cyclic_refresh_check_golden_update();
	void	set_size_independent_vars();
	void	update_mbgraph_stats();
	void	update_mbgraph_mb_stats(TileEncoder *x, MBGRAPH_MB_STATS *stats, FrameBuffer *buf, int mb_y_offset, FrameBuffer *golden_ref, const MotionVector *prev_golden_ref_mv, FrameBuffer *alt_ref, int mb_row, int mb_col);
	void	update_mbgraph_frame_stats(TileEncoder *x, MBGRAPH_FRAME_STATS *stats, FrameBuffer *buf, FrameBuffer *golden_ref, FrameBuffer *alt_ref);
	void	separate_arf_mbs();
};

} // namespace vp9
