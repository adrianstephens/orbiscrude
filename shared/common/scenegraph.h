#ifndef SCENEGRAPH_H
#define SCENEGRAPH_H

#include "iso/iso.h"
#include "base/vector.h"
#include "packed_types.h"
#include "vector_iso.h"
#include "common/shader.h"

namespace iso {

class Texture;
class DataBuffer;
struct SampleBuffer;

void Init(Texture*,void*);
void DeInit(Texture*);
void Init(DataBuffer*,void*);
void DeInit(DataBuffer*);
void Init(SampleBuffer*,void*);
void DeInit(SampleBuffer*);

class ISOTexture {
	ISO::ptr_machine<void>	p;
public:
	typedef	ISO::ptr_machine<void> P;
	ISOTexture()					{}
	ISOTexture(const P &p) 	: p(p)	{}

	void operator=(const P &_p)		{ p = _p; }
	operator Texture&() const		{ return *(Texture*)this; }
};

struct Node {
	float3x4p		matrix;
	anything		children;
};

struct Children : anything {};

struct Scene {
	ISO_ptr<Node>	root;
};

struct Animation :	anything {};

typedef ISO_openarray<uint16>	Keys;

template<typename T> struct KeyedStream {
	Keys				keys;
	ISO_openarray<T>	values;
};

struct AnimationHierarchy {
	ISO_ptr<void>				root;
};

struct AnimationEventKey {
	float						time;
	ISO_ptr<void>				event;
};
typedef ISO_openarray<AnimationEventKey> AnimationEvents;

struct Bone {
	float3x4p					basepose;
	ISO_ptr<Bone>				parent;
};

struct BasePose : ISO_openarray<ISO_ptr<Bone> > {};

struct Collision_Ray {
	float3p						pos;
	float3p						dir;
	uint32						mask;
};

struct Collision_OBB {
	float3x4p					obb;
	uint32						mask;
};

struct Collision_Sphere {
	float3p						centre;
	float						radius;
	uint32						mask;
};

struct Collision_Cylinder {
	float3p						centre;
	float						radius;
	float3p						dir;
	uint32						mask;
};

struct Collision_Cone {
	float3p						centre;
	float						radius;
	float3p						dir;
	uint32						mask;
};

struct Collision_Capsule {
	float3p						centre;
	float						radius;
	float3p						dir;
	uint32						mask;
};

struct Collision_Patch {
	float3p						p[16];
	uint32						mask;
};

//struct Collision_Hull {
//};

}

namespace ent {
using namespace iso;

struct Colour {
	float3p	v;
	Colour& operator=(iso::float3 _v) { v = _v; return *this; }
};

struct Timer {
	float			t;
	int				die;
	ISO_ptr<void>	children;
	int				pulse;
};

struct External : tag {
	External(const char *s) : tag(s) {}
};

struct Spline {
	ISO_openarray<float3p>		pts;
};

struct Light2 {
	enum TYPE { BACKGROUND, AMBIENT, DIRECTIONAL, OMNI, SPOT, FOG, WORLD_FOG, SHADOW = 8 };
	TYPE			type;
	Colour			colour;
	float			range;
	float			spread;
	float3x4p		matrix;
};

struct Camera {
	float			zoom;
	float			env_start, env_end;
	float			clip_start, clip_end;
};

struct SphericalHarmonics : ISO_ptr<ISO_openarray<float3p> > {};

struct Attachment {
	int				id;
	float3x4p		matrix;
};

struct Cluster {
	ISO_ptr<void>	lorez;
	ISO_ptr<void>	hirez;
	float			distance;
};

struct Splitter {
	enum Decision {
		Distance, 				// hirez if distance <= value, lorez otherwise
		SubView, 				// lorez if sub view, hirez otherwise
		Distance_NoSub, 		// if sub view, render nothing, otherwise Distance
		Distance_LoSub,			// if sub view, lorez, otherwise Distance
		Quality_Distance,		// value = quality_threshold. Use to do different distances for different split screen modes.  value2 is lower quality distance, value3 is higher quality distance.
		Quality_Distance_NoSub,	// if sub view, render nothing, otherwise Quality_Distance
		Quality_Distance_LoSub,	// if sub view, lorez, otherwise Quality_Distance
	};
	ISO_ptr<void>	lorez;
	ISO_ptr<void>	hirez;
	uint8			split_decision; // SplitDecision
	float			value;
	float			value2;
	float			value3;
};

struct QualityToggle {
	ISO_ptr<void>	lorez;
	ISO_ptr<void>	hirez;
	float			quality_threshold;
};

} // namespace ent

//-----------------------------------------------------------------------------

namespace ISO {

ISO_DEFCALLBACK(SampleBuffer,	ISO_ptr<void>);

ISO_DEFCALLBACK(Texture,	ISOTexture::P);
ISO_DEFSAME(ISOTexture,		ISOTexture::P);

ISO_DEFCALLBACK(DataBuffer,	ISO_ptr<void>);
ISO_DEFUSERCOMPV(Node,		matrix, children);
ISO_DEFUSER(Children,		anything);
ISO_DEFUSERCOMPV(Scene,		root);

ISO_DEFUSER(Animation, ISO_openarray<ISO_ptr<void> >);
ISO_DEFUSERCOMPV(AnimationHierarchy, root);
ISO_DEFUSERCOMPV(AnimationEventKey, time, event);
ISO_DEFUSERCOMPV(Bone, basepose, parent);
ISO_DEFUSER(BasePose, ISO_openarray<ISO_ptr<Bone> >);

ISO_DEFUSERCOMPV(Collision_Ray,		pos, dir, mask);
ISO_DEFUSERCOMPV(Collision_OBB,		obb, mask);
ISO_DEFUSERCOMPV(Collision_Sphere,	centre, radius, mask);
ISO_DEFUSERCOMPV(Collision_Cylinder,centre, radius, dir, mask);
ISO_DEFUSERCOMPV(Collision_Cone,	centre, radius, dir, mask);
ISO_DEFUSERCOMPV(Collision_Capsule,	centre, radius, dir, mask);
ISO_DEFUSERCOMPV(Collision_Patch,	p, mask);

ISO_DEFUSERX(ent::External, const char*, "External");

ISO_DEFUSERCOMPXV(ent::Timer, "Timer", t, die, children, pulse);
ISO_DEFUSERCOMPXV(ent::Spline, "Spline",pts);

ISO_DEFUSERX(ent::Colour, float3p, "colour");
ISO_DEFUSER(ent::SphericalHarmonics, ISO_ptr<ISO_openarray<ent::Colour> >);

ISO_DEFUSERENUMQV(ent::Light2::TYPE, BACKGROUND, AMBIENT, DIRECTIONAL, OMNI, SPOT, FOG, WORLD_FOG, SHADOW);
ISO_DEFUSERCOMPXV(ent::Light2, "Light2", type, colour, range, spread, matrix);
ISO_DEFUSERCOMPXV(ent::Camera, "Camera", zoom, env_start, env_end, clip_start, clip_end);
ISO_DEFUSERCOMPXV(ent::Attachment, "Attachment", id, matrix);
ISO_DEFUSERCOMPXV(ent::Cluster, "Cluster", lorez, hirez, distance);
ISO_DEFUSERCOMPXV(ent::Splitter, "Splitter", lorez, hirez, split_decision, value, value2, value3);
ISO_DEFUSERCOMPXV(ent::QualityToggle, "QualityToggle", lorez, hirez, quality_threshold);

}//namespace ISO

//-----------------------------------------------------------------------------
#endif	// SCENEGRAPH_H
