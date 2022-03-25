#include "thread.h"

namespace iso {

TLS<Thread*> Thread::curr;

//-----------------------------------------------------------------------------
// Thread
//-----------------------------------------------------------------------------

void Thread::Create(const char *_name, int stack_size, ThreadPriority priority, void *p, void*(*proc)(void*)) {
	pthread_attr_t  attr;
    int	ret;

	name	= _name;

	ret = pthread_attr_init(&attr);
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_attr_setstacksize(&attr, stack_size);
	sched_param	sched;
	sched.sched_priority = priority;
	ret = pthread_attr_setschedparam(&attr, &sched);
	ret = pthread_create(&id, &attr, proc, p);
//	ret = pthread_setschedprio(id, priority);
	ret = pthread_attr_destroy(&attr);
}

void Thread::_Start() {
	curr = this;
	pthread_setname_np(id, name);
}

int Thread::Join(float timeout) {
	void	*val;
	return pthread_join(id, &val) ? -1 : (int)intptr_t(val);
}

} // namespace iso

