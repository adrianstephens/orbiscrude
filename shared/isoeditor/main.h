#ifndef MAIN_H
#define MAIN_H

#include "viewers/viewer.h"
#include "filename.h"

#ifdef PLAT_WIN32
#include "windows/treecolumns.h"
#include "windows/control_helpers.h"
#include "resource.h"
#endif

#ifndef PLAT_WINRT
#include "windows/registry.h"
#endif

size_t	get_crc_string(iso::crc32 crc, char *dest, size_t maxlen);

namespace iso {
struct PhysicalMemory : public ISO::VirtualDefaults, memory_block {
	int				Count()					{ return 1; }
	ISO::Browser2	Index(int i)			{
		ISO_ptr<ISO_openarray<uint8> > ptr(0);
		memcpy(ptr->Create(size32(), false), p, size());
		return ptr;
	}
	PhysicalMemory(const memory_block &mb) : memory_block(mb)	{}
};
}

ISO_DEFUSERVIRTX(iso::PhysicalMemory, "BigBin");

namespace app {
using namespace iso;

struct ScaleProgress {
	timer	time;
	float	digits			= 0;
	
	uint64	offset			= 0;
	uint64	total			= 1;

	uint64	update_offset	= 0;
	float	update_time		= 0;
	int64	update_val		= -1;

	ScaleProgress() {}
	ScaleProgress&	operator++()		{ ++offset; return *this; }
	ScaleProgress&	operator=(uint32 n)	{ total *= n; offset *= n; update_offset *= n; return *this; }

	uint32	pos(uint32 range) const {
		return mul_div(offset, range, total);
	}

	bool	ready();
	float	remaining_time() const;

	friend string_accum& operator<<(string_accum &sa, const ScaleProgress &p) {
		uint32	idigits	= uint32(p.digits + 0.5f);
		int64	frac	= pow(int64(10), idigits);
		return sa << p.update_val / frac << '.' << formatted(p.update_val % frac, FORMAT::ZEROES, idigits) << "%";
	}
};

#ifdef PLAT_WIN32
struct HierarchyProgress : win::ProgressTaskBar {
	ScaleProgress	prog;
	const char	*caption	= nullptr;
	HierarchyProgress() {}
	HierarchyProgress(const WindowPos &wpos, const char *caption) : win::ProgressTaskBar(wpos, 100), caption(caption) {}

	void Create(const WindowPos &wpos, const char* _caption, uint32 total);
	void Next();
};
#else
struct HierarchyProgress {
	int	prog;
	HierarchyProgress(const WindowPos &wpos, const char *caption) {}
	void Next() {}
};

#endif

//fixed_string<256>	get_id(tag2 id);
fixed_string<256>	get_id(ISO_ptr<void> p);

void				WriteFiles(const filename &fn, const ISO::Browser2 &b, HierarchyProgress &progress, int depth = 0);
const char *		WriteFilesExt(const ISO::Browser2 &b, tag id);
void				WriteDirectory(const filename &dir, const ISO::Browser2 &b, HierarchyProgress &progress, int depth = 0);

string				GetFileFilter();
ISO::Browser2		GetSettings(const char *path);
Menu				GetTypesMenu(Menu menu, istream_ref file, const char *ext, int id, int did = 0);

ISO::Browser2		VirtualDeref(ISO::Browser2 b);
ISO::Browser2		SkipPointer(ISO::Browser2 b, bool defer = false);
ISO_ptr<anything>	UnVirtualise(const ISO::Browser &b0);
ISO_ptr<void>		RemoteFix(const tag2 &id, const ISO::Type *type, ISO::Browser &v, const char *spec);
void				BrowserAssign(const ISO::Browser &dest, const ISO::Browser &srce);

#ifdef PLAT_PC
FileHandler*		GetFileHandlerFromMenu(filename &fn, HWND hWnd, const Point &pt);
#endif

ISO_ptr<void>		ExpandChild(tag2 name, istream_ref file, int mode);
void				ExpandChildren(const ISO::Browser &b0, string &errors, int mode);
void				Refresh(TreeControl &tree, ISO::Browser2 b, HTREEITEM h);

struct IsoEditorCache {
	ISO::FileHandlerCacheSave	prev_cache;
	map<string,ISO_ptr<void>>	cache;
	ISO_ptr<void>&	operator()(const filename &fn)	{
		return cache[fn];
	}
	const char*		operator()(ISO_ptr<void> p)		{
		for (auto &i : cache.with_keys()) {
			if (i.key() == p)
				return i.value();
		}
		return 0;
	}
	IsoEditorCache() : prev_cache(FileHandler::PushCache(this)) {}
};

//-----------------------------------------------------------------------------
//	Operation
//-----------------------------------------------------------------------------

class Operation : public e_link<Operation> {
public:
	virtual			~Operation()	{}
	virtual	bool	Do()			=0;
	virtual	bool	Undo()			=0;
	virtual	void	Commit()		{}
};

struct CompositeOp : Operation {
	e_list<Operation>	ops;
	bool				undone;
	CompositeOp() : undone(false) {}
	bool Add(Operation *op) {
		if (!op->Do()) {
			delete op;
			return false;
		}
		ops.push_front(op);
		return true;
	}
	bool			Do() {
		if (undone) {
			for (auto &op : ops)
				if (!op.Do())
					return false;
		}
		return true;
	}
	bool			Undo() {
		for (auto &op : reversed(ops))
			if (!op.Undo())
				return false;
		undone = true;
		return true;
	}
};

struct ModifyOp : Operation {
	ISO_ptr<void>	p, p2;
	bool			undone;
	ModifyOp(ISO_ptr<void> _p) : p(_p), undone(false)	{ p2 = Duplicate(p); }
	void			Swap() {
		ISO_ptr<void>	t = Duplicate(p);
		CopyData(p.GetType(), p2, p);
		p2 = t;
	}
	bool			Do()	{ if (undone) Swap(); return true; }
	bool			Undo()	{ Swap(); undone = true; return true; }
};

Operation* MakeModifyDataOp(const ISO::Browser &_b);
Operation* MakeDeleteOp(TreeControl _tree, HTREEITEM _hp, int _index, ISO_VirtualTarget &_v);
template<int B> Operation* MakeModifyPtrOp(TreeControl tree, HTREEITEM h, ISO_ptr<void, B> &p0, ISO_ptr<void, B> &p1);
Operation* MakeModifyValueOp(TreeControl _tree, HTREEITEM _h, ISO::Browser &_b);
Operation* MakeAddEntryOp(TreeControl _tree, HTREEITEM _hParent, const ISO::Browser2 &_b);
Operation* MakeInsertEntryOp(TreeControl _tree, HTREEITEM _hParent, HTREEITEM _hAfter, const ISO::Browser &_a, const ISO_ptr<void> &_b);

//-----------------------------------------------------------------------------
//	route
//-----------------------------------------------------------------------------

struct route : buffer_accum<1024> {
	struct node {
		route		&r;
		char		*p;
		node(route &_r, int i)			: r(_r)	{ p = r.getp(); r << '[' << i << ']'; }
		node(route &_r, const char *s)	: r(_r)	{ p = r.getp(); r << onlyif(p[-1] != ';', '.') << s; }
		~node()									{ r.move(int(p - r.getp())); }
		operator route&()						{ return r; }
		operator string_param()					{ return r; }
	};
	node	add_node(int i)			{ return node(*this, i); }
	node	add_node(const char *s)	{ return node(*this, s); }
	route()							{}
	route(const char *prefix)		{ *this << prefix << ';'; }
};

struct route_ptr {
	const ISO_ptr<void>	&p;
	const char			*r;
	route_ptr(const ISO_ptr<void> &_p, const char *_r) : p(_p), r(_r) {}
};

//-----------------------------------------------------------------------------
//	ISOTree
//-----------------------------------------------------------------------------

struct ISOTree : public TreeControl {
	enum ADD_FLAGS {
		NONE			= 0,
		HAS_CHILDREN	= 1 << 0,
		PRE_EXPAND		= 1 << 1,
	};
	static ADD_FLAGS			GetFlags(const ISO::Browser2 &b);
	static fixed_string<256>	ItemName(const ISO::Browser2 &b, int i);
	static fixed_string<256>	ItemNameI(const ISO::Browser &b, int i);

	ISOTree(const TreeControl &tree) : TreeControl(tree) {}

	int					GetIndex(HTREEITEM hItem);
	void				SetItem(HTREEITEM hItem, const ISO::Browser2 &b, const char *name) const;
	HTREEITEM			AddItem(HTREEITEM hParent, HTREEITEM hAfter, const char *name, int i, ADD_FLAGS flags) const;
	HTREEITEM			AddItem(HTREEITEM hParent, HTREEITEM hAfter, const char *name, int i, ADD_FLAGS flags, ImageListBitmap image) const;
	void				RemoveItem(const ISO::Browser2 &b, HTREEITEM hParent, int index) const;
	HTREEITEM			InsertItem(HTREEITEM hParent, HTREEITEM hAfter, int i, const ISO::Browser2 &b) const;

	int					Setup(const ISO::Browser2 &b, HTREEITEM hParent, int maximum);
	ISO::Browser2		GetChild(ISO::Browser2 &b, HTREEITEM hItem);
	ISO::Browser2		GetBrowser(const ISO::Browser2 &b, HTREEITEM hItem);
#ifdef PLAT_WIN32
	ISO::Browser2		GetBrowser(const ISO::Browser2 &b, TVITEMA &item);
#endif
	ISO_VirtualTarget	GetVirtualTarget(const ISO::Browser2 &b, HTREEITEM hItem);
	HTREEITEM			Locate(HTREEITEM hItem, ISO::Browser2 b, const ISO_ptr<void> &p);
	void				GetRoute(string_accum &spec, HTREEITEM hItem);
};

#ifndef PLAT_WINRT
RegKey	Settings(bool write = false);
#endif

#ifdef PLAT_WIN32
#define	CF_ISOPOD			CF_PRIVATEFIRST
#define	CF_HTML				CF_PRIVATEFIRST+1
#define	CF_AST				CF_PRIVATEFIRST+2
extern UINT	_CF_HTML;

struct TreeColumnDisplay {
	Rect	col0rect;
	int		col0width;
	bool	names;
	TreeColumnDisplay() : col0rect(0,0,0,0), col0width(0), names(true) {}
	bool	Display(const ISO::Browser2 &b0, ISOTree tree, NMTCCDISPINFO *nmdi);
	void	PostDisplay(TreeColumnControl &treecolumn);
	void	Expanding(HWND hWnd, const ISO::Browser2 &b0, ISOTree tree, NMTREEVIEW *nmtv, int max_expand);
};
#endif

//-----------------------------------------------------------------------------
//	IsoEditor
//-----------------------------------------------------------------------------

class IsoEditor : public MainWindow {
protected:
	ISO_ptr<anything>	root_ptr;
	e_list<Operation>	undo, redo;
	int					max_expand;

	ISO::Browser2		GetBrowser(HTREEITEM hItem);
	ISO_VirtualTarget	GetVirtualTarget(HTREEITEM hItem);

#if defined PLAT_PC && !defined PLAT_WINRT
	TreeColumnControl	treecolumn;
	Control				editwindow;
	TabControl2			tabcontrol;
	ISO::Browser2		GetBrowser(TVITEMA &item);
#endif

public:
#ifdef PLAT_WINRT
	TreeControl		tree;
	ISOTree			Tree()	const { return ISOTree(tree); }
#elif defined PLAT_PC
	static IsoEditor*	Cast(Control c);
	ISOTree			Tree()	const { return ISOTree(treecolumn.GetTreeControl()); }
#elif defined PLAT_MAC
	TreeControl		tree;
	OutlineControl	sidebar;
	ISOTree			Tree()	const { return ISOTree(tree); }
	void			Edit(ISO::Browser2 b, Editor *ed = 0);
	void			operator()(TreeMessageExpand &m);
	void			operator()(TreeMessagePreview &m);
	void			operator()(TreeMessageSelect &m);
	void			operator()(ControlMessageDestroy &m);
#endif

	void			SetEditWindow(Control c);

	bool			Do(Operation *op);
	ISO::Browser	GetSelection();
	void			AddRecent(const char *fn);
	void			AddEntry(const ISO_ptr<void> &p, bool test = true);
	void			AddEntry2(ISO_ptr<void> p, bool test = true)	{ return AddEntry(p, test); }
	HTREEITEM		Locate(const ISO_ptr<void> &p);
	bool			Select(const ISO_ptr<void> &p);
	bool			Update(const ISO_ptr<void> &p);
	bool			SelectRoute(const char *route);
	bool			LocateRoute(string_accum &spec, const ISO_ptr<void> &p);
	void			ModalError(const char *s);
	IsoEditor();
};

} //namespace app


#endif	// MAIN_H
