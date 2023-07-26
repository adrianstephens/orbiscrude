#ifndef SIM_DXBC_H
#define SIM_DXBC_H

#include "dx_sim.h"
#include "base/sparse_array.h"
#include "extra/memory_cache.h"

namespace iso { namespace dx {

//-----------------------------------------------------------------------------
//	SimulatorDXBC
//-----------------------------------------------------------------------------

uint3p	GetThreadGroupDXBC(const_memory_block ucode);
void	Read(DeclResources &decl, const_memory_block ucode);

class SimulatorDXBC : public SimulatorDX {
public:
	typedef float4p Register;

	struct ThreadState : e_link<ThreadState> {
		bool		carry, discarded;
		Register	*regs;
		void		discard()				{ discarded = true; unlink(); }
		void		reset(Register *_regs)	{ carry = discarded = false; regs = _regs; }//memset(r, 0xcd, sizeof(r)); }
	};
	
	sparse_array<Resource>					grp;

private:
	enum Phase {
		phNormal,
		phControlPoints,
		phConstantsFork,
		phConstantsJoin,
	};

	const_memory_block						ucode;
	malloc_block							group_mem;	//32-bit Thread Group Shared Memory (CS only); repurposed as streams
	Opcode									global_flags;
	uint32									streamptr;
	dynamic_array<ThreadState>				threads;
	dynamic_array<Register>					regs;
	e_list<ThreadState>						active_threads;
	dynamic_array<e_list<ThreadState> >		active_stack;
	uint32									input_offset, output_offset, const_offset, patch_offset;
	uint32									regs_per_thread	= 0;
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
	template<typename F, typename...OO> void operate(bool sat, F f, const ASMOperand& r1, const ASMOperand& r2, const OO&... oo) {
		operate(sat, f, max(r1.NumComponents(), r2.NumComponents()), r1, r2, oo...);
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
	const Opcode*	ProcessOp(const Opcode *op);
public:
	
	static const Register&	get_const(const ASMOperand &o);
	Resource&		get_resource(const ASMOperand &o);
	Sampler*		get_sampler(const ASMOperand &o);

	bool			IsDiscarded(int t)						const	{ return threads[t].discarded; }
	bool			IsActive(int t)							const	{ return active_threads.contains(&threads[t]); }
	int				reg(Operand::Type type, int index = 0)	const;
	int				reg(const ASMOperand &op)				const	{ return reg((Operand::Type)op.type, op.indices ? op.indices[0].index : 0); }
	const void*		reg_data(int thread, Operand::Type type, const range<const uint32*> offsets)	const;
	uint32			offset(ThreadState &ts, const ASMIndex &ix);
	Register&		ref(ThreadState &ts, const ASMOperand &o);
	Register&		ref(int t, const ASMOperand &o)					{ return ref(threads[t], o); }
	ThreadState&	other(ThreadState &ts, uint32 mask, uint32 add)	{ return threads[(threads.index_of(ts) & mask) | add]; }

	SimulatorDXBC() : regs_per_thread(0) {}
	SimulatorDXBC(const DXBC::UcodeHeader *header, const_memory_block ucode) : regs_per_thread(0) { Init(header, ucode); }
	SimulatorDXBC(const SimulatorDXBC&)	= delete;
	SimulatorDXBC(SimulatorDXBC&&)		= delete;

	void			Init(const DXBC::UcodeHeader *header, const_memory_block ucode);

//	void			Reset()										override;
	void			SetNumThreads(int num_threads)				override;
	const void*		Run(int max_steps = -1)						override;
	const void*		Continue(const void *op, int max_steps)		override;

	uint32			NumThreads()			const override	{ return threads.size32();  }
	uint64			Offset(const void *p)	const override	{ return (uint8*)p - ucode; }
	const_memory_block	GetUCode()			const override	{ return ucode; }

	range<stride_iterator<void>> _GetRegFile(Operand::Type type, int i = 0) const override;

	THREADFLAGS		ThreadFlags(int t)		const override;
};

} } //namespace iso::dx

#endif // SIM_DXBC_H
