#ifndef SIM_DXBC_H
#define SIM_DXBC_H

#include "dx_opcodes.h"
#include "dx_shaders.h"
#include "dxgi_helpers.h"
#include "base/hash.h"
#include "base/list.h"
#include "base/algorithm.h"
#include "extra/memory_cache.h"

namespace iso { namespace dx {

struct Decls {

	enum {
		vSpecial					= 32,

		//GS, HS, DS
		vPrim						= vSpecial + 0,
		//PS
		vMask						= vSpecial + 0,
		vInnerCoverage				= vSpecial + 1,
		//GS
		vInstanceID					= vSpecial + 1,
		//HS
		vOutputControlPointID		= vSpecial + 0,
		vForkInstanceID				= vSpecial + 1,
		vJoinInstanceID				= vSpecial + 2,
		//DS
		vDomain						= vSpecial + 1,
		//	vcp*32
		//	vpc*32
		//CS
		vThreadID					= vSpecial + 1,
		vThreadGroupID				= vSpecial + 2,
		vThreadIDInGroup			= vSpecial + 3,
		//vThreadIDInGroupFlattened	= vSpecial + 4,

		//PS
		oDepth						= 9,
		oDepthLessEqual				= 10,
		oDepthGreaterEqual			= 11,
		oMask						= 12,
		oStencilRef					= 13,
	};

	Opcode						global_flags;
	uint32						num_temp;
	uint32						num_index;
	uint3p						thread_group;
	uint32						input_control_points;
	uint32						output_control_points;
	uint32						max_output;
	uint32						max_tesselation;
	uint32						forks, joins, instances;
	PrimitiveType				gs_input;
	PrimitiveTopology			gs_output;
	TessellatorDomain			tess_domain;
	TessellatorPartitioning		tess_partitioning;
	TessellatorOutputPrimitive	tess_output;

	Decls() { clear(*this); }
	Decls(const memory_block &ucode);

	bool	Process(const Opcode *op);
	uint32	NumTemps()					const { return num_temp; }
	uint32	NumInputControlPoints()		const { return input_control_points; }
	uint32	NumOutputControlPoints()	const { return output_control_points; }
	uint32	MaxOutput()					const { return max_output; }
};

struct RegSpec {
	Operand::Type	type;
	int				index;
};

//-----------------------------------------------------------------------------
//	SimulatorDXBC
//-----------------------------------------------------------------------------

class SimulatorDXBC : public Decls {
public:
	typedef float4p Register;

	struct ThreadState : e_link<ThreadState> {
		bool		carry, discarded;
		Register	*regs;
		void		discard()				{ discarded = true; unlink(); }
		void		reset(Register *_regs)	{ carry = discarded = false; regs = _regs; }//memset(r, 0xcd, sizeof(r)); }
	};

private:
	enum Phase {
		phNormal,
		phControlPoints,
		phConstantsFork,
		phConstantsJoin,
	};


	memory_block	ucode;
	uint64			ucode_addr;
	malloc_block	group_mem;			//32-bit Thread Group Shared Memory (CS only); repurposed as streams
	uint32			streamptr;
	uint64			input_mask, output_mask, const_output_mask, patch_mask;
	uint32			input_offset, output_offset, regs_per_thread, const_offset, patch_offset;

	dynamic_array<ThreadState>				threads;
	dynamic_array<Register>					regs;
	e_list<ThreadState>						active_threads;
	dynamic_array<e_list<ThreadState> >		active_stack;
	Phase									phase;
	int										fork_count;
	const_memory_block						constants;

	hash_map<const Opcode*,const Opcode*>	loop_hash;
	hash_map<uint32,const Opcode*>			labels;
	hash_map<uint32,const uint32*>			function_tables;
	hash_map<uint32,const uint32*>			interface_tables;
	dynamic_array<const Opcode*>			call_stack;

	bool	split_exec(const ASMOperand &o, bool nz, e_list<ThreadState> &suspended);
	bool	split_exec(const ASMOperand &a, const ASMOperand &b, e_list<ThreadState> &suspended);

	template<typename F, typename...OO> void operate(bool sat, F f, int nc, const OO&... oo);
	template<typename F, typename...OO> void operate(bool sat, F f, const ASMOperand& r, const OO&... oo) {
		operate(sat, f, r.NumComponents(), r, oo...);
	}
	template<typename F, typename...OO> void operate_vec(bool sat, F f, const ASMOperand &r, const OO&... oo);
	template<typename F> void atomic_op(bool sat, ASMOperand *ops, uint32 stride, F f) {
		RawTextureReader	tex(get_resource(ops[0]), stride);
		operate_vec(sat, [&tex,f](ThreadState &ts, array_vec<uint32, 4> i, uint32 v) {
			f(*(uint32*)tex.index(i.x, i.y, i.z), v);
		}, ops[1], ops[2]);
	}
	template<typename F> void atomic_imm_op(bool sat, ASMOperand *ops, uint32 stride, F f) {
		RawTextureReader	tex(get_resource(ops[1]), stride);
		operate_vec(sat, [&tex,f](ThreadState &ts, uint32 &prev, array_vec<uint32, 4> i, uint32 v) {
			uint32	*p = (uint32*)tex.index(i.x, i.y, i.z);
			prev = *p;
			f(*p, v);
		}, ops[0], ops[2], ops[3]);
	}
public:
	struct Buffer : memory_block {
		Buffer()						{}
		Buffer(const memory_block &mem) : memory_block(mem)	{}
		void	set_mem(const memory_block &mem){ memory_block::operator=(mem); }
		memory_block	data()	const			{ return *this; }
	};
	struct Resource : Buffer {
		ResourceDimension	dim;
//		ResourceReturnType	type;
		DXGI_COMPONENTS		format;
		uint32				width, height, depth, mips;
		uint32				counter;
		Resource() : width(1), height(1), depth(1), mips(1), counter(0) {}
		void		init(ResourceDimension _dim, DXGI_COMPONENTS _format, uint32 _width, uint32 _height, uint32 _depth = 0, uint32 _mips = 0);
		void		offset(size_t o)					{ p = (uint8*)p + o; }
		void		set_slices(uint32 first, uint32 num);
		void		set_mips(uint32 first, uint32 num);
		void		set_sub(uint32 sub);
		uint32		num_subs()	{
			switch (dim) {
				default:										return 1;
				case RESOURCE_DIMENSION_TEXTURE1D:
				case RESOURCE_DIMENSION_TEXTURE2D:
				case RESOURCE_DIMENSION_TEXTURE2DMS:
				case RESOURCE_DIMENSION_TEXTURE3D:				return mips;
				case RESOURCE_DIMENSION_TEXTURECUBE:			return mips * 6;
				case RESOURCE_DIMENSION_TEXTURE1DARRAY:			return mips * depth;
				case RESOURCE_DIMENSION_TEXTURE2DARRAY:
				case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:		return mips * depth;
				case RESOURCE_DIMENSION_TEXTURECUBEARRAY:		return mips * depth * 6;
			}
		}
	};
	struct Sampler {
		TextureFilterMode	filter;
		TextureAddressMode	address_u, address_v, address_w;
		float				minlod, maxlod, bias;
		ComparisonFunction	comparison;
		float				border[4];
	};
	struct Triangle {
		float4	x, y, z;
		Triangle(param(float4) a, param(float4) b, param(float4) c) : x(b - a), y(c - a), z(a)	{}
		float4		interpolate(float u, float v)	const	{ return x * u + y * v + z;	}
		float4		interpolate(float2 uv)			const	{ return x * uv.x + y * uv.y + z;	}
	};

	hash_map<int, Buffer>	cbv;
	hash_map<int, Resource>	srv;
	hash_map<int, Resource>	uav;
	hash_map<int, Sampler>	smp;
	hash_map<int, Resource>	grp;
	
	static const Register&	get_const(const ASMOperand &o);
	static Operand::Type input_type(int &v, SHADERSTAGE stage) {
		switch (v) {
			default:		return Operand::TYPE_INPUT;
			case vSpecial + 0:
				switch (stage) {
					case PS: v = 0; return Operand::TYPE_INPUT_COVERAGE_MASK;
					case HS: v = 0; return Operand::TYPE_OUTPUT_CONTROL_POINT;
					default: return Operand::TYPE_INPUT_PRIMITIVEID;
				}

			case vSpecial + 1:
				switch (stage) {
					case PS: v = 0; return Operand::TYPE_INNER_COVERAGE;
					case GS: v = 0; return Operand::TYPE_INPUT_FORK_INSTANCE_ID;
					case HS: v = 0; return Operand::TYPE_OUTPUT_CONTROL_POINT;
					case DS: v = 0; return Operand::TYPE_INPUT_DOMAIN_POINT;
					case CS: v = -5; return Operand::TYPE_INPUT_THREAD_ID;
					default: ISO_ASSERT(0); return Operand::TYPE_TEMP;
				}
			case vSpecial + 2:
				switch (stage) {
					case HS: v = 0; return Operand::TYPE_INPUT_JOIN_INSTANCE_ID;
					case CS: v = -7; return Operand::TYPE_INPUT_THREAD_GROUP_ID;
					default: ISO_ASSERT(0); return Operand::TYPE_TEMP;
				}
			case vSpecial + 3:
				switch (stage) {
					case CS: v = -6; return Operand::TYPE_INPUT_THREAD_ID_IN_GROUP;
					default: ISO_ASSERT(0); return Operand::TYPE_TEMP;
		//			case vThreadIDInGroup:	return Operand::TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED;
				}
		}
	}
	static Operand::Type output_type(int v) {
		switch (v) {
			default:				return Operand::TYPE_OUTPUT;
			case oDepth:			return Operand::TYPE_OUTPUT_DEPTH;
			case oDepthLessEqual:	return Operand::TYPE_OUTPUT_DEPTH_LESS_EQUAL;
			case oDepthGreaterEqual:return Operand::TYPE_OUTPUT_DEPTH_GREATER_EQUAL;
			case oMask:				return Operand::TYPE_OUTPUT_COVERAGE_MASK;
		}
	}

	Resource*		get_resource(const ASMOperand &o);
	Sampler*		get_sampler(const ASMOperand &o);

	int				reg(Operand::Type type, int index = 0)	const;
	int				reg(const ASMOperand &op)				const	{ return reg((Operand::Type)op.type, op.indices ? op.indices[0].index : 0); }
	uint32			offset(ThreadState &ts, const ASMIndex &ix);
	Register&		ref(ThreadState &ts, const ASMOperand &o);
	const void*		reg_data(int thread, Operand::Type type, const range<const uint32*> offsets)	const;
	Register&		ref(int t, const ASMOperand &o)					{ return ref(threads[t], o); }
	ThreadState&	other(ThreadState &ts, uint32 mask, uint32 add)	{ return threads[(threads.index_of(ts) & mask) | add]; }

	SimulatorDXBC() : regs_per_thread(0) {
	}
	SimulatorDXBC(int num_threads) : regs_per_thread(0) {
		SetNumThreads(num_threads);
	}
	//SimulatorDXBC(const memory_block &_ucode, uint64 _ucode_addr, int num_threads) : regs_per_thread(0) {
	//	SetNumThreads(num_threads);
	//}

	SimulatorDXBC(const SimulatorDXBC&);
	SimulatorDXBC(SimulatorDXBC&&);
	
	void			Init(const memory_block &_ucode, uint64 _ucode_addr);
	void			SetNumThreads(int num_threads);
	const Opcode*	ProcessOp(const Opcode *op);
	const Opcode*	Run(int max_steps = -1);
	void			Reset();

	range<stride_iterator<const Register>>	GetRegFile(Operand::Type type, int i = 0)	const;//	{ return make_range_n(strided(regs + reg(type, i), regs_per_thread * sizeof(Register)), threads.size()); }
	auto			GetRegFile(Operand::Type type, int i = 0)			{ return element_cast<Register>(make_const(this)->GetRegFile(type, i)); }
	auto			GetScalarRegFile(Operand::Type type, int i = 0)		{ return element_cast<int>(GetRegFile(type, i)); }

	auto			GetStreamFile(int index, int i)				const	{ return make_range_n(strided((Register*)group_mem + index * count_bits(output_mask) + i, max_output * count_bits(output_mask) * sizeof(Register)), threads.size()); }
	auto			GetStreamFileAll(int i)						const	{ return make_range_n(strided((Register*)group_mem + i, count_bits(output_mask) * sizeof(Register)), max_output * threads.size()); }
	
	template<typename I> void SetRegFile(Operand::Type type, int v, I i)		{ copy(make_range_n(i, threads.size()), GetRegFile(type, v)); }
	template<typename I> void SetScalarRegFile(Operand::Type type, int v, I i)	{ copy(make_range_n(i, threads.size()), GetScalarRegFile(type, v)); }
	void			InterpolateInputQuad(int t, int r, const Triangle &tri, float u0, float v0, float u1, float v1);
	void			InterpolateInputQuad(int t, int r, const Triangle &tri, float2 uv0, float2 uv1, float2 uv2);

	uint32			NumThreads()				const { return threads.size32(); }
	bool			IsDiscarded(int t)			const { return threads[t].discarded; }
	bool			IsActive(int t)				const { return active_threads.contains(&threads[t]); }
	bool			HasInput(int i)				const { return input_mask & bit64(i); }
	bool			HasPatchInput(int i)		const { return patch_mask & bit64(i); }
	uint64			InputMask()					const { return input_mask; }
	uint64			OutputMask()				const { return output_mask; }
	uint64			PatchConstOutputMask()		const { return const_output_mask; }
	uint64			ControlPointMask()			const { return patch_mask; }
	const Opcode*	Begin()						const { return ucode; }
	uint64			Offset(const Opcode *p)		const { return (uint8*)p - ucode; }
	uint64			Address(const Opcode *p)	const { return p ? ucode_addr + Offset(p) : 0; }
	const Opcode*	OffsetToOp(uint32 p)		const { return ucode + p; }
	const Opcode*	AddressToOp(uint64 p)		const { return ucode + (p - ucode_addr); }
	uint32			AddressToOffset(uint64 p)	const { return uint32(p - ucode_addr); }

	void			SetPatchInput(int i)		{ patch_mask |= bit64(i); }

};

//-----------------------------------------------------------------------------
//	DeclReader
//-----------------------------------------------------------------------------

struct DeclReader : Decls {
	struct InputOutput {
		uint32				index;
		uint16				system;
		uint8				num_comps, swizzle, interp;
		InputOutput(const ASMOperand &op, SystemValue _system = SV_UNDEFINED) : system(_system), num_comps(op.num_components) {
			index	= op.indices ? op.indices[0].index : op.type;
			swizzle = op.swizzle_bits;
			interp	= 0;
		}
	};

	struct ConstantBuffer {
		uint32				index;
		uint32				size:31, dynamic:1;
		ConstantBuffer(const ASMOperand &op, bool _dynamic) : index(op.indices[0].index), size(op.indices[1].index), dynamic(_dynamic) {}
	};
	struct Resource {
		int					index;
		ResourceDimension	dim;
		ResourceReturnType	type;
		uint32				stride;	//or samples
		Resource(const ASMOperand &op, ResourceDimension _dim, ResourceReturnType _type, uint32 _stride) : index(op.indices[0].index), dim(_dim), type(_type), stride(_stride) {}
	};
	struct Sampler {
		uint32				index;
		SamplerMode			mode;
		Sampler(const ASMOperand &op, SamplerMode mode) : index(op.indices[0].index), mode(mode) {}
	};

	dynamic_array<InputOutput>		inputs;
	dynamic_array<InputOutput>		outputs;
	dynamic_array<ConstantBuffer>	cb;
	dynamic_array<Resource>			srv;
	dynamic_array<Resource>			uav;
	dynamic_array<Sampler>			smp;
	dynamic_array<Resource>			grp;

	const Opcode*	Process(const Opcode *op);
	
	DeclReader(const memory_block &_ucode);
};

} } //namespace iso::dx

#endif // SIM_DXBC_H
