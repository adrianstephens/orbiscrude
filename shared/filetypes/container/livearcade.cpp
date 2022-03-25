#include "iso/iso_files.h"
#include "archive_help.h"

using namespace iso;

class LiveArcadeFileHandler : public FileHandler {
	struct Header {
		char		filename[40];
		uint8		flags; //0x80 = dir
		uintn<3>	num_sectors2, num_sectors, start_sector;
		int8		a;
		int8		dir;
		uint32be	filelen;
		uint32		c, d;
	};

	const char*		GetExt() override { return "livearcade";		}
	const char*		GetDescription() override { return "Xbox 360 package";}

	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32be>() == 'LIVE' ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		if (file.get<uint32be>() != 'LIVE')
			return ISO_NULL;

		bool				raw = WantRaw();
		ISO_ptr<anything>	t(id);
		anything			array;

		interleaved_reader<istream_ref>	data(file, 0xaa000, 0xd000, 0xab000);

		for (uint32 fp = 0xc000;;) {
			file.seek(fp);

			Header	h	= file.get();
			if (h.filename[0] == 0)
				break;

			ISO_ptr<void>	p;
			if (h.flags & 0x80) {
				p	= ISO_ptr<anything>(h.filename);
			} else {
				uint32	start = h.start_sector;
				data.seek((start - (start < 0xaa)) * 0x1000);
				p = ReadData2(h.filename, data, h.filelen, raw || filename(h.filename).ext() == ".ibz" || filename(h.filename).ext() == ".ib");
			}
			array.Append(p);
			if (h.dir == -1)
				t->Append(p);
			else
				((anything*)array[int(h.dir)])->Append(p);

			fp += sizeof(h);
		}
		return t;
	}

} livearcade;
