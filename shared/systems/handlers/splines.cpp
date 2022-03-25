#include "splines.h"
#include "object.h"
#include "utilities.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Spline
//-----------------------------------------------------------------------------
Spline::Spline(const ent::Spline &spline)
	: beziers(spline.pts.Count() / 3)
{
	const float3p *pts = &spline.pts[0];
	for (Beziers::iterator iter = beziers.begin(), end = beziers.end(); iter != end; ++iter, pts += 3) {
		*iter = bezier_spline(
			float4(pts[0], zero),
			float4(pts[1], zero),
			float4(pts[2], zero),
			float4(pts[3], zero)
		);
	}
}

Spline::Spline(const dynamic_array<position3> &pts)
	: beziers(pts.size() - 1)
{
	ISO_ASSERT(pts.size() > 1);
	// outer
	for (size_t i = 0, j = 1, count = pts.size() - 1; i < count; ++i, ++j) {
		beziers[i].c0 = float4(pts[i], zero);
		beziers[i].c3 = float4(pts[j], zero);
	}
	// inner
	size_t i = 0;
	float1 r3 = reciprocal(3.0f);
	float1 dist0 = len(beziers[i].c3 - beziers[i].c0) * r3;
	for (size_t j = 1, count = beziers.size() - 1; i < count; ++i, ++j) {
		float4 dir = normalise(beziers[j].c3 - beziers[i].c0);
		float1 dist1 = len(beziers[j].c3 - beziers[j].c0) * r3;
		beziers[i].c2 = beziers[i].c3 - dir * dist0;
		beziers[j].c1 = beziers[j].c0 + dir * dist1;
		dist0 = dist1;
	}
	// term
	beziers[0].c1 = beziers[0].c0 + (beziers[0].c3 - beziers[0].c0) * r3;
	beziers[i].c2 = beziers[i].c3 - (beziers[i].c3 - beziers[i].c0) * r3;
}

float1 Spline::Span(float t) const
{
	// precompute
	if (beziers[beziers.size() - 1].c3.w == zero) {
		float1 total = zero;
		for (Beziers::iterator iter = beziers.begin(), end = beziers.end(); iter != end; ++iter) {
			iter->c0.w = total;
			iter->c1.w = total + length(iter->split_left(1.0f / 3.0f));
			iter->c2.w = total + length(iter->split_left(2.0f / 3.0f));
			iter->c3.w = total += length(*iter);
		}
	}
	size_t n = size_t(t);
	ISO_ASSERT(n <= beziers.size());
	return n < beziers.size() ? beziers[n].evaluate(t - n).w : beziers[n - 1].c3.w;
}

float1 Spline::Span(float t0, float t1, bool loop) const
{
	float1 s = Span(t1) - Span(t0);
	if (loop) {
		float1 l = Length();
		if (s < -0.5f * l)
			s += l;
		else if (s > 0.5f * l)
			s -= l;
	}
	return s;
}

float4 Spline::ClosestPoint(param(position3) pos, float *t, float *d) const
{
	// best chord
	size_t i = 0;
	size_t best_i = -1;
	float1 best_d2 = 1e6f;
	for (Beziers::const_iterator iter = beziers.begin(), end = beziers.end(); iter != end; ++iter, ++i) {
		float3 v0 = iter->c3.xyz - iter->c0.xyz;
		float3 v1 = pos - iter->c0.xyz;
		float1 r0 = reciprocal(dot(v0, v0));
		float1 p = clamp(dot(v1, v0) * r0, zero, one);
		float1 d2 = len2(v1 - v0 * p);
		if (d2 < best_d2) {
			best_i = i;
			best_d2 = d2;
		}
	}
	// refine
	float _t = best_i;
	float4 _pos = ClosestPointFrom(pos, &_t, d);
	if (t)
		*t = _t;
	return _pos;
}

float4 Spline::ClosestPointFrom(param(position3) pos, float *t, float *d) const
{
	ISO_ASSERT(*t <= beziers.size());
	bool loop = IsLoop();
	size_t l = beziers.size() - 1;
	size_t i = *t < beziers.size() ? size_t(*t) : l;
	float4 _pos;
	do {
		// step over, ignore end points
		float _t = *t;
		_pos = beziers[i].closest_point(pos, t, d);
		if (*t == zero && _t != one) {
			if (i)
				--i;
			else if (loop)
				i = l;
			else
				break;
		} else if (*t == one && _t != zero) {
			if (i != l)
				++i;
			else if(loop)
				i = 0;
			else
				break;
		} else
			break;
	} while (1);
	// wrap
	if (size_t(*t += i) == beziers.size())
		*t = zero;
	return _pos;
}

float Spline::ClosestChordFit(float t, float chord, dynamic_array<float> &params) const
{
	size_t iterate = 8;
	float epsilon = 1e-3f;

	size_t count = 0;
	bool loop = IsLoop();
	float loop_t = loop ? t + beziers.size() : beziers.size();
	position3 loop_pos = Evaluate(loop ? t : beziers.size()).xyz;
	while (1) {
		// walk
		float wrap_t = 0.0f;
		float t0, t1 = t;
		do {
			t1 = AdvanceChordLength(t0 = t1, chord, loop, false);
			if (t1 < t0)
				wrap_t += beziers.size();
			params.push_back(t0);
		} while (t1 + wrap_t < loop_t);

		// delta
		size_t n = params.size() - 1;
		float d = len(loop_pos - Evaluate(t0).xyz);
		// count
		if (!count) {
			count = n;
			if (d > chord * 0.5f)
				++count;
		}
		// invert
		if (n < count) {
			params.push_back(t1);
			d = d - chord;
		}
		// predicate
		if (!iterate-- || abs(d) < epsilon)
			break;
		// converge
		chord += d / n;
		params.clear();
	}
	return chord;
}

float Spline::AdvanceArcLength(float t, float distance, bool loop, bool trim) const
{
	while (1) {
		// advance
		float rspeed = rlen(Tangent(t));
		float _t = t + distance * rspeed;
		if (_t < 0.0f) {
			// lower bound
			if (loop) {
				distance += t / rspeed;
				t = beziers.size();
				continue;
			} else if (trim)
				_t = 0.0f;

		} else if (_t > beziers.size()) {
			// upper bound
			if (loop) {
				distance -= (beziers.size() - t) / rspeed;
				t = 0.0f;
				continue;
			} else if (trim)
				_t = beziers.size();
		}
		t = _t;
		break;
	}
	return t;
}

float Spline::AdvanceChordLength(float t, float distance, bool loop, bool trim) const
{
	size_t i = 0;
	float d = distance;
	position3 p = Evaluate(t).xyz;
	do {
		// advance
		t += d * rlen(Tangent(t));
		if (loop) {
			// lower, upper bound, 
			if (t < 0.0f)
				t = beziers.size() - t;
			else if (t > beziers.size())
				t = t - beziers.size();
		}
		// adjust
		d = distance - len(Evaluate(t).xyz - p);
	} while (abs(d) > 1e-3f && ++i < 8);

	// clamp
	if (!loop && trim)
		t = clamp(t, 0.0f, float(beziers.size()));
	return t;
}

bool Spline::Intersect(param(ray2) r, float4 *pos, float1 *t) const
{
	size_t i = 0;
	for (Beziers::const_iterator iter = beziers.begin(), end = beziers.end(); iter != end; ++iter, ++i) {
		if (intersect(*iter, r, pos, t)) {
			if (t)
				*t += float(i);
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//	Splines
//-----------------------------------------------------------------------------
namespace ent {
	struct Splines {
		ISO_openarray<ISO_ptr<ent::Spline> > splines;
	};
}
static ISO_openarray<ISO_ptr<ent::Spline> > splines;

static struct _Splines
	: TypeHandler<ent::Splines>
	, Handles2<_Splines, WorldEnd>
{
	_Splines()
		: TypeHandler<ent::Splines>(this)
	{}
	void Create(const CreateParams &cp, ISO_ptr<ent::Splines> t) {
		splines = t->splines;
	}
	void operator()(WorldEnd *m) {
		splines = ISO_openarray<ISO_ptr<ent::Spline> >();
	}
} thSplines;

ISO_ptr<ent::Spline> Splines::At(size_t i)
{
	return i < splines.Count() ? splines[i] : ISO_ptr<ent::Spline>();
}

ISO_ptr<ent::Spline> Splines::Find(crc32 id)
{
	for (size_t i = 0; i < splines.Count(); ++i) {
		ISO_ptr<ent::Spline> p = splines[i];
		if (p && p.ID() == id)
			return p;
	}
	return ISO_NULL;
}

namespace iso {
	ISO_DEFUSERCOMPX(ent::Splines, 1, "Splines") {
		ISO_SETFIELD(0, splines);
	}};
}
