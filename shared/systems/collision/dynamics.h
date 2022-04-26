#ifndef DYNAMICS_H
#define DYNAMICS_H

#include "object.h"
#include "maths/geometry.h"
#include "base/list.h"

namespace iso {

class DynamicsSystem;

//-----------------------------------------------------------------------------
// Mass
//-----------------------------------------------------------------------------

struct MassProperties {
	float3x3		inertia;
	position3		centre;
	float			mass;

	MassProperties()	{}
	MassProperties(const _zero &z) : inertia(zero, zero, zero), centre(zero), mass(0) 	{}
	MassProperties(float _mass, param(float3x3) _inertia, param(position3) _centre)
		: inertia(_inertia), centre(_centre), mass(_mass) {}
	MassProperties(param(sphere) s, float density)	{
		mass	= volume(s) * density;
		centre	= s.centre();
		inertia = (float3x3)scale(0.4f * mass * s.radius2());
	}
	MassProperties(param(cuboid) s, float density) {
		mass	= volume(s) * density;
		centre	= s.centre();
		float3 t = square(s.extent()) * (mass / 12.0f);
		inertia = (float3x3)scale(rotate<1>(t) + rotate<2>(t));
	}
};


auto rotate_inertia(const symmetrical3 &inertia, const float3x3 &rot) {
	return rot * inertia * transpose(rot);
}

auto scale_inertia(const diagonal3 &inertia, float3 s) {
	auto	d	= (inertia.trace() * half - inertia.d) * square(s);
	return d.zxy + d.yzx;
}

auto scale_inertia(const symmetrical3 &inertia, float3 s) {
	auto	d	= (inertia.trace() * half - inertia.d) * square(s);
	return symmetrical3(d.zxy + d.yzx, inertia.o * s * s.yzx);
}

auto inertia_from_translation(float3 t, float mass) {
	auto	t2 = square(t);
	return symmetrical3(t2.yzx + t2.zxy, -(t * t.yzx) * mass);
}


MassProperties operator+(const MassProperties &m1, const MassProperties &m2);
MassProperties operator-(const MassProperties &m1, const MassProperties &m2);
MassProperties operator*(const float3x3 &mat, const MassProperties &m1);
MassProperties operator*(const MassProperties &m1, float density_scale);
MassProperties CalcMassProperties(const position3 *vertices, int nfaces, const int *indices);

//-----------------------------------------------------------------------------
//	Entities
//-----------------------------------------------------------------------------

struct MassPropertiesData {
	float		mass;
	float3p		centre;
	float3p		Ia;
	float3p		Ib;
	MassProperties	GetMassProperties(float density) const {
		return MassProperties(
			mass,
			float3x3(
				float3{Ia.x, Ib.x, Ib.z},
				float3{Ib.x, Ia.y, Ib.y},
				float3{Ib.z, Ib.y, Ia.z}
			),
			position3(centre)
		) * density;
	}
};

struct PhysicsData {
	float density;
	float resistance;
	float restitution;
};

struct PhysicsMassData : MassProperties {
	float resistance;
	float restitution;
	PhysicsMassData(param(sphere) s, float density, float resistance, float restitution) : MassProperties(s, density), resistance(resistance), restitution(restitution) {}
	PhysicsMassData(param(cuboid) s, float density, float resistance, float restitution) : MassProperties(s, density), resistance(resistance), restitution(restitution) {}
};

//-----------------------------------------------------------------------------
// RigidBody
//-----------------------------------------------------------------------------

class RigidBody : public e_link<RigidBody>, public aligner<16> {
	friend class Object;
	friend class DynamicsSystem;

	enum RigidBodyFlags {
		RBF_USEALTTIME  = 1 << 0,
	};

	ObjectReference	obj;
	int			flags;
	float3		scle;
	position3	centre;
	float3x3	rI;
	float3x3	rIw;
	float		rM;
	float		M;
	float		restitution;
	float		friction;

	position3	position;
	quaternion	orient;
	float3		linear_velocity;
	float3		angular_velocity;

	float3		force;
	float3		torque;
//public:
	RigidBody();
	RigidBody(const MassProperties &m, Object *obj);
	RigidBody(const MassProperties &m, param(float3x4) matrix);
	~RigidBody();
public:
	void		UpdateWorldInvInertia();

	MassProperties	GetMassProperties()	const;
	void		SetMassProperties(const MassProperties &m);
	void		AddMass(const MassProperties &m);
	void		SubtractMass(const MassProperties &m);

	float		Restitution()		const	{ return restitution;		}
	float		Friction()			const	{ return friction;			}
	float		Mass()				const	{ return M;					}
	float		InvMass()			const	{ return rM;				}
	position3	CentreOfGravity()	const	{ return centre;			}
	float3x3	WorldInvInertia()	const	{ return rIw;				}
	float3x4	GetMatrix()			const	{ return translate(position) * float3x3(orient) * translate(-centre) * scale(scle);	}

	position3	Position()			const	{ return position;			}
	quaternion	Orientation()		const	{ return orient;			}
	float3		Velocity()			const	{ return linear_velocity;	}
	float3		AngularVelocity()	const	{ return angular_velocity;	}

	void		SetRestitution(float r)						{ restitution		= r; }
	void		SetFriction(float f)						{ friction			= f; }
	void		SetPosition(param(position3) p)				{ position			= p; }
	void		SetOrientation(param(quaternion) q)			{ orient			= q.closest(orient); }
	void		SetVelocity(param(float3) v)				{ linear_velocity	= v; }
	void		SetAngularVelocity(param(float3) v)			{ angular_velocity	= v; }

	position3	CalcPosition(float dt)				const	{ return position + linear_velocity * dt;}
	quaternion	CalcOrientation(float dt)			const	{ return quaternion(normalise(concat(angular_velocity * (half * dt), one))) * orient; }
	float3x4	CalcMatrix(float dt)				const;

	float		Denominator(param(position3) p, param(float3) n) const;
	float		LocalDenominator(param(position3) p, param(float3) n) const;
	float3		VelocityAt(param(position3) p)		const	{ return linear_velocity + cross(p - position, angular_velocity); }
	float3		VelocityAtOffset(param(float3) v)	const	{ return linear_velocity + cross(v, angular_velocity); }

	position3	ToLocal(param(position3) p)			const	{ return position3(centre + ~orient * (p - position));	}
	float3		ToLocal(param(float3) v)			const	{ return ~orient * v;									}
	position3	ToWorld(param(position3) p)			const	{ return position3(position + orient * (p - centre));	}
	float3		ToWorld(param(float3) v)			const	{ return orient * v;									}
	position3	ToWorldOffset(param(position3) p)	const	{ return position3(orient * (p - centre));				}

	void		AddForce(param(float3) f, param(position3) p)		{ force	+= f; torque += cross(f, p - position); }
	void		AddForceOffset(param(float3) f, param(float3) v)	{ force	+= f; torque += cross(f, v); }
	void		AddForce(param(float3) f)							{ force	+= f;	}
	void		AddTorque(param(float3) t)							{ torque += t;	}

	void		AddImpulse(param(float3) f, param(position3) p)		{ if (rM) { linear_velocity += f * rM; angular_velocity	+= rIw * cross(f, p - position);} }
	void		AddImpulseOffset(param(float3) f, param(float3) v)	{ if (rM) { linear_velocity += f * rM; angular_velocity	+= rIw * cross(f, v);			} }
	void		AddImpulse(param(float3) f)							{ if (rM) linear_velocity += f * rM;	}
	void		AddTorqueImpulse(param(float3) t)					{ if (rM) angular_velocity+= rIw * t;	}

	void		AddVelocityAt(param(position3) p, param(float3) n, float v);
	void		SetVelocityAt(param(position3) p, param(float3) n, float v);

	void		UpdateVel(float dt);
	void		UpdatePos(float dt);
	void		ResetForces(param(float3) g);

	bool		GetUseAltTimeScale() const				{ return !!(flags & RBF_USEALTTIME); }
	void		SetUseAltTimeScale(bool use = true)		{ if (use) flags |= RBF_USEALTTIME; else flags &= ~RBF_USEALTTIME; }
};

struct RigidBodyState {
	position3	position;
	quaternion	orient;
	float3		linear_velocity;
	float3		angular_velocity;

	RigidBodyState()	{}
	RigidBodyState(const RigidBody *rb) {
		position			= rb->Position();
		orient				= rb->Orientation();
		linear_velocity		= rb->Velocity();
		angular_velocity	= rb->AngularVelocity();
	}
	void	Update(RigidBody *rb) {
		rb->SetPosition(position);
		rb->SetOrientation(orient);
		rb->SetVelocity(linear_velocity);
		rb->SetAngularVelocity(angular_velocity);
		rb->UpdateWorldInvInertia();
	}
};

struct CompressedRigidBodyState {
	float3p				position;
	compressed_quaternion	orient;
	compressed_vector3<128>	linear_velocity;
	compressed_vector3<16>	angular_velocity;

	CompressedRigidBodyState()	{}
	CompressedRigidBodyState(const RigidBody *rb) {
		position			= rb->Position().v;
		orient				= rb->Orientation();
		linear_velocity		= rb->Velocity();
		angular_velocity	= rb->AngularVelocity();
	}
	void	Update(RigidBody *rb) {
//		rb->SetPosition(lerp(rb->Position(), position, 0.1f));
//		rb->SetOrientation(lerp(rb->Orientation(), orient, 0.1f));
		rb->SetPosition(position3(position));
		rb->SetOrientation(orient);
		rb->SetVelocity(linear_velocity);
		rb->SetAngularVelocity(angular_velocity);
		rb->UpdateWorldInvInertia();
	}
};

//-----------------------------------------------------------------------------
// Constraint
//-----------------------------------------------------------------------------

class Constraint : public e_link<Constraint>, public aligner<16> {
protected:
	struct Type {
		template<typename T> static void	thunk_init(Constraint *t, float dt)		{ return static_cast<T*>(t)->init(dt); }
		template<typename T> static bool	thunk_solve(Constraint *t, float dt)	{ return static_cast<T*>(t)->solve(dt); }
		template<typename T> static void	thunk_solveV(Constraint *t)				{ static_cast<T*>(t)->solveV(); }

		void	(*init)(Constraint *t, float dt);
		bool	(*solve)(Constraint *t, float dt);
		void	(*solveV)(Constraint *t);
		template<typename T> Type(T*) : init(&thunk_init<T>), solve(&thunk_solve<T>), solveV(&thunk_solveV<T>) {}
	};
	template<typename T> static Type *GetType(T *t) { static Type type(t); return &type; }
	Type		*type;
	RigidBody	*rb1, *rb2;
public:
	Constraint(Type *t, RigidBody *_rb1, RigidBody *_rb2);
	RigidBody*		GetRigidBody1()	const	{ return rb1;}
	RigidBody*		GetRigidBody2()	const	{ return rb2;}
	void			init(float dt)			{ type->init(this, dt); }
	bool			solve(float dt)			{ return type->solve(this, dt); }
	void			solveV()				{ type->solveV(this); }
};

class ConstraintDefaults : public Constraint {
public:
	void			init(float dt)			{}
	bool			solve(float dt)			{ return true; }
	void			solveV()				{}
	ConstraintDefaults(Type *t, RigidBody *_rb1, RigidBody *_rb2) : Constraint(t, _rb1, _rb2) {}
};

class ConstraintTranslation : public ConstraintDefaults {
protected:
	position3	r1, r2;
	ConstraintTranslation(Type *type, RigidBody *_rb1, RigidBody *_rb2, param(position3) pos)
		: ConstraintDefaults(type, _rb1, _rb2)
		, r1(rb1->ToLocal(pos))
		, r2(rb2->ToLocal(pos))
	{}
	ConstraintTranslation(Type *type, RigidBody *_rb1, RigidBody *_rb2, param(position3) p1, param(position3) p2)
		: ConstraintDefaults(type, _rb1, _rb2)
		, r1(rb1->ToLocal(p1))
		, r2(rb2->ToLocal(p2))
	{}
public:
	void	SetPosition1(param(position3) pos) { r1 = rb1->ToLocal(pos); }
	void	SetPosition2(param(position3) pos) { r2 = rb2->ToLocal(pos); }
};

class ConstraintPoint : public ConstraintTranslation {
public:
	ConstraintPoint(RigidBody *_rb1, RigidBody *_rb2, param(position3) pos)
		: ConstraintTranslation(GetType(this), _rb1, _rb2, pos) {}
	bool	solve(float dt);
	void	solveV();
};

class ConstraintLine : public ConstraintTranslation {
	float3		dir;
public:
	ConstraintLine(RigidBody *_rb1, RigidBody *_rb2, param(position3) pos, param(float3) _dir)
		: ConstraintTranslation(GetType(this), _rb1, _rb2, pos)
		, dir(rb1->ToLocal(_dir))
	{}
	bool	solve(float dt);
	void	solveV();
};

class ConstraintPlane : public ConstraintTranslation {
	float3		n;
public:
	ConstraintPlane(RigidBody *_rb1, RigidBody *_rb2, param(position3) pos, param(float3) normal)
		: ConstraintTranslation(GetType(this), _rb1, _rb2, pos)
		, n(rb1->ToLocal(normal))
	{}
	bool	solve(float dt);
};

class Constraint3Axes : public ConstraintDefaults {
	quaternion	q1, q2;
public:
	Constraint3Axes(RigidBody *_rb1, RigidBody *_rb2)
		: ConstraintDefaults(GetType(this), _rb1, _rb2)
		, q1(rb1->Orientation())
		, q2(rb2->Orientation())
	{}
	bool	solve(float dt);
};

class Constraint2Axes : public ConstraintDefaults {
	float3		a1, a2;
public:
	Constraint2Axes(RigidBody *_rb1, RigidBody *_rb2, param(float3) axis)
		: ConstraintDefaults(GetType(this), _rb1, _rb2)
		, a1(rb1->ToLocal(axis))
		, a2(rb2->ToLocal(axis))
	{}
	bool	solve(float dt);
};

class Constraint1Axis : public ConstraintDefaults {
	float3		a1, a2;
	float		phi;
public:
	Constraint1Axis(RigidBody *_rb1, RigidBody *_rb2, param(float3) axis1, param(float3) axis2)
		: ConstraintDefaults(GetType(this), _rb1, _rb2)
		, a1(rb1->ToLocal(axis1))
		, a2(rb2->ToLocal(axis2))
	//	, phi(dot(axis1, axis2))
		, phi(atan2(dot(axis1, axis2), len(cross(axis1, axis2))))
	{}
	void	SetAxis1(param(float3) axis1) { a1 = rb1->ToLocal(axis1); }
	void	SetAxis2(param(float3) axis2) { a2 = rb2->ToLocal(axis2); }
	bool	solve(float dt);
	void	solveV();
};

class ConstraintSpring : public ConstraintTranslation {
	float		rest, spring, damping;
public:
	ConstraintSpring(RigidBody *_rb1, RigidBody *_rb2, param(position3) p1, param(position3) p2, float _rest, float _spring, float _damping)
		: ConstraintTranslation(GetType(this), _rb1, _rb2, p1, p2)
		, rest(_rest)
		, spring(_spring / 2)
		, damping(_damping / 2)
	{}

	float	GetSpring()			const	{ return spring * 2.0f;		}
	float	GetDamping()		const	{ return damping * 2.0f;	}
	void	SetSpring(float _spring)	{ spring	= _spring / 2;	}
	void	SetDamping(float _damping)	{ damping	= _damping / 2;	}

	void	init(float dt);
};

class ConstraintWeeble : public ConstraintDefaults {
	float3		a1, a2;
	float		spring, damping;
public:
	ConstraintWeeble(RigidBody *_rb1, RigidBody *_rb2, param(float3) axis, float _spring, float _damping)
		: ConstraintDefaults(GetType(this), _rb1, _rb2)
		, a1(rb1->ToLocal(axis))
		, a2(rb2->ToLocal(axis))
		, spring(_spring / 2), damping(_damping / 2)
	{}
	ConstraintWeeble(RigidBody *_rb1, RigidBody *_rb2, param(float3) _a1, param(float3) _a2, float _spring, float _damping)
		: ConstraintDefaults(GetType(this), _rb1, _rb2)
		, a1(_a1)
		, a2(_a2)
		, spring(_spring / 2), damping(_damping / 2)
	{}
	void	init(float dt);
};
//-----------------------------------------------------------------------------
// DynamicsSystem
//-----------------------------------------------------------------------------

class DynamicsSystem : RigidBody, public HandlesWorld<DynamicsSystem, FrameEvent>, public DeleteOnDestroy<DynamicsSystem> {//, EVENT_PRIORITY_MAX> {
	e_list<RigidBody>	rigid_bodies;
	e_list<Constraint>	constraints;
	RigidBody			*rb_parent;
	float3				gravity;
public:
	using RigidBody::operator new;
	using RigidBody::operator delete;
	void				SetGravity(param(float3) g)			{ gravity = g;		}
	float3				GetGravity()			const		{ return gravity;	}

	RigidBody*			GetParentRigidBody()	const		{ return rb_parent; }
	void				SetParentRigidBody(RigidBody *rb)	{ rb_parent = rb;	}

	float3				GetParentVelocityAt(param(position3) p)	{ return rb_parent ? rb_parent->VelocityAt(p) : float3(zero); }

	RigidBody*			AddRigidBody(const MassProperties &m, param(float3x4) matrix)	{ RigidBody	*rb = new RigidBody(m, matrix); rigid_bodies.push_back(rb); return rb; }
	RigidBody*			AddRigidBody(const MassProperties &m, Object *obj)				{ RigidBody	*rb = new RigidBody(m, obj); rigid_bodies.push_back(rb); return rb; }

	void				AddConstraint(Constraint *c)									{ constraints.push_back(c);	}
	void				RemoveConstraint(Constraint *c)									{ delete c; }
	RigidBody*			GetRigidBody(Object *obj)										{ if (RigidBody *rb = obj->Property<RigidBody>()) return rb; return this; }

	float				CollisionResponse(RigidBody *rb1, RigidBody *rb2, param(position3) p1, param(position3) p2, param(float3) n, float penetration, float dt);
	void				operator()(FrameEvent &ev);

	void				Create(const CreateParams &cp, crc32 id, const PhysicsData *m);
	void				Create(const CreateParams &cp, crc32 id, const PhysicsMassData *t);

	DynamicsSystem(World *w) : DeleteOnDestroy<DynamicsSystem>(w), rb_parent(0), gravity{0, 0, -9.81f} {
		w->AddHandler<CreateMessage>(Creation<PhysicsData>(this));
		w->AddHandler<CreateMessage>(Creation<PhysicsMassData>(this));
		w->SetProperty((RigidBody*)this);
	}

	static DynamicsSystem	*Get(World *w) { return (DynamicsSystem*)w->Property<RigidBody>(); }
};

}	// namespace iso

ISO_DEFUSERCOMPXV(iso::PhysicsData, "Physics", density, resistance, restitution);
ISO_DEFUSERCOMPXV(iso::MassPropertiesData, "MassProperties", mass, centre, Ia, Ib);

#endif // DYNAMICS_H

