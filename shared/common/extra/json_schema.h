#ifndef JSON_SCHEMA_H
#define JSON_SCHEMA_H

#include "json.h"
#include "comms/HTTP.h"
#include "base/hash.h"

namespace iso {

class JSONpatch : public JSONval {
	static void validate(const JSONval &patch);
public:
	JSONpatch() = default;
	JSONpatch(const JSONval &patch) : JSONval(patch) {}

	JSONpatch &add(const JSONpath &path, const JSONval &value) {
		push_back(JSONval::object{{"op", "add"}, {"path", string(path.path)}, {"value", value}});
		return *this;
	}

	JSONpatch &replace(const JSONpath &path, const JSONval &value) {
		push_back(JSONval::object{{"op", "replace"}, {"path", string(path.path)}, {"value", value}});
		return *this;
	}
	JSONpatch &remove(const JSONpath &path) {
		push_back(JSONval::object{{"op", "remove"}, {"path", string(path.path)}});
		return *this;
	}
};


// A class representing a JSON-URI for schemas derived from section 8 of JSON Schema: A Media Type for Describing JSON Documents draft-wright-JSON-schema-00

struct json_uri : URLcomponents {
	json_uri(URLcomponents &&uri) : URLcomponents(move(uri)) {}

	bool	has_id() const {
		return URLcomponents::anchor && URLcomponents::anchor[0] && URLcomponents::anchor[0] != '/';
	}
	bool	has_route() const {
		return URLcomponents::anchor && URLcomponents::anchor[0] == '/';
	}

	// create a new json_uri based in this one and the given uri; resolves relative changes (paths or pointers) and resets part if proto or hostname changes
	json_uri derive(const char *uri) const {
		auto	url2 = URLcomponents(uri);
		// if it is an URN take it as it is
		if (url2.scheme == cstr("urn"))
			return move(url2);

		return URLcomponents(GetURL(url2).begin());
	}

	// append a pointer-field to the pointer-part of this uri
	json_uri append(const char *field) const {
		if (has_id())
			return *this;
		return URLcomponents(GetURL() + "/" + field);
	}
	string	location() const {
		return GetURL(0, URLcomponents::FLAGS::LOCATION);
	}
	string	anchor() const {
		return replace(replace(Unescape(URLcomponents::anchor), "~1", "/"), "~0", "~");
	}

	bool	operator==(const char *s) { return GetURL() == s; }
};

namespace json_schema {

typedef callback_ref<void(const JSONpath &path, const char *message)>	error_handler;

struct format_checker : static_list<format_checker> {
	typedef void(func_t)(const JSONpath &path, const count_string &value, const error_handler &e);
	const char		*format;
	func_t			*func;
	format_checker(const char *format, func_t *func) : format(format), func(func) {}
	bool operator==(const char *f) { return str(format) == f; }
};

struct content {
	const_memory_block	data;
	const char *encoding, *mediaType;
};

struct content_checker : static_list<content_checker> {
	typedef void(func_t)(const JSONpath &path, content &c, const error_handler &e);
	func_t	*func;
	content_checker(func_t *func) : func(func) {}
};

struct default_error_handler {
	bool error	= false;
	void operator()(const JSONpath &path, const char *message) {
		error = true;
	}
	operator bool() const { return error; }
};

class schema;

class validator {
	schema		*root;
public:
	JSONpatch	patch;

	validator(schema *root) : root(root) {}
	void	validate(const JSONval &instance, const error_handler &err);
	bool	validate(const JSONval &instance);
};

class walker;

class schema {
public:
	virtual ~schema()	{}
	virtual void 			validate(validator &validator, const JSONpath &path, const JSONval &instance, const error_handler &e) const = 0;
	virtual void			walk(walker *w) = 0;
	virtual const JSONval& 	defaultValue() const { return JSONval::none;}
};

typedef callback<void(const char *id, JSONval &v)> schema_loader;
schema*	load(const JSONval &s, schema_loader &&loader = nullptr);

class walker {
public:
	virtual void	handle_all(range<unique_ptr<schema>*> schemata)	{}
	virtual void	handle_any(range<unique_ptr<schema>*> schemata)	{}
	virtual void	handle_one(range<unique_ptr<schema>*> schemata)	{}
	virtual void	handle_not(schema *sch)	{}

	virtual void	handle_type(
		range<unique_ptr<schema>*> types,
		JSONval enum_,
		JSONval const_,
		range<unique_ptr<schema>*> logic,
		schema *if_, schema *then_, schema *else_
	) {}
	virtual void	handle_bool()	{}
	virtual void	handle_string(
		optional<size_t> 	maxLength, optional<size_t> minLength,
		optional<string> patternString,
		optional<string> format,
		optional<string> contentEncoding, optional<string> contentMediaType
	) {}
	virtual void	handle_numeric(
		optional<int> maximum, optional<int> minimum,
		optional<float> multipleOf,
		bool exclusiveMaximum, bool exclusiveMinimum
	) {}
	virtual void	handle_numeric(
		optional<float> maximum, optional<float> minimum,
		optional<float> multipleOf,
		bool exclusiveMaximum, bool exclusiveMinimum
	) {}

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
	) {}
	virtual void	handle_array(
		optional<size_t>				maxItems,
		optional<size_t>				minItems,
		bool							uniqueItems,
		schema							*items_schema,
		range<unique_ptr<schema>*> 		items,
		schema							*additionalItems,
		schema							*contains
	) {}
};
} } // namespace iso::json_schema

#endif
