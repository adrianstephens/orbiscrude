#ifndef XML_H
#define XML_H

#include "extra/identifier.h"
#include "base/array.h"
#include "base/hash.h"
#include "extra/text_stream.h"
#include "stream.h"

namespace iso {

//constexpr stuff

template<int N, typename T> struct XMLattr {
	constexpr_string<char, N>	name;
	const T	&value;
	constexpr XMLattr(const constexpr_string<char, N>& name, const T &value) : name(name), value(value) {}

	friend constexpr auto to_constexpr_string(const XMLattr &a) {
		return " " + a.name + "=\"" + to_constexpr_string(a.value) + "\"";
	}
};

template<int N, typename T> constexpr auto make_xml_attrib(const char (&name)[N], const T &value) {
	return XMLattr<N - 1, T>(name, value);
}

template<int N, typename A, typename C> struct XMLtag {
	constexpr_string<char, N>	name;
	A			attribs;
	C			children;
	constexpr XMLtag(const constexpr_string<char, N>& name, const A &attribs, const C &children) : name(name), attribs(attribs), children(children) {}

	friend constexpr auto to_constexpr_string(const XMLtag &tag) {
		return "<" + tag.name + to_constexpr_string(tag.attribs) + ">" + to_constexpr_string(tag.children) + "</" + tag.name + ">";
	}
};

template<int N, typename A> struct XMLtag<N, A, meta::tuple<>> {
	constexpr_string<char, N>	name;
	A			attribs;
	constexpr XMLtag(const constexpr_string<char, N>& name, const A &attribs, const meta::tuple<>&) : name(name), attribs(attribs) {}

	friend constexpr auto to_constexpr_string(const XMLtag &tag) {
		return "<" + tag.name + to_constexpr_string(tag.attribs) + "/>";
	}
};

template<int N, typename A, typename... C> constexpr auto make_xml_tag(const char (&name)[N], const A &attribs, const C&... children) {
	return XMLtag<N - 1, A, meta::tuple<C...>>(name, attribs, {children...});
}

template<typename X> constexpr auto xml_constexpr_string(const X &tag) {
	return "<?xml version='1.0'?>" + to_constexpr_string(tag);
}

//-----------------------------------------------------------------------------
//	XMLreader
//-----------------------------------------------------------------------------

class XMLreader : public text_mode_reader<istream_ref>	{
public:
	enum Flag {
		UNQUOTEDATTRIBS		= 1 << 0,
		NOEQUALATTRIBS		= 1 << 1,
		SKIPUNKNOWNENTITIES	= 1 << 2,
		ALLOWBADNAMES		= 1 << 3,
		SKIPBADNAMES		= 1 << 4,
		RELAXED				= 1 << 5,
		GIVEWHITESPACE		= 1 << 6,
		THROWEXCESSATTRIBS	= 1 << 7,
	};

	enum TagType {
		TAG_ERROR	= -1,
		TAG_EOF		= 0,
		TAG_BEGIN,
		TAG_END,
		TAG_BEGINEND,
		TAG_CONTENT,
		TAG_COMMENT,
		TAG_DECL,
		TAG_DIR,

		TAG_BEGINEND_ENTER,
	};

	struct Data {
		dynamic_array<char*>	values;

		struct Attribute {
			cstring				name;
			from_string_getter	value;
		};

		cstring			Name()								const { return values[0]; }
		count_string	Content()							const { return count_string(values[0], values[1]); }
		range<Attribute*>	Attributes() const { return range<Attribute*>((Attribute*)(values.begin() + 1), (Attribute*)(values.end())); }

		cstring			Find(const char *name)		const;
		cstring			Find(const ichar *name)	const;
		cstring			FindWild(const char *name, dynamic_array<count_string> &matches) const;
		cstring			FindWild(const ichar *name, dynamic_array<count_string> &matches) const;

		template<int N> bool	Is(const char (&s)[N]) const {
			return Name() == s;
		}
		template<int N> bool	Is(const ichar (&s)[N]) const {
			return Name() == s;
		}
		template<typename T> optional<T> Get(const char *name) const {
			if (auto s = Find(name))
				return from_string<T>(s);
			return none;
		}
		template<typename T> T Get(const char *name, T def) const {
			if (auto s = Find(name))
				return from_string<T>(s);
			return def;
		}
		template<typename T> bool Read(const char *name, T &t) const {
			if (auto s = Find(name))
				return from_string(s, t);
			return false;
		}
		from_string_getter operator[](const char *name) const {
			return Find(name).begin();
		}
	};

protected:
	flags<Flag>					flags;
	dynamic_array<char>			value;
	hash_map<string, string>	custom_entities, namespaces;

	int				GetEscape();
	bool			GetName(int c);

public:
	static	char	*FixEscapes(char *s);

	XMLreader(istream_ref file, int line_number = 1) : text_mode_reader<istream_ref>(file, line_number), flags(0) {}

	void			SetFlag(Flag f)			{ flags.set(f); }
	int				GetLineNumber()	const	{ return line_number; }
	void			AddEntity(const char *name, const char *value)			{ custom_entities[name] = value; }
	void			AddNamespace(string_param &&name, string_param &&value)	{ namespaces[name] = value; }

	void			Process(const Data &data, TagType type) {
		if (type == TAG_DECL && data.values.size() == 3 && str(data.values[0]) == "ENTITY" && str(data.values[1]) != "SYSTEM")
			AddEntity(data.values[1], data.values[2]);
	}

	float			CheckVersion();
	TagType			ReadNext(Data &data);
	TagType			ReadNext(Data &data, TagType type);
};

struct XMLiterator {
	XMLreader			&xml;
	XMLreader::Data		&data;
	XMLreader::TagType	tag;
	XMLiterator(XMLreader &_xml, XMLreader::Data &_data) : xml(_xml), data(_data), tag(XMLreader::TAG_END) {}

	bool	IsBeginEnd() {
		return tag == XMLreader::TAG_BEGINEND;
	}
	void	Enter() {
		tag	= tag == XMLreader::TAG_BEGINEND ? XMLreader::TAG_BEGINEND_ENTER : XMLreader::TAG_END;
	}
	bool	Next() {
		if (tag == XMLreader::TAG_BEGINEND_ENTER) {
			tag = XMLreader::TAG_BEGINEND;
			return false;
		}
		if (tag == XMLreader::TAG_BEGIN || tag == XMLreader::TAG_CONTENT)
			tag = xml.ReadNext(data, XMLreader::TAG_END);
		tag = xml.ReadNext(data, XMLreader::TAG_BEGIN);
		return tag == XMLreader::TAG_BEGIN || tag == XMLreader::TAG_BEGINEND;
	}
	count_string Content() {
		tag = xml.ReadNext(data, XMLreader::TAG_CONTENT);
		return tag == XMLreader::TAG_CONTENT ? data.Content() : count_string();
	}
};

//-----------------------------------------------------------------------------
//	XMLwriter
//-----------------------------------------------------------------------------

struct XMLelement;
class XMLwriter : public text_mode_writer<ostream_ref> {
	int						indent;
	bool					opentag, closetagnewline, format;

	void					EscapeString(const char *name, int num_entities);
	void					NewLine();

public:
	XMLelement				Element(const char *name);
	void					ElementBegin(const char *name);
	void					ElementEnd(const char *name);
	void					Attribute(const char *name, const char *value);
	void					ElementContent(const char *text);
	void					ElementContentVerbatim(const char *text, size_t len);
	void					ElementContentVerbatim(const_memory_block m) { ElementContentVerbatim(m, m.length()); }
	void					CDATA(const void *data, int len);
	void					Comment(const char *comment);

	template<typename T> void Attribute(const char *name, T &&t)	{ Attribute(name, to_string(t)); }

	XMLwriter(ostream_ref file, bool format, text_mode mode = text_mode::ASCII) : text_mode_writer<ostream_ref>(file, mode), indent(-1), opentag(false), format(format) {
		//puts("<?xml version=\"1.0\"?>\n");
	}
	XMLwriter&	me()						{ return *this; }
	int			GetIndentation()	const	{ return indent; }
};

struct XMLelement {
	XMLwriter	&xml;
	const char	*name;
public:
	XMLelement(XMLwriter &xml, const char *name) : xml(xml), name(name)		{ xml.ElementBegin(name); }
	~XMLelement()															{ xml.ElementEnd(name); }
	template<typename T> XMLelement &Attribute(const char *name, T &&t)		{ xml.Attribute(name, t); return *this; }
	operator XMLwriter&()													{ return xml; }
	XMLelement				Element(const char *name)						{ return XMLelement(xml, name); }
	void					ElementContent(const char *text)				{ xml.ElementContent(text); }
	void					CDATA(const void *data, int len)				{ xml.CDATA(data, len); }
	void					Comment(const char *comment)					{ xml.Comment(comment); }
};

inline XMLelement XMLwriter::Element(const char *name) {
	return XMLelement(*this, name);
}

//-----------------------------------------------------------------------------
//	XML DOM
//-----------------------------------------------------------------------------

struct XMLDOM_value {
	string	name, value;
	XMLDOM_value(const char *name, const char *value) : name(name), value(value)	{}
	const char*		Name()				const	{ return name;		}
	const char*		Value()				const	{ return value;		}
	void			Set(const char *_text)		{ value = _text;	}
};

struct XMLDOM_node : public XMLDOM_value, public dynamic_array<XMLDOM_node*> {
	dynamic_array<XMLDOM_value>	attrs;

	void	Append(const char *name, const char *value)	{ attrs.emplace_back(name, value);	}
	void	Append(XMLDOM_node *node)					{ push_back(node);							}

	size_t			NumAttrs()			const	{ return attrs.size();	}
	size_t			NumNodes()			const	{ return size();		}
	XMLDOM_value*	GetAttr(int i)		const	{ return i < attrs.size() ? &attrs[i] : NULL;	}
	XMLDOM_node*	GetNode(int i)		const	{ return i < size() ? (*this)[i] : NULL;	}
	const char*		GetAttr(const char *s) const;
	XMLDOM_node*	GetNode(const char *s) const;

	XMLDOM_node(const char *_name) : XMLDOM_value(_name, 0)	{}
	~XMLDOM_node() {
		for (size_t i = 0, n = size(); i < n; i++)
			delete (*this)[i];
	}
};

class XMLDOM0 {
	XMLDOM_node	*node;
	static bool	_write(XMLwriter &xml, XMLDOM_node *node);
public:
	XMLDOM0() : node(NULL)										{}
	XMLDOM0(const XMLDOM0 &b)	: node(b.node)					{}
	XMLDOM0(XMLDOM_node *node)	: node(node)					{}
	XMLDOM0(const char *name)	: node(new XMLDOM_node(name))	{}

	size_t		NumAttrs()						const	{ return node ? node->NumAttrs() : 0;	}
	size_t		NumNodes()						const	{ return node ? node->NumNodes() : 0;	}
	XMLDOM0		operator/(const char *s)		const	{ return XMLDOM0(node ? node->GetNode(s) : node);	}
	XMLDOM0		operator/(int i)				const	{ return XMLDOM0(node ? node->GetNode(i) : node);	}
	const char*	operator+(const char *s)		const	{ return node ? node->GetAttr(s) : NULL;}
	XMLDOM_node*operator->()					const	{ return node; }
	operator	const char*()					const	{ return node ? node->Value() : NULL;	}
	const char*	Value()							const	{ return node ? node->Value() : NULL;	}
	operator	XMLDOM_node*()					const	{ return node; }

	XMLDOM0&	AddNode(XMLDOM0 dom0)					{ node->Append(dom0); return *this; }
	XMLDOM0&	AddAttr(const char *name, const char *value)	{ node->Append(name, value); return *this; }
	XMLDOM0&	SetValue(const char *text)				{ node->Set(text); return *this;	}
	template<typename T> XMLDOM0&	SetValue(const T &t){ return SetValue(to_string(t));	}

	struct iterator {
		XMLDOM_node**	i;
		iterator(XMLDOM_node** i) : i(i)		{}
		XMLDOM0	operator*()						const	{ return XMLDOM0(*i); }
		bool	operator!=(const iterator &j)	const	{ return i != j.i; }
		void	operator++()							{ ++i; }
	};
	iterator	begin()	{ return node ? node->begin()	: 0; }
	iterator	end()	{ return node ? node->end()		: 0; }

	bool		write(ostream_ref file)			const	{ return _write(XMLwriter(file, true).me(), *this); }
};

class XMLDOM : public XMLDOM0 {
public:
	XMLDOM() : XMLDOM0(new XMLDOM_node("xml"))			{}
	XMLDOM(istream_ref file);
	~XMLDOM()											{ delete (XMLDOM_node*)*this; }
	bool		write(ostream_ref file)			const	{ return XMLDOM0::write(file); }
};

}	// namespace iso
#endif	//XML_H
