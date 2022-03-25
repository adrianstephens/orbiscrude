#include "filename.h"

namespace app {

using namespace iso;

struct DirectoriesWatcher {
	typedef callback_ref<void(DirectoriesWatcher*)>	Job;
	bool		AddSpec(const filename &spec);
	const char*	Find(uint64 hash64);
	void		AddJob(Job &&j);

	static DirectoriesWatcher *Create();
	void Destroy();
};

filename FindSDB(uint64 hash64);

}// namespace app
