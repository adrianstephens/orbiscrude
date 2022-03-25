#include "ik.h"
#include "maths/geometry.h"

namespace iso {

//-----------------------------------------------------------------------------
//	JointMapping
//-----------------------------------------------------------------------------

float3x4 *JointMapping::GetRelativeMatrices(float3x4 *mats, Joint *joints) const {
	for (int i = 0; i < num; i++)
		mats[i] = joints[i];
	return mats;
}

float3x4 *JointMapping::GetObjectMatrices(float3x4 *objmats, Joint *joints) const {
	GetRelativeMatrices(objmats, joints);
	for (int i = 0; i < num; i++) {
		if (parents[i] != Pose::INVALID)
			objmats[i] = objmats[parents[i]] * objmats[i];
	}
	return objmats;
}

float3x4 JointMapping::GetObjectMatrix(int i, Joint *joints) const {
	float3x4	m = joints[i];
	for (i = parents[i]; i != Pose::INVALID; i = parents[i])
		m = (float3x4)joints[i] * m;
	return m;
}

Joint *JointMapping::GetJoints(const Pose0 *pose, Joint *joints) const {
	for (int i = 0; i < num; i++) {
		if (mapping[i] != Pose::INVALID)
			joints[i] = pose->joints[mapping[i]];
	}
	return joints;
}

void JointMapping::UpdatePose(Pose *pose, Joint *joints) const {
	for (int i = 0; i < num; i++) {
		if (mapping[i] != Pose::INVALID)
			pose->mats[mapping[i]] = joints[i];
	}
	pose->Update();
}

void JointMapping::ToLocal(Joint *joints) const {
	quaternion	world[IK_MAX_BONES];
	for (int i = 0; i < num; i++) {
		uint8	j = mapping[i];
		if (j != Pose::INVALID) {
			uint8	p = parents[i];
			if (p != Pose::INVALID) {
				p = mapping[p];
				if (p != Pose::INVALID)
					joints[j].rot = joints[j].rot / joints[p].rot;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	JointRotFilter
//-----------------------------------------------------------------------------

void JointRotFilter::Filter(Joint *joints) {
	static float smoothing				= 0.75f;	// [0..1], lower values is closer to the raw data and more noisy
	static float correction				= 0.75f;	// [0..1], higher values correct faster and feel more responsive
	static float prediction				= 0.75f;	// [0..n], how many frames into the future we want to predict
	static float jitter_radius			= 0.10f;	// The deviation angle in radians that defines jitter
	static float max_deviation_radius	= 0.10f;	// The maximum angle in radians that filtered positions are allowed to deviate from raw data

	if (state == 0) {
		for (int i = 0; i < num; i++) {
			entries[i].filtered	= joints[i].rot;
			entries[i].trend	= identity;
		}
		state = 1;
		return;
	}

	bool	state1 = state == 1;
	if (state1)
		state = 2;

	for (int i = 0; i < num; i++) {
		quaternion	filtered		= joints[i].rot;
		quaternion	prev_filtered	= entries[i].filtered;
		quaternion	prev_trend		= entries[i].trend;

		if (state1) {
			filtered	= normalise(prev_filtered + filtered);
		} else {
			// First apply a jitter filter
			float diff	= sqrt(2 * max(1 - cosang(filtered, prev_filtered), 0.f));
			if (diff <= jitter_radius)
				filtered = lerp(prev_filtered, filtered, diff / jitter_radius);
			// Now the double exponential smoothing filter
			filtered	= lerp(filtered, prev_filtered * prev_trend, smoothing);
		}

		ISO_ASSERT(!is_nan(filtered));

		// Use the trend and predict into the future to reduce latency
		quaternion	trend		= normalise(lerp(prev_trend, filtered * ~prev_filtered, correction));
		quaternion	predicted	= slerp(filtered, filtered * trend, prediction);

		// Check that we are not too far away from raw data
		float diff = sqrt(2 * max(1 - cosang(predicted, filtered), 0.f));
		if (diff > max_deviation_radius)
			predicted = slerp(filtered, predicted, max_deviation_radius / diff);

		entries[i].filtered	= normalise(filtered);
		entries[i].trend	= trend;

		joints[i].rot		= normalise(predicted);
	}
}

//-----------------------------------------------------------------------------
//	JointPosConstraint
//-----------------------------------------------------------------------------

#define min_bone2	0.00001f

void JointPosConstraint::Init(const Pose0 *pose) {
	delete[] base_rots;
	delete[] parents;

	num			= pose->Count();
	base_rots	= new quaternion[num];
	parents		= new uint8[num];

	memcpy(parents, pose->parents, num);

	float3x4	objmats[IK_MAX_BONES];
	pose->GetObjectMatrices(objmats);

	for (int c = 0; c < num; c++) {
		int	p = parents[c];
		if (p != Pose::INVALID) {
			int	g = parents[p & 0x7f];
			float3x4	invg	= g == Pose::INVALID ? float3x4(identity) : float3x4(inverse(objmats[g & 0x7f]));
			float3x4	matc	= invg * objmats[c];
			float3x4	matp	= invg * objmats[p & 0x7f];
			float3		dirc	= get_trans(matc) - get_trans(matp);

			quaternion	rotp	= get_rot(matp);

			if (len2(dirc) < min_bone2) {
				parents[c] |= 0x80;
			} else {
				float3	dirp	= g == Pose::INVALID || len2(get_trans(matp)) < min_bone2 ? x_axis : normalise(matp.w);
				rotp			= rotp / quaternion::between(dirp, normalise(dirc));
			}
			base_rots[c]		= rotp;
		}
	}
}

Joint *JointPosConstraint::Calculate(Joint *joints, float4 *pos) const {
	quaternion	world[IK_MAX_BONES];

	for (int c = 0; c < num; c++) {
		uint8	p = parents[c];
		if (p & 0x80)
			continue;

		uint8	g		= parents[p];
		float3	dirc	= normalise(pos[c].xyz - pos[p].xyz);

		if (g == Pose::INVALID) {
			world[p] = joints[p].rot = quaternion::between(x_axis, dirc) * base_rots[c];

		} else {
			quaternion	wq	= world[g & 0x7f];

			if (joints[p].weight = pos[c].w * pos[p].w) {
				quaternion	wqi	= inverse(wq);
				quaternion	rot;

				if (g & 0x80) {
					rot	= quaternion::between(x_axis, wqi * dirc);
				} else {
					float3	dirp = normalise(pos[p].xyz - pos[g].xyz);
					rot	= wqi * quaternion::between(dirp, dirc) * wq;
				}
				joints[p].rot = rot * base_rots[c];
			}

			world[p] = wq * joints[p].rot;
		}
	}
	return joints;
}

//-----------------------------------------------------------------------------
//	JointPosConstraint2
//-----------------------------------------------------------------------------

JointPosConstraint2::entry *JointPosConstraint2::Entries::Init(const Pose0 *pose) {
	delete[] entries;
	num		= pose->Count();
	entries	= new entry[num];
	num_pos	= 0;

	uint8	children[IK_MAX_BONES] = {0};
	uint8	unique[IK_MAX_BONES];

	for (int i = 0; i < num; i++) {
		uint8	p = pose->parents[i];
		entries[i].parent	= p;
		entries[i].end		= Pose::INVALID;
		if (p == Pose::INVALID) {
			unique[i] = num_pos++;
		} else {
			children[p]++;
			if (len2(pose->joints[i].trans4.xyz) < min_bone2)
				unique[i] = unique[p];
			else
				unique[i] = num_pos++;
		}
	}

	for (int i = 0; i < num; i++) {
		if (!children[i])
			entries[i].start = entries[i].end = unique[i];
	}

	for (int i = 0; i < num; i++) {
		uint8	p = pose->parents[i];
		uint8	u = unique[i];
		if (p == Pose::INVALID) {
			entries[i].start	= u;
		} else if (entries[p].end == Pose::INVALID) {
			entries[i].start	= u;
			if (entries[p].start != u)
				entries[p].end = u;
		} else if (children[i]) {
			entries[i].start	= unique[p];
		}
	}
	return entries;
}

position3 *JointPosConstraint2::GetPositions(const float3x4 *objmats, position3 *pos) const {
	for (int i = 0; i < num; i++)
		pos[entries[i].start] = get_trans(objmats[i]);
	return pos;
}

void JointPosConstraint2::Init(const Pose0 *pose, const JointPosConstraint2::entry *_entries, const uint8 *_cone_map, const float4 *_cones) {
	delete[] base_rots;

	num			= pose->Count();
	num_pos		= 0;
	entries		= _entries;
	cone_map	= _cone_map;
	cones		= _cones;
	base_rots	= new quaternion[num];

	float3x4	objmats[IK_MAX_BONES];
	position3	positions[IK_MAX_BONES];

	GetPositions(pose->GetObjectMatrices(objmats), positions);

	for (int i = 0; i < num; i++) {
		const entry	&v		= entries[i];
		int			g		= v.parent;
		float3x4	invg	= g == Pose::INVALID ? float3x4(identity) : float3x4(inverse(objmats[g]));
		quaternion	rotp	= get_rot(invg * objmats[i]);

		if (v.start != v.end) {
			position3	pos0	= invg * positions[v.start];
			position3	pos1	= invg * positions[v.end];
			float3		dir1	= normalise(pos1 - pos0);
			float3		dir0	= g == Pose::INVALID || entries[g].start == v.start ? x_axis : normalise(pos0.v);
			rotp				= rotp / quaternion::between(dir0, dir1);
		}
		base_rots[i] = rotp;
		num_pos		= max(num_pos, uint32(v.start) + 1);
	}
}

Joint *JointPosConstraint2::Calculate(Joint *joints, float4 *pos) const {
	quaternion	world[IK_MAX_BONES];
	for (int i = 0; i < num; i++) {
		const entry	&v	= entries[i];

		if (v.start == v.end) {
			joints[i].rot = base_rots[i];
			continue;
		}

		uint8		g	= v.parent;
		quaternion	wq	= g == Pose::INVALID ? quaternion(identity) : world[g];

		if (joints[i].weight = pos[v.end].w * pos[v.start].w) {
			quaternion	wqi		= inverse(wq);
			float3		dir1	= wqi * normalise(pos[v.end].xyz - pos[v.start].xyz);
			float3		dir0	= g == Pose::INVALID || entries[g].start == v.start ? x_axis : wqi * normalise(pos[v.start].xyz - pos[entries[g].start].xyz);

			uint8		c		= cone_map ? cone_map[i] : Pose::INVALID;
			if (c != Pose::INVALID) {
				float	cosa	= dot(cones[c].xyz, dir1);
				if (cosa < cones[c].w)
					dir1 = cones[c].xyz * cones[c].w + (dir1 - cones[c].xyz * cosa) * sqrt((1 - square(cones[c].w)) / max(1 - square(cosa), 0.001f));
			}

			joints[i].rot = quaternion::between(dir0, dir1) * base_rots[i];
		}

		world[i] = wq * joints[i].rot;
	}
	return joints;
}

//-----------------------------------------------------------------------------
//	JointRotConstraint
//-----------------------------------------------------------------------------

void JointRotConstraint::Init(const Pose0 *pose, int _num, const uint8 *_mapping, const float4 *_cones) {
	num		= _num;
	mapping	= _mapping;
	cones	= _cones;
}

void JointRotConstraint::Calculate(Joint *joints) const {
	for (int i = 0; i < num; i++) {
		int		j		= mapping[i];
		float3	dirc	= cones[i].xyz;
		float	cosm	= cones[i].w;
		float3	dira	= joints[j].rot * (float3)x_axis;
		float	cosa	= dot(dirc, dira);
		if (cosa < cosm) {
			float		cosdh	= cos_half(cos_sub(cosa, cosm));
			float		sindh	= sin_cos(cosdh);
			quaternion	q(normalise(cross(dirc, dira)) * sindh, cosdh);
			joints[j].rot = joints[j].rot * q;
		}
	}
}

} // namespace iso
