#include "flatbuffer.h"
#include "base/algorithm.h"
#include "base/bits.h"
#include "iso/iso_files.h"
#include "extra/text_stream.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	tokeniser
//-----------------------------------------------------------------------------
namespace flatbuffers {
namespace schema {

	struct Type {
		enum TYPE {
			UNKNOWN = 0,
			BOOL,
			INT8,
			UINT8,
			INT16,
			UINT16,
			INT32,
			UINT32,
			INT64,
			UINT64,
			FLOAT32,
			FLOAT64,
			ENUM,
			STRING,
			UNION,
			STRUCT,
			TABLE,
		};
		TYPE	type:6, representation:6; bool array:1; uint32 count:19;
		void	*entries;
		Type(TYPE type = UNKNOWN) : type(type), representation(type), array(false), count(0), entries(0) {}

		template<typename T> void	set_entries(dynamic_array<T> &a) {
			count	= a.size32();
			entries = a.detach().begin();
		}
		template<typename T> auto	get_entries() {
			return make_range_n((T*)entries, count);
		}

		bool	simple() const {
			return !array && type < STRING;
		}

		friend uint32	size(TYPE t) {
			switch (t) {
				case BOOL:		return 1;
				case INT8:		return 1;
				case UINT8:		return 1;
				case INT16:		return 2;
				case UINT16:	return 2;
				case INT32:		return 4;
				case UINT32:	return 4;
				case INT64:		return 8;
				case UINT64:	return 8;
				case FLOAT32:	return 4;
				case FLOAT64:	return 8;
				case STRING:	return 4;
//				case ENUM:
//				case UNION:
//				case STRUCT:
//				case TABLE:
				default:		return 0;
			}
		}
	};

	struct EnumType : Type {
		struct Entry {
			string	id;
			int64	value;
			Entry(string_param &&id, int64 value) : id(move(id)), value(value) {}
		};
		EnumType() : Type(ENUM) { representation = INT32; }
		auto Entries() { return get_entries<Entry>(); }
	};

	struct UnionType : Type {
		struct Entry {
			Type	type;
			int64	value;
			Entry(Type type, int64 value) : type(type), value(value) {}
		};
		UnionType() : Type(UNION) {}
		auto Entries() { return get_entries<Entry>(); }
	};

	struct UserType : Type {
		struct Entry {
			string	id;
			Type	type;
			int64	default_value;
			Entry(string_param &&id, Type type) : id(move(id)), type(type), default_value(0) {}
		};
		UserType(bool table) : Type(table ? TABLE : STRUCT) {}
		auto Entries() { return get_entries<Entry>(); }
	};

	template<typename T> void	destruct(const range<T*> &r) {
		iso::destruct(r.begin(), r.size());
		aligned_free(r.begin());
	}

	struct Schema {
		Type	root;
		string	extension;
		string	identifier;
		hash_map<crc32, Type>	types;
		Schema()			= default;
		Schema(Schema&&)	= default;
		~Schema() {
			for (auto &i : types) {
				switch (i.type) {
					case Type::ENUM:
						destruct(((EnumType&)i).Entries());
						break;
					case Type::UNION:
						destruct(((UnionType&)i).Entries());
						break;
					case Type::STRUCT:
					case Type::TABLE:
						destruct(((UserType&)i).Entries());
						break;
				}
			}
		}
		Schema& operator=(Schema&&)	= default;
	};
} // namespace schema

/*
schema = include* ( namespace_decl | type_decl | enum_decl | root_decl | file_extension_decl | file_identifier_decl | attribute_decl | rpc_decl | object )*
include = include string_constant ;
namespace_decl = namespace ident ( . ident )* ;
attribute_decl = attribute ident | "</tt>ident<tt>" ;
type_decl = ( table | struct ) ident metadata { field_decl+ }
enum_decl = ( enum ident [ : type ] | union ident ) metadata { commasep( enumval_decl ) }
root_decl = root_type ident ;
field_decl = ident : type [ = scalar ] metadata ;
rpc_decl = rpc_service ident { rpc_method+ }
rpc_method = ident ( ident ) : ident metadata ;
type = bool | byte | ubyte | short | ushort | int | uint | float | long | ulong | double | int8 | uint8 | int16 | uint16 | int32 | uint32| int64 | uint64 | float32 | float64 | string | [ type ] | ident
enumval_decl = ident [ = integer_constant ]
metadata = [ ( commasep( ident [ : single_value ] ) ) ]
scalar = integer_constant | float_constant
object = { commasep( ident : value ) }
single_value = scalar | string_constant
value = single_value | object | [ commasep( value ) ]
commasep(x) = [ x ( , x )* ]
file_extension_decl = file_extension string_constant ;
file_identifier_decl = file_identifier string_constant ;
integer_constant = -?[0-9]+ | true | false
float_constant = -?[0-9]+.[0-9]+((e|E)(+|-)?[0-9]+)?
string_constant = \".*?\\"
ident = [a-zA-Z_][a-zA-Z0-9_]*
*/

class tokeniser {
	enum TOKEN {
		TOK_EOF					= -1,

		TOK_MINUS				= '-',
		TOK_PLUS				= '+',
		TOK_DOT					= '.',
		TOK_COLON				= ':',
		TOK_SEMICOLON			= ';',
		TOK_EQUALS				= '=',
		TOK_COMMA				= ',',
		TOK_DIVIDE				= '/',
		TOK_OPEN_BRACE			= '{',
		TOK_CLOSE_BRACE			= '}',
		TOK_OPEN_BRACKET		= '[',
		TOK_CLOSE_BRACKET		= ']',
		TOK_OPEN_PARENTHESIS	= '(',
		TOK_CLOSE_PARENTHESIS	= ')',

		TOK_IDENTIFIER			= 256,
		TOK_NUMBER,
		TOK_CHARLITERAL,
		TOK_STRINGLITERAL,

		_TOK_KEYWORDS,
		TOK_BOOL				= _TOK_KEYWORDS,
		TOK_BYTE,
		TOK_UBYTE,
		TOK_SHORT,
		TOK_USHORT,
		TOK_INT,
		TOK_UINT,
		TOK_FLOAT,
		TOK_LONG,
		TOK_ULONG,
		TOK_DOUBLE,
		TOK_INT8,
		TOK_UINT8,
		TOK_INT16,
		TOK_UINT16,
		TOK_INT32,
		TOK_UINT32,
		TOK_INT64,
		TOK_UINT64,
		TOK_FLOAT32,
		TOK_FLOAT64,
		TOK_STRING,
		TOK_TRUE,
		TOK_FALSE,
		TOK_INCLUDE,
		TOK_NAMESPACE,
		TOK_ATTRIBUTE,
		TOK_TABLE,
		TOK_STRUCT,
		TOK_ENUM,
		TOK_UNION,
		TOK_ROOT_TYPE,
		TOK_RPC_SERVICE,
		TOK_FILE_EXTENSION,
		TOK_FILE_IDENTIFIER,

	};

	static const char*				_keywords[];
	static hash_map<crc32, TOKEN>	keywords;

	text_reader<reader_intf>		&reader;
	schema::Schema					&schema;
	TOKEN							token;
	string							identifier;

	template<typename T> T next_return(T t) { Next(); return t; }

	void				MetaData();

	schema::Type::TYPE SimpleType0() {
		switch (token) {
			case TOK_BOOL:		return schema::Type::BOOL;
			case TOK_BYTE:		return schema::Type::INT8;
			case TOK_UBYTE:		return schema::Type::UINT8;
			case TOK_SHORT:		return schema::Type::INT16;
			case TOK_USHORT:	return schema::Type::UINT16;
			case TOK_INT:		return schema::Type::INT32;
			case TOK_UINT:		return schema::Type::UINT32;
			case TOK_FLOAT:		return schema::Type::FLOAT32;
			case TOK_LONG:		return schema::Type::INT64;
			case TOK_ULONG:		return schema::Type::UINT64;
			case TOK_DOUBLE:	return schema::Type::FLOAT64;
			case TOK_INT8:		return schema::Type::INT8;
			case TOK_UINT8:		return schema::Type::UINT8;
			case TOK_INT16:		return schema::Type::INT16;
			case TOK_UINT16:	return schema::Type::UINT16;
			case TOK_INT32:		return schema::Type::INT32;
			case TOK_UINT32:	return schema::Type::UINT32;
			case TOK_INT64:		return schema::Type::INT64;
			case TOK_UINT64:	return schema::Type::UINT64;
			case TOK_FLOAT32:	return schema::Type::FLOAT32;
			case TOK_FLOAT64:	return schema::Type::FLOAT64;
			case TOK_STRING:	return schema::Type::STRING;
			default:
				ISO_ASSERT(0);
				return schema::Type::UNKNOWN;
		}
	}

	schema::Type		Type();
	int64				Integer0() {
		switch (token) {
			case TOK_FALSE:	return 0;
			case TOK_TRUE:	return 1;
			case TOK_NUMBER:return read_integer(reader, 0);
			default:		throw_accum("Expected number");
		}
	}

	number				Scalar0(schema::Type type) {
		switch (token) {
			case TOK_FALSE:	return 0;
			case TOK_TRUE:	return 1;
			case TOK_NUMBER:return read_number(reader, 0); break;
			case TOK_IDENTIFIER:
				if (type.type == schema::Type::ENUM || type.type == schema::Type::UNION) {
					for (auto &i : ((schema::EnumType&)type).Entries()) {
						if (i.id == identifier)
							return i.value;
					}
				}
			default:
				throw_accum("Expected number");
		}
	}
	schema::Type::TYPE	SimpleType()				{ return next_return(SimpleType0()); }
	int64				Integer()					{ return next_return(Integer0()); }
	number				Scalar(schema::Type type)	{ return next_return(Scalar0(type)); }
	string				Identifier()	{
		if (token == TOK_IDENTIFIER) {
			string	s = move(identifier);
			Next();
			return move(s);
		}
		throw_accum("Expected identifier");
	}

	string				String()	{
		if (token == TOK_STRINGLITERAL) {
			string	s = move(identifier);
			Next();
			return move(s);
		}
		throw_accum("Expected string");
	}

public:
	tokeniser(text_reader<reader_intf> &reader, schema::Schema &schema);
	tokeniser(text_reader<reader_intf> &&reader, schema::Schema &schema) : tokeniser(reader, schema) {}

	TOKEN				GetToken();
	tokeniser&			Next()				{ token = GetToken(); return *this; }
	operator			TOKEN()				{ return token; }
	void				expect(TOKEN tok);
	void				Parse();
};

const char* tokeniser::_keywords[] = {
	"bool",
	"byte",
	"ubyte",
	"short",
	"ushort",
	"int",
	"uint",
	"float",
	"long",
	"ulong",
	"double",
	"int8",
	"uint8",
	"int16",
	"uint16",
	"int32",
	"uint32",
	"int64",
	"uint64",
	"float32",
	"float64",
	"string",
	"true",
	"false",
	"include",
	"namespace",
	"attribute",
	"table",
	"struct",
	"enum",
	"union",
	"root_type",
	"rpc_service",
	"file_extension",
	"file_identifier",
};
hash_map<crc32, tokeniser::TOKEN>	tokeniser::keywords;

tokeniser::tokeniser(text_reader<reader_intf> &reader, schema::Schema &schema) : reader(reader), schema(schema) {
	if (keywords.size() == 0) {
		for (int i = 0; i < num_elements(_keywords); i++)
			keywords[_keywords[i]] = TOKEN(_TOK_KEYWORDS + i);
	}
	Next();
}

void tokeniser::expect(TOKEN tok) {
	static const char* _tokens[] = {
		"identifier",
		"number",
		"character literal",
		"string literal",
		"'...'",
		"'=='",
		"'!='",
		"'<='",
		"'>='",
		"'<<'",
		"'>>'",
		"'&&'",
		"'||'",
		"'::'",
	};
	if (token != tok) {
		if (tok < 256)
			throw_accum("Expected '" << (char)tok << "'");
		else if (tok >= _TOK_KEYWORDS)
			throw_accum("Expected " << _keywords[tok - _TOK_KEYWORDS]);
		else
			throw_accum("Expected " << _tokens[tok - 256]);
	}
	Next();
}

tokeniser::TOKEN tokeniser::GetToken() {
	for (;;) {
		int	c	= skip_whitespace(reader);

		switch (c) {
			case '_':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
				identifier = read_string(reader, c);
				if (TOKEN *k = keywords.check(identifier))
					return *k;
				return TOK_IDENTIFIER;

			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
				reader.put_back(c);
				return TOK_NUMBER;

			case '\'':
				identifier = read_string(reader, c);
				return TOK_CHARLITERAL;

			case '/':
				c = reader.getc();
				if (c == '/') {
					do c = reader.getc(); while (c != '\n' && c != EOF);
				} else if (c == '*') {
					c = reader.getc();
					do {
						while (c != '*' && c != EOF) c = reader.getc();
						c = reader.getc();
					} while (c != '/' && c != EOF);
				} else {
					reader.put_back(c);
					return TOK_DIVIDE;
				}
				continue;

			case '"':
				identifier = read_string(reader, c);
				return TOK_STRINGLITERAL;

			default:
				return (TOKEN)c;
		}
	}
}

void tokeniser::MetaData() {
	if (token == TOK_OPEN_PARENTHESIS) {
		while (Next() != TOK_CLOSE_PARENTHESIS) {
			expect(TOK_IDENTIFIER);
			if (token == TOK_COLON) {
				Next();

				switch (token) {
					case TOK_STRINGLITERAL:
						break;
					case TOK_NUMBER:
						read_number(reader, 0);
						break;
					default:
						ISO_ASSERT(0);
				}
				Next();
			}
			if (token != TOK_COMMA)
				break;
			Next();
		}
		expect(TOK_CLOSE_PARENTHESIS);
	}
}

schema::Type tokeniser::Type() {
	switch (token) {
		case TOK_OPEN_BRACKET: {
			Next();
			auto	type = Type();
			type.array = true;
			expect(TOK_CLOSE_BRACKET);
			return type;
		}
		case TOK_IDENTIFIER:
			if (auto *t = schema.types.check(identifier))
				return next_return(*t);

		default:
			return SimpleType();
	}
}

void tokeniser::Parse() {
	while (token == TOK_INCLUDE) {
		Next();
		expect(TOK_STRINGLITERAL);
		expect(TOK_SEMICOLON);
	}
	for (;;) {
		switch (token) {
			//namespace_decl
			case TOK_NAMESPACE:
				Next();
				expect(TOK_IDENTIFIER);
				expect(TOK_SEMICOLON);
				break;

			// type_decl
			case TOK_TABLE:
			case TOK_STRUCT: {
				bool	was_table = token == TOK_TABLE;
				Next();
				auto&	type = schema.types.put(Identifier(), was_table ? schema::Type::TABLE : schema::Type::STRUCT);
				MetaData();
				expect(TOK_OPEN_BRACE);

				dynamic_array<schema::UserType::Entry>	array;
				while (token != TOK_CLOSE_BRACE) {
					string	id = Identifier();
					expect(TOK_COLON);
					auto	&f	= array.emplace_back(id, Type());
					if (token == TOK_EQUALS) {
						Next();
						f.default_value = Scalar(f.type);
					}
					MetaData();
					expect(TOK_SEMICOLON);
				}
				type.set_entries(array);
				Next();
				break;
			}

			// enum_decl
			case TOK_ENUM: {
				Next();
				auto&	type = schema.types.put(Identifier(), schema::Type::ENUM);

				if (token == TOK_COLON) {
					Next();
					type.representation = SimpleType();
				}
				MetaData();
				expect(TOK_OPEN_BRACE);

				int64	value = 0;
				dynamic_array<schema::EnumType::Entry>	array;
				while (token != TOK_CLOSE_BRACE) {
					string	id = Identifier();
					if (token == TOK_EQUALS) {
						Next();
						value = Integer();
					}
					array.emplace_back(move(id), value++);

					if (token != TOK_COMMA)
						break;
					Next();
				}
				expect(TOK_CLOSE_BRACE);
				type.set_entries(array);
				break;
			}

			case TOK_UNION: {
				Next();
				auto&	type = schema.types.put(Identifier(), schema::Type::UNION);

				MetaData();
				expect(TOK_OPEN_BRACE);

				int64	value = 0;
				dynamic_array<schema::UnionType::Entry>	array;
				while (token != TOK_CLOSE_BRACE) {
					string	id = Identifier();
					if (token == TOK_EQUALS) {
						Next();
						value = Integer();
					}
					if (auto *p = schema.types.check(id))
						array.emplace_back(*p, value++);

					if (token != TOK_COMMA)
						break;
					Next();
				}
				expect(TOK_CLOSE_BRACE);
				type.set_entries(array);
				break;
			}

			// root_decl
			case TOK_ROOT_TYPE:
				Next();
				schema.root = schema.types[Identifier()];
				expect(TOK_SEMICOLON);
				break;

			// file_extension_decl
			case TOK_FILE_EXTENSION:
				Next();
				schema.extension = String();
				expect(TOK_SEMICOLON);
				break;

			//file_identifier_decl
			case TOK_FILE_IDENTIFIER:
				Next();
				schema.identifier = String();
				expect(TOK_SEMICOLON);
				break;

			// attribute_decl
			case TOK_ATTRIBUTE:
				Next();
				switch (token) {
					case TOK_IDENTIFIER:
					case TOK_STRINGLITERAL:
						break;
					default:
						ISO_ASSERT(0);
				}
				Next();
				break;

			//rpc_decl
			case TOK_RPC_SERVICE:
				Next();
				expect(TOK_IDENTIFIER);
				expect(TOK_OPEN_BRACE);
				while (token != TOK_CLOSE_BRACE) {
					expect(TOK_IDENTIFIER);
					expect(TOK_OPEN_PARENTHESIS);
					expect(TOK_IDENTIFIER);
					expect(TOK_COLON);
					expect(TOK_IDENTIFIER);
					MetaData();
					expect(TOK_SEMICOLON);
				}
				break;

			// object
			case TOK_IDENTIFIER:
				ISO_ASSERT(0);

			case TOK_EOF:
				return;

			default:
				ISO_ASSERT(0);
		}

	}
}

} // namespace flatbuffers

template<> struct ISO::def<flatbuffers::String> : def<string_base<_pascal_string<uint32, char>>> {};

template<typename T> struct ISO::def<flatbuffers::Offset<T>> : ISO::VirtualT2<flatbuffers::Offset<T>> {
	static ISO::Browser2	Deref(flatbuffers::Offset<T> &a)	{ return ISO::MakeBrowser(*a); }
};

template<typename T> struct ISO::def<flatbuffers::Vector<T>> : ISO::VirtualT2<flatbuffers::Vector<T>> {
	typedef flatbuffers::Vector<T>	A;
//	static uint32		Count(A &a)			{ return a.size(); }
//	static ISO::Browser2	Index(A &a, int i)	{ return ISO::MakeBrowser(a[i]); }
	static ISO::Browser2	Deref(A &a)			{ return ISO_ptr<memory_block_deref>(0, a.data(), a.size()); }
};

ISO::Browser2 MakeISO_noarray(flatbuffers::schema::Type type, const void *data);
ISO::Browser2 MakeISO(flatbuffers::schema::Type type, const void *data);

struct ISOFlatBufferTable : ISO::VirtualDefaults {
	const flatbuffers::Table		*table;
	flatbuffers::schema::UserType	type;
	ISOFlatBufferTable(const flatbuffers::Table *table, flatbuffers::schema::UserType type) : table(table), type(type) {}

	uint32			Count()			{ return type.count; }
	tag2			GetName(int i)	{ return type.Entries()[i].id; }

	ISO::Browser2	Index(int i) {
		auto	&field	= type.Entries()[i];
		auto	data	= table->GetAddressOf(i * 2 + 4);
		if (!data && field.type.simple())
			data = (const uint8*)&field.default_value;
		return MakeISO(field.type, data);
	}
	int				GetIndex(const tag2& id, int from)	{
		tag	id2 = id;
		for (auto &i : type.Entries().slice(from)) {
			if (i.id == id2)
				return type.Entries().index_of(i);
		}
		return -1;
	}
};
ISO_DEFUSERVIRT(ISOFlatBufferTable);

struct ISOFlatBufferTableArray : ISO::VirtualDefaults {
	const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>> *vec;
	flatbuffers::schema::UserType	type;
	ISOFlatBufferTableArray(const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>> *vec, flatbuffers::schema::UserType type) : vec(vec), type(type) {}

	uint32			Count()			{ return vec->size(); }
	ISO::Browser2	Index(int i)	{ return MakePtr(0, ISOFlatBufferTable(vec->Get(i), type)); }
};
ISO_DEFUSERVIRT(ISOFlatBufferTableArray);

struct ISOFlatBufferEnumArray : ISO::VirtualDefaults {
	const flatbuffers::VectorOfAny	*vec;
	flatbuffers::schema::EnumType	type;
	uint32							stride;
	ISOFlatBufferEnumArray(const flatbuffers::VectorOfAny *vec, flatbuffers::schema::EnumType type) : vec(vec), type(type) {
		stride	= size(type.representation);
	}
	uint32			Count()			{ return vec->size(); }
	ISO::Browser2	Index(int i)	{ return MakeISO_noarray(type, vec->Data() + stride * i); }
};
ISO_DEFUSERVIRT(ISOFlatBufferEnumArray);

struct ISOFlatBuffer : ISO::VirtualDefaults {
	malloc_block						data;
	const flatbuffers::schema::Schema	&schema;
	ISOFlatBuffer(malloc_block &&data, const flatbuffers::schema::Schema &schema) : data(move(data)), schema(schema) {}

	ISO::Browser2	Deref() { return MakeISO(schema.root, data); }
	int				GetIndex(const tag2& id, int from)	{ return Deref().GetIndex(id, from); }
	ISO::Browser2	Index(int i)						{ return Deref().Index(i); }
};
ISO_DEFUSERVIRT(ISOFlatBuffer);

ISO::Browser2 MakeISO_noarray(flatbuffers::schema::Type type, const void *data) {
	using namespace flatbuffers;
	using namespace schema;

	switch (type.type) {
		case Type::BOOL:	return ISO::MakeBrowser(*(bool   *)data);
		case Type::INT8:	return ISO::MakeBrowser(*(int8   *)data);
		case Type::UINT8:	return ISO::MakeBrowser(*(uint8  *)data);
		case Type::INT16:	return ISO::MakeBrowser(*(int16  *)data);
		case Type::UINT16:	return ISO::MakeBrowser(*(uint16 *)data);
		case Type::INT32:	return ISO::MakeBrowser(*(int32  *)data);
		case Type::UINT32:	return ISO::MakeBrowser(*(uint32 *)data);
		case Type::INT64:	return ISO::MakeBrowser(*(int64  *)data);
		case Type::UINT64:	return ISO::MakeBrowser(*(uint64 *)data);
		case Type::FLOAT32:	return ISO::MakeBrowser(*(float32*)data);
		case Type::FLOAT64:	return ISO::MakeBrowser(*(float64*)data);
		case Type::STRING:	return ISO::MakeBrowser(**(Offset<String>*)data);

		case Type::ENUM: {
			auto	v = read_bytes<int64>(data, size(type.representation));
			for (auto &i : ((EnumType&)type).Entries()) {
				if (i.value == v)
					return ISO::MakeBrowser(i.id);
			}
			return MakePtr(tag(), v);
		}
		case Type::UNION: {
			auto	v = ((uint16*)data)[-1];
			if (v == 0)
				return ISO::MakeBrowser((const char*)"NONE");
			for (auto &i : ((UnionType&)type).Entries()) {
				if (i.value == v)
					return MakeISO(i.type, data);
			}
			return MakePtr(tag(), v);
		}

//		case Type::STRUCT:	return ISO_NULL;
		case Type::TABLE:	return MakePtr(tag(), ISOFlatBufferTable(*(Offset<Table>*)data, (UserType&)type));
		default:			return ISO_NULL;

	}
}

ISO::Browser2 MakeISO(flatbuffers::schema::Type type, const void *data) {
	if (!data)
		return ISO_NULL;

	using namespace flatbuffers;
	using namespace schema;

	if (type.array) {
		auto	start	= (char*)data + ReadScalar<uoffset_t>(data);
		switch (type.type) {
			case Type::BOOL:	return ISO::MakeBrowser(*(Vector<bool	>*)start);
			case Type::INT8:	return ISO::MakeBrowser(*(Vector<int8	>*)start);
			case Type::UINT8:	return ISO::MakeBrowser(*(Vector<uint8	>*)start);
			case Type::INT16:	return ISO::MakeBrowser(*(Vector<int16	>*)start);
			case Type::UINT16:	return ISO::MakeBrowser(*(Vector<uint16	>*)start);
			case Type::INT32:	return ISO::MakeBrowser(*(Vector<int32	>*)start);
			case Type::UINT32:	return ISO::MakeBrowser(*(Vector<uint32	>*)start);
			case Type::INT64:	return ISO::MakeBrowser(*(Vector<int64	>*)start);
			case Type::UINT64:	return ISO::MakeBrowser(*(Vector<uint64	>*)start);
			case Type::FLOAT32:	return ISO::MakeBrowser(*(Vector<float32>*)start);
			case Type::FLOAT64:	return ISO::MakeBrowser(*(Vector<float64>*)start);
			case Type::STRING:	return ISO::MakeBrowser(*(Vector<Offset<String>>*)start);

			case Type::ENUM:	return MakePtr(tag(), ISOFlatBufferEnumArray((VectorOfAny*)start, (EnumType&)type));
//			case Type::UNION:
//			case Type::STRUCT:
			case Type::TABLE:	return MakePtr(tag(), ISOFlatBufferTableArray((Vector<Offset<Table>>*)start, (UserType&)type));
			default:			return ISO_NULL;
		}

	} else {
		return MakeISO_noarray(type, data);
	}
}

struct FlatBuffersUserType : static_hash<FlatBuffersUserType, const char*>, ISO::TypeUser, flatbuffers::schema::Schema, FileHandler {
	string	desc;
	const char* GetDescription() override { return desc; }
	const char*	GetExt() override { return extension; }
	int			Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32le>() == 0x18 ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void> Read(tag id, istream_ref file) override {
		auto data = malloc_block::unterminated(file);
		flatbuffers::Verifier verifier(data, data.length());
		return MakePtr(this, id, ISOFlatBuffer(move(data), *this));
	}

//	FlatBuffersUserType() : ISO::TypeUser(0) {}
	FlatBuffersUserType(flatbuffers::schema::Schema &&schema) : base(schema.identifier), TypeUser(schema.identifier, ISO::getdef<ISOFlatBuffer>()), flatbuffers::schema::Schema(move(schema)) {
		desc = format_string("FlatBuffer(%s)", identifier.begin());
	}
};

//hash_map<uint32, FlatBuffersUserType>	schemas;

class FlatbufferFileHandler : public FileHandler {
	const char* GetDescription() override { return "Google FlatBuffer"; }
	int			Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32le>() == 0x18 ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		auto data = malloc_block::unterminated(file);
		flatbuffers::Verifier verifier(data, data.length());

		FlatBuffersUserType	*schema = FlatBuffersUserType::get(flatbuffers::GetBufferIdentifier(data));
		if (schema)
			return MakePtr(schema, id, ISOFlatBuffer(move(data), *schema));

//		auto	*schema	= flatbuffers::reflection::GetSchema(data);
//		auto	*root	= flatbuffers::GetRoot<flatbuffers::reflection::Object>(data);
//		bool	ok		= Verify(*schema, *root, data, data.length());
		return ISO_NULL;
	}
public:
	FlatbufferFileHandler() {
		flatbuffers::schema::Schema	schema;
//		flatbuffers::tokeniser(text_reader<reader_intf>(FileInput("D:\\dev\\Github\\tensorflow\\tensorflow\\lite\\schema\\schema.fbs").me()), schema).Parse();
//		schemas[*(uint32*)schema.identifier.begin()] = move(schema);
	}
} flatbuffer;

class FlatbufferScemaFileHandler : public FileHandler {
	const char*	GetExt() override { return "fbs"; }
	const char* GetDescription() override { return "Google FlatBuffer Schema"; }

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		flatbuffers::schema::Schema	schema;
		flatbuffers::tokeniser(file, schema).Parse();
		return ISO_NULL;
	}
} fbs;
