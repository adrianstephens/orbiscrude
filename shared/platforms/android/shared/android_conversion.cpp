#include "systems/conversion/platformdata.h"
#include "mesh/model_iso.h"

using namespace iso;

namespace iso {
	struct OGLTexture;
	struct OGLSubMesh;
}

extern template ISO::Type* ISO::getdef<OGLTexture>();
extern template ISO::Type* ISO::getdef<OGLSubMesh>();

ISO_ptr<void> ReadOGLFX(tag id, istream_ref file, const filename *fn, const char *platdef);
ISO_ptr<void> ReadVLKFX(tag id2, istream_ref file, const filename *fn, const char *platdef) {
	return ISO_NULL;
}

class PlatformAndroid : Platform {
public:
	PlatformAndroid() : Platform("android") {
	}
	ISO_ptr<void>	ReadFX(tag id, istream_ref file, const filename *fn) {
		bool	vulkan = ISO::root("variables")["vulkan"].Get(false);
		return vulkan
			? ReadVLKFX(id, file, fn, "PLAT_ANDROID")
			: ReadOGLFX(id, file, fn, "PLAT_ANDROID");
	}
	type	Set() {
		bool	vulkan = ISO::root("variables")["vulkan"].Get(false);
		((ISO::TypeUser*)ISO::getdef<Texture>())->subtype		= ISO::getdef<ISO_ptr<OGLTexture> >();
		((ISO::TypeUser*)ISO::getdef<SubMeshPtr>())->subtype	= ISO::getdef<ISO_ptr<OGLSubMesh> >();
		((ISO::TypeUser*)ISO::getdef<Model3>())->flags			|= ISO::TypeUser::CHANGE;
		return PT_LITTLEENDIAN;
	}
	uint32	NextTexWidth(uint32 x)	{ return min(next_pow2(x), 2048u); }
	uint32	NextTexHeight(uint32 x)	{ return min(next_pow2(x), 2048u); }
	bool	BetterTex(uint32 w0, uint32 h0, uint32 w1, uint32 h1)	{
		int	total0	= w0 * h0, total1 = w1 * h1;
		return total1 != total0 ? (total1 < total0) : (squareness(w1, h1) > squareness(w0, h0));
	}
	ISO_rgba DefaultColour()		{ return ISO_rgba(0,0,0,0); }
} platform_android;
