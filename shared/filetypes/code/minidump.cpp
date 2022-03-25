#include "minidump.h"
#include "iso/iso_files.h"
#include "base/algorithm.h"

using namespace iso;

ISO_DEFSAME(M128A,xint32[4]);

#ifdef ISO_PTR64
ISO_DEFUSERCOMPV(CONTEXT,P1Home,P2Home,P3Home,P4Home,P5Home,P6Home,ContextFlags,
	MxCsr,SegCs,SegDs,SegEs,SegFs,SegGs,SegSs,EFlags,
	Dr0,Dr1,Dr2,Dr3,Dr6,Dr7,
	Rax,Rcx,Rdx,Rbx,Rsp,Rbp,Rsi,Rdi,
	R8,R9,R10,R11,R12,R13,R14,R15,
	Rip,Header,Legacy,
	Xmm0,Xmm1,Xmm2,Xmm3,Xmm4,Xmm5,Xmm6,Xmm7,
	Xmm8,Xmm9,Xmm10,Xmm11,Xmm12,Xmm13,Xmm14,Xmm15,
	VectorRegister,VectorControl,DebugControl,LastBranchToRip,LastBranchFromRip,LastExceptionToRip,LastExceptionFromRip
);
#else
//    FLOATING_SAVE_AREA FloatSave;
ISO_DEFUSERCOMPV(CONTEXT,ContextFlags,Dr0,Dr1,Dr2,Dr3,Dr6,Dr7,FloatSave,
	SegGs,SegFs,SegEs,SegDs,
	Edi,Esi,Ebx,Edx,Ecx,Eax,Ebp,Eip,
	SegCs,EFlags,Esp,SegSs,ExtendedRegisters
);
#endif

//-----------------------------------------------------------------------------
//	MINIDUMP
//-----------------------------------------------------------------------------

ISO_DEFUSERCOMPPV(MINIDUMP_THREAD, const MINIDUMP*,ThreadId,SuspendCount,PriorityClass,Priority,Teb,Stack,ThreadContext);
ISO_DEFUSERCOMPPV(MINIDUMP_THREAD_EX, const MINIDUMP*,ThreadId,SuspendCount,PriorityClass,Priority,Teb,Stack,ThreadContext,BackingStore);
ISO_DEFUSERCOMPPV(MINIDUMP_EXCEPTION_STREAM, const MINIDUMP*, ThreadId,ExceptionRecord,ThreadContext);
ISO_DEFUSERCOMPPV(MINIDUMP_MODULE, const MINIDUMP*, BaseOfImage,SizeOfImage,CheckSum,TimeDateStamp,ModuleNameRva,VersionInfo,CvRecord,MiscRecord);

ISO_DEFUSERCOMPV(MINIDUMP_EXCEPTION,			ExceptionCode,ExceptionFlags,ExceptionRecord,ExceptionAddress,NumberParameters,ExceptionInformation);
ISO_DEFUSERCOMPV(MINIDUMP_MEMORY_DESCRIPTOR64,	StartOfMemoryRange,DataSize);
ISO_DEFUSERCOMPV(MINIDUMP_MISC_INFO_2,			SizeOfInfo,Flags1,ProcessId,ProcessCreateTime,ProcessUserTime,ProcessKernelTime,ProcessorMaxMhz,ProcessorCurrentMhz,ProcessorMhzLimit,ProcessorMaxIdleState,ProcessorCurrentIdleState);
ISO_DEFUSERCOMPV(VS_FIXEDFILEINFO,				dwSignature,dwStrucVersion,dwFileVersionMS,dwFileVersionLS,dwProductVersionMS,dwProductVersionLS,dwFileFlagsMask,dwFileFlags,dwFileOS,dwFileType,dwFileSubtype,dwFileDateMS,dwFileDateLS);

tag2 _GetName(const param_element<MINIDUMP_MODULE const&, const MINIDUMP*> &a) {
	return str((const char16*)(*a.p + a.t.ModuleNameRva + 4));
}

tag2 _GetName(const MINIDUMP_DIRECTORY &a) {
	static const char *stream_types[] = {
		"UnusedStream",
		"ReservedStream0",
		"ReservedStream1",
		"ThreadListStream",
		"ModuleListStream",
		"MemoryListStream",
		"ExceptionStream",
		"SystemInfoStream",
		"ThreadExListStream",
		"Memory64ListStream",
		"CommentStreamA",
		"CommentStreamW",
		"HandleDataStream",
		"FunctionTableStream",
		"UnloadedModuleListStream",
		"MiscInfoStream",
		"MemoryInfoListStream",
		"ThreadInfoListStream",
		"HandleOperationListStream",
	};
	return a.StreamType < iso::num_elements(stream_types) ? stream_types[a.StreamType] : 0;
}

template<> struct ISO::def<param_element<MINIDUMP_DIRECTORY&, const MINIDUMP*>> : ISO::VirtualT2<param_element<MINIDUMP_DIRECTORY&, const MINIDUMP*>> {
	typedef	param_element<MINIDUMP_DIRECTORY&, const MINIDUMP*>	T;

	ISO::Browser2	Deref(const T &a) {
		auto	data	= const_memory_block(*a.p + a.t.Location.Rva, a.t.Location.DataSize);

		switch (a.t.StreamType) {
			case ThreadListStream: {
				const MINIDUMP_THREAD_LIST	*list	= data;
				return MakeBrowser(with_param(make_range_n(&list->Threads[0], list->NumberOfThreads), a.p));
			}
			case ModuleListStream: {
				const MINIDUMP_MODULE_LIST	*list	= data;
				return MakeBrowser(with_param(make_range_n(&list->Modules[0], list->NumberOfModules), a.p));
			}
			case MemoryListStream: {
				const MINIDUMP_MEMORY_LIST	*list	= data;
				return MakeBrowser(with_param(make_range_n(&list->MemoryRanges[0], list->NumberOfMemoryRanges), a.p));
			}
			case ExceptionStream:
				return MakeBrowser(make_param_element(*(const MINIDUMP_EXCEPTION_STREAM*)data, a.p));

//			case SystemInfoStream:
//				return MakeBrowser(*(const MINIDUUMP_SYSTEM_INFO>*)data);

			case ThreadExListStream: {
				const MINIDUMP_THREAD_EX_LIST	*list	= data;
				return MakeBrowser(with_param(make_range_n(&list->Threads[0], list->NumberOfThreads), a.p));
			}
			case Memory64ListStream: {
				const MINIDUMP_MEMORY64_LIST	*list	= data;
				return MakeBrowser(make_range_n(&list->MemoryRanges[0], list->NumberOfMemoryRanges));
			}
			case CommentStreamA:
				return MakeBrowser((const char*)data);

			case CommentStreamW:
				return MakeBrowser((const char16*)data);

//			case HandleDataStream:
//				return MakeBrowser(*(const MINIDUMP_HANDLE_DATA_STREAM>*)data);

			case FunctionTableStream:		//The stream contains function table information. For more information, see MINIDUMP_FUNCTION_TABLE_STREAM.
				break;
			case UnloadedModuleListStream:	//The stream contains module information for the unloaded modules. For more information, see MINIDUMP_UNLOADED_MODULE_LIST.
				break;
			case MiscInfoStream:
				return MakeBrowser(*(const MINIDUMP_MISC_INFO_2*)data);

			case MemoryInfoListStream:		//The stream contains memory region description information. It corresponds to the information that would be returned for the process from the VirtualQuery function. For more information, see MINIDUMP_MEMORY_INFO_LIST.
				break;
			case ThreadInfoListStream:		//The stream contains thread state information. For more information, see MINIDUMP_THREAD_INFO_LIST.
				break;
			case HandleOperationListStream:	//This stream contains operation list information. For more information, see MINIDUMP_HANDLE_OPERATION_LIST.
				break;
			default:
				break;
		}
		return MakeBrowser(move(data));
	}
};

ISO_DEFUSERCOMPV(MINIDUMP, Directories);

class MinidumpFileHandler : public FileHandler {
	const char*		GetExt() override { return "dmp"; }
	const char*		GetDescription() override { return "Minidump";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		MINIDUMP_HEADER	h	= file.get();
		return h.Signature == MINIDUMP_SIGNATURE ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
} minidump;

ISO_ptr<void> MinidumpFileHandler::ReadWithFilename(tag id, const filename &fn) {
	MINIDUMP		dmp((const char*)fn);
	if (!dmp || dmp.Header().Signature != MINIDUMP_SIGNATURE)
		return ISO_NULL;

	return MakePtr(id, move(dmp));
}

//-----------------------------------------------------------------------------
//	DMP
//-----------------------------------------------------------------------------

struct ISO_DMP : DMP {
	dynamic_array<ISO::VStartBin<const_memory_block>>	PhysicalMem;

	uint64 add_physical(const uint8 *data, uint64 startpage, uint64 endpage) {
		uint64	len = (endpage - startpage) << 12;
		PhysicalMem.emplace_back(startpage << 12, const_memory_block(data, len));
		return len;
	}

	ISO_DMP(mapped_file &&mf) : DMP(move(mf)) {
		if (auto *bmp = BMP()) {
			const uint8	*data = (const uint8*)*this + bmp->FirstPage;
			auto	bits = bmp->bits();

			for (int i = 0, j; i < bits.size32(); i = j) {
				i = bits.next(i, true);
				j = bits.next(i, false);
				data += add_physical(data, i, j);
			}
		}
	}
};

auto	get(const param_element<void*&, const DMP*> &a)		{ return ISO::VStartBin<const_memory_block>((uint64)a.t, const_memory_block(a.p->Virtual(a.t), 1024)); }

ISO_DEFUSERCOMPV(LIST_ENTRY64, Flink, Blink);
ISO_DEFUSERCOMPV(DBGKD_DEBUG_DATA_HEADER64, List, OwnerTag, Size);

ISO_DEFUSERCOMPV(PHYSICAL_MEMORY_RUN, BasePage, PageCount);
ISO_DEFUSERCOMPV(PHYSICAL_MEMORY_DESCRIPTOR, NumberOfRuns, NumberOfPages, Run);
ISO_DEFUSERCOMPV(_EXCEPTION_RECORD64,ExceptionCode,ExceptionFlags,ExceptionRecord,ExceptionAddress,NumberParameters,ExceptionInformation);

ISO_DEFUSERCOMPPV(DMP_HEADER64, const DMP*,
	Signature,ValidDump,MajorVersion,MinorVersion,DirectoryTableBase,PfnDataBase,PsLoadedModuleList,PsActiveProcessHead,
	MachineImageType,NumberProcessors,BugCheckCode,BugCheckCodeParameter,KdDebuggerDataBlock,PhysicalMemoryBlockBuffer,Context,Exception,
	DumpType,RequiredDumpSpace,SystemTime,Comment,SystemUpTime,MiniDumpFields,SecondaryDataState,ProductType,
	SuiteMask,WriterStatus,KdSecondaryVersion
);

ISO_DEFUSERCOMPP(KDDEBUGGER_DATA64, const DMP*, 149) {
	ISO_SETFIELDS8(0,KernBase,BreakpointWithStatus,SavedContext,ThCallbackStack,NextCallback,FramePointer,PaeEnabled,KiCallUserMode);
	ISO_SETFIELDS8(8,KeUserCallbackDispatcher,PsLoadedModuleList,PsActiveProcessHead,PspCidTable,ExpSystemResourcesList,ExpPagedPoolDescriptor,ExpNumberOfPagedPools,KeTimeIncrement);
	ISO_SETFIELDS8(16,KeBugCheckCallbackListHead,KiBugcheckData,IopErrorLogListHead,ObpRootDirectoryObject,ObpTypeObjectType,MmSystemCacheStart,MmSystemCacheEnd,MmSystemCacheWs);
	ISO_SETFIELDS8(24,MmPfnDatabase,MmSystemPtesStart,MmSystemPtesEnd,MmSubsectionBase,MmNumberOfPagingFiles,MmLowestPhysicalPage,MmHighestPhysicalPage,MmNumberOfPhysicalPages);
	ISO_SETFIELDS8(32,MmMaximumNonPagedPoolInBytes,MmNonPagedSystemStart,MmNonPagedPoolStart,MmNonPagedPoolEnd,MmPagedPoolStart,MmPagedPoolEnd,MmPagedPoolInformation,MmPageSize);
	ISO_SETFIELDS8(40,MmSizeOfPagedPoolInBytes,MmTotalCommitLimit,MmTotalCommittedPages,MmSharedCommit,MmDriverCommit,MmProcessCommit,MmPagedPoolCommit,MmExtendedCommit);
	ISO_SETFIELDS8(48,MmZeroedPageListHead,MmFreePageListHead,MmStandbyPageListHead,MmModifiedPageListHead,MmModifiedNoWritePageListHead,MmAvailablePages,MmResidentAvailablePages,PoolTrackTable);
	ISO_SETFIELDS8(56,NonPagedPoolDescriptor,MmHighestUserAddress,MmSystemRangeStart,MmUserProbeAddress,KdPrintCircularBuffer,KdPrintCircularBufferEnd,KdPrintWritePointer,KdPrintRolloverCount);
	ISO_SETFIELDS8(64,MmLoadedUserImageList,NtBuildLab,KiNormalSystemCall,KiProcessorBlock,MmUnloadedDrivers,MmLastUnloadedDriver,MmTriageActionTaken,MmSpecialPoolTag);
	ISO_SETFIELDS8(72,KernelVerifier,MmVerifierData,MmAllocatedNonPagedPool,MmPeakCommitment,MmTotalCommitLimitMaximum,CmNtCSDVersion,MmPhysicalMemoryBlock,MmSessionBase);
	ISO_SETFIELDS8(80,MmSessionSize,MmSystemParentTablePage,MmVirtualTranslationBase,OffsetKThreadNextProcessor,OffsetKThreadTeb,OffsetKThreadKernelStack,OffsetKThreadInitialStack,OffsetKThreadApcProcess);
	ISO_SETFIELDS7(88,OffsetKThreadState,OffsetKThreadBStore,OffsetKThreadBStoreLimit,SizeEProcess,OffsetEprocessPeb,OffsetEprocessParentCID,OffsetEprocessDirectoryTableBase);
	ISO_SETFIELDS8(95,SizePrcb,OffsetPrcbDpcRoutine,OffsetPrcbCurrentThread,OffsetPrcbMhz,OffsetPrcbCpuType,OffsetPrcbVendorString,OffsetPrcbProcStateContext,OffsetPrcbNumber);
	ISO_SETFIELDS8(103,SizeEThread,KdPrintCircularBufferPtr,KdPrintBufferSize,KeLoaderBlock,SizePcr,OffsetPcrSelfPcr,OffsetPcrCurrentPrcb,OffsetPcrContainedPrcb);
	ISO_SETFIELDS8(111,OffsetPcrInitialBStore,OffsetPcrBStoreLimit,OffsetPcrInitialStack,OffsetPcrStackLimit,OffsetPrcbPcrPage,OffsetPrcbProcStateSpecialReg,GdtR0Code,GdtR0Data);
	ISO_SETFIELDS8(119,GdtR0Pcr,GdtR3Code,GdtR3Data,GdtR3Teb,GdtLdt,GdtTss,Gdt64R3CmCode,Gdt64R3CmTeb);
	ISO_SETFIELDS8(127,IopNumTriageDumpDataBlocks,IopTriageDumpDataBlocks,VfCrashDataBlock,MmBadPagesDetected,MmZeroedPageSingleBitErrorsDetected,EtwpDebuggerData,OffsetPrcbContext,OffsetPrcbMaxBreakpoints);
	ISO_SETFIELDS8(135,OffsetPrcbMaxWatchpoints,OffsetKThreadStackLimit,OffsetKThreadStackBase,OffsetKThreadQueueListEntry,OffsetEThreadIrpList,OffsetPrcbIdleThread,OffsetPrcbNormalDpcState,OffsetPrcbDpcStack);
	ISO_SETFIELDS6(143,OffsetPrcbIsrStack,SizeKDPC_STACK_FRAME,OffsetKPriQueueThreadListHead,OffsetKThreadWaitReason,Padding,PteBase);
}};

template<typename T, NT::_LIST_ENTRY T::*F> struct ISO::def<NT::LIST<T,F>>	: TISO_virtualarray<NT::LIST<T,F>>	{};

#define comma ,
ISO_DEFUSER(NT::_LARGE_INTEGER, int64);
ISO_DEFUSER(NT::_CONTEXT, CONTEXT);

ISO_DEFUSERCOMPPV(NT::_ETHREAD, const DMP*, CreateTime, PicoContext);

typedef NT::LIST<NT::_ETHREAD, &NT::_ETHREAD::ThreadListEntry>	ETHREADLIST;
ISO_DEFUSERCOMPP(NT::_EPROCESS, const DMP*, 2) {
	ISO_SETFIELD(0, CreateTime);
	ISO_SETFIELDT(1, ThreadListHead, ETHREADLIST);
}};

ISO_DEFUSERCOMPPV(NT::_LDR_DATA_TABLE_ENTRY, const DMP*, FullDllName, DllBase, EntryPoint, SizeOfImage);

ISO_DEFUSERCOMPXV(ISO_DMP, "DMP", Header, PhysicalMem);

class CrashDumpFileHandler : public FileHandler {
	const char*		GetExt() override { return "dmp"; }
	const char*		GetDescription() override { return "Full Dump";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		DMP_HEADER	h;
		return file.read(h) && h.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
} crashdump;

inline string_accum &operator<<(string_accum &sa, const param_element<const NT::_UNICODE_STRING&, const DMP*> &u) {
	return sa << count_string16(u.p->Virtual(u.t.Buffer), u.t.Length / 2);
}
tag2 _GetName(const param_element<NT::_LDR_DATA_TABLE_ENTRY const&, const DMP*> &a) {
	auto	u = a.t.BaseDllName;
	return string(a.p->Virtual(u.Buffer), u.Length / 2);
}
tag2 _GetName(const param_element<NT::_ETHREAD const&, const DMP*> &a) {
	auto	u = a.t.ThreadName;
	return u ? string(a.p->Virtual(u->Buffer), u->Length / 2) : none;
}

ISO_ptr<void> CrashDumpFileHandler::ReadWithFilename(tag id, const filename &fn) {
	ISO_DMP		dmp((const char*)fn);
	if (!dmp || !dmp.RawHeader().valid())
		return ISO_NULL;
	/*
	auto	debug = get(dmp.Header().get(&DMP_HEADER64::KdDebuggerDataBlock));
	for (auto &i : get(debug.get(&KDDEBUGGER_DATA64::PsLoadedModuleList))) {
		ISO_TRACEF("mod:") << i.get(&NT::_LDR_DATA_TABLE_ENTRY::BaseDllName) << '\n';
	}
	for (auto &i : get(debug.get(&KDDEBUGGER_DATA64::PsActiveProcessHead))) {
		ISO_TRACEF("proc:") << xint64((intptr_t)&i) << '\n';
	//ISO_SETFIELDT(1, ThreadListHead, NT::LIST<NT::_ETHREAD comma &NT::_ETHREAD::ThreadListEntry>);

		for (auto &t : make_DMP_list(i.get(&NT::_EPROCESS::ThreadListHead), &NT::_ETHREAD::ThreadListEntry)) {
			auto	&t1 = t.t;
			auto	kprcb	= get(t.get(&NT::_ETHREAD::Tcb).get(&NT::_KTHREAD::WaitPrcb));
			ISO_TRACEF("thread:");// << t.get(&NT::_ETHREAD::ThreadName);

		}
	}
	*/
	return MakePtr(id, move(dmp));
}
