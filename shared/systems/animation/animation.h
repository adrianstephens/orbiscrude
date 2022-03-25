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
	AnimationSetMessage(float _time, float _speed, float _value = -1.0f, crc32 _id = crc32()) : speed(_speed), value(_value), id(_id), handled(false)	{}
};

// AnimationLoopMessage
struct AnimationLoopMessage {
	float	dt;
	AnimationLoopMessage(float _dt) : dt(_dt)	{}
};

class AnimationHolder {
protected:
	ISO_ptr<Animation>	anim;
	ObjectReference		obj;
	void				*data;
	float				length;

	void	Init();

public:
	AnimationHolder(Object *_obj) : obj(_obj), data(0), length(0) {}
	AnimationHolder(Object *_obj, ISO_ptr<Animation> t) : anim(t), obj(_obj) { Init(); }
	~AnimationHolder();

	void	SetAnim(ISO_ptr<Animation> t);
	float	Evaluate(Pose *pose, float time);
	float	GetLength()	const	{ return length;}
	Object*	GetObject()	const	{ return obj;	}
};

}// namespace iso;

#endif //ANIMATION_H
