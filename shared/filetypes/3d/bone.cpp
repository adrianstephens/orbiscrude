#include "bone.h"
#include "geometry.h"

using namespace iso;

BONE iso::GetParent(BONE a) {
	static uint8 parents[] = {
		BONE_ROOT,
		BONE_ROOT,					//BONE_HIPS,

		BONE_HIPS,					//BONE_SPINE1,
		BONE_SPINE1,				//BONE_SPINE2,
		BONE_SPINE2,				//BONE_SPINE3,
		BONE_SPINE3,				//BONE_NECK1,
		BONE_NECK1,					//BONE_NECK2,
		BONE_NECK2,					//BONE_HEAD,

		BONE_SPINE2,				//BONE_LEFT_CLAVICLE,
		BONE_LEFT_CLAVICLE,			//BONE_LEFT_SHOULDER,
		BONE_LEFT_SHOULDER,			//BONE_LEFT_ARM,
		BONE_LEFT_ARM,				//BONE_LEFT_FOREARM,
		BONE_LEFT_FOREARM,			//BONE_LEFT_HAND,
		BONE_LEFT_HAND,				//BONE_LEFT_THUMB1,
		BONE_LEFT_THUMB1,			//BONE_LEFT_THUMB2,
		BONE_LEFT_THUMB2,			//BONE_LEFT_THUMB3,
		BONE_LEFT_HAND,				//BONE_LEFT_INDEX1,
		BONE_LEFT_INDEX1,			//BONE_LEFT_INDEX2,
		BONE_LEFT_INDEX2,			//BONE_LEFT_INDEX3,
		BONE_LEFT_HAND,				//BONE_LEFT_MIDDLE1,
		BONE_LEFT_MIDDLE1,			//BONE_LEFT_MIDDLE2,
		BONE_LEFT_MIDDLE2,			//BONE_LEFT_MIDDLE3,
		BONE_LEFT_HAND,				//BONE_LEFT_RING1,
		BONE_LEFT_RING1,			//BONE_LEFT_RING2,
		BONE_LEFT_RING2,			//BONE_LEFT_RING3,
		BONE_LEFT_HAND,				//BONE_LEFT_PINKY1,
		BONE_LEFT_PINKY1,			//BONE_LEFT_PINKY2,
		BONE_LEFT_PINKY2,			//BONE_LEFT_PINKY3,

		BONE_SPINE2,				//BONE_RIGHT_CLAVICLE,
		BONE_RIGHT_CLAVICLE,		//BONE_RIGHT_SHOULDER,
		BONE_RIGHT_SHOULDER,		//BONE_RIGHT_ARM,
		BONE_RIGHT_ARM,				//BONE_RIGHT_FOREARM,
		BONE_RIGHT_FOREARM,			//BONE_RIGHT_HAND,
		BONE_RIGHT_HAND,			//BONE_RIGHT_THUMB1,
		BONE_RIGHT_THUMB1,			//BONE_RIGHT_THUMB2,
		BONE_RIGHT_THUMB2,			//BONE_RIGHT_THUMB3,
		BONE_RIGHT_HAND,			//BONE_RIGHT_INDEX1,
		BONE_RIGHT_INDEX1,			//BONE_RIGHT_INDEX2,
		BONE_RIGHT_INDEX2,			//BONE_RIGHT_INDEX3,
		BONE_RIGHT_HAND,			//BONE_RIGHT_MIDDLE1,
		BONE_RIGHT_MIDDLE1,			//BONE_RIGHT_MIDDLE2,
		BONE_RIGHT_MIDDLE2,			//BONE_RIGHT_MIDDLE3,
		BONE_RIGHT_HAND,			//BONE_RIGHT_RING1,
		BONE_RIGHT_RING1,			//BONE_RIGHT_RING2,
		BONE_RIGHT_RING2,			//BONE_RIGHT_RING3,
		BONE_RIGHT_HAND,			//BONE_RIGHT_PINKY1,
		BONE_RIGHT_PINKY1,			//BONE_RIGHT_PINKY2,
		BONE_RIGHT_PINKY2,			//BONE_RIGHT_PINKY3,

		BONE_HIPS,					//BONE_LEFT_THIGH,
		BONE_LEFT_THIGH,			//BONE_LEFT_LEG,
		BONE_LEFT_LEG,				//BONE_LEFT_FOOT,
		BONE_LEFT_FOOT,				//BONE_LEFT_TOES,

		BONE_HIPS,					//BONE_RIGHT_THIGH,
		BONE_RIGHT_THIGH,			//BONE_RIGHT_LEG,
		BONE_RIGHT_LEG,				//BONE_RIGHT_FOOT,
		BONE_RIGHT_FOOT,			//BONE_RIGHT_TOES,
	};
	return (BONE)parents[a];
}

const char *iso::GetName(BONE a) {
	static const char *names[] = {
		"root",			//BONE_ROOT,
		"hips",			//BONE_HIPS,
		"spine1",		//BONE_SPINE1,
		"spine2",		//BONE_SPINE2,
		"spine3",		//BONE_SPINE3,
		"neck1",		//BONE_NECK1,
		"neck2",		//BONE_NECK2,
		"head",			//BONE_HEAD,

		"Lclavicle",	//BONE_LEFT_CLAVICLE,
		"Lshoulder",	//BONE_LEFT_SHOULDER,
		"Larm",			//BONE_LEFT_ARM,
		"Lforearm",		//BONE_LEFT_FOREARM,
		"Lhand",		//BONE_LEFT_HAND,
		"Lthumb1",		//BONE_LEFT_THUMB1,
		"Lthumb2",		//BONE_LEFT_THUMB2,
		"Lthumb3",		//BONE_LEFT_THUMB3,
		"Lindex1",		//BONE_LEFT_INDEX1,
		"Lindex2",		//BONE_LEFT_INDEX2,
		"Lindex3",		//BONE_LEFT_INDEX3,
		"Lmiddle1",		//BONE_LEFT_MIDDLE1,
		"Lmiddle2",		//BONE_LEFT_MIDDLE2,
		"Lmiddle3",		//BONE_LEFT_MIDDLE3,
		"Lring1",		//BONE_LEFT_RING1,
		"Lring2",		//BONE_LEFT_RING2,
		"Lring3",		//BONE_LEFT_RING3,
		"Lpinky1",		//BONE_LEFT_PINKY1,
		"Lpinky2",		//BONE_LEFT_PINKY2,
		"Lpinky3",		//BONE_LEFT_PINKY3,

		"Rclavicle",	//BONE_RIGHT_CLAVICLE,
		"Rshoulder",	//BONE_RIGHT_SHOULDER,
		"Rarm",			//BONE_RIGHT_ARM,
		"Rforearm",		//BONE_RIGHT_FOREARM,
		"Rhand",		//BONE_RIGHT_HAND,
		"Rthumb1",		//BONE_RIGHT_THUMB1,
		"Rthumb2",		//BONE_RIGHT_THUMB2,
		"Rthumb3",		//BONE_RIGHT_THUMB3,
		"Rindex1",		//BONE_RIGHT_INDEX1,
		"Rindex2",		//BONE_RIGHT_INDEX2,
		"Rindex3",		//BONE_RIGHT_INDEX3,
		"Rmiddle1",		//BONE_RIGHT_MIDDLE1,
		"Rmiddle2",		//BONE_RIGHT_MIDDLE2,
		"Rmiddle3",		//BONE_RIGHT_MIDDLE3,
		"Rring1",		//BONE_RIGHT_RING1,
		"Rring2",		//BONE_RIGHT_RING2,
		"Rring3",		//BONE_RIGHT_RING3,
		"Rpinky1",		//BONE_RIGHT_PINKY1,
		"Rpinky2",		//BONE_RIGHT_PINKY2,
		"Rpinky3",		//BONE_RIGHT_PINKY3,

		"Lthigh",		//BONE_LEFT_THIGH,
		"Lleg",			//BONE_LEFT_LEG,
		"Lfoot",		//BONE_LEFT_FOOT,
		"Ltoes",		//BONE_LEFT_TOES,

		"Rthigh",		//BONE_RIGHT_THIGH,
		"Rleg",			//BONE_RIGHT_LEG,
		"Rfoot",		//BONE_RIGHT_FOOT,
		"Rtoes",		//BONE_RIGHT_TOES,
	};
	if (a & BONE_ROLL) {
		static fixed_string<16>	name;
		name = str(names[a & 0xff]) + "_roll";
		return name;
	}
	return names[a];
}

float4 iso::GetRotCone(BONE a) {
	static const float4 cones[] = {			// cones, dir.xyz,cos(angle)
		float4(	0,	0,	1,	0),				//BONE_ROOT,
		float4( 0.0f,  0.0f,  1.0f, 0.95f),	//BONE_HIPS,
		float4( 1.0f,  0.0f,  0.0f, 0.64f),	//BONE_SPINE1,
		float4(	0,	0,	1,	0),				//BONE_SPINE2,
		float4(	0,	0,	1,	0),				//BONE_SPINE3,
		float4(	0,	0,	1,	0),				//BONE_NECK1,
		float4(	0,	0,	1,	0),				//BONE_NECK2,
		float4( 1.0f,  0.7f,  0.0f, 0.76f),	//BONE_HEAD,

		float4(	0,	0,	1,	0),				//BONE_LEFT_CLAVICLE,
		float4( 1.0f,  0.0f,  0.0f, 0.17f),	//BONE_LEFT_SHOULDER,
		float4(	0,	0,	1,	0),				//BONE_LEFT_ARM,
		float4( 0.0f,  0.0f, -1.0f, 0.00f),	//BONE_LEFT_FOREARM,
		float4( 1.0f,  0.0f,  0.0f, 0.76f),	//BONE_LEFT_HAND,
		float4(	0,	0,	1,	0),				//BONE_LEFT_THUMB1,
		float4(	0,	0,	1,	0),				//BONE_LEFT_THUMB2,
		float4(	0,	0,	1,	0),				//BONE_LEFT_THUMB3,
		float4(	0,	0,	1,	0),				//BONE_LEFT_INDEX1,
		float4(	0,	0,	1,	0),				//BONE_LEFT_INDEX2,
		float4(	0,	0,	1,	0),				//BONE_LEFT_INDEX3,
		float4(	0,	0,	1,	0),				//BONE_LEFT_MIDDLE1,
		float4(	0,	0,	1,	0),				//BONE_LEFT_MIDDLE2,
		float4(	0,	0,	1,	0),				//BONE_LEFT_MIDDLE3,
		float4(	0,	0,	1,	0),				//BONE_LEFT_RING1,
		float4(	0,	0,	1,	0),				//BONE_LEFT_RING2,
		float4(	0,	0,	1,	0),				//BONE_LEFT_RING3,
		float4(	0,	0,	1,	0),				//BONE_LEFT_PINKY1,
		float4(	0,	0,	1,	0),				//BONE_LEFT_PINKY2,
		float4(	0,	0,	1,	0),				//BONE_LEFT_PINKY3,

		float4(	0,	0,	1,	0),				//BONE_RIGHT_CLAVICLE,
		float4( 1.0f,  0.0f,  0.0f, 0.17f),	//BONE_RIGHT_SHOULDER,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_ARM,
		float4( 0.0f,  0.0f,  1.0f, 0.00f),	//BONE_RIGHT_FOREARM,
		float4( 1.0f,  0.0f,  0.0f, 0.76f),	//BONE_RIGHT_HAND,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_THUMB1,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_THUMB2,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_THUMB3,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_INDEX1,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_INDEX2,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_INDEX3,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_MIDDLE1,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_MIDDLE2,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_MIDDLE3,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_RING1,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_RING2,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_RING3,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_PINKY1,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_PINKY2,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_PINKY3,

		float4( 1.0f,  0.0f,  0.0f, 0.34f),	//BONE_LEFT_THIGH,
		float4( 0.5f, -1.0f,  0.0f, 0.50f),	//BONE_LEFT_LEG,
		float4( 1.0f,  1.0f,  0.0f, 0.50f),	//BONE_LEFT_FOOT,
		float4(	0,	0,	1,	0),				//BONE_LEFT_TOES,

		float4( 1.0f,  0.0f,  0.0f, 0.34f),	//BONE_RIGHT_THIGH,
		float4( 0.5f, -1.0f,  0.0f, 0.50f),	//BONE_RIGHT_LEG,
		float4( 1.0f,  1.0f,  0.0f, 0.50f),	//BONE_RIGHT_FOOT,
		float4(	0,	0,	1,	0),				//BONE_RIGHT_TOES,
	};
	return cones[a];
}

BONE iso::LookupBone(const BoneMapping *m, size_t num, const char *name) {
	for (int i = 0; i < num; i++) {
		if (m[i].name == str(name))
			return m[i].bone;
	}
	return BONE_ROOT;
}
