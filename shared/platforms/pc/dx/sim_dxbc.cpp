#include "sim_dxbc.h"
#include "dx_shaders.h"
#include "base/maths.h"
#include "base/soft_float.h"
#include "dxgi_read.h"

using namespace iso;
using namespace dx;

//-----------------------------------------------------------------------------
//	DeclReader
//-----------------------------------------------------------------------------

Decls::Decls(const memory_block &ucode) {
	clear(*this);
	for (const Opcode *op = ucode; op < ucode.end(); op = op->next())
		Process(op);
}

bool Decls::Process(const Opcode *op) {
	auto			code	= op->Op();
	const uint32	*p		= op->operands();

	switch (code) {
		default: return false;

		case OPCODE_DCL_GLOBAL_FLAGS:					global_flags 			= *op;	break;
		case OPCODE_DCL_TEMPS:							num_temp 				= *p;	break;
		case OPCODE_DCL_INDEXABLE_TEMP: {
			uint32	i		= p[0];
			uint32	num		= p[1];
			uint32	comps	= p[2];
			num_index += num;
			break;
		}
		case OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:		max_output 				= *p;	break;
		case OPCODE_DCL_THREAD_GROUP:					thread_group			= *(uint3p*)p;	break;
		case OPCODE_DCL_INPUT_CONTROL_POINT_COUNT:		input_control_points	= op->ControlPointCount;							break;
		case OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT:		output_control_points	= op->ControlPointCount;							break;
		case OPCODE_DCL_TESS_DOMAIN:					tess_domain				= (TessellatorDomain)op->TessDomain;				break;
		case OPCODE_DCL_TESS_PARTITIONING:				tess_partitioning		= (TessellatorPartitioning)op->TessPartitioning;	break;
		case OPCODE_DCL_GS_INPUT_PRIMITIVE:				gs_input				= (PrimitiveType)op->InputPrimitive;				break;
		case OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:	gs_output				= (PrimitiveTopology)op->OutputPrimitiveTopology;	break;
		case OPCODE_DCL_TESS_OUTPUT_PRIMITIVE:			tess_output				= (TessellatorOutputPrimitive)op->OutputPrimitive;	break;
		case OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:	forks					+= *p;	break;
		case OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:	joins					+= *p;	break;
		case OPCODE_DCL_GS_INSTANCE_COUNT:				instances				+= *p;	break;
		case OPCODE_DCL_HS_MAX_TESSFACTOR:				max_tesselation			= *p;	break;
	}
	return true;
}

SimulatorDXBC::Register	dummy;
float		tex_default[4] = {0, 0, 0, 1};

void SimulatorDXBC::Init(const memory_block &_ucode, uint64 _ucode_addr) {
	ucode		= _ucode;
	ucode_addr	= _ucode_addr;
	phase		= phNormal;
	fork_count	= 0;
	num_temp	= 0;
	input_mask	= patch_mask = output_mask = const_output_mask = 0;
	max_output	= 1;
	streamptr	= 0;


	dynamic_array<const Opcode*>	loop_stack;
	size_t	gmem	= 0;

	for (const Opcode *op = ucode; op < ucode.end(); op = op->next()) {
		if (Decls::Process(op))
			continue;

		const uint32	*p = op->operands();
		switch (op->Op()) {
			case OPCODE_DCL_INPUT:
			case OPCODE_DCL_INPUT_SIV:
			case OPCODE_DCL_INPUT_SGV:
			case OPCODE_DCL_INPUT_PS:
			case OPCODE_DCL_INPUT_PS_SIV:
			case OPCODE_DCL_INPUT_PS_SGV: {
				ASMOperand	r(p);
				int i;
				switch (r.type) {
					case Operand::TYPE_INPUT:								i = r.indices[0].index; break;
					case Operand::TYPE_INPUT_DOMAIN_POINT:					i = vDomain; break;
					case Operand::TYPE_INPUT_PRIMITIVEID:					i = vPrim; break;
					case Operand::TYPE_INPUT_COVERAGE_MASK:					i = vMask; break;
					case Operand::TYPE_INPUT_GS_INSTANCE_ID:				i = vInstanceID; break;
					case Operand::TYPE_INPUT_THREAD_ID:						i = vThreadID; break;
					case Operand::TYPE_INPUT_THREAD_GROUP_ID:				i = vThreadGroupID; break;
					case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			i = vThreadIDInGroup; break;
					case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	i = vThreadIDInGroup; break;// + 3;
					case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				i = vForkInstanceID; break;
					case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				i = vJoinInstanceID; break;
					case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				i = vOutputControlPointID; break;
					case Operand::TYPE_INPUT_CONTROL_POINT:
					case Operand::TYPE_OUTPUT_CONTROL_POINT:
						patch_mask |= bits64(r.indices[0].index / input_control_points, r.indices[1].index / input_control_points);
						continue;
					default:												continue;
				}
				if (r.indices.size() == 2)
					input_mask |= bits64(i, r.indices[1].index);
				else
					input_mask |= bit64(i);
				break;
			}

			case OPCODE_DCL_OUTPUT:
			case OPCODE_DCL_OUTPUT_SIV:
			case OPCODE_DCL_OUTPUT_SGV: {
				ASMOperand	r(p);
				int i;
				switch (r.type) {
					case Operand::TYPE_OUTPUT:								i = r.indices[0].index; break;
					case Operand::TYPE_OUTPUT_DEPTH:						i = oDepth; break;
					case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				i = oDepthLessEqual; break;
					case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			i = oDepthGreaterEqual; break;
					case Operand::TYPE_OUTPUT_COVERAGE_MASK:				i = oMask; break;
					default:												continue;
				}
				if (phase == phConstantsFork || phase == phConstantsJoin)
					const_output_mask |= bit64(i);
				else
					output_mask |= bit64(i);
				break;
			}

			case OPCODE_DCL_STREAM:
				gmem += 4096 * sizeof(Register);
				break;

			case OPCODE_LOOP:
				loop_stack.push_back(op->next());
				break;

			case OPCODE_ENDLOOP:
				loop_hash[op] = loop_stack.pop_back_value();
				break;

			case OPCODE_CONTINUE:
			case OPCODE_CONTINUEC:
				loop_hash[op] = loop_stack.back();
				break;

			case OPCODE_HS_CONTROL_POINT_PHASE:
				phase = phControlPoints;
				break;

			case OPCODE_HS_FORK_PHASE:
				phase = phConstantsFork;
				break;

			case OPCODE_HS_JOIN_PHASE:
				phase = phConstantsJoin;
				break;

			case OPCODE_CUSTOMDATA:
				switch (op->custom.Class) {
					case CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER:
						constants = const_memory_block(p + 1, p[0] * 4);
						break;
				}
				break;

			case OPCODE_LABEL:
				labels[p[1]] = op;
				break;

			case OPCODE_DCL_FUNCTION_TABLE:
				function_tables[p[0]] = p + 2;
				break;

			case OPCODE_DCL_INTERFACE:
				interface_tables[p[0]] = p + 3;
				break;

			case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW: {
				ASMOperand r(p);
				uint32		index	= r.indices[0].index;
				uint32		i		= *p++;
				Resource	&res	= grp[index];
				res.init(RESOURCE_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN, i, 0);
				res.n	= i;
				gmem	+= i;
				break;
			}
			case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
				ASMOperand r(p);
				uint32		index	= r.indices[0].index;
				uint32		stride	= *p++;
				uint32		count	= *p++;
				Resource	&res	= grp[index];
				res.init(RESOURCE_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN, stride, count);
				res.n	= stride * count;
				gmem	+= res.n;
				break;
			}
		}
	}

	group_mem.resize(gmem);
	uint8	*g = group_mem;
	for (auto &i : grp) {
		i.p = g;
		g += i.n;
	}

	phase			= phNormal;
	input_offset	= num_temp + num_index;
	output_offset	= input_offset + highest_set_index(input_mask) + 1;
	regs_per_thread	= output_offset + highest_set_index(output_mask) + 1;
	SetNumThreads(threads.size32());
}

void SimulatorDXBC::Reset() {
	active_stack.clear();
	active_threads.clear();
	
	auto r = regs.begin();
	group_mem.fill(0xcd);

	for (auto &t : threads) {
		t.reset(r);
		memset(r, 0xcd, input_offset * sizeof(Register));
		active_threads.push_back(&t);
		r += regs_per_thread;
	}
	phase		= phNormal;
	fork_count	= 0;
	streamptr	= 0;
}

void SimulatorDXBC::SetNumThreads(int num_threads) {
	active_threads.clear();
	threads.resize(num_threads);
	
	const_offset	= regs_per_thread * num_threads;
	patch_offset	= const_offset + highest_set_index(const_output_mask) + 1;

	regs.resize(patch_offset + (highest_set_index(patch_mask) + 1) * input_control_points);

	regs.raw_data().fill(0xcd);
	group_mem.fill(0xcd);

	auto *r = regs.begin();
	for (auto &t : threads) {
		t.reset(r);
		r += regs_per_thread;
		active_threads.push_back(&t);
	}
}

void SimulatorDXBC::InterpolateInputQuad(int t, int r, const Triangle &tri, float u0, float v0, float u1, float v1) {
	threads[t + 0].regs[input_offset + r] = tri.interpolate(u0, v0);
	threads[t + 1].regs[input_offset + r] = tri.interpolate(u1, v0);
	threads[t + 2].regs[input_offset + r] = tri.interpolate(u0, v1);
	threads[t + 3].regs[input_offset + r] = tri.interpolate(u1, v1);
}

void SimulatorDXBC::InterpolateInputQuad(int t, int r, const Triangle &tri, float2 uv0, float2 uv1, float2 uv2) {
	threads[t + 0].regs[input_offset + r] = tri.interpolate(uv0);
	threads[t + 1].regs[input_offset + r] = tri.interpolate(uv1);
	threads[t + 2].regs[input_offset + r] = tri.interpolate(uv2);
	threads[t + 3].regs[input_offset + r] = tri.interpolate(uv1 + uv2 - uv0);
}

uint32 SimulatorDXBC::offset(ThreadState &ts, const ASMIndex &ix) {
	uint32	off = 0;
	if (ix.relative) {
		auto	reg = ref(ts, ix.operand);
//		off	= (uint32)reg[ix.operand.swizzle.x];
		off	= ((int32*)&reg)[ix.operand.swizzle.x];
		if (off > 0x1000000)
			off = 0;
	}
	if (ix.absolute)
		off += ix.index;

	return off;
}

SimulatorDXBC::Register &SimulatorDXBC::ref(ThreadState &ts, const ASMOperand &op) {
	auto	indices	= op.indices.begin();

	Register	*r;
	switch (op.type) {
		case Operand::TYPE_TEMP:								r = ts.regs; break;
		case Operand::TYPE_OUTPUT:								r = phase == phConstantsFork || phase == phConstantsJoin ? regs + const_offset : ts.regs + output_offset; break;
		case Operand::TYPE_THREAD_GROUP_SHARED_MEMORY:			r = group_mem; break;
		case Operand::TYPE_INDEXABLE_TEMP:						r = ts.regs + num_temp; break;
		case Operand::TYPE_INPUT:								r = ts.regs + input_offset; break;
		case Operand::TYPE_IMMEDIATE32:							return (Register&)op.values;
		case Operand::TYPE_IMMEDIATE64:							return (Register&)op.values;
		case Operand::TYPE_CONSTANT_BUFFER:						r = (Register*)get(cbv[offset(ts, *indices++)].or_default()); break;
		case Operand::TYPE_IMMEDIATE_CONSTANT_BUFFER:			r = unconst((const Register*)constants); break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				return ts.regs[input_offset + vOutputControlPointID];
		case Operand::TYPE_INPUT_DOMAIN_POINT:					return ts.regs[input_offset + vDomain];
		case Operand::TYPE_INPUT_PRIMITIVEID:					return ts.regs[input_offset + vPrim];
		case Operand::TYPE_INPUT_COVERAGE_MASK:					return ts.regs[input_offset + vMask];
		case Operand::TYPE_INPUT_GS_INSTANCE_ID:				return ts.regs[input_offset + vInstanceID];
		case Operand::TYPE_INPUT_THREAD_ID:						return ts.regs[input_offset + vThreadID];
		case Operand::TYPE_INPUT_THREAD_GROUP_ID:				return ts.regs[input_offset + vThreadGroupID];
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			return ts.regs[input_offset + vThreadIDInGroup];
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	return *(Register*)((uint32*)(ts.regs + input_offset + vThreadIDInGroup) + 3);// + 3;
		case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				return ts.regs[input_offset + vForkInstanceID];
		case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				return ts.regs[input_offset + vJoinInstanceID];
		case Operand::TYPE_INPUT_CONTROL_POINT:					
		case Operand::TYPE_OUTPUT_CONTROL_POINT:				r = regs + patch_offset; break;
		case Operand::TYPE_INPUT_PATCH_CONSTANT:				r = regs + const_offset; break;
		case Operand::TYPE_OUTPUT_DEPTH:						return ts.regs[output_offset + oDepth];
		case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				return ts.regs[output_offset + oDepthLessEqual];
		case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			return ts.regs[output_offset + oDepthGreaterEqual];
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				return ts.regs[output_offset + oMask];
		case Operand::TYPE_NULL:
		default:												return dummy;
	}

	while (indices != op.indices.end())
		r += offset(ts, *indices++);
	return *r;
}

const void *SimulatorDXBC::reg_data(int thread, ASMOperand::Type type, const range<const uint32*> offsets) const {
	ThreadState &ts	= threads[thread];
	auto		i	= offsets.begin();
	const Register	*r	= ts.regs;
	switch (type) {
		case Operand::TYPE_TEMP:								break;
		case Operand::TYPE_OUTPUT:								r += output_offset; break;
		case Operand::TYPE_THREAD_GROUP_SHARED_MEMORY:			r = group_mem; break;
		case Operand::TYPE_INDEXABLE_TEMP:						r += num_temp; break;
		case Operand::TYPE_INPUT:
			if (offsets.size() > 1)
				return (const uint8*)threads[offsets[1]].regs + input_offset + offsets[0];
			r = ts.regs + input_offset;
			break;
		case Operand::TYPE_CONSTANT_BUFFER:						r = (Register*)get(cbv[*i++].or_default()); break;
		case Operand::TYPE_IMMEDIATE_CONSTANT_BUFFER:			r = unconst((const Register*)constants); break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				r += input_offset + vOutputControlPointID; break;
		case Operand::TYPE_INPUT_DOMAIN_POINT:					r += input_offset + vDomain; break;
		case Operand::TYPE_INPUT_PRIMITIVEID:					r += input_offset + vPrim; break;
		case Operand::TYPE_INPUT_COVERAGE_MASK:					r += input_offset + vMask; break;
		case Operand::TYPE_INPUT_GS_INSTANCE_ID:				r += input_offset + vInstanceID; break;
		case Operand::TYPE_INPUT_THREAD_ID:						r += input_offset + vThreadID + 5; break;
		case Operand::TYPE_INPUT_THREAD_GROUP_ID:				r += input_offset + vThreadGroupID + 7; break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			r += input_offset + vThreadIDInGroup + 6; break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	r = (Register*)((uint32*)(r + input_offset + vThreadIDInGroup + 8) + 3); break;
		case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				r += input_offset + vForkInstanceID; break;
		case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				r += input_offset + vJoinInstanceID; break;
		case Operand::TYPE_INPUT_CONTROL_POINT:
		case Operand::TYPE_OUTPUT_CONTROL_POINT:				r = regs + patch_offset; break;
		case Operand::TYPE_INPUT_PATCH_CONSTANT:				r = regs + const_offset; break;
		case Operand::TYPE_OUTPUT_DEPTH:						r += output_offset + oDepth; break;
		case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				r += output_offset + oDepthLessEqual; break;
		case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			r += output_offset + oDepthGreaterEqual; break;
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				r += output_offset + oMask; break;
		case Operand::TYPE_NULL:
		default:												return 0;
	}

	const void *v = r;
	while (i != offsets.end()) {
		int32	off = (int32)*i++;
//		if (off >= 0xffffffb0)
//			off -= 0xffffffb0;
		v = (uint8*)v + off;
	}
	return v;
}

int SimulatorDXBC::reg(const ASMOperand::Type type, int index) const {
	switch (type) {
		case Operand::TYPE_TEMP:								return index;
		case Operand::TYPE_OUTPUT:								return output_offset + index;
		case Operand::TYPE_INDEXABLE_TEMP:						return num_temp + index;
		case Operand::TYPE_INPUT:								return input_offset + index;
		case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				return input_offset + vOutputControlPointID;
		case Operand::TYPE_INPUT_DOMAIN_POINT:					return input_offset + vDomain;
		case Operand::TYPE_INPUT_PRIMITIVEID:					return input_offset + vPrim;
		case Operand::TYPE_INPUT_COVERAGE_MASK:					return input_offset + vMask;
		case Operand::TYPE_INPUT_GS_INSTANCE_ID:				return input_offset + vInstanceID;
		case Operand::TYPE_INPUT_THREAD_ID:						return input_offset + vThreadID;
		case Operand::TYPE_INPUT_THREAD_GROUP_ID:				return input_offset + vThreadGroupID;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			return input_offset + vThreadIDInGroup;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	return input_offset + vThreadIDInGroup;// + 3;
		case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				return input_offset + vForkInstanceID;
		case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				return input_offset + vJoinInstanceID;
		case Operand::TYPE_OUTPUT_DEPTH:						return output_offset + oDepth;
		case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				return output_offset + oDepthLessEqual;
		case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			return output_offset + oDepthGreaterEqual;
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				return output_offset + oMask;
		case Operand::TYPE_NULL:
		default:												return -1;
	}
}

range<stride_iterator<const SimulatorDXBC::Register>> SimulatorDXBC::GetRegFile(Operand::Type type, int i) const {
	auto	r		= regs.begin();
	size_t	stride	= regs_per_thread * sizeof(Register);
	size_t	num		= threads.size();

	switch (type) {
		case Operand::TYPE_TEMP:								r += i; break;
		case Operand::TYPE_OUTPUT:								r += output_offset + i; break;
		case Operand::TYPE_INDEXABLE_TEMP:						r += num_temp + i; break;
		case Operand::TYPE_INPUT:								r += input_offset + i; break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				r += input_offset + vOutputControlPointID; break;
		case Operand::TYPE_INPUT_DOMAIN_POINT:					r += input_offset + vDomain; break;
		case Operand::TYPE_INPUT_PRIMITIVEID:					r += input_offset + vPrim; break;
		case Operand::TYPE_INPUT_COVERAGE_MASK:					r += input_offset + vMask; break;
		case Operand::TYPE_INPUT_GS_INSTANCE_ID:				r += input_offset + vInstanceID; break;
		case Operand::TYPE_INPUT_THREAD_ID:						r += input_offset + vThreadID; break;
		case Operand::TYPE_INPUT_THREAD_GROUP_ID:				r += input_offset + vThreadGroupID; break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			r += input_offset + vThreadIDInGroup; break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	r += input_offset + vThreadIDInGroup; break;// + 3;
		case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				r += input_offset + vForkInstanceID; break;
		case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				r += input_offset + vJoinInstanceID; break;
		case Operand::TYPE_INPUT_PATCH_CONSTANT:				r += const_offset + i; stride = sizeof(Register); num = highest_set_index(const_output_mask) + 1; break;
		case Operand::TYPE_INPUT_CONTROL_POINT:					r += patch_offset + i; stride = sizeof(Register) * (highest_set_index(patch_mask) + 1); num = input_control_points; break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT:				r += patch_offset + i; stride = sizeof(Register) * (highest_set_index(patch_mask) + 1); num = output_control_points; break;
		case Operand::TYPE_OUTPUT_DEPTH:						r += output_offset + oDepth; break;
		case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				r += output_offset + oDepthLessEqual; break;
		case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			r += output_offset + oDepthGreaterEqual; break;
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				r += output_offset + oMask; break;
		case Operand::TYPE_NULL:
		default:	return none;
	}

	return make_range_n(strided(r, stride), num);
}


template<typename U> constexpr U modify2(U t, OperandModifier mod) {
	constexpr U	topbit = bit<U>(sizeof(U) * 8 - 1);
	return mod == OPERAND_MODIFIER_NEG ? t ^ topbit : mod == OPERAND_MODIFIER_ABS ? t & ~topbit : mod == OPERAND_MODIFIER_ABSNEG ? t | topbit : t;
}

template<typename T> constexpr T modify(T t, OperandModifier mod) {
	typedef uint_for_t<T>	U;
	return force_cast<T>(modify2((U&)t, mod));
}

constexpr uint32 modify(uint32 t, OperandModifier mod) {
	return modify((int)t, mod);
}

template<typename T> constexpr T saturate(T t, bool sat) { return sat ? clamp(t, 0, 1) : t; }

template<typename T> T swizzle1(const T &t, uint8 swizzle) {
	T	t2;
	for (int i = 0; i < num_elements_v<T>; i++, swizzle >>= 2)
		t2[i] = t[swizzle & 3];
	return t2;
}

template<typename T> void mask_write(T &d, const T &s, uint8 mask) {
//	simd::masked(mask, d) = s;
	for (int i = 0; i < num_elements_v<T>; i++, mask >>= 1) {
		if (mask & 1)
			d[i] = s[i];
	}
}

uint8	src_swizzle(const Operand &op) {
	return op.num_components == Operand::NUMCOMPS_1 ? 0
		: op.selection_mode == Operand::SELECTION_SWIZZLE || op.selection_mode == Operand::SELECTION_SELECT_1 ? op.swizzle_bits
		: 0xe4;
}

int		src_index(const Operand &op) {
	return op.selection_mode == Operand::SELECTION_MASK
		? lowest_set_index(op.swizzle_bits)
		: op.swizzle.x;
}
/*
template<typename T, int N = num_elements_v<T>> struct RegValue;

template<typename T> struct RegValue<T, 1> {
	T	t;
	RegValue(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod) : t(modify(((T*)&r)[op.swizzle.x], mod)) {}
	operator T() const { return t; }
};
*/
template<typename T, int N = num_elements_v<T>> struct RegValue {
	typedef element_type<T>	E;
	T	t;
	RegValue(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod) {
		uint8	swizzle = src_swizzle(op);
		for (int i = 0; i < N; i++, swizzle >>= 2)
			t[i] = modify(((E*)&r)[swizzle & 3], mod);
	}
	operator T() const { return t; }
};

template<typename T> struct RegValue<T, 1> {
	T	t;
	RegValue(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod) : t(modify(((T*)&r)[op.swizzle.x], mod)) {}
	operator T() const { return t; }
};

template<typename E> struct Reg {
	array_vec<E, 4>	t;
	Reg(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod, bool sat) { 
		uint8	swizzle = src_swizzle(op);
		for (int i = 0; i < 4; i++, swizzle >>= 2)
			t[i] = modify(((E*)&r)[swizzle & 3], mod);
	}
	E	operator[](int i) { return t[i]; }
};
template<typename E> struct Reg<E&> {
	array_vec<E, 4>	t;
	E			*p;
	uint8		mask;
	bool		sat;
	Reg(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod, bool sat) : p((E*)&r), mask(op.swizzle_bits), sat(sat) {}
	Reg(Reg &&r) : p(r.p), mask(r.mask), sat(r.sat) { r.p = 0; }
	~Reg() {
		if (p) {
			for (int i = 0; i < 4; i++) {
				if ((mask >> i) & 1)
					p[i] = saturate(t[i], sat);
			}
		}
	}
	E&	operator[](int i) { return t[i]; }
};

template<typename T, int N = num_elements_v<T>> struct RegVec : RegValue<T> {
	RegVec(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod, bool sat) : RegValue<T>(r, op, mod) {}
};
template<typename T, int N> struct RegVec<const T&, N> : RegValue<T> {
	RegVec(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod, bool sat) : RegValue<T>(r, op, mod) {}
};
template<typename T> struct RegVec<T&, 1> {
	T	&t;
	RegVec(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod, bool sat) : t(((T*)&r)[src_index(op)]) {}
	operator T&() const { return t; }
};

template<typename T, int N> struct RegVec<T&, N> {
	typedef element_type<T>	E;
	T				t;
	E				*p;
	uint8			mask;
	bool			sat;
	RegVec(SimulatorDXBC::Register &r, const Operand &op, OperandModifier mod, bool sat) : p((E*)&r), mask(op.swizzle_bits), sat(sat) {}
	~RegVec() {
		if (p) {
			for (int i = 0; i < N; i++) {
				if ((mask >> i) & 1)
					p[i] = saturate(t[i], sat);
			}
		}
	}
	operator T&()  { return t; }
};

template<uint32 M, uint32 A> struct DD : ASMOperand {
	DD(const ASMOperand &op) : ASMOperand(op) {}
};

DD<~1,1> ddx(const ASMOperand &op) { return op; }
DD<~2,2> ddy(const ASMOperand &op) { return op; }

template<typename T> struct RegVecDD {
	T	t;
	RegVecDD(SimulatorDXBC::Register &r1, SimulatorDXBC::Register &r2, const Operand &op, OperandModifier mod, bool sat) {
		t = (T)RegValue<T>(r2, op, mod) - (T)RegValue<T>(r1, op, mod);
	}
	operator const T&() const { return t; }
};

template<typename T> Reg<T>		get_scalar(SimulatorDXBC &sim, SimulatorDXBC::ThreadState &ts, const ASMOperand &o, bool sat) { return Reg<T>(sim.ref(ts, o), o, o.modifier, sat); }
template<typename T> RegVec<T>	get_vector(SimulatorDXBC &sim, SimulatorDXBC::ThreadState &ts, const ASMOperand &o, bool sat) { return RegVec<T>(sim.ref(ts, o), o, o.modifier, sat); }

template<typename T, uint32 M, uint32 A> RegVecDD<T> get_vector(SimulatorDXBC &sim, SimulatorDXBC::ThreadState &ts, const DD<M, A> &o, bool sat) {
	return RegVecDD<T>(sim.ref(sim.other(ts, M, 0), o), sim.ref(sim.other(ts, M, A), o), o, o.modifier, sat);
}

template<typename P> struct thread_s;
template<typename T, typename... PP> struct thread_s<type_list<T, PP...> > {
	template<typename F, typename...OO> static void scalar2(F f, T &t, int n, OO&&... oo) {
		for (int i = 0; i < n; i++)
			f(t, oo[i]...);
	}
	template<typename F, typename...OO> static void scalar(SimulatorDXBC &sim, bool sat, F f, T &t, int n, const OO&... oo) {
		scalar2(f, t, n, get_scalar<PP>(sim, t, oo, sat)...);
	}

	template<typename F, typename...OO> static void vector(SimulatorDXBC &sim, bool sat, F f, T &t, const OO&... oo) {
		f(t, get_vector<PP>(sim, t, oo, sat)...);
	}
};

template<typename F, typename...OO> void SimulatorDXBC::operate(bool sat, F f, int nc, const OO&... oo) {
	for (auto &t : active_threads)
		thread_s<typename function<F>::P>::scalar(*this, sat, f, t, nc, oo...);
}

template<typename F, typename...OO> void SimulatorDXBC::operate_vec(bool sat, F f, const ASMOperand &r, const OO&... oo) {
	for (auto i = active_threads.begin(), e = active_threads.end(); i != e;)
		thread_s<typename function<F>::P>::vector(*this, sat, f, *i++, r, oo...);
//	for (auto &t : active_threads) {
//		thread_s<typename function<F>::P>::vector(*this, f, t, r, oo...);
//	}
}

SimulatorDXBC::Resource *SimulatorDXBC::get_resource(const ASMOperand &o) {
	switch (o.type) {
		case Operand::TYPE_RESOURCE:				return &srv[o.indices[0].index];
		case Operand::TYPE_UNORDERED_ACCESS_VIEW:	return &uav[o.indices[0].index];
//		case Operand::TYPE_RASTERIZER:				return &rtv;
		case Operand::TYPE_THREAD_GROUP_SHARED_MEMORY: return &grp[o.indices[0].index];
		default:									return 0;
	}
}
SimulatorDXBC::Sampler *SimulatorDXBC::get_sampler(const ASMOperand &o) {
	switch (o.type) {
		case Operand::TYPE_SAMPLER:					return &smp[o.indices[0].index];
		default:									return 0;
	}
}
bool SimulatorDXBC::split_exec(const ASMOperand &o, bool nz, e_list<ThreadState> &suspended) {
	for (auto i = active_threads.begin(), e = active_threads.end(); i != e;) {
		auto	&t = *i++;
		if (!get_scalar<uint32>(*this, t, o, false)[0] ^ nz)
			suspended.push_back(t.unlink());
	}
	return !active_threads.empty();
}

bool SimulatorDXBC::split_exec(const ASMOperand &a, const ASMOperand &b, e_list<ThreadState> &suspended) {
	for (auto i = active_threads.begin(), e = active_threads.end(); i != e;) {
		auto	&t = *i++;
		if (get_scalar<uint32>(*this, t, a, false)[0] == get_scalar<uint32>(*this, t, b, false)[0])
			suspended.push_back(t.unlink());
	}
	return !active_threads.empty();
}

const SimulatorDXBC::Register& SimulatorDXBC::get_const(const ASMOperand &op) {
	switch (op.type) {
		case Operand::TYPE_IMMEDIATE32:	return *(const Register*)op.values;
		case Operand::TYPE_IMMEDIATE64:	return *(const Register*)op.values;
		default: ISO_ASSERT(0); return dummy;
	}
}

const Opcode *SimulatorDXBC::Run(int max_steps) {
	const Opcode	*op = ucode;

	while (op < ucode.end() && op->IsDeclaration())
		op = op->next();

	while (op && op < ucode.end() && max_steps--)
		op = ProcessOp(op);

	return op;
}

float sampler_address(float v, uint32 s, TextureAddressMode mode) {
	float	f = v / s;
	switch (mode) {
		default:
		case TEXTURE_ADDRESS_MODE_WRAP:			return s * frac(f < 0 ? 1 - f : f);
		case TEXTURE_ADDRESS_MODE_MIRROR:		{ float t = frac((f < 0 ? 1 - f : f) * 0.5f) * 2; return s * (t < 1 ? t : 2 - t); }
		case TEXTURE_ADDRESS_MODE_CLAMP:		return clamp(v, 0, s - 1);
		case TEXTURE_ADDRESS_MODE_MIRROR_ONCE:	{ float t = clamp(f, 0, 2); return s * (t < 1 ? t : 2 - t); }
	}
}

struct sampler_comparer {
	ComparisonFunction	comp;
	float				ref;
	sampler_comparer(ComparisonFunction _comp, float _ref) : comp(_comp), ref(_ref) {}
	float	operator()(float f) {
		switch (comp) {
			default:
			case COMPARISON_NEVER:			return 0;
			case COMPARISON_LESS:			return f < ref;
			case COMPARISON_EQUAL:			return f == ref;
			case COMPARISON_LESS_EQUAL:		return f <= ref;
			case COMPARISON_GREATER:		return f > ref;
			case COMPARISON_NOT_EQUAL:		return f != ref;
			case COMPARISON_GREATER_EQUAL:	return f >= ref;
			case COMPARISON_ALWAYS:			return 1;
		}
	}
};

struct RawTextureReader {
	memory_block	mem;
	uint32			tsize, stride, stride2;
	RawTextureReader(SimulatorDXBC::Resource *t, uint32 _stride) : mem(t ? t->data() : none) {
		if (t) {
			if (is_buffer(t->dim)) {
				tsize	= 1;
				stride	= _stride ? _stride : t->width;
				stride2 = 0;
			} else {
				tsize	= t->format.Bytes();
				stride	= dxgi_align(tsize * adjust_size(t->format, t->width));
				stride2 = t->depth ? stride * adjust_size(t->format, t->height) : 0; 
			}
		}
	}
	void	*index(uint32 x, uint32 y, uint32 z) {
		return mem ? (uint8*)mem + stride2 * z + stride * y + x * tsize : (void*)tex_default;
	}
};

struct TextureReader : SimulatorDXBC::Resource {
	struct MIP {
		uint8	*base;
		uint32	stride;
		uint32	stride2;
		uint8*	row(uint32 y, uint32 z) const { return base + stride2 * z + stride * y; }
	};
	uint32			tsize;
	bool			block;
	TexelOffset		offset;
	int				maxlod;
	MIP				mips[16];

	TextureReader(SimulatorDXBC::Resource *t, uint8 swizzle, TexelOffset offset = 0);

	bool	check(uint32 x, uint32 y, uint32 z, uint32 mip) {
		return p && (is_buffer(dim) ? x < height : mip <= maxlod && x < max(width >> mip, 1) && y < max(height >> mip, 1));
	}

	DXGI_Components	index(uint32 x, uint32 y, uint32 z, uint32 mip) {
		if (is_buffer(dim))
			return DXGI_Components(format,
				!p ? 0 : mips[0].row(y, z) + x * tsize
			);

		return DXGI_Components(format,
			!p ? 0 : block
			? mips[mip].row(y >> 2, z) + (((x & ~3) + (y & 3)) << 2) + (x & 3)
			: mips[mip].row(y, z) + x * tsize
		);
	}
	DXGI_Components	checked_index(uint32 x, uint32 y, uint32 z, uint32 mip) {
		return check(x, y, z, mip) ? index(x, y, z, mip) : DXGI_Components(DXGI_COMPONENTS(format.Layout(), format.Type(), DXGI_COMPONENTS::ALL0), this);
	}

	float	lod(const float4p &ddx, const float4p &ddy);
	float	biased_lod(SimulatorDXBC::Sampler *samp, const float4p &ddx, const float4p &ddy) {
		return lod(ddx, ddy) + (samp ? samp->bias : 0);
	}
	float	clamped_lod(SimulatorDXBC::Sampler *samp, const float4p &ddx, const float4p &ddy) {
		return clamp(biased_lod(samp, ddx, ddy), samp->minlod, min(samp->maxlod, maxlod));
	}

	float4	sample(float x, float y, float z, uint32 mip, bool bi);

	float4	sample(SimulatorDXBC::Sampler *samp, const float4p &i, float lod);
	float	sample_c(SimulatorDXBC::Sampler *samp, const float4p &i, float ref, float lod);
	float4	gather4(SimulatorDXBC::Sampler *samp, const float4p &i, float lod);
	float4	gather4_c(SimulatorDXBC::Sampler *samp, const float4p &i, float ref, float lod);
};

TextureReader::TextureReader(SimulatorDXBC::Resource *t, uint8 swizzle, TexelOffset offset) : SimulatorDXBC::Resource(*t), block(format.IsBlock()), offset(offset) {
	format.chans = Rearrange((DXGI_COMPONENTS::SWIZZLE)format.chans, Swizzle(
		DXGI_COMPONENTS::CHANNEL((swizzle >> 0) & 3),
		DXGI_COMPONENTS::CHANNEL((swizzle >> 2) & 3),
		DXGI_COMPONENTS::CHANNEL((swizzle >> 4) & 3),
		DXGI_COMPONENTS::CHANNEL((swizzle >> 6) & 3)
	));

	int	dbl		= format.layout == DXGI_COMPONENTS::BC1;

	maxlod		= t->mips - 1;
	tsize		= format.Bytes() << dbl;

	if (is_buffer(dim)) {
		mips[0].base		= *this;
		mips[0].stride		= 0;
		mips[0].stride2		= 0;
	} else {
		uint8		*base	= *this;
		for (int i = 0; i <= maxlod; i++) {
			mips[i].base	= (uint8*)((uintptr_t)base << dbl);
			mips[i].stride	= dxgi_align(mip_stride(format, width, i));
			mips[i].stride2	= uses_z(dim) ? mips[i].stride * mip_size(format, height, i) : 0;
			base			+= mips[i].stride2 * max(depth >> i, 1);
		}
	}
}

float TextureReader::lod(const float4p &ddx, const float4p &ddy) {
	float	dudx	= ddx.x * width;
	float	dudy	= ddy.x * width;
	float	dvdx	= ddx.y * height;
	float	dvdy	= ddy.y * height;
	float	d		= max(square(dudx) + square(dvdx), square(dudy) + square(dvdy));
	return 0.5f * log2(d);
}

float4 TextureReader::sample(float x, float y, float z, uint32 mip, bool bi) {
	float	s = iorf::exp2(-mip).f();
	x *= s;
	y *= s;
	z *= s;

	uint32	ix	= uint32(x), iy = uint32(y), iz = uint32(z);
	float4	r0	= index(ix, iy, iz, mip);

	if (!bi)
		return r0;

	float4	r1	= index(ix + 1, y, z, mip);
	float4	r2	= index(ix, iy + 1, z, mip);
	float4	r3	= index(ix + 1, iy + 1, z, mip);
	float	fx	= frac(x), fy = frac(y);
	return (1 - fy) * ((1 - fx) * r0 + fx * r1) + fy * ((1 - fx) * r2 + fx * r3);
}

float4 TextureReader::sample(SimulatorDXBC::Sampler *samp, const float4p &i, float lod) {
	if (!samp) {
		return index(
			uint32(sampler_address(i.x * width  + offset.x, width,  TEXTURE_ADDRESS_MODE_WRAP)),
			uint32(sampler_address(i.y * height + offset.y, height, TEXTURE_ADDRESS_MODE_WRAP)),
			uint32(sampler_address(i.z * depth  + offset.z, depth,  TEXTURE_ADDRESS_MODE_WRAP)),
			0
		);
	}

	float	x = sampler_address(i.x * width  + offset.x - .5f, width,  samp->address_u);
	float	y = sampler_address(i.y * height + offset.y - .5f, height, samp->address_v);
	float	z = sampler_address(i.z * depth  + offset.z - .5f, depth,  samp->address_w);

	if (lod > maxlod)
		return sample(x, y, z, maxlod, samp->filter.min == TextureFilterMode::LINEAR);

	lod			= clamp(lod, samp->minlod, samp->maxlod);
	float	fl	= frac(lod);

	float4	r	= sample(x, y, z, uint32(lod), samp->filter.mag == TextureFilterMode::LINEAR);
	if (samp->filter.mip == TextureFilterMode::POINT || fl == 0)
		return r;

	float4	r2	= sample(x, y, z, uint32(lod) + 1, samp->filter.mag == TextureFilterMode::LINEAR);
	return (1 - fl) * r + fl * r2;
}

float TextureReader::sample_c(SimulatorDXBC::Sampler *samp, const float4p &i, float ref, float lod) {
	sampler_comparer	c(samp->comparison, ref);

	float	x = sampler_address(i.x * width  + offset.x - .5f, width,  samp->address_u);
	float	y = sampler_address(i.y * height + offset.y - .5f, height, samp->address_v);
	float	z = sampler_address(i.z * depth  + offset.z - .5f, depth,  samp->address_w);

	uint32	ix	= uint32(x), iy = uint32(y), iz = uint32(z), im = uint32(lod);
	float	r	= c(index(ix, iy, iz, im));

	if (samp->filter.mag != TextureFilterMode::POINT) {
		float	r1	= c(index(ix + 1, y, z, im));
		float	r2	= c(index(ix, iy + 1, z, im));
		float	r3	= c(index(ix + 1, iy + 1, z, im));
		
		float	fx = frac(x), fy = frac(y);
		r = (1 - fy) * ((1 - fx) * r + fx * r1) + fy * ((1 - fx) * r2 + fx * r3);
	}
	return r;
}

float4 TextureReader::gather4(SimulatorDXBC::Sampler *samp, const float4p &i, float lod) {
	uint32	ix	= uint32(sampler_address(i.x * width  + offset.x - .5f, width,  samp->address_u));
	uint32	iy	= uint32(sampler_address(i.y * height + offset.y - .5f, height, samp->address_v));
	uint32	iz	= uint32(sampler_address(i.z * depth  + offset.z - .5f, depth,  samp->address_w));
	uint32	im	= uint32(lod);

	return {
		(float)index(ix, iy, iz, im),
		(float)index(ix + 1, iy, iz, im),
		(float)index(ix, iy + 1, iz, im),
		(float)index(ix + 1, iy + 1, iz, im)
	};
}

float4 TextureReader::gather4_c(SimulatorDXBC::Sampler *samp, const float4p &i, float ref, float lod) {
	sampler_comparer	c(samp->comparison, ref);

	uint32	ix	= uint32(sampler_address(i.x * width  + offset.x - .5f, width,  samp->address_u));
	uint32	iy	= uint32(sampler_address(i.y * height + offset.y - .5f, height, samp->address_v));
	uint32	iz	= uint32(sampler_address(i.z * depth  + offset.z - .5f, depth,  samp->address_w));
	uint32	im	= uint32(lod);

	return {
		c(index(ix, iy, iz, im)),
		c(index(ix + 1, iy, iz, im)),
		c(index(ix, iy + 1, iz, im)),
		c(index(ix + 1, iy + 1, iz, im))
	};
}

int CountCases(const Opcode *op, const Opcode *end, bool &loop) {
	int	cases = 0;
	for (int depth = 0; op < end; op = op->next()) {
		if (depth == 0 && (op->Op() == OPCODE_CASE || op->Op() == OPCODE_DEFAULT))
			++cases;

		if (op->Op() == OPCODE_LOOP || op->Op() == OPCODE_SWITCH)
			depth++;

		if ((op->Op() == OPCODE_ENDLOOP || op->Op() == OPCODE_ENDSWITCH) && depth-- == 0) {
			loop = op->Op() == OPCODE_ENDLOOP;
			break;
		}
	}
	return cases;
}

const Opcode *SimulatorDXBC::ProcessOp(const Opcode *op) {
	auto				code	= op->Op();
	const uint32		*p		= op->operands();
	ResourceDimension	dim		= RESOURCE_DIMENSION_UNKNOWN;
	uint32				stride	= 0;
	ResourceReturnType	ret;
	TexelOffset			tex_offset;

	for (bool extended = op->Extended == 1; extended;) {
		ExtendedOpcode		token	= (ExtendedOpcode&)*p++;
		switch (token.Type) {
			case EXTENDED_OPCODE_SAMPLE_CONTROLS:		tex_offset	= token; break;
			case EXTENDED_OPCODE_RESOURCE_DIM:			dim			= (ResourceDimension)token.ResourceDim; stride = token.BufferStride; break;
			case EXTENDED_OPCODE_RESOURCE_RETURN_TYPE:	ret			= token; break;
		}
		extended = token.Extended == 1;
	}

	uint32 func = 0;
	if (code == OPCODE_INTERFACE_CALL)
		func = *p++;

	dynamic_array<ASMOperand>	ops(op->NumOperands());

	for (size_t i = 0; i < ops.size(); i++)
		ops[i].Extract(p);

	bool	sat		= op->Saturate;
	bool	loop	= false;
	int		cases	= code == OPCODE_BREAKC || code == OPCODE_BREAK ? CountCases(op, ucode.end(), loop) : 0;

	switch (code) {
// CONTROL FLOW

		//loops
		case OPCODE_LOOP:
			active_stack.push_back();	// resume at next iteration
			active_stack.push_back();	// resume after loop
			break;
		case OPCODE_BREAKC:
			if (split_exec(ops[0], op->TestNonZero, active_stack.back(cases)))
				break;
			//fall through
		case OPCODE_BREAK:
			if (code == OPCODE_BREAK)
				active_stack.back(cases).append(move(active_threads));

			if (!loop || active_stack.back(1).empty()) {

				for (int depth = 0; op < ucode.end(); op = op->next()) {
					if (depth == 0 && (op->Op() == OPCODE_CASE || op->Op() == OPCODE_DEFAULT || op->Op() == OPCODE_ENDSWITCH))
						return op;

					if (op->Op() == OPCODE_LOOP || op->Op() == OPCODE_SWITCH)
						depth++;

					if ((op->Op() == OPCODE_ENDLOOP || op->Op() == OPCODE_ENDSWITCH) && depth-- == 0) {
						active_threads = active_stack.pop_back_value();
						active_stack.pop_back();	// discard next iteration threads
						break;
					}
				}
			}
			break;

		case OPCODE_CONTINUEC:
			if (split_exec(ops[0], op->TestNonZero, active_stack.back(1)))
				break;
			//fall through
		case OPCODE_CONTINUE:
		case OPCODE_ENDLOOP:
			active_threads.append(move(active_stack.back(1)));
			return loop_hash[op];

		//if
		case OPCODE_IF:
			if (split_exec(ops[0], !op->TestNonZero, active_stack.push_back()))
				break;

			op = op->next();
			for (int depth = 0; op < ucode.end(); op = op->next()) {
				if (op->Op() == OPCODE_IF)
					depth++;
				if (op->Op() == OPCODE_ENDIF || (depth == 0 && op->Op() == OPCODE_ELSE))
					depth--;
				if (depth < 0)
					break;
			}
			if (op->Op() == OPCODE_ENDIF) {
				active_threads = active_stack.pop_back_value();
			} else {
				swap(active_threads, active_stack.back());
			}
			break;

		case OPCODE_ELSE:
			if (active_stack.back().empty()) {
				active_stack.pop_back();
				for (int depth = 0; op < ucode.end(); op = op->next()) {
					if (op->Op() == OPCODE_IF)
						depth++;

					if (op->Op() == OPCODE_ENDIF && depth-- == 0)
						break;
				}
			} else {
				swap(active_threads, active_stack.back());
			}
			break;
			
		//switch
		case OPCODE_SWITCH: {
			int		start_stack		= active_stack.size32();
			int		default_stack	= -1;
			int		depth			= 0;

			for (auto op2 = op->next(); op2 < ucode.end(); op2 = op2->next()) {
				if (op2->Op() == OPCODE_SWITCH) {
					depth++;

				} else if (op2->Op() == OPCODE_ENDSWITCH) {
					if (depth-- == 0)
						break;

				} else if (depth == 0) {
					switch (op2->Op()) {
						case OPCODE_DEFAULT:
							default_stack = active_stack.size32();
						//case OPCODE_BREAK:
							active_stack.push_back();
							break;
						case OPCODE_CASE: {
							const uint32	*p = op2->operands();
							split_exec(ops[0], ASMOperand(p), active_stack.push_back());
							break;
						}
					}
				}
			}
			if (default_stack >= 0)
				active_stack[default_stack] = move(active_threads);

			active_stack.push_back(move(active_threads));
			reverse(active_stack.begin() + start_stack, active_stack.end());
			break;
		}
		case OPCODE_CASE:
		case OPCODE_DEFAULT:
			active_threads.append(active_stack.pop_back_value());
			if (active_threads.empty()) {
				int		depth	= 0;
				for (op = op->next(); op < ucode.end(); op = op->next()) {
					if (depth == 0 && (op->Op() == OPCODE_CASE || op->Op() == OPCODE_DEFAULT || op->Op() == OPCODE_ENDSWITCH))
						return op;

					if (op->Op() == OPCODE_LOOP || op->Op() == OPCODE_SWITCH)
						++depth;

					if (op->Op() == OPCODE_ENDLOOP || op->Op() == OPCODE_ENDSWITCH)
						--depth;
				}
			}
			break;

		case OPCODE_ENDIF:
		case OPCODE_ENDSWITCH:
			active_threads.append(active_stack.pop_back_value());
			break;

		case OPCODE_RETC:
			split_exec(ops[0], op->TestNonZero, active_stack.push_back());
			active_stack.pop_back_value();
			if (active_threads.empty() && active_stack.empty())
				return 0;
			break;

		case OPCODE_RET:
			if (!call_stack.empty())
				return call_stack.pop_back_value();
			active_threads.clear();
			if (phase == phNormal && active_stack.empty())
				return 0;
			if (op->next() >= ucode.end())
				return 0;
			break;

		case OPCODE_LABEL:
			break;

		case OPCODE_INTERFACE_CALL: {
			uint32	intf0	= 0;
			auto	intf1	= interface_tables[intf0];
			auto	intf2	= function_tables[intf1[ops[0].indices[0].index]];
			uint32	label	= intf2[ops[0].indices[1].index];
			call_stack.push_back(op->next());
			return labels[label];
		}

		case OPCODE_CALL:
			call_stack.push_back(op->next());
			return labels[ops[0].Index()];

		case OPCODE_CALLC:
			split_exec(ops[0], op->TestNonZero, active_stack.push_back());
			call_stack.push_back(op->next());
			return labels[ops[1].Index()];

		case OPCODE_HS_CONTROL_POINT_PHASE: {
			do
				op = op->next();
			while (op->IsDeclaration());

			active_stack.clear();
			active_threads.clear();
			
			auto	*t = threads.begin();
			for (int i = 0; i < output_control_points && t < threads.end(); i++, t++) {
				(uint32&)t->regs[input_offset + vOutputControlPointID] = i;
				active_threads.push_back(t);
			}

			phase = phControlPoints;
			return op;
		}

		case OPCODE_HS_FORK_PHASE: {
			uint32	count		= 1;
			do {
				op = op->next();
				if (op->Op() == OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT)
					count = *op->operands();
			} while (op->IsDeclaration());

			active_stack.clear();
			active_threads.clear();

			if (phase != phConstantsFork) {
				phase		= phConstantsFork;
				fork_count	= 0;
			}
			
			auto	*t = threads + fork_count;
			fork_count += count;
			for (int i = 0; count-- && t < threads.end(); i++, t++) {
				(uint32&)t->regs[input_offset + vForkInstanceID] = i;
				active_threads.push_back(t);
			}
			return op;
		}

		case OPCODE_HS_JOIN_PHASE: {
			uint32	count		= 1;
			do {
				op = op->next();
				if (op->Op() == OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT)
					count = *op->operands();
			} while (op->IsDeclaration());

			active_stack.clear();
			active_threads.clear();

			if (phase != phConstantsJoin) {
				phase = phConstantsJoin;
				fork_count	= 0;
			}

			auto	*t = threads + fork_count;
			fork_count += count;
			for (int i = 0; count-- && t < threads.end(); i++, t++) {
				(uint32&)t->regs[input_offset + vJoinInstanceID] = i;
				active_threads.push_back(t);
			}
			return op;
		}

// ALU

		case OPCODE_ADD:		operate(sat, [](ThreadState &ts, float &r, float a, float b)				{ r = a + b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_AND:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ r = a & b; }, ops[0], ops[1], ops[2]); break;
//		case OPCODE_DERIV_RTX:	ddxy(ops[0], ops[1], ~1, 1); break;
//		case OPCODE_DERIV_RTY:	ddxy(ops[0], ops[1], ~2, 2); break;
		case OPCODE_DERIV_RTX:	operate_vec(sat, [](ThreadState &ts, float4p &r, float4p a)					{ r = a; }, ops[0], ddx(ops[1])); break;
		case OPCODE_DERIV_RTY:	operate_vec(sat, [](ThreadState &ts, float4p &r, float4p a)					{ r = a; }, ops[0], ddy(ops[1])); break;
		case OPCODE_DISCARD:	operate_vec(sat, [op](ThreadState &ts, uint32 a)							{ if (!a ^ op->TestNonZero) ts.discard(); }, ops[0]); break;
		case OPCODE_DIV:		operate(sat, [](ThreadState &ts, float &r, float a, float b)				{ r = a / b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DP2:		operate_vec(sat, [](ThreadState &ts, float &r, float2p a, float2p b)		{ r = dot(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DP3:		operate_vec(sat, [](ThreadState &ts, float &r, float3p a, float3p b)		{ r = dot(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DP4:		operate_vec(sat, [](ThreadState &ts, float &r, float4p a, float4p b)		{ r = dot(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_EQ:			operate(sat, [](ThreadState &ts, uint32 &r, float a, float b)				{ r = a == b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_EXP:		operate(sat, [](ThreadState &ts, float &r, float a)							{ r = exp2(a); }, ops[0], ops[1]); break;
		case OPCODE_FRC:		operate(sat, [](ThreadState &ts, float &r, float a)							{ r = a - floor(a); }, ops[0], ops[1]); break;
		case OPCODE_FTOI:		operate(sat, [](ThreadState &ts, int32 &r, float a)							{ r = int(a); }, ops[0], ops[1]); break;
		case OPCODE_FTOU:		operate(sat, [](ThreadState &ts, uint32 &r, float a)						{ r = uint32(a); }, ops[0], ops[1]); break;
		case OPCODE_GE:			operate(sat, [](ThreadState &ts, uint32 &r, float a, float b)				{ r = -int(a >= b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_IADD:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = a + b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_IEQ:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = -int(a == b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_IGE:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = -int(a >= b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_ILT:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = -int(a < b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_IMAD:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b, int32 c)		{ r = a * b + c; }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_IMAX:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = max(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_IMIN:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = min(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_IMUL: {
			int	nc = max(ops[0].NumComponents(), ops[1].NumComponents());
			if (ops[0].type == Operand::TYPE_NULL)
				operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)	{ r = mul(a, b); }, nc, ops[1], ops[2], ops[3]);
			else
				operate(sat, [](ThreadState &ts, int32 &rhi, int32 &rlo, int32 a, int32 b)	{
					int64 r64 = (int64)a * (int64)b;
					rhi = hi(r64);
					rlo = lo(r64);
				}, nc, ops[0], ops[1], ops[2], ops[3]);
			break;
		}
		case OPCODE_INE:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = -int(a != b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_INEG:		operate(sat, [](ThreadState &ts, int32 &r, int32 a)							{ r = -a; }, ops[0], ops[1]); break;
		case OPCODE_ISHL:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = a << b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_ISHR:		operate(sat, [](ThreadState &ts, int32 &r, int32 a, int32 b)				{ r = ashift_right(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_ITOF:		operate(sat, [](ThreadState &ts, float &r, int32 a)							{ r = a; }, ops[0], ops[1]); break;

		case OPCODE_LOG:		operate(sat, [](ThreadState &ts, float &r, float a)							{ r = log2(a); }, ops[0], ops[1]); break;
		case OPCODE_LT:			operate(sat, [](ThreadState &ts, uint32 &r, float a, float b)				{ r = -int(a < b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_MAD:		operate(sat, [](ThreadState &ts, float &r, float a, float b, float c)		{ r = a * b + c; }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_MIN:		operate(sat, [](ThreadState &ts, float &r, float a, float b)				{ r = min(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_MAX:		operate(sat, [](ThreadState &ts, float &r, float a, float b)				{ r = max(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_MOV:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a)						{ r = a; }, ops[0], ops[1]); break;
		case OPCODE_MOVC:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 t, uint32 a, uint32 b)	{ r = t ? a : b; }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_MUL:		operate(sat, [](ThreadState &ts, float &r, float a, float b)				{ r = a * b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_NE:			operate(sat, [](ThreadState &ts, uint32 &r, float a, float b)				{ r = -int(a != b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_NOP:		break;
		case OPCODE_NOT:		operate(sat, [](ThreadState &ts, int32 &r, int32 a)							{ r = ~a; }, ops[0], ops[1]); break;
		case OPCODE_OR:			operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ r = a | b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_ROUND_NE:	operate(sat, [](ThreadState &ts, float &r, float a)							{ r = round(a); }, ops[0], ops[1]); break;
		case OPCODE_ROUND_NI:	operate(sat, [](ThreadState &ts, float &r, float a)							{ r = floor(a); }, ops[0], ops[1]); break;
		case OPCODE_ROUND_PI:	operate(sat, [](ThreadState &ts, float &r, float a)							{ r = ceil(a); }, ops[0], ops[1]); break;
		case OPCODE_ROUND_Z:	operate(sat, [](ThreadState &ts, float &r, float a)							{ r = trunc(a); }, ops[0], ops[1]); break;
		case OPCODE_RSQ:		operate(sat, [](ThreadState &ts, float &r, float a)							{ r = rsqrt(a); }, ops[0], ops[1]); break;

		case OPCODE_SQRT:		operate(sat, [](ThreadState &ts, float &r, float a)							{ r = sqrt(a); }, ops[0], ops[1]); break;
		case OPCODE_SINCOS:		operate(sat, [](ThreadState &ts, float &s, float &c, float a)				{ s = sin(a); c = cos(a); }, ops[0], ops[1], ops[2]); break;

		case OPCODE_UDIV:		operate(sat, [](ThreadState &ts, uint32 &rq, uint32 &rr, uint32 a, uint32 b){ rq = b ? a / b : ~0; rr = b ? a % b : ~0; }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_ULT:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ r = -int(a < b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_UGE:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ r = -int(a >= b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_UMUL:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ r = a * b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_UMAD:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b, uint32 c)	{ r = a * b + c; }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_UMAX:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ r = max(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_UMIN:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ r = min(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_USHR:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ r = a >> b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_UTOF:		operate(sat, [](ThreadState &ts, float &r, uint32 a)						{ r = a; }, ops[0], ops[1]); break;

		case OPCODE_DERIV_RTX_COARSE:	operate_vec(sat, [](ThreadState &ts, float4p &r, float4p a)			{ r = a; }, ops[0], DD<~3,1>(ops[1])); break;
		case OPCODE_DERIV_RTX_FINE:		operate_vec(sat, [](ThreadState &ts, float4p &r, float4p a)			{ r = a; }, ops[0], DD<~1,1>(ops[1])); break;
		case OPCODE_DERIV_RTY_COARSE:	operate_vec(sat, [](ThreadState &ts, float4p &r, float4p a)			{ r = a; }, ops[0], DD<~3,2>(ops[1])); break;
		case OPCODE_DERIV_RTY_FINE:		operate_vec(sat, [](ThreadState &ts, float4p &r, float4p a)			{ r = a; }, ops[0], DD<~2,2>(ops[1])); break;
//		case OPCODE_DERIV_RTX_COARSE:	ddxy(ops[0], ops[1], ~3, 1); break;
//		case OPCODE_DERIV_RTX_FINE:		ddxy(ops[0], ops[1], ~1, 1); break;
//		case OPCODE_DERIV_RTY_COARSE:	ddxy(ops[0], ops[1], ~3, 2); break;
//		case OPCODE_DERIV_RTY_FINE:		ddxy(ops[0], ops[1], ~2, 2); break;

		case OPCODE_RCP:		operate(sat, [](ThreadState &ts, float &r, float a)							{ r = reciprocal(a); }, ops[0], ops[1]); break;
		case OPCODE_F32TOF16:	operate(sat, [](ThreadState &ts, uint32 &r, float a)						{ (float16&)r = float16(a); }, ops[0], ops[1]); break;
		case OPCODE_F16TOF32:	operate(sat, [](ThreadState &ts, float &r, uint32 a)						{ r = (const float16&)a; }, ops[0], ops[1]); break;
		case OPCODE_UADDC:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ ts.carry = addc(a, b, ts.carry, r); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_USUBB:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 a, uint32 b)				{ ts.carry = subc(a, b, ts.carry, r); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_COUNTBITS:	operate(sat, [](ThreadState &ts, uint32 &r, uint32 a)						{ r = count_bits(a); }, ops[0], ops[1]); break;
		case OPCODE_FIRSTBIT_HI:operate(sat, [](ThreadState &ts, uint32 &r, uint32 a)						{ r = highest_set_index(a); }, ops[0], ops[1]); break;
		case OPCODE_FIRSTBIT_LO:operate(sat, [](ThreadState &ts, uint32 &r, uint32 a)						{ r = lowest_set_index(a); }, ops[0], ops[1]); break;
		case OPCODE_FIRSTBIT_SHI:operate(sat,[](ThreadState &ts, uint32 &r, int32 a)						{ r = highest_set_index(abs(a)); }, ops[0], ops[1]); break;
		case OPCODE_UBFE:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 width, uint32 offset, uint32 c)			{ r = extract_bits(c, offset & 31, width & 31); }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_IBFE:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 width, uint32 offset, int32 c)			{ r = extract_bits(c, offset & 31, width & 31); }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_BFI:		operate(sat, [](ThreadState &ts, uint32 &r, uint32 width, uint32 offset, uint32 a, uint32 b){ r = copy_bits(a, b, 0, offset & 31, width & 31); }, ops[0], ops[1], ops[2], ops[3], ops[4]); break;
		case OPCODE_BFREV:		operate(sat, [](ThreadState &ts, uint32 &r, int32 a)										{ r = reverse_bits(a); }, ops[0], ops[1]); break;
		case OPCODE_SWAPC:		operate(sat, [](ThreadState &ts, uint32 &d0, uint32 &d1, uint32 test, uint32 a, uint32 b)	{ d0 = test ? b : a; d1 = test ? a : b; }, ops[0], ops[1], ops[2], ops[3], ops[4]); break;

		case OPCODE_DADD:		operate(sat, [](ThreadState &ts, double &r, double a, double b)				{ r = a + b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DMAX:		operate(sat, [](ThreadState &ts, double &r, double a, double b)				{ r = max(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DMIN:		operate(sat, [](ThreadState &ts, double &r, double a, double b)				{ r = min(a, b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DMUL:		operate(sat, [](ThreadState &ts, double &r, double a, double b)				{ r = a * b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DEQ:		operate(sat, [](ThreadState &ts, uint32 &r, double a, double b)				{ r = -int(a == b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DGE:		operate(sat, [](ThreadState &ts, uint32 &r, double a, double b)				{ r = -int(a >= b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DLT:		operate(sat, [](ThreadState &ts, uint32 &r, double a, double b)				{ r = -int(a < b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DNE:		operate(sat, [](ThreadState &ts, uint32 &r, double a, double b)				{ r = -int(a != b); }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DMOV:		operate(sat, [](ThreadState &ts, double &r, double a)						{ r = a; }, ops[0], ops[1]); break;
		case OPCODE_DMOVC:		operate(sat, [](ThreadState &ts, double &r, uint32 t, double a, double b)	{ r = t ? a : b; }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_DTOF:		operate(sat, [](ThreadState &ts, float &r, double a)						{ r = a; }, ops[0], ops[1]); break;
		case OPCODE_FTOD:		operate(sat, [](ThreadState &ts, double &r, float a)						{ r = a; }, ops[0], ops[1]); break;
		case OPCODE_DDIV:		operate(sat, [](ThreadState &ts, double &r, double a, double b)				{ r = a / b; }, ops[0], ops[1], ops[2]); break;
		case OPCODE_DFMA:		operate(sat, [](ThreadState &ts, double &r, double a, double b, double c)	{ r = a * b + c; }, ops[0], ops[1], ops[2], ops[3]); break;
		case OPCODE_DRCP:		operate(sat, [](ThreadState &ts, double &r, double a)						{ r = reciprocal(a); }, ops[0], ops[1]); break;
		case OPCODE_DTOI:		operate(sat, [](ThreadState &ts, int32 &r, double a)						{ r = a; }, ops[0], ops[1]); break;
		case OPCODE_DTOU:		operate(sat, [](ThreadState &ts, uint32 &r, double a)						{ r = uint32(a); }, ops[0], ops[1]); break;
		case OPCODE_ITOD:		operate(sat, [](ThreadState &ts, double &r, int32 a)						{ r = a; }, ops[0], ops[1]); break;
		case OPCODE_UTOD:		operate(sat, [](ThreadState &ts, double &r, uint32 a)						{ r = a; }, ops[0], ops[1]); break;

		case OPCODE_MSAD:		operate_vec(sat, [](ThreadState &ts, array_vec<uint32, 4> &result, uint32 ref, array_vec<uint32, 2> source, array_vec<uint32, 4> accum) {
				uint64	s	= (uint64&)source;
				for (int i = 0; i < 4; i++) {
					uint32	t = s >> (i * 8);
					accum[i] += (ref & 0xff000000 ? abs(int(ref & 0xff000000) - int(t & 0xff000000)) >> 24 : 0)
							+	(ref & 0x00ff0000 ? abs(int(ref & 0x00ff0000) - int(t & 0x00ff0000)) >> 16 : 0)
							+	(ref & 0x0000ff00 ? abs(int(ref & 0x0000ff00) - int(t & 0x0000ff00)) >>  8 : 0)
							+	(ref & 0x000000ff ? abs(int(ref & 0x000000ff) - int(t & 0x000000ff)) >>  0 : 0);
				}
				result = accum;
			}, ops[0], ops[1], ops[2], ops[3]);
			break;

//		case OPCODE_EVAL_SNAPPED:
//		case OPCODE_EVAL_SAMPLE_INDEX:
//		case OPCODE_EVAL_CENTROID:
//		case OPCODE_ABORT:
//		case OPCODE_DEBUGBREAK:
//		case OPCODE_RESERVED2:

// BUFFER LOAD/STORE
		case OPCODE_LD: {
			TextureReader	tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			operate_vec(sat, [&tex](ThreadState &ts, float4p &r, array_vec<uint32, 4> i) { r = tex.checked_index(i.x, i.y, i.z, i.w); }, ops[0], ops[1]);
			break;
		}
		case OPCODE_LD_UAV_TYPED: {
			RawTextureReader	tex(get_resource(ops[2]), 0);
			operate_vec(sat, [&tex](ThreadState &ts, float4p &r, array_vec<uint32, 4> i) { r = *(float4p*)tex.index(i.x, i.y, i.z); }, ops[0], ops[1]);
			break;
		}
		case OPCODE_STORE_UAV_TYPED: {
			TextureReader	tex(get_resource(ops[0]), ops[0].swizzle_bits, tex_offset);
			operate_vec(sat, [&tex](ThreadState &ts, array_vec<uint32, 4> i, float4p v) { tex.checked_index(i.x, i.y, i.z, 0) = v; }, ops[1], ops[2]);
			break;
		}
		case OPCODE_LD_MS: {
			TextureReader	tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			operate_vec(sat, [&tex](ThreadState &ts, float4p &r, array_vec<uint32, 4> i, int sample) { r = tex.checked_index(i.x, i.y, i.z, i.w); }, ops[0], ops[1], ops[3]);
			break;
		}
		case OPCODE_LD_RAW: {
			TextureReader	tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			operate_vec(sat, [&tex](ThreadState &ts, float4p &r, uint32 i) { r = *(float4p*)(tex + i); }, ops[0], ops[1]);
			break;
		}
		case OPCODE_STORE_RAW: {
			TextureReader	tex(get_resource(ops[0]), ops[0].swizzle_bits, tex_offset);
			operate_vec(sat, [&tex](ThreadState &ts, uint32 i, float4p &v) { *(float4p*)(tex + i) = v; }, ops[1], ops[2]);
			break;
		}
		case OPCODE_LD_STRUCTURED: {
			RawTextureReader	tex(get_resource(ops[3]), stride);
			uint8				swiz = ops[3].swizzle_bits;
			operate_vec(sat, [&tex, swiz](ThreadState &ts, float4p &r, int row, uint32 elem) { r = swizzle1(*(float4p*)tex.index(elem, row, 0), swiz); }, ops[0], ops[1], ops[2]);
			break;
		}
		case OPCODE_STORE_STRUCTURED: {
			RawTextureReader	tex(get_resource(ops[0]), stride);
			uint8				mask = ops[0].swizzle_bits;
			operate_vec(sat, [&tex, mask](ThreadState &ts, int row, uint32 elem, float4p v) {
				mask_write(*(float4p*)tex.index(elem, row, 0), v, mask);
			}, ops[1], ops[2], ops[3]);
			break;
		}

//		case OPCODE_LD_FEEDBACK:
//		case OPCODE_LD_MS_FEEDBACK:
//		case OPCODE_LD_RAW_FEEDBACK:
//		case OPCODE_LD_STRUCTURED_FEEDBACK:
//		case OPCODE_LD_UAV_TYPED_FEEDBACK:

		case OPCODE_ATOMIC_AND:			atomic_op(sat, ops, stride, [](uint32 &r, uint32 a) { r &= a; }); break;
		case OPCODE_ATOMIC_OR:			atomic_op(sat, ops, stride, [](uint32 &r, uint32 a) { r |= a; }); break;
		case OPCODE_ATOMIC_XOR:			atomic_op(sat, ops, stride, [](uint32 &r, uint32 a) { r ^= a; }); break;
		case OPCODE_ATOMIC_IADD:		atomic_op(sat, ops, stride, [](uint32 &r, uint32 a) { r += a; }); break;
		case OPCODE_ATOMIC_IMAX:		atomic_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = max((int)r, (int)a); }); break;
		case OPCODE_ATOMIC_IMIN:		atomic_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = min((int)r, (int)a); }); break;
		case OPCODE_ATOMIC_UMAX:		atomic_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = max(r, a); }); break;
		case OPCODE_ATOMIC_UMIN:		atomic_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = min(r, a); }); break;
		case OPCODE_ATOMIC_CMP_STORE: {
			RawTextureReader	tex(get_resource(ops[0]), stride);
			operate_vec(sat, [&tex](ThreadState &ts, array_vec<uint32, 4> i, uint32 a, uint32 b) {
				uint32	*p = (uint32*)tex.index(i.x, i.y, i.z);
				if (*p == a)
					*p = b;
			}, ops[1], ops[2], ops[3]);
			break;
		}

									  
		case OPCODE_IMM_ATOMIC_ALLOC: {
			auto	*res = get_resource(ops[0]);
			operate_vec(sat, [res](ThreadState &ts, array_vec<uint32, 4> i) { uint32 x = res->counter++; i = x; }, ops[1]);
			break;
		}
		case OPCODE_IMM_ATOMIC_CONSUME: {
			auto	*res = get_resource(ops[0]);
			operate_vec(sat, [res](ThreadState &ts, array_vec<uint32, 4> i) { uint32 x = --res->counter; i = x; }, ops[1]);
			break;
		}

		case OPCODE_IMM_ATOMIC_AND:		atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r &= a; }); break;
		case OPCODE_IMM_ATOMIC_OR:		atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r |= a; }); break;
		case OPCODE_IMM_ATOMIC_XOR:		atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r ^= a; }); break;
		case OPCODE_IMM_ATOMIC_IADD:	atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r += a; }); break;
		case OPCODE_IMM_ATOMIC_IMAX:	atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = max((int)r, (int)a); }); break;
		case OPCODE_IMM_ATOMIC_IMIN:	atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = min((int)r, (int)a); }); break;
		case OPCODE_IMM_ATOMIC_UMAX:	atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = max(r, a); }); break;
		case OPCODE_IMM_ATOMIC_UMIN:	atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = min(r, a); }); break;
		case OPCODE_IMM_ATOMIC_EXCH:	atomic_imm_op(sat, ops, stride, [](uint32 &r, uint32 a) { r = a; }); break;
		case OPCODE_IMM_ATOMIC_CMP_EXCH: {
			RawTextureReader	tex(get_resource(ops[1]), stride);
			operate_vec(sat, [&tex](ThreadState &ts, uint32 &prev, array_vec<uint32, 4> i, uint32 a, uint32 b) {
				uint32	*p = (uint32*)tex.index(i.x, i.y, i.z);
				prev	= *p;
				if (prev == a)
					*p = b;
			}, ops[0], ops[2], ops[3], ops[4]);
			break;
		}

		case OPCODE_SYNC:
			break;

// TEXTURE SAMPLE
		case OPCODE_LOD: {
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p ddx, float4p ddy) { r.y = tex.biased_lod(samp, ddx, ddy); r.x = clamp(r.y, samp->minlod, min(samp->maxlod, tex.maxlod)); r.z = r.w = 0; }, ops[0], ddx(ops[1]), ddy(ops[1]));
			break;
		}
		case OPCODE_SAMPLE: {
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy) { r = tex.sample(samp, i, tex.biased_lod(samp, ddx, ddy)); }, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]));
			break;
		}
		case OPCODE_SAMPLE_L: {		//lod
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p i, float lod) { r = tex.sample(samp, i, lod); }, ops[0], ops[1], ops[2]);
			break;
		}
		case OPCODE_SAMPLE_B: {		//bias
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy, float bias) { r = tex.sample(samp, i, tex.biased_lod(samp, ddx, ddy) + bias); }, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[4]);
			break;
		}
		case OPCODE_SAMPLE_D: {		//derivs
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy) { r = tex.sample(samp, i, tex.biased_lod(samp, ddx, ddy)); }, ops[0], ops[1], ops[4], ops[5]);
			break;
		}
		case OPCODE_SAMPLE_C: {		//comparison
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float &r, float4p i, float4p ddx, float4p ddy, float ref) { r = tex.sample_c(samp, i, ref, tex.biased_lod(samp, ddx, ddy)); }, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[4]);
			break;
		}
		case OPCODE_SAMPLE_C_LZ: {	//comparison, level 0
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float &r, float4p i, float ref) { r = tex.sample_c(samp, i, ref, 0); }, ops[0], ops[1], ops[4]);
			break;
		}

//		case OPCODE_SAMPLE_L_FEEDBACK:
//		case OPCODE_SAMPLE_C_LZ_FEEDBACK:
//		case OPCODE_SAMPLE_CLAMP_FEEDBACK:
//		case OPCODE_SAMPLE_B_CLAMP_FEEDBACK:
//		case OPCODE_SAMPLE_D_CLAMP_FEEDBACK:
//		case OPCODE_SAMPLE_C_CLAMP_FEEDBACK:
//		case OPCODE_SAMPLE_POS:
//		case OPCODE_SAMPLE_INFO:
//		case OPCODE_CHECK_ACCESS_FULLY_MAPPED:

		case OPCODE_GATHER4: {
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy) { r = tex.gather4(samp, i, tex.biased_lod(samp, ddx, ddy)); }, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]));
			break;
		}
		case OPCODE_GATHER4_C: {
			TextureReader			tex(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[3]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy, float ref) { r = tex.gather4_c(samp, i, tex.biased_lod(samp, ddx, ddy), ref); }, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[4]);
			break;
		}
		case OPCODE_GATHER4_PO: {
			TextureReader			tex(get_resource(ops[3]), ops[3].swizzle_bits);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[4]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy, array_vec<int32, 4> offset) {
				uint32	ix	= uint32(sampler_address(i.x * tex.width  + offset.x - .5f, tex.width,  samp->address_u));
				uint32	iy	= uint32(sampler_address(i.y * tex.height + offset.y - .5f, tex.height, samp->address_v));
				uint32	iz	= uint32(sampler_address(i.z * tex.depth  + offset.z - .5f, tex.depth,  samp->address_w));
				uint32	im	= uint32(tex.clamped_lod(samp, ddx, ddy));
				r.x = tex.index(ix, iy, iz, im);
				r.y = tex.index(ix + 1, iy, iz, im);
				r.z = tex.index(ix, iy + 1, iz, im);
				r.w = tex.index(ix + 1, iy + 1, iz, im);
			}, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[2]);
			break;
		}
		case OPCODE_GATHER4_PO_C: {
			TextureReader			tex(get_resource(ops[3]), ops[3].swizzle_bits);
			SimulatorDXBC::Sampler	*samp = get_sampler(ops[4]);
			operate_vec(sat, [&tex, samp](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy, array_vec<int32, 4> offset, float ref) {
				sampler_comparer	c(samp->comparison, ref);
				uint32	ix	= uint32(sampler_address(i.x * tex.width  + offset.x - .5f, tex.width,  samp->address_u));
				uint32	iy	= uint32(sampler_address(i.y * tex.height + offset.y - .5f, tex.height, samp->address_v));
				uint32	iz	= uint32(sampler_address(i.z * tex.depth  + offset.z - .5f, tex.depth,  samp->address_w));
				uint32	im	= uint32(tex.clamped_lod(samp, ddx, ddy));
				r.x = c(tex.index(ix, iy, iz, im));
				r.y = c(tex.index(ix + 1, iy, iz, im));
				r.z = c(tex.index(ix, iy + 1, iz, im));
				r.w = c(tex.index(ix + 1, iy + 1, iz, im));
			}, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[2], ops[5]);
			break;
		}
//		case OPCODE_GATHER4_FEEDBACK:
//		case OPCODE_GATHER4_C_FEEDBACK:
//		case OPCODE_GATHER4_PO_FEEDBACK:
//		case OPCODE_GATHER4_PO_C_FEEDBACK:

		case OPCODE_BUFINFO: {
			SimulatorDXBC::Resource	*res = get_resource(ops[1]);
			operate_vec(sat, [res](ThreadState &ts, point4 &r) {
				r.x = r.y = r.z = r.w = res->height;
            }, ops[0]);
			break;
		}
		case OPCODE_RESINFO: {
			SimulatorDXBC::Resource	*res = get_resource(ops[2]);
			switch (op->ResinfoReturn) {
				case RETTYPE_FLOAT:
					operate_vec(sat, [res](ThreadState &ts, float4p &r, uint32 mip) {
						int	dim = dimensions(res->dim);
						r.x = max(res->width >> mip, 1);
						r.y = dim > 1 ? max(res->height >> mip, 1) : 0;
						r.z = dim == 3 ? max(res->depth >> mip, 1) : is_array(res->dim) ? res->depth : 0;
						r.w = res->mips;
					}, ops[0], ops[1]);
					break;
				case RETTYPE_RCPFLOAT:
					operate_vec(sat, [res](ThreadState &ts, float4p &r, uint32 mip) {
						int	dim = dimensions(res->dim);
						r.x = 1.f / max(res->width >> mip, 1);
						r.y = dim > 1 ? 1.f / max(res->height >> mip, 1) : 0;
						r.z = dim == 3 ? 1.f / max(res->depth >> mip, 1) : is_array(res->dim) ? 1.f / res->depth : 0;
						r.w = 1.f / res->mips;
					}, ops[0], ops[1]);
					break;
				case RETTYPE_UINT:
					operate_vec(sat, [res](ThreadState &ts, point4 &r, uint32 mip) {
						int	dim = dimensions(res->dim);
						r.x = max(res->width >> mip, 1);
						r.y = dim > 1 ? max(res->height >> mip, 1) : 0;
						r.z = dim == 3 ? max(res->depth >> mip, 1) : is_array(res->dim) ? res->depth : 0;
						r.w = res->mips;
					}, ops[0], ops[1]);
					break;
			}
			break;
		}

// STREAM
//		case OPCODE_CUT:
//		case OPCODE_EMIT:
//		case OPCODE_EMITTHENCUT:
		case OPCODE_EMIT_STREAM: {
			Register	*p = (Register*)group_mem + streamptr++ * count_bits(output_mask);
			for (auto &t : active_threads) {
				Register	*p1 = p + threads.index_of(t) * count_bits(output_mask) * max_output;
				for (uint32 m = output_mask; m; m = clear_lowest(m))
					*p1++ = t.regs[output_offset + lowest_set_index(m)];
			};
			break;
		}
		case OPCODE_CUT_STREAM:
			break;
		case OPCODE_EMITTHENCUT_STREAM:
			break;

// CUSTOM
		case OPCODE_CUSTOMDATA: {
			CustomDataClass customClass = (CustomDataClass)op->custom.Class;
			uint32 customDataLength = *p++;			// DWORD length including token0 and this length token
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

					for (uint32 i = 0; i < numOperands; i++) {
						ASMOperand	operand(p);
					}
					break;
				}
			}
			break;
		}

		default:
			ISO_ASSERT2(0, "unsupported");
			break;
	}
	return op->next();
}

//-----------------------------------------------------------------------------
//	SimulatorDXBC::Resource
//-----------------------------------------------------------------------------

void SimulatorDXBC::Resource::init(ResourceDimension _dim, DXGI_COMPONENTS _format, uint32 _width, uint32 _height, uint32 _depth, uint32 _mips) {
	dim		= _dim;
	format	= _format;
	width	= _width;
	height	= _height;
	depth	= _depth;
	mips	= _mips ? _mips : log2(max(_width, _height));

	switch (dim) {
		case RESOURCE_DIMENSION_BUFFER:
		case RESOURCE_DIMENSION_RAW_BUFFER:
		case RESOURCE_DIMENSION_STRUCTURED_BUFFER:
			n	= width * height;
			return;
		case RESOURCE_DIMENSION_TEXTURE1D:
			n	= size1D(format, width, mips);
			break;
		case RESOURCE_DIMENSION_TEXTURE2D:
			n	= size2D(format, width, height, mips);
			break;
		case RESOURCE_DIMENSION_TEXTURE3D:
			n	= size3D(format, width, height, depth, mips);
			break;
		case RESOURCE_DIMENSION_TEXTURE1DARRAY:
			n	= size1D(format, width, mips) * depth;
			break;
		case RESOURCE_DIMENSION_TEXTURE2DARRAY:
		case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
		case RESOURCE_DIMENSION_TEXTURECUBE:
			n	= size2D(format, width, height, mips) * depth;
			break;
//		case RESOURCE_DIMENSION_TEXTURE2DMS:
//		case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
		default:
			break;
	}
	n -= dxgi_padding(mip_stride(format, width, mips - 1));
}

void SimulatorDXBC::Resource::set_mips(uint32 first, uint32 num) {
	size_t	offset, size2;
	switch (dim) {
		case RESOURCE_DIMENSION_TEXTURE1D:
			offset	= size1D(format, width, first);
			width	= mip_size(format, width, first);
			size2	= size1D(format, width, num);
			break;
		case RESOURCE_DIMENSION_TEXTURE2D:
			offset	= size2D(format, width, height, first);
			width	= mip_size(format, width, first);
			height	= mip_size(format, height, first);
			size2	= size2D(format, width, height, num);
			break;
		case RESOURCE_DIMENSION_TEXTURE3D:
			offset	= size3D(format, width, height, depth, first);
			width	= mip_size(format, width, first);
			height	= mip_size(format, height, first);
			depth	= max(depth >> first, 1);
			size2	= size3D(format, width, height, depth, num);
			break;
		case RESOURCE_DIMENSION_TEXTURE1DARRAY:
			offset	= size1D(format, width, mips) * depth;
			width	= mip_size(format, width, first);
			size2	= size1D(format, width, num);
			break;
		case RESOURCE_DIMENSION_TEXTURE2DARRAY:
		case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
		case RESOURCE_DIMENSION_TEXTURECUBE:
			offset	= size2D(format, width, height, first) * depth;
			width	= mip_size(format, width, first);
			height	= mip_size(format, height, first);
			size2	= size2D(format, width, height, num);
			break;
//		case RESOURCE_DIMENSION_TEXTURE2DMS:
//		case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
		default:
			return;
	}

	p	= (uint8*)p + offset;
	n	= size2 - dxgi_padding(mip_stride(format, width, num - 1));
	mips	= num;
}

void SimulatorDXBC::Resource::set_slices(uint32 first, uint32 num) {
	size_t	stride2	= size2D(format, width, height, 1);
	p	= (uint8*)p + stride2 * first;
	n	= stride2 * num  - dxgi_padding(mip_stride(format, width, mips - 1));
	depth	= num;
}

void SimulatorDXBC::Resource::set_sub(uint32 sub) {
	uint32	mip		= sub % mips;
	uint32	slice	= (sub / mips) % depth;
	set_mips(mip, 1);
	set_slices(slice, 1);
}

//-----------------------------------------------------------------------------
//	DeclReader
//-----------------------------------------------------------------------------

const Opcode *DeclReader::Process(const Opcode *op) {
	if (Decls::Process(op))
		return op->next();

	const uint32	*p		= op->operands();
	switch (op->Op()) {
		case OPCODE_DCL_CONSTANT_BUFFER:
			cb.emplace_back(ASMOperand(p), op->AccessPattern);
			break;

		case OPCODE_DCL_INPUT:
			inputs.emplace_back(ASMOperand(p));
			break;

		case OPCODE_DCL_INDEXABLE_TEMP: {
			uint32	tempReg		= *p++;
			uint32	numTemps	= *p++;
			uint32	tempComponentCount = *p++;
			break;
		}
		case OPCODE_DCL_OUTPUT:
			outputs.emplace_back(ASMOperand(p));
			break;

		case OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
			max_output = *p++;
			break;

		case OPCODE_DCL_INPUT_SIV:
		case OPCODE_DCL_INPUT_SGV:
		case OPCODE_DCL_INPUT_PS_SIV:
		case OPCODE_DCL_INPUT_PS_SGV: {
			ASMOperand operand(p);
			inputs.emplace_back(operand, (SystemValue)*p++);
			break;
		}
		case OPCODE_DCL_OUTPUT_SIV:
		case OPCODE_DCL_OUTPUT_SGV: {
			ASMOperand operand(p);
			SystemValue	sv = (SystemValue)*p++;
			new (outputs) InputOutput(operand, (SystemValue)*p++);
			break;
		}
		case OPCODE_DCL_STREAM: {
			ASMOperand operand(p);
			break;
		}
		case OPCODE_DCL_SAMPLER:
			smp.emplace_back(ASMOperand(p), (SamplerMode)op->SamplerMode);
			break;
		case OPCODE_DCL_RESOURCE: {
			ASMOperand operand(p);
			srv.emplace_back(operand, (ResourceDimension)op->ResourceDim, ResourceReturnType(*p++), op->SampleCount);
			break;
		}
		case OPCODE_DCL_INPUT_PS: {
			ASMOperand operand(p);
			auto	&i	= inputs.emplace_back(operand);
			i.interp	= op->InterpolationMode;
			break;
		}
		case OPCODE_DCL_INDEX_RANGE: {
			ASMOperand operand(p);
			uint32	i = *p++;
			break;
		}
		case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW: {
			ASMOperand operand(p);
			uint32	i = *p++;
			break;
		}
		case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
			ASMOperand operand(p);
			uint32	stride	= *p++;
			uint32	count	= *p++;
			break;
		}
		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
			uav.emplace_back(ASMOperand(p), RESOURCE_DIMENSION_UNKNOWN, 0, 0);
			break;

		case OPCODE_DCL_RESOURCE_RAW:
			srv.emplace_back(ASMOperand(p), RESOURCE_DIMENSION_UNKNOWN, 0, 0);
			break;

		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
			ASMOperand operand(p);
			uav.emplace_back(operand, RESOURCE_DIMENSION_UNKNOWN, 0, *p++);
			break;
		}
		case OPCODE_DCL_RESOURCE_STRUCTURED: {
			ASMOperand operand(p);
			srv.emplace_back(operand, RESOURCE_DIMENSION_UNKNOWN, 0, *p++);
			break;
		}
		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED: {
			ASMOperand operand(p);
			uav.emplace_back(operand, (ResourceDimension)op->ResourceDim, ResourceReturnType(*p++), op->SampleCount);
			break;
		}
		case OPCODE_DCL_FUNCTION_BODY: {
			uint32	i = *p++;
			break;
		}
		case OPCODE_DCL_FUNCTION_TABLE: {
			uint32 functionTable	= *p++;
			uint32 TableLength		= *p++;
			for (uint32 i = 0; i < TableLength; i++) {
				uint32	j = *p++;
			}
			break;
		}
		case OPCODE_DCL_INTERFACE: {
			uint32	interfaceID = *p++;
			uint32	numTypes	= *p++;
			DeclarationCount CountToken = (DeclarationCount&)*p++;
			break;
		}
		case OPCODE_HS_DECLS:
			break;

		case OPCODE_CUSTOMDATA: {
			CustomDataClass customClass = (CustomDataClass)op->custom.Class;
			uint32 customDataLength = *p++;			// DWORD length including token0 and this length token
			ISO_ASSERT(customDataLength >= 2);

			switch (customClass) {
				case CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER: {
					uint32 dataLength = customDataLength - 2;
					ISO_ASSERT(dataLength % 4 == 0);
					break;
				}

				default:
					return 0;
			}
			break;
		}
		default:
			return 0;
	}
	return op->next();
}

DeclReader::DeclReader(const memory_block &ucode) {
	for (const Opcode *op = ucode; op;)
		op = Process(op);
}

