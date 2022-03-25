#include "gnf.h"
#include "bitmapfile.h"
#include "base/algorithm.h"

//-----------------------------------------------------------------------------
//	Playstation 4 bitmaps
//-----------------------------------------------------------------------------

//#define GPU_ADDRESS_FIRSTSAMPLE

namespace GNF {


template<typename T, int S, int F> struct FMask {
	enum {N = F + (S == F), B0 = klog2<N>, B = B0 == 3 ? 4 : B0, M = (1 << B) - 1};
	T	t;
	operator	ISO_rgba() const {
		T	x	= t;
		int	m	= 0;
		for (int f = F; f--; x >>= B)
			m = max(m, x & M);
		return m * 255 / N;
	}
};

uint32 Texture::init(const RenderTargetRecord &render) {
	return initAs2d(render.getPitch(), render.getHeight(), 0, render.getFormat(), render.getTileMode());
}

uint32 Texture::init(const DepthTargetRecord &depth) {
	return initAs2d(depth.getPitch(), depth.getHeight(), 0, depth.getZFormat(), depth.getZTileMode());
}

uint32 Texture::initAs2dArray(uint32 w, uint32 h, uint32 s, uint32 m, Format f, TileMode tile, uint32 log2Fragments, bool cube) {
	clear(*this);
	setSize(w, h, s);
	setPitch(CalcPitch(w, h, f.getDataFormat(), tile));
	setFormat(f);
	setMipLevelRange(0, m);
	setLog2Fragments(log2Fragments);
	setTileMode(tile);
	setType(cube
		? Tex_Cubemap
		: log2Fragments == 0
		? (h == 1
			? (s == 1 ? Tex_1d : Tex_1dArray)
			: (s == 1 ? Tex_2d : Tex_2dArray)
		) : (s == 1 ? Tex_2dMsaa : Tex_2dArrayMsaa)
	);
	pow2pad = m > 1;
	return size = CalcSize(*this);
}

uint32 Texture::initAs3d(uint32 w, uint32 h, uint32 d, uint32 m, Format f, TileMode tile) {
	clear(*this);
	setSize(w, h, d);
	setPitch(CalcPitch(w, h, f.getDataFormat(), tile));
	setFormat(f);
	setMipLevelRange(0, m);
	setTileMode(tile);
	setType(Tex_3d);
	pow2pad = m > 1;
	return size = CalcSize(*this);
}

uint32 Texture::init(const TextureRecord256 &tex) {
	TextureRecord256::operator=(tex);
	return size = CalcSize(*this);
}

ChannelUse::chans ConvertChannels(Format::Channels c) {
	static const uint8 chan_map[] = {ChannelUse::ZERO, ChannelUse::ONE, 0xff, 0xff, ChannelUse::RED, ChannelUse::GREEN, ChannelUse::BLUE, ChannelUse::ALPHA};
	return ChannelUse::chans(
		chan_map[c.x],
		chan_map[c.y],
		chan_map[c.z],
		chan_map[c.w]
	);
}

Format::Channels ConvertChannels(ChannelUse::chans c) {
	static const uint8 chan_map[] = {Format::ChanX, Format::ChanY, Format::ChanZ, Format::ChanW, Format::Chan0, Format::Chan1};
	return Format::Channels(
		chan_map[c.r],
		chan_map[c.g],
		chan_map[c.b],
		chan_map[c.a]
	);
}

} //namespace GNF

#ifndef _SCE_GPU_ADDRESS_H

namespace GNF {

template<typename D, typename S> static void *Get(const block<D, 3> &dest, S *srce) {
	uint32	width	= dest.size<1>(), height = dest.size<2>(), depth = dest.size<3>();
	copy(make_block(srce, width, height, depth), dest);
	return srce + width * height * depth;
}

template<typename D, typename S> static void *Get(const block<D, 3> &dest, S *srce, TileMode tile, uint32 pitch, bool pow2pad, int frags) {
	uint32	width	= dest.size<1>(), height = dest.size<2>(), depth = dest.size<3>();
	uint32	slices	= pow2pad ? next_pow2(depth) : depth;

	switch (gcn::TileModeReg::getReg(tile).array) {
		//Linear
		case gcn::TileModeReg::LinearGeneral: {
			uint32	slice	= pitch * height;
			copy(make_strided_block(srce, width, pitch, height, slice, depth), dest);
			return srce + slice * slices;
		}
		//Linear Aligned
		case gcn::TileModeReg::LinearAligned: {
			gcn::TilerLinear	tiler(tile, pitch, height, sizeof(S));
			uint32	slice	= tiler.elems_per_row * tiler.rows_per_slice;
			copy(make_strided_block(srce, tiler.elems_per_row, pitch, height, slice, depth), dest);
			return srce + slice * slices;
		}
		//1D Tiled
		case gcn::TileModeReg::Tiled1dThin: case gcn::TileModeReg::Tiled1dThick: {
			gcn::Tiler1d	tiler(tile, pitch, height, sizeof(S));
			for (uint32 z = 0; z < depth; z++) {
				for (uint32 y = 0; y < height; y++) {
					gcn::Tiler1::row_iterator	t = tiler.row_begin(y, z);
					for (D *d = dest[z][y].begin(), *de = dest[z][y].end(); d != de; ++d, ++t)
						*d = srce[t];
				}
			}
			return srce + tiler.getElementsPerSlice() * round_pow2(slices, tiler.tile_zshift);
		}
		//2D Tiled
		default: {
			gcn::Tiler2d	tiler(tile, pitch, height, sizeof(S), frags);
			for (uint32 z = 0; z < depth; z++) {
				for (uint32 y = 0; y < height; y++) {
					for (uint32 x = 0; x < width; x++)
						dest[z][y][x] = srce[tiler.getElementOffset(x, y, z, 0)];
				}
			}
			return srce + tiler.getElementsPerSlice() * round_pow2(slices, tiler.tile_zshift);
		}
	}
}

template<typename D, typename S> static const void *GetBC(const block<D, 3> &dest, const S *srce, TileMode tile, uint32 pitch, bool pow2pad) {
	uint32	width	= (dest.size<1>() + 3) / 4, height = (dest.size<2>() + 3) / 4, depth = dest.size<3>();
	uint32	slices	= pow2pad ? next_pow2(depth) : depth;
	pitch = (pitch + 3) / 4;

	switch (gcn::TileModeReg::getReg(tile).array) {
		//Linear
		case gcn::TileModeReg::LinearGeneral: {
			uint32	slice	= pitch * height;
			copy(make_strided_block(srce, width, pitch, height, slice, depth), dest);
			return srce + slice * slices;
		}
		//Linear Aligned
		case gcn::TileModeReg::LinearAligned: {
			gcn::TilerLinear	tiler(tile, pitch, height, sizeof(S));
			uint32	slice	= tiler.elems_per_row * tiler.rows_per_slice;
			copy(make_strided_block(srce, width, tiler.elems_per_row, height, slice, depth), dest);
			return srce + slice * slices;
		}
		//1D Tiled
		case gcn::TileModeReg::Tiled1dThin: case gcn::TileModeReg::Tiled1dThick: {
			gcn::Tiler1d	tiler(tile, pitch, height, sizeof(S));
			for (uint32 z = 0; z < depth; z++) {
				for (uint32 y = 0; y < height; y++) {
					gcn::Tiler1::row_iterator t = tiler.row_begin(y, z);
					for (uint32 x = 0; x < width; ++x, ++t)
						srce[t].Decode(dest[z].sub<1>(x * 4, 4).sub<2>(y * 4, 4));
				}
			}
			return srce + tiler.getElementsPerSlice() * round_pow2(slices, tiler.tile_zshift);
		}
		//2D Tiled
		default: {
			gcn::Tiler2d	tiler(tile, pitch, height, sizeof(S), 1);
			for (uint32 z = 0; z < depth; z++) {
				for (uint32 y = 0; y < height; y++) {
					for (uint32 x = 0; x < width; x++)
						srce[tiler.getElementOffset(x, y, z, 0)].Decode(dest[z].sub<1>(x * 4, 4).sub<2>(y * 4, 4));
				}
			}
			return srce + tiler.getElementsPerSlice() * round_pow2(slices, tiler.tile_zshift);
		}
	}
}

template<typename D, typename S> static malloc_block Put(const block<S, 3> &srce, TileMode tile) {
	uint32				width	= srce.size<1>(), height = srce.size<3>(), depth = srce.size<3>();
	gcn::Tiler1d	tiler(tile, width, height, sizeof(D));
	malloc_block		data(tiler.getElementsPerSlice() * round_pow2(depth, tiler.tile_zshift) * sizeof(D));

	D	*dest = data;
	for (uint32 z = 0; z < depth; z++) {
		for (uint32 y = 0; y < height; y++) {
			gcn::Tiler1::row_iterator	t = tiler.row_begin(y, z);
			for (S *s = srce[z][y].begin(), *se = srce[z][y].end(); s != se; ++s, ++t)
				dest[t] = *s;
		}
	}
	return data;
}

template<typename D> static malloc_block PutBC(const block<ISO_rgba, 3> &srce, TileMode tile) {
	uint32				width	= max(srce.size<1>() / 4, 1), height = max(srce.size<2>() / 4, 1), depth = srce.size<3>();
	gcn::Tiler1d	tiler(tile, width, height, sizeof(D));
	malloc_block		data(tiler.getElementsPerSlice() * round_pow2(depth, tiler.tile_zshift) * sizeof(D));

	D	*dest = data;
	for (uint32 z = 0; z < depth; z++) {
		for (uint32 y = 0; y < height; y++) {
			gcn::Tiler1::row_iterator t = tiler.row_begin(y, z);
			for (uint32 x = 0; x < width; ++x, ++t)
				dest[t].Encode(srce[z].sub<1>(x * 4, 4).sub<2>(y * 4, 4));
		}
	}
	return data;
}
const void *Read(const block<ISO_rgba, 3> &block, const void *srce, Format format, TileMode tile, uint32 pitch, bool pow2pad, int frags, bool rearrange) {
	switch (format.getDataFormat()) {
		case Format::_Bc1:			srce = GetBC(block, (BC<1>*)srce, tile, pitch, pow2pad); break;
		case Format::_Bc2:			srce = GetBC(block, (BC<2>*)srce, tile, pitch, pow2pad); break;
		case Format::_Bc3:			srce = GetBC(block, (BC<3>*)srce, tile, pitch, pow2pad); break;
		case Format::_Bc4:			srce = GetBC(block, (BC<4>*)srce, tile, pitch, pow2pad); break;
		case Format::_Bc5:			srce = GetBC(block, (BC<5>*)srce, tile, pitch, pow2pad); break;
		case Format::_Bc7:			srce = GetBC(block, (BC<7>*)srce, tile, pitch, pow2pad); break;
		default: {
			uint32	elem_size	= format.getSize();
			uint32	w			= block.size<1>();
			uint32	h			= block.size<2>();
			uint32	d			= block.size<3>();

			malloc_block	temp(elem_size * w * h * d);
			switch (elem_size) {
				case 1:		srce = Get(make_block((uint8*)	temp, w, h, d), (uint8*)	srce, tile, pitch, pow2pad, frags); break;
				case 2:		srce = Get(make_block((uint16*)	temp, w, h, d), (uint16*)	srce, tile, pitch, pow2pad, frags); break;
				case 4:		srce = Get(make_block((uint32*)	temp, w, h, d), (uint32*)	srce, tile, pitch, pow2pad, frags); break;
				case 8:		srce = Get(make_block((uint64*)	temp, w, h, d), (uint64*)	srce, tile, pitch, pow2pad, frags); break;
				case 16:	srce = Get(make_block((uint128*)temp, w, h, d), (uint128*)	srce, tile, pitch, pow2pad, frags); break;
			}
#if 0
			uint8		*s = (uint8*)temp;
			for (block<ISO_rgba, 3>::iterator z = block.begin(), ze = block.end(); z != ze; ++z) {
				for (block<ISO_rgba, 2>::iterator y = z.begin(), ye = z.end(); y != ye; ++y) {
					for (ISO_rgba *x = y.begin(), *xe = y.end(); x != xe; ++x, s += elem_size)
						GetComponents(format, (uint32*)s, (float*)x);
				}
			}
#else
			switch (format.getDataFormat()) {
				case Format::_8:			srce = Get(block, (Texel<TexelFormat< 8,0,8,0,0,0,0> >		*)srce); break;
				case Format::_8_8:			srce = Get(block, (Texel<TexelFormat<16,0,8,8,8,0,0> >		*)srce); break;
				case Format::_8_8_8_8:		srce = Get(block, (Texel<R8G8B8A8>							*)srce); break;
				case Format::_5_6_5:		srce = Get(block, (Texel<TexelFormat<16,0,5,5,6,11,5> >		*)srce); break;
				case Format::_1_5_5_5:		srce = Get(block, (Texel<TexelFormat<16,0,5,5,5,10,5,15,1> >*)srce); break;
				case Format::_5_5_5_1:		srce = Get(block, (Texel<TexelFormat<16,0,1,1,5,6,5,11,5> >	*)srce); break;
				case Format::_4_4_4_4:		srce = Get(block, (Texel<TexelFormat<16,0,4,4,4,8,4,12,4> >	*)srce); break;
				case Format::_GB_GR:
				case Format::_BG_RG:
				case Format::_4_4:			srce = Get(block, (Texel<TexelFormat< 8,0,4,4,4,0,0,0,0> >	*)srce); break;
				case Format::_6_5_5:		srce = Get(block, (Texel<TexelFormat<16,0,5,5,5,10,6> >		*)srce); break;
				case Format::_FM8_S2_F1:	srce = Get(block, (FMask<uint8,   2, 1>						*)srce); break;
				case Format::_FM8_S4_F1:	srce = Get(block, (FMask<uint8,   4, 1>						*)srce); break;
				case Format::_FM8_S8_F1:	srce = Get(block, (FMask<uint8,   8, 1>						*)srce); break;
				case Format::_FM8_S2_F2:	srce = Get(block, (FMask<uint8,   2, 2>						*)srce); break;
				case Format::_FM8_S4_F2:	srce = Get(block, (FMask<uint8,   4, 2>						*)srce); break;
				case Format::_FM8_S4_F4:	srce = Get(block, (FMask<uint8,   4, 4>						*)srce); break;
				case Format::_FM16_S16_F1:	srce = Get(block, (FMask<uint16, 16, 1>						*)srce); break;
				case Format::_FM16_S8_F2:	srce = Get(block, (FMask<uint16,  8, 2>						*)srce); break;
				case Format::_FM32_S16_F2:	srce = Get(block, (FMask<uint32, 16, 2>						*)srce); break;
				case Format::_FM32_S8_F4:	srce = Get(block, (FMask<uint32,  8, 4>						*)srce); break;
				case Format::_FM32_S8_F8:	srce = Get(block, (FMask<uint32,  8, 8>						*)srce); break;
				case Format::_FM64_S16_F4:	srce = Get(block, (FMask<uint64, 16, 4>						*)srce); break;
				case Format::_FM64_S16_F8:	srce = Get(block, (FMask<uint64, 16, 8>						*)srce); break;
			}
#endif
			break;
		}
	}
	if (rearrange)
		iso::RearrangeChannels(block, ConvertChannels(format.getChannels()));
	return srce;
}

const void *Read(const block<HDRpixel, 3> &b, const void *srce, Format format, TileMode tile, uint32 pitch, bool pow2pad, int frags, bool rearrange) {
	switch (format.getDataFormat()) {
		case Format::_Bc6:
			return format.getNumFormat() == gcn::Format::SNorm ? GetBC(b, (BC<-6>*)srce, tile, pitch, pow2pad) : GetBC(b, (BC<6>*)srce, tile, pitch, pow2pad);
	}

	uint32	elem_size	= format.getSize();
	uint32	w = b.size<1>();
	uint32	h = b.size<2>();
	uint32	d = b.size<3>();

	malloc_block	temp(elem_size * w * h * d);
	switch (elem_size) {
		case 1:		srce = Get(make_block((uint8*)	temp, w, h, d), (uint8*)	srce, tile, pitch, pow2pad, frags); break;
		case 2:		srce = Get(make_block((uint16*)	temp, w, h, d), (uint16*)	srce, tile, pitch, pow2pad, frags); break;
		case 4:		srce = Get(make_block((uint32*)	temp, w, h, d), (uint32*)	srce, tile, pitch, pow2pad, frags); break;
		case 8:		srce = Get(make_block((uint64*)	temp, w, h, d), (uint64*)	srce, tile, pitch, pow2pad, frags); break;
		case 16:	srce = Get(make_block((uint128*)temp, w, h, d), (uint128*)srce, tile, pitch, pow2pad, frags); break;
	}
	uint8		*s = (uint8*)temp;
	for (block<HDRpixel, 3>::iterator z = b.begin(), ze = b.end(); z != ze; ++z) {
		for (block<HDRpixel, 2>::iterator y = z.begin(), ye = z.end(); y != ye; ++y) {
			for (HDRpixel *x = y.begin(), *xe = y.end(); x != xe; ++x, s += elem_size)
				GetComponents(format, (uint32*)s, (float*)x);
		}
	}
	if (rearrange)
		iso::RearrangeChannels(b, ConvertChannels(format.getChannels()));
	return srce;
}

malloc_block Write(const block<ISO_rgba, 3> &b, Format::Data format, TileMode tile) {
	switch (format) {
		case Format::_8:		return Put<Texel<TexelFormat< 8,0,8,0,0,0,0> >		 >(b, tile);
		case Format::_8_8:		return Put<Texel<TexelFormat<16,0,8,8,8,0,0> >		 >(b, tile);
		case Format::_8_8_8_8:	return Put<Texel<R8G8B8A8>							 >(b, tile);
		case Format::_5_6_5:	return Put<Texel<TexelFormat<16,0,5,5,6,11,5> >		 >(b, tile);
		case Format::_1_5_5_5:	return Put<Texel<TexelFormat<16,0,5,5,5,10,5,15,1> > >(b, tile);
		case Format::_5_5_5_1:	return Put<Texel<TexelFormat<16,0,1,1,5,6,5,11,5> >	 >(b, tile);
		case Format::_4_4_4_4:	return Put<Texel<TexelFormat<16,0,4,4,4,8,4,12,4> >	 >(b, tile);
		case Format::_GB_GR:
		case Format::_BG_RG:
		case Format::_Bc1:		return PutBC<BC<1> >(b, tile);
		case Format::_Bc2:		return PutBC<BC<2> >(b, tile);
		case Format::_Bc3:		return PutBC<BC<3> >(b, tile);
		case Format::_Bc4:		return PutBC<BC<4> >(b, tile);
		case Format::_Bc5:		return PutBC<BC<5> >(b, tile);
		case Format::_Bc7:		return PutBC<BC<7> >(b, tile);
		case Format::_4_4:		return Put<Texel<TexelFormat< 8,0,4,4,4,0,0,0,0> >	>(b, tile);
		case Format::_6_5_5:	return Put<Texel<TexelFormat<16,0,5,5,5,10,6> >		>(b, tile);
	}
	return malloc_block();
}

malloc_block Write(const block<ISO_rgba, 3> &b, Format format, TileMode tile) {
	bool	rearrange = false;
	for (int i = 0, n = format.numComps(); !rearrange && i < n; i++)
		rearrange = format.getChan(i) != Format::Channel(Format::ChanX + i);

	if (rearrange) {
		auto_block<ISO_rgba, 3>	temp(b);
		iso::RearrangeChannels(b, temp, ConvertChannels(format.getChannels()).reverse());
		return Write(temp, format.getDataFormat(), tile);
	}
	return Write(b, format.getDataFormat(), tile);
}

malloc_block Write(const block<HDRpixel, 3> &block, Format format, TileMode tile) {
	switch (format.getNumFormat()) {
		case gcn::Format::Float:
			switch (format.getDataFormat()) {
//				case Format::_16_16:		return Put<array_vec<float16,2>	>(block, tile);
//				case Format::_32_32:		return Put<array_vec<float32,2>	>(block, tile);
				case Format::_16_16_16_16:	return Put<array_vec<float16,4>	>(block, tile);
//				case Format::_32_32_32:		return Put<array_vec<float32,3>	>(block, tile);
				case Format::_32_32_32_32:	return Put<array_vec<float32,4>	>(block, tile);
			}
			break;
		default:
			switch (format.getDataFormat()) {
		//		case Format::_16:			return Put<unorm16					>(block, tile);
		//		case Format::_32:			return Put<unorm32					>(block, tile);
//				case Format::_16_16:		return Put<array_vec<unorm16,2>	>(block, tile);
//				case Format::_10_11_11:		return Put<unorm3_10_11_11			>(block, tile);
//				case Format::_11_11_10:		return Put<unorm3_11_11_10			>(block, tile);
				case Format::_10_10_10_2:	return Put<unorm4_10_10_10_2		>(block, tile);
				case Format::_2_10_10_10:	return Put<unorm4_2_10_10_10		>(block, tile);
//				case Format::_32_32:		return Put<array_vec<unorm32,2>	>(block, tile);
				case Format::_16_16_16_16:	return Put<array_vec<unorm16,4>	>(block, tile);
//				case Format::_32_32_32:		return Put<array_vec<unorm32,3>	>(block, tile);
				case Format::_32_32_32_32:	return Put<array_vec<unorm32,4>	>(block, tile);
		//		case Format::_8_24:			return Put<array_vec<unorm16,2>	>(block, tile);
		//		case Format::_24_8:			return Put<array_vec<unorm16,2>	>(block, tile);
		//		case Format::_X24_8_32:		return Put<array_vec<unorm16,2>	>(block, tile);
//				case Format::_5_9_9_9:		return Put<scaled_vector4<uint32, 5, 31, 9, 511, 9, 511, 9, 511> >(block);
				case Format::_Bc6:			break;
			}
			break;
	}
	return malloc_block();
}

}// namespace GNF

#else	//_SCE_GPU_ADDRESS_H

#ifdef GPU_ADDRESS_FIRSTSAMPLE
namespace sce { namespace GpuAddress {

inline void* small_memcpy(void *dest, const void *src, size_t count) {
	switch(count)
	{
	case 1:		*((uint8_t *)dest + 0) = *((uint8_t *)src + 0);		break;
	case 2:		*((uint16_t*)dest + 0) = *((uint16_t*)src + 0);		break;
	case 4:		*((uint32_t*)dest + 0) = *((uint32_t*)src + 0);		break;
	case 8:		*((uint64_t*)dest + 0) = *((uint64_t*)src + 0);		break;
	case 16:
		*((uint64_t*)dest + 0) = *((uint64_t*)src + 0);
		*((uint64_t*)dest + 1) = *((uint64_t*)src + 1);
		break;
	default:
		SCE_GNM_ERROR("Unsupported byte count %ld", static_cast<uint64_t>(count));
	}
	return dest;
}
int32_t detileSurfaceRegion(sce::GpuAddress::Tiler2d &tiler, void *outUntiledPixels, const void *tiledPixelsBase, const SurfaceRegion *srcRegion, uint32_t destPitch, uint32_t destSliceSizeElems)
{
	const SurfaceRegion regionCopy = *srcRegion;
	if (regionCopy.m_left  == regionCopy.m_right ||
		regionCopy.m_top   == regionCopy.m_bottom ||
		regionCopy.m_front == regionCopy.m_back)
	{
		return kStatusSuccess; // Zero-area region; nothing to do.
	}
	const uint32_t regionWidth  = regionCopy.m_right  - regionCopy.m_left;
	const uint32_t regionHeight = regionCopy.m_bottom - regionCopy.m_top;
	const uint32_t regionDepth  = regionCopy.m_back   - regionCopy.m_front;

	const uint8_t *in_bytes = (uint8_t*)tiledPixelsBase;
	uint8_t *out_bytes = (uint8_t*)outUntiledPixels;
	const uint32_t bytesPerElement = tiler.m_bitsPerElement/8;
	for(uint32_t z=0; z<regionDepth; ++z)
	{
		for(uint32_t y=0; y<regionHeight; ++y)
		{
			for(uint32_t x=0; x<regionWidth; ++x)
			{
				uint64_t linear_offset, tiled_offset;
				tiler.getTiledElementByteOffset(&tiled_offset, regionCopy.m_left+x, regionCopy.m_top+y, regionCopy.m_front+z, 0);
				computeLinearElementByteOffset(&linear_offset, x,y,z,0, destPitch,destSliceSizeElems, tiler.m_bitsPerElement, tiler.m_numElementsPerPixel);
				small_memcpy(out_bytes + linear_offset / tiler.m_numElementsPerPixel, in_bytes + tiled_offset, bytesPerElement);
			}
		}
	}
	return kStatusSuccess;
}

int32_t detileSurfaceRegion2(void *outUntiledPixels, const void *tiledPixels, const TilingParameters *tp, const SurfaceRegion *srcRegion, uint32_t destPitch, uint32_t destSliceSizeElems)
{
	SurfaceInfo surfInfoOut = {0};
	int32_t status = computeSurfaceInfo(&surfInfoOut, tp);
	if (status != kStatusSuccess)
		return status;

	TilingParameters correctedTP = *tp;
	status = adjustTileMode(&correctedTP.m_tileMode, tp->m_tileMode, surfInfoOut.m_arrayMode);
	if (status != kStatusSuccess)
		return status;
	Gnm::ArrayMode arrayMode;
	status = getArrayMode(&arrayMode, correctedTP.m_tileMode);
	if (status != kStatusSuccess)
		return status;

	switch(arrayMode)
	{
	case Gnm::kArrayModeLinearGeneral:
		// Just memcpy
		SCE_GNM_ASSERT((surfInfoOut.m_surfaceSize & 0xFFFFFFFF00000000LL) == 0LL); // Make sure the size fit in 32b -> for the memcpy
		memcpy(outUntiledPixels, tiledPixels, static_cast<size_t>(surfInfoOut.m_surfaceSize));
		return kStatusSuccess;
	case Gnm::kArrayModeLinearAligned:
		{
			TilerLinear tiler(&correctedTP);
			return tiler.detileSurfaceRegion(outUntiledPixels, tiledPixels, srcRegion, destPitch, destSliceSizeElems);
		}
	case Gnm::kArrayMode1dTiledThin:
	case Gnm::kArrayMode1dTiledThick:
		{
			Tiler1d tiler(&correctedTP);
			return tiler.detileSurfaceRegion(outUntiledPixels, tiledPixels, srcRegion, destPitch, destSliceSizeElems);
		}
	case Gnm::kArrayMode2dTiledThin:
	case Gnm::kArrayMode2dTiledThick:
	case Gnm::kArrayMode2dTiledXThick:
	case Gnm::kArrayMode3dTiledThin:
	case Gnm::kArrayMode3dTiledThick:
	case Gnm::kArrayMode3dTiledXThick:
	case Gnm::kArrayModeTiledThinPrt:
	case Gnm::kArrayModeTiledThickPrt:
	case Gnm::kArrayMode2dTiledThinPrt:
	case Gnm::kArrayMode2dTiledThickPrt:
	case Gnm::kArrayMode3dTiledThinPrt:
	case Gnm::kArrayMode3dTiledThickPrt:
		{
			Tiler2d tiler(&correctedTP);
			return detileSurfaceRegion(tiler, outUntiledPixels, tiledPixels, srcRegion, destPitch, destSliceSizeElems);
		}
	default:
		// Unsupported tiling mode
		return kStatusInvalidArgument;
	}
}
} }
#endif

namespace GNF {
const void *Read(const block<ISO_rgba, 3> &dest, const void *srce, Format format, TileMode tile, TextureType type, uint32 pitch, bool pow2pad, int frags, bool rearrange) {
#ifndef GPU_ADDRESS_FIRSTSAMPLE
	frags	= abs(frags);
#endif
	uint32	width	= dest.size<1>(), height = dest.size<2>(), depth = dest.size<3>();
	uint32	width2	= frags < 0 ? width : width / frags;

	sce::GpuAddress::TilingParameters	tp;
	uint64								untiled_size;
	uint64								tiled_size;
	sce::Gnm::AlignmentType				align;
	clear(tp);
	tp.m_tileMode				= (sce::Gnm::TileMode)tile;
	tp.m_bitsPerFragment		= format.getSize() * 8;
	tp.m_isBlockCompressed		= format.isBlock();
	tp.m_linearWidth			= width2;
	tp.m_linearHeight			= height;
	tp.m_linearDepth			= type == TT_VOLUME ? depth : 1;
	tp.m_numFragmentsPerPixel	= frags < 0 ? 1 : frags;;
	tp.m_surfaceFlags.m_cube	= type == TT_CUBE;
	tp.m_surfaceFlags.m_volume	= type == TT_VOLUME;

	if (tp.m_isBlockCompressed) {
		tp.m_bitsPerFragment	>>= 4;
		width	= (width  + 3) >> 2;
		height	= (height + 3) >> 2;
		width2	= frags < 0 ? width : width / frags;
	}

	computeUntiledSurfaceSize(&untiled_size, &align, &tp);

	tp.m_surfaceFlags.m_pow2Pad	= pow2pad;
	computeTiledSurfaceSize(&tiled_size, &align, &tp);
	malloc_block			temp(untiled_size * depth);

	tp.m_linearWidth			= pitch;

	sce::GpuAddress::SurfaceRegion	region;
	region.m_left				= 0;
    region.m_top				= 0;
    region.m_front				= 0;
    region.m_right				= width2;
    region.m_bottom				= height;
    region.m_back				= tp.m_linearDepth;

	for (int s = 0, ns = tp.m_surfaceFlags.m_volume ? 1 : depth; s < ns; s++) {
#ifdef GPU_ADDRESS_FIRSTSAMPLE
		if (frags < 0)
			ISO_VERIFY(detileSurfaceRegion2(temp + untiled_size * s, (uint8*)srce + tiled_size * s, &tp, &region, width2, width2 * height) == 0);
		else
#endif
			ISO_VERIFY(detileSurfaceRegion(temp + untiled_size * s, (uint8*)srce + tiled_size * s, &tp, &region, width2, width2 * height) == 0);
	}

	uint32	slice	= width * height;
	switch (format.getDataFormat()) {
		case Format::_8:			copy(make_block((Texel<TexelFormat< 8,0,8,0,0,0,0> >		*)temp, width, height, depth), dest); break;
		case Format::_8_8:			copy(make_block((Texel<TexelFormat<16,0,8,8,8,0,0> >		*)temp, width, height, depth), dest); break;
		case Format::_8_8_8_8:		copy(make_block((Texel<R8G8B8A8>							*)temp, width, height, depth), dest); break;
		case Format::_5_6_5:		copy(make_block((Texel<TexelFormat<16,0,5,5,6,11,5> >		*)temp, width, height, depth), dest); break;
		case Format::_1_5_5_5:		copy(make_block((Texel<TexelFormat<16,0,5,5,5,10,5,15,1> >	*)temp, width, height, depth), dest); break;
		case Format::_5_5_5_1:		copy(make_block((Texel<TexelFormat<16,0,1,1,5,6,5,11,5> >	*)temp, width, height, depth), dest); break;
		case Format::_4_4_4_4:		copy(make_block((Texel<TexelFormat<16,0,4,4,4,8,4,12,4> >	*)temp, width, height, depth), dest); break;
		case Format::_GB_GR:		break;
		case Format::_BG_RG:		break;
		case Format::_Bc1:			copy(make_block((const BC<1>								*)temp, width, height, depth), dest); break;
		case Format::_Bc2:			copy(make_block((const BC<2>								*)temp, width, height, depth), dest); break;
		case Format::_Bc3:			copy(make_block((const BC<3>								*)temp, width, height, depth), dest); break;
		case Format::_Bc4:			copy(make_block((const BC<4>								*)temp, width, height, depth), dest); break;
		case Format::_Bc5:			copy(make_block((const BC<5>								*)temp, width, height, depth), dest); break;
		case Format::_Bc7:			copy(make_block((const BC<7>								*)temp, width, height, depth), dest); break;
		case Format::_4_4:			copy(make_block((Texel<TexelFormat< 8,0,4,4,4,0,0,0,0> >	*)temp, width, height, depth), dest); break;
		case Format::_6_5_5:		copy(make_block((Texel<TexelFormat<16,0,5,5,5,10,6> >		*)temp, width, height, depth), dest); break;
		case Format::_FM8_S2_F1:	copy(make_block((FMask<uint8,   2, 1>						*)temp, width, height, depth), dest); break;
		case Format::_FM8_S4_F1:	copy(make_block((FMask<uint8,   4, 1>						*)temp, width, height, depth), dest); break;
		case Format::_FM8_S8_F1:	copy(make_block((FMask<uint8,   8, 1>						*)temp, width, height, depth), dest); break;
		case Format::_FM8_S2_F2:	copy(make_block((FMask<uint8,   2, 2>						*)temp, width, height, depth), dest); break;
		case Format::_FM8_S4_F2:	copy(make_block((FMask<uint8,   4, 2>						*)temp, width, height, depth), dest); break;
		case Format::_FM8_S4_F4:	copy(make_block((FMask<uint8,   4, 4>						*)temp, width, height, depth), dest); break;
		case Format::_FM16_S16_F1:	copy(make_block((FMask<uint16, 16, 1>						*)temp, width, height, depth), dest); break;
		case Format::_FM16_S8_F2:	copy(make_block((FMask<uint16,  8, 2>						*)temp, width, height, depth), dest); break;
		case Format::_FM32_S16_F2:	copy(make_block((FMask<uint32, 16, 2>						*)temp, width, height, depth), dest); break;
		case Format::_FM32_S8_F4:	copy(make_block((FMask<uint32,  8, 4>						*)temp, width, height, depth), dest); break;
		case Format::_FM32_S8_F8:	copy(make_block((FMask<uint32,  8, 8>						*)temp, width, height, depth), dest); break;
		case Format::_FM64_S16_F4:	copy(make_block((FMask<uint64, 16, 4>						*)temp, width, height, depth), dest); break;
		case Format::_FM64_S16_F8:	copy(make_block((FMask<uint64, 16, 8>						*)temp, width, height, depth), dest); break;
			break;
	}
	if (rearrange)
		RearrangeChannels(dest, ConvertChannels(format.getChannels()));

	if (pow2pad)
		depth = next_pow2(depth);
	return (uint8*)srce + tiled_size * depth;
}

const void *Read(const block<HDRpixel, 3> &dest, const void *srce, Format format, TileMode tile, TextureType type, uint32 pitch, bool pow2pad, int frags, bool rearrange) {
#ifndef GPU_ADDRESS_FIRSTSAMPLE
	frags	= abs(frags);
#endif
	uint32	width	= dest.size<1>(), height = dest.size<2>(), depth = dest.size<3>();
	uint32	width2	= frags < 0 ? width : width / frags;

	sce::GpuAddress::TilingParameters	tp;
	uint64								untiled_size;
	uint64								tiled_size;
	sce::Gnm::AlignmentType				align;

	clear(tp);
	tp.m_tileMode				= (sce::Gnm::TileMode)tile;
	tp.m_bitsPerFragment		= format.getSize() * 8;
	tp.m_isBlockCompressed		= format.isBlock();
	tp.m_linearWidth			= width2;
	tp.m_linearHeight			= height;
	tp.m_linearDepth			= type == TT_VOLUME ? depth : 1;
	tp.m_numFragmentsPerPixel	= frags < 0 ? 1 : frags;
	tp.m_surfaceFlags.m_cube	= type == TT_CUBE;
	tp.m_surfaceFlags.m_volume	= type == TT_VOLUME;

	if (tp.m_isBlockCompressed) {
		tp.m_bitsPerFragment	>>= 4;
		width	= (width + 3) >> 2;
		height	= (height + 3) >> 2;
		width2	= frags < 0 ? width : width / frags;
	}

	computeUntiledSurfaceSize(&untiled_size, &align, &tp);
	computeTiledSurfaceSize(&tiled_size, &align, &tp);
	malloc_block			temp(untiled_size * depth);

	tp.m_surfaceFlags.m_pow2Pad	= pow2pad;
	tp.m_linearWidth			= pitch;

	sce::GpuAddress::SurfaceRegion	region;
	region.m_left				= 0;
    region.m_top				= 0;
    region.m_front				= 0;
    region.m_right				= width2;
    region.m_bottom				= height;
    region.m_back				= tp.m_linearDepth;

	for (int s = 0, ns = tp.m_surfaceFlags.m_volume ? 1 : depth; s < ns; s++) {
#ifdef GPU_ADDRESS_FIRSTSAMPLE
		if (frags < 0)
			ISO_VERIFY(detileSurfaceRegion2(temp + untiled_size * s, (uint8*)srce + tiled_size * s, &tp, &region, width2, width2 * height) == 0);
		else
#endif
			ISO_VERIFY(detileSurfaceRegion(temp + untiled_size * s, (uint8*)srce + tiled_size * s, &tp, &region, width2, width2 * height) == 0);
	}

	uint32	slice	= width * height;
	switch (format.getDataFormat()) {
		case Format::_Bc6:
			if (format.getNumFormat() == gcn::Format::SNorm)
				copy(make_block((const BC<-6>*)temp, width, height, depth), dest);
			else
				copy(make_block((const BC< 6>*)temp, width, height, depth), dest);
			break;
		default: {
			uint32	e	= format.getSize();
			uint8	*s	= (uint8*)temp;
			for (block<HDRpixel, 3>::iterator z = dest.begin(), ze = dest.end(); z != ze; ++z) {
				for (block<HDRpixel, 2>::iterator y = z.begin(), ye = z.end(); y != ye; ++y) {
					for (HDRpixel *x = y.begin(), *xe = y.end(); x != xe; ++x, s += e)
						GetComponents(format, (uint32*)s, (float*)x);
				}
			}
			break;
		}
	}

	if (rearrange)
		RearrangeChannels(dest, ConvertChannels(format.getChannels()));

	if (pow2pad)
		depth = next_pow2(depth);
	return (uint8*)srce + tiled_size * depth;
}

malloc_block Write2(const block<ISO_rgba, 3> &srce, Format format, TileMode tile, TextureType type, bool pow2pad) {
	uint32	width	= srce.size<1>(), height = srce.size<2>(), depth = srce.size<3>();
	uint32	zsize, slices;

	if (type == TT_VOLUME) {
		zsize	= depth;
		slices	= 1;
	} else {
		zsize	= 1;
		slices	= depth;
	}

	if (pow2pad) {
		slices	= next_pow2(slices);
		zsize	= next_pow2(zsize);
	}

	uint32	texel_size	= format.getSize();
	sce::GpuAddress::TilingParameters	tp;
	clear(tp);
	tp.m_tileMode				= (sce::Gnm::TileMode)tile;
	tp.m_bitsPerFragment		= texel_size * 8;
	tp.m_isBlockCompressed		= format.isBlock();
	tp.m_linearWidth			= width;
	tp.m_linearHeight			= height;
	tp.m_linearDepth			= zsize;
	tp.m_numFragmentsPerPixel	= 1;
	tp.m_surfaceFlags.m_cube	= type == TT_CUBE;
	tp.m_surfaceFlags.m_volume	= type == TT_VOLUME;
	tp.m_surfaceFlags.m_pow2Pad	= pow2pad;

	if (tp.m_isBlockCompressed) {
		tp.m_bitsPerFragment	>>= 4;
		width	= (width + 3) >> 2;
		height	= (height + 3) >> 2;
	}

	uint32					untiled_pitch = width * texel_size;
	uint64					untiled_size, tiled_size;
	sce::Gnm::AlignmentType	align;

	computeUntiledSurfaceSize(&untiled_size, &align, &tp);
	computeTiledSurfaceSize(&tiled_size, &align, &tp);
	malloc_block			temp(untiled_size * slices);
	malloc_block			data(tiled_size * slices);

	switch (format.getDataFormat()) {
		case Format::_8:		copy(srce, make_strided_block((Texel<TexelFormat< 8,0,8,0,0,0,0> >		*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_8_8:		copy(srce, make_strided_block((Texel<TexelFormat<16,0,8,8,8,0,0> >		*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_8_8_8_8:	copy(srce, make_strided_block((Texel<R8G8B8A8>							*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_5_6_5:	copy(srce, make_strided_block((Texel<TexelFormat<16,0,5,5,6,11,5> >		*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_1_5_5_5:	copy(srce, make_strided_block((Texel<TexelFormat<16,0,5,5,5,10,5,15,1> >*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_5_5_5_1:	copy(srce, make_strided_block((Texel<TexelFormat<16,0,1,1,5,6,5,11,5> >	*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_4_4_4_4:	copy(srce, make_strided_block((Texel<TexelFormat<16,0,4,4,4,8,4,12,4> >	*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_GB_GR:
		case Format::_BG_RG:
		case Format::_Bc1:		copy(srce, make_strided_block((BC<1>									*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_Bc2:		copy(srce, make_strided_block((BC<2>									*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_Bc3:		copy(srce, make_strided_block((BC<3>									*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_Bc4:		copy(srce, make_strided_block((BC<4>									*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_Bc5:		copy(srce, make_strided_block((BC<5>									*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_Bc7:		copy(srce, make_strided_block((BC<7>									*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_4_4:		copy(srce, make_strided_block((Texel<TexelFormat< 8,0,4,4,4,0,0,0,0> >	*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
		case Format::_6_5_5:	copy(srce, make_strided_block((Texel<TexelFormat<16,0,5,5,5,10,6> >		*)temp, width, untiled_pitch, height, untiled_size, depth)); break;
	}

	for (int i = 0; i < slices; i++)
		tileSurface(data + tiled_size * i, temp + untiled_size * i, &tp);
	return data;
}

malloc_block Write(const block<ISO_rgba, 3> &block, Format format, TileMode tile, TextureType type, bool pow2pad) {
	if (format.getChannels() != Format::Channels::XYZW())
		return Write2(iso::RearrangeChannels(block, make_auto_block<ISO_rgba>(block.size<1>(), block.size<2>(), block.size<3>()), ConvertChannels(format.getChannels()).reverse()), format, tile, type, pow2pad);
	return Write2(block, format, tile, type, pow2pad);
}

malloc_block Write2(const block<HDRpixel, 3> &srce, Format format, TileMode tile, TextureType type, bool pow2pad) {
	uint32	width	= srce.size<1>(), height = srce.size<2>(), depth = srce.size<3>();
	uint32	zsize, slices;

	if (type == TT_VOLUME) {
		zsize	= depth;
		slices	= 1;
	} else {
		zsize	= 1;
		slices	= depth;
	}

	if (pow2pad) {
		slices	= next_pow2(slices);
		zsize	= next_pow2(zsize);
	}

	uint32	texel_size	= format.getSize();
	sce::GpuAddress::TilingParameters	tp;
	clear(tp);
	tp.m_tileMode				= (sce::Gnm::TileMode)tile;
	tp.m_bitsPerFragment		= texel_size * 8;
	tp.m_isBlockCompressed		= format.isBlock();
	tp.m_linearWidth			= width;
	tp.m_linearHeight			= height;
	tp.m_linearDepth			= zsize;
	tp.m_numFragmentsPerPixel	= 1;
	tp.m_surfaceFlags.m_cube	= type == TT_CUBE;
	tp.m_surfaceFlags.m_volume	= type == TT_VOLUME;
	tp.m_surfaceFlags.m_pow2Pad	= pow2pad;

	if (tp.m_isBlockCompressed) {
		tp.m_bitsPerFragment	>>= 4;
		width	= (width + 3) >> 2;
		height	= (height + 3) >> 2;
	}

	uint32					untiled_pitch = width * texel_size;
	uint64					untiled_size, tiled_size;
	sce::Gnm::AlignmentType	align;

	computeUntiledSurfaceSize(&untiled_size, &align, &tp);
	computeTiledSurfaceSize(&tiled_size, &align, &tp);
	malloc_block			temp(untiled_size * slices);
	malloc_block			data(tiled_size * slices);

	switch (format.getDataFormat()) {
		case Format::_Bc6: {
			if (format.getNumFormat() == gcn::Format::SNorm)
				copy(srce, make_strided_block((BC<-6>*)temp, width, untiled_pitch, height, untiled_size, depth));
			else
				copy(srce, make_strided_block((BC< 6>*)temp, width, untiled_pitch, height, untiled_size, depth));
			break;
		}
		default: {
			uint32	e	= format.getSize();
			uint8	*s	= (uint8*)temp;
			for (block<HDRpixel, 3>::iterator z = srce.begin(), ze = srce.end(); z != ze; ++z) {
				for (block<HDRpixel, 2>::iterator y = z.begin(), ye = z.end(); y != ye; ++y) {
					for (HDRpixel *x = y.begin(), *xe = y.end(); x != xe; ++x, s += e)
						PutComponents(format, (uint32*)s, (float*)x);
				}
			}
			break;
		}
	}

	for (int i = 0; i < slices; i++)
		tileSurface(data + tiled_size * i, temp + untiled_size * i, &tp);
	return data;
}

malloc_block Write(const block<HDRpixel, 3> &block, Format format, TileMode tile, TextureType type, bool pow2pad) {
	if (format.getChannels() != Format::Channels::XYZW())
		return Write2(iso::RearrangeChannels(block, make_auto_block<HDRpixel>(block.size<1>(), block.size<2>(), block.size<3>()), ConvertChannels(format.getChannels()).reverse()), format, tile, type, pow2pad);
	return Write2(block, format, tile, type, pow2pad);
}

}// namespace GNF

#endif

namespace GNF {
	/*
template<typename T> void RearrangeChannels(block<T, 2> &block, Format::Channels chans) {
	iso::RearrangeChannels(block, ConvertChannels(chans));
}

template<typename T> void RearrangeChannels(block<T, 3> &block, Format::Channels chans) {
	iso::RearrangeChannels(block, ConvertChannels(chans));
}

template void RearrangeChannels(block<ISO_rgba, 2> &block, Format::Channels chans);
template void RearrangeChannels(block<HDRpixel, 2> &block, Format::Channels chans);
template void RearrangeChannels(block<ISO_rgba, 3> &block, Format::Channels chans);
template void RearrangeChannels(block<HDRpixel, 3> &block, Format::Channels chans);
*/

Format ComputeFormat(ISO_ptr<bitmap> bm) {
	bool		nocompress;
	ChannelUse	cu	= GetFormatString(bm).begin();
	if (cu) {
		nocompress	= !cu.IsCompressed();
	} else {
		cu = (bitmap*)bm;
		nocompress	= !!(bm->Flags() & BMF_NOCOMPRESS);
	}

	Format::Data	data	= Format::_Invalid;
	Format::Num		num		= Format::UNorm;

	switch (cu.nc) {
		case 0:
		case 1:
			data = nocompress ? Format::_8 : Format::_Bc4;
			break;
		case 2:
			if (nocompress) {
				data = Format::_8_8;
			} else if (cu.analog[cu.ch[0]] && cu.analog[cu.ch[1]]) {
				data = Format::_Bc5;
			} else {
				int		dig		= cu.analog[cu.ch[0]];
				for (int i = 0; i < 4; i++)
					cu.rc[i] = cu.analog[i] ? 0 : 3;
				data = Format::_Bc1;
			}
			break;
		case 3:
			if (nocompress) {
				data = Format::_8_8_8_8;
			} else {
				data = Format::_Bc1;
			}
			break;
		default:
			if (nocompress) {
				data = Format::_8_8_8_8;
			} else {
				int	a = cu.FindMaskChannel(bm);
				if (a < 0) {
					data = Format::_Bc3;
				} else {
					data = Format::_Bc1;
					if (a != 3) {
						cu.rc[(a+1)&3]	= 0;
						cu.rc[(a+2)&3]	= 1;
						cu.rc[(a+3)&3]	= 2;
						cu.rc[a]		= 3;
					}
				}
			}
			break;
	}
	return Format(data, num, ConvertChannels(cu.rc));
}

}// namespace GNF

//-----------------------------------------------------------------------------
//	GNFFileHandler
//-----------------------------------------------------------------------------

using namespace iso;

class GNFFileHandler : public BitmapFileHandler {
	ISO_ptr<void>			Read1(tag id, istream_ref file, const GNF::Header &head, const GNF::Texture &tex);
	void					Write(ISO_ptr<bitmap> bm, ostream_ref file, GNF::Header &head, GNF::Texture &tex);

	const char*		GetExt() override { return "gnf"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32>() == GNF::Header::MAGIC ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} gnf;

ISO_ptr<void> GNFFileHandler::Read1(tag id, istream_ref file, const GNF::Header &head, const GNF::Texture &tex) {
	file.seek(tex.base + head.content_size + 8);
	malloc_block	mem(file, tex.size);
	const void		*srce	= mem;
	int				width	= tex.getWidth(), height = tex.getHeight(), depth = tex.getLastSlice() + 1;
	int				mips	= tex.getLastMip();
	TextureType		type	= tex.is3D() ? TT_VOLUME : tex.isCubemap() ? TT_CUBE : tex.isArray() ? TT_ARRAY : TT_NORMAL;

	if (tex.getFormat().isHDR()) {
		ISO_ptr<HDRbitmap>	bm(id);
		if (mips) {
			bm->Create(width * 2, height * depth, 0, depth);
			bm->SetMips(mips);
			for (int i = 0; i <= mips; i++)
				srce = GNF::Read(bm->MipArray(i), srce, tex.getFormat(), tex.getTileMode(), type, tex.getPitch() >> i, tex.isPaddedToPow2(), 1, false);
			RearrangeChannels(bm->All(), GNF::ConvertChannels(tex.getChannels()));
		} else {
			bm->Create(width, height * depth, 0, depth);
			srce = GNF::Read(bm->All3D(), srce, tex.getFormat(), tex.getTileMode(), type, tex.getPitch(), tex.isPaddedToPow2(), 1, false);
		}
		return bm;

	} else {
		ISO_ptr<bitmap>	bm(id);
		if (mips) {
			bm->Create(width * 2, height * depth, 0, depth);
			bm->SetMips(mips);
			for (int i = 0; i <= mips; i++)
				srce = GNF::Read(bm->MipArray(i), srce, tex.getFormat(), tex.getTileMode(), type, tex.getPitch() >> i, tex.isPaddedToPow2(), 1, false);
			RearrangeChannels(bm->All(), GNF::ConvertChannels(tex.getChannels()));
		} else {
			bm->Create(width, height * depth, 0, depth);
			srce = GNF::Read(bm->All3D(), srce, tex.getFormat(), tex.getTileMode(), type, tex.getPitch(), tex.isPaddedToPow2(), 1, false);
		}
		return bm;
	}
}

ISO_ptr<void> GNFFileHandler::Read(tag id, istream_ref file) {
	GNF::Header	header	= file.get();
	if (header.magic != GNF::Header::MAGIC)
		return ISO_NULL;

	int	n = header.num_textures;
	if (n == 1)
		return Read1(id, file, header, file.get());

	GNF::Texture		*tex = new GNF::Texture[n];
	readn(file, tex, n);

	ISO_ptr<anything>	a(id);
	for (int i = 0; i < n; i++)
		a->Append(Read1(0, file, header, tex[i]));

	delete[] tex;
	return a;
}

void GNFFileHandler::Write(ISO_ptr<bitmap> bm, ostream_ref file, GNF::Header &head, GNF::Texture &tex) {
	bm->Unpalette();
	uint32		width	= bm->BaseWidth(), height = bm->BaseHeight(), depth = bm->Depth();
	uint32		mips	= bm->Mips();
	TextureType type	= bm->Type();

	gcn::Format	fmt		= GNF::ComputeFormat(bm);
	uint32				size	= !bm->IsVolume()
		? tex.initAs2dArray(width, height, depth, mips, fmt, gcn::Thin_1dThin, 0, bm->IsCube())
		: tex.initAs3d(width, height, depth, mips, fmt, gcn::Thin_1dThin);

	head.Add(tex);

	if (mips) {
		for (int i = 0; i <= mips; i++)
			GNF::Write(bm->MipArray(i), tex.getFormat(), tex.getTileMode(), type, tex.isPaddedToPow2()).write(file);
	} else {
		GNF::Write(bm->All3D(), tex.getFormat(), tex.getTileMode(), type, tex.isPaddedToPow2()).write(file);
	}
}

bool GNFFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	GNF::Header			header;

	if (p.IsType<anything>()) {
		anything		*a		= p;
		int				n		= a->Count();
		GNF::Texture	*tex	= new GNF::Texture[n];
		file.seek(header.Init(n));
		for (int i = 0; i < n; i++) {
			if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>((*a)[i]))
				Write((*a)[i], file, header, tex[i]);
		}

		file.seek(0);
		file.write(header);
		writen(file, tex, n);
		return true;

	} else if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p)) {
		GNF::Texture	tex;
		file.seek(header.Init(1));
		Write(bm, file, header, tex);
		file.seek(0);
		file.write(header);
		file.write(tex);
		return true;
	}

	return false;
}
