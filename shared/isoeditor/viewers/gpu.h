#ifndef GPU_H
#define GPU_H

#include "viewer_identifier.h"
#include "stream.h"
#include "graphics.h"
#include "maths/geometry.h"
#include "resource.h"

#include "viewers/viewer.h"
#include "viewers/debug.h"
#include "windows/treecolumns.h"
#include "windows/splitter.h"
#include "windows/control_helpers.h"
#include "windows/d2d.h"

#include "extra/memory_cache.h"
#include "extra/identifier.h"
#include "extra/kd_tree.h"
#include "extra/octree.h"
#include "extra/gpu_helpers.h"
#include "filetypes/3d/model_utils.h"

namespace iso {

//-----------------------------------------------------------------------------
//	TypedBuffer
//-----------------------------------------------------------------------------

struct TypedBuffer : memory_block {
	uint32			stride;
	uint32			divider	= 0;
	SoftType		format;
	bool			own_mem;
	TypedBuffer() : stride(0), own_mem(false) {}
	TypedBuffer(const memory_block &mem, uint32 stride, const C_type *format = 0) : memory_block(mem), stride(stride), format(format), own_mem(false) {}
	TypedBuffer(malloc_block &&mem, uint32 stride, const C_type *format = 0) : memory_block(mem.detach()), stride(stride), format(format), own_mem(true) {}
	~TypedBuffer()									{ if (own_mem) free(p); }
	TypedBuffer(const TypedBuffer &b)				{ raw_copy(b, *this); if (own_mem) duplicate(*this); }
	TypedBuffer(TypedBuffer &&b)					{ raw_copy(b, *this); b.p = 0; }
	TypedBuffer& operator=(const TypedBuffer &b)	{ if (own_mem) free(p); raw_copy(b, *this); if (own_mem) duplicate(*this); return *this; }
	TypedBuffer& operator=(TypedBuffer &&b)			{ raw_swap(*this, b); return *this; }

	void*		operator[](int i)	const	{ return *this + stride * i; }
	size_t		size()				const	{ return stride ? (length() + stride - format.Size()) / stride : 0; }
	uint32		size32()			const	{ return uint32(size()); }
	auto		begin()				const	{ return make_param_iterator(make_stride_iterator((const uint8*)memory_block::begin(), stride), format); }
	auto		end()				const	{ return make_param_iterator(make_stride_iterator((const uint8*)memory_block::end(), stride), format); }
	auto		clamped_begin()		const	{ return make_param_iterator(make_stride_iterator((const uint8*)memory_block::begin(), stride), this); }
	//friend void swap(TypedBuffer &a,TypedBuffer &b) { raw_swap(a, b); }
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

template<typename C, typename T> int GetThumbnail(Control control, ImageList &images, C *c, T *t) {
	if (t) {
		int	index = images.Add(win::Bitmap::Load("IDB_WAIT", 0, images.GetIconSize()));
		ConcurrentJobs::Get().add([control, images, index, c, t] {
			void			*bits;
			win::Bitmap		bitmap(win::Bitmap::CreateDIBSection(DIBHEADER(images.GetIconSize(), 32), &bits));
			MakeThumbnail(bits, c->GetBitmap(t), images.GetIconSize());
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

	bool	Command(MainWindow &main, ID id, MODE mode);
#endif
};

//-----------------------------------------------------------------------------
//	ListViews
//-----------------------------------------------------------------------------

struct BatchList : dynamic_array<uint32> {
	friend bool operator<(const BatchList &a, const BatchList &b) {
		return (a.size() ? (int)a.front() : -1) < (b.size() ? (int)b.front() : -1);
	}
};
typedef dynamic_array<BatchList>	BatchListArray;


string_accum&	WriteBatchList(string_accum &sa, BatchList &bl);
int				SelectBatch(HWND hWnd, const Point &mouse, BatchList &b, bool always_list);

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
	MeshNotification(Control from, Command c, int _v) : Notification(from, c), v(_v) {}


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
template<typename P> float3x4 prim_barycentric(const P &prim) {
	position3	pos[64];
	uint32		num_verts	= prim.size();
	copy(prim, pos);
	return inverse(triangle3(pos[0], pos[1], pos[2]).matrix());
}

struct index {
	uint8	a[4];
	index(uint8 i) { a[0] = a[1] = a[2] = a[3] = i; }
};

struct vertex_idx;
struct vertex_norm;

inline index *Rectify(index *p) {
	p[0]	= p[-1];
	p[1]	= p[-2];
	p[2]	= p[-2];
	return p + 3;
}


struct Axes {
	int			size;
	pass		*tech;

	Axes(uint32 _size, pass *_tech) : size(_size), tech(_tech) {}

	Rect		window(const Rect &client)	const { return client.Subbox(0, -size, size, 0); }
	float4x4	proj()						const { return parallel_projection(0.7f, 0.7f, -10.f, 10.f); }
	void		draw(GraphicsContext &ctx, param(float3x3) rot, const ISO::Browser &params)	const;
	int			click(const Rect &client, const point &mouse, param(float3x3) rot) const;
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

	ToolBarControl			toolbar;
	Axes					axes	= {100, 0};

	VertexBuffer<float4p>	vb;
	VertexBuffer<float3p>	vb_norm;
	VertexBuffer<index>		vb_matrix;
	IndexBuffer<uint32>		ib;
	dynamic_array<int>		patch_starts;
	Texture					ct;
	Texture					dt;

	//SpacePartition		partition;
	//kd_tree<dynamic_array<constructable<float3p> > > partition;
	octree				oct;
	cuboid				extent;
	int					num_verts;
	int					num_prims;
	float4				matrices[64];

	float4x4			fixed_proj	= identity;
	float3x2			viewport	= {float3(one), zero};
	cuboid				scissor		= unit_cube;
	Point				screen_size;
	Topology2			topology;

	MODE				mode		= SCREEN, prev_mode;
	BackFaceCull		cull		= BFC_BACK;
	iso::flags<FLAGS>	flags		= 0;
	float				move_scale	= 1;
	rot_trans			view_loc	= identity;
	rot_trans			target_loc	= identity;
	quaternion			light_rot	= identity;
	float				zoom		= 0.8f;
	float				zscale		= 1024;

	int					select		= -1;
	uint32				prevbutt	= 0;
	Point				prevmouse;

	int	PrimFromVertex(int v, bool hw) {
		if (topology.chunks)
			return topology.PrimFromVertexChunks(v, hw);
		return clamp(topology.PrimFromVertex(v, hw), 0, num_prims - 1);
	}
	int	VertexFromPrim(int v, bool hw) {
		return max(topology.VertexFromPrimChunks(v, hw), 0);
	}
	void				ResetView();
	void				Paint();
	void				SelectVertex(const Point &mouse);
	void				MouseMove(Point mouse, int buttons);
	void				MouseWheel(Point mouse, int buttons, int x);

	void				DrawMesh(GraphicsContext &ctx, int v, int n);
	void				DrawMesh(GraphicsContext &ctx)		{ DrawMesh(ctx, 0, num_prims); }
public:
	static uint32		get_class_style()					{ return CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW; }

	void				SetSelection(int i, bool zoom = false);
	void				GetMatrices(float4x4 *world, float4x4 *proj) const;
	float4x4			GetMatrix() const;
	void				Init(const Topology2 _topology, int num_verts, param(cuboid) &_extent, MODE _mode = SCREEN);
	void				SetColourTexture(const Texture &_ct);
	void				SetDepthTexture(const Texture &_dt);
	void				SetScissor(param(rectangle) _scissor);
	void				SetScissor(param(cuboid) _scissor);
	void				SetDepthBounds(float zmin, float zmax);
	void				SetScreenSize(const Point &s);
	void				AddPatchStart(int i)				{ patch_starts.push_back(i); }
	bool				IsChunkStart(int i) const;

	LRESULT				Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	MeshWindow()		{}
	MeshWindow(const WindowPos &wpos, ID id = ID());
};

//-----------------------------------------------------------------------------
//	MeshVertexWindow
//-----------------------------------------------------------------------------

class MeshVertexWindow : public win::SplitterWindow {
public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	MeshVertexWindow(const WindowPos &wpos, const char *title, ID id = ID());
};

//-----------------------------------------------------------------------------
//	VertexOutputWindow
//-----------------------------------------------------------------------------

struct VertexWindow : public Window<VertexWindow>, ColourList {
	EditControl2					edit_control;
	uint32							edit_row, edit_col;
	uint32							num_verts;
	uint32							num_instances;
	dynamic_array<uint16>			indexing;
	dynamic_array<TypedBuffer>		buffers;

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	void	Show();
	VertexWindow(const WindowPos &wpos, const char *title, ID id, dynamic_array<uint16> &&indexing);
	VertexWindow(const WindowPos &wpos, const char *title, ID id, named<TypedBuffer> *buffers, uint32 nb, dynamic_array<uint16> &&indexing, uint32 num_instances = 1);
};

struct VertexOutputWindow : public Window<VertexOutputWindow>, ColourList {
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
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
//	static TimingWindow	*me;
	Control						owner;
	d2d::WND					d2d;
	win::ToolTipControl			tooltip;
	bool						tip_on;
	com_ptr<ID2D1PathGeometry>	marker_geom;

	Point		prevmouse;
	int			last_batch;
	float		tscale, yscale, time;

	Rect		DragStrip()				const	{ return GetClientRect().Subbox(0, 0, 0, 12); }
	float		ClientToTime(int x)		const	{ return float(x) / GetClientRect().Width() * tscale + time; }
	int			TimeToClient(float t)	const	{ return (t - time) / tscale * GetClientRect().Width(); }
	float2x3	GetTrans()				const	{ auto s = d2d.Size(); return translate(0, s.y) * scale(s.x / tscale, -s.y / yscale) * translate(float2{-time, 0}); }

	void		DrawMarker(d2d::Target &d2d, const d2d::point &pos);
	void		DrawGrid(d2d::Target &d2d, IDWriteTextFormat *font);

public:
	enum {ID = 'TW'};
	static HCURSOR	get_class_cursor()		{ return 0; }
//	static TimingWindow *Get()			{ return me; }

	LRESULT		Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	TimingWindow(const WindowPos &wpos, Control owner);
	~TimingWindow() {}// me = 0; }
};

//-----------------------------------------------------------------------------
//	funcs
//-----------------------------------------------------------------------------

Control			MakeBufferWindow(const WindowPos &wpos, const char *title, ID id, const TypedBuffer &buff);
Control			MakeBufferWindow(const WindowPos &wpos, const char *title, ID id, TypedBuffer &&buff);
VertexWindow*	MakeVertexWindow(const WindowPos &wpos, const char *title, ID id, named<TypedBuffer> *buffers, uint32 nb, const indices &ix = 0, uint32 num_instances = 1);
MeshWindow*		MakeMeshView(const WindowPos &wpos, Topology2 topology, const TypedBuffer &vb, const indices &ix, param(float3x2) viewport, BackFaceCull cull, MeshWindow::MODE mode);
Control			MakeComputeGrid(const WindowPos &wpos, const char *title, ID id, const uint3p &dim, const uint3p &group);
uint32			GetComputeIndex(Control c, const Point &pt);

} // namespace app

#endif // GPU_H
