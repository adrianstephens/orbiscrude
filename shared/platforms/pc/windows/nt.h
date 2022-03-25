#ifndef NT_H
#define NT_H

#include "base/strings.h"

namespace NT {
using namespace iso;

#ifdef _WIN64
#include "ntdll.h"
#else
#include "ntdll32.h"
#endif

template<typename T, _LIST_ENTRY T::*F> struct LIST : _LIST_ENTRY {
	struct iterator {
		_LIST_ENTRY	*p;
		typedef forward_iterator_t	iterator_category;
		iterator(_LIST_ENTRY *_p) : p(_p)	{}
		auto&		operator*()						const	{ return *T_get_enclosing(p, F); }
		T*			operator->()					const	{ return T_get_enclosing(p, F); }
		bool		operator==(const iterator &b)	const	{ return p == b.p; }
		bool		operator!=(const iterator &b)	const	{ return p != b.p; }
		iterator	operator++()			{ p = p->Flink; return *this;	}
		iterator	operator++(int)			{ auto p0 = p; p = p0->Flink; return p0; }
		iterator	operator--()			{ p = p->Blink; return *this;	}
		iterator	operator--(int)			{ auto p0 = p; p = p0->Blink; return p0; }
	};

	struct const_iterator {
		const _LIST_ENTRY	*p;
		typedef forward_iterator_t	iterator_category;
		const_iterator(const _LIST_ENTRY *_p) : p(_p)	{}
		auto&			operator*()							const	{ return *T_get_enclosing(p, F); }
		const T*		operator->()						const	{ return T_get_enclosing(p, F); }
		bool			operator==(const const_iterator &b)	const	{ return p == b.p; }
		bool			operator!=(const const_iterator &b)	const	{ return p != b.p; }
		const_iterator	operator++()		{ p = p->Flink; return *this;	}
		const_iterator	operator++(int)		{ auto p0 = p; p = p0->Flink; return p0; }
		const_iterator	operator--()		{ p = p->Blink; return *this;	}
		const_iterator	operator--(int)		{ auto p0 = p; p = p0->Blink; return p0; }
	};

	LIST(_LIST_ENTRY &head) : _LIST_ENTRY(head) {}
	LIST(T &t)				: _LIST_ENTRY(t.*F) {}
	iterator		begin()			{ return Flink;			}
	iterator		end()			{ return this;			}
	const_iterator	begin()	const	{ return Flink;			}
	const_iterator	end()	const	{ return this;			}
	T&				front()	const	{ return *T_get_enclosing(Flink, F); }
	T&				back()	const	{ return *T_get_enclosing(Blink, F); }
	bool			empty()	const	{ return Flink == this; }
};

template<typename T, _LIST_ENTRY T::*F> auto& get_list(T &t)				{ return (LIST<T, F>&)t.*F; }
template<typename T, _LIST_ENTRY T::*F> auto& get_list(_LIST_ENTRY &head)	{ return (LIST<T, F>&)head; }

template<typename T, _SINGLE_LIST_ENTRY T::*F> struct SINGLE_LIST : _SINGLE_LIST_ENTRY {
	struct iterator {
		_SINGLE_LIST_ENTRY	*p;
		iterator(_SINGLE_LIST_ENTRY *_p) : p(_p)	{}
		auto&		operator*()						const	{ return *T_get_enclosing(p, F); }
		T*			operator->()					const	{ return T_get_enclosing(p, F); }
		bool		operator==(const iterator &b)	const	{ return p == b.p; }
		bool		operator!=(const iterator &b)	const	{ return p != b.p; }
		iterator	operator++()							{ p = p->Next; return *this;	}
		iterator	operator++(int)							{ auto p0 = p; p = p0->Next; return p0; }
	};

	iterator	begin()				{ return Next; }
	iterator	end()				{ return 0; }
	T&			front()		const	{ return T_get_enclosing(Next, F); }
	bool		empty()		const	{ return Next == 0; }
};

template<typename T, _SINGLE_LIST_ENTRY T::*F> auto& get_list(T &t)						{ return (SINGLE_LIST<T, F>&)t.*F; }
template<typename T, _SINGLE_LIST_ENTRY T::*F> auto& get_list(_SINGLE_LIST_ENTRY &head)	{ return (SINGLE_LIST<T, F>&)head; }

struct STRING : _STRING {
	STRING() {}
	STRING(const char *p) {
		Length = MaximumLength = uint16(string_len(p));
		Buffer = const_cast<char*>(p);
	}
	operator count_string() const { return count_string(Buffer, Length); }
	friend count_string str(const STRING &s) { return s; }
};

struct UNICODE_STRING : _UNICODE_STRING {
	UNICODE_STRING() {
		clear(*this);
	}
	UNICODE_STRING(const wchar_t *p) {
		Length = MaximumLength = uint16(string_len(p) * 2);
		Buffer = const_cast<wchar_t*>(p);
	}
	UNICODE_STRING(uint64 *id) {
		Length = MaximumLength = sizeof(uint64);
		Buffer = reinterpret_cast<wchar_t*>(id);
	}
	operator count_string16() const { return count_string16(Buffer, Length / 2); }
	friend count_string16 str(const UNICODE_STRING &s) { return s; }
};

inline string_accum &operator<<(string_accum &sa, const _UNICODE_STRING &u) {
	return sa << count_string16(u.Buffer, u.Length / 2);
}

} //namespace NT

#endif // NT_H