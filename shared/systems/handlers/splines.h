#ifndef	SPLINES_H
#define SPLINES_H

#include "geometry.h"
#include "scenegraph.h"

//-----------------------------------------------------------------------------
//	Spline
//-----------------------------------------------------------------------------
class Spline : public iso::aligner<16> {
	typedef iso::dynamic_array<iso::bezier_spline> Beziers;
	mutable Beziers beziers;

public:
	Spline(const ent::Spline &spline);
	Spline(const iso::dynamic_array<iso::position3> &pts);

	size_t Count() const {
		return beziers.size();
	}
	const iso::bezier_spline& At(size_t i) const {
		ISO_ASSERT(i < beziers.size());
		return beziers[i];
	}
	iso::bezier_spline& At(size_t i) {
		ISO_ASSERT(i < beziers.size());
		return beziers[i];
	}
	iso::float1 Span(float t) const;
	iso::float1 Span(float t0, float t1, bool loop) const;
	iso::float1 Length(size_t i) const {
		return Span(i + 1) - Span(i);
	}
	iso::float1 Length() const {
		return Span(beziers.size());
	}
	bool IsLoop() const {
		return beziers.front().c0.xyz == beziers.back().c3.xyz;
	}
	iso::float4 Evaluate(float t) const {
		size_t n = t < 0.0f ? 0 : t < beziers.size() ? size_t(t) : beziers.size() - 1;
		return beziers[n].evaluate(t - n);
	}
	iso::float3 Tangent(float t) const {
		size_t n = t < 0.0f ? 0 : t < beziers.size() ? size_t(t) : beziers.size() - 1;
		return beziers[n].tangent(t - n).xyz;
	}
	iso::float3 Normal(float t) const {
		size_t n = t < 0.0f ? 0 : t < beziers.size() ? size_t(t) : beziers.size() - 1;
		return beziers[n].normal(t - n).xyz;
	}
	iso::float4 ClosestPoint(param(iso::position3) pos, float *t = NULL, float *d = NULL) const;
	iso::float4 ClosestPointFrom(param(iso::position3) pos, float *t, float *d = NULL) const;
	float ClosestChordFit(float t, float chord, iso::dynamic_array<float> &params) const;
	float AdvanceArcLength(float t, float distance, bool loop, bool trim = true) const;
	float AdvanceChordLength(float t, float distance, bool loop, bool trim = true) const;
	bool Intersect(param(iso::ray2) r, iso::float4 *pos = NULL, iso::float1 *t = NULL) const;
};

//-----------------------------------------------------------------------------
//	Splines
//-----------------------------------------------------------------------------
struct Splines {
	typedef iso::ISO_ptr<ent::Spline>* iterator;
	static iterator begin();
	static iterator end();

	static iso::ISO_ptr<ent::Spline> At(size_t i);
	static iso::ISO_ptr<ent::Spline> Find(iso::crc32 id);

	template<typename T> static T Enum(T t) {
		for (iterator iter = Splines::begin(), end = Splines::end(); iter != end; ++iter) {
			if (iso::ISO_ptr<ent::Spline> spline = *iter) {
				if (!t(spline))
					break;
			}
		}
		return t;
	}
};

#endif