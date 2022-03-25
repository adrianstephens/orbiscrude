#ifndef ROADS_EDITOR_H
#define ROADS_EDITOR_H

#include "roads.h"
#include "extra/geo.h"

namespace iso {

class RoadSegList : public dynamic_array<RoadSeg*> {
public:
	static RoadSegList	TraceID(RoadSeg *rs, float epsilon = 0.001f);
	static RoadSegList	TraceRound(RoadSeg *rs, bool dir, int max = 0);

	RoadSegList()	{}
	RoadSegList(const dynamic_array<Junction2*> &jl);
	RoadSegList(dynamic_array<ent::RoadSeg*> &array);
	void SortStar(const Junction2 *j);

	bool	Loops() const {
		return !!shared_junction(front(), back());
	}

	dynamic_array<Junction*> GetJunctionList() {
		dynamic_array<Junction*>	juncs;
		Junction	*j		= shared_junction((*this)[0], (*this)[1]);
		for (auto &rs : *this)
			juncs.push_back(j = rs->jtn[rs->jtn[0] == j]);
		return juncs;
	}
};

struct LocalSpace {
	const GeoDatum *datum;
	UTM				origin;

	LocalSpace(const GeoDatum *_datum, const UTM &_origin) : datum(_datum), origin(_origin) {}

	position2 UTM2Local(const UTM &utm, const GeoDatum *_datum = 0) const {
		UTM utm_datum = _datum ? utm.to_UTM(_datum, datum) : utm;
		return position2(float(utm_datum.x - origin.x), float(utm_datum.y - origin.y));
	}

	position2 LLA2Local(const LLA &lla, const GeoDatum *_datum = 0) const {
		LLA lla_datum = _datum ? lla.to_LLA(_datum, datum) : lla;
		return UTM2Local(lla_datum.to_UTM(datum->re));
	}
};


} // namespace iso

#endif // ROADS_EDITOR_H

