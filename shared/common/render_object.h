#ifndef RENDER_OBJECT_H
#define RENDER_OBJECT_H

#include "render.h"
#include "scenetree.h"
#include "object.h"
#include "profiler.h"

namespace iso {
	
struct RenderParameters {
	float		opacity;
	uint32		flags;
	RenderParameters() : opacity(1), flags(0)	{}
};

struct RenderCollector : RenderParameters, MaskTester {
	float3x4	view;
	float4x4	viewProj;
	RenderEvent	*re;
	float		time;
	float		quality;
	bool		completely_visible;

	float		Time()								const	{ return time; }
	float		Distance(param(position3) wpos)		const	{ return (view * wpos).v.z; }

	void		SendTo(const SceneTree::items &array);
	void		SendTo(const SceneTree::List &list);

	RenderCollector(RenderEvent *_re);
};

// RenderItem
class RenderItem : public virtfunc<void(RenderCollector &rc)>, public aligner<16> {
public:
	SceneNode			*node;

	template<typename T> RenderItem(T *t) : virtfunc<void(RenderCollector &rc)>(t), node(0) {}

	~RenderItem() {
		if (node) {
			if (!node->InTree())
				World::Current()->Send(MoveMessage());
			node->Destroy();
		}
	}
};

// RenderObject
class RenderObject : public RenderItem, public MesssageDelegate<void, DestroyMessage> {
public:
	ObjectReference		obj;

	force_inline bool Move(const cuboid &world_box) {
		return node && node->Move(world_box);
	}

	template<typename T> RenderObject(T *t, Object *obj) : RenderItem(t), MesssageDelegate<void, DestroyMessage>(t, obj), obj(obj) {
		obj->SetHandler<MoveMessage>(t);
	}

	~RenderObject() {
		MesssageDelegate<void, DestroyMessage>::Remove(obj);
	}
};

// RenderMessage
struct RenderMessage {
	RenderParameters &params;
	float		time;
	float		Time()	const { return time; }
	RenderMessage(RenderParameters &_params, float _time) : params(_params), time(_time) {}
	RenderMessage&	me()	{ return *this;	}
};

struct RenderAddObjectMessage {
	RenderItem		*ro;
	const cuboid	box;
	RenderAddObjectMessage(RenderItem *_ro, param(cuboid) _box) : ro(_ro), box(_box) {}
	RenderAddObjectMessage(RenderItem *_ro, param(cuboid) _box, param(float3x4) _mat) : ro(_ro), box((_mat * _box).get_box()) {}
};


} //namespace iso
#endif //RENDER_OBJECT_H
