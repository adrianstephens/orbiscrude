#include "rcu.h"

namespace iso {

//alignas(CACHELINE_SIZE) rcu	_rcu;
thread_local RCU::per_thread RCU::_per_thread;// alignas(CACHELINE_SIZE);

void RCU::_synchronise() {
	enum {
		ACTIVE_ATTEMPTS	= 100,
	};


	MCS_lock::Node		waiter;
	if (waiters.try_lock(waiter)) {

		waiters_lock.lock();

		// Move all waiters into our local queue
		MCS_lock	local_waiters = move(waiters);

		readers_lock.lock();
		
		auto rsize = readers.size();

		if (!readers.empty()) {
		#if 0//(PHASE_MASK & (PHASE_MASK - 1)) == 0
			list_base<per_thread>	active_readers, quiescent_readers;
			uint32	phase = ctr;

			// Wait for readers to observe original parity or be quiescent
			for (uint32 wait_loops = 0;; ++wait_loops) {

				for (auto &i : with_iterator(readers)) {
					switch (i->state(phase)) {
						case ACTIVE_CURRENT:
							active_readers.push_front(i.remove());
							break;
						case INACTIVE:
							quiescent_readers.push_front(i.remove());
							break;
						case ACTIVE_OLD:
							break;
					}
				}

				if (readers.empty())
					break;

				readers_lock.unlock();
				if (wait_loops >= ACTIVE_ATTEMPTS) {
					futex = -1;
					while (futex)
						_atomic_pause();
				} else {
					_atomic_pause();
				}
				readers_lock.lock();
			}

			// Switch parity
			phase = (ctr ^= PHASE_MASK);

			// Wait for readers to observe new parity or be quiescent
			for (uint32 wait_loops = 0; !active_readers.empty(); ++wait_loops) {
				readers_lock.unlock();
				if (wait_loops >= ACTIVE_ATTEMPTS) {
					futex = -1;
					while (futex)
						_atomic_pause();
				} else {
					_atomic_pause();
				}
				readers_lock.lock();

				for (auto i : with_iterator(active_readers)) {
					switch (i->state(phase)) {
						case ACTIVE_CURRENT:
						case INACTIVE:
							quiescent_readers.push_front(i.remove());
							break;
						case ACTIVE_OLD:
							break;
					}
				}
			}
		#else
			list_base<per_thread>	quiescent_readers;
			quiescent_readers.push_front(_per_thread.unlink());

			// next phase
			uint32	phase = (ctr += PHASE_MASK);

			// Wait for readers to observe new phase or be quiescent
			for (uint32 wait_loops = 0;; ++wait_loops) {

				for (auto &i : with_iterator(readers)) {
					switch (i->state(phase)) {
						case ACTIVE_CURRENT:
						case INACTIVE:
							quiescent_readers.push_front(i.remove());
							break;
						case ACTIVE_OLD:
							break;
					}
				}

				if (readers.empty())
					break;

				readers_lock.unlock();
				if (wait_loops >= ACTIVE_ATTEMPTS) {
					futex = -1;
					while (futex)
						_atomic_pause();
				} else {
					_atomic_pause();
				}
				readers_lock.lock();
			}
		#endif

			// Put quiescent reader list back into readers
			readers.append(move(quiescent_readers));
			rsize = readers.size();

		}

		readers_lock.unlock();
		waiters_lock.unlock();

	} else {
		waiter.wait();
	}

	_per_thread.do_defered();
}

}//namespace iso
