#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "filetypes/iff.h"
#include "base/bits.h"

//-----------------------------------------------------------------------------
//	DeluxePaint Interleaved Bitmap
//-----------------------------------------------------------------------------

using namespace iso;

class ILBMFileHandler : public FileHandler {

	struct BMHD : bigendian_types {
		uint16	width;
		uint16	height;
		uint16	xpos;
		uint16	ypos;
		uint8	nplanes;
		uint8	masking;
		uint8	comp;
		uint8	pad1;
		uint16	transcolour;
		uint8	xaspect;
		uint8	yaspect;
		uint16	pagewidth;
		uint16	pageheight;
	};

	void	GetRow(istream_ref file, char *dest, int length, bool comp);
	void	PutRow(ostream_ref file, char *srce, int length, bool comp);

	const char*		GetExt() override { return "lbm";	}
	int				Check(istream_ref file) override { file.seek(0); return IFF_chunk(file).id == "FORM"_u32 ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} ilbm;

class BBMFileHandler : public ILBMFileHandler {
	const char*		GetExt() override { return "bbm";	}
} bbm;

void ILBMFileHandler::GetRow(istream_ref file, char *dest, int length, bool comp) {
	if (comp) {
		while (length > 0) {
			int	temp = file.getc();
			if (temp == EOF)
				return;
			if (temp > 0x80)
				memset(dest, file.getc(), temp = 0x101 - temp);
			else
				file.readbuff(dest, ++temp);
			dest	+= temp;
			length	-= temp;
		}
	} else {
		file.readbuff(dest, length);
	}
}

ISO_ptr<void> ILBMFileHandler::Read(tag id, istream_ref file) {
	IFF_chunk	iff(file);
	if (iff.id != "FORM"_u32)
		return ISO_NULL;

	bool		interleaved;
	bool		amiga	= false;
	bool		comp, alpha;
	int			width, height, nplanes;

	uint32		type	= file.get();
	if (type == "ILBM"_u32)
		interleaved = true;
	else if (type == "PBM "_u32)
		interleaved = false;
	else
		return ISO_NULL;

	ISO_ptr<bitmap> bm(id);

	while (iff.remaining()) {
		IFF_chunk	chunk(file);
		switch (chunk.id) {
			case "BMHD"_u32: {
				BMHD		bmhd	= file.get();
				width	= bmhd.width;
				height	= bmhd.height;
				nplanes	= bmhd.nplanes;
				comp	= bmhd.comp		!= 0;
				alpha	= bmhd.masking	!= 0;
				bm->Create(width, height);
				break;
			}

			case "CMAP"_u32: {
				Texel<R8G8B8>	colours[256];
				uint32			n = chunk.remaining() / sizeof(colours[0]);
				file.readbuff(colours, sizeof(colours[0]) * n);
				copy_n(colours, bm->CreateClut(n).begin(), n);
				break;
			}

			case "CAMG"_u32: {
				amiga = true;
				break;
			}

			case "BODY"_u32: {
				for (int y = 0; y < height; y++) {
					ISO_rgba	*dest = bm->ScanLine(y);
					if (interleaved) {
						int		iwidth		= ((width + 15) >> 4) * 2;
						int		n			= nplanes + int(alpha);
						malloc_block	temp_buffer(iwidth);
						malloc_block	unpack_buffer(iwidth * 8 * 4);
						unpack_buffer.fill(0);

						for (int i = 0; i < n; i++) {
							GetRow(file, temp_buffer, iwidth, comp);
							int		mask	= 1 << i;
							int		*p		= unpack_buffer;
							char	*temp	= temp_buffer;
							for (int j = 0; j < iwidth; j++) {
								for (int k = 0, b = temp[j]; k < 8; k++, p++, b <<= 1)
									if (b & 0x80)
										*p |= mask;
							}
						}

						int	*p		= unpack_buffer;
						if (nplanes < 8) {
							for (int x = 0; x < width; x++)
								dest[x] = ISO_rgba(p[x]);
						} else {
							for (int x = 0; x < width; x++)
								dest[x] = ISO_rgba(p[x] & 0xff, (p[x]>>8) & 0xff, (p[x]>>16) & 0xff, alpha && !(p[x]>>24) ? 0 : 255);
						}

					} else {
						malloc_block	buffer(width);
						char	*tempbuff	= buffer;
						GetRow(file, tempbuff, width, comp);
						for (int x = 0; x < width; x++)
							dest[x] = ISO_rgba(tempbuff[x]);
					}
				}

				if (amiga) {
					if (nplanes == 6) {
						ISO_rgba	c(0,0,0,0);
						for (int y = 0; y < height; y++) {
							ISO_rgba	*p = bm->ScanLine(y);
							for (int x = 0; x < width; x++, p++) {
								int	i	= p->r;
								switch (i >> 4) {
									case 0: c.r = bm->Clut(i & 0xf).r;
											c.g = bm->Clut(i & 0xf).g;
											c.b = bm->Clut(i & 0xf).b;
											break;
									case 1: c.b = (i << 4) & 0xf0; break;
									case 2: c.r = (i << 4) & 0xf0; break;
									case 3: c.g = (i << 4) & 0xf0; break;
								}
								*p = c;
							}
						}
						bm->CreateClut(0);
					} else if (nplanes == 8) {
						ISO_rgba	c(0,0,0,0);
						for (int y = 0; y < height; y++) {
							ISO_rgba	*p = bm->ScanLine(y);
							for (int x = 0; x < width; x++, p++) {
								int	i	= p->r;
								switch (i >> 6) {
									case 0: c.r = bm->Clut(i & 0x3f).r;
											c.g = bm->Clut(i & 0x3f).g;
											c.b = bm->Clut(i & 0x3f).b;
											break;
									case 1: c.b = (i << 2) & 0xfc; break;
									case 2: c.r = (i << 2) & 0xfc; break;
									case 3: c.g = (i << 2) & 0xfc; break;
								}
								*p = c;
							}
						}
						bm->CreateClut(0);
					}
				}
				break;
			}
		}
	}
	return bm;
}

void ILBMFileHandler::PutRow(ostream_ref file, char *srce, int length, bool comp) {
	if (comp) {
		char	p, c;
		int		s, i, run;

		i = 0;
		c = srce[i];

		do {
			s = i;
			do {
				p	= c;
				run	= 1;
				while (++i < length && (c = srce[i]) == p && run < 0x81)
					run++;
			} while (i < length && run <= 2);

			if (run <= 2)
				run = 0;

			while (i - run > s) {
				int	t = i - run - s;
				if (t > 0x80)
					t = 0x80;
				file.putc(t - 1);
				file.writebuff(srce + s, t);
				s = s + t;
			}

			if (run) {
				file.putc(0x101 - run);
				file.putc(p);
			}
		} while (i < length);

	} else {
		file.writebuff(srce, length);
	}
}

bool ILBMFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
	if (!bm)
		return false;

	bool	interleaved = false;//(flags & BMWF_TWIDDLE) || !bm.Paletted() || bm.ClutSize() <= 16;
	bool	amiga		= false;//!bm.Paletted() && (flags & BMWF_USERFLAG);
	bool	comp		= false;//true;
	bool	alpha		= bm->HasAlpha();
	int		nplanes		= amiga ? 6 : interleaved ? (bm->IsPaletted() ? iso::log2(bm->ClutSize()) : 24) : 8;
	int		width		= bm->Width();
	int		height		= bm->Height();

	IFF_Wchunk	iff(file, "FORM"_u32);
	iff.write(interleaved ? "ILBM"_u32 : "PBM "_u32);

	BMHD	bmhd;
	bmhd.width		= width;
	bmhd.height		= height;
	bmhd.xpos		= 0;
	bmhd.ypos		= 0;
	bmhd.nplanes	= nplanes;
	bmhd.masking	= alpha;
	bmhd.comp		= comp;
	bmhd.pad1		= 0;
	bmhd.transcolour= 0;
	bmhd.xaspect	= 1;
	bmhd.yaspect	= 1;
	bmhd.pagewidth	= width;
	bmhd.pageheight	= height;
	IFF_Wchunk(file, "BMHD"_u32).write(bmhd);

	if (bm->IsPaletted()) {
		Texel<R8G8B8>	colours[257];
		copy_n(bm->Clut(), colours, bm->ClutSize());
		IFF_Wchunk(file, "CMAP"_u32).writebuff(colours, sizeof(colours[0]) * bm->ClutSize());
	}

	if (amiga) {
		IFF_Wchunk	camg(file, "CAMG"_u32);
		if (nplanes == 6) {
			camg.putc(0);
			camg.putc(0);
			camg.putc(8);
			camg.putc(4);
		} else {
			camg.putc(0);
			camg.putc(1);
			camg.putc(9);
			camg.putc(8);
		}
	}

	IFF_Wchunk	body(file, "BODY"_u32);
	for (int y = 0; y < height; y++) {
		ISO_rgba	*srce = bm->ScanLine(y);
		if (interleaved) {
			int		iwidth		= ((width + 15) >> 4) * 2;
			int		n			= nplanes + int(alpha);
			malloc_block	temp_buffer(iwidth);
			malloc_block	unpack_buffer(iwidth * 8 * 4);
			unpack_buffer.fill(0);

			int	*p		= unpack_buffer;
			for (int i = 0; i < width; i++)
				p[i] = n < 8 ? srce[i].r : (srce[i].r | (srce[i].g<<8) | (srce[i].b<<16) | (srce[i].a ? 1<<24 : 0));

			for (int i = 0; i < n; i++) {
				int		mask	= 1 << i;
				int		*p		= unpack_buffer;
				char	*temp	= temp_buffer;
				for (int j = 0; j < iwidth; j++) {
					int	b = 0;
					for (int k = 0; k < 8; k++)
						b = (b << 1) | !!(*p++ & mask);
					temp[j] = b;
				}
				PutRow(file, temp_buffer, iwidth, comp);
			}

		} else {
			malloc_block	temp_buffer(width);
			char	*temp	= temp_buffer;
			for (int x = 0; x < width; x++)
				temp[x] = srce[x].r;
			PutRow(file, temp_buffer, width, comp);
		}
	}
	return true;
}
