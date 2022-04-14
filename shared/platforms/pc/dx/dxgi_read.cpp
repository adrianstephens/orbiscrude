#include "dxgi_read.h"

namespace iso {

template<typename D, typename S> const void* copy_slices(const block<D, 3> &dest, const S *srce, uint64 depth_stride) {
	uint32		w	= dest.template size<1>(), h = dest.template size<2>(), d = dest.template size<3>();
	uint32		s2	= aligned_stride(uint32(sizeof(S)), w);//, s3 = s2 * h;
	copy(make_strided_block(srce, w, s2, h, depth_stride, d), dest);
	return (const uint8*)srce + s2 * h;//s3 * d;
}

template<typename D, int N> const void* copy_slices(const block<D, 3> &dest, const BC<N> *srce, uint64 depth_stride) {
	uint32		w	= round_pow2(dest.template size<1>(), 2), h = round_pow2(dest.template size<2>(), 2), d = dest.template size<3>();
	uint32		s2	= aligned_stride(uint32(sizeof(BC<N>)), w);//, s3 = s2 * h;
	copy(make_strided_block(srce, w, s2, h, depth_stride, d), dest);
	return (const uint8*)srce + s2 * h;//s3 * d;
}

template<> const void *copy_slices(const block<ISO_rgba, 3> &dest, const void *srce, DXGI_COMPONENTS::LAYOUT layout, DXGI_COMPONENTS::TYPE type, uint64 depth_stride) {
	switch (layout) {
		//LDR
		case DXGI_COMPONENTS::R8G8B8A8:		return copy_slices(dest, (Texel<TexelFormat<32,0,8,8,8,16,8,24,8>	>*)srce, depth_stride);
		case DXGI_COMPONENTS::R8G8:			return copy_slices(dest, (Texel<TexelFormat<16,0,8,8,8,0,0>			>*)srce, depth_stride);
		case DXGI_COMPONENTS::R8:			return copy_slices(dest, (Texel<TexelFormat< 8,0,8,0,0,0,0>			>*)srce, depth_stride);
//		case DXGI_COMPONENTS::R1:			return copy_slices(dest, (Texel<TexelFormat< 1,0,1,0,0,0,0>			>*)srce, depth_stride);
		case DXGI_COMPONENTS::B5G6R5:		return copy_slices(dest, (Texel<TexelFormat<16,11,5,5,6,0,5>		>*)srce, depth_stride);
		case DXGI_COMPONENTS::B5G5R5A1:		return copy_slices(dest, (Texel<TexelFormat<16,10,5,5,5,0,5,15,1>	>*)srce, depth_stride);
		case DXGI_COMPONENTS::R4G4B4A4:		return copy_slices(dest, (Texel<TexelFormat<16,0,4,4,4,8,4,12,4>	>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC1:			return copy_slices(dest, (const BC<1>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC2:			return copy_slices(dest, (const BC<2>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC3:			return copy_slices(dest, (const BC<3>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC4:			return copy_slices(dest, (const BC<4>*)srce, depth_stride);
		case DXGI_COMPONENTS::BC5:			return copy_slices(dest, (const BC<5>*)srce, depth_stride);
		default:			return 0;
	}
}

template<> const void *copy_slices(const block<HDRpixel, 3> &dest, const void *srce, DXGI_COMPONENTS::LAYOUT layout, DXGI_COMPONENTS::TYPE type, uint64 depth_stride) {
	switch (type) {
		case DXGI_COMPONENTS::FLOAT:
			switch (layout) {
				//HDR
				case DXGI_COMPONENTS::R32G32B32A32:	return copy_slices(dest, (array_vec<float32,4>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32G32B32:	return copy_slices(dest, (array_vec<float32,3>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R16G16B16A16:	return copy_slices(dest, (array_vec<float16,4>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32G32:		return copy_slices(dest, (array_vec<float32,2>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R16G16:		return copy_slices(dest, (array_vec<float16,2>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32:			return copy_slices(dest, (float					*)srce, depth_stride);
				case DXGI_COMPONENTS::R16:			return copy_slices(dest, (float16				*)srce, depth_stride);
				case DXGI_COMPONENTS::BC6:			return copy_slices(dest, (const BC<-6>			*)srce, depth_stride);
				default:			return 0;
			}
		case DXGI_COMPONENTS::UFLOAT:
			switch (layout) {
				//HDR
				case DXGI_COMPONENTS::R11G11B10:	return copy_slices(dest, (float3_11_11_10		*)srce, depth_stride);
				case DXGI_COMPONENTS::BC6:			return copy_slices(dest, (const BC<6>			*)srce, depth_stride);
				default:			return 0;
			}
		case DXGI_COMPONENTS::SINT:
			switch (layout) {
				//HDR
				case DXGI_COMPONENTS::R32G32B32A32:	return copy_slices(dest, (array_vec<int32,4>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32G32B32:	return copy_slices(dest, (array_vec<int32,3>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R16G16B16A16:	return copy_slices(dest, (array_vec<int16,4>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32G32:		return copy_slices(dest, (array_vec<int32,2>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R16G16:		return copy_slices(dest, (array_vec<int16,2>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32:			return copy_slices(dest, (int32					*)srce, depth_stride);
				case DXGI_COMPONENTS::R16:			return copy_slices(dest, (int16					*)srce, depth_stride);
				case DXGI_COMPONENTS::BC6:			return copy_slices(dest, (const BC<6>			*)srce, depth_stride);
				default:			return 0;
			}
		default:
		case DXGI_COMPONENTS::UINT:
			switch (layout) {
				//HDR
				case DXGI_COMPONENTS::R32G32B32A32:	return copy_slices(dest, (array_vec<uint32,4>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32G32B32:	return copy_slices(dest, (array_vec<uint32,3>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R16G16B16A16:	return copy_slices(dest, (array_vec<uint16,4>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32G32:		return copy_slices(dest, (array_vec<uint32,2>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R10G10B10A2:	return copy_slices(dest, (uint4_10_10_10_2		*)srce, depth_stride);
				case DXGI_COMPONENTS::R11G11B10:	return copy_slices(dest, (uint3_11_11_10		*)srce, depth_stride);
				case DXGI_COMPONENTS::R16G16:		return copy_slices(dest, (array_vec<uint16,2>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R32:			return copy_slices(dest, (uint32				*)srce, depth_stride);
				case DXGI_COMPONENTS::R16:			return copy_slices(dest, (uint16				*)srce, depth_stride);
				case DXGI_COMPONENTS::R24G8:		return copy_slices(dest, (Texel<TexelFormat<32,0,24,24,8,0,0>>*)srce, depth_stride);
				default:			return 0;
			}
		case DXGI_COMPONENTS::UNORM:
			switch (layout) {
				//HDR
				case DXGI_COMPONENTS::R16G16B16A16:	return copy_slices(dest, (array_vec<unorm16,4>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R10G10B10A2:	return copy_slices(dest, (unorm4_10_10_10_2		*)srce, depth_stride);
				case DXGI_COMPONENTS::R11G11B10:	return copy_slices(dest, (unorm3_11_11_10		*)srce, depth_stride);
				case DXGI_COMPONENTS::R16G16:		return copy_slices(dest, (array_vec<unorm16,2>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R16:			return copy_slices(dest, (unorm16				*)srce, depth_stride);
				default:			return 0;
			}
		case DXGI_COMPONENTS::SNORM:
			switch (layout) {
				//HDR
				case DXGI_COMPONENTS::R16G16B16A16:	return copy_slices(dest, (array_vec<norm16,4>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R16G16:		return copy_slices(dest, (array_vec<unorm16,2>	*)srce, depth_stride);
				case DXGI_COMPONENTS::R16:			return copy_slices(dest, (unorm16				*)srce, depth_stride);
				default:			return 0;
			}

	}
}

}
