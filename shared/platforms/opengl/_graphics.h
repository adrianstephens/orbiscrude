#ifndef GRAPHICS_H
#define GRAPHICS_H

//-----------------------------------------------------------------------------
//	OpenGL graphics
//-----------------------------------------------------------------------------

#include "base/defs.h"
#include "base/pointer.h"
#include "base/strings.h"
#include "base/array.h"
#include "base/vector.h"
#include "base/soft_float.h"
#include "base/atomic.h"
#include "base/block.h"
#include "extra/colour.h"
#include "packed_types.h"
#include "shared/graphics_defs.h"
#include "allocators/lf_allocator.h"
#include "allocators/pool.h"

#if defined PLAT_ANDROID

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#elif defined PLAT_IOS

#include <OpenGLES/ES2/glext.h>
#include "extra/objc.h"

OBJC_INTRFACE(CALayer);

#define	GL_TEXTURE_MAX_LEVEL	GL_TEXTURE_MAX_LEVEL_APPLE
#define GL_TEXTURE_COMPARE_MODE		GL_TEXTURE_COMPARE_MODE_EXT
#define GL_COMPARE_REF_TO_TEXTURE	GL_COMPARE_REF_TO_TEXTURE_EXT
#define GL_TEXTURE_COMPARE_FUNC		GL_TEXTURE_COMPARE_FUNC_EXT
#define	GL_DEPTH_STENCIL			GL_DEPTH_STENCIL_OES
#define GL_SAMPLER_2D_SHADOW	GL_SAMPLER_2D_SHADOW_EXT
#define	GL_UNSIGNED_INT_24_8	GL_UNSIGNED_INT_24_8_OES
#define GL_HALF_FLOAT			GL_HALF_FLOAT_OES
#define GL_MIN					GL_MIN_EXT
#define GL_MAX					GL_MAX_EXT
#define GL_R8					GL_R8_EXT
#define GL_RG8					GL_RG8_EXT
#define GL_RED					GL_RED_EXT
#define GL_RG					GL_RG_EXT
#define GL_R16F					GL_R16F_EXT
#define GL_RG16F				GL_RG16F_EXT
#define GL_RGB16F				GL_RGB16F_EXT

#define GL_MAP_WRITE_BIT		GL_WRITE_ONLY_OES

#define glBindVertexArray		glBindVertexArrayOES
#define glUnmapBuffer			glUnmapBufferOES
#define glMapBufferRange(target, offset, length, access)	glMapBufferOES(target, access)
#define	glGenVertexArrays		glGenVertexArraysOES

#elif defined PLAT_MAC

#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#include "extra/objc.h"

OBJC_INTRFACE(CALayer);

#define GL_COMPARE_REF_TO_TEXTURE	GL_COMPARE_R_TO_TEXTURE
#define GL_MAP_WRITE_BIT		GL_WRITE_ONLY
#define glBindVertexArray		glBindVertexArrayAPPLE
#define glMapBufferRange(target, offset, length, access)	glMapBufferARB(target, access)
#define	glGenVertexArrays		glGenVertexArraysAPPLE

#endif

namespace iso {

template<typename T> void DeInit(T *t);
template<typename T> void Init(T *t, void*);

extern bool OpenGLThreadInit();
extern void OpenGLThreadDeInit();

#ifdef ISO_DEBUG
extern GLenum	gl_error;
extern void	glErrorCheck();
inline bool	glCheckResult() {
	return (gl_error = glGetError()) == GL_NO_ERROR;
}
#else
#define		glErrorCheck()
inline bool	glCheckResult() {
	return true;
}
#endif

//-----------------------------------------------------------------------------
//	point/rect
//-----------------------------------------------------------------------------

force_inline const float4x4 &hardware_fix(param(float4x4) mat)	{ return mat; }
float4x4 map_fix(param(float4x4) mat);

class ColourConverter {
	colour	c;
public:
	ColourConverter(param(colour) _c) : c(_c)	{}
	operator	uint32() const	{ float4 t = c.rgba * 255; return (int(t.x)<<24) | (int(t.y)<<16) | (int(t.z)<<8) | (int(t.w)<<0); }
};

//-----------------------------------------------------------------------------
//	enums
//-----------------------------------------------------------------------------

enum MSAA {
	MSAA_NONE,
	MSAA_FOUR,
};

enum Memory {
	MEM_DEFAULT		= GL_STATIC_DRAW,
	MEM_DYNAMIC		= GL_DYNAMIC_DRAW,
	MEM_STREAM		= GL_STREAM_DRAW,

	MEM_OTHER		= MEM_DEFAULT,
	MEM_TARGET		= MEM_DEFAULT,
	MEM_DEPTH		= MEM_DEFAULT,
	
	MEM_CPU_WRITE	= MEM_DEFAULT,
    MEM_CPU_READ	= MEM_DEFAULT,
};

enum PrimType {
	PRIM_POINTLIST	= GL_POINTS,
	PRIM_LINELIST	= GL_LINES,
	PRIM_LINELOOP	= GL_LINE_LOOP,
	PRIM_LINESTRIP	= GL_LINE_STRIP,
	PRIM_TRILIST	= GL_TRIANGLES,
	PRIM_TRISTRIP	= GL_TRIANGLE_STRIP,
	PRIM_TRIFAN		= GL_TRIANGLE_FAN,
};
inline PrimType AdjacencyPrim(PrimType p)	{ return p; }
inline PrimType PatchPrim(int n)			{ return PrimType(0); }

enum BackFaceCull {
	BFC_NONE		= 0,
	BFC_FRONT		= GL_FRONT,
	BFC_BACK		= GL_BACK,
	BFC_BOTH		= GL_FRONT_AND_BACK,
};

enum DepthTest {
	DT_NEVER		= GL_NEVER,
	DT_LESS			= GL_LESS,  
	DT_EQUAL		= GL_EQUAL,
	DT_LEQUAL		= GL_LEQUAL,
	DT_GREATER		= GL_GREATER,
	DT_NOTEQUAL		= GL_NOTEQUAL,
	DT_GEQUAL		= GL_GEQUAL,
	DT_ALWAYS		= GL_ALWAYS,

	DT_USUAL		= DT_LEQUAL,
	DT_CLOSER_SAME	= DT_LEQUAL,
	DT_CLOSER		= DT_LESS,
};

enum AlphaTest {
	AT_NEVER		= GL_NEVER,
	AT_LESS			= GL_LESS,  
	AT_EQUAL		= GL_EQUAL,
	AT_LEQUAL		= GL_LEQUAL,
	AT_GREATER		= GL_GREATER,
	AT_NOTEQUAL		= GL_NOTEQUAL,
	AT_GEQUAL		= GL_GEQUAL,
	AT_ALWAYS		= GL_ALWAYS,
};

enum UVMode {
	U_CLAMP			= GL_CLAMP_TO_EDGE,
	V_CLAMP			= GL_CLAMP_TO_EDGE	<<16,
	U_WRAP			= GL_REPEAT,
	V_WRAP			= GL_REPEAT			<<16,
	U_MIRROR		= GL_MIRRORED_REPEAT,
	V_MIRROR		= GL_MIRRORED_REPEAT<<16,

	ALL_CLAMP		= U_CLAMP	| V_CLAMP,
	ALL_WRAP		= U_WRAP	| V_WRAP,
	ALL_MIRROR		= U_MIRROR	| V_MIRROR,
};

#define MAKE_TEXFILTER(min, mag)	((min - GL_NEAREST_MIPMAP_NEAREST) << 0) | ((mag - GL_NEAREST) << 2)
enum TexFilter {// mag, min, mip
	TF_NEAREST_NEAREST_NEAREST	= MAKE_TEXFILTER(GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST),
	TF_NEAREST_LINEAR_NEAREST	= MAKE_TEXFILTER(GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR),
	TF_NEAREST_NEAREST_LINEAR	= MAKE_TEXFILTER(GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST),
	TF_NEAREST_LINEAR_LINEAR	= MAKE_TEXFILTER(GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR),

	TF_LINEAR_NEAREST_NEAREST	= MAKE_TEXFILTER(GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST),
	TF_LINEAR_LINEAR_NEAREST	= MAKE_TEXFILTER(GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR),
	TF_LINEAR_NEAREST_LINEAR	= MAKE_TEXFILTER(GL_LINEAR_MIPMAP_LINEAR, GL_NEAREST),
	TF_LINEAR_LINEAR_LINEAR		= MAKE_TEXFILTER(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR),

	TF_ANISOTROPIC_2			= TF_LINEAR_LINEAR_LINEAR | 0x10,
	TF_ANISOTROPIC_4			= TF_LINEAR_LINEAR_LINEAR | 0x20,
	TF_ANISOTROPIC_8			= TF_LINEAR_LINEAR_LINEAR | 0x30,
	TF_ANISOTROPIC_16			= TF_LINEAR_LINEAR_LINEAR | 0x40,
};
#undef MAKE_TEXFILTER

enum ChannelMask {
	CM_RED			= 1 << 0,
	CM_GREEN		= 1 << 1,
	CM_BLUE			= 1 << 2,
	CM_ALPHA		= 1 << 3,
	CM_RGB			= CM_RED | CM_GREEN | CM_BLUE,
	CM_ALL			= CM_RED | CM_GREEN | CM_BLUE | CM_ALPHA,
	CM_NONE			= 0,
};

enum CubemapFace {
	CF_POS_X		= GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	CF_NEG_X		= GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	CF_POS_Y		= GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	CF_NEG_Y		= GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	CF_POS_Z		= GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	CF_NEG_Z		= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

enum TexState {
	TS_ADDRESSU		= GL_TEXTURE_WRAP_S,
	TS_ADDRESSV		= GL_TEXTURE_WRAP_T,
	TS_MAG			= GL_TEXTURE_MAG_FILTER,
	TS_MIN			= GL_TEXTURE_MIN_FILTER,
	TS_MAXLOD		= GL_TEXTURE_MAX_LEVEL,
#ifndef PLAT_ANDROID
	TS_MAXANISO		= GL_TEXTURE_MAX_ANISOTROPY_EXT,
#endif
//#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT                       0x84FF
};

#define TSV_MAXLOD	13

enum RenderTarget {
	RT_COLOUR0					= GL_COLOR_ATTACHMENT0,
	RT_DEPTH					= GL_DEPTH_ATTACHMENT,
	_RT_NUM,
};

enum BlendOp {
	BLENDOP_ADD					= GL_FUNC_ADD,
	BLENDOP_SUBTRACT			= GL_FUNC_SUBTRACT,
	BLENDOP_REVSUBTRACT			= GL_FUNC_REVERSE_SUBTRACT,
	BLENDOP_MIN					= GL_MIN,
	BLENDOP_MAX					= GL_MAX,
};

enum BlendFunc {
	BLEND_ZERO					= GL_ZERO,
	BLEND_ONE					= GL_ONE,
	BLEND_SRC_COLOR				= GL_SRC_COLOR,
	BLEND_INV_SRC_COLOR			= GL_ONE_MINUS_SRC_COLOR,
	BLEND_DST_COLOR				= GL_DST_COLOR,
	BLEND_INV_DST_COLOR			= GL_ONE_MINUS_DST_COLOR,
	BLEND_SRC_ALPHA  			= GL_SRC_ALPHA,
	BLEND_INV_SRC_ALPHA			= GL_ONE_MINUS_SRC_ALPHA,
	BLEND_DST_ALPHA				= GL_DST_ALPHA,
	BLEND_INV_DST_ALPHA			= GL_ONE_MINUS_DST_ALPHA,
	BLEND_SRC_ALPHA_SATURATE	= GL_SRC_ALPHA_SATURATE,
	BLEND_CONSTANT_COLOR		= GL_CONSTANT_COLOR,
	BLEND_INV_CONSTANT_COLOR	= GL_ONE_MINUS_CONSTANT_COLOR,
	BLEND_CONSTANT_ALPHA		= GL_CONSTANT_ALPHA,
	BLEND_INV_CONSTANT_ALPHA	= GL_ONE_MINUS_CONSTANT_ALPHA,
};

enum StencilOp {
	STENCILOP_KEEP				= GL_KEEP,
	STENCILOP_ZERO				= GL_ZERO,
	STENCILOP_REPLACE			= GL_REPLACE,
	STENCILOP_INCR				= GL_INCR,
	STENCILOP_INCR_WRAP			= GL_INCR_WRAP,
	STENCILOP_DECR				= GL_DECR,
	STENCILOP_DECR_WRAP			= GL_DECR_WRAP,
	STENCILOP_INVERT			= GL_INVERT,
};

enum StencilFunc {
	STENCILFUNC_NEVER			= GL_NEVER,
	STENCILFUNC_LESS			= GL_LESS,  
	STENCILFUNC_LEQUAL			= GL_EQUAL,
	STENCILFUNC_GREATER			= GL_LEQUAL,
	STENCILFUNC_GEQUAL			= GL_GREATER,
	STENCILFUNC_EQUAL			= GL_NOTEQUAL,
	STENCILFUNC_NOTEQUAL		= GL_GEQUAL,
	STENCILFUNC_ALWAYS			= GL_ALWAYS,
};

enum FillMode {
//	FILL_SOLID			= D3DFILL_SOLID,
//	FILL_WIREFRAME		= D3DFILL_WIREFRAME,
};

//-----------------------------------------------------------------------------
//	component types
//-----------------------------------------------------------------------------

enum ComponentType {};

template<typename T> struct _ComponentType {};
#define DEFCOMPTYPE(T, V, N)	template<> struct _ComponentType<T> { enum {value = (V - GL_BYTE) | (N << 7)}; }
DEFCOMPTYPE(void,		GL_ZERO,			0);
DEFCOMPTYPE(uint8,		GL_UNSIGNED_BYTE,	0);
DEFCOMPTYPE(unorm8,		GL_UNSIGNED_BYTE,	1);
DEFCOMPTYPE(int8,		GL_BYTE,			0);
DEFCOMPTYPE(norm8,		GL_BYTE,			1);
DEFCOMPTYPE(uint16,		GL_UNSIGNED_SHORT,	0);
DEFCOMPTYPE(unorm16,	GL_UNSIGNED_SHORT,	1);
DEFCOMPTYPE(int16,		GL_SHORT,			0);
DEFCOMPTYPE(norm16,		GL_SHORT,			1);
DEFCOMPTYPE(float16,	GL_HALF_FLOAT,		0);
DEFCOMPTYPE(float,		GL_FLOAT,			0);

template<typename T, int N> struct _ComponentType<T[N]>			{ enum {value = _ComponentType<T>::value + ((N - 1) << 4)};	};
template<typename T> struct _ComponentType<array_vec<T,2> >	: _ComponentType<T[2]> {};
template<typename T> struct _ComponentType<array_vec<T,3> >	: _ComponentType<T[3]> {};
template<typename T> struct _ComponentType<array_vec<T,4> >	: _ComponentType<T[4]> {};
template<typename T> struct _ComponentType<constructable<T> >	: _ComponentType<T> {};
template<> struct _ComponentType<rgba8>							: _ComponentType<unorm8[4]> {};
template<> struct _ComponentType<float4>						: _ComponentType<float[4]> {};
template<> struct _ComponentType<colour>						: _ComponentType<float[4]> {};
//fake
template<> struct _ComponentType<uint32>						: _ComponentType<uint16[2]> {};

template<typename T> constexpr ComponentType	GetComponentType()			{ return (ComponentType)_ComponentType<T>::value; }
template<typename T> constexpr ComponentType	GetComponentType(const T&)	{ return GetComponentType<T>(); }
template<typename T> constexpr TexFormat		GetTexFormat()				{ return TexFormat(_ComponentType<T>::value); }

//-----------------------------------------------------------------------------
//	Texture/Surface
//-----------------------------------------------------------------------------

struct TextureData {
	uint32			width:11, height:11, format:5;
	void			*data;
	uint32			pitch;
	TextureData(uint32 width, uint32 height, TexFormat format);
	~TextureData();
};

class Surface {
	union {
		uint32	u0;
		struct {
			uint32	width:11, height:11, face:3, mip:4, fromtex:1, temp:1;
		};
	};
	GLuint	name;
	template<typename T> struct DataT : TextureData, block<T, 2> {
		DataT(Surface *s) : TextureData(s->width, s->height, GetTexFormat<T>()), block<T,2>(make_strided_block<T>(data, width + 1, pitch, height + 1)) {}
	};

public:
	Surface() : u0(0), name(0)	{}
	Surface(TexFormat format, int _width, int _height);
	Surface(uint32 _name, int _mip, int _face, int _width, int _height);
	Surface(const Surface &s) : u0(s.u0), name(s.name) { temp = 1; }
	~Surface();

	void		Set(GLuint _name, int _width, int _height);
	bool		FromTex()		const	{ return !!fromtex;						}
	int			Name()			const	{ return name;							}
	GLenum		Face()			const	{ return face == 0 ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP_POSITIVE_X + face - 1; }
	int			Level()			const	{ return mip;							}
	point		Size()			const	{ return point{width + 1, height + 1};	}
	rect		GetRect()		const	{ return rect(zero, Size()); }
	template<typename T> DataT<T>	Data()	const	{ return this; }
};

class Texture {
	pointer32<OGLTexture>	tex;
	
	template<typename T> struct DataT : TextureData, block<T, 2> {
		DataT(OGLTexture *tex) : TextureData(tex->width, tex->height, (TexFormat)tex->format), block<T,2>(make_strided_block<T>(data, width + 1, pitch, height + 1)) {}
	};
public:
	void		Set(GLuint name, TexFormat format, int width, int height);
	bool		Init(TexFormat format, int width, int height, int depth = 1, int mips = 1, uint16 flags = MEM_DEFAULT);
	void		DeInit();
	Texture		As(TexFormat format)	const;

	Texture()					: tex(0)	{}
	Texture(TexFormat format, int width, int height, int depth = 1, int mips = 1, uint16 flags = MEM_DEFAULT) : tex(0)	{
		Init(format, width, height, depth, mips, flags);
	}
	~Texture();

	Surface		GetSurface(int i = 0)				const;
	Surface		GetSurface(CubemapFace f, int i)	const;

	TexFormat	Format()		const	{ return tex ? TexFormat(tex->format) : TexFormat::TEXF_UNKNOWN; }
	bool		IsDepth()		const	{ return tex && tex->format >= TexFormat::_TEXF_DEPTH;	}
	bool		IsCube()		const	{ return tex && tex->cube;		}

	point		Size()			const	{ return point{tex->width + 1, tex->height + 1}; }
	int			Depth()			const	{ return tex->depth + 1;		}
	operator const GLuint()		const	{ return tex ? tex->name : 0;	}
	template<typename T> DataT<const T>		Data()		const	{ return tex; }
	template<typename T> DataT<T>			WriteData()	const	{ return tex; }

	OGLTexture*	GetRaw()		const	{ return tex; }
	void		SetTexFilter(TexFilter t);
};

class DataBuffer {
	pointer32<OGLBuffer>	buff;

	template<typename T> struct DataT : TextureData {
		DataT(OGLBuffer *buff) : TextureData(0, 0, (TexFormat)0) {}
	};

	bool	Init(TexFormat format, int width, Memory flags = MEM_DEFAULT) {
		return false;
	}
	bool	Init(TexFormat format, int width, Memory flags, void *data) {
		return false;
	}
	template<typename T> DataT<T>	Data()	const {
		return this;
	}
};

class ConstBuffer {
	malloc_block	data;
public:
	ConstBuffer(size_t size) : data(size) {}
	void	*Data() { return data; }
};

//-----------------------------------------------------------------------------
//	VertexBuffer/IndexBuffer
//-----------------------------------------------------------------------------

class GraphicsContext;

template<GLenum TARGET> class _GraphicsBuffer {
	friend GraphicsContext;
protected:
	GLuint			name;
public:
	bool			Init(void *data, uint32 size, Memory loc = MEM_DEFAULT);
	bool			Create(uint32 size, Memory loc = MEM_DEFAULT);
	void*			Begin();
	void			End();
	_GraphicsBuffer();
	~_GraphicsBuffer();
};

typedef _GraphicsBuffer<GL_ARRAY_BUFFER> 			_VertexBuffer;
typedef _GraphicsBuffer<GL_ELEMENT_ARRAY_BUFFER> 	_IndexBuffer;

template<typename T> class IndexBuffer : public _IndexBuffer {
public:
	IndexBuffer()										{}
	IndexBuffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)				{ Init(t, n, loc);	}
	template<int N> IndexBuffer(const T (&t)[N], Memory loc = MEM_DEFAULT)	{ Init(t, N, loc);	}

	bool					Init(const T *t, uint32 n, Memory loc = MEM_DEFAULT){ return _IndexBuffer::Init(t, n * sizeof(T), loc);	}
	template<int N> bool	Init(const T (&t)[N], Memory loc = MEM_DEFAULT)		{ return _IndexBuffer::Init(&t, sizeof(t), loc);	}
	bool	Create(uint32 n, Memory loc = MEM_DEFAULT)	{ return _IndexBuffer::Create(n * sizeof(T), loc);	}
	T*		Begin(uint32 n, Memory loc = MEM_DEFAULT)	{ return Create(n, loc) ? Begin() : NULL;	}
	T*		Begin()										{ return (T*)_IndexBuffer::Begin();			}
};

//-----------------------------------------------------------------------------
//	Vertex
//-----------------------------------------------------------------------------

struct VertexElement : OGLVertexElement {
	VertexElement()		{}
	VertexElement(int offset, int type, uint32 usage, int stream = 0) : OGLVertexElement(offset, type, usage, stream) {}
	template<typename B, typename T> VertexElement(T B::* p, uint32 usage, int stream = 0) : OGLVertexElement(int(intptr_t(get_ptr(((B*)0)->*p))), GetComponentType<T>(), usage, stream) {}
	void	SetUsage(uint32 usage) { attribute	= usage; }
};

struct VertexDescription {
	GLuint			vao;
	VertexDescription(const OGLVertexElement *ve, size_t n, size_t stride, GLuint vbo);
};

struct VertexDescription2 : _VertexBuffer, VertexDescription {
	VertexDescription2(VertexElements ve, size_t stride) : VertexDescription(ve.p, ve.n, stride, _VertexBuffer::name) {}
};

template<typename T> const	VertexDescription2&	GetVD()				{ static VertexDescription2 vd(GetVE<T>(), sizeof(T)); return vd; }
template<typename T> const	VertexDescription2&	GetVD(const T &t)	{ return GetVD<T>(); }

template<typename T> struct VertexBufferT : VertexDescription2 {
	VertexBufferT() : VertexDescription2(GetVE<T>(), sizeof(T)) {}
};

template<typename T> class VertexBuffer : public VertexBufferT<T> {
public:
	VertexBuffer()															{}
	VertexBuffer(const T *t, uint32 n, Memory loc = MEM_DEFAULT)			{ Init(t, n, loc);	}
	template<int N> VertexBuffer(const T (&t)[N], Memory loc = MEM_DEFAULT)	{ Init(t, loc);		}

	bool					Init(const T *t, uint32 n, Memory loc = MEM_DEFAULT){ return _VertexBuffer::Init(t, n * sizeof(T), loc);		}
	template<int N> bool	Init(const T (&t)[N], Memory loc = MEM_DEFAULT)		{ return _VertexBuffer::Init(&t, sizeof(t), loc);		}
	bool	Create(uint32 n, Memory loc = MEM_DEFAULT)		{ return _VertexBuffer::Create(n * sizeof(T), loc);	}
	T*		Begin(uint32 size, Memory loc = MEM_DEFAULT)	{ return Create(size, loc) ? Begin() : NULL;		}
	T*		Begin()											{ return (T*)_VertexBuffer::Begin();				}
};

//-----------------------------------------------------------------------------
//	Shaders
//-----------------------------------------------------------------------------

class OGLShader {
public:
	uint32			vs;
	uint32			ps;
//	GLuint			Name()	const	{ return ((GLuint*)this)[-2]; }
//	GLuint			&Name()			{ return ((GLuint*)this)[-2]; }
	GLuint			Name()	const	{ return (GLuint&)vs; }
	GLuint			&Name()			{ return (GLuint&)vs; }
	void*			Params()const	{ return (pointer32<void>&)ps; }
	pointer32<void>	&Params()		{ return (pointer32<void>&)ps; }
	operator GLuint() const	{ return Name(); }
};

struct ShaderReg {
	union {
		struct {uint16	reg:11, type:5, count:15, indirect:1;};
		uint32	u;
	};
	ShaderReg(uint32 _u)	{ u = _u;	}
	operator uint32() const	{ return u;	}
};

enum ShaderParameterType {
	SPT_INT1,
	SPT_INT2,
	SPT_INT3,
	SPT_INT4,
	SPT_FLOAT1,
	SPT_FLOAT2,
	SPT_FLOAT3,
	SPT_FLOAT4,
	SPT_FLOAT22,
	SPT_FLOAT33,
	SPT_FLOAT44,
	SPT_SAMPLER_2D,
	SPT_SAMPLER_CUBE,
	SPT_COUNT,
};

class ShaderParameterIterator {
	void			*params;
	uint8			*p;
	uint8			i, total;
public:
	ShaderParameterIterator(const OGLShader &s) : params(s.Params()) {
		p		= (uint8*)params;
		total	= *p++;
		i		= 0;
	}
	ShaderParameterIterator(const OGLShader &s, GraphicsContext &ctx) : ShaderParameterIterator(s) {}

	const char		*Name()		const		{ return (const char*)p + 3;	}
	const void		*Default()	const		{ return 0;						}
	const void		*DefaultPerm()	const	{ return 0;						}
	ShaderReg		Reg()		const		{ return p[0] + (p[1] << 8) + (p[2] << 16);	}

	int				Total()		const		{ return total;					}
	operator		bool()		const		{ return i < total;				}
	ShaderParameterIterator& operator++()	{
		p += 3;
		while (*p++)
			;
		i++;
		return *this;
	}
	ShaderParameterIterator&	Reset()		{ i = 0; p = (uint8*)params + 1; return *this;	}
};

//-----------------------------------------------------------------------------
//	Graphics
//-----------------------------------------------------------------------------

typedef atomic<circular_allocator> FrameAllocator;

class Graphics {
	friend GraphicsContext;
	template<GLenum TARGET> friend class _GraphicsBuffer;
	friend VertexDescription;

	struct State {
		enum FLAGS {
			EN_CULL_FACE				= 1<<0,
			EN_BLEND					= 1<<1,
			EN_DEPTH_TEST				= 1<<2,
			EN_SAMPLE_ALPHA_TO_COVERAGE = 1<<3,
			EN_DEPTH_WRITE				= 1<<4,
			EN_SCISSOR_TEST				= 1<<5,
			EN_POLYGON_OFFSET_FILL		= 1<<6,
		};
		enum BUFFERS {
			BUFFER_VB, BUFFER_IB,
			BUFFER_NUM,
		};
		struct Blend {uint16 op, src, dest; } rgb, a;
		GLuint			tex_2d[8], tex_cube[8], shader, buffers[BUFFER_NUM], va;
		float			bias, slope;
		uint16			cull, depth;
		uint8			active_tex, mask;
		flags<FLAGS>	enabled;

		template<int N> struct EnableBit;
		template<int N> struct BufferIndex;

		State()											{ clear(*this); }
		void			Reset()							{ clear(*this); }
		template<int N> inline bool Flip()				{ enabled.flip(FLAGS(N)); return true;	}
		template<int N> inline bool Enable()			{ return !enabled.test_set(FLAGS(N));	}
		template<int N> inline bool Disable()			{ return enabled.test_clear(FLAGS(N));	}
		template<int N> inline bool Enable(bool enable) { return enabled.test(FLAGS(N)) != enable && Flip<N>(); }
		template<int N> inline bool SetBuffer(uint32 i)	{ if (buffers[N] != i) { buffers[N] = i; return true; } return false; }
	};
	
	const char			*extensions;
	GLuint				framebuffer;
	FrameAllocator		fa;
	State				default_state, *current_state;
	
	struct id_tracker {
		bitarray<1024>	used;
		void	use(GLuint i)		{ ISO_ASSERT(i < 1024 && !used[i].test_set()); }
		void	disuse(GLuint i)	{ ISO_ASSERT(i < 1024 && used[i].test_clear());}
	} tex_ids, buff_ids;
	
	template<GLenum TARGET> void SetBuffer(GLuint name);
	void				SetVertices(GLuint name);

	void				GenBufferName(GLuint &name) {
		glGenBuffers(1, &name);
		buff_ids.use(name);
	}
	void				ReleaseBufferName(GLuint name) {
		buff_ids.disuse(name);
		glDeleteBuffers(1, &name);
	}

public:
	class Display {
	protected:
		GLuint	framebuffer, renderbuffer;

		Display() {
			glGenFramebuffers(1, &framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
			glGenRenderbuffers(1, &renderbuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
			glErrorCheck();
		}
		~Display();

		void	SetSize(point newsize);
		void	BeginFrame(CALayer *layer, Surface &surface);
		void	Present(GraphicsContext &ctx);
	};

	void				Init();
	void				BeginScene(GraphicsContext &ctx);
	void				EndScene(GraphicsContext &ctx);

	void	*alloc	(size_t size, size_t align)				{ OpenGLThreadInit(); return ::malloc(size); }
	void	*realloc(void *p, size_t size, size_t align)	{ return 0;							}
	bool	free	(void *p)								{ ::free(p); return true;			}
	bool	free	(void *p, size_t)						{ return free(p);					}
	void	transfer(void *d, const void *s, size_t size)	{ memcpy(d, s, size);				}
	uint32	fix		(void *p, size_t size)					{ glFlush(); ::free(p); return 0;	}
	void*	unfix	(uint32 p)								{ return 0;							}
};

extern Graphics graphics;

template<GLenum TARGET> _GraphicsBuffer<TARGET>::_GraphicsBuffer() {
	graphics.GenBufferName(name);
}

template<GLenum TARGET> _GraphicsBuffer<TARGET>::~_GraphicsBuffer() {
	graphics.ReleaseBufferName(name);
}

template<GLenum TARGET> void *_GraphicsBuffer<TARGET>::Begin() {
	ISO_ASSERT(!(name & 0x80000000));
	graphics.SetBuffer<TARGET>(name);
	name |= 0x80000000;
	return glMapBufferRange(TARGET, 0, 0, GL_MAP_WRITE_BIT);
}

template<GLenum TARGET> void _GraphicsBuffer<TARGET>::End() {
	ISO_ASSERT(name & 0x80000000);
	name &= ~0x80000000;
	glUnmapBuffer(TARGET);
}

template<GLenum TARGET> bool _GraphicsBuffer<TARGET>::Create(uint32 size, Memory loc) {
	graphics.SetBuffer<TARGET>(name);
	glBufferData(TARGET, size, 0, loc);
	return glCheckResult();
}

template<GLenum TARGET> bool _GraphicsBuffer<TARGET>::Init(void *data, uint32 size, Memory loc) {
	if (data) {
		graphics.SetBuffer<TARGET>(name);
		glBufferData(TARGET, size, data, loc);
		return glCheckResult();
	}
	return false;
}

class GraphicsContext {
	friend Graphics;
	typedef Graphics::State State;
	
	uint32				cubemaps;
	rect				window;
	Surface				targets[2];

	GLenum	TexTarget(int i) const {
		return i < 8 ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP;
	}

	Graphics::State	state;

	template<int N> inline void	Enable() {
		if (state.Enable<State::EnableBit<N>::value>())
			glEnable(N);
	}
	template<int N> inline void	Disable() {
		if (state.Disable<State::EnableBit<N>::value>())
			glDisable(N);
	}
	template<int N> inline void	Enable(bool enable) {
		if (state.Enable<State::EnableBit<N>::value>(enable))
			enable ? glEnable(N) : glDisable(N);
	}
	inline void SetActiveTex(uint8 i)				{
		if (state.active_tex != i)
			glActiveTexture(GL_TEXTURE0 + (state.active_tex = i));
	}

	inline int	Prim2Verts(PrimType prim, uint32 count) {
		//								   PL LL LP LS TL TS TF
		static const	uint8 mult[]	= {1, 2, 1, 1, 3, 1, 1};
		static const	uint8 add[]		= {0, 0, 1, 1, 0, 2, 2};
		return count * mult[prim] + add[prim];
	}

	void				BeginScene();
public:
	FrameAllocator&		allocator()					{ return graphics.fa; }

	void				PushMarker(const char *s) {
#ifdef PLAT_IOS
		glPushGroupMarkerEXT(0, s);
#endif
	}
	void				PopMarker() {
#ifdef PLAT_IOS
		glPopGroupMarkerEXT();
#endif
	}
	void				ReadPixels(TexFormat format, const rect &rect, void* pixels);

//	void				SetViewport(const rect &rect);
	void				SetWindow(const rect &rect);
	rect				GetWindow();
	void				Clear(param(colour) colour, bool zbuffer = true);
	void				ClearZ();

	void				_SetVertexBuffer(GLuint vbo)				{ if (state.SetBuffer<State::BUFFER_VB>(vbo)) glBindBuffer(GL_ARRAY_BUFFER, vbo);	}
	void				_SetIndices(GLuint ibo)						{ if (state.SetBuffer<State::BUFFER_IB>(ibo)) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo); }
	void				_SetVertices(GLuint vao)					{ if (state.va != vao) glBindVertexArray(state.va = vao); }

	void				SetVertexBuffer(const _VertexBuffer &vb)	{ _SetVertexBuffer(vb.name); }
	void				SetIndices(const _IndexBuffer &ib)			{ _SetIndices(ib.name); }
	void				SetVertices(const VertexDescription &vd)	{ _SetVertices(vd.vao);	}

	void				Resolve(RenderTarget i = RT_COLOUR0);
	void				SetRenderTarget(const Surface& s, RenderTarget i = RT_COLOUR0);
	Surface&			GetRenderTarget(RenderTarget i = RT_COLOUR0){ return targets[i==RT_DEPTH]; }
	void				SetZBuffer(const Surface& s)				{ SetRenderTarget(s, RT_DEPTH); }

	void				SetMSAA(MSAA _msaa)	{}
	MSAA				GetMSAA()			{ return MSAA_NONE; }

	void				SetTexture(const Texture& tex, int i = 0);
	void				SetSamplerState(int i, TexState type, int32 value);
	int32				GetSamplerState(int i, TexState type);
	void				GenerateMips(const Texture &tex);

	void				DrawPrimitive(PrimType prim, uint32 start, uint32 count) {
		DrawVertices(prim, start, Prim2Verts(prim, count));
	}
	void				DrawIndexedPrimitive(PrimType prim, uint32 start, uint32 count) {
		DrawIndexedVertices(prim, start, Prim2Verts(prim, count));
	}
	void				DrawIndexedPrimitive(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start, uint32 count) {
		DrawIndexedVertices(prim, min_index, num_verts, start, Prim2Verts(prim, count));
	}

	void				DrawVertices(PrimType prim, uint32 start, uint32 count);
	void				DrawIndexedVertices(PrimType prim, uint32 start, uint32 count);
	void				DrawIndexedVertices(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start, uint32 count);

	void				SetShader(const OGLShader &s);
	void				SetShaderConstants(ShaderReg reg, const void *values);

	void				SetUVMode(int i, UVMode t);
	void				SetTexFilter(int i, TexFilter t);

	void				SetDepthBias(float bias, float slope_bias);

	void	SetBackFaceCull(BackFaceCull c) {
		if (c && c != state.cull)
			glCullFace(state.cull = c);
		Enable<GL_CULL_FACE>(!!c);
	}
	void	SetBlendEnable(bool enable) {
		Enable<GL_BLEND>(enable);
	}
	void	SetBlend(BlendOp op, BlendFunc src, BlendFunc dest) {
		if (op != state.rgb.op)
			glBlendEquation(state.rgb.op = op);
		if (src != state.rgb.src || dest != state.rgb.dest)
			glBlendFunc(state.rgb.src = src, state.rgb.dest = dest);
	}
	void	SetBlendSeparate(BlendOp op, BlendFunc src, BlendFunc dest, BlendOp opAlpha, BlendFunc srcAlpha, BlendFunc destAlpha) {
		if (op != state.rgb.op || opAlpha != state.a.op)
			glBlendEquationSeparate(state.rgb.op = op, state.a.op = opAlpha);
		if (src != state.rgb.src || dest != state.rgb.dest || srcAlpha != state.a.src || destAlpha != state.a.dest)
			glBlendFuncSeparate(state.rgb.src = src, state.rgb.dest = dest, state.a.src = srcAlpha, state.a.dest = destAlpha);
	}
	void	SetBlendConst(param(colour) col) {
		glBlendColor(col.r, col.g, col.b, col.a);
	}
	void	SetStencilOp(StencilOp fail, StencilOp zfail, StencilOp zpass) {
		glStencilOp(fail, zfail, zpass);
	}
	void	SetStencilFunc(StencilFunc func, uint8 ref, uint8 mask) {
		glStencilFunc(func, ref, mask);
	}
	void	SetStencilMask(uint8 mask) {
		glStencilMask(mask);
	}
	void	SetDepthTest(DepthTest c) {
		if (c != state.depth)
			glDepthFunc(state.depth = c);
	}
	void	SetDepthTestEnable(bool enable)	{
		Enable<GL_DEPTH_TEST>(enable);
	}
	void	SetDepthWriteEnable(bool enable) {
		if (state.Enable<State::EN_DEPTH_WRITE>(enable))
			glDepthMask(enable);
	}
	void	SetAlphaTestEnable(bool enable) {
	}
	void	SetAlphaTest(AlphaTest func, uint32 ref) {
	}
	void	SetMask(uint32 mask) {
		if (mask != state.mask) {
			state.mask = mask;
			glColorMask(mask & 1, (mask >> 1) & 1, (mask >> 2) & 1, (mask >> 3) & 1);
		}
	}
	void	SetAlphaToCoverage(bool enable) {
		Enable<GL_SAMPLE_ALPHA_TO_COVERAGE>(enable);
	};
};

template<> struct Graphics::State::EnableBit<GL_CULL_FACE>					{ enum {value = State::EN_CULL_FACE};					};
template<> struct Graphics::State::EnableBit<GL_BLEND>						{ enum {value = State::EN_BLEND};						};
template<> struct Graphics::State::EnableBit<GL_DEPTH_TEST>					{ enum {value = State::EN_DEPTH_TEST};					};
template<> struct Graphics::State::EnableBit<GL_SAMPLE_ALPHA_TO_COVERAGE>	{ enum {value = State::EN_SAMPLE_ALPHA_TO_COVERAGE};	};
template<> struct Graphics::State::EnableBit<GL_SCISSOR_TEST>				{ enum {value = State::EN_SCISSOR_TEST};				};
template<> struct Graphics::State::EnableBit<GL_POLYGON_OFFSET_FILL>		{ enum {value = State::EN_POLYGON_OFFSET_FILL};			};

template<> struct Graphics::State::BufferIndex<GL_ARRAY_BUFFER>				{ enum {value = State::BUFFER_VB};						};
template<> struct Graphics::State::BufferIndex<GL_ELEMENT_ARRAY_BUFFER>		{ enum {value = State::BUFFER_IB};						};

template<GLenum TARGET> inline void Graphics::SetBuffer(GLuint name) {
	if (current_state->SetBuffer<State::BufferIndex<TARGET>::value>(name))
		glBindBuffer(TARGET, name);
}
inline void Graphics::SetVertices(GLuint name) {
	if (current_state->va != name)
		glBindVertexArray(current_state->va = name);
}

class _ImmediateStream {
	PrimType	prim;
protected:
	void		*p;
	int			count;
	_ImmediateStream(GraphicsContext &ctx, PrimType prim, int _count, size_t tsize, const VertexDescription2 &vd);
	~_ImmediateStream();
};

template<class T> class ImmediateStream : _ImmediateStream {
public:
	ImmediateStream(GraphicsContext &ctx, PrimType prim, int _count) : _ImmediateStream(ctx, prim, _count, sizeof(T), GetVD<T>()) {}
	T&		operator[](int i)	{ return ((T*)p)[i];		}
	T*		begin()				{ return (T*)p;				}
	T*		end()				{ return (T*)p + count;		}
};


}
#endif	// GRAPHICS_H

