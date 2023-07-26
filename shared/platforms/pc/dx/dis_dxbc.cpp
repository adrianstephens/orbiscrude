#include "disassembler.h"
#include "base/tree.h"
#include "base/strings.h"
#include "dx_opcodes.h"
#include "dx_shaders.h"

using namespace iso;
using namespace dx;

#define ISO_ERROR(...) do {ISO_TRACEF(__VA_ARGS__); _iso_break(); } while(0)

struct OpcodeInfo {
	const char	*name;
	uint32		num_ops:4, result:4;
};
enum {
	U	= 1,
	I	= 2,
	F	= 3,
	D	= 4,
	F16	= 5,
};
OpcodeInfo opcode_info[] = {
	{"add",												3, F},
	{"and",												3, U},
	{"break",											0, 0},
	{"breakc",											1, 0},
	{"call",											1, 0},
	{"callc",											2, 0},
	{"case",											1, 0},
	{"continue",										0, 0},
	{"continuec",										1, 0},
	{"cut",												1, 0},
	{"default",											0, 0},
	{"deriv_rtx",										2, F},
	{"deriv_rty",										2, F},
	{"discard",											1, 0},
	{"div",												3, F},
	{"dp2",												3, F},
	{"dp3",												3, F},
	{"dp4",												3, F},
	{"else",											0, 0},
	{"emit",											0, 0},
	{"emitthencut",										0, 0},
	{"endif",											0, 0},
	{"endloop",											0, 0},
	{"endswitch",										0, 0},
	{"eq",												3, U},
	{"exp",												2, F},
	{"frc",												2, F},
	{"ftoi",											2, I},
	{"ftou",											2, U},
	{"ge",												3, U},
	{"iadd",											3, I},
	{"if",												1, 0},
	{"ieq",												3, U},
	{"ige",												3, U},
	{"ilt",												3, U},
	{"imad",											4, I},
	{"imax",											3, I},
	{"imin",											3, I},
	{"imul",											4, I},
	{"ine",												3, U},
	{"ineg",											2, I},
	{"ishl",											3, I},
	{"ishr",											3, I},
	{"itof",											2, F},
	{"label",											1, 0},
	{"ld",												3, F},
	{"ld_ms",											4, F},
	{"log",												2, F},
	{"loop",											0, 0},
	{"lt",												3, U},
	{"mad",												4, F},
	{"min",												3, F},
	{"max",												3, F},
	{"customdata",										0, 0},
	{"mov",												2, F},
	{"movc",											4, F},
	{"mul",												3, F},
	{"ne",												3, U},
	{"nop",												0, 0},
	{"not",												2, U},
	{"or",												3, U},
	{"resinfo",											3, 0},
	{"ret",												0, 0},
	{"retc",											1, 0},
	{"round_ne",										2, F},
	{"round_ni",										2, F},
	{"round_pi",										2, F},
	{"round_z",											2, F},
	{"rsq",												2, F},
	{"sample",											4, F},
	{"sample_c",										5, F},
	{"sample_c_lz",										5, F},
	{"sample_l",										5, F},
	{"sample_d",										6, F},
	{"sample_b",										5, F},
	{"sqrt",											2, F},
	{"switch",											1, I},
	{"sincos",											3, F},
	{"udiv",											4, U},
	{"ult",												3, U},
	{"uge",												3, U},
	{"umul",											4, U},
	{"umad",											4, U},
	{"umax",											3, U},
	{"umin",											3, U},
	{"ushr",											3, U},
	{"utof",											2, F},
	{"xor",												3, U},
	{"dcl_resource",									0, 0},
	{"dcl_constant_buffer",								0, 0},
	{"dcl_sampler",										0, 0},
	{"dcl_index_range",									0, 0},
	{"dcl_gs_output_primitive_topology",				0, 0},
	{"dcl_gs_input_primitive",							0, 0},
	{"dcl_max_output_vertex_count",						0, 0},
	{"dcl_input",										0, 0},
	{"dcl_input_sgv",									0, 0},
	{"dcl_input_siv",									0, 0},
	{"dcl_input_ps",									0, 0},
	{"dcl_input_ps_sgv",								0, 0},
	{"dcl_input_ps_siv",								0, 0},
	{"dcl_output",										0, 0},
	{"dcl_output_sgv",									0, 0},
	{"dcl_output_siv",									0, 0},
	{"dcl_temps",										0, 0},
	{"dcl_indexable_temp",								0, 0},
	{"dcl_global_flags",								0, 0},
	{"reserved0",										0, 0},
	{"lod",												4, F},
	{"gather4",											4, F},
	{"sample_pos",										3, F},
	{"sample_info",										2, F},
	{"reserved1",										0, 0},
	{"hs_decls",										0, 0},
	{"hs_control_point_phase",							0, 0},
	{"hs_fork_phase",									0, 0},
	{"hs_join_phase",									0, 0},
	{"emit_stream",										1, 0},
	{"cut_stream",										0, 0},
	{"emitthencut_stream",								1, 0},
	{"interface_call",									1, 0},
	{"bufinfo",											2, U},
	{"deriv_rtx_coarse",								2, F},
	{"deriv_rtx_fine",									2, F},
	{"deriv_rty_coarse",								2, F},
	{"deriv_rty_fine",									2, F},
	{"gather4_c",										5, F},
	{"gather4_po",										5, F},
	{"gather4_po_c",									6, F},
	{"rcp",												2, F},
	{"f32tof16",										2, F16},
	{"f16tof32",										2, F},
	{"uaddc",											4, U},
	{"usubb",											4, U},
	{"countbits",										2, U},
	{"firstbit_hi",										2, U},
	{"firstbit_lo",										2, U},
	{"firstbit_shi",									2, U},
	{"ubfe",											4, U},
	{"ibfe",											4, U},
	{"bfi",												5, U},
	{"bfrev",											2, U},
	{"swapc",											5, F},
	{"dcl_stream",										0, 0},
	{"dcl_function_body",								0, 0},
	{"dcl_function_table",								0, 0},
	{"dcl_interface",									0, 0},
	{"dcl_input_control_point_count",					0, 0},
	{"dcl_output_control_point_count",					0, 0},
	{"dcl_tess_domain",									0, 0},
	{"dcl_tess_partitioning",							0, 0},
	{"dcl_tess_output_primitive",						0, 0},
	{"dcl_hs_max_tessfactor",							0, 0},
	{"dcl_hs_fork_phase_instance_count",				0, 0},
	{"dcl_hs_join_phase_instance_count",				0, 0},
	{"dcl_thread_group",								0, 0},
	{"dcl_unordered_access_view_typed",					0, 0},
	{"dcl_unordered_access_view_raw",					0, 0},
	{"dcl_unordered_access_view_structured",			0, 0},
	{"dcl_thread_group_shared_memory_raw",				0, 0},
	{"dcl_thread_group_shared_memory_structured",		0, 0},
	{"dcl_resource_raw",								0, 0},
	{"dcl_resource_structured",							0, 0},
	{"ld_uav_typed",									3, F},
	{"store_uav_typed",									3, 0},
	{"ld_raw",											3, F},
	{"store_raw",										3, 0},
	{"ld_structured",									4, F},
	{"store_structured",								4, 0},
	{"atomic_and",										3, U},
	{"atomic_or",										3, U},
	{"atomic_xor",										3, U},
	{"atomic_cmp_store",								4, 0},
	{"atomic_iadd",										3, I},
	{"atomic_imax",										3, I},
	{"atomic_imin",										3, I},
	{"atomic_umax",										3, U},
	{"atomic_umin",										3, U},
	{"imm_atomic_alloc",								2, 0},
	{"imm_atomic_consume",								2, 0},
	{"imm_atomic_iadd",									4, I},
	{"imm_atomic_and",									4, U},
	{"imm_atomic_or",									4, U},
	{"imm_atomic_xor",									4, U},
	{"imm_atomic_exch",									4, 0},
	{"imm_atomic_cmp_exch",								5, 0},
	{"imm_atomic_imax",									4, I},
	{"imm_atomic_imin",									4, I},
	{"imm_atomic_umax",									4, U},
	{"imm_atomic_umin",									4, U},
	{"sync",											0, 0},
	{"dadd",											3, D},
	{"dmax",											3, D},
	{"dmin",											3, D},
	{"dmul",											3, D},
	{"deq",												3, U},
	{"dge",												3, U},
	{"dlt",												3, U},
	{"dne",												3, U},
	{"dmov",											2, D},
	{"dmovc",											4, D},
	{"dtof",											2, F},
	{"ftod",											2, D},
	{"eval_snapped",									3, F},
	{"eval_sample_index",								3, F},
	{"eval_centroid",									2, F},
	{"dcl_gs_instance_count",							0, 0},
	{"abort",											0, 0},
	{"debugbreak",										0, 0},
	{"reserved2",										0, 0},
	{"ddiv",											3, D},
	{"dfma",											4, D},
	{"drcp",											2, D},
	{"msad",											4, D},
	{"dtoi",											2, I},
	{"dtou",											2, U},
	{"itod",											2, D},
	{"utod",											2, D},
	{"reserved3",										0, 0},
	{"gather4_feedback",								5, F},
	{"gather4_c_feedback",								6, F},
	{"gather4_po_feedback",								6, F},
	{"gather4_po_c_feedback",							7, F},
	{"ld_feedback",										4, F},
	{"ld_ms_feedback",									5, F},
	{"ld_uav_typed_feedback",							4, F},
	{"ld_raw_feedback",									4, F},
	{"ld_structured_feedback",							5, F},
	{"sample_l_feedback",								6, F},
	{"sample_c_lz_feedback",							6, F},
	{"sample_clamp_feedback",							6, F},
	{"sample_b_clamp_feedback",							6, F},
	{"sample_d_clamp_feedback",							7, F},
	{"sample_c_clamp_feedback",							6, F},
	{"check_access_fully_mapped",						2, U},
};

namespace iso {

const char *to_string(OpcodeType op) {
	return opcode_info[op].name;
}

const char *to_string(ResinfoRetType type) {
	switch (type) {
		case RETTYPE_FLOAT:		return "float";
		case RETTYPE_RCPFLOAT:	return "rcpfloat";
		case RETTYPE_UINT:		return "uint";
		default:
			ISO_ERROR("Unknown type: %u", type);
			return "";
	}
}

static const char *PrimitiveTopology_names[] = {
	"point",
	"linelist",
	"linestrip",
	"trianglelist",
	"trianglestrip",
	"linelist_adj",
	"linestrip_adj",
	"trianglelist_adj",
	"trianglestrip_adj",
};

const char *to_string(ResourceDimension dim) {
	switch (dim) {
		case RESOURCE_DIMENSION_UNKNOWN:				return "unknown";
		case RESOURCE_DIMENSION_BUFFER:					return "buffer";
		case RESOURCE_DIMENSION_TEXTURE1D:				return "texture1d";
		case RESOURCE_DIMENSION_TEXTURE2D:				return "texture2d";
		case RESOURCE_DIMENSION_TEXTURE2DMS:			return "texture2dms";
		case RESOURCE_DIMENSION_TEXTURE3D:				return "texture3d";
		case RESOURCE_DIMENSION_TEXTURECUBE:			return "texturecube";
		case RESOURCE_DIMENSION_TEXTURE1DARRAY:			return "texture1darray";
		case RESOURCE_DIMENSION_TEXTURE2DARRAY:			return "texture2darray";
		case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:		return "texture2dmsarray";
		case RESOURCE_DIMENSION_TEXTURECUBEARRAY:		return "texturecubearray";
		case RESOURCE_DIMENSION_RAW_BUFFER:				return "rawbuffer";
		case RESOURCE_DIMENSION_STRUCTURED_BUFFER:		return "structured_buffer";
		default:
			ISO_ERROR("Unknown dim: %u", dim);
			return "";
	}
}

const char *to_string(ResourceReturnType::Type type) {
	switch (type) {
		case ResourceReturnType::TYPE_UNORM:			return "unorm";
		case ResourceReturnType::TYPE_SNORM:			return "snorm";
		case ResourceReturnType::TYPE_SINT:				return "sint";
		case ResourceReturnType::TYPE_UINT:				return "uint";
		case ResourceReturnType::TYPE_FLOAT:			return "float";
		case ResourceReturnType::TYPE_MIXED:			return "mixed";
		case ResourceReturnType::TYPE_DOUBLE:			return "double";
		case ResourceReturnType::TYPE_CONTINUED:		return "continued";
		case ResourceReturnType::TYPE_UNUSED:			return "unused";
		default:
			ISO_ERROR("Unknown type: %u", type);
			return "";
	}
}

string_accum &operator<<(string_accum &a, ResourceReturnType r) {
	return a << '(' << r.x << ", " << r.y << ", " << r.z << ", " << r.w << ')';
}

const char *to_string(SystemValue x) {
	switch (x) {
		case SV_POSITION:						return "position";
		case SV_CLIP_DISTANCE:					return "clipdistance";
		case SV_CULL_DISTANCE:					return "culldistance";
		case SV_RENDER_TARGET_ARRAY_INDEX:		return "rendertarget_array_index";
		case SV_VIEWPORT_ARRAY_INDEX:			return "viewport_array_index";
		case SV_VERTEX_ID:						return "vertexid";
		case SV_PRIMITIVE_ID:					return "primitiveid";
		case SV_INSTANCE_ID:					return "instanceid";
		case SV_IS_FRONT_FACE:					return "isfrontface";
		case SV_SAMPLE_INDEX:					return "sampleidx";

		// tessellation factors don't correspond directly to their enum values
		case SV_FINAL_QUAD_EDGE_TESSFACTOR0:	return "finalQuadUeq0EdgeTessFactor";
		case SV_FINAL_QUAD_EDGE_TESSFACTOR1:	return "finalQuadVeq0EdgeTessFactor";
		case SV_FINAL_QUAD_EDGE_TESSFACTOR2:	return "finalQuadUeq1EdgeTessFactor";
		case SV_FINAL_QUAD_EDGE_TESSFACTOR3:	return "finalQuadVeq1EdgeTessFactor";
		case SV_FINAL_QUAD_INSIDE_TESSFACTOR0:	return "finalQuadUInsideTessFactor";
		case SV_FINAL_QUAD_INSIDE_TESSFACTOR1:	return "finalQuadVInsideTessFactor";
		case SV_FINAL_TRI_EDGE_TESSFACTOR0:		return "finalTriUeq0EdgeTessFactor";
		case SV_FINAL_TRI_EDGE_TESSFACTOR1:		return "finalTriVeq0EdgeTessFactor";
		case SV_FINAL_TRI_EDGE_TESSFACTOR2:		return "finalTriWeq0EdgeTessFactor";
		case SV_FINAL_TRI_INSIDE_TESSFACTOR0:	return "finalTriInsideTessFactor";
		case SV_FINAL_LINE_DETAIL_TESSFACTOR0:	return "finalLineEdgeTessFactor";
		case SV_FINAL_LINE_DENSITY_TESSFACTOR0:	return "finalLineInsideTessFactor";

		case SV_TARGET:							return "target";
		case SV_DEPTH:							return "depth";
		case SV_COVERAGE:						return "coverage";
		case SV_DEPTH_GREATER_EQUAL:			return "depthgreaterequal";
		case SV_DEPTH_LESS_EQUAL:				return "depthlessequal";
		default:
			ISO_ERROR("Unknown name: %u", x);
			return "";
	}
}

}

struct _no_swizzle {
	const ASMOperand	&op;
	_no_swizzle(const ASMOperand	&_op) : op(_op) {}
};

_no_swizzle no_swizzle(const ASMOperand	&op) { return op; }

//namespace iso {

string_accum &put_indices(string_accum &a, const ASMOperand &op);
string_accum &put(string_accum &a, const ASMOperand &op, bool swizzle);

string_accum &operator<<(string_accum &a, const _no_swizzle &op) {
	return put(a, op.op, false);
}

string_accum &operator<<(string_accum &a, const ASMOperand &op) {
	return put(a, op, true);
}

string_accum &operator<<(string_accum &a, const ASMIndex &ix)	{
	if (ix.relative)
		a << '[' << ix.operand << " + ";

	if (ix.absolute)
		a.format("%llu", ix.index);

	else if (ix.relative)
		a << "0";

	if (ix.relative)
		a << ']';

	return a;
}
//}

string_accum &put_indices(string_accum &a, const ASMOperand &op) {
#if 1
	for (size_t i = 0; i < op.indices.size(); i++) {
		if (i == 0)
			a << op.indices[i];
		else if (op.indices[i].relative)
			a << op.indices[i];
		else
			a << '[' << op.indices[i] << ']';
	}
#else
	if (op.indices.size() == 1 && op.type != Operand::TYPE_IMMEDIATE_CONSTANT_BUFFER) {
		a << op.indices[0];
	} else {
		for (size_t i = 0; i < op.indices.size(); i++) {
			if (i == 0 && (op.type == Operand::TYPE_CONSTANT_BUFFER || op.type == Operand::TYPE_INDEXABLE_TEMP))
				a << op.indices[i];
			else if (op.indices[i].relative)
				a << op.indices[i];
			else
				a << '[' << op.indices[i] << ']';
		}
	}
#endif
	return a;
}

string_accum &put_comps(string_accum &a, const uint32 values[], uint32 numComps) {
	bool floatOutput = false;

	float *vf = (float*)values;
	int32 *vi = (int32*)values;
	for (uint32 i = 0; i < numComps; i++) {
		uint32 exponent = vi[i] & 0x7f800000;
		if (exponent != 0 && exponent != 0x7f800000)
			floatOutput = true;
	}

	for (uint32 i = 0; i < numComps; i++) {
		if (floatOutput)
			a.format("%f", vf[i]);
		else if (vi[i] <= 10000 && vi[i] >= -10000)		// print small ints straight up, otherwise as hex
			a.format("%d", vi[i]);
		else
			a.format("0x%08x", vi[i]);

		if (i + 1 < numComps)
			a << ", ";
	}

	return a;
}

string_accum &put(string_accum &a, const ASMOperand &op, bool swizzle) {
	switch (op.modifier) {
		case OPERAND_MODIFIER_NEG:		a << '-'; break;
		case OPERAND_MODIFIER_ABS:		a << "abs("; break;
		case OPERAND_MODIFIER_ABSNEG:	a << "-abs("; break;
	}

	switch (op.type) {
		case Operand::TYPE_NULL:		a << "null"; break;
		case Operand::TYPE_INTERFACE:
			ISO_ASSERT(op.indices.size() == 2);
			a << "fp" << op.indices[0] << "[" << op.indices[1] << "][" << op.funcNum << ']';
			break;

		case Operand::TYPE_TEMP:						put_indices(a << "r", op);		break;
		case Operand::TYPE_RESOURCE:					put_indices(a << "t", op);		break;
		case Operand::TYPE_SAMPLER:						put_indices(a << "s", op);		break;
		case Operand::TYPE_OUTPUT:						put_indices(a << "o", op);		break;
		case Operand::TYPE_STREAM:						put_indices(a << "m", op);		break;
		case Operand::TYPE_THREAD_GROUP_SHARED_MEMORY:	put_indices(a << "g", op);		break;
		case Operand::TYPE_UNORDERED_ACCESS_VIEW:		put_indices(a << "u", op);		break;
		case Operand::TYPE_FUNCTION_BODY:				put_indices(a << "fb", op);		break;

		case Operand::TYPE_CONSTANT_BUFFER:				put_indices(a << "cb", op);		break;
		case Operand::TYPE_IMMEDIATE_CONSTANT_BUFFER:	put_indices(a << "icb", op);	break;
		case Operand::TYPE_INDEXABLE_TEMP:				put_indices(a << "x", op);		break;
		case Operand::TYPE_INPUT:						put_indices(a << "v", op);		break;
		case Operand::TYPE_INPUT_CONTROL_POINT:			put_indices(a << "vicp", op);	break;
		case Operand::TYPE_INPUT_PATCH_CONSTANT:		put_indices(a << "vpc", op);	break;
		case Operand::TYPE_THIS_POINTER:				put_indices(a << "vocp", op);	break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT:		put_indices(a << "this", op);	break;

		case Operand::TYPE_IMMEDIATE32:
			ISO_ASSERT(op.indices.size() == 0);
			put_comps(a << "l(", op.values, op.NumComponents()) << ")";
			swizzle = false;
			break;

		case Operand::TYPE_IMMEDIATE64: {
			double *dv = (double*)op.values;
			a.format("d(%lfl, %lfl)", dv[0], dv[1]);
			swizzle = false;
			break;
		}
		case Operand::TYPE_RASTERIZER:							a << "rasterizer";					break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				a << "vOutputControlPointID";		break;
		case Operand::TYPE_INPUT_DOMAIN_POINT:					a << "vDomain";						break;
		case Operand::TYPE_INPUT_PRIMITIVEID:					a << "vPrim";						break;
		case Operand::TYPE_INPUT_COVERAGE_MASK:					a << "vCoverageMask";				break;
		case Operand::TYPE_INPUT_GS_INSTANCE_ID:				a << "vGSInstanceID";				break;
		case Operand::TYPE_INPUT_THREAD_ID:						a << "vThreadID";					break;
		case Operand::TYPE_INPUT_THREAD_GROUP_ID:				a << "vThreadGroupID";				break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			a << "vThreadIDInGroup";			break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	a << "vThreadIDInGroupFlattened";	break;
		case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				a << "vForkInstanceID";				break;
		case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				a << "vJoinInstanceID";				break;
		case Operand::TYPE_OUTPUT_DEPTH:						a << "oDepth";						break;
		case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				a << "oDepthLessEqual";				break;
		case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			a << "oDepthGreaterEqual";			break;
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				a << "oMask";						break;
		default:
			ISO_ERROR("Unsupported system value semantic %d", op.type);
			a << "oUnsupported";
			break;
	}

	if (swizzle) {
		switch (op.selection_mode) {
			case Operand::SELECTION_MASK:
				if (op.swizzle_bits)
					a << '.' << onlyif(op.mask.x, 'x') << onlyif(op.mask.y, 'y') << onlyif(op.mask.z, 'z') << onlyif(op.mask.w, 'w');
				break;

			case Operand::SELECTION_SWIZZLE:
				a << '.';
				for (int bits = op.swizzle_bits | 0x100; bits != 1; bits >>= 2)
					a << "xyzw"[bits & 3];
				break;

			case Operand::SELECTION_SELECT_1:
				a << '.' << "xyzw"[op.swizzle.x];
				break;
		}
	}

	switch (op.precision) {
		case PRECISION_FLOAT10:			a << "{min2_8f as def32}"; break;
		case PRECISION_FLOAT16:			a << "{min16f as def32}"; break;
		case PRECISION_UINT16:			a << "{min16u}"; break;
		case PRECISION_SINT16:			a << "{min16i}"; break;
	}

	switch (op.modifier) {
		case OPERAND_MODIFIER_ABS:
		case OPERAND_MODIFIER_ABSNEG:	a << ')';
	}
	return a;
}

string_accum &put_indices(string_accum &a, range<uint32*> indices) {
	for (int i = 0; i < indices.size32(); i++) {
		if (i == 0)
			a << indices[i];
		else
			a << '[' << indices[i] << ']';
	}
	return a;
}

string_accum &dx::RegisterName(string_accum &a, const Operand::Type type, range<uint32*> indices, int fields) {

	switch (type) {
		default:										a << "?";		break;
		case Operand::TYPE_NULL:						a << "null";	break;

		case Operand::TYPE_TEMP:						put_indices(a << "r", indices);		break;
		case Operand::TYPE_RESOURCE:					put_indices(a << "t", indices);		break;
		case Operand::TYPE_SAMPLER:						put_indices(a << "s", indices);		break;
		case Operand::TYPE_OUTPUT:						put_indices(a << "o", indices);		break;
		case Operand::TYPE_STREAM:						put_indices(a << "m", indices);		break;
		case Operand::TYPE_THREAD_GROUP_SHARED_MEMORY:	put_indices(a << "g", indices);		break;
		case Operand::TYPE_UNORDERED_ACCESS_VIEW:		put_indices(a << "u", indices);		break;
		case Operand::TYPE_FUNCTION_BODY:				put_indices(a << "fb", indices);	break;

		case Operand::TYPE_CONSTANT_BUFFER:				put_indices(a << "cb", indices);	break;
		case Operand::TYPE_IMMEDIATE_CONSTANT_BUFFER:	put_indices(a << "icb", indices);	break;
		case Operand::TYPE_INDEXABLE_TEMP:				put_indices(a << "x", indices);		break;
		case Operand::TYPE_INPUT:						put_indices(a << "v", indices);		break;
		case Operand::TYPE_INPUT_CONTROL_POINT:			put_indices(a << "vicp", indices);	break;
		case Operand::TYPE_INPUT_PATCH_CONSTANT:		put_indices(a << "vpc", indices);	break;
		case Operand::TYPE_THIS_POINTER:				put_indices(a << "this", indices);	break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT:		put_indices(a << "vocp", indices);	break;

		case Operand::TYPE_RASTERIZER:							a << "rasterizer";					break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				a << "vOutputControlPointID";		break;
		case Operand::TYPE_INPUT_DOMAIN_POINT:					a << "vDomain";						break;
		case Operand::TYPE_INPUT_PRIMITIVEID:					a << "vPrim";						break;
		case Operand::TYPE_INPUT_COVERAGE_MASK:					a << "vCoverageMask";				break;
		case Operand::TYPE_INPUT_GS_INSTANCE_ID:				a << "vGSInstanceID";				break;
		case Operand::TYPE_INPUT_THREAD_ID:						a << "vThreadID";					break;
		case Operand::TYPE_INPUT_THREAD_GROUP_ID:				a << "vThreadGroupID";				break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			a << "vThreadIDInGroup";			break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	a << "vThreadIDInGroupFlattened";	break;
		case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				a << "vForkInstanceID";				break;
		case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				a << "vJoinInstanceID";				break;
		case Operand::TYPE_OUTPUT_DEPTH:						a << "oDepth";						break;
		case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				a << "oDepthLessEqual";				break;
		case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			a << "oDepthGreaterEqual";			break;
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				a << "oMask";						break;

	}

	if (fields)
		a << '.' << onlyif(fields & 1, 'x') << onlyif(fields & 2, 'y') << onlyif(fields & 4, 'z') << onlyif(fields & 8, 'w');

	return a;
}

size_t Opcode::NumOperands() const {
	return opcode_info[Type].num_ops;
}

bool ASMOperand::Extract(const uint32 *&p) {
	*(Operand*)this	= (Operand&)*p++;

	for (bool ext = extended; ext;) {
		ExtendedOperand token	= (ExtendedOperand&)*p++;

		if (token.Type == EXTENDED_OPERAND_MODIFIER) {
			modifier	= (OperandModifier)token.Modifier;
			precision	= (MinimumPrecision)token.MinPrecision;
		} else {
			ISO_ERROR("Unexpected extended operand modifier");
		}

		ext = token.extended == 1;
	}

	if (type == TYPE_IMMEDIATE32 || type == TYPE_IMMEDIATE64) {
		for (uint32 i = 0, n = NumComponents(); i < n; i++)
			values[i] = *p++;
		if (num_components == NUMCOMPS_1)
			swizzle.x = swizzle.y = swizzle.z = swizzle.w = 0;
	}

	if (selection_mode == SELECTION_SELECT_1)
		swizzle.y = swizzle.z = swizzle.w = swizzle.x;

	uint32 rep[] = {
		index0,
		index1,
		index2,
	};

	indices.resize(index_dim);

	for (int i = 0; i < index_dim; i++) {
		uint32	x = rep[i];
		indices[i].absolute	= x != INDEX_RELATIVE;
		indices[i].relative	= x != INDEX_IMMEDIATE32 && x != INDEX_IMMEDIATE64;

		if (indices[i].absolute) {
			indices[i].index = *p++;
			if (x == INDEX_IMMEDIATE64_PLUS_RELATIVE || x == INDEX_IMMEDIATE64)
				indices[i].index = (indices[i].index << 32) | *p++;
		}

		if (indices[i].relative)
			indices[i].operand.Extract(p);
	}

	return true;
}

int ASMOperand::Index() const {
	ISO_ASSERT(indices.size() == 1 && !indices[0].relative);
	return indices[0].index;
}

int ASMOperand::IndexMulti() const {
	switch (indices.size()) {
		default:
		case 0:
			return 0;
		case 1:
			ISO_ASSERT(!indices[0].relative);
			return indices[0].index;
		case 2: {
			ISO_ASSERT(!indices[0].relative && !indices[1].relative);
			uint32	x = indices[0].index, y = indices[1].index, r = 0, s = 0;
			while (x || y) {
				r |= ((x & 1) | ((y & 1) << 1)) << s;
				x >>= 1;
				y >>= 1;
				s += 2;
			}
			return r;
		}
		case 3: {
			ISO_ASSERT(!indices[0].relative && !indices[1].relative && !indices[2].relative);
			uint32	x = indices[0].index, y = indices[1].index, z = indices[2].index, r = 0, s = 0;
			while (x || y || z) {
				r |= ((x & 1) | ((y & 1) << 1) | ((z & 1) << 2)) << s;
				x >>= 1;
				y >>= 1;
				z >>= 1;
				s += 3;
			}
			return r;
		}
	}
}

int ASMOperand::Mask() const {
	switch (selection_mode) {
		default:
		case Operand::SELECTION_MASK:
			return swizzle_bits;

		case Operand::SELECTION_SWIZZLE: {
			uint8	m = 0;
			for (int bits = swizzle_bits | 0x100; bits != 1; bits >>= 2)
				m |= 1 << (bits & 3);
			return m;
		}

		case Operand::SELECTION_SELECT_1:
			return 1 << swizzle.x;
	}
}

ASMOperation::ASMOperation(const Opcode *op) : Opcode(*op), dim(0), stride(0) {
	auto			code	= Op();
	const uint32	*p		= op->operands();

	for (bool extended = Extended == 1; extended;) {
		ExtendedOpcode		token	= (ExtendedOpcode&)*p++;
		switch(token.Type) {
			case EXTENDED_OPCODE_SAMPLE_CONTROLS:		tex_offset	= token; break;
			case EXTENDED_OPCODE_RESOURCE_DIM:			dim			= token.ResourceDim; stride = token.BufferStride; break;
			case EXTENDED_OPCODE_RESOURCE_RETURN_TYPE:	ret			= token; break;
		}
		extended = token.Extended == 1;
	}

	uint32 func = 0;
	if (code == OPCODE_INTERFACE_CALL)
		func = *p++;

	for (size_t i = 0, n = NumOperands(); i < n; i++)
		ops.emplace_back(p);

	if (code == OPCODE_INTERFACE_CALL)
		ops[0].funcNum = func;
}

void DisassembleDecl(string_accum &a, const uint32 *p) {
	const uint32	*begin			= p;
	Opcode			token0			= (Opcode&)*p++;
	OpcodeType		op				= token0.Op();
	ASMOperand		operand;

	ISO_ASSERT(op < NUM_OPCODES);

	a << to_string(op);

	switch (op) {
		case OPCODE_DCL_GLOBAL_FLAGS: {
			const char *sep = " ";
			if (token0.global_flags.RefactoringAllowed) {
				a << sep << "refactoringAllowed";
				sep = ", ";
			}
			if (token0.global_flags.DoubleFloatOps) {
				a << sep << "doublePrecisionFloats";
				sep = ", ";
			}
			if (token0.global_flags.ForceEarlyDepthStencil) {
				a << sep << "forceEarlyDepthStencil";
				sep = ", ";
			}
			if (token0.global_flags.EnableRawStructuredBufs) {
				a << sep << "enableRawAndStructuredBuffers";
				sep = ", ";
			}
			break;
		}

		case OPCODE_DCL_CONSTANT_BUFFER: {
			CBufferAccessPattern accessPattern = (CBufferAccessPattern)token0.AccessPattern;
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << no_swizzle(operand) << ", ";
			switch (accessPattern) {
				case ACCESS_IMMEDIATE_INDEXED:	a << "immediateIndexed"; break;
				case ACCESS_DYNAMIC_INDEXED:	a << "dynamicIndexed"; break;
				default: ISO_ERROR("Unexpected cbuffer access pattern");
			}
			break;
		}

		case OPCODE_DCL_INPUT:
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << operand;
			break;

		case OPCODE_DCL_TEMPS:
			a << ' ' << *p++;
			break;

		case OPCODE_DCL_INDEXABLE_TEMP: {
			uint32	tempReg		= *p++;
			uint32	numTemps	= *p++;
			uint32	tempComponentCount = *p++;
			a.format(" x%u[%u], %u", tempReg, numTemps, tempComponentCount);
			break;
		}

		case OPCODE_DCL_OUTPUT:
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << operand;
			break;

		case OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
			a << ' ' <<  *p++;
			break;

		case OPCODE_DCL_INPUT_SIV:
		case OPCODE_DCL_INPUT_SGV:
		case OPCODE_DCL_INPUT_PS_SIV:
		case OPCODE_DCL_INPUT_PS_SGV:
		case OPCODE_DCL_OUTPUT_SIV:
		case OPCODE_DCL_OUTPUT_SGV:
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << operand << ", " << to_string((SystemValue)*p++);
			break;

		case OPCODE_DCL_STREAM:
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << no_swizzle(operand);
			break;

		case OPCODE_DCL_SAMPLER: {
			SamplerMode	samplerMode = (SamplerMode)token0.SamplerMode;
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << no_swizzle(operand) << ", ";
			switch (samplerMode) {
				case SAMPLER_MODE_DEFAULT:		a << "mode_default"; break;
				case SAMPLER_MODE_COMPARISON:	a << "mode_comparison"; break;
				case SAMPLER_MODE_MONO:			a << "mode_mono"; break;
			}
			break;
		}

		case OPCODE_DCL_RESOURCE: {
			ResourceDimension	dim = (ResourceDimension)token0.ResourceDim;
			uint32	sampleCount = dim == RESOURCE_DIMENSION_TEXTURE2DMS || dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY ? token0.SampleCount : 0;
			ISO_VERIFY(operand.Extract(p));
			a << '_' << dim << ' ' << ResourceReturnType(*p++) << ' ' << no_swizzle(operand);
			break;
		}
		case OPCODE_DCL_INPUT_PS:
			ISO_VERIFY(operand.Extract(p));
			a << ' ';
			switch (token0.InterpolationMode) {
				case INTERPOLATION_CONSTANT:						a << "constant"; break;
				case INTERPOLATION_LINEAR:							a << "linear"; break;
				case INTERPOLATION_LINEAR_CENTROID:					a << "linear_centroid"; break;
				case INTERPOLATION_LINEAR_NOPERSPECTIVE:			a << "linear_noperspective"; break;
				case INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID:	a << "linear_noperspective_centroid"; break;
				case INTERPOLATION_LINEAR_SAMPLE:					a << "linear_sample"; break;
				case INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE:		a << "linear_noperspective_sample"; break;
				default:				ISO_ERROR("Unexpected Interpolation Mode");
			}
			a << ' ' << operand;
			break;

		case OPCODE_DCL_INDEX_RANGE:
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << operand << ' ' << *p++;
			break;

		case OPCODE_DCL_THREAD_GROUP:
			a << ' ' << p[0] << ", " << p[1] << ", " << p[2];
			p += 3;
			break;

		case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << no_swizzle(operand) << ", " << *p++;
			break;

		case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
			ISO_VERIFY(operand.Extract(p));
			uint32	stride	= *p++;
			uint32	count	= *p++;
			a << ' ' << no_swizzle(operand) << ", " << stride << ", " << count;
			break;
		}

		case OPCODE_DCL_INPUT_CONTROL_POINT_COUNT:
		case OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT:
			a << ' ' << token0.ControlPointCount;
			break;

		case OPCODE_DCL_TESS_DOMAIN:
			a << ' ';
			switch (token0.TessDomain) {
				case DOMAIN_ISOLINE:	a << "domain_isoline"; break;
				case DOMAIN_TRI:		a << "domain_tri"; break;
				case DOMAIN_QUAD:		a << "domain_quad"; break;
				default:				ISO_ERROR("Unexpected Tessellation domain");
			}
			break;

		case OPCODE_DCL_TESS_PARTITIONING:
			a << ' ';
			switch (token0.TessPartitioning) {
				case PARTITIONING_INTEGER:			a << "partitioning_integer"; break;
				case PARTITIONING_POW2:				a << "partitioning_pow2"; break;
				case PARTITIONING_FRACTIONAL_ODD:	a << "partitioning_fractional_odd"; break;
				case PARTITIONING_FRACTIONAL_EVEN:	a << "partitioning_fractional_even"; break;
				default:				ISO_ERROR("Unexpected Partitioning");
			}
			break;

		case OPCODE_DCL_GS_INPUT_PRIMITIVE:
			a << ' ';
			switch (token0.InputPrimitive) {
				case PRIMITIVE_POINT:				a << "point"; break;
				case PRIMITIVE_LINE:				a << "line"; break;
				case PRIMITIVE_TRIANGLE:			a << "triangle"; break;
				case PRIMITIVE_LINE_ADJ:			a << "line_adj"; break;
				case PRIMITIVE_TRIANGLE_ADJ:		a << "triangle_adj"; break;
				default:
					if (between(token0.InputPrimitive, PRIMITIVE_1_CONTROL_POINT_PATCH, PRIMITIVE_32_CONTROL_POINT_PATCH))
						a << "control_point_patch_" << 1 + int(token0.InputPrimitive - PRIMITIVE_1_CONTROL_POINT_PATCH);
					else
						ISO_ERROR("Unexpected primitive type");
			}
			break;

		case OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY: {
			PrimitiveTopology	outTopology = (PrimitiveTopology)token0.OutputPrimitiveTopology;
			a << ' ' << PrimitiveTopology_names[outTopology];
			break;
		}

		case OPCODE_DCL_TESS_OUTPUT_PRIMITIVE: {
			TessellatorOutputPrimitive	outPrim = (TessellatorOutputPrimitive)token0.OutputPrimitive;
			a << ' ';
			switch (outPrim) {
				case OUTPUT_PRIMITIVE_POINT:		a << "output_point"; break;
				case OUTPUT_PRIMITIVE_LINE:			a << "output_line"; break;
				case OUTPUT_PRIMITIVE_TRIANGLE_CW:	a << "output_triangle_cw"; break;
				case OUTPUT_PRIMITIVE_TRIANGLE_CCW:	a << "output_triangle_ccw"; break;
				default:				ISO_ERROR("Unexpected output primitive");
			}
			break;
		}

		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
		case OPCODE_DCL_RESOURCE_RAW:
			a << ' ';
			ISO_VERIFY(operand.Extract(p));
			a << operand;
			break;

		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
		case OPCODE_DCL_RESOURCE_STRUCTURED:
			ISO_VERIFY(operand.Extract(p));
			a << ' ' << no_swizzle(operand) << ", " << *p++;
			if (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED && token0.HasOrderPreservingCounter)
				a << ", hasOrderPreservingCounter";
			break;

		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED: {
			a << "_" << (ResourceDimension)token0.ResourceDim;
			if (token0.GloballyCoherant)
				a << "_glc";

			ISO_VERIFY(operand.Extract(p));
			a << ' ' << ResourceReturnType(*p++) << ' ' << no_swizzle(operand);
			break;
		}

		case OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
		case OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:
		case OPCODE_DCL_GS_INSTANCE_COUNT:
			a << ' ' << *p++;
			break;

		case OPCODE_DCL_HS_MAX_TESSFACTOR:
			a.format(" l(%f)", *(float*)p++);
			break;

		case OPCODE_DCL_FUNCTION_BODY:
			a << " fb" << *p++;
			break;

		case OPCODE_DCL_FUNCTION_TABLE: {
			uint32 table	= *p++;
			uint32 length	= *p++;
			a << ' ' << table << " = \\{";

			for (uint32 i = 0; i < length; i++) {
				a << "fb" << *p++;
				if (i + 1 < length)
					a << ", ";
			}
			a << "\\}";
			break;
		}

		case OPCODE_DCL_INTERFACE: {
			uint32	interfaceID = *p++;
			uint32	numTypes	= *p++;
			DeclarationCount CountToken = (DeclarationCount&)*p++;

			a << " fp" << interfaceID << '[' << CountToken.NumInterfaces << "][" << numTypes << "] = \\{";
			for (uint32 i = 0; i < CountToken.TableLength; i++) {
				a << "ft" << *p++;
				if (i + 1 < CountToken.TableLength)
					a << ", ";
			}
			a << "\\}";
			break;
		}

		case OPCODE_HS_DECLS:
			break;

		default:
			ISO_ERROR("Unexpected opcode token0 %d", op);
	}
}

void DisassembleOperation(string_accum &a, const uint32 *p) {
	const uint32 *begin	= p;
	Opcode token0	= (Opcode&)*p++;

	OpcodeType op = (OpcodeType)token0.Type;
	ISO_ASSERT(op < NUM_OPCODES);

	if (op == OPCODE_CUSTOMDATA) {
		CustomDataClass customClass = (CustomDataClass)token0.custom.Class;

		// DWORD length including token0 and this length token
		uint32 customDataLength = *p++;

		ISO_ASSERT(customDataLength >= 2);

		switch (customClass) {
			case CUSTOMDATA_SHADER_MESSAGE: {
				const uint32 *end = p + customDataLength - 2;

				uint32 infoQueueMsgId	= p[0];
				uint32 messageFormat	= p[1]; // enum. 0 == text only, 1 == printf
				uint32 formatStringLen	= p[2]; // length NOT including null terminator
				uint32 numOperands		= p[3];
				uint32 operandDwordLen	= p[4];
				p += 5;

				a << "errorf \"" << (char*)p << "\"";

				for (uint32 i = 0; i < numOperands; i++) {
					ASMOperand	operand;
					ISO_VERIFY(operand.Extract(p));
					a << ", " <<operand;
				}

				p = end;
				break;
			}

			case CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER: {
				a << "dcl_immediateConstantBuffer \\{";
				uint32 dataLength = customDataLength - 2;
				ISO_ASSERT(dataLength % 4 == 0);

				for (uint32 i = 0; i < dataLength; i++) {
					if (i % 4 == 0)
						a << "\n\t\t\t{ ";
					put_comps(a, p++, 1);
					if ((i + 1) % 4 == 0)
						a << "\\}";
					if (i + 1 < dataLength)
						a << ", ";
				}

				a << " }";
				break;
			}

			case CUSTOMDATA_OPAQUE: {
				a << "dcl_opaque \\{";
				uint32 dataLength = customDataLength - 2;
				uint32	max_len = min(dataLength, 8);
				for (uint32 i = 0; i < max_len; i++)
					a.format(" 0x%08x", p[i]);
				if (dataLength > max_len)
					a << "... length = " << dataLength;
				a << "\\}";
				break;
			}

			default: {
				uint32 dataLength = customDataLength - 2;
				a << "unknown custom " <<  customClass << ", length = " << dataLength;
				uint32	max_len = min(dataLength, 8);
				for (uint32 i = 0; i < max_len; i++)
					a << ' ' << hex(p[i]);
				if (dataLength > max_len)
					a << "...";
				break;
			}
		}
		return;
	}

	a << to_string(op);

	for (bool extended = token0.Extended == 1; extended;) {
		ExtendedOpcode		token	= (ExtendedOpcode&)*p++;
		switch (token.Type) {
			case EXTENDED_OPCODE_SAMPLE_CONTROLS:
				a.format("(%d, %d, %d)", token.TexelOffsetU, token.TexelOffsetV, token.TexelOffsetW);
				break;

			case EXTENDED_OPCODE_RESOURCE_DIM: {
				ResourceDimension	resDim = (ResourceDimension)token.ResourceDim;
				if (op == OPCODE_LD_STRUCTURED)
					(a << "_indexable(" << resDim).format(", stride=%u)", token.BufferStride);
				else
					a << '(' << resDim << ')';
				break;
			}
			case EXTENDED_OPCODE_RESOURCE_RETURN_TYPE: {
				a << (ResourceReturnType)token;
				break;
			}
		}

		extended = token.Extended == 1;
	}

	if (op == OPCODE_RESINFO)
		a << "_" << (ResinfoRetType)token0.ResinfoReturn;

	if (op == OPCODE_SYNC) {
		if (token0.sync.UAV_Global)
			a << "_uglobal";
		if (token0.sync.UAV_Group)
			a << "_ugroup";
		if (token0.sync.TGSM)
			a << "_g";
		if (token0.sync.Threads)
			a << "_t";
	}

	uint32 func = 0;
	if (op == OPCODE_INTERFACE_CALL)
		func = *p++;

	dynamic_array<ASMOperand>	operands(token0.NumOperands());

	for (size_t i = 0; i < operands.size(); i++)
		ISO_VERIFY(operands[i].Extract(p));

	if (op == OPCODE_INTERFACE_CALL)
		operands[0].funcNum = func;

	if (op == OPCODE_IF
	||	op == OPCODE_BREAKC
	||	op == OPCODE_CALLC
	||	op == OPCODE_RETC
	||	op == OPCODE_SWAPC
	||	op == OPCODE_DMOVC
	||	op == OPCODE_DISCARD
	||	op == OPCODE_DMOVC
	)
		a << (token0.TestNonZero ? "_nz" : "_z");

	if (op != OPCODE_SYNC)
		a << (token0.Saturate ? "_sat" : "");

	for (size_t i = 0; i < operands.size(); i++) {
		if (i == 0)
			a << ' ';
		else
			a << ", ";
		a << operands[i];
	}
}

class DisassemblerDXBC : public Disassembler {
public:
	virtual	const char*	GetDescription() { return "DXBC"; }

	virtual State*		Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) {
		StateDefault2	*state	= new StateDefault2;
		uint32			indent	= 0;

		for (const uint32 *p = block, *end = block.end(); p < end;) {
			uint64	addr2 = addr + ((const char*)p - block);

			uint64			sym_addr;
			string_param	sym_name;
			if (sym_finder && sym_finder(addr2, sym_addr, sym_name) && sym_addr == addr2)
				state->lines.push_back(make_pair(sym_name, addr2));

			buffer_accum<1024>	ba("%012I64x	%08x ", addr2, p[0]);
			int		n = ((Opcode*)p)->length();

			uintptr_t offset = p - block;

			int	indent0 = indent;
			switch (((Opcode*)p)->Type) {
				case OPCODE_CASE:		--indent0; break;
				case OPCODE_DEFAULT:	--indent0; break;
				case OPCODE_ELSE:		--indent0; break;
				case OPCODE_ENDIF:		indent0 = --indent; break;
				case OPCODE_ENDLOOP:	indent0 = --indent; break;
				case OPCODE_ENDSWITCH:	indent0 = indent -= 2; break;
				case OPCODE_IF:			++indent; break;
				case OPCODE_LOOP:		++indent; break;
				case OPCODE_SWITCH:		indent += 2; break;

				case OPCODE_HS_CONTROL_POINT_PHASE:
				case OPCODE_HS_FORK_PHASE:
				case OPCODE_HS_JOIN_PHASE:
					indent0 = 0;
					indent = 1;
					break;
			}
			ba << repeat(' ', indent0 * 2);
			Opcode	*token	= (Opcode*)p;

			if (token->Type == OPCODE_CUSTOMDATA && token->custom.Class == CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER) {
				++p;
				uint32 dataLength = *p++ - 2;

				ba << "dcl_immediateConstantBuffer \\{";
				state->lines.emplace_back(ba, addr2);

				for (uint32 i = 0; i < dataLength / 4; i++) {
					uint64	addr2 = addr + ((char*)p - block);
					put_comps(ba.reset().format("%012I64x	%08x ", addr2, p[0]) << repeat(' ', (indent0 + 1) * 2) << "\\{", p, 4) << "\\}";
					state->lines.emplace_back(ba, addr2);
					p += 4;
				}

				uint64	addr2 = addr + ((char*)p - block);
				ba.reset().format("%012I64x	%08x ", addr2, p[0]) << repeat(' ', indent0 * 2) << "\\}";
				state->lines.emplace_back(ba, addr2);
				continue;

			} else if (token->IsDeclaration()) {
				DisassembleDecl(ba, p);

			} else {
				DisassembleOperation(ba, p);

			}

			state->lines.emplace_back(ba, addr2);
			p		+= n;
		}

		return state;
	}
} dxbc_dis;

void dxbc_dis_dummy() {}
