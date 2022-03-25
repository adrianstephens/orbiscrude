#include "ISO_files.h"
#include "archive_help.h"
#include "com.h"

using namespace iso;

struct ENCINT {
	uint64	v;
	template<class R> bool read(R &file) {
		v = 0;
		int	c;
		while ((c = file.getc()) & 0x80)
			v = (v << 7) | (c & 0x7f);
		v = (v << 7) | c;
		return true;
	}
	operator uint64() const { return v;			}
};

struct ITSF {
	uint32be	id;
	uint32		ver;	//3
	uint32		header_size;
	uint32		_;		//?
	uint32		timestamp;
	uint32		language_id;
	GUID		guid1;
	GUID		guid2;

	struct section {
		uint64	offset, length;
	} sections[2];
};

struct SECTION0 {
	uint64	_;	//0x1fe
	uint64	size;
	uint64	_2;	//0

};

struct SECTION1 {
	uint32be	id;	//ITSP
	uint32		version;
	uint32		length;
	uint32		_;	//0xa
	uint32		chunk_size;		//0x1000
	uint32		density;	//2
	uint32		depth;
	uint32		root_chunk;
	uint32		first_pmgl;
	uint32		last_pmgl;
	uint32		_2;	//-1
	uint32		dir_chunks;
	uint32		language_id;
	GUID		guid;
	uint32		length2;//0x54
	uint32		_3[3];
};

struct DIR_CHUNK {
	uint32be	id;	//PMGL
	uint32		free;
	uint32		_;//0
	uint32		prev_chunk;
	uint32		next_chunk;

	struct ENTRY {
		string	name;
		ENCINT	section, offset, length;
		template<class R> bool read(R &file) {
			ENCINT	len	= file.get();
			file.readbuff(name.alloc(len + 1).getp(), len);
			name.getp()[len] = 0;
			section = file.get();
			offset	= file.get();
			length	= file.get();
			return true;
		}
	};
};

struct INDEX_CHUNK {
	uint32be	id;	//PMGI
	uint32		free;

	struct ENTRY {
		string	name;
		ENCINT	chunk;
		template<class R> bool read(R &file) {
			ENCINT	length	= file.get();
			file.readbuff(name.alloc(length + 1).getp(), length);
			name.getp()[length] = 0;
			chunk	= file.get();
			return true;
		}
	};
};

class CHMFileHandler : public FileHandler {
	const char*		GetExt() override { return "chm";		}
	const char*		GetDescription() override { return "Compiled HTML Help";}

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32be>() == 'ITSF' ? 2 : -1;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ITSF	itsf = file.get();
		if (itsf.id != 'ITSF')
			return ISO_NULL;

		file.seek(itsf.sections[1].offset);
		SECTION1	sect1	= file.get();
		streamptr	chunks	= file.tell();
		streamptr	content	= chunks + sect1.chunk_size * sect1.dir_chunks;
		void		*chunk	= malloc(sect1.chunk_size);

		ISO_ptr<anything> p0(0);

		for (int i = sect1.first_pmgl; i <= sect1.last_pmgl; ++i) {
			file.seek(chunks + i * sect1.chunk_size);
			file.readbuff(chunk, sect1.chunk_size);
			DIR_CHUNK	*dir	= (DIR_CHUNK*)chunk;
			uint8		*end	= (uint8*)chunk + sect1.chunk_size - dir->free;
			byte_reader	r(dir + 1);
			while (r.p < end) {
				DIR_CHUNK::ENTRY	e	= r.get();
				if (e.length && e.name.begins("::DataSpace/")) {
					const char *name;
					ISO_ptr<anything> p2 = GetDir(p0, e.name, &name);
					file.seek(content + e.offset);
					p2->Append(ReadRaw(name, file, size_t(e.length)));
				}
			}
		}
		return p0;
		ISO::Browser	dataspace	= ISO::Browser(p0)["::DataSpace"];
		ISO::Browser	storage		= dataspace["Storage"];

		struct SECTION {
			string	name;
			istream	*file;
		} *sections = 0;

		if (void *namelist = dataspace["NameList"][0]) {
			byte_reader	r(namelist);
			uint32	len	= r.get<uint16>();
			uint32	n	= r.get<uint16>();
			sections	= new SECTION[n];

			for (int i = 0; i < n; i++) {
				uint32		namelen = r.get<uint16>();
				string16	name(namelen + 1);
				r.readbuff(name.getp(), (namelen + 1) * 2);
				sections[i].name	= name;
				if (ISO::Browser b = storage[sections[i].name]["Content"]) {
					if (name == "MSCompressed")
						sections[i].file = new

				}
			}
		}


		ISO_ptr<anything> p(id);

		for (int i = sect1.first_pmgl; i <= sect1.last_pmgl; ++i) {
			file.seek(chunks + i * sect1.chunk_size);
			file.readbuff(chunk, sect1.chunk_size);
			switch (*(uint32be*)chunk) {
				case 'PMGL': {
					DIR_CHUNK	*dir	= (DIR_CHUNK*)chunk;
					uint8		*end	= (uint8*)chunk + sect1.chunk_size - dir->free;
					byte_reader	r(dir + 1);
					while (r.p < end) {
						DIR_CHUNK::ENTRY	e	= r.get();
						if (e.length && !e.name.begins("::DataSpace/")) {
							const char *name;
							ISO_ptr<anything> p2 = GetDir(p, e.name, &name);
							if (e.section == 0) {
								file.seek(content + e.offset);
								p2->Append(ReadRaw(name, file, size_t(e.length)));
								continue;
							}
						}
					}
					break;
				}
				case 'PMGI': {
					INDEX_CHUNK	*index = (INDEX_CHUNK*)chunk;
					break;
				}
			}
		}
		return p;
	}

} chm;
