#include "iso/iso_files.h"
#include "vm.h"

//-----------------------------------------------------------------------------
//	Wireshark network capture
//-----------------------------------------------------------------------------

using namespace iso;

struct pcap_hdr {
	enum {
		MAGIC		= 0xa1b2c3d4,
		MAGIC_NS	= 0xa1b23c4d,
	};
	uint32	magic_number;	/* magic number */
	uint16	version_major;	/* major version number */
	uint16	version_minor;	/* minor version number */
	int32	thiszone;		/* GMT to local correction */
	uint32	sigfigs;		/* accuracy of timestamps */
	uint32	snaplen;		/* max length of captured packets, in octets */
	uint32	network;		/* data link type */

	int type() {
		return	magic_number == MAGIC					? 1
			:	magic_number == MAGIC_NS				? 2
			:	swap_endian(magic_number) == MAGIC		? -1
			:	swap_endian(magic_number) == MAGIC_NS	? -2
			:	0;
	}
};

struct pcaprec_hdr {
	uint32	ts_sec;			/* timestamp seconds */
	uint32	ts_usec;		/* timestamp microseconds */
	uint32	incl_len;		/* number of octets of packet saved in file */
	uint32	orig_len;		/* actual length of packet */
	pcaprec_hdr* next() { return (pcaprec_hdr*)((char*)(this + 1) + incl_len); }
};

struct ISO_pcap : mapped_file, ISO::VirtualDefaults {
	size_t	count;
	ISO_pcap(const char *fn)	: mapped_file(fn) {
		pcap_hdr	*hdr = *this;
		count = make_next_range<pcaprec_hdr>(slice(sizeof(pcap_hdr))).size();
	}
	uint32			Count() {
		return uint32((length() - sizeof(pcap_hdr)) /  sizeof(pcaprec_hdr));
	}
	ISO::Browser2	Index(int i) {
		return ISO::MakeBrowser(*nth(make_next_iterator<pcaprec_hdr>(slice(sizeof(pcap_hdr))), i));

	}

};

ISO_DEFUSERVIRT(ISO_pcap);
ISO_DEFUSERCOMPV(pcaprec_hdr, ts_sec, ts_usec, incl_len, orig_len);

class PCAPFileHandler : public FileHandler {
protected:
	const char*		GetExt() override { return "pcap"; }
	const char*		GetDescription() override { return "Wireshark network capture";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		pcap_hdr	header;
		return file.read(header) && header.type() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		return ISO::MakePtr<ISO_pcap>(id, fn.begin());
	}

} pcap;
