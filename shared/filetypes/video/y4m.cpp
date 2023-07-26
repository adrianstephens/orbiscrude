#include "movie.h"
#include "matroska.h"
#include "base/algorithm.h"
#include "iso/iso_files.h"
#include "thread.h"
#include "jobs.h"
#include "extra/text_stream.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Y4M
//-----------------------------------------------------------------------------

/*
All anti-aliasing filters in the following conversion functions are based on one of two window functions:

The 6-tap Lanczos window (for down-sampling and shifts):
   sinc(pi*t)*sinc(pi*t/3), |t|<3
   0,                       |t|>=3

The 4-tap Mitchell window (for up-sampling):
   7|t|^3 - 12|t|^2 + 16/3,               |t|<1
   -(7/3)|t|^3 + 12|t|^2 - 20|t| + 32/3,  |t|<2
   0,                                     |t|>=2

420jpeg chroma samples are sited like:
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|   BR  |       |   BR  |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|   BR  |       |   BR  |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |

420mpeg2 chroma samples are sited like:
	Y-------Y-------Y-------Y-------
	|       |       |       |
	BR      |       BR      |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	BR      |       BR      |
	|       |       |       |
	Y-------Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |

We use a resampling filter to shift the site locations one quarter pixel (at the chroma plane's resolution) to the right.
The 4:2:2 modes look exactly the same, except there are twice as many chroma lines, and they are vertically co-sited with the luma samples in both the mpeg2 and jpeg cases (thus requiring no vertical resampling).

420paldv chroma samples are sited like:
	YR------Y-------YR------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YB------Y-------YB------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YR------Y-------YR------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YB------Y-------YB------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |

We use a resampling filter to shift the site locations one quarter pixel (at the chroma plane's resolution) to the right.
Then we use another filter to move the C_r location down one quarter pixel, and the C_b location up one quarter pixel.

422jpeg chroma samples are sited like:
	Y---BR--Y-------Y---BR--Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	Y---BR--Y-------Y---BR--Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	Y---BR--Y-------Y---BR--Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	Y---BR--Y-------Y---BR--Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	
We use a resampling filter to decimate the chroma planes by two in the vertical direction.

422 chroma samples are sited like:
	YBR-----Y-------YBR-----Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YBR-----Y-------YBR-----Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YBR-----Y-------YBR-----Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YBR-----Y-------YBR-----Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	
We use a resampling filter to shift the original site locations one quarter  pixel (at the original chroma resolution) to the right.
Then we use a second resampling filter to decimate the chroma planes by two in the vertical direction.

411 chroma samples are sited like:
	YBR-----Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YBR-----Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YBR-----Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |
	YBR-----Y-------Y-------Y-------
	|       |       |       |
	|       |       |       |
	|       |       |       |

We use a filter to resample at site locations one eighth pixel (at the source chroma plane's horizontal resolution) and five eighths of a pixel to the right.
Then we use another filter to decimate the planes by 2 in the vertical direction.
*/

class Y4M_frames : public ISO::VirtualDefaults {
	istream_ptr				file;
	int						width, height;
	uint16					aspect_w, aspect_h;
	rational<int>			fps;
	streamptr				first_frame;
	int						num_frames;
	int						last_frame;
	vbitmap_format			bitdepths;
	planar_format			planes;
	malloc_block			framedata;
	ISO_ptr<vbitmap_yuv>	bm;

	bool	ReadFrameHeader() {
		string			s;
		make_text_reader(file).read_line(s);
		string_scan		ss(s);
		return ss.get_token() == "FRAME";
	}

	bool	InitFormat(string_param chroma_type);
	
public:
	Y4M_frames(istream_ref file) : file(file.clone()), num_frames(0), last_frame(-1), bm("Y4M") {}
	bool				Init();
	int					Count()		const	{ return num_frames; }
	ISO::Browser2		Index(int i);
	int					Width()		const	{ return width; }
	int					Height()	const	{ return height; }
	float				FrameRate()	const	{ return fps; }
};

bool Y4M_frames::InitFormat(string_param chroma_type) {
	bitdepths		= {8, 8, 8, 0};
	planes.planes[0]	= {};

	if (chroma_type == "" || chroma_type == "420" || chroma_type == "420jpeg") {
		planes.planes[1] = planes.planes[2] = {1,1};

	} else if (chroma_type == "420p10") {
		bitdepths		= {10, 10, 10, 0};
		planes.planes[1]	= planes.planes[2] = {1,1};

	} else if (chroma_type == "420p12") {
		bitdepths		= {12, 12, 12, 0};
		planes.planes[1]	= planes.planes[2] = {1,1};

	} else if (chroma_type == "420mpeg2") {
		planes.planes[1]	= planes.planes[2] = {1,1, 1,0};

	} else if (chroma_type == "420paldv") {
		planes.planes[1] = {1, 1, -1,-1};
		planes.planes[2] = {1, 1, -1,+1};

	} else if (chroma_type == "422jpeg") {
		planes.planes[1] = planes.planes[2] = {1,0};

	} else if (chroma_type == "422") {
		planes.planes[1] = planes.planes[2] = {1,0};

	} else if (chroma_type == "422p10") {
		bitdepths		= {10, 10, 10, 0};
		planes.planes[1] = planes.planes[2] = {1,0};

	} else if (chroma_type == "422p12") {
		bitdepths		= {12, 12, 12, 0};
		planes.planes[1] = planes.planes[2] = {1,0};

	} else if (chroma_type == "411") {
		planes.planes[1] = planes.planes[2] = {8, 2,0};

	} else if (chroma_type == "444") {
		planes.planes[1] = planes.planes[2] = {};

	} else if (chroma_type == "444p10") {
		bitdepths		= {10, 10, 10, 0};

	} else if (chroma_type == "444p12") {
		bitdepths		= {12, 12, 12, 0};

	} else if (chroma_type == "444alpha") {
		bitdepths		= {8, 8, 8, 8};

	} else if (chroma_type == "mono") {
		bitdepths		= {8, 0, 0, 0};

	} else if (chroma_type == "440") {
		planes.planes[1] = planes.planes[2] = {0,1};

	} else {
		return false;
	}
	return true;
}

bool Y4M_frames::Init() {
	string			s;
	make_text_reader(file).read_line(s);

	string_scan		ss(s);
	if (ss.get_token() != "YUV4MPEG2")
		return false;

	fixed_string<16>	chroma_type;

	while (char c = ss.skip_whitespace().getc()) {
		switch (c) {
			case 'W':	ss >> width;						break;
			case 'H':	ss >> height;						break;
			case 'F':	ss >> fps.n >> ':' >> fps.d;		break;
			case 'I':	planes.interlace = ss.getc();		break;
			case 'A':	ss >> aspect_w >> ':' >> aspect_h;	break;
			case 'C':	chroma_type = ss.get_token();		break;
			case 'X':	ss.get_token();						break;
			default:	return false;
		}
	}
	InitFormat(chroma_type);

	framedata.create(planes.size(bitdepths, width, height));

	first_frame = file.tell();
	num_frames	= (file.length() - first_frame) / (framedata.size() + 6);
	return true;
}

ISO::Browser2 Y4M_frames::Index(int i) {
	if (i == last_frame)
		return bm;

	if (i < last_frame) {
		file.seek(first_frame);
		last_frame = -1;
	}
	
	while (last_frame < i - 1) {
		if (!ReadFrameHeader())
			return ISO_NULL;
		file.seek_cur(framedata.size());
		++last_frame;
	}

	if (!ReadFrameHeader())
		return ISO_NULL;

	framedata.read(file);

	bm->width			= width;
	bm->height			= height;
	bm->format			= bitdepths;
	bm->planes			= planes;

	uint8	*p	= framedata;
	for (int i = 0; i < 4; i++) {
		bm->texels[i]	= p;
		bm->strides[i]	= planes.planes[i].size(width, 1, bitdepths.bitdepth(i));
		p	+= planes.planes[i].size(width, height, bitdepths.bitdepth(i));
	}

	++last_frame;
	return bm;
}

ISO_DEFVIRT(Y4M_frames);

class Y4MFileHandler : public FileHandler {
	const char*		GetExt()				override { return "Y4M"; }
	const char*		GetDescription()		override { return "YUV4MPEG2"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		return memcmp(file.get<array<char, 10> >(), "YUV4MPEG2 ", 10) == 0 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<Y4M_frames>	frames(0, file);
		if (frames->Init())
			return ISO_ptr<movie>(id, frames);
		return ISO_NULL;
	}
} Y4M;

