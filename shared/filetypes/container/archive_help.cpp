#include "iso/iso_files.h"
#include "archive_help.h"

//-----------------------------------------------------------------------------
//	Utilities
//-----------------------------------------------------------------------------

namespace iso {

bool WantRaw()	{ return ISO::root("variables")["raw"].GetInt() != 0; }
bool FindOnly() { return ISO::root("variables")["find_only"].GetInt() != 0; }

filename fixname(const filename &fn) {
	filename	name = fn.name();
	if (name[name.length() - 1] == ']') {
		if (char *p = strrchr(name, '['))
			*p = 0;
	}
	return name;
}

ISO_ptr<Folder> GetDir0(ISO_ptr<Folder> dir, char *name) {
	if (!name[0])
		return dir;

	ISO::Browser	b(dir);
	if (auto b2 = b[name])
		return b2;

	ISO_ptr<Folder> newdir(name);
	dir->Append(newdir);
	return newdir;
}

ISO_ptr<Folder> GetDir(ISO_ptr<Folder> dir, const char *fn) {
	return ISO::Browser2(dir).Parse(fn, ~char_set("/\\"), ISO::getdef<Folder>());
}

ISO_ptr<Folder> GetDir(ISO_ptr<Folder> dir, const char *fn, const char **name) {
	if (fn) {
		if (const char *div = strrchr(fn, '/')) {
			dir		= ISO::Browser2(dir).Parse({fn, div}, ~char_set("/\\"), ISO::getdef<Folder>());
			*name	= div + 1;
		} else {
			*name	= fn;
		}
	} else {
		*name = 0;
	}
	return dir;
}

//ISO_ptr<ISO_openarray_machine<uint8>> ReadRaw(tag id, istream_ref file, uint32 size) {
ISO_ptr<void> ReadRaw(tag id, istream_ref file, uint32 size) {
	return ISO_ptr<malloc_block>(id, file, size);
	//ISO_ptr<ISO_openarray_machine<uint8> >	p(id);
	//p->Resize(file.readbuff(p->Create(size, false), size));
	//return p;
}

ISO_ptr<void> TryRead(FileHandler *fh, const char *name, istream_ref file) {
	try {
		return fh->Read(filename(name).name(), file);
	} catch (const char *error) {
		throw_accum(error << " in " << name);
		return ISO_ptr<void>();
	}
}

ISO_ptr<void> ReadData0(const char *name, istream_ref file, uint32 size, bool raw) {
	FileHandler *fh;
	if (!raw && (fh = FileHandler::Get(filename(name).ext())))
		return TryRead(fh, name, file);
	return ReadRaw(name, file, size);
}

ISO_ptr<void> ReadData1(const char *name, istream_ref file, uint32 size, bool raw) {
	if (size == 0) {
		malloc_block	buffer;
		uint32			buff_size	= 32768;
		uint32			read0, read;
		do {
			buff_size *= 2;
			buffer.resize(buff_size);
			read0	= buff_size - size;
			read	= file.readbuff((char*)buffer + size, read0);
			size	+= read;
		} while (read == read0);

		if (size)
			return ReadData0(name, memory_reader(buffer.resize(size)).me(), size, raw);
		return ISO_NULL;
	}
	return ReadData0(name, file, size, raw);
}

ISO_ptr<void> ReadData2(const char *name, istream_ref file, uint32 size, bool raw) {
	return ReadData1(name, make_reader_offset(file, size), size, raw);
}

streamptr FindLength(istream_ref file) {
	streamptr	len = 0;
	char	buffer[65536];
	while (streamptr r = file.readbuff(buffer, sizeof(buffer)))
		len += r;
	return len;
}

} // namespace iso