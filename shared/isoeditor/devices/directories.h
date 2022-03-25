#include "iso/iso.h"

namespace iso {

	namespace dirs {
		struct Entry {
			string					name;
			uint64					size;
			Entry() : size(0) {}
			Entry(const string &name, uint64 size = 0) : name(move(name)), size(size) {}
			Entry(string &&name, uint64 size = 0) : name(move(name)), size(size) {}
		};
		struct Dir : Entry, ISO::VirtualDefaults {
			dynamic_array<Dir*>		subdirs;
			dynamic_array<Entry*>	entries;
			Dir() {}
			Dir(const string &name) : Entry(name) {}
			Dir(string &&name) : Entry(move(name)) {}
			~Dir()	{
				for (auto i : subdirs)
					delete i;
				for (auto i : entries)
					delete i;
			}
			uint64			CalcSize();
			uint32			Count()			{ return uint32(subdirs.size() + entries.size()); }
			tag2			GetName(int i)	{ return i < subdirs.size() ? subdirs[i]->name : entries[i - subdirs.size()]->name; }
			ISO::Browser2	Index(int i)	{ return i < subdirs.size() ? ISO::MakeBrowser(*subdirs[i]) : ISO::MakeBrowser(*entries[i - subdirs.size()]); }
		};
	}
}

ISO_DEFUSERCOMPV(iso::dirs::Entry, size);
ISO_DEFUSERVIRTX(iso::dirs::Dir, "Dir");
