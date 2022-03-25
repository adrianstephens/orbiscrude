#include "iso/iso_files.h"
#include "movie.h"
#include "filetypes/bitmap/jpg.h"

//--------------------------------------
// THPHeader
//--------------------------------------

namespace wii {

typedef iso::uint8		u8;
typedef iso::uint16be	u16;
typedef iso::uint32be	u32;
typedef iso::float32be	f32;

#define THP_VERSION		0x11000	// Version Infomation
#define THP_COMP_MAX	16		// Max component num

// Error
enum THPError {
	THP_ERROR_NOERROR =  0,
	THP_ERROR_FILEIO,
	THP_ERROR_THPFILE,
	THP_ERROR_JPEGFILE,
	THP_ERROR_DATA,
	THP_ERROR_FATAL,

	NUM_THP_ERRORS
};
// Component number
enum THPComponent {
	THP_VIDEO_COMP,
	THP_AUDIO_COMP,
	THP_NOCOMP_COMP = 0xFF
};
// VideoData Type
enum THPVideoType {
	THP_VIDEO_NON_INTERLACE,
	THP_VIDEO_ODD_INTERLACE,
	THP_VIDEO_EVEN_INTERLACE
};

//--------------------------------------
// THPHeader
//--------------------------------------
struct THPHeader {
	char	magic[4];				// "THP\0"
	u32		version;				// version number
	u32		bufSize;				// max frame size for buffer computation
	u32		audioMaxSamples;		// max samples of audio data
	f32		frameRate;				// frame per seconds
	u32		numFrames;
	u32		firstFrameSize;			// how much to load
	u32		movieDataSize;
	u32		compInfoDataOffsets;	// offset to component infomation data
	u32		offsetDataOffsets;		// offset to array of frame offsets
	u32		movieDataOffsets;		// offset to first frame (start of movie data)
	u32		finalFrameDataOffsets;	// offset to final frame
};

//--------------------------------------
// THPFrameCompInfo
//--------------------------------------
struct THPFrameCompInfo {
	u32		numComponents;				// a number of Components in a frame
	u8		frameComp[THP_COMP_MAX];	// kind of Components
};

//--------------------------------------
// THPVideoInfo
//--------------------------------------
struct THPVideoInfo {
	u32		xSize;					// width of video
	u32		ySize;					// height of video
	u32		videoType;
};

// OLD Structer (Ver 1.0000)
struct THPVideoInfoOld {
	u32		xSize;
	u32		ySize;
};

 //--------------------------------------
// THPAudioInfo
//--------------------------------------
struct THPAudioInfo {
	u32		sndChannels;
	u32		sndFrequency;
	u32		sndNumSamples;
	u32		sndNumTracks;			// number of Tracks
};

// OLD Structer
struct THPAudioInfoOld {
	u32		sndChannels;
	u32		sndFrequency;
	u32		sndNumSamples;
};

//--------------------------------------
// THPFileHeader
//--------------------------------------
struct THPFileHeader {
	THPHeader			header;
	THPFrameCompInfo	frameCompInfo;
	THPVideoInfo		videoInfo;	// THP_COMP_VIDEO
	THPAudioInfo		audioInfo;	// THP_COMP_AUDIO
};

//--------------------------------------
// THPFrameHeader
//--------------------------------------
struct THPFrameHeader {
	u32		frameSizeNext;
	u32		frameSizePrevious;
	u32		comp[THP_COMP_MAX];
};

}
//--------------------------------------

using namespace iso;

class THPFileHandler : FileHandler {
	const char*		GetExt() override { return "thp";			}
	const char*		GetDescription() override { return "Wii Movie";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} thp;

class THP_frames : public ISO::VirtualDefaults {
	wii::THPFileHeader	header;
    uint32				num_comps;
    uint32				frame_header_size;
	malloc_block		movie_data;
	uint32				index, offset, size;

	ISO_ptr<bitmap>	GetFrame(int i);
public:
	int					Count()							{ return header.header.numFrames;	}
	ISO::Browser2		Index(int i)					{ return GetFrame(i);				}

	bool				Init(istream_ref file);
	int					Width()							{ return header.videoInfo.xSize;	}
	int					Height()						{ return header.videoInfo.ySize;	}
	float				FrameRate()						{ return header.header.frameRate;	}
};
ISO_DEFVIRT(THP_frames);

bool THP_frames::Init(istream_ref file) {
	header			= file.get();
	if (memcmp(header.header.magic, "THP\0", 4) != 0)
		return false;

	index	= offset = 0;
	size	= header.header.firstFrameSize;

	num_comps	= header.frameCompInfo.numComponents;
    frame_header_size = sizeof(uint32) * num_comps + sizeof(uint32) * 2;

	file.seek(header.header.movieDataOffsets);
	movie_data.create(header.header.movieDataSize);
	file.readbuff(movie_data, header.header.movieDataSize);

	return true;
}

ISO_ptr<bitmap>	THP_frames::GetFrame(int i) {
	wii::THPFrameHeader	*frame = (wii::THPFrameHeader*)(movie_data + offset);
	while (i < index) {
		size	= frame->frameSizePrevious;
		offset	-= size;
		index--;
		frame = (wii::THPFrameHeader*)(movie_data + offset);
	}
	while (i > index) {
		offset	+= size;
		size	= frame->frameSizeNext;
		index++;
		frame	= (wii::THPFrameHeader*)(movie_data + offset);
	}

	uint8	*p = movie_data + offset + frame_header_size;
	for (int i = 0; i < num_comps; i++)	{
		uint32	comp_size = frame->comp[i];
		switch (header.frameCompInfo.frameComp[i]) {
			case wii::THP_VIDEO_COMP: {
				ISO_ptr<bitmap>	bm(0);
				bm->Create(header.videoInfo.xSize, header.videoInfo.ySize);
				memory_reader	file(memory_block(p, comp_size));
				int	length;
				for (JPG jpg;;) switch (jpg.GetMarker(file, length)) {
					case JPG::M_APP0:	jpg.GetAPP0(file, length, false); break;
					case JPG::M_SOI:	jpg.GetSOI(file); break;
					case JPG::M_EOI:	return bm;
					case JPG::M_SOS:	jpg.GetSOS(file, length); jpg.Decode(file, *bm); break;
					case JPG::M_SOF0:
					case JPG::M_SOF1:	jpg.GetSOF(file, length); break;
					case JPG::M_DHT:	jpg.GetDHT(file, length); break;
					case JPG::M_DQT:	jpg.GetDQT(file, length); break;
					case JPG::M_DRI:	jpg.GetDRI(file, length); break;
					case -1:
						return bm;
					default:
						file.seek_cur(length);
						break;
				}
			}
			case wii::THP_AUDIO_COMP:
				break;
			case wii::THP_NOCOMP_COMP:
				break;
			default:
				break;
		}
	}
	return ISO_NULL;
}

ISO_ptr<void> THPFileHandler::Read(tag id, istream_ref file) {
	ISO_ptr<THP_frames>	frames(NULL);
	if (frames->Init(file))
		return ISO_ptr<movie>(id, frames);
	return ISO_NULL;
}
