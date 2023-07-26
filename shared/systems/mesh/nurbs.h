#ifndef MESH_NURBS_H
#define MESH_NURBS_H

#include "patch.h"

namespace iso {

struct NurbsModel {
	float3p						minext;
	float3p						maxext;
	ISO_openarray<SubPatch>		subpatches;
};

ISO_DEFUSERCOMPV(NurbsModel, minext, maxext, subpatches);

void Draw(GraphicsContext &ctx, SubPatch &subpatch);
void Draw(GraphicsContext &ctx, ISO_ptr<NurbsModel> &patch);

} // namespace iso

#endif// MESH_NURBS_H
