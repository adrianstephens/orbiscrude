#include "object.h"
#include "extra/random.h"
#include "animation.h"
#include "packed_types.h"
#include "base/algorithm.h"
#include "profiler.h"

using namespace iso;

typedef static_array<ISO_ptr<void>, 16> Events;
Joint	joint_stack[2048], *joint_sp;

#define CHECK_INSTANCE

//-----------------------------------------------------------------------------

template<class A, class B> class allocator_mixin2 : public B {
public:
							allocator_mixin2()					{}
	template<typename X>	allocator_mixin2(X &x) : B(x)		{}
	template<typename X>	allocator_mixin2(const X &x) : B(x)	{}
	template<typename X, typename Y> allocator_mixin2(const X &x, const Y &y) : B(x, y)	{}
	void				*alloc(size_t size, size_t align = 4)	{ return static_cast<A*>(this)->_alloc(size, align); }
	template<typename T>				T*		alloc()			{ return (T*)alloc(sizeof(T), sizeof(void*)); }
	template<typename T, typename N>	T*		alloc(N n)		{ return (T*)alloc(sizeof(T) * n, sizeof(void*)); }
	template<typename T, typename N>	T*		allocp(N n)		{ return (T*)alloc(T::calc_size(n), sizeof(void*)); }

	template<typename T>				operator T*()			{ return alloc<T>(); }
	template<typename T>	void		put(const T& t)			{ *alloc<T>() = t; }
										operator void*()		{ return static_cast<A*>(this)->getp(); }
};

#ifdef CHECK_INSTANCE

class _an_data : public allocator_mixin2<_an_data, linear_allocator> {
public:
	_an_data(void *_p) : allocator_mixin2<_an_data, linear_allocator>(_p)	{}
	void	*_alloc(size_t size, size_t align) {
		linear_allocator::_alloc(2 * sizeof(void*));
		return linear_allocator::_alloc(size, align);
	}
};
class an_data_reserve : public _an_data {
public:
	an_data_reserve(void *_p) : _an_data(_p)	{}
};
class an_data_nosize : public allocator_mixin2<an_data_nosize, an_data_reserve> {
public:
	an_data_nosize(void *_p) : allocator_mixin2<an_data_nosize, an_data_reserve>(_p)	{}
	void	*_alloc(size_t size, size_t align) {
		uintptr_t	*p2 = (uintptr_t*)getp();
		ISO_ASSERT(p2[0] == (uintptr_t)p2);
		return _an_data::_alloc(size, align);
	}
};
class an_data : public allocator_mixin2<an_data, an_data_nosize> {
public:
	an_data(void *_p) : allocator_mixin2<an_data, an_data_nosize>(_p)	{}
	void	*_alloc(size_t size, size_t align) {
		uintptr_t	*p2 = (uintptr_t*)getp();
		ISO_ASSERT(p2[0] == (uintptr_t)p2 && p2[1] == size);
		return _an_data::_alloc(size, align);
	}
};
class an_data_create : public allocator_mixin2<an_data_create, an_data> {
	void	*end;
public:
	an_data_create(void *_p, uint32 size)	: allocator_mixin2<an_data_create, an_data>(_p), end((char*)_p + size)	{}
	an_data_create(void *_p, void *_end)	: allocator_mixin2<an_data_create, an_data>(_p), end(_end)	{}
	~an_data_create() { ISO_ASSERT(getp() <= end); }
	void	*_alloc(size_t size, size_t align) {
		uintptr_t	*p2 = (uintptr_t*)getp();
		p2[0] = (uintptr_t)p2;
		p2[1] = size;
		return _an_data::_alloc(size, align);
	}
};

#else

class _an_data : public linear_allocator {};

typedef _an_data	an_data_reserve;
typedef _an_data	an_data_nosize;
typedef _an_data	an_data;
class an_data_create : public _an_data {
public:
	an_data_create(void *_p, uint32 size)	: _an_data(_p)	{}
	an_data_create(void *_p, void *_end)	: _an_data(_p)	{}
};
#endif

inline void*	operator new(size_t size, an_data_create &a)	{ return a._alloc(size, sizeof(void*)); }
inline void		operator delete(void *p, an_data_create &a)		{}

class an_calls {
public:
	void			(*vReserve)		(const void *p, an_data_reserve &d, Object *obj);
	void			(*vCreate)		(const void *p, an_data_create &d, Object *obj);
	void			(*vDestroy)		(const void *p, an_data &d);
	float			(*vGetValue)	(const void *p, an_data &d, float t);
	float			(*vEvaluate)	(const void *p, an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t);
};

class an_type : public an_calls, public ISO::TypeUserSave	{
public:
	an_type(tag id) : ISO::TypeUserSave(id, NULL)	{ flags |= TYPE_FIXED; }
};

template<class T> class an_template : public an_type	{
	static void		Reserve			(const void *p, an_data_reserve &d, Object *obj)	{ ((T*)p)->Reserve(d, obj);			}
	static void		Create			(const void *p, an_data_create &d, Object *obj)		{ ((T*)p)->Create(d, obj);			}
	static void		Destroy			(const void *p, an_data &d)							{ ((T*)p)->Destroy(d);				}
	static float	GetValue		(const void *p, an_data &d, float t)				{ return ((T*)p)->GetValue(d, t);	}
	static float	Evaluate		(const void *p, an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) { return ((T*)p)->Evaluate(d, joints, initial, njoints, events, t); }
public:
	an_template(tag id) : an_type(id)	{
		vReserve		= Reserve;
		vCreate			= Create;
		vDestroy		= Destroy;
		vGetValue		= GetValue;
		vEvaluate		= Evaluate;
	}
};

struct an_base {
	void	Reserve(void *d, Object *obj)		{}
	void	Create(void *d, Object *obj)		{}
	void	Destroy(void *d)					{}
	float	GetValue(void *d, float t)			{ return 0; }
	float	Evaluate(void *d, Joint *joints, Joint *initial, int njoints, Events &events, float t) { return 0; }
};

struct an_item : ISO_ptr<an_base> {
	const an_type*	gettype()						const	{ return static_cast<const an_type*>(GetType());	}
	void	Reserve(an_data_reserve &d, Object *obj)const	{ gettype()->vReserve(*this, d, obj);			}
	void	Create(an_data_create &d, Object *obj)	const	{ gettype()->vCreate(*this, d, obj);			}
	void	Destroy(an_data &d)						const	{ gettype()->vDestroy(*this, d);				}
	float	GetValue(an_data &d, float t = 0.0f)	const	{ return gettype()->vGetValue(*this, d, t);		}
	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) const { return gettype()->vEvaluate(*this, d, joints, initial, njoints, events, t); }

	void	Reserve2(an_data_reserve &d, Object *obj);
	void	Create2(an_data_create &d, Object *obj);
	void	Destroy2(an_data &d);

	tag2	ID()									const	{ return ISO_ptr<an_base>::ID(); }
};


//-----------------------------------------------------------------------------

struct an_library : an_base {
	ISO_openarray<an_item>	anims;
	an_item		anim;

	static ISO::OpenArrayView<an_item>	anim_library;

	static an_item	Find(crc32 id) {
		int i = anim_library.GetIndex(tag2(id));
		if (i >= 0)
			return anim_library[i];
		ISO_ptr<void>	p;
		return (an_item&)p;
	}

	void	Reserve(an_data_reserve &d, Object *obj) {
		anim_library = anims;
		anim.Reserve(d, obj);
	}
	void	Create(an_data_create &d, Object *obj) {
		anim_library = anims;
		anim.Create(d, obj);
	}
	void	Destroy(an_data &d) {
		anim.Destroy(d);
	}
	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		anim_library = anims;
		return anim.Evaluate(d, joints, initial, njoints, events, t);
	}
	float	GetValue(an_data &d, float t) {
		return anim->GetValue(d, t);
	}
};

ISO::OpenArrayView<an_item>	an_library::anim_library;

struct an_fromlib : an_base {
	crc32		id;

	struct instance {
		an_item		anim;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		if (const an_item &a = an_library::Find(id))
			a.Reserve(d, obj);
	}
	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		if (my->anim = an_library::Find(id))
			my->anim.Create(d, obj);
	}
	void	Destroy(an_data &d) {
		instance	*my(d);
		if (my->anim) {
			my->anim.Destroy(d);
			my->anim.Header()->release();//Clear();
		}
	}
	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		return my->anim ? my->anim.Evaluate(d, joints, initial, njoints, events, t) : 0;
	}
	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		return  my->anim ? my->anim.GetValue(d, t) : 0;
	}
};


struct AnimationRT : Animation {
	typedef typename Keys::view_t	KeysView;

	struct binding {
		uint32					flags;
		float4p					*rot;
		compressed_quaternion	*comp_rot;
		float3p					*pos;
		float3p					*scl;
		KeysView				rotkeys, poskeys, sclkeys;
	};
	struct instance : trailing_array<instance, binding> {
		Object		*obj;
		size_t		length;
		AnimationEvents::iterator event_iter, event_iter_end, event_iter_rend;
		float		loop_threshold;
		float		prev_time;
		int8		event_dir;
		void		*end_data;
		instance(Object *_obj)
			: obj(_obj)
			, length(0)
			, event_iter(NULL)
		{}
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		if (Pose *pose = obj->Property<Pose>())
			d.allocp<instance>(pose->Count());
	}

	void	Create(an_data_create &d, Object *obj)	{
		Pose			*pose	= obj->Property<Pose>();
		int				nbones	= pose->Count();
		int				nanim	= Count();

//		instance	*my	= new(d.createn<instance>(nbones)) instance(obj);
		instance	*my	= new(nbones, d) instance(obj);
//		instance	*my	= new(d.create(instance::size(nbones))) instance(obj);
		my->end_data	= d;

		memset(my->begin(), 0, nbones * sizeof(binding));
		for (int i = 0, j = Pose::INVALID; i < nanim; i++) {
			ISO_ptr<Animation>	anim2	= (*this)[i];
			crc32				crc		= anim2.ID();

			if (crc == ISO_CRC("events", 0x5387574a)) {
				AnimationEvents *events = (*this)[i];
				if (my->event_iter = events->begin()) {
					my->event_iter_end = events->end();
					--(my->event_iter_rend = events->begin());
					my->event_dir	= 0;
					my->length		= max(my->length, events->back().time * 30);
				}

			} else if ((j = pose->Find(crc, j + 1)) != Pose::INVALID) {
				binding	&b = (*my)[j];
				b.flags	|= 1;

				uint32 len	= 0;
				if (ISO_ptr<void> &p = (*anim2)["rot"]) {
					switch (p.GetType()->GetType()) {
						case ISO::COMPOSITE: {
							KeyedStream<float4p>	*s = p;
							b.rot		= s->values;
							b.rotkeys	= s->keys.View();
							len			= s->keys.back();
							break;
						}
						case ISO::OPENARRAY: {
							ISO_openarray<float4p>		*a = p;
							b.rot		= *a;
							len			= a->Count() - 1;
							b.flags		|= int(len == 0) << 1;
							break;
						}
						default:
							b.rot		= p;
							b.flags		|= 1 << 1;
							break;
					}
				} else if (ISO_ptr<void> &p = (*anim2)["comp_rot"]) {
					switch (p.GetType()->GetType()) {
						case ISO::COMPOSITE: {
							KeyedStream<compressed_quaternion>	*s = p;
							b.rotkeys	= s->keys;
							b.comp_rot	= s->values;
							len			= s->keys.back();
							break;
						}
						case ISO::OPENARRAY: {
							ISO_openarray<compressed_quaternion> *a = p;
							b.comp_rot	= *a;
							len			= a->Count() - 1;
							b.flags		|= int(len == 0) << 1;
							break;
						}
						default:
							b.comp_rot	= p;
							b.flags		|= 1 << 1;
							break;
					}
				}
				my->length = max(my->length, len);
				if (ISO_ptr<void> &p = (*anim2)["pos"]) {
					uint32	len = 0;
					switch (p.GetType()->GetType()) {
						case ISO::COMPOSITE: {
							KeyedStream<float3p>	*s = p;
							b.poskeys	= s->keys;
							b.pos		= s->values;
							len			= s->keys.back();
							break;
						}
						case ISO::OPENARRAY: {
							ISO_openarray<float3p>		*a = p;
							b.pos		= *a;
							len			= a->Count() - 1;
							b.flags		|= int(len == 0) << 2;
							break;
						}
						default:
							b.pos		= p;
							b.flags		|= 1 << 2;
							break;
					}
					my->length = max(my->length, len);
				}
				if (ISO_ptr<void> &p = (*anim2)["scale"]) {
					uint32	len = 0;
					switch (p.GetType()->GetType()) {
						case ISO::COMPOSITE: {
							KeyedStream<float3p>	*s = p;
							b.sclkeys	= s->keys;
							b.scl		= s->values;
							len			= s->keys.back();
							break;
						}
						case ISO::OPENARRAY: {
							ISO_openarray<float3p>	*a = p;
							b.scl		= *a;
							len			= a->Count() - 1;
							b.flags		|= int(len == 0) << 3;
							break;
						}
						default:
							b.scl		= p;
							b.flags		|= 1 << 3;
							break;
					}
					my->length = max(my->length, len);
				}
			}
		}
		my->loop_threshold = my->length / (30.0f * 2.0f);
		my->prev_time	= 0.0f;
	}
	void	Destroy(an_data_nosize &d)	{
		instance	*my(d);
		d	= my->end_data;
	}

	float Evaluate(an_data_nosize &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		d = my->end_data;

		if (my->event_iter) {
			if (float dt = t - my->prev_time) {
				// wrap
				int8 dir;
				if (dt < -my->loop_threshold) {
					// forward
					if (my->event_iter != my->event_iter_rend) {
						while (my->event_iter != my->event_iter_end) {
							events.push_back(my->event_iter->event);
							++my->event_iter;
						}
						my->event_iter = my->event_iter_rend;
					}
					++my->event_iter;
					dir = +1;
				} else if (dt > my->loop_threshold) {
					// backward
					if (my->event_iter != my->event_iter_end) {
						while (my->event_iter != my->event_iter_rend) {
							events.push_back(my->event_iter->event);
							--my->event_iter;
						}
						my->event_iter = my->event_iter_end;
					}
					--my->event_iter;
					dir = -1;
				} else if ((dir = dt > 0.0f ? +1 : -1) != my->event_dir) {
					// reverse
					if (dir > 0) {
						while (my->event_iter != my->event_iter_end && (my->event_iter == my->event_iter_rend || my->event_iter->time < my->prev_time))
							++my->event_iter;
					} else {
						while (my->event_iter != my->event_iter_rend && (my->event_iter == my->event_iter_end || my->event_iter->time > my->prev_time))
							--my->event_iter;
					}
				}
				// advance
				if (dir > 0) {
					// forward
					while (my->event_iter != my->event_iter_end && my->event_iter->time <= t) {
						events.push_back(my->event_iter->event);
						++my->event_iter;
					}
				} else {
					// backward
					while (my->event_iter != my->event_iter_rend && my->event_iter->time >= t) {
						events.push_back(my->event_iter->event);
						--my->event_iter;
					}
				}
				my->event_dir	= dir;
			} else
				my->event_dir	= 0;
		}
		my->prev_time	= t;

		float	t2	= t * 30;
		int		ti	= int(t2);
		if (my->length && t2 > my->length + 0.01f)
			return 0;

		float	tf;
		if (ti == my->length) {
			--ti;
			tf = 1.0f;
		} else
			tf = t2 - ti;

		for (int i = 0; i < njoints; i++, joints++) {
			binding	&b = (*my)[i];

			if (b.flags == 0) {
				joints->weight = 0;
				continue;
			}

			if (b.rot) {
				if (b.flags & (1<<1)) {
					joints->rot = quaternion(float4(b.rot[0]));
				} else if (auto keys = b.rotkeys) {
					int		k	= lower_boundc(keys, ti) - keys.begin();
					joints->rot = slerp(quaternion(b.rot[k]), quaternion(b.rot[k + 1]), (t2 - keys[k]) / (keys[k + 1] - keys[k]));
				} else {
					joints->rot = lerp(float4(b.rot[ti]), float4(b.rot[ti + 1]), tf);
				}
			} else if (b.comp_rot) {
				if (b.flags & (1<<1)) {
					joints->rot = (quaternion)b.comp_rot[0];
				} else if (auto keys = b.rotkeys) {
					int		k	= lower_boundc(keys, ti) - keys.begin();
					joints->rot = slerp<float>(b.comp_rot[k], b.comp_rot[k + 1], (t2 - keys[k]) / (keys[k + 1] - keys[k]));
				} else {
					joints->rot = lerp_check<float>(b.comp_rot[ti], b.comp_rot[ti + 1], tf);
				}
			} else if (initial) {
				joints->rot = initial[i].rot;
			} else {
				joints->rot = identity;
			}

			if (b.pos) {
				if (b.flags & (1<<2)) {
					joints->trans4 = concat(b.pos[0], one);
				} else if (auto keys = b.poskeys) {
					int		k	= lower_boundc(keys, ti) - keys.begin();
					joints->trans4 = lerp(position3(b.pos[k]), position3(b.pos[k + 1]), (t2 - keys[k]) / (keys[k + 1] - keys[k]));
				} else {
					joints->trans4 = lerp(position3(b.pos[ti]), position3(b.pos[ti + 1]), tf);
				}
			} else if (initial) {
				joints->trans4 = initial[i].trans4;
			} else {
				joints->trans4 = zero;
			}

			if (b.scl) {
				if (b.flags & (1<<3)) {
					joints->scale = b.scl[0];
				} else if (auto keys = b.sclkeys) {
					int		k	= lower_boundc(keys, ti) - keys.begin();
					joints->scale = lerp(position3(b.scl[k]), position3(b.scl[k + 1]), (t2 - keys[k]) / (keys[k + 1] - keys[k])).v;
				} else {
					joints->scale = lerp(float3(b.scl[ti]), float3(b.scl[ti + 1]), tf);
				}
			} else if (initial) {
				joints->scale = initial[i].scale;
			} else {
				joints->scale = float3(one);
			}

			joints->weight		= 1;

		}
		return 1;
	}

	float	GetValue(an_data_nosize &d) {
		instance	*my(d);
		d = my->end_data;
		return my->length / 30.f;
	}

};

struct an_anim : an_base, ISO_ptr<AnimationRT> {
	void	Reserve(an_data_reserve &d, Object *obj)	{ return (*this)->Reserve(d, obj);	}
	void	Create(an_data_create &d, Object *obj)		{ return (*this)->Create(d, obj);	}
	void	Destroy(an_data_nosize &d)					{ return (*this)->Destroy(d);		}
	float	Evaluate(an_data_nosize &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		return (*this)->Evaluate(d, joints, initial, njoints, events, t);
	}
	float	GetValue(an_data_nosize &d, float t)		{ return (*this)->GetValue(d);		}
};

struct an_loop : an_base  {
	an_item	anim;

	struct instance {
		float	start;
		float	length;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		(void)my;
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		anim.Create(d, obj);
		my->length		= -1;
		my->start		= -1;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		if (my->length < 0) {
			an_data d1(d);
			if (my->length = anim.GetValue(d1, t))
				my->start = int(t / my->length) * my->length;
			else
				my->start = t;
		}
		if (my->length) {
			float loop_time = t - my->start;
			if (loop_time > my->length) {
				loop_time = mod(loop_time, my->length);
				// refresh length on loop
				an_data d1(d);
				if (my->length = anim.GetValue(d1, t))
					// chain on last
					my->start = t - loop_time;
				else
					my->start = t;
			} else if (loop_time < 0.0f) {
				// simple reverse loop only
				loop_time += my->length;
				my->start -= my->length;
			}
			return anim.Evaluate(d, joints, initial, njoints, events, loop_time);
		}
		an_data		d2	= d;
		if (float r = anim.Evaluate(d, joints, initial, njoints, events, t - my->start))
			return r;
		my->start = t;
		return anim.Evaluate(d2, joints, initial, njoints, events, 0);
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		return anim.GetValue(d, t);
	}
};

struct an_loopn : an_base  {
	an_item		anim;
	an_item		count;

	struct instance {
		float	start;
		float	length;
		int		count;
		int		loop;
		void	*end_data;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		anim.Reserve(d, obj);
		count.Reserve2(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		an_data		d2	= d;
		anim.Create(d, obj);
		count.Create(d, obj);
		my->start		= 0;
		my->length		= anim.GetValue(d2);
		my->count		= count.GetValue(d2);
		my->loop		= 0;
		my->end_data	= d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		anim.Destroy(d);
		count.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float		r;
		if (float length = my->length) {
			t -= length * min(int(t / length), my->count - 1);
			r = anim.Evaluate(d, joints, initial, njoints, events, t);
		} else {
			for (an_data d2 = d; r = anim.Evaluate(d, joints, initial, njoints, events, t - my->start); d = d2, my->start = t) {
				if (++my->loop >= my->count)
					break;
			}
		}
		d = my->end_data;
		return r;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		d = my->end_data;
		return my->length * my->count;
	}
};

struct an_once : an_base  {
	an_item		anim;
	an_item		on_end;

	struct instance {
		float	start;
		float	length;
		bool	done;
		void	*end_data;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		anim.Reserve(d, obj);
		on_end.Reserve2(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		an_data		d1(d);
		anim.Create(d, obj);
		on_end.Create2(d, obj);
		my->start		= -1.0f;
		my->length		= anim.GetValue(d1);
		my->done		= false;
		my->end_data	= d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		anim.Destroy(d);
		on_end.Destroy2(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		// start, loop
		if (my->start == -1.0f)
			my->start = t;
		else if (my->start == t)
			my->done = false;
		// evaluate
		float r;
		if (t - my->start < my->length)
			r = anim.Evaluate(d, joints, initial, njoints, events, t - my->start);
		else {
			// event
			r = anim.Evaluate(d, joints, initial, njoints, events, my->length);
			if (!my->done && on_end) {
				on_end.Evaluate(d, joints, initial, njoints, events, my->length);
				my->done = true;
			}
		}
		d = my->end_data;
		return r;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		d = my->end_data;
		return my->length;
	}
};

struct an_weight : an_base  {
	an_item	anim, value;

	struct instance {
		void	*end_data;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		value.Reserve2(d, obj);
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj)	{
		instance	*my = new(d) instance;
		value.Create2(d, obj);
		anim.Create(d, obj);
		my->end_data = d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		value.Destroy2(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		if (float c = value.GetValue(d, t))
			return c * anim.Evaluate(d, joints, initial, njoints, events, t);
		d = my->end_data;
		return 0;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		value.GetValue(d, t);
		return anim.GetValue(d, t);
	}
};

struct an_sum : an_base  {
	ISO_openarray<an_item>	anims;

	struct instance {
		float			length;
		void			*end;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		for (int i = 0, n = anims.Count(); i < n; i++)
			anims[i].Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj)	{
		instance	*my = new(d) instance;

		for (int i = 0, n = anims.Count(); i < n; i++) {
			an_data		d1(d);
			anims[i].Create(d, obj);
			my->length = max(my->length, anims[i].GetValue(d1));
		}
		my->end		= d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		for (int i = 0, n = anims.Count(); i < n; i++)
			anims[i].Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);

		float	f = anims[0].Evaluate(d, joints, initial, njoints, events, t);
		Joint	*joints2 = joint_sp; joint_sp += njoints;

		for (int i = 1, n = anims.Count(); i < n; i++) {
			if (float w = anims[i].Evaluate(d, joints2, NULL, njoints, events, t)) {
				for (int i = 0; i < njoints; i++) {
					if (float w2 = joints2[i].weight) {
						float	bw = w * w2;
						joints[i].scale = lerp(float3(joints[i].scale), float3(joints[i].scale * joints2[i].scale), bw);
						joints[i].rot	= slerp(joints[i].rot, joints2[i].rot * joints[i].rot, bw);
						joints[i].trans4 = joints[i].trans4 + joints2[i].trans4 * bw;
					}
				}
			}
		}

		joint_sp -= njoints;
		return 1;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		d = my->end;
		return my->length;
	}
};

struct an_blend : an_base  {
	an_item	left, right, blend;

	struct instance {
		void	*left_data;
		void	*end_data;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		blend.Reserve2(d, obj);
		right.Reserve(d, obj);
		left.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		blend.Create2(d, obj);
		right.Create(d, obj);
		my->left_data = d;
		left.Create(d, obj);
		my->end_data = d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		blend.Destroy2(d);
		right.Destroy(d);
		left.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float	b	= blend ? blend.GetValue(d, t) : 1;
		if (b == 0) {
			d = my->left_data;
			return left.Evaluate(d, joints, initial, njoints, events, t);
		}

		Events	eright;
		float	wright	= right.Evaluate(d, joints, initial, njoints, eright, t) * b;
		if (wright >= 1) {
			append(events, eright.begin(), eright.end());
			d = my->end_data;
			return 1;
		} else if (wright == 0) {
			return left.Evaluate(d, joints, initial, njoints, events, t);
		}
		Events	eleft;
		Joint	*joints2 = joint_sp; joint_sp += njoints;
		float	wleft	= left.Evaluate(d, joints2, initial, njoints, eleft, t) * (1 - wright);
		for (int i = 0; i < njoints; i++) {
			float	wr	= joints[i].weight * wright;
			float	wl	= joints2[i].weight	* wleft;
			float	w	= joints[i].weight = wl + wr;
			if (wl) {
				float	bw = wl / w;
				joints[i].scale = lerp(joints[i].scale, joints2[i].scale, bw);
				joints[i].rot	= slerp(joints[i].rot, joints2[i].rot, bw);
				joints[i].trans4 = lerp(joints[i].trans4, joints2[i].trans4, bw);
			}
		}
		joint_sp -= njoints;

		// events
		if (wleft > wright)
			append(events, eleft.begin(), eleft.end());
		else
			append(events, eright.begin(), eright.end());

		return wleft + wright;
	}
	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		if (blend)
			blend.GetValue(d, t);
		float	r = right.GetValue(d, t);
		float	l = left.GetValue(d, t);
		return max(l, r);
	}
};

struct an_add : an_base  {
	an_item	left, right, blend;

	struct instance {
		void	*end_data;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		blend.Reserve2(d, obj);
		left.Reserve(d, obj);
		right.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		blend.Create2(d, obj);
		left.Create(d, obj);
		right.Create(d, obj);
		my->end_data = d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		blend.Destroy2(d);
		left.Destroy(d);
		right.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float		b = blend ? blend.GetValue(d, t) : 1;
		left.Evaluate(d, joints, initial, njoints, events, t);
		if (b) {
			Joint	*joints2 = joint_sp; joint_sp += njoints;
			b *= right.Evaluate(d, joints2, NULL, njoints, events, t);
			for (int i = 0; i < njoints; i++) {
				if (float w = joints2[i].weight) {
					float	bw = b * w;
					joints[i].scale = lerp(float3(joints[i].scale), float3(joints[i].scale * joints2[i].scale), bw);
					joints[i].rot	= slerp(joints[i].rot, joints2[i].rot * joints[i].rot, bw);
					joints[i].trans4 = joints[i].trans4 + joints2[i].trans4 * bw;
				}
			}
			joint_sp -= njoints;
		} else {
			d = my->end_data;
		}
		return 1;
	}
	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		if (blend)
			blend.GetValue(d, t);
		float	a = left.GetValue(d, t);
		float	b = right.GetValue(d, t);
		return max(a, b);
	}
};

struct an_time : an_base  {
	an_item	anim, value;

	struct instance {
		float			length;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance *my(d);
		value.Reserve2(d, obj);
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		value.Create(d, obj);
		an_data d1(d);
		anim.Create(d, obj);
		my->length	= anim.GetValue(d1);
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		value.Destroy(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float	c = value.GetValue(d, t);
		return anim.Evaluate(d, joints, initial, njoints, events, c * my->length);
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		return my->length;
	}
};

struct an_delay : an_base  {
	an_item		before;
	an_item		after;
	an_item		anim;

	struct instance {
		float	start;
		float	delay;
		void	*end_data;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		anim.Reserve(d, obj);
		before.Reserve2(d, obj);
		after.Reserve2(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		anim.Create(d, obj);
		an_data		d1(d);
		before.Create(d, obj);
		after.Create(d, obj);
		my->start		= -1.0f;
		my->delay		= before.GetValue(d1);
		my->end_data	= d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		anim.Destroy(d);
		before.Destroy(d);
		after.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		// start
		if (my->start == -1.0f)
			my->start = t + my->delay;
		// delay
		float r = t > my->start ? anim.Evaluate(d, joints, initial, njoints, events, t - my->start) : 0.0f;
		d = my->end_data;
		return r;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		float length = anim.GetValue(d, t);
		float delay_before = before.GetValue(d, t);
		float delay_after = after.GetValue(d, t);
		// restart on length change
		if (my->delay != delay_before) {
			my->delay = delay_before;
			my->start = -1.0f;
		}
		return length + delay_before + delay_after;
	}
};

struct an_rewind : an_base  {
	an_item		anim;

	struct instance {
		float	start;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		anim.Create(d, obj);
		my->start = -1.0f;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		if (my->start == -1.0f)
			my->start = t;
		return anim.Evaluate(d, joints, initial, njoints, events, t - my->start);
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		return anim.GetValue(d, t);
	}
};

struct an_speed : an_base  {
	an_item	anim, value;

	struct instance {
		float	prev_time, time;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance *my(d);
		value.Reserve2(d, obj);
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		my->prev_time = my->time = 0;
		value.Create(d, obj);
		anim.Create(d, obj);
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		value.Destroy(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance *my(d);
		float	c = value.GetValue(d, t);
		my->time += c * (t - my->prev_time);
		my->prev_time = t;
		return anim.Evaluate(d, joints, initial, njoints, events, my->time);
	}

	float	GetValue(an_data &d, float t) {
		instance *my(d);
		value.GetValue(d, t);
		return	anim.GetValue(d, t);
	}
};

struct an_timeclamp : an_base {
	an_item	anim, value;

	struct instance {
		float			prevt;
		float			blend;
		float			pos;
		float			length;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance *my(d);
		value.Reserve2(d, obj);
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		value.Create(d, obj);
		an_data d1(d);
		anim.Create(d, obj);
		my->length	= anim.GetValue(d1);
		my->prevt	= 0;
		my->pos		= 0;
		my->blend	= 1;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		value.Destroy(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float	c = value.GetValue(d, t);
		float	dt	= t - my->prevt;
		my->prevt = t;

		if (c < 0.01f)
			c = 0;

		float	v	= my->blend * my->pos;
		if (c < v) {
			v = max(v - 2 * dt, c);
			if (v == 0) {
				my->pos		= 0;
				my->blend	= 1;
			} else {
				my->blend	= v / my->pos;
			}
		} else if (c > v) {
			v = min(v + dt, c);
			my->blend = v / my->pos;
			if (my->blend > 1) {
				my->blend	= 1;
				my->pos		= v;
			}
		}
		return anim.Evaluate(d, joints, initial, njoints, events, my->pos * my->length) * my->blend;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		value.GetValue(d, t);
		return	anim.GetValue(d, t);
	}

};

struct an_blendrelease : an_base  {
	an_item	anim, value;

	struct instance {
		float			prev;
		float			length;
		float			start_time;
		float			blend;
		void			*end_data;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance *my(d);
		value.Reserve2(d, obj);
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		value.Create(d, obj);
		an_data d1(d);
		anim.Create(d, obj);
		my->length		= anim.GetValue(d1);
		my->prev		= 0;
		my->start_time	= -my->length;
		my->end_data	= d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		value.Destroy(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float	v	= min(value.GetValue(d, t), one);
		float	t2	= t - my->start_time;

		if (t2 < my->length) {
			return anim.Evaluate(d, joints, initial, njoints, events, t2) * my->blend;
		}
#if 1
		// In progress only used to trigger land anim right now,
		// needs to hold the first frame of the anim when you leave the ground.
		if (v > .2f && my->prev < .1f) {
			my->start_time	= t;
			my->prev		=
			my->blend		= v;
		} else {
			my->prev		= v;
		}
#else
		if (v < 0 && my->prev > 0.1f) {
			my->start_time = t;
			my->prev		= 0;
		} else {
			if (v > my->prev)
				my->prev = v;
			my->blend		= my->prev;
		}
		if (my->blend) {
			my->prev = (my->blend *= .95f);
			if (my->blend < 0.1f)
				my->prev = my->blend = 0.0f;
			else
				return anim.Evaluate(d, joints, initial, njoints, events, 0) * my->blend;
		}
#endif
		d = my->end_data;
		return  0;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		value.GetValue(d, t);
		return	anim.GetValue(d, t);
	}
};

struct an_fadeinout : an_base  {
	float	fadein;
	float	fadeout;
	an_item	anim;

	struct instance {
		float			start_time;
		float			length;
		instance() : length(0)	{}
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		anim.Reserve(d, obj);
	}
	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		anim.Create(d, obj);
	}
	void	Destroy(an_data &d)	{
		instance	*my(d);
		anim.Destroy(d);
	}
	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		an_data		d1	= d;
		float		w = anim.Evaluate(d, joints, initial, njoints, events, t);
		if (w) {
			if (!my->length) {
				my->start_time	= t;
				my->length		= anim.GetValue(d1, t);
			}
			t -= my->start_time;
			if (t >= my->length)
				my->length = 0;
			else if (t < fadein)
				w *= t / fadein;
			else if (t > my->length - fadeout)
				w *= (my->length - t) / fadeout;
		}
		return w;
	}
	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		return anim.GetValue(d, t);
	}
};

struct an_switch : an_base  {
	an_item					value;
	ISO_openarray<an_item>	anims;

	struct instance : trailing_array<instance, pair<void*, float> > {
		float			prev;
		float			length;
		float			start_time;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		int	n = anims.Count();
		d.allocp<instance>(n);
		value.Reserve2(d, obj);
		for (int i = 0; i < n; i++) {
			if (anims[i])
				anims[i].Reserve(d, obj);
		}
	}

	void	Create(an_data_create &d, Object *obj) {
		int			n	= anims.Count();
		instance	*my = new(n, d) instance;
//		instance	*my = new(d.createn<instance>(n)) instance;
//		instance	*my = new(d.create(instance::size(n))) instance;

		value.Create(d, obj);
		my->prev		= -1;
		my->length		= 0;
		my->start_time	= 0;

		for (int i = 0; i < n; i++) {
			if (anims[i]) {
				an_data		d1(d);
				anims[i].Create(d, obj);
				(*my)[i].b	= anims[i].GetValue(d1);
				my->length		= max(my->length, (*my)[i].b);
			}
			(*my)[i].a = d;
		}
	}

	void	Destroy(an_data &d)	{
		int			n	= anims.Count();
		instance	*my = d.allocp<instance>(n);
		value.Destroy(d);
		for (int i = 0; i < n; i++)
			anims[i].Destroy2(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		int			n	= anims.Count();
		instance	*my = d.allocp<instance>(n);

		int	i = int(value.GetValue(d, t)) % n;
		if (i < 0)
			i += n;
		if (i != my->prev) {
			my->prev		= i;
			my->start_time	= t;
		}

		float	r = 0;
		if (anims[i]) {
			an_data	d1(i == 0 ? (void*)d : (*my)[i - 1].a);
			r = anims[i].Evaluate(d1, joints, initial, njoints, events, t - my->start_time);
		}
		d = (*my)[n - 1].a;
		return r;
	}

	float	GetValue(an_data_nosize &d, float t) {
		int			n	= anims.Count();
		instance	*my(d);
		d = (*my)[n - 1].a;
		return my->length;
	}
};

struct an_blend_switch : an_base  {
	an_item					value;
	an_item					blend_time;
	ISO_openarray<an_item>	anims;

	struct instance : trailing_array<instance, void*> {
		Object	*obj;
		float	current;
		an_item	current_anim, prev_anim;
		void	*current_data, *prev_data;
		float	time;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		int			n	= anims.Count();
		instance	*my = d.allocp<instance>(n + 1);

		value.Reserve2(d, obj);
		blend_time.Reserve2(d, obj);
		for (int i = 0; i < n; i++)
			anims[i].Reserve2(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		int			n	= anims.Count();
//		instance	*my = new(d.create(instance::size(n + 1))) instance;
//		instance	*my = new(d.createn<instance>(n + 1)) instance;
		instance	*my = new(n + 1, d) instance;

		value.Create(d, obj);
		blend_time.Create(d, obj);
		my->obj		= obj;
		my->current	= -1;
		my->time	= 0;

		for (int i = 0; i < n; i++) {
			(*my)[i] = d;
			anims[i].Reserve2(d, obj);
		}
		(*my)[n] = d;
	}

	void	Destroy(an_data &d)	{
		int			n	= anims.Count();
		instance	*my = d.allocp<instance>(n + 1);

		value.Destroy(d);
		blend_time.Destroy(d);
		if (my->current_anim) {
			an_data	d1(my->current_data);
			my->current_anim.Destroy2(d1);
			my->current_anim.Clear();
		}
		if (my->prev_anim) {
			an_data	d2(my->prev_data);
			my->prev_anim.Destroy2(d2);
			my->prev_anim.Clear();
		}
		d = (*my)[n];
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		int			n	= anims.Count();
		instance	*my = d.allocp<instance>(n + 1);

		// state change
		if (!my->prev_anim) {
			float current = value.GetValue(d, t);
			if (current != my->current) {
				// lookup
				int index;
				if (!current)
					index = 0;
				else if ((index = anims.GetIndex(tag2(*(crc32*)&current))) == -1) {
					index = unsigned(current) % n;
					ISO_ASSERT(!(my->current_anim && my->current_anim == anims[index]));
				}
				// shuffle
				my->prev_anim		= my->current_anim;
				my->prev_data		= my->current_data;
				my->current_anim	= anims[index];
				my->current_data	= (*my)[index];
				// create
				an_data_create	d1(my->current_data, (*my)[index + 1]);
				my->current_anim.Create2(d1, my->obj);
				// value, time
				my->current	= current;
				my->time	= t;
			}
		}

		// evaluate
		float r = 0;
		float blend_interval = blend_time.GetValue(d, t);
		if (!my->prev_anim) {
			// current
			an_data	d1(my->current_data);
			r = my->current_anim ? my->current_anim.Evaluate(d1, joints, initial, njoints, events, t) : 0.0f;

		} else {
			float interval = t - my->time;
			if (interval < blend_interval) {
				if (my->current_anim && my->prev_anim) {
					// current
					an_data	d1(my->current_data);
					float wcurrent = my->current_anim.Evaluate(d1, joints, initial, njoints, events, t) * (interval / blend_interval);
					// previous
					Joint *joints2 = joint_sp;
					joint_sp += njoints;
					an_data	d2(my->prev_data);
					float wprev = my->prev_anim.Evaluate(d2, joints2, initial, njoints, events, t) * (1.0f - wcurrent);
					// blend
					for (int i = 0; i < njoints; i++) {
						float	wc	= joints[i].weight * wcurrent;
						float	wp	= joints2[i].weight	* wprev;
						float	w	= joints[i].weight = wc + wp;
						if (wp) {
							float	bw = wp / w;
							joints[i].scale = lerp(joints[i].scale, joints2[i].scale, bw);
							joints[i].rot	= slerp(joints[i].rot, joints2[i].rot, bw);
							joints[i].trans4 = lerp(joints[i].trans4, joints2[i].trans4, bw);
						}
					}
					joint_sp -= njoints;
					r = wcurrent + wprev;

				} else if (my->current_anim) {
					// current fade-in
					an_data	d1(my->current_data);
					r = my->current_anim.Evaluate(d1, joints, initial, njoints, events, t) * (interval / blend_interval);

				} else {
					// previous fade-out
					an_data	d2(my->prev_data);
					r = my->prev_anim.Evaluate(d2, joints, initial, njoints, events, t) * (1.0f - interval / blend_interval);
				}

			} else {
				// destroy
				an_data	d2(my->prev_data);
				my->prev_anim.Destroy2(d2);
				my->prev_anim.Clear();
				// current
				an_data	d1(my->current_data);
				r = my->current_anim ? my->current_anim.Evaluate(d1, joints, initial, njoints, events, t) : 0.0f;
			}
		}
		d = (*my)[n];
		return r;
	}

	float	GetValue(an_data &d, float t) {
		int			n	= anims.Count();
		instance	*my = d.allocp<instance>(n + 1);

		float length;
		if (my->current_anim) {
			an_data	d1(my->current_data);
			length = my->current_anim.GetValue(d1, t);
		} else
			length = 0.0f;

		d = (*my)[anims.Count()];
		return length;
	}
};

struct an_static_switch : an_base  {
	an_item					value;
	ISO_openarray<an_item>	anims;

	struct instance {
		int				index;
		void			*end_data;
	};

	void	_Reserve(an_data_reserve &d, Object *obj) {
		an_data_reserve		d0(d);
		value.Reserve(d0, obj);

		void				*end = d0;
		for (int i = 0, n = anims.Count(); i < n; i++) {
			if (anims[i]) {
				d0	= d;
				anims[i].Reserve(d0, obj);
				if (d0 > end)
					end = d0;
			}
		}
		d = end;
	}

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance			*my(d);
		_Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj)	{
		instance	*my = new(d) instance;

		an_data_reserve r0(d);
		_Reserve(r0, obj);
		my->end_data = r0;

		an_data		d0(d);
		value.Create(d, obj);
		((an_data&)d)	= d0;
		float	v = value.GetValue(d);
		((an_data&)d)	= d0;
		value.Destroy(d);

		int		n	= anims.Count();
		int		i	= int(v) % n;
		if (i < 0)
			i += n;
		my->index = i;
		if (anims[i])
			anims[i].Create(d, obj);
		((an_data&)d) = my->end_data;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		if (anims[my->index])
			anims[my->index].Destroy(d);
		d = my->end_data;
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float r = anims[my->index] ? anims[my->index].Evaluate(d, joints, initial, njoints, events, t) : 0;
		d = my->end_data;
		return r;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		float r = anims[my->index] ? anims[my->index].GetValue(d, t) : 0;
		d = my->end_data;
		return r;
	}
};

struct an_if : an_base  {
	an_item		value;
	an_item		anim;

	struct instance {
		float	start_time;
		float	value;
		void	*end_data;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		value.Reserve2(d, obj);
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance;
		value.Create(d, obj);
		anim.Create(d, obj);
		my->start_time = 0.0f;
		my->value = 0.0f;
		my->end_data = d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		value.Destroy(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float v = value.GetValue(d, t);
		if (v != my->value) {
			my->value = v;
			my->start_time = t;
		}
		if (!v) {
			d = my->end_data;
			return 0;
		}
		return anim.Evaluate(d, joints, initial, njoints, events, t - my->start_time);
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		value.GetValue(d, t);
		return anim.GetValue(d, t);
	}
};

struct an_trigger : an_base, ISO_openarray<pair<crc32, an_item> > {
	struct instance : trailing_array<instance, pair<void*, float> > {
		const an_trigger *a;
		int				index;
		float			start_time;
		float			length;

		void operator()(TriggerMessage &m) {
			for (int i = 0, n = a->Count(); i < n; i++) {
				if (m.dispatch_id == (*a)[i].a) {
					index		= i;
					start_time	= 0;
					length		= (*this)[i].b;
					break;
				}
			}
		}
		instance(Object *obj, const an_trigger *_a) : a(_a), index(-1), start_time(0), length(0) {
			obj->Root()->SetHandler<TriggerMessage>(this);
		}
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		int			n = Count();
		instance	*my = d.allocp<instance>(n);
		for (int i = 0; i < n; i++)
			(*this)[i].b.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		int			n	= Count();
//		instance	*my = new(d.create(instance::size(n))) instance(obj, this);
//		instance	*my = new(d.createn<instance>(n)) instance(obj, this);
		instance	*my = new(n, d) instance(obj, this);

		for (int i = 0; i < n; i++) {
			an_data		d1(d);
			(*this)[i].b.Create(d, obj);
			(*my)[i].b	= (*this)[i].b.GetValue(d1);
			(*my)[i].a	= d;
		}
	}

	void	Destroy(an_data &d)	{
		int			n	= Count();
		instance	*my = d.allocp<instance>(n);
		my->~instance();
		for (int i = 0; i < n; i++)
			(*this)[i].b.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		int			n	= Count();
		instance	*my = d.allocp<instance>(n);
		float		r	= 0;
		int			i	= my->index;
		if (i >= 0) {
			if (my->start_time == 0)
				my->start_time = t;
			if (my->length <= 0.0f || t - my->start_time < my->length) {
				an_data	d1(i == 0 ? (void*)d : (*my)[i - 1].a);
				r = (*this)[my->index].b.Evaluate(d1, joints, initial, njoints, events, t - my->start_time);
			} else {
				my->length	= 0;
				my->index	= -1;
			}
		}
		d = (*my)[n - 1].a;
		return r;
	}

	float	GetValue(an_data_nosize &d, float t) {
		int			n	= Count();
		instance	*my = d.allocp<instance>(n);
		d = (*my)[n - 1].a;
		return my->length;
	}
};

struct an_chain : an_base  {
	an_item		anim;
	an_item		next;

	struct instance {
		int		stage;
		float	start_time;
		void	*next_data;
		void	*end_data;
		instance() : stage(-1), start_time(0) {}
	};

	void	Reserve(an_data_reserve &d, Object *obj)	{
		instance	*my(d);
		anim.Reserve(d, obj);
		next.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj)	{
		instance	*my = new(d) instance;
		anim.Create(d, obj);
		my->next_data = d;
		next.Create(d, obj);
		my->end_data = d;
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		anim.Destroy(d);
		next.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		float		r;
		if (my->stage < 1) {
			if (r = anim.Evaluate(d, joints, initial, njoints, events, t)) {
				if (my->stage == -1)
					my->stage = 0;
			} else if (my->stage == 0) {
				an_data d1(my->next_data);
				r = next.Evaluate(d1, joints, initial, njoints, events, 0.0f);
				my->start_time = t;
				my->stage = 1;
			}
		} else {
			an_data d1(my->next_data);
			if (!(r = t < my->start_time ? 0 : next.Evaluate(d1, joints, initial, njoints, events, t - my->start_time)))
				my->stage = -1;
		}
		d = my->end_data;
		return r;
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		float	v = anim.GetValue(d, t);
		return	v + next.GetValue(d, t);
	}
};

struct an_checkstate : an_base  {
	ISO::ptr_string<char,32>	id;
	an_item		anim;

	struct instance {
		Object	*obj;
		crc32	id;
		int		state;
		float	set(float f) {
			if (state == 0) {
				if (f > 0)
					obj->Send(TriggerMessage(obj, id, state = 1));
			} else {
				if (f == 0)
					obj->Send(TriggerMessage(obj, id, state = 0));
			}
			return f;
		}
		instance(Object *_obj, crc32 _id) : obj(_obj), id(_id), state(0)	{}
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		anim.Reserve(d, obj);
	}

	void	Create(an_data_create &d, Object *obj) {
		instance	*my = new(d) instance(obj->Root(), crc32((const char*)id));
		anim.Create(d, obj);
	}

	void	Destroy(an_data &d)	{
		instance	*my(d);
		anim.Destroy(d);
	}

	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		return		my->set(anim.Evaluate(d, joints, initial, njoints, events, t));
	}

	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		return anim.GetValue(d, t);
	}
};

//-----------------------------------------------------------------------------

template<typename T> struct fn_id_type : an_base {
	ISO::ptr_string<char,32>	id;

	struct instance {
		T	*value;
	};

	void	Reserve(instance *my, Object *obj)	{}
	void	Create(an_data_create &d, Object *obj)	{
		instance	*my = new(d) instance;
		LookupMessage	m = crc32(id);
		do {
			obj->Send(m);
		} while (!m.result && (obj = obj->Parent()));
		my->value = (T*)m.result;
	}
	void	Destroy(instance *my)				{}
	float	GetValue(instance *my, float t)		{ return my->value ? *my->value : 0; }
};
typedef fn_id_type<float>	fn_id_float;
typedef fn_id_type<int>		fn_id_int;

template<> float fn_id_type<crc32>::GetValue(instance *my, float t)	{ return my->value ? *(float*)my->value : 0; }
typedef fn_id_type<crc32>	fn_id_crc;

struct fn_float : an_base {
	float	f;
	float	GetValue(an_data &d, float t)		{ return f;	}
};

struct fn_int : an_base {
	int		i;
	float	GetValue(an_data &d, float t)		{ return i;	}
};

struct fn_crc : an_base {
	ISO::ptr_string<char,32>	s;

	struct instance {
		crc32	value;
	};

	void	Reserve(instance *my, Object *obj)	{}
	void	Create(an_data_create &d, Object *obj) {
		instance *my = new(d) instance;
		my->value = crc32(s);
	}
	void	Destroy(instance *my)				{}
	float	GetValue(instance *my, float t)		{ return 0; }//*(float*)&my->value;	}
};

struct fn_static_rand : an_base {
	float	to;

	struct instance {
		float	value;
	};

	void	Reserve(instance *my, Object *obj)	{}
	void	Create(an_data_create &d, Object *obj) {
		instance *my = new(d) instance;
		my->value = iso::random.to(to);
	}
	void	Destroy(instance *my)				{}
	float	GetValue(instance *my, float t)		{ return my->value;	}
};

struct fn_rand : an_base {
	float	to;
	float	GetValue(an_data &d, float t)		{ return iso::random.to(to); }
};

struct fn_unary : an_base {
	an_item	x;

	void	Reserve(an_data_reserve &d, Object *obj)	{ x.Reserve2(d, obj);	}
	void	Create(an_data_create &d, Object *obj)		{ x.Create(d, obj);		}
	void	Destroy(an_data &d)							{ x.Destroy(d);			}
};

struct fn_binary : an_base {
	an_item	x, y;

	void	Reserve(an_data_reserve &d, Object *obj)	{ x.Reserve2(d, obj); y.Reserve2(d, obj);}
	void	Create(an_data_create &d, Object *obj)		{ x.Create(d, obj); y.Create(d, obj);	}
	void	Destroy(an_data &d)							{ x.Destroy(d); y.Destroy(d);			}
};

struct fn_ternary : an_base {
	an_item	x, y, z;

	void	Reserve(an_data_reserve &d, Object *obj)	{ x.Reserve2(d, obj); y.Reserve2(d, obj); z.Reserve2(d, obj);	}
	void	Create(an_data_create &d, Object *obj)		{ x.Create(d, obj); y.Create(d, obj); z.Create(d, obj);			}
	void	Destroy(an_data &d)							{ x.Destroy(d); y.Destroy(d); z.Destroy(d);						}
};

struct fn_clamp	: fn_unary { float	GetValue(an_data &d, float t) { return clamp(x.GetValue(d, t), 0.f, 1.f); } };
struct fn_add	: fn_binary { float GetValue(an_data &d, float t) { float a = x.GetValue(d, t), b = y.GetValue(d, t); return a + b; } };
struct fn_sub	: fn_binary { float GetValue(an_data &d, float t) { float a = x.GetValue(d, t), b = y.GetValue(d, t); return a - b; } };
struct fn_mul	: fn_binary { float GetValue(an_data &d, float t) { float a = x.GetValue(d, t), b = y.GetValue(d, t); return a * b; } };
struct fn_div	: fn_binary { float GetValue(an_data &d, float t) { float a = x.GetValue(d, t), b = y.GetValue(d, t); return a / b; } };

struct fn_filter : an_base {
	an_item	x;
	float	y;
	struct instance {
		float	previous;
	};

	void	Reserve(an_data_reserve &d, Object *obj)	{ instance	*my = d; x.Reserve2(d, obj); }
	void	Create(an_data_create &d, Object *obj)		{ instance	*my = new(d) instance; an_data d1(d); x.Create(d, obj); my->previous = x.GetValue(d1);	}
	void	Destroy(an_data &d)							{ instance	*my = d; x.Destroy(d);		}

	float GetValue(an_data &d, float t) {
		instance	*my = d;
		return my->previous = lerp(my->previous, x.GetValue(d, t), y);
	}
};

struct fn_ramp : an_base {
	float	span;
	float	GetValue(an_data &d, float t)		{ return min(t / span, 1.0f);	}
};

struct fn_clear : fn_id_float {
	float	Evaluate(instance *my, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		return *my->value = 0;
	}
};

struct fn_clear_bits : fn_id_int {
	uint32 mask;
	float	Evaluate(instance *my, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		*my->value &= ~mask;
		return 0;
	}
};

struct fn_set : an_base {
	ISO::ptr_string<char,32>		id;
	an_item			value;

	struct instance {
		float	*lvalue;
	};

	void	Reserve(an_data_reserve &d, Object *obj) {
		instance	*my(d);
		value.Reserve2(d, obj);
	}
	void	Create(an_data_create &d, Object *obj)	{
		instance	*my = new(d) instance;
		value.Create(d, obj);

		LookupMessage m	= crc32(id);
		obj->Send(m);
		ISO_VERIFY(my->lvalue = (float*)m.result);
	}
	void	Destroy(an_data &d) {
		instance	*my(d);
		value.Destroy(d);
	}
	float	Evaluate(an_data &d, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		instance	*my(d);
		*my->lvalue = value.GetValue(d, t);
		return 0;
	}
	float	GetValue(an_data &d, float t) {
		instance	*my(d);
		return *my->lvalue;
	}
};

struct fn_sendtrigger : an_base  {
	ISO::ptr_string<char,32>		id;

	struct instance {
		Object	*obj;
		instance(Object *_obj) : obj(_obj)	{}
	};

	void	Reserve(instance *my, Object *obj)	{}

	void	Create(an_data_create &d, Object *obj)	{
		instance	*my = new(d) instance(obj->Root());
	}

	float	Evaluate(instance *my, Joint *joints, Joint *initial, int njoints, Events &events, float t) {
		my->obj->Send(TriggerMessage(my->obj, crc32((const char*)id)));
		return 0;
	}
};

#define AN_TYPE(T)	an_template<T>	def_##T(#T)
AN_TYPE(an_library);
AN_TYPE(an_fromlib);
AN_TYPE(an_anim);
AN_TYPE(an_loop);
AN_TYPE(an_loopn);
AN_TYPE(an_once);
AN_TYPE(an_chain);
AN_TYPE(an_weight);
AN_TYPE(an_sum);
AN_TYPE(an_blend);
AN_TYPE(an_add);
AN_TYPE(an_time);
AN_TYPE(an_rewind);
AN_TYPE(an_delay);
AN_TYPE(an_speed);
AN_TYPE(an_timeclamp);
AN_TYPE(an_blendrelease);
AN_TYPE(an_fadeinout);
AN_TYPE(an_switch);
AN_TYPE(an_blend_switch);
AN_TYPE(an_static_switch);
AN_TYPE(an_if);
AN_TYPE(an_trigger);

AN_TYPE(fn_id_float);
AN_TYPE(fn_id_int);
AN_TYPE(fn_id_crc);
AN_TYPE(fn_float);
AN_TYPE(fn_int);
AN_TYPE(fn_crc);
AN_TYPE(fn_rand);
AN_TYPE(fn_static_rand);
AN_TYPE(fn_clamp);
AN_TYPE(fn_ramp);
AN_TYPE(fn_add);
AN_TYPE(fn_sub);
AN_TYPE(fn_mul);
AN_TYPE(fn_div);
AN_TYPE(fn_filter);
AN_TYPE(fn_clear);
AN_TYPE(fn_clear_bits);
AN_TYPE(fn_set);

void an_item::Reserve2(an_data_reserve &d, Object *obj)	{
	if (*this) {
		switch (TypeType(GetType())) {
			case ISO::FLOAT:
				Header()->type = &def_fn_float;
				break;

			case ISO::INT:
				Header()->type = &def_fn_int;
				break;

			case ISO::STRING: {
				LookupMessage msg(crc32((const char*)GetType()->ReadPtr(get())));
				if (obj->Send(msg) && msg.result) {
					if (msg.type->Is(ISO_CRC("crc32", 0xafabd35e)))
						Header()->type = &def_fn_id_crc;
					else if (msg.type->GetType() == ISO::INT)
						Header()->type = &def_fn_id_int;
					else
						Header()->type = &def_fn_id_float;
				} else {
					Header()->type = &def_fn_crc;
				}
				break;
			}
		}
		Reserve(d, obj);
	}
}

void an_item::Create2(an_data_create &d, Object *obj) {
	if (*this)
		Create(d, obj);
}

void an_item::Destroy2(an_data &d) {
	if (*this)
		Destroy(d);
}
//-----------------------------------------------------------------------------

float AnimationHolder::Evaluate(Pose *pose, float time) {
	an_data	d1(data);

	int		njoints = pose->Count();
	Joint	*joints	= joint_stack;
	Events	events;

	an_data		d(data);
	float		r = ((AnimationRT*)anim)->Evaluate(d, joints, pose->joints, njoints, events, time);
	if (r) {
		for (int i = 0; i < njoints; i++) {
			if (joints[i].weight)
				pose->mats[i] = joints[i];
		}
		pose->Update();
	}
	if (!events.empty()) {
		for (auto &i : events)
			obj->AddEntities(i);
	}
	return r;
}

void AnimationHolder::Init() {
	AnimationRT	*t	= (AnimationRT*)anim;
	an_data		d(0);
	t->Reserve(d, obj);
	size_t	size	= (size_t)(void*)d;
	an_data_create	d2(data = malloc(size), uint32(size));
	t->Create(d2, obj);
	an_data_nosize	d3(data);
	length			= t->GetValue(d3);
}

void AnimationHolder::SetAnim(ISO_ptr<Animation> t) {
	free(data);
	anim	= t;
	Init();
}

AnimationHolder::~AnimationHolder() {
	an_data		d(data);
	((AnimationRT*)anim)->Destroy(d);
	free(data);
}

struct AnimationHandlerBones : DeleteOnDestroy<AnimationHandlerBones>, HandlesWorld<AnimationHandlerBones, FrameEvent2>, AnimationHolder {
	float				time;
	float				speed;
	bool				idle;

	void	Evaluate(float time) {
		AnimationHolder::Evaluate(obj->Property<Pose>(), time);
		if (speed == 0.0f)
			idle = true;
	}

	void	operator()(AnimationSetMessage &m) {
		if (!m.id || m.id == anim.ID().get_crc32()) {
			if ((speed = m.speed) != 0)
				idle = false;
			if (m.value >= 0) {
				time = m.value;
				idle = false;
			}
			m.handled = true;
		} else {
			speed	= 0;
			idle	= true;
		}
	}

	void	operator()(PostCreateMessage &m) {
		DelegateArray<PostCreateMessage>::Remove(obj, this);
		if (obj->Property<Pose>()) {
			Init();
			if (!idle)
				Evaluate(time);
		} else {
			delete this;
		}
	}

	void	operator()(FrameEvent2 &ev) {
		PROFILE_CPU_EVENT("AnimBones");
		if (idle)
			return;

		an_data	d1(data);
		float speed_dt	= ev.dt * speed;
		float time_next	= time + speed_dt;
		if (time_next > length) {
			float d = length - time;
			obj->Send(AnimationLoopMessage(d));
			if (speed == 0.0f)
				speed_dt = time == 0.0f ? 0.0f : d;
			else
				speed_dt -= length;

		} else if (time_next < 0.0f) {
			float d = -time;
			if (time != 0.0f)
				obj->Send(AnimationLoopMessage(d));
			if (speed == 0.0f)
				speed_dt = time == 0.0f ? 0.0f : d;
			else
				speed_dt += length;

		}
		Evaluate(time += speed_dt);
	}

	AnimationHandlerBones(Object *_obj, ISO_ptr<Animation> t)
		: DeleteOnDestroy<AnimationHandlerBones>(_obj)
		, AnimationHolder(_obj)
		, time(0), speed(1), idle(false)
	{
		anim = t;
		_obj->SetHandler<AnimationSetMessage>(this);
		_obj->AddHandler<PostCreateMessage>(this);
	}
};

struct AnimationHandlerBonesHierarchy : DeleteOnDestroy<AnimationHandlerBonesHierarchy>, HandlesWorld<AnimationHandlerBonesHierarchy, FrameEvent2> {
	an_item				anim;
	Object				*obj;
	malloc_block		data;
	float				time;

	void	operator()(FrameEvent2 &ev) {
		PROFILE_CPU_EVENT("AnimBonesHier");

		time	+= ev.dt;
		Pose	*pose	= obj->Property<Pose>();
		int		njoints = pose->Count();
		Joint	*joints	= joint_stack;
		joint_sp = joints + njoints;
		Events	events;

		an_data		d(data);
		if (anim.Evaluate(d, joints, pose->joints, njoints, events, time)) {
			for (int i = 0; i < njoints; i++) {
				if (joints[i].weight)
					pose->mats[i] = joints[i];
			}
			pose->Update();
		}
		if (!events.empty()) {
			for (auto &i : events)
				obj->AddEntities(i);
		}
	}

	void	operator()(PostCreateMessage &m) {
		DelegateArray<PostCreateMessage>::Remove(obj, this);
		an_data			d(0);
		anim.Reserve(d, obj);
		size_t	size = (size_t)(void*)d;
		an_data_create	d2(data.create(size), uint32(size));
		anim.Create(d2, obj);
	}

	AnimationHandlerBonesHierarchy(Object *_obj, const AnimationHierarchy *t)
		: DeleteOnDestroy<AnimationHandlerBonesHierarchy>(_obj)
		, anim((an_item&)t->root), obj(_obj), time(0)
	{
		_obj->AddHandler<PostCreateMessage>(this);
	}

	~AnimationHandlerBonesHierarchy() {
		an_data		d(data);
		anim.Destroy(d);
	}
};

//-----------------------------------------------------------------------------
//	Simple Animation
//-----------------------------------------------------------------------------

struct AnimationHandler : DeleteOnDestroy<AnimationHandler>, HandlesWorld<AnimationHandler, FrameEvent2> {
	Object				*obj;

	const ISO_ptr<Animation> anim;
	float					loop_threshold;
	float					start_time, prev_time;
	float					speed;
	bool8					idle, set_time;
	uint32					num_frames;
	uint32					flags;
	float4p					*rot;
	compressed_quaternion	*comp_rot;
	float3p					*pos;
	float3p					*scl;
	float					*cuts;
	uint32					num_cuts, cut_index;
	AnimationEvents::iterator event_iter, event_iter_end, event_iter_rend;
	int8					event_dir;
	struct Extra {int type; void *dest; void *array; };
	dynamic_array<Extra>	extras;

	float	CalcTime(float world_time) {
		return world_time * speed - start_time;
	}

	void	operator()(PostCreateMessage &m) {
		DelegateArray<PostCreateMessage>::Remove(obj, this);
		ISO::Browser	b(anim);
		for (int i = 0, n = b.Count(); i < n; i++) {
			ISO::Browser	b2	= b[i];
			uint32		id	= b.GetName(i).get_crc32();

			if (id == ISO_CRC("events", 0x5387574a)) {
				AnimationEvents *events = (*anim)[i];
				event_iter = events->begin();
				event_iter_end = events->end();
				--(event_iter_rend = events->begin());
				event_dir = 0;

			} else if (id == ISO_CRC("rot", 0x1d39a761)) {
				rot			= *(ISO_openarray<float4p>*)*b2;
				int	n		= b2.Count();
				if (n == 1) {
					flags |= 1 << 1;
				} else {
					ISO_ASSERT(!num_frames || n == num_frames);
					num_frames	= n;
				}
			} else if (id == ISO_CRC("comp_rot", 0x7a16d9f1)) {
				comp_rot	= *(ISO_openarray<compressed_quaternion>*)*b2;
				int	n		= b2.Count();
				if (n == 1) {
					flags |= 1 << 1;
				} else {
					ISO_ASSERT(!num_frames || n == num_frames);
					num_frames	= n;
				}
			} else if (id == ISO_CRC("pos", 0x80d9e6ac)) {
				pos			= *(ISO_openarray<float3p>*)*b2;
				int	n		= b2.Count();
				if (n == 1) {
					flags |= 1 << 2;
				} else {
					ISO_ASSERT(!num_frames || n == num_frames);
					num_frames	= n;
				}
			} else if (id == ISO_CRC("scale", 0xec462584)) {
				scl			= *(ISO_openarray<float3p>*)*b2;
				int	n		= b2.Count();
				if (n == 1) {
					flags |= 1 << 3;
				} else {
					ISO_ASSERT(!num_frames || n == num_frames);
					num_frames	= n;
				}
			} else if (id == ISO_CRC("cuts", 0xd8b2b13e)) {
				ISO_openarray<float>	&t =*(ISO_openarray<float>*)*b2;
				cuts		= t;
				num_cuts	= t.Count();

			} else {
				ISO::Browser	be		= b2[0];
				int			type;
				if (be.Is<int>())
					type = 0;
				else if (be.Is<float>())
					type = 1;
				else if (be.Is<float[2]>())
					type = 2;
				else if (be.Is<float[3]>())
					type = 3;
				else if (be.Is<float[4]>())
					type = 4;
				else
					continue;

				LookupMessage	lum(id);
				m.obj->Send(lum);
				if (lum.result) {
					Extra	*e	= new(extras) Extra;
					e->array	= *(ISO_openarray<char>*)*b2;
					e->dest		= lum.result;
					int	n		= b2.Count();
					if (n == 1) {
						type	|= 0x10;
						flags	|= 1 << 4;
					} else {
						ISO_ASSERT(!num_frames || b2.Count() == num_frames);
						num_frames	= n;
					}
					e->type		= type;
				}
			}
		}
		if (!num_frames && flags) {
			if (flags == 1) {
				delete this;
				return;
			}
			num_frames = 1;
		}
		ISO_ASSERT(num_frames);

		loop_threshold = num_frames / (30.0f * 2.0f);
		prev_time	= 0;

		// pose
		if (!idle)
			Evaluate(prev_time);
	}

	void	Evaluate(float time) {
		float	t2	= time * 30;
		int		ti0	= int(t2);
		ISO_ASSERT(ti0 < num_frames);
		int		ti1	= ti0 == num_frames - 1 ? 0 : ti0 + 1;
		float	tf	= t2 - ti0;

		while (cut_index < num_cuts) {
			int	cut = int(cuts[cut_index] * 30);
			if (ti0 < cut) {
				if (ti1 == cut)
					ti1 = ti0;
				break;
			}
			cut_index++;
		}

		if (rot || comp_rot || pos || scl) {
			quaternion	q;
			float3		s;
			position3	t;

			if (scl)
				s = (flags & (1<<3)) ? float3(scl[0]) : lerp(float3(scl[ti0]), float3(scl[ti1]), tf);
			if (rot)
				q = quaternion((flags & (1<<1)) ? float4(rot[0]) : lerp(float4(rot[ti0]), float4(rot[ti1]), tf));
			else if (comp_rot) {
				if (flags & (1<<1)) {
					unused(quaternion(comp_rot[0]));
				} else {
					quaternion q0 = quaternion(comp_rot[ti0]);
					quaternion q1 = quaternion(comp_rot[ti1]);
					q1 = q1.closest(q0);
					q = quaternion(lerp(q0.v, q1.v, tf));
				}
			}
			if (pos)
				t = (flags & (1<<2)) ? position3(pos[0]) : lerp(position3(pos[ti0]), position3(pos[ti1]), tf);

			float3x4	m;
			if (rot || comp_rot) {
				if (scl)
					m = (float3x4)(q * scale(s));
				else
					m = (float3x4)(float3x3(q));
			} else if (scl) {
				m = (float3x4)((float3x3)scale(s));
			} else {
				m = obj->GetMatrix();
			}
			m.w = pos ? t : obj->GetPos();
			obj->SetMatrix(m);
		}

		if (event_iter) {
			if (float dt = time - prev_time) {
				// wrap
				int8 dir;
				if (dt < -loop_threshold) {
					// forward
					if (event_iter != event_iter_rend) {
						while (event_iter != event_iter_end) {
							obj->AddEntities(event_iter->event);
							++event_iter;
						}
						event_iter = event_iter_rend;
					}
					++event_iter;
					dir = +1;
				} else if (dt > loop_threshold) {
					// backward
					if (event_iter != event_iter_end) {
						while (event_iter != event_iter_rend) {
							obj->AddEntities(event_iter->event);
							--event_iter;
						}
						event_iter = event_iter_end;
					}
					--event_iter;
					dir = -1;
				} else if ((dir = dt > 0.0f ? +1 : -1) != event_dir) {
					// reverse
					if (dir > 0) {
						while (event_iter != event_iter_end && (event_iter == event_iter_rend || event_iter->time < prev_time))
							++event_iter;
					} else {
						while (event_iter != event_iter_rend && (event_iter == event_iter_end || event_iter->time > prev_time))
							--event_iter;
					}
				}
				// advance
				if (dir > 0) {
					// forward
					while (event_iter != event_iter_end && event_iter->time <= time) {
						obj->AddEntities(event_iter->event);
						++event_iter;
					}
				} else {
					// backward
					while (event_iter != event_iter_rend && event_iter->time >= time) {
						obj->AddEntities(event_iter->event);
						--event_iter;
					}
				}
				event_dir	= dir;
			} else
				event_dir	= 0;
		}

		Extra	*e = extras;
		for (size_t n = extras.size(); n--; e++) {
			void	*dest	= e->dest;
			void	*array	= e->array;
			switch (e->type) {
				case 0:		*(int*)dest		= ((int*)array)[ti0]; break;	// int
				case 1:		*(float*)dest	= lerp(((float*)array)[ti0], ((float*)array)[ti1], tf); break;						// float1
				case 2:		*(float2p*)dest	= lerp(float2(((float2p*)array)[ti0]), float2(((float2p*)array)[ti1]), tf);	break;	// float2
				case 3:		*(float3p*)dest	= lerp(float3(((float3p*)array)[ti0]), float3(((float3p*)array)[ti1]), tf);	break;	// float3
				case 4:		*(float4p*)dest	= lerp(float4(((float4p*)array)[ti0]), float4(((float4p*)array)[ti1]), tf);	break;	// float4
				case 0x10:	*(int*)dest		= *(int*)array; break;			// static int
				case 0x11:	*(float*)dest	= *(float*)array; break;		// static float1
				case 0x12:	*(float2p*)dest	= *(float2p*)array; break;		// static float2
				case 0x13:	*(float3p*)dest	= *(float3p*)array; break;		// static float3
				case 0x14:	*(float4p*)dest	= *(float4p*)array; break;		// static float4
			}
		}

		// idle
		if (speed == 0)
			idle = true;
	}

	void	operator()(AnimationSetMessage &m) {
		if (!m.id || m.id == anim.ID().get_crc32()) {
			float	length = (num_frames - 1) / 30.f;
			if (m.value >= 0) {
				start_time	= m.time * m.speed - (m.value == 0 && m.speed < 0 ? length : m.value);
				set_time	= true;
				idle		= false;
			} else {
				if (speed == 0 && start_time == 0 && m.speed < 0)
					start_time = m.time * m.speed - length;
				else
					start_time += m.time * (m.speed - speed);
				if (m.speed)
					idle = false;
			}
			speed		= m.speed;
			m.handled	= true;
		} else {
			speed		= 0;
			idle		= true;
		}
	}

	void	operator()(FrameEvent2 &ev) {
		PROFILE_CPU_EVENT("AnimHandler");
		if (idle)
			return;

		float length	= (num_frames - 1) / 30.f;
		if (length == 0)
			speed = 0;

		float time_next	= CalcTime(ev.time);
		while (time_next > length || time_next < 0.0f) {
			if (time_next > length) {
				if (speed > 0) {
					set_time	= false;
					start_time	+= length;
					obj->Send(AnimationLoopMessage(0));
					if (idle || (speed == 0 && !set_time)) {
						time_next	= length;
						start_time	= ev.time * speed - time_next;
					} else {
						time_next	= CalcTime(ev.time);
					}
				} else {
					time_next	= length;
					start_time	= ev.time * speed - time_next;
				}
			} else {
				if (speed < 0) {
					set_time	= false;
					start_time	-= length;
					obj->Send(AnimationLoopMessage(0));
					if (idle || (speed == 0 && !set_time)) {
						time_next	= 0;
						start_time = ev.time * speed - time_next;
					} else {
						time_next	= CalcTime(ev.time);
					}
				} else {
					time_next	= 0;
					start_time	= ev.time * speed - time_next;
				}
			}
		}

		Evaluate(time_next);
		prev_time = time_next;
	}

	AnimationHandler(float time, Object *_obj, ISO_ptr<Animation> t)
		: DeleteOnDestroy<AnimationHandler>(_obj), obj(_obj)
		, anim(t)
		, start_time(time), speed(1), idle(false), num_frames(0), flags(0)
		, rot(0), comp_rot(0), pos(0), scl(0)
		, num_cuts(0), cut_index(0), event_iter(0)
	{
		_obj->SetHandler<AnimationSetMessage>(this);
		_obj->AddHandler<PostCreateMessage>(this);
	}
};

namespace iso {

static initialise init(
	ISO::getdef<AnimationEventKey>(),
	ISO::getdef<compressed_quaternion>()
);

template<> void TypeHandler<AnimationHierarchy>::Create(const CreateParams &cp, crc32 id, const AnimationHierarchy *t) {
	new AnimationHandlerBonesHierarchy(cp.obj, t);
}
TypeHandler<AnimationHierarchy>	thAnimationHierarchy;

template<> void TypeHandler<Animation>::Create(const CreateParams &cp, crc32 id, const Animation *t) {
	for (int i = 0, n = t->Count(); i < n; i++) {
		if ((*t)[i].IsID("events") || (*t)[i].IsType<ISO_openarray<AnimationEventKey> >())
			continue;

		if ((*t)[i].IsType<Animation>(ISO::MATCH_NOUSERRECURSE))
			new AnimationHandlerBones(cp.obj, GetPtr(t));
		else
			new AnimationHandler(cp.Time(), cp.obj, GetPtr(t));
		return;
	}
}

extern "C" { TypeHandler<Animation> thAnimation; }

}// namespace iso;
