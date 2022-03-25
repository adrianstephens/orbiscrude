#include "hook_stack.h"
#include "base/atomic.h"
#include "base/strings.h"
#include "base/skip_list.h"
#include "base/tree.h"
#include "windows/registry.h"

#pragma comment(lib, "dbghelp")

using namespace iso;

//-----------------------------------------------------------------------------
//	CallStacks
//-----------------------------------------------------------------------------

CallStacks::CallStacks() {
	current_process	= GetCurrentProcess();
	current_thread	= GetCurrentThread();
	reinterpret_cast<void*&>(CaptureStackBackTrace) = ::GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlCaptureStackBackTrace");
}

uint32 CallStacks::GetStackTrace(const context& ctx, void **addr, uint32 maxdepth) {
	uint32		count = 0;
	if (ctx.func)
		addr[count++] = ctx.func;

#if defined(_M_IX86)
	for (void **fp = ctx.fp; count < maxdepth; fp = (void**)*fp) {
		if (*fp < fp) {
			if (*fp != NULL)
				return 0;
		}
		if (uintptr_t(*fp) & (sizeof(void*) - 1))
			return 0;

		addr[count++] = fp[1];
	}
#elif defined(_M_X64)
	uint32	maxframes	= maxdepth + 10;
	void	**temp		= alloc_auto(void*, maxframes);
//	void	*temp[64];
	maxframes	= CaptureStackBackTrace(0, maxframes, temp, NULL);

	uint32	start = 0;
	for (uint32 i = 0; i < maxframes; i++) {
		if (temp[i] == ctx.fp[1]) {
			start = i;
			break;
		}
	}
	maxframes = min(maxframes, start + maxdepth - count);
	while (start < maxframes)
		addr[count++] = temp[start++];
#endif
	return count;
}
uint32 CallStacks::GetStackTraceSlow(const context& ctx, void **addr, uint32 maxdepth) {
	UINT32		count = 0;
	if (ctx.func)
		addr[count++] = ctx.func;

	CONTEXT			currentContext;
	STACKFRAME64	frame;
	clear(currentContext);
	clear(frame);

#if defined(_M_IX86)
	DWORD		architecture = IMAGE_FILE_MACHINE_I386;
	frame.AddrStack.Offset	= currentContext.Esp = (uintptr_t)ctx.sp;
	frame.AddrFrame.Offset	= currentContext.Ebp = (uintptr_t)ctx.fp;
	frame.AddrPC.Offset		= currentContext.Eip = (uintptr_t)ctx.pc;
#elif defined(_M_X64)
	DWORD		architecture = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrStack.Offset	= currentContext.Rsp = (uintptr_t)ctx.sp;
	frame.AddrFrame.Offset	= currentContext.Rbp = (uintptr_t)ctx.fp;
	frame.AddrPC.Offset		= currentContext.Rip = (uintptr_t)ctx.pc;
#endif

	frame.AddrPC.Mode	= frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
	frame.Virtual		= TRUE;

	// Walk the stack.
	auto	cs = with(stackWalkLock);
	while (count < maxdepth && StackWalk64(architecture, current_process, current_thread, &frame, &currentContext, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL) && frame.AddrFrame.Offset != 0)
		addr[count++] = (void*)frame.AddrPC.Offset;

	return count;
}

void **CallStacks::FindEnd(void **addr, uint32 maxdepth) {
	void	**end = addr + maxdepth;
	while (addr < end && *addr)
		++addr;
	return addr;
}

int	CallStacks::FindDivergence(void **a, void **aend, void **b, void **bend) {
	while (--aend > a && --bend > b && *aend == *bend)
		;
	return aend - a;
}

//-----------------------------------------------------------------------------
//	CompressedCallStacks
//-----------------------------------------------------------------------------

CompressedCallStacks::stack_node* CompressedCallStacks::Compress(void **addr, int len) {
	stack_node			*node	= 0;
	stack_node			*prev	= 0;
#if 1
	e_rbtree<stack_node>	*s	= &stacks;
	for (int i = len; i--;) {
		void	*a	= addr[i];
		auto	j	= lower_boundc(*s, a);
		if (!j || a < *j)
			s->insert(j, node = new stack_node(a, prev));
		else
			node	= j;
		prev	= node;
		s		= &node->next;
	}
#endif
	return node;
}

int CompressedCallStacks::Decompress(stack_node *node, void **addr, int maxlen) const {
	void	**p = addr, **e = p + maxlen;
	while (node && p < e) {
		*p++ = node->addr;
		node = node->prev;
	}
	return p - addr;
};

