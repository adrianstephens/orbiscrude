#ifndef NURBS_H
#define NURBS_H

#include "patch.h"

namespace iso {

struct NurbsModel {
	float3p						minext;
	float3p						maxext;
	ISO_openarray<SubPatch>		subpatches;
};

} // namespace iso

ISO_DEFUSERCOMPV(iso::NurbsModel, minext, maxext, subpatches);

void Draw(iso::GraphicsContext &ctx, iso::SubPatch &subpatch);
void Draw(iso::GraphicsContext &ctx, iso::ISO_ptr<iso::NurbsModel> &patch);

#endif// NURBS_H
