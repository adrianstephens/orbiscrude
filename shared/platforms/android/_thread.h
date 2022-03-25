#ifndef _THREAD_H
#define _THREAD_H

#include "base/defs.h"
#include <pthread.h>
#include <unistd.h>

#define THREAD_STACK_DEFAULT 65536//32768//8192

namespace iso {
//-----------------------------------------------------------------------------
//	Time
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Thread Local Storage
//-----------------------------------------------------------------------------

class _TLS {
	pthread_key_t	tls;
protected:
	void	set(void *p)	const	{ pthread_setspecific(tls, p);		}
	void	*get()			const	{ return pthread_getspecific(tls);	}
	_TLS()							{ pthread_key_create(&tls, 0);		}
};

template<class T> class TLS : _TLS {
public:
	TLS()							{}
	TLS(const T &t)					{ set((void*)t);			}
	const T &operator=(const T &t)	{ set((void*)t); return t;	}
	operator T()					{ return (T)get();			}
};

template<class T> class TLS<T*> : _TLS {
public:
	TLS()							{}
	TLS(T *t)						{ set(t);			}
	T *operator=(T *t)				{ set(t); return t;	}
	operator T*()					{ return (T*)get(); }
	T *operator->()					{ return (T*)get(); }
	T &operator *()					{ return *(T*)get(); }
};

//-----------------------------------------------------------------------------
// Thread
//-----------------------------------------------------------------------------

enum ThreadPriority {
	HIGHEST	= 0,
	HIGH	= 8,
	DEFAULT = 16,
	LOW		= 24,
	LOWEST	= 31,
};

class Thread {
	static TLS<Thread*> curr;
	template<class T>	static void *proc(void *p) {
		T *t = (T*)p;
		t->_Start();
		return (void*)(intptr_t)(*t)();
	}
	void	Create(const char *name, int stack_size, ThreadPriority priority, void *p, void*(*proc)(void*));

	pthread_t				id;
	const char				*name;
public:
	static Thread*	current()					{ return curr; }
	static pthread_t current_id()				{ return curr->id; }
	static void		sleep(float t = 0)			{ usleep(useconds_t(t * 1000000));	}
	static void		yield()						{ sched_yield(); }
	static void		exit(int ret)				{ pthread_exit((void*)intptr_t(ret)); }
	static int		processors()				{ return 1; }

	void			_Start();

	void			SetProcessor(int processor)	{}
	void			SetAffinity(uint64 affinity) {}
	bool			Start()						{ return true; }
	void			Suspend()					{}
	int				Join(float timeout = -1.f);

	template<class T> Thread(T *t, const char *name = 0, int stack_size = THREAD_STACK_DEFAULT, ThreadPriority priority = ThreadPriority::DEFAULT) {
		Create(name, stack_size, priority, t, proc<T>);
	}
	template<class T> Thread(T *t, const char *name, ThreadPriority priority) {
		Create(name, THREAD_STACK_DEFAULT, priority, t, proc<T>);
	}
	~Thread()	{ pthread_detach(id); }
};

//-----------------------------------------------------------------------------
// mutex
//-----------------------------------------------------------------------------
class Mutex {
	pthread_mutex_t	m;
public:
	Mutex(bool locked = false)			{ pthread_mutex_init(&m, NULL);			}
	~Mutex()							{ pthread_mutex_destroy(&m);			}
	bool	lock(float timeout = -1.f)	{ return pthread_mutex_lock(&m) == 0;	}
	void	unlock()					{ pthread_mutex_unlock(&m);				}
};

//-----------------------------------------------------------------------------
// Semaphore
//-----------------------------------------------------------------------------

class Semaphore {
	pthread_mutex_t	m;
	volatile	int	count;
public:
	Semaphore(int initial_count = 1)	{ pthread_mutex_init(&m, NULL);	count = initial_count; }
	~Semaphore()						{ pthread_mutex_destroy(&m); }
//	bool	lock(float timeout) {}
	bool	lock() {
		return __sync_add_and_fetch(&count, -1) != 0 || pthread_mutex_lock(&m) == 0;
	}
	void	unlock()					{
		if (__sync_add_and_fetch(&count, 1) == 1)
			pthread_mutex_unlock(&m);
	}
};

//-----------------------------------------------------------------------------
// CriticalSecion
//-----------------------------------------------------------------------------
class CriticalSection : public Mutex {
public:
};

//-----------------------------------------------------------------------------
// Event
//-----------------------------------------------------------------------------

class Event {
	pthread_cond_t	cond;
	pthread_mutex_t mutex;
	bool			state;
	bool			auto_reset;

public:
	Event(bool manual_reset = false, bool set = false) : state(false), auto_reset(!manual_reset) {
		pthread_cond_init(&cond, 0);
		pthread_mutex_init(&mutex, 0);
		if (set)
			signal();
	}
	~Event() {
		pthread_cond_destroy(&cond);
		pthread_mutex_destroy(&mutex);
	}
		
	bool	signal() {
		pthread_mutex_lock(&mutex);
		state = true;
		pthread_mutex_unlock(&mutex);

		if (auto_reset)
			pthread_cond_signal(&cond);
		else
			pthread_cond_broadcast(&cond);
		return true;
	}
	bool	clear() {
		pthread_mutex_lock(&mutex);
		state = false;
		pthread_mutex_unlock(&mutex);
		return true;
	}
	bool	wait()	{
		if (!state) {
			do {
				if (pthread_cond_wait(&cond, &mutex) != 0)
					return false;
			} while (!state);
		}
		if (auto_reset)
			state = false;

		pthread_mutex_unlock(&mutex);
		return true;
	}

	bool	wait(float timeout)	{
		if (!state) {
			timespec ts = _time::now() + _time::from_secs(timeout);
			do {
				if (pthread_cond_timedwait(&cond, &mutex, &ts) != 0)
					return false;
			} while (!state);
		}
		if (auto_reset)
			state = false;

		pthread_mutex_unlock(&mutex);
		return true;
	}

};


} // namespace iso

#endif // _THREAD_H

