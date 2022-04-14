#include "hook.h"
#include "debug.h"
#include "base/atomic.h"
#include "base/strings.h"
#include "base/skip_list.h"
#include "base/tree.h"
#include "thread.h"
#include "filename.h"

using namespace iso;

#ifdef _M_X64
#define ADDRESSFORMAT	"0x%I64x"	// Format string for 64-bit addresses
#else
#define ADDRESSFORMAT	"0x%.8X"	// Format string for 32-bit addresses
#endif

//-----------------------------------------------------------------------------
//	MallocHooks
//-----------------------------------------------------------------------------

#define STACK_COMPRESSION
struct patchentry {
	const char	*name;
	void		*dest;
};

struct Heap {
//	HANDLE	h
	void	*alloc(size_t size);
	void	free(void *p);
};

Module GetCurrentProcess();

struct MallocHooks {
	enum FLAGS {
		ENABLED				= 1 << 0,
		INITIALISED			= 1 << 1,
		OUTPUT_EVENTS		= 1 << 2,
		OUTPUT_ERRORS		= 1 << 3,
		DUMP_ALLOC_ALLOCD	= 1 << 4,
		DUMP_FREE_UNKNOWN	= 1 << 5,
		DUMP_FREE_FREED		= 1 << 6,
	};

	struct tls_data {
		bool			in_use;
		tls_data() : in_use(false) {}
		bool	use() {
			if (in_use)
				return false;
			return in_use = true;
		}
	};

	struct heap_val {
		static Heap	heap;
		void	*operator new(size_t size)	{ return heap.alloc(size); }
		void	operator delete(void *p)	{ heap.free(p); }
	};

	struct entry : heap_val {
		entry	*next;
		size_t	size;
		uint32	event;
#ifdef STACK_COMPRESSION
		CompressedCallStacks::Stack<64>	stack;
#else
		CallStacks::Stack<32>			stack;
#endif
		entry(entry *_next, uint32 _event, size_t _size) : next(_next), size(_size), event(_event) {}
		bool	is_free() const { return size == 0; }
	};
	
	struct entry_list : heap_val {
		entry	*last;
		entry_list() : last(0) {}
	};

	static MallocHooks		*me;
	static thread_local tls_data	tls;
	atomic<uint32>			event;
	uint32					dump_event;
#ifdef STACK_COMPRESSION
	CompressedCallStacks	cs;
#else
	CallStacks				cs;
#endif
	CallStackDumper			stack_dumper;
	skip_list<void*, entry_list>	allocs;
	CriticalSection			allocsLock;
	flags<FLAGS>			flags;

	struct tls_use {
		bool		ok;
		tls_use() : ok(tls.use() && me->flags.test(ENABLED))	{}
		~tls_use()					{ if (ok) tls.in_use = false; }
		operator bool() const		{ return ok; }
	};


	static const patchentry *msvc_patches[];
	string					msvc_dlls[8];

	void init(const patchentry *patches, size_t num, void **original, const char *module_name) {
		ISO_OUTPUTF("Adding allocation patches to ") << module_name << '\n';
	}
	void remove(const patchentry *patches, size_t num, void **original, const char *module_name) {
		ISO_OUTPUTF("Removing allocation patches from ") << module_name << '\n';
		for (; num--; patches++, original++)
			RemoveHook(original, patches->name, module_name, patches->dest);
	}

	template<typename T> void init(T &t, const char *module_name) {
		init(t.patches, sizeof(t) / sizeof(void*), (void**)&t, module_name);
	}
	template<typename T> void remove(T &t, const char *module_name) {
		remove(t.patches, sizeof(t) / sizeof(void*), (void**)&t, module_name);
	}

	uint32	next_event(context &ctx);
	
	void*	add_alloc(void *p, size_t size, context &ctx);
	void	add_free(void *p, context &ctx);
	void*	add_realloc(void *newp, void *oldp, size_t size, context &ctx);

	void	get_callstack(entry *e, context &ctx);
	void	dump_callstacks(const entry *e);
	void	dump_callstack(const entry *e);

	void	CheckHeap(Heap heap);
	void	CheckHeaps();
	size_t	GetHeapUsed(Heap heap);

	MallocHooks(uint32 _flags);
	~MallocHooks();
};

MallocHooks *dummy = new MallocHooks(MallocHooks::ENABLED);

MallocHooks*				MallocHooks::me;
thread_local MallocHooks::tls_data	MallocHooks::tls;
Heap						MallocHooks::heap_val::heap;

MallocHooks::MallocHooks(uint32 _flags) : flags(_flags), event(1), dump_event(0), stack_dumper(GetCurrentProcess(), SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES, BuildSymbolSearchPath()) {
	me = this;

	if (flags.test(ENABLED)) {
		flags.set(INITIALISED);
	}
}

MallocHooks::~MallocHooks() {
	tls.use();
	if (flags.test(INITIALISED)) {
	}
	me = 0;
}

size_t MallocHooks::GetHeapUsed(Heap heap) {
	size_t	used		= 0;
	return used;
}

void MallocHooks::CheckHeap(Heap heap) {
}

void MallocHooks::CheckHeaps() {
	tls_use	u;
}

void MallocHooks::get_callstack(entry *e, context &ctx) {
	e->stack.init(cs, ctx);
}

void MallocHooks::dump_callstack(const entry *e) {
	stack_dumper.Dump(cs, e->stack);
}

void MallocHooks::dump_callstacks(const entry *e) {
	if (e->next) {
		ISO_OUTPUT("previous callstack:\n");
		dump_callstack(e->next);
	}
	ISO_OUTPUT("current callstack:\n");
	dump_callstack(e);
}

uint32 MallocHooks::next_event(context &ctx) {
	uint32	i = event++;
	if (i == dump_event) {
		ISO_OUTPUTF("%i: dump event callstack:\n", i);
		const void	*callstack[32];
		stack_dumper.Dump(callstack, cs.GetStackTrace(ctx, callstack, 32));
	}
	return i;
}

void *MallocHooks::add_alloc(void *p, size_t size, context &ctx) {
	uint32	event = next_event(ctx);
	uint32	allocd;

	entry	*e	= 0;
	allocsLock.lock();
		entry_list	&r	= allocs[p];
		allocd		= r.last && !r.last->is_free() ? r.last->event : 0;
		r.last		= e = new entry(r.last, event, size);
	allocsLock.unlock();
	get_callstack(e, ctx);

//	if (flags.test(OUTPUT_EVENTS) || (allocd && flags.test(OUTPUT_ERRORS)))
//		ISO_OUTPUTF("(%i) %i: alloc: 0x%0I64x:0x%I64x", GetCurrentThreadId(), event, p, size)
//		<< onlyif(allocd, format_string(" - already allocated:  %i", allocd))
//		<< '\n';

	if (allocd && flags.test(DUMP_ALLOC_ALLOCD))
		dump_callstacks(e);

	return p;
}

void MallocHooks::add_free(void *p, context &ctx) {
	uint32	event = next_event(ctx);
	uint32	freed;
	entry	*e	= 0;

	allocsLock.lock();
		auto	i	= lower_boundc(allocs, p);
		if (i != allocs.end() && i.key() == p) {
			freed	= i->last && i->last->is_free() ? i->last->event : 0;
			i->last	= e = new entry(i->last, event, 0);
		} else {
			freed	= ~0;
			allocs[p].last = e	= new entry(0, event, 0);
		}
	allocsLock.unlock();
	get_callstack(e, ctx);

//	if (flags.test(OUTPUT_EVENTS) || (freed && flags.test(OUTPUT_ERRORS)))
//		ISO_OUTPUTF("(%i) %i: free: 0x%0I64x", GetCurrentThreadId(), event, p)
//		<< onlyif(freed && ~freed, format_string(" - already freed:  %i", freed))
//		<< onlyif(!~freed, " - unallocated")
//		<< '\n';

	if (freed && flags.test(!~freed ? DUMP_FREE_UNKNOWN : DUMP_FREE_FREED))
		dump_callstacks(e);
}

void *MallocHooks::add_realloc(void *newp, void *oldp, size_t size, context &ctx) {
	uint32	event = next_event(ctx);
	uint32	freed = 0, allocd = 0;
	entry	*e1	= 0;
	entry	*e2	= 0;
	
	allocsLock.lock();
		if (oldp) {
			auto	i = lower_boundc(allocs, oldp);
			if (i != allocs.end() && i.key() == oldp) {
				freed	= i->last && i->last->is_free() ? i->last->event : 0;
				i->last	= e1 = new entry(i->last, event, 0);
			} else {
				freed	= ~0;
				allocs[oldp].last	= e1 = new entry(0, event, 0);
			}
		}
		if (newp) {
			entry_list	&r	= allocs[newp];
			allocd		= r.last && !r.last->is_free() ? r.last->event : 0;
			r.last		= e2 = new entry(r.last, event, size);
		}
	allocsLock.unlock();
	if (oldp)
		get_callstack(e1, ctx);
	if (newp)
		get_callstack(e2, ctx);

//	if (flags.test(OUTPUT_EVENTS) || ((freed || allocd) && flags.test(OUTPUT_ERRORS)))
//		ISO_OUTPUTF("(%i) %i: realloc: 0x%0I64x -> 0x%0I64x", GetCurrentThreadId(), event, oldp, newp)
//		<< onlyif(freed && ~freed, format_string(" - already freed:  %i", freed))
//		<< onlyif(!~freed, " - unallocated")
//		<< onlyif(allocd, format_string(" - already allocated:  %i", allocd))
//		<< '\n';
	
	if (freed && flags.test(!~freed ? DUMP_FREE_UNKNOWN : DUMP_FREE_FREED))
		dump_callstacks(e1);

	if (allocd && flags.test(DUMP_ALLOC_ALLOCD))
		dump_callstacks(e2);

	return newp;
}


namespace iso {
bool MallocTracking(bool enable) {
	return MallocHooks::me && MallocHooks::me->flags.test_set(MallocHooks::ENABLED, enable);
}
}
