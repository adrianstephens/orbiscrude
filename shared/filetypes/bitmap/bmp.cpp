#include "bitmapfile.h"
#include "base/bits.h"
#include "filetypes/riff.h"
#include "filetypes/code/pe.h"

//-----------------------------------------------------------------------------
//	Windows bitmaps
//-----------------------------------------------------------------------------

using namespace iso;

struct BmpFileHelper {
	struct BITMAPINFOHEADER : packed_types<littleendian_types> {
		uint32		biSize;
		uint32		biWidth;
		uint32		biHeight;
		uint16		biPlanes;
		uint16		biBitCount;
		uint32		biCompression;
		uint32		biSizeImage;
		uint32		biXPelsPerMeter;
		uint32		biYPelsPerMeter;
		uint32		biClrUsed;
		uint32		biClrImportant;

		BITMAPINFOHEADER()	{}
		BITMAPINFOHEADER(int width, int height, int bits, int clut = 0) {
			biSize			= sizeof(BITMAPINFOHEADER);
			biWidth			= width;
			biHeight		= height;
			biPlanes		= 1;
			biBitCount		= bits;
			biCompression	= 0;
			biSizeImage		= align(width * bits, 32) / 8 * height;
			biXPelsPerMeter	= 0;
			biYPelsPerMeter	= 0;
			biClrUsed		= clut;
			biClrImportant	= 0;
		}
	};

	struct BITMAPFILEHEADER : packed_types<littleendian_types> {
		uint16be	bfType;
		uint32		bfSize;
		uint16		bfReserved1;
		uint16		bfReserved2;
		uint32		bfOffBits;

		BITMAPFILEHEADER()	{}
		BITMAPFILEHEADER(int size, int clut = 0) {
			bfType			= 'BM';
			bfOffBits		= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + clut * 4;
			bfSize			= bfOffBits + size;
			bfReserved1		= 0;
			bfReserved2		= 0;
		}
	};

#ifndef BI_RGB
	enum {
		BI_RGB,
		BI_RLE8,
		BI_RLE4,
		BI_BITFIELDS,
		BI_JPEG,
		BI_PNG,
	};
#endif
	static bool				ReadBmp(bitmap &bm, BITMAPINFOHEADER &bi, istream_ref file, size_t offset);
	static bool				WriteBmp(bitmap &bm, BITMAPINFOHEADER &bi, ostream_ref file);
	static bool				ReadMask(bitmap &bm, istream_ref file);
};

bool BmpFileHelper::ReadMask(bitmap &bm, istream_ref file) {
	uint32	width	= bm.Width();
	int		scan	= ((width + 7) / 8 + 3) & ~3;
	malloc_block	srce(scan);
	for (int y = bm.Height(); y--;) {
		file.readbuff(srce, scan);

		ISO_rgba	*dest	= bm.ScanLine(y);
		uint8		*alpha	= srce;
		for (int x = 0; x < width; x += 8) {
			uint8	t	= *alpha++;
			for (int n = min(width - x, 8u); n--; dest++, t <<= 1)
				dest->a = t & 0x80 ? 0 : 255;
		}
	}
	return true;
}

bool BmpFileHelper::ReadBmp(bitmap &bm, BITMAPINFOHEADER &bi, istream_ref file, size_t offset) {
	uint32	width	= bm.Width(), height = bm.Height(), bitcount = bi.biBitCount;
	if (bitcount <= 8) {
		Texel<B8G8R8_8>	colours[256];
		int				n		= bi.biClrUsed ? int(bi.biClrUsed) : (1 << bitcount);
		file.readbuff(colours, sizeof(colours[0]) * n);
		rcopy(bm.CreateClut(n), colours);
	}

	if (offset)
		file.seek(offset);

	if (bi.biCompression == BI_RLE8) {
		int			x = 0, y = height - 1;
		ISO_rgba	*line = bm.ScanLine(y);
		while (y >= 0) {
			uint8	code0 = file.getc();
			uint8	code1 = file.getc();
			if (code0) {
				while (code0--)
					line[x++] = code1;
			} else if (code1 < 3) {
				switch (code1) {
					case 0: y--; x = 0;break;
					case 1: y = -1; break;
					case 2: x += file.getc(); y -= file.getc(); break;
				}
				line = bm.ScanLine(y);
			} else {
				int	odd = code1 & 1;
				while (code1--)
					line[x++] = file.getc();
				if (odd)
					file.getc();
			}
		}
		return true;
	}

	if (bi.biCompression == BI_RLE4) {
		int			x = 0, y = height - 1;
		ISO_rgba	*line = bm.ScanLine(y);
		while (y >= 0) {
			uint8	code0 = file.getc();
			uint8	code1 = file.getc();
			if (code0) {
				while (code0 > 1) {
					line[x++] = code1 >> 4;
					line[x++] = code1 & 15;
					code0 -= 2;
				}
				if (code0)
					line[x++] = code1 >> 4;
			} else if (code1 < 3) {
				switch (code1) {
					case 0: y--; x = 0;break;
					case 1: y = -1; break;
					case 2: x += file.getc(); y -= file.getc(); break;
				}
				line = bm.ScanLine(y);
			} else {
				int	odd = !((code1 - 1) & 2);
				while (code1 > 1) {
					uint8	b = file.getc();
					line[x++] = b >> 4;
					line[x++] = b & 15;
					code1 -= 2;
				}
				if (code1)
					line[x++] = file.getc() >> 4;
				if (odd)
					file.getc();
			}
		}
		return true;
	}

	int				scan	= (width * bitcount + 31) / 32 * 4;
	malloc_block	srce0(scan);
	uint8			*srce	= srce0;
	for (int y = height - 1; y >= 0; y--) {
		ISO_rgba	*dest = bm.ScanLine(y);
		file.readbuff(srce, scan);
		switch (bitcount) {
			case 1: {
				uint8	*alpha	= srce;
				for (int x = 0; x < width; x += 8) {
					uint8	t	= *alpha++;
					int		n	= width - x;
					if (n > 8)
						n = 8;
					while (n--) {
						*dest++ = (t&0x80) ? 1 : 0;
						t <<= 1;
					}
				}
				break;
			}
			case 2:
				for (int x = 0; x < width / 4; x++) {
					dest[4 * x + 0] = (srce[x] >> 6) & 3;
					dest[4 * x + 1] = (srce[x] >> 4) & 3;
					dest[4 * x + 2] = (srce[x] >> 2) & 3;
					dest[4 * x + 3] = (srce[x] >> 0) & 3;
				}
				break;
			case 4:
				for (int x = 0; x < width / 2; x++) {
					dest[2 * x + 0] = srce[x] >> 4;
					dest[2 * x + 1] = srce[x] & 0xf;
				}
				break;

			case 8:
				for (int x = 0; x < width; x++)
					dest[x] = srce[x];
				break;

			case 16:
				copy_n((const Texel<R5G5B5>*)srce, dest, width);
				break;

			case 24:
				copy_n((const Texel<B8G8R8>*)srce, dest, width);
				break;

			case 32:
				copy_n((const Texel<B8G8R8A8>*)srce, dest, width);
				break;
		}
	}
	return true;
}

bool BmpFileHelper::WriteBmp(bitmap &bm, BITMAPINFOHEADER &bi, ostream_ref file) {
	uint32	width		= bm.Width(), height = bm.Height(), bitcount = bi.biBitCount;
	int		scan		= ((width * bitcount + 7) / 8 + 3) & ~3;

	if (bi.biClrUsed) {
		Texel<B8G8R8_8>		colours[256];
		if (bm.IsPaletted()) {
			copy_n(bm.Clut(), colours, bi.biClrUsed);
		} else {
			for (int i = 0; i < bi.biClrUsed; i++)
				colours[i] = ISO_rgba(i);
		}
		file.writebuff(colours, sizeof(colours[0]) * bi.biClrUsed);
	}

	malloc_block	dest0(scan);
	char*	dest	= dest0;
	for (int y = height - 1; y >= 0; y--) {
		ISO_rgba	*srce = bm.ScanLine(y);
		switch (bitcount) {
			case 1: {
				char	*alpha	= dest;
				for (int x = 0; x < width; x += 8) {
					uint8	t	= 0;
					int		n	= width - x;
					if (n > 8)
						n = 8;
					while (n--)
						t = (t<<1) | srce++->r;
					*alpha++ = t;
				}
				break;
			}
			case 4:
				for (int x = 0; x < width / 2; x++)
					dest[x] = (srce[2 * x].r << 4) | srce[2 * x + 1].r;
				break;
			case 8:
				for (int x = 0; x < width; x++)
					dest[x] = srce[x].r;
				break;
			case 16:
				copy_n(srce, (Texel<R5G5B5>*)dest, width);
				break;

			case 24:
				copy_n(srce, (Texel<B8G8R8>*)dest, width);
				break;

			case 32:
				copy_n(srce, (Texel<B8G8R8A8>*)dest, width);
				break;
		}
		file.writebuff(dest, scan);
	}

	return true;
}

class BmpFileHandler : public BitmapFileHandler, BmpFileHelper {
	const char*		GetExt()				override { return "bmp"; }
	const char*		GetMIME()				override { return "image/bmp"; }
	int				Check(istream_ref file)	override { file.seek(0); return file.get<uint16be>() == 'BM' ? CHECK_POSSIBLE : CHECK_DEFINITE_NO; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
public:
	BmpFileHandler()		{ ISO::getdef<bitmap>(); }
} bmp;

ISO_ptr<void> BmpFileHandler::Read(tag id, istream_ref file) {
	BITMAPFILEHEADER	bfh	= file.get();
	if (bfh.bfType != 'BM')
		return ISO_NULL;

	BITMAPINFOHEADER	bi	= file.get();
	ISO_ptr<bitmap> bm(id);
	bm->Create(bi.biWidth, bi.biHeight);
	ReadBmp(*bm, bi, file, bfh.bfOffBits);
	return bm;
}

bool BmpFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(FileHandler::ExpandExternals(p), ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
	if (!bm)
		return false;

	bool	intensity	= bm->IsIntensity();
	bool	paletted	= /*intensity || */bm->IsPaletted();
	bool	alpha		= bm->HasAlpha();
	uint32	clutsize	= paletted ? (intensity ? 256 : bm->ClutSize()) : 0;
	int		width		= bm->Width(), height = bm->Height();
	int		bitcount	= paletted ? (clutsize <= 16 ? 4 : 8) : alpha ? 32 : 24;
	int		scan		= ((width * bitcount + 7) / 8 + 3) & ~3;

	BITMAPFILEHEADER	bfh(scan * height, clutsize);
	file.write(bfh);

	BITMAPINFOHEADER	bi;
	bi.biSize			= sizeof(BITMAPINFOHEADER);
	bi.biWidth			= width;
	bi.biHeight			= height;
	bi.biPlanes			= 1;
	bi.biBitCount		= bitcount;
	bi.biCompression	= 0;
	bi.biSizeImage		= scan * height;
	bi.biXPelsPerMeter	= 0;
	bi.biYPelsPerMeter	= 0;
	bi.biClrUsed		= clutsize;
	bi.biClrImportant	= 0;

	file.write(bi);

	return WriteBmp(*bm, bi, file);
};

struct IconCursorFileHelper : public BmpFileHelper {
	static ISO_ptr<void>	Read(tag id, istream_ref file);
	static bool				Write(ISO_ptr<void> p, ostream_ref file, int type);
	static bool				Write(bitmap *bm, pe::RESOURCE_ICONDIR::ENTRY &entry, ostream_ref file);
};

ISO_ptr<void> IconCursorFileHelper::Read(tag id, istream_ref file) {
	streamptr	start	= file.tell();
	pe::RESOURCE_ICONDIR		dir		= file.get();

	if (!dir.valid())
		return ISO_NULL;

	auto	*entries = new pe::RESOURCE_ICONDIR::ENTRY[dir.Count];
	file.readbuff(entries, dir.Count * sizeof(pe::RESOURCE_ICONDIR::ENTRY));

	ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > >	array(id, dir.Count);

	for (int i = 0; i < dir.Count; i++) {
		auto		&entry	= entries[i];

		file.seek(start + entry.ImageOffset);
		malloc_block		mb(file, size_t(entry.BytesInRes));
		memory_reader			mi(mb);

		if (*(uint32be*)mb == 0x89504e47) {
			if (FileHandler *png = FileHandler::Get("png"))
				(*array)[i] = png->Read(tag(), mi);
		} else {
			BITMAPINFOHEADER	bi		= mi.get();
			ISO_ptr<bitmap> &bm		= (*array)[i];
			bm.Create(tag2());
			bm->Create(entry.Width, entry.Height);

			ReadBmp(*bm, bi, mi, 0);

			if (bi.biBitCount < 32)
				ReadMask(*bm, mi);
		}
	}

	delete[] entries;

	if (dir.Count == 1)
		return (*array)[0];
	return array;
}

bool IconCursorFileHelper::Write(ISO_ptr<void> p, ostream_ref file, int type) {
	p = FileHandler::ExpandExternals(p);

	pe::RESOURCE_ICONDIR	dir;
	dir.Reserved		= 0;
	dir.Type			= type;

//	if (p.IsType<ISO_openarray<ISO_ptr<bitmap> > >()) {
	if (p.IsType<anything>(ISO::MATCH_MATCHNULLS)) {
		ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > >	bms(p);
		int		num			= bms->Count();
		auto	*entries	= new pe::RESOURCE_ICONDIR::ENTRY[num];

		dir.Count	= num;
		file.write(dir);
		file.seek_cur(num * sizeof(pe::RESOURCE_ICONDIR::ENTRY));

		for (int i = 0; i < num; i++) {
			Write((*bms)[i], entries[i], file);
			if (type == 2) {
				entries[i].Planes = 7;
				entries[i].BitCount = 0;
			}
		}

		file.seek(sizeof(dir));
		file.writebuff(entries, num * sizeof(pe::RESOURCE_ICONDIR::ENTRY));
		delete[] entries;
	} else {
		ISO_ptr<bitmap>	bm = ISO_conversion::convert<bitmap>(p, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
		if (!bm)
			return false;

		if (ISO::Browser b = ISO::root("variables")["sizes"]) {
			int		num			= b.Count();
			auto	*entries	= new pe::RESOURCE_ICONDIR::ENTRY[num];

			dir.Count	= num;
			file.write(dir);
			file.seek_cur(num * sizeof(pe::RESOURCE_ICONDIR::ENTRY));

			for (int i = 0; i < num; i++) {
				int	size	= b[i].GetInt();
				ISO_ptr<bitmap>	bm2(0);
				bm2->Create(size, size);
				resample_via<HDRpixel>(bm2->All(), bm->All());
				Write(bm2, entries[i], file);
			}
			file.seek(sizeof(dir));
			file.writebuff(entries, num * sizeof(pe::RESOURCE_ICONDIR::ENTRY));
			delete[] entries;
		} else {
			dir.Count	= 1;
			file.write(dir);
			file.seek_cur(sizeof(pe::RESOURCE_ICONDIR::ENTRY));

			pe::RESOURCE_ICONDIR::ENTRY		entry;
			Write(bm, entry, file);
			if (type == 2) {
				entry.Planes	= 5;
				entry.BitCount	= 0;
			}
			file.seek(sizeof(dir));
			file.write(entry);
		}
	}
	return true;
}

bool IconCursorFileHelper::Write(bitmap *bm, pe::RESOURCE_ICONDIR::ENTRY &entry, ostream_ref file) {
	clear(entry);

	int		width		= bm->Width();
	int		height		= bm->Height();
	entry.Width			= width;
	entry.Height		= height;
	entry.ImageOffset	= file.tell32();
#if 0
	if (FileHandler *png = FileHandler::Get("png")) {
		png->Write(bm, file);
		entry.dwBytesInRes	= file.tell() - entry.dwImageOffset;
		return true;
	}
#endif
	bool	intensity	= bm->IsIntensity();
	bool	paletted	= intensity || bm->IsPaletted();
	bool	alpha		= bm->HasAlpha();
	uint32	clutsize	= intensity ? 256 : paletted ? bm->ClutSize() : 0;
	int		bitcount	= paletted ? (clutsize <= 16 ? 4 : 8) : alpha ? 32 : 24;
	int		scan		= ((width * bitcount + 7) / 8 + 3) & ~3;
	int		imagesize	= (scan + (width + 7) / 8) * height;

	BITMAPINFOHEADER	bi;
	bi.biSize			= sizeof(BITMAPINFOHEADER);
	bi.biWidth			= width;
	bi.biHeight			= height * 2;
	bi.biPlanes			= 1;
	bi.biBitCount		= bitcount;
	bi.biCompression	= 0;
	bi.biSizeImage		= imagesize;
	bi.biXPelsPerMeter	= 0;
	bi.biYPelsPerMeter	= 0;
	bi.biClrUsed		= clutsize;
	bi.biClrImportant	= 0;
	file.write(bi);

	if (!WriteBmp(*bm, bi, file))
		return false;

	scan = ((width + 7) / 8 + 3) & ~3;
	malloc_block	dest(scan);
	for (int y = height; y--;) {
		ISO_rgba	*srce	= bm->ScanLine(y);
		uint8		*alpha	= dest;
		for (int x = 0; x < width; x += 8) {
			uint8	t	= 0;
			for (int n = min(width - x, 8); n--; srce++)
				t = (t<<1) | (srce->a == 0);
			*alpha++ = t;
		}
		file.writebuff(dest, scan);
	}

	entry.BytesInRes	= file.tell32() - entry.ImageOffset;
	return true;
}

class IconFileHandler : public FileHandler, IconCursorFileHelper {
	const char*		GetExt() override { return "ico"; }
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return IconCursorFileHelper::Write(p, file, 1);
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return IconCursorFileHelper::Read(id, file);
	}
} ico;

class CursorFileHandler : public FileHandler, IconCursorFileHelper {
	const char*		GetExt() override { return "cur"; }
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return IconCursorFileHelper::Write(p, file, 2);
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return IconCursorFileHelper::Read(id, file);
	}
} cur;

ISO_ptr<void> ReadBmp(tag id, const memory_block &mb, bool icon) {
	try {
		memory_reader			mi(mb);

		if (*(uint32be*)mb == 0x89504e47) {
			if (FileHandler *png = FileHandler::Get("png"))
				return png->Read(tag(), mi);
		}

		BmpFileHelper::BITMAPINFOHEADER	bi		= mi.get();
		ISO_ptr<bitmap>		bm(id);
		bm->Create(bi.biWidth, bi.biHeight >> int(icon));
		BmpFileHelper::ReadBmp(*bm, bi, mi, 0);
		if (bi.biBitCount < 32)
			BmpFileHelper::ReadMask(*bm, mi);

		return bm;

	} catch (...) {
		return ISO::MakePtr(none, mb);
	}
};

struct anih {
	enum {
		SequenceFlag	= 1 << 1, //File contains sequence data
		IconFlag		= 1 << 0, // bit 0  TRUE: Frames are icon or cursor data, FALSE: Frames are raw data
	};
	uint32	NumFrames;		// number of stored frames in this animation
	uint32	NumSteps;		// number of steps in this animation (may include duplicate frames, = NumFrames, if no 'seq '-chunk is present)
	uint32	Width;			// total width in pixels
	uint32	Height;			// total height in pixels
	uint32	BitCount;		// number of bits/pixel ColorDepth = 2BitCount
	uint32	NumPlanes;		// =1
	uint32	DisplayRate;	// default display rate in 1/60s (Rate = 60 / DisplayRate fps)
	uint32	Flags;			// currently only 2 bits are used   reserved  bits 31..2  unused =0
};

class AniCursorFileHandler : public FileHandler {
	const char*		GetExt() override { return "ani"; }
	const char*		GetDescription() override { return "Windows animated cursor";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		if (file.get<uint32>() != "RIFF"_u32)
			return CHECK_DEFINITE_NO;
		file.get<uint32>();
		return file.get<uint32>() == "ACON"_u32 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return false;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		RIFF_chunk	riff(file);
		if (riff.id != "RIFF"_u32 || file.get<uint32>() != "ACON"_u32)
			return ISO_NULL;

		ISO_ptr<bitmap_anim>	anim(id);
		anih					header;

		while (riff.remaining()) {
			RIFF_chunk	chunk(file);
			switch (chunk.id) {
				case "anih"_u32:
					file.read(header);
					break;

				case "seq "_u32:
					break;

				case "rate"_u32:
					break;

				case "LIST"_u32: {
					switch (file.get<uint32>()) {
						case "fram"_u32: {
							while (chunk.remaining()) {
								RIFF_chunk	subchunk(file);
								switch (subchunk.id) {
									case "icon"_u32: {
										//if (header.Flags & anih::IconFlag) {
											ISO_ptr<void> p = IconCursorFileHelper::Read(tag(), file);
											anim->Append(make_pair(p, 1 / 60.f));
										//}
										break;
									}
								}
							}
							break;
						}
						break;
					}
				}
			}
		}
		return anim;
	}
} ani;
