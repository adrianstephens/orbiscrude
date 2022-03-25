#include "bin.h"
#include "base/vector.h"
#include "base/algorithm.h"
#include "base/tree.h"
#include "thread.h"
#include "stream.h"
#include "extra/text_stream.h"
#include "windows/d2d.h"
#include "iso/iso_files.h"

#ifdef ISO_EDITOR
#include "disassembler.h"
#endif

#ifndef PLAT_WINRT
#include "windows/control_helpers.h"
#endif

namespace app {
using namespace iso;

size_t ReadHexLine(ISO_openarray<uint8> &array, string_scan &ss);

//-----------------------------------------------------------------------------
//	ViewBinMode
//-----------------------------------------------------------------------------

struct ViewBinMode {
	static int		CalcDigits(int bits, int base) {
		return int(ceil(bits / log2(float(base))));
	}
	static int		CalcCharsPerElement(int base, int mantissa, int exponent, bool sign) {
		return CalcDigits(mantissa - sign, base) + (exponent ? (3 + CalcDigits(exponent, base)) : 0) + sign;
	}
	static int		CalcCharsPerLine(int bytes_per_line, int bytes_per_element, int chars_per_element, int address_digits, bool ascii) {
		return (chars_per_element + 1) * max(bytes_per_line / bytes_per_element, 1) + (ascii ? bytes_per_line : 0) + address_digits + 2;
	}
	static int		CalcBytesPerLine(float width, float char_width, int bytes_per_element, int chars_per_element, int address_digits, bool ascii) {
		int	chars_per_line		= int(width / char_width) - (address_digits + 2 + int(ascii));	// after address and spaces
		int	chars_per_element2	= chars_per_element + 1 + (ascii ? bytes_per_element : 0);
		return max(chars_per_line / chars_per_element2, 1) * bytes_per_element;
	}

	enum FLAGS {
		AUTOSIZE	= 1 << 0,
		ASCII		= 1 << 1,
		SIGNED		= 1 << 2,
		ZEROS		= 1 << 3,
		BIGENDIAN	= 1 << 4,
		FLOAT		= 1 << 5,
	};
	iso::flags<FLAGS,uint8>	flags;
	uint8	bytes_per_element;
	uint8	base, mantissa, exponent;
	uint16	bytes_per_line;

	static ViewBinMode	&Current()	{
		static ViewBinMode mode; return mode;
	}

	void	SetBytesPerElement(int n) {
		switch (bytes_per_element = n) {
			case 1: mantissa = 5; exponent = 3; break;
			case 2: mantissa = 11; exponent = 5; break;
			case 4: mantissa = 24; exponent = 8; break;
			case 8: mantissa = 53; exponent = 11; break;
		}
	}

	bool	SetBytesPerLine(int n) {
		if (n == bytes_per_line)
			return false;
		bytes_per_line	= n;
		return true;
	}

	bool	SetFixedBytesPerLine(int n) {
		flags.clear(AUTOSIZE);
		return SetBytesPerLine(n);
	}

	int		CalcCharsPerElement2() {
		return flags.test(FLOAT)
			? CalcCharsPerElement(10, mantissa + 1, exponent, flags.test(SIGNED))
			: CalcCharsPerElement(base, bytes_per_element * 8, 0, flags.test(SIGNED));
	}

	uint64	LoadNumber(const uint8 *p, uint32 n = 0)	const;
	void	WriteNumber(char *p, uint64 v, int nchars)	const;

	ViewBinMode() : flags(ZEROS | ASCII | AUTOSIZE), base(16), bytes_per_line(32) {
		SetBytesPerElement(1);
	}
};

//-----------------------------------------------------------------------------
//	MemGetter
//-----------------------------------------------------------------------------

struct MemGetter {
	ISO::Browser2			b;
	void					*bin;
	uint64					length;
	BigBinCache				cache;

	void Init(const ISO::Browser2 &_b) {
		b		= _b;
		bin		= 0;
		length	= 0;

		if (b.GetType() == ISO::ARRAY) {
			bin		= b;
		} else {
			bin		= *b;
			if (b.Is("BigBin") || !bin) {
				if (uint32 num_blocks = b.Count()) {
					uint32	block_size = b[0].Count();
					cache.reset(block_size);
					length	= uint64(block_size) * (num_blocks - 1) + b[num_blocks - 1].Count();
				}
			}
		}

		if (length == 0) {
			bin		= b[0];
			length	= b.Count() * b[0].GetSize();
		}
	}

	memory_block Get(uint64 address) {
		if (bin)
			return memory_block(bin, length).slice(uint32(address));
		return cache.get_mem(b, address);
	}
	void	Flush() {
		if (!bin)
			cache.flush();
	}
};

struct MemGetterStream : reader_mixin<MemGetterStream> {
	MemGetter	&getter;
	streamptr	begin, end, current;

	MemGetterStream(MemGetter &getter, streamptr begin, streamptr end) : getter(getter), begin(begin), end(end), current(begin) {}

	void		seek(streamptr offset)				{ current = begin + offset; }
	streamptr	tell()								{ return current - begin;	}
	streamptr	length()							{ return end - begin; }
	size_t		readbuff(void *buffer, size_t size)	{
		uint8	*p	= (uint8*)buffer;
		uint8	*pe	= p + size;
		while (p < pe) {
			memory_block	mb	= getter.Get(current);
			size_t			n	= min(min(mb.length(), pe - p), end - current);
			if (n == 0)
				break;
			memcpy(p, mb, n);
			p		+= n;
			current += n;
		}
		return p - (uint8*)buffer;
	}
};

//-----------------------------------------------------------------------------
//	Finder
//-----------------------------------------------------------------------------

struct FindPattern {
	enum MODE {
		ALIGN,
		OFFSET,
		FILEHANDLER,
	};
	struct Chunk {
		MODE			mode;
		union {
			struct {
				uint32		align, align_offset;
			};
			FileHandler		*fh;
		};
		uint64			begin;
		malloc_block	data;
		Chunk() : mode(ALIGN), align(1), align_offset(0) {}
		Chunk(FileHandler *fh) : mode(FILEHANDLER), fh(fh) {}
	};
	dynamic_array<Chunk>	chunks;
	bool					findall;
	string					save;
	void init(const char *pattern);
};

struct Finder {
	typedef callback<bool(int, interval<uint64>)> Callback;

	FindPattern			&pattern;
	Callback			cb;
	MemGetter			getter;
	interval<uint64>	found;
	uint64				current;
	bool				fwd;
	volatile int		state;

	uint64	FindChunkForward(FindPattern::Chunk &chunk, uint64 begin, uint64 end);
	uint64	FindChunkBackward(FindPattern::Chunk &chunk, uint64 begin, uint64 end);
	uint64	FindForward(uint64 addr, uint64 end);
	uint64	FindBackward(uint64 addr0, uint64 end);
	Finder(FindPattern &_pattern, const Callback &_cb, MemGetter &_getter, const interval<uint64> &range, bool _fwd);
};


//-----------------------------------------------------------------------------
//	ViewBin_base
//-----------------------------------------------------------------------------

class ViewBin_base : protected ViewBinMode {
protected:
	MemGetter				getter;
	uint64					start_addr;
	malloc_block			prev_bin;
	fixed_string<1024>		find_text;
	uint64					offset;
	float					zoom;
	interval<uint64>		selection;
	uint64					edit_addr;
	int						prevz;
	bool					edit_ascii;
	bool					read_only;

	float					char_width, line_height;
	int						address_digits;

	unique_ptr<Finder>		finder;

	map<interval<uint64>, win::Colour>	colours;

#ifdef DISASSEMBLER_H
	typedef pair<uint64, const char*>	Symbol;

	Disassembler					*disassembler = 0;
	unique_ptr<Disassembler::State>	dis_state;
	dynamic_array<Symbol>			symbols;
	Disassembler::SymbolFinder		symbol_finder;
	ISO::Browser					strings;

	bool SymbolFinder(uint64 addr, uint64 &sym_addr, string_param &sym_name);

	struct line_iterator {
		typedef uint64 element, &reference;
		typedef random_access_iterator_t	iterator_category;
		Disassembler::State *state;
		int	i;
		line_iterator(Disassembler::State *_state, int _i) : state(_state), i(_i) {}
		line_iterator	&operator++()								{ ++i; return *this; }
		line_iterator	operator+(size_t j)							{ return line_iterator(state, i + (int)j); }
		int				operator-(const line_iterator &b)	const	{ return i - b.i; }
		bool			operator!=(const line_iterator &b)	const	{ return i != b.i; }
		uint64			operator*()							const	{ return state->GetAddress(i); }
	};

	uint64	IndexToAddress(uint64 i) {
		return dis_state ? dis_state->GetAddress(int(i)) : i;
	}
	constexpr bool is_dis() const { return !!dis_state; }
#else
	constexpr bool is_dis() const { return false; }
#endif

	void			FlushMemory()				{ getter.Flush(); }
	memory_block	GetMemory(uint64 address)	{ return getter.Get(address - start_addr); }
	void			PutNumber(char *string, const uint8 *mem, int nchars);
	void			PutLine(string_accum &acc, uint64 address, const uint8 *mem0, size_t size0, const uint8 *mem1, size_t size1, int chars_per_element);
	bool			GetNumber(char *string, uint8 *mem);
	int				OffsetFromChar(int x, bool *is_ascii = 0);
	int				CharFromOffset(int off, int *ascii);

	int				CalcBytesPerLine2(float width) {
		return CalcBytesPerLine(width, char_width, bytes_per_element, CalcCharsPerElement2(), address_digits, flags.test(ASCII));
	}
	uint64			AddressAtLine(int64 line) {
		return is_dis() ? line : line * bytes_per_line + offset + start_addr;
	}

public:
	ViewBin_base() : ViewBinMode(ViewBinMode::Current()), offset(0), zoom(1), selection(0, 0), prevz(0), read_only(false) {}

	void SetBinary(const ISO::Browser2 &b) {
		if (b.Is("StartBin")) {
			start_addr	= b[0].Get(uint64(0));
			getter.Init(b[1]);
		} else {
			start_addr	= 0;
			getter.Init(b);
		}

		address_digits	= (log2(start_addr + getter.length) + 3) / 4;
	}

	void	SetFontMetrics(d2d::Write &write, d2d::Font &font) {
		d2d::TextLayout				layout(write, L"x", font, 1000, 1000);
		DWRITE_TEXT_METRICS			metrics;
		layout->GetMetrics(&metrics);
		char_width		= metrics.width;
		line_height		= metrics.height;
	}

	void Paint(d2d::Target &target, const win::Rect &rect, int hscroll, int64 scroll, d2d::Write &write, d2d::Font &font);

public:
	static bool Matches(const ISO::Type *type) {
		return	type->Is("BigBin")
			||	type->Is("StartBin")
			||	type->Is("Bin")
			||	(is_any(type->GetType(), ISO::OPENARRAY, ISO::ARRAY) && TypeType(type->SubType()->SkipUser()) == ISO::INT);
	}
};

#ifndef PLAT_WINRT
//-----------------------------------------------------------------------------
//	ViewBin
//-----------------------------------------------------------------------------
class ViewBin : ViewBin_base, public win::Window<ViewBin> {
	d2d::WND				target;
	d2d::Write				write;
	d2d::Font				font;

	win::ToolBarControl		toolbar;
	win::Rect				rects[2];
	win::EditControl2		miniedit;
	win::ToolTipControl		tooltip;
	win::ProgressBarControl	prog;
	uint64					prog_start;
	uint32					prog_shift;
	int						tip_state;
	win::TScrollInfo<int64> si;
	win::ScrollInfo			sih;
	win::Point				mouse_down;
	int64					mouse_pos;
	float					mouse_frac;
	float					mouse_inertia;
	UINT_PTR				timer;
	HFONT					hFont;
	FindPattern				find_pattern;

	uint64	AddressAtLine(int line) {
		return ViewBin_base::AddressAtLine(si.Pos() + line);
	}
	uint64	AddressFromWindow(const win::Point &p, bool *is_ascii = 0) {
		int	x = OffsetFromChar(int(p.x / (char_width * zoom)), is_ascii);
		return x >= bytes_per_line
			? ~uint64(0)
			: AddressAtLine(int((p.y - rects[1].top) / (line_height * zoom))) + x;
	}

	void		Find(const interval<uint64> &range, bool fwd);
	bool		CheckFinding();
	void		AbortFind();
	win::Point	CalcPoint(uint64 a, bool br, bool ascii);
	win::Rect	CalcRect(uint64 a, bool ascii);
	win::Rect	CalcRect(uint64 a, uint64 b);
	int			CalcRegion(uint64 a, uint64 b, win::Rect *rects);
	void		ChangedBytesPerLine(uint64 address);
	void		ChangedSize();
	void		Changed();
	void		Goto(uint64 address);
	void		Select(const interval<uint64> &i);
	void		Check(uint32 id, bool check);
	void		Radio(uint32 id);

	int		CalcBytesPerLine2(float width) {
		return CalcBytesPerLine(width, char_width, bytes_per_element, CalcCharsPerElement2(), address_digits, flags.test(ASCII));
	}
	void	SetFixedBytesPerLine(int n) {
		uint64 address	= AddressAtLine(0);
		if (ViewBinMode::SetFixedBytesPerLine(n))
			ChangedBytesPerLine(address);
	}

	string_accum&	GetTipText(string_accum &acc, uint64 addr) {
		return acc.format("%0*I64x", address_digits, uint64(addr));
	}
#ifdef DISASSEMBLER_H
	void	SetDisassembler(Disassembler *d);
#endif

	void	VScroll(float y);
public:
	bool	operator()(int state, interval<uint64> &found);

	LRESULT	Proc(UINT message, WPARAM wParam, LPARAM lParam);
	int		GetMouseWheel(WPARAM wParam);
	ViewBin(const win::WindowPos &wpos, const char *title, const ISO::Browser2 &b, win::ID id = win::ID());
};
#endif //PLAT_WINRT

} // namespace app

