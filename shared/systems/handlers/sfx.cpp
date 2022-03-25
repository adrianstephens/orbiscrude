#include "object.h"
#include "sound.h"
#include "utilities.h"
#include "triggers.h"

using namespace iso;

// SfxQueue
const size_t sfx_queue_size_mask = 15;

struct SfxQueue	: Handles2<SfxQueue, FrameEvent2>, CleanerUpper<SfxQueue> {
	static SfxQueue *self;
	fixed_array<SoundVoice*, sfx_queue_size_mask + 1> queue;
	size_t current, end;

	SfxQueue()	: current(0), end(0) {}
	~SfxQueue() {
		while (current != end) {
			queue[current]->Stop();
			queue[current]->release();
			current = (current + 1) & sfx_queue_size_mask;
		}
		self = NULL;
	}

	void operator()(FrameEvent2*) {
		// release
		if (current != end && !queue[current]->IsPlaying()) {
			queue[current]->release();
			// advance
			for (;;) {
				current = (current + 1) & sfx_queue_size_mask;
				if (current == end)
					break;
				else if (queue[current]->IsPaused()) {
					queue[current]->Pause(false);
					break;
				} else
					queue[current]->release();
			}
		}
	}

	static void Queue(SoundVoice *voice);
};

SfxQueue *SfxQueue::self = NULL;

void SfxQueue::Queue(SoundVoice *voice) {
	if (!self)
		self = new SfxQueue;
	if (self->current != self->end)
		voice->Pause(true);
	// retain
	voice->addref();
	self->queue[self->end] = voice;
	// advance
	self->end = (self->end + 1) & sfx_queue_size_mask;
	ISO_ASSERT(self->current != self->end);
}


// SfxEvents
enum {
	SFX_EVENT = ISO_CRC("SFX_EVENT", 0xa51e808f),
};

namespace iso {
	template<> struct EventMessage<SFX_EVENT> {
		SoundVoice *voice;
		ISO_ptr<void> on_start;
		ISO_ptr<void> on_end;
		EventMessage(SoundVoice *_voice, ISO_ptr<void> &_on_start, ISO_ptr<void> &_on_end)
			: on_start(_on_start)
			, on_end(_on_end)
		{
			(voice = _voice)->addref();
		}
		EventMessage(const EventMessage &rhs)
			: on_start(rhs.on_start)
			, on_end(rhs.on_end)
		{
			(voice = rhs.voice)->addref();
		}
		~EventMessage() {
			voice->release();
		}
		EventMessage& operator=(const EventMessage& rhs) {
			if (voice)
				voice->release();
			(voice = rhs.voice)->addref();
			on_start = rhs.on_start;
			on_end = rhs.on_end;
			return *this;
		}
	};
	typedef EventMessage<SFX_EVENT>	SfxMessage;
	typedef EventHandlerCRC<SFX_EVENT> SfxHandler;
}

struct SfxEvents : DeleteOnDestroy<SfxEvents>, Handles2<SfxEvents, FrameEvent2> {
	typedef dynamic_array<SfxMessage> Events;
	ObjectReference obj;
	Events events;
	SfxHandler sfx_handler;
	SfxEvents(Object *_obj)
		: DeleteOnDestroy<SfxEvents>(_obj)
		, obj(_obj)
		, sfx_handler(this, _obj)
	{
		obj->AddEventHandler(sfx_handler);
		obj->AddEventHandler(destroy_handler);
	}
	~SfxEvents() {
		// release
		for (Events::iterator iter = events.begin(), end = events.end(); iter != end; ++iter)
			iter->voice->Stop();
		obj->RemoveEvent(SFX_EVENT);
	}
	void operator()(FrameEvent2 *) {
		// enum, indexed erase, events might mutate collection
		size_t i = 0;
		do {
			SfxMessage &msg = events[i];
			if (!msg.voice->IsPlaying()) {
				// end, erase
				obj->AddEntities(msg.on_end);
				events.erase_unordered(&events[i]);
			} else if (msg.on_start) {
				// start
				obj->AddEntities(msg.on_start);
				msg.on_start = ISO_NULL;
				// erase
				if (!msg.on_end)
					events.erase_unordered(&events[i]);
				else
					++i;
			} else
				++i;
		} while (i < events.size());
		// cleanup
		if (events.empty())
			deferred_delete(this);
	}

	void operator()(SfxMessage &m) {
		events.push_back(m);
	}
};

//-----------------------------------------------------------------------------
//	SFX
//-----------------------------------------------------------------------------
namespace ent {
struct SFX {
	enum {
		ATTACH_OBJECT_POSITION,
		ATTACH_OBJECT,
		ATTACH_TRIGGER_POSITION,
		ATTACH_NONE,
		ATTACH_ABSOLUTE
	};
	SampleBuffer sfx;
	float volume, pitch;
	float volume_random, pitch_random;
	uint8 type;
	uint8 attach;
	uint8 queue;
	uint8 falloff_type;
	ISO_ptr<void> on_start;
	ISO_ptr<void> on_end;
	float3x4p matrix;
};
}

namespace iso {
	ISO_DEFUSERCOMPX(ent::SFX, 12, "SFX") {
		ISO_SETFIELDS8(0, sfx, volume, pitch, volume_random, pitch_random, type, attach, queue);
		ISO_SETFIELDS4(8, falloff_type, on_start, on_end, matrix);
	}};

	template<> void TypeHandler<ent::SFX>::Create(const CreateParams &cp, ISO_ptr<ent::SFX> t) {
		ent::SFX	&data	= *t;
		Object		*obj	= cp.obj;
		// attach
		SoundVoice *voice;
		switch (data.attach) {
			case ent::SFX::ATTACH_OBJECT: {
				voice = PlayPositional(data.sfx, obj);
				break;
			}
			case ent::SFX::ATTACH_OBJECT_POSITION: {
				voice = PlayAt(data.sfx, obj->GetWorldMat() * position3(data.matrix.w));
				break;
			}
			case ent::SFX::ATTACH_TRIGGER_POSITION: {
				QueryTriggerMessage info_msg;
				obj->Send(info_msg);
				voice = PlayAt(data.sfx, info_msg.contact_mat * position3(data.matrix.w));
				break;
			}
			case ent::SFX::ATTACH_ABSOLUTE:
				voice = PlayAt(data.sfx, position3(data.matrix.w));
				break;
			case ent::SFX::ATTACH_NONE: {
				voice = Play(data.sfx, SoundType(data.type));
				break;
			}
			default:
				return;
		}

		// setup
		if (voice) {
			// volume
			if (data.volume_random != 0.0f)
				voice->SetVolume(data.volume + random.to(2.0f * data.volume_random) - data.volume_random);
			else
				voice->SetVolume(data.volume);
			// pitch
			if (data.pitch_random != 0.0f)
				voice->SetPitch(data.pitch + random.to(2.0f * data.pitch_random) - data.pitch_random);
			else
				voice->SetPitch(data.pitch);
			// falloff
//			voice->SetCutOffDistance(game_tuning.sound_cutoff_distances[clamp(data.falloff_type, 0, GameTuning::_SOUND_FALLOFF_TYPE_COUNT - 1)]);
		}

		// queue
		if (data.queue)
			SfxQueue::Queue(voice);

		// events
		if (data.on_start || data.on_end) {
			SfxMessage msg(voice, data.on_start, data.on_end);
			if (!obj->Send(msg))
				(*new SfxEvents(obj))(msg);
		}
	}
	TypeHandler<ent::SFX> thSfx;
}


class SoundVoiceFader : public Handles2<SoundVoiceFader, FrameEvent> {
	SoundVoice *voice;
	float rate;
	float time;
	float volume;
	float factor;
public:
	SoundVoiceFader(SoundVoice *_voice, float _rate, float _time) 
		: voice(_voice)
		, rate(_rate)
		, time(_time)
	{
		volume = voice->GetVolume();
		factor = rate < 0;
		voice->SetVolume(volume * factor);
		voice->addref();
	}
	~SoundVoiceFader() {
		voice->release();
	}
	void operator()(FrameEvent *ev) {
		if (time > ev->time)
			return;
		factor += rate * ev->dt;
		if (factor < 0.0f) {
			voice->Stop();
			delete this;
		} else if (factor > 1.0f) {
			voice->SetVolume(volume);
			delete this;
		} else
			voice->SetVolume(volume * factor);
	}
};
inline void FadeSound(SoundVoice *voice, float rate = -1.0f, float time = -1.0f) {
	new SoundVoiceFader(voice, rate, time);
}


