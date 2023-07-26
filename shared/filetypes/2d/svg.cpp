#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "fonts.h"
#include "extra/xml.h"
#include "maths/geometry_iso.h"
#include "codec/base64.h"

using namespace iso;

#define make_rgb(r,g,b)	(r | (g<<8) | (b<<16))
struct {const char *name; uint32 col; } colours[] = {
	{ "none",					0xffffffff				},
	{ "aliceblue",				make_rgb(240, 248, 255)	},
	{ "antiquewhite",			make_rgb(250, 235, 215)	},
	{ "aqua",					make_rgb( 0, 255, 255)	},
	{ "aquamarine",				make_rgb(127, 255, 212)	},
	{ "azure",					make_rgb(240, 255, 255)	},
	{ "beige",					make_rgb(245, 245, 220)	},
	{ "bisque",					make_rgb(255, 228, 196)	},
	{ "black",					make_rgb( 0, 0, 0)		},
	{ "blanchedalmond",			make_rgb(255, 235, 205)	},
	{ "blue",					make_rgb( 0, 0, 255)	},
	{ "blueviolet",				make_rgb(138, 43, 226)	},
	{ "brown",					make_rgb(165, 42, 42)	},
	{ "burlywood",				make_rgb(222, 184, 135)	},
	{ "cadetblue",				make_rgb( 95, 158, 160)	},
	{ "chartreuse",				make_rgb(127, 255, 0)	},
	{ "chocolate",				make_rgb(210, 105, 30)	},
	{ "coral",					make_rgb(255, 127, 80)	},
	{ "cornflowerblue",			make_rgb(100, 149, 237)	},
	{ "cornsilk",				make_rgb(255, 248, 220)	},
	{ "crimson",				make_rgb(220, 20, 60)	},
	{ "cyan",					make_rgb( 0, 255, 255)	},
	{ "darkblue",				make_rgb( 0, 0, 139)	},
	{ "darkcyan",				make_rgb( 0, 139, 139)	},
	{ "darkgoldenrod",			make_rgb(184, 134, 11)	},
	{ "darkgray",				make_rgb(169, 169, 169)	},
	{ "darkgreen",				make_rgb( 0, 100, 0)	},
	{ "darkgrey",				make_rgb(169, 169, 169)	},
	{ "darkkhaki",				make_rgb(189, 183, 107)	},
	{ "darkmagenta",			make_rgb(139, 0, 139)	},
	{ "darkolivegreen",			make_rgb( 85, 107, 47)	},
	{ "darkorange",				make_rgb(255, 140, 0)	},
	{ "darkorchid",				make_rgb(153, 50, 204)	},
	{ "darkred",				make_rgb(139, 0, 0)		},
	{ "darksalmon",				make_rgb(233, 150, 122)	},
	{ "darkseagreen",			make_rgb(143, 188, 143)	},
	{ "darkslateblue",			make_rgb( 72, 61, 139)	},
	{ "darkslategray",			make_rgb( 47, 79, 79)	},
	{ "darkslategrey",			make_rgb( 47, 79, 79)	},
	{ "darkturquoise",			make_rgb( 0, 206, 209)	},
	{ "darkviolet",				make_rgb(148, 0, 211)	},
	{ "deeppink",				make_rgb(255, 20, 147)	},
	{ "deepskyblue",			make_rgb( 0, 191, 255)	},
	{ "dimgray",				make_rgb(105, 105, 105)	},
	{ "dimgrey",				make_rgb(105, 105, 105)	},
	{ "dodgerblue",				make_rgb( 30, 144, 255)	},
	{ "firebrick",				make_rgb(178, 34, 34)	},
	{ "floralwhite",			make_rgb(255, 250, 240)	},
	{ "forestgreen",			make_rgb( 34, 139, 34)	},
	{ "fuchsia",				make_rgb(255, 0, 255)	},
	{ "gainsboro",				make_rgb(220, 220, 220)	},
	{ "ghostwhite",				make_rgb(248, 248, 255)	},
	{ "gold",					make_rgb(255, 215, 0)	},
	{ "goldenrod",				make_rgb(218, 165, 32)	},
	{ "gray",					make_rgb(128, 128, 128)	},
	{ "grey",					make_rgb(128, 128, 128)	},
	{ "green",					make_rgb( 0, 128, 0)	},
	{ "greenyellow",			make_rgb(173, 255, 47)	},
	{ "honeydew",				make_rgb(240, 255, 240)	},
	{ "hotpink",				make_rgb(255, 105, 180)	},
	{ "indianred",				make_rgb(205, 92, 92)	},
	{ "indigo",					make_rgb( 75, 0, 130)	},
	{ "ivory",					make_rgb(255, 255, 240)	},
	{ "khaki",					make_rgb(240, 230, 140)	},
	{ "lavender",				make_rgb(230, 230, 250)	},
	{ "lavenderblush",			make_rgb(255, 240, 245)	},
	{ "lawngreen",				make_rgb(124, 252, 0)	},
	{ "lemonchiffon",			make_rgb(255, 250, 205)	},
	{ "lightblue",				make_rgb(173, 216, 230)	},
	{ "lightcoral",				make_rgb(240, 128, 128)	},
	{ "lightcyan",				make_rgb(224, 255, 255)	},
	{ "lightgoldenrodyellow",	make_rgb(250, 250, 210)	},
	{ "lightgray",				make_rgb(211, 211, 211)	},
	{ "lightgreen",				make_rgb(144, 238, 144)	},
	{ "lightgrey",				make_rgb(211, 211, 211)	},
	{ "lightpink",				make_rgb(255, 182, 193)	},
	{ "lightsalmon",			make_rgb(255, 160, 122)	},
	{ "lightseagreen",			make_rgb( 32, 178, 170)	},
	{ "lightskyblue",			make_rgb(135, 206, 250)	},
	{ "lightslategray",			make_rgb(119, 136, 153)	},
	{ "lightslategrey",			make_rgb(119, 136, 153)	},
	{ "lightsteelblue",			make_rgb(176, 196, 222)	},
	{ "lightyellow",			make_rgb(255, 255, 224)	},
	{ "lime",					make_rgb( 0, 255, 0)	},
	{ "limegreen",				make_rgb( 50, 205, 50)	},
	{ "linen",					make_rgb(250, 240, 230)	},
	{ "magenta",				make_rgb(255, 0, 255)	},
	{ "maroon",					make_rgb(128, 0, 0)		},
	{ "mediumaquamarine",		make_rgb(102, 205, 170)	},
	{ "mediumblue",				make_rgb( 0, 0, 205)	},
	{ "mediumorchid",			make_rgb(186, 85, 211)	},
	{ "mediumpurple",			make_rgb(147, 112, 219)	},
	{ "mediumseagreen",			make_rgb( 60, 179, 113)	},
	{ "mediumslateblue",		make_rgb(123, 104, 238)	},
	{ "mediumspringgreen",		make_rgb( 0, 250, 154)	},
	{ "mediumturquoise",		make_rgb( 72, 209, 204)	},
	{ "mediumvioletred",		make_rgb(199, 21, 133)	},
	{ "midnightblue",			make_rgb( 25, 25, 112)	},
	{ "mintcream",				make_rgb(245, 255, 250)	},
	{ "mistyrose",				make_rgb(255, 228, 225)	},
	{ "moccasin",				make_rgb(255, 228, 181)	},
	{ "navajowhite",			make_rgb(255, 222, 173)	},
	{ "navy",					make_rgb( 0, 0, 128)	},
	{ "oldlace",				make_rgb(253, 245, 230)	},
	{ "olive",					make_rgb(128, 128, 0)	},
	{ "olivedrab",				make_rgb(107, 142, 35)	},
	{ "orange",					make_rgb(255, 165, 0)	},
	{ "orangered",				make_rgb(255, 69, 0)	},
	{ "orchid",					make_rgb(218, 112, 214)	},
	{ "palegoldenrod",			make_rgb(238, 232, 170)	},
	{ "palegreen",				make_rgb(152, 251, 152)	},
	{ "paleturquoise",			make_rgb(175, 238, 238)	},
	{ "palevioletred",			make_rgb(219, 112, 147)	},
	{ "papayawhip",				make_rgb(255, 239, 213)	},
	{ "peachpuff",				make_rgb(255, 218, 185)	},
	{ "peru",					make_rgb(205, 133, 63)	},
	{ "pink",					make_rgb(255, 192, 203)	},
	{ "plum",					make_rgb(221, 160, 221)	},
	{ "powderblue",				make_rgb(176, 224, 230)	},
	{ "purple",					make_rgb(128, 0, 128)	},
	{ "red",					make_rgb(255, 0, 0)		},
	{ "rosybrown",				make_rgb(188, 143, 143)	},
	{ "royalblue",				make_rgb( 65, 105, 225)	},
	{ "saddlebrown",			make_rgb(139, 69, 19)	},
	{ "salmon",					make_rgb(250, 128, 114)	},
	{ "sandybrown",				make_rgb(244, 164, 96)	},
	{ "seagreen",				make_rgb( 46, 139, 87)	},
	{ "seashell",				make_rgb(255, 245, 238)	},
	{ "sienna",					make_rgb(160, 82, 45)	},
	{ "silver",					make_rgb(192, 192, 192)	},
	{ "skyblue",				make_rgb(135, 206, 235)	},
	{ "slateblue",				make_rgb(106, 90, 205)	},
	{ "slategray",				make_rgb(112, 128, 144)	},
	{ "slategrey",				make_rgb(112, 128, 144)	},
	{ "snow",					make_rgb(255, 250, 250)	},
	{ "springgreen",			make_rgb( 0, 255, 127)	},
	{ "steelblue",				make_rgb( 70, 130, 180)	},
	{ "tan",					make_rgb(210, 180, 140)	},
	{ "teal",					make_rgb( 0, 128, 128)	},
	{ "thistle",				make_rgb(216, 191, 216)	},
	{ "tomato",					make_rgb(255, 99, 71)	},
	{ "turquoise",				make_rgb( 64, 224, 208)	},
	{ "violet",					make_rgb(238, 130, 238)	},
	{ "wheat",					make_rgb(245, 222, 179)	},
	{ "white",					make_rgb(255, 255, 255)	},
	{ "whitesmoke",				make_rgb(245, 245, 245)	},
	{ "yellow",					make_rgb(255, 255, 0)	},
	{ "yellowgreen",			make_rgb(154, 205, 50)	},
};

uint32 GetColour(const char* s) {
	if (s) {
		if (s[0] == '#') {
			uint32	v;
			s = get_num_base<16>(s + 1, v);
			return swap_endian(v) >> 8;
		}
		for (auto &i: colours) {
			if (str(i.name) == s)
				return i.col;
		}
	}
	return -1;
}

float2 get_float2(string_scan &ss) {
	float	x = ss.get<float>();
	if (ss.skip_whitespace().peekc() == ',')
		ss.move(1);
	float	y = ss.get<float>();
	return {x, y};
}

float2x3 GetTransform(const char* s) {
	string_scan		ss(s);
	auto	tok	= ss.get_token();

	ss.skip_whitespace().check('(');

	if (tok == "matrix") {
		return {
			get_float2(ss),
			(ss.skip_whitespace().check(','), get_float2(ss)),
			(ss.skip_whitespace().check(','), get_float2(ss))
		};
	} else if (tok == "translate") {
		return translate(get_float2(ss));

	} else if (tok == "scale") {
		float	x = ss.get<float>(), y = x;
		if (ss.skip_whitespace().check(','))
			y = ss.get<float>();
		return scale(float2{x, y});

	} else if (tok == "rotate") {
		float	a = degrees(ss.get<float>());
		if (ss.skip_whitespace().check(',')) {
			auto	origin = translate(get_float2(ss));
			return origin * rotate2D(a) * inverse(origin);
		}
		return (float2x3)rotate2D(a);

	} else if (tok == "skewX") {
		float	a = tan(degrees(ss.get<float>()));
		return {float2{1, 0}, float2{a, 1}, float2{0, 0}};

	} else if (tok == "skewY") {
		float	a = tan(degrees(ss.get<float>()));
		return {float2{1, a}, float2{0, 1}, float2{0, 0}};
	}
	return identity;
}

Gradient GetGradient(XMLiterator &it) {
	Gradient	g;

	if (it.data.Is("linearGradient")) {
		position2	p1	= {it.data["x1"], it.data["y1"]};
		position2	p2	= {it.data["x2"], it.data["y2"]};
		g.SetLinear(p1, p2 - p1);

	} else if (it.data.Is("radialGradient")) {
		circle		c1	= {{it.data["cx"], it.data["cy"]}, it.data["r"]};
		circle		c0	= {{it.data["fx"], it.data["fy"]}, 0};
		g.SetRadial(c0, c1);
	}

	g.Transform(GetTransform(it.data["gradientTransform"]));

	g.spread		= which(1, str(it.data["spreadMethod"]), "pad", "reflect", "repeat");
	g.user_space	= which(0, str(it.data["gradientUnits"]), "objectBoundingBox", "userSpaceOnUse");

	for (it.Enter(); it.Next();) {
		if (it.data.Is("stop")) {
			float	offset	= it.data["offset"];
			auto	col		= force_cast<rgba8>(GetColour(it.data["stop-color"]));
			col.w			= (float)it.data["stop-opacity"];;
			g.stops.emplace_back(offset, col);
		}
	}
	return g;
}

struct PathReader {
	float2	base	= zero;
	float2	prev	= zero;
	float2	prev2	= zero;

	float2	fix(float2 v) {
		v		+= base;
		prev2	= prev;
		prev	= v;
		return v;
	}
	float2	read_float2(string_scan &ss) {
		return fix(get_float2(ss));
	}
	void	set_base(bool relative)	{ base = relative ? prev : zero; }
	float2	get_cp()	const		{ return prev - (prev2 - prev); }
};

ISO_ptr<void> GetPath(tag2 id, const char *s) {
	ISO_ptr<ISO_openarray_machine<curve_vertex>>	p(id);
	string_scan		path(s);
	PathReader		pos;

	char	mode		= 0;
	bool	closed		= true;

	while (path.skip_whitespace().remaining()) {
		char c = path.peekc();
		if (is_alpha(c)) {
			pos.set_base(is_lower(c));
			mode		= to_upper(c);
			path.move(1);
		} else if (c == ',') {
			path.move(1);
			continue;
		} else {
			closed = false;
		}

		switch (mode) {
			case 'M': {
				float2	v = pos.read_float2(path);
				p->Append({v, ON_BEGIN});
				mode	= 'L';
				break;
			}
			case 'Z': {
				closed = true;
				break;
			}
			case 'L': {
				float2	b = pos.read_float2(path);
				p->Append({b, ON_CURVE});
				break;
			}
			case 'H': {
				float	x = path.get<float>();
				float2	v = {x, pos.prev.y - pos.base.y};
				p->Append({pos.fix(v), ON_CURVE});
				break;
			}
			case 'V': {
				float	y = path.get<float>();
				float2	v = {pos.prev.x - pos.base.x, y};
				p->Append({pos.fix(v), ON_CURVE});
				break;
			}
			case 'Q': {
				float2	v1 = pos.read_float2(path);
				float2	v2 = pos.read_float2(path);
				p->Append({v1, OFF_BEZ2});
				p->Append({v2, ON_CURVE});
				break;
			}
			case 'T': {
				float2	v1 = pos.get_cp();
				float2	v2 = pos.read_float2(path);
				p->Append({v1, OFF_BEZ2});
				p->Append({v2, ON_CURVE});
				break;
			}
			case 'C': {
				float2	v1 = pos.read_float2(path);
				float2	v2 = pos.read_float2(path);
				float2	v3 = pos.read_float2(path);
				p->Append({v1, OFF_BEZ3});
				p->Append({v2, OFF_BEZ3});
				p->Append({v3, ON_CURVE});
				break;
			}
			case 'S': {
				float2	v1 = pos.get_cp();
				float2	v2 = pos.read_float2(path);
				float2	v3 = pos.read_float2(path);
				p->Append({v1, OFF_BEZ3});
				p->Append({v2, OFF_BEZ3});
				p->Append({v3, ON_CURVE});
				break;
			}

			case 'A': {
				float2	start	= pos.prev;
				float2	radii	= get_float2(path);
				float	angle	= path.get<float>();
				int		large	= path.get<int>();
				int		sweep	= path.get<int>();
				float2	end		= pos.read_float2(path);

				ArcParams	arc(radii, degrees(angle), sweep, large);
				arc.fix_radii(start, end);
				float2x2 cp	= arc.control_points(start, end);
				p->Append({cp.x, OFF_ARC});
				p->Append({cp.y, OFF_ARC});
				p->Append({end, ON_CURVE});
				break;
			}
		}
	}

	return p;
}

/*
core attributes:
type, xml:base, xml:lang and xml:space

presentation attributes:
alignment-baseline, baseline-shift, clip, clip-path, clip-rule, color, color-interpolation, color-interpolation-filters, color-profile, color-rendering,
cursor, direction, display, dominant-baseline, enable-background, fill, fill-opacity, fill-rule, filter, flood-color, flood-opacity,
font-family, font-size, font-size-adjust, font-stretch, font-style, font-variant, font-weight,
glyph-orientation-horizontal, glyph-orientation-vertical, image-rendering, kerning, letter-spacing, lighting-color,
marker-end, marker-mid, marker-start, mask, opacity, overflow, pointer-events, shape-rendering, stop-color, stop-opacity,
stroke, stroke-dasharray, stroke-dashoffset, stroke-linecap, stroke-linejoin, stroke-miterlimit, stroke-opacity, stroke-width,
text-anchor, text-decoration, text-rendering, unicode-bidi, visibility, word-spacing, writing-mode

conditional processing attributes:
requiredExtensions, requiredFeatures and systemLanguage

graphical event attributes:
onactivate, onclick, onfocusin, onfocusout, onload, onmousedown, onmousemove, onmouseout, onmouseover, onmouseup

xlink attributes:
xlink:href, xlink:type, xlink:role, xlink:arcrole, xlink:title, xlink:show, xlink:actuate

animation element:
animateColor, animateMotion, animateTransform, animate, set

externalResourcesRequired=false|true
*/

auto GetDef(const ISO_ptr<anything64> &defs, const char *ref) {
	if (!defs)
		return ISO_NULL64;
	auto	i = lower_boundc(*defs, ref, [](const ISO_ptr<void> &a, const char *b) { return a.ID().get_tag() < b; });
	if (i == defs->end())
		return ISO_NULL64;
	return *i;//ISO::Browser(defs)/href;
}

ISO_ptr<anything64> GetGroup(tag2 id, XMLiterator &it, ISO_ptr<anything64> defs) {
	ISO_ptr<anything64>	p(id);

	for (it.Enter(); it.Next();) {
		ISO_ptr<void>	temp;
		auto	ref	= &temp;
		tag2	id	= it.data.Find("id");

		if (auto fill = it.data.Find("fill")) {
			if (fill.begins("url(#")) {
				string	url		= {fill + 5, string_find(fill + 5, ')')};
				auto	grad	= GetDef(defs, url);
				if (!grad)
					grad = GetDef(p, url);
				auto	x	= ISO::MakePtr(id, Filled{grad});
				*ref	= x;
				ref		= &x->element;
			} else {
				auto	col = GetColour(fill);
				if (~col) {
					auto x	= ISO::MakePtr(id, Filled{ISO::MakePtr(none, force_cast<rgba8>(col | 0xff000000))});
					*ref	= x;
					ref		= &x->element;
				}
			}
		}

		if (auto trans = it.data.Find("transform")) {
			auto x	= ISO::MakePtr(id, Transformed{GetTransform(trans)});
			*ref	= x;
			ref		= &x->element;
		}

		if (it.data.Is("defs")) {
			defs = GetGroup(id, it, defs);
			sort(*defs,[](const ISO_ptr<void> &a, const ISO_ptr<void> &b) { return a.ID().get_tag() < b.ID().get_tag(); });

		} else if (it.data.Is("g")) {
			auto	data = it.data;
			*ref = GetGroup(id, it, defs);

		} else if (it.data.Is("path")) {
			*ref = GetPath(id, it.data.Find("d"));

		} else if (it.data.Is("circle")) {
			*ref = ISO_ptr<circle>(id, position2(it.data["cx"], it.data["cy"]), it.data["r"]);

		} else if (it.data.Is("ellipse")) {
			float	rx	= it.data["rx"], ry = it.data["ry"];
			*ref = ISO_ptr<ellipse>(id, position2(it.data["cx"], it.data["cy"]), float2{rx, 0}, ry / rx);

		//} else if (it.data.Is("image")) {
			//preserveAspectRatio
			//transform
			//x
			//y
			//width
			//height
			//xlink:href
		} else if (it.data.Is("line")) {
			ISO_ptr<ISO_openarray_machine<curve_vertex>>	p2(id);
			p2->Append({float2{it.data["x1"], it.data["y1"]}, ON_BEGIN});
			p2->Append({float2{it.data["x2"], it.data["y2"]}, ON_CURVE});
			*ref = p2;

		} else if (it.data.Is("polygon")) {
			ISO_ptr<ISO_openarray_machine<curve_vertex>>	p2(id);
			string_scan		points(it.data.Find("points"));
			auto			flags	= ON_BEGIN;
			while (points.skip_whitespace().remaining()) {
				p2->Append({get_float2(points), flags});
				flags = ON_CURVE;
			}
			*ref = p2;

		} else if (it.data.Is("polyline")) {
			ISO_ptr<ISO_openarray_machine<curve_vertex>>	p2(id);
			string_scan		points(it.data.Find("points"));
			auto			flags	= ON_BEGIN;
			while (points.skip_whitespace().remaining()) {
				p2->Append({get_float2(points), flags});
				flags = ON_CURVE;
			}
			*ref = p2;

		} else if (it.data.Is("rect")) {
			position2	p0		= {it.data["x"], it.data["y"]};
			float2		size	= {it.data["width"], it.data["height"]};
			float2		radii	= {it.data["rx"], it.data["ry"]};

			if (all(radii == zero)) {
				*ref = ISO_ptr<rectangle>(id, p0, p0 + size);
			} else {
				ISO_ptr<ISO_openarray_machine<curve_vertex>>	p2(id);
				p2->Append({p0 + float2{radii.x, 0},				ON_BEGIN});
				p2->Append({p0 + float2{size.x - radii.x, 0},		ON_CURVE});
				p2->Append({p0 + float2{size.x, radii.y},			ON_ARC});
				p2->Append({p0 + float2{size.x, size.y - radii.y},	ON_CURVE});
				p2->Append({p0 + float2{size.x - radii.x, size.y},	ON_ARC});
				p2->Append({p0 + float2{radii.x, size.y},			ON_CURVE});
				p2->Append({p0 + float2{0, size.y - radii.y},		ON_ARC});
				p2->Append({p0 + float2{0, radii.y},				ON_CURVE});
				p2->Append({p0 + float2{radii.x, 0},				ON_ARC});
				*ref = p2;
			}

		//} else if (it.data.Is("text")) {
			//x = "<list-of-coordinates>"
			//y = "<list-of-coordinates>"
			//dx = "<list-of-lengths>"
			//dy = "<list-of-lengths>"
			//rotate = "<list-of-numbers>"
			//textLength = "<length>"
			//lengthAdjust = "spacing|spacingAndGlyphs"
		} else if (it.data.Is("use")) {
			const char		*href = it.data["xlink:href"];
			if (href[0] == '#') {
				*ref = GetDef(defs, href + 1);
				if (!*ref)
					*ref = GetDef(p, href + 1);
			}

		} else if (it.data.Is("linearGradient") || it.data.Is("radialGradient")) {
			*ref = ISO_ptr<Gradient>(id, GetGradient(it));

		} else if (it.data.Is("filter")) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("feBlend")) {
				} else if (it.data.Is("feColorMatrix")) {
				} else if (it.data.Is("feComponentTransfer")) {
				} else if (it.data.Is("feComposite")) {
				} else if (it.data.Is("feConvolveMatrix")) {
				} else if (it.data.Is("feDiffuseLighting")) {
				} else if (it.data.Is("feDisplacementMap")) {
				} else if (it.data.Is("feDropShadow")) {
				} else if (it.data.Is("feFlood")) {
				} else if (it.data.Is("feGaussianBlur")) {
				} else if (it.data.Is("feImage")) {
				} else if (it.data.Is("feMerge")) {
				} else if (it.data.Is("feMorphology")) {
				} else if (it.data.Is("feOffset")) {
				} else if (it.data.Is("feSpecularLighting")) {
				} else if (it.data.Is("feTile")) {
				} else if (it.data.Is("feTurbulence")) {
				}
			}

		} else if (it.data.Is("image")) {
			uint32	width	= it.data["width"];
			uint32	height	= it.data["height"];
			p->Append(FileHandler::Read(id, (const char*)it.data["xlink:href"]));

		} else {
			ISO_TRACEF("missing ") << it.data.Name() << '\n';
		}
		if (temp)
			p->Append(temp);
	}
	return p;
}

class SVGFileHandler : public FileHandler {
	const char*		GetExt() override { return "svg"; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {

		XMLreader		xml(file);
		XMLreader::Data	data;
		XMLiterator		it(xml, data);

		if (!it.Next() || !data.Is("svg"))
			return ISO_NULL;

		float	width = data["width"], height = data["height"];
		return GetGroup(id, it, ISO_NULL);
	}
} svg;
