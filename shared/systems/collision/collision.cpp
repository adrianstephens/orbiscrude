#include "collision.h"
#include "maths/simplex.h"
#include "utilities.h"
#include "base/algorithm.h"
#include "thread.h"
#include "scenetree.h"
#include "jobs.h"
#include "fibers.h"
#ifdef PLAT_X360
#include "lockfree.h"
#endif
#include "profiler.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Collision
//-----------------------------------------------------------------------------
atomic<int32>	total_pairs1, total_pairs2;

class CollisionContainer : public SceneTree, public DeleteOnDestroy<CollisionContainer> {
public:
	using SceneTree::operator new;
	using SceneTree::operator delete;
	static CollisionContainer	*me;
	static CollisionContainer	*Get() { return me; }

	lf_array_queue<Contact,1024>		contacts;

	struct ColliderJob {
		CollisionContainer	*cc;
		CollisionItem0		*c0, *c1;
		void	operator()() {
			cc->CollidePair(c0, c1);
			++total_pairs2;
		}
		ColliderJob(CollisionContainer *_cc, CollisionItem0 *_c0, CollisionItem0 *_c1) : cc(_cc), c0(_c0), c1(_c1) {}
	};

	void	AddContact(Object *obj1, Object *obj2, CollisionItem0 *p1, CollisionItem0 *p2, float penetration, param(position3) position, param(float3) normal) {
		contacts.emplace_back(obj1, obj2, p1, p2, penetration, position, normal);
	}
	void	AddContact(Object *obj1, Object *obj2, CollisionItem0 *p1, CollisionItem0 *p2, float penetration, param(position3) position1, param(position3) position2, param(float3) normal) {
		contacts.emplace_back(obj1, obj2, p1, p2, penetration, position1, position2, normal);
	}

	void			CollidePair(CollisionItem0 *i, CollisionItem0 *j);
	void			CollideAll();

//	void	operator()(WorldEvent *ev) {
//		if (ev->state == WorldEvent::BEGIN)
//			tree.Init();
//	}

	int	pair_wait = 0;
	bool PopContact(Contact &c) {
		while (!contacts.pop_front(c)) {
			if (total_pairs1 == total_pairs2) {
				if (contacts.empty())
					return false;
			}
			pair_wait++;
		}
		return true;
	}

	void CollectFrustum(param(float4x4) frustum, dynamic_array<CollisionItem0*> &items) {
		// static
		static SceneTree::items visible;
		static SceneTree::items completely_visible;

		SceneTree::CollectFrustum(frustum, visible, completely_visible);
		for (auto &i : visible)
			items.push_back(static_cast<CollisionItem0*>(i));
		for (auto &i : completely_visible)
			items.push_back(static_cast<CollisionItem0*>(i));

		// dynamic
		for (auto &i : GetList()) {
			if (is_visible(i.box, frustum))
				items.push_back((CollisionItem0*)i.data);
		}
	}

	CollisionItem0*	Add(CollisionItem0 *c, param(cuboid) box) {
		c->container	= this;
		c->node			= Insert(box, c);
		return c;
	}

	template<typename T> CollisionItem<T> *Add(CollisionItem<T> *c) {
		c->container	= this;
		c->node			= Insert(c->CalcBox(c->obj->GetWorldMat()), c);
		return c;
	}

	void Create(const CreateParams &cp, crc32 id, const Collision_Ray *t)		{ Add(new CollisionItem<ray3>(cp.obj, ray3(position3(t->pos), vector3(t->dir)), t->mask)); }
	void Create(const CreateParams &cp, crc32 id, const Collision_Sphere *t)	{ Add(new CollisionItem<sphere>(cp.obj, sphere(position3(t->centre), t->radius), t->mask)); }
	void Create(const CreateParams &cp, crc32 id, const Collision_OBB *t)		{ Add(new CollisionItem<obb3>(cp.obj, obb3(t->obb).right(), t->mask)); }
	void Create(const CreateParams &cp, crc32 id, const Collision_Cylinder *t)	{ Add(new CollisionItem<cylinder>(cp.obj, cylinder(position3(t->centre), vector3(t->dir), t->radius), t->mask)); }
	void Create(const CreateParams &cp, crc32 id, const Collision_Cone *t)		{ Add(new CollisionItem<cone>(cp.obj, cone(position3(t->centre), vector3(t->dir), t->radius), t->mask)); }
	void Create(const CreateParams &cp, crc32 id, const Collision_Capsule *t)	{ Add(new CollisionItem<capsule>(cp.obj, capsule(position3(t->centre), vector3(t->dir), t->radius), t->mask)); }
	void Create(const CreateParams &cp, crc32 id, const Collision_Patch *t)		{
		bezier_patch	p;
		for (int i = 0; i < 16; i++)
			p.cp(i) = position3(t->p[i]);
		Add(new CollisionItem<bezier_patch>(cp.obj, p, t->mask));
	}

	CollisionContainer(World *w) : DeleteOnDestroy<CollisionContainer>(w) {
		me = this;
		w->AddHandler<CreateMessage>(Creation<Collision_Ray>(this));
		w->AddHandler<CreateMessage>(Creation<Collision_Sphere>(this));
		w->AddHandler<CreateMessage>(Creation<Collision_OBB>(this));
		w->AddHandler<CreateMessage>(Creation<Collision_Cylinder>(this));
		w->AddHandler<CreateMessage>(Creation<Collision_Cone>(this));
		w->AddHandler<CreateMessage>(Creation<Collision_Capsule>(this));
		w->AddHandler<CreateMessage>(Creation<Collision_Patch>(this));
	}
	~CollisionContainer() {
		me = 0;
	}
};

CollisionContainer	*CollisionContainer::me;

struct CollisionsMaker: Handles2<CollisionsMaker, WorldEvent> {
	void	operator()(WorldEvent *ev) {
		if (ev->state == WorldEvent::BEGIN)
			new CollisionContainer(ev->world);
	}
} collisions_maker;

cuboid	CollisionItem0::GetBox()	const			{ return node->box; }
void	CollisionItem0::_Update(param(cuboid) box)	{ container->Move(box, node); }
CollisionItem0::~CollisionItem0()					{ node->Destroy(); }

template<typename T> position3	canonical_support(param(float3) v);
template<>	position3	canonical_support<ray3>		(param(float3) v)	{ return position3(sign1(v.x), zero, zero); }
template<>	position3	canonical_support<sphere>	(param(float3) v)	{ return position3(normalise(v)); }
template<>	position3	canonical_support<obb3>		(param(float3) v)	{ return position3(sign1(v)); }
template<>	position3	canonical_support<capsule>	(param(float3) v)	{ return position3(normalise(v.xy), sign1(v.z)); }
template<>	position3	canonical_support<cylinder>	(param(float3) v)	{ return position3(normalise(v.xy), sign1(v.z)); }
template<>	position3	canonical_support<cone>		(param(float3) v)	{ return position3(select(v.z > len(v) * rsqrt(5.0f), float3(z_axis), normalise(concat(v.xy, -one)))); }

template<typename C1, typename C2> void coll_gjk(CollisionContainer *cc, CollisionItem0 *i, CollisionItem0 *j) {

	Object		*obj1	= i->Obj();
	Object		*obj2	= j->Obj();
	float3x4	matrix1	= obj1->GetWorldMat() * i->Prim<C1>().matrix();
	float3x4	matrix2	= obj2->GetWorldMat() * j->Prim<C2>().matrix();
	float3x4	matrix2to1	= matrix1 / matrix2;
	float3x4	matrix1to2	= inverse(matrix2to1);

	position3	closestA, closestB;
	float		dist;

	bool hit = simplex_difference<3>().gjk(
		[&](param(float3) v)	{ return matrix1to2 * canonical_support<C1>(matrix2to1 * v); },
		[](param(float3) v)		{ return canonical_support<C2>(v); },
		zero,	//_initial_dir,
		0,		//margins,
		0,		//separated,
		0,		//eps,
		closestA, closestB, dist
	);
	if (hit) {
		float3		normal = zero;//TBD
		cc->AddContact(obj1, obj2, i, j, dist, closestA, normal);
	}
}

void coll_ray_sphere(CollisionContainer *cc, CollisionItem0 *i, CollisionItem0 *j) {
	Object		*obj1	= i->Obj();
	Object		*obj2	= j->Obj();
	float3x4	matrix1	= obj1->GetWorldMat();
	float3x4	matrix2	= obj2->GetWorldMat();

	const ray3	&r		= i->Prim<ray3>();
	auto		rworld	= matrix1 * r;
	float		t;
	float3		normal;
	const sphere &c		= j->Prim<sphere>();
	float3x4	im		= inverse(matrix2);
	if (c.ray_check(im * rworld, t, &normal))
		cc->AddContact(obj1, obj2, i, j, len(r.d) * (one - t), rworld.pt1(), rworld.from_parametric(t), matrix2 * normal);
}

void coll_ray_obb(CollisionContainer *cc, CollisionItem0 *i, CollisionItem0 *j) {
	Object		*obj1	= i->Obj();
	Object		*obj2	= j->Obj();
	float3x4	matrix1	= obj1->GetWorldMat();
	float3x4	matrix2	= obj2->GetWorldMat();

	const ray3	&r		= i->Prim<ray3>();
	auto		rworld	= matrix1 * r;
	float		t;
	float3		normal;
	const obb3	&c		= j->Prim<obb3>();
	float3x4	m		= (matrix2 * c).matrix();
	float3x4	im		= inverse(m);
	if (unit_cube.ray_check(im * rworld, t, &normal))
		cc->AddContact(obj1, obj2, i, j, len(r.d) * (one - t), rworld.pt1(), rworld.from_parametric(t), normalise(m * normal));
}

void coll_ray_cylinder(CollisionContainer *cc, CollisionItem0 *i, CollisionItem0 *j) {
	Object		*obj1	= i->Obj();
	Object		*obj2	= j->Obj();
	float3x4	matrix1	= obj1->GetWorldMat();
	float3x4	matrix2	= obj2->GetWorldMat();

	const ray3	&r		= i->Prim<ray3>();
	auto		rworld	= matrix1 * r;
	float		t;
	float3		normal;
	const cylinder	&c		= j->Prim<cylinder>();
	float3x4	im		= inverse(matrix2);
	if (c.ray_check(im * rworld, t, &normal))
		cc->AddContact(obj1, obj2, i, j, len(r.d) * (one - t), rworld.pt1(), rworld.from_parametric(t), matrix2 * normal);
}

void coll_ray_patch(CollisionContainer *cc, CollisionItem0 *i, CollisionItem0 *j) {
	Object		*obj1	= i->Obj();
	Object		*obj2	= j->Obj();
	float3x4	matrix1	= obj1->GetWorldMat();
	float3x4	matrix2	= obj2->GetWorldMat();

	const ray3	&r		= i->Prim<ray3>();
	auto		rworld	= matrix1 * r;
	float		t;
	float3		normal;
	const bezier_patch &c		= j->Prim<bezier_patch>();
	float3x4	im		= inverse(matrix2);
	if (c.ray_check(im * rworld, t, &normal) && t > 0 && t < 1)
		cc->AddContact(obj1, obj2, i, j, len(r.d) * (one - t), rworld.pt1(), rworld.from_parametric(t), matrix2 * normal);
}

void coll_sphere_sphere(CollisionContainer *cc, CollisionItem0 *i, CollisionItem0 *j) {
	Object		*obj1	= i->Obj();
	Object		*obj2	= j->Obj();
	float3x4	matrix1	= obj1->GetWorldMat();
	float3x4	matrix2	= obj2->GetWorldMat();

	sphere		c1 = i->Prim<sphere>();
	sphere		c2 = j->Prim<sphere>();
	position3	p1 = matrix1 * c1.centre();
	position3	p2 = matrix2 * c2.centre();
	float		d2 = len2(p2 - p1);
	if (d2 < square(c1.radius() + c2.radius())) {
		float3	n	= normalise(p1 - p2);
		cc->AddContact(obj1, obj2, i, j, (c1.radius() + c2.radius()) - iso::sqrt(d2), p1 - n * c1.radius(), n);
	}
}

void coll_sphere_patch(CollisionContainer *cc, CollisionItem0 *i, CollisionItem0 *j) {
	Object		*obj1	= i->Obj();
	Object		*obj2	= j->Obj();
	float3x4	matrix1	= obj1->GetWorldMat();
	float3x4	matrix2	= obj2->GetWorldMat();

	const sphere c1		= i->Prim<sphere>();
	float3x4	im		= inverse(matrix2);
	const bezier_patch &c2	= j->Prim<bezier_patch>();
	sphere		sphere(im * (matrix1 * c1.centre()), c1.radius());
	float3		normal;
	float		t;
	if (c2.test(sphere, t, &normal)) {
		normal	= matrix2 * normal;
		cc->AddContact(obj1, obj2, i, j, t, matrix1 * c1.centre() - normal * c1.radius(), normal);
	}
}

void coll_obb_patch(CollisionContainer *cc, CollisionItem0 *i, CollisionItem0 *j) {
	Object		*obj1	= i->Obj();
	Object		*obj2	= j->Obj();
	float3x4	matrix1	= obj1->GetWorldMat();
	float3x4	matrix2	= obj2->GetWorldMat();

	const obb3	&c1		= i->Prim<obb3>();
	float3x4	im		= inverse(matrix2);
	const bezier_patch &c2	= j->Prim<bezier_patch>();
	float3		normal;
	position3	position;
	float		t;
	if (c2.test(im * matrix1 * c1, t, &position, &normal))
		cc->AddContact(obj1, obj2, i, j, t, matrix2 * position3(position - normal * t), matrix2 * normal);
}

typedef void (collision_function)(CollisionContainer*, CollisionItem0*, CollisionItem0*);
collision_function *collision_function_table[] = {
	//ray3 vs:
	0, coll_ray_sphere, coll_ray_obb, coll_ray_cylinder, 0, 0, coll_ray_patch, 0,
	//sphere vs:
	0, coll_sphere_sphere, coll_gjk<sphere,obb3>, coll_gjk<sphere,cylinder>, coll_gjk<sphere,cone>, coll_gjk<sphere,capsule>, coll_sphere_patch, 0,
	//obb3 vs:
	0, 0, coll_gjk<obb3,obb3>, coll_gjk<obb3,cylinder>, coll_gjk<obb3,cone>, coll_gjk<obb3,capsule>, coll_obb_patch, 0,
	//cylinder vs:
	0, 0, 0, coll_gjk<cylinder,cylinder>, coll_gjk<cylinder,cone>, coll_gjk<cylinder,capsule>, 0, 0,
	//cone vs:
	0, 0, 0, 0, coll_gjk<cone,cone>, coll_gjk<cone,capsule>, 0, 0,
	//capsule vs:
	0, 0, 0, 0, 0, coll_gjk<capsule,capsule>, 0, 0,
	//patch vs:
	0, 0, 0, 0, 0, 0, 0, 0,
};

void CollisionContainer::CollidePair(CollisionItem0 *i, CollisionItem0 *j) {
	if (collision_function *fn = collision_function_table[i->type * 8 + j->type])
		fn(this, i, j);
}

void CollisionContainer::CollideAll() {
	contacts.clear();

	PROFILE_CPU_EVENT("collect");
	static SceneTree::intersections intersect_list;
	intersect_list.clear();
	CollectTreeIntersect_Dynamics(*this, intersect_list);

	total_pairs1	= 0;
	total_pairs2	= 0;

	PROFILE_CPU_EVENT_NEXT("filter");
	for (auto &i : intersect_list) {
		CollisionItem0	*i0 = (CollisionItem0*)i.a;
		CollisionItem0	*i1 = (CollisionItem0*)i.b;
#if 0
		uint32 exclude;
		exclude = ((i0->masks >> _CF_BITS) & (i1->masks & _CF_MASK));
		if (i0->masks & CF_INCLUDE ? !exclude : exclude)
			continue;
		exclude = ((i1->masks >> _CF_BITS) & (i0->masks & _CF_MASK));
		if (i1->masks & CF_INCLUDE ? !exclude : exclude)
			continue;
#else
		if (CMASK_FAIL_CMP(i0->masks, i1->masks))
			continue;
#endif
		Object		*obj0	= i0->Obj();
		Object		*obj1	= i1->Obj();
		if (obj0 == obj1)
			continue;

		if (obj0->Root() == obj1->Root())
			continue;

		total_pairs1++;
		if (i0->type > i1->type)
			swap(i0, i1);
		if (auto *j = FiberJobs::get()) {
			j->add([this, i0, i1]() {
				CollidePair(i0, i1);
				++total_pairs2;
			});
		} else {
			ConcurrentJobs::Get().add([this, i0, i1]() {
				CollidePair(i0, i1);
				++total_pairs2;
			});
		}

	}

	PROFILE_CPU_EVENT_NEXT("pairs");

//	while (total_pairs2 < total_pairs1)
//		ConcurrentJobs::Get().dispatch();
}

// STUBS

void CollisionSystem::CollideAll() {
	CollisionContainer::Get()->CollideAll();
}

bool CollisionSystem::PopContact(Contact &c) {
	return CollisionContainer::Get()->PopContact(c);
}

void CollisionSystem::CollectFrustum(param(float4x4) frustum, dynamic_array<CollisionItem0*> &items) {
	CollisionContainer::Get()->CollectFrustum(frustum, items);
}

CollisionItem0*	CollisionSystem::Add(CollisionItem0 *c, param(cuboid) box)	{
	return CollisionContainer::Get()->Add(c, box);
}

template<typename S> struct TypeHandlerShapeCollision : TypeHandler<ShapeCollision<S>> {
	void	Create(const CreateParams &cp, crc32 id, const ShapeCollision<S> *p) {
		CollisionSystem::Add(cp.obj, *(S*)p, p->mask);
	}
	TypeHandlerShapeCollision() : TypeHandler<ShapeCollision<S>>(this) {}
};


TypeHandlerShapeCollision<cuboid>		th_cuboid;
TypeHandlerShapeCollision<ray3>			th_ray3;
TypeHandlerShapeCollision<sphere>		th_sphere;
TypeHandlerShapeCollision<obb3>			th_obb3;
TypeHandlerShapeCollision<cylinder>		th_cylinder;
TypeHandlerShapeCollision<cone>			th_cone;
TypeHandlerShapeCollision<capsule>		th_capsule;
TypeHandlerShapeCollision<bezier_patch>	th_bezier_patch;
