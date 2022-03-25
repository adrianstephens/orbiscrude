#ifndef GRAPHICS_COMMON_H
#define GRAPHICS_COMMON_H

#include "base/defs.h"

namespace iso {
	//defined  by platform
	struct VertexElement;

	struct VertexElements {
		const VertexElement	*p;
		size_t				n;
		VertexElements(const VertexElement *p, size_t n) : p(p), n(n) {}
		template<size_t N> constexpr VertexElements(const meta::array<VertexElement, N> &a) : p(a), n(N) {}
		template<size_t N> constexpr VertexElements(const VertexElement (&a)[N]) : p(a), n(N) {}
	};

	template<typename T, typename V=void> struct GetVE_s;
	template<typename T> VertexElements	GetVE()				{ return GetVE_s<T>::ve; }
	template<typename T> VertexElements	GetVE(const T &t)	{ return GetVE<T>(); }
	template<typename T> struct TextureT;
	template<typename T> struct SurfaceT;

	struct TriList;

	struct PointList;
	struct LineList;
	struct LineStrip;
	struct TriList;
	struct TriStrip;
	struct QuadList;
	struct RectList;
	struct TriFan;
}

#include "_graphics.h"
#include "base/block.h"

namespace iso {

class CONCAT2(ISO_PREFIX,Shader);
typedef CONCAT2(ISO_PREFIX,Shader) pass;

//-----------------------------------------------------------------------------
//	enum ops
//-----------------------------------------------------------------------------

inline BackFaceCull operator~(BackFaceCull b)				{ return b == BFC_NONE ? b : BackFaceCull(BFC_FRONT + BFC_BACK - b); }
inline BackFaceCull reverse(BackFaceCull c)					{ return ~c; }
inline BackFaceCull reverse(BackFaceCull c, bool reverse)	{ return reverse ? ~c : c; }

inline UVMode		operator|(UVMode a, UVMode b)			{ return UVMode(int(a) | int(b)); }

inline ChannelMask	operator|(ChannelMask a, ChannelMask b) { return ChannelMask(int(a) | int(b)); }
inline ChannelMask	operator~(ChannelMask a)				{ return ChannelMask(int(a) ^ CM_ALL); }
inline ChannelMask	operator&(ChannelMask a, ChannelMask b)	{ return ChannelMask(int(a) & int(b)); }

//-----------------------------------------------------------------------------
//	VertexElements
//-----------------------------------------------------------------------------

template<typename T> VertexElement MakeVE(int offset, const char *usage, int stream = 0) {
	return VertexElement(offset, GetComponentType<T>(), usage, stream);
}
template<typename T, int N> struct VE_array { static VertexElement ve[1]; };
template<typename T, int N> VertexElement VE_array<T, N>::ve[1] = { VertexElement(0, GetComponentType<T[N]>(), 1) };

template<typename T> struct GetVE_s<T, enable_if_t<(num_elements_v<T> > 1)>> : VE_array<element_type<T>, num_elements_v<T>> {};

//-----------------------------------------------------------------------------
//	typed Surface/Texture/DataBuffer/ConstBuffer
//-----------------------------------------------------------------------------

template<typename T> struct SurfaceT : Surface {
	SurfaceT() {}
	SurfaceT(int width, int height, Memory loc = MEM_DEFAULT) : Surface(GetTexFormat<T>(), width, height, loc) {}
	SurfaceT(const point &size, Memory loc = MEM_DEFAULT) : Surface(GetTexFormat<T>(), size.x, size.y, loc) {}
	template<typename...P> auto	Data(P&&...p)	const	{ return Surface::Data<T>(forward<P>(p)...); }
};

template<typename T> struct TextureT : Texture {
	bool	Init(int width, int height, int depth, int mips, Memory flags = MEM_DEFAULT) {
		return Texture::Init(GetTexFormat<T>(), width, height, depth, mips, flags);
	}
	bool	Init(int width, int height, int mips, Memory flags = MEM_DEFAULT) {
		return Init(width, height, 1, mips, flags);
	}
	bool	Init(int width, int height, Memory flags = MEM_DEFAULT) {
		return Init(width, height, 1, 1, flags);
	}
	bool	Init(int width, int height, int depth, int mips, Memory flags, T *data, int pitch) {
		return Texture::Init(GetTexFormat<T>(), width, height, depth, mips, flags, data, pitch);
	}
	bool	Init(const block<T, 2> &block, Memory flags = MEM_DEFAULT) {
		return Init(block.template size<1>(), block.template size<2>(), 1, 1, flags, block, block.pitch());
	}
	bool	Init(const block<T, 3> &block, Memory flags = MEM_DEFAULT) {
		return Init(block.template size<1>(), block.template size<2>(), block.template size<3>(), 1, flags, block, block.pitch());
	}

	TextureT()		{}
	TextureT(int width, int height, int depth, int mips, Memory flags = MEM_DEFAULT) {
		Init(width, height, depth, mips, flags);
	}
	TextureT(int width, int height, int mips, Memory flags = MEM_DEFAULT) {
		Init(width, height, 1, mips, flags);
	}
	TextureT(int width, int height, Memory flags = MEM_DEFAULT) {
		Init(width, height, 1, 1, flags);
	}
	TextureT(int width, int height, int depth, int mips, Memory flags, T *data, int pitch) {
		Init(width, height, depth, mips, flags, data, pitch);
	}
	TextureT(int width, int height, int mips, Memory flags, T *data, int pitch) {
		Init(width, height, 1, mips, flags, data, pitch);
	}
	TextureT(int width, int height, Memory flags, T *data, int pitch) {
		Init(width, height, 1, 1, flags, data, pitch);
	}
	TextureT(const block<T, 2> &block, Memory flags = MEM_DEFAULT) {
		Init(block, flags);
	}
	TextureT(const block<T, 3> &block, Memory flags = MEM_DEFAULT) {
		Init(block, flags);
	}
	TextureT(const TextureT &t)				= default;
	TextureT& operator=(const TextureT &t)	= default;
	TextureT(TextureT &&t)					= default;
	TextureT& operator=(TextureT &&t)		= default;

	template<typename...P> auto	Data(P&&...p)		const	{ return Texture::Data<T>(forward<P>(p)...); }
	template<typename...P> auto	WriteData(P&&...p)	const	{ return Texture::WriteData<T>(forward<P>(p)...); }
};

template<typename T> TextureT<T> make_texture(const block<T, 2> &block, Memory flags = MEM_DEFAULT) {
	return TextureT<T>(block, flags);
}

#ifdef ISO_HAS_GRAHICSBUFFERS

template<typename T> struct DataBufferT : DataBuffer {
	bool	Init(int width, Memory flags = MEM_DEFAULT) {
		return DataBuffer::Init(GetTexFormat<T>(), width, flags);
	}
	bool	Init(int width, Memory flags, T *data) {
		return DataBuffer::Init(GetTexFormat<T>(), width, flags, data);
	}
	DataBufferT()		{}
	DataBufferT(int width, Memory flags = MEM_DEFAULT) {
		Init(width, flags);
	}
	DataBufferT(int width, Memory flags, T *data) {
		Init(width, flags, data);
	}
	DataBufferT(const block<T, 1> &block, Memory flags = MEM_DEFAULT) {
		Init(block.size(), flags, block);
	}
	template<typename...P> auto	Data(P&&...p)	const	{ return DataBuffer::Data<T>(forward<P>(p)...); }
};

template<typename T> DataBufferT<T> make_buffer(const block<T, 1> &block, Memory flags = MEM_DEFAULT) {
	return DataBufferT<T>(block, flags);
}

template<typename T> struct ConstBufferT : ConstBuffer {
	ConstBufferT() : ConstBuffer(sizeof(T)) {}
	T*	operator->()	{ return (T*)Data(); }
	T&	operator*()		{ return *(T*)Data(); }
	friend T& get(ConstBufferT &c)	{ return c; }
};

#endif

//-----------------------------------------------------------------------------
//	Prim
//-----------------------------------------------------------------------------

template<typename T, int V, PrimType P> struct PrimT {
	static const int		verts	= V;
	static const PrimType	prim	= P;
	T		*t;
	PrimT(T *_t) : t(_t)			{}
	T*		next()				{ return t + verts; }
	PrimT&	operator++()		{ t += verts; return *this; }
	T&		operator[](int i)	{ return t[i]; }
	friend bool	operator!=(const PrimT &a, const PrimT &b)	{ a.t != b.t; }
	template<typename S> void operator=(const S &s)	{ copy(s.corners(), t); }
};

#ifdef ISO_HAS_QUADS

template<typename T> struct QuadListT : PrimT<T, 4, PRIM_QUADLIST> {
	T	&operator[](int i)	{ return this->t[i ^ ((i & 2) ? 0 : 1)]; }
	QuadListT(T *_t) : Prim<T, 4, PRIM_QUADLIST>(_t)	{}
};
template<typename T> struct QuadStripT : PrimT<T, 4, PRIM_QUADSTRIP> {
	QuadStripT(T *_t) : Prim<T, 4, PRIM_QUADSTRIP>(_t)	{}
};

#else

template<typename T> struct QuadListT : PrimT<T, 6, PRIM_TRILIST> {
	QuadListT(T *_t) : PrimT<T, 6, PRIM_TRILIST>(_t)		{}
	~QuadListT()				{ next(); }
	T*			next()			{ this->t[4] = this->t[2]; this->t[5] = this->t[1]; return this->t + 6; }
	QuadList&	operator++()	{ this->t = next(); }
	template<typename S> void operator=(const S &s)	{ copy(s.corners(), this->t); }
};

template<typename T> struct QuadStripT : PrimT<T, 4, PRIM_TRISTRIP> {
	QuadStripT(T *_t) : PrimT<T, 4, PRIM_TRISTRIP>(_t)	{}
};

#endif

template<typename T> struct SingleQuadT : QuadStripT<T> {
	SingleQuadT(T *_t) : QuadStripT<T>(_t)		{}
	template<typename S> void operator=(const S &s)	{ copy(s.corners(), this->t); }
};

#ifdef ISO_HAS_RECTS

template<typename T> struct RectListT : PrimT<T, 3, PRIM_RECTLIST> {
	T	&operator[](int i)	{ return this->t[i]; }
	RectListT(T *_t) : Prim<T, 3, PRIM_RECTLIST>(_t)	{}
};

#else

template<typename T> struct RectListT : QuadListT<T> {
	RectListT(T *_t) : QuadListT<T>(_t)		{}
	~RectListT()				{ next(); }
	T*			next()			{ (*this)[3] = (*this)[2] + ((*this)[1] - (*this)[0]); return QuadListT<T>::next(); }
	RectListT&	operator++()	{ this->t = next(); }
};

#endif

template<template<typename> class T> PrimType prim() {
	return PrimType(T<int>::prim);
}
template<template<typename> class T> int verts(int n = 1) {
	return T<int>::verts * n;
}

template<class T> class ImmediatePrims;

template<template<typename> class P, class T> class ImmediatePrims<P<T> > : public ImmediateStream<T> {
public:
	ImmediatePrims(GraphicsContext &ctx, int count = 1) : ImmediateStream<T>(ctx, PrimType(P<T>::prim), P<T>::verts * count) {}
	P<T>	begin()			{ return ImmediateStream<T>::begin(); }
	P<T>	end()			{ return ImmediateStream<T>::end(); }
};

struct GraphicsBlock {
	GraphicsContext &ctx;
	GraphicsBlock(GraphicsContext &ctx, const char *label) : ctx(ctx)	{ ctx.PushMarker(label); }
	~GraphicsBlock()				{ ctx.PopMarker(); }
	void Next(const char *label)	{ ctx.PopMarker(); ctx.PushMarker(label); }
};

} // namespace iso

#endif
