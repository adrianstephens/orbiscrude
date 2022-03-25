#include "filetypes/ebml.h"
#include "base/array.h"
#include "base/maths.h"

namespace matroska {
using namespace iso;

enum TrackType {
	TRACK_TYPE_VIDEO	= 0x1,
	TRACK_TYPE_AUDIO	= 0x2,
	TRACK_TYPE_COMPLEX	= 0x3,
	TRACK_TYPE_LOGO		= 0x10,
	TRACK_TYPE_SUBTITLE = 0x11,
	TRACK_TYPE_CONTROL	= 0x20,
};

enum EyeMode {
	EYE_MODE_MONO	= 0x0,
	EYE_MODE_RIGHT	= 0x1,
	EYE_MODE_LEFT	= 0x2,
	EYE_MODE_BOTH	= 0x3,
};

enum RatioMode {
	ASPECT_RATIO_MODE_FREE	= 0x0,
	ASPECT_RATIO_MODE_KEEP	= 0x1,
	ASPECT_RATIO_MODE_FIXED = 0x2,
};

#define MATROSKA_CODEC_ID_VIDEO_VFW_FOURCC	"V_MS/VFW/FOURCC"
#define MATROSKA_CODEC_ID_AUDIO_ACM			"A_MS/ACM"

const int E_PARSE_FAILED		= -1;
const int E_FILE_FORMAT_INVALID = -2;
const int E_BUFFER_NOT_FULL		= -3;

class Segment;
class Cluster;

struct MKVSection {
	Segment* const	segment;
	file_range		range;
	MKVSection(Segment *_segment, const file_range &_range) : segment(_segment), range(_range) {}

};

//-----------------------------------------------------------------------------
//	Block
//-----------------------------------------------------------------------------

class Block {
public:
	enum Lacing { LacingNone, LacingXiph, LacingFixed, LacingEbml };
	struct Frame {
		int64	pos;  // absolute offset
		int32	len;
	};

	const file_range		range;
	dynamic_array<Frame>	frames;
	int64					track;
	uint16					timecode;  // relative to cluster
	uint16					flags;

	static Lacing	GetLacing(uint8 flags)	{ return Lacing((flags & 0x06) >> 1); }

	Block(const file_range &_range) : range(_range), track(0), timecode(uint16(~0)), flags(0)  {}
	int		Parse(const Cluster *cluster);
	int64	GetTimeCode(Cluster *cluster)	const;
	int64	GetTime(Cluster *cluster)		const;
	bool	IsKey()			const	{ return !!(flags & 0x80); }
	bool	IsInvisible()	const	{ return !!(flags & 0x08); }
	bool	DiscardPadding() const	{ return !!(flags & 0x100); }
	Lacing	GetLacing()		const	{ return GetLacing(flags); }
};

class BlockEntry {
public:
	enum Kind { kBlockSimple, kBlockGroup };
protected:
	BlockEntry(Cluster *_cluster, uint32 _index, const file_range &_range, Kind _kind) : cluster(_cluster), index(_index), kind(_kind), block(_range) {}
public:
	Cluster* const	cluster;
	const uint32	index;
	Kind			kind;
	Block			block;

	virtual ~BlockEntry() {}
	const Block*		GetBlock()	const	{ return &block; }
	Kind				GetKind()	const	{ return kind; }
};

class SimpleBlock : public BlockEntry {
public:
	enum {
		ID					= 0xA3,
	};
	SimpleBlock(Cluster *cluster, uint32 index, const file_range &_range) : BlockEntry(cluster, index, _range, kBlockSimple) {}
};

class BlockGroup : public BlockEntry {
public:
	enum {
		ID					= 0xA0,
		ID_PrevSize			= 0xAB,
		ID_Block			= 0xA1,
		ID_Duration			= 0x9B,
		ID_Reference		= 0xFB,
		ID_DiscardPadding	= 0x75A2,
		ID_LaceNumber		= 0xCC,
		ID_BlockAdditions	= 0x75A1,
		ID_BlockMore		= 0xA6,
		ID_BlockAddID		= 0xEE,
		ID_BlockAdditional	= 0xA5,
	};
	const int64 prev, next, duration;
	BlockGroup(Cluster *cluster, uint32 index, const file_range &_range, int64 _prev, int64 _next, int64 _duration)
		: BlockEntry(cluster, index, _range, kBlockGroup)
		, prev(_prev), next(_next), duration(_duration)
	{}
};

//-----------------------------------------------------------------------------
//	ContentEncoding
//-----------------------------------------------------------------------------

class ContentEncoding {
public:
	struct ContentCompression {
		enum {
			ID			= 0x5034,
			ID_Algo		= 0x4254,
			ID_Settings = 0x4255,
		};
		uint64			algo;
		malloc_block	settings;
		ContentCompression() : algo(0) {}
	};
	struct ContentEncAESSettings {
		enum {
			ID					= 0x47E7,
			ID_CipherMode		= 0x47E8,
			ID_CipherInitData	= 0x47E9,
		};
		enum { kCTR = 1 };
		ContentEncAESSettings() : cipher_mode(kCTR) {}
		uint64 cipher_mode;
	};
	struct ContentEncryption {
		enum {
			ID				= 0x5035,
			ID_Algo			= 0x47E1,
			ID_KeyID		= 0x47E2,
			ID_Signature	= 0x47E3,
			ID_SigKeyID		= 0x47E4,
			ID_SigAlgo		= 0x47E5,
			ID_SigHashAlgo	= 0x47E6,
		};
		uint64			algo;
		uint64			sig_algo;
		uint64			sig_hash_algo;
		malloc_block	key_id;
		malloc_block	signature;
		malloc_block	sig_key_id;
		ContentEncAESSettings aes_settings;
		ContentEncryption() : algo(0), sig_algo(0), sig_hash_algo(0) {}
	};
private:
	ContentEncoding(const ContentEncoding&);
	ContentEncoding& operator=(const ContentEncoding&);
public:
	enum {
		ID			= 0x6240,
		ID_Order	= 0x5031,
		ID_Scope	= 0x5032,
		ID_Type		= 0x5033,
	};
	// ContentEncoding element names
	uint64 encoding_order;
	uint64 encoding_scope;
	uint64 encoding_type;
	dynamic_array<ContentCompression>	compression_entries;
	dynamic_array<ContentEncryption>	encryption_entries;

	ContentEncoding() : encoding_order(0), encoding_scope(1), encoding_type(0) {}
	ContentEncoding(ContentEncoding&&)=default;

	int ParseCompressionEntry(EBMLreader &reader, ContentCompression* compression);
	int ParseContentEncAESSettingsEntry(EBMLreader &reader, ContentEncAESSettings* aes);
	int ParseContentEncodingEntry(EBMLreader &reader);
	int ParseEncryptionEntry(EBMLreader &reader, ContentEncryption* encryption);
};

//-----------------------------------------------------------------------------
//	Tracks
//-----------------------------------------------------------------------------

class TrackInfo {
public:
	enum {
		ID					= 0xAE,
		ID_Number			= 0xD7,
		ID_UID				= 0x73C5,
		ID_Type				= 0x83,
		ID_Audio			= 0xE1,
		ID_Video			= 0xE0,
		ID_ContentEncodings = 0x6D80,

		ID_Name				= 0x536E,
		ID_Language			= 0x22B59C,
		ID_FlagEnabled		= 0xB9,
		ID_FlagDefault		= 0x88,
		ID_FlagLacing		= 0x9C,
		ID_Mincache			= 0x6DE7,
		ID_Maxcache			= 0x6DF8,
		ID_DefaultDuration	= 0x23E383,
		ID_SeekPreRoll		= 0x56BB,

		ID_Codec_ID			= 0x86,
		ID_Codec_Private	= 0x63A2,
		ID_Codec_Name		= 0x258688,
		ID_Codec_InfoURL	= 0x3B4040,
		ID_Codec_DownloadURL= 0x26B240,
		ID_Codec_Delay		= 0x56AA,
	};
	int32			type;
	int32			number;
	uint64			uid;
	string			name;
	string			language;
	uint64			defaultDuration;
	uint64			seekPreRoll;
	bool			lacing;

	uint64			codec_delay;
	string			codec_id;
	string			codec_name;
	malloc_block	codec_private;

	file_range		settings;

	TrackInfo() : uid(0), defaultDuration(0), seekPreRoll(0), codec_delay(0), settings(0, 0) {}
	void	Copy(TrackInfo &b) const {
		b = *this;
	}
};

class Track : MKVSection, public TrackInfo {
protected:
	Track(Segment *_segment, const file_range &_range) : MKVSection(_segment, _range) {}

public:
	enum Type { kVideo = 1, kAudio = 2, kSubtitle = 0x11, kMetadata = 0x21 };
	dynamic_array<ContentEncoding> content_encoding_entries;

	static int Create(Segment*, const TrackInfo&, EBMLreader &reader, Track*&);

	bool			VetEntry(const BlockEntry*) const;
	int				Seek(int64 time_ns, const BlockEntry*&) const;
	int				GetFirst(const BlockEntry*&) const;
	int				GetNext(const BlockEntry* curr, const BlockEntry*& next) const;
	int				ParseContentEncodingsEntry(const file_range &range);
};

class VideoTrack : public Track {
public:
	enum {
		ID_Framerate		= 0x2383E3,
		ID_DisplayWidth		= 0x54B0,
		ID_DisplayHeight	= 0x54BA,
		ID_PixelWidth		= 0xB0,
		ID_PixelHeight		= 0xBA,
		ID_FlagInterlaced	= 0x9A,
		ID_DisplayUnit		= 0x54B2,
		ID_StereoMode		= 0x53B9,
		ID_AspectRatio		= 0x54B3,
		ID_Colourspace		= 0x2EB524,
		ID_FrameRate		= 0x2383E3,
	};
	int64	width;
	int64	height;
	int64	display_width;
	int64	display_height;
	int64	display_unit;
	int64	stereo_mode;
	double	rate;

	VideoTrack(Segment *_segment, const file_range &_range)
		: Track(_segment, _range)
		, width(0), height(0)
		, display_width(0), display_height(0)
		, display_unit(0), stereo_mode(0)
		, rate(0)
	{}
	int		Parse(Segment*, const TrackInfo&);
};

class AudioTrack : public Track {
public:
	enum {
		ID_SamplingFreq		= 0xB5,
		ID_OutsamplingFreq	= 0x78B5,
		ID_Bitdepth			= 0x6264,
		ID_Channels			= 0x9F,
	};
	double	rate;
	int64	channels;
	int64	bit_depth;

	AudioTrack(Segment *_segment, const file_range &_range)
		: Track(_segment, _range)
		, rate(8000), channels(1), bit_depth(0)
	{}
	int		Parse(Segment*, const TrackInfo&);
};

class Tracks : MKVSection, public dynamic_array<Track*> {
	int			ParseTrackEntry(EBMLreader &reader, Track*&) const;
public:
	enum {
		ID				= 0x1654AE6B,
		ID_TrackEntry	= 0xAE,
	};
	Tracks(Segment *_segment, const file_range &_range) : MKVSection(_segment, _range) {}
	~Tracks();
	int				Parse();
	const Track*	GetTrackByNumber(int32 tn) const;
};

//-----------------------------------------------------------------------------
//	Chapters
//-----------------------------------------------------------------------------

class Chapters : MKVSection {
public:
	enum {
	  ID				= 0x1043A770,
	  ID_EditionEntry	= 0x45B9,
	  ID_Atom			= 0xB6,
	  ID_UID			= 0x73C4,
	  ID_StringUID		= 0x5654,
	  ID_TimeStart		= 0x91,
	  ID_TimeEnd		= 0x92,
	  ID_Display		= 0x80,
	  ID_String			= 0x85,
	  ID_Language		= 0x437C,
	  ID_Country		= 0x437E,
	};
	class Atom;
	class Edition;
	class Display {
		friend class Atom;
		void	Clear() { chapstring.clear(); language.clear(); country.clear(); }
		int		Parse(EBMLreader &reader);
	public:
		string	chapstring;
		string	language;
		string	country;
	};

	class Atom {
		friend class Edition;
		void	Clear() { string_uid.clear(); displays.clear(); }
		int		Parse(EBMLreader &reader);
		static int64 GetTime(const Chapters*, int64 timecode);
	public:
		string	string_uid;
		uint64	uid;
		int64	start_timecode;
		int64	stop_timecode;
		dynamic_array<Display> displays;
		Atom() : uid(0), start_timecode(-1), stop_timecode(-1) {}
		int64 GetStartTime(const Chapters* chapters)	const { return GetTime(chapters, start_timecode); }
		int64 GetStopTime(const Chapters* chapters)		const { return GetTime(chapters, stop_timecode); }
	};

	class Edition {
		friend class Chapters;
		void	Clear()	{ atoms.clear(); }
		int		Parse(EBMLreader &reader);
	public:
		dynamic_array<Atom>	atoms;
	};

	dynamic_array<Edition> editions;

	Chapters(Segment *_segment, file_range &_range) : MKVSection(_segment, _range) {}
	int		Parse();
};

//-----------------------------------------------------------------------------
//	Tags
//-----------------------------------------------------------------------------

class Tags : MKVSection {
public:
	enum {
		ID				= 0x1254C367,
		ID_Tag			= 0x7373,
		ID_SimpleTag	= 0x67C8,
		ID_Name			= 0x45A3,
		ID_String		= 0x4487
	};
	class Tag;
	class SimpleTag;

	class SimpleTag {
		friend class Tag;
		int		Parse(EBMLreader &reader);
	public:
		string	tag_name;
		string	tag_string;
	};

	class Tag {
		friend class Tags;
		int		Parse(EBMLreader &reader);
	public:
		dynamic_array<SimpleTag> simple_tags;
	};

	dynamic_array<Tag>	tags;

	Tags(Segment *_segment, const file_range &_range) : MKVSection(_segment, _range) {}
	int		Parse();
};

//-----------------------------------------------------------------------------
//	Cues
//-----------------------------------------------------------------------------

class CuePoint {
public:
	enum {
		ID		= 0xBB,
		ID_Time	= 0xB3,
	};
	struct TrackPosition {
		enum {
			ID					= 0xB7,
			ID_Track			= 0xF7,
			ID_ClusterPosition	= 0xF1,
			ID_BlockNumber		= 0x5378,
		};
		int64	track;
		uint64	pos;  // of cluster
		int64	block;
		TrackPosition() : track(-1), pos(maximum), block(1) {}
		bool Parse(EBMLreader &reader);
	};
	dynamic_array<TrackPosition> track_positions;
	const uint32	index;
	int64			timecode;

	CuePoint(uint32 _index, int64 pos) : index(_index), timecode(-pos) {}
	bool	Load(EBMLreader &reader);
	int64	GetTime(const Segment *segment) const;
	const TrackPosition* Find(const Track*) const;
};

class Cues : MKVSection, public dynamic_array<CuePoint> {
	friend class Segment;
	uint32	loaded_count;
	int64	pos;

	Cues(Segment *_segment, const file_range &_range) : MKVSection(_segment, _range), loaded_count(0), pos(_range.start) {}
	void Init();
public:
	enum {
		ID					= 0x1C53BB6B,
	};

	const CuePoint::TrackPosition	*Find(int64 time_ns, const Track *track, const CuePoint*&cp) const;
	const CuePoint*		GetFirst() const;
	const CuePoint*		GetLast() const;
	const CuePoint*		GetNext(const CuePoint*) const;
	const BlockEntry*	GetBlock(const CuePoint*, const CuePoint::TrackPosition*) const;
	bool	LoadCuePoint();
	int32	GetCount()		const	{ return loaded_count; }  // loaded only
	bool	DoneParsing()	const	{ return pos >= range.end(); }
};

//-----------------------------------------------------------------------------
//	Cluster
//-----------------------------------------------------------------------------

class Cluster : MKVSection {
	friend class Segment;
	uint64	pos;
	int64	timecode;

	int		ParseSimpleBlock(EBMLreader &reader);
	int		ParseBlockGroup(EBMLreader &reader);

protected:
	Cluster(Segment *_segment, uint32 _index, const file_range &_range) : MKVSection(_segment, _range), pos(_range.start), timecode(-1), index(_index) {}

public:
	enum {
		ID			= 0x1F43B675,
		ID_Timecode	= 0xE7,
	};
	dynamic_array<BlockEntry*> entries;
	int32	index;

	static Cluster* Create(Segment*, int32 index, const file_range &range);
	static Cluster *GetEOS() { return 0; }

	~Cluster();
	int64	GetTimeCode();					// absolute, but not scaled
	int64	GetTime();						// absolute, and scaled (nanosecond units)
	int64	GetFirstTime();					// time (ns) of first (earliest) block
	int64	GetLastTime();					// time (ns) of last (latest) block
	int		GetFirst(const BlockEntry*&);
	int		GetLast(const BlockEntry*&);
	int		GetNext(const BlockEntry* curr, const BlockEntry*& next);

	static bool			HasBlockEntries(const Segment*, const file_range &range);
	static Cluster**	FindByTime(Cluster** const begin, Cluster** const end, int64 time);
	static Cluster**	FindByPos(Cluster** const begin, Cluster** const end, int64 pos);
	const BlockEntry*	GetEntry(const Track*, int64 ns = -1);
	const BlockEntry*	GetEntry(const CuePoint&, const CuePoint::TrackPosition&);

	int			Load();
	int			Parse();
	istream_ref	File()			const;
	int64		TimecodeScale()	const;
};

//-----------------------------------------------------------------------------
//	Segments
//-----------------------------------------------------------------------------

class SegmentInfo : MKVSection {
public:
	enum {
		ID				= 0x1549A966,
		ID_TimecodeScale = 0x2AD7B1,
		ID_Duration		= 0x4489,
		ID_DateUTC		= 0x4461,
		ID_Title		= 0x7BA9,
		ID_MuxingApp	= 0x4D80,
		ID_WritingApp	= 0x5741,
	};

	double	duration;
	int64	timecode_scale;
	string	muxingApp;
	string	writingApp;
	string	title;

	SegmentInfo(Segment *_segment, const file_range &_range) : MKVSection(_segment, _range) {}
	int		Parse();
	int64	GetDuration() const { return duration < 0 ? -1 : static_cast<int64>(duration * timecode_scale); }
};

class SeekHead : MKVSection {
public:
	enum {
		ID				= 0x114D9B74,
		ID_Void			= 0xEC,
		ID_Seek			= 0x4DBB,
		ID_SeekID		= 0x53AB,
		ID_SeekPosition = 0x53AC,
	};
	struct Entry {
		int64	id, pos;
		bool	Parse(istream_ref file);
	};
	dynamic_array<Entry>		entries;
	dynamic_array<file_range>	void_elements;

	SeekHead(Segment *_segment, const file_range &_range) : MKVSection(_segment, _range) {}
	int		Parse();
};

class Segment {
	friend class Cues;
	friend class Track;
	friend class VideoTrack;

	uint64			pos;				// absolute file posn; what has been consumed so far
	Cluster			*unknownSize;
	dynamic_array<Cluster*> clusters;
	int32			loaded_clusters;	// number of clusters for which index >= 0

	int		DoLoadCluster();
	int		DoParseNext(Cluster* &result);
	bool	AppendCluster(Cluster *cluster);
	bool	PreloadCluster(Cluster *cluster, uint32 idx);
	const BlockEntry* GetBlock(const CuePoint &cp, const CuePoint::TrackPosition &tp);
	Segment(istream_ref file, const file_range &range) : unknownSize(0), loaded_clusters(0), file(file), range(range), info(0), cues(0), tracks(0), seekHead(0), chapters(0), tags(0) {}

public:
	enum {
		ID	= 0x18538067,
	};
	istream_ref			file;
	const file_range	range;

	SegmentInfo			*info;
	Cues				*cues;
	Tracks				*tracks;
	SeekHead			*seekHead;
	Chapters			*chapters;
	Tags				*tags;

	static int64		CreateInstance(istream_ref file, int64 pos, Segment *&segment);

	~Segment();

	bool		DoneParsing() const;
	int			Load();				// loads headers and all clusters
	int64		ParseHeaders();		// stops when first cluster is found
	int			LoadCluster();		// load one cluster
	int			ParseNext(Cluster *curr, Cluster *&next);
	Cluster*	GetFirst();
	Cluster*	GetLast();
	Cluster*	GetNext(const Cluster *cluster);
	Cluster*	FindCluster(int64 time_nanoseconds);
	Cluster*	FindOrPreloadCluster(uint64 pos, int from = 0);

	int			GetAvail(uint64 &total, uint64 &avail) const;
};

inline istream_ref	Cluster::File()			const	{ return segment->file; }
inline int64	Cluster::TimecodeScale()	const	{ return segment->info->timecode_scale; }

//-----------------------------------------------------------------------------
//	TrackReader
//-----------------------------------------------------------------------------

struct TrackReader {
	Segment				*segment;
	Cluster				*cluster;
	const BlockEntry	*block_entry;
	int					block_frame_index;
	int					track_index;

	TrackReader() : segment(0) {}
	TrackReader(Segment *_segment, int _track_index) : segment(_segment), track_index(_track_index) {
		Rewind();
	}
	void	Init(Segment *_segment, int _track_index) {
		segment		= _segment;
		track_index = _track_index;
		Rewind();
	}
	void Rewind() {
		cluster		= segment->GetFirst();
		block_entry	= NULL;
	}
	void WindTo(int64 time_nanoseconds) {
		cluster		= segment->FindCluster(time_nanoseconds);
		block_entry	= NULL;
	}
	uint64	GetTimestamp()		{
		return block_entry->block.GetTime(cluster);
	}
	Block::Frame *GetNextFrame();
	Block::Frame *GetNextFrame(int *_track_index) {
		*_track_index = track_index;
		return GetNextFrame();
	}
};

struct MultiTrackReader {
	Segment				*segment;
	Cluster				*cluster;
	const BlockEntry	*block_entry;
	int					block_frame_index;
	uint64				track_filter;

	MultiTrackReader() : segment(0) {}
	MultiTrackReader(Segment *_segment, uint64 _track_filter) : segment(_segment), track_filter(_track_filter) {
		Rewind();
	}
	void	Init(Segment *_segment, uint64 _track_filter) {
		segment			= _segment;
		track_filter	= _track_filter;
		Rewind();
	}
	void Rewind() {
		cluster		= segment->GetFirst();
		block_entry	= NULL;
	}
	void WindTo(int64 time_nanoseconds) {
		cluster		= segment->FindCluster(time_nanoseconds);
		block_entry	= NULL;
	}
	uint64	GetTimestamp()		{
		return block_entry->block.GetTime(cluster);
	}
	Block::Frame *GetNextFrame(int *track_index);
};

} //namespace matroska
