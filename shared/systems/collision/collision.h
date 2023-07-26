#ifndef COLLISION_H
#define COLLISION_H

#include "maths/geometry.h"
#include "maths/bezier.h"
#include "object.h"

class CollisionContainer;

enum {
	CF_GROUND			= 1 << 0,
	CF_USER				= 1 << 1,

	CF_INCLUDE			= 1 << 22,
	CF_NO_PHYSICS		= 1 << 23,

	_CF_BITS			= 8,
	_CF_MASK			= (1 << _CF_BITS) - 1,

	_CF_CUSTOM_BITS		= 6,
	_CF_CUSTOM_MASK		= (1 << _CF_CUSTOM_BITS) - 1,
	_CF_CUSTOM_OFFSET	= _CF_BITS * 2,
};

#define CMASK_DEFINE(x)		((x) & _CF_MASK)
#define CMASK_EXCLUDE(x)	(CMASK_DEFINE(x) << _CF_BITS)
#define CMASK_INCLUDE(x)	(CF_INCLUDE | CMASK_EXCLUDE(x))

#define CMASK_CUSTOM(x)		(((x) & _CF_CUSTOM_MASK) << _CF_CUSTOM_OFFSET)
#define CMASK_GET_CUSTOM(x)	(((x) >> _CF_CUSTOM_OFFSET) & _CF_CUSTOM_MASK)

#define CMASK_CMP(x, y)			(((x) >> _CF_BITS) & ((y) & _CF_MASK))
#define CMASK_FAIL_CMP(x, y)	(((x) & CF_INCLUDE ? !CMASK_CMP(x, y) : CMASK_CMP(x, y)) || ((y) & CF_INCLUDE ? !CMASK_CMP(y, x) : CMASK_CMP(y, x)))

namespace iso {

//-----------------------------------------------------------------------------
//	CollisionItem
//-----------------------------------------------------------------------------

typedef enum {COLL_RAY, COLL_SPHERE, COLL_OBB, COLL_CYLINDER, COLL_CONE, COLL_CAPSULE, COLL_PATCH} COLL_TYPE;
template<typename P> struct CollisionType {};
template<> struct CollisionType<ray3>			{ enum {type = COLL_RAY};		};
template<> struct CollisionType<sphere>			{ enum {type = COLL_SPHERE};	};
template<> struct CollisionType<obb3>			{ enum {type = COLL_OBB};		};
template<> struct CollisionType<cylinder>		{ enum {type = COLL_CYLINDER};	};
template<> struct CollisionType<cone>			{ enum {type = COLL_CONE};		};
template<> struct CollisionType<capsule>		{ enum {type = COLL_CAPSULE};	};
template<> struct CollisionType<bezier_patch>	{ enum {type = COLL_PATCH};		};

template<typename P> class CollisionItem;

class CollisionItem0 : public DeleteOnDestroy<CollisionItem0>, public aligner<16> {
	friend CollisionContainer;
protected:
	CollisionContainer	*container;
	struct SceneNode	*node;
	Object*			obj;
	COLL_TYPE		type:8;
	uint32			masks:24;
	void			_Update(param(cuboid) box);
public:
	COLL_TYPE		Type()		const				{ return type; }
	uint32			Masks()		const				{ return masks; }
	Object*			Obj()		const				{ return obj; }
	cuboid			GetBox()	const;
	void			SetMasks(uint32 _masks)			{ masks = _masks; }

/*	void			operator()(MoveMessage &m)		{ _Update(CalcBox()); }

	CollisionItem0(Object *_obj, COLL_TYPE _type, uint32 _masks)
		: DeleteOnDestroy<CollisionItem0>(_obj)
		, move_handler(this, _obj)
		,obj(_obj), type(_type), masks(_masks), node(0)
		{}
*/
	template<typename P> CollisionItem0(CollisionItem<P> *t, Object *_obj, uint32 _masks)
		: DeleteOnDestroy<CollisionItem0>(_obj)
		, node(0)
		, obj(_obj), type(COLL_TYPE(CollisionType<P>::type)), masks(_masks)
	{
		_obj->SetHandler<MoveMessage>(t);
	}

	template<typename P> const P& Prim() const;
	~CollisionItem0();
};


template<typename P> class CollisionItem : public CollisionItem0 {
public:
	P				p;
	inline cuboid	CalcBox(param(float3x4) matrix);
	inline cuboid	CalcBox()						{ return CalcBox(obj->GetWorldMat()); }
	void			operator()(MoveMessage &m)		{ _Update(CalcBox()); }
//	CollisionItem(Object *_obj, typename param(P) _p, uint32 _masks = 0) : CollisionItem0(_obj, COLL_TYPE(CollisionType<P>::type), _masks), p(_p) {}
	CollisionItem(Object *_obj, typename param(P) _p, uint32 _masks = 0) : CollisionItem0(this, _obj, _masks), p(_p) {}
	void			Update()	{ _Update(CalcBox(obj->GetWorldMat())); }
};
template<typename P> const P& CollisionItem0::Prim() const { return ((const CollisionItem<P>*)this)->p; }
template<typename P> inline cuboid CollisionItem<P>::CalcBox(param(float3x4) matrix)	{ return (matrix * p).get_box(); }
template<> inline cuboid CollisionItem<sphere>::CalcBox(param(float3x4) matrix)			{ return cuboid::with_centre(matrix * p.centre(), float3(p.radius())); }

//-----------------------------------------------------------------------------
//	Contact
//-----------------------------------------------------------------------------

class Contact {
public:
	Object			*obj1, *obj2;
	CollisionItem0	*p1, *p2;
	float			penetration;
	position3		position1, position2;
	float3			normal;
	Contact() {}
	Contact(Object *_obj1, Object *_obj2, CollisionItem0 *_p1, CollisionItem0 *_p2, float _penetration, param(position3) _position, param(float3) _normal)
		: obj1(_obj1), obj2(_obj2)
		, p1(_p1), p2(_p2)
		, penetration(_penetration)
		, position1(_position), position2(_position + _normal * _penetration)
		, normal(_normal)
	{}
	Contact(Object *_obj1, Object *_obj2, CollisionItem0 *_p1, CollisionItem0 *_p2, float _penetration, param(position3) _position1, param(position3) _position2, param(float3) _normal)
		: obj1(_obj1), obj2(_obj2)
		, p1(_p1), p2(_p2)
		, penetration(_penetration)
		, position1(_position1), position2(_position2)
		, normal(_normal)
	{}
	void	Swap()	{
		swap(obj1, obj2);
		swap(p1, p2);
		swap(position1, position2);
		normal = -normal;
	}
};

//-----------------------------------------------------------------------------
//	CollisionSystem
//-----------------------------------------------------------------------------

class CollisionSystem {
public:
	static CollisionItem0*	Add(CollisionItem0 *c, param(cuboid) box);
	template<typename P> static CollisionItem<P>* Add(CollisionItem<P> *c) {
		return (CollisionItem<P>*)Add(c, c->CalcBox());
	}
	template<typename P> static CollisionItem<P>* Add(Object *obj, const P &p, uint32 masks = 0) {
		return Add(new CollisionItem<P>(obj, p, masks));
	}
	static CollisionItem<obb3>* Add(Object *obj, const cuboid &p, uint32 masks = 0) {
		return Add(new CollisionItem<obb3>(obj, obb3(p), masks));
	}
	static void				CollideAll();
	static bool				PopContact(Contact &c);
	static void				CollectFrustum(param(float4x4) frustum, dynamic_array<CollisionItem0*> &items);
};

//-----------------------------------------------------------------------------
//	Events
//-----------------------------------------------------------------------------

struct CollisionMessage {
	Contact	&c;
	float	dt;
	bool	physics;
	class RigidBody *rb;
	CollisionMessage(Contact &_c, float _dt) : c(_c), dt(_dt), physics(true), rb(NULL)	{}
};

//-----------------------------------------------------------------------------
//	geometry
//-----------------------------------------------------------------------------

template<typename S> struct ShapeCollision : S {
	uint32		mask;
	ShapeCollision(const S &s, uint32 mask) : S(s), mask(mask) {}
};
template<typename S> ShapeCollision<S> make_shape_collision(const S &s, uint32 mask = 0) { return {s, mask}; }


} // namespace iso

#endif // COLLISION_H
