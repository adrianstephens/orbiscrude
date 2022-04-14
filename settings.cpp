#include "main.h"
#include "resource.h"
#include "windows/treecolumns.h"
#include "windows/filedialogs.h"
#include "windows/registry.h"
#include "windows/control_helpers.h"
#include "iso/iso_script.h"
#include "vector_iso.h"

using namespace iso;
using namespace win;
using namespace app;

//-----------------------------------------------------------------------------
//	TreeControl operations
//-----------------------------------------------------------------------------

fixed_string<256> ItemName(const ISO::Browser2 &b, int i) {
	if (tag2 id = b.GetName(i))
		return replace(tag(id), "&", "&&");
	return format_string("[%i]", i);
};

void RemoveTreeItem(TreeControl tree, const ISO::Browser2 &b, HTREEITEM hItemParent, int index) {
	HTREEITEM hChild = tree.GetChildItem(hItemParent);
	for (int i = 0; i < index; i++)
		hChild = tree.GetNextItem(hChild);

	HTREEITEM hSel = tree.GetNextItem(hChild);
	tree.DeleteItem(hChild);

	if (hSel) {
		HTREEITEM h = hSel;
		for (int i = index; h; i++, h = tree.GetNextItem(h))
			TreeControl::Item(h).Get(tree).Text(ItemName(b, i)).Param((void*)i).Set(tree);
	} else if (index == 0) {
		hSel = hItemParent;
	} else {
		hSel = tree.GetChildItem(hItemParent);
		for (int i = 0; i < index - 1; i++)
			hSel = tree.GetNextItem(hSel);
	}
	tree.SetSelectedItem(hSel);
}

HTREEITEM AddTreeItem(TreeControl tree, HTREEITEM hItemParent, const char *name, int i, bool children) {
	return tree.InsertItem(
		TVIF_TEXT | TVIF_IMAGE | TVIF_PARAM | TVIF_CHILDREN,
		name,
		0, 0,
		0, 0,
		hItemParent, TVI_LAST,
		(void*)i,
		int(children)
	);
}

bool HasChildren(const ISO::Browser2 &b) {
	switch (b.SkipUser().GetType()) {
		case ISO::COMPOSITE:
		case ISO::ARRAY:
		case ISO::OPENARRAY:
		case ISO::VIRTUAL:
			return true;
		case ISO::REFERENCE:
			return !b.ISO::Browser::External() && !b.HasCRCType() && HasChildren(*b);
		default:
			return false;
	};
}

int SetupTree(TreeControl tree, const ISO::Browser2 &b, HTREEITEM hItemParent) {
	switch (b.GetType()) {
		case ISO::ARRAY:
		case ISO::OPENARRAY:
		case ISO::COMPOSITE:
			for (int i = 0, c = b.Count(); i < c; i++)
				SetupTree(tree, b[i], AddTreeItem(tree, hItemParent, ItemName(b, i), i, HasChildren(b[i])));
			break;

		case ISO::REFERENCE:
			if (!b.ISO::Browser::External() && !b.HasCRCType())
				return SetupTree(tree, *b, hItemParent);
			break;

		case ISO::VIRTUAL: {
			int	c = b.Count();
			if (c == 0) {
				tree.GetItem(hItemParent).SetState(TVIS_EXPANDEDONCE).Set(tree);
				SetupTree(tree, *b, hItemParent);
				break;
			}
			for (int i = 0; i < c; i++)
				SetupTree(tree, b[i], AddTreeItem(tree, hItemParent, ItemName(b, i), i, true));
			break;
		}

		case ISO::USER:
			return SetupTree(tree, b.SkipUser(), hItemParent);
	}
	return 0;
}

ISO::Browser2 GetChild(ISO::Browser2 &b, TreeControl tree, HTREEITEM hItem) {
	TreeControl::Item i = tree.GetItem(hItem, TVIF_PARAM | TVIF_STATE);
	if (i.state & TVIS_USERMASK)
		return (ISO::ptr<void>&)i.Param();
	return b[(int)i.Param()];
}

int GetIndex(TreeControl tree, HTREEITEM hItem) {
	return tree.GetItemParam(hItem);
}

struct TreeIterator {
	typedef callback<void(ISO::Browser2, int i)> cbtype;
	cbtype			cb;
	ISO::Browser2	b;
	TreeControl		tree;

	void	Up(HTREEITEM hItem) {
		if (hItem) {
			Up(tree.GetParentItem(hItem));
			while (b.GetType() == ISO::REFERENCE)
				b = *b;
			cb(b, tree.GetItemParam(hItem));
			b = GetChild(b, tree, hItem);
		}
	}

	template<typename T> TreeIterator(T &&t, const ISO::Browser2 &_b, TreeControl _tree, HTREEITEM hItem) : cb(&t), b(_b), tree(_tree) {
		Up(hItem);
	}
	operator ISO::Browser2()	const { return b; }
};

ISO::Browser2 GetBrowser(const ISO::Browser2 &root, TreeControl tree, HTREEITEM hItem) {
	struct handler {
		void	operator()(const ISO::Browser2 &b, int i) {}
	};
	return TreeIterator(handler(), root, tree, hItem);
}

ISO::Browser2 GetVirtual(const ISO::Browser2 &root, TreeControl tree, HTREEITEM hItem, ISO::Browser &v, buffer_accum<256> &spec) {
	struct handler {
		ISO::Browser			&v;
		buffer_accum<256>	&spec;

		void	operator()(const ISO::Browser2 &b, int i) {
			if (b.SkipUser().GetType() == ISO::VIRTUAL) {
				v = b;
				spec.reset();
			}
			spec << '[' << i << ']';
		}
		handler(ISO::Browser &_v, buffer_accum<256> &_spec) : v(_v), spec(_spec) {}
	};
	return TreeIterator(handler(v, spec), root, tree, hItem);
}

//-----------------------------------------------------------------------------
//	Registry
//-----------------------------------------------------------------------------

void LoadFromRegistry(const ISO::Browser2 &b, const RegKey &r) {
//	for (ISO::Browser2::iterator i = b.begin(), e = b.end(); i != e; ++i) {
	for (int i = 0, n = b.Count(); i < n; ++i) {
		tag2			id2		= b.GetName(i);
		tag				id		= id2;
		ISO::Browser2	b2		= b[i];//*i;

		if (b2.GetType() == ISO::REFERENCE)
			b2 = *b2;

		const ISO::Type	*type	= b2.GetTypeDef()->SkipUser();
		RegKey::Value	v		= r.values()[id];

		switch (type->GetType()) {
			case ISO::OPENARRAY: {
				const ISO::Type	*sub = type->SubType()->SkipUser();
				switch (sub->GetType()) {
					case ISO::REFERENCE:
						LoadFromRegistry(b2, r[id]);
						break;

					case ISO::STRING:
						if (string s = v) {
							b2.Resize(0);
							char *t, *u;
							for (t = s; u = strchr(t, ';'); t = u + 1) {
								*u = 0;
								b2.Append().Set(t);
							}
							b2.Append().Set(t);
						}
						break;

					default:
						if (sub->IsPlainData()) {
							uint32	count = v.size / sub->GetSize();
							b2.Resize(count);
							v.get_raw(*b2, v.size);
						} else {
							ISO_TRACE("?\n");
						}
						break;
				}
				break;
			}
			case ISO::STRING:
				if (string s = v)
					b2.Set(s);
				break;

			default:
				if (type->IsPlainData()) {
					size_t bsize = type->GetSize();
					if (bsize <= 4) {
						uint32	x;
						if (v.get(x))
							b2.UnsafeSet(&x);
					} else if (bsize <= 8) {
						uint64	x;
						if (v.get(x))
							b2.UnsafeSet(&x);
					} else {
						v.get_raw(b2, bsize);
					}
					break;
				}
			case ISO::COMPOSITE:
				LoadFromRegistry(b2, r[id]);
				break;
		}
	}
}

bool SaveToRegistry(const ISO::Browser2 &b, RegKey::Value &&v) {
	const ISO::Type	*type	= b.GetTypeDef();
	if (!type)
		return false;

	if (type->IsPlainData()) {
		uint32 bsize = type->GetSize();
		if (bsize <= 4)
			v = *(uint32*)b & bits(bsize * 8);
		else if (bsize <= 8)
			v = *(uint64*)b & bits64(bsize * 8);
		else
			v.set(b, bsize);

	} else if (type->SameAs<ISO::ptr_string<char,32>>()) {
//		v = (const char*)type->ReadPtr(b);
		v = *(ISO::ptr_string<char,32>*)b;

	} else if (type->SameAs<ISO::OpenArray<ISO::ptr_string<char,32>> >(ISO::MATCH_IGNORE_SIZE)) {
		buffer_accum<4096>	ba;
#if 1
		ISO::OpenArray<ISO::ptr_string<char,32>>	*a	= b;
		for (int i = 0, n = a->Count(); i < n; i++) {
			ba << (*a)[i];
			if (i < n - 1)
				ba << ';';
		}
#else
		for (int i = 0, n = b.Count(); i < n; i++) {
			ba << b[i].GetString();
			if (i < n - 1)
				ba << ';';
		}
#endif
		v = (const char*)ba;

	} else if (type->GetType() == ISO::OPENARRAY && type->SubType()->IsPlainData()) {
		v.set(b[0], b.Count() * b[0].GetSize());

	#if 0
	} else if (type->SameAs<anything>()) {
		anything	*a = b;
		bool		has_id = false;
		for (auto &i : *a) {
			if (has_id = !!i.ID())
				break;
		}
		if (!has_id) {

		}
	#endif
	} else {
		return false;
	}
	return true;
}

void SaveToRegistry(const ISO::Browser2 &b, const RegKey &r) {
	buffer_accum<8>	ba;
	int				x = 0;
	for (ISO::Browser2::iterator i = b.begin(), e = b.end(); i != e; ++i, ++x) {
		tag2			id2		= i.GetName();
		ISO::Browser2	b		= *i;
		if (b.GetType() == ISO::REFERENCE)
			b = *b;

		tag		id		= id2 ? id2 : (ba.reset() << '[' << x << ']').term();
		if (!SaveToRegistry(b, r.values()[id]))
			SaveToRegistry(b, r.subkey(id, KEY_ALL_ACCESS));
	}
}

class RegistryBacked : public ISO::VirtualDefaults {
	ISO::Browser2	b;
	RegKey			r;
public:
	RegistryBacked(const pair<const ISO::Browser2,HKEY> &i) : b(i.a), r(i.b) {
		LoadFromRegistry(b, r);
	}
	~RegistryBacked()	{
		SaveToRegistry(b, r);
	}
	tag2			GetName(int i = 0)			{ return b.GetName(i); }
	ISO::Browser2	Index(int i)				{ return b.Index(i); }
	int				GetIndex(tag2 id, int from)	{ return b.GetIndex(id, from); }
	int				Count()						{ return b.Count(); }
	ISO::Browser2	Deref()						{ return b; }
	bool			Update(const char *spec, bool from);
};

bool RegistryBacked::Update(const char *spec, bool from) {
	ISO::Browser2	b2 = b;
	RegKey			r2 = r;

	string_scan s(spec);
	for (;;) {
		int	i = b2.ParseField(s);
		if (i < 0)
			break;

		tag		id	= b2.GetName(i);
		if (!id)
			break;

		b2	= b2[i];
		if (!r2.HasSubKey(id)) {
			while (b2.GetType() == ISO::REFERENCE)
				b2 = *b2;

			return SaveToRegistry(b2, r2.values()[id]);
		}

		r2	= r2[id];
	}
	SaveToRegistry(b2, r2);
	return true;
}

ISO::ptr<void> MakeRegistryBacked(const ISO::Browser2 &b, HKEY h) {
	return ISO::ptr<RegistryBacked>(b.GetName(), make_pair(b, h));
}

ISO_DEFUSERVIRT(RegistryBacked);

//-----------------------------------------------------------------------------
//	Settings
//-----------------------------------------------------------------------------

class SettingsWindow : public SeparateWindow {
	typedef	SeparateWindow	Super;

	TreeColumnControl	treecolumn;
	EditControl2		edit_control;
	ImageList			images;
	HTREEITEM			edit_item;
	ISO::Browser2		settings;

	ISO::Browser2		GetBrowser(HTREEITEM hItem) {
		return ::GetBrowser(settings, treecolumn.GetTreeControl(), hItem);
	}
	ISO::Browser2		GetBrowser(TVITEMA &item) {
		TreeControl	tree = treecolumn.GetTreeControl();
		return ::GetBrowser(settings, tree, tree.GetParentItem(item.hItem))[int(item.lParam)];
	}
	ISO::Browser2		GetVirtual(HTREEITEM hItem, ISO::Browser &v, buffer_accum<256> &spec) {
		return ::GetVirtual(settings, treecolumn.GetTreeControl(), hItem, v, spec);
	}
	void				EditItem(HTREEITEM h);
	int					DrawButton(DeviceContext dc, const Rect &rect, HTREEITEM h);
	void				PressButton(HTREEITEM h);

public:
	static SettingsWindow	*me;

	LRESULT	Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	SettingsWindow() : SeparateWindow(48), images(IDB_SETTINGS, 16, LR_CREATEDIBSECTION) {
		me	= this;
		Create(WindowPos(0, Rect(512, 640, 1024, 1024)), "Settings",
			OVERLAPPEDWINDOW | CLIPCHILDREN | VISIBLE,
			NOEX
		);
		Rebind(this);
		settings = ISO::root("settings");
		SetupTree(treecolumn.GetTreeControl(), settings, TVI_ROOT);
	}
};

SettingsWindow *SettingsWindow::me;

LRESULT SettingsWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			treecolumn.Create(GetChildWindowPos(), NULL, CHILD | VISIBLE | HSCROLL | TCS_GRIDLINES | TCS_HEADERAUTOSIZE, CLIENTEDGE);
			treecolumn.GetTreeControl().style = CHILD | VISIBLE | CLIPSIBLINGS | TVS_NOHSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT;
			HeaderControl	header	= treecolumn.GetHeaderControl();
			header.style = CHILD | VISIBLE | HDS_FULLDRAG;
			HeaderControl::Item("Setting").	Format(HDF_LEFT).Width(200).Insert(header, 0);
			HeaderControl::Item("").		Format(HDF_LEFT | HDF_FIXEDWIDTH).Width( 20).Insert(header, 1);
			HeaderControl::Item("Value").	Format(HDF_LEFT).Width(100).Insert(header, 2);
			treecolumn.AdjustColumns();
			SetChild(treecolumn);
			break;
		}

		case WM_SETFOCUS:
			SetAccelerator(*this, IDR_ACCELERATOR_SETTINGS);
			return 0;

		case WM_CHAR:
			if (wParam == 27)
				edit_control.SetText("");
			if (edit_control.hWnd)
				edit_control.Destroy();
			return 0;

		case WM_COMMAND: {
			int	id = LOWORD(wParam);
			switch (id) {
   				case ID_EDIT:
					switch (HIWORD(wParam)) {
						case EN_KILLFOCUS:
							if (size_t len = edit_control.GetText().len()) {
								buffer_accum<256>	spec;
								ISO::Browser		v, b	= GetVirtual(edit_item, v, spec);
								auto			buffer	= alloc_auto(char, len + 1);
								edit_control.GetText().get((char*)buffer, len + 1);
								try {
									ISO::Browser		b2 = b.SkipUser();
									if (b2.GetType() == ISO::STRING)
										((ISO::TypeString*)b2.GetTypeDef())->set(b2, buffer);
									else
										ISO::ScriptRead(b, lvalue(memory_reader(buffer)), ISO::SCRIPT_KEEPEXTERNALS | ISO::SCRIPT_DONTCONVERT);
									v.Update(spec);
									treecolumn.Invalidate();
								} catch (const char *s) {
									MessageBoxA(*this, s, "Edit Error", MB_ICONERROR | MB_OK);
								}
							}
							edit_control.Destroy();
							treecolumn.GetTreeControl().Invalidate();
							break;
					}
					break;

   				case ID_EDIT_DELETE: {
					TreeControl	tree	= treecolumn.GetTreeControl();
					if (HTREEITEM h = tree.GetSelectedItem()) {
						buffer_accum<256>	spec;
						ISO::Browser		v;
						HTREEITEM		hp	= tree.GetParentItem(h);
						ISO::Browser2	bp	= GetVirtual(hp, v, spec);
						if (bp.GetTypeDef()->SkipUser()->GetType() == ISO::OPENARRAY) {
							buffer_accum<256>	spec;
							ISO::Browser		v;
							HTREEITEM		hp	= tree.GetParentItem(h);
							ISO::Browser2	bp	= GetVirtual(hp, v, spec);
							int				i	= GetIndex(tree, h);
							bp.Remove(i);
							RemoveTreeItem(tree, bp, hp, i);
							v.Update(spec);
						}
					}
					break;
				}
			}
			break;
		}

		case WM_ISO_SET:
			EditItem((HTREEITEM)wParam);
			return 0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CLICK: {
					if (edit_control.hWnd)
						edit_control.Destroy();
					int	subitem = -1;
					HTREEITEM h = treecolumn.HitTest(treecolumn.ToClient(GetMousePos()), subitem);
					if (subitem == 1)
						PressButton(h);
					else if (subitem == 2)
						PostMessage(WM_ISO_SET, (WPARAM)h);
					return 0;
				}
				case NM_CUSTOMDRAW: {
					NMTCCCUSTOMDRAW	*nm	= (NMTCCCUSTOMDRAW*)lParam;
					return nm->iSubItem == 1
						? DrawButton(nm->nmcd.hdc, nm->rcItem, (HTREEITEM)nm->nmcd.dwItemSpec)
						: 0;
				}
				case TCN_GETDISPINFO: {
					NMTCCDISPINFO	*nmdi	= (NMTCCDISPINFO*)nmh;
					if (nmdi->iSubItem == 2) try {
						memory_writer	m(memory_block(nmdi->item.pszText, nmdi->item.cchTextMax - 1));
						ISO::Browser2	b(GetBrowser(nmdi->item));
						const ISO::Type *t = b.GetTypeDef()->SkipUser();
						if (t->GetType() == ISO::REFERENCE && *(ISO::ptr<void>*)b) {
							b = *b;
							t = b.GetTypeDef()->SkipUser();
							if ((nmdi->item.state & TVIS_EXPANDEDONCE) && t->GetType() == ISO::VIRTUAL && b.Count() == 0) {
								b = *b;
								t = b.GetTypeDef()->SkipUser();
							}
						}
						if (const char *e = b.External()) {
							m.write(e);
						} else if (t->GetType() == ISO::STRING) {
							void	*s = t->ReadPtr(b);
							if (t->flags & ISO::TypeString::UTF16) {
								size_t	size	= chars_count<char>((const char16*)s) + 1;
								void	*buff	= m.get_block(size);
								chars_copy((char*)buff, (const char16*)s, size);
							} else {
								m.write((const char*)s);
							}
						} else {
							ISO::ScriptWriter(m).SetFlags(ISO::SCRIPT_ONLYNAMES).DumpData(b);
						}
						m.putc(0);
					} catch (const char *s) {
						strcpy(nmdi->item.pszText, s);
					}
					return nmdi->iSubItem;
				}
			}
			break;
		}

		case WM_NCDESTROY:
			me = 0;
			delete this;
			return 0;
	}
	return Super::Proc(message, wParam, lParam);
}

void SettingsWindow::EditItem(HTREEITEM h) {
	fixed_string<256> s;
	memory_writer	m(memory_block(s, sizeof(s) - 1));

	ISO::Browser2	b		= GetBrowser(h);
	const ISO::Type*	type	= b.GetTypeDef()->SkipUser();
	switch (type->GetType()) {
		case ISO::STRING: {
			void	*s = type->ReadPtr(b);
			if (type->flags & ISO::TypeString::UTF16)
				m.write(string((const char16*)s));
			else
				m.write((const char*)s);
			break;
		}

		case ISO::INT:
		case ISO::FLOAT: {
			ISO::ScriptWriter	s(m);
			s.DumpData(b);
			break;
		}

		default:
			return;
	}

	s[int(m.tell())] = 0;

	edit_control.Create(treecolumn, NULL, CHILD | VISIBLE | CLIPSIBLINGS | ES_AUTOHSCROLL | ES_WANTRETURN, CLIENTEDGE, treecolumn.GetItemRect(h, 2), ID_EDIT);
	edit_control.SetFont(treecolumn.GetTreeControl().GetFont());
	edit_control.MoveAfter(HWND_TOP);
	edit_control	= s;
	edit_item		= h;
	edit_control.SetSelection(CharRange::all());
	edit_control.SetFocus();
}

int SettingsWindow::DrawButton(DeviceContext dc, const Rect &rect, HTREEITEM h) {
	ISO::Browser2	b		= GetBrowser(h);
	const ISO::Type*	type	= b.GetTypeDef();
	Rect			rect2	= rect.Centre(16, 16);//(rect.Left(), rect.Top(), 0, 0);

	if (type->GetType() == ISO::REFERENCE) {
		b		= *b;
		type	= b.GetTypeDef();
	}
	if (type->Is("rgba8")) {
		dc.Fill(rect, Brush(Colour(*(uint32*)b & 0x00ffffff)));

	} else if (type->Is("path") || type->Is("filename")) {
		dc.Fill(rect, COLOR_BTNFACE);
		dc.DrawIcon(images.GetIcon(2), rect2);

	} else if (type->Is("font")) {
		dc.Fill(rect, COLOR_BTNFACE);
		dc.DrawIcon(images.GetIcon(4), rect2);

	} else {
		type	=  type->SkipUser();
		switch (type->GetType()) {
			case ISO::INT:
				if (type->flags & ISO::TypeInt::ENUM) {
					dc.Fill(rect, COLOR_BTNFACE);
					dc.DrawIcon(images.GetIcon(3), rect2);
				}
				break;
			case ISO::OPENARRAY:
				dc.Fill(rect, COLOR_BTNFACE);
				dc.DrawIcon(images.GetIcon(0), rect2);
				break;
		}
	}
	return CDRF_SKIPDEFAULT;
}

void SettingsWindow::PressButton(HTREEITEM h) {
	buffer_accum<256>	spec;
	ISO::Browser			v;
	ISO::Browser2		b0		= GetVirtual(h, v, spec);
	ISO::Browser2		b		= b0;
	const ISO::Type*		type	= b.GetTypeDef();

	if (type->GetType() == ISO::REFERENCE) {
		b		= *b;
		type	= b.GetTypeDef();
	}

	const ISO::Type*		type2	= type->SkipUser();

	if (type->Is("rgba8")) {
		CHOOSECOLOR	cc;
		COLORREF	custom[16];

		clear(cc);
		cc.lStructSize	= sizeof(cc);
		cc.hwndOwner	= *this;
		cc.lpCustColors	= custom;
		cc.Flags		= CC_RGBINIT;
		cc.rgbResult	= *(COLORREF*)b;

		if (ChooseColor(&cc)) {
			*(COLORREF*)b = cc.rgbResult;
			v.Update(spec);
			treecolumn.Invalidate(treecolumn.GetItemRect(h, -1));
		}

	} else if (type->Is("path")) {
		ISO::TypeString	*s	= (ISO::TypeString*)type2;
		filename	dir		= (const char*)s->ReadPtr(b);
		if (GetDirectory(*this, dir, "Select Directory")) {
//			free(r);
			s->set(b, dir);
			v.Update(spec);
			treecolumn.Invalidate();
		}

	} else if (type->Is("filename")) {
		ISO::TypeString	*s	= (ISO::TypeString*)type2;
		filename		fn	= (const char*)s->ReadPtr(b);
		if (GetOpen(*this, fn, "Select File", "C header files\0*.h\0")) {
//			free(r);
			s->set(b, fn);
			v.Update(spec);
//			treecolumn.Invalidate();
		}

	} else if (type->Is("font")) {
		ISO::TypeString	*s	= (ISO::TypeString*)type2;
		Font::Params		font((const char*)s->ReadPtr(b));
		if (win::GetFont(*this, font)) {
			s->set(b, font.Description());
			v.Update(spec);
		}

	} else {
		type	=  type->SkipUser();
		switch (type->GetType()) {
			case ISO::INT:
				if (type->flags & ISO::TypeInt::ENUM) {
					Menu	context_menu	= Menu::Popup();
					ISO::TypeEnum	&e		= *(ISO::TypeEnum*)type;
					uint32	value			= e.get(b);
					uint32	factors[64], nf = e.factors(factors, 64);

					for (int i = 0, j = 0; !e[i].id.blank(); i++) {
						if (nf > 0 && e[i].value >= factors[j]) {
							context_menu.Separator();
							j++;
						}
						context_menu.Append(e[i].id.get_tag(), e[i].value + 1);
					}

					for (uint32 x = value; x;) {
						auto	i = e.biggest_factor(x);
						context_menu.CheckByID(i->value + 1);
						x -= i->value;
					}

					Rect	rect	= treecolumn.GetItemRect(h, 1);
					int		ret		= context_menu.Track(*this, treecolumn.ToScreen(rect.BottomLeft()), TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON);

					if (ret > 0) {
						ret--;
						if (nf > 0) for (int i = 1; ; i++) {
							if (factors[i] > ret) {
								uint32	prev = value % factors[i] - value % factors[i - 1];
								if (prev == ret)
									ret = 0;
								value = value % factors[i - 1] + value / factors[i] * factors[i] + ret;
								break;
							}
						}
						e.set(b, value);
						v.Update(spec);
						treecolumn.Invalidate(treecolumn.GetItemRect(h, -1));
					}
				}
				break;

			case ISO::OPENARRAY: {
				int		i = b.Count();
				b.Resize(i + 1);
				TreeControl	tree	= treecolumn.GetTreeControl();
				tree.SetSelectedItem(AddTreeItem(tree, h, ItemName(b, i), i, HasChildren(b[i])));
				break;
			}
		}
	}
}

Control Settings(MainWindow &main) {
	if (!SettingsWindow::me)
		new SettingsWindow;
	SetForegroundWindow(*SettingsWindow::me);
	return *SettingsWindow::me;
}
