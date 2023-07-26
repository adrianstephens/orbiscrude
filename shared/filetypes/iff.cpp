#include "iso/iso_files.h"
#include "iff.h"

using namespace iso;

//FORM, CAT, LIST and PROP
//FOR4, CAT4, LIS4 and PRO4
//FOR8, CAT8, LIS8 and PRO8

class IFFFileHandler : public FileHandler {
	const char*		GetDescription() override {
		return "Generic IFF container";
	}

	int	Check(istream_ref file) override {
		file.seek(0);
		return (file.get<uint32be>() >> 8) == 'FOR' ? CHECK_POSSIBLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		IFF_chunk	iff(file);
		if ((iff.id >> 8) != 'FOR')
			return ISO_NULL;

		return ReadForm(iff);
	}
} iff;

ISO_ptr<void> iso::ReadForm(IFF_chunk &iff, int alignment) {
	ISO_ptr<anything> p(as_chars(iff.get<uint32>()));

	while (iff.remaining()) {
		IFF_chunk	chunk(iff.istream(), alignment);
		int			n;

		if ((n = chunk.is_ext('FORM')) || (n = chunk.is_ext('LIST')) || (n = chunk.is_ext('PROP'))) {
			p->Append(ReadForm(chunk, n));

		} else {
			ISO_ptr<ISO_openarray<uint8> >	r(chunk.id, chunk.remaining());
			iff.readbuff(*r, chunk.remaining());
			p->Append(r);
		}
	}
	return p;
}
