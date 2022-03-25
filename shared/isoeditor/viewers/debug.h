#ifndef DEBUG_H
#define DEBUG_H

#include "windows/control_helpers.h"
#include "windows/text_control.h"
#include "windows/docker.h"
#include "windows/splitter.h"
#include "windows/treecolumns.h"

#include "base/array.h"
#include "disassembler.h"

#include "extra/identifier.h"
#include "extra/colourise.h"
#include "extra/c-types.h"
#include "extra/memory_cache.h"
#include "extra/ast.h"
#include "natvis.h"

namespace app {
using namespace iso;
using namespace win;

void			SetSourceWindow(win::RichEditControl &text, const SyntaxColourer &colourer, const memory_block &s, int *active, size_t num_active);
EditControl		MakeSourceWindow(const WindowPos &wpos, const char *title, const SyntaxColourer &colourer, const memory_block &s, int *active, size_t num_active, Control::Style style, Control::StyleEx styleEx = Control::NOEX);

void			ShowSourceLine(EditControl edit, uint32 line);
void			ShowSourceTabLine(TabControl2 tabs, uint32 file, uint32 line);
void			ShowSourceTabLine(TabControl2 tabs, const Disassembler::Location *loc);

void			DumpDisassemble(win::RichEditControl &text, Disassembler::State *state, int flags, Disassembler::SymbolFinder sym_finder);

//-----------------------------------------------------------------------------
//	CodeHelper
//-----------------------------------------------------------------------------

class CodeHelper {
protected:
	const SyntaxColourerRE&			colourer;
	Disassembler::AsyncSymbolFinder	sym_finder;
	int								mode;
	dynamic_array<int>	loc_indices;
	dynamic_array<int>	loc_indices_combine;

	auto	GetLocations() const {
		return make_indexed_container(locations, make_const(loc_indices_combine));
	}
public:
	uint64							base;
	unique_ptr<Disassembler::State>	state;
	Disassembler::Locations			locations;

	CodeHelper(const SyntaxColourerRE &colourer, Disassembler::AsyncSymbolFinder &&sym_finder, int mode)
		: colourer(colourer), sym_finder(move(sym_finder)), mode(mode), base(0) {}

	void	FixLocations(uint64 base);
	void	SetDisassembly(RichEditControl c, Disassembler::State *_state);
	void	SetDisassembly(RichEditControl c, Disassembler::State *_state, const Disassembler::Files &files);

	void	ShowCode(EditControl c, const Disassembler::Location *loc) const {
		if (loc) {
			c.SetSelection(c.GetLineRange(OffsetToLine(loc->offset)));
			c.EnsureVisible();
		}
	}
	void	ShowCode(EditControl c, int file, int line)	const { ShowCode(c, locations.find(file, line)); }
	uint64	LineToAddress(int line)						const {
		if (mode & Disassembler::SHOW_SOURCE)
			state->MixedLineToSource(GetLocations(), line, base);
		return state->LineToAddress(line);
	}

	auto	LineToSource(int line)						const { return mode & Disassembler::SHOW_SOURCE ? &*state->MixedLineToSource(GetLocations(), line, base) : locations.find(state->LineToAddress(line) - base); }
	uint32	AddressToLine(uint64 addr)					const { return mode & Disassembler::SHOW_SOURCE ? state->AddressToMixedLine(GetLocations(), addr, base) : state->AddressToLine(addr); }
	void	GetDisassembly(string_accum &sa, int line)	const {
		if (mode & Disassembler::SHOW_SOURCE)
			state->MixedLineToSource(GetLocations(), line, base);
		state->GetLine(sa, line, mode, sym_finder);
	}

	uint32	LineToOffset(int line)						const { return LineToAddress(line) - base;	}
	uint32	OffsetToLine(uint32 offset)					const { return AddressToLine(base + offset); }

	void	Select(EditControl click, EditControl dis, TabControl2 *tabs);
	void	SourceTabs(TabControl2 &tabs, const Disassembler::Files &files);
};

//-----------------------------------------------------------------------------
//	DebugWindow
//-----------------------------------------------------------------------------

class DebugWindow : public D2DTextWindow, public CodeHelper {
protected:
	ImageList			images;
	uint32				pc_line;
	int					orig_mode;

public:
	enum {
		ID_DEBUG_BREAKPOINT	= 1000,
		ID_DEBUG_SHOWSOURCE,
		ID_DEBUG_COMBINESOURCE,
		ID_DEBUG_STEPOVER,
		ID_DEBUG_STEPBACK,
		ID_DEBUG_RUN,
		ID_DEBUG_PIXEL,
		ID_DEBUG_STEPIN,
		ID_DEBUG_STEPOUT,
		ID_DEBUG_RUNTO,
	};

	Disassembler::Files		files;
	dynamic_array<int>		bp;

	static Accelerator GetAccelerator();

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	DebugWindow(const SyntaxColourerRE &colourer, Disassembler::AsyncSymbolFinder &&sym_finder, int mode);

	bool	ToggleBreakpoint(int y) {
		auto	i = lower_boundc(bp, y);
		bool	r = i == bp.end() || *i != y;
		if (r)
			bp.insert(i, y);
		else
			bp.erase(i);
		Invalidate(Margin());
		return r;
	}

	void	SetMode(int mode);

	HWND	Create(const WindowPos &pos, const char *_title, Style style, StyleEx styleEx = NOEX) {
		D2DTextWindow::Create(pos, _title, style, styleEx);
		Rebind(this);
		SendMessage(EM_EXLIMITTEXT, 0, ~0);
		return *this;
	}

	void	SetDisassembly(Disassembler::State *state, bool show_source) {
		mode	= show_source ? orig_mode : 0;
		CodeHelper::SetDisassembly(*this, state, files);
	}
	void	SourceTabs(TabControl2 &tabs) {
		CodeHelper::SourceTabs(tabs, files);
		ShowSourceTabLine(tabs, LineToSource(0));
		tabs.ShowSelectedControl();
	}

	bool	HasFiles()									const { return !files.empty(); }
	void	ShowCode(const Disassembler::Location *loc) const { CodeHelper::ShowCode(*this, loc); }
	void	ShowCode(int file, int line)				const { CodeHelper::ShowCode(*this, file, line); }
	Rect	Margin()									const { return GetClientRect().Subbox(0,0,16,0); }

	void	SetPC(uint32 pc) {
		pc_line = pc;
		SetSelection(GetLineStart(pc));
		EnsureVisible();
		Invalidate(Margin());
	}
};

//-----------------------------------------------------------------------------
//	RegisterWindow
//-----------------------------------------------------------------------------

class RegisterWindow : public InfiniteSplitterWindow {
protected:
	typedef	ListViewControl::Item	Item;
	malloc_block				prev_regs;

public:
	struct Entry {
		enum {
			UNSET		= 1,
			DISABLED	= 2,
			CHANGED		= 4,

			SIZE		= 0x08,	SIZE_MASK = SIZE * 3,
			SIZE32		= SIZE * 0,
			SIZE64		= SIZE * 1,
			SIZE128		= SIZE * 2,

			LINK		= 0x20, LINK_MASK = LINK * 3,
			BUFFER		= LINK * 1,
			TEXTURE		= LINK * 2,
			PTR			= LINK * 3,
		};

		uint32			offset;
		string			name;
		const char		*type;
		const field		*fields;
		uint32			flags;
		Entry(uint32 offset, const char *name, const field *fields = nullptr, uint32 flags = 0) : offset(offset), name(name), type(0), fields(fields), flags(flags) {}

		const char*		Name() const { return name; }
		const char*		Type() const { return type; }

		string_accum&	GetValue(string_accum &a, int col, uint32 *val) const;
	};

	dynamic_array<Entry>	entries;

	static RegisterWindow	*Cast(Control c) {
		return c.FindAncestorByProc<RegisterWindow>();
	}

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	void	Init(ListViewControl lv);
	void*	Reg(const Entry &e) { return (void*)(prev_regs + e.offset); }

	RegisterWindow(const WindowPos &wpos) : InfiniteSplitterWindow(0) {
		Create(wpos, "Registers", CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE);
		Rebind(this);
	}
};

//-----------------------------------------------------------------------------
//	LocalsWindow
//-----------------------------------------------------------------------------

class FrameData {
protected:
	struct block {
		uint64	addr;
		uint32	offset, size:31, remote:1;
		block(uint64 addr, uint32 offset, uint32 size, bool remote) : addr(addr), offset(offset), size(size), remote(remote) {}
		uint32		end()		const	{ return offset + size; }
		operator	uint32()	const	{ return offset; }
	};
	uint32					total;
	dynamic_array<block>	blocks;

	void	_add_block(uint32 offset, uint64 addr, uint32 size, bool remote);

public:
	void	clear()														{ blocks.clear(); total = 0; }
	uint32	add_chunk(uint32 size)										{ return exchange(total, total + size); }
	void	add_block(uint32 offset, uint64 remote, uint32 size)		{ _add_block(offset, remote, size, true); }
	void	add_block(uint32 offset, const void *local, uint32 size)	{ _add_block(offset, (uint64)local, size, false); }

	uint64	remote_address(uint32 offset) const {
		auto	b	= upper_boundc(blocks, offset);
		return b != blocks.begin() && b[-1].remote ? b[-1].addr + offset - b[-1].offset : 0;
	}

	uint32	frame_address(uint64 addr) const {
		for (auto &i : blocks) {
			if (i.remote && between(addr, i.addr, i.addr + i.size - 1))
				return i.offset + addr - i.addr;
		}
		return -1;
	}

	bool	exists(uint32 offset, uint32 size) const {
		auto	b	= upper_boundc(blocks, offset);
		return b != blocks.begin() && b[-1].end() >= offset + size;
	}

	bool	read(void *buffer, size_t size, uint32 offset, memory_interface *mem) const;

	FrameData() : total(0) {}
};

struct FrameMemoryInterface0 : FrameData, memory_interface {
	virtual bool _get(void *buffer, size_t size, uint64 addr)	{ return FrameData::read(buffer, size, addr, 0); }
};

struct FrameMemoryInterface : FrameData, memory_interface {
	memory_interface	*mem;
	uint64				base;

	uint64		remote_address(uint64 addr)				const	{ return between(addr, base, base + total) ? FrameData::remote_address(addr - base) : addr; }
	uint64		frame_address(uint64 addr)				const	{ uint32 o = FrameData::frame_address(addr); return ~o ? o + base : addr; }
	virtual bool _get(void *buffer, size_t size, uint64 addr)	{ return between(addr, base, base + total) ? FrameData::read(buffer, size, addr - base, mem) : mem->get(buffer, size, addr); }

	FrameMemoryInterface(memory_interface *mem, uint64 base = bit64(63)) : mem(mem), base(base) {}
};

//-----------------------------------------------------------------------------
//	LocalsWindow
//-----------------------------------------------------------------------------

string Description(ast::node *node);

class LocalsWindow : public Window<LocalsWindow> {
protected:
	TreeColumnControl	tc;
	memory_interface*	mem;
	const C_types&		types;
	hash_set<uint64>	bps;
	ast::get_variable_t	get_var;
	NATVIS*				natvis;

	void		AppendEntry(string_param &&id, const C_type *type, uint64 addr, bool local = false);
	ast::node*	GetEntry(HTREEITEM h)						const	{ return tc.GetItemParam(h); }
	uint64		GetAddress(ast::node *node, uint64 *size)	const;

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	LocalsWindow(const WindowPos &wpos, const char *title, const C_types &types, memory_interface *mem = 0) : mem(mem), types(types), natvis(0) {
		Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE);
	}
};

//-----------------------------------------------------------------------------
//	WatchWindow
//-----------------------------------------------------------------------------

class WatchWindow : public Window<WatchWindow>, public DropTarget<WatchWindow> {
protected:
	TreeColumnControl	tc;
	memory_interface*	mem;
	hash_set<uint64>	bps;
	C_types&			types;
	ast::get_variable_t	get_var;
	NATVIS*				natvis;

	EditControl2		edit_control;
	HTREEITEM			edit_hitem;

	void		RemoveEntry(HTREEITEM h, bool backwards);
	void		AppendEntry(string_param &&s, ast::node *node);
	ast::node*	GetEntry(HTREEITEM h)						const	{ return tc.GetItemParam(h); }
	uint64		GetAddress(ast::node *node, uint64 *size)	const;

public:
	bool	Drop(const Point &pt, uint32 effect, IDataObject* data);
	void	DragEnter(const Point &pt)		{ ImageList::DragEnter(*this, pt - GetRect().TopLeft()); }
	void	DragOver(const Point &pt)		{ ImageList::DragMove(pt - GetRect().TopLeft()); }
	void	DragExit()						{ ImageList::DragLeave(*this); }

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void	Redraw();

	WatchWindow(const WindowPos &wpos, const char *title, C_types &types, ast::get_variable_t get_var, memory_interface *mem) : mem(mem), types(types), get_var(get_var), natvis(0) {
		Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE);
		RegisterDragDrop(*this, this);
	}
};

//-----------------------------------------------------------------------------
//	TraceWindow
//-----------------------------------------------------------------------------

class TraceWindow : public Window<TraceWindow> {
protected:
	ListViewControl	c;
	struct row_data {
		struct field_data {
			uint16	reg; uint16 mask:4, write:1;
		} fields[8];
		row_data() { clear(*this); }

	};
	dynamic_array<row_data>	rows;
	int						selected;
	int						cols_per_entry;
	/*
	struct RegAdder : buffer_accum<256>, ListViewControl::Item {
		ListViewControl			c;
		row_data				&row;
		uint8					types[2];
		bool					source;

		void	SetAddress(uint64 addr) {
			format("%010I64x", addr);
			*getp() = 0;
			Insert(c);
		}
		void	SetDis(const char *dis) {
			reset() << dis;
			*getp() = 0;
			Column(1).Set(c);
		}

		void	AddValue(int i, int reg, int mask, float *f) {
			row.fields[i].reg	= reg;
			row.fields[i].write	= !source;
			row.fields[i].mask	= mask;

			for (int j = 0; j < 4; j++) {
				reset() << f[j] << '\0';
				Column(i * 4 + j + 2).Set(c);
			}
		}

		RegAdder(ListViewControl _c, row_data &_row) : ListViewControl::Item(getp()), c(_c), row(_row) {}
	};
	int InsertRegColumns(int nc, const char *name) {
		for (int i = 0; i < 4; i++)
			ListViewControl::Column(buffer_accum<64>(name) << '.' << "xyzw"[i]).Width(120).Insert(c, nc++);
		return nc;
	}
	*/

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	TraceWindow(const WindowPos &wpos, int cols_per_entry);
};

} // namespace app

#endif // DEBUG_H
