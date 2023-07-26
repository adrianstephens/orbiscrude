#include "extra/xml.h"
#include "extra/date.h"
#include "iso/iso_files.h"

#undef OPTIONAL

using namespace iso;

//-----------------------------------------------------------------------------
//	XML
//-----------------------------------------------------------------------------

class XMLFileHandler : public FileHandler {
protected:
	void	Read(XMLreader &xml, XMLreader::Data &data, ISO_ptr<anything_machine> &top);
	void	Write(XMLwriter &xml, ISO::Browser2 b);

	const char*		GetExt() override {
		return "xml";
	}
	int				Check(istream_ref file) override {
		file.seek(0);
		float	ver = XMLreader(file).CheckVersion();
		return	ver < 0 ? CHECK_DEFINITE_NO
			:	ver > 0 ? CHECK_PROBABLE
			:	CHECK_POSSIBLE;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		XMLreader				xml(file);
		XMLreader::Data	data;
		xml.SetFlag(XMLreader::UNQUOTEDATTRIBS);
		try {
			ISO_ptr<anything_machine>	p(id);
			Read(xml, data, p);
			return p;
		} catch (const char *error) {
			throw_accum(error << " at line " << xml.GetLineNumber());
			return ISO_NULL;
		}
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		if (p.GetType()->SameAs<anything>() || TypeType(p.GetType()->SkipUser()) == ISO::VIRTUAL) {
			XMLwriter	w(file, true);
			XMLelement(w, "?xml")
				.Attribute("version", "1.0")
				.Attribute("encoding", "UTF-8")
				.Attribute("standalone", "no");
			Write(w, ISO::Browser2(p));
			return true;
		}
		return false;
	}
} xml;

void XMLFileHandler::Read(XMLreader &xml, XMLreader::Data &data, ISO_ptr<anything_machine> &top) {
	while (XMLreader::TagType tag = xml.ReadNext(data)) {
		switch (tag) {
			case XMLreader::TAG_BEGIN: case XMLreader::TAG_BEGINEND: {
				ISO_ptr<anything_machine>	p(data.Name());
				top->Append(p);

				for (auto &i : data.Attributes())
					p->Append(ISO_ptr<string>(i.name, i.value));

				if (tag == XMLreader::TAG_BEGIN) {
					string	start(data.Name());
					Read(xml, data, p);
					if (istr(data.Name()) != start)
						return;
				}
				break;
			}
			case XMLreader::TAG_END:
				return;

			case XMLreader::TAG_CONTENT:
				top->Append(ISO_ptr<string>(NULL, data.Name()));
				break;
		}
	}
}
/*
void XMLFileHandler::Write(XMLwriter &xml, anything *a) {
	for (int i = 0, n = a->Count(); i < n; i++) {
		ISO_ptr<void>	p = (*a)[i];
		if (tag2 id = p.ID()) {
			const ISO::Type*	type = p.GetType()->SkipUser();
			switch (type->GetType()) {
				case ISO::OPENARRAY:
					xml.ElementBegin(tag(id));
					Write(xml, p);
					xml.ElementEnd(tag(id));
					break;
				case ISO::INT:
					xml.Attribute(tag(id), ISO::Browser(p).GetInt());
					break;
				case ISO::FLOAT:
					xml.Attribute(tag(id),  ISO::Browser(p).GetFloat());
					break;
				default:
					xml.Attribute(tag(id), *(char**)p);
					break;
			}
		} else if (p.GetType()->SameAs<string>()) {
			xml.ElementContent(*(char**)p);
		}
	}
}
*/
void XMLFileHandler::Write(XMLwriter &xml, ISO::Browser2 b) {
	for (int i = 0, n = b.Count(); i < n; i++) {
		ISO::Browser2	b2 = b[i];
		while (b2.GetType() == ISO::REFERENCE && !b2.External())
			b2 = *b2;

		tag2 id = b.GetName(i);
		if (id || b2.GetType() == ISO::USER) {
			const ISO::Type*	type = b2.GetTypeDef()->SkipUser();
			switch (TypeType(type)) {
				default:
					if (b2.GetType() == ISO::USER && !b2.Is<anything>()) {
						tag2	id2		= ((const ISO::TypeUser*)b2.GetTypeDef())->ID();
						xml.ElementBegin(id2.get_tag());
						xml.Attribute("name", id.get_tag());
						id = id2;
					} else {
						xml.ElementBegin(id.get_tag());
					}
					Write(xml, b2);
					xml.ElementEnd(id.get_tag());
					break;
				case ISO::INT:
					xml.Attribute(id.get_tag(), b2.GetInt());
					break;
				case ISO::FLOAT:
					xml.Attribute(id.get_tag(),  b2.GetFloat());
					break;
				case ISO::STRING:
					xml.Attribute(id.get_tag(), b2.GetString());
					break;
			}
		} else if (b2.Is<string>()) {
			xml.ElementContent(b2.GetString());
		} else {
			Write(xml, b2);
		}
	}
}

//-----------------------------------------------------------------------------
//	HTML
//-----------------------------------------------------------------------------

class HTMLFileHandler : public FileHandler {
	struct Reader {
		XMLreader				&xml;
		XMLreader::Data	data;
		string					base;
		void	Read(ISO_ptr<anything> &top);
		Reader(XMLreader &_xml) : xml(_xml) {}
	};

	void	Write(XMLwriter &xml, ISO::Browser2 b);

	const char*		GetExt() override { return "html";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		float	ver = XMLreader(file).CheckVersion();
		if (ver < 0)
			return CHECK_DEFINITE_NO;
		return ver > 0 ? CHECK_NO_OPINION : CHECK_POSSIBLE;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		XMLreader				xml(file);
		xml.SetFlag(XMLreader::UNQUOTEDATTRIBS);
		xml.SetFlag(XMLreader::NOEQUALATTRIBS);
		xml.SetFlag(XMLreader::SKIPUNKNOWNENTITIES);
		xml.SetFlag(XMLreader::SKIPBADNAMES);
		xml.SetFlag(XMLreader::RELAXED);
		try {
			ISO_ptr<anything>	p(id);
			Reader(xml).Read(p);
			return p;
		} catch (const char *error) {
			throw_accum(error << " at line " << xml.GetLineNumber());
			return ISO_NULL;
		}
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		if (p.GetType()->SameAs<anything>() || TypeType(p.GetType()->SkipUser()) == ISO::VIRTUAL) {
			XMLwriter	w(file, true);
			Write(w, ISO::Browser2(p));
			return true;
		}
		return false;
	}
} html;

class HTMFileHandler : public HTMLFileHandler {
	const char*		GetExt() override { return "htm";	}
	int				Check(istream_ref file) override { return CHECK_DEFINITE_NO;	}
} htm;

//closing tags optional
const char *close_optional[] = {
	"HTML",		"HEAD",		"BODY",		"P",
	"DT",		"DD",		"LI",		"OPTION",
	"THEAD",	"TH",		"TBODY",	"TR",
	"TD",		"TFOOT",	"COLGROUP",
};

//closing tags forbidden
const char *close_forbidden[] = {
	"IMG",		"INPUT",	"BR",		"HR",
	"FRAME",	"AREA",		"BASE",		"BASEFONT",
	"COL",		"ISINDEX",	"LINK",		"META",
	"PARAM",
};

void HTMLFileHandler::Reader::Read(ISO_ptr<anything> &top) {
	while (XMLreader::TagType tag = xml.ReadNext(data)) {
		switch (tag) {
			case XMLreader::TAG_BEGIN: case XMLreader::TAG_BEGINEND: {
				if (data.Is("a")) {
					ISO_ptr<void>	p;
					p.CreateExternal(filename(base).relative(data.Find("href")));
					top->Append(p);

					if ((tag = xml.ReadNext(data)) == XMLreader::TAG_CONTENT)
						p.SetID(data.Name());
					xml.ReadNext(data, XMLreader::TAG_END);
					break;
				} else if (data.Is("base")) {
					base	= data.Find("href");
					break;
				}

				ISO_ptr<anything>	p(data.Name());
				top->Append(p);
				for (auto &i : data.Attributes())
					p->Append(ISO_ptr<string>(i.name, i.value));

				if (tag == XMLreader::TAG_BEGIN) {
					bool	close = true;
					if (data.Is("script")) {
						string_builder	b;
						int		c;
						for (; (c = xml.getc()) != -1;) {
							b.putc(c);
							if (c == '>') {
								if (str(b).slice(-9) == "</script>")
									break;
							}
						}
						if (str(b).length() > 9)
							p->Append(ISO_ptr<string>("script", str(b).slice(0, -9)));
						break;

					} else {
						for (int i = 0; close && i < num_elements(close_forbidden); i++) {
							if (data.Name() == istr(close_forbidden[i]))
								close = false;
						}
					}

					if (close) {
						string	start(data.Name());
						Read(p);
						if (istr(data.Name()) != start)
							return;
					}
				}
				break;
			}
			case XMLreader::TAG_END:
				return;

			case XMLreader::TAG_CONTENT:
				top->Append(ISO_ptr<string>(NULL, data.Name()));
				break;

			default:
				break;
		}
	}
}

void HTMLFileHandler::Write(XMLwriter &xml, ISO::Browser2 b) {
	for (int i = 0, n = b.Count(); i < n; i++) {
		ISO::Browser2	b2 = b[i];
		while (b2.GetType() == ISO::REFERENCE && !b2.External())
			b2 = *b2;
		if (b2.External()) {
			if (!b2.IsPtr())
				b2 = *b2;
			xml.ElementBegin("a");
			xml.Attribute("href",  (char*)b2);
			xml.ElementEnd("a");
			continue;
		}

		tag2 id = b.GetName(i);
		if (id || b2.GetType() == ISO::USER) {
			const ISO::Type*	type = b2.GetTypeDef()->SkipUser();
			switch (TypeType(type)) {
				default:
					if (b2.GetType() == ISO::USER && !b2.Is<anything>()) {
						tag2	id2		= ((const ISO::TypeUser*)b2.GetTypeDef())->ID();
						xml.ElementBegin(id2.get_tag());
						xml.Attribute("name", id.get_tag());
						id = id2;
					} else {
						xml.ElementBegin(id.get_tag());
					}
					Write(xml, b2);
					xml.ElementEnd(id.get_tag());
					break;
				case ISO::INT:
					xml.Attribute(id.get_tag(), b2.GetInt());
					break;
				case ISO::FLOAT:
					xml.Attribute(id.get_tag(),  b2.GetFloat());
					break;
				case ISO::STRING:
					xml.Attribute(id.get_tag(), b2.GetString());
					break;
			}
		} else if (b2.Is<string>()) {
			xml.ElementContent(b2.GetString());
		} else {
			Write(xml, b2);
		}
	}
}
//-----------------------------------------------------------------------------
//	XSD
//-----------------------------------------------------------------------------

/*
schema
	annotation
		documentation
		appinfo
	simpleType
		annotation
		restriction
			pattern
	complexType
		annotation
		attribute
			annotation
		simpleContent
		choice
		all
		element

	attributeGroup
		attribute
	group
		choice
			element
			sequence
	element
		annotation
		complexType
*/

struct XSD {
	enum TYPE {
		TYPE_anyType,

		//strings
		TYPE_string,
			TYPE_normalizedString,			//Newline, tab and carriage-return characters in a normalizedString type are converted to space characters before schema processing
				TYPE_token,					//As normalizedString, and adjacent space characters are collapsed to a single space character, and leading and trailing spaces are removed
					TYPE_language,			//e.g. en-GB, en-US, fr	- valid values for xml:lang as defined in XML 1.0
					TYPE_NMTOKEN,			//XML 1.0 NMTOKEN attribute type - attributes only
					TYPE_Name,				//XML 1.0 Name type
						TYPE_NCName,		//XML Namespace NCName, i.e. a QName without the prefix and colon
							TYPE_ID,		//XML 1.0 ID attribute type - attributes only
							TYPE_IDREF,		//XML 1.0 IDREF attribute type - attributes only
							TYPE_ENTITY,	//XML 1.0 ENTITY attribute type - attributes only
		TYPE_QName,							//anyURI:NCName
		TYPE_NOTATION,						//XML 1.0 NOTATION attribute type - attributes only
		TYPE_anyURI,

		//binary data
		TYPE_base64Binary,
		TYPE_hexBinary,

		//numbers
		TYPE_decimal,
			TYPE_integer,
				TYPE_nonNegativeInteger,
					TYPE_positiveInteger,
					TYPE_unsignedLong,
						TYPE_unsignedInt,
							TYPE_unsignedShort,
								TYPE_unsignedByte,
				TYPE_nonPositiveInteger,
					TYPE_negativeInteger,
			TYPE_long,
				TYPE_int,
					TYPE_short,
						TYPE_byte,
		TYPE_boolean,						//true, false, 1, 0
		TYPE_double,
		TYPE_float,							//NaN is "not a number", (+/-)INF is infinity

		//date-time
		TYPE_date,
		TYPE_dateTime,
		TYPE_time,
		TYPE_duration,			//P1Y2M3DT10H30M12.3S would be 1 year, 2 months, 3 days, 10 hours, 30 minutes, and 12.3 seconds
		TYPE_gDay,
		TYPE_gMonth,
		TYPE_gMonthDay,
		TYPE_gYear,
		TYPE_gYearMonth,

		TYPE_restrict,
		TYPE_list,
		TYPE_union,
		TYPE_complex,

		//builtin list types
		//	ENTITIES			//XML 1.0 ENTITIES attribute type - attributes only
		//	IDREFS				//XML 1.0 IDREFS attribute type - attributes only
		//	NMTOKENS			//XML 1.0 NMTOKENS attribute type, i.e. a whitespace separated list of NMTOKEN's - attributes only
	};

	struct NumRestriction {
		uint32 n:31; bool fixed:1, set:1;
		NumRestriction() : set(false) {}
		void	read(XMLreader::Data &data) {
			n		= data["value"];
			fixed	= data["fixed"];
			set		= true;
		}
	};
	struct LengthRestriction {
		NumRestriction	min, max;
		void	read(XMLreader::Data &data) {
			min.read(data);
			max = min;
		}
	};
	struct RegExRestriction {
		string re;
		RegExRestriction() {}
		RegExRestriction(const char *s) : re(s) {}
	};
	struct WhitespaceRestriction {
		enum WHITESPACE {PRESERVE, REPLACE, COLLAPSE};
		WHITESPACE	val:2; bool fixed:1;
		void	read(XMLreader::Data &data) {
			val		= data["value"];
			fixed	= data["fixed"];
		}
	};
	struct EnumerationRestriction : dynamic_array<string> {};

	template<typename T> struct RangeRestriction {
		T val;
		bool inclusive:1, fixed:1, set:1;
		RangeRestriction() : set(false) {}
		RangeRestriction(T val, bool inclusive) : val(val), inclusive(inclusive), set(true) {}
	};

	struct Annotation {
		string	documentation, appinfo;
		Annotation(XMLiterator &it) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("xs::documentation")) {
					documentation = it.Content();
				} else if (it.data.Is("xs::appinfo")) {
					appinfo = it.Content();
				}
			}
		}
	};

	struct Attribute {
		enum USE { REQUIRED, OPTIONAL, PROHIBITED } use;
		string		name, type;
		unique_ptr<Annotation>		annotation;
		Attribute(XMLiterator &it) : name(it.data["name"]), type(it.data["type"]), use(it.data.Get("use", OPTIONAL)) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("xs::annotation"))
					annotation = new Annotation(it);
			}
		}
	};

	struct Type {
		TYPE	type;
		Type(TYPE type) : type(type) {}
		virtual Type *read(XMLiterator &it) = 0;
	};

	template<TYPE T> struct TypeT : Type { TypeT() : Type(T) {} };

	hash_map<string, Type*>	types;

	struct TypeDecimal : TypeT<TYPE_decimal> {
		NumRestriction				totalDigits, fractionDigits;
		RegExRestriction			pattern;
		EnumerationRestriction		enumeration;
		WhitespaceRestriction		whiteSpace;
		RangeRestriction<double>	min, max;

		TypeDecimal(TypeDecimal *super, XMLiterator &it) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("xs::totalDigits"))
					totalDigits.read(it.data);
				else if (it.data.Is("xs::fractionDigits"))
					fractionDigits.read(it.data);
				else if (it.data.Is("xs::pattern"))
					pattern = (const char*)it.data["value"];
				else if (it.data.Is("xs::enumeration"))
					enumeration.push_back(it.data["value"]);
				else if (it.data.Is("xs::whiteSpace"))
					whiteSpace.read(it.data);
				else if (it.data.Is("xs::minInclusive"))
					min = {it.data["value"], true};
				else if (it.data.Is("xs::minExclusive"))
					min = {it.data["value"], false};
				else if (it.data.Is("xs::maxInclusive"))
					max = {it.data["value"], true};
				else if (it.data.Is("xs::maxExclusive"))
					max = {it.data["value"], false};
			}
		}
		Type* read(XMLiterator &it) { return new TypeDecimal(this, it); }
	};

	struct TypeString : TypeT<TYPE_string> {
		RegExRestriction			pattern;
		EnumerationRestriction		enumeration;
		WhitespaceRestriction		whiteSpace;
		LengthRestriction			len;

		TypeString(TypeString *super, XMLiterator &it) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("xs::pattern"))
					pattern = (const char*)it.data["value"];
				else if (it.data.Is("xs::enumeration"))
					enumeration.push_back(it.data["value"]);
				else if (it.data.Is("xs::whiteSpace"))
					whiteSpace.read(it.data);
				else if (it.data.Is("xs::minLength"))
					len.min.read(it.data);
				else if (it.data.Is("xs::maxLength"))
					len.max.read(it.data);
				else if (it.data.Is("xs::length"))
					len.read(it.data);
			}
		}
		Type* read(XMLiterator &it) { return new TypeString(this, it); }
	};

	struct TypeBinary : TypeT<TYPE_hexBinary> {
		RegExRestriction			pattern;
		EnumerationRestriction		enumeration;
		WhitespaceRestriction		whiteSpace;
		LengthRestriction			len;

		TypeBinary(TypeBinary *super, XMLiterator &it) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("xs::pattern"))
					pattern = (const char*)it.data["value"];
				else if (it.data.Is("xs::enumeration"))
					enumeration.push_back(it.data["value"]);
				else if (it.data.Is("xs::whiteSpace"))
					whiteSpace.read(it.data);
				else if (it.data.Is("xs::minLength"))
					len.min.read(it.data);
				else if (it.data.Is("xs::maxLength"))
					len.max.read(it.data);
				else if (it.data.Is("xs::length"))
					len.read(it.data);
			}
		}
		Type* read(XMLiterator &it) { return new TypeBinary(this, it); }
	};

	template<TYPE type, typename T> struct TypeRanged : TypeT<type> {
		RegExRestriction			pattern;
		EnumerationRestriction		enumeration;
		WhitespaceRestriction		whiteSpace;
		RangeRestriction<T>			min, max;

		TypeRanged(TypeRanged *super, XMLiterator &it) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("xs::pattern"))
					pattern = (const char*)it.data["value"];
				else if (it.data.Is("xs::enumeration"))
					enumeration.push_back(it.data["value"]);
				else if (it.data.Is("xs::whiteSpace"))
					whiteSpace.read(it.data);
				else if (it.data.Is("xs::minInclusive"))
					min = {it.data["value"], true};
				else if (it.data.Is("xs::minExclusive"))
					min = {it.data["value"], false};
				else if (it.data.Is("xs::maxInclusive"))
					max = {it.data["value"], true};
				else if (it.data.Is("xs::maxExclusive"))
					max = {it.data["value"], false};
			}
		}
		Type* read(XMLiterator &it) { return new TypeRanged(this, it); }
	};


	typedef TypeRanged<TYPE_float, float>		TypeFloat;
	typedef TypeRanged<TYPE_double, double>		TypeDouble;
	typedef TypeRanged<TYPE_dateTime, ISO_8601> TypeDataTime;

	struct TypeRestrict : TypeT<TYPE_restrict> {
		Type	*super, *type;
		TypeRestrict(Type *super, XMLiterator &it) : super(super) {
			type = super->read(it);
		}
		Type* read(XMLiterator &it) { return new TypeRestrict(this, it); }
	};

	struct SimpleType {
		string		name;
		Type		*type;
		unique_ptr<Annotation>	annotation;

		SimpleType(XSD *xsd, XMLiterator &it) : name(it.data.Find("name")) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("xs::annotation")) {
					annotation = new Annotation(it);

				} else if (it.data.Is("xs::restriction")) {
					if (auto base = it.data.Find("base")) {
						auto t = xsd->types[base];
						if (t.exists()) {
							type = new TypeRestrict(t, it);

						} else if (base == "xs:string") {
							type = new TypeString(nullptr, it);

						} else if (base == "xs::decimal") {
							type = new TypeDecimal(nullptr, it);

						}//etc
					}
				}
			}
		}

	};

	struct ComplexType : TypeT<TYPE_complex> {
		string		name;
		bool		mixed;
		unique_ptr<Annotation>		annotation;
		dynamic_array<Attribute*>	attributes;

		ComplexType(XSD *xsd, XMLiterator &it) : name(it.data.Find("name")), mixed(it.data.Get("mixed", false)) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("xs::annotation")) {
					annotation = new Annotation(it);
				} else if (it.data.Is("xs::attribute")) {
					attributes.push_back(new Attribute(it));
				} else if (it.data.Is("xs::simpleContent")) {
				} else if (it.data.Is("xs::complexContent")) {
				} else if (it.data.Is("xs::choice")) {
				} else if (it.data.Is("xs::all")) {
				} else if (it.data.Is("xs::element")) {
				}
			}
		}
		Type* read(XMLiterator &it) { return nullptr; }
	};
};

template<> const char *field_names<XSD::WhitespaceRestriction::WHITESPACE>::s[] = {
	"preserve",
	"replace",
	"collapse"
};

template<> const char *field_names<XSD::Attribute::USE>::s[] = {
	"required",
	"optional",
	"prohibited"
};

class XSDFileHandler : public FileHandler {

	void	DumpType(XMLwriter &xml, const ISO::Type *type, const char *name, bool def);

	const char*		GetExt() override { return "xsd";	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override;

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		XMLwriter	xml(file, true);
		xml.ElementBegin("xs:schema");
		xml.Attribute("xmlns:xs", "http://www.w3.org/2001/XMLSchema");
		for (auto &i : ISO::user_types) {
			if ((i->flags & ISO::TypeUser::FROMFILE) && i->subtype) {
				DumpType(xml, i->subtype, i->id.get_tag(), true);
			}
		}
		xml.ElementEnd("xs:schema");
		return true;
	}
public:
	XSDFileHandler()		{}
} xsd_file;



ISO_ptr<void>	XSDFileHandler::Read(tag id, istream_ref file) {
	XMLreader::Data	data;
	XMLreader				xml(file);

	XMLiterator it(xml, data);
	if (!it.Next())
		return ISO_NULL;

	dynamic_array<count_string>	matches;
	auto	ns = data.FindWild("xmlns:*", matches);
	xml.AddNamespace(matches[0], ns);

	XSD	xsd;

	if (data.Is("xs:schema")) {
		for (it.Enter(); it.Next();) {
			if (data.Is("xs::annotation")) {
			} else if (data.Is("xs::simpleType")) {
				auto	t = new XSD::SimpleType(&xsd, it);
				xsd.types[t->name] = t->type;

			} else if (data.Is("xs::complexType")) {
				auto	t = new XSD::ComplexType(&xsd, it);
				xsd.types[t->name] = t;

			} else if (data.Is("xs::attributeGroup")) {
			} else if (data.Is("xs::group")) {
			} else if (data.Is("xs::element")) {
			}
		}
	}

	return ISO_ptr<int>(0);
}

void XSDFileHandler::DumpType(XMLwriter &xml, const ISO::Type *type, const char *name, bool def) {
	switch (TypeType(type)) {
		case ISO::INT:
			if (def) {
				ISO::TypeInt	*i = (ISO::TypeInt*)type;
				XMLelement	st(xml, "xs:simpleType");
				xml.Attribute("name", name);
				XMLelement	res(xml, "xs:restriction");
				xml.Attribute("base", "xs:integer");
				XMLelement(xml, "xs:minInclusive").Attribute("value", to_string(i->get_min()));
				XMLelement(xml, "xs:maxInclusive").Attribute("value", to_string(i->get_max()));
			} else {
				xml.Attribute("type", "xs:integer");
			}
			break;

		case ISO::FLOAT: {
			ISO::TypeFloat	*f = (ISO::TypeFloat*)type;
			xml.Attribute("type", f->num_bits() <= 32 ? "xs:float" : "xs:double");
			break;
		}

		case ISO::STRING:
			xml.Attribute("type", "xs:string");
			break;

		case ISO::COMPOSITE: {
			XMLelement	com(xml, "xs:complexType");
			if (def)
				xml.Attribute("name", name);
			XMLelement	seq(xml, "xs:sequence");
			ISO::TypeComposite	&comp	= *(ISO::TypeComposite*)type;
			char				buffer[256];
			int					u		= 0;
			for (auto &e : comp) {
				const char	*id = comp.GetID(&e).get_tag();
				if (!id) {
					sprintf(buffer, "_unnamed%i", u++);
					id = buffer;
				}
				DumpType(XMLelement(xml, "xs:element").Attribute("name", id), e.type, NULL, false);
			}
			break;
		}

		case ISO::ARRAY: {
			ISO::TypeArray	*a = (ISO::TypeArray*)type;
			switch (TypeType(SkipUser(a->subtype))) {
				case ISO::INT:
				case ISO::FLOAT:
				case ISO::STRING:
					XMLelement	st(xml, "xs:simpleType");
					xml.Attribute("name", name);
					XMLelement	list(xml, "xs:list");
					xml.Attribute("itemType", "integer");
					break;
			}

			break;
		}
		case ISO::OPENARRAY:
		case ISO::REFERENCE:
			break;

		case ISO::USER: {
			ISO::TypeUser	*user = (ISO::TypeUser*)type;
			if (def)
				DumpType(xml, user->subtype, user->ID().get_tag(), true);
			else
				xml.Attribute("type", user->ID().get_tag());
			break;
		}
	}
}
