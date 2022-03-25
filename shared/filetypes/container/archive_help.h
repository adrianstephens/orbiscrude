#ifndef ARCHIVE_HELP_H
#define ARCHIVE_HELP_H

#include "iso/iso.h"
#include "stream.h"

//-----------------------------------------------------------------------------
//	Utilities
//-----------------------------------------------------------------------------

namespace ISO {
	class FileHandler;
}

namespace iso {
	using ISO::FileHandler;

	class filename;
	struct Folder : anything {};

	bool							WantRaw();
	bool							FindOnly();

	ISO_ptr<Folder>					GetDir0(ISO_ptr<Folder> dir, char *name);
	ISO_ptr<Folder>					GetDir(ISO_ptr<Folder> dir, const char *fn);
	ISO_ptr<Folder>					GetDir(ISO_ptr<Folder> dir, const char *fn, const char **name);
//	ISO_ptr<ISO_openarray_machine<uint8>>	ReadRaw(tag id, istream_ref file, uint32 size);
	ISO_ptr<void>					ReadRaw(tag id, istream_ref file, uint32 size);
	ISO_ptr<void>					TryRead(FileHandler *fh, const char *name, istream_ref file);
	ISO_ptr<void>					ReadData1(const char *name, istream_ref file, uint32 size, bool raw);
	ISO_ptr<void>					ReadData2(const char *name, istream_ref file, uint32 size, bool raw);
	streamptr						FindLength(istream_ref file);
}
ISO_DEFUSER(iso::Folder, anything);

#endif	// ARCHIVE_HELP_H
