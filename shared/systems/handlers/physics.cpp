#include "physics.h"
#include "collision.h"
#include "profiler.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	MassProperties
//-----------------------------------------------------------------------------
MassProperties ent::MassProperties::GetMassProperties(float density)
{
	return iso::MassProperties(
		mass,
		float3x3(
			float3(Ia.x, Ib.x, Ib.z),
			float3(Ib.x, Ia.y, Ib.y),
			float3(Ib.z, Ib.y, Ia.z)
		),
		position3(centre)
	) * density;
}

//-----------------------------------------------------------------------------
// Physics
//-----------------------------------------------------------------------------
namespace ent {
	struct Physics {
		float density;
		float resistance;
		float restitution;
	};
}

namespace iso {
	ISO_DEFUSERCOMPX(ent::Physics, 3, "Physics") {
		ISO_SETFIELDS3(0, density, resistance, restitution);
	}};
	template<> void TypeHandler<ent::Physics>::Create(const CreateParams &cp, ISO_ptr<ent::Physics> t) {
		if (ent::MassProperties *mp = cp.obj->FindType<ent::MassProperties>()) {
//			RigidBody *rb = new RigidBody(mp->GetMassProperties(t->density), cp.obj->Detach());
			cp.obj->Detach();
			RigidBody *rb = dynamics.AddRigidBody(mp->GetMassProperties(t->density), cp.obj);
			rb->SetRestitution(t->restitution);
			rb->SetFriction(t->resistance);
		}
	}
	static TypeHandler<ent::Physics> thPhysics;
}

//-----------------------------------------------------------------------------
//	ForceField
//-----------------------------------------------------------------------------
#define MAX_ANGULAR_SPEED	(pi * 2.0f)

namespace ent {
	struct ForceField {
		float radius;
		float strength;
		float life;
		float3x4p matrix;
		ISO_ptr<void> child;
	};
}

class ForceField 
	: public CleanerUpper<ForceField>
{
	ObjectReference		obj;
	float				radius2;
	float				strength;
	float				last_time;
	RigidBody			*last_rb;
	CollisionHandler	collision_handler;

public:
	ForceField(const CreateParams &cp, ent::ForceField *t) 
		: radius2(square(t->radius))
		, strength(t->strength)
		, last_time(0.0f)
		, last_rb(NULL)
		, collision_handler(this)
	{
		// owner
		obj = new Object(cp.obj->GetWorldMat() * cp.matrix * float3x4(t->matrix));
		// collision volume
		obj->AddEventHandler(collision_handler);
		CollisionSystem::Add(obj, sphere(position3(zero), min(100.0f, t->radius)), CF_NO_PHYSICS);
		// life
		if (t->life > 0)
			cp.world->AddTimer(this, t->life);
	}

	~ForceField() {
		delete obj;
	}

	void operator()(TimerMessage &m) {
		delete this;
	}

	void operator()(CollisionMessage &m) {
		Object	*obj2 = m.c.obj2;
		if (RigidBody *rb = dynamics.GetRigidBody0(obj2)) {
			if (rb != last_rb || GetGlobalTime() != last_time) {
				CollisionItem0 *c = m.c.p2;
				// impulse position
				position3 pos, center = obj->GetPos();
				if (c->Type() == COLL_OBB) {
					obb3	ext	= obj2->GetMatrix() * c->Prim<obb3>();
					pos			= ext.from_unit_cube() * position3(clamp(ext.to_unit_cube() * center, -float3(one), float3(one)));
				} else {
					cuboid	box	= c->GetBox();
					pos			= clamp(center, box.pt0(), box.pt1());
				}
				// force
				vector3 force = pos - center;
				force = force * (1.0f - square(min(1.0f, len2(force) / radius2))) * strength;
				rb->AddImpulse(force * m.dt, pos);
				// clamp angular velocity
				float angular_speed = len(rb->AngularVelocity());
				if (angular_speed > MAX_ANGULAR_SPEED)
					rb->SetAngularVelocity(rb->AngularVelocity() / angular_speed * MAX_ANGULAR_SPEED);
				// tag
				last_time = GetGlobalTime();
				last_rb = rb;
			}
		}
		m.physics = false;
	}
};

namespace iso {
	ISO_DEFUSERCOMPX(ent::ForceField, 5, "ForceField") {
		ISO_SETFIELDS5(0, radius, strength, life, matrix, child);
	}};

	template<> void TypeHandler<ent::ForceField>::Create(const CreateParams &cp, ISO_ptr<ent::ForceField> t) {
		new ForceField(cp, t);
	}
	static TypeHandler<ent::ForceField> thForceField;
}
