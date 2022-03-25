#ifndef GTF_H
#define GTF_H

#include "bitmap.h"
#include "platforms/ps3/shared/rsx_regs.h"

namespace iso {
using namespace ps3::gpu;

// Bigendian versions of Texture File Header structures

struct CellGtfFileHeader_be : bigendian_types {
	uint32		version;		// Version (Correspond to dds2gtf converter version)
	uint32		size;			// Total size of Texture. (Excluding size of header & attribute)
	uint32		numTexture;		// Number of textures in this file.
};

struct CellGcmTexture_be : bigendian_types {
	uint8		format;
	uint8		mipmap;
	uint8		dimension;
	uint8		cubemap;
	uint32		remap;
	uint16		width;
	uint16		height;
	uint16		depth;
	uint8		location;
	uint8		padding;
	uint32		pitch;
	uint32		offset;
};

struct CellGtfTextureAttribute_be : bigendian_types {
	uint32		id;				// Texture ID.
	uint32		offsetToTex;	// Offset to texture from begining of file.
	uint32		textureSize;	// Size of texture.
	CellGcmTexture_be tex;		// Texture structure defined in GCM library.
};

enum {
	CELL_GCM_TEXTURE_COMPRESSED_B8R8_G8R8_RAW = 0x8D,
	CELL_GCM_TEXTURE_COMPRESSED_R8B8_R8G8_RAW = 0x8E,
};

#define GTF_REMAP_0			4
#define GTF_REMAP_1			5
#define GTF_REMAP1(C)		(C < 4 ? (C | CELL_GCM_TEXTURE_REMAP_REMAP << 8) : unsigned(C - 4) << 8)
#define GTF_REMAP0(X,Y,Z,W)	(GTF_REMAP1(X) | GTF_REMAP1(Y) << 2 | GTF_REMAP1(Z) << 4 | GTF_REMAP1(W) << 6)
#define GTF_REMAP(R,G,B,A)	GTF_REMAP0(A,R,G,B)

class GTF {
	static void		Swizzle(uint32 width, uint32 height, uint32 depth, void *srce, void *dest, uint32 bpp);
	static void		DeSwizzle(uint32 width, uint32 height, uint32 depth, void *srce, void *dest, uint32 bpp);
public:
	static BaseTextureFormat	GetBaseFormat(uint8 f);
	static bool		IsSwizzle(uint8 format);
	static uint32	GetScans(BaseTextureFormat format, uint32 height);
	static uint32	GetSize(BaseTextureFormat format, uint32 width, uint32 height);
	static bool		IsDxtn(BaseTextureFormat format);
	static bool		IsHDR(BaseTextureFormat format);
	static bool		IsSwizzlable(const CellGcmTexture_be &tex);
	static void		Convert(
		uint32 width, uint32 height, uint32 depth,
		void *srce, uint32 srce_pitch, uint8 srce_format, remap_chans srce_remap,
		void *dest, uint32 dest_pitch, uint8 dest_format, remap_chans dest_remap,
		bool nodither = false
	);
};

} //namespace iso

#endif//GTF_H
