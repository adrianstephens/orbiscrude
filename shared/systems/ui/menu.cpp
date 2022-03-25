#include "graphics.h"
#include "font.h"
#include "menu_structs.h"
#include "postprocess/post.h"
#include "scenegraph.h"
#include "font_iso.h"

namespace iso {
#include "menu.fx.h"
}

using namespace iso;

class MenuSystem : public Handles2<MenuSystem, AppEvent> {
	ISO_ptr<fx>	iso_fx;
public:
	layout_menu *shaders;

	void	operator()(AppEvent *ev) {
		unused(ISO::getdef<Font>());
		if (ev->state == AppEvent::BEGIN)
			shaders	= (iso_fx = ISO::root("data")["menu"]) ? (layout_menu*)(ISO_ptr<technique>*)*iso_fx : 0;
	}
} menu_system;

float	fontparams::LineSpacing() const { return font->spacing * (line + 1) * scale;		}
float	fontparams::ParaSpacing() const { return font->spacing * (paragraph + 1) * scale;	}

MenuRect GetTextBox(const char *text, uint32 flags, fontparams *fp, float w, float h) {
	ISO_ASSERT(fp);
	Font	*font	= fp->font;
	float	w1		= w / fp->scale;
	float	h1		= h / fp->scale;

	FontMeasurer	fontprinter(font);
	fontprinter.SetAlign(FontAlign(flags & 3)).SetLine(fp->line + 1).SetParagraph(fp->paragraph + 1);

	if (flags & TF_WRAP)
		fontprinter.SetWidth(w1);

	float2	ext = fontprinter.CalcRect(text);
	if (!(flags & (TF_NOSQUISH_H|TF_WRAP|TF_CLIP_H)))
		fontprinter.SetWidth(w1);

	float	x = 0, y = 0, yscale = fp->scale, xscale = fp->scale;

	if (ext.x > w1 && !(flags & TF_NOSQUISH_H)) {
		xscale = w / ext.x;
		if (flags & TF_WRAP) {
			fontprinter.SetWidth(ext.x);
			ext = fontprinter.CalcRect(text);
		}
	} else switch (flags & 3) {
		case FA_RIGHT:	x = w1 - ext.x;			break;
		case FA_CENTRE:	x = (w1 - ext.x) / 2;	break;
		case FA_JUSTIFY: ext.x = w1;			break;
	}

	if (ext.y > h1 && !(flags & TF_NOSQUISH_V)) {
		yscale	= h / ext.y;
	} else switch ((flags >> 2) & 3) {
		case FA_RIGHT:	y = h1 - ext.y;		break;
		case FA_CENTRE:	y = (h1 - ext.y) / 2;	break;
		case FA_JUSTIFY: ext.y = h1;			break;
	}

	return MenuRect(x * xscale, y * yscale, (x + ext.x) * xscale, (y + ext.y) * yscale);
}

void SetTechniquePass(GraphicsContext &ctx, technique *technique, uint32 pass, const ISO::Browser &parameters = ISO::Browser()) {
	ISO_ASSERT(technique && *technique);
	pass &= 0x7f;
	Set(ctx, (*technique)[pass < technique->Count() ? pass : 0], parameters);
}

//-----------------------------------------------------------------------------
//	type helpers
//-----------------------------------------------------------------------------

class InstanceData : public linear_allocator {
public:
	InstanceData(void *_p) : linear_allocator(_p)			{}
	InstanceData(uint32 s) : linear_allocator(iso::malloc(s))	{}
	template<typename T, typename N>T*	alloc2(N n)	{ return (T*)alloc(T::calc_size(n), alignof(T));	}
	template<typename T>	operator T*()			{ return (T*)getp(); }
};

class CheckedInstanceData : public InstanceData {
	void	*end;
public:
	CheckedInstanceData(uint32 s)			: InstanceData(s)	{ end = (char*)p + s; }
	CheckedInstanceData(void *p, uint32 s)	: InstanceData(p)	{ end = (char*)p + s; }
	~CheckedInstanceData() {
		ISO_ASSERT(p <= end);
	}
};

class mi_calls {
public:
	void	(*vReserve)			(const void *p, MenuInstance *mi, InstanceData &d);
	void	(*vCreate)			(const void *p, MenuInstance *mi, InstanceData &d);
	void	(*vDestroy)			(const void *p, MenuInstance *mi, InstanceData d);
	bool	(*vCanBeSelected)	(const void *p, MenuInstance *mi, InstanceData d);
	mreturn (*vUpdate)			(const void *p, MenuInstance *mi, InstanceData d);
	void	(*vDraw)			(const void *p, MenuInstance *mi, InstanceData d, RenderRegion *rr, int state);
};

class mi_type : public mi_calls, public ISO::TypeUserSave {
public:
	mi_type(tag _id) : ISO::TypeUserSave(_id, NULL)	{ flags |= TYPE_FIXED; }
};

template<class T> class mi_template : public mi_type {
	static void	Reserve			(const void *p, MenuInstance *mi, InstanceData &d)								{ ((T*)p)->Reserve(mi, d);				}
	static void	Create			(const void *p, MenuInstance *mi, InstanceData &d)								{ ((T*)p)->Create(mi, d);				}
	static void	Destroy			(const void *p, MenuInstance *mi, InstanceData d)								{ ((T*)p)->Destroy(mi, d);				}
	static bool	CanBeSelected	(const void *p, MenuInstance *mi, InstanceData d)								{ return ((T*)p)->CanBeSelected(mi, d);	}
	static mreturn Update		(const void *p, MenuInstance *mi, InstanceData d)								{ return ((T*)p)->Update(mi, d);		}
	static void	Draw			(const void *p, MenuInstance *mi, InstanceData d, RenderRegion *rr, int state)	{ ((T*)p)->Draw(mi, d, rr, state);		}
public:
	mi_template(tag _id) : mi_type(_id)	{
		vReserve		= Reserve;
		vCreate			= Create;
		vDestroy		= Destroy;
		vCanBeSelected	= CanBeSelected;
		vUpdate			= Update;
		vDraw			= Draw;
	}
};

inline const mi_type* mi_gettype(const ISO_ptr<void> &p) {
	return static_cast<const mi_type*>(p.GetType());
}

uint32 ReserveInstanceData(MenuInstance *mi, const mitem &i) {
	if (i) {
		InstanceData	d((void*)0);
		mi_gettype(i)->vReserve(i, mi, d);
		return uint32(uintptr_t(d.getp()));
	}
	return 0;
};
void CreateInstanceData(void *&start, MenuInstance *mi, const mitem &i) {
	if (i) {
		uint32	size	= ReserveInstanceData(mi, i);
		void	*p		= start = iso::malloc(size);
		InstanceData	d(p);
		mi_gettype(i)->vCreate(i, mi, d);
		ISO_ASSERT(d - (uint8*)p <= size);
	}
}
template<typename T> uint32 ReserveInstanceData(MenuInstance *mi, const T *t) {
	InstanceData	d((void*)0);
	t->Reserve(mi, d);
	return uint32(uintptr_t(d.getp()));
};
template<typename P, typename T> void CreateInstanceData(P *&start, MenuInstance *mi, const T *t) {
	uint32	size	= ReserveInstanceData(mi, t);
	void	*p		= iso::malloc(size);
	start			= (P*)p;
	InstanceData	d(p);
	t->Create(mi, d);
	ISO_ASSERT((uint8*)d - (uint8*)p <= size);
}

//-----------------------------------------------------------------------------
//	RenderRegionValue
//-----------------------------------------------------------------------------

struct RenderRegionValue {
	crc32			id;
	const ISO::Type	*type;
	void			*data;
	RenderRegionValue(crc32 _id, const ISO::Type *_type, void *_data) : id(_id), type(_type), data(_data)	{}
	RenderRegionValue(crc32 _id, const ISO_ptr<void> &p) : id(_id), type(p.GetType()), data(p)					{}
	RenderRegionValue(const ISO_ptr<void> &p) : id(p.ID().get_crc32()), type(p.GetType()), data(p)					{}
	ISO::Browser 	Lookup(const RenderRegion *rr) {
		if (rr) {
			if (type == ISO::getdef<REGION_PARAM>()) {
				REGION_PARAM	x = *(REGION_PARAM*)data;
				return x < _REGION_WRITEONLY ? ISO::MakeBrowser(((float*)rr)[x]) : ISO::Browser();//ISO::MakeBrowser(rr->GetField(x));
			} else if (type == ISO::getdef<REGION_PARAM2>()) {
				return ISO::Browser(ISO::getdef<float[2]>(), ((float*)rr) + *(int*)data);
			} else if (type == ISO::getdef<REGION_PARAM4>()) {
				return ISO::Browser(ISO::getdef<float[4]>(), ((float*)rr) + *(int*)data);
			}
		}
		return ISO::Browser(type, data);
	}
};

struct RenderRegionValuesNode {
	uint32					count;
	RenderRegionValue		*values;
	RenderRegion			*rr;
	RenderRegionValuesNode	*next;
	RenderRegionValuesNode	**pnext;

	void insert(RenderRegionValuesNode *&first) {
		next	= first;
		first	= this;
		pnext	= &first;
	}
	void remove() {
		*pnext	= next;
	}
};

struct RenderRegionValuesNode1 : RenderRegionValuesNode {
	void init(const anything &a) {
		for (int i = 0; i < count; i++)
			new(values + i) RenderRegionValue(a[i]);
	}
	RenderRegionValuesNode1(MenuInstance *mi, InstanceData &d, uint32 n, RenderRegion *_rr = 0) {
		count	= n;
		values	= d.alloc<RenderRegionValue>(n);
		rr		= _rr;
		insert(*(RenderRegionValuesNode**)&mi->values);
	}
	~RenderRegionValuesNode1() {
		remove();
	}
};

struct RenderRegionValues0 : RenderRegionValuesNode, ISO::VirtualDefaults {
	uint32		Count()			{ return count;	}
	crc32		GetName(int i)	{ return i < count ? values[i].id : crc32(); }
	ISO::Browser	Index(int i)	{ return i < count ? values[i].Lookup(rr) : ISO::Browser(); }
	int			GetIndex(crc32 id, int from) {
		for (int i = from; i < count; i++) {
			if (values[i].id == id)
				return i;
		}
		return -1;
	}
};

struct RenderRegionValues : RenderRegionValuesNode, ISO::VirtualDefaults {
	uint32		Count() {
		uint32 t = 0;
		for (RenderRegionValuesNode *n = this; n; n = n->next)
			t += n->count;
		return t;
	}
	crc32		GetName(int i) {
		for (RenderRegionValuesNode *n = this; n; n = n->next) {
			if (i < n->count)
				return n->values[i].id;
			i -= n->count;
		}
		return crc32();
	}
	ISO::Browser	Index(int i) {
		for (RenderRegionValuesNode *n = this; n; n = n->next) {
			if (i < n->count)
				return n->values[i].Lookup(rr);
			i -= n->count;
		}
		return ISO::Browser();
	}
	int			GetIndex(crc32 id, int from) {
		uint32 t = 0;
		for (RenderRegionValuesNode *n = this; n; n = n->next) {
			for (int i = from; i < n->count; i++) {
				if (n->values[i].id == id)
					return t + i;
			}
			from	= max(from - n->count, 0);
			t		+= n->count;
		}
		return -1;
	}
};

//MenuValue::MenuValue(MenuInstance *mi, ISO_ptr<anything> &a) {
//	insert(*(RenderRegionValuesNode**)&mi->values, a->Count(), RenderRegionValue *_values);
//}

void MenuValue::init(MenuInstance *mi, tag2 id, const ISO::Type *type, void *data, RenderRegion *rr) {
	linear_allocator		a(dummy);
	RenderRegionValuesNode	*node	= new(a) RenderRegionValuesNode;
	RenderRegionValue		*value	= new(a) RenderRegionValue(id, type, data);
	ISO_ASSERT(a.getp() < this + 1);
	node->count		= 1;
	node->values	= value;
	node->rr		= rr;
	node->insert(*(RenderRegionValuesNode**)&mi->values);
}

MenuValue::~MenuValue() {
	RenderRegionValuesNode	*node	= (RenderRegionValuesNode*)dummy;
	node->remove();
}

//-----------------------------------------------------------------------------
//	menu_item
//-----------------------------------------------------------------------------

struct menu_item {
	static void			Reserve			(MenuInstance *mi, InstanceData &d)	{}
	static void			Create			(MenuInstance *mi, InstanceData &d)	{}
	static void			Destroy			(MenuInstance *mi, InstanceData &d)	{}
	static bool			CanBeSelected	(MenuInstance *mi, InstanceData &d)	{ return false;		}
	static mreturn		Update			(MenuInstance *mi, void *my)		{ return mi->SendUpdate(0, NULL); }

	static inline void	mi_Reserve		(const mitem &p, MenuInstance *mi, InstanceData &d)								{ if (p) mi_gettype(p)->vReserve(p, mi, d);				}
	static inline void	mi_Create		(const mitem &p, MenuInstance *mi, InstanceData &d)								{ if (p) mi_gettype(p)->vCreate(p, mi, d);				}
	static inline void	mi_Destroy		(const mitem &p, MenuInstance *mi, InstanceData d)								{ if (p) mi_gettype(p)->vDestroy(p, mi, d);				}
	static inline bool	mi_CanBeSelected(const mitem &p, MenuInstance *mi, InstanceData d)								{ return p && mi_gettype(p)->vCanBeSelected(p, mi, d);	}
	static inline mreturn mi_Update	(const mitem &p, MenuInstance *mi, InstanceData d)								{ return p ? mi_gettype(p)->vUpdate(p, mi, d) : Update(mi, NULL);	}
	static inline void	mi_Draw			(const mitem &p, MenuInstance *mi, InstanceData d, RenderRegion *rr, int state)	{ if (p) mi_gettype(p)->vDraw(p, mi, d, rr, state);		}
};

//-----------------------------------------------------------------------------
//	menu
//-----------------------------------------------------------------------------

enum MENU_STATE {
	MS_NORMAL			= 0,
	MS_BACK				= 1, // has been returned to
	MS_NORMAL_TRANS		= 2, // returning to MS_NORMAL
	MS_BACK_TRANS		= 3, // returning to MS_BACK
	MS_COVERED			= 4,
	MS_TRANS_OUT		= 5,
	MS_DIE				= 6,
};

struct menu : _menu, menu_item {
	bool	is_popup() const	{ return flags.test(POPUP);		}

	void	Reserve(MenuInstance *mi, InstanceData &d)			const;
	void	Create(MenuInstance *mi, InstanceData &d)			const;
	void	Destroy(MenuInstance *mi, InstanceData d)			const;
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const;
	mreturn Update(MenuInstance *mi, InstanceData d)			const;
	void	Draw(MenuInstance *mi, InstanceData d, RenderRegion *rr, int state) const;
};

struct menu_data {
	const menu	*m;
	MENU_STATE	state;
	menu_data	*next;
	menu_data	*prev;

	menu_data(const menu *_m, menu_data *_prev) : m(_m), state(MS_NORMAL), next(0), prev(_prev) {}

	void		killchild(MenuInstance *mi);
	void		addchild(MenuInstance *mi, const menu *submenu, MENU_STATE newstate);
	bool		pop(MenuInstance *mi, bool imm);

	menu_data*	last_root() {
		menu_data *m = this, *p;
		do {
			p = m;
			m = m->prev;
		} while (m && m->next != p);
		return p;
	}

	bool		returning()		const		{ return state == MS_NORMAL_TRANS || state == MS_BACK_TRANS; }
	bool		returned()		const		{ return state == MS_BACK || state == MS_BACK_TRANS; }
	bool		quitting()		const		{ return state == MS_TRANS_OUT;	}
	bool		is_popup()		const		{ return m->is_popup();			}

	void		Destroy(MenuInstance *mi)	{ m->Destroy(mi, this);			}
	mreturn	Update(MenuInstance *mi)	{ return m->Update(mi, this);	}
	void		Draw(MenuInstance *mi, RenderRegion *rr, int state) { m->Draw(mi, this, rr, state); }
};

void menu_data::killchild(MenuInstance *mi) {
	if (menu_data *d2 = next) {
		if (!d2->m->is_popup())
			state = state == MS_NORMAL_TRANS ? MS_NORMAL : MS_BACK;
		next	= 0;
		d2->Destroy(mi);
		iso::free(d2);
	}
}

void menu_data::addchild(MenuInstance *mi, const menu *submenu, MENU_STATE newstate) {
	if (next)
		killchild(mi);
	if (!submenu->is_popup())
		state = newstate;

	save(mi->currmenu, this), CreateInstanceData(next, mi, submenu);
}

bool menu_data::pop(MenuInstance *mi, bool imm) {
	state = MS_TRANS_OUT;

	imm = imm || !m->flags.test(menu::TRANS_OUT);
	if (prev) {
		if (imm)
			prev->killchild(mi);
		else
			prev->state	= m->is_popup() && (prev->state == MS_NORMAL || prev->state == MS_NORMAL_TRANS) ? MS_NORMAL_TRANS : MS_BACK_TRANS;
		return true;
	}
	return !imm;
}

void menu::Reserve(MenuInstance *mi, InstanceData &d) const {
	menu_data	*my	= d;
	mi_Reserve(item, mi, d);
}

void menu::Create(MenuInstance *mi, InstanceData &d) const {
	menu_data	*my	= new(d) menu_data(this, mi->currmenu);
	if (mi->currmenu == mi->topmenu)
		mi->topmenu = my;

	save(mi->currmenu, my),
	save(mi->index, 0),
	mi_Create(item, mi, d);
}

void menu::Destroy(MenuInstance *mi, InstanceData d) const {
	menu_data	*my	= d;

	if (menu_data *d2 = my->next) {
		my->next	= 0;
		d2->Destroy(mi);
		iso::free(d2);
	}

	mi_Destroy(item, mi, d);

	if (mi->topmenu == my)
		mi->topmenu = my->prev;
}

bool menu::CanBeSelected(MenuInstance *mi, InstanceData &d) const {
	menu_data	*my	= d;
	return true;
}

mreturn menu::Update(MenuInstance *mi, InstanceData d) const {
	menu_data	*my	= d;

	if (my->quitting())
		return MIU_DEFAULT;

	else if (my->state == MS_DIE)
		return MIU_COMPLETEDALL;

	saver<menu_data*>	savethis(mi->currmenu, my);
	mi->active		= MenuInstance::DEFAULT;
	mreturn	ret		= (save(mi->index, 0),
		my->next && !my->next->quitting()
		? my->next->Update(mi)
		: mi_Update(item, mi, d)
	);

	switch (ret) {
		case MIU_CANCEL:
			if (my->prev && my->prev->next != my)
				break;
			if (!flags.test(NO_CANCEL)) {
				mi->SendEvent(0, MMSG_NOTIFY, ret);
				my->pop(mi, false);
			}
			return MIU_DEFAULT;

		case MIU_CANCELALL:
			if (my->prev && my->prev->next != my)
				break;
			if (flags.test(NO_CANCELALL))
				return MIU_DEFAULT;
			mi->SendEvent(0, MMSG_NOTIFY, ret);
			my->pop(mi, false);
			break;
	}
	return ret;
}

void menu::Draw(MenuInstance *mi, InstanceData d, RenderRegion *rr, int state) const {
	menu_data	*my	= d;

	saver<int>			saveindex(mi->index, 0);
	saver<menu_data*>	savethis(mi->currmenu, my);

	mi->active	= MenuInstance::DEFAULT;
	int		sel = (flags.test(INACTIVE_SELECTED) ? MenuInstance::SELECTED : 0);

	if (menu_data *child = my->next) {
		if ((child->m->flags.test(DRAW_UNDER)) && my->state != MS_COVERED) {
			mi_Draw(item, mi, d, rr, my->quitting()
				? sel | MenuInstance::BACK
				: (child->quitting()
					? MenuInstance::SELECTED | MenuInstance::TO | MenuInstance::ACTIVE
					: (child->m->is_popup()
						? MenuInstance::TO
						: 0
					) | sel
				) | (my->returned() ? MenuInstance::BACK : 0)
			);
		}
		child->Draw(mi, rr, (state & MenuInstance::SELECTED)
			? MenuInstance::SELECTED | MenuInstance::TO | MenuInstance::ACTIVE
			: state
		);
	} else {
		mi_Draw(item, mi, d, rr, sel | (my->quitting()
			? MenuInstance::BACK
			: state | (my->returned() ? MenuInstance::BACK : 0)
		));
	}

	if (my->state == MS_DIE && my->prev)
		my->prev->killchild(mi);
}

//-----------------------------------------------------------------------------
//	mi_list / mi_arrange
//-----------------------------------------------------------------------------

struct mi_list : _mi_list, menu_item {
	struct instance : trailing_array<instance, void*> {
		int		active;
	};

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		int			n	= Count();
		instance	*my = d.alloc2<instance>(n);
		for (int i = 0; i < n; i++)
			mi_Reserve((*this)[i], mi, d);
	}

	void	Create(MenuInstance *mi, InstanceData &d) const {
		Create(mi, d, -1);
	}

	void	Destroy(MenuInstance *mi, instance *my) const {
		for (int i = 0, n = Count(); i < n; i++)
			mi_Destroy((*this)[i], mi, (*my)[i]);
	}

	bool	Valid(instance *my) const {
		return my->active >= 0 && my->active < Count();
	}

	bool	CanBeSelected(MenuInstance *mi, instance *my) const {
		if (my->active != -2 && (!Valid(my) || mi_CanBeSelected((*this)[my->active], mi, (*my)[my->active])))
			return true;
		for (int i = 0, n = Count(); i < n; i++) {
			if (mi_CanBeSelected((*this)[i], mi, (*my)[i]))
				return true;
		}
		return false;
	}

	mreturn Update(MenuInstance *mi, instance *my) const {
		return Update(mi, my, 1);
	}

	void	Draw(MenuInstance *mi, instance *my, RenderRegion *rr, int state) const {
		bool	selectable = CanBeSelected(mi, my);
		if (my->active != MenuInstance::DESELECT && (state & 1) && selectable && !Valid(my))
			Next(mi, my, 0);

		int	pointed = -1;
		for (int i = 0, n = Count(); i < n; i++) {
			mi->active = MenuInstance::DEFAULT;
			int	item_state	= state & ~int(selectable && i != my->active);
			mi_Draw((*this)[i], mi, (*my)[i], rr, item_state);
			if (mi->active == MenuInstance::ME) {
				my->active = i;
				pointed = mi->active;
			} else if ((item_state & 1) && mi->active == MenuInstance::DESELECT) {
				if (my->active == -2)
					pointed = mi->active;
				else
					my->active = -3;
			}
		}
		mi->active = pointed;
	}

	void	Create(MenuInstance *mi, InstanceData &d, int offset) const {
		int			n	= Count();
		instance	*my = d.alloc2<instance>(n);
		bool		selectable	= false;
		int			selectme	= -1;
		saver<int>	saveindex(mi->index);

		if (offset >= 0 && mi->active - offset >= 0 && mi->active - offset < n) {
			selectme	= mi->active - offset;
//			my->active	= mi->active - offset;
			mi->active	= MenuInstance::DEFAULT;
		} else {
			my->active	= -1;
		}

		for (int i = 0; i < n; i++) {
			if (offset >= 0)
				mi->index = i + offset;

			(*my)[i] = d.getp();
			if (mi->active >= 0) {
				mi_Create((*this)[i], mi, d);
				if (mi->active == MenuInstance::DEFAULT)
					my->active = i;
			} else {
				mi->active = MenuInstance::DEFAULT;
				mi_Create((*this)[i], mi, d);
				if (mi->active == MenuInstance::ME) {
					selectme	= i;
				} else if (mi->active == MenuInstance::DESELECT) {
					my->active = -3;
				}
			}

			selectable = selectable || mi_CanBeSelected((*this)[i], mi, (*my)[i]);
		}
		if (!selectable)
			my->active = -2;
		else if (selectme >= 0)
			my->active = selectme;

		if (selectme >= 0)
			mi->active = MenuInstance::ME;
	}

	mreturn Update(MenuInstance *mi, instance *my, flags<ARRANGE_FLAGS> flags) const {
//		saver<int>	saveindex(mi->index);

		mreturn ret = MIU_DEFAULT;
		if (flags.test(ARR_INDEX) && mi->active >= 0) {
			int	t = mi->active - flags / ARR_START;
			if (t < 0)
				ret = MIU_PREV;
			else if (t >= Count())
				ret = MIU_NEXT;
			else {
//			if (t >= 0 && t < Count()) {
				my->active = mi->active;
				return MIU_DEFAULT;
			}
		}

		if (ret == MIU_DEFAULT) {
			if (Valid(my)) {
				if (flags.test(ARR_INDEX))
					mi->index = my->active + flags / ARR_START;
				ret = mi_Update((*this)[my->active], mi, (*my)[my->active]);
			} else {
				ret = menu_item::Update(mi, NULL);
			}
		}
		int		forwards0	= flags.test(ARR_REVERSE_DIR) ^ flags.test(ARR_REVERSE_ORDER) ? -1 : 1;
		int		forwards	= 0;
		bool	changed		= false;
		switch (ret) {
			case MIU_HITDOWN:
				if (flags.test(ARR_VERTICAL))
					changed = Next(mi, my, forwards = forwards0);
				break;
			case MIU_HITUP:
				if (flags.test(ARR_VERTICAL))
					changed = Next(mi, my, forwards = -forwards0);
				break;
			case MIU_HITRIGHT:
				if (!flags.test(ARR_VERTICAL))
					changed = Next(mi, my, forwards = forwards0);
				break;
			case MIU_HITLEFT:
				if (!flags.test(ARR_VERTICAL))
					changed = Next(mi, my, forwards = -forwards0);
				break;
			case MIU_NEXT:
				changed = Next(mi, my, forwards = 1);
				break;
			case MIU_PREV:
				changed = Next(mi, my, forwards = -1);
				break;
			case MIU_DESELECT:
				my->active = -3;
				break;
		}
		if (changed) {
			if (mi_Update((*this)[my->active], mi, (*my)[my->active]))
				mi->SendEvent(0, MMSG_NOTIFY, ret);
			ret = MIU_NOTDEFAULT;
		} else if (forwards && flags.test(forwards > 0 ? ARR_NO_WRAP1 : ARR_NO_WRAP0)) {
			if (Next(mi, my, -forwards)) {
//				SendEvent(0, MMSG_NOTIFY, (void*)ret);
				mi_Update((*this)[my->active], mi, (*my)[my->active]);
				ret = MIU_NOTDEFAULT;
			}
		}
		if (flags.test(ARR_INDEX))
			mi->active = my->active + flags / ARR_START;
		return ret;
	}

	bool	Next(MenuInstance *mi, instance *my, int incr) const {
		int		count	= Count();
		int		i		= max(my->active, -1);

		if (incr == 0)
			incr = i < 0 ? 1 : -1;

		if (i < 0 || i >= count)
			i = incr > 0 ? -1 : count;

		for (;;) {
			i += incr;
			if (i < 0) {
				if (my->active > -2)
					my->active = count;
				return false;
			} else if (i >= count) {
				if (my->active > -2)
					my->active = -1;
				return false;
			}

			if (mi_CanBeSelected((*this)[i], mi, (*my)[i])) {
				my->active = i;
				return true;	// resend to new item
			}
		}
	}
};

struct mi_arrange : _mi_arrange, menu_item {
	bool	CanBeSelected(MenuInstance *mi, mi_list::instance *my)	const { return ((mi_list&)list).CanBeSelected(mi, my);	}
	void	Reserve(MenuInstance *mi, InstanceData &d)				const { ((mi_list&)list).Reserve(mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)				const { ((mi_list&)list).Create(mi, d, flags.test(ARR_INDEX) ? flags / ARR_START : -1);	}

	void	Destroy(MenuInstance *mi, mi_list::instance *my) const {
		if (flags.test(ARR_INDEX)) {
			saver<int> saveindex(mi->index);
			for (int i = 0, n = list.Count(); i < n; i++) {
				mi->index = i + flags / ARR_START;
				mi_Destroy(list[i], mi, (*my)[i]);
			}
		} else
			((mi_list&)list).Destroy(mi, my);
	}

	mreturn Update(MenuInstance *mi, mi_list::instance *my) const {
		if (flags.test(ARR_SEPARATE_INPUTS)) {
			saver<int>	saveindex(mi->index);
			mreturn	retall	= MIU_DEFAULT;
			for (int i = 0, n = list.Count(); i < n; i++) {
				if (flags.test(ARR_INDEX))
					mi->index = i + flags / ARR_START;
				mreturn	ret = mi_Update(list[i], mi, (*my)[i]);
				switch (ret) {
					case MIU_DEFAULT:
						break;
					case MIU_HITUP: case MIU_HITDOWN: case MIU_HITLEFT: case MIU_HITRIGHT:
						mi_Update(list[i], mi, (*my)[i]);
						break;
					case MIU_ABORTUPDATE:
						return ret;
					default:
						if (!retall) {
							retall		= ret;
							my->active	= i;
						}
						break;
				}
			}
			return retall;

		} else if (flags.test(ARR_SELECT_ALL)) {
			saver<int>	saveindex(mi->index);
			mreturn	retall	= MIU_DEFAULT;
			for (int i = 0, n = list.Count(); i < n; i++) {
				if (flags.test(ARR_INDEX))
					mi->index = i + flags / ARR_START;
				mreturn	ret = mi_Update(list[i], mi, (*my)[i]);
				if (i == 0) {
					retall = ret;
				} else if (ret != retall) {
					if (ret > MIU_DEFAULT && ret <= MIU_COMPLETEDALL)
						retall = ret;
					else if (retall > MIU_COMPLETEDALL)
						retall = MIU_DEFAULT;
				}
				if (ret == MIU_ABORTUPDATE)
					return MIU_DEFAULT;
//				if (ret >= MIU_HOLDUP && ret <= MIU_PREV)
//					ret = mi_Update(list[i], (*my)[i]);
			}
			return retall;
		} else {
			return ((mi_list&)list).Update(mi, my, flags);
		}
	}

	void	Draw(MenuInstance *mi, mi_list::instance *my, RenderRegion *rr, int state) const {
		bool	selected = !!(state & 1);
		if (my->active != -3 && selected && !((mi_list&)list).Valid(my))
			((mi_list&)list).Next(mi, my, 0);

		saver<int>		saveindex(mi->index);
		RenderRegion	rr2			= *rr;
		int				count		= list.Count();
		int				size_count	= sizes.Count();
		float			size_scale	= 1;
		float			off			= rr->offset[flags.test(ARR_VERTICAL)];
		float			w			= rr->size[flags.test(ARR_VERTICAL)];
		float			t			= 0;
		int				pointed		= -1;

		if (flags.test(ARR_PROPORTIONS)) {
			float	t = count - size_count;
			for (int i = 0; i < size_count; i++)
				t += sizes[i];
			size_scale = w / t;
		}

		for (int i = 0; i < count; i++) {
			int		i2	= flags.test(ARR_REVERSE_ORDER) ? count - 1 - i : i;
			float	v	= i < size_count ? sizes[i] * size_scale : 0;
			if (v < 0)
				v = max(w - t + v, zero);
			else if (v > 0)
				v = min(v, w - t);
			else if (flags.test(ARR_NO_SUBBOXING))
				v = w - t;
			else
				v = (w - t) / (count - i);

			if (flags.test(ARR_INDEX))
				mi->index = i2 + flags / ARR_START;

			float	off2	= 0;
			float	t2		= flags.test(ARR_REVERSE_DIR) ? w - t - v + off : t + off;

			if (t2 < 0) {
				off2 = t2;
				t2	 = 0;
			} else if (t2 + v > w) {
				off2 = t2 + v - w;
				t2	-= off2;
			}

			if (flags.test(ARR_VERTICAL)) {
				rr2.size.y		= v;
				rr2.offset.y	= off2;
				rr2.matrix		= rr->matrix * translate(0, t2, 0);
			} else {
				rr2.size.x		= v;
				rr2.offset.x	= off2;
				rr2.matrix		= rr->matrix * translate(t2, 0, 0);
			}
//			bool	sel = selected && (flags.test_any(ARR_SEPARATE_INPUTS | ARR_SELECT_ALL)
//				? mi_CanBeSelected(list[i2], (*my)[i2])
//				: i == my->active || my->active == -2
//			);
			bool	sel = selected && (flags.test_any(ARR_SEPARATE_INPUTS | ARR_SELECT_ALL) || i == my->active /*|| my->active == -2*/ );	// commented out the == -2 because transitions into the menu would makee all things selected in an arrange during the transition.  (might break something).
//			bool	sel = selected && (i == my->active || my->active == -2);
			mi->active	= MenuInstance::DEFAULT;
			mi_Draw(list[i2], mi, (*my)[i2], &rr2, state & ~int(!sel));
			if (mi->active == MenuInstance::ME) {
				my->active = i;
				pointed = mi->active;
			} else if (sel && mi->active == MenuInstance::DESELECT) {
				if (my->active == -2)
					pointed = mi->active;
				else
					my->active = -3;
			}

			if (flags.test(ARR_INHERIT_START)) {
				if (flags.test(ARR_REVERSE_DIR))
					;//t = flags.test(ARR_VERTICAL) ? rr2.y : rr2.x;
				else
					t += rr2.size[flags.test(ARR_VERTICAL)];
			} else if (!flags.test(ARR_NO_SUBBOXING)) {
				t += v;
			}
		}
		mi->active	= pointed;
	}
};

//-----------------------------------------------------------------------------
//	mi_offset
//-----------------------------------------------------------------------------

struct mi_offset : _mi_offset, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { mi_Reserve(item, mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)			const { mi_Create(item, mi, d);					}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { mi_Destroy(item, mi, d);				}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return mi_Update(item, mi, d);			}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return mi_CanBeSelected(item, mi, d);	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		RenderRegion	rr2 = *rr;
		float			s	= iso::abs(scle);
		if (s) {
			rr2.matrix		= rr->matrix * (translate(x, y, 0) * scale(scle, s, one));
			rr2.size		/= s;
			rr2.offset		/= s;
		} else {
			rr2.offset		= rr2.offset + float2{x, y};
		}
		mi_Draw(item, mi, d, &rr2, state);
#ifdef ISO_EDITOR
		mi->SendEvent(0, MMSG_EDITOR, MenuEditor(this, d, rr, &rr2).me());
#endif
	}
};

//-----------------------------------------------------------------------------
//	mi_box
//-----------------------------------------------------------------------------

struct mi_box : _mi_box, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { mi_Reserve(item, mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)			const { mi_Create(item, mi, d);					}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { mi_Destroy(item, mi, d);				}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return mi_Update(item, mi, d);			}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return mi_CanBeSelected(item, mi, d);	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		RenderRegion	rr2 = *rr;
		if (scale == 0) {
			rr2.matrix		= rr->matrix * translate(x, y, 0);
			rr2.size.x		= w;
			rr2.size.y		= h;
			mi_Draw(item, mi, d, &rr2, state);

		} else {
			float	newx	= x >= 0 ? x : max(rr->size.x + x, 0.f);
			float	newy	= y >= 0 ? y : max(rr->size.y + y, 0.f);
			float	maxw	= rr->size.x - newx;
			float	maxh	= rr->size.y - newy;
			float	sx		= scale, sy = iso::abs(sx);
			float	offx, offy;
			float	neww	= w > 0 ? min(w, maxw - rr->offset.x) : maxw + w;
			float	newh	= h > 0 ? min(h, maxh - rr->offset.y) : maxh + h;
			newx += rr->offset.x;
			newy += rr->offset.y;
			if (newx < 0) {
				offx = newx;
				newx = 0;
				neww = min(neww, rr->size.x);
			} else if (newx + neww > rr->size.x) {
				offx = newx + neww - rr->size.x;
//				newx = rr->w - neww;
				newx-= offx;
			} else {
				offx = 0;
			}
			if (newy < 0) {
				offy = newy;
				newy = 0;
				newh = min(newh, rr->size.y);
			} else if (newy + newh > rr->size.y) {
				offy = newy + newh - rr->size.y;
//				newy = rr->h - newh;
				newy-= offy;
			} else {
				offy = 0;
			}
			rr2.size.x		= neww / sy;
			rr2.size.y		= newh / sy;
			rr2.offset.x	= offx / sy;
			rr2.offset.y	= offy / sy;
			if (sx < 0)
				newx += neww;

			rr2.matrix		= rr->matrix * (translate(newx, newy, 0) * iso::scale(sx, sy, one));
			mi_Draw(item, mi, d, &rr2, state);
		}

#ifdef ISO_EDITOR
		mi->SendEvent(0, MMSG_EDITOR, MenuEditor(this, d, rr, &rr2).me());
#endif
	}
};

//-----------------------------------------------------------------------------
//	mi_textbox
//-----------------------------------------------------------------------------
struct mi_textbox : _mi_textbox, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { mi_Reserve(item, mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)			const { mi_Create(item, mi, d);					}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { mi_Destroy(item, mi, d);				}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return mi_Update(item, mi, d);			}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return mi_CanBeSelected(item, mi, d);	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		if (!mi->text)
			return;

		if (!rr->fp)
			iso_throw("No font set");

		float			w		= rr->size.x - x;
		float			h		= rr->size.y - y;
		MenuRect		rect	= GetTextBox(mi->text, flags, rr->fp, w, h);

		float			x0		= rect.left();
		float			y0		= rect.top();
		float			x1		= rect.right();
		float			y1		= rect.bottom();

		if (flags & TF_CLIP_H) {
			x0		= max(x0, zero);
			x1		= min(x1, w);
		}
		if (flags & TF_CLIP_V) {
			y0		= max(y0, zero);
			y1		= min(y1, h);
		}

		x1 += x;
		y1 += y;

		if (flags & TF_PASS_ON) {
			rr->matrix		= rr->matrix * translate(x0, y0, 0);
			rr->size.x		= x1 - x0;
			rr->size.y		= y1 - y0;
			mi_Draw(item, mi, d, rr, state);
		} else {
			RenderRegion	rr2		= *rr;

			float	offx	= 0, offy = 0;
			float	w		= x1 - x0, h = y1 - y0;
			x0 += rr->offset.x;
			y0 += rr->offset.y;
			if (flags & TF_CLIP_H) {
				if (x0 < 0) {
					offx = x0;
					x0 = 0;
					w = min(w, rr->size.x);
				} else if (x0 + w > rr->size.x) {
					offx = x0 + w - rr->size.x;
					x0 -= offx;
				}
			}
			if (flags & TF_CLIP_V) {
				if (y0 < 0) {
					offy = y0;
					y0 = 0;
					h = min(h, rr->size.y);
				} else if (y0 + h > rr->size.y) {
					offy = y0 + h - rr->size.y;
					y0 -= offy;
				}
			}

			rr2.matrix		= rr->matrix * translate(x0, y0, 0);
			rr2.size.x		= w;
			rr2.size.y		= h;
			rr2.offset.x	= offx;
			rr2.offset.y	= offy;
			mi_Draw(item, mi, d, &rr2, state);
		}
	}
};

//-----------------------------------------------------------------------------
//	mi_centre
//-----------------------------------------------------------------------------
struct mi_centre : _mi_centre, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { mi_Reserve(item, mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)			const { mi_Create(item, mi, d);					}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { mi_Destroy(item, mi, d);				}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return mi_Update(item, mi, d);			}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return mi_CanBeSelected(item, mi, d);	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		RenderRegion	rr2 = *rr;
		if (w)
			rr2.size.x = w < 0 ? rr->size.x - w * 2 : min(rr->size.x, w);
		if (h)
			rr2.size.y = h < 0 ? rr->size.y - h * 2 : min(rr->size.y, h);
		rr2.matrix			= rr->matrix * translate((rr->size.x - rr2.size.x) / 2, (rr->size.y - rr2.size.y) / 2, 0);
		mi_Draw(item, mi, d, &rr2, state);
#ifdef ISO_EDITOR
		mi->SendEvent(0, MMSG_EDITOR, MenuEditor(this, d, rr, &rr2).me());
#endif
	}
};

//-----------------------------------------------------------------------------
//	mi_text
//-----------------------------------------------------------------------------
struct mi_text : _mi_text, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { if (item) mi_Reserve(item, mi, d);	}
	void	Create(MenuInstance *mi, InstanceData &d)			const { if (item) mi_Create(item, mi, d);	}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { if (item) mi_Destroy(item, mi, d);	}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return item ? mi_Update(item, mi, d) : menu_item::Update(mi, d);	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return item && mi_CanBeSelected(item, mi, d);	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		if (item) {
			save(mi->text, text), mi_Draw(item, mi, d, rr, state);
		} else if (text) {
			rr->DrawText(text, TF_WRAP);
		}
	}
};

//-----------------------------------------------------------------------------
//	mi_gettext
//-----------------------------------------------------------------------------
struct mi_gettext : _mi_gettext, menu_item {
	typedef char*	instance;

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		if (item)
			mi_Reserve(item, mi, d);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		*my	= (char*)mi->SendEvent(funcid, MMSG_GETSTRING, NULL);
		if (item)
			mi_Create(item, mi, d);
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		if (item)
			mi_Destroy(item, mi, d);
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		return item ? mi_Update(item, mi, d) : menu_item::Update(mi, d);
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		return item && mi_CanBeSelected(item, mi, d);
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance	*my(d);
		char		*p	= *my;
		char		buffer[1024];
		if (!p) {
			if (mreturn r = mi->SendEvent(funcid, MMSG_GETSTRING, buffer))
				p = r < sizeof(buffer) ? buffer : (char*)r;
		}
		if (p) {
			if (item)
				save(mi->text, (const char*)p), mi_Draw(item, mi, d, rr, state);
			else
				rr->DrawText(p, TF_WRAP);
		}
	}
};

//-----------------------------------------------------------------------------
//	mi_int
//-----------------------------------------------------------------------------
MenuIntVals	*MenuIntVals::current;

bool MenuIntVals::SetVal(int v) {
	if (flags & NOWRAP) {
		if (v < minval) {
			*pval = minval;
			return false;
		} else if (v > maxval) {
			*pval = maxval;
			return false;
		}
		*pval = v;
	} else {
		int	numvals = maxval - minval + iso::abs(inc);
		*pval = ((v - minval + numvals) % numvals) + minval;
	}
	return true;
}

struct mi_int : _mi_int, menu_item {
	struct instance : MenuIntVals {
		float	lasthittime;
		float	lastupdatetime;
		int		value;
		instance(MenuInstance *mi) : MenuIntVals(&value) {
			lasthittime = lastupdatetime = mi->GetTime();
		}
	};

	mreturn	Changed(MenuInstance *mi, instance *my, mreturn ret) const {
		mi->SendEvent(funcid, MMSG_NOTIFY, ret);
		return my->flags & MenuIntVals::NOTIFY_CHANGED ? mi->SendEvent(funcid, MMSG_CHANGED, my) : MIU_DEFAULT;
	}

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		if (item)
			mi_Reserve(item, mi, d);
	}

	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance	*my = new(d) instance(mi);
		saver<MenuIntVals*>	save_miv(MenuIntVals::current, my);

		mi->SendEvent(funcid, MMSG_GETINTPARAMS, my);
		if (item) {
			if (my->flags & MenuIntVals::ALLOWSET)
				mi->active = *my->pval;
			mi_Create(item, mi, d);
		}
		*my->pval	= clamp(*my->pval, my->minval, my->maxval);
	}

	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		saver<MenuIntVals*>	save(MenuIntVals::current, my);
		if (item)
			mi_Destroy(item, mi, d);
	}

	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		return true;
	}

	mreturn Update(MenuInstance *mi, InstanceData &d)	const {
		instance	*my(d);
		saver<MenuIntVals*>	save_miv(MenuIntVals::current, my);

		mreturn	ret;
		if (my->flags & MenuIntVals::CONTROL) {
			ret = mi->SendUpdate(funcid, my);
		} else {
			if (my->flags & MenuIntVals::ALLOWSET) {
				mi->active	= MenuInstance::DEFAULT;
				ret = mi_Update(item, mi, d);
				if (mi->active >= 0) {
					if (*my->pval != mi->active) {
						*my->pval = mi->active;
						if (mreturn ret2 = Changed(mi, my, ret))
							ret = ret2;
					}
					return ret;
				}
			} else {
				ret = mi_Update(item, mi, d);
			}
			switch (ret) {
				case MIU_NOTDEFAULT:
				case MIU_TRIGGER: case MIU_CANCEL:
				case MIU_DEC: case MIU_INC: case MIU_HOLDDEC: case MIU_HOLDINC:
					break;
				default:
					ret = mi->SendUpdate(funcid, my);
			}
		}

		float	ctime		= mi->GetTime();
		float	updatedelay = (my->flags & MenuIntVals::FAST) || (ctime - my->lasthittime > 2.0f && (my->maxval - my->minval) > 20) ? .05f : 0.25f;

		for (bool loop = true; loop;) switch (ret) {
			case MIU_DEC:
				if ((loop = my->Down())) {
					my->lasthittime = my->lastupdatetime = ctime;
					ret = Changed(mi, my, ret);
				}
				break;
			case MIU_INC:
				if ((loop = my->Up())) {
					my->lasthittime = my->lastupdatetime = ctime;
					ret = Changed(mi, my, ret);
				}
				break;
			case MIU_HOLDDEC:
				if ((loop = !(my->flags & MenuIntVals::NOHOLD) && ctime - my->lastupdatetime >= updatedelay && my->Down())) {
					my->lastupdatetime += updatedelay;
					ret = Changed(mi, my, ret);
				}
				break;
			case MIU_HOLDINC:
				if ((loop = !(my->flags & MenuIntVals::NOHOLD) && ctime - my->lastupdatetime >= updatedelay && my->Up())) {
					my->lastupdatetime += updatedelay;
					ret = Changed(mi, my, ret);
				}
				break;
			case MIU_TRIGGER:	return mi->SendEvent(funcid, MMSG_TRIGGER,	my->pval);
			case MIU_TRIGGER2:	return mi->SendEvent(funcid, MMSG_TRIGGER2, my->pval);
			case MIU_TRIGGER3:	return mi->SendEvent(funcid, MMSG_TRIGGER3, my->pval);
			case MIU_CANCEL:	return mi->SendEvent(funcid, MMSG_CANCEL,	my->pval);
			default:
				my->lastupdatetime = 1e38f;
				return ret;
		}
		return ret;
//		return my->flags & MenuIntVals::CONTROL ? (save(mi->active, *my->pval), mi_Update(item, mi, d)) : MIU_DEFAULT;
	}

	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance	*my(d);
		saver<MenuIntVals*>	save_miv(MenuIntVals::current, my);
		char		buffer[256];
		if (my->format) {
			_format(buffer, sizeof(buffer), my->format, my->Value());
		} else if ((!(my->flags & MenuIntVals::INDEX) || (saver<int>(mi->index, my->Value()), true)) && !mi->SendEvent(funcid, MMSG_GETSTRING, buffer)) {
			if (item)
				mi_Draw(item, mi, d, rr, state);
			return;
		}

		if (item) {
			save(mi->text, (const char*)buffer), mi_Draw(item, mi, d, rr, state);
		} else {
			rr->DrawText(buffer);
		}
	}
};

//-----------------------------------------------------------------------------
//	mi_colour
//-----------------------------------------------------------------------------
struct mi_colour : _mi_colour, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { mi_Reserve(item, mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)			const { mi_Create(item, mi, d);					}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { mi_Destroy(item, mi, d);				}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return mi_Update(item, mi, d);			}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return mi_CanBeSelected(item, mi, d);	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		colour	src((rgba8&)col);
		int		i	= flags & F_OUTLINE;

		if (flags & F_COPY_ALPHA)
			src.a = rr->cols[1 - i].a;
		else if (src.a == 0)
			src.a = flags & F_MULTIPLY ? 1.f : rr->cols[i].a;
		if (flags & F_MULTIPLY) {
			float4	mul	= one - abs(src.rgba - one);
			float4	add	= max(src.rgba - one, zero);
			src = colour(mul * rr->cols[i].rgba + add);
		}

		save(rr->cols[i], src), mi_Draw(item, mi, d, rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_font
//-----------------------------------------------------------------------------
struct mi_font : _mi_font, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { mi_Reserve(item, mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)			const { mi_Create(item, mi, d);					}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { mi_Destroy(item, mi, d);				}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return mi_Update(item, mi, d);			}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return mi_CanBeSelected(item, mi, d);	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		save(rr->fp, (fontparams*)p), mi_Draw(item, mi, d, rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_print
//-----------------------------------------------------------------------------
struct mi_print : _mi_print, menu_item {
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state)	const {
		if (mi->text)
			rr->DrawText(mi->text, flags);
	}
};

//-----------------------------------------------------------------------------
//	mi_fill
//-----------------------------------------------------------------------------
struct mi_fill : _mi_fill, menu_item {
	void Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state)	const {
		rr->Fill(rr->cols[0]);
	}
};

//-----------------------------------------------------------------------------
//	mi_shader
//-----------------------------------------------------------------------------
struct mi_shader : _mi_shader, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const	{ d.alloc<RenderRegionValue>(params.Count()); mi_Reserve(item, mi, d);			}
	void	Create(MenuInstance *mi, InstanceData &d)			const	{ RenderRegionValuesNode1(mi, d, params.Count()).init(params), mi_Create(item, mi, d);				}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const	{ d.alloc<RenderRegionValue>(params.Count()); mi_Destroy(item, mi, d);			}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const	{ d.alloc<RenderRegionValue>(params.Count()); return mi_Update(item, mi, d);		}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const	{ d.alloc<RenderRegionValue>(params.Count()); return mi_CanBeSelected(item, mi, d); }

	void Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state)	{
//		RenderRegionValues			values(params, rr);
		saver<iso::technique*>		_technique(rr->technique, technique);
//		saver<RenderRegionValues*>	_values(rr->values, &values);
		RenderRegionValuesNode1(mi, d, params.Count(), rr), mi_Draw(item, mi, d, rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_texture
//-----------------------------------------------------------------------------
struct mi_texture : _mi_texture, menu_item {
	inline void Draw1(const Texture &tex2, RenderRegion *rr, const ISO::Browser &b) {
		switch (rr->pass) {
			case TB_MULTIPLY:
				rr->ctx.SetBlend(BLENDOP_ADD, BLEND_DST_COLOR, BLEND_INV_SRC_ALPHA);
				rr->Draw(tex2, rr->technique, b, align);
				rr->ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
				break;
			case TB_DISABLE:
				rr->ctx.SetBlendEnable(false);
				rr->Draw(tex2, rr->technique, b, align);
				rr->ctx.SetBlendEnable(true);
				break;
			default:
				rr->Draw(tex2, rr->technique, b, align);
				break;
		}
	}
	void Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state)	{
		if (const Texture *ptex = tex ? (const Texture*)&tex : mi->texture) {
			if (uint32 blend = align / _TAL_BLEND)
				save(rr->pass, blend), Draw1(*ptex, rr, mi->GetBrowser());
			else
				Draw1(*ptex, rr, mi->GetBrowser());
		}
	}
};

//-----------------------------------------------------------------------------
//	mi_button
//-----------------------------------------------------------------------------
struct mi_button : _mi_button, menu_item {
	void Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) {
		const Texture *ptex = tex ? *(iso_ptr32<Texture>*)tex : mi->texture;
		if (ptex && rr->size.y && rr->size.x) {
			float4x4	worldViewProj = hardware_fix(rr->matrix);
			AddShaderParameter(ISO_CRC("worldViewProj", 0xb3f90db0),	worldViewProj);
			AddShaderParameter(ISO_CRC("diffuse_samp", 0xe31becbe),		*ptex);
			SetTechniquePass(rr->ctx, rr->technique, rr->pass, mi->GetBrowser());

			float2	c	= to<float>(ptex->Size()) / 3;

			// uniform scale
			if (rr->size.y < c.y * 2 || rr->size.x < c.x * 2) {
				float s = min(rr->size.y / (c.y * 2), rr->size.x / (c.x * 2));
				c = max(c * s, one);
			}
			// quads
			ImmediateStream<PostEffects::vertex_tex_col> ims(rr->ctx, prim<QuadListT>(), verts<QuadListT>(9));
			PostEffects	post(rr->ctx);
			PostEffects::vertex_tex_col	*p = ims.begin();
			float2 v[] = { float2(zero), c,				float2(rr->size - c),	float2(rr->size) };
			float2 m[] = { float2(zero), float2(third),	float2(2 / 3.0f),		float2(one) };
			for (int ny = 0; ny < 3; ++ny) {
				for (int nx = 0; nx < 3; ++nx) {
					p = post.PutQuad(p,
						float2{v[nx].x, v[ny].y}, float2{v[nx + 1].x, v[ny + 1].y},
						float2{m[nx].x, m[ny].y}, float2{m[nx + 1].x, m[ny + 1].y},
						rr->cols[0]
					);
				}
			}
		}
	}
};

//-----------------------------------------------------------------------------
//	mi_trigger
//-----------------------------------------------------------------------------
struct mi_trigger : _mi_trigger, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		mi_Reserve(item, mi, d);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		mi_Create(item, mi, d);
		if (funcid)
			mi->SendEvent(funcid, MMSG_TRIGGERINIT, NULL);
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		if (funcid)
			mi->SendEvent(funcid, MMSG_TRIGGEREXIT, NULL);
		mi_Destroy(item, mi, d);
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		return item;
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		mi_Draw(item, mi, d, rr, state);
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		mreturn ret = funcid ? mi->SendEvent(funcid, MMSG_TRIGGERUPDATE, NULL) : mreturn();
		if (!ret)
			ret = item ? mi_Update(item, mi, d) : menu_item::Update(mi, NULL);
		switch (ret) {
			case MIU_TRIGGER:	if (funcid) return mi->SendEvent(funcid, MMSG_TRIGGER,	NULL);
			case MIU_TRIGGER2:	if (funcid) return mi->SendEvent(funcid, MMSG_TRIGGER2, NULL);
			case MIU_TRIGGER3:	if (funcid) return mi->SendEvent(funcid, MMSG_TRIGGER3, NULL);
			case MIU_CANCEL:	if (funcid) return mi->SendEvent(funcid, MMSG_CANCEL,	NULL);
		}
		return ret;
	}
};

//-----------------------------------------------------------------------------
//	mi_trigger_menu
//-----------------------------------------------------------------------------
struct mi_trigger_menu : _mi_trigger_menu, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		mi_Reserve(item, mi, d);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		if (item)
			mi_Create(item, mi, d);
		else
			mi->PushMenu(triggermenu, back, false);
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		mi_Destroy(item, mi, d);
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		return item;
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		mi_Draw(item, mi, d, rr, state);
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		mreturn ret = item ? mi_Update(item, mi, d) : MIU_DEFAULT;
		if (ret == MIU_TRIGGER) {
			mi->SendEvent(0, MMSG_NOTIFY, ret);
//			ret	= mi->currmenu == mi->topmenu ? MIU_ABORTUPDATE : MIU_CANCEL;
			mi->PushMenu(triggermenu, back, false);
			ret = MIU_ABORTUPDATE;
		}
		return ret;
	}
};

//-----------------------------------------------------------------------------
//	mi_sellist
//-----------------------------------------------------------------------------
struct mi_sellist : _mi_sellist, menu_item {
	struct instance : MenuIndexVals {
		int		lastindex;
		uint32	size;
		instance(uint32 _size) : lastindex(-1), size(_size) {}
	};

	inline mitem Sub(const instance *my) const {
		return (my->lastindex >= 0 && my->lastindex < list->Count()) ? (*list)[my->lastindex]
			:  ISO_NULL;
	}


	void	Create(MenuInstance *mi, InstanceData &d) const {
		uint32	size = 0;
		for (int i = 0, n = list->Count(); i < n; i++)
			size = max(size, ReserveInstanceData(mi, (*list)[i]));

		instance	*my	= new(d) instance(size);
		d.alloc(my->size);
		if (funcid)
			mi->SendEvent(funcid, MMSG_GETINDEXPARAMS, my);
	}

	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		mi_Destroy(Sub(my), mi, d.alloc(my->size));
	}

	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		d.alloc(my->size);
		return true;
	}

	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		void		*data	= d.alloc(my->size);
		mreturn	ret		= mi->SendUpdate(funcid, NULL);

		if (my->index || !funcid) {
			int	count	= list->Count();
			int	index	= my->lastindex;

			if (!funcid) {
				index	= clamp(index, 0, count - 1);
				if (ret == MIU_HITLEFT)
					ret = MIU_DEC;
				else if (ret == MIU_HITRIGHT)
					ret = MIU_INC;
			}

			mitem	sub	= Sub(my);
			switch (ret) {
				case MIU_DEC:
					index -= 2;
				case MIU_INC:
					index++;
					index = my->flags & instance::NOWRAP ? clamp(index, 0,  count - 1) : (index + count) % count;
					if (index != my->lastindex) {
						mi_Destroy(sub, mi, data);
						if (my->index)
							*my->index = index;
						my->lastindex = mi->active;
						if (sub = Sub(my)) {
							CheckedInstanceData	cid(data, my->size);
							mi_Create(sub, mi, cid);
						}
						if (funcid)
							mi->SendEvent(funcid, MMSG_NOTIFY, ret);
						mi->active = index;
					}
					return MIU_DEFAULT;

				case MIU_TRIGGER:
					return mi->SendEvent(funcid, MMSG_TRIGGER, mreturn(index));
			}
		}
		return ret;
	}

	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance	*my(d);
		void		*data	= d.alloc(my->size);
		mitem		sub		= Sub(my);
		int			index	= my->lastindex;

		if (my->index && index != *my->index) {
			mi_Destroy(sub, mi, data);
			index = *my->index;
		} else if (!funcid && index < 0) {
			index = 0;
		}
		if (index != my->lastindex) {
			my->lastindex = index;
			if (sub = Sub(my)) {
				CheckedInstanceData	cid(data, my->size);
				mi_Create(sub, mi, cid);
			}
		}
		mi_Draw(sub, mi, data, rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_indexed
//-----------------------------------------------------------------------------
struct mi_indexed : _mi_indexed, menu_item {
	struct instance : MenuIndexVals {
		int		lastindex;
		uint32	size;
		instance(uint32 _size) : size(_size) {}
	};

	inline mitem Sub(const instance *my) const {
		return (my->flags & MenuIndexVals::CYCLE)						? (*list)[wrap(my->lastindex, list->Count())]
			:  (my->lastindex >= 0 && my->lastindex < list->Count())	? (*list)[my->lastindex]
			:  ISO_NULL;
	}

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance *my	= d;
		uint32	size	= 0;
		for (int i = 0, n = list->Count(); i < n; i++)
			size = max(size, ReserveInstanceData(mi, (*list)[i]));
		d.alloc(size);
	}

	void	Create(MenuInstance *mi, InstanceData &d) const {
		uint32	size	= 0;
		for (int i = 0, n = list->Count(); i < n; i++)
			size = max(size, ReserveInstanceData(mi, (*list)[i]));

		instance	*my		= new(d) instance(size);
		void		*data	= d.alloc(my->size);

		if (funcid) {
			if (mi->SendEvent(funcid, MMSG_GETINDEXPARAMS, (MenuIndexVals*)my))
				my->lastindex = my->flags & MenuIndexVals::FIXED ? my->value : *my->index;
			else
				my->lastindex = mi->SendEvent(funcid, MMSG_GETINDEXPARAMS, NULL);
		}

		CheckedInstanceData	cid(data, size);
		mi_Create(Sub(my), mi, cid);
	}

	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		mi_Destroy(Sub(my), mi, d.alloc(my->size));
	}

	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		return mi_CanBeSelected(Sub(my), mi, d.alloc(my->size));
	}

	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		void		*data	= d.alloc(my->size);
		mitem		sub		= Sub(my);
		if (!funcid && mi->active >= 0 && my->lastindex != mi->active) {
			mi_Destroy(sub, mi, data);
			my->lastindex = mi->active;
			if (sub = Sub(my)) {
				CheckedInstanceData	cid(data, my->size);
				mi_Create(sub, mi, cid);
			}
		}
		return sub ? mi_Update(sub, mi, data) : mreturn();
	}

	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance	*my(d);
		void		*data	= d.alloc(my->size);
		mitem		sub		= Sub(my);

		if (!(my->flags & MenuIndexVals::FIXED)) {
			int	index = my->index						? *my->index
				: funcid								? (int)mi->SendEvent(funcid, MMSG_GETINDEXPARAMS, NULL, state)
				: mi->active == MenuInstance::DESELECT	? -1
				: mi->active >= 0						? mi->active
				: my->lastindex;

			if (my->lastindex != index) {
				mi_Destroy(sub, mi, data);
				my->lastindex = index;
				if (sub = Sub(my)) {
					CheckedInstanceData	cid(data, my->size);
					mi_Create(sub, mi, cid);
				}
			}
		}
		mi_Draw(sub, mi, data, rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_ifselected
//-----------------------------------------------------------------------------
struct mi_ifselected : _mi_ifselected, menu_item {
	struct instance {
		uint8	prev_selected;
		uint32	size;
		instance(uint32 _size) : prev_selected(-1), size(_size) {}
	};

	const mitem&	Item(uint8 selected) const { return selected == uint8(-1) ? init : selected ? yes : no; }

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		d.alloc(max(max(ReserveInstanceData(mi, yes), ReserveInstanceData(mi, no)), ReserveInstanceData(mi, init)));
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance	*my	= new(d) instance(
			max(max(ReserveInstanceData(mi, yes), ReserveInstanceData(mi, no)), ReserveInstanceData(mi, init))
		);

		void		*data = d.alloc(my->size);
		if (init) {
			CheckedInstanceData	cid(data, my->size);
			mi_Create(init, mi, cid);
		}
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		void		*data = d.alloc(my->size);
		mi_Destroy(Item(my->prev_selected), mi, data);
	}
//	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
//		return true;
//	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		void		*data = d.alloc(my->size);
		return yes && my->prev_selected == 1 ? mi_Update(yes, mi, data) : menu_item::Update(mi, NULL);
	}

	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance	*my(d);
		void		*data		= d.alloc(my->size);
		uint8		selected	= state & 1;
		if ((selected || my->prev_selected != uint8(-1)) && selected != my->prev_selected) {
			mi_Destroy(Item(my->prev_selected), mi, data);
			my->prev_selected = selected;
			if (mitem item = selected ? yes : no) {
				CheckedInstanceData	cid(data, my->size);
				mi_Create(item, mi, cid);
			}
		}
		mi_Draw(Item(my->prev_selected), mi, data, rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_state
//-----------------------------------------------------------------------------
struct mi_state : _mi_state, menu_item {
	struct instance : malloc_block {
		int		prev_state;
		instance(uint32 _size) : malloc_block(_size), prev_state(1) {}
	};

	const mitem& GetItem(int state) const { return (&trans_from)[state]; }

	void	Reserve(MenuInstance *mi, instance *my) const {
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance	*my	= new(d) instance(
			max(
				max(ReserveInstanceData(mi, trans_from), ReserveInstanceData(mi, trans_to)),
				max(ReserveInstanceData(mi, back_from), ReserveInstanceData(mi, back_to))
			)
		);

		if (trans_to) {
			CheckedInstanceData	cid(*my, my->size32());
			mi_Create(trans_to, mi, cid);
		}
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		mi_Destroy(GetItem(my->prev_state), mi, (void*)*my);
		my->~instance();
	}
	bool	CanBeSelected(MenuInstance *mi, instance *my) const {
		return mi_CanBeSelected(GetItem(my->prev_state), mi, (void*)*my);
	}
	mreturn Update(MenuInstance *mi, instance *my) const {
		const mitem	&m = GetItem(my->prev_state);
		return m ? mi_Update(m, mi, (void*)*my) : menu_item::Update(mi, NULL);
	}
	void	Draw(MenuInstance *mi, instance *my, RenderRegion *rr, int state) const {
		int	state2 = (state >> 1) & 3;
		if (state2 != my->prev_state) {
			mi_Destroy(GetItem(my->prev_state), mi, (void*)*my);
			my->prev_state = state2;
			if (const mitem &i = GetItem(state2)) {
				CheckedInstanceData	cid(*my, my->size32());
				mi_Create(i, mi, cid);
			}
		}
		mi_Draw(GetItem(state2), mi, (void*)*my, rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_set / mi_set2
//-----------------------------------------------------------------------------
struct mi_set : _mi_set, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { mi_Reserve(item, mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)			const { mi_Create(item, mi, d);					}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { mi_Destroy(item, mi, d);				}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return mi_CanBeSelected(item, mi, d);	}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return mi_Update(item, mi, d);			}

	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		if (field & _REGION_PASS_ON) {
			rr->SetField(REGION_PARAM(field & 0xff), value);
			mi_Draw(item, mi, d, rr, state);
		} else {
			RenderRegion	rr2(*rr);
			rr2.SetField(field, value);
			mi_Draw(item, mi, d, &rr2, state);
		}
	}
};

struct mi_set2 : _mi_set2, menu_item {
	typedef	float *instance;

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		mi_Reserve(item, mi, d);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		*my = (float*)mi->SendEvent(funcid, MMSG_GETFLOAT, NULL);
		mi_Create(item, mi, d);
	}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { instance *my(d); mi_Destroy(item, mi, d); }
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { instance *my(d); return mi_CanBeSelected(item, mi, d);	}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { instance *my(d); return mi_Update(item, mi, d);	}

	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance *my(d);
		float	value;
		if (*my)
			value = **my;
		else if (!mi->SendEvent(funcid, MMSG_GETFLOAT, &value))
			return;

		if (field & _REGION_PASS_ON) {
			rr->SetField(REGION_PARAM(field & 0xff), value);
			mi_Draw(item, mi, d, rr, state);
		} else {
			RenderRegion	rr2(*rr);
			rr2.SetField(field, value);
			mi_Draw(item, mi, d, &rr2, state);
		}
	}
};

//-----------------------------------------------------------------------------
//	mi_curve/mi_curve2
//-----------------------------------------------------------------------------

int LookupCurve(float t, const ISO_openarray<float2p> &control, float &v, int flags) {
	float2p	*c = control;

	if (t < c->x) {
		v = c->y;
		return -1;
	}

	float2p	*c0		= c, *cn = c + control.Count();
	bool	loop	= !!(flags & LC_LOOP);

	if (loop) {
		float	t0 = c[0].x, len;
		if (cn[-1].x < 0) {
			len = -(--cn)->x;
			t0	= cn[-1].x - len;
		} else {
			len = cn[-1].x - t0;
		}
		t = t - iso::floor((t - t0) / len) * len;

	} else if (t >= cn[-1].x) {
		v = cn[-1].y;
		return 1;
	}

	while (t >= c->x)
		c++;

	float t0 = c[-1].x,	v0 = c[-1].y;
	float t1 = c[0].x,	v1 = c[0].y;
	float dt = t1 - t0;
	t = (t - t0) / dt;

	switch (flags & _LC_MODE_MASK) {
		case LC_DISCRETE:		v = v0;									break;
		case LC_LINEAR:			v = lerp(v0, v1, t);					break;
		case LC_EASY_IN_OUT:	v = lerp(v0, v1, t * t * (3 - 2 * t));	break;
		case LC_EASY_IN:		v = lerp(v0, v1, 1 - cube(1 - t));		break;
		case LC_EASY_OUT:		v = lerp(v0, v1, cube(t));				break;
		case LC_CUBIC: {
			// handles
			float2p	*p0	= c > c0 + 1 ? c : loop && v0 == cn[-1].y ? cn : 0;
			float2p	*p1	= c < cn - 1 ? c : loop && v1 == c0[ 0].y ? c0 : 0;

			float	r	= dt / 3;
			float	h0	= p0 ? v0 + (v1 - p0[-2].y) / (p0[-1].x - p0[-2].x + dt) * r : v0;
			float	h1	= p1 ? v1 - (p1[+1].y - v0) / (p1[+1].x - p1[ 0].x + dt) * r : v1;

			// cubic
			float	it	= 1 - t;
			v = it * it * (it * v0 + 3 * t * h0) + t * t * (t * v1 + 3 * it * h1);
			break;
		}
	}
	return 0;
}

struct mi_curve : _mi_curve, menu_item {
	struct instance {
		float	start_time;
		void	*data;
		instance(float _start_time) : start_time(_start_time), data(0) {}
	};

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
//		if (!flags.test(LC_RELATIVE_INIT))
			mi_Reserve(item, mi, d);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance *my = new(d) instance(
			flags.test(LC_RELATIVE_START)	? -1
		:	flags.test(LC_OFFSET_START)		? mi->GetTime() - control[0][0]
		:	mi->GetTime()
		);
		if (!flags.test(LC_RELATIVE_INIT))
			mi_Create(item, mi, d);
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		if (!flags.test(LC_RELATIVE_INIT)) {
			mi_Destroy(item, mi, d);
		} else if (my->data) {
			mi_Destroy(item, mi, my->data);
			iso::free(my->data);
		}
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		if (flags.test(LC_RELATIVE_INIT))
			return my->data && mi_CanBeSelected(item, mi, my->data);
		return mi_CanBeSelected(item, mi, d);
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		if (!(my->start_time >= 0 && mi->GetTime() - my->start_time > control[0][0]))
			return mreturn();
		if (flags.test(LC_RELATIVE_INIT))
			return my->data ? mi_Update(item, mi, my->data) : MIU_DEFAULT;
		if (flags.test(LC_END_TRIGGER) && mi->GetTime() - my->start_time >= control.back()[0])
			return MIU_TRIGGER;
		return mi_Update(item, mi, d);
	}

	void	Draw0(MenuInstance *mi, instance *my, RenderRegion *rr, int state, float v) const {
		if (flags.test(LC_TRUNC))
			v = int(v);

		if (flags.test(LC_RELATIVE_INIT) && !my->data)
			save(mi->time, my->start_time + control[0].x), CreateInstanceData(my->data, mi, item);

		InstanceData	d(my->data ? my->data : my + 1);
		if (field & _REGION_PASS_ON) {
			rr->SetField(REGION_PARAM(field & 0xff), v);
			mi_Draw(item, mi, d, rr, state);
		} else {
			RenderRegion	rr2(*rr);
			rr2.SetField(field, v);
			mi_Draw(item, mi, d, &rr2, state);
		}
	}

	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance	*my(d);
		if (my->start_time < 0) {
			my->start_time = mi->GetTime();
			if (flags.test(LC_OFFSET_START))
				my->start_time -= control[0][0];
		}
		float		v;
		switch (LookupCurve(mi->GetTime() - my->start_time, control, v, flags)) {
			case -1:
				if (flags.test(LC_CLAMP_START)) {
					RenderRegion	rr2(*rr);
					rr2.SetField(field, v);
					mi_Draw(item, mi, my->data ? InstanceData(my->data) : d, &rr2, state);
				}
				break;
			case 1:
				if (flags.test(LC_KILLMENU)) {
					mi->currmenu->last_root()->state = MS_DIE;

				} else if (flags.test(LC_COVER)) {
					if (state == 0)
						mi->currmenu->state = MS_COVERED;
					else if (menu_data *m = mi->currmenu->last_root()->prev)
						m->state = MS_COVERED;
				}

				if (flags.test(LC_COVER_ME))
					mi->currmenu->last_root()->state = MS_COVERED;

				if (!flags.test_any(LC_KILLMENU | LC_STOP))
					Draw0(mi, my, rr, state, v);

				if (flags.test(LC_DESELECT) && (state & 1))
					mi->active = MenuInstance::DESELECT;
				break;
			case 0:
				Draw0(mi, my, rr, state, v);
				break;
		}
	}
};

struct mi_curve2 : _mi_curve2, menu_item {
	typedef		float	*instance;

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		mi_Reserve(item, mi, d);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		*my = (float*)mi->SendEvent(funcid, MMSG_GETFLOAT, NULL);
		mi_Create(item, mi, d);
	}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { instance *my(d); mi_Destroy(item, mi, d); }
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { instance *my(d); return mi_CanBeSelected(item, mi, d);	}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { instance *my(d); return mi_Update(item, mi, d);	}

	void	Draw0(MenuInstance *mi, instance *my, RenderRegion *rr, int state, float v) const {
		if (flags.test(LC_TRUNC))
			v = int(v);
		if (field & _REGION_PASS_ON) {
			rr->SetField(REGION_PARAM(field & 0xff), v);
			mi_Draw(item, mi, my + 1, rr, state);
		} else {
			RenderRegion rr2(*rr);
			rr2.SetField(field, v);
			if (rr2.size.x > 0 && rr2.size.y > 0)
				mi_Draw(item, mi, my + 1, &rr2, state);
		}
	}

	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance *my(d);
		float t;
		if (*my)
			t = **my;
		else if (!mi->SendEvent(funcid, MMSG_GETFLOAT, &t))
			return;

		float v;
		switch (LookupCurve(t, control, v, flags)) {
			case 1:
				if (flags.test(LC_KILLMENU)) {
					mi->currmenu->last_root()->state = MS_DIE;

				} else if (flags.test(LC_COVER)) {
					if (state == 0)
						mi->currmenu->state = MS_COVERED;
					else if (menu_data *m = mi->currmenu->last_root()->prev)
						m->state = MS_COVERED;
				}

				if (flags.test(LC_COVER_ME))
					mi->currmenu->last_root()->state = MS_COVERED;

				if (!flags.test_any(LC_KILLMENU | LC_STOP))
					Draw0(mi, my, rr, state, v);

				if (flags.test(LC_DESELECT) && (state & 1))
					mi->active = MenuInstance::DESELECT;
				break;
			case 0: {
				Draw0(mi, my, rr, state, v);
				break;
			}
		}
	}
};

//-----------------------------------------------------------------------------
//	mi_custom
//-----------------------------------------------------------------------------
struct mi_custom : _mi_custom, menu_item {
	struct instance {
		void		*allocated;
		void		*start;
		void		*end;
		arbitrary	user;
		int8		init;
	};

	struct header {
		const mi_custom			&self;
		InstanceData			&data;
		instance				*my;
		RenderRegionValuesNode1	values;

		header(MenuInstance *mi, const mi_custom &_self, InstanceData &_data, RenderRegion *rr = 0)
		 : self(_self), data(_data), my(data)
		 , values(mi, _data, _self.args.Count(), rr)
		{}

		void	init() {
			my->start		= data;
			my->allocated	= (void*)0;
			my->user		= (void*)0;
		}
		void	prolog() {
			data			= my->start;
		}
		void*	_Allocate(size_t size, size_t align, bool called) {
			my->allocated = data.alloc(size, align);
			if (!called)
				my->start = data;
			return my->allocated;
		}

		void	_Reserve(MenuInstance *mi)	{ menu_item::mi_Reserve(self.item, mi, data);	}
		void	_Create(MenuInstance *mi)	{ menu_item::mi_Create(self.item, mi, data);	}
		void	_Destroy(MenuInstance *mi)	{ prolog(); menu_item::mi_Destroy(self.item, mi, data);	}
		mreturn	_Update(MenuInstance *mi)	{ prolog(); return self.item ? menu_item::mi_Update(self.item, mi, data) : MIU_DEFAULT; }
		void	_Draw(MenuInstance *mi, RenderRegion *rr, int state) { prolog(); menu_item::mi_Draw(self.item, mi, data, rr, state); }
	};

	template<class T> struct CustomAdaptor : header, T {
		CustomAdaptor(MenuInstance *mi, const mi_custom &self, InstanceData &data, RenderRegion *rr = 0) : header(mi, self, data, rr) {}
		operator T*()	{ return this; }
	};

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		CustomAdaptor<MenuCustomReserve>	c(mi, *this, d);
		mi->SendEvent(funcid, MMSG_CUSTOMRESERVE, (MenuCustomReserve*)c);
		if (!c.called)
			c._Reserve(mi);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		CustomAdaptor<MenuCustomInit>	c(mi, *this, d);
		c.values.init(args);
		if (!(c.my->init = mi->SendEvent(funcid, MMSG_CUSTOMINIT, (MenuCustomInit*)c)))
			c.my->init = c.ret;
		if (!c.called)
			c._Create(mi);
		c.my->end	= d;
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		CustomAdaptor<MenuCustomExit>	c(mi, *this, d);
		mi->SendEvent(funcid, MMSG_CUSTOMEXIT, (MenuCustomExit*)c);
		if (!c.called)
			c._Destroy(mi);
		d = c.my->end;
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		return my->init > 0 || (my->init == 0 && item && mi_CanBeSelected(item, mi, d));
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		CustomAdaptor<MenuCustomUpdate>	c(mi, *this, d);
		mreturn	r = mi->SendEvent(funcid, MMSG_CUSTOMUPDATE, (MenuCustomUpdate*)c);
		if (!c.called && r == 0)
			r = c._Update(mi);
		d = c.my->end;
		return r;
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		CustomAdaptor<MenuCustomDraw> c(mi, *this, d, rr);
		if (c.my->init > -2) {
			if (c.my->init < 0)
				state &= ~1;
			c.rr	= rr;
			mreturn	r = mi->SendEvent(funcid, MMSG_CUSTOMDRAW, (MenuCustomDraw*)c, state);
			if (!c.called && r >= 0)
				c._Draw(mi, rr, r & 1 ? (state ^ 1) : state);
			if (iso::abs((int)r) == 2)
				mi->active = MenuInstance::ME;
			//if (mi->active == MenuInstance::ME && c.my->init < 0)
			//	mi->active = MenuInstance::DEFAULT;
		}
		d = c.my->end;
	}
};

template<typename T> force_inline const mi_custom::header *GetCustomHeader(const T *me) {
	return static_cast<const mi_custom::CustomAdaptor<T>*>(me);
}
template<typename T> force_inline mi_custom::header *GetCustomHeader(T *me) {
	return static_cast<mi_custom::CustomAdaptor<T>*>(me);
}

// MenuCustom
const ISO::Browser MenuCustom::GetArgs() const {
	return ISO::MakeBrowser((RenderRegionValues0*)&GetCustomHeader(this)->values);
}
arbitrary& MenuCustom::User() {
	return GetCustomHeader(this)->my->user;
}
void *MenuCustom::Allocated() {
	return GetCustomHeader(this)->my->allocated;
}

// MenuCustomReserve
void MenuCustomReserve::Reserve(MenuInstance *mi) {
	called = true;
	GetCustomHeader(this)->_Reserve(mi);
}
void MenuCustomReserve::Reserve(size_t size, size_t align) {
	GetCustomHeader(this)->data.alloc(size, align);
}
// MenuCustomInit
MenuCustomInit::MenuCustomInit() : ret(0) {
	GetCustomHeader(this)->init();
}
void MenuCustomInit::Init(MenuInstance *mi) {
	called = true;
	GetCustomHeader(this)->_Create(mi);
}
void *MenuCustomInit::Allocate(size_t size, size_t align) {
	return GetCustomHeader(this)->_Allocate(size, align, called);
}
// MenuCustomExit
void MenuCustomExit::Destroy(MenuInstance *mi) {
	called = true;
	GetCustomHeader(this)->_Destroy(mi);
}
// MenuCustomDraw
void MenuCustomDraw::Draw(MenuInstance *mi, RenderRegion *rr, int state) {
	called = true;
	GetCustomHeader(this)->_Draw(mi, rr, state);
}
void MenuCustomDraw::Draw(MenuInstance *mi, RenderRegion *rr) {
	Draw(mi, rr, mi->state);
}
void MenuCustomDraw::Draw(MenuInstance *mi, int state) {
	Draw(mi, rr, state);
}
void MenuCustomDraw::Draw(MenuInstance *mi) {
	Draw(mi, rr, mi->state);
}

// MenuCustomUpdate
mreturn MenuCustomUpdate::Update(MenuInstance *mi) {
	called = true;
	return GetCustomHeader(this)->_Update(mi);
}

//-----------------------------------------------------------------------------
//	mi_warp
//-----------------------------------------------------------------------------
struct mi_warp : _mi_warp, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d)			const { mi_Reserve(item, mi, d);				}
	void	Create(MenuInstance *mi, InstanceData &d)			const { mi_Create(item, mi, d);					}
	void	Destroy(MenuInstance *mi, InstanceData &d)			const { mi_Destroy(item, mi, d);				}
	mreturn Update(MenuInstance *mi, InstanceData &d)			const { return mi_Update(item, mi, d);			}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d)	const { return mi_CanBeSelected(item, mi, d);	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		RenderRegion	rr2 = *rr;
		float			w	= rr2.size.x, h = rr2.size.y;

		float3x3	matC(
			float3{c[1].x + w,	c[1].y,		one},
			float3{c[2].x,		c[2].y + h, one},
			float3{c[0].x * 2,	c[0].y * 2,	2}
		);
		float3x3	matP(
			float3{w,		zero,	one},
			float3{zero,	h,		one},
			float3{zero,	zero,	2}
		);

		float3x3	invP	= inverse(matP);
		float3x3	invC	= inverse(matC);
		float3		v		= (invC * position2(c[3].x + w , c[3].y + h)).v / (invP * position2(w, h)).v;

		matC.x *= v.x;
		matC.y *= v.y;
		matC.z *= v.z;

		float4x4	mat = float4x4(matC * invP);
		mat = transpose(mat); swap(mat.z, mat.w);
		mat = transpose(mat); swap(mat.z, mat.w);
		rr2.matrix	= rr2.matrix * mat;

		mi_Draw(item, mi, d, &rr2, state);
#ifdef ISO_EDITOR
		mi->SendEvent(0, MMSG_EDITOR, MenuEditor(this, d, rr, &rr2).me());
#endif
	}
};

//-----------------------------------------------------------------------------
//	mi_param / mi_arg
//-----------------------------------------------------------------------------
struct mi_param : _mi_param, menu_item {
	static struct handle {
		const mi_param	*parm;
		handle			*prev;
		handle(const mi_param *_param) : parm(_param)	{ prev = first; first = this; }
		~handle()										{ first = prev; }
	} *first;

	static const mitem &get(const tag id) {
		for (handle *p = first; p; p = p->prev) {
			if (p->parm->id == id)
				return p->parm->item;
		}
		return ISO::iso_nil<32>;
	}

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		if (process)
			(handle(this), mi_Reserve(process, mi, d));
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		if (process)
			(handle(this), mi_Create(process, mi, d));
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		if (process)
			(handle(this), mi_Destroy(process, mi, d));
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		return process ? (handle(this), mi_Update(process, mi, d)) : menu_item::Update(mi, NULL);
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		return process && (handle(this), mi_CanBeSelected(process, mi, d));
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		if (process)
			(handle(this), mi_Draw(process, mi, d, rr, state));
	}
};
mi_param::handle *mi_param::first = NULL;

struct mi_arg : _mi_arg, menu_item {
	struct instance {
		const mitem &item;
		instance(const mitem &_item) : item(_item) {}
	};
	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		mi_Reserve(mi_param::get(id), mi, d);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance	*my = new(d) instance(mi_param::get(id));
		mi_Create(my->item, mi, d);
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		mi_Destroy(my->item, mi, d);
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		return mi_CanBeSelected(my->item, mi, d);
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		instance *my(d);
		return my->item ? mi_Update(my->item, mi, d) : menu_item::Update(mi, NULL);
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state)	const {
		instance *my(d);
		mi_Draw(my->item, mi, d, rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_param2 / mi_arg2
//-----------------------------------------------------------------------------
struct mi_param2 : _mi_param2, menu_item {
	struct instance {
		void	*item_data;
	};

	static struct handle {
		const mi_param2	*parm;
		handle			*prev;
		instance		*inst;
		handle(const mi_param2 *_parm, instance *_inst) : parm(_parm), inst(_inst) {
			prev	= first;
			first	= this;
		}
		~handle() {
			first	= prev;
		}
		void			*data() { return inst + 1; }
	} *first;

	static handle *get(const tag id) {
		for (handle *p = first; p; p = p->prev) {
			if (p->parm && p->parm->id == id)
				return p;
		}
		return NULL;
	}

	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		mi_Reserve(item, mi, d);
		(handle(this, 0), mi_Reserve(process, mi, d));
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		mi_Create(item, mi, d);
		my->item_data = d;
		(handle(this, my), mi_Create(process, mi, d));
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		(handle(this, my), mi_Destroy(process, mi, my->item_data));
		mi_Destroy(item, mi, d);
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		return (handle(this, my), mi_CanBeSelected(process, mi, my->item_data));
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		instance	*my(d);
		return (handle(this, my), mi_Update(process, mi, my->item_data));
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		instance	*my(d);
		(handle(this, my), mi_Draw(process, mi, my->item_data, rr, state));
	}
};
mi_param2::handle *mi_param2::first = NULL;

struct mi_arg2 : _mi_arg2, menu_item {
	struct disable {
		mi_param2::handle *h;
		const mi_param2	*parm;
		disable(mi_param2::handle *_h) : h(_h), parm(_h->parm) { h->parm = 0; }
		~disable()	{ h->parm = parm; }
		operator const mitem&()	const { return parm->item; }
	};
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		mi_param2::handle *i = mi_param2::get(id);
		return i && mi_CanBeSelected(disable(i), mi, i->data());
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		mi_param2::handle *i = mi_param2::get(id);
		return i ? mi_Update(disable(i), mi, i->data()) : menu_item::Update(mi, NULL);
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		if (mi_param2::handle *i = mi_param2::get(id))
			mi_Draw(disable(i), mi, i->data(), rr, state);
	}
};

//-----------------------------------------------------------------------------
//	mi_vars
//-----------------------------------------------------------------------------

struct mi_vars : _mi_vars, menu_item {
	void	Reserve(MenuInstance *mi, InstanceData &d) const {
		d.alloc<RenderRegionValue>(vars.Count()); mi_Reserve(item, mi, d);
	}
	void	Create(MenuInstance *mi, InstanceData &d) const {
		(RenderRegionValuesNode1(mi, d, vars.Count()).init(vars), mi_Create(item, mi, d));
	}
	void	Destroy(MenuInstance *mi, InstanceData &d) const {
		(RenderRegionValuesNode1(mi, d, vars.Count()), mi_Destroy(item, mi, d));
	}
	bool	CanBeSelected(MenuInstance *mi, InstanceData &d) const {
		return (RenderRegionValuesNode1(mi, d, vars.Count()), mi_CanBeSelected(item, mi, d));
	}
	mreturn Update(MenuInstance *mi, InstanceData &d) const {
		return (RenderRegionValuesNode1(mi, d, vars.Count()), mi_Update(item, mi, d));
	}
	void	Draw(MenuInstance *mi, InstanceData &d, RenderRegion *rr, int state) const {
		(RenderRegionValuesNode1(mi, d, vars.Count(), rr), mi_Draw(item, mi, d, rr, state));
	}
};

//-----------------------------------------------------------------------------
//	ISO_defs
//-----------------------------------------------------------------------------

ISO_DEFVIRT(RenderRegionValues);
ISO_DEFVIRT(RenderRegionValues0);
#define MI_TYPE(T)	mi_template<T> def_##T(#T); template<> ISO::Type *ISO::getdef<T>() { return &def_##T; }

MI_TYPE(menu)
MI_TYPE(mi_arrange)
MI_TYPE(mi_offset)
MI_TYPE(mi_box)
MI_TYPE(mi_textbox)
MI_TYPE(mi_centre)
//MI_TYPE(mi_button)
MI_TYPE(mi_fill)
MI_TYPE(mi_colour)
MI_TYPE(mi_font)
MI_TYPE(mi_int)
MI_TYPE(mi_indexed)
MI_TYPE(mi_list)
MI_TYPE(mi_sellist)
MI_TYPE(mi_text)
MI_TYPE(mi_gettext)
MI_TYPE(mi_print)
MI_TYPE(mi_shader)
MI_TYPE(mi_texture)
MI_TYPE(mi_button)
MI_TYPE(mi_trigger)
MI_TYPE(mi_trigger_menu)
MI_TYPE(mi_ifselected)
MI_TYPE(mi_state)
MI_TYPE(mi_set)
MI_TYPE(mi_set2)
MI_TYPE(mi_curve)
MI_TYPE(mi_curve2)
MI_TYPE(mi_custom)
MI_TYPE(mi_warp)
MI_TYPE(mi_param)
MI_TYPE(mi_arg)
MI_TYPE(mi_param2)
MI_TYPE(mi_arg2)
MI_TYPE(mi_vars)
initialise menu_defs(
	ISO::getdef<REGION_PARAM>(),
	ISO::getdef<REGION_PARAM2>(),
	ISO::getdef<REGION_PARAM4>()
);

//-----------------------------------------------------------------------------
//	RenderRegion
//-----------------------------------------------------------------------------

float2x3 RenderRegion::region_trans = identity;

RenderRegion::RenderRegion(GraphicsContext &_ctx, param(float4x4) _matrix, float _w, float _h) : ctx(_ctx), matrix(_matrix), fp(0), pass(0) {
	size	= float2{_w, _h};
	offset	= zero;
	cols[0] = cols[1] = colour(one);
	technique = menu_system.shaders ? menu_system.shaders->blend_texture : 0;
	clear(params);
}

inline float trans_scale(float v) { return v ? (1 - v) / (v * 2) : 0; }

RenderRegion &RenderRegion::SetField(REGION_PARAM i, float v) {
	if (i < REGION_X) {
		((float*)this)[i] = v;
	} else switch (i) {
		case REGION_X1:				offset.x	-= v;
		case REGION_X:				size.x		-= v;
		case REGION_X2:				matrix		= matrix * translate(v, zero, zero);	break;
		case REGION_Y1:				offset.y	-= v;
		case REGION_Y:				size.y		-= v;
		case REGION_Y2:				matrix		= matrix * translate(zero, v, zero);	break;

		case REGION_SCALE:			matrix		= matrix * scale(concat(float2(v), one, one)); size /= v; offset /= v; break;
		case REGION_PERSPECTIVE:	matrix.z.w = v; matrix.z.z = v * matrix.w.z;		break;

//		case REGION_ROT:			matrix = matrix * float4x4(translate(w/2,h/2,0) * rotate_in_z(v * pi / 180) * translate(-w/2,-h/2,0));	break;
		case REGION_ROT_O:			matrix = matrix * rotate_in_z(degrees(v));			break;
		case REGION_ROT:			matrix = matrix * (translate(position3(concat(float2(size) / 2, zero))) * rotate_in_z(degrees(v)) * translate(position3(concat(float2(size)/-2, zero))));	break;
		case REGION_ROT_X:			matrix = matrix * translate(zero, size.y/2, half) * rotate_in_x(degrees(v)) * translate(zero,-size.y/2,-half);	break;
		case REGION_ROT_Y:			matrix = matrix * translate(size.x/2, zero, half) * rotate_in_y(degrees(v)) * translate(-size.x/2,zero,-half);	break;

		case REGION_WIDTH1:			offset.x += v - size.x; size.x = v;					break;
		case REGION_HEIGHT1:		offset.y += v - size.y; size.y = v;					break;

		case REGION_SCALE1:			matrix = matrix * scale(concat(float2(v), one, one)) * translate(position3(concat(size, zero)) * trans_scale(v)); break;
		case REGION_SCALE_X:		if (v < 0) matrix = matrix * translate(size.x, 0, 0); matrix = matrix * scale(float4{v, one, one, one}); v = iso::abs(v); size.x /= v; offset.x /= v; break;
		case REGION_SCALE_Y:		if (v < 0) matrix = matrix * translate(0, size.y, 0); matrix = matrix * scale(float4{one, v, one, one}); v = iso::abs(v); size.y /= v; offset.y /= v; break;
		case REGION_SCALE_X1:		matrix = matrix * scale(float4{v, one, one, one}) * translate(size.x * trans_scale(v), 0, 0); break;
		case REGION_SCALE_Y1:		matrix = matrix * scale(float4{one, v, one, one}) * translate(0, size.y * trans_scale(v), 0); break;

		case REGION_OFFSET_X_ADD:	offset.x += v;										break;
		case REGION_OFFSET_Y_ADD:	offset.y += v;										break;
		case REGION_OPACITY:		cols[0].a *= v;										break;
		case REGION_BRIGHTNESS:		cols[0].rgb = max(cols[0].rgb + v, zero); cols[1].rgb = max(cols[1].rgb + v, zero); break;
		case REGION_Z:				matrix = matrix * translate(zero, zero, v);			break;
		case REGION_POST_Z:			matrix.w.w += v;									break;
//		case REGION_INDEX:			menu_item::SetIndex(int(v));						break;
		case REGION_PASS:			pass = int(v);										break;
		case REGION_COLOUR_INTERP:	cols[0] = colour(lerp(cols[0].rgba, cols[1].rgba, v));	break;
	}
	return *this;
}

float RenderRegion::GetField(REGION_PARAM i) const {
	if (i < REGION_X) {
		return ((float*)this)[i];
	} else switch (i) {
//		case REGION_INDEX: return menu_item::GetIndex();
		case REGION_WIDTH_FONT:		return size.x / fp->scale;
		case REGION_HEIGHT_FONT:	return size.y / fp->scale;
		case REGION_WIDTH_FONT_R:	return fp->scale / size.x;
		case REGION_HEIGHT_FONT_R:	return fp->scale / size.y;
	}
	return 0.f;
}

MenuRect RenderRegion::GetTextBox(const char *text, uint32 flags) const {
	return ::GetTextBox(text, flags, fp, size.x, size.y);
}

void RenderRegion::DrawText(const char *text, uint32 flags, float threshold, iso::technique *technique, const ISO::Browser &parameters) const {
	ISO_ASSERT(technique && *technique && fp);

	Font	*font	= fp->font;
	float	w1		= size.x / fp->scale;
	float	h1		= size.y / fp->scale;

	FontPrinter	fontprinter(ctx, font);
	fontprinter.SetAlign(FontAlign(flags & 3)).SetLine(fp->line + 1).SetParagraph(fp->paragraph + 1).SetFlags(flags >> 8);

	if (flags & TF_WRAP)
		fontprinter.SetWidth(w1);

	auto	ext = fontprinter.CalcRect(text);
	ext.y	-= font->top;
	if (!(flags & (TF_NOSQUISH_H | TF_WRAP | TF_CLIP_H)))
		fontprinter.SetWidth(w1);

	float	x = 0, y = 0, yscale = fp->scale, xscale = fp->scale;

	if (flags & TF_CLIP)
		fontprinter.SetClip0(0, 0, w1, h1);

	if (flags & (TF_CLIP_H | TF_NOSQUISH_H)) {
		if (!(flags & TF_WRAP)) {
			switch (flags & 3) {
				case FA_RIGHT:	x = w1 - offset.x / fp->scale;		break;
				case FA_CENTRE:	x = w1 / 2; break;
			}
		}
	} else if (ext.x > w1) {
		xscale = size.x / ext.x;
		fontprinter.SetWidth(ext.x);
		if (flags & TF_WRAP)
			ext = fontprinter.CalcRect(text);
	}

	if (ext.y > h1 && !(flags & (TF_CLIP_V | TF_NOSQUISH_V))) {
		yscale	= size.y / ext.y;
	} else switch ((flags >> 2) & 3) {
		case FA_RIGHT:	y = h1 - ext.y - offset.y / fp->scale;	break;
		case FA_CENTRE:	y = (h1 - ext.y) / 2;	break;
		case FA_JUSTIFY: {
			int	ncr = 0;
			for (const char *p = text; (p = strchr(p, '\n')); p++)
				ncr++;
			if (ncr)
				fontprinter.SetParagraph(fp->paragraph + 1 + (h1 - ext.y) / (font->spacing * ncr));
			break;
		}
	}

	float4		font_size		= concat(reciprocal(to<float>(font->Tex().Size())), float(font->outline), threshold);
	float4x4	worldViewProj	= hardware_fix(matrix * scale(float4{xscale, yscale, one, one}));

	AddShaderParameter(ISO_CRC("worldViewProj", 0xb3f90db0),	worldViewProj);
	AddShaderParameter(ISO_CRC("font_samp", 0x810bb451),		font->Tex());
	AddShaderParameter(ISO_CRC("font_size", 0xe7468594),		font_size);
	SetTechniquePass(ctx, technique, pass, parameters);
	float	shear	= fp->italic + (flags & TF_ITALIC ? 1/3.0f : 0);
	fontprinter.SetColour(cols[0])
		.SetShear(shear * font->height)
		.At(offset.x / fp->scale - shear * (font->height - font->baseline) + x, offset.y / fp->scale + y + (font->baseline - font->top) * (fp->shift + 1));

	fontprinter.Print(text);
}

void RenderRegion::DrawText(const char *text, uint32 flags) const {
	if (!fp)
		iso_throw("No font set");

	float	outline = fp->outline;
	float	bold	= fp->bold;

	if (flags & TF_BOLD)
		bold	= 0.4f;

	switch (flags & (TF_OUTLINE|TF_GLOW)) {
		case TF_OUTLINE:	outline	= 1; break;
		case TF_GLOW:		outline	= -2; break;
		case TF_OUTLINE|TF_GLOW:
			outline = params[1] * matrix.w.w * rlen(matrix.x.xyz) / (fp->scale * 480);
			outline = min(outline,6);
			break;
	}

	if (outline && cols[1].a) {
		AddShaderParameter(ISO_CRC("diffuse_colour", 0x6d407ef0), cols[1]);
		if (outline < 0) {
			DrawText(text, flags, .5f + outline / 16, menu_system.shaders->font_dist_glow);
			return;
		} else {
			DrawText(text, flags, .5f - (outline + bold) / 16, menu_system.shaders->font_dist_outline);
			if (bold == 0 && outline <= 1)
				return;
		}
	}
	DrawText(text, flags, .5f - bold / 16, menu_system.shaders->font_dist);
}

void RenderRegion::Fill(param(colour) col) const {
	float		x0, x1, y0, y1;

	if (offset.x >= 0) {
		x0 = offset.x;
		x1 = size.x;
	} else {
		x0 = 0;
		x1 = size.x + offset.x;
	}

	if (offset.y >= 0) {
		y0 = offset.y;
		y1 = size.y;
	} else {
		y0 = 0;
		y1 = size.y + offset.y;
	}

	if (x0 < x1 && y0 < y1) {
		float4x4	worldViewProj = hardware_fix(matrix);
		AddShaderParameter(ISO_CRC("worldViewProj", 0xb3f90db0),	worldViewProj);
		AddShaderParameter(ISO_CRC("diffuse_colour", 0x6d407ef0),	col);
		SetTechniquePass(ctx, menu_system.shaders->blend_solid, pass);

		PostEffects(ctx).DrawQuad(float2{x0, y0}, float2{x1, y1});
	}
}

void RenderRegion::Line(param(colour) col, float x0, float y0, float x1, float y1) const {
	if (x0 > x1)
		swap(x0, x1);
	if (y0 > y1)
		swap(y0, y1);

	x0 = max(x0 + offset.x, zero);
	x1 = min(x1 + offset.x, size.x);
	y0 = max(y0 + offset.y, zero);
	y1 = min(y1 + offset.y, size.y);

	if (x0 <= x1 && y0 <= y1) {
		float4x4	worldViewProj = hardware_fix(matrix);
		AddShaderParameter(ISO_CRC("worldViewProj", 0xb3f90db0),		worldViewProj);
		AddShaderParameter(ISO_CRC("diffuse_colour", 0x6d407ef0),	col);
		SetTechniquePass(ctx, menu_system.shaders->blend_solid, pass);

		ImmediateStream<float2p>	ims(ctx, PRIM_LINELIST, 2);
		float2p	*p	= ims.begin();
		p[0] = float2{x0,	y0};
		p[1] = float2{x1,	y1};
	}
}

void RenderRegion::Draw(const Texture &tex, iso::technique *technique, const ISO::Browser &b, param(float2) p0, param(float2) p1, param(float2) uv0, param(float2) uv1) const {
	float4x4	worldViewProj = hardware_fix(matrix);
	AddShaderParameter(ISO_CRC("worldViewProj", 0xb3f90db0),	worldViewProj);
	AddShaderParameter(ISO_CRC("diffuse_samp", 0xe31becbe),		tex);
	SetTechniquePass(ctx, technique, pass, b);//*values);
	PostEffects(ctx).DrawQuad(p0, p1, uv0, uv1, cols[0]);
}

void RenderRegion::Draw(const Texture &tex, iso::technique *technique, const ISO::Browser &b, int align) const {
	float	x	= offset.x, y = offset.y;
	float	w1	= size.x, h1 = size.y;
	if (align & (_TAL_ALIGN_H | _TAL_ALIGN_V | TAL_FIT)) {
		iso::point	ts	= tex.Size();
		auto	ext	= to<float>(ts);

		if (align & TAL_FIT)
			align &= ~(TAL_FIT | (ext.x / ext.y < w1 / h1 ? _TAL_ALIGN_V : _TAL_ALIGN_H));

		if ((align & _TAL_ALIGN_H) == 0)
			h1 = w1 * ext.y / ext.x;
		else if ((align & _TAL_ALIGN_V) == 0)
			w1 = h1 * ext.x / ext.y;
		else
			w1 = ext.x, h1 = ext.y;

		switch (align & _TAL_ALIGN_H) {
//			case TAL_LEFT:		x = offset.x;						break;
			case TAL_RIGHT:		x = offset.x + size.x - w1;			break;
			case TAL_CENTRE_H: 	x = offset.x + (size.x - w1) / 2;	break;
		}
		switch (align & _TAL_ALIGN_V) {
//			case TAL_TOP:		y = offset.y;						break;
			case TAL_BOTTOM:	y = offset.y + size.y - h1;			break;
			case TAL_CENTRE_V:	y = offset.y +(size.y - h1) / 2;	break;
		}
	}

	float2	o{x, y}, s1{w1, h1};
	float4	s	= concat(float2(zero), size);
	uint8	clip_mask = ((align / TAL_NOCLIP_H)	& 3) * 5;
	uint8	wrap_mask = ((align / TAL_WRAP_H)	& 3) * 5;
	uint8	flip_mask = ((align / TAL_FLIP_H)	& 3) * 5;

	float4	uv = (s - o.xyxy) / s1.xyxy;
	uv	= select(wrap_mask, uv, clamp(uv, zero, one));
	uv	= select(clip_mask, concat(float2(zero), float2(one)), uv);

	if (any(uv.xy > uv.zw))
		return;

	float4	v = select(wrap_mask, s, uv * s1.xyxy + o.xyxy);
	uv	= select(flip_mask, float4(one) - uv, uv);

	ctx.SetUVMode(0, align & TAL_WRAP ? ALL_WRAP : ALL_CLAMP);
	Draw(tex, technique, b, v.xy, v.zw, uv.xy, uv.zw);
}

//-----------------------------------------------------------------------------

MenuInstance::MenuInstance(const MenuDelegate &_delegate)
	: delegate(_delegate)
	, rootmenu(0), currmenu(0), topmenu(0)
	, text(0), values(0), texture(0)
	, ret(0), state(0)
{
}

void MenuInstance::Kill() {
	if (menu_data *d = rootmenu) {
		rootmenu = 0;
		d->Destroy(this);
		iso::free(d);
	}
	currmenu = topmenu = 0;
}

void MenuInstance::Update() {
	if (rootmenu) {
		switch (ret = rootmenu->Update(this)) {
			case MIU_HITUP:
			case MIU_HITDOWN:
			case MIU_HITLEFT:
			case MIU_HITRIGHT:
			case MIU_INC:
			case MIU_DEC:
			case MIU_TRIGGER:
				rootmenu->Update(this);
				break;
			case MIU_COMPLETEDALL:
				Kill();
				break;
		}
		ret = mreturn();
	}
}

void MenuInstance::Render0(const RenderRegion &rr) {
	rootmenu->Draw(this, const_cast<RenderRegion*>(&rr), SELECTED | TO | ACTIVE);
	if (rootmenu->state == MS_DIE)
		Kill();
}

void MenuInstance::Render(const RenderRegion &rr) {
	if (rootmenu) {
		rr.ctx.SetBackFaceCull(BFC_NONE);
		rr.ctx.SetDepthTestEnable(false);
		rr.ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
		rr.ctx.SetBlendEnable(true);
		rr.ctx.SetUVMode(0, ALL_CLAMP);
		rr.ctx.SetUVMode(1, ALL_CLAMP);
		Render0(rr);
		rr.ctx.SetUVMode(0, ALL_WRAP);
		rr.ctx.SetUVMode(1, ALL_WRAP);
	}
}
void MenuInstance::Init(const menu *m, float _time) {
	Kill();
	if (m) {
		time		= _time;
		save(index,-1), CreateInstanceData(rootmenu, this, m);
	}
}
void MenuInstance::PushMenu(const menu *m, int back, bool imm) {
	menu_data	*top = topmenu;
	while (top && top->quitting())
		top = top->prev;
	menu_data	*i;
	if (back < 0)
		for (i = top; i; i = i->prev, back++);
	for (i = top; i && back--; i = i->prev);
	if (i) {
		save(active), i->addchild(this, m, imm ? MS_COVERED : MS_NORMAL);
	} else {
		CreateInstanceData(rootmenu, this, m);
	}
}
void MenuInstance::PopMenu(bool imm) {
	if (!currmenu->pop(this, imm))
		Kill();
}
void MenuInstance::PopMenuFromTop(bool imm) {
	if (!topmenu->pop(this, imm))
		Kill();
}
int MenuInstance::Depth() const {
	int	n = 0;
	for (menu_data *i = currmenu; i; i = i->prev) {
		if (i->next)
			n++;
	}
	return n;
}

void *MenuInstance::MenuData() const {
	return (void*)rootmenu->m;
}

bool MenuInstance::IsCovered() const {
	return !currmenu->prev || currmenu->prev->state == MS_COVERED;
}

const ISO::Browser MenuInstance::GetBrowser() const {
	return ISO::MakeBrowser(*(RenderRegionValues*)values);
}
