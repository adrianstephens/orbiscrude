#ifndef TEXT_CONTROL_H
#define TEXT_CONTROL_H

#include "window.h"

namespace iso { namespace win {

class D2DEditControl : public create_mixin<D2DEditControl, RichEditControl> {
public:
	static const char *ClassName()	{ return "D2DEditControl"; }
	using Base::Base;
};

Control CreateD2DEditControl(const WindowPos &pos);

class D2DTextWindow : public Subclass<D2DTextWindow, D2DEditControl> {
	string	title;
public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	D2DTextWindow();
	D2DTextWindow(const WindowPos &pos, const char *_title, Style style = NOSTYLE, StyleEx styleEx = NOEX, ID id = ID());
	HWND	Create(const WindowPos &pos, const char *_title, Style style = NOSTYLE, StyleEx styleEx = NOEX, ID id = ID());
	void	Online(bool enable);
	void	SetTitle(const char *_title) { title = _title; }
};

//-----------------------------------------------------------------------------
//	TextWindow
//-----------------------------------------------------------------------------

class TextWindow : public Subclass<TextWindow, RichEditControl> {
	enum {
		WORDWRAP = 1 << 0,
	};
	string	title;
	string	search;
	int		flags;
public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	TextWindow();
	TextWindow(const WindowPos &pos, const char *_title, Style style = NOSTYLE, StyleEx styleEx = NOEX, ID id = ID());
	HWND	Create(const WindowPos &pos, const char *_title, Style style = NOSTYLE, StyleEx styleEx = NOEX, ID id = ID());
	void	Online(bool enable);
	void	SetTitle(const char *_title) { title = _title; }
};

//-----------------------------------------------------------------------------
//	functions
//-----------------------------------------------------------------------------

void AddTextANSI(RichEditControl control, const char *p);
void AddTextANSI(RichEditControl control, const char16 *p);

} } // namespace iso::win

#endif // TEXT_CONTROL_H
