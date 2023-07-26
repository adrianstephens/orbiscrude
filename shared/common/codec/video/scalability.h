#ifndef SCALABILITY_H
#define SCALABILITY_H

#include "h265.h"

namespace h265 {

//-----------------------------------------------------------------------------
// HRD - hypothetical reference decoder
//-----------------------------------------------------------------------------

struct hrd_parameters {
	struct sub_pic {
		as_minus<u<8>,2>	tick_divisor;
		as_minus<u<5>,1>	du_cpb_removal_delay_increment_length;
		bool				sub_pic_cpb_params_in_pic_timing_sei;
		as_minus<u<5>,1>	dpb_output_delay_du_length;
		uint8				cpb_size_du_scale;
		bool	read(bitreader& r) { return r.read(tick_divisor, du_cpb_removal_delay_increment_length, sub_pic_cpb_params_in_pic_timing_sei, dpb_output_delay_du_length); }
	};
	struct layer {
		struct nalvcl {
			as_minus<ue,1>	bit_rate;
			as_minus<ue,1>	cpb_size;
			as_minus<ue,1>	cpb_size_du;
			as_minus<ue,1>	bit_rate_du;
			bool			cbr;
			void read(bitreader& r, bool sub_pic) {
				r.read(bit_rate, cpb_size);
				if (sub_pic)
					r.read(cpb_size_du, bit_rate_du);
				cbr = r.get_bit();
			}
		};
		bool	fixed_pic_rate_general;
		bool	fixed_pic_rate_within_cvs;
		bool	low_delay_hrd;
		uint32	cpb_cnt;
		uint32	elemental_duration_in_tc;
		nalvcl	nal[32];
		nalvcl	vcl[32];

		void read(bitreader& r) {
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
	};

	bool				nal_present	= false;
	bool				vcl_present	= false;
	optional<sub_pic>	sub_pic;
	u<4>				bit_rate_scale;
	u<4>				cpb_size_scale;
	as_minus<u<5>,1>	initial_cpb_removal_delay_length;
	as_minus<u<5>,1>	au_cpb_removal_delay_length;
	as_minus<u<5>,1>	dpb_output_delay_length;
	layer				layers[7];

	bool	read(bitreader& r, bool common, int max_sub_layers) {
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
};

//-----------------------------------------------------------------------------
// VUI
//-----------------------------------------------------------------------------

struct video_signal_info {
	enum Format {
		Component	= 0,
		PAL			= 1,
		NTSC		= 2,
		SECAM		= 3,
		MAC			= 4,
		Unspecified = 5
	};
	struct colour_description {
		uint8	colour_primaries			= 2;
		uint8	transfer_characteristics	= 2;
		uint8	matrix_coeffs				= 2;
		bool read(bitreader &r) { return r.read(colour_primaries, transfer_characteristics, matrix_coeffs); }
	};

	compact<Format,3>	format		= Unspecified;
	bool				full_range	= false;
	optional<colour_description>	colour_description;

	bool read(bitreader &r)		{ return r.read(format, full_range, colour_description); }
};

struct video_timing {
	uint32						num_units_in_tick	= 0;
	uint32						time_scale			= 0;
	optional<as_minus<ue,1>>	num_ticks_poc_diff_one;

	bool	read(bitreader& r) { return r.read(num_units_in_tick, time_scale, num_ticks_poc_diff_one); }
};

static uint16x2 sar_presets[] = {
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

struct video_usability_information {
	struct chroma_loc {
		ue		top_field		= 0;
		ue		bottom_field	= 0;
		bool read(bitreader& r)	{ return r.read(top_field, bottom_field); }
	};

	struct restrictions {
		bool	tiles_fixed_structure				= false;
		bool	motion_vectors_over_pic_boundaries	= true;
		bool	restricted_ref_pic_lists			= false;
		ue		min_spatial_segmentation_idc		= 0;
		ue		max_bytes_per_pic_denom				= 2;
		ue		max_bits_per_min_cu_denom			= 1;
		ue		log2_max_mv_length_horizontal		= 15;
		ue		log2_max_mv_length_vertical			= 15;

		bool read(bitreader& r) {
			return r.read(
				tiles_fixed_structure, motion_vectors_over_pic_boundaries, restricted_ref_pic_lists,
				min_spatial_segmentation_idc, max_bytes_per_pic_denom, max_bits_per_min_cu_denom, log2_max_mv_length_horizontal, log2_max_mv_length_vertical
			);
		}
		bool valid() const{
			return min_spatial_segmentation_idc < 4096
				&& max_bytes_per_pic_denom		<= 16
				&& max_bits_per_min_cu_denom	<= 16
				&& log2_max_mv_length_horizontal<= 15
				&& log2_max_mv_length_vertical	<= 15;
		}
	};


	optional<uint16x2>			sar;						// sample aspect ratio (SAR)
	optional<bool>				overscan_appropriate;		// overscan
	optional<video_signal_info>	video_signal;
	optional<chroma_loc>		chroma_loc;

	bool						neutral_chroma_indication	= false;
	bool						field_seq					= false;
	bool						frame_field_info_present	= false;

	optional<uint32x4>			def_disp_offsets;			// default display window
	optional<video_timing>		timing;
	optional<hrd_parameters>	hrd_parameters;
	optional<restrictions>		restrictions;

	bool read(bitreader& r, int max_sub_layers) {
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
};

//-----------------------------------------------------------------------------
// Profile
//-----------------------------------------------------------------------------

struct Profile {
	enum PROFILE {
		UNKNOWN							= 0,
		MAIN							= 1,
		MAIN10							= 2,
		MAIN_STILL_PICTURE				= 3,
		REXT							= 4,
		HIGH_THROUGHPUT					= 5,
		MULTIVIEW_MAIN					= 6,
		SCALABLE_MAIN					= 7,
		MAIN_3D							= 8,
		SCREEN_EXTENDED					= 9,
		SCALABLE_REXT					= 10,
		HIGH_THROUGHPUT_SCREEN_EXTENDED	= 11,
	};
	enum FLAGS {
		progressive_source,
		interlaced_source,
		non_packed_constraint,
		frame_only_constraint,
		max_12bit_constraint,
		max_10bit_constraint,
		max_8bit_constraint,
		max_422chroma_constraint,
		max_420chroma_constraint,
		max_monochrome_constraint,
		intra_constraint,
		one_picture_only_constraint,
		lower_bit_rate_constraint,
		max_14bit_constraint,
		in_bld							= 46,
	};

	u<2>				space;					// currently always 0
	bool				tier;					// main tier or low tier (see Table A-66/A-67)
	compact<PROFILE,5>	profile;				// profile
	uint32				compatibility;			// to which profile we are compatible
	bitarray<48>		flags;

	void	init(PROFILE _profile) {
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

	bool	read(bitreader &r) {
		r.read(space, tier, profile, compatibility);
		for (auto &&i : flags)
			i = r.get_bit();
		return true;
	}
	bool	write(bitwriter &w) const {
		w.write(space, tier, profile, compatibility);
		for (auto i : flags)
			w.put_bit(i);
		return true;
	}
};

struct ProfileLayer : Profile {
	uint8	level;
	bool	profile_present;
	bool	level_present;
	bool	read(bitreader &r)			{ return (!profile_present || Profile::read(r))  && (!level_present || r.read(level)); }
	bool	write(bitwriter &w) const	{ return (!profile_present || Profile::write(w)) && (!level_present || w.write(level)); }
};

struct profile_tier_level : static_array<ProfileLayer, MAX_TEMPORAL_SUBLAYERS> {

	void init(Profile::PROFILE _profile, uint8 level) {
		resize(1);
		(*this)[0].init(_profile);
		(*this)[0].level	= level;
	}

	bool	read(bitreader &r, bool profile_present, uint32 max_sublayers) {
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
	bool	write(bitwriter &w) const {
		(*this)[0].write(w);

		for (auto &i : slice(1))
			w.write(i.profile_present, i.level_present);

		if (size() > 1)
			w.put(0, (9 - size()) * 2);

		return w.write(slice(1));
	}
};

#ifdef HEVC_ML

//-----------------------------------------------------------------------------
// ML
//-----------------------------------------------------------------------------

struct multilayer_extension {
	enum Dimension {
		DepthLayerFlag	= 0,
		ViewOrderIdx	= 1,
		DependencyId	= 2,
		AuxId			= 3,
	};
	enum Aux {
		AUX_ALPHA		= 1,
		AUX_DEPTH		= 2,
	};

	struct restriction {
		int	min_spatial_segment_offset, min_horizontal_ctu_offset;
		bool read(bitreader &r) {
			min_spatial_segment_offset = r.getu() - 1;
			if (min_spatial_segment_offset >= 0 && r.get_bit())
				min_horizontal_ctu_offset = r.getu() - 1;
			return true;
		}
	};

	struct layer {
		uint8					id_in_nuh;
		uint8					max_sub_layers;

		uint8					dimension_id[16]			= {0};
		uint64					direct_dependency			= 0;
		uint64					dependency					= 0;
		uint64					predicted					= 0;
		uint32					rep_format_idx				= 0;
		bool					poc_lsb_not_present;
		uint32					direct_dependency_type[8]	= {0};
		u<3>					max_tid_il_ref_pics_plus1[8]= {7,7,7,7,7,7,7,7};

		//vui extension
		int						video_signal_info_idx		= -1;
		bool					tiles_in_use;
		bool					loop_filter_not_across_tiles;
		uint64					tile_boundaries_aligned;
		bool					wpp_in_use;
		bool					base_layer_parameter_set_compatibility;
		restriction				restrictions[8];

		layer(uint8 max_sub_layers) : max_sub_layers(max_sub_layers) {}

		uint8	depth_layer()		const { return dimension_id[DepthLayerFlag]; }
		uint8	view_order_idx()	const { return dimension_id[ViewOrderIdx]; }
		uint8	dependency_id()		const { return dimension_id[DependencyId]; }
		uint8	aux_id()			const { return dimension_id[AuxId]; }

		bool	InterLayerSamplePredictionEnabled(int j) const { return direct_dependency_type[j] & 1; }
		bool	InterLayerMotionPredictionEnabled(int j) const { return direct_dependency_type[j] & 2; }
	};

	struct output_layer_set {
		struct schedule {
			uint32	hrd_idx, sched_idx;
		};
		struct sub_layer {
			bool					sub_layer_dpb_info_present;
			dynamic_array<uint32>	max_vps_dec_pic_buffering;
			uint32					max_vps_num_reorder_pics;
			uint32					max_vps_latency_increase_plus1;
			dynamic_array<dynamic_array<dynamic_array<schedule>>>	bsp_schedules;
		};

		uint32						layer_set;
		uint64						flags;
		uint64						NecessaryLayerFlags;
		dynamic_array<uint32>		profile_tier_level_idx;
		bool						alt_output_layer;
		dynamic_array<sub_layer>	sub_layers;

		dynamic_array<dynamic_array<uint64>>	signalled_partitioning_schemes;
	};

	struct vui_layer_set_entry {
		uint16	avg_bit_rate;
		uint16	max_bit_rate;
		uint16	avg_pic_rate;
		uint8	constant_pic_rate;

		bool read(bitreader &r, bool bit_rate_present, bool pic_rate_present) {
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
	};

	struct rep_format {
		struct _bitdepths {
			CHROMA	chroma_format;
			bool	separate_colour_plane;
			uint8	bit_depth_luma;
			uint8	bit_depth_chroma;
			bool read(bitreader &r) {
				chroma_format		= (CHROMA)r.get(2);
				separate_colour_plane = chroma_format == CHROMA_444 && r.get_bit();
				bit_depth_luma		= r.get(4) + 8;
				bit_depth_chroma	= r.get(4) + 8;
				return true;
			}
		};
		uint16					pic_width_luma;
		uint16					pic_height_luma;
		optional<_bitdepths>	bitdepths;
		optional<array<ue,4>>	conformance_window;

		bool read(bitreader &r) { return r.read(pic_width_luma, pic_height_luma, bitdepths, conformance_window); }
	};

	struct region {
		struct phase {
			ue				hor_luma,	ver_luma;
			as_plus<ue,8>	hor_chroma,	ver_chroma;
			bool	read(bitreader &r) { return r.read(hor_luma, ver_luma, hor_chroma, ver_chroma); }
		};
		optional<array<se,4>>	scaled_ref_layer;
		optional<array<se,4>>	ref_region;
		optional<phase>			phase;
		bool	read(bitreader &r) { return r.read(scaled_ref_layer, ref_region, phase); }
	};

	class colour_mapping {
		struct coeff_reader {
			uint8				res_quant_bits;
			uint8				delta_flc_bits;
			uint8				rbits;

			coeff_reader(int luma_bit_depth_input, int luma_bit_depth_output, uint8 res_quant_bits, uint8 delta_flc_bits)
				: res_quant_bits(res_quant_bits)
				, delta_flc_bits(delta_flc_bits)
				, rbits(max(0, (10 + luma_bit_depth_input - luma_bit_depth_output - res_quant_bits - delta_flc_bits)))
			{}
			int		read_value(bitreader &r) const {
				uint32	v	= r.getu();
				if (rbits)
					v = (v << rbits) + r.get(rbits);
				return plus_minus(v << res_quant_bits, v && r.get_bit());
			}
		};

		dynamic_array<int>		coeffs;

		u<2>				octant_depth;
		u<2>				y_part_num_log2;
		uint16x2			adapt_threshold;
		uint8				yshift, cshift, mapping_shift;

		bool	read(bitreader &r, int *e, const coeff_reader &otr, int depth) {
			if (depth < octant_depth && r.get_bit()) {
				auto	offset = (3 * 4) << ((octant_depth - depth - 1) * 3 + y_part_num_log2);
				for (int i = 0; i < 8; i++)
					read(r, e + offset * i, otr, depth + 1);

			} else {
				int	a0	= 0, a1 = 0, a2 = 0;
				for (int i = 0; i < (1 << y_part_num_log2); i++) {
					for (int j = 0; j < 4; j++) {
						if (r.get_bit()) {
							a0 += otr.read_value(r);
							a1 += otr.read_value(r);
							a2 += otr.read_value(r);
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
		int get_index0(int y, int cb, int cr) const {
			return ((((cr << octant_depth) + cb) << (octant_depth + y_part_num_log2)) + y) * 3 * 4;
		}
		int get_index(int y, int cb, int cr) const {
			return get_index0(y >> yshift, octant_depth == 1 ? (cb >= adapt_threshold.x) : (cb >> cshift), octant_depth == 1 ? (cr >= adapt_threshold.y) : (cr >> cshift));
		}
		int get_value(int y, int cb, int cr, int32x4 c) const {
			return (c.x * y + c.y * cb + c.z * cr + (1 << (mapping_shift - 1)) >> mapping_shift) + c.w;
		}

	public:
		uint64				layer_enable;
		as_minus<ue,8>		luma_bit_depth_input;
		as_minus<ue,8>		chroma_bit_depth_input;
		as_minus<ue,8>		luma_bit_depth_output;
		as_minus<ue,8>		chroma_bit_depth_output;

		bool	read(bitreader &r) {
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
				return read(r, coeffs, coeff_reader(luma_bit_depth_input, luma_bit_depth_output, res_quant_bits, delta_flc_bits), 0);
			}
			return false;
		}
		int get_Y(int y, int cb, int cr) const {
			return clamp(get_value(y, cb, cr, load<4>(coeffs + get_index(y, cb, cr) + 0)), 0, bits(luma_bit_depth_output));
		}
		int get_Cb(int y, int cb, int cr) const {
			return clamp(get_value(y, cb, cr, load<4>(coeffs + get_index(y, cb, cr) + 4)), 0, bits(chroma_bit_depth_output));
		}
		int get_Cr(int y, int cb, int cr) const {
			return clamp(get_value(y, cb, cr, load<4>(coeffs + get_index(y, cb, cr) + 8)), 0, bits(chroma_bit_depth_output));
		}
	};

	struct vps {
		dynamic_array<uint64>		layer_sets;
		dynamic_array<uint64>		TreePartitionList;
		dynamic_array<profile_tier_level>	profile_tier_levels;

		dynamic_array<layer>		layers;
		dynamic_array<uint16>		view_ids;

		bool	default_ref_layers_active;
		bool	max_one_active_ref_layer;
		bool	poc_lsb_aligned;

		uint8	default_output_layer_idc;

		dynamic_array<output_layer_set>	output_layer_sets;
		dynamic_array<rep_format>		rep_formats;
		dynamic_array<uint8>			non_vui_extension;

		//vui extension
		bool	cross_layer_pic_type_aligned;
		bool	cross_layer_irap_aligned;
		bool	all_layers_idr_aligned;
		bool	single_layer_for_non_irap;
		bool	higher_layer_irap_skip;
		dynamic_array<dynamic_array<vui_layer_set_entry>>	vui_layer_sets;

		struct video_signal_info : h265::video_signal_info {
			bool read(bitreader &r)	{ return r.read(format, full_range, colour_description.put()); }
		};
		dynamic_array<video_signal_info>			vsi;
		dynamic_array<pair<uint16, hrd_parameters>>	hrd_layer_sets;
		dynamic_array<hrd_parameters>				hrd_layer_sets2;

		vps(uint8 max_layers, uint8 max_sub_layers, dynamic_array<uint64> &&layer_sets, dynamic_array<pair<uint16, hrd_parameters>> &&hrd_layer_sets) : layer_sets(move(layer_sets)), layers(max_layers, max_sub_layers), hrd_layer_sets(move(hrd_layer_sets)) {}

		bool read(bitreader &r, bool base_layer_internal);
		bool read_vui(bitreader& r, bool base_layer_internal);

		const layer*	layer_by_id(int layer_id) const {
			for (auto& i : layers) {
				if (i.id_in_nuh == layer_id)
					return &i;
			}
			return nullptr;
		}
		const layer*	find_layer(int view_order, bool depth_layer) const {
			for (auto &j : layers) {
				if (j.depth_layer() == depth_layer && j.view_order_idx() == view_order && j.dependency_id() == 0 && j.aux_id() == 0)
					return &j;
			}
			return nullptr;
		}

		bool	RefCmpLayerAvail(const layer *layer1, const layer *layer2, int temporal_id) const {
			return layer2
				&& (layer1->direct_dependency & (1 << layer_index(layer2)))
				&& layer2->max_sub_layers > temporal_id
				&& (temporal_id == 0 || layer2->max_tid_il_ref_pics_plus1[layer_index(layer1)] > temporal_id);
		}


		bool	allRefCmpLayersAvail(const layer *layer1, range<const uint8*> ref_layers, int temporal_id) const {
			bool	depth_layer	= layer1->depth_layer();
			for (auto i : ref_layers) {
				if (auto layer2 = find_layer(layers[i].view_order_idx(), !depth_layer)) {
					if (!RefCmpLayerAvail(layer1, layer2, temporal_id))
						return false;
				}
			}
			return true;
		}

		int	layer_index(const layer *i)		const { return layers.index_of(i); }
		int	layer_index(int layer_id)		const { return layer_index(layer_by_id(layer_id)); }
	};

	struct sps {
		bool	inter_view_mv_vert_constraint;
		bool	read(bitreader &r) {
			return r.read(inter_view_mv_vert_constraint);
		}
	};

	struct pps {
		bool									poc_reset_info_present;
		optional<u<6>>							scaling_list_ref_layer_id;
		dynamic_array<pair<uint32, region>>		regions;
		optional<colour_mapping>				colour_mapping;

		bool	read(bitreader &r) {
			r.read(poc_reset_info_present, scaling_list_ref_layer_id);

			regions.resize(r.getu());
			for (auto &i : regions) {
				i.a = r.getu();
				i.b.read(r);
			}

			return r.read(colour_mapping);
		}
	};

	struct shdr {
		uint32		poc_msb_cycle_val;
		static_array<uint8, 8>	ref_layers;

		u<2>		poc_reset_idc;
		u<6>		poc_reset_period_id;
		bool		full_poc_reset;
		uint32		poc_lsb_val;

		bool	read(bitreader &r, const pps &pps, uint8 log2_max_pic_order_cnt_lsb) {
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

		bool read(bitreader &r, const vps *vpsml, const layer *layer, int temporal_id) {
			//auto	layer	= layer_id > 0 ? vpsml->layer_by_id(layer_id) : nullptr;

			if (layer && layer->direct_dependency) {
				if (vpsml->default_ref_layers_active || r.get_bit()) {
					int			layer_idx	= vpsml->layer_index(layer);
					auto		ndirect		= count_bits(layer->direct_dependency);
					int			nbits		= log2_ceil(ndirect);

					int			j = 0;
					uint8		inter_layer_pred_layer[8];
					for (auto i : make_bit_container(layer->direct_dependency)) {
						auto&	layer2 = vpsml->layers[i];
						if (layer2.max_sub_layers > temporal_id && (temporal_id == 0 || layer2.max_tid_il_ref_pics_plus1[layer_idx] > temporal_id))
							inter_layer_pred_layer[j++] = i;
					}

					int	inter_layer_refs	= j == 0	?	0
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
	};
};

bool multilayer_extension::vps::read(bitreader &r, bool base_layer_internal) {
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
		if (!splitting) {
			auto	len = dimension_id_len.begin();
			for (auto j : make_bit_container(scalability_mask))
				layer.dimension_id[j] = r.get(*len++);
		}
	}

	// --- view ---
	int	num_views = 1;
	for (auto &layer : slice(layers, 1)) {
		bool	new_view	= true;
		auto	idx			= layer.dimension_id[ViewOrderIdx];
		for (auto &j : layers.slice_to(&layer)) {
			if (idx == j.dimension_id[ViewOrderIdx])
				new_view = false;
		}
		num_views += new_view;
	}

	view_ids.resize(num_views, 0);
	if (auto nbits = r.get(4))
		for(auto &i : view_ids)
			i = r.get(nbits);

	//--- dependencies ---
	for (auto &i : slice(layers, 1))
		i.direct_dependency = r.get_reverse(layers.index_of(i));

	for (auto &i : layers) {
		i.dependency		= i.direct_dependency;
		for (auto j : make_bit_container(i.direct_dependency))
			i.dependency |= layers[j].dependency;
		uint64	predicted = 1 << layers.index_of(i);
		for (auto j : make_bit_container(i.direct_dependency))
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
	if (r.get_bit()) {	//vps_sub_layers_max_minus1_present
		for (auto &layer : layers)
			layer.max_sub_layers = r.get(3) + 1;
	}

	// --- max_tid_il_ref_pics ---
	if (r.get_bit()) {
		for (auto &layer : layers)
			readn(r, layer.max_tid_il_ref_pics_plus1, count_bits(layer.direct_dependency));
	}

	default_ref_layers_active = r.get_bit();

	//
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
		for (auto j : make_bit_container(ols.flags))
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
		for (auto k : make_bit_container(included))
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

bool multilayer_extension::vps::read_vui(bitreader& r, bool base_layer_internal) {
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

#ifdef HEVC_3D
//-----------------------------------------------------------------------------
// 3D
//-----------------------------------------------------------------------------

struct three_d_extension {

	struct depth_lookup {
		optional<uint64>		val_flags;
		dynamic_array<uint32>	delta_list;

		bool	read(bitreader &r, uint8 bit_depth) {
			if (!r.get_bit()) {
				if (r.get_bit()) {
					val_flags = r.get(bits(bit_depth));
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
			return true;
		}

		auto	idx2val(int i) const {
			return i < delta_list.size() ? delta_list[i] : 0;
		}
		
		auto	val2idx(int d) const {
			int		lower	= 0;
			bool	found	= false;
			for (int i = 1; i < delta_list.size(); i++ ) {
				if (delta_list[i] > d) {
					lower = i - 1;
					found = true;
					break;
				}
			}
			int	upper = found ? (lower + 1) : delta_list.size() - 1;
			return abs(d - (int)delta_list[lower]) < abs(d - (int)delta_list[upper]) ? lower : upper;
		}

		int		fix(int pred, int offset, uint8 bit_depth) const {
			return idx2val(min(val2idx(pred) + offset, bits(bit_depth)));
		}

	};

	//cp = camera parameters?
	struct per_cp {
		ue	ref_voi;
		se	scale, off, inv_scale_plus_scale, inv_off_plus_off;
		bool	read(bitreader &r, bool cp_in_slice_segment_header) {
			return r.read(ref_voi) && (cp_in_slice_segment_header || r.read(scale, off, inv_scale_plus_scale, inv_off_plus_off));
		}

		int DepthToDisparityB(int d, int bit_depth, int precision) const {
			int	log2Div		= bit_depth - 1 + precision;
			return (scale * d + (off << bit_depth) + ((1 << log2Div) >> 1)) >> log2Div;
		}
		int DepthToDisparityF(int d, int bit_depth, int precision) const {
			int	log2Div		= bit_depth - 1 + precision;
			int	invOffset	= ((inv_off_plus_off - off) << bit_depth) + ((1 << log2Div) >> 1);
			int	invScale	= inv_scale_plus_scale - scale;
			return (invScale * d + invOffset) >> log2Div;
		}

	};

	struct vps {
		ue	cp_precision;
		dynamic_array<dynamic_array<per_cp>>	cps;

		bool	read(bitreader &r, int num_views) {
			r.read(cp_precision);
			cps.resize(num_views);
			for (auto& i : cps.slice(1)) {
				i.resize(r.get(6));
				if (!i.empty()) {
					bool cp_in_slice_segment_header = r.get_bit();
					for (auto& j : i)
						j.read(r, cp_in_slice_segment_header);
				}
			}
			return true;
		}

		const per_cp*	get_cp(int vo1, int vo2) const {
			for (auto &j : cps[vo1]) {
				if (j.ref_voi == vo2)
					return &j;
			}
			return nullptr;
		}
	};

	struct sps {
		struct inter_view {
			bool	iv_di_mc_enabled;
			bool	iv_mv_scal_enabled;
			as_minus<ue,3>	log2_sub_pb_size;//log2_ivmc_sub_pb_size or log2_texmc_sub_pb_size
			union {
				struct {
					bool	iv_res_pred_enabled;
					bool	depth_ref_enabled;
					bool	vsp_mc_enabled;
					bool	dbbp_enabled;
				};
				struct {
					bool	tex_mc_enabled;
					bool	intra_contour_enabled;
					bool	intra_dc_only_wedge_enabled;
					bool	cqt_cu_part_pred_enabled;
					bool	inter_dc_only_enabled;
					bool	skip_intra_enabled;
				};
			};
			bool	read_common(bitreader &r) { return r.read(iv_di_mc_enabled, iv_mv_scal_enabled); }
			bool	read0(bitreader &r) { return read_common(r) && r.read(log2_sub_pb_size, iv_res_pred_enabled, depth_ref_enabled, vsp_mc_enabled, dbbp_enabled); }
			bool	read1(bitreader &r) { return read_common(r) && r.read(tex_mc_enabled, log2_sub_pb_size, intra_contour_enabled, intra_dc_only_wedge_enabled, cqt_cu_part_pred_enabled, inter_dc_only_enabled, skip_intra_enabled); }
		};

		inter_view	iv[2];
		bool	read(bitreader &r) { return iv[0].read0(r) && iv[1].read1(r); }
	};

	struct pps {
		dynamic_array<optional<depth_lookup>>	dlts;

		depth_lookup*	get_dlt(int i) const {
			return i < dlts.size() ? dlts[0].exists_ptr() : nullptr;
		}

		bool	read(bitreader &r) {
			if (r.get_bit()) {
				dlts.resize(r.get(6) + 1);
				auto	bit_depth = r.get(4) + 8;
				for (auto &i : dlts) {
					if (r.get_bit())
						i.put().read(r, bit_depth);
				}
			}
			return true;
		}
	};

	struct shdr {
		bool	in_comp_pred				= false;
		bool	slice_ic_enabled			= false;
		bool	slice_ic_disabled_merge_zero_idx = false;

		bool	depth_layer;
		bool	IvMvScalEnabledFlag			= false;
		bool	cp_available				= false;

		bool read(bitreader &r, const multilayer_extension::vps *vpsml, const vps *vps3d, const sps *sps3d, const multilayer_extension::layer *layer, bool allRefCmpLayersAvail) {
			depth_layer			= layer->depth_layer();

			int		view_order	= layer->view_order_idx();
			auto&	iv			= sps3d->iv[depth_layer];

			in_comp_pred	= allRefCmpLayersAvail && (depth_layer
				? iv.intra_contour_enabled || iv.cqt_cu_part_pred_enabled || iv.tex_mc_enabled
				: iv.vsp_mc_enabled || iv.dbbp_enabled || iv.depth_ref_enabled
			) && r.get_bit();

			IvMvScalEnabledFlag	= iv.iv_mv_scal_enabled && view_order;

			if (!depth_layer && iv.depth_ref_enabled && in_comp_pred) {
				cp_available = true;
				for (auto &i : vpsml->layers) {
					if (!vps3d->get_cp(view_order, i.view_order_idx())) {
						cp_available = false;
						break;
					}
				}
			}


			return true;
		}
		bool read2(bitreader &r) {
			slice_ic_enabled					= r.get_bit();
			slice_ic_disabled_merge_zero_idx	= slice_ic_enabled && r.get_bit();
			return true;
		}

	};
};
#endif	//HEVC_3D
#else
#undef HEVC_3D
#endif	//HEVC_ML

#ifdef HEVC_SCC

//-----------------------------------------------------------------------------
// SCC
//-----------------------------------------------------------------------------

struct scc_extension {
	struct qp_offsets {
		bool			present;
		as_plus<se, 5>	y;
		as_plus<se, 5>	cb;
		as_plus<se, 3>	cr;
		bool	read(bitreader &r) { return r.read(present, y, cb, cr); }
	};

	struct palette_predictor_initializers : dynamic_array<array<uint16, 3>> {
		bool	read(bitreader &r, uint32 num, int bit_depth_luma, int bit_depth_chroma) {
			resize(num);
			for (int i = 0, n = bit_depth_chroma ? 3 : 1; i < n; i++) {
				uint32	nbits	= i == 0 ? bit_depth_luma : bit_depth_chroma;
				for (auto &e : *this)
					e[i] = r.get(nbits);
			}
			return true;
		}
	};

	//7.3.2.2.3 Sequence parameter set screen content coding extension syntax
	struct sps {
		bool				curr_pic_ref;
		bool				palette_mode;
		uint32				palette_max_size;
		uint32				delta_palette_max_predictor_size;
		optional<palette_predictor_initializers>	ppi;
		u<2>	motion_vector_resolution_control;
		bool	intra_boundary_filtering_disabled;

		bool	read(bitreader &r, CHROMA chroma_format, int bit_depth_luma, int bit_depth_chroma) {
			r.read(curr_pic_ref, palette_mode);
			if (palette_mode) {
				r.read(palette_max_size, delta_palette_max_predictor_size);
				if (r.get_bit())
					ppi.put().read(r, r.getu() + 1, bit_depth_luma, chroma_format == CHROMA_MONO ? 0 : bit_depth_chroma);
			}
			return r.read(motion_vector_resolution_control, intra_boundary_filtering_disabled);
		}
	};

	//7.3.2.3.3 Picture parameter set screen content coding extension syntax
	struct pps {
		bool					curr_pic_ref;
		optional<qp_offsets>	qp_offsets;
		optional<palette_predictor_initializers>	ppi;

		bool	read(bitreader &r, CHROMA chroma_format, int bit_depth_luma, int bit_depth_chroma) {
			r.read(curr_pic_ref, qp_offsets);
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
	};

	struct shdr {
		bool	use_integer_mv = false;
	};

};
#endif //HEVC_SCC

} //namespace h265
#endif //SCALABILITY_H