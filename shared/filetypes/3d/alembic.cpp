#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"
#include "model_utils.h"
#include "extra/indexer.h"

using namespace iso;

class AlembicFileHandler : public FileHandler {
	const char*		GetExt() override { return "abs"; }
	const char*		GetDescription() override { return "Alembic";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return ISO_NULL;
	}
} alembic;
