#include "triggers.h"
#include "collision.h"
#include "utilities.h"
#include "profiler.h"
#include "groups.h"

using namespace iso;

Object* GetTriggerContact(Object *obj, float3x4 *contact_mat, unsigned *contact_mask)
{
	// query
	QueryTriggerMessage info_msg;
	if (!(obj->Send(info_msg) && info_msg.contact_obj))
		return NULL;

	// contact
	if (contact_mat)
		*contact_mat = info_msg.contact_mat;
	if (contact_mask)
		*contact_mask = info_msg.contact_mask;

	return info_msg.contact_obj;
}

//-----------------------------------------------------------------------------
//	Trigger
//-----------------------------------------------------------------------------
namespace ent {
	struct Trigger {
		ISO_ptr<void> on_trigger;
		ISO_ptr<void> on_enter;
		ISO_ptr<void> on_exit;
		float interval;
		uint8 along_only;
	};
}

class Trigger
	: public Handles2<Trigger, FrameEvent2>
	, public CleanerUpper<Trigger>
	, public aligner<16>
{
	enum {
		F_NONE			= 0x0,
		F_DISABLED		= 0x1,
	};
	// Entry
	struct Entry {
		ObjectReference contact_obj;
		float3x4 contact_mat;
		Entry()
		{}
		Entry(const ObjectReference &_contact_obj, const float3x4 &_contact_mat)
			: contact_obj(_contact_obj)
			, contact_mat(_contact_mat)
		{}
		Entry& operator=(const Entry &rhs) {
			contact_obj = rhs.contact_obj;
			contact_mat = rhs.contact_mat;
			return *this;
		}
		bool operator==(const Entry &rhs) const {
			return contact_obj == rhs.contact_obj;
		}
		bool operator==(const Object *obj) const {
			return contact_obj == obj;
		}
		operator bool() const {
			return !!contact_obj;
		}
	};
	typedef array<Entry, 16> Entries;

	ObjectReference obj;
	ent::Trigger &data;
	uint8 flags;
	float next_time;
	Entries contact_buffer[2];
	Entries *contact, *contact_queue;
	ObjectReference contact_obj;
	float3x4 contact_mat;

public:
	CollisionHandler collision_handler;
	QueryTypeHandler query_type_handler;
	QueryTriggerHandler query_trigger_handler;
	DisableTriggerHandler disable_trigger_handler;

	Trigger(Object *_obj, ISO_ptr<void> t)
		: obj(_obj)
		, data(*static_cast<ent::Trigger*>(t))
		, flags(F_NONE)
		, next_time(0)
		, contact(&contact_buffer[0])
		, contact_queue(&contact_buffer[1])
		, collision_handler(this)
		, query_type_handler(this)
		, query_trigger_handler(this)
		, disable_trigger_handler(this)
	{
		// events
		obj->AddEventHandler(collision_handler);
		obj->AddEventHandler(query_type_handler);
		obj->AddEventHandler(query_trigger_handler);
		obj->AddEventHandler(disable_trigger_handler);
	}

	void operator()(FrameEvent2 *ev);
	void operator()(CollisionMessage &msg);

	void operator()(QueryTypeMessage &msg) {
		msg.set(ISO_CRC("TYPE_TRIGGER", 0x5e0e4bee), (Object*)obj);
	}

	void operator()(QueryTriggerMessage &msg) {
		if (msg.contact_obj = contact_obj)
			msg.contact_mat = contact_mat;
	}

	void operator()(DisableTriggerMessage &msg) {
		if (msg.disable != !!(flags & F_DISABLED)) {
			if (msg.disable) {
				collision_handler.unlink();
				flags |= F_DISABLED;
			} else {
				obj->AddEventHandler(collision_handler);
				flags &= ~F_DISABLED;
			}
		}
	}
};

void Trigger::operator()(FrameEvent2 *ev) {
	if (!obj) {
		deferred_delete(this);
		return;
	}

	// early out
	if (contact->empty() && contact_queue->empty())
		return;

	// enter
	if (data.on_enter && !contact_queue->empty()) {
		// additions
		for (Entries::iterator iter = contact_queue->begin(), end = contact_queue->end(); iter != end; ++iter) {
			if (*iter && find(contact->begin(), contact->end(), *iter) == contact->end()) {
				contact_obj = iter->contact_obj;
				contact_mat = iter->contact_mat;
				obj->AddEntities(data.on_enter);
			}
		}
	}

	// exit
	if (data.on_exit && !contact->empty()) {
		// dropouts
		for (Entries::iterator iter = contact->begin(), end = contact->end(); iter != end; ++iter) {
			if (*iter && find(contact_queue->begin(), contact_queue->end(), *iter) == contact_queue->end()) {
				contact_obj = iter->contact_obj;
				contact_mat = iter->contact_mat;
				obj->AddEntities(data.on_exit);
			}
		}
	}

	// swap, reset
	swap(contact, contact_queue);
	contact_queue->clear();

	// trigger
	if (!contact->empty() && !(ev->time < next_time)) {
		// events
		for (Entries::iterator iter = contact->begin(), end = contact->end(); iter != end; ++iter) {
			if (*iter) {
				contact_obj = iter->contact_obj;
				contact_mat = iter->contact_mat;
				obj->AddEntities(data.on_trigger);
			}
		}
		// interval
		if (!(data.interval < 0.0f))
			next_time = ev->time + data.interval;
	}
}

void Trigger::operator()(CollisionMessage &m)
{
	m.physics = false;

	// early out
	if (!(data.on_enter || data.on_exit) && GetGlobalTime() < next_time)
		return;

	// along only
	if (data.along_only) {
		float3 dir = float3(m.c.obj2->GetWorldMat().y);
		if (dot(dir, obj->GetWorldMat().y) < 0.0f)
			return;
	}

	// contact
	if (contact_queue->size() < contact_queue->capacity()) {
		if (find(contact_queue->begin(), contact_queue->end(), m.c.obj2) == contact_queue->end()) {
			Entry entry(m.c.obj2, translate(m.c.position2) * look_along_y(m.c.normal));
			contact_queue->push_back(entry);
		}
	} else {
		ISO_TRACE("ERR[Trigger]: too many contacts, ignoring.\n");
	}
}

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("Trigger", 0xd5d636c1)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		new Trigger(cp.obj, t);
	}
	TypeHandlerCRC<ISO_CRC("Trigger", 0xd5d636c1)> thTrigger;
}

//-----------------------------------------------------------------------------
//	OnTrigger
//-----------------------------------------------------------------------------
namespace ent {
	struct OnTrigger {
		crc32 dispatch_id;
		crc32 enable_dispatch_id;
		crc32 disable_dispatch_id;
		ISO_ptr<void> on_trigger;
	};
}

class OnTrigger : public CleanerUpper<OnTrigger>  {
protected:
	ObjectReference obj, trigger_obj;
	ent::OnTrigger *data;
	bool disabled;

	TriggerHandler trigger_handler;
	QueryTriggerHandler query_trigger_handler;

public:
	OnTrigger(Object *_obj, ent::OnTrigger *_data)
		: obj(_obj)
		, data(_data)
		, disabled(false)
		, trigger_handler(this)
		, query_trigger_handler(this)
	{
		// events
		obj->AddEventHandler(trigger_handler);
		obj->AddEventHandler(query_trigger_handler);
	}

	void operator()(TriggerMessage &m) {
		if (!obj) {
			deferred_delete(this);
			return;
		}
		// pre-enable
		if (disabled && m.dispatch_id == data->enable_dispatch_id)
			disabled = false;
		// events
		if (!disabled) {
			if (m.dispatch_id == data->dispatch_id) {
				// prevent self query
				if (m.obj != obj) {
					trigger_obj = m.obj;
					obj->AddEntities(data->on_trigger);
					trigger_obj = NULL;
				} else
					obj->AddEntities(data->on_trigger);
			}
			// post-disable
			if (!data->disable_dispatch_id.blank() && m.dispatch_id == data->disable_dispatch_id)
				disabled = true;
		}
	}

	void operator()(QueryTriggerMessage &msg) {
		if (trigger_obj)
			trigger_obj->Send(msg);
	}
};

namespace iso {
	ISO_DEFUSERCOMPX(ent::OnTrigger, 4, "OnTrigger") {
		ISO_SETFIELDS4(0, dispatch_id, enable_dispatch_id, disable_dispatch_id, on_trigger);
	}};
	template<> void TypeHandler<ent::OnTrigger>::Create(const CreateParams &cp, ISO_ptr<ent::OnTrigger> t) {
		new OnTrigger(cp.obj, t);
	}
	static TypeHandler<ent::OnTrigger> thOnTrigger;
}

//-----------------------------------------------------------------------------
//	SendTrigger, ForwardTrigger
//-----------------------------------------------------------------------------
struct TriggerRelay {
	// Iterator
	struct Iterator {
		Object *obj, *child;
		Object::iterator iter;

		Iterator(Object *_obj)
			: obj(_obj)
			, child(_obj)
			, iter(_obj)
		{
			ISO_ASSERT(obj);
		}
		operator Object*() const {
			return iter;
		}
		Object* operator->() const {
			return iter;
		}
		Iterator& operator++() {
			if (++iter == child)
				iter.skip();
			if (!iter) {
				if (obj = (child = obj)->Parent())
					iter = obj;
			}
			return *this;
		}
	};

	enum {
		MODE_ALL,
		MODE_RANDOM,
		MODE_SEQUENCE,
		MODE_SWEEP,

		INDEX_ALL = ~0,
	};
	uint8 mode;
	crc32 dispatch_id;
	crc32 value;
	crc32 group_id;
	ISO_openarray<crc32> node_ids;

	size_t GetCount() const;
	bool HandleKeyword(Object *obj, crc32 keyword_id, const TriggerMessage &msg);
	void Send(Object *obj, size_t index, const TriggerMessage &msg);
};

namespace iso {
	ISO_DEFUSERCOMP5(TriggerRelay, mode, dispatch_id, value, group_id, node_ids);
}

size_t TriggerRelay::GetCount() const
{
	if (size_t count = node_ids.Count())
		return count;
	return groups.Count(group_id);
}

bool TriggerRelay::HandleKeyword(Object *obj, crc32 keyword_id, const TriggerMessage &msg)
{
	switch (keyword_id.as<unsigned>()) {
		case ISO_CRC("_self", 0x266e4978): {
			obj->Send(msg);
			return true;
		}
		case ISO_CRC("_parent", 0xc4bd87cc): {
			if (Object *parent = obj->Parent())
				parent->Send(msg);
			return true;
		}
		case ISO_CRC("_child", 0xa88bb8b5): {
			if (Object *child = obj->Child())
				child->Send(msg);
			return true;
		}
		case ISO_CRC("_trigger", 0x6e23ee20): {
			if (Object *contact_obj = GetTriggerContact(obj))
				contact_obj->Send(msg);
			return true;
		}
		case 0:
			return true;
	}
	return false;
}

void TriggerRelay::Send(Object *obj, size_t index, const TriggerMessage &msg)
{
	if (size_t count = node_ids.Count()) {
		if (index == INDEX_ALL) {
			// all
			for (size_t i = 0; i < count; ++i) {
				if (!HandleKeyword(obj, node_ids[i], msg)) {
					for (Iterator iter = obj; iter; ++iter) {
						if (iter->GetName() == node_ids[i]) {
							iter->Send(msg);
							break;
						}
					}
				}
			}

		} else {
			// lookup
			ISO_ASSERT(index < count);
			if (!HandleKeyword(obj, node_ids[index], msg)) {
				for (Iterator iter = obj; iter; ++iter) {
					if (iter->GetName() == node_ids[index]) {
						iter->Send(msg);
						break;
					}
				}
			}
		}

	} else if (!group_id.blank()) {
		// group
		if (index != INDEX_ALL) {
			if (Object *obj = groups.At(group_id, index))
				obj->Send(msg);
		} else
			groups.Enum(group_id, Groups::MessageFn(msg));

	} else
		// self
		obj->Send(msg);
}

// SendTrigger
namespace ent {
	struct SendTrigger : TriggerRelay {};
}

// SendTriggerCounter
namespace iso {
	template<> struct EventMessage<ISO_CRC("SendTriggerCounter", 0x3f2bb987)> {
		size_t	count;
	};
}

class SendTriggerCounter
	: public CleanerUpper<SendTriggerCounter>
{
	EventHandlerCRC<ISO_CRC("SendTriggerCounter", 0x3f2bb987)>	handler;
	size_t count;
public:
	SendTriggerCounter(Object *obj)
		: handler(this)
		, count(0)
	{
		obj->AddEventHandler(handler);
	}
	void operator()(EventMessage<ISO_CRC("SendTriggerCounter", 0x3f2bb987)> &m) { m.count = count++; }
};

namespace iso {
	ISO_DEFUSER2(ent::SendTrigger, TriggerRelay, SendTrigger);

	template<> void TypeHandler<ent::SendTrigger>::Create(const CreateParams &cp, ISO_ptr<ent::SendTrigger> t) {
		Object		*obj	= cp.obj;
		// index
		size_t index;
		if (t->mode == TriggerRelay::MODE_SEQUENCE) {
			// counter
			EventMessage<ISO_CRC("SendTriggerCounter", 0x3f2bb987)> msg;
			while (!obj->Send(msg))
				new SendTriggerCounter(obj);
			index = msg.count % t->GetCount();

		} else if (t->mode == TriggerRelay::MODE_RANDOM)
			index = random.to(t->GetCount());
		else if (t->mode == TriggerRelay::MODE_ALL)
			index = TriggerRelay::INDEX_ALL;

		// post
		t->Send(obj, index, TriggerMessage(obj, t->dispatch_id, t->value));
	}
	static TypeHandler<ent::SendTrigger> thSendTrigger;
}

// ForwardTrigger
namespace ent {
	struct ForwardTrigger : TriggerRelay {};
}

class ForwardTrigger : public CleanerUpper<ForwardTrigger> {
protected:
	ObjectReference obj;
	ent::ForwardTrigger *data;
	size_t counter;

	TriggerHandler trigger_handler;

public:
	ForwardTrigger(ent::ForwardTrigger *_data, Object *_obj)
		: obj(_obj)
		, data(_data)
		, counter(0)
		, trigger_handler(this)
	{
		obj->AddEventHandler(trigger_handler);
	}

	void operator()(TriggerMessage &msg) {
		// dispose
		if (!obj) {
			deferred_delete(this);
			return;
		}

		// filter
		if (!data->dispatch_id.blank() && msg.dispatch_id != data->dispatch_id)
			return;

		// index
		size_t index;
		if (data->mode == TriggerRelay::MODE_SEQUENCE)
			index = counter++ % data->GetCount();
		else if (data->mode == TriggerRelay::MODE_RANDOM)
			index = iso::random.to(data->GetCount());
		else if (data->mode == TriggerRelay::MODE_ALL)
			index = TriggerRelay::INDEX_ALL;

		// post
		data->Send(obj, index, msg);
	}
};

namespace iso {
	ISO_DEFUSER2(ent::ForwardTrigger, TriggerRelay, ForwardTrigger);

	template<> void TypeHandler<ent::ForwardTrigger>::Create(const CreateParams &cp, ISO_ptr<ent::ForwardTrigger> t) {
		new ForwardTrigger(t, cp.obj);
	}
	static TypeHandler<ent::ForwardTrigger> thForwardTrigger;
}


//-----------------------------------------------------------------------------
//	TriggerEcho
//-----------------------------------------------------------------------------
class TriggerEcho : public DeleteOnDestroy<TriggerEcho> {
protected:
	Object *contact_obj;
	float3x4 contact_mat;
	QueryTriggerHandler query_trigger_handler;

public:
	TriggerEcho(Object *_obj, const QueryTriggerMessage &_info_msg)
		: DeleteOnDestroy<TriggerEcho>(_obj)
		, contact_obj(_info_msg.contact_obj)
		, contact_mat(_info_msg.contact_mat)
		, query_trigger_handler(this, _obj)
	{}
	void operator()(QueryTriggerMessage &msg) {
		msg.contact_obj = contact_obj;
		msg.contact_mat = contact_mat;
	}
};

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("TriggerEcho", 0x78facb8f)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		if (Object *parent_obj = cp.obj->Parent()) {
			QueryTriggerMessage info_msg;
			if (parent_obj->Send(info_msg) && info_msg.contact_obj)
				new TriggerEcho(cp.obj, info_msg);
		}
	}
	TypeHandlerCRC<ISO_CRC("TriggerEcho", 0x78facb8f)> thTriggerEcho;
}

//-----------------------------------------------------------------------------
//	GroupTrigger
//-----------------------------------------------------------------------------
namespace ent {
struct GroupTrigger {
	crc32 group_id;
	ISO_ptr<void> on_empty;
};
}

class GroupTrigger
	: public Handles2<GroupTrigger, FrameEvent>
	, public CleanerUpper<GroupTrigger>
{
	ObjectReference obj;
	ent::GroupTrigger *data;

public:
	GroupTrigger(Object *_obj, ent::GroupTrigger *_data)
		: obj(_obj)
		, data(_data)
	{}
	void operator()(FrameEvent *ev) {
		PROFILE_CPU_EVENT("GroupTrigger");
		// compact count
		if (!obj)
			deferred_delete(this);
		else if (!groups.Enum(data->group_id, Groups::CountFn())) {
			obj->AddEntities(data->on_empty);
			deferred_delete(this);
		}
	}
};

namespace iso {
	ISO_DEFUSERCOMPX(ent::GroupTrigger, 2, "GroupTrigger") {
		ISO_SETFIELDS2(0, group_id, on_empty);
	}};

	template<> void TypeHandler<ent::GroupTrigger>::Create(const CreateParams &cp, ISO_ptr<ent::GroupTrigger> t) {
		new GroupTrigger(cp.obj, t);
	}
	static TypeHandler<ent::GroupTrigger> thGroupTrigger;
}

//-----------------------------------------------------------------------------
//	DisableTrigger
//-----------------------------------------------------------------------------
namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("DisableTrigger", 0x7db22afa)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		DisableTriggerMessage msg(!!ISO_browser(t).GetMember(ISO_CRC("disable", 0x4f454d51)).GetInt());
		cp.obj->Send(msg);
	}
	TypeHandlerCRC<ISO_CRC("DisableTrigger", 0x7db22afa)> thDisableTrigger;
}
