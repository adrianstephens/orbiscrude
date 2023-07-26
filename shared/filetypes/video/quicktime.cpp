#include "movie.h"
#include "../bitmap/bitmapfile.h"
#include "../bitmap/tiff.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "codec/video/h265.h"
#include "extra/icc_profile.h"
#include "extra/date.h"

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
			tapt
				clef	clean aperture dimension
				prof	production "	"
				enof	encoded pixels dimension
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

/*
©arg	Name of arranger
©ark	Keywords for arranger							X
©cok	Keywords for composer							X
©com	Name of composer
©cpy	Copyright statement
©day	Date the movie content was created
©dir	Name of movie’s director
©ed1 ... ©ed9	Edit dates and descriptions
©fmt	Indication of movie format (computer-generated, digitized, and so on)
©inf	Information about the movie
©isr	ISRC code
©lab	Name of record label
©lal	URL of record label
©mak	Name of file creator or maker
©mal	URL of file creator or maker
©nak	Title keywords of the content					X
©nam	Title of the content
©pdk	Keywords for producer							X
©phg	Recording copyright statement, normally preceded by the symbol ../art/phono_symbol.gif
©prd	Name of producer
©prf	Names of performers
cprk	Keywords of main artist and performer			X
©prl	URL of main artist and performer
©req	Special hardware and software requirements
©snk	Subtitle keywords of the content				X
©snm	Subtitle of content
©src	Credits for those who provided movie source content
©swf	Name of songwriter
©swk	Keywords for songwriter							X
©swr	Name and version number of the software (or hardware) that generated this movie
©wrt	Name of movie’s writer
AllF	Play all frames—byte indicating that all frames of video should be played, regardless of timing
hinf	Hint track information—statistical data for real-time streaming of a particular track. For more information, see Hint Track User Data Atom.
hnti	Hint info atom—data used for real-time streaming of a movie or a track. For more information, see Movie Hint Info Atom and Hint Track User Data Atom.
name	Name of object
tnam	Localized track name optionally present in Track user data. The payload is described in Track Name.
tagc	Media characteristic optionally present in Track user data—specialized text that describes something of interest about the track. For more information, see Media Characteristic Tags.
LOOP	Long integer indicating looping style. This atom is not present unless the movie is set to loop. Values are 0 for normal looping, 1 for palindromic looping.
ptv 	Print to video—display movie in full screen mode. This atom contains a 16-byte structure, described in Print to Video (Full Screen Mode).
SelO	Play selection only—byte indicating that only the selected area of the movie should be played
WLOC	Default window location for movie—two 16-bit values, {x,y}

//https://cconcolato.github.io/mp4ra/atoms.html

ainf	Asset information to identify, license and play					DECE
avcn	AVC NAL Unit Storage Box										DECE
bloc	Base location and purchase location for license acquisition		DECE
bpcc	Bits per component												JPEG2000
buff	Buffering information											NALu Video
bxml	binary XML container											ISO
ccid	OMA DRM Content ID												OMA DRM 2.1
cdef	type and ordering of the components within the codestream		JPEG2000
clip	Reserved														ISO
cmap	mapping between a palette and codestream components				JPEG2000
co64	64-bit chunk offset												ISO
coin	Content Information Box											DECE
colr	specifies the colourspace of the image							JPEG2000
crgn	Reserved														ISO
crhd	reserved for ClockReferenceStream header						MP4v1
cslg	composition to decode timeline mapping							ISO
ctab	Reserved														ISO
ctts	(composition) time to sample									ISO
cvru	OMA DRM Cover URI												OMA DRM 2.1
dinf	data information box, container									ISO
dref	data reference box, declares source(s) of media data in track	ISO
dsgd	DVB Sample Group Description Box								DVB
dstg	DVB Sample to Group Box											DVB
edts	edit list container												ISO
elst	an edit list													ISO
emsg	event message													DASH
evti	Event information												ATSC 3.0
fdel	File delivery information (item info extension)					ISO
feci	FEC Informatiom													ISO
fecr	FEC Reservoir													ISO
fiin	FD Item Information												ISO
fire	File Reservoir													ISO
fpar	File Partition													ISO
free	free space														ISO
frma	original format box												ISO
ftyp	file type and compatibility										ISO
gitn	Group ID to name												ISO
grpi	OMA DRM Group ID												OMA DRM 2.0
hdlr	handler, declares the media (handler) type						ISO
hmhd	hint media header, overall information (hint track only)		ISO
hpix	Hipix Rich Picture (user-data or meta-data)						Hipix
icnu	OMA DRM Icon URI												OMA DRM 2.0
ID32	ID3 version 2 container											id3v2
idat	Item data														ISO
ihdr	Image Header													JPEG2000
iinf	item information												ISO
iloc	item location													ISO
imap	Reserved														ISO
imif	IPMP Information box											ISO
infe	Item information entry											ISO
infu	OMA DRM Info URL												OMA DRM 2.0
iods	Object Descriptor container box									MP4v1
iphd	reserved for IPMP Stream header									MP4v1
ipmc	IPMP Control Box												ISO
ipro	item protection													ISO
iref	Item reference													ISO
jP  	JPEG 2000 Signature												JPEG2000
jp2c	JPEG 2000 contiguous codestream									JPEG2000
jp2h	Header															JPEG2000
jp2i	intellectual property information								JPEG2000
kmat	Reserved														ISO
leva	Leval assignment												ISO
load	Reserved														ISO
loop	Looping behavior												WhatsApp
lrcu	OMA DRM Lyrics URI												OMA DRM 2.1
m7hd	reserved for MPEG7Stream header									MP4v1
matt	Reserved														ISO
mdat	media data container											ISO
mdhd	media header, overall information about the media				ISO
mdia	container for the media information in a track					ISO
mdri	Mutable DRM information											OMA DRM 2.0
meco	additional metadata container									ISO
mehd	movie extends header box										ISO
mere	metabox relation												ISO
meta	Metadata container												ISO
mfhd	movie fragment header											ISO
mfra	Movie fragment random access									ISO
mfro	Movie fragment random access offset								ISO
minf	media information container										ISO
mjhd	reserved for MPEG-J Stream header								MP4v1
moof	movie fragment													ISO
moov	container for all the meta-data									ISO
mvcg	Multiview group													NALu Video
mvci	Multiview Information											NALu Video
mvex	movie extends box												ISO
mvhd	movie header, overall declarations								ISO
mvra	Multiview Relation Attribute									NALu Video
nmhd	Null media header, overall information (some tracks only)		ISO
ochd	reserved for ObjectContentInfoStream header						MP4v1
odaf	OMA DRM Access Unit Format										OMA DRM 2.0
odda	OMA DRM Content Object											OMA DRM 2.0
odhd	reserved for ObjectDescriptorStream header						MP4v1
odhe	OMA DRM Discrete Media Headers									OMA DRM 2.0
odrb	OMA DRM Rights Object											OMA DRM 2.0
odrm	OMA DRM Container												OMA DRM 2.0
odtt	OMA DRM Transaction Tracking									OMA DRM 2.0
ohdr	OMA DRM Common headers											OMA DRM 2.0
padb	sample padding bits												ISO
paen	Partition Entry													ISO
pclr	palette															JPEG2000
pdin	Progressive download information								ISO
pitm	primary item reference											ISO
pnot	Reserved														ISO
prft	Producer reference time											ISO
pssh	Protection system specific header								ISO Common Encryption
res 	grid resolution													JPEG2000
resc	grid resolution at which the image was captured					JPEG2000
resd	default grid resolution at which the image should be displayed	JPEG2000
rinf	restricted scheme information box								ISO
saio	Sample auxiliary information offsets							ISO
saiz	Sample auxiliary information sizes								ISO
sbgp	Sample to Group box												ISO
schi	scheme information box											ISO
schm	scheme type box													ISO
sdep	Sample dependency												NALu Video
sdhd	reserved for SceneDescriptionStream header						MP4v1
sdtp	Independent and Disposable Samples Box							ISO
sdvp	SD Profile Box													SDV
segr	file delivery session group										ISO
senc	Sample specific encryption data									ISO Common Encryption
sgpd	Sample group definition box										ISO
sidx	Segment Index Box												3GPP
sinf	protection scheme information box								ISO
skip	free space														ISO
smhd	sound media header, overall information (sound track only)		ISO
srmb	System Renewability Message										DVB
srmc	System Renewability Message container							DVB
srpp	STRP Process													ISO
ssix	Sub-sample index												ISO
stbl	sample table box, container for the time/space map				ISO
stco	chunk offset, partial data-offset information					ISO
stdp	sample degradation priority										ISO
sthd	Subtitle Media Header Box										ISO
strd	Sub-track definition											ISO
stri	Sub-track information											ISO
stsc	sample-to-chunk, partial data-offset information				ISO
stsd	sample descriptions (codec types, initialization etc.)			ISO
stsg	Sub-track sample grouping										ISO
stsh	shadow sync sample table										ISO
stss	sync sample table (random access points)						ISO
stsz	sample sizes (framing)											ISO
stts	(decoding) time-to-sample										ISO
styp	Segment Type Box												3GPP
stz2	compact sample sizes (framing)									ISO
subs	Sub-sample information											ISO
swtc	Multiview Group Relation										NALu Video
tfad	Track fragment adjustment box									3GPP
tfdt	Track fragment decode time										ISO
tfhd	Track fragment header											ISO
tfma	Track fragment media adjustment box								3GPP
tfra	Track fragment radom access										ISO
tibr	Tier Bit rate													NALu Video
tiri	Tier Information												NALu Video
tkhd	Track header, overall information about the track				ISO
traf	Track fragment													ISO
trak	container for an individual track or stream						ISO
tref	track reference container										ISO
trex	track extends defaults											ISO
trgr	Track grouping information										ISO
trik	Facilitates random access and trick play modes					DECE
trun	track fragment run												ISO
udta	user-data														ISO
uinf	UUID info														JPEG2000
UITS	Unique Identifier Technology Solution							Universal Music Group
ulst	a list of UUID’s												JPEG2000
url 	a URL															JPEG2000
uuid	user-extension box												ISO
vmhd	video media header, overall information (video track only)		ISO
vwdi	Multiview Scene Information										NALu Video
xml 	XML container													ISO
xml 	XML info														JPEG2000

User-data Codes

albm	Album title and track number for media							3GPP
alou	Album loudness base												ISO
angl	Name of the camera angle through which the clip was shot		Apple
auth	Author of the media												3GPP
clfn	Name of the clip file											Apple
clid	Identifier of the clip											Apple
clsf	Classification of the media										3GPP
cmid	Identifier of the camera										Apple
cmnm	Name that identifies the camera									Apple
coll	Name of the collection from which the media comes				3GPP
cprt	copyright etc.													ISO
date	ISO8601 Date and time content creation							Apple
dscp	Media description												3GPP
gnre	Media genre														3GPP
hinf	hint information												ISO
hnti	Hint information												ISO
hpix	Hipix Rich Picture (user-data or meta-data)						Hipix
kywd	Media keywords													3GPP
loci	Media location information										3GPP
ludt	Track loudness container										ISO
manu	Manufacturer name of the camera									Apple
modl	Model name of the camera										Apple
orie	Orientation information											3GPP
perf	Media performer name											3GPP
reel	Name of the tape reel											Apple
rtng	Media rating													3GPP
scen	Name of the scene for which the clip was shot					Apple
shot	Name that identifies the shot									Apple
slno	Serial number of the camera										Apple
strk	Sub track information											ISO
thmb	Thumbnail image of the media									3GPP
titl	Media title														3GPP
tlou	Track loudness base												ISO
tsel	Track selection													ISO
tsel	Track selection													3GPP
urat	User 'star' rating of the media									3GPP
yrrc	Year when media was recorded									3GPP

albm	Album title and track number						3GPP
auth	Media author name									3GPP
clsf	Media classification								3GPP
cprt	copyright etc										ISO
dcfD	Marlin DCF Duration, user-data atom type						OMArlin

QuickTime Codes

clip	Visual clipping region container								QT
crgn	Visual clipping region definition								QT
ctab	Track color-table												QT
elng	Extended Language Tag											QT
imap	Track input map definition										QT
kmat	Compressed visual track matte									QT
load	Track pre-load definitions										QT
matt	Visual track matte for compositing								QT
pnot	Preview container												QT
wide	Expansion space reservation										QT

*/
//-----------------------------------------------------------------------------
// base types
//-----------------------------------------------------------------------------

typedef endian_t<fixed<16, 16>, true>	fixed32be;
typedef endian_t<fixed<8, 8>, true>		fixed16be;

struct qttime {
	uint64	v;	// seconds since midnight, January 1, 1904
	void operator=(uint64 _v) {v = _v; }
	operator DateTime() const { return DateTime(1904, 1, 1) + Duration::Secs(v); }
};

struct id4 {
	uint32	id;

	id4(uint32 u = 0) : id(u) {}

	operator fixed_string<9>() const {
		fixed_string<9> t;
		if (string_check((const char*)&id, 4, char_set::alphanum + ' ' + '\xa9')) {
			(uint32&)t = id;
			t[4] = 0;
		} else {
			t[to_string(t.begin(), hex(force_cast<uint32be>(id)))] = 0;
		}
		return t;
	}
	operator tag() const {
		return operator fixed_string<9>();
	}
	operator uint32() const { return id; }
	bool	valid() {
		return between((id >>  0) & 0xff, 0x20, 0x7f)
			&& between((id >>  8) & 0xff, 0x20, 0x7f)
			&& between((id >> 16) & 0xff, 0x20, 0x7f)
			&& between((id >> 24) & 0xff, 0x20, 0x7f);
	}
};

struct atom {
	enum {
		SIZE_TOEND	= 0,
		SIZE_64BIT	= 1,
	};
	uint64		size;
	id4			id;

	bool	read(istream_ref file) {
		uint32be	size0;
		if (file.read(size0, id)) {
			size	= size0 == SIZE_64BIT	? file.get<uint64be>() - 16
					: size0 > 8				? size0 - 8
					: 0;
			return true;
		}
		return false;
	}
};

struct version_flags {
	uint32		version:8, flags:24;
	bool	read(istream_ref file) { return check_readbuff(file, this, sizeof(*this)); }
};


struct version_flags_time : version_flags {
	qttime	creation_time;
	qttime	modification_time;
	bool	read(istream_ref file) {
		return version_flags::read(file) && (version == 0
			? file.read(read_as<uint32be>(creation_time), read_as<uint32be>(modification_time))
			: file.read(read_as<uint64be>(creation_time), read_as<uint64be>(modification_time))
			);
	}
	template<typename T> bool read(T *p, istream_ref file) {
		return read(file) && check_readbuff(file, this + 1, sizeof(T) - sizeof(*this));
	}
};

template<typename T> struct qt_table : version_flags {//, trailing_array2<qt_table<T>, T> {
	uint32be	count;			//count of data references that follow.
	T			_entries[];
	const	T*			begin()		const	{ return _entries; }
	const	T*			end()		const	{ return _entries + count; }
	range<const T*>		entries()	const	{ return make_range_n(_entries, count); }
	auto&		operator[](int i)	const	{ return _entries[i]; }
};

template<> struct qt_table<void> : version_flags {
	uint32be	count;			//count of data references that follow.
	auto		entries()	const	{ return 0; }

};

//-----------------------------------------------------------------------------
// atom types
//-----------------------------------------------------------------------------

#if 1
typedef void	ipmc, imif, schm, schi, pdin, qtfree, qtskip, mvex;
#else
struct ipmc	{//IPMP Control Box
};
struct imif	{//IPMP Information box
};
struct schm	{//scheme type box
};
struct schi {//	scheme information box
};
struct pdin	{//Progressive download information
};
struct mvex	{//movie extends box
};
#endif

typedef version_flags qtmeta, nmhd, iref;

struct mhdr : version_flags {
	uint32be	next_item_id;
};

struct keys : version_flags {
	struct key {
		uint32be	size;
		id4			namespce;
		char		_name[];
		auto	name()	const { return str(_name, size - 8); }
		auto	next()	const { return (const key*)((const uint8*)this + size); }
	};
	uint32be	count;
	key			key0;
	auto		entries() const { return make_range_n(make_next_iterator(&key0), count); }
};

struct frma {
	id4 format;
};

struct ftyp {
	id4			major_brand;
	uint32be	minor_version;
	id4			compatible_brands[];
};

struct reference {
	uint16be	from_item_id;
	uint16be	num_refs;
	uint16be	to_item_ids[];
	auto	to() const { return make_range_n(to_item_ids, num_refs); }
};

typedef reference	auxl, cdsc, dimg, thmb;

struct ipma : version_flags {
	struct Association {
		uint16 index:15, essential:1;
	};
	struct Entry {
		uint32	item_id;
		dynamic_array<Association> associations;
	};

	dynamic_array<Entry>	entries;

	bool	read(istream_ref file) {
		version_flags::read(file);
		entries.resize(file.get<uint32be>());
		for (auto &i : entries) {
			i.item_id = version < 1 ? file.get<uint16be>() : file.get<uint32be>();
			i.associations.resize(file.get<uint8>());
			if (flags & 1) {
				for (auto &j : i.associations) {
					uint16	x = file.get<uint16be>();
					j.index		= x & 0x7fff;
					j.essential	= x >> 15;
				}
			} else {
				for (auto &j : i.associations) {
					auto	x = file.get<uint8>();
					j.index		= x & 0x7f;
					j.essential	= x >> 7;
				}
			}
		}
		return true;
	}

	range<const Association*> get_associations(uint32 id) const {
		for (auto &i : entries) {
			if (i.item_id == id)
				return i.associations;
		}
		return none;
	}
};

struct mvhd : version_flags_time {
	uint32be	time_scale;			//time scale for this movie—that is, the number of time units that pass per second in its time coordinate system. A time coordinate system that measures time in sixtieths of a second, for example, has a time scale of 60.
	uint32be	duration;			//duration of the movie in time scale units
	fixed32be	preferred_rate;		//fixed-point number that specifies the rate at which to play this movie. A value of 1.0 indicates normal rate.
	fixed16be	preferred_volume;	//fixed-point number that specifies how loud to play this movie’s sound. A value of 1.0 indicates full volume.
	uint8		reserved[10];		//set to 0.
	fixed32be	matrix[9];			//how to map points from one coordinate space into another
	uint32be	preview_time;		//time value in the movie at which the preview begins.
	uint32be	preview_duration;	//duration of the movie preview in movie time scale units.
	uint32be	poster_time;		//time value of the time of the movie poster.
	uint32be	selection_time;		//time value for the start time of the current selection.
	uint32be	selection_duration;	//duration of the current selection in movie time scale units.
	uint32be	current_time;		//time value for current time position within the movie.
	uint32be	next_track;			//value to use for the track ID number of the next track added to this movie. Note that 0 is not a valid track ID value.
	bool	read(istream_ref file) { return version_flags_time::read(this, file); }
};

struct tkhd : version_flags_time {
	enum {
		enabled		= 1,	//track is enabled
		in_movie	= 2,	//track is used in the movie
		in_preview	= 4,	//track is used in the movie’s preview
		in_poster	= 8,	//track is used in the movie’s poster
	};
	uint32be	id;					//uniquely identifies the track. The value 0 cannot be used.
	uint32be	reserved;			//Set this field to 0.
	uint32be	duration;			//duration of the this track in movies time scale units
	packed<uint64be>	reserved2;			//Set this field to 0.
	uint16be	layer;				//indicates this track’s spatial priority in its movie
	uint16be	alternate_group;	//identifies a collection of movie tracks that contain alternate data for one another (0 indicates that the track is not in an alternate track group)
	fixed16be	volume;				//fixed-point value that indicates how loudly this track’s sound is to be played. A value of 1.0 indicates normal volume.
	uint16be	reserved3;			//A 16-bit integer that is reserved for use by Apple. Set this field to 0.
	fixed32be	matrix[9];			//how to map points from one coordinate space into another
	fixed32be	width;				//fixed point width of this track in pixels.
	fixed32be	height;				//fixed point height of this track in pixels.
	bool	read(istream_ref file) { return version_flags_time::read(this, file); }
};

struct mdhd : version_flags_time {
	uint32be	time_scale;			//time scale for this media—that is, the number of time units that pass per second in its time coordinate system. A time coordinate system that measures time in sixtieths of a second, for example, has a time scale of 60.
	uint32be	duration;			//duration of the this track in time scale units
	uint16be	language;			//language code for this media. See “Language Code Values” for valid language codes. Also see “Extended Language Tag Atom” for the preferred code to use here if an extended language tag is also included in the media atom.
	uint16be	quality;			//media’s playback quality—that is, its suitability for playback in a given environment.
	bool	read(istream_ref file) { return version_flags_time::read(this, file); }
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

typedef qt_table<void>	dref;
typedef qt_table<id4>	tref;

//sample descriptor
struct sample_desc {
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
	fixed32be	horizontal_resolution;	//fixed-point number containing the horizontal resolution of the image in pixels per inch.
	fixed32be	verical_resolution;		//fixed-point number containing the vertical resolution of the image in pixels per inch.
	uint32be	data_size;				//set to 0.
	uint16be	frame_count;			//how many frames of compressed data are stored in each sample. Usually set to 1.
	char		compressor_name[32];	//A 32-byte Pascal string containing the name of the compressor that created the image, such as "jpeg".
	uint16be	depth;					//pixel depth of the compressed image. Values of 1, 2, 4, 8 ,16, 24, and 32 indicate the depth of color images. The value 32 should be used only if the image contains an alpha channel. Values of 34, 36, and 40 indicate 2-, 4-, and 8-bit grayscale, respectively, for grayscale images.
	uint16be	colortable_id;			//which color table to use. If this field is set to –1, the default color table should be used for the specified depth. For all depths below 16 bits per pixel, this indicates a standard Macintosh color table for the specified depth. Depths of 16, 24, and 32 have no color table.
};

struct audio_sample_desc : sample_desc {
	uint32be	reserved[2];
	uint16be	channelcount;	// = 2;
	uint16be	samplesize;		// = 16;
	uint16be	pre_defined;	// = 0;
	uint16be	reserved2;		// = 0;
	uint32be	samplerate;		// = { timescale of media } << 16;
};


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

struct mehd : version_flags {
	uint32be	fragment_duration;
	bool read(istream_ref file) {
		version_flags::read(file);
		fragment_duration = version == 0 ? file.get<uint32be>() : file.get<uint64be>();
		return true;
	}
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
		data_offset_present						= 0x000001,
		first_sample_flags_present				= 0x000004,
		sample_duration_present					= 0x000100,
		sample_size_present						= 0x000200,
		sample_flags_present					= 0x000400,
		sample_composition_time_offsets_present	= 0x000800,
	};
	struct sample {
		// all fields in the following array are optional 
		uint32be duration;
		uint32be size;
		uint32be flags;
		uint32be composition_time_offset;
	};
	uint32be	count;
	// the following are optional fields 
	uint32be	data_offset;
	uint32be	first_sample_flags;
	sample		e[];
	auto	entries() const { return make_range_n(e, count); }
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

struct xml : version_flags {
	embedded_string	xml;
};

struct bxml : version_flags {
	uint8	data[1];
};

struct pitm : version_flags {
	uint16be	item_id;
};

struct ccst : version_flags {
	uint32	AllReferencePicturesIntra : 1, IntraPredUsed : 1, MaxRefPerPic : 4, : 26;
};

struct qt_size  : version_flags {
	fixed32be	width, height;
};

typedef qt_size clef, prof, enof;

struct iloc : version_flags {
	uint8 offset_size, length_size, base_offset_size, index_size;

	struct entry {
		uint32		item_id;
		uint16		construction_method;	//v1+ only
		uint16		data_reference_index;
		uint64		base_offset;
		struct extent {
			uint64	index	= 0;
			uint64	offset	= 0;
			uint64	length	= 0;
		};
		dynamic_array<extent>	extents;
		bool			read_data(istream_ref src, const_memory_block, ostream_ref dest) const;
	};
	dynamic_array<entry>	entries;

	static uint64	read_num(istream_ref file, uint32 size) {
		return size == 4 ? (uint64)file.get<uint32be>() : size == 8 ? (uint64)file.get<uint64be>() : 0;
	}

	bool	read(istream_ref file) {
		version_flags::read(file);
		uint16	head = file.get<uint16be>();
		offset_size		= (head >> 12) & 15;
		length_size		= (head >> 8) & 15;
		base_offset_size= (head >> 4) & 15;
		index_size		= version > 1 ? (head >>  0) & 15 : 0;

		entries.resize(version < 2 ? file.get<uint16be>() : file.get<uint32be>());
		for (auto &i : entries) {
			i.item_id				= version < 2 ? file.get<uint16be>() : file.get<uint32be>();
			i.construction_method	= version < 1 ? 0 : (uint16)file.get<uint16be>();
			i.data_reference_index	= file.get<uint16be>();
			i.base_offset			= read_num(file, base_offset_size);
			i.extents.resize(file.get<uint16be>());
			for (auto &e : i.extents) {
				e.index		= read_num(file, index_size);
				e.offset	= read_num(file, offset_size);
				e.length	= read_num(file, length_size);
			}
		}
		return true;
	}

	const entry*	find(uint32 item_id) const {
		for (auto &i : entries) {
			if (i.item_id == item_id)
				return &i;
		}
		return nullptr;
	}

};

bool iloc::entry::read_data(istream_ref src, const_memory_block idat, ostream_ref dest) const {
	for (auto& e : extents) {
		switch (construction_method) {
			case 0:
				src.seek(e.offset + base_offset);
				stream_copy(dest, src, e.length);
				return true;
			case 1:
				return dest.write(idat.slice(e.offset + base_offset, e.length));

			default:
				return false;
		}
	}
	return true;
}


struct infe : version_flags {
	enum FLAGS {
		HIDDEN	= 1 << 0,
	};
	uint32		item_id;
	uint16		item_protection_index;
	id4			item_type;
	string		item_name;
	string		content_type;
	string		content_encoding;

	bool	read(istream_ref file) {
		version_flags::read(file);
		item_id					= version < 3 ? file.get<uint16be>() : file.get<uint32be>();
		item_protection_index	= file.get<uint16be>();
		item_type				= version > 1 ? file.get<uint32>() : 0;
		item_name.read(file);
		content_type.read(file);
		if (item_type == "mime"_u32)
			content_encoding.read(file);
		return true;
	}

	bool	is_image() const {
		return is_any(item_type, "hvc1"_u32, "grid"_u32, "iden"_u32, "iovl"_u32, "av01"_u32, "vvc1"_u32);
	}
	bool	is_hidden() const { return flags & HIDDEN; }
};

struct iinf : version_flags {
	uint32	count;
	bool	read(istream_ref file) {
		version_flags::read(file);
		count = version < 2 ? file.get<uint16be>() : file.get<uint32be>();
		return true;
	}
};

struct AV1Configuration {
	union {
		bitfield<uint8, 0, 7>	version;
		bitfield<uint8, 7, 1>	marker;
	};
	union {
		bitfield<uint8, 0, 5>	seq_level_idx_0;
		bitfield<uint8, 5, 3>	seq_profile;
	};
	union {
		bitfield<uint8, 0, 2>	chroma_sample_position;
		bitfield<uint8, 2, 1>	chroma_subsampling_y;
		bitfield<uint8, 3, 1>	chroma_subsampling_x;
		bitfield<uint8, 4, 1>	monochrome;
		bitfield<uint8, 5, 1>	twelve_bit;
		bitfield<uint8, 6, 1>	high_bitdepth;
		bitfield<uint8, 7, 1>	seq_tier_0;
	};
	union {
		bitfield<uint8, 0, 4>	initial_presentation_delay_minus_one;
		bitfield<uint8, 4, 4>	initial_presentation_delay_present;
	};
	uint8	obu[];
};

struct dvcc {
	uint8	version_major, version_minor;
	union {
		bitfield<uint16be,0,7>	profile;
		bitfield<uint16be,7,6>	level;
		bitfield<uint16be,13,1>	rpu_present_flag;
		bitfield<uint16be,14,1>	el_present_flag;
		bitfield<uint16be,15,1>	bl_present_flag;
	};
	uint32be	dv_bl_signal_compatibility_id;//:4, reserved:28;
	uint32be	reserved2;
};

struct esds : version_flags {
	static uint32 get_len(const uint8 *&p) {
		uint32	r = 0;
		while (*p & 0x80)
			r = (r << 7) + (*p++ & 0x7f);
		return r | *p++;
	};
	struct section {
		uint8		type;
		uint32		len;
		const uint8	*p;
		section(const uint8	*p) : type(*p++), len(get_len(p)), p(p) {}
	};
	struct section_iterator {
		const uint8	*p;
		section_iterator(const uint8 *p) : p(p) {}
		auto&	operator++() {
			++p;
			uint32	len	= get_len(p);
			p += len;
			return *this;
		}
		bool	operator!=(const section_iterator& b) const { return p != b.p; }
		section	operator*() const { return p; }
	};
	
	struct section3 {
		uint16be	es_id;
		uint8		priority;	//0->31, default 16
		uint8		_sections[];
		auto		sections(uint32 len) const { return make_range(section_iterator(_sections), section_iterator((const uint8*)this + len)); }
	};

	struct section4 {
		enum TYPE {
			V1								= 1,
			V2								= 2,
			MPEG4_VIDEO						= 32,
			MPEG4_AVC_SPS					= 33,
			MPEG4_AVC_PPS					= 34,
			MPEG4_AUDIO						= 64,
			MPEG2_SIMPLE_VIDEO				= 96,
			MPEG2_MAIN_VIDEO				= 97,
			MPEG2_SNR_VIDEO					= 98,
			MPEG2_SPATIAL_VIDEO				= 99,
			MPEG2_HIGH_VIDEO				= 100,
			MPEG2_422_VIDEO					= 101,
			MPEG4_ADTS_MAIN					= 102,
			MPEG4_ADTS_LOW_COMPLEXITY		= 103,
			MPEG4_ADTS_SCALEABLE_SAMPLING	= 104,
			MPEG2_ADTS_MAIN					= 105,
			MPEG1_VIDEO						= 106,
			MPEG1_ADTS						= 107,
			JPEG_VIDEO						= 108,
			PRIVATE_AUDIO					= 192,
			PRIVATE_VIDEO					= 208,
			PCM_LITTLE_ENDIAN_AUDIO			= 224,
			VORBIS_AUDIO					= 225,
			DOLBY_V3_AUDIO					= 226,
			ALAW_AUDIO						= 227,
			MULAW_AUDIO						= 228,
			ADPCM_AUDIO						= 229,
			PCM_BIG_ENDIAN_AUDIO			= 230,
			YV12_VIDEO						= 240,
			H264_VIDEO						= 241,
			H263_VIDEO						= 242,
			H261_VIDEO						= 243
		};
		enum STREAM {
			object_desc	= 1,
			clock_ref	= 2,
			scene_desc	= 4,
			visual		= 4,
			audio		= 5,
			MPEG7		= 6,
			IPMP		= 7,
			OCI			= 8,
			MPEGJava	= 9,
			user		= 32,
		};
		uint8		type;
		union {
			bitfield<packed<uint32be>,0,6>	stream;
			bitfield<packed<uint32be>,6,1>	upstream;
			bitfield<packed<uint32be>,7,1>	reserved;
			bitfield<packed<uint32be>,8,24>	buffer_size;
		};
		packed<uint32be>	maximum_bit_rate;
		packed<uint32be>	average_bit_rate;
	};

	struct section5 {
		enum AudioProfile {
			MAIN			= 1, 
			LOW_COMPLEXITY	= 2, 
			SCALEABLE		= 3, 
			T_F				= 4, 
			T_F_MAIN		= 5, 
			T_F_LC			= 6, 
			TWIN_VQ			= 7, 
			CELP			= 8, 
			HVXC			= 9, 
			HILN			= 10,
			TTSI			= 11,
			MAIN_SYNTHESIS	= 12,
			WAVETABLE		= 13,
		};
		uint8		audio_profile;
		union {
			bitfield<uint8,0,5>		profile_id;
			//bitfield<uint8,5,3>	unknown;
		};
		uint8		other_flags;
		union {
			//bitfield<uint8,0,3>	unknown;
			bitfield<uint8,3,2>		num_channels;
			//bitfield<uint8,5,3>	unknown;
		};
	};
	struct section6 {
		uint8	sl;	//=2
	};

	uint8	_sections[];
	auto	sections() const { return section(_sections); }
};

struct pixi : version_flags {
	uint8	num_channels;
	uint8	_bits_per_channel[];
	auto	bits_per_channel() const { return make_range_n(_bits_per_channel, num_channels); }
};

struct auxc : version_flags {
	embedded_string	type;
	//followed by 8 bit subtypes
};
struct ispe : version_flags {
	uint32be	width, height;
};

struct imir {
	//bit0 - axis (0=v, 1=h)
	uint8	flags;
};

struct irot {
	//bottom 2 bits are rotation in 90 degree increments
	uint8	flags;
};

//-----------------------------------------------------------------------------
// colour
//-----------------------------------------------------------------------------

//If the equation for normalized Y' has the form:
//	EY’ = KG’EG’ + KB’EB’ + KR’ER’
//Then the formulas for normalized Cb and Cr are:
//	ECb = (0.5 / (1 - KB’)) * (EB’ - EY’)
//	ECr = (0.5 / (1 - KR’)) * (ER’ - EY’)
//
enum class ColourMatrix : uint16 {
	RGB_GBR										= 0,
	ITU_R_BT_709_5								= 1,	// EY’ = 0.7152 EG’ + 0.0722 EB’+ 0.2126 ER’
	unspecified									= 2,
	US_FCC_T47									= 4,
	ITU_R_BT_470_6_System_B_G					= 5,
	ITU_R_BT_601_6								= 6,	// EY’ = 0.5870 EG’ + 0.1140 EB’+ 0.2990 ER’
	SMPTE_240M									= 7,	// EY’ = 0.7010 EG’ + 0.0870 EB’+ 0.2120 ER’
	YCgCo										= 8,
	ITU_R_BT_2020_2_non_constant_luminance		= 9,	// EY’ = 0.6780 EG’ + 0.0593 EB’+ 0.2627 ER’
	ITU_R_BT_2020_2_constant_luminance			= 10,
	SMPTE_ST_2085								= 11,
	chromaticity_derived_non_constant_luminance	= 12,
	chromaticity_derived_constant_luminance		= 13,
	ICtCp										= 14
};

enum class ColourPrimaries : uint16 {
	ITU_R_BT_709_5				= 1,	//red={0.640,0.330} green={0.300,0.600} blue={0.150,0.060}
	unspecified					= 2,
	ITU_R_BT_470_6_System_M		= 4,
	ITU_R_BT_470_6_System_B_G	= 5,	//red={0.640,0.330} green={0.290,0.600} blue={0.150,0.060} white={0.3127,0.3290} (D65)	
	ITU_R_BT_601_6				= 6,	//red={0.630,0.340} green={0.310,0.595} blue={0.155,0.070} white={0.3127,0.3290} (D65)	
	SMPTE_240M					= 7,
	generic_film				= 8,
	ITU_R_BT_2020_2_and_2100_0	= 9,	//red={0.708,0.292} green={0.170,0.797} blue={0.131,0.046} white={0.3127,0.3290} (D65)	
	SMPTE_ST_428_1				= 10,
	SMPTE_RP_431_2				= 11,	//red={0.680,0.320} green={0.265,0.690} blue={0.150,0.060} white={0.3140,0.3510}		aka DCI P3
	SMPTE_EG_432_1				= 12,	//red={0.680,0.320} green={0.265,0.690} blue={0.150,0.060} white={0.3127,0.3290} (D65)	aka P3 D65 or Display P3
	EBU_Tech_3213_E				= 22
};

enum class ColourTransfer : uint16 {
	ITU_R_BT_709_5				= 1,
	unspecified					= 2,
	ITU_R_BT_470_6_System_M		= 4,
	ITU_R_BT_470_6_System_B_G	= 5,
	ITU_R_BT_601_6				= 6,
	SMPTE_240M					= 7,
	linear						= 8,
	logarithmic_100				= 9,
	logarithmic_100_sqrt10		= 10,
	IEC_61966_2_4				= 11,
	ITU_R_BT_1361				= 12,
	IEC_61966_2_1				= 13,
	ITU_R_BT_2020_2_10bit		= 14,
	ITU_R_BT_2020_2_12bit		= 15,
	ITU_R_BT_2100_0_PQ			= 16,
	SMPTE_ST_428_1				= 17,
	ITU_R_BT_2100_0_HLG			= 18
};


struct nclc {
	endian_t<ColourPrimaries, true>	primaries;
	endian_t<ColourTransfer, true>	transfer;
	endian_t<ColourMatrix, true>	matrix;
};

struct nc1x : nclc {
	uint8		flags;//full range in 0x80
};

struct colr {
	id4		type;
	union {
		nc1x	nc1x;
		// 'rICC'	ICC_profile;    // restricted ICC profile
		// 'prof'	ICC_profile;    // unrestricted ICC profile
	};
};


float transfer(ColourTransfer t, float w) {
	switch (t) {
		case ColourTransfer::ITU_R_BT_709_5:
			return w < 0.018 ? w * 4.5f : 1.099f * pow(w, 0.45f) - 0.099f;

		//case ColourTransfer::unspecified:
		//case ColourTransfer::ITU_R_BT_470_6_System_M:
		//case ColourTransfer::ITU_R_BT_470_6_System_B_G:
		//case ColourTransfer::ITU_R_BT_601_6:
		case ColourTransfer::SMPTE_240M:
			return w < 0.0228 ? w * 4 : 1.1115f * pow(w, 0.45f) - 0.115;

		case ColourTransfer::linear:
			return w;

		//case ColourTransfer::logarithmic_100:
		//case ColourTransfer::logarithmic_100_sqrt10:
		//case ColourTransfer::IEC_61966_2_4:
		//case ColourTransfer::ITU_R_BT_1361:
		//case ColourTransfer::IEC_61966_2_1:
		//case ColourTransfer::ITU_R_BT_2020_2_10bit:
		//case ColourTransfer::ITU_R_BT_2020_2_12bit:
		//case ColourTransfer::ITU_R_BT_2100_0_PQ:

		case ColourTransfer::SMPTE_ST_428_1:
			return pow(w * 48 / 52.37f, 1 / 2.6f);

		//case ColourTransfer::ITU_R_BT_2100_0_HLG:
		default:
			return w;
	}
};

colour_primaries get_colour_primaries(ColourPrimaries primaries) {
	switch (primaries) {	//		rX,			gX,			bX,			wX,			rY,			gY,			bY,			wY
		case ColourPrimaries::ITU_R_BT_709_5:				return {{0.640f,	0.300f,		0.150f,		0.3127f},	{0.330f,	0.600f,		0.060f,		0.3290f}};
		case ColourPrimaries::ITU_R_BT_470_6_System_M:		return {{0.67f,		0.21f,		0.14f,		0.310f},	{0.33f,		0.71f,		0.08f,		0.316f}};
		case ColourPrimaries::ITU_R_BT_470_6_System_B_G:	return {{0.64f,		0.29f,		0.15f,		0.3127f},	{0.33f,		0.60f,		0.06f,		0.3290f}};
		case ColourPrimaries::SMPTE_240M:					return {{0.630f,	0.310f,		0.155f,		0.3127f},	{0.340f,	0.595f,		0.070f,		0.3290f}};
		case ColourPrimaries::generic_film:					return {{0.681f,	0.243f,		0.145f,		0.310f},	{0.319f,	0.692f,		0.049f,		0.316f}};
		case ColourPrimaries::ITU_R_BT_2020_2_and_2100_0:	return {{0.708f,	0.170f,		0.131f,		0.3127f},	{0.292f,	0.797f,		0.046f,		0.3290f}};
		case ColourPrimaries::SMPTE_ST_428_1:				return {{1.0f,		0.0f,		0.0f,		0.333333f},	{0.0f,		1.0f,		0.0f,		0.33333f}};
		case ColourPrimaries::SMPTE_RP_431_2:				return {{0.680f,	0.265f,		0.150f,		0.314f},	{0.320f,	0.690f,		0.060f,		0.351f}};
		case ColourPrimaries::SMPTE_EG_432_1:				return {{0.680f,	0.265f,		0.150f,		0.3127f},	{0.320f,	0.690f,		0.060f,		0.3290f}};
		case ColourPrimaries::EBU_Tech_3213_E:				return {{0.630f,	0.295f,		0.155f,		0.3127f},	{0.340f,	0.605f,		0.077f,		0.3290f}};
		default:											return {zero, zero};
	}
}

Kr_Kb	get_Kr_Kb(ColourMatrix matrix, ColourPrimaries primaries) {
	switch (matrix) {
		case ColourMatrix::ITU_R_BT_709_5:							return {0.2126f,	0.0722f};
		case ColourMatrix::US_FCC_T47:								return {0.30f, 		0.11f};
		case ColourMatrix::ITU_R_BT_470_6_System_B_G:
		case ColourMatrix::ITU_R_BT_601_6:							return {0.299f,		0.114f};
		case ColourMatrix::SMPTE_240M:								return {0.212f,		0.087f};
		case ColourMatrix::ITU_R_BT_2020_2_non_constant_luminance:
		case ColourMatrix::ITU_R_BT_2020_2_constant_luminance:		return {0.2627f,	0.0593f};
		case ColourMatrix::chromaticity_derived_non_constant_luminance:
		case ColourMatrix::chromaticity_derived_constant_luminance:	return get_colour_primaries(primaries);
		default:													return {0, 0};
	}
}

struct srgb_converter2 {
	srgb_converter		coeffs;
	ColourMatrix		matrix		= ColourMatrix::unspecified;
	bool				full_range	= true;

	srgb_converter2(const colr *colr) {
		if (colr) {
			switch (colr->type) {
				case "nc1x"_u32:
					full_range	= colr->nc1x.flags & 0x80;
					//fallthrough
				case "nclc"_u32:
					matrix		= colr->nc1x.matrix.get();
					coeffs		= get_Kr_Kb(matrix, colr->nc1x.primaries.get());
					break;

				case "prof"_u32:
				case "ricc"_u32: {
					auto	icc = (const ICC_Profile*)&colr->nc1x; 
					colour_XYZ	r, g, b, w;
					if (const ICC_Profile::XYZArray *p = icc->find("wtpt"_u32))
						w = p->values[0];
					if (const ICC_Profile::XYZArray *p = icc->find("rXYZ"_u32))
						r = p->values[0];
					if (const ICC_Profile::XYZArray *p = icc->find("gXYZ"_u32))
						g = p->values[0];
					if (const ICC_Profile::XYZArray *p = icc->find("bXYZ"_u32))
						b = p->values[0];
					float4	X	= {r.v.x, g.v.x, b.v.x, w.v.x};
					float4	Y	= {r.v.y, g.v.y, b.v.y, w.v.y};
					float4	Z	= {r.v.z, g.v.z, b.v.z, w.v.z};
					auto	d	= X + Y + Z;
					coeffs		= Kr_Kb(X / d, Y / d);
				}
			}
		}
	}

	template<typename T, typename D> void process_row(T *py, T *pcr, T *pcb, D *out, int width, int bpp, int shiftw) const {
		uint16	halfRange		= bits(bpp - 1);
		int32	fullRange		= bits(bpp);
		int		limited_offset	= 16 << (bpp - 8);

		switch (matrix) {
			case ColourMatrix::RGB_GBR:
				for (int x = 0; x < width; x++) {
					int		yv = py[x];
					int		cr = pcr[x >> shiftw];
					int		cb = pcb[x >> shiftw];
					if (full_range) {
						out[x] = {cr, yv, cb};
					} else {
						out[x] = {
							((cr * 219 + 128) >> 8) + limited_offset,
							((yv * 219 + 128) >> 8) + limited_offset,
							((cb * 219 + 128) >> 8) + limited_offset
						};
					}
				}
				break;

			case ColourMatrix::YCgCo:
				for (int x = 0; x < width; x++) {
					int		yv = py[x];
					int		cr = pcr[x >> shiftw];
					int		cb = pcb[x >> shiftw];
					out[x] = {
						clamp(yv - cb + cr,					0, fullRange),
						clamp(yv + cb - halfRange,			0, fullRange),
						clamp(yv - cb - cr - halfRange * 2,	0, fullRange)
					};
				}
				break;

			// TODO:
			case ColourMatrix::ITU_R_BT_2020_2_constant_luminance:
			case ColourMatrix::SMPTE_ST_2085:
			case ColourMatrix::chromaticity_derived_non_constant_luminance:
			case ColourMatrix::chromaticity_derived_constant_luminance:
			case ColourMatrix::ICtCp:

			default:
				for (int x = 0; x < width; x++) {
					float	yv = py[x];
					float	cr = pcr ? pcr[x >> shiftw] - halfRange : 0;
					float	cb = pcb ? pcb[x >> shiftw] - halfRange : 0;

					if (!full_range) {
						yv = (yv - limited_offset) * 256 / 219;
						cr = cr * 256 / 224;
						cb = cb * 256 / 224;
					}
					auto	rgb	= coeffs({yv, cr, cb});
					auto	v	= clamp(to<int>(rgb.t + 0.5f), zero, fullRange);
					out[x]	= {v.x, v.y, v.z};
				}
				break;
		}
	}
};

//-----------------------------------------------------------------------------
// samples
//-----------------------------------------------------------------------------

struct padb : version_flags {			// sample padding bits
	uint32be	count;
	range<bitfield_pointer<uint8, uint8, 4>>	entries() const { return make_range_n(bitfield_pointer<uint8, uint8, 4>(this + 1), count); }
};

struct time_to_sample {
	uint32be	sample_count;			//the number of consecutive samples that have the same duration.
	uint32be	sample_delta;			//duration of each sample.
};
typedef qt_table<time_to_sample>	stts;

struct sample_to_chunk {
	uint32be	first_chunk;			//first chunk number using this table entry.
	uint32be	samples_per_chunk;		//number of samples in each chunk.
	uint32be	sample_desc_id;			//identification number associated with the sample description for the sample
};
typedef qt_table<sample_to_chunk>	stsc;

struct stsz : version_flags {
	uint32be	sample_size;			//fixed sample size (if 0, use table)
	uint32be	count;
	uint32be	_entries[];
	auto	entries() const { return make_range_n(_entries, count); }
};

struct stz2 : version_flags {
	uint8		reserved[3];
	uint8		field_size;
	uint32be	count;
};

struct shadow_sync_sample {
	uint32be	shadowed_sample_number;
	uint32be	sync_sample_number;
};
typedef qt_table<shadow_sync_sample>	stsh;

struct composition_offset {
	uint32be	sample_count;			//the number of consecutive samples with the calculated composition offset in the field
	uint32be	sample_offset;			//the value of the calculated compositionOffset
};
typedef qt_table<composition_offset>	ctts;

struct sdtp : version_flags {			//sample dependency flags
	enum {
		// bit 0x80 is reserved; bit combinations 0x30, 0xC0 and 0x03 are reserved
		EarlierDisplayTimesAllowed				= 1 << 6,
		SampleDoesNotDependOnOthers				= 1 << 5,	// ie: an I picture
		SampleDependsOnOthers					= 1 << 4,	// ie: not an I picture
		NoOtherSampleDependsOnThisSample		= 1 << 3,
		OtherSamplesDependOnThisSample			= 1 << 2,
		ThereIsNoRedundantCodingInThisSample	= 1 << 1,
		ThereIsRedundantCodingInThisSample		= 1 << 0
	};
	uint8	entries[1];	// one per sample
};

struct sbgp : version_flags {
	id4			grouping_type;
	uint32be	count;
	struct entry {
		uint32be	sample_count;
		uint32be	group_description_index;
	}	e[];
	auto	entries() const { return make_range_n(e, count); }
};

struct sgpd : version_flags {
	id4			grouping_type;
	uint32be	count;
	uint8	e[][5];
	auto	entries() const { return make_range_n(e, count); }
};


typedef qt_table<xint32be>	stco;	//chunk offsets
typedef qt_table<xint64be>	co64;	//chunk offsets 64
typedef qt_table<uint32be>	stss;	//sync sample
typedef qt_table<uint16be>	stdp;

struct subs : version_flags {
	struct subsample {
		uint32		size;
		uint8		priority;
		uint8		discardable;
		uint32		reserved;// = 0;
		bool	read(istream_ref file, int version) {
			size = version == 0 ? file.get<uint16be>() : file.get<uint32be>();
			return file.read(priority, discardable, reserved);
		}
	};
	struct entry {
		uint32	sample_delta;
		dynamic_array<subsample>	subs;
		bool	read(istream_ref file, int version) {
			file.read(sample_delta);
			subs.resize(file.get<uint16be>());
			for (auto &i : subs)
				i.read(file, version);
			return true;
		}
	};
	dynamic_array<entry>	entries;

	bool	read(istream_ref file) {
		version_flags::read(file);
		entries.resize(file.get<uint32be>());
		for (auto &i : entries)
			i.read(file, version);
		return true;
	}
};

struct cslg	{};//composition to decode timeline mapping

struct QTSamples {
	struct chunk_block {
		uint32	first_chunk;			//first chunk number using this table entry
		uint32	samples_per_chunk;		//number of samples in each chunk
		uint32	end_sample;				//first sample of next block
		chunk_block(const sample_to_chunk &a) : first_chunk(a.first_chunk), samples_per_chunk(a.samples_per_chunk) {}
	};

	dynamic_array<chunk_block>	chunk_blocks;
	dynamic_array<streamptr>	chunk_offsets;
	dynamic_array<uint32>		sample_sizes;

	struct file_region {
		streamptr		offset;
		uint32			size;
		auto operator()(istream_ref file) const {
			file.seek(offset);
			return malloc_block(file, size);
		}
	};

	struct iterator {
		uint32				sample;		//sample index
		uint32				chunk_end;	//sample at which to move to next chunk
		streamptr			offset;		//file offset
		const chunk_block	*block;		//current block
		const streamptr		*chunk;		//current chunk's offset
		const uint32		*size;		//current samples's size

		iterator()	{}
		iterator(const QTSamples *samples, uint32 sample) : sample(sample) {
			block		= lower_boundc(samples->chunk_blocks, sample, [](const chunk_block &b, uint32 s) {return b.end_sample < s; });
			uint32	first_sample 	= block == samples->chunk_blocks.begin() ? 0 : block[-1].end_sample;
			uint32	offset_sample	= sample - first_sample;
			uint32	offset_chunk	= offset_sample / block->samples_per_chunk;

			chunk		= samples->chunk_offsets + (block->first_chunk + offset_chunk - 1);
			chunk_end	= first_sample + (offset_chunk + 1) * block->samples_per_chunk;

			size		= samples->sample_sizes + (chunk_end - block->samples_per_chunk);
			offset		= *chunk;
			for (uint32 n = offset_sample % block->samples_per_chunk; n--;)
				offset += *size++;
		}
		void		next() {
			offset	+= *size++;
			if (++sample >= chunk_end) {
				offset		= *++chunk;
				if (sample >= block->end_sample)
					++block;
				chunk_end	+= block->samples_per_chunk;
			}
		}
		void		move(int n);
		void		move_to(uint32 i)						{ move(i - sample); }

		file_region operator*()						const	{ return {offset, *size}; }
		bool		operator!=(const iterator &b)	const	{ return sample != b.sample; }
		auto&		operator++()							{ next(); return *this; }
		auto&		operator+=(int n)						{ move(n); return *this; }
	};

	QTSamples() {}
	QTSamples(const stsc *stsc, const stco *stco, const stsz *stsz) : chunk_blocks(stsc->entries()), chunk_offsets(stco->entries()), sample_sizes(stsz->entries()) {
		uint32	sample = 0;
		for (auto &i : chunk_blocks.slice_to(-1)) {
			sample	+= ((&i)[1].first_chunk - i.first_chunk) * i.samples_per_chunk;
			i.end_sample	= sample;
		}
		chunk_blocks.back().end_sample = sample_sizes.size();
	}
	iterator	at_index(uint32 i)	const	{ return {this, i}; }
	iterator	begin()				const	{ return {this, 0}; }
	iterator	end()				const	{ return {this, sample_sizes.size32()}; }
	auto		size()				const	{ return sample_sizes.size(); }
};

void QTSamples::iterator::move(int n) {
	if (n == 0)
		return;

	auto	sample1	= sample + n;

	if (sample1 >= block->end_sample) {
		//new block forward
		auto	nc	= (block->end_sample - chunk_end) / block->samples_per_chunk;
		++block;
		while (sample1 > block->end_sample) {
			nc += block[1].first_chunk - block[0].first_chunk;
			++block;
		}
		chunk		+= nc;
		chunk_end	= block[-1].end_sample;

	} else if (n < 0 && block->first_chunk > 1 && sample1 < block[-1].end_sample) {
		//new block backwards
		auto	nc	= (chunk_end - block[-1].end_sample) / block->samples_per_chunk;
		--block;
		while (sample1 < block[-1].end_sample) {
			nc += block[0].first_chunk - block[-1].first_chunk;
			--block;
		}
		chunk		-= nc;
		chunk_end	= block[-1].end_sample;
	}

	int	samples_per_chunk	= block->samples_per_chunk;

	if (sample1 >= chunk_end) {
		//new chunk forwards
		n			= sample1 - chunk_end;
		auto	nc	= n / samples_per_chunk + 1;
		chunk		+= nc;
		chunk_end	+= nc * samples_per_chunk;
		size		+= chunk_end - samples_per_chunk - sample;

		offset		= *chunk;
		n			%= samples_per_chunk;

	} else if (n < 0 && sample1 < chunk_end - samples_per_chunk) {
		//new chunk backwards
		n			= sample1 - chunk_end;
		auto	nc	= n / samples_per_chunk;
		chunk		+= nc;
		chunk_end	+= nc * samples_per_chunk;
		size		+= int(chunk_end - samples_per_chunk - sample);

		offset		= *chunk;
		n			%= samples_per_chunk;
		if (n)
			n		+= samples_per_chunk;
	}

	//same chunk;
	while (n < 0) {
		offset -= *--size;
		++n;
	}
	while (n--)
		offset	+= *size++;
	sample	= sample1;
}

struct QTSampleTimes {
	struct time_block {
		uint32	count;
		uint32	delta;
		time_block(const time_to_sample &a) : count(a.sample_count), delta(a.sample_delta) {}
	};
	dynamic_array<time_block>	time_blocks;

	QTSampleTimes() {}
	QTSampleTimes(const stts *stts) : time_blocks(stts->entries()) {}

	uint32		time_to_sample(uint64 t) const	{
		uint32	sample = 0;
		for (auto &i : time_blocks) {
			if (t < i.count * i.delta)
				return sample + t / i.delta;
			t		-= i.count * uint64(i.delta);
			sample	+= i.count;
		}
		return sample;
	}
};

struct QTSampleSync {
	dynamic_array<uint32>		sync_samples;

	uint32		sample_to_sync(uint32 i) const {
		if (sync_samples) {
			auto	p = lower_boundc(sync_samples, i);
			i	= *p;
		}
		return i;
	}
};


//-----------------------------------------------------------------------------
// atom hierarchy
//-----------------------------------------------------------------------------

struct AtomType {
	typedef	bool (*reader_t)(istream_ref, void*, size_t);
	id4				id;
	const ISO::Type	*type;
	reader_t		reader;
	const AtomType	*subtypes;

	AtomType(const _none&) : type(0), subtypes(0) {}
	AtomType(uint32 id, const ISO::Type *type, reader_t reader, const AtomType *subtypes = 0) : id(id), type(type), reader(reader), subtypes(subtypes) {}
	AtomType(uint32 id, const AtomType *subtypes) : id(id), type(0), subtypes(subtypes) {}

	template<typename T> static reader_t make_reader() {
		return [](istream_ref file, void *p, size_t) { return file.read(*(T*)p); };
	}
	template<> reader_t make_reader<void>() {
		return [](istream_ref file, void *p, size_t size) { return check_readbuff(file, p, size); };
	}

	template<typename T, uint32 ID, typename...S> static AtomType make(AtomType *subtypes) {
		return AtomType(ID, ISO::getdef<T>(), nullptr, subtypes);
	}
	template<typename T, uint32 ID, typename...S> static AtomType make(S&&... subtypes) {
		static const AtomType	atoms[] = { subtypes..., terminate };
		return AtomType(ID, ISO::getdef<T>(), nullptr, atoms);
	}
	template<typename T, uint32 ID> static AtomType make() {
		return AtomType(ID, ISO::getdef<T>(), nullptr, nullptr);
	}
	template<typename T, uint32 ID, typename...S> static AtomType maker(S&&... subtypes) {
		static const AtomType	atoms[] = { subtypes..., terminate };
		return AtomType(ID, ISO::getdef<T>(), make_reader<T>(), atoms);
	}
	template<typename T, uint32 ID> static AtomType maker() {
		return AtomType(ID, ISO::getdef<T>(), make_reader<T>(), nullptr);
	}
};


AtomType	meta_data = AtomType::make<qtmeta, "meta"_u32>(
	AtomType::make<mhdr, "mhdr"_u32>(),
	AtomType::make<hdlr, "hdlr"_u32>(),
	AtomType::make<keys, "keys"_u32>(),
	AtomType::make<void, "ilst"_u32>(
		AtomType::make<void, 0>()
	)
);

AtomType	meta_data0 = AtomType::make<void, "meta"_u32>(
	AtomType::make<mhdr, "mhdr"_u32>(),
	AtomType::make<hdlr, "hdlr"_u32>(),
	AtomType::make<keys, "keys"_u32>(),
	AtomType::make<void, "ilst"_u32>(
		AtomType::make<void, 0>()
	)
);

AtomType	video_codec_children[] = {
	AtomType::make<h265::Configuration, "hvcC"_u32>(),
	AtomType::make<AV1Configuration, "avcC"_u32>(),
	AtomType::make<dvcc, "dvcC"_u32>(),
	AtomType::make<dvcc, "dvvC"_u32>(),
	AtomType::make<colr, "colr"_u32>(),
	AtomType::make<void, "amve"_u32>(),
	AtomType::make<void, "pasp"_u32>(),
	AtomType::make<void, "btrt"_u32>(),
	terminate
};
AtomType	audio_codec_children[] = {
	AtomType::make<void, "btrt"_u32>(),
	AtomType::make<esds, "esds"_u32>(),
	AtomType::make<frma, "frma"_u32>(),
	AtomType::make<void, "chan"_u32>(),
	terminate
};

AtomType root_atoms[] = {
	AtomType::make<ftyp, "ftyp"_u32>(),
	AtomType::make<pdin, "pdin"_u32>(),

	AtomType::make<void, "moov"_u32>(
		AtomType::maker<mvhd, "mvhd"_u32>(),
		AtomType::make<void, "trak"_u32>(
			AtomType::maker<tkhd, "tkhd"_u32>(),
			AtomType::make<tref, "tref"_u32>(),
			AtomType::make<void, "edts"_u32>(
				AtomType::make<elst, "elst"_u32>()
			),
			AtomType::make<void, "tapt"_u32>(
				AtomType::make<clef, "clef"_u32>(),
				AtomType::make<prof, "prof"_u32>(),
				AtomType::make<enof, "enof"_u32>()
			),
			AtomType::make<void, "mdia"_u32>(
				AtomType::maker<mdhd, "mdhd"_u32>(),
				AtomType::make<hdlr, "hdlr"_u32>(),
				AtomType::make<void, "minf"_u32>(
					AtomType::make<hdlr, "hdlr"_u32>(),
					AtomType::make<vmhd, "vmhd"_u32>(),
					AtomType::make<smhd, "smhd"_u32>(),
					AtomType::make<hmhd, "hmhd"_u32>(),
					AtomType::make<nmhd, "nmhd"_u32>(),
					AtomType::make<void, "dinf"_u32>(
						AtomType::make<urn, "urn "_u32>(),
						AtomType::make<dref, "dref"_u32>(
							AtomType::make<data_entry, "alis "_u32>(),
							AtomType::make<data_entry, "rsrc "_u32>(),
							AtomType::make<url, "url "_u32>(),
							AtomType::make<urn, "urn "_u32>()
						)
					),
					AtomType::make<void, "stbl"_u32>( 
						AtomType::make<stsd, "stsd"_u32>(
							AtomType::make<video_sample_desc, "hevc"_u32>(video_codec_children),
							AtomType::make<video_sample_desc, "dvav"_u32>(video_codec_children),	//DolbyVisionAVC3SampleEntry 
							AtomType::make<video_sample_desc, "dva1"_u32>(video_codec_children),	//DolbyVisionAVC1SampleEntry 
							AtomType::make<video_sample_desc, "dvhe"_u32>(video_codec_children),	//DolbyVisionHEV1SampleEntry 
							AtomType::make<video_sample_desc, "dvh1"_u32>(video_codec_children),	//DolbyVisionHVC1SampleEntry 
							AtomType::make<video_sample_desc, "avc1"_u32>(video_codec_children),	//DolbyVisionAVCCompatibleSampleEntry 
							AtomType::make<video_sample_desc, "avc3"_u32>(video_codec_children),	//DolbyVisionAVCCompatibleSampleEntry
							AtomType::make<video_sample_desc, "avc2"_u32>(video_codec_children),	//AVC2SampleEntry 
							AtomType::make<video_sample_desc, "avc4"_u32>(video_codec_children),	//AVC2SampleEntry 
							AtomType::make<video_sample_desc, "hev1"_u32>(video_codec_children),	//HEVCSampleEntry 
							AtomType::make<video_sample_desc, "hvc1"_u32>(video_codec_children),	//HEVCSampleEntry 
							AtomType::make<audio_sample_desc, "mp4a"_u32>(audio_codec_children)
						),
						AtomType::make<stts, "stts"_u32>(),
						AtomType::make<ctts, "ctts"_u32>(),
						AtomType::make<stsc, "stsc"_u32>(),
						AtomType::make<stsz, "stsz"_u32>(),
						AtomType::make<stz2, "stz2"_u32>(),
						AtomType::make<stco, "stco"_u32>(),
						AtomType::make<co64, "co64"_u32>(),
						AtomType::make<stss, "stss"_u32>(),
						AtomType::make<stsh, "stsh"_u32>(),
						AtomType::make<padb, "padb"_u32>(),
						AtomType::make<stdp, "stdp"_u32>(),
						AtomType::make<sdtp, "sdtp"_u32>(),
						AtomType::make<sbgp, "sbgp"_u32>(),
						AtomType::make<sgpd, "sgpd"_u32>(),
						AtomType::maker<subs, "subs"_u32>()
					)
				)
			)
		),
		AtomType::make<void, "mvex"_u32>(
			AtomType::maker<mehd, "mehd"_u32>(),
			AtomType::make<trex, "trex"_u32>()
		),
		AtomType::make<ipmc, "ipmc"_u32>(),
		AtomType::make<void, "udta"_u32>(meta_data),
		meta_data0
	),

	AtomType::make<void, "moof"_u32>(
		AtomType::make<mfhd, "mfhd"_u32>(),
		AtomType::make<void, "traf"_u32>(
			AtomType::make<tfhd, "tfhd"_u32>(),
			AtomType::make<trun, "trun"_u32>(),
			AtomType::make<sdtp, "sdtp"_u32>(),
			AtomType::make<sbgp, "sbgp"_u32>(),
			AtomType::maker<subs, "subs"_u32>()
		)
	),
	AtomType::make<void, "mfra"_u32>(
		AtomType::make<tfra, "tfra"_u32>(),
		AtomType::make<mfro, "mfro"_u32>()
	),
//	AtomType::make<void, "mdat"_u32>(),
	AtomType::make<qtfree, "free"_u32>(),
	AtomType::make<qtskip, "skip"_u32>(),
	AtomType::make<void, "udta"_u32>(
		AtomType::make<cprt, "cprt"_u32>()
	),
	AtomType::make<qtmeta, "meta"_u32>(
		AtomType::make<hdlr, "hdlr"_u32>(),
		AtomType::make<void, "dinf"_u32>(
			AtomType::make<dref, "dref"_u32>()
		),
		AtomType::make<void, "idat"_u32>(),
		AtomType::make<ipmc, "ipmc"_u32>(),
		AtomType::maker<iloc, "iloc"_u32>(),
		AtomType::make<iref, "iref"_u32>(
			AtomType::make<auxl, "auxl"_u32>(),
			AtomType::make<cdsc, "cdsc"_u32>(),
			AtomType::make<dimg, "dimg"_u32>(),
			AtomType::make<thmb, "thmb"_u32>()
		),
		AtomType::make<void, "iprp"_u32>(
			AtomType::make<void, "ipco"_u32>(
				AtomType::make<auxc, "auxC"_u32>(),
				AtomType::make<void, "av1C"_u32>(),
				AtomType::make<void, "clap"_u32>(),
				AtomType::make<colr, "colr"_u32>(),
				AtomType::make<h265::Configuration, "hvcC"_u32>(),
				AtomType::make<irot, "irot"_u32>(),
				AtomType::make<imir, "imir"_u32>(),
				AtomType::make<ispe, "ispe"_u32>(),
				AtomType::make<void, "pasp"_u32>(),
				AtomType::make<pixi, "pixi"_u32>(),
				AtomType::make<void, "rloc"_u32>()
			),
			AtomType::maker<ipma, "ipma"_u32>()
		),
		AtomType::make<void, "ipro"_u32>(
			AtomType::make<void, "sinf"_u32>(
				AtomType::make<frma, "frma"_u32>(),
				AtomType::make<imif, "imif"_u32>(),
				AtomType::make<schm, "schm"_u32>(),
				AtomType::make<schi, "schi"_u32>()
			)
		),
		AtomType::maker<iinf, "iinf"_u32>(
			AtomType::maker<infe, "infe"_u32>()
		),
		AtomType::make<xml , "xml "_u32>(),
		AtomType::make<bxml, "bxml"_u32>(),
		AtomType::make<pitm, "pitm"_u32>()
	),
	terminate,
};

//-----------------------------------------------------------------------------
// iso defs
//-----------------------------------------------------------------------------

template<typename T> ISO_DEFCOMPT(qt_table,T,1)	{ ISO_SETFIELD(0, entries); }};

template<> struct ISO::def<qttime> : ISO::VirtualT2<qttime> {
	static ISO_ptr<void>	Deref(const qttime &t) {
		return ISO_ptr<string>(0, to_string((DateTime)t));
	}
};


ISO_DEFSAME(id4, char[4]);
ISO_DEFSAME(version_flags, uint32);

ISO_DEFCOMPBV(version_flags_time, version_flags, creation_time, modification_time);

ISO_DEFUSERCOMPV(ftyp, major_brand, minor_version);// , compatible_brands);
ISO_DEFSAME(data_entry, version_flags);
ISO_DEFUSERCOMPBV(url, data_entry, location);
ISO_DEFUSERCOMPBV(urn, data_entry, name, location);

ISO_DEFUSERCOMPV(reference, from_item_id, to);
ISO_DEFSAME(ipma::Association, xint16);
ISO_DEFUSERCOMPV(ipma::Entry, item_id, associations);
ISO_DEFUSERCOMPBV(ipma, version_flags, entries);

ISO_DEFUSERCOMPBV(mvhd, version_flags_time,
	time_scale,duration,preferred_rate,preferred_volume,reserved,matrix,preview_time,preview_duration,
	poster_time,selection_time,selection_duration,current_time,next_track
);

ISO_DEFUSERCOMPBV(tkhd, version_flags_time, id,reserved,duration,reserved2,layer,alternate_group,volume,reserved3, matrix,width,height);
ISO_DEFUSERCOMPBV(mdhd, version_flags_time, time_scale,duration,language,quality);
ISO_DEFUSERCOMPBV(vmhd, version_flags, graphics_mode,op_color);
ISO_DEFUSERCOMPBV(smhd, version_flags, balance);
ISO_DEFUSERCOMPBV(hmhd, version_flags, maxPDUsize, avgPDUsize, maxbitrate, avgbitrate);
ISO_DEFUSERCOMPBV(hdlr, version_flags, type,subtype,manufacturer,flags2,flags2_mask,name);

//sample descriptor
ISO_DEFUSERCOMPV(sample_desc,reserved,data_ref_index);
ISO_DEFUSERCOMPBV(video_sample_desc, sample_desc,
	version,revision_level,vendor,temporal_quality,spatial_quality,width,height,horizontal_resolution,
	verical_resolution,data_size,frame_count,compressor_name,depth,colortable_id
);
ISO_DEFUSERCOMPBV(audio_sample_desc, sample_desc,
	channelcount, samplesize, pre_defined, reserved2, samplerate
);

ISO_DEFUSERCOMPV(time_to_sample,sample_count,sample_delta);
ISO_DEFUSERCOMPV(sample_to_chunk,first_chunk,samples_per_chunk,sample_desc_id);
ISO_DEFUSERCOMPV(composition_offset,sample_count, sample_offset);
ISO_DEFUSERCOMPBV(stsz, version_flags, sample_size, entries);
ISO_DEFUSERCOMPBV(dref, version_flags, count);
ISO_DEFUSERCOMPBV(padb, version_flags, count);
ISO_DEFUSERCOMPBV(stz2, version_flags, field_size, count);
ISO_DEFUSERCOMPV(shadow_sync_sample, shadowed_sample_number, sync_sample_number);
ISO_DEFUSERCOMPBV(cprt, version_flags, language, notice);

template<int VER> ISO_DEFCOMPVT(elst_entry, VER, segment_duration, media_time, media_rate_int, media_rate_frac);
ISO_DEFCOMPBV(mehd, version_flags, fragment_duration);

ISO_DEFUSERCOMPBV(trex, version_flags, track_ID, default_sample_description_index, default_sample_duration, default_sample_size, default_sample_flags);
ISO_DEFUSERCOMPBV(mfhd, version_flags, sequence_number);
ISO_DEFUSERCOMPBV(tfhd, version_flags, track_ID, base_data_offset, sample_description_index, default_sample_duration, default_sample_size, default_sample_flags);

ISO_DEFUSERCOMPV(trun::sample, duration, size, flags, composition_time_offset);
ISO_DEFUSERCOMPBV(trun, version_flags, data_offset, first_sample_flags, entries);

ISO_DEFUSERCOMPBV(tfra, version_flags, track_ID);
ISO_DEFUSERCOMPBV(mfro, version_flags, size);
ISO_DEFUSERCOMPV(sbgp::entry, sample_count, group_description_index);
ISO_DEFUSERCOMPBV(sbgp, version_flags, grouping_type, entries);
ISO_DEFUSERCOMPBV(sdtp, version_flags, entries);
ISO_DEFUSERCOMPBV(sgpd, version_flags, grouping_type, entries);

ISO_DEFUSERCOMPV(subs::subsample, size, priority, discardable);
ISO_DEFUSERCOMPV(subs::entry, sample_delta, subs);
ISO_DEFUSERCOMPBV(subs, version_flags, entries);

ISO_DEFUSERCOMPBV(xml, version_flags, xml);
ISO_DEFUSERCOMPBV(bxml, version_flags, data);
ISO_DEFUSERCOMPBV(pitm, version_flags, item_id);
ISO_DEFUSERCOMPV(iloc::entry::extent, index, offset, length);
ISO_DEFUSERCOMPV(iloc::entry, item_id, construction_method, data_reference_index, base_offset, extents);
ISO_DEFUSERCOMPBV(iloc, version_flags, entries);

ISO_DEFUSERCOMPBV(infe, version_flags, item_id, item_protection_index, item_type, item_name, content_type, content_encoding);
ISO_DEFUSERCOMPBV(iinf, version_flags, count);


template<typename T> struct is_bit_field_t {
	template<typename U> static constexpr bool helper(...)					{ return true;	}
	template<typename U> static constexpr bool helper(decltype(&U::a) arg)	{ return false; }
	static constexpr bool value = helper<T>(nullptr); 
};

ISO_DEFUSERCOMPV(h265::Configuration::entry, array_completeness, NAL_unit_type, units);
ISO_DEFUSERCOMPV(h265::Configuration::unit, get);

ISO_DEFUSERCOMPV(h265::Configuration,
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

ISO_DEFUSERCOMPV(AV1Configuration, version, marker, seq_level_idx_0, seq_profile, chroma_sample_position, chroma_subsampling_y, chroma_subsampling_x, monochrome, twelve_bit, high_bitdepth, seq_tier_0, initial_presentation_delay_minus_one, initial_presentation_delay_present);

ISO_DEFUSERCOMPV(dvcc, version_major, version_minor, profile, level, rpu_present_flag, el_present_flag, bl_present_flag, dv_bl_signal_compatibility_id);

ISO_DEFUSERCOMPV(colr, type);

ISO_DEFUSERCOMPBV(pixi, version_flags, bits_per_channel);
ISO_DEFUSERCOMPBV(auxc, version_flags, type);
ISO_DEFUSERCOMPBV(ispe, version_flags, width, height);
ISO_DEFUSERCOMPV(irot, flags);
ISO_DEFUSERCOMPV(imir, flags);

ISO_DEFUSERCOMPBV(mhdr, version_flags, next_item_id);
ISO_DEFUSERCOMPV(keys::key, namespce, name);
ISO_DEFUSERCOMPBV(keys, version_flags, entries);

ISO_DEFUSERCOMPBV(qt_size, version_flags, width, height);
ISO_DEFUSERCOMPV(frma, format);

ISO_DEFUSERCOMPPV(esds::section3, uint32, es_id, priority, sections);
ISO_DEFUSERCOMPV(esds::section4, type,stream,upstream,reserved,buffer_size,maximum_bit_rate,average_bit_rate);
ISO_DEFUSERCOMPV(esds::section5, audio_profile,profile_id,other_flags,num_channels);
ISO_DEFUSERCOMPV(esds::section6, sl);
ISO_DEFUSERCOMPBV(esds, version_flags, sections);

template<> struct ISO::def<esds::section> : ISO::VirtualT2<esds::section> {
//	uint32	Count(esds::section& a)		{ return 1; }
	Browser2 Deref(esds::section& a)	{
		switch (a.type) {
			case 3:	return MakeBrowser(make_param_element(*(esds::section3*)a.p, +a.len));
			case 4: return MakeBrowser((esds::section4*)a.p);
			case 5: return MakeBrowser((esds::section5*)a.p);
			case 6: return MakeBrowser((esds::section6*)a.p);
			default: return Browser();
		}
	}
};

namespace h265 {
ISO_DEFUSERENUMV(CHROMA, CHROMA_MONO, CHROMA_420, CHROMA_422, CHROMA_444);
}

//-----------------------------------------------------------------------------

void ReadAtoms(anything *b, istream_ref file, streamptr end, const AtomType *atom_types, bool read_unknown=true) {
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
			if (end)
				file.seek(end);
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
					if (i->reader)
						i->reader(file, p, size);
					else
						file.readbuff(p, i->type->GetSize());
					b2->Append(p);
				}
				ReadAtoms(b2, file, end1, i->subtypes, read_unknown);
				b->Append(b2);

			} else if (i->type) {
				if (i->reader) {
					auto	p = ISO::MakePtr(i->type, id1);
					make_skip_size(file, size), i->reader(file, p, size);
					b->Append(p);
				} else {
					void *p = ISO::MakeRawPtrSize<32>(i->type, id1, size);
					file.readbuff(p, size);
					b->Append(ISO_ptr<void>::Ptr(p));
				}

			} else {
				b->Append(ISO::MakePtr(id1, malloc_block(file, size)));
			}
		} else if (read_unknown || !atom_types[0].id) {
			b->Append(ISO::MakePtr(id1, malloc_block(file, size)));
		} else {
			file.seek_cur(size);
		}
	}
}

uint64 ReadAtomsTo(istream_ref file, streamptr size, id4 tag) {
	atom	a;
	auto	start	= file.tell();
	while ((size == 0 || file.tell() - start < size) && file.read(a)) {
		if (a.size == 0 && size == 0)
			break;

		if (!a.id.valid())
			break;

		if (a.id == tag)
			return a.size;

		file.seek_cur(a.size);
	}
	return 0;
}

uint32 GetMajorBrand(istream_ref file) {
	ftyp	f;
	file.seek(0);
	return ReadAtomsTo(file, file.length(), "ftyp"_u32) && file.read(f) ? (uint32)f.major_brand : 0;
}

//-----------------------------------------------------------------------------
// meta data
//-----------------------------------------------------------------------------

hash_map<const char *, tiff::TAG> qt_meta = {
//	{"com.apple.quicktime.album",				tiff::TAG::					},
	{"com.apple.quicktime.artist",				tiff::TAG::Artist			},
//	{"com.apple.quicktime.artwork",				tiff::TAG::					},
	{"com.apple.quicktime.author",				tiff::TAG::Author			},
	{"com.apple.quicktime.comment",				tiff::TAG::Comment			},
	{"com.apple.quicktime.copyright",			tiff::TAG::Copyright		},
	{"com.apple.quicktime.creationdate",		tiff::TAG::CreateDate		},
	{"com.apple.quicktime.description",			tiff::TAG::ImageDescription	},
	{"com.apple.quicktime.director",			tiff::TAG::Director			},
	{"com.apple.quicktime.title",				tiff::TAG::Title			},
	{"com.apple.quicktime.genre",				tiff::TAG::Music_Genre		},
//	{"com.apple.quicktime.information",			tiff::TAG::Information		},
	{"com.apple.quicktime.keywords",			tiff::TAG::Keywords			},
//	{"com.apple.quicktime.location.ISO6709",	tiff::TAG::					},
	{"com.apple.quicktime.producer",			tiff::TAG::Producer			},
	{"com.apple.quicktime.publisher",			tiff::TAG::Publisher		},
	{"com.apple.quicktime.software",			tiff::TAG::Software			},
	{"com.apple.quicktime.year",				tiff::TAG::VersionYear		},
//	{"com.apple.quicktime.collection.user",		tiff::TAG::					},
	{"com.apple.quicktime.rating.user",			tiff::TAG::Rating			},
//	{"com.apple.quicktime.location.name",		tiff::TAG::
//	{"com.apple.quicktime.location.body",		tiff::TAG::
//	{"com.apple.quicktime.location.note",		tiff::TAG::
//	{"com.apple.quicktime.location.role",		tiff::TAG::
//	{"com.apple.quicktime.location.date",		tiff::TAG::
//	{"com.apple.quicktime.direction.facing",	tiff::TAG::GPSImgDirection,
//	{"com.apple.quicktime.direction.motion",	tiff::TAG::
//	{"\xa9arg",									tiff::TAG:://	Name of arranger
//	{"\xa9ark",									tiff::TAG:://	Keywords for arranger							X
//	{"\xa9cok",									tiff::TAG:://	Keywords for composer							X
//	{"\xa9com",									tiff::TAG:://	Name of composer
	{"\xa9" "cpy",								tiff::TAG::Copyright},
	{"\xa9" "day",								tiff::TAG::CreateDate},
	{"\xa9" "dir",								tiff::TAG::Director},
//	{"\xa9" "fmt",								tiff::TAG:://	Indication of movie format (computer-generated, digitized, and so on)
//	{"\xa9" "inf",								tiff::TAG::Information},
//	{"\xa9" "isr",								tiff::TAG:://	ISRC code
//	{"\xa9" "lab",								tiff::TAG:://	Name of record label
//	{"\xa9" "lal",								tiff::TAG:://	URL of record label
//	{"\xa9" "mak",								tiff::TAG:://	Name of file creator or maker
//	{"\xa9" "mal",								tiff::TAG:://	URL of file creator or maker
	{"\xa9" "nak",								tiff::TAG::Keywords},
	{"\xa9" "nam",								tiff::TAG::Title},
//	{"\xa9" "pdk",								tiff::TAG:://	Keywords for producer							X
//	{"\xa9" "phg",								tiff::TAG:://	Recording copyright statement, normally preceded by the symbol ../art/phono_symbol.gif
	{"\xa9" "prd",								tiff::TAG::Producer},
//	{"\xa9" "prf",								tiff::TAG:://	Names of performers
//	{"\xa9" "prk",								tiff::TAG:://	Keywords of main artist and performer			X
//	{"\xa9" "prl",								tiff::TAG:://	URL of main artist and performer
//	{"\xa9" "req",								tiff::TAG:://	Special hardware and software requirements
//	{"\xa9" "snk",								tiff::TAG:://	Subtitle keywords of the content				X
//	{"\xa9" "snm",								tiff::TAG::Subtitle},
//	{"\xa9" "src",								tiff::TAG:://	Credits for those who provided movie source content
//	{"\xa9" "swf",								tiff::TAG:://	Name of songwriter
//	{"\xa9" "swk",								tiff::TAG:://	Keywords for songwriter							X
	{"\xa9" "swr",								tiff::TAG::Software},
//	{"\xa9" "wrt",								tiff::TAG:://	Name of movie’s writer
//	{"AllF",									tiff::TAG:://	Play all frames—byte indicating that all frames of video should be played, regardless of timing
//	{"hinf",									tiff::TAG:://	Hint track information—statistical data for real-time streaming of a particular track. For more information, see Hint Track User Data Atom.
//	{"hnti",									tiff::TAG:://	Hint info atom—data used for real-time streaming of a movie or a track. For more information, see Movie Hint Info Atom and Hint Track User Data Atom.
//	{"name",									tiff::TAG:://	Name of object
//	{"tnam",									tiff::TAG:://	Localized track name optionally present in Track user data. The payload is described in Track Name.
//	{"tagc",									tiff::TAG:://	Media characteristic optionally present in Track user data—specialized text that describes something of interest about the track. For more information, see Media Characteristic Tags.
//	{"LOOP",									tiff::TAG:://	Long integer indicating looping style. This atom is not present unless the movie is set to loop. Values are 0 for normal looping, 1 for palindromic looping.
//	{"ptv ",									tiff::TAG:://	Print to video—display movie in full screen mode. This atom contains a 16-byte structure, described in Print to Video (Full Screen Mode).
//	{"SelO",									tiff::TAG:://	Play selection only—byte indicating that only the selected area of the movie should be played
//	{"WLOC",									tiff::TAG:://	Default window location for movie—two 16-bit values, {x,y}
};

malloc_block MOVGetExif(istream_ref file) {
	ISO_ptr<anything>	p("");
	ReadAtoms(p, file, file.length(), root_atoms, false);

	tiff::EXIF_maker	exif;

	if (auto moov = ISO::Browser(p)["moov"]) {
		for (auto i : moov) {
			auto	name = i.GetName().get_tag();
			if (name == "mvhd") {
				mvhd *mvhd = i;
				exif.add_entry(tiff::TAG::Duration, mvhd->duration * 10000000ull / mvhd->time_scale);
				exif.add_entry(tiff::TAG::CreateDate, (DateTime)mvhd->creation_time);
				exif.add_entry(tiff::TAG::ModifyDate, (DateTime)mvhd->modification_time);

			} else if (name == "trak") {
				if (tkhd *tkhd = i["tkhd"]) {
					if (auto w = get(tkhd->width)) {
						exif.add_entry(tiff::TAG::ImageWidth, (uint32)w);
						exif.add_entry(tiff::TAG::ImageHeight, (uint32)get(tkhd->height));
					}
				}

				auto	media		= i["mdia"];
				uint32	timescale	= 1;

				if (mdhd *mdhd = media["mdhd"])
					timescale	= mdhd->time_scale;
				
				stts *stts = media/"minf"/"stbl"/"stts";

				if (hdlr *hdlr = media["hdlr"]) {
					if (hdlr->subtype == "vide"_u32) {
						if (stts)
							exif.add_entry(tiff::TAG::FrameRate, float(timescale) * 1000 / stts->entries()[0].sample_delta);

					} else if (hdlr->subtype == "soun"_u32) {
					}
				}

			} else if (name == "udta" || name == "meta") {
				if (auto meta = name == "meta" ? i : i["meta"]) {
					dynamic_array<named<malloc_block>>	m;

					if (keys *keys = meta["keys"]) {
						for (auto &i : keys->entries())
							m.emplace_back(i.name());

						for (auto i : meta["ilst"]) {
							int	x = from_string(i.GetName().get_tag());
							if (x > 0 && x <= m.size())
								static_cast<malloc_block&>(m[x - 1]) = i.RawData();
						}
					} else {
						for (auto i : meta["ilst"])
							m.emplace_back(i.GetName().get_tag(), i.RawData());
					}

					for (auto& i : m) {
						if (qt_meta[i.name()].exists())
							exif.add_entry(qt_meta[i.name()], 42);
					}

				}
			}
		}

	}

	return exif.get_data();

	//	Audio.EncodingBitrate
	//	Audio.ChannelCount
	//	Audio.SampleRate

}

template<typename D> void colour_conversion(h265::image_base *in, block<D,2> out, srgb_converter2 conv) {
	//for (int y = 0; y < height; y++) {
	parallel_for(int_range(in->get_height()), [in, out, conv, chroma = in->get_chroma_format()](int y) {
		int		shiftw	= get_shift_W(chroma);
		int		shifth	= get_shift_H(chroma);
		int		width	= in->get_width();
		int		bpp		= in->get_bit_depth(0);

		if (bpp > 8) {
			conv.process_row(
				in->get_plane_ptr<uint16>(0)[y],
				in->get_plane_ptr<uint16>(1)[y >> shifth],
				in->get_plane_ptr<uint16>(2)[y >> shifth],
				out[y].begin(),
				width, bpp, shiftw
			);
		} else {
			conv.process_row(
				in->get_plane_ptr<uint8>(0)[y],
				in->get_plane_ptr<uint8>(1)[y >> shifth],
				in->get_plane_ptr<uint8>(2)[y >> shifth],
				out[y].begin(),
				width, bpp, shiftw
			);
		}
	});
}

enum ROT : uint8 {
	HFLIP		= 1,
	VFLIP		= 2,
	TRANSPOSE	= 4,
};

template<typename A, typename B> void copy1(transpose_s<iblock<A, 2>> &&a, iblock<B, 2> &&b) {
#if 1
	int	w = a.m.template size<1>();
	int	h = a.m.template size<2>();
	if (w > 64 && h > 64) {
	#if 0
		copy1(transpose(get(a.m.template slice<1>(0, w / 2))), b.template slice<2>(0, w / 2));
		copy1(transpose(get(a.m.template slice<1>(w / 2))), b.template slice<2>(w / 2));
	#else
		copy1(transpose(get(a.m.template slice<2>(0, h / 2))), b.template slice<1>(0, h / 2));
		copy1(transpose(get(a.m.template slice<2>(h / 2))), b.template slice<1>(h / 2));
	#endif
		return;
	}
#endif
	int	y = 0;
	for (auto ay : a.m) {
		int	x = 0;
		for (auto &ax : ay)
			b[x++][y] = ax;
		++y;
	}
}

void OrientImage(ISO_ptr<bitmap> &bm, ROT rot) {
	if (rot & TRANSPOSE) {
		ISO_ptr<bitmap>	bm2(bm.ID(), bm->Height(), bm->Width());
	#if 1
		copy1(transpose(bm->All()), bm2->All());
		bm	= bm2;
	#else
		auto	src		= bm->All();
		auto	dst		= bm2->All();
		int		xxor	= rot & HFLIP ? -1 : 0;
		int		xadd	= rot & HFLIP ? bm->Height() : 0;
		int		yxor	= rot & VFLIP ? -1 : 0;
		int		yadd	= rot & VFLIP ? bm->Width() : 0;
		parallel_for(int_range(bm->Width()), [=](int y) {
			for (int x = 0; x < bm->Height(); x++)
			dst[y][x] = src[(x ^ xxor) + xadd][(y ^ yxor) + yadd];
			});
		bm	= bm2;
		return;
	#endif
	}
	if (rot & VFLIP) {
		reverse(bm->All());
	}
	if (rot & HFLIP) {
		for (int y = 0; y < bm->Height(); y++)
			reverse(bm->ScanLineBlock(y));
	}
}



//-----------------------------------------------------------------------------
// file handlers
//-----------------------------------------------------------------------------

class QTFileHandler : public FileHandler {
	int				Check(istream_ref file) override {
		atom	a;
		file.seek(0);
		return file.read(a) && a.id == "ftyp"_u32 ? CHECK_PROBABLE : CHECK_UNLIKELY;
	}
public:
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	p(id);
		ReadAtoms(p, file, file.length(), root_atoms);
		return p;
	}
} qt;

class MOVFileHandler : public QTFileHandler {
	const char*		GetExt()				override { return "mov"; }
	int				Check(istream_ref file)	override { return GetMajorBrand(file) == "qt  "_u32 ? CHECK_PROBABLE : CHECK_UNLIKELY; }
public:
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	MOVFileHandler()		{ ISO::getdef<movie>(); }
} mov;

class MP4FileHandler : public QTFileHandler {
	const char*		GetExt()				override { return "mp4"; }
	const char*		GetMIME()				override { return "video/mp4"; }
	int				Check(istream_ref file)	override { return GetMajorBrand(file) == "mp42"_u32 ? CHECK_PROBABLE : CHECK_UNLIKELY; }
} mp4;


class MOV_frames : public ISO::VirtualDefaults {
	istream_ptr					file;
	const video_sample_desc*	vsd;
	const h265::Configuration*	hvc;
	const colr*					colr;

	h265::decoder_context	decoder	= {h265::OPT_force_sequential | h265::OPT_disable_deblocking};// | h265::decoder_context::OPT_disable_sao};
//	h265::decoder_context	decoder;
	h265::NAL::Parser		parser;
	QTSamples				samples;
	QTSamples::iterator		sample;

public:
	MOV_frames(ISO_ptr<anything> p, istream_ref file);
	int				Count()								{ return samples.size();	}
	ISO::Browser2	Index(int i);
	int				Width()								{ return vsd->width;	}
	int				Height()							{ return vsd->height;	}
	float			FrameRate()							;//{ return frame_rate;	}
};

ISO_DEFVIRT(MOV_frames);


MOV_frames::MOV_frames(ISO_ptr<anything> p, istream_ref file) : file(file.clone()) {
	ISO::Browser2	moov	= (*p)["moov"];
	auto	trak	= moov["trak"];
	auto	stbl	= trak["mdia"]["minf"]["stbl"];
	auto	stsd	= stbl["stsd"];

	samples	= {stbl["stsc"], stbl["stco"], stbl["stsz"]};

//	if (stss *stss = stbl["stss"])
//		samples.sync_samples = stss->entries();

	sample	= samples.begin();

	auto	codec	= stsd[1];
	colr	= codec["colr"];
	vsd		= codec[0];

	auto	name	= codec.GetName();

	if (name == "hvc1") {
		hvc = codec["hvcC"];

		for (auto &e : hvc->entries()) {
			for (auto& unit : e.units())
				parser.push_nal(unit, 0, nullptr);
		}

		while (decoder.decode(parser) == 1)
			;
	}
}

ISO::Browser2 MOV_frames::Index(int i) {
	//sample.move_to(i);
	
	auto	mem	= (*sample)(file);
	for (auto j : chunked_mem(mem))
		parser.push_nal(j, 0, nullptr);

	parser.end_of_frame	= true;
	sample.next();

	while (decoder.decode(parser) == 1) {
		if (auto img = decoder.get_next_picture_in_output_queue()) {
			//ISO_TRACEF("image ") << i << ", poc = " << img->picture_order_cnt << '\n';
			decoder.pop_next_picture_in_output_queue();
			if (img->get_bit_depth(0) > 8) {
				ISO_ptr<HDRbitmap>	bm(to_string(i), img->get_width(), img->get_height());
				//copy(img->get_plane_block<uint16>(0), bm->All());
				colour_conversion(img, bm->All(), colr);
				return bm;
			} else {
				ISO_ptr<bitmap>	bm(to_string(i), img->get_width(), img->get_height());
				colour_conversion(img, bm->All(), colr);
				return bm;
			}
		}
	}
	return {};
}

ISO_ptr<void> MOVFileHandler::Read(tag id, istream_ref file) {
	ISO_ptr<anything>	p(id);
	ReadAtoms(p, file, file.length(), root_atoms);
//	return p;

#if 1
	ISO_ptr<MOV_frames>	frames(id, p, file);
	return ISO_ptr<movie>(id, frames, frames->Width(), frames->Height(), 24);
#else
	ISO::Browser2	moov	= (*p)["moov"];
	auto	trak	= moov["trak"];
	auto	stbl	= trak["mdia"]["minf"]["stbl"];
	auto	stsd	= stbl["stsd"];

	QTSamples	samples(stbl["stsc"], stbl["stco"], stbl["stsz"]);
//	if (stss *stss = stbl["stss"])
//		samples.sync_samples = stss->entries();

	auto	s1 = samples.begin();
	auto	s1a	= samples.at_index(42);
	s1 += 42;
	s1 += 123 - 42;
	s1 += 42 - 123;
	auto	s2	= samples.at_index(123);


	auto	codec	= stsd[1];
	auto	name	= codec.GetName();
	colr	*colr	= codec["colr"];

	video_sample_desc*	vsd	= codec[0];

	if (name == "hvc1") {
		h265::Configuration		*hvc = codec["hvcC"];
	#if 0
		malloc_block2	raw;
	#if 0
		for (auto &e : hvc->entries()) {
			for (const_memory_block unit : e.units()) {
				uint32be	*p = raw.extend(unit.size() + 4);
				*p = unit.size();
				unit.copy_to(p + 1);
			}
		}
		for (auto &i : samples) {
			raw += i(file);
			//for (auto j : chunked_mem(i))
			//	raw += j;
		}
	#else
		for (auto &e : hvc->entries()) {
			for (const_memory_block unit : e.units()) {
				uint32be	*p = raw.extend(unit.size() + 4);
				*p = 1;
				unit.copy_to(p + 1);
			}
		}
		for (auto i : samples) {
			auto	mem	= i(file);
			for (auto j : chunked_mem(mem)) {
				uint32be	*p = raw.extend(j.size() + 4);
				*p = 1;
				j.copy_to(p + 1);
			}
		}
	#endif
		FileOutput("D:\\raw.bin").write(raw);
	#endif
		h265::decoder_context	decoder(h265::OPT_force_sequential|h265::OPT_disable_deblocking | h265::OPT_disable_sao);
		h265::NAL::Parser		parser;

		for (auto &e : hvc->entries()) {
			for (auto& unit : e.units())
				parser.push_nal(unit, 0, nullptr);
		}

		while (decoder.decode(parser) == 1)
			;

		for (auto i : samples) {
			auto	mem	= i(file);
			for (auto j : chunked_mem(mem))
				parser.push_nal(j, 0, nullptr);
			parser.end_of_frame	= true;

			while (decoder.decode(parser) == 1) {
				if (auto img = decoder.get_next_picture_in_output_queue()) {
					decoder.pop_next_picture_in_output_queue();
					if (img->get_bit_depth(0) > 8) {
						ISO_ptr<HDRbitmap>	bm(id, img->get_width(), img->get_height());
						colour_conversion(img, bm->All(), colr);
						return bm;
					} else {
						ISO_ptr<bitmap>	bm(id, img->get_width(), img->get_height());
						colour_conversion(img, bm->All(), colr);
						return bm;
					}
				}
			}
		}

	}
	return p;
#endif
}

struct vbitmap_YUV : vbitmap {
	h265::image_base	*img;

	bool	get(const vbitmap_loc &in, vbitmap_format fmt, void *dest, uint32 stride, uint32 width, uint32 height) {
		return false;
	}
	void*	get_raw(uint32 plane, vbitmap_format *fmt, uint32 *stride, uint32 *width, uint32 *height) {
		*width	= img->get_width(plane)		- (img->offsets[0] + img->offsets[2]);
		*height	= img->get_height(plane)	- (img->offsets[1] + img->offsets[3]);
		*stride	= img->get_image_stride(plane);
		*fmt	= img->get_bit_depth(plane);
		return (char*)img->planes[plane].pixels + img->offsets[1] * *stride + img->offsets[0];
	}

	vbitmap_YUV(h265::image_base *img) : vbitmap(this, img->get_bit_depth(0), img->get_width(), img->get_height()), img(img) {}
};
ISO_DEFSAME(vbitmap_YUV, vbitmap);

class HEVC_frames : public ISO::VirtualDefaults {
	istream_ptr				file;
	int						layer	= -1;
//	h265::decoder_context	decoder	= {h265::OPT_force_sequential | h265::OPT_disable_deblocking | h265::OPT_disable_sao};// | h265::OPT_disable_SHVC};
	h265::decoder_context	decoder;// = {h265::OPT_force_sequential};
	h265::NAL::Parser		parser;
	
	h265::image_base	*Next();

public:
	HEVC_frames(istream_ref file, int layer = -1) : file(file.clone()), layer(layer) {}
	int					Count()			{ return 1000;	}

	uint32x2			Size() {
		uint8	buffer[1024];
		size_t	n;

		while (all(decoder.size == zero) && (n = file.readbuff(buffer, sizeof(buffer)))) {
			parser.push_stream(const_memory_block(buffer, n), 0);
			while (decoder.decode(parser) == h265::decoder_context::RES_ok) {
				if (all(decoder.size != zero))
					break;
			}
		}
		return decoder.size;
	}

	ISO::Browser2		Index(int i) {
		if (auto img = Next()) {
		#if 1
			ISO_ptr<vbitmap_YUV>	frame("frame", img);
			decoder.pop_next_picture_in_output_queue(img->layer_id);
			return frame;
		#else
			if (img->get_bit_depth(0) > 8) {
				ISO_ptr<HDRbitmap>	bm("frame", img->get_width(), img->get_height());
				colour_conversion(img, bm->All(), nullptr);
				decoder.pop_next_picture_in_output_queue(img->layer_id);
				return bm;
			} else {
				ISO_ptr<bitmap>	bm("frame", img->get_width(), img->get_height());
				//copy(img->get_plane_block<uint8>(0), bm->All());
				colour_conversion(img, bm->All(), nullptr);
				decoder.pop_next_picture_in_output_queue(img->layer_id);
				return bm;
			}
		#endif
		}
		return {};
	}
};

h265::image_base* HEVC_frames::Next() {
#ifdef HEVC_ML
	if (auto img = decoder.get_next_picture_in_output_queue(layer < 0 ? ~0ull : bit64(layer)))
		return img;
#else
	if (auto img = decoder.get_next_picture_in_output_queue())
		return img;
#endif

	uint8	buffer[1024];

	for (;;) {
		while (decoder.decode(parser) == h265::decoder_context::RES_ok) {
		#ifdef HEVC_ML
			if (layer >= 0)
				decoder.clear_output_queues(~bit64(layer));
			if (auto img = decoder.get_next_picture_in_output_queue(layer < 0 ? ~0ull : bit64(layer)))
				return img;
		#else
			if (auto img = decoder.get_next_picture_in_output_queue())
				return img;
		#endif
		}

		if (parser.end_of_stream)
			return nullptr;

		if (auto n = file.readbuff(buffer, sizeof(buffer))) {
			parser.push_stream(const_memory_block(buffer, n), 0);

		} else {
			parser.end_of_stream = true;
			parser.flush_data();
		}
	}
}

ISO_DEFVIRT(HEVC_frames);

class HEVCBitstreamFileHandler : public FileHandler {
	const char*		GetExt()			override { return "hevc"; }
	const char*		GetDescription()	override { return "HEVC bitstream"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<HEVC_frames>	frames(id, file, ISO::root("variables")["layer"].Get(-1));
		auto	size = frames->Size();
		return ISO_ptr<movie>(id, frames, +size.x, +size.y, 24);
	}

} hevc_bitstream;


class YUVFileHandler : public FileHandler {
	const char*		GetExt()			override { return "yuv"; }
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<movie> m = ISO_conversion::convert<movie>(p);
		if (!m)
			return false;

		for (auto f : ISO::Browser(m->frames)) {
			if (f.Is("vbitmap")) {
				int	w, h, bit_depth;
				for (int i = 0; i < 3; ++i) {
					vbitmap_format fmt;
					uint32	w1, h1, s;
					if (void *data = ((vbitmap*)f)->GetRaw(i, &fmt, &s, &w1, &h1)) {
						bit_depth	= fmt;
						w			= w1 << (bit_depth > 8);
						h			= h1;
						for (int y = 0; y < h; y++)
							file.writebuff((char*)data + y * s, w);

					} else if (i > 0) {
						if (i == 1) {
							h >>= 1;
							w >>= 1;
						}
						malloc_block	temp(w);
						if (fmt > 8)
							temp.fill(uint16(1 << (bit_depth - 1)));
						else
							temp.fill(uint8(1 << (bit_depth - 1)));
						for (int y = 0; y < h; y++)
							file.write(temp);
					}
				}

			} else if (f.Is<bitmap>()) {
				ISO_ptr<bitmap>	p = f;
			} else {
				break;
			}
		}
		return true;
	}

} yuv;

//-----------------------------------------------------------------------------
// HEIF
//-----------------------------------------------------------------------------

struct HEIF {
	enum colorspace {
		colorspace_YCbCr		= 0,
		colorspace_RGB			= 1,
		colorspace_monochrome	= 2
	};
	enum channel {
		channel_Y		= 0,
		channel_Cb		= 1,
		channel_Cr		= 2,
		channel_R		= 3,
		channel_G		= 4,
		channel_B		= 5,
		channel_Alpha	= 6,
	};
	struct ImageGrid {
		uint8	version;
		uint16	rows,	cols;
		uint32	width,	height;

		ImageGrid(istream_ref file) {
			uint8	flags;
			file.read(version, flags);
			rows	= file.get<uint8>() + 1;
			cols	= file.get<uint8>() + 1;

			if (flags & 1) {
				width	= file.get<uint32be>();
				height	= file.get<uint32be>();
			} else {
				width	= file.get<uint16be>();
				height	= file.get<uint16be>();
			}
		}
	};

	struct ImageInfo {
		uint32x2		size		= zero;
		h265::CHROMA	chroma		= h265::CHROMA_MONO;
		uint8			bits[4];
	};

	Mutex				mutex;
	istream_ref			file;
	const_memory_block	idat;

	iloc*			iloc;
	ipma*			ipma;
	ISO::Browser	ipco;
	ISO::Browser	iref;

	hash_map<uint32, const infe*>	infe_map;
	uint32	primary_id		= ~0;

	HEIF(istream_ref file, ISO::Browser2 meta) : file(file),
		idat((meta/"idat").RawData()),
		iloc(meta/"iloc"),
		ipma(meta/"iprp"/"ipma"),
		ipco(meta/"iprp"/"ipco"),
		iref(meta/"iref")
	{
		if (pitm *p = meta/"pitm")
			primary_id = p->item_id;

		for (auto box : meta/"iinf") {
			if (box.GetName() == "infe") {
				infe	*i = box;
				infe_map[i->item_id] = i;
			}
		}
	}

	arbitrary_ptr	get_property(uint32 id, tag2 type) const {
		for (auto &a : ipma->get_associations(id)) {
			int	i = a.index - 1;
			if (ipco.GetName(i) == type)
				return (int*)ipco[i];
		}
		return nullptr;
	}

	reference*		get_reference(tag2 type) const {
		for (auto ref : iref) {
			if (ref.GetName() == type)
				return ref;
		}
		return nullptr;
	}

	reference*		get_reference(uint32 id, tag2 type) const {
		for (auto ref : iref) {
			if (ref.GetName() == type) {
				reference	*r	= ref;
				if (r->from_item_id == id)
					return r;
			}
		}
		return nullptr;
	}

	ImageInfo			image_info(uint32 id) const;
	h265::image_base*	decode_image(uint32 id, bool parallel) const;
	malloc_block		get_exif()	const;
};

malloc_block HEIF::get_exif() const {
	for (auto &i : infe_map) {
		if (i->item_type == "Exif"_u32) {
			auto	data	= iloc->find(i->item_id);
			dynamic_memory_writer	mem;
			data->read_data(file, idat, mem);
			return move(mem);
		}
	}
	return none;
}

HEIF::ImageInfo HEIF::image_info(uint32 id) const {
	HEIF::ImageInfo		info;

	ispe *ispe = get_property(id, "ispe");
	if (ispe)
		info.size = {ispe->width, ispe->height};

	const infe	*i		= infe_map[id];
	auto		data	= iloc->find(id);
	switch (i->item_type) {
		case "hvc1"_u32:
			if (h265::Configuration *hvc = get_property(id, "hvcC")) {
				info.chroma		= hvc->chromaFormat;
				info.bits[0]	= hvc->bitDepthLumaMinus8 + 8;
				info.bits[1]	= info.bits[2] = hvc->bitDepthChromaMinus8 + 8;
				
				if (!ispe) {
					h265::NAL::Parser		parser;
					for (auto &e : hvc->entries()) {
						for (auto& unit : e.units())
							parser.push_nal(unit, 0, nullptr);
					}
					h265::decoder_context::get_image_size(parser);
				}
			}
			break;

		case "av01"_u32:
			if (AV1Configuration *avc = get_property(id, "av1C")) {
			}
			break;

		case "grid"_u32: {
			dynamic_memory_writer	mem;
			with(mutex), data->read_data(file, idat, mem);

			ImageGrid grid(memory_reader(mem.data()));
			info.size = {grid.width, grid.height};

			if (pixi *pixi = get_property(id, "pixi"))
				copy(pixi->bits_per_channel(), info.bits);
			break;
		}
		default:
			break;
	}
	return info;
}


void paste_tile(h265::image_base *img, h265::image_base *tile, uint32 x0, uint32 y0, uint32 channels) {
	for (; channels; channels = clear_lowest(channels)) {
		int		channel		= lowest_set_index(channels);

		ISO_ASSERT(img->get_bit_depth(channel) == tile->get_bit_depth(channel));

		int		shiftw		= get_shift_W(img->chroma_format, channel);
		int		shifth		= get_shift_H(img->chroma_format, channel);
		auto	out_data	= img->get_plane_block<uint8>(channel, x0 >> shiftw, y0 >> shifth, tile->get_width() >> shiftw, tile->get_height() >> shifth);
		auto	tile_data	= tile->get_plane_block<uint8>(channel, 0, 0, out_data.size<1>(), out_data.size<2>());

		copy(tile_data, out_data);
	}
}

h265::image_base* HEIF::decode_image(uint32 id, bool parallel) const {
	const infe	*i		= infe_map[id];
	auto		data	= iloc->find(id);
	
	dynamic_memory_writer	mem;

	switch (i->item_type) {
		case "hvc1"_u32:
			if (h265::Configuration *hvc = get_property(id, "hvcC")) {
				//h265::decoder_context	decoder(h265::OPT_disable_deblocking | h265::OPT_disable_sao | h265::OPT_force_sequential);
				h265::decoder_context	decoder((parallel ? h265::OPT_default : h265::OPT_force_sequential));
				h265::NAL::Parser		parser;

				for (auto &e : hvc->entries()) {
					for (auto& unit : e.units())
						parser.push_nal(unit, 0, nullptr);
				}

				with(mutex), data->read_data(file, idat, mem);
				for (auto i : chunked_mem(mem.data()))
					parser.push_nal(i, 0, nullptr);

				parser.end_of_stream	= true;

				while (decoder.decode(parser)) {
					if (auto img = decoder.get_next_picture_in_output_queue()) {
					#ifdef HEVC_ML
						decoder.pop_next_picture_in_output_queue(0, true);
					#else
						decoder.pop_next_picture_in_output_queue(true);
					#endif
						return img;
					}
				}
			}
			break;

		case "av01"_u32:
			if (AV1Configuration *avc = get_property(id, "av1C")) {
				with(mutex), data->read_data(file, idat, mem);
			}
			break;

		case "grid"_u32: {
			with(mutex), data->read_data(file, idat, mem);
			ImageGrid grid(memory_reader(mem.data()));

			auto r = get_reference(id, "dimg");
			if (!r)
				break;

			dynamic_array<uint16>	image_references = r->to();
			if (image_references.size() != grid.rows * grid.cols)
				break;

			//return decode_image(image_references[46], false);

			auto	img			= new h265::image_base;
			auto	tile_info	= image_info(image_references[0]);
			img->chroma_format	= tile_info.chroma;

			pixi	 *pixi		= get_property(id, "pixi");
			int		nchannels	= pixi ? pixi->num_channels : num_channels(tile_info.chroma);
			for (int i = 0; i < nchannels; i++) {
				int		bit_depth	= pixi ? pixi->_bits_per_channel[i] : tile_info.bits[i];
				int		shiftw		= get_shift_W(tile_info.chroma, i);
				int		shifth		= get_shift_H(tile_info.chroma, i);
				img->planes[i].create(grid.width >> shiftw, grid.height >> shifth, bit_depth);
			}

			h265::tables_reference	refs;
			if (parallel) {
				parallel_for(image_references, [&](const uint16 &ref) {
					int	i	= image_references.index_of(ref);
					int	ix	= i % grid.cols, iy = i / grid.cols;

					auto	tile	= decode_image(ref, false);
					paste_tile(img, tile, ix * tile_info.size.x, iy * tile_info.size.y, 7);
					delete tile;
				});

			} else {
				auto	refs = image_references.begin();
				for (int y = 0, y0 = 0; y < grid.rows; y++) {
					for (int x = 0, x0 = 0; x < grid.cols; x++) {
						uint32	tile		= *refs++;
						auto	tile_img	= decode_image(tile, false);
						//return tile_img;

						paste_tile(img, tile_img, x0, y0, 7);
						delete tile_img;

						x0	+= tile_info.size.x;
					}
					y0 += tile_info.size.y;
				}
			}

			return img;

		};

		case "mime"_u32:
			//if (i->content_encoding == "deflate") {
			//	read_uncompressed = false;
			//	std::vector<uint8_t> compressed_data;
			//	error = m_iloc_box->read_data(*item, m_input_stream, m_idat_box, &compressed_data);
			//	*data = inflate(compressed_data);
			//}
			break;

		default:
			break;
	}
	return nullptr;
}


malloc_block HEICGetExif(istream_ref file) {
	ISO_ptr<anything>	p("");
	ReadAtoms(p, file, file.length(), root_atoms, false);
	HEIF	heif(file, (*p)["meta"]);
	return heif.get_exif();
}

bool HEICRotate(iostream_ref file, int rot) {
	auto	size = file.length();
	if (size = ReadAtomsTo(file, size, "meta"_u32)) {
		file.seek_cur(4);
		if ((size = ReadAtomsTo(file, size - 4, "iprp"_u32))
		&&  (size = ReadAtomsTo(file, size, "ipco"_u32))
		&&  (size = ReadAtomsTo(file, size, "irot"_u32))
		) {
			irot	r;
			file.read(r);
			r.flags = (r.flags + rot) & 3;
			file.seek_cur(-sizeof(r));
			file.write(r);
			return true;
		}
	}
	return false;
}

class HEICFileHandler : public BitmapFileHandler {
	const char*		GetExt()				override { return "heic"; }
	int				Check(istream_ref file)	override { return GetMajorBrand(file) == "heic"_u32 ? CHECK_PROBABLE : CHECK_UNLIKELY; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	p(id);
		ReadAtoms(p, file, file.length(), root_atoms, false);

		HEIF	heif(file, (*p)["meta"]);
		auto	item_id	= heif.primary_id;

		if (int thumb = WantThumbnail()) {
			if (auto tr = heif.get_reference("thmb")) {
				auto	info	= heif.image_info(tr->from_item_id);
				if (reduce_max(info.size) >= thumb)
					item_id = tr->from_item_id;
			}
		}

		timer	t;
		if (auto img = heif.decode_image(item_id, true)) {//true)) {
			ISO_OUTPUTF("decoded in ") << (float)t << "s\n";

			ISO_ptr<bitmap>	bm(id, img->get_width(), img->get_height());
		#if 0
			copy(img->get_plane_block<uint8>(0), bm->All());
		#else
			colour_conversion(img, bm->All(), (colr*)heif.get_property(item_id, "colr"));
		#endif
			delete img;
			ISO_OUTPUTF("converted after ") << (float)t << "s\n";
			
			uint8	rot	= 0;
			if (irot *p = heif.get_property(item_id, "irot"))
				rot = ((p->flags & 1) * (TRANSPOSE|VFLIP)) ^ (!!(p->flags & 2) * (HFLIP|VFLIP));
			if (imir *p = heif.get_property(item_id, "imir"))
				rot ^= p->flags & 1 ? HFLIP : VFLIP;

			OrientImage(bm, (ROT)rot);
			ISO_OUTPUTF("completed in ") << (float)t << "s\n";
			return bm;
		}
		return p;
		/*
		for (auto ref : meta/"iref") {
			if (ref.GetName() == "thmb") {
				// --- this is a thumbnail image, attach to the main image

			} else if (ref.GetName() == "auxl") {
				// --- this is an auxiliary image
				//     check whether it is an alpha channel and attach to the main image if yes
				// alpha channel
				if (auxC_property->get_aux_type() == "urn:mpeg:avc:2015:auxid:1" ||   // HEIF (avc)
					auxC_property->get_aux_type() == "urn:mpeg:hevc:2015:auxid:1" ||  // HEIF (h265)
					auxC_property->get_aux_type() == "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha") { // MIAF
				}
				// depth channel

				if (auxC_property->get_aux_type() == "urn:mpeg:hevc:2015:auxid:2" || // HEIF
					auxC_property->get_aux_type() == "urn:mpeg:mpegB:cicp:systems:auxiliary:depth") { // AVIF
					image->set_is_depth_channel_of(refs[0]);
				}
			}
		}
		*/
	}
} heic;

