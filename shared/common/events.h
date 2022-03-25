#ifndef EVENTS_H
#define EVENTS_H

#include "base/functions.h"
#include "base/list.h"

namespace iso {

//-----------------------------------------------------------------------------
//	Events
//-----------------------------------------------------------------------------

// Event: list of (void returning) callbacks
#if 0
template<class F> class Event {
public:
	struct handler : e_link<handler>, callback<F> {
		template<class T> handler(T *t) : callback<F>(t)	{}
	};
	typedef	e_list<handler>	listtype;
	typedef typename listtype::iterator iterator;
	listtype				list;

	void					add(handler &link)	{
		list.push_back(&link);
	}
	handler*				find(const void *p)				{
		for (iterator i = list.begin(); i != list.end(); ++i) {
			if (i->me == p)
				return i;
		}
		return 0;
	}

	void go() {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)();
	}

	template<class P1> void go(const P1 &p1) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1);
	}

	template<class P1> void go(P1 &p1) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1);
	}

	template<class P1, class P2> void go(const P1 &p1, const P2 &p2) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1, p2);
	}

	template<class P1, class P2, class P3> void go(const P1 &p1, const P2 &p2, const P3 &p3) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1, p2, p3);
	}

	template<class P1, class P2, class P3, class P4> void go(const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1, p2, p3, p4);
	}
};
#endif

const int8 EVENT_PRIORITY_MIN		= -128;
const int8 EVENT_PRIORITY_LOW		= -1;
const int8 EVENT_PRIORITY_NORMAL	= 0;
const int8 EVENT_PRIORITY_HIGH		= 1;
const int8 EVENT_PRIORITY_MAX		= 127;

template<class F> class EventP {
public:
	struct handler : e_link<handler>, callback<F> {
		char priority;
		template<class T> handler(T *t, int8 _priority = EVENT_PRIORITY_NORMAL) : callback<F>(t), priority(_priority) {}
	};
	typedef	e_list<handler>	listtype;
	typedef typename listtype::iterator iterator;
	listtype				list;
	void add(handler &link) {
		iterator i = list.begin();
		while (i != list.end() && link.priority <= i->priority)
			++i;
		i->insert_before(&link);
	}

	void go() {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)();
	}

	template<class P> void priority_go(const P &pred) {
		for (iterator i = list.begin(), j; i != list.end(); ) {
			 if (pred((j = i++)->priority))
				(*j)();
		}
	}

	template<class P1> void go(const P1 &p1) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1);
	}

	template<class P1> void go(P1 &p1) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1);
	}

	template<class P1, class P> void priority_go(const P1 &p1, const P &pred) {
		for (iterator i = list.begin(), j; i != list.end(); ) {
			 if (pred((j = i++)->priority))
				(*j)(p1);
		}
	}

	template<class P1, class P2> void go(const P1 &p1, const P2 &p2) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1, p2);
	}

	template<class P1, class P2, class P> void priority_go(const P1 &p1, const P2 &p2, const P &pred) {
		for (iterator i = list.begin(), j; i != list.end(); ) {
			 if (pred((j = i++)->priority))
				(*j)(p1, p2);
		}
	}

	template<class P1, class P2, class P3> void go(const P1 &p1, const P2 &p2, const P3 &p3) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1, p2, p3);
	}

	template<class P1, class P2, class P3, class P> void priority_go(const P1 &p1, const P2 &p2, const P3 &p3, const P &pred) {
		for (iterator i = list.begin(), j; i != list.end(); ) {
			if (pred((j = i++)->priority))
				(*j)(p1, p2, p3);
		}
	}

	template<class P1, class P2, class P3, class P4> void go(const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4) {
		for (iterator i = list.begin(); i != list.end(); )
			(*i++)(p1, p2, p3, p4);
	}

	template<class P1, class P2, class P3, class P4, class P> void go(const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P &pred) {
		for (iterator i = list.begin(), j; i != list.end(); ) {
			if (pred((j = i++)->priority))
				(*j)(p1, p2, p3, p4);
		}
	}
};

// Event2: adaptor to pass T in as first argument (for differentiating operator()'s)
#if 0
template<class F> struct Event2;
template<class T> struct Event2<T()> : EventP<void(T*)> {
	void go()									{ EventP<void(T*)>::go((T*)this); }
	template<class P> void priority_go(P pred)	{ EventP<void(T*)>::priority_go((T*)this, pred); }
};
template<class T, class P1> struct Event2<T(P1)> : EventP<void(T*,P1)> {
	void go(P1 p1)										{ EventP<void(T*,P1)>::go((T*)this, p1); }
	template<class P> void priority_go(P1 p1, P pred)	{ EventP<void(T*,P1)>::priority_go((T*)this, p1, pred); }
};
template<class T, class P1, class P2> struct Event2<T(P1,P2)> : EventP<void(T*,P1,P2)> {
	void go(P1 p1, P2 p2)										{ EventP<void(T*,P1,P2)>::go((T*)this, p1, p2); }
	template<class P> void priority_go(P1 p1, P2 p2, P pred)	{ EventP<void(T*,P1,P2)>::priority_go((T*)this, p1, p2, pred); }
};
template<class T, class P1, class P2, class P3>	struct Event2<T(P1,P2,P3)> : EventP<void(T*,P1,P2,P3)> {
	void go(P1 p1, P2 p2, P3 p3)									{ EventP<void(T*,P1,P2,P3)>::go((T*)this, p1, p2, p3); }
	template<class P> void priority_go(P1 p1, P2 p2, P3 p3, P pred)	{ EventP<void(T*,P1,P2,P3)>::priority_go((T*)this, p1, p2, p3, pred); }
};

// GlobalEvent: makes event itself a static member of the handlers

template<class E> struct GlobalEvent : E::handler {
	static E										&get_event()							{ static E e; return e;			}
	template<typename T>							GlobalEvent(T *t, int8 priority = EVENT_PRIORITY_NORMAL) : E::handler(t, priority) { get_event() += *this; }
//	template<typename S, typename T>				GlobalEvent(T *t) : E::handler((S*)t)	{ get_event() += *this;			}
													static void	go()																	{ get_event().go(); }
//													static void priority_go(char priority)												{ get_event().priority_go(bind_second(greater_equal<char>(), priority)); }
//	template<class P>								static void priority_go(const P &pred)												{ get_event().priority_go(pred); }
	template<class P1>								static void	go(const P1 &p1)														{ get_event().go(p1); }
//	template<class P1>								static void	priority_go(const P1 &p1, char priority)								{ get_event().priority_go(p1, bind_second(greater_equal<char>(), priority)); }
//	template<class P1, class P>						static void	priority_go(const P1 &p1, const P &pred)								{ get_event().priority_go(p1, pred); }
	template<class P1, class P2>					static void go(const P1 &p1, const P2 &p2)											{ get_event().go(p1, p2); }
//	template<class P1, class P2>					static void priority_go(const P1 &p1, const P2 &p2, char priority)					{ get_event().priority_go(p1, p2, bind_second(greater_equal<char>(), priority)); }
//	template<class P1, class P2, class P>			static void priority_go(const P1 &p1, const P2 &p2, const P &pred)					{ get_event().priority_go(p1, p2, pred); }
	template<class P1, class P2, class P3>			static void go(const P1 &p1, const P2 &p2, const P3 &p3)							{ get_event().go(p1, p2, p3); }
//	template<class P1, class P2, class P3>			static void priority_go(const P1 &p1, const P2 &p2, const P3 &p3, char priority)	{ get_event().priority_go(p1, p2, p3, bind_second(greater_equal<char>(), priority)); }
//	template<class P1, class P2, class P3, class P>	static void priority_go(const P1 &p1, const P2 &p2, const P3 &p3, const P &pred)	{ get_event().priority_go(p1, p2, p3, pred); }
};
#endif

// Message/Handles:

template<class M> struct Message {
	typedef EventP<void(M*)>	E;
	typedef typename E::handler	handler;
	static	E&		get_event()			{ static E e; return e;	}
	void			send()				{ get_event().go((M*)this);	}
	static	void	add(handler &h)		{ get_event().add(h);	}
	static	void	remove(handler &h)	{ h.unlink();			}
};

#if 0
template<class M> class Handles {
	typename M::handler	h;
public:
	template<typename T>	Handles(T *t) : h(t) { M::add(h); }
	void	add()		{ M::add(h);	}
	void	remove()	{ M::remove(h); }
};
#endif

template<class T, class M, int8 P=EVENT_PRIORITY_NORMAL> class Handles2 {
	typename M::handler	h;
public:
	Handles2() : h(static_cast<T*>(this), P) { M::add(h); }
	void	add()		{ M::add(h);	}
	void	remove()	{ M::remove(h); }
};

}// namespace iso

#endif	// EVENTS_H
