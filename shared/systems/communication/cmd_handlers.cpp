#if USE_CMDLINK

#include "app.h"
#include "stream.h"
#include "iso/iso_binary.h"
#include "iso/iso_convert.h"
#include "vector_string.h"
#include "object.h"
#include "utilities.h"
#include "base/list.h"
#include "collision/dynamics.h"
#include "mesh/model.h"
#include "mesh/light.h"
#include "communication/cmd_link.h"
#include "render.h"
#include "thread.h"
#include "bitmap/bitmap.h"

using namespace iso;

struct TTY {};
ISO_DEFUSER(TTY, void);

#if defined PLAT_WII
struct GPUState : ISO_ptr<xint32> { GPUState(const GPState *s) : ISO_ptr<xint32>(0, uint32(s)) {} };
ISO_DEFUSERX(GPUState, ISO_ptr<xint32>, EXP_STRINGIFY(ISO_PLATFORM) "GPUState");
#endif

#if 0//defined PLAT_IOS
struct SurfaceSender : HandlesGlobal<SurfaceSender, RenderEvent> {
	Semaphore	sem;
	bitmap		*bm;
	const Surface	&s;
	TexFormat		f;

	void operator()(RenderEvent *re, uint32 stage) {
		uint32	width	= s.Width(), height = s.Height();
		bool	depth	= f >= _TEXF_DEPTH;
		re->ctx.SetRenderTarget(Surface(), depth ? RT_COLOUR0 : RT_DEPTH);
		re->ctx.SetRenderTarget(s, depth ? RT_DEPTH : RT_COLOUR0);
		re->ctx.ReadPixels(f, rect(0, 0, width, height), bm->Create(width, height));
		sem.unlock();
	}

	void operator()(RenderEvent *re) {
		if (!re->Excluded(RMASK_NOSHADOW))
			re->AddRenderItem(this, MakeKey(RS_LAST, 0), 0);
	}

	SurfaceSender(bitmap *_bm, const Surface &_s, TexFormat _f) : bm(_bm), s(_s), f(_f), sem(0) {
		sem.lock();
	}
};

struct IOSSurface : ISO::VirtualDefaults {
	const Surface	&s;
	IOSSurface(const Surface &_s) : s(_s) {}
	ISO::Browser2	Index(int i) {
		ISO_ptr<bitmap>	bm(0);
		return SurfaceSender(bm, s, TEXF_R8G8B8A8), bm;
	}
};
ISO_DEFUSERVIRT(IOSSurface);
struct IOSTexture2 : ISO::VirtualDefaults {
	const Texture	&s;
	IOSTexture2(const Texture &_s) : s(_s) {}
	ISO::Browser2	Index(int i) {
		ISO_ptr<bitmap>	bm(0);
		return SurfaceSender(bm, s.GetSurface(), s.Format()), bm;
	}
};
ISO_DEFUSERVIRT(IOSTexture2);
#endif


#if USE_PROFILER && !defined(USE_TELEMETRY)
ISO_DEFUSERX(Profiler::Marker::colour, uint8[4], "rgba8");

struct ISOProfileMarker : public Profiler::Marker, ISO::VirtualDefaults {
	static Time::type	to_microsecs(Time::type t)	{
		static rational<Time::type> f = rational<Time::type>::normalised(1000000, Time::get_freq());
		return f * t;
	}

	int					Count()			const {
		return num_elements(children) + 3;
	}
	tag					GetName(int i)	const {
		switch (i) {
			case 0:		return "time";
			case 1:		return "count";
			case 2:		return "colour";
			default:	return nth(children.begin(), i - 3)->label;
		}
	}
	ISO::Browser2		Index(int i)	const {
		switch (i) {
			case 0:		return ISO_ptr<Time::type>(0, to_microsecs(total));
			case 1:		return ISO::MakeBrowser(num_calls);
			case 2:		return ISO::MakeBrowser(col);
			default:	return ISO::MakeBrowser((ISOProfileMarker&)*nth(children.begin(), i - 3));
		}
	}
};

ISO_DEFUSERVIRTX(ISOProfileMarker, "ProfileData");

struct ISO_Profiler : pointer<ISOProfileMarker> {
	ISO_Profiler(ISOProfileMarker *p) : pointer<ISOProfileMarker>(p)	{}
};
ISO_DEFUSERX(ISO_Profiler, pointer<ISOProfileMarker>, "Profiler");
#endif


#if defined PLAT_IOS
/*void IsoLinkAdd(tag2 id, const Surface &s) {
	ISO::root.Add(MakePtr(id, new IOSSurface(s)));
}
void IsoLinkAdd(tag2 id, const Texture &s) {
	ISO::root.Add(MakePtr(id, new IOSTexture2(s)));
}
*/
#else
void IsoLinkAdd(tag2 id, const Texture &s) {
	ISO::root().Add(MakePtr(id, &s));
}
#endif

//-----------------------------------------------------------------------------
//	CmdLinkInit
//-----------------------------------------------------------------------------

static struct CmdLinkInit : public Handles2<CmdLinkInit, AppEvent> {
	void operator()(AppEvent *ev) {
		if (ev->state == AppEvent::BEGIN) {
			ISO::root().Add(ISO_ptr<TTY>("tty"));
		#if defined PLAT_IOS
			//IsoLinkAdd("dispbuffer", graphics.GetDispSurface());
		#elif defined PLAT_PS3 || defined PLAT_X360
			//IsoLinkAdd("dispbuffer", graphics.GetDispTexture());
		#endif
		#if defined PLAT_WII
			GPUState	p(graphics.GetState());
			ISO::root.Add(ISO_ptr<GPUState>("gpu", p));
		#endif
		#if USE_PROFILER && !defined(USE_TELEMETRY)
			ISO::root.Add(ISO_ptr<ISO_Profiler>("profiler", (ISOProfileMarker*)&Profiler::instance->head));
		#endif
		}
	}
} init;

//-----------------------------------------------------------------------------
//	Low level handlers
//-----------------------------------------------------------------------------
template<> bool CmdHandler<ISO_CRC("ASyncFlush", 0x8cd9351a)>::Process(ISO::Browser b, bool immediate) {
	CmdLinkFlush();
	return true;
}
static CmdHandler<ISO_CRC("ASyncFlush", 0x8cd9351a)> chASyncFlush;

#if !defined(PLAT_PC) && !defined(PLAT_PS4)
namespace iso {
	struct Texture2 : ISO_ptr<TexturePlat> {};
}
ISO_DEFUSERX(Texture2, ISO_ptr<TexturePlat>, "Texture");
#endif

ISO_ptr<void> Browser2Ptr0(const ISO::Browser2 &b) {
	if (b.IsPtr())
		return b;
	return b.Duplicate();
}

struct unknown : ISO::VirtualDefaults {};
ISO_DEFVIRT(unknown);

ISO_ptr<void> Browser2Ptr(const ISO::Browser2 &b, bool all) {
	if (b.SkipUser().GetType() == ISO::VIRTUAL) {
#if !defined(PLAT_PC) && !defined(PLAT_PS4)
		if (b.Is<pointer<Texture> >()) {
			ISO_ptr<Texture2> p(0);
			memcpy(p->Create(), *(ISO_ptr<TexturePlat>*)*b, sizeof(TexturePlat));
//			memcpy(p->Create(), **(TexturePlat***)b, sizeof(TexturePlat));
			return p;
		}
#endif
		int		n		= b.Count();
		bool	dir		= b.Is("Directory");
//		bool	profile = b.Is("ProfileData");
		ISO_ptr<anything>	p(0, n);
		for (int i = 0; i < n; i++) {
			ISO_ptr<void>&	e	= (*p)[i];
			tag2			id	= b.GetName(i);
			if (all) {
				e = Browser2Ptr(b[i], all);
				if (!e.ID())
					e.SetID(id);
			} else if (dir) {
				e = ISO_ptr<unknown>(id);
			} else {
				e = Browser2Ptr0(b[i]);
				if (!e.ID())
					e.SetID(id);
			}
		}
		return p;
	}
	if (b.IsPtr())
		return b;
	return b.Duplicate();
}

bool GetResponse(isolink_handle_t handle, ISO::Browser b, bool all) { PROFILE_FN
	dynamic_memory_writer	file;
	ISO::Browser2	b2	= ISO::root().Parse(b.GetString());
	if (b2.GetType() == ISO::REFERENCE)
		b2 = *b2;
	ISO_binary().Write(Browser2Ptr(b2, all), file, 0, ISO::BIN_EXPANDEXTERNALS | ISO::BIN_DONTCONVERT | ISO::BIN_WRITEALLTYPES | ISO::BIN_STRINGIDS);
	uint32be		len = file.size32();
	if (isolink_send(handle, &len, sizeof(len)))
		isolink_send(handle, file.data(), len);
	isolink_close(handle);
	return true;
}


template<> bool CmdHandler<ISO_CRC("SendFile", 0x34a0b19b)>::Process(isolink_handle_t handle, ISO::Browser b, bool immediate) {
	ISO_OUTPUTF("Send File: %s (%i bytes)\n", b[0].GetString(), b[1].Count());
	filename	fn	= DocsDir().add_dir(b[0].GetString());
	FileOutput	file(fn);
	file.writebuff(b[1][0], b[1].Count());
	isolink_close(handle);
	ISO_OUTPUT("File saved");
	ISO::root("data").Update(fn.name_ext());
	return true;
}
static CmdHandler<ISO_CRC("SendFile", 0x34a0b19b)> chSendFile;

template<> bool CmdHandler<ISO_CRC("Get", 0xc5766890)>::Process(isolink_handle_t handle, ISO::Browser b, bool immediate) {
	return GetResponse(handle, b, false);
}
static CmdHandler<ISO_CRC("Get", 0xc5766890)> chGet;

template<> void CmdHandler<ISO_CRC("Set", 0xde59633c)>::Process(ISO::Browser b/*, bool immediate*/) {
	Assign(ISO::root().Parse(b[0].GetString()), b[1]);
}
static CmdHandler<ISO_CRC("Set", 0xde59633c)> chSet;

template<> void CmdHandler<ISO_CRC("GetAll", 0xea04baae)>::Process(isolink_handle_t handle, ISO::Browser b/*, bool immediate*/) {
	GetResponse(handle, b, true);
}
static CmdHandler<ISO_CRC("GetAll", 0xea04baae)> chGetAll;

template<> bool CmdHandler<ISO_CRC("GetMemory", 0x80d5b027)>::Process(isolink_handle_t handle, ISO::Browser b, bool immediate) {
	uintptr_t	addr	= b[0].GetInt();
	uint32		size	= (uint32)b[1].GetInt();
	if (immediate && (size & 0x80000000))
		return false;
	isolink_send(handle, (void*)addr, size & 0x7fffffff);
	isolink_close(handle);
	return true;
}
static CmdHandler<ISO_CRC("GetMemory", 0x80d5b027)> chGetMemory;

template<> void CmdHandler<ISO_CRC("MemoryInterface", 0x065f9d3b)>::Process(isolink_handle_t handle, ISO::Browser b) {
	isolink_send(handle, "OK", 2);
	pair<void*, uint32>	req;
	while (isolink_receive(handle, &req, sizeof(req)) == sizeof(req) && req.b > 0) {
		ISO_OUTPUTF("transfer: 0x%08x:0x%08x\n", req.a, req.b);
		isolink_send(handle, req.a, req.b);
	}
	isolink_close(handle);
}
static CmdHandler<ISO_CRC("MemoryInterface", 0x065f9d3b)> chMemoryInterface;

#ifdef PLAT_IOS

int pipe_file(int fd, int &save) {
	int		pipefds[2];
	int		r = pipe(pipefds);
	if (r < 0)
		return r;

	save = dup(fd);
	dup2(pipefds[1], fd);
	return pipefds[0];
}

void unpipe_file(int fd, int save) {
	dup2(save, fd);
	close(save);
}

struct PipeThread : Thread {
	int					pipe, orig;
	isolink_handle_t	handle;
	int operator()() {
		for (;;) {
			char	buffer[256];
			auto	r = ::read(pipe, buffer, sizeof(buffer));
			if (!isolink_send(handle, buffer, r)) {
				unpipe_file(STDERR_FILENO, orig);
				::write(STDERR_FILENO, buffer, r);
				delete this;
				break;
			}
		}
		return 0;
	}
	PipeThread(isolink_handle_t _handle) : Thread(this), handle(_handle), pipe(pipe_file(STDERR_FILENO, orig)) {
		Start();
	}
};

template<> bool CmdHandler<ISO_CRC("GetDebugOut", 0xd635b307)>::Process(isolink_handle_t handle, ISO::Browser b, bool immediate) {
	new PipeThread(handle);
	return true;
}

#else

_iso_debug_print_t isolink_debug_print_old;

void isolink_debug_print(void *params, const char *buffer) {
	if (!isolink_send((isolink_handle_t)intptr_t(params), buffer, strlen(buffer))) {
		_iso_set_debug_print(isolink_debug_print_old);
		isolink_debug_print_old(buffer);
	}
}
template<> bool CmdHandler<ISO_CRC("GetDebugOut", 0xd635b307)>::Process(isolink_handle_t handle, ISO::Browser b, bool immediate) {
	isolink_debug_print_old		= _iso_set_debug_print({isolink_debug_print, (void*)(intptr_t)handle});
	return true;
}
#endif
static CmdHandler<ISO_CRC("GetDebugOut", 0xd635b307)> chGetDebugOut;

//-----------------------------------------------------------------------------
//	ObjectMap
//-----------------------------------------------------------------------------
const size_t map_search_depth = 5;

static class ObjectMap : public Handles2<ObjectMap, WorldEvent> {
	// Entry
	struct Entry : e_link<Entry> {
		crc32 id;
		ObjectReference obj;
		Entry(crc32 _id, Object *_obj)
			: id(_id)
			, obj(_obj)
		{}
	};
	typedef e_list<Entry> ObjectCache;
	mutable ObjectCache cache;

	Object* Lookup(Object *obj, crc32 id, size_t depth) const;
	Object* Lookup(crc32 id, size_t depth) const;

public:
	Object* Lookup(crc32 id) const;
	void Append(crc32 id, Object *obj) {
		cache.push_front(new Entry(id, obj));
	}
	void operator()(WorldEvent *ev) {
		if (ev->state == WorldEvent::END) {
			while (!cache.empty())
				delete cache.pop_front();
		}
	}
} object_map;

Object* ObjectMap::Lookup(Object *obj, crc32 id, size_t depth) const {
	// enum
	Object *_obj, *root = obj->Parent();
	do {
		// match
		if (obj->GetName().get_crc32() == id)
			return obj;
		// child
		if (depth && !obj->children.empty()) {
			--depth;
			obj = obj->child();
		// sibling
		} else if (_obj = obj->next) {
			obj = _obj;
		} else {
			// backup
			while ((obj = obj->Parent()) != root) {
				++depth;
				if (_obj = obj->next) {
					obj = _obj;
					break;
				}
			}
		}
	} while (obj != root);
	return NULL;
}

Object* ObjectMap::Lookup(crc32 id, size_t depth) const {
	// scene
	Object *obj	= Lookup(World::Current(), id, depth);
	if (!obj) {
		for (auto &i : World::Current()->children) {
			if (obj = Lookup(&i, id, depth + 3))
				break;
		}
	}
	return obj;
}

Object* ObjectMap::Lookup(crc32 id) const {
	// cache
	ObjectCache::iterator iter = cache.begin();
	while (iter != cache.end()) {
		// dead
		if (!iter->obj) {
			ObjectCache::iterator _iter = iter++;
			delete _iter->unlink();
			continue;
		}
		// match, reorder
		if (iter->id == id) {
			if (iter != cache.begin())
				cache.push_front(iter->unlink());
			return iter->obj;
		}
		++iter;
	}
	// scene fallback
	if (Object *obj = Lookup(id, map_search_depth)) {
		cache.push_front(new Entry(id, obj));
		return obj;
	}
	return NULL;
}

// TransformObject
template<> void CmdHandler<ISO_CRC("TransformObject", 0xe805f392)>::Process(ISO::Browser b) {
	if (Object *obj = object_map.Lookup(b.GetMember(ISO_CRC("id", 0xbf396750)).GetString())) {
		float3x4 tm = *(float3x4p*)b.GetMember(ISO_CRC("matrix", 0xf83341cf));
		if (RigidBody *rb = obj->Property<RigidBody>()) {
			rb->SetOrientation(get_rot(tm));
			rb->SetPosition(get_trans(tm));
			rb->SetVelocity(vector3(zero));
			rb->SetAngularVelocity(vector3(zero));
		} else
			obj->SetMatrix(tm);
	}
}
static CmdHandler<ISO_CRC("TransformObject", 0xe805f392)> chTransformObject;

// CloneObject
template<> void CmdHandler<ISO_CRC("CloneObject", 0x6e00ba3b)>::Process(ISO::Browser b) {
	Object *_obj, *obj = object_map.Lookup(b.GetMember(ISO_CRC("id", 0xbf396750)).GetString());
	if (obj && obj->GetNode()) {
		(_obj = new Object(obj->GetNode(), obj->Parent()))->SetMatrix(obj->GetMatrix());
		object_map.Append(b.GetMember(ISO_CRC("clone", 0xec6dedd8)).GetString(), _obj);
	}
}
static CmdHandler<ISO_CRC("CloneObject", 0x6e00ba3b)> chCloneObject;

// DeleteObject
template<> void CmdHandler<ISO_CRC("DeleteObject", 0x5b4ba3de)>::Process(ISO::Browser b) {
	if (Object *obj = object_map.Lookup(b.GetMember(ISO_CRC("id", 0xbf396750)).GetString()))
		deferred_delete(obj);
}
static CmdHandler<ISO_CRC("DeleteObject", 0x5b4ba3de)> chDeleteObject;

//namespace iso {
//	ISO_DEFUSER(Light, void);
//}

// ModifyLight
template<> void CmdHandler<ISO_CRC("ModifyLight", 0x302e70f9)>::Process(ISO::Browser b) {
	crc32	id = b.GetMember(ISO_CRC("id", 0xbf396750)).GetString();
	Light	light;
	light.col		= colour(*(float3p*)b.GetMember(ISO_CRC("colour", 0xfaf865ce)), one);
	light.range		= b.GetMember(ISO_CRC("range", 0x93875a49)).GetFloat();
	light.spread	= b.GetMember(ISO_CRC("spread", 0x7cb5a078)).GetFloat();
	if (float3x4p *mat = (float3x4p*)b.GetMember(ISO_CRC("matrix", 0xf83341cf)))
		light.matrix = *mat;
	World::Current()->SetItem(id, &light);
}
static CmdHandler<ISO_CRC("ModifyLight", 0x302e70f9)> chModifyLight;
/*
string_accum& operator<<(string_accum &a, float v) {
	return a.format("%g", v);
}
string_accum& operator<<(string_accum &a, param(float3) v) {
	return a << "{" << float(v.x) << ' ' << float(v.y) << ' ' << float(v.z) << "}";
}
string_accum& operator<<(string_accum &a, param(float3x4) m) {
	return a << "{" << m.x << m.y << m.z << float3(m.w) << "}";
}
*/
// RetrieveLight
template<> void CmdHandler<ISO_CRC("RetrieveLight", 0xb9a7cd42)>::Process(isolink_handle_t handle, ISO::Browser b) {
	// lookup
	buffer_accum<256> buffer;
	crc32 id = b.GetMember(ISO_CRC("id", 0xbf396750)).GetString();
	if (const Light *light = World::Current()->GetItem<Light>(id)) {
		buffer
			<< "colour = " << light->col.rgb << " "
			<< "range = " << light->range << " "
			<< "spread = " << light->spread << " "
			<< "matrix = " << light->matrix;
	}

	// respond
	uint32be buffer_len = buffer.size32() + 1;
	if (isolink_send(handle, &buffer_len, sizeof(buffer_len)))
		isolink_send(handle, buffer, buffer_len);
	isolink_close(handle);
}
static CmdHandler<ISO_CRC("RetrieveLight", 0xb9a7cd42)> chRetrieveLight;

// CreateLight
template<> void CmdHandler<ISO_CRC("CreateLight", 0xf0c31156)>::Process(ISO::Browser b) {
	World::Current()->AddEntity(*b[0]);
}
static CmdHandler<ISO_CRC("CreateLight", 0xf0c31156)> chCreateLight;

// ModifyMaterial
template<> void CmdHandler<ISO_CRC("ModifyMaterial", 0x6ea5c7df)>::Process(ISO::Browser b) {
	const char*	material_id		= b[0].GetString();
	crc32		parameter_crc	= (b = b[1]).GetName();//)).as<uint32>();
	uint32		parameter_id	= parameter_crc;
	const ISO::Type *type		= (b = *b).GetTypeDef();
	switch (type->GetType()) {
		case ISO::INT: ISO_OUTPUTF("ModifyMaterial(%s): %#08x=%i\n", material_id, parameter_id, b.GetInt()); break;
		case ISO::FLOAT: ISO_OUTPUTF("ModifyMaterial(%s): %#08x=%g\n", material_id, parameter_id, b.GetFloat()); break;
		default: {
			if (type == ISO::getdef<float3p>()) {
				float3p *v = b;
				ISO_OUTPUTF("ModifyMaterial(%s): %#08x={%g %g %g}\n", material_id, parameter_id, v->x, v->y, v->z); break;
			} else if (type == ISO::getdef<float4p>()) {
				float4p *v = b;
				ISO_OUTPUTF("ModifyMaterial(%s): %#08x={%g %g %g %g}\n", material_id, parameter_id, v->x, v->y, v->z, v->w); break;
			}
		}
	}
}
static CmdHandler<ISO_CRC("ModifyMaterial", 0x6ea5c7df)> chModifyMaterial;

#endif // USE_CMDLINK
