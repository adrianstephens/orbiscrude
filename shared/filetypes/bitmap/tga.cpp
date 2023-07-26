#include "bitmapfile.h"

//-----------------------------------------------------------------------------
//	TARGA bitmaps
//-----------------------------------------------------------------------------

using namespace iso;

class TgaFileHandler : public BitmapFileHandler {
	#pragma pack(1)

	struct TARGA_HEADER : littleendian_types {
		enum {
			NOIMAGE	= 0,
			CI		= 1,
			TRUECOL	= 2,
			MONO	= 3,

			TYPE	= 7,
			RL		= 8,	// rl compressed
			HDR16	= 0x80,
		};

		uint8		IDLength;
		uint8		ColorMapType;
		uint8		ImageType;
		uint16		CMapStart;
		uint16		CMapLength;
		uint8		CMapDepth;
		uint16		XOffset;
		uint16		YOffset;
		uint16		Width;
		uint16		Height;
		uint8		PixelDepth;
		uint8		ImageDescriptor;
		bool	valid() const {
			return between(ImageType & 7, CI, MONO) && !(ImageType & 0x70)
			&& CMapLength	<= 256
			&& CMapStart	<= CMapLength
			&& PixelDepth	<= (ImageType & HDR16 ? 64 : 32)
			&& CMapDepth	<= 32
			&& Width		!= 0
			&& Height		!= 0;
		}
	};

	#pragma pack()

	typedef TexelFormat<32, 16,8, 8,8, 0,8, 24,8>	TARGA_RGBA32;
	typedef TexelFormat<24, 16,8, 8,8, 0,8, 0,0>	TARGA_RGB24;
	typedef TexelFormat<16, 10,5, 5,5, 0,5, 15,1>	TARGA_RGBA16;
	typedef TexelFormat<16, 11,5, 5,6, 0,5, 0,0>	TARGA_RGB16;
	typedef TexelFormat<8,  0,8,  0,8, 0,8, 0,0>	TARGA_I8;
	typedef TexelFormat<16, 0,8,  0,8, 0,8, 8,8>	TARGA_IA16;

	typedef field_vec<float16, float16, float16>			TARGA_RGB48;
	typedef field_vec<float16, float16, float16, float16>	TARGA_RGBA64;
	typedef float16											TARGA_I16;
	typedef field_vec<float16, float16>						TARGA_IA32;

	static void	Decode(uint8 *buffer, int bpp, int width, istream_ref file);
	static void	Encode(uint8 *buffer, int bpp, int width, ostream_ref file);
public:
	const char*		GetExt() override { return "tga";	}
	const char*		GetDescription() override { return "Targa Image";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		TARGA_HEADER	th;
		return file.read(th) && th.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} tga;

ISO_ptr<void> TgaFileHandler::Read(tag id, istream_ref file) {
	TARGA_HEADER	th;
	if (!file.read(th) || !th.valid())
		return ISO_NULL;

	int		type	= th.ImageType & TARGA_HEADER::TYPE;
	int		bpp		= (th.PixelDepth + 7) / 8;
	int		scan	= bpp * th.Width;
	malloc_block	buffer;

	if (th.ImageType & TARGA_HEADER::HDR16) {
		ISO_ptr<HDRbitmap> bm(id);
		bm->Create(th.Width, th.Height);
		buffer.create(scan);

		for (int y = 0; y < th.Height; y++) {
			auto	*dest	= bm->ScanLine(th.ImageDescriptor & 32 ? y : th.Height - y - 1);
			uint16	*srce	= buffer;
			file.readbuff(srce, scan);
			switch (type) {
				case TARGA_HEADER::TRUECOL:
					switch (th.PixelDepth) {
						case 48: copy_n((TARGA_RGB48*)srce, dest, th.Width); break;
						case 64: copy_n((TARGA_RGBA64*)srce, dest, th.Width); break;
					}
					break;

				case TARGA_HEADER::MONO:
					switch (th.PixelDepth) {
						case 16: copy_n((TARGA_I16*)srce, dest, th.Width); break;
						case 32: copy_n((TARGA_IA32*)srce, dest, th.Width); break;
					}
					break;
			}
		}
		return bm;
	}

	ISO_ptr<bitmap> bm(id);
	bm->Create(th.Width, th.Height);

	file.seek_cur(th.IDLength);

	if (th.ColorMapType) {
		int				pallen	= th.CMapLength * ((th.CMapDepth + 7) / 8);
		malloc_block	pal(pallen);
		file.readbuff(pal, pallen);

		if (type == TARGA_HEADER::CI) {
			ISO_rgba	*c = bm->CreateClut(th.CMapLength).begin();
			switch (th.CMapDepth) {
				case 16: copy_n((Texel<TARGA_RGB16>*)pal, c, th.CMapLength); break;
				case 24: copy_n((Texel<TARGA_RGB24>*)pal, c, th.CMapLength); break;
				default: copy_n((Texel<TARGA_RGBA32>*)pal, c, th.CMapLength); /*SetFlag(BMF_CLUTALPHA);*/; break;
			}
			for (int i = 0; i < th.CMapLength / 2; i++)
				swap(c[i], c[th.CMapLength - 1 - i]);
		}
	}

/*	if (type == TARGA_HEADER::MONO)
		SetFlag(BMF_ALPHA | BMF_INTENSITY);

	if (type == TARGA_HEADER::TRUECOL) {
		if (th.PixelDepth == 32)
			SetFlag(BMF_ALPHA);
		else if (th.PixelDepth == 16)
			SetFlag(BMF_ALPHA | BMF_ONEBITALPHA);
	}
*/

	bool	rl		= !!(th.ImageType & TARGA_HEADER::RL);
	if (rl) {
		buffer.create(scan * th.Height);
		Decode(buffer, bpp, th.Width * th.Height, file);
	} else {
		buffer.create(scan);
	}

	for (int y = 0; y < th.Height; y++) {
		ISO_rgba	*dest = bm->ScanLine(th.ImageDescriptor & 32 ? y : th.Height - y - 1);
		uint8		*srce	= buffer;
		if (rl) {
			srce += scan * y;
		} else {
			file.readbuff(srce, scan);
		}
		switch (type) {
			case TARGA_HEADER::CI:
				for (int x = 0; x < th.Width; x++)
					dest[x] = th.CMapLength - 1 - srce[x];
				break;

			case TARGA_HEADER::TRUECOL:
				switch (th.PixelDepth) {
					case 16: copy_n((Texel<TARGA_RGBA16>*)srce, dest, th.Width); break;
					case 24: copy_n((Texel<TARGA_RGB24>*)srce, dest, th.Width); break;
					case 32: copy_n((Texel<TARGA_RGBA32>*)srce, dest, th.Width); break;
				}
				break;

			case TARGA_HEADER::MONO:
				switch (th.PixelDepth) {
					case 8:
						for (int x = 0; x < th.Width; x++)
							dest[x] = ISO_rgba(srce[x], 255);
						break;
					case 16:	copy_n((Texel<TARGA_IA16>*)srce, dest, th.Width); break;
				}
				break;
		}
	}
	SetBitmapFlags(bm.get());
	return bm;
}

void TgaFileHandler::Decode(uint8 *buffer, int bpp, int width, istream_ref file) {
	while (width > 0) {
		uint8	b = file.getc();
		width -= (b & 0x7f) + 1;
		if (b & 0x80) {
			file.readbuff(buffer, bpp);
			buffer += bpp;
			for (int n = bpp * (b & 0x7f); n--; buffer++)
				*buffer = buffer[-bpp];
//			memcpy(buffer + bpp, buffer, bpp * (b & 0x7f));
		} else {
			file.readbuff(buffer, bpp * (b + 1));
			buffer += bpp * (b + 1);
		}
	}
}

bool TgaFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
	if (!bm)
		return false;

	bool	compress= false;
	uint32	clut	= bm->ClutSize();
	int		type	= bm->IsIntensity() ? TARGA_HEADER::MONO : clut ? TARGA_HEADER::CI : TARGA_HEADER::TRUECOL;

	TARGA_HEADER	th;
	th.IDLength		= 0;
	th.ColorMapType	= clut;
	th.ImageType	= type | (compress ? 8 : 0);
	th.CMapStart	= 0;
	th.CMapLength	= clut ? bm->ClutSize() : 0;
	th.CMapDepth	= clut ? 32 : 0;
	th.XOffset		= 0;
	th.YOffset		= 0;
	th.Width		= bm->BaseWidth();
	th.Height		= bm->Height();
	th.PixelDepth	= bm->IsIntensity() || clut ? 8 : bm->HasAlpha() ? 32 : 24;
	th.ImageDescriptor = 32;

	file.write(th);

	if (clut) {
		Texel<TARGA_RGBA32>	*c = new Texel<TARGA_RGBA32>[clut];
		copy_n(bm->Clut(), c, clut);
//		for (int i = 0; i < clut / 2; i++)
//			swap(c[i], c[clut - 1 - i]);
		file.writebuff(c, sizeof(*c) * clut);
		delete[] c;
	}

	int		bpp		= (th.PixelDepth + 7) / 8;
	int		scan	= bpp * th.Width;
	malloc_block	dest(scan + 3);
	for (int y = 0; y < th.Height; y++) {
		const ISO_rgba	*srce = bm->ScanLine(y);
		switch (type) {
			case TARGA_HEADER::CI:
			case TARGA_HEADER::MONO:
				for (int x = 0; x < th.Width; x++)
					((char*)dest)[x] = srce[x].r;
				break;

			case TARGA_HEADER::TRUECOL:
				switch (th.PixelDepth) {
					case 16: copy_n(srce, (Texel<TARGA_RGBA16>*)dest, th.Width); break;
					case 24: copy_n(srce, (Texel<TARGA_RGB24>*)dest, th.Width); break;
					case 32: copy_n(srce, (Texel<TARGA_RGBA32>*)dest, th.Width); break;
				}
				break;

		}
//		if (compress)
//			Encode(dest, bpp, th.Width, file);
//		else
			file.writebuff(dest, scan);
	}
	return true;
}
