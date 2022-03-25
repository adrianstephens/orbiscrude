#if USE_TWEAKS

#include "tweak.h"
#include "iso/iso.h"
#include "base/algorithm.h"

using namespace iso;

//-----------------------------------------------------------------------------
// SubMenu
//-----------------------------------------------------------------------------
class SubMenu : dynamic_array<tweak*>, public ISO::VirtualDefaults {
	const char *name;
	struct cmp { bool operator()(tweak *a, tweak *b) { return istr(a->GetItemName()) < b->GetItemName(); } };

	// ISO::Virtual adapter
public:
	int			Count()					{ return size32();	}
	tag			GetName(int i) const	{ return (*this)[i]->GetItemName();}
	ISO::Browser	Index(int i) {
		tweak	*t = (*this)[i];
		void	*p = t->GetPtr();
		if (p) switch (t->GetType()) {
			case tw_bool:	return ISO::MakeBrowser(*(bool8*)p);
			case tw_int:	return ISO::MakeBrowser(*(int*)p);
			case tw_float:	return ISO::MakeBrowser(*(float*)p);
			case tw_float3:	return ISO::Browser(ISO::getdef<float[3]>(),	p);
		}
		return ISO::Browser();
	}
	int			Find(tag id) {
		for (int i = 0, n = size32(); i < n; i++) {
			if ((*this)[i]->GetItemName() == id)
				return i;
		}
		return -1;
	}

public:
	SubMenu(const char *_name) : name(_name) {}

	tag		GetName() const		{ return name; }
	void	Add(tweak *t)	{
		iterator at = lower_bound(begin(), end(), t, cmp());
		insert(at, t);
	}
};

ISO_DEFVIRT(SubMenu);

//-----------------------------------------------------------------------------
// TweakMenu
//-----------------------------------------------------------------------------
class TweakMenu : public singleton<TweakMenu>, public ISO::VirtualDefaults {
	dynamic_array<SubMenu*> menus;
	struct cmp { bool operator()(SubMenu *a, SubMenu *b) { return istr(a->GetName()) < b->GetName(); } };

	// ISO::Virtual adapter
public:
	int				Count()					{ return menus.size32(); }
	tag				GetName(int i)			{ return menus[i]->GetName(); }
	ISO::Browser	Index(int i)			{ return ISO::MakeBrowser(*menus[i]); }
	int				Find(tag id) {
		for (int i = 0, n = menus.size32(); i < n; i++) {
			if (menus[i]->GetName() == id)
				return i;
		}
		return -1;
	}

public:
	TweakMenu();

	TweakMenu	*operator&()					{ return this; }
	void		Add(tweak *tweak, const char *name);
};

ISO_DEFVIRT(TweakMenu);

TweakMenu::TweakMenu() {
	ISO::root().Add(MakePtr("tweaks", this));
}

void TweakMenu::Add(tweak *tweak, const char *name) {
	SubMenu *menu;
	int i = Find(name);
	if (i < 0) {
		menu = new SubMenu(iso::strdup(name));
		dynamic_array<SubMenu*>::iterator at = lower_bound(menus.begin(), menus.end(), menu, cmp());
		menus.insert(at, menu);
	} else {
		menu = menus[i];
	}
	menu->Add(tweak);
}

//-----------------------------------------------------------------------------
//	tweak
//-----------------------------------------------------------------------------
tweak::tweak(tweak_spec *_spec, const char *_name, const char *_comment) : comment(_comment), spec(_spec) {
	char	buffer[64], *d = buffer;
	bool	upper = true;

	name	= 0;
	for (const char *s = _name; *s; s++) {
		if (*s == '_') {
			upper = true;
			if (!name) {
				*d++ = 0;
				name = d;
			} else {
				*d++ = ' ';
			}
		} else {
			*d++ = upper ? *s : to_lower(*s);
			upper = false;
		}
	}
	*d = 0;
	name = name ? iso::strdup(name) : "no name";

	TweakMenu::single().Add(this, buffer);
}

#endif // USE_TWEAKS
