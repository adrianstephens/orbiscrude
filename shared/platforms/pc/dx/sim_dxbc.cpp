#include "sim_dxbc.h"
#include "dxgi_read.h"
#include "base/maths.h"
#include "base/soft_float.h"

using namespace iso;
using namespace dx;

//-----------------------------------------------------------------------------
//	DeclResources
//-----------------------------------------------------------------------------

static bool Process(Decls &d, const Opcode *op, bool const_phase) {
	auto			code	= op->Op();
	const uint32	*p		= op->operands();

	switch (code) {
		default: return false;

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
				case Operand::TYPE_INPUT_DOMAIN_POINT:					i = Decls::vDomain; break;
				case Operand::TYPE_INPUT_PRIMITIVEID:					i = Decls::vPrim; break;
				case Operand::TYPE_INPUT_COVERAGE_MASK:					i = Decls::vCoverage; break;
				case Operand::TYPE_INPUT_GS_INSTANCE_ID:				i = Decls::vInstanceID; break;
				case Operand::TYPE_INPUT_THREAD_ID:						i = Decls::vThreadID; break;
				case Operand::TYPE_INPUT_THREAD_GROUP_ID:				i = Decls::vThreadGroupID; break;
				case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			i = Decls::vThreadIDInGroup; break;
				case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	i = Decls::vThreadIDInGroup; break;// + 3;
				case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				i = Decls::vForkInstanceID; break;
				case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				i = Decls::vJoinInstanceID; break;
				case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				i = Decls::vOutputControlPointID; break;
				case Operand::TYPE_INPUT_CONTROL_POINT:
				case Operand::TYPE_OUTPUT_CONTROL_POINT:
					d.patch_mask |= bits64(r.indices[0].index / d.num_input, r.indices[1].index / d.num_input);
				default:
					return true;
			}
			if (r.indices.size() == 2)
				d.input_mask |= bits64(i, r.indices[1].index);
			else
				d.input_mask |= bit64(i);
			break;
		}

		case OPCODE_DCL_OUTPUT:
		case OPCODE_DCL_OUTPUT_SIV:
		case OPCODE_DCL_OUTPUT_SGV: {
			ASMOperand	r(p);
			int i;
			switch (r.type) {
				case Operand::TYPE_OUTPUT:								i = r.indices[0].index; break;
				case Operand::TYPE_OUTPUT_DEPTH:						//i = Decls::oDepth; break;
				case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				//i = Decls::oDepthLessEqual; break;
				case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			i = Decls::oDepth; break;//i = Decls::oDepthGreaterEqual; break;
				case Operand::TYPE_OUTPUT_COVERAGE_MASK:				i = Decls::oCoverage; break;
				default:
					return true;
			}
			if (const_phase)
				d.const_output_mask |= bit64(i);
			else
				d.output_mask |= bit64(i);
			break;
		}
		case OPCODE_DCL_TEMPS:							d.num_temp 				= *p;	break;
		case OPCODE_DCL_INDEXABLE_TEMP: {
			uint32	i		= p[0];
			uint32	num		= p[1];
			uint32	comps	= p[2];
			d.num_index += num;
			break;
		}
		case OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:		d.max_output 			= *p;	break;
		case OPCODE_DCL_THREAD_GROUP:					d.thread_group			= *(uint3p*)p;	break;
		case OPCODE_DCL_INPUT_CONTROL_POINT_COUNT:		d.num_input				= op->ControlPointCount;							break;
		case OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT:		d.max_output			= op->ControlPointCount;							break;
		case OPCODE_DCL_TESS_DOMAIN:					d.tess_domain			= (TessellatorDomain)op->TessDomain;				break;
		case OPCODE_DCL_TESS_PARTITIONING:				d.tess_partitioning		= (TessellatorPartitioning)op->TessPartitioning;	break;
		case OPCODE_DCL_GS_INPUT_PRIMITIVE:				d.input_prim			= (PrimitiveType)op->InputPrimitive;				break;
		case OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:	d.output_topology		= (PrimitiveTopology)op->OutputPrimitiveTopology;	break;
		case OPCODE_DCL_TESS_OUTPUT_PRIMITIVE:			d.tess_output			= (TessellatorOutputPrimitive)op->OutputPrimitive;	break;
		case OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:	d.forks					+= *p;	break;
		case OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:	d.joins					+= *p;	break;
		case OPCODE_DCL_GS_INSTANCE_COUNT:				d.instances				+= *p;	break;
		case OPCODE_DCL_HS_MAX_TESSFACTOR:				d.max_tesselation		= *p;	break;
	}
	return true;
}

uint3p dx::GetThreadGroupDXBC(const_memory_block ucode) {
	for (const Opcode *op = ucode; op < ucode.end(); op = op->next()) {
		if (op->Op() == OPCODE_DCL_THREAD_GROUP)
			return *(uint3p*)op->operands();
	}
	return zero;
}

const Opcode *Process(DeclResources &d, const Opcode *op) {
	const uint32	*p	= op->operands();
	switch (op->Op()) {

		//input

		case OPCODE_DCL_INPUT: {
			ASMOperand		a(p);
			d.inputs.emplace(a.indices ? a.indices[0].index : a.type, uint8(a.num_components), uint8(a.swizzle_bits));
			break;
		}
		case OPCODE_DCL_INPUT_SIV:
		case OPCODE_DCL_INPUT_SGV:
		case OPCODE_DCL_INPUT_PS_SIV:
		case OPCODE_DCL_INPUT_PS_SGV: {
			ASMOperand		a(p);
			d.inputs.emplace(a.indices ? a.indices[0].index : a.type, uint8(a.num_components), uint8(a.swizzle_bits), 0, (SystemValue)*p++);
			break;
		}
		case OPCODE_DCL_INPUT_PS: {
			ASMOperand		a(p);
			d.inputs.emplace(a.indices ? a.indices[0].index : a.type, uint8(a.num_components), uint8(a.swizzle_bits), op->InterpolationMode);
			break;
		}

		//output

		case OPCODE_DCL_OUTPUT: {
			ASMOperand		a(p);
			d.outputs.emplace(a.indices ? a.indices[0].index : a.type, uint8(a.num_components), uint8(a.swizzle_bits));
			break;
		}
		
		case OPCODE_DCL_OUTPUT_SIV:
		case OPCODE_DCL_OUTPUT_SGV: {
			ASMOperand		a(p);
			p++;
			d.outputs.emplace(a.indices ? a.indices[0].index : a.type, uint8(a.num_components), uint8(a.swizzle_bits), 0, (SystemValue)*p++);
			break;
		}

		case OPCODE_DCL_STREAM: {
			ASMOperand a(p);
			break;
		}

		//resources

		case OPCODE_DCL_SAMPLER: {
			ASMOperand		a(p);
			d.smp.emplace(a.indices[0].index, (SamplerMode)op->SamplerMode);
			break;
		}
		case OPCODE_DCL_CONSTANT_BUFFER: {
			ASMOperand		a(p);
			d.cb.emplace(a.indices[0].index, a.indices[1].index, op->AccessPattern);
			break;
		}
		case OPCODE_DCL_RESOURCE: {
			ASMOperand		a(p);
			d.srv.emplace(a.indices[0].index, (ResourceDimension)op->ResourceDim, ResourceReturnType(*p++), op->SampleCount);
			break;
		}
		case OPCODE_DCL_INDEX_RANGE: {
			ASMOperand a(p);
			uint32	i = *p++;
			break;
		}
		case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW: {
			ASMOperand a(p);
			uint32	i = *p++;
			break;
		}
		case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
			ASMOperand a(p);
			uint32	stride	= *p++;
			uint32	count	= *p++;
			break;
		}
		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW: {
			ASMOperand a(p);
			d.uav.emplace(a.indices[0].index, RESOURCE_DIMENSION_UNKNOWN, 0, 0);
			break;
		}
		case OPCODE_DCL_RESOURCE_RAW: {
			ASMOperand a(p);
			d.srv.emplace(a.indices[0].index, RESOURCE_DIMENSION_UNKNOWN, 0, 0);
			break;
		}
		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
			ASMOperand a(p);
			d.uav.emplace(a.indices[0].index, RESOURCE_DIMENSION_UNKNOWN, 0, *p++);
			break;
		}
		case OPCODE_DCL_RESOURCE_STRUCTURED: {
			ASMOperand a(p);
			d.srv.emplace(a.indices[0].index, RESOURCE_DIMENSION_UNKNOWN, 0, *p++);
			break;
		}
		case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED: {
			ASMOperand a(p);
			d.uav.emplace(a.indices[0].index, (ResourceDimension)op->ResourceDim, ResourceReturnType(*p++), op->SampleCount);
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
			//if (!Process((Decls&)d, op, false))
				return 0;
	}
	return op->next();
}

void dx::Read(DeclResources &decl, const_memory_block ucode) {
	for (const Opcode *op = ucode; op < ucode.end(); op = op->next())
		Process(decl, op);
}

//-----------------------------------------------------------------------------
//	SimulatorDXBC
//-----------------------------------------------------------------------------

SimulatorDXBC::Register	dummy;

void SimulatorDXBC::Init(const DXBC::UcodeHeader *header, const_memory_block _ucode) {
	stage		= conv<SHADERSTAGE>(header->ProgramType);
	ucode		= _ucode;
	phase		= phNormal;
	fork_count	= 0;

	dynamic_array<const Opcode*>	loop_stack;

	for (const Opcode *op = ucode; op < ucode.end(); op = op->next()) {
		if (Process(*(Decls*)this, op, phase == phConstantsFork || phase == phConstantsJoin))
			continue;

		const uint32	*p = op->operands();
		switch (op->Op()) {
			case OPCODE_DCL_GLOBAL_FLAGS:
				global_flags = *op;
				break;

			case OPCODE_DCL_STREAM:
				gmem_size += 4096 * sizeof(Register);
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
				gmem_size	+= i;
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
				gmem_size	+= res.n;
				break;
			}
		}
	}

	group_mem.resize(gmem_size);
	uint8	*g = group_mem;
	for (auto &i : grp) {
		i->p = g;
		g += i->n;
	}

	phase			= phNormal;
	input_offset	= num_temp + num_index;
	output_offset	= input_offset + highest_set_index(input_mask) + 1;
	regs_per_thread	= output_offset + highest_set_index(output_mask) + 1;
	SetNumThreads(threads.size32());
}

void SimulatorDXBC::SetNumThreads(int num_threads) {
	threads.resize(num_threads);
	
	const_offset	= regs_per_thread * num_threads;
	patch_offset	= const_offset + highest_set_index(const_output_mask) + 1;

	regs.resize(patch_offset + (highest_set_index(patch_mask) + 1) * num_input);
}

SimulatorDXBC::THREADFLAGS SimulatorDXBC::ThreadFlags(int t) const {
	return THREADFLAGS(IsActive(t) | (IsDiscarded(t) << 1));
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
		case Operand::TYPE_INPUT_COVERAGE_MASK:					return ts.regs[input_offset + vCoverage];
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
		//case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				return ts.regs[output_offset + oDepthLessEqual];
		//case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			return ts.regs[output_offset + oDepthGreaterEqual];
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				return ts.regs[output_offset + oCoverage];
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
		case Operand::TYPE_INPUT_COVERAGE_MASK:					r += input_offset + vCoverage; break;
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
		//case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				r += output_offset + oDepthLessEqual; break;
		//case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			r += output_offset + oDepthGreaterEqual; break;
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				r += output_offset + oCoverage; break;
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
		case Operand::TYPE_INPUT_COVERAGE_MASK:					return input_offset + vCoverage;
		case Operand::TYPE_INPUT_GS_INSTANCE_ID:				return input_offset + vInstanceID;
		case Operand::TYPE_INPUT_THREAD_ID:						return input_offset + vThreadID;
		case Operand::TYPE_INPUT_THREAD_GROUP_ID:				return input_offset + vThreadGroupID;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			return input_offset + vThreadIDInGroup;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	return input_offset + vThreadIDInGroup;// + 3;
		case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				return input_offset + vForkInstanceID;
		case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				return input_offset + vJoinInstanceID;
		case Operand::TYPE_OUTPUT_DEPTH:						return output_offset + oDepth;
		//case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				return output_offset + oDepthLessEqual;
		//case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			return output_offset + oDepthGreaterEqual;
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				return output_offset + oCoverage;
		case Operand::TYPE_NULL:
		default:												return -1;
	}
}

range<stride_iterator<void>> SimulatorDXBC::_GetRegFile(Operand::Type type, int i) const {
	auto	r		= regs.begin();
	size_t	stride	= regs_per_thread * sizeof(Register);
	size_t	num		= threads.size();

	switch (type) {
		case Operand::TYPE_OUTPUT:
			if (stage == GS) {
				r		= (Register*)group_mem + i;
				stride	= count_bits(output_mask) * sizeof(Register);
				num		*= max_output;
			} else {
				r += output_offset + i;
			}
			break;
		case Operand::TYPE_TEMP:								r += i; break;
		case Operand::TYPE_INDEXABLE_TEMP:						r += num_temp + i; break;
		case Operand::TYPE_INPUT:								r += input_offset + i; break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT_ID:				r += input_offset + vOutputControlPointID; break;
		case Operand::TYPE_INPUT_DOMAIN_POINT:					r += input_offset + vDomain; break;
		case Operand::TYPE_INPUT_PRIMITIVEID:					r += input_offset + vPrim; break;
		case Operand::TYPE_INPUT_COVERAGE_MASK:					r += input_offset + vCoverage; break;
		case Operand::TYPE_INPUT_GS_INSTANCE_ID:				r += input_offset + vInstanceID; break;
		case Operand::TYPE_INPUT_THREAD_ID:						r += input_offset + vThreadID; break;
		case Operand::TYPE_INPUT_THREAD_GROUP_ID:				r += input_offset + vThreadGroupID; break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP:			r += input_offset + vThreadIDInGroup; break;
		case Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:	r += input_offset + vThreadIDInGroup; break;// + 3;
		case Operand::TYPE_INPUT_FORK_INSTANCE_ID:				r += input_offset + vForkInstanceID; break;
		case Operand::TYPE_INPUT_JOIN_INSTANCE_ID:				r += input_offset + vJoinInstanceID; break;
		case Operand::TYPE_INPUT_PATCH_CONSTANT:				r += const_offset + i; stride = sizeof(Register); num = highest_set_index(const_output_mask) + 1; break;
		case Operand::TYPE_INPUT_CONTROL_POINT:					r += patch_offset + i; stride = sizeof(Register) * (highest_set_index(patch_mask) + 1); num = num_input; break;
		case Operand::TYPE_OUTPUT_CONTROL_POINT:				r += patch_offset + i; stride = sizeof(Register) * (highest_set_index(patch_mask) + 1); num = max_output; break;
		case Operand::TYPE_OUTPUT_DEPTH:						r += output_offset + oDepth; break;
//		case Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL:				r += output_offset + oDepthLessEqual; break;
//		case Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL:			r += output_offset + oDepthGreaterEqual; break;
		case Operand::TYPE_OUTPUT_COVERAGE_MASK:				r += output_offset + oCoverage; break;
		case Operand::TYPE_NULL:
		default:	return none;
	}

	return make_range_n(stride_iterator<void>((void*)r, stride), num);
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

Resource &SimulatorDXBC::get_resource(const ASMOperand &o) {
	switch (o.type) {
		case Operand::TYPE_RESOURCE:				return srv[o.indices[0].index];
		case Operand::TYPE_UNORDERED_ACCESS_VIEW:	return uav[o.indices[0].index];
//		case Operand::TYPE_RASTERIZER:				return rtv;
		case Operand::TYPE_THREAD_GROUP_SHARED_MEMORY: return grp[o.indices[0].index];
		default: {
			static Resource dummy;
			return dummy;
		}
	}
}
Sampler *SimulatorDXBC::get_sampler(const ASMOperand &o) {
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


const void *SimulatorDXBC::Continue(const void *p, int max_steps) {
	const Opcode	*op = (const Opcode*)p;

	while (op && op < ucode.end() && max_steps--)
		op = ProcessOp(op);

	return op;
}
const void *SimulatorDXBC::Run(int max_steps) {
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

	const Opcode	*op = ucode;
	while (op < ucode.end() && op->IsDeclaration())
		op = op->next();

	return Continue(op, max_steps);
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
			for (int i = 0; i < max_output && t < threads.end(); i++, t++) {
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
			auto&	res = get_resource(ops[0]);
			operate_vec(sat, [res](ThreadState &ts, array_vec<uint32, 4> i) mutable { uint32 x = res.counter++; i = x; }, ops[1]);
			break;
		}
		case OPCODE_IMM_ATOMIC_CONSUME: {
			auto&	res = get_resource(ops[0]);
			operate_vec(sat, [res](ThreadState &ts, array_vec<uint32, 4> i) mutable { uint32 x = --res.counter; i = x; }, ops[1]);
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
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float4p &r, float4p ddx, float4p ddy) {
				r.y = tex.biased_lod(samp, ddx.xyz, ddy.xyz); r.x = clamp(r.y, samp->min_lod, min(samp->max_lod, tex.max_lod)); r.z = r.w = 0;
			}, ops[0], ddx(ops[1]), ddy(ops[1]));
			break;
		}
		case OPCODE_SAMPLE: {
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy) {
				r = tex.samplef(samp, i.xyz, tex.biased_lod(samp, ddx.xyz, ddy.xyz));
			}, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]));
			break;
		}
		case OPCODE_SAMPLE_L: {		//lod
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float4p &r, float4p i, float lod) {
				r = tex.samplef(samp, i.xyz, lod);
			}, ops[0], ops[1], ops[2]);
			break;
		}
		case OPCODE_SAMPLE_B: {		//bias
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy, float bias) {
				r = tex.samplef(samp, i.xyz, tex.biased_lod(samp, ddx.xyz, ddy.xyz) + bias);
			}, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[4]);
			break;
		}
		case OPCODE_SAMPLE_D: {		//derivs
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy) {
				r = tex.samplef(samp, i.xyz, tex.biased_lod(samp, ddx.xyz, ddy.xyz));
			}, ops[0], ops[1], ops[4], ops[5]);
			break;
		}
		case OPCODE_SAMPLE_C: {		//comparison
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float &r, float4p i, float4p ddx, float4p ddy, float ref) {
				r = tex.samplec(samp, i.xyz, tex.biased_lod(samp, ddx.xyz, ddy.xyz), ref);
			}, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[4]);
			break;
		}
		case OPCODE_SAMPLE_C_LZ: {	//comparison, level 0
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float &r, float4p i, float ref) {
				r = tex.samplec(samp, i.xyz, 0, ref);
			}, ops[0], ops[1], ops[4]);
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
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy) {
				r = tex.gather4(samp, i.xyz, tex.biased_lod(samp, ddx.xyz, ddy.xyz));
			}, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]));
			break;
		}
		case OPCODE_GATHER4_C: {
			operate_vec(sat, [tex = TextureReader(get_resource(ops[2]), ops[2].swizzle_bits, tex_offset), samp = get_sampler(ops[3])](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy, float ref) {
				r = tex.gather4c(samp, i.xyz, tex.biased_lod(samp, ddx.xyz, ddy.xyz), ref);
			}, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[4]);
			break;
		}
		case OPCODE_GATHER4_PO: {
			operate_vec(sat, [tex = TextureReader(get_resource(ops[3]), ops[3].swizzle_bits, tex_offset), samp = get_sampler(ops[4])](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy, array_vec<int32, 4> offset) {
				uint32	ix	= uint32(sampler_address(i.x * tex.width  + offset.x - .5f, tex.width,  samp->address_u));
				uint32	iy	= uint32(sampler_address(i.y * tex.height + offset.y - .5f, tex.height, samp->address_v));
				uint32	iz	= uint32(sampler_address(i.z * tex.depth  + offset.z - .5f, tex.depth,  samp->address_w));
				uint32	im	= uint32(tex.clamped_lod(samp, ddx.xyz, ddy.xyz));
				r.x = tex.index(ix, iy, iz, im);
				r.y = tex.index(ix + 1, iy, iz, im);
				r.z = tex.index(ix, iy + 1, iz, im);
				r.w = tex.index(ix + 1, iy + 1, iz, im);
			}, ops[0], ops[1], ddx(ops[1]), ddy(ops[1]), ops[2]);
			break;
		}
		case OPCODE_GATHER4_PO_C: {
			operate_vec(sat, [tex = TextureReader(get_resource(ops[3]), ops[3].swizzle_bits, tex_offset), samp = get_sampler(ops[4])](ThreadState &ts, float4p &r, float4p i, float4p ddx, float4p ddy, array_vec<int32, 4> offset, float ref) {
				Sampler::Comparer	c(samp->comparison_func, ref);
				uint32	ix	= uint32(sampler_address(i.x * tex.width  + offset.x - .5f, tex.width,  samp->address_u));
				uint32	iy	= uint32(sampler_address(i.y * tex.height + offset.y - .5f, tex.height, samp->address_v));
				uint32	iz	= uint32(sampler_address(i.z * tex.depth  + offset.z - .5f, tex.depth,  samp->address_w));
				uint32	im	= uint32(tex.clamped_lod(samp, ddx.xyz, ddy.xyz));
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
			operate_vec(sat, [res = get_resource(ops[1])](ThreadState &ts, point4 &r) {
				r.x = r.y = r.z = r.w = res.height;
            }, ops[0]);
			break;
		}
		case OPCODE_RESINFO: {
			Resource	&res = get_resource(ops[2]);
			switch (op->ResinfoReturn) {
				case RETTYPE_FLOAT:
					operate_vec(sat, [res](ThreadState &ts, float4p &r, uint32 mip) {
						int	dim = dimensions(res.dim);
						r.x = max(res.width >> mip, 1);
						r.y = dim > 1 ? max(res.height >> mip, 1) : 0;
						r.z = dim == 3 ? max(res.depth >> mip, 1) : is_array(res.dim) ? res.depth : 0;
						r.w = res.mips;
					}, ops[0], ops[1]);
					break;
				case RETTYPE_RCPFLOAT:
					operate_vec(sat, [res](ThreadState &ts, float4p &r, uint32 mip) {
						int	dim = dimensions(res.dim);
						r.x = 1.f / max(res.width >> mip, 1);
						r.y = dim > 1 ? 1.f / max(res.height >> mip, 1) : 0;
						r.z = dim == 3 ? 1.f / max(res.depth >> mip, 1) : is_array(res.dim) ? 1.f / res.depth : 0;
						r.w = 1.f / res.mips;
					}, ops[0], ops[1]);
					break;
				case RETTYPE_UINT:
					operate_vec(sat, [res](ThreadState &ts, point4 &r, uint32 mip) {
						int	dim = dimensions(res.dim);
						r.x = max(res.width >> mip, 1);
						r.y = dim > 1 ? max(res.height >> mip, 1) : 0;
						r.z = dim == 3 ? max(res.depth >> mip, 1) : is_array(res.dim) ? res.depth : 0;
						r.w = res.mips;
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
