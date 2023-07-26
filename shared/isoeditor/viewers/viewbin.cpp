#include "viewbin.h"
#include "iso/iso_files.h"

namespace app {

size_t ReadHexLine(ISO_openarray<uint8> &array, string_scan &ss) {
	while (is_hex(ss.skip_whitespace().peekc())) {
		const char	*p = ss.getp();
		if (ss.peekc() == '0' && to_lower(ss.peekc(1)) == 'x')
			ss.move(2);

		count_string	tok	= ss.get_token(char_set("0123456789ABCDEFabcdef"));
		if (is_alpha(ss.peekc())) {
			ss.move(p - ss.getp());
			break;
		}

		uint32			size	= array.Count();
		const char		*n		= tok.begin();

		if (tok.length() <= 2) {
			get_num_base<16>(n, array.Append());
		} else if (tok.length() <= 4) {
			get_num_base<16>(n, (uint16&)array.Resize(size + 2)[size]);
		} else if (tok.length() <= 8) {
			get_num_base<16>(n, (uint32&)array.Resize(size + 4)[size]);
		} else if (tok.length() <= 16) {
			get_num_base<16>(n, (uint64&)array.Resize(size + 8)[size]);
		} else {
			const char*	p = tok.begin();
			while (p < tok.end() && is_hex(*p))
				++p;
			int		n = (p - tok.begin()) / 2;
			uint8*	d = array.Resize(size + n) + size;
			for (const char *p = tok.begin(); n--; p += 2)
				*d++ = (from_digit(p[0]) << 4) + from_digit(p[1]);
		}
	}
	return array.size();
}

//-----------------------------------------------------------------------------
//	ViewBinMode
//-----------------------------------------------------------------------------

uint64 ViewBinMode::LoadNumber(const uint8 *p, uint32 n) const {
	uint64	v = 0;
	n = n ? min(n, bytes_per_element) : bytes_per_element;
	if (flags.test(BIGENDIAN)) {
		if (flags.test(SIGNED) && (p[0] & 0x80))
			v = -1;
		for (int i = 0; i < n; i++)
			v = (v << 8) | *p++;
	} else {
		p += bytes_per_element;
		if (flags.test(SIGNED) && n == bytes_per_element && (p[-1] & 0x80))
			v = -1;
		for (int i = 0; i < n; i++)
			v = (v << 8) | *--p;
	}
	return v;
}

void ViewBinMode::WriteNumber(char *p, uint64 v, int nchars) const {
	if (flags.test(FLOAT)) {
		int		sb		= int(flags.test(SIGNED));
		int		mb		= mantissa - sb;
		int		eb		= exponent;
		int		bias	= 1024 - (1 << (eb - 1));
		uint64	m		= extract_bits(v, 0, mb);
		int		e		= int(extract_bits(v, mb, eb));
		int		s		= int(extract_bits(v, mb + eb, sb));
		if (e == 0) {
			if (m) {
				int	h = highest_set_index(m);
				m <<= 52 - h;
				e	= bias - mb + h + 1;
				if (e < 0) {
					m >>= -e;
					e = 0;
				}
			}
		} else {
			m	<<= 52 - mb;
			e	+= bias;
		}
		fixed_accum(p, nchars + 1) << formatted(iord(m, e, s).f(), FORMAT::SHORTEST|FORMAT::PRECISION, nchars, CalcDigits(mb + 1, 10));
		return;
	}

	bool negative = flags.test(SIGNED) && int64(v) < 0;
	if (negative)
		v = 0 - v;

	do {
		p[--nchars] = to_digit(v % base);
		v /= base;
	} while (v);

	if (flags.test(SIGNED) && !flags.test(ZEROS))
		p[--nchars] = negative ? '-' : '+';

	while (nchars > int(flags.test_all(SIGNED | ZEROS)))
		p[--nchars] = flags.test(ZEROS) ? '0' : ' ';

	if (flags.test_all(SIGNED | ZEROS))
		p[--nchars] = negative ? '-' : '+';
}


//-----------------------------------------------------------------------------
//	Finder
//-----------------------------------------------------------------------------
//	switches:
//	-a				find all
//	-s<filename>	save
//
//	expression:
//	@<n>			alignment
//	=<ext>			use filehandler
//	"<text>"		literal text
//
//	otherwise:
//	hex hex... text
//-----------------------------------------------------------------------------

void FindPattern::init(const char *pattern) {
	chunks.clear();
	save.clear();
	findall	= false;

	string_scan	ss(pattern);

	if (ss.peekc() == '-') {
		switch (ss.move(1).getc()) {
			case 'a':
				findall = true;
				break;
			case 's':
				save = ss.get_token();
				break;
			default:
				break;
		}
	}


	Chunk	*c = 0;
	while (ss.skip_whitespace().remaining()) {
		switch (ss.peekc()) {
			case '@':
				c	= &chunks.push_back();
				ss.move(1).get(c->align);
				if (ss.peekc() == '+')
					ss.move(1).get(c->align_offset);
				break;

			case '=':
				if (FileHandler *fh = FileHandler::Get(string(ss.move(1).get_token()))) {
					chunks.push_back(fh);
					c = 0;
				}
				break;

			case '"': {
				if (!c)
					c	= &chunks.push_back();
				const char *a = ss.move(1).getp();
				if (ss.scan('"')) {
					c->data	+= const_memory_block(a, ss.getp() - a);
					ss.move(1);
				} else {
					c->data	+= ss.remainder().data();
					return;
				}
				break;
			}

			default: {
				if (!c)
					c	= &chunks.push_back();
				ISO_openarray<uint8>	data;
				if (ReadHexLine(data, ss)) {
					c->data	+= const_memory_block(data.begin(), data.size());
				} else {
					c->data	+= const_memory_block(ss.getp(), ss.remaining());
					return;
				}
				break;
			}
		}

	}
}
uint64 Finder::FindChunkForward(FindPattern::Chunk &chunk, uint64 begin, uint64 end) {
	if (chunk.mode == FindPattern::FILEHANDLER) {
		ISO_OUTPUTF("Trying load of ") << chunk.fh->GetDescription() << " from " << hex(begin) << '\n';
		MemGetterStream	file(getter, begin, end);
		try {
			if (chunk.fh->Read(tag(), file))
				return begin + file.tell();
		} catch (const char *err) {
			ISO_OUTPUTF(err) << '\n';
		}
		return 0;
	}

	current = align(begin - chunk.align_offset, chunk.align);

	uint8	*prev = 0;
	while (current < end && state == 0) {
		memory_block	mb		= getter.Get(current);
		size_t			start	= align(current - chunk.align_offset, chunk.align) - current + chunk.align_offset;
		uint8			*mem	= mb;

		if (prev) {
			for (int64 i = align_down(chunk.data.length(), chunk.align); i > start; i -= chunk.align) {
				if (memcmp(prev - i, chunk.data, i) == 0 && memcmp(mem, chunk.data + i, chunk.data.length() - i) == 0)
					return current - i;
			}
		}

		for (size_t i = start, n = mb.length() - chunk.data.length(); i < n; i += chunk.align) {
			if (memcmp(mem + i, chunk.data, chunk.data.length()) == 0)
				return current + i;
		}
		prev	= (uint8*)mb.end();
		current	+= mb.length();
	}
	return 0;
}

uint64 Finder::FindChunkBackward(FindPattern::Chunk &chunk, uint64 begin, uint64 end) {
	current = align(begin - chunk.align_offset, chunk.align) + chunk.align_offset;
	while (current > 0 && state == 0) {
		current -= chunk.align;
		uint8 *mem = getter.Get(current);
		if (memcmp(mem, chunk.data, chunk.data.length()) == 0)
			return current;
	}
	return 0;
}

uint64 Finder::FindForward(uint64 addr, uint64 end) {
#if 0//def DISASSEMBLER_H
	if (bin->dis_state) {
		while (address < bin->dis_state->Count() && state == 0) {
			buffer_accum<1024>	ba;
			bin->dis_state->GetLine(ba, address);
			if (str((const char*)ba).find(str((const char*)val, len)))
				return address;
			++address;
		}
		return 0;
	}
#endif
	FindPattern::Chunk *chunk	= pattern.chunks.begin();
	for (;;) {
		uint64	found = FindChunkForward(*chunk, addr, end);
		if (found) {
			chunk->begin	= found;
			addr			= found;// + chunk->data.length();
			++chunk;
			if (chunk == pattern.chunks.end())
				return found + chunk[-1].data.length();
		} else {
			if (chunk == pattern.chunks.begin())
				return 0;
			--chunk;
			addr = chunk->begin + 1;
		}
	}
}

uint64 Finder::FindBackward(uint64 addr0, uint64 end) {
#if 0//def DISASSEMBLER_H
	if (bin->dis_state) {
		while (address && state == 0) {
			buffer_accum<1024>	ba;
			bin->dis_state->GetLine(ba, address);
			if (str((const char*)ba).find(str((const char*)val, len)))
				return address;
			--address;
		}
		return 0;
	}
#endif
	FindPattern::Chunk *chunk	= pattern.chunks.begin();
	while (addr0 = FindChunkBackward(*chunk, addr0, end)) {
		chunk->begin	= addr0;
		++chunk;

		uint64	addr = addr0 + chunk->data.length();
		for (;;) {
			uint64	found = FindChunkForward(*chunk, addr, end);
			if (found) {
				chunk->begin	= found;
				addr			= found + chunk->data.length();
				++chunk;
				if (chunk == pattern.chunks.end())
					return addr;
			} else {
				if (--chunk == pattern.chunks.begin())
					break;
				addr = chunk->begin + 1;
			}
		}
	}
	return 0;
}

Finder::Finder(FindPattern &_pattern, const Callback &_cb, MemGetter &_getter, const interval<uint64> &range, bool _fwd)
	: pattern(_pattern), cb(_cb), getter(_getter), fwd(_fwd), state(0)
{
	RunThread([this, range]() {
		uint64	find_addr = range.a;
		do {
			found.b	= fwd ? FindForward(find_addr, range.b) : FindBackward(find_addr, range.b);
			if (!found.b) {
				found.a = found.b = current;
				if (state == 0)
					state = 2;
				break;
			}

			found.a	= pattern.chunks[0].begin;

			if (pattern.save) {
				ISO_OUTPUTF("Found begin:") << hex(found.a) << " end:" << hex(found.b) << '\n';
				filename		fn(pattern.save);
				filename		fn2	= fn.dir().add_dir(fn.name() + format_string("_%llx", found.a)).set_ext(fn.ext());
				malloc_block	buffer(found.b - found.a);
				stream_copy(FileOutput(fn2).me(), MemGetterStream(getter, found.a, found.b).me(), buffer, found.b - found.a);
			}

			find_addr = found.a + 1;
		} while (pattern.findall);

		cb(state, found);
	});
}

//-----------------------------------------------------------------------------
//	BinTextEffect
//-----------------------------------------------------------------------------

struct BinTextEffect : public com<d2d::TextEffect> {
	com_ptr2<ID2D1SolidColorBrush>	bg, fg;

	STDMETHOD(DrawGlyphRun)(void * context, ID2D1DeviceContext *device, D2D1_POINT_2F baseline, const DWRITE_GLYPH_RUN *glyphs, const DWRITE_GLYPH_RUN_DESCRIPTION *desc, DWRITE_MEASURING_MODE measure) {
		DWRITE_FONT_METRICS	metrics;
		glyphs->fontFace->GetMetrics(&metrics);

		if (bg) {
			float	y0	= metrics.ascent * glyphs->fontEmSize / metrics.designUnitsPerEm;
			float	y1	= (metrics.descent + metrics.lineGap) * glyphs->fontEmSize / metrics.designUnitsPerEm;
			float	w	= 0;
			for (const float *i = glyphs->glyphAdvances, *e = i + glyphs->glyphCount; i != e; ++i)
				w += *i;

			d2d::rect	bounds(baseline.x, baseline.y - y0, baseline.x + w, baseline.y + y1);
			device->FillRectangle(&bounds, bg);
		}
		device->DrawGlyphRun(baseline, glyphs, desc, fg, measure);
		return S_OK;
	}
	STDMETHOD(DrawGeometry)(void * context, ID2D1DeviceContext *device, ID2D1Geometry *geometry) {
		if (bg) {
			d2d::matrix			transform = float2x3(identity);
			d2d::rect			bounds;
			geometry->GetBounds(&transform, &bounds);
			device->FillRectangle(&bounds, bg);
		}
		device->FillGeometry(geometry, fg);
		return S_OK;
	}

	template<typename T> static HRESULT Set(IDWriteTextLayout *layout, DWRITE_TEXT_RANGE textRange, const T &t);
};

template<typename T> HRESULT BinTextEffect::Set(IDWriteTextLayout *layout, DWRITE_TEXT_RANGE textRange, const T &t) {
	for (uint32 pos = textRange.startPosition, end = pos + textRange.length, next; pos < end; pos = next) {
		// Get the drawing effect at the current position
		IUnknown			*old = 0;
		DWRITE_TEXT_RANGE	range;
		HRESULT				hr;

		if (FAILED(hr = layout->GetDrawingEffect(pos, &old, &range)))
			return hr;

		// Create a new BinTextEffect or clone the existing one
		BinTextEffect	*effect = old ? new BinTextEffect(*static_cast<BinTextEffect*>(old)) : new BinTextEffect();
		t.ApplyTo(effect);

		next				= range.startPosition + range.length;
		range.startPosition	= pos;
		range.length		= min(end, next) - pos;

		if (FAILED(hr = layout->SetDrawingEffect(effect, range)))
			return hr;
	}
	return S_OK;
}

//-----------------------------------------------------------------------------
//	TextState
//-----------------------------------------------------------------------------

class TextEffect_ForegroundColour : d2d::SolidBrush {
public:
	void	ApplyTo(BinTextEffect *effect) const { effect->fg = *this; }
	TextEffect_ForegroundColour(ID2D1DeviceContext *device, const d2d::colour &col) : d2d::SolidBrush(device, col) {}
};

class TextEffect_BackgroundColour : d2d::SolidBrush {
	com_ptr<ID2D1SolidColorBrush>	fg;
public:
	void	ApplyTo(BinTextEffect *effect) const { effect->bg = *this; if (!effect->fg) effect->fg = fg; }
	TextEffect_BackgroundColour(ID2D1DeviceContext *device, const d2d::colour &col, ID2D1SolidColorBrush *_fg) : d2d::SolidBrush(device, col), fg(_fg) {
		fg->AddRef();
	}
};

struct TextState {
	enum STATE {
		MODE, BG, FG, ALIGN,
	};

	struct change {
		int				offset;
		STATE			state;
		uint32			value;
		change(int _offset, STATE _state, uint32 _value) : offset(_offset), state(_state), value(_value) {}
		bool	operator<(int i) const	{ return offset < i; }
	};
	dynamic_array<change>	changes;

	void	AddChange(int offset, STATE state, uint32 value) {
		auto	i = lower_boundc(changes, offset);
		changes.insert(i, change(offset, state, value));
	}
	void	OutputTo(d2d::Target &target, d2d::Write &write, d2d::Font &font, const d2d::rect &rect, char16 *buffer);
};

void TextState::OutputTo(d2d::Target &target, d2d::Write &write, d2d::Font &font, const d2d::rect &rect, char16 *buffer) {
	d2d::SolidBrush		brush(target, d2d::colour(0,0,0));
	d2d::TextLayout		layout(write, buffer, font, 10000, 1000);
	d2d::TextRenderer	renderer(target, brush);
	renderer.PixelSnapping(false);

	win::Colour	fg(0,0,0);
	win::Colour	bg(255,255,255);
	int			bg_pos = 0, fg_pos = 0;

	for (auto i = changes.begin(), e = changes.end(); i != e; ++ i) {
		int	offset = i->offset;
		switch (i->state) {
			case FG:
				BinTextEffect::Set(layout, d2d::TextLayout::Range(fg_pos, offset - fg_pos), TextEffect_ForegroundColour(target, fg));
				fg		= win::Colour(i->value);
				fg_pos	= offset;
				break;
			case BG:
				BinTextEffect::Set(layout, d2d::TextLayout::Range(bg_pos, offset - bg_pos), TextEffect_BackgroundColour(target, bg, brush));
				bg		= win::Colour(i->value);
				bg_pos	= offset;
				break;
		}
	}
	layout->Draw(0, &renderer, rect.left, rect.top);
}

//-----------------------------------------------------------------------------
//	ViewBin_base
//-----------------------------------------------------------------------------


#ifdef DISASSEMBLER_H
bool ViewBin_base::SymbolFinder(uint64 addr, uint64 &sym_addr, string_param &sym_name) {
	if (auto *s	= upper_boundc(symbols, addr, [](uint64 addr, const Symbol &sym) { return addr < sym.a; })) {
		sym_addr	= s[-1].a;
		sym_name	= s[-1].b;
		return true;
	}
	return false;
}
#endif

void ViewBin_base::Paint(d2d::Target &target, const win::Rect &rect0, int hscroll, int64 scroll, d2d::Write &write, d2d::Font &font) {
	target.SetTransform(translate(0, rect0.top) * float2x3(scale(zoom, zoom)));

	win::Colour	bg	= colour::white;
	win::Colour	fg	= colour::black;
	win::Colour	sel = colour::yellow;

	target.Clear((COLORREF)bg);

	d2d::rect	rect(-hscroll * char_width, 0, rect0.Width() / zoom, rect0.Height() / zoom);
	int			first_line	= int(floor(rect.top / line_height));
	int			last_line	= int(ceil(rect.bottom / line_height));
	uint64		sel0		= min(selection.a, selection.b);
	uint64		sel1		= max(selection.a, selection.b);

	if (is_dis()) {
#ifdef DISASSEMBLER_H
		d2d::SolidBrush	brush(target, d2d::colour(0,0,0));
		for (int i = first_line; i < last_line; i++) {
			rect.bottom	= (rect.top = i * line_height) + line_height;
			int	j = scroll + i;
			if (j < dis_state->Count()) {
				buffer_accum<2048>	ba;
				dis_state->GetLine(ba, j, Disassembler::SHOW_DEFAULT, make_callback(this, &ViewBin_base::SymbolFinder));
				if (j >= sel0 && j < sel1)
					target.Fill(rect, d2d::SolidBrush(target, sel));
				target.DrawText(rect, str16(ba), ba.length(), font, brush);
			}
		}
#endif
	} else {
		int		chars_per_element	= CalcCharsPerElement2();
		int		chars_per_line		= CalcCharsPerLine(bytes_per_line, bytes_per_element, chars_per_element, address_digits, flags.test(ASCII));
		char	*buffer				= alloc_auto(char, chars_per_line + 1);
		uint64	end_addr			= start_addr + getter.length;
		auto	cols				= interval_begin(colours, AddressAtLine(first_line + scroll), AddressAtLine(last_line + scroll));

		for (int i = first_line; i <= last_line; i++) {
			rect.bottom	= (rect.top = i * line_height) + line_height;

			uint64	address = AddressAtLine(i + scroll);
			if (address < end_addr) {
				fixed_accum		acc(buffer, chars_per_line + 1);
				memory_block	mem0	= GetMemory(address),	mem1	= empty;
				size_t			size0	= mem0.length(),		size1	= 0;

				if (size0 < bytes_per_line) {
					if (address + size0 < end_addr) {
						mem1	= GetMemory(address + size0);
						size1	= min(mem1.length(), bytes_per_line - size0);
					}
				} else {
					size0	= bytes_per_line;
				}
				PutLine(acc, address, mem0, size0, mem1, size1, chars_per_element);

				TextState	out;
				while (cols && cols.key().b < address)
					++cols;
				for (auto i = cols; i && i.key().a < address + bytes_per_line; ++i) {
					int	a0, c0 = CharFromOffset(i.key().a < address ? 0 : i.key().a - address, &a0);
					int	a1, c1 = CharFromOffset(i.key().b > address + bytes_per_line ? bytes_per_line : align(uint32(i.key().b - address), bytes_per_element), &a1) - 1;
					out.AddChange(c0, TextState::BG, *i);
					out.AddChange(c1, TextState::BG, bg);
					out.AddChange(a0, TextState::BG, *i);
					out.AddChange(a1, TextState::BG, bg);
				}
				if (sel0 != sel1 && sel0 < address + bytes_per_line && sel1 > address) {
					int	a0, c0 = CharFromOffset(sel0 < address ? 0 : sel0 - address, &a0);
					int	a1, c1 = CharFromOffset(sel1 > address + bytes_per_line ? bytes_per_line : align(uint32(sel1 - address), bytes_per_element), &a1) - 1;
					out.AddChange(c0, TextState::BG, sel);
					out.AddChange(c1, TextState::BG, bg);
					out.AddChange(a0, TextState::BG, sel);
					out.AddChange(a1, TextState::BG, bg);
				}

				if (prev_bin) {
					const uint8 *curr	= mem0;
					const uint8 *prev	= (uint8*)prev_bin + address;

					for (uint32 i = 0; i < bytes_per_line; ) {
						while (i < bytes_per_line && curr[i] == prev[i])
							++i;

						out.AddChange(CharFromOffset(align_down(i, bytes_per_element), 0), TextState::FG, RGB(192,0,0));
						while (i < bytes_per_line && curr[i] != prev[i])
							++i;
						i = align(i, bytes_per_element);
						out.AddChange(CharFromOffset(i, 0), TextState::FG, fg);
					}
					if (flags.test(ASCII)) {
						int	c1;
						for (uint32 i = 0; i < bytes_per_line; ) {
							while (i < bytes_per_line && curr[i] == prev[i])
								++i;

							CharFromOffset(i, &c1);
							out.AddChange(c1, TextState::FG, RGB(192,0,0));
							while (i < bytes_per_line && curr[i] != prev[i])
								++i;

							CharFromOffset(i, &c1);
							out.AddChange(c1, TextState::FG, fg);
						}
					}
				} else {
#if 0
					const uint8 *curr	= mem0;
					uint8	prev_col	= 0xff;
					for (uint32 i = 0; i < bytes_per_line; i++) {
						uint8	col = curr[i] / 8;
						if (col != prev_col) {
							//out.AddChange(CharFromOffset(i, 0), TextState::FG, col);//RGB((col & 3) * 255 / 3, ((col >> 2) & 3) * 255 / 3, (col >> 4) * 255));
							out.AddChange(CharFromOffset(i, 0), TextState::FG, RGB((col & 3) * 255 / 3, ((col >> 2) & 3) * 255 / 3, (col >> 4) * 255));
							prev_col = col;
						}
					}
#endif
				}

				out.OutputTo(target, write, font, rect, string16(buffer, chars_per_line));
			}
		}
	}
}


int ViewBin_base::OffsetFromChar(int x, bool *is_ascii) {
	if (is_ascii)
		*is_ascii = false;
	if (!is_dis()) {
		int		chars_per_element	= CalcCharsPerElement2();
		int		elements_per_line	= bytes_per_line / bytes_per_element;
		if ((x -= address_digits + 2) >= 0) {
			if (flags.test(ASCII)) {
				int	a = (chars_per_element + 1) * elements_per_line;
				if (x >= a) {
					if (is_ascii)
						*is_ascii = true;
					return x - a;
				}
			}
			return min(x / (chars_per_element + 1), elements_per_line - 1) * bytes_per_element;
		}
	}
	return 0;
}

int ViewBin_base::CharFromOffset(int off, int *ascii) {
	int		chars	= CalcCharsPerElement2() + 1;
	if (ascii)
		*ascii = (bytes_per_line / bytes_per_element) * chars + address_digits + 2 + off;
	return	(off / bytes_per_element) * chars + address_digits + 2;
}

void ViewBin_base::PutNumber(char *string, const uint8 *mem, int nchars) {
	uint64	v = LoadNumber(mem);
	WriteNumber(string, v, nchars);
}

bool ViewBin_base::GetNumber(char *string, uint8 *mem) {
	uint64	v = 0;
	if (flags.test(FLOAT)) {
		iord	d(0.0);
		if (sscanf(string, "%lf", (double*)&d) == 0)
			return false;
		int		m = mantissa - flags.test(SIGNED), e = exponent;
		v	= (d.m >> (52 - m)) | ((d.e - 1024 + (uint64(1) << (e - 1))) << m) | (d.s << (m + e));
	} else {
		bool	neg = string[0] == '-';
		if (neg)
			string++;
		while (char c = *string++)
			v = v * base + from_digit(c);
		if (neg)
			v = -v;
	}
	if (flags.test(BIGENDIAN)) {
		for (int i = bytes_per_element; i--; v >>= 8)
			mem[i] = v;
	} else {
		for (int i = bytes_per_element; i--; v >>= 8)
			*mem++ = v;
	}
	return true;
}

void ViewBin_base::PutLine(string_accum &acc, uint64 address, const uint8 *mem0, size_t size0, const uint8 *mem1, size_t size1, int chars_per_element) {
	if (!mem0)
		size0 = 0;

	acc.format("%0*I64x: ", address_digits, uint64(address));

	size_t	size	= size0 + size1;
	size_t	j		= 0;
	for (; j < size; j += bytes_per_element) {
		PutNumber(acc.getp(chars_per_element), j < size0 ? mem0 + j : mem1 + j - size0, chars_per_element);
		acc << ' ';
	}
	for (; j < bytes_per_line; j += bytes_per_element) {
		acc << repeat('-', chars_per_element) << ' ';
	}

	if (flags.test(ASCII)) {
		for (int j = 0; j < size; j++) {
			uint8	t =  j < size0 ? mem0[j] : mem1[j - size0];
			acc << (is_print(t) ? char(t) : '.');
		}
		if (bytes_per_line - size > 0)
			acc << repeat(' ', int(bytes_per_line - size));
	}
}

//-----------------------------------------------------------------------------
//	Disassembler
//-----------------------------------------------------------------------------

#ifdef DISASSEMBLER_H

class DisassemblerHex : public Disassembler {
public:
	virtual	const char*	GetDescription()	{ return "Hex"; }
	virtual void		Disassemble(const_memory_block block, uint64 start, dynamic_array<string> &lines, SymbolFinder sym_finder, const char *strings, uint32 strings_len);
	virtual State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
		StateDefault	*state = new StateDefault;
		Disassemble(block, addr, state->lines, sym_finder, 0, 0);
		return state;
	}
} dishex;

void DisassemblerHex::Disassemble(const_memory_block block, uint64 start, dynamic_array<string> &lines, SymbolFinder sym_finder, const char *strings, uint32 strings_len) {
	ViewBinMode		mode	= ViewBinMode::Current();
	uint32			bpl		= mode.flags.test(ViewBinMode::AUTOSIZE) ? 16 : mode.bytes_per_line;
	int				cpe		= mode.CalcCharsPerElement2();

	for (uint32 offset = 0; offset < block.length(); ) {
		uint64	addr = start + offset;
		//while (s != se && addr == s->a)
		//	lines.push_back(s++->b);

		int	n	= min(block.length() - offset, bpl);
//		if (s != se)
//			n = min(n, s->a - addr);

		buffer_accum<1024>	ba("%08x    ", addr);
		if (mode.bytes_per_element == 1) {
			while (n--) {
				uint8	b = ((const uint8*)block)[offset++];
				ba << to_digit(b >> 4) << to_digit(b & 15);
			}
		} else {
			while (n > 0) {
				uint64	v = mode.LoadNumber((const uint8*)block + offset, n);
				if (v && v < strings_len && strings[v - 1] == 0) {
					ba << '"' << (strings + v) << '"';
				} else {
					uint64			sym_addr;
					string_param	sym_name;
					if (sym_finder && sym_finder(offset, sym_addr, sym_name) && sym_addr == offset)
						ba << sym_name;
					else
						mode.WriteNumber(ba.getp(cpe), v, cpe);
				}
				ba << ' ';
				offset	+= mode.bytes_per_element;
				n		-= mode.bytes_per_element;
			}
		}

		lines.push_back(ba);
	}
}
/*
class DisassemblerString : public Disassembler {
public:
	struct symbol_sorter {
		bool operator()(const Symbol &a, const Symbol &b) { return a.a < b.a; }
	};
	virtual	const char*	GetDescription()	{ return "Strings"; }
	virtual void		Disassemble(void *bin, uint32 len, dynamic_array<string> &lines, const SymbolList &symbols);
} disstring;

void DisassemblerString::Disassemble(void *bin, uint32 len, dynamic_array<string> &lines, const SymbolList &symbols) {
	SymbolList	symbols2(symbols);
	sort(symbols2, symbol_sorter());
	const Symbol	*s	= symbols2.begin(), *se = symbols2.end();

	for (uint32 offset = 0; offset < len; ) {
		if (offset == s->a)
			lines.push_back(s++->b);

		int	n	= min(len - offset, 256);
		if (s < se)
			n = min(n, s->a - offset);

		buffer_accum<1024>	ba("%08x    \"", offset);

		char	b;
		while (n--) {
			if (!(b = ((char*)bin)[offset++]))
				break;
			ba << b;
		}

		ba << '"';
		if (b == 0)
			ba << ';';
		lines.push_back((const char*)ba);
	}
}
*/

#endif

} // namespace app

