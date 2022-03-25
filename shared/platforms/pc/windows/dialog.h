#include "window.h"
#include "base\array.h"

namespace iso { namespace win {

//-----------------------------------------------------------------------------
//	Dialogs
//-----------------------------------------------------------------------------

struct NameOrOrdinal {
	uint16	v[1];
	bool	IsOrdinal()	const	{ return v[0] == 0xffff; }
	operator bool()		const	{ return v[0] == 0; }
	operator ID()		const	{ return IsOrdinal() ? ID(v[1]) : ID((const char*)this); }
	void	*after()	const	{ return IsOrdinal() ? (void*)(v + 2) : (void*)(str((char16*)this).end() + 1); }

	void	operator=(ID id)	{
		if (!id) {
			v[0] = 0;
		} else if (id.IsOrdinal()) {
			v[0] = 0xffff;
			v[1] = id;
		} else {
			string_copy((char16*)this, str16(id.s));
		}
	}
	void	operator=(const _none&)	{
		v[0] = 0;
	}
};

inline void *get_after(const NameOrOrdinal *t) {
	return t->after();
}

struct Rect16 {
	int16		x, y, w, h;
	void operator=(const Rect &r) { x = r.Left(); y = r.Top(); w = r.Width(); h = r.Height(); }
};

struct DialogUnits {
	Point	units;

	static Point ForFont(Font f) {
		DeviceContext	dc = DeviceContext::Screen();
		dc.Select(f);
		Point	size	= dc.GetTextExtent("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 26 * 2);
		return Point((size.x / 26 + 1) / 2, dc.GetTextMetrics().AverageSize().y);
	}

	static Point DialogToScreen(const Point &pt, const Point &units) {
		return Point(
			MulDiv(pt.x, units.x, 4),
			MulDiv(pt.y, units.y, 8)
		);
	}
	static Point ScreenToDialog(const Point &pt, const Point &units) {
		return Point(
			MulDiv(pt.x, 4, units.x),
			MulDiv(pt.y, 8, units.y)
		);
	}
	static Rect DialogToScreen(const Rect &r, const Point &units) {
		return Rect(DialogToScreen(r.TopLeft(), units), DialogToScreen(r.BottomRight(), units));
	}
	static Rect ScreenToDialog(const Rect &r, const Point &units) {
		return Rect(ScreenToDialog(r.TopLeft(), units), ScreenToDialog(r.BottomRight(), units));
	}

	DialogUnits()			: units(GetDialogBaseUnits()) {}
	DialogUnits(Control c)	: units(ForFont(c.GetFont())) {}
	DialogUnits(Font f)		: units(ForFont(f)) {}
	void	SetFont(Font f) { units = ForFont(f); }

	Point	ScreenToDialog(const Point &p)	const	{ return ScreenToDialog(p, units); }
	Point	DialogToScreen(const Point &p)	const	{ return DialogToScreen(p, units); }
	Rect	ScreenToDialog(const Rect &r)	const	{ return ScreenToDialog(r, units); }
	Rect	DialogToScreen(const Rect &r)	const	{ return DialogToScreen(r, units); }
};

enum DialogItemType {
	DLG_BUTTON		= 0x80,	//CHILD|VISIBLE|TABSTOP
	DLG_EDIT		= 0x81,	//CHILD|VISIBLE|BORDER|TABSTOP | ES_AUTOHSCROLL
	DLG_STATIC		= 0x82,	//CHILD|VISIBLE|GROUP
	DLG_LISTBOX		= 0x83,
	DLG_SCROLLBAR	= 0x84,
	DLG_COMBOBOX	= 0x85,
};

struct DialogBox {
	uint32		style, ex_style;
	uint16		num_items;
	Rect16		rect;

	struct Font {
		int16				pointsize;
		embedded_string16	name;
	};

	struct Control {
		uint32		style, ex_style;
		Rect16		rect;
		uint32		id;
		union {
			NameOrOrdinal										class_id;
			after<NameOrOrdinal,NameOrOrdinal>					title;
			after<uint16, after<NameOrOrdinal,NameOrOrdinal> >	extra_bytes;
		};
		Control	*next()	const { uint16 &x = extra_bytes; return (Control*)align((char*)(&x + int(!x)) + x, 4); }
	};

	union {
		NameOrOrdinal																menu_id;
		after<NameOrOrdinal,NameOrOrdinal>											class_id;
		after<embedded_string16,after<NameOrOrdinal,NameOrOrdinal> >				caption;
		after<Font,after<embedded_string16,after<NameOrOrdinal,NameOrOrdinal> > >	font;		// Only here if FONT set for dialog
		after<Control[1],after<Font,after<embedded_string16,after<NameOrOrdinal,NameOrOrdinal> > > >	controls;
	};

	bool	is_extended()		const { return (style >> 16) == 0xffff; }
	bool	has_font()			const { return style & DS_SETFONT; }
};

struct DialogBoxEx {
	uint16		ver;
	uint16		signature;
	uint32		help_id, ex_style, style;
	uint16		num_items;
	Rect16		rect;

	struct Font {
		int16				pointsize;
		int16				weight;
		int16				italic;
		embedded_string16	name;
	};

	struct Control {
		uint32		help_id, ex_style, style;
		Rect16		rect;
		uint32		id;
		union {
			NameOrOrdinal										class_id;
			after<NameOrOrdinal,NameOrOrdinal>					title;
			after<uint16, after<NameOrOrdinal,NameOrOrdinal> >	extra_bytes;
		};
		Control	*next()	const { uint16 &x = extra_bytes; return (Control*)align((char*)(&x + 1) + x, 4); }
	};

	union {
		NameOrOrdinal																menu_id;
		after<NameOrOrdinal,NameOrOrdinal>											class_id;
		after<embedded_string16,after<NameOrOrdinal,NameOrOrdinal> >				caption;
		after<Font,after<embedded_string16,after<NameOrOrdinal,NameOrOrdinal> > >	font;		// Only here if FONT set for dialog
		after<Control[1],after<Font,after<embedded_string16,after<NameOrOrdinal,NameOrOrdinal> > > >	controls;
	};

	bool	has_font()			const { return style & DS_SETFONT; }
};

inline void *get_after(const DialogBox::Font *t)	{ return (void*)(t->name.end() + 1); }
inline void *get_after(const DialogBoxEx::Font *t)	{ return (void*)(t->name.end() + 1); }

struct DialogBoxCreator {
	struct Control {
		DialogItemType	type;
		string			title;
		uint32			id, style, ex_style, help_id;
		Control(DialogItemType type, const char *title, uint32 id = 0, win::Control::Style style = win::Control::OVERLAPPED, win::Control::StyleEx ex_style = win::Control::NOEX, uint32 _help_id = 0)
			: type(type), title(title), id(id), style(style|win::Control::CHILD|win::Control::VISIBLE), ex_style(ex_style), help_id(_help_id)
		{}
	};

	struct Control2 : Control {
		Rect			rect;
		Control2(const Rect &_rect, const Control &c)
			: Control(c), rect(_rect)
		{}
		size_t	CalcSize() const;
		void	Make(DialogBoxEx::Control *c) const;
	};

	uint32			style, ex_style, help_id;
	Rect			rect;
	DialogUnits		units;
	string			title;
	Font::Params	font;
	dynamic_array<Control2>	controls;

	DialogBoxCreator(const Rect &_rect, const char *_title, uint32 _style, uint32 _ex_style = 0, uint32 _help_id = 0)
		: rect(_rect), title(_title), style(_style), ex_style(_ex_style), help_id(_help_id)
	{}

	void		SetFont(const Font::Params &_font) {
		font	= _font;
		style	|= DS_SETFONT;
		units.SetFont(Font(font));
	}

	Control2	*AddControl(const Rect &rect, DialogItemType type, const char *title, uint32 id = 0, win::Control::Style style = win::Control::OVERLAPPED, win::Control::StyleEx ex_style = win::Control::NOEX, uint32 help_id = 0) {
		return &controls.emplace_back(rect, Control(type, title, id, style, ex_style, help_id));
	}
	Control2	*AddControl(const Rect &rect, const Control &c) {
		return &controls.emplace_back(rect, c);
	}
	size_t	CalcSize() const;
	void*	Make(DialogBoxEx *db) const;

	Point	DialogToScreen(const Point &pt) const { return units.DialogToScreen(pt); }
	Point	ScreenToDialog(const Point &pt) const { return units.ScreenToDialog(pt); }
	Rect	DialogToScreen(const Rect &r)	const { return units.DialogToScreen(r); }
	Rect	ScreenToDialog(const Rect &r)	const { return units.ScreenToDialog(r); }

	unique_ptr<DLGTEMPLATE> GetTemplate() const {
		size_t		size	= CalcSize();
		DialogBoxEx	*db		= new(malloc(size)) DialogBoxEx;
		void		*end	= Make(db);
		ISO_ASSERT((char*)db + size == end);
		return unique_ptr<DLGTEMPLATE>((DLGTEMPLATE*)db);
	}
};

} } // namespace iso::win