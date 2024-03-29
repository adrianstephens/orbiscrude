#ifndef FONT_H
#define FONT_H

#include "base/strings.h"
#include "base/pointer.h"
#include "packed_types.h"

namespace ISO {
template<typename T, int B> class OpenArray;
}

namespace iso {

class GraphicsContext;

//-----------------------------------------------------------------------------
// formatting codes
//-----------------------------------------------------------------------------

enum FontAlign {
	FA_LEFT,
	FA_RIGHT,
	FA_CENTRE,
	FA_JUSTIFY,
};

enum {
	CC_COLOUR		= 1,	// + r, g, b
	CC_HMOVE		= 2,	// + s8
	CC_VMOVE		= 3,	// + s8
	CC_HSET			= 4,	// + u8
	CC_CENTRE		= 5,
	CC_RIGHTJ		= 6,
	CC_VSET			= 7,	// + u8
	CC_BACKSPACE	= 8,
	CC_TAB			= 9,
	CC_LF			= 10,
	CC_CURSOR_DOWN	= 11,
	CC_CURSOR_UP	= 12,
	CC_CR			= 13,
	CC_SET_WIDTH	= 14,	// + u16
	_CC_ALIGN		= 15,

	CC_ALIGN_LEFT	= _CC_ALIGN,
	CC_ALIGN_RIGHT	= 16,
	CC_ALIGN_CENTRE	= 17,
	CC_ALIGN_JUSTIFY= 18,

	CC_SET_TAB		= 19,	// +u8
	CC_SET_INDENT	= 20,
	CC_NEWLINE		= 21,
	CC_RESTORECOL	= 22,
	CC_SCALE		= 23,	// +u8
	CC_ALPHA		= 24,	// +u8
	CC_SOFTRETURN	= 25,
	CC_LINESPACING	= 26,

	CC_NOP			= 31,
};

template<int C, typename...P> struct T_cc_control {
	tuple<packed<P>...>	p;
	T_cc_control(const P&...p) : p(p...) {}
	friend string_accum& operator<<(string_accum &a, const T_cc_control &c) {
		int		n	= sizeof(c.p) + 1;
		auto	s	= a.getp(n);
		s[0] = C;
		*(tuple<packed<P>...>*)(s + 1) = c.p;
		return a;
	}
};

extern T_cc_control<CC_CENTRE>				cc_centre;
extern T_cc_control<CC_RIGHTJ>				cc_rightj;
extern T_cc_control<CC_BACKSPACE>			cc_backspace;
extern T_cc_control<CC_TAB>					cc_tab;
extern T_cc_control<CC_LF>					cc_lf;
extern T_cc_control<CC_CURSOR_DOWN>			cc_cursor_down;
extern T_cc_control<CC_CURSOR_UP>			cc_cursor_up;
extern T_cc_control<CC_ALIGN_LEFT>			cc_align_left;
extern T_cc_control<CC_ALIGN_RIGHT>			cc_align_right;
extern T_cc_control<CC_ALIGN_CENTRE>		cc_align_centre;
extern T_cc_control<CC_ALIGN_JUSTIFY>		cc_align_justify;
extern T_cc_control<CC_SET_INDENT>			cc_set_indent;
extern T_cc_control<CC_NEWLINE>				cc_newline;
extern T_cc_control<CC_RESTORECOL>			cc_restorecol;
extern T_cc_control<CC_SOFTRETURN>			cc_softreturn;

typedef T_cc_control<CC_HMOVE,		uint8>	cc_hmove;
typedef T_cc_control<CC_VMOVE,		uint8>	cc_vmove;
typedef T_cc_control<CC_HSET,		uint8>	cc_hset;
typedef T_cc_control<CC_VSET,		uint8>	cc_vset;
typedef T_cc_control<CC_SET_TAB,	uint8>	cc_set_tab;
typedef T_cc_control<CC_LINESPACING,uint8>	cc_linespacing;

typedef T_cc_control<CC_SET_WIDTH,	uint16>				cc_set_width;
typedef T_cc_control<CC_SCALE,		scaled<uint8,16>>	cc_scale;
typedef T_cc_control<CC_ALPHA,		unorm8>				cc_alpha;
typedef T_cc_control<CC_COLOUR,		rgb8>				cc_colour;

//inline size_t	to_string(char *s, FontAlign a)	{ s[0] = CC_ALIGN_LEFT + a; return 1; }
inline string_accum& operator<<(string_accum &a, FontAlign x) { return a.putc(_CC_ALIGN + x); }

//-----------------------------------------------------------------------------
// font structures
//-----------------------------------------------------------------------------

struct _GlyphInfo {
	uint8		w, s;
	uint8		flags;
	int8		k;
};

struct GlyphTable {
	uint8		n, f;
	uint8		flags2;
	uintn<3>	offset;
};

struct GlyphLayered : _GlyphInfo {
	uintn<3>	offset;
	uint8		num_layers;
};

struct GlyphInfo : _GlyphInfo {
	enum {
		BREAKABLE	= 1 << 0,
		NOT_SOL		= 1 << 1,
		NOT_EOL		= 1 << 2,
		PACKED		= 1 << 6,
		LAYERED		= 1 << 6,
		TABLE		= 1 << 7,
	};
	auto	get_table()		const { return flags & TABLE ? (const GlyphTable*)this : nullptr; }
	auto	num_layers()	const { return w ? (flags & LAYERED ? ((const GlyphLayered*)this)->num_layers : 1) : 0; }
};


struct Glyphs {
	stride_iterator<const GlyphInfo> glyphs;
	uint8			f, n;
	const GlyphInfo*	GetGlyph(int c)	 const { return c >= f && c - f < n ? &glyphs[c - f] : nullptr; }
	const GlyphInfo*	GetGlyph(const char *&string, int replacement_c = '_') const;
	int					CountRects(const char *string, const char *end) const;
	float				Width(const char *string, int len = -1) const;
	float				Width(const char *string, float width, float char_scale, const char **linebreak, bool linebreakwords = true, int *numspaces = 0) const;
};

struct Font {
	static uint8	format_extra[32];
	static int		IsControlChar(uint8 c) { return c < num_elements(format_extra) ? format_extra[c] : 0; }
	static int		Length(const char *string);

	uint8			height, baseline, top, spacing;
};

class Texture;
class DataBuffer;
void Init(Texture*,void*);
void DeInit(Texture*);


struct TexGlyphInfo : GlyphInfo {
	uint16		u, v;
};

struct TexFont : Font {
	uint8			firstchar, numchars;
	uint8			outline;
//	uint8			unused;
	_read_as<ISO::OpenArray<TexGlyphInfo,32>, pointer32<TexGlyphInfo>> glyph_info;
	_read_as<Texture,void*>		tex;

	auto&		Tex()		const	{ return tex.cast(); }
	Glyphs		GetGlyphs()	const	{ return {get(get(glyph_info)), firstchar, numchars};}
	void		Start(GraphicsContext &ctx);
};


struct SlugGlyphInfo : GlyphInfo {
	uint32		indices_offset, curves_offset;
	uint16		bands;
	int8		y;
	uint8		h;
};

struct SlugLayerInfo {
	uint8x4		colour;
	uint32		indices_offset, curves_offset;
	uint16		unused_gradient;
	int8		unused_y;
	uint8		unused_h;
};


struct SlugFont : Font {
	uint8	firstchar, numchars;

	_read_as<ISO::OpenArray<SlugGlyphInfo,32>, pointer32<SlugGlyphInfo>> glyph_buffer;
	_read_as<DataBuffer,void*>	band_buffer;
	_read_as<DataBuffer,void*>	indices_buffer;
	_read_as<DataBuffer,void*>	curve_buffer;
	_read_as<Texture,void*>		palette;

	Glyphs		GetGlyphs()	const	{ return {get(get(glyph_buffer)), firstchar, numchars};}
	void		Start(GraphicsContext &ctx);
};

//-----------------------------------------------------------------------------
// printing
//-----------------------------------------------------------------------------

struct FontPrinterFixed {
	enum FLAGS {
		F_COL_IGNORE	= 0x1,
		F_COL_MUL		= 0x2,
		F_HCLIP			= 0x4,
		F_VCLIP			= 0x8,
		F_CLIP			= 0xC
	};

	Glyphs				glyphs;
	iso::flags<FLAGS>	flags		= 0;
	float				x0			= 0, y0 = 0;
	colour				col			= colour(float4(one));
	float				line_scale	= 1;
	float				para_scale	= 1;
	float				char_scale	= 1;
	uint8				height, baseline, spacing;

	FontPrinterFixed(const Font *font, Glyphs glyphs) : glyphs(glyphs), height(font->height), baseline(font->baseline), spacing(font->spacing) {}
};

struct FontPrinterOverridden {
	float		width, tabstop, scale;
	FontAlign	align;
	FontPrinterOverridden() : width(0), tabstop(64), scale(1), align(FA_LEFT)	{}
};

class FontMeasurer : protected FontPrinterFixed, protected FontPrinterOverridden {
friend struct FontPrinterState;
protected:
	float		x = 0, y = 0;
	float		shear = 0;
	float		minx, miny, maxx, maxy;

public:
	FontMeasurer(const TexFont *font)	: FontPrinterFixed(font, font->GetGlyphs()) {}
	FontMeasurer(const SlugFont *font)	: FontPrinterFixed(font, font->GetGlyphs()) {}

	float			GetX()						const	{ return x;		}
	float			GetY()						const	{ return y;		}
//	const Font*		GetFont()					const	{ return font;	}

	FontMeasurer	&At(float _x, float _y)				{ x = x0 = _x; y = y0 = _y;	return *this; }
//	FontMeasurer	&SetFont(Font *_font)				{ font			= _font;	return *this; }
	FontMeasurer	&SetAlign(FontAlign _align)			{ align			= _align;	return *this; }
	FontMeasurer	&SetColour(const colour &_col)		{ col			= _col;		return *this; }
	FontMeasurer	&SetWidth(float _width)				{ width			= _width;	return *this; }
	FontMeasurer	&SetTabs(float _tabstop)			{ tabstop		= _tabstop;	return *this; }
	FontMeasurer	&SetLine(float v)					{ line_scale	= v;		return *this; }
	FontMeasurer	&SetParagraph(float v)				{ para_scale	= v;		return *this; }
	FontMeasurer	&SetScale(float _scale)				{ scale			= _scale;	return *this; }
	FontMeasurer	&SetSpacing(float _scale)			{ char_scale	= _scale;	return *this; }
	FontMeasurer	&SetShear(float _shear)				{ shear			= _shear;	return *this; }
	FontMeasurer	&SetFlags(unsigned _flags)			{ flags.set_all(_flags);	return *this; }
	FontMeasurer	&ClearFlags(unsigned _flags)		{ flags.clear_all(_flags);	return *this; }

	FontMeasurer	&SetClip0(float _minx, float _miny, float _maxx, float _maxy) {
		minx = _minx;
		miny = _miny;
		maxx = _maxx;
		maxy = _maxy;
		return *this;
	}
	FontMeasurer	&SetClip(float _minx, float _miny, float _maxx, float _maxy) {
		flags.set(F_CLIP);
		return SetClip0(_minx, _miny, _maxx, _maxy);
	}

	int			CountRects(const char *string, const char *end)		const;
	float2		CalcRect(const char *string, int len = -1)			const;
};

class FontPrinter : public FontMeasurer {
protected:
	GraphicsContext &ctx;
	bool			slug;

	template<int N>	class FontPrinterAcc : public buffer_accum<N> {
		FontPrinter		&fp;
	 public:
		//FontPrinterAcc(FontPrinterAcc &&a);
		FontPrinterAcc(FontPrinter &fp) : fp(fp)	{}
		~FontPrinterAcc() { fp.Print(begin(), int(length())); }
	};

	template<typename I> void			Print(I &i, const GlyphInfo *gi, float2 pos, float scale, colour col);
	template<typename I> void			Print(I &i, const char *string, const char *end);
	template<typename T> FontPrinter&	Print(const char *string, int len = -1);

public:
	FontPrinter(GraphicsContext &ctx, const TexFont *font)	: FontMeasurer(font), ctx(ctx), slug(false) {}
	FontPrinter(GraphicsContext &ctx, const SlugFont *font)	: FontMeasurer(font), ctx(ctx), slug(true) {}

	FontPrinter	&At(float x, float y)				{ FontMeasurer::At(x, y);			return *this; }
//	FontPrinter	&SetFont(Font *font)				{ FontMeasurer::SetFont(font);		return *this; }
	FontPrinter	&SetAlign(FontAlign align)			{ FontMeasurer::SetAlign(align);	return *this; }
	FontPrinter	&SetColour(const colour &col)		{ FontMeasurer::SetColour(col);		return *this; }
	FontPrinter	&SetWidth(float width)				{ FontMeasurer::SetWidth(width);	return *this; }
	FontPrinter	&SetTabs(float tabstop)				{ FontMeasurer::SetTabs(tabstop);	return *this; }
	FontPrinter	&SetLine(float v)					{ FontMeasurer::SetLine(v);			return *this; }
	FontPrinter	&SetParagraph(float v)				{ FontMeasurer::SetParagraph(v);	return *this; }
	FontPrinter	&SetScale(float scale)				{ FontMeasurer::SetScale(scale);	return *this; }
	FontPrinter	&SetSpacing(float scale)			{ FontMeasurer::SetSpacing(scale);	return *this; }
	FontPrinter	&SetShear(float shear)				{ FontMeasurer::SetShear(shear);	return *this; }
	FontPrinter	&SetFlags(unsigned flags)			{ FontMeasurer::SetFlags(flags);	return *this; }
	FontPrinter	&ClearFlags(unsigned flags)			{ FontMeasurer::ClearFlags(flags);	return *this; }
	FontPrinter	&SetClip0(float minx, float miny, float maxx, float maxy)	{ FontMeasurer::SetClip0(minx, miny, maxx, maxy);	return *this; }
	FontPrinter	&SetClip(float minx, float miny, float maxx, float maxy)	{ FontMeasurer::SetClip(minx, miny, maxx, maxy);	return *this; }

	FontPrinter&	Print(const char *string, int len = -1);
	FontPrinter&	PrintF(const char *format, ...);

	template<int N>	FontPrinterAcc<N>	Begin()		{ return *this; }
					FontPrinterAcc<512>	Begin()		{ return *this; }
};


}//namespace iso

#endif	// FONT_H
