#include "scenegraph.h"
#include "iso/iso_convert.h"
#include "filetypes/2d/flash.h"
#include "maths/geometry.h"
#include "piece_wise.h"
#include "extra/xml.h"
#include "systems/ui/menu_structs.h"
#include "systems/ui/font.h"
#include "platformdata.h"
#include "packed_types.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Menu
//-----------------------------------------------------------------------------

#if defined PLAT_PC && defined ISO_EDITOR
#define MENUTYPE(x)	struct x : _##x {}; ISO_EXTTYPE(x);
#else
namespace ISO {
template<int N> struct menuitem_def : public TypeUserCompN<N> {
	menuitem_def(const char *_id) : TypeUserCompN<N>(_id, ISO::TypeUserCallback::WRITETOBIN)	{}
};
template<> struct def<_mi_list> : public ISO::TypeUser {
	def() : ISO::TypeUser("mi_list", ISO::getdef<mlist>(), ISO::TypeUserCallback::WRITETOBIN)	{}
};

template<> struct def<_mi_curve> : public menuitem_def<4> {
	typedef _mi_curve _S, _T;
	def() : menuitem_def<4>("mi_curve") {
	ISO_SETFIELDS(0, field, control, item, flags);
}};
template<> struct def<_mi_set> : public menuitem_def<3> {
	typedef _mi_set _S, _T;
	def() : menuitem_def<3>("mi_set") {
	ISO_SETFIELDS(0,field, value, item);
}};
template<> struct def<_mi_offset> : public menuitem_def<4> {
	typedef _mi_offset _S, _T;
	def() : menuitem_def<4>("mi_offset") {
	ISO_SETFIELDS(0,x, y, scle, item);
}};
template<> struct def<_mi_box> : public menuitem_def<6> {
	typedef _mi_box _S, _T;
	def() : menuitem_def<6>("mi_box") {
	ISO_SETFIELDS(0,x, y, w, h, scale, item);
}};
template<> struct def<_mi_texture> : public menuitem_def<2> {
	typedef _mi_texture _S, _T;
	def() : menuitem_def<2>("mi_texture") {
	ISO_SETFIELDT1(0,tex,Texture);
	ISO_SETFIELD(1,align);
}};

template<> struct def<_mi_text> : public menuitem_def<2> {
	typedef _mi_text _S, _T;
	def() : menuitem_def<2>("mi_text") {
	ISO_SETFIELDS(0,text, item);
}};

template<> struct def<_mi_gettext> : public menuitem_def<2> {
	typedef _mi_gettext _S, _T;
	def() : menuitem_def<2>("mi_gettext") {
	ISO_SETFIELDS(0,funcid, item);
}};

template<> struct def<_mi_warp> : public menuitem_def<2> {
	typedef _mi_warp _S, _T;
	def() : menuitem_def<2>("mi_warp") {
	ISO_SETFIELDS(0,c, item);
}};

template<> struct def<_mi_trigger> : public menuitem_def<2> {
	typedef _mi_trigger _S, _T;
	def() : menuitem_def<2>("mi_trigger") {
	ISO_SETFIELDS(0,item, funcid);
}};

template<> struct def<_mi_ifselected> : public menuitem_def<3> {
	typedef _mi_ifselected _S, _T;
	def() : menuitem_def<3>("mi_ifselected") {
	ISO_SETFIELDS(0,yes, no, init);
}};

template<> struct def<_mi_fill> : public menuitem_def<0> {
	typedef _mi_fill _S, _T;
	def() : menuitem_def<0>("mi_fill") {
}};

template<> struct def<_mi_colour> : public menuitem_def<3> {
	typedef _mi_colour _S, _T;
	def() : menuitem_def<3>("mi_colour") {
	ISO_SETFIELDS(0,col, item, flags);
}};
template<> struct def<_mi_print> : public menuitem_def<1> {
	typedef _mi_print _S, _T;
	def() : menuitem_def<1>("mi_print") {
	ISO_SETFIELD(0,flags);
}};
template<> struct def<_mi_custom> : public menuitem_def<3> {
	typedef _mi_custom _S, _T;
	def() : menuitem_def<3>("mi_custom") {
	ISO_SETFIELDS(0,funcid, item, args);
}};

} // namespace ISO
#define MENUTYPE(x)	typedef _##x x;
#endif

MENUTYPE(mi_list)
MENUTYPE(mi_arrange)
MENUTYPE(mi_offset)
MENUTYPE(mi_box)
MENUTYPE(mi_textbox)
MENUTYPE(mi_centre)
MENUTYPE(mi_text)
MENUTYPE(mi_gettext)
MENUTYPE(mi_int)
MENUTYPE(mi_colour)
MENUTYPE(mi_font)
MENUTYPE(mi_print)
MENUTYPE(mi_fill)
MENUTYPE(mi_shader)
MENUTYPE(mi_texture)
MENUTYPE(mi_trigger)
MENUTYPE(mi_trigger_menu)
MENUTYPE(mi_sellist)
MENUTYPE(mi_indexed)
MENUTYPE(mi_ifselected)
MENUTYPE(mi_state)
MENUTYPE(mi_set)
MENUTYPE(mi_set2)
MENUTYPE(mi_curve)
MENUTYPE(mi_curve2)
MENUTYPE(mi_custom)
MENUTYPE(mi_warp)
MENUTYPE(mi_param)
MENUTYPE(mi_arg)
MENUTYPE(mi_param2)
MENUTYPE(mi_arg2)
MENUTYPE(mi_vars)

ISO_ptr<void> MakeSet(float v, REGION_PARAM field, ISO_ptr<void> item) {
	switch (field) {
		case REGION_RED: case REGION_GREEN: case REGION_BLUE: case REGION_ALPHA:
		case REGION_RED_OUTLINE: case REGION_GREEN_OUTLINE: case REGION_BLUE_OUTLINE: case REGION_ALPHA_OUTLINE:
		case REGION_SCALE_X: case REGION_SCALE_Y: case REGION_SCALE_X1: case REGION_SCALE_Y1: case REGION_SCALE1: case REGION_SCALE:
			if (abs(v - 1) < 0.01f)
				return item;
			break;
		default:
			if (abs(v) < 0.001f)
				return item;
	}
	ISO_ptr<mi_set>	set(item.ID());
	set->field	= field;
	set->value	= v;
	set->item	= item;
	return set;
}

ISO_ptr<void> MakeCurve(const dynamic_array<pair<float, float> > &a, REGION_PARAM field, ISO_ptr<void> item, int flags, bool *curve = 0) {
	size_t	n		= a.size();
	if (n == 0)
		return item;

	if (n == 1)
		return MakeSet(a[0].a, field, item);

	if (curve)
		*curve = true;

	ISO_ptr<mi_curve>	c(item.ID());
	c->field	= field;
	c->item		= item;
	c->flags	= flags;

	float2p	*d = c->control.Create(uint32(n));
	for (size_t i = 0; i < n; i++)
		d[i] = float2{a[i].b, a[i].a};
	return c;
}

ISO_ptr<void> AnimationToCurves(ISO_ptr<Animation> anim, ISO_ptr<void> item) {
	ISO::Browser			b(anim);
	int					flags	= LC_LINEAR;
	if (ISO::Browser	b2 = b["pos"]) {
		int		n	= b2.Count();
		float3p	*p	= b2[0];
		dynamic_array<pair<float,float> >	a(n);

		for (int i = 0; i < n; i++) {
			a[i].a = p[i].x;
			a[i].b = i / 30.f;
		}
		item = MakeCurve(Optimise(a, 0.001f), REGION_X2, item, flags);

		a.resize(n);
		for (int i = 0; i < n; i++) {
			a[i].a = -p[i].z;
			a[i].b = i / 30.f;
		}
		item = MakeCurve(Optimise(a, 0.001f), REGION_Y2, item, flags);

		a.resize(n);
		for (int i = 0; i < n; i++) {
			a[i].a = p[i].y;
			a[i].b = i / 30.f;
		}
		item = MakeCurve(Optimise(a, 0.001f), REGION_Z, item, flags);
	}
	return item;
}

uint32 ConvertControlCodes(string_accum &out, string_scan &&text) {
	static const char *codes[] = {
		0,				"COLOUR",		"HMOVE",			"VMOVE",
		"HSET",			"CENTRE",		"RIGHTJ",			"VSET",
		"BACKSPACE",	"TAB",			"LF",				"CURSOR_DOWN",
		"CURSOR_UP",	"CR",			"SET_WIDTH",		"ALIGN_LEFT",
		"ALIGN_RIGHT",	"ALIGN_CENTRE",	"ALIGN_JUSTIFY",	"SET_TAB",
		"SET_INDENT",	"NEWLINE",		"RESTORECOL",		"SCALE",
		"ALPHA",		"SOFTRETURN",	"LINESPACING",
	};
	static struct {const char *name; uint32 val;} flags[] = {
		{"WRAP",		TF_WRAP			},
		{"NOSQUISH_H",	TF_NOSQUISH_H	},
		{"NOSQUISH_V",	TF_NOSQUISH_V	},
		{"NOSQUISH",	TF_NOSQUISH		},
		{"IGNORE_COL",	TF_IGNORE_COL	},
		{"MULT_COL",	TF_MULT_COL		},
		{"CLIP_H",		TF_CLIP_H		},
		{"CLIP_V",		TF_CLIP_V		},
		{"CLIP",		TF_CLIP			},
	};

	uint32	outflags = 0;
	const char *p = text.getp();
	while (const char *p2 = text.scan('<')) {
		if (const char *p3 = text.scan('>')) {
			out << str(p, p2 - p);
			bool	found = false;
			for (int i = 1; !found && i < num_elements(codes); i++) {
				size_t	code_len = strlen(codes[i]);
				if ((p2 + code_len + 1 == p3 || p2[code_len + 1] == ',') && str(p2 + 1).begins(codes[i])) {
					out << char(i);
					p2 += code_len + 1;
					while (*p2++ == ',') {
						uint8	v;
						p2 += from_string(p2, v);
						out << char(v + int(v == 0));
					}
					found = true;
				}
			}
			for (int i = 0; !found && i < num_elements(flags); i++) {
				if (str(flags[i].name) == str(p2 + 1, p3 - p2 - 1)) {
					outflags |= flags[i].val;
					found = true;
				}
			}
			if (!found)
				out << str(p2, p3 + 1 - p2);
			p2 = p3;
		}
		p = p2 + 1;
	}
	out << p;
	return outflags;
}

template<typename T, int N> struct stack {
	T	a[N], *sp;
	stack() : sp(a) {}
	void		push(const T &t)	{ *sp++ = t;			}
	T			pop()				{ return *--sp;			}
	const T&	top()	const		{ return sp[-1];		}
	operator const T&() const		{ return top();			}
	bool		empty()	const		{ return sp == a;		}
	int			depth()	const		{ return int(sp - a);	}
};

class HTMLtext {
	buffer_accum<1024>	out;
	stack<rgb8, 16>		col;
	uint32				align;
	float				size;

public:
	HTMLtext(istream_ref in);

	operator const char*()			{ return out.term(); }
	uint32		GetAlign()	const	{ return align;		}
	const rgb8&	GetColour()	const	{ return col;		}
	float		GetSize()	const	{ return size;		}
};

HTMLtext::HTMLtext(istream_ref in) : align(FA_LEFT) {
//	bold.push(false);
//	italic.push(false);

	XMLreader::Data	data;
	XMLreader				xml(in);

	xml.SetFlag(XMLreader::UNQUOTEDATTRIBS);
	xml.SetFlag(XMLreader::NOEQUALATTRIBS);
	xml.SetFlag(XMLreader::SKIPUNKNOWNENTITIES);
	xml.SetFlag(XMLreader::SKIPBADNAMES);
	xml.SetFlag(XMLreader::GIVEWHITESPACE);

	while (XMLreader::TagType tag = xml.ReadNext(data)) {
		switch (tag) {
			case XMLreader::TAG_BEGIN: case XMLreader::TAG_BEGINEND:
				if (data.Is("font")) {
					for (auto &i : data.Attributes()) {
						if (i.name == "color") {
							rgb8	c;
							uint32	r, g, b;
							string_scan(i.value).move(1) >> formatted(r, FORMAT::HEX, 2) >> formatted(g, FORMAT::HEX, 2) >> formatted(b, FORMAT::HEX, 2);
							c = rgb8{r,g,b};
							if (!col.empty() && any(c != col.top()))
								out << cc_colour(c);
							col.push(c);
						} else if (i.name == "size") {
							i.value.read(size);
						}
					}

				} else if (data.Is("p")) {
					if (out.length())
						out << '\n';
					for (auto &i : data.Attributes()) {
						if (i.name == "align") {
							FontAlign	curr = FontAlign(align & 3), prev = curr;
							if (i.value == "left")
								curr = FA_LEFT;
							else if (i.value == "right")
								curr = FA_RIGHT;
							else if (i.value == "center")
								curr = FA_CENTRE;
							else if (i.value == "justify")
								curr = FA_JUSTIFY;
							if (curr != prev) {
								out << curr;//_CC_ALIGN + curr;
								align = (align & ~3) | curr;
							}
						}
					}
				} else if (data.Is("i")) {
					align	|= TF_ITALIC;

				} else if (data.Is("b")) {
					align	|= TF_BOLD;

				}
				break;

			case XMLreader::TAG_END:
				if (data.Is("font") && col.depth() > 1)
					col.pop();
				break;

			case XMLreader::TAG_CONTENT:
				align |= ConvertControlCodes(out, data.Content());
				break;
		}
	}
}


float get_scale(const mi_box *box) { return box->scale ? box->scale : 1; }

struct FlashToCurvesObject {
	typedef	pair<float, float>	key;
	typedef	dynamic_array<key>	keys;

	bool	uniform;
	keys	scalex, scaley, transx, transy, rot, alpha, brightness;
	keys	glow_strength, glow_size;

	FlashToCurvesObject(ISO_ptr<void> item) : uniform(true) {}
	void	AddFrame(float f, flash::Object *obj);
	ISO_ptr<void> Done(ISO_ptr<void> item, int flags);
};

void FlashToCurvesObject::AddFrame(float f, flash::Object *obj) {
	float	sx	= len(float2{obj->trans[0][0], obj->trans[0][1]});
	float	sy	= len(float2{obj->trans[1][0], obj->trans[1][1]});
	scalex.emplace_back(sx, f);
	scaley.emplace_back(sy, f);
	uniform		= uniform && abs(sx / sy - 1) < 0.01f;

	float	r	= atan2(obj->trans[0][1], obj->trans[0][0]) * (180 / pi);
	if (!rot.empty()) {
		float	d = (rot.back().a - r) / 360;
		r += iso::trunc(d + iso::copysign(0.5f, d)) * 360;
	}
	new(rot)	key(r, f);
	new(transx)	key(obj->trans[2][0], f);
	new(transy)	key(obj->trans[2][1], f);

	float	b	= 0;
	bool	mul_same = obj->col_trans[0].x == obj->col_trans[0].y && obj->col_trans[0].x == obj->col_trans[0].z;
	bool	add_same = obj->col_trans[1].x == obj->col_trans[1].y && obj->col_trans[1].x == obj->col_trans[1].z;
	if (mul_same && add_same)
		b = obj->col_trans[1].x == 0 ? obj->col_trans[0].x - 1 : obj->col_trans[1].x;

	new(brightness)	key(b, f);
	new(alpha)		key((float)obj->col_trans[0][3], f);

	if (ISO::Browser	b = ISO::Browser(obj->filters["GlowFilter"])) {
		new (glow_strength)	key(min(b["strength"].GetFloat(0) / 8, 1.f), f);
		new (glow_size)		key(b["size"][0].GetFloat(0) / 2, f);
	}
}

ISO_ptr<void> FlashToCurvesObject::Done(ISO_ptr<void> item, int flags) {
	ISO_ptr<mi_box>	box;
	float			box_scale = 1, box_size = 100, cx = 0, cy = 0;

	if (item.IsType<mi_box>(ISO::MATCH_NOUSERRECURSE)) {
		box			= item;
		item		= box->item;
		box_scale	= get_scale(box);
		box_size	= max(box->w, box->h) / box_scale;

		cx			= box->x + box->w / 2;
		cy			= box->y + box->h / 2;
	}

	bool	curve	= false;
	Optimise(glow_strength, 0.01f);
	if (glow_strength.size() != 1 || glow_strength[0].a != 1)
		item = MakeCurve(glow_strength, REGION_PARAMS, item, flags, &curve);

	item = MakeCurve(Optimise(glow_size, 0.01f),	REGION_PARAM(REGION_PARAMS + 1),item, flags, &curve);
	item = MakeCurve(Optimise(alpha, 0.01f),		REGION_ALPHA,					item, flags, &curve);
	item = MakeCurve(Optimise(brightness, 0.01f),	REGION_BRIGHTNESS,				item, flags, &curve);

	//calculate translation as done first
	keys	ptransx, ptransy;
	for (int j = 0; j < transx.size(); j++) {
		float	f	= transx[j].b;
		float	s, c;
		sincos(rot[j].a * (pi / 180), &s, &c);
		float	tx	= transx[j].a, ty = transy[j].a;
		float	sx	= scalex[j].a, sy = scaley[j].a;

		float	x	= (c * cx * sx - s * cy * sy) + tx;
		float	y	= (s * cx * sx + c * cy * sy) + ty;

		ptransx.emplace_back((x - cx) / box_scale, f);
		ptransy.emplace_back((y - cy) / box_scale, f);
	}

	item = MakeCurve(Optimise(rot, 1.0f), box ? REGION_ROT : REGION_ROT_O, item, flags, &curve);

	float	scale = 1;
	if (uniform) {
		Optimise(scalex, 0.01f);
		if (scalex.size() == 1) {
			if (abs(scalex[0].a - 1) > 0.01f) {
				scale = scalex[0].a;
				if (box) {
					box->w		*= scale;
					box->h		*= scale;
					box->x		= cx - box->w / 2;
					box->y		= cy - box->h / 2;
					box->scale	= box_scale * scale;
					scale		= 1;
				}
			}
		} else {
			item = MakeCurve(scalex, box ? REGION_SCALE1 : REGION_SCALE, item, flags, &curve);
		}
	} else {
		item = MakeCurve(Optimise(scalex, 0.01f), box ? REGION_SCALE_X1 : REGION_SCALE_X, item, flags, &curve);
		item = MakeCurve(Optimise(scaley, 0.01f), box ? REGION_SCALE_Y1 : REGION_SCALE_Y, item, flags, &curve);
	}

	Optimise(ptransx, max(Extent(ptransx), box_size) / 100);
	Optimise(ptransy, max(Extent(ptransy), box_size) / 100);

	if (ptransx.size() == 1 && ptransy.size() == 1) {
		if (!curve && ptransx[0].b > 0) {
			ISO_ptr<mi_curve>	c(NULL);
			c->field	= REGION_PARAMS;
			c->item		= item;
			c->flags	= flags & ~(LC_LOOP | LC_STOP | LC_KILLMENU | LC_COVER | LC_DESELECT);
			c->control.Create(1)[0] = float2{ptransx[0].b, 0};
			item		= c;
		}
		if (box) {
			box->x += ptransx[0].a * box_scale;
			box->y += ptransy[0].a * box_scale;
			box->item = item;
			return box;
		}

		float	x = ptransx[0].a, y = ptransy[0].a;
		if (abs(x) < 0.1f && abs(y) < 0.1f)
			return MakeSet(scale, REGION_SCALE, item);

		ISO_ptr<mi_offset>	off(0);
		off->x		= x;
		off->y		= y;
		off->scle	= scale;
		off->item	= item;
		return off;
	}

	if (box) {
		if (ptransx.size() == 1)
			box->x += ptransx[0].a * box_scale;
		else
			item = MakeCurve(ptransx, REGION_X2, item, flags, &curve);

		if (ptransy.size() == 1)
			box->y += ptransy[0].a * box_scale;
		else
			item = MakeCurve(ptransy, REGION_Y2, item, flags, &curve);

		box->item = item;
		return box;
	}

	item = MakeSet(scale, REGION_SCALE, item);
	item = MakeCurve(ptransx, REGION_X2, item, flags, &curve);
	item = MakeCurve(ptransy, REGION_Y2, item, flags, &curve);
	return item;
}

mi_curve *FindCurve(ISO_ptr<void> p, REGION_PARAM f) {
	do {
		if (p.IsType("mi_curve")) {
			mi_curve *c = p;
			if (c->field == f)
				return c;
		}
	} while (p = *ISO::Browser(p)["item"]);
	return 0;
}

void OffsetCurve(mi_curve *curve, float v) {
	float2p	*c	= curve->control;
	for (int n = curve->control.Count(); n--; )
		(c++)->y += v;
}

float CurveMin(const mi_curve *curve) {
	const float2p	*c	= curve->control;
	float			v	= c->y;
	for (int n = curve->control.Count(); --n; )
		v = min(v, (++c)->y);
	return v;
}

float CurveMax(const mi_curve *curve) {
	const float2p	*c	= curve->control;
	float			v	= c->y;
	for (int n = curve->control.Count(); --n; )
		v = max(v, (++c)->y);
	return v;
}

ISO_ptr<void> CombineBoxes(ISO_ptr<mi_list> list) {
	float		min_x	= maximum, min_y = maximum, max_x = -maximum, max_y = -maximum;
	float		min_cx	= maximum, min_cy = maximum, max_cx = -maximum, max_cy = -maximum;
	float		max_w	= 0, max_h = 0;
	int			num		= list->Count();

	for (int i = 0; i < num; i++) {
		ISO_ptr<void>	p = (*list)[i];
		if (p.IsType<mi_box>(ISO::MATCH_NOUSERRECURSE)) {
			mi_box	*box	= p;
			float	scale	= get_scale(box);
			max_w	= max(max_w, box->w);
			max_h	= max(max_h, box->h);

			if (mi_curve *cx = FindCurve(box->item, REGION_X2)) {
				min_cx	= min(min_cx, box->x + CurveMin(cx) * scale);
				max_cx	= max(max_cx, box->x + CurveMax(cx) * scale);
			} else {
				min_x	= min(min_x, box->x);
				max_x	= max(max_x, box->x + box->w);
			}
			if (mi_curve *cy = FindCurve(box->item, REGION_Y2)) {
				min_cy	= min(min_cy, box->y + CurveMin(cy) * scale);
				max_cy	= max(max_cy, box->y + CurveMax(cy) * scale);
			} else {
				min_y	= min(min_y, box->y);
				max_y	= max(max_y, box->y + box->h);
			}

		} else if (p.IsType<mi_offset>(ISO::MATCH_NOUSERRECURSE)) {
			mi_offset	*off	= p;
			if (mi_curve *cx = FindCurve(off->item, REGION_X2)) {
				min_cx	= min(min_cx, off->x + CurveMin(cx));
				max_cx	= max(max_cx, off->x + CurveMax(cx));
			} else {
				min_x	= min(min_x, off->x);
			}
			if (mi_curve *cy = FindCurve(off->item, REGION_Y2)) {
				min_cy	= min(min_cy, off->y + CurveMin(cy));
				max_cy	= max(max_cy, off->y + CurveMax(cy));
			} else {
				min_y	= min(min_y, off->y);
			}

		} else {
			return list;
		}
	}
	if (min_x == (float)maximum) {
		min_x = max(min_cx, 0);
		max_x = max(max_x, min_x + max_w);
	}
	max_w = max_x - min_x;

	if (min_y == (float)maximum) {
		min_y = max(min_cy, 0);
		max_y = max(max_y, min_y + max_h);
	}
	max_h = max_y - min_y;

	for (int i = 0; i < num; i++) {
		ISO_ptr<void>	&p = (*list)[i];
		if (p.IsType<mi_box>(ISO::MATCH_NOUSERRECURSE)) {
			mi_box	*box	= p;
			float	scale	= get_scale(box);
			if (mi_curve *cx = FindCurve(box->item, REGION_X2)) {
				OffsetCurve(cx, (box->x - min_x) / scale);
				box->x = 0;
			} else {
				box->x -= min_x;
			}
			if (mi_curve *cy = FindCurve(box->item, REGION_Y2)) {
				OffsetCurve(cy, (box->y - min_y) / scale);
				box->y = 0;
			} else {
				box->y -= min_y;
			}
			if (box->w == max_w && box->h == max_h) {
				tag2	id = p.ID();
				if (box->x == 0 && box->y == 0) {
					if (box->scale == 0) {
						p = box->item;
						p.SetID(id);
					} else {
						ISO_ptr<mi_set>	set(id);
						set->field	= REGION_SCALE;
						set->value	= box->scale;
						set->item	= box->item;
						p = set;
					}
				} else {
					ISO_ptr<mi_offset>	off(id);
					off->x		= box->x;
					off->y		= box->y;
					off->scle	= scale;
					off->item	= box->item;
					p = off;
				}
			}
		} else {
			mi_offset	*off	= p;
			if (mi_curve *cx = FindCurve(off->item, REGION_X2)) {
				OffsetCurve(cx, off->x - min_x);
				off->x = 0;
			} else {
				off->x -= min_x;
			}
			if (mi_curve *cy = FindCurve(off->item, REGION_Y2)) {
				OffsetCurve(cy, off->y - min_y);
				off->y = 0;
			} else {
				off->y -= min_y;
			}
			if (off->x == 0 && off->y == 0) {
				tag2	id = p.ID();
				p = off->item;
				p.SetID(id);
			}
		}
	}

	ISO_ptr<mi_box>	box(0);
	box->x		= min_x;
	box->y		= min_y;
	box->w		= max_w;
	box->h		= max_h;
	box->scale	= 0;
	box->item	= list;
	return box;
}

ISO_ptr<void> CollapseList(ISO_ptr<mi_list> list) {
	switch (list->Count()) {
		case 0:	return ISO_NULL;
		case 1:	return (*list)[0];
		default:return CombineBoxes(list);
	}
}

bool SetItem(ISO::Browser bitem, ISO_ptr<void> item) {
	if (bitem.Set(item))
		return true;
	if (bitem.Is<ISO_ptr<mi_list> >()) {
		ISO_ptr<mi_list>	list(0);
		list->Append(item);
		return bitem.Set(list);
	}
	return false;
}

ISO::Browser FindInsertion(ISO::Browser bcom) {
	for (;;) {
		ISO::Browser	b;
		if (!(b = bcom["item"]) && !(b = bcom["list"]))
			return b;

		ISO_ptr<void> p = b.Get(ISO_NULL);
		if (!p)
			return b;

		b.Set(Duplicate(p));
		bcom = *b;

		if (bcom.SkipUser().GetType() == ISO::OPENARRAY) {
			for (int i = 0, n = bcom.Count(); i < n; i++) {
				if (b = FindInsertion(bcom[i]))
					return b;
			}
		}
	}
}

ISO_ptr<void> InsertCommand(ISO_ptr<void> com, ISO_ptr<void> item) {
	if (ISO::Browser bins = FindInsertion(ISO::Browser(com))) {
		if (item.IsType<mi_box>(ISO::MATCH_NOUSERRECURSE)) {
			ISO_ptr<mi_box>	box = item;
			SetItem(bins, box->item);
			box->item = com;
			return item;
		}

		SetItem(bins, item);
		return com;
	}

	ISO::Browser	b = ISO::Browser(item);
	if (!b.Check("item"))
		return com;

	do {
		b	= b["item"];
	} while (b.Check("item"));

	if ((*b).Is("mi_list")) {
		ISO_ptr<mi_list>	list = *b;
		b.Set(com);
		for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
			int	x = list->GetIndex(i.GetName());
			if (x >= 0)
				i->Set((*list)[x]);
		}
		return item;
	}

	b.Set(com);
	return item;
}

ISO_ptr<void> Sequence(ISO_ptr<void> p0, ISO_ptr<void> p1) {
	ISO_ptr<mi_list>	list(0);
	list->Append(p0);
	list->Append(p1);
	ISO_ptr<void> p = CombineBoxes(list);

	if ((*list)[0].IsType("mi_curve")) {
		mi_curve	*c	= (*list)[0];
		float		len	= c->control.back().x;
		c->flags.set(LC_STOP);

		ISO_ptr<mi_curve>	d(0);
		d->control.Append() = float2{len, 0};
		d->field	= REGION_PARAMS;
		d->flags	= LC_RELATIVE_INIT;
		d->item		= (*list)[1];
		(*list)[1]	= d;
	}

	return p;
}

bool HasGetText(const ISO_ptr<void> &p) {
	return p && p.IsType<mi_gettext>();
}

struct FlashColours {
	float4p	cols[2];

	FlashColours() {
		cols[0] = one;
		cols[1] = one;
	}

	ISO_ptr<void>	AddCommands(const FlashColours &prev, ISO_ptr<void> item) {
		for (int i = 0; i < 2; i++) {
			if (any(cols[i] != prev.cols[i])) {
				ISO_ptr<mi_colour>	col(0);
				(rgba8&)col->col	= cols[i];
				col->flags			= i ? mi_colour::F_OUTLINE : 0;
				if (cols[i].w == 1)
					col->col[3] = 0;

				if (item.IsType<mi_box>(ISO::MATCH_NOUSERRECURSE)) {
					mi_box	*box = item;
					col->item	= box->item;
					box->item	= col;
				} else {
					col->item	= item;
					item		= col;
				}
			}
		}
		return item;
	}
};

struct FlashRearrange {
	struct Item {
		int				start;
		flash::Object		*best_obj;
		float			best_dist;
		dynamic_array<flash::Object*>	anim;

		ISO_ptr<flash::Object>	Obj()		const	{ return (ISO_ptr<flash::Object>&)anim.front(); }
		flash::Object				*Last()		const	{ return anim.empty() ? NULL : anim.back(); }
		void					EndFrame()			{ anim.push_back(best_obj); best_dist = 1e38f; best_obj = 0; }
	};
	dynamic_array<Item>		array;

	int		FindClosest(flash::Object *obj0);
	void	Add(flash::Object *obj, int f);
	void	EndFrame();

	FlashRearrange(const flash::Movie &movie, int sf, int nf);
};

void FlashRearrange::EndFrame() {
	for (size_t i = 0, n = array.size(); i < n; i++)
		array[i].EndFrame();
}

void FlashRearrange::Add(flash::Object *obj, int f) {
	Item	*it = new(array) Item;
	it->start		= f;
	it->best_dist	= 0;
	it->best_obj	= obj;
}

int FlashRearrange::FindClosest(flash::Object *obj0) {
	size_t			n	= array.size();
	float			d	= 1e38f;
	int				i	= -1;
	const float2x3p	&t0	= obj0->trans;

	for (size_t i1 = 0; i1 < n; i1++) {
		flash::Object	*obj1 = array[i1].Last();
		if (obj1 && obj1->character == obj0->character) {
			const float2x3p	&t1	= obj1->trans;
			float	d1 = len2(float2(t1.z) - float2(t0.z));
			if (d1 < d) {
				if (d1 < array[i1].best_dist) {
					d	= d1;
					i	= int(i1);
				}
			}
		}
	}
	if (i >= 0)
		array[i].best_dist	= d;
	return i;
}

FlashRearrange::FlashRearrange(const flash::Movie &movie, int sf, int nf) {
	for (int f = 0; f < nf; f++) {
		flash::Frame	*frame		= movie[f + sf];

		for (int s = 0, ns = frame->Count(); s < ns; s++) {
			flash::Object	*obj	= (*frame)[s];
			int			i		= FindClosest(obj);
			while (i >= 0) {
				swap(obj, array[i].best_obj);
				if (!obj)
					break;
				i = FindClosest(obj);
			}
			if (obj)
				Add(obj, f);
		}
		EndFrame();
	}
}

int	check_only(const float *p, int n, float threshold = 0.01f) {
	for (int i = 0; i < n; i++) {
		if (abs(p[i]) > threshold) {
			for (int j = i + 1; j < n; j++) {
				if (abs(p[j]) > threshold)
					return -1;
			}
			return i;
		}
	}
	return n;
}

struct FlashToMenu {
	float4p				rect;
	float4p				bg;
	float				fps;
	const anything						&overrides;
	flags<CURVE_FLAGS>					curve_flags;
	const ISO_openarray<ISO_ptr<uint32>> &curve_overrides;

	int					blend;
	float				glow;
	bool				touch;
	bool				has_override;

	FlashColours		cols;

	struct chain {
		chain		*back, *&top;
		ISO_ptr<flash::Object>	obj;
		chain(chain *&_top, const ISO_ptr<flash::Object> &_obj) : back(_top), top(_top), obj(_obj) {
			_top = this;
		}
		~chain() {
			top = back;
		}
	} *top;

	FlashToMenu(const float4p &rect, const float4p &bg, float fps, const anything &overrides, uint32 curve_flags, const ISO_openarray<ISO_ptr<uint32>> &curve_overrides, int blend = 0)
		: rect(rect), bg(bg), fps(fps)
		, overrides(overrides), curve_flags(curve_flags), curve_overrides(curve_overrides)
		, blend(blend), glow(0), top(0)
	{
		touch			= str(ISO::root("variables")["flashsemantics"].GetString()) == "touch";
		has_override	= false;
	}

	ISO_ptr<void> Process(FlashRearrange &fr);
	ISO_ptr<void> Movie(const flash::Movie &movie);
	ISO_ptr<void> Item(ISO_ptr<void> chr, ISO_ptr<flash::Object> obj);
};

ISO_ptr<void> FlashToMenu::Item(ISO_ptr<void> chr, ISO_ptr<flash::Object> obj) {
	chain		ch(top, obj);

	ISO::Browser	glowfilter = ISO::Browser(obj->filters["GlowFilter"]);
	cols.cols[0] = one;
	cols.cols[1] = one;
	if (glowfilter) {
		glow			+= glowfilter["strength"].GetFloat(0);
		rgba8	temp(one);
		cols.cols[1]	= *(glowfilter["colour"].Get(&temp));
	}

	if (chr.IsType<flash::Movie>()) {
		return Movie(*(flash::Movie*)chr);

	} else if (chr.IsType<flash::Shape>()) {
		flash::Shape			*shape	= chr;
		ISO_ptr<bitmap>		bm		= ISO_conversion::convert<bitmap>(((flash::Bitmap*)shape->a)->a);
		ISO_ptr<void>		item;

#if 0
		ISO_rgba			monocol;
		if (IsMonochrome(bm->All(), 0.01f, &monocol)) {
			cols.cols[0] = cols.cols[0] * (float4p&)HDRpixel(monocol);
			bm	= Duplicate(bm);
			MakeMonochrome(*bm, monocol);
		}
#endif

		bool	overridden = false;
		for (chain *c = top; c; c = c->back) {
			ISO_ptr<flash::Object> obj = c->obj;
			for (int i = 0, n = obj->filters.Count(); i < n; i++) {
				if (ISO_ptr<void> o = overrides[obj->filters[i].ID()]) {
					overridden		= true;
					item			= Duplicate(o);
					ISO::Browser	b	= ISO::Browser(item);
					for (;;) {
						ISO::Browser	b2	= b.FindByType<ISO_ptr<bitmap> >(ISO::MATCH_MATCHNULLS);
						if (!b2)
							b2	= b.FindByType<Texture>(ISO::MATCH_MATCHNULLS);
						if (!b2)
							break;
						if (!*(iso_ptr32<void>*)b2) {
							b2.Set(bm);
							break;
						}
						b2.Set((*b2).Duplicate());
						b = *b2;
					}
				}
			}
		}

		int	i	= int(shape->b[0].x > shape->b[1].x) ^ (int(shape->b[0].y > shape->b[2].y) * 3);
		quadrilateral	q(
			position2((float2)shape->b[i]),
			position2((float2)shape->b[(i + 1) & 3]),
			position2((float2)shape->b[(i + 3) & 3]),
			position2((float2)shape->b[(i + 2) & 3])
		);
		position2	centre	= q.centre();
		float2		e0		= q.pt1() - q.pt0(), e1 = q.pt2() - q.pt0();
		float		a		= q.area(), w = len(e0), h = len(e1);
		position2	p0		= centre - float2{w, h} / 2;

		ISO_ptr<mi_box>	box(0);
		box->x		= p0.v.x;
		box->y		= p0.v.y;
		box->w		= w;
		box->h		= h;
		box->scale	= 0;

		if (!overridden) {
			if (has_override) {
				ISO_ptr<mi_texture>	tex(0);
				tex->tex	= bm;
				tex->align	= 0;
				switch (blend) {
					case flash::Object::multiply:	tex->align = _TALB_MULTIPLY; break;
					default:						tex->align = 0; break;
				}
				item		= tex;
			} else {
				iso::rect	rects[16];
				int		nrects = FindBitmapRegions(*bm, rects);
				if (nrects > 1) {
					ISO_ptr<mi_list>	list(0);
					for (int i = 0; i < nrects; i++) {
						auto		&r	= rects[i];
						ISO_ptr<bitmap>	bm2 = Duplicate(bm);
						bm2->Crop(r.a.x, r.a.y, r.extent().x, r.extent().y);

						ISO_ptr<mi_texture>	tex(0);
						tex->tex	= bm2;
						switch (blend) {
							case flash::Object::multiply: tex->align = _TALB_MULTIPLY; break;
							default:					tex->align = 0; break;
						}
						ISO_ptr<mi_box>		box(0);
						box->x		= p0.v.x + w * r.a.x / bm->Width();
						box->y		= p0.v.y + h * r.a.y / bm->Height();
						box->w		= w * r.extent().x / bm->Width();
						box->h		= h * r.extent().y / bm->Height();
						box->scale	= 0;
						box->item	= tex;
						list->Append(box);
					}
					item	= list;
				} else {
					auto	&r	= rects[0];
					ISO_ptr<bitmap>	bm2 = Duplicate(bm);
					bm2->Crop(r.a.x, r.a.y, r.extent().x, r.extent().y);
					box->x		= p0.v.x + w * r.a.x / bm->Width();
					box->y		= p0.v.y + h * r.a.y / bm->Height();
					box->w		= w * r.extent().x / bm->Width();
					box->h		= h * r.extent().y / bm->Height();

					ISO_ptr<mi_texture>	tex(0);
					tex->tex	= bm2;
					tex->align	= 0;
					switch (blend) {
						case flash::Object::multiply: tex->align = _TALB_MULTIPLY; break;
						default:					tex->align = 0; break;
					}
					item		= tex;
				}
			}
		}

		box->item	= item;

		if (abs(a / (w * h) - 1) < 0.01f) {
			float	r	= atan2(e0.xy);
			if (abs(r) > 0.01f)
				box->item	= MakeSet(r, REGION_ROT, item);
			return box;
		}

		ISO_ptr<mi_warp>	warp(0);
		warp->item	= box;
		warp->c[0] = q.pt0() - p0;
		warp->c[1] = q.pt1() - (p0 + float2{w, 0});
		warp->c[2] = q.pt2() - (p0 + float2{0, h});
		warp->c[3] = q.pt3() - (p0 + float2{w, h});
		return warp;

	} else if (chr.IsType<flash::Text>()) {
		flash::Text	*ft		= chr;
		ISO_ptr<mi_box>		box(0);
		box->x		= ft->bounds.x;
		box->y		= ft->bounds.y;
		box->w		= ft->bounds.z - ft->bounds.x;
		box->h		= ft->bounds.w - ft->bounds.y;

		HTMLtext			html(memory_reader(ft->text.data()).me());
		ISO_ptr<mi_print>	print(0);
		ISO_ptr<void>		item	= print;

		print->flags = html.GetAlign() | TF_WRAP | TF_NOSQUISH;

		if (glow)
//			print->flags	|= glow > 1 ? TF_OUTLINE : TF_GLOW;
			print->flags	|= TF_USEPARAM1;

		(float3p&)cols.cols[0] = html.GetColour();
		if (ISO_ptr<void> o = overrides[ft->font.ID()])
			item = InsertCommand(Duplicate(o), item);

		box->scale	= html.GetSize();

		if (tag2 id = chr.Flags() & ISO::Value::ALWAYSMERGE ? obj.ID() : chr.ID()) {
			if (HasGetText(overrides[id])) {
				box->item		= item;
			} else {
				ISO_ptr<mi_gettext>	text(0);
				text->funcid	= id.get_tag();
				text->item		= item;
				box->item		= text;
			}
		} else {
			ISO_ptr<mi_text>	text(0);
			if (ISO::root("variables")["warn_directtext"].GetInt())
				text->text	= string("<DIRECT:") + (const char*)html + ">";
			else
				text->text	= str((const char*)html);
			text->item		= item;
			box->item		= text;
		}
		return box;

	} else if (chr.IsType<anything>()) {
		anything			*a = chr;
		ISO_ptr<mi_list>	list(0);
		for (int i = 0, n = a->Count(); i < n; i++)
			list->Append(Item((*a)[i], obj));
		return list;
	}
	return ISO_NULL;
}

ISO_ptr<void> FlashToMenu::Process(FlashRearrange &fr) {
	ISO_ptr<mi_list>	list(0);

	FlashColours	prev_cols	= cols;

	for (size_t s = 0, ns = fr.array.size(); s < ns; s++) {
		FlashRearrange::Item	&fri	= fr.array[s];
		ISO_ptr<flash::Object>	obj0	= fri.Obj();
		ISO_ptr<void>			chr		= obj0->character;
		ISO_ptr<void>			item	= ISO_ptr<int>(tag2(), int(s));
		saver<int>				save_blend(blend, max(blend, obj0->flags & 15));
		saver<bool>				save_override(has_override, has_override || overrides[obj0.ID()]);
		ISO::Browser				curve_override = ISO::Browser(curve_overrides)[obj0.ID()];
		saver<flags<CURVE_FLAGS> >	save_curve(curve_flags, curve_override.GetInt(curve_flags));

		if (chr.IsType<flash::Frame>()) {
			// button
			flash::Frame		*a = chr;
			ISO_ptr<void>		states[4];
			FlashColours		button_cols	= prev_cols;
			for (int state = 0; state < 4; state++) {
				ISO_ptr<mi_list>	list(0);
				int					mask = 0x100 << state;
				for (int i = 0, n = a->Count(); i < n; i++) {
					ISO_ptr<flash::Object>	obj = (*a)[i];
					if (obj->flags & mask) {
						flags<CURVE_FLAGS>	save_curve2	= curve_flags;
						curve_flags.set(LC_LOOP, (touch && !curve_flags.test(LC_DESELECT_ANIM) && state == 1) || (!curve_flags.test(LC_NO_LOOP) && (!touch || state != 2 || curve_override)));
						ISO_ptr<void> item = Item(obj->character, obj);
						curve_flags = save_curve2;

						FlashToCurvesObject	ftoc(item);
						for (int j = 0; j < fri.anim.size(); j++) {
							flash::Object	*obj1 = fri.anim[j];
							ftoc.AddFrame((j + fri.start) / fps, (*(flash::Frame*)(obj1->character))[i]);
						}
						item = ftoc.Done(item, state != 2);
						if (state == 0 && list->Count() == 0) {
							button_cols	= cols;
						} else {
							item		= cols.AddCommands(button_cols, item);
						}
						list->Append(item);
					}
				}

				switch (list->Count()) {
					case 0:	break;
					case 1:	states[state] = (*list)[0]; break;
					default:states[state] = CombineBoxes(list); break;
				}
			}
			cols = button_cols;

			if (touch) {
				if (states[1] && !curve_flags.test_any(LC_DESELECT | LC_DESELECT_ANIM))
					states[1] = Sequence(states[2], states[1]);
				else
					swap(states[1], states[2]);
			}
			if (curve_flags & LC_DESELECT_ANIM) {
				if (CompareData(states[0], states[1], ISO::TRAV_DEEP) && CompareData(states[0], states[2], ISO::TRAV_DEEP)) {
					item	= states[0];
				} else {
					ISO_ptr<mi_list>	temp(0);
					temp->Append(states[0]);
					temp->Append(states[1]);
					temp->Append(states[2]);
					item	= CombineBoxes(temp);

					ISO_ptr<mi_ifselected>	ifsel(0);
					ifsel->yes	= (*temp)[1];
					ifsel->no	= (*temp)[2];
					ifsel->init	= (*temp)[0];
					if (item.IsType<mi_box>(ISO::MATCH_NOUSERRECURSE))
						((ISO_ptr<mi_box>&)item)->item = ifsel;
					else
						item = ifsel;
				}
			} else {
				if (CompareData(states[0], states[1], ISO::TRAV_DEEP)) {
					item	= states[0];
				} else {
					ISO_ptr<mi_list>	temp(0);
					temp->Append(states[0]);
					temp->Append(states[1]);
					item	= CombineBoxes(temp);

					ISO_ptr<mi_ifselected>	ifsel(0);
					ifsel->yes	= (*temp)[1];
					ifsel->no	= (*temp)[0];
					ifsel->init	= (*temp)[0];
					if (item.IsType<mi_box>(ISO::MATCH_NOUSERRECURSE))
						((ISO_ptr<mi_box>&)item)->item = ifsel;
					else
						item = ifsel;
				}
			}

			if (touch) {
				ISO_ptr<mi_custom>	c(0);
				c->funcid	= "selection";
				item		= InsertCommand(c, item);
			}

		} else {
			// non-button
			item = Item(chr, obj0);
			if (!item)
				continue;
			if (item.IsType<mi_list>(ISO::MATCH_NOUSERRECURSE))
				item = CombineBoxes(item);
		}

		if (ISO_ptr<void> f = obj0->filters["ColorMatrixFilter"]) {
			struct cm0s { float m[4][5]; } *cm0p = f;
			bool	mult = true;
			for (int i = 0; mult && i < 4; i++) {
				int	only = check_only(cm0p->m[i], 5);
				mult = only == i || only == 5;
			}
			if (mult) {
				cols.cols[0].x *= cm0p->m[0][0];
				cols.cols[0].y *= cm0p->m[1][1];
				cols.cols[0].z *= cm0p->m[2][2];
				cols.cols[0].w *= cm0p->m[3][3];
			} else {
				struct cm1s { float m[5][4]; } cm1;
				for (int i = 0; i < 5; i++) {
					for (int j = 0; j < 4; j++)
						cm1.m[i][j] = cm0p->m[j][i];
				}
				ISO_ptr<mi_custom>	c(0);
				c->args.Append(MakePtr(tag2(), cm1.m));
				c->funcid	= "colour_matrix";
				item		= InsertCommand(c, item);
			}
		}

		FlashToCurvesObject	ftoc(item);
		for (int i = 0; i < fri.anim.size(); i++) {
			flash::Object	*obj1	= fri.anim[i];
			if (!obj1)
				break;
			ftoc.AddFrame((i + fri.start) / fps, obj1);
		}

		if (tag2 id = obj0.ID()) {
			if (ISO_ptr<void> o = overrides[id]) {
				item = InsertCommand(Duplicate(o), item);

			} else if (id.get_tag()[0] != '_' && chr.IsType<flash::Frame>(ISO::MATCH_NOUSERRECURSE)) {
				ISO_ptr<mi_trigger>	trig(0);
				trig->funcid	= id.get_tag();
				item			= InsertCommand(trig, item);
			}
			item.SetID(id);
		}

		item = ftoc.Done(item, curve_flags.with_clear(LC_NO_LOOP));
		if (s == 0)
			prev_cols	= cols;
		else
			item		= cols.AddCommands(prev_cols, item);
		list->Append(item);
	}

	cols = prev_cols;

	switch (list->Count()) {
		case 0:	return ISO_NULL;
		case 1: return (*list)[0];
		default: return list;
	}
}

bool Labeled(ISO_ptr<void> p) {
	tag2	id	= p.ID();
	return id && !id.get_tag().begins("_");
}
ISO_ptr<void> FlashToMenu::Movie(const flash::Movie &movie) {
	if (Labeled(movie[0])) {
		ISO_ptr<mi_list>	list(0);
		FlashColours		prev_cols(cols);

		for (int i = 1, s = 0, n = movie.Count(); i <= n; i++) {
			if (i == n || Labeled(movie[i])) {
				FlashRearrange	fr(movie, s, i - s);
				tag2			id		= movie[s].ID();
				ISO_ptr<void>	item	= (save(curve_flags, ISO::Browser(curve_overrides)[id].GetInt(curve_flags.with_clear(LC_LOOP))), Process(fr));
				if (item.IsType<mi_list>(ISO::MATCH_NOUSERRECURSE))
					item = CombineBoxes(item);

				if (s == 0)
					prev_cols	= cols;
				else
					item		= cols.AddCommands(prev_cols, item);

				item.SetID(id);
				if (ISO_ptr<void> o = overrides[id])
					item = InsertCommand(Duplicate(o), item);
				list->Append(item);
				s = i;
			}
		}

		cols = prev_cols;
		return list;

	} else {
		FlashRearrange	fr(movie, 0, movie.Count());
		return Process(fr);
	}
}

ISO_ptr<void> FlashToCurves(ISO_ptr<flash::File> swf, const anything &overrides, uint32 curve_flags, const ISO_openarray<ISO_ptr<uint32>> &curve_overrides) {
	if (curve_flags == 0)
		curve_flags = LC_LINEAR | LC_LOOP | LC_DESELECT;

	swf = ISO_conversion::convert<flash::File>(swf, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);
	FlashToMenu		f2m(swf->rect, swf->background, swf->framerate, overrides, curve_flags, curve_overrides);
	ISO_ptr<void>	item = f2m.cols.AddCommands(FlashColours(), f2m.Movie(swf->movie));

	if (ISO_ptr<void> o = overrides["root"])
		item = InsertCommand(Duplicate(o), item);

	return item;
}

string ControlCodes(const string &in) {
	string		temp(in.length());
	fixed_accum	a(temp.begin(), in.length());
	ConvertControlCodes(a, in);
	return temp;
}
//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation(AnimationToCurves),
	ISO_get_operation(FlashToCurves),
	ISO_get_operation(ControlCodes)
);
