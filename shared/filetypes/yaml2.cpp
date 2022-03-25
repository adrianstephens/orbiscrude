#include "iso/iso_files.h"
#include "extra/yaml.h"
#include "codec/base64.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	YAML
//-----------------------------------------------------------------------------

const char	YAMLmagic[]		= "%YAML 1.1";

class YAMLFileHandler : public FileHandler {
	struct YAMLtoISO {
		dynamic_array<ISO_ptr<anything> > stack;
		void		Begin(const char *tag, const void *key, YAMLreader::CONSTRUCT c) {
			stack.push_back().Create(0);
		}
		void*		End(YAMLreader::CONSTRUCT c) {
			return stack.pop_back_retref();
		}
		void*		Value(const char *tag, const void *key, const char *val);

		void		Entry(const void *key, void *val) {
			ISO_ptr<void>	p = ISO_ptr<void>::Ptr(val);
			if (key && ISO_ptr<void>::Ptr(unconst(key)).IsType<string>())
				p.SetID(*(const char**)key);
			stack.back()->Append(p);
		}

		template<typename T> static void *make_value(const T &t) {
			ISO_ptr<T>	p(0, t);
			p.Header()->addref();
			return p;
		}

		YAMLtoISO() {
			stack.push_back().Create(0);
		}
	};

	void	Write(YAMLwriter &json, ISO::Browser b);

	const char*		GetExt() override {
		return "yaml";
	}
	int				Check(istream_ref file) override {
		file.seek(0);
		char compare[sizeof(YAMLmagic) - 1];
		return file.read(compare) && str(compare).begins(YAMLmagic) ? CHECK_PROBABLE : CHECK_NO_OPINION;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		YAMLtoISO	yaml2iso;
		YAMLreader	reader(&yaml2iso, file);
		try {
			reader.Read();
			return yaml2iso.stack[0];
		} catch (const char *error) {
			throw_accum(error << " at line " << reader.GetLineNumber());
			return ISO_ptr<void>();
		}
	}
} yaml;

void *YAMLFileHandler::YAMLtoISO::Value(const char *tag, const void *key, const char *val) {
	if (tag) {
		if (str(tag).begins("tag:yaml.org,2002:")) {
			auto	sub = str(tag).slice(sizeof("tag:yaml.org,2002:") - 1);
			if (sub == "binary") {
				ISO_ptr<ISO_openarray<uint8> > p(0, make_range<uint8>(transcode(base64_decoder(), val)));
				p.Header()->addref();
				return p;

			} else if (sub == "bool") {
				bool	v;
				bool	ok;
				switch (to_lower(val[0])) {
					case 'y':	v = true; ok = istr(val) == "y" || istr(val) == "yes"; break;
					case 'n':	v = false; ok = istr(val) == "n" || istr(val) == "no"; break;
					case 't':	v = true; ok = istr(val) == "true"; break;
					case 'f':	v = false; ok = istr(val) == "false"; break;
					case 'o':	v = istr(val) == "on"; ok = v || istr(val) == "off"; break;
					default:	ok = false;
				}
				if (!ok)
					throw("Bad bool");
				return make_value(v);

			} else if (sub == "int") {
				return make_value(from_string<int64>(val));

			} else if (sub == "float") {
				return make_value(from_string<float>(val));
			}
		}
	}

	if (val && (char_set::digit | char_set("+-")).test(val[0])) {
		const char *d = val + int(!char_set::digit.test(val[0]));
		if (val[0] == '0' ? (
				(val[1] == 'x' && string_check(val + 2, char_set("0123456789abcdefABCDEF")))
			||	(val[1] == 'b' && string_check(val + 2, char_set("01")))
			||	string_check(val, char_set("01234567"))
		) : string_check(val, char_set::digit))
			return make_value(from_string<int64>(val));
	}

	ISO_ptr<string>	s(0, val);
	ISO::Value		*v	= s.Header();
	v->addref();
	return s;
}

