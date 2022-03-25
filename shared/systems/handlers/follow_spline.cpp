#include "splines.h"
#include "geometry.h"
#include "object.h"
#include "utilities.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	FollowSpline
//-----------------------------------------------------------------------------
class FollowSpline
	: public Handles2<FollowSpline, FrameEvent2> 
	, public CleanerUpper<FollowSpline>
	, public aligner<16>
{
	enum {
		F_NONE		= 0x0,
		F_INIT		= 0x1,
		F_LOOP		= 0x2,
		F_REVERSE	= 0x4,
	};

protected:
	ObjectReference obj;
	Spline *spline;
	crc32 spline_id;
	float speed;
	float bank_angle, bank_smooth;
	ISO_ptr<void> on_start;
	ISO_ptr<void> on_end;
	size_t flags;
	float t;

	AnimationSetHandler	animationset_handler;

	float3x4 GetMatrix(float t) const;

public:
	FollowSpline(Object *_obj, const ISO_browser &b);
	~FollowSpline();
	void operator()(FrameEvent2 *ev);
	void operator()(AnimationSetMessage &m) {
		speed = m.speed;
		if (m.value >= 0)
			t = m.value;
	}
};

FollowSpline::FollowSpline(Object *_obj, const ISO_browser &b)
	: obj(_obj)
	, animationset_handler(this)
	, spline(NULL)
	, flags(F_INIT)
{
	// init
	speed = b["speed"].GetFloat();
	bank_angle = degrees(b["bank_angle"].GetFloat());
	bank_smooth = b["bank_smooth"].GetFloat();
	if (b["loop"].GetInt())
		flags |= F_LOOP;
	on_start = *b["on_start"];
	on_end = *b["on_end"];

	// spline
	if (ent::Spline *p = *b["spline"])
		spline = new Spline(*p);
	else
		spline_id = CRC32(b["spline_id"].GetString());

	// detach
	obj->Detach();

	// events
	obj->AddEventHandler(animationset_handler);
}

FollowSpline::~FollowSpline()
{
	if (spline)
		delete spline;
}

float3x4 FollowSpline::GetMatrix(float t) const
{
	float3 along = spline->Tangent(t);
	return translate(spline->Evaluate(t).xyz) * look_along_y(flags & F_REVERSE ? -along : along);
}

void FollowSpline::operator()(FrameEvent2 *ev)
{
	// dispose
	if (!obj) {
		deferred_delete(this);
		return;
	}

	// init
	if (flags & F_INIT) {
		// lookup
		if (!spline) {
			ent::Spline *p = Splines::Find(spline_id);
			ISO_ASSERT(p);
			spline = new Spline(*p);
		}

		// setup
		spline->ClosestPoint(obj->GetPos(), &t);

		if (dot(spline->Tangent(t), obj->GetMatrix().y) < 0.0f)
			flags |= F_REVERSE;
		obj->SetMatrix(GetMatrix(t));

		// event
		if (on_start)
			obj->AddEntities(on_start);

		flags &= ~F_INIT;
	}

	// ignore
	if (!speed)
		return;

	// advance
	float _t = spline->AdvanceArcLength(t, speed * ev->dt, !!(flags & F_LOOP), false);

	// loop
	if (_t > spline->Count() || _t < 0.0f) {
		// events
		if (on_end)
			obj->AddEntities(on_end);
		// clamp, stop
		_t = clamp(t, 0.0f, float(spline->Count()));
		speed = 0.0f;
	} else if (_t < t && speed > 0.0f || _t > t && speed < 0.0f) {
		// events
		if (on_end)
			obj->AddEntities(on_end);
	}
	t = _t;

	// transform
	obj->SetMatrix(GetMatrix(t));

	// bank
	if (bank_angle != 0.0f) {
		float _t = t + (speed < 0.0f ? -bank_smooth : bank_smooth) * rlen(spline->Tangent(t));
		if (flags & F_LOOP)
			_t = wrap(_t, 0.0f, static_cast<float>(spline->Count()));
		else
			_t = clamp(_t, 0.0f, static_cast<float>(spline->Count()));

		// project
		float3 _along = spline->Tangent(_t);
		_along = normalise(_along - obj->GetMatrix().z * dot(obj->GetMatrix().z, _along));
		float strength = dot(obj->GetMatrix().z, cross(obj->GetMatrix().y, _along));
		obj->SetMatrix(obj->GetMatrix() * rotate_in_y((flags & F_REVERSE ? strength : -strength) * bank_angle));
	}
}

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("FollowSpline", 0xb5b78d01)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		new FollowSpline(cp.obj, ISO_browser(t));
	}
	static TypeHandlerCRC<ISO_CRC("FollowSpline", 0xb5b78d01)> thFollowSpline;
}