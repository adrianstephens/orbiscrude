#ifndef GPU_H
#define GPU_H

#include "viewer_identifier.h"
#include "stream.h"
#include "graphics.h"
#include "utilities.h"
#include "maths/geometry.h"
#include "resource.h"

#include "viewers/viewer.h"
#include "viewers/debug.h"
#include "windows/treecolumns.h"
#include "windows/splitter.h"
#include "windows/d2d.h"

#include "extra/memory_cache.h"
#include "extra/identifier.h"
#include "extra/kd_tree.h"
#include "extra/octree.h"
#include "extra/gpu_helpers.h"
#include "filetypes/3d/model_utils.h"

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
	bool	operator!()						const { return !t; }
	bool	operator==(const SoftType &b)	const { return t == b.t; }

	friend string_accum& operator<<(string_accum &a, const SoftType &s) { return s.Name(a, -1); }
	friend string_accum& operator<<(string_accum &a, const param_element<const uint8&, const SoftType&> &s) { return s.p.Get(a, &s.t, -1); }
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

template<> SoftType::vtable SoftType::vtable_s<const C_type>::v = {
	[](void *t)																	->size_t		{ return ((const C_type*)t)->size(); },
	[](void *t)																	->int			{ return NumElements((const C_type*)t); },
	[](void *t, string_accum &sa, int i, const char *name)						->string_accum&	{ int shift; void *data = 0; return DumpType(sa, i < 0 ? (const C_type*)t : GetNth(data, (const C_type*)t, i, shift), name, 0); },
	[](void *t, string_accum &sa, int i)										->string_accum&	{ return GetNthName(sa, (const C_type*)t, i); },
	[](void *t, string_accum &sa, const void *data, int i, FORMAT::FLAGS flags)	->string_accum&	{ int shift; auto subtype = GetNth(data, (const C_type*)t, i, shift); return DumpData(sa, data, subtype, shift, 0, flags); },
	[](void *t, string_scan &ss, void *data, int i)								->string_scan&	{ int shift; auto subtype = GetNth(data, (const C_type*)t, i, shift); return SetData(ss, data, subtype, shift); },
	[](void *t, const void *data, int i)										->float			{ int shift; auto subtype = GetNth(data, (const C_type*)t, i, shift); return iso::GetFloat(data, subtype, shift); }
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
//	TypedBuffer
//-----------------------------------------------------------------------------

struct TypedBuffer : memory_block_own {
	uint32			stride;
	uint32			divider	= 0;
	SoftType		format;
	TypedBuffer(uint32 stride = 0) : stride(stride) {}
	TypedBuffer(const memory_block &mem, uint32 stride = 0, SoftType format = {})	: memory_block_own(mem), stride(stride), format(format) {}
	TypedBuffer(malloc_block &&mem, uint32 stride = 0, SoftType format = {})		: memory_block_own(move(mem)), stride(stride), format(format) {}
	TypedBuffer(const TypedBuffer &b)												: memory_block_own(b), stride(b.stride), format(b.format) {}
	TypedBuffer(TypedBuffer &&b)													: memory_block_own(move(b)), stride(b.stride), format(b.format) {}
	template<typename T> TypedBuffer(const _ptr_array<T> &a)						: TypedBuffer(unconst(a).raw_data(), sizeof(T), ctypes.get_type<T>()) {}
	template<typename T> TypedBuffer(dynamic_array<T> &&a)							: TypedBuffer(a.detach_raw(), sizeof(T), ctypes.get_type<T>()) {}

//	TypedBuffer& operator=(const TypedBuffer &b)	= default;
	TypedBuffer& operator=(TypedBuffer &&b)			= default;

	TypedBuffer& operator+=(const TypedBuffer &b)	{
		ISO_ASSERT(stride == b.stride);// && format == b.format);
		memory_block_own::operator+=(b);
		return *this;
	}

	void*		operator[](int i)	const	{ return (uint8*)p + stride * i; }
	size_t		raw_size()			const	{ return memory_block_own::size(); }
	size_t		size()				const	{ return stride ? (raw_size() + stride - (!format ? 1 : format.Size())) / stride : 1; }
	uint32		size32()			const	{ return uint32(size()); }
	auto		begin()				const	{ return make_param_iterator(make_stride_iterator((const uint8*)memory_block_own::begin(), stride), format); }
	auto		end()				const	{ return make_param_iterator(make_stride_iterator((const uint8*)memory_block_own::end(), stride), format); }
	auto		clamped_begin()		const	{ return make_param_iterator(make_stride_iterator((const uint8*)memory_block_own::begin(), stride), this); }
	auto		extend(uint32 n)			{ return make_strided<void>(memory_block_own::extend(n * stride), stride); }
	auto&		resize(uint32 n)			{ memory_block_own::resize(n * stride); return *this; }
};

template<typename T> void assign(T &f, const param_element<const uint8&, const TypedBuffer*> &a)	{
	if (&a.t >= a.p->memory_block::end()) {
		f = zero;
		return;
	}
	typedef element_type<T>	E;
	f = to<E>(
		a.p->format.GetFloat(&a.t, 0),
		a.p->format.GetFloat(&a.t, 1),
		a.p->format.GetFloat(&a.t, 2),
		a.p->format.GetFloat(&a.t, 3)
	);
}

}

//-----------------------------------------------------------------------------
//	Views
//-----------------------------------------------------------------------------

namespace app {
using namespace iso;
using namespace win;

enum {
	WM_ISO_BATCH	= WM_USER + 100,
	WM_ISO_JOG,
};

extern Cursor CURSOR_LINKBATCH, CURSOR_LINKDEBUG, CURSOR_ADDSPLIT;

Control ErrorControl(const WindowPos &wpos, const char *error);
Control BinaryWindow(const WindowPos &wpos, const ISO::Browser2 &b, ID id = ID());
Control BitmapWindow(const WindowPos &wpos, const ISO_ptr_machine<void> &p, const char *text, bool auto_scale);
Control ThumbnailWindow(const WindowPos &wpos, const ISO_ptr_machine<void> &p, const char *_text);
bool	MakeThumbnail(void *dest, const ISO_ptr_machine<void> &p, const POINT &size);
int		MakeHeaders(ListViewControl lv, int nc, const SoftType &type, string_accum &prefix);

template<typename C, typename T> int GetThumbnail(Control control, ImageList &images, C *c, T *t) {
	if (t) {
		int	index = images.Add(win::Bitmap::Load("IDB_WAIT", 0, images.GetIconSize()));
		ConcurrentJobs::Get().add([control, images, index, c, t] {
			void			*bits;
			win::Bitmap		bitmap(win::Bitmap::CreateDIBSection(DIBHEADER(images.GetIconSize(), 32), &bits));
			if (!MakeThumbnail(bits, c->GetBitmap(t), images.GetIconSize()))
				bitmap = win::Bitmap::Load("IDB_BAD");
			images.Replace(index, bitmap);
			control.Invalidate(0, false);
		});
		return index;
	}
	return images.Add(win::Bitmap::Load("IDB_BAD"));
}

class EditorGPU : public Editor {
public:
#ifndef ISO_EDITOR
	static int	num_frames;
	static bool	until_halt;
	static bool	resume_after;

	bool	Command(MainWindow &main, ID id);
#endif
};

//-----------------------------------------------------------------------------
//	ListViews
//-----------------------------------------------------------------------------

struct BatchList : dynamic_array<uint32> {
	void	push_back(uint32 a) {
		if (empty() || back() != a)
			dynamic_array<uint32>::push_back(a);
	}
	friend bool operator<(const BatchList &a, const BatchList &b) {
		return (a.size() ? (int)a.front() : -1) < (b.size() ? (int)b.front() : -1);
	}
};
typedef dynamic_array<BatchList>	BatchListArray;

string_accum&	WriteBatchList(string_accum &sa, BatchList &bl);
int				SelectBatch(HWND hWnd, const Point &mouse, BatchList &b, bool always_list);

template<typename A> struct UsageSorter2 : ColumnSorter {
	typedef element_t<A>	T;
	const BatchList	*batchlists;
	const A			&array;
	int				first(const T *t)		{ return batchlists[array.index_of(t)].size() ? (int)batchlists[array.index_of(t)][0] : -1; }
	int	operator()(const T *a, const T *b)		{ return compare(first(a), first(b)); }
	UsageSorter2(const A &array, const BatchList *batchlists, int dir) : ColumnSorter(dir), batchlists(batchlists), array(array) {}
};

template<typename T> struct UsageSorter : ColumnSorter {
	const BatchList	*batchlists;
	const T			*array;
	int				first(const T *t)		{ return batchlists[t - array].size() ? (int)batchlists[t - array][0] : -1; }
	int	operator()(const T *a, const T *b)	{ return compare(first(a), first(b)); }
	UsageSorter(const T *array, const BatchList *batchlists, int dir) : ColumnSorter(dir), batchlists(batchlists), array(array) {}
};

template<typename T> struct SizeSorter : ColumnSorter {
	int	operator()(T *a, T *b) {
		return compare(GetSize(*a), GetSize(*b));
	}
	SizeSorter(int dir) : ColumnSorter(dir) {}
};

//-----------------------------------------------------------------------------
//	Asset Tables
//-----------------------------------------------------------------------------

template<typename T, typename U = T, typename A = dynamic_array<U>> struct EntryTable : EntryTable0<T, U, A> {
	BatchListArray		usedat;

	void SortOnColumn(int col) {
		int	dir = SetColumn(GetHeader(), col);
		switch (col) {
			case 0:
				SortByIndex(ColumnTextSorter(*this, col, dir));
				break;
			case 1:
				SortByParam(UsageSorter<U>(table, usedat, dir));
				break;
			case 2:
				SortByParam(SizeSorter<T>(dir));
				break;
			default:
				SortByParam(FieldSorter(FieldIndex(fields<T>::f, col - 3), dir));
				break;
		}
	}
	void AddEntry(const RegisterList::cb_type &cb, const U &u, int i, int image = 0) {
		if (i >= usedat.size32())
			usedat.resize(i + 1);
		Item	item;
		item.Text(u.GetName()).Image(image).Param(&u).Insert(*this);
		item.Column(1).Text(WriteBatchList(lvalue(buffer_accum<256>()), usedat[i])).Set(*this);
		item.Column(2).Text(to_string(GetSize((T&)u))).Set(*this);
		FillRow(*this, cb, item.Column(3), this->format, fields<T>::f, (uint32*)&u);
	}
	void AddEntry(const U &u, int i, int image = 0) {
		AddEntry(none, u, i, image);
	}

	BatchList&	GetBatches(int i)		const	{ return usedat[GetEntryIndex(i)]; }

	EntryTable(A &table, IDFMT format = IDFMT_FOLLOWPTR | IDFMT_NOPREFIX | IDFMT_FIELDNAME_AFTER_UNION) : EntryTable0<T, U, A>(table, format), usedat(table.size()) {}
};

//-----------------------------------------------------------------------------
//	MeshWindow
//-----------------------------------------------------------------------------

struct MeshNotification : win::Notification {
	enum Command {
		SET		= 0x8000,
		CHANGE,
	};
	int		v;
	MeshNotification(Control from, Command c, int v) : Notification(from, c), v(v) {}

	LRESULT	Process(ListViewControl &lv) {
		int	s = lv.Selected().front();
		if (s >= 0)
			lv.ClearItemState(s, LVIS_SELECTED);
		lv.SetItemState(v, LVIS_SELECTED);
		lv.EnsureVisible(v);
		return 0;
	}

	static MeshNotification Set(Control from, int v)	{ return MeshNotification(from, SET, v); }
	static MeshNotification Change(Control from, int v) { return MeshNotification(from, CHANGE, v); }
};


template<typename P> bool prim_check_ray(const P &prim, param(ray3) r, float &t) {
	position3	pos[64];
	uint32		num_verts	= prim.size();

	copy(prim, pos);

	float3x4	tri(pos[1] - pos[0], pos[2] - pos[0], -r.d, pos[0]);
	float3		u	= r.p / tri;
	if (any(u < zero))
		return false;

	if (prim.is_rect() ? all(u.xy <= one) : (u.x + u.y) <= one) {
		t = u.z;
		return true;
	}
	return false;
}

template<typename P> triangle3 prim_triangle(const P &prim) {
	position3	pos[64];
	uint32		num_verts	= prim.size();
	copy(prim, pos);
	return triangle3(pos[0], pos[1], pos[2]);
}

struct index {
	uint8	a[4];
	index(uint8 i) { a[0] = a[1] = a[2] = a[3] = i; }
};

inline index *Rectify(index *p) {
	p[0]	= p[-1];
	p[1]	= p[-2];
	p[2]	= p[-2];
	return p + 3;
}


struct Axes {
	int			size;
	pass		*tech;

	Axes(uint32 size, pass *tech) : size(size), tech(tech) {}

	Rect		window(const Rect &client)	const { return client.Subbox(0, -size, size, 0); }
	float4x4	proj()						const { return parallel_projection(0.7f, 0.7f, -10.f, 10.f); }
	void		draw(GraphicsContext &ctx, param(float3x3) rot, const ISO::Browser &params)	const;
	int			click(const Rect &client, const point &mouse, param(float3x3) rot)			const;
};

struct MeshWindow : public aligned<win::Window<MeshWindow>, 16>, win::WindowTimer<MeshWindow>, Graphics::Display, refs<MeshWindow> {
	enum MODE	{SCREEN, PERSPECTIVE, SCREEN_PERSP, FIXEDPROJ};
	enum FLAGS	{
		FILL			= 1 << 0,
		SCISSOR			= 1 << 1,
		FRUSTUM_PLANES	= 1 << 2,
		FRUSTUM_EDGES	= 1 << 3,
		BOUNDING_EDGES	= 1 << 4,
		SCREEN_RECT		= 1 << 5,
	};

	struct Item {
		cuboid	extent	= empty;

		virtual ~Item()	{}
		virtual void	Draw(MeshWindow *mw, GraphicsContext &ctx) = 0;
		virtual int		Select(MeshWindow *mw, const float4x4 &mat) = 0;
		virtual void	Select(MeshWindow *mw, int i) = 0;
	};

	ToolBarControl			toolbar;
	Axes					axes	= {100, 0};

	dynamic_array<int>		patch_starts;
	Texture					ct;
	Texture					dt;

	cuboid				extent		= empty;
	float4x4			fixed_proj	= identity;
	float3x2			viewport	= {float3(one), zero};
	cuboid				scissor		= unit_cube;
	point				screen_size;
	MODE				mode		= SCREEN, prev_mode;
	BackFaceCull		cull		= BFC_BACK;
	iso::flags<FLAGS>	flags		= FILL;
	float				move_scale	= 1;
	rot_trans			view_loc	= identity;
	rot_trans			target_loc	= identity;
	quaternion			light_rot	= identity;
	float				zoom		= 0.8f;
	float				zscale		= 1024;

	int					select		= -1;
	uint32				prevbutt	= 0;
	Point				prevmouse;

	dynamic_array<unique_ptr<Item>>	items;

	void				ResetView();
	void				Paint();
	void				Select(const Point &mouse);
	void				MouseMove(Point mouse, int buttons);
	void				MouseWheel(Point mouse, int buttons, int x);

public:
	static uint32		get_class_style()					{ return CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW; }

	void				SetTarget(rot_trans t)				{ target_loc = t; Timer::Start(0.01f); }
	void				SetSelection(int i, bool zoom = false);
	void				GetMatrices(float4x4 *world, float4x4 *proj) const;
	float4x4			GetMatrix() const;
	void				SetColourTexture(const Texture &_ct);
	void				SetDepthTexture(const Texture &_dt);
	void				SetScissor(param(rectangle) _scissor);
	void				SetScissor(param(cuboid) _scissor);
	void				SetDepthBounds(float zmin, float zmax);
	void				SetScreenSize(const point &s);
	void				AddPatchStart(int i)				{ patch_starts.push_back(i); }
	int					ChunkOffset(int i)		const;
	int					PrimFromVertex(int v)	const;
	void				AddDrawable(Item *p);

	LRESULT				Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	MeshWindow(param(float3x2) viewport, BackFaceCull cull, MeshWindow::MODE mode) : viewport(viewport), mode(mode), cull(cull), move_scale(len(viewport.x)) {}
	MeshWindow(const WindowPos &wpos, param(float3x2) viewport, BackFaceCull cull, MeshWindow::MODE mode) : MeshWindow(viewport, cull, mode) {
		Create(wpos, "Mesh", Control::CHILD | Control::CLIPCHILDREN | Control::CLIPSIBLINGS, Control::NOEX);
	}
};

struct MeshInstance : MeshWindow::Item {
	float3x4				transform	= identity;
	Topology2				topology;
	int						num_verts;
	int						num_prims;
	VertexBuffer<float4p>	vb;
	VertexBuffer<float3p>	vb_norm;
	IndexBuffer<uint32>		ib;
	VertexBuffer<index>		vb_matrix;
	float4					matrices[64];
	octree					oct;

	int	PrimFromVertex(int v, bool hw) {
		if (topology.chunks)
			return topology.PrimFromVertexChunks(v, hw);
		return clamp(topology.PrimFromVertex(v, hw), 0, num_prims - 1);
	}
	int	VertexFromPrim(int v, bool hw) {
		return max(topology.VertexFromPrimChunks(v, hw), 0);
	}

	void	DrawMesh(GraphicsContext &ctx, int v, int n);
	void	DrawMesh(GraphicsContext &ctx)		{ DrawMesh(ctx, 0, num_prims); }

	void	Draw(MeshWindow *mw, GraphicsContext &ctx) override;
	int		Select(MeshWindow *mw, const float4x4 &mat) override;
	void	Select(MeshWindow *mw, int i) override;

	MeshInstance(const Topology2 topology, const TypedBuffer &pos, const indices &ix, bool use_w);
};

struct MeshAABBInstance : MeshWindow::Item {
	float3x4	transform	= identity;
	dynamic_array<cuboid>	boxes;
	void	Draw(MeshWindow *mw, GraphicsContext &ctx) override;
	int		Select(MeshWindow *mw, const float4x4 &mat) override;
	void	Select(MeshWindow *mw, int i) override;
	MeshAABBInstance(const TypedBuffer &b);
};


//-----------------------------------------------------------------------------
//	MeshVertexWindow
//-----------------------------------------------------------------------------

class MeshVertexWindow : public win::SplitterWindow {
public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	MeshVertexWindow(const WindowPos &wpos, text title, ID id = ID());
};

//-----------------------------------------------------------------------------
//	VertexOutputWindow
//-----------------------------------------------------------------------------

struct VertexWindow : public Window<VertexWindow>, ColourList {
	EditControl2					edit_control;
	uint32							edit_row, edit_col;
	uint32							num_verts;
	uint32							num_instances;
	dynamic_array<TypedBuffer>		buffers;
	dynamic_array<uint32>			indexing;

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	void	Show();
	uint32	NumUnique()	const;
	VertexWindow(const WindowPos &wpos, const char *title, ID id, dynamic_array<uint32> &&indexing);

	void AddVertices(range<TypedBuffer*> _buffers, const dynamic_array<uint32> &_indexing, uint32 _num_instances);
	void AddBuffer(const TypedBuffer& buffer, const char *name);

};

struct VertexOutputWindow : public Window<VertexOutputWindow>, ColourList {
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	VertexOutputWindow(const WindowPos &wpos, const char *title, ID id) {
		Create(wpos, 0, CHILD | CLIPCHILDREN | CLIPSIBLINGS);
		vw.id	= id;
	}
	void FillShaderIndex(int num);
};

//-----------------------------------------------------------------------------
//	TimingWindow
//-----------------------------------------------------------------------------

class TimingWindow : public win::Window<TimingWindow> {
protected:
	Control						owner;
	d2d::WND					d2d;
	win::ToolTipControl			tooltip;
	bool						tip_on;
	com_ptr<ID2D1PathGeometry>	marker_geom;

	Point		prevmouse;
	int			last_batch	= 0;
	float		tscale		= 1, yscale = 1, time = 0;

	Rect		DragStrip()				const	{ return GetClientRect().Subbox(0, 0, 0, 12); }
	float		ClientToTime(int x)		const	{ return float(x) / GetClientRect().Width() * tscale + time; }
	int			TimeToClient(float t)	const	{ return (t - time) / tscale * GetClientRect().Width(); }
	float2x3	GetTrans()				const	{ auto s = d2d.Size(); return translate(0, s.y) * scale(s.x / tscale, -s.y / yscale) * translate(float2{-time, 0}); }

	void		DrawMarker(d2d::Target &d2d, const d2d::point &pos);
	void		DrawGrid(d2d::Target &d2d, IDWriteTextFormat *font);

public:
	enum {ID = 'TW'};
	static HCURSOR	get_class_cursor()		{ return 0; }

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	TimingWindow(const WindowPos &wpos, Control owner);
	~TimingWindow() {}// me = 0; }
};

//-----------------------------------------------------------------------------
//	Compute Grid
//-----------------------------------------------------------------------------

class ComputeGrid : public d2d::Window {
	win::ToolTipControl		tooltip;
	bool					tip_on;

	d2d::Write	write;
	d2d::Font	font;

	float		zoom		= 1;
	d2d::point	pos			= {0, 0};
	d2d::point	prevmouse;

	uint3p		dim, group;
	uint32		selected	= ~0;
	bool		dim_swap;

	d2d::point	ClientToWorld(const d2d::point &pt) const { return (pt - pos) * (1.0f / zoom); }
	float2x3	GetTransform()						const { return translate(pos) * scale(zoom); }

	string_accum&	GetSelectedText(string_accum &&a, uint32x3 group, uint32x3 dim) const;

public:
	bool		GetLoc(position2 mouse, uint3p &dim_loc, uint3p &group_loc) const;
	void		Paint() const;
	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	ComputeGrid(const WindowPos &wpos, const char *title, ID id, const uint3p &dim, const uint3p &group);
};

//-----------------------------------------------------------------------------
//	funcs
//-----------------------------------------------------------------------------

Control			MakeBufferWindow(const WindowPos &wpos, text title, ID id, const TypedBuffer &buff);
Control			MakeBufferWindow(const WindowPos &wpos, text title, ID id, TypedBuffer &&buff);
VertexWindow*	MakeVertexWindow(const WindowPos &wpos, text title, ID id, range<named<TypedBuffer>*> buffers, const indices &ix = 0, uint32 num_instances = 1);
MeshWindow*		MakeMeshView(const WindowPos &wpos, Topology2 topology, const TypedBuffer &vb, const indices &ix, param(float3x2) viewport, BackFaceCull cull, MeshWindow::MODE mode);

} // namespace app

#endif // GPU_H
