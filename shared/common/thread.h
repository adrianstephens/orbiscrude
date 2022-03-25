#ifndef THREAD_H
#define THREAD_H

#include "_thread.h"
#include "base/atomic.h"

namespace iso {

inline uint32 thread_random() {
	thread_local uint64	seed = 31415926535L;
	return uint32(seed = 1664525L * seed + 1013904223L);
}

// depracted
//template<typename T> struct ThreadT : Thread { ThreadT() : Thread(static_cast<T*>(this)) {} };

template<typename F> Thread *RunThread(const char *name, F &&f, int stack_size = THREAD_STACK_DEFAULT, ThreadPriority priority = ThreadPriority::DEFAULT, uint64 affinity = ~uint64(0)) {
	struct LambdaThread : Thread {
		F	f;
		int	operator()()	{ f(); delete this; return 0; }
		LambdaThread(const char *name, F &&_f, int stack_size, ThreadPriority priority, uint64 affinity)
			: Thread(this, name, stack_size, priority), f(move(_f)) {
			SetAffinity(affinity); Start();
		}
	};
	return new LambdaThread(name, move(f), stack_size, priority, affinity);
}
template<typename F> Thread *RunThread(const char *name, F &&f, ThreadPriority priority, uint64 affinity = ~uint64(0)) {
	return RunThread(name, forward<F>(f), THREAD_STACK_DEFAULT, priority, affinity);
}
template<typename F> Thread *RunThread(F &&f, int stack_size = THREAD_STACK_DEFAULT, ThreadPriority priority = ThreadPriority::DEFAULT, uint64 affinity = ~uint64(0)) {
	return RunThread(0, forward<F>(f), stack_size, priority, affinity);
}
template<typename F> Thread *RunThread(F &&f, ThreadPriority priority, uint64 affinity = ~uint64(0)) {
	return RunThread(0, forward<F>(f), THREAD_STACK_DEFAULT, priority, affinity);
}

//-----------------------------------------------------------------------------
// with (...something that supports lock/unlock)
//-----------------------------------------------------------------------------

template<typename T> struct with_s {
	T	&t;
	with_s(T &_t) : t(_t)					{ t.lock();	}
	with_s(T &_t, float timeout) : t(_t)	{ t.lock(timeout); }
	~with_s()								{ t.unlock(); }
};

template<typename T> with_s<T> with(T &t)					{ return with_s<T>(t); }
template<typename T> with_s<T> with(T &t, float timeout)	{ return with_s<T>(t, timeout); }

template<typename T, typename M> struct locked_s : holder<T&>, with_s<M> {
	locked_s(T &t, M &m) : holder<T&>(t), with_s<M>(m) {}
};

template<typename T, typename M> locked_s<T, M> locked(T &t, M &m)	{ return locked_s<T, M>(t, m); }

//---------------------------------------------------------
// Benaphore
//---------------------------------------------------------

class Benaphore : Semaphore {
	atomic<int> count;
public:
	Benaphore(bool locked = false) : Semaphore(0), count(int(locked)) {}

	void lock() {
#if 0
		if (count++ > 0)
			Semaphore::lock();
#else
		if (!try_lock()) {
			for (int spin = 10000; spin--;) {
				if (try_lock())
					return;
			}
			if (count++ > 0)
				Semaphore::lock();
		}
#endif
	}
	void unlock() {
		if (--count > 0)
			Semaphore::unlock();
	}
	bool try_lock()  {
		int	i = count;
		while (i <= 0 && !count.cas_exch(i, i + 1));
		return i <= 0;
    }
};

class RecursiveBenaphore : Semaphore  {
    Thread		*owner;
	uint32		recursion;
	atomic<int>	count;
public:
    RecursiveBenaphore() : Semaphore(0), owner(0), recursion(0), count(0) {}

    void lock() {
        auto tid = Thread::current();
        if (count++ > 0) {
            if (tid != owner)
				Semaphore::lock();
        }
        owner = tid;
        ++recursion;
    }
    void unlock() {
        ISO_CHEAPASSERT(Thread::current() == owner);
        if (--recursion == 0) {
            owner = 0;
			if (--count > 0)
				Semaphore::unlock();
		} else {
			--count;
		}
    }
	bool try_lock()  {
		if (!count.cas(0, 1))
			return false;
		owner = Thread::current();
		return true;
    }

};

#if 0

//---------------------------------------------------------
// LightweightSemaphore
//---------------------------------------------------------
class LightweightSemaphore : Semaphore {
private:
	atomic<int> count;

public:
	LightweightSemaphore(int initialCount = 0) : count(initialCount), Semaphore(0) {}

	bool try_lock() {
		int		count0 = count;
		return count0 > 0 && count.cas(count0, count0 - 1);
	}

	void lock() {
		if (!try_lock()) {
			for (int spin = 10000; spin--;) {
				if (try_lock())
					return;
			}
			if (count-- <= 0)
				Semaphore::lock();
		}
	}

	void unlock(int n = 1) {
		int count0 = count += n;
		if (count0 < n)
			Semaphore::unlock(n - max(count0, 0));
	}
};

//---------------------------------------------------------
// AutoResetEvent
//---------------------------------------------------------

class AutoResetEvent : LightweightSemaphore {
	// 1 => signaled, 0 => reset and no threads are waiting, -N => reset and N threads are waiting
	atomic<int> status;
public:
	AutoResetEvent(int initialStatus = 0) : status(initialStatus) {}
	void unlock() {
		for (;;) {
			int status0 = status;
			if (status.cas(status0, status0 < 1 ? status0 + 1 : 1)) {
				if (status0 < 0)
					LightweightSemaphore::unlock();
				return;
			}
		}
	}
	void lock() {
		if (--status < 0)
			LightweightSemaphore::lock();
	}
};
#endif
} // namespace iso

#endif // THREAD_H
