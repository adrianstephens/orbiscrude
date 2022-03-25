#include "iso/iso_binary.h"
#include "filename.h"
#include "vm.h"
#include "windows/nt.h"

#define ULONG32	iso::baseint<16,iso::uint32>
#define ULONG64	iso::baseint<16,iso::uint64>

#include <DbgHelp.h>

using namespace iso;

//-----------------------------------------------------------------------------
//	MINIDUMP
//-----------------------------------------------------------------------------

struct MINIDUMP : mapped_file {
	typedef		string_base<_pascal_string<uint32, char16>>	string;

	MINIDUMP(mapped_file &&mf) : mapped_file(move(mf)) {}
	const MINIDUMP_HEADER&		Header()	const { return *(const MINIDUMP_HEADER*)*this; }

	auto	RawDirectories() const {
		auto&	h = Header();
		return make_range_n((MINIDUMP_DIRECTORY*)((char*)&h + h.StreamDirectoryRva), h.NumberOfStreams);
	}
	auto	Directories() const {
		return with_param(RawDirectories(), this);
	}
	const_memory_block GetMemory(const MINIDUMP_LOCATION_DESCRIPTOR &loc) const {
		return const_memory_block(*this + loc.Rva, loc.DataSize);
	}
	const_memory_block GetDirectory(_MINIDUMP_STREAM_TYPE type) const {
		for (auto &i : RawDirectories()) {
			if (i.StreamType == type)
				return GetMemory(i.Location);
		}
		return none;
	}
	const_memory_block Virtual(uint64 addr) const {
		if (const MINIDUMP_MEMORY_LIST *mem = GetDirectory(MemoryListStream)) {
			for (auto &i : make_range_n(&mem->MemoryRanges[0], mem->NumberOfMemoryRanges)) {
				if (i.StartOfMemoryRange <= addr && addr - i.StartOfMemoryRange < i.Memory.DataSize)
					return GetMemory(i.Memory) + (addr - i.StartOfMemoryRange);
			}
		}
		if (const MINIDUMP_MEMORY64_LIST *mem = GetDirectory(Memory64ListStream)) {
			intptr_t	offset = 0;
			for (auto &i : make_range_n(&mem->MemoryRanges[0], mem->NumberOfMemoryRanges)) {
				if (i.StartOfMemoryRange < addr && addr - i.StartOfMemoryRange < i.DataSize)
					return *this + (offset + addr - i.StartOfMemoryRange);
				offset += i.DataSize;
			}
		}
		return none;
	}

};

inline auto	get(const param_element<MINIDUMP_LOCATION_DESCRIPTOR&, const MINIDUMP*> &a)		{ return const_memory_block(*a.p + a.t.Rva, a.t.DataSize); }
inline auto	get(const param_element<MINIDUMP_LOCATION_DESCRIPTOR64&, const MINIDUMP*> &a)	{ return const_memory_block(*a.p + a.t.Rva, a.t.DataSize); }
inline auto	get(const param_element<MINIDUMP_MEMORY_DESCRIPTOR&, const MINIDUMP*> &a)		{ return ISO::VStartBin<const_memory_block>(xint64(a.t.StartOfMemoryRange), const_memory_block(*a.p + a.t.Memory.Rva, a.t.Memory.DataSize)); }


//-----------------------------------------------------------------------------
//	DMP
//-----------------------------------------------------------------------------

struct PHYSICAL_MEMORY_RUN {
	uint32	BasePage;
	uint32	PageCount;
};

struct PHYSICAL_MEMORY_DESCRIPTOR {
	uint32	NumberOfRuns;
	uint32	NumberOfPages;
	PHYSICAL_MEMORY_RUN Run[1];
};

struct DBGKD_DEBUG_DATA_HEADER32 {
	LIST_ENTRY32	List;
	ULONG			OwnerTag;
	ULONG			Size;
};

struct KDDEBUGGER_DATA32 : DBGKD_DEBUG_DATA_HEADER32 {
	ULONG			KernBase;
	ULONG			BreakpointWithStatus;		// address of breakpoint
	ULONG			SavedContext;
	USHORT			ThCallbackStack;			// offset in thread data
	USHORT			NextCallback;				// saved pointer to next callback frame
	USHORT			FramePointer;				// saved frame pointer
	USHORT			PaeEnabled : 1;
	ULONG			KiCallUserMode;				// kernel routine
	ULONG			KeUserCallbackDispatcher;	// address in ntdll

	ULONG			PsLoadedModuleList;
	ULONG			PsActiveProcessHead;
	ULONG			PspCidTable;

	ULONG			ExpSystemResourcesList;
	ULONG			ExpPagedPoolDescriptor;
	ULONG			ExpNumberOfPagedPools;

	ULONG			KeTimeIncrement;
	ULONG			KeBugCheckCallbackListHead;
	ULONG			KiBugcheckData;

	ULONG			IopErrorLogListHead;

	ULONG			ObpRootDirectoryObject;
	ULONG			ObpTypeObjectType;

	ULONG			MmSystemCacheStart;
	ULONG			MmSystemCacheEnd;
	ULONG			MmSystemCacheWs;

	ULONG			MmPfnDatabase;
	ULONG			MmSystemPtesStart;
	ULONG			MmSystemPtesEnd;
	ULONG			MmSubsectionBase;
	ULONG			MmNumberOfPagingFiles;

	ULONG			MmLowestPhysicalPage;
	ULONG			MmHighestPhysicalPage;
	ULONG			MmNumberOfPhysicalPages;

	ULONG			MmMaximumNonPagedPoolInBytes;
	ULONG			MmNonPagedSystemStart;
	ULONG			MmNonPagedPoolStart;
	ULONG			MmNonPagedPoolEnd;

	ULONG			MmPagedPoolStart;
	ULONG			MmPagedPoolEnd;
	ULONG			MmPagedPoolInformation;
	ULONG			MmPageSize;

	ULONG			MmSizeOfPagedPoolInBytes;

	ULONG			MmTotalCommitLimit;
	ULONG			MmTotalCommittedPages;
	ULONG			MmSharedCommit;
	ULONG			MmDriverCommit;
	ULONG			MmProcessCommit;
	ULONG			MmPagedPoolCommit;
	ULONG			MmExtendedCommit;

	ULONG			MmZeroedPageListHead;
	ULONG			MmFreePageListHead;
	ULONG			MmStandbyPageListHead;
	ULONG			MmModifiedPageListHead;
	ULONG			MmModifiedNoWritePageListHead;
	ULONG			MmAvailablePages;
	ULONG			MmResidentAvailablePages;

	ULONG			PoolTrackTable;
	ULONG			NonPagedPoolDescriptor;

	ULONG			MmHighestUserAddress;
	ULONG			MmSystemRangeStart;
	ULONG			MmUserProbeAddress;

	ULONG			KdPrintCircularBuffer;
	ULONG			KdPrintCircularBufferEnd;
	ULONG			KdPrintWritePointer;
	ULONG			KdPrintRolloverCount;

	ULONG			MmLoadedUserImageList;
};


struct DMP_HEADER {
	uint8				Signature[4];				//0x0
	uint8				ValidDump[4];				//0x4
	uint32				MajorVersion;				//0x8
	uint32				MinorVersion;				//0xc
	int	valid() const {
		return (uint32be&)Signature == 'PAGE' ? ((uint32be&)ValidDump == 'DU64' ? 64 : (uint32be&)ValidDump == 'DUMP' ? 32 : 0) : 0;
	}
};

struct DMP_HEADER32 : DMP_HEADER {//(4096 bytes)
	uint32				DirectoryTableBase;			//0x10
	uint32				PfnDataBase;				//0x14
	uint32				PsLoadedModuleList;			//0x18
	uint32				PsActiveProcessHead;		//0x1c
	uint32				MachineImageType;			//0x20
	uint32				NumberProcessors;			//0x24
	uint32				BugCheckCode;				//0x28
	uint32				BugCheckCodeParameter[4];	//0x2c
	uint8				VersionUser[32];			//0x3c
	uint8				PaeEnabled;					//0x5c
	uint8				KdSecondaryVersion;			//0x5d
	uint8				VersionUser2[2];			//0x5e
	uint32				KdDebuggerDataBlock;		//0x60
	uint8				PhysicalMemoryBlockBuffer[0x320 - 0x64];	//0x64 (PHYSICAL_MEMORY_DESCRIPTOR)
	uint8				ContextRecord[1200];		//0x320
	_EXCEPTION_RECORD32	Exception;					//0x7d0 
	uint8				Comment[128];				//0x820 
	uint32				DumpType;					//0xf88 
	uint32				MiniDumpFields;				//0xf8c 
	uint32				SecondaryDataState;			//0xf90 
	uint32				ProductType;				//0xf94 
	uint32				SuiteMask;					//0xf98 
	uint32				WriterStatus;				//0xf9c 
	uint64				RequiredDumpSpace;			//0xfa0 
	uint64				SystemUpTime;				//0xfb8 
	uint64				SystemTime;					//0xfc0 
	uint8				reserved3[56];				//0xfc8 
};

struct DBGKD_DEBUG_DATA_HEADER64 {
	LIST_ENTRY64	List;		// Link to other blocks
	ULONG			OwnerTag;	// This is a unique tag to identify the owner of the block.
	ULONG			Size;		// This must be initialized to the size of the data block, including this structure.
};

struct KDDEBUGGER_DATA64 : DBGKD_DEBUG_DATA_HEADER64 {
	void*			KernBase;					// Base address of kernel image
	void*			BreakpointWithStatus;		// address of breakpoint
	void*			SavedContext;
	USHORT			ThCallbackStack;			// offset in thread data
	USHORT			NextCallback;				// saved pointer to next callback frame
	USHORT			FramePointer;				// saved frame pointer
	USHORT			PaeEnabled;					// :1
	void*			KiCallUserMode;				// kernel routine
	void*			KeUserCallbackDispatcher;	// address in ntdll

	NT::LIST<NT::_LDR_DATA_TABLE_ENTRY,&NT::_LDR_DATA_TABLE_ENTRY::InLoadOrderLinks>*	PsLoadedModuleList;
	NT::LIST<NT::_EPROCESS,&NT::_EPROCESS::ActiveProcessLinks>*							PsActiveProcessHead;
	void*			PspCidTable;

	void*			ExpSystemResourcesList;
	void*			ExpPagedPoolDescriptor;
	ULONG64			ExpNumberOfPagedPools;

	void*			KeTimeIncrement;
	void*			KeBugCheckCallbackListHead;
	void*			KiBugcheckData;

	void*			IopErrorLogListHead;

	void*			ObpRootDirectoryObject;
	void*			ObpTypeObjectType;

	void*			MmSystemCacheStart;
	void*			MmSystemCacheEnd;
	void*			MmSystemCacheWs;

	void*			MmPfnDatabase;
	void*			MmSystemPtesStart;
	void*			MmSystemPtesEnd;
	void*			MmSubsectionBase;
	ULONG64			MmNumberOfPagingFiles;

	ULONG64			MmLowestPhysicalPage;
	ULONG64			MmHighestPhysicalPage;
	ULONG64			MmNumberOfPhysicalPages;

	ULONG64			MmMaximumNonPagedPoolInBytes;
	void*			MmNonPagedSystemStart;
	void*			MmNonPagedPoolStart;
	void*			MmNonPagedPoolEnd;

	void*			MmPagedPoolStart;
	void*			MmPagedPoolEnd;
	void*			MmPagedPoolInformation;
	ULONG64			MmPageSize;

	ULONG64			MmSizeOfPagedPoolInBytes;

	ULONG64			MmTotalCommitLimit;
	ULONG64			MmTotalCommittedPages;
	ULONG64			MmSharedCommit;
	ULONG64			MmDriverCommit;
	ULONG64			MmProcessCommit;
	ULONG64			MmPagedPoolCommit;
	ULONG64			MmExtendedCommit;

	void*			MmZeroedPageListHead;
	void*			MmFreePageListHead;
	void*			MmStandbyPageListHead;
	void*			MmModifiedPageListHead;
	void*			MmModifiedNoWritePageListHead;
	ULONG64			MmAvailablePages;
	ULONG64			MmResidentAvailablePages;

	void*			PoolTrackTable;
	ULONG64			NonPagedPoolDescriptor;

	ULONG64			MmHighestUserAddress;
	ULONG64			MmSystemRangeStart;
	ULONG64			MmUserProbeAddress;

	void*			KdPrintCircularBuffer;
	void*			KdPrintCircularBufferEnd;
	void*			KdPrintWritePointer;
	void*			KdPrintRolloverCount;

	void*			MmLoadedUserImageList;

	ULONG64			NtBuildLab;
	void*			KiNormalSystemCall;

	ULONG64			KiProcessorBlock;
	void*			MmUnloadedDrivers;
	void*			MmLastUnloadedDriver;
	void*			MmTriageActionTaken;
	void*			MmSpecialPoolTag;
	void*			KernelVerifier;
	void*			MmVerifierData;
	void*			MmAllocatedNonPagedPool;
	void*			MmPeakCommitment;
	void*			MmTotalCommitLimitMaximum;
	void*			CmNtCSDVersion;

	void*			MmPhysicalMemoryBlock;
	void*			MmSessionBase;
	ULONG64			MmSessionSize;
	void*			MmSystemParentTablePage;

	void*			MmVirtualTranslationBase;

	USHORT			OffsetKThreadNextProcessor;
	USHORT			OffsetKThreadTeb;
	USHORT			OffsetKThreadKernelStack;
	USHORT			OffsetKThreadInitialStack;

	USHORT			OffsetKThreadApcProcess;
	USHORT			OffsetKThreadState;
	USHORT			OffsetKThreadBStore;
	USHORT			OffsetKThreadBStoreLimit;

	USHORT			SizeEProcess;
	USHORT			OffsetEprocessPeb;
	USHORT			OffsetEprocessParentCID;
	USHORT			OffsetEprocessDirectoryTableBase;

	USHORT			SizePrcb;
	USHORT			OffsetPrcbDpcRoutine;
	USHORT			OffsetPrcbCurrentThread;
	USHORT			OffsetPrcbMhz;

	USHORT			OffsetPrcbCpuType;
	USHORT			OffsetPrcbVendorString;
	USHORT			OffsetPrcbProcStateContext;
	USHORT			OffsetPrcbNumber;

	USHORT			SizeEThread;

	void*			KdPrintCircularBufferPtr;
	void*			KdPrintBufferSize;

	void*			KeLoaderBlock;

	USHORT			SizePcr;
	USHORT			OffsetPcrSelfPcr;
	USHORT			OffsetPcrCurrentPrcb;
	USHORT			OffsetPcrContainedPrcb;

	USHORT			OffsetPcrInitialBStore;
	USHORT			OffsetPcrBStoreLimit;
	USHORT			OffsetPcrInitialStack;
	USHORT			OffsetPcrStackLimit;

	USHORT			OffsetPrcbPcrPage;
	USHORT			OffsetPrcbProcStateSpecialReg;
	USHORT			GdtR0Code;
	USHORT			GdtR0Data;

	USHORT			GdtR0Pcr;
	USHORT			GdtR3Code;
	USHORT			GdtR3Data;
	USHORT			GdtR3Teb;

	USHORT			GdtLdt;
	USHORT			GdtTss;
	USHORT			Gdt64R3CmCode;
	USHORT			Gdt64R3CmTeb;

	ULONG64			IopNumTriageDumpDataBlocks;
	void*			IopTriageDumpDataBlocks;

	void*			VfCrashDataBlock;
	ULONG64			MmBadPagesDetected;
	ULONG64			MmZeroedPageSingleBitErrorsDetected;

	void*			EtwpDebuggerData;
	USHORT			OffsetPrcbContext;

	USHORT			OffsetPrcbMaxBreakpoints;
	USHORT			OffsetPrcbMaxWatchpoints;

	ULONG			OffsetKThreadStackLimit;
	ULONG			OffsetKThreadStackBase;
	ULONG			OffsetKThreadQueueListEntry;
	ULONG			OffsetEThreadIrpList;

	USHORT			OffsetPrcbIdleThread;
	USHORT			OffsetPrcbNormalDpcState;
	USHORT			OffsetPrcbDpcStack;
	USHORT			OffsetPrcbIsrStack;

	USHORT			SizeKDPC_STACK_FRAME;

	USHORT			OffsetKPriQueueThreadListHead;
	USHORT			OffsetKThreadWaitReason;

	USHORT			Padding;
	ULONG64			PteBase;
};

struct DMP_HEADER64 : DMP_HEADER {//(8192 bytes)
	xint64				DirectoryTableBase;	        //0x10
	xint64				PfnDataBase;	            //0x18
	NT::LIST<NT::_LDR_DATA_TABLE_ENTRY,&NT::_LDR_DATA_TABLE_ENTRY::InLoadOrderLinks>*	PsLoadedModuleList;		//0x20
	NT::LIST<NT::_EPROCESS,&NT::_EPROCESS::ActiveProcessLinks>*							PsActiveProcessHead;	//0x28
	uint32				MachineImageType;	        //0x30
	uint32				NumberProcessors;	        //0x34
	uint32				BugCheckCode;	            //0x38
	xint64				BugCheckCodeParameter[8];	//0x40
	KDDEBUGGER_DATA64*	KdDebuggerDataBlock;	    //0x80
//	void*				KdDebuggerDataBlock;	    //0x80
	uint8				PhysicalMemoryBlockBuffer[0x348 - 0x88];	//0x88 (PHYSICAL_MEMORY_DESCRIPTOR)
	union {
		uint8			ContextRecord[3000];	    //0x348
		_packed<CONTEXT> Context;
	};
	_EXCEPTION_RECORD64	Exception;	                //0xf00
	uint32				DumpType;	                //0xf98
	uint64				RequiredDumpSpace;	        //0xfa0
	uint64				SystemTime;	                //0xfa8
	uint8				Comment[128];	            //0xfb0
	uint64				SystemUpTime;	            //0x1030
	uint32				MiniDumpFields;	            //0x1038
	uint32				SecondaryDataState;	        //0x103c
	uint32				ProductType;	            //0x1040
	uint32				SuiteMask;	                //0x1044
	uint32				WriterStatus;	            //0x1048
	uint8				Unused1;	                //0x104c
	uint8				KdSecondaryVersion;	        //0x104d
	uint8				Unused[2];	                //0x104e
	uint8				_reserved0[4016];	        //0x1050
};


//If the dump is a BMP dump, this is at 0x2000

struct BMP_DUMP_HEADER {// total [0x38, { 
	uint8	Signature[4];		// 0x0	"FDMP"
	uint8	ValidDump[4];		// 0x4	"DUMP
	uint32	unknown[6];
	uint64	FirstPage;			// 0x20 The offset of the first page in the file. 
	uint64	TotalPresentPages;	// 0x28 Total number of pages present in the bitmap. 
	uint64	Pages;				// 0x30	Total number of pages in image; This dictates the total size of the bitmap. This is not the same as the TotalPresentPages which is only the sum of the bits set to 1. 
	uint64	Bitmap[];			// 0x38	bit per page

	bool	valid() const {
		return (uint32be&)Signature == 'SDMP' && (uint32be&)ValidDump == 'DUMP';
	}
	auto	bits() const {
		return make_range_n(bit_pointer<const uint64>(Bitmap), Pages);
	}
};

struct mm_entry {
	union {
		uint64	u;
		struct {
			uint64	present:1, user:1, :3, accessed:1, dirty:1, pagesize:1, :1, copyonwrite:1, prototype:1, :52, nx:1;
		};
	};
};

inline uint64 PagedMemoryTranslate(uint64 v, uint64 dtb, callback<bool(uint64, void*, size_t)>	mem) {
	uint64		a = dtb + ((v >> 36) & 0xff8);
	mm_entry	e;
	mem(a, &e, sizeof(e));
	if (!e.present)
		return 0;

	a = (e.u & 0xffffffffff000ull) | ((v >> 27) & 0xff8);
	mem(a, &e, sizeof(e));
	if (!e.present)
		return 0;

	if (e.pagesize)
		return (e.u & 0xfffffc0000000ull) | (v & 0x3fffffff);

	a = (e.u & 0xffffffffff000ull) + ((v >> 18) & 0xff8);
	mem(a, &e, sizeof(e));
	if (!e.present)
		return 0;

	if (e.pagesize)
		return (e.u & 0xfffffffe00000ull) | (v & 0x1fffff);

	a = (e.u & 0xffffffffff000ull) + ((v >> 9) & 0xff8);
	mem(a, &e, sizeof(e));

	return e.present ? (e.u & 0xffffffffff000ull) | (v & 0xfff) : 0;
}


struct DMP : mapped_file {
	dynamic_array<uint64>	offsets;

	DMP(mapped_file &&mf) : mapped_file(move(mf)) {
		if (auto bmp = BMP()) {
			offsets.resize(bmp->Pages / 64);
			const uint64	*p = bmp->Bitmap;
			uint64	total	= 0;
			for (auto &i : offsets) {
				i = total; 
				total += count_bits(*p++);
			}
			ISO_ASSERT(total == bmp->TotalPresentPages);
		}
	}
	
	const DMP_HEADER64&		RawHeader()	const { return *(const DMP_HEADER64*)*this; }
	const BMP_DUMP_HEADER*	BMP()		const { auto p = (BMP_DUMP_HEADER*)(&RawHeader() + 1); return p->valid() ? p : nullptr; }

	bool operator()(uint64 addr, void *buffer, size_t size) const {
		if (auto bmp = BMP()) {
			uint64	i = addr >> 12;
			if (i < bmp->Pages) {
				uint64	b = bmp->Bitmap[i >> 6];
				if (b & bit64(i & 63)) {
					memcpy(buffer, *this + (bmp->FirstPage + ((offsets[i / 64] + count_bits(b & bits64(i & 63))) << 12) + (addr & 0xfff)), size);
					return true;
				}
			}
		}
		return false;
	}
	arbitrary_const_ptr Physical(uint64 a) const {
		if (auto bmp = BMP()) {
			uint64	i = a >> 12;
			if (i < bmp->Pages) {
				uint64	b = bmp->Bitmap[i >> 6];
				if (b & bit64(i & 63))
					return *this + (bmp->FirstPage + ((offsets[i / 64] + count_bits(b & bits64(i & 63))) << 12) + (a & 0xfff));
			}
		}
		return nullptr;
	}

#if 0
	uint64 PagedMemoryTranslate(uint64 v, uint64 dtb) const {
		uint64		a = dtb + ((v >> 36) & 0xff8);
		mm_entry	e = *Physical(a);
		if (!e.present)
			return 0;

		a = (e.u & 0xffffffffff000ull) | ((v >> 27) & 0xff8);
		e = *Physical(a);
		if (!e.present)
			return 0;

		if (e.pagesize)
			return (e.u & 0xfffffc0000000ull) | (v & 0x3fffffff);

		a = (e.u & 0xffffffffff000ull) + ((v >> 18) & 0xff8);
		e = *Physical(a);
		if (!e.present)
			return 0;

		if (e.pagesize)
			return (e.u & 0xfffffffe00000ull) | (v & 0x1fffff);

		a = (e.u & 0xffffffffff000ull) + ((v >> 9) & 0xff8);
		e = *Physical(a);

		return e.present ? (e.u & 0xffffffffff000ull) | (v & 0xfff) : 0;
	}

	uint64 PagedMemoryTranslate(uint64 v) const {
		return PagedMemoryTranslate(a, RawHeader().DirectoryTableBase);
	}

	arbitrary_const_ptr  Virtual(uint64 a) const {
		return a ? Physical(PagedMemoryTranslate(a)) : nullptr;
	}
	template<typename T> const T *Virtual(const T *t) const {
		return t ? (const T*)Physical(PagedMemoryTranslate(uint64(t))) : nullptr;
	}
#else
	arbitrary_const_ptr  Virtual(uint64 a) const {
		return a ? Physical(PagedMemoryTranslate(a, RawHeader().DirectoryTableBase, this)) : nullptr;
	}
	template<typename T> const T *Virtual(const T *t) const {
		return t ? (const T*)Physical(PagedMemoryTranslate(uint64(t), RawHeader().DirectoryTableBase, this)) : nullptr;
	}
#endif

	auto	Header()	const { return make_param_element(*(DMP_HEADER64*)*this, this); }
};

template<typename T> auto	get(const param_element<T*&, const DMP*> &a)		{ return make_param_element(*a.p->Virtual(a.t), a.p); }
template<typename T> auto	get(const param_element<T* const&, const DMP*> &a)	{ return get(param_element<T*&, const DMP*>(unconst(a.t), a.p)); }

template<typename T, NT::_LIST_ENTRY T::*F> struct DMP_iterator {
	const NT::_LIST_ENTRY	*p;
	const DMP				*d;

	typedef forward_iterator_t	iterator_category;
	DMP_iterator(const NT::_LIST_ENTRY *p, const DMP* d) : p(p), d(d)	{}
	auto			operator*()							const	{ return make_param_element(*T_get_enclosing(p, F), d); }
	bool			operator!=(const DMP_iterator &b)	const	{ return p != b.p; }
	DMP_iterator	operator++()	{ p = d->Virtual(p->Flink); return *this;	}
};

template<typename T> struct DMP_list_maker;
template<typename T> struct DMP_list_maker<NT::_LIST_ENTRY T::*> {
	template<NT::_LIST_ENTRY T::*F> auto make(const param_element<const NT::_LIST_ENTRY&, const DMP*> &a) {
		return make_range(
			DMP_iterator<T,F>(a.p->Virtual(a.t.Flink), a.p),
			DMP_iterator<T,F>(&a.t, a.p)
		);
	}
};
#define make_DMP_list(L, F) T_get_class<DMP_list_maker>(F)->make<F>(L)

template<typename T, NT::_LIST_ENTRY T::*F> auto get(const param_element<NT::LIST<T,F>*&, const DMP*> &a) {
	auto	*p = a.p->Virtual(a.t);
	return make_range(
		DMP_iterator<T,F>(a.p->Virtual(p->Flink), a.p),
		DMP_iterator<T,F>(p, a.p)
	);
}

template<typename T, NT::_LIST_ENTRY T::*F> auto get(const param_element<NT::LIST<T,F>&, const DMP*> &a) {
	return make_range(
		DMP_iterator<T,F>(a.p->Virtual(a.t.Flink), a.p),
		DMP_iterator<T,F>(&a.t, a.p)
	);
}

inline string16	get(const param_element<NT::_UNICODE_STRING&, const DMP*> &a) {
	return string16(a.p->Virtual(a.t.Buffer), a.t.Length / 2);
}
inline string16	get(const param_element<const NT::_UNICODE_STRING&, const DMP*> &a) {
	return string16(a.p->Virtual(a.t.Buffer), a.t.Length / 2);
}
inline string16	get(const param_element<NT::_UNICODE_STRING*&, const DMP*> &a) {
	return a.t ? string16(a.p->Virtual(a.t->Buffer), a.t->Length / 2) : none;
}
