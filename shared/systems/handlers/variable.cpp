#include "variable.h"
#include "crc_dictionary.h"

using namespace iso;

namespace ent {
struct Variables	: anything {};
struct Interpolator {
	ISO_ptr<void>	value;
	float			attack, sustain, release;
};
}

struct Variables : DeleteOnDestroy<Variables> {
	ISO::OpenArrayView<ISO::ptr<void>>	v;

	void operator()(LookupMessage &m) {
		int	i = v.GetIndex(tag2(m.id));
		if (i >= 0) {
			m.result = v[i];
			m.type = v[i].GetType();
			return;
		}
	}

	Variables(Object *obj, anything &v) : DeleteOnDestroy<Variables>(obj), v(v) { obj->AddHandler<LookupMessage>(this);  }
};

template<typename T> struct Interpolator0 : DeleteOnDestroy<Interpolator0<T> >, HandlesWorld<Interpolator0<T>, FrameEvent2> {
	crc32			id;
	float			start, attack, sustain, release;
	T				&v, i, f;

	void operator()(FrameEvent2 &ev) {
		float	t = ev.time - start;
		if (sustain < 0) {
			v = lerp(i, f, min(t / attack, 1));
		} else if (t < attack + sustain + release) {
			v = lerp(i, f, t < attack ? t / attack : min((attack + sustain + release - t) / release, 1));
		} else {
			delete this;
		}
	}
	void operator()(WorldEvent *ev) {
		if (ev->state == WorldEvent::END)
			delete this;
	}

	Interpolator0(float time, crc32 _id, void *_pi, const T *_pf, float _attack, float _sustain, float _release)
		: DeleteOnDestroy<Interpolator0<T> >(World::Current())
		, id(_id)
		, start(time), attack(_attack), sustain(_sustain), release(_release)
		, v(*(T*)_pi), i(v), f(*_pf)
	{}
	~Interpolator0() {
		v = i;
	}
};

template<typename T> struct Interpolator : DeleteOnDestroy<Interpolator<T> >, HandlesWorld<Interpolator<T>, FrameEvent2> {
	ObjectReference	obj;
	crc32			id;
	float			start, attack, sustain, release;
	T				v, f, i;

	void operator()(FrameEvent2 &ev) {
		float	t = ev.time - start;
		if (sustain < 0) {
			v = lerp(i, f, min(t / attack, 1));
		} else if (t < attack + sustain + release) {
			v = lerp(i, f, t < attack ? t / attack : min((attack + sustain + release - t) / release, 1));
		} else {
			delete this;
		}
	}

	void operator()(LookupMessage &m) {
		if (m.id == id) {
			m.result = &v;
			m.type = ISO::getdef<T>();
		}
	}

	Interpolator(float time, Object *_obj, crc32 _id, void *_pi, const T *_pf, float _attack, float _sustain, float _release)
		: DeleteOnDestroy<Interpolator<T> >(_obj)
		, obj(_obj)
		, id(_id)
		, start(time), attack(_attack), sustain(_sustain), release(_release)
		, f(*_pf), i(*(T*)_pi)
	{
		v = i;
		obj->AddHandler<LookupMessage>(this);
	}
};

void *FindVariable(Object *obj, crc32 id) {
	LookupMessage	m(id);
	obj->Send(m);
	if (!m.result)
		m.result	= GetShaderParameter(id);
	return m.result;
}

template<typename T> bool CreateInterpolator0(float time, Object *obj, crc32 id, const T &t, float attack, float sustain, float release) {
	if (void *p = FindVariable(obj, id)) {
		new Interpolator0<T>(time, id, p, &t, attack, sustain, release);
		return true;
	}
	return false;
}

template<typename T> bool CreateInterpolator(float time, Object *obj, crc32 id, const T &t, float attack, float sustain, float release) {
	if (void *p = FindVariable(obj, id)) {
		new Interpolator<T>(time, obj, id, p, &t, attack, sustain, release);
		return true;
	}
	return false;
}

template bool CreateInterpolator(float time, Object *obj, crc32 id, const float &t, float attack, float sustain, float release);
template bool CreateInterpolator(float time, Object *obj, crc32 id, const float2p &t, float attack, float sustain, float release);
template bool CreateInterpolator(float time, Object *obj, crc32 id, const float3p &t, float attack, float sustain, float release);
template bool CreateInterpolator(float time, Object *obj, crc32 id, const float4p &t, float attack, float sustain, float release);

template bool CreateInterpolator0(float time, Object *obj, crc32 id, const float &t, float attack, float sustain, float release);
template bool CreateInterpolator0(float time, Object *obj, crc32 id, const float2p &t, float attack, float sustain, float release);
template bool CreateInterpolator0(float time, Object *obj, crc32 id, const float3p &t, float attack, float sustain, float release);
template bool CreateInterpolator0(float time, Object *obj, crc32 id, const float4p &t, float attack, float sustain, float release);

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("Variables", 0x0a3affb2)>::Create(const CreateParams &cp, crc32 id, const void *t) {
		new Variables(cp.obj, *(anything*)t);
	}
	TypeHandlerCRC<ISO_CRC("Variables", 0x0a3affb2)> thVariables;

	template<> void TypeHandlerCRC<ISO_CRC("Interpolator", 0x169a41ae)>::Create(const CreateParams &cp, crc32 _id, const void *t) {
		ent::Interpolator *i = (ent::Interpolator*)t;

		ISO_ptr<void>	v	= i->value;
		crc32			id	= v.ID().get_crc32();
		if (void *p	= FindVariable(cp.obj, id)) {
			if (v.GetType()->SameAs<float>())
				new Interpolator<float>(cp.Time(), cp.obj, id, p, i->value, i->attack, i->sustain, i->release);
			else if (v.GetType()->SameAs<float[2]>())
				new Interpolator<float2p>(cp.Time(), cp.obj, id, p, i->value, i->attack, i->sustain, i->release);
			else if (v.GetType()->SameAs<float[3]>())
				new Interpolator<float3p>(cp.Time(), cp.obj, id, p, i->value, i->attack, i->sustain, i->release);
			else if (v.GetType()->SameAs<float[4]>())
				new Interpolator<float4p>(cp.Time(), cp.obj, id, p, i->value, i->attack, i->sustain, i->release);
		}
	}
	TypeHandlerCRC<ISO_CRC("Interpolator", 0x169a41ae)> thInterpolator;
}

void FlashObject(float time, Object *obj, param(colour) c, float attack, float sustain, float release) {
	float4p	fog_dir1, fog_col1;
	fog_dir1 = (float4)w_axis;
	fog_col1 = c.rgba;
	CreateInterpolator(time, obj, ISO_CRC("fog_dir1", 0x1e7fa614), fog_dir1, attack, sustain, release);
	CreateInterpolator(time, obj, ISO_CRC("fog_col1", 0x5364ddc0), fog_col1, attack, sustain, release);
}
