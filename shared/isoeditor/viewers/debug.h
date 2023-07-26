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

void			SetSourceWindow(RichEditControl &text, const Disassembler::File *file, const SyntaxColourer &colourer, range<int*> active);
EditControl		MakeSourceWindow(const WindowPos &wpos, const Disassembler::File *file, const SyntaxColourer &colourer, range<int*> active, Control::Style style, Control::StyleEx styleEx = Control::NOEX, ID id = {});

void			ShowSourceLine(EditControl edit, uint32 line);
void			ShowSourceTabLine(TabControl2 tabs, uint32 file, uint32 line);
void			ShowSourceTabLine(TabControl2 tabs, const Disassembler::Location *loc);

void			DumpDisassemble(win::RichEditControl &text, Disassembler::State *state, int flags, Disassembler::SymbolFinder sym_finder);

void			DrawBreakpoints(RichEditControl edit, void *target, int pc_line, const dynamic_array<uint32> &bp_lines, bool line_nos = false);

//-----------------------------------------------------------------------------
//	CodeHelper
//-----------------------------------------------------------------------------

class CodeHelper {
protected:
	const SyntaxColourerRE&			colourer;
	Disassembler::AsyncSymbolFinder	sym_finder;
	Disassembler::MODE				mode;
	dynamic_array<int>				loc_indices;
	dynamic_array<int>				loc_indices_combine;

	auto	GetLocations() const {
		return make_indexed_container(locations, make_const(loc_indices_combine));
	}
public:
	uint64							base;
	unique_ptr<Disassembler::State>	state;
	Disassembler::Locations			locations;

	CodeHelper(const SyntaxColourerRE &colourer, Disassembler::AsyncSymbolFinder &&sym_finder, Disassembler::MODE mode)
		: colourer(colourer), sym_finder(move(sym_finder)), mode(mode), base(0) {}

	void	FixLocations(uint64 base);
	void	UpdateDisassembly(RichEditControl c);
	void	UpdateDisassembly(RichEditControl c, const Disassembler::Files &files);

	void	SetDisassembly(RichEditControl c, Disassembler::State* _state) {
		state		= _state;
		UpdateDisassembly(c);

	}
	void	SetDisassembly(RichEditControl c, Disassembler::State* _state, const Disassembler::Files& files) {
		state		= _state;
		UpdateDisassembly(c, files);
	}

	void	RemapFromHashLine(Disassembler::Files &files, const char *search_path, Disassembler::SharedFiles &shared_files);
	uint64	OffsetToAddress(uint32 offset)	const { return offset ? offset + base : 0; }

	void	ShowCode(EditControl c, const Disassembler::Location *loc) const {
		if (loc) {
			c.SetSelection(c.GetLineRange(OffsetToLine(loc->offset)));
			c.EnsureVisible();
		}
	}
	void	ShowCode(EditControl c, int file, int line)	const {
		ShowCode(c, locations.find(file, line));
	}
	void	GetDisassembly(string_accum &sa, int line)	const {
		if (mode & Disassembler::SHOW_SOURCE)
			state->MixedLineToSource(GetLocations(), line, base);
		state->GetLine(sa, line, mode, sym_finder);
	}

	auto	LineToSource(int line)						const {
		return (mode & Disassembler::SHOW_SOURCE) && locations
			? &*state->MixedLineToSource(GetLocations(), line, base)
			: locations.find(state->LineToAddress(line) - base);
	}

	uint64	LineToAddress(int line)						const {
		if (mode & Disassembler::SHOW_SOURCE)
			state->MixedLineToSource(GetLocations(), line, base);
		return state->LineToAddress(line);
	}
	uint32	LineToOffset(int line)						const {
		return LineToAddress(line) - base;
	}

	uint32	AddressToLine(uint64 addr)					const {
		return mode & Disassembler::SHOW_SOURCE
			? state->AddressToMixedLine(GetLocations(), addr, base)
			: state->AddressToLine(addr);
	}
	int		OffsetToLine(uint32 offset)					const {
		return AddressToLine(base + offset);
	}

	uint64	LegalAddress(uint64 addr)					const {
		return state->GetAddress(state->AddressToLine(addr));
	}
	uint64	NextAddress(uint64 addr)					const {
		return state->GetAddress(state->AddressToLine(addr) + 1);
	}
	uint32	NextOffset(uint32 addr)						const {
		return NextAddress(addr + base) - base;
	}

	const Disassembler::Location *OffsetToSource(uint32 offset) const {
		auto	locs	= GetLocations();
		auto	loc		= lower_boundc(locs, offset);
		return loc != locs.end() ? &*loc : nullptr;
	}
	const Disassembler::Location *AddressToSource(uint64 addr) const {
		return OffsetToSource(addr - base);
	}
	uint32	NextSourceOffset(uint32 offset, uint32 funcid = 0)	const;
	uint64	NextSourceAddress(uint64 addr, uint32 funcid = 0)	const {
		return OffsetToAddress(NextSourceOffset(addr - base, funcid));
	}

	void	Select(EditControl click, EditControl dis, TabControl2 *tabs);
	void	SourceTabs(TabControl2 &tabs, const Disassembler::Files &files);
};

//-----------------------------------------------------------------------------
//	CodeWindow
//-----------------------------------------------------------------------------

class CodeWindow : public D2DTextWindow {
public:
	enum {
		ID_DEBUG_BREAKPOINT	= 1000,
		ID_DEBUG_SWITCHSOURCE,
		ID_DEBUG_SHOWSOURCE,
		ID_DEBUG_COMBINESOURCE,
		ID_DEBUG_SHOWLINENOS,
		ID_DEBUG_STEPOVER,
		ID_DEBUG_STEPBACK,
		ID_DEBUG_RUN,
		ID_DEBUG_PIXEL,
		ID_DEBUG_STEPIN,
		ID_DEBUG_STEPOUT,
		ID_DEBUG_RUNTO,
	};
	static Accelerator GetAccelerator();

	uint32	margin = 24;
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	HWND	Create(const WindowPos &pos, const char *_title, Style style, StyleEx styleEx = NOEX, ID id = ID()) {
		D2DTextWindow::Create(pos, _title, style, styleEx, id);
		Rebind(this);
		SendMessage(EM_EXLIMITTEXT, 0, ~0);
		return *this;
	}

	Rect	Margin()			const { return GetClientRect().Subbox(0,0,margin,0); }
	void	InvalidateMargin()	const { Invalidate(Margin()); }
	CodeWindow()	{}
	CodeWindow(const WindowPos& pos, const char* _title, Style style = NOSTYLE, StyleEx styleEx = NOEX, ID id = ID()) {
		Create(pos, _title, style, styleEx, id);
	}
};


//-----------------------------------------------------------------------------
//	DebugWindow
//-----------------------------------------------------------------------------

class DebugWindow : public CodeWindow, public CodeHelper {
protected:
	Disassembler::MODE	orig_mode;

public:
	Disassembler::Files		files;

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	DebugWindow(const SyntaxColourerRE &colourer, Disassembler::AsyncSymbolFinder &&sym_finder, Disassembler::MODE mode);

	void	SetMode(Disassembler::MODE mode);

	HWND	Create(const WindowPos &pos, const char *_title, Style style, StyleEx styleEx = NOEX) {
		D2DTextWindow::Create(pos, _title, style, styleEx);
		Rebind(this);
		SendMessage(EM_EXLIMITTEXT, 0, ~0);
		return *this;
	}

	void	SetDisassembly(Disassembler::State *state, bool show_source) {
		mode	= show_source ? orig_mode : Disassembler::MODE(0);
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

	void	SetPC(uint32 pc) {
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
		Entry(uint32 offset, text name, const field *fields = nullptr, uint32 flags = 0) : offset(offset), name(name), type(0), fields(fields), flags(flags) {}

		const char*		Name() const { return name; }
		const char*		Type() const { return type; }

		string_accum&	GetValue(string_accum &a, int col, uint32 *val) const;
	};

	dynamic_array<Entry>	entries;

	static RegisterWindow	*Cast(Control c) {
		return c.FindAncestorByProc<RegisterWindow>();
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
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
		operator	uint32()	const	{ return end() - 1; }
	};
	uint32					total;
	dynamic_array<block>	blocks;

	void	_add_block(uint32 offset, uint64 addr, uint32 size, bool remote, bool replace);

public:
	void	clear()																	{ blocks.clear(); total = 0; }
	uint32	add_chunk(uint32 size)													{ return exchange(total, total + size); }
	void	add_block(uint32 offset, uint64 remote, uint32 size, bool replace = true)		{ _add_block(offset, remote, size, true, replace); }
	void	add_block(uint32 offset, const void *local, uint32 size, bool replace = true)	{ _add_block(offset, (uint64)local, size, false, replace); }

	uint64	remote_address(uint32 offset) const {
		auto	b	= lower_boundc(blocks, offset);
		return b != blocks.end() && b->remote ? b->addr + offset - b->offset : 0;
	}

	uint32	frame_address(uint64 addr) const {
		for (auto &i : blocks) {
			if (i.remote && between(addr, i.addr, i.addr + i.size - 1))
				return i.offset + addr - i.addr;
		}
		return -1;
	}

	bool	exists(uint32 offset, uint32 size) const {
		auto	b	= lower_boundc(blocks, offset);
		return b != blocks.end() && b->end() >= offset + size;
	}

	bool	read(void *buffer, size_t size, uint32 offset, memory_interface *mem) const;
	uint32	next(uint32 offset, size_t &size, bool dir) const;

	FrameData() : total(0) {}
};

struct FrameMemoryInterface0 : FrameData, memory_interface {
	bool _get(void *buffer, uint64 size, uint64 addr)			override{
		return FrameData::read(buffer, size, addr, 0);
	}
	virtual uint64	_next(uint64 addr, uint64 &size, bool dir)	override {
		return FrameData::next(addr, size, dir);
	};

};

struct FrameMemoryInterface : FrameData, memory_interface {
	memory_interface	*mem;
	uint64				base;

	uint64		remote_address(uint64 addr)				const	{ return between(addr, base, base + total) ? FrameData::remote_address(addr - base) : addr; }
	uint64		frame_address(uint64 addr)				const	{ uint32 o = FrameData::frame_address(addr); return ~o ? o + base : addr; }
	virtual bool _get(void *buffer, size_t size, uint64 addr)	{ return between(addr, base, base + total) ? FrameData::read(buffer, size, addr - base, mem) : mem->get(buffer, size, addr); }

	FrameMemoryInterface(memory_interface *mem, uint64 base = bit64(63)) : mem(mem), base(base) {}
};

string Description(ast::node *node);

class LocalsWindow : public Window<LocalsWindow> {
protected:
	TreeColumnControl	tc;
	memory_interface*	mem;
	const C_types&		types;
	hash_set<uint64>	bps;
	ast::get_variable_t	get_var;
	NATVIS*				natvis;

	uint64			GetAddress(ast::node *node, uint64 *size)	const;

public:
	LRESULT			Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	HTREEITEM		AppendEntry(string_param &&id, const C_type *type, uint64 addr, bool local = false);
	ast::lit_node*	GetEntry(HTREEITEM h)						const	{ return tc.GetItemParam(h); }
	void			Update()									const	{ tc.Invalidate(); }

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
	using DropTarget<WatchWindow>::Drop;
	using DropTarget<WatchWindow>::DragEnter;
	using DropTarget<WatchWindow>::DragOver;
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

	struct RegAdder : buffer_accum<256>, ListViewControl::Item {
		ListViewControl			c;
		row_data				&row;
		bool					source;

		void	SetColumn(int col) {
			term();
			Column(col).Set(c);
		}

		RegAdder(ListViewControl c, row_data &row) : ListViewControl::Item(getp()), c(c), row(row) {}
	};
	
public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	TraceWindow(const WindowPos &wpos, int cols_per_entry);
};

} // namespace app

#endif // DEBUG_H
