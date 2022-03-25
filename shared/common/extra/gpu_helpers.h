#ifndef GPU_HELPERS_H
#define GPU_HELPERS_H

#include "base/defs.h"
#include "base/vector.h"
#include "base/algorithm.h"

namespace iso {

//-----------------------------------------------------------------------------
//	prims
//-----------------------------------------------------------------------------

struct _indices {
	const void	*p;
	uint32	size;
	uint32	offset;

	_indices(const void *_p, uint32 _size, uint32 _offset = 0) : p(_p), size(_size), offset(_offset) {}
	void*				addr(int i)			const	{ return !p ? 0 : size == 2 ? (void*)((uint16*)p + i) : (void*)((uint32*)p + i); }
	uint32				operator[](int i)	const	{ return (p ? (size == 2 ? ((uint16*)p)[i] : ((uint32*)p)[i]) : i) + offset; }
	friend constexpr ptrdiff_t	operator-(const _indices &a, const _indices &b) { return a.size ? ((uint8*)a.p - (uint8*)b.p) / a.size : (uint8*)a.p - (uint8*)b.p; }
};

struct indices : _indices {
	uint32	num;

	struct iterator : _indices {
		typedef random_access_iterator_t	iterator_category;
		typedef uint32						element, reference;
		iterator(const void *p, uint32 size, uint32 offset) : _indices(p, size, offset) {}
		bool		operator==(const iterator &b)	const	{ return p == b.p; }
		bool		operator!=(const iterator &b)	const	{ return p != b.p; }
		uint32		operator*()						const	{ return (size == 0 ? (uint32)(intptr_t)p : size == 2 ? *(uint16*)p : *(uint32*)p) + offset; }
		iterator&	operator++()							{ p = (char*)p + (size ? size : 1); return *this; }
		iterator&	operator+=(int i)						{ p = (char*)p + (size ? size * i : i); return *this; }
		iterator	operator+(int i)				const	{ return iterator((char*)p + (size ? size * i : i), size, offset); }
	};
	typedef iterator	const_iterator;
	typedef uint32		element, reference;

	indices(uint32 _num = 0)	: _indices(0, 0, 0), num(_num) {}
	indices(const void *p, uint32 size, uint32 offset, uint32 num) : _indices(p, size, offset), num(num) {}
	template<typename T> indices(const T *p, uint32 offset, uint32 num) : _indices(p, sizeof(T), offset), num(num) {}
	template<typename T> indices(const dynamic_array<T> &p, uint32 offset = 0) : _indices(p.begin(), sizeof(T), offset), num(p.size32()) {}
	iterator		begin()		const	{ return iterator(p, size, offset); }
	iterator		end()		const	{ return iterator((char*)p + (size ? size * num : num), size, offset); }
	uint32			size32()	const	{ return num; }
	uint32			max_index()	const	{ return p ? reduce<op_max>(begin(), end()) : num + offset - 1; }

	indices			operator+(int n) const {
		return indices(addr(n), size, offset + (p ? 0 : n), num - n);
	}

	explicit operator bool()	const	{ return !!p; }
};
//-----------------------------------------------------------------------------
//	prims
//-----------------------------------------------------------------------------

struct Prim2Vert {
	uint8	mul, add:5, flip:1, adj:1, rect:1;

	constexpr Prim2Vert(uint8 mul = 1, uint8 add = 0, bool flip = false, bool adj = false, bool rect = false) : mul(mul), add(add), flip(flip), adj(adj), rect(rect) {}
	uint32	prims_to_verts(uint32 n)	const { return n * mul + add + adj; }
	uint32	verts_to_prims(uint32 n)	const { uint8 a = add + adj; return (max(n, a) - a) / mul; }
	uint32	verts_per_prim()			const { return mul + add + adj; }
	uint32	first_vert(uint32 p)		const { return p * mul + adj; }
	uint32	first_prim(uint32 v)		const { uint8 a = add + adj; return (max(v, a) - a) / mul; }
	uint32	last_prim(uint32 v)			const { return (max(v, adj) - adj) / mul; }
	bool	winding(int p)				const { return !!(p & flip); }

	static Prim2Vert trianglelist()		{ return Prim2Vert(3,0,0); }
	static Prim2Vert trianglestrip()	{ return Prim2Vert(1,2,1); }
	static Prim2Vert quadlist()			{ return Prim2Vert(4,0,0); }
	static Prim2Vert quadstrip()		{ return Prim2Vert(2,2,0); }
};

template<typename I> struct prim_iterator {
	Prim2Vert	p2v;
	I			i;
	int			index;

	struct prim {
		typedef typename iterator_traits<I>::reference reference;
		struct iterator {
			prim		&p;
			int			i;
			iterator(prim &p, int i) : p(p), i(i)			{}
			iterator&	operator++()						{ ++i; return *this;}
			bool		operator==(const iterator &b)		{ return i == b.i;}
			bool		operator!=(const iterator &b)		{ return i != b.i;}
			reference	operator*()							{ return p[i]; }
			reference	operator[](int j)					{ return p[i + j]; }
		};
		struct const_iterator {
			const prim	&p;
			int			i;
			const_iterator(const prim &p, int i) : p(p), i(i)	{}
			const_iterator&	operator++()					{ ++i; return *this;}
			bool		operator==(const const_iterator &b)	{ return i == b.i;}
			bool		operator!=(const const_iterator &b)	{ return i != b.i;}
			const reference	operator*()						{ return p[i]; }
			const reference	operator[](int j)				{ return p[i + j]; }
		};
		I			i;
		Prim2Vert	p2v;
		int			exor, add;
		prim(const I &i, Prim2Vert	p2v, bool flip) : i(i), p2v(p2v), exor(flip ? 3 : 0), add(flip ? -1 : 0) {}
		reference		operator[](int j)		{ return i[(j ^ exor) + add]; }
		const reference operator[](int j) const { return i[(j ^ exor) + add]; }
		uint32			size()	const			{ return p2v.verts_per_prim(); }
		const_iterator	begin()	const			{ return const_iterator(*this, 0); }
		const_iterator	end()	const			{ return const_iterator(*this, p2v.verts_per_prim()); }
		const reference	front()	const			{ return (*this)[0]; }
		const reference	back()	const			{ return (*this)[p2v.verts_per_prim() - 1]; }
		iterator		begin()					{ return iterator(*this, 0); }
		iterator		end()					{ return iterator(*this, p2v.verts_per_prim()); }
		bool			is_rect() const			{ return p2v.rect; }
	};

	typedef random_access_iterator_t	iterator_category;
	typedef prim						element, reference;

	prim_iterator(const Prim2Vert p2v, const I &i) : p2v(p2v), i(i), index(0) {}
	prim_iterator&	operator++()							{ ++index; return *this; }
	prim_iterator&	operator+=(intptr_t i)					{ index += int(i); return *this; }
	prim_iterator	operator+(intptr_t i)			const	{ return prim_iterator(*this) += i; }
	intptr_t		operator-(const prim_iterator &b) const	{ return index - b.index; }
	prim			operator*()						const	{ return prim(i + p2v.first_vert(index), p2v, p2v.winding(index)); }
	ref_helper<prim> operator->()					const	{ return operator*(); }
	prim			operator[](intptr_t i)			const	{ return *(*this + i); }
	bool		operator==(const prim_iterator &b)	const	{ return index == b.index; }
	bool		operator!=(const prim_iterator &b)	const	{ return index != b.index; }
};

template<typename I> inline auto make_prim_iterator(const Prim2Vert p2v, const I &i) {
	return prim_iterator<I>(p2v, i);
}

template<typename I> inline auto make_prim_container(const Prim2Vert p2v, const I &i) {
	return make_range_n(make_prim_iterator(p2v, i.begin()), p2v.verts_to_prims(i.size32()));
}

template<typename D, typename I> D *convex_to_tris(D *d, I a, I b) {
	if (b != a) {
		--b;
		while (a + 1 != b) {
			(*d)[0] = *a;
			(*d)[1] = *++a;
			(*d)[2] = *b;
			d++;

			if (a + 1 == b)
				break;

			(*d)[0] = *b;
			(*d)[1] = *a;
			(*d)[2] = *--b;
			d++;
		}
	}
	return d;
}

template<typename D, typename C> D *convex_to_tris(D *d, C&& c) {
	return convex_to_tris(d, begin(c), end(c));
}

template<typename D, typename I> D *convex_to_tristrip(D *d, I a, I b) {
	if (b != a) {
		*d++ = *a;
		*d++ = *++a;

		while (a != --b) {
			*d++ = *b;

			if (++a == b)
				break;

			*d++ = *a;
		}
	}
	return d;
}

template<typename D, typename C> D *convex_to_tristrip(D *d, C&& c) {
	return convex_to_tristrip(d, begin(c), end(c));
}

//-----------------------------------------------------------------------------
//	Topology
//-----------------------------------------------------------------------------

struct Topology {
	enum Type : uint8 {	//	mul	add	num
		UNKNOWN,		//	0	0	0
		POINTLIST,		//	1	0	1
		LINELIST,		//	2	0	2
		LINESTRIP,		//	1	1	2
		LINELOOP,		//	1	0	2
		TRILIST,		//	3	0	3
		TRISTRIP,		//	1	2	3
		TRIFAN,			//	1	2	3
		RECTLIST,		//	3	0	3
		QUADLIST,		//	4	0	4
		QUADSTRIP,		//	2	2	4
		POLYGON,		//	n	0	n
		LINELIST_ADJ,	//	4	0	4
		LINESTRIP_ADJ,	//	2	2	4
		TRILIST_ADJ,	//	6	0	6
		TRISTRIP_ADJ,	//	2	2	6
		PATCH,			//	n	0	n
	} type;
	Prim2Vert	p2v;

	Topology() : type(UNKNOWN) {}
	Topology(Type type) : type(type) {
		static const Prim2Vert p2v_table[] = {
		//	mul		add	flp adj rect
			{0,		0,	0,	0,  0},	//UNKNOWN,
			{1,		0,	0,	0,  0},	//POINTLIST,
			{2,		0,	0,	0,  0},	//LINELIST,
			{1,		1,	0,	0,  0},	//LINESTRIP,
			{1,		0,	0,	0,  0},	//LINELOOP,
			{3,		0,	0,	0,  0},	//TRILIST,
			{1,		2,	1,	0,  0},	//TRISTRIP,
			{1,		2,	0,	0,  0},	//TRIFAN,
			{3,		0,	0,	0,  1},	//RECTLIST,
			{4,		0,	0,	0,  0},	//QUADLIST,
			{2,		2,	0,	0,  0},	//QUADSTRIP,
			{0xff,	0,	0,	0,  0},	//POLYGON,
			{4,		0,	0,	0,  0},	//LINELIST_ADJ,
			{1,		2,	0,	1,  0},	//LINESTRIP_ADJ,
			{6,		0,	0,	0,  0},	//TRILIST_ADJ,
			{2,		2,	0,	0,  0},	//TRISTRIP_ADJ,
			{0xff,	0,	0,	0,  0},	//PATCH,
		};

		p2v		= p2v_table[type];
	}

	void	SetNumCP(int n)					{ p2v.mul = n; }
	int		VertsPerPrim()			const	{ return p2v.verts_per_prim(); }
	int		PrimFromVertex(int v)	const	{ return p2v.last_prim(v); }
	int		VertexFromPrim(int p)	const	{ return p2v.first_vert(p); }
	bool	Winding(int p)			const	{ return p2v.winding(p); }
	explicit operator bool()		const	{ return type != UNKNOWN; }
};

template<typename T> T *Quadify(T *p) {
	p[1]	= p[-1];
	p[0]	= p[-2];
	p[-1]	= p[-4];
	return p + 2;
}

template<typename T> T *Rectify(T *p) {
	p[0]	= p[-1];
	p[1]	= p[-2];
	p[2]	= p[-2];
	p[2].y	= p[-1].y;
	return p + 3;
}

inline uint32 *Rectify(uint32 *p) {
	p[0]	= p[-1];
	p[1]	= p[-2];
	p[2]	= p[-2];
	return p + 3;
}

struct Topology2 : Topology {
	struct hw_conv {uint8 type:4, num:4; };
	static hw_conv get_hw(Type type) {
		static const hw_conv table[] = {
			{UNKNOWN,		1},	//UNKNOWN,
			{POINTLIST,		1},	//POINTLIST,
			{LINELIST,		1},	//LINELIST,
			{LINESTRIP,		1},	//LINESTRIP,
			{LINELOOP,		1},	//LINELOOP,
			{TRILIST,		1},	//TRILIST,
			{TRISTRIP,		1},	//TRISTRIP,
			{TRIFAN,		1},	//TRIFAN,
			{TRILIST,		2},	//RECTLIST,
			{TRILIST,		2},	//QUADLIST,
			{TRISTRIP,		2},	//QUADSTRIP,
			{TRIFAN,		0},	//POLYGON,
			{LINELIST,		1},	//LINELIST_ADJ,
			{LINESTRIP,		1},	//LINESTRIP_ADJ,
			{TRILIST,		1},	//TRILIST_ADJ,
			{TRISTRIP,		1},	//TRISTRIP_ADJ,
			{POINTLIST,		0},	//PATCH,
		};
		return table[type];
	}

	Topology	hw;
	uint8		hw_mul;
	uint32		chunks;

	Topology2() {}
	Topology2(Type type, uint32 chunks = 0) : Topology(type), hw((Type)get_hw(type).type), hw_mul(get_hw(type).num), chunks(chunks) {}

	void	SetNumCP(int n)							{ hw_mul = p2v.mul = n; }

	int		FirstPrimFromVertex(int v, bool _hw)	const	{ return (_hw ? hw.p2v : p2v).first_prim(v); }
	int		LastPrimFromVertex(int v, bool _hw)		const	{ return (_hw ? hw.p2v : p2v).last_prim(v); }
	int		PrimFromVertex(int v, bool _hw)			const	{ return (_hw ? hw.p2v : p2v).last_prim(v); }
	int		VertexFromPrim(int p, bool _hw)			const	{ return (_hw ? hw.p2v : p2v).first_vert(p); }
	bool	Winding(int p)							const	{ return hw.p2v.winding(p); }
	int		NumHWVertices(int nv)					const	{ return hw.p2v.prims_to_verts(p2v.verts_to_prims(nv) * hw_mul); }

	int		PrimFromVertexChunks(int v, bool _hw) const	{
		if (chunks) {
			int		ppc = p2v.verts_to_prims(chunks);
			return (v / chunks) * ppc + clamp(PrimFromVertex(v % chunks, _hw), 0, ppc - 1);
		}
		return PrimFromVertex(v, _hw);
	}
	int		VertexFromPrimChunks(int p, bool _hw)	const {
		if (chunks) {
			int		ppc = p2v.verts_to_prims(chunks);
			return  (p / ppc) * chunks + VertexFromPrim(p % ppc, _hw);
		}
		return VertexFromPrim(p, _hw);
	}

	int		FromHWOffset(int offset) const {
		static const uint8 quad_offsets[] = {0,1,2,0,2,3};
		static const uint8 rect_offsets[] = {0,1,2,2,1,1};
		switch (type) {
			case QUADLIST:
				return quad_offsets[offset];
			case RECTLIST:
				return rect_offsets[offset];
//			case LINESTRIP_ADJ:
//				return offset + 1;
			case LINELIST_ADJ: case TRILIST_ADJ: case TRISTRIP_ADJ:
				return offset * 2;
			default:
				return offset;
		}
	}
	int		ToHWOffset(int offset) const {
		static const uint8 quad_offsets[] = {0,1,2,5};
		switch (type) {
			case QUADLIST:
				return quad_offsets[offset];
			//case LINESTRIP_ADJ:
			//	return offset - 1;
			case LINELIST_ADJ: case TRILIST_ADJ: case TRISTRIP_ADJ:
				return offset / 2;
			default:
				return offset;
		}
	}
	template<typename T> T *Adjust(T *p, int index) const {
		switch (type) {
			case QUADLIST:
				if ((index % 4) == 3)
					return Quadify(p + 1);
				break;
			case RECTLIST:
				if ((index % 3) == 2)
					return Rectify(p + 1);
				break;
			case LINESTRIP_ADJ:
				if (index == 0)
					return p;
				break;
			case LINELIST_ADJ:
			case TRILIST_ADJ:
			case TRISTRIP_ADJ:
				if ((index % 2) == 1)
					return p;
				break;
		}
		return ++p;
	}
};

//-----------------------------------------------------------------------------
//	Tesselation
//-----------------------------------------------------------------------------

struct Tesselation {
	enum {MAX = 64};
	enum Spacing {EQUAL, FRACT_EVEN, FRACT_ODD};

	dynamic_array<uint16>	indices;
	dynamic_array<float2p>	uvs;

	template<typename T> static auto	effective(Spacing spacing, T x) {
		switch (spacing) {
			default:
			case EQUAL:			return to<int>(ceil(clamp(x, T(1), T(MAX))));
			case FRACT_EVEN:	return to<int>(ceil(clamp(x, T(2), T(MAX)) / 2) * 2);
			case FRACT_ODD:		return to<int>(ceil(clamp(x + 1, T(2), T(MAX)) / 2) * 2 - 1);
		}
	}
	static int	effective_min(Spacing spacing) {
		return spacing == FRACT_ODD ? 3 : 2;
	}

	Tesselation()	{}
	Tesselation(param(float4) edges, param(float2) inside, Spacing spacing = EQUAL);	//quad
	Tesselation(param(float3) edges, float inside, Spacing spacing = EQUAL);			//tri
	Tesselation(param(float2) edges, Spacing spacing = EQUAL);							//isoline
};

} // namespace iso
#endif //GPU_HELPERS_H
