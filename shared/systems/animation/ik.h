#ifndef IK_H
#define IK_H

#include "object.h"

namespace iso {

#define IK_MAX_BONES	128

struct JointRotFilter {

	struct entry {
		quaternion	filtered;
		quaternion	trend;
	};

	uint8		num, state;
	entry		*entries;

	void	Init(size_t _num, entry *_entries) {
		num		= (uint8)_num;
		state	= 0;
		entries	= _entries;
	}

	JointRotFilter() : entries(0) {}
	JointRotFilter(int _num, entry *_entries) : num(_num), state(0), entries(_entries)	{}

	void	Filter(Joint *joints);
};

struct JointMapping {
	uint8		num, num2;
	uint8		mapping[IK_MAX_BONES];
	const uint8	*parents;

	void	Init(const Pose0 *pose, size_t _num, const char **names, const uint8 *_parents = 0) {
		ISO_ASSERT(_num < 256);
		num		= uint8(_num);
		num2	= pose->Count();
		parents = _parents;
		for (int i = 0; i < num; i++)
			mapping[i]	= pose->Find(crc32(names[i]));
	}
	void	Init(const Pose0 *pose, const Pose0 *target) {
		num		= target->Count();
		num2	= pose->Count();
		parents = target->parents;
		for (int i = 0; i < num; i++)
			mapping[i]	= pose->Find(target->joints[i].name);
	}
	void	InitReverse(const uint8 *revmap, int _num, int _num2) {
		num		= _num;
		num2	= _num2;
		memset(mapping, Pose::INVALID, num);
		for (int i = 0; i < num2; i++)
			mapping[revmap[i]] = i;
	}

	JointMapping() : num(0), num2(0), parents(0) {}
	JointMapping(const Pose0 *pose, size_t _num, const char **names, const uint8 *_parents = 0) {
		Init(pose, _num, names, _parents);
	}
	JointMapping(const Pose0 *pose, const Pose0 *target) {
		Init(pose, target);
	}

	operator const uint8*()		const	{ return mapping;	}
	int		Count()				const	{ return num;		}
	int		CountIndices()		const	{ return num2;		}

	uint8	reverse(int i)		const {
		for (int j = 0; j < num; j++) {
			if (mapping[j] == i)
				return j;
		}
		return Pose::INVALID;
	}

	uint8*	reverse(uint8 *revmap) const {
		memset(revmap, Pose::INVALID, num2);
		for (int i = 0; i < num; i++)
			revmap[mapping[i]] = i;
		return revmap;
	}

	float3x4*	GetRelativeMatrices(float3x4 *mats, Joint *joints)	const;
	float3x4*	GetObjectMatrices(float3x4 *objmats, Joint *joints)	const;
	float3x4	GetObjectMatrix(int i, Joint *joints)				const;
	Joint*		GetJoints(const Pose0 *pose, Joint *joints)			const;
	void		UpdatePose(Pose *pose, Joint *joints)				const;
	void		ToLocal(Joint *joints)								const;
};

struct RemappedPose {
	Pose0	*pose;
	RemappedPose(const Pose0 *from, const JointMapping &map) {
		int	n = map.Count();
		pose = Pose0::Create(n);
		for (int i = 0; i < n; i++) {
			pose->parents[i] = map.parents[i];
			pose->joints[i] = from->joints[map[i]];
		}
	}
	operator const Pose0*()	const	{ return pose; }
};

struct JointPosConstraint {
	uint32		num;
	uint8		*parents;
	quaternion	*base_rots;

	void		Init(const Pose0 *pose);
	Joint*		Calculate(Joint *joints, float4 *pos) const;
	int			Count()	const	{ return num; }

	JointPosConstraint() : num(0), parents(0), base_rots(0)	{}
	JointPosConstraint(const Pose0 *pose) : parents(0), base_rots(0) { Init(pose); }
	~JointPosConstraint() { delete[] parents; delete[] base_rots; }
};

struct JointPosConstraint2 {
	struct entry {
		uint8 start, end, parent;
	};

	struct Entries {
		uint32		num, num_pos;
		entry		*entries;
		entry		*Init(const Pose0 *pose);
		Entries() : entries(0)				{}
		Entries(const Pose0 *pose)			{ Init(pose);				}
		~Entries()							{ delete[] entries;			}
		operator	entry*()		const	{ return entries;			}
		int			Count()			const	{ return num;				}
		int			CountPos()		const	{ return num_pos;			}
		int			PosIndex(int i)	const	{ return entries[i].start;	}
	};

	uint32			num, num_pos;
	const entry		*entries;
	const float4	*cones;
	const uint8		*cone_map;
	quaternion		*base_rots;

	void		Init(const Pose0 *pose, const JointPosConstraint2::entry *_entries, const uint8 *_cone_map = 0, const float4 *_cones = 0);
	Joint*		Calculate(Joint *joints, float4 *pos) const;
	position3*	GetPositions(const float3x4 *objmats, position3 *pos) const;
	int			Count()				const	{ return num;				}
	int			CountPos()			const	{ return num_pos;			}
	int			PosIndex(int i)		const	{ return entries[i].start;	}
	int			JointIndex(int i)	const {
		for (int j = 0; j < num; j++) {
			if (entries[j].start == i)
				return j;
		}
		return Pose::INVALID;
	}

	JointPosConstraint2() : num(0), num_pos(0), entries(0), base_rots(0) {}
	~JointPosConstraint2() { delete[] base_rots; }
};

struct JointRotConstraint {
	uint32			num;
	const uint8		*mapping;
	const float4	*cones;

	void		Init(const Pose0 *pose, int _num, const uint8 *_mappings, const float4 *_cones);
	void		Calculate(Joint *joints) const;
	int			Count()	const	{ return num; }

	JointRotConstraint()	{}
	JointRotConstraint(const Pose0 *pose, int _num, const uint8 *_mapping, const float4 *_cones) {
		Init(pose, _num, _mapping, _cones);
	}
};

} // namespace iso

#endif //IK_H
