#include "platformdata.h"
#include "scenegraph.h"
#include "2d/fonts.h"
#include "systems/ui/font_iso.h"
#include "iso/iso_files.h"
#include "allocators/allocator2D.h"
#include "maths/geometry_iso.h"
#include "extra/indexer.h"

#ifdef PLAT_MAC
#include <CoreText/CTFont.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

using namespace iso;

//-----------------------------------------------------------------------------
//	Conversions
//-----------------------------------------------------------------------------

#ifdef PLAT_PC

MAT2	mat_identity = {{0,1},{0,0},{0,0},{0,1}};

struct PCGlyph : GLYPHMETRICS {
	ABC		abc;
	malloc_block	buffer;
	PCGlyph(struct PCFont *font, int i, int bits);
	operator	bool()		const	{ return !!buffer;		}
	uint32		width()		const	{ return gmBlackBoxX;	}
	uint32		stride()	const	{ return (gmBlackBoxX + 3) & ~3;	}
	uint32		height()	const	{ return buffer.size32() / stride(); }

	int			yoffset()	const	{ return gmptGlyphOrigin.y; }// + height() - gmBlackBoxY; }
	int			xoffset()	const	{ return gmptGlyphOrigin.x; }

	float		kern()		const	{ return abc.abcA; }
	float		advance()	const	{ return abc.abcA + abc.abcB + abc.abcC; }

	block<uint8, 2>	block()	const	{ return make_strided_block((uint8*)buffer, width(), stride(), height()); }
};

struct PCFont {
	HDC				hdc;
	HFONT			hFont;
	TEXTMETRIC		tm;

	HBITMAP			hbm;
	HGDIOBJ			oldhbm;
	HBRUSH			hBrush;
	RGBQUAD			*pixels;

	PCFont(const char *name, float size, float weight, bool italics, bool underline, bool strikeout);
	~PCFont();

	int		Leading()	const	{ return tm.tmInternalLeading; }
	int		Baseline()	const	{ return tm.tmAscent;	}// + tm.tmInternalLeading; }
	int		VSpacing()	const	{ return tm.tmHeight + tm.tmExternalLeading; }

	PCGlyph	Glyph(int i, int bits) { return PCGlyph(this, i, bits); }
};

PCFont::PCFont(const char *name, float size, float weight, bool italics, bool underline, bool strikeout) : hbm(0) {
	hdc		= CreateCompatibleDC(NULL);
	hFont	= CreateFontA(
		-int(size),				// logical height of font
		0,						// logical average character width
		0,						// angle of escapement
		0,						// base-line orientation angle
		weight,					// font weight
		italics,				// italic attribute flag
		underline,				// underline attribute flag
		strikeout,				// strikeout attribute flag
		DEFAULT_CHARSET,		// character set identifier
		OUT_DEFAULT_PRECIS,		// output precision
		CLIP_DEFAULT_PRECIS,	// clipping precision
		DEFAULT_QUALITY,		// output quality
		FF_DONTCARE,			// pitch and family
		name				 	// pointer to typeface name string
	);

	SelectObject(hdc, hFont);
	GetTextMetrics(hdc, &tm);

	ABC abc;
	if (!GetCharABCWidths(hdc, ' ', ' ', &abc)) {
		BITMAPINFO	bi;
		clear(bi);
		bi.bmiHeader.biSize		= sizeof(bi.bmiHeader);
		bi.bmiHeader.biWidth	= size * 2;
		bi.bmiHeader.biHeight	= size * -2;
		bi.bmiHeader.biPlanes	= 1;
		bi.bmiHeader.biBitCount	= 32;
		hbm		= CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
		oldhbm	= SelectObject(hdc, hbm);
		hBrush	= CreateSolidBrush(RGB(0,0,0));

		SetBkColor(hdc, RGB(0,0,0));
		SetTextColor(hdc, RGB(255,255,255));
	}
}

PCFont::~PCFont() {
	if (hbm) {
		DeleteObject(hBrush);
		DeleteObject(SelectObject(hdc, oldhbm));
	}
	DeleteDC(hdc);
}

PCGlyph::PCGlyph(PCFont *font, int i, int bits) : buffer(0) {
	int	mode = bits <= 2 ? GGO_GRAY2_BITMAP : bits <= 4 ? GGO_GRAY4_BITMAP : GGO_GRAY8_BITMAP;
	auto	size = GetGlyphOutlineW(font->hdc, i, mode, this, 0, NULL, &mat_identity);
	if (size > 0) {
		buffer.create(size);
		GetGlyphOutlineW(font->hdc, i, mode, this, size, buffer, &mat_identity);
	}
	if (!GetCharABCWidthsW(font->hdc, i, i, &abc))
		clear(abc);
}

typedef PCFont		PlatformFont;
typedef PCGlyph		PlatformGlyph;

void copy_n(RGBQUAD *srce, ISO_rgba *dest, size_t count)			{ memcpy(dest, srce, count * sizeof(ISO_rgba)); }


#else

CGAffineTransform	mat_identity = {1, 0, 0, 1, 0, 0};

struct MACGlyph {
	int			xoff, yoff;
	uint32		width, height;
	float		k, a;

	malloc_block	buffer;
	MACGlyph(struct MACFont *font, int i, int bits);

	operator	bool()		const	{ return !!buffer; }
	int			xoffset()	const	{ return xoff;	}
	int			yoffset()	const	{ return yoff;	}
	float		kern()		const	{ return k;		}
	float		advance()	const	{ return a;		}
	block<uint8, 2>	block()	const {
		return make_strided_block((uint8*)buffer, width, (width + 15) & ~15, height);
	}
};

struct MACFont {
	CTFontRef	font;
	uint32		extent;

	MACFont(const char *name, float size, float weight, bool italics, bool underline, bool strikeout) {
	#if 0
		void	*keys[]	= {
			kCTFontFamilyNameKey
		};
		void	*values[]	= {
		};
		CFDictionaryRef		attr	= CFDictionaryCreate(NULL, keys, values, num_elements(keys), kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
		CTFontDescriptorRef desc	= CTFontDescriptorCreateWithAttributes(attr);
		font = CTFontCreateWithFontDescriptor(fd, size, &mat);
		#else

		CFStringRef		name2 = CFStringCreateWithCStringNoCopy(NULL, name, kCFStringEncodingMacRoman, NULL);
		font	= CTFontCreateWithName(name2, size, &mat_identity);
		extent	= int(size * 2);
		#endif
	}
	~MACFont() {
		CFRelease(font);
	}

	float	Leading()	const	{ return CTFontGetLeading(font); }
	float	Baseline()	const	{ return CTFontGetAscent(font) + CTFontGetLeading(font); }
	float	VSpacing()	const	{ return CTFontGetAscent(font) + CTFontGetDescent(font) + CTFontGetLeading(font); }

	MACGlyph	Glyph(int i, int bits) { return MACGlyph(this, i, bits); }
};

MACGlyph::MACGlyph(struct MACFont *font, int i, int bits) : buffer(0), xoff(0), yoff(0) {
	UniChar	u	= i;
	CGGlyph	g;
	CGSize	advance;
	CGRect	rect;

	CTFontGetGlyphsForCharacters(font->font, &u, &g, 1);
	CTFontGetBoundingRectsForGlyphs(font->font, kCTFontOrientationDefault, &g, &rect, 1);
	CTFontGetAdvancesForGlyphs(font->font, kCTFontOrientationDefault, &g, &advance, 1);
	a	= advance.width;
	k	= rect.origin.x;

	width	= int(iso::ceil(rect.size.width));
	height	= int(iso::ceil(rect.size.height));

	if (width && height) {
		uint32	stride	= (width + 15) & ~15;
		buffer.create(stride * height);
		memset(buffer, 0, stride * height);

		CGPathRef		path	= CTFontCreatePathForGlyph(font->font, g, &mat_identity);
		CGColorSpaceRef mono	= CGColorSpaceCreateDeviceGray();
		CGContextRef	context	= CGBitmapContextCreate(buffer, width, height, 8, stride, mono, kCGImageAlphaNone);

		CGContextTranslateCTM(context, -rect.origin.x, -rect.origin.y);
		CGContextAddPath(context, path);
		CGContextSetFillColorWithColor(context, CGColorGetConstantColor(kCGColorWhite));
		CGContextFillPath(context);

		CGContextRelease(context);
		CGColorSpaceRelease(mono);
		CGPathRelease(path);

//		rect.origin.y += font->Baseline();
		yoff	= rect.origin.y + rect.size.height;
		xoff	= rect.origin.x;
	}
}

typedef MACFont		PlatformFont;
typedef MACGlyph	PlatformGlyph;

#endif


// MacRoman chars to unicode
uint16 MacRoman[] = {
	196,197,199,201,209,214,220,225,	// 80-87
	224,226,228,227,229,231,233,232,	// 88-8f
	234,235,237,236,238,239,241,243,	// 90-97
	242,244,246,245,250,249,251,252,	// 98-9f
	822,176,162,163,167,872,182,223,	// a0-a7
	174,169,848,180,168,880,198,216,	// a8-af
	873,177,880,880,165,181,870,872,	// b0-b7
	871,960,874,170,186,937,230,248,	// b8-bf
	191,161,172,873,402,877,916,171,	// c0-c7
	187,894,160,192,195,213,338,339,	// c8-cf
	821,821,822,822,821,821,247,967,	// d0-d7
	255,376,826,164,824,825,642,642,	// d8-df
	822,183,821,822,824,194,202,193,	// e0-e7
	203,200,205,206,207,204,211,212,	// e8-ef
	000,210,218,219,217,305,770,771,	// f0-f7
	175,728,729,730,184,733,731,711,	// f8-ff
};

// MacRoman chars to unicode
uint16 ANSI[] = {
	0x20AC,	//0x80	EURO SIGN
	0x0000,	//0x81	UNDEFINED
	0x201A,	//0x82	SINGLE LOW-9 QUOTATION MARK
	0x0192,	//0x83	LATIN SMALL LETTER F WITH HOOK
	0x201E,	//0x84	DOUBLE LOW-9 QUOTATION MARK
	0x2026,	//0x85	HORIZONTAL ELLIPSIS
	0x2020,	//0x86	DAGGER
	0x2021,	//0x87	DOUBLE DAGGER
	0x02C6,	//0x88	MODIFIER LETTER CIRCUMFLEX ACCENT
	0x2030,	//0x89	PER MILLE SIGN
	0x0160,	//0x8A	LATIN CAPITAL LETTER S WITH CARON
	0x2039,	//0x8B	SINGLE LEFT-POINTING ANGLE QUOTATION MARK
	0x0152,	//0x8C	LATIN CAPITAL LIGATURE OE
	0x0000,	//0x8D	UNDEFINED
	0x017D,	//0x8E	LATIN CAPITAL LETTER Z WITH CARON
	0x0000,	//0x8F	UNDEFINED
	0x0000,	//0x90	UNDEFINED
	0x2018,	//0x91	LEFT SINGLE QUOTATION MARK
	0x2019,	//0x92	RIGHT SINGLE QUOTATION MARK
	0x201C,	//0x93	LEFT DOUBLE QUOTATION MARK
	0x201D,	//0x94	RIGHT DOUBLE QUOTATION MARK
	0x2022,	//0x95	BULLET
	0x2013,	//0x96	EN DASH
	0x2014,	//0x97	EM DASH
	0x02DC,	//0x98	SMALL TILDE
	0x2122,	//0x99	TRADE MARK SIGN
	0x0161,	//0x9A	LATIN SMALL LETTER S WITH CARON
	0x203A,	//0x9B	SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
	0x0153,	//0x9C	LATIN SMALL LIGATURE OE
	0x0000,	//0x9D	UNDEFINED
	0x017E,	//0x9E	LATIN SMALL LETTER Z WITH CARON
	0x0178,	//0x9F	LATIN CAPITAL LETTER Y WITH DIAERESIS
};

// ANSI 0x80-0xbf
uint16 Unicode_0x80[] = {
	0x00C7,	// 128	Ä
	0x00FC,	// 129 	Å
	0x00E9,	// 130 	Ç
	0x00E2,	// 131 	É
	0x00E4,	// 132 	Ñ
	0x00E0,	// 133 	Ö
	0x00E5,	// 134 	Ü
	0x00E7,	// 135 	á
	0x00EA,	// 136 	à
	0x00EB,	// 137 	â
	0x00E8,	// 138 	ä
	0x00EF,	// 139 	ã
	0x00EE,	// 140 	å
	0x00EC,	// 141 	ç
	0x00C4,	// 142 	é
	0x00C5,	// 143 	è
	0x00C9,	// 144 	ê
	0x00E6,	// 145 	ë
	0x00C6,	// 146 	í
	0x00F4,	// 147 	ì
	0x00F6,	// 148 	î
	0x00F2,	// 149 	ï
	0x00FB,	// 150 	ñ
	0x00F9,	// 151 	ó
	0x00FF,	// 152 	ò
	0x00D6,	// 153 	ô
	0x00DC,	// 154 	ö
	0x00A2,	// 155 	õ
	0x00A3,	// 156 	ú
	0x00A5,	// 157 	ù
	0x20A7,	// 158 	û
	0x0192,	// 159 	ü
	0x00E1,	// 160
	0x00ED,	// 161 	°
	0x00F3,	// 162 	¢
	0x00FA,	// 163 	£
	0x00F1,	// 164 	§
	0x00D1,	// 165 	•
	0x00AA,	// 166 	¶
	0x00BA,	// 167 	ß
	0x00BF,	// 168 	®
	0x2310,	// 169 	©
	0x00AC,	// 170 	™
	0x00BD,	// 171 	´
	0x00BC,	// 172 	¨
	0x00A1,	// 173 	≠
	0x00AB,	// 174 	Æ
	0x00BB,	// 175	Ø
	0x2591,	// 176	∞
	0x2592,	// 177 	±
	0x2593,	// 178 	≤
	0x2502,	// 179 	≥
	0x2524,	// 180 	¥
	0x2561,	// 181 	µ
	0x2562,	// 182 	∂
	0x2556,	// 183 	∑
	0x2555,	// 184 	∏
	0x2563,	// 185 	π
	0x2551,	// 186 	∫
	0x2557,	// 187 	ª
	0x255D,	// 188 	º
	0x255C,	// 189 	Ω
	0x255B,	// 190 	æ
	0x2510,	// 191	ø
};

//-----------------------------------------------------------------------------
//	Font structures
//-----------------------------------------------------------------------------

struct FontParams {
	string		name;
	int			size;		// +ve: character height; -ve: cell height
	uint32		weight;
	bool8		italics;
	bool8		underline;
	int8		outline;
	bool8		strikeout;
	ISO_openarray<pair<int,int> >	chars;
	ISO_openarray<char16>			scan;
	uint32		width;
	uint32		xspacing;
	int			yspacing;
};

//enum {
//	FF2_PACK	= 0x80
//};
struct FontParams2 {
	ISO_ptr<bitmap>	bm;
	uint32		xspacing;
	uint32		yspacing;
	ISO_openarray<char16>	chars;
	uint8		baseline, spacing, top, outline;
};

struct TTFontParams {
	const char	*name;
	uint32		size;
	uint32		weight;
	bool8		italics;
	bool8		underline;
	bool8		strikeout;
	uint16		firstchar;
	uint16		numchars;
};

ISO_DEFUSERCOMPV(FontParams,name,size,weight,italics,underline,outline,strikeout,chars,scan,width,xspacing,yspacing);
ISO_DEFUSERCOMPV(FontParams2, bm, xspacing, yspacing, chars, baseline, spacing, top, outline);
ISO_DEFUSERCOMPV(TTFontParams, name,weight,italics,underline,strikeout,firstchar,numchars);

//-----------------------------------------------------------------------------
//	Packing
//-----------------------------------------------------------------------------

struct FontRect {
	int minx, maxx, miny, maxy;
	FontRect(const block<ISO_rgba, 2> &block);
	bool	Any()		{ return minx <= maxx; }
	int		MinX()		{ return minx; }
	int		MaxX()		{ return maxx; }
	int		MinY()		{ return miny; }
	int		MaxY()		{ return maxy; }
	int		Width()		{ return maxx - minx + 1; }
	int		Height()	{ return maxy - miny + 1; }
};


FontRect::FontRect(const block<ISO_rgba, 2> &block) : maxx(0), maxy(0) {
	int	w = minx = block.size<1>(), h = miny = block.size<2>();
	for (int j = 0; j < h; j++) {
		const ISO_rgba	*srce	= block[j];
		bool			any		= false;
		for (int k = 0; k < w; k++, srce++) {
			if (srce->a) {
				any = true;
				if (k < minx) minx = k;
				if (k > maxx) maxx = k;
			}
		}
		if (any) {
			if (j < miny) miny = j;
			if (j > maxy) maxy = j;
		}
	}
	if (minx > maxx) {
		maxx = w - 1;
		maxy = h - 1;
	}
}

bool CheckPack(const block<ISO_rgba, 2> &b) {
	return !find_if(b, [](const ISO_rgba &x) {
		if (x.a) {
			if (x.a == 255) {
				if (!x.IsGrey())
					return true;
			} else {
				if (x.r || x.g || x.b)
					return true;
			}
		}
		return false;
	});
}

void CopyAlpha(const block<ISO_rgba, 2> &srce, const block<ISO_rgba, 2> &dest) {
	for (int j = 0, w = dest.size<1>(), h = dest.size<2>(); j < h; j++) {
		ISO_rgba	*s = srce[j];
		ISO_rgba	*d = dest[j];
		for (int i = w; i--; s++, d++)
			d->a = s->a;
	}
}

void CopyChan(int c, const block<ISO_rgba, 2> &srce, const block<ISO_rgba, 2> &dest) {
	for (int j = 0, w = dest.size<1>(), h = dest.size<2>(); j < h; j++) {
		ISO_rgba	*s = srce[j];
		uint8		*d = &dest[j][0][c];
		for (int i = w; i--; s++, d += 4) {
			*d	= s->a == 255
				? s->r * 127 / 255 + 128
				: s->a * 127 / 255;
		}
	}
}

bool CompareRects(const block<ISO_rgba, 2> &a, const block<ISO_rgba, 2> &b) {
	for (int j = 0, w = a.size<1>(), h = a.size<2>(); j < h; j++) {
		if (memcmp(a[j], b[j], w * sizeof(ISO_rgba)))
			return false;
	}
	return true;
}

#if 1
void MakeDistanceField(const block<ISO_rgba, 2> &drect, const block<uint8, 2> &srect, int xoffset, int yoffset, int scale, int samples);
#else
static void MakeDistanceField(const block<ISO_rgba, 2> &drect, const block<uint8, 2> &srect, int xoffset, int yoffset, int scale, int samples)
{
	for (int y0 = 0; y0 < drect.Height(); y0++) {
		ISO_rgba	*dest = drect[y0];
		for (int x0 = 0; x0 < drect.Width(); x0++, dest++) {
			int		x1	= (x0 - samples) * scale + xoffset,
					y1	= y0 * scale + yoffset;
			int		set	= x1 < 0 || x1 >= srect.Width() || y1 < 0 || y1 >= srect.Height() ? 0 : !!srect[y1][x1];
			float	r	= 1;
			for (int d = 1; d < scale * samples && r > d / (sqrt2 * scale * samples); d++) {
				for (int t0 = 0; t0 <= d * 2; t0++) {
					int	t = t0 & 1 ? -(t0 + 1) / 2 : t0 / 2;
					int	x2, y2;
					int	s0, s1, s2, s3;

					if ((x2 = x1 + t) >= 0 && x2 < srect.Width()) {
						s0 = (y2 = y1 + d) >= 0 && y2 < srect.Height() ? !!srect[y2][x2] : 0;
						s1 = (y2 = y1 - d) >= 0 && y2 < srect.Height() ? !!srect[y2][x2] : 0;
					} else {
						s0 = s1 = 0;
					}

					if ((y2 = y1 + t) >= 0 && y2 < srect.Height()) {
						s2 = (x2 = x1 + d) >= 0 && x2 < srect.Width() ? !!srect[y2][x2] : 0;
						s3 = (x2 = x1 - d) >= 0 && x2 < srect.Width() ? !!srect[y2][x2] : 0;
					} else {
						s2 = s3 = 0;
					}

					if (s0 != set || s1 != set || s2 != set || s3 != set) {
						r = min(r, sqrt(t * t + d * d) / (sqrt2 * scale * samples));
						break;
					}
				}
			}
			dest->r = dest->g = dest->b = 255;
			dest->a = 255 * (set ? (r + 1) / 2 : (1 - r) / 2);
		}
	}
}
#endif

//-----------------------------------------------------------------------------
//	banking
//-----------------------------------------------------------------------------

class FontBank {
protected:
	int				first, last;
	FontBank		*subbank[256];
	const GlyphInfo	*gi[256];
public:
	FontBank() : first(256), last(0) {
		clear(subbank);
		clear(gi);
	}

	~FontBank()	{
		for (int i = first; i <= last; i++)
			delete subbank[i];
	}

	int			FirstEntry()	const { return first; }
	int			NumEntries()	const { return last - first + 1; }

	int			Count() const {
		int c = NumEntries();
		for (int i = first; i <= last; i++) {
			if (subbank[i])
				c += subbank[i]->Count();
		}
		return c;
	}

	void		AddChar(int c) {
		if (c < first)
			first = c;
		if (c > last)
			last = c;
	}

	FontBank*	Subbank(int c) {
		AddChar(c);
		if (!subbank[c])
			subbank[c] = new FontBank;
		return subbank[c];
	}

	void		SetGlyph(int c, const GlyphInfo *g) {
		AddChar(c);
		gi[c] = g;
	}

	int		Write(stride_iterator<GlyphInfo> out, int offset) {
		auto	p	= out + offset;
		offset	+= NumEntries();
		for (int i = first; i <= last; i++, p++) {
			if (subbank[i]) {
				p->flags	= GlyphInfo::TABLE;
				auto	p2	= unconst(p->get_table());
				p2->n		= subbank[i]->NumEntries();
				p2->f		= subbank[i]->FirstEntry();
				p2->offset	= offset;
				offset		= subbank[i]->Write(out, offset);
			} else if (gi[i]) {
				memcpy(&*p, gi[i], out.stride());
			} else {
				clear(*p);
			}
		}
		return offset;
	}
};

struct RootFontBank : public FontBank {
	void	Add(const GlyphInfo *gi, uint8 *chars, size_t num) {
		FontBank	*bank	= this;
		for (int i = 0; i < num - 1; i++)
			bank = bank->Subbank(chars[i]);
		bank->SetGlyph(chars[num - 1], gi);
	}
	void	Write(stride_iterator<GlyphInfo> out) {
		FontBank::Write(out, 0);
	}
};

uint8 GetGlyphFlags(int c) {
	if (c == ' ' || (c >= 0x4e00 && c < 0xa000) || (c >= 0x3040 && c < 0x30a0) || c == 0x3002)
		return GlyphInfo::BREAKABLE;
	if (c == '?' || c == '!' || c == ';' || c == ':' || c == '.')	// some languages require a space before these punctuation symbols
		return GlyphInfo::NOT_SOL;
	return 0;
}

//-----------------------------------------------------------------------------
//	Conversions
//-----------------------------------------------------------------------------

ISO_ptr_machine<bitmap> FontBitmap(TexFont &p) {
	return *(ISO_ptr_machine<bitmap>*)&p.tex;
}

ISO_ptr<bitmap> FontParamsBitmap(FontParams2 &p) {
	return p.bm;
}

FontParams2 FontMaker1(FontParams &p) {
	FontParams2		fp2;

	if (!p.size)		p.size		= 16;
	if (!p.xspacing)	p.xspacing	= abs(p.size) * 2;
	if (!p.yspacing)	p.yspacing	= abs(p.size) * 2;

	int	first = ~0u >> 1, last = -1;
	if (p.chars || p.scan) {
		for (pair<int,int> *i = p.chars.begin(), *e = p.chars.end(); i < e; ++i) {
			int	a	= i->a;
			int	b	= i->b;
			if (b == 0)
				b = i->b = 1;
			if (a < first)
				first = a;
			if (a + b > last)
				last = a + b;
		}
		for (char16 *i = p.scan.begin(), *e = p.scan.end(); i < e; ++i) {
			int	a = *i;
			if (a < 32)
				continue;
			if (a < first)
				first = a;
			if (a >= last)
				last = a + 1;
		}
	} else {
		first	= ' ';
		last	= 127;
	}

	int		num		= last - first;
	bool8	*use	= new bool8[num];
	memset(use, int(!p.chars && !p.scan), num);
	for (pair<int,int> *i = p.chars.begin(), *e = p.chars.end(); i < e; ++i) {
		for (int a = i->a - first, b = a + i->b; a < b; a++)
			use[a] = true;
	}
	for (char16 *i = p.scan.begin(), *e = p.scan.end(); i < e; ++i) {
		int	a = *i - first;
		if (a >= 0)
			use[a] = true;
	}

	int	num_used = 0;
	for (int i = 0; i < num; i++)
		num_used += int(use[i]);

	char16	*d = fp2.chars.Create(num_used);
	for (int i = 0; i < num; i++) {
		if (use[i])
			*d++ = i + first;
	}

	num = num_used;

	if (p.width == 0)
		p.width = int(iso::sqrt(float(num * p.yspacing / p.xspacing))) * p.xspacing;

	int				fontscale	= 1;
	int				fontoffset	= p.outline;
	bool			dist		= fontoffset < 0;
	int				across		= p.width / p.xspacing;
	bitmap			&bm			= *fp2.bm.Create();
	bm.Create(p.width, (num + across - 1) / across * p.yspacing);

	if (dist) {
		fontscale	= 16;
		fontoffset	= -fontoffset;
		bm.SetFlags(BMF_NOMIP | BMF_UNNORMALISED | BMF_NOCOMPRESS);
	}

	PlatformFont	pcfont(p.name, p.size * fontscale, p.weight, p.italics, p.underline, p.strikeout);

	ISO_rgba	xlat[65];
	for (int i = 0; i < 65; i++)
		xlat[i] = ISO_rgba(255,255,255,(i * 255 + 32) / 64);

	fill(bm.All(), p.outline > 0 ? ISO_rgba(0,0,0,0) : ISO_rgba(255,255,255,0));

	for (int i = 0; i < num; i++) {
		int	c = fp2.chars[i];
		if (use && !use[c - first])
			continue;

		if (c == 0xa0)
			c = ' ';	// non-breaking space
		else if (c >= 0x80 && c < 0xa0)
			c = ANSI[c - 0x80];

		int	x = (i % across) * p.xspacing, y = (i / across) * p.yspacing;

		PlatformGlyph	glyph = pcfont.Glyph(c, dist ? 2 : 8);

		if (glyph) {
			block<uint8, 2>	block	= glyph.block();
			int				yoffset	= pcfont.Baseline() - glyph.yoffset();
			int				xoffset	= max(glyph.xoffset(), 0);

			if (dist) {
				MakeDistanceField(
					bm.Block(x, y, p.xspacing - 1, p.yspacing - 1),
					block,
					-xoffset, -yoffset, fontscale, fontoffset
				);

			} else {
				if (p.outline) {
					for (int oy = 0; oy <= p.outline * 2; oy++) {
						for (int ox = 0; ox <= p.outline * 2; ox++) {
							if (square(ox - p.outline) + square(oy - p.outline) > square(p.outline + 1))
								continue;
							for (uint32 j = 0; j < block.size<2>(); j++) {
								int	y1 = oy + j + yoffset;
								if (y1 >= 0 && y1 < int(p.yspacing)) {
									ISO_rgba	*dest = bm.ScanLine(y + y1) + ox + x + xoffset;
									uint8		*srce = block[j];
									for (uint32 x1 = 0; x1 < block.size<1>(); x1++) {
										uint8	a = xlat[*srce++].a;
										*dest *= 255 - a;
										dest->a = (dest->a * (255 - a) + 255 * a) / 255;
										dest++;
									}
								}
							}
						}
					}
				}
				for (uint32 j = 0; j < block.size<2>(); j++) {
					int	y1 = j + p.outline + yoffset;
					if (y1 >= 0 && y1 < int(p.yspacing)) {
						ISO_rgba	*dest = bm.ScanLine(y + y1) + x + p.outline + xoffset;
						uint8		*srce = block[j];
						for (uint32 x1 = 0; x1 < block.size<1>(); x1++) {
							if (uint8 a = xlat[*srce++].a) {
								dest->a = (dest->a * (255 - a) + 255 * a) / 255;
								dest->r =
								dest->g =
								dest->b = a * 255 / dest->a;
							}
							dest++;
						}
					}
				}
			}
		}

		ISO_rgba	*dest = bm.ScanLine(y + p.yspacing - 1) + x + fontoffset - min(int(glyph.kern()), 0) / fontscale;
		for (int j = 0, w = int(glyph.advance() / fontscale + 0.5f); j < w; j++)
			*dest++ = (char)255;
	}

	fp2.xspacing	= p.xspacing;
	fp2.yspacing	= p.yspacing;
	fp2.outline		= p.outline < 0 ? -p.outline : 0;
	fp2.baseline	= uint8(pcfont.Baseline() / fontscale) + fontoffset;
	fp2.spacing		= uint8(pcfont.VSpacing() / fontscale);
	fp2.top			= uint8(pcfont.Leading() / fontscale) + fontoffset;

	delete[] use;
	return fp2;
}

TexFont FontMaker2(FontParams2 &p) {
	if (p.xspacing == 0)
		throw_accum("Need at least x-spacing");

	if (p.yspacing == 0)
		p.yspacing = p.xspacing;

	TexFont		font;
	bitmap		&bm			= *p.bm;
	int			across		= bm.Width() / p.xspacing;
	int			num			= p.chars.Count();
	auto		glyph_info	= new TexGlyphInfo[num];

	int	font_maxy = 0, font_miny = p.yspacing,
		font_maxx = 0, font_minx = p.xspacing,
		font_width = 0;

	for (int i = 0; i < num; i++) {
		int			c	= p.chars[i];
		auto&		gi	= glyph_info[i];
		int			x = (i % across) * p.xspacing,
					y = (i / across) * p.yspacing;
		FontRect	rect(bm.Block(x, y, p.xspacing, p.yspacing - 1));
		FontRect	kern(bm.Block(x, y + p.yspacing - 1, p.xspacing, 1));

		clear(gi);
		gi.w		= rect.Width();
//		if ((p.flags & FF2_PACK) && CheckPack(bm.Block(x, y, p.xspacing, p.yspacing - 1)))
//			gi.flags = GlyphInfo::TABLE;

		if (kern.Any()) {
			gi.k	= rect.MinX() - kern.MinX();
			gi.s	= kern.Width();
		} else {
			gi.k	= rect.MinX();
			gi.s	= rect.Width();
		}

		if (rect.Any()) {
			if (rect.MinY() < font_miny)
				font_miny = rect.MinY();
			if (rect.MaxY() > font_maxy)
				font_maxy = rect.MaxY();
			if (rect.MinX() < font_minx)
				font_minx = rect.MinX();
			if (rect.MaxX() > font_maxx)
				font_maxx = rect.MaxX();
			font_width += rect.Width();

			for (int j = 0; j < i; j++) {
				if (gi.w == glyph_info[j].w && CompareRects(bm.Block(x, y, p.xspacing, p.yspacing - 1), bm.Block((j % across) * p.xspacing, (j / across) * p.yspacing, 0, 0))) {
					gi.flags	= GlyphInfo::TABLE;	// here means it's a dupe of another character
					gi.u		= j;
					gi.w		= 0;
				}
			}

		} else if (i > 'a' - 'A' && c >= 'a' && c <= 'z' && glyph_info[i + 'A' - 'a'].w) {
			gi.flags	= GlyphInfo::TABLE;	// here means it's a dupe of another character
			gi.u		= i + 'A' - 'a';
		}
	}

	int			font_height	= font_maxy - font_miny + 1;
	Platform	*platform = Platform::Get(ISO::root("variables")["exportfor"].GetString());

#if 1
	int	next_width	= font_maxx - font_minx;
	int	best_width	= 0, best_height = 0;
	int	curr_width;
    while (next_width && next_width <= (curr_width = platform->NextTexWidth(next_width))) {
		int	lines		= 1;
		next_width		= 0;
		/*
		if (p.flags & FF2_PACK) {
			int	xc[4] = {0,0,0,0};
			for (int i = 0; i < num; i++) {
				GlyphInfo	&gi	= glyph_info[i];
				if (gi.w && (gi.flags & GlyphInfo::PACKED)) {
					int	c	= 0;
					for (int j = 1; j < 4; j++) {
						if (xc[j] < xc[c])
							c = j;
					}
					xc[c] += gi.w + 1;
					if (xc[c] > curr_width) {
						if (next_width)
							next_width = min(next_width, xc[c]);
						else
							next_width = xc[c];
						xc[0] = xc[1] = xc[2] = xc[3] = 0;
						xc[c] = gi.w + 1;
						lines++;
					}
				}
			}
			for (int i = 0, x = max(max(max(xc[0], xc[1]), xc[2]), xc[3]); i < num; i++) {
				GlyphInfo	&gi	= glyph_info[i];
				if (gi.w && !(gi.flags & GlyphInfo::PACKED)) {
					x += gi.w + 1;
					if (x > curr_width) {
						if (next_width)
							next_width = min(next_width, x);
						else
							next_width = x;
						x  = gi.w + 1;
						lines++;
					}
				}
			}
		} else */ {
			for (int i = 0, x = 0; i < num; i++) {
				if (int w = glyph_info[i].w) {
					x += w + 1;
					if (x > curr_width) {
						if (next_width)
							next_width = min(next_width, x);
						else
							next_width = x;
						x  = w + 1;
						lines++;
					}
				}
			}
		}
		int	temp_height	= lines * (font_height + 1);
		int	curr_height	= platform->NextTexHeight(temp_height);
		if (curr_height >= temp_height && (best_width == 0 || platform->BetterTex(best_width, best_height, curr_width, curr_height))) {
			best_width	= curr_width;
			best_height	= curr_height;
		}
	}

	if (p.spacing == 0)
		p.spacing = font_height;
	if (p.baseline == 0)
		p.baseline = font_maxy;

	font.height		= font_height;
	font.baseline	= p.baseline - font_miny;
	font.top		= p.top;
	font.spacing	= p.spacing;
	font.outline	= (best_width / p.outline + 2) / 4;
	font.tex		= 0;

	auto	&fontbitmap = (ISO_ptr_machine<bitmap>&)font.tex;
	fontbitmap.Create()->Create(best_width, best_height, bm.Flags());
	ISO_rgba	clear_col = platform->DefaultColour();
	clear_col.a	= 0;
	fill(fontbitmap->All(), clear_col);

/*	if (p.flags & FF2_PACK) {
		int	xc[4]	= {0,0,0,0};
		int	y		= 0;
		for (int i = 0; i < num; i++) {
			GlyphInfo	&gi	= glyph_info[i];
			if (gi.w && (gi.flags & GlyphInfo::PACKED)) {
				int			w	= gi.w;
				int			c	= 0;
				int			sx	= (i % across) * p.xspacing,
							sy	= (i / across) * p.yspacing;

				for (int j = 1; j < 4; j++) {
					if (xc[j] < xc[c])
						c = j;
				}

				if (xc[c] + w + 1 > best_width) {
					xc[0] = xc[1] = xc[2] = xc[3] = 0;
					y += font_height + 1;
				}

				FontRect	rect(bm.Block(sx, sy, p.xspacing, p.yspacing - 1));
				CopyChan(c, bm.Block(sx + rect.MinX(), sy + font_miny, w, font_height), fontbitmap->Block(xc[c], y, w, font_height));

				gi.u	= xc[c];
				gi.v	= y;
				gi.flags= c + 1;
				xc[c]	+= w + 1;
			}
		}
		for (int i = 0, x = max(max(max(xc[0], xc[1]), xc[2]), xc[3]); i < num; i++) {
			GlyphInfo	&gi	= glyph_info[i];
			if (gi.w && !(gi.flags & GlyphInfo::PACKED)) {
				int			w	= gi.w;
				int			sx = (i % across) * p.xspacing,
							sy = (i / across) * p.yspacing;
				if (x + w + 1 > best_width) {
					x  = 0;
					y += font_height + 1;
				}
				FontRect	rect(bm.Block(sx, sy, p.xspacing, p.yspacing - 1));
				copy(bm.Block(sx + rect.MinX(), sy + font_miny, w, font_height), fontbitmap->Block(x, y, w, font_height));

				gi.u	= x;
				gi.v	= y;
				x		+= w + 1;
			}
		}
	} else*/ {
		for (int i = 0, x = 0, y = 0; i < num; i++) {
			auto&	gi	= glyph_info[i];
			if (int w = gi.w) {
				int		sx = (i % across) * p.xspacing,
						sy = (i / across) * p.yspacing;
				if (x + w + 1 > best_width) {
					x  = 0;
					y += font_height + 1;
				}
				FontRect	rect(bm.Block(sx, sy, p.xspacing, p.yspacing - 1));
				CopyAlpha(bm.Block(sx + rect.MinX(), sy + font_miny, w, font_height), fontbitmap->Block(x, y, w, font_height));

				gi.u	= x;
				gi.v	= y;
				x		+= w + 1;
			} else if (gi.flags & GlyphInfo::TABLE) {
				gi		= glyph_info[gi.u];
			}
		}
	}
#else
	allocator2d	a;
	a.grow_ratio = 1;
	for (int i = 0, x = 0; i < num; i++) {
		allocator2d::point	result;
		if (int w = glyph_info[i].w) {
			a.allocate(w, font_height, result);
			glyph_info[i].u = result.x;
			glyph_info[i].v = result.y;
		}
	}


	if (p.spacing == 0)
		p.spacing = font_height;
	if (p.baseline == 0)
		p.baseline = font_maxy;

	font.height		= font_height;
	font.baseline	= p.baseline - font_miny;
	font.top		= p.top;
	font.spacing	= p.spacing;
	font.outline	= (a.size.x / p.outline + 2) / 4;
	font.tex		= 0;

	ISO_ptr<bitmap>	&fontbitmap = (ISO_ptr<bitmap>&)font.tex;
	fontbitmap.Create()->Create(a.size.x, a.size.y, bm.Flags());
	ISO_rgba	clear_col = platform->DefaultColour();
	clear_col.a	= 0;
	fill(fontbitmap->All(), clear_col);

	for (int i = 0; i < num; i++) {
		allocator2d::point	result;
		if (int w = glyph_info[i].w) {
			int		sx = (i % across) * p.xspacing,
					sy = (i / across) * p.yspacing;
			FontRect	rect(bm.Block(sx, sy, p.xspacing, p.yspacing - 1));
			CopyAlpha(bm.Block(sx + rect.MinX(), sy + font_miny, w, font_height), fontbitmap->Block(glyph_info[i].u, glyph_info[i].v, w, font_height));
		}
	}
#endif

	RootFontBank	bank;
	for (int i = 0; i < num; i++) {
		auto&	gi	= glyph_info[i];
		if (gi.w || gi.s) {
			uint8	chars[8];
			int		c	= p.chars[i];
			gi.flags	|= GetGlyphFlags(c);
			if (c < 0xc0) {
				chars[0] = c;
				bank.Add(&gi, chars, 1);
			}
			if (c >= 0x80)
				bank.Add(&gi, chars, put_char(c, (char*)chars));
		}
	}

	ISO_openarray<TexGlyphInfo>	glyphs(bank.Count());
	bank.Write(glyphs.begin());
	font.glyph_info = glyphs.detach();

	font.firstchar	= bank.FirstEntry();
	font.numchars	= bank.NumEntries();

	delete[] glyph_info;
	return font;
}

struct ISOSlugFont : Font {
	uint8	firstchar, numchars;

	ISO_openarray<SlugGlyphInfo>	glyph_buffer;

	ISODataBuffer	band_buffer;
	ISODataBuffer	indices_buffer;
	ISODataBuffer	curve_buffer;
	ISOTexture		palette;
};

ISO_DEFSAME(ISOSlugFont, SlugFont);

dynamic_array<uint16> MakeSlugBands(range<const float2*> points, int num_bands, uint16x2 *hdest) {
	temp_array<dynamic_array<uint32>>	hbands(num_bands), vbands(num_bands);

	for (int i = 0, n = points.size() / 2 - 1; i < n; ++i) {
		auto	p	= &points[i * 2];
		if (is_nan(p[1]))
			continue;
		auto	a	= p[0] - p[1] * 2 + p[2];
		auto	b	= p[0] - p[1];
		auto	c	= p[0];
		auto	t	= clamp(b / a, zero, one);
		auto	m	= c - t * b;
		auto	mins	= to<int>(floor(min(p[0], p[2], m) * num_bands));
		auto	maxs	= min(to<int>(floor(max(p[0], p[2], m) * num_bands)), num_bands - 1);
		auto	flat	= a == zero & b == zero;
		if (!flat.x)
			for (int x = mins.x; x <= maxs.x; x++)
				vbands[x].push_back(i);
		if (!flat.y)
			for (int y = mins.y; y <= maxs.y; y++)
				hbands[y].push_back(i);
	}

	dynamic_array<uint16>		indices;

	struct Band {
		uint32	offset, size;
	};

	auto	get_offsets = [&](const dynamic_array<uint32> *bands) {
		dynamic_array<Band>	offsets;
		for (int i = 0; i < num_bands; i++) {
			if (i > 0 && bands[i] == bands[i - 1]) {
				offsets.push_back(offsets.back());

			} else {
				offsets.push_back({indices.size32(), bands[i].size32()});
				indices.append(bands[i]);
			}
		}
		return offsets;
	};

	auto	hband_offsets = get_offsets(hbands);
	auto	vband_offsets = get_offsets(vbands);

	auto	vdest = hdest + num_bands;

	for (int i = 0; i < num_bands; i++) {
		sort(indices.slice(hband_offsets[i].offset, hband_offsets[i].size), [&](uint16 a, uint16 b) {
			return max(points[a * 2].x, points[a * 2 + 1].x, points[a * 2 + 2].x) > max(points[b * 2].x, points[b * 2 + 1].x, points[b * 2 + 2].x);
		});
		sort(indices.slice(vband_offsets[i].offset, vband_offsets[i].size), [&](uint16 a, uint16 b) {
			return max(points[a * 2].y, points[a * 2 + 1].y, points[a * 2 + 2].y) > max(points[b * 2].y, points[b * 2 + 1].y, points[b * 2 + 2].y);
		});
		hdest[i] = {hband_offsets[i].offset, hband_offsets[i].size};
		vdest[i] = {vband_offsets[i].offset, vband_offsets[i].size};
	}

	return indices;
}

dynamic_array<float2> GetSlugPoints(const curves& c) {
	if (direction(c)) {
		curves	c2 = c;
		reverse_curves(c2);
		if (!direction(c2))
			return GetSlugPoints(c2);
		ISO_TRACEF("wierd curve\n");
	}
//	ISO_ASSERT(!direction(c));

	interval<float2>	ext(none);
	for (auto& i : c)
		ext |= float2{i.x, i.y};

	dynamic_array<bezier_chain<float2,2>>	chains2 = get_bezier2(c, 6, len(ext.extent()) / 250);
	dynamic_array<float2> points;

	for (auto &c : chains2) {
		points.append(c.c);
		points.push_back(nan);
	}
	return points;
}

template<typename S, typename D> uint32 AppendISO(const S& srce, ISO_openarray<D>& dest) {
	uint32	offset = dest.size32();
	dest.Resize(offset + num_elements(srce));
	copy(srce, &dest[offset]);
	return offset;
}

ISO_rgba to_ISO_rgba(rgba8 c) {
	return force_cast<ISO_rgba>(c);
}

template<typename I, class P = equal_to> auto unique_indices(I i, I end, P pred = P()) {
	if (i == end)
		return end;

	I prev = i;
	while (++i != end) {
		if (!pred(*prev, *i))
			*++prev = move(*i);
		else
			*prev = *i;
	}
	return ++prev;
}

template<class C, class P = equal_to> inline auto unique_indices(C&& c, P pred = P()) {
	return unique_indices(begin(c), end(c), pred);
}

struct gradient {
	glyph_fill::colour_line	colours;
	bool	can_extend;

	gradient(const glyph_fill::colour_line &c) : colours(c), can_extend(c.extend != c.EXTEND_NONE) {}

	void make(block<ISO_rgba, 1> dest) const {
		auto	n	= colours.colours.begin() + 1;
		float	t	= n[-1].t;
		float	dt	= (colours.colours.back().t - t) / (dest.size() - 1);

		for (auto& i : dest) {
			while (n < colours.colours.end() - 1 && t >= n->t)
				++n;
			i	= force_cast<ISO_rgba>((rgba8)evaluate(n[-1], n[0], t));
			t	+= dt;
		}
	}

	bool operator<(const gradient& b) const {
		auto	sizea	= colours.colours.size() + int(can_extend);
		auto	sizeb	= b.colours.colours.size() + int(b.can_extend);
		return sizea < sizeb || (sizea == sizeb && colours.colours < b.colours.colours);
	}
	bool operator==(const gradient& b) const {
		return colours == b.colours;
	}
};

ISO_ptr<ISOSlugFont> SlugMaker(ISO_ptr<FontWrapper> fw, int num_bands) {
	struct glyph_info {
		uint32				indices_offset	= 0;
		uint32				curves_offset	= 0;
		interval<float2>	ext				= empty;
		float				advance			= 0;
		bool				layered			= false;
	};
	struct layer_info {
		uint32				indices_offset	= 0;
		uint32				curves_offset	= 0;
		uint8x4				colour;
	};

	ISO_ptr<ISOSlugFont>	out(fw.ID());
	
	if (num_bands == 0)
		num_bands = 8;

	uint32	num_glyphs	= fw->glyphs.size32();
	uint32	num_layers	= 0;

	for (auto& i : fw->layers_map)
		num_layers += i->size32();

	dynamic_array<glyph_info>			glyph_infos(num_glyphs);
	dynamic_array<layer_info>			layer_infos(num_layers);
	ISO_ptr<ISO_openarray<uint16x2>>	band_buffer(none, (num_glyphs + num_layers) * num_bands * 2);
	ISO_ptr<ISO_openarray<uint16>>		indices_buffer(none, 0);
//	ISO_ptr<ISO_openarray<array_vec<unorm8, 2>>>	curve_buffer(none, 0);
//	ISO_ptr<ISO_openarray<array_vec<unorm16, 2>>>	curve_buffer(none, 0);
	ISO_ptr<ISO_openarray<float2>>					curve_buffer(none, 0);
	ISO_ptr<bitmap>						palette;

	// make gradient texture
	dynamic_array<gradient>	gradients;
	for (auto& i : fw->layers_map) {
		for (auto& j : *i)
			gradients.push_back(j.cols);
	}
	temp_array<uint32>	indices		= int_range(gradients.size32());
	temp_array<uint32>	rev_indices	= indices;

	auto	indices2	= make_double_index_container(rev_indices, indices);
	auto	gradients2	= make_indexed_container(gradients, indices2);
	sort(gradients2);
	auto	end			= unique_indices(gradients2);
	auto	num_unique	= end - gradients2.begin();
	uint32	num_single	= 0;

	for (auto&& i : slice_to(gradients2, num_unique)) {
		if (i->colours.colours.size() > 1)
			break;
		++num_single;
	}

	bool	single_pal	= false;//true;
	uint32	col_width	= 64;
	uint32	col_grads	= single_pal ? div_round_up(num_single, col_width) : 0;
	uint32	col_height	= col_grads + num_unique - num_single;

	if (col_height) {
		col_height	= next_pow2(col_height);
		palette.Create(none, col_width, col_height, BMF_NOMIP|BMF_NOCOMPRESS);

		if (single_pal) {
			auto	single = palette->ScanLine(0);
			for (auto&& i : slice_to(gradients2, num_single))
				*single++ = to_ISO_rgba(i->colours.colours[0].c);
		}

		if (num_unique - num_single) {
			uint32	y	= col_grads;
			for (auto&& i : slice(gradients2, num_single, num_unique - num_single))
				i->make(palette->ScanLineBlock(y++));
		}
	}

	// add layered glyphs
	uint32	index = num_glyphs;
	for (auto& i : fw->layers_map) {
		auto&		gi0		= glyph_infos[i.index()];
		gi0.layered			= true;
		gi0.indices_offset	= index;
		gi0.curves_offset	= i->size32();

		for (auto& j : *i)
			gi0.ext	|= get_extent(GetSlugPoints(j));

		float2x3	ext_mat		= translate(gi0.ext.a) * scale(gi0.ext.extent());

		for (auto& j : *i) {
			auto&	gi			= layer_infos[index - num_glyphs];
			uint32	gradient	= rev_indices[index - num_glyphs];

			if (gradient < num_single) {
				if (j.cols.colours.empty()) {
					gi.colour	= zero;
				} else if (single_pal) {
					gi.colour.x		= gradient % col_width;
					gi.colour.y		= gradient / col_width;
					gi.colour.z		= GradientTransform::SOLID;
					gi.colour.w		= zero;
				} else {
					gi.colour	= as<uint8>(j.cols.colours[0].c);
				}
			} else {
				auto	m		= ext_mat / j.grad.transform;
				float2	g[4]	= {m.x, m.y, m.z, j.grad.params};
				AppendISO(g, *curve_buffer);
				gi.colour.xy	= as<uint8>(uint16(((gradient - num_single + col_grads) * 0x10000 + 0x8000) / col_height));
				gi.colour.z		= j.grad.fill | (j.cols.extend << 4);
				gi.colour.w		= zero;
			}

			dynamic_array<float2> points = scale_with_extent(GetSlugPoints(j), gi0.ext);
			gi.curves_offset	= AppendISO(points, *curve_buffer);

			auto	indices		= MakeSlugBands(points, num_bands, &(*band_buffer)[index * num_bands * 2]);
			gi.indices_offset	= AppendISO(indices, *indices_buffer);
			++index;
		}
	}

	// add monochrome glyphs
	for (index = 0; index < num_glyphs; index++) {
		auto	glyph	= fw->glyphs[index];
		auto&	gi		= glyph_infos[index];

		gi.advance	= glyph.advance;
		
		if (!gi.layered && !glyph.empty()) {
			auto	points		= GetSlugPoints(glyph);
			gi.ext				= get_extent(points);

			decltype(points) points2		= scale_with_extent(points, gi.ext);
			gi.curves_offset	= AppendISO(points2, *curve_buffer);

			auto	indices		= MakeSlugBands(points2, num_bands, &(*band_buffer)[index * num_bands * 2]);
			gi.indices_offset	= AppendISO(indices, *indices_buffer);
		}
	}


	interval<float2>	ext = empty;
	float				maxadv = 0;
	for (auto& i : glyph_infos) {
		ISO_ASSERT(i.ext.a.y > -1e6f && i.ext.a.x > -1e6f);

		ext |= i.ext;
		maxadv = max(maxadv, i.advance);
	}
	
	auto	scale	= 255 / max(ext.extent(), float2{maxadv, fw->spacing});
	float	scale1	= reduce_min(scale);

	temp_array<SlugGlyphInfo>	glyph_infos2 = transformc(glyph_infos, [&](const glyph_info& g) {
		SlugGlyphInfo	gi;

		auto	p0	= (g.ext.a - ext.a) * scale1;
		gi.k	= p0.x;
		gi.y	= max(p0.y, -127);
		auto	p1	= g.ext.extent() * scale1;
		gi.w	= p1.x;
		gi.h	= p1.y;
		gi.s	= g.advance * scale1;
		gi.flags	= 0;

		if (g.layered) {
			gi.flags	|= GlyphInfo::LAYERED;
			gi.bands			= g.indices_offset;
			auto	gl			= (GlyphLayered*)&gi;
			gl->offset			= g.indices_offset;
			gl->num_layers		= g.curves_offset;
		} else {
			gi.bands			= glyph_infos.index_of(g);
			gi.indices_offset	= g.indices_offset;
			gi.curves_offset	= g.curves_offset;
		}

		return gi;
	});

	RootFontBank	bank;
	uint32	num_chars	= fw->max_char + 1;

	hash_map<uint32, char32>	glyphid_to_char;
	if (fw->ligatures) {
		for (auto &i : fw->glyph_map)
			glyphid_to_char[*i] = i.index();
	}

	for (auto &i : fw->glyph_map) {
		auto	c	= i.index();
		auto	g	= *i;
		//auto	g	= &i - fw->glyph_map.begin();
		auto	*gi	= &glyph_infos2[g];

		if (c < num_chars) {
			uint8	chars[8];
			gi->flags	|= GetGlyphFlags(c);
			if (c < 0xc0) {
				chars[0] = c;
				bank.Add(gi, chars, 1);
			}
			if (c >= 0x80)
				bank.Add(gi, chars, put_char(c, (char*)chars));
		} else {
			auto&	lig	= fw->ligatures[c - num_chars];
			string_builder	b;
			for (auto g : lig.b.glyphs)
				b << glyphid_to_char[g];
			bank.Add(gi, (uint8*)b.begin(), b.length());
		}
	}

	uint32	num_bank	= bank.Count();
	out->glyph_buffer.Create(num_bank + num_layers);
	bank.Write(out->glyph_buffer.begin());

	for (int i = 0; i < num_bank; i++) {
		auto&	gi = out->glyph_buffer[i];
		if (gi.flags & gi.LAYERED) {
			auto	gl = (GlyphLayered*)&gi;
			gl->offset = gl->offset - (i - num_bank + num_glyphs);
		}
	}

	SlugLayerInfo*	layer = (SlugLayerInfo*)(out->glyph_buffer.begin() + num_bank);
	for (auto &i : layer_infos) {
		layer->colour			= i.colour;
		layer->indices_offset	= i.indices_offset;
		layer->curves_offset	= i.curves_offset;
		++layer;
	}

	out->height			= fw->height	* scale1;
	out->baseline		= fw->baseline	* scale1;
	out->top			= ext.a.y		* scale1;
	out->spacing		= fw->spacing	* scale1;

	out->firstchar		= bank.FirstEntry();
	out->numchars		= bank.NumEntries();

	out->band_buffer	= band_buffer;
	out->indices_buffer	= indices_buffer;
	out->curve_buffer	= curve_buffer;
	out->palette		= palette;
	return out;
}

initialise font_initialise(
	ISO::getdef<TexFont>(),
	ISO::getdef<FontParams>(),
	ISO::getdef<FontParams2>(),
	ISO_get_cast(FontParamsBitmap),
	ISO_get_cast(FontBitmap),
	ISO_get_conversion(FontMaker1),
	ISO_get_conversion(FontMaker2),
	ISO::getdef<SlugFont>(),
	ISO_get_operation(SlugMaker)
);
