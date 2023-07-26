#ifndef EVENTS_H
#define EVENTS_H

#include "base/functions.h"
#include "base/list.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Events
//-----------------------------------------------------------------------------

// Event: list of (void returning) callbacks

const int8 EVENT_PRIORITY_MIN		= -128;
const int8 EVENT_PRIORITY_LOW		= -1;
const int8 EVENT_PRIORITY_NORMAL	= 0;
const int8 EVENT_PRIORITY_HIGH		= 1;
const int8 EVENT_PRIORITY_MAX		= 127;

template<class F> class EventP {
public:
	struct handler : e_link<handler>, callback<F> {
		int8 priority;
		template<class T> handler(T *t, int8 priority = EVENT_PRIORITY_NORMAL) : callback<F>(t), priority(priority) {}
	};
	e_list<handler>	list;

	void add(handler &link) {
		auto i = list.begin();
		while (i != list.end() && link.priority <= i->priority)
			++i;
		i->insert_before(&link);
	}

	template<typename...P> void go(P&&...p) {
		for (auto i = list.begin(); i != list.end(); )
			(*i++)(forward<P>(p)...);
	}

	template<class F, typename...P> void priority_go(F &&pred, P&&...p) {
		for (auto i = list.begin(), j; i != list.end(); ) {
			 if (pred((j = i++)->priority))
				(*j)(forward<P>(p)...);
		}
	}
};

// Message/Handles:

template<class M> struct Message {
	typedef EventP<void(M*)>	E;
	typedef typename E::handler	handler;
	static	E&		get_event()			{ static E e; return e;	}
	static	void	add(handler &h)		{ get_event().add(h);	}
	static	void	remove(handler &h)	{ h.unlink();			}
	
	void			send()				{ get_event().go((M*)this);	}
};

template<class T, class M, int8 P=EVENT_PRIORITY_NORMAL> class Handles2 {
	typename M::handler	h;
public:
	Handles2() : h(static_cast<T*>(this), P) { M::add(h); }
	void	add()		{ M::add(h);	}
	void	remove()	{ M::remove(h); }
};

//-----------------------------------------------------------------------------
//	AppEvent
//-----------------------------------------------------------------------------

struct AppEvent : Message<AppEvent> {
	enum STATE {PRE_GRAPHICS, BEGIN, END, UPDATE, RENDER};
	STATE	state;
	AppEvent(STATE state) : state(state) {}
};

}// namespace iso

#endif	// EVENTS_H
