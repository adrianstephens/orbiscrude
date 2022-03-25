#include "sample.h"
#include "iso/iso_files.h"

namespace iso {
class SampleFileHandler : public FileHandler {
	const char*			GetCategory() override { return "sample"; }
};
}