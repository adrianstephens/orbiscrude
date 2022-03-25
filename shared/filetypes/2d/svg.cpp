#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "maths/geometry.h"
#include "vector_iso.h"

using namespace iso;

#define make_rgb(r,g,b)	(r | (g<<8) | (b<<16))
struct {const char *name; uint32 col; } colours[] = {
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

class SVG {
	ISO::Browser		root;
	float			width, height;

public:
	struct line			: array<float2p, 2> { line(const float2 &a, const float2 &b) : array<float2p, 2>(a, b) {} };
	struct quadratic	: array<float2p, 3> { quadratic(const float2 &a, const float2 &b, const float2 &c) : array<float2p, 3>(a, b, c) {} };
	struct cubic		: array<float2p, 4> { cubic(const float2 &a, const float2 &b, const float2 &c, const float2 &d) : array<float2p, 4>(a, b,c, d) {} };
	struct elliptic		{
		float2p start, end, radii; float angle; uint32 flags;
		elliptic(const float2 &_start, const float2 &_end, const float2 &_radii, float _angle, uint32 _flags) : angle(_angle), flags(_flags) { start = _start; end = _end; radii = _radii; }
	};

	SVG(ISO::Browser &b) : root(b) {
		width = b["width"].GetFloat();
		height = b["height"].GetFloat();
	}

	ISO_ptr<void>	GetPath(const ISO::Browser &b);
	ISO_ptr<void>	GetGroup(const ISO::Browser &b);
};

ISO_DEFUSERX(SVG::line,		float2p[2], "line");
ISO_DEFUSERX(SVG::quadratic,float2p[3], "quadratic");
ISO_DEFUSERX(SVG::cubic,	float2p[4], "cubic");
ISO_DEFUSERCOMPXV(SVG::elliptic, "elliptic", start, end, radii, angle, flags);

float2 get_float2(string_scan &ss) {
	float	x = ss.get<float>();
	if (ss.skip_whitespace().peekc() == ',')
		ss.move(1);
	float	y = ss.get<float>();
	return {x, y};
}

struct PathReader {
	float2	start;
	float2	pos;
	float2	cp;
	float2 read_float2(string_scan &ss, bool relative) {
		float2	vec = get_float2(ss);
		pos = relative ? (pos + vec) : vec;
		return pos;
	}
	operator float2()		const		{ return pos; }
	float2		get_cp()	const		{ return pos + cp; }
	void		set_cp(param(float2) v)	{ cp = pos - v; }
	PathReader() : start(zero), pos(zero), cp(zero)	{}
};


ISO_ptr<void> SVG::GetPath(const ISO::Browser &b)	{
	ISO_ptr<anything>	p(b["id"].GetString());
	string_scan		path(b["d"].GetString());
	PathReader		pos;

	bool	relative	= false;
	char	mode		= 0;
	bool	closed		= true;

	while (path.skip_whitespace().remaining()) {
		char c = path.peekc();
		if (is_alpha(c)) {
			relative	= is_lower(c);
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
				pos.read_float2(path, relative);
				if (closed)
					pos.start = pos.pos;
				mode	= 'L';
				break;
			}
			case 'Z': {
				p->Append(ISO::MakePtr(0, SVG::line(pos, pos.start)));
				closed = true;
				break;
			}
			case 'L': {
				float2	a = pos;
				float2	b = pos.read_float2(path, relative);
				p->Append(ISO::MakePtr(0, SVG::line(a, b)));
				break;
			}
			case 'H': {
				float2	a = pos;
				float x = path.get<float>();
				pos.pos.x += x;
				p->Append(ISO::MakePtr(0, SVG::line(a, pos)));
				break;
			}
			case 'V': {
				float2	a = pos;
				float y = path.get<float>();
				pos.pos.y += y;
				p->Append(ISO::MakePtr(0, SVG::line(a, pos)));
				break;
			}
			case 'Q': {
				float2	v0 = pos;
				float2	v1 = pos.read_float2(path, relative);
				float2	v2 = pos.read_float2(path, relative);
				pos.set_cp(v1);
				p->Append(ISO::MakePtr(0, SVG::quadratic(v0, v1, v2)));
				break;
			}
			case 'T': {
				float2	v0 = pos;
				float2	v1 = pos.get_cp();
				float2	v2 = pos.read_float2(path, relative);
				pos.set_cp(v1);
				p->Append(ISO::MakePtr(0, SVG::quadratic(v0, v1, v2)));
				break;
			}
			case 'C': {
				float2	v0 = pos;
				float2	v1 = pos.read_float2(path, relative);
				float2	v2 = pos.read_float2(path, relative);
				float2	v3 = pos.read_float2(path, relative);
				pos.set_cp(v2);
				p->Append(ISO::MakePtr(0, SVG::cubic(v0, v1, v2, v3)));
				break;
			}
			case 'S': {
				float2	v0 = pos;
				float2	v1 = pos.get_cp();
				float2	v2 = pos.read_float2(path, relative);
				float2	v3 = pos.read_float2(path, relative);
				pos.set_cp(v2);
				p->Append(ISO::MakePtr(0, SVG::cubic(v0, v1, v2, v3)));
				break;
			}

			case 'A': {
				float2	start	= pos;
				float2	radii	= get_float2(path);
				float	angle	= path.get<float>();
				int		large	= path.get<int>();
				int		sweep	= path.get<int>();
				float2	end		= pos.read_float2(path, relative);
				p->Append(ISO::MakePtr(0, SVG::elliptic(start, end, radii, angle, (large ? 1 : 0) | (sweep ? 2 : 0))));
				break;
			}
		}
	}
	return p;
}
/*
core attributes:
id, xml:base, xml:lang and xml:space.

conditional processing attributes
are requiredExtensions, requiredFeatures and systemLanguage

graphical event attributes
onactivate, onclick, onfocusin, onfocusout, onload, onmousedown, onmousemove, onmouseout, onmouseover and onmouseup.

xlink attributes
xlink:href, xlink:type, xlink:role, xlink:arcrole, xlink:title, xlink:show and xlink:actuate.

animation element
animateColor, animateMotion, animateTransform, animate and set.

externalResourcesRequired=false|true
*/

ISO_ptr<void> SVG::GetGroup(const ISO::Browser &b)	{
	ISO_ptr<anything>	p(b["id"].GetString());

	for (int i = 0, n = b.Count(); i < n; i++) {
		tag2	id = b.GetName(i);
		if (id == "g") {
			p->Append(GetGroup(*b[i]));
		} else if (id == "circle") {
			//cx = "<coordinate>"
			//cy = "<coordinate>"
			//r = "<length>"
		} else if (id == "ellipse") {
			//cx = "<coordinate>"
			//cy = "<coordinate>"
			//rx = "<length>"
			//ry = "<length>"
		} else if (id == "image") {
			//preserveAspectRatio
			//transform
			//x
			//y
			//width
			//height
			//xlink:href
		} else if (id == "line") {
			//x1 = "<coordinate>"
			//y1 = "<coordinate>"
			//x2 = "<coordinate>"
			//y2 = "<coordinate>"
		} else if (id == "path") {
			p->Append(GetPath(*b[i]));
		} else if (id == "polygon") {
			//points = "<list-of-points>"
		} else if (id == "polyline") {
			//points = "<list-of-points>"
		} else if (id == "rect") {
			//x = "<coordinate>"
			//y = "<coordinate>"
			//width = "<length>"
			//height = "<length>"
			//rx = "<length>"
			//ry = "<length>"
		} else if (id == "text") {
			//x = "<list-of-coordinates>"
			//y = "<list-of-coordinates>"
			//dx = "<list-of-lengths>"
			//dy = "<list-of-lengths>"
			//rotate = "<list-of-numbers>"
			//textLength = "<length>"
			//lengthAdjust = "spacing|spacingAndGlyphs"
		} else if (id == "use") {
			//x, y, width, height (optional)
		} else {
		}
	}
	return p;
}

class SVGFileHandler : public FileHandler {
	const char*		GetExt() override { return "svg"; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<void>	p = FileHandler::Get("xml")->Read(id, file);
		ISO::Browser		b(p);
		if (b.Count() == 1 && b.GetName() == "svg") {
			b = *b[0];
			return SVG(b).GetGroup(b);
		}
		return p;
	}
} svg;
