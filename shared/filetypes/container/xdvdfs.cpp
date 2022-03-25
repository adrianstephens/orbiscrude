#include "iso/iso_files.h"
#include "archive_help.h"

using namespace iso;

#define MAX_DEPTH	0xFF				//maximum directory recursion
#define FILE_RO		0x01
#define FILE_HID	0x02
#define FILE_SYS	0x04
#define FILE_DIR	0x10
#define	FILE_ARC	0x20
#define FILE_NOR	0x80

struct DIRENT {
	uint16	ltable, rtable;
	uint32	sector;
	uint32	size;
	uint8	attribs;
	uint8	fnamelen;
	char	name[256-14];
};

class XDVDFSFileHandler : public FileHandler {
	void					ReadFile(ISO_ptr<anything> &p, istream_ref file, uint32 sector, uint32 offset);
	const char*		GetExt() override { return "xsf"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} xdvdfs;

void XDVDFSFileHandler::ReadFile(ISO_ptr<anything> &p, istream_ref file, uint32 sector, uint32 offset) {
	DIRENT	dirent;

	file.seek(sector * 2048 + offset);
	file.read(dirent);
	dirent.name[dirent.fnamelen] = 0;

	if (dirent.attribs & FILE_DIR) {
		ISO_ptr<anything>	p2(dirent.name);
		ReadFile(p2, file, dirent.sector, 0);
		p->Append(p2);
	} else {
		if (dirent.size < 0x10000000) {
			file.seek(dirent.sector * 2048);
			p->Append(ReadData2(dirent.name, file, dirent.size, true));
		}
	}
	if (dirent.rtable)
		ReadFile(p, file, sector, dirent.rtable * 4);
	if (dirent.ltable)
		ReadFile(p, file, sector, dirent.ltable * 4);
}

ISO_ptr<void> XDVDFSFileHandler::Read(tag id, istream_ref file) {
	static const char header[] = "MICROSOFT*XBOX*MEDIA";
	uint8	buffer[2048];

	if (file.get<uint32be>() != 'XSF\x1a')
		return ISO_NULL;

	file.seek(0x10000);
	file.readbuff(buffer, 20);
	if (str(header, 20) != header)
		return ISO_NULL;

	uint32	dtable		= file.get<uint32le>();
	uint32	dtablesize	= file.get<uint32le>();

	ISO_ptr<anything>	p(id);
	ReadFile(p, file, dtable, 0);
	return p;
}
