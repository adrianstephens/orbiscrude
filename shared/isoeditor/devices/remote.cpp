#include "iso/iso_binary.h"
#include "systems/communication/connection.h"
#include "stream.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Remote
//-----------------------------------------------------------------------------

class Remote : public ISO::VirtualDefaults {
public:
	ISO_ptr<void>	p;
	const char		*target;
	bool			Init(const char *_target, const char *spec);
	~Remote()	{}
public:
//	int			Count()				{ return p.GetType()->GetType() == ISO::REFERENCE ? 1 : ISO::Browser(p).Count();		}
	int			Count()				{ return ISO::Browser(p).Count();		}
	tag			GetName(int i = 0)	{ return ISO::Browser(p).GetName(i);		}
	int			GetIndex(tag id, int from)	{ return ISO::Browser(p).GetIndex(id, from);	}
	ISO::Browser	Index(int i);
	bool		Update(const char *spec, bool from);
};

ISO_DEFVIRT(Remote);

bool Remote::Init(const char *_target, const char *spec) {
	target = _target;

	uint32be			sizebe;
	isolink_handle_t	handle = SendCommand(target, ISO_ptr<const char*>("Get", spec));
	if (handle != isolink_invalid_handle && ReceiveCheck(target, handle, &sizebe, 4)) {
		malloc_block	buffer(sizebe);
		if (isolink_receive(handle, buffer, sizebe) == sizebe) {
			ISO::binary_data.SetRemoteTarget(target);
			this->p = ISO::binary_data.Read(spec, lvalue(memory_reader(buffer)));
		}
		isolink_close(handle);
		return true;
	}
	return false;
}

ISO::Browser Remote::Index(int i) {
	ISO::binary_data.SetRemoteTarget(target);
	return ISO::Browser(p)[i];
	ISO::Browser b(p);
	if (p.GetType()->GetType() == ISO::REFERENCE)
		return b;
	return b[i];
}

bool Remote::Update(const char *spec, bool from) {
	fixed_string<256>	spec2	= p.ID().get_tag() + spec;
	ISO::Browser2		b		= ISO::Browser2(p).Parse(spec);

	if (!from) {
		if (!b.IsPtr())
			b = b.GetType() == ISO::REFERENCE ? *b : ISO::Browser2(b.Duplicate());
		return isolink_close(SendCommand(target, ISO_ptr<pair<const char*, ISO_ptr<void> > >("Set",
			make_pair((const char*)spec2, b.GetPtr())
		)));
	}

	uint32be			sizebe;
	isolink_handle_t	handle = SendCommand(target, ISO_ptr<const char*>("Get", (const char*)spec2));
	if (handle != isolink_invalid_handle && ReceiveCheck(target, handle, &sizebe, 4)) {
		malloc_block	buffer(sizebe);
		if (isolink_receive(handle, buffer, sizebe) == sizebe) {
			ISO::binary_data.SetRemoteTarget(target);
			ISO_ptr<void>	p2 = ISO::binary_data.Read(0, lvalue(memory_reader(buffer)));
			if (b.GetType() == ISO::REFERENCE)
				*(ISO_ptr<void>*)b = p2;
		}
		isolink_close(handle);
		return true;
	}
	return false;
}

const char *GetSpec(Remote *r) { return r->p.ID().get_tag(); }

ISO_ptr<void> GetRemote(tag2 id, const char *target, const char *spec) {
	ISO_ptr<Remote>	p(id);
	if (p->Init(target, spec))
		return p;
	return ISO_NULL;
}
ISO_ptr<void> GetRemote(tag2 id, Remote *r, const char *spec) {
	ISO_ptr<Remote>	p(id);
	if (p->Init(r->target, fixed_string<256>(r->p.ID().get_tag()) + spec))
		return p;
	return ISO_NULL;
}
ISO_ptr<void> GetRemote(tag2 id, ISO_ptr<void> p, const char *spec) {
	ISO_ptr<Remote>	p2(id);
	Remote	*r	= p;
	if (p2->Init(r->target, fixed_string<256>(r->p.ID().get_tag()) + spec))
		return p2;
	return ISO_NULL;
}

//-----------------------------------------------------------------------------
//	IsoLinkDevice
//-----------------------------------------------------------------------------

#include "device.h"
#include "utilities.h"
#include "thread.h"

#ifdef PLAT_PC
#include "windows/registry.h"
#endif

using namespace app;

struct IsoLinkDevice : DeviceT<IsoLinkDevice>, MenuCallbackT<IsoLinkDevice> {
	int	id;

	struct Target : DeviceCreateT<Target>, Handles2<Target, AppEvent> {
		string id;
		string target;
		ISO_ptr<void>	operator()(const Control &main) {
			Busy			bee;
			ISO_ptr<void>	p = GetRemote(id, target, "");
			if (!p)
				(throw_accum(isolink_get_error()));
			return p;
		}
		void	operator()(AppEvent *ev) {
			if (ev->state == AppEvent::END)
				delete this;
		}
		Target(const char *_id, const char *_target) : id(_id), target(_target)	{}
	};
	void			operator()(const DeviceAdd &add) {
		id	= add.id;
		auto	sub		= add("IsoLink Target");
		auto	lambda	= [&sub](const char *target, isolink_platform_t platform) {
			sub(target, new Target(target, target));
			return false;
		};
		isolink_enum_hosts(callback_function_end<bool(const char*, const char*)>(&lambda), ISOLINK_ENUM_PLATFORMS, &lambda);
		sub("Enumerate All", this);
	}

	void			operator()(Control c, Menu m) {
		auto	add		= DeviceAdd(m, id);
		auto	lambda	= [&add](const char *target, isolink_platform_t platform) {
			add(target, new Target(target, target));
			return false;
		};

	#ifdef PLAT_PC
		while (m.Count()) {
			delete (Target*)m.GetItemByPos(0, MIIM_DATA).Param();
			m.RemoveByPos(0);
		}
	#endif
		RunThread([&add, &lambda]() {
	#ifdef PLAT_PC
			string	targets = RegKey(HKEY_CURRENT_USER, "Software\\Isopod\\IsoLink").values()["targets"];
			for (char *p = targets.begin(), *n; p; p = n) {
				if (n = strchr(p, ';'))
					*n++ = 0;
				if (*p)
					add(p, new Target(p, p));
			}
	#endif
			isolink_enum_hosts(callback_function_end<bool(const char*, const char*)>(&lambda), ISOLINK_ENUM_DEVICES, &lambda);
		});
	}
} isolink_device;

