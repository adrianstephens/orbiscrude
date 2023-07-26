#ifndef ANIMATION_H
#define ANIMATION_H

#include "object.h"

namespace iso {

// AnimationSetMessage
struct AnimationSetMessage {
	float	time;
	float	speed;
	float	value;
	crc32	id;
	bool	handled;
	AnimationSetMessage(float time, float speed, float value = -1.0f, crc32 id = crc32()) : speed(speed), value(value), id(id), handled(false)	{}
};

// AnimationLoopMessage
struct AnimationLoopMessage {
	float	dt;
	AnimationLoopMessage(float dt) : dt(dt)	{}
};

class AnimationHolder {
protected:
	ISO_ptr<Animation>	anim;
	ObjectReference		obj;
	void				*data;
	float				length;

	void	Init();

public:
	AnimationHolder(Object *obj) : obj(obj), data(0), length(0) {}
	AnimationHolder(Object *obj, ISO_ptr<Animation> t) : anim(t), obj(obj) { Init(); }
	~AnimationHolder();

	void	SetAnim(ISO_ptr<Animation> t);
	float	Evaluate(Pose *pose, float time);
	float	GetLength()	const	{ return length;}
	Object*	GetObject()	const	{ return obj;	}
};

}// namespace iso;

#endif //ANIMATION_H
