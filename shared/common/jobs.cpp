#include "jobs.h"
#include "thread.h"
#include "lockfree.h"

#include "base/strings.h"

using namespace iso;

#if 0
bool job_queue_base::get(job &j) {
	for (;;) {
		job_fifo	*f = get_fifo;
		if (f->pop_front(j))
			return true;

		job_fifo	*f2 = front();
		if (!f2)
			return false;

		if (get_fifo.cas(f, f2)) {
			pop_front();
			if (f = exchange(spare_fifo, f)) {
				ISO_ASSERT(f->empty());
				delete f;
			}
		}
	}
}
void job_queue_base::put(const job &j) {
	for (;;) {
		job_fifo	*f = put_fifo;
		if (f->push_back(j))
			break;

		job_fifo	*f2 = new job_fifo;
		if (put_fifo.cas(f, f2)) {
			ISO_TRACEF("new job fifo from thread ") << Thread::current_id() << '\n';
			push_back(f2);
		} else {
			ISO_TRACEF("aborted new job fifo from thread ") << Thread::current_id() << '\n';
			delete f2;
		}
	}
}
#elif 0
bool JobQueue::get(job &j) {
	for (;;) {
		auto	f = safe_front();
		if (!f)
			return false;

		if (f->pop_front(j))
			return true;

		if (f == put_fifo.get().a)
			return false;

//		if (!f->stop_queuing())
//			return false;

		if (auto *h = try_pop_front(f))
			delete h;
	}
}
void JobQueue::put(const job &j) {
	for (;;) {
		tagged_pointer<job_fifo>	f = put_fifo;

		if (f && f->push_back(j))
			break;

		job_fifo	*f2 = new job_fifo;

		if (put_fifo.cas(f, {f2, f.b + 1})) {
			push_back(f2);
		} else {
			delete f2;
		}
	}
	semaphore.unlock();
}
#endif

