#if USE_PROFILER

#include "profiler.h"
#include "tweak.h"
#include "base/algorithm.h"

#if defined(PLAT_WII)
  #include <wiiprofiler.h>
#endif

namespace iso {

Profiler	*Profiler::instance;
Profiler	profiler;

#ifdef USE_TUNER
//-----------------------------------------------------------------------------
//	SN Tuner
//-----------------------------------------------------------------------------
Profiler::Profiler()		{ instance = this; }
Profiler::~Profiler()		{}
void Profiler::EndFrame()	{}

#elif defined(USE_TELEMETRY)
//-----------------------------------------------------------------------------
//	RAD Telemetry
//-----------------------------------------------------------------------------
void*		Profiler::tm_arena;
HTELEMETRY	Profiler::tm_context;
const unsigned int TM_ARENA_SIZE = 2 * 1024 * 1024; // How much memory you want Telemetry to use

Profiler::Profiler() {
	instance	= this;

	tmLoadTelemetry(1);
	tmStartup();
	tm_arena	= malloc(TM_ARENA_SIZE);
	tmInitializeContext(&tm_context, tm_arena, TM_ARENA_SIZE);

	if (tmOpen(tm_context, "Hill", __DATE__ " " __TIME__, "AdrianMacMini", TMCT_TCP, TELEMETRY_DEFAULT_PORT, TMOF_DEFAULT, 1000) != TM_OK)
		ISO_TRACE("Telemetry couldn't connect");
}
Profiler::~Profiler() {
	tmClose(tm_context);
	tmShutdownContext(tm_context);
	tmShutdown();
	free(tm_arena);
}
void Profiler::EndFrame() {
	tmTick(tm_context);
}
#else
//-----------------------------------------------------------------------------
//	ISO Profiler
//-----------------------------------------------------------------------------
bool		enable_profiler		= false;
TWICK(bool, PROFILER_ENABLE, enable_profiler);
TWICK(float, PROFILER_FPS, profiler.fps, 0.0f, 1000.0f);
TWICK(int, PROFILER_AVERAGING, profiler.frame_averaging, 1, 60);

Profiler::Marker* Profiler::Marker::AddChild(const char *label) {
	for (iterator i = children.begin(); i != children.end(); ++i) {
		if (i->label == label) {
			i->num_calls++;
			return i;
		}
	}
	Marker *m = new Marker(label);
	children.push_back(m);
	return m;
}

void Profiler::Marker::UpdateTotal(bool reset) {
	if (reset) {
		total		-= prev_total;
		num_calls	= 1;
	}
	prev_total		= total;
	for (iterator i = children.begin(); i != children.end(); ++i)
		i->UpdateTotal(reset);
}

Profiler::Profiler() : head("Head"), frame_index(0), frame_averaging(60), curr(&head) {
	instance = this;
}

Profiler::~Profiler() {}

void Profiler::EndFrame() {
	ISO_ASSERT2(curr == &head, "StartMarker unmatched by an EndMarker");
	time	now	= Time::get();

	if (enabled) {
		head.total		+= now - frame_starts[frame_index];
		fps				= 1.0f / Time::to_secs(head.total - head.prev_total);
		head.UpdateTotal(reset);
		reset			= false;
	}
	frame_index = (frame_index + 1) % frame_averaging;

	if (enabled = enable_profiler) { // needed, otherwise push-pop mis-alignment could occur from toggling this mid-frame
	#ifdef WIIPROFILER
		static const uint32	wii_bufsize	= 32*1024*1024;		//giving 32 megs to the profiler
		static void *profMem;
		if (!profMem) {
			PROFILE_MEM_EVENT("WIIPROFILER");
			WIIPROFILER_Init(profMem = wii_alloc2(wii_bufsize, 32), wii_bufsize, FALSE);
			WIIPROFILER_EnableDebugOutput();
		}
		WIIPROFILER_MarkFrameBegin();
	#endif
		head.num_calls++;
	} else {
		fps = float(frame_averaging) / Time::to_secs(now - frame_starts[frame_index]);
	}

	frame_starts[frame_index] = now;
}
#endif

} // namespace iso
#endif

#if USE_MEMPROFILER
//-----------------------------------------------------------------------------
//	MemProfiler
//-----------------------------------------------------------------------------

#include "list.h"
#include "thread.h"

namespace iso {

class MemProfiler {
	static MemProfiler	*instance;

public:
	struct Alloc : e_link<Alloc> {
		static uint32 next_inception;
		void*		p;
		uint32		size;
		uint32		inception;

		Alloc(void *_p, uint32 _size) : p(_p), size(_size), inception(next_inception++)	{}
		bool		InBank(int bank) const { return bank == -1 || bank == !!((uint32)p & 0x10000000); }
	};

	struct Marker : e_link<Marker> {
		const char		*label;
		e_list<Marker>	children;
		e_list<Alloc>	allocs;

		Marker(const char* _label) : label(_label)	{}
		Marker*		AddChild(MemProfiler *mp, const char *_label);
		uint32		TotalMem(int bank) const;

		void		alloc(void *p, uint32 size) { allocs.push_back(new Alloc(p, size));	}
		bool		free(void *p);
	} head;

	typedef		e_list<Marker>::iterator		iterator;
	typedef		e_list<Marker>::const_iterator	const_iterator;

	TLS<Marker*>	curr;
	int				overhead;

	MemProfiler(): overhead(0), head("Head")				{ instance = this; }

	Marker	*GetCurrMarker()								{ Marker *p = curr; return p ? p : &head;	}
	void	StartMarker(Marker *parent, const char *label)	{ curr = parent->AddChild(this, label);		}
	void	EndMarker(Marker *parent)						{ curr = parent;							}

	void	alloc(void *p, uint32 size)						{ if (p) { curr->alloc(p, size); overhead += sizeof(Alloc); } }
	void	free(void *p)									{ if (p) { ISO_VERIFY(head.free(p)); overhead -= sizeof(Alloc);	} }
	void	realloc(void *to, void *from, uint32 size)		{ free(from); alloc(to, size);			}
	void	Print(const Marker *m, int tab, int bank);
	void	DumpMemory();
} mem_profiler;

MemProfiler	*MemProfiler::instance;
uint32		MemProfiler::Alloc::next_inception;

void MemProfiler::Print(const Marker *m, int tab, int bank) {
	uint32 total_mem = m->TotalMem(bank);
	if (!total_mem)
		return;

	ISO_TRACEF() << rightjustify("", tab * 4) << m->label << ": " << rightjustify(separated_number(total_mem), 12) << '\n';
	for (const_iterator i = m->children.begin(); i != m->children.end(); ++i)
		Print(i, tab + 1, bank);
}

void MemProfiler::DumpMemory() {
	ISO_TRACE("Memory Allocation Records:\n");
	ISO_TRACEF("mem_overhead: ") << rightjustify(separated_number(overhead),12) << '\n';
#ifdef PLAT_WII
	ISO_TRACEF("free mem1: ") << rightjustify(separated_number(separated_number(0)),12)) << '\n';
	ISO_TRACEF("free mem2: ") << rightjustify(separated_number(separated_number(1)),12)) << '\n';
	ISO_TRACEF("largest free chunk mem1: ") << rightjustify(separated_number(GetFreeMemBlock(0)),12)) << '\n';
	ISO_TRACEF("largest free chunk mem2: ") << rightjustify(separated_number(GetFreeMemBlock(1)),12)) << '\n';
#endif
	Print(&head, 1, -1);
}

//-----------------------------------------------------------------------------
//	MemProfiler::Marker
//-----------------------------------------------------------------------------

MemProfiler::Marker* MemProfiler::Marker::AddChild(MemProfiler *mp, const char *label) {
	for (iterator i = children.begin(); i != children.end(); ++i) {
		if (i->label == label)
			return i;
	}
	Marker *m		= new Marker(label);
	mp->overhead	+= sizeof(Marker);
	children.push_back(m);
	return m;
}

bool MemProfiler::Marker::free(void *p) {
	for (e_list<Alloc>::iterator i = allocs.begin(); i != allocs.end(); ++i) {
		if (i->p == p) {
			delete i;
			#if 0
			if (allocs.empty() && parent) {
				delete this;
				instance->overhead -= sizeof(Marker) + sizeof(Alloc);
			}
			#endif
			return true;
		}
	}
	for (iterator i = children.begin(); i != children.end(); ++i) {
		if (i->free(p))
			return true;
	}
	return false;
}

uint32 MemProfiler::Marker::TotalMem(int bank) const {
	uint32 total = 0;
	for (e_list<Alloc>::const_iterator i = allocs.begin(); i != allocs.end(); ++i)
		if (i->InBank(bank))
			total += i->size;

	for (const_iterator i = children.begin(); i != children.end(); ++i)
		total += i->TotalMem(bank);

	return total;
}
//-----------------------------------------------------------------------------
//	ProfileMemEvent
//-----------------------------------------------------------------------------

ProfileMemEvent::ProfileMemEvent(const char *label) : parent(mem_profiler.GetCurrMarker()) {
	mem_profiler.StartMarker((MemProfiler::Marker*)parent, label);
}
ProfileMemEvent::~ProfileMemEvent() {
	mem_profiler.EndMarker((MemProfiler::Marker*)parent);
}
void ProfileMemEvent::Next(const char *label) {
	mem_profiler.EndMarker((MemProfiler::Marker*)parent);
	mem_profiler.StartMarker((MemProfiler::Marker*)parent, label);
}
void ProfileMemEvent::alloc(void *p, uint32 size) {
	mem_profiler.alloc(p, size);
}
void ProfileMemEvent::realloc(void *to, void *from, uint32 size) {
	mem_profiler.realloc(to, from, size);
}
void ProfileMemEvent::free(void *p) {
	mem_profiler.free(p);
}

} // namespace iso

#endif
