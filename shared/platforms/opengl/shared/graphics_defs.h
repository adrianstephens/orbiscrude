#ifndef GRAPHICS_DEFS_H
#define GRAPHICS_DEFS_H

//#define ISO_PREFIX		OGL

#include "base/defs.h"

namespace iso {

enum TexFormatFlags {
	TEXF_SHADOW	= 0x80,
};

enum TexFormat {
	_TEXF_BASEMASK		= 0x7f,

	TEXF_UNKNOWN		= 0,

	TEXF_R8,
	TEXF_R8G8,
	TEXF_R8G8B8,
	TEXF_R8G8B8A8,
	TEXF_L8,
	TEXF_A8,
	TEXF_L8A8,

	TEXF_R16F,
	TEXF_R16G16F,
	TEXF_R16G16B16F,
	TEXF_R16G16B16A16F,
	TEXF_L16F,
	TEXF_A16F,
	TEXF_L16A16F,

	TEXF_R32F,
	TEXF_R32G32F,
	TEXF_R32G32B32F,
	TEXF_R32G32B32A32F,
	TEXF_L32F,
	TEXF_A32F,
	TEXF_L32A32F,

	TEXF_R5G6B5,
	TEXF_R4G4B4A4,
	TEXF_R5G5B5A1,
	TEXF_R4G2B2,
	TEXF_PVRTC2,
	TEXF_PVRTC4,

	_TEXF_DEPTH,
	TEXF_D16		= _TEXF_DEPTH,
	TEXF_D24S8,
	TEXF_D32,
	TEXF_D32F,
};

inline TexFormat operator|(TexFormat a, TexFormatFlags b)	{ return TexFormat((uint32)a | b); }

struct OGLTexture  {
	uint32	width:11, height:11, format:5, mips:4, cube:1;
	union {
		struct { uint32 depth0:5, offset0:26, clamp0:1; };			// in file
		struct { uint32 name:24, depth:5, managed:1, unused:2; };	// runtime
		uint32	u;
	};
};

struct OGLVertexElement {
	uint8	stream;
	uint8	offset;
	uint8	type;
	uint8	attribute;
	OGLVertexElement() {}
	OGLVertexElement(int _offset, int _type, int _attribute, int _stream = 0) :
		stream(_stream), offset(_offset), type(_type), attribute(_attribute)	{}
};

struct OGLBuffer {
	uint32	size:24, format:8;
	uint32	offset;
};

} // namespace iso

#endif	// GRAPHICS_DEFS_H
