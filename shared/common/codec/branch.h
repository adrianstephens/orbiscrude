#include "codec.h"

namespace branch {
using namespace iso;

uint32	BCJ_Convert(memory_block mem, uint32 ip, uint32 mask, bool encoding);
void	ARM_Convert(memory_block mem, uint32 ip, bool encoding);
void	ARMT_Convert(memory_block mem, uint32 ip, bool encoding);
void	PPC_Convert(memory_block mem, uint32 ip, bool encoding);
void	SPARC_Convert(memory_block mem, uint32 ip, bool encoding);
void	X86_Convert(memory_block mem, uint32 ip, uint8 Mask, bool encoding);
void	Itanium_Convert(memory_block mem, uint32 ip, bool encoding);

}  // namespace branch
