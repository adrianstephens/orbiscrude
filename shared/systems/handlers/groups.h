#ifndef GROUPS_H
#define GROUPS_H

#include "object.h"
#include "utilities.h"
#include "crc_handler.h"
#include "triggers.h"

//-----------------------------------------------------------------------------
//	Groups
//-----------------------------------------------------------------------------
class Groups : public iso::Handles2<Groups, iso::WorldEnd> {
public:
	// MessageFn
	template<typename T> struct _MessageFn {
		T &m;
		_MessageFn(T &_m) : m(_m) {}
		bool operator()(iso::Object *obj) {
			obj->Send(m);
			return true;
		}
	};
	template<typename T> static _MessageFn<T> MessageFn(T &t) { return _MessageFn<T>(t); }

	// IdFn
	struct IdFn {
		iso::crc32 id;
		iso::Object *ref;
		IdFn(iso::crc32 _id)
			: id(_id)
			, ref(NULL)
		{}
		bool operator()(iso::Object *obj) {
			if (obj->GetName() == id) {
				ref = obj;
				return false;
			}
			return true;
		}
		operator iso::Object*() {
			return ref;
		}
	};

	// IndexFn
	struct IndexFn {
		int index;
		iso::Object *ref;
		IndexFn(size_t _index) : index(_index), ref(NULL) {}
		bool operator()(iso::Object *obj) {
			if (!index)
				ref = obj;
			return !(--index < 0);
		}
		operator iso::Object*() {
			return ref;
		}
	};

	// CountFn
	struct CountFn {
		size_t count;
		CountFn() : count(0) {}
		bool operator()(iso::Object *obj) {
			++count;
			return true;
		}
		operator size_t() const {
			return count;
		}
	};

protected:
	// Group
	struct Group {
		enum { MEMBER_COUNT = 128 };
		struct Cmp {
			bool operator()(const Group *group, const iso::crc32 &id) const {
				return group->id < id;
			}
		};
		typedef iso::array<iso::ObjectReference, MEMBER_COUNT> Members;
		iso::crc32 id;
		Members members;
		Group(const iso::crc32 &_id)
			: id(_id)
		{}
		size_t Count() const	{ return members.size(); }
		size_t Compact();
	};
	typedef iso::dynamic_array<Group*> Pool;
	Pool groups;

public:
	size_t			Count(iso::crc32 id) const;
	iso::Object*	At(iso::crc32 id, size_t index) const;
	void			Insert(iso::crc32 id, iso::Object *obj);

	template<typename T> T Enum(iso::crc32 id, T t) const {
		Pool::const_iterator iter = lower_bound(groups.begin(), groups.end(), id, Group::Cmp());
		if (iter != groups.end() && (*iter)->id == id) {
			Group::Members &pool = (*iter)->members;
			Group::Members::iterator _iter = pool.begin();
			while (_iter != pool.end()) {
				if (iso::Object *obj = *_iter) {
					// op
					if (!t(obj))
						break;
					++_iter;
				} else {
					// compact
					obj = pool.pop_back();
					if (_iter != pool.end())
						*_iter = obj;
				}
			}
		}
		return t;
	}

	void operator()(iso::WorldEnd*) {
		for (size_t i = 0, count = groups.size(); i < count; ++i)
			delete groups[i];
		groups.clear();
	}
};

extern Groups groups;

#endif