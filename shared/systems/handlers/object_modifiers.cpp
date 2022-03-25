#include "object_modifiers.h"
#include "utilities.h"
#include "render.h"
#include "sound.h"
#include "crc_handler.h"
#include "triggers.h"
#include "profiler.h"
#include "particle/particle.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Rotator
//-----------------------------------------------------------------------------
namespace ent {
	struct Rotator {
		float			speed;
		float3x4p		matrix;
	};
}

class Rotator
	: public Handles2<Rotator, FrameEvent2>
	, public CleanerUpper<Rotator>
	, public aligner<16>
{
	ObjectReference	obj;
	float3x4		origmat;
	quaternion		quat;
	position3		pos;
	float			angle;
	float			speed;
	float			orig_speed;

	AnimationSetHandler	animationset_handler;

public:
	Rotator(Object *_obj, param(float3x4) _mat, float _speed)
		: obj(_obj)
		, origmat(_obj->GetMatrix())
		, speed(_speed)
		, orig_speed(_speed)
		, angle(0)
		, quat(_mat)
		, pos(_mat.translation())
		, animationset_handler(this, _obj)
	{
	}

	void operator()(FrameEvent2 *ev) {
		if (!obj) {
			deferred_delete(this);
			return;
		}

		if (!speed)
			return;

		angle += speed * ev->dt;
		if (abs(angle) >= 2 * pi) {
			if (angle < 0.0f) {
				do {
					angle += 2 * pi;
				} while (angle < 0);
			} else {
				do {
					angle -= 2 * pi;
				} while (angle >= 2 * pi);
			}
		}
		obj->SetMatrix(origmat * translate(pos) * quat * rotate_in_z(angle) * ~quat * translate(-pos));
	}

	void operator()(AnimationSetMessage &m)	{
		speed = orig_speed * m.speed;
		if (m.value >= 0)
			angle = m.value * speed;
	}
};

namespace iso {
	ISO_DEFUSERCOMPX(ent::Rotator, 2, "Rotator") {
		ISO_SETFIELDS2(0, speed, matrix);
	}};
	template<> void TypeHandler<ent::Rotator>::Create(const CreateParams &cp, ISO_ptr<ent::Rotator> t)	{
		new Rotator(cp.obj, float3x4(t->matrix), t->speed);
	}
	static TypeHandler<ent::Rotator> thRotator;
}

//-----------------------------------------------------------------------------
//	LookAtRotator
//-----------------------------------------------------------------------------
namespace ent {
	struct LookAtRotator {
		crc32		lookat_id;
		float3x4p	matrix;
	};
}

class LookAtRotator
	: public Handles2<LookAtRotator, FrameEvent2>
	, public CleanerUpper<LookAtRotator>
	, public aligner<16>
{
	ObjectReference	obj;
	float3x4		origmat;
	quaternion		quat;
	position3		pos;
	crc32			lookat_id;
	ObjectReference	lookat_obj;

public:
	LookAtRotator(Object *_obj, param(float3x4) _mat, crc32 _lookat_id)
		: obj(_obj)
		, origmat(_obj->GetMatrix())
		, quat(_mat)
		, pos(_mat.translation())
		, lookat_id(_lookat_id)
	{}

	void operator()(FrameEvent2 *ev) {
		if (!lookat_obj) {
			for (Object::iterator iter = obj->Root(); iter; ++iter) {
				if (iter->GetName() == lookat_id) {
					lookat_obj = iter;
					break;
				}
			}
			if (!lookat_obj)
				return;
		}
		float3x4 lwmat	= obj->Parent() ? obj->Parent()->GetWorldMat() * origmat : origmat;
		float3x4 lmat2	= translate(pos) * quat;
		float3x4 tmat	= look_at(lmat2, lookat_obj->GetWorldPos() / lwmat);
		obj->SetMatrix(origmat * tmat * inverse(lmat2));
	}
};

namespace iso {
	ISO_DEFUSERCOMPX(ent::LookAtRotator, 2, "LookAtRotator") {
		ISO_SETFIELDS2(0, lookat_id, matrix);
	}};
	template<> void TypeHandler<ent::LookAtRotator>::Create(const CreateParams &cp, ISO_ptr<ent::LookAtRotator> t)	{
		new LookAtRotator(cp.obj, t->matrix, t->lookat_id);
	}
	static TypeHandler<ent::LookAtRotator> thLookAtRotator;
}

//-----------------------------------------------------------------------------
//	MotionMapScroller
//-----------------------------------------------------------------------------
namespace ent {
	struct MotionMapScroller {
		crc32 bone;
	};
}

class MotionMapScroller
	: public DeleteOnDestroy<MotionMapScroller>
	, public Handles2<MotionMapScroller, FrameEvent2, EVENT_PRIORITY_LOW>
	, public CleanerUpper<MotionMapScroller>
	, public aligner<16>
{
	ObjectReference	obj;
	uint8 bone_index;
	float2p distance;
	position3 last_pos;
	LookupHandler lookup_handler;

public:
	MotionMapScroller(Object *_obj, uint8 _bone_index = Pose::INVALID)
		: DeleteOnDestroy<MotionMapScroller>(_obj)
		, obj(_obj)
		, bone_index(_bone_index)
		, lookup_handler(this, _obj)
	{
		distance.set(0, 0);
		last_pos = (bone_index != Pose::INVALID ? obj->GetBoneWorldMat(bone_index) : obj->GetMatrix()).w;
	}
	void operator()(FrameEvent2 *ev) {
		// distance, adjust for object scale
		float3x4 tm = bone_index != Pose::INVALID ? obj->GetBoneWorldMat(bone_index) : obj->GetMatrix();

		vector3 dir = tm.w - last_pos;
		distance[0] += dot(tm.y, dir) * reciprocal(dot(tm.y, tm.y));
		distance[1] += dot(tm.x, dir) * reciprocal(dot(tm.x, tm.x));
		last_pos = tm.w;
	}
	void operator()(LookupMessage &m) {
		if (m.id == ISO_CRC("uv_scroll_mult", 0xed32b54f))
			m.set(&distance);
	}
};

namespace iso {
	template<> struct ISO_def<ent::MotionMapScroller> : public CISO_type_user_comp {
		CISO_element bone;
		ISO_def() : CISO_type_user_comp("MotionMapScroller", 1)
			, bone)
		{}
	};
	template<> void TypeHandler<ent::MotionMapScroller>::Create(const CreateParams &cp, ISO_ptr<ent::MotionMapScroller> t) {
		Pose *pose;
		if (t->bone && (pose = cp.obj->Property<Pose>()))
			new MotionMapScroller(cp.obj, pose->Find(t->bone));
		else
			new MotionMapScroller(cp.obj);
	}
	static TypeHandler<ent::MotionMapScroller> thMotionMapScroller;
}

//-----------------------------------------------------------------------------
//	MotionEmitterScaler
//-----------------------------------------------------------------------------
namespace ent {
	struct MotionEmitterScaler {
		float speed_range[2];
	};
}

class MotionEmitterScaler
	: public Handles2<MotionEmitterScaler, FrameEvent2>
	, public DeleteOnDestroy<MotionEmitterScaler>
	, public aligner<16>
{
	ObjectReference	obj;
	ParticleHolderReference ph;
	float2 speed_range2;
	float1 scaler;
	position3 last_pos;
	PostCreateHandler post_create_handler;

public:
	MotionEmitterScaler(Object *_obj, float _speed_range[2])
		: obj(_obj)
		, scaler(one)
		, post_create_handler(this, _obj)
		, DeleteOnDestroy<MotionEmitterScaler>(_obj)
	{
		// range
		speed_range2 = float2(square(_speed_range[0]), square(_speed_range[1]));
	}
	void operator()(FrameEvent2 *ev) {
		// validate
		ParticleSet *ps;
		if (ph && (ps = ph->GetParticleSet())) {
			// distance
			float3x4 tm = obj->GetMatrix();
			float1 speed2 = len2(tm.translation() - last_pos) / ev->dt;
			if (speed2 > speed_range2.y) {
				if (scaler < one)
					ps->ScaleEmitRate(scaler = one);
			} else if (speed2 <= speed_range2.x) {
				if (scaler > zero)
					ps->ScaleEmitRate(scaler = zero);
			} else {
				ps->ScaleEmitRate(scaler = (speed2 - speed_range2.x) / (speed_range2.y - speed_range2.x));
			}
			last_pos = tm.translation();
		} else
			deferred_delete(this);
	}
	void operator()(PostCreateMessage &msg) {
		// handler order indepent lookup
		if (ph = static_cast<ParticleHolder*>(QueryTypeMessage(ISO_CRC("TYPE_PARTICLE_HOLDER", 0xa8a21a2f)).send(obj)))
			last_pos = obj->GetPos();
		else
			delete this;
	}
};

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("MotionEmitterScaler", 0xaf608a63)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		ent::MotionEmitterScaler &data = *static_cast<ent::MotionEmitterScaler*>(t);
		new MotionEmitterScaler(cp.obj, data.speed_range);
	}
	static TypeHandlerCRC<ISO_CRC("MotionEmitterScaler", 0xaf608a63)> thMotionEmitterScaler;
}

//-----------------------------------------------------------------------------
//	OffsetFollower
//-----------------------------------------------------------------------------
namespace ent {
	struct OffsetFollower {
		crc32		follow_id;
	};
}

class OffsetFollower
	: public Handles2<OffsetFollower, FrameEvent2>
	, public CleanerUpper<OffsetFollower>
	, public aligner<16>
{
	ObjectReference		obj;
	crc32				follow_id;
	ObjectReference		follow_obj;
	vector3				follow_offset;	// My offset from the lookat_id's node (in local space)

public:
	OffsetFollower(Object *_obj, crc32 _lookat_id)
		: obj(_obj)
		, follow_id(_lookat_id)
		, follow_offset(zero)
	{
		if (_obj && _obj->Parent()) {
			for (Object::iterator iter = obj->Parent(); iter; ++iter) {
				if (iter->GetName() == follow_id) {
					follow_obj = iter;
					follow_offset = _obj->GetMatrix().translation() - iter->GetNode()->matrix.w;
					break;
				}
			}
		}
	}
	void operator()(FrameEvent2 *ev) {
		if (!obj) {
			deferred_delete(this);
			return;
		}

		// My lookat_obj wasn't there when I was created, so try once to see if its there now and try to create a new me
		// so the framecallback of the new one will be after the target.
		if (!follow_obj) {
			for (Object::iterator iter = obj->Parent(); iter; ++iter) {
				if (iter->GetName() == follow_id) {
					new OffsetFollower(obj, follow_id);
					break;
				}
			}
			deferred_delete(this);
			return;
		}
		obj->SetMatrix(translate(follow_obj->GetMatrix().translation() + follow_offset) * float3x3(obj->GetMatrix()));
	}
};

namespace iso {
	ISO_DEFUSERCOMPX(ent::OffsetFollower, 1, "OffsetFollower") {
		ISO_SETFIELDS1(0, follow_id);
	}};
	template<> void TypeHandler<ent::OffsetFollower>::Create(const CreateParams &cp, ISO_ptr<ent::OffsetFollower> t)	{
		new OffsetFollower(cp.obj, t->follow_id);
	}
	static TypeHandler<ent::OffsetFollower> thOffsetFollower;
}

//-----------------------------------------------------------------------------
//	BoneFollower
//-----------------------------------------------------------------------------
namespace ent {
	struct BoneFollower {
		crc32 bone;
	};
}

class BoneFollower
	: public Handles2<BoneFollower, FrameEvent2, EVENT_PRIORITY_LOW>
	, public aligner<16>
{
	Object				*obj, *skin_obj;
	Pose				*pose;
	uint8				index;
	QueryTypeHandler	query_type_handler;
	DestroyHandler		destroy_handler, destroy_handler_skin;

public:
	BoneFollower(Object *_obj, Object *_skin_obj, Pose *_pose, uint8 _index)
		: obj(_obj)
		, skin_obj(_skin_obj)
		, pose(_pose)
		, index(_index)
		, query_type_handler(this, _obj)
		, destroy_handler(this, _obj)
		, destroy_handler_skin(this, _skin_obj)
	{}
	void Delete() {
		delete this;
	}
	void operator()(FrameEvent2 *ev) {
		if (obj)
			obj->SetMatrix(obj->Parent() == skin_obj ? pose->GetObjectMatrix(index) : skin_obj->GetWorldMat() * pose->GetObjectMatrix(index));
	}
	void operator()(QueryTypeMessage &msg) {
		msg.set(ISO_CRC("TYPE_BONE_FOLLOWER", 0x47210836), this);
	}
	void operator()(DestroyMessage &msg) {
		delete this;
	}
};

void FollowBone(Object *obj, Object *skin_obj, crc32 bone) {
	// pose
	if (Pose *pose = skin_obj->Property<Pose>()) {
		// index
		uint8 index = pose->Find(bone);
		if (index != Pose::INVALID)
			new BoneFollower(obj, skin_obj, pose, index);
	}
}

void UnfollowBone(Object *obj) {
	if (BoneFollower *follower = static_cast<BoneFollower*>(QueryTypeMessage(ISO_CRC("TYPE_BONE_FOLLOWER", 0x47210836)).send(obj)))
		follower->Delete();
}

namespace iso {
	template<> struct ISO_def<ent::BoneFollower> : public CISO_type_user_comp {
		CISO_element	bone;
		ISO_def() : CISO_type_user_comp("BoneFollower", 1)
			, bone)
		{}
	};
	template<> void TypeHandler<ent::BoneFollower>::Create(const CreateParams &cp, ISO_ptr<ent::BoneFollower> t) {
		if (Object *parent = cp.obj->Parent())
			FollowBone(cp.obj, parent, t->bone);
	}
	static TypeHandler<ent::BoneFollower> thBoneFollower;
}

//-----------------------------------------------------------------------------
//	Switch
//-----------------------------------------------------------------------------
namespace ent {
	struct Switch {
		enum {
			MODE_RANDOM,
			MODE_SEQUENCE,
		};
		uint8 mode;
		ISO_ptr<void> children;
	};
}

// SwitchCounter
namespace iso {
	template<> struct EventMessage<ISO_CRC("SwitchCounter", 0xcace8cec)> {
		size_t	count;
	};
}

class SwitchCounter : public CleanerUpper<SwitchCounter> {
	EventHandlerCRC<ISO_CRC("SwitchCounter", 0xcace8cec)>	handler;
	size_t count;
public:
	SwitchCounter(Object *obj)
		: handler(this, obj)
		, count(0)
	{
	}
	void operator()(EventMessage<ISO_CRC("SwitchCounter", 0xcace8cec)> &m) { m.count = count++; }
};

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("Switch", 0x68454e2e)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		ent::Switch &data	= *static_cast<ent::Switch*>(t);
		Object		*obj	= cp.obj;
		size_t		index;
		if (data.mode == ent::Switch::MODE_SEQUENCE) {
			// counter
			EventMessage<ISO_CRC("SwitchCounter", 0xcace8cec)> msg;
			while (!obj->Send(msg))
				new SwitchCounter(obj);
			index = msg.count % static_cast<anything*>(data.children)->Count();

		} else
			index = random.to(static_cast<anything*>(data.children)->Count());

		// switch
		obj->AddEntities((*static_cast<anything*>(data.children))[index]);
	}
	TypeHandlerCRC<ISO_CRC("Switch", 0x68454e2e)> thSwitch;
}

//-----------------------------------------------------------------------------
// Fader
//-----------------------------------------------------------------------------
namespace ent {
	struct Fader {
		float	value;
		float	rate;
		bool8	hierarchy;
		Fader(float _value, float _rate, bool _hierarchy)
			: value(_value)
			, rate(_rate)
			, hierarchy(_hierarchy)
		{}
	};
}

namespace iso {
	template<> struct EventMessage<ISO_CRC("Fader", 0x7b54269c)> {
		const ent::Fader &data;
		EventMessage(const ent::Fader &_data) : data(_data) {}
	};
}

class Fader
	: public Handles2<Fader, FrameEvent2>
	, public CleanerUpper<Fader>
{
	ObjectReference	obj;
	float			value;
	float			rate;
	AttachHandler	attach_handler;
	EventListener<RENDER_EVENT, Fader> render_listener;
	EventHandlerCRC<ISO_CRC("Fader", 0x7b54269c)> fader_handler;

public:
	Fader(Object *_obj, bool hierarchy)
		: obj(_obj)
		, value(1)
		, rate(0)
		, attach_handler(this)
		, render_listener(this)
		, fader_handler(this, _obj)
	{
		if (hierarchy) {
			for (Object::iterator iter(obj); iter; ++iter)
				render_listener.Add(iter);
			obj->AddEventHandler(attach_handler);
		} else
			render_listener.Add(obj);
	}
	void operator()(FrameEvent2 *ev) {
		if (!obj)
			deferred_delete(this);

		if (rate != 0.0f) {
			value += rate * ev->dt;
			if (value < 0) {
				value = 0;
				rate = 0;
			} else if (value > 1) {
				value = 1;
				rate = 0;
			}
		}
	}
	void operator()(AttachMessage &msg) {
		if (msg.attach) {
			for (Object::iterator iter(msg.obj); iter; ++iter)
				render_listener.Add(iter);
		} else {
			for (Object::iterator iter(msg.obj); iter; ++iter)
				render_listener.Remove(iter);
		}
	}
	void operator()(RenderMessage &msg) {
		msg.params.opacity *= value;
	}
	void operator()(EventMessage<ISO_CRC("Fader", 0x7b54269c)> &msg) {
		// state
		if (!(msg.data.value < 0.0f))
			value = msg.data.value;
		rate = msg.data.rate;
	}
};

void FadeObject(Object *obj, const ent::Fader &data) {
	EventMessage<ISO_CRC("Fader", 0x7b54269c)> msg(data);
	while (!obj->Send(msg))
		new Fader(obj, data.hierarchy);
}

void FadeObject(Object *obj, float value, float rate, bool hierarchy) {
	if (obj)
		FadeObject(obj, ent::Fader(value, rate, hierarchy));
}

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("Fader", 0x7b54269c)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		FadeObject(cp.obj, *(ent::Fader*)t);
	}
	TypeHandlerCRC<ISO_CRC("Fader", 0x7b54269c)> thFader;
}

//-----------------------------------------------------------------------------
// Detacher
//-----------------------------------------------------------------------------
namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("Detacher", 0xec4c2c91)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		cp.obj->Detach();
	}
	TypeHandlerCRC<ISO_CRC("Detacher", 0xec4c2c91)> thDetacher;
}

//-----------------------------------------------------------------------------
// RandomDisplacer
//-----------------------------------------------------------------------------
namespace ent {
	struct RandomDisplacer {
		float3p translation;
		float3p rotation;
		float3p scale;
		uint8 uniform_scale;
		ISO_ptr<void> children;
	};
}

static float3 Displacement(param(float3) v)	{ return float3(iso::random.to(v.x * 2.0f), iso::random.to(v.y * 2.0f), iso::random.to(v.z * 2.0f)) - v; }
static float3 UniformDisplacement(param(float3) v)	{ return iso::random.to(v * 2.0f) - v; }

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("RandomDisplacer", 0x715a8af1)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		ent::RandomDisplacer *data = t;
		float3x4 tm =
			translate(Displacement(float3(data->translation))) *
			euler_angles(Displacement(degrees(float3(data->rotation)))) *
			scale((data->uniform_scale ? UniformDisplacement(float3(data->scale)) : Displacement(float3(data->scale))) + one);
		if (data->children)
			_TypeHandler::FindAndCreate(CreateParams(cp.obj, tm), data->children);
		else
			cp.obj->SetMatrix(cp.obj->GetMatrix() * tm);
	}
	TypeHandlerCRC<ISO_CRC("RandomDisplacer", 0x715a8af1)> thRandomDisplacer;
}