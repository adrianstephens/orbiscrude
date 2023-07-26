#ifndef H265_H
#define H265_H

#include "nal.h"
#include "stream.h"
#include "base/vector.h"
#include "base/bits.h"
#include "base/block.h"
#include "base/array.h"
#include "base/interval.h"
#include "base/hash.h"
#include "extra/colour.h"

#include "codec/vlc.h"
#include "codec/cabac.h"

#define HEVC_RANGE
#define HEVC_ML
#define HEVC_3D
#define HEVC_SCC

#ifndef HEVC_ML
#undef HEVC_3D
#endif

namespace h265 {
using namespace iso;

enum {
	MAX_VIDEO_PARAMETER_SETS	= 16,	// this is the maximum as defined in the standard
	MAX_SEQ_PARAMETER_SETS		= 16,	// this is the maximum as defined in the standard
	MAX_PIC_PARAMETER_SETS		= 64,	// this is the maximum as defined in the standard
	MAX_NUM_REF_PICS			= 16,	// maximum defined by standard, may be lower for some Levels
	MAX_TEMPORAL_SUBLAYERS		= 8,
};

enum CHROMA {
	CHROMA_MONO		= 0,
	CHROMA_420		= 1,
	CHROMA_422		= 2,
	CHROMA_444		= 3,
};
constexpr int	num_channels(CHROMA c)			{ return c == CHROMA_MONO ? 1 : 3; }
constexpr int	get_shift_W(CHROMA c)			{ return c == CHROMA_420 || c == CHROMA_422; }
constexpr int	get_shift_H(CHROMA c)			{ return c == CHROMA_420; }
constexpr int	get_shift_W(CHROMA c, int i)	{ return i && get_shift_W(c); }
constexpr int	get_shift_H(CHROMA c, int i)	{ return i && get_shift_H(c); }

struct Configuration {
	struct unit {
		packed<uint16be>	size;
		uint8				data[];
		operator const_memory_block()	const { return { data, size }; }
		const_memory_block	get()		const { return { data, size }; }
		const unit*			next()		const { return (const unit*)(data + size); }
	};
	struct entry {
		union {
			bitfield<uint8, 0, 1>	array_completeness;
			bitfield<uint8, 2, 6>	NAL_unit_type;
		};
		packed<uint16be>	num;
		unit				u[];

		const entry* next() const {
			const unit	*p = u;
			for (int i = num; i--;)
				p = p->next();
			return (const entry*)p;
		}
		auto		units() const { return make_range_n(make_next_iterator(u), num); }

	};
	uint8		configurationVersion;
	union {
		bitfield<uint8, 0, 2>		general_profile_space;
		bitfield<uint8, 2, 1>		general_tier_flag;
		bitfield<uint8, 3, 5>		general_profile_idc;
	};
	packed<xint32be>				general_profile_compatibility_flags;
	uintn<6, true>					general_constraint_indicator_flags;
	uint8							general_level_idc;
	packed<uint16be>				min_spatial_segmentation_idc;
	bitfield<uint8, 0, 2>			parallelismType;
	bitfield<CHROMA, 0, 2>			chromaFormat;
	bitfield<uint8, 0, 3>			bitDepthLumaMinus8;
	bitfield<uint8, 0, 3>			bitDepthChromaMinus8;
	packed<uint16be>				avgFrameRate;
	union {
		bitfield<uint8, 0, 2>		constantFrameRate;
		bitfield<uint8, 2, 3>		numTemporalLayers;
		bitfield<uint8, 5, 1>		temporalIdNested;
		bitfield<uint8, 6, 2>		lengthSizeMinusOne;
	};
	uint8		numOfArrays;
	entry		e[];

	auto		entries() const { return make_range_n(make_next_iterator(e), numOfArrays); }
};


struct memory_reader_0 : memory_reader {
	using memory_reader::memory_reader;
	int		getc()	{ return eof() ? 0 : *p++; }
};

template<typename T> struct golomb0 {
	T	t;
	golomb0()			{}
	golomb0(T t) : t(t)	{}
	operator T() const { return t;}
};

template<int N> using u = compact<uint32, N>;
template<int N> using s = compact<int32, N>;
using ue = golomb0<uint32>;
using se = golomb0<int32>;

template<typename OP, typename T, typename T2, T2 B, typename R = decltype(OP()(declval<T>(), B))> using read_with_op = _read_as<with_op<OP, T, T2, B, R>, R>;
template<typename T, int B> using as_minus	= read_with_op<op_add, T, int, B>;
template<typename T, int B> using as_plus	= read_with_op<op_sub, T, int, B>;

struct bitreader : vlc_in<uint32, true, memory_reader_0> {
	typedef vlc_in<uint32, true, memory_reader_0>	B;

	enum {MAX_LEADING_ZEROS = 20};

	bitreader(const_memory_block b) : B(b) {}
	bitreader(memory_reader& r)		: B(r.get_block(maximum)) {}

	auto get_reverse(int n) {
		return reverse_bits(get(n), n);
	}

	uint32 getu() {
		int nz = 0;
		while (!get_bit()) {
			++nz;
			ISO_ASSERT(nz <= MAX_LEADING_ZEROS);
		}
		return nz ? get(nz) + (1 << nz) - 1 : 0;
	}
	int gets() {
		int v = getu();
		return (v & 1) ? (v + 1) / 2 : -v / 2;
	}

	bool	custom_read(bool& t)	{ t = get_bit(); return true; }

	template<typename T, int N> bool							custom_read(compact<T,N> &t)	{ t = (T)get<N>(); return true; }
	template<typename T> enable_if_t<is_builtin_int<T>, bool>	custom_read(T& t)				{ t = (T)get<BIT_COUNT<T>>(); return true; }
	template<typename T> bool									custom_read(optional<T> &t)		{ return !get_bit() || iso::read(*this, put(t)); }
	template<typename T> enable_if_t< is_signed<T>, bool>		custom_read(golomb0<T>& t)		{ t.t = gets(); return true; }
	template<typename T> enable_if_t<!is_signed<T>, bool>		custom_read(golomb0<T>& t)		{ t.t = getu(); return true; }

	template<typename...T> bool read(T&&...t)	{ return iso::read(*this, t...); }
};

struct bitwriter : vlc_out<uint32, true, dynamic_memory_writer> {
	void putu(uint32 value) {
		int nLeadingZeros	= 0;
		int base			= 0;
		int range			= 1;
		while (value >= base + range) {
			base += range;
			range <<= 1;
			nLeadingZeros++;
		}
		put((1 << nLeadingZeros) | (value - base), 2 * nLeadingZeros + 1);
	}
	void puts(int value) {
		if (value == 0)
			put_bit(true);
		else
			putu(value > 0 ? 2 * value - 1 : -2 * value);
	}

	bool	custom_write(bool t)	{ put_bit(t); return true; }

	template<typename T, int N> bool							custom_write(const compact<T,N> &t)		{ put(t, N); return true; }
	template<typename T> enable_if_t<is_builtin_int<T>, bool>	custom_write(const T& t)				{ put(t, BIT_COUNT<T>); return true; }
	template<typename T> bool									custom_write(const optional<T> &t)		{ return put_bit(t.exists()) && (!r.exists() || iso::write(*this, get(t))); }
	template<typename T> enable_if_t< is_signed<T>, bool>		custom_write(const golomb0<T>& t)		{ puts(t.t); return true; }
	template<typename T> enable_if_t<!is_signed<T>, bool>		custom_write(const golomb0<T>& t)		{ putu(t.t); return true; }

	template<typename...T> bool write(const T&...t)	{ return iso::write_early(*this, t...); }
};

//-----------------------------------------------------------------------------
// NAL
//-----------------------------------------------------------------------------

struct NAL : iso::NAL {
	enum TYPE : uint8 {
		TRAIL_N				= 0,	//slice segment of a non-TSA, non-STSA trailing picture
		TRAIL_R				= 1,
		TSA_N				= 2,	//slice segment of a TSA picture		TSA:	temporal sub-layer access
		TSA_R				= 3,
		STSA_N				= 4,	//slice segment of an STSA picture		STSA:	step-wise temporal sub-layer access
		STSA_R				= 5,
		RADL_N				= 6,	//slice segment of a RADL picture		RADL:	random access decodable leading
		RADL_R				= 7,
		RASL_N				= 8,	//slice segment of a RASL picture		RASL:	random access skipped leading
		RASL_R				= 9,
		RESERVED_VCL_N10	= 10,	//reserved non-IRAP SLNR VCL NAL unit types
		RESERVED_VCL_R11	= 11,
		RESERVED_VCL_N12	= 12,
		RESERVED_VCL_R13	= 13,
		RESERVED_VCL_N14	= 14,
		RESERVED_VCL_R15	= 15,
		BLA_W_LP			= 16,	//slice segment of a BLA picture		BLA:	broken link access
		BLA_W_RADL			= 17,
		BLA_N_LP			= 18,
		IDR_W_RADL			= 19,	//slice segment of an IDR picture		IDR:	instantaneous decoding refresh
		IDR_N_LP			= 20,
		CRA_NUT				= 21,	//slice segment of a CRA picture		CRA:	clean random access
		RESERVED_IRAP_VCL22	= 22,	//Reserved IRAP VCL NAL unit types
		RESERVED_IRAP_VCL23	= 23,
		RESERVED_VCL24		= 24,	//Reserved non-IRAP VCL NAL unit types
		RESERVED_VCL25		= 25,
		RESERVED_VCL26		= 26,
		RESERVED_VCL27		= 27,
		RESERVED_VCL28		= 28,
		RESERVED_VCL29		= 29,
		RESERVED_VCL30		= 30,
		RESERVED_VCL31		= 31,
		VPS_NUT				= 32,	// Video parameter set	
		SPS_NUT				= 33,	// Sequence parameter set
		PPS_NUT				= 34,	// Picture parameter set	
		AUD_NUT				= 35,	// Access unit delimiter	
		EOS_NUT				= 36,	// End of sequence		
		EOB_NUT				= 37,	// End of bitstream		
		FD_NUT				= 38,	// Filler data			
		PREFIX_SEI_NUT		= 39,	// Supplemental enhancement information
		SUFFIX_SEI_NUT		= 40,
		RESERVED_NVCL41		= 41,
		RESERVED_NVCL42		= 42,
		RESERVED_NVCL43		= 43,
		RESERVED_NVCL44		= 44,
		RESERVED_NVCL45		= 45,
		RESERVED_NVCL46		= 46,
		RESERVED_NVCL47		= 47,
		UNDEFINED			= 255,
	};

	friend bool isIDR(TYPE unit_type)	{ return unit_type == IDR_W_RADL || unit_type == IDR_N_LP; }
	friend bool isBLA(TYPE unit_type)	{ return unit_type == BLA_W_LP || unit_type == BLA_W_RADL || unit_type == BLA_N_LP; }
	friend bool isCRA(TYPE unit_type)	{ return unit_type == CRA_NUT; }
	friend bool isRAP(TYPE unit_type)	{ return isIDR(unit_type) || isBLA(unit_type) || isCRA(unit_type); }
	friend bool isRASL(TYPE unit_type)	{ return unit_type == RASL_N || unit_type == RASL_R; }
	friend bool isIRAP(TYPE unit_type)	{ return between(unit_type, BLA_W_LP, RESERVED_IRAP_VCL23); }
	friend bool isRADL(TYPE unit_type)	{ return unit_type == RADL_N || unit_type == RADL_R; }
	friend bool isReference(TYPE unit_type) {
		return (unit_type <= RESERVED_VCL_R15 && (unit_type & 1)) || isIRAP(unit_type);
	}
	friend bool isSublayerNonReference(TYPE unit_type) {
		return unit_type <= RESERVED_VCL_R15 && (unit_type & 1) == 0;
	}
};

//-----------------------------------------------------------------------------
// image_base
//-----------------------------------------------------------------------------

class image_base {//}; : public refs<image_base> {
	struct plane {
		malloc_block	pixels;
		uint32			width = 0, height = 0, stride = 0;
		uint8			bit_depth = 0, channel = 0;
		void			create(uint32 _width, uint32 _height, uint8 _bit_depth);
		template<typename T> auto get_block()		const { return make_strided_block<T>(pixels, width, stride, height); }
		template<typename T> auto get_blockptr()	const { return make_blockptr<T>(pixels, stride); }
	};
public:
	void*		free_ptr			= nullptr;
	int			refs				= 0;
	bool		is_output			= false;
	int			picture_order_cnt	= -1;
	uint8		layer_id			= 0;
	uint8		temporal_id			= 0;
	uint8		view_idx			= 0;
	NAL::TYPE	nal_unit			= NAL::UNDEFINED;
	CHROMA		chroma_format		= CHROMA_MONO;
	array<uint32,4>	offsets			= 0;
	plane		planes[3];

	void		addref()	{ ++refs; }
	//void		release()	{ --refs; }

	int			get_width (int c = 0)		const { return planes[c].width;  }
	int			get_height(int c = 0)		const { return planes[c].height; }
	int			get_image_stride(int c)		const { return planes[c].stride; }
	CHROMA		get_chroma_format()			const { return chroma_format; }
	int			get_bit_depth(int c)		const { return planes[c].bit_depth; }
	bool		high_bit_depth(int c)		const { return get_bit_depth(c) > 8; }

	template<typename T> auto get_plane_ptr(int c)		const { return planes[c].get_blockptr<T>(); }
	template<typename T> auto get_plane_block(int c)	const { return planes[c].get_block<T>(); }
	template<typename T> auto get_plane_block(int c, int x, int y, int w, int h) const {
		return get_plane_block<T>(c).template sub<1>(x, w).template sub<2>(y, h);
	}
	template<typename T> auto get_plane_block(int c, int16x2 p, int w, int h) const {
		return get_plane_block<T>(c, p.x, p.y, w, h);
	}

	bool	compatible(const image_base* ref) const {
		return ref->get_width(0) == get_width(0)
			&& ref->get_height(0) == get_height()
			&& get_chroma_format() == ref->get_chroma_format()
			&& get_bit_depth(0) == ref->get_bit_depth(0)
			&& get_bit_depth(1) == ref->get_bit_depth(1);
	}

	void	create(uint32 width, uint32 height, CHROMA _chroma_format, uint8 bit_depth_luma, uint8 bit_depth_chroma);
};

class image;

//-----------------------------------------------------------------------------
// SEI
//-----------------------------------------------------------------------------

struct SEI {
	enum TYPE {
		buffering_period							= 0,
		pic_timing									= 1,
		pan_scan_rect								= 2,
		filler_payload								= 3,
		user_data_registered_itu_t_t35				= 4,
		user_data_unregistered						= 5,
		recovery_point								= 6,
		scene_info									= 9,
		picture_snapshot							= 15,
		progressive_refinement_segment_start		= 16,
		progressive_refinement_segment_end			= 17,
		film_grain_characteristics					= 19,
		post_filter_hint							= 22,
		tone_mapping_info							= 23,
		frame_packing_arrangement					= 45,
		display_orientation							= 47,
		green_metadata								= 56,
		structure_of_pictures_info					= 128,
		active_parameter_sets						= 129,
		decoding_unit_info							= 130,
		temporal_sub_layer_zero_idx					= 131,
		decoded_picture_hash						= 132,
		scalable_nesting							= 133,
		region_refresh_info							= 134,
		no_display									= 135,
		time_code									= 136,
		mastering_display_colour_volume				= 137,
		segmented_rect_frame_packing_arrangement	= 138,
		temporal_motion_constrained_tile_sets		= 139,
		chroma_resampling_filter_hint				= 140,
		knee_function_info							= 141,
		colour_remapping_info						= 142,
		deinterlaced_field_identification			= 143,
		content_light_level_info					= 144,
		dependent_rap_indication					= 145,
		coded_region_completion						= 146,
		alternative_transfer_characteristics		= 147,
		ambient_viewing_environment					= 148,
		content_colour_volume						= 149,
		equirectangular_projection					= 150,
		cubemap_projection							= 151,
		fisheye_video_info							= 152,
		sphere_rotation								= 154,
		regionwise_packing							= 155,
		omni_viewport								= 156,
		regional_nesting							= 157,
		mcts_extraction_info_sets					= 158,
		mcts_extraction_info_nesting				= 159,
		layers_not_present							= 160,
		inter_layer_constrained_tile_sets			= 161,
		bsp_nesting									= 162,
		bsp_initial_arrival_time					= 163,
		sub_bitstream_property						= 164,
		alpha_channel_info							= 165,
		overlay_info								= 166,
		temporal_mv_prediction_constraints			= 167,
		frame_field_info							= 168,
		three_dimensional_reference_displays_info	= 176,
		depth_representation_info					= 177,
		multiview_scene_info						= 178,
		multiview_acquisition_info					= 179,
		multiview_view_position						= 180,
		alternative_depth_info						= 181,
		sei_manifest								= 200,
		sei_prefix_indication						= 201,
		annotated_regions							= 202,
		shutter_interval_info						= 205,
	};

	template<TYPE t> struct T : malloc_block {
		bool	read(memory_reader& r) { return malloc_block::read(r, r.remaining()); }
		bool	process(image* img) const { return false; }
	};
	struct functions : static_hash<functions, TYPE> {
		SEI*	(*load)(memory_reader&);
		void	(*del)(SEI*);
		bool	(*process)(SEI*, image*);
		functions(TYPE t,  SEI* (*load)(memory_reader&), void (*del)(SEI*), bool (*process)(SEI*, image*)) : base(t), load(load), del(del), process(process) {}
	};
	
	template<TYPE t> struct T1;

	TYPE			type;
	functions		*funcs;

	SEI(TYPE type, functions *funcs) : type(type), funcs(funcs) {}
	~SEI()									{ funcs->del(this); }
	bool		process0(image* img)		{ return funcs->process(this, img); }
	template<TYPE t> auto	as()	const	{ ISO_ASSERT(type == t); return static_cast<const T1<t>*>(this); }

	static SEI*	read0(const_memory_block data);
};

template<SEI::TYPE t> struct SEI::T1 : SEI, T<t> {
	static functions	funcs;
	T1() : SEI(t, &funcs) {}
};

template<SEI::TYPE t> SEI::functions SEI::T1<t>::funcs(t, 
	[](memory_reader &r)->SEI* {
		auto	p = new T1<t>;
		if (r.read(*static_cast<T<t>*>(p)))
			return p;
		delete p;
		return nullptr;
	},
	[](SEI* sei) {
		static_cast<T<t>*>(static_cast<T1<t>*>(sei))->~T<t>();
	},
	[](SEI *sei, image *img) {
		return static_cast<T1<t>*>(sei)->process(img);
	}
);

//-----------------------------------------------------------------------------
// decoder
//-----------------------------------------------------------------------------

struct slice_segment_header;
struct video_parameter_set;
struct seq_parameter_set;
struct pic_parameter_set;
class image_unit;

enum OPTIONS {
	OPT_none					= 0,
	OPT_sei_check_hash			= 1 << 0,
	OPT_show_stream_errors		= 1 << 1,
	OPT_suppress_faulty_pictures= 1 << 2,
	OPT_disable_deblocking		= 1 << 3,
	OPT_disable_sao				= 1 << 4,
	OPT_force_sequential		= 1 << 5,
	OPT_disable_SHVC			= 1 << 6,
	OPT_output_immediately		= 1 << 7,

	OPT_default					= OPT_none,
};

constexpr OPTIONS operator|(OPTIONS a, OPTIONS b) { return OPTIONS((int)a | (int(b))); }

struct POCreference {
	int		poc				= 0;	//relative for st
	bool	used			= false;
	bool	has_delta_msb	= false;

	void	read(bitreader& r, int num_bits) {
		poc		= r.get(num_bits);
		used	= r.get_bit();
	}
};

struct ShortTermReferenceSet {
	static_array<POCreference, MAX_NUM_REF_PICS>	neg; // sorted in decreasing order (e.g. -1, -2, -4, -7, ...)
	static_array<POCreference, MAX_NUM_REF_PICS>	pos; // sorted in ascending order (e.g. 1, 2, 4, 7)
	uint8	size()			const { return neg.size() + pos.size(); }
	bool	read(bitreader &r, range<const ShortTermReferenceSet*> sets, bool slice);
};

struct LongTermReferenceSet : static_array<POCreference, MAX_NUM_REF_PICS>	{
	bool	read(bitreader &r, const POCreference *refs, int nrefs, int lsb_bits);
};

struct tables_reference {
	tables_reference();
	~tables_reference();
};

struct Layer {
	image*							free_images	= nullptr;
	dynamic_array<ref_ptr<image>>	short_term_images;
	dynamic_array<ref_ptr<image>>	long_term_images;

	dynamic_array<ref_ptr<image>>	reorder_output_queue;
	dynamic_array<ref_ptr<image>>	image_output_queue;

	dynamic_array<image_unit*>		image_units;
	dynamic_array<SEI*>				held_seis;

	bool	first_decoded_picture	= true;
	bool	first_after_EOS			= false;
	bool	NoRaslOutput			= false;
	int		prev_POC				= 0;
	
	const slice_segment_header* prev_shdr = nullptr; // Remember the last slice for a successive dependent slice

	image*		new_image(const pic_parameter_set *pps, int64 pts, void* user_data);
	image*		new_dummy(const pic_parameter_set* pps, int poc);
	image*		image_for_POC(int poc, uint32 poc_mask = ~0, bool preferLongTerm = false) const;

	void		start_pic(image *img, slice_segment_header* shdr, uint8 temporal_id, const ShortTermReferenceSet &shortterm, const LongTermReferenceSet &longterm);
	bool		decode(bool parallel);
	image*		finish_pic(OPTIONS options);
	void		flush_reorder_buffer();

public:
	~Layer();
	void		reset();

	uint64		ref_avail(int poc)					const;
	image_unit*	get_unit(int poc)					const;
	image*		current()							const;
	int			num_pictures_in_output_queue()		const	{ return image_output_queue.size(); }
	image_base*	get_next_picture_in_output_queue()	const;
	void		pop_next_picture_in_output_queue(bool hold = false);
	void		output_next_picture_in_reorder_buffer();
};


class decoder_context : tables_reference
#ifndef HEVC_ML
	, public Layer
#endif
{
	ref_ptr<video_parameter_set>	vps[MAX_VIDEO_PARAMETER_SETS];
	ref_ptr<seq_parameter_set>		sps[MAX_SEQ_PARAMETER_SETS];
	ref_ptr<pic_parameter_set>		pps[MAX_PIC_PARAMETER_SETS];

	// input parameters
	uint8	framerate_ratio			= 100;
	uint8	limit_HighestTid		= 6;	// never switch to a layer above this one

	// current control parameters
	uint8	layer_framerate_ratio	= 100;	// ratio of frames to keep in the current layer
	uint8	goal_HighestTid			= 6;	// this is the layer we want to decode at
	uint8	current_HighestTid		= 6;	// the layer which we are currently decoding at
	uint8	highestTid				= 6;
	uint8	frame_accum				= 0;

	struct { int8 tid, ratio; } framedrop_tab[100+1];
	//int		framedrop_tid_index[6+1];

#ifdef HEVC_ML
	dynamic_array<Layer>			layers;
#endif

	image*	img						= nullptr;

	void		compute_framedrop_table();
	void		calc_tid_and_framerate_ratio();
	int			decode_NAL(NAL::unit* nal);
#ifdef HEVC_ML
	void		finish_pic(Layer &layer);
#endif

public:

	enum DecodeResult {
		RES_error		= 0,
		RES_ok			= 1,
		RES_stall_out	= 2,	//output stalled (no free images)
		RES_stall_in	= 3,	//input stalled (need more data)
		RES_abort		= 4,	//stop now
	};

	OPTIONS		options;
	uint32x2	size	= zero;

	static uint32x2 get_image_size(NAL::Parser &nal_parser);

	decoder_context(OPTIONS options = OPT_default);
	~decoder_context();
	DecodeResult	decode(NAL::Parser &nal_parser);

	seq_parameter_set* get_sps(uint32 id)			const	{ return id < MAX_SEQ_PARAMETER_SETS ? get(sps[id]) : nullptr; }
	pic_parameter_set* get_pps(uint32 id)			const	{ return id < MAX_PIC_PARAMETER_SETS ? get(pps[id]) : nullptr; }

	// frame dropping
	int			get_current_TID()					const	{ return current_HighestTid; }
	void		set_limit_TID(int tid);
	void		set_framerate_ratio(int percent);
	void		change_framerate(int more_vs_less); // 1: more, -1: less

#ifdef HEVC_ML
	image_base* get_next_picture_in_output_queue(uint64 layer_mask = ~0ull)	const;
	void		clear_output_queues(uint64 layer_mask);
	void		pop_next_picture_in_output_queue(int layer = 0, bool hold = false);
#endif
};

} // namespace h265

#endif //H265_H
