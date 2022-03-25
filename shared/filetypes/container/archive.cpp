#include "iso/iso_files.h"
#include "comms/zlib_stream.h"
#if HAVE_BZLIB
#include "comms/bz2_stream.h"
#endif
#include "codec/base64.h"
#include "archive_help.h"
#include "comms/zip.h"
#include "comms/gz.h"
#include "tar.h"
#include "filetypes/bin.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	BASE64
//-----------------------------------------------------------------------------

class BASE64FileHandler : public FileHandler {
	const char*		GetDescription() override	{ return "Base64 encode data"; }
	int				Check(istream_ref file) override {
		char	buffer[64];
		file.seek(0);
		uint32	n = file.readbuff(buffer, sizeof(buffer));
		return n > 16 && !string_find(buffer, ~char_set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\r\n"), buffer + n) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		if (auto d = transcode(base64_decoder(), malloc_block::unterminated(file)))
			return ISO_ptr<ISO_openarray<uint8> >(id, make_range<uint8>(d));
		return ISO_NULL;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override	{
		if (memory_block m = GetRawData(p))
			return file.write(transcode(base64_encoder(), m));
		return false;
	}
} base64;

#ifdef BZ2_STREAM_H

//-----------------------------------------------------------------------------
//	BZIP2
//-----------------------------------------------------------------------------

filename fixname(const filename &fn) {
	filename	name = fn.name();
	if (name[name.length() - 1] == ']') {
		if (char *p = strrchr(name, '['))
			*p = 0;
	}
	return name;
}

class BZ2FileHandler : public FileHandler {
	const char*		GetExt() override { return "bz2"; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		streamptr	fp	= file.tell();
		BZ2istream	bzs(file);
		char		dummy[1024];
		uint32		size	= 0;

		while (uint32 got = bzs.readbuff(dummy, 1024))
			size += got;

		file.seek(fp);
		return ReadRaw(id, BZ2istream(file).me(), size);
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		FileInput	file(fn);
		filename	name = (const char*)fixname(fn);
		if (FileHandler *fh = FileHandler::Get(name.ext()))
			return fh->Read(name.name(), BZ2istream(file).me());
		return Read(id, file);
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override	{
		if (memory_block m = GetRawData(p)) {
			BZ2ostream(file, 1).writebuff(m, m.length());
			return true;
		}
		return false;
	}

	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		FileOutput	file(fn);
		if (!file.exists())
			return false;
		if (FileHandler *fh = FileHandler::Get(filename(fn.name()).ext()))
			return fh->Write(p, BZ2ostream(file, 1).me());
		return Write(p, file);
	}
} bz2;
#endif

//-----------------------------------------------------------------------------
//	TAR
//-----------------------------------------------------------------------------
class TARFileHandler : public FileHandler {
	friend class TGZFileHandler;

	void					WriteDir(anything *dir, ostream_ref file, char *name);

	const char*		GetExt() override { return "tar"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		TARheader	th;
		return file.read(th) && th.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} tar;

ISO_ptr<void> TARFileHandler::Read(tag id, istream_ref file) {
	TARheader	th = file.get();

	if (th.filename[0] == 0x1f && (uint8)th.filename[1] == 0x8b) {
		file.seek(0);
		GZheader	gz;
		if (!file.read(gz))
			return ISO_NULL;

		return Read(id, GZistream(file).me());
	}

	if (!th.valid())
		return ISO_NULL;

	ISO_ptr<anything> t(id);
	bool	raw = WantRaw();
	char	name_buffer[1024];

	for (;th.filename[0]; th = file.get()) {
		char	*name	= th.filename;
		uint64	size;
		if (th.link == 'L') {
			get_num_base<8>(skip_whitespace(th.filesize), size);
			file.readbuff(name_buffer, size_t(size));
			file.seek_cur((512 - size) & 511);
			file.read(th);
			name	= name_buffer;
			file.seek_cur(512 - sizeof(th));
		}

		get_num_base<8>(skip_whitespace(th.filesize), size);
		streamptr	end = (file.tell() + size + 511) & ~511;

		if (!str(name).find("PaxHeader")) {
			ISO_ptr<Folder> dir = t;
			if (char *div = strrchr(name, '/')) {
				*div++ = 0;
				dir = GetDir(dir, name);
				name = div;
			}

			if (*name) {
				if (th.link == '2') {
					if (ISO_ptr<void> p = (*dir)[th.linkedfile]) {
						dir->Append(ISO_ptr<ISO_ptr<void> >(name, p));
					}
				} else {
					dir->Append(ReadData2(name, file, uint32(size), raw));
				}
			}
		}

		file.seek(end);
	}


	malloc_block	buffer(65536);
	malloc_block	buffer2;
	while (size_t r = file.readbuff(buffer, 65536))
		buffer2 += buffer.slice_to(r);

	if (uint32 extra = buffer2.size32()) {
		ISO_ptr<ISO_openarray<uint8> > p("extra");
		memcpy(p->Create(extra), buffer2, extra);
		t->Append(p);
	}

	if (t->Count() == 1 && (*t)[0].IsID((const char*)id))
		return (*t)[0];

	return t;
}

void TARFileHandler::WriteDir(anything *dir, ostream_ref file, char *name) {
	char	*name_end = name + strlen(name);

	for (int i = 0, n = dir->Count(); i < n; i++) {
		ISO_ptr<void>	p		= (*dir)[i];
		if (!p)
			continue;

		strcpy(name_end, p.ID().get_tag());

		if (p.IsType<anything>()) {
			strcat(name, "/");
			WriteDir(p, file, name);
		}

		memory_block	m	= GetRawData(p);
		TARheader		th(name, 0100644, 0001753, 0001001, m.length(), 0);

		file.write(th);
		file.writebuff(m, m.length());
		file.align(512, 0);
	}
}

bool TARFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (!p.IsType<anything>())
		return false;

	char	name[256] = "";
	WriteDir(p, file, name);
	return true;
}

//-----------------------------------------------------------------------------
//	GZIP
//-----------------------------------------------------------------------------

class GZFileHandler : public FileHandler {
	ISO_ptr<void>	Read1(istream_ref file, const char *name0) {
		GZheader	gz(name0);
		if (!file.read(gz))
			return ISO_NULL;

		tag id	= gz.name.name();
		if (!WantRaw()) {
			if (FileHandler *fh = FileHandler::Get(gz.name.ext()))
				return fh->Read(id, GZistream(file).me());
		}
		return ReadData1(id, GZistream(file).me(), 0, true);
	}
	ISO_ptr<void>	Read(tag id, istream_ref file, const char *name0) {
		ISO_ptr<void> p1 = Read1(file, name0);
		if (p1) {
			if (ISO_ptr<void> p2 = Read1(file, 0)) {
				ISO_ptr<anything>	a(id);
				a->Append(p1);
				a->Append(p2);
				while (p2 = Read1(file, 0))
					a->Append(p2);
				return a;
			}
			p1.SetID(id);
		}
		return p1;
	}

	const char*		GetExt() override { return "gz";	}

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.getc() == 0x1f && file.getc() == 0x8b ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		return Read(id, FileInput(fn).me(), fn.name());
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return Read(id, file, NULL);
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		if (memory_block m = GetRawData(p)) {
			file.write(GZheader());

			GZostream(file).writebuff(m, m.length());
			return true;
			//if (zlib_writer(file).writebuff(m, m.length())) {
			//	file.write(iso::crc32(m, m.length()));
			//	file.write(m.size32());
			//	return true;
			//}
		}
		return false;
	}
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		FileOutput	file(fn);
		if (!file.exists())
			return false;

		if (FileHandler *fh = FileHandler::Get(filename(fn.name()).ext())) {
			file.write(GZheader(fn.name()));

			return fh->Write(p, GZostream(file));

			//zlib_writer		zlib(file);
			//CRCostream		crc(zlib);
			//if (fh->Write(p, crc)) {
			//	file.write((uint32)crc);
			//	file.write(uint32(crc.output()));
			//	return true;
			//}
			//return false;
		}
		return Write(p, file);
	}
} gz;

class TGZFileHandler : public FileHandler {
	const char*		GetExt() override { return "tgz"; }

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		GZheader	gz;
		if (!file.read(gz))
			return ISO_NULL;

		return tar.Read(id, GZistream(file).me());
	}
#if 0
	bool			Write(ISO_ptr<void> p, ostream_ref file) override	{
		if (memory_block m = GetRawData(p)) {
			file.write(GZheader());
			GZostream(file).writebuff(m, m.length());
			return true;
		}
		return false;
	}
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		FileOutput	file(fn);
		if (!file.exists())
			return false;

		if (FileHandler *fh = FileHandler::Get(filename(fn.name()).ext())) {
			file.write(GZheader(fn.name()));
			return fh->Write(p, GZostream(file));
		}
		return Write(p, file);
	}
#endif
} tgz;

//-----------------------------------------------------------------------------
//	ZLIB
//-----------------------------------------------------------------------------

struct _ZLIBFileHandler : public FileHandler {
public:
	static int _Check(istream_ref file) {
		if (file.getc() != 0x78)
			return CHECK_DEFINITE_NO;
		switch (file.getc()) {
			case 0xDA:
			case 0x9C:
			case 0x5E:
				return CHECK_PROBABLE;
		}
		return CHECK_UNLIKELY;
	}
};

class ZLIBFileHandler : public _ZLIBFileHandler {
	const char*		GetExt() override			{ return "z"; }
	const char*		GetDescription() override	{ return "ZLIB compressed data"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		return _Check(file);
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return ReadRaw(id, zlib_reader(file).me(), 0x100000);
	}
} zlibfh;

class ZLIBLenFileHandler : public _ZLIBFileHandler {
	const char*		GetDescription() override	{ return "ZLIB compressed data with length"; }
	int				Check(istream_ref file) override {
		file.seek(4);
		return _Check(file);
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		uint32	len = file.get<uint32le>();
		return ReadRaw(id, zlib_reader(file).me(), len);
	}
} zliblenfh;

class ZLIBRawFileHandler : public FileHandler {
	const char*		GetExt() override			{ return "rawz"; }
	const char*		GetDescription() override	{ return "ZLIB compressed data without header"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return ReadRaw(id, deflate_reader(file).me(), 0x100000);
	}
} zlibrawfh;


//-----------------------------------------------------------------------------
//	ZIP
//-----------------------------------------------------------------------------

struct ZIProot : refs<ZIProot> {
	istream_ptr			file;
	string				pw;
	ZIProot(istream_ref	file, const char *_pw = 0) : file(file.clone()), pw(_pw) {}
};

struct ZIPentry : ISO::VirtualDefaults {
	ref_ptr<ZIProot>	r;
	ZIPfile0			zf;
	streamptr			p;

	ZIPentry(ZIProot *r, const ZIPfile0 &zf, streamptr p) : r(r), zf(zf), p(p) {}

	uint32			Count() {
		return zf.Length();
	}
	ISO_ptr<void>	Deref()	{
		r->file.seek(p);
		return ReadRaw(0, zf.Reader(r->file), zf.Length());
	}
};
ISO_DEFUSERVIRTX(ZIPentry, "File");

class ZIPFileHandlerBase : public FileHandler, protected ZIP {
protected:
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		const char	*pw		= ISO::root("variables")["password"].GetString();

		if (auto r = ZIPreaderCD(file)) {
			ZIProot				*ziproot = new ZIProot(file, pw);
			ISO_ptr<anything>	t(id);

			for (ZIPfile zf; r.Next(zf);) {
				ISO_ptr<Folder> dir		= t;
				filename		fn		= zf.fn;
				char			*name	= strrchr(fn, '/');
				if (name) {
					*name++	= 0;
					dir		= GetDir(dir, fn);
				} else {
					name	= fn;
				}

				if (*name)
					dir->Append(ISO_ptr<ZIPentry>(name, ZIPentry(ziproot, zf, file.tell())));
			}

			return t;
		}
		file.seek(0);

		ZIPreader			r(file);
		ISO_ptr<anything>	t(id);
		bool				raw			= WantRaw();
		int					find_only	= FindOnly();

		for (ZIPfile zf; r.Next(zf);) {
			if (find_only) {
				++find_only;

			} else {
				ISO_ptr<Folder> dir		= t;
				filename		fn		= zf.fn;
				char			*name	= strrchr(fn, '/');
				if (name) {
					*name++	= 0;
					dir		= GetDir(dir, fn);
				} else {
					name	= fn;
				}

				if (zf.Length() || zf.UnknownSize()) {
					if (ISO_ptr<void> p = ReadData1(name, zf.Reader(file, pw).me(), zf.Length(), raw))
						dir->Append(p);
				}
			}
		}

		if (find_only > 1)
			return ISO_ptr<bool>(id, true);

		return t;
	}
#if 0
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		FileInput			file(fn);
		ZIPreaderCD			r(file);
		ZIProot				*ziproot = new ZIProot(fn, ISO::root("variables")["password"].GetString());
		ISO_ptr<anything>	t(id);

		for (;;) {
			ZIPfile		zf;
			if (!r.Next(zf))
				return t;

			ISO_ptr<Folder> dir = t;
			filename	fn	= zf.fn;
			char	*name;
			if ((name = strrchr(fn, '/'))) {
				*name++	= 0;
				dir		= GetDir(dir, fn);
			} else {
				name	= fn;
			}

			if (*name)
				dir->Append(ISO_ptr<ZIPentry>(name, ZIPentry(ziproot, zf, file.tell())));
		}
		return t;
	}
#endif

	void Write(ZIPwriter &w, ISO::Browser2 b, char *name, const char *password) {
		const char *id = b.GetName().get_tag();

		while (b.SkipUser().GetType() == ISO::REFERENCE || b.SkipUser().IsVirtPtr()) {
			b = *b;
			if (!id)
				id = b.GetName().get_tag();
		}

		strcat(name, id);

		if (IsRawData(b)) {
			char			random[10];
			if (password) {
				for (int i = 0; i < 10; i++)
					random[i]	= char(rand() >> 7);
			}
			w.Write(name, GetRawData(b), DateTime::Now(), password, random);

		} else {
			if (id)
				strcat(name, "/");

			char	*name_end = name + strlen(name);
			for (int i = 0, n = b.Count(); i < n; i++) {
				Write(w, b[i], name, password);
				*name_end = 0;
			}
		}
	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ZIPwriter		w(file);
		ISO::Browser2	b(p);
		while (b.SkipUser().GetType() == ISO::REFERENCE || b.SkipUser().IsVirtPtr())
			b = *b;

		char		name[2048];
		const char *password = ISO::root("variables")["password"].GetString();

		if (!IsRawData(b)) {
			for (int i = 0, n = b.Count(); i < n; i++) {
				name[0] = 0;
				Write(w, b[i], name, password);
			}
		} else {
			name[0]		= 0;
			Write(w, p, name, password);
		}
		return true;
	}
};

class ZIPFileHandler : public ZIPFileHandlerBase {
	const char*		GetExt()			override { return "zip";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32le>() == file_header::sig ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
} zip;

class MSHCFileHandler : public ZIPFileHandlerBase {
	const char*		GetExt()			override { return "mshc";	}
	const char*		GetDescription()	override { return "Microsoft Help Container";	}
} mshc;

