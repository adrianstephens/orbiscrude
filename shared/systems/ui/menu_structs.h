#ifndef MENU_STRUCTS_H
#define MENU_STRUCTS_H

#include "menu.h"

typedef	iso::ISO_ptr<void>			mitem;
typedef iso::ISO_openarray<mitem>	mlist;
typedef ISO::ptr_string<char,32>	mstring;

struct _menu {
	enum FLAGS {
		NO_CANCEL		= 1,
		NO_CANCELALL	= 2,
		DRAW_UNDER		= 4,
		TRANS_OUT		= 8,
		POPUP			= 16,
		INACTIVE_SELECTED = 32,
	};
	mitem				item;
	iso::flags<FLAGS>	flags;
};

enum ARRANGE_FLAGS {
	ARR_HORIZONTAL		= 0,
	ARR_VERTICAL		= 1,
	ARR_SEPARATE_INPUTS	= 2,
	ARR_INDEX			= 4,
	ARR_NO_SUBBOXING	= 8,
	ARR_INHERIT_START	= 16,
	ARR_REVERSE_DIR		= 32,
	ARR_REVERSE_ORDER	= 64,
	ARR_BACKWARDS		= ARR_REVERSE_DIR | ARR_REVERSE_ORDER,
	ARR_SELECT_ALL		= 128,
	ARR_NO_WRAP0		= 256,
	ARR_NO_WRAP1		= 512,
	ARR_NO_WRAP			= 768,
	ARR_PROPORTIONS		= 1024,
	ARR_START			= 0x1000,
};

struct _mi_list : mlist {};

struct _mi_arrange {
	iso::flags<ARRANGE_FLAGS>	flags;
	mlist						list;
	iso::ISO_openarray<float>	sizes;
};

struct _mi_offset {
	float				x, y;
	float				scle;
	mitem				item;
};

struct _mi_box {
	float				x, y, w, h;
	float				scale;
	mitem				item;
};

struct _mi_textbox {
	iso::uint32			flags;
	float				x, y;
	mitem				item;
};

struct _mi_centre {
	float				w, h;
	mitem				item;
};

struct _mi_text {
	mstring				text;
	mitem				item;
};

struct _mi_gettext {
	mstring				funcid;
	mitem				item;
};

struct _mi_int {
	mstring				funcid;
	mitem				item;
};

struct _mi_colour {
	enum {
		F_OUTLINE		= 0x1,
		F_MULTIPLY		= 0x2,
		F_COPY_ALPHA	= 0x4,
	};
	iso::uint8			col[4];
	mitem				item;
	iso::uint8			flags;
};

struct _mi_font {
	iso::ISO_ptr<fontparams>	p;
	mitem				item;
};

struct _mi_print {
	iso::uint32			flags;
};

struct _mi_fill {
};

struct _mi_shader {
	iso::ISO_ptr<iso::technique>	technique;
	iso::anything		params;
	mitem				item;
};

struct _mi_texture {
//	Texture			tex;
	iso::ISO_ptr<void>	tex;
	iso::uint32			align;
};

struct _mi_button {
	iso::ISO_ptr<void>	tex;
};

struct _mi_trigger {
	mitem				item;
	mstring				funcid;
};

struct _mi_trigger_menu {
	mitem				item;
	iso::ISO_ptr<menu>	triggermenu;
	int					back;
};

struct _mi_sellist {
	mstring				funcid;
	iso::ISO_ptr<mlist>	list;
};

struct _mi_indexed {
	mstring				funcid;
	iso::ISO_ptr<mlist>	list;
};

struct _mi_ifselected {
	mitem				yes;
	mitem				no;
	mitem				init;
};

struct _mi_state {
	mitem				trans_from;
	mitem				trans_to;
	mitem				back_from;
	mitem				back_to;
};

struct _mi_set {
	REGION_PARAM		field;
	float				value;
	mitem				item;
};

struct _mi_set2 {
	mstring				funcid;
	REGION_PARAM		field;
	mitem				item;
};

enum CURVE_FLAGS {
	LC_EASY_IN_OUT, LC_EASY_IN, LC_EASY_OUT, LC_CUBIC, LC_LINEAR, LC_DISCRETE, _LC_MODE_MASK = 0x7,
	LC_LOOP				= 0x0008,
	LC_STOP 			= 0x0010,
	LC_KILLMENU			= 0x0020,
	LC_COVER			= 0x0040,
	LC_DESELECT			= 0x0080,
	LC_RELATIVE_START	= 0x0100,
	LC_RELATIVE_INIT	= 0x0200,
	LC_TRUNC			= 0x0400,
	LC_CLAMP_START		= 0x0800,
	LC_OFFSET_START		= 0x1000,
	LC_END_TRIGGER		= 0x2000,
	LC_COVER_ME			= 0x4000,

	LC_NO_LOOP			= 0x10000000,	// FlashToCurves loop suppression for buttons
	LC_DESELECT_ANIM	= 0x20000000	// FlashToCurves use 'hit' state as deselect animation
};
struct _mi_curve {
	REGION_PARAM			field;
	iso::ISO_openarray<iso::float2p>	control;
	mitem					item;
	iso::flags<CURVE_FLAGS>	flags;
};

struct _mi_curve2 {
	mstring					funcid;
	REGION_PARAM			field;
	iso::ISO_openarray<iso::float2p>	control;
	mitem					item;
	iso::flags<CURVE_FLAGS>	flags;
};

struct _mi_custom {
	mstring				funcid;
	mitem				item;
	iso::anything		args;
};

struct _mi_warp {
	iso::float2p		c[4];
	mitem				item;
};

struct _mi_param {
	mstring				id;
	mitem				item, process;
};
struct _mi_arg {
	mstring				id;
};

struct _mi_param2 {
	mstring				id;
	mitem				item, process;
};
struct _mi_arg2 {
	mstring				id;
};

struct _mi_vars {
	iso::anything		vars;
	mitem				item;
};

#endif // MENU_STRUCTS_H
