#include "iso/iso_files.h"
#include "base/vector.h"
#include "vector_iso.h"
#include "base/algorithm.h"
#include "ttf.h"
#include "cff.h"
#include "utilities.h"
#include "maths/geometry_iso.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Postscript VM
//-----------------------------------------------------------------------------

template<typename T, int N> struct stack {
	T	array[N];
	T	*sp;

	size_t	count()	const		{ return sp - array;}
	T*		clear()				{ return sp = array;}
	T		pop()				{ /*ISO_ASSERT(sp > array); */return sp > array ? *--sp : T(0); }
	void	push(T t)			{ ISO_ASSERT(sp < array + N); *sp++ = t; }
	T&		top(int i = 0)		{ return sp[~i];	}
	T&		bot(int i = 0)		{ return array[i];	}
	T&		operator[](int i)	{ return sp[~i];	}

	void	roll(int n, int j) {
		T	*p = sp + ~n;
		if (j < 0) {
			for (int i = 0; i < j; ++i)
				sp[i] = p[i];
			for (int i = j; i < n; ++i)
				p[i + j] = p[i];
			for (int i = 0; i < j; ++i)
				sp[i - j] = sp[i];
		} else {
			for (int i = n; --i;)
				p[i + j] = p[i];
			for (int i = 0; i < j; ++i)
				p[i] = sp[i];
		}
	}

	stack() : sp(array)	{}
};

template<typename N> class PS_VM {
protected:
	typedef	N				num;
	typedef	array_vec<N, 2>	num2;

	stack<num, 48>	st;
	num2			pt;

	dynamic_array<float2p>	verts;

	typedef pair<num, num> stem;
	struct stems : dynamic_array<num2> {
		void	set(num *s, size_t n)	{
			resize(n);
			num	x	= 0;
			for (auto &i : *this) {
				i.x = x += *s++;
				i.y = x += *s++;
			}
		}
	} hstems, vstems;

	void	MoveTo(const num2 &p) {	verts.push_back(to<float>(p));	}
	void	LineTo(const num2 &p) {	verts.push_back(to<float>(p));	}
	void	BezierTo(const num2 &p1, const num2 &p2, const num2 &p3) {
		verts.push_back(to<float>(p1));
		verts.push_back(to<float>(p2));
		verts.push_back(to<float>(p3));
	}

public:
	void		Reset() {
		st.clear();
		pt = zero;
		hstems.clear();
		vstems.clear();
		verts.clear();
	}

	size_t		NumVerts()	const { return verts.size(); }
	float2p*	Verts()		const { return verts; }
};

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

struct TTF {
	ttf::SFNTHeader		sfnt;
	dynamic_array<ttf::TableRecord>	tables;
	unique_ptr<ttf::head>	head;

	ttf::SFNTHeader*	GetSFNT() {
		return &sfnt;
	}
	uint32				NumTables()	const {
		return sfnt.num_tables;
	}
	const ttf::TableRecord*	FindTable(ttf::TAG tag) const {
		for (auto &i : tables) {
			if (i.tag == tag)
				return &i;
		}
		return nullptr;
	}
	template<typename T> ttf::TableRecord*	FindTable() const {
		return FindTable(T::tag);
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
		sfnt	= file.get();
		tables.read(file, sfnt.num_tables);
		head	= ReadTable<ttf::head>(file);
	}
	~TTF() {
	}
};

ISO_ptr<void> ReadCMAP(tag id, const ttf::cmap *cmap) {
	ISO_ptr<anything>	p(id);
	for (int i = 0, n = cmap->num; i < n; i++) {
		ttf::cmap::format_header1	*table	= (ttf::cmap::format_header1*)((uint8*)cmap + cmap->tables[i].offset);
		switch (table->format) {
			case 0: {
				ttf::cmap::format0	*table0 = (ttf::cmap::format0*)table;
				ISO_ptr<ISO_openarray<array<uint16,2> > >	t(0);
				for (int c = 0; c < 256; c++) {
					if (uint16 g = table0->glyphIndexArray[c])
						t->Append(make_array<uint16>(c, g));
				}
				p->Append(t);
				break;
			}
			case 4: {
				ttf::cmap::format4	*table4 = (ttf::cmap::format4*)table;
				uint32		segcount = table4->segCountX2 / 2;
				uint16be	*ends	= table4->endCode;
				uint16be	*starts	= ends		+ segcount + 1;
				uint16be	*deltas	= starts	+ segcount;
				uint16be	*ranges	= deltas	+ segcount;
				uint16be	*glyphs	= ranges	+ segcount;

				ISO_ptr<ISO_openarray<array<uint16,2> > >	t(0);
				for (int i = 0, n = segcount; i < n; i++) {
					uint16	start = starts[i], end = ends[i];
					if (uint16 r = ranges[i]) {
						for (int c = start; c <= end; c++)
							t->Append(make_array<uint16>(c, ranges[i + r / 2 + c - start]));
					} else {
						uint16	d = deltas[i];
						for (int c = start; c <= end; c++)
							t->Append(make_array<uint16>(c, c + d));
					}
				}

				p->Append(t);
				break;
			}
			default: {
				ISO_ptr<ISO_openarray<uint8> >	t(0);
				uint32	length	= table->format <= 6 ? (uint32)table->length : (uint32)((ttf::cmap::format_header2*)table)->length;
				memcpy(t->Create(length), table, length);
				p->Append(t);
				break;
			}
		}
	}
	return p;
}

typedef ISO_openarray<curve_vertex>						iso_simpleglyph;
typedef pair<ISO_ptr<iso_simpleglyph>, float2x3p>		iso_glyphref;
typedef ISO_openarray<iso_glyphref>						iso_compoundglyph;

template<typename R> int16 get_glyph_delta(R &r, bool SHORT, bool SAMEPOS) {
	return SHORT
		? int16(r.template get<uint8>()) * (SAMEPOS ? 1 : -1)
		: SAMEPOS ? 0 : int16(r.template get<int16be>());
}

ISO_ptr<void> ReadGlyph(const memory_block &mb, const anything &glyphs) {
	ttf::glyf::glyph *g = mb;
	if (!g)
		return ISO_NULL;

	int	ncnt = g->num_contours;
	if (ncnt >= 0) {
		// simple
		auto	gs = (ttf::glyf::simple_glyph*)g;

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
			p.flags	= *f & ttf::glyf::simple_glyph::on_curve ? 1 : 2;
			p.x		= x += get_glyph_delta(r, *f & ttf::glyf::simple_glyph::short_x, *f & ttf::glyf::simple_glyph::same_x);
			++f;
		}

		f	= flags.begin();
		x	= 0;
		for (auto &p : *curve) {
			p.y		 = x += get_glyph_delta(r, *f & ttf::glyf::simple_glyph::short_y, *f & ttf::glyf::simple_glyph::same_y);
			++f;
		}

		for (int i = 0, j = 0; i < ncnt; i++) {
			(*curve)[j].flags = 0;
			j = gs->end_pts[i] + 1;
		}

		return curve;
#endif

	} else {
		// compound
		auto	gc		= (ttf::glyf::compound_glyph*)g;
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
				xoff = r.get<fixed<2,6> >();
				yoff = r.get<fixed<2,6> >();
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
//-----------------------------------------------------------------------------

class CFF_VM : public PS_VM<fixed<16,16> > {
	typedef PS_VM<fixed<16,16> > B;
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
				case cff::op_hintmask:
					stclear(0);
					hintmask = p;
					p += (hstems.size() + vstems.size() + 7) / 8;
					break;

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
ISO_ptr<void> CFFDict(tag id, const cff::dictionary &dict, cff::index *strings) {
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
					(*x)[i] = CFFDictValue(0, vals[i]);
				d = x;
			} else {
				if (type == cff::t_int) {
					ISO_ptr<ISO_openarray<int> >	x(id, num);
					for (int i = 0; i < num; i++)
						(*x)[i] = (int&)vals[i].data;
					d = x;
				} else {
					ISO_ptr<ISO_openarray<float> >	x(id, num);
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
				if (vals[0].data >= cff::nStdStrings) {
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
	ISO_ptr<ISO_openarray<string> >	p(id, ind->count);
	for (int i = 0, n = ind->count; i < n; i++) {
		const_memory_block	m = (*ind)[i];
		(*p)[i] = str((const char*)m, m.length());
	}
	return p;
}

ISO_ptr<void> ReadCFFBlocks(tag id, cff::index *ind) {
	ISO_ptr<ISO_openarray<ISO_openarray<uint8> > >	p(id, ind->count);
	for (int i = 0, n = ind->count; i < n; i++) {
		const_memory_block	m = (*ind)[i];
		memcpy((*p)[i].Create(m.size32()), m, m.length());
	}
	return p;
}

ISO_ptr<void> ReadCFFCharSet(tag id, uint8 *p, int nglyphs) {
	ISO_ptr<ISO_openarray<pair<uint16,uint16> >	> r(id);
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
//	TTF & OTF FileHandlers
//-----------------------------------------------------------------------------

class TTFFileHandler : public FileHandler {
	const char*		GetExt() override { return "ttf"; }
	const char*		GetDescription() override { return "Truetype font";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	int				Check(istream_ref file) override {
		file.seek(0);
		uint32	t = file.get<uint32be>();
		return t == 0x00010000 || t == 'true' || t == 'typ1' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
} ttf_fh;

class OTFFileHandler : public TTFFileHandler {
	const char*		GetExt() override { return "otf"; }
	const char*		GetDescription() override { return "Opentype font";	}
	int				Check(istream_ref file) override {
		return 0;
		file.seek(0);
		return file.get<uint32be>() == 'OTTO' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
} otf_fh;

ISO_ptr<void>	TTFFileHandler::Read(tag id, istream_ref file) {
	TTF	t(file);

	ISO_ptr<anything>	p(id);

	for (auto &table : t.tables) {
		uint32	length	= table.length;
		uint32	offset	= table.offset;
		switch (table.tag) {
			case 'loca':
			case 'head':
				break;

			case 'glyf': {
				auto	mb = ReadTable(file, t.FindTable('loca'));
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
					i = ReadGlyph(ReadBlock(file, table.offset + loc[0], loc[1] - loc[0]), *x);
					++loc;
				}

				p->Append(x);
				break;
			}
			case 'cmap':
				p->Append(ReadCMAP("cmap", ReadBlock(file, offset, length)));
				break;

			case 'cvt ': {
				file.seek(offset);
				uint32	n = length / sizeof(ttf::fword);
				ISO_ptr<ISO_openarray<int16> >	x("cvt", n);
				for (int16 *d = *x; n--; d++)
					*d = file.get<ttf::fword>();
				p->Append(x);
				break;
			}
			case 'name':
				p->Append(ReadNAME("names", ReadBlock(file, offset, length)));
				break;

			case 'CFF ': {
				ISO_ptr<anything>	x("CFF");
				cff::header	*h			= ReadBlock(file, offset, length);
				cff::index	*names		= (cff::index*)((uint8*)h + h->hdrSize);
				cff::index	*dict		= (cff::index*)names->end();
				cff::index	*strings	= (cff::index*)dict->end();
				cff::index	*gsubrs		= (cff::index*)strings->end();
				cff::index	*lsubrs		= 0;
				void		*prvt		= 0;
				float		nominalWidth = 0, defaultWidth = 0;

				cff::dictionary	pub_dict(dict);
				cff::dictionary	prv_dict;

				x->Append(ReadCFFStrings("names", names));
				x->Append(CFFDict("dict", pub_dict, strings));
				x->Append(ReadCFFStrings("strings", strings));
				x->Append(ReadCFFBlocks("global_subrs", gsubrs));

				auto e = pub_dict.lookup(cff::Private);
				if (!e.empty()) {
					prvt	= (uint8*)h + e[0].data;
					prv_dict.add(lvalue(memory_reader(const_memory_block(prvt, e[0].data))));

					cff::dictionary_with_defaults	prv_dict2(prv_dict, cff::dict_pvr_defaults());
					e = prv_dict2.lookup(cff::nominalWidthX);
					if (!e.empty())
						nominalWidth = e[0];
					e = prv_dict2.lookup(cff::nominalWidthX);
					if (!e.empty())
						defaultWidth = e[0];

					x->Append(CFFDict("prvt", prv_dict, strings));

					e = prv_dict.lookup(cff::Subrs);
					if (!e.empty()) {
						lsubrs = (cff::index*)((uint8*)prvt + e[0].data);
						x->Append(ReadCFFBlocks("local_subrs", lsubrs));
					}
				}

				cff::index	*chrstr	= (cff::index*)((uint8*)h + pub_dict.lookup(cff::CharStrings)[0].data);
				x->Append(ReadCFFBlocks("CharStrings", chrstr));
				x->Append(ReadCFFCharSet("charset",	(uint8*)h + pub_dict.lookup(cff::charset)[0].data, chrstr->count));

				ISO_ptr<ISO_openarray<ISO_ptr< ISO_openarray<float2p> > > > curves("curves", chrstr->count);
				x->Append(curves);

				CFF_VM	vm(gsubrs, lsubrs, nominalWidth);
				for (int i = 0; i < chrstr->count; i++) {
					vm.Reset(defaultWidth);
					vm.Interpret((*chrstr)[i]);
					memcpy((*curves)[i].Create()->Create(uint32(vm.NumVerts())), vm.Verts(), vm.NumVerts() * sizeof(float2p));
				}

				p->Append(x);
				break;
			}

			default: {
				char		name[5];
				memcpy(name, &table.tag, 4);
				name[4] = 0;
				ISO_ptr<ISO_openarray<uint8> >	x(name, length);
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

	tokeniser(const char *_p, size_t len) : p(_p), e(_p + len), t(token) {}

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

			ISO_ptr<ISO_openarray<uint8> > x(0);
			memcpy(x->Create(len), mb, len);
			p->Append(x);

			if (s == 2) {
				ISO_ptr<anything> x("subrs");
				ISO_ptr<ISO_openarray<ISO_ptr< ISO_openarray<float2p> > > > curves("curves");
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
						ISO_ptr<ISO_openarray<uint8> > y(last_name);
						enc.charstring_decrypt(tok.p, y->Create(len - 4), len);
						x->Append(y);
						tok.skip(len);
						if (charstrings) {
							vm.Reset();
							vm.Interpret(*y, *y + len);
							memcpy(curves->Append().Create(last_name)->Create(uint32(vm.NumVerts())), vm.Verts(), vm.NumVerts() * sizeof(float2p));
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
	ISO_ptr<ISO_openarray<uint8> >	p(id);
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

	memcpy(data, t.GetSFNT(), sizeof(ttf::SFNTHeader));
	memcpy(data + sizeof(ttf::SFNTHeader), t.tables, n * sizeof(ttf::TableRecord));
	return p;
}

ISO_ptr<void>	TTCFileHandler::Read(tag id, istream_ref file) {
	ttf::TTCHeader	h	= file.get();
	if (h.tag != 'ttcf')
		return ISO_NULL;

	int				n		= h.num_fonts;
	temp_array<ttf::uint32>	offsets(n);
//	ttf::uint32*	offsets = new ttf::uint32[n];
	file.readbuff(offsets, n * sizeof(ttf::uint32));

	ISO_ptr<anything>	p(id);
	p->Create(n);

	for (int i = 0; i < n; i++) {
		file.seek(offsets[i]);
		(*p)[i] = ReadTTF(to_string(i), file);
	}

//	delete[] offsets;
	return p;
}

//-----------------------------------------------------------------------------
//	EOT FileHandler
//-----------------------------------------------------------------------------

class EOTFileHandler : public FileHandler {
	const char*		GetExt() override { return "eot"; }
	const char*		GetDescription() override { return "Embedded Opentype";	}
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
			case 'OS/2': {
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
			case 'head': {
				ttf::head* head = (ttf::head*)start;
				eot.checksum_adjustment = head->checksum_adjustment;
				break;
			}
			case 'name': {
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
	void		_Disassemble(const memory_block &block, uint64 addr, dynamic_array<string> &lines, SymbolFinder sym_finder, const char **ops, size_t nops);
};

void DisassemblerPostscript::_Disassemble(const memory_block &block, uint64 addr, dynamic_array<string> &lines, SymbolFinder sym_finder, const char **ops, size_t nops) {
	byte_reader	r(block);
	while (r.p < (uint8*)block.end()) {
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
			case cff::t_int:	ba << (int&)v.data; break;
			case cff::t_float:	ba << (fixed<16,16>&)v.data; break;
			case cff::t_prop:	ba << (v.data > nops || ops[v.data] == 0 ? "--unknown--"  : ops[v.data]); break;
		}

		lines.push_back((const char*)ba);
	}
}

class DisassemblerCFF : public DisassemblerPostscript {
	static const char *ops[];
public:
	const char*	GetDescription() override { return "Adobe Compact Font"; }
	State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) override;
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
Disassembler::State *DisassemblerCFF::Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;
	_Disassemble(block, addr, state->lines, sym_finder, ops, num_elements(ops));
	return state;
}


class DisassemblerType1 : public DisassemblerPostscript {
	static const char *ops[];
public:
	const char*	GetDescription() override { return "Adobe Type1 Font"; }
	State*		Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) override;
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

Disassembler::State *DisassemblerType1::Disassemble(const iso::memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state = new StateDefault;
	_Disassemble(block, addr, state->lines, sym_finder, ops, num_elements(ops));
	return state;
}

#endif	//ISO_EDITOR