#ifndef GRABCUT_H
#define GRABCUT_H

#include "base/vector.h"
#include "base/block.h"
#include "base/array.h"

namespace iso {

template<typename T> struct GaussianModel {
	T	coefs;
	T	mean[3];
	T	cov[9];
	GaussianModel() { clear(*this); }
};

typedef array<GaussianModel<double>, 5>				Grabcut_Model;

typedef  aligned<array_vec<unorm8, 3>, 4> rgbx8;

//! GrabCut algorithm flags
enum GrabCutModes {
	GC_EVAL				= 0,	//algorithm should just resume
	GC_INIT_WITH_RECT	= 1,	//initializes the state and the mask using the provided rectangle
	GC_INIT_WITH_MASK	= 2,	//initializes the state using the provided mask
	GC_GRAPH2			= 4,
};

enum GrabCutClasses {
	GC_BGD    = 0,		// an obvious background pixel
	GC_FGD    = 1,		// an obvious foreground pixel
	GC_PR_BGD = 2,		// a possible background pixel
	GC_PR_FGD = 3		// a possible foreground pixel
};


void grabCut(const block<rgbx8, 2> &img, Grabcut_Model &bgdModel, Grabcut_Model &fgdModel, int rectx, int recty, int rectw, int recth, int iterCount, int mode);
void grabCut(const block<rgbx8, 2> &img, int x, int y, int w, int h, int iterCount);
}// namespace iso

#endif // GRABCUT_H
