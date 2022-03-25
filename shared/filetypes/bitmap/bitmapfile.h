#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

namespace iso {
class BitmapFileHandler : public FileHandler {
	const char*			GetCategory() override { return "bitmap"; }
	ISO_ptr<void>		ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<bitmap> bm;
		if (filename(fn.name()).ext() == ".mip") {
			if (id == fn.name())
				id = filename(fn.name()).name();
			if (bm = Read(id, FileInput(fn).me()))
				bm->SetMips(MaxMips(bm->Width() / 2, bm->Height()));
		} else {
			bm = FileHandler::ReadWithFilename(id, fn);
		}
		if (bm.IsType<bitmap>() || bm.IsType<HDRbitmap>())
			SetBitmapFlags(bm.get());
		return bm;
	}
};
}