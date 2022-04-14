#ifndef HOOK_H
#define HOOK_H

#include "base/defs.h"
#include "base/list.h"
#include "base/strings.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Hook
//-----------------------------------------------------------------------------

struct Hook : link_base<Hook> {
	const char	*name;
	void		*orig;
	void		*dest;

	Hook *set(const char *_name, void *_dest) {
		name		= _name;
		dest		= _dest;
		orig		= 0;
		return this;
	}

	bool		Apply(void **IATentry);
	bool		Remove(void **IATentry);
};

template<typename F, F *f> struct T_Hook : Hook {
	F	*orig()								{ return (F*)Hook::orig; }
	Hook *set(const char *_name, F *_dest)	{ return Hook::set(_name, (void*)_dest); }
};

template<typename F> struct hook_helper {
	template<F *f> static auto get() {
		static T_Hook<F,f>	hook;
		return &hook;
	}
};

void		AddHook(const char *module_name, Hook *hook);
void		RemoveHook(void **orig, const char *function, const char *module_name, void *dest);
void*		HookImmediate(const char *function, const char *module_name, void *dest);
void		ApplyHooks();

#define get_hook(f)				hook_helper<decltype(f)>::get<f>()
#define get_orig(f)				hook_helper<decltype(f)>::get<f>()->orig()
#define hook(f, module_name)	AddHook(module_name, get_hook(f)->set(#f, Hooked_##f))

} // namespace iso

#endif // HOOK_H

