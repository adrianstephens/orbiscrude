#include "directories.h"
#include "extra/date.h"
#include "base/algorithm.h"
#include "extra/NTFS.h"
#include "main.h"
#include "device.h"
#include "directory.h"
#include "windows/dialog.h"
#include "windows/filedialogs.h"
#include "vm.h"

#include <winioctl.h>

using namespace iso;

typedef callback<void(const char *, time)>	busy_update;

class BusyDialog : public win::Dialog<BusyDialog> {
	time	start, last;

public:
	LRESULT	Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_INITDIALOG:
				break;
			case WM_COMMAND:
				break;
			case WM_CLOSE:
				Destroy();
				break;
			case WM_NCDESTROY:
				delete this;
				break;
		}
		return FALSE;
	}

	void Create(Control parent, const char *title, const char *message) {
		last = start = time::now();

		win::Font::Params		font = win::Font::Caption();
		win::DialogBoxCreator	dc(win::DialogUnits(font).ScreenToDialog(parent.GetClientRect()).Centre(256,64), title, POPUP|CAPTION|DS_SETFONT|DS_MODALFRAME|DS_FIXEDSYS);
		dc.SetFont(font);
		dc.AddControl(win::Rect(0, 0, 256, 64), win::DLG_STATIC, message, 0, win::StaticControl::CENTER | win::StaticControl::PATHELLIPSIS);
		Modeless(parent, dc.GetTemplate());
		Show();
	}
	bool Ready(time rate) {
		if (time::now() < last + rate)
			return false;
		last += rate;
		return true;
	}

	void Set(const char *message) {
		Item(0).SetText(message);
	}
	void operator()(const char *message, time rate = 1.f) {
		if (Ready(rate))
			Set(message);
	}
	BusyDialog() {}
	BusyDialog(Control parent, const char *title, const char *message) {
		Create(parent, title, message);
	}
};

class BusyDialogASync {
	BusyDialog	*p;
public:
	BusyDialogASync(win::Control parent, const char *title, const char *message) : p(0) {
		p = new BusyDialog;
		JobQueue::Main().add([p = p, parent, title = string(title), message = string(message)]() {
			p->Create(parent, title, message);
		});
	}
	~BusyDialogASync() {
		JobQueue::Main().add([p = p]() {
			p->Destroy();
			});
	}
	void operator()(const char *message, time rate = 1.f) {
		if (p && p->Ready(rate)) {
			JobQueue::Main().add([this, message = string(message)]() {
				p->Set(message);
			});
		}
	}
};

struct ProgressASync {
	win::Progress	*p;
public:
	ProgressASync(const win::WindowPos &wpos, const char *caption, uint64 total) : p(0) {
		JobQueue::Main().add([this, wpos, caption, total]() {
			p = new win::Progress(wpos, caption, total);
			});
	}
	~ProgressASync() {
		if (p) {
			p->PostMessage(WM_CLOSE);
			delete p;
		}
	}
	void Reset(const char *caption, uint64 total) {
		if (p)
			p->Reset(caption, total);
	}
	void	operator()(uint64 pos) {
		if (p && p->Changes(pos))
			JobQueue::Main().add([this, pos]() {
			p->Set(pos);
				});
	}
};

count_string no_trailing_slash(const char *v) {
	auto	s = str(v);
	if (s.ends("\\"))
		return s.slice(0, -1);
	return count_string(v, string_len(v));
}

HANDLE	OpenVolume(const char *vol) {
	return CreateFileA(
		buffer_accum<256>("\\\\.\\") << no_trailing_slash(vol)
		, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE
		, NULL
		, OPEN_EXISTING
		, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
		, NULL
	);
}

//-----------------------------------------------------------------------------
//	NTFS Journal
//-----------------------------------------------------------------------------

enum USN_REASON;
ISO_DEFUSERENUM(USN_REASON, 22) {
#define ENUM(x) #x, USN_REASON_##x
	Init(0,
		ENUM(DATA_OVERWRITE),
		ENUM(DATA_EXTEND),
		ENUM(DATA_TRUNCATION),
		ENUM(NAMED_DATA_OVERWRITE),
		ENUM(NAMED_DATA_EXTEND),
		ENUM(NAMED_DATA_TRUNCATION),
		ENUM(FILE_CREATE),
		ENUM(FILE_DELETE),
		ENUM(EA_CHANGE),
		ENUM(SECURITY_CHANGE),
		ENUM(RENAME_OLD_NAME),
		ENUM(RENAME_NEW_NAME),
		ENUM(INDEXABLE_CHANGE),
		ENUM(BASIC_INFO_CHANGE),
		ENUM(HARD_LINK_CHANGE),
		ENUM(COMPRESSION_CHANGE),
		ENUM(ENCRYPTION_CHANGE),
		ENUM(OBJECT_ID_CHANGE),
		ENUM(REPARSE_POINT_CHANGE),
		ENUM(STREAM_CHANGE),
		ENUM(TRANSACTED_CHANGE),
		ENUM(CLOSE)
	);
#undef ENUM
}};

enum FILE_ATTRIBUTE;
ISO_DEFUSERENUM(FILE_ATTRIBUTE, 15) {
#define ENUM(x) #x, FILE_ATTRIBUTE_##x
	Init(0,
		ENUM(READONLY),
		ENUM(HIDDEN),
		ENUM(SYSTEM),
		ENUM(DIRECTORY),
		ENUM(ARCHIVE),
		ENUM(DEVICE),
		ENUM(NORMAL),
		ENUM(TEMPORARY),
		ENUM(SPARSE_FILE),
		ENUM(REPARSE_POINT),
		ENUM(COMPRESSED),
		ENUM(OFFLINE),
		ENUM(NOT_CONTENT_INDEXED),
		ENUM(ENCRYPTED),
		ENUM(VIRTUAL)
	);
#undef ENUM
}};

template<> struct ISO::def<FILETIME> : ISO::VirtualT2<FILETIME> {
	static ISO_ptr<void>	Deref(const FILETIME &ft) {
		return ISO_ptr<string>(0, to_string(DateTime(ft)));
	}
};

struct FILE_NAME {
	uint16		length;
	uint16		offset;
	count_string16	get() const {
		char	*base	= (char*)this - iso_offset(USN_RECORD, FileNameLength);
		return str((const char16*)(base + offset), length / 2);
	}
};
template<> struct ISO::def<FILE_NAME> : ISO::VirtualT2<FILE_NAME> {
	static ISO_ptr<void>	Deref(const FILE_NAME &fn) {
//		char	*base	= (char*)&fn - iso_offset(USN_RECORD, FileNameLength);
//		return ISO_ptr<string16>(0, str((char16*)(base + fn.offset), fn.length / 2));
		return ISO_ptr<string16>(0, fn.get());
	}
};

struct ISO_USN_RECORD {
	DWORD			RecordLength;
	WORD			MajorVersion;
	WORD			MinorVersion;
	xint64			FileReferenceNumber;
	xint64			ParentFileReferenceNumber;
	xint64			Usn;
	FILETIME		TimeStamp;
	USN_REASON		Reason;
	DWORD			SourceInfo;
	DWORD			SecurityId;
	FILE_ATTRIBUTE	FileAttributes;
	FILE_NAME		FileName;
};

ISO_DEFUSERCOMPV(ISO_USN_RECORD,
	Usn, FileName, FileReferenceNumber, ParentFileReferenceNumber, TimeStamp, Reason, FileAttributes
);

struct USNbuffer {
	typedef ISO_USN_RECORD	USN_RECORD;
	char		buffer[64 * 1024];
	DWORD		bytes;
	uint32		num;

	struct iterator {
		USN_RECORD	*p;
		iterator(USN_RECORD *_p) : p(_p)			{}
		iterator&			operator++()			{ p = (USN_RECORD*)((char*)p + p->RecordLength); return *this; }
		const USN_RECORD&	operator*()		const	{ return *p;	}
		const USN_RECORD*	operator->()	const	{ return p;		}
		operator const USN_RECORD*()		const	{ return p;	}
		bool		operator==(iterator b)	const	{ return p == b.p; }
		bool		operator!=(iterator b)	const	{ return p != b.p; }
	};

	iterator	begin()		const	{ return (USN_RECORD*)(buffer + sizeof(USN)); }
	iterator	end()		const	{ return (USN_RECORD*)(buffer + bytes); }
	uint32		size()		const	{ return num; }

	USN			FirstUSN()	const	{ return begin()->Usn; }
	USN			NextUSN()	const	{ return *(USN*)buffer; }
	bool		Has(USN i)	const	{ return i >= FirstUSN() && i < NextUSN(); }

	template<typename T> USNbuffer(HANDLE h, DWORD code, const T &t) {
		num	= 0;
		if (DeviceIoControl(h, code, (void*)&t, sizeof(T), buffer, sizeof(buffer), &bytes, NULL)) {
			for (iterator p = begin(), e = end(); p != e; ++p)
				++num;
		}
	}
	const USN_RECORD*	Get(USN i) const {
		if (Has(i)) {
			for (iterator p = begin(), e = end(); p != e; ++p) {
				if (p->Usn == i)
					return p;
			}
		}
		return 0;
	}
	const USN_RECORD&	operator[](int i) const {
		iterator	p	= begin();
		while (i--)
			++p;
		return *p;
	}
};

template<> struct ISO::def<USNbuffer> : ISO::VirtualT2<USNbuffer> {
	static uint32		Count(const USNbuffer &b)			{ return b.size(); }
	static ISO::Browser	Index(const USNbuffer &b, int i)	{ return ISO::MakeBrowser(b[i]); }
};

class NTFSJournalBase : public ISO::VirtualDefaults {
public:
	dynamic_array<USNbuffer*>	buffers;

	~NTFSJournalBase()					{ for_each(buffers, [](USNbuffer *b) { delete b; }); }
	uint32			Count()	const		{ return uint32(buffers.size()); }
	ISO::Browser2	Index(int i) const	{ return ISO::MakeBrowser(*buffers[i]);	}
};

class NTFSJournal : public NTFSJournalBase {
public:
	NTFSJournal(HANDLE h) {
		DWORD					bytes;
		USN_JOURNAL_DATA		journal_data;
		DeviceIoControl(h, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &journal_data, sizeof(journal_data), &bytes, NULL);
		READ_USN_JOURNAL_DATA	data = {journal_data.FirstUsn, 0xFFFFFFFF, FALSE, 0, 0, journal_data.UsnJournalID};
		for (;;) {
			USNbuffer *buff	= new USNbuffer(h, FSCTL_READ_USN_JOURNAL, data);
			if (buff->size() == 0) {
				delete buff;
				break;
			}
			buffers.push_back(buff);
			data.StartUsn = buff->NextUSN();
		}
	}
};
ISO_DEFVIRT(NTFSJournal);

//-----------------------------------------------------------------------------
//	NTFSFiles, NTFSFiles2 - get all files from USN records
//-----------------------------------------------------------------------------

class NTFSFiles : public NTFSJournalBase {
public:
	NTFSFiles(HANDLE h) {
		MFT_ENUM_DATA	data	= {0, 0, maximum};
		for (;;) {
			USNbuffer *buff	= new USNbuffer(h, FSCTL_ENUM_USN_DATA, data);
			if (buff->size() == 0) {
				delete buff;
				break;
			}
			buffers.push_back(buff);
			data.StartFileReferenceNumber = buff->NextUSN();
		}
	}
};
ISO_DEFVIRT(NTFSFiles);
#if 0
class NTFSFiles2 : public ISO::VirtualDefaults {
public:
	hash_map<uint64, dirs::Dir*>	dirs;

	NTFSFiles2(HANDLE h) {
		MFT_ENUM_DATA	data	= {0, 0, maximum};
		for (;;) {
			USNbuffer buff(h, FSCTL_ENUM_USN_DATA, data);
			if (buff.size() == 0)
				break;

			for (auto i = buff.begin(), e = buff.end(); i != e; ++i) {
				auto	parent = dirs[i->ParentFileReferenceNumber];
				if (!parent.exists())
					parent = new dirs::Dir;

				auto	name = i->FileName.get();
				if (i->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					auto		me	= dirs[i->FileReferenceNumber];
					dirs::Dir	*d	= me;;
					if (d)
						d->name = name;
					else
						me = d = new dirs::Dir(name, 0);
					parent->subdirs.push_back(d);
				} else {
					parent->entries.push_back(new dirs::Entry(name));
				}
			}

			data.StartFileReferenceNumber = buff.NextUSN();
		}
	}
	ISO::Browser2	Deref()		{ return ISO::MakeBrowser(dirs[NTFS::FILE_ID::Root].put());	}
};
ISO_DEFVIRT(NTFSFiles2);
#endif
//-----------------------------------------------------------------------------
//	NTFSJournalDevice
//-----------------------------------------------------------------------------

struct NTFSJournalDevice : app::DeviceT<NTFSJournalDevice> {
	struct Volume : app::DeviceCreateT<Volume> {
		string name;
		ISO_ptr<void>	operator()(const win::Control &main) {
			Win32Handle	h = CreateFileA(buffer_accum<256>("\\\\.\\") << name.slice(0, -1), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
			if (!h)
				return ISO_NULL;
			return ISO_ptr<NTFSJournal>(str(buffer_accum<256>("NTFS Journal for ") << name), h);
		}
		Volume(const char *_name) : name(_name) {}
	};

	void operator()(const app::DeviceAdd &add) {
		ConcurrentJobs::Get().add([=]() {
			app::DeviceAdd	sub = add("NTFS Journal", app::LoadPNG("IDB_DEVICE_HARDDRIVE"));

			char	name[MAX_PATH + 1];
			HANDLE	h = FindFirstVolumeA(name, sizeof(name));
			if (h != INVALID_HANDLE_VALUE) {
				do {
					DWORD			size = 0;
					GetVolumePathNamesForVolumeNameA(name, 0, 0, &size);
					if (GetLastError() == ERROR_MORE_DATA) {
						malloc_block	paths(size);
						if (GetVolumePathNamesForVolumeNameA(name, paths, size, &size)) {
							for (char *p = paths; *p; p += strlen(p) + 1) {
								char	system[256];
								if (GetVolumeInformationA(p, 0, 0, 0, 0, 0, system, sizeof(system)) && str(system) == "NTFS")
									sub(p, new Volume(p));
							}
						}
					}

				} while (FindNextVolumeA(h, name, sizeof(name)));
				FindVolumeClose(h);
			}
			if (sub.menu.Count() == 0)
				sub.menu.Append("<no NTFS volumes found>", 0, MF_DISABLED);
		});
	}
} journal_device;

//-----------------------------------------------------------------------------
//	NTFS Files
//-----------------------------------------------------------------------------

#include "winternl.h"
#include "windows/nt.h"

namespace NT {

//#define X(F, T) T_type<T>::type &F = *(T_type<T>::type*)GetProcAddress(GetModuleHandleA("NTDLL.dll"), #F)
#define X(F, T)	dll_function<T>	F(GetModuleHandleA("NTDLL.dll"), #F)

	X(NtOpenFile, NTSTATUS NTAPI( PHANDLE FileHandle, ACCESS_MASK DesiredAccess,  OBJECT_ATTRIBUTES *ObjectAttributes,  IO_STATUS_BLOCK *IoStatusBlock,  ULONG ShareAccess,  ULONG OpenOptions));
	X(NtQueryVolumeInformationFile, NTSTATUS NTAPI(HANDLE FileHandle, IO_STATUS_BLOCK *IoStatusBlock, PVOID FsInformation, ULONG Length, ULONG FsInformationClass));
	X(NtQueryInformationFile, NTSTATUS NTAPI( HANDLE FileHandle, IO_STATUS_BLOCK *IoStatusBlock,  PVOID FileInformation,  ULONG Length,  ULONG FileInformationClass));
	X(NtSetInformationFile, NTSTATUS NTAPI( HANDLE FileHandle, IO_STATUS_BLOCK *IoStatusBlock,  PVOID FileInformation,  ULONG Length,  ULONG FileInformationClass));
	X(RtlInitUnicodeString, VOID NTAPI(UNICODE_STRING * DestinationString, PCWSTR SourceString));
	X(RtlNtStatusToDosError, ULONG NTAPI( NTSTATUS NtStatus));
	X(RtlSystemTimeToLocalTime, NTSTATUS NTAPI( LARGE_INTEGER const *SystemTime, PLARGE_INTEGER LocalTime));
#undef X

	struct FILE_FS_SIZE_INFORMATION {
		int64	TotalAllocationUnits, ActualAvailableAllocationUnits;
		uint32	SectorsPerAllocationUnit, BytesPerSector;
	};

	struct FILE_FS_ATTRIBUTE_INFORMATION {
		uint32	FileSystemAttributes;
		uint32	MaximumComponentNameLength;
		uint32	FileSystemNameLength;
		wchar_t FileSystemName[1];
	};

	struct ObjectAttributes : OBJECT_ATTRIBUTES {
		ObjectAttributes(HANDLE h, UNICODE_STRING &s) {
			Length						= sizeof(*this);
			RootDirectory				= h;
			ObjectName					= (::UNICODE_STRING*)&s;
			Attributes					= OBJ_CASE_INSENSITIVE;
			SecurityDescriptor			= 0;
			SecurityQualityOfService	= 0;
		}
	};
}

struct ntfs_mapping {
	uint64 num;
	uint64 lcn;
};

HANDLE OpenRelative(HANDLE parent, NT::UNICODE_STRING path) {
	HANDLE		h;
	NT::ObjectAttributes	oa(parent, path);
	IO_STATUS_BLOCK			iosb;
	return NT::NtOpenFile(&h, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_SYNCHRONOUS_IO_NONALERT) == 0 ? h : 0;
}

HANDLE OpenRelative(HANDLE parent, uint64 id) {
	HANDLE		h;
	NT::UNICODE_STRING		path(&id);
	NT::ObjectAttributes	oa(parent, path);
	IO_STATUS_BLOCK			iosb;
	return NT::NtOpenFile(&h, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN_BY_FILE_ID | FILE_SYNCHRONOUS_IO_NONALERT) == 0 ? h : 0;
}

dynamic_array<ntfs_mapping> GetRetrievalPointers(HANDLE handle) {
	STARTING_VCN_INPUT_BUFFER	input;
	malloc_block				buffer;

	clear(input);
	for (DWORD nr = 1024; ; nr = nr * 2) {
		buffer = malloc_block(nr);
		if (DeviceIoControl(handle, FSCTL_GET_RETRIEVAL_POINTERS, &input, sizeof(input), buffer, nr, &nr, NULL))
			break;
		if (GetLastError() != ERROR_MORE_DATA)
			return none;
	}

	RETRIEVAL_POINTERS_BUFFER	*output = buffer;
	uint64	vcn	= output->StartingVcn.QuadPart;
	auto	*s	= output->Extents;
	dynamic_array<ntfs_mapping> result(output->ExtentCount + int(vcn != 0));
	auto	*d	= result.begin();

	if (vcn != 0) {
		d->num	= vcn;
		d->lcn	= 0;
		++d;
	}
	for (int n = output->ExtentCount; n--; ++d, ++s) {
		uint64	prev	= vcn;
		vcn		= s->NextVcn.QuadPart;
		d->num	= vcn - prev;
		d->lcn	= s->Lcn.QuadPart;
	}

	return result;
}

dynamic_array<ntfs_mapping> UnpackMappingPairs(uint8 *data, uint64 vcn) {
	uint64	lcn	= 0;
	dynamic_array<ntfs_mapping>	res;

	if (vcn) {
		auto	&i	= res.push_back();
		i.num		= vcn;
		i.lcn		= 0;
	}

	while (uint8 byte = *data++) {
		uint8	v	= byte & 15, l = byte >> 4;
		int64	n	= read_bytes<int64>(data, v);
		data		+= v;
		lcn			+= read_bytes<int64>(data, l);
		data		+= l;

		auto	&i	= res.push_back();
		i.num		= n;
		i.lcn		= lcn;
	}
	return res;
}

uint64 UsedClusters(uint8 *data) {
	uint64	used	= 0;
	uint64	lcn		= 0;
	while (uint8 byte = *data++) {
		uint8	v	= byte & 15, l = byte >> 4;
		int64	n	= read_bytes<int64>(data, v);
		data		+= v;
		lcn			+= read_bytes<int64>(data, l);
		data		+= l;

		if (lcn)
			used += n;
	}
	return used;
}

uint64 UsedClusters(const dynamic_array<ntfs_mapping> &map) {
	uint64	used = 0;
	for (auto i = map.begin(), e = map.end(); i != e; ++i) {
		if (i->lcn)
			used += i->num;
	}
	return used;
}

const NTFS::ATTR_RECORD *GetAttribute(const NTFS::MFT_RECORD *rec, NTFS::ATTR_TYPE type, int instance) {
	for (auto i = rec->begin(), e = rec->end(); i != e && i->type != NTFS::AT_UNUSED && i->type != NTFS::AT_END; ++i) {
		if (i->type == type && (instance == 0 || instance == i->instance))
			return &*i;
	}
	return 0;
}

struct FastCopier {
	static const uint32	block	= 1 << 24;

	struct Buffer {
		void		*mem;
		OVERLAPPED	tov;

		Buffer() {
			clear(tov);
			tov.hEvent	= CreateEvent(NULL, FALSE, TRUE, NULL);
			mem			= vmem::reserve_commit(block);
		};
		~Buffer() {
			WaitForSingleObject(tov.hEvent, INFINITE);
			vmem::decommit_release(mem, block);
		}
		void	Transfer(HANDLE fh, uint64 foffset, HANDLE th, uint64 toffset, uint64 size, uint32 fsect) {
			WaitForSingleObject(tov.hEvent, INFINITE);

			LARGE_INTEGER	seek;
			seek.QuadPart = foffset;
			SetFilePointerEx(fh, seek, &seek, SEEK_SET);

			DWORD	r, w;
			if (!ReadFile(fh, mem, size, &r, NULL)) {
				ISO_TRACEF("Retrying at ") << hex(foffset) << "\n";
				for (uint32 off = 0; off < size; off += fsect) {
					if (!ReadFile(fh, (uint8*)mem + off, fsect, &r, NULL)) {
						ISO_TRACEF("Failed at ") << hex(foffset + off) << "\n";
						seek.QuadPart = foffset + off + fsect;
						SetFilePointerEx(fh, seek, &seek, SEEK_SET);
						memset((uint8*)mem + off, 0, fsect);
					}
				}
			} else {
				ISO_ASSERT(size == r);
			}
			tov.Offset		= toffset;
			tov.OffsetHigh	= toffset >> 32;
			WriteFile(th, mem, size, &w, &tov);
		}
	};

	Buffer	buffer[2];
	int		i;

	FastCopier() : i(0) {}

	void	Copy(HANDLE fh, uint64 foffset, HANDLE th, uint64 toffset, uint64 total, uint32 fsect) {
		auto	start	= time::now();
		auto	next	= start + time(1.f);

		while (foffset < total) {
			if (time::now() >= next) {
				ISO_TRACEF("At ") << float(next - start) << "s " << foffset * 100.0 / total << "%\n";
				next += 1.f;
			}
			uint64	chunk	= min(total - foffset, block);
			buffer[i].Transfer(fh, foffset, th, toffset, chunk, fsect);
			foffset	+= chunk;
			toffset += chunk;
			i = 1 - i;
		}
	}
};

struct FastReader {
	static const uint32	block	= 1 << 24;

	struct Buffer {
		OVERLAPPED	tov;
		Buffer() {
			clear(tov);
			tov.hEvent	= CreateEvent(NULL, FALSE, TRUE, NULL);
		};
		~Buffer() {
			Wait();
		}
		void	Wait() {
			if (!HasOverlappedIoCompleted(&tov))
				WaitForSingleObject(tov.hEvent, INFINITE);
		}
		void	Read(HANDLE fh, uint64 foffset, uint8 *dest, uint64 size) {
			Wait();

			tov.Offset		= foffset;
			tov.OffsetHigh	= foffset >> 32;

			DWORD	r;
			ReadFile(fh, dest, size, &r, &tov);
		}
	};

	Buffer	buffer[2];
	int		i;

	FastReader() : i(0) {}

	void	Wait() {
		buffer[i].Wait();
	}

	void	Read(HANDLE fh, uint64 foffset, uint8 *dest, uint64 total) {
		buffer[i].Read(fh, foffset, dest, total);
		i = 1 - i;
	}
};

struct FastReaderUnaligned {
	static const uint32	block	= 1 << 24;

	struct Buffer {
		OVERLAPPED	tov;
		void		*mem;
		void		*prev_dest;
		uint32		prev_size;
		Buffer() : prev_dest(0) {
			clear(tov);
			tov.hEvent	= CreateEvent(NULL, FALSE, TRUE, NULL);
			mem			= vmem::reserve_commit(block);
		};
		~Buffer() {
			Wait();
			vmem::decommit_release(mem, block);
		}
		void	Wait() {
			if (prev_dest) {
				WaitForSingleObject(tov.hEvent, INFINITE);
				memcpy(prev_dest, mem, prev_size);
			}
		}
		void	Read(HANDLE h, uint64 offset, void *dest, uint32 size, uint32 cluster_size) {
			Wait();
			prev_dest		= dest;
			prev_size		= size;

			tov.Offset		= offset;
			tov.OffsetHigh	= offset >> 32;

			DWORD	r;
			ReadFile(h, mem, align(size, cluster_size), &r, &tov);
		}
	};

	Buffer	buffer[2];
	int		i;

	FastReaderUnaligned() : i(0) {}

	void	Wait() {
		buffer[i].Wait();
	}

	void	Read1(HANDLE h, uint64 offset, uint8 *dest, uint64 total, uint32 cluster_size) {
		buffer[i].Read(h, offset, dest, total, cluster_size);
		i = 1 - i;
	}

	void	Read(HANDLE h, uint64 offset, uint8 *dest, uint64 size, uint32 cluster_size) {
		while (size > block) {
			Read1(h, offset, dest, block, cluster_size);
			dest	+= block;
			offset	+= block;
			size	-= block;
		}
		Read1(h, offset, dest, (uint32)size, cluster_size);
	}
	void	Read(HANDLE h, const dynamic_array<ntfs_mapping> &ptrs, uint8 *dest, uint64 total, uint32 cluster_size) {
		uint8	*end = dest + total;
		for (auto &m : ptrs) {
			uint64		offset	= m.lcn * cluster_size;
			uint64		size	= min(m.num * cluster_size, end - dest);
			Read(h, offset, dest, size, cluster_size);
			dest	+= size;
		}
	}
};

struct FilesDevice : app::DeviceT<FilesDevice> {
	typedef dirs::Dir	Dir;
	typedef dirs::Entry	Entry;

	static Dir	*GetDir(Dir *d, const char *fn, busy_update update) {
		for (directory_iterator i(filename(fn).add_dir("*.*")); i; ++i) {
			auto	fn2 = filename(fn).add_dir(i);
			if (i.is_dir()) {
				if (i[0] != '.')
					d->subdirs.push_back(GetDir(new Dir(filename(fn2).name_ext_ptr()), fn2, update));

			} else {
				update(fn2, 0.1f);
				d->entries.push_back(new Entry((const char*)i, i.size()));
			}
		}
		return d;
	}

	static ISO_ptr<void> GetFAT(const win::Control &main, const char *name) {
		BusyDialogASync	b(main, "Scanning for files", "...");
		auto	p = ISO_ptr<Dir>(name);
		GetDir(p, name, &b);
		p->CalcSize();
		return p;
	}

	struct FATVolume : app::DeviceCreateT<FATVolume> {
		string name;
		ISO_ptr<void>	operator()(const win::Control &main) {
			return GetFAT(main, name);
		}
		FATVolume(const char *_name) : name(_name) {}
	};

	struct NTFSVolume : app::DeviceCreateT<NTFSVolume> {
		string name;
		ISO_ptr<void>	operator()(const win::Control &main) {
			Win32Handle	h = OpenVolume(name);
			if (!h)
				return GetFAT(main, name);

			hash_map<uint64, Dir*>	dirs;

			DWORD	br;
			NTFS_VOLUME_DATA_BUFFER volume_data;
			DeviceIoControl(h, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &volume_data, sizeof(volume_data), &br, NULL);
			uint32	cluster_size	= volume_data.BytesPerCluster;
			uint64	mft_start_lcn	= volume_data.MftStartLcn.QuadPart;
			uint32	mft_record_size = volume_data.BytesPerFileRecordSegment;
			uint64	size			= volume_data.MftValidDataLength.QuadPart;

			Win32Handle	hMFT		= OpenRelative(h, NTFS::FILE_ID::MFT);
			dynamic_array<ntfs_mapping> ptrs = GetRetrievalPointers(hMFT);

			ProgressASync	progress(win::WindowPos(main, AdjustRect(main.GetRect().Centre(500, 30), win::Control::OVERLAPPED | win::Control::CAPTION, false)), "Reading MFT", size);

			malloc_block	mft(size);
			uint8			*p	= mft;
			{
				FastReader	reader;
				for (ntfs_mapping *i = ptrs.begin(), *e = ptrs.end(); i != e; ++i) {
					uint64		lbn		= i->lcn * cluster_size;
					DWORD		read	= i->num * cluster_size;
					reader.Read(h, lbn, p, read);
					p += read;
					progress(p - mft);
				}
			}

			{
				progress.Reset("Parsing MFT", size);
				uint64	refno	= 0;
				for (NTFS::MFT_RECORD *rec = mft; rec < mft.end(); rec = (NTFS::MFT_RECORD*)((uint8*)rec + mft_record_size), refno++) {
					if (rec->magic == NTFS::MFT_RECORD::FILE && rec->flags.test(NTFS::MFT_RECORD::IN_USE) && rec->PatchUSA(mft_record_size)) {
						NTFS::ATTRIBUTE<NTFS::AT_STANDARD_INFORMATION>	*si = 0;
						NTFS::ATTRIBUTE<NTFS::AT_FILENAME>				*fn	= 0;
						uint64						filesize = 0;
						for (auto i = rec->begin(), e = rec->end(); i != e && i->type != NTFS::AT_UNUSED && i->type != NTFS::AT_END; ++i) {
							switch (i->type) {
								case NTFS::AT_STANDARD_INFORMATION:
									si = *i;
									break;

								case NTFS::AT_FILENAME: {
									NTFS::ATTRIBUTE<NTFS::AT_FILENAME>	*fn2 = *i;
									if (fn2->filename_type != fn2->DOS)
										fn = fn2;
									break;
								}

								case NTFS::AT_INDEX_ROOT:
								case NTFS::AT_DATA:
									filesize = i->is_non_resident
										? UsedClusters(i->mapping_pairs()) * cluster_size
										: i->resident.value_length;
									break;

								case NTFS::AT_ATTRIBUTE_LIST:
									if (const NTFS::ATTRIBUTE<NTFS::AT_ATTRIBUTE_LIST> *entry = *i) {
										for (const void *end = i->next(); entry < end; entry = entry->next()) {
											NTFS::MFT_RECORD *rec2 = (NTFS::MFT_RECORD*)(mft + (entry->mft_reference & bits64(48)) * mft_record_size);
											if (const NTFS::ATTR_RECORD *a = GetAttribute(rec2, entry->type, entry->instance)) {
												switch (a->type) {
													case NTFS::AT_STANDARD_INFORMATION:
														si = *a;
														break;

													case NTFS::AT_FILENAME:
														fn = *a;
														break;
												}
											}
										}
									}
									break;
							}
						}

						if (fn && (fn->filename_length > 1 || fn->filename[0] != '.')) {
							auto	parent = dirs[fn->parent_directory];
							if (!parent.exists())
								parent = new dirs::Dir;

							auto	name = str(fn->filename, fn->filename_length);
							if (rec->flags.test(NTFS::MFT_RECORD::IS_DIRECTORY)) {
								auto		me	= dirs[refno | (uint64(rec->sequence_number) << 48)];
								dirs::Dir	*d	= me;;
								if (d)
									d->name = name;
								else
									me = d = new dirs::Dir(name);
								parent->subdirs.push_back(d);
							} else {
								parent->entries.push_back(new Entry(name, filesize));
							}
						}

					}
					progress((uint8*)rec - mft);
				}
			}

			auto root	= dirs[NTFS::FILE_ID::Root].put();
			root->entries.push_back(new Entry(0, volume_data.FreeClusters.QuadPart * volume_data.BytesPerCluster));
			root->CalcSize();

			return ISO_ptr<Dir>(str(buffer_accum<256>("NTFS Files on ") << name), *root);
		}
		NTFSVolume(const char *_name) : name(_name) {}
	};

	struct Browse : app::DeviceCreateT<Browse> {
		filename	fn;
		ISO_ptr<void>	operator()(const win::Control &main) {
			if (GetDirectory(main, fn, "Select Directory"))
				return GetFAT(*app::MainWindow::Get(), fn);
			return ISO_NULL;
		}
	} browser;

	void add_path(const app::DeviceAdd &add, const char *title, const char *path) {
		char	system[256];
		if (GetVolumeInformationA(path, 0, 0, 0, 0, 0, system, sizeof(system)))
			add(title, str(system) == "NTFS" ? (app::DeviceCreate*)new NTFSVolume(path) : (app::DeviceCreate*)new FATVolume(path));
	}

	void operator()(const app::DeviceAdd &add)	{
		ConcurrentJobs::Get().add([=]() {
			app::DeviceAdd	sub = add("Files", app::LoadPNG("IDB_DEVICE_HARDDRIVE"));

			wchar_t	name16[MAX_PATH + 1];
			HANDLE	h = FindFirstVolumeW(name16, sizeof(name16));
			uint32	last_error = 0;
			if (h != INVALID_HANDLE_VALUE) {
				do {
					DWORD			size = 0;
					GetVolumePathNamesForVolumeNameW(name16, 0, 0, &size);
					if (size) {
						malloc_block	paths(size * 2);
						if (GetVolumePathNamesForVolumeNameW(name16, paths, size, &size)) {
							for (wchar_t *p = paths; *p; p += string_len(p) + 1)
								add_path(sub, str8(p), str8(p));
						}
					}
					last_error = GetLastError();

				} while (FindNextVolumeW(h, name16, sizeof(name16)));
				FindVolumeClose(h);
			}
			if (sub.menu.Count() == 0) {
				sub.menu.Append("<no NTFS volumes found>", 0, MF_DISABLED);

				for (DWORD log = GetLogicalDrives(); log; log = clear_lowest(log)) {
					char	c = lowest_set_index(log) + 'A';
					//if (Win32Handle(CreateFileA(format_string("\\\\.\\%c:", i), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS, 0)).Valid())
					add_path(sub, format_string("Logical Drive %c", c), format_string("\\\\.\\%c:\\", c));
				}
			}
			sub("Browse...", &browser);
		});
	}
} files_device;

uint64 dirs::Dir::CalcSize() {
	if (size)
		return size;

	uint64	total = 0;
	for (auto i : subdirs) {
		ISO_ASSERT(i != this);
		total += i->CalcSize();
	}

	for (auto i = entries.begin(), e = entries.end(); i != e; ++i)
		total += (*i)->size;

	return (size = total);
}

//-----------------------------------------------------------------------------
//	NTFSVolume
//-----------------------------------------------------------------------------

struct NTFSVolume {
	HANDLE			h;
	memory_block	mft;
	uint32			record_size;
	uint32			cluster_size;

	struct element {
		NTFS::MFT_RECORD										*rec;
		const NTFS::ATTRIBUTE<NTFS::AT_STANDARD_INFORMATION>	*si;
		const NTFS::ATTRIBUTE<NTFS::AT_FILENAME>				*fn;
		const NTFS::ATTR_RECORD									*data;

		element(NTFS::MFT_RECORD *_rec) : rec(_rec), si(0), fn(0), data(0) {}
	};

	struct iterator {
		const NTFSVolume	*volume;
		NTFS::MFT_RECORD	*rec;

		void	read_attribute(element &e, const NTFS::ATTR_RECORD *attr) const {
			switch (attr->type) {
			case NTFS::AT_STANDARD_INFORMATION:
				e.si = *attr;
				break;

			case NTFS::AT_FILENAME: {
				NTFS::ATTRIBUTE<NTFS::AT_FILENAME>	*fn2 = *attr;
				if (fn2->filename_type != fn2->DOS)
					e.fn = fn2;
				break;
			}

			case NTFS::AT_INDEX_ROOT:
			case NTFS::AT_DATA:
				e.data = attr;
				break;

			case NTFS::AT_ATTRIBUTE_LIST:
				if (const NTFS::ATTRIBUTE<NTFS::AT_ATTRIBUTE_LIST> *entry = *attr) {
					for (const void *end = attr->next(); entry < end; entry = entry->next()) {
						NTFS::MFT_RECORD *rec2 = (NTFS::MFT_RECORD*)((uint8*)volume->mft + (entry->mft_reference & bits64(48)) * volume->record_size);
						if (const NTFS::ATTR_RECORD *a = GetAttribute(rec2, entry->type, entry->instance))
							read_attribute(e, a);
					}
				} else {
					FastReaderUnaligned	reader;
					malloc_block		temp(attr->non_resident.data_size);
					reader.Read(volume->h, UnpackMappingPairs(attr->mapping_pairs(), 0), temp, attr->non_resident.data_size, volume->cluster_size);
					reader.Wait();

					for (const NTFS::ATTRIBUTE<NTFS::AT_ATTRIBUTE_LIST> *entry = temp; entry < temp.end(); entry = entry->next()) {
						NTFS::MFT_RECORD *rec2 = (NTFS::MFT_RECORD*)((uint8*)volume->mft + (entry->mft_reference & bits64(48)) * volume->record_size);
						if (const NTFS::ATTR_RECORD *a = GetAttribute(rec2, entry->type, entry->instance))
							read_attribute(e, a);
					}
				}
				break;
			}
		}

		iterator(const NTFSVolume *_volume, NTFS::MFT_RECORD *_rec) : volume(_volume), rec(_rec) {}
		iterator& operator++() {
			rec = (NTFS::MFT_RECORD*)((uint8*)rec + volume->record_size);
			return *this;
		}
		element operator*() const {
			element	v(rec);
			if (rec->magic == NTFS::MFT_RECORD::FILE && rec->flags.test(NTFS::MFT_RECORD::IN_USE) && rec->PatchUSA(volume->record_size)) {
				for (auto i = rec->begin(), e = rec->end(); i != e && i->type != NTFS::AT_UNUSED && i->type != NTFS::AT_END; ++i)
					read_attribute(v, &*i);
			}
			return v;
		}
		bool	operator!=(const iterator& i) const { return rec != i.rec; }
	};

	NTFSVolume(HANDLE _h) : h(_h) {
		DWORD	br;
		NTFS_VOLUME_DATA_BUFFER volume_data;
		DeviceIoControl(h, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &volume_data, sizeof(volume_data), &br, NULL);
		cluster_size	= volume_data.BytesPerCluster;
		record_size		= volume_data.BytesPerFileRecordSegment;

		uint64	mft_start_lcn	= volume_data.MftStartLcn.QuadPart;
		uint64	size			= volume_data.MftValidDataLength.QuadPart;

		Win32Handle	hMFT = OpenRelative(h, NTFS::FILE_ID::MFT);
		dynamic_array<ntfs_mapping> ptrs = GetRetrievalPointers(hMFT);

		mft = memory_block(vmem::reserve_commit(size), size);

		uint8			*p = mft;
		FastReader		reader;
		for (ntfs_mapping *i = ptrs.begin(), *e = ptrs.end(); i != e; ++i) {
			uint64		lbn = i->lcn * cluster_size;
			DWORD		read	= i->num * cluster_size;
			reader.Read(h, lbn, p, read);
			p += read;
		}
	}
	~NTFSVolume() {
		vmem::decommit_release(mft, mft.length());
	}

	iterator	begin() {
		return iterator(this, mft);
	}
	iterator	end() {
		return iterator(this, mft.end());
	}
};

struct NTFSVolumeCopier {
	struct Entry {
		string		name;
		uint64		parent;
		int64		size;
		int64		creation_time;
		int64		change_time;
		int64		access_time;
		uint32		attributes;
		Entry() : parent(0) {}
		Entry(const NTFSVolume::element &e)
			: name(str(e.fn->filename, e.fn->filename_length))
			, parent(e.fn->parent_directory)
			, size(e.fn->data_size)
			, creation_time(e.fn->creation_time)
			, change_time(e.fn->last_data_change_time)
			, access_time(e.fn->last_access_time)
		{}
	};
	struct File : Entry {
		const NTFS::ATTR_RECORD *data_attr;
		File(const NTFSVolume::element &e) : Entry(e), data_attr(e.data) {}

		dynamic_array<ntfs_mapping> data() const {
			if (data_attr->is_non_resident)
				return UnpackMappingPairs(data_attr->mapping_pairs(), 0);

			dynamic_array<ntfs_mapping> pairs(1);
			pairs[0].lcn = 0;
			pairs[0].num = size;
			return pairs;
		}
	};
	struct Dir : Entry {
		dynamic_array<Dir*>		subdirs;
		dynamic_array<Entry*>	entries;
		Dir() {}
		Dir(const NTFSVolume::element &e) : Entry(e) {}
	};

	struct Chunk : ntfs_mapping {
		File	&file;
		uint64	offset;
		Chunk(const ntfs_mapping &a, File &_file, uint64 _offset) : ntfs_mapping(a), file(_file), offset(_offset) {}
		bool	operator<(const Chunk &b) const { return lcn < b.lcn; }
	};

	Win32Handle				fh;
	map<uint64, Dir>		dirs;
	dynamic_array<File>		files;
	dynamic_array<Chunk>	chunks;

	string_accum &get_filename(string_accum &a, const Entry &e) {
		if (e.parent)
			get_filename(a, dirs[e.parent]) << '\\';
		return a << e.name;
	}

	NTFSVolumeCopier(const char *from) {
		Win32Handle	fh = CreateFileA(buffer_accum<256>("\\\\.\\") << from
			, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE
			, NULL
			, OPEN_EXISTING
			, 0//FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
			, NULL
		);
		NTFSVolume	vol0(fh);

		int	refno	= 0;
		for (auto &&i : vol0) {
			if (i.fn) {
				if (i.rec->flags.test(NTFS::MFT_RECORD::IS_DIRECTORY)) {
					dirs[refno | (uint64(i.rec->sequence_number) << 48)] = Dir(i);
				} else {
					auto	&file	= files.push_back(i);
					uint64	offset	= 0;
					for (auto &j : file.data()) {
						new(chunks) Chunk(j, file, offset);
						offset += j.num;
					}
				}
			}
			refno++;
		}
	}

	void Copy(const char *to) {
		dynamic_array<int>	indices = int_range(0, (int)chunks.size());
		auto	indexed = make_indexed_container(chunks.begin(), indices);
		sort(indexed);

		FastCopier	copier;
		for (auto &&i : indexed) {
			auto	&j = get(i);
			filename	f;
			fixed_accum	a(f);
			get_filename(a << to << '\\', j.file);
			Win32Handle	th	= CreateFile(f,  GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, 0);
			copier.Copy(fh, j.lcn, th, j.offset, j.num, 2048);

		}
	}
};
