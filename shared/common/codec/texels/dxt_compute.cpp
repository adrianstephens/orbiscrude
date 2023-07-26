//#define HAS_COMPUTE

#include "dxt_compute.h"

#ifdef HAS_COMPUTE

#include "iso/iso_binary.h"
#include "common/shader.h"

extern "C" ISO::Value dxt_bin;
extern "C" char dxt_end[];

using namespace iso;

pass *GetDXTShader(const char *name) {
	ISO::getdef<fx>();
	ISO::getdef<pass>();
	static PtrBrowser2	dxt(dxt_bin, dxt_end);
	return (pass*)dxt[name][0];
}

void iso::copy_compute(Texture &&src, block<BC<1>, 2> &dest) {
	ComputeContext	cc;
	cc.Begin();
	cc.SetShader(*GetDXTShader("dxt1"));
	
	point	size	= src.Size();
	int 	width 	= size.x, height = size.y;
	int		width4 	= div_round_up(width, 4), height4 = div_round_up(height, 4);
	
#if 0//def PLAT_METAL
	TextureT<uint2p>	dst(width4, height4, MEM_WRITABLE|MEM_CPU_READ|MEM_FORCE2D);
	cc.SetTexture(dst, 1);
#else
	SurfaceT<uint2p>	dst(width4, height4, MEM_STAGING|MEM_CPU_READ|MEM_FORCE2D);
	TextureT<uint2p>	dst1(width4, height4, MEM_WRITABLE|MEM_FORCE2D);
	cc.SetRWTexture(dst1, 0);
#endif

	cc.SetTexture(src, 0);
	cc.Dispatch(width4, height4, 1);
#if 1//ndef PLAT_METAL
	cc.Blit(dst, dst1);
#endif
	cc.PutFence().Wait();
	copy(dst.Data(cc), element_cast<uint2p>(dest));
}

void iso::copy_compute(Texture &&src, block<BC<2>, 2> &dest) {
	ComputeContext	cc;
	cc.Begin();
	cc.SetShader(*GetDXTShader("dxt3"));

	point	size	= src.Size();
	int 	width 	= size.x, height = size.y;
	int		width4 	= div_round_up(width, 4), height4 = div_round_up(height, 4);

#if 0//def PLAT_METAL
	TextureT<uint4p>	dst(width4, height4, MEM_WRITABLE|MEM_CPU_READ|MEM_FORCE2D);
	cc.SetTexture(dst, 1);
#else
	SurfaceT<uint4p>	dst(width4, height4, MEM_STAGING|MEM_CPU_READ|MEM_FORCE2D);
	TextureT<uint4p>	dst1(width4, height4, MEM_WRITABLE|MEM_FORCE2D);
	cc.SetRWTexture(dst1, 0);
#endif

	cc.SetTexture(src, 0);
	cc.Dispatch(width4, height4, 1);
#if 1//ndef PLAT_METAL
	cc.Blit(dst, dst1);
#endif
	cc.PutFence().Wait();
	copy(dst.Data(cc), element_cast<uint4p>(dest));
}
void iso::copy_compute(Texture &&src, block<BC<3>, 2> &dest) {
	ComputeContext	cc;
	cc.Begin();
	cc.SetShader(*GetDXTShader("dxt5"));
	
	point	size	= src.Size();
	int 	width 	= size.x, height = size.y;
	int		width4 	= div_round_up(width, 4), height4 = div_round_up(height, 4);
	
#if 0//def PLAT_METAL
	TextureT<uint4p>	dst(width4, height4, MEM_WRITABLE|MEM_CPU_READ|MEM_FORCE2D);
	cc.SetTexture(dst, 1);
#else
	SurfaceT<uint4p>	dst(width4, height4, MEM_STAGING|MEM_CPU_READ|MEM_FORCE2D);
	TextureT<uint4p>	dst1(width4, height4, MEM_WRITABLE|MEM_FORCE2D);
	cc.SetRWTexture(dst1, 0);
#endif

	cc.SetTexture(src, 0);
	cc.Dispatch(width4, height4, 1);
#if 1//ndef PLAT_METAL
	cc.Blit(dst, dst1);
#endif
	cc.PutFence().Wait();
	copy(dst.Data(cc), element_cast<uint4p>(dest));
}

void iso::copy_compute(Texture &&src, block<BC<6>, 2> &dest) {
	ComputeContext	cc;
	cc.Begin();
	cc.SetShader(*GetDXTShader("dxt6h"));

	point	size	= src.Size();
	int 	width 	= size.x, height = size.y;
	int		width4 	= div_round_up(width, 4), height4 = div_round_up(height, 4);

#if 0//def PLAT_METAL
	TextureT<uint4p>	dst(width4, height4, MEM_WRITABLE|MEM_CPU_READ|MEM_FORCE2D);
	cc.SetTexture(dst, 1);
#else
	SurfaceT<uint4p>	dst(width4, height4, MEM_STAGING|MEM_CPU_READ|MEM_FORCE2D);
	TextureT<uint4p>	dst1(width4, height4, MEM_WRITABLE|MEM_FORCE2D);
	cc.SetRWTexture(dst1, 0);
#endif

	cc.SetTexture(src, 0);
	cc.Dispatch(width4, height4, 1);
#if 1//ndef PLAT_METAL
	cc.Blit(dst, dst1);
#endif
	cc.PutFence().Wait();
	copy(dst.Data(cc), element_cast<uint4p>(dest));
}

#endif // HAS_COMPUTE
