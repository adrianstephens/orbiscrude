#include "thread.h"

namespace iso {

//-----------------------------------------------------------------------------
// Thread
//-----------------------------------------------------------------------------

TLS<Thread*> Thread::curr;


void Thread::_create(const char *name, int stack_size, ThreadPriority priority, void *p, LPTHREAD_START_ROUTINE proc) {
	DWORD	id;
	if (h = CreateThread(NULL, stack_size, proc, p, CREATE_SUSPENDED, &id)) {
		SetThreadPriority(h, (int)priority);
		if (name)
			ThreadName(id, name);
	}
}

int Thread::Join(Timeout timeout) {
	DWORD ret = 0;
	if (WaitForSingleObject(h, timeout) == WAIT_OBJECT_0)
		GetExitCodeThread(h, &ret);
	return ret;
}

#ifndef PLAT_WINRT
thread_local void	*Fiber::primary;
#endif

}

