#include "graphics.h"
#include "font.h"
#include "common/shader.h"

namespace iso {

T_cc_control0<CC_CENTRE>		cc_centre;
T_cc_control0<CC_RIGHTJ>		cc_rightj;
T_cc_control0<CC_BACKSPACE>		cc_backspace;
T_cc_control0<CC_TAB>			cc_tab;
T_cc_control0<CC_LF>			cc_lf;
T_cc_control0<CC_CURSOR_DOWN>	cc_cursor_down;
T_cc_control0<CC_CURSOR_UP>		cc_cursor_up;
T_cc_control0<CC_ALIGN_LEFT>	cc_align_left;
T_cc_control0<CC_ALIGN_RIGHT>	cc_align_right;
T_cc_control0<CC_ALIGN_CENTRE>	cc_align_centre;
T_cc_control0<CC_ALIGN_JUSTIFY>	cc_align_justify;
T_cc_control0<CC_SET_INDENT>	cc_set_indent;
T_cc_control0<CC_NEWLINE>		cc_newline;
T_cc_control0<CC_RESTORECOL>	cc_restorecol;
T_cc_control0<CC_SOFTRETURN>	cc_softreturn;

#ifdef PLAT_WII
struct FontVertex {
	float2p	pos;
	rgba8	col;
	float2p	uv;
};
#elif defined PLAT_PS4
struct FontVertex {
	float2p	pos;
	rgba8	col;
	array_vec<uint16,2>	uv;
};
#else
struct FontVertex {
	float2p	pos;
	float4p	col;
	float2p	uv;
};
#endif

FontVertex operator-(const FontVertex &a, const FontVertex &b) {
	FontVertex	v = a;
	v.pos	-= b.pos;
	v.uv	-= b.uv;
	return v;
}
FontVertex operator+(const FontVertex &a, const FontVertex &b) {
	FontVertex	v	= a;
	v.pos	+= b.pos;
	v.uv	+= b.uv;
	return v;
}


#undef ISO_HAS_RECTS
#ifdef ISO_HAS_RECTS
#define PrimType	RectListT
#else
#define PrimType	RectListT
//#define PrimType	QuadListT
#endif

template<> VertexElements GetVE<FontVertex>() {
	static VertexElement ve[] = {
		VertexElement(&FontVertex::pos,	"position"_usage),
		VertexElement(&FontVertex::col,	"colour"_usage),
		VertexElement(&FontVertex::uv,	"texcoord"_usage)
	};
	return ve;
};

template<template<class> class T> inline FontVertex *Put(T<FontVertex> p, param(float2) pos1, param(float2) pos2, param(float2) uv1, param(float2) uv2, param(colour) col) {
	p[0].pos = pos1;
	p[1].pos.set(pos2.x, pos1.y);
	p[2].pos.set(pos1.x, pos2.y);
	p[3].pos = pos2;

	p[0].uv = uv1;
	p[1].uv.set(uv2.x, uv1.y);
	p[2].uv.set(uv1.x, uv2.y);
	p[3].uv = uv2;

	p[0].col = p[1].col = p[2].col = p[3].col = col;
	return p.next();
}

template<template<class> class T> inline FontVertex *Put(T<FontVertex> p, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float shear, param(colour) col) {
	p[0].pos = float2{x0 + shear, y0};
	p[1].pos = float2{x1 + shear, y0};
	p[2].pos = float2{x0, y1};
	p[3].pos = float2{x1, y1};

	p[0].uv = float2{u0, v0};
	p[1].uv = float2{u1, v0};
	p[2].uv = float2{u0, v1};
	p[3].uv = float2{u1, v1};

	p[0].col = p[1].col = p[2].col = p[3].col = col.rgba;
	return p.next();
}

//#ifdef ISO_HAS_RECTS
inline FontVertex *Put(RectListT<FontVertex> p, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, float shear, param(colour) col) {
	p[0].pos = float2{x0 + shear, y0};
	p[1].pos = float2{x1 + shear, y0};
	p[2].pos = float2{x0, y1};

	p[0].uv = float2{u0, v0};
	p[1].uv = float2{u1, v0};
	p[2].uv = float2{u0, v1};

	p[0].col = p[1].col = p[2].col = col.rgba;
	return p.next();
}
//#endif

uint8 Font::format_extra[32] = {
	1,4,2,2,2,1,1,2,		// 0..7
	1,1,1,1,1,1,3,1,		// 8..15
	1,1,1,2,1,1,1,2,		//16..23
	2,1,2,1,1,1,1,1			//24..31
};
uint8 cont_chars[32] = {
	0,4,2,2,0,0,0,2,		// 0..7
	0,0,0,1,1,0,0,0,		// 8..15
	0,0,0,0,0,0,1,2,		//16..23
	2,0,1,0,0,0,0,1			//24..31
};

//point Font::TexSize()	const { return ((Texture&)tex).Size(); }

const GlyphInfo* Font::GetGlyph(const GlyphInfo *glyphs, const char *&string, int replacement_c) const {
	int	c = (uint8)*string++;
	const GlyphInfo	*gi		= GetGlyph(glyphs, c);

	while (gi && (gi->flags & GLYPH_TABLE)) {
		int	c	= (uint8)*string++;
		gi		= (c >= gi->f && c - gi->f < gi->n) ? &glyphs[gi->offset + c - gi->f] : NULL;
	}
	
	if (!gi || (gi->w == 0 && gi->s == 0)) {
		if ((c & 0xc0) == 0xc0) {
			while ((*string & 0xc0) == 0x80)
				string++;
		}
		if (replacement_c)
			gi = GetGlyph(glyphs, replacement_c);
	}
	return gi;
}

float Font::Width(const char *string, int len) const {
	int		c;
	float	total	= 0, totalw = 0;
	float	scale	= 1;
	const GlyphInfo	*glyphs	= get(glyph_info);

	while (len-- && (c = (uint8)*string)) {
		if (c < ' ') {
			if (int x = cont_chars[c]) {
				if (c == CC_SCALE)
					scale = uint8(string[1]) / 16.f;
				string += x;
			} else {
				break;
			}
		} else if (const GlyphInfo *gi = GetGlyph(glyphs, string)) {
			if (gi->w)
				totalw	= total + (gi->k + gi->w) * scale;
			total += gi->s * scale;
		}
	}
	return totalw;
}

float Font::Width(const char *string, float width, float char_scale, const char **linebreak, bool linebreakwords, int *numspaces) const {
	float		total	= 0;
	float		totalw	= 0,	spacetotalw	= 0;
	float		scale	= 1;
	int			pflags	= 0;
	int			spaces	= 0,	spaces2		= 0, spaces3 = 0;
	const char	*space	= NULL;

	if (linebreak)
		*linebreak = NULL;

	const GlyphInfo	*glyphs	= get(glyph_info);
	while (int c = (uint8)*string) {

		if (c < ' ') {
			if (cont_chars[c] == 0)
				break;
			if (c == CC_SCALE)
				scale = uint8(string[1]) / 16.f;
			string += cont_chars[c];
			continue;
		}

		if (const GlyphInfo *gi = GetGlyph(glyphs, string)) {

			if (gi->w) {
				totalw	= total + (gi->k + gi->w) * scale;
				spaces2	= spaces;

				if (width && totalw > width && space) {
					if (linebreak)
						*linebreak = space;
					totalw	= spacetotalw;
					break;
				}
			} else if (gi->s) {
				spaces++;
			}

			total += gi->s * char_scale * scale;

			if (width && (!linebreakwords || (gi->flags & GLYPH_BREAKABLE)) && !(pflags & GLYPH_NOT_EOL)) {
				const char	*string2 = string;
				const GlyphInfo *gi2 = GetGlyph(glyphs, string2);
				if (!gi2 || !(gi2->flags & GLYPH_NOT_SOL)) {
					space		= string;
					spacetotalw	= totalw;
					spaces3		= spaces2;
				}
			}

			pflags	= gi->flags;
		}

	}

	if (numspaces)
		*numspaces = spaces3;
	return totalw;
}

int Font::Length(const char *string) {
	int count = 0;
	while (char c = *string) {
		if (int skip = IsControlChar(c)) {
			string += skip;
		} else {
			if ((c & 0xc0) == 0xc0)
				while ((*++string & 0xc0) == 0x80);
			++count;
		}
	}
	return count;
}

int Font::CountRects(const char *string, const char *end) const {
	const GlyphInfo	*glyphs	= get(glyph_info);
	const GlyphInfo	*gi;
	int				c, count = 0;
	while (string < end && (c = (uint8)*string)) {
		if (int skip = IsControlChar(c))
			string += skip;
		else if ((gi = GetGlyph(glyphs, string)) && gi->w)
			count++;
	}
	return count;
}

//-----------------------------------------------------------------------------
//	FontPrinter
//-----------------------------------------------------------------------------

struct FontPrinterState : FontPrinterFixed, FontPrinterOverridden {
	colour		curr_col;
	float		lspacing;
	float		pspacing;
	float		prevs;
	float		spacewidth;
	float		x, y, x1;
	const char	*linebreak;

	void		AddGlyph(const GlyphInfo *gi) {
		x += (prevs = (gi->s * scale + (gi->w == 0 && gi->s ? spacewidth : 0)) * char_scale);
	}

	float		AlignText(const char *string, float width, float x, bool full) {
		spacewidth = 0;
		if (width)
			width = (width - x) / scale;
		switch (align) {
			case FA_RIGHT:	return full || width ? (width - font->Width(string, width, char_scale, &linebreak)) * scale : 0;
			case FA_CENTRE:	return full || width ? (width - font->Width(string, width, char_scale, &linebreak)) * scale / 2.f : 0;
			case FA_JUSTIFY:
				if (width) {
					int		numspaces;
					float	w = font->Width(string, width, char_scale, &linebreak, true, &numspaces);
					spacewidth = numspaces ? (width - w) * scale / numspaces : 0;
				}
				return 0;
			default:
				if (width)
					font->Width(string, width, char_scale, &linebreak);
				return 0;
		}
	}


	void		Begin(const char *string, bool full) {
		x = x1 + AlignText(string, width, x1 - x0, full);
	}
	void		LineBreak(const char *string, bool full) {
		y += lspacing * scale;
		Begin(string, full);
	}

	const char*	SkipLine(const char *string, bool full);
	const char*	SkipTo(const char *string, const char *end, float miny, bool full);
	int			CountRectsTo(const char *string, const char *end, float maxy);
	void		ProcessControl(const char *string, bool full);

	FontPrinterState(const FontMeasurer *fp) : FontPrinterFixed(*fp), FontPrinterOverridden(*fp)
		, curr_col(fp->col)
		, lspacing(font->spacing * line_scale), pspacing(font->spacing * para_scale), prevs(0), spacewidth(0)
		, x(fp->x), y(fp->y), x1(fp->x0)
		, linebreak(0)
	{}
};

const char *FontPrinterState::SkipLine(const char *string, bool full) {
	y	+= (linebreak ? lspacing : pspacing) * scale;
	while (int c = (uint8)*string) {
		if (int skip = Font::IsControlChar(c)) {
			if (c == CC_SOFTRETURN || c ==  CC_LF) {
				string += skip;
				break;
			}
			ProcessControl(string, full);
			string += skip;

		} else if (auto gi = font->GetGlyph(string)) {
			AddGlyph(gi);
		}

		if (string == linebreak)
			break;
	}

	x = x0 + AlignText(string, width, x0, full);
	return string;
}

const char *FontPrinterState::SkipTo(const char *string, const char *end, float miny, bool full) {
	miny += font->baseline - font->height;
#if 0
	while (y < miny)
		string	= SkipLine(string, full);
#else
	int	c;
	while (y < miny && string < end && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
			ProcessControl(string, false);
			string += skip;

		} else if (auto gi = font->GetGlyph(string)) {
			AddGlyph(gi);

		}
		if (string == linebreak)
			LineBreak(string, false);
	}
#endif
	return string;
}

int FontPrinterState::CountRectsTo(const char *string, const char *end, float maxy) {
#if 0
	static const uint32	ignore	=
		(1<<CC_COLOUR)		| (1<<CC_HMOVE)			| (1<<CC_HSET)
	|	(1<<CC_CENTRE)		| (1<<CC_RIGHTJ)		| (1<<CC_BACKSPACE)
//	|	(1<<CC_TAB)			| (1<<CC_SET_TAB)
	|	(1<<CC_SET_INDENT)	| (1<<CC_RESTORECOL)	| (1<<CC_ALPHA);
#endif
	int			c, count = 0;

	maxy += font->baseline;
	while (string < end && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
//			if (!(ignore & (1<<c)))
				ProcessControl(string, false);
			string += skip;

		} else if (auto gi = font->GetGlyph(string)) {
			if (y > maxy)
				break;
			if (gi->w)
				count++;
			AddGlyph(gi);
		}
		if (string == linebreak)
			LineBreak(string, false);
	}
	return count;
}

void FontPrinterState::ProcessControl(const char *string, bool full) {
	switch (string[0]) {
		case CC_COLOUR:	{	// set colour
			int r = uint8(string[1]), g = uint8(string[2]), b = uint8(string[3]);
			if (!(flags.test(F_COL_IGNORE))) {
				float3 rgb = float3{float(r), float(g), float(b)} / 255.0f;
				if (flags.test(F_COL_MUL))
					curr_col.rgb = col.rgb * rgb;
				else
					curr_col.rgb = rgb;
			}
			break;
		}
		case CC_ALPHA: {
			float	a = (uint8(string[1]) - 1) / 254.0f;
			curr_col.a = flags.test(F_COL_MUL) ? float(col.a) * a : a;
			break;
		}
		case CC_HMOVE:		x += string[1];											break;	// horizontal move
		case CC_VMOVE:		y += string[1];											break;	// vertical move
		case CC_HSET:		x  = x0 + uint8(string[1]);								break;	// horizontal offset from margin
		case CC_VSET:		y  = y0 + uint8(string[1]);								break;	// vertical offset from top
		case CC_CENTRE:		x -= font->Width(string + 1, width, char_scale, &linebreak) / 2.f;	break;	// centre
		case CC_RIGHTJ:		x -= font->Width(string + 1, width, char_scale, &linebreak);		break;	// right justify
		case CC_BACKSPACE:	x -= prevs;												break;	// backspace
		case CC_SET_TAB:	tabstop = uint8(string[1]);								break;	// set tab width
		case CC_SET_INDENT: x1 = x;													break;	// set indent
		case CC_SET_WIDTH:	width = float(uint8(string[1]) | (uint8(string[2])<<8));break;
		case CC_RESTORECOL:	curr_col.rgb = col.rgb;									break;
		case CC_SCALE:		scale = uint8(string[1]) / 16.f;						break;	// set scale
		case CC_CURSOR_DOWN:y += lspacing * scale;									break;	// cursor down
		case CC_CURSOR_UP:	y -= lspacing * scale;									break;	// cursor up

		case CC_LINESPACING:
			lspacing	= uint8(string[1]) * line_scale;
			pspacing	= uint8(string[1]) * para_scale;
			break;

		case CC_TAB:
			x1	= x0 + (int((x - x0 + tabstop) / tabstop) * tabstop);
			Begin(string + 1, full);
			break;

		case CC_SOFTRETURN:
			y	+= lspacing * scale;
			Begin(string + 1, full);
			break;

		case CC_LF:
			y += pspacing * scale; x1 = x0;
		case CC_NEWLINE:
			x = x0 + AlignText(string + 1, width, x0, full);
			break;

		case CC_ALIGN_LEFT:
			align = FA_LEFT;
			x += AlignText(string + 1, width, x, full);
			break;

		case CC_ALIGN_RIGHT:
			align = FA_RIGHT;
			x += AlignText(string + 1, width, x, full);
			break;

		case CC_ALIGN_CENTRE:
			align = FA_CENTRE;
			x = x1 + AlignText(string + 1, width, x1, full);
			break;

		case CC_ALIGN_JUSTIFY:
			align = FA_JUSTIFY;
			x += AlignText(string + 1, width, x, full);
			break;
	}
}

int FontMeasurer::CountRects(const char *string, const char *end) const {
	if (!flags.test(F_VCLIP))
		return font->CountRects(string, end);

	if (maxy <= miny)
		return 0;

	FontPrinterState	state(this);
	state.Begin(string, false);

	string = state.SkipTo(string, end, miny, false);
	return state.CountRectsTo(string, end, maxy);
}

float2 FontMeasurer::CalcRect(const char *string, int len) const {
	FontPrinterState	state(this);
	state.x0 = state.x = 0;
	state.y0 = state.y = font->height;

	state.Begin(string, false);

	int		c;
	float	w = 0, h = 0;

	uint32	ignore = (1<<CC_COLOUR) | (1<<CC_RESTORECOL) | (1<<CC_ALPHA);

	while (len-- && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
			if (!(ignore & (1<<c)))
				state.ProcessControl(string, false);
			string += skip;

		} else if (auto gi = font->GetGlyph(string)) {
			if (gi->w) {
				w = max(w, state.x + (gi->k + gi->w));
				h = max(h, state.y);
			}
			state.AddGlyph(gi);
		}
		if (string == state.linebreak)
			state.LineBreak(string, false);
	}
	return {w, h};
}

FontVertex *FontPrinter::Print(FontVertex *p, const char *string, const char *end) {
	float		h			= font->height;
	float		b			= font->baseline;
	float		cliptop		= 0, clipbot	= 0;
	float		clipleft	= 0, clipright	= 0;
	int			c;

	FontPrinterState	state(this);
	state.Begin(string, true);

	float		miny, maxy;
	bool		vclip = flags.test(F_VCLIP);
	if (vclip) {
		miny = this->miny;
		maxy = this->maxy;
		if (maxy <= miny)
			return p;
		string = state.SkipTo(string, end, miny, true);
	}

	float		minx, maxx;
	bool		hclip = flags.test(F_HCLIP);
	if (hclip) {
		minx = this->minx;
		maxx = this->maxx;
	}

	while (string < end && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
			state.ProcessControl(string, true);
			string += skip;

		} else if (auto gi = font->GetGlyph(string)) {
			if (gi->w) {
				float	x = state.x, y = state.y, scale = state.scale;
				if (vclip) {
					cliptop = min(y - b - miny,		0.f);
					clipbot = max(y - b + h - maxy, 0.f);
					if (clipbot > h)
						break;
				}
				float	k = gi->k, w = gi->w;
				if (hclip) {
					clipleft	= clamp(x + k - minx,		-w,	0.f);
					clipright	= clamp(x + k + w - maxx,	0.f, w);
				}
				p = Put<PrimType>(p,
					x + (	 k	- clipleft)		* scale,
					y + (	-b	- cliptop)		* scale,
					x + (w + k	- clipright)	* scale,
					y + (h - b	- clipbot)		* scale,
					gi->u		- clipleft,
					gi->v		- cliptop,
					gi->u + w	- clipright,
					gi->v + h	- clipbot,
					shear,
					state.curr_col
				);
			}
			state.AddGlyph(gi);
		}
		if (string == state.linebreak)
			state.LineBreak(string, true);
	}
	x = state.x;
	y = state.y;
	return p;
}

FontPrinter	&FontPrinter::Print(const char *string, int len) {
/*	FontPrinterState	state(this);
	if (flags & F_VCLIP) {
		if (maxy <= miny)
			return *this;
		string = state.SkipTo(string, miny, true);
	}
*/
	const char	*end	= len < 0 ? (char*)intptr_t(-1) : string + len;
	if (int n = CountRects(string, end)) {
		ImmediateStream<FontVertex>	im(ctx, prim<PrimType>(), verts<PrimType>(n));
		FontVertex	*p = im.begin();
		size_t		n2 = Print(p, string, end) - p;
		ISO_VERIFY(n2 == verts<PrimType>(n));
	}
	return *this;
}

FontPrinter	&FontPrinter::PrintF(const char *format, ...) {
	va_list valist;
	va_start(valist, format);
	fixed_string<512>	s;
	return Print(s, int(_format(s, 512, format, valist)));
}

} //namespace iso
