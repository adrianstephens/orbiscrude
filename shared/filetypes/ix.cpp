#include "iso/iso_script.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

using namespace iso;

class IXFileHandler : public FileHandler {
	const char*		GetExt() override {
		return "ix";
	}
	bool			ReadWithFilename(ISO_ptr<void> &p, tag id, const filename &fn) override {
		return ISO::ScriptRead(p, id, fn, FileInput(fn).me(), ISO::ScriptFlags(), ISO::root("variables")["prefix"].GetString("_"));
	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<void>	p;
		return ReadWithFilename(p, id, fn) ? p : ISO_NULL;
	}
	bool			Read(ISO_ptr<void> &p, tag id, istream_ref file) override {
		return ISO::ScriptRead(p, id, filename(), file, ISO::ScriptFlags(), ISO::root("variables")["prefix"].GetString("_"));
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<void>	p;
		return Read(p, id, file) ? p : ISO_NULL;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		if (p) {
			ISO::ScriptWriter	is(file);
			is.SetFlags(ISO::SCRIPT_IGNORE_DEFER);
			is.DumpType(p.GetType());
			is.putc(' ');
			if (p.IsExternal()) {
				is.format("external \"%s\"", (const char*)p);
			} else {
				is.DumpData(ISO::Browser(p));
			}
			return true;
		}
		return false;
	}
} ix;
#if 0
class IHFileHandler : public FileHandler {
	const char*		GetExt() override {
		return "ih";
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		UserTypeArray	types;
		ISO_script_read_defines(file, types);
		return ISO_ptr<int>(0);
	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		UserTypeArray	types;
		ISO_script_read_defines(fn, types);
		return ISO_ptr<int>(0);
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO::ScriptWriter	is(file);
		for (UserTypeArray::iterator i = user_types.begin(), n = user_types.end(); i != n; i++) {
			if (((*i)->flags & ISO::TypeUser::FROMFILE) && (*i)->subtype) {
				is.puts("define ");
				is.DumpType((*i)->subtype);
				is.format(" %s\n\n", (const char*)(*i)->ID().get_tag());
			}
		}
		return true;
	}
} ih;
#endif
