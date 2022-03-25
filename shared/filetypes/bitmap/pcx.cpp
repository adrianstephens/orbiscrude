#include "bitmapfile.h"
#include "utilities.h"

//-----------------------------------------------------------------------------
//	ZSoft PC Paintbrush
//-----------------------------------------------------------------------------

using namespace iso;

class PCXFileHandler : BitmapFileHandler {
	const char*		GetExt() override { return "pcx";	}
	const char*		GetDescription() override { return "ZSoft PC Paintbrush";	}

	struct HEADER {
		uint8	identifier;
		uint8	version;
		uint8	encoding;
		uint8	bitsperpixel;
		uint16	xstart;
		uint16	ystart;
		uint16	xend;
		uint16	yend;
		uint16	hrez;
		uint16	vrez;
		uint8	egapal[48];
		uint8	reserved1;
		uint8	numbitplanes;
		uint16	bytesperline;
		uint16	palettetype;
		uint16	screenwidth;
		uint16	screenheight;
		uint8	reserved2[54];

		void	set(int width, int height, int nplanes) {
			clear(*this);
			identifier		= 10;
			version			= 5;
			encoding		= 1;
			bitsperpixel	= 8;
			xend			= width - 1;
			yend			= height - 1;
			numbitplanes	= nplanes;
			bytesperline	= width;
			palettetype		= 1;
			screenwidth		= 640;
			screenheight	= 480;
		}

		bool	valid() const {
			if (identifier != 10 || version > 5 || encoding != 1 || (bitsperpixel & (bitsperpixel >> 1)) || bitsperpixel > 8)
				return false;
			for (auto& i : reserved2) {
				if (i)
					return false;
			}
			return true;
		}

	};

	static void ReadLine(istream_ref file, const memory_block &mem);
	static void WriteLine(ostream_ref file, const const_memory_block &mem);

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		HEADER	header	= file.get();

		if (!header.valid())
			return ISO_NULL;

		ISO_ptr<bitmap>	bm(id);
		bm->Create(header.xend - header.xstart + 1, header.yend - header.ystart + 1);

		{
			//temp_array<malloc_block>	buffers(header.numbitplanes, header.bytesperline);
			temp_array<malloc_block>	buffers(header.numbitplanes);
			for (auto &i : buffers)
				i.resize(header.bytesperline);

			for (int y = 0; y < bm->Height(); y++) {
				for (auto &i : buffers)
					ReadLine(file, i);

				switch (header.bitsperpixel) {
					case 1:
						for (int x = 0; x < bm->Width(); x++) {
						}
						break;
					case 2:
					case 8:
						for (int i = 0; i < header.numbitplanes; i++) {
							uint8*	dest = (uint8*)bm->ScanLine(y) + i;
							uint8*	srce = buffers[i];
							for (int x = 0; x < bm->Width(); x++, dest += 4, ++srce)
								*dest = *srce;
						}
						break;
				}
			}
		}

		int		n = 1 << (header.numbitplanes * header.bitsperpixel);

		if (n <= 256) {
			bm->CreateClut(n);

			if (header.version >= 5 && file.getc() == 0xc) {
				for (int i = 0; i < n; i++) {
					int	r = file.getc(), g = file.getc(), b = file.getc();
					bm->Clut(i) = ISO_rgba(r,g,b,255);
				}
			} else {
				for (int i = 0; i < n; i++)
					bm->Clut(i) = ISO_rgba(header.egapal[i*3+0],header.egapal[i*3+1],header.egapal[i*3+2],255);
			}
		}

		return bm;
	}

	bool Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(FileHandler::ExpandExternals(p), ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
		if (!bm)
			return false;

		HEADER	header;
		header.set(bm->Width(), bm->Height(), bm->IsPaletted() || bm->IsIntensity() ? 1 : 3);
		file.write(header);

		for (int y = 0; y < bm->Height(); y++) {
			for (int i = 0; i < header.numbitplanes; i++)
				WriteLine(file, const_memory_block((uint8*)bm->ScanLine(y) + i, header.bytesperline));
		}

		if (bm->IsPaletted()) {
			file.putc(0xc);
			for (int i = 0; i < 256; i++) {
				ISO_rgba	c = i < bm->ClutSize() ? ISO_rgba(0,0,0,0) : bm->Clut(i);
				file.putc(c.r);
				file.putc(c.g);
				file.putc(c.b);
			}
		}

		return true;
	}
} pcx;

void PCXFileHandler::ReadLine(istream_ref file, const memory_block &mem) {
	uint8* p = mem;
	for (size_t count = mem.length(); count;) {
		int	b = file.getc();
		if (b >= 0xc0) {
			int	v	= file.getc();
			b		&= 0x3f;
			count	-= b;
			while (b--)
				*p++ = v;
		} else {
			*p++ = b;
			count--;
		}
	}
}

void PCXFileHandler::WriteLine(ostream_ref file, const const_memory_block &mem) {
	const uint8* p = mem;
	int	b = *p, b2;
	int	c = 0;
	for (size_t count = mem.length() + 1; count--;) {
		if (count && (b2 = *p) == b && c < 63)
			c++;
		else {
			if (c == 1 && b < 0xc0)
				file.putc(b);
			else {
				file.putc(c | 0xc0);
				file.putc(b);
			}
			b = b2;
			c = 1;
		}
		p += 4;
	}
}
