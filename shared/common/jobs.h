#ifndef JOBS_H
#define JOBS_H

#include "lockfree.h"
#include "events.h"
#include "thread.h"
#include "base/atomic.h"
#include "allocators/allocator.h"

#ifdef PLAT_PS4
#define MAX_JOB_THREADS	5
#else
#define MAX_JOB_THREADS	8
#endif

namespace iso {

typedef callback<void()>	job;

template<typename L, typename D> struct lambda_job : pair<L, D> {
	lambda_job(const L &x, D &&d = D())	: pair<L, D>(x, move(d)) {}
	void	operator()()	{ this->a(); this->b(this); }
};

template<typename T> auto make_job(T &&t) {
	return new lambda_job<T, deleter<T>>(t);
}

template<typename T, typename A> auto make_job(T &&t, A &a) {
	return new(a) lambda_job<T, decltype(a.template get_deleter<T>())>(forward<T>(t), a.template get_deleter<T>());
}

//template<typename T> T *copy_lambda(void *p, const T &t) {
//	return new(p) T(t);
//}

//struct job_fifo : atomic<slink_base<job_fifo>>, lf_array_queue<job, 256> {};

//-----------------------------------------------------------------------------
// JobQueue
//-----------------------------------------------------------------------------

#ifdef PLAT_PC
class JobQueueMain {
	static void	put(const job &j);
public:
	template<class T> static void add(T *t) {
		put(t);
	}
	template<class T> static void add(T &&t) {
		put(make_job(forward<T>(t)));
	}
};
#endif

class JobQueue : protected lf_array_queue_list<job, 256> {
	Semaphore	semaphore;

public:
	JobQueue() : semaphore(0) {
	}
	void	put(job &&j) {
		lf_array_queue_list<job, 256>::put(move(j));
		semaphore.unlock();
	}

	int operator()() {
		for (job j;;) {
			semaphore.lock();
			while (!get(j)) {
				if (empty())
					return 0;
			}
			j();
		}
	}
	void dispatch() {
		job j;
		if (get(j))
			j();
	}
	void join() {
		for (job j; get(j);)
			j();
	}

#ifdef PLAT_PC
	static inline JobQueueMain& Main() {
		static JobQueueMain	main;
		return main;
	}
#else
	static inline JobQueue& Main() {
		static JobQueue	main;
		return main;
	}
#endif
};

//-----------------------------------------------------------------------------
// parallel_for
//-----------------------------------------------------------------------------
struct atomic_countdown : Event {
	atomic<int>		n;
	atomic_countdown(int _n) :  n(_n) {}
	void	signal() {
		if (--n == 0)
			Event::signal();
	}
};

template<class I, class F> void parallel_for(JobQueue &q, I i, I end, F f) {
	auto		n	= end - i;
	atomic_countdown	cd(n - 1);

	auto	j		= [i, &f, &cd]() {};
//	auto	*jobs	= alloc_auto(decltype(j), n - 1);
	malloc_block	jobmem(sizeof(j) * (n - 1));
	auto	*jobs	= (decltype(j)*)jobmem;
	auto	*jend	= jobs + n - 1;

	while (jobs < jend) {
		q.put(create((void*)jobs, [i, &f, &cd]() {
			f(*i);
			cd.signal();
		}));
		++jobs;
		++i;
	}

	// do last one on current thread
	f(*i);
	cd.wait();
}

template<class I, class F> void parallel_for_block(JobQueue &q, I i, I end, F f, int nt) {
	auto	n		= end - i;
	if (nt > n)
		nt = int(n);

	atomic_countdown	cd(nt - 1);

	auto	d		= n / nt;
	int		m		= int(n % nt);
	int		e		= -nt / 2;

	I		a		= i;
	I		b		= i;
	auto	j		= [a, b, &f, &cd]() {};
	auto	*jobs	= alloc_auto(decltype(j), nt - 1);
	auto	*jend	= jobs + nt - 1;

	while (jobs < jend) {
		b += d;
		if ((e += m) >= 0) {
			++b;
			e -= nt;
		}
		q.put(create((void*)jobs, [a, b, &f, &cd]() {
			for (I i = a; i != b; ++i)
				f(*i);
			cd.signal();
		}));
		a = b;
		++jobs;
	}

	// do last one on current thread
	while (a != end) {
		f(*a);
		++a;
	}

	cd.wait();
}

//-----------------------------------------------------------------------------
// SerialJobs
//-----------------------------------------------------------------------------

class SerialJobs : public Thread, public JobQueue {
public:
	SerialJobs(int stack_size = THREAD_STACK_DEFAULT, ThreadPriority priority = ThreadPriority::DEFAULT) : Thread(this, "SerialJobs", stack_size, priority) {
		Start();
	}
	SerialJobs(ThreadPriority priority, int stack_size = THREAD_STACK_DEFAULT) : Thread(this, "SerialJobs", stack_size, priority) {
		Start();
	}
};

//-----------------------------------------------------------------------------
// ConcurrentJobs
//-----------------------------------------------------------------------------

class ConcurrentJobs : public JobQueue {

	struct JobThread : Thread {
		ConcurrentJobs		*jobs;
		int operator()() {
			return (*jobs)();
		}
		JobThread(ConcurrentJobs *_jobs, int stack_size, ThreadPriority priority) : Thread(this, "ConcurrentJobs", stack_size, priority), jobs(_jobs) {
			Start();
		}
	};

	unique_ptr<JobThread>	*threads;
	malloc_block		lambda_memory;
	circular_allocator2	lambda_allocator;

public:

	ConcurrentJobs() : threads(0) {}
	ConcurrentJobs(int num, int stack_size = THREAD_STACK_DEFAULT, ThreadPriority priority = ThreadPriority::DEFAULT) : lambda_memory(65536), lambda_allocator(lambda_memory) {
		Init(num, stack_size, priority);
	}
	~ConcurrentJobs() {
		join();
		delete[] threads;
	}
	bool	Initialised() const {
		return !!threads;
	}
	void	Init(int num, int stack_size = THREAD_STACK_DEFAULT, ThreadPriority priority = ThreadPriority::DEFAULT) {
		threads = new unique_ptr<JobThread>[num];
		for (int i = 0; i < num; i++)
			threads[i] = make_unique<JobThread>(this, stack_size, priority);
	}

	template<class T> void add(T *t) {
		put(t);
	}
	template<class T> void add(T &&t) {
		put(make_job(forward<T>(t), lambda_allocator));
	}

	template<ThreadPriority PRIORITY> static inline ConcurrentJobs& Get() {
		static ConcurrentJobs	jobs(Thread::processors(), THREAD_STACK_DEFAULT, PRIORITY);
		return jobs;
	}
	static inline ConcurrentJobs& Get() {
		return Get<ThreadPriority::DEFAULT>();
	}
};

template<class I, class F> void parallel_for(I i, I end, F f) {
	parallel_for(ConcurrentJobs::Get(), i, end, f);
}

template<class C, class F> void parallel_for(JobQueue &q, C &&c, F f) {
	parallel_for(q, begin(c), end(c), f);
}

template<class C, class F> void parallel_for(C &c, F f) {
	parallel_for(begin(c), end(c), f);
}

template<class I, class F> void parallel_for_block(I i, I end, F f, int nt = 64) {
	parallel_for_block(ConcurrentJobs::Get(), i, end, f, nt);
}

template<class C, class F> void parallel_for_block(JobQueue &q, C &&c, F f, int nt = 64) {
	parallel_for_block(q, begin(c), end(c), f, nt);
}

template<class C, class F> void parallel_for_block(C &&c, F f, int nt = 64) {
	parallel_for_block(begin(c), end(c), f, nt);
}

} // namespace iso

#endif

