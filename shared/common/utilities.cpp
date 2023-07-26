#include "utilities.h"
#include "base/algorithm.h"
#include "shader.h"
#include "crc32.h"

namespace iso {

deferred_deletes _dd, cleanup_dd;

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

const char* get_name(USAGE u) {
	return usage_names[u];
}

static const struct { crc32 id; USAGE usage; } known_uses1[] = {
	{"pos"_crc32,			USAGE_POSITION 		},
	{"position"_crc32,		USAGE_POSITION 		},
	{"norm"_crc32,			USAGE_NORMAL		},
	{"normal"_crc32,		USAGE_NORMAL		},
	{"colour"_crc32,		USAGE_COLOR			},
	{"color"_crc32,			USAGE_COLOR			},
	{"col"_crc32,			USAGE_COLOR			},
	{"texcoord"_crc32,		USAGE_TEXCOORD		},
	{"uv"_crc32,			USAGE_TEXCOORD		},
	{"tangent"_crc32,		USAGE_TANGENT		},
	{"binormal"_crc32,		USAGE_BINORMAL		},
	{"weights"_crc32,		USAGE_BLENDWEIGHT	},
	{"bones"_crc32,			USAGE_BLENDINDICES	},
	{"indices"_crc32,		USAGE_BLENDINDICES	},
	{"POSITION"_crc32,		USAGE_POSITION 		},
	{"NORMAL"_crc32,		USAGE_NORMAL		},
	{"COLOR"_crc32,			USAGE_COLOR			},
	{"TEXCOORD"_crc32,		USAGE_TEXCOORD		},
	{"TANGENT"_crc32,		USAGE_TANGENT		},
	{"BINORMAL"_crc32,		USAGE_BINORMAL		},
	{"BLENDWEIGHT"_crc32,	USAGE_BLENDWEIGHT	},
	{"BLENDINDICES"_crc32,	USAGE_BLENDINDICES	},
};

static const struct { crc32 id; USAGE2 usage; } known_uses2[] = {
	{"centre"_crc32,		{USAGE_POSITION, 	1}},
	{"smooth_normal"_crc32,	{USAGE_NORMAL,		1}},
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
