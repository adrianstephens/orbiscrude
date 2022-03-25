#include "movie.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "com.h"
#include <vfw.h>

using namespace iso;

#define MAX_AUDIO_STREAMS 16
#define MAX_VIDEO_STREAMS 16

enum {
	DIBC	= mmioFOURCC('D','I','B',' '),
	MRLE	= mmioFOURCC('m','r','l','e'),
	DIVX	= mmioFOURCC('d','i','v','x'),
	MP42	= mmioFOURCC('M','P','4','2'),
	XVID	= mmioFOURCC('X','V','I','D'),
//	DIVX	= mmioFOURCC('D','I','V','X'),
	DX50	= mmioFOURCC('D','X','5','0'),
	YUYV	= mmioFOURCC('Y','U','Y','V'),
	YUY2	= mmioFOURCC('Y','U','Y','2'),
	YVYU	= mmioFOURCC('Y','V','Y','U'),
	UYVY	= mmioFOURCC('U','Y','V','Y'),
	I420	= mmioFOURCC('I','4','2','0'),
	IYUV	= mmioFOURCC('I','Y','U','V'),
	YV12	= mmioFOURCC('Y','V','1','2'),
};

int EnumerateCompressors(uint32 type) {
	int	n = 0;
	ICINFO	icinfo;
	for (int i = 0; ICInfo(type, i, &icinfo); i++) {
		if (HIC hic = ICOpen(icinfo.fccType, icinfo.fccHandler, ICMODE_QUERY)) {
			// Skip this compressor if it can't handle the format.
//			if (fccType == ICTYPE_VIDEO && pvIn != NULL && ICDecompressQuery(hic, pvIn, NULL) != ICERR_OK) {
//				ICClose(hic);
//				continue;
//			}

			// Find out the compressor name.
			ICGetInfo(hic, &icinfo, sizeof(icinfo));
			ISO_TRACEF() << (char(&)[5])icinfo.fccHandler << " : " << icinfo.szDescription << '\n';
			ICClose(hic);
			++n;
		}
	}
	return n;
}

class AviFileHandler : public FileHandler {
	const char*		GetExt() override { return "avi";	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override;
public:
	AviFileHandler()		{ ISO::getdef<movie>(); }
} avi;

class AVI_frames : public ISO::VirtualDefaults {
	IAVIFile			*avifile;
	IAVIStream			*audio[MAX_AUDIO_STREAMS], *video[MAX_VIDEO_STREAMS];
	int					num_audio, num_video;
	IGetFrame			*gf;
	AVISTREAMINFO		si;

//	ISO_ptr<bitmap>	prev_bitmap;	// keeps reference count for a bit
	ISO_ptr<bitmap>		GetFrame(int i);
public:
	int					Count()							{ return AVIStreamLength(video[0]);	}
	ISO::Browser2		Index(int i)					{ return GetFrame(i);				}

	bool				Init(const filename &fn);
	int					Width()							{ return si.rcFrame.right  - si.rcFrame.left;	}
	int					Height()						{ return si.rcFrame.bottom - si.rcFrame.top;	}
	float				FrameRate()						{ return float(si.dwRate) / float(si.dwScale);	}
};

ISO_DEFVIRT(AVI_frames);

bool AVI_frames::Init(const filename &fn) {
	avifile = NULL;
	gf		= NULL;
	AVIFileInit();
	if (AVIFileOpenA(&avifile, fn, OF_READ, NULL) == 0) {
		num_audio = num_video = 0;
		while (avifile->GetStream(&audio[num_audio], streamtypeAUDIO, num_audio) == 0 && ++num_audio < MAX_AUDIO_STREAMS);
		while (avifile->GetStream(&video[num_video], streamtypeVIDEO, num_video) == 0 && ++num_video < MAX_VIDEO_STREAMS);
		if (num_video) {
			AVIStreamInfo(video[0], &si, sizeof(si));
			return true;
		}
	}
	return false;
}

ISO_ptr<bitmap> AVI_frames::GetFrame(int i) {
	ISO_ptr<bitmap>	p;
	if (!gf) {
		BITMAPINFOHEADER	bih;
		clear(bih);
		bih.biSize			= sizeof(BITMAPINFOHEADER);
		bih.biWidth			= Width();
		bih.biHeight		= Height();
		bih.biPlanes		= 1;
		bih.biBitCount		= 24;
		bih.biCompression	= BI_RGB;
		if (!(gf = AVIStreamGetFrameOpen(video[0], &bih)))
			return p;
	}

	if (BITMAPINFOHEADER *bi = (BITMAPINFOHEADER*)gf->GetFrame(i)) {
		int		width	= bi->biWidth, height = bi->biHeight, bitcount = bi->biBitCount;
		p.Create()->Create(width, height);

		int		scan	= (width * bitcount + 31) / 32 * 4;
		char	*srce	= (char*)bi + bi->biSize + bi->biClrUsed * sizeof(RGBQUAD);
		for (int y = height - 1; y >= 0; y--, srce += scan)
			copy_n((Texel<B8G8R8>*)srce, p->ScanLine(y), width);

//		prev_bitmap	= p;
	}
	return p;
}


ISO_ptr<void> AviFileHandler::ReadWithFilename(tag id, const filename &fn) {
	ISO_ptr<AVI_frames>	frames(NULL);
	if (frames->Init(fn))
		return ISO_ptr<movie>(id, frames);
	return ISO_NULL;
}

class AVI_writer {
	com_ptr<IAVIFile>		avifile;
	com_ptr<IAVIStream>		video_raw, video_comp;
	AVISTREAMINFO	si;
public:
	AVI_writer() {
		clear(si);
		si.fccType	= streamtypeVIDEO;// stream type
		si.dwScale	= 100;
		si.dwRate	= 2997;
	}

	bool	Init(const filename &fn) {
		init_com();
		return AVIFileOpenA(&avifile, fn, OF_CREATE|OF_WRITE, NULL) == 0;
	}
	void	SetFrameRate(float f)	{ si.dwRate = f * si.dwScale; }
	bool	PutFrame(bitmap &bm, int i);
};

bool AVI_writer::PutFrame(bitmap &bm, int i) {
	int			width		= bm.Width();
	int			height		= bm.Height();
	int			scan		= (sizeof(RGBTRIPLE) * width + 1) & ~1;
	int			npix		= width * height;
	int			buff_size	= scan * height;
	malloc_block	buffer(buff_size + 3);

	for (int y = 0; y < height; y++)
		copy_n(bm.ScanLine(height - 1 - y), (Texel<B8G8R8>*)((char*)buffer + y * scan), width);

	if (!video_raw) {
		si.dwSuggestedBufferSize = buff_size;//npix * sizeof(RGBTRIPLE);
		si.rcFrame.right	= width;
		si.rcFrame.bottom	= height;

		AVIFileCreateStream(avifile, &video_raw, &si);

		// Create Compressed Stream

		BITMAPINFOHEADER	bi;
		clear(bi);
		bi.biSize			= sizeof(BITMAPINFOHEADER);
		bi.biWidth			= width;
		bi.biHeight			= height;
		bi.biPlanes			= 1;
		bi.biBitCount		= 24;
		bi.biSizeImage		= buff_size;//npix * sizeof(RGBTRIPLE);

		AVICOMPRESSOPTIONS	opts;
		clear(opts);
		opts.fccHandler		= DIBC;

		AVIMakeCompressedStream(&video_comp, video_raw, &opts, NULL);
		AVIStreamSetFormat(video_comp, 0, &bi, bi.biSize);
	}

	return AVIStreamWrite(video_comp, i, 1, buffer, buff_size, AVIIF_KEYFRAME, NULL, NULL) == 0;
}

bool AviFileHandler::WriteWithFilename(ISO_ptr<void> p, const filename &fn) {
	if (ISO_ptr<movie> m = ISO_conversion::convert<movie>(p)) {
		AVI_writer	avi;
		if (avi.Init(fn)) {
			avi.SetFrameRate(m->fps);
			ISO::Browser	b(m->frames);
			for (int i = 0, n = b.Count(); i < n; i++)
				avi.PutFrame(*(bitmap*)b[i], i);
			return true;
		}
	}
	return false;
}
