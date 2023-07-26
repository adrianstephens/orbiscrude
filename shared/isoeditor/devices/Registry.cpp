#include "windows/registry.h"
#include "iso/iso_script.h"

using namespace iso;
using namespace iso::win;

#if 0
#include <winternl.h>
class RegistryExtras {
	dll_function<NTSTATUS __stdcall(
		HANDLE				hKey,
		DWORD				DesiredAccess,
		OBJECT_ATTRIBUTES*	ObjectAttributes,
		DWORD				TitleIndex,
		UNICODE_STRING*		Class,
		DWORD				CreateOptions,
		DWORD*				Disposition
	)>	NtCreateKey;

	dll_function<NTSTATUS __stdcall(
		HANDLE				hKey,
		UNICODE_STRING*		ValueName,
		DWORD				TitleIndex,
		DWORD				Type,
		VOID*				Data,
		DWORD				DataSize
	)>	NtSetValueKey;

	dll_function<NTSTATUS __stdcall(
		HANDLE				hKey
	)>	NtDeleteKey;

	struct UNICODE_STRING2 : UNICODE_STRING {
		UNICODE_STRING2(const char *p) {
			Length	= uint16(strlen(p));
			Buffer	= (WCHAR*)malloc((Length + 1) * sizeof(WCHAR));
			mbstowcs(Buffer, p, Length);
		}
		~UNICODE_STRING2() {
			free(Buffer);
		}
	};

	UNICODE_STRING2	FixName(const char *p) {
		if (istr(p).begins("HKEY_LOCAL_MACHINE\\") || istr(p).begins("HKLM\\"))
			return UNICODE_STRING2(fixed_string<256>("\\Registry\\Machine") + str(p).find('\\'));
		if (istr(p).begins("HKEY_USERS\\") || istr(p).begins("HKUS\\") || istr(p).begins("HKU\\"))
			return UNICODE_STRING2(fixed_string<256>("\\Registry\\User%s") + str(p).find('\\'));
		return p;
	}

public:
	RegistryExtras() {
		HMODULE	ntdll	= GetModuleHandle("ntdll.dll");
		NtCreateKey.bind(ntdll, "NtCreateKey");
		NtSetValueKey.bind(ntdll, "NtSetValueKey");
		NtDeleteKey.bind(ntdll, "NtDeleteKey");
	}

	NTSTATUS  Open(const char *keyname, DWORD access, DWORD opts, DWORD attr, HANDLE *h) {
		UNICODE_STRING2		name	= FixName(keyname);
		OBJECT_ATTRIBUTES	oa		= {sizeof(OBJECT_ATTRIBUTES), NULL, &name, attr, NULL, NULL};
		DWORD				disposition;
		return NtCreateKey(h, access, &oa, 0,  NULL, REG_OPTION_NON_VOLATILE, &disposition);
	}
	NTSTATUS Close(HANDLE h) {
		NtClose(h);
	}

	NTSTATUS  Delete(const char *keyname) {
		HANDLE		h;
		NTSTATUS	status = Open(keyname, KEY_ALL_ACCESS, REG_OPTION_NON_VOLATILE, OBJ_CASE_INSENSITIVE | REG_OPTION_OPEN_LINK, &h);
		if (status == 0)
			status = NtDeleteKey(h);
		return status;
	}
	NTSTATUS CreateLink(const char *keyname, const char *dest, bool opt_volatile) {
		HANDLE		h;
		NTSTATUS	status = Open(keyname, KEY_ALL_ACCESS, (opt_volatile ? REG_OPTION_VOLATILE : REG_OPTION_NON_VOLATILE) | REG_OPTION_CREATE_LINK, OBJ_CASE_INSENSITIVE, &h);
		if (status == 0) {
			UNICODE_STRING2	dest2= FixName(dest);
			status = NtSetValueKey(h, &UNICODE_STRING2("SymbolicLinkValue"), 0, REG_LINK, dest2.Buffer, dest2.Length * 2);
		}
		return status;
	}
};

singleton<RegistryExtras> registry_extras;

class RegKeyExt {
	HANDLE	h;
public:
	RegKeyExt(const char *keyname) : h(0) {
		registry_extras->Open(keyname, KEY_ALL_ACCESS, REG_OPTION_NON_VOLATILE, OBJ_CASE_INSENSITIVE | REG_OPTION_OPEN_LINK, &h);
	}
	~RegKeyExt() {
		NtClose(h);
	}
	operator HANDLE()	const	{ return h;}
};

#endif

//-----------------------------------------------------------------------------
//	Registry
//-----------------------------------------------------------------------------
#define USE_CACHE
#define USE_CACHE2

class Registry : public ISO::VirtualDefaults {
protected:
	RegKey			r;
	uint32			flags;
#ifdef USE_CACHE
	anything		found;
	void			Init();
#else
	def_init<int>	num_sub, num_val;
	void					Init() {
		if (num_sub == 0 && num_val == 0) {
			num_sub	= r.end() - r.begin();
			num_val	= r.values().end() - r.values().begin();
		}
	}
#endif

	static bool				Set(const RegKey &r, ISO_ptr<void> p, bool strings);
	static ISO_ptr<void>	GetValue(const RegKey::Value &v, uint32 flags);

public:
	Registry(HKEY h) : r(h), flags(0)	{}
#ifdef USE_CACHE
	~Registry()				{ found.Clear(); }
#ifdef USE_CACHE2
	ISO::Browser		Deref()	{ Init(); return ISO::MakeBrowser(found); }
#else
	~Registry()				{ found.Clear(); }
	int				Count() {
		Init();
		return found.Count();
	}
	tag				GetName(int i) {
		Init();
		if (found[i])
			return found[i].ID();
		return (r.begin() + i).Name();
	}
	ISO::Browser2	Index(int i) {
		Init();
		if (!found[i]) {
			RegKey::iterator	r2 = r.begin() + i;
			found[i] = ISO_ptr<Registry>(r2.Name(), *r2);
		}
		return found[i];
	}
#endif
	bool			Update(const char *spec, bool from, bool strings = false);
#else
	tag				GetName(int i) {
		Init();
		if (i < num_sub)
			return (r.begin() + i).Name();
		i -= num_sub;
		if (i < num_val)
			return r.values()[i].Name();
		return 0;
	}
	ISO::Browser2	Index(int i) {
		Init();
		if (i < num_sub)
			return ISO_ptr<Registry>((r.begin() + i).Name(), r[i]);
		i -= num_sub;
		if (i < num_val)
			return GetValue(r.values()[i], flags);
		return ISO::Browser2();
	}
	int				Count() {
		Init();
		return num_sub + num_val;
	}
	//int			GetIndex(tag2 id)	{ return found.GetIndex(id); }
#endif
};

bool Registry::Set(const RegKey &r, ISO_ptr<void> p, bool strings) {
	tag				id		= p.ID();
	RegKey::Value	v		= r.values()[id];
	const ISO::Type	*ptype	= p.GetType()->SkipUser();

	switch (v.type) {
		case RegKey::none: {
			if (id && r.HasSubKey(id))
				return false;

			if (ptype->GetType() == ISO::STRING)
				return v = (const char*)ptype->ReadPtr(p);

			if (ptype->SameAs<ISO_openarray<string>>(ISO::MATCH_IGNORE_SIZE)) {
				dynamic_array<const char*>	a = transformc(ISO::Browser2(p), [](const ISO::Browser2& b) { return b.GetString(); });
				return v = multi_string_alloc<char>(a.begin(), a.size());
			}

			if (strings) {
				dynamic_memory_writer	m;
				ISO::ScriptWriter(m).SetFlags(ISO::SCRIPT_ONLYNAMES|ISO::SCRIPT_IGNORE_DEFER).DumpData(p);
				return v = (const char*)m.data();
			}
			if (ptype->IsPlainData())
				return v.set_int(p, ptype->GetSize());

			if (ptype->GetType() == ISO::OPENARRAY) {
				RegKey				r2(r, id, KEY_ALL_ACCESS);
#if 1
				for (auto b : ISO::Browser2(p)) {
					if (!Set(r2, b.IsPtr() ? (ISO_ptr_machine<void>&)b : b.GetType() == ISO::REFERENCE ? (ISO_ptr_machine<void>)*b : b.Duplicate(), strings))
						return false;
				}
#else
				ISO::Browser2		b(p);
				for (int i = 0, n = b.Count(); i < n; i++) {
					ISO::Browser2	b2 = b[i];
					if (!Set(r2, b2.IsPtr() ? (ISO_ptr_machine<void>&)b2 : b2.GetType() == ISO::REFERENCE ? (ISO_ptr_machine<void>)*b2 : b2.Duplicate(), strings))
						return false;
				}
#endif
				return true;
			}

			return false;
		}

		case RegKey::sz:
		case RegKey::expand_sz:
			if (ptype->GetType() == ISO::STRING) {
				return v = (const char*)ptype->ReadPtr(p);

			} else if (p.GetType()->SameAs<ISO_openarray<string>>(ISO::MATCH_IGNORE_SIZE)) {
				buffer_accum<4096>	ba;
#if 1
				ba << separated_list(transformc(ISO::Browser(p), [](const ISO::Browser2& b) { return b.GetString(); }), ";");
#else
				ISO::Browser			b(p);
				for (int i = 0, n = b.Count(); i < n; i++) {
					ba << b[i].GetString();
					if (i < n - 1)
						ba << ';';
				}
#endif
				return v = str(ba);

			} else {
				dynamic_memory_writer	m;
				ISO::ScriptWriter(m).SetFlags(ISO::SCRIPT_ONLYNAMES|ISO::SCRIPT_IGNORE_DEFER).DumpData(p);
				return v = (const char*)m.data();
			}

		case RegKey::multi_sz: {
			if (ptype->GetType() == ISO::STRING) {
				const char *s = (const char*)ptype->ReadPtr(p);
				return v = multi_string_alloc<char>(&s, 1);
			}
			if (ptype->SameAs<ISO_openarray<string>>(ISO::MATCH_IGNORE_SIZE)) {
				dynamic_array<const char*>	a = transformc(ISO::Browser2(p), [](const ISO::Browser2& b) { return b.GetString(); });
				return v = multi_string_alloc<char>(a.begin(), a.size());
			}
			return false;
		}

		case RegKey::binary:
			return v.set(p, ptype->GetSize());

		case RegKey::uint32:
			return v = *(uint32*)p;

		case RegKey::uint32be:
			return v = uint32be(*(uint32*)p);

		case RegKey::uint64:
			return v = *(uint64*)p;

		default:
			return false;
	}
}

template<char SEP> struct parts2 : parts<SEP> {
	typedef parts<SEP>	B;
	typedef typename B::iterator	BI;

	struct iterator : BI {
		iterator(const char *s, const char *e, const char *p) : BI(s, e, p) {}
		iterator&	operator++() {
			if (n != e) {
				p = n + 1;
				while ((n = string_find(n + 1, e, SEP)) && (string_count(p, n, '"') & 1))
					;
				if (!n)
					n = e;
			} else {
				p = n;
			}
			return *this;
		}
		iterator&	operator--() {
			if (p != s) {
				if (p == e)
					++p;
				n = p - 1;
				while ((p = string_rfind(s, p - 1, SEP)) && (string_count(p, n, '"') & 1))
					;
				p = p ? p + 1 : s;
			} else {
				n = p;
			}
			return *this;
		}
	};
	parts2(const char *s)					: B(s)		{}
	parts2(const char *s, const char *e)	: B(s, e)	{}
	iterator	begin()		const { return iterator(s, e, s); }
	iterator	end()		const { return iterator(s, e, e); }
};


ISO_ptr<void> Registry::GetValue(const RegKey::Value &v, uint32 flags) {
	const char		*name	= v.name;
	const ISO::Type	*type	= 0;

	if (const char *colon = str(name).find(':')) {
		type	= ISO::ScriptReadType(name);
		name	= colon + 1;
	}

	if (name && name[0] == 0)
		name = 0;

	switch (v.type) {
		case RegKey::sz:
		case RegKey::expand_sz: {
			string	s = v;

			if (flags & 1) {
				int	n = num_elements32(parts2<';'>(s));
				if (n > 1) {
					return ISO_ptr<ISO_openarray<string> >(name, parts2<';'>(s));
					ISO_ptr<ISO_openarray<string> >	a(name, n);
					string	*d	= *a;
					for (auto &&i : parts<';'>(s))
						*d++ = i;
					return a;
				}
/*				for (parts<';'>::iterator i = p; *i; ++i)  {
					if ((string_count(p, '"', i.n) & 1) == 0) {
						a->Append(str(p, i.n));
						p = i.n + 1;
					}
				}
				if (a)
					return a;
					*/
			}

			return ISO_ptr<string>(name, s);
		}
		case RegKey::binary: {
			ISO_ptr<ISO_openarray<uint8>>	p(name, v.size);
			v.get_raw(*p, v.size);
			return p;
		}

		case RegKey::uint32:
			if (type) {
				ISO_ptr<void>	p = MakePtr(type, name);
				v.get_raw(p, 4);
				return p;
			}
			return ISO_ptr<uint32>(name, v.get<uint32>());

		case RegKey::uint32be:
			return ISO_ptr<uint32>(name, v.get<uint32be>());

		case RegKey::multi_sz: {
			ISO_ptr<ISO_openarray<string> >	a(name);
			string	s = v;
			for (char *t = s; *t; t = t + strlen(t) + 1)
				a->Append(t);
			return a;
		}
		case RegKey::uint64:
			return ISO_ptr<uint64>(name, v.get<uint64>());

		default:
			return ISO_NULL;
	}
}

#ifdef USE_CACHE

void Registry::Init() {
	if (!found) {
	#ifdef  USE_CACHE2
		auto	n = r.size();
		for (auto i = r.begin(KEY_ALL_ACCESS), e = r.end(); i != e; ++i)
			found.Append(ISO_ptr<Registry>(i.Name(), *i));
	#else
		found.Create(r.size());
	#endif
		for (auto i : r.values()) {
			ISO_ptr<void>	p	= GetValue(i, flags);
			if (p)
				found.Append(p);
		}
	}
}

bool Registry::Update(const char *spec, bool from, bool strings) {
	if (from)
		return false;

	int	index = -1;
	if (!spec || !spec[0]) {
		dynamic_bitarray<uint32>	used(found.Count());
		for (auto i = r.begin(), e = r.end(); i != e; ++i) {
			int	j = found.GetIndex(i.Name());
			if (j < 0)
				return RegDeleteTreeA(r, i.Name()) == ERROR_SUCCESS;
			used.set(j);
		}
		for (auto v : r.values()) {
			int	j = found.GetIndex(v.name);
			if (j < 0)
				return RegDeleteValueA(r, v.name) == ERROR_SUCCESS;
			used.set(j);
		}
		index = used.lowest(false);
		if (index == used.size())
			return false;
	} else {
		get_num_base<10>(spec + 1, index);
	}

	ISO_ptr<void>	&p	= found[index];
	tag				id	= p.ID();
	if (id && r.HasSubKey(id))
		return ISO::Browser(p).Update(0, from);

	if (index && !id)
		p.SetID(format_string("item%i", index));

	if (Set(r, p, strings))
		return true;

	found[index] = GetValue(r.values()[index], flags);
	return false;
}

#endif

class RegistryEnv : public Registry {
public:
	RegistryEnv(HKEY h) : Registry(h)	{
		flags = 1;
	}
	bool		Update(const char *spec, bool from) {
		if (Registry::Update(spec, from, true) && !from) {
			DWORD_PTR	ret;
			SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, &ret);
			return true;
		}
		return false;
	}
};

ISO_DEFUSERVIRTF(Registry, ISO::Virtual::DEFER);
ISO_DEFUSERVIRTX(RegistryEnv, "Environment");

//-------------------------------------
#include "device.h"
#include "resource.h"

namespace app {

ISO_ptr_machine<void> GetRegistry(tag id, HKEY h)						{ return ISO_ptr_machine<Registry>(id, h); }
ISO_ptr_machine<void> GetEnvironment(tag id, HKEY h)					{ return ISO_ptr_machine<RegistryEnv>(id, h); }
ISO_ptr_machine<void> GetRegistry(tag id, HKEY h, const char *subkey)	{ return ISO_ptr_machine<Registry>(id, RegKey(h, subkey).detach()); }

struct RegistryDevice : DeviceT<RegistryDevice>, DeviceCreateT<RegistryDevice> {
	void			operator()(const DeviceAdd &add) {
		add("Registry", this, LoadPNG("IDB_DEVICE_REGISTRY"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		ISO_ptr<anything>	registry("registry");
		registry->Append(GetRegistry("HKEY_CLASSES_ROOT",		HKEY_CLASSES_ROOT));
		registry->Append(GetRegistry("HKEY_CURRENT_USER",		HKEY_CURRENT_USER));
		registry->Append(GetRegistry("HKEY_LOCAL_MACHINE",		HKEY_LOCAL_MACHINE));
		registry->Append(GetRegistry("HKEY_USERS",				HKEY_USERS));
		registry->Append(GetRegistry("HKEY_CURRENT_CONFIG",		HKEY_CURRENT_CONFIG));
//		registry->Append(GetRegistry("HKEY_PERFORMANCE_DATA",	HKEY_PERFORMANCE_DATA, KEY_READ));
		return registry;
	}
} registry_device;


//-------------------------------------

struct SettingsDevice : DeviceT<SettingsDevice>, DeviceCreateT<SettingsDevice> {
	void			operator()(const DeviceAdd &add) {
		add("Settings", this, LoadPNG("IDB_DEVICE_SETTINGS"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		return GetRegistry("Settings", RegKey(HKEY_CURRENT_USER, "Software\\Isopod", KEY_ALL_ACCESS).detach());
	}
} settings_device;

//-------------------------------------

struct EnvironmentDevice : DeviceT<EnvironmentDevice>, DeviceCreateT<EnvironmentDevice> {
	void			operator()(const DeviceAdd &add) {
		add("Environment", this, LoadPNG("IDB_DEVICE_CONSOLE"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		ISO_ptr<anything>	env("environment");
		env->Append(GetEnvironment("user",		RegKey(HKEY_CURRENT_USER,  "Environment", KEY_ALL_ACCESS).detach()));
		env->Append(GetEnvironment("system",	RegKey(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Session Manager\\Environment", KEY_ALL_ACCESS).detach()));
		return env;
	}
} environment_device;

}

