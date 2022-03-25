#include "plist.h"
#include "iso/iso_files.h"

using namespace iso;

class GarageBandFileHandler : public FileHandler {
	const char*		GetDescription() override { return "GarageBand"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		XPLISTreader		xml(file);
		return xml.get_item(id);
	}
} garageband;

