#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

//-----------------------------------------------------------------------------
//	ILM OpenEXR
//-----------------------------------------------------------------------------

#undef _INC_MATH
#undef _CSTDLIB_

#include "ImfArray.h"
#include "ImfRgbaFile.h"
#include "ImfArray.h"
#include "ImfIO.h"

using namespace Imf;
using namespace Imath;
using namespace iso;

class EXRFileHandler : FileHandler {
	const char	*GetExt()			{ return "exr";			}
	const char	*GetDescription()	{ return "ILM OpenEXR";	}


	class ISO_IStream: public Imf::IStream {
		istream_ref	file;
	public:
		ISO_IStream(istream_ref _file) : IStream("unknown"), file(_file) {}
		bool	read(char c[], int n) override { return n == file.readbuff(c, n);	}
		Int64	tellg() override { return file.tell();	}
		void	seekg(Int64 pos) override { file.seek(pos);		}
		void	clear() override {}
	};

	class ISO_OStream: public Imf::OStream {
		ostream_ref	file;
	public:
		ISO_OStream(ostream_ref _file) : OStream("unknown"), file(_file) {}
		void	write(const char c[], int n) override { file.writebuff(c, n);	}
		Int64	tellp() override { return file.tell();	}
		void	seekp(Int64 pos) override { file.seek(pos);		}
		virtual void	clear() {}
	};

	static HDRpixel	MakeFrgba(const Imf::Rgba &in) {
		return HDRpixel(in.r, in.g, in.b, in.a);
	}

	ISO_ptr<void>	Read(tag id, istream_ref file)	{
		ISO_ptr<HDRbitmap> bm(id);
		ISO_IStream		tfile(file);
		RgbaInputFile	ifile(tfile);
		Array2D<Rgba>	pixels;
		Box2i			dw		= ifile.dataWindow();
		int				width	= dw.max.x - dw.min.x + 1;
		int				height	= dw.max.y - dw.min.y + 1;

		pixels.resizeErase(height, width);
		ifile.setFrameBuffer(&pixels[0][0] - dw.min.x - dw.min.y * width, 1, width);
		ifile.readPixels(dw.min.y, dw.max.y);

		bm->Create(width, height);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				Imf::Rgba &in		= pixels[y][x];
				bm->ScanLine(y)[x]	= HDRpixel(in.r, in.g, in.b, in.a);
			}
		}

		return bm;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) {
		ISO_ptr<HDRbitmap> bm = ISO_conversion::convert<HDRbitmap>(p);
		if (!bm)
			return false;

		int				width	= bm->Width();
		int				height	= bm->Height();
		Header			header(width, height);
		ISO_OStream		tfile(file);
		RgbaOutputFile	ofile(tfile, header);
		Array2D<Rgba>	pixels;

		pixels.resizeErase(height, width);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				HDRpixel	c = bm->ScanLine(y)[x] / 255;
				pixels[y][x] = Rgba(float(c.r), float(c.g), float(c.b), float(c.a));
			}
		}

		ofile.setFrameBuffer(&pixels[0][0], 1, width);
		ofile.writePixels(height);

		return true;
	}
} exr;
