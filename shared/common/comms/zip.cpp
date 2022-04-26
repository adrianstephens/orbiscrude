#include "base/algorithm.h"
#include "zlib_stream.h"
#include "zip.h"

using namespace iso;

//-------------------------------------
//	ZIP::encryption
//-------------------------------------

//-------------------------------------
//	ZIPwriter
//-------------------------------------

bool ZIPwriter::Entry::WriteLocal(ostream_ref file) {
	file_header	h(fn);
	return file.write(h.sig, h, fn);
}

bool ZIPwriter::Entry::WriteCD(ostream_ref file) {
	centraldir_entry	cd(fn);
	cd.header.crc	= crc;

	bool needs64 = ((compressed_size | uncompressed_size | offset) >> 32) > 0;

	if (needs64) {
		cd.header.compressed_size	= ~0;
		cd.header.uncompressed_size	= ~0;
		cd.offset					= ~0;
		cd.header.extrafield_length	= sizeof(extension_zip64);
	} else {
		cd.header.compressed_size	= compressed_size;
		cd.header.uncompressed_size	= uncompressed_size;
		cd.offset					= offset;
	}

	return file.write(centraldir_entry::sig, cd, fn) && (!needs64 || file.write(extension_zip64(uncompressed_size, compressed_size, offset)));
}

ZIPwriter::~ZIPwriter() {
	streamptr	fp = file.tell();

	sort(centraldir);

	for (auto& i : centraldir)
		i.WriteCD(file);

	centraldir_end	end;
	clear(end);
	end.total_disk	= end.total_entries = uint16(centraldir.size());
	end.dir_size	= int(file.tell() - fp);
	end.dir_offset	= int(fp);

	file.write(centraldir_end::sig);
	file.write(end);
}

void ZIPwriter::Write(const char *name, const memory_block &data, const DateTime &mod, const char *password, const char *random) {
	auto		&r		= centraldir.emplace_back(name);
	r.mod				= mod;
	r.offset			= file.tell();
	r.crc				= crc32(data);
	r.uncompressed_size	= data.length();

	r.WriteLocal(file);

	streamptr		fp2	= file.tell();

	if (password) {
		encryption	ze(password);
		char		buffer[12];
		for (int i = 0; i < 10; i++)
			buffer[i]	= char(rand() >> 7);
		buffer[10] = char(r.crc >> 16);
		buffer[11] = char(r.crc >> 24);

		ze.decrypt(make_range(buffer));
		file.writebuff(buffer, 12);

		zlib_writer(encrypt_stream(file, ze)).writebuff(data, data.length());
		r.flags	= ENCRYPTION;

	} else {
		zlib_writer(file).writebuff(data, data.length());
	}

	r.compressed_size	= int( file.tell() - fp2);
}
