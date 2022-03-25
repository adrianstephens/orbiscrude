#ifndef SPLITTER_H
#define SPLITTER_H

#include "window.h"
#include "base/array.h"

namespace iso { namespace win {

//-----------------------------------------------------------------------------
//	Arrange
//-----------------------------------------------------------------------------

void MoveWindows(Control *controls, const Rect *rects, int n);

class ControlArrangement {
	enum {
		SPLIT		= 1 << 15,
		VERT		= 1 << 14,
		PROP		= 1 << 13,
		SKIP		= 1 << 14,
		VALUE_SCALE	= 0x1000,
		VALUE_MASK	= 0x1fff,
	};
	static int	from_float(float x)		{ return clamp(x * VALUE_SCALE, -VALUE_SCALE, VALUE_SCALE - 1); }
public:
	struct Token {
		uint16	value;
		Token(uint16 _value) : value(_value)	{}
		void	set(int x)		{ value = (value & ~VALUE_MASK) | (x & VALUE_MASK); }
		void	set(float x)	{ set(from_float(x)); }
	};
	struct ControlRect : Token {
		ControlRect(int i) : Token(i)	{}
	};
	struct End : Token {
		End() : Token(VALUE_MASK)	{}
	};
	struct VSplit : Token {
		VSplit(int y)	: Token((y & VALUE_MASK) | SPLIT | VERT)	{}
	};
	struct PropVSplit : Token {
		PropVSplit(float y)	: Token(from_float(y) | SPLIT | PROP | VERT)	{}
	};
	struct HSplit : Token {
		HSplit(int x)	: Token((x & VALUE_MASK) | SPLIT)	{}
	};
	struct PropHSplit : Token {
		PropHSplit(float x)	: Token(from_float(x) | SPLIT | PROP)	{}
	};
	struct Skip : Token {
		Skip(int x)	: Token((x & VALUE_MASK) | SKIP)	{}
	};
private:
	Token		*arrange;
	Control		*controls;
	int			num_controls;
public:
	iso_export static void	GetRects(const Token *p, Rect rect, Rect *rects);
	static Rect	GetRect(const Token *p, Rect rect, int i)	{ Rect rects[64]; GetRects(p, rect, rects); return rects[i]; }

	void	Arrange(const Rect &rect)				const	{ Rect rects[64]; GetRects(arrange, rect, rects); MoveWindows(controls, rects, num_controls); }
	Rect	GetRect(const Rect &rect, int i)		const	{ Rect rects[64]; GetRects(arrange, rect, rects); return rects[i]; }
	ControlArrangement(Token *_arrange, Control *_controls, int _num_controls) : arrange(_arrange), controls(_controls), num_controls(_num_controls) {}
};

class ArrangeWindow : public Window<ArrangeWindow>, public ControlArrangement {
public:
	LRESULT		Proc(UINT message, WPARAM wParam, LPARAM lParam);
	void		Arrange()	const { ControlArrangement::Arrange(GetClientRect()); }
	ArrangeWindow(ControlArrangement::Token *_arrange, Control *_controls, int _num_controls) : ControlArrangement(_arrange, _controls, _num_controls) {}
};

// predefined arrangements
extern ControlArrangement::Token ToolbarArrange[];

//-----------------------------------------------------------------------------
//	Split
//-----------------------------------------------------------------------------

struct SplitAdjuster {
	enum {
		SWF_HORZ				= 0,
		SWF_VERT				= 1 << 0,
		SWF_PROP				= 1 << 1,	// proportional
		SWF_STATIC				= 1 << 2,	// not moveable
		SWF_FROM2ND				= 1 << 3,
		SWF_DELETE_ON_DESTROY	= 1 << 4,
		SWF_FLIP				= 1 << 8,	// for infinite splitter
		SWF_ALWAYSSPLIT			= 1 << 9,
		SWF_NO_AUTO_DESTROY		= 1 << 10,
		SWF_DOCK				= 1 << 11,	// can be docked into
		SWF_PASSUP				= 1 << 14,
		SWF_DRAG				= 1 << 15,
	};
	int		flags, gripper, range;

	SplitAdjuster() : flags(0) {}
	SplitAdjuster(int _flags) { Init(_flags); }

	iso_export void Init(int _flags);

	int ToProportional(int pos)	const {
		return flags & SWF_PROP		? pos
			: (flags & SWF_FROM2ND	? range - pos : pos) * MAXWORD / range;
	}
	int ToAbsolute(int pos)		const {
		return	flags & SWF_PROP	? range * pos / MAXWORD
			:	flags & SWF_FROM2ND	? range - pos : pos;
	}

	int FromProportional(int pos) {
		if (pos < 0)
			pos += MAXWORD;
		return	flags & SWF_PROP	? pos
			: (flags & SWF_FROM2ND	? MAXWORD - pos : pos) * range / MAXWORD;
	}

	int FromAbsolute(int pos) {
		if (pos < 0)
			pos += range;
		return	flags & SWF_PROP	? (pos ? pos * MAXWORD / range : 0)
			:	flags & SWF_FROM2ND	? range - pos : pos;
	}

	int Fix(int pos) {
		return  flags & SWF_PROP
			? (pos < 0 ? 100 + pos : pos) * MAXWORD / 100
			: pos < 0 ? range + pos : pos;
	}
};

//-----------------------------------------------------------------------------
//	SplitterWindow
//-----------------------------------------------------------------------------

class SplitterWindow : public Window<SplitterWindow>, public SplitAdjuster {
	Control		pane[2];
	int			pos, offset;
	Rect		rcGripper;

protected:
	recursion_checker<MessageRecord>	recursion;

	void		UpdateSplitter(bool always);
	void		_SetPane(int i, Control c);
	void		_SetPanes(Control pane0, Control pane1) {
		_SetPane(0, pane0);
		_SetPane(1, pane1);
	}

public:
	iso_export LRESULT	Proc(UINT message, WPARAM wParam, LPARAM lParam);

	iso_export void		Init(int _flags, int pos = 0);
	Control				GetPane(int i)			const		{ return pane[i];	}
	void				SetPane(int i, Control c)			{ _SetPane(i, c); UpdateSplitter(true); }

	int					GetGripper()			const		{ return gripper;	}
	void				SetGripper(int g)					{ gripper = g; UpdateSplitter(true); }

	int					GetFlags()				const		{ return flags;		}
	int					GetPos()				const		{ return pos;		}
	bool				Dragging()				const		{ return !!(flags & SWF_DRAG);	}
	int					WhichPane(Control c)	const		{ return c == pane[0] ? 0 : c == pane[1] ? 1 : -1; }

	int					GetClientPos()			const		{ return ToAbsolute(pos); }
	int					GetProportionalPos()	const		{ return ToProportional(pos); }
	void				SetClientPos(int _pos)				{ pos = FromAbsolute(_pos); }
	void				SetClientPos(int _pos, int _flags)	{ flags = _flags; SetClientPos(_pos); }
	void				SetProportionalPos(int _pos)		{ pos = FromProportional(_pos); }
	void				SetPos(int _pos)					{ pos = Fix(_pos); }

	iso_export Rect		_GetPaneRect(int i)		const;
	iso_export Rect		GetPaneRect(int i)		const;
	iso_export void		SetPaneAndSize(int i, Control c);

	WindowPos			_GetPanePos(int i)		const		{ return WindowPos(*this, _GetPaneRect(i)); }
	WindowPos			GetPanePos(int i)		const		{ return WindowPos(*this, GetPaneRect(i)); }

	void		SetPanes(Control pane0, Control pane1)							{ _SetPanes(pane0, pane1); UpdateSplitter(true); }
	void		SetPanes(Control pane0, Control pane1, int _pos)				{ _SetPanes(pane0, pane1); SetPos(_pos); UpdateSplitter(true); }
	void		SetPanesClient(Control pane0, Control pane1, int _pos)			{ _SetPanes(pane0, pane1); SetClientPos(_pos); UpdateSplitter(true); }
	void		SetPanesProportional(Control pane0, Control pane1, int _pos)	{ _SetPanes(pane0, pane1); SetProportionalPos(_pos); UpdateSplitter(true); }
	void		SetProportionalPos(int _pos, int _flags)						{ flags = _flags; SetProportionalPos(_pos); }

	SplitterWindow(int flags = 0, int pos = 0) {
		Init(flags, pos);
	}
	SplitterWindow(const WindowPos &wpos, const char *caption, int flags, Style style = CHILD | VISIBLE, StyleEx exstyle = NOEX, ID id = ID()) {
		Init(flags);
		Create(wpos, caption, style, exstyle, id);
	}
};

//-----------------------------------------------------------------------------
//	MultiSplitterWindow
//-----------------------------------------------------------------------------

class MultiSplitterWindow : public Window<MultiSplitterWindow>, public SplitAdjuster {
	struct Pane {
		Control		c;
		int			edge;
	};
	dynamic_array<Pane>	panes;
	int			drag, offset;

	void		UpdateSplitters();
public:
	iso_export LRESULT	Proc(UINT message, WPARAM wParam, LPARAM lParam);

	iso_export void		Init(int n, int flags);
	int					NumPanes()				const	{ return panes.size32(); }
	Control				GetPane(int i)			const	{ return panes[i].c; }
	iso_export int		WhichPane(Control c)	const;
	iso_export Rect		GetPaneRect(int i)		const;
	WindowPos			GetPanePos(int i)		const	{ return WindowPos(*this, GetPaneRect(i)); }
	iso_export void		SetPane(int i, Control c);
	bool				Dragging()				const	{ return drag >= 0;	}
	void				SetPaneSize(int i, int w)		{ panes[i].edge = (i ? panes[i - 1].edge : 0) + w; }

	void				RemovePane(int i);
	WindowPos			InsertPane(int i, int w);
	WindowPos			AppendPane(int w)				{ return InsertPane(panes.size32(), w); }
	void				InsertPane(Control c, int i)	{ InsertPane(i, FromAbsolute(flags & SWF_VERT ? c.GetRect().Width() : c.GetRect().Height())); SetPane(i, c); }
	void				AppendPane(Control c)			{ InsertPane(c, panes.size32()); }

	MultiSplitterWindow(int n, int flags) {
		Init(n, flags);
	}
	MultiSplitterWindow(const WindowPos &wpos, const char *caption, int n, int flags, Style style = CHILD | VISIBLE, StyleEx exstyle = NOEX, ID id = ID()) {
		Init(n, flags);
		Create(wpos, caption, style, exstyle, id);
	}
};

//-----------------------------------------------------------------------------
//	InfiniteSplitterWindow
//-----------------------------------------------------------------------------

class InfiniteSplitterWindow : public SplitterWindow {
protected:
	iso_export void		UpdateSplitter(bool root);
	iso_export void		Split(int a);
	iso_export void		Unsplit(int a, bool root);
public:
	enum {
		WM_ISO_NEWPANE	= WM_USER + 0x100,
	};

	iso_export LRESULT	Super(UINT message, WPARAM wParam, LPARAM lParam);
	iso_export LRESULT	Proc(UINT message, WPARAM wParam, LPARAM lParam);
	iso_export InfiniteSplitterWindow(int _flags) : SplitterWindow(_flags | SWF_ALWAYSSPLIT) {}
};

} }	// namespace iso::win

#endif	//SPLITTER_H