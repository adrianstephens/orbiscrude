#ifndef LENSFLARE_H
#define LENSFLARE_H

#include "base/vector.h"

namespace iso {
	class GraphicsContext;
}

void LensFlarePreRender(iso::GraphicsContext &ctx, param(iso::float3x4) view, param(iso::float4x4) proj);
void LensFlareRender(iso::GraphicsContext &ctx, param(iso::float3x4) view, param(iso::float4x4) proj, float brightness);

#endif // LENSFLARE_H