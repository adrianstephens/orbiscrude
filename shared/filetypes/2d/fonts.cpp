#include "fonts.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "base/vector.h"
#include "base/algorithm.h"
#include "vector_iso.h"
#include "vector_string.h"
#include "base/algorithm.h"
#include "ttf.h"
#include "cff.h"
#include "utilities.h"

using namespace iso;

template<typename T> auto& operator<<(string_accum& sa, const packed<T> &v) { return sa << get(v); }
template<typename T> auto& operator<<(string_accum& sa, const T_swap_endian<T> &v) { return sa << get(v); }
template<typename T> auto& operator<<(string_accum& sa, const constructable<T> &v) { return sa << get(v); }


/*
struct tester {
	tester() {
		ArcParams	arc(float2{1,2}, degrees(20), false, false);
		auto		centre	= arc.centre(float2{0,0}, float2{1,0});
	}
} _tester;
*/

void iso::reverse_curves(range<curve_vertex*> curves) {
	for (auto i = curves.begin(), e = curves.end(); i != e;) {
		auto	z = ++i;
		while (z != e && z->flags != 0)
			++z;

		reverse(i, z);
		i = z;
	}
}

bool iso::direction(range<const curve_vertex*> curves) {
	if (num_elements(curves) < 3)
		return false;

	float				miny	= maximum;
	const curve_vertex	*mini	= nullptr;
	const curve_vertex	*min0	= nullptr;
	const curve_vertex	*min1	= nullptr;
	const curve_vertex	*loop	= nullptr;

	for (auto &i : curves) {
		if (i.flags == ON_BEGIN) {
			if (min0 == loop)
				min1 = &i;
			loop	= &i;
		}
		if (i.y < miny) {
			miny	= i.y;
			mini	= &i;
			min0	= loop;
		}
	}

	if (min0 == loop)
		min1 = curves.end();

	if (mini == min0) {
		for (auto i = min1; i[-1].y == miny;)
			mini = --i;
	}
	const curve_vertex	*next	= mini + 1 == curves.end() || mini + 1 == min1 ? min0 : mini + 1;
	const curve_vertex	*prev	= (mini == curves.begin() || mini == min0 ? min1 : mini) - 1;

#if 1
	return cross(*next - *mini, *mini - *prev) < zero;
#else
	bool	r = cross(*next - *mini, *mini - *prev) < zero;

	if (next->x != mini->x)
		ISO_ASSERT((next->x > mini->x) == r);


	if (next->x != mini->x)
		return next->x > mini->x;

	return mini->x > prev->x;
#endif
}

bool iso::overlap(range<const curve_vertex*> a, range<const curve_vertex*> b) {
	return false;
	//return true;
}

struct GetBezier2 : CurveTranslator {
	uint32	max_per_curve	= 6;
	float	tol				= 0.01f;

	dynamic_array<float2>					points;
	dynamic_array<bezier_chain<float2, 2>>	chains;

	void Begin(float2 p0) {
		points.push_back(p0);
	}
	void End() {
		chains.emplace_back(move(points));
		points.clear();
	}
	void Line(float2 p0, float2 p1) {
		if (approx_equal(p0, p1))
			return;
		points.push_back((p0 + p1) / 2);
		points.push_back(p1);
	}
	void Bezier(float2 p0, float2 p1, float2 p2) {
		if (approx_equal(p0, p2))
			return;
		points.push_back(p1);
		points.push_back(p2);
	}
	void Bezier(float2 p0, float2 p1, float2 p2, float2 p3) {
		if (approx_equal(p0, p3))
			return;
		auto	offset	= points.size();
		points.resize(offset + max_per_curve * 2);
		auto	p		= reduce_spline(bezier_splineT<float2,3>{p0, p1, p2, p3}, points + offset, points.end(), tol);
		points.resize(points.index_of(p));
	}
	void Arc(float2 p0, float2 p1, float2 p2)				{}
	void Arc(float2 p0, float2 p1, float2 p2, float2 p3)	{}

	GetBezier2(range<const curve_vertex*> curves, uint32 max_per_curve, float tol) : max_per_curve(max_per_curve), tol(tol) {
		add(*this, curves);
	}
};

dynamic_array<bezier_chain<float2, 2>> iso::get_bezier2(range<const curve_vertex*> curves, uint32 max_per_curve, float tol) {
	return move(GetBezier2(curves, max_per_curve, tol).chains);
}

struct GetBezier3 : CurveTranslator {
	dynamic_array<float2>					points;
	dynamic_array<bezier_chain<float2, 3>>	chains;

	void Begin(float2 p0) {
		points.push_back(p0);
	}
	void End() {
		chains.emplace_back(move(points));
		points.clear();
	}
	void Line(float2 p0, float2 p1) {
		if (approx_equal(p0, p1))
			return;
		points.push_back(lerp(p0, p1, 1/3.f));
		points.push_back(lerp(p0, p1, 2/3.f));
		points.push_back(p1);
	}
	void Bezier(float2 p0, float2 p1, float2 p2) {
		if (approx_equal(p0, p2))
			return;
		auto	b3 = bezier_splineT<float2,2>{p0, p1, p2}.elevate<1>();
		points.push_back(b3[1]);
		points.push_back(b3[2]);
		points.push_back(b3[3]);
	}
	void Bezier(float2 p0, float2 p1, float2 p2, float2 p3) {
		if (approx_equal(p0, 3))
			return;
		points.push_back(p1);
		points.push_back(p2);
		points.push_back(p3);
	}
	void Arc(float2 p0, float2 p1, float2 p2)				{}
	void Arc(float2 p0, float2 p1, float2 p2, float2 p3)	{}
	GetBezier3(range<const curve_vertex*> curves) { add(*this, curves); }
};

dynamic_array<bezier_chain<float2, 3>> iso::get_bezier3(range<const curve_vertex*> curves) {
	return move(GetBezier3(curves).chains);
}

//-----------------------------------------------------------------------------
//	Checksum
//-----------------------------------------------------------------------------

ttf::uint32 CalcTableChecksum(ttf::uint32 *table, iso::uint32 length) {
	iso::uint32		sum = 0;
	ttf::uint32*	end_ptr = table + length / sizeof(iso::uint32);
	while (table < end_ptr)
		sum += *table++;
	if (length & 3) {
		ttf::uint32	t = 0;
		memcpy(&t, table, length & 3);
		sum += t;
	}
	return sum;
}

//-----------------------------------------------------------------------------
//	Truetype
//-----------------------------------------------------------------------------

malloc_block ReadBlock(istream_ref file, streamptr offset, size_t size) {
	file.seek(offset);
	return malloc_block(file, size);
}

malloc_block ReadTable(istream_ref file, const ttf::TableRecord *table) {
	return ReadBlock(file, table->offset, table->length);
}

template<typename T> auto ReadBlockT(tag2 id, istream_ref file, streamptr offset, size_t size) {
	auto	p = ISO::MakePtrSize<32>(ISO::getdef<T>(), id, size);
	file.seek(offset);
	file.readbuff(p, size);
	return p;
}

struct TTF {
	ttf::SFNTHeader		sfnt;
	dynamic_array<ttf::TableRecord>	tables;
	unique_ptr<ttf::head>	head;

	const ttf::TableRecord*	FindTable(ttf::TAG tag) const {
		for (auto &i : tables) {
			if (i.tag == tag)
				return &i;
		}
		return nullptr;
	}
	template<typename T> unique_ptr<T>	ReadTable(istream_ref file) const {
		return ::ReadTable(file, FindTable(T::tag));
	}
	size_t				CalcSize() const {
		size_t	total	= sizeof(ttf::SFNTHeader);
		for (auto &i : tables)
			total += align(i.length, 4) + sizeof(ttf::TableRecord);
		return total;
	}

	TTF(istream_ref file) {
		file.read(sfnt);
		tables.read(file, sfnt.num_tables);
		head	= ReadTable<ttf::head>(file);
	}
	~TTF() {
	}
};

typedef ISO_openarray<curve_vertex>			iso_simpleglyph;
typedef pair<ISO_ptr<void>, float2x3p>		iso_glyphref;
typedef ISO_openarray<iso_glyphref>			iso_compoundglyph;

template<typename R> int16 get_glyph_delta(R &r, bool SHORT, bool SAMEPOS) {
	return SHORT
		? int16(r.template get<uint8>()) * (SAMEPOS ? 1 : -1)
		: SAMEPOS ? 0 : int16(r.template get<int16be>());
}

ISO_ptr<void> ReadSimpleGlyph(const ttf::glyf::simple_glyph *gs) {
	int		ncnt = gs->num_contours;

#if 0
	ISO_ptr<iso_simpleglyph>	cont(0, ncnt);
	uint32	npts	= 0;
	for (int i = 0; i < ncnt; i++) {
		uint32	end = gs->end_pts[i] + 1;
		(*cont)[i].Create(end - npts);
		npts = end;
	}

	uint16	inslen	= gs->end_pts[ncnt];
	byte_reader	r((uint8*)(gs->end_pts + ncnt + 1) + inslen);

	temp_array<uint8>	flags(npts);
	for (int i = 0; i < npts; ++i) {
		uint8	f = r.getc();
		flags[i] = f;
		if (f & ttf::glyf::simple_glyph::repeat) {
			for (uint8 x = r.getc(); x--;)
				flags[++i] = f;
		}
	}

	uint8	*f = flags;
	for (int i = 0, x = 0; i < ncnt; i++) {
		curve_vertex	*p	= (*cont)[i];
		for (uint8	*e = flags + gs->end_pts[i] + 1; f < e; ++f, ++p) {
			p->flags	= *f & ttf::glyf::simple_glyph::on_curve ? 0 : 1;
			p->x		= x += get_glyph_delta(r, *f & ttf::glyf::simple_glyph::short_x, *f & ttf::glyf::simple_glyph::same_x);
		}
	}

	f = flags;
	for (int i = 0, y = 0; i < ncnt; i++) {
		curve_vertex	*p	= (*cont)[i];
		for (uint8	*e = flags + gs->end_pts[i] + 1; f < e; ++f, ++p)
			p->y		 = y += get_glyph_delta(r, *f & ttf::glyf::simple_glyph::short_y, *f & ttf::glyf::simple_glyph::same_y);
	}
	return cont;
#else
	uint32	npts	= gs->num_pts();
	byte_reader	r(gs->contours());

	temp_array<uint8>	flags(npts);
	for (int i = 0; i < npts; ++i) {
		uint8	f = r.getc();
		flags[i] = f;
		if (f & ttf::glyf::simple_glyph::repeat) {
			for (uint8 x = r.getc(); x--;)
				flags[++i] = f;
		}
	}

	ISO_ptr<ISO_openarray<curve_vertex>>	curve(0, npts);

	auto	f = flags.begin();
	int		x	= 0;
	for (auto &p : *curve) {
		p.flags	= *f & ttf::glyf::simple_glyph::on_curve ? ON_CURVE : OFF_BEZ2;
		p.x		= x += get_glyph_delta(r, *f & ttf::glyf::simple_glyph::short_x, *f & ttf::glyf::simple_glyph::same_x);
		++f;
	}

	f	= flags.begin();
	x	= 0;
	for (auto &p : *curve) {
		p.y		 = x += get_glyph_delta(r, *f & ttf::glyf::simple_glyph::short_y, *f & ttf::glyf::simple_glyph::same_y);
		++f;
	}

	curve_vertex	*curve2	= curve->begin();
	for (int i = 0, j = 0; i < ncnt; i++) {
		int	j1 = gs->end_pts[i] + 1;

		if (curve2[j].flags != 1) {
			for (int k = j; k < j1; k++) {
				if (curve2[k].flags == 1) {
					rotate(curve2 + j, curve2 + k, curve2 + j1);
					break;
				}
			}
		}
		curve2[j].flags = ON_BEGIN;
		j = j1;
	}

	return curve;
#endif
}

ISO_ptr<void> ReadCompoundGlyph(const ttf::glyf::compound_glyph *gc, const anything &glyphs) {
	byte_reader		r(gc->entries);
	uint16			flags;
	ISO_ptr<iso_compoundglyph>	comp(0);

	do {
		auto	entry	= r.get_ptr<ttf::glyf::compound_glyph::entry>();
		flags	= entry->flags;

		ttf::_fixed16	xoff, yoff;
		if (flags & ttf::glyf::compound_glyph::arg12_words) {
			xoff = r.get<ttf::fixed16>();
			yoff = r.get<ttf::fixed16>();
		} else {
			xoff = r.get<fixed<2,6>>();
			yoff = r.get<fixed<2,6>>();
		}

		float2x2	mat;
		if (flags & ttf::glyf::compound_glyph::have_scale) {
			mat = (float2x2)scale(ttf::_fixed16(r.get<ttf::fixed16>()));
		} else if (flags & ttf::glyf::compound_glyph::x_and_y_scale) {
			ttf::fixed16	t[2]; r.read(t);
			mat = (float2x2)scale(ttf::_fixed16(t[0]), ttf::_fixed16(t[1]));
		} else if (flags & ttf::glyf::compound_glyph::two_by_two) {
			ttf::fixed16	t[4]; r.read(t);
			mat = float2x2(float2{ttf::_fixed16(t[0]), ttf::_fixed16(t[1])}, float2{ttf::_fixed16(t[2]), ttf::_fixed16(t[3])});
		} else {
			mat = identity;
		}

		iso_glyphref	&ref = comp->Append();
		ref.a	= glyphs[entry->index];
		ref.b	= float2x3(mat, float2{xoff, yoff});
	} while (flags & ttf::glyf::compound_glyph::more_components);
	return comp;
}

ISO_ptr<void> ReadGlyph(const memory_block &mb, const anything &glyphs) {
	ttf::glyf::glyph *g = mb;
	if (!g)
		return ISO_NULL;

	if (g->num_contours < 0)
		return ReadCompoundGlyph(mb, glyphs);
	return ReadSimpleGlyph(mb);
}

ISO_ptr<void> ReadNAME(tag id, const ttf::name *name) {
	static const char *ids[] = {
		"Copyright",
		"Family",
		"Subfamily",
		"Identification",
		"FullName",
		"Version",
		"PostScriptName",
		"Trademark",
		"Manufacturer",
		"Designer",
		"Description",
		"VendorURL",
		"DesignerURL",
		"LicenseDescription",
		"LicenseURL",
		"Reserved",
		"PreferredFamily",
		"PreferredSubfamily",
		"CompatibleFull",
		"SampleText",
	};
	ISO_ptr<anything>	p(id);
	const ttf::name::NameRecord	*r = name->names;
	uint8	*start	= (uint8*)name + name->stringOffset;
	for (int n = name->count; n--; r++) {
		uint16		i = r->nameID;
		if (i < iso::num_elements(ids))
			id = ids[i];
		else
			id = format_string("%s_%04x", i >= ttf::name::ID_FONT_SPECIFIC ? "Specific" : "Reserved", i);

		void	*s = start + r->offset;
		switch (r->platformID) {
			case ttf::PLAT_unicode:
				break;
			case ttf::PLAT_macintosh:
				p->Append(ISO_ptr<string>(id, str((const char*)s, r->length)));
				break;
			case ttf::PLAT_microsoft:
				switch (r->encodingID) {
					case ttf::name::MS_UCS2:
						p->Append(ISO_ptr<string16>(id, str((const char16be*)s, r->length / 2)));
						break;
				};
				break;
		};
	}
	return p;
}

//-----------------------------------------------------------------------------
//	Compact Font Format
//	also known as a PostScript Type 1, or CIDFont
//	container to store multiple fonts together in a single unit known as a FontSet
//	allows embedding PostScript language code that permits additional flexibility and extensibility of the format for usage with printer environments
//-----------------------------------------------------------------------------

class CFF_VM : public PS_VM<fixed<16,16>> {
	typedef PS_VM<fixed<16,16>> B;
	using typename B::num;

	num				temps[32];
	num				width, nom_width;
	const uint8		*hintmask;
	const uint8		*cntrmask_array[8];
	const uint8		**cntrmask;
	bool			cleared, dotsection;

	struct subrs {
		cff::index *index;
		uint32		bias;
		subrs(cff::index *_index) : index(_index), bias(!_index ? 0 : _index->count < 1240 ? 107 : _index->count < 33900 ? 1131 : 32768) {}
		const_memory_block operator[](int i)	const { return index ? (*index)[i + bias] : empty; }
	} gsubrs, lsubrs;

	num		*stclear(size_t n)	{
		size_t	n0	= st.count();
		num		*p	= st.clear();
		if (!cleared && n0 > n) {
			cleared = true;
			width	= *p++;
			if (width < 0)
				width += nom_width;
		}
		return p;
	}

public:
	void	Interpret(const uint8 *p, const uint8 *e);
	void	Interpret(const const_memory_block &m) { Interpret(m, m.end());	}
	void	Reset(num _width) {
		PS_VM::Reset();
		cleared		= dotsection = false;
		cntrmask	= cntrmask_array;
		width		= _width;
	}
	float	Width() const { return width; }

	CFF_VM(cff::index *gs, cff::index *ls, num nom_width) : nom_width(nom_width), gsubrs(gs), lsubrs(ls) {}
};

void CFF_VM::Interpret(const uint8 *p, const uint8 *e) {
	num	v;
	while (p < e) {
		uint8	b0 = *p++;
		if (b0 < 0x20) {
			if (b0 == cff::escape)
				b0 = *p++ + 0x20;
			switch (b0) {
				// unary math ops
				case cff::op_not:		st[0] = !st[0];							break;
				case cff::op_abs:		st[0] = abs(st[0]);						break;
				case cff::op_neg:		st[0] = -st[0];							break;
				case cff::op_sqrt:		st[0] = sqrt(st[0]);					break;

				// logical ops
				case cff::op_and:		v = st.pop(); st[0] = v && st[0];		break;
				case cff::op_or:		v = st.pop(); st[0] = v || st[0];		break;

				// binary math ops
				case cff::op_add:		v = st.pop(); st[0] += v;				break;
				case cff::op_sub:		v = st.pop(); st[0] -= v;				break;
				case cff::op_mul:		v = st.pop(); st[0] *= v;				break;
				case cff::op_div:		v = st.pop(); st[0] /= v;				break;
				case cff::op_eq:		v = st.pop(); st[0] = st[0] == v;		break;

				// stack ops
				case cff::op_drop:		st.pop();								break;
				case cff::op_dup:		st.push(st[0]);							break;
				case cff::op_exch:		swap(st[0], st[1]);						break;
				case cff::op_roll:		v = st.pop(); st.roll(st.pop(), v);		break;
				case cff::op_index:		st.push(st[st.pop()]);					break;
				case cff::op_ifelse:	v = st.pop(); if (v < st.pop()) st[1] = st[0]; st.pop(); break;

				// storage ops
				case cff::op_put:		v = st.pop(); temps[v.to_int()] = st.pop();	break;
				case cff::op_get:		st[0] = temps[st[0].to_int()];			break;

				// flow control
				case cff::op_callsubr:	Interpret(lsubrs[st.pop().to_int()]);	break;
				case cff::op_callgsubr:	Interpret(gsubrs[st.pop().to_int()]);	break;
				case cff::op_return:	return;
				case cff::op_endchar:	stclear(0); return;

				// hint ops
				case cff::op_hstemhm:	//|- y dy {dya dyb}*
				case cff::op_hstem:	{	//|- y dy {dya dyb}*
					size_t	n = st.count() / 2;
					hstems.set(stclear(n * 2), n);
					break;
				}
				case cff::op_vstemhm:	//|- x dx {dxa dxb}*
				case cff::op_vstem:	{	//|- x dx {dxa dxb}*
					size_t	n = st.count() / 2;
					vstems.set(stclear(n * 2), n);
					break;
				}
				case cff::op_hintmask: {
					if (vstems.empty()) {
						size_t n = st.count() / 2;
						vstems.set(stclear(n * 2), n);
					}
					hintmask = p;
					p += (hstems.size() + vstems.size() + 7) / 8;
					break;
				}

				case cff::op_cntrmask:
					stclear(0);
					*cntrmask++ = p;
					p += (hstems.size() + vstems.size() + 7) / 8;
					break;

				case cff::op_dotsection:
					dotsection = !dotsection;
					st.clear();
					break;

				// flex path ops
				case cff::op_flex:	{	//|- dx1 dy1 dx2 dy2 dx3 dy3 dx4 dy4 dx5 dy5 dx6 dy6 fd
					num		*s = st.clear();
					num2	*v = (num2*)s;
					num2	p1 = pt + v[0], p2 = p1 + v[1], p3 = p2 + v[2], p4 = p3 + v[3], p5 = p4 + v[4];
					pt = p5 + v[5];
					num		fd = s[12] * 0.01f;
					BezierTo(p1, p2, p3);
					BezierTo(p4, p5, pt);
					break;
				}
				case cff::op_flex1:	{	//|- dx1 dy1 dx2 dy2 dx3 dy3 dx4 dy4 dx5 dy5 d6
					num		*s = st.clear();
					num2	*v = (num2*)s;
					num2	p1 = pt + v[0], p2 = p1 + v[1], p3 = p2 + v[2], p4 = p3 + v[3], p5 = p4 + v[4], d = p5 - pt;
					pt	= p5;
					pt[abs(d.x) <= abs(d.y)] += s[10];
					num		fd = 0.5f;
					BezierTo(p1, p2, p3);
					BezierTo(p4, p5, pt);
					break;
				}
				case cff::op_hflex:	{	//|- dx1 dx2 dy2 dx3 dx4 dx5 dx6
					num		*s = st.clear();
					num2	p1 = pt; p1.x += s[0];
					num2	p2 = p1 + *(num2*)(s + 1);
					num2	p3 = p2; p3.x += s[3];
					num2	p4 = p3; p4.x += s[4];
					num2	p5 = p4; p5.x += s[5];
					pt = p5; pt.x += s[6];
					num		fd = 0.5f;
					BezierTo(p1, p2, p3);
					BezierTo(p4, p5, pt);
					break;
				}
				case cff::op_hflex1: {	//|- dx1 dy1 dx2 dy2 dx3 dx4 dx5 dy5 dx6
					st.clear();
					num		*s = st.clear();
					num2	*v = (num2*)s;
					num2	p1 = pt + v[0], p2 = p1 + v[1];
					num2	p3 = p2; p3.x += s[4];
					num2	p4 = p3; p4.x += s[5];
					num2	p5 = p4 + v[3];
					pt = p5; pt.x += s[8];
					num		fd = 0.5f;
					BezierTo(p1, p2, p3);
					BezierTo(p4, p5, pt);
					break;
				}

				// path ops
				case cff::op_rmoveto:
					pt += *(num2*)stclear(2);
					MoveTo(pt);
					break;

				case cff::op_hmoveto:
					pt.x += *stclear(1);
					MoveTo(pt);
					break;

				case cff::op_vmoveto:
					pt.y += *stclear(1);
					MoveTo(pt);
					break;

				case cff::op_rlineto:	//|- {dxa dya}+
					for (num *e = st.sp, *s = st.clear(); s < e; s += 2) {
						pt += *(num2*)s;
						LineTo(pt);
					}
					break;

				case cff::op_hlineto:	//|- dx1 {dya dxb}*
					for (num *e = st.sp, *s = st.clear(); s < e;) {
						pt.x += *s++;
						LineTo(pt);
						if (s < e) {
							pt.y += *s++;
							LineTo(pt);
						}
					}
					break;

				case cff::op_vlineto:	//|- dy1 {dxa dyb}*
					for (num *e = st.sp, *s = st.clear(); s < e;) {
						pt.y += *s++;
						LineTo(pt);
						if (s < e) {
							pt.x += *s++;
							LineTo(pt);
						}
					}
					break;

				case cff::op_rcurveline: {//|- {dxa dya dxb dyb dxc dyc}+ dxd dyd
					num *e = st.sp - 2, *s = st.clear();
					while (s < e) {
						num2	*v = (num2*)s;
						num2	p1 = pt + v[0], p2 = p1 + v[1];
						pt = p2 + v[2];
						BezierTo(p1, p2, pt);
						s += 6;
					}
					pt += *(num2*)s;
					LineTo(pt);
					break;
				}

				case cff::op_rlinecurve: {//|- {dxa dya}+ dxb dyb dxc dyc dxd dyd
					num *e = st.sp - 6, *s = st.clear();
					while (s < e) {
						pt += *(num2*)s;
						LineTo(pt);
						s += 2;
					}
					num2	*v = (num2*)s;
					num2	p1 = pt + v[0], p2 = p1 + v[1];
					pt = p2 + v[2];
					BezierTo(p1, p2, pt);
					break;
				}

				case cff::op_rrcurveto:	//|- {dxa dya dxb dyb dxc dyc}+
					for (num *e = st.sp, *s = st.clear(); s < e; s += 6) {
						num2	*v = (num2*)s;
						num2	p1 = pt + v[0], p2 = p1 + v[1];
						pt = p2 + v[2];
						BezierTo(p1, p2, pt);
					}
					break;

				case cff::op_hhcurveto: //|- dy1? {dxa dxb dyb dxc}+
					for (num *e = st.sp, *s = st.clear(); s < e; s += 4) {
						num2	p1 = pt;
						if (s == &st.bot() && ((e - s) & 1))
							p1.y += *s++;
						p1.x += s[0];
						num2	p2 = p1 + *(num2*)(s + 1);
						pt = p2; pt.x += s[3];
						BezierTo(p1, p2, pt);
					}
					break;

				case cff::op_vvcurveto: //|- dx1? {dya dxb dyb dyc}+
					for (num *e = st.sp, *s = st.clear(); s < e; s += 4) {
						num2	p1 = pt;
						if (s == &st.bot() && ((e - s) & 1))
							p1.x += *s++;
						p1.y += s[0];
						num2	p2 = p1 + *(num2*)(s + 1);
						pt = p2; pt.y += s[3];
						BezierTo(p1, p2, pt);
					}
					break;

				case cff::op_hvcurveto: //|- dx1 dx2 dy2 dy3 {dya dxb dyb dxc dxd dxe dye dyf}* dxf?
					for (num *e = st.sp, *s = st.clear(); s < e; ) {
						num2	p1 = pt;
						p1.x += s[0];
						num2	p2 = p1 + *(num2*)(s + 1);
						pt = p2; pt.y += s[3];
						s	+= 4;
						if (s == e - 1)
							pt.x += *s++;
						BezierTo(p1, p2, pt);
						if (s < e) {
							p1 = pt;
							p1.y += s[0];
							p2 = p1 + *(num2*)(s + 1);
							pt = p2; pt.x += s[3];
							s	+= 4;
							if (s == e - 1)
								pt.y += *s++;
							BezierTo(p1, p2, pt);
						}
					}
					break;

				case cff::op_vhcurveto: //|- dy1 dx2 dy2 dx3 {dxa dxb dyb dyc dyd dxe dye dxf}* dyf?
					for (num *e = st.sp, *s = st.clear(); s < e; ) {
						num2	p1 = pt;
						p1.y += s[0];
						num2	p2 = p1 + *(num2*)(s + 1);
						pt = p2; pt.x += s[3];
						s	+= 4;
						if (s == e - 1)
							pt.y += *s++;
						BezierTo(p1, p2, pt);
						if (s < e) {
							p1 = pt;
							p1.x += s[0];
							p2 = p1 + *(num2*)(s + 1);
							pt = p2; pt.y += s[3];
							s	+= 4;
							if (s == e - 1)
								pt.x += *s++;
							BezierTo(p1, p2, pt);
						}
					}
					break;

				// misc ops
//				case cff::op_random:	st.push(random); break;;
				case cff::op_shortint:
					st.push((p[0] << 8) | p[1]);
					p += 2;
					break;

				default:
					break;
			}

		} else if (b0 == cff::op_fixed16_16) {
			st.push(*(packed<BE(num)>*)p);
			p += sizeof(num);

		} else {
			st.push(  b0 < 247	?  int(b0) - 139						// 32  < b0 < 246:	bytes:1; range:�107..+107
					: b0 < 251	? (int(b0) - 247) * +256 + *p++ + 108	// 247 < b0 < 250:	bytes:2; range:+108..+1131
								: (int(b0) - 251) * -256 - *p++ - 108	// 251 < b0 < 254:	bytes:2; range:�1131..�108
			);
		}
	}
}

ISO_ptr<void> CFFDictValue(tag id, const cff::value &v) {
	if (v.type == cff::t_int)
		return ISO_ptr<int>(id, (int&)v.data);
	else
		return ISO_ptr<float>(id, (float&)v.data);
}
ISO_ptr<void> ReadCFFDict(tag id, const cff::dictionary &dict, cff::index *strings) {
	static const char *props[] = {
		"version",			"Notice",			"FullName",			"FamilyName",			"Weight",				"FontBBox",			"BlueValues",		"OtherBlues",
		"FamilyBlues",		"FamilyOtherBlues",	"StdHW",			"StdVW",				"escape",				"UniqueID",			"XUID",				"charset",
		"Encoding",			"CharStrings",		"Private",			"Subrs",				"defaultWidthX",		"nominalWidthX",	"-Reserved-",		"-Reserved-",
		"-Reserved-",		"-Reserved-",		"-Reserved-",		"-Reserved-",			"shortint",				"longint",			"BCD",				"-Reserved-",
		"Copyright",		"isFixedPitch",		"ItalicAngle",		"UnderlinePosition",	"UnderlineThickness",	"PaintType",		"CharstringType",	"FontMatrix",
		"StrokeWidth",		"BlueScale",		"BlueShift",		"BlueFuzz",				"StemSnapH",			"StemSnapV",		"ForceBold",		"-Reserved-",
		"-Reserved-",		"LanguageGroup",	"ExpansionFactor",	"initialRandomSeed",	"SyntheticBase",		"PostScript",		"BaseFontName",		"BaseFontBlend",
		"-Reserved-",		"-Reserved-",		"-Reserved-",		"-Reserved-",			"-Reserved-",			"-Reserved-",		"ROS",				"CIDFontVersion",
		"CIDFontRevision",	"CIDFontType",		"CIDCount",			"UIDBase",				"FDArray",				"FDSelect",			"FontName",
	};

	ISO_ptr<anything>	p(id, uint32(dict.size()));
	for (auto &i : dict) {
		ISO_ptr<void>		&d		= (*p)[dict.index_of(i)];
		const char			*id		= i.p < iso::num_elements(props) ? props[i.p] : "bad_enum";
		auto				vals	= dict.get_values(&i);
		cff::Type			type	= vals[0].type;
		uint32				num		= vals.size32();
		if (num > 1) {
			bool	hetero = false;
			for (int i = 1; !hetero && i < num; i++)
				hetero = vals[i].type != type;
			if (hetero) {
				ISO_ptr<anything>	x(id, num);
				for (int i = 0; i < num; i++)
					(*x)[i] = CFFDictValue(none, vals[i]);
				d = x;
			} else {
				if (type == cff::t_int) {
					ISO_ptr<ISO_openarray<int>>	x(id, num);
					for (int i = 0; i < num; i++)
						(*x)[i] = (int&)vals[i].data;
					d = x;
				} else {
					ISO_ptr<ISO_openarray<float>>	x(id, num);
					for (int i = 0; i < num; i++)
						(*x)[i] = (float&)vals[i].data;
					d = x;
				}
			}
		} else switch (i.p) {
			case cff::version:
			case cff::Notice:
			case cff::FullName:
			case cff::FamilyName:
			case cff::Weight:
			case cff::Copyright:
			case cff::BaseFontName:
			case cff::PostScript:
				if ((int)vals[0].data >= cff::nStdStrings) {
					const_memory_block	m = (*strings)[vals[0].data - cff::nStdStrings];
					d = ISO_ptr<string>(id, str((const char*)m, m.length()));
					break;
				}
			default:
				d = CFFDictValue(id, vals[0]);
				break;
		}
	}
	return p;
}

ISO_ptr<void> ReadCFFStrings(tag id, cff::index *ind) {
	ISO_ptr<ISO_openarray<string>>	p(id, ind->count);
	for (int i = 0, n = ind->count; i < n; i++) {
		const_memory_block	m = (*ind)[i];
		(*p)[i] = str((const char*)m, m.length());
	}
	return p;
}

ISO_ptr<void> ReadCFFBlocks(tag id, cff::index *ind) {
	ISO_ptr<ISO_openarray<ISO_openarray<uint8>>>	p(id, ind->count);
	for (int i = 0, n = ind->count; i < n; i++) {
		const_memory_block	m = (*ind)[i];
		memcpy((*p)[i].Create(m.size32()), m, m.length());
	}
	return p;
}

ISO_ptr<void> ReadCFFCharSet(tag id, uint8 *p, int nglyphs) {
	ISO_ptr<ISO_openarray<pair<uint16,uint16>>	> r(id);
	--nglyphs;
	switch (*p++) {
		case 0:
			for (cff::charset0 *c = (cff::charset0*)p; nglyphs--; c++)
				r->Append(make_pair(c->sid, 1));
			break;
		case 1:
			for (cff::charset1 *c = (cff::charset1*)p; nglyphs; c++) {
				r->Append(make_pair(c->sid, c->left + 1));
				nglyphs -= c->left + 1;
			}
			break;
		case 2:
			for (cff::charset2 *c = (cff::charset2*)p; nglyphs; c++) {
				r->Append(make_pair(c->sid, c->left + 1));
				nglyphs -= c->left + 1;
			}
			break;
	}
	return r;
}

//-----------------------------------------------------------------------------
//	Colours
//-----------------------------------------------------------------------------
void glyph_fill::colour_line::reverse() {
	iso::reverse(colours);
	for (auto &i : colours)
		i.t = 1 - i.t;
}

void glyph_fill::colour_line::clip(interval<float> t) {
	auto	b = upper_boundc(colours, t.b, [](float t, const colour_stop &c) { return t < c.t; });
	if (b != colours.end()) {
		
		b->c	= evaluate(b[-1], b[0], t.b);
		b->t	= t.b;
		colours.erase(b + 1, colours.end());
	}

	auto	a = lower_boundc(colours, t.a, [](const colour_stop &c, float t) { return c.t < t; });
	if (a != colours.begin()) {
		--a;
		a->c	= evaluate(a[0], a[1], t.a);
		a->t	= t.a;
		colours.erase(colours.begin(), a);
	}
}

void glyph_fill::colour_line::strip_redundant(float epsilon) {
	for (auto i = colours.begin(); i < colours.end() - 2; ) {
		if (i[1].t != i[0].t) {
			auto	dist2 = len2(i[1].c - evaluate(i[0], i[2], i[1].t));
			if (dist2 < epsilon) {
				colours.erase(i + 1);
				continue;
			}
		}
		++i;
	}
}

void glyph_fill::optimise(interval<float> t) {
	if (cols.get_range().contains(t))
		cols.extend = cols.EXTEND_NONE;

//	cols.clip(t);
	if (cols.colours.size() < 2)
		grad.Set(GradientTransform::SOLID);

	auto	t2 = cols.get_range();
	if (grad.scale_range(t2)) {
		for (auto &i : cols.colours)
			i.t = t2.to(i.t);
	}

	cols.strip_redundant();

//	if (extend == EXTEND_NONE && colours.size() == 2)
//		ISO_TRACEF("only two!\n");
}

interval<float> glyph_layer::get_extent() const {
	switch (grad.fill) {
		default:
		case GradientTransform::SOLID:
			return {0, 0};

		case GradientTransform::LINEAR: {
			rectangle	ext2 = (rectangle(iso::get_extent<position2>(*this)) / grad.transform).get_box();
			return {ext2.a.v.x, ext2.b.v.x};
		}
		case GradientTransform::SWEEP:
			// could check for centre being outside glyph and restrict angles?
			return {0, 1};

		case GradientTransform::RADIAL: {
		case GradientTransform::RADIAL1:
		case GradientTransform::RADIAL_LT1_SWAP:
		case GradientTransform::RADIAL_SAME:
			interval<float>	t = none;
			for (float2 i : *this)
				t |= grad.get_value(position2(i));
			return t;
		}
		case GradientTransform::RADIAL0: {
		case GradientTransform::RADIAL_GT1:
			interval<float>	t(zero);//assume centre is in rect
			for (float2 i : *this)
				t |= grad.get_value(position2(i));
			return t;
		}
	}
}

struct glyph_layers_maker {
	glyph_layers			cg;
	const ttf::COLR1		*head;
	const ttf::CPAL::Color	*palette;
	dynamic_array<curves>	&all_curves;
	float2x3				transform	= identity;
	curves					temp_curves;

	template<typename T> void	Transform(const T* p) {
		save(transform, transform * p->matrix()),
			ttf::process<void>(p->paint.get(p), *this);
	}

	glyph_layer*	AddSub() {
		if (temp_curves.empty())
			return nullptr;
		auto	&sub	= cg.push_back();
		(curves&)sub	= move(temp_curves);
		temp_curves.clear();
		return &sub;
	}

	rgba8 GetColour(int i) {
		auto	&c = palette[i];
		return {
			reinterpret_cast<const unorm8&>(c.r),
			reinterpret_cast<const unorm8&>(c.g),
			reinterpret_cast<const unorm8&>(c.b),
			reinterpret_cast<const unorm8&>(c.a)
		};
	}

	rgba8 GetColour(int i, float alpha) {
		auto	c = GetColour(i);
		c.a *= alpha;
		return c;
	}

	glyph_fill::colour_line GetColours(ttf::ColorLine *line) {
		glyph_fill::colour_line	cols;
		cols.extend	= glyph_fill::colour_line::EXTEND(line->extend + 1);
		for (auto& i : line->stops())
			cols.colours.emplace_back(GetColour(i.paletteIndex, get(i.alpha)), get(i.stopOffset));
		return cols;
	}

	glyph_layers_maker(const ttf::COLR1 *head, const ttf::CPAL::Color *palette, dynamic_array<curves> &all_curves)
		: head(head), palette(palette), all_curves(all_curves) {}

	glyph_layers make(const ttf::PaintBase* p);

	void	operator()(const ttf::Paint<ttf::ColrLayers> *p) {
		auto	layers	= head->layers1();
		for (auto &i : layers->all().slice(p->firstLayerIndex, p->numLayers))
			ttf::process<void>(i.get(layers), *this);
	}

	void	operator()(const ttf::Paint<ttf::Solid> *p) {
		if (auto sub = AddSub())
			sub->SetSolidColour(GetColour(p->paletteIndex));
	}
	void	operator()(const ttf::Paint<ttf::LinearGradient> *p) {
		if (auto sub = AddSub()) {
			sub->cols = GetColours(p->colorLine.get(p));
			sub->grad.SetLinear({p->x0, p->y0}, {p->x1, p->y1}, {p->x2, p->y2});
			sub->grad.Transform(transform);
		}
	}
	void	operator()(const ttf::Paint<ttf::RadialGradient> *p) {
		if (auto sub = AddSub()) {
			sub->cols = GetColours(p->colorLine.get(p));
			sub->grad.SetRadial(circle({p->x0, p->y0}, p->radius0), circle({p->x1, p->y1}, p->radius1));
			sub->grad.Transform(transform);
		}

	}
	void	operator()(const ttf::Paint<ttf::SweepGradient> *p) {
		if (auto sub = AddSub()) {
			sub->cols = GetColours(p->colorLine.get(p));
			sub->grad.SetSweep({p->centerX, p->centerY}, p->startAngle.get(), p->endAngle.get());
			sub->grad.Transform(transform);
		}
	}

	void	operator()(const ttf::Paint<ttf::Glyph> *p) {
		auto	&g = all_curves[p->glyphID];
		if (direction(g) ^ (transform.det() < 0))
		//if (direction(g))
			reverse_curves(g);
		//ISO_ASSERT(transform.det() > 0);
		temp_curves.append(transformc(g, [m = transform](curve_vertex &i) { return m * i; }));
		ttf::process<void>(p->paint.get(p), *this);
	}
	void	operator()(const ttf::Paint<ttf::ColrGlyph> *p) {
		auto	base	= head->base_glyphs1();
		ttf::process<void>(base->all()[p->glyphID].paint.get(base), *this);
	}

	void	operator()(const ttf::Paint<ttf::Transform> *p)					{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::Translate> *p)					{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::Scale> *p)						{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::ScaleAroundCenter> *p)			{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::ScaleUniform> *p)				{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::ScaleUniformAroundCenter> *p)	{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::Rotate> *p)					{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::RotateAroundCenter> *p)		{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::Skew> *p)						{ Transform(p); }
	void	operator()(const ttf::Paint<ttf::SkewAroundCenter> *p)			{ Transform(p); }

	void	operator()(const ttf::Paint<ttf::Composite> *p) {
		auto	mode = p->mode;
		ttf::process<void>(p->backdrop.get(p), *this);
		ttf::process<void>(p->source.get(p), *this);
	}

	void	operator()(const ttf::PaintBase *p) {
	}

};

glyph_layers glyph_layers_maker::make(const ttf::PaintBase* p) {
	cg.clear();
	temp_curves.clear();
	transform	= identity;
	ttf::process<void>(p, *this);

	ISO_ASSERT(!cg.empty());
	if (cg) {
		auto	i	= cg.begin(), e = cg.end(), d = i++;
		for (;i != e; ++i) {
			if (*(glyph_fill*)i == *(glyph_fill*)d && !overlap(*i, *d)) {
				d->append(*i);
			} else {
				++d;
				*d = move(*i);
			}
		}
		cg.erase(++d, e);
		for (auto &i : cg) {
			i.optimise(i.get_extent());
		}
	}
	return move(cg);
}

//-----------------------------------------------------------------------------
//	TTF & OTF FileHandlers
//-----------------------------------------------------------------------------

struct ISO_new {
	const ISO::Type*	type;
	tag2				id;
	ISO_new(const ISO::Type *type, tag2 id = tag2()) : type(type), id(id) {}
	void* alloc(size_t size) const {
		return ISO::MakeRawPtrSize<32>(type, id, size);
	}
};

template<typename T> struct ISO_newT : ISO_new {
	ISO_newT(tag2 id = tag2()) : ISO_new(ISO::getdef<T>(), id) {}
};


void* operator new(size_t size, const ISO_new& n) {
	return n.alloc(size);
}

class TTFFileHandler : public FileHandler {
	const char*		GetExt()				override { return "ttf"; }
	const char*		GetDescription()		override { return "Truetype font";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		return is_any(file.get<uint32be>(), 0x00010000, 'true', 'typ1') ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override;

	struct Wrapper : FontWrapper {
		Wrapper(ISO_ptr<anything> pa);
	};
	static ISO_ptr<FontWrapper> to_wrapper(ISO_ptr<anything> a) {
		return ISO::GetPtr((new(ISO_newT<FontWrapper>(none)) Wrapper(a)));
		//return new(ISO_new(ISO::getdef<FontWrapper>())) Wrapper(a);
	}
public:
	TTFFileHandler() {
		ISO_get_conversion(to_wrapper);
	}
} ttf_fh;


void svg_layers(glyph_layers &layers, ISO::Browser2 b, ISO_ptr<void> fill, float2x3 transform) {
	if (b.GetType() == ISO::REFERENCE)
		b = *b;

	if (b.Is<ISO_openarray_machine<curve_vertex>>()) {
		auto&	sub		= layers.push_back();
		(curves&)sub	= transformc((ISO_openarray_machine<curve_vertex>&)b, [&](curve_vertex &i) { return transform * i; });
		//if (direction(sub)) {
			reverse_curves(sub);
		//}
		if (fill) {
			if (fill.IsType<rgba8>()) {
				sub.SetSolidColour(*(rgba8*)fill);
			}
		}

	} else if (b.Is<Filled>()) {
		const Filled	*f = b;
		svg_layers(layers, f->element, f->fill, transform);

	} else  if (b.Is<Transformed>()) {
		const Transformed *t = b;
		svg_layers(layers, t->element, fill, t->transform * transform);

	} else {
		for (auto i : b)
			svg_layers(layers, i, fill, transform);
	}
}

void svg_curves(curves &c, glyph_layers &layers, ISO::Browser2 b) {
#if 1
	svg_layers(layers, b, ISO_NULL, scale(1,-1));
#else
	static const float2x3	transform	= identity;
	//static const float2x3	transform	= scale(1,-1);

	if (b.GetType() == ISO::REFERENCE)
		b = *b;

	if (b.Is<ISO_openarray_machine<curve_vertex>>()) {
		c.append(transformc((ISO_openarray_machine<curve_vertex>&)b, [](const curve_vertex &i) { return transform * i; }));

	} else if (b.Is<Filled>()) {
		const Filled	*f = b;
		svg_layers(layers, f->element, f->fill, scale(1,-1));

	} else  if (b.Is<Transformed>()) {
		const Transformed *t = b;
		svg_layers(layers, t->element, ISO_NULL, t->transform * transform);

	} else {
		for (auto i : b)
			svg_curves(c, layers, i);
	}
#endif
}

curves	FlattenGlyph(ISO_ptr<void> p) {
	curves	out;
	if (p.IsType<iso_simpleglyph>()) {
		out = *(iso_simpleglyph*)p;

	} else if (p.IsType<iso_compoundglyph>()) {
		for (auto& i : *(iso_compoundglyph*)p) {
			if (i.a)
				out.append(transformc(FlattenGlyph(i.a), [m = float2x3(i.b)](const curve_vertex &i) { return m * i; }));
		}
	}

	if (direction(out))
		reverse_curves(out);
	return out;
}


TTFFileHandler::Wrapper::Wrapper(ISO_ptr<anything> pa) {
	auto		&a		= *pa;
	ttf::hhea	*hhea	= ISO::Browser(a["hhea"]);
	ttf::hmtx	*hmtx	= ISO::Browser(a["hmtx"]);


	if (const ttf::cmap* cmap = ISO::Browser(a["cmap"])) {
		const ttf::cmap::cmap_table	*uvs_table	= nullptr;

		//score the tables to favour UNICODE_FULL
		temp_array<uint32>	scores = transformc(cmap->tables, [&uvs_table](const ttf::cmap::cmap_table &i) {
			switch (i.platform) {
				case ttf::cmap::UNICODE:
					if (i.encoding == ttf::cmap::UNICODE_UVS)
						uvs_table = &i;
					return  abs(i.encoding - ttf::cmap::UNICODE_FULL);
				case ttf::cmap::WINDOWS:
					return i.encoding == ttf::cmap::WIN_UNICODE_FULL ? 1 : i.encoding == ttf::cmap::WIN_UNICODE_BMP ? 2 : 3;
				default:
					return 4;
			}
		});
		
		uint32	i	= scores.index_of(argmin(scores));
		max_char	= cmap->tables[i].data.get(cmap)->get(glyph_map);
	}

#if 0
	if (const ttf::GSUB* gsub = ISO::Browser(a["GSUB"])) {
		auto	scripts		= gsub->scripts.get(gsub);
		auto	features	= gsub->features.get(gsub);
		auto	lookups		= gsub->lookups.get(gsub);

		auto	script		= scripts->begin()->script.get(scripts);
		auto	lang		= script->defaultLangSys.get(script);

		uint32	dummy_char	= max_char + 1;

		for (auto& i : lang->featureIndices) {
			auto	f = (*features)[i];
			if (f.tag == "ccmp"_u32) {
				auto	f2	= f.feature.get(features);
				for (auto& j : f2->indices) {
					auto	*lookup = (*lookups)[j].get(lookups);

					for (auto& k : lookup->subtables) {
						auto	table	= k.get(lookup);
						uint32	format	= table->format;

						switch (lookup->type) {
							/*
							case ttf::GSUB::SINGLE: {
								ISO_ASSERT(format == 2);
								auto	table1 = (ttf::GSUB::Single2*)table;
								auto	all		= table1->substitutes.all();
								break;
							}*/
							case ttf::GSUB::LIGATURE: {
								ISO_ASSERT(format == 1);
								auto	table1 = (ttf::GSUB::Ligature1*)table;
								for (auto& set : table1->sets) {
									auto	set2 = set.get(table1);
									for (auto& lig : *set2) {
										auto	lig2	= lig.get(set2);
										auto&	dest	= ligatures.push_back();
										dest.a			= lig2->ligature;
										dest.b.glyphs	= *lig2;

										glyph_map[dummy_char++] = dest.a;
									}
								}
								break;
							}
							default:
								break;

						}
					}

				}

			}
		}
	}
#endif

	hash_map<uint32, uint32>	glyphid_to_index;
	temp_array<uint32>			index_to_glyphid(glyph_map.size());

	uint32	j = 0;
	for (auto& i : glyph_map) {
		if (!glyphid_to_index[*i].exists()) {
			glyphid_to_index[*i]	= j;
			index_to_glyphid[j]		= *i;
			++j;
		}
	}

	for (auto& i : glyph_map)
		*i	= glyphid_to_index[*i];

	glyphs.resize(j);

	dynamic_array<curves>	all_curves;

	if (auto b = ISO::Browser(a["glyphs"])) {
		anything	&in_curves = b;
		all_curves	= transformc(in_curves, [](const ISO_ptr<void> &p) { return FlattenGlyph(p); });

		for (auto &g : glyphs) {
			int	i2	= index_to_glyphid[glyphs.index_of(g)];
			g		= glyph(all_curves[i2], hmtx->left(hhea, i2), hmtx->advance(hhea, i2));
		}

	} else if (b = ISO::Browser(a["CFF"])) {
		anything	&in_curves	= b["curves"];
		all_curves	= transformc(in_curves, [](const iso_simpleglyph *p)->curves {
			return *p;
		});

		int		*bbox	= b["dict"]["isFixedPitch"].get<bool>() ? (int*)b["dict"]["FontBBox"][0] : nullptr;
		
		for (auto &g : glyphs) {
			int	i2	= index_to_glyphid[glyphs.index_of(g)];
			g		= glyph(all_curves[i2], hmtx->left(hhea, i2), bbox ? bbox[2] : (float)hmtx->advance(hhea, i2));
		}
	}


#if 1
	if (const ttf::SVG* svg = ISO::Browser(a["SVG"])) {
		if (auto fh = FileHandler::Get("svg")) {
			for (auto&& i : svg->documents()) {
				auto			d	= get(i);
				ISO::Browser2	svg = fh->Read(none, memory_reader(d.data));

				if (d.start == d.end) {
					svg_curves(all_curves[d.start], layers_map[glyphid_to_index[d.start]], svg);
				} else {
					for (uint32 g = 0; g <= d.end - d.start; g++) {
						svg_curves(all_curves[g + d.start], layers_map[glyphid_to_index[g + d.start]], svg[g]);
					}
				}
			}
		}
	}
#endif
	if (ttf::COLR0 *colr = ISO::Browser(a["COLR"])) {
		ttf::CPAL	*cpal = ISO::Browser(a["CPAL"]);

		switch (colr->version) {
			case 0: {
				break;
			}
			case 1: {
				auto	head	= static_cast<const ttf::COLR1*>(colr);
				auto	base	= head->base_glyphs1();
				glyph_layers_maker	maker(head, cpal->colors.get(cpal) + cpal->colorRecordIndices[0], all_curves);

				for (auto &i0 : base->all()) {
					auto	i2 = glyphid_to_index[i0.glyphID];
					if (i2.exists())
						layers_map[i2] = maker.make(i0.paint.get(base));
				}
				break;
			}
			default:
				break;
		}

	}

	height		= hhea->ascent - hhea->descent;
	baseline	= hhea->ascent;
	top			= 0;
	spacing		= height + hhea->lineGap;
}

class OTFFileHandler : public TTFFileHandler {
	const char*		GetExt()				override { return "otf"; }
	const char*		GetDescription()		override { return "Opentype font";	}
	int				Check(istream_ref file)	override {
		return 0;
		file.seek(0);
		return file.get<uint32be>() == 'OTTO' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
} otf_fh;

ISO_DEFUSERCOMPV(ttf::head,
	version, font_revision, checksum_adjustment, magic, flags, units_per_em, created, modified,
	xMin, yMin, xMax, yMax,
	macStyle, lowestRecPPEM, fontDirectionHint, indexToLocFormat, glyphDataFormat
);

ISO_DEFUSERCOMPV(ttf::maxp,
	version,numGlyphs,maxPoints,maxContours,maxComponentPoints,maxComponentContours,maxZones,maxTwilightPoints,
	maxStorage,maxFunctionDefs,maxInstructionDefs,maxStackElements,maxSizeOfInstructions,maxComponentElements,maxComponentDepth
);

ISO_DEFUSERCOMPV(ttf::PANOSE,bFamilyType,bSerifStyle,bWeight,bProportion,bContrast,bStrokeVariation,bArmStyle,bLetterform,bMidline,bXHeight);
ISO_DEFUSERCOMPV(ttf::OS2,
	version,avg_char_width,weight_class,width_class,type,
	subscript_size_x,subscript_size_y,subscript_offset_x,subscript_offset_y,superscript_size_x,superscript_size_y,superscript_offset_x,superscript_offset_y,
	strikeout_size,strikeout_position,family_class,panose,unicode_range,vend_id,selection,first_char_index,last_char_index,
	typo_ascender,typo_descender,typo_line_gap,win_ascent,win_descent,codepage_range,height,cap_height,default_char,break_char,max_context
);


ISO_DEFUSERCOMPV(ttf::hhea,
	version, ascent, descent, lineGap, advanceWidthMax, minLeftSideBearing, minRightSideBearing,xMaxExtent,
	caretSlopeRise, caretSlopeRun, caretOffset, reserved, metricDataFormat, numOfLongHorMetrics
);

ISO_DEFUSERCOMPV(ttf::sbix, version, flags, numStrikes, strikes2);
ISO_DEFUSERCOMPV(ttf::sbix::strike, ppem, ppi, glyphs2);
ISO_DEFUSERCOMPV(ttf::sbix::glyph, originOffsetX, originOffsetY, graphicType);

struct glyph_ref {
	const ttf::sbix::glyph	*g;
	const void	*end;

	ISO_ptr<void>	image() const {
		switch (g->graphicType) {
			case g->JPG:	return FileHandler::Read(none, memory_reader(g->data, end), "jpg");
			case g->PNG:	return FileHandler::Read(none, memory_reader(g->data, end), "png");
			case g->TIFF:	return FileHandler::Read(none, memory_reader(g->data, end), "tiff");
			default:		return ISO_NULL;
		}
	}
};
ISO_DEFUSERCOMPV(glyph_ref, g, image);

glyph_ref get(const param_element<offset_pointer<ttf::sbix::glyph, uint32be, ttf::sbix::strike>&, const ttf::sbix::strike*>& p) {
	return {p.t.get(p.p), (&p.t)[1].get(p.p)};
}

ISO_DEFUSERCOMPV(ttf::SVG::Document_ref, start, end, data);
ISO_DEFUSERCOMPV(ttf::SVG, documents);

ISO_DEFUSERCOMPV(ttf::GSUB, majorVersion, minorVersion);

#if 0
void dump(const ttf::Coverage* cov) {
	ISO_TRACEF("coverage fmt ") << cov->format << '\n';
	switch (cov->format) {
		case 1: {
			auto	cov1 = (ttf::Coverage1*)cov;
			for (auto &i : cov1->glyphs)
				ISO_TRACEF("glyph=") << hex(i) << '\n';
			break;
		}
		case 2: {
			auto	cov1 = (ttf::Coverage2*)cov;
			for (auto& i : cov1->ranges)
				ISO_TRACEF("start=") << hex(i.start) << " end=" << hex(i.end) << " cov=" << hex(i.startCoverageIndex) << '\n';
			break;
		}
		default: ISO_ASSERT(0);
	}
}

void dump(const ttf::SequenceLookup& seq) {

}

void dump(const ttf::GSUB* gsub) {
	auto	scripts = gsub->scripts.get(gsub);
	auto	features = gsub->features.get(gsub);
	auto	lookups = gsub->lookups.get(gsub);

	auto	script	= scripts->begin()->script.get(scripts);
	auto	lang	= script->defaultLangSys.get(script);

	for (auto& i : lang->featureIndices) {
		auto	f = (*features)[i];
		ISO_TRACEF("TAG:") << (char(&)[4])f.tag << '\n';
		auto	f2	= f.feature.get(features);

		for (auto& j : f2->indices) {
			auto	*lookup = (*lookups)[j].get(lookups);
			ISO_TRACEF("index:") << j << " type:" << lookup->type << " flag:" << hex(lookup->flag) << '\n';

			for (auto& k : lookup->subtables.all()) {
				auto	table	= k.get(lookup);
				uint32	format	= table->format;

				ISO_TRACEF("format:") << table->format << '\n';
				switch (lookup->type) {
					case ttf::GSUB::SINGLE:
						break;
					case ttf::GSUB::MULTIPLE:
						break;
					case ttf::GSUB::ALTERN:
						break;

					case ttf::GSUB::LIGATURE: {
						ISO_ASSERT(format == 1);
						auto	table1 = (ttf::GSUB::Ligature1*)table;
						dump(table1->coverage.get(table1));
						for (auto& set : table1->sets) {
							auto	set2 = set.get(table1);
							for (auto& lig : *set2) {
								auto	lig2 = lig.get(set2);
								ISO_TRACEF("ligature ") << hex(lig2->ligature) << ':';
								for (auto &g : *lig2)
									ISO_TRACEF(" ") << hex(g);
								ISO_TRACEF("\n");
							}
						}
						break;
					}

					case ttf::GSUB::CONTEXTUAL:
						switch (format) {
							case 3: {
								auto	table1 = (ttf::SequenceContext3*)table;

								for (auto &i : table1->coverages()) 
									dump(i.get(table1));
								for (auto &i : table1->seqLookup()) 
									dump(i);

								break;
							}
							default: ISO_ASSERT(0);
						}
						break;

					case ttf::GSUB::CHAINED:
						switch (format) {
							case 3: {
								auto	table1 = (ttf::ChainedSequenceContext3*)table;

								for (auto &i : table1->backtrack())
									dump(i.get(table1));
								for (auto &i : table1->input())
									dump(i.get(table1));
								for (auto &i : table1->lookahead())
									dump(i.get(table1));
								for (auto &i : table1->seqLookup())
									dump(i);


								break;
							}
							default: ISO_ASSERT(0);
						}
						break;

					case ttf::GSUB::EXTENSION:
						break;
					case ttf::GSUB::REVERSE:
						break;
				}
			}

		}

	}

}
#endif

ISO_ptr<void> TTFFileHandler::Read(tag id, istream_ref file) {
	TTF	t(file);

	ISO_ptr<anything>	p(id);

	for (auto &table : t.tables) {
		uint32	length	= table.length;
		uint32	offset	= table.offset;
		switch (table.tag) {
			case "loca"_u32:
				break;

			case "head"_u32:
				p->Append(ReadBlockT<ttf::head>("head",file, offset, length));
				break;

			case "glyf"_u32: {
				auto	mb = ReadTable(file, t.FindTable("loca"_u32));
				dynamic_array<uint32>	locs;
				if (t.head->indexToLocFormat == 0) {
					locs	= make_range<uint16be>(mb);
					for (auto &i : locs)
						i *= 2;
				} else {
					locs	= make_range<uint32be>(mb);
				}
				ISO_ptr<anything>	x("glyphs", locs.size32() - 1);
				auto	*loc = locs.begin();
				for (auto &i : *x) {
					if (auto mb = ReadBlock(file, table.offset + loc[0], loc[1] - loc[0])) {
						if (((ttf::glyf::glyph*)mb)->num_contours >= 0)
							i = ReadSimpleGlyph(mb);
					}
					++loc;
				}

				loc = locs.begin();
				for (auto &i : *x) {
					if (!i) {
						if (auto mb = ReadBlock(file, table.offset + loc[0], loc[1] - loc[0])) {
							if (((ttf::glyf::glyph*)mb)->num_contours < 0)
								i = ReadCompoundGlyph(mb, *x);
						}
					}
					++loc;
				}

				p->Append(x);
				break;
			}

			case "cvt "_u32: {
				file.seek(offset);
				uint32	n = length / sizeof(ttf::fword);
				ISO_ptr<ISO_openarray<int16>>	x("cvt", n);
				for (int16 *d = *x; n--; d++)
					*d = file.get<ttf::fword>();
				p->Append(x);
				break;
			}
			case "name"_u32:
				p->Append(ReadNAME("names", ReadBlock(file, offset, length)));
				break;

			case "CFF "_u32: {
				ISO_ptr<anything>	x("CFF");
				auto	buff = ReadBlock(file, offset, length);
				cff::header	*h			= buff;
				cff::index	*names		= (cff::index*)((uint8*)h + h->hdrSize);
				cff::index	*dict		= (cff::index*)names->end();
				cff::index	*strings	= (cff::index*)dict->end();
				cff::index	*gsubrs		= (cff::index*)strings->end();
				cff::index	*lsubrs		= 0;
				float		nominalWidth = 0, defaultWidth = 0;

				cff::dictionary					pub_dict(dict);
				cff::dictionary_with_defaults	pub_dict2(pub_dict, cff::dict_top_defaults());

				x->Append(ReadCFFStrings("names", names));
				x->Append(ReadCFFDict("dict", pub_dict, strings));
				x->Append(ReadCFFStrings("strings", strings));
				x->Append(ReadCFFBlocks("global_subrs", gsubrs));

				if (auto e = pub_dict.lookup(cff::Private)) {
					void	*prvt	= (uint8*)h + e[1].data;
					
					cff::dictionary					prv_dict(memory_reader(const_memory_block(prvt, e[0].data)));
					cff::dictionary_with_defaults	prv_dict2(prv_dict, cff::dict_pvr_defaults());

					if (e = prv_dict2.lookup(cff::nominalWidthX))
						nominalWidth = e[0];

					if (e = prv_dict2.lookup(cff::nominalWidthX))
						defaultWidth = e[0];

					x->Append(ReadCFFDict("prvt", prv_dict, strings));

					if (e = prv_dict.lookup(cff::Subrs)) {
						lsubrs = (cff::index*)((uint8*)prvt + e[0].data);
						x->Append(ReadCFFBlocks("local_subrs", lsubrs));
					}
				}

				cff::index	*chrstr	= (cff::index*)((uint8*)h + pub_dict.lookup(cff::CharStrings)[0].data);
				x->Append(ReadCFFBlocks("CharStrings", chrstr));
				x->Append(ReadCFFCharSet("charset",	(uint8*)h + pub_dict.lookup(cff::charset)[0].data, chrstr->count));

				ISO_ptr<ISO_openarray<ISO_ptr<iso_simpleglyph>>> curves("curves", chrstr->count);
				x->Append(curves);

				CFF_VM	vm(gsubrs, lsubrs, nominalWidth);
				for (int j = 0; j < chrstr->count; j++) {
					vm.Reset(defaultWidth);
					vm.Interpret((*chrstr)[j]);
					*(*curves)[j].Create() = vm.Verts1();
					//if (direction(*(*curves)[j]))
					//	reverse_curves(*(*curves)[j]);
					//else
					//	ISO_TRACEF("whoops ") << j << '\n';
				}

				p->Append(x);
				break;
			}
			
			case "OS/2"_u32:
				p->Append(ReadBlockT<ttf::OS2>("OS/2",file, offset, length));
				break;

			case "sbix"_u32:
				p->Append(ReadBlockT<ttf::sbix>("sbix",file, offset, length));
				break;

			case "maxp"_u32:
				p->Append(ReadBlockT<ttf::maxp>("maxp",file, offset, length));
				break;

			case "hhea"_u32:
				p->Append(ReadBlockT<ttf::hhea>("hhea",file, offset, length));
				break;

			case "SVG "_u32:
				p->Append(ReadBlockT<ttf::SVG>("SVG",file, offset, length));
				break;

			case "GSUB"_u32:
				p->Append(ReadBlockT<ttf::GSUB>("GSUB",file, offset, length));
				//dump((const ttf::GSUB*)p->back());
				break;

			default: {
				char		name[5];
				memcpy(name, &table.tag, 4);
				name[4] = 0;
				ISO_ptr<ISO_openarray<uint8>>	x(name, length);
				file.seek(offset);
				file.readbuff(*x, length);
				p->Append(x);
				break;
			}
		}

	}
	return p;
}

//-----------------------------------------------------------------------------
//	PS Type1 FileHandler
//-----------------------------------------------------------------------------

class TYPE1_VM : public PS_VM<int> {
	bool	dotsection;
	num2	left, size;

public:
	void	Interpret(uint8 *p, uint8 *e);
	void	Interpret(memory_block &m) { Interpret(m, (uint8*)m.end());	}
	void	Reset() {
		PS_VM::Reset();
		dotsection = false;
		left = zero;
		size = zero;
	}
};

void TYPE1_VM::Interpret(uint8 *p, uint8 *e) {
	num	v;
	while (p < e) {
		uint8	b0 = *p++;
		if (b0 < 0x20) {
			if (b0 == cff::escape)
				b0 = *p++ + 0x20;
			switch (b0) {
				// binary math ops
				case cff::op1_div:
					v = st.pop();
					st[0] /= v;
					break;

				// stack ops
				case cff::op1_pop:
					st.pop();
					break;

				// flow control
				case cff::op1_callsubr:
					break;
				case cff::op1_return:
					return;
				case cff::op1_endchar:
					return;

				// hint ops
				case cff::op1_hstem:	//|- y dy {dya dyb}*
					hstems.set(st.clear(), 1);
					break;
				case cff::op1_hstem3:	//|- y0 dy0 y1 dy1 y2 dy2
					hstems.set(st.clear(), 3);
					break;
				case cff::op1_vstem:	//|- x dx {dxa dxb}*
					vstems.set(st.clear(), 1);
					break;
				case cff::op1_vstem3:	//|- x0 dx0 x1 dx1 x2 dx2
					vstems.set(st.clear(), 3);
					break;

				case cff::op1_dotsection:
					dotsection = !dotsection;
					st.clear();
					break;

				// initialisation ops
				case cff::op1_sbw: {	//|- sbx sby wx wy
					num2	*s = (num2*)st.clear();
					left	= s[0];
					size	= s[1];
					pt		= left;
					break;
				}
				case cff::op1_hsbw: {	//|- sbx wx
					num		*s = st.clear();
					left = int32x2{s[0], 0};
					size = int32x2{s[1], 0};
					pt		= left;
					break;
				}
				case cff::op1_seac: {//|- asb adx ady bchar achar
					num		*s = st.clear();
					st.clear();
					break;
				}

				// path ops
				case cff::op1_rmoveto:
					pt += *(num2*)st.clear();
					MoveTo(pt);
					break;

				case cff::op1_hmoveto:
					pt.x += *st.clear();
					MoveTo(pt);
					break;

				case cff::op1_vmoveto:
					pt.y += *st.clear();
					MoveTo(pt);
					break;

				case cff::op1_rlineto:	//|- dx dy
					pt += *(num2*)st.clear();
					LineTo(pt);
					break;

				case cff::op1_hlineto:	//|- dx1
					pt.x += *st.clear();
					LineTo(pt);
					break;

				case cff::op1_vlineto:	//|- dy1
					pt.y += *st.clear();
					LineTo(pt);
					break;

				case cff::op_rrcurveto: {	//|- dx1 dy1 dx2 dy2 dx3 dy3
					num2	*v = (num2*)st.clear();
					num2	p1 = pt + v[0], p2 = p1 + v[1];
					pt = p2 + v[2];
					BezierTo(p1, p2, pt);
					break;
				}

				case cff::op1_hvcurveto: { //|- dx1 dx2 dy2 dy3
					num *s = st.clear();
					num2	p1 = pt;
					p1.x += s[0];
					num2	p2 = p1 + *(num2*)(s + 1);
					pt = p2; pt.y += s[3];
					BezierTo(p1, p2, pt);
					break;
				}

				case cff::op1_vhcurveto: { //|- dy1 dx2 dy2 dx3
					num *s = st.clear();
					num2	p1 = pt;
					p1.y += s[0];
					num2	p2 = p1 + *(num2*)(s + 1);
					pt = p2; pt.x += s[3];
					BezierTo(p1, p2, pt);
					break;
				}

				case cff::op1_closepath:
					st.clear();
					break;

				case cff::op1_setcurrentpoint:
					pt = *(num2*)st.clear();
					break;

				default:
					break;
			}

		} else if (b0 == cff::op1_longint) {
			st.push(*(packed<BE(num)>*)p);

		} else {
			st.push(  b0 < 247	?  int(b0) - 139						// 32  < b0 < 246:	bytes:1; range:�107..+107
					: b0 < 251	? (int(b0) - 247) * +256 + *p++ + 108	// 247 < b0 < 250:	bytes:2; range:+108..+1131
								: (int(b0) - 251) * -256 - *p++ - 108	// 251 < b0 < 254:	bytes:2; range:�1131..�108
			);
		}
	}
}

struct tokeniser {
	const char *p, *e;
	char	token[256], *t;

	tokeniser(const char *p, size_t len) : p(p), e(p + len), t(token) {}

	void	skip(size_t n) {
		p += n;
	}
	const char *next() {
		while (p < e) {
			char	c = *p++;
			switch (c) {
				case ' ': case '\t': case '\n': case '\r':
					if (t > token) {
						*t = 0;
						t = token;
						return token;
					}
					break;
				case '%':
					while (p < e && *++p != '\n');
					break;
				default:
					if (t < token + sizeof(token) - 1)
						*t++ = c;
					break;
			}
		}
		return 0;
	}
};

class Type1FileHandler : public FileHandler {
	const char*		GetExt() override { return "pfb"; }
	const char*		GetDescription() override { return "Adobe Type1 Font";	}
public:
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	p(id);
		postscript::encryption	enc;
		dynamic_array<malloc_block> sections;
		for (;;) {
			uint8	m = file.getc();
			uint8	s = file.getc();
			if (m != 0x80)
				return ISO_NULL;

			if (s == 3)
				break;

			uint32			len = file.get<uint32le>();
			malloc_block&	mb = sections.emplace_back(len);
			file.readbuff(mb, len);

			if (s == 2)
				enc.eexec_decrypt(mb, mb, len);

			ISO_ptr<ISO_openarray<uint8>> x(0);
			memcpy(x->Create(len), mb, len);
			p->Append(x);

			if (s == 2) {
				ISO_ptr<anything> x("subrs");
				ISO_ptr<ISO_openarray<ISO_ptr< ISO_openarray<curve_vertex>>>> curves("curves");
				p->Append(x);

				TYPE1_VM	vm;
				bool		charstrings = false;
				string		last_name;
				int			stack[8], sp = 0;
				const char *t;

				for (tokeniser tok(mb, len); t = tok.next(); ) {
					if (is_digit(t[0])) {
						from_string(t, stack[++sp & 7]);

					} else if (t[0] == '/') {
						if (str(t + 1) == "CharStrings") {
							x.Create("CharStrings");
							p->Append(x);
							p->Append(curves);
							charstrings = true;
						} else if (charstrings) {
							last_name = t + 1;
						}
					} else if (str(t) == "RD") {
						int	len = stack[sp & 7];
						ISO_ptr<ISO_openarray<uint8>> y(last_name);
						enc.charstring_decrypt(tok.p, y->Create(len - 4), len);
						x->Append(y);
						tok.skip(len);
						if (charstrings) {
							vm.Reset();
							vm.Interpret(*y, *y + len);
							*curves->Append().Create(last_name) = vm.Verts1();
							//memcpy(curves->Append().Create(last_name)->Create(uint32(vm.NumVerts())), vm.Verts(), vm.NumVerts() * sizeof(float2p));
						}
						last_name.clear();
					}
				}
			}
		}
		return p;

	}
} type1;

class Type1MetricsFileHandler : public FileHandler {
	const char*		GetExt() override { return "pfm"; }
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		FileInput file(filename(fn).set_ext("pfb"));
		if (file.exists())
			return type1.Read(id, file);
		return ISO_NULL;
	}
} type1_metrics;

//-----------------------------------------------------------------------------
//	TTC FileHandler
//-----------------------------------------------------------------------------

class TTCFileHandler : public FileHandler {
	const char*		GetExt() override { return "ttc"; }
	const char*		GetDescription() override { return "Truetype Collection";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} ttc;

ISO_ptr<void> ReadTTF(tag id, istream_ref file) {
	TTF		t(file);
	ISO_ptr<ISO_openarray<uint8>>	p(id);
	uint8	*data	= p->Create(uint32(t.CalcSize()), false);

	int		n		= t.tables.size();
	uint32	offset	= sizeof(ttf::SFNTHeader) + n * sizeof(ttf::TableRecord);
	for (auto &table : t.tables) {
		uint32				length = align(table.length, 4);

		file.seek(table.offset);
		file.readbuff(data + offset, length);

//		uint32 check = CalcTableChecksum((uint32*)(data + offset), length);
//		ISO_ASSERT(check == recs[i].checkSum);

		table.offset	= offset;
		offset			+= length;
	}

	memcpy(data, &t.sfnt, sizeof(t.sfnt));
	memcpy(data + sizeof(ttf::SFNTHeader), t.tables, n * sizeof(ttf::TableRecord));
	return p;
}

ISO_ptr<void>	TTCFileHandler::Read(tag id, istream_ref file) {
	ttf::TTCHeader	h	= file.get();
	if (h.tag != "ttcf"_u32)
		return ISO_NULL;

	int				n	= h.num_fonts;
	
	temp_array<uint32be>	offsets(file, n);
	ISO_ptr<anything>		p(id);
	p->Create(n);

	for (int i = 0; i < n; i++) {
		file.seek(offsets[i]);
		(*p)[i] = ReadTTF(to_string(i), file);
	}

	return p;
}

//-----------------------------------------------------------------------------
//	EOT FileHandler
//-----------------------------------------------------------------------------

class EOTFileHandler : public FileHandler {
	const char*		GetExt()			override { return "eot"; }
	const char*		GetDescription()	override { return "Embedded Opentype";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} eot;

ISO_ptr<void>	EOTFileHandler::Read(tag id, istream_ref file) {
	return ISO_NULL;
}

struct EOTname {
	char16be	*name;
	uint16		size;
	bool		write(ostream_ref file) const {
		return file.write(size)
			&& file.writebuff(name, size)
			&& file.write(uint16(0));
	}
};

void WriteEOT(uint8 *font_data, size_t font_size, ostream_ref file) {
	ttf::SFNTHeader		*sfnt	= (ttf::SFNTHeader*)font_data;
	ttf::TableRecord	*tables = (ttf::TableRecord*)(sfnt + 1);
	ttf::EOTHeader		eot;

	clear(eot);
	eot.font_size	= uint32(font_size);
	eot.version		= 0x00020001;
	eot.charset		= DEFAULT_CHARSET;
	eot.magic		= ttf::EOTHeader::MAGIC;

	enum {
		name_family		= 1,
		name_subfamily	= 2,
		name_fullname	= 4,
		name_version	= 5,
		name_max
	};
	EOTname				names[name_max];

	for (uint32 i = 0; i < sfnt->num_tables; i++) {
		uint8	*start = font_data + tables[i].offset;

		switch (tables[i].tag) {
			case "OS/2"_u32: {
				ttf::OS2	*OS2 = (ttf::OS2*)start;
				eot.panose = OS2->panose;
				eot.italic = OS2->selection & 1;
				eot.weight = OS2->weight_class;
				// FIXME: Should use OS2->type, but some TrueType fonts set it to an over-restrictive value.
				// Since ATS does not enforce this on Mac OS X, we do not enforce it either.
				eot.type = 0;
				for (uint32 j = 0; j < 4; j++)
					eot.unicode_range[j] = OS2->unicode_range[j];
				for (uint32 j = 0; j < 2; j++)
					eot.codepage_range[j] = OS2->codepage_range[j];
				break;
			}
			case "head"_u32: {
				ttf::head* head = (ttf::head*)start;
				eot.checksum_adjustment = head->checksum_adjustment;
				break;
			}
			case "name"_u32: {
				ttf::name* name = (ttf::name*)start;
				for (int j = 0; j < name->count; j++) {
					if (name->names[j].platformID == ttf::PLAT_microsoft && name->names[j].encodingID == ttf::name::MS_UCS2 && name->names[j].languageID == 0x0409) {
						int		id = name->names[j].nameID;
						if (id < name_max) {
							names[id].name = (char16be*)(start + name->stringOffset + name->names[j].offset);
							names[id].size = name->names[j].length;
						}
					}
				}
				break;
			}
			default:
				break;
		}
	}

	file.seek(sizeof(ttf::EOTHeader));
	file.write(names[name_family]);
	file.write(names[name_subfamily]);
	file.write(names[name_version]);
	file.write(names[name_fullname]);

	file.write(uint16(0));
	eot.eot_size = uint32(file.tell() + font_size);

	file.writebuff(font_data, font_size);

	file.seek(0);
	file.write(eot);
}

//-----------------------------------------------------------------------------
//	Disassemblers
//-----------------------------------------------------------------------------

#ifdef ISO_EDITOR

#include "disassembler.h"

class DisassemblerPostscript : public Disassembler {
protected:
	void		_Disassemble(const_memory_block block, uint64 addr, dynamic_array<string> &lines, SymbolFinder sym_finder, const char **ops, size_t nops);
};

void DisassemblerPostscript::_Disassemble(const_memory_block block, uint64 addr, dynamic_array<string> &lines, SymbolFinder sym_finder, const char **ops, size_t nops) {
	byte_reader	r(block);
	dynamic_array<cff::value>	stack;

	//bias	= 107 or 1131 or 32768;


	while (r.p < block.end()) {
		const uint8	*start	= r.p;

		buffer_accum<1024>	ba("%08x ", r.p - start);
		cff::value_op	v = r.get();

		const uint8 *p = start;
		while (p < r.p)
			ba.format("%02x ", *p++);
		while (p < start + 8) {
			ba << "   ";
			p++;
		}
		switch (v.type) {
			case cff::t_int:
				stack.push_back(v);
				ba << (int&)v.data;
				break;

			case cff::t_float:
				stack.push_back(v);
				ba << (float)(fixed<16,16>&)v.data;
				break;

			case cff::t_prop:
				if (v.data > nops || ops[v.data] == 0) {
					ba << "--unknown--";
					break;
				}
				ba << ops[v.data];
				if (!stack.empty()) {
					switch (v.data) {
						case cff::op_callsubr:
						case cff::op_callgsubr:	ba << " " << (int)stack.back() + 107; break;
					}
					stack.clear();
				}
				break;
		}

		lines.push_back(ba);
	}
}

class DisassemblerCFF : public DisassemblerPostscript {
	static const char *ops[];
public:
	const char*	GetDescription() override { return "Adobe Compact Font"; }
	State*		Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) override;
} disassembler_cff;

const char *DisassemblerCFF::ops[] = {
	0,				"hstem",		0,				"vstem",
	"vmoveto",		"rlineto",		"hlineto",		"vlineto",
	"rrcurveto",	0,				"callsubr",		"return",
	0,				0,				"endchar",		0,
	0,				0,				"hstemhm",		"hintmask",
	"cntrmask",		"rmoveto",		"hmoveto",		"vstemhm",
	"rcurveline",	"rlinecurve",	"vvcurveto",	"hhcurveto",
	"shortint",		"callgsubr",	"vhcurveto",	"hvcurveto",
	"dotsection",	0,				0,				"and",
	"or",			"not",			0,				0,
	0,				"abs",			"add",			"sub",
	"div",			0,				"neg",			"eq",
	0,				0,				"drop",			0,
	"put",			"get",			"ifelse",		"random",
	"mul",			0,				"sqrt",			"dup",
	"exch",			"index",		"roll",			0,
	0,				0,				"hflex",		"flex",
	"hflex1",		"flex1",
};
Disassembler::State *DisassemblerCFF::Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;
	_Disassemble(block, addr, state->lines, sym_finder, ops, num_elements(ops));
	return state;
}


class DisassemblerType1 : public DisassemblerPostscript {
	static const char *ops[];
public:
	const char*	GetDescription() override { return "Adobe Type1 Font"; }
	State*		Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) override;
} disassembler_type1;

const char *DisassemblerType1::ops[] = {
	0,				"hstem",		0,				"vstem",
	"vmoveto",		"rlineto",		"hlineto",		"vlineto",
	"rrcurveto",	"closepath",	"callsubr",		"return",
	0,				"hsbw",			"endchar",		0,
	0,				0,				0,				0,
	0,				"rmoveto",		"hmoveto",		0,
	0,				0,				0,				0,
	0,				0,				"vhcurveto",	"hvcurveto",
	"dotsection",	"vstem3",		"hstem3",		0,
	0,				0,				"seac",			"sbw",
	0,				0,				0,				0,
	"div",			0,				"neg",			"eq",
	"callothersubr","pop",			0,				0,
	0,				0,				0,				0,
	0,				0,				0,				0,
	0,				0,				0,				0,
	0,				"setcurrentpoint",
};

Disassembler::State *DisassemblerType1::Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;
	_Disassemble(block, addr, state->lines, sym_finder, ops, num_elements(ops));
	return state;
}

#endif	//ISO_EDITOR