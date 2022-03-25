#include "dynamics.h"
#include "collision.h"
#include "base/list.h"
#include "utilities.h"
#include "profiler.h"

namespace iso {

//-----------------------------------------------------------------------------
// Mass
//-----------------------------------------------------------------------------

float3x3 displacement(param(float3) v) {
	float3x3	m = (float3x3)scale(dot(v, v));
	m.x -= v * v.x;
	m.y -= v * v.y;
	m.z -= v * v.z;
	return m;
}

MassProperties operator+(const MassProperties &m1, const MassProperties &m2) {
	MassProperties	m;
	m.mass		= m1.mass + m2.mass;
	m.centre	= (m1.centre * m1.mass + m2.centre * m2.mass) / m.mass;
	m.inertia	= m1.inertia + m2.inertia + displacement(m1.centre - m.centre) * scale(m1.mass) + displacement(m2.centre - m.centre) * scale(m2.mass);
	return m;
}

MassProperties operator-(const MassProperties &m1, const MassProperties &m2) {
	MassProperties	m;
	m.mass		= m1.mass - m2.mass;
	m.centre	= position3((m1.centre * m1.mass - m2.centre * m2.mass) / m.mass);
	m.inertia	= m1.inertia - m2.inertia + displacement(m1.centre - m.centre) * scale(m1.mass) - displacement(m2.centre - m.centre) * scale(m2.mass);
	return m;
}

MassProperties operator*(const float3x4 &mat, const MassProperties &m1) {
	MassProperties	m;
	m.mass		= m1.mass;
	m.centre	= mat * m1.centre;
//	m.inertia	= mat * m1.inertia * inverse(mat);
	m.inertia	= float3x3(mat) * m1.inertia * transpose(float3x3(mat)) * scale(mat.det());
	return m;
}

MassProperties operator*(const MassProperties &m1, float density_scale) {
	return MassProperties(m1.mass * density_scale, scale(density_scale) * m1.inertia, m1.centre);
}

MassProperties CalcMassProperties(const position3 *vertices, int nfaces, const int *indices) {
	//Order:  1, x, y, z, x^2, y^2, z^2, xy, yz, zx
	float	integrals0		= 0;
	float3	integrals123	= float3(zero),
			integrals456	= float3(zero),
			integrals789	= float3(zero);

	for (int i = 0; i < nfaces; i++) {
		float3	v0	= vertices[*indices++].v;
		float3	v1	= vertices[*indices++].v;
		float3	v2	= vertices[*indices++].v;

		float3	n	= cross(v1 - v0, v2 - v0);

		float3	t0	= v0 * v0;
		float3	t1	= t0 + v1 * (v0 + v1);			//00+01+11
		float3	f1	= v0 + v1 + v2,					//0+1+2
				f2	= t1 + v2 * f1,					//00+01+02+11+12+22
				f3	= v0 * t0 + v1 * t1 + v2 * f2;	//000+001+011+111+002+012+022+112+122+222
		float3	g0	= f2 + v0 * (f1 + v0),
				g1	= f2 + v1 * (f1 + v1),
				g2	= f2 + v2 * (f1 + v2);

		integrals0		+= n.x * f1.x;
		integrals123	+= n * f2;
		integrals456	+= n * f3;
		integrals789	+= n * (rotate(v0) * g0 + rotate(v1) * g1 + rotate(v2) * g2);
	}

	integrals0		/= 6;
	integrals123	/= 24.f;
	integrals456	/= 60.f;
	integrals789	/= 120.f;

	MassProperties m;

	//Mass
	m.mass		= integrals0;
	m.centre	= position3(integrals123 / integrals0);
#if 0
	//Inertia relative to origin
	float3		d	= rotate_l(integrals456); d += rotate_l(d);	//y+z z+x x+y
	float3		t	= -integrals789;
	m.inertia	= float3x3(
		float3(d.x(), t.x(), t.z()),
		float3(t.x(), d.y(), t.y()),
		float3(t.z(), t.y(), d.z())
	);

	//Inertia relative to centre of mass
//	if (body_relative)
		m.inertia = m.inertia - m.mass * displacement(m.centre);
#else
	float3		d	= rotate((integrals456 - integrals123 * integrals123 / integrals0)); d += rotate(d);
	float3		t	= -integrals789 + integrals123 * rotate(integrals123) / integrals0;
	m.inertia	= float3x3(
		float3{d.x, t.x, t.z},
		float3{t.x, d.y, t.y},
		float3{t.z, t.y, d.z}
	);
#endif
	return m;
}

//-----------------------------------------------------------------------------
// RigidBody
//-----------------------------------------------------------------------------


float3 get_scale_remove(float3x3 &m) {
	float3	v = get_scale(m);
	m = m / scale(v);
	return v;
}

RigidBody::RigidBody() : obj(NULL), flags(0), linear_velocity(zero), angular_velocity(zero), force(zero), torque(zero) {
	clear(rI);
	M			= 0;
	rM			= 0;
	centre		= position3(zero);
	orient		= identity;
	position	= position3(zero);
	restitution	= 1;
	friction	= 0;
	UpdateWorldInvInertia();
}

RigidBody::RigidBody(const MassProperties &m, Object *_obj) : obj(_obj), flags(0), angular_velocity(zero), force(zero), torque(zero) {
	float3x4	matrix = obj->GetMatrix();
	rI			= inverse(m.inertia);
	rM			= reciprocal(m.mass);
	M			= m.mass;
	scle		= get_scale_remove(get_rot(matrix));
	centre		= m.centre;
	orient		= get_rot(matrix);
	position	= matrix * centre;//.translation();
	restitution	= 1;
	friction	= 0;
	UpdateWorldInvInertia();
	linear_velocity = zero;//dynamics.GetParentVelocityAt(position);
	obj->SetProperty(this);

	CTOR_RETURN
}

RigidBody::RigidBody(const MassProperties &m, param(float3x4) _matrix) : obj(NULL), flags(0), angular_velocity(zero), force(zero), torque(zero) {
	float3x4	matrix = _matrix;
	rI			= inverse(m.inertia);
	rM			= reciprocal(m.mass);
	M			= m.mass;
	scle		= get_scale_remove(get_rot(matrix));
	centre		= m.centre;
	orient		= get_rot(matrix);
	position	= matrix * centre;//.translation();
	restitution	= 1;
	friction	= 0;
	UpdateWorldInvInertia();
	linear_velocity = zero;//dynamics.GetParentVelocityAt(position);

	CTOR_RETURN
}

RigidBody::~RigidBody() {}

void RigidBody::UpdateWorldInvInertia() {
	rIw			= orient * rI * inverse(orient);
}

float RigidBody::Denominator(param(position3) p, param(float3) n) const {
//	float3		r		= p - position;
//	return rM + dot(n, cross(rIw * cross(r, n), r));
// equivalent but faster:
	float3		t		= cross(p - position, n);
	return rM + dot(t, rIw * t);
}

float RigidBody::LocalDenominator(param(position3) p, param(float3) n) const {
//	float3		r		= p - centre;
//	return rM + dot(n, cross(rI * cross(r, n), r));
// equivalent but faster:
	float3		t		= cross(p - centre, n);
	return rM + dot(t, rI * t);
}

void RigidBody::AddVelocityAt(param(position3) p, param(float3) n, float v) {
//	float		k		= rM + dot(n, cross(rIw * cross(r, n), r));
	AddImpulse(n * (v / Denominator(p, n)), p);
}

void RigidBody::SetVelocityAt(param(position3) p, param(float3) n, float v) {
	AddVelocityAt(p, n, v - dot(VelocityAt(p), n));
}

void RigidBody::UpdateVel(float dt) {
	linear_velocity		+= force * (rM * dt);
	angular_velocity	+= rIw * torque * dt;
}

float3x4 RigidBody::CalcMatrix(float dt) const {
	return translate(CalcPosition(dt)) * CalcOrientation(dt) * translate(-centre);// * iso::scale(scale);
}

void RigidBody::UpdatePos(float dt) {
	position	= CalcPosition(dt);
	orient		= CalcOrientation(dt);
	UpdateWorldInvInertia();
}

void RigidBody::ResetForces(param(float3) g) {
	force		= g * M;
	torque		= zero;
}

MassProperties RigidBody::GetMassProperties() const {
	MassProperties	m;
	m.inertia	= inverse(rI);
	m.centre	= centre;
	m.mass		= M;
	return m;
}

void RigidBody::SetMassProperties(const MassProperties &m) {
	rI			= inverse(m.inertia);
	rM			= reciprocal(m.mass);
	M			= m.mass;
	position	= position + orient * float3(m.centre - centre);
	centre		= m.centre;
}

void RigidBody::AddMass(const MassProperties &m) {
	SetMassProperties(GetMassProperties() + m);
}

void RigidBody::SubtractMass(const MassProperties &m) {
	SetMassProperties(GetMassProperties() - m);
}

//-----------------------------------------------------------------------------
// Constraint
//-----------------------------------------------------------------------------

Constraint::Constraint(Type *_type, RigidBody *_rb1, RigidBody *_rb2) : type(_type), rb1(_rb1), rb2(_rb2) {
//	dynamics.AddConstraint(this);
}

bool ConstraintPoint::solve(float dt) {
	position3	w1	= rb1->CalcMatrix(dt) * r1;
	position3	w2	= rb2->CalcMatrix(dt) * r2;
	float3		v	= w2 - w1;
	if (len2(v) < 0.00001f)
		return true;

	float3		t1	= rb1->ToWorldOffset(r1);					//constant for this time step
	float3		t2	= rb2->ToWorldOffset(r2);					//constant for this time step
	float3x3	K	= (float3x3)scale(rb1->InvMass() + rb2->InvMass())	//constant for this time step
					- (skew(t1) * rb1->WorldInvInertia() * skew(t1))
					- (skew(t2) * rb2->WorldInvInertia() * skew(t2));
	float3		i	= (v / K) / dt;

	rb1->AddImpulseOffset( i, t1);
	rb2->AddImpulseOffset(-i, t2);
	return false;
}

void ConstraintPoint::solveV() {
	float3		t1	= rb1->ToWorldOffset(r1);
	float3		t2	= rb2->ToWorldOffset(r2);
	float3		dv	= rb2->VelocityAtOffset(t2) - rb1->VelocityAtOffset(t1);

	float3x3	K	= (float3x3)scale(rb1->InvMass() + rb2->InvMass())	//constant for this time step
					- (skew(t1) * rb1->WorldInvInertia() * skew(t1))
					- (skew(t2) * rb2->WorldInvInertia() * skew(t2));
	float3		i	= dv / K;

	rb1->AddImpulseOffset( i, t1);
	rb2->AddImpulseOffset(-i, t2);
}

bool ConstraintLine::solve(float dt) {
	float3x3	P	= look_along_z(rb1->ToWorld(dir));
//	Matrix		P	= look_along_z(rb1->CalcMatrix(dt) * dir);
	position3	w1	= rb1->CalcMatrix(dt) * r1;
	position3	w2	= rb2->CalcMatrix(dt) * r2;
	float2		v	= (transpose(P) * float3(w2 - w1)).xy;
	if (len2(v) < 0.00001f)
		return true;

	float3		t1	= rb1->ToWorldOffset(r1);					//constant for this time step
	float3		t2	= rb2->ToWorldOffset(r2);					//constant for this time step
	float3x3	K	= (float3x3)scale(rb1->InvMass() + rb2->InvMass())	//constant for this time step
					- (skew(t1) * rb1->WorldInvInertia() * skew(t1))
					- (skew(t2) * rb2->WorldInvInertia() * skew(t2));

	float3x3	K2	= transpose(P) * K * P;
	float2x2	K3(K2.x.xy, K2.y.xy);

	float3		i	= P * concat((v / K3) / dt, zero);

	rb1->AddImpulseOffset( i, t1);
	rb2->AddImpulseOffset(-i, t2);
	return false;
}

void ConstraintLine::solveV() {
	float3x3	P	= look_along_z(rb1->ToWorld(dir));
	float3		t1	= rb1->ToWorldOffset(r1);
	float3		t2	= rb2->ToWorldOffset(r2);
	float3		dv	= rb2->VelocityAtOffset(t2) - rb1->VelocityAtOffset(t1);

	float3x3	K	= (float3x3)scale(rb1->InvMass() + rb2->InvMass())	//constant for this time step
					- (skew(t1) * rb1->WorldInvInertia() * skew(t1))
					- (skew(t2) * rb2->WorldInvInertia() * skew(t2));
	float3x3	K2	= transpose(P) * K * P;
	float2x2	K3(K2.x.xy, K2.y.xy);

	float3		i	= P * concat(dv.xy / K3, zero);

	rb1->AddImpulseOffset( i, t1);
	rb2->AddImpulseOffset(-i, t2);
}

bool ConstraintPlane::solve(float dt) {
	float3		P	= rb1->ToWorld(n);
	position3	w1	= rb1->CalcMatrix(dt) * r1;
	position3	w2	= rb2->CalcMatrix(dt) * r2;
	float		v	= dot(P, float3(w2 - w1));
	if (abs(v) < 0.001f)
		return true;

	position3	p	= rb2->ToWorld(r2);							//constant for this time step

	float3		t1	= rb1->ToWorldOffset(r1);					//constant for this time step
	float3		t2	= rb2->ToWorldOffset(r2);					//constant for this time step
	float3x3	K	= (float3x3)scale(rb1->InvMass() + rb2->InvMass())	//constant for this time step
					- (skew(t1) * rb1->WorldInvInertia() * skew(t1))
					- (skew(t2) * rb2->WorldInvInertia() * skew(t2));

	float		K2	= dot(P, K * P);

	float3		i	= P * (v / K2 / dt);

	rb1->AddImpulse( i, p);
	rb2->AddImpulse(-i, p);
	return false;
}

bool Constraint3Axes::solve(float dt) {
	quaternion	dq	= ~(~q2 * rb2->CalcOrientation(dt))
					* (~q1 * rb1->CalcOrientation(dt));
	float3		d	= (dq.v.xyz + dq.v.xyz) / dq.v.w;
	if (len2(d) < 0.00001f)
		return true;

	float3x3	L	= rb1->WorldInvInertia() + rb2->WorldInvInertia();
	float3		t	= (d / L) / dt;

	rb1->AddTorqueImpulse(-t);
	rb2->AddTorqueImpulse( t);
	return false;
}

bool Constraint2Axes::solve(float dt)
{
	float3		d	= cross(rb1->CalcOrientation(dt) * a1, rb2->CalcOrientation(dt) * a2);
	if (len2(d) < 0.0000001f)
		return true;

	float3x3	P	= look_along_z(rb1->ToWorld(a1));
	float3x3	L	= rb1->WorldInvInertia() + rb2->WorldInvInertia();
	float3x3	L2	= transpose(P) * L * P;
	float2x2	L3(L2.x.xy, L2.y.xy);

	float3		t	= P * concat((inverse(L3) * (transpose(P) * d).xy) / dt, zero);

	rb1->AddTorqueImpulse(-t);
	rb2->AddTorqueImpulse( t);
	return false;
}

bool Constraint1Axis::solve(float dt) {
	float3		a1w	= rb1->CalcOrientation(dt) * a1;
	float3		a2w	= rb2->CalcOrientation(dt) * a2;

	float		d	= atan2(dot(a1w, a2w), len(cross(a1w, a2w))) - phi;
//	float		d	= dot(a1w, a2w) - phi;
	if (abs(d) < 0.0000001f)
		return true;

	float3		P	= cross(a1w, a2w);
	float3x3	L	= rb1->WorldInvInertia() + rb2->WorldInvInertia();
	float		L2	= dot(P, L * P);

	float3		t	= P * (d / L2 / dt);

	rb1->AddTorqueImpulse( t);
	rb2->AddTorqueImpulse(-t);
	return false;
}

void Constraint1Axis::solveV() {
	float3		dw	= rb2->AngularVelocity() - rb1->AngularVelocity();

	float3		a1w	= rb1->Orientation() * a1;
	float3		a2w	= rb2->Orientation() * a2;
	float3		P	= cross(a1w, a2w);

	float3x3	L	= rb1->WorldInvInertia() + rb2->WorldInvInertia();
	float		L2	= dot(P, L * P);

	float3		t	= (P * dot(P, dw)) / L2;

	rb1->AddTorqueImpulse( t);
	rb2->AddTorqueImpulse(-t);
}

void ConstraintSpring::init(float dt) {
	position3	w1	= rb1->ToWorld(r1);
	position3	w2	= rb2->ToWorld(r2);
	float3		dv	= w2 - w1;
	float		d	= len(dv);

	if (d > 0.001f) {
		float3		force	= (dv * spring * (d - rest) / d + (rb2->VelocityAt(w2) - rb1->VelocityAt(w1)) * damping) * dt;
		rb1->AddImpulse( force, w1);
		rb2->AddImpulse(-force, w2);
	}
}

void ConstraintWeeble::init(float dt) {
	float3		d	= cross(rb1->CalcOrientation(dt) * a1, rb2->CalcOrientation(dt) * a2);

	if (len(d) > 0.001f) {
		float3		t	= (d * spring + (rb2->AngularVelocity() - rb1->AngularVelocity()) * damping) * dt;
		rb1->AddTorqueImpulse( t);
		rb2->AddTorqueImpulse(-t);
	}
}

//-----------------------------------------------------------------------------
// DynamicsSystem
//-----------------------------------------------------------------------------

float DynamicsSystem::CollisionResponse(RigidBody *rb1, RigidBody *rb2, param(position3) p1, param(position3) p2, param(float3) n, float penetration, float dt) {
	if (rb1->InvMass() == 0.0f && rb2->InvMass() == 0.0f)
		return 0;

	float3		vv		= rb2->VelocityAt(p2) - rb1->VelocityAt(p1);
	float		v		= dot(vv, n);
/*
	if (penetration > v * dt) {
		if (v > 0) {
			penetration -= v * dt;
			v = min(v * 2, penetration / dt);
		}
		rb1->SetPosition(rb1->Position() + n * (penetration / 2));
		rb2->SetPosition(rb2->Position() - n * (penetration / 2));
	}
*/
	if (v > 0) {
		float3		va	= vv - n * v;
		if (penetration > v * dt)
			v = min(v * 2, penetration / dt);

		float		d	= rb1->Denominator(p1, n) + rb2->Denominator(p2, n);
		float		e	= rb1->Restitution() * rb2->Restitution();
		float		f	= (1 + e) * v / d;
		float3		i	= n * f;

		float		fr	= rb1->Friction() + rb2->Friction();
		if (fr && len2(va) != 0) {
			float3		na	= normalise(va);
			float		da	= rb1->Denominator(p1, na) + rb2->Denominator(p2, na);
			i += va * (fr / da);
		}

		rb1->AddImpulse( i, p1);
		rb2->AddImpulse(-i, p2);
		return f;
	}
	return 0;
}


//#define MAX_DT	(1/60.f)
#define MAX_DT	0.05f

int		total_iterations	= 0;
int		num_averages		= 0;
float	average_iterations	= 0;

#ifdef CHECKRB
bool check(RigidBody *rb) {
	float3	v = rb->Velocity();
	if (((uint32*)&v)[0] == 0x7fc00000) {
		for(;;);
		return false;
	}
	return true;
}
#endif

void DynamicsSystem::operator()(FrameEvent &ev) {
	float	dt = ev.dt;
	PROFILE_CPU_EVENT("Move RigidBody");
	for (auto i = rigid_bodies.begin(); i != rigid_bodies.end(); ++i) {
		if (i->obj) {
			i->UpdateVel(dt);
			i->obj->SetMatrix(i->CalcMatrix(dt));
		} else {
			delete i.remove();
		}
	}

	PROFILE_CPU_EVENT_NEXT("CollideAll");
	World::Current()->MoveDynamicObjects();
	CollisionSystem::CollideAll();

	PROFILE_CPU_EVENT_NEXT("CollisionResponse");
	Contact		c;
	while (CollisionSystem::PopContact(c)) {
		CollisionMessage	cm(c, dt);
		if (c.obj1 && !(c.p2->Masks() & CF_NO_PHYSICS))
			c.obj1->Send(cm);
		if (c.obj2 && !(c.p1->Masks() & CF_NO_PHYSICS)) {
			cm.c.Swap();
			c.obj1->Send(cm);	// Obj1 since it was swapped.
			cm.c.Swap();		// put it back to stay consistent
		}
		if (cm.physics) {
			CollisionResponse(
				GetRigidBody(c.obj1),
				GetRigidBody(c.obj2),
				c.position1,
				c.position2,
				c.normal,
				c.penetration, dt
			);
		}
	}

	PROFILE_CPU_EVENT_NEXT("Init Constraints");
	for (e_list<Constraint>::iterator i = constraints.begin(); i != constraints.end(); ++i)
		i->init(dt);

	PROFILE_CPU_EVENT_NEXT("Solve Constraints");
	for (int j = 0; j < 10; j++) {
		bool	solved = true;
		for (auto &i : constraints) {
#ifdef CHECKRB
			RigidBodyState	rbs1(i.GetRigidBody1());
			RigidBodyState	rbs2(i.GetRigidBody2());
#endif
			if (!i.solve(dt))
				solved = false;
#ifdef CHECKRB
			if (!check(i.GetRigidBody1()) || !check(i.GetRigidBody2())) {
				rbs1.Update(i.GetRigidBody1());
				rbs2.Update(i.GetRigidBody2());
				for(;;);
			}
#endif
		}
		total_iterations++;
		if (solved)
			break;
	}

	num_averages++;
	average_iterations = float(total_iterations) / num_averages;

	PROFILE_CPU_EVENT_NEXT("Apply Constraints to Positions");
	for (auto &i : rigid_bodies) {
//		i.UpdateVel(dt2);
		i.UpdatePos(dt);
		if (i.obj)
			i.obj->SetMatrix(i.GetMatrix());
	}

	PROFILE_CPU_EVENT_NEXT("Solve Velocities");
	for (auto &i : constraints)
		i.solveV();

	PROFILE_CPU_EVENT_NEXT("Clear Forces");
	for (auto &i : rigid_bodies)
		i.ResetForces(gravity);
}

//-----------------------------------------------------------------------------
//	Create
//-----------------------------------------------------------------------------

struct DynamicsMaker: Handles2<DynamicsMaker, WorldEvent> {
	void	operator()(WorldEvent *ev) {
		if (ev->state == WorldEvent::BEGIN)
			new DynamicsSystem(ev->world);
	}
} dynamics_maker;

void DynamicsSystem::Create(const CreateParams &cp, crc32 id, const PhysicsData *t) {
	if (MassPropertiesData *m = cp.obj->FindType<MassPropertiesData>()) {
		RigidBody	*rb = new RigidBody(m->GetMassProperties(t->density), cp.obj);
		rb->SetRestitution(t->restitution);
		rb->SetFriction(t->resistance);
		rigid_bodies.push_back(rb);
	}
}

void DynamicsSystem::Create(const CreateParams &cp, crc32 id, const PhysicsMassData *t) {
	RigidBody	*rb = new RigidBody(*t, cp.obj);
	rb->SetRestitution(t->restitution);
	rb->SetFriction(t->resistance);
	rigid_bodies.push_back(rb);
}

}	// namespace iso
