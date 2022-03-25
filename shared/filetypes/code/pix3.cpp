#include "iso/iso_files.h"

using namespace iso;

class PIX3FileHandler : public FileHandler {
	const char*			GetExt() override { return "pix3";	}
	ISO_ptr<void>		Read(tag id, istream_ref file) override;
} pix3;


class Chunk {
	istream_ref		file;
	streamptr	endofchunk;
public:
	uint32		id;

	Chunk(istream_ref _file) : file(_file) {
		if (file.read(id)) {
			endofchunk = file.get<int32le>();
			endofchunk += file.tell();
		} else {
			id = 0;
			endofchunk = 0;
		}
	}
	~Chunk()						{ if (endofchunk) file.seek((endofchunk + 1) & ~1);	}

	uint32		remaining()	const	{ return (uint32)max(int(endofchunk - file.tell()), 0); }
	operator	bool()		const	{ return id != 0; }
};


ISO_ptr<void> PIX3FileHandler::Read(tag id, istream_ref file) {
	ISO_ptr<anything>	p(id);

	while (Chunk chunk = Chunk(file)) {
		char	name[5];
		(uint32be&)name = chunk.id;
		name[4] = 0;

		if (0 && chunk.id == 'COMP') {
			ISO_TRACEF("COMP @ 0x%I64x\n", file.tell());
			ISO_ptr<ISO_openarray<ISO_openarray<xint8> > >	a(name);
			p->Append(a);
			while (chunk.remaining() > 16) {
				uint32	head	= file.get<uint32>();
				switch (head >> 24) {
					case 0: {
						struct packet0 {
							uint32	v0;
							uint32	comp_size;
							uint32	v1;
						} packet = file.get();
						ISO_TRACEF("packet=0x%08x (compressed to 0x%08x)\n", head, packet.comp_size);
						file.readbuff(a->Append().Create(packet.comp_size, false), packet.comp_size);
						break;
					}
					case 1:
					default:
						ISO_TRACEF("packet=0x%08x\n", head);
						break;
				}
//				if (len1 == 0 || len2 == 0 || len2 >= 0x10000000) {
//					ISO_TRACEF("%i remains\n", chunk.remaining());
//					break;
//				}
//				file.readbuff(a->Append().Create(len2, false), len2);
			}
		} else {
			ISO_ptr<ISO_openarray<xint8> >	a(name);
			file.readbuff(a->Create(chunk.remaining(), false), chunk.remaining());
			p->Append(a);
		}
	};

	return p;
}
