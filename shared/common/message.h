#ifndef MESSAGE_H
#define MESSAGE_H

#include "base/algorithm.h"
#include "base/functions.h"
#include "base/hash.h"
#include "base/shared_ptr.h"
#include "base/strings.h"
#include "base/array.h"
#include "crc32.h"

namespace iso {

//template<class R> struct returns {
//	typedef R type;
//};
//
//template<class M> struct returns_s {
//};

template<class M> uint32 type_id() {
	static crc32 id(meta::fixed_name<M>().begin());
	return id;
}

class MessageReceiver {
	hash_map<uint32, callback<void(int&)>>	handlers;
public:
	void ClearAllHandlers() {
		handlers.clear();
	}
	callback<void(int&)> _SetHandler(uint32 id, const callback<void(int&)> &h) {
		return exchange(handlers[id].put(), h);
	}
	template<class M> const callback<void(M&)> *GetHandler() {
		return reinterpret_cast<const callback<void(M&)>*>(handlers.check(type_id<M>()));
	}
	template<class M> callback<void(M&)> SetHandler(const callback<void(M&)> &h) {
		return exchange(reinterpret_cast<callback<void(M&)>&>(handlers[type_id<M>()].put()), h);
	}
	template<class M> void ClearHandler() {
		handlers.remove(type_id<M>());
	}
	template<class M> bool Send(M &m) {
		if (const callback<void(M&)> *p = GetHandler<M>()) {
			(*p)(m);
			return true;
		}
		return false;
	}
	template<class M> bool Send(const M &m) {
		return Send(const_cast<M&>(m));
	}
};

template<class T, class M> struct SafeDelegate {
	weak_ptr<T>	w;
	void operator()(M &m) {
		if (shared_ptr<T> s = w.get())
			(*s)(m);
	}
	SafeDelegate(const shared_ptr<T> &p)	: w(p)	{}
	SafeDelegate(const weak_ptr<T> &p)		: w(p)	{}
	SafeDelegate(T *p)						: w(p)	{}
};

template<class T, class M> struct DelegateLink {
	callback<void(M&)>	prev;
	void	operator()(M &m) {
		callback<void(M&)>	save_prev = prev;
		(*static_cast<T*>(this))(m);
		if (save_prev)
			save_prev(m);
	}
	DelegateLink() {}
	DelegateLink(MessageReceiver *r) {
		if (r)
			prev = r->SetHandler<M>(this);
	}
	void	Add(MessageReceiver *r)	{
		prev = r->SetHandler<M>(this);
	}
	void	Remove(MessageReceiver *r)	{
		r->SetHandler<M>(prev);
	}
};

template<class M> struct DelegateLink<void, M> {
	callback<void(M&)>	prev;
	template<typename T> static void	f(DelegateLink<void, M> *me, M &m) {
		callback<void(M&)>	save_prev = me->prev;
		(*static_cast<T*>(me))(m);
		if (save_prev)
			save_prev(m);
	}
	template<typename T> DelegateLink(T *t, MessageReceiver *r) {
		if (r) {
			callback<void(M&)>	c(this, f<T>);
			prev = r->SetHandler<M>(c);
		}
	}
	template<typename T> void	Add(T *t, MessageReceiver *r)	{
		callback<void(M&)>	c(f<T>, this);
		prev = r->SetHandler<M>(c);
	}
	void	Remove(MessageReceiver *r)	{
		r->SetHandler<M>(prev);
	}
};

template<class M> struct DelegateChain {
	callback<void(M&)>	cb, prev;
	void	operator()(M &m) {
		cb(m);
		if (prev)
			prev(m);
	}
	DelegateChain(MessageReceiver *r, const callback<void(M&)> &cb) : cb(cb) {
		prev	= r->SetHandler<M>(this);
	}
	void	Remove(MessageReceiver *r)	{
		r->SetHandler<M>(prev);
	}
	static void	Add(MessageReceiver *r, const callback<void(M&)> &_cb) {
		new DelegateChain(r, _cb);
	}
};

template<class M> class DelegateArray {
public:
	typedef	callback<void(M&)>	cb;
	void	operator()(M &m) {
		for (auto &i : a)
			i(m);
	}
private:
	dynamic_array<cb>			a;
	void	Add(const cb &c) {
		a.push_back(c);
	}
	bool	Remove(const cb &c) {
		cb	*i = find(a, c);
		if (i != a.end()) {
			a.erase_unordered(i);
			return a.size() == 0;
		}
		return false;
	}
public:
	static void	Add(MessageReceiver *r, const cb &c) {
		DelegateArray *da;
		if (const cb *p = r->GetHandler<M>()) {
			da = (DelegateArray*)p->me;
		} else {
			da = new DelegateArray;
			r->SetHandler<M>(da);
		}
		da->Add(c);
	}
	static void	Remove(MessageReceiver *r, const cb &c) {
		if (const cb *p = r->GetHandler<M>()) {
			DelegateArray	*da = (DelegateArray*)p->me;
			if (da->Remove(c)) {
				r->ClearHandler<M>();
				delete da;
			}
		}
	}
};

template<class T, class M> struct MessageHandler {
	MessageHandler() { static_cast<T*>(this)->template SetHandler<M>(static_cast<T*>(this)); }
	void	Remove() { static_cast<T*>(this)->template ClearHandler<M>(); }
};

template<class T, class M> struct MesssageDelegate {
	MesssageDelegate(MessageReceiver *r)	{ r->template SetHandler<M>(static_cast<T*>(this)); }
	void	Remove(MessageReceiver *r)		{ r->template ClearHandler<M>(); }
};

template<class M> struct DelegateCollection : DelegateArray<M> {};


} // namespace iso

#endif // MESSAGE_H
