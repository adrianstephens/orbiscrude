#include "bitmapfile.h"

//-----------------------------------------------------------------------------
//	Autodesk Flic Animation
//-----------------------------------------------------------------------------

using namespace iso;

#pragma pack(1)
struct FLIheader {
	uint32	size;			// Length of file, for programs that want to read the FLI all at once if possible.
	uint16	magic;			// Set to hex AF11.  Please use another value here if you change format (even to a different resolution) so Autodesk Animator won't crash trying to read it.
	uint16	frames;			// Number of frames in FLI.  FLI files have a maxium length of 4000 frames.
	uint16	width;			// Screen width (320).
	uint16	height;			// Screen height (200).
	uint16	depth;			// Depth of a pixel (8).
	uint16	flags;			// Must be 0.
	uint16	speed;			// Number of video ticks between frames.
	uint32	next;			// Set to 0.
	uint32	frit;			// Set to 0.
	uint8	expand[102];	// All zeroes -- for future enhancement.
};

struct FLIframe {
	uint32	size;			// Bytes in this frame.  Autodesk Animator demands that this be less than 64K.
	uint16	magic;			// Always hexadecimal F1FA
	uint16	chunks;			// Number of 'chunks' in frame.
	uint8	expand[8];		// Space for future enhancements.  All zeros.
};

struct FLIchunk {
	uint32	size;			// Bytes in this chunk.
	uint16	type;			// Type of chunk (see below).
};

enum FLItype {
	FLI_256_COLOR= 4,//
	FLI_DELTA	=  7,//

	FLI_COLOR	= 11,// Compressed color map
	FLI_LC		= 12,// Line compressed -- the most common type of compression for any but the first frame.  Describes the pixel difference from the previous frame.
	FLI_BLACK	= 13,// Set whole screen to color 0 (only occurs on the first frame).
	FLI_BRUN	= 15,// Bytewise run-length compression -- first frame only
	FLI_COPY	= 16,// Indicates uncompressed 64000 bytes soon to follow.  For those times when compression just doesn't work!
	FLI_MINI	= 18,// Miniature 64 x 32 version of the first frame in FLI_BRUN format
};
#pragma pack()


class FLICFileHandler : FileHandler {
	const char*		GetDescription() override { return "Autodesk Flic Animation";	}
protected:
	static ISO_ptr<void> Read2(tag id, istream_ref file, float timescale);
};

ISO_ptr<void> FLICFileHandler::Read2(tag id, istream_ref file, float timescale) {
	FLIheader	header	= file.get();
	float		delay;

	if (header.magic == 0xAF11)
		delay = header.speed / 70.f;
	else if (header.magic == 0xAF12)
		delay = header.speed / 1000.f;
	else
		return ISO_NULL;

	ISO_ptr<bitmap_anim>	anim(id, header.frames);

	for (int i = 0; i < header.frames; i++) {
		FLIframe	frame	= file.get();
		streamptr	end		= file.tell() + frame.size - sizeof(frame);

		while (frame.magic == 0xF100) {
			file.seek_cur(frame.size - sizeof(frame));
			file.read(frame);
		}
		if (frame.magic != 0xF1FA)
			return ISO_NULL;

		bitmap_frame	&frame2 = (*anim)[i];
		frame2.b	= delay;
		if (frame.chunks == 0) {
			frame2.a = (*anim)[i - 1].a;
			continue;
		}

		bitmap	*bm	= frame2.a.Create(0);
		bm->Create(header.width, header.height);
		bm->CreateClut(256);

		if (i > 0) {
			bitmap	*pbm	= (*anim)[i - 1].a;
			memcpy(bm->ScanLine(0), pbm->ScanLine(0), header.width * header.height * sizeof(ISO_rgba));
			memcpy(bm->Clut(), pbm->Clut(), 256 * sizeof(ISO_rgba));
		}

		for (int j = 0; j < frame.chunks; j++) {
			FLIchunk	chunk	= file.get();
			streamptr	end		= file.tell() + chunk.size - sizeof(chunk);

			switch (chunk.type) {
				case FLI_256_COLOR: {
					int	npackets	= file.get<uint16>();
					int	pal			= 0;
					int	count;
					for (int i = 0; i < npackets; i++) {
						pal		+= file.getc();
						count	= file.getc();
						if (count == 0)
							count = 256;
						while (count--) {
							int	r = file.getc(), g = file.getc(), b = file.getc();
							bm->Clut(pal++) = ISO_rgba(r, g, b, 255);
						}

					}
					break;
				};
				case FLI_COLOR: {
					int	npackets	= file.get<uint16>();
					int	pal			= 0;
					int	count;
					for (int i = 0; i < npackets; i++) {
						pal		+= file.getc();
						count	= file.getc();
						if (count == 0) count = 256;
						while (count--) {
							int	r = file.getc(), g = file.getc(), b = file.getc();
							bm->Clut(pal++) = ISO_rgba(r * 4, g * 4, b * 4, 255);
						}

					}
					break;
				};
				case FLI_DELTA: {
					int	nlines	= file.get<uint16>();
					int	y		= 0;
					while (nlines--) {
						int	npackets = (short)file.get<uint16>();
						if (npackets < 0) {
							y -= npackets;
							continue;
						}
						ISO_rgba	*dest = bm->ScanLine(y++);
						signed char	count;
						while (npackets--) {
							dest	+= file.getc();
							count	= file.getc();
							if (count < 0) {
								int	byte1 = file.getc();
								int	byte2 = file.getc();
								while (count++) {
									*dest++ = byte1;
									*dest++ = byte2;
								}
							} else {
								while (count--) {
									*dest++ = file.getc();
									*dest++ = file.getc();
								}
							}

						}
					}
					break;
				}

				case FLI_LC: {
					int	y = file.get<uint16>();
					int	n = file.get<uint16>();
					while (n--) {
						ISO_rgba	*dest = bm->ScanLine(y++);
						int		npackets = file.getc();
						signed char	count;
						while (npackets--) {
							dest	+= file.getc();
							count	= file.getc();
							if (count < 0) {
								int	byte = file.getc();
								while (count++)
									*dest++ = byte;
							} else {
								while (count--)
									*dest++ = file.getc();
							}

						}
					}
					break;
				}

				case FLI_BLACK: {
					ISO_rgba	*dest	= bm->ScanLine(0);
					for (int i = header.width * header.height; i--;)
						*dest++ = 0;
					break;
				}

				case FLI_BRUN: {
					for (int y = 0; y < header.height; y++) {
						ISO_rgba	*dest = bm->ScanLine(y);
						int			npackets = file.getc();
						int8		count;
						while (npackets--) {
							count	= file.getc();
							if (count >= 0) {
								int	byte = file.getc();
								while (count--)
									*dest++ = byte;
							} else {
								while (count++)
									*dest++ = file.getc();
							}

						}
					}
					break;
				}

				case FLI_COPY: {
					ISO_rgba	*dest	= bm->ScanLine(0);
					int			size	= header.width * header.height;
					uint8		*buffer	= new uint8[size], *srce = buffer;
					for (int i = size; i--;)
						*dest++ = *srce++;
					delete[] buffer;
					break;
				}
			}
			file.seek(end);
		}
		file.seek(end);
	}
	return anim;
}

#if 0
bool Write2(ISO_ptr<void> p, ostream_ref file, float timescale) {
	FLIheader	header;
	int			sof	= file.tell(), eof;

	memset(&header, 0, sizeof(header));
	header.magic	= 0xAF12;
	header.frames	= bm.NumFrames();
	header.width	= bm.Width();
	header.height	= bm.Height();
	header.depth	= 8;
	header.speed	= bm.Delay() * 70 / 1000;
	file.seek_cur(sizeof(header));

	for (LuxBitmap *bm = &bm, *prev = NULL; bm; prev = bm, bm = bm->Next()) {
		FLIframe	frame;
		FLIchunk	chunk;
		int			sof	= file.tell(), eof;

		memset(&frame, 0, sizeof(frame));
		frame.magic		= 0xF1FA;
		frame.chunks	= prev ? 1 : 2;
		file.seek_cur(sizeof(header));

		if (!prev) {
			int	npal	= bm.ClutSize();
			chunk.size	= sizeof(chunk) + 2 + 1 + 1 + npal * 3;
			chunk.type	= FLI_256_COLOR;
			file.Write(&chunk, sizeof(chunk));
			file.PutW(1);
			file.PutC(0);
			file.PutC(npal);
			for (int i = 0; i < npal; i++) {
				ISO_rgba	&c = bm.Clut(i);
				file.PutC(c.r);
				file.PutC(c.g);
				file.PutC(c.b);
			}

//				chunk.size	= sizeof(chunk) + 2 + 1 + 1 + npal * 3;
			chunk.type	= FLI_BRUN;
			Brun(bm);
		} else {
			chunk.type	= FLI_DELTA;
			Delta(bm, prev);
		}

		eof = file.tell();
		file.seek(sof);
		file.Write(&frame, sizeof(frame));
		file.seek(eof);

	}

	eof = file.tell();
	file.seek(sof);
	header.size = eof - sof;
	file.Write(&header, sizeof(header));
	file.seek(eof);
	return true;
}
#endif

class FLIFileHandler : FLICFileHandler {
	const char*		GetExt() override { return "fli";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return Read2(id, file, 70); }
} fli;

class FLCFileHandler : FLICFileHandler {
	const char*		GetExt() override { return "flc";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override { return Read2(id, file, 1000); }
} flc;
