#ifndef LIBORBISCRUDE_H
#define LIBORBISCRUDE_H

#include <stdint.h>
#include <gnm.h>

namespace sce {
	namespace Gnmx {
		class GfxContext;
	}
}

namespace orbiscrude {

void	setGfxContext(const sce::Gnmx::GfxContext *gfxc);
void	init(uint32_t num_frames, uint32_t num_submits);
int32_t	submitCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes);
int32_t	submitAndFlipCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes, uint32_t videoOutHandle, uint32_t rtIndex, uint32_t flipMode, int64_t flipArg);
int32_t	submitDone();
void	flip();

int32_t mapComputeQueue(uint32_t *vqueueId, uint32_t globalPipeId, uint32_t queueId, void *ringBaseAddr, uint32_t ringSizeInDW, void *readPtrAddr);
void	unmapComputeQueue(uint32_t vqueueId);
void	dingDong(uint32_t vqueueId, uint32_t nextStartOffsetInDw);

} //namespace orbiscrude

namespace orbiscrude_usebp {
int32_t	submitCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes);
int32_t	submitAndFlipCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes, uint32_t videoOutHandle, uint32_t rtIndex, uint32_t flipMode, int64_t flipArg);
int32_t	submitDone();

int32_t mapComputeQueue(uint32_t *vqueueId, uint32_t globalPipeId, uint32_t queueId, void *ringBaseAddr, uint32_t ringSizeInDW, void *readPtrAddr);
void	unmapComputeQueue(uint32_t vqueueId);
void	dingDong(uint32_t vqueueId, uint32_t nextStartOffsetInDw);

} //namespace orbiscrude_usebp

#endif	// LIBORBISCRUDE_H
