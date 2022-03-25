#if USE_CMDLINK

#include "cmd_link.h"
#include "iso/iso_files.h"
#include "thread.h"
#include "utilities.h"

using namespace iso;

// config
const unsigned short cmd_port = 31978;

//-----------------------------------------------------------------------------
//	CmdDispatch
//-----------------------------------------------------------------------------
static class CmdDispatch {
	typedef	pair<ISO_ptr<void>, isolink_handle_t> entry;
	dynamic_array<entry>	pool;
	CriticalSection			lock;

public:
	void Append(isolink_handle_t handle, ISO_ptr<void> &p) {
		new (pool) entry(p, handle);
	}

	void Flush() {
		// empty
		if (pool.size() == 0)
			return;

		// lock
		lock.lock();
		// dispatch
		for (int i = 0, n = pool.size32(); i < n; i++) {
			// read
			ISO_ptr<void> p = pool[i].a;
			if (_CmdHandler *handler = _CmdHandler::Find(p.ID()))
				(*handler)(pool[i].b, ISO::Browser(p), false);
		}
		pool.reset();
		// unlock
		lock.unlock();
	}

} cmd_dispatch;

void CmdLinkFlush() {
	cmd_dispatch.Flush();
}

//-----------------------------------------------------------------------------
//	CmdListener
//-----------------------------------------------------------------------------
class CmdListener : public Thread {
	malloc_block cmd;

public:
	CmdListener() : Thread(this, "CmdListener", THREAD_STACK_DEFAULT * 4) {
		Start();
	}
	int operator()() {
		// serve
		isolink_handle_t handle;
		while ((handle = isolink_listen(cmd_port)) != isolink_invalid_handle) {
			// receive
			uint32be size;
			if (isolink_receive(handle, &size, sizeof(size)) == sizeof(size)) {
				// resize
				if (cmd.length() < size)
					cmd.resize(size);
				// append
				if (isolink_receive(handle, cmd, size) == size) {
					static FileHandler *ib = FileHandler::Get(".ib");
					ISO_ptr<anything> p = ib->Read(tag(), memory_reader(cmd).me());
					for (int i = 0, n = p->Count(); i < n; i++) {
						ISO_ptr<void> _p = (*p)[i];
						if (_CmdHandler *handler = _CmdHandler::Find(_p.ID())) {
							if (!(*handler)(handle, ISO::Browser(_p), true))
								cmd_dispatch.Append(handle,_p);
						}
					}
					continue;
				}
			}
			isolink_close(handle);
		}
		ISO_TRACEF("ERR[ISOLINK]: %s\n", isolink_get_error());
		return 0;
	}
};

class BroadcastListener : public Thread {
public:
	int operator()() {
		isolink_announce();
		delete this;
		return 0;
	}
	BroadcastListener() : Thread(this, "BroadcastListener") { Start(); }
};

bool CmdLinkServe() {
	if (isolink_init()) {
		new CmdListener;
		new BroadcastListener;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//	LinkHandler
//-----------------------------------------------------------------------------
_CmdHandler *_CmdHandler::Find(const crc32 id) {
	for (iterator iter = begin(); iter != end(); ++iter) {
		if (iter->id == id)
			return iter;
	}
	return NULL;
}

#endif