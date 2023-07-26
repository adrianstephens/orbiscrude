#ifndef GRAPHICS_COMMON_H
#define GRAPHICS_COMMON_H

#include "base/defs.h"

namespace iso {
	//defined  by platform
	struct VertexElement;

	struct VertexElements {
		const VertexElement	*p;
		size_t				n;
		VertexElements() : p(0), n(0) {}
		VertexElements(const VertexElement *p, size_t n) : p(p), n(n) {}
		template<size_t N> constexpr VertexElements(const meta::array<VertexElement, N> &a) : p(a), n(N) {}
		template<size_t N> constexpr VertexElements(const VertexElement (&a)[N]) : p(a), n(N) {}
	};

	template<typename T, typename V=void> extern const VertexElements ve;
	template<typename T> VertexElements	GetVE()				{ return ve<T>; }
	template<typename T> VertexElements	GetVE(const T &i)	{ return ve<T>; }

	template<typename T> struct TextureT;
	template<typename T> struct SurfaceT;

	struct PointList;
	struct LineList;
	struct LineStrip;
	struct TriList;
	struct TriStrip;
	struct QuadList;
	struct QuadStrip;
	struct RectList;
	struct TriFan;

	struct stride_t {
		uint32 v;
		explicit constexpr stride_t(uint32 v) : v(v)	{}
		constexpr operator uint32() const	{ return v; }
	};
	template<typename T> static constexpr stride_t strideof((uint32)sizeof(T));
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

template<typename T> constexpr VertexElement MakeVE(int offset, uint32 usage, int stream = 0) {
	return {offset, GetComponentType<T>(), usage, stream};
}
template<typename T> static const VertexElements ve<T, enable_if_t<(num_elements_v<T> > 1)>> = (VertexElement[1]) {
	{0, GetComponentType<element_type<T>[num_elements_v<T>]>(), 1}
};

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
		return Init(block.template size<1>(), block.template size<2>(), 1, 1, flags, block[0].begin(), block.pitch());
	}
	bool	Init(const block<T, 3> &block, Memory flags = MEM_DEFAULT) {
		return Init(block.template size<1>(), block.template size<2>(), block.template size<3>(), 1, flags, block[0].begin(), block.pitch());
	}

	TextureT()																				{}
	TextureT(int width, int height, int depth, int mips, Memory flags = MEM_DEFAULT)		{ Init(width, height, depth, mips, flags); }
	TextureT(int width, int height, int mips, Memory flags = MEM_DEFAULT)					{ Init(width, height, 1, mips, flags); }
	TextureT(int width, int height, Memory flags = MEM_DEFAULT)								{ Init(width, height, 1, 1, flags); }
	TextureT(int width, int height, int depth, int mips, Memory flags, T *data, int pitch)	{ Init(width, height, depth, mips, flags, data, pitch); }
	TextureT(int width, int height, int mips, Memory flags, T *data, int pitch)				{ Init(width, height, 1, mips, flags, data, pitch); }
	TextureT(int width, int height, Memory flags, T *data, int pitch)						{ Init(width, height, 1, 1, flags, data, pitch); }
	TextureT(const block<T, 2> &block, Memory flags = MEM_DEFAULT)							{ Init(block, flags); }
	TextureT(const block<T, 3> &block, Memory flags = MEM_DEFAULT)							{ Init(block, flags); }
	TextureT(const TextureT &i)				= default;
	TextureT& operator=(const TextureT &i)	= default;
	TextureT(TextureT &&i)					= default;
	TextureT& operator=(TextureT &&i)		= default;

	template<typename...P> auto	Data(P&&...p)		const	{ return Texture::Data<T>(forward<P>(p)...); }
	template<typename...P> auto	WriteData(P&&...p)	const	{ return Texture::WriteData<T>(forward<P>(p)...); }
};

template<typename T> TextureT<T> make_texture(const block<T, 2> &block, Memory flags = MEM_DEFAULT) {
	return TextureT<T>(block, flags);
}

#ifdef ISO_HAS_GRAHICSBUFFERS

template<typename T, bool = ValidTexFormat<T>()> struct DataBufferT : DataBuffer {
	bool	Init(int count, Memory flags = MEM_DEFAULT)					{ return DataBuffer::Init(GetTexFormat<T>(), count, flags); }
	bool	Init(const T *data, int count, Memory flags = MEM_DEFAULT)	{ return DataBuffer::Init(data, GetTexFormat<T>(), count, flags); }
	bool	Init(range<const T*> data, Memory flags = MEM_DEFAULT)		{ return Init(data.begin(), data.size32(), flags); }

	DataBufferT()		{}
	DataBufferT(int count, Memory flags = MEM_DEFAULT)					{ Init(count, flags); }
	DataBufferT(const T *data, int count, Memory flags = MEM_DEFAULT)	{ Init(data, count, flags); }
//	DataBufferT(const block<T, 1> &block, Memory flags = MEM_DEFAULT)	: DataBufferT(block, block.size(), flags)	{}
	DataBufferT(range<const T*> block, Memory flags = MEM_DEFAULT)		: DataBufferT(block.begin(), block.size(), flags) {}
	DataBufferT(initializer_list<T> c, Memory flags = MEM_DEFAULT)		: DataBufferT(c.begin(), c.size(), flags)	{}
	template<typename C, typename=enable_if_t<has_begin_v<C>>> DataBufferT(C &&c, Memory flags = MEM_DEFAULT) : DataBufferT(c.begin(), num_elements(c), flags) {}

	template<typename...P> auto	Data(P&&...p)		const	{ return DataBuffer::Data<T>(forward<P>(p)...); }
	template<typename...P> auto	WriteData(P&&...p)	const	{ return DataBuffer::WriteData<T>(forward<P>(p)...); }
};

template<typename T> struct DataBufferT<T, false> : DataBuffer {
	bool	Init(int count, Memory flags = MEM_DEFAULT)					{ return DataBuffer::Init(strideof<T>, count, flags); }
	bool	Init(const T *data, int count, Memory flags = MEM_DEFAULT)	{ return DataBuffer::Init(data, strideof<T>, count, flags); }
	bool	Init(range<const T*> data, Memory flags = MEM_DEFAULT)		{ return Init(data.begin(), data.size32(), flags); }

	DataBufferT()		{}
	DataBufferT(int count, Memory flags = MEM_DEFAULT)					{ Init(count, flags); }
	DataBufferT(const T *data, int count, Memory flags = MEM_DEFAULT)	{ Init(data, count, flags); }
//	DataBufferT(const block<T, 1> &block, Memory flags = MEM_DEFAULT)	: DataBufferT(block, block.size(), flags)	{}
	DataBufferT(range<const T*> block, Memory flags = MEM_DEFAULT)		: DataBufferT(block.begin(), block.size(), flags) {}
	DataBufferT(initializer_list<T> c, Memory flags = MEM_DEFAULT)		: DataBufferT(c.begin(), c.size(), flags)	{}
	template<typename C, typename=enable_if_t<has_begin_v<C>>> DataBufferT(C &&c, Memory flags = MEM_DEFAULT) : DataBufferT(c.begin(), num_elements(c), flags) {}

	template<typename...P> auto	Data(P&&...p)		const	{ return DataBuffer::Data<T>(forward<P>(p)...); }
	template<typename...P> auto	WriteData(P&&...p)	const	{ return DataBuffer::WriteData<T>(forward<P>(p)...); }
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

template<typename T> static const PrimType	prim	= PRIM_UNKNOWN;
template<> static const PrimType prim<PointList>	= PRIM_POINTLIST;
template<> static const PrimType prim<LineList>		= PRIM_LINELIST;
template<> static const PrimType prim<LineStrip>	= PRIM_LINESTRIP;
template<> static const PrimType prim<TriList>		= PRIM_TRILIST;
template<> static const PrimType prim<TriStrip>		= PRIM_TRISTRIP;
template<> static const PrimType prim<TriFan>		= PRIM_TRIFAN;

template<typename T> int	verts(int p);
template<> inline int verts<PointList>(int p)	{ return p * 1;}
template<> inline int verts<LineList>(int p)	{ return p * 2;}
template<> inline int verts<LineStrip>(int p)	{ return p + 1;}
template<> inline int verts<TriList>(int p)		{ return p * 3;}
template<> inline int verts<TriStrip>(int p)	{ return p + 2;}
template<> inline int verts<TriFan>(int p)		{ return p + 1;}

template<typename P, typename I> struct Prim {
	I		i;
	Prim(I i) : i(i)			{}
	auto		next()			{ return this->i + (verts<P>(2) - verts<P>(1)); }
	Prim&		operator++()	{ i = next(); return *this; }
	decltype(auto)	operator[](int j)	{ return i[j]; }
};

template<typename P, typename I> int operator-(const Prim<P,I> &a, const Prim<P,I> &b) { return (a.i - b.i - verts<P>(1)) / (verts<P>(2) - verts<P>(1)) + 1; }

#ifdef ISO_HAS_QUADS

template<> static const PrimType prim<QuadList>		= PRIM_QUADLIST;
template<> static const PrimType prim<QuadStrip>	= PRIM_QUADSTRIP;
template<> inline int verts<QuadList>(int p)	{ return p * 4;}
template<> inline int verts<QuadStrip>(int p)	{ return p * 2 + 2;}

template<typename I> struct Prim<QuadList, I> : PrimT<I, 4, PRIM_QUADLIST> {
	decltype(auto)	operator[](int j)	{ return this->i[j ^ ((j & 2) ? 0 : 1)]; }
	QuadListT(I i) : Prim<I, 4, PRIM_QUADLIST>(i)	{}
};
template<typename I> struct QuadStripT : PrimT<I, 4, PRIM_QUADSTRIP> {
	QuadStripT(I i) : Prim<I, 4, PRIM_QUADSTRIP>(i)	{}
};

#else

template<> static const PrimType prim<QuadList>		= PRIM_TRILIST;
template<> static const PrimType prim<QuadStrip>	= PRIM_TRISTRIP;
template<> inline int verts<QuadList>(int p)	{ return p * 6;}
template<> inline int verts<QuadStrip>(int p)	{ return p * 2 + 2;}

template<typename I> struct Prim<QuadList, I> {
	I		i;
	Prim(I i) : i(i)		{}
	~Prim()						{ next(); }
	auto		next()			{ this->i[4] = this->i[2]; this->i[5] = this->i[1]; return this->i + 6; }
	Prim&		operator++()	{ this->i = next(); return *this; }
	decltype(auto)	operator[](int j)	{ return i[j]; }
	template<typename S> void operator=(const S &s)	{ copy(s.corners(), this->i); }
};

#endif

typedef QuadStrip SingleQuad;

#ifdef ISO_HAS_RECTS

template<> static const PrimType prim<RectList>		= PRIM_RECTLIST;
template<> inline int verts<RectList>(int p)	{ return p * 2;}

template<typename I> struct RectListT : PrimT<I, 3, PRIM_RECTLIST> {
	decltype(auto)	operator[](int j)	{ return this->i[j]; }
	RectListT(I i) : Prim<I, 3, PRIM_RECTLIST>(i)	{}
};

#else

template<> static const PrimType prim<RectList>		= prim<QuadList>;
template<> inline int verts<RectList>(int p)	{ return verts<QuadList>(p); }

template<typename I> struct Prim<RectList, I> : Prim<QuadList, I> {
	Prim(I i) : Prim<QuadList, I>(i)		{}
	~Prim()						{ next(); }
	auto		next()			{ (*this)[3] = (*this)[2] + ((*this)[1] - (*this)[0]); return Prim<QuadList,I>::next(); }
	Prim&		operator++()	{ this->i = next(); return *this; }
};

#endif

template<class T> class ImmediatePrims;

template<class P, class T> class ImmediateStream<Prim<P, T*>> : ImmediateStream<T> {
public:
	ImmediateStream(GraphicsContext &ctx, int count) : ImmediateStream<T>(ctx, prim<P>, verts<P>(count)) {}
	Prim<P, T*>		begin()					{ return ImmediateStream<T>::begin(); }
	Prim<P, T*>		end()					{ return ImmediateStream<T>::end(); }
	void			SetCount(int i)			{ ImmediateStream<T>::SetCount(verts<P>(i)); }
};

struct GraphicsBlock {
	GraphicsContext &ctx;
	GraphicsBlock(GraphicsContext &ctx, const char *label) : ctx(ctx)	{ ctx.PushMarker(label); }
	~GraphicsBlock()				{ ctx.PopMarker(); }
	void Next(const char *label)	{ ctx.PopMarker(); ctx.PushMarker(label); }
};

} // namespace iso

#endif
