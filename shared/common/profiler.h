#ifndef	PROFILER_H
#define PROFILER_H

#ifndef USE_PROFILER
#define USE_PROFILER	0
#endif
#ifndef USE_MEMPROFILER
#define USE_MEMPROFILER	0
#endif

#include "base/list.h"
#include "thread.h"

#ifdef USE_TUNER
	#include <libsntuner.h>
#endif

#ifdef USE_TELEMETRY
	#include "TelemetrySDKiOS/include/telemetry.h"
#endif

namespace iso {

extern bool enable_profiler;
class GraphicsContext;

class ProfileEvent {
public:
	ProfileEvent(const char *label, bool cond = true)	{}
	void	Next(const char *label, bool cond = true)	{}
};

#ifdef GRAPHICS_H
struct AlwaysProfileGpuEvent {
	GraphicsContext &_ctx;
	AlwaysProfileGpuEvent(GraphicsContext &ctx, const char *label) : _ctx(ctx)	{ ctx.PushMarker(label); }
	~AlwaysProfileGpuEvent()			{ _ctx.PopMarker(); }
	void Next(const char *label)		{ _ctx.PopMarker(); _ctx.PushMarker(label); }
};
#endif

#if USE_PROFILER	//USE_PROFILER
#define ON_PROFILER(x)		x
#define ON_PROFILER_STMT(x)	x

class Profiler {
public:
	static Profiler	*instance;
	typedef	time::type	time;

	#ifdef USE_TUNER
	class Event {
		bool	cond;
	public:
		Event(const char *label)				{ cond = true; snPushMarker(label);			}
		Event(const char *label, bool _cond)	{ if (cond = _cond) snPushMarker(label);	}
		~Event()								{ if (cond) snPopMarker();					}
		void Next(const char *label)			{ if (cond)	snPopMarker(); cond = true; snPushMarker(label);		}
		void Next(const char *label, bool _cond){ if (cond)	snPopMarker(); if (cond = _cond) snPushMarker(label);	}
	};
	#elif defined(USE_TELEMETRY)
	static void*		tm_arena;
	static HTELEMETRY	tm_context;

	class Event {
		bool	cond;
	public:
		Event(const char *label)				{ cond = true; tmEnter(tm_context, TMZF_NONE, label);		}
		Event(const char *label, bool _cond)	{ if (cond = _cond) tmEnter(tm_context, TMZF_NONE, label);	}
		~Event()								{ if (cond) tmLeave(tm_context);							}
		void Next(const char *label)			{ if (cond)	tmLeave(tm_context); cond = true; tmEnter(tm_context, TMZF_NONE, label);		}
		void Next(const char *label, bool _cond){ if (cond)	tmLeave(tm_context); if (cond = _cond) tmEnter(tm_context, TMZF_NONE, label);	}
	};
	#else
	bool 		enabled, reset;
	int			frame_index;
	int			frame_averaging;
	time		frame_starts[60];
	float		fps;

	struct Marker : e_link<Marker> {
		const char*		label;
		struct colour {uint8 r,g,b,a;} col;
		e_list<Marker>	children;
		int				num_calls;
		time			total, prev_total;

		const char*	Label()			const	{ return label;				}
		int			NumCalls()		const	{ return num_calls;			}
		time		Total()			const	{ return total - prev_total;}
		time		RunningTotal()	const	{ return total;				}

		Marker(const char *_label) : label(_label), prev_total(0), total(0), num_calls(1) { clear(col); }
		Marker*		AddChild(const char *_label);
		void		StopTimer(time start, time end)	{ total += end - start;	}
		void		UpdateTotal(bool reset);
	} head, *curr;

	typedef		e_list<Marker>::iterator		iterator;
	typedef		e_list<Marker>::const_iterator	const_iterator;

	Marker	*GetCurrMarker()		const	{ return curr; }
	inline time	StartMarker(Marker *parent, const char* label) {
		if (enabled) {
			curr = parent->AddChild(label);
			return time::now();
		}
		return 0;
	}
	inline void	EndMarker(Marker *parent, time start) {
		if (enabled) {
			curr->StopTimer(start, time::now());
			curr = parent;
		}
	}

	class Event {
		Marker	*parent;
		time	start;
		ISO_ON_DEBUG(Marker *curr);
	public:
		Event(const char *label) : parent(instance->GetCurrMarker()) {
			start	= instance->StartMarker(parent, label);
			ISO_ON_DEBUG(curr = instance->GetCurrMarker());
		}
		Event(const char *label, bool cond) : parent(instance->GetCurrMarker()) {
			start	= cond ? instance->StartMarker(parent, label) : 0;
			ISO_ON_DEBUG(curr = instance->GetCurrMarker());
		}
		~Event() {
			ISO_ON_DEBUG(ISO_ASSERT(curr == instance->GetCurrMarker()));
			if (start)
				instance->EndMarker(parent, start);
		}
		void Next(const char *label) {
			ISO_ON_DEBUG(ISO_ASSERT(curr == instance->GetCurrMarker()));
			if (start)
				instance->EndMarker(parent, start);
			start = instance->StartMarker(parent, label);
			ISO_ON_DEBUG(curr = instance->GetCurrMarker());
		}
		void Next(const char *label, bool cond) {
			ISO_ON_DEBUG(ISO_ASSERT(curr == instance->GetCurrMarker()));
			if (start)
				instance->EndMarker(parent, start);
			start = cond ? instance->StartMarker(parent, label) : 0;
			ISO_ON_DEBUG(curr = instance->GetCurrMarker());
		}
	};

	void		Reset()					{ reset = true;				}
	int			TotalFrames()	const	{ return head.num_calls;	}
	float		Fps()			const	{ return fps;				}
#endif
	Profiler();
	~Profiler();
	void		EndFrame();
};

#ifdef GRAPHICS_H
typedef AlwaysProfileGpuEvent ProfileGpuEvent;

struct ProfileCpuGpuEvent : Profiler::Event, ProfileGpuEvent {
	ProfileCpuGpuEvent(GraphicsContext &ctx, const char *label) : Profiler::Event(label), ProfileGpuEvent(ctx, label)	{}
	void Next(const char *label)		{ Profiler::Event::Next(label); ProfileGpuEvent::Next(label); }
};
#endif

#else			//!USE_PROFILER
#define ON_PROFILER(x)
#define ON_PROFILER_STMT(x)	((void)0)

#ifdef GRAPHICS_H
struct ProfileGpuEvent {
	ProfileGpuEvent(GraphicsContext &ctx, const char *label)	{}
	void Next(const char *label)								{}
};
typedef ProfileGpuEvent ProfileCpuGpuEvent;
#endif

#endif

#define PROFILER_END_FRAME()						ON_PROFILER(Profiler::instance->EndFrame())
#define PROFILE_CPU_EVENT(label)					ON_PROFILER(Profiler::Event _cpu_event(label))
#define PROFILE_CPU_EVENT_NEXT(label)				ON_PROFILER(_cpu_event.Next(label))
#define PROFILE_CPU_EVENT_COND(label, cond)			ON_PROFILER(Profiler::Event _cpu_event(label, cond))
#define PROFILE_CPU_EVENT_COND_NEXT(label, cond)	ON_PROFILER(_cpu_event.Next(label, cond))
#define PROFILE_FN									ON_PROFILER(Profiler::Event _cpu_event(__func__);)
#define PROFILE_STMT(label)							ON_PROFILER_STMT(Profiler::Event(label))

#ifdef USE_TELEMETRY
#define PROFILE_DYNAMIC(label)						ON_PROFILER(tmDynamicString(Profiler::tm_context, label))
#else
#define PROFILE_DYNAMIC(label)						ON_PROFILER((const char*)(label))
#endif

#define PROFILE_GPU_EVENT(ctx, label)				ON_PROFILER(ProfileGpuEvent _gpu_event(ctx, label))
#define PROFILE_GPU_EVENT_NEXT(label)				ON_PROFILER(_gpu_event.Next(label))

//---------------------------------------------------

#if USE_MEMPROFILER
#define ON_MEMPROFILER(x)	x
class ProfileMemEvent {
	void	*parent;
public:
	static void	alloc(void *ptr, uint32 size);
	static void	realloc(void *to, void *from, uint32 size);
	static void	free(void *ptr);

	ProfileMemEvent(const char *_label);
	~ProfileMemEvent();
	void Next(const char *_label);
};
#else
#define ON_MEMPROFILER(x)
typedef ProfileEvent ProfileMemEvent;
#endif

#define PROFILE_MEM_EVENT(label)					ON_MEMPROFILER(ProfileMemEvent _mem_event(label))
#define PROFILE_MEM_EVENT_NEXT(label)				ON_MEMPROFILER(_mem_event.Next(label))
#define MEM_PROFILER_ALLOC(ptr, size)				ON_MEMPROFILER(ProfileMemEvent::alloc(ptr, size))
#define MEM_PROFILER_REALLOC(pto, pfrom, size)		ON_MEMPROFILER(ProfileMemEvent::realloc(pto, pfrom, size))
#define MEM_PROFILER_FREE(ptr)						ON_MEMPROFILER(ProfileMemEvent::free(ptr))
#define MEM_PROFILER_DUMP_MEMORY()					ON_MEMPROFILER(ProfileMemEvent::DumpMemory())

//---------------------------------------------------

#define PROFILE_EVENT(ctx, label)	PROFILE_GPU_EVENT(ctx, label); PROFILE_CPU_EVENT(label); PROFILE_MEM_EVENT(label)
#define PROFILE_EVENT_NEXT(label)	PROFILE_GPU_EVENT_NEXT(label); PROFILE_CPU_EVENT_NEXT(label); PROFILE_MEM_EVENT_NEXT(label)
} // namespace iso

#endif//PROFILER_H
