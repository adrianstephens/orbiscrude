#ifndef PLUGIN_H
#define PLUGIN_H

#include "base/defs.h"

namespace iso {

class Plugin : public static_list<Plugin> {
public:
	virtual	const char*		GetDescription()=0;
};

} // namespace iso

#endif
