#include "vpx_encode.h"
#include "extra/dct.h"
#include "base/strings.h"
#include "base/algorithm.h"
#include "jobs.h"
#include "graphics.h"
#include "iso/iso.h"

namespace vp9 {

static int						fixed_divide[512];
static int						sad_per_bit16lut_8[QINDEX_RANGE];
static int						sad_per_bit4lut_8[QINDEX_RANGE];
static prob_code::token			mv_joint_encodings[MotionVector::JOINTS];
static prob_code::token			mv_class_encodings[MotionVector::CLASSES];
static prob_code::token			mv_fp_encodings[MotionVector::FP_SIZE];
static prob_code::token			mv_class0_encodings[MotionVector::CLASS0_SIZE];

extern const prob_code::tree_index	coef_con_tree[];
extern const prob_code::tree_index	inter_mode_tree[];
extern const prob_code::tree_index	partition_tree[];
extern const prob_code::tree_index	switchable_interp_tree[];
extern const prob_code::tree_index	intra_mode_tree[];
extern const prob_code::tree_index	segment_tree[];
extern const prob_code::tree_index	mv_joint_tree[];
extern const prob_code::tree_index	mv_class_tree[];
extern const prob_code::tree_index	mv_class0_tree[];
extern const prob_code::tree_index	mv_fp_tree[];

const uint8		num_4x4_blocks_wide_lookup[BLOCK_SIZES] = {1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16};
const uint8		num_4x4_blocks_high_lookup[BLOCK_SIZES] = {1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16};
const uint8		mi_width_log2_lookup[BLOCK_SIZES]		= {0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3};
const uint8		num_8x8_blocks_wide_lookup[BLOCK_SIZES] = {1, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8};
const uint8		num_8x8_blocks_high_lookup[BLOCK_SIZES] = {1, 1, 1, 1, 2, 1, 2, 4, 2, 4, 8, 4, 8};

static void copy_frame(FrameBuffer *dest, const FrameBuffer *srce) {
	ISO_ASSERT(dest->y.width	== srce->y.width);
	ISO_ASSERT(dest->y.height	== srce->y.height);
	copy_buffer(dest->y_buffer, srce->y_buffer, dest->y.width, dest->y.height);
}


//-----------------------------------------------------------------------------
// RATE_CONTROL
//-----------------------------------------------------------------------------

RATE_CONTROL::tables RATE_CONTROL::tables8, RATE_CONTROL::tables10, RATE_CONTROL::tables12;
const float RATE_CONTROL::MIN_BPB_FACTOR		= 0.005f;
const int8	RATE_CONTROL::rate_thresh_mult[]	= { 1, 2 };
const int8	RATE_CONTROL::rcf_mult[]			= { 1, 2 };

static int get_minq_index(double maxq, const int16 *table, int scale, double x3, double x2, double x1) {
	const double minqtarget = min(((x3 * maxq + x2) * maxq + x1) * maxq, maxq);
	// Special case handling to deal with the step from q=2.0 down to lossless mode represented by q=1.0
	if (minqtarget <= 2.0)
		return 0;
	const int16	*i	= lower_bound(table, table + QINDEX_RANGE, int(minqtarget * scale));
	return min(i - table, QINDEX_RANGE - 1);
}
void RATE_CONTROL::tables::init(int bit_depth) {
	const int16 *table	= Quantisation::ac_quant_table(bit_depth);
	int			scale	= Quantisation::q_scale(bit_depth);
	for (int i = 0; i < QINDEX_RANGE; i++) {
		const double maxq = double(table[i]) / scale;
		kf_low[i]		= get_minq_index(maxq, table, scale, 0.00000100, -0.00040, 0.150);
		kf_high[i]		= get_minq_index(maxq, table, scale, 0.00000210, -0.00125, 0.550);
		arfgf_low[i]	= get_minq_index(maxq, table, scale, 0.00000150, -0.00090, 0.300);
		arfgf_high[i]	= get_minq_index(maxq, table, scale, 0.00000210, -0.00125, 0.550);
		inter[i]		= get_minq_index(maxq, table, scale, 0.00000271, -0.00113, 0.900);
		rtc[i]			= get_minq_index(maxq, table, scale, 0.00000271, -0.00113, 0.700);
	}
}

void RATE_CONTROL::init(const EncoderConfig &oxcf, int pass) {
	if (pass == 0 && oxcf.rc_mode == RC_CBR) {
		avg_frame_qindex[KEY_FRAME]		= oxcf.worst_allowed_q;
		avg_frame_qindex[INTER_FRAME]	= oxcf.worst_allowed_q;
	} else {
		avg_frame_qindex[KEY_FRAME]		= (oxcf.worst_allowed_q + oxcf.best_allowed_q) / 2;
		avg_frame_qindex[INTER_FRAME]	= (oxcf.worst_allowed_q + oxcf.best_allowed_q) / 2;
	}

	last_q[KEY_FRAME]			= oxcf.best_allowed_q;
	last_q[INTER_FRAME]			= oxcf.worst_allowed_q;

	buffer_level				= starting_buffer_level;
	bits_off_target				= starting_buffer_level;

	rolling_target_bits			= avg_frame_bandwidth;
	rolling_actual_bits			= avg_frame_bandwidth;
	long_rolling_target_bits	= avg_frame_bandwidth;
	long_rolling_actual_bits	= avg_frame_bandwidth;

	total_actual_bits			= 0;
	total_target_bits			= 0;
	total_target_vs_actual		= 0;

	frames_since_key			= 8;  // Sensible default for first frame.
	this_key_frame_forced		= 0;
	next_key_frame_forced		= 0;
	source_alt_ref_pending		= 0;
	source_alt_ref_active		= false;

	frames_till_gf_update_due	= 0;
	ni_av_qi					= oxcf.worst_allowed_q;
	ni_tot_qi					= 0;
	ni_frames					= 0;

	tot_q						= 0.0;
	avg_q						= Quantisation::qindex_to_q(oxcf.worst_allowed_q, oxcf.cs.bit_depth);

	for (int i = 0; i < RATE_FACTOR_LEVELS; ++i)
		rate_correction_factors[i] = 1;

	min_gf_interval = oxcf.min_gf_interval;
	max_gf_interval = oxcf.max_gf_interval;
	if (min_gf_interval == 0)
		min_gf_interval = get_default_min_gf_interval(oxcf.init_framerate, oxcf.width, oxcf.height);
	if (max_gf_interval == 0)
		max_gf_interval = get_default_max_gf_interval(oxcf.init_framerate, min_gf_interval);
	baseline_gf_interval = (min_gf_interval + max_gf_interval) / 2;
}


int RATE_CONTROL::clamp_pframe_target_size(const EncoderConfig &oxcf, int target, bool refresh_golden_frame) {
	// If there is an active ARF at this location use the minimum bits on this frame even if it is a constructed arf.
	// The active maximum quantizer insures that an appropriate number of bits will be spent if needed for constructed ARFs.
	if (refresh_golden_frame && is_src_frame_alt_ref)
		target = max(target, max(min_frame_bandwidth, avg_frame_bandwidth >> 5));

	if (oxcf.rc_max_inter_bitrate_pct)
		target = min(target, avg_frame_bandwidth * oxcf.rc_max_inter_bitrate_pct / 100);

	return min(target, max_frame_bandwidth);
}

int RATE_CONTROL::clamp_iframe_target_size(const EncoderConfig &oxcf, int target) {
	if (oxcf.rc_max_intra_bitrate_pct)
		target = min(target, avg_frame_bandwidth * oxcf.rc_max_intra_bitrate_pct / 100);
	return min(target, max_frame_bandwidth);
}

// Update the buffer level: leaky bucket model.
void RATE_CONTROL::update_buffer_level(int encoded_frame_size, bool show_frame, bool can_drop) {
	// Non-viewable frames are treated as pure overhead
	bits_off_target -= encoded_frame_size;
	if (show_frame)
		bits_off_target += avg_frame_bandwidth;

	// Clip the buffer level to the maximum specified buffer size
	bits_off_target = min(bits_off_target, maximum_buffer_size);

	// For screen-content mode, and if frame-dropper is off, don't let buffer level go below threshold, given here as -maximum_ buffer_size
	if (!can_drop)
		bits_off_target = max(bits_off_target, -maximum_buffer_size);

	buffer_level = bits_off_target;
}

bool RATE_CONTROL::drop_frame(const EncoderConfig &oxcf) {
	// Always drop if buffer is below 0.
	if (buffer_level < 0)
		return true;

	// If buffer is below drop_mark, for now just drop every other frame (starting with the next frame) until it increases back over drop_mark.
	int drop_mark = (int)(oxcf.drop_frames_water_mark * optimal_buffer_level / 100);
	if (buffer_level > drop_mark && decimation_factor > 0)
		--decimation_factor;
	else if (buffer_level <= drop_mark && decimation_factor == 0)
		decimation_factor = 1;

	if (decimation_factor > 0) {
		if (decimation_count > 0) {
			--decimation_count;
			return true;
		} else {
			decimation_count = decimation_factor;
			return false;
		}
	}
	decimation_count = 0;
	return false;
}

void RATE_CONTROL::set_frame_target(const EncoderConfig &oxcf, int target, int mbs) {
	// Modify frame size target when down-scaling.
	this_frame_target = oxcf.resize_mode == RESIZE_DYNAMIC && frame_size_selector != UNSCALED
		? int(target * rate_thresh_mult[frame_size_selector])
		: target;

	// Target rate per SB64 (including partial SB64s.
	sb64_target_rate = ((int64)this_frame_target * 4 * 4) / mbs;
}

void RATE_CONTROL::update_framerate(const EncoderConfig &oxcf, float framerate, bool clamp_static, int mbs) {
	avg_frame_bandwidth = int(oxcf.target_bandwidth / framerate);
	min_frame_bandwidth = max(int(avg_frame_bandwidth *	oxcf.two_pass_vbrmin_section / 100), FRAME_OVERHEAD_BITS);

	// A maximum bitrate for a frame is defined.
	// The baseline for this aligns with HW implementations that can support decode of 1080P content up to a bitrate of MAX_MB_RATE bits per 16x16 MB (averaged over a frame).
	// However this limit is extended if a very high rate is given on the command line or the the rate cannnot be acheived because of a user specificed max q (e.g. when the user specifies lossless encode.
	max_frame_bandwidth = max(max((mbs * MAX_MB_RATE), MAXRATE_1080P), int(((int64)avg_frame_bandwidth * oxcf.two_pass_vbrmax_section) / 100));
	set_gf_interval_range(oxcf, framerate, clamp_static);
}

void RATE_CONTROL::update_rate_correction_factors(RATE_FACTOR_LEVEL level, int projected_size, int base_qindex) {
	// Work out a size correction factor.
	float	correction_factor = projected_size > FRAME_OVERHEAD_BITS ? float(projected_frame_size) / projected_size : 1;

	// More heavily damped adjustment used if we have been oscillating either side of target.
	float	adjustment_limit = 0.25f + 0.5f * min(1, abs(log10(correction_factor)));

	q_2_frame	= q_1_frame;
	q_1_frame	= base_qindex;
	rc_2_frame	= rc_1_frame;
	rc_1_frame	= correction_factor > 1.1f ? -1 : correction_factor < .9f ? 1 : 0;

	// Turn off oscilation detection in the case of massive overshoot.
	if (rc_1_frame == -1 && rc_2_frame == 1 && correction_factor > 10)
		rc_2_frame = 0;

	float	rate_correction_factor = get_rate_correction_factor(level);
	if (correction_factor > 1.02f) {
		// We are not already at the worst allowable quality
		correction_factor		= 1 + (correction_factor - 1) * adjustment_limit;
		rate_correction_factor	= min(rate_correction_factor * correction_factor, MAX_BPB_FACTOR);

	} else if (correction_factor < .99f) {
		// We are not already at the best allowable quality
		correction_factor		= 1 - (1 - correction_factor) * adjustment_limit;
		rate_correction_factor	= max(rate_correction_factor * correction_factor, MIN_BPB_FACTOR);
	}

	set_rate_correction_factor(level, rate_correction_factor);
}

int RATE_CONTROL::compute_qdelta(double qstart, double qtarget, int bit_depth) const {
	int start_index, target_index;
	// Convert the average q value to an index.
	for (start_index = best_quality; start_index < worst_quality; ++start_index) {
		if (Quantisation::qindex_to_q(start_index, bit_depth) >= qstart)
			break;
	}

	// Convert the q target to an index
	for (target_index = best_quality; target_index < worst_quality; ++target_index) {
		if (Quantisation::qindex_to_q(target_index, bit_depth) >= qtarget)
			break;
	}

	return target_index - start_index;
}

void RATE_CONTROL::set_gf_interval_range(const EncoderConfig &oxcf, float framerate, bool clamp_static) {
	// Special case code for 1 pass fixed Q mode tests
	if (oxcf.pass == 0 && oxcf.rc_mode == RC_Q) {
		max_gf_interval = FIXED_GF_INTERVAL;
		min_gf_interval = FIXED_GF_INTERVAL;
		static_scene_max_gf_interval = FIXED_GF_INTERVAL;
	} else {
		// Set Maximum gf/arf interval
		max_gf_interval = oxcf.max_gf_interval;
		min_gf_interval = oxcf.min_gf_interval;
		if (min_gf_interval == 0)
			min_gf_interval = get_default_min_gf_interval(framerate, oxcf.width, oxcf.height);
		if (max_gf_interval == 0)
			max_gf_interval = get_default_max_gf_interval(framerate, min_gf_interval);

		// Extended interval for genuinely static scenes
		static_scene_max_gf_interval = MAX_LAG_BUFFERS * 2;

		if (clamp_static && static_scene_max_gf_interval > oxcf.lag_in_frames - 1)
			static_scene_max_gf_interval = oxcf.lag_in_frames - 1;

		if (max_gf_interval > static_scene_max_gf_interval)
			max_gf_interval = static_scene_max_gf_interval;

		// Clamp min to max
		min_gf_interval = min(min_gf_interval, max_gf_interval);
	}
}

int RATE_CONTROL::adjust_target_rate(int target, int max_delta, bool fast) {
	// vbr_bits_off_target > 0 means we have extra bits to spend
	if (vbr_bits_off_target > 0)
		target += vbr_bits_off_target > max_delta ? max_delta : (int)vbr_bits_off_target;
	else
		target -= vbr_bits_off_target < -max_delta ? max_delta : (int)-vbr_bits_off_target;

	// Fast redistribution of bits arising from massive local undershoot (dont do it for kf,arf,gf or overlay frames)
	if (fast && !is_src_frame_alt_ref && vbr_bits_off_target_fast) {
		int		one_frame_bits		= max(avg_frame_bandwidth, this_frame_target);
		int		fast_extra_bits		= min(min(vbr_bits_off_target_fast, one_frame_bits), max(one_frame_bits / 8, vbr_bits_off_target_fast / 8));
		target						+= fast_extra_bits;
		vbr_bits_off_target_fast	-= fast_extra_bits;
	}
	return target;
}

int RATE_CONTROL::pick_q_one_pass(RC_MODE rc_mode, int cq_level, FRAME_TYPE frame_type, int worst, int bit_depth, int curr_frame, float size_adj, bool refresh_alt_ref_frame) {
	if (frame_type == INTRA_FRAME) {
		return rc_mode == RC_CBR && curr_frame == 0 ? best_quality
			: rc_mode == RC_Q	? adjust_qindex(get_active_cq_level(rc_mode, cq_level), 0.25f, bit_depth)
			: this_key_frame_forced				? max(adjust_qindex(last_boosted_qindex, 0.75f, bit_depth), best_quality)
			: adjust_qindex(get_kf_active_quality(avg_frame_qindex[KEY_FRAME], bit_depth), size_adj, bit_depth);

	} else if (frame_type == INTER_FRAME && !is_src_frame_alt_ref) {
		int		q = frames_since_key > 1 && avg_frame_qindex[INTER_FRAME] < worst ? avg_frame_qindex[INTER_FRAME] : avg_frame_qindex[KEY_FRAME];
		if (rc_mode == RC_CQ) {
			return get_gf_active_quality(max(q, get_active_cq_level(rc_mode, cq_level)), bit_depth) * 15 / 16;

		} else if (rc_mode == RC_Q) {
			return max(adjust_qindex(get_active_cq_level(rc_mode, cq_level), refresh_alt_ref_frame ? 0.4f : 0.5f, bit_depth), best_quality);

		} else {
			return get_gf_active_quality(q, bit_depth);
		}
	} else {
		if (rc_mode == RC_Q) {
			static const float	delta_rate[FIXED_GF_INTERVAL] = { 0.5f, 1.0f, 0.85f, 1.0f, 0.7f, 1.0f, 0.85f, 1.0f };
			return max(adjust_qindex(get_active_cq_level(rc_mode, cq_level), delta_rate[curr_frame % FIXED_GF_INTERVAL], bit_depth), best_quality);

		} else if (rc_mode == RC_CBR) {
			const int *rtc	= get_tables(bit_depth).rtc;
			return curr_frame > 1
				? (avg_frame_qindex[INTER_FRAME] < worst ? rtc[avg_frame_qindex[INTER_FRAME]] : rtc[worst])
				: (avg_frame_qindex[KEY_FRAME]	 < worst ? rtc[avg_frame_qindex[KEY_FRAME]]	  : rtc[worst]);

		} else {
			const int *inter = get_tables(bit_depth).inter;
			// Use the lower of active_worst_quality and recent/average Q.
			int q = inter[avg_frame_qindex[curr_frame > 1 ? INTER_FRAME : KEY_FRAME]];
			return rc_mode == RC_CQ ? max(q, get_active_cq_level(rc_mode, cq_level)) : q;
		}
	}
}

int RATE_CONTROL::pick_q_two_pass(RC_MODE rc_mode, int cq_level, FRAME_TYPE frame_type, int worst, int bit_depth, float size_adj, bool motion, bool refresh_alt_ref_frame) {
	if (frame_type == INTRA_FRAME) {
		return !this_key_frame_forced	? adjust_qindex(get_kf_active_quality(worst, bit_depth), size_adj, bit_depth)
				: motion				? min(adjust_qindex(min(last_kf_qindex, last_boosted_qindex), 1.25f, bit_depth), worst)
				: max(adjust_qindex(last_boosted_qindex, 0.75f, bit_depth), best_quality);
	}

	if (!is_src_frame_alt_ref && frame_type == INTER_FRAME) {
		int	q = frames_since_key > 1 && avg_frame_qindex[INTER_FRAME] < worst ? avg_frame_qindex[INTER_FRAME] : worst;
		if (rc_mode == RC_CQ)
			return get_gf_active_quality(max(q, get_active_cq_level(rc_mode, cq_level)), bit_depth) * 15 / 16;

		if (rc_mode == RC_Q) {
			if (!refresh_alt_ref_frame)
				return get_active_cq_level(rc_mode, cq_level);

			return get_gf_active_quality(q, bit_depth);
		}

		return get_gf_active_quality(q, bit_depth);
	}
	if (rc_mode == RC_Q)
		return get_active_cq_level(rc_mode, cq_level);
	int q = get_tables(bit_depth).inter[worst];
	return rc_mode == RC_CQ ? max(q, get_active_cq_level(rc_mode, cq_level)) : q;
}

// Test if encoded frame will significantly overshoot the target bitrate, and if so, set the QP, reset/adjust some rate control parameters, and return 1.
float RATE_CONTROL::encodedframe_overshoot(int frame_size, int *q, int qbase_index, int mbs, int bit_depth) {
	int thresh_qp			= worst_quality * 3 / 4;
	int thresh_rate			= avg_frame_bandwidth * 10;
	if (qbase_index < thresh_qp && frame_size > thresh_rate) {
		float		rate_correction_factor	= rate_correction_factors[INTER_NORMAL];
		const int	target_size				= avg_frame_bandwidth;
		// Force a re-encode, and for now use max-QP.
		*q = worst_quality;

		// Adjust avg_frame_qindex, buffer_level, and rate correction factors, as these parameters will affect QP selection for subsequent frames
		// If they have settled down to a very different (low QP) state, then not adjusting them may cause next frame to select low QP and overshoot again
		avg_frame_qindex[INTER_FRAME] = *q;
		buffer_level	= optimal_buffer_level;
		bits_off_target	= optimal_buffer_level;
		// Reset rate under/over-shoot flags
		rc_1_frame		= 0;
		rc_2_frame		= 0;
		// Adjust rate correction factor.
		int			target_bits_per_mb = ((uint64)target_size << BPER_MB_NORMBITS) / mbs;
		// Rate correction factor based on target_bits_per_mb and qp (==max_QP).
		// This comes from the inverse computation of bits_per_mb().
		float	q2 = Quantisation::qindex_to_q(*q, bit_depth);
		int		enumerator = 1800000;  // Factor for inter frame.
		enumerator += (int)(enumerator * q2) >> 12;
		float	new_correction_factor = float(target_bits_per_mb) * q2 / enumerator;
		if (new_correction_factor > rate_correction_factor) {
			rate_correction_factor = min(min(2 * rate_correction_factor, new_correction_factor), MAX_BPB_FACTOR);
			rate_correction_factors[INTER_NORMAL] = rate_correction_factor;
		}

		return rate_correction_factor;
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	vp9_aq_cyclicrefresh.h
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//	RD
//-----------------------------------------------------------------------------

void RD_OPT::set_block_thresholds(Segmentation &seg, Quantisation &quant, int bit_depth) {
	// The baseline rd thresholds for breaking out of the rd loop for certain modes are assumed to be based on 8x8 blocks.
	// This table is used to correct for block size.
	// The factors here are << 2 (2 = x0.5, 32 = x8 etc).
	static const uint8 block_size_factor[BLOCK_SIZES] = { 2, 3, 3, 4, 6, 6, 8, 12, 12, 16, 24, 24, 32};

	for (int segment_id = 0; segment_id < MAX_SEGMENTS; ++segment_id) {
		const int qindex	= seg.get_data(segment_id, Segmentation::FEATURE_ALT_Q, quant.base_index) + quant.y_dc_delta;
		const int q			= thresh_factor(qindex, bit_depth);

		for (int bsize = 0; bsize < BLOCK_SIZES; ++bsize) {
			// Threshold here seems unnecessarily harsh but fine given actual range of values used for cpi->sf.thresh_mult[].
			const int t				= q * block_size_factor[bsize];
			const int thresh_max	= INT_MAX / t;

			if (bsize >= BLOCK_8X8) {
				for (int i = 0; i < MAX_MODES; ++i)
					threshes[segment_id][bsize][i] = thresh_mult[i] < thresh_max ? thresh_mult[i] * t / 4 : INT_MAX;
			} else {
				for (int i = 0; i < MAX_REFS; ++i)
					threshes[segment_id][bsize][i] = thresh_mult_sub8x8[i] < thresh_max ? thresh_mult_sub8x8[i] * t / 4 : INT_MAX;
			}
		}
	}
}

static void model_rd_norm(int xsq_q10, int *r_q10, int *d_q10) {
	// NOTE: The tables below must be of the same size.
	// The functions described below are sampled at the four most significant bits of x^2 + 8 / 256.

	// Normalized rate:
	// This table models the rate for a Laplacian source with given variance when quantized with a uniform quantizer with given stepsize
	// The closed form expression is: Rn(x) = H(sqrt(r)) + sqrt(r)*[1 + H(r)/(1 - r)], where r = exp(-sqrt(2) * x) and x = qpstep / sqrt(variance), and H(x) is the binary entropy function
	static const int rate_tab_q10[] = {
	  65536,  6086,  5574,  5275,  5063,  4899,  4764,  4651,
	   4553,  4389,  4255,  4142,  4044,  3958,  3881,  3811,
	   3748,  3635,  3538,  3453,  3376,  3307,  3244,  3186,
	   3133,  3037,  2952,  2877,  2809,  2747,  2690,  2638,
	   2589,  2501,  2423,  2353,  2290,  2232,  2179,  2130,
	   2084,  2001,  1928,  1862,  1802,  1748,  1698,  1651,
	   1608,  1530,  1460,  1398,  1342,  1290,  1243,  1199,
	   1159,  1086,  1021,   963,   911,   864,   821,   781,
		745,   680,   623,   574,   530,   490,   455,   424,
		395,   345,   304,   269,   239,   213,   190,   171,
		154,   126,   104,    87,    73,    61,    52,    44,
		 38,    28,    21,    16,    12,    10,     8,     6,
		  5,     3,     2,     1,     1,     1,     0,     0,
	};
	// Normalized distortion:
	// This table models the normalized distortion for a Laplacian source with given variance when quantized with a uniform quantizer with given stepsize
	// The closed form expression is: Dn(x) = 1 - 1/sqrt(2) * x / sinh(x/sqrt(2)) where x = qpstep / sqrt(variance)
	// Note the actual distortion is Dn * variance.
	static const int dist_tab_q10[] = {
		 0,     0,     1,     1,     1,     2,     2,     2,
		 3,     3,     4,     5,     5,     6,     7,     7,
		 8,     9,    11,    12,    13,    15,    16,    17,
		18,    21,    24,    26,    29,    31,    34,    36,
		39,    44,    49,    54,    59,    64,    69,    73,
		78,    88,    97,   106,   115,   124,   133,   142,
	   151,   167,   184,   200,   215,   231,   245,   260,
	   274,   301,   327,   351,   375,   397,   418,   439,
	   458,   495,   528,   559,   587,   613,   637,   659,
	   680,   717,   749,   777,   801,   823,   842,   859,
	   874,   899,   919,   936,   949,   960,   969,   977,
	   983,   994,  1001,  1006,  1010,  1013,  1015,  1017,
	  1018,  1020,  1022,  1022,  1023,  1023,  1023,  1024,
	};
	static const int xsq_iq_q10[] = {
		   0,      4,      8,     12,     16,     20,     24,     28,
		  32,     40,     48,     56,     64,     72,     80,     88,
		  96,    112,    128,    144,    160,    176,    192,    208,
		 224,    256,    288,    320,    352,    384,    416,    448,
		 480,    544,    608,    672,    736,    800,    864,    928,
		 992,   1120,   1248,   1376,   1504,   1632,   1760,   1888,
		2016,   2272,   2528,   2784,   3040,   3296,   3552,   3808,
		4064,   4576,   5088,   5600,   6112,   6624,   7136,   7648,
		8160,   9184,  10208,  11232,  12256,  13280,  14304,  15328,
	   16352,  18400,  20448,  22496,  24544,  26592,  28640,  30688,
	   32736,  36832,  40928,  45024,  49120,  53216,  57312,  61408,
	   65504,  73696,  81888,  90080,  98272, 106464, 114656, 122848,
	  131040, 147424, 163808, 180192, 196576, 212960, 229344, 245728,
	};
	const int tmp		= (xsq_q10 >> 2) + 8;
	const int k			= highest_set_index(tmp) - 3;
	const int xq		= (k << 3) + ((tmp >> k) & 0x7);
	const int one_q10	= 1 << 10;
	const int a_q10		= ((xsq_q10 - xsq_iq_q10[xq]) << 10) >> (2 + k);
	const int b_q10		= one_q10 - a_q10;
	*r_q10	= (rate_tab_q10[xq] * b_q10 + rate_tab_q10[xq + 1] * a_q10) >> 10;
	*d_q10	= (dist_tab_q10[xq] * b_q10 + dist_tab_q10[xq + 1] * a_q10) >> 10;
}

void RD_OPT::from_var_lapndz(uint32 var, uint32 n_log2, uint32 qstep, int *rate, int64 *dist) {
	// This function models the rate and distortion for a Laplacian source with given variance when quantized with a uniform quantizer with given stepsize
	// The closed form expressions are in: Hang and Chen, "Source Model for transform video coder and its application - Part I: Fundamental Theory", IEEE Trans. Circ. Sys. for Video Tech., April 1997.
	if (var == 0) {
		*rate = 0;
		*dist = 0;
	} else {
		int d_q10, r_q10;
		static const uint32 MAX_XSQ_Q10 = 245727;
		const uint64 xsq_q10_64 = (((uint64)qstep * qstep << (n_log2 + 10)) + (var >> 1)) / var;
		const int xsq_q10 = (int)min(xsq_q10_64, MAX_XSQ_Q10);
		model_rd_norm(xsq_q10, &r_q10, &d_q10);
		*rate = round_pow2(r_q10 << n_log2, 10 - PROB_COST_SHIFT);
		*dist = (var * (int64)d_q10 + 512) >> 10;
	}
}

//	bool	extra_mv		= (cpi->sf.adaptive_motion_search && block_size < x->max_partition_size)
//	uint8	*src_y_buffer	= x->plane[0].src.buf;
//	int		src_y_stride	= x->plane[0].src.stride
void TileEncoder::mv_pred(uint8 *src_y_buffer, int src_y_stride, uint8 *ref_y_buffer, int ref_y_stride, int ref_frame, BLOCK_SIZE block_size, bool extra_mv) {
	bool	zero_seen	= false;
	int		best_index	= 0;
	int		best_sad	= INT_MAX;
	int		max_mv		= 0;
	bool	near_same_nearest	= mbmi_ext->ref_mvs[ref_frame][0] == mbmi_ext->ref_mvs[ref_frame][1];
	const int num_mv_refs		= MotionVectorRef::MAX_CANDIDATES + int(extra_mv);

	MotionVector	pred_mv[3];
	pred_mv[0] = mbmi_ext->ref_mvs[ref_frame][0];
	pred_mv[1] = mbmi_ext->ref_mvs[ref_frame][1];
	pred_mv[2] = pred_mv[ref_frame];

	// Get the sad for each candidate reference mv.
	for (int i = 0; i < num_mv_refs; ++i) {
		if (i == 1 && near_same_nearest)
			continue;

		const MotionVector &mv = pred_mv[i];
		int	fp_row	= (mv.row + 3 + (mv.row >= 0)) >> 3;
		int	fp_col	= (mv.col + 3 + (mv.col >= 0)) >> 3;
		max_mv		= max(max_mv, max(abs(mv.row), abs(mv.col)) >> 3);

		if (fp_row || fp_col || !zero_seen) {
			zero_seen |= fp_row == 0 && fp_col == 0;
			// Find sad for current vector.
			int	sad = variance_table[block_size].sdf(src_y_buffer, src_y_stride, &ref_y_buffer[ref_y_stride * fp_row + fp_col], ref_y_stride);
			// Note if it is the best so far.
			if (sad < best_sad) {
				best_sad	= sad;
				best_index	= i;
			}
		}
	}

	// Note the index of the mv that worked best in the reference list.
	mv_best_ref_index[ref_frame]	= best_index;
	max_mv_context[ref_frame]		= max_mv;
	pred_mv_sad[ref_frame]			= best_sad;
}

int raster_block_offset(BLOCK_SIZE plane_bsize, int raster_block, int stride) {
	const int bw = b_width_log2_lookup[plane_bsize];
	return 4 * (raster_block >> bw) * stride + 4 * (raster_block & ((1 << bw) - 1));
}

int16* raster_block_offset_int16(BLOCK_SIZE plane_bsize, int raster_block, int16 *base) {
	const int stride = 4 * num_4x4_blocks_wide_lookup[plane_bsize];
	return base + raster_block_offset(plane_bsize, raster_block, stride);
}

void RD_OPT::set_speed_thresholds(bool best, bool adaptive) {
	// Set baseline threshold values.
	for (int i = 0; i < MAX_MODES; ++i)
		thresh_mult[i] = best ? -500 : 0;

	if (adaptive) {
		thresh_mult[THR_NEARESTMV] = 300;
		thresh_mult[THR_NEARESTG] = 300;
		thresh_mult[THR_NEARESTA] = 300;
	} else {
		thresh_mult[THR_NEARESTMV] = 0;
		thresh_mult[THR_NEARESTG] = 0;
		thresh_mult[THR_NEARESTA] = 0;
	}

	thresh_mult[THR_DC] += 1000;

	thresh_mult[THR_NEWMV] += 1000;
	thresh_mult[THR_NEWA] += 1000;
	thresh_mult[THR_NEWG] += 1000;

	thresh_mult[THR_NEARMV] += 1000;
	thresh_mult[THR_NEARA] += 1000;
	thresh_mult[THR_COMP_NEARESTLA] += 1000;
	thresh_mult[THR_COMP_NEARESTGA] += 1000;

	thresh_mult[THR_TM] += 1000;

	thresh_mult[THR_COMP_NEARLA] += 1500;
	thresh_mult[THR_COMP_NEWLA] += 2000;
	thresh_mult[THR_NEARG] += 1000;
	thresh_mult[THR_COMP_NEARGA] += 1500;
	thresh_mult[THR_COMP_NEWGA] += 2000;

	thresh_mult[THR_ZEROMV] += 2000;
	thresh_mult[THR_ZEROG] += 2000;
	thresh_mult[THR_ZEROA] += 2000;
	thresh_mult[THR_COMP_ZEROLA] += 2500;
	thresh_mult[THR_COMP_ZEROGA] += 2500;

	thresh_mult[THR_H_PRED] += 2000;
	thresh_mult[THR_V_PRED] += 2000;
	thresh_mult[THR_D45_PRED] += 2500;
	thresh_mult[THR_D135_PRED] += 2500;
	thresh_mult[THR_D117_PRED] += 2500;
	thresh_mult[THR_D153_PRED] += 2500;
	thresh_mult[THR_D207_PRED] += 2500;
	thresh_mult[THR_D63_PRED] += 2500;

	static const int thresh_mult[2][MAX_REFS] =	{ {2500, 2500, 2500, 4500, 4500, 2500}, {2000, 2000, 2000, 4000, 4000, 2000} };
	memcpy(thresh_mult_sub8x8, thresh_mult[best], sizeof(thresh_mult[best]));
}

void RD_OPT::update_rd_thresh_fact(int(*factor_buf)[MAX_MODES], int rd_thresh, int bsize, int best_mode_index) {
	if (rd_thresh > 0) {
		const int top_mode = bsize < BLOCK_8X8 ? MAX_REFS : MAX_MODES;
		for (int mode = 0; mode < top_mode; ++mode) {
			const BLOCK_SIZE min_size = (BLOCK_SIZE)max(bsize - 1, BLOCK_4X4);
			const BLOCK_SIZE max_size = (BLOCK_SIZE)min(bsize + 2, BLOCK_64X64);
			for (int bs = min_size; bs <= max_size; ++bs) {
				int &fact	= factor_buf[bs][mode];
				fact		= mode == best_mode_index
					? fact - (fact >> 4)
					: min(fact + RD_THRESH_INC, rd_thresh * RD_THRESH_MAX_FACT);
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	RATE CONTROL
//-----------------------------------------------------------------------------

int Encoder::calc_iframe_target_size_one_pass_cbr() {
	int	target = rc.calc_iframe_target_size_temporal(
		curr_frame == 0 ? 0
		:	svc.number_temporal_layers > 1 ? svc.layer_context[svc.index(svc.temporal_layer_id)].framerate	// Use the layer framerate for temporal layers CBR mode.
		:	framerate
	);
	return rc.clamp_iframe_target_size(oxcf, target);
}

int Encoder::calc_pframe_target_size_one_pass_cbr() {
	int min_frame_target;
	int target;

	if (is_one_pass_cbr_svc()) {
		const LAYER_CONTEXT &lc = svc.layer_context[svc.index(svc.temporal_layer_id)];
		target				= lc.avg_frame_size;
		min_frame_target	= max(lc.avg_frame_size >> 4, RATE_CONTROL::FRAME_OVERHEAD_BITS);
	} else {
		target				= rc.boost_golden_frame(oxcf.gf_cbr_boost_pct, refresh_golden_frame);
		min_frame_target	= max(rc.avg_frame_bandwidth >> 4, RATE_CONTROL::FRAME_OVERHEAD_BITS);
	}

	target = rc.adjust_target_size(oxcf, target);
	return max(min_frame_target, target);
}

void Encoder::get_one_pass_params() {
	if (!refresh_alt_ref_frame && curr_frame == 0 || (frame_flags & FRAMEFLAGS_KEY) || rc.frames_to_key == 0) {
		frame_type	= KEY_FRAME;
		rc.set_keyframe(oxcf.key_freq, curr_frame != 0 && rc.frames_to_key == 0);
	} else {
		frame_type = INTER_FRAME;
	}
	refresh_golden_frame = rc.update_golden_frame_stats(oxcf.aq_mode == AQ_CYCLIC_REFRESH ? cr.percent_refresh : 0);

	// Any update/change of global cyclic refresh parameters (amount/delta-qp) should be done here, before the frame qp is selected.
	if (oxcf.rc_mode == RC_CBR && oxcf.aq_mode == AQ_CYCLIC_REFRESH)
		cyclic_refresh_update_parameters();
	
	int	target;
	if (frame_type == KEY_FRAME) {
		if (oxcf.rc_mode == RC_CBR) {
			target = calc_iframe_target_size_one_pass_cbr();
		} else {
			static const int kf_ratio = 25;
			target = rc.clamp_iframe_target_size(oxcf, rc.avg_frame_bandwidth * kf_ratio);
		}
	} else {
		if (oxcf.rc_mode == RC_CBR) {
			target = calc_pframe_target_size_one_pass_cbr();
		} else {
		#if USE_ALTREF_FOR_ONE_PASS
			static const int af_pct = 1100;
		#else
			static const int af_pct = 0;
		#endif
			target = rc.boost_golden_frame(af_pct, refresh_golden_frame);
			target = rc.clamp_pframe_target_size(oxcf, target, refresh_golden_frame);
		}
	}

	rc.set_frame_target(oxcf, target, MBs);

	resize_pending = oxcf.resize_mode == RESIZE_DYNAMIC && oxcf.rc_mode == RC_CBR && resize_one_pass_cbr();
}
void Encoder::get_svc_params() {
	int target	= rc.avg_frame_bandwidth;
	int layer	= svc.index(svc.temporal_layer_id);

	if (curr_frame == 0 || (frame_flags & FRAMEFLAGS_KEY) || (oxcf.auto_key && (rc.frames_since_key % oxcf.key_freq == 0))) {
		frame_type					= KEY_FRAME;
		rc.source_alt_ref_active	= false;

		if (is_two_pass_svc()) {
			svc.layer_context[layer].is_key_frame = true;
			ref_frame_flags &= ~((1 << REFFRAME_LAST) | (1 << REFFRAME_GOLDEN) | (1 << REFFRAME_ALTREF));

		} else if (is_one_pass_cbr_svc()) {
			reset_temporal_layer_to_zero();
			layer	= svc.index(svc.temporal_layer_id);
			svc.layer_context[layer].is_key_frame = true;
			ref_frame_flags &= ~((1 << REFFRAME_LAST) | (1 << REFFRAME_GOLDEN) | (1 << REFFRAME_ALTREF));
			// Assumption here is that LAST_FRAME is being updated for a keyframe, thus no change in update flags.
			target	= calc_iframe_target_size_one_pass_cbr();
		}
	} else {
		frame_type = INTER_FRAME;
		if (is_two_pass_svc()) {
			if (svc.layer_context[layer].is_key_frame = svc.spatial_layer_id != 0 && svc.layer_context[svc.temporal_layer_id].is_key_frame)
				ref_frame_flags &= ~(1 << REFFRAME_LAST);
			ref_frame_flags &= ~(1 << REFFRAME_ALTREF);

		} else if (is_one_pass_cbr_svc()) {
			svc.layer_context[layer].is_key_frame = svc.spatial_layer_id != svc.first_spatial_layer_to_encode && svc.layer_context[svc.temporal_layer_id].is_key_frame;
			target = calc_pframe_target_size_one_pass_cbr();
		}
	}

	// Any update/change of global cyclic refresh parameters (amount/delta-qp)
	// should be done here, before the frame qp is selected.
	if (oxcf.aq_mode == AQ_CYCLIC_REFRESH)
		cyclic_refresh_update_parameters();

	rc.set_frame_target(oxcf, target, MBs);
	rc.frames_till_gf_update_due	= INT_MAX;
	rc.baseline_gf_interval			= INT_MAX;
}

// Compute average source sad (temporal sad: between current source and previous source) over a subset of superblocks. Use this is detect big changes in content and allow rate control to react.
// TODO(marpan): Superblock sad is computed again in variance partition for non-rd mode (but based on last reconstructed frame). Should try to reuse these computations.
void Encoder::avg_source_sad() {
	rc.high_source_sad = false;
	if (Last_Source) {
		const uint8			*src_y				= Source->y_buffer.buffer;
		const int			src_ystride			= Source->y_buffer.stride;
		const uint8			*last_src_y			= Last_Source->y_buffer.buffer;
		const int			last_src_ystride	= Last_Source->y_buffer.stride;
		const BLOCK_SIZE	bsize				= BLOCK_64X64;

		// Loop over sub-sample of frame, and compute average sad over 64x64 blocks.
		uint64	avg_sad			= 0;
		int		num_samples		= 0;
		int		sb_cols			= (mi_cols + MI_BLOCK_SIZE - 1) / MI_BLOCK_SIZE;
		int		sb_rows			= (mi_rows + MI_BLOCK_SIZE - 1) / MI_BLOCK_SIZE;
		// ignore boundary.
		for (int sbi_row = 1; sbi_row < sb_rows - 1; sbi_row++) {
			for (int sbi_col = 1; sbi_col < sb_cols - 1; sbi_col++) {
				// Checker-board pattern
				if (((sbi_row + sbi_col) & 1) == 0) {
					num_samples++;
					avg_sad += variance_table[bsize].sdf(src_y + sbi_col * 64, src_ystride, last_src_y + sbi_col * 64, last_src_ystride);
				}
			}
			src_y		+= src_ystride * 64;
			last_src_y	+= last_src_ystride * 64;
		}
		if (num_samples > 0)
			avg_sad = avg_sad / num_samples;
		// Set high_source_sad flag if we detect very high increase in avg_sad between current and the previous frame value(s). Use a minimum threshold for cases where there is small change from content that is completely static
		rc.high_source_sad	= avg_sad > max(4000, rc.avg_source_sad << 3) && rc.frames_since_key > 1;
		rc.avg_source_sad	= (rc.avg_source_sad + avg_sad) >> 1;
	}
}

// Check if we should resize, based on average QP from past x frames.
// Only allow for resize at most one scale down for now, scaling factor is 2.
bool Encoder::resize_one_pass_cbr() {
	RESIZE_ACTION	resize_action	= NO_RESIZE;
	int				avg_qp_thr1		= 70;
	int				avg_qp_thr2		= 50;
	int				min_width		= 180;
	int				min_height		= 180;

	resize_scale_num	= 1;
	resize_scale_den	= 1;

	// Don't resize on key frame; reset the counters on key frame.
	if (frame_type == KEY_FRAME) {
		resize_avg_qp	= 0;
		resize_count	= 0;
		return false;
	}

	// Check current frame reslution to avoid generating frames smaller than the minimum resolution.
	if (resize_state == RESIZE_ORIG && (width * 3 / 4 < min_width || height * 3 / 4 < min_height))
		return false;

	bool	down_size_on	= !(resize_state == RESIZE_THREE_QUARTER && ((oxcf.width >> 1) < min_width || (oxcf.height >> 1) < min_height));

#if CONFIG_VP9_TEMPORAL_DENOISING
	// If denoiser is on, apply a smaller qp threshold.
	if (oxcf.noise_sensitivity > 0) {
		avg_qp_thr1 = 60;
		avg_qp_thr2 = 40;
	}
#endif

	// Resize based on average buffer underflow and QP over some window.
	// Ignore samples close to key frame, since QP is usually high after key.
	if (rc.frames_since_key > 2 * framerate) {
		const int window = int(4 * framerate);
		resize_avg_qp	+= quant.base_index;
		if (rc.buffer_level < int(30 * rc.optimal_buffer_level / 100))
			++resize_buffer_underflow;
		++resize_count;
		// Check for resize action every "window" frames.
		if (resize_count >= window) {
			int avg_qp = resize_avg_qp / resize_count;
			// Resize down if buffer level has underflowed sufficient amount in past window, and we are at original or 3/4 of original resolution.
			// Resize back up if average QP is low, and we are currently in a resized down state, i.e. 1/2 or 3/4 of original resolution.
			// Currently, use a flag to turn 3/4 resizing feature on/off.
			if (resize_buffer_underflow > resize_count >> 2) {
				if (resize_state == RESIZE_THREE_QUARTER && down_size_on) {
					resize_action	= DOWN_ONEHALF;
					resize_state	= RESIZE_ONE_HALF;
				} else if (resize_state == RESIZE_ORIG) {
					resize_action	= DOWN_THREEFOUR;
					resize_state	= RESIZE_THREE_QUARTER;
				}
			} else if (resize_state != RESIZE_ORIG && avg_qp < avg_qp_thr1 * rc.worst_quality / 100) {
				if (resize_state == RESIZE_THREE_QUARTER || avg_qp < avg_qp_thr2 * rc.worst_quality / 100) {
					resize_action	= UP_ORIG;
					resize_state	= RESIZE_ORIG;
				} else if (resize_state == RESIZE_ONE_HALF) {
					resize_action	= UP_THREEFOUR;
					resize_state	= RESIZE_THREE_QUARTER;
				}
			}
			// Reset for next window measurement.
			resize_avg_qp	= 0;
			resize_count	= 0;
			resize_buffer_underflow = 0;
		}
	}
	// If decision is to resize, reset some quantities, and check is we should reduce rate correction factor,
	if (resize_action) {
		if (resize_action == DOWN_THREEFOUR || resize_action == UP_THREEFOUR) {
			resize_scale_num = 3;
			resize_scale_den = 4;
		} else if (resize_action == DOWN_ONEHALF) {
			resize_scale_num = 1;
			resize_scale_den = 2;
		} else {  // UP_ORIG or anything else
			resize_scale_num = 1;
			resize_scale_den = 1;
		}
		int	tot_scale_change	= (resize_scale_den * resize_scale_den) / (resize_scale_num * resize_scale_num);

		// Reset buffer level to optimal, update target size.
		rc.buffer_level			= rc.optimal_buffer_level;
		rc.bits_off_target		= rc.optimal_buffer_level;
		rc.this_frame_target	= calc_pframe_target_size_one_pass_cbr();
		// Get the projected qindex, based on the scaled target frame size (scaled so target_bits_per_mb in regulate_q will be correct target).
		int target_bits_per_frame	= resize_action >= 0 ? rc.this_frame_target * tot_scale_change : rc.this_frame_target / tot_scale_change;
		int active_worst_quality	= frame_type == KEY_FRAME ? rc.worst_quality : rc.calc_active_worst_quality_one_pass_cbr(curr_frame < 5 * svc.number_temporal_layers ? KEY_FRAME : INTER_FRAME);
		int qindex					= regulate_q(target_bits_per_frame, rc.best_quality, active_worst_quality);

		// If resize is down, check if projected q index is close to worst_quality, and if so, reduce the rate correction factor (since likely can afford lower q for resized frame).
		if (resize_action > 0 && qindex > 90 * rc.worst_quality / 100)
			rc.rate_correction_factors[INTER_NORMAL] *= 0.85f;

		// If resize is back up, check if projected q index is too much above the current quant.base_index, and if so, reduce the rate correction factor (since prefer to keep q for resized frame at least close to previous q).
		if (resize_action < 0 && qindex > 130 * quant.base_index / 100)
			rc.rate_correction_factors[INTER_NORMAL] *= 0.9f;
	}
	return resize_action;
}

// Reset information needed to set proper reference frames and buffer updates for temporal layering. This is called when a key frame is encoded.
void Encoder::reset_temporal_layer_to_zero() {
	svc.temporal_layer_id = 0;
	for (int sl = 0; sl < svc.number_spatial_layers; ++sl) {
		LAYER_CONTEXT &lc = svc.layer_context[sl * svc.number_temporal_layers];
		lc.current_video_frame_in_layer = 0;
		lc.frames_from_key_frame		= 0;
	}
}
void Encoder::update_rate_correction_factors() {
	// Do not update the rate factors for arf overlay frames.
	if (rc.is_src_frame_alt_ref)
		return;

	RATE_FACTOR_LEVEL	level	= get_rate_factor_level();
	float				factor	= rc.get_rate_correction_factor(level);

	// Work out how big we would have expected the frame to be at this Q given the current correction factor.
	// Stay in double to avoid int overflow when values are large
	int projected_size_based_on_q = oxcf.aq_mode == AQ_CYCLIC_REFRESH && seg.enabled
		? cyclic_refresh_estimate_bits_at_q(factor)
		: rc.estimate_bits_at_q(frame_type, quant.base_index, MBs, factor, cs.bit_depth);

	// Work out a size correction factor.
	rc.update_rate_correction_factors(level, projected_size_based_on_q, quant.base_index);
}

int Encoder::regulate_q(int target_bits_per_frame, int active_best_quality, int active_worst_quality) {
	const float correction_factor = get_rate_correction_factor();

	// Calculate required scaling factor based on target frame size and size of frame produced using previous Q.
	int target_bits_per_mb = ((uint64)target_bits_per_frame << RATE_CONTROL::BPER_MB_NORMBITS) / MBs;

	int q			= active_worst_quality;
	int last_error	= INT_MAX;
	for (int i = active_best_quality; i <= active_worst_quality; ++i) {
		int bits_per_mb_at_this_q = oxcf.aq_mode == AQ_CYCLIC_REFRESH && seg.enabled && svc.temporal_layer_id == 0
			? (int)cyclic_refresh_rc_bits_per_mb(i, correction_factor)
			: (int)rc.bits_per_mb(frame_type, i, correction_factor, cs.bit_depth);

		if (bits_per_mb_at_this_q <= target_bits_per_mb) {
			q = target_bits_per_mb - bits_per_mb_at_this_q <= last_error ? i : i - 1;
			break;
		}
		last_error = bits_per_mb_at_this_q - target_bits_per_mb;
	}

	// In CBR mode, this makes sure q is between oscillating Qs to prevent resonance.
	if (oxcf.rc_mode == RC_CBR && rc.rc_1_frame * rc.rc_2_frame == -1 && rc.q_1_frame != rc.q_2_frame)
		q = clamp(q, min(rc.q_1_frame, rc.q_2_frame), max(rc.q_1_frame, rc.q_2_frame));
	return q;
}

void RATE_CONTROL::postencode_update(uint64 bytes_used, RATE_FACTOR_LEVEL level, int qindex, FRAME_TYPE frame_type, int bit_depth, bool show_frame, bool can_drop) {
	// Update rate control heuristics
	projected_frame_size = int(bytes_used << 3);

	// Post encode loop adjustment of Q prediction.
	update_rate_correction_factors(level, projected_frame_size, qindex);

	// Keep a record of last Q and ambient average Q.
	if (frame_type == KEY_FRAME) {
		last_q[KEY_FRAME] = qindex;
		avg_frame_qindex[KEY_FRAME] = round_pow2(3 * avg_frame_qindex[KEY_FRAME] + qindex, 2);
		last_boosted_qindex = qindex;

	} else if (frame_type == INTER_FRAME && !is_src_frame_alt_ref) {
		last_q[INTER_FRAME] = qindex;
		avg_frame_qindex[INTER_FRAME] =	round_pow2(3 * avg_frame_qindex[INTER_FRAME] + qindex, 2);
		ni_frames++;
		tot_q		+= Quantisation::qindex_to_q(qindex, bit_depth);
		avg_q		= tot_q / ni_frames;
		// Calculate the average Q for normal inter frames (not key or GFU frames).
		ni_tot_qi	+= qindex;
		ni_av_qi	= ni_tot_qi / ni_frames;
		if (!constrained_gf_group)
			last_boosted_qindex = qindex;
	}

	// If the current frame is coded at a lower Q then we also update it.
	if (qindex < last_boosted_qindex)
		last_boosted_qindex = qindex;

	update_buffer_level(projected_frame_size, show_frame, can_drop);

	if (frame_type == KEY_FRAME) {
		last_kf_qindex				= qindex;
		frames_since_key			= 0;
	} else {
		// Rolling monitors of whether we are over or underspending used to help regulate min and Max Q in two pass.
		rolling_target_bits			= round_pow2(rolling_target_bits * 3 + this_frame_target, 2);
		rolling_actual_bits			= round_pow2(rolling_actual_bits * 3 + projected_frame_size, 2);
		long_rolling_target_bits	= round_pow2(long_rolling_target_bits * 31 + this_frame_target, 5);
		long_rolling_actual_bits	= round_pow2(long_rolling_actual_bits * 31 + projected_frame_size, 5);
	}

	// Actual bits spent
	total_actual_bits		+= projected_frame_size;
	total_target_bits		+= show_frame ? avg_frame_bandwidth : 0;
	total_target_vs_actual	= total_actual_bits - total_target_bits;

	if (show_frame) {
		frames_since_key++;
		frames_to_key--;
	}
}

void Encoder::postencode_update(uint64 bytes_used) {
	if (oxcf.aq_mode == AQ_CYCLIC_REFRESH && seg.enabled)
		cr.postencode(mi_rows, mi_cols, seg_map[1]);

	rc.postencode_update(bytes_used
		, get_rate_factor_level()
		, quant.base_index
		, use_svc && oxcf.rc_mode == RC_CBR || !(refresh_golden_frame || refresh_alt_ref_frame) ? frame_type : ALTREF_FRAME
		, cs.bit_depth, show_frame
		, oxcf.drop_frames_water_mark || oxcf.content != CONTENT_SCREEN
	);

	if (frame_type == KEY_FRAME && use_svc) {
		for (int i = 0; i < svc.number_temporal_layers; ++i) {
			RATE_CONTROL	&lrc			= svc.layer_context[svc.index(i)].rc;
			lrc.last_q[KEY_FRAME]			= rc.last_q[KEY_FRAME];
			lrc.avg_frame_qindex[KEY_FRAME] = rc.avg_frame_qindex[KEY_FRAME];
		}
	}
	if (is_one_pass_cbr_svc())
		svc.update_layer_buffer_level(rc.projected_frame_size);

	if (!use_svc || is_two_pass_svc()) {
		if (is_altref_enabled() && refresh_alt_ref_frame && frame_type != KEY_FRAME) {
			// Update the alternate reference frame stats as appropriate.
			rc.update_alt_ref_frame_stats();
		} else {
			// Update the Golden frame stats as appropriate.
			rc.update_golden_frame_stats(refresh_golden_frame, refresh_alt_ref_frame, oxcf.pass == 2 && twopass.gf_group.index == 0);
		}
	}

	// Trigger the resizing of the next frame if it is scaled.
	if (oxcf.pass != 0) {
		resize_pending			= rc.next_frame_size_selector != rc.frame_size_selector;
		rc.frame_size_selector	= rc.next_frame_size_selector;
	}
}

void Encoder::compute_frame_size_bounds(int frame_target, int *frame_under_shoot_limit, int *frame_over_shoot_limit) {
	if (oxcf.rc_mode == RC_Q) {
		*frame_under_shoot_limit	= 0;
		*frame_over_shoot_limit		= INT_MAX;
	} else {
		// For very small rate targets where the fractional adjustment may be tiny make sure there is at least a minimum range.
		const int tolerance			= (sf.recode_tolerance * frame_target) / 100;
		*frame_under_shoot_limit	= max(frame_target - tolerance - 200, 0);
		*frame_over_shoot_limit		= min(frame_target + tolerance + 200, rc.max_frame_bandwidth);
	}
}

//-----------------------------------------------------------------------------
//	CYCLIC REFRESH
//-----------------------------------------------------------------------------

// Setup cyclic background refresh: set delta q and segmentation map.
void Encoder::cyclic_refresh_setup() {
	// TODO(marpan): Look into whether we should reduce the amount/delta-qp instead of completely shutting off at low bitrates. For now keep it on.
	// const int apply_cyclic_refresh = apply_cyclic_refresh_bitrate(cm, rc);
	const int apply_cyclic_refresh = 1;
	if (curr_frame == 0)
		cr.low_content_avg = 0;

	// Don't apply refresh on key frame or temporal enhancement layer frames.
	if (!apply_cyclic_refresh || frame_type == KEY_FRAME || svc.temporal_layer_id > 0) {
		// Set segmentation map to 0 and disable.
		memset(seg_map[1], 0, mi_rows * mi_cols);
		seg.disable();
		if (frame_type == KEY_FRAME) {
			memset(cr.last_coded_q_map, MAXQ, mi_rows * mi_cols);
			memset(cr.consec_zero_mv, 0, mi_rows * mi_cols);
			cr.sb_index = 0;
		}
		return;
	}
	const float q = Quantisation::qindex_to_q(quant.base_index, cs.bit_depth);
	// Set rate threshold to some multiple (set to 2 for now) of the target rate (target is given by sb64_target_rate and scaled by 256).
	cr.thresh_rate_sb = ((int64)(rc.sb64_target_rate) << 8) << 2;
	// Distortion threshold, quadratic in Q, scale factor to be adjusted. q will not exceed 457, so (q * q) is within 32bit; see: vp9_convert_qindex_to_q(), vp9_ac_quant(), ac_qlookup*[].
	cr.thresh_dist_sb = ((int64)(q * q)) << 2;

	// Set up segmentation.
	// Clear down the segment map.
	seg.enable();
	seg.clearall();
	// Select delta coding method.
	seg.abs_delta = false;

	// Note: setting temporal_update has no effect, as the seg-map coding method (temporal or spatial) is determined in vp9_choose_segmap_coding_method(), based on the coding cost of each method.
	// For error_resilient mode on the last_frame_seg_map is set to 0, so if temporal coding is used, it is relative to 0 previous map.
	// seg->temporal_update = 0;
	seg.disable(CYCLIC_REFRESH::SEGMENT_ID_BASE, Segmentation::FEATURE_ALT_Q);	// Segment BASE "Q" feature is disabled so it defaults to the baseline Q.
	seg.enable(CYCLIC_REFRESH::SEGMENT_ID_BOOST1, Segmentation::FEATURE_ALT_Q);	// Use segment BOOST1 for in-frame Q adjustment.
	seg.enable(CYCLIC_REFRESH::SEGMENT_ID_BOOST2, Segmentation::FEATURE_ALT_Q);	// Use segment BOOST2 for more aggressive in-frame Q adjustment.

	// Set the q delta for segment BOOST1.
	int qindex_delta	= compute_deltaq(quant.base_index, cr.rate_ratio_qdelta);
	cr.qindex_delta[1]	= qindex_delta;

	// Compute rd-mult for segment BOOST1.
	int qindex2 = clamp(quant.base_index + quant.y_dc_delta + qindex_delta, 0, MAXQ);

	cr.rdmult = compute_rd_mult(qindex2);

	seg.set_data(CYCLIC_REFRESH::SEGMENT_ID_BOOST1, Segmentation::FEATURE_ALT_Q, qindex_delta);

	// Set a more aggressive (higher) q delta for segment BOOST2.
	qindex_delta = compute_deltaq(quant.base_index, min(CYCLIC_REFRESH::MAX_RATE_TARGET_RATIO, 0.1 * cr.rate_boost_fac * cr.rate_ratio_qdelta));
	cr.qindex_delta[2] = qindex_delta;
	seg.set_data(CYCLIC_REFRESH::SEGMENT_ID_BOOST2, Segmentation::FEATURE_ALT_Q, qindex_delta);

	// Reset if resoluton change has occurred.
	if (resize_pending) {
		cr.reset_resize(mi_rows * mi_cols);
		refresh_golden_frame = true;
	}

	// Update the segmentation map, and related quantities: cyclic refresh map, refresh sb_index, and target number of blocks to be refreshed.
	// The map is set to either 0/CR_SEGMENT_ID_BASE (no refresh) or to 1/CR_SEGMENT_ID_BOOST1 (refresh) for each superblock.
	// Blocks labeled as BOOST1 may later get set to BOOST2 (during the encoding of the superblock).

	memset(seg_map[1], CYCLIC_REFRESH::SEGMENT_ID_BASE, mi_rows * mi_cols);
	int		count_sel		= 0;
	int		count_tot		= 0;
	int		sb_cols			= (mi_cols + MI_BLOCK_SIZE - 1) / MI_BLOCK_SIZE;
	int		sb_rows			= (mi_rows + MI_BLOCK_SIZE - 1) / MI_BLOCK_SIZE;
	int		sbs_in_frame	= sb_cols * sb_rows;
	// Number of target blocks to get the q delta (segment 1).
	int	block_count			= cr.percent_refresh * mi_rows * mi_cols / 100;
	// Set the segmentation map: cycle through the superblocks, starting at cr.mb_index, and stopping when either block_count blocks have been found to be refreshed, or we have passed through whole frame.
	cr.target_num_seg_blocks = 0;

	int consec_zero_mv_thresh	= oxcf.content == CONTENT_SCREEN ? 0 : noise_estimate.enabled && noise_estimate.level >= NOISE_ESTIMATE::Medium ? 80 : 100;
	int	qindex_thresh			= seg.get_data(oxcf.content == CONTENT_SCREEN ? CYCLIC_REFRESH::SEGMENT_ID_BOOST2 : CYCLIC_REFRESH::SEGMENT_ID_BOOST1, Segmentation::FEATURE_ALT_Q, quant.base_index);

	int	i = cr.sb_index;
	do {
		int		sum_map			= 0;
		// Get the mi_row/mi_col corresponding to superblock index i.
		int		sb_row_index	= i / sb_cols;
		int		sb_col_index	= i - sb_row_index * sb_cols;
		int		mi_row			= sb_row_index * MI_BLOCK_SIZE;
		int		mi_col			= sb_col_index * MI_BLOCK_SIZE;
		int		bl_index		= mi_row * mi_cols + mi_col;
		// Loop through all 8x8 blocks in superblock and update map.
		int		xmis			= min(mi_cols - mi_col, 8);
		int		ymis			= min(mi_rows - mi_row, 8);
		for (int y = 0; y < ymis; y++) {
			for (int x = 0; x < xmis; x++) {
				const int bl_index2 = bl_index + y * mi_cols + x;
				// If the block is as a candidate for clean up then mark it for possible boost/refresh (segment 1). The segment id may get reset to 0 later if block gets coded anything other than ZEROMV.
				if (cr.map[bl_index2] == 0) {
					count_tot++;
					if (cr.last_coded_q_map[bl_index2] > qindex_thresh || cr.consec_zero_mv[bl_index2] < consec_zero_mv_thresh) {
						sum_map++;
						count_sel++;
					}
				} else if (cr.map[bl_index2] < 0) {
					cr.map[bl_index2]++;
				}
			}
		}
		// Enforce constant segment over superblock. If segment is at least half of superblock, set to 1.
		if (sum_map >= xmis * ymis / 2) {
			for (int y = 0; y < ymis; y++) {
				for (int x = 0; x < xmis; x++)
					seg_map[bl_index + y * mi_cols + x] = CYCLIC_REFRESH::SEGMENT_ID_BOOST1;
			}
			cr.target_num_seg_blocks += xmis * ymis;
		}
		if (++i == sbs_in_frame)
			i = 0;
	} while (cr.target_num_seg_blocks < block_count && i != cr.sb_index);

	cr.sb_index			= i;
	cr.reduce_refresh	= count_sel < (3 * count_tot) >> 2;
}

// Prior to coding a given prediction block, of size bsize at (mi_row, mi_col), check if we should reset the segment_id, and update the cr map and segmentation map.
void Encoder::cyclic_refresh_update_segment(ModeInfo *mi, int mi_row, int mi_col, BLOCK_SIZE bsize, int64 rate, int64 dist, int skip, FrameBuffer *fb) const {
	const int bw			= num_8x8_blocks_wide_lookup[bsize];
	const int bh			= num_8x8_blocks_high_lookup[bsize];
	const int xmis			= min(mi_cols - mi_col, bw);
	const int ymis			= min(mi_rows - mi_row, bh);
	const int block_index	= mi_row * mi_cols + mi_col;
	int		refresh_this_block = cr.candidate_refresh_aq(mi, rate, dist, bsize);
	// Default is to not update the refresh map.
	int		new_map_value	= cr.map[block_index];

	if (refresh_this_block == 0 && bsize <= BLOCK_16X16 && use_skin_detection && compute_skin_block(fb->y_buffer, fb->u_buffer, fb->v_buffer, mi_row * 8, mi_col * 8, bw, bh))
		refresh_this_block = 1;

	// If this block is labeled for refresh, check if we should reset the segment_id.
	if (CYCLIC_REFRESH::segment_id_boosted(mi->segment_id))
		mi->segment_id = skip ? CYCLIC_REFRESH::SEGMENT_ID_BASE : refresh_this_block;

	// Update the cyclic refresh map, to be used for setting segmentation map for the next frame. If the block  will be refreshed this frame, mark it as clean.
	// The magnitude of the -ve influences how long before we consider it for refresh again.
	if (CYCLIC_REFRESH::segment_id_boosted(mi->segment_id)) {
		new_map_value = -cr.time_for_refresh;
	} else if (refresh_this_block) {
		// Else if it is accepted as candidate for refresh, and has not already been refreshed (marked as 1) then mark it as a candidate for cleanup for future time (marked as 0), otherwise don't update it.
		if (cr.map[block_index] == 1)
			new_map_value = 0;
	} else {
		// Leave it marked as block that is not candidate for refresh.
		new_map_value = 1;
	}

	// Update entries in the cyclic refresh map with new_map_value, and
	// copy mbmi->segment_id into global segmentation map.
	for (int y = 0; y < ymis; y++) {
		for (int x = 0; x < xmis; x++) {
			int		offset		= block_index + y * mi_cols + x;
			cr.map[offset]		= new_map_value;
			((uint8*)seg_map[1])[offset]	= mi->segment_id;
		}
	}
}
void Encoder::cyclic_refresh_update_sb_postencode(const ModeInfo *const mi, int mi_row, int mi_col, BLOCK_SIZE bsize) {
	MotionVector mv			= mi->sub_mv[3].mv[0];
	const int bw			= num_8x8_blocks_wide_lookup[bsize];
	const int bh			= num_8x8_blocks_high_lookup[bsize];
	const int xmis			= min(mi_cols - mi_col, bw);
	const int ymis			= min(mi_rows - mi_row, bh);
	const int block_index	= mi_row * mi_cols + mi_col;
	for (int y = 0; y < ymis; y++) {
		for (int x = 0; x < xmis; x++) {
			int map_offset = block_index + y * mi_cols + x;
			// Inter skip blocks were clearly not coded at the current qindex, so don't update the map for them
			// For cases where motion is non-zero or the reference frame isn't the previous frame, the previous value in the map for this spatial location is not entirely correct
			if ((!mi->is_inter_block() || !mi->skip) && mi->segment_id <= CYCLIC_REFRESH::SEGMENT_ID_BOOST2) {
				cr.last_coded_q_map[map_offset] = clamp(quant.base_index + cr.qindex_delta[mi->segment_id], 0, MAXQ);
			} else if (mi->is_inter_block() && mi->skip && mi->segment_id <= CYCLIC_REFRESH::SEGMENT_ID_BOOST2) {
				cr.last_coded_q_map[map_offset] = min(clamp(quant.base_index + cr.qindex_delta[mi->segment_id], 0, MAXQ), cr.last_coded_q_map[map_offset]);
				// Update the consecutive zero/low_mv count.
				if (mi->is_inter_block() && (abs(mv.row) < 8 && abs(mv.col) < 8)) {
					if (cr.consec_zero_mv[map_offset] < 255)
						cr.consec_zero_mv[map_offset]++;
				} else {
					cr.consec_zero_mv[map_offset] = 0;
				}
			}
		}
	}
}

// Update some encoding stats (from the just encoded frame). If this frame's background has high motion, refresh the golden frame
// Otherwise, if the golden reference is to be updated check if we should NOT update the golden ref
void Encoder::cyclic_refresh_check_golden_update() {
	ModeInfo	**mi	= mi_grid + mi_stride + 1;

	int			cnt1 = 0, cnt2 = 0, low_content_frame = 0;

	for (int mi_row = 0; mi_row < mi_rows; mi_row++) {
		for (int mi_col = 0; mi_col < mi_cols; mi_col++) {
			int16	abs_mvr = abs(mi[0]->sub_mv[3].mv[0].row);
			int16	abs_mvc = abs(mi[0]->sub_mv[3].mv[0].col);

			// Calculate the motion of the background.
			if (abs_mvr <= 16 && abs_mvc <= 16) {
				cnt1++;
				if (abs_mvr == 0 && abs_mvc == 0)
					cnt2++;
			}
			mi++;

			// Accumulate low_content_frame.
			if (cr.map[mi_row * mi_cols + mi_col] < 1)
				low_content_frame++;
		}
		mi += 8;
	}

	// For video conference clips, if the background has high motion in current
	// frame because of the camera movement, set this frame as the golden frame.
	// Use 70% and 5% as the thresholds for golden frame refreshing.
	// Also, force this frame as a golden update frame if this frame will change
	// the resolution (resize_pending != 0).
	bool	force_gf_refresh = false;
	if (resize_pending || (cnt1 > (7 * mi_rows * mi_cols) && cnt2 * 20 < cnt1)) {
		rc.set_golden_update(cr.percent_refresh);
		rc.frames_till_gf_update_due = rc.baseline_gf_interval;

		if (rc.frames_till_gf_update_due > rc.frames_to_key)
			rc.frames_till_gf_update_due = rc.frames_to_key;
		refresh_golden_frame = force_gf_refresh = true;
	}

	float	fraction_low = float(low_content_frame) / (mi_rows * mi_cols);
	// Update average.
	cr.low_content_avg = (fraction_low + 3 * cr.low_content_avg) / 4;
	if (!force_gf_refresh && refresh_golden_frame) {
		// Don't update golden reference if the amount of low_content for the current encoded frame is small, or if the recursive average of the low_content over the update interval window falls below threshold.
		if (fraction_low < 0.8f || cr.low_content_avg < 0.7f)
			refresh_golden_frame = false;
		// Reset for next internal.
		cr.low_content_avg = fraction_low;
	}
}
//-----------------------------------------------------------------------------
//	RD
//-----------------------------------------------------------------------------

void extend_probs(const prob *model, prob *full) {
	if (full != model)
		memcpy(full, model, sizeof(prob) * UNCONSTRAINED_NODES);
	memcpy(full + UNCONSTRAINED_NODES, pareto8_full[model[PIVOT_NODE] == 0 ? 254 : model[PIVOT_NODE] - 1], MODEL_NODES * sizeof(prob));
}

void build_mv_component_cost_table(uint32 *mvcost, const FrameContext::mvs::component &comp, bool usehp) {
	uint32	sign_cost[2] = {prob_code::cost0(comp.sign), prob_code::cost1(comp.sign)};
	uint32	class_cost[MotionVector::CLASSES];
	uint32	class0_cost[MotionVector::CLASS0_SIZE];

	uint32	bits_cost[MotionVector::OFFSET_BITS][2];
	uint32	class0_fp_cost[MotionVector::CLASS0_SIZE][MotionVector::FP_SIZE];
	uint32	fp_cost[MotionVector::FP_SIZE];
	uint32	class0_hp_cost[2];
	uint32	hp_cost[2];

	prob_code::cost(class_cost, comp.classes, mv_class_tree);
	prob_code::cost(class0_cost, comp.class0, mv_class0_tree);
	prob_code::cost(fp_cost, comp.fp, mv_fp_tree);

	for (int i = 0; i < MotionVector::OFFSET_BITS; ++i) {
		bits_cost[i][0] = prob_code::cost0(comp.bits[i]);
		bits_cost[i][1] = prob_code::cost1(comp.bits[i]);
	}

	for (int i = 0; i < MotionVector::CLASS0_SIZE; ++i)
		prob_code::cost(class0_fp_cost[i], comp.class0_fp[i], mv_fp_tree);

	if (usehp) {
		class0_hp_cost[0]	= prob_code::cost0(comp.class0_hp);
		class0_hp_cost[1]	= prob_code::cost1(comp.class0_hp);
		hp_cost[0]			= prob_code::cost0(comp.hp);
		hp_cost[1]			= prob_code::cost1(comp.hp);
	}
	mvcost[0] = 0;
	for (int v = 1; v <= MotionVector::MAX; ++v) {
		int o;
		int	z		= v - 1;
		int	c		= MotionVector::get_class(z, &o);
		int	d		= o >> 3;			// int mv data
		int	f		= (o >> 1) & 3;		// fractional pel mv data
		int	e		= o & 1;			// high precision mv data

		int	cost	= class_cost[c];
		if (c == MotionVector::CLASS_0) {
			cost += class0_cost[d] + class0_fp_cost[d][f];
		} else {
			int	b = c + MotionVector::CLASS0_BITS - 1;  /* number of bits */
			for (int i = 0; i < b; ++i)
				cost += bits_cost[i][((d >> i) & 1)];
			cost += fp_cost[f];
		}
		if (usehp)
			cost += c == MotionVector::CLASS_0 ? class0_hp_cost[e] : hp_cost[e];

		mvcost[v]	= cost + sign_cost[0];
		mvcost[-v]	= cost + sign_cost[1];
	}
}

void Encoder::initialize_rd_consts(TileEncoder *x) {
	rd.rddiv	= RD_OPT::RD_DIVBITS;  // In bits (to multiply D by 128).
	rd.rdmult	= compute_rd_mult(quant.base_index + quant.y_dc_delta);

	x->errorperbit		= max(rd.rdmult / RD_MULT_EPB_RATIO, 1);
	x->select_tx_size	= (sf.tx_size_search_method == SPEED_FEATURES::USE_LARGESTALL && frame_type != KEY_FRAME) ? 0 : 1;


	rd.set_block_thresholds(seg, quant, cs.bit_depth);

	if (!sf.use_nonrd_pick_mode || frame_type == KEY_FRAME) {
		for (int t = TX_4X4; t <= TX_32X32; ++t) {
			for (int i = 0; i < PLANE_TYPES; ++i) {
				for (int j = 0; j < REF_TYPES; ++j) {
					auto	*c = x->token_costs[t][i][j].all();
					auto	*p = fc.coef_probs[t][i][j].all();
					for (int k = 0; k < Bands<uint32>::TOTAL; ++k) {
						prob	probs[ENTROPY_NODES];
						extend_probs(p[k], probs);
						prob_code::cost(c[k][0], probs, coef_con_tree);
						prob_code::cost_skip(c[k][1], probs, coef_con_tree);
						ISO_ASSERT(c[k][0][EOB_TOKEN] == c[k][1][EOB_TOKEN]);
					}
				}
			}
		}
	}

	if (sf.partition_search_type != SPEED_FEATURES::VAR_BASED_PARTITION || frame_type == KEY_FRAME) {
		const FrameContext::partition_probs_t	*partition_probs = fc.get_partition_probs(frame_is_intra_only());
		for (int i = 0; i < PARTITION_CONTEXTS; ++i)
			prob_code::cost(partition_cost[i], partition_probs[i], partition_tree);
	}

	if (!sf.use_nonrd_pick_mode || (curr_frame & 0x07) == 1 || frame_type == KEY_FRAME) {
		for (int i = 0; i < INTRA_MODES; ++i)
			for (int j = 0; j < INTRA_MODES; ++j)
				prob_code::cost(y_mode_costs[i][j], kf_y_mode_prob[i][j], intra_mode_tree);

		prob_code::cost(mbmode_cost, fc.y_mode_prob[1], intra_mode_tree);
		for (int i = 0; i < INTRA_MODES; ++i) {
			prob_code::cost(intra_uv_mode_cost[KEY_FRAME][i], kf_uv_mode_prob[i], intra_mode_tree);
			prob_code::cost(intra_uv_mode_cost[INTER_FRAME][i], fc.uv_mode_prob[i], intra_mode_tree);
		}

		for (int i = 0; i < SWITCHABLE_FILTER_CONTEXTS; ++i)
			prob_code::cost(switchable_interp_costs[i], fc.switchable_interp_prob[i], switchable_interp_tree);

		if (!frame_is_intra_only()) {
			prob_code::cost(x->nmvjointcost, fc.mv.joints, mv_joint_tree);
			uint32 **mvcost		= allow_high_precision_mv ? x->nmvcost_hp : x->nmvcost;
			build_mv_component_cost_table(mvcost[0], fc.mv.comps[0], allow_high_precision_mv);
			build_mv_component_cost_table(mvcost[1], fc.mv.comps[1], allow_high_precision_mv);

			for (int i = 0; i < INTER_MODE_CONTEXTS; ++i)
				prob_code::cost(inter_mode_cost[i], fc.inter_mode_probs[i], inter_mode_tree);
		}
	}
}

//-----------------------------------------------------------------------------
//	Active Map
//-----------------------------------------------------------------------------

#define AM_SEGMENT_ID_INACTIVE		7
#define AM_SEGMENT_ID_ACTIVE		0
#define ALTREF_HIGH_PRECISION_MV	1	// Whether to use high precision mv for altref computation.
#define HIGH_PRECISION_MV_QTHRESH	200	// Q threshold for high precision mv. Choose a very high value for now so that HIGH_PRECISION is always chosen.

static inline void Scale2Ratio(SCALING mode, int *hr, int *hs) {
	switch (mode) {
		default:
		case NORMAL:	*hr = 1; *hs = 1; break;
		case FOURFIVE:	*hr = 4; *hs = 5; break;
		case THREEFIVE:	*hr = 3; *hs = 5; break;
		case ONETWO:	*hr = 1; *hs = 2; break;
	}
}

// Mark all inactive blocks as active.
// Other segmentation features may be set so memset cannot be used, instead only inactive blocks should be reset.
void Encoder::suppress_active_map() {
	if (active_map.enabled || active_map.update) {
		uint8 *const map = segmentation_map;
		for (int i = 0; i < mi_rows * mi_cols; ++i)
			if (map[i] == AM_SEGMENT_ID_INACTIVE)
				map[i] = AM_SEGMENT_ID_ACTIVE;
	}
}

void Encoder::apply_active_map() {
	uint8		*const seg_map	= segmentation_map;
	const uint8 *const active	= active_map.map;
	
	//assert(AM_SEGMENT_ID_ACTIVE == CYCLIC_REFRESH::SEGMENT_ID_BASE);

	if (frame_is_intra_only()) {
		active_map.enabled	= false;
		active_map.update	= true;
	}

	if (active_map.update) {
		if (active_map.enabled) {
			for (int i = 0; i < mi_rows * mi_cols; ++i)
				if (seg_map[i] == AM_SEGMENT_ID_ACTIVE)
					seg_map[i] = active[i];
			seg.enable();
			seg.enable(AM_SEGMENT_ID_INACTIVE, Segmentation::FEATURE_SKIP);
			seg.enable(AM_SEGMENT_ID_INACTIVE, Segmentation::FEATURE_ALT_LF);
			// Setting the data to -MAX_LOOP_FILTER will result in the computed loop
			// filter level being zero regardless of the value of seg->abs_delta.
			seg.set_data(AM_SEGMENT_ID_INACTIVE, Segmentation::FEATURE_ALT_LF, -LoopFilter::MAX_LOOP_FILTER);
		} else {
			seg.disable(AM_SEGMENT_ID_INACTIVE, Segmentation::FEATURE_SKIP);
			seg.disable(AM_SEGMENT_ID_INACTIVE, Segmentation::FEATURE_ALT_LF);
			if (seg.enabled)
				seg.update_data = seg.update_map = true;
		}
		active_map.update = false;
	}
}

int Encoder::set_active_map(uint8* new_map_16x16, int rows, int cols) {
	if (rows == mi_rows / 2 && cols == mi_cols / 2) {
		uint8 *const active_map_8x8 = active_map.map;
		active_map.update = 1;
		if (new_map_16x16) {
			for (int r = 0; r < mi_rows; ++r) {
				for (int c = 0; c < mi_cols; ++c)
					active_map_8x8[r * mi_cols + c] = new_map_16x16[(r >> 1) * cols + (c >> 1)] ? AM_SEGMENT_ID_ACTIVE : AM_SEGMENT_ID_INACTIVE;
			}
			active_map.enabled = true;
		} else {
			active_map.enabled = false;
		}
		return 0;
	} else {
		return -1;
	}
}

int Encoder::get_active_map(uint8* new_map_16x16, int rows, int cols) {
	if (rows == mi_rows / 2 && cols == mi_cols / 2) {
		uint8* const seg_map_8x8 = segmentation_map;
		memset(new_map_16x16, !active_map.enabled, rows * cols);
		if (active_map.enabled) {
			for (int r = 0; r < mi_rows; ++r) {
				for (int c = 0; c < mi_cols; ++c)
					// Cyclic refresh segments are considered active despite not having AM_SEGMENT_ID_ACTIVE
					new_map_16x16[(r >> 1) * cols + (c >> 1)] |= seg_map_8x8[r * mi_cols + c] != AM_SEGMENT_ID_INACTIVE;
			}
		}
		return 0;
	} else {
		return -1;
	}
}


//-----------------------------------------------------------------------------
//	NOISE
//-----------------------------------------------------------------------------

void Encoder::update_noise_estimate() {
	// Estimate of noise level every frame_period frames.
	int			frame_period			= 10;
	int			thresh_consec_zeromv	= 8;
	uint32		thresh_sum_diff			= 100;
	uint32		thresh_sum_spatial		= (200 * 200) << 8;
	uint32		thresh_spatial_var		= (32 * 32) << 8;
	int			min_blocks_estimate		= mi_rows * mi_cols >> 7;

	// Estimate is between current source and last source.
	FrameBuffer *last_source = Last_Source;
#if CONFIG_VP9_TEMPORAL_DENOISING
	if (oxcf.noise_sensitivity > 0)
		last_source = &denoiser.last_source;
#endif
	noise_estimate.enabled = enable_noise_estimation();
	if (!noise_estimate.enabled || curr_frame % frame_period || !last_source || noise_estimate.last_w != width || noise_estimate.last_h != height) {
	#if CONFIG_VP9_TEMPORAL_DENOISING
		if (oxcf.noise_sensitivity > 0)
			copy_frame(&denoiser.last_source, fb_srce);
	#endif
		if (last_source)
			noise_estimate.set_size(width, height);
		return;
	}

	int		num_samples = 0;
	uint64	avg_est		= 0;
	static const uint8 const_source[16] = {	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	// Loop over sub-sample of 16x16 blocks of frame, and for blocks that have been encoded as zero/small mv at least x consecutive frames, compute the variance to update estimate of noise in the source.
	const int	src_ystride			= Source->y_buffer.stride;
	const int	last_src_ystride	= last_source->y_buffer.stride;

	int		num_low_motion	= 0;
	for (int mi_row = 0; mi_row < mi_rows; mi_row++) {
		for (int mi_col = 0; mi_col < mi_cols; mi_col++) {
			int bl_index = mi_row * mi_cols + mi_col;
			if (cr.consec_zero_mv[bl_index] > thresh_consec_zeromv)
				num_low_motion++;
		}
	}

	bool	frame_low_motion = num_low_motion >= ((3 * mi_rows * mi_cols) >> 3);

	// 16x16 blocks, 1/4 sample of frame.
	for (int mi_row = 0; mi_row < mi_rows - 1; mi_row += 4) {
		for (int mi_col = 0; mi_col < mi_cols - 1; mi_col += 4) {
			int bl_index	= mi_row * mi_cols + mi_col;
			// Only consider blocks that are likely steady background. i.e, have been encoded as zero/low motion x (= thresh_consec_zeromv) frames in a row
			// consec_zero_mv[] defined for 8x8 blocks, so consider all 4 sub-blocks for 16x16 block. Also, avoid skin blocks.
			bool	is_skin = compute_skin_block(Source->y_buffer, Source->u_buffer, Source->v_buffer, mi_col * 8, mi_row * 8, 16, 16);

			if (frame_low_motion
			&&	cr.consec_zero_mv[bl_index]  > thresh_consec_zeromv
			&&	cr.consec_zero_mv[bl_index + 1] > thresh_consec_zeromv
			&&	cr.consec_zero_mv[bl_index + mi_cols] > thresh_consec_zeromv
			&&	cr.consec_zero_mv[bl_index + mi_cols + 1] > thresh_consec_zeromv
			&&	!is_skin
			) {
				const uint8 *src_y1			= Source->y_buffer.row(mi_row * 8) + mi_col * 8;
				const uint8 *last_src_y1	= last_source->y_buffer.row(mi_row * 8) + mi_col * 8;

				// Compute variance
				uint32 sse;
				uint32 variance = variance_table[BLOCK_16X16].vf(src_y1, src_ystride, last_src_y1, last_src_ystride, &sse);
				// Only consider this block as valid for noise measurement if the average term (sse - variance = N * avg^{2}, N = 16X16) of the temporal residual is small (avoid effects from lighting change).
				if (sse - variance < thresh_sum_diff) {
					uint32 sse2;
					const uint32 spatial_variance = variance_table[BLOCK_16X16].vf(src_y1, src_ystride, const_source, 0, &sse2);
					// Avoid blocks with high brightness and high spatial variance.
					if ((sse2 - spatial_variance) < thresh_sum_spatial && spatial_variance < thresh_spatial_var) {
						avg_est += variance / ((spatial_variance >> 9) + 1);
						num_samples++;
					}
				}
			}
		}
	}
	// Update noise estimate if we have at a minimum number of block samples, and avg_est > 0 (avg_est == 0 can happen if the application inputs duplicate frames).
	if (num_samples > min_blocks_estimate && avg_est > 0) {
		if (noise_estimate.update(avg_est / num_samples)) {
		#if CONFIG_VP9_TEMPORAL_DENOISING
			if (oxcf.noise_sensitivity > 0)
				vp9_denoiser_set_noise_level(&denoiser, ne->level);
		#endif
		}
	}
#if CONFIG_VP9_TEMPORAL_DENOISING
	if (oxcf.noise_sensitivity > 0)
		copy_frame(&denoiser.fb_prev, fb_srce);
#endif
}

//-----------------------------------------------------------------------------
//SPEED FEATURES
//-----------------------------------------------------------------------------

void SPEED_FEATURES::set_good_framesize_dependent(int width, int height, int speed, int base_qindex, bool show_frame, bool animation) {
	if (speed >= 1) {
		if (min(width, height) >= 720) {
			disable_split_mask = show_frame ? DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
			partition_search_breakout_dist_thr = (1 << 23);
		} else {
			disable_split_mask = DISABLE_COMPOUND_SPLIT;
			partition_search_breakout_dist_thr = (1 << 21);
		}
	}

	if (speed >= 2) {
		if (min(width, height) >= 720) {
			disable_split_mask			= show_frame ? DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
			adaptive_pred_interp_filter = 0;
			partition_search_breakout_dist_thr = (1 << 24);
			partition_search_breakout_rate_thr = 120;
		} else {
			disable_split_mask			= LAST_AND_INTRA_SPLIT_ONLY;
			partition_search_breakout_dist_thr = (1 << 22);
			partition_search_breakout_rate_thr = 100;
		}

		// Select block size based on image format size.
		uint32 screen_area = width * height;
		rd_auto_partition_min_limit = screen_area < 1280 * 720	? BLOCK_4X4	// Formats smaller in area than 720P
			:	screen_area < 1920 * 1080						? BLOCK_8X8	// Format >= 720P and < 1080P
			:	BLOCK_16X16;												// Formats 1080P and up
	}

	if (speed >= 3) {
		if (min(width, height) >= 720) {
			disable_split_mask = DISABLE_ALL_SPLIT;
			schedule_mode_search = base_qindex < 220;
			partition_search_breakout_dist_thr = (1 << 25);
			partition_search_breakout_rate_thr = 200;
		} else {
			max_intra_bsize = BLOCK_32X32;
			disable_split_mask = DISABLE_ALL_INTER_SPLIT;
			schedule_mode_search = base_qindex < 175;
			partition_search_breakout_dist_thr = (1 << 23);
			partition_search_breakout_rate_thr = 120;
		}
	}

	if (animation)
		disable_split_mask = DISABLE_COMPOUND_SPLIT;

	if (speed >= 4) {
		if (min(width, height) >= 720) {
			partition_search_breakout_dist_thr = (1 << 26);
		} else {
			partition_search_breakout_dist_thr = (1 << 24);
		}
		disable_split_mask = DISABLE_ALL_SPLIT;
	}
}

void SPEED_FEATURES::set_good(int speed, bool boosted, bool animation, bool inter) {
	partition_search_breakout_dist_thr = (1 << 20);
	partition_search_breakout_rate_thr = 80;
	tx_size_search_breakout				= true;
	adaptive_rd_thresh					= true;
	allow_skip_recode					= true;
	less_rectangular_check				= true;
	use_square_partition_only			= !boosted;
	use_square_only_threshold			= BLOCK_16X16;

	if (speed >= 1) {
		use_square_partition_only	= animation ? !boosted : inter;
		use_square_only_threshold	= BLOCK_4X4;
		use_rd_breakout				= true;
		adaptive_motion_search		= true;
		adaptive_rd_thresh			= 2;
		mode_skip_start				= 10;
		adaptive_pred_interp_filter = 1;

		mv.auto_mv_step_size		= true;
		mv.subpel_iters_per_step	= 1;

		recode_loop						= ALLOW_RECODE_KFARFGF;
		intra_y_mode_mask[TX_32X32]		= INTRA_DC_H_V;
		intra_uv_mode_mask[TX_32X32]	= INTRA_DC_H_V;
		intra_y_mode_mask[TX_16X16]		= INTRA_DC_H_V;
		intra_uv_mode_mask[TX_16X16]	= INTRA_DC_H_V;
	}

	if (speed >= 2) {
		tx_size_search_method				= boosted ? USE_FULL_RD : USE_LARGESTALL;
		mode_search_skip_flags				= inter ? FLAG_SKIP_INTRA_DIRMISMATCH | FLAG_SKIP_INTRA_BESTINTER | FLAG_SKIP_COMP_BESTINTRA | FLAG_SKIP_INTRA_LOWVAR : 0;
		disable_filter_search_var_thresh	= 100;
		comp_inter_joint_search_thresh		= BLOCK_SIZES;
		auto_min_max_partition_size			= RELAXED_NEIGHBORING_MIN_MAX;
		allow_partition_search_skip			= true;
	}

	if (speed >= 3) {
		use_square_partition_only		= inter;
		tx_size_search_method			= inter ? USE_LARGESTALL : USE_FULL_RD;
		mv.subpel_search_method			= MV::SUBPEL_TREE_PRUNED;
		adaptive_pred_interp_filter		= 0;
		adaptive_mode_search			= true;
		cb_partition_search				= !boosted;
		cb_pred_filter_search			= true;
		alt_ref_search_fp				= true;
		recode_loop						= ALLOW_RECODE_KFMAXBW;
		adaptive_rd_thresh				= 3;
		mode_skip_start					= 6;
		intra_y_mode_mask[TX_32X32]		= INTRA_DC;
		intra_uv_mode_mask[TX_32X32]	= INTRA_DC;
		adaptive_interp_filter_search	= true;
	}

	if (speed >= 4) {
		use_square_partition_only		= true;
		tx_size_search_method			= USE_LARGESTALL;
		adaptive_rd_thresh				= 4;
		if (inter)
			mode_search_skip_flags |= FLAG_EARLY_TERMINATE;
		disable_filter_search_var_thresh = 200;
		use_lp32x32fdct					= true;
		use_fast_coef_updates			= ONE_LOOP_REDUCED;
		use_fast_coef_costing			= true;
		motion_field_mode_search		= !boosted;
		partition_search_breakout_rate_thr = 300;

		mv.search_method				= MV::BIGDIA;
		mv.subpel_search_method			= MV::SUBPEL_TREE_PRUNED_MORE;
	}

	if (speed >= 5) {
		optimize_coefficients			= false;
		mv.search_method				= MV::HEX;
		disable_filter_search_var_thresh = 500;
		for (int i = 0; i < TX_SIZES; ++i) {
			intra_y_mode_mask[i]	= INTRA_DC;
			intra_uv_mode_mask[i]	= INTRA_DC;
		}
		partition_search_breakout_rate_thr = 500;
		mv.reduce_first_step_size		= true;
		simple_model_rd_from_var		= true;
	}
}

void SPEED_FEATURES::set_realtime_framesize_dependent(int width, int height, int speed, bool show_frame) {
	if (speed >= 1)
		disable_split_mask = min(width, height) >= 720
			? (show_frame ? DISABLE_ALL_SPLIT		: DISABLE_ALL_INTER_SPLIT)
			: (speed >= 2 ? DISABLE_COMPOUND_SPLIT	: LAST_AND_INTRA_SPLIT_ONLY);

	if (speed >= 5)
		partition_search_breakout_dist_thr = min(width, height) >= 720 ? (1 << 25) : (1 << 23);

	if (speed >= 7)
		encode_breakout_thresh = min(width, height) >= 720 ? 800 : 300;
}

void SPEED_FEATURES::set_realtime(int speed, TUNE_CONTENT content, int frames_since_key, bool inter) {
//	const int is_keyframe = cm->frame_type == KEY_FRAME;
//	const int frames_since_key = is_keyframe ? 0 : cpi->rc.frames_since_key;

	static_segmentation			= false;
	adaptive_rd_thresh			= 1;
	use_fast_coef_costing		= true;
	allow_exhaustive_searches	= false;
	exhaustive_searches_thresh	= INT_MAX;

	if (speed >= 1) {
		use_square_partition_only		= inter;
		less_rectangular_check			= true;
		tx_size_search_method			= inter ? USE_LARGESTALL : USE_FULL_RD;
		use_rd_breakout					= true;
		adaptive_motion_search			= true;
		adaptive_pred_interp_filter		= 1;
		mv.auto_mv_step_size			= true;
		adaptive_rd_thresh				= 2;
		intra_y_mode_mask[TX_32X32]		= INTRA_DC_H_V;
		intra_uv_mode_mask[TX_32X32]	= INTRA_DC_H_V;
		intra_uv_mode_mask[TX_16X16]	= INTRA_DC_H_V;
	}

	if (speed >= 2) {
		mode_search_skip_flags			= frames_since_key == 0 ? 0 : FLAG_SKIP_INTRA_DIRMISMATCH | FLAG_SKIP_INTRA_BESTINTER | FLAG_SKIP_COMP_BESTINTRA | FLAG_SKIP_INTRA_LOWVAR;
		adaptive_pred_interp_filter		= 2;

		// Disable reference masking if using spatial scaling since pred_mv_sad will not be set (since vp9_mv_pred will not be called).
		//AJS_TODO sf->reference_masking = (oxcf.resize_mode != RESIZE_DYNAMIC && svc.number_spatial_layers == 1) ? 1 : 0;

		disable_filter_search_var_thresh	= 50;
		comp_inter_joint_search_thresh		= BLOCK_SIZES;
		auto_min_max_partition_size			= RELAXED_NEIGHBORING_MIN_MAX;
		lf_motion_threshold					= LOW_MOTION_THRESHOLD;
		adjust_partitioning_from_last_frame = true;
		last_partitioning_redo_frequency	= 3;
		use_lp32x32fdct						= true;
		mode_skip_start						= 11;
		intra_y_mode_mask[TX_16X16]			= INTRA_DC_H_V;
	}

	if (speed >= 3) {
		use_square_partition_only			= true;
		disable_filter_search_var_thresh	= 100;
		use_uv_intra_rd_estimate			= true;
		skip_encode_sb						= true;
		mv.subpel_iters_per_step			= 1;
		adaptive_rd_thresh					= 4;
		mode_skip_start						= 6;
		allow_skip_recode					= false;
		optimize_coefficients				= false;
		disable_split_mask					= DISABLE_ALL_SPLIT;
		lpf_pick							= LPF_PICK_FROM_Q;
	}

	if (speed >= 4) {
		last_partitioning_redo_frequency = 4;
		adaptive_rd_thresh			= 5;
		use_fast_coef_costing		= false;
		auto_min_max_partition_size = STRICT_NEIGHBORING_MIN_MAX;
		adjust_partitioning_from_last_frame = /*AJS_TODO cm->last_frame_type != cm->frame_type || */(frames_since_key + 1) % last_partitioning_redo_frequency == 0;
		mv.subpel_force_stop = 1;
		for (int i = 0; i < TX_SIZES; i++) {
			intra_y_mode_mask[i]	= INTRA_DC_H_V;
			intra_uv_mode_mask[i]	= INTRA_DC;
		}
		intra_y_mode_mask[TX_32X32]		= INTRA_DC;
		frame_parameter_update			= false;
		mv.search_method				= MV::FAST_HEX;

		inter_mode_mask[BLOCK_32X32]	= INTER_NEAREST_NEAR_NEW;
		inter_mode_mask[BLOCK_32X64]	= INTER_NEAREST;
		inter_mode_mask[BLOCK_64X32]	= INTER_NEAREST;
		inter_mode_mask[BLOCK_64X64]	= INTER_NEAREST;
		max_intra_bsize					= BLOCK_32X32;
		allow_skip_recode				= true;
	}

	if (speed >= 5) {
		use_quant_fp = frames_since_key > 0;
		auto_min_max_partition_size		= frames_since_key == 0 ? RELAXED_NEIGHBORING_MIN_MAX : STRICT_NEIGHBORING_MIN_MAX;
		default_max_partition_size		= BLOCK_32X32;
		default_min_partition_size		= BLOCK_8X8;
		force_frame_boost				= frames_since_key == 0 || (frames_since_key % (last_partitioning_redo_frequency << 1) == 1);
		max_delta_qindex				= frames_since_key == 0 ? 20 : 15;
		partition_search_type			= REFERENCE_PARTITION;
		use_nonrd_pick_mode				= true;
		allow_skip_recode				= false;
		inter_mode_mask[BLOCK_32X32]	= INTER_NEAREST_NEW_ZERO;
		inter_mode_mask[BLOCK_32X64]	= INTER_NEAREST_NEW_ZERO;
		inter_mode_mask[BLOCK_64X32]	= INTER_NEAREST_NEW_ZERO;
		inter_mode_mask[BLOCK_64X64]	= INTER_NEAREST_NEW_ZERO;
		adaptive_rd_thresh = 2;
		// This feature is only enabled when partition search is disabled.
		reuse_inter_pred_sby			= true;
		partition_search_breakout_rate_thr = 200;
		coeff_prob_appx_step			= 4;
		use_fast_coef_updates			= frames_since_key == 0 ? TWO_LOOP : ONE_LOOP_REDUCED;
		mode_search_skip_flags			= FLAG_SKIP_INTRA_DIRMISMATCH;
		tx_size_search_method			= frames_since_key == 0 ? USE_LARGESTALL : USE_TX_8X8;
		simple_model_rd_from_var		= true;

		if (frames_since_key != 0) {
			for (int i = 0; i < BLOCK_SIZES; ++i)
				intra_y_mode_bsize_mask[i] = content == CONTENT_SCREEN ? INTRA_DC_TM_H_V : i > BLOCK_16X16 ? INTRA_DC : INTRA_DC_H_V;
		}
		if (content == CONTENT_SCREEN)
			short_circuit_flat_blocks = true;
	}

	if (speed >= 6) {
		partition_search_type		= VAR_BASED_PARTITION;
		// Turn on this to use non-RD key frame coding mode.
		use_nonrd_pick_mode			= true;
		mv.search_method			= MV::NSTEP;
		mv.reduce_first_step_size	= true;
		skip_encode_sb				= false;
	}

	if (speed >= 7) {
		adaptive_rd_thresh			= 3;
		mv.search_method			= MV::FAST_DIAMOND;
		mv.fullpel_search_step_param = 10;
/*		AJS_TODO
		if (svc.number_temporal_layers > 2 && svc.temporal_layer_id == 0) {
			mv.search_method			= MV::NSTEP;
			mv.fullpel_search_step_param = 6;
		}*/
	}
	if (speed >= 8) {
		adaptive_rd_thresh		= 4;
		mv.subpel_force_stop	= 2;
		lpf_pick				= LPF_PICK_MINIMAL_LPF;
	}
}

void SPEED_FEATURES::set_framesize_independent() {
	// best quality defaults
	frame_parameter_update			= true;
	recode_loop						= ALLOW_RECODE;
	mv.search_method				= MV::NSTEP;
	mv.subpel_search_method			= MV::SUBPEL_TREE;
	mv.subpel_iters_per_step		= 2;
	mv.subpel_force_stop			= 0;
	mv.reduce_first_step_size		= false;
	coeff_prob_appx_step			= 1;
	mv.auto_mv_step_size			= false;
	mv.fullpel_search_step_param	= 6;
	comp_inter_joint_search_thresh	= BLOCK_4X4;
	tx_size_search_method			= USE_FULL_RD;
	use_lp32x32fdct					= false;
	adaptive_motion_search			= false;
	adaptive_pred_interp_filter		= 0;
	adaptive_mode_search			= false;
	cb_pred_filter_search			= false;
	cb_partition_search				= false;
	motion_field_mode_search		= false;
	alt_ref_search_fp				= false;
	use_quant_fp					= false;
	reference_masking				= false;
	partition_search_type			= SEARCH_PARTITION;
	less_rectangular_check			= false;
	use_square_partition_only		= false;
	use_square_only_threshold		= BLOCK_SIZES;
	auto_min_max_partition_size		= NOT_IN_USE;
	rd_auto_partition_min_limit		= BLOCK_4X4;
	default_max_partition_size		= BLOCK_64X64;
	default_min_partition_size		= BLOCK_4X4;
	adjust_partitioning_from_last_frame = false;
	last_partitioning_redo_frequency = 4;
	disable_split_mask				= 0;
	mode_search_skip_flags			= 0;
	force_frame_boost				= false;
	max_delta_qindex				= 0;
	disable_filter_search_var_thresh = 0;
	adaptive_interp_filter_search	= false;
	allow_partition_search_skip		= false;

	for (int i = 0; i < TX_SIZES; i++)
		intra_y_mode_mask[i] = intra_uv_mode_mask[i] = INTRA_ALL;

	use_rd_breakout					= false;
	skip_encode_sb					= false;
	use_uv_intra_rd_estimate		= false;
	allow_skip_recode				= false;
	lpf_pick						= LPF_PICK_FROM_FULL_IMAGE;
	use_fast_coef_updates			= TWO_LOOP;
	use_fast_coef_costing			= false;
	mode_skip_start					= RD_OPT::MAX_MODES;  // Mode index at which mode skip mask set
	schedule_mode_search			= false;
	use_nonrd_pick_mode				= false;

	for (int i = 0; i < BLOCK_SIZES; ++i)
		inter_mode_mask[i] = INTER_ALL;

	max_intra_bsize					= BLOCK_64X64;
	reuse_inter_pred_sby			= false;
	// This setting only takes effect when partition_search_type is set to FIXED_PARTITION.
	always_this_block_size			= BLOCK_16X16;
	search_type_check_frequency		= 50;
	encode_breakout_thresh			= 0;
	// Recode loop tolerance %.
	recode_tolerance				= 25;
	default_interp_filter			= INTERP_SWITCHABLE;
	simple_model_rd_from_var		= false;
	short_circuit_flat_blocks		= false;

	// Some speed-up features even for best quality as minimal impact on quality.
	adaptive_rd_thresh				= 1;
	tx_size_search_breakout			= true;
	partition_search_breakout_dist_thr = 1 << 19;
	partition_search_breakout_rate_thr = 80;

	allow_exhaustive_searches = true;
}

void SPEED_FEATURES::set_meshes(int speed, bool animation) {
	// Mesh search patters for various speed settings
	static const MESH_PATTERN patterns[][MAX_MESH_STEP] = {
		{{64, 4}, {28, 2}, {15, 1}, {7, 1}},
		{{64, 8}, {28, 4}, {15, 1}, {7, 1}},
		{{64, 8}, {28, 4}, {15, 1}, {7, 1}},
		{{64, 8},  {14, 2}, {7, 1},  {7, 1}},
		{{64, 16}, {24, 8}, {12, 4}, {7, 1}},
		{{64, 16}, {24, 8}, {12, 4}, {7, 1}},
		{{64, 16}, {24, 8}, {12, 4}, {7, 1}},
	};
	static const uint8 max_pct[] = { 100, 50, 25, 15, 5, 1, 1 };

	int	index = clamp(speed + 1, 0, num_elements(patterns) - 1);
	mesh_patterns		= patterns[index];
	max_exaustive_pct	= max_pct[index];
	exhaustive_searches_thresh	=
		  index == 0 ? (animation ? 1 << 20 : 1 << 21)
		: index == 1 ? (animation ? 1 << 22 : 1 << 23)
		: exhaustive_searches_thresh << 1;

}
//-----------------------------------------------------------------------------
//	MOTION
//-----------------------------------------------------------------------------
int find_best_16x16_intra(const Buffer2D &src, const Buffer2D &dst, int mb_row, int mb_col, PREDICTION_MODE *pbest_mode);
int do_16x16_zerozero_search(const Buffer2D &src, const Buffer2D &dst, int mb_row, int mb_col, MotionVector *dst_mv);
int do_16x16_motion_search(const Buffer2D &src, const Buffer2D &pre, int mb_row, int mb_col, const MotionVector *ref_mv, MotionVector *dst_mv);

void MBGRAPH_MB_STATS::update(FrameBuffer *src, FrameBuffer *dst, FrameBuffer *golden_ref, const MotionVector *prev_golden_ref_mv, FrameBuffer *alt_ref, int mb_row, int mb_col) {
	// do intra 16x16 prediction
	int		intra_error = find_best_16x16_intra(src.y_buffer, dst.y_buffer, int mb_row, int mb_col, &ref[REFFRAME_INTRA].mode);
	if (intra_error <= 0)
		intra_error = 1;
	ref[REFFRAME_INTRA].err = intra_error;

	// Golden frame MotionVector search, if it exists and is different than last frame
	if (golden_ref) {
		ref[REFFRAME_GOLDEN].err = do_16x16_motion_search(src.y_buffer, golden_ref->y_buffer, prev_golden_ref_mv, int mb_row, int mb_col, &ref[REFFRAME_GOLDEN].mv);
	} else {
		ref[REFFRAME_GOLDEN].err = INT_MAX;
		ref[REFFRAME_GOLDEN].mv.clear();
	}

	// Do an Alt-ref frame MotionVector search, if it exists and is different than last/golden frame.
	if (alt_ref) {
		ref[REFFRAME_ALTREF].err = do_16x16_zerozero_search(src.y_buffer, alt_ref.y_buffer, mb_row, mb_col, &ref[REFFRAME_ALTREF].mv);
	} else {
		ref[REFFRAME_ALTREF].err = INT_MAX;
		ref[REFFRAME_ALTREF].mv.clear();
	}
}

// void separate_arf_mbs_byzz
static void Encoder::separate_arf_mbs() {
	int ncnt[4]		= { 0 };
	int n_frames	= mbgraph_n_frames;
	int *arf_not_zz = calloc(cm->mb_rows * cm->mb_cols * sizeof(*arf_not_zz), 1));

	// We are not interested in results beyond the alt ref itself.
	if (n_frames > rc.frames_till_gf_update_due)
		n_frames = rc.frames_till_gf_update_due;

	// defer cost to reference frames
	for (int i = n_frames - 1; i >= 0; i--) {
		MBGRAPH_FRAME_STATS &frame_stats = mbgraph_stats[i];

		for (int offset = 0, mb_row = 0; mb_row < cm->mb_rows;
			offset += cm->mb_cols, mb_row++) {
			for (int mb_col = 0; mb_col < cm->mb_cols; mb_col++) {
				MBGRAPH_MB_STATS *mb_stats = &frame_stats->mb_stats[offset + mb_col];

				int altref_err = mb_stats->ref[REFFRAME_ALTREF].err;
				int intra_err = mb_stats->ref[REFFRAME_INTRA].err;
				int golden_err = mb_stats->ref[REFFRAME_GOLDEN].err;

				// Test for altref vs intra and gf and that its mv was 0,0.
				if (altref_err > 1000 ||
					altref_err > intra_err ||
					altref_err > golden_err) {
					arf_not_zz[offset + mb_col]++;
				}
			}
		}
	}

	// arf_not_zz is indexed by MB, but this loop is indexed by MI to avoid out of bound access in segmentation_map
	for (int mi_row = 0; mi_row < cm->mi_rows; mi_row++) {
		for (int mi_col = 0; mi_col < cm->mi_cols; mi_col++) {
			// If any of the blocks in the sequence failed then the MB goes in segment 0
			if (arf_not_zz[mi_row / 2 * cm->mb_cols + mi_col / 2]) {
				ncnt[0]++;
				seg_map[mi_row * cm->mi_cols + mi_col] = 0;
			} else {
				seg_map[mi_row * cm->mi_cols + mi_col] = 1;
				ncnt[1]++;
			}
		}
	}

	// Only bother with segmentation if over 10% of the MBs in static segment
	// if ( ncnt[1] && (ncnt[0] / ncnt[1] < 10) )
	if (1) {
		// Note % of blocks that are marked as static
		if (MBs)
			static_mb_pct = (ncnt[1] * 100) / (mi_rows * mi_cols);

		// This error case should not be reachable as this function should
		// never be called with the common data structure uninitialized.
		else
			static_mb_pct = 0;

		seg.enable();
	} else {
		static_mb_pct = 0;
		seg.disable();
	}

	// Free localy allocated storage
	free(arf_not_zz);
}


void Encoder::update_mbgraph_frame_stats(TileEncoder *x, MBGRAPH_FRAME_STATS *stats, FrameBuffer *buf, FrameBuffer *golden_ref, FrameBuffer *alt_ref) {
	int offset = 0;
	int mb_y_offset = 0, arf_y_offset = 0, gld_y_offset = 0;
	MotionVector gld_top_mv = { 0, 0 };
	MODE_INFO mi_local;

	vp9_zero(mi_local);
	// Set up limit values for motion vectors to prevent them extending outside
	// the UMV borders.
	x->mv_row_min = -BORDER_MV_PIXELS_B16;
	x->mv_row_max = (cm->mb_rows - 1) * 8 + BORDER_MV_PIXELS_B16;
	xd->up_available = 0;
	xd->plane[0].dst.stride = buf->y_stride;
	xd->plane[0].pre[0].stride = buf->y_stride;
	xd->plane[1].dst.stride = buf->uv_stride;
	xd->mi[0] = &mi_local;
	mi_local.sb_type = BLOCK_16X16;
	mi_local.ref_frame[0] = LAST_FRAME;
	mi_local.ref_frame[1] = NONE;

	for (mb_row = 0; mb_row < mi_rows / 2; mb_row++) {
		MotionVector gld_left_mv = gld_top_mv;
		int mb_y_in_offset = mb_y_offset;
		int arf_y_in_offset = arf_y_offset;
		int gld_y_in_offset = gld_y_offset;

		// Set up limit values for motion vectors to prevent them extending outside
		// the UMV borders.
		x->mv_col_min = -BORDER_MV_PIXELS_B16;
		x->mv_col_max = (cm->mb_cols - 1) * 8 + BORDER_MV_PIXELS_B16;
		xd->left_available = 0;

		for (int mb_col = 0; mb_col < mi_cols / 2; mb_col++) {
			MBGRAPH_MB_STATS *mb_stats = &stats->mb_stats[offset + mb_col];

			update_mbgraph_mb_stats(mb_stats, buf, mb_y_in_offset, golden_ref, &gld_left_mv, alt_ref, mb_row, mb_col);
			gld_left_mv = mb_stats->ref[REFFRAME_GOLDEN].m.mv.as_mv;
			if (mb_col == 0)
				gld_top_mv = gld_left_mv;
			xd->left_available = 1;
			mb_y_in_offset += 16;
			gld_y_in_offset += 16;
			arf_y_in_offset += 16;
			x->mv_col_min -= 16;
			x->mv_col_max -= 16;
		}
		xd->up_available = 1;
		mb_y_offset += buf->y_stride * 16;
		gld_y_offset += golden_ref->y_stride * 16;
		if (alt_ref)
			arf_y_offset += alt_ref->y_stride * 16;
		x->mv_row_min -= 16;
		x->mv_row_max -= 16;
		offset += cm->mb_cols;
	}
}

void Encoder::update_mbgraph_stats() {
	int			n_frames	= lookahead->depth();
	FrameBuffer *golden_ref = get_ref_frame_buffer(REFFRAME_GOLDEN);

	// we need to look ahead beyond where the ARF transitions into being a GF - so exit if we don't look ahead beyond that
	if (n_frames <= rc.frames_till_gf_update_due)
		return;

	if (n_frames > MAX_LAG_BUFFERS)
		n_frames = MAX_LAG_BUFFERS;

	mbgraph_n_frames = n_frames;
	for (int i = 0; i < n_frames; i++) {
		MBGRAPH_FRAME_STATS *frame_stats = &mbgraph_stats[i];
		memset(frame_stats->mb_stats, 0, mi_rows * mi_cols / 4 * sizeof(*mbgraph_stats[i].mb_stats));
	}

	// do motion search to find contribution of each reference to data later on in this GF group
	// FIXME really, the GF/last MC search should be done forward, and the ARF MC search backwards, to get optimal results for MotionVector caching
	for (int i = 0; i < n_frames; i++) {
		MBGRAPH_FRAME_STATS &frame_stats = mbgraph_stats[i];
		lookahead::entry	*q_cur = lookahead->peek(i);
		update_mbgraph_frame_stats(frame_stats, &q_cur->img, golden_ref, Source);
	}
	separate_arf_mbs();
}

//-----------------------------------------------------------------------------
//	GENERIC
//-----------------------------------------------------------------------------

void Encoder::setup_frame() {
	// Set up entropy context depending on frame type. The decoder mandates the use of the default context, index 0, for keyframes and inter frames where the error_resilient_mode or intra_only flag is set
	// For other inter-frames the encoder currently uses only two contexts - context 1 for ALTREF frames and context 0 for the others
	if (frame_is_intra_only() || error_resilient_mode) {
		setup_past_independence(RESET_ALL);
	} else if (!use_svc) {
		frame_context_idx = refresh_alt_ref_frame;
	}

	if (frame_type == KEY_FRAME) {
		if (!is_two_pass_svc())
			refresh_golden_frame = true;
		refresh_alt_ref_frame = true;
		clear(interp_filter_selected);
	} else {
		fc = frame_contexts[frame_context_idx];
		clear(interp_filter_selected[0]);
	}
}

bool Encoder::static_init() {
	for (int i = 0; i < QINDEX_RANGE; i++) {
		const double q = Quantisation::qindex_to_q(i, 8);
		sad_per_bit16lut_8[i]	= int(0.0418 * q / 4.0 + 2.4107);
		sad_per_bit4lut_8[i]	= int(0.063  * q / 4.0 + 2.742);
	}

	RATE_CONTROL::static_init();

	prob_code::tokens_from_tree(mv_joint_encodings,		mv_joint_tree);
	prob_code::tokens_from_tree(mv_class_encodings,		mv_class_tree);
	prob_code::tokens_from_tree(mv_class0_encodings,	mv_class0_tree);
	prob_code::tokens_from_tree(mv_fp_encodings,		mv_fp_tree);

	fixed_divide[0] = 0;
	for (int i = 1; i < 512; ++i)
		fixed_divide[i] = 0x80000 / i;
	return true;
}

void Encoder::save_coding_context(TileEncoder *x) {
	CodingContext *const cc = &coding_context;
	*(FrameContext*)cc = fc;

	// Stores a snapshot of key state variables which can subsequently be restored with a call to vp9_restore_coding_context.
	// These functions are intended for use in a re-code loop in vp9_compress_frame where the quantizer value is adjusted between loop iterations.
	memcpy(cc->nmvjointcost, x->nmvjointcost, sizeof(x->nmvjointcost));
	memcpy(cc->nmvcosts[0], nmvcosts[0], MotionVector::VALS * sizeof(*nmvcosts[0]));
	memcpy(cc->nmvcosts[1], nmvcosts[1], MotionVector::VALS * sizeof(*nmvcosts[1]));
	memcpy(cc->nmvcosts_hp[0], nmvcosts_hp[0], MotionVector::VALS * sizeof(*nmvcosts_hp[0]));
	memcpy(cc->nmvcosts_hp[1], nmvcosts_hp[1], MotionVector::VALS * sizeof(*nmvcosts_hp[1]));
	memcpy(cc->segment_pred_probs, seg.pred_probs, sizeof(seg.pred_probs));
	memcpy(cc->seg_map, seg_map[0], (mi_rows * mi_cols));
	memcpy(cc->last_ref_lf_deltas, lf.last_ref_deltas, sizeof(lf.last_ref_deltas));
	memcpy(cc->last_mode_lf_deltas, lf.last_mode_deltas, sizeof(lf.last_mode_deltas));

}

void Encoder::restore_coding_context(TileEncoder *x) {
	CodingContext *const cc = &coding_context;
	fc = *(FrameContext*)cc;

	// Restore key state variables to the snapshot state stored in the previous call to vp9_save_coding_context.
	memcpy(x->nmvjointcost, cc->nmvjointcost, sizeof(x->nmvjointcost));

	memcpy(nmvcosts[0], cc->nmvcosts[0], MotionVector::VALS * sizeof(*cc->nmvcosts[0]));
	memcpy(nmvcosts[1], cc->nmvcosts[1], MotionVector::VALS * sizeof(*cc->nmvcosts[1]));
	memcpy(nmvcosts_hp[0], cc->nmvcosts_hp[0], MotionVector::VALS * sizeof(*cc->nmvcosts_hp[0]));
	memcpy(nmvcosts_hp[1], cc->nmvcosts_hp[1], MotionVector::VALS * sizeof(*cc->nmvcosts_hp[1]));
	memcpy(seg.pred_probs, cc->segment_pred_probs, sizeof(seg.pred_probs));
	memcpy(seg_map[0], coding_context.seg_map, mi_rows * mi_cols);
	memcpy(lf.last_ref_deltas, cc->last_ref_lf_deltas, sizeof(lf.last_ref_deltas));
	memcpy(lf.last_mode_deltas, cc->last_mode_lf_deltas, sizeof(lf.last_mode_deltas));

}

void Encoder::configure_static_seg_features() {
	bool high_q = rc.avg_q > 48.0;

	// Disable and clear down for KF
	if (frame_type == KEY_FRAME) {
		// Clear down the global segmentation map
		memset(segmentation_map, 0, mi_rows * mi_cols);
		seg.update_map	= seg.update_data = false;
		static_mb_pct	= 0;

		seg.disable();
		seg.clearall();

	} else if (refresh_alt_ref_frame) {
		// If this is an alt ref frame
		// Clear down the global segmentation map
		memset(segmentation_map, 0, mi_rows * mi_cols);
		seg.update_map = seg.update_data = false;
		static_mb_pct = 0;

		// Disable segmentation and individual segment features by default
		seg.disable();
		seg.clearall();

		// Scan frames from current to arf frame.
		// This function re-enables segmentation if appropriate.
		update_mbgraph_stats();

		// If segmentation was enabled set those features needed for the
		// arf itself.
		if (seg.enabled) {
			seg.update_map = seg.update_data = true;

			int	qi_delta	= rc.compute_qdelta(rc.avg_q, rc.avg_q * 0.875, cs.bit_depth);
			seg.set_data(1, Segmentation::FEATURE_ALT_Q, qi_delta - 2);
			seg.set_data(1, Segmentation::FEATURE_ALT_LF, -2);

			seg.enable(1, Segmentation::FEATURE_ALT_Q);
			seg.enable(1, Segmentation::FEATURE_ALT_LF);

			// Where relevant assume segment data is delta data
			seg.abs_delta = false;
		}
	} else if (seg.enabled) {
		// All other frames if segmentation has been enabled

		// First normal frame in a valid gf or alt ref group
		if (rc.frames_since_golden == 0) {
			// Set up segment features for normal frames in an arf group
			if (rc.source_alt_ref_active) {
				seg.update_map	= false;
				seg.update_data = true;
				seg.abs_delta	= false;

				int	qi_delta = rc.compute_qdelta(rc.avg_q, rc.avg_q * 1.125, cs.bit_depth);
				seg.set_data(1, Segmentation::FEATURE_ALT_Q, qi_delta + 2);
				seg.enable(1, Segmentation::FEATURE_ALT_Q);

				seg.set_data(1, Segmentation::FEATURE_ALT_LF, -2);
				seg.enable(1, Segmentation::FEATURE_ALT_LF);

				// Segment coding disabled for compred testing
				if (high_q || (static_mb_pct == 100)) {
					seg.set_data(1, Segmentation::FEATURE_REF_FRAME, REFFRAME_ALTREF);
					seg.enable(1, Segmentation::FEATURE_REF_FRAME);
					seg.enable(1, Segmentation::FEATURE_SKIP);
				}
			} else {
				// Disable segmentation and clear down features if alt ref is not active for this group
				seg.disable();
				memset(segmentation_map, 0, mi_rows * mi_cols);
				seg.update_map = seg.update_data = false;
				seg.clearall();
			}
		} else if (rc.is_src_frame_alt_ref) {
			// Special case where we are coding over the top of a previous alt ref frame.
			// Segment coding disabled for compred testing

			// Enable ref frame features for segment 0 as well
			seg.enable(0, Segmentation::FEATURE_REF_FRAME);
			seg.enable(1, Segmentation::FEATURE_REF_FRAME);

			// All mbs should use ALTREF_FRAME
			seg.clear_data(0, Segmentation::FEATURE_REF_FRAME);
			seg.set_data(0, Segmentation::FEATURE_REF_FRAME, REFFRAME_ALTREF);
			seg.clear_data(1, Segmentation::FEATURE_REF_FRAME);
			seg.set_data(1, Segmentation::FEATURE_REF_FRAME, REFFRAME_ALTREF);

			// Skip all MBs if high Q (0,0 mv and skip coeffs)
			if (high_q) {
				seg.enable(0, Segmentation::FEATURE_SKIP);
				seg.enable(1, Segmentation::FEATURE_SKIP);
			}
			// Enable data update
			seg.update_data = true;
		} else {
			// All other frames - leave things as they are.
			seg.update_map	= seg.update_data = false;
		}
	}
}

void Encoder::update_reference_segmentation_map() {
	ModeInfo	**mi	= mi_grid + mi_stride + 1;
	uint8		*cache	= seg_map[0];

	for (int row = 0; row < mi_rows; row++) {
		for (int col = 0; col < mi_cols; col++)
			cache[col] = mi[col]->segment_id;
		mi		+= mi_stride;
		cache	+= mi_cols;
	}
}

RETURN Encoder::alloc_raw_frame_buffers() {
	if (!lookahead)
		lookahead = new lookahead(oxcf.width, oxcf.height, cs.subsampling_x, cs.subsampling_y, oxcf.lag_in_frames);

	// TODO(agrange) Check if ARF is enabled and skip allocation if not.
	if (alt_ref_buffer.resize(oxcf.width, oxcf.height, cs, ENC_BORDER, buffer_alignment, stride_alignment, false))
		return RETURN_MEM_ERROR;

	return RETURN_OK;
}

RETURN Encoder::alloc_util_frame_buffers() {
	if (last_frame_uf.resize(width, height, cs, ENC_BORDER, buffer_alignment, stride_alignment, false))
		return RETURN_MEM_ERROR;

	if (scaled_source.resize(width, height, cs, ENC_BORDER, buffer_alignment, stride_alignment, false))
		return RETURN_MEM_ERROR;

	if (scaled_last_source.resize(width, height, cs, ENC_BORDER, buffer_alignment, stride_alignment, false))
		return RETURN_MEM_ERROR;

	return RETURN_OK;
}


void Encoder::alloc_compressor_data() {
	alloc_context_buffers(width, height);

	mbmi_ext_base = calloc(mi_cols * mi_rows, sizeof(*mbmi_ext_base));

	free(tile_tok[0][0]);

	uint32 tokens	= get_token_alloc(mb_rows, mb_cols);
	tile_tok[0][0]	= calloc(tokens, sizeof(*tile_tok[0][0])));

	setup_pc_tree(&td);
}

void Encoder::set_tile_limits() {
	int min_log2_tile_cols, max_log2_tile_cols;
	vp9_get_tile_n_bits(mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

	if (is_two_pass_svc() && (svc.encode_empty_frame_state == ENCODING || svc.number_spatial_layers > 1)) {
		log2_tile_cols = 0;
		log2_tile_rows = 0;
	} else {
		log2_tile_cols = clamp(oxcf.tile_columns, min_log2_tile_cols, max_log2_tile_cols);
		log2_tile_rows = oxcf.tile_rows;
	}
}

void Encoder::update_frame_size() {
	TileDecoder *const xd = &td.mb;

	set_mb_mi(cm, width, height);
	init_context_buffers();
	init_macroblockd(xd, NULL);
	td.mb.mbmi_ext_base = mbmi_ext_base;
	memset(mbmi_ext_base, 0, mi_rows * mi_cols * sizeof(*mbmi_ext_base));

	set_tile_limits();

	if (is_two_pass_svc()) {
		if (vpx_realloc_frame_buffer(&alt_ref_buffer, width, height, subsampling_x, subsampling_y, ENC_BORDER, byte_alignment, NULL, NULL, NULL))
			return RETURN_MEM_ERROR;
	}
	return RETURN_OK;
}

void Encoder::realloc_segmentation_maps() {
	// Create the encoder segmentation map and set all entries to 0
	seg_map[1] = calloc(mi_rows * mi_cols, 1);

	// Create a map used for cyclic background refresh.
	cr.init(mi_rows * mi_cols);

	// Create a map used to mark inactive areas.
	free(active_map.map);
	active_map.map = calloc(mi_rows * mi_cols, 1);

	// And a place holder structure is the coding context for use if we want to save and restore it
	free(coding_context.seg_map);
	coding_context.seg_map = calloc(mi_rows * mi_cols, 1);
}

void Encoder::change_config(const EncoderConfig *poxcf) {
	int last_w	= oxcf.width;
	int last_h	= oxcf.height;

	profile		= poxcf->profile;
	bit_depth	= poxcf->bit_depth;
	color_space = poxcf->color_space;
	color_range = poxcf->color_range;
	oxcf		= *poxcf;

	if (poxcf->pass == 0 && poxcf->rc_mode == RC_Q) {
		rc.baseline_gf_interval = FIXED_GF_INTERVAL;
	} else {
		rc.baseline_gf_interval = (MIN_GF_INTERVAL + MAX_GF_INTERVAL) / 2;
	}

	refresh_golden_frame	= false;
	refresh_last_frame		= true;
	refresh_frame_context	= true;
	reset_frame_context		= false;

	seg.reset();
	set_high_precision_mv(false);

	for (int i = 0; i < MAX_SEGMENTS; i++)
		segment_encode_breakout[i] = oxcf.encode_breakout;
	encode_breakout = oxcf.encode_breakout;

	rc.starting_buffer_level	= oxcf.starting_buffer_level_ms * oxcf.target_bandwidth / 1000;
	rc.optimal_buffer_level		= oxcf.optimal_buffer_level_ms == 0 ? oxcf.target_bandwidth / 8 : oxcf.optimal_buffer_level_ms * oxcf.target_bandwidth / 1000;
	rc.maximum_buffer_size		= oxcf.maximum_buffer_size_ms == 0 ? oxcf.target_bandwidth / 8 : oxcf.maximum_buffer_size_ms * oxcf.target_bandwidth / 1000;

	// Under a configuration change, where maximum_buffer_size may change, keep buffer level clipped to the maximum allowed buffer size.
	rc.bits_off_target	= min(rc.bits_off_target, rc.maximum_buffer_size);
	rc.buffer_level		= min(rc.buffer_level, rc.maximum_buffer_size);

	// Set up frame rate and related parameters rate control values.
	new_framerate(framerate);

	// Set absolute upper and lower quality limits
	rc.worst_quality	= oxcf.worst_allowed_q;
	rc.best_quality		= oxcf.best_allowed_q;

	interp_filter = sf.default_interp_filter;

	if (oxcf.render_width > 0 && oxcf.render_height > 0) {
		render_width	= oxcf.render_width;
		render_height	= oxcf.render_height;
	} else {
		render_width	= oxcf.width;
		render_height	= oxcf.height;
	}
	if (last_w != oxcf.width || last_h != oxcf.height) {
		width = oxcf.width;
		height = oxcf.height;
	}

	if (initial_width) {
		int new_mi_size = 0;
		set_mb_mi(width, height);
		new_mi_size = mi_stride * calc_mi_size(mi_rows);
		if (mi_alloc_size < new_mi_size) {
			free_context_buffers(cm);
			alloc_compressor_data();
			realloc_segmentation_maps();
			initial_width = initial_height = 0;
		}
	}
	update_frame_size();

	if ((svc.number_temporal_layers > 1 && oxcf.rc_mode == RC_CBR) || ((svc.number_temporal_layers > 1 || svc.number_spatial_layers > 1) && oxcf.pass != 1))
		update_layer_context_change_config((int)oxcf.target_bandwidth);

	alt_ref_source = NULL;
	rc.is_src_frame_alt_ref = false;

	set_tile_limits();

	ext_refresh_frame_flags_pending		= false;
	ext_refresh_frame_context_pending	= false;

}

#ifndef M_LOG2_E
#define M_LOG2_E 0.693147180559945309417
#endif
#define log2f(x) (log (x) / (float) M_LOG2_E)

// Read before modifying 'cal_nmvjointsadcost' or 'cal_nmvsadcosts'
// cal_nmvjointsadcost and cal_nmvsadcosts are used to calculate cost lookup tables used by 'vp9_diamond_search_sad'
// The C implementation of the function is generic, but the AVX intrinsics optimised version relies on the following properties of the computed tables:
// For cal_nmvjointsadcost: - mvjointsadcost[1] == mvjointsadcost[2] == mvjointsadcost[3]
// For cal_nmvsadcosts: (Equal costs for both components), (Cost function is even)

static void cal_nmvjointsadcost(int *mvjointsadcost) {
	mvjointsadcost[0] = 600;
	mvjointsadcost[1] = 300;
	mvjointsadcost[2] = 300;
	mvjointsadcost[3] = 300;
}

static void cal_nmvsadcosts(int *mvsadcost[2]) {
	int i = 1;

	mvsadcost[0][0] = 0;
	mvsadcost[1][0] = 0;

	do {
		double z = 256 * (2 * (log2f(8 * i) + .6));
		mvsadcost[0][i] = (int)z;
		mvsadcost[1][i] = (int)z;
		mvsadcost[0][-i] = (int)z;
		mvsadcost[1][-i] = (int)z;
	} while (++i <= MV_MAX);
}

static void cal_nmvsadcosts_hp(int *mvsadcost[2]) {
	int i = 1;

	mvsadcost[0][0] = 0;
	mvsadcost[1][0] = 0;

	do {
		double z = 256 * (2 * (log2f(8 * i) + .6));
		mvsadcost[0][i] = (int)z;
		mvsadcost[1][i] = (int)z;
		mvsadcost[0][-i] = (int)z;
		mvsadcost[1][-i] = (int)z;
	} while (++i <= MV_MAX);
}


Encoder::Encoder(EncoderConfig *poxcf) {
	static bool init = static_init();

	clear(*this);

	use_svc					= false;
	resize_state			= RESIZE_ORIG;
	resize_avg_qp			= false;
	resize_buffer_underflow = false;
	use_skin_detection		= false;

	rc.high_source_sad = false;

	oxcf		= *poxcf;
	framerate	= oxcf.init_framerate;
	profile		= oxcf.profile;
	bit_depth	= oxcf.bit_depth;
	color_space = oxcf.color_space;
	color_range = oxcf.color_range;

	width		= oxcf.width;
	height		= oxcf.height;
	alloc_compressor_data();

	svc.temporal_layering_mode = oxcf.temporal_layering_mode;

	// Single thread case: use counts in common.
	td.counts = &counts;

	// Spatial scalability.
	svc.number_spatial_layers = oxcf.ss_number_layers;
	// Temporal scalability.
	svc.number_temporal_layers = oxcf.ts_number_layers;

	if ((svc.number_temporal_layers > 1 && oxcf.rc_mode == RC_CBR) ||
		((svc.number_temporal_layers > 1 ||
		svc.number_spatial_layers > 1) &&
		oxcf.pass != 1)) {
		init_layer_context();
	}

	// change includes all joint functionality
	change_config(oxcf);

	static_mb_pct = 0;
	ref_frame_flags = 0;

	lst_fb_idx = 0;
	gld_fb_idx = 1;
	alt_fb_idx = 2;

	vp9_noise_estimate_init(&noise_estimate, width, height);


	rc.init(&oxcf, oxcf.pass, &rc);

	curr_frame = 0;
	partition_search_skippable_frame = 0;
	tile_data = NULL;

	realloc_segmentation_maps();

	nmvcosts[0]			= calloc(MV_VALS, sizeof(*nmvcosts[0]));
	nmvcosts[1]			= calloc(MV_VALS, sizeof(*nmvcosts[1]));
	nmvcosts_hp[0]		= calloc(MV_VALS, sizeof(*nmvcosts_hp[0]));
	nmvcosts_hp[1]		= calloc(MV_VALS, sizeof(*nmvcosts_hp[1]));
	nmvsadcosts[0]		= calloc(MV_VALS, sizeof(*nmvsadcosts[0]));
	nmvsadcosts[1]		= calloc(MV_VALS, sizeof(*nmvsadcosts[1]));
	nmvsadcosts_hp[0]	= calloc(MV_VALS, sizeof(*nmvsadcosts_hp[0]));
	nmvsadcosts_hp[1]	= calloc(MV_VALS, sizeof(*nmvsadcosts_hp[1]));

	for (int i = 0; i < (sizeof(mbgraph_stats) / sizeof(mbgraph_stats[0])); i++)
		CHECK_MEM_ERROR(cm, mbgraph_stats[i].mb_stats, vpx_calloc(MBs * sizeof(*mbgraph_stats[i].mb_stats), 1));

#if CONFIG_FP_MB_STATS
	use_fp_mb_stats = 0;
	if (use_fp_mb_stats) {
		// a place holder used to store the first pass mb stats in the first pass
		CHECK_MEM_ERROR(cm, twopass.frame_mb_stats_buf, vpx_calloc(MBs * sizeof(uint8), 1));
	} else {
		twopass.frame_mb_stats_buf = NULL;
	}
#endif

	refresh_alt_ref_frame		= false;
	multi_arf_last_grp_enabled	= false;
	b_calculate_psnr			= false;
	first_time_stamp_ever		= INT64_MAX;

	/*********************************************************************
	 * Warning: Read the comments around 'cal_nmvjointsadcost' and       *
	 * 'cal_nmvsadcosts' before modifying how these tables are computed. *
	 *********************************************************************/
	cal_nmvjointsadcost(td.mb.nmvjointsadcost);
	td.mb.nmvcost[0]		= &nmvcosts[0][MV_MAX];
	td.mb.nmvcost[1]		= &nmvcosts[1][MV_MAX];
	td.mb.nmvsadcost[0]		= &nmvsadcosts[0][MV_MAX];
	td.mb.nmvsadcost[1]		= &nmvsadcosts[1][MV_MAX];
	cal_nmvsadcosts(td.mb.nmvsadcost);

	td.mb.nmvcost_hp[0]		= &nmvcosts_hp[0][MV_MAX];
	td.mb.nmvcost_hp[1]		= &nmvcosts_hp[1][MV_MAX];
	td.mb.nmvsadcost_hp[0]	= &nmvsadcosts_hp[0][MV_MAX];
	td.mb.nmvsadcost_hp[1]	= &nmvsadcosts_hp[1][MV_MAX];
	cal_nmvsadcosts_hp(td.mb.nmvsadcost_hp);

	allow_encode_breakout	= ENCODE_BREAKOUT_ENABLED;

	if (oxcf.pass == 1) {
		init_first_pass();
	} else if (oxcf.pass == 2) {
		const size_t packet_sz	= sizeof(FIRSTPASS_STATS);
		const int packets		= (int)(oxcf.two_pass_stats_in.sz / packet_sz);

		if (svc.number_spatial_layers > 1 || svc.number_temporal_layers > 1) {
			FIRSTPASS_STATS *const stats = oxcf.two_pass_stats_in.buf;
			FIRSTPASS_STATS *stats_copy[VPX_SS_MAX_LAYERS] = { 0 };
			for (int i = 0; i < oxcf.ss_number_layers; ++i) {
				FIRSTPASS_STATS *const last_packet_for_layer = &stats[packets - oxcf.ss_number_layers + i];
				const int layer_id = (int)last_packet_for_layer->spatial_layer_id;
				const int packets_in_layer = (int)last_packet_for_layer->count + 1;
				if (layer_id >= 0 && layer_id < oxcf.ss_number_layers) {
					LAYER_CONTEXT *const lc = &svc.layer_context[layer_id];

					vpx_free(lc->rc_twopass_stats_in.buf);

					lc->rc_twopass_stats_in.sz = packets_in_layer * packet_sz;
					CHECK_MEM_ERROR(cm, lc->rc_twopass_stats_in.buf, vpx_malloc(lc->rc_twopass_stats_in.sz));
					lc->twopass.stats_in_start = lc->rc_twopass_stats_in.buf;
					lc->twopass.stats_in = lc->twopass.stats_in_start;
					lc->twopass.stats_in_end = lc->twopass.stats_in_start + packets_in_layer - 1;
					stats_copy[layer_id] = lc->rc_twopass_stats_in.buf;
				}
			}

			for (int i = 0; i < packets; ++i) {
				const int layer_id = (int)stats[i].spatial_layer_id;
				if (layer_id >= 0 && layer_id < oxcf.ss_number_layers && stats_copy[layer_id] != NULL) {
					*stats_copy[layer_id] = stats[i];
					++stats_copy[layer_id];
				}
			}

			init_second_pass_spatial_svc();
		} else {
		#if CONFIG_FP_MB_STATS
			if (use_fp_mb_stats) {
				const size_t psz = common.MBs * sizeof(uint8);
				const int ps = (int)(oxcf.firstpass_mb_stats_in.sz / psz);

				twopass.firstpass_mb_stats.mb_stats_start	= oxcf.firstpass_mb_stats_in.buf;
				twopass.firstpass_mb_stats.mb_stats_end		= twopass.firstpass_mb_stats.mb_stats_start + (ps - 1) * common.MBs * sizeof(uint8);
			}
		#endif

			twopass.stats_in_start	= oxcf.two_pass_stats_in.buf;
			twopass.stats_in		= twopass.stats_in_start;
			twopass.stats_in_end	= &twopass.stats_in[packets - 1];

			vp9_init_second_pass();
		}
	}

	set_speed_features_framesize_independent();
	set_speed_features_framesize_dependent();

	// Allocate memory to store variances for a frame.
	source_diff_var = calloc(MBs, sizeof(diff));
	source_var_thresh = 0;
	frames_till_next_var_check = 0;

	// init_quantizer() is first called here. Add check in vp9_frame_init_quantizer() so that vp9_init_quantizer is only called later when needed. This will avoid unnecessary calls of vp9_init_quantizer() for every frame.
	init_quantizer();
	loop_filter_init();
}

Encoder::~Encoder() {
	for (int i = 0; i < sizeof(mbgraph_stats) / sizeof(mbgraph_stats[0]); ++i)
		vpx_free(mbgraph_stats[i].mb_stats);

#if CONFIG_FP_MB_STATS
	if (use_fp_mb_stats) {
		vpx_free(twopass.frame_mb_stats_buf);
		twopass.frame_mb_stats_buf = NULL;
	}
#endif

	remove_common(cm);
	free_ref_frame_buffers(buffer_pool);
#if CONFIG_VP9_POSTPROC
	free_postproc_buffers(cm);
#endif
}

int Encoder::use_as_reference(int ref_frame_flags) {
	if (ref_frame_flags > 7)
		return -1;

	ref_frame_flags = ref_frame_flags;
	return 0;
}

void Encoder::update_reference(int ref_frame_flags) {
	ext_refresh_golden_frame	= !!(ref_frame_flags & VP9_GOLD_FLAG);
	ext_refresh_alt_ref_frame	= !!(ref_frame_flags & VP9_ALT_FLAG);
	ext_refresh_last_frame		= !!(ref_frame_flags & VP9_LAST_FLAG);
	ext_refresh_frame_flags_pending = true;
}

static FrameBuffer *get_ Encoder::ref_frame_buffer(
	VP9_REFFRAME ref_frame_flag) {
	MV_REFERENCE_FRAME ref_frame = NONE;
	if (ref_frame_flag == VP9_LAST_FLAG)
		ref_frame = REFFRAME_LAST;
	else if (ref_frame_flag == VP9_GOLD_FLAG)
		ref_frame = REFFRAME_GOLDEN;
	else if (ref_frame_flag == VP9_ALT_FLAG)
		ref_frame = REFFRAME_ALTREF;

	return ref_frame == NONE ? NULL : get_ref_frame_buffer(ref_frame);
}

int Encoder::copy_reference_enc(VP9_REFFRAME ref_frame_flag,
	FrameBuffer *sd) {
	FrameBuffer *cfg = get_vp9_ref_frame_buffer(ref_frame_flag);
	if (cfg) {
		vp8_yv12_copy_frame(cfg, sd);
		return 0;
	} else {
		return -1;
	}
}

int Encoder::set_reference_enc(VP9_REFFRAME ref_frame_flag,
	FrameBuffer *sd) {
	FrameBuffer *cfg = get_vp9_ref_frame_buffer(ref_frame_flag);
	if (cfg) {
		vp8_yv12_copy_frame(sd, cfg);
		return 0;
	} else {
		return -1;
	}
}

int Encoder::update_entropy(bool update) {
	ext_refresh_frame_context			= update;
	ext_refresh_frame_context_pending	= true;
	return 0;
}


bool Encoder::scale_down(int q) {
	return rc->frame_size_selector == UNSCALED && q >= rc->rf_level_maxq[twopass.gf_group->rf_level[twopass.gf_group->index]]
		&& rc->projected_frame_size > int(rate_thresh_mult[SCALE_STEP1] * max(rc.this_frame_target, rc.avg_frame_bandwidth));
}

// Function to test for conditions that indicate we should loop
// back and recode a frame.
bool Encoder::recode_loop_test(int high_limit, int low_limit, int q, int maxq, int minq) {
	const bool frame_is_kfgfarf = frame_is_kf_gf_arf();
	bool	force_recode = false;

	if ((rc.projected_frame_size >= rc.max_frame_bandwidth) || (sf.recode_loop == ALLOW_RECODE) || (frame_is_kfgfarf && (sf.recode_loop == ALLOW_RECODE_KFARFGF))) {
		if (frame_is_kfgfarf && oxcf.resize_mode == RESIZE_DYNAMIC && scale_down(q)) {
			// Code this group at a lower resolution.
			return resize_pending = true;
		}

		// TODO(agrange) high_limit could be greater than the scale-down threshold.
		if ((rc.projected_frame_size > high_limit && q < maxq) || (rc.projected_frame_size < low_limit && q > minq)) {
			force_recode = true;
		} else if (oxcf.rc_mode == RC_CQ) {
			// Deal with frame undershoot and whether or not we are below the automatically set cq level.
			if (q > oxcf.cq_level && rc.projected_frame_size < ((rc.this_frame_target * 7) >> 3)) {
				force_recode = true;
			}
		}
	}
	return force_recode;
}

void Encoder::update_reference_frames() {
	// At this point the new frame has been encoded.
	// If any buffer copy / swapping is signaled it should be done here.
	if (frame_type == KEY_FRAME) {
		ref_cnt_fb(pool->frame_bufs, &ref_frame_map[gld_fb_idx], new_fb_idx);
		ref_cnt_fb(pool->frame_bufs, &ref_frame_map[alt_fb_idx], new_fb_idx);
	} else if (preserve_existing_gf()) {
		// We have decided to preserve the previously existing golden frame as our new ARF frame.
		// However, in the short term in function vp9_bitstream.c::get_refresh_mask() we left it in the GF slot and, if we're updating the GF with the current decoded frame, we save it to the ARF slot instead
		// We now have to update the ARF with the current frame and swap gld_fb_idx and alt_fb_idx so that, overall, we've stored the old GF in the new ARF slot and, if we're updating the GF, the current frame becomes the new GF.
		ref_cnt_fb(pool->frame_bufs, &ref_frame_map[alt_fb_idx], new_fb_idx);
		swap(alt_fb_idx, gld_fb_idx);

		if (is_two_pass_svc()) {
			svc.layer_context[0].gold_ref_idx	= gld_fb_idx;
			svc.layer_context[0].alt_ref_idx	= alt_fb_idx;
		}
	} else { /* For non key/golden frames */
		if (refresh_alt_ref_frame) {
			int arf_idx = alt_fb_idx;
			if (oxcf.pass == 2 && multi_arf_allowed)
				arf_idx = twopass.gf_group.arf_update_idx[twopass.gf_group.index];

			ref_cnt_fb(pool->frame_bufs, &ref_frame_map[arf_idx], new_fb_idx);
			memcpy(interp_filter_selected[REFFRAME_ALTREF], interp_filter_selected[0], sizeof(interp_filter_selected[0]));
		}

		if (refresh_golden_frame) {
			ref_cnt_fb(pool->frame_bufs, &ref_frame_map[gld_fb_idx], new_fb_idx);
			if (!rc.is_src_frame_alt_ref)
				memcpy(interp_filter_selected[REFFRAME_GOLDEN], interp_filter_selected[0], sizeof(interp_filter_selected[0]));
			else
				memcpy(interp_filter_selected[REFFRAME_GOLDEN], interp_filter_selected[REFFRAME_ALTREF], sizeof(interp_filter_selected[REFFRAME_ALTREF]));
		}
	}

	if (refresh_last_frame) {
		ref_cnt_fb(pool->frame_bufs, &ref_frame_map[lst_fb_idx], new_fb_idx);
		if (!rc.is_src_frame_alt_ref)
			memcpy(interp_filter_selected[LAST_FRAME], interp_filter_selected[0], sizeof(interp_filter_selected[0]));
	}
#if CONFIG_VP9_TEMPORAL_DENOISING
	if (oxcf.noise_sensitivity > 0) {
		vp9_denoiser_update_frame_info(&denoiser,
			*Source,
			common.frame_type,
			refresh_alt_ref_frame,
			refresh_golden_frame,
			refresh_last_frame,
			resize_pending);
	}
#endif
	if (is_one_pass_cbr_svc()) {
		// Keep track of frame index for each reference frame.
		if (frame_type == KEY_FRAME) {
			svc.ref_frame_index[lst_fb_idx] = svc.current_superframe;
			svc.ref_frame_index[gld_fb_idx] = svc.current_superframe;
			svc.ref_frame_index[alt_fb_idx] = svc.current_superframe;
		} else {
			if (refresh_last_frame)
				svc.ref_frame_index[lst_fb_idx] = svc.current_superframe;
			if (refresh_golden_frame)
				svc.ref_frame_index[gld_fb_idx] = svc.current_superframe;
			if (refresh_alt_ref_frame)
				svc.ref_frame_index[alt_fb_idx] = svc.current_superframe;
		}
	}
}

void Encoder::loopfilter_frame() {
	TileDecoder *xd = &td.mb;

	if (xd->lossless) {
		lf->filter_level = 0;
		lf->last_filt_level = 0;
	} else {
		struct vpx_usec_timer timer;

		vpx_usec_timer_start(&timer);

		if (!rc.is_src_frame_alt_ref) {
			if ((common.frame_type == KEY_FRAME) &&
				(!rc.this_key_frame_forced)) {
				lf->last_filt_level = 0;
			}
			vp9_pick_filter_level(Source, cpi, sf.lpf_pick);
			lf->last_filt_level = lf->filter_level;
		} else {
			lf->filter_level = 0;
		}

		vpx_usec_timer_mark(&timer);
		time_pick_lpf += vpx_usec_timer_elapsed(&timer);
	}

	if (lf->filter_level > 0) {
		vp9_build_mask_frame(cm, lf->filter_level, 0);

		if (num_workers > 1)
			vp9_loop_filter_frame_mt(frame_to_show, cm, xd->plane,
			lf->filter_level, 0, 0,
			workers, num_workers,
			&lf_row_sync);
		else
			vp9_loop_filter_frame(frame_to_show, cm, xd, lf->filter_level, 0, 0);
	}

	vpx_extend_frame_inner_borders(frame_to_show);
}

static inline void alloc_frame_mvs(const VP9_COMMON *cm, int buffer_idx) {
	RefCntBuffer *const new_fb_ptr = &buffer_pool->frame_bufs[buffer_idx];
	if (new_fb_ptr->mvs == NULL || new_fb_ptr->mi_rows < mi_rows || new_fb_ptr->mi_cols < mi_cols) {
		vpx_free(new_fb_ptr->mvs);
		new_fb_ptr->mvs = (MV_REF *)vpx_calloc(mi_rows * mi_cols, sizeof(*new_fb_ptr->mvs));
		new_fb_ptr->mi_rows = mi_rows;
		new_fb_ptr->mi_cols = mi_cols;
	}
}

void Encoder::scale_references() {
	MV_REFERENCE_FRAME ref_frame;
	const VP9_REFFRAME ref_mask[3] = { VP9_LAST_FLAG, VP9_GOLD_FLAG, VP9_ALT_FLAG };

	for (ref_frame = REFFRAME_LAST; ref_frame <= REFFRAME_ALTREF; ++ref_frame) {
		// Need to convert from VP9_REFFRAME to index into ref_mask (subtract 1).
		if (ref_frame_flags & ref_mask[ref_frame - 1]) {
			BufferPool *const pool = buffer_pool;
			const FrameBuffer *const ref = get_ref_frame_buffer(ref_frame);

			if (ref == NULL) {
				scaled_ref_idx[ref_frame - 1] = INVALID_IDX;
				continue;
			}

			if (ref->y_crop_width != width || ref->y_crop_height != height) {
				RefCntBuffer *new_fb_ptr = NULL;
				int force_scaling = 0;
				int new_fb = scaled_ref_idx[ref_frame - 1];
				if (new_fb == INVALID_IDX) {
					new_fb = get_free_fb(cm);
					force_scaling = true;
				}
				if (new_fb == INVALID_IDX)
					return;
				new_fb_ptr = &pool->frame_bufs[new_fb];
				if (force_scaling ||
					new_fb_ptr->buf.y_crop_width != width ||
					new_fb_ptr->buf.y_crop_height != height) {
					vpx_realloc_frame_buffer(&new_fb_ptr->buf,
						width, height,
						subsampling_x, subsampling_y,
						ENC_BORDER, byte_alignment,
						NULL, NULL, NULL);
					vp9_scale_and_extend_frame(ref, &new_fb_ptr->buf);
					scaled_ref_idx[ref_frame - 1] = new_fb;
					alloc_frame_mvs(cm, new_fb);
				}
			} else {
				const int buf_idx = get_ref_frame_buf_idx(ref_frame);
				RefCntBuffer *const buf = &pool->frame_bufs[buf_idx];
				buf->buf.y_crop_width = ref->y_crop_width;
				buf->buf.y_crop_height = ref->y_crop_height;
				scaled_ref_idx[ref_frame - 1] = buf_idx;
				++buf->ref_count;
			}
			} else {
			if (oxcf.pass != 0 || use_svc)
				scaled_ref_idx[ref_frame - 1] = INVALID_IDX;
		}
		}
	}

void Encoder::release_scaled_references() {
	if (oxcf.pass == 0 && !use_svc) {
		// Only release scaled references under certain conditions:
		// if reference will be updated, or if scaled reference has same resolution.
		bool	refresh[3];
		refresh[0] = refresh_last_frame;
		refresh[1] = refresh_golden_frame;
		refresh[2] = refresh_alt_ref_frame;
		for (int i = REFFRAME_LAST; i <= REFFRAME_ALTREF; ++i) {
			const int idx = scaled_ref_idx[i - 1];
			RefCntBuffer *const buf = idx != INVALID_IDX ? &buffer_pool->frame_bufs[idx] : NULL;
			const FrameBuffer *const ref = get_ref_frame_buffer(i);
			if (buf != NULL && (refresh[i - 1] || (buf->buf.y_crop_width == ref->y_crop_width && buf->buf.y_crop_height == ref->y_crop_height))) {
				--buf->ref_count;
				scaled_ref_idx[i - 1] = INVALID_IDX;
			}
		}
	} else {
		for (int i = 0; i < MAX_REF_FRAMES; ++i) {
			const int idx = scaled_ref_idx[i];
			RefCntBuffer *const buf = idx != INVALID_IDX ? &buffer_pool->frame_bufs[idx] : NULL;
			if (buf != NULL) {
				--buf->ref_count;
				scaled_ref_idx[i] = INVALID_IDX;
			}
		}
	}
}

static void full_to_model_count(uint32 *model_count, uint32 *full_count) {
	model_count[ZERO_TOKEN] = full_count[ZERO_TOKEN];
	model_count[ONE_TOKEN] = full_count[ONE_TOKEN];
	model_count[TWO_TOKEN] = full_count[TWO_TOKEN];
	for (int n = THREE_TOKEN; n < EOB_TOKEN; ++n)
		model_count[TWO_TOKEN] += full_count[n];
	model_count[EOB_MODEL_TOKEN] = full_count[EOB_TOKEN];
}

static void full_to_model_counts(vp9_coeff_count_model *model_count, vp9_coeff_count *full_count) {
	for (int i = 0; i < PLANE_TYPES; ++i)
		for (int j = 0; j < REF_TYPES; ++j)
			for (int k = 0; k < COEF_BANDS; ++k)
				for (int l = 0; l < BAND_COEFF_CONTEXTS(k); ++l)
					full_to_model_count(model_count[i][j][k][l], full_count[i][j][k][l]);
}

void Encoder::set_mv_search_params() {
	const uint32 max_mv_def = min(width, height);

	// Default based on max resolution.
	mv_step_param = init_search_range(max_mv_def);

	if (sf.mv.auto_mv_step_size) {
		if (frame_is_intra_only(cm)) {
			// Initialize max_mv_magnitude for use in the first INTER frame
			// after a key/intra-only frame.
			max_mv_magnitude = max_mv_def;
		} else {
			if (show_frame) {
				// Allow mv_steps to correspond to twice the max mv magnitude found
				// in the previous frame, capped by the default max_mv_magnitude based
				// on resolution.
				mv_step_param = vp9_init_search_range(
					min(max_mv_def, 2 * max_mv_magnitude));
			}
			max_mv_magnitude = 0;
		}
	}
}

static void Encoder::set_size_independent_vars() {
	sf.set_framesize_independent();

	if (oxcf.mode == REALTIME)
		sf.set_rt_speed_feature(this, sf, oxcf->speed, oxcf->content);
	else if (oxcf.mode == GOOD) {
		sf.set_good(oxcf.speed, frame_is_boosted(),
			twopass.fr_content_type == FC_GRAPHICS_ANIMATION || internal_image_edge(),
			!frame_is_intra_only()
		);
		// Reference masking is not supported in dynamic scaling mode.
		sf.reference_masking = oxcf.resize_mode != RESIZE_DYNAMIC;
	}

	full_search_sad		= vp9_full_search_sad;
	diamond_search_sad	= vp9_diamond_search_sad;

	sf.set_meshes(oxcf.mode == BEST ? -1 : oxcf.speed, twopass.fr_content_type == FC_GRAPHICS_ANIMATION);
	sf.optimize_coefficients = oxcf.pass == 2 && !oxcf.is_lossless_requested();
	if (oxcf.pass == 0)
		// No recode for 1 pass.
		sf.recode_loop = DISALLOW_RECODE;

	find_fractional_mv_step = 
		sf.mv.subpel_search_method == SUBPEL_TREE					? vp9_find_best_sub_pixel_tree
	:	sf.mv.subpel_search_method == SUBPEL_TREE_PRUNED			? vp9_find_best_sub_pixel_tree_pruned
	:	sf.mv.subpel_search_method == SUBPEL_TREE_PRUNED_MORE		? vp9_find_best_sub_pixel_tree_pruned_more
	:	sf.mv.subpel_search_method == SUBPEL_TREE_PRUNED_EVENMORE	? vp9_find_best_sub_pixel_tree_pruned_evenmore
	: 0;

	x->optimize				= sf.optimize_coefficients;
	x->min_partition_size	= sf.default_min_partition_size;
	x->max_partition_size	= sf.default_max_partition_size;

	if (!oxcf.frame_periodic_boost)
		sf.max_delta_qindex = 0;

	rd.set_speed_thresholds(oxcf.mode == BEST, sf->adaptive_rd_thresh);
	interp_filter = sf.default_interp_filter;
}

#define STATIC_MOTION_THRESH 95

void Encoder::set_size_dependent_vars(int *pq, int *bottom_index, int *top_index) {
	// Setup variables that depend on the dimensions of the frame.
	if (oxcf->mode == REALTIME) {
		sf.set_framesize_dependent(width, height, oxcf->speed, show_frame);
	} else if (oxcf->mode == GOOD) {
		sf.set_good_framesize_dependent(width, height, oxcf->speed, quant.base_index, show_frame,
			oxcf.speed >= 1 && oxcf.pass == 2 && (twopass.fr_content_type == FC_GRAPHICS_ANIMATION || internal_image_edge())
		);
		if ((speed >= 1) && (oxcf.pass == 2) && ((twopass.fr_content_type == FC_GRAPHICS_ANIMATION) || (internal_image_edge())))
			disable_split_mask = DISABLE_COMPOUND_SPLIT;
	}

	if (sf.disable_split_mask == DISABLE_ALL_SPLIT)
		sf.adaptive_pred_interp_filter = 0;

	if (encode_breakout && oxcf.mode == REALTIME && sf.encode_breakout_thresh > encode_breakout)
		encode_breakout = sf.encode_breakout_thresh;

	// Check for masked out split cases.
	for (int i = 0; i < MAX_REFS; ++i) {
		if (sf.disable_split_mask & (1 << i))
			rd.thresh_mult_sub8x8[i] = INT_MAX;
	}
}
	// Decide q and q bounds.

	FRAME_TYPE	ft		= frame_is_intra_only() || is_upper_layer_key_frame()		? INTRA_FRAME
		: frame_type == INTER_FRAME && curr_frame < 5 * svc.number_temporal_layers	? KEYLIKE_FRAME
		: !use_svc && (refresh_golden_frame || refresh_alt_ref_frame)				? INTER_FRAME
		: ALTREF_FRAME;

	if (oxcf.pass) {
		int	active_worst_quality	= twopass.active_worst_quality;
		int active_best_quality		= rc.pick_q_two_pass(oxcf.rc_mode, oxcf.cq_level, frame_type, active_worst_quality, cs.bit_depth, (width * height <= 352 * 288 ? 0.75f : 1) + 0.05f - twopass.kf_zeromotion_pct / 100.f, twopass.last_kfgroup_zeromotion_pct >= STATIC_MOTION_THRESH, refresh_alt_ref_frame);

		if (!rc.is_src_frame_alt_ref && frame_type == INTER_FRAME && twopass.gf_group.rf_level[twopass.gf_group.index] == GF_ARF_LOW)
			active_best_quality = (active_best_quality + get_active_cq_level(oxcf.rc_mode, oxcf.cq_level) + 1) / 2 : q;

	} else {
		int active_worst_quality = oxcf.rc_mode == RC_CBR
			? rc.calc_active_worst_quality_one_pass_cbr(frame_type)
			: rc.calc_active_worst_quality_one_pass_vbr(frame_type);

		int active_best_quality = rc.pick_q_one_pass(oxcf.rc_mode, oxcf.cq_level, frame_type, active_worst_quality, cs.bit_depth, curr_frame, width * height <= 352 * 288 ? 0.75f : 1, refresh_alt_ref_frame);

		// Clip the active best and worst quality values to limits
		active_best_quality		= clamp(active_best_quality, rc.best_quality, rc.worst_quality);
		active_worst_quality	= clamp(active_worst_quality, active_best_quality, rc.worst_quality);

	#if LIMIT_QRANGE_FOR_ALTREF_AND_KEY
		// Limit Q range for the adaptive loop.
		if (frame_type == KEY_FRAME && !rc.this_key_frame_forced && curr_frame != 0)
			active_worst_quality = max(active_worst_quality + rc.compute_qdelta_by_rate(frame_type, active_worst_quality, 2.0f, cs.bit_depth), active_best_quality);
		else if (!is_src_frame_alt_ref && frame_type == INTER_FRAME)
			active_worst_quality += rc.compute_qdelta_by_rate(frame_type, active_worst_quality, 1.75f, cs.bit_depth);
		active_worst_quality = max(active_worst_quality, active_best_quality);
	#endif
		*top_index		= active_worst_quality;
		*bottom_index	= active_best_quality;
	}

//	if (twopass.last_kfgroup_zeromotion_pct >= STATIC_MOTION_THRESH) {

	int	q;
	if (oxcf.rc_mode == RC_Q) {
		q = rc.active_best_quality;
	} else if (ft == KEYLIKE_FRAME && rc.this_key_frame_forced) {
		q = oxcf.pass == 0 || twopass.last_kfgroup_zeromotion_pct < STATIC_MOTION_THRESH ? rc.last_boosted_qindex : min(last_kf_qindex, last_boosted_qindex);
	} else {
		q = regulate_q(this_frame_target, active_best_quality, active_worst_quality);
		if (q > *top_index) {
			// Special case when we are targeting the max allowed rate
			if (this_frame_target >= max_frame_bandwidth)
				*top_index = q;
			else
				q = *top_index;
		}
	}

	if (sf.use_nonrd_pick_mode) {
		if (sf.force_frame_boost)
			q -= sf.max_delta_qindex;
		if (q < *bottom_index)
			*bottom_index = q;
		else if (q > *top_index)
			*top_index = q;
	}
	*pq = q;

	if (!frame_is_intra_only())
		set_high_precision_mv(q < HIGH_PRECISION_MV_QTHRESH);

	// Configure experimental use of segmentation for enhanced coding of static regions if indicated.
	// Only allowed in the second pass of a two pass encode, as it requires lagged coding, and if the relevant speed feature flag is set.
	if (oxcf.pass == 2 && sf.static_segmentation)
		configure_static_seg_features();

#if CONFIG_VP9_POSTPROC && !(CONFIG_VP9_TEMPORAL_DENOISING)
	if (oxcf.noise_sensitivity > 0) {
		int l = 0;
		switch (oxcf.noise_sensitivity) {
			case 1:	l = 20;	break;
			case 2:	l = 40;	break;
			case 3:	l = 60;	break;
			case 4:			 
			case 5:	l = 100; break;
			case 6:	l = 150; break;
		}
		vp9_denoise(Source, Source, l);
	}
#endif
}

#if CONFIG_VP9_TEMPORAL_DENOISING
static void setup_denoiser_buffer(VP9_COMP *cpi) {
	VP9_COMMON *const cm = &common;
	if (oxcf.noise_sensitivity > 0 &&
		!denoiser.frame_buffer_initialized) {
		vp9_denoiser_alloc(&(denoiser), width, height,
			subsampling_x, subsampling_y,
			ENC_BORDER);
	}
}
#endif

static void init_motion_estimation(VP9_COMP *cpi) {
	int y_stride = scaled_source.y_stride;

	if (sf.mv.search_method == NSTEP) {
		vp9_init3smotion_compensation(&ss_cfg, y_stride);
	} else if (sf.mv.search_method == DIAMOND) {
		vp9_init_dsmotion_compensation(&ss_cfg, y_stride);
	}
}

void Encoder::set_frame_size() {
	MACROBLOCKD &xd = td.mb;

	if (oxcf.pass == 2 && oxcf.rc_mode == RC_VBR && ((oxcf.resize_mode == RESIZE_FIXED && curr_frame == 0) || (oxcf.resize_mode == RESIZE_DYNAMIC && resize_pending))) {
		calculate_coded_size(cpi, &oxcf.scaled_frame_width, &oxcf.scaled_frame_height);
		// There has been a change in frame size.
		vp9_set_size_literal(cpi, oxcf.scaled_frame_width, oxcf.scaled_frame_height);
	}

	if (oxcf.pass == 0 && oxcf.rc_mode == RC_CBR && !use_svc && oxcf.resize_mode == RESIZE_DYNAMIC && resize_pending) {
		oxcf.scaled_frame_width		= (oxcf.width * resize_scale_num) / resize_scale_den;
		oxcf.scaled_frame_height	= (oxcf.height * resize_scale_num) / resize_scale_den;
		// There has been a change in frame size.
		vp9_set_size_literal(cpi, oxcf.scaled_frame_width, oxcf.scaled_frame_height);

		// TODO(agrange) Scale max_mv_magnitude if frame-size has changed.
		set_mv_search_params(cpi);

		vp9_noise_estimate_init(&noise_estimate, width, height);
	#if CONFIG_VP9_TEMPORAL_DENOISING
		// Reset the denoiser on the resized frame.
		if (oxcf.noise_sensitivity > 0) {
			vp9_denoiser_free(&(denoiser));
			setup_denoiser_buffer(cpi);
			// Dynamic resize is only triggered for non-SVC, so we can force
			// golden frame update here as temporary fix to denoiser.
			refresh_golden_frame = 1;
		}
	#endif
	}

	if (oxcf.pass == 2 && (!use_svc || (is_two_pass_svc(cpi) && svc.encode_empty_frame_state != ENCODING))) {
		int target_rate = frame_type == KEY_FRAME
			? rc.clamp_iframe_target_size(oxcf, rc.base_frame_target)
			: rc.clamp_pframe_target_size(oxcf, rc.base_frame_target);

		// Correction to rate target based on prior over or under shoot.
		if (oxcf.rc_mode == RC_VBR || oxcf.rc_mode == RC_CQ) {
			static const float VBR_PCT_ADJUSTMENT_LIMIT = 0.5f;
			float	position_factor	= twopass.total_stats.count ? sqrt((float)curr_frame / twopass.total_stats.count) : 1;
			target_rate = rc.adjust_target_rate(target_rate, int(position_factor * target_rate * VBR_PCT_ADJUSTMENT_LIMIT), !frame_is_kf_gf_arf());
		}
		rc.set_frame_target(oxcf, target_rate, MBs);
	}

	alloc_frame_mvs(cm, new_fb_idx);

	// Reset the frame pointers to the current frame size.
	vpx_realloc_frame_buffer(get_frame_new_buffer(cm),
		width, height,
		subsampling_x, subsampling_y,
		ENC_BORDER, byte_alignment,
		NULL, NULL, NULL);

	alloc_util_frame_buffers(cpi);
	init_motion_estimation(cpi);

	for (int ref_frame = REFFRAME_LAST; ref_frame <= REFFRAME_ALTREF; ++ref_frame) {
		RefBuffer *const ref_buf = &frame_refs[ref_frame - 1];
		const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);

		ref_buf->idx = buf_idx;

		if (buf_idx != INVALID_IDX) {
			FrameBuffer *const buf = &buffer_pool->frame_bufs[buf_idx].buf;
			ref_buf->buf	= buf;

			vp9_setup_scale_factors_for_frame(&ref_buf->sf, buf->y_crop_width, buf->y_crop_height, width, height);
			if (vp9_is_scaled(&ref_buf->sf))
				vpx_extend_frame_borders(buf);
		} else {
			ref_buf->buf = NULL;
		}
	}

	set_ref_ptrs(cm, xd, LAST_FRAME, LAST_FRAME);
}

static void encode_without_recode_loop(VP9_COMP *cpi,
	size_t *size,
	uint8 *dest) {
	VP9_COMMON *const cm = &common;
	int q = 0, bottom_index = 0, top_index = 0;  // Dummy variables.

	set_frame_size(cpi);
	Source = vp9_scale_if_required(cm,
		un_scaled_source,
		&scaled_source,
		(oxcf.pass == 0));

	// Avoid scaling last_source unless its needed.
	// Last source is currently only used for screen-content mode,
	// if partition_search_type == SOURCE_VAR_BASED_PARTITION, or if noise
	// estimation is enabled.
	if (unscaled_last_source != NULL &&
		(oxcf.content == CONTENT_SCREEN ||
		sf.partition_search_type == SOURCE_VAR_BASED_PARTITION ||
		noise_estimate.enabled))
		Last_Source = vp9_scale_if_required(cm,
		unscaled_last_source,
		&scaled_last_source,
		(oxcf.pass == 0));
	vp9_update_noise_estimate(cpi);

	if (oxcf.pass == 0 &&
		oxcf.rc_mode == RC_CBR &&
		resize_state == RESIZE_ORIG &&
		frame_type != KEY_FRAME &&
		oxcf.content == CONTENT_SCREEN)
		vp9_avg_source_sad(cpi);

	// TODO(wonkap/marpan): For 1 pass SVC, since only ZERMOV is allowed for
	// upsampled reference frame (i.e, svc->force_zero_mode_spatial_ref = 0),
	// we should be able to avoid this frame-level upsampling.
	// Keeping it for now as there is an asan error in the multi-threaded SVC
	// rate control test if this upsampling is removed.
	if (frame_is_intra_only(cm) == 0) {
		vp9_scale_references(cpi);
	}

	set_size_independent_vars();
	set_size_dependent_vars(cpi, &q, &bottom_index, &top_index);

	if (oxcf.speed >= 5 &&
		oxcf.pass == 0 &&
		oxcf.rc_mode == RC_CBR &&
		oxcf.content != CONTENT_SCREEN &&
		oxcf.aq_mode == AQ_CYCLIC_REFRESH) {
		use_skin_detection = true;
	}

	vp9_set_quantizer(cm, q);
	vp9_set_variance_partition_thresholds(cpi, q);

	setup_frame(cpi);

	suppress_active_map(cpi);
	// Variance adaptive and in frame q adjustment experiments are mutually
	// exclusive.
	if (oxcf.aq_mode == AQ_VARIANCE) {
		vp9_vaq_frame_setup(cpi);
	} else if (oxcf.aq_mode == AQ_EQUATOR360) {
		vp9_360aq_frame_setup(cpi);
	} else if (oxcf.aq_mode == AQ_COMPLEXITY) {
		vp9_setup_in_frame_q_adj(cpi);
	} else if (oxcf.aq_mode == AQ_CYCLIC_REFRESH) {
		vp9_cyclic_refresh_setup(cpi);
	}
	apply_active_map(cpi);

	// transform / motion compensation build reconstruction frame
	vp9_encode_frame(cpi);

	// Check if we should drop this frame because of high overshoot.
	// Only for frames where high temporal-source sad is detected.
	if (oxcf.pass == 0 && oxcf.rc_mode == RC_CBR && resize_state == RESIZE_ORIG && frame_type != KEY_FRAME && oxcf.content == CONTENT_SCREEN && rc.high_source_sad) {
		int frame_size = 0;
		// Get an estimate of the encoded frame size.
		save_coding_context(cpi);
		vp9_pack_bitstream(cpi, dest, size);
		restore_coding_context(cpi);
		frame_size = (int)(*size) << 3;
		// Check if encoded frame will overshoot too much, and if so, set the q and
		// adjust some rate control parameters, and return to re-encode the frame.

		// Test if encoded frame will significantly overshoot the target bitrate, and if so, set the QP, reset/adjust some rate control parameters, and return 1.

		if (float rate_correction_factor = rc.encodedframe_overshoot(frame_size, q, quant.base_index, MBs, cs.bit_depth)) {
			// For temporal layers, reset the rate control parametes across all temporal layers.
			if (use_svc) {
				for (int i = 0; i < svc.number_temporal_layers; ++i) {
					const int layer = svc.index(i);
					RATE_CONTROL	&lrc	= svc.layer_context[layer].rc;
					lrc.avg_frame_qindex[INTER_FRAME] = *q;
					lrc.buffer_level		= rc.optimal_buffer_level;
					lrc.bits_off_target		= rc.optimal_buffer_level;
					lrc.rc_1_frame			= 0;
					lrc.rc_2_frame			= 0;
					lrc.rate_correction_factors[INTER_NORMAL] = rate_correction_factor;
				}
			}
			vp9_set_quantizer(cm, q);
			vp9_set_variance_partition_thresholds(cpi, q);
			suppress_active_map(cpi);
			// Turn-off cyclic refresh for re-encoded frame.
			if (oxcf.aq_mode == AQ_CYCLIC_REFRESH) {
				uint8 *const seg_map = segmentation_map;
				memset(seg_map, 0, mi_rows * mi_cols);
				vp9_disable_segmentation(&seg);
			}
			apply_active_map(cpi);
			vp9_encode_frame(cpi);
		}
	}

	// Update some stats from cyclic refresh, and check if we should not update
	// golden reference, for non-SVC 1 pass CBR.
	if (oxcf.aq_mode == AQ_CYCLIC_REFRESH &&
		frame_type != KEY_FRAME &&
		!use_svc &&
		ext_refresh_frame_flags_pending == 0 &&
		(oxcf.pass == 0 && oxcf.rc_mode == RC_CBR))
		vp9_cyclic_refresh_check_golden_update(cpi);

	// Update the skip mb flag probabilities based on the distribution
	// seen in the last encoder iteration.
	// update_base_skip_probs(cpi);
}

void Encoder::encode_with_recode_loop(VP9_COMP *cpi, size_t *size, uint8 *dest) {
	int bottom_index, top_index;
	int loop_count = 0;
	int loop_at_this_size = 0;
	int loop = 0;
	bool overshoot_seen = false;
	bool undershoot_seen = false;
	int frame_over_shoot_limit;
	int frame_under_shoot_limit;
	int q = 0, q_low = 0, q_high = 0;

	set_size_independent_vars();

	do {
		set_frame_size(cpi);

		if (loop_count == 0 || resize_pending) {
			set_size_dependent_vars(cpi, &q, &bottom_index, &top_index);

			// TODO(agrange) Scale max_mv_magnitude if frame-size has changed.
			set_mv_search_params(cpi);

			// Reset the loop state for new frame size.
			overshoot_seen = undershoot_seen = false;

			// Reconfiguration for change in frame size has concluded.
			resize_pending = false;

			q_low	= bottom_index;
			q_high	= top_index;

			loop_at_this_size = 0;
		}

		// Decide frame size bounds first time through.
		if (loop_count == 0)
			compute_frame_size_bounds(rc.this_frame_target, &frame_under_shoot_limit, &frame_over_shoot_limit);

		Source = vp9_scale_if_required(cm, un_scaled_source, &scaled_source, oxcf.pass == 0);

		if (unscaled_last_source != NULL)
			Last_Source = vp9_scale_if_required(cm, unscaled_last_source, &scaled_last_source, oxcf.pass == 0);

		if (frame_is_intra_only(cm) == 0) {
			if (loop_count > 0)
				release_scaled_references(cpi);
			vp9_scale_references(cpi);
		}

		vp9_set_quantizer(cm, q);

		if (loop_count == 0)
			setup_frame(cpi);

		// Variance adaptive and in frame q adjustment experiments are mutually
		// exclusive.
		if (oxcf.aq_mode == AQ_VARIANCE) {
			vp9_vaq_frame_setup(cpi);
		} else if (oxcf.aq_mode == AQ_EQUATOR360) {
			vp9_360aq_frame_setup(cpi);
		} else if (oxcf.aq_mode == AQ_COMPLEXITY) {
			vp9_setup_in_frame_q_adj(cpi);
		}

		// transform / motion compensation build reconstruction frame
		vp9_encode_frame(cpi);

		// Update the skip mb flag probabilities based on the distribution seen in the last encoder iteration.
		// update_base_skip_probs(cpi);

		// Dummy pack of the bitstream using up to date stats to get an accurate estimate of output frame size to determine if we need to recode.
		if (sf.recode_loop >= ALLOW_RECODE_KFARFGF) {
			save_coding_context(cpi);
			if (!sf.use_nonrd_pick_mode)
				vp9_pack_bitstream(cpi, dest, size);

			rc->projected_frame_size = (int)(*size) << 3;
			restore_coding_context(cpi);

			if (frame_over_shoot_limit == 0)
				frame_over_shoot_limit = 1;
		}

		if (oxcf.rc_mode == RC_Q) {
			loop = 0;
		} else {
			if (frame_type == KEY_FRAME && rc->this_key_frame_forced && rc->projected_frame_size < rc->max_frame_bandwidth) {
				int		last_q = q;
				int64	high_err_target	= ambient_err;
				int64	low_err_target	= ambient_err >> 1;
				int64	kf_err			= max(PSNR::get_sse(Source->y_buffer, get_frame_new_buffer(cm)->y_buffer, Source->y.width, Source->y.height), 1);

				// The key frame is not good enough or we can afford to make it better without undue risk of popping.
				if ((kf_err > high_err_target && rc->projected_frame_size <= frame_over_shoot_limit) || (kf_err > low_err_target &&	rc->projected_frame_size <= frame_under_shoot_limit)) {
					// Lower q_high
					q_high = q > q_low ? q - 1 : q_low;

					// Adjust Q
					q = min((int)((q * high_err_target) / kf_err), (q_high + q_low) >> 1);
				} else if (kf_err < low_err_target &&
					rc->projected_frame_size >= frame_under_shoot_limit) {
					// The key frame is much better than the previous frame
					// Raise q_low
					q_low = q < q_high ? q + 1 : q_high;

					// Adjust Q
					q = min((int)((q * low_err_target) / kf_err), (q_high + q_low + 1) >> 1);
				}

				// Clamp Q to upper and lower limits:
				q = clamp(q, q_low, q_high);

				loop = q != last_q;
			} else if (recode_loop_test(cpi, frame_over_shoot_limit, frame_under_shoot_limit, q, max(q_high, top_index), bottom_index)) {
				// Is the projected frame size out of range and are we allowed to attempt to recode.
				int last_q = q;
				int retries = 0;

				if (resize_pending) {
					// Change in frame size so go back around the recode loop.
					rc.frame_size_selector = SCALE_STEP1 - rc.frame_size_selector;
					rc.next_frame_size_selector = rc.frame_size_selector;
					++loop_count;
					loop = 1;
					continue;
				}

				// Frame size out of permitted range:
				// Update correction factor & compute new Q to try...

				// Frame is too large
				if (rc->projected_frame_size > rc->this_frame_target) {
					// Special case if the projected size is > the max allowed.
					if (rc->projected_frame_size >= rc->max_frame_bandwidth)
						q_high = rc->worst_quality;

					// Raise Qlow as to at least the current value
					q_low = q < q_high ? q + 1 : q_high;

					if (undershoot_seen || loop_at_this_size > 1) {
						// Update rate_correction_factor unless
						rc.update_rate_correction_factors(cpi);
						q = (q_high + q_low + 1) / 2;
					} else {
						// Update rate_correction_factor unless
						rc.update_rate_correction_factors(cpi);
						q = rc.regulate_q(cpi, rc->this_frame_target, bottom_index, max(q_high, top_index));

						while (q < q_low && retries < 10) {
							rc.update_rate_correction_factors(cpi);
							q = rc.regulate_q(cpi, rc->this_frame_target, bottom_index, max(q_high, top_index));
							retries++;
						}
					}
					overshoot_seen = true;

				} else {
					// Frame is too small
					q_high = q > q_low ? q - 1 : q_low;

					if (overshoot_seen || loop_at_this_size > 1) {
						rc.update_rate_correction_factors(cpi);
						q = (q_high + q_low) / 2;
					} else {
						rc.update_rate_correction_factors(cpi);
						q = rc.regulate_q(cpi, rc->this_frame_target, bottom_index, top_index);
						// Special case reset for qlow for constrained quality.
						// This should only trigger where there is very substantial undershoot on a frame and the auto cq level is above the user passsed in value.
						if (oxcf.rc_mode == RC_CQ && q < q_low)
							q_low = q;

						while (q > q_high && retries < 10) {
							rc.update_rate_correction_factors(cpi);
							q = rc.regulate_q(cpi, rc->this_frame_target, bottom_index, top_index);
							retries++;
						}
					}

					undershoot_seen = true;
				}

				// Clamp Q to upper and lower limits:
				q		= clamp(q, q_low, q_high);
				loop	= q != last_q;
			} else {
				loop	= false;
			}
		}

		// Special case for overlay frame.
		if (rc->is_src_frame_alt_ref && rc->projected_frame_size < rc->max_frame_bandwidth)
			loop = false;

		if (loop) {
			++loop_count;
			++loop_at_this_size;

		}
	} while (loop);
}

int Encoder::setup_interp_filter_search_mask() {
	if (last_frame_type == KEY_FRAME || refresh_alt_ref_frame)
		return 0;

	int		ref_total[MAX_REF_FRAMES] = { 0 };
	for (int ref = REFFRAME_LAST; ref <= REFFRAME_ALTREF; ++ref)
		for (int ifilter = EIGHTTAP; ifilter <= EIGHTTAP_SHARP; ++ifilter)
			ref_total[ref] += interp_filter_selected[ref][ifilter];

	int mask = 0;
	for (int ifilter = EIGHTTAP; ifilter <= EIGHTTAP_SHARP; ++ifilter) {
		if ((ref_total[REFFRAME_LAST] && interp_filter_selected[REFFRAME_LAST][ifilter] == 0)
		&&	(ref_total[REFFRAME_GOLDEN] == 0 || interp_filter_selected[REFFRAME_GOLDEN][ifilter] * 50 < ref_total[REFFRAME_GOLDEN])
		&&	(ref_total[REFFRAME_ALTREF] == 0 ||	interp_filter_selected[ALTREF_FRAME][ifilter] * 50 < ref_total[REFFRAME_ALTREF])
		)
			mask |= 1 << ifilter;
	}
	return mask;
}

void Encoder::encode_frame_to_data_rate(size_t *size, uint8 *dest, uint32 *frame_flags) {
	TX_SIZE t;

	// Overrides the defaults with the externally supplied values with vp9_update_reference() and vp9_update_entropy() calls
	// Note: The overrides are valid only for the next frame passed to encode_frame_to_data_rate() function
	if (ext_refresh_frame_context_pending) {
		refresh_frame_context = ext_refresh_frame_context;
		ext_refresh_frame_context_pending = false;
	}
	if (ext_refresh_frame_flags_pending) {
		refresh_last_frame = ext_refresh_last_frame;
		refresh_golden_frame = ext_refresh_golden_frame;
		refresh_alt_ref_frame = ext_refresh_alt_ref_frame;
	}

	// Set the arf sign bias for this frame.
	ref_frame_sign_bias[REFFRAME_ALTREF] = oxcf.pass == 2 && multi_arf_allowed
		? rc.source_alt_ref_active && (!refresh_alt_ref_frame || (twopass.gf_group.rf_level[twopass.gf_group.index] == GF_ARF_LOW))
		: (rc.source_alt_ref_active && !refresh_alt_ref_frame);

	// Set default state for segment based loop filter update flags.
	lf.mode_ref_delta_update = 0;

	if (oxcf.pass == 2 && sf.adaptive_interp_filter_search)
		sf.interp_filter_search_mask = setup_interp_filter_search_mask(cpi);

	// Set various flags etc to special state if it is a key frame.
	if (frame_is_intra_only(cm)) {
		// Reset the loop filter deltas and segmentation map.
		vp9_reset_segment_features(&seg);

		// If segmentation is enabled force a map update for key frames.
		if (seg->enabled)
			seg->update_map = seg->update_data = true;

		// The alternate reference frame cannot be active for a key frame.
		rc.source_alt_ref_active = false;

		error_resilient_mode			= oxcf.error_resilient_mode;
		frame_parallel_decoding_mode	= oxcf.frame_parallel_decoding_mode;

		// By default, encoder assumes decoder can use prev_mi.
		if (error_resilient_mode) {
			frame_parallel_decoding_mode = true;
			reset_frame_context			= 0;
			refresh_frame_context		= false;
		} else if (intra_only) {
			// Only reset the current context.
			reset_frame_context			= 2;
		}
	}
	if (is_two_pass_svc(cpi) && error_resilient_mode == 0) {
		// Use context 0 for intra only empty frame, but the last frame context
		// for other empty frames.
		frame_context_idx = svc.encode_empty_frame_state == ENCODING
			?	(svc.encode_intra_empty_frame != 0 ? 0 : FRAME_CONTEXTS - 1)
			:	svc.spatial_layer_id * svc.number_temporal_layers + svc.temporal_layer_id;

		frame_parallel_decoding_mode = oxcf.frame_parallel_decoding_mode;

		// The probs will be updated based on the frame type of its previous
		// frame if frame_parallel_decoding_mode is 0. The type may vary for
		// the frame after a key frame in base layer since we may drop enhancement
		// layers. So set frame_parallel_decoding_mode to 1 in this case.
		if (frame_parallel_decoding_mode == 0) {
			if (svc.number_temporal_layers == 1) {
				if (svc.spatial_layer_id == 0 && svc.layer_context[0].last_frame_type == KEY_FRAME)
					frame_parallel_decoding_mode = true;
			} else if (svc.spatial_layer_id == 0) {
				// Find the 2nd frame in temporal base layer and 1st frame in temporal
				// enhancement layers from the key frame.
				for (int i = 0; i < svc.number_temporal_layers; ++i) {
					if (svc.layer_context[0].frames_from_key_frame == 1 << i) {
						frame_parallel_decoding_mode = true;
						break;
					}
				}
			}
		}
	}

	// For 1 pass CBR, check if we are dropping this frame.
	// For spatial layers, for now only check for frame-dropping on first spatial layer, and if decision is to drop, we drop whole super-frame.
	if (oxcf.pass == 0 && oxcf.rc_mode == RC_CBR && frame_type != KEY_FRAME) {
		if (is_one_pass_cbr_svc()
			? (svc.rc_drop_superframe == 1)
			: (oxcf.drop_frames_water_mark && svc.spatial_layer_id <= svc.first_spatial_layer_to_encode && rc.drop_frame(oxcf)
		) {
			update_buffer_level(0, show_frame, oxcf.drop_frames_water_mark || oxcf.content != CONTENT_SCREEN);
			if (is_one_pass_cbr_svc(cpi))
				update_layer_buffer_level(&svc, 0);

			rc.drop_frame();
			++curr_frame;
			ext_refresh_frame_flags_pending = false;
			svc.rc_drop_superframe			= true;
			return;
		}
	}

	if (sf.recode_loop == DISALLOW_RECODE)
		encode_without_recode_loop(cpi, size, dest);
	else
		encode_with_recode_loop(cpi, size, dest);

	// Special case code to reduce pulsing when key frames are forced at a
	// fixed interval. Note the reconstruction error if it is the frame before
	// the force key frame
	if (rc.next_key_frame_forced && rc.frames_to_key == 1)
		ambient_err = PSNR::get_sse(Source->y_buffer, get_frame_new_buffer(cm)->y_buffer, Source->y.width, Source->y.height);

	// If the encoder forced a KEY_FRAME decision
	if (frame_type == KEY_FRAME)
		refresh_last_frame = 1;

	frame_to_show = get_frame_new_buffer(cm);
	frame_to_show->color_space = color_space;
	frame_to_show->color_range = color_range;
	frame_to_show->render_width = render_width;
	frame_to_show->render_height = render_height;

	// Pick the loop filter level for the frame.
	loopfilter_frame(cpi, cm);

	// build the bitstream
	vp9_pack_bitstream(cpi, dest, size);

	if (seg.update_map)
		update_reference_segmentation_map(cpi);

	if (frame_is_intra_only() == 0)
		release_scaled_references(cpi);

	vp9_update_reference_frames(cpi);

	for (int t = TX_4X4; t <= TX_32X32; t++)
		full_to_model_counts(td.counts->coef[t],
		td.rd_counts.coef_counts[t]);

	if (!error_resilient_mode && !frame_parallel_decoding_mode)
		vp9_adapt_coef_probs(cm);

	if (!frame_is_intra_only()) {
		if (!error_resilient_mode && !frame_parallel_decoding_mode) {
			vp9_adapt_mode_probs(cm);
			vp9_adapt_mv_probs(cm, allow_high_precision_mv);
		}
	}

	ext_refresh_frame_flags_pending = 0;

	if (refresh_golden_frame)
		frame_flags |= FRAMEFLAGS_GOLDEN;
	else
		frame_flags &= ~FRAMEFLAGS_GOLDEN;

	if (refresh_alt_ref_frame)
		frame_flags |= FRAMEFLAGS_ALTREF;
	else
		frame_flags &= ~FRAMEFLAGS_ALTREF;

	ref_frame_flags = (1 << REFFRAME_LAST)
		|	(int(ref_frame_map[gld_fb_idx] != ref_frame_map[lst_fb_idx] && (rc.frames_till_gf_update_due != INT_MAX || svc.number_temporal_layers != 1 || svc.number_spatial_layers != 1)) << REFFRAME_GOLDEN)
		|	(int(ref_frame_map[alt_fb_idx] != ref_frame_map[lst_fb_idx] && ref_frame_map[gld_fb_idx] != ref_frame_map[alt_fb_idx]) << REFFRAME_ALTREF);

	last_frame_type = frame_type;

	if (!(is_two_pass_svc(cpi) && svc.encode_empty_frame_state == ENCODING))
		postencode_update(*size);

	if (frame_type == KEY_FRAME) {
		// Tell the caller that the frame was coded as a key frame
		*frame_flags = frame_flags | FRAMEFLAGS_KEY;
	} else {
		*frame_flags = frame_flags & ~FRAMEFLAGS_KEY;
	}

	// Clear the one shot update flags for segmentation map and mode/ref loop
	// filter deltas.
	seg.update_map	= false;
	seg.update_data = false;
	lf.mode_ref_delta_update = 0;

	// keep track of the last coded dimensions
	last_width		= width;
	last_height		= height;

	// reset to normal state now that we are done.
	if (!show_existing_frame)
		last_show_frame = show_frame;

	if (show_frame) {
		vp9_swap_mi_and_prev_mi(cm);
		// Don't increment frame counters if this was an altref buffer
		// update not a real frame
		++curr_frame;
		if (use_svc)
			vp9_inc_frame_in_layer(cpi);
	}
	prev_frame = cur_frame;

	if (use_svc)
		svc.layer_context[svc.spatial_layer_id * svc.number_temporal_layers + svc.temporal_layer_id].last_frame_type = frame_type;
}

static void Encode::SvcEncode(size_t *size, uint8 *dest, uint32 *frame_flags) {
	rc.get_svc_params(this);
	encode_frame_to_data_rate(size, dest, frame_flags);
}

void Encode::Pass0Encode(size_t *size, uint8 *dest, uint32 *frame_flags) {
	get_one_pass_params();
	encode_frame_to_data_rate(size, dest, frame_flags);
}

void Encode::Pass2Encode(size_t *size, uint8 *dest, uint32 *frame_flags) {
	allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;
	encode_frame_to_data_rate(size, dest, frame_flags);

	if (!(is_two_pass_svc() && svc.encode_empty_frame_state == ENCODING))
		vp9_twopass_postencode_update(cpi);
}

void Encode::init_ref_frame_bufs() {
	BufferPool *const pool = buffer_pool;
	new_fb_idx = INVALID_IDX;
	for (int i = 0; i < REF_FRAMES; ++i) {
		ref_frame_map[i] = INVALID_IDX;
		pool->frame_bufs[i].ref_count = 0;
	}
}

static void check_initial_width(VP9_COMP *cpi, int subsampling_x, int subsampling_y) {
	if (!initial_width || subsampling_x != subsampling_x || subsampling_y != subsampling_y) {
		subsampling_x = subsampling_x;
		subsampling_y = subsampling_y;

		alloc_raw_frame_buffers(cpi);
		init_ref_frame_bufs(cm);
		alloc_util_frame_buffers(cpi);

		init_motion_estimation(cpi);  // TODO(agrange) This can be removed.

		initial_width = width;
		initial_height = height;
		initial_mbs = MBs;
	}
}

int  Encoder::receive_raw_frame(uint32 frame_flags, FrameBuffer *sd, int64 time_stamp, int64 end_time) {
	check_initial_width(cpi, sd->subsampling_x, sd->subsampling_y);

#if CONFIG_VP9_TEMPORAL_DENOISING
	setup_denoiser_buffer(cpi);
#endif
	//vpx_usec_timer_start(&timer);

	if (vp9_lookahead_push(lookahead, sd, time_stamp, end_time, frame_flags))
		return RETURN_ERROR;

	vpx_usec_timer_mark(&timer);
	time_receive_data += vpx_usec_timer_elapsed(&timer);

	if ((profile == PROFILE_0 || profile == PROFILE_2) && (subsampling_x != 1 || subsampling_y != 1))
		return RETURN_INVALID_PARAM;

	if ((profile == PROFILE_1 || profile == PROFILE_3) && (subsampling_x == 1 && subsampling_y == 1))
		return RETURN_INVALID_PARAM;

	return RETURN_OK;
}


bool Encoder::frame_is_reference() const {
	return frame_type == KEY_FRAME
		|| refresh_last_frame
		|| refresh_golden_frame
		|| refresh_alt_ref_frame
		|| refresh_frame_context
		|| lf.mode_ref_delta_update
		|| seg.update_map
		|| seg.update_data;
}

static void adjust_frame_rate(VP9_COMP *cpi, const lookahead_entry *source) {
	int64	this_duration;
	int		step = 0;

	if (source->ts_start == first_time_stamp_ever) {
		this_duration = source->ts_end - source->ts_start;
		step = 1;
	} else {
		int64 last_duration = last_end_time_stamp_seen - last_time_stamp_seen;
		this_duration = source->ts_end - last_end_time_stamp_seen;

		// do a step update if the duration changes by 10%
		if (last_duration)
			step = (int)((this_duration - last_duration) * 10 / last_duration);
	}

	if (this_duration) {
		if (step) {
			new_framerate(10000000.0 / this_duration);
		} else {
			// Average this frame's rate into the last second's average
			// frame rate. If we haven't seen 1 second yet, then average
			// over the whole interval seen.
			const double	interval = min((double)(source->ts_end - first_time_stamp_ever), 10000000.0);
			double			avg_duration = 10000000.0 / framerate * (interval - avg_duration + this_duration) / interval;
			new_framerate(10000000.0 / avg_duration);
		}
	}
	last_time_stamp_seen = source->ts_start;
	last_end_time_stamp_seen = source->ts_end;
}

// Returns 0 if this is not an alt ref else the offset of the source frame
// used as the arf midpoint.
static int get_arf_src_index(VP9_COMP *cpi) {
	RATE_CONTROL *const rc = &rc;
	int arf_src_index = 0;
	if (is_altref_enabled(cpi)) {
		if (oxcf.pass == 2) {
			const GF_GROUP *const gf_group = &twopass.gf_group;
			if (gf_group->update_type[gf_group->index] == ARF_UPDATE) {
				arf_src_index = gf_group->arf_src_offset[gf_group->index];
			}
		} else if (rc->source_alt_ref_pending) {
			arf_src_index = rc->frames_till_gf_update_due;
		}
	}
	return arf_src_index;
}

static void check_src_altref(VP9_COMP *cpi,
	const struct lookahead_entry *source) {
	RATE_CONTROL *const rc = &rc;

	if (oxcf.pass == 2) {
		const GF_GROUP *const gf_group = &twopass.gf_group;
		rc->is_src_frame_alt_ref = gf_group->update_type[gf_group->index] == OVERLAY_UPDATE;
	} else {
		rc->is_src_frame_alt_ref = alt_ref_source && source == alt_ref_source;
	}

	if (rc->is_src_frame_alt_ref) {
		// Current frame is an ARF overlay frame.
		alt_ref_source = NULL;

		// Don't refresh the last buffer for an ARF overlay frame. It will
		// become the GF so preserve last as an alternative prediction option.
		refresh_last_frame = 0;
	}
}

int  Encoder::get_compressed_data(uint32 *frame_flags,
	size_t *size, uint8 *dest,
	int64 *time_stamp, int64 *time_end, int flush) {
	const EncoderConfig *const oxcf = &oxcf;
	VP9_COMMON *const cm = &common;
	BufferPool *const pool = buffer_pool;
	RATE_CONTROL *const rc = &rc;
	struct vpx_usec_timer  cmptimer;
	FrameBuffer *force_src_buffer = NULL;
	struct lookahead_entry *last_source = NULL;
	struct lookahead_entry *source = NULL;
	int arf_src_index;
	int i;

	if (is_two_pass_svc(cpi)) {
	#if CONFIG_SPATIAL_SVC
		vp9_svc_start_frame(cpi);
		// Use a small empty frame instead of a real frame
		if (svc.encode_empty_frame_state == ENCODING)
			source = &svc.empty_frame;
	#endif
		if (oxcf.pass == 2)
			vp9_restore_layer_context(cpi);
	} else if (is_one_pass_cbr_svc(cpi)) {
		vp9_one_pass_cbr_svc_start_layer(cpi);
	}

	vpx_usec_timer_start(&cmptimer);

	set_high_precision_mv(ALTREF_HIGH_PRECISION_MV);

	// Is multi-arf enabled.
	// Note that at the moment multi_arf is only configured for 2 pass VBR and
	// will not work properly with svc.
	multi_arf_allowed = oxcf.pass == 2 && !use_svc && oxcf.enable_auto_arf > 1;

	// Normal defaults
	reset_frame_context = 0;
	refresh_frame_context = 1;
	if (!is_one_pass_cbr_svc(cpi)) {
		refresh_last_frame = 1;
		refresh_golden_frame = 0;
		refresh_alt_ref_frame = 0;
	}

	// Should we encode an arf frame.
	arf_src_index = get_arf_src_index(cpi);

	// Skip alt frame if we encode the empty frame
	if (is_two_pass_svc(cpi) && source != NULL)
		arf_src_index = 0;

	if (arf_src_index) {
		assert(arf_src_index <= rc->frames_to_key);

		if ((source = vp9_lookahead_peek(lookahead, arf_src_index)) != NULL) {
			alt_ref_source = source;

		#if CONFIG_SPATIAL_SVC
			if (is_two_pass_svc(cpi) && svc.spatial_layer_id > 0) {
				int i;
				// Reference a hidden frame from a lower layer
				for (i = svc.spatial_layer_id - 1; i >= 0; --i) {
					if (oxcf.ss_enable_auto_arf[i]) {
						gld_fb_idx = svc.layer_context[i].alt_ref_idx;
						break;
					}
				}
			}
			svc.layer_context[svc.spatial_layer_id].has_alt_frame = 1;
		#endif

			if (oxcf.arnr_max_frames > 0 && oxcf.arnr_strength > 0) {
				// Produce the filtered ARF frame.
				vp9_temporal_filter(cpi, arf_src_index);
				vpx_extend_frame_borders(&alt_ref_buffer);
				force_src_buffer = &alt_ref_buffer;
			}

			show_frame					= false;
			intra_only					= false;
			refresh_alt_ref_frame		= true;
			refresh_golden_frame		= false;
			refresh_last_frame			= false;
			rc->is_src_frame_alt_ref	= false;
			rc->source_alt_ref_pending	= false;
		} else {
			rc->source_alt_ref_pending	= false;
		}
	}

	if (!source) {
		// Get last frame source.
		if (curr_frame > 0) {
			if ((last_source = vp9_lookahead_peek(lookahead, -1)) == NULL)
				return -1;
		}

		// Read in the source frame.
		if (use_svc)
			source = vp9_svc_lookahead_pop(cpi, lookahead, flush);
		else
			source = vp9_lookahead_pop(lookahead, flush);

		if (source != NULL) {
			show_frame = 1;
			intra_only = 0;
			// if the flags indicate intra frame, but if the current picture is for
			// non-zero spatial layer, it should not be an intra picture.
			// TODO(Won Kap): this needs to change if per-layer intra frame is
			// allowed.
			if ((source->flags & VPX_EFLAG_FORCE_KF) &&
				svc.spatial_layer_id > svc.first_spatial_layer_to_encode) {
				source->flags &= ~(uint32)(VPX_EFLAG_FORCE_KF);
			}

			// Check to see if the frame should be encoded as an arf overlay.
			check_src_altref(cpi, source);
		}
	}

	if (source) {
		un_scaled_source = Source = force_src_buffer ? force_src_buffer : &source->img;
		unscaled_last_source = last_source != NULL ? &last_source->img : NULL;
		*time_stamp		= source->ts_start;
		*time_end		= source->ts_end;
		*frame_flags	= (source->flags & VPX_EFLAG_FORCE_KF) ? FRAMEFLAGS_KEY : 0;

	} else {
		*size = 0;
		if (flush && oxcf.pass == 1 && !twopass.first_pass_done) {
			vp9_end_first_pass(cpi);    /* get last stats packet */
			twopass.first_pass_done = true;
		}
		return -1;
	}

	if (source->ts_start < first_time_stamp_ever) {
		first_time_stamp_ever = source->ts_start;
		last_end_time_stamp_seen = source->ts_start;
	}

	// adjust frame rates based on timestamps given
	if (show_frame) {
		adjust_frame_rate(cpi, source);
	}

	if (is_one_pass_cbr_svc(cpi)) {
		vp9_update_temporal_layer_framerate(cpi);
		vp9_restore_layer_context(cpi);
	}

	// Find a free buffer for the new frame, releasing the reference previously
	// held.
	if (new_fb_idx != INVALID_IDX) {
		--pool->frame_bufs[new_fb_idx].ref_count;
	}
	new_fb_idx = get_free_fb(cm);

	if (new_fb_idx == INVALID_IDX)
		return -1;

	cur_frame = &pool->frame_bufs[new_fb_idx];

	if (!use_svc && multi_arf_allowed) {
		if (frame_type == KEY_FRAME) {
			lst_fb_idx = 0;
			gld_fb_idx = 1;
			alt_fb_idx = 2;
		} else if (oxcf.pass == 2) {
			const GF_GROUP *const gf_group = &twopass.gf_group;
			alt_fb_idx = gf_group->arf_ref_idx[gf_group->index];
		}
	}

	// Start with a 0 size frame.
	*size = 0;

	frame_flags = *frame_flags;

	if ((oxcf.pass == 2) &&
		(!use_svc ||
		(is_two_pass_svc(cpi) &&
		svc.encode_empty_frame_state != ENCODING))) {
		rc.get_second_pass_params(cpi);
	} else if (oxcf.pass == 1) {
		set_frame_size(cpi);
	}

	if (oxcf.pass != 0 ||
		use_svc ||
		frame_is_intra_only(cm) == 1) {
		for (i = 0; i < MAX_REF_FRAMES; ++i)
			scaled_ref_idx[i] = INVALID_IDX;
	}

	if (oxcf.pass == 1 &&
		(!use_svc || is_two_pass_svc(cpi))) {
		const int lossless = is_lossless_requested(oxcf);
		td.mb.fwd_txm4x4 = lossless ? vp9_fwht4x4 : vpx_fdct4x4;
		td.mb.itxm_add = lossless ? vp9_iwht4x4_add : vp9_idct4x4_add;
		vp9_first_pass(cpi, source);
	} else if (oxcf.pass == 2 &&
		(!use_svc || is_two_pass_svc(cpi))) {
		Pass2Encode(cpi, size, dest, frame_flags);
	} else if (use_svc) {
		SvcEncode(cpi, size, dest, frame_flags);
	} else {
		// One pass encode
		Pass0Encode(cpi, size, dest, frame_flags);
	}

	if (refresh_frame_context)
		frame_contexts[frame_context_idx] = *fc;

	// No frame encoded, or frame was dropped, release scaled references.
	if ((*size == 0) && (frame_is_intra_only(cm) == 0)) {
		release_scaled_references(cpi);
	}

	if (*size > 0) {
		droppable = !frame_is_reference(cpi);
	}

	// Save layer specific state.
	if (is_one_pass_cbr_svc(cpi) ||
		((svc.number_temporal_layers > 1 ||
		svc.number_spatial_layers > 1) &&
		oxcf.pass == 2)) {
		vp9_save_layer_context(cpi);
	}

	vpx_usec_timer_mark(&cmptimer);
	time_compress_data += vpx_usec_timer_elapsed(&cmptimer);

	if (b_calculate_psnr && oxcf.pass != 1 && show_frame) {
		if (use_svc) {
			calc_psnr(Source, common.frame_to_show, &svc.layer_context[svc.spatial_layer_id * svc.number_temporal_layers].psnr);
		} else {
			PKT	&pkt	= output_pkt_list.push_back();
			pkt.kind	= PKT::PKT_PSNR;
			calc_psnr(Source, common.frame_to_show, &pkt.psnr);
		}
	}

	if (is_two_pass_svc(cpi)) {
		if (svc.encode_empty_frame_state == ENCODING) {
			svc.encode_empty_frame_state = ENCODED;
			svc.encode_intra_empty_frame = 0;
		}

		if (show_frame) {
			++svc.spatial_layer_to_encode;
			if (svc.spatial_layer_to_encode >= svc.number_spatial_layers)
				svc.spatial_layer_to_encode = 0;

			// May need the empty frame after an visible frame.
			svc.encode_empty_frame_state = NEED_TO_ENCODE;
		}
	} else if (is_one_pass_cbr_svc(cpi)) {
		if (show_frame) {
			++svc.spatial_layer_to_encode;
			if (svc.spatial_layer_to_encode >= svc.number_spatial_layers)
				svc.spatial_layer_to_encode = 0;
		}
	}
	return 0;
}

int  Encoder::get_preview_raw_frame(FrameBuffer *dest,
	vp9_ppflags_t *flags) {
	VP9_COMMON *cm = &common;
#if !CONFIG_VP9_POSTPROC
	(void)flags;
#endif

	if (!show_frame) {
		return -1;
	} else {
		int ret;
	#if CONFIG_VP9_POSTPROC
		ret = vp9_post_proc_frame(cm, dest, flags);
	#else
		if (frame_to_show) {
			*dest = *frame_to_show;
			dest->y_width = width;
			dest->y_height = height;
			dest->uv_width = width >> subsampling_x;
			dest->uv_height = height >> subsampling_y;
			ret = 0;
		} else {
			ret = -1;
		}
	#endif  // !CONFIG_VP9_POSTPROC
		return ret;
	}
}

int  Encoder::set_internal_size(
	VPX_SCALING horiz_mode, VPX_SCALING vert_mode) {
	VP9_COMMON *cm = &common;
	int hr = 0, hs = 0, vr = 0, vs = 0;

	if (horiz_mode > ONETWO || vert_mode > ONETWO)
		return -1;

	Scale2Ratio(horiz_mode, &hr, &hs);
	Scale2Ratio(vert_mode, &vr, &vs);

	// always go to the next whole number
	width = (hs - 1 + oxcf.width * hr) / hs;
	height = (vs - 1 + oxcf.height * vr) / vs;
	if (curr_frame) {
		assert(width <= initial_width);
		assert(height <= initial_height);
	}

	update_frame_size(cpi);

	return 0;
}

int  Encoder::set_size_literal(uint32 width,
	uint32 height) {
	VP9_COMMON *cm = &common;
	check_initial_width(cpi, 1, 1);

#if CONFIG_VP9_TEMPORAL_DENOISING
	setup_denoiser_buffer(cpi);
#endif

	if (width) {
		width = width;
		if (width > initial_width) {
			width = initial_width;
			printf("Warning: Desired width too large, changed to %d\n", width);
		}
	}

	if (height) {
		height = height;
		if (height > initial_height) {
			height = initial_height;
			printf("Warning: Desired height too large, changed to %d\n", height);
		}
	}
	assert(width <= initial_width);
	assert(height <= initial_height);

	update_frame_size(cpi);

	return 0;
}

void  Encoder::set_svc(int use_svc) {
	use_svc = use_svc;
	return;
}

void  Encoder::apply_encoding_flags(vpx_enc_frame_flags_t flags) {
	if (flags & (VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_GF |
		VP8_EFLAG_NO_REF_ARF)) {
		int ref = 7;

		if (flags & VP8_EFLAG_NO_REF_LAST)
			ref ^= VP9_LAST_FLAG;

		if (flags & VP8_EFLAG_NO_REF_GF)
			ref ^= VP9_GOLD_FLAG;

		if (flags & VP8_EFLAG_NO_REF_ARF)
			ref ^= VP9_ALT_FLAG;

		vp9_use_as_reference(cpi, ref);
	}

	if (flags & (VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF |
		VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_FORCE_GF |
		VP8_EFLAG_FORCE_ARF)) {
		int upd = 7;

		if (flags & VP8_EFLAG_NO_UPD_LAST)
			upd ^= VP9_LAST_FLAG;

		if (flags & VP8_EFLAG_NO_UPD_GF)
			upd ^= VP9_GOLD_FLAG;

		if (flags & VP8_EFLAG_NO_UPD_ARF)
			upd ^= VP9_ALT_FLAG;

		vp9_update_reference(cpi, upd);
	}

	if (flags & VP8_EFLAG_NO_UPD_ENTROPY) {
		vp9_update_entropy(cpi, 0);
	}
}


} // namespace vp9
