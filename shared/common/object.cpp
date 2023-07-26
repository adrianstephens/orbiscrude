#include "object.h"
#include "vector_string.h"
#include "crc_dictionary.h"

#define USE_OBJDIAGNOSTICS	0

namespace iso {

World	*World::current;
/*
const char* GetLabel(tag2 id) {
	return id ? (id.get_tag() ? (const char*)id.get_tag() : LookupCRC32(id, "Unknown")) : "Unnamed";
}
const char* GetLabel(crc32 id) {
	return id ? LookupCRC32(id, "Unknown") : "Unnamed";
}
*/
ent::Attachment *FindAttachment(const anything &children, int id) {
	for (int i = children.Count(); i--; ) {
		const ISO_ptr<void>	&child = children[i];
		if (child.IsType<ent::Attachment>()) {
			if (((ent::Attachment*)child)->id == id)
				return child;
		} else if (child.IsType<ent::Cluster>()) {
			ent::Cluster	*cluster = child;
			if (cluster->hirez.IsType<anything>()) {
				if (ent::Attachment *att = FindAttachment(*(anything*)cluster->hirez, id))
					return att;
			}
		} else if (child.IsType<ent::Splitter>()) {
			ent::Splitter	*splitter = child;
			if (splitter->hirez.IsType<anything>()) {
				if (ent::Attachment *att = FindAttachment(*(anything*)splitter->hirez, id))
					return att;
			}
		} else if (child.IsType<anything>()) {
			if (ent::Attachment *att = FindAttachment(*(anything*)child, id))
				return att;
		}
	}
	return NULL;
}

const ISO_ptr<void> &FindType(const anything &children, const ISO::Type *type) {
	for (int i = children.Count(); i--; ) {
		const ISO_ptr<void>	&child = children[i];
		if (child.IsType(type))
			return child;

		if (child.IsType<ent::Cluster>()) {
			ent::Cluster	*cluster = child;
			if (cluster->hirez.IsType<anything>()) {
				if (const ISO_ptr<void> &p = FindType(*(anything*)cluster->hirez, type))
					return p;
			}
		} else if (child.IsType<ent::Splitter>()) {
			ent::Splitter *splitter = child;
			if (splitter->hirez.IsType<anything>()) {
				if (const ISO_ptr<void> &p = FindType(*(anything*)splitter->hirez, type))
					return p;
			}
			if (splitter->lorez.IsType<anything>()) {
				if (const ISO_ptr<void> &p = FindType(*(anything*)splitter->lorez, type))
					return p;
			}
		} else if (child.IsType<anything>()) {
			if (const ISO_ptr<void> &p = FindType(*(anything*)child, type))
				return p;
		}
	}
	return ISO::iso_nil<32>;
}

const ISO_ptr<void> &FindType(const anything &children, tag2 id) {
	for (int i = children.Count(); i--; ) {
		const ISO_ptr<void>	&child = children[i];
		if (child.IsType(id))
			return child;

		if (child.IsType<ent::Cluster>()) {
			ent::Cluster	*cluster = child;
			if (cluster->hirez.GetType()->SameAs<anything>()) {
				if (const ISO_ptr<void> &p = FindType(*(anything*)cluster->hirez, id))
					return p;
			}
		} else if (child.IsType<ent::Splitter>()) {
			ent::Splitter *splitter = child;
			if (splitter->hirez.IsType<anything>()) {
				if (const ISO_ptr<void> &p = FindType(*(anything*)splitter->hirez, id))
					return p;
			}
			if (splitter->lorez.IsType<anything>()) {
				if (const ISO_ptr<void> &p = FindType(*(anything*)splitter->lorez, id))
					return p;
			}
		} else if (child.IsType<anything>()) {
			if (const ISO_ptr<void> &p = FindType(*(anything*)child, id))
				return p;
		}
	}
	return ISO::iso_nil<32>;
}

//-----------------------------------------------------------------------------
//	Object
//-----------------------------------------------------------------------------

Object *Attach(Object *parent, Object *child) {
	parent->attach(child);
	if (parent->flags.test(Object::DYNAMIC))
		child->SetMoved();
	parent->Send(AttachMessage(child, true));
	return child;
}

Object *Attach(Object *parent, Object *child, ent::Attachment *at) {
	child->matrix = at->matrix;
	Attach(parent, child);
	return child;
}

void Adopt(Object *parent, Object *child) {
	float3x4 tm = child->GetWorldMat();
	UnLink(child);
	child->matrix = tm / parent->GetWorldMat();
	Attach(parent, child);
}

void UnLink(Object *child) {
	if (Object *parent = child->parent) {
		parent->Send(AttachMessage(child, false));
		child->detach();
	}
}

void Object::Init(param(float3x4) mat) {
#if USE_OBJDIAGNOSTICS
	ISO_TRACEF("Creating object:0x%08x\n", this);
#endif
	matrix	= mat;
	flags	= MAGIC;
	clear(properties);
}

void Object::Init(World *world, Object *_parent, param(float3x4) mat) {
	Init(mat);
	Attach(_parent ? _parent : world, this);
	if (node)
		AddEntitiesArray(world, node->children);
}

Object::~Object() {
#if USE_OBJDIAGNOSTICS
	ISO_TRACEF("Deleting object:0x%08x ", this) << onlyif(node, node.ID()) << " @" << get_trans(matrix) << "\n";
	if (!verify_valid(this))
		ISO_TRACE("BAD!\n");
	flags = flags - MAGIC;
	for (Object *obj = Parent(); obj; obj = obj->Parent()) {
		ISO_TRACEF(" Child of object:0x%08x ", obj) << onlyif(node, node.ID()) << " @" << get_trans(matrix) << "\n";
		if (!verify_valid(obj)) {
			ISO_TRACE("BAD CHILD!\n");
			break;
		}
	}
#else
	ISO_ASSERT(verify_valid(this));
#endif
#if 0
	detach();
	for (Object *t = child, *s; t; t = s) {
		s			= t->sibling;
		t->parent	= 0;
		delete t;
	}
#endif
	Send(DestroyMessage(this));
	if (Pose *pose = Property<Pose>())
		delete pose;
	UnLink(this);
}

float3x4 Object::GetWorldMat() const {
	float3x4	mat = GetMatrix();
	for (const Object *p = parent, *g; p; p = g) {
		if (g = p->parent)
			prefetch(g);
		composite(mat, p->matrix, mat);
	}
	return mat;
}

float3x4 Object::GetBoneMat(crc32 name) const {
	if (Pose *pose = Property<Pose>()) {
		uint8 index = pose->Find(name);
		if (index != Pose::INVALID)
			return pose->GetObjectMatrix(index);
	}
	return identity;
}

float3x4 Object::GetBoneMat(uint8 index) const {
	Pose *pose = Property<Pose>();
	return pose ? pose->GetObjectMatrix(index) : identity;
}

position3 Object::GetWorldPos()	const {
	position3 pos = GetPos();
	for (const Object *p = parent; p; p = p->parent)
		pos = p->GetMatrix() * pos;
	return pos;
}

position3 Object::GetRelativePos(const Object *obj2) const {
	position3	p = GetPos();
	Object		*obj;
	for (obj = parent; obj && obj != obj2; obj = obj->parent)
		p = obj->matrix * p;
	if (obj != obj2)
		p = p / obj2->GetWorldMat();
	return p;
}

float3x4 Object::GetRelativeMat(const Object *obj2) const {
	float3x4	m(matrix);
	Object		*obj;
	for (obj = parent; obj && obj != obj2; obj = obj->parent)
		m = obj->matrix * m;
	if (obj != obj2)
		m = m / obj2->GetWorldMat();
	return m;
}

ent::Attachment *Object::FindAttachmentHier(int id, Object **obj) const {
	for (const_iterator i(this); i; ++i) {
		if (ent::Attachment *attachment = i->FindAttachment(id)) {
			if (obj)
				*obj = const_cast<Object*>(&*i);
			return attachment;
		}
	}
	return NULL;
}

const ISO_ptr<void> &Object::FindTypeHier(tag2 id, Object **obj) const {
	for (const_iterator i(this); i; ++i) {
		if (const ISO_ptr<void> &p = i->FindType(id)) {
			if (obj)
				*obj = const_cast<Object*>(&*i);
			return p;
		}
	}
	return ISO::iso_nil<32>;
}

const ISO_ptr<void> &Object::FindTypeHier(const ISO::Type *type, Object **obj) const {
	for (const_iterator i(this); i; ++i) {
		if (const ISO_ptr<void> &p = i->FindType(type)) {
			if (obj)
				*obj = const_cast<Object*>(&*i);
			return p;
		}
	}
	return ISO::iso_nil<32>;
}

bool Object::AddEntities(World *world, const ISO_ptr<void> &p) {
	if (AddEntity(world, p)) {
		if (Send(PostCreateMessage(this)))
			ClearHandler<PostCreateMessage>();
		return true;
	}
	return false;
}

void Object::AddEntitiesArray(World *world, const anything &a) {
	for (auto i : a)
		AddEntity(world, i);
	if (Send(PostCreateMessage(this)))
		ClearHandler<PostCreateMessage>();
}

void Object::SetMoved(World *world) {
#if 1
	Send(MoveMessage());
#else
	if (!flags.test(DYNAMIC)) {
		for (iterator i(this); i;) {
			if (!i->flags.test_set(DYNAMIC)) {
				world->AddDynamicObject(&*i);
				++i;
			} else {
				i.skip();
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
//	World
//-----------------------------------------------------------------------------

void World::MoveDynamicObjects() {
	MoveMessage m;
	dynamic_objects.send(m);
	if (m.moved_static)
		Send(m);
}

void World::Tick1(float _time) {
	float	dt = _time - time;
	while (!timers.empty() && _time >= timers.begin()->time) {
		Timer	*timer	= timers.pop_front();
		(*timer)(TimerMessage(this, time = timer->time).me());
		delete timer;
	}
	Send(FrameEvent(this, time = _time, dt));
}

void World::Tick2() {
	Send(FrameEvent2(this, time, time - time2));
	time2	= time;
}

void World::Begin() {
	Reset();
	WorldEvent(this, WorldEvent::BEGIN).send();
	Tick1(0);
	Tick2();
}

void World::End() {
	while (!children.empty()) {
		Object	*obj = children.pop_front();
		delete obj;
	}

	ClearNode();

	WorldEvent(this, WorldEvent::END).send();
	timers.deleteall();
	Send(DestroyMessage(this));
	ClearAllHandlers();
}



//-----------------------------------------------------------------------------
//	TypeHandler
//-----------------------------------------------------------------------------

bool Object::AddEntity(const CreateParams &cp, tag2 id, crc32 type, const void *p) {
	CreateMessage	m(cp, id, type, p);
	for (Object *obj = this; obj; obj = obj->Parent()) {
		if (obj->Send(m) && !m.p)
			return true;
	}

	return (m.world->Send(m) && !m.p)
		|| (World::Global()->Send(m) && !m.p);
}

bool Object::AddEntity(World *world, tag2 id, crc32 type, const void *p, param(float3x4) matrix) {
	CreateMessage	m(world, this, matrix, id, type, p);
	for (Object *obj = this; obj; obj = obj->Parent()) {
		if (obj->Send(m) && !m.p)
			return true;
	}

	return (world->Send(m) && !m.p)
		|| (World::Global()->Send(m) && !m.p);
}

bool Object::AddEntity(World *world, const ISO_ptr<void> &p, param(float3x4) matrix) {
	if (p && p.GetType() && !(p.Flags() & ISO::Value::CRCTYPE)) {
		if (AddEntity(world, p.ID(), CRC_type(p.GetType()), (void*)p, matrix))
			return true;
		if (p.IsType<anything>()) {
			for (auto &i : *(anything*)p)
				AddEntity(world, i, matrix);
			return true;
		}
	}
	return false;
}

ISO_ptr<void> LoadParams::Hold(ISO::Browser data, tag2 id) {
	for (auto &i : hold) {
		if (i.ID() == id) {
			if (i.IsType<ent::External>())
				i = data[i.ID()];
			return i;
		}
	}
	if (ISO_ptr<void> p = data[id]) {
		hold.push_back(p);
		return p;
	}
	return ISO_NULL;
}
void LoadParams::Hold0(ISO::Browser data, tag2 id) {
	for (auto &i : hold) {
		if (i.ID() == id)
			return;
	}
	ISO_ptr<ent::External> p(id, id.get_tag());
	hold.push_back(p);
}

void LoadParams::LoadExternals(ISO::Browser data) {
	for (auto &i : hold) {
		if (i.IsType<ent::External>())
			i = data[i.ID()];
	}
}

void LoadParams::PreLoad(tag2 id, crc32 type, const void *p) {
	World::Global()->Send(LoadMessage(*this, id, type, p));
}

void LoadParams::PreLoad(const ISO_ptr<void> &p) {
	if (p && p.GetType() && !(p.Flags() & ISO::Value::CRCTYPE)) {
		if (p.IsType<anything>()) {
			for (auto &i : *(anything*)p)
				PreLoad(i);
		} else {
			PreLoad(p.ID(), CRC_type(p.GetType()), (void*)p);
		}
	}
}

//-----------------------------------------------------------------------------
//	TypeHandler<Node>
//-----------------------------------------------------------------------------

template<> void TypeHandler<Node>::Create(const CreateParams &cp, crc32 id, const Node *t) {
	new Object(cp.world, ISO::GetPtr(t), cp.obj, cp.matrix);
}
TypeHandler<Node>	thNode;

//-----------------------------------------------------------------------------
//	TypeHandler<Children>
//-----------------------------------------------------------------------------

template<> void TypeHandler<Children>::Create(const CreateParams &cp, crc32 id, const Children *t) {
	(new Object(cp.world, cp.matrix, cp.obj))->AddEntitiesArray(cp.world, *t);
}
TypeHandler<Children>	thChildren;

//-----------------------------------------------------------------------------
//	TypeHandler<Scene> (for xref'd proxies)
//-----------------------------------------------------------------------------

template<> void TypeHandler<Scene>::Create(const CreateParams &cp, crc32 id, const Scene *scene) {
	ISO_ptr<Node> node = scene->root;
	if (int count = node->children.Count()) {
		if (node->children[0].IsType<Node>()) {
			if (count == 1 || (count == 2 && node->children[1].IsType<ent::Light2>()))
				node = node->children[0];
		}
		// retain proxy node
		Object *obj = cp.obj;
		if (!obj->GetNode())
			obj->SetNode(cp.world, node);
		else
			obj->AddEntitiesArray(cp.world, node->children);
	}
}
TypeHandler<Scene>	thScene;

//-----------------------------------------------------------------------------
//	TypeHandler<BasePose>
//-----------------------------------------------------------------------------

Pose0 *Pose0::Create(int n) {
	size_t	size	= align(sizeof(Pose0) + n * sizeof(uint8), 16) + n * (sizeof(Joint));
	linear_allocator	a(aligned_alloc(size, 16));
	return new(a) Pose0(a, n);
}

Pose0 *Pose0::Create(const BasePose *bp) {
	return Create(bp->Count())->Init(bp);
}

Pose0::Pose0(linear_allocator &a, int n) {
	count		= n;
	parents		= a.alloc<uint8>(n);
	joints		= a.alloc<Joint>(n);
}

Pose0 *Pose0::Init(const BasePose *bp) {
	float3x4	mats[256];
	for (int i = 0; i < count; i++) {
		joints[i].name	= (uint32)(*bp)[i].ID().get_crc32();
		mats[i]			= (*bp)[i]->basepose;
	}
	for (int i = 0; i < count; i++) {
		parents[i]	= Find((*bp)[i]->parent.ID());
		joints[i]	= parents[i] == INVALID ? mats[i] : mats[i] / mats[parents[i]];
	}
	return this;
}

float3x4 Pose0::GetObjectMatrix(int i) const {
	float3x4	m = joints[i];
	for (i = parents[i]; i != INVALID; i = parents[i])
		m = (float3x4)joints[i] * m;
	return m;
}

float3x4 *Pose0::GetObjectMatrices(float3x4 *objmats) const {
	for (int i = 0; i < count; i++) {
		if (parents[i] == INVALID)
			objmats[i] = joints[i];
		else
			objmats[i] = objmats[parents[i]] * (float3x4)joints[i];
	}
	return objmats;
}

uint8 Pose0::Find(crc32 name, uint8 hint) const {
	for (int i = hint; i < count; i++) {
		if (joints[i].name == name)
			return i;
	}
	for (int i = 0; i < hint; i++) {
		if (joints[i].name == name)
			return i;
	}
	return INVALID;
}

//-------------------------------------

Pose *Pose::Create(const BasePose *bp) {
	int		n		= bp->Count();
	size_t	size	= align(sizeof(Pose) + n * sizeof(uint8), 16) + n * (sizeof(Joint) + 2 * sizeof(float3x4));
	linear_allocator	a(aligned_alloc(size, 16));
	return new(a) Pose(a, bp);
}

Pose::Pose(linear_allocator &a, const BasePose *bp) : Pose0(a, bp->Count()) {
	Init(bp);
	mats		= a.alloc<float3x4>(count);	//new(a, 16) float3x4[n];
	invbpmats	= a.alloc<float3x4>(count);	//new(a, 16) float3x4[n];
	skinmats	= 0;//a.alloc<float3x4>(count);	//new(a, 16) float3x4[n];

	for (int i = 0; i < count; i++)
		mats[i] = joints[i];

	GetObjectMatrices(invbpmats);
	for (int i = 0; i < count; i++) {
		invbpmats[i]	= inverse(invbpmats[i]);
		//skinmats[i]		= identity;
	}
}

float3x4 Pose::GetObjectMatrix(int i) const {
	float3x4	m = mats[i];
	for (i = parents[i]; i != INVALID; i = parents[i])
		m = mats[i] * m;
	return m;
}

float3x4 *Pose::GetObjectMatrices(float3x4 *objmats) const {
	for (int i = 0; i < count; i++) {
		if (parents[i] == INVALID)
			objmats[i] = mats[i];
		else
			objmats[i] = objmats[parents[i]] * mats[i];
	}
	return objmats;
}

void Pose::Reset() {
	for (int i = 0; i < count; i++)
		mats[i] = joints[i];
	skinmats = 0;
}

void Pose::Update() {
	skinmats = 0;
}

void Pose::GetSkinMats(float3x4 *dest) {
#ifdef PLAT_WII
	static
#endif
	float3x4	objmats[256];
	GetObjectMatrices(objmats);
	for (int i = 0; i < count; i++)
		dest[i] = objmats[i] * invbpmats[i];
	skinmats = dest;
}

template<> void TypeHandler<BasePose>::Create(const CreateParams &cp, crc32 id, const BasePose *t) {
	cp.obj->SetProperty(Pose::Create(t));
}
TypeHandler<BasePose>	thBasePose;

//-----------------------------------------------------------------------------
//	Matrix
//-----------------------------------------------------------------------------

struct NodeMatrix {
	union { float3	x; struct { uint32 _0[3], parent;	}; };
	union { float3	y; struct { uint32 _1[3], child;	}; };
	union { float3	z; struct { uint32 _2[3], sibling;	}; };
	union { float3	w; struct { uint32 _3[3], unused;	}; };
	operator const float3x4&() const			{ return *(const float3x4*)this; }
	float3x4	operator=(const float3x4 &m)	{ x = m.x; y = m.y; z = m.z; w = m.w; return *this; }
};

struct NodeMatrices {
	uint32		count;
	uint32		world_index;
	NodeMatrix	*local;
	float3x4	*world[2];

	NodeMatrices(uint32 _count) : count(_count), world_index(0) {
		linear_allocator	a(aligned_alloc((sizeof(NodeMatrix) + sizeof(float3x4) * 2) * count, 16));
		local		= a.alloc<NodeMatrix>(count);
		world[0]	= a.alloc<float3x4>(count);
		world[1]	= a.alloc<float3x4>(count);
	}

	void	Update() {
		world_index			= 1 - world_index;
		float3x4	*dest	= world[world_index];
		NodeMatrix	*srce	= local;
		for (int i = 0; i < count; i++)
			dest[i] = (float3x4)srce[srce[i].parent] * (float3x4)srce[i];
	}

	const float3x4&	Local(uint32 i) const {
		return local[i];
	}
	NodeMatrix&	Local(uint32 i) {
		return local[i];
	}
	const float3x4&	World(uint32 i) const {
		return world[world_index][i];
	}
};

struct ObjectMatrix {
	NodeMatrices	*mats;
	uint32			index;
	operator const float3x4&() const {
		return mats->Local(index);
	}
	NodeMatrix&	Local() {
		return mats->Local(index);
	}
	const float3x4	&World() const {
		return mats->World(index);
	}
};

} // namespace iso
