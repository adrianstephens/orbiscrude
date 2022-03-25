#include "groups.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Groups
//-----------------------------------------------------------------------------
Groups groups;

size_t Groups::Group::Compact()
{
	Members::iterator iter = members.begin();
	while (iter != members.end()) {
		if (!*iter) {
			Object *obj;
			do {
				obj = members.pop_back();
			} while (!obj && iter != members.end());
			if (iter != members.end())
				*iter++ = obj;
		} else
			++iter;
	}
	return members.size();
}

size_t Groups::Count(crc32 id) const {
	Pool::const_iterator iter = lower_bound(groups.begin(), groups.end(), id, Group::Cmp());
	return iter != groups.end() && (*iter)->id == id ? (*iter)->Count() : 0;
}

Object* Groups::At(crc32 id, size_t index) const {
	Pool::const_iterator iter = lower_bound(groups.begin(), groups.end(), id, Group::Cmp());
	return iter != groups.end() && (*iter)->id == id && (*iter)->Count() > index ? static_cast<Object*>((*iter)->members[index]) : NULL;
}

void Groups::Insert(crc32 id, Object *obj) {
	Pool::iterator iter = lower_bound(groups.begin(), groups.end(), id, Group::Cmp());
	if (!(iter != groups.end() && (*iter)->id == id))
		iter = groups.insert(iter, new Group(id));
	else if ((*iter)->Count() > Group::MEMBER_COUNT - 8) {
		(*iter)->Compact();
		ISO_ASSERT((*iter)->Count() < Group::MEMBER_COUNT);
	}
	(*iter)->members.push_back(obj);
}


// Group
namespace ent {
struct Group {
	crc32 group_id;
};
}

namespace iso {
	ISO_DEFUSERCOMPX(ent::Group, 1, "Group") {
		ISO_SETFIELD(0, group_id);
	}};

	template<> void TypeHandler<ent::Group>::Create(const CreateParams &cp, ISO_ptr<ent::Group> t) {
		groups.Insert(t->group_id, cp.obj);
	}
	static TypeHandler<ent::Group> thGroup;
}
