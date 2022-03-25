#include "iso/iso_files.h"
#include "extra/json.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	JSON
//-----------------------------------------------------------------------------

class JSONFileHandler : public FileHandler {
	void			Write(JSONwriter &json, ISO::Browser b);
	ISO_ptr<void>	Read(tag id, JSONreader &json);

	const char*		GetExt() override { return "json"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		switch (JSONreader(file).GetToken().type) {
			case JSONreader::Token::ARRAY_BEGIN:
			case JSONreader::Token::OBJECT_BEGIN:
				return	CHECK_POSSIBLE;
			case JSONreader::Token::BAD:
				return	CHECK_DEFINITE_NO;
			default:
				return CHECK_NO_OPINION;
		}
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		JSONreader	json(file);
		try {
			return Read(id, json);
		} catch (const char *error) {
			throw_accum(error << " at line " << json.GetLineNumber());
			return ISO_NULL;
		}
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		JSONwriter	w(file);
		Write(w, ISO::Browser(p));
		return true;
	}
} json;

ISO_ptr<void> JSONFileHandler::Read(tag id, JSONreader &json) {
	JSONreader::Token tok = json.GetToken();
	switch (tok.type) {
		case JSONreader::Token::ARRAY_BEGIN: {
			ISO_ptr<anything>	p(id);
			int	c = skip_whitespace(json);
			for (bool first = true; c != ']'; first = false) {
				if (!first) {
					if (c != ',')
						throw("Missing ','");
					c = skip_whitespace(json);
					if (c == ']')
						break;
				}
				json.put_back(c);
				p->Append(Read(0, json));
				c = skip_whitespace(json);
			}
			return p;
		}

		case JSONreader::Token::OBJECT_BEGIN: {
			ISO_ptr<anything>	p(id);
			int	c = skip_whitespace(json);
			for (bool first = true; c != '}'; first = false) {
				if (!first) {
					if (c != ',')
						throw("Missing ','");
					c = skip_whitespace(json);
					if (c == '}')
						break;
				}
				json.put_back(c);
				tok = json.GetToken().me();
				if (tok.type != JSONreader::Token::ELEMENT_STRING)
					throw("Expected String");
				if (skip_whitespace(json) != ':')
					throw("Expected Colon");
				p->Append(Read(tok.s, json));
				c = skip_whitespace(json);
			}
			return p;
		}

		case JSONreader::Token::ELEMENT_NIL:	return ISO_ptr<ISO_ptr<void> >(id, ISO_NULL);
		case JSONreader::Token::ELEMENT_BOOL:	return ISO_ptr<bool8>(id, !!tok.i);
		case JSONreader::Token::ELEMENT_INT:	return ISO_ptr<int64>(id, tok.i);
		case JSONreader::Token::ELEMENT_FLOAT:	return ISO_ptr<float64>(id, tok.f);
		case JSONreader::Token::ELEMENT_STRING:	return ISO_ptr<string>(id, tok.s);
		case JSONreader::Token::BAD: throw("Syntax Error");
	}
	return ISO_NULL;
}


void JSONFileHandler::Write(JSONwriter &json, ISO::Browser b) {
	bool			names	= false;
	tag2			id		= b.GetName();

	const ISO::Type	*type	= b.GetTypeDef()->SkipUser();
	while (type && TypeType(type->SkipUser()) == ISO::REFERENCE) {
		b		= *b;
		type	= b.GetTypeDef();
	}

	if (type) switch (type->GetType()) {
		case ISO::ARRAY:
		case ISO::OPENARRAY:
			if (TypeType(type->SubType()->SkipUser()) == ISO::REFERENCE) {
				for (ISO::Browser::iterator i = b.begin(), e = b.end(); !names && i != e; ++i)
					names = !!i.GetName();
			}
			break;
		case ISO::COMPOSITE:
			names = true;
	}

	json.Begin(id.get_tag(), names);

	for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
		tag2			id		= i.GetName();
		ISO::Browser		b2		= *i;
		const ISO::Type*	type	= b2.GetTypeDef();
		while (type && TypeType(type->SkipUser()) == ISO::REFERENCE) {
			b2		= *b2;
			type	= b2.GetTypeDef();
		}
		if (type) switch (TypeType(type->SkipUser())) {
			case ISO::INT:
				if (type->Is<bool8>())
					json.Write(id.get_tag(), b2.GetInt() != 0);
				else
					json.Write(id.get_tag(), b2.GetInt());
				break;
			case ISO::FLOAT:
				json.Write(id.get_tag(),  b2.GetFloat());
				break;
			case ISO::STRING:
				json.Write(id.get_tag(), b2.GetString());
				break;
			default:
				Write(json, b2);
		}
	}

	json.End();
}

//-----------------------------------------------------------------------------
//	BSON
//-----------------------------------------------------------------------------
enum BSON_TYPE {
	BSON_end			= 0x00,	// end of list
	BSON_double			= 0x01,	// 64 bit Floating point
	BSON_string			= 0x02,	// UTF-8 string
	BSON_document		= 0x03,	// Embedded document
	BSON_Array			= 0x04,	// as embedded document
	BSON_binary			= 0x05,	// int32 subtype (byte*); Binary data
	BSON_Undefined		= 0x06,	// Deprecated
	BSON_ObjectId		= 0x07,	// 12 bytes
	BSON_Boolean		= 0x08,	// byte 0x00 or 0x01
	BSON_UTCdate		= 0x09,	// int64
	BSON_Null			= 0x0A,	//
	BSON_Regex			= 0x0B,	// cstring, cstring
	BSON_DBPointer		= 0x0C,	// 12 bytes Deprecated
	BSON_JavaScript		= 0x0D,	// string
	BSON_Symbol			= 0x0E,	// Deprecated
	BSON_ScopedJS		= 0x0F,	// int32 string document; JavaScript code w/ scope
	BSON_int32			= 0x10,	// 32-bit Integer
	BSON_Timestamp		= 0x11,	// int64
	BSON_int64			= 0x12,	// 64-bit integer
	BSON_Minkey			= 0xFF,	//
	BSON_Maxkey			= 0x7F,	//
};
enum BSONBIN_TYPE {
	BSONBIN_Generic		= 0x00,
	BSONBIN_Function	= 0x01,
	BSONBIN_Generic_Old	= 0x02,
	BSONBIN_UUID_Old	= 0x03,
	BSONBIN_UUID		= 0x04,
	BSONBIN_MD5			= 0x05,
	BSONBIN_User		= 0x80,
};

class BSONFileHandler : public FileHandler {
	const char*		GetExt() override { return "bson"; }

	string					GetString(istream_ref file);
	ISO_ptr<anything>		ReadDocument(tag id, istream_ref file);

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	a(id);
		streamptr			end	= file.length();

		while (file.tell() < end)
			a->Append(ReadDocument(0, file));
		return a;
	}
} bson;

string BSONFileHandler::GetString(istream_ref file) {
	size_t		len = file.get<uint32le>();
	string		s(len);
	file.readbuff(s.begin(), len);
	return s;
}

ISO_ptr<anything> BSONFileHandler::ReadDocument(tag id, istream_ref file) {
	ISO_ptr<anything>	a(id);

	streamptr	end	= file.tell();
	uint32		len = file.get<uint32le>();
	end	+= len;

	while (BSON_TYPE type = (BSON_TYPE)file.getc()) {
		char	name[256];
		for (char *p = name; *p++ = file.getc(););
		id		= name;
		switch (type) {
			case BSON_double:		a->Append(ISO_ptr<double>(id, file.get<doublele>())); break;
			case BSON_string:		a->Append(ISO_ptr<string>(id, GetString(file))); break;
			case BSON_document:		a->Append(ReadDocument(id, file)); break;
			case BSON_Array:		a->Append(ReadDocument(id, file)); break;
			case BSON_binary: {
				uint32			len	= file.get<uint32le>();
				BSONBIN_TYPE	st	= (BSONBIN_TYPE)file.getc();
				ISO_ptr<ISO_openarray<uint8> >	bin(id);
				file.readbuff(bin->Create(len, false), len);
				a->Append(bin);
				break;
			}
			case BSON_Undefined:	break;
			case BSON_ObjectId:		a->Append(ISO_ptr<array<uint8, 12> >(id, file.get<array<uint8, 12> >())); break;
			case BSON_Boolean:		a->Append(ISO_ptr<bool8>(id, file.getc() != 0)); break;
			case BSON_UTCdate:		a->Append(ISO_ptr<int64>(id, file.get<int64le>())); break;
			case BSON_Null:			a->Append(ISO_ptr<ISO_ptr<void> >(id, ISO_NULL)); break;
			case BSON_Regex: {
				char	*p = name;
				while (*p++ = file.getc());
				char	*b	= p;
				while (*p++ = file.getc());
				a->Append(ISO_ptr<pair<string,string> >(id, pair<string,string>(name, b)));
				break;
			}
			case BSON_DBPointer:	a->Append(ISO_ptr<array<uint8, 12> >(id, file.get<array<uint8, 12> >())); break;
			case BSON_JavaScript:	a->Append(ISO_ptr<string>(id, GetString(file))); break;
			case BSON_Symbol:		a->Append(ISO_ptr<string>(id, GetString(file))); break;
			case BSON_ScopedJS: {
				uint32	len		= file.get<uint32le>();
				string	code	= GetString(file);
				a->Append(ISO_ptr<pair<string, anything> >(id, pair<string, anything>(code, move(*ReadDocument(id, file)))));
				break;
			}
			case BSON_int32:		a->Append(ISO_ptr<int32>(id, file.get<int32le>())); break;
			case BSON_Timestamp:	a->Append(ISO_ptr<int64>(id, file.get<int64le>())); break;
			case BSON_int64:		a->Append(ISO_ptr<int64>(id, file.get<int64le>())); break;
		}
	}
	ISO_ASSERT(file.tell() == end);
	file.seek(end);
	return a;
}
