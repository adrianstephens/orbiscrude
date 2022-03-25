#include "iso/iso_files.h"
#include "bitmap.h"
#include "codec/texels/dxt.h"

#include <windows.h>
#include "d3d9.h"

#if 1
#define __XBOXMATH2_H__
typedef __m128 XMVECTOR;
#endif
#include "xgraphics.h"

//-----------------------------------------------------------------------------
//	XBox resource
//-----------------------------------------------------------------------------

using namespace iso;

class XPR {
	struct XPR_HEADER {
		uint32be	dwMagic;
		uint32be	header_size;
		uint32be	dwDataSize;
	};

	struct XPR_RESOURCE {
		uint32be	type;
		uint32be	offset;
		uint32be	size;
		uint32be	name;
	};

	struct RESOURCE {
		uint32		type;
		uint32		offset;
		uint32		size;
		string		name;
	};

	struct BLOCK {
		uint32		size;
		uint32		alloc;
		void		*buffer;

		BLOCK() : size(0), alloc(0), buffer(0)	{}
		~BLOCK() { aligned_free(buffer); }

		void	Init(void *_buffer, uint32 _size, uint32 _alloc) {
			size	= _size;
			alloc	= _alloc;
			buffer	= _buffer;
		}

		void	Add(void *data, uint32 extra) {
			uint32	size2 = size + extra;
			if (size2 > alloc) {
				alloc	= max(size2, alloc * 2);
				buffer = aligned_realloc(buffer, alloc, 16);
			}
			memcpy((char*)buffer + size, data, extra);
			size = size2;
		}

		void	Align(uint32 align) {
			if (int fill = size % align) {
				malloc_block	temp(fill = align - fill);
				memset(temp, 0xDEAD, fill);
				Add(temp, fill);
			}
		}

	};

	dynamic_array<RESOURCE>	resources;
	BLOCK		header, data;

public:
	XPR()		{}
	XPR(istream_ref file);

	uint32		Count()					const	{ return uint32(resources.size());	}
	tag2		GetResourceID(int i)	const	{ return tag2(resources[i].name);	}
	void*		GetResource(int i, uint32 &type, uint32 &size) const {
		RESOURCE	&r = resources[i];
		type	= r.type;
		size	= r.size;
		return (uint8*)header.buffer + r.offset;
	}
	void*		GetPhysical(uint32 base_offset, uint32 base_size) const {
		return (uint8*)data.buffer + base_offset;
	}
	void		Write(ostream_ref file);
};

void XPR::Write(ostream_ref file) {
	XPR_HEADER xprh;
	xprh.dwMagic = 'XPR2';

	// Compute size of resource table

	// Add offsets table
	size_t	num		= resources.size();
	size_t	offsets = num * sizeof(XPR_RESOURCE) + 2 * sizeof(uint32);
	size_t	strings	= offsets;
	// Add string space
	for (int i = 0; i < num; i++ )
		offsets += strlen(resources[i].name) + 1;

	// Pad to 16 bytes
	size_t	offsets_pad	= (16 - (offsets % 16)) % 16;
	offsets += offsets_pad;

	// Calculate header size
	size_t	header_size	= offsets + header.size;

	// We may need to pad the file to a sector multiple for the start of the data block
	size_t header_pad = 0;
	if (header_size % 2048) {
		header_pad = 2048 - (header_size % 2048);
		header_size += header_pad;
	}
	xprh.header_size	= uint32(header_size);
	xprh.dwDataSize		= data.size;

	// Write out header stuff
	file.write(xprh);

	// Write out the number of resources
	file.write(uint32be(uint32(num)));

	// Write out the offsets of each resource
	for (int i = 0; i < num; i++) {
		RESOURCE		&rin	= resources[i];
		XPR_RESOURCE	rout;
		rout.type		= rin.type;
		rout.offset		= uint32(rin.offset + offsets);
		rout.size		= rin.size;
		rout.name		= uint32(strings);
		file.write(rout);
		strings	+= strlen(rin.name) + 1;
	}

	// Write a NULL terminator
	file.write(uint32(0));

	// Write the string table
	for (int i = 0; i < num; i++)
		file.write(resources[i].name);

	// Add padding
	if (offsets_pad) {
		uint8 pad[16] = {0};
		file.writebuff(pad, offsets_pad);
	}

	file.writebuff(header.buffer, header.size);

	// Write out any remaining padding
	if (header_pad) {
		uint8 pad[2048] = {0};
		file.writebuff(pad, header_pad);
	}

	file.writebuff(data.buffer, data.size);
}

XPR::XPR(istream_ref file) {
	XPR_HEADER	xprh		= file.get();
	if (xprh.dwMagic != 'XPR2')
		return;

	uint32			hs		= xprh.header_size;
	void			*h		= aligned_alloc(hs, 16);
	file.readbuff(h,  hs);

	uint32			num		= *(uint32be*)h;
	XPR_RESOURCE	*rin	= (XPR_RESOURCE*)((uint32*)h + 1);
	uint32			offset	= align(rin[num - 1].name + uint32(strlen((char*)h) + uint32(rin[num - 1].name)) + 1, 16);

	for (int i = 0; i < num; i++, rin++) {
		RESOURCE	*rout = new (resources) RESOURCE;
		rout->type		= rin->type;
		rout->offset	= rin->offset - offset;
		rout->size		= rin->size;
		rout->name		= (char*)h + uint32(rin->name);
	}

	uint32	ds	= xprh.dwDataSize;
	void	*d	= aligned_alloc(ds, 16);
	file.readbuff(d,  ds);
	data.Init(d, ds, ds);

	memcpy(h, (char*)h + offset, hs - offset);
	header.Init(h, hs - offset, hs);
}

//-----------------------------------------------------------------------------
//	XPRFileHandler
//-----------------------------------------------------------------------------

class XPRFileHandler : public FileHandler {
	const char*		GetExt() override { return "xpr";				}
	const char*		GetDescription() override { return "XBox resource";	}

	ISO_ptr<bitmap>		GetBitmap(const XPR &xpr, tag2 id, D3DBaseTexture *tex);

	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} xpr;

ISO_ptr<void> XPRFileHandler::Read(tag id, istream_ref file) {
	XPR	xpr(file);
	int	count = xpr.Count();
	if (count == 0)
		return ISO_NULL;

	ISO_ptr<anything>	p(id, count);
	for (int i = 0; i < count; i++) {
		uint32	type, size;
		void	*b = xpr.GetResource(i, type, size);
		tag2	id = xpr.GetResourceID(i);
		switch (type) {
			case 'USER':	//RESOURCETYPE_USERDATA
				break;
			case 'TX2D': {	//RESOURCETYPE_TEXTURE
				D3DBaseTexture	tex;
				copy_n((uint32be*)b, (uint32*)&tex, sizeof(tex) / 4);
				(*p)[i] = GetBitmap(xpr, id, &tex);
				break;
			}
			case 'TXCM':	//RESOURCETYPE_CUBEMAP
				break;
			case 'TX3D':	//RESOURCETYPE_VOLUMETEXTURE
				break;
			case 'VBUF':	//RESOURCETYPE_VERTEXBUFFER
				break;
			case 'IBUF':	//RESOURCETYPE_INDEXBUFFER
				break;
			case 'VSHD':	//RESOURCETYPE_VERTEXSHADER
				break;
			case 'PSHD':	//RESOURCETYPE_PIXELSHADER
				break;
			case 0xffffffff://RESOURCETYPE_EOF
			default:
				break;
		}
	}
	return p;
}

ISO_ptr<bitmap> XPRFileHandler::GetBitmap(const XPR &xpr, tag2 id, D3DBaseTexture *tex) {
	XGTEXTURE_DESC	base_desc;
	UINT			base_offset, base_size, mip_offset, mip_size;

	XGGetTextureDesc(tex, 0, &base_desc);
	XGGetTextureLayout(tex, &base_offset, &base_size, NULL, NULL, 0, &mip_offset, &mip_size, NULL, NULL, 0);

	if (base_desc.ResourceType == D3DRTYPE_CUBETEXTURE)
		base_desc.Depth = 6;

	ISO_ptr<bitmap>	bm(id);
	bm->Create(base_desc.Width, base_desc.Height * base_desc.Depth, base_desc.ResourceType == D3DRTYPE_VOLUMETEXTURE ? BMF_VOLUME : 0, base_desc.Depth);

	D3DFORMAT	dst_format	= D3DFORMAT(MAKED3DFMT(GPUTEXTUREFORMAT_8_8_8_8, GPUENDIAN_8IN32, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_RGBA));
	D3DFORMAT	src_format  = D3DFORMAT(base_desc.Format & ~D3DFORMAT_TILED_MASK);
	bool		tiled		= !!(base_desc.Format & D3DFORMAT_TILED_MASK);
	float		alpha_threshold = 0.5f;
	uint32		copy_flags	= 0;
	uint32		tile_flags	= 0;	// XGTILE_NONPACKED | XGTILE_BORDER
	uint32		pitch		= sizeof(ISO_rgba) * base_desc.Width;
	void		*buffer		= xpr.GetPhysical(base_offset, base_size);
	void		*untiled;

	if (tiled) {
		untiled		= iso::malloc(base_size);
		XGUntileTextureLevel(base_desc.Width, base_desc.Height * base_desc.Depth, 0,
			XGGetGpuFormat(src_format), tile_flags,
			untiled, base_desc.RowPitch, NULL, buffer, NULL);
	} else {
		untiled		= buffer;
	}

	XGCopySurface(bm->ScanLine(0),
		pitch, base_desc.Width, base_desc.Height * base_desc.Depth, dst_format, NULL,
		untiled, base_desc.RowPitch, src_format, NULL,
		copy_flags, alpha_threshold
	);

	if (tiled)
		iso::free(untiled);
	return bm;
}
