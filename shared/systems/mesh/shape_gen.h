#include "graphics.h"
#include "common/shader.h"

namespace iso {

#ifdef PLAT_PS3
#define BUFFER_FLAGS		MEM_HOST
#else
#define BUFFER_FLAGS		MEM_CPU_WRITE
#endif

struct VertexIndexBuffer {
	struct vertex {
		float3p pos;
		float3p norm;
		void set(param(float3) p, param(float3) n) { pos = p; norm = n; }
	};

	VertexBuffer<vertex>	vb;
	IndexBuffer<uint16>		ib;
	uint32			num_verts, num_prims;

	vertex	*BeginVerts(uint32 n) {
		num_verts = n;
		return vb.Begin(num_verts, BUFFER_FLAGS);
	}

	uint16	*BeginPrims(uint32 n) {
		num_prims	= n;
		return ib.Begin(num_prims * 3, BUFFER_FLAGS);
	}

	void Render(GraphicsContext &ctx) const {
		ctx.SetVertices(vb);
		ctx.SetIndices(ib);
		ctx.DrawIndexedPrimitive(PRIM_TRILIST, 0, num_verts, 0, num_prims);
	}
};

template<> inline VertexElements GetVE<VertexIndexBuffer::vertex>() {
	static VertexElement ve[] = {
		VertexElement(&VertexIndexBuffer::vertex::pos, "position"_usage),
		VertexElement(&VertexIndexBuffer::vertex::norm, "normal"_usage)
	};
	return ve;
};


struct MeshHolder {
	struct vertex {
		position3	v;
		void set(param(position3) p, param(float3) n) { v = p; }
		operator position3() const { return position3(v); }
	};
	dynamic_array<vertex>	vertices;
	dynamic_array<uint16>	indices;
	vertex	*BeginVerts(uint32 n)	{ return vertices.resize(n).begin(); }
	uint16	*BeginPrims(uint32 n)	{ return indices.resize(n * 3).begin(); }
};

template<typename A> void RectangleVerts(A &a, float w, float h) {
	auto	*v		= a.BeginVerts(4);
	float3	norm	= float3{zero, zero, one};
	v++->set(float3{-w, -h, zero}, norm);
	v++->set(float3{+w, -h, zero}, norm);
	v++->set(float3{+w, +h, zero}, norm);
	v++->set(float3{-w, +h, zero}, norm);

	uint16	*x	= a.BeginPrims(6);
	*x++	= 0;
	*x++	= 1;
	*x++	= 3;

	*x++	= 3;
	*x++	= 1;
	*x++	= 2;
}

struct RectangleVB : public VertexIndexBuffer {
	RectangleVB(float w = 1, float h = 1) {
		RectangleVerts(*this, w, h);
		vb.End();
		ib.End();
	}
	static const RectangleVB&	get() {
		static RectangleVB	vib;
		return vib;
	}
	void RenderOutline(GraphicsContext &ctx) const {
		ctx.SetVertices(vb);
		ctx.DrawVertices(PRIM_LINESTRIP, 0, 4);
	}
	void RenderOutlineA(GraphicsContext &ctx) const {
		ctx.SetVertices(vb);
		ctx.DrawVertices(AdjacencyPrim(PRIM_LINESTRIP), 0, 4);
	}
};

template<typename A> void CircleVerts(A &a, int n, float r = 1, int adj = 0) {
	auto	*v		= a.BeginVerts(n + adj);
	float3	norm	= float3{zero, zero, one};
	for (int i = 0; i < n + adj; i++) {
		float2	r2  = sincos(float(i) / n * pi * 2);
		v++->set(concat(r2, zero), norm);
	}

	uint16	*x	= a.BeginPrims(n - 2);
	for (int i = 0, j = n - 1; i < j - 1;) {
		*x++	= j;
		*x++	= i;
		*x++	= ++i;

		if (i < j - 1) {
			*x++	= j;
			*x++	= i;
			*x++	= --j;
		}
	}
}

struct CircleVB : public VertexIndexBuffer {
	CircleVB(int n, float r = 1) {
		CircleVerts(*this, n, r, 2);
		vb.End();
		ib.End();
	}
	void RenderOutline(GraphicsContext &ctx) const {
		ctx.SetVertices(vb);
		ctx.DrawVertices(PRIM_LINESTRIP, 0, num_verts + 1);
	}
	void RenderOutlineA(GraphicsContext &ctx) const {
		ctx.SetVertices(vb);
		ctx.DrawVertices(AdjacencyPrim(PRIM_LINESTRIP), 0, num_verts + 2);
	}
	template<int N> static const CircleVB&	get() {
		static CircleVB	vib(N);
		return vib;
	}
};

template<typename A> void SphereVerts(A &a, int n, float r = 1, bool half = false) {
	int	lats		= half ? n / 4 : n / 2 - 1;

	auto	*v	= a.BeginVerts(n * lats + (half ? 1 : 2));
	v++->set(float3{0, 0, -r}, float3{0, 0, -1});
	for (int i = 1; i <= lats; i++) {
		float2	r2  = sincos((float(i) / n - .25f) * pi * 2);
		for (int j = 0; j < n; j++) {
			float3	norm	= concat(sincos(two * pi * j / n) * r2.x, r2.y);
			v++->set(norm * r, norm);
		}
	}
	if (!half)
		v++->set(float3{0, 0, +r}, float3{0, 0, 1});

	uint16	*x	= a.BeginPrims((lats - 1) * n * 2 + (half ? 1 : 2) * n);
	for (int i = 0; i < n; i++) {
		*x++	= 0;
		*x++	= (i + 1) % n + 1;
		*x++	= i + 1;
	}
	for (int i = 0; i < lats - 1; i++) {
		for (int j = 0; j < n; j++) {
			int	k = (j + 1) % n;
			*x++	= (i + 0) * n + j + 1;
			*x++	= (i + 1) * n + k + 1;
			*x++	= (i + 1) * n + j + 1;

			*x++	= (i + 0) * n + k + 1;
			*x++	= (i + 1) * n + k + 1;
			*x++	= (i + 0) * n + j + 1;
		}
	}
	if (!half) {
		for (int i = 0; i < n; i++) {
			*x++	= n * (n / 2 - 1) + 1;
			*x++	= n * (n / 2 - 2) + i + 1;
			*x++	= n * (n / 2 - 2) + (i + 1) % n + 1;
		}
	}
}

struct SphereVB : public VertexIndexBuffer {
	SphereVB(int n, float r = 1, bool half = false) {
		SphereVerts(*this, n, r, half);
		vb.End();
		ib.End();
	}
	template<int N> static const SphereVB&	get() {
		static SphereVB	vib(N);
		return vib;
	}
};

template<typename A> void TorusVerts(A &a, int n, int m, float r_outer = 1, float r_inner = 0.5f) {
	auto	*v = a.BeginVerts(n * m);
	float	r_middle	= (r_outer + r_inner) / 2;
	float	r_tube		= (r_outer - r_inner) / 2;
	for (int i = 0; i < n; i++) {
		float2	r2  = sincos(two * pi * i / n);
		for (int j = 0; j < m; j++) {
			float2	r3		= sincos(float(j) / m * pi * 2);
			float	r		= r_middle + r3.x * r_tube;
			v++->set(concat(r2 * r, r3.y * r_tube), concat(r2 * r3.x, r3.y));
		}
	}

	uint16	*x = a.BeginPrims(n * m * 2);
	for (int i = 0; i < n; i++) {
		int	i0 = i * m;
		int	i1 = ((i + 1) % n) * m;
		for (int j = 0; j < m; j++) {
			int		j1 = (j + 1) % m;
			*x++	= i0 + j;
			*x++	= i1 + j;
			*x++	= i0 + j1;

			*x++	= i0 + j1;
			*x++	= i1 + j;
			*x++	= i1 + j1;
		}
	}
}

struct TorusVB : public VertexIndexBuffer {
	TorusVB(int n, int m, float r_outer = 1, float r_inner = 0.5f) {
		TorusVerts(*this, n, m, r_outer, r_inner);
		vb.End();
		ib.End();
	}

	template<int N, int M> static const VertexIndexBuffer&	get() {
		static TorusVB	vib(N, M);
		return vib;
	}
};

template<typename A> void CylinderVerts(A &a, int n, float r = 1, float h = 1, bool ends = true) {
	auto	*v = a.BeginVerts(n * (ends ? 4 : 2));
	for (int i = 0; i < n; i++) {
		float2	t = sincos(two * pi * i / n);
		v++->set(concat(t * r, -h), concat(t, 0));
		v++->set(concat(t * r, +h), concat(t, 0));
	}

	uint16	*x = a.BeginPrims(n * 2 + (ends ? (n - 2) * 2 : 0));
	for (int i = 0; i < n; i++) {
		int		j = i * 2, k = (i + 1) % n * 2;

		*x++	= j;
		*x++	= k;
		*x++	= j + 1;

		*x++	= k;
		*x++	= k + 1;
		*x++	= j + 1;
	}
	if (ends) {
		for (int i = 0; i < n; i++) {
			float2	t = sincos(two * pi * i / n);
			v++->set(concat(t * r, -h), float3{0, 0, -1});
			v++->set(concat(t * r, +h), float3{0, 0, +1});
		}
		int	o = n * 2;
		for (int i = 1; i < n - 1; i++) {
			*x++ = o;
			*x++ = o + (i + 1) * 2;
			*x++ = o + i * 2;
		}
		for (int i = 1; i < n - 1; i++) {
			*x++ = o + 1;
			*x++ = o + i * 2 + 1;
			*x++ = o + (i + 1) * 2 + 1;
		}
	}
}

struct CylinderVB : public VertexIndexBuffer {
	CylinderVB(int n, float r = 1, float h = 1, bool ends = true) {
		CylinderVerts(*this, n, r, h, ends);
		vb.End();
		ib.End();
	}
	template<int N> static const CylinderVB&	get() {
		static CylinderVB	vib(N);
		return vib;
	}
};


template<typename A> void ConeVerts(A &a, int n, float r = 1, float h = 1, bool bottom = true) {
	float2	z		= normalise(float2{-r, h});

	auto	*v = a.BeginVerts(1 + n * (bottom ? 2 : 1));
	v++->set(float3{0, 0, +h}, float3{0,0,0});

	for (int i = 0; i < n; i++) {
		float2	t = sincos(two * pi * i / n);
		v++->set(concat(t * r, -h), concat(t * z.y, -z.x));
	}

	uint16	*x = a.BeginPrims(n + (bottom ? n - 2 : 0));
	for (int i = 0; i < n; i++) {
		*x++	= ((i + 1) % n) + 1;
		*x++	= 0;
		*x++	= i + 1;
	}
	if (bottom) {
		for (int i = 0; i < n; i++) {
			float2	t = sincos(two * pi * i / n);
			v++->set(concat(t * r, -h), float3{0, 0, -1});
		}
		int	o = n + 1;
		for (int i = 1; i < n - 1; i++) {
			*x++ = o;
			*x++ = o + i + 1;
			*x++ = o + i;
		}
	}
}

struct ConeVB : public VertexIndexBuffer {
	ConeVB(int n, float r = 1, float h = 1, bool bottom = true) {
		ConeVerts(*this, n, r, h, bottom);
		vb.End();
		ib.End();
	}
	template<int N> static const ConeVB&	get() {
		static ConeVB	vib(N);
		return vib;
	}
};

template<typename A> void BoxVerts(A &a, param(float3) dims = float3(one)) {
	auto	*v = a.BeginVerts(4 * 6);
	uint16	*x = a.BeginPrims(6 * 2);
	for (int f = 0; f < 6; f++) {
		float	d = f & 1 ? 1 : -1;
		int		s = f >> 1;
		float3	n = rotate(float3{zero, zero, d}, -s);
		float2	p = float2{d, one};
		for (int i = 0; i < 4; i++)
			v++->set(rotate(concat(select(i, p, -p), d), -s) * dims, n);
		int	o = f * 4;
		*x++ = o + 0;
		*x++ = o + 1;
		*x++ = o + 2;
		*x++ = o + 2;
		*x++ = o + 1;
		*x++ = o + 3;
	}
}

struct BoxVB : public VertexIndexBuffer {
	BoxVB(param(float3) dims = float3(one)) {
		BoxVerts(*this, dims);
		vb.End();
		ib.End();
	}
	static const BoxVB&	get() {
		static BoxVB	vib;
		return vib;
	}
};

template<typename A> void TetrahedronVerts(A &a, param(float3x4) m = identity) {
	static uint8 indices[] = {2,1,0, 2,3,1, 2,0,3, 1,3,0};
	position3	pos[4] = {
		position3(m.w),
		position3(m.w + m.x),
		position3(m.w + m.y),
		position3(m.w + m.z)
	};
	auto	*v = a.BeginVerts(3 * 4);
	uint16	*x = a.BeginPrims(4);
	for (int f = 0; f < 4; f++) {
		int		o	= f * 3;
		auto	*i	= indices + o;
		float3	n	= normalise(cross(pos[i[1]] - pos[i[0]], pos[i[2]] - pos[i[1]]));
		for (int j = 0; j < 3; j++)
			v++->set(pos[i[j]].v, n);
		*x++ = o + 0;
		*x++ = o + 1;
		*x++ = o + 2;
	}
}

struct TetrahedronVB : public VertexIndexBuffer {
	TetrahedronVB(param(float3x4) m = identity) {
		TetrahedronVerts(*this, m);
		vb.End();
		ib.End();
	}
	static const TetrahedronVB&	get() {
		static TetrahedronVB	vib;
		return vib;
	}
};

//-----------------------------------------------------------------------------
// Grid
//-----------------------------------------------------------------------------

void MakeGridVerts(stride_iterator<float2p> p, int nx, int ny, float sx, float sy);
void MakeGridIndices(uint16 *i, int nx, int ny);
void MakeGrid(stride_iterator<float2p> p, uint16 *i, int nx, int ny, float sx, float sy);

struct _Grid : static_list<_Grid>, refs<_Grid> {
	int						nx, ny;
	uint32					nv, np;
	VertexBuffer<float2p>	vb;
	IndexBuffer<uint16>		ib;

	_Grid(int nx, int ny) : nx(nx), ny(ny), nv((nx + 1) * (ny + 1)), np(((nx + 1) * 2 + 1) * ny) {
		MakeGridVerts(vb.Begin(nv), nx, ny, 1.f / nx,  1.f / ny);
		vb.End();

		MakeGridIndices(ib.Begin(np), nx, ny);
		ib.End();
	}

	void Render(GraphicsContext &ctx) const {
		ctx.SetIndices(ib);
		ctx.SetVertices(vb);
		ctx.DrawIndexedPrimitive(PRIM_TRISTRIP, 0, nv, 0, np);
	}

	static _Grid* Get(int nx, int ny) {
		for (_Grid::iterator i = _Grid::begin(), e = _Grid::end(); i != e; ++i) {
			if (i->nx == nx && i->ny == ny)
				return i;
		}
		return new _Grid(nx, ny);
	}
};

struct Grid {
	ref_ptr<_Grid> grid;

	Grid(int nx, int ny) {
		for (_Grid::iterator i = _Grid::begin(), e = _Grid::end(); i != e; ++i) {
			if (i->nx == nx && i->ny == ny) {
				grid = i;
				return;
			}
		}
		grid = new _Grid(nx, ny);
	}
	void Render(GraphicsContext &ctx) const {
		grid->Render(ctx);
	}
};

struct _LineGrid : static_list<_LineGrid>, refs<_LineGrid> {
	int						nx, ny;
	uint32					nv, np;
	VertexBuffer<float2p>	vb;
	IndexBuffer<uint16>		ib;

	_LineGrid(int _nx, int _ny) : nx(_nx), ny(_ny), nv(((nx + 1) + (ny + 1)) * 2), np((nx + 1) + (ny + 1)) {
		float2p	*p = vb.Begin(nv);
		float	sx = 1.f / nx, sy = 1.f / ny;
		for (int x = 0; x <= nx; x++) {
			*p++ = float2{x * sx, 0};
			*p++ = float2{x * sx, 1};
		}
		for (int y = 0; y <= ny; y++) {
			*p++ = float2{0, y * sy};
			*p++ = float2{1, y * sy};
		}
		vb.End();

		uint16	*i	= ib.Begin(np);
		for (int x = 0; x <= nx; x++) {
			*i++ = x * 2 + 0;
			*i++ = x * 2 + 1;
		}
		for (int y = 0; y <= ny; y++) {
			*i++ = (nx + 1 + y) * 2 + 0;
			*i++ = (nx + 1 + y) * 2 + 1;
		}
		ib.End();
	}
	void Render(GraphicsContext &ctx) const {
		ctx.SetIndices(ib);
		ctx.SetVertices(vb);
		ctx.DrawIndexedPrimitive(PRIM_LINELIST, 0, nv, 0, np);
	}

	static _LineGrid* Get(int nx, int ny) {
		for (_LineGrid::iterator i = _LineGrid::begin(), e = _LineGrid::end(); i != e; ++i) {
			if (i->nx == nx && i->ny == ny)
				return i;
		}
		return new _LineGrid(nx, ny);
	}
};

}//namespace iso
