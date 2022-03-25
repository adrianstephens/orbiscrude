#include "utilities.h"
#include "base/algorithm.h"
#include "shader.h"
#include "crc32.h"

namespace iso {

deferred_deletes _dd, cleanup_dd;
rng<simple_random>				random;
rng<mersenne_twister32_19937>	random2(123456789);

static const char *usage_names[] = {
	"POSITION",
	"NORMAL",
	"COLOR",
	"TEXCOORD",
	"TANGENT",
	"BINORMAL",
	"BLENDWEIGHT",
	"BLENDINDICES",
};

static const struct { crc32 id; USAGE usage; } known_uses1[] = {
	{"pos",			USAGE_POSITION 		},
	{"position",	USAGE_POSITION 		},
	{"norm",		USAGE_NORMAL		},
	{"normal",		USAGE_NORMAL		},
	{"colour",		USAGE_COLOR			},
	{"color",		USAGE_COLOR			},
	{"col",			USAGE_COLOR			},
	{"texcoord",	USAGE_TEXCOORD		},
	{"uv",			USAGE_TEXCOORD		},
	{"tangent",		USAGE_TANGENT		},
	{"binormal",	USAGE_BINORMAL		},
	{"weights",		USAGE_BLENDWEIGHT	},
	{"bones",		USAGE_BLENDINDICES	},
	{"indices",		USAGE_BLENDINDICES	},
	{"POSITION",	USAGE_POSITION 		},
	{"NORMAL",		USAGE_NORMAL		},
	{"COLOR",		USAGE_COLOR			},
	{"TEXCOORD",	USAGE_TEXCOORD		},
	{"TANGENT",		USAGE_TANGENT		},
	{"BINORMAL",	USAGE_BINORMAL		},
	{"BLENDWEIGHT",	USAGE_BLENDWEIGHT	},
	{"BLENDINDICES",USAGE_BLENDINDICES	},
};

static const struct { crc32 id; USAGE2 usage; } known_uses2[] = {
	{"centre",			{USAGE_POSITION, 	1}},
	{"smooth_normal",	{USAGE_NORMAL,		1}},
};

USAGE2::USAGE2(crc32 id) : usage(USAGE_UNKNOWN), index(0) {
	for (auto &i : known_uses1) {
		if (id == i.id) {
			usage = i.usage;
			return;
		}
	}
	for (auto &i : known_uses2) {
		if (id == i.id) {
			*this = i.usage;
			return;
		}
	}
	for (int x = 0; x < 8; ++x) {
		for (auto &i : known_uses1) {
			if (id == i.id + (const char*)to_string(x)) {
				usage = i.usage;
				index = x;
				return;
			}
		}
	}
}

USAGE2::USAGE2(const char *id) : usage(USAGE_UNKNOWN), index(0) {
	for (auto &i : usage_names) {
		if (str(id).begins(i)) {
			usage = USAGE(&i - usage_names + 1);
			index = from_string<int>(id + strlen(i));
			return;
		}
	}
}

}
