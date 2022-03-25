#include "iso/iso_files.h"
#include "movie.h"
#include "comms/HTTP.h"
#include "xvidcore/src/xvid.h"
#include "filetypes/riff.h"

#pragma comment(lib, "libxvidcore")

using namespace iso;

class XVID_frames : public ISO::VirtualDefaults {
	istream_ptr					file;
	dynamic_array<RIFF_chunk>	riff_stack;

	xvid_gbl_init_t		xvid_gbl_init;
	xvid_dec_create_t	xvid_dec_create;
	xvid_dec_frame_t	xvid_dec_frame;
	xvid_dec_stats_t	xvid_dec_stats;

	uint8				*in_buffer;
	uint8				*out_buffer;
	uint8				*in;
	void				*dec_handle;
	int					width, height, index, remain;

	ISO_ptr<bitmap>		bm;
	ISO_ptr<bitmap>		GetFrame(int i);
public:
	XVID_frames(istream_ptr &&file);
	~XVID_frames();
	bool				Valid()			{ return index >= 0;	}
	int					Count()			{ return 100;			}
	ISO::Browser2		Index(int i)	{ return GetFrame(i);	}

	int					Width()			{ return width;			}
	int					Height()		{ return height;		}
	float				FrameRate()		{ return 30.f;			}
};

ISO_DEFVIRT(XVID_frames);

class XVIDFileHandler : public FileHandler {
	const char*		GetExt() override { return "avi";	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<XVID_frames>	frames(
			0,
			fn.find(':') > fn + 1 ? istream_ptr(new HTTPinput(HTTPopenURL("isopod", fn))) : istream_ptr(new FileInput(fn))
		);
		if (frames->Valid()) {
			ISO_ptr<movie>	movie(id);
			movie->frames	= frames;
			movie->width	= frames->Width();
			movie->height	= frames->Height();
			movie->fps		= frames->FrameRate();
			return movie;
		}
		return ISO_NULL;
	}
} xvid;


ISO_ptr<bitmap>	XVID_frames::GetFrame(int i) {
	if (i == index)
		return bm;

	for (;;) {
		if (riff_stack.back().length() == 0)
			riff_stack.pop_back();

		new (riff_stack) RIFF_chunk(file);

		if (riff_stack.back().id == "RIFF"_u32) {
			uint32	type = file.get();	// check for "AVIX"_u32?
			continue;
		}
		if (riff_stack.back().id == "LIST"_u32) {
			uint32	type = file.get();	// check for 'rec '?
			continue;
		}
		if (riff_stack.back().id != "00dc"_u32) {
			riff_stack.pop_back();
			continue;
		}

		uint32	length	= riff_stack.back().length();
		remain += length;
		file.readbuff(in, length);
		riff_stack.pop_back();

		while (remain > 16) {
			xvid_dec_frame.bitstream		= in;
			xvid_dec_frame.length			= remain;

			int	used = xvid_decore(dec_handle, XVID_DEC_DECODE, &xvid_dec_frame, &xvid_dec_stats);

			if (xvid_dec_stats.type == XVID_TYPE_VOL) {
				if (width != xvid_dec_stats.data.vol.width || height != xvid_dec_stats.data.vol.height) {
					width		= xvid_dec_stats.data.vol.width;
					height		= xvid_dec_stats.data.vol.height;
					free(out_buffer);
					out_buffer	= (uint8*)malloc(width * height * 3);
					xvid_dec_frame.output.plane[0]	= out_buffer;
					xvid_dec_frame.output.stride[0]	= width * 3;
				}
			}
			in		+= used;
			remain	-= used;

			if (out_buffer) {
				index = i;
				bm.Create()->Create(width, height);
				for (int y = 0; y < height; y++)
					copy_n((Texel<B8G8R8>*)(out_buffer + xvid_dec_frame.output.stride[0] * y), bm->ScanLine(y), width);
			}
		}
		if (remain < 0)
			remain = 0;
		if (remain)
			memcpy(in_buffer, in, remain);

		in = in_buffer + remain;
		if (i == index)
			return bm;
	}
	return ISO_NULL;
}

XVID_frames::XVID_frames(istream_ptr &&_file) : file(move(_file)),
	in_buffer((uint8*)malloc(1024 * 1024)), out_buffer(0),
	width(0), height(0), remain(0), index(-1)
{
	in	= in_buffer;
	clear(xvid_gbl_init);
	clear(xvid_dec_create);
	clear(xvid_dec_frame);
	clear(xvid_dec_stats);

	int 	ret;

	xvid_gbl_init.version		= XVID_VERSION;
//	xvid_gbl_init.cpu_flags		= XVID_CPU_FORCE;
	ret			= xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL);

	xvid_dec_create.version		= XVID_VERSION;
	ret			= xvid_decore(NULL, XVID_DEC_CREATE, &xvid_dec_create, NULL);
	dec_handle	= xvid_dec_create.handle;

	xvid_dec_frame.version		= XVID_VERSION;
	xvid_dec_stats.version		= XVID_VERSION;
	xvid_dec_frame.output.csp	= XVID_CSP_BGR;

	new (riff_stack) RIFF_chunk(file);
	uint32	type	= file.get();
	while (riff_stack.back().length()) {
		new (riff_stack) RIFF_chunk(file);
		if (riff_stack.back().id == "LIST"_u32) {
			uint32	type = file.get();
			if (type == "movi"_u32) {
				GetFrame(0);
				return;
			}
		}
		riff_stack.pop_back();
	}
	riff_stack.pop_back();
}

XVID_frames::~XVID_frames() {
	xvid_decore(dec_handle, XVID_DEC_DESTROY, NULL, NULL);
	free(in_buffer);
	free(out_buffer);
}

//-----------------------------------------------------------------------------

class MP4_frames : public ISO::VirtualDefaults {
	istream_ptr			file;

	xvid_gbl_init_t		xvid_gbl_init;
	xvid_dec_create_t	xvid_dec_create;
	xvid_dec_frame_t	xvid_dec_frame;
	xvid_dec_stats_t	xvid_dec_stats;

	uint8				*in_buffer;
	uint8				*out_buffer;
	uint8				*in;
	void				*dec_handle;
	int					width, height, index, remain;

	ISO_ptr<bitmap>		bm;
	ISO_ptr<bitmap>		GetFrame(int i);
public:
	MP4_frames(istream_ptr &&file);
	~MP4_frames();
	bool				Valid()			{ return index >= 0;	}
	int					Count()			{ return 100;			}
	ISO::Browser2		Index(int i)	{ return GetFrame(i);	}

	int					Width()			{ return width;			}
	int					Height()		{ return height;		}
	float				FrameRate()		{ return 30.f;			}
};

template<> struct ISO_def<MP4_frames> : public ISO::VirtualT<MP4_frames> {};

class MP4FileHandler : public FileHandler {
	const char*		GetExt() override { return "mp4"; }

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<MP4_frames>	frames(
			0,
			fn.find(':') > fn + 1 ? istream_ptr(new HTTPinput(HTTPopenURL("isopod", fn))) : istream_ptr(new FileInput(fn))
		);
		if (frames->Valid()) {
			ISO_ptr<movie>	movie(id);
			movie->frames	= frames;
			movie->width	= frames->Width();
			movie->height	= frames->Height();
			movie->fps		= frames->FrameRate();
			return movie;
		}
		return ISO_NULL;
	}
} mp4x;

ISO_ptr<bitmap>	MP4_frames::GetFrame(int i){
	if (i == index)
		return bm;

	for (;;) {
		uint32	length = 1024 * 1024 - remain;
		remain += file.readbuff(in, length);

		while (remain > 16) {
			xvid_dec_frame.bitstream		= in;
			xvid_dec_frame.length			= remain;

			int	used = xvid_decore(dec_handle, XVID_DEC_DECODE, &xvid_dec_frame, &xvid_dec_stats);

			if (xvid_dec_stats.type == XVID_TYPE_VOL) {
				if (width != xvid_dec_stats.data.vol.width || height != xvid_dec_stats.data.vol.height) {
					width		= xvid_dec_stats.data.vol.width;
					height		= xvid_dec_stats.data.vol.height;
					free(out_buffer);
					out_buffer	= (uint8*)malloc(width * height * 3);
					xvid_dec_frame.output.plane[0]	= out_buffer;
					xvid_dec_frame.output.stride[0]	= width * 3;
				}
			}
			in		+= used;
			remain	-= used;

			if (out_buffer) {
				index = i;
				bm.Create()->Create(width, height);
				for (int y = 0; y < height; y++)
					copy_n((Texel<B8G8R8>*)(out_buffer + xvid_dec_frame.output.stride[0] * y), bm->ScanLine(y), width);
			}
		}

		if (remain)
			memcpy(in_buffer, in, remain);
		in = in_buffer + remain;
		if (i == index)
			return bm;
	}
	return ISO_NULL;
}

MP4_frames::MP4_frames(istream_ptr &&_file) : file(move(_file)),
	in_buffer((uint8*)malloc(1024 * 1024)), out_buffer(0),
	width(0), height(0), remain(0), index(-1)
{
	in	= in_buffer;
	clear(xvid_gbl_init);
	clear(xvid_dec_create);
	clear(xvid_dec_frame);
	clear(xvid_dec_stats);

	int 	ret;

	xvid_gbl_init.version		= XVID_VERSION;
//	xvid_gbl_init.cpu_flags		= XVID_CPU_FORCE;
	ret			= xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL);

	xvid_dec_create.version		= XVID_VERSION;
	ret			= xvid_decore(NULL, XVID_DEC_CREATE, &xvid_dec_create, NULL);
	dec_handle	= xvid_dec_create.handle;

	xvid_dec_frame.version		= XVID_VERSION;
	xvid_dec_stats.version		= XVID_VERSION;
	xvid_dec_frame.output.csp	= XVID_CSP_BGR;

	GetFrame(0);
}

MP4_frames::~MP4_frames() {
	xvid_decore(dec_handle, XVID_DEC_DESTROY, NULL, NULL);
	free(in_buffer);
	free(out_buffer);
}
