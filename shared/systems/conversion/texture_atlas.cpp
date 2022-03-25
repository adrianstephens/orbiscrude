#include "allocators/allocator2d.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "filetypes/bitmap/bitmap.h"
#include "bits.h"

using namespace iso;
/*
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
*/
struct split : rect {
	int		state;
	split	*child[2], *parent;

	split(const rect &_rect, split *_parent) : rect(_rect), parent(_parent), state(0) {
		child[0] = child[1] = 0;
	}

	void	split_h(int v) {
		child[0] = new split(rect(a, point{a.x + v, b.y}), this);
		child[1] = new split(rect(point{a.x + v, a.y}, b), this);
	}
	void	split_v(int v) {
		child[0] = new split(rect(a, point{b.x, a.y + v}), this);
		child[1] = new split(rect(point{a.x, a.y + v}, b), this);
	}
};

split *insert(split *s, rect &r) {
	auto	size = r.extent();

	while (s) {
		auto	ssize = s->extent();
		if (size.x > ssize.x || size.y > ssize.y || s->state != 0) {
			for (;;) {
				if (s->child[1]) {
					s = s->child[1];
					break;
				} else {
					s = s->parent;
					if (!s)
						return 0;
				}
			}
			continue;
		}

		if (!s->child[0]) {
			r.a = s->a;

			auto	d = ssize - size;
			if (d.x > d.y) {
				s->split_h(size.x);
				s->child[0]->split_v(size.y);
			} else {
				s->split_v(size.y);
				s->child[0]->split_h(size.x);
			}
			s->child[0]->state = 1;
			break;
		}
		s = s->child[0];
	}
	return s;
}

bool test_pack(int w, int h, rect *rects, int n) {
	split	*root	= new split(rect(zero, point{w, h}), 0);
	for (int i = 0; i < n; i++) {
		if (!insert(root, rects[i]))
			return false;
	}
	return true;
}

ISO_ptr<bitmap> TextureAtlas(holder<ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > > > bms) {
	int		n		= bms->Count();
	rect	*rects	= new rect[n];
	int		maxw	= 0, maxh = 0, area = 0;

	for (int i = 0; i < n; i++) {
		bitmap	*bm = (*bms.t)[i];
		int		w	= bm->Width(), h = bm->Height();
		rects[i]	= rect(zero, point{w, h});
		maxw		= max(maxw, w);
		maxh		= max(maxh, h);
		area		+= w * h;
	}

	int	l2w = log2(maxw), l2h = log2(maxh), l2a = log2(area);
	if (l2w + l2h < l2a) {
		if (l2w >= l2a / 2) {
			l2h = l2a - l2w;
		} else if (l2h >= l2a / 2) {
			l2w = l2a - l2h;
		} else {
			l2w = (l2a + 1) / 2;
			l2h = l2a / 2;
		}
	}

	while (!test_pack(1 << l2w, 1 << l2h, rects, n)) {
		if (l2w < l2h)
			l2w++;
		else
			l2h++;
	}

	ISO_ptr<bitmap>	bm2(0);
	bm2->Create(1 << l2w, 1 << l2h);
	for (int i = 0; i < n; i++) {
		bitmap	*bm = (*bms.t)[i];
		rect	&r	= rects[i];
		copy(bm->All(), bm2->Block(r.a.x, r.a.y, r.extent().x, r.extent().y));
	}
	return bm2;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#if 0
void allocator2d::reset(int width, int height, int maxWidth, int maxHeight) {
	size		= point(width, height);
	max_size	= point(maxWidth, maxHeight);
	double_x	= true;
	grow_ratio	= 2;

	free_rects.clear();
}

bool allocator2d::allocate(uint32 width, uint32 height, point &result) {
	rect	*best			= free_rects.end();
	uint32	bestFreeArea	= maximum;

	for (rect *i = free_rects.begin(); i != free_rects.end(); ++i) {
		if (i->w >= width && i->h >= height) {
			// Calculate rank for free area. Lower is better
			uint32 freeArea = i->w * i->h;
			if (freeArea < bestFreeArea) {
				best			= i;
				bestFreeArea	= freeArea;
			}
		}
	}

	if (best == free_rects.end()) {
		if (width > max_size.x || height > max_size.y)
			return false;

		// If no allocations yet, simply expand the single free area
		if (free_rects.empty()) {
			size.x	= width  > size.x ? clamp(int(size.x * grow_ratio), width,  max_size.x) : size.x;
			size.y	= height > size.y ? clamp(int(size.y * grow_ratio), height, max_size.y) : size.y;
			free_rects.push_back(rect(0, 0, size.x, size.y));

		} else if ((double_x || size.y + height >= max_size.y) && size.x + width < max_size.x) {
			point	old = size;
			if (height > size.y) {
				size.y = clamp(int(size.y * grow_ratio), height, max_size.y);
				free_rects.push_back(rect(0, old.y, old.x, size.y - old.y));
			}
			size.x = clamp(int(size.x * grow_ratio), old.x + width, max_size.x);
			free_rects.push_back(rect(old.x, 0, size.x - old.x, size.y));

			double_x = false;

		} else if (size.y + height < max_size.y) {
			point	old = size;
			if (width > size.x) {
				size.x = clamp(int(size.x * grow_ratio), width, max_size.x);
				free_rects.push_back(rect(old.x, 0, size.x - old.x, old.y));
			}
			size.y = clamp(int(size.y * grow_ratio), old.y + height, max_size.y);
			free_rects.push_back(rect(0, old.y, size.x, size.y - old.y));

			double_x = true;

		} else {
			return false;
		}

		best		= &free_rects.back();
	}

	result = point(best->x, best->y);

	// Reserve the area by splitting up the remaining free area
	if (best->w - width > best->h - height) {
		if (best->w > width)
			free_rects.push_back(rect(best->x + width, best->y, best->w - width, best->h));
		best->y += height;
		best->h -= height;
		best->w = width;

	} else {
		if (best->h > height)
			free_rects.push_back(rect(best->x, best->y + height, best->w, best->h - height));
		best->x += width;
		best->w -= width;
		best->h = height;
	}

	return true;
}

struct test_allocate2d {
	test_allocate2d() {
		allocator2d	a;
		allocator2d::point	result;
		a.allocate(100, 200, result);

	}
} tester;
#endif

//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation(TextureAtlas)
);
