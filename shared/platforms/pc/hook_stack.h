#ifndef HOOK_STACK_H
#define HOOK_STACK_H

#include "hook.h"
#include "base/tree.h"
#include "dbghelp.h"
#include <intrin.h>

#undef ADDRESS
#define STACK_COMPRESSION

namespace iso {

//-----------------------------------------------------------------------------
//	context
//-----------------------------------------------------------------------------

struct context {
	void		*func;
	void		**fp;
	void		*sp;
	void		*pc;

#if defined(_M_IX86)
	context(void *_func, void **_fp = (void**)_AddressOfReturnAddress() - 1) : func(_func), fp(_fp) {
		sp		= *fp - maxdepth * 10 * sizeof(void*);	// An approximation.
		pc		= fp[1];
		CONTEXT c;
	}
#elif defined(_M_X64)
	context(void *_func, void **_fp = (void**)_AddressOfReturnAddress() - 1) : func(_func), fp(_fp) {
		CONTEXT c;
		RtlCaptureContext(&c);
		sp		= (void*)c.Rsp;
		pc		= (void*)c.Rip;
	}
#endif
	void *ret() const { return fp[1]; }
};

//-----------------------------------------------------------------------------
//	CallStack
//-----------------------------------------------------------------------------

struct CallStacks {
	CriticalSection	stackWalkLock;

	HANDLE			current_process, current_thread;
	USHORT			(WINAPI *CaptureStackBackTrace)(ULONG FramesToSkip, ULONG FramesToCapture, PVOID* BackTrace, PULONG BackTraceHash);

	uint32			GetStackTrace(const context& ctx, void **addr, uint32 maxdepth);
	uint32			GetStackTraceSlow(const context& ctx, void **addr, uint32 maxdepth);

	static void**	FindEnd(void **addr, uint32 maxdepth);
	static int		FindDivergence(void **a, void **aend, void **b, void **bend);

public:
	CallStacks();

	template<int N> struct Stack {
		void	*stack[N];
		Stack() {}
		Stack(CallStacks &cs, const context &ctx) { init(cs, ctx); }
		void init(CallStacks &cs, const context &ctx) {
			int	len = cs.GetStackTrace(ctx, stack, N);
			if (len < N)
				stack[len] = 0;
		}
	};
};

//-----------------------------------------------------------------------------
//	CompressedCallStacks
//-----------------------------------------------------------------------------

struct CompressedCallStacks : CallStacks {
#if 1
	struct stack_node : e_rbnode<stack_node, false> {
		void					*addr;
		stack_node				*prev;
		e_rbtree<stack_node>	next;
		operator void*() const { return addr; }
		stack_node(void *addr, stack_node *prev) : addr(addr), prev(prev) {}
	};
	
	e_rbtree<stack_node>	stacks;
#else
	struct stack_node {
		void					*addr;
		stack_node				*prev;
	};

#endif
	stack_node*		Compress(void **addr, int len);
	int				Decompress(stack_node *node, void **addr, int maxlen) const;

public:
	template<int N> struct Stack {
		stack_node	*stack;
		Stack() : stack(0) {}
		Stack(CompressedCallStacks &cs, const context &ctx) { init(cs, ctx); }
		void init(CompressedCallStacks &cs, const context &ctx) {
			void	*addr[N];
			int		len	= cs.GetStackTrace(ctx, addr, N);
			stack		= cs.Compress(addr, len);
		}
	};
};

} //namespace iso
#endif //HOOK_STACK_H

