#include "samplefile.h"
#include "iso/iso_convert.h"
#include "filetypes/iff.h"

using namespace iso;

class AIFFFileHandler : public SampleFileHandler {
	const char*			GetExt() override { return "aif";	}
	const char*			GetDescription() override { return "Audio Interchange File Format"; }

	ISO_ptr<void>		Read(tag id, istream_ref file) override;
	bool				Write(ISO_ptr<void> p, ostream_ref file) override;
} aiff;

ISO_ptr<void> AIFFFileHandler::Read(tag id, istream_ref file) {
	IFF_chunk	iff(file);
	if (iff.id != "FORM"_u32 || file.get<uint32>() != "AIFF"_u32)
		return ISO_NULL;

	ISO_ptr<sample> s(id);

	while (iff.remaining()) {
		IFF_chunk	chunk(file);
		switch (chunk.id) {
			case "COMM"_u32: {
				aiff_structs::COMMchunk	comm = file.get();
				s->Create(comm.frames, comm.channels, comm.bits);
				s->SetFrequency(float(comm.samplerate));
				break;
			}
			case "INST"_u32: {
				aiff_structs::INSTchunk	inst = file.get();
				if (inst.sustainLoop.playMode != 0) {
					s->flags |= sample::LOOP;
					//sm.Mark(0).Set(0, 0, sm.frames);
				}
				break;
			}
			case "SSND"_u32: {
				file.get<aiff_structs::SSNDchunk>();
				file.readbuff(s->Samples(), chunk.remaining());
#ifndef ISO_BIGENDIAN
				if (s->Bits() == 16) {
					uint16		*p = (uint16*)s->Samples();
					for (int n = s->Length() * s->Channels(); --n; p++)
						*p	= *(uint16be*)p;
				}
#endif
				break;
			}
		}
	}
	return s;
}

bool AIFFFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (ISO_ptr<sample> s = ISO_conversion::convert<sample>(p)) {
		IFF_Wchunk	iff(file, "FORM"_u32);
		iff.write("AIFF"_u32);

		aiff_structs::COMMchunk	comm;
		comm.channels		= s->Channels();
		comm.frames			= s->Length();
		comm.bits			= s->Bits();
		comm.samplerate		= s->Frequency();
		IFF_Wchunk(file, "COMM"_u32).write(comm);

		{
			IFF_Wchunk	chunk(file, "SSND"_u32);
			aiff_structs::SSNDchunk	ssnd;
			ssnd.blocksize	= 0;
			ssnd.offset		= 0;
			chunk.write(ssnd);
			chunk.writebuff(s->Samples(), s->Length() * s->BytesPerSample());
		}

		return true;
	}
	return false;
}
