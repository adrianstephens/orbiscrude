#include "iso/iso_binary.h"
#include "iso/iso_files.h"

using namespace iso;

class IBFileHandler : public FileHandler {
	const char*		GetExt() override { return "ib"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<ISO::FILE_HEADER>().Check() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	template<int B> bool	Read(ISO_ptr<void, B> &p, tag id, istream_ref file, const char *fn) {
		return file.exists() && ISO::binary_data.Read(p, id, file, fn,
			ISO::root("variables")["keepexternals"].GetInt() ? 0 : ISO::BIN_EXPANDEXTERNALS
		);
	}
	bool			Read(ISO_ptr<void> &p, tag id, istream_ref file) override {
		return Read(p, id, file, 0);
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<void>	p;
		Read(p, id, file, 0);
		return p;
	}
	bool			ReadWithFilename(ISO_ptr<void> &p, tag id, const filename &fn) override {
		return Read(p, id, FileInput(fn).me(), fn);
	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<void>	p;
		Read(p, id, FileInput(fn).me(), fn);
		return p;
	}
	bool			Read64(ISO_ptr<void,64> &p, tag id, istream_ref file) override {
		return Read(p, id, file, 0);
	}
	ISO_ptr<void,64>	Read64(tag id, istream_ref file) override {
		ISO_ptr<void,64>	p;
		Read(p, id, file, 0);
		return p;
	}
	bool			ReadWithFilename64(ISO_ptr<void,64> &p, tag id, const filename &fn) override {
		return Read(p, id, FileInput(fn).me(), fn);
	}
	ISO_ptr<void,64>	ReadWithFilename64(tag id, const filename &fn) override {
		ISO_ptr<void,64>	p;
		Read(p, id, FileInput(fn).me(), fn);
		return p;
	}

	bool					Write(ISO_ptr<void> p, ostream_ref file, const char *fn, uint32 flags)	{
		return ISO::binary_data.Write(p, file, fn, flags
			|	(ISO::root("variables")["expandexternals"].GetInt()	? ISO::BIN_EXPANDEXTERNALS: 0)
			|	(ISO::root("variables")["stringids"].GetInt()		? ISO::BIN_STRINGIDS		: 0)
			|	(ISO::root("variables")["keepenums"].GetInt(1)		? ISO::BIN_ENUMS			: 0)
			|	(ISO::binary_data.IsBigEndian()						? ISO::BIN_BIGENDIAN		: 0)
		);
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return Write(p, file, 0, ISO::BIN_WRITEREADTYPES);
	}
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		return Write(p, FileOutput(fn).me(), fn,
			ISO::root("variables")["relativepaths"].GetInt() ? ISO::BIN_RELATIVEPATHS | ISO::BIN_WRITEREADTYPES : ISO::BIN_WRITEREADTYPES
		);
	}
} ib;
