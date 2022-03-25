#include "movie.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

using namespace iso;

//obsolete:clip, crgn, matt, kmat, pnot, ctab, load, imap; 
//these track reference types (as found in the reference_type of a Track Reference Box): tmcd, chap, sync, scpt, ssrc.
/*
	Table 1 — Box types, structure, and cross-reference 

	ftyp *	4.3 file type and compatibility
	pdin	8.43 progressive download information 
	moov *	8.1 container for all the metadata
		mvhd *	8.3 movie header, overall declarations
			trak *	8.4 container for an individual track or stream
				tkhd *	8.5 track header, overall information about the track
				tref	8.6 track reference container
				edts	8.25 edit list container
					elst	8.26 an edit list
				mdia *	8.7 container for the media information in a track
					mdhd *	8.8 media header, overall information about the media
					hdlr *	8.9 handler, declares the media (handler) type
					minf *	8.10 media information container
						vmhd	8.11.2 video media header, overall information (video track only)
						smhd	8.11.3 sound media header, overall information (sound track only)
						hmhd	8.11.4 hint media header, overall information (hint track only)
						nmhd	8.11.5 Null media header, overall information (some tracks only) 
						dinf *	8.12 data information box, container
							dref * 8.13 data reference box, declares source(s) of media data in track
						stbl * 8.14 sample table box, container for the time/space map
							stsd *	8.16 sample descriptions (codec types, initialization etc.)
							stts *	8.15.2 (decoding) time-to-sample 
							ctts	8.15.3 (composition) time to sample 
							stsc *	8.18 sample-to-chunk, partial data-offset information
							stsz	8.17.2 sample sizes (framing)
							stz2	8.17.3 compact sample sizes (framing) 
							stco *	8.19 chunk offset, partial data-offset information
							co64	8.19 64-bit chunk offset 
							stss	8.20 sync sample table (random access points) 
							stsh	8.21 shadow sync sample table 
							padb	8.23 sample padding bits 
							stdp	8.22 sample degradation priority 
							sdtp	8.40.2 independent and disposable samples 
							sbgp	8.40.3.2 sample-to-group 
							sgpd	8.40.3.3 sample group description 
							subs	8.42 sub-sample information 
		mvex	8.29 movie extends box
			mehd	8.30 movie extends header box 
			trex *	8.31 track extends defaults
		ipmc	8.45.4 IPMP Control Box 
	moof	8.32 movie fragment
		mfhd *	8.33 movie fragment header
		traf	8.34 track fragment
			tfhd *	8.35 track fragment header
			trun	8.36 track fragment run
			sdtp	8.40.2 independent and disposable samples 
			sbgp	8.40.3.2 sample-to-group 
			subs	8.42 sub-sample information 
	mfra	8.37 movie fragment random access 
		tfra	8.38 track fragment random access 
		mfro *	8.39 movie fragment random access offset 
	mdat	8.2 media data container
	free	8.24 free space
	skip	8.24 free space
		udta	8.27 user-data
			cprt	8.28 copyright etc. 
	meta	8.44.1 metadata 
		hdlr *	8.9 handler, declares the metadata (handler) type
		dinf	8.12 data information box, container
			dref	8.13 data reference box, declares source(s) of metadata items
		ipmc	8.45.4 IPMP Control Box 
		iloc	8.44.3 item location 
		ipro	8.44.5 item protection 
			sinf	8.45.1 protection scheme information box 
				frma	8.45.2 original format box 
				imif	8.45.3 IPMP Information box 
				schm	8.45.5 scheme type box 
				schi	8.45.6 scheme information box 
		iinf	8.44.6 item information 
		xml		8.44.2 XML container 
		bxml	8.44.2 binary XML container 
		pitm	8.44.4 primary item reference
*/

struct id4 {
	uint32be	id;

	id4(uint32 u = 0) : id(u) {}

	operator fixed_string<9>() const {
		fixed_string<9> t;
		if (string_check((const char*)&id, 4, char_set::alphanum + ' ')) {
			(uint32be&)t = id;
			t[4] = 0;
		} else {
			t[to_string(t.begin(), hex(id))] = 0;
		}
		return t;
	}
	operator tag() const {
		return operator fixed_string<9>();
	}
	operator uint32() const { return id; }
};

struct atom {
	enum {
		SIZE_TOEND	= 0,
		SIZE_64BIT	= 1,
	};
	uint64		size;
	id4			id;

	bool	read(istream_ref file) {
		uint32	size0 = file.get<uint32be>();
		file.read(id);
		size	= size0 == SIZE_64BIT	? file.get<uint64be>() - 16
				: size0 > 8				? size0 - 8
				: 0;
		return true;
	}
};
//not sure:
typedef void	ipmc, ipro, qtsinf, frma, imif, schm, schi, ipco, ipma, auxl, cdsc, dimg, thmb;

typedef void pdin, moov, moof, mfra, mdat, qtfree, qtskip, trak, edts, mdia, minf, dinf, mvex, udta, stbl, traf, iprp;


struct version_flags {
	uint32		version:8, flags:24;//1-byte version of this movie header atom, + 3 bytes for future movie header flags.
};

typedef version_flags qtmeta, nmhd, iref;

template<int VER> struct version_flags_time;

template<> struct version_flags_time<0> : version_flags {
	uint32be	creation_time;		//the calendar date and time (in seconds since midnight, January 1, 1904) when the movie atom was created
	uint32be	modification_time;
};

template<> struct version_flags_time<1> : version_flags {
	uint64be	creation_time;		//the calendar date and time (in seconds since midnight, January 1, 1904) when the movie atom was created
	uint64be	modification_time;
};

template<typename T> struct qt_table : version_flags, trailing_array2<qt_table<T>, T> {
	uint32be	count;			//count of data references that follow.
	constexpr uint32	size()		const	{ return count; }
	range<const T*>		entries()	const { return range<const T*>(this->begin(), this->end()); }
};

template<> struct qt_table<void> : version_flags {
	uint32be	count;			//count of data references that follow.
};

struct ftyp {
	id4			major_brand;
	uint32be	minor_version;
	id4			compatible_brands[];
};

struct mvhd : version_flags_time<0> {
	uint32be	time_scale;			//time scale for this movie—that is, the number of time units that pass per second in its time coordinate system. A time coordinate system that measures time in sixtieths of a second, for example, has a time scale of 60.
	uint32be	duration;			//duration of the movie in time scale units
	uint32be	preferred_rate;		//fixed-point number that specifies the rate at which to play this movie. A value of 1.0 indicates normal rate.
	uint16be	preferred_volume;	//fixed-point number that specifies how loud to play this movie’s sound. A value of 1.0 indicates full volume.
	uint8		reserved[10];		//set to 0.
	uint32be	matrix[9];			//how to map points from one coordinate space into another
	uint32be	preview_time;		//time value in the movie at which the preview begins.
	uint32be	preview_duration;	//duration of the movie preview in movie time scale units.
	uint32be	poster_time;		//time value of the time of the movie poster.
	uint32be	selection_time;		//time value for the start time of the current selection.
	uint32be	selection_duration;	//duration of the current selection in movie time scale units.
	uint32be	current_time;		//time value for current time position within the movie.
	uint32be	next_track;			//value to use for the track ID number of the next track added to this movie. Note that 0 is not a valid track ID value.
};

struct tkhd : version_flags_time<0> {
	enum {
		enabled		= 1,	//track is enabled
		in_movie	= 2,	//track is used in the movie
		in_preview	= 4,	//track is used in the movie’s preview
		in_poster	= 8,	//track is used in the movie’s poster
	};
	uint32be	id;					//uniquely identifies the track. The value 0 cannot be used.
	uint32be	reserved;			//Set this field to 0.
	uint32be	duration;			//duration of the this track in movies time scale units
	uint64be	reserved2;			//Set this field to 0.
	uint16be	layer;				//indicates this track’s spatial priority in its movie
	uint16be	alternate_group;	//identifies a collection of movie tracks that contain alternate data for one another (0 indicates that the track is not in an alternate track group)
	uint16be	volume;				//fixed-point value that indicates how loudly this track’s sound is to be played. A value of 1.0 indicates normal volume.
	uint16be	reserved3;			//A 16-bit integer that is reserved for use by Apple. Set this field to 0.
	uint32be	matrix[9];			//how to map points from one coordinate space into another
	uint32be	width;				//fixed point width of this track in pixels.
	uint32be	height;				//fixed point height of this track in pixels.
};

struct mdhd : version_flags_time<0> {
	uint32be	time_scale;			//time scale for this media—that is, the number of time units that pass per second in its time coordinate system. A time coordinate system that measures time in sixtieths of a second, for example, has a time scale of 60.
	uint32be	duration;			//duration of the this track in time scale units
	uint16be	language;			//language code for this media. See “Language Code Values” for valid language codes. Also see “Extended Language Tag Atom” for the preferred code to use here if an extended language tag is also included in the media atom.
	uint16be	quality;			//media’s playback quality—that is, its suitability for playback in a given environment.
};

struct vmhd : version_flags {
	enum {
		no_lean_ahead	= 1,		//compatibility flag to distinguish between movies created with QuickTime 1.0 and newer movies
	};
	uint16be	graphics_mode;		//transfer mode - which Boolean operation QuickDraw should perform when drawing or transferring an image
	uint16be	op_color[3];		//red, green, and blue colors for the transfer mode operation
};

struct smhd : version_flags {
	uint16be	balance;
	uint16be	reserved;
};

struct hmhd : version_flags {
	uint16be	maxPDUsize; 
	uint16be	avgPDUsize; 
	uint32be	maxbitrate; 
	uint32be	avgbitrate; 
	uint32be	reserved;
};

struct hdlr : version_flags {	// component
	id4			type;			//identifies the type of the handler ('mhlr' for media handlers or 'dhlr' for data handlers)
	id4			subtype;		//identifies the type of the media handler or data handler
	id4			manufacturer;	//Reserved. Set to 0.
	uint32be	flags2;			//Reserved. Set to 0.
	uint32be	flags2_mask;	//Reserved. Set to 0.
	embedded_string	name;		//A (counted) string that specifies the name of the component
};

struct data_entry : version_flags {
	enum {
		self_reference = 1,	//media’s data is in the same file as the movie atom
	};
};

struct url : data_entry {
	embedded_string	location;
};

struct urn : data_entry {
	embedded_string	name;
	after<embedded_string, decltype(name)>	location;
};

typedef qt_table<void> dref;
typedef qt_table<id4> tref;

//sample descriptor
struct sample_desc {
	uint32be	size;					//number of bytes in the sample description.
	id4			format;					//usually either the compression format or the media type.
	uint8		reserved[6];			//set to 0
	uint16be	data_ref_index;			//index of the data reference to use to retrieve data associated with samples that use this sample description
};
typedef qt_table<void> stsd;

struct video_sample_desc : sample_desc {
	uint16be	version;				//version number of the compressed data. This is set to 0, unless a compressor has changed its data format.
	uint16be	revision_level;			//set to 0.
	id4			vendor;					//developer of the compressor that generated the compressed data. Often this field contains 'appl' to indicate Apple Computer, Inc.
	uint32be	temporal_quality;		//0 to 1023 indicating the degree of temporal compression.
	uint32be	spatial_quality;		//0 to 1024 indicating the degree of spatial compression.
	uint16be	width;					//width of the source image in pixels.
	uint16be	height;					//height of the source image in pixels.
	uint32be	horizontal_resolution;	//fixed-point number containing the horizontal resolution of the image in pixels per inch.
	uint32be	verical_resolution;		//fixed-point number containing the vertical resolution of the image in pixels per inch.
	uint32be	data_size;				//set to 0.
	uint16be	frame_count;			//how many frames of compressed data are stored in each sample. Usually set to 1.
	char		compressor_name[32];	//A 32-byte Pascal string containing the name of the compressor that created the image, such as "jpeg".
	uint16be	depth;					//pixel depth of the compressed image. Values of 1, 2, 4, 8 ,16, 24, and 32 indicate the depth of color images. The value 32 should be used only if the image contains an alpha channel. Values of 34, 36, and 40 indicate 2-, 4-, and 8-bit grayscale, respectively, for grayscale images.
	uint16be	colortable_id;			//which color table to use. If this field is set to –1, the default color table should be used for the specified depth. For all depths below 16 bits per pixel, this indicates a standard Macintosh color table for the specified depth. Depths of 16, 24, and 32 have no color table.
};

struct audo_sample_desc : sample_desc {
	uint32be	reserved[2];
	uint16be	channelcount;	// = 2;
	uint16be	samplesize;		// = 16;
	uint16be	pre_defined;	// = 0;
	uint16be	reserved2;		// = 0;
	uint32be	samplerate;		// = { timescale of media } << 16;
};

struct time_to_sample {
	uint32be	sample_count;			//the number of consecutive samples that have the same duration.
	uint32be	sample_delta;			//duration of each sample.
};
typedef qt_table<time_to_sample>	stts;
typedef qt_table<uint32be>			stss;	//sync sample
typedef qt_table<uint16be>			stdp;

struct shadow_sync_sample {
	uint32be	shadowed_sample_number;
	uint32be	sync_sample_number;
};
typedef qt_table<shadow_sync_sample>	stsh;

struct composition_offset {
	uint32be	sample_count;			//the number of consecutive samples with the calculated composition offset in the field
	uint32be	sample_offset;			//the value of the calculated compositionOffset
};
typedef qt_table<composition_offset> ctts;

//sample to chunk
struct sample_to_chunk {
	uint32be	first_chunk;			//first chunk number using this table entry.
	uint32be	samples_per_chunk;		//number of samples in each chunk.
	uint32be	sample_desc_id;			//identification number associated with the sample description for the sample
};
typedef qt_table<sample_to_chunk>	stsc;

//padding bits
struct padb : version_flags {
	uint32be	count;
	range<bitfield_pointer<uint8, uint8, 4>>	entries() const { return make_range_n(bitfield_pointer<uint8, uint8, 4>(this + 1), count); }
};


//sample size
struct stsz : version_flags, trailing_array2<stsz, uint32be> {
	uint32be	sample_size;	//the sample size (0 -> samples have different sizes, stored in the sample size table)
	uint32be	count;
	constexpr uint32		size()	const	{ return count; }
	range<const uint32be*>	entries() const { return range<const uint32be*>(begin(), end()); }
};

struct stz2 : version_flags {
	uint8		reserved[3];
	uint8		field_size;
	uint32be	count;
};

//chunk offset
typedef qt_table<uint32be> stco;
typedef qt_table<uint64be> co64;

template<int VER> struct elst_entry;
template<> struct elst_entry<0> {
	uint32be	segment_duration;
	int32be		media_time;
	int16be		media_rate_int;
	int16be		media_rate_frac;
};
template<> struct elst_entry<1> {
	uint64be	segment_duration;
	int64be		media_time;
	int16be		media_rate_int;
	int16be		media_rate_frac;
};

typedef qt_table<elst_entry<0>> elst;

struct cprt : version_flags {
	uint16be	language;
	embedded_string	notice;
};

template<int VER> struct mehd;
template<> struct mehd<0> : version_flags {
	uint32be	fragment_duration;
};
template<> struct mehd<1> : version_flags {
	uint64be	fragment_duration;
};

struct sample_flags {
	uint16		reserved:6, sample_depends_on:2, sample_is_depended_on:2, sample_has_redundancy:2, sample_padding_value:3, sample_is_difference_sample:1;
	uint16be	sample_degradation_priority;
};

struct trex : version_flags {
	uint32be track_ID; 
	uint32be default_sample_description_index; 
	uint32be default_sample_duration; 
	uint32be default_sample_size; 
	uint32be default_sample_flags;
};

struct mfhd : version_flags {
	uint32be	sequence_number;
};

struct tfhd : version_flags {
	enum {
		basedataoffset			= 0x000001,
		sampledescriptionindex	= 0x000002,
		sampleduration			= 0x000008,
		defaultsamplesize		= 0x000010,
		defaultsampleflags		= 0x000020,
		duration_empty			= 0x010000,
	};
	uint32be	track_ID; 
	// all the following are optional fields 
	uint32be	base_data_offset; 
	uint32be	sample_description_index; 
	uint32be	default_sample_duration; 
	uint32be	default_sample_size; 
	uint32be	default_sample_flags;
};


struct trun : version_flags {
	enum {
		data_offset_present = 0x000001,
		first_sample_flags_present = 0x000004,
		sample_duration_present = 0x000100,
		sample_size_present = 0x000200,
		sample_flags_present = 0x000400,
		sample_composition_time_offsets_present = 0x000800,
	};
	struct entry {
		// all fields in the following array are optional 
		uint32be sample_duration;
		uint32be sample_size;
		uint32be sample_flags;
		uint32be sample_composition_time_offset;
	};
	uint32be	count;
	// the following are optional fields 
	uint32be	data_offset;
	uint32be	first_sample_flags;
	entry		e[];
	range<const entry*>	entries() const { return range<const entry*>(e, e + count); }
};

struct tfra : version_flags {
	uint32be track_ID;
	uint32	reserved : 26, length_size_of_traf_num : 2, length_size_of_trun_num : 2, length_size_of_sample_num : 2;
	uint32be count;
	struct entry {
		uint64be	time;
		uint64be	moof_offset;
		uint32be	traf_number;	//(length_size_of_traf_num + 1) * 8
		uint32be	trun_number;	//(length_size_of_trun_num + 1) * 8
		uint32be	sample_number;	//(length_size_of_sample_num + 1) * 8
	};
	entry		e[];
	range<const entry*>	entries() const { return range<const entry*>(e, e + count); }
};

struct mfro : version_flags {
	uint32be	size;
};

struct sdtp : version_flags {
	//x sample_count
	uint8	reserved:2, sample_depends_on:2, sample_is_depended_on:2, sample_has_redundancy:2;
	uint8	dummy;
};

struct sbgp : version_flags {
	uint32be grouping_type;
	uint32be count;
	struct entry {
		uint32be	sample_count;
		uint32be	group_description_index;
	};
	entry		e[];
	range<const entry*>	entries() const { return range<const entry*>(e, e + count); }
};

struct sgpd : version_flags {
	uint32be grouping_type;
	uint32be count;
};

template<int VER> struct subsample_size_type;
template<> struct subsample_size_type<0> : T_type<uint16be> {};
template<> struct subsample_size_type<1> : T_type<uint32be> {};
template<int VER> using subsample_size_t = typename subsample_size_type<VER>::type;

template<int VER> struct subs_subsample {
	subsample_size_t<VER>	subsample_size;
	uint8		subsample_priority;
	uint8		discardable;
	uint32be	reserved;// = 0;
};
template<int VER> struct subs_entry {
	uint32be	sample_delta;
	uint16be	count;
	subs_subsample<VER>	e[];
	auto	entries() const { return make_range_n(e, count); }
};

template<int VER> struct subs : version_flags {
	uint32be count;
	subs_entry<VER>	e[];
	auto	entries() const { return make_range_n(e, count); }
};


struct xml : version_flags {
	embedded_string	xml;
};

struct bxml : version_flags {
	uint8	data[1];
};

struct pitm : version_flags {
	uint16be	item_id;
};

struct iloc : version_flags {
	uint16		offset_size : 4, length_size : 4, base_offset_size : 4, : 4;
	uint16be	count;
	struct entry {
		uint16be	item_id;
		uint16be	data_reference_index;
		uint32be	base_offset;//base_offset_size * 8
		uint16be	count;
		struct extent {
			uint32be	extent_offset;//offset_size * 8
			uint32be	extent_length;//length_size * 8
		};
		extent	e[];
		auto	extents() const { return make_range_n(e, count); }
	};
	entry	e[];
	auto	entries() const { return make_range_n(e, count); }
};

struct infe : version_flags {
	uint16be	item_id;
	uint16be	item_protection_index;
	embedded_string item_name;
	after<embedded_string, embedded_string> content_type;
	after<embedded_string, decltype(content_type)> content_encoding; //optional 
};
struct iinf : version_flags {
	uint16be	count;
};

//hvcC
struct HEVCDecoderConfigurationRecord {
	struct unit {
		packed<uint16be>	nalUnitLength;
		uint8				nalUnit[];
		const_memory_block	get() const { return { nalUnit, nalUnitLength }; }
		const unit* next() const { return (const unit*)(nalUnit + nalUnitLength); }
	};
	struct entry {
		union {
			bitfield<uint8, 0, 1>	array_completeness;
			bitfield<uint8, 2, 6>	NAL_unit_type;
		};
		packed<uint16be>	numNalus;
		unit				u[];

		const entry* next() const {
			const unit	*p = u;
			for (int i = numNalus; i--;)
				p = p->next();
			return (const entry*)p;
		}
		auto		units() const { return make_range_n(make_next_iterator(u), numNalus); }

	};
	uint8		configurationVersion;
	union {
		bitfield<uint8, 0, 2>		general_profile_space;
		bitfield<uint8, 2, 1>		general_tier_flag;
		bitfield<uint8, 3, 5>		general_profile_idc;
	};
	packed<xint32be>		general_profile_compatibility_flags;
	uintn<6, true>			general_constraint_indicator_flags;
	uint8					general_level_idc;
	packed<uint16be>		min_spatial_segmentation_idc;
	bitfield<uint8, 0, 2>	parallelismType;
	bitfield<uint8, 0, 2>	chromaFormat;
	bitfield<uint8, 0, 3>	bitDepthLumaMinus8;
	bitfield<uint8, 0, 3>	bitDepthChromaMinus8;
	packed<uint16be>		avgFrameRate;
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


//------------

struct AtomType {
	id4				id;
	const ISO::Type	*type;
	const AtomType	*subtypes;

	AtomType(const _none&) : type(0), subtypes(0) {}
	AtomType(uint32 id, const ISO::Type *type, const AtomType *subtypes = 0) : id(id), type(type), subtypes(subtypes) {}
	AtomType(uint32 id, const AtomType *subtypes) : id(id), type(0), subtypes(subtypes) {}
	template<typename T, uint32 ID, typename...S> static AtomType make(S&&... subtypes) {
		static const AtomType	atoms[] = { subtypes..., terminate };
		return AtomType(ID, ISO::getdef<T>(), atoms);
	}
	template<typename T, uint32 ID> static AtomType make() {
		return AtomType(ID, ISO::getdef<T>(), nullptr);
	}
};


AtomType root_atoms[] = {
	AtomType::make<ftyp, 'ftyp'>(),
	AtomType::make<pdin, 'pdin'>(),
	AtomType::make<moov, 'moov'>(
		AtomType::make<mvhd, 'mvhd'>(
			AtomType::make<trak, 'trak'>(
				AtomType::make<tkhd, 'tkhd'>(),
				AtomType::make<tref, 'tref'>(),
				AtomType::make<edts, 'edts'>(
					AtomType::make<elst, 'elst'>()
				),
				AtomType::make<mdia, 'mdia'>(
					AtomType::make<mdhd, 'mdhd'>(),
					AtomType::make<hdlr, 'hdlr'>(),
					AtomType::make<minf, 'minf'>(
						AtomType::make<vmhd, 'vmhd'>(),
						AtomType::make<smhd, 'smhd'>(),
						AtomType::make<hmhd, 'hmhd'>(),
						AtomType::make<nmhd, 'nmhd'>(),
						AtomType::make<dinf, 'dinf'>(
							AtomType::make<url, 'url '>(),
							AtomType::make<urn, 'urn '>(),
							AtomType::make<dref, 'dref'>()
						),
						AtomType::make<stbl, 'stbl'>( 
							AtomType::make<stsd, 'stsd'>(),
							AtomType::make<stts, 'stts'>(),
							AtomType::make<ctts, 'ctts'>(),
							AtomType::make<stsc, 'stsc'>(),
							AtomType::make<stsz, 'stsz'>(),
							AtomType::make<stz2, 'stz2'>(),
							AtomType::make<stco, 'stco'>(),
							AtomType::make<co64, 'co64'>(),
							AtomType::make<stss, 'stss'>(),
							AtomType::make<stsh, 'stsh'>(),
							AtomType::make<padb, 'padb'>(),
							AtomType::make<stdp, 'stdp'>(),
							AtomType::make<sdtp, 'sdtp'>(),
							AtomType::make<sbgp, 'sbgp'>(),
							AtomType::make<sgpd, 'sgpd'>(),
							AtomType::make<subs<0>, 'subs'>()

						)
					)
				)
			)
		),
		AtomType::make<mvex, 'mvex'>(),
		AtomType::make<ipmc, 'ipmc'>()
	),
	AtomType::make<moof, 'moof'>(
		AtomType::make<mfhd, 'mfhd'>(),
		AtomType::make<traf, 'traf'>(
			AtomType::make<tfhd, 'tfhd'>(),
			AtomType::make<trun, 'trun'>(),
			AtomType::make<sdtp, 'sdtp'>(),
			AtomType::make<sbgp, 'sbgp'>(),
			AtomType::make<subs<0>, 'subs'>()
		)
	),
	AtomType::make<mfra, 'mfra'>(
		AtomType::make<tfra, 'tfra'>(),
		AtomType::make<mfro, 'mfro'>()
	),
	AtomType::make<mdat, 'mdat'>(),
	AtomType::make<qtfree, 'free'>(),
	AtomType::make<qtskip, 'skip'>(
		AtomType::make<udta, 'udta'>(
			AtomType::make<cprt, 'cprt'>()
		)
	),
	AtomType::make<qtmeta, 'meta'>(
		AtomType::make<hdlr, 'hdlr'>(),
		AtomType::make<dinf, 'dinf'>(
			AtomType::make<dref, 'dref'>()
		),
		AtomType::make<ipmc, 'ipmc'>(),
		AtomType::make<iloc, 'iloc'>(),
		AtomType::make<iref, 'iref'>(
			AtomType::make<auxl, 'auxl'>(),
			AtomType::make<cdsc, 'cdsc'>(),
			AtomType::make<dimg, 'dimg'>(),
			AtomType::make<thmb, 'thmb'>()
		),
		AtomType::make<iprp, 'iprp'>(
			AtomType::make<ipco, 'ipco'>(
				AtomType::make<void, 'auxC'>(),
				AtomType::make<void, 'av1C'>(),
				AtomType::make<void, 'clap'>(),
				AtomType::make<void, 'colr'>(),
				AtomType::make<HEVCDecoderConfigurationRecord, 'hvcC'>(),
				AtomType::make<void, 'irot'>(),
				AtomType::make<void, 'ispe'>(),
				AtomType::make<void, 'pasp'>(),
				AtomType::make<void, 'pixi'>(),
				AtomType::make<void, 'rloc'>()
			),
			AtomType::make<ipma, 'ipma'>()
		),
		AtomType::make<ipro, 'ipro'>(
			AtomType::make<qtsinf, 'sinf'>(
				AtomType::make<frma, 'frma'>(),
				AtomType::make<imif, 'imif'>(),
				AtomType::make<schm, 'schm'>(),
				AtomType::make<schi, 'schi'>()
			)
		),
		AtomType::make<iinf, 'iinf'>(
			AtomType::make<infe, 'infe'>()
		),
		AtomType::make<xml , 'xml '>(),
		AtomType::make<bxml, 'bxml'>(),
		AtomType::make<pitm, 'pitm'>()
	),
	terminate,
};

//------------

template<typename T> ISO_DEFCOMPT(qt_table,T,1)	{ ISO_SETFIELD(0, entries); }};

ISO_DEFSAME(id4, char[4]);
ISO_DEFSAME(version_flags, uint32);
template<int VER> ISO_DEFCOMPBVT(version_flags_time, VER, version_flags, creation_time, modification_time);

ISO_DEFUSERCOMPV(ftyp, major_brand, minor_version);// , compatible_brands);
ISO_DEFSAME(data_entry, version_flags);
ISO_DEFUSERCOMPBV(url, data_entry, location);
ISO_DEFUSERCOMPBV(urn, data_entry, name, location);

ISO_DEFUSERCOMPBV(mvhd, version_flags_time<0>,
	time_scale,duration,preferred_rate,preferred_volume,reserved,matrix,preview_time,preview_duration,
	poster_time,selection_time,selection_duration,current_time,next_track
);

ISO_DEFUSERCOMPBV(tkhd, version_flags_time<0>,
	id,reserved,duration,reserved2,layer,alternate_group,volume,reserved3,
	matrix,width,height
);

ISO_DEFUSERCOMPBV(mdhd, version_flags_time<0>,
	time_scale,duration,language,quality
);

ISO_DEFUSERCOMPBV(vmhd, version_flags,
	graphics_mode,op_color
);

ISO_DEFUSERCOMPBV(smhd, version_flags,
	balance
);

ISO_DEFUSERCOMPBV(hmhd, version_flags,
	maxPDUsize, avgPDUsize, maxbitrate, avgbitrate
);


ISO_DEFUSERCOMPBV(hdlr, version_flags,
	type,subtype,manufacturer,flags2,flags2_mask,name
);

//sample descriptor
ISO_DEFUSERCOMPV(sample_desc,size,format,reserved,data_ref_index);

ISO_DEFUSERCOMPBV(video_sample_desc, sample_desc,
	version,revision_level,vendor,temporal_quality,spatial_quality,width,height,horizontal_resolution,
	verical_resolution,data_size,frame_count,compressor_name,depth,colortable_id
);

ISO_DEFUSERCOMPV(time_to_sample,sample_count,sample_delta);
ISO_DEFUSERCOMPV(sample_to_chunk,first_chunk,samples_per_chunk,sample_desc_id);
ISO_DEFUSERCOMPV(composition_offset,sample_count, sample_offset);

ISO_DEFUSERCOMPBV(stsz, version_flags,
	sample_size, count, entries
);

ISO_DEFUSERCOMPBV(dref, version_flags, count);
ISO_DEFUSERCOMPBV(padb, version_flags, count);
ISO_DEFUSERCOMPBV(stz2, version_flags, field_size, count);
ISO_DEFUSERCOMPV(shadow_sync_sample, shadowed_sample_number, sync_sample_number);

ISO_DEFUSERCOMPBV(cprt, version_flags, language, notice);

template<int VER> ISO_DEFCOMPVT(elst_entry, VER, segment_duration, media_time, media_rate_int, media_rate_frac);
template<int VER> ISO_DEFCOMPBVT(mehd, VER, version_flags, fragment_duration);

ISO_DEFUSERCOMPBV(mfhd, version_flags, sequence_number);
ISO_DEFUSERCOMPBV(tfhd, version_flags, track_ID, base_data_offset, sample_description_index, default_sample_duration, default_sample_size, default_sample_flags);
ISO_DEFUSERCOMPBV(trun, version_flags, data_offset, first_sample_flags, entries);

ISO_DEFUSERCOMPV(trun::entry, sample_duration, sample_size, sample_flags, sample_composition_time_offset);

ISO_DEFUSERCOMPBV(tfra, version_flags, track_ID);
ISO_DEFUSERCOMPBV(mfro, version_flags, size);
ISO_DEFUSERCOMPBV(sbgp, version_flags, grouping_type, count);
ISO_DEFUSERCOMPBV(sdtp, version_flags, dummy);
ISO_DEFUSERCOMPBV(sgpd, version_flags, grouping_type, count);

template<int VER> ISO_DEFUSERCOMPVT(subs_subsample, VER, subsample_size, subsample_priority, discardable);
template<int VER> ISO_DEFUSERCOMPVT(subs_entry, VER, sample_delta, entries);
template<int VER> ISO_DEFUSERCOMPBVT(subs, VER, version_flags, entries);

ISO_DEFUSERCOMPBV(xml, version_flags, xml);
ISO_DEFUSERCOMPBV(bxml, version_flags, data);
ISO_DEFUSERCOMPBV(pitm, version_flags, item_id);
ISO_DEFUSERCOMPV(iloc::entry, item_id, data_reference_index, base_offset, count);
ISO_DEFUSERCOMPBV(iloc, version_flags, entries);

ISO_DEFUSERCOMPBV(infe, version_flags, item_id, item_protection_index, item_name, content_type, content_encoding);
ISO_DEFUSERCOMPBV(iinf, version_flags, count);


template<typename T> struct is_bit_field_t {
	template<typename U> static constexpr bool helper(...)					{ return true;	}
	template<typename U> static constexpr bool helper(decltype(&U::a) arg)	{ return false; }
	static constexpr bool value = helper<T>(nullptr); 
};


ISO_DEFUSERCOMPV(HEVCDecoderConfigurationRecord::entry, array_completeness, NAL_unit_type, units);
ISO_DEFUSERCOMPV(HEVCDecoderConfigurationRecord::unit, get);

ISO_DEFUSERCOMPV(HEVCDecoderConfigurationRecord,
	configurationVersion,
	general_profile_space, general_tier_flag, general_profile_idc,
	general_profile_compatibility_flags,
	general_constraint_indicator_flags,
	general_level_idc,
	min_spatial_segmentation_idc,
	parallelismType,
	chromaFormat,
	bitDepthLumaMinus8,
	bitDepthChromaMinus8,
	avgFrameRate,
	constantFrameRate, numTemporalLayers, temporalIdNested, lengthSizeMinusOne,
	entries
);

//------------

void ReadAtoms(anything *b, istream_ref file, streamptr end, const AtomType *atom_types) {
	atom	a;
	while ((end == 0 || file.tell() < end) && file.read(a)) {
		if (a.size == 0 && end == 0) {
			throw_accum("unterminated QT at 0x" << hex(file.tell()));
			end = file.length();
		}

		tag		id1		= a.id;
		uint64	size	= a.size;
		uint64	end1	= file.tell() + size;
		if (int64(size) < 0 || (end && end1 > end)) {
			//throw_accum("malformed QT at 0x" << hex(file.tell()));
			return;
		}

		const AtomType *i = atom_types;
		while (i->id && i->id != a.id)
			++i;

		if (i->id) {
			if (i->subtypes) {
				ISO_ptr<anything>	b2(id1);
				if (i->type) {
					auto	p = ISO::MakePtr(i->type, id1);
					//void *p = ISO::MakeRawPtrSize<32>(i->type, id1, size);
					file.readbuff(p, i->type->GetSize());
					b2->Append(p);
					//if (auto x = i->type->GetSize() & 7)
					//	file.seek_cur(8 - x);
				}
				ReadAtoms(b2, file, end1, i->subtypes);
				b->Append(b2);

			} else if (i->type) {
				void *p = ISO::MakeRawPtrSize<32>(i->type, id1, size);
				file.readbuff(p, size);
				b->Append(ISO_ptr<void>::Ptr(p));

			} else {
				b->Append(ISO::MakePtr(id1, malloc_block(file, size)));
			}
		} else {
			b->Append(ISO::MakePtr(id1, malloc_block(file, size)));
		}
	}
}

class QTFileHandler : public FileHandler {
	const char*		GetExt()	override { return "mov";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		atom	a = file.get();
		return	a.id == 'ftyp' ? CHECK_PROBABLE : CHECK_UNLIKELY;
	}
public:
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	p(id);
		ReadAtoms(p, file, file.length(), root_atoms);
		return p;
	}
	QTFileHandler()		{ ISO::getdef<movie>(); }
} qt;

class MP4FileHandler : public QTFileHandler {
	const char*		GetExt()	override { return "mp4";	}
	const char*		GetMIME()	override { return "video/mp4"; }
} mp4;


class HEICFileHandler : public QTFileHandler {
	const char*		GetExt()	override { return "heic"; }
} heic;


//aligned(8) class HEVCItemData
//{
//	unsigned int PictureLength = sample_size; //Size of Item
//	for (i=0; i<PictureLength; )		// to end of the picture
//	{
//		unsigned int((DecoderConfigurationRecord.LengthSizeMinusOne+1)*8)
//			NALUnitLength;
//		bit(NALUnitLength * 8) NALUnit;
//		i += (DecoderConfigurationRecord.LengthSizeMinusOne+1) + NALUnitLength;
//	}
//}

struct ccst : version_flags {
	uint32	AllReferencePicturesIntra : 1, IntraPredUsed : 1, MaxRefPerPic : 4, : 26;
};
/*
class VisualSampleToMetadataItemEntry() 
	extends VisualSampleGroupEntry (’vsmi’) {
	unsigned int(32) meta_box_handler_type;
	unsigned int(16) num_items;
	for(i = 0; i < num_items; i++) {
		unsigned int(16) item_id[i];
	}
}

aligned(8) class MetadataIntegrityBox
extends FullBox(‘mint’, version = 0, 0) {
	MD5IntegrityBox(); // one or more MD5IntegrityBox
}

aligned(8) class MD5IntegrityBox(){
	string input_MD5; 
	unsigned int(32) input_4cc; 
	if (input_4cc == ‘sgpd’) {
		unsigned int(32) grouping_type; 
	}
}

aligned(8) class ExifDataBlock() {
	unsigned int(32) exif_tiff_header_offset;
	signed   int(32) exif_offset_correction;  // needed?
	unsigned int(8)  exif_payload[];
}

*/
