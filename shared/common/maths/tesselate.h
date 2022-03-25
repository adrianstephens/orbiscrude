#ifndef TESSELATE_H
#define TESSELATE_H

#include "geometry.h"

namespace iso {

int find_contour(WINDING_RULE rule, const position2 *p, int n, position2 *result, int *lengths);

} // namespace iso

#endif // TESSELATE_H
