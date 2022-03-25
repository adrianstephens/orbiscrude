#include "main.h"
#include "base/maths.h"
#include "iso/iso_script.h"
#include "directory.h"
#include "stream.h"

using namespace iso;
using namespace app;
//-----------------------------------------------------------------------------
//	Finder
//-----------------------------------------------------------------------------

class FinderThread : public Thread {
protected:
	Control							target;
	ref_ptr<with_refs<bool> >		abort;
	hash_map<void*, ISO_ptr<void> > found;
	ScaleProgress					prog;

	void AddFound(ISO_ptr<void> &p, const char *r) {
		route_ptr		rp(p, r);
		target(WM_ISO_SET, 0, (LPARAM)&rp);
	}
	void AddFound(const ISO::Browser2 &b, const char *r, tag2 id = tag2()) {
		if (!b.IsPtr() && b.SkipUser().GetType() == ISO::VIRTUAL) {
			if (b.SkipUser().IsVirtPtr())
				AddFound(*b, r, id);
			return;
		}
#if 1
		ISO_ptr<void>	p;
		if (b.IsPtr())
			p = b;
		else
			p = b.Duplicate(id);
		AddFound(p, r);
#else
		ISO_ptr<void>	&p = found[(void*)b];
		if (!p) {
			if (b.IsPtr())
				p = b;
			else
				p = b.Duplicate(id);
			AddFound(p, r);
		}
#endif
	}
	void TestAbort() {
		if (*abort) {
			delete this;
			exit(0);
		}
	}
	void Finished() {
		target.SetText("Find (finished)");
	}
	void Done(int depth = 0) {
		if ((++prog).ready()) {
			buffer_accum<256>	ba;
			ba << "Find (" << prog << ")";
			if (depth)
				ba << ", depth:" << depth;
			target.SetText(ba);
		}
	}
	template<class T> FinderThread(T *t, Control &_target, with_refs<bool> *_abort) : Thread(t), target(_target), abort(_abort) {}
};

class FindTypeThread : public FinderThread {
	ISO::Browser2		b;
	tag2				value;
	const ISO::Type		*type;

	void FindAll(const ISO::Browser2 &b, route &r, int depth) {
		if (++depth > 20)
			return;
		TestAbort();
		if (b.GetTypeDef()->SameAs(type, ISO::MATCH_NOUSERRECURSE))
			AddFound(b, r);

		if (!b.External()) {
			if (b.SkipUser().GetType() == ISO::REFERENCE) {
				ISO::Browser2	b2 = *b;
				if (type->GetType() == ISO::REFERENCE && b2.GetTypeDef()->SameAs(((ISO::TypeReference*)type)->subtype))
					AddFound(b, r);
				FindAll(b2, r, depth);

			} else if (b.SkipUser().IsVirtPtr()) {
				FindAll(*b, r, depth);

			} else {
				int		n		= b.Count();
				auto	saver	= save(prog, n);
				for (int i = 0; i < n; i++) try {
					FindAll(b[i], r.add_node(i), depth);
					Done();
				} catch (const char *s) {
					ISO_TRACEF(s) << '\n';
				}
			}
		}
	}

public:
	int	operator()() {
		route	r;
		FindAll(b, r, 0);
		Finished();
		delete this;
		return 0;
	}
	FindTypeThread(Control &_target, with_refs<bool> *_abort, const ISO::Browser2 &_b, const ISO::Type *_type) : FinderThread(this, _target, _abort), b(_b), type(_type) {
		Start();
	}
};

class FindValueThread : public FinderThread {
public:
	enum FIND_FLAGS {
		FIND_TYPES		= 1 << 0,
		FIND_VALUES		= 1 << 1,
		FIND_FIELDS		= 1 << 2,
		FIND_DEPTHFIRST	= 1 << 3,
		_FIND_LAST		= 1 << 4,
	};
private:
	ISO::Browser2		b;
	tag2				value;
	uint32				flags;
	int					recurse;
	int					current_depth;
	bool				wildcards;

	bool FindAll(const ISO::Browser2 &b, route &r, int depth) {
		TestAbort();
		switch (b.SkipUser().GetType()) {
			case ISO::REFERENCE:
				if (!b.HasCRCType() && !b.External())
					return FindAll(*b, r, depth);
				break;

			case ISO::ARRAY:
			case ISO::OPENARRAY:
				if (!(flags & (FIND_FIELDS | FIND_VALUES)) && !b.GetTypeDef()->ContainsReferences())
					return false;
				goto join;

			case ISO::VIRTUAL:
				if (auto b2 = *b) {
					if (FindAll(b2, r, depth))
						return true;
				}

			// fall through
			default:
			join: {
				int		n		= b.Count();
				auto	saver	= save(prog, n);
				bool	more	= false;
				for (int i = 0; i < n; i++) try {
					ISO::Browser2	b2		= b[i];
					if ((flags & FIND_DEPTHFIRST) || depth == 1) {
						if (tag2 name2 = b.GetName(i)) {
							if (wildcards ? matches(tag(name2), tag(value)) : name2 == value)
								AddFound(b2, r.add_node(i), name2);
						}
						if ((flags & FIND_VALUES)) {
							string	s;
							if (s = b2.Get(s)) {
								if (wildcards ? matches(s, tag(value)) : s == tag(value))
									AddFound(b2, r.add_node(i), "string");
							}
						}
					}
					more |= depth == 1 || FindAll(b2, r.add_node(i), depth - 1);
					Done(current_depth);
				} catch (const char *s) {
					ISO_TRACEF(s) << '\n';
				}
				return more;
			}
		}
		return false;
	}

public:
	int	operator()() {
		route	r;
		if (flags & FIND_DEPTHFIRST) {
			current_depth	= 0;
			FindAll(b, r, recurse);
		} else {
			for (current_depth = 1; current_depth != recurse && FindAll(b, r, current_depth); current_depth++);
		}
		Finished();
		delete this;
		return 0;
	}
	FindValueThread(Control &_target, with_refs<bool> *_abort, const ISO::Browser2 &_b, const char *_value, int _flags, int _recurse)
		: FinderThread(this, _target, _abort), b(_b), value(_value), wildcards(str(_value).find(char_set("*?"))), flags(_flags), recurse(_recurse) {
		Start();
	}
};

class FindFileThread : public FinderThread {
	string		dir, value;
	int			recurse;

	void FindAllFiles(const char *dir, int depth) {
		TestAbort();
		for (directory_iterator d = filename(dir).add_dir(tag(value)).begin(); d; ++d) {
			TestAbort();
			ISO_ptr<void>	e;
			tag				id	= (const char*)d;
			e.CreateExternal(filename(dir).add_dir(d), id);
			AddFound(e, 0);
		}

		if (depth != 1) {
			for (directory_iterator d = filename(dir).add_dir("*.*").begin(); d; ++d) {
				if (d.is_dir() && d[0] != '.')
					FindAllFiles(filename(dir).add_dir(d), depth - 1);
			}
		}
	}

public:
	int	operator()() {
		FindAllFiles(dir, recurse);
		Finished();
		delete this;
		return 0;
	}
	FindFileThread(Control &_target, with_refs<bool> *_abort, const char *_dir, const char *_value, int _recurse)
		: FinderThread(this, _target, _abort), dir(_dir), value(_value), recurse(_recurse) {
		Start();
	}
};

class FinderWindow : public Window<FinderWindow> {
	static	ControlArrangement::Token arrange[];
	ControlArrangement			arrangement;
	StaticControl				label;
	Control						_value;
	Control						_tree;
	Control						_depth;
	ToolBarControl				toolbar;

	MainWindow					&main;
	EditControl2				value;
	EditControl2				depth;
	TreeColumnControl			treecolumn;
	TreeColumnDisplay			treecolumn_display;

	ISO_ptr<anything>			p;
	ISO::Browser2				b;
	string						route;
	dynamic_array<string>		routes;
	ref_ptr<with_refs<bool>>	abort;
	uint32						flags;

	void			SetFlags(uint32 _flags) {
		flags = _flags;
		for (int id = ID_FIND_TYPES, f = flags | FindValueThread::_FIND_LAST; f > 1; ++id, f >>= 1)
			toolbar.CheckButton(id, !!(f & 1));
	}
public:
	LRESULT			Proc(UINT message, WPARAM wParam, LPARAM lParam);

	FinderWindow(MainWindow &_main, const ISO::Browser2 &_b, const char *_route) : main(_main), route(_route),
		arrangement(arrange, &label, 5), b(_b), p("found"), abort(new with_refs<bool>(false)) {
		Create(WindowPos(main, Rect(0,0,320,640)), "Find", OVERLAPPEDWINDOW | CLIPCHILDREN | VISIBLE, NOEX);
	}
};

Control MakeFinderWindow(MainWindow &main, const ISO::Browser2 &b, const char *route) {
	return *new FinderWindow(main, b, route);
}

LRESULT FinderWindow::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			label.Create(*this, NULL, CHILD | VISIBLE | SS_CENTERIMAGE);
			value.Create(*this, NULL, CHILD | VISIBLE | BORDER | ES_AUTOHSCROLL, NOEX, 1);
			depth.Create(*this, NULL, CHILD | VISIBLE | BORDER | ES_AUTOHSCROLL, NOEX, 1);
			treecolumn.Create(WindowPos(*this, Rect(0,0,0,0)), NULL, CHILD | VISIBLE | BORDER | HSCROLL | TCS_GRIDLINES | TCS_HEADERAUTOSIZE);
			toolbar.Create(*this, NULL, CHILD | VISIBLE | CCS_NORESIZE | CCS_NOPARENTALIGN);
			_value	= value;
			_depth	= depth;
			_tree	= treecolumn;

			toolbar.Init(IDR_TOOLBAR_FIND);
			Rect	br = toolbar.GetItemRect(toolbar.Count() - 1);
			arrange[0].set(br.Bottom());
			arrange[5].set(-br.Right());

			HeaderControl	header	= treecolumn.GetHeaderControl();
			header.SetValue(GWL_STYLE, CHILD | VISIBLE | HDS_FULLDRAG);
			HeaderControl::Item("Symbol").	Format(HDF_LEFT).Width(100).Insert(header, 0);
			HeaderControl::Item("Type").	Format(HDF_LEFT).Width(100).Insert(header, 1);
			HeaderControl::Item("Value").	Format(HDF_LEFT).Width(100).Insert(header, 2);
			treecolumn.SetMinWidth(2, 100);
			treecolumn.GetTreeControl().style = CHILD | VISIBLE | CLIPSIBLINGS | TVS_NOHSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT;

			label.SetText("value:");
			label.SetFont(Font(Font::Caption()));

			value.SetFont(Font(Font::Caption()));
			depth.SetFont(Font(Font::Caption()));
			value.SetFocus();

			SetFlags(FindValueThread::FIND_VALUES);
			SetAccelerator(*this, IDR_ACCELERATOR_FIND);
			return 0;
		}

		case WM_SIZE:
			arrangement.Arrange(GetClientRect());
			return 0;

		case WM_CHAR:
			if (wParam == '\r' && !(value.style & ES_READONLY)) {
				depth(EM_SETREADONLY, TRUE);
				fixed_string<1024>	d = depth.GetText();

				int	recurse = from_string<int>(d);

				value(EM_SETREADONLY, TRUE);
				fixed_string<1024>	v = value.GetText();

				ISO_TRACEF("Find %s\n", (char*)v);

				if (flags & FindValueThread::FIND_TYPES) {
					memory_reader		m(v.data());
					const ISO::Type *type = ISO::ScriptReadType(m);

					if (type && m.eof())
						new FindTypeThread(*this, abort, b, type);
					break;
				}
#if 0
				if ((*b).Is("Directory"))
					new FindFileThread(*this, abort, GetDirectoryPath(*b), v, recurse);

				else
#endif
					new FindValueThread(*this, abort, b, v, flags, recurse);
			}
			break;

		case WM_ISO_SET:
			if (route_ptr	*rp	= (route_ptr*)lParam) {
				int	i	= p->Count();
				p->Append(rp->p);
				routes.push_back(rp->r);
				ISOTree(treecolumn.GetTreeControl()).AddItem(TVI_ROOT, TVI_LAST, ISOTree::ItemName(p, i), i, ISOTree::GetFlags(rp->p));
				return 1;
			}
			break;

		case WM_COMMAND:
			switch (uint16 id = LOWORD(wParam)) {
				case ID_FIND_TYPES:
					SetFlags(flags & FindValueThread::FIND_TYPES ? FindValueThread::FIND_VALUES : FindValueThread::FIND_TYPES);
					break;
				case ID_FIND_VALUES:
					SetFlags(flags & FindValueThread::FIND_TYPES ? FindValueThread::FIND_VALUES : (flags ^ FindValueThread::FIND_VALUES));
					break;
				case ID_FIND_FIELDS:
					SetFlags(flags & FindValueThread::FIND_TYPES ? FindValueThread::FIND_FIELDS : (flags ^ FindValueThread::FIND_FIELDS));
					break;
				case ID_FIND_DISPLAY: {
					Busy			bee;
					for (auto i = p->begin(), e = p->end(); i != e; ++i) {
						ISO::Browser2	b(*i);
						if (Editor *ed = Editor::Find(b)) {
							ed->Create(main, Docker(&main).Dock(DOCK_RIGHT | DOCK_EXTEND | DOCK_FIXED_SIB | DOCK_OR_TAB, 480), (ISO_ptr<void,64>)b);
						}
					}
					break;
				}
			}
			break;

		case WM_NOTIFY: {
			NMHDR			*nmh	= (NMHDR*)lParam;
			ISOTree			tree	= treecolumn.GetTreeControl();
			HeaderControl	header	= treecolumn.GetHeaderControl();
			switch (nmh->code) {
				case TCN_GETDISPINFO: {
					NMTCCDISPINFO *nmdi = (NMTCCDISPINFO*)nmh;
					treecolumn_display.Display(p, tree, nmdi);
					return nmdi->iSubItem;
				}
				case TVN_ITEMEXPANDING:
					treecolumn_display.Expanding(*this, p, tree, (NMTREEVIEW*)nmh, 1024);
					return 0;

				case NM_CUSTOMDRAW: {
					NMCUSTOMDRAW *nmcd = (NMCUSTOMDRAW*)nmh;
					if (nmcd->dwDrawStage == CDDS_POSTPAINT && nmh->hwndFrom == tree)
						treecolumn_display.PostDisplay(treecolumn);
					break;
				}
				case NM_DBLCLK:
					if (HTREEITEM h = tree.HitTest(tree.ToClient(GetMousePos()))) {
						buffer_accum<256>	ba;
						tree.GetRoute(ba, h);
						int		i;
						size_t	n	= from_string(ba.begin() + 1, i);
						const char	*p	= ba.begin() + n + 2;
						main(WM_ISO_SELECT, 0, (LPARAM)(const char*)(route + routes[i] + p));
						return 1;
					}
					break;
			}
			break;
		}

		case WM_NCDESTROY:
			*abort = true;
			delete this;
			return 0;


	}
	return Super(message, wParam, lParam);
}

ControlArrangement::Token FinderWindow::arrange[] = {
	ControlArrangement::VSplit(GetNonClientMetrics().iCaptionHeight),
	ControlArrangement::Skip(1),
	ControlArrangement::ControlRect(2),

	ControlArrangement::HSplit(40),
	ControlArrangement::ControlRect(0),
	ControlArrangement::HSplit(-80),

	ControlArrangement::Skip(1),
	ControlArrangement::ControlRect(4),

	ControlArrangement::HSplit(-40),
	ControlArrangement::ControlRect(1),
	ControlArrangement::ControlRect(3),
};
