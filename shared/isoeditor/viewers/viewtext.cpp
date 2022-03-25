#include "main.h"
#include "thread.h"
#include "iso/iso_script.h"
#include "extra/xml.h"
#include "systems/communication/connection.h"
#include "windows/text_control.h"
#include "windows/filedialogs.h"

#if 0
#define TextWindow1	TextWindow
#else
#define TextWindow1	D2DTextWindow
#endif

using namespace app;

void HTMLformat(RichEditControl &re, istream_ref in) {
	XMLreader::Data	data;
	XMLreader				xml(in);

	xml.SetFlag(XMLreader::UNQUOTEDATTRIBS);
	xml.SetFlag(XMLreader::NOEQUALATTRIBS);
	xml.SetFlag(XMLreader::SKIPUNKNOWNENTITIES);
	xml.SetFlag(XMLreader::SKIPBADNAMES);
	xml.SetFlag(XMLreader::GIVEWHITESPACE);

	CharFormat				stack[8], *sp = stack;

	while (XMLreader::TagType tag = xml.ReadNext(data)) {
		switch (tag) {
			case XMLreader::TAG_BEGIN: case XMLreader::TAG_BEGINEND:
				if (data.Is("font")) {
					sp[1] = sp[0];
					sp++;
					if (const char *col = data.Find("color")) {
						uint32	r, g, b;
						sscanf(col + 1, "%02x%02x%02x", &r, &g, &b);
						sp->Colour(win::Colour(r,g,b));
					}
				} else if (data.Is("body")) {
					if (const char *col = data.Find("bgcolor")) {
						uint32	r, g, b;
						sscanf(col + 1, "%02x%02x%02x", &r, &g, &b);
						re.SetBackground(win::Colour(r,g,b));
					}
				}
				break;

			case XMLreader::TAG_END:
				if (data.Is("font"))
					--sp;
				break;

			case XMLreader::TAG_CONTENT:
				re.SetSelection(CharRange::end());
				re.SetFormat(*sp);
				re.ReplaceSelection(string(data.Content()), false);
				break;
		}
	}
}

class ViewHTML : public TextWindow1 {
public:
	ViewHTML(const WindowPos &wpos, const char *title, const char *text, size_t len) : TextWindow1(wpos, title, CHILD | VISIBLE) {
		SetFont(win::Font("Courier New", 12));
#if 1
		HTMLformat(*this, memory_reader(const_memory_block(text, len)).me());
#else
		malloc_block	m(len);
		memcpy(m, text, len);
		RunThread([this, m]() {
			HTMLformat(*this, memory_reader(m).me());
		});
#endif
	}
};

class ViewTTY : public TextWindow1, public Thread {
	isolink_handle_t	handle;
public:
	int operator()() {
		char	buffer[1024];
		while (size_t r = isolink_receive0(handle, buffer, 1023)) {
			buffer[r] = 0;
			SetSelection(CharRange::end());
			ReplaceSelection(buffer, false);
			SendMessage(WM_VSCROLL, SB_BOTTOM);
		}
		return 0;
	}
public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		if (message == WM_NCDESTROY) {
			delete this;
			return 0;
		}
		return TextWindow1::Proc(message, wParam, lParam);
	}
	ViewTTY(const WindowPos &wpos, isolink_handle_t _handle) : TextWindow1(wpos, "TTY", CHILD | VISIBLE), Thread(this), handle(_handle) {
		Rebind(this);
		Start();
	}
};

class ViewLocalTTY : public TextWindow1 {
public:
	_iso_debug_print_t		isolink_debug_print_old;

	void debug_print(const char *buffer) {
		SetSelection(CharRange::end());
		ReplaceSelection(buffer, false);
		SendMessage(WM_VSCROLL, SB_BOTTOM);
	}

	static void _debug_print(void *me, const char *buffer) {
		((ViewLocalTTY*)me)->debug_print(buffer);
	}

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		if (message == WM_NCDESTROY) {
			delete this;
			return 0;
		}
		return TextWindow1::Proc(message, wParam, lParam);
	}
public:
	ViewLocalTTY(const WindowPos &wpos, const char *title) : TextWindow1(wpos, title, CHILD | VISIBLE) {
		Rebind(this);
		isolink_debug_print_old	= _iso_set_debug_print({_debug_print, this});
	}
	~ViewLocalTTY() {
		_iso_set_debug_print(isolink_debug_print_old);
	}
};

Point GetCharSize(win::Font font, wchar_t wch) {
	return DeviceContext::Screen().SelectSave(font).GetTextExtent(&wch, 1);
}

class EditorText : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is<string>() || type->Is<string16>() || type->Is("text8") || type->Is("text16") || type->Is("TTY");
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void, 64> &p) {
		if (p.IsType("TTY"))
			return *new ViewTTY(wpos, SendCommand(ISO::binary_data.RemoteTarget(), ISO_ptr<int>("GetDebugOut",0)));

#if 1
		D2DEditControl c(wpos, "edit", Control::CHILD | Control::VISIBLE);
		c.SetFont(win::Font("Courier New", 12));
		if (p.IsType<string>()) {
			c.SetText(((string*)p)->begin());

		} else if (p.IsType<string16>()) {
			c.SetText(str8(((string16*)p)->begin()));

		} else if (p.IsType("text16")) {
			ISO_openarray<wchar_t> *text	= p;
			c.SetText(*text, text->Count());

		} else {
			ISO_openarray<char> *text	= p;
			c.SetText(*text, text->Count());
		}
//		c.SetSelection(CharRange::all());
//		c.SetParagraph(RichEditControl::Paragraph().Tabs(GetCharSize(font, ' ').x * 4));
//		c.SetSelection(CharRange::begin());

		return c;
#else
		TextWindow	*tw = new TextWindow(wpos, "text");
		if (p.IsType<string>()) {
			tw->SetText(((string*)p)->getp());

		} else if (p.IsType<string16>()) {
			tw->SetText(((string16*)p)->getp());

		}else if (p.IsType("text16")) {
			ISO_openarray<wchar_t> *text	= p;
			tw->SetText(*text, text->Count());

		} else {
			ISO_openarray<char> *text	= p;
			tw->SetText(*text, text->Count());
		}
		return *tw;
#endif
	}
} editortext;

Control MakeTextViewer(const WindowPos &wpos, const char *title, const char *text, size_t len) {
	TextWindow	*tw = new TextWindow(wpos, title, Control::CHILD | Control::VISIBLE);
	tw->SetText(text, len);
	return *tw;
}

Control MakeHTMLViewer(const WindowPos &wpos, const char *title, const char *text, size_t len) {
	return *new ViewHTML(wpos, title, text, len);
}

Control MakeTTYViewer(const WindowPos &wpos) {
//	if (ViewLocalTTY::me)
//		return Control();
	return *new ViewLocalTTY(wpos, "TTY");
}

//-----------------------------------------------------------------------------
//	EditIX
//-----------------------------------------------------------------------------
struct RichEditControlOutput {
	RichEditControl	rich;
	streamptr		pos;

	RichEditControlOutput(RichEditControl _rich)  : rich(_rich), pos(0) {}

	size_t		writebuff(const void *buffer, size_t size) {
		CharRange	sel = rich.GetSelection();

//		rich.EnableRedraws(false);
		rich.HideSelection();
		rich.SetSelection(pos);
		rich.ReplaceSelection(string(str((const char*)buffer, size)), false);
		rich.SetSelection(sel);
		rich.HideSelection(false);
//		rich.EnableRedraws(true);
		pos += size;
		return size;
	}
	bool		exists()	{ return rich; }
	void		seek(streamptr offset)	{ pos = offset; }
	streamptr	length()	{ return pos; }
	bool		eof()		{ return false; }
};

template<typename S> struct abort_writer : S {
	bool	&abort;
	size_t	writebuff(const void *buffer, size_t size) {
		return abort ? 0 : S::writebuff(buffer, size);
	}
	abort_writer&	_clone()	{ return *this; }
	template<typename A> abort_writer(const A &a, const bool &_abort) : S(a), abort(const_cast<bool&>(_abort)) {}
};

class EditIX : public TextWindow1, public refs<EditIX> {
	MainWindow			&main;
	ISO_VirtualTarget	v;
	bool				abort;
	bool				writing;
public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				addref();
				break;

			case WM_SETFOCUS:
				if (writing)
					return 0;
				break;

			case WM_NCDESTROY:
				Super(message, wParam, lParam);
				abort	= true;
				hWnd	= 0;
				release();
				return 0;
		}
		return TextWindow1::Proc(message, wParam, lParam);
	}

	EditIX(MainWindow &_main, const WindowPos &wpos,  ISO_VirtualTarget &_v) : TextWindow1(wpos, _v.spec, CHILD | VISIBLE), main(_main), v(_v), abort(false), writing(false) {
		SetFont(win::Font("Arial", 12));
		Rebind(this);

		ISO::Browser2	b = v;
		if (v.External())
			b = v.GetPtr();

		RunThread([this, b] {
			abort_writer<buffered_writer<RichEditControlOutput, 1024> > mb(*this, abort);

			addref();
			writing	= true;
	//		EnableInput(false);
			ISO::ScriptWriter	ix(mb);
			ix.SetFlags(ISO::SCRIPT_VIRTUALS).SetMaxDepth(16).DumpData(b);
	//		EnableInput(true);
			writing	= false;
			release();
		});
	}
};

Control MakeIXEditor(MainWindow &main, const WindowPos &wpos, ISO_VirtualTarget &v) {
	return *new EditIX(main, wpos, v);
}

//-----------------------------------------------------------------------------
//	TextEditor
//-----------------------------------------------------------------------------

class TextEditor : public Subclass<TextEditor, RichEditControl> {
	MainWindow	&main;
	Control		*cref;
	filename	fn;
	int			line_height, char_width;

	bool Save() {
		Busy	bee;
		FileOutput	out(fn);
		if (!out.exists())
			return false;

		string	buffer		= GetText(CharRange::all());
		const char *p, *n;
		for (p = buffer; n = strchr(p, 0x0d); p = n + 1) {
			out.writebuff(p, n + 1 - p);
			out.putc('\n');
		}
		out.writebuff(p, strlen(p));
		return true;
	}

public:
	static Control	texteditor;

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_GETTEXT: {
				int	n = min(wParam, strlen(fn) + 1);
				memcpy((void*)lParam, fn, n);
				return n - 1;
			}

			case WM_COMMAND:
				switch (int id = LOWORD(wParam)) {
					case ID_EDIT_SELECT: {
						ISO::Browser2	*b	= (ISO::Browser2*)lParam;
						if (ISO_ptr<void> p = b->GetPtr()) {
							if (b->SkipUser().GetType() == ISO::REFERENCE)
								p = **b;
							const char		*fn;
							if (int line = ISO::Tokeniser::GetLineNumber(p, &fn))
								Set(fn, line);
						}
						return 0;
					}

					case ID_SCRIPT_SAVE:
						Save();
						return 0;

					case ID_SCRIPT_SAVEAS:
						if (GetSave(*this, fn, "Save As", "IX file\0*.ix\0"))
							Save();
						return 0;

					case ID_SCRIPT_COMPILE: {
						string	buffer		= GetText(CharRange::all());
						for (char *p = buffer; *p; ++p) {
							if (*p == 0x0d)
								*p = '\n';
						}
						try {
							((IsoEditor&)main).AddEntry(ISO::ScriptRead(fn.name(), fn, memory_reader(buffer.data()), ISO::SCRIPT_LINENUMBERS | ISO::SCRIPT_KEEPEXTERNALS));
						} catch (const char *s) {
							MessageBoxA(*this, s, "Compilation error", MB_ICONERROR|MB_OK);
						}
						return 0;
					}
				}
				break;

			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
				SetAccelerator(*this, 0);
				SetFocus();
				break;

			case WM_CONTEXTMENU:
				Menu(IDR_MENU_SUBMENUS).GetSubMenuByName("Script").Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
				break;

			case WM_ISO_SET:
				Set((const char*)lParam, wParam);
				break;

/*			case WM_SETFOCUS:
				main.AddSubMenu("Script", Menu(IDR_MENU_SUBMENUS));
				break;
			case WM_KILLFOCUS:
				main.RemoveSubMenu("Script");
				break;
*/
			case WM_NCDESTROY:
				Super(message, wParam, lParam);
				if (cref)
					*cref = Control();
				delete this;
				return 0;
		}
		return Super(message, wParam, lParam);
	}

	TextEditor(MainWindow &_main, const WindowPos &wpos, Control *c) : main(_main), cref(c) {
		HINSTANCE	hInst	= LoadLibraryA("RICHED20.DLL");
		Create(wpos, NULL, CHILD | VISIBLE | HSCROLL | VSCROLL | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL, CLIENTEDGE);
		if (cref)
			*cref = *this;

		SetFormat(CharFormat().Font("Consolas").Weight(FW_REGULAR).Size(12 * 15), SCF_DEFAULT);

		SetText("XX\rXX");
		POINTL	p1, p2, p3, p4;
		SendMessage(EM_POSFROMCHAR, (WPARAM)&p1, 0);
		SendMessage(EM_POSFROMCHAR, (WPARAM)&p2, 1);
		SendMessage(EM_POSFROMCHAR, (WPARAM)&p3, 3);
		SendMessage(EM_POSFROMCHAR, (WPARAM)&p4, 4);

		char_width	= p2.x - p1.x;
		line_height	= p3.y - p1.y;
	}

	void Set(const char *_fn, int line) {

		CHARRANGE	sel	= {0, 0};

		if (fn == _fn) {
			string		buffer	= GetText(CharRange::all());
			const char *p = buffer, *n;
			for (int i = line; (n = strchr(p, 0x0d)) && --i; p = n + 1);
			sel.cpMin = p - buffer;
			sel.cpMax = n - buffer;

		} else {
			fn = _fn;
			SetText((char*)malloc_block::zero_terminated(FileInput(fn).me()));

			string	buffer		= GetText(CharRange().all());
			for (char *p = buffer; *p; ++p) {
				if (*p = 0x0d)
					*p = '\n';
			}
			memory_reader		ms(buffer.data());
			ISO::Tokeniser	tokeniser(ms);

			for (;;) {
				int		c		= tokeniser.SkipWhitespace();
				uint32	start	= ms.tell() - 1;
				int		t		= 0;

				if (!sel.cpMin && tokeniser.GetLineNumber() == line)
					sel.cpMin = start;
				if (!sel.cpMax && tokeniser.GetLineNumber() > line)
					sel.cpMax = start;

				auto	tok = tokeniser.GetToken(c);
				if (tok == ISO::Tokeniser::TOK_EOF)
					break;

				if (tok == ISO::Tokeniser::TOK_NUMBER) {
					read_number(tokeniser, 0);
				} else if (tok >= ISO::Tokeniser::TOK_KEYWORDS) {
					t = 1;
				} else if (tok == ISO::Tokeniser::TOK_COMMENT || tok == ISO::Tokeniser::TOK_LINECOMMENT) {
					tokeniser.SwallowComments(tok);
					t = 2;
				} else if (tok == ISO::Tokeniser::TOK_STRINGLITERAL) {
					t = 3;
				} else if (tok == '#') {
					tokeniser.SwallowComments(ISO::Tokeniser::TOK_LINECOMMENT);
					t = 4;
				}
				if (t) {
					static const win::Colour cols[] = {
						win::Colour(0,0,192),
						win::Colour(0,128,0),
						win::Colour(192,0,0),
						win::Colour(192,0,192),
					};
					SetSelection(CharRange(start, ms.tell()));
					SetFormat(CharFormat().Colour(cols[t - 1]));
				}
			}

			CHARRANGE	all	= {0, -1};
			SendMessage(EM_EXSETSEL, 0, (LPARAM)&all);
			PARAFORMAT2	pm;
			clear(pm);
			pm.cbSize		= sizeof(pm);
			pm.dwMask		= PFM_TABSTOPS;
			pm.cTabCount	= MAX_TAB_STOPS;
			for (int i = 0; i < MAX_TAB_STOPS; i++)
				pm.rgxTabs[i] = i * char_width * 4 * 15;
			SendMessage(EM_SETPARAFORMAT, 0, (LPARAM)&pm);

			SendMessage(EM_SETUNDOLIMIT, 0);
			SendMessage(EM_SETUNDOLIMIT, 100);
		}

		SendMessage(EM_EXSETSEL, 0, (LPARAM)&sel);
		SendMessage(EM_HIDESELECTION, 0);
		EnsureVisible();
		//SendMessage(EM_SETSCROLLPOS, 0, (LPARAM)&Point(0, max(line - 8, 0) * line_height));
		SetFocus();
	}
};

Control	TextEditor::texteditor;

bool MakeTextEditor(MainWindow &main, const WindowPos &wpos, Control *c) {
	*new TextEditor(main, wpos, c);
	return true;
}
