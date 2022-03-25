#include "iso/iso_binary.h"
#include "iso/iso_files.h"
#include "extra/async_stream.h"
#include "comms/zlib_stream.h"

using namespace iso;

class IBZFileHandler : public FileHandler {
	const char*		GetExt() override { return "ibz";	}

	bool			ReadWithFilename(ISO_ptr<void> &p, tag id, const filename &fn) override {
		async_istream	async(fn);

		istream_ref	file(async);
		auto	c = file.clone();
		return async.exists() && Read(p, id, async);
	}
	bool			Read(ISO_ptr<void> &p, tag id, istream_ref file) override {
		deflate_reader	z(file, file.get<uint32le>());
		#ifdef PLAT_IOS
		return ISO::binary_data.Read(p, id, z, 0, ISO::BIN_EXPANDEXTERNALS);
		#else
		return ISO::binary_data.Read(p, id, z, 0, ISO::BIN_CHUNKEDREAD | ISO::BIN_EXPANDEXTERNALS);
		#endif
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<void> p;
		return Read(p, id, file) ? p : ISO_NULL;
	}

#ifdef CROSS_PLATFORM
	bool			Write(ISO_ptr<void> p, ostream_ref file, const char *fn, uint32 flags)	{
		dynamic_memory_writer	m;
		ISO::binary_data.Write(p, m, fn, flags
			| (ISO::root("variables")["stringids"].GetInt()	? ISO::BIN_STRINGIDS : 0)
			| (ISO::binary_data.IsBigEndian()				? ISO::BIN_BIGENDIAN : 0)
		);
		file.write(uint32le(m.length()));
		deflate_writer(file).write(m.data());
		return true;
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		return Write(p, file, 0, ISO::BIN_WRITEREADTYPES);
	}
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		return Write(p, FileOutput(fn).me(), fn, ISO::BIN_WRITEREADTYPES | (ISO::root("variables")["relativepaths"].GetInt() ? ISO::BIN_RELATIVEPATHS | ISO::BIN_WRITEREADTYPES	: ISO::BIN_WRITEREADTYPES));
	}
#endif
} ibz;
