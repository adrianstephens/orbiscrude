#ifndef DEBUG_H
#define DEBUG_H

#include "hook.h"
#include "base/tree.h"
#include "filename.h"
#include "thread.h"
#include "_debug.h"

#define STACK_COMPRESSION

namespace iso {

//-----------------------------------------------------------------------------
//	CallStack
//-----------------------------------------------------------------------------

struct CallStacks {
	static void**	FindEnd(void **addr, uint32 maxdepth);
	static int		FindDivergence(void **a, void **aend, void **b, void **bend);

public:
	CallStacks();

	template<int N> struct Stack {
		void	*stack[N];
		Stack() {}
		Stack(CallStacks &cs, const context &ctx) { init(cs, ctx); }
		void init(CallStacks &cs, const context &ctx) {
			int	len = backtrace(ctx, stack, N);
			if (len < N)
				stack[len] = 0;
		}
	};
};

//-----------------------------------------------------------------------------
//	CompressedCallStacks
//-----------------------------------------------------------------------------

struct CompressedCallStacks : CallStacks {
	struct stack_node : e_rbnode<stack_node, false> {
		void					*addr;
		stack_node				*prev;
		e_rbtree<stack_node>	next;
		operator void*() const { return addr; }
		stack_node(void *addr, stack_node *prev) : addr(addr), prev(prev) {}
	};
	
	e_rbtree<stack_node>	stacks;

	stack_node*		Compress(void **addr, int len);
	int				Decompress(stack_node *node, void **addr, int maxlen) const;

public:
	template<int N> struct Stack {
		stack_node	*stack;
		Stack() : stack(0) {}
		Stack(CompressedCallStacks &cs, const context &ctx) { init(cs, ctx); }
		void init(CompressedCallStacks &cs, const context &ctx) {
			void	*addr[N];
			int		len	= backtrace(ctx, addr, N);
			stack		= cs.Compress(addr, len);
		}
	};
};

//-----------------------------------------------------------------------------
//	Symbolicator
//-----------------------------------------------------------------------------

class Symbolicator : _Symbolicator {
	uint32		options;
	string		search_path;
	
public:
	enum {
		UNDNAME			= 1 << 0,
		DEFERRED_LOADS	= 1 << 1,
		LOAD_LINES		= 1 << 2,
	};

	struct Frame : _Symbolicator::Frame {
		Frame(uint64 pc, Symbolicator *syms) : _Symbolicator::Frame(pc, syms) {}
		friend string_accum &operator<<(string_accum &a, const Frame &f) { return f.Dump(a); }
	};


	Symbolicator(uint32 options = 0);
	Symbolicator(Module process, uint32 options, const char *search_path);

	void			SetOptions(uint32 options, const char *search_path);
	void			AddModule(const char *fn, uint64 a, uint64 b);
	void			AddModule(Module file, uint64 base);
	const char*		GetModule(uint64 addr);

	Frame			GetFrame(uint64 addr)	{ return {addr, this}; }
};

} //namespace iso

#endif // DEBUG_H
