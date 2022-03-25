#ifndef SOUND_DEFS_H
#define SOUND_DEFS_H

#include "base/defs.h"

namespace ios {
using namespace iso;

class SoundBuffer {
public:
	uint32			buffer;
	uint32			size;
	uint16			freq;
	uint16			type;
	enum {
		STEREO	= 1 << 0,
		INT16	= 1 << 1,
		LOOP	= 1 << 2,
		MUSIC	= 1 << 3,
		MP3		= 1 << 4,
		FIXED	= 1 << 15,
	};
};

} // namespace ios

#endif // SOUND_DEFS_H
