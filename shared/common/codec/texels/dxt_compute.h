#ifndef DXT_COMPUTE_H
#define DXT_COMPUTE_H

#include "graphics.h"
#include "dxt.h"

#ifdef HAS_COMPUTE

#include "iso/iso_binary.h"
#include "common/shader.h"

extern "C" ISO::Value dxt_bin;
extern "C" char dxt_end[];

namespace iso {

template<> inline TexFormat GetTexFormat<ISO_rgba>()	{ return GetTexFormat<unorm8[4]>(); }
template<> inline TexFormat GetTexFormat<HDRpixel>()	{ return GetTexFormat<float4>(); }

void copy_compute(Texture &&tex, block<BC<1>, 2> &dest);
void copy_compute(Texture &&tex, block<BC<2>, 2> &dest);
void copy_compute(Texture &&tex, block<BC<3>, 2> &dest);
void copy_compute(Texture &&tex, block<BC<6>, 2> &dest);

template<typename S> void copy(const block<S, 2> &srce, block<BC<1>, 2> &dest) {
	copy_compute(make_texture(srce, MEM_DEFAULT|MEM_FORCE2D), dest);
}
template<typename S> void copy(const block<S, 2> &srce, block<BC<2>, 2> &dest) {
	copy_compute(make_texture(srce, MEM_DEFAULT|MEM_FORCE2D), dest);
}
template<typename S> void copy(const block<S, 2> &srce, block<BC<3>, 2> &dest) {
	copy_compute(make_texture(srce, MEM_DEFAULT|MEM_FORCE2D), dest);
}
template<typename S> void copy(const block<S, 2> &srce, block<BC<6>, 2> &dest) {
	copy_compute(make_texture(srce, MEM_DEFAULT|MEM_FORCE2D), dest);
}

} //namespace iso

#endif // HAS_COMPUTE

#endif	// DXT_COMPUTE_H
