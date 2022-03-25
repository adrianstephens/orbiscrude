#ifndef GESTURE_H
#define GESTURE_H

#include "base/defs.h"
#include "base/array.h"
#include "base/list.h"
#include "base/vector.h"
#include "base/functions.h"
#include "maths/polygon.h"

namespace iso {

struct Touch {
	enum	PHASE {BEGAN, MOVED, STATIONARY, ENDED, CANCELLED} phase;
	float	time;
	int		taps;
	float2p	pos, prev_pos;
};

struct Gesture;

struct GestureList : e_list<Gesture> {
	static GestureList	root, *current_root;

	GestureList*	SetAsRoot()	{
		return exchange(current_root, this);
	}

	void	Add(Gesture *gesture) {
		if (this)
			push_back(gesture);
		else
			current_root->push_back(gesture);
	}

	void Process(const Touch* touches, int num, Touch::PHASE phase);
	void Process(const Touch& touch, Touch::PHASE phase) { Process(&touch, 1, phase); }
};

struct WithGestureRoot {
	GestureList	*prev;
	WithGestureRoot(GestureList *g)	: prev(g->SetAsRoot())	{}
	~WithGestureRoot()				{ prev->SetAsRoot(); }
};

struct Gesture : e_link<Gesture>, virtfunc<bool(const Touch*, int, Touch::PHASE)> {
	enum FLAGS {ENABLE = 1 << 0, GOT = 1 << 1, USER = 1 << 2};
	flags<FLAGS>		flags;

	template<typename T> Gesture(GestureList *parent, const T*) : flags(ENABLE) {
		bind<T>();
		parent->Add(this);
	}

	static float2	Centroid(const Touch *touches, int num) {
		if (num > 1) {
			float2	p(touches[0].pos);
			for (int i = 1; i < num; i++)
				p += touches[i].pos;
			return p / num;
		}
		if (num)
			return float2(touches[0].pos);
		return float2(zero);
	}

	static float Time(const Touch* touches, int num) {
		if (num > 1) {
			float	time = touches[0].time;
			for (int i = 1; i < num; i++) {
				if (touches[i].time > time)
					time = touches[i].time;
			}
			return time;
		}
		if (num)
			return touches[0].time;
		return 0;
	}

	void	Update(const Touch *touches, int num, Touch::PHASE phase) {
		if (flags.test(ENABLE) && (*this)(touches, num, phase))
			flags.set(GOT);
	}

	void	Enable(bool enable)	{ flags.set(ENABLE, enable); }
	bool	Test()				{ return flags.test(GOT); }
	bool	TestClear()			{ return flags.test_clear(GOT); }
	void	Clear()				{ flags.clear(GOT); }
};

struct GestureGroup : Gesture, GestureList {
	bool operator()(const Touch *touches, int num, Touch::PHASE phase) { return false; }
	GestureGroup(GestureList *parent) : Gesture(parent, this) {}
};

struct GestureFilterRegion : Gesture, GestureList, aligner<16> {
	float2x3	region;

	position2	Relative(param(position2) p)	const	{ return region * p; }
	vector2		Relative(param(vector2) p)		const	{ return region * p; }
	void		SetRegion(param(float2x3) _region)		{ region = _region;	}

	bool operator()(const Touch *touches, int num, Touch::PHASE phase) {
		Touch	touches2[8];
		int		num2 = 0;
		for (int i = 0; i < num; i++) {
			if (unit_rect.contains(region * position2(touches[i].pos)))
				touches2[num2++] = touches[i];
		}
		Process(touches2, num2, phase);
		return false;
	}
	GestureFilterRegion(GestureList *parent, param(float2x3) region) : Gesture(parent, this), region(region) {}
};

struct GestureTransform : Gesture, GestureList, aligner<16> {
	float2x3	trans;

	bool operator()(const Touch *touches, int num, Touch::PHASE phase) {
		Touch	*touches2 = alloc_auto(Touch, num);
		for (int i = 0; i < num; i++) {
			touches2[i].phase		= touches[i].phase;
			touches2[i].time		= touches[i].time;
			touches2[i].taps		= touches[i].taps;
			touches2[i].pos			= (trans * position2(touches[i].pos)).v;
			touches2[i].prev_pos	= (trans * position2(touches[i].prev_pos)).v;
		}
		Process(touches2, num, phase);
		return false;
	}
	GestureTransform(GestureList *parent, param(float2x3) trans) : Gesture(parent, this), trans(trans) {}
};

struct GestureTap : Gesture {
	uint8		num_needed;

	GestureTap(GestureList *parent, uint8 num) : Gesture(parent, this), num_needed(num) {}
	bool	operator()(const Touch *touches, int num, Touch::PHASE phase) {
		return phase == Touch::BEGAN && num == num_needed;
	}
};

struct GestureDrag : Gesture, aligner<16> {
	uint8		num_needed;
	bool		prog;
	float2		start, trans;

	GestureDrag(GestureList *parent, uint8 num) : Gesture(parent, this), num_needed(num), prog(false) {}
	bool	operator()(const Touch *touches, int num, Touch::PHASE phase) {
		switch (phase) {
			case Touch::BEGAN:
				if (prog = num == num_needed) {
					start	= Centroid(touches, num);
					trans	= zero;
				}
				break;
			case Touch::ENDED: case Touch::CANCELLED:
				prog = false;
				break;
			default:
				if (num != num_needed)
					prog = false;
				trans = Centroid(touches, num) - start;
				break;
		}
		return false;
	}
	bool		InProgress()	const		{ return prog;	}
	vector2		Translation()	const		{ return trans;	}
	position2	Start()			const		{ return position2(start);	}
	void		Start(param(position2) pos)	{ start = pos; prog = true; }
};

struct GestureSwipe : Gesture, aligner<16> {
	float2		dir, trans, start;
	uint8		num_needed;
	bool		prog;

	GestureSwipe(GestureList *parent, param(float2) dir, uint8 num) : Gesture(parent, this), dir(dir), num_needed(num) {}
	bool	operator()(const Touch *touches, int num, Touch::PHASE phase) {
		switch (phase) {
			case Touch::BEGAN:
				if (prog = num == num_needed) {
					start	= Centroid(touches, num);
					trans	= zero;
				}
				break;
			case Touch::ENDED:
			case Touch::CANCELLED:
				prog = false;
				break;
			default:
				if (prog) {
					trans = Centroid(touches, num) - start;

					float	d	= len2(dir);
					float	x	= dot(trans, dir);

					if (x > d) {
						prog = false;
						return true;
					}

					if (x < d * touches[0].time)
						prog = false;

				}
				break;
		}
		return false;
	}
	bool		InProgress()	const	{ return prog;	}
	vector2		Translation()	const	{ return trans;	}
	position2	Start()			const	{ return position2(start);	}
};

struct GesturePinch : Gesture, aligner<16> {
	uint8		num_needed;
	float		dist;
	circle		start;
	bool		prog;

	GesturePinch(GestureList *parent, float dist, uint8 num) : Gesture(parent, this), num_needed(num), dist(dist) {}
	bool	operator()(const Touch *touches, int num, Touch::PHASE phase) {
		switch (phase) {
			case Touch::ENDED:
			case Touch::CANCELLED:
				prog = false;
				break;
			case Touch::BEGAN:
				prog = num == num_needed;
			default:
				if (prog) {
					position2	*pos2 = alloc_auto(position2, num);
					for (int i = 0; i < num; i++)
						pos2[i] = position2(touches[i].pos);

					circle	c = minimum_circle(make_range_n(pos2, num));
					if (phase == Touch::BEGAN) {
						start = c;
					} else if ((c.radius() - start.radius()) > dist) {
						prog = false;
						return true;
					}
				}
				break;
		}
		return false;
	}
	bool		InProgress()	const	{ return prog;	}
	position2	Start()			const	{ return start.centre();	}
};

}//namespace iso

#endif	// GESTURE_H
