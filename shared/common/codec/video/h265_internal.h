#ifndef H265_INTERNAL_H
#define H265_INTERNAL_H

#include "h265.h"
#include "base/algorithm.h"

namespace h265 {

//CTU	coding tree unit	16x16 - 64x64	contains a CTB per plane
//CTB	coding tree block					split into CBs
//CB	coding block		8x8 - 64x64		split into PBs
//PB	prediction block	1 to 4 per CB
//TB	transform block		division of CB	unit of DCT transform

//CU	coding unit			CB per plane	inter/intra prediction unit

//Pictures are divided into slices and tiles
//A slice is a sequence of one or more slice segments starting with an independent slice segment and containing all subsequent dependent slice segments
//A slice segment is a sequence of CTUs (non rectangular)
//A tile is a rectangular sequence of CTUs
//Either all CTUs in a slice segment belong to the same tile, or all CTUs in a tile belong to the same slice segment
//
//When a picture is coded using three separate colour planes, a slice contains only CTBs of one colour component
//When not using separate_colour_plane, each CTB of a picture is contained in exactly one slice
//When using separate_colour_plane, each CTB of a colour component is contained in exactly one slice

//-----------------------------------------------------------------------------
// constants
//-----------------------------------------------------------------------------

enum ScanOrder {
	SCAN_DIAG		= 0,
	SCAN_HORIZ		= 1,
	SCAN_VERT		= 2,
	SCAN_TRAVERSE	= 3,
};

enum SliceType {
	SLICE_TYPE_B = 0,
	SLICE_TYPE_P = 1,
	SLICE_TYPE_I = 2
};

constexpr int	num_ref_lists(SliceType t)	{ return 2 - t; }

enum PredMode {
	MODE_INTRA, MODE_INTER, MODE_SKIP
};

//  16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32
//  15 \                     ^                     /
//  14    \                  |                 /
//  13       \               |              /
//  12-         \            |           /
//  11             \         |        /
//  10                \      |     /
//   9                   \   |  /
//   8 <---------------------x
//   7                    /   
//   6                 /      
//   5              /         
//   4           /            
//   3        /               
//   2     /                  
//   1  /                     
//   0                        

enum IntraPredMode : uint8 {
	INTRA_PLANAR	= 0,
	INTRA_DC		= 1,

	INTRA_ANGLE_0	= 2,	INTRA_ANGLE_1	= 3,	INTRA_ANGLE_2	= 4,	INTRA_ANGLE_3	= 5,
	INTRA_ANGLE_4	= 6,	INTRA_ANGLE_5	= 7,	INTRA_ANGLE_6	= 8,	INTRA_ANGLE_7	= 9,
	INTRA_ANGLE_8	= 10,	INTRA_ANGLE_9	= 11,	INTRA_ANGLE_10	= 12,	INTRA_ANGLE_11	= 13,
	INTRA_ANGLE_12	= 14,	INTRA_ANGLE_13	= 15,	INTRA_ANGLE_14	= 16,	INTRA_ANGLE_15	= 17,
	INTRA_ANGLE_16	= 18,	INTRA_ANGLE_17	= 19,	INTRA_ANGLE_18	= 20,	INTRA_ANGLE_19	= 21,
	INTRA_ANGLE_20	= 22,	INTRA_ANGLE_21	= 23,	INTRA_ANGLE_22	= 24,	INTRA_ANGLE_23	= 25,
	INTRA_ANGLE_24	= 26,	INTRA_ANGLE_25	= 27,	INTRA_ANGLE_26	= 28,	INTRA_ANGLE_27	= 29,
	INTRA_ANGLE_28	= 30,	INTRA_ANGLE_29	= 31,	INTRA_ANGLE_30	= 32,	INTRA_ANGLE_31	= 33,
	INTRA_ANGLE_32	= 34,

#ifdef HEVC_3D
	INTRA_SINGLE2	= 35,
	INTRA_SINGLE3	= 36,
#endif

	INTRA_PER_TU,

#ifdef HEVC_3D
	INTRA_WEDGE		= 37,
	INTRA_CONTOUR	= 38,
#endif

	INTRA_UNKNOWN	= 255,
};

constexpr int			to_angle(IntraPredMode m)	{ return m - INTRA_ANGLE_0; }
constexpr IntraPredMode	from_angle(int a)			{ return IntraPredMode((a & 31) + INTRA_ANGLE_0); }
constexpr int			steepness(IntraPredMode m)	{ return min(abs(m - INTRA_ANGLE_24), abs(m - INTRA_ANGLE_8)); }
constexpr bool			is_per_tu(IntraPredMode m)	{ return m < INTRA_PER_TU; }
#ifdef HEVC_3D
constexpr bool			is_dim(IntraPredMode m)		{ return m == INTRA_WEDGE || m == INTRA_CONTOUR; }
#endif

enum IntraChromaMode {
	INTRA_CHROMA_PLANAR_OR_34	= 0,
	INTRA_CHROMA_ANGLE_24_OR_32	= 1,
	INTRA_CHROMA_ANGLE_8_OR_32	= 2,
	INTRA_CHROMA_DC_OR_34		= 3,
	INTRA_CHROMA_LIKE_LUMA		= 4
};


/*
2Nx2N           2NxN             Nx2N            NxN
+-------+       +-------+       +---+---+       +---+---+
|       |       |       |       |   |   |       |   |   |
|       |       |_______|       |   |   |       |___|___|
|       |       |       |       |   |   |       |   |   |
|       |       |       |       |   |   |       |   |   |
+-------+       +-------+       +---+---+       +---+---+

AMP: asymmetric motion partitions

2NxnU           2NxnD           nLx2N           nRx2N
+-------+       +-------+       +-+-----+       +-----+-+
|_______|       |       |       | |     |       |     | |
|       |       |       |       | |     |       |     | |
|       |       |_______|       | |     |       |     | |
|       |       |       |       | |     |       |     | |
+-------+       +-------+       +-+-----+       +-----+-+
*/
enum PartMode {
	PART_2Nx2N		= 0,
	PART_2NxN		= 1,
	PART_Nx2N		= 2,
	PART_NxN		= 3,
	PART_2NxnU		= 4,
	PART_2NxnD		= 5,
	PART_nLx2N		= 6,
	PART_nRx2N		= 7,
	PART_UNKNOWN	= 8,
};

constexpr int fix_offset(PartMode part_mode, int offset) {
	return !(part_mode & 4) ? offset : (part_mode & 1 ? offset + (offset >> 1) : offset >> 1);
}
constexpr int get_offset(PartMode part_mode, int size) {
	return fix_offset(part_mode, size >> 1);
}
constexpr bool has_vsplit(PartMode part_mode) {
	return part_mode & 2;
	//return is_any(part_mode, PART_Nx2N, PART_NxN, PART_nLx2N, PART_nRx2N);
}
constexpr bool has_hsplit(PartMode part_mode) {
	return is_any(part_mode, PART_2NxN, PART_NxN, PART_2NxnU, PART_2NxnD);//1,3,4,5
}

inline int	get_subindex(PartMode part_mode, int size, int x, int y) {
	if (part_mode == PART_2Nx2N)
		return 0;
	auto	offset	= get_offset(part_mode, size);
	return ((has_vsplit(part_mode) ? x : y) >= offset) + (part_mode == PART_NxN && y > offset ? 2 : 0);
}

//-----------------------------------------------------------------------------
// templates
//-----------------------------------------------------------------------------

template<typename T> struct scaled_block {
	T*		data			= nullptr;
	int		width_in_units	= 0;
	int		height_in_units	= 0;
	int		log2unitSize	= 0;
	int		data_size		= 0;

	scaled_block()	{}
	scaled_block& operator=(const scaled_block &b)	{
		alloc(b.width_in_units, b.height_in_units, b.log2unitSize);
		copy_n(b.data, data, width_in_units * height_in_units);
		return *this;
	}

	bool	alloc(int w, int h, int _log2unitSize) {
		int size = w * h;
		if (size != data_size) {
			delete[] data;
			data		= new T[size];
			data_size	= size;
		}
		width_in_units	= w;
		height_in_units	= h;
		log2unitSize	= _log2unitSize;
		return !!data;
	}
	bool	alloc2(int w, int h, int _log2unitSize) {
		return alloc(shift_right_ceil(w, _log2unitSize), shift_right_ceil(h, _log2unitSize), _log2unitSize);
	}

	~scaled_block()	{ delete[] data; }

	auto		index0(int x,int y)	const	{ return x + y * width_in_units; }
	auto		index(int x,int y)	const	{ return index0(x >> log2unitSize, y >> log2unitSize); }
	const T&	get0(int x,int y)	const	{ return data[index0(x, y)]; }
	T&			get0(int x,int y)			{ return data[index0(x, y)]; }
	const T&	get(int x,int y)	const	{ return data[index(x, y)]; }
	T&			get(int x,int y)			{ return data[index(x, y)]; }
	const T&	get(int16x2 v)		const	{ return get(v.x, v.y); }
	const T&	operator[](int idx) const	{ return data[idx]; }
	T&			operator[](int idx)			{ return data[idx]; }
	auto		total()				const	{ return width_in_units * height_in_units; }
	
	auto		all() {
		return make_block(data, width_in_units, height_in_units);
	}
	auto		sub_block(int i, int log2blkSize) {
		int width	= 1 << (log2blkSize - log2unitSize);
		return make_strided_block(data + i, width, width_in_units * sizeof(T), width);
	}
	auto		sub_block(int x, int y, int log2BlkWidth) {
		return sub_block(index(x, y), log2BlkWidth);
	}
	auto		sub_block(int x, int y, int w, int h) {
		return make_strided_block(data + index(x, y), w >> log2unitSize, width_in_units * sizeof(T), h >> log2unitSize);
	}
};

template<typename T, int S, int L> struct edge_history {
	T	data[1 << (S - L)];
	void	reset(const T &t)						{ fill(data, t);}
	T		get(int i)			const				{ return data[(i & bits(S)) >> L]; }
	void	set(int i, int log2size, const T &t)	{ fill_n(data + ((i & bits(S)) >> L), 1 << (log2size - L), t); }
};

template<int S, int L> struct edge_history<bool, S, L> {
	uint_bits_t<1 << (S - L)>	data;
	void	reset()							{ data = 0; }
	bool	get(int i)		const			{ return data & (1 << ((i & bits(S)) >> L)); }
	void	set(int i, int log2CbSize)		{ data |=  bits(1 << (log2CbSize - L), (i & bits(S)) >> L); }
	void	clear(int i, int log2CbSize)	{ data &= ~bits(1 << (log2CbSize - L), (i & bits(S)) >> L); }
};

template<typename T, int S, int L> struct edge_history2 : array<edge_history<T, S, L>, 2> {
	//0 is left, 1 is above
	void	set(int x, int y, int log2size, const T &v) {
		t[0].set(y, log2size, v);
		t[1].set(x, log2size, v);
	}
};

//-----------------------------------------------------------------------------
//	Motion
//-----------------------------------------------------------------------------

typedef int16x2 MotionVector;

struct PB_info {
	enum {
		SUB			= 1 << 0,
		VSP			= 1 << 1,
		IV			= 1 << 2,
		INVALID		= 1 << 7
	};
	uint16			flags		= INVALID;	// log2subsize-3 in top 8 bits
	int8			refIdx[2]	= {-1, -1};	// index into RefPicList (-1 -> not used)
	MotionVector	mv[2];					// the absolute motion vectors

	constexpr int	dir() const {
		return (refIdx[0] >= 0) | (refIdx[1] >= 0 ? 2 : 0);
	}
	constexpr bool	valid() const {
		return !(flags & INVALID);
	}
	bool operator==(const PB_info &b) const {
		for (int i = 0; i < 2; i++) {
			if (refIdx[i] != b.refIdx[i] || (refIdx[i] >= 0 && any(mv[i] != b.mv[i])))
				return false;
		}
		return true;
	}
	bool operator!=(const PB_info &b) const {
		return !(*this == b);
	}
	void		set(bool X, int idx, MotionVector _mv) {
		refIdx[X]	= idx;
		mv[X]		= _mv;
	}
	PB_info		cleared_flags() const {
		PB_info	b = *this;
		b.flags	= 0;
		return b;
	}
};

constexpr auto offset_pos(int16x2 pos, MotionVector v) {
	return pos + ((v + 2) >> 2);
}
constexpr auto offset_pos3(int16x2 pos, MotionVector v) {
	return (pos << 3) + (v << 1);
}

//-----------------------------------------------------------------------------
//	Weights
//-----------------------------------------------------------------------------

struct pred_weight {
	int16	weight	= 0;
	int16	offset	= 0;
};

//-----------------------------------------------------------------------------
// scaling
//-----------------------------------------------------------------------------

struct ScalingList {
	static uint8 default_4x4[16];
	static uint8 default_8x8_intra[64];
	static uint8 default_8x8_inter[64];

	uint8 size0[6][4 * 4];
	uint8 size1[6][8 * 8];
	uint8 size2[6][16 * 16];
	uint8 size3[6][32 * 32];

	auto	get_matrix(int log2TrSize, int matrixID) const {
		switch (log2TrSize) {
			case 2:		return size0[matrixID];
			case 3:		return size1[matrixID];
			case 4:		return size2[matrixID];
			default:	return size3[matrixID];
		}
	}
	void set_default();
	bool read(bitreader &r);
};

//-----------------------------------------------------------------------------
// Deblocking
//-----------------------------------------------------------------------------

struct Deblocking {
	bool	overridden	= false;
	bool	disabled	= false;
	int		beta_offset	= 0;
	int		tc_offset	= 0;

	bool	read(bitreader &r) {
		overridden		= r.get_bit();
		disabled		= r.get_bit();
		if (!disabled) {
			beta_offset	= r.gets() * 2;
			tc_offset	= r.gets() * 2;
		}
		return true;
	}
	bool	read0(bitreader &r) {
		disabled		= r.get_bit();
		if (!disabled) {
			beta_offset	= r.gets() * 2;
			tc_offset	= r.gets() * 2;
		}
		return true;
	}

	uint8	beta(int y)	const;
	uint8	tc(int y)	const;

};

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
		bool	read(bitreader& r);
	};
	struct layer {
		struct nalvcl {
			as_minus<ue,1>	bit_rate;
			as_minus<ue,1>	cpb_size;
			as_minus<ue,1>	cpb_size_du;
			as_minus<ue,1>	bit_rate_du;
			bool			cbr;
			void read(bitreader& r, bool sub_pic);
		};
		bool	fixed_pic_rate_general;
		bool	fixed_pic_rate_within_cvs;
		bool	low_delay_hrd;
		uint32	cpb_cnt;
		uint32	elemental_duration_in_tc;
		nalvcl	nal[32];
		nalvcl	vcl[32];

		void read(bitreader& r);
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

	bool	read(bitreader& r, bool common, int max_sub_layers);
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
		bool	read(bitreader &r);
	};

	compact<Format,3>	format		= Unspecified;
	bool				full_range	= false;
	optional<colour_description>	colour_description;
	bool	read(bitreader &r);
};

struct video_timing {
	uint32						num_units_in_tick	= 0;
	uint32						time_scale			= 0;
	optional<as_minus<ue,1>>	num_ticks_poc_diff_one;
	bool	read(bitreader& r);
};

struct video_usability_information {
	struct chroma_loc {
		ue		top_field		= 0;
		ue		bottom_field	= 0;
		bool	read(bitreader& r);
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

		bool	read(bitreader& r);
		bool	valid() const{
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

	bool	read(bitreader& r, int max_sub_layers);
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

	void	init(PROFILE _profile);
	bool	read(bitreader &r);
	bool	write(bitwriter &w) const;
};

struct ProfileLayer : Profile {
	uint8	level;
	bool	profile_present;
	bool	level_present;
	bool	read(bitreader &r)			{ return (!profile_present || Profile::read(r))  && (!level_present || r.read(level)); }
	bool	write(bitwriter &w) const	{ return (!profile_present || Profile::write(w)) && (!level_present || w.write(level)); }
};

struct profile_tier_level : static_array<ProfileLayer, MAX_TEMPORAL_SUBLAYERS> {
	void	init(Profile::PROFILE _profile, uint8 level);
	bool	read(bitreader &r, bool profile_present, uint32 max_sublayers);
	bool	write(bitwriter &w) const;
};

//-----------------------------------------------------------------------------
// Extensions
//-----------------------------------------------------------------------------

enum EXTENSION {
	EXT_RANGE		= 1 << 0,
	EXT_MULTILAYER	= 1 << 1,
	EXT_3D			= 1 << 2,
	EXT_SCC			= 1 << 3,
	EXT_4			= 1 << 4,
	EXT_5			= 1 << 5,
	EXT_6			= 1 << 6,
	EXT_7			= 1 << 7,
};

#ifdef HEVC_RANGE

//-----------------------------------------------------------------------------
// range
//-----------------------------------------------------------------------------

struct extension_range {
	struct sps {
		bool	transform_skip_rotation_enabled		= false;
		bool	transform_skip_context_enabled		= false;
		bool	implicit_rdpcm_enabled				= false;
		bool	explicit_rdpcm_enabled				= false;
		bool	extended_precision_processing		= false;
		bool	intra_smoothing_disabled			= false;
		bool	high_precision_offsets_enabled		= false;
		bool	persistent_rice_adaptation_enabled	= false;
		bool	cabac_bypass_alignment_enabled		= false;

		bool	read(bitreader &r);
	};

	struct pps {
		struct chroma_qp_offset_list : static_array<int32x2, 6> {
			uint8	delta_depth		= 0;

			bool read(bitreader &r);
		};


		uint8	log2_max_transform_skip_block_size	= 2;
		bool	cross_component_prediction_enabled	= false;
		optional<chroma_qp_offset_list>	chroma_qp_offsets;
		ue		log2_sao_offset_scale[2]			= {0, 0};

		bool	read(bitreader &r, const seq_parameter_set* sps, bool skip_enabled, int bit_depth_luma, int bit_depth_chroma);
	};
	struct shdr {
		bool	cu_chroma_qp_offset_enabled;
	};

};

#endif	//HEVC_RANGE


#ifdef HEVC_ML

//-----------------------------------------------------------------------------
// ML
//-----------------------------------------------------------------------------

struct extension_multilayer {
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
		uint8			id_in_nuh;
		uint8			max_sub_layers;

		uint8			dimension_id[16]			= {0};
		uint64			direct_dependency			= 0;
		uint64			dependency					= 0;
		uint64			predicted					= 0;
		uint32			rep_format_idx				= 0;
		bool			poc_lsb_not_present;
		uint32			direct_dependency_type[8]	= {0};
		u<3>			max_tid_il_ref_pics_plus1[8]= {7,7,7,7,7,7,7,7};

		//vui extension
		int				video_signal_info_idx		= -1;
		bool			tiles_in_use;
		bool			loop_filter_not_across_tiles;
		uint64			tile_boundaries_aligned;
		bool			wpp_in_use;
		bool			base_layer_parameter_set_compatibility;
		restriction		restrictions[8];

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

		bool read(bitreader &r, bool bit_rate_present, bool pic_rate_present);
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
		optional<array<ue, 4>>	conformance_window;

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
		dynamic_array<int>	coeffs;
		u<2>				octant_depth;
		u<2>				y_part_num_log2;
		uint16x2			adapt_threshold;
		uint8				yshift, cshift, mapping_shift;

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

		bool	read(bitreader &r);
		int		get_Y(int y, int cb, int cr)	const { return clamp(get_value(y, cb, cr, load<4>(coeffs + get_index(y, cb, cr) + 0)), 0, bits(luma_bit_depth_output)); }
		int		get_Cb(int y, int cb, int cr)	const { return clamp(get_value(y, cb, cr, load<4>(coeffs + get_index(y, cb, cr) + 4)), 0, bits(chroma_bit_depth_output)); }
		int		get_Cr(int y, int cb, int cr)	const { return clamp(get_value(y, cb, cr, load<4>(coeffs + get_index(y, cb, cr) + 8)), 0, bits(chroma_bit_depth_output)); }
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
		uint64	depth_layers;	// mask for which layers are depth

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
		bool	read(bitreader &r);
	};

	struct shdr {
		uint32		poc_msb_cycle_val;
		static_array<uint8, 8>	ref_layers;
		u<2>		poc_reset_idc;
		u<6>		poc_reset_period_id;
		bool		full_poc_reset;
		uint32		poc_lsb_val;

		bool	read(bitreader &r, const pps &pps, uint8 log2_max_pic_order_cnt_lsb);
		bool	read(bitreader &r, const vps *vpsml, const layer *layer, int temporal_id);
	};
};
#endif	//HEVC_ML

#ifdef HEVC_3D
//-----------------------------------------------------------------------------
// 3D
//-----------------------------------------------------------------------------

struct extension_3d {

	struct depth_lookup {
		dynamic_array<uint32>	delta_list;

		bool	read(bitreader &r, uint8 bit_depth, const depth_lookup *base);

		auto	idx2val(int i) const {
			return i >= 0 && i < delta_list.size() ? delta_list[i] : 0;
		}

		int		val2idx(int d) const {
			auto	i = lower_boundc(delta_list, d);
			return  i == delta_list.end()		? delta_list.size32() - 1
				:	i == delta_list.begin()		? 0
				:	delta_list.index_of(i) - (abs(d - (int)i[-1]) < abs(d - (int)i[0]));
		}

		int		fix(int pred, int offset, uint8 bit_depth) const {
			return idx2val(min(val2idx(pred) + offset, bits(bit_depth)));
		}

	};

	//cp = camera parameters?
	struct cp {
		se	scale, off, inv_scale_plus_scale, inv_off_plus_off;
		bool	read(bitreader &r) {
			return r.read(scale, off, inv_scale_plus_scale, inv_off_plus_off);
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

	struct inter_view {
		bool	di_mc_enabled;
		bool	mv_scal_enabled;
		as_minus<ue,3>	log2_sub_pb_size;//log2_ivmc_sub_pb_size or log2_texmc_sub_pb_size
		union {
			struct {	//depth_layer = 0
				bool	res_pred_enabled;
				bool	depth_ref_enabled;				// depth refinement
				bool	vsp_mc_enabled;					// view-synthesis prediction
				bool	dbbp_enabled;					// depth-based-block-part
			};
			struct {	//depth_layer = 1
				bool	tex_mc_enabled;
				bool	intra_contour_enabled;
				bool	intra_dc_only_wedge_enabled;
				bool	cqt_cu_part_pred_enabled;
				bool	inter_dc_only_enabled;
				bool	skip_intra_enabled;
			};
		};
		constexpr bool	has_inter_comp(bool depth) const {
			return depth
				? (intra_contour_enabled || cqt_cu_part_pred_enabled || tex_mc_enabled)
				: (vsp_mc_enabled || dbbp_enabled || depth_ref_enabled);
		}
		void			clear_inter_comp(bool depth) {
			if (depth)
				tex_mc_enabled	= intra_contour_enabled = cqt_cu_part_pred_enabled = false;
			else
				dbbp_enabled	= false;
		}
		constexpr bool	needs_tex(bool depth)	const	{
			return depth && (tex_mc_enabled || cqt_cu_part_pred_enabled || intra_contour_enabled);
		}
		void			no_tex(bool depth) {
			if (depth)
				tex_mc_enabled = cqt_cu_part_pred_enabled = intra_contour_enabled	= false;
		}
		inter_view()	{ clear(*this); }
		bool	read_common(bitreader &r) { return r.read(di_mc_enabled, mv_scal_enabled); }
		bool	read0(bitreader &r) { return read_common(r) && r.read(log2_sub_pb_size, res_pred_enabled, depth_ref_enabled, vsp_mc_enabled, dbbp_enabled); }
		bool	read1(bitreader &r) { return read_common(r) && r.read(tex_mc_enabled, log2_sub_pb_size, intra_contour_enabled, intra_dc_only_wedge_enabled, cqt_cu_part_pred_enabled, inter_dc_only_enabled, skip_intra_enabled); }
	};

	struct vps {
		ue	cp_precision;
		struct cp_ref : cp { ue ref_voi; };

		struct cp_refs : dynamic_array<cp_ref> {
			bool cp_in_slice_segment_header;
			const cp*	find(int vo2, cp *shdr_cps) const {
				for (auto &j : *this) {
					if (j.ref_voi == vo2) {
						if (shdr_cps && cp_in_slice_segment_header)
							return shdr_cps + index_of(j);
						return &j;
					}
				}
				return nullptr;
			}
		};
		dynamic_array<cp_refs>	cps;

		bool	read(bitreader &r, int num_views);

		const cp*	get_cp(int vo1, int vo2, cp *shdr_cps) const {
			return cps[vo1].find(vo2, shdr_cps);
		}
	};

	struct sps {
		inter_view	iv[2];
		bool	read(bitreader &r) { return iv[0].read0(r) && iv[1].read1(r); }
	};

	struct pps {
		optional<depth_lookup>	dlts[64];
		const depth_lookup*		get_dlt(int i) const { return dlts[i].exists_ptr(); }
		bool	read(bitreader &r, const extension_multilayer::vps *vpsml);
	};

	struct shdr {
		inter_view	iv;
		bool	depth_layer					= false;
		bool	ic_enabled					= false;
		bool	ic_disabled_merge_zero_idx	= false;

		ref_ptr<image>	reftex;
		int				rpref_idx[2]		= {-1, -1};
		uint64			ref_rpref_avail[2]	= {0, 0};
		uint64			view_mask			= 0;
		int				default_view		= -1;

		dynamic_array<const image*>		depth_images;
		dynamic_array<cp>				cps;

		constexpr bool	view_avail(bool X, int v)	const	{ return ref_rpref_avail[X] & bit64(v); }
		constexpr bool	has_inter_comp()			const	{ return iv.has_inter_comp(depth_layer); }
		constexpr bool	needs_tex()					const	{ return iv.needs_tex(depth_layer); }
		void			clear_inter_comp()					{ iv.clear_inter_comp(depth_layer); }
		void			set_tex(const image *tex)			{ reftex = unconst(tex); if (!tex) iv.no_tex(depth_layer); }

		void			init(const slice_segment_header* shdr, const extension_multilayer::vps *vpsml, const extension_multilayer::layer *layer, int poc);
	};
};
#endif	//HEVC_3D

#ifdef HEVC_SCC

//-----------------------------------------------------------------------------
// SCC	- screen content coding
//-----------------------------------------------------------------------------

struct extension_scc {
	struct qp_offsets {
		bool			present;
		as_plus<se, 5>	y;
		as_plus<se, 5>	cb;
		as_plus<se, 3>	cr;
		bool	read(bitreader &r) { return r.read(present, y, cb, cr); }
	};

	struct palette_predictor_initializers : dynamic_array<array<uint16, 3>> {
		bool	read(bitreader &r, uint32 num, int bit_depth_luma, int bit_depth_chroma);
	};

	//7.3.2.2.3 Sequence parameter set screen content coding extension syntax
	struct sps {
		bool				curr_pic_ref;
		bool				palette_mode	= false;
		uint32				palette_max_size;
		uint32				delta_palette_max_predictor_size;
		optional<palette_predictor_initializers>	ppi;
		u<2>	motion_vector_resolution_control;
		bool	intra_boundary_filtering_disabled;
		bool	read(bitreader &r, CHROMA chroma_format, int bit_depth_luma, int bit_depth_chroma);
	};

	//7.3.2.3.3 Picture parameter set screen content coding extension syntax
	struct pps {
		bool					curr_pic_ref;
		optional<qp_offsets>	act_qp_offsets;
		optional<palette_predictor_initializers>	ppi;
		bool	read(bitreader &r, CHROMA chroma_format, int bit_depth_luma, int bit_depth_chroma);
	};

	struct shdr {
		bool	use_integer_mv	= false;
		int32x3	act_qp_offsets	= zero;
	};

};

#endif //HEVC_SCC

//-----------------------------------------------------------------------------
// sub_layer_ordering
//-----------------------------------------------------------------------------

struct sub_layer_ordering {
	struct layer {
		ue		max_dec_pic_buffering	= 1;	// [1 ; ]
		ue		max_num_reorder_pics	= 0;	// [0 ; ]
		ue		max_latency_increase	= 0;	// 0 -> no limit, otherwise value is (x-1)

		bool	read(bitreader &r)			{ return r.read(max_dec_pic_buffering, max_num_reorder_pics, max_latency_increase); }
		bool	write(bitwriter& w) const	{ return w.write(max_dec_pic_buffering, max_num_reorder_pics, max_latency_increase); }
	};

	static_array<layer, MAX_TEMPORAL_SUBLAYERS> layers;
	bool	ordering_info_present	= false;

	bool	read(bitreader &r, uint32 max_sublayers);
	bool	write(bitwriter& w) const;
};


//-----------------------------------------------------------------------------
// VPS
//-----------------------------------------------------------------------------

struct video_parameter_set : refs<video_parameter_set> {
	u<4>					id						= 0;
	bool					base_layer_internal		= false;
	bool					base_layer_available	= false;
	as_minus<u<6>, 1>		max_layers				= 1;

	as_minus<u<3>, 1>		max_sub_layers			= 1;
	bool					temporal_id_nesting		= true;
	profile_tier_level		profile_tier_level;
	sub_layer_ordering		ordering;

	optional<video_timing>				timing;
#ifdef HEVC_ML
	optional<extension_multilayer::vps>	extension_multilayer;
#ifdef HEVC_3D
	optional<extension_3d::vps>			extension_3d;
#endif
#endif

	bool read(bitreader &r);
};

//-----------------------------------------------------------------------------
// SPS
//-----------------------------------------------------------------------------

struct seq_parameter_set : refs<seq_parameter_set> {
	enum {MAX_NUM_LT_REF_PICS_SPS = 32};

	struct pcm {
		as_minus<u<4>,1>	sample_bit_depth_luma				= 8;
		as_minus<u<4>,1>	sample_bit_depth_chroma				= 8;
		as_minus<ue,3>		log2_min_luma_coding_block_size		= 0;
		ue					log2_diff_luma_coding_block_size	= 0;
		bool				loop_filter_disable					= true;

		bool	read(bitreader& r) {
			return r.read(sample_bit_depth_luma, sample_bit_depth_chroma, log2_min_luma_coding_block_size, log2_diff_luma_coding_block_size, loop_filter_disable);
		}
		bool	valid_size(int log2CbSize) const {
			return between(log2CbSize, log2_min_luma_coding_block_size, log2_min_luma_coding_block_size + log2_diff_luma_coding_block_size);
		}
	};

	uint32				id;
	ref_ptr<video_parameter_set> vps;

	uint8				max_sub_layers					= 1;
	bool				temporal_id_nesting				= true;
	profile_tier_level	profile_tier_level;
	sub_layer_ordering	ordering;

	CHROMA				chroma_format					= CHROMA_420;
	bool				separate_colour_plane			= false;
	uint32				pic_width_luma					= 0;
	uint32				pic_height_luma					= 0;

	optional<array<ue,4>>	conformance_window;

	uint8				bit_depth_luma					= 8;
	uint8				bit_depth_chroma				= 8;
	uint8				log2_max_pic_order_cnt_lsb		= 8;

	uint8				log2_min_luma_coding_block_size				= 4;	// smallest CB size [3;6]
	uint8				log2_diff_max_min_luma_coding_block_size	= 0;	// largest  CB size
	uint8				log2_min_transform_block_size				= 3;	// smallest TB size [2;5]
	uint8				log2_diff_max_min_transform_block_size		= 1;	// largest  TB size
	uint8				max_transform_hierarchy_depth_inter			= 1;
	uint8				max_transform_hierarchy_depth_intra			= 1;

	optional<ScalingList>	scaling_list;

	bool				amp_enabled						= false;
	bool				sample_adaptive_offset_enabled	= false;
	bool				long_term_refs_enabled			= false;

	optional<pcm>		pcm;
	dynamic_array<ShortTermReferenceSet>				shortterm_sets;
	static_array<POCreference, MAX_NUM_LT_REF_PICS_SPS>	longterm_refs;

	bool				temporal_mvp_enabled			= false;
	bool				strong_intra_smoothing_enabled	= false;

	optional<video_usability_information> vui;

	// --- extensions ---
	uint8				extensions	= 0;
#ifdef HEVC_RANGE
	extension_range::sps		extension_range;
#endif
#ifdef HEVC_ML
	extension_multilayer::sps	extension_multilayer;
#ifdef HEVC_3D
	extension_3d::sps			extension_3d;
#endif
#endif
#ifdef HEVC_SCC
	extension_scc::sps			extension_scc;
#endif

	seq_parameter_set(video_parameter_set *vps) : vps(vps) {}
	bool	read(bitreader &r, int layer_id);

	auto	log2_max_tu()		const { return log2_min_transform_block_size + log2_diff_max_min_transform_block_size; }
	auto	log2_ctu()			const { return log2_min_luma_coding_block_size + log2_diff_max_min_luma_coding_block_size; }
	auto	pic_width_ctu()		const { return shift_right_ceil(pic_width_luma, log2_ctu()); }
	auto	pic_height_ctu()	const { return shift_right_ceil(pic_height_luma,log2_ctu()); }
	auto	total_ctus()		const { return pic_width_ctu() * pic_height_ctu(); }
};

//-----------------------------------------------------------------------------
// PPS
//-----------------------------------------------------------------------------

struct pic_parameter_set : refs<pic_parameter_set> {
	struct Tiling {
		enum {
			MAX_TILE_COLUMNS	= 10,
			MAX_TILE_ROWS		= 10,
		};
		uint32	num_tile_columns			= 1;		// [1;pic_width_ctu]
		uint32	num_tile_rows				= 1;		// [1;pic_height_ctu]
		bool	uniform_spacing				= true;
		bool	loop_filter_across_tiles	= false;
		uint32	colBd[MAX_TILE_COLUMNS + 1];
		uint32	rowBd[MAX_TILE_ROWS + 1];

		auto	rows()					const { return make_range_n(rowBd, num_tile_rows); }
		auto	cols()					const { return make_range_n(colBd, num_tile_columns); }
		auto	total()					const { return num_tile_columns * num_tile_rows; }
		uint32	get_col_width(int i)	const { return colBd[i + 1] - colBd[i]; }
		uint32	get_row_height(int i)	const { return rowBd[i + 1] - rowBd[i]; }
		uint32	get_tile_size(int i)	const { return get_col_width(i % num_tile_columns) * get_row_height(i / num_tile_columns); }
		uint32	get_tile_start(int i)	const { return rowBd[i / num_tile_columns] * colBd[num_tile_columns] + colBd[i % num_tile_columns]; }

		// --- derived values ---
		dynamic_array<uint8>	RStoTile;
		dynamic_array<uint16>	RStoTS;
		dynamic_array<uint16>	TStoRS;

		bool	read(bitreader &r, uint32 pic_width_ctu, uint32 pic_height_ctu);
		auto	next_ctu(uint32 rs) const {
			int		ts = RStoTS[rs] + 1;
			return ts < TStoRS.size() ? TStoRS[ts] : ts;
		}
		bool	is_tile_start(uint32 ctb) const {
			return ctb == get_tile_start(RStoTile[ctb]);
		}
	};

	uint8			id;
	ref_ptr<seq_parameter_set> sps;

	bool			dependent_slice_segments_enabled	= false;
	bool			output_flag_present					= false;
	u<3>			num_extra_slice_header_bits			= 0;
	bool			sign_data_hiding					= false;
	bool			cabac_init_present					= false;
	as_minus<ue,1>	num_ref_idx_l0_default_active		= 1; // [1;16]
	as_minus<ue,1>	num_ref_idx_l1_default_active		= 1; // [1;16]

	// --- QP ---
	as_minus<se, 26>	init_qp							= 27;
	bool			constrained_intra_pred				= false;
	bool			transform_skip_enabled				= false;
	int				cu_qp_delta_depth					= -1;		// [ 0 ; log2_diff_max_min_luma_coding_block_size ]
	int32x2			qp_offset							= zero;
	bool			slice_chroma_qp_offsets_present		= false;

	bool			weighted_pred						= false;
	bool			weighted_bipred						= false;
	bool			transquant_bypass_enabled			= false;
	bool			entropy_coding_sync_enabled			= false;	// for WPP

	optional<Tiling>		tiles;
	optional<Deblocking>	deblocking;
	bool			loop_filter_across_slices			= true;

	ScalingList		scaling_list;

	bool			lists_modification_present			= false;
	uint8			log2_parallel_merge_level			= 2; // [2 ; log2(max CB size)]
	bool			slice_segment_header_extension		= false;

	// --- extensions ---
	uint8			extensions	= 0;
#ifdef HEVC_RANGE
	extension_range::pps		extension_range;
#endif
#ifdef HEVC_ML
	extension_multilayer::pps	extension_multilayer;
#ifdef HEVC_3D
	extension_3d::pps			extension_3d;
#endif
#endif
#ifdef HEVC_SCC
	extension_scc::pps			extension_scc;
#endif
	
	pic_parameter_set(uint8 id, seq_parameter_set *sps) : id(id), sps(sps) {}
	bool	read(bitreader &r);
};

//-----------------------------------------------------------------------------
//	slice
//-----------------------------------------------------------------------------

struct PicReference {
	image	*img			= nullptr;
	bool	longterm		= false;
	int		poc				= 0;
	int		view_idx		= 0;
	PicReference()			{}
	PicReference(image *img, bool longterm = false);
};

struct PicIndex {
	uint8	idx:4, list:1;
};

struct slice_segment_header {
	struct RefList : static_array<uint8, 16> {
		void		read(bitreader &r, int num_bits) {
			for (int i = 0; i < size(); i++)
				(*this)[i] = r.get(num_bits);
		}
		void		init(int n) {
			resize(n);
			for (int i = 0; i < n; i++)
				(*this)[i] = i;
		}
	};


	int			index	= 0; // index through all slices in a picture (internal only)
	ref_ptr<pic_parameter_set> pps;
	NAL::TYPE	unit_type;

	bool		first_slice_segment_in_pic	= false;
//	bool		no_output_of_prior_pics		= false;
	bool		dependent		= false;
	uint32		segment_address				= 0;
	uint8		header_bits					= 0;	//0->discardable, 1->cross_layer_bla

	//inherited if dependent
	SliceType	type						= SLICE_TYPE_B;
	bool		pic_output					= false;
	int			colour_plane_id				= 0;

	//refs: unit_type != NAL::IDR_W_RADL && unit_type != NAL::IDR_N_LP
	int			pic_order_cnt_lsb			= 0;
	bool		temporal_mvp_enabled		= false;
	//refs end

	bool		sao_luma					= false;
	bool		sao_chroma					= false;

	//P & B only
	RefList		ref_list[2];
	int			reduce_merge_candidates		= 0;
	int			NumPocTotalCurr				= 0;
	bool		mvd_l1_zero					= false;
	bool		cabac_init					= false;
	PicIndex	collocated;
	uint8		luma_log2_weight_denom;
	uint8		chroma_log2_weight_denom;
	pred_weight	weights[2][16][3];
	//P & B only end

	int			qp_delta					= 0;
	int32x2		qp_offset					= zero;

	Deblocking	deblocking;
	bool		loop_filter_across_slices	= false;

	//not inherited
	dynamic_array<int>			entry_points;
#ifdef HEVC_RANGE
	extension_range::shdr		extension_range;
#endif
#ifdef HEVC_ML
	extension_multilayer::shdr	extension_multilayer;
#ifdef HEVC_3D
	extension_3d::shdr			extension_3d;
#endif
#endif
#ifdef HEVC_SCC
	extension_scc::shdr			extension_scc;
#endif

	const_memory_block			stream_data;

	// --- derived data ---
	bool			has_future_refs			= false;
	int				slice_address			= 0;	// segment_address of last independent slice
	PicReference	ref_pics[2][MAX_NUM_REF_PICS];	// number of entries: num_ref_idx_l0_active / num_ref_idx_l1_active

	slice_segment_header(pic_parameter_set* pps, NAL::TYPE unit_type, bool first_slice_segment_in_pic) : pps(pps), unit_type(unit_type), first_slice_segment_in_pic(first_slice_segment_in_pic) {}
	~slice_segment_header() {}
	bool	read(bitreader &r, int layer_id, int temporal_id, const slice_segment_header* prev_shdr, ShortTermReferenceSet &shortterm, LongTermReferenceSet &longterm);
	auto&	ref(bool list, int idx)			const	{ return ref_pics[list][idx]; }
	auto&	ref(PicIndex i)					const	{ return ref_pics[i.list][i.idx]; }
	auto	get_ref_pics(bool i)			const	{ return make_range_n(ref_pics[i], ref_list[i].size()); }
	constexpr int	num_ref_lists()			const	{ return h265::num_ref_lists(type); }
	const PicReference*	find_ref(bool list, int poc, int view) const;
	const PicReference*	find_ref_any(int poc, int view) const;
	int		get_qp()						const	{ return pps->init_qp + qp_delta; }

	auto	get_substream_data(int i) const {
		int	start	= i == 0 ? 0 : entry_points[i - 1];
		int end		= i == entry_points.size() ? stream_data.size() : entry_points[i];
		return stream_data.slice(start, end - start);
	}

	friend bool isIRAP(const slice_segment_header *shdr) { return shdr && isIRAP(shdr->unit_type); }

};

//-----------------------------------------------------------------------------
// image
//-----------------------------------------------------------------------------

/*
+--+                +--+--+
|B2|                |B1|B0|
+--+----------------+--+--+
   |                   |
   |                   |
   |                   |
   |                   |
   |        PB         |
   |     (w x h)       |
   |                   |
+--+                   |
|A1|                   |
+--+-------------------+
|A0|
+--+
*/

enum AVAIL {
	AVAIL_A1		= 1 << 0,
	AVAIL_B1		= 1 << 1,
	AVAIL_A0		= 1 << 2,
	AVAIL_B0		= 1 << 3,
	AVAIL_B2		= 1 << 4,

	//3D
	AVAIL_DI		= 1 << 5,
	AVAIL_IV		= 1 << 6,
	AVAIL_IV_SUB	= 1 << 7,
	AVAIL_IV_SHIFT	= 1 << 8,
	AVAIL_T			= 1 << 9,
	AVAIL_VSP		= 1 << 10,
	AVAIL_RESPRED	= 1 << 11,
	AVAIL_RESPRED2	= 1 << 12,
	AVAIL_ILLU		= 1 << 13,

	//SCC
	AVAIL_RESIDUAL_ACT	= 1 << 14,

	AVAIL_CROSS_COMP	= 1 << 15,
	AVAIL_NO_BIPRED		= 1 << 16,

};
constexpr AVAIL	operator*(bool b, AVAIL a)	{ return b ? a : AVAIL(0); }
constexpr AVAIL	operator*(AVAIL a, bool b)	{ return b ? a : AVAIL(0); }
constexpr bool	operator&(AVAIL a, AVAIL b)	{ return (int)a & (int)b; }
constexpr AVAIL	operator|(AVAIL a, AVAIL b)	{ return AVAIL((int)a | (int)b); }
constexpr AVAIL	operator-(AVAIL a, AVAIL b)	{ return AVAIL((int)a & ~(int)b); }

inline AVAIL& operator|=(AVAIL& a, AVAIL b)	{ return a = a | b; }
inline AVAIL& operator-=(AVAIL& a, AVAIL b)	{ return a = a - b; }

constexpr AVAIL	avail_quad0(AVAIL avail, bool right = true, bool below = true)	{ return (avail - AVAIL_B0) | (below && (avail & AVAIL_A1)) * AVAIL_A0 | (right && (avail & AVAIL_B1)) * AVAIL_B0; }
constexpr AVAIL	avail_quad1(AVAIL avail)										{ return (avail | AVAIL_A1 | (avail & AVAIL_B1) * AVAIL_B2) - AVAIL_A0; }
constexpr AVAIL	avail_quad2(AVAIL avail, bool right = true)						{ return  avail | AVAIL_B1 | right * AVAIL_B0 | (avail & AVAIL_A1) * AVAIL_B2; }
constexpr AVAIL	avail_quad3(AVAIL avail)										{ return (avail | AVAIL_A1 | AVAIL_B1 | AVAIL_B2) - (AVAIL_A0 | AVAIL_B0); }

//constexpr AVAIL	avail_quad0(AVAIL avail)	{ return avail | (avail & AVAIL_A1) * AVAIL_A0 | (avail & AVAIL_B1) * AVAIL_B0; }
//constexpr AVAIL	avail_quad1(AVAIL avail)	{ return (avail | AVAIL_A1 | (avail & AVAIL_B1) * AVAIL_B2) - AVAIL_A0; }
//constexpr AVAIL	avail_quad2(AVAIL avail)	{ return avail | AVAIL_B1 | AVAIL_B0 | (avail & AVAIL_A1) * AVAIL_B2; }
//constexpr AVAIL	avail_quad3(AVAIL avail)	{ return (avail | AVAIL_A1 | AVAIL_B1 | AVAIL_B2) - (AVAIL_A0 | AVAIL_B0); }

constexpr AVAIL	avail_top(AVAIL avail)					{ return avail | (avail & AVAIL_A1) * AVAIL_A0; }
constexpr AVAIL	avail_bot(AVAIL avail, bool merge)		{ return ((avail - AVAIL_B0) | AVAIL_B1 | (avail & AVAIL_A1) * AVAIL_B2) - merge * AVAIL_B1; }
constexpr AVAIL	avail_left(AVAIL avail)					{ return (avail - AVAIL_B0) | (avail & AVAIL_B1) * AVAIL_B0; }
constexpr AVAIL	avail_right(AVAIL avail, bool merge)	{ return ((avail - AVAIL_A0) | AVAIL_A1 | (avail & AVAIL_B1) * AVAIL_B2) - merge * AVAIL_A1; }


struct sao_info {
	uint8	type = 0, pos = 0;
	int8	offset[4];
};

struct CTU_info {
	enum PROGRESS : uint8 {
		NONE		= 0,
		PREFILTER	= 2,
		DEBLK_V		= 4,
		DEBLK_H		= 6,
		SAO			= 8,
	};
	enum FLAGS : uint16 {
		BOUNDARY_H			= 1 << 0,
		BOUNDARY_V			= 1 << 1,
		TILE_BOUNDARY_H		= 1 << 2,
		TILE_BOUNDARY_V		= 1 << 3,
		FILTER_BOUNDARY_H	= 1 << 4,
		FILTER_BOUNDARY_V	= 1 << 5,
		HAS_PCM_OR_BYPASS	= 1 << 6,	// pcm or transquant_bypass is used in this CTB
		NO_DEBLOCK			= 1 << 7,
		SAO_LUMA			= 1 << 8,
		SAO_CHROMA			= 1 << 9,
	};
	friend constexpr auto soft_available(FLAGS flags)	{ return AVAIL(~(flags / FILTER_BOUNDARY_H) & 3); }
	friend constexpr auto check_available(FLAGS flags)	{ return AVAIL(~flags & 3); }

	uint16		slice_address		= 0;
	uint16		shdr_index			= 0;	// index into array to slice header for this CTB
//	uint16		CtbAddrInRS			= ~0;	// for debugging
	array<sao_info,3>	sao_info;
	atomic<PROGRESS>	progress	= NONE;
	FLAGS		flags				= (FLAGS)0;
	Event*		event				= nullptr;

	void	set_flags(uint32 _flags) {
		flags	= (FLAGS)(flags | _flags);
	}
	void	init(int slice_rs, int slice_index, uint32 _flags) {
		slice_address	= slice_rs;
		shdr_index		= slice_index;
		set_flags(_flags);
	}

	constexpr bool	has_sao(int c) const {
		return (flags & (c == 0 ? SAO_LUMA : SAO_CHROMA)) && sao_info[c].type;
	}

	void	wait(PROGRESS p, Event *event);
	void	set_progress(PROGRESS p);
};

struct CU_info {
	union {
		struct {
			int8	QP_Y;
			uint8	log2_size			: 3;
			uint8	part_mode			: 3;	// (enum PartMode)  [0:7] set only in top-left of CB
			uint8	pred_mode			: 2;	// (enum PredMode)  [0;2] must be saved for past images
			uint8	pcm					: 1;	// Stored for intra-prediction / SAO
			uint8	transquant_bypass	: 1;	// Stored for SAO
		};
		struct { uint32 u; };
	};
	CU_info() : u(0) {}

	bool	allow_filter(bool allow_pcm)	const	{ return !transquant_bypass && (!pcm || allow_pcm); }
};

class image : public image_base {
public:
	enum {
		TU_NONZERO_COEFF			= 1 << 5,
		TU_SPLIT_TRANSFORM_MASK		= 0x1F,
	};
	enum INTEGRITY : uint8 {
		CORRECT							= 0,
		UNAVAILABLE_REFERENCE			= 1,
		NOT_DECODED						= 2,
		DECODING_ERRORS					= 3,
		DERIVED_FROM_FAULTY_REFERENCE	= 4,
	};

	ref_ptr<pic_parameter_set>		pps;

	scaled_block<CTU_info>			ctu_info;	//sps->log2_ctu
	scaled_block<CU_info>			cu_info;	//sps->log2_min_luma_coding_block_size
	scaled_block<PB_info>			pb_info;	//2
	scaled_block<uint8>				tu_info;	//sps->log2_min_transform_block_size

	dynamic_array<slice_segment_header*> slices;

	bool			allow_filter_pcm= false;
	CHROMA			ChromaArrayType;

	INTEGRITY		integrity		= NOT_DECODED;
	int64			pts				= 0;
	void*			user_data		= nullptr;

	void		release();
	void		alloc(const pic_parameter_set *pps, int64 pts, void* user_data);

	int			add_slice_segment_header(slice_segment_header* shdr) {
		slices.push_back(shdr);
		return slices.size() - 1;
	}

	// --- CU metadata access ---
	void		clear_metadata();
	void		set_pred_mode(int x, int y, int log2_size, PredMode mode)	{
		over(cu_info.sub_block(x, y, log2_size), [=](CU_info& i) { i.log2_size = log2_size; i.pred_mode = mode; });
	}
	PredMode	get_pred_mode(int x, int y)								const	{ return (PredMode)cu_info.get(x,y).pred_mode; }
	PredMode	get_pred_mode(int16x2 v)								const	{ return get_pred_mode(v.x, v.y); }

	void		set_pcm(int x, int y, int log2BlkWidth) {
		over(cu_info.sub_block(x, y, log2BlkWidth), [](CU_info& i) { i.pcm = true; });
		ctu_info.get(x,y).set_flags(CTU_info::HAS_PCM_OR_BYPASS);
	}
	void		set_transquant_bypass(int x, int y, int log2BlkWidth) {
		over(cu_info.sub_block(x, y, log2BlkWidth), [](CU_info& i) { i.transquant_bypass = true; });
		ctu_info.get(x,y).set_flags(CTU_info::HAS_PCM_OR_BYPASS);
	}
	bool		allow_filter(int x, int y, bool allow_pcm)				const	{ return cu_info.get(x,y).allow_filter(allow_pcm); }

	int			get_ct_depth(int x, int y)								const	{ return ctu_info.log2unitSize - cu_info.get(x, y).log2_size; }
	void		set_QPY(int x, int y, int log2BlkWidth, int QP_Y)				{ over(cu_info.sub_block(x, y, log2BlkWidth), [QP_Y](CU_info& i) { i.QP_Y = QP_Y; }); }
	int			get_QPY(int x, int y)									const	{ return cu_info.get(x, y).QP_Y; }

	// --- TU metadata access ---
	void		set_split_transform(int x, int y, int trans_depth)				{ tu_info.get(x, y) |= 1 << trans_depth; }
	int			get_split_transform(int x, int y, int trans_depth)		const	{ return (tu_info.get(x, y) & (1 << trans_depth)); }
	void		set_nonzero_coefficient(int x, int y, int log2TrSize)			{ over(tu_info.sub_block(x, y, log2TrSize), [](uint8 &i) { i |= TU_NONZERO_COEFF; }); }
	bool		has_nonzero_coefficient(int x, int y)					const	{ return tu_info.get(x, y) & TU_NONZERO_COEFF; }

	// --- CTU metadata access ---
	uint32		get_CTB_flags0(int x, int y)							const	{ return ctu_info.get0(x, y).flags; }
	uint32		get_CTB_flags(int x, int y)								const	{ return ctu_info.get(x, y).flags; }
	auto		_get_SliceHeader(int idx)								const	{ return idx < slices.size() ? slices[idx] : nullptr; }
	auto		get_SliceHeader(int x, int y)							const	{ return _get_SliceHeader(ctu_info.data ? ctu_info.get(x, y).shdr_index : 0); }
	auto		get_SliceHeader(int16x2 v)								const	{ return get_SliceHeader(v.x, v.y); }
	uint32		check_CTB_available(int x, int y)						const;
	uint32		available_pred_flags(int x, int y, int w, int h)		const;
	uint32		available_pred_flags(int x, int y, int log2CbSize)		const	{ return available_pred_flags(x, y, 1 << log2CbSize, 1 << log2CbSize); }
	bool		filter_top_right0(int x, int y);

	// --- PB metadata access ---
	const auto&	get_mv_info(int x, int y)								const	{ return pb_info.get(x, y); }
	const auto&	get_mv_info(int16x2 v)									const	{ return get_mv_info(v.x, v.y); }
	void		set_mv_info(int x, int y, int w, int h, const PB_info& mv)		{ over(pb_info.sub_block(x, y, w, h), [mv](PB_info &d) { d = mv; }); }

	friend constexpr auto clamp_to(const image *i, int16x2 pos) {
		return clamp(pos, zero, int16x2{i->get_width() - 1, i->get_height() - 1});
	}
	friend bool check_image_pos(const image *img, int16x2 R) {
		return img && img->integrity != image::UNAVAILABLE_REFERENCE && R.x < img->get_width() && R.y < img->get_height();
	}
	friend bool check_image_ref(const image *img, int16x2 R) {
		return check_image_pos(img, R) && img->get_pred_mode(R) != MODE_INTRA;
	}
};

} //namespace h265
#endif //H265_INTERNAL_H
