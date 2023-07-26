#ifndef MENU_H
#define MENU_H

#include "iso/iso.h"
#include "base/vector.h"
#include "utilities.h"

#undef DrawText
#undef GetObject

class MenuInstance;

enum REGION_PARAM {
// read / write
	REGION_RED, REGION_GREEN, REGION_BLUE, REGION_ALPHA,
	REGION_RED_OUTLINE, REGION_GREEN_OUTLINE, REGION_BLUE_OUTLINE, REGION_ALPHA_OUTLINE,
	REGION_WIDTH, REGION_HEIGHT,
	REGION_OFFSET_X, REGION_OFFSET_Y,
	REGION_PARAMS, _REGION_PARAMS_COUNT = 8, _REGION_PARAMS_LAST = REGION_PARAMS + _REGION_PARAMS_COUNT - 1,
// write only
	_REGION_WRITEONLY,
	REGION_X = _REGION_WRITEONLY, REGION_Y,
	REGION_SCALE,
	REGION_PERSPECTIVE,
	REGION_ROT_O, REGION_ROT, REGION_ROT_X, REGION_ROT_Y,
	REGION_X2, REGION_Y2, REGION_X1, REGION_Y1, REGION_WIDTH1, REGION_HEIGHT1, REGION_SCALE1,
	REGION_SCALE_X, REGION_SCALE_Y,
	REGION_SCALE_X1, REGION_SCALE_Y1,
	REGION_OFFSET_X_ADD, REGION_OFFSET_Y_ADD,
	REGION_OPACITY, REGION_BRIGHTNESS,
	REGION_Z,
	REGION_POST_Z,
	REGION_INDEX,
	REGION_PASS,
	REGION_COLOUR_INTERP,
// read only
	_REGION_READONLY,
	REGION_WIDTH_FONT = _REGION_READONLY, REGION_HEIGHT_FONT,
	REGION_WIDTH_FONT_R, REGION_HEIGHT_FONT_R,
// flags
	_REGION_PASS_ON		= 256,
	_REGION_FORCE32BITS	= 0x7fffffff
};

enum REGION_PARAM2 {
	REGION_SIZE			= REGION_WIDTH,
	REGION_OFFSET		= REGION_OFFSET_X,
	REGION_SIZE_FONT	= REGION_WIDTH_FONT,
	REGION_SIZE_FONT_R	= REGION_WIDTH_FONT_R,
};
enum REGION_PARAM4 {
	REGION_COLOUR			= REGION_RED,
	REGION_COLOUR_OUTLINE	= REGION_RED_OUTLINE,
};
enum TEXT_FLAGS {
	TF_LEFT			= 0,
	TF_RIGHT		= 1,
	TF_CENTRE		= 2,
	TF_JUSTIFY		= 3,
	TF_TOP			= 0,
	TF_BOTTOM		= 4,
	TF_CENTRE_V		= 8,
	TF_JUSTIFY_V	= 12,
	TF_WRAP			= 16,
	TF_NOSQUISH_H	= 32,
	TF_NOSQUISH_V	= 64,
	TF_NOSQUISH		= 96,
//passed to FontPrinter
	TF_IGNORE_COL	= 0x100,
	TF_MULT_COL		= 0x200,
	TF_CLIP_H		= 0x400,
	TF_CLIP_V		= 0x800,
	TF_CLIP			= 0xC00,
//textbox only
	TF_PASS_ON		= 0x10000,
//
	TF_BOLD			= 0x100000,
	TF_ITALIC		= 0x200000,
	TF_OUTLINE		= 0x400000,
	TF_GLOW			= 0x800000,
	TF_USEPARAM1	= TF_OUTLINE | TF_GLOW,
	_TF_FORCE32BITS	= 0x7fffffff,
};

enum TEXTURE_BLEND {
	TB_NORMAL		= 0,
	TB_VPOS			= 1,
	TB_PREMULT		= 2,
	TB_MULTIPLY		= 3,
	TB_DISABLE		= 15,
};

enum TEXTURE_ALIGN {
	_TAL_ALIGN_H	= 3,
	TAL_LEFT		= 1,
	TAL_RIGHT		= 2,
	TAL_CENTRE_H	= 3,

	_TAL_ALIGN_V	= 12,
	TAL_TOP			= 4,
	TAL_BOTTOM		= 8,
	TAL_CENTRE_V	= 12,

	TAL_CENTRE		= TAL_CENTRE_H | TAL_CENTRE_V,
	TAL_NOCLIP_H	= 16,
	TAL_NOCLIP_V	= 32,
	TAL_NOCLIP		= TAL_NOCLIP_H | TAL_NOCLIP_V,
	TAL_FLIP_H		= 64,
	TAL_FLIP_V		= 128,
	TAL_FLIP		= TAL_FLIP_H | TAL_FLIP_V,
	TAL_WRAP_H		= 256,
	TAL_WRAP_V		= 512,
	TAL_WRAP		= TAL_WRAP_H | TAL_WRAP_V,
	TAL_FIT			= 1024,

	_TAL_ALIGNMASK	= _TAL_ALIGN_H | _TAL_ALIGN_V | TAL_FIT,

	_TAL_BLEND		= 0x10000,
	_TAL_BLEND_MASK	= _TAL_BLEND * 15,
	_TALB_NORMAL	= _TAL_BLEND * TB_NORMAL,
	_TALB_VPOS		= _TAL_BLEND * TB_VPOS,
	_TALB_PREMULT	= _TAL_BLEND * TB_PREMULT,
	_TALB_MULTIPLY	= _TAL_BLEND * TB_MULTIPLY,
	_TALB_DISABLE	= _TAL_BLEND * TB_DISABLE,

	_TAL_FORCE32BITS= 0x7fffffff,
};

ISO_DEFUSER(REGION_PARAM, int);
ISO_DEFUSER(REGION_PARAM2, int);
ISO_DEFUSER(REGION_PARAM4, int);

namespace iso {
struct TexFont;
class Texture;
struct technique;
class GraphicsContext;
}

struct fontparams {
	ISO::ptr<iso::TexFont>	font;
	float		scale, bold, italic, outline, paragraph, line, shift;

	float	LineSpacing() const;
	float	ParaSpacing() const;
};

class MenuRect {
	iso::float4	v;
public:
	MenuRect() 						: v(iso::zero)		{}
	MenuRect(param(iso::float4) a)	: v(a)				{}
	MenuRect(iso::position2 a, iso::position2 b) : v(iso::concat(a.v, b.v)) {}
	MenuRect(float a, float b, float c, float d) : v{a, b, c, d}	{}

	force_inline float			left()			const	{ return v.x;		}
	force_inline float			top()			const	{ return v.y;		}
	force_inline float			right()			const	{ return v.z;		}
	force_inline float			bottom()		const	{ return v.w;		}
	force_inline iso::float2	top_left()		const	{ return v.xy;	}
	force_inline iso::float2	bottom_right()	const	{ return v.zw;	}
	force_inline float			width()			const	{ return right() - left(); }
	force_inline float			height()		const	{ return bottom() - top(); }
	force_inline iso::float2	size()			const	{ return bottom_right() - top_left(); }
	force_inline iso::float2	margin_size()	const	{ return top_left() + bottom_right(); }
	force_inline bool			contains(param(iso::position2) b)	const	{ return iso::all(b.v < v.zw) && iso::all(b.v > v.xy);	}
};

struct RenderRegionValues;

struct RenderRegion {
	iso::GraphicsContext &ctx;
	iso::colour			cols[2];
	iso::float2p		size;
	iso::float2p		offset;
	float				params[_REGION_PARAMS_COUNT];
	iso::float4x4		matrix;
	fontparams			*fp;
	iso::technique		*technique;
	iso::uint32			pass;

	static iso::float2x3	region_trans;

	RenderRegion&		SetField(REGION_PARAM i, float v);
	float				GetField(REGION_PARAM i)				const;
	iso::float2			GetField(REGION_PARAM2 i)				const	{ return iso::load<iso::simd::float2>((const float*)this + i); }
	float				Width()									const	{ return size.x;				}
	float				Height()								const	{ return size.y;				}

	iso::float4x4		GetMatrix()								const	{ return matrix;			}
	iso::float3x3		GetMatrix3x3()							const	{ return iso::swizzle<0,1,3>(matrix);	}
	//	iso::float2x3		GetRegion()								const	{ iso::float2 s = size / 2; return inverse(region_trans * (iso::float2x3)matrix * translate(iso::position2(s)) * scale(s)); }
	iso::float2x3		GetRegion()								const	{
		iso::float2x2 mat2x3 = iso::swizzle<0,1>(matrix);
		iso::float2 s = size / 2;
		return iso::inverse(region_trans * mat2x3 * translate(iso::position2(s)) * iso::scale(s));
	}
	iso::position2		GetRelativePos(param(iso::float2) p)	const	{ return iso::position2(p) / GetMatrix3x3(); }
	MenuRect			GetBox()								const	{ return MenuRect(iso::position2(iso::zero), iso::position2(size)); }
	MenuRect			GetScreenBox()							const	{ iso::float3x3 m = GetMatrix3x3(); return MenuRect(iso::position2(m.z), m * iso::position2(size.x, size.y));	}
	bool				ContainsScreenPos(param(iso::float2) p)	const	{ return GetBox().contains(GetRelativePos(p)); }

	MenuRect			GetTextBox(const char *text, iso::uint32 flags = 0) const;
	void				DrawText(const char *text, iso::uint32 flags, float threshold, iso::technique *technique, const ISO::Browser &parameters = ISO::Browser()) const;
	void				DrawText(const char *text, iso::uint32 flags = 0) const;
	void				Fill(param(iso::colour) col) const;
	void				Line(param(iso::colour) col, float x0, float y0, float x1, float y1) const;
	void				Draw(const iso::Texture &tex, iso::technique *technique, const ISO::Browser &b, param(iso::float2) p0, param(iso::float2) p1, param(iso::float2) uv0 = iso::float2{0,0}, param(iso::float2) uv1 = iso::float2{1,1}) const;
	void				Draw(const iso::Texture &tex, iso::technique *technique, const ISO::Browser &b, int align) const;

	RenderRegion(iso::GraphicsContext &_ctx, param(iso::float4x4) _matrix, float _w, float _h);
};

struct WithRegionTrans : iso::saver<iso::float2x3> {
	WithRegionTrans(param(iso::float2x3) t) : iso::saver<iso::float2x3>(RenderRegion::region_trans, t) {}
};


struct menu;

enum MENU_MESSAGE {
	MMSG_TRIGGERUPDATE,
	MMSG_TRIGGERINIT,
	MMSG_TRIGGEREXIT,
	MMSG_TRIGGER,
	MMSG_TRIGGER2,
	MMSG_TRIGGER3,
	MMSG_CANCEL,

	MMSG_GETINTPARAMS,
	MMSG_GETFLOAT,
	MMSG_GETSTRING,
	MMSG_GETINDEXPARAMS,

	MMSG_CUSTOMRESERVE,
	MMSG_CUSTOMINIT,
	MMSG_CUSTOMUPDATE,
	MMSG_CUSTOMDRAW,
	MMSG_CUSTOMEXIT,

	MMSG_INPUTUPDATE,
	MMSG_NOTIFY,
	MMSG_CHANGED,
#ifdef ISO_EDITOR
	MMSG_EDITOR,
#endif
};

enum MENU_RETURNVAL {
	MIU_DEFAULT			= 0,
	MIU_CANCEL,			// B or back
	MIU_CANCELALL,
	MIU_DESELECT,
	MIU_TRIGGER,		// A or start
	MIU_TRIGGER2,		// X
	MIU_TRIGGER3,		// Y
	MIU_COMPLETEDALL,

	MIU_HITUP,
	MIU_HITDOWN,
	MIU_HITLEFT,
	MIU_HITRIGHT,

	MIU_HOLDUP,
	MIU_HOLDDOWN,
	MIU_HOLDLEFT,
	MIU_HOLDRIGHT,
	MIU_NEXT,
	MIU_PREV,

	MIU_INC,
	MIU_DEC,
	MIU_HOLDINC,
	MIU_HOLDDEC,

	MIU_NOTDEFAULT,
	MIU_ABORTUPDATE,

	MIU_FALSE			= 0,
	MIU_TRUE			= 1,

	MIU_NODRAW_ACTIVATE	= -2,
	MIU_NODRAW_TOGGLE	= -1,
	MIU_DRAW			= 0,
	MIU_DRAW_TOGGLE		= 1,
	MIU_DRAW_ACTIVATE	= 2,

	MIU_UPDATE_NONE		= -1,
};

struct mreturn {
	intptr_t	i;
	mreturn(MENU_RETURNVAL r = MIU_DEFAULT) : i(r) {}
	mreturn(const void *p)					: i(intptr_t(p)) {}
	mreturn(int x)							: i(x) {}
	operator intptr_t()					const { return i; }
	template<typename T> operator T*()	const { return (T*)i; }
};


struct MenuIntVals {
	static MenuIntVals *current;

	int			minval;
	int			maxval;
	int			inc;
	const char	*format;
	int			*pval;
	int			flags;

	enum { NOWRAP = 1, NOHOLD = 2, FAST = 4, CONTROL = 8, NOTIFY_CHANGED = 16, INDEX = 32, ALLOWSET = 64, USER = 0x100 };

	MenuIntVals(int *p) : minval(0), maxval(0), inc(1), format(0), pval(p), flags(0) {}

	bool	SetVal(int v);
	bool	Up()			{ return SetVal(*pval + inc); }
	bool	Down()			{ return SetVal(*pval - inc); }
	int		Value()	const	{ return *pval;	}

	void	SetLimits(int _min, int _max) {
		minval	= _min;
		maxval	= _max;
	}
	void	SetLimits(int _min, int _max, int _flags) {
		minval	= _min;
		maxval	= _max;
		flags	= _flags;
	}
	void	Set(int *p, int _min, int _max, const char *_format = NULL) {
		pval	= p;
		minval	= _min;
		maxval	= _max;
		format	= _format;
	}
	void	Set(int *p, int _min, int _max, int _inc, const char *_format = NULL) {
		pval	= p;
		minval	= _min;
		maxval	= _max;
		inc		= _inc;
		format	= _format;
	}
};

struct MenuIndexVals {
	enum { NOWRAP = 1, CYCLE = 2, FIXED = 4 };
	union {
		int		*index;
		int		value;
	};
	int		flags;
	void	set(int *p, int f = 0)	{ index = p; flags = f & ~FIXED;	}
	void	set(int i, int f = 0)	{ value = i; flags = f | FIXED;		}
	MenuIndexVals() : flags(0) { index = 0; }
};

struct MenuCustom {
	bool					called;
	const ISO::Browser		GetArgs()	const;
	void*					Allocated();
	iso::arbitrary&			User();
	MenuCustom() : called(false)	{}
};

struct MenuCustomReserve : MenuCustom {
	void	Reserve(MenuInstance *mi);
	void	Reserve(size_t size, size_t align = 4);
	template<typename T> void Reserve()		{ Reserve(sizeof(T), alignof(T)); }
};

struct MenuCustomInit : MenuCustom {
	int		ret;
	MenuCustomInit();
	void	Init(MenuInstance *mi);
	void*	Allocate(size_t size, size_t align = 4);
	template<typename T> void* Allocate()	{ return Allocate(sizeof(T), alignof(T)); }
};

struct MenuCustomExit : MenuCustom {
	void	Destroy(MenuInstance *mi);
};

struct MenuCustomDraw : MenuCustom {
	RenderRegion *rr;
	void	Draw(MenuInstance *mi, RenderRegion *rr, int state);
	void	Draw(MenuInstance *mi, RenderRegion *rr);
	void	Draw(MenuInstance *mi, int state);
	void	Draw(MenuInstance *mi);
};

struct MenuCustomUpdate : MenuCustom {
	mreturn Update(MenuInstance *mi);
};

#ifdef ISO_EDITOR
struct MenuEditor {
	const void		*p;
	void			*context;
	RenderRegion	*rr0;
	RenderRegion	*rr1;
	MenuEditor(const void *p, void *context, RenderRegion *rr0, RenderRegion *rr1) : p(p), context(context), rr0(rr0), rr1(rr1) {}
	MenuEditor*		me()	{ return this; }
};
#endif

class MenuValue {
	iso::uint8	dummy[64];
	void	init(MenuInstance *mi, iso::tag2 id, const ISO::Type *type, void *data, RenderRegion *rr);
public:
//	MenuValue(MenuInstance *mi, const ISO::ptr<iso::anything> &a);
	MenuValue(MenuInstance *mi, iso::tag2 id, const ISO::Type *type, void *data, RenderRegion *rr = 0) {
		init(mi, id, type, data, rr);
	}
	MenuValue(MenuInstance *mi, iso::tag2 id, ISO::ptr<void> p, RenderRegion *rr = 0) {
		init(mi, id, p.GetType(), p, rr);
	}
	MenuValue(MenuInstance *mi, ISO::ptr<void> p, RenderRegion *rr = 0) {
		init(mi, p.ID(), p.GetType(), p, rr);
	}
	template<typename T>MenuValue(MenuInstance *mi, iso::tag2 id, const T &t, RenderRegion *rr = 0) {
		init(mi, id, ISO::getdef<T>(), const_cast<T*>(&t), rr);
	}
	~MenuValue();
};

typedef iso::callback<mreturn(MenuInstance *mi, iso::tag id, MENU_MESSAGE msg, void *params)>	MenuDelegate;

class MenuInstance {
protected:
	MenuDelegate		delegate;
public:
	enum ACTIVE { DEFAULT = -1, ME = -2, DESELECT = -3};
	enum STATE	{ SELECTED = 1, TO = 2, BACK = 4, ACTIVE = 8};

	struct menu_data	*rootmenu, *currmenu, *topmenu;
	float				time;
	const char			*text;
	void				*values;
	const iso::Texture	*texture;
	int					index, active;
	mreturn				ret;
	iso::flags<STATE>	state;

	void		Update();
	void		Render0(const RenderRegion &rr);	// use current graphics state
	void		Render(const RenderRegion &rr);

	void		Kill();
	void		Init(const menu *_m, float time);
	void		PushMenu(const menu *submenu, int back = 0, bool imm = true);
	void		PopMenu(bool imm = true);
	void		PopMenuFromTop(bool imm = true);
	int			Depth()		const;
	void*		MenuData()	const;

	MenuDelegate SetDelegate(const MenuDelegate &delegate2) {
		MenuDelegate prev	= delegate;
		delegate			= delegate2;
		return prev;
	}
	mreturn	SendEvent(iso::tag id, MENU_MESSAGE msg, void *params) {
		return delegate(this, id, msg, params);
	}
	mreturn	SendEvent(iso::tag id, MENU_MESSAGE msg, void *params, int _state) {
		return iso::save(state, _state), delegate(this, id, msg, params);
	}
	mreturn	SendUpdate(iso::tag id, void *params) {
		return ret ? ret : SendEvent(id, MMSG_INPUTUPDATE, params);
	}

	bool		IsRunning()				const	{ return rootmenu != NULL; }
	void		SetTime(float _time)			{ time = _time;		}
	float		GetTime()				const	{ return time;		}
	void		SetText(const char *_text)		{ text = _text;		}
	const char*	GetText()						{ return text;		}
	void		SetIndex(int i)					{ index = i;		}
	int			GetIndex()				const	{ return index;		}
	void		SetActive(int i = ME)			{ active = i;		}
	int			GetActive()				const	{ return active;	}
	bool		SetTexture(const iso::Texture *t){ texture = t;	return !!t; }
	const iso::Texture* GetTexture()	const	{ return texture;	}

	bool		IsSelected()			const	{ return state.test(SELECTED);	}
	bool		IsMenuActive()			const	{ return state.test(ACTIVE);	}
	bool		IsBack()				const	{ return state.test(BACK);		}
	bool		IsCovered()				const;

	const ISO::Browser GetBrowser() const;

	MenuInstance(const MenuDelegate &_delegate);
	~MenuInstance()	{ Kill(); }
};

#endif // MENU_H
