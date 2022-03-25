#include "object.h"
#include "utilities.h"
#include "crc_handler.h"
#include "profiler.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Animator
//-----------------------------------------------------------------------------
namespace ent {
	struct Animator {
		crc32 id;
		float speed;
		float speed_random;
		float time;
		float time_random;
		int16 loop;
		uint8 hierarchy;
		ISO_ptr<void> on_loop;
	};
}
enum {
	ANIMATOR_EVENT = ISO_CRC("ANIMATOR_EVENT", 0x1741fac1),
};

namespace iso {
	template<> struct EventMessage<ANIMATOR_EVENT> {
		const AnimationSetMessage &relay_msg;
		int16 loop;
		ISO_ptr<void> on_loop;
		EventMessage(const AnimationSetMessage &_relay_msg, int16 _loop, ISO_ptr<void> _on_loop)
			: relay_msg(_relay_msg)
			, loop(_loop)
			, on_loop(_on_loop)
		{}
	};
	typedef EventMessage<ANIMATOR_EVENT>	AnimatorMessage;
	typedef EventHandlerCRC<ANIMATOR_EVENT>	AnimatorHandler;
}

class Animator : public DeleteOnDestroy<Animator> {
	ObjectReference	obj;
	int				loop;
	ISO_ptr<void>	on_loop;

	AnimatorHandler animator_handler;
	AnimationLoopHandler animation_loop_handler;
public:
	Animator(Object *_obj)
		: DeleteOnDestroy<Animator>(_obj)
		, obj(_obj)
		, loop(0)
		, animator_handler(this, _obj)
		, animation_loop_handler(this, _obj)
	{}
	void operator()(AnimatorMessage &msg) {
		// state, post
		loop = msg.loop;
		on_loop = msg.on_loop;
		obj->Send(msg.relay_msg);
	}
	void operator()(AnimationLoopMessage &m) {
		if (loop > 0 && !--loop)
			obj->Send(AnimationSetMessage(0.0f));
		if (on_loop)
			obj->AddEntities(on_loop);
	}
};

namespace iso {
	ISO_DEFUSERCOMPX(ent::Animator, 8, "Animator") {
		ISO_SETFIELDS8(0, id, speed, speed_random, time, time_random, loop, hierarchy, on_loop);
	}};

	template<> void TypeHandler<ent::Animator>::Create(const CreateParams &cp, ISO_ptr<ent::Animator> t) {
		Object			*obj	= cp.obj;
		ent::Animator	&data	= *t;
		// loop, speed, time
		bool attach = data.loop > 0 || data.on_loop;
		float speed = data.speed_random != 0 ? data.speed + random.to(2.0f * data.speed_random) - data.speed_random : data.speed;
		float time = data.time_random != 0 ? data.time + random.to(2.0f * data.time_random) - data.time_random : data.time;

		// hierarchy/self
		AnimationSetMessage animation_set_msg(speed, time, data.id);
		AnimatorMessage animator_msg(animation_set_msg, data.loop, data.on_loop);
		if (data.hierarchy) {
			for (Object::iterator iter = obj; iter; ++iter) {
				// existing
				if (!iter->Send(animator_msg)) {
					if (!attach)
						iter->Send(animation_set_msg);
					else if (iter->FindEvent(ANIMATIONSET_EVENT))
						(*new Animator(iter))(animator_msg);
				}
			}

		} else if (!obj->Send(animator_msg)) {
			if (!attach)
				obj->Send(animation_set_msg);
			else if (obj->FindEvent(ANIMATIONSET_EVENT))
				(*new Animator(obj))(animator_msg);
		}
	}
	TypeHandler<ent::Animator> thAnimator;
}

//-----------------------------------------------------------------------------
//	OnLoop
//-----------------------------------------------------------------------------
namespace ent {
	struct OnLoop {
		crc32 id;
		ISO_ptr<void> on_begin;
		ISO_ptr<void> on_end;
	};
}

class OnLoop : public DeleteOnDestroy<OnLoop> {
	ObjectReference obj;
	const ent::OnLoop &data;
	bool match;
	int8 direction;
	AnimationSetHandler animation_set_handler;
	AnimationLoopHandler animation_loop_handler;

public:
	OnLoop(Object *_obj, const ent::OnLoop &_data)
		: DeleteOnDestroy<OnLoop>(_obj)
		, obj(_obj)
		, data(_data)
		, match(true)
		, direction(1)
		, animation_set_handler(this, _obj)
		, animation_loop_handler(this, _obj)
	{}

	void operator()(AnimationSetMessage &msg) {
		match = data.id.blank() || data.id == msg.id;
		direction = msg.speed > 0.0f ? 1 : msg.speed < 0.0f ? -1 : 0;
	}
	void operator()(AnimationLoopMessage &msg) {
		if (match && direction)
			obj->AddEntities(direction == 1 ? data.on_end : data.on_begin);
	}
};

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("OnLoop", 0x49e7bdfa)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		new OnLoop(cp.obj, *static_cast<ent::OnLoop*>(t));
	}
	extern "C" {
		TypeHandlerCRC<ISO_CRC("OnLoop", 0x49e7bdfa)> thOnLoop;
	}
}
