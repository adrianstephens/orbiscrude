#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "base/bits.h"
#include "base/array.h"
#include "base/strings.h"
#include "base/hash.h"
#include "base/algorithm.h"
#include "base/constants.h"
#include "stream.h"

namespace iso {

class Disassembler : public static_list<Disassembler> {
public:
	enum {
		FLAG_JMP			= 1 << 0,
		FLAG_CALLRET		= 1 << 1,
		FLAG_CONDITIONAL	= 1 << 2,
		FLAG_RELATIVE		= 1 << 3,
		FLAG_INDIRECT		= 1 << 4,

		//FLAG_JMP|FLAG_CALLRET		= CALL
		//FLAG_CALLRET				= RET
		//FLAG_JMP|FLAG_CONDITIONAL	= CONDITIONAL BRANCH
		//FLAG_CONDITIONAL			= REPNE, etc
	};
	enum MODE {
		SHOW_SOURCE		= 1 << 0,	// implied by locs
		SHOW_ALLSOURCE	= 1 << 1,	// implied by locs
		SHOW_ADDRESS	= 1 << 2,
		SHOW_BYTES		= 1 << 3,
		SHOW_SYMBOLS	= 1 << 4,
		SHOW_LINENOS	= 1 << 5,	// handled elsewhere
		STEP_SOURCE		= 1 << 6,
		SHOW_DEFAULT	= SHOW_SOURCE | SHOW_ADDRESS | SHOW_BYTES | SHOW_SYMBOLS
	};
	friend MODE		operator^(MODE a, MODE b)	{ return MODE((int)a ^ (int)b); }
	friend MODE&	operator^=(MODE& a, MODE b) { return a = a ^ b; }
	friend MODE&	operator-=(MODE& a, MODE b) { return a = MODE(a & ~b); }
	friend MODE&	operator|=(MODE& a, MODE b) { return a = MODE(a | b); }

	struct InstructionInfo {
		uint32	offset;	//or len
		union {
			uint32	flags;
			struct {uint32	jmp:1, callret:1, conditional:1, relative:1, indirect:1, nest:4;};
		};
		uint64	dest;
		InstructionInfo(uint32 offset) : offset(offset), flags(0), dest(0) {}
	};

	typedef callback_ref<bool(uint64, uint64&,string_param&)>	SymbolFinder;
	typedef async_callback<bool(uint64, uint64&,string_param&)>	AsyncSymbolFinder;

	struct State {
		virtual ~State()	{}
		virtual	int			Count()				const = 0;
		virtual	void		GetLine(string_accum &a, int i, int flags = SHOW_DEFAULT, SymbolFinder sym_finder = none) const = 0;
		virtual	uint64		GetAddress(int i)	const	{ return 0; }

		struct iterator {
			const State	*s;
			int		i;
			iterator(const State *s, int i) : s(s), i(i) {}
			uint64		operator*()						const		{ return s->GetAddress(i); }
			iterator&	operator++()								{ ++i; return *this; }
			iterator&	operator--()								{ --i; return *this; }
			bool		operator==(const iterator &b)	const		{ return i == b.i; }
			bool		operator!=(const iterator &b)	const		{ return i != b.i; }
			friend intptr_t	operator-(const iterator &a, const iterator &b)	{ return a.i - b.i; }
			friend iterator	operator+(const iterator &a, int b)		{ return iterator(a.s, a.i + b); }
			friend iterator	operator-(const iterator &a, int b)		{ return iterator(a.s, a.i - b); }
		};

		iterator	begin()						const	{ return iterator(this, 0); }
		iterator	end()						const	{ return iterator(this, Count()); }
		uint64		LineToAddress(int i)		const	{ return i < Count() ? GetAddress(i) : 0; }
		int			AddressToLine(uint64 addr)	const	{ return addr ? iso::lower_boundc(*this, addr).i : 0; }

		template<typename L> bool			IgnoreLoc(L loc, uint64 base, bool combine_lines) {
			return combine_lines && loc[0].line == loc[-1].line && GetAddress(AddressToLine(loc[-1].offset + base) + 1) >= base + loc[0].offset;
		}
		template<typename L> iterator_t<L>	MixedLineToSource(const L &source, int &line, uint64 base);
		template<typename L> int			AddressToMixedLine(const L &source, uint64 addr, uint64 base);
	};

	struct Location {
		uint32	offset;
		uint32	file, line;
		uint16	start_col, end_col;
		uint32	func_id	= 0;
		Location(uint32 offset, uint32 file, uint32 line, uint16 start_col = 0, uint16 end_col = 0) : offset(offset), file(file), line(line), start_col(start_col), end_col(end_col) {}
		operator uint32() const { return offset; }
	};

	struct Locations : dynamic_array<Location> {
		const Location*	next(uint32 offset) const {
			auto i = lower_boundc(*this, offset);
			return i == end() ? nullptr : i;
		}
		const Location*	find(uint32 offset) const {
			auto i = lower_boundc(*this, offset);
			return i == end() || offset < i->offset ? nullptr : i;
		}
		const Location*	find(uint32 file, uint32 line) const {
			uint32			best	= maximum;
			const Location *loc		= 0;
			for (auto &i : *this) {
				if (i.file == file && abs(int(i.line) - int(line)) < best) {
					best	= abs(int(i.line) - int(line));
					loc		= &i;
				}
			}
			return loc;
		}
		dynamic_array<const Location*>	find_all(uint32 file, uint32 line) const {
			dynamic_array<const Location*>	locs;
			uint32			best	= maximum;
			const Location *loc		= 0;
			for (auto &i : *this) {
				if (i.file == file) {
					if (i.line == line) {
						best	= 0;
						locs.push_back(&i);
					} else if (abs(int(i.line) - int(line)) < best) {
						best	= abs(int(i.line) - int(line));
						loc		= &i;
					}
				}
			}
			if (loc && locs.empty())
				locs.push_back(loc);
			return locs;
		}
	};

	struct File : refs<File> {
		string			name;
		malloc_block	source;
		dynamic_array<pair<uint32,uint32>>	token_starts;

		File(string &&name, malloc_block &&source) : name(move(name)), source(move(source)) {}
		File(File &&b)				= default;
		File&	operator=(File &&b)	= default;

		range<const char*> get_line(int line) const {
			const char *p = source, *e = source.end();
			while (p && --line) {
				if (p = string_find(p, e, '\n'))
					++p;
			}
			if (!p)
				e = p;
			else if (auto end = string_find(p, e, '\n'))
				e = end;
			return {p, e};
		}
		int get_line_num(const char *p) const {
			return string_count((const char*)source, p, '\n');
		}
	};

	struct SharedFiles : hash_map<const char*, ref_ptr<File>> {
		File*		get(const char* fn) {
			auto	f = (*this)[fn];
			if (!f.exists())
				f = new File(fn, malloc_block::unterminated(FileInput(fn)));
			return *f;
		}
	};

	struct Files : hash_map_with_key<uint32, ref_ptr<File>> {
		range<const char*>	get_line(uint32 file, uint32 line) const {
			if (auto f = (*this)[file].or_default())
				return f->get_line(line);
			return {0, 0};
		}
		range<const char*>	get_line(const Location &loc) const			{ return get_line(loc.file, loc.line); }
		void	add(uint32 id, string &&name, malloc_block &&source)	{ (*this)[id] = new File(move(name), move(source)); }
	};

	struct StateDefault : State {
		dynamic_array<string>				lines;
		int		Count()												const override	{ return lines.size32(); }
		void	GetLine(string_accum &a, int i, int, SymbolFinder)	const override	{ if (i < lines.size32()) a << lines[i]; }
	};

	struct StateDefault2 : State {
		dynamic_array<pair<string,uint64>>	lines;
		int		Count()												const override	{ return lines.size32(); }
		void	GetLine(string_accum &a, int i, int, SymbolFinder)	const override	{ if (i < lines.size32()) a << lines[i].a; }
		uint64	GetAddress(int i)									const override	{ return lines[min(i, lines.size32() - 1)].b; }
	};

	static	Disassembler *Find(const char *desc) {
		for (iterator i = begin(); i != end(); ++i) {
			if (str(i->GetDescription()) == desc)
				return i;
		}
		return 0;
	}

	virtual	const char*	GetDescription()=0;
	virtual State*		Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder = none);
	virtual InstructionInfo	GetInstructionInfo(const_memory_block block) {
		State	*state = Disassemble(block, 0);
		return state->Count() != 0 ? uint32(state->GetAddress(1)) : 0;
	}
	virtual bool		Assemble(const char *line, uint64 addr, SymbolFinder sym_finder = none)	{ return false; }
	virtual void		DisassembleLine(string_accum &a, const void *data, uint64 addr, SymbolFinder sym_finder = none) {}
};

template<typename L> int Disassembler::State::AddressToMixedLine(const L &source, uint64 addr, uint64 base) {
	using iso::index_of;
	int		line	= AddressToLine(addr);
	auto	loc		= lower_boundc(source, addr - base);
	return line + int(index_of(source, loc));
}

template<typename L> iterator_t<L> Disassembler::State::MixedLineToSource(const L &source, int &line, uint64 base) {
	auto	loc = lower_boundc(int_range(int(source.size())), line, [this, source, base](int i, int line) {
		return AddressToLine(source[i].offset + base) < line - i;
	});
	if (loc == source.size())
		--loc;
	line -= *loc;
	return source.begin() + *loc;
}

}

#endif	// DISASSEMBLER_H
