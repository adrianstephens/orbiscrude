#include "base/strings.h"
#include "iso/iso.h"
#include "comms/http.h"
#include "filename.h"
#include "thread.h"
#include "bitmap/gtf.h"

using namespace iso;
#if 0
pc:DBG 0x0020000f
30 10 00 00
00 00 04 49
48 54 02 10
00 00 02 00
00 20 00 0F
00 00 00 04
00 00 04 28
00 00 00 00
00 00 00 06
00 00 00 00
2F 00 00 00
00 00 00 00...
84

ps3:DRFP_CODE_STAT
30 10 00 00
00 00 00 19
54 48 02 00
00 00 01 10
00 00 00 0E
00 00 00 2D
00
pc:DRFP_CODE_STATR
30 10 00 00
00 00 00 48
48 54 02 00
00 00 01 10
00 00 00 0F
00 00 00 2D
00 00 00 00
00 00 41 C0
FF FF FF FF
FF FF FF FF
00 00 00 00
4D D6 FA E8
00 00 00 00
4D D6 FA E8
00 00 00 00
4C 6B 39 63
00 00 00 00
00 00 00 00

ps3:DRFP_CODE_OPENDIR
30 10 00 00
00 00 00 19
54 48 02 00
00 00 01 10
00 00 00 14
00 00 00 2E
00
pc:DRFP_CODE_OPENDIRR
30 10 00 00
00 00 00 20
48 54 02 00
00 00 01 10
00 00 00 15
00 00 00 2E
00 00 00 00
00 00 00 0C

pc:NETMP_CODE_VERSION
30 10 00 00
00 00 00 14
48 4D 00 00
00 00 00 10
10 00 00 00

pc:NETMP_CODE_VERSION
30 10 00 00
00 00 00 14
48 4D 00 00
00 00 00 10
10 00 00 00

pc:TSMP_TYPE_VERSION - key f543
30 10 00 00
00 00 00 18
48 4D 00 03
00 00 00 20
00 21 00 08
01 00 F5 43

ps3:NETMP_CODE_VERSIONR
30 10 00 00
00 00 00 18
4D 48 00 00
00 00 00 10
11 00 32 2E
31 2E 32 00

ps3:NETMP_CODE_VERSIONR
30 10 00 00
00 00 00 18
4D 48 00 00
00 00 00 10
11 00 32 2E
31 2E 32 00

ps3:TSMP_TYPE_VERSION (response?)
30 10 00 00
00 00 00 24
4D 48 00 03
00 00 00 20
00 21 00 14
01 01 F5 43
00 00 00 00
01 30 01 11
01 00 02 00

pc:DBG 0x0020000f
30 10 00 00
00 00 04 49
48 54 02 10
00 00 02 00
00 20 00 0F
00 00 00 06
00 00 04 28
00 00 00 00
00 00 00 06
00 00 00 00
2F 64 65 76		/dev_hdd0/game_debug
5F 68 64 64
30 2F 67 61
6D 65 5F 64
65 62 75 67
00 00 00 00....
B3

ps3:DBG 0x8020000f
30 10 00 00
00 00 03 35
54 48 02 10
00 00 02 00
80 20 00 0F
00 00 00 06
00 00 03 10
00 00 00 00
00 00 00 00
00 00 00 07
00 00 00 00
00 00 00 00
00 00 00 00
00 00 00 02
00 00 00 02
00 00 00 02
00 00 81 80
00 00 00 05
00 00 00 36
00 00 00 15
00 00 00 17
00 00 00 00
00 00 00 6C
00 00 00 03
00 00 00 16
00 00 00 00
00 00 00 33
00 00 00 35
00 00 00 0F
00 00 00 06
00 00 00 04
00 00 00 6D
00 00 00 03
00 00 00 7D
00 00 00 00
00 00 00 37
00 00 00 38
00 00 00 10
00 00 00 17
00 00 00 00
00 00 00 6C
00 00 00 03
00 00 00 16
00 00 00 00
00 00 00 00
00 D2 E5 88
55 46 4F 73		UFOs.ppu.self
2E 70 70 75
2E 73 65 6C
66 00 00 00
00 00 00 00...
68

#endif

typedef	fixed_string<16>	LogicalPartition;

struct DECI3header {
	enum protocols {
		DCMP	= 0x0001,
		NETMP	= 0x0010,
		NTTYP	= 0x0011,
		TSMP	= 0x0020,
		DFMP	= 0x0100,
		DRFP	= 0x0110,
		DBGP	= 0x0200,
		TTYP	= 0x0300
	};

	uint8		ver, size; uint16be reserved;
	uint32be	packet_size;
	uint8		srce, dest, lpar, port;
	uint32be	protocol;

	DECI3header()		{}
	DECI3header(uint32 protocol) : ver(0x30), size(0x10), reserved(0), srce('H'), lpar(0), port(0), protocol(protocol) {}

	char*			body()				{ return (char*)(this + 1);	}
	uint32			body_size()			{ return packet_size - uint32(sizeof(DECI3header));	}

	//need packet_size, srce, dest
};

struct DCMPheader : DECI3header {
	enum {
		CONNECT,
		ECHO,
		STATUS,
		ERROR2,
		PROTOCOL	= DECI3header::DCMP
	};
	uint8	type, code; uint16be param;
	DCMPheader(uint8 _code) : DECI3header(PROTOCOL), code(_code)	{}
};

struct NETMPheader : DECI3header {
	enum {
		PROTOCOL			= NETMP,

		CONNECT				= 0,
		CONNECTR,
		DISCONNECT,
		DISCONNECTR,
		REGISTER,
		REGISTERR,
		UNREGISTER,
		UNREGISTERR,
		POWER_STATUS,
		POWER_STATUSR,
		REGISTERED_LIST,
		REGISTERED_LISTR,
		LPAR_LIST,
		LPAR_LISTR,
		PROTO_LIST,
		PROTO_LISTR,
		VERSION,
		VERSIONR,

		ERR_OK				= 0,
		ERR_INVAL,
		ERR_BUSY,
		ERR_NOTCONNECT,
		ERR_ALREADYCONN,
		ERR_NOMEM,
		ERR_NOPROTO,
		ERR_INVALPRIORITY,
		ERR_INVALPROTO,
		ERR_PERM,
	};
	uint8	code, result; uint16be param;
	NETMPheader(uint8 _code) : DECI3header(PROTOCOL), code(_code), result(0)	{ dest = 'M'; }
};

struct NETMPregister : NETMPheader {
	uint32be			protocol;
	LogicalPartition	lpar;
	NETMPregister(const LogicalPartition &lpar, uint32 protocol) : NETMPheader(REGISTER), lpar(lpar), protocol(protocol) {
		param		= 0x80 << 8;
		packet_size = (uint32)sizeof(*this);
	}
};
struct NETMPlparlist : NETMPheader {
	NETMPlparlist() : NETMPheader(LPAR_LIST) {
		packet_size = (uint32)sizeof(*this);
	}
};

struct DRFPheader : DECI3header {
	enum {
 		PROTOCOL			= DRFP,

		INIT				= 0,
		INITR				= 1,
		OPEN				= 2,
		OPENR				= 3,
		CLOSE				= 4,
		CLOSER				= 5,
		READ				= 6,
		READR				= 7,
		WRITE				= 8,
		WRITER				= 9,
		SEEK				= 10,
		SEEKR				= 11,
		FSTAT				= 12,
		FSTATR				= 13,
		STAT				= 14,
		STATR				= 15,
		MKDIR				= 16,
		MKDIRR				= 17,
		RMDIR				= 18,
		RMDIRR				= 19,
		OPENDIR				= 20,
		OPENDIRR			= 21,
		CLOSEDIR			= 22,
		CLOSEDIRR			= 23,
		READDIR				= 24,
		READDIRR			= 25,
		FTRUNCATE			= 26,
		FTRUNCATER			= 27,
		TRUNCATE			= 28,
		TRUNCATER			= 29,
		RENAME				= 30,
		RENAMER				= 31,
		UNLINK				= 32,
		UNLINKR				= 33,

		ERR_OK				= 0,
		ERR_EINVAL			= 0x80010002,
		ERR_ENOMEM			= 0x80010004,
		ERR_ENOENT			= 0x80010006,
		ERR_EBUSY			= 0x8001000a,
		ERR_EISDIR			= 0x80010012,
		ERR_EEXIST			= 0x80010014,
		ERR_EFBIG			= 0x80010020,
		ERR_ENOSPC			= 0x80010023,
		ERR_EROFS			= 0x80010026,
		ERR_EACCES			= 0x80010029,
		ERR_EBADF			= 0x8001002a,
		ERR_EIO				= 0x8001002b,
		ERR_EMFILE			= 0x8001002c,
		ERR_ENODEV			= 0x8001002d,
		ERR_ENOTDIR			= 0x8001002e,
		ERR_EXDEV			= 0x80010030,
		ERR_ENAMETOOLONG	= 0x80010034,
		ERR_ENOTEMPTY		= 0x80010036,
		ERR_ENOTSUP			= 0x80010037,
		ERR_ESPECIFIC		= 0x80010038,
	};
	static int	next_seq;
	uint32be	code, seq;
	DRFPheader(uint32 _code) : DECI3header(PROTOCOL), code(_code), seq(++next_seq)	{ dest = 'T'; }
};

int	DRFPheader::next_seq;

struct DRFPheader2 : DRFPheader, filename {
	DRFPheader2(uint32 _code, const char *fn) : DRFPheader(_code), filename(fn)	{
		packet_size	= uint32(sizeof(DRFPheader) + filename::length() + 1);
	}
};

struct DFMPheader : DECI3header {
	enum {
		PROTOCOL	= DFMP,
		INIT		= 0,
		INITR,
		COPY,
		COPYR,
		FORMAT,
		FORMATR,
		EJECT,
		EJECTR,
		INSERT,
		INSERTR,
	};
	uint32be	code;
	DFMPheader(uint32 _code) : DECI3header(PROTOCOL), code(_code)	{ dest = 'T'; }
};

struct TSMPheader : DECI3header {
	enum {
 		PROTOCOL		= TSMP,

		ERR				= 0x0000,
		GET_TSM			= 0x0100,
		GET_TSMR		= 0x0101,
		GET_SDK			= 0x0102,
		GET_SDKR		= 0x0103,
		GET_CP			= 0x0104,
		GET_CPR			= 0x0105,
		ASSIGN_PORT		= 0x0200,
		ASSIGN_PORTR	= 0x0201,
		LOGIN			= 0x0202,
		LOGINR			= 0x0203,
		LOGOUT			= 0x0204,
		LOGOUTR			= 0x0205,
		GET_MODE		= 0x0206,
		GET_MODER		= 0x0207,
		KILL			= 0x0300,
		KILLR			= 0x0301,
		SYS_STATUS		= 0x2000,
		SYS_STATUSR		= 0x2001,
		SYS_POWER_ON	= 0x2002,
		SYS_POWER_ONR	= 0x2003,
		SYS_POWER_OFF	= 0x2004,
		SYS_POWER_OFFR	= 0x2005,
		SYS_RESET		= 0x2006,
		SYS_RESETR		= 0x2007,
		SYS_SHUTDOWN	= 0x2008,
		SYS_SHUTDOWNR	= 0x2009,
		SYS_REBOOT		= 0x200A,
		SYS_REBOOTR		= 0x200B,
		LPAR_STATUS		= 0x2100,
		LPAR_STATUSR	= 0x2101,
		LPAR_RESET		= 0x2106,
		LPAR_RESETR		= 0x2107,
		LPAR_REBOOT		= 0x210A,
		LPAR_REBOOTR	= 0x210B,
		GET_BOOT_PARAM	= 0x3000,
		GET_BOOT_PARAMR	= 0x3001,
		SET_BOOT_PARAM	= 0x3002,
		SET_BOOT_PARAMR	= 0x3003,
		GET_CURR_PARAM	= 0x3100,
		GET_CURR_PARAMR	= 0x3101,
		GET_SYS_PARAM	= 0x3200,
		GET_SYS_PARAMR	= 0x3201,
		SET_SYS_PARAM	= 0x3202,
		SET_SYS_PARAMR	= 0x3203,
		GET_MAC			= 0x4100,
		GET_MACR		= 0x4101,
		GET_IP			= 0x4102,
		GET_IPR			= 0x4103,
		GET_IP2			= 0x4104,
		GET_IP2R		= 0x4105,
		GET_TEST_PARAM	= 0x4106,
		GET_TEST_PARAMR	= 0x4107,
		SET_TEST_PARAM	= 0x4108,
		SET_TEST_PARAMR	= 0x4109,

		ERR_OK			= 0,
		ERR_FAIL,
		ERR_INVALVERSION,
		ERR_INVALPACKETSIZE,
		ERR_INVALTYPE,
		ERR_INVALCODE,
		ERR_INVALKEY,
		ERR_TARGETNOTSTARTED,
	};

	uint16be	tsmp_ver, tsmp_packetsize;
	uint16be	code;
	uint16be	key;
	TSMPheader(uint32 _code, uint32 key) : DECI3header(PROTOCOL), tsmp_ver(0x21), code(_code)	{ dest = 'M'; }
};


struct DECI3_holder {
	DECI3header	*h;
	DECI3_holder(const DECI3header &_h)	{ h = (DECI3header*)iso::malloc(_h.packet_size); *h = _h; }
	DECI3_holder(DECI3header *_h)		{ h = _h; }
	DECI3header*	operator->()		{ return h;		}
	operator DECI3header*()				{ return h;		}
	template<typename T> operator T*()	{ return (T*)h;	}
};

struct DECI3_temp : DECI3_holder {
	DECI3_temp(DECI3_holder &_h)	: DECI3_holder(_h)	{}
	DECI3_temp(DECI3header *_h)		: DECI3_holder(_h)	{}
	~DECI3_temp()						{ iso::free(h);		}
};

struct DECI3_struct {
	SOCKET				sock;
	LogicalPartition	*lpar;
	int					nlpar;

	bool	init(const char *target, uint16 port) {
		socket_init();
		sock = socket_address::TCP().connect(target, port);
		return	sock != INVALID_SOCKET;
	}

	bool connect(const char *info) {
		uint32		slen	= uint32(strlen(info));
		NETMPheader	header(NETMPheader::CONNECT);
		header.packet_size	= uint32(sizeof(header) - 2 + slen + 1);
		return	::send(sock, (const char*)&header, sizeof(header) - 2, 0) != SOCKET_ERROR
			&&	::send(sock, info, slen + 1, 0) != SOCKET_ERROR;
	}

	bool	send(const DECI3header &header) {
		return	::send(sock, (const char*)&header, header.packet_size, 0) != SOCKET_ERROR;
	}

	DECI3_temp	get_reply() {
		DECI3header	header;
		int	r = recv(sock, (char*)&header, sizeof(header), 0);
		if (r >= sizeof(header)) {
			DECI3_holder	ret(header);
			recv(sock, ret->body(), ret->body_size(), 0);
			return ret;
		}
		return 0;
	}

	void set_lpars(int n, LogicalPartition *_lpar) {
		if (nlpar = n) {
			lpar	= new LogicalPartition[n];
			memcpy(lpar, _lpar, sizeof(LogicalPartition) * n);
		}
	}
	LogicalPartition get_lpars(int i, const char *def = "PS3_LPAR") {
		if (i < nlpar)
			return lpar[i];
		return def;
	}

	int	Register(uint32 protocol) {
		send(NETMPregister(get_lpars(0), protocol));
		DECI3_temp	reply	= get_reply();
		if (reply->protocol == NETMPheader::PROTOCOL) {
			NETMPheader	*net = reply;
			if (net->code == NETMPheader::REGISTERR && net->result == NETMPheader::ERR_OK)
				return net->param >> 8;
		}
		return -1;
	}

	void	GetLogicalParitions() {
		send(NETMPlparlist());
		DECI3_temp	reply	= get_reply();
		if (reply->protocol == NETMPheader::PROTOCOL) {
			NETMPheader	*net = reply;
			if (net->code == NETMPheader::LPAR_LISTR)
				set_lpars(net->param >> 8, (LogicalPartition*)(net + 1));
		}
	}


	~DECI3_struct() { delete[] lpar;	}
};

bool DECI3() {
	DECI3_struct	d;

	if (!d.init("192.168.0.196", 8530))
		return false;

	if (!d.connect("adrian@adrians,isoeditor"))
		return false;

	if (DECI3_temp reply = d.get_reply()) {
		ISO_ASSERT(reply->protocol == NETMPheader::PROTOCOL && ((NETMPheader*)reply)->code == NETMPheader::CONNECTR && ((NETMPheader*)reply)->result == 0);
	}

	// get logical partitions
	d.GetLogicalParitions();

	// register DFMP
	int	lpar	= d.Register(DFMPheader::PROTOCOL);

	{
		DFMPheader	header(DFMPheader::INIT);
		header.packet_size	= (uint32)sizeof(header);
		header.lpar			= lpar;
		d.send(header);
		DECI3_temp	reply = d.get_reply();
	}

	const char *source	= "/app_homes/test.emu";
	const char *device	= "/dev_bdemu/1";
	uint32		slen	= uint32(strlen(source));
	uint32		dlen	= uint32(strlen(device));

	{
		DFMPheader	header(DFMPheader::INSERT);
		header.packet_size	= uint32(sizeof(header) + dlen + 1);
		header.lpar			= lpar;
		bool ok =	::send(d.sock, (const char*)&header, sizeof(header), 0) != SOCKET_ERROR
				&&	::send(d.sock, device, dlen + 1, 0) != SOCKET_ERROR;
		DECI3_temp	reply = d.get_reply();
	}

	{
		DFMPheader	header(DFMPheader::COPY);
		header.packet_size	= uint32(sizeof(header) + slen + 1 + dlen + 1);
		header.lpar			= lpar;
		bool ok =	::send(d.sock, (const char*)&header, sizeof(header), 0) != SOCKET_ERROR
				&&	::send(d.sock, source, slen + 1, 0) != SOCKET_ERROR
				&&	::send(d.sock, device, dlen + 1, 0) != SOCKET_ERROR;
		DECI3_temp	reply = d.get_reply();
	}

	return true;
}

class PS3Explorer : public ISO::VirtualDefaults {
	DECI3_struct	d;
public:
	bool			Init();
public:
	tag				GetName(int i)		{ return 0;		}
	ISO::Browser2	Index(int i)		{ return ISO::Browser2();	}
};

bool PS3Explorer::Init() {
	if (!d.init("192.168.2.203", 8530))
		return false;

	if (!d.connect("adrian@adrians,isoeditor"))
		return false;

	if (DECI3_temp reply = d.get_reply()) {
		ISO_ASSERT(reply->protocol == NETMPheader::PROTOCOL && ((NETMPheader*)reply)->code == NETMPheader::CONNECTR && ((NETMPheader*)reply)->result == 0);
	}

	d.GetLogicalParitions();

	int	lpar;
	lpar	= d.Register(TSMPheader::PROTOCOL);

	lpar	= d.Register(DRFPheader::PROTOCOL);

	{
		DRFPheader	header(DRFPheader::INIT);
		header.packet_size		= (uint32)sizeof(header);
		d.send(header);
		DECI3_temp	reply		= d.get_reply();
		DRFPheader::next_seq	= ((DRFPheader*)reply)->seq;
	}

	{
		d.send(DRFPheader2(DRFPheader::OPENDIR, "."));
		DECI3_temp	reply = d.get_reply();
	}

	return true;
}

ISO_DEFVIRT(PS3Explorer);

//-----------------------------------------------------------------------------
//	TMAPI
//-----------------------------------------------------------------------------
#include <ps3tmapi.h>
#include "communication/connection.h"

struct PS3TMMemory : public ISO::VirtualDefaults {
	static const uint64 BLOCK_SIZE = 0x10000;
	HTARGET				hTarget;
	uint32				process;
	xint64				start, end;

	PS3TMMemory(HTARGET h, uint32 p) : hTarget(h), process(p) {}

	uint32			Count() {
		return (end - start + BLOCK_SIZE - 1) / BLOCK_SIZE;
	}
	ISO::Browser2	Index(int i) {
		uint64		a		= start + i * BLOCK_SIZE;
		uint32		size	= (uint32)min(end - a, BLOCK_SIZE);
		ISO_ptr<ISO_openarray<xint8> >	p(0, size);
		SNPS3ProcessGetMemory(hTarget, PS3_UI_CPU, process, 0, a, size, (BYTE*)&**p);
		return p;
	}
};
ISO_DEFVIRT(PS3TMMemory);

struct PS3TMModule : ISO::VirtualDefaults {
	struct SEGMENT {
		PS3TMMemory	bin;
		xint64	index;
		xint64	elf_type;
		SEGMENT(HTARGET h, uint32 p, const SNPS3PRXSEGMENT &s) : bin(h, p) {
			bin.start	= s.uBase;
			bin.end		= s.uBase + s.uMemSize;
			index		= s.uIndex;
			elf_type	= s.uElfType;
		}
	};
	HTARGET				hTarget;
	uint32				process;
	uint32				module;

	fixed_string<30>	name;
	xint32				attribute;
	xint32				start_entry;
	xint32				stop_entry;
	fixed_string<512>	elf_name;
	dynamic_array<SEGMENT>	segments;

	char	aName[30];
	UINT32	uAttribute;
	UINT32	uStartEntry;
	char	aElfName[512];
	UINT32	uNumSegments;

	PS3TMModule(HTARGET h, uint32 p, uint32 m) : hTarget(h), process(p), module(m) {
		uint64	size;
		SNPS3GetModuleInfo(h, process, module, &size, 0);
		malloc_block	buffer(size);
		SNPS3MODULEINFO	*info = buffer;
		SNPS3GetModuleInfo(h, process, module, &size, info);
		name			= info->Hdr.aName;
		attribute		= info->Hdr.uAttribute;
		start_entry		= info->Hdr.uStartEntry;
		stop_entry		= info->Hdr.uStopEntry;
		elf_name		= info->Hdr.aElfName;

		for (int i = 0, n = info->Hdr.uNumSegments; i < n; i++)
			new(segments) SEGMENT(h, p, info->Segments[i]);
	}
	tag2	ID()	const { return name; }
};

ISO_DEFUSERCOMPX(PS3TMModule::SEGMENT, 4, "StartBin") {
	ISO_SETFIELDX1(0, bin.start, "start");
	ISO_SETFIELDS3(1, bin, index, elf_type);
} };

ISO_DEFUSERCOMPV(PS3TMModule,attribute, start_entry, stop_entry, elf_name, segments);

struct PS3TMProcess {
	HTARGET				hTarget;
	uint32				process;

	uint32				status;
	uint32				num_PPU_threads;
	uint32				num_SPU_threads;
	uint32				parent;
	xint64				max_memory;
	fixed_string<512>	path;

	dynamic_array<PS3TMModule>	modules;
	uint32				num_vmem;
	malloc_block		vmem;

	PS3TMProcess(HTARGET h, uint32 p) : hTarget(h), process(p) {
		uint32		size;

		SNPS3ProcessInfo(h, process, &size, 0);
		malloc_block		buffer(size);
		SNPS3PROCESSINFO	*info = buffer;
		SNPS3ProcessInfo(h, process, &size, info);
		status			= info->Hdr.uStatus;
		num_PPU_threads	= info->Hdr.uNumPPUThreads;
		num_SPU_threads	= info->Hdr.uNumSPUThreads;
		parent			= info->Hdr.uParentProcessID;
		max_memory		= info->Hdr.uMaxMemorySize;
		path			= info->Hdr.szPath;

		uint32		num;
		if (SN_SUCCEEDED(SNPS3GetModuleList(h, process, &num, 0))) {
			uint32	*mod_ids	= new uint32[num];
			SNPS3GetModuleList(h, process, &num, mod_ids);
			for (int i = 0; i < num; i++)
				new(modules) PS3TMModule(h, p, mod_ids[i]);
			delete[] mod_ids;
		}

		if (SN_SUCCEEDED(SNPS3GetVirtualMemoryInfo(h, process, FALSE, &num_vmem, &size, 0))) {
			vmem.create(size);
			SNPS3GetVirtualMemoryInfo(h, process, FALSE, &num_vmem, &size, vmem);
		}

	}
	range<SNPS3VirtualMemoryArea*>	get_vmem() const	{ return make_range_n((SNPS3VirtualMemoryArea*)vmem, num_vmem); }
};
ISO_DEFUSERCOMPV(SNPS3VirtualMemoryArea,uAddress,uFlags,uVSize,uOptions,uPageFaultPPU,uPageFaultSPU,uPageIn,uPageOut);
	ISO_SETFIELDS4(0, uPMemTotal,uPMemUsed,uTime,uPageCount);
}};
ISO_DEFUSERCOMPV(PS3TMProcess,status,parent,max_memory,path,num_PPU_threads, num_SPU_threads, modules);
	ISO_SETFIELDX(7, get_vmem, "vmem");
}};

class PS3GPUState : public ISO::VirtualDefaults {
	HTARGET	hTarget;
	uint32	process;
public:
	PS3GPUState(const PS3TMProcess &p) : hTarget(p.hTarget), process(p.process) {}
};
ISO_DEFUSERVIRT(PS3GPUState);

struct PS3TMConsole : ISO::VirtualDefaults {
	static const char *items[];

	HTARGET					hTarget;
	string					name, home_dir, fs_dir;

	SNPS3SystemInfo				sys;
	dynamic_array<PS3TMProcess>	processes;
	anything					xmb;
	ISO_ptr<PS3GPUState>		gpu;
	ISO_ptr<bitmap>				screen;

	PS3TMConsole(HTARGET h) : hTarget(h) {
		SNPS3TargetInfo	ti;
		ti.nFlags	= SN_TI_TARGETID | SN_TI_NAME | SN_TI_INFO | SN_TI_HOMEDIR | SN_TI_FILESERVEDIR | SN_TI_BOOT;
		ti.hTarget	= h;
		SNPS3GetTargetInfo(&ti);

		name		= ti.pszName;
		home_dir	= ti.pszHomeDir;
		fs_dir		= ti.pszFSDir;

		uint32	mask = SYS_INFO_SDK_VERSION | SYS_INFO_TIMEBASE_FREQ | SYS_INFO_RT_SDK_VERSION | SYS_INFO_TOTAL_SYSTEM_MEM | SYS_INFO_AVAILABLE_SYS_MEM | SYS_INFO_DCM_BUFFER_SIZE;
		SNPS3GetSystemInfo(h, 0, &mask, &sys);

		uint32		num_processes;
		if (SN_SUCCEEDED(SNPS3ProcessList(h, &num_processes, 0)) && num_processes) {
			uint32	*proc_ids	= new uint32[num_processes];
			SNPS3ProcessList(h, &num_processes, proc_ids);
			for (int i = 0; i < num_processes; i++)
				new(processes) PS3TMProcess(h, proc_ids[i]);
			gpu.Create("gpu", processes[0]);
			delete[] proc_ids;
		}

		UINT	size;
		if (SN_SUCCEEDED(SNPS3GetXMBSettings(h, 0, &size, TRUE))) {
			malloc_block	xmb0(size);
			SNPS3GetXMBSettings(h, xmb0, &size, TRUE);
			for (const char *s = xmb0; const char *comma = strchr(s, ','); s = comma + 1) {
				const char *eq = strchr(s, '=');
				const char *div = strchr(s, '/');
				if (div && div < eq) {
					ISO_ptr<anything>	p = xmb[tag2(crc32(str(s, div - s)))];
					if (!p)
						xmb.Append(p.Create(str(s, div)));
					p->Append(ISO_ptr<string>(str(div + 1, eq), str(eq + 1, comma)));
				} else {
					xmb.Append(ISO_ptr<string>(str(s, eq), str(eq + 1, comma)));
				}
			}
		}
	}

	tag2			GetName(int i)	{ return items[i];	}
	uint32			Count();
	ISO::Browser2	Index(int i);
	bool			Update(const char *spec, bool from);
};

const char *PS3TMConsole::items[] = {
	"SDK",
	"Home",
	"Fileserving",
	"XMB",
	"GPU",
	"Screen",
	"Processes",
};

uint32 PS3TMConsole::Count() {
	return num_elements32(items);
}

ISO::Browser2 PS3TMConsole::Index(int i)	{
	switch (i) {
		case 0: return ISO::MakeBrowser<baseint<16, UINT32> >((baseint<16, UINT32>&)sys.uCellSdkVersion);
		case 1: return ISO::MakeBrowser(home_dir);
		case 2: return ISO::MakeBrowser(fs_dir);
		case 3: return ISO::MakeBrowser(xmb);
		case 4: return gpu;
		case 5: {
			if (screen)
				return screen;

			SNPS3VRAMInfo		vram0, vram1;
			if (processes.empty()
			|| SN_FAILED(SNPS3Connect(hTarget, "IsoEditor"))
			|| SN_FAILED(SNPS3GetVRAMInformation(hTarget, processes[0].process, &vram0, &vram1))
			)
				return ISO::Browser2();

			uint32	size	= vram0.uHeight * vram0.uPitch;
			malloc_block	buffer(size);

			SNPS3ProcessGetMemory(hTarget, PS3_UI_CPU, processes[0].process, 0, vram0.uTopAddressPointer, size, buffer);
//			SNPS3GetMemoryCompressed(hTarget, processes[0], -1, vram0.uTopAddressPointer, size, buffer);
			switch (vram0.colour) {
				case X8R8G8B8: {	// 0 = X8R8G8B8
					screen.Create()->Create(vram0.uWidth, vram0.uHeight);
					copy(make_strided_block((Texel<R8G8B8A8>*)buffer, vram0.uWidth, vram0.uPitch, vram0.uHeight), screen->All());
					return screen;
				}
				case X8B8G8R8: {	// 1 = X8B8G8R8
					screen.Create()->Create(vram0.uWidth, vram0.uHeight);
					copy(make_strided_block((Texel<B8G8R8_8>*)buffer, vram0.uWidth, vram0.uPitch, vram0.uHeight), screen->All());
					return screen;
				}
				default: {	// 2 = R16G16B16X16
//					ISO_ptr<HDRbitmap>	bm("screen");
//					bm->Create(vram0.uWidth, vram0.uHeight);
//					copy(block<Texel<B8G8R8_8, 2> >((Texel<B8G8R8_8>*)buffer, vram0.uPitch / 8, vram0.uWidth, vram0.uHeight), bm->All());
//					return bm;
					return ISO::Browser2();
				}
			}
		}
		case 6:
			return ISO::MakeBrowser(processes);

		default: return ISO::Browser2();
	}
}

bool PS3TMConsole::Update(const char *spec, bool from) {
	uint32	i;
	if (!from && spec[0] == '[' && from_string(spec + 1, i)) {
		SNPS3TargetInfo	ti;
		switch (i) {
			case 1:	ti.nFlags	= SN_TI_TARGETID | SN_TI_HOMEDIR; break;
			case 2:	ti.nFlags	= SN_TI_TARGETID | SN_TI_FILESERVEDIR; break;
			default: return false;
		}
		ti.hTarget		= hTarget;
		return SN_SUCCEEDED(SNPS3SetTargetInfo(&ti));
	}
	return false;
}

ISO_DEFVIRT(PS3TMConsole);

class PS3TMExplorer : public ISO::VirtualDefaults {
	static HMODULE	handle;
	HTARGET	default_target;
	dynamic_array<PS3TMConsole>	targets;
	static int __stdcall target_enum(HTARGET target, void *param) {
		new (((PS3TMExplorer*)param)->targets) PS3TMConsole(target);
		return 0;
	}
public:
	bool			Init();
	int				Count()				{ return targets.size32();	}
	tag				GetName(int i)		{ return targets[i].name;	}
	ISO::Browser2	Index(int i)		{ return ISO::MakeBrowser(targets[i]);}
};

bool PS3TMExplorer::Init() {
	if (handle)
		return true;

	if (!(handle = load_library(sizeof(void*) == 4 ? "ps3tmapi.dll" : "ps3tmapix64.dll", "sn_ps3_path", "bin")))
		return false;

	return	SN_SUCCEEDED(SNPS3InitTargetComms())
		&&	SN_SUCCEEDED(SNPS3GetDefaultTarget(&default_target))
		&&	SN_SUCCEEDED(SNPS3EnumerateTargetsEx(target_enum, this));
}

HMODULE	PS3TMExplorer::handle;
ISO_DEFVIRT(PS3TMExplorer);

//-----------------------------------------------------------------------------
//	device
//-----------------------------------------------------------------------------

#include "device.h"

struct PS3Device : app::DeviceT<PS3Device>, app::DeviceCreateT<PS3Device> {
	void			operator()(const app::DeviceAdd &add)	{
		add("PS3 Explorer", this, app::LoadPNG("IDB_DEVICE_PLAYSTATION"));
	}
	ISO_ptr<void>	operator()(const win::Control &main) {
		ISO_ptr<PS3TMExplorer>	p("PS3 Explorer");
		if (p->Init())
			return p;
		return ISO_NULL;
	}
} ps3_device;
