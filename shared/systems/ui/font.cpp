#include "graphics.h"
#include "font.h"
#include "common/shader.h"

namespace iso {

T_cc_control<CC_CENTRE>			cc_centre;
T_cc_control<CC_RIGHTJ>			cc_rightj;
T_cc_control<CC_BACKSPACE>		cc_backspace;
T_cc_control<CC_TAB>			cc_tab;
T_cc_control<CC_LF>				cc_lf;
T_cc_control<CC_CURSOR_DOWN>	cc_cursor_down;
T_cc_control<CC_CURSOR_UP>		cc_cursor_up;
T_cc_control<CC_ALIGN_LEFT>		cc_align_left;
T_cc_control<CC_ALIGN_RIGHT>	cc_align_right;
T_cc_control<CC_ALIGN_CENTRE>	cc_align_centre;
T_cc_control<CC_ALIGN_JUSTIFY>	cc_align_justify;
T_cc_control<CC_SET_INDENT>		cc_set_indent;
T_cc_control<CC_NEWLINE>		cc_newline;
T_cc_control<CC_RESTORECOL>		cc_restorecol;
T_cc_control<CC_SOFTRETURN>		cc_softreturn;

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

const GlyphInfo* Glyphs::GetGlyph(const char *&string, int replacement_c) const {
	int	c = (uint8)*string++;
	auto	gi		= GetGlyph(c);
	const GlyphTable	*table;

	while (gi && (table = gi->get_table())) {
		int	c	= (uint8)*string++;
		gi		= (c >= table->f && c - table->f < table->n) ? &glyphs[table->offset + c - table->f] : NULL;
	}
	
	if (!gi || (gi->w == 0 && gi->s == 0)) {
		if ((c & 0xc0) == 0xc0) {
			while ((*string & 0xc0) == 0x80)
				string++;
		}
		if (replacement_c)
			gi = GetGlyph(replacement_c);
	}
	return gi;
}

float Glyphs::Width(const char *string, int len) const {
	int		c;
	float	total	= 0, totalw = 0;
	float	scale	= 1;

	while (len-- && (c = (uint8)*string)) {
		if (c < ' ') {
			if (int x = cont_chars[c]) {
				if (c == CC_SCALE)
					scale = uint8(string[1]) / 16.f;
				string += x;
			} else {
				break;
			}
		} else if (auto gi = GetGlyph(string)) {
			if (gi->w)
				totalw	= total + (gi->k + gi->w) * scale;
			total += gi->s * scale;
		}
	}
	return totalw;
}

float Glyphs::Width(const char *string, float width, float char_scale, const char **linebreak, bool linebreakwords, int *numspaces) const {
	float		total	= 0;
	float		totalw	= 0,	spacetotalw	= 0;
	float		scale	= 1;
	int			pflags	= 0;
	int			spaces	= 0,	spaces2		= 0, spaces3 = 0;
	const char	*space	= NULL;

	if (linebreak)
		*linebreak = NULL;

	while (int c = (uint8)*string) {

		if (c < ' ') {
			if (cont_chars[c] == 0)
				break;
			if (c == CC_SCALE)
				scale = uint8(string[1]) / 16.f;
			string += cont_chars[c];
			continue;
		}

		if (auto gi = GetGlyph(string)) {
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

			if (width && (!linebreakwords || (gi->flags & TexGlyphInfo::BREAKABLE)) && !(pflags & TexGlyphInfo::NOT_EOL)) {
				const char	*string2 = string;
				auto	gi2 = GetGlyph(string2);
				if (!gi2 || !(gi2->flags & TexGlyphInfo::NOT_SOL)) {
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

int Glyphs::CountRects(const char *string, const char *end) const {
	int				c, count = 0;
	while (string < end && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
			string += skip;
		} else if (auto gi = GetGlyph(string)) {
			count += gi->num_layers();
		}
	}
	return count;
}

//-----------------------------------------------------------------------------
//	FontPrinter
//-----------------------------------------------------------------------------

#ifdef PLAT_WII
struct TexFontVertex {
	float2p	pos;
	rgba8	col;
	float2p	uv;
};
#elif defined PLAT_PS4
struct TexFontVertex {
	float2p	pos;
	rgba8	col;
	array_vec<uint16,2>	uv;
};
#else
struct TexFontVertex {
	float2p	pos;
	float4p	col;
	float2p	uv;
};
#endif

struct TexFontVertexGeom {
	float2p		pos;
	float2p		size;
	float4p		uv;
	float4p		col;
};

struct SlugFontVertex {
	float2p		pos;
	float4p		col;
	float2p		uv;
	uint3p		offsets;
};

struct SlugFontVertexGeom {
	float2p		pos;
	float4p		col;
	float2p		size;
	uint3p		offsets;
};

#define PrimType	QuadList

template<> const VertexElements ve<TexFontVertex> = (const VertexElement[]) {
	VertexElement(&TexFontVertex::pos,			"position"_usage),
	VertexElement(&TexFontVertex::col,			"colour"_usage),
	VertexElement(&TexFontVertex::uv,			"texcoord"_usage)
};

template<> const VertexElements ve<TexFontVertexGeom> = (const VertexElement[]) {
	VertexElement(&TexFontVertexGeom::pos,		"position"_usage),
	VertexElement(&TexFontVertexGeom::size,		"position1"_usage),
	VertexElement(&TexFontVertexGeom::uv,		"texcoord"_usage),
	VertexElement(&TexFontVertexGeom::col,		"colour"_usage)
};

template<> const VertexElements ve<SlugFontVertex> = (const VertexElement[]) {
	VertexElement(&SlugFontVertex::pos,			"position"_usage),
	VertexElement(&SlugFontVertex::col,			"colour"_usage),
	VertexElement(&SlugFontVertex::uv,			"texcoord"_usage),
	VertexElement(&SlugFontVertex::offsets,		"texcoord1"_usage)
};

template<> const VertexElements ve<SlugFontVertexGeom> = (const VertexElement[]) {
	VertexElement(&SlugFontVertexGeom::pos,		"position"_usage),
	VertexElement(&SlugFontVertexGeom::col,		"colour"_usage),
	VertexElement(&SlugFontVertexGeom::size,	"texcoord"_usage),
	VertexElement(&SlugFontVertexGeom::offsets,	"texcoord1"_usage)
};

template<typename P> inline void Put(Prim<P, TexFontVertex*> p, param(float2) pos1, param(float2) pos2, param(float2) uv1, param(float2) uv2, param(colour) col) {
	p[0].pos = pos1;
	p[1].pos = {pos2.x, pos1.y};
	p[2].pos = {pos1.x, pos2.y};
	p[3].pos = pos2;

	p[0].uv = uv1;
	p[1].uv = {uv2.x, uv1.y};
	p[2].uv = {uv1.x, uv2.y};
	p[3].uv = uv2;

	p[0].col = p[1].col = p[2].col = p[3].col = col.rgba;
//	return p.next();
}

template<typename P> inline void Put(Prim<P, SlugFontVertex*> p, param(float2) pos1, param(float2) pos2, param(float2) uv1, param(float2) uv2, param(colour) col, uint3p offsets) {
	p[0].pos = pos1;
	p[1].pos = {pos2.x, pos1.y};
	p[2].pos = {pos1.x, pos2.y};
	p[3].pos = pos2;

	p[0].uv = uv1;
	p[1].uv = {uv2.x, uv1.y};
	p[2].uv = {uv1.x, uv2.y};
	p[3].uv = uv2;

	p[0].col = p[1].col = p[2].col = p[3].col = col.rgba;
	p[0].offsets = p[1].offsets = p[2].offsets = p[3].offsets = offsets;
//	return p.next();
}

void TexFont::Start(GraphicsContext& ctx) {
	AddShaderParameter("font_samp"_crc32, Tex());
}

void SlugFont::Start(GraphicsContext &ctx) {
	ctx.SetBuffer(SS_PIXEL, band_buffer.cast(),	0);
	ctx.SetBuffer(SS_PIXEL, indices_buffer.cast(),1);
	ctx.SetBuffer(SS_PIXEL, curve_buffer.cast(),	2);
	ctx.SetTexture(SS_PIXEL, (Texture&)palette,	3);

	ctx.SetTexture(SS_GEOMETRY, curve_buffer.cast(), 0);
	ctx.SetTexture(SS_GEOMETRY, (Texture&)palette,	1);
}


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
			case FA_RIGHT:	return full || width ? (width - glyphs.Width(string, width, char_scale, &linebreak)) * scale : 0;
			case FA_CENTRE:	return full || width ? (width - glyphs.Width(string, width, char_scale, &linebreak)) * scale / 2.f : 0;
			case FA_JUSTIFY:
				if (width) {
					int		numspaces;
					float	w = glyphs.Width(string, width, char_scale, &linebreak, true, &numspaces);
					spacewidth = numspaces ? (width - w) * scale / numspaces : 0;
				}
				return 0;
			default:
				if (width)
					glyphs.Width(string, width, char_scale, &linebreak);
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
		, lspacing(fp->spacing * line_scale), pspacing(fp->spacing * para_scale), prevs(0), spacewidth(0)
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

		} else if (auto gi = glyphs.GetGlyph(string)) {
			AddGlyph(gi);
		}

		if (string == linebreak)
			break;
	}

	x = x0 + AlignText(string, width, x0, full);
	return string;
}

const char *FontPrinterState::SkipTo(const char *string, const char *end, float miny, bool full) {
	miny += baseline - height;
#if 0
	while (y < miny)
		string	= SkipLine(string, full);
#else
	int	c;
	while (y < miny && string < end && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
			ProcessControl(string, false);
			string += skip;

		} else if (auto gi = glyphs.GetGlyph(string)) {
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

	maxy += baseline;
	while (string < end && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
//			if (!(ignore & (1<<c)))
				ProcessControl(string, false);
			string += skip;

		} else if (auto gi = glyphs.GetGlyph(string)) {
			if (y > maxy)
				break;
			count += gi->num_layers();
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
		case CC_CENTRE:		x -= glyphs.Width(string + 1, width, char_scale, &linebreak) / 2.f;	break;	// centre
		case CC_RIGHTJ:		x -= glyphs.Width(string + 1, width, char_scale, &linebreak);		break;	// right justify
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
		return glyphs.CountRects(string, end);

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
	state.y0 = state.y = height;

	state.Begin(string, false);

	int		c;
	float	w = 0, h = 0;

	uint32	ignore = (1<<CC_COLOUR) | (1<<CC_RESTORECOL) | (1<<CC_ALPHA);

	while (len-- && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
			if (!(ignore & (1<<c)))
				state.ProcessControl(string, false);
			string += skip;

		} else if (auto gi = glyphs.GetGlyph(string)) {
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

template<> void FontPrinter::Print(Prim<PrimType, TexFontVertex*> &p, const GlyphInfo *_gi, float2 pos, float scale, colour col) {
	auto	gi		= static_cast<const TexGlyphInfo*>(_gi);
	float4	clip	= zero;
	const float	h	= height;
	const float	b	= baseline;

	if (flags.test(F_VCLIP)) {
		clip.y = min(pos.y - b - miny,		0.f);
		clip.w = max(pos.y - b + h - maxy, 0.f);
		if (clip.w > h)
			return;
	}
	if (flags.test(F_HCLIP)) {
		clip.x	= clamp(pos.x + gi->k - minx,			-gi->w,	0.f);
		clip.z	= clamp(pos.x + gi->k + gi->w - maxx,	0.f,	gi->w);
	}

	float2	off	= {gi->k, -b};
	float2	uv	= {gi->u, gi->v};
	float2	ext	= {gi->w, h};

	Put<PrimType>(p,
		pos + (off			- clip.xy) * scale,
		pos + (off + ext	- clip.zw) * scale,
		uv					- clip.xy,
		uv + ext			- clip.zw,
		//shear,
		col
	);
	++p;
}

template<> void FontPrinter::Print(Prim<PointList, TexFontVertexGeom*> &p, const GlyphInfo *_gi, float2 pos, float scale, colour col) {
	auto	gi		= static_cast<const TexGlyphInfo*>(_gi);
	float4	clip	= zero;
	const float	h	= height;
	const float	b	= baseline;

	if (flags.test(F_VCLIP)) {
		clip.y = min(pos.y - b - miny,		0.f);
		clip.w = max(pos.y - b + h - maxy,	0.f);
		if (clip.w > h)
			return;
	}
	if (flags.test(F_HCLIP)) {
		clip.x	= clamp(pos.x + gi->k - minx,			-gi->w,	0.f);
		clip.z	= clamp(pos.x + gi->k + gi->w - maxx,	0.f,	gi->w);
	}

	float2	off	= {gi->k, -b};
	float2	uv	= {gi->u, gi->v};
	float2	ext	= {gi->w, h};

	p[0].pos		= pos + off * scale;
	p[0].size		= ext * scale;
	p[0].uv			= concat(uv, uv + ext);
	p[0].col		= col.rgba;
	++p;
}

template<> void FontPrinter::Print(Prim<PrimType, SlugFontVertex*> &p, const GlyphInfo *_gi, float2 pos, float scale, colour col) {
	auto	gi		= static_cast<const SlugGlyphInfo*>(_gi);
	float2	off		= {gi->k, gi->y};
	float2	ext		= {gi->w, gi->h};
	float4	clip	= zero;

	Put<PrimType>(p,
		pos + (off			- clip.xy) * scale,
		pos + (off + ext	- clip.zw) * scale,
		float2(zero)		- clip.xy,
		float2(one)			- clip.zw,
		//shear,
		col,
		{uint32(gi->bands * 8 * 2), gi->indices_offset, gi->curves_offset}
	);
	++p;
}

uint16	start_layer = 0, max_layers = -1;
template<> void FontPrinter::Print(Prim<PointList, SlugFontVertexGeom*> &p, const GlyphInfo *_gi, float2 pos, float scale, colour col) {
	auto	gi		= static_cast<const SlugGlyphInfo*>(_gi);
	float2	off		= {gi->k, -gi->y};
	float2	ext		= {gi->w, -gi->h};

	if (gi->flags & gi->LAYERED) {
		auto	gl		= (GlyphLayered*)_gi;
		auto	lay		= (SlugLayerInfo*)(gi + gl->offset);
		auto	bands	= gi->bands * 8 * 2;
		lay		+= start_layer;
		bands	+= start_layer * 8 * 2;

		for (uint32	n = min(gl->num_layers - start_layer, max_layers); n--; ++lay, bands += 8 * 2) {
			p[0].pos		= pos + off * scale;
			p[0].size		= ext * scale;
			if (lay->colour.a == 0 && lay->colour.b == 0)
				p[0].col	= col.rgba;
			else
				p[0].col	= to<float>(lay->colour) / 255.f;
			p[0].offsets	= {bands, lay->indices_offset, lay->curves_offset};
			++p;
		}
	} else {
		p[0].pos		= pos + off * scale;
		p[0].size		= ext * scale;
		p[0].col		= col.rgba;
		p[0].offsets	= {uint32(gi->bands * 8 * 2), gi->indices_offset, gi->curves_offset};
		++p;
	}
}

template<typename I> void FontPrinter::Print(I &p, const char *string, const char *end) {
	FontPrinterState	state(this);
	state.Begin(string, true);
	
	if (flags.test(F_VCLIP)) {
		if (maxy <= miny)
			return;
		string = state.SkipTo(string, end, miny, true);
	}

	int	c;
	while (string < end && (c = (uint8)*string)) {
		if (int skip = Font::IsControlChar(c)) {
			state.ProcessControl(string, true);
			string += skip;

		} else if (auto gi = glyphs.GetGlyph(string)) {
			if (gi->w)
				Print(p, gi, float2{state.x, state.y}, state.scale,  state.curr_col);
			state.AddGlyph(gi);
		}
		if (string == state.linebreak)
			state.LineBreak(string, true);
	}
	x = state.x;
	y = state.y;
}

template<typename T> FontPrinter &FontPrinter::Print(const char *string, int len) {
/*	FontPrinterState	state(this);
	if (flags & F_VCLIP) {
		if (maxy <= miny)
			return *this;
		string = state.SkipTo(string, miny, true);
	}
*/
	const char	*end	= len < 0 ? (char*)intptr_t(-1) : string + len;
	if (int n = CountRects(string, end)) {
		ImmediateStream<T>	im(ctx, n);
		auto	p	= im.begin();
		Print(p, string, end);
		size_t	n2	= p - im.begin();
		im.SetCount(n2);
//		ISO_VERIFY(n2 == n);
	}
	return *this;
}

FontPrinter	&FontPrinter::Print(const char *string, int len) {
	if (ctx.Enabled(SS_GEOMETRY)) {
		if (slug)
			Print<Prim<PointList,SlugFontVertexGeom*>>(string, len);
		else
			Print<Prim<PointList,TexFontVertexGeom*>>(string, len);
	} else {
		if (slug)
			Print<Prim<PrimType,SlugFontVertex*>>(string, len);
		else
			Print<Prim<PrimType,TexFontVertex*>>(string, len);
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

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#include "gradient.h"
#include "maths/bezier.h"
#include "utilities.h"

using namespace iso;

struct ColourLine {
	enum EXTEND {
		EXTEND_NONE		= 0,
		EXTEND_PAD		= 1,
		EXTEND_REPEAT	= 2,
		EXTEND_REFLECT	= 3,
	};
	EXTEND		extend		= EXTEND_NONE;
	dynamic_array<pair<float, colour>> stops;
	interval<float>	get_range() const { return {stops.front().a, stops.back().a}; }
};

//#define BANDING

struct SoftCurves {
	DataBufferT<float2>		curve_buffer;
	DataBufferT<uint8>		indices_buffer;
	TextureT<rgba8>			palette;
#ifdef BANDING
	DataBufferT<uint16x2>	band_buffer;
#endif

	struct Shape {
		GradientTransform	gradient;
		const ColourLine*	cols	= nullptr;
		float4				col		= one;
		dynamic_array<bezier_chain<float2,2>>	curves;
		// for rendering
		rectangle			extent;
		uint32				indices_offset, curves_offset, total_curves;

		void	SetSolidColour(colour _col) {
			gradient.Set(gradient.SOLID);
			cols	= nullptr;
			col		= _col.rgba;
		}
		void	SetLinearGradient(const ColourLine* _cols, position2 p0, position2 p1, position2 p2) {
			cols	= _cols;
			gradient.SetLinear(p0, p1, p2);
		}
		void	SetSweepGradient(const ColourLine* _cols, position2 p0, float angle0, float angle1) {
			cols	= _cols;
			gradient.SetSweep(p0, angle0, angle1);
		}
		void	SetRadialGradient(const ColourLine* _cols, circle c0, circle c1) {
			cols	= _cols;
			gradient.SetRadial(c0, c1);
		}
	};
	dynamic_array<Shape>	shapes;

	bool	Make();
	void	Draw(GraphicsContext &ctx);
};

static dynamic_array<uint16> MakeBands(range<const float2*> points, int num_bands, uint16x2 *dest) {
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

	for (int i = 0; i < num_bands; i++) {
		sort(indices.slice(hband_offsets[i].offset, hband_offsets[i].size), [&](uint16 a, uint16 b) {
			return max(points[a * 2].x, points[a * 2 + 1].x, points[a * 2 + 2].x) > max(points[b * 2].x, points[b * 2 + 1].x, points[b * 2 + 2].x);
		});
		sort(indices.slice(vband_offsets[i].offset, vband_offsets[i].size), [&](uint16 a, uint16 b) {
			return max(points[a * 2].y, points[a * 2 + 1].y, points[a * 2 + 2].y) > max(points[b * 2].y, points[b * 2 + 1].y, points[b * 2 + 2].y);
		});
		dest[i + 0]			= {hband_offsets[i].offset, hband_offsets[i].size};
		dest[i + num_bands] = {vband_offsets[i].offset, vband_offsets[i].size};
	}

	return indices;
}

template<typename D> void make_palette(range<const pair<float, colour>*> srce, D &&dest) {
	auto	n	= srce.begin() + 1;
	float	t	= n[-1].a;
	float	dt	= (srce.back().a - t) / (dest.size() - 1);

	for (auto& i : dest) {
		while (n < srce.end() - 1 && t >= n->a)
			++n;

		i	= lerp(n[-1].b.rgba, n[0].b.rgba, (t - n[-1].a) / (n[0].a - n[-1].a));
		t	+= dt;
	}
}

bool SoftCurves::Make() {
	const uint32	num_bands	= 8;

	uint32	num_points	= 0;
	uint32	num_grads	= 0;

	for (auto& s : shapes) {
		if (s.gradient.fill != GradientTransform::SOLID) {
			num_points	+= 4;
			num_grads	+= !!s.cols;
		}

		s.curves_offset	= num_points;
		s.extent		= empty;
		for (auto &b : s.curves) {
			num_points	+= b.size() * 2 + 2;
			s.extent	|= b.get_box();
		}
	}
	if (num_points == 0)
		return false;

	// make palette

	auto	pal = make_auto_block<rgba8>(64, num_grads);

	num_grads	= 0;
	for (auto& s : shapes) {
		if (s.gradient.fill != GradientTransform::SOLID && s.cols) {
			if (s.gradient.reverse)
				make_palette(s.cols->stops, reversed(pal[num_grads++]));
			else
				make_palette(s.cols->stops, pal[num_grads++]);
		}
	}

	palette.Init(pal, MEM_FORCE2D);

	// make buffers

#ifdef BANDING
	//with bands
	temp_array<float2>		curve_array(num_points);
	temp_array<uint16x2>	band_array(shapes.size32() * 8 * 2);
	dynamic_array<uint8>	indices_array;

	auto	points	= curve_array.begin();

	num_grads	= 0;
	for (auto &s : shapes) {
		if (s.gradient.fill != GradientTransform::SOLID) {
			mat<float,2,4>	fill_data	= s.gradient.scaled_range(s.cols->get_range());
			(float2x3&)fill_data		= s.extent.from_0_1() / (float2x3&)fill_data;

			copy(fill_data, points);
			points	+= 4;
			if (s.cols)
				s.col	= {num_grads++, 0, (s.gradient.fill + (s.cols->extend << 4)) / 255.f, 0};
			else
				s.col	= {0, 0, (s.gradient.fill + .5f) / 255.f, 0};
		}

		auto	m		= s.extent.to_0_1();
		auto	points0	= points;
		for (auto &b : s.curves) {
			copy(transformc(b.c, [m](float2 p) { return m * position2(p); }), points);
			points += b.size() * 2 + 1;
			*points++ = nan;
		}
		s.indices_offset	= indices_array.size32();
		auto	indices		= MakeBands(make_range(points0, points), num_bands, band_array + shapes.index_of(s) * num_bands * 2);
		indices_array.append(indices);
	}

	curve_buffer.Init(curve_array.begin(), num_points);
	band_buffer.Init(band_array);
	indices_buffer.Init(indices_array);

#else
	
	//no bands
	temp_array<float2>		curve_array(num_points);
	dynamic_array<uint8>	indices_array;

	auto	points	= curve_array.begin();

	num_grads	= 0;
	for (auto &s : shapes) {
		float2x3	m		= identity;//s.extent.to_0_1();

		if (s.gradient.fill != GradientTransform::SOLID) {
			mat<float,2,4>	fill_data	= s.gradient.scaled_range(s.cols->get_range());
			(float2x3&)fill_data		= inverse(m) / (float2x3&)fill_data;

			copy(fill_data, points);
			points	+= 4;
			if (s.cols)
				s.col	= {num_grads++, 0, (s.gradient.fill + (s.cols->extend << 4)) / 255.f, 0};
			else
				s.col	= {0, 0, (s.gradient.fill + .5f) / 255.f, 0};
		}

		auto	points0	= points;
		for (auto &b : s.curves) {
			copy(transformc(b.c, [m](float2 p) { return m * position2(p); }), points);
			points += b.size() * 2 + 1;
			*points++ = nan;
		}
		s.indices_offset	= indices_array.size32();
		s.total_curves		= points - points0;
		dynamic_array<uint16>	indices = int_range(s.total_curves);
		indices_array.append(indices);
	}

	curve_buffer.Init(curve_array.begin(), num_points);
	indices_buffer.Init(indices_array);
#endif
	return true;
}

void SoftCurves::Draw(GraphicsContext& ctx) {
	if (!curve_buffer && !Make())
		return;

#ifdef BANDING
	ctx.SetBuffer(SS_PIXEL, band_buffer,	0);
	auto	bands	= 0;
	ctx.SetBuffer(SS_PIXEL, indices_buffer,	1);
	ctx.SetBuffer(SS_PIXEL, curve_buffer,	2);
	ctx.SetTexture(SS_PIXEL, palette,		3);
#else
	ctx.SetBuffer(SS_PIXEL, indices_buffer,	0);
	ctx.SetBuffer(SS_PIXEL, curve_buffer,	1);
	ctx.SetTexture(SS_PIXEL, palette,		2);
#endif

	ctx.SetTexture(SS_GEOMETRY, curve_buffer,	0);	
	ctx.SetTexture(SS_GEOMETRY, palette,	1);

	uint32	n		= shapes.size();
	ImmediateStream<Prim<PointList,SlugFontVertexGeom*>>	im(ctx, n);
	
	auto	p		= im.begin();

	for (auto &s : shapes) {
		p[0].pos		= s.extent.a;
		p[0].size		= s.extent.extent();
		p[0].col		= s.col;
	#ifdef BANDING
		p[0].offsets	= {bands, s.indices_offset, s.curves_offset};
		bands += 8 * 2;
	#else
		p[0].offsets	= {s.total_curves, s.indices_offset, s.curves_offset};
	#endif
		++p;
	}
}

#include "render.h"
#include "object.h"

struct CurvesTest : DeleteOnDestroy<CurvesTest> {
	static CreateWithWorld<CurvesTest> maker;

	SoftCurves	curves;
	ColourLine	gradient;

	void operator()(RenderEvent &re) {
		re.AddRenderItem(this, _MakeKey(RS_LAST, 1), 0);
	}
	void operator()(RenderEvent *re, uint32 extra) {
		static pass				*slug_test	= *ISO::root("data")["menu"]["test_slug_geom"][0];
		//static pass				*slug_test	= *ISO::root("data")["menu"]["test_slug"][0];
		
		auto	worldViewProj	= hardware_fix(perspective_projection_fov(re->fov, .1f) * re->offset * translate(0,0,1));
		AddShaderParameter("worldViewProj"_crc32, worldViewProj);

		Set(re->ctx, slug_test);
		re->ctx.SetBackFaceCull(BFC_NONE);
		curves.Draw(re->ctx);
	}

	CurvesTest(World *world) : DeleteOnDestroy<CurvesTest>(world) {
		world->AddHandler<RenderEvent>(this);

//		gradient.stops.emplace_back(-1, colour::cyan);
		gradient.stops.emplace_back(0, colour::red);
		gradient.stops.emplace_back(0.5f, colour::yellow);
		gradient.stops.emplace_back(1, colour::blue);
//		gradient.stops.emplace_back(2, colour::blue);
		gradient.extend	= gradient.EXTEND_REPEAT;

		bezier_chain<float2, 2>	ch({
			float2{-.5,  0},
			float2{  0, 1.0},
			float2{ .5,  0},
			float2{  0,-1.0},
			float2{-.5,  0},
		});

		auto	&s = curves.shapes.push_back();
		s.curves.push_back(ch);
//		s.curves.push_back((float2x2)scale(.8f) * ch.reverse());
		s.SetSolidColour(colour::cyan);

//		s.SetLinearGradient(position2(-.25f,0), position2(.25f,0), position2(-.25f,1));
//		s.SetSweepGradient(position2(0.1,0.2), degrees(0), degrees(90));
//		s.SetRadialGradient(circle(position2(.25f,0), .2f), circle(position2(-.25f,0), .1f));
		s.SetRadialGradient(&gradient, circle(position2(0,0), .25f), circle(position2(-.1f,0), .05f));
	}
};

CreateWithWorld<CurvesTest> CurvesTest::maker;

#include "font_iso.h"


struct EmojiTest : DeleteOnDestroy<EmojiTest> {
	static CreateWithWorld<EmojiTest> maker;

	char32	c = 0x1f60a;

	void operator()(RenderEvent &re) {
		re.AddRenderItem(this, _MakeKey(RS_LAST, 1), 0);
	}
	void operator()(RenderEvent *re, uint32 extra) {
		static auto	type = ISO::getdef<SlugFont>();
		static ISO_ptr<SlugFont> slug_font	= ISO::root("data")["slug_font"];
		static pass				*slug_test	= *ISO::root("data")["menu"]["test_slug_banded_geom"][0];

		auto	worldViewProj	= hardware_fix(parallel_projection_fov(re->fov, 640.f, -1.f, 1.f));
		AddShaderParameter("worldViewProj"_crc32, worldViewProj);

		Set(re->ctx, slug_test);
		re->ctx.SetBackFaceCull(BFC_NONE);
		slug_font->Start(re->ctx);
		
	#if 1
		FontPrinter(re->ctx, slug_font.get()).SetAlign(FA_CENTRE).SetScale(1).At(0, -500).SetWidth(640).Begin() << "The fuNny Font WiLl TrUmp AlL MindS";
	#else
		auto	a = FontPrinter(re->ctx, slug_font.get()).SetAlign(FA_CENTRE).SetScale(0.5f).At(0, -256).Begin();
		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++)
				a << char32(c + i * 8 + j);
			a << '\n';
		}
	#endif
	}
	void operator()(TimerMessage &e) {
		c += 64;
		switch (c) {
			case 0x120:		c = 0x2000;		break;
			case 0x3300:	c = 0x1f000;	break;
			case 0x1fb00:	c = 0xe0000;	break;
			case 0xe0100:	c = 0xfe400;	break;
			case 0xfe900:	c = 0x20;		break;
		}
		ISO_TRACEF("c=") << hex(c) << '\n';
		e.world->AddTimer(this, 0.5f);
	}

	EmojiTest(World *world) : DeleteOnDestroy<EmojiTest>(world) {
		world->AddHandler<RenderEvent>(this);
//		world->AddTimer(this, 1);
	}

};

CreateWithWorld<EmojiTest> EmojiTest::maker;
