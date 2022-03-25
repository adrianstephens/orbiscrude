#include "json_schema.h"
#include "base/hash.h"
#include "base/maths.h"
#include "regex.h"
#include "date.h"

//for tests
#include "directory.h"
#include "codec/base64.h"

namespace iso {

namespace json_schema {

//-----------------------------------------------------------------------------
//	format checkers
//-----------------------------------------------------------------------------

const auto non_neg_int	= "0|[1-9][0-9]*"_re;
const auto decOctet		= "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"_re; // matches numbers 0-255
const auto ipv4Address	= (decOctet + ".") * 3 + decOctet;
const auto h16			= "[0-9A-Fa-f]{1,4}"_re;
const auto h16Left		= h16 + ":";

const auto ipv6Address	=
(
	(													h16Left * 6	)
	|	( 											"::" +	h16Left * 5	)
	|	( maybe(h16)							+	"::" +	h16Left * 4	)
	|	( maybe(between(h16Left, 0, 1) + h16)	+	"::" +	h16Left * 3	)
	|	( maybe(between(h16Left, 0, 2) + h16)	+	"::" +	h16Left * 2	)
	|	( maybe(between(h16Left, 0, 3) + h16)	+	"::" +	h16Left		)
	|	( maybe(between(h16Left, 0, 4) + h16)	+	"::"				)
	)
	+ (h16Left + (h16 | ipv4Address))
	| (maybe(between(h16Left, 0, 5) + h16) + "::" + h16)
	| (maybe(between(h16Left, 0, 6) + h16) + "::")
	;

const auto ipvFuture	= R"([Vv][0-9A-Fa-f]+\.[A-Za-z0-9\-._~!$&'()*+,;=:]+)"_re;
const auto regName		= R"((?:[A-Za-z0-9\-._~!$&'()*+,;=]|%[0-9A-Fa-f]{2})*)"_re;
const auto host			= "[" + (ipv6Address | ipvFuture) + "]" | ipv4Address | regName;

const auto atom			= +re2::regex(char_set::alphanum | char_set("!#$%&'*+/=?^_`{|}~-"));
const auto dottedAtoms	= atom + *("." + atom);

const auto domainpart	= char_set::alphanum + maybe(between(re2::regex(char_set::alphanum|'-'), 0, 61) + char_set::alphanum);
const auto domain		= domainpart + *("." + domainpart);

const auto email		= dottedAtoms + "@" + domain;

const auto hostname		= re2::begin_text + domain + re2::end_text;


const auto json_token	= *"[^~/]|~[01]"_re;
const auto json_pointer	= *("/" + json_token);
const auto rel_json_pointer	= non_neg_int + ("#" | json_pointer);

template <typename T> void range_check(const JSONpath &path, const T value, const T min, const T max, const error_handler &e) {
	if (!between(value, min, max))
		e(path, buffer_accum<256>() << "Value " << value << " should be in interval [" << min << "," << max << "] but is not!");
}

void rfc3339_date_check(const JSONpath &path, const count_string &value, const error_handler &e) {
	const static auto dateRegex = R"(^([0-9]{4})\-([0-9]{2})\-([0-9]{2})$)"_re;

	dynamic_array<count_string> matches;
	if (!dateRegex.match(value, matches)) {
		e(path, "not a date string according to RFC 3339");
		return;
	}

	if (!Date::IsValid(from_string<int>(matches[1]), from_string<int>(matches[2]), from_string<int>(matches[3])))
		e(path, "not a valid date");
}

void rfc3339_time_check(const JSONpath &path, const count_string &value, const error_handler &e) {
	const static auto timeRegex	= R"(^([0-9]{2})\:([0-9]{2})\:([0-9]{2})(\.[0-9]+)?(?:[Zz]|((?:\+|\-)[0-9]{2})\:([0-9]{2}))$)"_re;

	dynamic_array<count_string> matches;
	if (!timeRegex.match(value, matches)) {
		e(path, "not a time string according to RFC 3339.");
		return;
	}

	if (!TimeOfDay::IsValid(from_string<int>(matches[1]), from_string<int>(matches[2]), from_string<int>(matches[3])))
		e(path, "not a valid time");

	// don't check the numerical offset if time zone is specified as 'Z'
	if (matches[5]) {
		range_check(path, from_string<int>(matches[5]), -23, 23, e);
		range_check(path, from_string<int>(matches[6]), 0, 59, e);
	}
}

format_checker checkers[] = {
{"date-time",	[](const JSONpath &path, const count_string &value, const error_handler &e) {
	const static re2::regex dateTime = R"(^([0-9]{4}\-[0-9]{2}\-[0-9]{2})[Tt]([0-9]{2}\:[0-9]{2}\:[0-9]{2}(?:\.[0-9]+)?(?:[Zz]|(?:\+|\-)[0-9]{2}\:[0-9]{2}))$)"_re;
	dynamic_array<count_string> matches;
	if (!dateTime.match(value, matches)) {
		e(path, "not a date-time string according to RFC 3339.");
		return;
	}
	rfc3339_date_check(path, matches[1], e);
	rfc3339_time_check(path, matches[2], e);
}},
{"date",		rfc3339_date_check},
{"time",		rfc3339_time_check},
{"email",		[](const JSONpath &path, const count_string &value, const error_handler &e) {
	if (!email.match(value))
		e(path, value + " is not a valid email according to RFC 5322.");
}},
{"hostname",	[](const JSONpath &path, const count_string &value, const error_handler &e) {
	if (!hostname.match(value))
		e(path, value + " is not a valid hostname according to RFC 3986 Appendix A.");
}},
{"ipv4",		[](const JSONpath &path, const count_string &value, const error_handler &e) {
	if (!ipv4Address.match(value))
		e(path, value + " is not an IPv4 string according to RFC 2673.");
}},
{"ipv6",		[](const JSONpath &path, const count_string &value, const error_handler &e) {
	if (!ipv6Address.match(value))
		e(path, value + " is not an IPv6 string according to RFC 5954.");
}},
{"regex",		[](const JSONpath &path, const count_string &value, const error_handler &e) {
	re2::regex re(value, re2::ECMAScript);
	if (!re)
		e(path, value + " is not an ECMA-262 regex.");
}},
{"json-pointer",			[](const JSONpath &path, const count_string &value, const error_handler &e) {
	if (!json_pointer.match(value))
		e(path, value + " is not an json-pointer according to RFC 6901.");
}},
{"relative-json-pointer",	[](const JSONpath &path, const count_string &value, const error_handler &e) {
	if (!rel_json_pointer.match(value))
		e(path, value + " is not an relative-json-pointer.");
}},
//{"idn-email",				format_check_tbd},
//{"idn-hostname",			format_check_tbd},
//{"uri",						format_check_tbd},
//{"uri-reference",			format_check_tbd},
//{"iri",						format_check_tbd},
//{"iri-reference",			format_check_tbd},
//{"uri-template",			format_check_tbd},
};

//-----------------------------------------------------------------------------
//	content checkers
//-----------------------------------------------------------------------------

content_checker content_checker_base64([](const JSONpath &path, content &c, const error_handler &e) {
	if (c.encoding == cstr("base64")) {
		char_set	cs("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=");
		if (auto p = string_find((const char*)c.data.begin(), ~cs, (const char*)c.data.end()))
			e(path, string("base64-decode: unexpected character in encode string:") + *p);

		malloc_block	temp = transcode(base64_decoder(), c.data);
//		malloc_block	temp(c.data.size * 6 / 8);
//		temp.resize(decode_base64(c.data, c.data.size, temp));
		c.data		= temp.detach();
		c.encoding	= 0;
	}
});

content_checker content_checker_json([](const JSONpath &path, content &c, const error_handler &e) {
	if (!c.encoding && c.mediaType == cstr("application/json")) {
		try {
			auto dummy = memory_reader(c.data).get<JSONval>();
		} catch (const char* error) {
			e(path, error);
		}
	}
});

//-----------------------------------------------------------------------------
//	schema reader & validator
//-----------------------------------------------------------------------------

struct first_error_handler {
	JSONpath 	path;
	string 		message;

	void operator()(const JSONpath &_path, const char *_message) {
		if (!message) {
			path 	= _path;
			message = _message;
		}
	}

	operator bool() const { return !!message; }
};

class schema_ref : public schema {
public:
	schema	 	*target;

	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		if (target)
			target->validate(validator, path, instance, e);
		else
			e(path, "unresolved");
	}

	const JSONval& defaultValue() const override {
		return target->defaultValue();
	}

	void walk(walker *w) override { target->walk(w); }

	schema_ref(schema *target = nullptr) : target(target) {}
};

class resolver {
public:

	struct schema_file {
		hash_map<string, schema*> 		schemas;
		hash_map<string, schema_ref*> 	unresolved; // contains all unresolved references from any other file seen during parsing
		JSONval							unknown_keywords;
	};

	hash_map_with_key<string, schema_file> 	files;	// location as key
	
	resolver() {}
	
	void load_remainder(const schema_loader &loader) {
		// load all files which have not yet been loaded
		for (bool new_schema_loaded = true; new_schema_loaded;) {
			new_schema_loaded = false;

			dynamic_array<string> locations = transformc(files, [](const pair<string, schema_file> &x) { return x.a; });
			for (auto &loc : locations) {
				if (!files[loc]->schemas.size()) { // nothing has been loaded for this file
					JSONval sch;
					loader(loc, sch);
					make_schema(sch, 0, {{loc + "#"}});
					new_schema_loaded = true;
				}
			}
		};
	}

	void insert(const json_uri &uri, schema *s) {
		auto	&file	= files[uri.location()].put();
		auto	anchor	= uri.anchor();

		file.schemas[anchor] = s;

		// was someone referencing this newly inserted schema?
		auto unresolved = file.unresolved[anchor];
		if (unresolved.exists()) {
			unresolved->target = s;
			unresolved.remove();
		}
	}

	void insert_unknown_keyword(const json_uri &uri, const string &key, JSONval &value) {
		auto &file 		= files[uri.location()].put();
		auto new_uri 	= uri.append(URLcomponents::Escape(key));
		auto fragment 	= new_uri.anchor();

		// is there a reference looking for this unknown-keyword, which is thus no longer an unknown keyword but a schema
		auto unresolved = file.unresolved.find(fragment);
		if (unresolved != file.unresolved.end())
			make_schema(value, 0, {{new_uri}});
		else // no, nothing ref'd it, keep for later
			JSONpath(fragment).set(file.unknown_keywords) = value;

		// recursively add possible subschemas of unknown keywords
		if (value.type == JSONval::OBJECT)
			for (auto &subsch : value.items())
				insert_unknown_keyword(new_uri, subsch.a, subsch.b);
	}

	schema* get_or_create_ref(const json_uri &uri) {
		auto	&file	= files[uri.location()].put();
		auto	anchor	= uri.anchor();

		// existing schema
		auto schema = file.schemas[anchor];
		if (schema.exists())
			return schema;

		// referencing an unknown keyword, turn it into schema
		// an unknown keyword can only be referenced by a JSON-pointer, not by a plain name fragment
		if (auto& subschema = JSONpath(anchor).get(file.unknown_keywords)) {
			if (auto s = make_schema(subschema, 0, {{uri}})) {	// A JSON Schema MUST be an object or a boolean.
				//subschema.remove();
				return s;
			}
		}

		// get or create a schema_ref
		auto r = file.unresolved[anchor];
		if (!r.exists())
			r = new schema_ref;
			
		return r.get();
	}

	schema *make_schema(const JSONval &sch, const char *key, dynamic_array<json_uri> uris);

};

schema* load(const JSONval& s, schema_loader&& loader) {
	resolver	res;
	schema	*sch = res.make_schema(s, 0, {{"#"}});
	res.load_remainder(loader);
	return sch;
}

class schema_not : public schema {
	friend walker;
	unique_ptr<schema> subschema;

	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		first_error_handler esub;
		subschema->validate(validator, path, instance, esub);
		if (!esub)
			e(path, "the subschema has succeeded, but it is required to not validate");
	}
	const JSONval& defaultValue() const override {
		return subschema->defaultValue();
	}
	void walk(walker *w) override;
public:
	schema_not(JSONval &sch, resolver *res, const dynamic_array<json_uri> &uris) : subschema(res->make_schema(sch, "not", uris)) {}
};

class schema_combination : public schema {
	friend walker;
protected:
	dynamic_array<unique_ptr<schema>> subschemata;
public:
	schema_combination(JSONval &sch, resolver *res, const dynamic_array<json_uri> &uris, const char *key) {
		size_t c = 0;
		for (auto &sub : sch)
			subschemata.push_back(res->make_schema(sub, string(key) + "/" + to_string(c++), uris));
	}
};

class schema_allOf : public schema_combination {
	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		for (schema *s : subschemata) {
			first_error_handler esub;
			s->validate(validator, path, instance, esub);
			if (esub)
				e(esub.path, "at least one subschema has failed, but all of them are required to validate - " + esub.message);
		}
	}
	void walk(walker *w);
public:
	schema_allOf(JSONval &sch, resolver *res, const dynamic_array<json_uri> &uris) : schema_combination(sch, res, uris, "allOf") {}
};

class schema_anyOf : public schema_combination {
	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		for (schema *s : subschemata) {
			first_error_handler esub;
			s->validate(validator, path, instance, esub);
			if (!esub)
				return;
		}
		e(path, "no subschema has succeeded, but one of them is required to validate");
	}
	void walk(walker *w);
public:
	schema_anyOf(JSONval &sch, resolver *res, const dynamic_array<json_uri> &uris) : schema_combination(sch, res, uris, "anyOf") {}
};

class schema_oneOf : public schema_combination {
	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		size_t count = 0;
		for (schema *s : subschemata) {
			first_error_handler esub;
			s->validate(validator, path, instance, esub);
			if (!esub && count++) {
				e(path, "more than one subschema has succeeded, but exactly one of them is required to validate");
				return;
			}
		}
		if (!count)
			e(path, "no subschema has succeeded, but one of them is required to validate");
	}
	void walk(walker *w);
public:
	schema_oneOf(JSONval &sch, resolver *res, const dynamic_array<json_uri> &uris) : schema_combination(sch, res, uris, "oneOf") {}
};


class schema_type : public schema {
	JSONval					defaultValue_;
	unique_ptr<schema>		type[JSONval::NUM_TYPES];
	JSONval 				enum_, const_;
	dynamic_array<unique_ptr<schema>> 	logic;
	unique_ptr<schema>		if_, then_, else_;

	static schema* make(const JSONval &schema, JSONval::TYPE type, resolver *, const dynamic_array<json_uri> &, hash_set<string> &);

	const JSONval& defaultValue() const override	{
		return defaultValue_;
	}

	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		// depending on the type of instance run the type specific validator - if present
		if (schema *t = type[instance.type]) {
			t->validate(validator, path, instance, e);
		} else {
			e(path, "unexpected instance type");
		}

		if (enum_) {
			bool found = false;
			for (auto &v : get(enum_))
				if (instance == v) {
					found = true;
					break;
				}

			if (!found)
				e(path, "instance not found in required enum");
		}

		if (const_ && get(const_) != instance)
			e(path, "instance not const");

		for (schema *l : logic)
			l->validate(validator, path, instance, e);

		if (if_) {
			first_error_handler esub;
			if_->validate(validator, path, instance, esub);
			if (!esub) {
				if (then_)
					then_->validate(validator, path, instance, e);
			} else {
				if (else_)
					else_->validate(validator, path, instance, e);
			}
		}
	}
	void walk(walker *w) override;

public:
	schema_type(const JSONval &sch, resolver *res, hash_set<string> &kw, const dynamic_array<json_uri> &uris) {
		static const char *schema_types[] = {
			0,
		    "null",
		    "boolean",
		    "integer",
		    "number",
		    "string",
		    "array",
		    "object",
		};
		
		kw.insert({"type", "default", "enum", "const", "not", "allOf", "anyOf", "oneOf", "if", "then", "else"});

		clear(type);
		if (auto& attr = sch/"type") {
			switch (attr.type) { // "type": "type"
				case JSONval::STRING: {
					auto schema_type = attr.get("");
					for (auto i : int_range_inc(JSONval::NIL, JSONval::OBJECT))
						if (str(schema_types[i]) == schema_type)
							type[i] = schema_type::make(sch, i, res, uris, kw);
					break;
				}

				case JSONval::ARRAY: // "type": ["type1", "type2"]
					for (auto &schema_type : attr)
						for (auto i : int_range_inc(JSONval::NIL, JSONval::OBJECT))
							if (str(schema_types[i]) == schema_type.get(""))
								type[i] = schema_type::make(sch, i, res, uris, kw);
					break;

				default:
					break;
			}
			
		} else { // no type field means all sub-types possible
			for (auto i : int_range_inc(JSONval::NIL, JSONval::OBJECT))
				type[i] = make(sch, i, res, uris, kw);
		}

		defaultValue_ 	= sch/"default";
		enum_ 			= sch/"enum";
		const_ 			= sch/"const";

		// with nlohmann::JSON float instance (but number in schema-definition) can be seen as unsigned or integer -
		// reuse the number-validator for integer values as well, if they have not been specified explicitly
		if (type[JSONval::FLOAT] && !type[JSONval::INT])
			type[JSONval::INT] = new schema_ref(type[JSONval::FLOAT]);


		if (auto& attr = sch/"not")
			logic.push_back(new schema_not(attr, res, uris));

		if (auto& attr = sch/"allOf")
			logic.push_back(new schema_allOf(attr, res, uris));

		if (auto& attr = sch/"anyOf")
			logic.push_back(new schema_anyOf(attr, res, uris));

		if (auto& attr = sch/"oneOf")
			logic.push_back(new schema_oneOf(attr, res, uris));

		if (auto& attr = sch/"if") {
			auto attr_then = sch/"then";
			auto attr_else = sch/"else";
			if (attr_then || attr_else) {
				if_ = res->make_schema(attr, "if", uris);
				if (attr_then)
					then_ = res->make_schema(attr_then, "then", uris);
				if (attr_else)
					else_ = res->make_schema(attr_else, "else", uris);
			}
		}
	}
};

class schema_string : public schema {
	optional<size_t> 	maxLength;
	optional<size_t> 	minLength;
	re2::regex 			pattern;
	optional<string>	patternString;
	optional<string> 	format;
	optional<string>	contentEncoding;
	optional<string>	contentMediaType;


	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		if (minLength.exists() || maxLength.exists()) {
			auto	len = chars_count<char32>(instance.get(""));
			if (minLength.exists() && len < minLength)
				e(path, format_string("instance is too short as per minLength:%i", minLength));
			if (maxLength.exists() && len > maxLength)
				e(path, format_string("instance is too long as per maxLength:%i", maxLength));
		}

		if (contentEncoding.exists() || contentMediaType.exists()) {
			content	c		= {instance.get<const_memory_block>(), contentEncoding.or_default(), contentMediaType.or_default()};
			const void	*orig_data	= c.data;
			if (c.encoding) {
				for (auto i : content_checker::all())
					i.func(path, c, e);
			}
			if (!c.encoding) {
				for (auto i : content_checker::all())
					i.func(path, c, e);
			}
			if (c.data != orig_data)
				free((void*)(const void*)c.data);

		} else if (instance.type == JSONval::BINARY) {
			e(path, "expected string, but get binary data");
		}

		if (patternString.exists() && !get(pattern).search(count_string(instance.get(""))))
			e(path, "instance does not match regex pattern: " + get(patternString));

		if (format.exists()) {
			if (auto *f = format_checker::find_by(get(format))) {
				(f->func)(path, count_string(instance.get("")), e);
			} else {
				e(path, "JSON schema string format not supported.");
			}
		}
	}
	void walk(walker *w);

public:
	schema_string(const JSONval &sch, resolver *res, hash_set<string> &kw) {
		kw.insert({"maxLength", "minLength", "pattern", "format", "contentEncoding", "contentMediaType"});
		
		maxLength = (sch/"maxLength").get<size_t>();
		minLength = (sch/"minLength").get<size_t>();

		contentEncoding	= (sch/"contentEncoding").get<string>();
		contentMediaType = (sch/"contentMediaType").get<string>();

		patternString = (sch/"pattern").get<string>();
		if (patternString.exists())
			pattern = re2::regex(get(patternString), re2::ECMAScript | re2::oneline);

		format = (sch/"format").get<string>();
	}
};

template<typename T> T next_after(T from, T to) {
	return get_components(from).add_mant(simple_compare(to, from)).f();
}
bool not_multiple_of(float x, float y) {
	double res = mod(x, y);
	return abs(res) > abs(next_after(x, 0.f) - x);
}

template<typename T> class schema_numeric : public schema {
	optional<T> maximum;
	optional<T> minimum;
	optional<float> multipleOf;
	bool exclusiveMaximum = false;
	bool exclusiveMinimum = false;

	// multipleOf - if the remainder of the division is 0 -> OK

	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		T value = instance.template get<T>(); // conversion of JSON to value_type

		if (multipleOf.exists() && not_multiple_of(value, multipleOf))
			e(path, "instance is not a multiple of " + to_string(multipleOf));

		if (maximum.exists() && (exclusiveMaximum ? value >= maximum : value > maximum))
			e(path, "instance exceeds maximum of " + to_string(maximum));

		if (minimum.exists() && (exclusiveMinimum ? value <= minimum : value < minimum))
			e(path, "instance is below minimum of " + to_string(minimum));
	}
	void walk(walker *w) { w->handle(this); }

public:
	schema_numeric(const JSONval &sch, hash_set<string> &kw) {
		kw.insert({"maximum", "minimum", "exclusiveMaximum", "exclusiveMinimum", "multipleOf"});

		maximum = (sch/"exclusiveMaximum").get<T>();
		exclusiveMaximum = maximum.exists();
		if (!exclusiveMaximum)
			maximum = (sch/"maximum").get<T>();
		
		minimum = (sch/"exclusiveMinimum").get<T>();
		exclusiveMinimum = minimum.exists();
		if (!exclusiveMinimum)
			minimum = (sch/"minimum").get<T>();

		multipleOf = (sch/"multipleOf").get<float>();
	}
};

class schema_nil : public schema {
	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		if (instance.type != JSONval::NIL)
			e(path, "expected to be null");
	}
	void walk(walker *w);
};

class schema_bool_type : public schema {
	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {}
	void walk(walker *w);
};

class schema_bool : public schema {
	bool val;
	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		if (!val)
			e(path, "instance invalid as per false-schema");
	}
	void walk(walker *w);
public:
	schema_bool(const JSONval &sch) : val(sch.get<bool>()) {}
};

class schema_required : public schema {
	const dynamic_array<string> required;

	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		for (auto &r : required)
			if (!(instance/r))
				e(path, "required property '" + r + "' not found in object as a dependency");
	}
	void walk(walker *w);

public:
	schema_required(const dynamic_array<string> &r) : required(r) {}
};

class schema_object : public schema {
	optional<size_t>		maxProperties;
	optional<size_t>		minProperties;
	dynamic_array<string>	required;

	hash_map_with_key<string, unique_ptr<schema>>	properties;
	hash_map_with_key<string, unique_ptr<schema>> 	dependencies;
	dynamic_array<pair<re2::regex, schema*>>		patternProperties;
	unique_ptr<schema>								additionalProperties;
	unique_ptr<schema>								propertyNames;

	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		if (maxProperties.exists() && instance.size() > maxProperties)
			e(path, "too many properties");

		if (minProperties.exists() && instance.size() < minProperties)
			e(path, "too few properties");

		for (auto &r : required)
			if (!(instance/r))
				e(path, "required property '" + r + "' not found in object");

		// for each property in instance
		for (auto &p : instance.items()) {
			if (propertyNames)
				propertyNames->validate(validator, path, JSONval(string(p.a)), e);

			bool matched = false;
			// check if it is in "properties"
			if (properties.count(p.a)) {
				matched = true;
				get(properties[p.a])->validate(validator, path / p.a, p.b, e);
			}

			// check all matching patternProperties
			for (auto &pp : patternProperties)
				if (pp.a.search(count_string(p.a), re2::encoding_utf8)) {
					matched = true;
					pp.b->validate(validator, path / p.a, p.b, e);
				}

			// check additionalProperties as a last resort
			if (!matched && additionalProperties) {
				first_error_handler additional_prop_err;
				additionalProperties->validate(validator, path / p.a, p.b, additional_prop_err);
				if (additional_prop_err)
					e(path, "validation failed for additional property '" + p.a + "': " + additional_prop_err.message);
			}
		}

		// reverse search
		for (auto const &prop : properties) {
			if (!(instance / prop.a)) {									// if the prop is not in the instance
				if (const auto &defaultValue = prop.b->defaultValue())	// if default value is available
					validator.patch.add(path / prop.a, defaultValue);
			}
		}

		for (auto &dep : dependencies) {
			if (auto prop = instance/dep.a)								// if dependency-property is present in instance
				dep.b->validate(validator, path / dep.a, instance, e);	// validate
		}
	}
	void walk(walker *w);

public:
	schema_object(const JSONval &sch, resolver *res, hash_set<string> &kw, const dynamic_array<json_uri> &uris) {
	
		kw.insert({"maxProperties", "minProperties", "required", "properties", "patternProperties", "additionalProperties", "dependencies", "propertyNames"});
	
		maxProperties 	= (sch/"maxProperties").get<size_t>();
		minProperties 	= (sch/"minProperties").get<size_t>();
		required 		= (sch/"required").get<dynamic_array<string>>();

		for (auto &prop : (sch/"properties").items())
			properties[prop.a] = res->make_schema(prop.b, "properties/" + prop.a, uris);

		for (auto &prop : (sch/"patternProperties").items())
			patternProperties.push_back(make_pair(re2::regex(prop.a, re2::ECMAScript), res->make_schema(prop.b, prop.a, uris)));

		if (auto& attr = sch/"additionalProperties")
			additionalProperties = res->make_schema(attr, "additionalProperties", uris);

		for (auto &dep : (sch/"dependencies").items()) {
			dependencies[dep.a] = dep.b.type == JSONval::ARRAY
				? new schema_required(dep.b.get<dynamic_array<string>>())
				: res->make_schema(dep.b, "dependencies/" + dep.a, uris);
		}

		if (auto& attr = sch/"propertyNames")
			propertyNames = res->make_schema(attr, "propertyNames", uris);
	}
};

class schema_array : public schema {
	optional<size_t>	maxItems;
	optional<size_t>	minItems;
	bool				uniqueItems;

	unique_ptr<schema>		items_schema;
	dynamic_array<unique_ptr<schema>> 	items;
	unique_ptr<schema>		additionalItems;
	unique_ptr<schema>		contains;

	void validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const final {
		if (maxItems.exists() && instance.size() > maxItems)
			e(path, "array has too many items");

		if (minItems.exists() && instance.size() < minItems)
			e(path, "array has too few items");

		if (uniqueItems) {
			for (auto it = instance.begin(); it != instance.end(); ++it) {
				auto v = find(it + 1, instance.end(), *it);
				if (v != instance.end())
					e(path, "items have to be unique for this array");
			}
		}

		int index = 0;
		if (items_schema)
			for (auto &i : instance) {
				items_schema->validate(validator, path[index], i, e);
				index++;
			}
		else {
			auto item = items.begin();
			for (auto &i : instance) {
				schema	*item_validator = item == items.end() ? additionalItems : *item++;

				if (!item_validator)
					break;

				item_validator->validate(validator, path[index], i, e);
			}
		}

		if (contains) {
			bool contained = false;
			for (auto &i : instance) {
				first_error_handler local_e;
				contains->validate(validator, path, i, local_e);
				if (!local_e) {
					contained = true;
					break;
				}
			}
			if (!contained)
				e(path, "array does not contain required element as per 'contains'");
		}
	}
	void walk(walker *w);

public:
	schema_array(const JSONval &sch, resolver *res, hash_set<string> &kw, const dynamic_array<json_uri> &uris) : uniqueItems(false) {
		kw.insert({"maxItems", "minItems", "uniqueItems", "items", "additionalItems", "contains"});
		
		maxItems 	= (sch/"maxItems").get<size_t>();
		minItems 	= (sch/"minItems").get<size_t>();
		uniqueItems = (sch/"uniqueItems").get(false);

		if (auto& attr = sch/"items") {
			if (attr.type == JSONval::ARRAY) {
				size_t c = 0;
				for (auto& subsch : attr)
					items.push_back(res->make_schema(subsch, string("items/") + to_string(c++), uris));

				if (auto& attr_add = sch/"additionalItems")
					additionalItems = res->make_schema(attr_add, "additionalItems", uris);

			} else if (attr.type == JSONval::OBJECT || attr.type == JSONval::BOOL)
				items_schema = res->make_schema(attr, "items", uris);
		}

		if (auto& attr = sch/"contains")
			contains = res->make_schema(attr, "contains", uris);
	}
};

schema* schema_type::make(const JSONval &sch, JSONval::TYPE type, resolver *res, const dynamic_array<json_uri> &uris, hash_set<string> &kw) {
	switch (type) {
		case JSONval::NIL:		return new schema_nil;
		case JSONval::INT:		return new schema_numeric<int>(sch, kw);
		case JSONval::FLOAT:	return new schema_numeric<float>(sch, kw);
		case JSONval::STRING:	return new schema_string(sch, res, kw);
		case JSONval::BOOL:		return new schema_bool_type;
		case JSONval::OBJECT:	return new schema_object(sch, res, kw, uris);
		case JSONval::ARRAY:	return new schema_array(sch, res, kw, uris);
		default:				return nullptr;
	}
}

schema *resolver::make_schema(const JSONval &sch, const char *key, dynamic_array<json_uri> uris) {
	// remove URIs which contain plain name identifiers, as sub-schemas cannot be referenced
	for (auto uri = uris.begin(); uri != uris.end();)
		if (uri->has_id())
			uri = uris.erase(uri);
		else
			uri++;

	// append to all URIs the keys for this sub-schema
	if (key)
		for (auto &uri : uris)
			uri = uri.append(key);
	
	schema	*s = 0;
	hash_set<string> kw;

	// boolean schema
	if (sch.type == JSONval::BOOL) {
		s = new schema_bool(sch);
		
	} else if (sch.type== JSONval::OBJECT) {
		kw.insert({"$id", "definitions", "$ref", "$schema", "default", "title", "description"});
		
		if (auto& attr = sch/"$id") { // if $id is present, this schema can be referenced by this ID as an additional URI
			if (find(uris.begin(), uris.end(), attr.get("")) == uris.end())
				uris.push_back(uris.back().derive(attr.get(""))); // so add it to the list if it is not there already
		}

		for (auto &def : (sch/"definitions").items())
			make_schema(def.b, "definitions/" + def.a, uris);

		if (auto& attr = sch/"$ref") { // this schema is a reference
			// the last one on the uri-stack is the last id seen before coming here, so this is the origial URI for this reference, the $ref-value has thus be resolved from it
			auto id = uris.back().derive(attr.get(""));
			s = get_or_create_ref(id);
		} else {
			s = new schema_type(sch, this, kw, uris);
		}

	}

	ISO_ASSERT(s);

	for (auto &uri : uris) { // for all URIs this schema is referenced by
		insert(uri, s);
		if (sch.type== JSONval::OBJECT) {
			for (auto &u : sch.items()) {
				if (!kw.count(u.a))
					insert_unknown_keyword(uri, u.a, u.b); // insert unknown keywords for later reference
			}
		}
	}
	return s;
}

void validator::validate(const JSONval &instance, const error_handler &err) {
	if (!root)
		err(JSONpath(), "no root schema has yet been set for validating an instance");
	else
		root->validate(*this, JSONpath(), instance, err);
}

bool validator::validate(const JSONval &instance) {
	default_error_handler	err;
	validate(instance, err);
	return !err;
}

//-----------------------------------------------------------------------------
//	walker
//-----------------------------------------------------------------------------

void	schema_not::walk(walker *w)				{ w->handle_not(subschema); }
void	schema_allOf::walk(walker *w)			{ w->handle_all(subschemata); }
void	schema_anyOf::walk(walker *w)			{ w->handle_any(subschemata); }
void	schema_oneOf::walk(walker *w)			{ w->handle_one(subschemata); }
void	schema_type::walk(walker *w)			{ w->handle_type(type, enum_, const_, logic, if_, then_, else_); }
void	schema_string::walk(walker *w)			{ w->handle_string(maxLength, minLength, patternString, format, contentEncoding, contentMediaType); }
template<> void	schema_numeric<int>::walk(walker *w)	{ w->handle_numeric(maximum, minimum, multipleOf, exclusiveMaximum, exclusiveMinimum); }
template<> void	schema_numeric<float>::walk(walker *w)	{ w->handle_numeric(maximum, minimum, multipleOf, exclusiveMaximum, exclusiveMinimum); }
void	schema_nil::walk(walker *w)				{ w->handle_nil(); }
void	schema_bool_type::walk(walker *w)		{ w->handle_bool(); }
void	schema_bool::walk(walker *w)			{ w->handle_false(); }
void	schema_required::walk(walker *w)		{ w->handle_required(required); }
void	schema_object::walk(walker *w)			{ w->handle_object(maxProperties, minProperties, required, properties, dependencies, additionalProperties, propertyNames); }
//void	schema_array::walk(walker *w)			{}
void	schema_array::walk(walker *w)			{ w->handle_array(maxItems, minItems, uniqueItems, items_schema, items, additionalItems, contains); }

} // namespace json_schema


//-----------------------------------------------------------------------------
//	JSONpatch
//-----------------------------------------------------------------------------

// originally from http://jsonpatch.com/, http://json.schemastore.org/json-patch with fixes
const JSONval patch_schema = R"patch({
    "title": "JSON schema for JSONPatch files",
    "$schema": "http://json-schema.org/draft-04/schema#",
    "type": "array",

    "items": {
        "oneOf": [
            {
                "additionalProperties": false,
                "required": [ "value", "op", "path"],
                "properties": {
                    "path" : { "$ref": "#/definitions/path" },
                    "op": {
                        "description": "The operation to perform.",
                        "type": "string",
                        "enum": [ "add", "replace", "test" ]
                    },
                    "value": {
                        "description": "The value to add, replace or test."
                    }
                }
            },
            {
                "additionalProperties": false,
                "required": [ "op", "path"],
                "properties": {
                    "path" : { "$ref": "#/definitions/path" },
                    "op": {
                        "description": "The operation to perform.",
                        "type": "string",
                        "enum": [ "remove" ]
                    }
                }
            },
            {
                "additionalProperties": false,
                "required": [ "from", "op", "path" ],
                "properties": {
                    "path" : { "$ref": "#/definitions/path" },
                    "op": {
                        "description": "The operation to perform.",
                        "type": "string",
                        "enum": [ "move", "copy" ]
                    },
                    "from": {
                        "$ref": "#/definitions/path",
                        "description": "A JSON Pointer path pointing to the location to move/copy from."
                    }
                }
            }
        ]
    },
    "definitions": {
        "path": {
            "description": "A JSON Pointer path.",
            "type": "string"
        }
    }
})patch"_json;

void JSONpatch::validate(const JSONval &patch) {
	static auto	patch_schema2 = json_schema::load(patch_schema);

	json_schema::validator(patch_schema2).validate(patch);
	//for (auto const &op : patch)
	//	json::json_pointer(op["path"].get<string>());
}

//-----------------------------------------------------------------------------
//	dumper
//-----------------------------------------------------------------------------

class dumper : json_schema::walker {
	typedef json_schema::schema schema;
public:
	string_accum	&a;


	void	handle_multi(const char *tag, range<unique_ptr<schema>*> schemata)	{
		a << tag << " {";
		for (auto& s : schemata) {
			s->walk(this);
		}
		a << "}";
	}

	virtual void	handle_all(range<unique_ptr<schema>*> schemata)	{
		handle_multi("all", schemata);
	}
	virtual void	handle_any(range<unique_ptr<schema>*> schemata)	{
		handle_multi("any", schemata);
	}
	virtual void	handle_one(range<unique_ptr<schema>*> schemata)	{
		handle_multi("one", schemata);
	}
	virtual void	handle_not(schema *sch)	{
		a << "not ";
		sch->walk(this);
	}

	virtual void	handle_type(
		range<unique_ptr<schema>*> types,
		JSONval enum_,
		JSONval const_,
		range<unique_ptr<schema>*> logic,
		schema *if_, schema *then_, schema *else_
	) {
		int	n = 0;
		for (auto &s : types) {
			if (s)
				++n;
		}
		if (n > 1) {
			a << "union {\n";
			for (auto &s : types) {
				if (s)
					s->walk(this);
			}
			a << "};\n";
		} else {
			for (auto &s : types)
				if (s)
					s->walk(this);
		}
	}
	virtual void	handle_bool()	{
		a << "bool;\n";
	}
	virtual void	handle_string(
		optional<size_t> 	maxLength, optional<size_t> minLength,
		optional<string> patternString,
		optional<string> format,
		optional<string> contentEncoding, optional<string> contentMediaType
	) {
		a << "string;\n";
	}
	virtual void	handle_numeric(
		optional<int> maximum, optional<int> minimum,
		optional<float> multipleOf,
		bool exclusiveMaximum, bool exclusiveMinimum
	) {
		a << "int;\n";
	}
	virtual void	handle_numeric(
		optional<float> maximum, optional<float> minimum,
		optional<float> multipleOf,
		bool exclusiveMaximum, bool exclusiveMinimum
	) {
		a << "float;\n";
	}

	virtual void	handle_nil()	{}
	virtual void	handle_false()	{}
	virtual void	handle_required(
		range<const string*> required
	) {}
	virtual void	handle_object(
		optional<size_t>				maxProperties,
		optional<size_t>				minProperties,
		range<string*>					required,
		const hash_map_with_key<string, unique_ptr<schema>>	&properties,
		const hash_map_with_key<string, unique_ptr<schema>> &dependencies,
		schema*							additionalProperties,
		schema*							propertyNames
	) {
		a << "struct {\n";
		for (auto& s : properties) {
		
		}
		a << "};\n";
	}
	virtual void	handle_array(
		optional<size_t>				maxItems,
		optional<size_t>				minItems,
		bool							uniqueItems,
		schema							*items_schema,
		range<unique_ptr<schema>*> 		items,
		schema							*additionalItems,
		schema							*contains
	) {
		a << "array";
	}

	dumper(string_accum &&a, schema *s) : a(a) { s->walk(this); }
};

//-----------------------------------------------------------------------------
//	tester
//-----------------------------------------------------------------------------

template<typename I> auto	last(I i) { return *--i.end(); }

class DumpTypes : indenter {
	hash_map<const JSONval&, string>	done;
	static constexpr const char *schema_types[] = {
		0,
		"null",
		"boolean",
		"integer",
		"number",
		"string",
		"array",
		"object",
	};
	static constexpr const char *ctypes[] = {
		0,
		"void",
		"bool",
		"int",
		"float",
		"string",
	};

public:
	const JSONval	&root;
	string_accum	&a;

	uint32	GetTypesMask(const JSONval& sch) {
		uint32	mask = 0;

		if (auto& attr = sch/"type") {
			if (attr.type == JSONval::STRING) {
				auto type = attr.get("");
				for (auto i : int_range_inc(JSONval::NIL, JSONval::OBJECT))
					if (str(schema_types[i]) == type) {
						mask = bit(i);
						break;
					}

			} else if (attr.type == JSONval::ARRAY) {
				for (auto &type : attr) {
					for (auto i : int_range_inc(JSONval::NIL, JSONval::OBJECT))
						if (str(schema_types[i]) == type.get("")) {
							mask |= bit(i);
						}
				}
			}

		} else {
			mask = bit(JSONval::BINARY) - bit(JSONval::NIL);
			if (auto& attr = sch / "allOf") {
				for (auto &i : attr)
					mask &= GetTypesMask(i);
			}
			if (auto& attr = sch / "$ref") {
				auto	ref = attr.get("");
				if (ref[0] == '#') {
					mask &= GetTypesMask(JSONpath(ref + 1).get(root));
				}
			}
		}
		return mask;
	}

	string FixName(count_string name) {
		static hash_set<const char*> keywords = {
			"alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand",
			"bitor", "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept",
			"const", "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await", "co_return", "co_yield",
			"decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
			"false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable",
			"namespace", "new", "noexcept", "not", "not_eq", "nullptr",
			"operator", "or", "or_eq", "private", "protected", "public", "reflexpr", "register", "reinterpret_cast", "requires", "return",
			"short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized",
			"template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
			"virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
		};
		string	name2 = transformc(count_string(name), [](char c) { return is_alphanum(c) ? c : '_'; });
		if (keywords.count(name2))
			name2 = "_" + name2;
		return name2;
	}

	void	DumpSimpleType(JSONval::TYPE type, const char *name, bool def, bool optional) {
		if (def)
			a << "typedef ";
		if (optional)
			a << "optional<";
		a << ctypes[type];
		if (optional)
			a << '>';
		if (name)
			a << ' ' << name;
	}

	void	DumpType(JSONval::TYPE type, const JSONval& sch, const char *name, bool def, bool optional) {
		switch (type) {
			case JSONval::NIL:
			case JSONval::INT:
			case JSONval::FLOAT:
			case JSONval::STRING:
			case JSONval::BOOL:
				DumpSimpleType(type, name, def, optional);
				break;

			case JSONval::OBJECT: {
				auto	required 	= (sch/"required").get<dynamic_array<string>>();
				auto	properties	= (sch/"properties").items();
				auto&	additional	= sch / "additionalProperties";

				if (properties.size()) {
					open(a << "struct " << onlyif(def && name, name));
					for (auto &prop : (sch/"properties").items()) {
						newline(a);
						if (prop.b.type != JSONval::OBJECT) {
							a << "bool" << ' ' << FixName(count_string(prop.a));
						} else {
							DumpType(prop.b, FixName(count_string(prop.a)), false, !find_check(required, prop.a));
						}
						a << ';';
					}
					if (additional) {
						newline(a) << "hash_map<string, ";
						DumpType(additional, 0, false, false);
						a << "> hash;";
					}
					close(a);
					if (!def && name)
						a << ' ' << name;

				} else {
					a << "hash_map<string, ";
					DumpType(additional, 0, false, false);
					a << ">";
					if (name)
						a << ' ' << name;
				}

				break;
			}
			case JSONval::ARRAY: {
				auto	maxItems 	= (sch/"maxItems").get<size_t>();
				auto	minItems 	= (sch/"minItems").get<size_t>();
				auto&	items		= sch/"items";

				if (def)
					a << "typedef ";

				if (maxItems.exists()) {
					auto	n = maxItems.or_default();
					if (minItems.exists() && n == minItems.or_default()) {
						DumpType(items, 0, false, false);
						a << ' ' << name << '[' << n << ']';
						return;
					}
				}
				a << "dynamic_array<";
				if (items && items.type == JSONval::OBJECT)
					DumpType(items, 0, false, false);
				else
					a << "anything";
				a << '>';
				if (name)
					a << ' ' << name;
				break;
			}
			default:
				break;
		}
	}

	void DumpType(const JSONval& sch, const char *name, bool def, bool optional) {
		auto &prev_name = done[sch].put();

		if (!def && prev_name) {
			a << onlyif(optional, "optional<") << prev_name << onlyif(optional, ">");
			if (name)
				a << ' ' << name;
			return;
		}

		if (def) {
			prev_name = name;
		} else {
			if (auto &id = sch/"$id")
				prev_name = FixName(last(parts<'/'>(URLcomponents(id.get("")).path)));
			else
				prev_name = "?";
		}

		if (auto& attr = sch / "$ref") {
			auto	ref = attr.get("");
			if (ref[0] == '#') {
				DumpType(JSONpath(ref + 1).get(root), name, def, optional);
				return;
			}
		}

		uint32	mask = GetTypesMask(sch);
		if (is_pow2(mask)) {
			DumpType((JSONval::TYPE)lowest_set_index(mask), sch, name, def, optional);

		} else if (is_pow2(mask & ~bit(JSONval::BOOL))) {
			DumpType((JSONval::TYPE)lowest_set_index(mask & ~bit(JSONval::BOOL)), sch, name, def, true);

		} else if (auto& attr = sch / "enum") {
			open(a << "enum " << onlyif(def && name, name));
			for (auto& i : attr)
				newline(a) << i.get("") << ',';
			close(a);
			if (!def && name)
				a << ' ' << name;

		} else {
			a << "union_of<";
			int		j = 0;
			if (auto& attr = sch / "anyOf") {
				for (auto& i : attr) {
					if (j++)
						a << ',';
					DumpType(i, 0, false, false);
				}
			} else {
				while (mask) {
					if (j++)
						a << ',';
					int	i = lowest_set_index(mask);
					DumpType((JSONval::TYPE)i, sch, 0, false, false);
					mask = clear_lowest(mask);
				}
			}
			a << '>';
			if (name)
				a << ' ' << name;
		}
	}

	DumpTypes(string_accum&& a, const JSONval& root) : root(root), a(a) {
		string	name = FixName(last(parts<'/'>(URLcomponents((root/"$id").get("?")).path)));
		done[root]			= name;
		done[JSONval::none]	= "anything";

		newline(a << "struct " << name << ';');

		for (auto &def : (root/"definitions").items()) {
			DumpType(def.b, def.a, true, false);
			newline(a << ';');
		}
		
		DumpType(root, name, true, false);
	}

};

//-----------------------------------------------------------------------------
//	tester
//-----------------------------------------------------------------------------

JSONval draft7_schema_builtin = R"( {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "$id": "http://json-schema.org/draft-07/schema#",
    "title": "Core schema meta-schema",
    "definitions": {
        "nonNegativeInteger": {
            "type": "integer",
            "minimum": 0
        },
        "nonNegativeIntegerDefault0": {
            "allOf": [
                { "$ref": "#/definitions/nonNegativeInteger" },
                { "default": 0 }
            ]
        },
        "schemaArray": {
            "type": "array",
            "minItems": 1,
            "items": { "$ref": "#" }
        },
        "simpleTypes": {
            "enum": [
                "array",
                "boolean",
                "integer",
                "null",
                "number",
                "object",
                "string"
            ]
        },
        "stringArray": {
            "type": "array",
            "items": { "type": "string" },
            "uniqueItems": true,
            "default": []
        }
    },
    "type": ["object", "boolean"],
    "properties": {
        "$id": {
            "type": "string",
            "format": "uri-reference"
        },
        "$schema": {
            "type": "string",
            "format": "uri"
        },
        "$ref": {
            "type": "string",
            "format": "uri-reference"
        },
        "$comment": {
            "type": "string"
        },
        "title": {
            "type": "string"
        },
        "description": {
            "type": "string"
        },
        "default": true,
        "readOnly": {
            "type": "boolean",
            "default": false
        },
        "examples": {
            "type": "array",
            "items": true
        },
        "multipleOf": {
            "type": "number",
            "exclusiveMinimum": 0
        },
        "maximum": {
            "type": "number"
        },
        "exclusiveMaximum": {
            "type": "number"
        },
        "minimum": {
            "type": "number"
        },
        "exclusiveMinimum": {
            "type": "number"
        },
        "maxLength": { "$ref": "#/definitions/nonNegativeInteger" },
        "minLength": { "$ref": "#/definitions/nonNegativeIntegerDefault0" },
        "pattern": {
            "type": "string",
            "format": "regex"
        },
        "additionalItems": { "$ref": "#" },
        "items": {
            "anyOf": [
                { "$ref": "#" },
                { "$ref": "#/definitions/schemaArray" }
            ],
            "default": true
        },
        "maxItems": { "$ref": "#/definitions/nonNegativeInteger" },
        "minItems": { "$ref": "#/definitions/nonNegativeIntegerDefault0" },
        "uniqueItems": {
            "type": "boolean",
            "default": false
        },
        "contains": { "$ref": "#" },
        "maxProperties": { "$ref": "#/definitions/nonNegativeInteger" },
        "minProperties": { "$ref": "#/definitions/nonNegativeIntegerDefault0" },
        "required": { "$ref": "#/definitions/stringArray" },
        "additionalProperties": { "$ref": "#" },
        "definitions": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "default": {}
        },
        "properties": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "default": {}
        },
        "patternProperties": {
            "type": "object",
            "additionalProperties": { "$ref": "#" },
            "propertyNames": { "format": "regex" },
            "default": {}
        },
        "dependencies": {
            "type": "object",
            "additionalProperties": {
                "anyOf": [
                    { "$ref": "#" },
                    { "$ref": "#/definitions/stringArray" }
                ]
            }
        },
        "propertyNames": { "$ref": "#" },
        "const": true,
        "enum": {
            "type": "array",
            "items": true,
            "minItems": 1,
            "uniqueItems": true
        },
        "type": {
            "anyOf": [
                { "$ref": "#/definitions/simpleTypes" },
                {
                    "type": "array",
                    "items": { "$ref": "#/definitions/simpleTypes" },
                    "minItems": 1,
                    "uniqueItems": true
                }
            ]
        },
        "format": { "type": "string" },
        "contentMediaType": { "type": "string" },
        "contentEncoding": { "type": "string" },
        "if": { "$ref": "#" },
        "then": { "$ref": "#" },
        "else": { "$ref": "#" },
        "allOf": { "$ref": "#/definitions/schemaArray" },
        "anyOf": { "$ref": "#/definitions/schemaArray" },
        "oneOf": { "$ref": "#/definitions/schemaArray" },
        "not": { "$ref": "#" }
    },
    "default": true
} )"_json;


		struct schema;
		struct anything;
		typedef int nonNegativeInteger;
		typedef int nonNegativeIntegerDefault0;
		typedef dynamic_array<schema*> schemaArray;
		enum simpleTypes{
			_array,
			_boolean,
			_integer,
			_null,
			_number,
			_object,
			_string,
		};
		typedef dynamic_array<string> stringArray;
		struct schema{
			optional<string> _comment;
			optional<string> _id;
			optional<string> _ref;
			optional<string> _schema;
			optional<schema*> additionalItems;
			optional<schema*> additionalProperties;
			optional<schemaArray> allOf;
			optional<schemaArray> anyOf;
			bool _const;
			optional<schema*> contains;
			optional<string*> contentEncoding;
			optional<string*> contentMediaType;
			bool _default;
			hash_map<string, schema*> definitions;
			hash_map<string, union_of<schema*,stringArray>> dependencies;
			optional<string> description;
			optional<schema*> _else;
			dynamic_array<anything> _enum;
			dynamic_array<anything> examples;
			optional<float> exclusiveMaximum;
			optional<float> exclusiveMinimum;
			optional<string> format;
			optional<schema*> _if;
			union_of<schema*,schemaArray> items;
			optional<nonNegativeInteger> maxItems;
			optional<nonNegativeInteger> maxLength;
			optional<nonNegativeInteger> maxProperties;
			optional<float> maximum;
			optional<nonNegativeIntegerDefault0> minItems;
			optional<nonNegativeIntegerDefault0> minLength;
			optional<nonNegativeIntegerDefault0> minProperties;
			optional<float> minimum;
			optional<float> multipleOf;
			optional<schema*> _not;
			optional<schemaArray> oneOf;
			optional<string> pattern;
			hash_map<string, schema*> patternProperties;
			hash_map<string, schema*> properties;
			optional<schema*> propertyNames;
			optional<bool> readOnly;
			optional<stringArray> required;
			optional<schema*> then;
			optional<string> title;
			union_of<simpleTypes,dynamic_array<simpleTypes>> type;
			optional<bool> uniqueItems;
		};

struct tester {

	static void loader(const char *uri, JSONval &val) {
		if (uri == cstr("http://json-schema.org/draft-07/schema")) {
			val = draft7_schema_builtin;

		} else {

			string fn = "D:/dev/github/json-schema-validator/test/JSON-Schema-Test-Suite";
			fn += "/remotes";
			fn += URLcomponents(uri).path;

			val.read(lvalue(JSONreader(lvalue(FileInput(fn)))));
		}
	}

	tester() {
		
//		DumpTypes(make_stream_accum(filewriter(stderr)), draft7_schema_builtin);
		DumpTypes(trace_accum(), draft7_schema_builtin);

		size_t total_failed = 0, total = 0;

		for (auto dir = directory_recurse("D:\\dev\\Github\\json-schema-validator\\test\\JSON-Schema-Test-Suite\\tests", "*.json"); dir; ++dir) {
			filename	fn = dir;
			ISO_OUTPUTF("") << "**** Testing File " << fn << "****\n";

			JSONval	validation = FileInput(fn).get();

			for (auto &test_group : validation) {
				size_t group_failed = 0,
					group_total = 0;

				ISO_OUTPUTF("") << "Testing Group " << test_group/"description" << "\n";

				const auto &schema = test_group/"schema";

				//json_schema::validator	validator(json_schema::load(schema, &loader), &json_schema::default_format_check, &content_check);
				auto	schema2 = json_schema::load(schema, &loader);
				json_schema::validator	validator(schema2);

				dumper	d(trace_accum(), schema2);

				for (auto &test_case : test_group/"tests") {
					ISO_OUTPUTF("") << "  Testing Case " << test_case/"description" << "\n";

					bool valid = validator.validate(test_case/"data");

					if (valid == (test_case/"valid").get<bool>())
						ISO_OUTPUTF("") << "      --> Test Case exited with " << valid << " as expected.\n";
					else {
						group_failed++;
						ISO_OUTPUTF("") << "      --> Test Case exited with " << valid << " NOT expected.\n";
					}
					group_total++;
					ISO_OUTPUTF("") << "\n";
				}
				total_failed += group_failed;
				total += group_total;
				ISO_OUTPUTF("") << "Group RESULT: " << test_group/"description" << " "
					<< (group_total - group_failed) << " of " << group_total
					<< " have succeeded - " << group_failed << " failed\n";
				ISO_OUTPUTF("") << "-------------\n";
			}

			ISO_OUTPUTF("") << "Total RESULT: " << (total - total_failed) << " of " << total << " have succeeded - " << total_failed << " failed\n";

		}
	}

};
// static tester _tester;

} // namespace iso
