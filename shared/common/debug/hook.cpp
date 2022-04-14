#include "debug.h"
#include "hook.h"
#include "base/tree.h"
#include "base/algorithm.h"
#include "base/bits.h"
#include "base/functions.h"

#include <dlfcn.h>

namespace iso {

//-----------------------------------------------------------------------------
//	Hooks
//-----------------------------------------------------------------------------

bool Hook::Apply(void **IATentry) {
	return false;
}

bool Hook::Remove(void **IATentry) {
	return false;
}

struct ModuleHooks : Module {
	e_list<Hook>	hooks;
	ModuleHooks() {}

	void	Add(Hook *hook) {
		hooks.push_back(hook);
	}
//	void	Remove(const char *f, void **o, void *d) {
//		Hook	&h = hooks[f];
//		if (h.orig)
//			h.dest = *h.orig;
//	}

	Hook	*Find(const char *function) {
		auto	found = find_if(hooks, [function](Hook &h) { return str8(h.name) == function; });
		return found != hooks.end() ? found.get() : nullptr;
	}

};

class Hooks {
public:
	map<string, ModuleHooks>	modules;
	static bool					any;

	void	Add(const char *module_name, Hook *hook) {
		modules[to_lower(module_name)].Add(hook);
		any = true;
	}
	Hooks();
};

bool	Hooks::any = false;
singleton<Hooks> hooks;

Hooks::Hooks() {
}

void ApplyHooks() {
//	if (Hooks::any)
//		hooks->ProcessAll(true);
}

void AddHook(const char *module_name, Hook *hook) {
	hooks->Add(module_name, hook);
}

} // namespace iso
