#include "directory_scan.h"
#include "base/atomic.h"
#include "directory.h"
#include "base/strings.h"
#include "base/hash.h"
#include "thread.h"

namespace app {

using namespace iso;

struct FileNotifyInformation : FILE_NOTIFY_INFORMATION {
	const FileNotifyInformation	*next() const {
		if (NextEntryOffset == 0)
			return 0;
		return (const FileNotifyInformation*)((const char*)this + NextEntryOffset);
	}
	operator filename() const {
		return str(FileName, FileNameLength / 2);
	}
};

struct DirectoryWatcher : OVERLAPPED {
	DWORD		buffer[1024];
	HANDLE		hDir;
	filename	spec;

	/*
	//doesn't seem to ever get called :(
	static void	FIELD_CALLBACK CompletionRoutine(DWORD error_code, DWORD size, OVERLAPPED *overlapped) {
		watch	*w = static_cast<watch*>(overlapped);
		if (w->hash->process_changes(w->dir, (const FileNotifyInformation*)w->buffer))
			w->start();
	}
	*/
	bool	start() {
		return !!ReadDirectoryChangesW(hDir, buffer, sizeof(buffer),
			TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
			NULL,
			this, 0//CompletionRoutine
		);
	}

	uint32	get_result() {
		DWORD	size;
		return GetOverlappedResult(hDir, this, &size, FALSE) ? size : 0;
	}

	DirectoryWatcher(const filename &spec) : spec(spec) {
		iso::clear(*(OVERLAPPED*)this);
		hDir = CreateFileA(spec.dir(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL
		);
		hEvent = CreateEvent(0, FALSE, FALSE, NULL);
		ISO_VERIFY(start());
	}
	~DirectoryWatcher() {
		CancelIo(hDir);
		CloseHandle(hDir);
		CloseHandle(hEvent);
	}
};

struct FilenamesHash : DirectoriesWatcher, hash_map<uint64, string>, atomic<refs<FilenamesHash> > {
	Mutex		mutex;
	Semaphore	command_semaphore;
	Job			command_job;
	dynamic_array<DirectoryWatcher>	watches;

	enum COMMAND {
		NONE	= 0,
		STOP	= -1,
		JOB		= 1,
	} command = NONE;

	static uint64 get_hash(const filename &fn) {
		uint64	x;
		const char *p = fn.ext_ptr();
		while (is_hex(p[-1]))
			--p;
		return get_num_base<16>(p, x) ? x : 0;
	}

	bool		add(const filename &fn) {
		//ISO_OUTPUTF("Found ") << fn << '\n';
		if (uint64 x = get_hash(fn)) {
			auto	w	= with(mutex);
			if (command == STOP)
				return false;
			(*this)[x]	= fn;
		}
		return true;
	}

	void		del(const filename &fn) {
		//ISO_OUTPUTF("Lost ") << fn << '\n';
		if (uint64 x = get_hash(fn))
			with(mutex), remove(x);
	}

	void	process_changes(const filename &spec, const FileNotifyInformation *fn) {
		while (fn && command != STOP) {
			filename	f = *fn;
			switch (fn->Action) {
				case FILE_ACTION_RENAMED_NEW_NAME:
				case FILE_ACTION_ADDED:
					if (f.matches(spec.name_ext_ptr()))
						add(spec.dir().add_dir(f));
					break;
				case FILE_ACTION_RENAMED_OLD_NAME:
				case FILE_ACTION_REMOVED:
					del(f);
					break;
			}
			fn = fn->next();
		}
	}

	const char	*Find(uint64 hash64) {
		auto	w	= with(mutex);
		if (string *p = check(hash64))
			return *p;
		return nullptr;
	}

	bool		AddSpec(const filename &spec) {
		DirectoryWatcher	&w = watches.emplace_back(spec);
		for (recursive_directory_iterator j(spec); j; ++j) {
			if (!add(j))
				return false;
		}
		return true;
	}
	void		RemoveSpec(const filename &spec) {
		for (auto &i : watches) {
			if (i.spec == spec) {
				watches.erase(&i);
				auto	w	= with(mutex);
				for (recursive_directory_iterator j(spec); j; ++j) {
					if (uint64 x = get_hash(j))
						remove(x);
				}
			}
		}
	}

	void		Destroy()				{ command = STOP; command_semaphore.unlock(); }
	void		AddJob(const Job &j)	{ command_job = j; command = JOB; command_semaphore.unlock(); }

	FilenamesHash() : command_semaphore(0) {

		RunThread([this]() {
			command_semaphore.lock();

			for (;;) {
				switch (command) {
					case STOP:
						delete this;
						return 0;

					case JOB:
						command_job(this);
						command = NONE;
						//fall through

					case NONE: {
						uint32	num_handles = watches.size32() + 1;
						HANDLE	*handles	= alloc_auto(HANDLE, num_handles), *ph = handles;

						*ph++ = command_semaphore;
						for (auto &i : watches)
							*ph++ = i.hEvent;

						for (;;) {
							DWORD r = WaitForMultipleObjects(num_handles, handles, FALSE, INFINITE);
							if (r == WAIT_OBJECT_0)
								break;
							if (r <= watches.size()) {
								auto	&w = watches[r - 1];
								if (w.get_result())
									process_changes(w.spec, (const FileNotifyInformation*)w.buffer);
								w.start();
							}
						}
					}
					break;
				}

			}

		}, ThreadPriority::LOW);
	}
};

DirectoriesWatcher *DirectoriesWatcher::Create() {
	return new FilenamesHash;
}

void DirectoriesWatcher::Destroy() {
	return static_cast<FilenamesHash*>(this)->Destroy();
}

bool DirectoriesWatcher::AddSpec(const filename &spec) {
	return static_cast<FilenamesHash*>(this)->AddSpec(spec);
}

const char *DirectoriesWatcher::Find(uint64 hash64) {
	return static_cast<FilenamesHash*>(this)->Find(hash64);
}

void DirectoriesWatcher::AddJob(Job &&j) {
	static_cast<FilenamesHash*>(this)->AddJob(move(j));
}

}// namespace app
