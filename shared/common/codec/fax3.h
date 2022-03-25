#include "base/defs.h"
#include "base/bits.h"

namespace iso {

enum FAXMODE {
	FAXMODE_CLASSIC		= 0x0000,		// default, include RTC
	FAXMODE_NORTC		= 0x0001,		// no RTC at end of data
	FAXMODE_NOEOL		= 0x0002,		// no EOL code at end of row
	FAXMODE_BYTEALIGN	= 0x0004,		// byte align row
	FAXMODE_WORDALIGN	= 0x0008,		// word align row
	FAXMODE_ALIGN		= FAXMODE_BYTEALIGN | FAXMODE_WORDALIGN,
	FAXMODE_2D			= 0x0010,
	FAXMODE_FIXED2D		= 0x0020,


	FAXMODE_CLASSF		= FAXMODE_NORTC,// TIFF Class F
};

inline FAXMODE	operator|(FAXMODE a, FAXMODE b) { return FAXMODE(int(a) | int(b)); }
inline bool		operator&(FAXMODE a, FAXMODE b) { return !!(int(a) & int(b)); }

bool	FaxDecode(const_memory_block srce, const bitmatrix_aligned<uint32> &dest, FAXMODE mode = FAXMODE_CLASSIC);
int		FaxEncode(memory_block dest, const bitmatrix_aligned<uint32> &srce, FAXMODE mode = FAXMODE_CLASSIC, int maxk = 0);

}