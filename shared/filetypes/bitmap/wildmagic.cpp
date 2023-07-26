#include "bitmapfile.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	WildMagic image file
//-----------------------------------------------------------------------------

class IMFileHandler : public FileHandler {

	typedef Texel<TexelFormat<16, 0,5, 5,5, 10,5> >			rgb5;
	typedef Texel<TexelFormat<24, 0,8, 8,8, 16,8> >			rgb8;

	enum {
		WM5_char	= 0,
		WM5_uchar	= 1,
		WM5_short	= 2,
		WM5_ushort	= 3,
		WM5_int		= 4,
		WM5_uint	= 5,
		WM5_long	= 6,
		WM5_ulong	= 7,
		WM5_float	= 8,
		WM5_double	= 9,
		WM5_rgb5	= 10,
		WM5_rgb8	= 11,
	};

	struct header {
		fixed_string<12>	uniqueIdentifier;		//= "Magic Image"; // null-terminated string (12 bytes)
		int					numberOfDimensions;		//= 2; // (4 bytes)
		int					xDimension;				// number of image columns (4 bytes)
		int					yDimension;				// number of image rows (4 bytes)
		int					rtti;					// type of pixels, see WildMagic5/LibImagics/Images/Wm5Element.cpp, (4 bytes)
		int					size;					// bytes per pixel [size of the pixel type], (4 bytes)
//		char*				data;					// the image data in row-major order (row0 pixels, row1 pixels, ...)
		bool	check() const { return uniqueIdentifier == "Magic Image"; }
	};
	template<typename T>	static void ReadImage2D(istream_ref file, bitmap *p) {
		ISO_rgba	*d = p->ScanLine(0);
		for (int y = 0, h = p->Height(); y < h; y++) {
			*d = file.get<T>();
		}
	}

	const char*		GetExt() override { return "im"; }
	const char*		GetDescription() override { return "WildMagic image file"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		header	h;
		return file.read(h) && h.check() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		header			h	= file.get();
		if (!h.check())
			return ISO_NULL;

		ISO_ptr<bitmap>	p(id);
		p->Create(h.xDimension, h.yDimension);
		switch (h.rtti) {
			case WM5_char:		ReadImage2D<int8>(file, p); break;
			case WM5_uchar:		ReadImage2D<uint8>(file, p); break;
			case WM5_short:		ReadImage2D<int16>(file, p); break;
			case WM5_ushort:	ReadImage2D<uint16>(file, p); break;
			case WM5_int:		ReadImage2D<int32>(file, p); break;
			case WM5_uint:		ReadImage2D<uint32>(file, p); break;
			case WM5_long:		ReadImage2D<int64>(file, p); break;
			case WM5_ulong:		ReadImage2D<uint64>(file, p); break;
			case WM5_float:		ReadImage2D<float>(file, p); break;
			case WM5_double:	ReadImage2D<double>(file, p); break;
			case WM5_rgb5:		ReadImage2D<rgb5>(file, p); break;
			case WM5_rgb8:		ReadImage2D<rgb8>(file, p); break;
		}
		return p;
	}
} im;
