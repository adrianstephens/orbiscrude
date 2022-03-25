#include "text_control.h"
#include "base/maths.h"
#include "control_helpers.h"
#include "base/skip_list.h"
#include "common.rc.h"
#include "extra/random.h"

#include "d2d.h"

using namespace iso;
using namespace win;

template<typename T> struct accum_tree : dynamic_array<T> {
	accum_tree(size_t n) : dynamic_array<T>(n) {}

	size_t	find(T v) {
		auto	first = begin();
		for (auto n = next_pow2(size()); n; n >>= 1) {
			auto	m = first + (n >> 1);
			if (*m < v) {
				v -= *m;
				first = m;
			}
		}
		return index_of(first);
	}

	void	adjust(size_t i, T d) {
		size_t	n;
		while ((n = lowest_clear(i)) < size()) {
			i |= n;
			at(i & ~(n - 1)) += d;
		}
	}

	T		sum(size_t i) {
		T		d = 0;
		while (i) {
			d += at(i);
			i = clear_lowest(i);
		}
		return d;
	}

	T		value(size_t i) {
		size_t	n	= lowest_clear(i);
		T		d	= at(i + 1);
		while (n > 1) {
			n >>= 1;
			d -= at(i & ~(n - 1));
		}
		return d;
	}
};

struct test_accum_tree {
	test_accum_tree() {
		rng<simple_random>	random;
		dynamic_array<int>	nums = transformc(int_range(1024), [&random](int) { return random.to(1024); });
		dynamic_array<int>	sums(1024);
		accum_tree<int>		ac(1024);

		uint32	offset = 0;
		for (int i = 0; i < 1024; i++) {
			ac.adjust(i, nums[i]);
			sums[i] = offset += nums[i];
		}

		for (int i = 0; i < 1023; i++) {
			if (i != 1023) {
				int	d = ac.sum(i + 1);
				ISO_ASSERT(d == sums[i]);

				int	n = ac.value(i);
				ISO_ASSERT(n == nums[i]);
			}
		}

		for (int x = 0, xe = nums.back(); x < xe; ++x) {
			auto	i = ac.find(x);
			auto	j = sums.index_of(lower_boundc(sums, x));
			ISO_ASSERT(i == j);
		}
	}
} _test_accum_tree;

#ifdef D2D_H

namespace iso { namespace d2d {

typedef TextLayout::Range char_range;

struct format0 {
	enum STATE {
		FONT, SIZE, STRETCH, STYLE, WEIGHT, STRIKE, UNDERLINE,
		EFFECT,
		COUNT,
	};
	STATE	s;
	format0() {}
	format0(STATE _s) : s(_s) {}
//	virtual	void	set(IDWriteTextLayout *layout, const char_range &range)=0;
	void	set(IDWriteTextLayout *layout, const char_range &range);
};
template<format0::STATE S> struct format : format0 { format() : format0(S) {} };

struct format_font : format<format0::FONT> {
	string16				value;
	void	set(IDWriteTextLayout *layout, const char_range &range) { layout->SetFontFamilyName(value, range); }
	format_font(const char *v) : value(v) {}
};
struct format_size : format<format0::SIZE> {
	float					value;
	void	set(IDWriteTextLayout *layout, const char_range &range) { layout->SetFontSize(value, range); }
	format_size(float v) : value(v) {}
};
struct format_stretch : format<format0::STRETCH> {
	DWRITE_FONT_STRETCH		value;
	void	set(IDWriteTextLayout *layout, const char_range &range) { layout->SetFontStretch(value, range); }
	format_stretch(DWRITE_FONT_STRETCH v) : value(v) {}
};
struct format_style : format<format0::STYLE> {
	DWRITE_FONT_STYLE		value;
	void	set(IDWriteTextLayout *layout, const char_range &range) { layout->SetFontStyle(value, range); }
	format_style(DWRITE_FONT_STYLE v) : value(v) {}
};
struct format_weight : format<format0::WEIGHT> {
	DWRITE_FONT_WEIGHT		value;
	void	set(IDWriteTextLayout *layout, const char_range &range) { layout->SetFontWeight(value, range); }
	format_weight(DWRITE_FONT_WEIGHT v) : value(v) {}
};
struct format_strike : format<format0::STRIKE> {
	bool					value;
	void	set(IDWriteTextLayout *layout, const char_range &range) { layout->SetStrikethrough(value, range); }
	format_strike(bool v) : value(v) {}
};
struct format_underline : format<format0::UNDERLINE> {
	bool					value;
	void	set(IDWriteTextLayout *layout, const char_range &range) { layout->SetUnderline(value, range); }
	format_underline(bool v) : value(v) {}
};

struct FormatEffect : public com<TextEffect> {
	d2d::colour	col;
	d2d::colour	bg;
	float		voffset;
	uint32		effects;

	COM_DECLSPEC_NOTHROW HRESULT DrawGlyphRun(void * context, ID2D1DeviceContext *device, D2D1_POINT_2F baseline, const DWRITE_GLYPH_RUN *glyphs, const DWRITE_GLYPH_RUN_DESCRIPTION *desc, DWRITE_MEASURING_MODE measure) {
		DWRITE_FONT_METRICS	metrics;
		glyphs->fontFace->GetMetrics(&metrics);
		baseline.y += voffset;

		if (bg.a) {
			float	y0	= metrics.ascent * glyphs->fontEmSize / metrics.designUnitsPerEm;
			float	y1	= (metrics.descent + metrics.lineGap) * glyphs->fontEmSize / metrics.designUnitsPerEm;
			float	width	= 0;
			for (const float *i = glyphs->glyphAdvances, *e = i + glyphs->glyphCount; i != e; ++i)
				width += *i;

			d2d::rect	bounds(baseline.x, baseline.y - y0, baseline.x + width + 1, baseline.y + y1 + 1);
			device->FillRectangle(&bounds, d2d::SolidBrush(device, bg));
		}

		device->DrawGlyphRun(baseline, glyphs, desc, d2d::SolidBrush(device, col), measure);
		return S_OK;
	}
	COM_DECLSPEC_NOTHROW HRESULT	DrawGeometry(void * context, ID2D1DeviceContext *device, ID2D1Geometry *geometry) {
		return S_OK;
	}
	FormatEffect() : voffset(0), effects(0) { bg.a = 0; }
	FormatEffect(d2d::colour _col, d2d::colour _bg, float _voffset, uint32 _effects) : col(_col), bg(_bg), voffset(_voffset), effects(_effects) {}
};

// implemented with TextEffects
struct format_effect : format<format0::EFFECT>, FormatEffect {
	void	set(IDWriteTextLayout *layout, const char_range &range) {
		layout->SetDrawingEffect(this, range);
	}
};

void format0::set(IDWriteTextLayout *layout, const char_range &range) {
	switch (s) {
		case FONT:		static_cast<format_font*>(this)->set(layout, range); break;
		case SIZE:		static_cast<format_size*>(this)->set(layout, range); break;
		case STRETCH:	static_cast<format_stretch*>(this)->set(layout, range); break;
		case STYLE:		static_cast<format_style*>(this)->set(layout, range); break;
		case WEIGHT:	static_cast<format_weight*>(this)->set(layout, range); break;
		case STRIKE:	static_cast<format_strike*>(this)->set(layout, range); break;
		case UNDERLINE:	static_cast<format_underline*>(this)->set(layout, range); break;
		case EFFECT:	static_cast<format_effect*>(this)->set(layout, range); break;
	}
}

struct paragraph {
	enum {
		LEFT, RIGHT, CENTRE, JUSTIFY,
		ALIGN_MASK	= 3 << 0,
		WRAP		= 1 << 2,
		UNIFORM		= 1 << 3,
	};
	d2d::rect	margins;
	float		indent;
	float		tabs;
	float		spacing;
	float		baseline;
	uint32		flags;

	paragraph() { clear(*this); }
	void	SetAlign(DWRITE_TEXT_ALIGNMENT a) { flags = (flags & ~ALIGN_MASK) | a; }
	void	SetSpacing(DWRITE_LINE_SPACING_METHOD _method, float _spacing, float _baseline) {
		flags = (flags & ~UNIFORM) | _method * UNIFORM;
		spacing		= _spacing;
		baseline	= _baseline;
	}
	void	SetSpacing(uint8 rule, float _spacing) {
		spacing		= _spacing;
		baseline	= _spacing * .8f;
	}
};

class TextDisplay {
protected:
	struct chunk : paragraph {
		d2d::TextLayout	layout;
		string16		s;
		uint32			offset;
		uint32			lines;
		float			width, height, y;
		const chunk		&next()			const { return *(this + 1); }
		uint32			begin()			const { return offset; }
		uint32			end()			const { return next().offset; }
		uint32			length()		const { return end() - begin(); }
		auto			text()			const { return count_string16(s.begin(), length()); }
		interval<uint32>	get_range()	const { return interval<uint32>(begin(), end()); }
		template<typename B> chunk(const B &_s) : s(_s), offset(0), lines(0), width(0), height(0), y(0) {}
	};

	d2d::colour				background;
	dynamic_array<chunk>	text;
//	accum_tree<uint32>		offsets;
//	accum_tree<uint32>		lines;

private:
	static const uint32 optimal_chunk = 1024;

	d2d::Write				write;
	d2d::Font				font;
	interval_tree<uint32, format0*>	formats;
	int						dirty_index;

	chunk*			Split(chunk *c, uint32 split);

	const chunk*	GetChunk(uint32 offset) const {
		return lower_bound(text.begin(), text.end() - 1, offset, [](const chunk &c, uint32 offset) { return c.next().offset <= offset; });
	}
	chunk*			GetChunk(uint32 offset) {
		return lower_bound(text.begin(), text.end() - 1, offset, [](const chunk &c, uint32 offset) { return c.next().offset <= offset; });
	}
	const chunk*	GetChunkByPos(float y) const {
		if (dirty_index && y < text[dirty_index].y)
			return lower_bound(text.begin(), text.begin() + dirty_index - 1, y, [](const chunk &c, float y) { return c.next().y < y; });
		return 0;
	}
	chunk*			GetChunkByPos(float y) {
		if (dirty_index && y < text[dirty_index].y)
			return lower_bound(text.begin(), text.begin() + dirty_index - 1, y, [](const chunk &c, float y) { return c.next().y < y; });
		return 0;
	}

public:
	void			ReplaceText(const interval<uint32> &r, const wchar_t *s, const wchar_t *e);
	uint32			GetText(const interval<uint32> &r, wchar_t *dest);
	range<chunk*>	MakeParagraphs(const interval<uint32> &r);

	void			WidthChanged()								{ dirty_index = 0; }
	void			Dirty(uint32 offset)						{ if (offset < ScannedOffset()) dirty_index = text.index_of(GetChunk(offset)); }
	float			UpdateTo(float ey, uint32 offset, float width);
	float			Width(float offsety, float height) const;
	void			Paint(d2d::Target &target, float x, float y, float offsety, float height);

	void			ApplyTemp(const interval<uint32> &r, TextEffect *fx);

	void			SetBackground(const d2d::colour &col)		{ background = col;	}
	void			SetFont(const win::Font::Params16 &p);

	void			AddFormat(const interval<uint32> &r, format0 *f)	{ formats.insert(r, f); }

	uint32			Length()							const	{ return text.back().offset; }
	float			ScannedHeight()						const	{ return text[dirty_index].y; }
	uint32			ScannedOffset()						const	{ return text[dirty_index].offset; }
	float			EstimatedHeight()					const	{ return text[dirty_index].y / dirty_index * (text.size() - 1); }
	const chunk*	HitTestFromWindow(float px, float py, DWRITE_HIT_TEST_METRICS *hit) const;
	const chunk*	TextPosition(uint32 offset, float *px, float *py, DWRITE_HIT_TEST_METRICS *hit) const;

	uint32			LineFromChar(uint32 offset)			const;
	int				CharFromLine(uint32 line)			const;
	uint32			LineCount()							const;
	uint32			LineLength(uint32 offset)			const;

	uint32			Find(const char16 *search, const interval<uint32> &r, uint32 flags);

	TextDisplay() : background(0, 0, 0), font(write, L"Arial", 12), dirty_index(0) {
		text.push_back("");
	}
};

void TextDisplay::SetFont(const win::Font::Params16 &p) {
	font = d2d::Font(write, p);
	if (text[0].tabs == 0)
		text[0].tabs = d2d::TextLayout(write, "xxxx", font, 1000).GetExtent().Width();
}


uint32 TextDisplay::LineFromChar(uint32 offset) const {
	uint32	line	= 0;
	for (auto &i : text) {
		if (offset < i.end()) {
			const char16 *s = i.s;
			const char16 *e = s + offset - i.offset;
			for (const char16 *p; p = string_find(s, '\n', e); s = p + 1)
				++line;
			break;
		}
		line += i.lines;

	}
	return line;
}

int TextDisplay::CharFromLine(uint32 line) const {
	for (auto &i : text) {
		if (line < i.lines) {
			const char16 *p = i.s - 1;
			while (line && (p = string_find(p + 1, '\n')))
				--line;
			return i.offset + (p + 1 - i.s);
		}
		line -= i.lines;
	}
	return -1;
}

uint32 TextDisplay::LineCount() const {
	uint32	line	= 0;
	for (auto &i : text)
		line += i.lines;
	return line;
}

uint32 TextDisplay::LineLength(uint32 offset) const {
	const chunk	*i = GetChunk(offset);
	if (const char16 *s = i->s) {
		auto	a = string_rfind(s, '\n', s + offset - i->offset);
		if (!a)
			a = s - 1;
		auto	b = string_find(a, '\n');
		if (!b)
			b = string_end(s);
		return b - a - 1;
	}
	return 0;
}

uint32 count_lines(const char16 *s) {
	uint32		line	= 0;
	while (const char16 *p = string_find(s, '\n')) {
		s = p + 1;
		++line;
	}
	return line;
}

TextDisplay::chunk *TextDisplay::Split(chunk *c, uint32 split) {
	if (split) {
		bool	right	= split > c->length() / 2;
		auto	i		= text.index_of(c);
		auto	n		= text.insert(c, chunk(c->s.slice(0, split)));
		c				= n + 1;
		c->s			= c->s.slice(split);

		*(paragraph*)n	= *(paragraph*)c;
		n->tabs			= c->tabs;
		n->offset		= c->offset;
		n->y			= c->y;
		n->lines		= right ? c->lines - count_lines(c->s) : count_lines(n->s);

		c->offset		+= split;
		c->lines		-= n->lines;
	}
	return c;
}

range<TextDisplay::chunk*> TextDisplay::MakeParagraphs(const interval<uint32> &r) {
	chunk	*i		= GetChunk(r.begin());
	if (i->offset < r.begin())
		i = Split(i, r.begin() - i->offset);

	uint32	index	= text.index_of(i);
	uint32	end		= r.end();
	chunk	*back	= text.end() - 1;
	while (i != back && i->offset <= end)
		++i;
	if (i->offset > end) {
		--i;
		i = Split(i, end - i->offset);
	}

	return range<chunk*>(text + index, i);
}

void TextDisplay::ReplaceText(const interval<uint32> &r, const wchar_t *s, const wchar_t *e) {
	chunk	*i0			= GetChunk(r.begin());
	chunk	*back		= text.end() - 1;
	uint32	orig_end	= Length();
	auto	length		= e - s;
	int		adjust		= int(length) - r.extent();

	if (i0 == back && s != e && i0 != text.begin())
		--i0;

	dirty_index = min(dirty_index, text.index_of(i0));

	if (i0 != back) {
		auto	i1		= i0 + 1;
		while (i1 != back && i1->end() < r.end())
			i1 = text.erase(i1);

		if (i1 == back) {
			i0->s = i0->s.slice(0, r.begin() - i0->offset) + str(s, e);
		} else {
			i0->s = i0->s.slice(0, r.begin() - i0->offset) + str(s, e) + i1->s.slice(r.end() - i1->offset);
			i1 = text.erase(i1);
		}

		while (i1 != text.end())
			i1++->offset	+= adjust;

	} else if (s != e) {
		i0->s			= str(s, e);
		auto	&chunk	= text.push_back("");
		chunk.tabs		= i0->tabs;
		chunk.offset	= i0->offset + (e - s);
	}

	if (i0 != back) {
		i0->lines = count_lines(i0->s);

		while (i0->length() > optimal_chunk * 2) {
	#if 1
			auto	t	= i0->text();
			auto	s	= t.begin();
			auto	n	= t.end() - optimal_chunk;
			auto	nf	= string_find(n, '\n');
			auto	nr	= string_rfind(s, '\n', n);

			if (nf && nf[1] == 0)
				nf = nullptr;

			n = !nf ? nr : !nr ? nf : (nf - n) < (n - nr) ? nf : nr;

			if (!n)
				break;

			i0 = Split(i0, n + 1 - s) - 1;
	#else
			auto	*s	= i0->s.begin();
			auto	*n	= s + optimal_chunk;
			auto	*nf = string_find(n, '\n');
			auto	*nr = string_rfind(s, '\n', n);
			n = !nf ? nr : !nr ? nf : (nf - n) < (n - nr) ? nf : nr;

			if (!n)
				break;

			i0	= Split(i0, n + 1 - s);
	#endif
		}
	}

	format0	*last[format0::COUNT] = {0};

	if (r.a || r.b != orig_end) {
		for (auto j = interval_begin(formats, r); j; ++j) {
			auto	&k = j.key();
			if (k.b == r.b)
				last[(*j)->s] = *j;
		}
	}

	for (auto j = interval_begin(formats, r); j;) {
		auto	&k = j.key();

		if (k.a > r.a)
			k.set_begin(k.a > r.b ? k.a + adjust : r.a);

		if (k.b >= r.a) {
			if (last[(*j)->s] == *j) {
				k.set_end(k.b + adjust);
			} else {
				k.set_end(r.a);
				if (k.empty()) {
					formats.remove(j);
					continue;
				}
			}
		}
		++j;
	}

//	if (r.a == 0 && r.b == orig_end)
//		ISO_ASSERT(formats.empty());
}

uint32 TextDisplay::GetText(const interval<uint32> &r, wchar_t *dest) {
	chunk	*i		= GetChunk(r.begin());
	chunk	*back	= text.end() - 1;

	if (i == back)
		return 0;

	wchar_t *dest0	= dest;

	uint32	n = min(i->end() - r.begin(), r.extent());
	memcpy(dest, i->s.begin() + (r.begin() - i->offset), n * sizeof(wchar_t));
	dest	+= n;

	for (++i; i != back && i->end() < r.end(); ++i) {
		n = i->length();
		memcpy(dest, i->s, n * 2);
		dest	+= n;
	}

	if (i != back && i->offset < r.end()) {
		n = r.end() - i->offset;
		memcpy(dest, i->s, n * 2);
		dest	+= n;
	}

	return dest - dest0;
}

const TextDisplay::chunk *TextDisplay::HitTestFromWindow(float px, float py, DWRITE_HIT_TEST_METRICS *hit) const {
#if 1
	if (const chunk *i = GetChunkByPos(py)) {
		BOOL	trailing, inside;
		if (SUCCEEDED(i->layout->HitTestPoint(px, py - i->y, &trailing, &inside, hit))) {
			hit->textPosition += trailing;
			return i;
		}
	}
#else
	auto	i	= text.begin(), e = i + dirty_index;
	i = lower_bound(i, e, py, [](chunk &c, float y) { return c.next().y < y; });

	if (i != e) {
		BOOL	trailing, inside;
		if (i->layout->HitTestPoint(px, py - i->y, &trailing, &inside, hit)) {
			hit->textPosition += trailing;
			return i;
		}
	}
#endif
	return 0;
}

float TextDisplay::UpdateTo(float ey, uint32 offset, float width) {
	chunk	*i	= text + dirty_index;
	chunk	*e	= text.end() - 1;
	float	y	= i->y;

	while (i < e && (y < ey || i->offset <= offset)) {
		i->width		= 0;
		if (i->s) {

			font->SetIncrementalTabStop(i->tabs);
			uint32	end	= i->end();
			uint32	len	= end - i->offset;
			if (len && i->s[len - 1] == '\n') {
				--len;
				if (len && i->s[len - 1] == '\r')
					--len;
			}
			d2d::TextLayout	layout(write, i->s, len, font, width);

			interval<uint32>	ci(i->begin(), end);

			for (auto j = interval_begin(formats, ci.a, end); j && j.key().begin() < end; ++j)
				(*j)->set(layout, (j.key() & ci) - ci.a);

			i->layout = layout.detach();

			DWRITE_TEXT_METRICS	metrics;
			i->layout->GetMetrics(&metrics);
			i->width	= metrics.width;
			i->height	= metrics.height;
			y			+= metrics.height;
		}
		(++i)->y	= y;
	}
	dirty_index	= text.index_of(i);
	return y;
}

void TextDisplay::ApplyTemp(const interval<uint32> &r, TextEffect *fx) {
	chunk	*i		= GetChunk(r.begin());
	chunk	*end	= text.begin() + dirty_index;
	while (i < end && overlap(r, i->get_range())) {
		i->layout.SetDrawingEffect(fx, (r & i->get_range()) - i->begin());
		++i;
	}
}

float TextDisplay::Width(float offsety, float height) const {
	float	max_w = 0;
	if (const chunk *i = GetChunkByPos(offsety)) {
		for (const chunk *e = text.end() - 1; i != e && i->y < offsety + height; ++i)
			max_w = max(max_w, i->width);
	}

	return max_w;
}

const TextDisplay::chunk *TextDisplay::TextPosition(uint32 offset, float *px, float *py, DWRITE_HIT_TEST_METRICS *hit) const {
	const chunk	*i	= GetChunk(offset);
	if (i == text.end() - 1) {
		if (i == text.begin())
			return 0;
		--i;
	}

	if (!i->layout)
		return 0;

	i->layout->HitTestTextPosition(offset - i->offset, FALSE, px, py, hit);
	hit->top	+= i->y;
	*py			+= i->y;
	return i;
}

void TextDisplay::Paint(d2d::Target &target, float x, float y, float offsety, float height) {
	d2d::TextRenderer	renderer(target, d2d::SolidBrush(target, iso::colour::black));
	target.Clear(background);

	if (chunk *i = GetChunkByPos(offsety)) {
		for (chunk *e = text.end() - 1; i != e && i->y - offsety < height && i->layout; ++i)
			i->layout->Draw(0, &renderer, x, y + i->y - offsety);
	}
}

inline bool word_delim(char16 c) {
	return c < '0';
}

inline bool delim(char16 *s, bool word) {
	return !word || word_delim(*s);
}

bool compare(const char16 *s1, const char16 *s2, size_t len, bool matchcase) {
	if (matchcase)
		return memcmp(s1, s2, len * 2) == 0;
	while (len--) {
		if (to_lower(*s1++) != to_lower(*s2++))
			return false;
	}
	return true;
}

uint32 TextDisplay::Find(const char16 *search, const interval<uint32> &r, uint32 flags) {
	uint32	search_len	= string_len32(search);
	bool	down		= !!(flags & FR_DOWN);
	bool	matchcase	= !!(flags & FR_MATCHCASE);
	bool	word		= !!(flags & FR_WHOLEWORD);

	chunk *i = GetChunk(r.a);
	for (;;) {
		uint32	begin	= i->begin(), end = i->end();

		if (down) {
			auto	s		= i->s.slice(max(r.a, begin) - begin, min(r.b, end) - begin);
			for (char16 *f = s.begin(), *e = s.end() - search_len; f < e; f++) {
				if (delim(f, word) && compare(f, search, search_len, matchcase) && delim(f + search_len, word))
					return f - i->s.begin() + begin;
			}

			if (++i == text.end() - 1 || i->begin() >= r.b)
				break;

			auto	&s2	= i->s;
			for (int j = min(r.b - end, search_len); j--; ) {
				if (delim(s.end() - j - 1, word)
					&& compare(s.end() - j, search, j, matchcase)
					&& compare(s2.begin(), search + j, search_len - j, matchcase)
					&& delim(s2.begin() + search_len, word)
				)
					return end - j;
			}

		} else {
			auto	s		= i->s.slice(max(r.b, begin) - begin, min(r.a, end) - begin);
			for (char16 *f = s.end() - search_len, *e = s.begin(); f > e; f--) {
				if (delim(f, word) && compare(f, search, search_len, matchcase) && delim(f - 1, word))
					return f - i->s.begin() + begin;
			}

			if (i == text.begin() || (--i)->end() < r.b)
				break;

			auto	&s2	= i->s;
			for (int j = 1, n = min(r.b - begin, search_len); j < n; j++) {
				if (delim(s.begin() + search_len - j, word)
					&& compare(s.begin(), search + j, search_len - j, matchcase)
					&& compare(s2.end() - j, search, j, matchcase)
					&& delim(s2.end() - j - 1, word)
				)
					return begin - j;
			}

		}
	}

	return ~0;
}
} } // namespace iso::d2d

//-----------------------------------------------------------------------------
//	D2DEditControlImp
//-----------------------------------------------------------------------------

struct TextFinder16 : FINDREPLACEW {
	void	Find(HWND hWnd, wchar_t *s, size_t len) {
		clear(*this);
		lStructSize		= sizeof(FINDREPLACEW);
		hwndOwner		= hWnd;
		Flags			= FR_DOWN;
		lpstrFindWhat	= s;
		wFindWhatLen	= (WORD)len;
		FindTextW(this);
	}
	static TextFinder16 *CheckMessage(UINT message, WPARAM wParam, LPARAM lParam);
};

TextFinder16 *TextFinder16::CheckMessage(UINT message, WPARAM wParam, LPARAM lParam) {
	static UINT WM_FINDMSGSTRING = RegisterWindowMessageW(FINDMSGSTRINGW);
	if (message == WM_FINDMSGSTRING) {
		auto	*t = (TextFinder16*)lParam;
		if (!(t->Flags & FR_DIALOGTERM))
			return t;

	} else if (message == WM_PARENTNOTIFY) {
		if (LOWORD(wParam) == WM_DESTROY)
			SetModelessDialog(0);
	}
	return 0;
}

class D2DEditControlImp : public Window<D2DEditControlImp>, public WindowTimer<D2DEditControlImp>, d2d::TextDisplay {
	d2d::WND				target;
	TScrollInfo<float>		si, sih;
	float					scroll_target, scroll_speed;
	float					margins[2], zoom;
	Rect					drawrect;
	Rect					rel_drawrect;
	dynamic_array<float>	tabs;

	Point					gesture_loc;
	uint32					gesture_dist;
	interval<uint32>		selection;
	fixed_string<256, wchar_t>	search;
	TextFinder16			finder;

	enum FLAGS {
		WORDWRAP	= 1 << 0,
		HIDESEL		= 1 << 1,
		ABSDRAWRECT	= 1 << 2,
		READONLY	= 1 << 3,
		MODIFIED	= 1 << 4,
		TIMER		= 1 << 4,
	};
	flags<FLAGS>			flags;

	void	SetDrawRect(const Rect &r) {
		int	width0	= drawrect.Width();
		drawrect	= r;
		if (flags.test(WORDWRAP) && drawrect.Width() != width0)
			WidthChanged();
		si.SetPage(drawrect.Height());
		sih.SetPage(drawrect.Width());
		SetScroll(si);
	}

	void	Zoom(float mult) {
		zoom	*= mult;
		if (!flags.test(ABSDRAWRECT))
			SetDrawRect((GetClientRect() + rel_drawrect) / zoom);
	}

	void UpdateTo(float ey, uint32 offset) {
		float	width	= flags.test(WORDWRAP) ? drawrect.Width() - margins[0] - margins[1] : 1e38f;
		float	y		= d2d::TextDisplay::UpdateTo(ey, offset, width);
		if (!flags.test(HIDESEL) && !abs(selection).empty())
			ApplyTemp(abs(selection), new d2d::FormatEffect(colour::white, win::Colour::SysColor(COLOR_HIGHLIGHT), 0, 0));
	}

	const chunk*	TextPosition(uint32 offset, float *px, float *py, DWRITE_HIT_TEST_METRICS *hit, bool visible_only = true) {
		if (offset > ScannedOffset())
			UpdateTo(visible_only ? si.Pos() + drawrect.Height() + 16 : 0, visible_only ? 0 : offset);
		return d2d::TextDisplay::TextPosition(offset, px, py, hit);
	}
	bool	GetPosition(uint32 offset, POINT &pos, DWRITE_HIT_TEST_METRICS &hit, bool visible_only = true) {
		float	x, y;
		if (TextPosition(offset, &x, &y, &hit, visible_only)) {
			pos = Point((x + drawrect.left + margins[0] - sih.Pos()) * zoom, (y + drawrect.top - si.Pos()) * zoom);
			return true;
		}
		return false;
	}

	const chunk*	HitTestFromWindow(const POINT &p, DWRITE_HIT_TEST_METRICS *hit) {
		float	x = p.x / zoom - drawrect.left - margins[0] + sih.Pos();
		float	y = p.y / zoom - drawrect.top + si.Pos();
		UpdateTo(y, 0);
		return d2d::TextDisplay::HitTestFromWindow(x, y, hit);
	}

	uint32	OffsetFromWindow(const POINT &p) {
		DWRITE_HIT_TEST_METRICS	hit;
		if (const chunk *i = HitTestFromWindow(p, &hit))
			return i->offset + hit.textPosition;
		return Length();
	}

	bool	Visible(uint32 a) const {
		DWRITE_HIT_TEST_METRICS	hit;
		float x, y;
		return d2d::TextDisplay::TextPosition(a, &x, &y, &hit) && between(y - si.Pos(), 0, drawrect.Height());
	}

	bool	Visible(const interval<uint32> &r) const {
		DWRITE_HIT_TEST_METRICS	hit;
		float x, y;
		return d2d::TextDisplay::TextPosition(r.a, &x, &y, &hit) && y - si.Pos() < drawrect.Height()
			&& d2d::TextDisplay::TextPosition(r.b, &x, &y, &hit) && y > si.Pos();
	}

	void	SetSelection(uint32 a) {
		if (!abs(selection).empty())
			Dirty(selection.minimum());
		selection.a = selection.b = a;
	}

	void	SetSelection(const interval<uint32> &r) {
		uint32		len		= Length();
		auto		prev	= abs(selection);

		selection.a	= min(r.a, len);
		selection.b = min(r.b, len);

		if (!flags.test(HIDESEL)) {
			auto	sel	= abs(selection);

			if (sel.a != prev.a) {
				if (Visible(make_interval_abs(sel.a, prev.a)))
					Invalidate();
			}
			if (sel.b != prev.b) {
				if (Visible(make_interval_abs(sel.b, prev.b)))
					Invalidate();
			}

			Dirty(sel.a != prev.a ? min(prev.a, sel.a) : min(prev.b, sel.b));
		}
	}

	void	MakeVisible(uint32 offset) {
		Timer::Stop();

		float	x, y;
		DWRITE_HIT_TEST_METRICS	hit;
		TextPosition(offset, &x, &y, &hit);

		float	height	= drawrect.Height();
		uint32	s		= style;
		if ((s & ES_AUTOHSCROLL) && !(s & WS_HSCROLL) && !flags.test(WORDWRAP))
			height -= SystemMetrics::HScrollHeight();

		si.SetMax(EstimatedHeight());
		if (hit.top < si.Pos()) {
			si.MoveTo(hit.top);
		} else if (hit.top + hit.height - si.Pos() > height) {
			si.MoveTo(hit.top + hit.height - height);
		} else {
			return;
		}

		scroll_target = si.Pos();
		SetScroll(si);
		Invalidate();
		SetCaret(offset);
	}

	void	SetCaret(uint32 offset) {
		if (Visible(offset)) {
			DWRITE_HIT_TEST_METRICS	hit;
			Point					pos;
			if (GetPosition(offset, pos, hit)) {
				CreateCaret(*this, 0, 2 * zoom, hit.height * zoom);
				SetCaretPos(pos.x, pos.y);
				ShowCaret(*this);
			}
		}
	}

	void	MoveCaret(uint32 offset, bool shift) {
		auto	sel = selection;
		sel.b = offset;
		if (!shift)
			sel.a = offset;
		SetSelection(sel);
		MakeVisible(offset);
		Invalidate();
	}


	void	FindNext(const wchar_t *text, uint32 fflags) {
		uint32	found	= Find(text, fflags & FR_DOWN ? interval<uint32>(selection.minimum() + 1, Length()) : interval<uint32>(selection.maximum() - 1, 0), fflags);
		if (~found) {
			flags.clear(HIDESEL);
			SetSelection(CharRange(found, found + string_len32(text)));
			MakeVisible(selection.b);
		}
	}

	string16	GetText(const interval<uint32> &r) {
		auto		r2	= r & interval<uint32>(0, Length());
		string16	s(r2.extent());
		uint32		len	= d2d::TextDisplay::GetText(r2, s);
		ISO_ASSERT(len == r2.extent());
		if (len)
			s[len] = 0;
		return s;
	}

public:
	static const char*			ClassName()	{ return "D2DEditControl"; }
	static WindowClass			Register()	{ return get_class(); }
	static D2DEditControlImp*	make()		{ return new D2DEditControlImp(); }

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	D2DEditControlImp() : scroll_target(0), scroll_speed(0), zoom(1), selection(0, 0) {
		SetBackground(win::Colour::SysColor(COLOR_WINDOW));
		clear(margins);
		clear(rel_drawrect);
	}
};

LRESULT D2DEditControlImp::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			flags.set(WORDWRAP, !(style & HSCROLL));
			break;

		case WM_SIZE: {
			Point	size(lParam);
			if (!flags.test(ABSDRAWRECT))
				SetDrawRect((GetClientRect() + rel_drawrect) / zoom);

			SetCaret(selection.b);
			Invalidate();
			return 0;
		}

		case WM_ISO_TIMER: {
			float		dp = scroll_target - si.pos;
			scroll_speed	= scroll_speed * 7 / 9 + dp / 32;
			si.pos += scroll_speed;
			save(si.pos, round(si.pos)), SetScroll(si);
			Invalidate();
			if (abs(dp) < 1 && abs(scroll_speed) < 1)
				Timer::Stop();
			return 0;
		}
		case SBM_SETSCROLLINFO: {
			float	old_pos = si.Pos();
			si = *(SCROLLINFO*)lParam;
			scroll_target += si.Pos() - old_pos;
			SetScroll(*(SCROLLINFO*)lParam);
			break;
		}

		case EM_SCROLL:
		case WM_VSCROLL:
			Timer::Stop();
			if (LOWORD(wParam) == SB_THUMBTRACK) {
				si = GetScroll(SIF_TRACKPOS|SIF_RANGE);
			} else {
				si.ProcessScroll(wParam);
			}
			SetScroll(si);
			SetCaret(selection.b);
			Invalidate();
			return 0;

		case WM_HSCROLL:
			sih.ProcessScroll(wParam);
			SetScroll(sih, false);
			SetCaret(selection.b);
			Invalidate();
			return 0;

		case WM_LBUTTONDOWN: {
			Timer::Stop();
			SetFocus();
			uint32	offset	= OffsetFromWindow(Point(lParam));
			auto	sel		= selection;
			if (sel.empty() || !(wParam & MK_SHIFT))
				sel.a = sel.b = offset;
			else
				sel.b = offset;
			SetSelection(sel);
			SetCaret(offset);
			Invalidate();
			SetCapture(*this);
			Parent()(WM_COMMAND, wparam(id.get(), EN_SETFOCUS), hWnd);
			return 0;
		}
		case WM_LBUTTONUP:
			ReleaseCapture();
			return 0;

		case WM_LBUTTONDBLCLK: {
			Timer::Stop();
			SetFocus();
			DWRITE_HIT_TEST_METRICS	hit;
			if (const chunk *i = HitTestFromWindow(Point(lParam), &hit)) {
				const char16 *p = i->s;
				const char16 *e = string_find(p + hit.textPosition, char_set::whitespace);
				const char16 *s = string_rfind(p, char_set::whitespace, p + hit.textPosition);
				SetSelection(interval<uint32>(i->offset + (s ? s + 1 - p : 0), i->offset + (e - p)));
			}
			SetCapture(*this);
			return 0;
		}

		case WM_MOUSEMOVE: {
			Point	mouse(lParam);
			if (wParam & MK_LBUTTON) {
				Rect	rect = GetClientRect();
				if (int move = mouse.y > rect.Bottom() ? 1 : mouse.y < rect.Top() ? -1 : 0) {
					si.MoveBy(move);
					SetScroll(si);
				}
				uint32	offset	= OffsetFromWindow(mouse);
				SetSelection(interval<uint32>(selection.a, offset));
				SetCaret(offset);
				Invalidate();
			}
			return 0;
		}

		case WM_MOUSEWHEEL: {
			int	dz = short(HIWORD(wParam));
			if (wParam & MK_CONTROL) {
				Zoom(iso::pow(1.05f, dz / 64.f));
				SetScroll(si);
				Invalidate();
			} else {
				if (!Timer::IsRunning())
					scroll_target = si.Pos();
				scroll_target = clamp(scroll_target - dz, 0, max(si.Max() - si.Page(), 0));
				if (!Timer::IsRunning() && abs(scroll_target - si.pos) > 16)
					Timer::Start(0.02f, Timer::TIMER_THREAD);
			}
			return 0;
		}

		case WM_MOUSEHWHEEL:
			return 0;

		case WM_GESTURE: {
			GestureInfo gi((HGESTUREINFO)lParam);
			Point	prev_loc	= gesture_loc;
			uint32	prev_dist	= gesture_dist;
			gesture_loc		= gi.Location();
			gesture_dist	= gi.Distance();

			if (!(gi.dwFlags & GF_BEGIN)) {
				Point	delta_loc	= DeviceContext::ScreenCaps().PixelUnScale(gesture_loc - prev_loc);
				gesture_loc			= prev_loc + DeviceContext::ScreenCaps().PixelUnScale(delta_loc);

				switch (gi.dwID) {
					case GID_ZOOM:
						si.MoveBy(-delta_loc.y);
						Zoom(float(gesture_dist) / prev_dist);
						SetScroll(si);
						SetCaret(selection.b);
						Invalidate();
						return 0;
					case GID_PAN:
						si.MoveBy(-delta_loc.y);
						SetScroll(si);
						sih.MoveBy(-delta_loc.x);
						SetScroll(sih, false);
						SetCaret(selection.b);
						Invalidate();
						return 0;
					case GID_ROTATE:
						return 0;
					case GID_TWOFINGERTAP:
						return 0;
					case GID_PRESSANDTAP:
						return 0;
					default:
						break;
				}
			}
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		case WM_GETTEXTLENGTH:
			return d2d::TextDisplay::Length();

		case WM_SETTEXT: {
			string16	s((const char*)lParam);
			d2d::TextDisplay::ReplaceText(interval<uint32>(0, Length()), s.begin(), s.end());
			UpdateTo(0, ~0);
			SetScroll(si);
			Invalidate();
			return 0;
		}

		case WM_SETFONT:
			d2d::TextDisplay::SetFont(win::Font(HFONT(wParam)).GetParams16());
			return 0;

		case WM_KEYDOWN: {
			bool	shift	= !!(GetAsyncKeyState(VK_SHIFT) & 0x8000);
			switch (wParam) {
				case VK_LEFT:
					MoveCaret(selection.b - 1, shift);
					break;

				case VK_RIGHT:
					MoveCaret(selection.b + 1, shift);
					break;

				case VK_DOWN:	{
					DWRITE_HIT_TEST_METRICS	hit;
					float		x, y;
					const chunk		*i	= TextPosition(selection.b, &x, &y, &hit);
					i = d2d::TextDisplay::HitTestFromWindow(x, y + hit.height, &hit);
					MoveCaret(i ? i->offset + hit.textPosition : Length(), shift);
					break;
				}

				case VK_UP: {
					DWRITE_HIT_TEST_METRICS	hit;
					float		x, y;
					const chunk		*i	= TextPosition(selection.b, &x, &y, &hit);
					i = d2d::TextDisplay::HitTestFromWindow(x, y - hit.height, &hit);
					if (hit.textPosition == i->length())
						--hit.textPosition;
					MoveCaret(i->offset + hit.textPosition, shift);
					break;
				}

				//case VK_PRIOR:	selection.b -= int(rects[1].Height() / line_height) * bytes_per_line; break;
				//case VK_NEXT:		selection.b += int(rects[1].Height() / line_height) * bytes_per_line; break;

				default:
					if (!shift) {
						if (int move = si.ProcessKey(wParam)) {
							SetScroll(si);
							Invalidate();
						}
					}
					break;
			}
			SetCaret(selection.b);
			return 0;
		}

		case WM_CONTEXTMENU:
			CheckByID(Menu(IDR_MENU_SUBMENUS).GetSubMenuByName("Text"), ID_EDIT_WORDWRAP, flags.test(WORDWRAP)).Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
			return 0;

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_EDIT_UNDO:		return SendMessage(EM_UNDO);
				case ID_EDIT_REDO:		return SendMessage(EM_REDO);
				case ID_EDIT_CUT:		return SendMessage(WM_CUT);
				case ID_EDIT_COPY:		return SendMessage(WM_COPY);
				case ID_EDIT_DELETE: {
					auto	sel = abs(selection);
					if (sel.a == sel.b)
						--sel.a;
					ReplaceText(sel, 0, 0);
					DWRITE_HIT_TEST_METRICS	hit;
					Point					pos;
					if (GetPosition(sel.a, pos, hit)) {
						Invalidate(GetClientRect().Subbox(pos.x, pos.y, 0, 0));
						SetSelection(sel.a);
						SetCaret(selection.b);
					}
					break;
				}
				case ID_EDIT_DELETE_FWD: {
					auto	sel = abs(selection);
					if (sel.a == sel.b) {
						if (sel.b == Length())
							return 0;
						++sel.b;
					}
					ReplaceText(sel, 0, 0);
					DWRITE_HIT_TEST_METRICS	hit;
					Point					pos;
					if (GetPosition(sel.a, pos, hit)) {
						Invalidate(GetClientRect().Subbox(pos.x, pos.y, 0, 0));
						SetSelection(sel.a);
						SetCaret(selection.b);
					}
					break;
				}

				case ID_EDIT_FIND:
					finder.Find(*this, search, 256);
					break;

				case ID_EDIT_FINDNEXT:
					if (!search.blank())
						FindNext(search, finder.Flags);
					break;

				case ID_EDIT_FINDPREV:
					if (!search.blank())
						FindNext(search, finder.Flags ^ FR_DOWN);
					break;

				case ID_EDIT_SELECT_ALL:
					SetSelection(CharRange::all());
					break;

				case ID_EDIT_WORDWRAP:
					flags.flip(WORDWRAP);
					Dirty(0);
					Invalidate();
					break;

				default:
					return 0;
			}
			return 1;

		case WM_CUT:
			if (!abs(selection).empty()) {
				Clipboard(*this).Set(GetText(abs(selection)));
				ReplaceText(abs(selection), 0, 0);
				SetSelection(selection.a);
			}
			return 0;

		case WM_COPY:
			if (!abs(selection).empty()) {
				Clipboard	c(*this);
				c.Empty();
				c.Set(string(GetText(abs(selection))));
			}
			return 0;

		case WM_CLEAR:
			ReplaceText(abs(selection), 0, 0);
			SetSelection(selection.a);
			return 0;

		case EM_SETUNDOLIMIT:
		case EM_CANREDO:
		case EM_CANUNDO:
		case EM_UNDO:
		case EM_REDO:
			break;

		case EM_GETTEXTRANGE: {
			TEXTRANGE	*tr = (TEXTRANGE*)lParam;
			string_copy(tr->lpstrText, GetText((const CharRange&)tr->chrg));
			return string_len(tr->lpstrText);
		}

		case EM_SETTARGETDEVICE:
			flags.set(WORDWRAP, lParam);
			Dirty(0);
			Invalidate();
			break;

		case EM_SETBKGNDCOLOR:
			return exchange(background, wParam ? win::Colour::SysColor(COLOR_WINDOW) : win::Colour(lParam));

		case EM_SETCHARFORMAT: {
			CHARFORMAT2A			*f		= (CHARFORMAT2A*)lParam;
			uint32					mask	= f->dwMask;
			uint32					effects	= f->dwEffects;
			const interval<uint32>	r		= wParam == SCF_ALL ? interval<uint32>(0, Length()) : abs(selection);

			if (mask & CFM_WEIGHT)
				AddFormat(r, new d2d::format_weight(DWRITE_FONT_WEIGHT(f->wWeight)));
			else if (mask & CFM_BOLD)
				AddFormat(r, new d2d::format_weight(effects & CFE_BOLD ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL));
			if (mask & CFM_ITALIC)
				AddFormat(r, new d2d::format_style(effects & CFE_ITALIC ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL));
			if (mask & CFM_UNDERLINE)
				AddFormat(r, new d2d::format_underline(!!(effects & CFE_UNDERLINE)));
			if (mask & CFM_STRIKEOUT)
				AddFormat(r, new d2d::format_strike(!!(effects & CFE_STRIKEOUT)));
			if (mask & CFM_SIZE)
				AddFormat(r, new d2d::format_size(f->yHeight));
			if (mask & CFM_FACE)
				AddFormat(r, new d2d::format_font(f->szFaceName));

			if (mask & CFM_SPACING) {
				for (auto &p : MakeParagraphs(r))
					p.SetSpacing(DWRITE_LINE_SPACING_METHOD_DEFAULT, f->sSpacing, f->sSpacing * .8f);
			}

			if (mask & (CFM_COLOR | CFM_BACKCOLOR | CFM_OFFSET | CFM_SUBSCRIPT | CFM_SUPERSCRIPT)) {
				d2d::format_effect	*f2 = new d2d::format_effect;
				if (mask & CFM_COLOR)
					f2->col = f->crTextColor;
				if (mask & CFM_BACKCOLOR)
					f2->bg = f->crBackColor;
				if (mask & CFM_OFFSET)
					f2->voffset = f->yOffset;
				if (mask & CFM_SUBSCRIPT)
					f2->effects = f->dwEffects & (CFE_SUBSCRIPT | CFE_SUPERSCRIPT);

				AddFormat(r, f2);
			}
			return 1;
		}
		case EM_SETPARAFORMAT: {
			PARAFORMAT2		*f		= (PARAFORMAT2*)lParam;
			uint32			mask	= f->dwMask;
			float			tabs	= f->rgxTabs[0] * 96 / (72 * 20);
			for (auto &p : MakeParagraphs(abs(selection))) {
				if (mask & PFM_STARTINDENT)
					p.indent = f->dxStartIndent;
				if (mask & PFM_RIGHTINDENT)
					p.margins.right = f->dxRightIndent;
				if (mask & PFM_OFFSET)
					p.margins.left = f->dxOffset;
				if (mask & PFM_ALIGNMENT)
					p.SetAlign(DWRITE_TEXT_ALIGNMENT(f->wAlignment - 1));
				if (mask & PFM_TABSTOPS)
					p.tabs = tabs;
				if (mask & PFM_OFFSETINDENT)
					p.indent += f->dxStartIndent;
				if (mask & PFM_SPACEBEFORE)
					p.margins.top = f->dySpaceBefore;
				if (mask & PFM_SPACEAFTER)
					p.margins.bottom = f->dySpaceAfter;
				if (mask & PFM_LINESPACING)
					p.SetSpacing(f->bLineSpacingRule, f->dyLineSpacing);
			}
			return 1;
		}

		case EM_GETSEL:
			if (wParam)
				*(uint32*)wParam = selection.a;
			if (lParam)
				*(uint32*)lParam = selection.b;
			return min(selection.a, 0xffff) | (min(selection.b, 0xffff) << 16);

		case EM_EXGETSEL: {
			CHARRANGE *cr = (CHARRANGE*)lParam;
			cr->cpMin	= selection.minimum();
			cr->cpMax	= selection.maximum();
			return 1;
		}

		case EM_EXSETSEL: {
			CharRange *cr = (CharRange*)lParam;
			SetSelection(*cr);
			return LRESULT(cr);
		}
		case EM_SETSEL:
			SetSelection(CharRange(wParam, lParam));
			return 1;

		case EM_REPLACESEL: {
			auto	sel = abs(selection);
			string16	s((const char*)lParam);
			ReplaceText(sel, s.begin(), s.end());
			SetSelection(sel.a + string_len32((const char*)lParam));

			if (between(ScannedHeight() - si.Pos(), 0, drawrect.bottom)) {
				DWRITE_HIT_TEST_METRICS	hit;
				Point					pos;
				if (GetPosition(sel.a, pos, hit)) {
					Invalidate(GetClientRect().Subbox(pos.x, pos.y, 0, 0));
					SetCaret(selection.b);
				}
			}
			return 1;
		}
//		case WM_TIMER:
//			flags.clear(TIMER);
//			KillTimer(wParam);
//			if (UpdateTo(0, ~0))
//				SetScroll(si);
//			return 0;

		case EM_GETRECT:
			*(RECT*)lParam = drawrect;
			return 1;

		case EM_SETRECTNP:
		case EM_SETRECT:
			if (lParam == 0) {
				flags.clear(ABSDRAWRECT);
				clear(rel_drawrect);
				SetDrawRect(GetClientRect() / zoom);
			} else if (wParam == 0) {
				flags.set(ABSDRAWRECT);
				SetDrawRect(*(RECT*)lParam);
			} else {
				flags.clear(ABSDRAWRECT);
				rel_drawrect = rel_drawrect + *(RECT*)lParam;
				SetDrawRect((GetClientRect() + rel_drawrect) / zoom );
			}

			if (message != EM_SETRECTNP)
				Invalidate();
			return 1;

		case EM_SETMARGINS:
			if (wParam & EC_LEFTMARGIN)
				margins[0] = wParam == EC_USEFONTINFO || LOWORD(lParam) == EC_USEFONTINFO ? 1 : LOWORD(lParam);
			if (wParam & EC_RIGHTMARGIN)
				margins[0] = wParam == EC_USEFONTINFO || HIWORD(lParam) == EC_USEFONTINFO ? 1 : HIWORD(lParam);
			break;

		case EM_GETMARGINS:
			return MAKELONG(int(margins[0]), int(margins[1]));

		case EM_FINDTEXTEX: {
			uint32		flags	= wParam;
			FINDTEXTEXA	*find	= (FINDTEXTEXA*)lParam;
			return Find(str16(find->lpstrText), interval<uint32>(find->chrg.cpMin, find->chrg.cpMax), flags);
		}

		case EM_SCROLLCARET:
			MakeVisible(selection.b);
			return 1;

		case EM_SETTABSTOPS:
			tabs.clear();
			for (int i = 0; i < wParam; i++)
				tabs.push_back(((uint32*)lParam)[i]);
			return 1;

		case EM_HIDESELECTION:
			if (flags.test_set(HIDESEL, wParam) != !!wParam && Visible(abs(selection)))
				Invalidate();
			break;

		case EM_SETREADONLY:
			flags.set(READONLY, wParam);
			style = (style & ~ES_READONLY) | (wParam ? ES_READONLY : 0);
			break;

		case EM_LINESCROLL: {
			float	dy = lParam / si.Scale();
			scroll_target += dy;
			si.MoveBy(dy);
			SetScroll(si);
			Invalidate();
			break;
		}

		case EM_POSFROMCHAR: {
			DWRITE_HIT_TEST_METRICS	hit;
			GetPosition(lParam, *(POINT*)wParam, hit, false);
			return 0;
		}

		case EM_CHARFROMPOS:
			return OffsetFromWindow(*(POINT*)lParam);

		case EM_LINEFROMCHAR:
			return LineFromChar(wParam);

		case EM_LINEINDEX:
			return CharFromLine(wParam == -1 ? LineFromChar(selection.b) : wParam);

		case EM_GETLINECOUNT:
			return LineCount();

		case EM_LINELENGTH:
			return LineLength(wParam);

		case EM_GETLINE: {
			int		start	= CharFromLine(wParam);
			if (start < 0)
				return 0;
			uint32	len		= LineLength(start);
			string_copy((char*)lParam, GetText(interval<uint32>(start, start + len)));
			return len;
		}
		case EM_GETFIRSTVISIBLELINE:
			break;

		case EM_SETCUEBANNER:
		case EM_SETEVENTMASK:
			break;

		case EM_GETTEXTLENGTHEX: {
			GETTEXTLENGTHEX *t = (GETTEXTLENGTHEX*)lParam;
			uint32		len		= 0;
			uint32		numcr	= t->flags & GTL_USECRLF ? LineCount() : 0;
			if (t->flags & GTL_NUMBYTES) {
				switch (t->codepage) {
					case 1200:		len = (Length() + numcr) * 2; break;
					case CP_ACP:
					case CP_UTF7:
					case CP_UTF8:
						len = numcr;
						for (auto &i : text)
							len += uint32(chars_count<char16>(i.s.begin()));
						break;
				}
			} else {
				len = Length() + numcr;
			}
			return len;
		}
		case EM_SETTEXTEX:
			break;

		case EM_GETMODIFY:
			return flags.test(MODIFIED);

		case EM_SETMODIFY:
			flags.set(MODIFIED, wParam);
			return 0;

		case EM_GETTHUMB:
			return si.Pos();

		case EM_STREAMIN: {
			EDITSTREAM		*es = (EDITSTREAM*)lParam;
			LONG			read;
			malloc_block	buffer(1 << 16);

			interval<uint32>	sel	= wParam & SFF_SELECTION ? abs(selection) : interval<uint32>(0, Length());

			switch (wParam & 15) {
				case SF_TEXT:
					while (!(es->dwError = es->pfnCallback(es->dwCookie, buffer, buffer.size32(), &read)) && read) {

						if (wParam & SF_UNICODE) {
							ReplaceText(sel, (wchar_t*)buffer, (wchar_t*)(buffer + read));
							sel = interval<uint32>(sel.a + read / 2);
						} else {
							string16	s((char*)buffer, read);
							ReplaceText(sel, s.begin(), s.end());
							sel = interval<uint32>(sel.a + read);
						}
					}

					UpdateTo(0, ~0);
					si.SetMax(ScannedHeight());
					SetScroll(si);
					return Length();

				case SF_RTF:
				default:
					return 0;
			}
		}

		case EM_STREAMOUT: {
			EDITSTREAM		*es		= (EDITSTREAM*)lParam;
			LONG			total	= 0;
			switch (wParam & 15) {
				case SF_TEXT:
					for (auto &i : text) {
						LONG	read = 0;
						if (wParam & SF_UNICODE) {
							uint8	*p	= (uint8*)i.s.begin(), *e = (uint8*)i.s.end();
							while (p < e && !(es->dwError = es->pfnCallback(es->dwCookie, p, e - p, &read)) && read) {
								p		+= read;
								total	+= read / 2;
							}

						} else {
							string	s(i.s);
							uint8	*p	= (uint8*)s.begin(), *e = (uint8*)s.end();
							while (p < e && !(es->dwError = es->pfnCallback(es->dwCookie, p, e - p, &read)) && read) {
								p		+= read;
								total	+= read;
							}
						}
						if (es->dwError || !read)
							break;
					}
					return total;

				case SF_RTF:
				default:
					return 0;
			}
		}

		case WM_PAINT: {
			DeviceContextPaint(*this);
			if (target.Init(hWnd, GetClientRect().Size()) && !target.Occluded()) {
				target.BeginDraw();
				target.SetTransform(scale(zoom));

				UpdateTo(si.Pos() + drawrect.Height() + 16, 0);
				float	y = EstimatedHeight();
				if (y != si.Max()) {
					si.SetMax(y);
					SetScroll(si);
				}

				Paint(target, drawrect.left + margins[0] - sih.Pos(), drawrect.top, si.Pos(), drawrect.Height());

				d2d::PAINT_INFO(*this, &target).Send(Parent());

				if (target.EndDraw())
					target.DeInit();

				if (!flags.test(WORDWRAP)) {
					float	h	= drawrect.Height();
					uint32	s	= style;
					if ((s & ES_AUTOHSCROLL) && !(s & WS_HSCROLL))
						h -= SystemMetrics::HScrollHeight();
					sih.SetMax(Width(si.Pos(), h));
					SetScroll(sih, false);
				}
			}
			return 0;
		}

		case WM_MOUSEACTIVATE:
			SetAccelerator(*this, IDR_ACCELERATOR_TEXT);
			break;

		case WM_KILLFOCUS:
			DestroyCaret();
			break;

		case WM_DESTROY:
			destruct(*this);
			//delete this;
			return 0;

		default:
			if (auto *t = TextFinder16::CheckMessage(message, wParam, lParam)) {
				if (t->Flags & FR_FINDNEXT) {
					FindNext(search, t->Flags);
				}
				return 0;
			}
			break;
	}
	return Super(message, wParam, lParam);
};

static initialise init(D2DEditControlImp::Register());

D2DTextWindow::D2DTextWindow() {
	LoadLibraryA("RICHED20.DLL");
}

D2DTextWindow::D2DTextWindow(const WindowPos &pos, const char *_title, Style style, StyleEx styleEx, ID id) {
	LoadLibraryA("RICHED20.DLL");
	Create(pos, _title, style, styleEx, id);
}

LRESULT D2DTextWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_GETTEXT:
			if (title) {
				int	n = min(wParam, title.length() + 1);
				memcpy((void*)lParam, title, n);
				return n - 1;
			}
			return 0;

		case WM_GETTEXTLENGTH:
			return title.length();

		case WM_DESTROY:
			hWnd = 0;
			break;
	}
	return Super(message, wParam, lParam);
}

HWND D2DTextWindow::Create(const WindowPos &pos, const char *_title, Style style, StyleEx styleEx, ID id) {
	title = _title;
	Subclass<D2DTextWindow, D2DEditControl>::Create(pos, _title, style | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL | HSCROLL | VSCROLL, styleEx, id);
	return *this;
}

void D2DTextWindow::Online(bool enable) {
	if (enable)
		Rebind(this);
//	else
//		Unbind();
}

Control iso::win::CreateD2DEditControl(const WindowPos &pos) {
	HINSTANCE	hInst	= LoadLibraryA("RICHED20.DLL");
	auto	c = new D2DEditControlImp();
	c->Create(pos, "edit", Control::CHILD | Control::VISIBLE);
	return *c;
}

#endif	//D2D

LRESULT TextWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_GETTEXT:
			if (title) {
				int	n = min(wParam, title.length() + 1);
				memcpy((void*)lParam, title, n);
				return n - 1;
			}
			return 0;

		case WM_CONTEXTMENU:
			CheckByID(Menu(IDR_MENU_SUBMENUS).GetSubMenuByName("Text"), ID_EDIT_WORDWRAP, !!(flags & WORDWRAP)).Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
			return 0;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case ID_EDIT_UNDO:		SendMessage(EM_UNDO);	break;
				case ID_EDIT_REDO:		SendMessage(EM_REDO);	break;
				case ID_EDIT_CUT:		SendMessage(WM_CUT);	break;
				case ID_EDIT_COPY:		SendMessage(WM_COPY);	break;
				case ID_EDIT_DELETE:	SendMessage(WM_CLEAR);	break;
				case ID_EDIT_FIND:		new TextFinder(*this);	break;
				case ID_EDIT_SELECT_ALL:SetSelection(CharRange::all()); break;
				case ID_EDIT_WORDWRAP:	flags ^= WORDWRAP; SendMessage(EM_SETTARGETDEVICE, 0, flags & WORDWRAP ? 0 : 1); break;
				default:				return 0;//Parent()(message, wParam, lParam);
			}
			return 1;

		case WM_LBUTTONDOWN:
			Super(message, wParam, lParam);
			Parent()(WM_COMMAND, MAKEWPARAM(id.get(), EN_SETFOCUS), (LPARAM)hWnd);
			break;

		case WM_SETFOCUS:
			SetAccelerator(*this, IDR_ACCELERATOR_TEXT);
			Super(message, wParam, lParam);
			return 0;

		case WM_NCDESTROY:
			Super(message, wParam, lParam);
			delete this;
			return 0;

		default:
			if (TextFinder *t = TextFinder::CheckMessage(message, wParam, lParam)) {
				if (t->Flags & FR_FINDNEXT)
					FindNextText(*this, t->lpstrFindWhat, t->Flags);
				return 0;
			}
			break;
	}
	return Super(message, wParam, lParam);
}

TextWindow::TextWindow() : flags(0) {
	LoadLibraryA("RICHED20.DLL");
}

TextWindow::TextWindow(const WindowPos &pos, const char *_title, Style style, StyleEx styleEx, ID id) : flags(0) {
	LoadLibraryA("RICHED20.DLL");
	Create(pos, _title, style, styleEx, id);
}

HWND TextWindow::Create(const WindowPos &pos, const char *_title, Style style, StyleEx styleEx, ID id) {
	title = _title;
	Subclass<TextWindow, RichEditControl>::Create(pos, NULL, style | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL | CHILD | HSCROLL | VSCROLL, styleEx, id);
	SetFormat(CharFormat().Font("Courier New").Weight(FW_NORMAL).Size(12 * 15), SCF_DEFAULT);
	return *this;
}

void TextWindow::Online(bool enable) {
	if (enable)
		Rebind(this);
//	else
//		Unbind();
	SetUndoLimit(enable ? 100 : 0);
}


static const win::Colour ansi_cols[] = {
	{0,0,0},
	{128,0,0},
	{0,128,0},
	{128,128,0},
	{0,0,128},
	{128,0,128},
	{0,128,128},
	{192,192,192},
	{128,128,128},
	{255,0,0},
	{0,255,0},
	{255,255,0},
	{0,0,255},
	{255,0,255},
	{0,255,255},
	{255,255,255},
};

int get_ansi_col(win::Colour &col, const uint32 *codes) {
	switch (codes[0]) {
		case 2:
			col = win::Colour(codes[1], codes[2], codes[3]);
			return 4;
		case 5: {
			int	i = codes[1];
			col = i < 16 ? ansi_cols[i]
				: i < 232 ? win::Colour((i - 16) / 36 * 51, (((i - 16) / 6) % 6) * 51, ((i - 16) % 6) * 51)	//16-231:  6 × 6 × 6 cube (216 colors): 16 + 36 × r + 6 × g + b
				: win::Colour::Grey((i - 232) * 11);														//232-255:  grayscale from black to white in 24 steps
			return 2;
		}
	}
	return 0;
}

void get_ansi_format(win::CharFormat &format, const uint32 *codes, int ncodes) {
	for (int i = 0; i <= ncodes; i++) {
		int	c = codes[i];
		switch (c) {
			case 0:		format.SetEffects(0xff, false).Colour(colour::black).BgColor(colour::white); break;	//Reset / Normal
			case 1:		format.Bold();				break;	//Bold or increased intensity
			//case 2:	format.;					break;	//Faint (decreased intensity)
			case 3:		format.Italic();			break;	//Italic
			case 4:		format.Underline();			break;	//Underline
			//case 5:	format.;					break;	//Slow Blink
			//case 6:	format.;					break;	//Rapid Blink
			//case 7:	format.;					break;	//reverse video
			case 8:		format.Hidden();			break;	//Conceal
			case 9:		format.Strikeout();			break;	//Crossed-out
			//case 10:	format.;					break;	//Primary(default) font
			//case 11–19									//Alternative font
			//case 20:	format.;					break;	//Fraktur
			//case 21:	format.;					break;	//Doubly underline or Bold off
			case 22:	format.Bold(false);			break;	//Normal color or intensity
			case 23:	format.Italic(false);		break;	//Not italic, not Fraktur
			case 24:	format.Underline(false);	break;	//Underline off
			//case 25:	format.;					break;	//Blink off
			//case 27:	format.;					break;	//Inverse off
			case 28:	format.Hidden(false);		break;	//Reveal
			case 29:	format.Strikeout(false);	break;	//Not crossed out

			//Set foreground color
			case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
				format.Colour(ansi_cols[c - 30]);
				break;
			//Set bright foreground color
			case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
				format.Colour(ansi_cols[c - 90 + 8]);
				break;
			case 38:	i += get_ansi_col(format.SetColour(), codes + i + 1);	break;
			case 39:	format.Colour(colour::black); break;

			//Set background color
			case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
				format.BgColor(ansi_cols[c - 40]);
				break;
			//Set bright background color
			case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
				format.BgColor(ansi_cols[c - 100 + 8]);
				break;
			case 48:	i += get_ansi_col(format.SetBgColor(), codes + i + 1);	break;
			case 49:	format.BgColor(colour::white);	break;

			//case 51:	format.;					break;	//Framed
			//case 52:	format.;					break;	//Encircled
			//case 53:	format.;					break;	//Overlined
			//case 54:	format.;					break;	//Not framed or encircled
			//case 55:	format.;					break;	//Not overlined
			//case 60:	format.;					break;	//ideogram underline or right side line
			//case 61:	format.;					break;	//ideogram double underline or double line on the right side
			//case 62:	format.;					break;	//ideogram overline or left side line
			//case 63:	format.;					break;	//ideogram double overline or double line on the left side
			//case 64:	format.;					break;	//ideogram stress marking
			//case 65:	format.;					break;	//ideogram attributes off
			default:
				break;
		}
	}
}

template<typename C> void AddTextANSIT(RichEditControl control, const C *p) {
	control.SetSelection(win::CharRange::end());
	while (auto e = string_find(p, '\x1b')) {
		control.ReplaceSelection(str(p, e));
		if (e[1] == '[') {
			++e;
			uint32	codes[8], ncodes;
			for (ncodes = 0; ncodes < 8; ncodes++) {
				e = get_num_base<10>(e + 1, codes[ncodes]);
				if (e[0] != ';')
					break;
			}
			if (e[0] == 'm') {
				CharFormat	format;
				get_ansi_format(format, codes, ncodes);
				control.SetSelection(win::CharRange::end());
				control.SetFormat(format);
			}
		}
		p = e + 1;
	}
	control.ReplaceSelection(p);
}


void win::AddTextANSI(RichEditControl control, const char *p) {
	AddTextANSIT(control, p);
}

void win::AddTextANSI(RichEditControl control, const char16 *p) {
	AddTextANSIT(control, p);
}
