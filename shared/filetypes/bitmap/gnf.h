#ifndef GNF_H
#define GNF_H

#include "codec/texels/dxt.h"
#include "bitmap.h"
#include "base/bits.h"
#include "systems/conversion/channeluse.h"

#include "..\..\platforms\gcn\gpu_regs.h"
#include "..\..\platforms\gcn\gpu_tile.h"

namespace GNF {
	using namespace iso;
	using namespace gcn;

	struct Texture : TextureRecord256 {
		uint32		initAs2dArray(uint32 width, uint32 height, uint32 numSlices, uint32 numMipLevels, Format format, TileMode tileModeHint, uint32 log2Fragments, bool isCubemap);
		uint32		initAs3d(uint32 width, uint32 height, uint32 depth, uint32 numMipLevels, Format format, TileMode tileModeHint);

		uint32		initAs1d(uint32 width, uint32 numMipLevels, Format format, TileMode tileModeHint) {
			return initAs2dArray(width, 1, 1, numMipLevels, format, tileModeHint, 0, false);
		}
		uint32		initAs1dArray(uint32 width, uint32 numSlices, uint32 numMipLevels, Format format, TileMode tileModeHint) {
			return initAs2dArray(width, 1, numSlices, numMipLevels, format, tileModeHint, 0, false);
		}
		uint32		initAs2d(uint32 width, uint32 height, uint32 numMipLevels, Format format, TileMode tileModeHint, uint32 log2Fragments = 0) {
			return initAs2dArray(width, height, 1, numMipLevels, format, tileModeHint, log2Fragments, false);
		}
		uint32		initAsCubemap(uint32 width, uint32 height, uint32 numMipLevels, Format format, TileMode tileModeHint) {
			return initAs2dArray(width, height, 1, numMipLevels, format, tileModeHint, 0, true);
		}
		uint32		initAsCubemapArray(uint32 width, uint32 height, uint32 numSlices, uint32 numMipLevels, Format format, TileMode tileModeHint) {
			return initAs2dArray(width, height, numSlices, numMipLevels, format, tileModeHint, 0, true);
		}

		uint32		init(const TextureRecord256 &tex);
		uint32		init(const RenderTargetRecord &render);
		uint32		init(const DepthTargetRecord &depth);
	};

	struct Header {
		enum { VERSION = 1, MAGIC = 0x20464E47 };
		uint32	magic;
		uint32	content_size;
		uint8	version;
		uint8	num_textures;
		uint8	alignment;	// log2 of max alignment
		uint8	unused;
		uint32	stream_size;
		Texture	textures[0];

		uint32	Init(uint32 n) {
			magic		= MAGIC;
			version		= VERSION;
			num_textures = n;
			content_size = sizeof(Texture) * n + sizeof(Header) - 8;
			alignment	= 8;
			unused		= 0;
			return stream_size = content_size + 8;
		}
		void	Add(GNF::Texture &tex) {
			tex.setBase(stream_size - content_size - 8);
			tex.base_hi	= 8;
			stream_size += tex.getSize();
		}
	};

	Format			ComputeFormat(ISO_ptr<bitmap> bm);
	const void*		Read(const block<ISO_rgba, 3> &block, const void *srce, Format format, TileMode tile, TextureType type, uint32 pitch, bool pow2pad, int frags = 1, bool rearrange = true);
	const void*		Read(const block<HDRpixel, 3> &block, const void *srce, Format format, TileMode tile, TextureType type, uint32 pitch, bool pow2pad, int frags = 1, bool rearrange = true);
	malloc_block	Write(const block<ISO_rgba, 3> &block, Format format, TileMode tile, TextureType type, bool pow2pad);
	malloc_block	Write(const block<HDRpixel, 3> &block, Format format, TileMode tile, TextureType type, bool pow2pad);

	ChannelUse::chans	ConvertChannels(Format::Channels c);
	Format::Channels	ConvertChannels(ChannelUse::chans c);

//	template<typename T> void RearrangeChannels(block<T, 2> &block, Format::Channels chans);
//	template<typename T> void RearrangeChannels(block<T, 3> &block, Format::Channels chans);

} // namespace GNF

#endif //GNF_H

