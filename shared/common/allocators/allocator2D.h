#ifndef ALLOCATOR2D_H
#define ALLOCATOR2D_H

#include "base/defs.h"

namespace iso {

class allocator2D {

	enum dir { HORZ = 1, VERT = -1 };

	struct split {
		split	*parent, *link[2];
		int		value;
	};

	split		*avail;
	split		*pool;
	split		*allocs;

#if 1
	struct iterator {
		split	*i;
		uint32	x0, x1, y0, y1;

		iterator(split *_i, uint32 w, uint32 h) : i(_i), x0(0), x1(w), y0(0), y1(h)	{}
		operator bool()		{ return i != 0; }
		split *operator->()	{ return i; }

		uint32	width()		{ return x1 - x0; }
		uint32	height()	{ return y1 - y0; }
		uint32	divider()	{ int v = i->value; return v > 0 ? x0 - x1 + v : y0 - y1 - v; }
		bool	vert()		{ return i->value < 0;	}
		dir		direction()	{ return vert() ? VERT : HORZ;	}

		void	next(int s) {
			if (int v = i->value) {
				uint32	a	= v > 0 ? x0 - x1 + v : y0 - y1 - v;
				if (split *n = i->link[s]) {
					(v > 0 ? (s ? x0 : x1) : (s ? y0 : y1)) = a;
					i	= n;
					return;
				} else if (split *n = i->link[1 - s]) {
					(v > 0 ? (s ? x1 : x0) : (s ? y1 : y0)) = a;
					i	= n;
					return;
				}
			}
			while (split *p = i->parent) {
				int		v	= p->value;
				int		s1	= s;
				if (i == p->link[s]) {
					s1 = 1 - s;
					if (split *n = p->link[s1]) {
						i = n;
						if (v > 0) {
							if (s) {
								uint32	t = x0;
								x0	= x0 + x1 - v;
								x1	= t;
							} else {
								uint32	t = x1;
								x1	= x0 - x1 + v;
								x0	= t;
							}
						} else {
							if (s) {
								uint32	t = y0;
								y0	= y0 + y1 + v;
								y1	= t;
							} else {
								uint32	t = y1;
								y1	= y0 - y1 - v;
								y0	= t;
							}
						}
						return;
					}
				}
				i = p;
				if (v > 0) {
					if (s1)
						x1	= x0 - x1 + v;
					else
						x0	= x0 + x1 - v;
				} else {
					if (s1)
						y1	= y0 - y1 - v;
					else
						y0	= y0 + y1 + v;
				}
			}
			i = 0;
		}
		void	up() {
			split	*p	= i->parent;
			int		v	= p->value;
			int		s	= i == p->link[1];
			if (v > 0) {
				if (s)
					x0	= x0 + x1 - v;
				else
					x1	= x0 - x1 + v;
			} else {
				if (s)
					y0	= y0 + y1 + v;
				else
					y1	= y0 - y1 - v;
			}
			i = p;
		}

	};

	iterator	begin()	{ return iterator(avail, width, height);	}
#endif

	split		*new_split(split *parent, int value) {
		split	*i	= pool;
		pool		= i->parent;
		i->parent	= parent;
		i->value	= value;
		return i;
	}

	void		del_split(split *i) {
		i->parent = pool;
		pool = i;
	}

	split		*root(split *i, dir d, int s) {
		split *p;
		for (;;) {
			p = i->parent;
			if (!p || p->value * d < 0)
				return 0;
			if (i != p->link[s])
				return p;
			i = p;
		}
	}

	// -1 - wrong split, 0 - free, 1 - used
	int		last_state(split *i, dir d, int s) {
		while (i && i->value) {
			if (i->value * d < 0)
				return -1;
			i = i->link[s];
		}
		return !i;
	}

	void	adjust(split *i, dir d, int s, int e) {
		while (i && i->value * d > 0) {
			i->value += e * d;
			i = i->link[s];
		}
	}

	split		*split_dir(split *i, dir d, int s, uint32 v0, uint32 v1, uint32 v, bool free) {
		if (v == (s ? v1 : v0))
			return i;
		i->value			= (v + v1 - v0) * d;
		i->link[s]			= free ? new_split(i, 0) : 0;
		return i->link[1-s]	= new_split(i, 0);
	}

public:
	uint32	width, height;

	void	init(uint32 _width, uint32 _height, int N)	{
		width	= _width;
		height	= _height;
		allocs	= new split[N];
		for (int i = 0; i < N - 1; i++)
			allocs[i].parent = &allocs[i + 1];
		allocs[N - 1].parent = 0;
		pool	= &allocs[0];
		avail	= new_split(0, 0);
	}

	void	print();

	void	get_ext(split *i, uint32 &x, uint32 &y, uint32 &w, uint32 &h) {
		uint32	x0 = 0, y0 = 0, x1 = width, y1 = height;
		if (i)
		while (split *p = i->parent) {
			int		v	= p->value;
			if (v > 0)
				(i == p->link[0] ? x1 : x0) = x0 - x1 + v;
			else
				(i == p->link[0] ? y1 : y0) = y0 - y1 - v;
		}
		x = x0;
		y = y0;
		w = x1 - x0;
		h = y1 - y0;
	}

	bool	alloc(uint32 &x, uint32 &y, uint32 w, uint32 h, uint32 xalign = 1, uint32 yalign = 1) {
		uint32	x0 = 0, y0 = 0, x1 = width, y1 = height;
		uint32	best_x0 = 0, best_y0 = 0, best_x1 = 0, best_y1 = 0, best_score = x1 + y1 + 1;
		split	*best_split	= 0;
		split	*i	= avail;
		xalign--;
		yalign--;

		while (i) {
			int	v	= i->value;
			if (v > 0) {
				uint32	s	= x0 - x1 + v;
				if (w <= s - x0 && i->link[0]) {
					x1	= s;
					i	= i->link[0];
					continue;
				} else if (w <= x1 - s && i->link[1]) {
					x0	= s;
					i	= i->link[1];
					continue;
				}
			} else if (v < 0) {
				uint32	s	= y0 - y1 - v;
				if (h <= s - y0 && i->link[0]) {
					y1	= s;
					i	= i->link[0];
					continue;
				} else if (h <= y1 - s && i->link[1]) {
					y0	= s;
					i	= i->link[1];
					continue;
				}
			} else if (((x0 + xalign) & ~xalign) + w <= x1 && ((y0 + yalign) & ~yalign) + h <= y1) {
				uint32	score = x1 - x0 + y1 - y0;
				if (score < best_score) {
					best_score	= score;
					best_split	= i;
					best_x0		= x0;
					best_y0		= y0;
					best_x1		= x1;
					best_y1		= y1;
				}
			}

			do {
				split *p = i->parent;
				if (p) {
					int		v	= p->value;
					if (i == p->link[0]) {
						if (v > 0) {
							uint32	s = x1;
							x1 = x0 - x1 + v;
							if (x1 - s > w && p->link[1]) {
								x0	= s;
								i	= p->link[1];
								break;
							}
						} else {
							uint32	s = y1;
							y1 = y0 - y1 - v;
							if (y1 - s > h && p->link[1]) {
								y0	= s;
								i	= p->link[1];
								break;
							}
						}
					} else {
						if (v > 0) {
							x0 = x0 + x1 - v;
						} else {
							y0 = y0 + y1 + v;
						}
					}
				}
				i = p;
			} while (i);
		}

		if (split *i = best_split) {
			x	= (best_x1 - w) & ~xalign;
			y	= (best_y1 - h) & ~yalign;
//			x	= (best_x0 + xalign) & ~xalign;
//			y	= (best_y0 + yalign) & ~yalign;
			uint32	x0 = best_x0, y0 = best_y0, x1 = best_x1, y1 = best_y1;

			if (y - y0 < x - x0) {
				i = split_dir(i, VERT, 0, y0, y1, y, true);
				i = split_dir(i, HORZ, 0, x0, x1, x, true);
			} else {
				i = split_dir(i, HORZ, 0, x0, x1, x, true);
				i = split_dir(i, VERT, 0, y0, y1, y, true);
			}
			x0	= x;
			y0	= y;

			if (y1 - (y + h) < x1 - (x + w)) {
				i = split_dir(i, VERT, 1, y0, y1, y + h, true);
				i = split_dir(i, HORZ, 1, x0, x1, x + w, true);
			} else {
				i = split_dir(i, HORZ, 1, x0, x1, x + w, true);
				i = split_dir(i, VERT, 1, y0, y1, y + h, true);
			}
			x1	= x + w;
			y1	= y + h;

			split	*p	= i->parent;
			int		s	= i == p->link[1];
			int		v	= p->value;

			while (p->link[1 - s] == 0) {
				if (v > 0)
					(s ? x0 : x1) = x0 - x1 + v;
				else
					(s ? y0 : y1) = y0 - y1 - v;

				p->link[s] = 0;
				del_split(i);

				i	= p;
				p	= i->parent;
				if (p == 0) {
					avail = 0;
					del_split(i);
					return true;
				}
				s	= i == p->link[1];
				v	= p->value;
			}

			p->link[s] = 0;
			del_split(i);

			dir		d	= v > 0 ? HORZ : VERT;
			split	*r	= root(p, d, s);

			if (r && last_state(r->link[s], d, 1) == 1) {
				split *sib = p->link[1 - s];
				if (p->value = sib->value) {
					if (p->link[0] = sib->link[0])
						p->link[0]->parent = p;
					if (p->link[1] = sib->link[1])
						p->link[1]->parent = p;
				}
				del_split(sib);
				int		e	= v > 0 ? x1 - x0 : y1 - y0;
				if (s)
					e = -e;
				adjust(r->link[1], d, 0, -e);
				adjust(r->link[0], d, 1, +e);
				r->value += e * d;
			}

			return true;
		}
		return false;
	}


	bool	free(uint32 x, uint32 y, uint32 w, uint32 h) {
		uint32	x0 = 0, y0 = 0, x1 = width, y1 = height;
		split	*i	= avail;

		if (!i) {
			avail = i = new_split(0, 0);

		} else for (;;) {
			int	v = i->value;
			if (v > 0) {
				int	s = x0 - x1 + v;
				if (x < s) {
					x1	= s;
					if (!i->link[0]) {
						i = i->link[0] = new_split(i, 0);
						break;
					}
					i	= i->link[0];
				} else {
					x0	= s;
					if (!i->link[1]) {
						i = i->link[1] = new_split(i, 0);
						break;
					}
					i	= i->link[1];
				}
			} else if (v < 0) {
				int	s = y0 - y1 - v;
				if (y < s) {
					y1	= s;
					if (!i->link[0]) {
						i = i->link[0] = new_split(i, 0);
						break;
					}
					i	= i->link[0];
				} else {
					y0	= s;
					if (!i->link[1]) {
						i = i->link[1] = new_split(i, 0);
						break;
					}
					i	= i->link[1];
				}
			} else {
				return false;
			}
		}

		i = split_dir(i, HORZ, 0, x0, x1, x,		false);
		i = split_dir(i, VERT, 0, y0, y1, y,		false);
		x0 = x;
		y0 = y;
		i = split_dir(i, HORZ, 1, x0, x1, x + w,	false);
		i = split_dir(i, VERT, 1, y0, y1, y + h,	false);
		x1	= x + w;
		y1	= y + h;

		split	*p	= i->parent;
		int		s	= i == p->link[1];
		split	*sib = p->link[1 - s];
		int		v	= p->value;

		if (sib && sib->value == 0) {
			if (v > 0)
				(s ? x0 : x1) = x0 - x1 + v;
			else
				(s ? y0 : y1) = y0 - y1 - v;

			del_split(i);
			del_split(sib);
			p->value	= 0;
			i			= p;

			p	= i->parent;
			s	= i == p->link[1];
			v	= p->value;
			sib = p->link[1 - s];

			if (sib && sib->value == 0) {
				del_split(i);
				del_split(sib);
				p->value	= 0;
				return true;
			}
		}

		dir		d	= v > 0 ? HORZ : VERT;
		split	*r	= root(p, d, s);

		if (r && last_state(r->link[s], d, 1) == 0) {
			if (split *sib = p->link[1 - s]) {
				p->value = sib->value;
				if (p->link[0] = sib->link[0])
					p->link[0]->parent = p;
				if (p->link[1] = sib->link[1])
					p->link[1]->parent = p;
				del_split(sib);
			} else {
				split	*g = p->parent;
				g->link[p == g->link[1]] = 0;
				del_split(p);
			}
			int		e	= v > 0 ? x1 - x0 : y1 - y0;
			if (s)
				e = -e;
			adjust(r->link[1], d, 0, -e);
			adjust(r->link[0], d, 1, +e);
			r->value += e * d;
		}

		return true;
	}

};

}

#endif
