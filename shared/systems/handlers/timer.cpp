#include "object.h"
#include "extra/random.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Timer
//-----------------------------------------------------------------------------
enum {
	LIVE_ATTACH_CHILDREN,
	DIE_DETACH_CHILDREN,
	DIE_PARENT_ATTACH_CHILDREN
};

class ObjectTimer
	: public CleanerUpper<ObjectTimer>
	, public aligner<16>
{
	ObjectReference	obj;
	float3x4 matrix;
	float timeout;
	int die;
	int pulse;
	ISO_ptr<void> on_timeout;
	Timer*	handle;


public:
	ObjectTimer(const CreateParams &cp, const ent::Timer *t) : obj(cp.obj) {
		// init
		timeout = t->t;
		die = t->die;
		pulse = t->pulse - 1;
		on_timeout = t->children;
		matrix = obj->GetWorldMat();
		// timer
		handle = cp.world->AddTimer(this, timeout < 0 ? iso::random.to(-timeout) : timeout);
	}

	~ObjectTimer() {
		delete handle;
	}

	void operator()(TimerMessage &m) {
		handle = 0;
		if (!obj) {
			// dead owner, spawn at last known location
			Object temp(matrix);
			temp.AddEntities(on_timeout);
			while (!temp.children.empty())
				temp.children.front().Detach();

		} else if (die) {
			// kill owner, parent attach/detach children
			if (on_timeout) {
				Object temp(obj->GetWorldMat());
				temp.AddEntities(on_timeout);

				// attach to owner's parent or world
				Object *attach = die == DIE_PARENT_ATTACH_CHILDREN ? obj->Parent() : NULL;
				while (!temp.children.empty())
					Adopt(attach, temp.children.pop_front());
			}
			if (!pulse)
				iso::deleter<Object>()(obj);

		} else {
			// attach to owner
			obj->AddEntities(on_timeout);
		}

		// pulse
		if (obj && pulse) {
			// timer
			handle = m.world->AddTimer(this, timeout < 0 ? iso::random.to(-timeout) : timeout);
			// count
			if (pulse > 0)
				--pulse;
		} else {
			delete this;
		}
	}
};

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("Timer", 0xab11f11e)>::Create(const CreateParams &cp, crc32 id, const void *t)	{
		new ObjectTimer(cp, (const ent::Timer*)t);
	}
	extern "C" {
		TypeHandlerCRC<ISO_CRC("Timer", 0xab11f11e)> thTimer;
	}
}
