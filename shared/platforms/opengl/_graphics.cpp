#include "graphics.h"
#include "allocators/allocator.h"
#include "base/strings.h"
#include "base/bits.h"
#include "profiler.h"
#include "usage.h"

namespace iso {

#ifdef ISO_DEBUG
GLenum	gl_error;
void glErrorCheck() {
	static const char *errors[] = {
		"GL_INVALID_ENUM", "GL_INVALID_VALUE", "GL_INVALID_OPERATION", "", "", "GL_OUT_OF_MEMORY", "GL_INVALID_FRAMEBUFFER_OPERATION"
	};
	ISO_ASSERTF((gl_error = glGetError()) == GL_NO_ERROR, "GL error 0x%04x (%s)", gl_error, gl_error >= GL_INVALID_ENUM && gl_error <= GL_INVALID_FRAMEBUFFER_OPERATION ? errors[gl_error - GL_INVALID_ENUM] : "unknown");
}
#endif

enum GL_EXT {
	HAS_shadow_samplers,
};

Graphics	graphics;
uint32		abilities;

float4x4 map_fix(param(float4x4) mat) {
	return translate(half) * scale(half) * mat;
}

iso_export vallocator&	allocator32();

//-----------------------------------------------------------------------------
//	Texture/Surface
//-----------------------------------------------------------------------------

void Init(Texture *x, void *physram) {}
void DeInit(Texture *x)	{}

struct OGLTexture2 : OGLTexture {
	OGLTexture2() { u = 0; }
};
fixed_pool<OGLTexture2, 64>	(*temp_textures)[2];

struct ogl_format {uint16 fmt, type; uint8 bpp; } ogl_formats[] = {
	{0,						0,								0	},	//TEXF_UNKNOWN		= 0,

	{GL_R8,					GL_UNSIGNED_BYTE,				8	},	//TEXF_R8,
	{GL_RG8,				GL_UNSIGNED_BYTE,				16	},	//TEXF_RG8, (GL_UNSIGNED_SHORT_8_8_APPLE?)
	{GL_RGB,				GL_UNSIGNED_BYTE,				24	},	//TEXF_RGB8,
	{GL_RGBA,				GL_UNSIGNED_BYTE,				32	},	//TEXF_RGBA8,
	{GL_LUMINANCE,			GL_UNSIGNED_BYTE,				8	},	//TEXF_L8,
	{GL_ALPHA,				GL_UNSIGNED_BYTE,				8	},	//TEXF_A8,
	{GL_LUMINANCE_ALPHA,	GL_UNSIGNED_BYTE,				16	},	//TEXF_LA8,

	{GL_R16F,				GL_HALF_FLOAT,					16	},	//TEXF_R16F,
	{GL_RG16F,				GL_HALF_FLOAT,					32	},	//TEXF_RG16F,
#ifndef PLAT_MAC
	{GL_RGB16F,				GL_HALF_FLOAT,					48	},	//TEXF_RGB16F,
#endif
	{GL_RGBA,				GL_HALF_FLOAT,					64	},	//TEXF_RGBA16F,
	{GL_LUMINANCE,			GL_HALF_FLOAT,					16	},	//TEXF_L16F,
	{GL_ALPHA,				GL_HALF_FLOAT,					16	},	//TEXF_A16F,
	{GL_LUMINANCE_ALPHA,	GL_HALF_FLOAT,					32	},	//TEXF_LA16F,

	{GL_RED,				GL_FLOAT,						32	},	//TEXF_R32F,
	{GL_RG,					GL_FLOAT,						64	},	//TEXF_RG32F,
	{GL_RGB,				GL_FLOAT,						96	},	//TEXF_RGB32F,
	{GL_RGBA,				GL_FLOAT,						128	},	//TEXF_RGBA32F,
	{GL_LUMINANCE,			GL_FLOAT,						32	},	//TEXF_L32F,
	{GL_ALPHA,				GL_FLOAT,						32	},	//TEXF_A32F,
	{GL_LUMINANCE_ALPHA,	GL_FLOAT,						64	},	//TEXF_LA32F,

	{GL_RGB,				GL_UNSIGNED_SHORT_5_6_5,		16	},	//TEXF_RGB565
	{GL_RGBA4,				GL_UNSIGNED_SHORT_4_4_4_4,		16	},	//TEXF_RGBA4,
	{GL_RGB5_A1,			GL_UNSIGNED_SHORT_5_5_5_1,		16	},	//TEXF_RGB5A1,
#ifdef PLAT_IOS
	{GL_RGB_422_APPLE,		GL_UNSIGNED_SHORT_8_8_APPLE,	8	},	//TEXF_RGB422
	{GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,	0,				2	},	//TEXF_PVRTC2,
	{GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,	0,				4	},	//TEXF_PVRTC4,
#endif
	{GL_DEPTH_COMPONENT,	GL_UNSIGNED_SHORT,				16	},	//TEXF_D16,
	{GL_DEPTH_STENCIL,		GL_UNSIGNED_INT_24_8,			32	},	//TEXF_D24S8
	{GL_DEPTH_COMPONENT,	GL_UNSIGNED_INT,				32	},	//TEXF_D32,
	{GL_DEPTH_COMPONENT,	GL_FLOAT,						32	},	//TEXF_D32F,
};

uint32 TexturePitch(TexFormat format, uint32 width) {
    return format == TEXF_PVRTC2 || format == TEXF_PVRTC4
    ?   max(width    >> (format == TEXF_PVRTC2 ? 3 : 2), 2) * 8
    :   max(width, 2) * ogl_formats[format].bpp / 8;
}

uint32 TextureRows(TexFormat format, uint32 height) {
    return max(format == TEXF_PVRTC2 || format == TEXF_PVRTC4 ? height >> 2 : height, 2);
}

uint32 TextureMemory(TexFormat format, uint32 width, uint32 height) {
    return TexturePitch(format, width) * TextureRows(format, height);
}

template<> void Init<OGLTexture>(OGLTexture *tex, void *physram) {
	glErrorCheck();
	if (((int*)tex)[-2])
		return;
	((int*)tex)[-2] = 1;

	TexFormat	format	= TexFormat(tex->format);
	uint32		width	= tex->width	+ 1;
	uint32		height	= tex->height	+ 1;
	uint32		depth	= tex->depth0;
	uint32		mips	= tex->mips;

	uint16		fmt		= ogl_formats[format].fmt;
	uint16		type	= ogl_formats[format].type;
	uint8		*bytes	= (uint8*)physram + (tex->offset0 << 5);

	GLuint	name;
	glGenTextures(1, &name);
//	tex_ids.use(name);
	ISO_ASSERT((name & 0xff000000) == 0);

	GLenum	target		= GL_TEXTURE_2D;
	GLenum	firstface	= GL_TEXTURE_2D, lastface = GL_TEXTURE_2D;

	if (tex->cube) {
		target		= GL_TEXTURE_CUBE_MAP;
		firstface	= GL_TEXTURE_CUBE_MAP_POSITIVE_X;
		lastface	= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
	}

	glBindTexture(target, name);
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
//	if (mips == 0)
//		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(target, TS_MAXLOD, mips);
	if (tex->clamp0) {
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glErrorCheck();

	for (GLenum face = firstface; face <= lastface; ++face) {
		for (int level = 0; level <= mips; level++) {
			uint32	mip_width	= max(width  >> level, 1);
			uint32	mip_height	= max(height >> level, 1);
			uint32	size		= TextureMemory(format, mip_width, mip_height);

			if (type == 0)
				glCompressedTexImage2D(face, level, fmt, mip_width, mip_height, 0, size, bytes);
			else
				glTexImage2D(face, level, fmt, mip_width, mip_height, 0, fmt, type, bytes);

			bytes += size;
		}
	}
	tex->name		= name;
	tex->depth		= depth;
	tex->managed	= 1;

	glErrorCheck();
}

template<> void DeInit<OGLTexture>(OGLTexture *tex) {
	if (GLuint name = tex->name) {	// strip depth & flags
//		tex_ids.disuse(name);
		glDeleteTextures(1, &name);
		tex->u	= 0;
	}
}

void Init(DataBuffer *x, void *physram) {
}
void DeInit(DataBuffer *x) {
}


void Surface::Set(GLuint _name, int _width, int _height) {
	name	= _name;
	width	= _width - 1;
	height	= _height - 1;
	fromtex	= 0;
	temp	= 1;
}

Surface::Surface(TexFormat format, int _width, int _height) { PROFILE_FN
	width	= _width - 1;
	height	= _height - 1;
	fromtex	= 0;
	temp	= 0;
	glGenRenderbuffers(1, &name);
	glBindRenderbuffer(GL_RENDERBUFFER, name);
	glRenderbufferStorage(GL_RENDERBUFFER, ogl_formats[format].fmt, _width, _height);
}

Surface::Surface(uint32 _name, int _mip, int _face, int _width, int _height) {
	name	= _name;
	width	= _width - 1;
	height	= _height - 1;
	mip		= _mip;
	face	= _face;
	fromtex	= 1;
	temp	= 0;
}

Surface::~Surface() {
	if (name && !fromtex && !temp)
		glDeleteRenderbuffers(1, &name);
}

void Texture::Set(GLuint name, TexFormat format, int width, int height) {
	if (!tex) {
		tex = (*temp_textures)[0].alloc();
		if (tex->managed) {
			GLuint	name = tex->name;
			glDeleteTextures(1, &name);
		}
	}
	
	tex->u		= 0;
	tex->name	= name;
	tex->format	= format;
	tex->width	= width		- 1;
	tex->height	= height	- 1;
	tex->mips	= 0;
	
	glBindTexture(GL_TEXTURE_2D, name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool Texture::Init(TexFormat format, int width, int height, int depth, int mips, uint16 flags) {
	glErrorCheck();
	if (!tex)
		tex = (*temp_textures)[depth == 6].alloc();

	bool	gen	= !tex->name;
	if (gen) {
		glGenTextures(1, (GLuint*)&tex->u);
		tex->managed = 1;
//		tex_ids.use(tex->name);
	}
	tex->format	= format;
	tex->width	= width		- 1;
	tex->height	= height	- 1;
	tex->mips	= mips		- 1;

	ogl_format	&f	= ogl_formats[format & _TEXF_BASEMASK];

	GLenum	target;
	if (tex->cube = depth == 6) {
		glBindTexture(target = GL_TEXTURE_CUBE_MAP, tex->name);
		if (mips < 2)
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		else if (!gen)
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		for (int face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++)
			glTexImage2D(face, 0, f.fmt, width, height, 0, f.fmt, f.type, NULL);
	} else {
		glBindTexture(target = GL_TEXTURE_2D, tex->name);
		glErrorCheck();
		if (mips < 2)
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		else if (!gen)
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, f.fmt, width, height, 0, f.fmt, f.type, NULL);
	}
	glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, mips - 1);
	
	if (abilities & (1 << HAS_shadow_samplers)) {
		if (format & TEXF_SHADOW) {
			glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		} else if (!gen) {
			glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_ZERO);
		}
	}
	glErrorCheck();
	return true;
}

void Texture::DeInit() {
	if (tex) {
		if (!tex->managed) {
			// we're not managing this id
			GLuint	name = tex->name;
			glDeleteTextures(1, &name);
			tex->name = 0;
		}
		(*temp_textures)[tex->cube].release((OGLTexture2*)tex.get());
	}
	tex = 0;
}

Texture::~Texture() {
	DeInit();
}

Surface Texture::GetSurface(int i) const {
	return Surface(tex->name, i, 0, (tex->width + 1) >> i, (tex->height + 1) >> i);
}

Surface Texture::GetSurface(CubemapFace f, int i) const {
	return Surface(tex->name, i, f + 1, (tex->width + 1) >> i, (tex->height + 1) >> i);
}

void Texture::SetTexFilter(TexFilter t) {
	GLenum	target = IsCube() ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

	glBindTexture(target, tex->name);
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST + (t & 3));
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST + ((t >> 2) & 1));
#ifdef PLAT_IOS
	glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, float(1 << (t >> 4)));
#endif
}

TextureData::TextureData(uint32 width, uint32 height, TexFormat format) : width(width), height(height), format(format) {
	pitch 	= TexturePitch(format, width + 1);
	data 	= malloc(pitch * TextureRows(format, height + 1));
}

TextureData::~TextureData() {
	ogl_format	&f	= ogl_formats[format];
	glTexImage2D(GL_TEXTURE_2D, 0, f.fmt, width + 1, height + 1, 0, f.fmt, f.type, data);
	free(data);
}

//-----------------------------------------------------------------------------
//	VertexBuffer/IndexBuffer
//-----------------------------------------------------------------------------

VertexDescription::VertexDescription(const OGLVertexElement *ve, size_t n, size_t stride, GLuint vbo) { PROFILE_FN
	graphics.SetBuffer<GL_ARRAY_BUFFER>(vbo);
	glGenVertexArrays(1, &vao);
	graphics.SetVertices(vao);

	for (const OGLVertexElement *p = ve; n-- && p->stream != 0xff; p++) {
		glVertexAttribPointer(p->attribute, ((p->type >> 4) & 3) + 1, (p->type & 0xf) + GL_BYTE, p->type & 0x80 ? GL_TRUE : GL_FALSE, stride, (void*)intptr_t(p->offset));
		glEnableVertexAttribArray(p->attribute);
	}
}

//-----------------------------------------------------------------------------
//	Shaders
//-----------------------------------------------------------------------------

static const struct _component { const char *name; USAGE usage; int indices; } components[] = {
	{"POSITION",		USAGE_POSITION,			1},
	{"NORMAL",			USAGE_NORMAL,			1},
	{"COLOR",			USAGE_COLOR,			2},
	{"TEXCOORD",		USAGE_TEXCOORD,			8},
	{"TANGENT",			USAGE_TANGENT,			1},
	{"BINORMAL",		USAGE_BINORMAL,			1},
	{"BLENDWEIGHT",		USAGE_BLENDWEIGHT,		1},
	{"BLENDINDICES",	USAGE_BLENDINDICES,		1},
};

GLuint compile_shader(const char *source, GLenum type, const char *prefix = 0) {
	glErrorCheck();
	GLuint	name = glCreateShader(type);

	if (prefix) {
		const GLchar *sources[] = {
			(GLchar*)prefix,
			(GLchar*)source
		};
		glShaderSource(name, 2, sources, NULL);
	} else {
		glShaderSource(name, 1, (const GLchar**)&source, NULL);
	}
	glErrorCheck();
	glCompileShader(name);

#if defined(ISO_DEBUG)
	GLint logLength;
	glGetShaderiv(name, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0) {
		char *log = (char*)malloc(logLength);
		glGetShaderInfoLog(name, logLength, &logLength, (GLchar*)log);
		ISO_TRACEF("Shader compile log:\n%s", log);
		free(log);
	}
#endif

	GLint status;
	glGetShaderiv(name, GL_COMPILE_STATUS, &status);
	if (status == 0) {
		glDeleteShader(name);
		name = 0;
	}
	return name;
}

inline const char *next_line(const char *p) {
	const char *n = strchr(p, '\n');
	return n ? n + 1 : p + strlen(p);
}

inline bool is_varchar(char c) {
	return	c <= '9' ? c >= '0'
		:	c <= 'Z' ? c >= 'A'
		:	c <= 'z' && (c >= 'a' || c == '_');
}

GLuint make_program(const char *ps_source, const char *vs_source) {
	glUseProgram(0);
	if (!ps_source)
		ps_source = "void main() { gl_FragColor = vec4(0,0,0,0); }";
	GLuint	ps	= compile_shader(ps_source, GL_FRAGMENT_SHADER,
		"#extension GL_OES_standard_derivatives : enable\n"
//		"#extension GL_EXT_frag_depth : enable\n"
//		"#define gl_FragDepth gl_FragDepthEXT\n"
		"#extension GL_EXT_shadow_samplers : require\n"
		"precision mediump float;\n"
	);
	if (!ps)
		return 0;

	if (str(vs_source).begins("precision mediump float;"))
		vs_source = str(vs_source).find('\n') + 1;
	GLuint	vs	= compile_shader(vs_source, GL_VERTEX_SHADER, "precision highp float;\n");
	if (!vs) {
		glDeleteShader(ps);
		return 0;
	}

	GLuint	prog = glCreateProgram();

	glAttachShader(prog, vs);
	glAttachShader(prog, ps);
	glErrorCheck();

	// Bind attribute locations. This needs to be done prior to linking.
	for (const char *p = vs_source; *p; p = next_line(p)) {
		if (str(p).begins("attribute ")) {
			fixed_string<16>	attr;
			const char	*a		= strchr(p + 10, ' ') + 1;
			const char	*b		= strchr(a, ';');
			int			index	= b[-1] >= '0' && b[-1] <= '9' ? b[-1] - '0' : 0;
			memcpy(attr, a, b - a);
			attr[b - a] = 0;
			for (int i = 0; i < num_elements(components); i++) {
				if (attr.begins(str(components[i].name)) && index < components[i].indices) {
					glBindAttribLocation(prog, components[i].usage + index, attr);
					glErrorCheck();
					break;
				}
			}
		}
	}

	glLinkProgram(prog);
	if (vs) {
		glDetachShader(prog, vs);
		glDeleteShader(vs);
	}
	if (ps) {
		glDetachShader(prog, ps);
		glDeleteShader(ps);
	}

	GLint	status, logLength;

#if defined(ISO_DEBUG)
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0) {
		char *log = (char*)malloc(logLength);
		glGetProgramInfoLog(prog, logLength, &logLength, (GLchar*)log);
		ISO_TRACEF("Shader link log:\n%s", log);
		free(log);
	}
#endif

	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	#if 0
	if (status) {
		glValidateProgram(prog);
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			char *log = (char*)malloc(logLength);
			glGetProgramInfoLog(prog, logLength, &logLength, (GLchar*)log);
			ISO_TRACE("Program validate log:\n%s", log);
			free(log);
		}
		glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
	}
	#endif

	if (status) {
		// map samplers
		int	num_uniforms, samp = 0;//_2d = 0, samp_cube = 0;
		glUseProgram(prog);
		glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &num_uniforms);
		for (int i = 0; i < num_uniforms; i++) {
			char		name[32];
			GLint		len;
			GLenum		type;
			glGetActiveUniform(prog, i, sizeof(name), 0, &len, &type, name);
			#ifdef __IPHONE_6_0
			if (type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE || type == GL_SAMPLER_2D_SHADOW) {
			#else
			if (type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE) {
			#endif
//				int	&samp = type == GL_SAMPLER_CUBE ? samp_cube : samp_2d;
				for (int loc = glGetUniformLocation(prog, name); len--; loc++) {
					if (samp < 8)
						glUniform1i(loc, samp++);
				}
			}
		}
	} else {
		glDeleteProgram(prog);
		prog = 0;
	}
	return prog;
}


void *make_parameters(GLuint prog) {
	GLint	total = 0;
	glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &total);


	fixed_string<1024>	names;
	uint16				regs[64], lens[64];
	char	*pname			= names;
	int		actual_total	= 0;
	int		samp			= 0;//_2d = 0, samp_cube = 0;

	for (int i = 0; i < total; i++) {
		uint32	reg;
		GLint	len;
		GLenum	gltype;
		int		j = 0;

		glGetActiveUniform(prog, i, 32, 0, (GLint*)&len, &gltype, pname);
		while (is_varchar(pname[j]))
			j++;

		uint8	type;
		reg	= 0;
		switch (gltype) {
			case GL_BOOL:		case GL_INT:
				type = SPT_INT1; break;
			case GL_BOOL_VEC2:	case GL_INT_VEC2:
				type = SPT_INT2; break;
			case GL_BOOL_VEC3:	case GL_INT_VEC3:
				type = SPT_INT3; break;
			case GL_BOOL_VEC4:	case GL_INT_VEC4:
				type = SPT_INT4; break;
			case GL_FLOAT:
				type = SPT_FLOAT1; break;
			case GL_FLOAT_VEC2:
				type = SPT_FLOAT2; break;
			case GL_FLOAT_VEC3:
				type = SPT_FLOAT3; break;
			case GL_FLOAT_VEC4:
				type = SPT_FLOAT4; break;
			case GL_FLOAT_MAT2:
				type = SPT_FLOAT22; break;
			case GL_FLOAT_MAT3:
				type = SPT_FLOAT33; break;
			case GL_FLOAT_MAT4:
				if (j == 5 && memcmp(pname, "bones", 5) == 0)
					len = 0;
				type = SPT_FLOAT44; break;
			case GL_SAMPLER_2D:
			#if defined PLAT_MAC || defined __IPHONE_6_0
			case GL_SAMPLER_2D_SHADOW:
			#endif
//			case GL_SAMPLER_CUBE:
				reg			= samp | (SPT_SAMPLER_2D << 11);
				samp		+= len;
				break;
			case GL_SAMPLER_CUBE:
				reg			= samp | (SPT_SAMPLER_CUBE << 11);
				samp		+= len;
				break;
		}
		if (!reg)
			reg = glGetUniformLocation(prog, pname) | (type << 11);

		if (pname[j]) {
			pname[j] = 0;

			fixed_string<32>	name2;
			while (++i < total) {
				GLenum	gltype;
				GLint	len;
				glGetActiveUniform(prog, i, sizeof(name2), 0, &len, &gltype, name2);
				if (!name2.begins(str(pname)))
					break;
			}
			--i;
		}
		pname += j + 1;
		regs[actual_total] = reg;
		lens[actual_total] = len;
		actual_total++;
	}

	void	*param_block = allocator32().alloc(pname - names + actual_total * 3 + 1, 4);
	uint8	*p			= (uint8*)param_block;

	pname	= names;
	*p++	= actual_total;

	for (int i = 0; i < actual_total; i++) {
		p[0] = regs[i] & 0xff;
		p[1] = regs[i] >> 8;
		p[2] = lens[i];
		p += 3;
		while (*p++ = *pname++);
	}
	return param_block;
}

void Init(OGLShader *x, void *physram) {
	x->Name() = ISO_VERIFY(make_program(x->ps ? (char*)physram + x->ps : 0, (char*)physram + x->vs));
	x->Params()	= make_parameters(x->Name());
}

void DeInit(OGLShader *x) {
	glDeleteProgram(x->Name());
	allocator32().free(x->Params());
	x->Name() = 0;
	x->Params() = 0;
}
//-----------------------------------------------------------------------------
//	Graphics
//-----------------------------------------------------------------------------

template<typename T> void make_new(T *&p) {
	p = new T;
}
template<typename T, int N> void make_new(T (*&p)[N]) {
	p = (T(*)[N])new (T[N]);
}

template<typename T> void make_isonew(T *&p) {
	p = new(allocator32()) T;
}
template<typename T, int N> void make_isonew(T (*&p)[N]) {
	p = (T(*)[N])new(allocator32()) (T[N]);
}

template<int N> inline bool check_extension(const char *p, const char (&e)[N]) {
	return memcmp(p, e, N) == 0 && p[N] <= ' ';
}
void Graphics::Init() {
	make_isonew(temp_textures);

	current_state	= &default_state;
	extensions		= (const char*)glGetString(GL_EXTENSIONS);
	abilities		= 0;

	if (extensions) {
		for (const char *p = extensions, *s; s = strchr(p, ' '); p = s + 1) {
			if (check_extension(p, "GL_EXT_shadow_samplers"))
				abilities |= 1 << HAS_shadow_samplers;
		}
	}
	glGenFramebuffers(1, &framebuffer);
}

void Graphics::BeginScene(GraphicsContext &ctx) { PROFILE_FN
	current_state = &ctx.state;
	glGetError();
	ctx.BeginScene();
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glErrorCheck();
}

void Graphics::EndScene(GraphicsContext &ctx) { PROFILE_FN
	default_state.Reset();
	current_state = &default_state;
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, GL_NONE);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
	glErrorCheck();
}

//-----------------------------------------------------------------------------
//	GraphicsContext
//-----------------------------------------------------------------------------

void GraphicsContext::BeginScene() {
	Disable<GL_SCISSOR_TEST>();
	glBlendEquation(state.rgb.op = GL_FUNC_ADD);
	glBlendFunc(state.rgb.src = GL_ONE, state.rgb.dest = GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(state.depth = DT_USUAL);
	state.a			= state.rgb;
	state.shader	= 0;
	clear(state.tex_2d);
	clear(state.tex_cube);
	clear(targets);
	cubemaps		= 0;
}

void GraphicsContext::ReadPixels(TexFormat format, const rect &rect, void* pixels) {
	ogl_format	&f	= ogl_formats[format];
	glReadPixels(rect.Left(), rect.Top(), rect.Width(), rect.Height(), f.fmt, f.type, pixels);
	glErrorCheck();
}

void GraphicsContext::Clear(param(colour) colour, bool zbuffer) { PROFILE_FN
//	glDiscardFramebufferEXT(GL_FRAMEBUFFER, zbuffer ? 3 : 1, discards);

/*	if (zbuffer) {
		if (state.Enable<State::EN_DEPTH_WRITE>())
			glDepthMask(1);
		glStencilMask(0xffffffff);
	}
*/
	if (zbuffer && state.Enable<State::EN_DEPTH_WRITE>())
		glDepthMask(true);

	glClearColor(colour.r, colour.g, colour.b, colour.a);
	SetMask(CM_ALL);
	glClear(zbuffer ? (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) : GL_COLOR_BUFFER_BIT);
	glErrorCheck();
}

void GraphicsContext::ClearZ() { PROFILE_FN
//	glDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, discards + 1);
	if (state.Enable<State::EN_DEPTH_WRITE>())
		glDepthMask(1);
	glStencilMask(0xffffffff);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glErrorCheck();
}

void GraphicsContext::SetWindow(const rect &rect) { PROFILE_FN
	if (memcmp(&rect, &window, sizeof(rect)) != 0) {
		window	= rect;
		glViewport(rect.Left(), rect.Top(), rect.Width(), rect.Height());
		glScissor(rect.Left(), rect.Top(), rect.Width(), rect.Height());
	}
	Enable<GL_SCISSOR_TEST>();
}

rect GraphicsContext::GetWindow() { PROFILE_FN
//	rect	rect;
//	glGetIntegerv(GL_SCISSOR_BOX, (int*)&rect);
//	return rect;
	return window;
}

void GraphicsContext::Resolve(RenderTarget i) {
}

GLenum glFBS;
void GraphicsContext::SetRenderTarget(const Surface& s, RenderTarget i) { PROFILE_FN
	Surface	&d = targets[i==RT_DEPTH];
	if (s.FromTex()) {
		if (s.Name() != d.Name() || s.Level() != d.Level() || s.Face() != d.Face()) {
			d = s;
			glFramebufferTexture2D(GL_FRAMEBUFFER, i, s.Face(), s.Name(), s.Level());
		}
	} else {
		if (s.Name() != d.Name()) {
			d = s;
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, i, GL_RENDERBUFFER, s.Name());
		}
	}
	if (s.Name()) {
		point	size = s.Size();
		Disable<GL_SCISSOR_TEST>();
		glViewport(0, 0, size.x, size.y);
		glScissor(0, 0, size.x, size.y);
		window = rect(0, 0, size.x, size.y);

	} else if (i == RT_COLOUR0) {
		SetMask(CM_NONE);

	} else if (i == RT_DEPTH) {
		if (state.Disable<State::EN_DEPTH_WRITE>())
			glDepthMask(0);
		Disable<GL_DEPTH_TEST>();
	}
	glFBS = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glErrorCheck();
}

void GraphicsContext::SetTexture(const Texture &tex, int i) { PROFILE_FN
	SetActiveTex(i & 0xffff);
	if (tex.IsCube()) {
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
		cubemaps	|= 1 << i;
	} else {
		glBindTexture(GL_TEXTURE_2D, tex);
		cubemaps	&= ~(1 << i);
	}
	glErrorCheck();
}

void GraphicsContext::SetSamplerState(int i, TexState type, int32 value) { PROFILE_FN
	SetActiveTex(i);
	glTexParameteri(TexTarget(i), type, value);
	glErrorCheck();
}

int32 GraphicsContext::GetSamplerState(int i, TexState type) { PROFILE_FN
	GLint	value;
	SetActiveTex(i);
	glGetTexParameteriv(TexTarget(i), type, &value);
	glErrorCheck();
	return value;
}

void GraphicsContext::GenerateMips(const Texture &tex) { PROFILE_FN
	GLenum	target = tex.IsCube() ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
	glBindTexture(target, tex);
	glGenerateMipmap(target);
}

void GraphicsContext::DrawVertices(PrimType prim, uint32 start, uint32 count) { PROFILE_FN
	glDrawArrays(prim, start, count);
	glErrorCheck();
}
void GraphicsContext::DrawIndexedVertices(PrimType prim, uint32 start, uint32 count) { PROFILE_FN
	glDrawElements(prim, count, GL_UNSIGNED_SHORT, (void*)(start * sizeof(uint16)));
	glErrorCheck();
}
void GraphicsContext::DrawIndexedVertices(PrimType prim, uint32 min_index, uint32 num_verts, uint32 start, uint32 count) { PROFILE_FN
	glDrawElements(prim, count, GL_UNSIGNED_SHORT, (void*)(start * sizeof(uint16)));
	glErrorCheck();
}

void GraphicsContext::SetShader(const OGLShader &s) { PROFILE_FN
	if (state.shader != s)
		glUseProgram(state.shader = s);
}

void GraphicsContext::SetShaderConstants(ShaderReg reg, const void *values) { PROFILE_FN
	if (!values)
		return;

	GLint	loc		= reg.reg;
	GLsizei	count	= reg.count;
	switch (reg.type) {
		case SPT_INT1:		glUniform1iv(loc, count, (const int*)values); break;
		case SPT_INT2:		glUniform2iv(loc, count, (const int*)values); break;
		case SPT_INT3:		glUniform3iv(loc, count, (const int*)values); break;
		case SPT_INT4:		glUniform4iv(loc, count, (const int*)values); break;
		case SPT_FLOAT1:	glUniform1fv(loc, count, (const float*)values); break;
		case SPT_FLOAT2:	glUniform2fv(loc, count, (const float*)values); break;
		case SPT_FLOAT3:	glUniform3fv(loc, count, (const float*)values); break;
		case SPT_FLOAT4:	glUniform4fv(loc, count, (const float*)values); break;
		case SPT_FLOAT22:	glUniformMatrix2fv(loc, count, GL_FALSE, (const float*)values); break;
		case SPT_FLOAT33:	glUniformMatrix3fv(loc, count, GL_FALSE, (const float*)values); break;
		case SPT_FLOAT44:	glUniformMatrix4fv(loc, count, GL_FALSE, (const float*)values); break;

		case SPT_SAMPLER_2D:
			for (const Texture *tex = (const Texture*)values; count--; loc++, tex++) {
				ISO_ASSERT(loc < 8);
				if (state.tex_2d[loc] != *tex) {
					SetActiveTex(loc);
					glBindTexture(GL_TEXTURE_2D, state.tex_2d[loc] = *tex);
				}
//				glBindTexture(tex.IsCube() ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, tex);
//				glBindTexture(GL_TEXTURE_2D, ((const Texture*)values)[i]);
			}
			break;
		case SPT_SAMPLER_CUBE:
			for (const Texture *tex = (const Texture*)values; count--; loc++, tex++) {
				ISO_ASSERT(loc < 8);
				if (state.tex_cube[loc] != *tex) {
					SetActiveTex(loc);
					glBindTexture(GL_TEXTURE_CUBE_MAP, state.tex_cube[loc] = *tex);
				}
			}
			break;
	}
//	glErrorCheck();
}

void GraphicsContext::SetUVMode(int i, UVMode t) {
}
void GraphicsContext::SetTexFilter(int i, TexFilter t) {
}

void GraphicsContext::SetDepthBias(float bias, float slope)	{
	if (bias || slope) {
		if (bias != state.bias || slope != state.slope)
//			glPolygonOffset(-(state.slope = slope), -(state.bias = bias));
			glPolygonOffset((state.bias = bias), (state.slope = slope));
		Enable<GL_POLYGON_OFFSET_FILL>();
	} else {
		Disable<GL_POLYGON_OFFSET_FILL>();
	}
}

//-----------------------------------------------------------------------------
//	_ImmediateStream
//-----------------------------------------------------------------------------

_ImmediateStream::_ImmediateStream(GraphicsContext &ctx, PrimType _prim, int _count, size_t tsize, const VertexDescription2 &vd) : prim(_prim), count(_count) { PROFILE_FN
	ctx.SetVertices(vd);
	ctx.SetVertexBuffer(vd);
	glBufferData(GL_ARRAY_BUFFER, count * tsize, 0, GL_DYNAMIC_DRAW);
	p = glMapBufferRange(GL_ARRAY_BUFFER, 0, 0, GL_MAP_WRITE_BIT);
	glErrorCheck();
}

_ImmediateStream::~_ImmediateStream() { PROFILE_FN
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glDrawArrays(prim, 0, count);
	glErrorCheck();
}

} // namespace iso
