#include "fibers.h"

namespace iso {

atomic<freelist<stack::block>>	stack::pool[8];
atomic<int>						stack::inuse;
fiber::_primary 				fiber::primary;
thread_local fcontext_t 		fiber::_primary::ctx;

FiberJobs*						FiberJobs::me;
lf_array_queue_list<fiber, 256>	FiberJobs::ready_fibers;
Benaphore						FiberJobs::ben;
//atomic<int>					FiberJobs::ready_queued, FiberJobs::ready_dequeued;

void fiber_sleep(float t, fiber parent) {
	parent.resume_with([t](fiber &&f) {
		RunThread("fiber_sleep", [t, f(move(f))]() mutable {
			Thread::sleep(t);
			f.resume();
		});
	});

	// now running on new thread:
	fiber::primary.ctx = parent.ctx;
}

}
