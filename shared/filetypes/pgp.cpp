#include "iso/iso_files.h"
#include "bits.h"

using namespace iso;

class PGPFileHandler : public FileHandler {
	const char*		GetExt() override { return "pgp";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
//	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} pgp;


enum PGP_PACKET {
	PGPP_Reserved			= 0,	// Reserved - a packet tag MUST NOT have this value
	PGPP_PublicKeyEncKey	= 1,	// Public-Key Encrypted Session Key Packet
	PGPP_Signature			= 2,	// Signature Packet
	PGPP_SymmetricKeyEncKey	= 3,	// Symmetric-Key Encrypted Session Key Packet
	PGPP_OnePass			= 4,	// One-Pass Signature Packet
	PGPP_SecretKey			= 5,	// Secret-Key Packet
	PGPP_PublicKey			= 6,	// Public-Key Packet
	PGPP_SecretSubkey		= 7,	// Secret-Subkey Packet
	PGPP_CompressedData		= 8,	// Compressed Data Packet
	PGPP_SymmetricData		= 9,	// Symmetrically Encrypted Data Packet
	PGPP_Marker				= 10,	// Marker Packet
	PGPP_Literal			= 11,	// Literal Data Packet
	PGPP_Trust				= 12,	// Trust Packet
	PGPP_UserID				= 13,	// User ID Packet
	PGPP_PublicSubkey		= 14,	// Public-Subkey Packet
	PGPP_UserAttr			= 17,	// User Attribute Packet
	PGPP_SymmInteg			= 18,	// Sym. Encrypted and Integrity Protected Data Packet
	PGPP_ModDetection		= 19,	// Modification Detection Code Packet
//       60 to 63 -- Private or Experimental Values
};

#pragma pack(1)
struct PublicKeyEncKey {
	uint8	version;
	uint8	keyid[8];
	uint8	algorithm;
};

struct SymmetricKeyEncKey {
	uint8	version;
	uint8	algorithm;
};

struct PublicKey3 {
	uint8	version;
	uint32	created;
	uint16	valid;
	uint8	algorithm;
};

struct PublicKey4 {
	uint8	version;
	uint32	created;
	uint8	algorithm;
};

#pragma pack()

ISO_ptr<void> PGPFileHandler::Read(tag id, istream_ref file)
{
	for (;;) {
//		streamptr	start	= file.tell();
		int			h		= file.getc();
		if (h == EOF)
			break;
		bool		partial	= false;
		PGP_PACKET	pgpp;
		uint32		len;
		if (h & 0x40) {
			//new header
			pgpp	= PGP_PACKET(h & 0x3f);
			len		= file.getc();
			if (len >= 0xc0) {
				if (len < 0xe0) {
					len = ((len - 0xc0) << 8) + file.getc() + 0xc0;
				} else if (len == 0xff) {
					len = file.get<uint32be>();
				} else {
					len = 1 << (len & 0x1f);
					partial = true;
				}
			}
		} else {
			//old header
			pgpp	= PGP_PACKET((h >> 2) & 0x0f);
			switch (h & 3) {
				case 0: len = file.getc();			break;
				case 1: len = file.get<uint16>();	break;
				case 2: len = file.get<uint32>();	break;
				case 3: len = 0; partial = true;	break;
			}
		}
		streamptr	start	= file.tell();
		switch (pgpp) {
			case PGPP_PublicKeyEncKey: {
				PublicKeyEncKey	header = file.get();
				break;
			}

			case PGPP_PublicKey:
			case PGPP_PublicSubkey: {
				PublicKey3	header = file.get();
				break;
			}

			case PGPP_CompressedData: {
				uint8	algorithm = file.getc();
				break;
			}

			case PGPP_Literal: {
			}
		}
		file.seek(start + len);

	}
	return ISO_NULL;
}
