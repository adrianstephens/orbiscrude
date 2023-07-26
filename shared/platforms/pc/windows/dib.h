#ifndef DIB_H
#define DIB_H

#include "filetypes\bitmap\bitmap.h"
#include "window.h"

namespace iso { namespace win {

//-----------------------------------------------------------------------------
//	DIB
//-----------------------------------------------------------------------------
struct DIBHEADER : public BITMAPINFOHEADER {
	typedef PreMultipliedTexel<TexelFormat<32, 16,8, 8,8, 0,8, 24,8> >	RGBQUAD;
	typedef Texel<TexelFormat<24, 16,8, 8,8, 0,8, 0,0>	>				RGB;

	uint32		ScanWidth()		const			{ return ((biWidth * biBitCount * biPlanes + 31) >> 3) & ~3; }
	int			Width()			const			{ return biWidth; }
	int			Height()		const			{ return abs(biHeight); }
	Point		Size()			const			{ return Point(Width(), Height()); }
	int			Bits()			const			{ return biBitCount; }
	int			Planes()		const			{ return biPlanes; }

	static size_t	CalcDataSize(int width, int height, int bits, int planes = 1, int clut = 0) {
		return sizeof(RGBQUAD) * clut + (((width * bits * planes + 31) >> 3) & ~3) * height;
	}

	DIBHEADER&	operator=(const BITMAPINFOHEADER &bmi) {
		memcpy(this, &bmi, bmi.biSize);
		return *this;
	}

	DIBHEADER()	{}
	DIBHEADER(int width, int height, int bits, int planes = 1, int clut = 0)	{ Init(width, height, bits, planes, clut); }
	DIBHEADER(const POINT &size, int bits, int planes = 1, int clut = 0)		{ Init(size.x, size.y, bits, planes, clut); }
	void						Init(int width, int height, int bits, int planes = 1, int clut = 0) {
		biSize			= sizeof(BITMAPINFOHEADER);
		biWidth			= width;
		biHeight		= height;
		biPlanes		= planes;
		biBitCount		= bits;
		biCompression	= 0;
		biSizeImage		= ScanWidth() * abs(height);
		biXPelsPerMeter	= 1;
		biYPelsPerMeter	= 1;
		biClrUsed		= clut;
		biClrImportant	= 0;
	}
	void						InitV5(int width, int height, int bits, int planes = 1, int clut = 0) {
		BITMAPV5HEADER	*p = (BITMAPV5HEADER*)this;
		clear(*p);
		Init(width, height, bits, planes, clut);
		biSize				= sizeof(*p);
		p->bV5Compression	= BI_BITFIELDS;
		p->bV5RedMask		= 0x00FF0000;
		p->bV5GreenMask		= 0x0000FF00;
		p->bV5BlueMask		= 0x000000FF;
		p->bV5AlphaMask		= 0xFF000000;
	}

	iso::ISO_ptr<iso::bitmap>	CreateBitmap(iso::tag2 id, void *data) const;
};

class DIB : public DIBHEADER {
	DIB*		Init(int width, int height, int bits, int planes, int clut) {
	//	if (bits == 32)
	//		DIBHEADER::InitV5(width, height, bits, planes, clut);
	//	else
			DIBHEADER::Init(width, height, bits, planes, clut);
		return this;
	}
	DIB*	CopyBits(const block<ISO_rgba, 2> &texels, const block<ISO_rgba, 1> &newclut) {
		if (newclut) {
			copy_n(newclut.begin(), Clut(), newclut.size());
			return SetPixels<Texel<R8>>(texels);

		}
		return SetPixels(texels);
	}
	template<int B> DIB*	CopyBits(const _bitmap<B> &bm) {
		return CopyBits(bm.All(), bm.ClutBlock());
	}
public:
	static size_t	CalcSize(int width, int height, int bits, int planes = 1, int clut = 0) {
		return sizeof(BITMAPV5HEADER) + CalcDataSize(width, height, bits, planes, clut);
	}
	static DIB*		Init(void *p, int width, int height, int bits, int planes = 1, int clut = 0) {
		return ((DIB*)p)->Init(width, -height, bits, planes, clut);
	}
	static DIB*		Init(void *p, const block<ISO_rgba, 2> &texels, const block<ISO_rgba, 1> &clut) {
		return Init(p, texels.size<1>(), texels.size<2>(), 32, 1, clut.size())->CopyBits(texels, clut);
	}
	template<int B> static DIB*		Init(void *p, const iso::_bitmap<B> &bm) {
		return Init(p, bm.All(), bm.ClutBlock());
	}

	static DIB*		Create(int width, int height, int bits, int planes = 1, int clut = 0) {
		return ((DIB*)malloc(CalcSize(width, height, bits, planes, clut)))->Init(width, -height, bits, planes, clut);
	}
	static DIB*		Create(const block<ISO_rgba, 2> &texels, const block<ISO_rgba, 1> &clut) {
		return Create(texels.size<1>(), texels.size<2>(), clut ? 8 : 32, 1, clut.size())->CopyBits(texels, clut);
	}
	template<int B> static DIB*		Create(const iso::_bitmap<B> &bm) {
		return Create(bm.All(), bm.ClutBlock());
	}

	static DIB*		Create(HDC hdc, HBITMAP hbm, int width, int height, int bits, int planes = 1, int clut = 0) {
		DIB	*dib = Create(width, height, bits, planes, clut);
		save(dib->biSize), GetDIBits(hdc, hbm, 0, height, dib->Data(), (BITMAPINFO*)dib, DIB_RGB_COLORS);
		return dib;
	}
	static DIB*		Create(HDC hdc, HBITMAP hbm) {
		Bitmap::Params	p = Bitmap(hbm).GetParams();
		return Create(hdc, hbm, p.Size().x, p.Size().y, p.BitsPerPixel(), p.Planes());
	}
	Bitmap						CreateDIBSection(HDC hdc)	const {
		void	*bits;
		Bitmap	bm	= Bitmap::CreateDIBSection(*this, &bits);
		if (bm && bits)
			memcpy(bits, Data(), Height() * ScanWidth());
		return bm;
	}

	uint8*		Data()			const			{ return (uint8*)this + biSize + biClrUsed * sizeof(RGBQUAD); }
	size_t		DataSize()		const			{ return ScanWidth() * Height(); }
	uint8*		ScanLine(int i)					{ return Data() + i * ScanWidth(); }
	RGBQUAD*	Clut()							{ return (RGBQUAD*)((uint8*)this + biSize); }
	RGBQUAD&	Clut(int i)						{ return Clut()[i]; }

	bool		Update(const iso::bitmap &bm)	{ return bm.Width() == Width() && bm.Height() == Height() && CopyBits(bm);	}
	bool		Update(const iso::bitmap64 &bm)	{ return bm.Width() == Width() && bm.Height() == Height() && CopyBits(bm);	}
//	void		Destroy()						{ free(this);	}

	iso::ISO_ptr<iso::bitmap>	CreateBitmap(iso::tag2 id)	const { return DIBHEADER::CreateBitmap(id, Data()); }

	template<typename T> block<T, 2> GetPixels() {
		return make_strided_block((T*)Data(), Width(), ScanWidth(), Height());
	}
	block<RGBQUAD, 2> GetPixels() {
		return GetPixels<RGBQUAD>();
	}
	template<typename P, typename T> DIB *SetPixels(const block<T, 2> &srce) {
		copy(srce, GetPixels<P>());
		return this;
	}
	template<typename T> DIB *SetPixels(const block<T, 2> &srce) {
		return SetPixels<RGBQUAD>(srce);
	}

	int Stretch(HDC hdc, const Rect &rect, DWORD rop = SRCCOPY) const {
		return StretchDIBits(hdc,
			rect.left, rect.top,
			rect.Width(), rect.Height(),
			0, 0, Width(), Height(),
			this + 1, (BITMAPINFO*)this,
			DIB_RGB_COLORS,	rop);
	}

	int Stretch(HDC hdc, int x, int y, int w, int h, DWORD rop = SRCCOPY) const	{
		return StretchDIBits(hdc,
			x, y,
			w, h,
			0, 0, Width(), Height(),
			this + 1, (BITMAPINFO*)this,
			DIB_RGB_COLORS,	rop);
	}

	int Stretch(HDC hdc, int xd, int yd, int wd, int hd, int xs, int ys, int ws, int hs, DWORD rop = SRCCOPY) const	{
		return StretchDIBits(hdc,
			xd, yd, wd, hd,
			xs, ys, ws, hs,
			Data(), (BITMAPINFO*)this,
			DIB_RGB_COLORS,	rop);
	}
};

template<typename T> block<T, 2> MakeDIBBlock(T *data, const Point &size) {
	return make_strided_block(data, size.x, (size.x * sizeof32(T) + 3) & ~3, size.y);
}

Cursor	MakeCursor(HBITMAP hb, const POINT &hotspot, bool shadow = true);
Cursor	MakeCursor(HCURSOR hc, bool shadow = true);
Cursor	CompositeCursor(HCURSOR hc, HBITMAP hb, const POINT offset, bool shadow = true);
Cursor	CompositeCursor(HCURSOR hc1, HCURSOR hc2, bool shadow = true);
Cursor	TextCursor(HCURSOR hc, HFONT hf, const char *text, bool shadow = true);

} } //namespace iso::win

#endif //DIB_H
