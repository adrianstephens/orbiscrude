#ifndef WINDOW_H
#define WINDOW_H

#include "base.h"
#include "coroutine.h"
#include "Windows.UI.Xaml.Controls.h"

#include "win32_shared.h"

namespace iso { namespace win {

using namespace iso_winrt;
using namespace Windows::UI::Xaml;

struct ID : hstring_ref {
	ID()					: hstring_ref(nullptr)	{}
	ID(const char16 *id)	: hstring_ref(id) {}
	ID(const char *id)		: hstring_ref(str16(id)) {}
};


struct Bitmap : ptr<Controls::BitmapIcon> {
	Bitmap() {}
	Bitmap(ptr<Controls::BitmapIcon> &&p) : ptr<Controls::BitmapIcon>(move(p)) {}
	operator bool()	{ return false; }
};

typedef int	ImageListBitmap;

struct Menu {
	struct Item {};
};

//-----------------------------------------------------------------------------
//	Control
//-----------------------------------------------------------------------------

struct Control : ptr<Controls::Control> {
	Control() {}
	Control(ptr<Controls::Control> &&p) : ptr<Controls::Control>(move(p)) {}
	void	Invalidate() {}
	void	EnableRedraws(bool enable = true)	const	{}
};

typedef virtfunc<void(Control,Menu)>		MenuCallback;
typedef virtfunc<void(Control,Menu::Item)>	MenuItemCallback;

//-----------------------------------------------------------------------------
//	WindowPos
//-----------------------------------------------------------------------------

struct WindowPos : Rect {
	Control    *parent;
	Control    *after;

	WindowPos() { clear(*this); }
	WindowPos(Control *h, const RECT &r) : win::Rect(r) {
		parent	= h;
		after	= 0;
	}
	WindowPos&			Parent(Control *h) {
		parent	= h;
		return *this;
	}
	inline Control		Parent() const;
	WindowPos&		After(Control *h) {
		after	= h;
		return *this;
	}
	inline Control		After() const;
	WindowPos&			Rect(const RECT &r) {
		Rect::operator=(r);
		return *this;
	}
	const win::Rect&	Rect() const {
		return *this;
	}
};

//-----------------------------------------------------------------------------
//	TreeControl
//-----------------------------------------------------------------------------

typedef struct TREEITEM*	HTREEITEM;

extern HTREEITEM TVI_ROOT, TVI_FIRST, TVI_LAST;

struct TreeControl : Control {
	struct ItemData {
		enum StateFlag {
			SELECTED		= 1 << 0,
			BOLD			= 1 << 1,
			EXPANDED		= 1 << 2,
			EXPANDEDONCE	= 1 << 3,
			EXPANDPARTIAL	= 1 << 4,
		};
		const char *text;
		uint32		state;
		uint32		image, image2;
		uint32		children;
		arbitrary	param;
	};
	struct Item;
	HTREEITEM	*root;
	
	TreeControl();
	
	HTREEITEM	GetParentItem(HTREEITEM h);
	HTREEITEM	GetChildItem(HTREEITEM h);
	HTREEITEM	GetNextItem(HTREEITEM h);
	arbitrary	GetItemParam(HTREEITEM h);

	bool		GetItem(HTREEITEM h, ItemData *i);
	bool		SetItem(HTREEITEM h, const ItemData *i);
	bool		GetItem(Item *i);
	bool		SetItem(const Item *i);
	Item		GetItem(HTREEITEM h);
	
	HTREEITEM	Insert(const Item *i, HTREEITEM parent, HTREEITEM after);
	HTREEITEM	InsertItem(const char *name, HTREEITEM parent, HTREEITEM after, arbitrary param, int children);
	bool		DeleteItem(HTREEITEM h);
	bool		SetSelectedItem(HTREEITEM h);
	bool		ExpandItem(HTREEITEM h)								const;
	bool		ExpandedOnce(HTREEITEM h)							const;
	bool		EnsureVisible(HTREEITEM hItem)						const;
	
	HTREEITEM	GetSelectedItem()									const;
	int			FindChildIndex(HTREEITEM hParent, HTREEITEM hChild)	const;
	HTREEITEM	GetChildItem(HTREEITEM hParent, int i)				const;
	
	void		DeleteChildren(HTREEITEM hItem);
};

struct TreeControl::Item : TreeControl::ItemData {
	HTREEITEM	hItem;
	
				Item(const char *s)					{ text = s;					}
				Item(HTREEITEM h, uint32 _mask = 0)	{ hItem = h;				}
	Item&		Handle(HTREEITEM h)					{ hItem = h; return *this;	}
	Item&		Text(const char *s)					{ text = s; return *this;	}
	template<int N> Item& Text(fixed_string<N> &s)	{ return Text(s, N);		}
	Item&		Param(const arbitrary &p)			{ param = p; return *this;	}
	Item&		Image(int i)						{ image = i; return *this;	}
	Item&		Image2(int i)						{ image2 = i; return *this;	}
	Item&		Children(int i)						{ children = i; return *this;				}
	Item&		ChangeState(uint32 v, uint32 m)		{ state = (state & ~m) | v; return *this;	}
	Item&		SetState(int i)						{ return ChangeState(i, i);					}
	Item&		ToggleState(int i)					{ return ChangeState(~state & i, i);		}
	Item&		SetState(int i, bool enable)		{ return ChangeState(enable ? i : 0, i);	}
	Item&		ClearState(int i)					{ return ChangeState(0, i);					}
	Item&		Bold(bool enable = true)			{ return SetState(BOLD, enable);			}
	Item&		Expand(bool enable = true)			{ return SetState(EXPANDED, enable);		}

	HTREEITEM	Handle()				const		{ return hItem;		}
	const char*	Text()					const		{ return text;		}
	arbitrary	Param()					const		{ return param;		}
	int			Image()					const		{ return image;		}
	int			Image2()				const		{ return image2;		}
	int			Children()				const		{ return children;	}
	uint32		State()					const		{ return state;		}

	bool		_Get(TreeControl tree)				{ return tree.GetItem(this);	}
	Item&		Get(TreeControl tree)				{ _Get(tree); return *this;	}
	bool		Set(TreeControl tree)	const		{ return tree.SetItem(this);	}
	HTREEITEM	Insert(TreeControl &tree, HTREEITEM hItemParent = TVI_ROOT, HTREEITEM hItemAfter = TVI_LAST)	{ return tree.Insert(this, hItemParent, hItemAfter); }
};


//-----------------------------------------------------------------------------
//	Timer
//-----------------------------------------------------------------------------

class Timer : callback<void(Timer*)> {
public:
	template<typename T> Timer(T *_me) : callback(_me)	{}
	void	Init(float t) {
	}
	void	Next(float t) {
	}
	void	Stop()				{}
	bool	IsRunning()			{ return false; }
};

struct Busy {};

} } // namespace iso::win

#endif	// WINDOW_H
