#include "h265.h"
#include "hashes/md5.h"
#include "crc32.h"

namespace h265 {

template<> struct SEI::T<SEI::buffering_period> {
	struct delays {
		uint32	cpb_delay_offset;
		uint32	dpb_delay_offset;
	};
	struct initial {
		uint32	cpb_removal_delay;
		uint32	cpb_removal_offset;
		uint32	alt_cpb_removal_delay;
		uint32	alt_cpb_removal_offset;
	};
	golomb0<uint32>		bp_seq_parameter_set_id;
	optional<delays>	delays;
	bool				concatenation_flag;
	uint32				au_cpb_removal_delay_delta_minus1;
	dynamic_array<initial>	nal_initial;
	dynamic_array<initial>	vcl_initial;
	bool				use_alt_cpb__params_flag;
	bool	read(bitreader&& r) {
		//b.read(bp_seq_parameter_set_id, delays
		return false;
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::buffering_period>;

// D.2.3 Picture timing
template<> struct SEI::T<SEI::pic_timing> {
	compact<uint8,4>	pic_struct;
	compact<uint8,2>	source_scan_type;
	bool				duplicate_flag;

	uint32				au_cpb_removal_delay_minus1;
	uint32				pic_dpb_output_delay;

	uint32					pic_dpb_output_du_delay;
	optional<uint32>		du_common_cpb_removal_delay_increment_minus1;
	dynamic_array<uint32>	du_cpb_removal_delay_increment_minus1;

	bool	read(bitreader&& r) {
		r.read(pic_struct, source_scan_type, duplicate_flag);
	#if 0
		if (CpbDpbDelaysPresentFlag) {
			r.read(au_cpb_removal_delay_minus1, pic_dpb_output_delay);
			if (sub_pic_hrd_params_present_flag && sub_pic_cpb_params_in_pic_timing_sei_flag ) {
				num_decoding_units_minus1 = r.getu();

			}
		}
	#endif
		return true;
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::pic_timing>;

template<> struct SEI::T<SEI::user_data_unregistered> {
	GUID	uuid;
	malloc_block	user_data;
	bool	read(memory_reader& r) {
		return r.read(uuid)
			&& user_data.read(r, r.remaining());
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::user_data_unregistered>;

template<> struct SEI::T<SEI::recovery_point> {
	golomb0<int>	recovery_poc_cnt;
	bool			exact_match;
	bool			broken_link;
	bool	read(bitreader&& r) {
		return r.read(recovery_poc_cnt, exact_match, broken_link);
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::recovery_point>;

typedef CRC_def<uint16, 0x8408, false, false>			CRC_CCITT;//8408 0x1021

uint16 crc_ccitt(uint8 c, uint16 crc) {
	for(int i = 0; i < 8; ++i) {
		uint16	msb		= (crc >> 15) & 1;
		uint16	bit		= (c >> (7 - i)) & 1;
		crc = (((crc << 1) + bit) & 0xffff) ^ (msb * 0x1021);
	}
	return crc;
}

uint16 crc_ccitt(const uint8 *p, size_t len, uint16 crc) {
	while (len--)
		crc = crc_ccitt(*p++, crc);
	return crc;
}


template<> struct SEI::T<SEI::decoded_picture_hash> {
	enum TYPE : uint8 {
		MD5			= 0,
		CRC			= 1,
		CHECKSUM	= 2
	};
	TYPE	type;
	malloc_block	data;

	union {
		uint8		md5[3][16];
		uint16		crc[3];
		uint32		checksum[3];
	};

	static constexpr uint8 xor_mask(int x, int y) { uint16 t = x ^ y; return uint8(t ^ (t >> 8)); }

	bool	read(memory_reader& r) {
		r.read(type);
		data	= malloc_block::unterminated(r);
		return true;
		//switch (type) {
		//	case MD5:		return r.read(md5);
		//	case CRC:		return r.read(crc);
		//	case CHECKSUM:	return r.read(checksum);
		//}
		//return false;
	}
	bool process(image* _img) const {
		//return true;
		auto	img = (image_base*)_img;
		// Do not check SEI on pictures that are not output - hash may be wrong, because of a broken link (BLA)
		// This happens, for example in conformance stream RAP_B, where a EOS-NAL appears before a CRA (POC=32)
		if (!img->is_output)
			return true;
		
		auto	ck_size = type == MD5 ? 16 : type == CRC ? 2 : 4;
		if (data.size() != num_channels(img->chroma_format) * ck_size)
			return false;

		const uint8	*p	= data;
		for (int i = 0, n = num_channels(img->chroma_format); i < n; i++, p += ck_size) {
			int		bit_depth	= img->get_bit_depth(i);

			switch (type) {
				case MD5: {
					iso::MD5		m;
					if (bit_depth > 8) {
						for (auto y : img->get_plane_block<uint16>(i))
							m.write(y);
					} else {
						for (auto y : img->get_plane_block<uint8>(i))
							m.write(y);
					}
					auto	md51	= m.digest();
					if (memcmp(p, &md51, sizeof(md51)))
						return false;
					break;
				}

				case CRC: {
					uint16			c = -1;
					if (bit_depth > 8) {
						for (auto y : img->get_plane_block<uint16>(i))
							c = crc_ccitt((uint8*)y.begin(), y.size() * 2, c);
					} else {
						for (auto y : img->get_plane_block<uint8>(i))
							c = crc_ccitt(y, y.size(), c);
					}
					c = crc_ccitt(0, c);
					c = crc_ccitt(0, c);
					if (c != *(const uint16be*)p)
						return false;
					break;
				}

				case CHECKSUM: {
					uint32	sum = 0;
					int		y	= 0;
					if (bit_depth > 8) {
						for (auto row : img->get_plane_block<uint16>(i)) {
							int	x = 0;
							for (auto i : row)
								sum += i ^ (xor_mask(x++, y) * 0x1001);
							++y;
						}
					} else {
						for (auto row : img->get_plane_block<uint8>(i)) {
							int	x = 0;
							for (auto i : row)
								sum += i ^ xor_mask(x++, y);
							++y;
						}
					}
					if (sum != *(const uint32be*)p)
						return false;
					break;
				}
			}
		}
		ISO_OUTPUT("SEI::decoded_picture_hash passed\n");
		return true;
	}
};
template struct SEI::T1<SEI::decoded_picture_hash>;

//G.14.2.4
template<> struct SEI::T<SEI::depth_representation_info> {
	enum Representation {
		uniform_inverse_Z	= 0,
		uniform_disparity	= 1,
		uniform_Z			= 2,
		nonuniform_disparity= 3
	};
	enum { //probaby need to reverse
		has_z_near	= 1 << 0,
		has_z_far	= 1 << 1,
		has_d_min	= 1 << 2,
		has_d_max	= 1 << 3,
	};
	uint8			flags;
	Representation	representation;
	double			z_near, z_far, d_min, d_max;
	uint32			disparity;
	malloc_block	nonlinear_model;

	static double getf(bitreader& r) {
		int s		= r.get(1);
		int e		= r.get(7);
		int mlen	= r.get(5) + 1;
		int m		= r.get(mlen);
		return iord(m << (32 - mlen), e + iord::E_OFF, s).f();
	}

	bool	read(memory_reader& r0) {
		bitreader	r(r0.get_block(maximum));
		flags			= r.get(4);
		representation	= (Representation)r.getu();
		disparity		= flags & (has_d_min | has_d_max) ? r.getu() : 0;

		z_near	= flags & has_z_near	? getf(r) : 0;
		z_far	= flags & has_z_far		? getf(r) : 0;
		d_min	= flags & has_d_min		? getf(r) : 0;
		d_max	= flags & has_d_max		? getf(r) : 0;

		if (representation == nonuniform_disparity) {
			// TODO: load non-uniform response curve
		}
		return true;
	}
	bool process(image* img) const { return false; }
};
template struct SEI::T1<SEI::depth_representation_info>;

template<> struct SEI::T<SEI::ambient_viewing_environment> {
	uint32be	illuminance;
	uint16be	light_x;
	uint16be	light_y;
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::ambient_viewing_environment>;


// D.2.4 Pan-scan rectangle
template<> struct SEI::T<SEI::pan_scan_rect> {
	golomb0<uint32>			id;
	bool					cancel;
	dynamic_array<int32x4>	offsets;
	bool					persistence;

	bool	read(memory_reader& r0, decoder_context *ctx) {
		bitreader	r(r0.get_block(maximum));
		r.read(id, cancel);
		if (!cancel) {
			offsets.resize(r.getu());
			for (auto &i : offsets)
				i = { r.gets(), r.gets(), r.gets(), r.gets() };
			persistence = r.get_bit();
		}
		return true;
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::pan_scan_rect>;

// D.2.6 User data registered by Recommendation ITU-T T.35
template<> struct SEI::T<SEI::user_data_registered_itu_t_t35> {
	uint16			country_code;
	malloc_block	payload;

	bool	read(memory_reader& r) {
		country_code = r.getc();
		if (country_code == 0xff)
			country_code += r.getc();
		return payload.read(r, r.remaining());
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::user_data_registered_itu_t_t35>;

// D.2.9 Scene information
template<> struct SEI::T<SEI::scene_info> {
	struct info {
		bool	prev_scene_id_valid_flag;
		golomb0<uint32>	scene_id;
		golomb0<uint32>	scene_transition_type;
		golomb0<uint32>	second_scene_id;
		bool	read(bitreader& r) {
			return r.read(prev_scene_id_valid_flag, scene_id, scene_transition_type)
				&& (scene_transition_type <= 3 ||  r.read(second_scene_id));
		}
	};
	optional<info>	info;

	bool	read(memory_reader& r0) {
		return bitreader(r0.get_block(maximum)).read(info);
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::scene_info>;

// D.2.10 Picture snapshot
template<> struct SEI::T<SEI::picture_snapshot> {
	golomb0<uint32>	id;
	bool	read(memory_reader& r0) {
		return bitreader(r0.get_block(maximum)).read(id);
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::picture_snapshot>;

// D.2.11 Progressive refinement segment start
template<> struct SEI::T<SEI::progressive_refinement_segment_start> {
	golomb0<uint32>	id;
	golomb0<uint32>	pic_order_cnt_delta;
	bool	read(memory_reader& r0) {
		return bitreader(r0.get_block(maximum)).read(id, pic_order_cnt_delta);
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::progressive_refinement_segment_start>;

// D.2.12 Progressive refinement segment end
template<> struct SEI::T<SEI::progressive_refinement_segment_end> {
	golomb0<uint32>	id;
	bool	read(memory_reader& r0) {
		return bitreader(r0.get_block(maximum)).read(id);
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::progressive_refinement_segment_end>;

// D.2.13 Film grain characteristics
template<> struct SEI::T<SEI::film_grain_characteristics> {
	struct colour_description {
		compact<uint8,3>	bit_depth_luma_minus8;
		compact<uint8,3>	bit_depth_chroma_minus8;
		bool	full_range_flag;
		uint8	colour_primaries;
		uint8	transfer_characteristics;
		uint8	matrix_coeffs;
		bool	read(bitreader& r) {
			return r.read(bit_depth_luma_minus8, bit_depth_chroma_minus8, full_range_flag, colour_primaries, transfer_characteristics, matrix_coeffs);
		}
	};
	struct model {
		interval<uint8>		intensity_interval;
		dynamic_array<int>	values;
	};

	bool	cancel;
	uint8	id;

	optional<colour_description>	colour_description;
	compact<uint8,2>				blending_mode_id;
	compact<uint8,4>				log2_scale_factor;
	optional<dynamic_array<model>>	models[3];
	bool							persistence;

	bool	read(memory_reader& r0) {
		bitreader	r(r0.get_block(maximum));
		cancel	= r.get_bit();
		if (!cancel) {
			r.read(id, colour_description, blending_mode_id, log2_scale_factor);
			for (uint8	present = r.get(3); present; present = clear_lowest(present)) {
				auto	&m = models[2 - lowest_set_index(present)].put();
				m.resize(r.get(8) + 1);
				uint8	num_values = r.get(3) + 1;
				for (auto& i : m) {
					i.intensity_interval.a = r.get(8);
					i.intensity_interval.b = r.get(8);
					i.values.resize(num_values);
					for (auto &j : i.values)
						j = r.gets();
				}
			}
		}
		return true;
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::film_grain_characteristics>;

// D.2.14 Post-filter hint
template<> struct SEI::T<SEI::post_filter_hint> {
	golomb0<uint32>		stride;
	compact<uint8,2>	type;
	dynamic_array<golomb0<int>>	value[3];

	bool	read(memory_reader& r0) {
		bitreader	r(r0.get_block(maximum));
		golomb0<uint32>	sizey;
		r.read(sizey, stride, type);
		for (auto &i : value)
			i.read(r, sizey * stride);
		return true;
	}
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::post_filter_hint>;

template struct SEI::T1<SEI::no_display>;

// D.2.21 Active parameter sets
template<> struct SEI::T<SEI::active_parameter_sets> {
	compact<uint8,4>	id;
	bool	self_contained_cvs;
	bool	no_parameter_set_update;

	dynamic_array<golomb0<uint32>>	active_seq_parameter_set_id;
	dynamic_array<golomb0<uint32>>	layer_sps_idx;

	bool	read(bitreader r) {
		r.read(id, self_contained_cvs, no_parameter_set_update);
		active_seq_parameter_set_id.resize(r.getu() + 1);
		r.read(active_seq_parameter_set_id);
		return true;
	}
#if 0
	bool	read(memory_reader& r0) {
		bitreader	r(r0.get_block(maximum));
		r.read(id, self_contained_cvs, no_parameter_set_update);
		active_seq_parameter_set_id.resize(r.getu() + 1);
		r.read(active_seq_parameter_set_id);
	#if 0
		int	max_layers = min(vps_max_layers, 64);
		layer_sps_idx.resize(max_layers)
		for( i = vps_base_layer_internal_flag; i < max_layers; i++ )
			layer_sps_idx[i] = r.getu();
	#endif
		return true;
	}
#endif
	bool	process(image* img) const { return false; }
};
template struct SEI::T1<SEI::active_parameter_sets>;

//F.14.2.8 Alpha channel information SEI message syntax
template<> struct SEI::T<SEI::alpha_channel_info> {
	enum USE {
		USE_MULT	= 0,	// the decoded samples of the associated primary picture should be multiplied by the interpretation sample values of the auxiliary coded picture in the display process after output from the decoding process
		USE_NOMULT	= 1,	// the decoded samples of the associated primary picture should not be multiplied by the interpretation sample values of the auxiliary coded picture in the display process after output from the decoding process
		USE_UNSPEC	= 2,	// the usage of the auxiliary picture is unspecified
	};

	bool				cancel;
	compact<USE,3>		use;
	as_minus<u<3>, 8>	bit_depth;
	uint16				transparent_value;
	uint16				opaque_value;
	bool				incr_flag;
	optional<bool>		clip_type_flag;

	bool	read(memory_reader& r0) {
		bitreader	r(r0.get_block(maximum));
		cancel	= r.get_bit();
		if (!cancel) {
			r.read(use, bit_depth);
			transparent_value	= r.get(bit_depth + 1);
			opaque_value		= r.get(bit_depth + 1);
			r.read(incr_flag, clip_type_flag);
		}
		return true;
	}
	bool	process(image* img) const {
		if (!img)
			return true;

		//do post thing
		return true;
	}
};
template struct SEI::T1<SEI::alpha_channel_info>;

} //namespace h265 {

#if 0
//-----------------------------------------------------------------------------
//	TBD
//-----------------------------------------------------------------------------
 
// D.2.15 Tone mapping information
template<> struct SEI::T<SEI::tone_mapping_info> {
uint32	tone_map_id;
optional {
	bool	tone_map_persistence_flag;
	uint8	coded_data_bit_depth;
	uint8	target_bit_depth;
	uint32	tone_map_model_id;
	if( tone_map_model_id = = 0 ) {
	uint32	min_value;
	uint32	max_value;
	} else if( tone_map_model_id = = 1 ) {
	uint32	sigmoid_midpoint;
	uint32	sigmoid_width;
	} else if( tone_map_model_id 2:		array<uint32>	start_of_coded_interval[ i ];
	else if( tone_map_model_id = = 3 ) {
	array{
	uint32	coded_pivot_value[ i ];
	uint32	target_pivot_value[ i ];
	}
	} else if( tone_map_model_id = = 4 ) {
	uint8	camera_iso_speed_idc;
	uint32	camera_iso_speed_value;
	uint8	exposure_idx_idc;
	uint32	exposure_idx_value;
	bool	exposure_compensation_value_sign_flag;
	uint16	exposure_compensation_value_numerator;
	uint16	exposure_compensation_value_denom_idc;
	uint32	ref_screen_luminance_white;
	uint32	extended_range_white_level;
	uint16	nominal_black_level_code_value;
	uint16	nominal_white_level_code_value;
	uint16	extended_white_level_code_value;
	}
}
};

// D.2.16 Frame packing arrangement
template<> struct SEI::T<SEI::frame_packing_arrangement> {
uint32	frame_packing_arrangement_id;
optional{
	uint8	frame_packing_arrangement_type;
	bool	quincunx_sampling_flag;
	uint8	content_interpretation_type;
	bool	spatial_flipping_flag;
	bool	frame0_flipped_flag;
	bool	field_views_flag;
	bool	current_frame_is_frame0_flag;
	bool	frame0_self_contained_flag;
	bool	frame1_self_contained_flag;
	uint8	frame0_grid_position_x;
	uint8	frame0_grid_position_y;
	uint8	frame1_grid_position_x;
	uint8	frame1_grid_position_y;
	uint8	frame_packing_arrangement_reserved_byte;
	bool	frame_packing_arrangement_persistence_flag;
};
bool	upsampled_aspect_ratio_flag;
}
// D.2.17 Display orientation
template<> struct SEI::T<SEI::display_orientation> {
optional{
bool	hor_flip;
bool	ver_flip;
uint16	anticlockwise_rotation;
bool	display_orientation_persistence_flag;
}
};

// D.2.18 Green metadata
//The syntax for green metadata SEI message is specified in ISO/IEC 23001-11 (Green metadata). Green metadata facilitates reduced power consumption in decoders, encoders, displays and in media selection.

// D.2.19 Structure of pictures information
template<> struct SEI::T<SEI::structure_of_pictures_info> {
uint32	sop_seq_parameter_set_id;
array{
uint8	sop_vcl_nut[ i ];
uint8	sop_temporal_id[ i ];
uint32	sop_short_term_rps_idx[ i ];
if( i > 0 )
int	sop_poc_delta[ i ];
}
};

// D.2.20 Decoded picture hash
template<> struct SEI::T<SEI::decoded_picture_hash> {
}


// D.2.22 Decoding unit information
template<> struct SEI::T<SEI::decoding_unit_info> {
uint32	decoding_unit_idx;
uint32	du_spt_cpb_removal_delay_increment;
bool	dpb_output_du_delay_present_flag;
uint32	pic_spt_dpb_output_du_delay;
};

// D.2.23 Temporal sub-layer zero index
template<> struct SEI::T<SEI::temporal_sub_layer_zero_idx> {
	uint8	temporal_sub_layer_zero_idx;
	uint8	irap_pic_id;
};

// D.2.24 Scalable nesting
template<> struct SEI::T<SEI::scalable_nesting> {
bool	bitstream_subset_flag;
bool	nesting_op_flag;
{
bool	default_op_flag;
uint32	nesting_num_ops_minus1;
array{
uint8	nesting_max_temporal_id_plus1[ i ];
uint32	nesting_op_idx[ i ];
}
}
 else
 {
bool	all_layers_flag;
if( !all_layers_flag ) {
array<uint8>	nesting_layer_id[ i ];
}
}
while( !byte_aligned( ) )
bool	nesting_zero_bit /* equal to 0 */;
do
sei_message( )
while( more_rbsp_data( ) )
}
// D.2.25 Region refresh information
template<> struct SEI::T<SEI::region_refresh_info> {
	bool	refreshed_region_flag;
};

// D.2.27 Time code
template<> struct SEI::T<SEI::time_code> {
array {
optional{
bool	units_field_based_flag[ i ];
uint8	counting_type[ i ];
bool	full_timestamp_flag[ i ];
bool	discontinuity_flag[ i ];
bool	cnt_dropped_flag[ i ];
u<9>	n_frames[ i ] ;
if( full_timestamp_flag[ i ] ) {
uint8	seconds_value[ i ] /* 0..59 */;
uint8	minutes_value[ i ] /* 0..59 */;
uint8	hours_value[ i ] /* 0..23 */;
} else {
bool	seconds_flag[ i ];
if( seconds_flag[ i ] ) {
uint8	seconds_value[ i ] /* 0..59 */;
bool	minutes_flag[ i ];
if( minutes_flag[ i ] ) {
uint8	minutes_value[ i ] /* 0..59 */;
bool	hours_flag[ i ];
if( hours_flag[ i ] )
uint8	hours_value[ i ] /* 0..23 */;
}
}
}
uint8	time_offset_length[ i ];
if( time_offset_length[ i ] > 0 )
int	time_offset_value[ i ];
}
}
};

// D.2.28 Mastering display colour volume
template<> struct SEI::T<SEI::mastering_display_colour_volume> {
array{3,
uint16	display_primaries_x[ c ];
uint16	display_primaries_y[ c ];
};
uint16	white_point_x;
uint16	white_point_y;
uint32	max_display_mastering_luminance;
uint32	min_display_mastering_luminance;
};

// D.2.29 Segmented rectangular frame packing arrangement
template<> struct SEI::T<SEI::segmented_rect_frame_packing_arrangement> {
optional{
uint8	segmented_rect_content_interpretation_type;
bool	segmented_rect_frame_packing_arrangement_persistence_flag;
}
};

// D.2.30 Temporal motion-constrained tile sets
template<> struct SEI::T<SEI::temporal_motion_constrained_tile_sets> {
bool	mc_all_tiles_exact_sample_value_match_flag;
bool	each_tile_one_tile_set_flag;
if( !each_tile_one_tile_set_flag ) {
bool	limited_tile_set_display_flag;
uint32	num_sets_in_message_minus1;
for( i = 0; i <= num_sets_in_message_minus1; i++ ) {
uint32	mcts_id[ i ];
if( limited_tile_set_display_flag )
bool	display_tile_set_flag[ i ];
uint32	num_tile_rects_in_set_minus1[ i ];
for( j = 0; j <= num_tile_rects_in_set_minus1[ i ]; j++ ) {
uint32	top_left_tile_idx[ i ][ j ];
uint32	bottom_right_tile_idx[ i ][ j ];
}
if( !mc_all_tiles_exact_sample_value_match_flag )
bool	mc_exact_sample_value_match_flag[ i ];
bool	mcts_tier_level_idc_present_flag[ i ];
if( mcts_tier_level_idc_present_flag[ i ] ) {
bool	mcts_tier_flag[ i ];
uint8	mcts_level_idc[ i ];
}
}
} else {
bool	max_mcs_tier_level_idc_present_flag;
if( mcts_max_tier_level_idc_present_flag ) {
bool	mcts_max_tier_flag;
uint8	mcts_max_level_idc;
}
}
}
// D.2.31 Chroma resampling filter hint
template<> struct SEI::T<SEI::chroma_resampling_filter_hint> {
uint8	ver_chroma_filter_idc;
uint8	hor_chroma_filter_idc;
bool	ver_filtering_field_processing_flag;
if( ver_chroma_filter_idc = = 1 | | hor_chroma_filter_idc = = 1 ) {
uint32	target_format_idc;
if( ver_chroma_filter_idc = = 1 ) {
uint32	num_vertical_filters;
for( i = 0; i < num_vertical_filters; i++ ) {
uint32	ver_tap_length_minus1[ i ];
for( j = 0; j <= ver_tap_length_minus1[ i ]; j++ )
int	ver_filter_coeff[ i ][ j ];
}
}
if( hor_chroma_filter_idc = = 1 ) {
uint32	num_horizontal_filters;
for( i = 0; i < num_horizontal_filters; i++ ) {
uint32	hor_tap_length_minus1[ i ];
for( j = 0; j <= hor_tap_length_minus1[ i ]; j++ )
int	hor_filter_coeff[ i ][ j ];
}
}
}
}
// D.2.32 Knee function information
template<> struct SEI::T<SEI::knee_function_info> {
uint32	knee_function_id;
bool	knee_function_cancel_flag;
if( !knee_function_cancel_flag ) {
bool	knee_function_persistence_flag;
uint32	input_d_range;
uint32	input_disp_luminance;
uint32	output_d_range;
uint32	output_disp_luminance;
uint32	num_knee_points_minus1;
for( i = 0; i <= num_knee_points_minus1; i++ ) {
u<10>	input_knee_point[ i ] ;
u<10>	output_knee_point[ i ] ;
}
}
}
// D.2.33 Colour remapping information
template<> struct SEI::T<SEI::colour_remapping_info> {
uint32	colour_remap_id;
bool	colour_remap_cancel_flag;
if( !colour_remap_cancel_flag ) {
bool	colour_remap_persistence_flag;
bool	colour_remap_video_signal_info_present_flag;
if( colour_remap_video_signal_info_present_flag ) {
bool	colour_remap_full_range_flag;
uint8	colour_remap_primaries;
uint8	colour_remap_transfer_function;
uint8	colour_remap_matrix_coefficients;
}
uint8	colour_remap_input_bit_depth;
uint8	colour_remap_output_bit_depth;
for( c = 0; c < 3; c++ ) {
uint8	pre_lut_num_val_minus1[ c ];
if( pre_lut_num_val_minus1[ c ] > 0 )
for( i = 0; i <= pre_lut_num_val_minus1[ c ]; i++ ) {
uint32	pre_lut_coded_value[ c ][ i ];
uint32	pre_lut_target_value[ c ][ i ];
}
}
bool	colour_remap_matrix_present_flag;
if( colour_remap_matrix_present_flag ) {
uint8	log2_matrix_denom;
for( c = 0; c < 3; c++ )
for( i = 0; i < 3; i++ )
int	colour_remap_coeffs[ c ][ i ];
}
for( c = 0; c < 3; c++ ) {
uint8	post_lut_num_val_minus1[ c ];
if( post_lut_num_val_minus1[ c ] > 0 )
for( i = 0; i <= post_lut_num_val_minus1[ c ]; i++ ) {
uint32	post_lut_coded_value[ c ][ i ];
uint32	post_lut_target_value[ c ][ i ];
}
}
}
}
// D.2.34 Deinterlaced field identification
template<> struct SEI::T<SEI::deinterlaced_field_indentification> {
bool	deinterlaced_picture_source_parity_flag;
}
// D.2.35 Content light level information
template<> struct SEI::T<SEI::content_light_level_info> {
uint16	max_content_light_level;
uint16	max_pic_average_light_level;
}
// D.2.36 Dependent random access point indication
template<> struct SEI::T<SEI::dependent_rap_indication> {
}
// D.2.37 Coded region completion
template<> struct SEI::T<SEI::coded_region_completion> {
uint32	next_segment_address;
if( next_segment_address > 0 )
bool	independent_slice_segment_flag;
}
// D.2.38 Alternative transfer characteristics information
template<> struct SEI::T<SEI::alternative_transfer_characteristics > {
uint8	preferred_transfer_characteristics;
}
// D.2.39 Ambient viewing environment
template<> struct SEI::T<SEI::ambient_viewing_environment> {
uint32	ambient_illuminance;
uint16	ambient_light_x;
uint16	ambient_light_y;
};

// D.2.40 Content colour volume
template<> struct SEI::T<SEI::content_colour_volume> {
bool	ccv_cancel_flag;
if( !ccv_cancel_flag ) {
bool	ccv_persistence_flag;
bool	ccv_primaries_present_flag;
bool	ccv_min_luminance_value_present_flag;
bool	ccv_max_luminance_value_present_flag;
bool	ccv_avg_luminance_value_present_flag;
uint8	ccv_reserved_zero_2bits;
if( ccv_primaries_present_flag )
for( c = 0; c < 3; c++ ) {
int	ccv_primaries_x[ c ];
int	ccv_primaries_y[ c ];
}
if( ccv_min_luminance_value_present_flag )
uint32	ccv_min_luminance_value;
if( ccv_max_luminance_value_present_flag )
uint32	ccv_max_luminance_value;
if( ccv_avg_luminance_value_present_flag )
uint32	ccv_avg_luminance_value;
}
}
D.2.41 Syntax of omnidirectional video specific SEI messages
// D.2.41.1Equirectangular projection
template<> struct SEI::T<SEI::equirectangular_projection> {
bool	erp_cancel_flag;
if( !erp_cancel_flag ) {
bool	erp_persistence_flag;
bool	erp_guard_band_flag;
uint8	erp_reserved_zero_2bits;
if( erp_guard_band_flag = = 1 ) {
uint8	erp_guard_band_type;
uint8	erp_left_guard_band_width;
uint8	erp_right_guard_band_width;
}
}
}
// D.2.41.2Cubemap projection
template<> struct SEI::T<SEI::cubemap_projection> {
bool	cmp_cancel_flag;
if( !cmp_cancel_flag )
bool	cmp_persistence_flag;
}
// D.2.41.3Fisheye video information
template<> struct SEI::T<SEI::fisheye_video_info> {
bool	fisheye_cancel_flag;
if( !fisheye_cancel_flag ) {
bool	fisheye_persistence_flag;
uint8	fisheye_view_dimension_idc;
uint8	fisheye_reserved_zero_3bits;
uint8	fisheye_num_active_areas_minus1;
for( i = 0; i <= fisheye_num_active_areas_minus1; i++ ) {
uint32	fisheye_circular_region_centre_x[ i ];
uint32	fisheye_circular_region_centre_y[ i ];
uint32	fisheye_rect_region_top[ i ];
uint32	fisheye_rect_region_left[ i ];
uint32	fisheye_rect_region_width[ i ];
uint32	fisheye_rect_region_height[ i ];
uint32	fisheye_circular_region_radius[ i ];
uint32	fisheye_scene_radius[ i ];
int	fisheye_camera_centre_azimuth[ i ];
int	fisheye_camera_centre_elevation[ i ];
int	fisheye_camera_centre_tilt[ i ];
uint32	fisheye_camera_centre_offset_x[ i ];
uint32	fisheye_camera_centre_offset_y[ i ];
uint32	fisheye_camera_centre_offset_z[ i ];
uint32	fisheye_field_of_view[ i ];
uint16	fisheye_num_polynomial_coeffs[ i ];
for( j = 0; j < fisheye_num_polynomial_coeffs[ i ]; j++ )
int	fisheye_polynomial_coeff[ i ][ j ];
}
}
}
// D.2.41.4Sphere rotation
template<> struct SEI::T<SEI::sphere_rotation> {
bool	sphere_rotation_cancel_flag;
if( !sphere_rotation_cancel_flag ) {
bool	sphere_rotation_persistence_flag;
uint8	sphere_rotation_reserved_zero_6bits;
int	yaw_rotation;
int	pitch_rotation;
int	roll_rotation;
}
}
// D.2.41.5Region-wise packing
template<> struct SEI::T<SEI::regionwise_packing> {
bool	rwp_cancel_flag;
if( !rwp_cancel_flag ) {
bool	rwp_persistence_flag;
bool	constituent_picture_matching_flag;
uint8	rwp_reserved_zero_5bits;
uint8	num_packed_regions;
uint32	proj_picture_width;
uint32	proj_picture_height;
uint16	packed_picture_width;
uint16	packed_picture_height;
for( i = 0; i < num_packed_regions; i++ ) {
uint8	rwp_reserved_zero_4bits[ i ];
uint8	rwp_transform_type[ i ];
bool	rwp_guard_band_flag[ i ];
uint32	proj_region_width[ i ];
uint32	proj_region_height[ i ];
uint32	proj_region_top[ i ];
uint32	proj_region_left[ i ];
uint16	packed_region_width[ i ];
uint16	packed_region_height[ i ];
uint16	packed_region_top[ i ];
uint16	packed_region_left[ i ];
if( rwp_guard_band_flag[ i ] ) {
uint8	rwp_left_guard_band_width[ i ];
uint8	rwp_right_guard_band_width[ i ];
uint8	rwp_top_guard_band_height[ i ];
uint8	rwp_bottom_guard_band_height[ i ];
bool	rwp_guard_band_not_used_for_pred_flag[ i ];
for( j = 0; j < 4; j++ )
uint8	rwp_guard_band_type[ i ][ j ];
uint8	rwp_guard_band_reserved_zero_3bits[ i ];
}
}
}
}
// D.2.41.6Omnidirectional viewport
template<> struct SEI::T<SEI::omni_viewport> {
u<10>	omni_viewport_id ;
bool	omni_viewport_cancel_flag;
if( !omni_viewport_cancel_flag ) {
bool	omni_viewport_persistence_flag;
uint8	omni_viewport_cnt_minus1;
for( i = 0; i <= omni_viewport_cnt_minus1; i++ ) {
int	omni_viewport_azimuth_centre[ i ];
int	omni_viewport_elevation_centre[ i ];
int	omni_viewport_tilt_centre[ i ];
uint32	omni_viewport_hor_range[ i ];
uint32	omni_viewport_ver_range[ i ];
}
}
}
// D.2.42 Regional nesting
template<> struct SEI::T<SEI::regional_nesting> {
uint16	regional_nesting_id;
uint8	regional_nesting_num_rect_regions;
for( i = 0; i < regional_nesting_num_rect_regions; i++ ) {
uint8	regional_nesting_rect_region_id[ i ];
uint16	regional_nesting_rect_left_offset[ i ];
uint16	regional_nesting_rect_right_offset[ i ];
uint16	regional_nesting_rect_top_offset[ i ];
uint16	regional_nesting_rect_bottom_offset[ i ];
}
uint8	num_sei_messages_in_regional_nesting_minus1;
for( i = 0; i <= num_sei_messages_in_regional_nesting_minus1; i++ ) {
uint8	num_regions_for_sei_message[ i ];
for(j = 0; j < num_regions_for_sei_message[ i ]; j++ )
uint8	regional_nesting_sei_region_idx[ i ][ j ];
sei_message( )
}
}
// D.2.43 Motion-constrained tile sets extraction information sets
template<> struct SEI::T<SEI::mcts_extraction_info_sets> {
uint32	num_info_sets_minus1;
for( i = 0; i <= num_info_sets_minus1; i++ ) {
uint32	num_mcts_sets_minus1[ i ];
for( j = 0; j <= num_mcts_sets_minus1[ i ]; j++ ) {
uint32	num_mcts_in_set_minus1[ i ][ j ];
for( k = 0; k <= num_mcts_in_set_minus1[ i ][ j ]; k++ )
uint32	idx_of_mcts_in_set[ i ][ j ][ k ];
}
bool	slice_reordering_enabled_flag[ i ];
if( slice_reordering_enabled_flag[ i ] ) {
uint32	num_slice_segments_minus1[ i ];
for( j = 0; j <= num_slice_segments_minus1[ i ]; j++ )
uint32	output_slice_segment_address[ i ][ j ];
}
uint32	num_vps_in_info_set_minus1[ i ];
for( j = 0; j <= num_vps_in_info_set_minus1[ i ]; j++ )
uint32	vps_rbsp_data_length[ i ][ j ];
uint32	num_sps_in_info_set_minus1[ i ];
for( j = 0; j <= num_sps_in_info_set_minus1[ i ]; j++ )
uint32	sps_rbsp_data_length[ i ][ j ];
uint32	num_pps_in_info_set_minus1[ i ];
for( j = 0; j <= num_pps_in_info_set_minus1[ i ]; j++ ) {
uint8	pps_nuh_temporal_id_plus1[ i ][ j ];
uint32	pps_rbsp_data_length[ i ][ j ];
}
while( !byte_aligned( ) )
mcts_alignment_bit_equal_to_zero f(1)
for( j = 0; j <= num_vps_in_info_set_minus1[ i ]; j++ )
for( k = 0; k < vps_rbsp_data_length[ i ][ j ]; k++ )
uint8	vps_rbsp_data_byte[ i ][ j ][ k ];
for( j = 0; j <= num_sps_in_info_set_minus1[ i ]; j++ )
for( k = 0; k < sps_rbsp_data_length[ i ][ j ]; k++ )
uint8	sps_rbsp_data_byte[ i ][ j ][ k ];
for( j = 0; j <= num_pps_in_info_set_minus1[ i ]; j++ )
for( k = 0; k < pps_rbsp_data_length[ i ][ j ]; k++ )
uint8	pps_rbsp_data_byte[ i ][ j ][ k ];
}
}
// D.2.44 Motion-constrained tile sets extraction information nesting
template<> struct SEI::T<SEI::mcts_extraction_info_nesting> {
bool	all_mcts_flag;
if( !all_mcts_flag ) {
uint32	num_associated_mcts_minus1;
for( i = 0; i <= num_associated_mcts_minus1; i++ )
uint32	idx_of_associated_mcts[ i ];
}
uint32	num_sei_messages_in_mcts_extraction_nesting_minus1;
while( !byte_aligned( ) )
bool	mcts_nesting_zero_bit /* equal to 0 */;
for( i = 0; i <= num_sei_messages_in_mcts_extraction_nesting_minus1; i++ )
sei_message( )
}
// D.2.45 SEI manifest
template<> struct SEI::T<SEI::sei_manifest> {
uint16	manifest_num_sei_msg_types;
for( i = 0; i < manifest_num_sei_msg_types; i++ ) {
uint16	manifest_sei_payload_type[ i ];
uint8	manifest_sei_description[ i ];
}
}
// D.2.46 SEI prefix indication
template<> struct SEI::T<SEI::sei_prefix_indication> {
uint16	prefix_sei_payload_type;
uint8	num_sei_prefix_indications_minus1;
for( i = 0; i <= num_sei_prefix_indications_minus1; i++ ) {
uint16	num_bits_in_prefix_indication_minus1[ i ];
for( j = 0; j <= num_bits_in_prefix_indication_minus1[ i ]; j++ )
bool	sei_prefix_data_bit[ i ][ j ];
while( !byte_aligned( ) )
byte_alignment_bit_equal_to_one /* equal to 1 */ f(1)
}
}
// D.2.47 Annotated regions
template<> struct SEI::T<SEI::annotated_regions> {
bool	ar_cancel_flag;
if(!ar_cancel_flag) {
bool	ar_not_optimized_for_viewing_flag;
bool	ar_true_motion_flag;
bool	ar_occluded_object_flag;
bool	ar_partial_object_flag_present_flag;
bool	ar_object_label_present_flag;
bool	ar_object_confidence_info_present_flag;
if( ar_object_confidence_info_present_flag )
uint8	ar_object_confidence_length_minus1;
if( ar_object_label_present_flag ) {
bool	ar_object_label_language_present_flag;
if( ar_object_label_language_present_flag ) {
while( !byte_aligned( ) )
ar_bit_equal_to_zero /* equal to 0 */ f(1)
ar_object_label_language st(v)
}
uint32	ar_num_label_updates;
for( i = 0; i < ar_num_label_updates; i++ ) {
uint32	ar_label_idx[ i ];
bool	ar_label_cancel_flag;
LabelAssigned[ ar_label_idx[ i ] ] = !ar_label_cancel_flag
if( !ar_label_cancel_flag ) {
while( !byte_aligned( ) )
ar_bit_equal_to_zero /* equal to 0 */ f(1)
ar_label[ ar_label_idx[ i ] ] st(v)
}
}
}
uint32	ar_num_object_updates;
for( i = 0; i < ar_num_object_updates; i++ ) {
uint32	ar_object_idx[ i ];
bool	ar_object_cancel_flag;
ObjectTracked[ ar_object_idx[ i ] ] = !ar_object_cancel_flag
if( !ar_object_cancel_flag ) {
if( ar_object_label_present_flag ) {
bool	ar_object_label_update_flag;
if( ar_object_label_update_flag )
uint32	ar_object_label_idx[ ar_object_idx[ i ] ];
}
bool	ar_bounding_box_update_flag;
if( ar_bounding_box_update_flag ) {
bool	ar_bounding_box_cancel_flag;
ObjectBoundingBoxAvail[ ar_object_idx[ i ] ] = !ar_bounding_box_cancel_flag
if( !ar_bounding_box_cancel_flag ) {
uint16	ar_bounding_box_top[ ar_object_idx[ i ] ];
uint16	ar_bounding_box_left[ ar_object_idx[ i ] ];
uint16	ar_bounding_box_width[ ar_object_idx[ i ] ];
uint16	ar_bounding_box_height[ ar_object_idx[ i ] ];
if( ar_partial_object_flag_present_flag )
bool	ar_partial_object_flag[ ar_object_idx[ i ] ];
if( ar_object_confidence_info_present_flag )
uint32	ar_object_confidence[ ar_object_idx[ i ] ];
}
}
}
}
}
}
// D.2.48 Shutter interval information
template<> struct SEI::T<SEI::shutter_interval_info> {
uint32	sii_time_scale;
bool	fixed_shutter_interval_within_clvs_flag;
if( fixed_shutter_interval_within_clvs_flag )
uint32	sii_num_units_in_shutter_interval;
else {
uint8	sii_max_sub_layers_minus1;
for( i = 0; i <= sii_max_sub_layers_minus1; i++ )
uint32	sub_layer_num_units_in_shutter_interval[ i ];
}
}


//F.14.2.3 Layers not present SEI message syntax
template<> struct SEI::T<SEI::layers_not_present> {
	u<4>	lnp_sei_active_vps_id ;
for( i = 0; i <= MaxLayersMinus1; i++ ) 
bool	layer_not_present_flag[ i ] ;
}

//F.14.2.4 Inter-layer constrained tile sets SEI message syntax
template<> struct SEI::T<SEI::inter_layer_constrained_tile_sets> {
	bool	il_all_tiles_exact_sample_value_match_flag ;
bool	il_one_tile_per_tile_set_flag ;
if( !il_one_tile_per_tile_set_flag ) {
	ue	il_num_sets_in_message_minus1 ;
		if( il_num_sets_in_message_minus1 )
			bool	skipped_tile_set_present_flag ;
			numSignificantSets = il_num_sets_in_message_minus1
			- skipped_tile_set_present_flag + 1
			for( i = 0; i < numSignificantSets; i++ ) {
				ue	ilcts_id[ i ] ;
					ue	il_num_tile_rects_in_set_minus1[ i ] ;
					for( j = 0; j <= il_num_tile_rects_in_set_minus1[ i ]; j++ ) {
						ue	il_top_left_tile_idx[ i ][ j ] ;
							ue	il_bottom_right_tile_idx[ i ][ j ] ;
					}
				u<2>	ilc_idc[ i ] ;
					if( !il_all_tiles_exact_sample_value_match_flag )
						bool	il_exact_sample_value_match_flag[ i ] ;
			}
} else
u<2>	all_tiles_ilc_idc ;
}
//F.14.2.5 Bitstream partition nesting SEI message syntax
template<> struct SEI::T<SEI::bsp_nesting> {
	ue	sei_ols_idx ;
ue	sei_partitioning_scheme_idx ;
ue	bsp_idx ;
ue	num_seis_in_bsp_minus1 ;
while( !byte_aligned( ) )
bool	bsp_nesting_zero_bit /* equal to 0 */ ;
for( i = 0; i <= num_seis_in_bsp_minus1; i++ )
sei_message( )
}
//F.14.2.6 Bitstream partition initial arrival time SEI message syntax
template<> struct SEI::T<SEI::bsp_initial_arrival_time> {
	psIdx = sei_partitioning_scheme_idx
if( nalInitialArrivalDelayPresent )
for( i = 0; i < BspSchedCnt[ sei_ols_idx ][ psIdx ][ MaxTemporalId[ 0 ] ]; i++ )
	nal_initial_arrival_delay[ i ] u(v)
	if( vclInitialArrivalDelayPresent )
		for( i = 0; i < BspSchedCnt[ sei_ols_idx ][ psIdx ][ MaxTemporalId[ 0 ] ]; i++ )
			vcl_initial_arrival_delay[ i ] u(v)
}
//F.14.2.7 Sub-bitstream property SEI message syntax
template<> struct SEI::T<SEI::sub_bitstream_property> {
	u<4>	sb_property_active_vps_id ;
ue	num_additional_sub_streams_minus1 ;
for( i = 0; i <= num_additional_sub_streams_minus1; i++ ) {
	u<2>	sub_bitstream_mode[ i ] ;
		ue	ols_idx_to_vps[ i ] ;
		u<3>	highest_sublayer_id[ i ] ;
		u<16>	avg_sb_property_bit_rate[ i ] ;
		u<16>	max_sb_property_bit_rate[ i ] ;
}
}

//F.14.2.9 Overlay information SEI message syntax
template<> struct SEI::T<SEI::overlay_info> {
	bool	overlay_info_cancel_flag ;
if( !overlay_info_cancel_flag ) {
	ue	overlay_content_aux_id_minus128 ;
		ue	overlay_label_aux_id_minus128 ;
		ue	overlay_alpha_aux_id_minus128 ;
		ue	overlay_element_label_value_length_minus8 ;
		ue	num_overlays_minus1 ;
		for( i = 0; i <= num_overlays_minus1; i++ ) {
			ue	overlay_idx[ i ] ;
				bool	language_overlay_present_flag[ i ] ;
				u<6>	overlay_content_layer_id[ i ] ;
				bool	overlay_label_present_flag[ i ] ;
				if( overlay_label_present_flag[ i ] ) 
					u<6>	overlay_label_layer_id[ i ] ;
					bool	overlay_alpha_present_flag[ i ] ;
					if( overlay_alpha_present_flag[ i ] )
						u<6>	overlay_alpha_layer_id[ i ] ;
						if( overlay_label_present_flag[ i ] ) {
							ue	num_overlay_elements_minus1[ i ] ;
								for( j = 0; j <= num_overlay_elements_minus1[ i ]; j++ ) {
									overlay_element_label_min[ i ][ j ] u(v)
										overlay_element_label_max[ i ][ j ] u(v)
								}
						}
		}
	while( !byte_aligned( ) )
		overlay_zero_bit /* equal to 0 */ f(1)
		for( i = 0; i <= num_overlays_minus1; i++ ) {
			if( language_overlay_present_flag[ i ] )
				overlay_language[ i ] st(v)
				overlay_name[ i ] st(v)
				if( overlay_label_present_flag[ i ] )
					for( j = 0; j <= num_overlay_elements_minus1[ i ]; j++ )
						overlay_element_name[ i ][ j ] st(v)
		}
	bool	overlay_info_persistence_flag ;
}
}
//F.14.2.10Temporal motion vector prediction constraints SEI message syntax
template<> struct SEI::T<SEI::temporal_mv_prediction_constraints> {
	bool	prev_pics_not_used_flag ;
bool	no_intra_layer_col_pic_flag ;
}
//F.14.2.11Frame-field information SEI message syntax
template<> struct SEI::T<SEI::frame_field_info> {
	u<4>	ffinfo_pic_struct ;
u<2>	ffinfo_source_scan_type ;
bool	ffinfo_duplicate_flag ;
}


//G.14.2.33D reference displays information SEI message syntax
template<> struct SEI::T<SEI::three_dimensional_reference_displays_info> {
	ue	prec_ref_display_width ;
bool	ref_viewing_distance_flag ;
if( ref_viewing_distance_flag )
ue	prec_ref_viewing_dist ;
ue	num_ref_displays_minus1 ;
for( i = 0; i <= num_ref_displays_minus1; i++ ) {
	ue	left_view_id[ i ] ;
		ue	right_view_id[ i ] ;
		u<6>	exponent_ref_display_width[ i ] ;
		mantissa_ref_display_width[ i ] u(v)
		if( ref_viewing_distance_flag ) {
			u<6>	exponent_ref_viewing_distance[ i ] ;
				mantissa_ref_viewing_distance[ i ] u(v)
		}
	bool	additional_shift_present_flag[ i ] ;
		if( additional_shift_present_flag[ i ] )
			u<10>	num_sample_shift_plus512[ i ] ;
}
bool	three_dimensional_reference_displays_extension_flag ;
}
//G.14.2.5Multiview scene information SEI message syntax
template<> struct SEI::T<SEI::multiview_scene_info> {min_disparity se(v)
ue	max_disparity_range ;
}
//G.14.2.6Multiview acquisition information SEI message syntax
template<> struct SEI::T<SEI::multiview_acquisition_info> {
	bool	intrinsic_param_flag ;
bool	extrinsic_param_flag ;
if( intrinsic_param_flag )  {
	bool	intrinsic_params_equal_flag ;
		ue	prec_focal_length ;
		ue	prec_principal_point ;
		ue	prec_skew_factor ;
		for( i = 0; i <= intrinsic_params_equal_flag ? 0 : numViewsMinus1; i++ ) {
			bool	sign_focal_length_x[ i ] ;
				u<6>	exponent_focal_length_x[ i ] ;
				mantissa_focal_length_x[ i ] u(v)
				bool	sign_focal_length_y[ i ] ;
				u<6>	exponent_focal_length_y[ i ] ;
				mantissa_focal_length_y[ i ] u(v)
				bool	sign_principal_point_x[ i ] ;
				u<6>	exponent_principal_point_x[ i ] ;
				mantissa_principal_point_x[ i ] u(v)
				bool	sign_principal_point_y[ i ] ;
				u<6>	exponent_principal_point_y[ i ] ;
				mantissa_principal_point_y[ i ] u(v)
				bool	sign_skew_factor[ i ] ;
				u<6>	exponent_skew_factor[ i ] ;
				mantissa_skew_factor[ i ] u(v)
		}
}
if( extrinsic_param_flag ) {
	ue	prec_rotation_param ;
		ue	prec_translation_param ;
		for( i = 0; i <= numViewsMinus1; i++ )
			for( j = 0; j < 3; j++ ) { /* row */
				for( k = 0; k < 3; k++ ) { /* column */
					bool	sign_r[ i ][ j ][ k ] ;
						u<6>	exponent_r[ i ][ j ][ k ] ;
						mantissa_r[ i ][ j ][ k ] u(v)
				}
				bool	sign_t[ i ][ j ] ;
					u<6>	exponent_t[ i ][ j ] ;
					mantissa_t[ i ][ j ] u(v)
			}
}
}
//G.14.2.7Multiview view position SEI message syntax
template<> struct SEI::T<SEI::multiview_view_position> {
	ue	num_views_minus1 ;
for( i = 0; i <= num_views_minus1; i++ )
ue	view_position[ i ] ;
}


//I.14.2.3 Alternative depth information SEI message syntax
template<> struct SEI::T<SEI::alternative_depth_info> {
	bool	alternative_depth_info_cancel_flag ;
if( alternative_depth_info_cancel_flag = = 0 ) {
	u<2>	depth_type ;
		if( depth_type = = 0 ) {
			ue	num_constituent_views_gvd_minus1 ;
				bool	depth_present_gvd_flag ;
				bool	z_gvd_flag ;
				bool	intrinsic_param_gvd_flag ;
				bool	rotation_gvd_flag ;
				bool	translation_gvd_flag ;
				if( z_gvd_flag ) 
					for( i = 0; i <= num_constituent_views_gvd_minus1 + 1; i++ ) {
						bool	sign_gvd_z_near_flag[ i ] ;
							u<7>	exp_gvd_z_near[ i ] ;
							u<5>	man_len_gvd_z_near_minus1[ i ] ;
							man_gvd_z_near[ i ] u(v)
							bool	sign_gvd_z_far_flag[ i ] ;
							u<7>	exp_gvd_z_far[ i ] ;
							u<5>	man_len_gvd_z_far_minus1[ i ] ;
							man_gvd_z_far[ i ] u(v)
					}
			if( intrinsic_param_gvd_flag ) {
				ue	prec_gvd_focal_length ;
					ue	prec_gvd_principal_point ;
			}
			if( rotation_gvd_flag )
				ue	prec_gvd_rotation_param ;
				if( translation_gvd_flag )
					ue	prec_gvd_translation_param ;
					for( i = 0; i <= num_constituent_views_gvd_minus1 + 1; i++ ) {
						if( intrinsic_param_gvd_flag ) {
							bool	sign_gvd_focal_length_x[ i ] ;
								u<6>	exp_gvd_focal_length_x[ i ] ;
								man_gvd_focal_length_x[ i ] u(v)
								bool	sign_gvd_focal_length_y[ i ] ;
								u<6>	exp_gvd_focal_length_y[ i ] ;
								man_gvd_focal_length_y[ i ] u(v)
								bool	sign_gvd_principal_point_x[ i ] ;
								u<6>	exp_gvd_principal_point_x[ i ] ;
								man_gvd_principal_point_x[ i ] u(v)
								bool	sign_gvd_principal_point_y[ i ] ;
								u<6>	exp_gvd_principal_point_y[ i ] ;
								man_gvd_principal_point_y[ i ] u(v)
						}
						if( rotation_gvd_flag ) 
							for( j = 0; j < 3; j++ ) /* row */
								for( k = 0; k < 3; k++ ) { /* column */
									bool	sign_gvd_r[ i ][ j ][ k ] ;
										u<6>	exp_gvd_r[ i ][ j ][ k ] ;
										man_gvd_r[ i ][ j ][ k ] u(v)
								}
						bool	if( translation_gvd_flag ) {sign_gvd_t_x[ i ] ;
							u<6>	exp_gvd_t_x[ i ] ;
							man_gvd_t_x[ i ] u(v)
						}
					}
		}
	if( depth_type = = 1 ) {
		min_offset_x_int se(v)
			u<8>	min_offset_x_frac ;
			max_offset_x_int se(v)
			u<8>	max_offset_x_frac ;
			bool	offset_y_present_flag ;
			if( offset_y_present_flag ){
				min_offset_y_int se(v)
					u<8>	min_offset_y_frac ;
					max_offset_y_int se(v)
					u<8>	max_offset_y_frac ;
			}
		bool	warp_map_size_present_flag ;
			if( warp_map_size_present_flag ) {
				ue	warp_map_width_minus2 ;
					ue	warp_map_height_minus2 ;
			}
	}
}
}
#endif
