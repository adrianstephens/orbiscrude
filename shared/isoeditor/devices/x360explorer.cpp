#include "comms/http.h"
#include <xdevkit.h>
#include "stream.h"
#include "filename.h"
#include "iso/iso_binary.h"
#include "communication/isolink.h"
#include "bitmap/bitmap.h"
#include "container/archive_help.h"
#include "com.h"
#include "extra/text_stream.h"

using namespace iso;
//					201- connected
//screenshot		202- multiline response follows
//					pitch=%i width=%i height=%i format=%i offsetx=%i offsety=%i, framebuffersize=%i sw=%i sh=%i colorspace=%i
//magicboot  COLD
//consoletype		200- testkit
//consolefeatures	200- OK
//			or		200-  DEBUGGING 1GB_RAM
//BYE				200- bye
//DBGNAME			200- x360testkit
//SYSTIME			200- high=%i low=%i
//DRIVELIST			202- multiline response follows
//					drivename="SysCache0"
//					drivename="E"
//					drivename="DEVKIT"
//					drivename="HDD"
//					.
//DRIVEFREESPACE NAME="<drive>:\"
//					202- multiline response follows
//					freetocallerlo=%i freetocallerhi=%i totalbyteslo=%i totalbyteshi=%i totalfreebyteslo=%i totalfreebyteshi=%i
//					.
//DIRLIST NAME="DEVKIT:\"
//					202- multiline response follows
//					name="dmext" sizehi=0x0 sizelo=0x0 createhi=%i createlo=%i changehi=%i changelo=%i directory
//					name="music1.wma" sizehi=%i sizelo=%i createhi=%i createlo=%i changehi=%i changelo=%i
//GETFILE NAME="DEVKIT:\music1.wma"
//					203- binary response follows

struct X360tiler {
	uint32	tile_pitch, logbpp;
	X360tiler(uint32 pitch, uint32 block) {
		tile_pitch	= pitch / (4 * 32);
		logbpp		= log2(block);
	};
	uint32	operator()(uint32 x, uint32 y) {
		uint32	macro		= ((x >> 5) + (y >> 5) * tile_pitch) << (logbpp + 7);
		uint32	micro		= ((x & 7) + ((y & 6) << 2)) << logbpp;
		uint32	offset		= macro + ((micro & ~15) << 1) + (micro & 15) + ((y & 8) << (3 + logbpp)) + ((y & 1) << 4);
		return	((offset & ~0x1ff) << 3)
			+	((offset & 0x1c0) << 2)
			+	(offset & 0x3f)
			+	((y & 16) << 7)
			+	(((((y & 8) >> 2) + (x >> 3)) & 3) << 6);
	}
};


class X360ExplorerFile : public ISO::VirtualDefaults {
public:
	string			host, spec;
	ISO_ptr<void>	ptr;

	X360ExplorerFile(const pair<const char*,const char*> &p) : host(p.a), spec(p.b) {}
	ISO::Browser2	Deref() {
		if (!ptr) {
			char			discard[256];
			SOCKET			sock = socket_address::TCP().connect(host, 730);
			socket_input	si(sock);
			socket_output	so(sock);
			si.read(discard);

			so.write((const char*)format_string("GETFILE NAME=\"%s\"\r\n", spec.begin()));
			buffered_reader<socket_input, 1024>	bi(si);

			fixed_string<256>	line;
			make_text_reader(bi).read_line(line);
			if (line[0] == '2') {
				uint32	size	= bi.get<uint32le>();
				ISO_ptr<ISO_openarray<uint8> >	p(0);
				bi.readbuff(p->Create(size, false), size);
				ptr = p;
			}

			closesocket(sock);
		}
		return ptr;
	}
};

class X360ExplorerDir : public ISO::VirtualDefaults {
public:
	string			host, spec;
	dynamic_array<ISO_ptr<void> >	ptrs;

	X360ExplorerDir(const pair<const char*,const char*> &p) : host(p.a), spec(p.b) {}
	uint32			Count();
	tag				GetName(int i)		{ return ptrs[i].ID();	}
	ISO::Browser2	Index(int i)		{ return ptrs[i];		}
};

uint32 X360ExplorerDir::Count() {
	if (ptrs.empty()) {
		char			discard[256];
		SOCKET			sock = socket_address::TCP().connect(host, 730);
		socket_input	si(sock);
		socket_output	so(sock);

		si.read(discard);
		so.write((const char*)format_string("DIRLIST NAME=\"%s\\\"\r\n", spec.begin()));
		si.read(discard);
		if (discard[0] == '2') {
			buffered_reader<socket_input, 1024>	bi(si);
			auto	text = make_text_reader(bi);

			for (;;) {
				fixed_string<256>	line;
				text.read_line(line);
				if (line[0] == '.')
					break;
				if (line.begins("name=")) {
					bool	dir		= line.slice(-9) == "directory";
					char	*end	= str(line.begin() + 6).find('"');
					*end = 0;
					tag		id		= line.begin() + 6;
					if (dir) {
						ptrs.push_back(ISO_ptr<X360ExplorerDir>(id, make_pair(host, filename(spec).add_dir(id))));
					} else {
						ptrs.push_back(ISO_ptr<X360ExplorerFile>(id, make_pair(host, filename(spec).add_dir(id))));
					}
				}
			}
		}
		closesocket(sock);
	}
	return uint32(ptrs.size());
}


class X360ExplorerConsole : public ISO::VirtualDefaults {
	string							name, host;
	ISO_ptr<bitmap>					bm;
	dynamic_array<X360ExplorerDir>	drives;
public:
	X360ExplorerConsole(const char *_name, const char *_host);
	const char*		Name()				{ return name;						}
	uint32			Count()				{ return uint32(drives.size()) + 1;	}
	tag				GetName(int i)		{ return i == 0 ? "screen" : drives[i - 1].spec; }
	ISO::Browser2	Index(int i);
};

class X360Explorer : public ISO::VirtualDefaults {
	dynamic_array<X360ExplorerConsole>	consoles;
public:
	bool			Init();
public:
	uint32			Count()				{ return uint32(consoles.size());	}
	tag				GetName(int i)		{ return consoles[i].Name();		}
	ISO::Browser2	Index(int i)		{ return MakePtr(consoles[i].Name(), &consoles[i]);	}
};

X360ExplorerConsole::X360ExplorerConsole(const char *_name, const char *_host) : name(_name), host(_host) {
	char			message[256];
	SOCKET			sock = socket_address::TCP().connect(host, 730);
	socket_input	si(sock);
	socket_output	so(sock);

	si.readbuff(message, 256);
	so.write("DRIVELIST\r\n");
	buffered_reader<socket_input, 1024>	bi(si);
	auto	text = make_text_reader(bi);
	while (text.read_line(message) && message[0] != '.') {
		if (strncmp(message, "drivename=", 10) == 0) {
			char	*p = message + strlen(message) - 1;
			p[0] = ':';
			p[1] = 0;
			new(drives) X360ExplorerDir(make_pair(host, message + 11));
		}
	}
	closesocket(sock);
}

ISO::Browser2 X360ExplorerConsole::Index(int i) {
	if (i == 0) {
		if (!bm) {
			char			message[256];
			SOCKET			sock = socket_address::TCP().connect(host, 730);
			socket_input	si(sock);
			socket_output	so(sock);

			si.readbuff(message, 256);
			so.write("screenshot\r\n");
			buffered_reader<socket_input, 1024>	bi(si);
			auto	text = make_text_reader(bi);
			text.read_line(message);
			text.read_line(message);

			int	pitch, width, height, format, offsetx, offsety, framebuffersize, sw, sh, colorspace;
			int	n = sscanf(message,
				"pitch=%i width=%i height=%i format=%i offsetx=%i offsety=%i, framebuffersize=%i sw=%i sh=%i colorspace=%i",
				&pitch, &width, &height, &format, &offsetx, &offsety, &framebuffersize, &sw, &sh, &colorspace
			);

			malloc_block	buffer(framebuffersize);
			bi.readbuff(buffer, framebuffersize);
			closesocket(sock);

			bm.Create()->Create(width, height);

			X360tiler	tiler(pitch, 4);
			for (int y = 0; y < height; y++) {
				ISO_rgba	*dest	= bm->ScanLine(y);
				for (int x = 0; x < width; x++, dest++) {
					uint8	*src = buffer + tiler(x, y);
					dest->r = src[2];
					dest->g = src[1];
					dest->b = src[0];
					dest->a = src[3];
				}
			}
		}
		return bm;
	}
	return MakePtr(drives[i - 1].spec, &drives[i - 1]);
}

bool X360Explorer::Init() {
	com_ptr<IXboxManager>	xbm;
	com_ptr<IXboxConsoles>	xconsoles;

	init_com();
	LONG			num_consoles;

	if (!xbm.create<XboxManager>()
	||	FAILED(xbm->get_Consoles(&xconsoles))
	||	FAILED(xconsoles->get_Count(&num_consoles))
	)
		return false;

	for (LONG i = 0; i < num_consoles; i++) {
		com_ptr<IXboxConsole>	console;
		com_string				wname;
		uint8					ip[4];
		if (	SUCCEEDED(xconsoles->get_Item(i, &wname))
			&&	SUCCEEDED(xbm->OpenConsole(wname, &console))
			&&	SUCCEEDED(console->get_IPAddress((DWORD*)ip))
		) {
			new(consoles) X360ExplorerConsole(string(wname), format_string("%i.%i.%i.%i", ip[3], ip[2], ip[1], ip[0]));
		}
	}
	return true;
}

ISO_DEFVIRT(X360ExplorerFile);
ISO_DEFVIRT(X360ExplorerDir);
ISO_DEFVIRT(X360ExplorerConsole);
ISO_DEFVIRT(X360Explorer);

#include "device.h"

extern ISO_ptr<void> GetX360Explorer(tag id);
struct X360Device : app::DeviceT<X360Device>, app::DeviceCreateT<X360Device> {
	void			operator()(const app::DeviceAdd &add) {
		add("Xbox360 Explorer", this, app::LoadPNG("IDB_DEVICE_XBOX"));
	}
	ISO_ptr<void>	operator()(const win::Control &main) {
		win::Busy();
		ISO_ptr<X360Explorer> p("Xbox360 Explorer");
		if (p->Init())
			return p;
		return ISO_NULL;
	}
} x360_device;
