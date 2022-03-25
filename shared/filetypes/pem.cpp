#include "iso/iso_files.h"
#include "codec/base64.h"
#include "comms/x509.h"

using namespace iso;

static struct { const char *name; const ASN1::Type *type; } types[] = {
	{"CERTIFICATE",			ASN1::get_type<X509::Certificate>(),			},
//	{"CERTIFICATE REQUEST",	ASN1::get_type<X509::RawValue>(),				},
	{"CERTIFICATE REQUEST",	ASN1::get_type<X509::CertificationRequest>(),	},
	{"RSA PRIVATE KEY",		ASN1::get_type<X509::RSAPrivateKey>(),			},
	{"DH PARAMETERS",		ASN1::get_type<X509::DHParameters>(),			},
	{"DSA PRIVATE KEY",		ASN1::get_type<X509::DSAPrivateKey>(),			},
	{"EC PARAMETERS",		ASN1::get_type<X509::ECParameters>(),			},
	{"EC PRIVATE KEY",		ASN1::get_type<X509::ECPrivateKey>(),			},
};


struct ASN1_ISO : ISO::VirtualDefaults {
	ISO_ptr<ISO_openarray<uint8> >	basedata;
	const ASN1::Type	*type;
	void				*data;
	ASN1_ISO(const ASN1::Type *_type) : basedata(0, _type->size), type(_type), data(*basedata) {}

	void	set(const ASN1::Type *_type, void *_data) {
		type = _type;
		data = _data;
	}

	uint32			Count() {
		if (type->raw) {
			ASN1::Value	*v = (ASN1::Value*)data;
			switch (v->type) {
				case ASN1::TYPE_SEQUENCE:
				case ASN1::TYPE_SET:
					return v->length / sizeof(ASN1::Value);
			}
		}

		switch (type->type) {
			case ASN1::TYPE_SEQUENCE:
			case ASN1::TYPE_SET: {
				const ASN1::TypeCompositeBase	*comp = (const ASN1::TypeCompositeBase*)type;
				return comp->count();
			}

			case ASN1::TYPE_SEQUENCE_OF:
			case ASN1::TYPE_SET_OF: {
				const ASN1::TypeArray	*array = (const ASN1::TypeArray*)type;
				return uint32(array->count(data));
			}
			default:
				return 0;
		}
	}
	tag2			GetName(int i) {
		switch (type->type) {
			case ASN1::TYPE_SEQUENCE:
			case ASN1::TYPE_SET: {
				const ASN1::TypeCompositeBase	*comp = (const ASN1::TypeCompositeBase*)type;
				return (*comp)[i].name;
			}
			default:
				return tag2();
		}
	}
	ISO::Browser2	Index(int i) {
		if (type->raw) {
			ASN1::Value	*v = (ASN1::Value*)data;
			switch (v->type) {
				case ASN1::TYPE_SEQUENCE:
				case ASN1::TYPE_SET: {
					const ASN1::Value	*b = v->get_buffer();
					ISO_ptr<ASN1_ISO>	p(0, *this);
					p->set(ASN1::get_type<ASN1::Value>(), unconst(b) + i);
					return p;
				}
			}
		}
		switch (type->type) {
			case ASN1::TYPE_SEQUENCE:
			case ASN1::TYPE_SET: {
				const ASN1::TypeCompositeBase	*comp	= (const ASN1::TypeCompositeBase*)type;
				const ASN1::Field				&field	= (*comp)[i];
				ISO_ptr<ASN1_ISO>	p(0, *this);
				p->set(field.type, (uint8*)data + field.offset);
				return p;
			}

			case ASN1::TYPE_SEQUENCE_OF:
			case ASN1::TYPE_SET_OF: {
				const ASN1::TypeArray	*array = (const ASN1::TypeArray*)type;
				ISO_ptr<ASN1_ISO>	p(0, *this);
				p->set(array->subtype, array->get_element(data, i));
				return p;
			}
			default:
				return ISO::Browser();
		}
	}
	ISO::Browser2	Deref() {
		if (type->raw) {
			ASN1::Value	*v = (ASN1::Value*)data;
			auto	buffer	= v->get_buffer();
			switch (v->type) {
				case ASN1::TYPE_BOOLEAN:		return ISO::MakeBrowser(*(const bool*)buffer); break;

				case ASN1::TYPE_GRAPHIC_STRING:
				case ASN1::TYPE_ISO64_STRING:
				case ASN1::TYPE_GENERAL_STRING:
				case ASN1::TYPE_UNIVERSAL_STRING:
				case ASN1::TYPE_CHARACTER_STRING:
				case ASN1::TYPE_BMP_STRING:
				case ASN1::TYPE_NUMERIC_STRING:
				case ASN1::TYPE_PRINTABLE_STRING:
				case ASN1::TYPE_T61_STRING:
				case ASN1::TYPE_VIDEOTEX_STRING:
				case ASN1::TYPE_IA5_STRING:
				case ASN1::TYPE_UTF8_STRING:	return ISO_ptr<string>(0, str8(buffer)); break;

				case ASN1::TYPE_REAL:			return MakePtr(tag2(), v->get_float()); break;
				case ASN1::TYPE_INTEGER:		return ISO_ptr<string>(0, to_string(hex(mpi(buffer))));

				case ASN1::TYPE_EOC:
				case ASN1::TYPE_OCTET_STRING:
				case ASN1::TYPE_OBJECT_ID:
				case ASN1::TYPE_OBJECT_DESCRIPTOR:
				case ASN1::TYPE_EXTERNAL:
				case ASN1::TYPE_ENUMERATED:
				case ASN1::TYPE_EMBEDDED_PDV:
				case ASN1::TYPE_REL_OBJECT_ID:
				case ASN1::TYPE_SEQUENCE:
				case ASN1::TYPE_SET:
				case ASN1::TYPE_UTC_TIME:
				case ASN1::TYPE_GENERALIZED_TIME:
				default:
					return ISO_ptr<const_memory_block>(0, v->get_buffer());
			}
		}
		if (type == ASN1::get_type<mpi>())
			return ISO_ptr<string>(0, to_string(hex(*(mpi*)data)));

		if (type == ASN1::get_type<ASN1::ResolvedObjectID>())
			return ISO::MakeBrowser(((ASN1::ResolvedObjectID*)data)->oid->name);

		if (type == ASN1::get_type<DateTime>())
			return ISO_ptr<string>(0, to_string(*(DateTime*)data));

		switch (type->type) {
			case ASN1::TYPE_BOOLEAN:		return ISO::MakeBrowser(*(bool*)data); break;
//			case ASN1::TYPE_BIT_STRING:		return ISO::MakeBrowser(*(dynamic_bitarray<>*)data); break;
			case ASN1::TYPE_INTEGER:
				if (type->size <= 8)
					return MakePtr(tag2(), *(int64*)data & bits64(type->size * 8));
				return ISO_ptr<string>(0, to_string(hex(mpi(const_memory_block(data, type->size), false))));

			case ASN1::TYPE_EOC:
			case ASN1::TYPE_OCTET_STRING:
			case ASN1::TYPE_OBJECT_ID:
			case ASN1::TYPE_OBJECT_DESCRIPTOR:
			case ASN1::TYPE_EXTERNAL:
			case ASN1::TYPE_REAL:
			case ASN1::TYPE_ENUMERATED:
			case ASN1::TYPE_EMBEDDED_PDV:
			case ASN1::TYPE_UTF8_STRING:
			case ASN1::TYPE_REL_OBJECT_ID:
			case ASN1::TYPE_SEQUENCE:
			case ASN1::TYPE_SET:
			case ASN1::TYPE_NUMERIC_STRING:
			case ASN1::TYPE_PRINTABLE_STRING:
			case ASN1::TYPE_T61_STRING:
			case ASN1::TYPE_VIDEOTEX_STRING:
			case ASN1::TYPE_IA5_STRING:
			case ASN1::TYPE_UTC_TIME:
			case ASN1::TYPE_GENERALIZED_TIME:
			case ASN1::TYPE_GRAPHIC_STRING:
			case ASN1::TYPE_ISO64_STRING:
			case ASN1::TYPE_GENERAL_STRING:
			case ASN1::TYPE_UNIVERSAL_STRING:
			case ASN1::TYPE_CHARACTER_STRING:
			case ASN1::TYPE_BMP_STRING:
			default:
				return ISO_ptr<memory_block>(0, memory_block(data, type->size));
			case ASN1::TYPE_CHOICE:
				return ISO_ptr<memory_block>(0, memory_block(data, type->size));
		}
	}
};

ISO_DEFVIRT(ASN1_ISO);

class PEMFileHandler : public FileHandler {
	const char*		GetExt() override { return "pem"; }
	int				Check(istream_ref file) override {
		char	test[11];
		file.seek(0);
		file.read(test);
		return str(test, test+11) == "-----BEGIN " ? CHECK_PROBABLE : CHECK_NO_OPINION;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything>	p(id);

		string			s(file.length());
		file.readbuff(s, file.length());

		string_scan		ss(s);

		while (ss.remaining()) {
			if (!ss.scan_skip("-----BEGIN "))
				break;

			count_string	name	= ss.get_token(~char_set('-'));
			const char*		begin	= ss.scan_skip('\n');
			const char		*end	= ss.scan_skip("-----END ");

			if (!end)
				break;

			const ASN1::Type *type = 0;
			for (auto &i : types) {
				if (name == i.name) {
					type = i.type;
					break;
				}
			}

			auto	data = transcode(base64_decoder(), const_memory_block(begin, end));
			if (!type)
				type = ASN1::get_type<ASN1::Value>();

			ISO_ptr<ASN1_ISO>	p2(0, type);
			if (ASN1::Read(lvalue(memory_reader(data)), p2->data, type))
				p->Append(p2);
		}

		return p;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return false;
	}
} pem;



class DERFileHandler : public FileHandler {
	const char*		GetExt() override { return "der"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		const ASN1::Type*	type = ASN1::get_type<ASN1::Value>();
		ISO_ptr<anything>	p(id);
		while (!file.eof()) {
			ISO_ptr<ASN1_ISO>	p2(0, type);
			ASN1::Read(file, p2->data, type);
			p->Append(p2);
		}
		return p;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return false;
	}
} der;
