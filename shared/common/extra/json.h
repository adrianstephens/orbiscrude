#ifndef JSON_H
#define JSON_H

#include "base/defs.h"
#include "base/strings.h"
#include "base/array.h"
#include "extra/text_stream.h"
#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	JSONreader
//-----------------------------------------------------------------------------

class JSONreader : public text_reader<reader_intf> {
public:
	struct Token {
		enum TYPE {
			BAD,			COMA,			COLON,
			ARRAY_BEGIN,	ARRAY_END,		OBJECT_BEGIN,	OBJECT_END,
			ELEMENT_NIL,	ELEMENT_BOOL,	ELEMENT_INT,	ELEMENT_FLOAT,	ELEMENT_STRING,
		} type;
		union {
			char	*s;
			int64	i;
			float64	f;
		};
		void	clear() {
			if (type == ELEMENT_STRING)
				free(s);
		}
		Token()					: type(BAD)				{ i = 0; }
		Token(TYPE t)			: type(t)				{ i = 0; }
		Token(bool _b)			: type(ELEMENT_BOOL)	{ i = _b; }
		Token(int64 _i)			: type(ELEMENT_INT)		{ i = _i; }
		Token(float64 _f)		: type(ELEMENT_FLOAT)	{ f = _f; }
		Token(const char *_s)	: type(ELEMENT_STRING)	{ s = strdup(_s); }
		Token(const Token &b)	: type(b.type)			{ i = b.i; const_cast<Token&>(b).i = 0; }
		~Token()										{ clear(); }
		Token& operator=(Token &b)						{ swap(type, b.type); swap(i, b.i); return *this; }
		Token&	me()									{ return *this; }
	};
public:
	JSONreader(istream_ref file) : text_reader<reader_intf>(file)	{}
	JSONreader&	me()					{ return *this; }
	int			GetLineNumber()	const	{ return line_number; }
	Token		GetToken();
};

//-----------------------------------------------------------------------------
//	JSONwriter
//-----------------------------------------------------------------------------

class JSONwriter : public text_writer<writer_intf> {
	typedef text_writer<writer_intf> B;
	int			indent;
	uint32		named;
	bool		first;

	decltype(declval<B>().begin())	Start(const char *id);

public:
	void		Begin(const char *id, bool names);
	void		End();
	template<typename T> enable_if_t<is_num<T>> Write(const char *id, T t) 	{ Start(id) << t; }
	void					Write(const char *id, bool t) 					{ Start(id) << t; }
	void					Write(const char *id, const char *t);

	struct array;

	struct object {
		JSONwriter	&json;
	public:
		object(JSONwriter &json, const char *name) : json(json) 	{ json.Begin(name, true); }
		~object()													{ json.End(); }
		template<typename T> const object& Write(const char *id, const T &t) const { json.Write(id, t); return *this; }
		object 	Object(const char *id)	const	{ return {json, id}; }
		array	Array(const char *id)	const	{ return {json, id}; }
	};
	struct array {
		JSONwriter	&json;
	public:
		array(JSONwriter &json, const char *name) : json(json) 		{ json.Begin(name, false); }
		~array()													{ json.End(); }
		template<typename T> const array& Write(const char *id, const T &t) const { json.Write(id, t); return *this; }
		object 	Object(const char *id)	const	{ return {json, id}; }
		array	Array(const char *id)	const	{ return {json, id}; }
	};
	object 	Object(const char *id)	{ return {*this, id}; }
	array	Array(const char *id)	{ return {*this, id}; }

	JSONwriter(ostream_ref file) : B(file), indent(0), named(0), first(true)	{}
	JSONwriter&	me()	{ return *this; }
};

//-----------------------------------------------------------------------------
//	JSON DOM
//-----------------------------------------------------------------------------

struct JSONval {
	enum TYPE {NONE, NIL, BOOL, INT, FLOAT, STRING, ARRAY, OBJECT, BINARY, NUM_TYPES} type;
	typedef compact_array<JSONval>				array;
	typedef	compact_array<pair<string,JSONval>>	object;
	typedef compact_array<char>					binary;

	union {
		bool	b;
		int64	i;
		double	f;
		string	s;
		array	a;
		object	o;
		binary	bin;
	};
	
	static JSONval	none;

	template<typename T, typename = void> struct get_s;

	JSONval&	GetNode(const char *s) const;

	constexpr JSONval() : type(NONE), i(0) {}
	constexpr JSONval(_none&) : type(NONE), i(0)	{}
	explicit constexpr JSONval(TYPE _type) : type(_type),	i(0)	{}
	explicit constexpr JSONval(bool		b) : type(BOOL),	b(b)	{}
	explicit constexpr JSONval(int64	i) : type(INT),		i(i)	{}
	explicit constexpr JSONval(double	f) : type(FLOAT),	f(f)	{}
	constexpr JSONval(JSONval&&	j) : type(j.type),	i(j.i)		{ j.type = NIL; }
	constexpr JSONval(string&&	s) : type(STRING),	s(move(s))	{}
	JSONval(array&&		a)	: type(ARRAY),	a(move(a))	{}
	JSONval(object&&	o)	: type(OBJECT),	o(move(o))	{}
	
	constexpr JSONval(const JSONval& j) : type(j.type), i(0) {
		switch (type) {
			case STRING:	s = j.s; 	break;
			case ARRAY:		a = j.a;	break;
			case OBJECT:	o = j.o; 	break;
			default:		i = j.i;	break;
		}
	}
	~JSONval() {
		switch (type) {
			case STRING:	s.~string(); 	break;
			case ARRAY:		a.~array();		break;
			case OBJECT:	o.~object(); 	break;
			default:						break;
		}
	}
	JSONval& operator=(JSONval&& j) 		{ swap(type, j.type); swap(i, j.i); return *this; }
	JSONval& operator=(const JSONval& j) 	{ return operator=(JSONval(j)); }

	explicit 	operator bool()				const	{ return type != NONE; }

	JSONval&	set(const char *s) {
		if (type == NIL || type == NONE)
			*this = object();
		if (type == OBJECT) {
			for (auto &i : o) {
				if (i.a == s)
					return i.b;
			}
			return o.emplace_back(s, none).b;
		}
		return none;
	}

	JSONval&	operator[](int i)			const	{ return type == ARRAY && i < a.size() ? a[i] : type == OBJECT && i < o.size() ? o[i].b : none; }
	JSONval&	operator[](const JSONval &i)	const	{
		return	i.type == JSONval::INT		? operator[](i.i)
			:	i.type == JSONval::STRING	? operator/(i.s)
			:	none;
	}
	JSONval&	operator/(const char *s)	const	{
		if (type == JSONval::OBJECT) {
			for (auto &i : o) {
				if (i.a == s)
					return i.b;
			}
		}
		return none;
	}
	JSONval&	operator/(const JSONval &i)	const	{ return operator[](i); }
	JSONval&	operator/(int i)			const	{ return operator[](i); }

	const char*	get_name(int i)				const	{ return type == OBJECT ? o[i].a : 0; }
	template<typename T> optional<T> get()	const	{ return get_s<T>::f(this); }
	template<typename T> T	get(T def)		const	{ return get<T>().or_default(def); }
	getter<const JSONval>	get()			const	{ return *this; }
	size_t		size()						const	{ return type == ARRAY ? a.size() : type == OBJECT ? o.size() : 0; }
	auto		begin()						const	{ return make_indexed_iterator(*this, make_int_iterator(0)); }
	auto		end()						const	{ return make_indexed_iterator(*this, make_int_iterator((int)size())); }
	
	range<pair<string,JSONval>*>	items()	const	{ if (type == JSONval::OBJECT) return {o.begin(), o.end()}; return {}; }
	
	bool		push_back(JSONval &&v) {
		if (type = NONE)
			*this = JSONval(JSONval::array());
		if (type != ARRAY)
			return false;
		a.push_back(move(v));
		return true;
	}
	
	bool 		write(JSONwriter &w, const char *id);
	bool		read(JSONreader &r);
	bool 		write(ostream_ref w)	{ return write(lvalue(JSONwriter(w)), 0); }
	bool		read(istream_ref r)	{ return read(lvalue(JSONreader(r))); }

	friend bool operator==(const JSONval &a, const JSONval &b) {
		if (a.type == b.type) {
			switch(a.type) {
				default:	return false;
				case NIL:	return true;
				case BOOL:	return a.b == b.b;
				case INT:	return a.i == b.i;
				case FLOAT:	return a.f == b.f;
				case STRING:return a.s == b.s;
				case ARRAY:	return a.a == b.a;
				case OBJECT:return a.o == b.o;
			}
		}
		return 	a.type == INT && b.type == FLOAT ? a.i == b.f
			:	a.type == FLOAT && b.type == INT ? a.f == b.i
			:	false;
	}
	
	friend bool operator<(const JSONval &a, const JSONval &b) {
		if (a.type == b.type) {
			switch(a.type) {
				default:	return false;
				case NIL:	return true;
				case BOOL:	return a.b < b.b;
				case INT:	return a.i < b.i;
				case FLOAT:	return a.f < b.f;
				case STRING:return a.s < b.s;
				case ARRAY:	return a.a < b.a;
				case OBJECT:return a.o < b.o;
			}
		}
		return 	a.type == INT && b.type == FLOAT ? a.i < b.f
			:	a.type == FLOAT && b.type == INT ? a.f < b.i
			:	a.type < b.type;
	}
	friend bool operator> (const JSONval &a, const JSONval &b) 	{ return b < a; }
	friend bool operator!=(const JSONval &a, const JSONval &b) 	{ return !(a == b); }
	friend bool operator<=(const JSONval &a, const JSONval &b) 	{ return !(b < a); }
	friend bool operator>=(const JSONval &a, const JSONval &b) 	{ return !(a < b); }
};

force_inline JSONval operator""_json(const char* s, std::size_t n) {
	return memory_reader(s, s + n).get();
}

template<> force_inline optional<const char*> JSONval::get<const char*>() const {
	if (type == STRING)
		return s.begin();
	return {};
}

template<> force_inline optional<string> JSONval::get<string>() const {
	if (type == STRING)
		return s;
	return {};
}

template<> force_inline optional<bool> JSONval::get<bool>() const {
	if (type == BOOL)
		return b;
	return {};
}

template<> force_inline optional<const_memory_block> JSONval::get<const_memory_block>() const {
	switch (type) {
		case BOOL:		return const_memory_block(&b);
		case INT:		return const_memory_block(&i);
		case FLOAT:		return const_memory_block(&f);
		case STRING:	return const_memory_block(s.begin(), s.end());
		case BINARY:	return const_memory_block(&bin[0], bin.size());
		default:		return {};
	}
}

template<typename T> struct JSONval::get_s<T, enable_if_t<is_num<T>>> {
	static optional<T> f(const JSONval *p) {
		if (p->type == JSONval::INT || p->type == JSONval::FLOAT)
			return p->type == JSONval::INT ? T(p->i) : T(p->f);
		return {};
	}
};

template<typename T> struct JSONval::get_s<dynamic_array<T>> {
	static optional<dynamic_array<T>> f(const JSONval *p) {
		if (p->type == JSONval::ARRAY) {
			dynamic_array<T>	a;
			for (auto &i : p->a)
				a.push_back(i.get<T>());
			return a;
		}
		return {};
	}
};

template<typename T, int N> struct JSONval::get_s<iso::array<T, N>> {
	static optional<iso::array<T, N>> f(const JSONval* p) {
		if (p->type == JSONval::ARRAY && p->a.size() == N) {
			iso::array<T, N>	a;
			for (int i = 0; i < N; i++)
				a[i] = p->a[i].get<T>();
			return a;
		}
		return {};
	}
};

string_accum& operator<<(string_accum& a, const pair<string,JSONval>& v);
string_accum& operator<<(string_accum& a, const JSONval& v);

struct JSONpath {
	string	path;
	JSONpath() {}
	JSONpath(const string &path) : path(path) {}
	JSONpath(string &&path) : path(move(path)) {}

	const JSONval&	get(const JSONval &val) {
		const JSONval	*pv = &val;
		for (auto x : parts<'/'>(path)) {
			if (x)
				pv = &((*pv) / string(x).begin());
		}
		return *pv;
	}
	JSONval&	set(JSONval &val) {
		JSONval	*pv = &val;
		for (auto x : parts<'/'>(path)) {
			if (x)
				pv = &pv->set(string(x));
		}
		return *pv;
	}

	JSONpath	operator/(const char *s)	const	{
		return path + "/" + s;
	}
	JSONpath	operator[](int i)	const	{
		return path + "/" + to_string(i);
	}

};


} // namespace iso

#endif	//JSON_H

