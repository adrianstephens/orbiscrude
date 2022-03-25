#ifndef ALLOCATOR2D_H
#define ALLOCATOR2D_H

#include "base/array.h"

namespace iso {

struct allocator2d {

	struct point {
		int	x, y;
		point()	{}
		point(int _x, int _y) : x(_x), y(_y)	{}
	};

	struct rect {
		int	x, y, w, h;
		rect()	{}
		rect(int _x, int _y, int _w, int _h) : x(_x), y(_y), w(_w), h(_h)	{}
	};

	dynamic_array<rect> free_rects;
	point	size;
	point	max_size;
	bool	double_x;
	float	grow_ratio;

	allocator2d() {
		reset(0, 0, maximum, maximum);
	}
	allocator2d(int width, int height) {
		reset(width, height, width, height);
	}
	allocator2d(int width, int height, int maxWidth, int maxHeight) {
		reset(width, height, maxWidth, maxHeight);
	}

	void	reset(int width, int height, int maxWidth, int maxHeight);
	bool	allocate(uint32 width, uint32 height, point &result);
};

} // namespace iso

#endif // ALLOCATOR2D_H
