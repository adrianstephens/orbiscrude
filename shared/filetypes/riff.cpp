#include "iso/iso_files.h"
#include "riff.h"

using namespace iso;

template<bool be> ISO_ptr<void>	ReadForm(_RIFF_chunk<be> &riff, int alignment = 2) {
	ISO_ptr<anything> p(as_chars(riff.template get<uint32>()));

	while (riff.remaining()) {
		_RIFF_chunk<be>	chunk(riff.istream());

		if (chunk.id == "LIST"_u32) {
			p->Append(ReadForm(chunk));

		} else {
			ISO_ptr<ISO_openarray<uint8> >	r(as_chars(chunk.id), chunk.remaining());
			riff.readbuff(*r, chunk.remaining());
			p->Append(r);
		}
	}
	return p;
}

class RIFFFileHandler : public FileHandler {
	const char*		GetDescription() override { return "Generic RIFF container"; }

	int	Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32>() == "RIFF"_u32 ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		RIFF_chunk	riff(file);
		if (riff.id != "RIFF"_u32)
			return ISO_NULL;
		return ReadForm(riff);
	}
} riff;

class RIFXFileHandler : public FileHandler {
	const char*		GetDescription() override { return "Generic RIFX container"; }

	int	Check(istream_ref file) override {
		file.seek(0);
		return file.get<uint32>() == "RIFX"_u32 ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		RIFX_chunk	riff(file);
		if (riff.id != "RIFX"_u32)
			return ISO_NULL;
		return ReadForm(riff);
	}
} rifx;
