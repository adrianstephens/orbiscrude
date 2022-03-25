#include "liborbiscrude.h"
#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned uint32;

struct Named {
	SceKernelSema	semaphore;
	Named(const char *name, const void *p) {
		char	buffer[256];
		sprintf(buffer, "%s=0x%012lx", name, uintptr_t(p));
		sceKernelCreateSema(&semaphore, buffer, SCE_KERNEL_SEMA_ATTR_TH_FIFO, 1, 0x7fffffff, NULL);
	}
	~Named() {
		sceKernelDeleteSema(semaphore);
	}
};

struct PS4compute {
	uint32	id;
	uint32	size;
	void*	base;
	void*	read;
	PS4compute(uint32 _id, void *_base, uint32 _size, void *_read) : id(_id), size(_size), base(_base), read(_read) {}
};

//-----------------------------------------------------------------------------
//	Manual
//-----------------------------------------------------------------------------
namespace orbiscrude {

void setGfxContext(const sce::Gnmx::GfxContext *gfxc) {
	static Named	named_gfx("GfxContext", gfxc);
}

struct GfxSubmission {
	uint32_t		count;
	uint32_t		unused;
	const void**	dcb_addrs;
	const uint32_t*	dcb_sizes;
	const void**	ccb_addrs;
	const uint32_t*	ccb_sizes;

	void	set(uint32_t _count, const void **_dcb_addrs, const uint32_t *_dcb_sizes, const void **_ccb_addrs, const uint32_t *_ccb_sizes) {
		count		= _count;
		dcb_addrs	= _dcb_addrs;
		dcb_sizes	= _dcb_sizes;
		ccb_addrs	= _ccb_addrs;
		ccb_sizes	= _ccb_sizes;
	}
	GfxSubmission() : count(0) {}
};

template<typename T> void move(T *dest, T *srce, uint32 n) {
	memmove(dest, srce, n * sizeof(T));
}

//srce doesn't wrap
template<typename T> void move_circular2(T *buffer, uint32 buffer_size, uint32 dest, uint32 srce, uint32 n) {
	uint32	first = buffer_size - dest;
	if (n < first) {
		move(buffer + dest, buffer + srce, n);
	} else {
		move(buffer + dest, buffer + srce, first);
		move(buffer, buffer + srce + first, n - first);
	}
}

template<typename T> void move_circular(T *buffer, uint32 buffer_size, uint32 dest, uint32 srce, uint32 n) {
	uint32	first = buffer_size - srce;
	if (n < first) {
		move_circular2(buffer, buffer_size, dest, srce, n);
	} else {
		move_circular2(buffer, buffer_size, (dest + first) % buffer_size, 0, n - first);
		move_circular2(buffer, buffer_size, dest, srce, first);
	}
}

struct GfxSubmissions {
	ScePthreadMutex	mutex;
	GfxSubmission	*submission;
	uint32			num_frames, num_submits;
	uint32			index, frame;

	uint32			*prev_indices;
	const void**	dcb_addrs, **ccb_addrs;
	uint32*			dcb_sizes,  *ccb_sizes;

	struct FrameMarker {
		uint32	size;
		uint32	start[15];

		void	set(int frame) {
			sce::Gnm::DrawCommandBuffer	db;
			db.init(start, sizeof(start), 0, 0);
			char	buffer[64];
			sprintf(buffer, "Frame %i", frame);
			db.setMarker(buffer, 0xff0000ff);
			size = db.getSizeInBytes();
		}
	};
	FrameMarker		*frame_markers;

	GfxSubmissions() : num_frames(0), num_submits(0), index(0), frame(0), prev_indices(0), frame_markers(0) {
		scePthreadMutexInit(&mutex, NULL, NULL);
	}

	bool	initialised() const {
		return !!submission;
	}
	//markDispatchDrawAcbAddress

	void	init(uint32_t _num_frames, uint32_t _num_submits) {
		if (!submission) {
			submission	= new GfxSubmission;
			static Named named("GfxSubmission", submission);
		}

		num_frames	= _num_frames;
		num_submits	= _num_submits;
		index		= frame = 0;

		free(prev_indices);
		void	*buffer	= malloc(num_frames * sizeof(uint32) + (num_frames + 1) * sizeof(FrameMarker) + num_submits * (sizeof(void*) + sizeof(uint32)) * 2);
		prev_indices	= (uint32*)buffer;
		frame_markers	= (FrameMarker*)(prev_indices + num_frames);
		dcb_sizes		= (uint32*)(frame_markers + num_frames + 1);
		ccb_sizes		= dcb_sizes + num_submits;
		dcb_addrs		= (const void**)(ccb_sizes + num_submits);
		ccb_addrs		= dcb_addrs + num_submits;

		memset(prev_indices, 0, sizeof(uint32) * num_frames);
	}

	void	add(const void *_dcb_addr, uint32_t _dcb_size, const void *_ccb_addr, uint32_t _ccb_size) {
		if (!_dcb_size)
			return;

		if (!initialised())
			init(1, 4096);

		scePthreadMutexLock(&mutex);

		int	j = index;
		dcb_addrs[j]	= _dcb_addr;
		dcb_sizes[j]	= _dcb_size;
		ccb_addrs[j]	= _ccb_addr;
		ccb_sizes[j]	= _ccb_size;
		if (++j == num_submits)
			j = 0;
		index	= j;

		scePthreadMutexUnlock(&mutex);
	}

	void	add(uint32_t n, const void *const*_dcb_addrs, const uint32_t *_dcb_sizes, const void *const*_ccb_addrs, const uint32_t *_ccb_sizes) {
		if (!initialised())
			init(1, 4096);

		scePthreadMutexLock(&mutex);

		int	j = index;
		for (int i = 0; i < n; ++i) {
			dcb_addrs[j]	= _dcb_addrs[i];
			dcb_sizes[j]	= _dcb_sizes[i];
			ccb_addrs[j]	= _ccb_addrs ? _ccb_addrs[i] : 0;
			ccb_sizes[j]	= _ccb_sizes ? _ccb_sizes[i] : 0;
			if (++j == num_submits)
				j = 0;
		}
		index	= j;

		scePthreadMutexUnlock(&mutex);
	}

	void	flip() {
		if (!initialised())
			return;

		int	i = prev_indices[frame % num_frames];
		int	n = index - i;
		if (n < 0) {
			n		+= num_submits;
			index	= n;
			for (int j = 0; j < num_frames; ++j)
				prev_indices[j] -= i;

			move_circular(dcb_addrs, num_submits, 0, i, n);
			move_circular(ccb_addrs, num_submits, 0, i, n);
			move_circular(dcb_sizes, num_submits, 0, i, n);
			move_circular(ccb_sizes, num_submits, 0, i, n);
		}
		prev_indices[frame++ % num_frames]	= index;
		submission->set(n, dcb_addrs + i, dcb_sizes + i, ccb_addrs + i, ccb_sizes + i);
	#if 0
		FrameMarker	&fm = frame_markers[frame % (num_frames + 1)];
		fm.set(frame);
		add(fm.start, fm.size, 0, 0);
	#endif
	}
};

static GfxSubmissions submissions;

struct ComputeQueue : PS4compute, Named {
	static	ComputeQueue	*first;

	uint32			offset;
	ComputeQueue	*next;
	SceKernelSema	semaphore;

	static	ComputeQueue	*get(uint32_t id) {
		for (ComputeQueue *i = first; i; i = i->next) {
			if (i->id == id)
				return i;
		}
		return 0;
	}
	static	bool	remove(ComputeQueue *q) {
		for (ComputeQueue **i = &first; *i; i = &(*i)->next) {
			if (*i == q) {
				*i = q->next;
				return true;
			}
		}
		return false;
	}

	ComputeQueue(uint32 _id, void *_base, uint32 _size, void *_read) : PS4compute(_id, _base, _size, _read), Named("ComputeQueue", this), offset(0) {
		next	= first;
		first	= this;
	}

	~ComputeQueue() {
		remove(this);
	}
};

ComputeQueue	*ComputeQueue::first;

void init(uint32_t num_frames, uint32_t num_submits) {
	submissions.init(num_frames, num_submits);
}

int32_t submitCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes) {
	submissions.add(count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes);
	return sce::Gnm::submitCommandBuffers(count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes);
}

int32_t submitAndFlipCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes, uint32_t videoOutHandle, uint32_t rtIndex, uint32_t flipMode, int64_t flipArg) {
	submissions.add(count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes);
	submissions.flip();
	return sce::Gnm::submitAndFlipCommandBuffers(count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes, videoOutHandle, rtIndex, flipMode, flipArg);
};

void flip() {
	submissions.flip();
}

int32_t submitDone() {
	submissions.flip();
	return sce::Gnm::submitDone();
}

int32_t mapComputeQueue(uint32_t *vqueueId, uint32_t globalPipeId, uint32_t queueId, void *ringBaseAddr, uint32_t ringSizeInDW, void *readPtrAddr) {
	int32_t	r = sce::Gnm::mapComputeQueue(vqueueId, globalPipeId, queueId, ringBaseAddr, ringSizeInDW, readPtrAddr);
	if (r == SCE_GNM_OK)
		new ComputeQueue(*vqueueId, ringBaseAddr, ringSizeInDW, readPtrAddr);
	return r;
}

void unmapComputeQueue(uint32_t vqueueId) {
	sce::Gnm::unmapComputeQueue(vqueueId);
	delete ComputeQueue::get(vqueueId);
}

void dingDong(uint32_t vqueueId, uint32_t nextStartOffsetInDw) {
	sce::Gnm::dingDong(vqueueId, nextStartOffsetInDw);
	if (submissions.initialised()) {
		if (ComputeQueue *q = ComputeQueue::get(vqueueId)) {
			if (nextStartOffsetInDw < q->offset) {
				submissions.add((uint32*)q->base + q->offset, (q->size - q->offset) * sizeof(uint32), 0, 0);
				q->offset = 0;
			}
			submissions.add((uint32*)q->base + q->offset, (nextStartOffsetInDw - q->offset) * sizeof(uint32), 0, 0);
			q->offset = nextStartOffsetInDw;
		}
	}
}

}//namespace orbiscrude

//-----------------------------------------------------------------------------
//	Breakpoints
//-----------------------------------------------------------------------------

namespace orbiscrude_usebp {

struct ComputeQueue : PS4compute, Named {
	static	ComputeQueue	*first;

	ComputeQueue	*next;
	SceKernelSema	semaphore;

	static	ComputeQueue	*get(uint32_t id) {
		for (ComputeQueue *i = first; i; i = i->next) {
			if (i->id == id)
				return i;
		}
		return 0;
	}
	static	bool	remove(ComputeQueue *q) {
		for (ComputeQueue **i = &first; *i; i = &(*i)->next) {
			if (*i == q) {
				*i = q->next;
				return true;
			}
		}
		return false;
	}
	ComputeQueue(uint32 _id, void *_base, uint32 _size, void *_read) : PS4compute(_id, _base, _size, _read), Named("ComputeQueue", this) {
		next	= first;
		first	= this;
	}
	~ComputeQueue() {
		remove(this);
	}
};

ComputeQueue	*ComputeQueue::first;

int32_t submitCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes) {
	return sce::Gnm::submitCommandBuffers(count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes);
}

int32_t submitAndFlipCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes, uint32_t videoOutHandle, uint32_t rtIndex, uint32_t flipMode, int64_t flipArg) {
	return sce::Gnm::submitAndFlipCommandBuffers(count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes, videoOutHandle, rtIndex, flipMode, flipArg);
};

int32_t submitDone() {
	return sce::Gnm::submitDone();
}

int32_t mapComputeQueue(uint32_t *vqueueId, uint32_t globalPipeId, uint32_t queueId, void *ringBaseAddr, uint32_t ringSizeInDW, void *readPtrAddr) {
	int32_t	r = sce::Gnm::mapComputeQueue(vqueueId, globalPipeId, queueId, ringBaseAddr, ringSizeInDW, readPtrAddr);
	if (r == SCE_GNM_OK)
		new ComputeQueue(*vqueueId, ringBaseAddr, ringSizeInDW, readPtrAddr);
	return r;
}

void unmapComputeQueue(uint32_t vqueueId) {
	sce::Gnm::unmapComputeQueue(vqueueId);
	delete ComputeQueue::get(vqueueId);
}

void dingDong(uint32_t vqueueId, uint32_t nextStartOffsetInDw) {
	sce::Gnm::dingDong(vqueueId, nextStartOffsetInDw);
}

} //namespace orbiscrude_usebp

#if 0
extern "C" {
int32_t sceGnmSubmitCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes) {
	orbiscrude::submissions.add(count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes);
	return 0;
}
int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, void *dcbGpuAddrs[], uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes, uint32_t videoOutHandle, uint32_t rtIndex, uint32_t flipMode, int64_t flipArg) {
	orbiscrude::submissions.add(count, dcbGpuAddrs, dcbSizesInBytes, ccbGpuAddrs, ccbSizesInBytes);
	orbiscrude::submissions.flip();
	return 0;
}
}
#endif