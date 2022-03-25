#ifndef OBJECT_H
#define OBJECT_H

#include "scenegraph.h"
#include "events.h"
#include "allocators/pool.h"
#include "message.h"
#include "maths/geometry.h"

#undef GetObject

namespace iso {

class Object;
class RigidBody;
struct Pose;
class World;

const char* GetLabel(tag2 id);
const char* GetLabel(crc32 id);
ent::Attachment		*FindAttachment(const anything &children, int id);
const ISO_ptr<void> &FindType(const anything &children, const ISO::Type *type);
const ISO_ptr<void> &FindType(const anything &children, tag2 id);

template<typename T> inline const ISO_ptr<T> &Find(const anything &children) {
	return reinterpret_cast<const ISO_ptr<T>&>(FindType(children, ISO::getdef<T>()));
}

template<typename T> struct ObjectPropertySlot	{ enum {slot = T::PROPERTY_INDEX}; };
template<> struct ObjectPropertySlot<RigidBody>	{ enum {slot = 0}; };
template<> struct ObjectPropertySlot<Pose>		{ enum {slot = 1}; };

//-----------------------------------------------------------------------------
//	CRC_type
//-----------------------------------------------------------------------------

template<typename T, bool> struct CRC_type_s2		{ static crc32	f() { return type_id<T>(); } };
template<typename T> struct CRC_type_s2<T, true>	{ static crc32	f()	{ return ((ISO::TypeUser*)ISO::getdef<T>())->ID(); } };
template<typename T> struct CRC_type_s				{ static crc32	f() { return CRC_type_s2<T, T_is_base_of<ISO::TypeUser, ISO::def<T>>::value>::f(); } };

template<typename T> crc32 CRC_type() {
	static crc32 c = CRC_type_s<T>::f();
	return c;
}
template<typename T> crc32 CRC_type(const T &t) {
	return CRC_type<T>();
}
inline crc32 CRC_type(const ISO::Type *type) {
	return type->GetType() == ISO::USER ? ((ISO::TypeUser*)type)->ID() : crc32();
}
template<typename T> crc32 CRC_type(const ISO_ptr<T> &p) {
	return CRC_type(p.GetType());
}

//-----------------------------------------------------------------------------
//	Object
//-----------------------------------------------------------------------------

class Object : public aligner<128>, public referee, public MessageReceiver, public hierarchy<Object> {
	friend hierarchy<Object>;
	friend Object*	Attach(Object *parent, Object *child);
	friend Object*	Attach(Object *parent, Object *child, ent::Attachment *at);
	friend Object*	Attach(World *world, Object *parent, ISO_ptr<Node> t, ent::Attachment *at);
	friend void		Adopt(Object *parent, Object *child);
	friend void		UnLink(Object *child);

public:
	enum FLAGS {
		DYNAMIC		= 1 << 0,	// dynamic = not static, i.e. moves or has moved
		LOOKUPEVENT	= 1 << 1,
		MAGIC		= 0xad21a700,
	};

	typedef hierarchy<Object>::iterator			iterator;
	typedef hierarchy<Object>::const_iterator	const_iterator;

	flags<FLAGS>	flags;

private:
	ISO_ptr<Node>	node;
	void*			properties[6];
	float3x4		matrix;

	void	Init(param(float3x4) mat);
	void	Init(World *world, Object *_parent, param(float3x4) mat);
public:
	Object()	{ Init(identity); }
	Object(World *world, param(float3x4) mat, Object *_parent = NULL)										{ Init(world, _parent, mat); }
	Object(World *world, ISO_ptr<Node> _node, Object *_parent = NULL) : node(_node)							{ Init(world, _parent, node ? float3x4(node->matrix) : identity); }
	Object(World *world, ISO_ptr<Node> _node, Object *_parent, param(float3x4) offset_mat) : node(_node)	{ Init(world, _parent, node ? float3x4(node->matrix) * offset_mat : offset_mat); }

	Object(param(float3x4) mat, Object *_parent = NULL);
	Object(ISO_ptr<Node> _node, Object *_parent = NULL);
	Object(ISO_ptr<Node> _node, Object *_parent, param(float3x4) offset_mat);
	~Object();

	tag2					GetName()	 						const	{ return node.ID();	}
	Object*					Root()										{ return root();	}
	Object*					Parent()							const	{ return parent;	}
//	Object*					Child()								const	{ return child;		}
//	Object*					Sibling()							const	{ return sibling;	}
	Object*					Detach();

	void					ClearNode()									{ node.Clear();	}
	void					SetNode(World *world, ISO_ptr<Node> &_node)	{ AddEntitiesArray(world, (node = _node)->children); }
	const ISO_ptr<Node>&	GetNode()							const	{ return node;	}

	position3				GetPos()							const	{ return get_trans(matrix);	}
	const float3x4&			GetMatrix()							const	{ return matrix;				}
	void					SetMatrix(param(float3x4) m)				{ matrix = m; SetMoved();		}
	void					SetMoved(World *world);
	void					SetMoved();
	position3				GetWorldPos()						const;
	float3x4				GetWorldMat()						const;
	position3				GetRelativePos(const Object *obj2)	const;
	float3x4				GetRelativeMat(const Object *obj2)	const;
	float3x4				GetBoneMat(crc32 name)				const;
	float3x4				GetBoneMat(uint8 index)				const;
	float3x4				GetBoneWorldMat(crc32 name)			const	{ return GetWorldMat() * GetBoneMat(name); }
	float3x4				GetBoneWorldMat(uint8 index)		const	{ return GetWorldMat() * GetBoneMat(index); }

	ent::Attachment*		FindAttachment(int id)				const	{ return node ? iso::FindAttachment(node->children, id) : NULL; }
	const ISO_ptr<void>&	FindType(tag2 id)					const	{ return iso::FindType(node->children, id);		}
	const ISO_ptr<void>&	FindType(const ISO::Type *type)		const	{ return iso::FindType(node->children, type);	}
	template<typename T> const ISO_ptr<T> &FindType()			const	{ return reinterpret_cast<const ISO_ptr<T>&>(FindType(ISO::getdef<T>()));	}

	ent::Attachment*		FindAttachmentHier(int id, Object **obj = NULL)			const;
	const ISO_ptr<void>&	FindTypeHier(tag2 id, Object **obj = NULL)				const;
	const ISO_ptr<void>&	FindTypeHier(const ISO::Type *type, Object **obj = NULL)	const;
	template<typename T> const ISO_ptr<T> &FindTypeHier(Object **obj = NULL)		const	{ return reinterpret_cast<const ISO_ptr<T>&>(FindTypeHier(ISO::getdef<T>(), obj)); }

	bool					AddEntity(const struct CreateParams &cp, tag2 id, crc32 type, const void *p);
	bool					AddEntity(World *world, tag2 id, crc32 type, const void *p, param(float3x4) matrix = identity);
	bool					AddEntity(World *world, const ISO_ptr<void> &p, param(float3x4) matrix = identity);
	bool					AddEntities(World *world, const ISO_ptr<void> &p);
	void					AddEntitiesArray(World *world, const anything &a);

	bool					AddEntity(tag2 id, crc32 type, const void *p);
	bool					AddEntity(const ISO_ptr<void> &p);
	bool					AddEntities(const ISO_ptr<void> &p);
	void					AddEntitiesArray(const anything &a);

	template<typename T>bool	AddEntity(World *world, tag2 id, const T &t)	{ return AddEntity(world, id, CRC_type<T>(), &t); }
	template<typename T>bool	AddEntity(tag2 id, const T &t)					{ return AddEntity(id, CRC_type<T>(), &t); }

	// so can be overridden based on M:

//	template<class M> void		RemoveHandler(const callback<void(M&)> &h)	{ DelegateCollection<M>::Remove(this, h); }
	template<class M, typename T> void	RemoveHandler(const T &h)			{ DelegateCollection<M>::Remove(this, h); }
	template<class M, typename T> void	AddHandler(const T &h)				{ DelegateCollection<M>::Add(this, h); }
	template<class M> bool		SendUp(const M &m);

	template<typename T> void	SetProperty(T *t)	{ properties[ObjectPropertySlot<T>::slot]	= t;		}
	template<typename T> T*		Property()	const	{ return (T*)properties[ObjectPropertySlot<T>::slot];	}

	friend Object*	Attach(Object *parent, ISO_ptr<Node> t, ent::Attachment *at) {
		return t ? Attach(parent, new Object(t), at) : NULL;
	}
	friend Object*	Attach(World *world, Object *parent, ISO_ptr<Node> t, ent::Attachment *at) {
		return t ? Attach(parent, new Object(world, t), at) : NULL;
	}
};

inline bool verify_valid(Object *obj) {
	return (obj->flags & ~3) == Object::MAGIC;
}

//void deleter(Object *obj);
typedef linked_ref<Object>	ObjectReference;

//-----------------------------------------------------------------------------
//	World
//-----------------------------------------------------------------------------
template<int N> struct ObjectArray : static_array<ObjectReference, N> {
	typedef static_array<ObjectReference, N>	B;
	class iterator2 {
		iterator_t<B>	i, e;

		void		Next() {
			while (i != e) {
				if (*++i)
					break;
			}
		}
		Object*		GetObj()		const	{ return i != e ? (Object*)*i : 0; }

	public:
		iterator2(ObjectArray &array) : i(array.begin()), e(array.end())	{ while (i != e && !*i) ++i; }
		Object*		operator->()	const	{ return GetObj();		}
		Object&		operator*()		const	{ return *GetObj();		}
		operator	Object*()		const	{ return GetObj();		}
		void		operator++()			{ Next();	}
	};

	void compact();

	template<typename T> void send(T &m) {
		int	n = 0;
		for (auto &i : *this) {
			if (Object *obj = i) {
				obj->Send(m);
				n++;
			}
		}
		if (n * 2 < B::size())
			compact();
	}

	void cleanup() {
		for (auto &i : *this) {
			if (Object *obj = i) {
				if (!obj->Parent())
					delete obj;
			}
		}
		compact();
	}

	void add(Object *obj) {
		if (B::size() == B::capacity())
			compact();
		B::push_back(obj);
	}
};
template<int N> void ObjectArray<N>::compact() {
	auto i = B::begin(), e = B::end();
	while (i != e && *i)
		++i;
	auto d = i;
	while (i != e) {
		if (Object *obj = *i++)
			*d++ = obj;
	}
	B::resize(int(d - B::begin()));
}

struct TimerMessage {
	World	*world;
	float	time;
	TimerMessage(World *world, float time) : world(world), time(time) {}
	TimerMessage&	me() { return *this; }
};
struct Timer : e_link<Timer>, callback<void(TimerMessage&)>, from<fixed_pool<Timer, 1024> >	{
	float	time;
	bool	operator<(const Timer &t2) const { return time < t2.time; }
};

struct WorldFrameMessage {
	World	*world;
	float	time, dt;
	WorldFrameMessage(World *_world, float _time, float _dt) : world(_world), time(_time), dt(_dt) {}
};
struct FrameEvent		: WorldFrameMessage {
	FrameEvent(World *w, float time, float dt) : WorldFrameMessage(w, time, dt) {}
};
struct FrameEvent2		: WorldFrameMessage {
	FrameEvent2(World *w, float time, float dt) : WorldFrameMessage(w, time, dt) {}
};

class World : public Object {
	enum { MAX_ROOT_COUNT = 1000, MAX_DYNAMIC_COUNT = 2000 };
	static World					*current;
	ObjectArray<MAX_DYNAMIC_COUNT>	dynamic_objects;
	float							time, time2;
	e_list<Timer>					timers;
	cuboid							extent;

	void _add(Timer *timer, float dt) {
		timer->time = time + dt;
		lower_boundc(timers, *timer)->insert_before(timer);
	}
public:
//	typedef ObjectArray<MAX_ROOT_COUNT>::iterator2 iterator;

	World() : time(0), time2(0), extent(iso::empty) {}

	static World*	Current()	{ return current; }
	static Object*	Global()	{ static Object global; return &global; }

	void	Use()				{ current = this;		}
	Object*	Root()				{ return this;			}
	float	Time()		const	{ return time;			}
	float	FrameDT()	const	{ return time - time2;	}
	void	Reset()				{ time = time2 = 0;		}

	void	Begin();
	void	End();
	void	Tick1(float _time);
	void	Tick2();

	void	Remove(Timer *timer) {
		delete timer;
	}

	template<typename T> Timer *AddTimer(T *t, float dt) {
		Timer	*timer = new Timer;
		timer->bind(t);
		_add(timer, dt);
		return timer;
	}

	template<typename T> Timer *AddTimer(void (*f)(T*, TimerMessage&), T *param, float dt) {
		Timer	*timer = new Timer;
		timer->bind(f, param);
		_add(timer, dt);
		return timer;
	}
	void			AddDynamicObject(Object *obj)		{ return dynamic_objects.add(obj); }
	void			MoveDynamicObjects();

	void			AddBox(param(cuboid) box)			{ extent |= box; }
	const cuboid&	Extent()	const					{ return extent; }

	inline bool		AddEntity(const ISO_ptr<void> &p, param(float3x4) matrix = identity)	{ return Object::AddEntity(this, p, matrix); }
	inline bool		AddEntities(const ISO_ptr<void> &p)										{ return Object::AddEntities(this, p); }
	inline void		AddEntitiesArray(const anything &a)										{ return Object::AddEntitiesArray(this, a); }

	template<typename T> bool		AddEntity(tag2 id, const T &t)							{ return Object::AddEntity(this, id, CRC_type<T>(), &t); }
	inline bool						AddEntity(tag2 id, crc32 type, const void *p)			{ return Object::AddEntity(this, id, type, p); }
	template<typename T> void		SetItem(crc32 id, const T *p);
	template<typename T> const T*	GetItem(crc32 id);
};

struct WorldEvent : Message<WorldEvent> {
	enum STATE {BEGIN, END, UPDATE};
	World		*world;
	STATE		state;
	WorldEvent(World *w, STATE _state) : world(w), state(_state) {}
};

template<class M> bool Object::SendUp(const M &m) {
	for (Object *obj = this; obj; obj = obj->Parent()) {
		if (obj->Send(m))
			return true;
	}
	return World::Current()->Send(m) || World::Global()->Send(m);
}
//-----------------------------------------------------------------------------
//	inline Object functions that need World
//-----------------------------------------------------------------------------

inline Object::Object(param(float3x4) mat, Object *_parent)												{ Init(World::Current(), _parent, mat); }
inline Object::Object(ISO_ptr<Node> _node, Object *_parent) : node(_node)								{ Init(World::Current(), _parent, node ? float3x4(node->matrix) : identity); }
inline Object::Object(ISO_ptr<Node> _node, Object *_parent, param(float3x4) offset_mat) : node(_node)	{ Init(World::Current(), _parent, node ? float3x4(node->matrix) * offset_mat : offset_mat); }

inline void		Object::SetMoved()										{ SetMoved(World::Current()); }
inline Object*	Object::Detach()										{ Adopt(World::Current(), this); return this; }

inline bool		Object::AddEntity(tag2 id, crc32 type, const void *p)	{ return AddEntity(World::Current(), id, type, p); }
inline bool		Object::AddEntity(const ISO_ptr<void> &p)				{ return AddEntity(World::Current(), p); }
inline bool		Object::AddEntities(const ISO_ptr<void> &p)				{ return AddEntities(World::Current(), p); }
inline void		Object::AddEntitiesArray(const anything &a)				{ return AddEntitiesArray(World::Current(), a); }


//-----------------------------------------------------------------------------
//	Messages
//-----------------------------------------------------------------------------

template<class T, class M> class HandlesGlobal {
public:
	HandlesGlobal() {
		World::Global()->AddHandler<M>(static_cast<T*>(this));
	}
	~HandlesGlobal() {
		World::Global()->RemoveHandler<M>(static_cast<T*>(this));
	}
};

template<class T, class M> class HandlesWorld {
public:
	HandlesWorld() {
		World::Current()->AddHandler<M>(static_cast<T*>(this));
	}
	~HandlesWorld() {
		World::Current()->RemoveHandler<M>(static_cast<T*>(this));
	}
};

struct GenericMessage {
	uint32		id;
	int			result;
	GenericMessage(uint32 _id) : id(_id), result(0) {}
	int	Send()			{ World::Current()->Send(this); return result; }
	int	Send(World &w)	{ w.Send(this); return result; }
};

template<typename T> struct T_GenericMessage : GenericMessage {
	T_GenericMessage() : GenericMessage(T::ID) {}
};

struct DebugUpdateMessage {
	float	time, dt;
	DebugUpdateMessage(float _time, float _dt) : time(_time), dt(_dt) {}
};

//-------------------------------------
//	Create
//-------------------------------------

struct CreateParams {
	World				*world;
	Object				*obj;
	crc32				bone;
	num_init<float,1>	quality;
	float3x4			matrix;

	CreateParams(World *_world, Object *_obj = 0, param(float3x4) _matrix = identity)	: world(_world), obj(_obj), matrix(_matrix)	{}
	CreateParams(World *_world, Object *_obj, crc32 _bone, param(float3x4) _matrix)		: world(_world), obj(_obj), bone(_bone), matrix(_matrix)	{}
	CreateParams(World *_world, param(float3x4) _matrix)								: world(_world), obj(NULL), matrix(_matrix)	{}

	float	Time() const { return world->Time(); }
};

typedef callback<void(const CreateParams&, crc32, arbitrary_const_ptr)> create_cb;

struct CreateMessage : CreateParams {
	crc32			id;
	crc32			type;
	const void		*p;

	CreateMessage(const CreateParams &_cp, crc32 _id, crc32 _type, const void *_p)								: CreateParams(_cp), id(_id), type(_type), p(_p)					{}
	CreateMessage(World *_world, Object *_obj, param(float3x4) matrix, crc32 _id, crc32 _type, const void *_p)	: CreateParams(_world, _obj, matrix), id(_id), type(_type), p(_p)			{}
	CreateMessage(World *_world, Object *_obj, param(float3x4) matrix, const ISO_ptr<void> &_p)					: CreateParams(_world, _obj, matrix), id(_p.ID().get_crc32()), type(CRC_type(_p.GetType())), p(_p)	{}
	template<typename T> CreateMessage(World *_world, Object *_obj, param(float3x4) matrix, crc32 _id, T *t)	: CreateParams(_world, _obj, matrix), id(_id), type(CRC_type<T>()), p(t)	{}
};

// inherit from this
template<typename B, typename T> struct Creator {
	void operator()(const CreateParams &cp, crc32 id, arbitrary_const_ptr p) {
		(static_cast<B*>(this))->Create(cp, id, (const T *)p);
	}
	Creator(Object *obj) {
		obj->AddHandler<CreateMessage>(this);
	}
};

// add this handler
template<typename T> struct Creation : create_cb {
	template<typename B> static void	thunk(void *b, const CreateParams &cp, crc32 id, arbitrary_const_ptr p) {
		((B*)b)->Create(cp, id, (const T*)p);
	}
	template<typename B> Creation(B *b) : create_cb(b, thunk<B>) {}
};

template<uint32 CRC> struct CreationCRC : create_cb {
	template<typename B> static void	thunk(void *b, const CreateParams &cp, crc32 id, arbitrary_const_ptr p) {
		((B*)b)->template Create<CRC>(cp, id, p);
	}
	template<typename B> CreationCRC(B *b) : create_cb(b, thunk<B>) {}
};

template<> struct DelegateCollection<CreateMessage> {
	hash_map<uint32, create_cb>	handlers;

	void operator()(CreateMessage &m) {
		if (const create_cb *c = handlers.check(m.type)) {
			(*c)(m, m.id, m.p);
			m.p = 0;
		}
	}
	static DelegateCollection	*get(MessageReceiver *r) {
		if (const callback<void(CreateMessage&)> *p = r->GetHandler<CreateMessage>())
			return (DelegateCollection*)p->me;
		DelegateCollection *da = new DelegateCollection;
		r->SetHandler<CreateMessage>(da);
		return da;
	}
	template<typename B, typename T> static void Add(MessageReceiver *r, Creator<B, T> *t) {
		DelegateCollection *da = get(r);
		da->handlers[CRC_type<T>()] = t;
	}
	template<typename T> static void Add(MessageReceiver *r, const Creation<T> &c) {
		DelegateCollection *da = get(r);
		da->handlers[CRC_type<T>()] = c;
	}
	template<uint32 CRC> static void Add(MessageReceiver *r, const CreationCRC<CRC> &c) {
		DelegateCollection *da = get(r);
		da->handlers[CRC] = c;
	}
	static void Add(MessageReceiver *r, const pair<uint32,create_cb> &p) {
		DelegateCollection *da = get(r);
		da->handlers[p.a] = p.b;
	}
};

template<uint32 CRC> class TypeHandlerCRC {
public:
	TypeHandlerCRC() {
		World::Global()->AddHandler<CreateMessage>(make_pair(CRC, make_callback(this, &TypeHandlerCRC::Create)));
		instance() = this;
	}
	template<class C> TypeHandlerCRC(C *c) {
		World::Global()->AddHandler<CreateMessage>(make_pair(CRC, create_cb(c)));
		instance() = this;
	}
	void	Create(const CreateParams &cp, crc32 id, const void *t);
	static TypeHandlerCRC *&instance()											{ static TypeHandlerCRC *t; return t; }
};
template<uint32 CRC> void CreateType(const CreateParams &cp, ISO_ptr<void> t)	{ if (t) TypeHandlerCRC<CRC>::instance()->Create(cp, t.ID(), t);	}

template<typename T> class TypeHandler {
public:
	TypeHandler() {
		World::Global()->AddHandler<CreateMessage>(Creation<T>(this));
		instance() = this;
	}
	template<class C> TypeHandler(C *c) {
		World::Global()->AddHandler<CreateMessage>(Creation<T>(c));
		instance() = this;
	}
	void	Create(const CreateParams &cp, crc32 id, const T *p);
	static TypeHandler *&instance()												{ static TypeHandler *t; return t; }
};
template<typename T> void CreateType(const CreateParams &cp, ISO_ptr<T> &t)		{ if (t) TypeHandler<T>::instance()->Create(cp, t.ID(), t);	}

//-------------------------------------
//	Load
//-------------------------------------

struct LoadParams {
	dynamic_array<ISO_ptr<void> > &hold;

	LoadParams(dynamic_array<ISO_ptr<void> > &_hold) : hold(_hold) {}
	ISO_ptr<void>	Hold(ISO::Browser data, tag2 id);
	void			Hold0(ISO::Browser data, tag2 id);
	void			LoadExternals(ISO::Browser data);

	void	PreLoad(tag2 id, crc32 type, const void *p);
	void	PreLoad(const ISO_ptr<void> &p);
};


typedef callback<void(LoadParams &lp, crc32, arbitrary_const_ptr)> load_cb;

struct LoadMessage : LoadParams {
	crc32			id;
	crc32			type;
	const void		*p;

	LoadMessage(LoadParams &lp, crc32 _id, crc32 _type, const void *_p)		: LoadParams(lp), id(_id), type(_type), p(_p)							{}
	LoadMessage(LoadParams &lp, const ISO_ptr<void> &_p)					: LoadParams(lp), id(_p.ID().get_crc32()), type(CRC_type(_p.GetType())), p(_p)	{}
	template<typename T> LoadMessage(LoadParams &lp, crc32 _id, T *t)		: LoadParams(lp), id(_id), type(CRC_type<T>()), p(t)					{}
};


template<typename T> struct Loading : load_cb {
	template<typename B> static void	thunk(void *b, dynamic_array<ISO_ptr<void> > &hold, tag2 id, arbitrary_const_ptr p) {
		((B*)b)->Load(hold, id, (const T*)p);
	}
	template<typename B> Loading(B *b) : load_cb(b, thunk<B>) {}
};

template<> struct DelegateCollection<LoadMessage> {
	hash_map<uint32, load_cb>	handlers;

	void operator()(LoadMessage &m) {
		if (const load_cb *c = handlers.check(m.type)) {
			(*c)(m, tag2(m.id), m.p);
			m.p = 0;
		}
	}
	static DelegateCollection	*get(MessageReceiver *r) {
		if (const callback<void(LoadMessage&)> *p = r->GetHandler<LoadMessage>())
			return (DelegateCollection*)p->me;
		DelegateCollection *da = new DelegateCollection;
		r->SetHandler<LoadMessage>(da);
		return da;
	}
	template<typename T> static void Add(MessageReceiver *r, const Loading<T> &c) {
		DelegateCollection *da = get(r);
		da->handlers[CRC_type<T>()] = c;
	}
	template<typename T> static void Add(MessageReceiver *r, const pair<uint32,load_cb> &p) {
		DelegateCollection *da = get(r);
		da->handlers[p.a] = p.b;
	}
};

template<uint32 CRC> class LoadHandlerCRC {
public:
	LoadHandlerCRC() {
		World::Global()->AddHandler<LoadMessage>(make_pair(CRC, load_cb(this)));
	}
	template<class C> LoadHandlerCRC(C *c) {
		World::Global()->AddHandler<LoadMessage>(make_pair(CRC, load_cb(c)));
	}
	void	Load(LoadParams &lp, crc32 id, const void *t);
	void	Load(LoadParams &lp, const ISO_ptr<void> &t)	{ return Load(lp, t.ID(), t); }
};

template<typename T> class LoadHandler {
public:
	LoadHandler() {
		World::Global()->AddHandler<LoadMessage>(Loading<T>(this));
	}
	template<class C> LoadHandler(C *c) {
		World::Global()->AddHandler<LoadMessage>(Loading<T>(c));
	}
	void	Load(LoadParams &lp, crc32 id, const T *p);
	void	Load(LoadParams &lp, const ISO_ptr<T> &t)		{ return Load(lp, t.ID(), t); }
};

//-------------------------------------
//	PostCreateMessage
//-------------------------------------

struct PostCreateMessage {
	Object	*obj;
	PostCreateMessage(Object *_obj) : obj(_obj)	{}
};

//-------------------------------------
//	Modify
//-------------------------------------

struct GetSetMessage {
	crc32			id;
	crc32			type;
	const void		*p;
	GetSetMessage(crc32 _id, crc32 _type, const void *_p = 0) : id(_id), type(_type), p(_p) {}
};

template<typename T> void		World::SetItem(crc32 id, const T *p) {
	Send(GetSetMessage(id, CRC_type<T>(), p));
}
template<typename T> const T*	World::GetItem(crc32 id) {
	GetSetMessage m(id, CRC_type<T>());
	return Send(m) ? (const T*)m.p : 0;
}

template<typename T> struct GetSetter {
	World	*w;
	T		&t;
	void operator()(GetSetMessage &m) {
		if (m.type == CRC_type<T>()) {
			if (m.p)
				t	= *(T*)m.p;
			else
				m.p	= &t;
		}
	}
	GetSetter(World *_w, T &_t) : w(_w), t(_t) {
		w->AddHandler<GetSetMessage>(this);
	}
	~GetSetter() {
		w->RemoveHandler<GetSetMessage>(this);
	}
};

template<typename T> struct GetSetter<const T> {
	World	*w;
	const T	&t;
	void operator()(GetSetMessage &m) {
		if (m.type == CRC_type<T>())
			m.p	= &t;
	}
	GetSetter(World *_w, const T &_t) : w(_w), t(_t) {
		w->AddHandler<GetSetMessage>(this);
	}
	~GetSetter() {
		w->RemoveHandler<GetSetMessage>(this);
	}
};

template<typename T> GetSetter<T> make_GetSetter(World *w, T &t) { return GetSetter<T>(w, t); }

//-------------------------------------
//	Destroy
//-------------------------------------

struct DestroyMessage {
	Object	*obj;
	DestroyMessage(Object *_obj) : obj(_obj)	{}
};

template<> struct DelegateCollection<DestroyMessage> : DelegateChain<DestroyMessage> {};

template<typename T> struct MesssageDelegate<T, DestroyMessage> : DelegateLink<T, DestroyMessage> {
	MesssageDelegate(MessageReceiver *r) : DelegateLink<T, DestroyMessage>(r) {}
};

template<> struct MesssageDelegate<void, DestroyMessage> : DelegateLink<void, DestroyMessage> {
	template<typename T> MesssageDelegate(T *t, MessageReceiver *r) : DelegateLink<void, DestroyMessage>(t, r) {}
};

template<typename T> struct MessageHandler<T, DestroyMessage> : DelegateLink<T, DestroyMessage> {
	MessageHandler() {}
	MessageHandler(MessageReceiver *r) : DelegateLink<T, DestroyMessage>(r) {}
};

template<typename T> struct DeleteOnDestroy : DelegateLink<DeleteOnDestroy<T>, DestroyMessage> {
	void operator()(DestroyMessage &msg) { delete static_cast<T*>(this); }
	DeleteOnDestroy(Object *obj) : DelegateLink<DeleteOnDestroy<T>, DestroyMessage>(obj) {}
};

template<typename T> struct CleanerUpper : DeleteOnDestroy<T> {
	CleanerUpper() : DeleteOnDestroy<T>(World::Current()) {}
};

//-------------------------------------
//	Query
//-------------------------------------

struct LookupMessage {
	crc32			id;
	uint32			flags;
	void			*result;
	const ISO::Type	*type;
	LookupMessage(crc32 _id, uint32 _flags = 0) : id(_id), flags(_flags), result(NULL)	{}
	template<typename T> void set(T *t)	{ result = t; type = ISO::getdef<T>(); }
	void set(ISO::Browser &b)			{ result = b; type = b.GetTypeDef(); }
};

template<> struct DelegateCollection<LookupMessage> : DelegateArray<LookupMessage> {
	static void	Add(MessageReceiver *r, const cb &c) {
		DelegateArray<LookupMessage>::Add(r, c);
		static_cast<Object*>(r)->flags.set(Object::LOOKUPEVENT);
	}
	static void	Remove(MessageReceiver *r, const cb &c) {
		DelegateArray<LookupMessage>::Remove(r, c);
		static_cast<Object*>(r)->flags.set(Object::LOOKUPEVENT);
	}
};
template<> inline void	Object::RemoveHandler<LookupMessage>(const callback<void(LookupMessage&)> &h) {
	DelegateArray<LookupMessage>::Remove(this, h);
	flags.set(Object::LOOKUPEVENT);
}

struct QueryTypeMessage {
	typedef void enumfn_t(crc32 type, arbitrary data);

	enumfn_t	*enumfn;
	crc32		filter;
	uint32		type;
	arbitrary	data;

	QueryTypeMessage()					: enumfn(NULL), type(0), data(0)	{}
	QueryTypeMessage(crc32 _filter)		: enumfn(NULL), filter(_filter), data(0) {}
	QueryTypeMessage(enumfn_t *_enumfn) : enumfn(_enumfn) {}
	void set(crc32 _type, arbitrary _data) {
		if (enumfn)
			enumfn(_type, _data);
		else if (!filter || filter == _type) {
			type = _type;
			data = _data;
		}
	}
	arbitrary send(Object *obj) {
		return obj->Send(*this), data;
	}
};

//-------------------------------------
//	Move/Attach objects
//-------------------------------------

struct MoveMessage {
	bool	moved_static;
	MoveMessage() : moved_static(false)	{}
	void	Update(bool m) {
		if (m)
			moved_static = true;
	}
};

struct AttachMessage {
	Object	*obj;
	bool	attach;
	AttachMessage(Object *_obj, bool _attach) : obj(_obj), attach(_attach) {}
};

//-------------------------------------
//	Trigger
//-------------------------------------

// TriggerMessage
struct TriggerMessage {
	Object	*obj;
	crc32	dispatch_id;
	uint32	value;
	TriggerMessage(Object* _obj, crc32 _dispatch_id = crc32(), uint32 _value = 0U) : obj(_obj), dispatch_id(_dispatch_id), value(_value) {}
};


//-------------------------------------
//	EventListener
//-------------------------------------

template<typename M, typename T> struct EventListener {
	T						*listener;
	dynamic_array<Object*>	handlers;

	EventListener(T *_listener) : listener(_listener) {}

	void Add(Object *obj) {
		obj->SetHandler<M>(listener);
		handlers.push_back(obj);
	}
	void Remove(Object *obj) {
		Object	**i = find(handlers, obj);
		if (i != handlers.end()) {
			(*i)->RemoveHandler<M>(listener);
			handlers.erase_unordered(i);
		}
	}
};

//-----------------------------------------------------------------------------
//	Pose
//-----------------------------------------------------------------------------

struct Joint : rot_trans {
	float3p	scale;
	union {
		float	weight;
		uint32	name;
	};

	void		clear() {
		rot_trans::reset();
		scale	= float3(one);
	}
	operator	float3x4() const {
		return rot_trans::operator iso::float3x4() * iso::scale(float3(scale));
	}
	void operator=(param(float3x4) m) {
		scale	= get_scale(m);
		if (m.det() < zero)
			scale = -scale;
		rot_trans::operator=(m / iso::scale(one / float3(scale)));
	}
};

struct Pose0 : public aligner<16> {
	static const uint8 INVALID = 0xff;
	int			count;
	uint8		*parents;
	Joint		*joints;

	Pose0(linear_allocator &a, int n);
	Pose0*		Init(const BasePose *bp);

	float3x4*	GetObjectMatrices(float3x4 *objmats)	const;
	float3x4	GetObjectMatrix(int i)					const;
	uint8		Find(crc32 name, uint8 hint = 0)		const;

	int			Count()		const { return count; }

	static Pose0 *Create(int n);
	static Pose0 *Create(const BasePose *bp);
};

struct Pose : public Pose0 {
	float3x4	*mats;
	float3x4	*invbpmats;
	float3x4	*skinmats;

	Pose(linear_allocator &a, const BasePose *bp);

	float3x4*	GetObjectMatrices(float3x4 *objmats)	const;
	float3x4	GetObjectMatrix(int i)					const;
	void		Reset();
	void		Update();
	void		GetSkinMats(float3x4 *dest);

	static Pose *Create(const BasePose *bp);
};

} // namespace iso

#endif // OBJECT_H
