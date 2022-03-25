#ifndef PHYSICS_H
#define PHYSICS_H

#include "dynamics.h"

namespace ent {
	struct MassProperties {
		float			mass;
		iso::float3p	centre;
		iso::float3p	Ia;
		iso::float3p	Ib;
		iso::MassProperties	GetMassProperties(float density);
	};
}
namespace iso {
	ISO_DEFUSERCOMPX(ent::MassProperties, 4, "MassProperties") {
		ISO_SETFIELDS4(0, mass, centre, Ia, Ib);
	}};
}

#endif