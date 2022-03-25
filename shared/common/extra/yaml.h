#ifndef YAML_H
#define YAML_H

#include "base/defs.h"
#include "base/strings.h"
#include "base/array.h"
#include "base/tree.h"
#include "extra/text_stream.h"
#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	YAMLreader
//-----------------------------------------------------------------------------

class YAMLreader : public text_reader<istream_ref, 2> {

	enum CONTEXT {
		KEY, BLOCK, FLOW,
	} context;

	map<string,void*>	anchors;
	map<string,string>	tags;
	string				version;

	int get_leading_spaces() {
		char	c;
		while ((c = getc()) == ' ');
		put_back(c);
		return col_number;
	}

	string	get_token0(const char_set &set) {
		string_builder	b;
		int			c;
		while ((c = getc()) == ' ');
		for (; c != -1 && set.test(c); c = getc())
			b.putc(c);
		return b;
	}
	string	get_token(const char_set &set) {
		string_builder	b;
		int			c;
		while ((c = getc()) == ' ');
		for (;c != -1 && set.test(c); c = getc())
			b.putc(c);
		put_back(c);
		return b;
	}
	string	get_token(const char_set &first, const char_set &set) {
		string_builder	b;
		int				c;
		while ((c = getc()) == ' ');
		if (first.test(c)) {
			b.putc(c);
			while (set.test(c = getc()))
				b.putc(c);
		}
		put_back(c);
		return b;
	}

	int		get_start();
	void*	get_node(const void *key, int indentation);
	string	get_string(int indentation);
	void*	get_value(const char *tag, const void *key, int indentation);
	void*	get_block(const char *map_tag, const char *tag, const void *map_key, int indentation, const char *anchor);
	void*	get_map(const char *map_tag, const char *tag, const void *map_key, const void *key, int indentation, const char *anchor);
	string	get_tag();

public:
	enum CONSTRUCT {DOC, SEQ, MAP};

	struct delegate {
		void		*me;
		void		(*vBegin)	(void *me, const char *tag, const void *key, CONSTRUCT c);
		void*		(*vEnd)		(void *me, CONSTRUCT c);
		void*		(*vValue)	(void *me, const char *tag, const void *key, const char *val);
		void		(*vEntry)	(void *me, const void *key, void *val);
		template<typename T> struct thunks {
			static void		Begin(void *t, const char *tag, const void *key, CONSTRUCT c)		{ ((T*)t)->Begin(tag, key, c); }
			static void*	End(void *t, CONSTRUCT c)											{ return ((T*)t)->End(c); }
			static void*	Value(void *t, const char *tag, const void *key, const char*val)	{ return ((T*)t)->Value(tag, key, val); }
			static void		Entry(void *t, const void *key, void *val)							{ ((T*)t)->Entry(key, val); }
		};
		template<typename T> delegate(T *t) : me(t), vBegin(thunks<T>::Begin), vEnd(thunks<T>::End), vValue(thunks<T>::Value), vEntry(thunks<T>::Entry) {}
	} d;

	void		Begin(const char *tag, const void *key, CONSTRUCT c)		{ d.vBegin(d.me, tag, key, c); }
	void*		End(CONSTRUCT c)											{ return d.vEnd(d.me, c); }
	void*		Value(const char *tag, const void *key, const char *val)	{ return d.vValue(d.me, tag, key, val); }
	void		Entry(const void *key, void *val)							{ d.vEntry(d.me, key, val); }

public:
	YAMLreader(const delegate &_d, istream_ref _file, int _line_number = 0);
	void		Read();
	int			GetLineNumber() const { return line_number; }
};

//-----------------------------------------------------------------------------
//	YAMLwriter
//-----------------------------------------------------------------------------

class YAMLwriter {
	ostream_ref	file;
	int			indent;
	uint32		named;
	bool		first;

	void		NewLine();
	void		Start(const char *id);

public:
	void		Begin(const char *id, bool names);
	void		End();
	void		Write(const char *id, bool b);
	void		Write(const char *id, int i);
	void		Write(const char *id, float f);
	void		Write(const char *id, const char *s);

	YAMLwriter(ostream_ref _file) : file(_file), indent(0), named(0), first(true)	{}
	YAMLwriter&	me()	{ return *this; }
};

//-----------------------------------------------------------------------------
//	YAML DOM
//-----------------------------------------------------------------------------

struct YAMLval {
	enum TYPE {NIL, BOOL, INT, FLOAT, STRING, ARRAY, OBJECT} type;
	YAMLval(TYPE _type) : type(_type)	{}
};
struct YAMLarray	: YAMLval, dynamic_array<YAMLval*> {
	YAMLarray() : YAMLval(ARRAY)	{}
	YAMLval	*&add()	{ return push_back(); }
};
typedef pair<string, YAMLval*> YAMLentry;
struct YAMLobject	: YAMLval, dynamic_array<YAMLentry>	{
	YAMLobject() : YAMLval(OBJECT)	{}
	YAMLval	*&add(const char *id)	{ YAMLentry	&e = push_back(); e.a = id; return e.b; }
};

template<typename T> struct YAMLelement;
template<> struct YAMLelement<bool>		: YAMLval, holder<bool>	{ YAMLelement(bool v)		: YAMLval(BOOL),	holder<bool>(v)	{} };
template<> struct YAMLelement<int>		: YAMLval, holder<int>	{ YAMLelement(int v)		: YAMLval(INT),		holder<int>(v)	{} };
template<> struct YAMLelement<float>	: YAMLval, holder<float>{ YAMLelement(float v)		: YAMLval(FLOAT),	holder<float>(v)	{} };
template<> struct YAMLelement<string>	: YAMLval, string		{ YAMLelement(const char *v): YAMLval(STRING),	string(v)		{} };

class YAML0 {
	static bool	write(YAMLwriter &w, const char *id, YAMLval *p);
protected:
	YAMLval*	p;
	YAMLval*	GetNode(const char *s) const;
	YAMLval*	GetNode(int i) const;
	YAML0()				: p(0)	{}
	YAML0(YAMLval *p)	: p(p)	{}
public:
	static YAMLarray					*CreateArray()		{ return new YAMLarray(); }
	static YAMLobject					*CreateObject()		{ return new YAMLobject(); }
	template<typename T> static YAMLval *CreateElement(T t)	{ return new YAMLelement<T>(t); }

	YAML0		operator/(const char *s)		const	{ return YAML0(GetNode(s));	}
	YAML0		operator/(int i)				const	{ return YAML0(GetNode(i));	}
				operator YAMLval*()				const	{ return p;					}
	const char*	get_name(int i)					const	{ return p && p->type == YAMLval::OBJECT ? (const char*)(*(YAMLobject*)p)[i].a : 0; }
	const char*	get_string(const char *def = 0)	const	{ return p && p->type == YAMLval::STRING ? (const char*)*(YAMLelement<string>*)p : def; }
	int			get_int(int def = 0)			const	{ return p && p->type == YAMLval::INT ? int(*(YAMLelement<int>*)p) : p && p->type == YAMLval::FLOAT ? int(*(YAMLelement<float>*)p) : def; }
	float		get_float(float def = 0.f)		const	{ return p && p->type == YAMLval::INT ? float(*(YAMLelement<int>*)p) : p && p->type == YAMLval::FLOAT ? float(*(YAMLelement<float>*)p) : def; }
	bool		get_bool(bool def = false)		const	{ return p && p->type == YAMLval::BOOL ? bool(*(YAMLelement<bool>*)p) : def; }
	size_t		get_count()						const;

	bool		write(ostream_ref file)			const	{ return write(YAMLwriter(file).me(), 0, p); }
};

class YAML : public YAML0 {
	static void	kill(YAMLval *p);
public:
	~YAML()		{ kill(p); }
};

string_accum& operator<<(string_accum &a, YAML0 *j);

} // namespace iso

#endif	//YAML_H

