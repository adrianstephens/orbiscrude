#ifndef VIEWER_IDENTIFIER_H
#define VIEWER_IDENTIFIER_H

//-----------------------------------------------------------------------------
//	glue between c-types, identifier and controls
//-----------------------------------------------------------------------------

#include "extra/c-types.h"
#include "extra/identifier.h"
#include "base/vector.h"
#include "windows/control_helpers.h"

namespace iso {

//-----------------------------------------------------------------------------
//	SoftType
//-----------------------------------------------------------------------------

struct SoftType {
	struct vtable {
		size_t			(*Size)(void*);
		int				(*Count)(void*);
		string_accum&	(*Type)(void*, string_accum &sa, int i, const char *name);
		string_accum&	(*Name)(void*, string_accum &sa, int i);
		string_accum&	(*Get)(void*, string_accum &sa, const void *data, int i, FORMAT::FLAGS flags);
		string_scan&	(*Set)(void*, string_scan &ss, void *data, int i);
		float			(*GetFloat)(void*, const void *data, int i);
	};
	template<typename T> struct vtable_s { static vtable v; };
	void	*t;
	vtable	*v;
public:
	size_t			Size()																				const { return v->Size(t); }
	int				Count()																				const { return v->Count(t); }
	string_accum&	Type(string_accum &sa, int i, string_param &&name)									const { return v->Type(t, sa, i, name); }
	string_accum&	Name(string_accum &sa, int i)														const { return v->Name(t, sa, i); }
	string_accum&	Get(string_accum &sa, const void *data, int i, FORMAT::FLAGS flags = FORMAT::NONE)	const { return v->Get(t, sa, data, i, flags); }
	string_scan&	Set(string_scan &ss, void *data, int i)												const { return v->Set(t, ss, data, i); }
	float			GetFloat(const void *data, int i)													const { return v->GetFloat(t, data, i); }
	SoftType() : t(0), v(0) {}
	template<typename T> SoftType(T *t) : t((void*)t), v(&vtable_s<T>::v) {}
	bool operator!() const	{ return !t; }
};

template<typename T> SoftType::vtable SoftType::vtable_s<T>::v = {
	[](void *t)																	->size_t		{ return ((T*)t)->Size(); },
	[](void *t)																	->int			{ return ((T*)t)->Count(); },
	[](void *t, string_accum &sa, int i, const char *name)						->string_accum&	{ return ((T*)t)->Type(sa, i, name); },
	[](void *t, string_accum &sa, int i)										->string_accum&	{ return ((T*)t)->Name(sa, i); },
	[](void *t, string_accum &sa, const void *data, int i, FORMAT::FLAGS flags)	->string_accum&	{ return ((T*)t)->Get(sa, data, i, flags); },
	[](void *t, string_scan &ss, void *data, int i)								->string_scan&	{ return ((T*)t)->Set(ss, data, i); },
	[](void *t, const void *data, int i)										->float			{ return ((T*)t)->GetFloat(data, i); }
};

//template<> extern SoftType::vtable SoftType::vtable_s<const C_type>::v;
inline string_accum&	Type(const C_type *t, string_accum &sa, int i, const char *name)						{ int shift; void *data = 0; return DumpType(sa, GetNth(data, t, i, shift), name, 0); }
inline string_accum&	Name(const C_type *t, string_accum &sa, int i)											{ return GetNthName(sa, t, i); }
inline string_accum&	Get(const C_type *t, string_accum &sa, const void *data, int i, FORMAT::FLAGS flags)	{ int shift; auto subtype = GetNth(data, t, i, shift); return DumpData(sa, data, subtype, shift, 0, flags); }
inline string_scan&		Set(const C_type *t, string_scan &ss, void *data, int i)								{ int shift; auto subtype = GetNth(data, t, i, shift); return SetData(ss, data, subtype, shift); }
inline float			GetFloat(const C_type *t, const void *data, int i)										{ int shift; auto subtype = GetNth(data, t, i, shift); return GetFloat(data, subtype, shift); }


template<> SoftType::vtable SoftType::vtable_s<const C_type>::v = {
	[](void *t)																	->size_t		{ return ((const C_type*)t)->size(); },
	[](void *t)																	->int			{ return NumElements((const C_type*)t); },
	[](void *t, string_accum &sa, int i, const char *name)						->string_accum&	{ return iso::Type((const C_type*)t, sa, i, name); },
	[](void *t, string_accum &sa, int i)										->string_accum&	{ return iso::Name((const C_type*)t, sa, i); },
	[](void *t, string_accum &sa, const void *data, int i, FORMAT::FLAGS flags)	->string_accum&	{ return iso::Get((const C_type*)t, sa, data, i, flags); },
	[](void *t, string_scan &ss, void *data, int i)								->string_scan&	{ return iso::Set((const C_type*)t, ss, data, i); },
	[](void *t, const void *data, int i)										->float			{ return iso::GetFloat((const C_type*)t, data, i); }
};

template<typename T> void assign(T &f, const param_element<const uint8&, const SoftType&> &a)	{
	typedef element_type<T>	E;
	f = to<E>(
		a.p.GetFloat(&a.t, 0),
		a.p.GetFloat(&a.t, 1),
		a.p.GetFloat(&a.t, 2),
		a.p.GetFloat(&a.t, 3)
	);
}

//-----------------------------------------------------------------------------
//	C_types
//-----------------------------------------------------------------------------

extern		C_types ctypes, user_ctypes;
C_types&	builtin_ctypes();

#if 0
template<int N, typename S> struct C_types::type_getter<_soft_vector<N, S> > : type_getter<S> {};

template<typename T, typename XT, int XB, typename YT, int YB> struct C_types::type_getter<_packed2<T, XT, XB, YT, YB> > {
	static const C_type *f(C_types &ctypes)	{
		C_type_struct	s;
		s.set_packed(true);
		s.add_atbit("x", ctypes.get_type<XT>(), 0);
		s.add_atbit("y", ctypes.get_type<YT>(), XB);
		return ctypes.add(move(s));
	}
};

template<typename T, typename XT, int XB, typename YT, int YB, typename ZT, int ZB> struct C_types::type_getter<_packed3<T, XT, XB, YT, YB, ZT, ZB> > {
	static const C_type *f(C_types &ctypes)	{
		C_type_struct	s;
		s.set_packed(true);
		s.add_atbit("x", ctypes.get_type<XT>(), 0);
		s.add_atbit("y", ctypes.get_type<YT>(), XB);
		s.add_atbit("z", ctypes.get_type<ZT>(), XB+YB);
		return ctypes.add(move(s));
	}
};

template<typename T, typename XT, int XB, typename YT, int YB, typename ZT, int ZB, typename WT, int WB> struct C_types::type_getter<_packed4<T, XT, XB, YT, YB, ZT, ZB, WT, WB> > {
	static const C_type *f(C_types &ctypes)	{
		C_type_struct	s;
		s.set_packed(true);
		s.add_atbit("x", ctypes.get_type<XT>(), 0);
		s.add_atbit("y", ctypes.get_type<YT>(), XB);
		s.add_atbit("z", ctypes.get_type<ZT>(), XB+YB);
		s.add_atbit("w", ctypes.get_type<WT>(), XB+YB+ZB);
		return ctypes.add(move(s));
	}
};

template<typename I, int B, typename T_signed_native<I, false>::type S> struct C_types::type_getter<scaled_field<I, B, S> > {
	static const C_type *f(C_types &types)	{
		return C_type_int::get<B, num_traits<I>::is_signed ? C_type_int::SIGN | C_type_int::NORM  : C_type_int::NORM>();
	}
};
template<uint32 M, uint32 E, bool S> struct C_types::type_getter<soft_float<M, E, S> > {
	static const C_type *f(C_types &types)	{
		return C_type_float::get<M + E + int(S), E, S>();
	}
};
template<typename I, typename T_signed_native<I, false>::type S> struct C_types::type_getter<scaled<I, S> > : C_types::type_getter<I> {
	static const C_type *f(C_types &types)	{
		return C_type_int::get<sizeof(I) * 8, num_traits<I>::is_signed ? C_type_int::SIGN | C_type_int::NORM  : C_type_int::NORM>();
	}
};
#else

template<uint32 M, uint32 E, bool S> struct C_types::type_getter<soft_float_imp<M, E, S> > {
	static const C_type *f(C_types &types)	{
		return C_type_float::get<M + E + int(S), E, S>();
	}
};

template<typename I, typename T_signed_native<I, false>::type S> struct C_types::type_getter<scaled<I, S> > : C_types::type_getter<I> {
	static const C_type *f(C_types &types)	{
		return C_type_int::get<sizeof(I) * 8, num_traits<I>::is_signed ? C_type_int::SIGN | C_type_int::NORM  : C_type_int::NORM>();
	}
};

template<typename T, int N> struct C_types::type_getter<compact<T, N> > : C_types::type_getter<T> {};

template<typename X, typename Y> struct C_types::type_getter<bitfield_vec<X, Y> > {
	static const C_type *f(C_types &ctypes)	{
		C_type_struct	s;
		s.set_packed(true);
		s.add_atbit("x", ctypes.get_type<X>(), 0);
		s.add_atbit("y", ctypes.get_type<Y>(), BIT_COUNT<X>);
		return ctypes.add(move(s));
	}
};
template<typename X, typename Y, typename Z> struct C_types::type_getter<bitfield_vec<X, Y, Z> > {
	static const C_type *f(C_types &ctypes)	{
		C_type_struct	s;
		s.set_packed(true);
		s.add_atbit("x", ctypes.get_type<X>(), 0);
		s.add_atbit("y", ctypes.get_type<Y>(), BIT_COUNT<X>);
		s.add_atbit("z", ctypes.get_type<Z>(), BIT_COUNT<X> + BIT_COUNT<Y>);
		return ctypes.add(move(s));
	}
};
template<typename X, typename Y, typename Z, typename W> struct C_types::type_getter<bitfield_vec<X, Y, Z, W> > {
	static const C_type *f(C_types &ctypes)	{
		C_type_struct	s;
		s.set_packed(true);
		s.add_atbit("x", ctypes.get_type<X>(), 0);
		s.add_atbit("y", ctypes.get_type<Y>(), BIT_COUNT<X>);
		s.add_atbit("z", ctypes.get_type<Z>(), BIT_COUNT<X> + BIT_COUNT<Y>);
		s.add_atbit("w", ctypes.get_type<Z>(), BIT_COUNT<X> + BIT_COUNT<Y> + BIT_COUNT<W>);
		return ctypes.add(move(s));
	}
};

#endif
}

namespace app {
using namespace iso;
using namespace win;

//-----------------------------------------------------------------------------
//	ListViews
//-----------------------------------------------------------------------------

template<typename T> void FillRow(ListViewControl &c, ListViewControl::Item &item, const T &t, IDFMT format = IDFMT_LEAVE) {
	RegisterList(c, format).FillRow(item, fields<T>::f, (const uint32*)&t, 0);
}

int MakeHeaders(ListViewControl lv, int nc, const C_type *type, string_accum &prefix);
int MakeHeaders(ListViewControl lv, int nc, const SoftType &type, string_accum &prefix);

template<typename T> int MakeHeaders(ListViewControl lv, const C_types &ctypes, int nc, string_accum &prefix) {
	return MakeHeaders(lv, nc, ctypes.get_type<T>(), prefix);
}

template<typename R, typename E> int FillColumn(ListViewControl &c, int col, const R &rf, uint8 mask, const E &enabled) {
	int	t = 0;
	for (auto &r : rf) {
		char				text[64] = "-";
		ListViewControl2::Item	item(text);
		item.Index(t).Column(col - 1);

		bool	en	= enabled[t];
		float	*o = (float*)&r;

		for (int m = mask; m; m = clear_lowest(m)) {
			int	i = lowest_set_index(m);
			if (en)
				fixed_accum(text) << o[i];
			item.NextColumn().Set(c);
		}
		t++;
	}
	return col + count_bits(mask);
}

template<typename T, typename U> void FillTable(ListViewControl &c, dynamic_array<U> &table) {
	for (auto &i : table) {
		ListViewControl::Item	item;
		item.Text(i.name).Param(&i).Insert(c);
		item.Column(2).Text(to_string(GetSize((T&)i))).Set(c);
		FillRow(c, item.Column(3), fields<T>::f, (uint32*)&i);
	}
}

struct FieldSorter : ColumnSorter {
	const field	f;
	int	operator()(uint32 *a, uint32 *b) const { return compare(f.get_value(a), f.get_value(b)); }
	FieldSorter(const field &f, int dir) : ColumnSorter(dir), f(f) {}
};

struct IndirectFieldSorter : ColumnSorter {
	const field*	pf;
	int				index;
	int	operator()(uint32 *a, uint32 *b) const {
		const uint32	*pa = a, *pb = b;
		uint32			offseta = 0, offsetb = 0;
		const field* fa = FieldIndex(pf, index, pa, offseta, true);
		const field* fb = FieldIndex(pf, index, pb, offsetb, true);
		return compare(fa ? fa->get_value(pa, offseta) : 0, fb ? fb->get_value(pb, offsetb) : 0);
	}
	IndirectFieldSorter(const field *pf, int index, int dir) : ColumnSorter(dir), pf(pf), index(index) {}
};

//-----------------------------------------------------------------------------
//	RegisterList
//-----------------------------------------------------------------------------

struct RegisterList {
	typedef callback<void(string_accum &sa, const field *pf, const uint32le *p, uint32 offset)>	cb_type;
	ListViewControl	lv;
	IDFMT			format;
	cb_type			cb;

	void	Callback(string_accum &sa, const field *pf, const uint32le *p, uint32 offset) {
		if (cb)
			cb(sa, pf, p, offset);
		else
			PutField(sa << "<custom>", format, pf, p, offset, 0);
	}

	RegisterList(ListViewControl lv, IDFMT format = IDFMT_LEAVE) : lv(lv), format(format) {}
	RegisterList(ListViewControl lv, const cb_type &cb, IDFMT format = IDFMT_LEAVE) : lv(lv), format(format), cb(cb) {}


	int				MakeColumns(const field *pf, int nc, const char *prefix = 0);
	void			AddText(ListViewControl::Item &item, const char *text) { item.Text(text).Set(lv); }
	string_accum&	FillSubItem(string_accum &b, const field *pf, const uint32 *p, uint32 offset);
	void			FillRow(ListViewControl::Item &item, const field *pf, const uint32 *p, uint32 offset = 0);
	void			FillRowArray(ListViewControl::Item &item, const field *pf, const uint32 *p, uint32 stride, uint32 n);
	template<typename T> void FillRow(ListViewControl::Item &item, const T &t) {
		FillRow(item, fields<T>::f, (const uint32*)&t, 0);
	}
};

inline int	MakeColumns(ListViewControl c, const field *pf, IDFMT format, int nc) {
	return RegisterList(c, format).MakeColumns(pf, nc);
}
template<typename T>  int	MakeColumns(ListViewControl c, IDFMT format, int nc) {
	return RegisterList(c, format).MakeColumns(fields<T>::f, nc);
}
inline void	FillRow(ListViewControl c, ListViewControl::Item &item, IDFMT format, const field *pf, const uint32 *p) {
	RegisterList(c, format).FillRow(item, pf, p, 0);
}
inline void	FillRow(ListViewControl c, const RegisterList::cb_type &cb, ListViewControl::Item &item, IDFMT format, const field *pf, const uint32 *p) {
	RegisterList(c, cb, format).FillRow(item, pf, p, 0);
}
template<typename T> void	FillRow(ListViewControl c, const RegisterList::cb_type &cb, ListViewControl::Item &item, IDFMT format, const T *p) {
	RegisterList(c, cb, format).FillRow(item, fields<T>::f, (const uint32*)p, 0);
}

//-----------------------------------------------------------------------------
//	RegisterTree
//-----------------------------------------------------------------------------

struct RegisterTree {
	typedef callback<void(RegisterTree &tree, HTREEITEM h, const field *pf, const uint32le *p, uint32 offset, uint32 addr)>	cb_type;
	TreeControl		tree;
	IDFMT			format;
	cb_type			cb;

	HTREEITEM	curr_h;

	void	Open(const char* title, uint32 addr) {
		curr_h = AddText(curr_h, title, addr);
	}
	void	Close() {
		curr_h = tree.GetParentItem(curr_h);

	}
	void	Line(const char* name, const char* value, uint32 addr) {
		if (name)
			AddText(curr_h, format_string("%s = %s", name, value), addr);
		else
			AddText(curr_h, value, addr);
	}
	void	Callback(const field *pf, const uint32le *p, uint32 offset, uint32 addr) {
		if (cb)
			cb(*this, curr_h, pf, p, offset, addr);
		else
			Line("<custom>", PutField(lvalue(buffer_accum<256>()), format, pf, p, offset, 0), addr + offset);

	}

	RegisterTree(TreeControl tree, IDFMT format = IDFMT_LEAVE) : tree(tree), format(format) {}
	RegisterTree(TreeControl tree, const cb_type &cb, IDFMT format = IDFMT_LEAVE) : tree(tree), format(format), cb(cb) {}

	HTREEITEM	AddFields(HTREEITEM h, const field* pf, const void* p, uint32 addr = 0) {
		curr_h = h;
		FieldPutter(this, format).AddFields(pf, (const uint32le*)p, addr);
		return h;
	}
	template<typename T> HTREEITEM	AddFields(HTREEITEM h, const T *p, uint32 addr = 0) {
		return AddFields(h, fields<T>::f, p, addr);
	}
	void		AddField(HTREEITEM h, const field& pf, const void* p, uint32 offset = 0, uint32 addr = 0) {
		curr_h = h;
		FieldPutter(this, format).AddField(&pf, (const uint32le*)p, addr, offset);
	}
	template<typename T> void	AddField(HTREEITEM h, const T *p) {
		AddField(h, *fields<T>::f, (uint32le*)p);
	}
	template<typename T> void	AddArray(HTREEITEM h, const T *p, uint32 n, uint32 addr = 0) {
		curr_h = h;
		FieldPutter(this, format).AddArray(fields<T>::f, (uint32le*)p, sizeof(T) / 4, n, addr);
	}
	template<typename T> void	AddArray(HTREEITEM h, const T &p) {
		curr_h = h;
		FieldPutter(this, format).AddArray(p);
	}

	HTREEITEM	Add(HTREEITEM h, const char *text, int image, const arbitrary &param, int state = 0) {
		return TreeControl::Item(text).Image(image).SetState(state).Param(param).Insert(tree, h);
	}

	HTREEITEM	AddText(HTREEITEM h, const char* text, uint32 addr = 0) {
		return TreeControl::Item(text).Param(addr).Insert(tree, h);
	}

	void		AddHex(HTREEITEM h, uint32 addr, const uint32le* vals, uint32 n) {
		curr_h = h;
		FieldPutter(this, format).AddHex(vals, n, addr);
	}
};

void StructureHierarchy(RegisterTree &c, HTREEITEM h, const C_types &types, const char *name, const C_type *type, uint32 offset, const void *data, const uint64 *valid = nullptr);

//-----------------------------------------------------------------------------
//	ColourList
//-----------------------------------------------------------------------------

win::Colour		FadeColour(win::Colour c);
win::Colour		MakeColour(int c);

struct ColourList {
	ListViewControl2		vw;
	dynamic_array<COLORREF>	colours;
	COLORREF				rowcol;

	operator ListViewControl&()		{ return vw; }

	int		AddColumn(int nc, const char *title, int width, COLORREF col) {
		ListViewControl2::Column(title).Width(width).Insert(vw, nc++);
		colours.push_back(col);
		return nc;
	}
	int		AddColour(int nc, COLORREF col) {
		while (colours.size() < nc)
			colours.push_back(col);
		return nc;
	}
	int		AddColumns(int nc, const C_type *type, string_accum &prefix, win::Colour col0, win::Colour col1);
	LRESULT	CustomDraw(NMCUSTOMDRAW	*nmcd, Control parent);
};

//-----------------------------------------------------------------------------
//	ColourTree
//-----------------------------------------------------------------------------

struct ColourTree : Subclass<ColourTree, TreeControl> {
	win::Colour*	colours;
	const Cursor*	cursors;
	uint8			(*cursor_indices)[3];
	HTREEITEM		hot;

	ColourTree(win::Colour *colours, const Cursor *cursors, uint8 (*cursor_indices)[3]) : colours(colours), cursors(cursors), cursor_indices(cursor_indices), hot(0) {}

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT	CustomDraw(NMCUSTOMDRAW	*nmcd) const;
	void	SetCursor(int type, int mod) const;
};

//-----------------------------------------------------------------------------
//	Asset Tables
//-----------------------------------------------------------------------------

template<typename T, typename U = T, typename A = dynamic_array<U>> struct EntryTable0 : ListViewControl, SortColumn {
	A		&table;
	IDFMT	format;

	void WriteRow(int row) {
		U		*u	= GetEntry(row);
		Item	item;
		item.Text(u->name).Set(*this, row);
		item.Column(2).Text(to_string(GetSize(*(T*)u))).Set(*this);
		FillRow(*this, item.Column(3), format, fields<T>::f, (uint32*)u);
	}

	void WriteColumn(int row, int col) {
		U		*u	= GetEntry(row);
		if (col == 0) {
			Item(u->GetName()).Column(col).Set(*this, row);

		} else if (col >= 3) {
			field	f	= FieldIndex(fields<T>::f, col - 3);
			Item(PutConst(lvalue(buffer_accum<256>()), IDFMT_CAMEL, &f, f.get_value((uint32*)u))).Column(col).Set(*this, row);
		}
	}

	bool UpdateColumn(int row, int col, const char *value) {
		if (col >= 3) {
			field	f	= FieldIndex(fields<T>::f, col - 3);
			uint64	v	= f.values == sSigned ? uint64(from_string<int64>(value)) : from_string<uint64>(value);
			U		*u	= GetEntry(row);
			f.set_value((uint32*)u, v);
			return true;
		}
		return false;
	}

	int InitColumns() {
		AddColumns(
			"name",		200,
			"used at",	100,
			"size",		100
		);
		return MakeColumns(*this, fields<T>::f, IDFMT_CAMEL | IDFMT_FOLLOWPTR, 3);
	}

	HWND Create(const WindowPos &wpos, text title, Style style = CHILD | CLIPSIBLINGS | VISIBLE | REPORT | SINGLESEL | SHOWSELALWAYS, StyleEx styleEx = GRIDLINES | DOUBLEBUFFER | FULLROWSELECT, ID id = ID()) {
		HWND h = ListViewControl::Create(wpos, title, style, styleEx, id);
		user = this;
		SetExtendedStyle(styleEx);
		InitColumns();
		return h;
	}
	HWND CreateWithID(const WindowPos &wpos, text title, ID id, Style style = CHILD | CLIPSIBLINGS | VISIBLE | REPORT | SINGLESEL | SHOWSELALWAYS, StyleEx styleEx = GRIDLINES | DOUBLEBUFFER | FULLROWSELECT) {
		return Create(wpos, title, style, styleEx, id);
	}

	U*			GetEntry(int i)			const	{ return GetItemParam(i); }
	int			GetEntryIndex(int i)	const	{ return GetEntry(i) - table; }

	EntryTable0(A &table, IDFMT format = IDFMT_FOLLOWPTR | IDFMT_FIELDNAME_AFTER_UNION) : table(table), format(format) {}
};

} // namespace app

#endif // VIEWER_IDENTIFIER_H
