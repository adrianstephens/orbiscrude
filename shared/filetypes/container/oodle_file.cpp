#include "iso/iso_files.h"
#include "codec/oodle.h"
#include "archive_help.h"

using namespace iso;

//-----------------------------------------------------------------------------
// FileHandler
//-----------------------------------------------------------------------------

class OodleFileHandler : public FileHandler {

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		auto	in	= malloc_block::unterminated(file);

		// detect if file uses 4-byte or 8-byte header
		int		hdrsize			= *(uint64*)in >= 0x10000000000 ? 4 : 8;
		uint64	unpacked_size	= (hdrsize == 8) ? *(uint64*)in : *(uint32*)in;
		
		malloc_block	out(unpacked_size + oodle::SAFE_SPACE);
		int		r	= oodle::decompress(in + hdrsize, in.size() - hdrsize, out, unpacked_size);
		out.resize(unpacked_size);
		return ISO::MakePtr(id, out);
	}
};

class BitknitFileHandler	: public OodleFileHandler { const char* GetExt() override { return "bitknit";	} } bitknit;
class KrakenFileHandler		: public OodleFileHandler { const char* GetExt() override { return "kraken";	} } kraken;
class LeviathanFileHandler	: public OodleFileHandler { const char* GetExt() override { return "leviathan";	} } leviathan;
class LznaFileHandler		: public OodleFileHandler { const char* GetExt() override { return "lzna";		} } lzna;
class MermaidFileHandler	: public OodleFileHandler { const char* GetExt() override { return "mermaid";	} } mermaid;
class SelkieFileHandler		: public OodleFileHandler { const char* GetExt() override { return "selkie";	} } selkie;
