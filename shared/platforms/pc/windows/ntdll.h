

// TI = 0x100a
struct LIST_ENTRY64 {
	uint64	Flink;
	uint64	Blink;
};

// TI = 0x100e
struct LIST_ENTRY32 {
	uint32	Flink;
	uint32	Blink;
};

// TI = 0x1056
enum SE_WS_APPX_SIGNATURE_ORIGIN {
	SE_WS_APPX_SIGNATURE_ORIGIN_NOT_VALIDATED	= 0,
	SE_WS_APPX_SIGNATURE_ORIGIN_UNKNOWN			= 1,
	SE_WS_APPX_SIGNATURE_ORIGIN_APPSTORE		= 2,
	SE_WS_APPX_SIGNATURE_ORIGIN_WINDOWS			= 3,
	SE_WS_APPX_SIGNATURE_ORIGIN_ENTERPRISE		= 4,
};

// TI = 0x1058
enum _PS_MITIGATION_OPTION {
	PS_MITIGATION_OPTION_NX										= 0,
	PS_MITIGATION_OPTION_SEHOP									= 1,
	PS_MITIGATION_OPTION_FORCE_RELOCATE_IMAGES					= 2,
	PS_MITIGATION_OPTION_HEAP_TERMINATE							= 3,
	PS_MITIGATION_OPTION_BOTTOM_UP_ASLR							= 4,
	PS_MITIGATION_OPTION_HIGH_ENTROPY_ASLR						= 5,
	PS_MITIGATION_OPTION_STRICT_HANDLE_CHECKS					= 6,
	PS_MITIGATION_OPTION_WIN32K_SYSTEM_CALL_DISABLE				= 7,
	PS_MITIGATION_OPTION_EXTENSION_POINT_DISABLE				= 8,
	PS_MITIGATION_OPTION_PROHIBIT_DYNAMIC_CODE					= 9,
	PS_MITIGATION_OPTION_CONTROL_FLOW_GUARD						= 10,
	PS_MITIGATION_OPTION_BLOCK_NON_MICROSOFT_BINARIES			= 11,
	PS_MITIGATION_OPTION_FONT_DISABLE							= 12,
	PS_MITIGATION_OPTION_IMAGE_LOAD_NO_REMOTE					= 13,
	PS_MITIGATION_OPTION_IMAGE_LOAD_NO_LOW_LABEL				= 14,
	PS_MITIGATION_OPTION_IMAGE_LOAD_PREFER_SYSTEM32				= 15,
	PS_MITIGATION_OPTION_RETURN_FLOW_GUARD						= 16,
	PS_MITIGATION_OPTION_LOADER_INTEGRITY_CONTINUITY			= 17,
	PS_MITIGATION_OPTION_STRICT_CONTROL_FLOW_GUARD				= 18,
	PS_MITIGATION_OPTION_RESTRICT_SET_THREAD_CONTEXT			= 19,
	PS_MITIGATION_OPTION_ROP_STACKPIVOT							= 20,
	PS_MITIGATION_OPTION_ROP_CALLER_CHECK						= 21,
	PS_MITIGATION_OPTION_ROP_SIMEXEC							= 22,
	PS_MITIGATION_OPTION_EXPORT_ADDRESS_FILTER					= 23,
	PS_MITIGATION_OPTION_EXPORT_ADDRESS_FILTER_PLUS				= 24,
	PS_MITIGATION_OPTION_RESTRICT_CHILD_PROCESS_CREATION		= 25,
	PS_MITIGATION_OPTION_IMPORT_ADDRESS_FILTER					= 26,
	PS_MITIGATION_OPTION_MODULE_TAMPERING_PROTECTION			= 27,
	PS_MITIGATION_OPTION_RESTRICT_INDIRECT_BRANCH_PREDICTION	= 28,
	PS_MITIGATION_OPTION_SPECULATIVE_STORE_BYPASS_DISABLE		= 29,
	PS_MITIGATION_OPTION_ALLOW_DOWNGRADE_DYNAMIC_CODE_POLICY	= 30,
	PS_MITIGATION_OPTION_CET_SHADOW_STACKS						= 31,
};

// TI = 0x105d
struct _PS_MITIGATION_OPTIONS_MAP {
	uint64	Map[2];
};

// TI = 0x1060
struct _PS_MITIGATION_AUDIT_OPTIONS_MAP {
	uint64	Map[2];
};

// TI = 0x1cda
struct _XSTATE_FEATURE {
	uint32	Offset;
	uint32	Size;
};

// TI = 0x1a1c
struct _XSTATE_CONFIGURATION {
	uint64			EnabledFeatures;
	uint64			EnabledVolatileFeatures;
	uint32			Size;
	union {
		uint32			ControlFlags;
		struct {
			uint32			OptimizedSave:1;
			uint32			CompactionEnabled:1;
		};
	};
	_XSTATE_FEATURE	Features[64];
	uint64			EnabledSupervisorFeatures;
	uint64			AlignedFeatures;
	uint32			AllFeatureSize;
	uint32			AllFeatures[64];
	uint64			EnabledUserVisibleSupervisorFeatures;
};

// TI = 0x111b
union _LARGE_INTEGER {
	struct {
		uint32	LowPart;
		int		HighPart;
	};
	struct {
		uint32	LowPart;
		int		HighPart;
	}		u;
	int64	QuadPart;
};

// TI = 0x106c
enum _ALTERNATIVE_ARCHITECTURE_TYPE {
	StandardDesign	= 0,
	NEC98x86		= 1,
	EndAlternatives	= 2,
};

// TI = 0x1068
enum _NT_PRODUCT_TYPE {
	NtProductWinNt		= 1,
	NtProductLanManNt	= 2,
	NtProductServer		= 3,
};

// TI = 0x1a06
struct _KSYSTEM_TIME {
	uint32	LowPart;
	int		High1Time;
	int		High2Time;
};

// TI = 0x1085
struct _KUSER_SHARED_DATA {
	uint32							TickCountLowDeprecated;
	uint32							TickCountMultiplier;
	volatile _KSYSTEM_TIME			InterruptTime;
	volatile _KSYSTEM_TIME			SystemTime;
	volatile _KSYSTEM_TIME			TimeZoneBias;
	uint16							ImageNumberLow;
	uint16							ImageNumberHigh;
	wchar_t							NtSystemRoot[520];
	uint32							MaxStackTraceDepth;
	uint32							CryptoExponent;
	uint32							TimeZoneId;
	uint32							LargePageMinimum;
	uint32							AitSamplingValue;
	uint32							AppCompatFlag;
	uint64							RNGSeedVersion;
	uint32							GlobalValidationRunlevel;
	volatile int					TimeZoneBiasStamp;
	uint32							NtBuildNumber;
	_NT_PRODUCT_TYPE				NtProductType;
	uint8							ProductTypeIsValid;
	uint8							Reserved0[1];
	uint16							NativeProcessorArchitecture;
	uint32							NtMajorVersion;
	uint32							NtMinorVersion;
	uint8							ProcessorFeatures[64];
	uint32							Reserved1;
	uint32							Reserved3;
	volatile uint32					TimeSlip;
	_ALTERNATIVE_ARCHITECTURE_TYPE	AlternativeArchitecture;
	uint32							BootId;
	_LARGE_INTEGER					SystemExpirationDate;
	uint32							SuiteMask;
	uint8							KdDebuggerEnabled;
	union {
		uint8							MitigationPolicies;
		struct {
			uint8							NXSupportPolicy:2;
			uint8							SEHValidationPolicy:2;
			uint8							CurDirDevicesSkippedForDlls:2;
			uint8							Reserved:2;
		};
	};
	uint8							Reserved6[2];
	volatile uint32					ActiveConsoleId;
	volatile uint32					DismountCount;
	uint32							ComPlusPackage;
	uint32							LastSystemRITEventTickCount;
	uint32							NumberOfPhysicalPages;
	uint8							SafeBootMode;
	uint8							VirtualizationFlags;
	uint8							Reserved12[2];
	union {
		uint32							SharedDataFlags;
		struct {
			uint32							DbgErrorPortPresent:1;
			uint32							DbgElevationEnabled:1;
			uint32							DbgVirtEnabled:1;
			uint32							DbgInstallerDetectEnabled:1;
			uint32							DbgLkgEnabled:1;
			uint32							DbgDynProcessorEnabled:1;
			uint32							DbgConsoleBrokerEnabled:1;
			uint32							DbgSecureBootEnabled:1;
			uint32							DbgMultiSessionSku:1;
			uint32							DbgMultiUsersInSessionSku:1;
			uint32							DbgStateSeparationEnabled:1;
			uint32							SpareBits:21;
		};
	};
	uint32							DataFlagsPad[1];
	uint64							TestRetInstruction;
	int64							QpcFrequency;
	uint32							SystemCall;
	uint32							SystemCallPad0;
	uint64							SystemCallPad[2];
	union {
		volatile _KSYSTEM_TIME			TickCount;
		volatile uint64					TickCountQuad;
		uint32							ReservedTickCountOverlay[3];
	};
	uint32							TickCountPad[1];
	uint32							Cookie;
	uint32							CookiePad[1];
	int64							ConsoleSessionForegroundProcessId;
	uint64							TimeUpdateLock;
	uint64							BaselineSystemTimeQpc;
	uint64							BaselineInterruptTimeQpc;
	uint64							QpcSystemTimeIncrement;
	uint64							QpcInterruptTimeIncrement;
	uint8							QpcSystemTimeIncrementShift;
	uint8							QpcInterruptTimeIncrementShift;
	uint16							UnparkedProcessorCount;
	uint32							EnclaveFeatureMask[4];
	uint32							TelemetryCoverageRound;
	uint16							UserModeGlobalLogger[16];
	uint32							ImageFileExecutionOptions;
	uint32							LangGenerationCount;
	uint64							Reserved4;
	volatile uint64					InterruptTimeBias;
	volatile uint64					QpcBias;
	uint32							ActiveProcessorCount;
	volatile uint8					ActiveGroupCount;
	uint8							Reserved9;
	union {
		uint16							QpcData;
		struct {
			volatile uint8					QpcBypassEnabled;
			uint8							QpcShift;
		};
	};
	_LARGE_INTEGER					TimeZoneBiasEffectiveStart;
	_LARGE_INTEGER					TimeZoneBiasEffectiveEnd;
	_XSTATE_CONFIGURATION			XState;
};

// TI = 0x108d
union _ULARGE_INTEGER {
	struct {
		uint32	LowPart;
		uint32	HighPart;
	};
	struct {
		uint32	LowPart;
		uint32	HighPart;
	}		u;
	uint64	QuadPart;
};

// TI = 0x10a7
enum _TP_CALLBACK_PRIORITY {
	TP_CALLBACK_PRIORITY_HIGH		= 0,
	TP_CALLBACK_PRIORITY_NORMAL		= 1,
	TP_CALLBACK_PRIORITY_LOW		= 2,
	TP_CALLBACK_PRIORITY_INVALID	= 3,
	TP_CALLBACK_PRIORITY_COUNT		= 3,
};

// TI = 0x109c
struct _TP_CALLBACK_INSTANCE {};

// TI = 0x109a
struct _ACTIVATION_CONTEXT {};

// TI = 0x1095
struct _TP_CLEANUP_GROUP {};

// TI = 0x1093
struct _TP_POOL {};

// TI = 0x10a9
struct _TP_CALLBACK_ENVIRON_V3 {
	uint32						Version;
	struct _TP_POOL				*Pool;
	struct _TP_CLEANUP_GROUP	*CleanupGroup;
	void (*CleanupGroupCancelCallback)(void*, void*);
	void						*RaceDll;
	struct _ACTIVATION_CONTEXT	*ActivationContext;
	void (*FinalizationCallback)(struct _TP_CALLBACK_INSTANCE *, void*);
	union  {
		uint32	Flags;
		struct {
			uint32	LongFunction:1;
			uint32	Persistent:1;
			uint32	Private:30;
		}		s;
	}							u;
	enum _TP_CALLBACK_PRIORITY {
		TP_CALLBACK_PRIORITY_HIGH		= 0,
		TP_CALLBACK_PRIORITY_NORMAL		= 1,
		TP_CALLBACK_PRIORITY_LOW		= 2,
		TP_CALLBACK_PRIORITY_INVALID	= 3,
		TP_CALLBACK_PRIORITY_COUNT		= 3,
	}							CallbackPriority;
	uint32						Size;
};

// TI = 0x1b03
struct _TEB_ACTIVE_FRAME_CONTEXT {
	uint32		Flags;
	const char	*FrameName;
};

// TI = 0x10cd - forward reference
struct _TEB_ACTIVE_FRAME;

// TI = 0x190f
struct _TEB_ACTIVE_FRAME {
	uint32							Flags;
	_TEB_ACTIVE_FRAME				*Previous;
	const _TEB_ACTIVE_FRAME_CONTEXT	*Context;
};

// TI = 0x193d
struct _PROCESSOR_NUMBER {
	uint16	Group;
	uint8	Number;
	uint8	Reserved;
};

// TI = 0x117c
struct _GUID {
	uint32	Data1;
	uint16	Data2;
	uint16	Data3;
	uint8	Data4[8];
};

// TI = 0x10c8 - forward reference
struct _LIST_ENTRY;

// TI = 0x10e6
struct _LIST_ENTRY {
	_LIST_ENTRY	*Flink;
	_LIST_ENTRY	*Blink;
};

// TI = 0x1111
struct _UNICODE_STRING {
	uint16	Length;
	uint16	MaximumLength;
	wchar_t	*Buffer;
};

// TI = 0x1aeb
struct _GDI_TEB_BATCH {
	uint32	Offset:31;
	uint32	HasRenderingCommand:1;
	uint64	HDC;
	uint32	Buffer[310];
};

// TI = 0x196f - forward reference
struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME;

// TI = 0x1ced
struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME {
	_RTL_ACTIVATION_CONTEXT_STACK_FRAME	*Previous;
	struct _ACTIVATION_CONTEXT			*ActivationContext;
	uint32								Flags;
};

// TI = 0x1972
struct _ACTIVATION_CONTEXT_STACK {
	_RTL_ACTIVATION_CONTEXT_STACK_FRAME	*ActiveFrame;
	_LIST_ENTRY							FrameListCache;
	uint32								Flags;
	uint32								NextCookieSequenceNumber;
	uint32								StackId;
};

// TI = 0x1b5a
struct _LEAP_SECOND_DATA {
	uint8			Enabled;
	uint32			Count;
	_LARGE_INTEGER	Data[1];
};

// TI = 0x16b6
struct _FLS_CALLBACK_INFO {};

// TI = 0x16b4
struct _ASSEMBLY_STORAGE_MAP {};

// TI = 0x16b1
struct _ACTIVATION_CONTEXT_DATA {};

// TI = 0x12bc
union _SLIST_HEADER {
	struct {
		uint64	Alignment;
		uint64	Region;
	};
	struct {
		uint64	Depth:16;
		uint64	Sequence:48;
		uint64	Reserved:4;
		uint64	NextEntry:60;
	}		HeaderX64;
};

// TI = 0x1660 - forward reference
struct _RTL_CRITICAL_SECTION;

// TI = 0x1b9e
struct _RTL_CRITICAL_SECTION_DEBUG {
	uint16					Type;
	uint16					CreatorBackTraceIndex;
	_RTL_CRITICAL_SECTION	*CriticalSection;
	_LIST_ENTRY				ProcessLocksList;
	uint32					EntryCount;
	uint32					ContentionCount;
	uint32					Flags;
	uint16					CreatorBackTraceIndexHigh;
	uint16					SpareUSHORT;
};

// TI = 0x166d
struct _RTL_CRITICAL_SECTION {
	_RTL_CRITICAL_SECTION_DEBUG	*DebugInfo;
	int							LockCount;
	int							RecursionCount;
	void						*OwningThread;
	void						*LockSemaphore;
	uint64						SpinCount;
};

// TI = 0x1116
struct _STRING {
	uint16	Length;
	uint16	MaximumLength;
	char	*Buffer;
};

// TI = 0x1b05
struct _RTL_DRIVE_LETTER_CURDIR {
	uint16	Flags;
	uint16	Length;
	uint32	TimeStamp;
	_STRING	DosPath;
};

// TI = 0x1c97
struct _CURDIR {
	_UNICODE_STRING	DosPath;
	void			*Handle;
};

// TI = 0x19e1
struct _RTL_USER_PROCESS_PARAMETERS {
	uint32						MaximumLength;
	uint32						Length;
	uint32						Flags;
	uint32						DebugFlags;
	void						*ConsoleHandle;
	uint32						ConsoleFlags;
	void						*StandardInput;
	void						*StandardOutput;
	void						*StandardError;
	_CURDIR						CurrentDirectory;
	_UNICODE_STRING				DllPath;
	_UNICODE_STRING				ImagePathName;
	_UNICODE_STRING				CommandLine;
	void						*Environment;
	uint32						StartingX;
	uint32						StartingY;
	uint32						CountX;
	uint32						CountY;
	uint32						CountCharsX;
	uint32						CountCharsY;
	uint32						FillAttribute;
	uint32						WindowFlags;
	uint32						ShowWindowFlags;
	_UNICODE_STRING				WindowTitle;
	_UNICODE_STRING				DesktopInfo;
	_UNICODE_STRING				ShellInfo;
	_UNICODE_STRING				RuntimeData;
	_RTL_DRIVE_LETTER_CURDIR	CurrentDirectores[32];
	uint64						EnvironmentSize;
	uint64						EnvironmentVersion;
	void						*PackageDependencyData;
	uint32						ProcessGroupId;
	uint32						LoaderThreads;
	_UNICODE_STRING				RedirectionDllName;
};

// TI = 0x16c1
struct _PEB_LDR_DATA {
	uint32		Length;
	uint8		Initialized;
	void		*SsHandle;
	_LIST_ENTRY	InLoadOrderModuleList;
	_LIST_ENTRY	InMemoryOrderModuleList;
	_LIST_ENTRY	InInitializationOrderModuleList;
	void		*EntryInProgress;
	uint8		ShutdownInProgress;
	void		*ShutdownThreadId;
};

// TI = 0x16be
struct _PEB {
	uint8									InheritedAddressSpace;
	uint8									ReadImageFileExecOptions;
	uint8									BeingDebugged;
	union {
		uint8									BitField;
		struct {
			uint8									ImageUsesLargePages:1;
			uint8									IsProtectedProcess:1;
			uint8									IsImageDynamicallyRelocated:1;
			uint8									SkipPatchingUser32Forwarders:1;
			uint8									IsPackagedProcess:1;
			uint8									IsAppContainer:1;
			uint8									IsProtectedProcessLight:1;
			uint8									IsLongPathAwareProcess:1;
		};
	};
	uint8									Padding0[4];
	void									*Mutant;
	void									*ImageBaseAddress;
	_PEB_LDR_DATA							*Ldr;
	_RTL_USER_PROCESS_PARAMETERS			*ProcessParameters;
	void									*SubSystemData;
	void									*ProcessHeap;
	_RTL_CRITICAL_SECTION					*FastPebLock;
	_SLIST_HEADER							volatile *AtlThunkSListPtr;
	void									*IFEOKey;
	union {
		uint32									CrossProcessFlags;
		struct {
			uint32									ProcessInJob:1;
			uint32									ProcessInitializing:1;
			uint32									ProcessUsingVEH:1;
			uint32									ProcessUsingVCH:1;
			uint32									ProcessUsingFTH:1;
			uint32									ProcessPreviouslyThrottled:1;
			uint32									ProcessCurrentlyThrottled:1;
			uint32									ProcessImagesHotPatched:1;
			uint32									ReservedBits0:24;
		};
	};
	uint8									Padding1[4];
	union {
		void									*KernelCallbackTable;
		void									*UserSharedInfoPtr;
	};
	uint32									SystemReserved;
	uint32									AtlThunkSListPtr32;
	void									*ApiSetMap;
	uint32									TlsExpansionCounter;
	uint8									Padding2[4];
	void									*TlsBitmap;
	uint32									TlsBitmapBits[2];
	void									*ReadOnlySharedMemoryBase;
	void									*SharedData;
	void									**ReadOnlyStaticServerData;
	void									*AnsiCodePageData;
	void									*OemCodePageData;
	void									*UnicodeCaseTableData;
	uint32									NumberOfProcessors;
	uint32									NtGlobalFlag;
	_LARGE_INTEGER							CriticalSectionTimeout;
	uint64									HeapSegmentReserve;
	uint64									HeapSegmentCommit;
	uint64									HeapDeCommitTotalFreeThreshold;
	uint64									HeapDeCommitFreeBlockThreshold;
	uint32									NumberOfHeaps;
	uint32									MaximumNumberOfHeaps;
	void									**ProcessHeaps;
	void									*GdiSharedHandleTable;
	void									*ProcessStarterHelper;
	uint32									GdiDCAttributeList;
	uint8									Padding3[4];
	_RTL_CRITICAL_SECTION					*LoaderLock;
	uint32									OSMajorVersion;
	uint32									OSMinorVersion;
	uint16									OSBuildNumber;
	uint16									OSCSDVersion;
	uint32									OSPlatformId;
	uint32									ImageSubsystem;
	uint32									ImageSubsystemMajorVersion;
	uint32									ImageSubsystemMinorVersion;
	uint8									Padding4[4];
	uint64									ActiveProcessAffinityMask;
	uint32									GdiHandleBuffer[60];
	void (*PostProcessInitRoutine)();
	void									*TlsExpansionBitmap;
	uint32									TlsExpansionBitmapBits[32];
	uint32									SessionId;
	uint8									Padding5[4];
	_ULARGE_INTEGER							AppCompatFlags;
	_ULARGE_INTEGER							AppCompatFlagsUser;
	void									*pShimData;
	void									*AppCompatInfo;
	_UNICODE_STRING							CSDVersion;
	const struct _ACTIVATION_CONTEXT_DATA	*ActivationContextData;
	struct _ASSEMBLY_STORAGE_MAP			*ProcessAssemblyStorageMap;
	const struct _ACTIVATION_CONTEXT_DATA	*SystemDefaultActivationContextData;
	struct _ASSEMBLY_STORAGE_MAP			*SystemAssemblyStorageMap;
	uint64									MinimumStackCommit;
	struct _FLS_CALLBACK_INFO				*FlsCallback;
	_LIST_ENTRY								FlsListHead;
	void									*FlsBitmap;
	uint32									FlsBitmapBits[4];
	uint32									FlsHighIndex;
	void									*WerRegistrationData;
	void									*WerShipAssertPtr;
	void									*pUnused;
	void									*pImageHeaderHash;
	union {
		uint32									TracingFlags;
		struct {
			uint32									HeapTracingEnabled:1;
			uint32									CritSecTracingEnabled:1;
			uint32									LibLoaderTracingEnabled:1;
			uint32									SpareTracingBits:29;
		};
	};
	uint8									Padding6[4];
	uint64									CsrServerReadOnlySharedMemoryBase;
	uint64									TppWorkerpListLock;
	_LIST_ENTRY								TppWorkerpList;
	void									*WaitOnAddressHashTable[128];
	void									*TelemetryCoverageHeader;
	uint32									CloudFileFlags;
	uint32									CloudFileDiagFlags;
	char									PlaceholderCompatibilityMode;
	char									PlaceholderCompatibilityModeReserved[7];
	_LEAP_SECOND_DATA						*LeapSecondData;
	union {
		uint32									LeapSecondFlags;
		struct {
			uint32									SixtySecondEnabled:1;
			uint32									Reserved:31;
		};
	};
	uint32									NtGlobalFlag2;
};

// TI = 0x18f2
struct _CLIENT_ID {
	void	*UniqueProcess;
	void	*UniqueThread;
};

// TI = 0x10b3 - forward reference
struct _NT_TIB;

// TI = 0x1b95
struct _M128A {
	uint64	Low;
	int64	High;
};

// TI = 0x1c61
struct _XSAVE_FORMAT {
	uint16	ControlWord;
	uint16	StatusWord;
	uint8	TagWord;
	uint8	Reserved1;
	uint16	ErrorOpcode;
	uint32	ErrorOffset;
	uint16	ErrorSelector;
	uint16	Reserved2;
	uint32	DataOffset;
	uint16	DataSelector;
	uint16	Reserved3;
	uint32	MxCsr;
	uint32	MxCsr_Mask;
	_M128A	FloatRegisters[8];
	_M128A	XmmRegisters[16];
	uint8	Reserved4[96];
};

// TI = 0x1b79
struct _CONTEXT {
	uint64			P1Home;
	uint64			P2Home;
	uint64			P3Home;
	uint64			P4Home;
	uint64			P5Home;
	uint64			P6Home;
	uint32			ContextFlags;
	uint32			MxCsr;
	uint16			SegCs;
	uint16			SegDs;
	uint16			SegEs;
	uint16			SegFs;
	uint16			SegGs;
	uint16			SegSs;
	uint32			EFlags;
	uint64			Dr0;
	uint64			Dr1;
	uint64			Dr2;
	uint64			Dr3;
	uint64			Dr6;
	uint64			Dr7;
	uint64			Rax;
	uint64			Rcx;
	uint64			Rdx;
	uint64			Rbx;
	uint64			Rsp;
	uint64			Rbp;
	uint64			Rsi;
	uint64			Rdi;
	uint64			R8;
	uint64			R9;
	uint64			R10;
	uint64			R11;
	uint64			R12;
	uint64			R13;
	uint64			R14;
	uint64			R15;
	uint64			Rip;
	union {
		_XSAVE_FORMAT	FltSave;
		struct {
			_M128A			Header[2];
			_M128A			Legacy[8];
			_M128A			Xmm0;
			_M128A			Xmm1;
			_M128A			Xmm2;
			_M128A			Xmm3;
			_M128A			Xmm4;
			_M128A			Xmm5;
			_M128A			Xmm6;
			_M128A			Xmm7;
			_M128A			Xmm8;
			_M128A			Xmm9;
			_M128A			Xmm10;
			_M128A			Xmm11;
			_M128A			Xmm12;
			_M128A			Xmm13;
			_M128A			Xmm14;
			_M128A			Xmm15;
		};
	};
	_M128A			VectorRegister[26];
	uint64			VectorControl;
	uint64			DebugControl;
	uint64			LastBranchToRip;
	uint64			LastBranchFromRip;
	uint64			LastExceptionToRip;
	uint64			LastExceptionFromRip;
};

// TI = 0x1ae4 - forward reference
struct _EXCEPTION_RECORD;

// TI = 0x1d38
struct _EXCEPTION_RECORD {
	int					ExceptionCode;
	uint32				ExceptionFlags;
	_EXCEPTION_RECORD	*ExceptionRecord;
	void				*ExceptionAddress;
	uint32				NumberParameters;
	uint64				ExceptionInformation[15];
};

// TI = 0x1ae3
enum _EXCEPTION_DISPOSITION {
	ExceptionContinueExecution	= 0,
	ExceptionContinueSearch		= 1,
	ExceptionNestedException	= 2,
	ExceptionCollidedUnwind		= 3,
};

// TI = 0x1ac4 - forward reference
struct _EXCEPTION_REGISTRATION_RECORD;

// TI = 0x1aea
struct _EXCEPTION_REGISTRATION_RECORD {
	_EXCEPTION_REGISTRATION_RECORD	*Next;
	_EXCEPTION_DISPOSITION (*Handler)(_EXCEPTION_RECORD *, void*, _CONTEXT *, void*);
};

// TI = 0x1ac8
struct _NT_TIB {
	_EXCEPTION_REGISTRATION_RECORD	*ExceptionList;
	void							*StackBase;
	void							*StackLimit;
	void							*SubSystemTib;
	union {
		void							*FiberData;
		uint32							Version;
	};
	void							*ArbitraryUserPointer;
	_NT_TIB							*Self;
};

// TI = 0x10e1
struct _TEB {
	_NT_TIB						NtTib;
	void						*EnvironmentPointer;
	_CLIENT_ID					ClientId;
	void						*ActiveRpcHandle;
	void						*ThreadLocalStoragePointer;
	_PEB						*ProcessEnvironmentBlock;
	uint32						LastErrorValue;
	uint32						CountOfOwnedCriticalSections;
	void						*CsrClientThread;
	void						*Win32ThreadInfo;
	uint32						User32Reserved[26];
	uint32						UserReserved[5];
	void						*WOW32Reserved;
	uint32						CurrentLocale;
	uint32						FpSoftwareStatusRegister;
	void						*ReservedForDebuggerInstrumentation[16];
	void						*SystemReserved1[30];
	char						PlaceholderCompatibilityMode;
	uint8						PlaceholderHydrationAlwaysExplicit;
	char						PlaceholderReserved[10];
	uint32						ProxiedProcessId;
	_ACTIVATION_CONTEXT_STACK	_ActivationStack;
	uint8						WorkingOnBehalfTicket[8];
	int							ExceptionCode;
	uint8						Padding0[4];
	_ACTIVATION_CONTEXT_STACK	*ActivationContextStackPointer;
	uint64						InstrumentationCallbackSp;
	uint64						InstrumentationCallbackPreviousPc;
	uint64						InstrumentationCallbackPreviousSp;
	uint32						TxFsContext;
	uint8						InstrumentationCallbackDisabled;
	uint8						UnalignedLoadStoreExceptions;
	uint8						Padding1[2];
	_GDI_TEB_BATCH				GdiTebBatch;
	_CLIENT_ID					RealClientId;
	void						*GdiCachedProcessHandle;
	uint32						GdiClientPID;
	uint32						GdiClientTID;
	void						*GdiThreadLocalInfo;
	uint64						Win32ClientInfo[62];
	void						*glDispatchTable[233];
	uint64						glReserved1[29];
	void						*glReserved2;
	void						*glSectionInfo;
	void						*glSection;
	void						*glTable;
	void						*glCurrentRC;
	void						*glContext;
	uint32						LastStatusValue;
	uint8						Padding2[4];
	_UNICODE_STRING				StaticUnicodeString;
	wchar_t						StaticUnicodeBuffer[522];
	uint8						Padding3[6];
	void						*DeallocationStack;
	void						*TlsSlots[64];
	_LIST_ENTRY					TlsLinks;
	void						*Vdm;
	void						*ReservedForNtRpc;
	void						*DbgSsReserved[2];
	uint32						HardErrorMode;
	uint8						Padding4[4];
	void						*Instrumentation[11];
	_GUID						ActivityId;
	void						*SubProcessTag;
	void						*PerflibData;
	void						*EtwTraceData;
	void						*WinSockData;
	uint32						GdiBatchCount;
	union {
		_PROCESSOR_NUMBER			CurrentIdealProcessor;
		uint32						IdealProcessorValue;
		struct {
			uint8						ReservedPad0;
			uint8						ReservedPad1;
			uint8						ReservedPad2;
			uint8						IdealProcessor;
		};
	};
	uint32						GuaranteedStackBytes;
	uint8						Padding5[4];
	void						*ReservedForPerf;
	void						*ReservedForOle;
	uint32						WaitingOnLoaderLock;
	uint8						Padding6[4];
	void						*SavedPriorityState;
	uint64						ReservedForCodeCoverage;
	void						*ThreadPoolData;
	void						**TlsExpansionSlots;
	void						*DeallocationBStore;
	void						*BStoreLimit;
	uint32						MuiGeneration;
	uint32						IsImpersonating;
	void						*NlsCache;
	void						*pShimData;
	uint32						HeapData;
	uint8						Padding7[4];
	void						*CurrentTransactionHandle;
	_TEB_ACTIVE_FRAME			*ActiveFrame;
	void						*FlsData;
	void						*PreferredLanguages;
	void						*UserPrefLanguages;
	void						*MergedPrefLanguages;
	uint32						MuiImpersonation;
	union {
		volatile uint16				CrossTebFlags;
		uint16						SpareCrossTebBits:16;
	};
	union {
		uint16						SameTebFlags;
		struct {
			uint16						SafeThunkCall:1;
			uint16						InDebugPrint:1;
			uint16						HasFiberData:1;
			uint16						SkipThreadAttach:1;
			uint16						WerInShipAssertCode:1;
			uint16						RanProcessInit:1;
			uint16						ClonedThread:1;
			uint16						SuppressDebugMsg:1;
			uint16						DisableUserStackWalk:1;
			uint16						RtlExceptionAttached:1;
			uint16						InitialThread:1;
			uint16						SessionAware:1;
			uint16						LoadOwner:1;
			uint16						LoaderWorker:1;
			uint16						SkipLoaderInit:1;
			uint16						SpareSameTebBits:1;
		};
	};
	void						*TxnScopeEnterCallback;
	void						*TxnScopeExitCallback;
	void						*TxnScopeContext;
	uint32						LockCount;
	int							WowTebOffset;
	void						*ResourceRetValue;
	void						*ReservedForWdf;
	uint64						ReservedForCrt;
	_GUID						EffectiveContainerId;
};

// TI = 0x10f5 - forward reference
struct _SINGLE_LIST_ENTRY;

// TI = 0x10f8
struct _SINGLE_LIST_ENTRY {
	_SINGLE_LIST_ENTRY	*Next;
};

// TI = 0x10fa - forward reference
struct _RTL_SPLAY_LINKS;

// TI = 0x10fd
struct _RTL_SPLAY_LINKS {
	_RTL_SPLAY_LINKS	*Parent;
	_RTL_SPLAY_LINKS	*LeftChild;
	_RTL_SPLAY_LINKS	*RightChild;
};

// TI = 0x1102
struct _RTL_DYNAMIC_HASH_TABLE_CONTEXT {
	_LIST_ENTRY	*ChainHead;
	_LIST_ENTRY	*PrevLinkage;
	uint64		Signature;
};

// TI = 0x193b
struct _RTL_DYNAMIC_HASH_TABLE_ENTRY {
	_LIST_ENTRY	Linkage;
	uint64		Signature;
};

// TI = 0x1107
struct _RTL_DYNAMIC_HASH_TABLE_ENUMERATOR {
	union {
		_RTL_DYNAMIC_HASH_TABLE_ENTRY	HashEntry;
		_LIST_ENTRY						*CurEntry;
	};
	_LIST_ENTRY						*ChainHead;
	uint32							BucketIndex;
};

// TI = 0x110b
struct _RTL_DYNAMIC_HASH_TABLE {
	uint32	Flags;
	uint32	Shift;
	uint32	TableSize;
	uint32	Pivot;
	uint32	DivisorMask;
	uint32	NumEntries;
	uint32	NonEmptyBuckets;
	uint32	NumEnumerators;
	void	*Directory;
};

// TI = 0x1120
struct _RTL_BITMAP {
	uint32	SizeOfBitMap;
	uint32	*Buffer;
};

// TI = 0x1125
struct _LUID {
	uint32	LowPart;
	int		HighPart;
};

// TI = 0x1129
struct _CUSTOM_SYSTEM_EVENT_TRIGGER_CONFIG {
	uint32			Size;
	const wchar_t	*TriggerId;
};

// TI = 0x1cb4
struct _IMAGE_DATA_DIRECTORY {
	uint32	VirtualAddress;
	uint32	Size;
};

// TI = 0x1bd0
struct _IMAGE_OPTIONAL_HEADER64 {
	uint16					Magic;
	uint8					MajorLinkerVersion;
	uint8					MinorLinkerVersion;
	uint32					SizeOfCode;
	uint32					SizeOfInitializedData;
	uint32					SizeOfUninitializedData;
	uint32					AddressOfEntryPoint;
	uint32					BaseOfCode;
	uint64					ImageBase;
	uint32					SectionAlignment;
	uint32					FileAlignment;
	uint16					MajorOperatingSystemVersion;
	uint16					MinorOperatingSystemVersion;
	uint16					MajorImageVersion;
	uint16					MinorImageVersion;
	uint16					MajorSubsystemVersion;
	uint16					MinorSubsystemVersion;
	uint32					Win32VersionValue;
	uint32					SizeOfImage;
	uint32					SizeOfHeaders;
	uint32					CheckSum;
	uint16					Subsystem;
	uint16					DllCharacteristics;
	uint64					SizeOfStackReserve;
	uint64					SizeOfStackCommit;
	uint64					SizeOfHeapReserve;
	uint64					SizeOfHeapCommit;
	uint32					LoaderFlags;
	uint32					NumberOfRvaAndSizes;
	_IMAGE_DATA_DIRECTORY	DataDirectory[16];
};

// TI = 0x19b0
struct _IMAGE_FILE_HEADER {
	uint16	Machine;
	uint16	NumberOfSections;
	uint32	TimeDateStamp;
	uint32	PointerToSymbolTable;
	uint32	NumberOfSymbols;
	uint16	SizeOfOptionalHeader;
	uint16	Characteristics;
};

// TI = 0x1130
struct _IMAGE_NT_HEADERS64 {
	uint32						Signature;
	_IMAGE_FILE_HEADER			FileHeader;
	_IMAGE_OPTIONAL_HEADER64	OptionalHeader;
};

// TI = 0x1136
struct _IMAGE_DOS_HEADER {
	uint16	e_magic;
	uint16	e_cblp;
	uint16	e_cp;
	uint16	e_crlc;
	uint16	e_cparhdr;
	uint16	e_minalloc;
	uint16	e_maxalloc;
	uint16	e_ss;
	uint16	e_sp;
	uint16	e_csum;
	uint16	e_ip;
	uint16	e_cs;
	uint16	e_lfarlc;
	uint16	e_ovno;
	uint16	e_res[4];
	uint16	e_oemid;
	uint16	e_oeminfo;
	uint16	e_res2[10];
	int		e_lfanew;
};

// TI = 0x1139 - forward reference
struct _RTL_BALANCED_NODE;

// TI = 0x113e
struct _RTL_BALANCED_NODE {
	union {
		_RTL_BALANCED_NODE	*Children[2];
		struct {
			_RTL_BALANCED_NODE	*Left;
			_RTL_BALANCED_NODE	*Right;
		};
	};
	union {
		uint8				Red:1;
		uint8				Balance:2;
		uint64				ParentValue;
	};
};

// TI = 0x1142
struct _RTL_RB_TREE {
	_RTL_BALANCED_NODE	*Root;
	union {
		uint8				Encoded:1;
		_RTL_BALANCED_NODE	*Min;
	};
};

// TI = 0x116a
struct _RTL_AVL_TREE {
	_RTL_BALANCED_NODE	*Root;
};

// TI = 0x1186
enum _MODE {
	KernelMode	= 0,
	UserMode	= 1,
	MaximumMode	= 2,
};

// TI = 0x19a3
struct _KREQUEST_PACKET {
	void	*CurrentPacket[3];
	void (*WorkerRoutine)(void*, void*, void*, void*);
};

// TI = 0x11df - forward reference
struct _REQUEST_MAILBOX;

// TI = 0x196e
struct _REQUEST_MAILBOX {
	_REQUEST_MAILBOX	*Next;
	uint64				RequestSummary;
	_KREQUEST_PACKET	RequestPacket;
	volatile int		*NodeTargetCountAddr;
	volatile int		NodeTargetCount;
};

// TI = 0x1190 - forward reference
struct _KSPIN_LOCK_QUEUE;

// TI = 0x1297
struct _KSPIN_LOCK_QUEUE {
	_KSPIN_LOCK_QUEUE	volatile *Next;
	uint64				volatile *Lock;
};

// TI = 0x1aca
struct _KLOCK_QUEUE_HANDLE {
	_KSPIN_LOCK_QUEUE	LockQueue;
	uint8				OldIrql;
};

// TI = 0x1d14
struct _MACHINE_FRAME {
	uint64	Rip;
	uint16	SegCs;
	uint16	Fill1[3];
	uint32	EFlags;
	uint32	Fill2;
	uint64	Rsp;
	uint16	SegSs;
	uint16	Fill3[3];
};

// TI = 0x1b01
struct _MACHINE_CHECK_CONTEXT {
	_MACHINE_FRAME	MachineFrame;
	uint64			Rax;
	uint64			Rcx;
	uint64			Rdx;
	uint64			GsBase;
	uint64			Cr3;
};

// TI = 0x1b85
struct _KTIMER_EXPIRATION_TRACE {
	uint64			InterruptTime;
	_LARGE_INTEGER	PerformanceCounter;
};

// TI = 0x1ac3
struct _KSHARED_READY_QUEUE {
	uint64		Lock;
	uint32		ReadySummary;
	_LIST_ENTRY	ReadyListHead[32];
	char		RunningSummary[64];
	uint8		Span;
	uint8		LowProcIndex;
	uint8		QueueIndex;
	uint8		ProcCount;
	uint8		ScanOwner;
	uint8		Spare[3];
	uint64		Affinity;
	uint32		ReadyThreadCount;
	uint64		ReadyQueueExpectedRunTime;
};

// TI = 0x199e
struct _KSECURE_FAULT_INFORMATION {
	uint64	FaultCode;
	uint64	FaultVa;
};

// TI = 0x1b72
struct _IOP_IRP_STACK_PROFILER {
	uint32	Profile[20];
	uint32	TotalIrps;
};

// TI = 0x1286 - forward reference
struct _KSCB;

// TI = 0x1a91
struct _KSCB {
	uint64				GenerationCycles;
	uint64				MinQuotaCycleTarget;
	uint64				MaxQuotaCycleTarget;
	uint64				RankCycleTarget;
	uint64				LongTermCycles;
	uint64				LastReportedCycles;
	volatile uint64		OverQuotaHistory;
	uint64				ReadyTime;
	uint64				InsertTime;
	_LIST_ENTRY			PerProcessorList;
	_RTL_BALANCED_NODE	QueueNode;
	uint8				Inserted:1;
	uint8				MaxOverQuota:1;
	uint8				MinOverQuota:1;
	uint8				RankBias:1;
	uint8				SoftCap:1;
	uint8				ShareRankOwner:1;
	uint8				Spare1:2;
	uint8				Depth;
	uint16				ReadySummary;
	uint32				Rank;
	volatile uint32		*ShareRank;
	volatile uint32		OwnerShareRank;
	_LIST_ENTRY			ReadyListHead[16];
	_RTL_RB_TREE		ChildScbQueue;
	_KSCB				*Parent;
	_KSCB				*Root;
};

// TI = 0x18fb
union _KLOCK_ENTRY_BOOST_BITMAP {
	uint32	AllFields;
	struct {
		uint32	AllBoosts:17;
		uint32	Reserved:15;
	};
	struct {
		uint16	CpuBoostsBitmap:15;
		uint16	IoBoost:1;
		uint16	IoQoSBoost:1;
		uint16	IoNormalPriorityWaiterCount:8;
		uint16	IoQoSWaiterCount:7;
	};
};

// TI = 0x1b1f
struct _KLOCK_ENTRY_LOCK_STATE {
	union {
		struct {
			uint64	CrossThreadReleasable:1;
			uint64	Busy:1;
			uint64	Reserved:61;
			uint64	InTree:1;
		};
		void	*LockState;
	};
	union {
		void	*SessionState;
		struct {
			uint32	SessionId;
			uint32	SessionPad;
		};
	};
};

// TI = 0x18db
struct _KLOCK_ENTRY {
	union {
		_RTL_BALANCED_NODE			TreeNode;
		_SINGLE_LIST_ENTRY			FreeListEntry;
	};
	union {
		uint32						EntryFlags;
		struct {
			uint8						EntryOffset;
			union {
				uint8						ThreadLocalFlags;
				struct {
					uint8						WaitingBit:1;
					uint8						Spare0:7;
				};
			};
			union {
				uint8						AcquiredByte;
				uint8						AcquiredBit:1;
			};
			union {
				uint8						CrossThreadFlags;
				struct {
					uint8						HeadNodeBit:1;
					uint8						IoPriorityBit:1;
					uint8						IoQoSWaiter:1;
					uint8						Spare1:5;
				};
			};
		};
		struct {
			uint32						StaticState:8;
			uint32						AllFlags:24;
		};
	};
	uint32						SpareFlags;
	union {
		_KLOCK_ENTRY_LOCK_STATE		LockState;
		void						volatile *LockUnsafe;
		struct {
			volatile uint8				CrossThreadReleasableAndBusyByte;
			uint8						Reserved[6];
			volatile uint8				InTreeByte;
			union {
				void						*SessionState;
				struct {
					uint32						SessionId;
					uint32						SessionPad;
				};
			};
		};
	};
	union {
		struct {
			_RTL_RB_TREE				OwnerTree;
			_RTL_RB_TREE				WaiterTree;
		};
		char						CpuPriorityKey;
	};
	uint64						EntryLock;
	_KLOCK_ENTRY_BOOST_BITMAP	BoostBitmap;
	uint32						SparePad;
};

// TI = 0x198b
struct _DISPATCHER_HEADER {
	union {
		volatile int	Lock;
		int				LockNV;
		struct {
			uint8			Type;
			uint8			Signalling;
			uint8			Size;
			uint8			Reserved1;
		};
		struct {
			uint8			TimerType;
			union {
				uint8			TimerControlFlags;
				struct {
					uint8			Absolute:1;
					uint8			Wake:1;
					uint8			EncodedTolerableDelay:6;
				};
			};
			uint8			Hand;
			union {
				uint8			TimerMiscFlags;
				struct {
					uint8			Index:6;
					uint8			Inserted:1;
					volatile uint8	Expired:1;
				};
			};
		};
		struct {
			uint8			Timer2Type;
			union {
				uint8			Timer2Flags;
				struct {
					uint8			Timer2Inserted:1;
					uint8			Timer2Expiring:1;
					uint8			Timer2CancelPending:1;
					uint8			Timer2SetPending:1;
					uint8			Timer2Running:1;
					uint8			Timer2Disabled:1;
					uint8			Timer2ReservedFlags:2;
				};
			};
			uint8			Timer2ComponentId;
			uint8			Timer2RelativeId;
		};
		struct {
			uint8			QueueType;
			union {
				uint8			QueueControlFlags;
				struct {
					uint8			Abandoned:1;
					uint8			DisableIncrement:1;
					uint8			QueueReservedControlFlags:6;
				};
			};
			uint8			QueueSize;
			uint8			QueueReserved;
		};
		struct {
			uint8			ThreadType;
			uint8			ThreadReserved;
			union {
				uint8			ThreadControlFlags;
				struct {
					uint8			CycleProfiling:1;
					uint8			CounterProfiling:1;
					uint8			GroupScheduling:1;
					uint8			AffinitySet:1;
					uint8			Tagged:1;
					uint8			EnergyProfiling:1;
					uint8			SchedulerAssist:1;
					uint8			ThreadReservedControlFlags:1;
				};
			};
			union {
				uint8			DebugActive;
				struct {
					uint8			ActiveDR7:1;
					uint8			Instrumented:1;
					uint8			Minimal:1;
					uint8			Reserved4:3;
					uint8			UmsScheduled:1;
					uint8			UmsPrimary:1;
				};
			};
		};
		struct {
			uint8			MutantType;
			uint8			MutantSize;
			uint8			DpcActive;
			uint8			MutantReserved;
		};
	};
	int				SignalState;
	_LIST_ENTRY		WaitListHead;
};

// TI = 0x12b3
struct _KEVENT {
	_DISPATCHER_HEADER	Header;
};

// TI = 0x119a - forward reference
struct _KTHREAD;

// TI = 0x14c6
struct _KDPC {
	union {
		uint32				TargetInfoAsUlong;
		struct {
			uint8				Type;
			uint8				Importance;
			volatile uint16		Number;
		};
	};
	_SINGLE_LIST_ENTRY	DpcListEntry;
	uint64				ProcessorHistory;
	void (*DeferredRoutine)(_KDPC *, void*, void*, void*);
	void				*DeferredContext;
	void				*SystemArgument1;
	void				*SystemArgument2;
	void				*DpcData;
};

// TI = 0x1bb4
struct _KENTROPY_TIMING_STATE {
	uint32	EntropyCount;
	uint32	Buffer[64];
	_KDPC	Dpc;
	uint32	LastDeliveredBuffer;
};

// TI = 0x1a72
struct _XSAVE_AREA_HEADER {
	uint64	Mask;
	uint64	CompactionMask;
	uint64	Reserved2[6];
};

// TI = 0x18e6
struct _XSAVE_AREA {
	_XSAVE_FORMAT		LegacyState;
	_XSAVE_AREA_HEADER	Header;
};

// TI = 0x18c7
struct _FILESYSTEM_DISK_COUNTERS {
	uint64	FsBytesRead;
	uint64	FsBytesWritten;
};

// TI = 0x1bb0
struct _SYNCH_COUNTERS {
	uint32	SpinLockAcquireCount;
	uint32	SpinLockContentionCount;
	uint32	SpinLockSpinCount;
	uint32	IpiSendRequestBroadcastCount;
	uint32	IpiSendRequestRoutineCount;
	uint32	IpiSendSoftwareInterruptCount;
	uint32	ExInitializeResourceCount;
	uint32	ExReInitializeResourceCount;
	uint32	ExDeleteResourceCount;
	uint32	ExecutiveResourceAcquiresCount;
	uint32	ExecutiveResourceContentionsCount;
	uint32	ExecutiveResourceReleaseExclusiveCount;
	uint32	ExecutiveResourceReleaseSharedCount;
	uint32	ExecutiveResourceConvertsCount;
	uint32	ExAcqResExclusiveAttempts;
	uint32	ExAcqResExclusiveAcquiresExclusive;
	uint32	ExAcqResExclusiveAcquiresExclusiveRecursive;
	uint32	ExAcqResExclusiveWaits;
	uint32	ExAcqResExclusiveNotAcquires;
	uint32	ExAcqResSharedAttempts;
	uint32	ExAcqResSharedAcquiresExclusive;
	uint32	ExAcqResSharedAcquiresShared;
	uint32	ExAcqResSharedAcquiresSharedRecursive;
	uint32	ExAcqResSharedWaits;
	uint32	ExAcqResSharedNotAcquires;
	uint32	ExAcqResSharedStarveExclusiveAttempts;
	uint32	ExAcqResSharedStarveExclusiveAcquiresExclusive;
	uint32	ExAcqResSharedStarveExclusiveAcquiresShared;
	uint32	ExAcqResSharedStarveExclusiveAcquiresSharedRecursive;
	uint32	ExAcqResSharedStarveExclusiveWaits;
	uint32	ExAcqResSharedStarveExclusiveNotAcquires;
	uint32	ExAcqResSharedWaitForExclusiveAttempts;
	uint32	ExAcqResSharedWaitForExclusiveAcquiresExclusive;
	uint32	ExAcqResSharedWaitForExclusiveAcquiresShared;
	uint32	ExAcqResSharedWaitForExclusiveAcquiresSharedRecursive;
	uint32	ExAcqResSharedWaitForExclusiveWaits;
	uint32	ExAcqResSharedWaitForExclusiveNotAcquires;
	uint32	ExSetResOwnerPointerExclusive;
	uint32	ExSetResOwnerPointerSharedNew;
	uint32	ExSetResOwnerPointerSharedOld;
	uint32	ExTryToAcqExclusiveAttempts;
	uint32	ExTryToAcqExclusiveAcquires;
	uint32	ExBoostExclusiveOwner;
	uint32	ExBoostSharedOwners;
	uint32	ExEtwSynchTrackingNotificationsCount;
	uint32	ExEtwSynchTrackingNotificationsAccountedCount;
};

// TI = 0x198d
struct _PEBS_DS_SAVE_AREA {
	uint64	BtsBufferBase;
	uint64	BtsIndex;
	uint64	BtsAbsoluteMaximum;
	uint64	BtsInterruptThreshold;
	uint64	PebsBufferBase;
	uint64	PebsIndex;
	uint64	PebsAbsoluteMaximum;
	uint64	PebsInterruptThreshold;
	uint64	PebsCounterReset0;
	uint64	PebsCounterReset1;
	uint64	PebsCounterReset2;
	uint64	PebsCounterReset3;
};

// TI = 0x1918
struct _PROCESSOR_PROFILE_CONTROL_AREA {
	_PEBS_DS_SAVE_AREA	PebsDsSaveArea;
};

// TI = 0x1a87
struct _KAFFINITY_EX {
	uint16	Count;
	uint16	Size;
	uint32	Reserved;
	uint64	Bitmap[20];
};

// TI = 0x1a4c
enum _PROCESSOR_CACHE_TYPE {
	CacheUnified		= 0,
	CacheInstruction	= 1,
	CacheData			= 2,
	CacheTrace			= 3,
};

// TI = 0x1a4e
struct _CACHE_DESCRIPTOR {
	uint8					Level;
	uint8					Associativity;
	uint16					LineSize;
	uint32					Size;
	_PROCESSOR_CACHE_TYPE	Type;
};

// TI = 0x1baa
struct _KTIMER {
	_DISPATCHER_HEADER	Header;
	_ULARGE_INTEGER		DueTime;
	_LIST_ENTRY			TimerListEntry;
	_KDPC				*Dpc;
	uint32				Processor;
	uint32				Period;
};

// TI = 0x1b45
enum _KHETERO_CPU_QOS {
	KHeteroCpuQosDefault	= 0,
	KHeteroCpuQosHigh		= 0,
	KHeteroCpuQosMedium		= 1,
	KHeteroCpuQosLow		= 2,
	KHeteroCpuQosMultimedia	= 3,
	KHeteroCpuQosDynamic	= 4,
	KHeteroCpuQosMax		= 4,
};

// TI = 0x1b42
struct _POP_FX_DEVICE {};

// TI = 0x1c95
struct _PROC_PERF_HISTORY_ENTRY {
	uint16	Utility;
	uint16	AffinitizedUtility;
	uint8	Frequency;
	uint8	TaggedPercent[2];
};

// TI = 0x1bdd
struct _PROC_PERF_HISTORY {
	uint32						Count;
	uint32						Slot;
	uint32						UtilityTotal;
	uint32						AffinitizedUtilityTotal;
	uint32						FrequencyTotal;
	uint32						TaggedPercentTotal[2];
	_PROC_PERF_HISTORY_ENTRY	HistoryList[1];
};

// TI = 0x1ce8
struct _PROC_PERF_LOAD {
	uint8	BusyPercentage;
	uint8	FrequencyPercentage;
};

// TI = 0x1b52
struct _PPM_CONCURRENCY_ACCOUNTING {
	uint64	Lock;
	uint32	Processors;
	uint32	ActiveProcessors;
	uint64	LastUpdateTime;
	uint64	TotalTime;
	uint64	AccumulatedTime[1];
};

// TI = 0x1cf1
struct _PERF_CONTROL_STATE_SELECTION {
	uint64	SelectedState;
	uint32	SelectedPercent;
	uint32	SelectedFrequency;
	uint32	MinPercent;
	uint32	MaxPercent;
	uint32	TolerancePercent;
	uint32	EppPercent;
	uint32	AutonomousActivityWindow;
	uint8	Autonomous;
	uint8	InheritFromDomain;
};

// TI = 0x1b3a - forward reference
struct _PROC_PERF_CONSTRAINT;

// TI = 0x1cef
struct _PROC_PERF_QOS_CLASS_POLICY {
	uint32	MaxPolicyPercent;
	uint32	MaxEquivalentFrequencyPercent;
	uint32	MinPolicyPercent;
	uint32	AutonomousActivityWindow;
	uint32	EnergyPerfPreference;
	uint8	ProvideGuidance;
	uint8	AllowThrottling;
	uint8	PerfBoostMode;
	uint8	LatencyHintPerf;
	uint8	TrackDesiredCrossClass;
};

// TI = 0x118e - forward reference
struct _KPRCB;

// TI = 0x1b38 - forward reference
struct _PROC_PERF_DOMAIN;

// TI = 0x1d9b
struct _PROC_PERF_CHECK_SNAP {
	uint64	Time;
	uint64	Active;
	uint64	Stall;
	uint64	FrequencyScaledActive;
	uint64	PerformanceScaledActive;
	uint64	PerformanceScaledKernelActive;
	uint64	CyclesActive;
	uint64	CyclesAffinitized;
	uint64	TaggedThreadCycles[2];
	uint32	ResponsivenessEvents;
};

// TI = 0x1cb1
struct _PROC_PERF_CHECK {
	uint64					LastActive;
	uint64					LastTime;
	uint64					LastStall;
	uint32					LastResponsivenessEvents;
	_PROC_PERF_CHECK_SNAP	LastPerfCheckSnap;
	_PROC_PERF_CHECK_SNAP	CurrentSnap;
	_PROC_PERF_CHECK_SNAP	LastDeliveredSnap;
	uint32					LastDeliveredPerformance;
	uint32					LastDeliveredFrequency;
	uint8					TaggedThreadPercent[2];
	uint8					Class0FloorPerfSelection;
	uint8					Class1MinimumPerfSelection;
	uint32					CurrentResponsivenessEvents;
};

// TI = 0x1cdf
struct _PROC_IDLE_SNAP {
	uint64	Time;
	uint64	Idle;
};

// TI = 0x1b9c
struct _PPM_FFH_THROTTLE_STATE_INFO {
	uint8			EnableLogging;
	uint32			MismatchCount;
	uint8			Initialized;
	uint64			LastValue;
	_LARGE_INTEGER	LastLogTickCount;
};

// TI = 0x1b33
enum _PROC_HYPERVISOR_STATE {
	ProcHypervisorNone			= 0,
	ProcHypervisorPresent		= 1,
	ProcHypervisorPower			= 2,
	ProcHypervisorHvCounters	= 3,
};

// TI = 0x1b93
struct _PROC_FEEDBACK_COUNTER {
	union {
		void (*InstantaneousRead)(uint64, uint32*);
		void (*DifferentialRead)(uint64, uint8, uint64*, uint64*);
	};
	uint64	LastActualCount;
	uint64	LastReferenceCount;
	uint32	CachedValue;
	char	_pdb_padding0[4];
	uint8	Affinitized;
	uint8	Differential;
	uint8	DiscardIdleTime;
	uint8	Scaling;
	uint64	Context;
};

// TI = 0x1b8a
struct _PROC_FEEDBACK {
	uint64					Lock;
	uint64					CyclesLast;
	uint64					CyclesActive;
	_PROC_FEEDBACK_COUNTER	*Counters[2];
	uint64					LastUpdateTime;
	uint64					UnscaledTime;
	volatile int64			UnaccountedTime;
	uint64					ScaledTime[2];
	uint64					UnaccountedKernelTime;
	uint64					PerformanceScaledKernelTime;
	uint32					UserTimeLast;
	uint32					KernelTimeLast;
	uint64					IdleGenerationNumberLast;
	uint64					HvActiveTimeLast;
	uint64					StallCyclesLast;
	uint64					StallTime;
	uint8					KernelTimesIndex;
	uint8					CounterDiscardsIdleTime;
};

// TI = 0x1bbf
union _PPM_IDLE_SYNCHRONIZATION_STATE {
	int		AsLong;
	struct {
		int		RefCount:24;
		uint32	State:8;
	};
};

// TI = 0x1d16
struct _PROC_IDLE_POLICY {
	uint8	PromotePercent;
	uint8	DemotePercent;
	uint8	PromotePercentBase;
	uint8	DemotePercentBase;
	uint8	AllowScaling;
	uint8	ForceLightIdle;
};

// TI = 0x1da5
struct _PROC_IDLE_STATE_BUCKET {
	uint64	TotalTime;
	uint64	MinTime;
	uint64	MaxTime;
	uint32	Count;
};

// TI = 0x1df7
struct _PPM_VETO_ENTRY {
	_LIST_ENTRY	Link;
	uint32		VetoReason;
	uint32		ReferenceCount;
	uint64		HitCount;
	uint64		LastActivationTime;
	uint64		TotalActiveTime;
	uint64		CsActivationTime;
	uint64		CsActiveTime;
};

// TI = 0x1dd2
struct _PPM_VETO_ACCOUNTING {
	volatile int	VetoPresent;
	_LIST_ENTRY		VetoListHead;
	uint8			CsAccountingBlocks;
	uint8			BlocksDrips;
	uint32			PreallocatedVetoCount;
	_PPM_VETO_ENTRY	*PreallocatedVetoList;
};

// TI = 0x1dcc
struct _PPM_SELECTION_STATISTICS {
	uint64					SelectedCount;
	uint64					VetoCount;
	uint64					PreVetoCount;
	uint64					WrongProcessorCount;
	uint64					LatencyCount;
	uint64					IdleDurationCount;
	uint64					DeviceDependencyCount;
	uint64					ProcessorDependencyCount;
	uint64					PlatformOnlyCount;
	uint64					InterruptibleCount;
	uint64					LegacyOverrideCount;
	uint64					CstateCheckCount;
	uint64					NoCStateCount;
	uint64					CoordinatedDependencyCount;
	uint64					NotClockOwnerCount;
	_PPM_VETO_ACCOUNTING	*PreVetoAccounting;
};

// TI = 0x1d43
struct _PROC_IDLE_STATE_ACCOUNTING {
	uint64						TotalTime;
	uint32						CancelCount;
	uint32						FailureCount;
	uint32						SuccessCount;
	uint32						InvalidBucketIndex;
	uint64						MinTime;
	uint64						MaxTime;
	_PPM_SELECTION_STATISTICS	SelectionStatistics;
	_PROC_IDLE_STATE_BUCKET		IdleTimeBuckets[26];
};

// TI = 0x1bdf
enum PPM_IDLE_BUCKET_TIME_TYPE {
	PpmIdleBucketTimeInQpc		= 0,
	PpmIdleBucketTimeIn100ns	= 1,
	PpmIdleBucketTimeMaximum	= 2,
};

// TI = 0x1be3
struct _PROC_IDLE_ACCOUNTING {
	uint32						StateCount;
	uint32						TotalTransitions;
	uint32						ResetCount;
	uint32						AbortCount;
	uint64						StartTime;
	uint64						PriorIdleTime;
	PPM_IDLE_BUCKET_TIME_TYPE	TimeUnit;
	_PROC_IDLE_STATE_ACCOUNTING	State[1];
};

// TI = 0x1d30
struct _PPM_IDLE_STATE {
	_KAFFINITY_EX			DomainMembers;
	_UNICODE_STRING			Name;
	uint32					Latency;
	uint32					BreakEvenDuration;
	uint32					Power;
	uint32					StateFlags;
	_PPM_VETO_ACCOUNTING	VetoAccounting;
	uint8					StateType;
	uint8					InterruptsEnabled;
	uint8					Interruptible;
	uint8					ContextRetained;
	uint8					CacheCoherent;
	uint8					WakesSpuriously;
	uint8					PlatformOnly;
	uint8					NoCState;
};

// TI = 0x1dbf
struct _PPM_COORDINATED_SELECTION {
	uint32	MaximumStates;
	uint32	SelectedStates;
	uint32	DefaultSelection;
	uint32	*Selection;
};

// TI = 0x1d94 - forward reference
struct _PPM_SELECTION_MENU_ENTRY;

// TI = 0x1d97
struct _PPM_SELECTION_MENU {
	uint32						Count;
	_PPM_SELECTION_MENU_ENTRY	*Entries;
};

// TI = 0x1e17
struct _PPM_SELECTION_DEPENDENCY {
	uint32				Processor;
	_PPM_SELECTION_MENU	Menu;
};

// TI = 0x1da1
struct _PPM_SELECTION_MENU_ENTRY {
	uint8						StrictDependency;
	uint8						InitiatingState;
	uint8						DependentState;
	uint32						StateIndex;
	uint32						Dependencies;
	_PPM_SELECTION_DEPENDENCY	*DependencyList;
};

// TI = 0x1ddb
struct _PERFINFO_PPM_STATE_SELECTION {
	uint32	SelectedState;
	uint32	VetoedStates;
	uint32	VetoReason[1];
};

// TI = 0x1db9
struct _PROCESSOR_IDLE_DEPENDENCY {
	uint32	ProcessorIndex;
	uint8	ExpectedState;
	uint8	AllowDeeperStates;
	uint8	LooseDependency;
};

// TI = 0x1d20
struct _PROCESSOR_IDLE_CONSTRAINTS {
	uint64	TotalTime;
	uint64	IdleTime;
	uint64	ExpectedIdleDuration;
	uint64	MaxIdleDuration;
	uint32	OverrideState;
	uint32	TimeCheck;
	uint8	PromotePercent;
	uint8	DemotePercent;
	uint8	Parked;
	uint8	Interruptible;
	uint8	PlatformIdle;
	uint8	ExpectedWakeReason;
	uint8	IdleStateMax;
};

// TI = 0x1dad
struct _PROCESSOR_IDLE_PREPARE_INFO {
	void						*Context;
	_PROCESSOR_IDLE_CONSTRAINTS	Constraints;
	uint32						DependencyCount;
	uint32						DependencyUsed;
	_PROCESSOR_IDLE_DEPENDENCY	*DependencyArray;
	uint32						PlatformIdleStateIndex;
	uint32						ProcessorIdleStateIndex;
	uint32						IdleSelectFailureMask;
};

// TI = 0x1cd3
struct _PPM_IDLE_STATES {
	uint8							InterfaceVersion;
	uint8							IdleOverride;
	uint8							EstimateIdleDuration;
	uint8							ExitLatencyTraceEnabled;
	uint8							NonInterruptibleTransition;
	uint8							UnaccountedTransition;
	uint8							IdleDurationLimited;
	uint8							IdleCheckLimited;
	uint8							StrictVetoBias;
	uint32							ExitLatencyCountdown;
	uint32							TargetState;
	uint32							ActualState;
	uint32							OldState;
	uint32							OverrideIndex;
	uint32							ProcessorIdleCount;
	uint32							Type;
	uint64							LevelId;
	uint16							ReasonFlags;
	volatile uint64					InitiateWakeStamp;
	int								PreviousStatus;
	uint32							PreviousCancelReason;
	_KAFFINITY_EX					PrimaryProcessorMask;
	_KAFFINITY_EX					SecondaryProcessorMask;
	void (*IdlePrepare)(_PROCESSOR_IDLE_PREPARE_INFO *);
	int (*IdlePreExecute)(void*, uint32, uint32, uint32, uint32*);
	int (*IdleExecute)(void*, uint64, uint32, uint32, uint32, uint32, uint32*);
	uint32 (*IdlePreselect)(void*, _PROCESSOR_IDLE_CONSTRAINTS *);
	uint32 (*IdleTest)(void*, uint32, uint32);
	uint32 (*IdleAvailabilityCheck)(void*, uint32);
	void (*IdleComplete)(void*, uint32, uint32, uint32, uint32*);
	void (*IdleCancel)(void*, uint32);
	uint8 (*IdleIsHalted)(void*);
	uint8 (*IdleInitiateWake)(void*);
	_PROCESSOR_IDLE_PREPARE_INFO	PrepareInfo;
	_KAFFINITY_EX					DeepIdleSnapshot;
	_PERFINFO_PPM_STATE_SELECTION	*Tracing;
	_PERFINFO_PPM_STATE_SELECTION	*CoordinatedTracing;
	_PPM_SELECTION_MENU				ProcessorMenu;
	_PPM_SELECTION_MENU				CoordinatedMenu;
	_PPM_COORDINATED_SELECTION		CoordinatedSelection;
	_PPM_IDLE_STATE					State[1];
};

// TI = 0x1b47
struct _PROCESSOR_POWER_STATE {
	_PPM_IDLE_STATES							*IdleStates;
	_PROC_IDLE_ACCOUNTING						*IdleAccounting;
	uint64										IdleTimeLast;
	uint64										IdleTimeTotal;
	volatile uint64								IdleTimeEntry;
	uint64										IdleTimeExpiration;
	uint8										NonInterruptibleTransition;
	uint8										PepWokenTransition;
	uint8										HvTargetState;
	uint8										Reserved;
	uint32										TargetIdleState;
	_PROC_IDLE_POLICY							IdlePolicy;
	volatile _PPM_IDLE_SYNCHRONIZATION_STATE	Synchronization;
	_PROC_FEEDBACK								PerfFeedback;
	_PROC_HYPERVISOR_STATE						Hypervisor;
	uint32										LastSysTime;
	uint64										WmiDispatchPtr;
	int											WmiInterfaceEnabled;
	_PPM_FFH_THROTTLE_STATE_INFO				FFHThrottleStateInfo;
	_KDPC										PerfActionDpc;
	volatile int								PerfActionMask;
	_PROC_IDLE_SNAP								HvIdleCheck;
	_PROC_PERF_CHECK							*PerfCheck;
	_PROC_PERF_DOMAIN							*Domain;
	_PROC_PERF_CONSTRAINT						*PerfConstraint;
	_PPM_CONCURRENCY_ACCOUNTING					*Concurrency;
	_PPM_CONCURRENCY_ACCOUNTING					*ClassConcurrency;
	_PROC_PERF_LOAD								*Load;
	_PROC_PERF_HISTORY							*PerfHistory;
	uint8										ArchitecturalEfficiencyClass;
	uint8										PerformanceSchedulingClass;
	uint8										EfficiencySchedulingClass;
	uint8										GuaranteedPerformancePercent;
	uint8										Parked;
	uint8										LongPriorQosPeriod;
	uint16										LatestAffinitizedPercent;
	uint32										LatestPerformancePercent;
	uint32										AveragePerformancePercent;
	uint32										RelativePerformance;
	uint32										Utility;
	uint32										AffinitizedUtility;
	union {
		uint64										SnapTimeLast;
		uint64										EnergyConsumed;
	};
	uint64										ActiveTime;
	uint64										TotalTime;
	struct _POP_FX_DEVICE						*FxDevice;
	uint64										LastQosTranstionTsc;
	uint64										QosTransitionHysteresis;
	_KHETERO_CPU_QOS							RequestedQosClass;
	_KHETERO_CPU_QOS							ResolvedQosClass;
	uint16										QosEquivalencyMask;
	uint16										HwFeedbackTableIndex;
	uint8										HwFeedbackParkHint;
	uint8										HwFeedbackPerformanceClass;
	uint8										HwFeedbackEfficiencyClass;
	uint8										HeteroCoreType;
};

// TI = 0x1a00
struct _KGATE {
	_DISPATCHER_HEADER	Header;
};

// TI = 0x1d3a
struct _KTIMER_TABLE_ENTRY {
	uint64			Lock;
	_LIST_ENTRY		Entry;
	_ULARGE_INTEGER	Time;
};

// TI = 0x1c93
struct _KTIMER_TABLE {
	_KTIMER				*TimerExpiry[64];
	_KTIMER_TABLE_ENTRY	TimerEntries[256];
};

// TI = 0x1b60
struct _KDPC_LIST {
	_SINGLE_LIST_ENTRY	ListHead;
	_SINGLE_LIST_ENTRY	*LastEntry;
};

// TI = 0x18d3
struct _KDPC_DATA {
	_KDPC_LIST		DpcList;
	uint64			DpcLock;
	volatile int	DpcQueueDepth;
	uint32			DpcCount;
	_KDPC			volatile *ActiveDpc;
};

// TI = 0x12c3 - forward reference
struct _LOOKASIDE_LIST_EX;

// TI = 0x12cd
enum _POOL_TYPE {
	NonPagedPool							= 0,
	NonPagedPoolExecute						= 0,
	PagedPool								= 1,
	NonPagedPoolMustSucceed					= 2,
	DontUseThisType							= 3,
	NonPagedPoolCacheAligned				= 4,
	PagedPoolCacheAligned					= 5,
	NonPagedPoolCacheAlignedMustS			= 6,
	MaxPoolType								= 7,
	NonPagedPoolBase						= 0,
	NonPagedPoolBaseMustSucceed				= 2,
	NonPagedPoolBaseCacheAligned			= 4,
	NonPagedPoolBaseCacheAlignedMustS		= 6,
	NonPagedPoolSession						= 32,
	PagedPoolSession						= 33,
	NonPagedPoolMustSucceedSession			= 34,
	DontUseThisTypeSession					= 35,
	NonPagedPoolCacheAlignedSession			= 36,
	PagedPoolCacheAlignedSession			= 37,
	NonPagedPoolCacheAlignedMustSSession	= 38,
	NonPagedPoolNx							= 512,
	NonPagedPoolNxCacheAligned				= 516,
	NonPagedPoolSessionNx					= 544,
};

// TI = 0x12c0 - forward reference
struct _LOOKASIDE_LIST_EX;

// TI = 0x19be
struct _GENERAL_LOOKASIDE_POOL {
	union {
		_SLIST_HEADER		ListHead;
		_SINGLE_LIST_ENTRY	SingleListHead;
	};
	uint16				Depth;
	uint16				MaximumDepth;
	uint32				TotalAllocates;
	union {
		uint32				AllocateMisses;
		uint32				AllocateHits;
	};
	uint32				TotalFrees;
	union {
		uint32				FreeMisses;
		uint32				FreeHits;
	};
	_POOL_TYPE			Type;
	uint32				Tag;
	uint32				Size;
	union {
		void *(*AllocateEx)(_POOL_TYPE, uint64, uint32, _LOOKASIDE_LIST_EX *);
		void *(*Allocate)(_POOL_TYPE, uint64, uint32);
	};
	union {
		void (*FreeEx)(void*, _LOOKASIDE_LIST_EX *);
		void (*Free)(void*);
	};
	_LIST_ENTRY			ListEntry;
	uint32				LastTotalAllocates;
	union {
		uint32				LastAllocateMisses;
		uint32				LastAllocateHits;
	};
	uint32				Future[2];
};

// TI = 0x12c3
struct _LOOKASIDE_LIST_EX {
	_GENERAL_LOOKASIDE_POOL	L;
};

// TI = 0x1353
struct _GENERAL_LOOKASIDE {
	union {
		_SLIST_HEADER		ListHead;
		_SINGLE_LIST_ENTRY	SingleListHead;
	};
	uint16				Depth;
	uint16				MaximumDepth;
	uint32				TotalAllocates;
	union {
		uint32				AllocateMisses;
		uint32				AllocateHits;
	};
	uint32				TotalFrees;
	union {
		uint32				FreeMisses;
		uint32				FreeHits;
	};
	_POOL_TYPE			Type;
	uint32				Tag;
	uint32				Size;
	union {
		void *(*AllocateEx)(_POOL_TYPE, uint64, uint32, _LOOKASIDE_LIST_EX *);
		void *(*Allocate)(_POOL_TYPE, uint64, uint32);
	};
	union {
		void (*FreeEx)(void*, _LOOKASIDE_LIST_EX *);
		void (*Free)(void*);
	};
	_LIST_ENTRY			ListEntry;
	uint32				LastTotalAllocates;
	union {
		uint32				LastAllocateMisses;
		uint32				LastAllocateHits;
	};
	uint32				Future[2];
};

// TI = 0x134d
struct _PP_LOOKASIDE_LIST {
	_GENERAL_LOOKASIDE	*P;
	_GENERAL_LOOKASIDE	*L;
};

// TI = 0x1d2d
struct _KDESCRIPTOR {
	uint16	Pad[3];
	uint16	Limit;
	void	*Base;
};

// TI = 0x1cae
struct _KSPECIAL_REGISTERS {
	uint64			Cr0;
	uint64			Cr2;
	uint64			Cr3;
	uint64			Cr4;
	uint64			KernelDr0;
	uint64			KernelDr1;
	uint64			KernelDr2;
	uint64			KernelDr3;
	uint64			KernelDr6;
	uint64			KernelDr7;
	_KDESCRIPTOR	Gdtr;
	_KDESCRIPTOR	Idtr;
	uint16			Tr;
	uint16			Ldtr;
	uint32			MxCsr;
	uint64			DebugControl;
	uint64			LastBranchToRip;
	uint64			LastBranchFromRip;
	uint64			LastExceptionToRip;
	uint64			LastExceptionFromRip;
	uint64			Cr8;
	uint64			MsrGsBase;
	uint64			MsrGsSwap;
	uint64			MsrStar;
	uint64			MsrLStar;
	uint64			MsrCStar;
	uint64			MsrSyscallMask;
	uint64			Xcr0;
	uint64			MsrFsBase;
	uint64			SpecialPadding0;
};

// TI = 0x1a75
struct _KPROCESSOR_STATE {
	_KSPECIAL_REGISTERS	SpecialRegisters;
	_CONTEXT			ContextFrame;
};

// TI = 0x1a10
union _KPRCBFLAG {
	volatile int	PrcbFlags;
	struct {
		uint32			BamQosLevel:2;
		uint32			PendingQosUpdate:2;
		uint32			CacheIsolationEnabled:1;
		uint32			PrcbFlagsReserved:27;
	};
};

// TI = 0x191a
struct _KHETERO_PROCESSOR_SET {
	uint64	IdealMask;
	uint64	PreferredMask;
	uint64	AvailableMask;
};

// TI = 0x1a02
struct _flags {
	uint8	Removable:1;
	uint8	GroupAssigned:1;
	uint8	GroupCommitted:1;
	uint8	GroupAssignmentFixed:1;
	uint8	Fill:4;
};

// TI = 0x18e8
struct _GROUP_AFFINITY {
	uint64	Mask;
	uint16	Group;
	uint16	Reserved[3];
};

// TI = 0x1358
struct _KNODE {
	uint64					IdleNonParkedCpuSet;
	uint64					IdleSmtSet;
	uint64					IdleCpuSet;
	char					_pdb_padding0[40];
	uint64					DeepIdleSet;
	uint64					IdleConstrainedSet;
	uint64					NonParkedSet;
	uint64					NonIsrTargetedSet;
	int						ParkLock;
	uint32					Seed;
	char					_pdb_padding1[24];
	uint32					SiblingMask;
	union {
		_GROUP_AFFINITY			Affinity;
		struct {
			uint8					AffinityFill[10];
			uint16					NodeNumber;
			uint16					PrimaryNodeNumber;
			uint8					Stride;
			uint8					Spare0;
		};
	};
	uint64					SharedReadyQueueLeaders;
	uint32					ProximityId;
	uint32					Lowest;
	uint32					Highest;
	uint8					MaximumProcessors;
	_flags					Flags;
	uint8					Spare10;
	_KHETERO_PROCESSOR_SET	HeteroSets[5];
	uint64					PpmConfiguredQosSets[4];
};

// TI = 0x11ea
struct _KPRCB {
	uint32							MxCsr;
	uint8							LegacyNumber;
	uint8							ReservedMustBeZero;
	uint8							InterruptRequest;
	uint8							IdleHalt;
	_KTHREAD						*CurrentThread;
	_KTHREAD						*NextThread;
	_KTHREAD						*IdleThread;
	uint8							NestingLevel;
	uint8							ClockOwner;
	union {
		uint8							PendingTickFlags;
		struct {
			uint8							PendingTick:1;
			uint8							PendingBackupTick:1;
		};
	};
	uint8							IdleState;
	uint32							Number;
	uint64							RspBase;
	uint64							PrcbLock;
	char							*PriorityState;
	char							CpuType;
	char							CpuID;
	union {
		uint16							CpuStep;
		struct {
			uint8							CpuStepping;
			uint8							CpuModel;
		};
	};
	uint32							MHz;
	uint64							HalReserved[8];
	uint16							MinorVersion;
	uint16							MajorVersion;
	uint8							BuildType;
	uint8							CpuVendor;
	uint8							CoresPerPhysicalProcessor;
	uint8							LogicalProcessorsPerCore;
	uint64							PrcbPad04[6];
	_KNODE							*ParentNode;
	uint64							GroupSetMember;
	uint8							Group;
	uint8							GroupIndex;
	uint8							PrcbPad05[2];
	uint32							InitialApicId;
	uint32							ScbOffset;
	uint32							ApicMask;
	void							*AcpiReserved;
	uint32							CFlushSize;
	_KPRCBFLAG						PrcbFlags;
	union {
		struct {
			uint64							TrappedSecurityDomain;
			union {
				uint8							BpbState;
				struct {
					uint8							BpbCpuIdle:1;
					uint8							BpbFlushRsbOnTrap:1;
					uint8							BpbIbpbOnReturn:1;
					uint8							BpbIbpbOnTrap:1;
					uint8							BpbIbpbOnRetpolineExit:1;
					uint8							BpbStateReserved:3;
				};
			};
			union {
				uint8							BpbFeatures;
				struct {
					uint8							BpbClearOnIdle:1;
					uint8							BpbEnabled:1;
					uint8							BpbSmep:1;
					uint8							BpbFeaturesReserved:5;
				};
			};
			uint8							BpbCurrentSpecCtrl;
			uint8							BpbKernelSpecCtrl;
			uint8							BpbNmiSpecCtrl;
			uint8							BpbUserSpecCtrl;
			volatile int16					PairRegister;
		};
		uint64							PrcbPad11[2];
	};
	_KPROCESSOR_STATE				ProcessorState;
	_XSAVE_AREA_HEADER				*ExtendedSupervisorState;
	uint32							ProcessorSignature;
	uint32							ProcessorFlags;
	union {
		struct {
			uint8							BpbRetpolineExitSpecCtrl;
			uint8							BpbTrappedRetpolineExitSpecCtrl;
			union {
				uint8							BpbTrappedBpbState;
				struct {
					uint8							BpbTrappedCpuIdle:1;
					uint8							BpbTrappedFlushRsbOnTrap:1;
					uint8							BpbTrappedIbpbOnReturn:1;
					uint8							BpbTrappedIbpbOnTrap:1;
					uint8							BpbTrappedIbpbOnRetpolineExit:1;
					uint8							BpbtrappedBpbStateReserved:3;
				};
			};
			union {
				uint8							BpbRetpolineState;
				struct {
					uint8							BpbRunningNonRetpolineCode:1;
					uint8							BpbIndirectCallsSafe:1;
					uint8							BpbRetpolineEnabled:1;
					uint8							BpbRetpolineStateReserved:5;
				};
			};
			uint32							PrcbPad12b;
		};
		uint64							PrcbPad12a;
	};
	uint64							PrcbPad12[3];
	_KSPIN_LOCK_QUEUE				LockQueue[17];
	_PP_LOOKASIDE_LIST				PPLookasideList[16];
	_GENERAL_LOOKASIDE_POOL			PPNxPagedLookasideList[32];
	_GENERAL_LOOKASIDE_POOL			PPNPagedLookasideList[32];
	_GENERAL_LOOKASIDE_POOL			PPPagedLookasideList[32];
	uint64							PrcbPad20;
	_SINGLE_LIST_ENTRY				DeferredReadyListHead;
	volatile int					MmPageFaultCount;
	volatile int					MmCopyOnWriteCount;
	volatile int					MmTransitionCount;
	volatile int					MmDemandZeroCount;
	volatile int					MmPageReadCount;
	volatile int					MmPageReadIoCount;
	volatile int					MmDirtyPagesWriteCount;
	volatile int					MmDirtyWriteIoCount;
	volatile int					MmMappedPagesWriteCount;
	volatile int					MmMappedWriteIoCount;
	uint32							KeSystemCalls;
	uint32							KeContextSwitches;
	uint32							PrcbPad40;
	uint32							CcFastReadNoWait;
	uint32							CcFastReadWait;
	uint32							CcFastReadNotPossible;
	uint32							CcCopyReadNoWait;
	uint32							CcCopyReadWait;
	uint32							CcCopyReadNoWaitMiss;
	volatile int					IoReadOperationCount;
	volatile int					IoWriteOperationCount;
	volatile int					IoOtherOperationCount;
	_LARGE_INTEGER					IoReadTransferCount;
	_LARGE_INTEGER					IoWriteTransferCount;
	_LARGE_INTEGER					IoOtherTransferCount;
	volatile int					PacketBarrier;
	volatile int					TargetCount;
	volatile uint32					IpiFrozen;
	uint32							PrcbPad30;
	void							*IsrDpcStats;
	uint32							DeviceInterrupts;
	int								LookasideIrpFloat;
	uint32							InterruptLastCount;
	uint32							InterruptRate;
	uint64							LastNonHrTimerExpiration;
	_KPRCB							*PairPrcb;
	uint64							PrcbPad35[1];
	_SLIST_HEADER					InterruptObjectPool;
	uint64							PrcbPad41[6];
	_KDPC_DATA						DpcData[2];
	void							*DpcStack;
	int								MaximumDpcQueueDepth;
	uint32							DpcRequestRate;
	uint32							MinimumDpcRate;
	uint32							DpcLastCount;
	uint8							ThreadDpcEnable;
	volatile uint8					QuantumEnd;
	volatile uint8					DpcRoutineActive;
	volatile uint8					IdleSchedule;
	union {
		volatile int					DpcRequestSummary;
		int16							DpcRequestSlot[2];
		struct {
			int16							NormalDpcState;
			int16							ThreadDpcState;
		};
		struct {
			uint32							DpcNormalProcessingActive:1;
			uint32							DpcNormalProcessingRequested:1;
			uint32							DpcNormalThreadSignal:1;
			uint32							DpcNormalTimerExpiration:1;
			uint32							DpcNormalDpcPresent:1;
			uint32							DpcNormalLocalInterrupt:1;
			uint32							DpcNormalSpare:10;
			uint32							DpcThreadActive:1;
			uint32							DpcThreadRequested:1;
			uint32							DpcThreadSpare:14;
		};
	};
	uint32							LastTimerHand;
	uint32							LastTick;
	uint32							ClockInterrupts;
	uint32							ReadyScanTick;
	void							*InterruptObject[256];
	_KTIMER_TABLE					TimerTable;
	_KGATE							DpcGate;
	void							*PrcbPad52;
	_KDPC							CallDpc;
	int								ClockKeepAlive;
	uint8							PrcbPad60[2];
	uint16							NmiActive;
	int								DpcWatchdogPeriod;
	int								DpcWatchdogCount;
	volatile int					KeSpinLockOrdering;
	uint32							DpcWatchdogProfileCumulativeDpcThreshold;
	void							*CachedPtes;
	_LIST_ENTRY						WaitListHead;
	uint64							WaitLock;
	uint32							ReadySummary;
	int								AffinitizedSelectionMask;
	uint32							QueueIndex;
	uint32							PrcbPad75[3];
	_KDPC							TimerExpirationDpc;
	_RTL_RB_TREE					ScbQueue;
	_LIST_ENTRY						DispatcherReadyListHead[32];
	uint32							InterruptCount;
	uint32							KernelTime;
	uint32							UserTime;
	uint32							DpcTime;
	uint32							InterruptTime;
	uint32							AdjustDpcThreshold;
	uint8							DebuggerSavedIRQL;
	uint8							GroupSchedulingOverQuota;
	volatile uint8					DeepSleep;
	uint8							PrcbPad80;
	uint32							DpcTimeCount;
	uint32							DpcTimeLimit;
	uint32							PeriodicCount;
	uint32							PeriodicBias;
	uint32							AvailableTime;
	uint32							KeExceptionDispatchCount;
	uint32							ReadyThreadCount;
	uint64							ReadyQueueExpectedRunTime;
	uint64							StartCycles;
	uint64							TaggedCyclesStart;
	uint64							TaggedCycles[2];
	uint64							GenerationTarget;
	uint64							AffinitizedCycles;
	uint64							ImportantCycles;
	uint64							UnimportantCycles;
	uint32							DpcWatchdogProfileSingleDpcThreshold;
	volatile int					MmSpinLockOrdering;
	void							volatile *CachedStack;
	uint32							PageColor;
	uint32							NodeColor;
	uint32							NodeShiftedColor;
	uint32							SecondaryColorMask;
	uint8							PrcbPad81[7];
	uint8							TbFlushListActive;
	uint64							PrcbPad82[2];
	uint64							CycleTime;
	uint64							Cycles[4][2];
	uint32							CcFastMdlReadNoWait;
	uint32							CcFastMdlReadWait;
	uint32							CcFastMdlReadNotPossible;
	uint32							CcMapDataNoWait;
	uint32							CcMapDataWait;
	uint32							CcPinMappedDataCount;
	uint32							CcPinReadNoWait;
	uint32							CcPinReadWait;
	uint32							CcMdlReadNoWait;
	uint32							CcMdlReadWait;
	uint32							CcLazyWriteHotSpots;
	uint32							CcLazyWriteIos;
	uint32							CcLazyWritePages;
	uint32							CcDataFlushes;
	uint32							CcDataPages;
	uint32							CcLostDelayedWrites;
	uint32							CcFastReadResourceMiss;
	uint32							CcCopyReadWaitMiss;
	uint32							CcFastMdlReadResourceMiss;
	uint32							CcMapDataNoWaitMiss;
	uint32							CcMapDataWaitMiss;
	uint32							CcPinReadNoWaitMiss;
	uint32							CcPinReadWaitMiss;
	uint32							CcMdlReadNoWaitMiss;
	uint32							CcMdlReadWaitMiss;
	uint32							CcReadAheadIos;
	volatile int					MmCacheTransitionCount;
	volatile int					MmCacheReadCount;
	volatile int					MmCacheIoCount;
	uint32							PrcbPad91;
	void							*MmInternal;
	_PROCESSOR_POWER_STATE			PowerState;
	void							*HyperPte;
	_LIST_ENTRY						ScbList;
	_KDPC							ForceIdleDpc;
	_KDPC							DpcWatchdogDpc;
	_KTIMER							DpcWatchdogTimer;
	_CACHE_DESCRIPTOR				Cache[5];
	uint32							CacheCount;
	volatile uint32					CachedCommit;
	volatile uint32					CachedResidentAvailable;
	void							*WheaInfo;
	void							*EtwSupport;
	void							*ExSaPageArray;
	uint32							KeAlignmentFixupCount;
	uint32							PrcbPad95;
	_SLIST_HEADER					HypercallPageList;
	uint64							*StatisticsPage;
	uint64							PrcbPad85[5];
	void							*HypercallCachedPages;
	void							*VirtualApicAssist;
	_KAFFINITY_EX					PackageProcessorSet;
	uint64							PrcbPad86;
	uint64							SharedReadyQueueMask;
	_KSHARED_READY_QUEUE			*SharedReadyQueue;
	uint32							SharedQueueScanOwner;
	uint32							ScanSiblingIndex;
	uint64							CoreProcessorSet;
	uint64							ScanSiblingMask;
	uint64							LLCMask;
	uint64							CacheProcessorMask[5];
	_PROCESSOR_PROFILE_CONTROL_AREA	*ProcessorProfileControlArea;
	void							*ProfileEventIndexAddress;
	void							**DpcWatchdogProfile;
	void							**DpcWatchdogProfileCurrentEmptyCapture;
	void							*SchedulerAssist;
	_SYNCH_COUNTERS					SynchCounters;
	uint64							PrcbPad94;
	_FILESYSTEM_DISK_COUNTERS		FsCounters;
	uint8							VendorString[13];
	uint8							PrcbPad100[3];
	uint64							FeatureBits;
	_LARGE_INTEGER					UpdateSignature;
	uint64							PteBitCache;
	uint32							PteBitOffset;
	uint32							PrcbPad105;
	_CONTEXT						*Context;
	uint32							ContextFlagsInit;
	uint32							PrcbPad115;
	_XSAVE_AREA						*ExtendedState;
	void							*IsrStack;
	_KENTROPY_TIMING_STATE			EntropyTimingState;
	uint64							PrcbPad110;
	struct {
		uint32		UpdateCycle;
		union {
			int16		PairLocal;
			struct {
				uint8		PairLocalLow;
				uint8		PairLocalForceStibp:1;
				uint8		Reserved:4;
				uint8		Frozen:1;
				uint8		ForceUntrusted:1;
				uint8		SynchIpi:1;
			};
		};
		union {
			int16		PairRemote;
			struct {
				uint8		PairRemoteLow;
				uint8		Reserved2;
			};
		};
		uint8		Trace[24];
		uint64		LocalDomain;
		uint64		RemoteDomain;
		_KTHREAD	*Thread;
	}								StibpPairingTrace;
	_SINGLE_LIST_ENTRY				AbSelfIoBoostsList;
	_SINGLE_LIST_ENTRY				AbPropagateBoostsList;
	_KDPC							AbDpc;
	_IOP_IRP_STACK_PROFILER			IoIrpStackProfilerCurrent;
	_IOP_IRP_STACK_PROFILER			IoIrpStackProfilerPrevious;
	_KSECURE_FAULT_INFORMATION		SecureFault;
	uint64							PrcbPad120;
	_KSHARED_READY_QUEUE			LocalSharedReadyQueue;
	uint64							PrcbPad125[2];
	uint32							TimerExpirationTraceCount;
	uint32							PrcbPad127;
	_KTIMER_EXPIRATION_TRACE		TimerExpirationTrace[16];
	uint64							PrcbPad128[7];
	_REQUEST_MAILBOX				*Mailbox;
	uint64							PrcbPad130[7];
	_MACHINE_CHECK_CONTEXT			McheckContext[2];
	uint64							PrcbPad134[4];
	_KLOCK_QUEUE_HANDLE				SelfmapLockHandle[4];
	uint64							PrcbPad134a[4];
	uint8							PrcbPad138[960];
	uint64							KernelDirectoryTableBase;
	uint64							RspBaseShadow;
	uint64							UserRspShadow;
	uint32							ShadowFlags;
	uint32							DbgMceNestingLevel;
	uint32							DbgMceFlags;
	uint32							PrcbPad139;
	uint64							PrcbPad140[507];
	_REQUEST_MAILBOX				RequestMailbox[1];
};

// TI = 0x1c89
struct _PROC_PERF_DOMAIN {
	_LIST_ENTRY						Link;
	_KPRCB							*Master;
	_KAFFINITY_EX					Members;
	uint64							DomainContext;
	uint32							ProcessorCount;
	uint8							EfficiencyClass;
	uint8							NominalPerformanceClass;
	uint8							HighestPerformanceClass;
	uint8							Hidden;
	_PROC_PERF_CONSTRAINT			*Processors;
	void (*GetFFHThrottleState)(uint64*);
	void (*TimeWindowHandler)(uint64, uint32);
	void (*BoostPolicyHandler)(uint64, uint32);
	void (*BoostModeHandler)(uint64, uint32);
	void (*AutonomousActivityWindowHandler)(uint64, uint32);
	void (*AutonomousModeHandler)(uint64, uint32);
	void (*ReinitializeHandler)(uint64);
	uint32 (*PerfSelectionHandler)(uint64, uint32, uint32, uint32, uint32, uint32, uint32, uint32*, uint64*);
	void (*PerfControlHandler)(uint64, _PERF_CONTROL_STATE_SELECTION *, uint8, uint8);
	void (*PerfControlHandlerHidden)(uint64, _PERF_CONTROL_STATE_SELECTION *, uint8, uint8);
	void (*DomainPerfControlHandler)(uint64, _PERF_CONTROL_STATE_SELECTION *, uint8, uint8);
	uint32							MaxFrequency;
	uint32							NominalFrequency;
	uint32							MaxPercent;
	uint32							MinPerfPercent;
	uint32							MinThrottlePercent;
	uint32							AdvertizedMaximumFrequency;
	uint64							MinimumRelativePerformance;
	uint64							NominalRelativePerformance;
	uint8							NominalRelativePerformancePercent;
	uint8							Coordination;
	uint8							HardPlatformCap;
	uint8							AffinitizeControl;
	uint8							EfficientThrottle;
	uint8							AllowSchedulerDirectedPerfStates;
	uint8							InitiateAllProcessors;
	uint8							AutonomousMode;
	uint8							ProvideGuidance;
	uint32							DesiredPercent;
	uint32							GuaranteedPercent;
	uint8							EngageResponsivenessOverrides;
	_PROC_PERF_QOS_CLASS_POLICY		QosPolicies[4];
	uint32							QosDisableReasons[4];
	uint16							QosEquivalencyMasks[4];
	uint8							QosSupported;
	volatile uint32					SelectionGeneration;
	_PERF_CONTROL_STATE_SELECTION	QosSelection[4];
	uint64							PerfChangeTime;
	uint32							PerfChangeIntervalCount;
	uint8							Force;
	uint8							Update;
	uint8							Apply;
};

// TI = 0x1d3e
struct _PROC_PERF_CONSTRAINT {
	_KPRCB							*Prcb;
	uint64							PerfContext;
	uint8							HiddenProcessor;
	uint32							HiddenProcessorId;
	uint32							PlatformCap;
	uint32							ThermalCap;
	uint32							LimitReasons;
	uint64							PlatformCapStartTime;
	uint32							ProcCap;
	uint32							ProcFloor;
	uint32							TargetPercent;
	uint8							EngageResponsivenessOverrides;
	uint8							ResponsivenessChangeCount;
	_PERF_CONTROL_STATE_SELECTION	Selection;
	uint32							DomainSelectionGeneration;
	uint32							PreviousFrequency;
	uint32							PreviousPercent;
	uint32							LatestFrequencyPercent;
	uint8							Force;
	uint8							UseQosUpdateLock;
	uint64							QosUpdateLock;
};

// TI = 0x127e - forward reference
struct _KAPC;

// TI = 0x1a58
struct _KAPC {
	uint8		Type;
	uint8		SpareByte0;
	uint8		Size;
	uint8		SpareByte1;
	uint32		SpareLong0;
	_KTHREAD	*Thread;
	_LIST_ENTRY	ApcListEntry;
	union {
		struct {
			void (*KernelRoutine)(_KAPC *, void (**)(void*, void*, void*), void **, void **, void **);
			void (*RundownRoutine)(_KAPC *);
			void (*NormalRoutine)(void*, void*, void*);
		};
		void		*Reserved[3];
	};
	void		*NormalContext;
	void		*SystemArgument1;
	void		*SystemArgument2;
	char		ApcStateIndex;
	char		ApcMode;
	uint8		Inserted;
};

// TI = 0x1242 - forward reference
struct _KSCHEDULING_GROUP;

// TI = 0x1c40
struct _KSCHEDULING_GROUP_POLICY {
	union {
		uint32	Value;
		uint16	Weight;
		struct {
			uint16	MinRate;
			uint16	MaxRate;
		};
	};
	union {
		uint32	AllFlags;
		struct {
			uint32	Type:1;
			uint32	Disabled:1;
			uint32	RankBias:1;
			uint32	Spare1:29;
		};
	};
};

// TI = 0x190a
struct _KSCHEDULING_GROUP {
	_KSCHEDULING_GROUP_POLICY	Policy;
	uint32						RelativeWeight;
	uint32						ChildMinRate;
	uint32						ChildMinWeight;
	uint32						ChildTotalWeight;
	uint64						QueryHistoryTimeStamp;
	int64						NotificationCycles;
	int64						MaxQuotaLimitCycles;
	volatile int64				MaxQuotaCyclesRemaining;
	union {
		_LIST_ENTRY					SchedulingGroupList;
		_LIST_ENTRY					Sibling;
	};
	_KDPC						*NotificationDpc;
	_LIST_ENTRY					ChildList;
	_KSCHEDULING_GROUP			*Parent;
	char						_pdb_padding0[24];
	_KSCB						PerProcessor[1];
};

// TI = 0x1a35
union _KSTACK_COUNT {
	int		Value;
	struct {
		uint32	State:3;
		uint32	StackCount:29;
	};
};

// TI = 0x1976
union _KEXECUTE_OPTIONS {
	struct {
		uint8			ExecuteDisable:1;
		uint8			ExecuteEnable:1;
		uint8			DisableThunkEmulation:1;
		uint8			Permanent:1;
		uint8			ExecuteDispatchEnable:1;
		uint8			ImageDispatchEnable:1;
		uint8			DisableExceptionChainValidation:1;
		uint8			Spare:1;
	};
	volatile uint8	ExecuteOptions;
	uint8			ExecuteOptionsNV;
};

// TI = 0x124c
struct _KPROCESS {
	_DISPATCHER_HEADER		Header;
	_LIST_ENTRY				ProfileListHead;
	uint64					DirectoryTableBase;
	_LIST_ENTRY				ThreadListHead;
	uint32					ProcessLock;
	uint32					ProcessTimerDelay;
	uint64					DeepFreezeStartTime;
	_KAFFINITY_EX			Affinity;
	_LIST_ENTRY				ReadyListHead;
	_SINGLE_LIST_ENTRY		SwapListEntry;
	volatile _KAFFINITY_EX	ActiveProcessors;
	union {
		struct {
			uint32					AutoAlignment:1;
			uint32					DisableBoost:1;
			uint32					DisableQuantum:1;
			uint32					DeepFreeze:1;
			uint32					TimerVirtualization:1;
			uint32					CheckStackExtents:1;
			uint32					CacheIsolationEnabled:1;
			uint32					PpmPolicy:3;
			uint32					ActiveGroupsMask:20;
			uint32					VaSpaceDeleted:1;
			uint32					ReservedFlags:1;
		};
		volatile int			ProcessFlags;
	};
	char					BasePriority;
	char					QuantumReset;
	char					Visited;
	_KEXECUTE_OPTIONS		Flags;
	uint32					ThreadSeed[20];
	uint16					IdealNode[20];
	uint16					IdealGlobalNode;
	uint16					Spare1;
	volatile _KSTACK_COUNT	StackCount;
	_LIST_ENTRY				ProcessListEntry;
	uint64					CycleTime;
	uint64					ContextSwitches;
	_KSCHEDULING_GROUP		*SchedulingGroup;
	uint32					FreezeCount;
	uint32					KernelTime;
	uint32					UserTime;
	uint32					ReadyTime;
	uint64					UserDirectoryTableBase;
	uint8					AddressPolicy;
	uint8					Spare2[71];
	void					*InstrumentationCallback;
	union  {
		uint64	SecureHandle;
		struct {
			uint64	SecureProcess:1;
			uint64	Unused:1;
		}		Flags;
	}						SecureState;
};

// TI = 0x1a3a
struct _KEXCEPTION_FRAME {
	uint64	P1Home;
	uint64	P2Home;
	uint64	P3Home;
	uint64	P4Home;
	uint64	P5;
	uint64	Spare1;
	_M128A	Xmm6;
	_M128A	Xmm7;
	_M128A	Xmm8;
	_M128A	Xmm9;
	_M128A	Xmm10;
	_M128A	Xmm11;
	_M128A	Xmm12;
	_M128A	Xmm13;
	_M128A	Xmm14;
	_M128A	Xmm15;
	uint64	TrapFrame;
	uint64	OutputBuffer;
	uint64	OutputLength;
	uint64	Spare2;
	uint64	MxCsr;
	uint64	Rbp;
	uint64	Rbx;
	uint64	Rdi;
	uint64	Rsi;
	uint64	R12;
	uint64	R13;
	uint64	R14;
	uint64	R15;
	uint64	Return;
};

// TI = 0x1a64
struct _KTRAP_FRAME {
	uint64	P1Home;
	uint64	P2Home;
	uint64	P3Home;
	uint64	P4Home;
	uint64	P5;
	union {
		char	PreviousMode;
		uint8	InterruptRetpolineState;
	};
	uint8	PreviousIrql;
	union {
		uint8	FaultIndicator;
		uint8	NmiMsrIbrs;
	};
	uint8	ExceptionActive;
	uint32	MxCsr;
	uint64	Rax;
	uint64	Rcx;
	uint64	Rdx;
	uint64	R8;
	uint64	R9;
	uint64	R10;
	uint64	R11;
	union {
		uint64	GsBase;
		uint64	GsSwap;
	};
	_M128A	Xmm0;
	_M128A	Xmm1;
	_M128A	Xmm2;
	_M128A	Xmm3;
	_M128A	Xmm4;
	_M128A	Xmm5;
	union {
		uint64	FaultAddress;
		uint64	ContextRecord;
	};
	uint64	Dr0;
	uint64	Dr1;
	uint64	Dr2;
	uint64	Dr3;
	uint64	Dr6;
	uint64	Dr7;
	uint64	DebugControl;
	uint64	LastBranchToRip;
	uint64	LastBranchFromRip;
	uint64	LastExceptionToRip;
	uint64	LastExceptionFromRip;
	uint16	SegDs;
	uint16	SegEs;
	uint16	SegFs;
	uint16	SegGs;
	uint64	TrapFrame;
	uint64	Rbx;
	uint64	Rdi;
	uint64	Rsi;
	uint64	Rbp;
	union {
		uint64	ErrorCode;
		uint64	ExceptionFrame;
	};
	uint64	Rip;
	uint16	SegCs;
	uint8	Fill0;
	uint8	Logging;
	uint16	Fill1[2];
	uint32	EFlags;
	uint32	Fill2;
	uint64	Rsp;
	uint16	SegSs;
	uint16	Fill3;
	uint32	Fill4;
};

// TI = 0x196b
struct _KUMS_CONTEXT_HEADER {
	uint64				P1Home;
	uint64				P2Home;
	uint64				P3Home;
	uint64				P4Home;
	void				*StackTop;
	uint64				StackSize;
	uint64				RspOffset;
	uint64				Rip;
	_XSAVE_FORMAT		*FltSave;
	union {
		struct {
			uint64				Volatile:1;
			uint64				Reserved:63;
		};
		uint64				Flags;
	};
	_KTRAP_FRAME		*TrapFrame;
	_KEXCEPTION_FRAME	*ExceptionFrame;
	_KTHREAD			*SourceThread;
	uint64				Return;
};

// TI = 0x1ce4
struct _KQUEUE {
	_DISPATCHER_HEADER	Header;
	_LIST_ENTRY			EntryListHead;
	volatile uint32		CurrentCount;
	uint32				MaximumCount;
	_LIST_ENTRY			ThreadListHead;
};

// TI = 0x1be4 - forward reference
struct _RTL_UMS_CONTEXT;

// TI = 0x1d2b
struct _RTL_UMS_CONTEXT {
	_SINGLE_LIST_ENTRY	Link;
	char				_pdb_padding0[8];
	_CONTEXT			Context;
	void				*Teb;
	void				*UserContext;
	union {
		struct {
			volatile uint32		ScheduledThread:1;
			volatile uint32		Suspended:1;
			volatile uint32		VolatileContext:1;
			volatile uint32		Terminated:1;
			volatile uint32		DebugActive:1;
			volatile uint32		RunningOnSelfThread:1;
			volatile uint32		DenyRunningOnSelfThread:1;
		};
		volatile int		Flags;
	};
	union {
		struct {
			volatile uint64		KernelUpdateLock:2;
			volatile uint64		PrimaryClientID:62;
		};
		volatile uint64		ContextLock;
	};
	_RTL_UMS_CONTEXT	*PrimaryUmsContext;
	uint32				SwitchCount;
	uint32				KernelYieldCount;
	uint32				MixedYieldCount;
	uint32				YieldCount;
};

// TI = 0x1be7
struct _UMS_CONTROL_BLOCK {
	_RTL_UMS_CONTEXT	*UmsContext;
	_SINGLE_LIST_ENTRY	*CompletionListEntry;
	_KEVENT				*CompletionListEvent;
	uint32				ServiceSequenceNumber;
	union {
		struct {
			_KQUEUE				UmsQueue;
			_LIST_ENTRY			QueueEntry;
			_RTL_UMS_CONTEXT	*YieldingUmsContext;
			void				*YieldingParam;
			void				*UmsTeb;
		};
		struct {
			_KQUEUE				*UmsAssociatedQueue;
			_LIST_ENTRY			*UmsQueueListEntry;
			_KEVENT				UmsWaitEvent;
			void				*StagingArea;
			union {
				struct {
					uint32				UmsPrimaryDeliveredContext:1;
					uint32				UmsAssociatedQueueUsed:1;
					uint32				UmsThreadParked:1;
				};
				uint32				UmsFlags;
			};
		};
	};
};

// TI = 0x1dce
struct _XSTATE_CONTEXT {
	uint64		Mask;
	uint32		Length;
	uint32		Reserved1;
	_XSAVE_AREA	*Area;
	void		*Buffer;
};

// TI = 0x1271 - forward reference
struct _XSTATE_SAVE;

// TI = 0x1ce2
struct _XSTATE_SAVE {
	_XSTATE_SAVE	*Prev;
	_KTHREAD		*Thread;
	uint8			Level;
	_XSTATE_CONTEXT	XStateContext;
};

// TI = 0x195d
enum _HARDWARE_COUNTER_TYPE {
	PMCCounter				= 0,
	MaxHardwareCounterType	= 1,
};

// TI = 0x195f
struct _COUNTER_READING {
	_HARDWARE_COUNTER_TYPE	Type;
	uint32					Index;
	uint64					Start;
	uint64					Total;
};

// TI = 0x1b5c
struct _THREAD_PERFORMANCE_DATA {
	uint16				Size;
	uint16				Version;
	_PROCESSOR_NUMBER	ProcessorNumber;
	uint32				ContextSwitches;
	uint32				HwCountersCount;
	volatile uint64		UpdateCount;
	uint64				WaitReasonBitMap;
	uint64				HardwareCounters;
	_COUNTER_READING	CycleTime;
	_COUNTER_READING	HwCounters[16];
};

// TI = 0x18e1
struct _KTHREAD_COUNTERS {
	uint64						WaitReasonBitMap;
	_THREAD_PERFORMANCE_DATA	*UserData;
	uint32						Flags;
	uint32						ContextSwitches;
	uint64						CycleTimeBias;
	uint64						HardwareCounters;
	_COUNTER_READING			HwCounter[16];
};

// TI = 0x1aae
struct _KWAIT_BLOCK {
	_LIST_ENTRY		WaitListEntry;
	uint8			WaitType;
	volatile uint8	BlockState;
	uint16			WaitKey;
	int				SpareLong;
	union {
		_KTHREAD		*Thread;
		_KQUEUE			*NotificationQueue;
	};
	void			*Object;
	void			*SparePtr;
};

// TI = 0x1a78
struct _KAPC_STATE {
	_LIST_ENTRY	ApcListHead[2];
	_KPROCESS	*Process;
	union {
		uint8		InProgressFlags;
		struct {
			uint8		KernelApcInProgress:1;
			uint8		SpecialApcInProgress:1;
		};
	};
	uint8		KernelApcPending;
	union {
		uint8		UserApcPendingAll;
		struct {
			uint8		SpecialUserApcPending:1;
			uint8		UserApcPending:1;
		};
	};
};

// TI = 0x1af8
union _KWAIT_STATUS_REGISTER {
	uint8	Flags;
	struct {
		uint8	State:3;
		uint8	Affinity:1;
		uint8	Priority:1;
		uint8	Apc:1;
		uint8	UserApc:1;
		uint8	Alert:1;
	};
};

// TI = 0x1289
struct _KTHREAD {
	_DISPATCHER_HEADER		Header;
	void					*SListFaultAddress;
	uint64					QuantumTarget;
	void					*InitialStack;
	void					volatile *StackLimit;
	void					*StackBase;
	uint64					ThreadLock;
	volatile uint64			CycleTime;
	uint32					CurrentRunTime;
	uint32					ExpectedRunTime;
	void					*KernelStack;
	_XSAVE_FORMAT			*StateSaveArea;
	_KSCHEDULING_GROUP		volatile *SchedulingGroup;
	_KWAIT_STATUS_REGISTER	WaitRegister;
	volatile uint8			Running;
	uint8					Alerted[2];
	union {
		struct {
			uint32					AutoBoostActive:1;
			uint32					ReadyTransition:1;
			uint32					WaitNext:1;
			uint32					SystemAffinityActive:1;
			uint32					Alertable:1;
			uint32					UserStackWalkActive:1;
			uint32					ApcInterruptRequest:1;
			uint32					QuantumEndMigrate:1;
			uint32					UmsDirectedSwitchEnable:1;
			uint32					TimerActive:1;
			uint32					SystemThread:1;
			uint32					ProcessDetachActive:1;
			uint32					CalloutActive:1;
			uint32					ScbReadyQueue:1;
			uint32					ApcQueueable:1;
			uint32					ReservedStackInUse:1;
			uint32					UmsPerformingSyscall:1;
			uint32					TimerSuspended:1;
			uint32					SuspendedWaitMode:1;
			uint32					SuspendSchedulerApcWait:1;
			uint32					CetShadowStack:1;
			uint32					Reserved:11;
		};
		int						MiscFlags;
	};
	union {
		struct {
			uint32					BamQosLevel:2;
			uint32					AutoAlignment:1;
			uint32					DisableBoost:1;
			uint32					AlertedByThreadId:1;
			uint32					QuantumDonation:1;
			uint32					EnableStackSwap:1;
			uint32					GuiThread:1;
			uint32					DisableQuantum:1;
			uint32					ChargeOnlySchedulingGroup:1;
			uint32					DeferPreemption:1;
			uint32					QueueDeferPreemption:1;
			uint32					ForceDeferSchedule:1;
			uint32					SharedReadyQueueAffinity:1;
			uint32					FreezeCount:1;
			uint32					TerminationApcRequest:1;
			uint32					AutoBoostEntriesExhausted:1;
			uint32					KernelStackResident:1;
			uint32					TerminateRequestReason:2;
			uint32					ProcessStackCountDecremented:1;
			uint32					RestrictedGuiThread:1;
			uint32					VpBackingThread:1;
			uint32					ThreadFlagsSpare:1;
			uint32					EtwStackTraceApcInserted:8;
		};
		volatile int			ThreadFlags;
	};
	volatile uint8			Tag;
	uint8					SystemHeteroCpuPolicy;
	uint8					UserHeteroCpuPolicy:7;
	uint8					ExplicitSystemHeteroCpuPolicy:1;
	uint8					Spare0;
	uint32					SystemCallNumber;
	uint32					ReadyTime;
	void					*FirstArgument;
	_KTRAP_FRAME			*TrapFrame;
	union {
		_KAPC_STATE				ApcState;
		struct {
			uint8					ApcStateFill[43];
			char					Priority;
			uint32					UserIdealProcessor;
		};
	};
	volatile int64			WaitStatus;
	_KWAIT_BLOCK			*WaitBlockList;
	union {
		_LIST_ENTRY				WaitListEntry;
		_SINGLE_LIST_ENTRY		SwapListEntry;
	};
	_DISPATCHER_HEADER		volatile *Queue;
	void					*Teb;
	uint64					RelativeTimerBias;
	_KTIMER					Timer;
	union {
		_KWAIT_BLOCK			WaitBlock[4];
		struct {
			uint8					WaitBlockFill4[20];
			uint32					ContextSwitches;
		};
		struct {
			uint8					WaitBlockFill5[68];
			volatile uint8			State;
			char					Spare13;
			uint8					WaitIrql;
			char					WaitMode;
		};
		struct {
			uint8					WaitBlockFill6[116];
			uint32					WaitTime;
		};
		struct {
			uint8					WaitBlockFill7[164];
			union {
				struct {
					int16					KernelApcDisable;
					int16					SpecialApcDisable;
				};
				uint32					CombinedApcDisable;
			};
		};
		struct {
			uint8					WaitBlockFill8[40];
			_KTHREAD_COUNTERS		*ThreadCounters;
		};
		struct {
			uint8					WaitBlockFill9[88];
			_XSTATE_SAVE			*XStateSave;
		};
		struct {
			uint8					WaitBlockFill10[136];
			void					volatile *Win32Thread;
		};
		struct {
			uint8					WaitBlockFill11[176];
			_UMS_CONTROL_BLOCK		*Ucb;
			_KUMS_CONTEXT_HEADER	volatile *Uch;
		};
	};
	void					*Spare21;
	_LIST_ENTRY				QueueListEntry;
	union {
		volatile uint32			NextProcessor;
		struct {
			uint32					NextProcessorNumber:31;
			uint32					SharedReadyQueue:1;
		};
	};
	int						QueuePriority;
	_KPROCESS				*Process;
	union {
		_GROUP_AFFINITY			UserAffinity;
		struct {
			uint8					UserAffinityFill[10];
			char					PreviousMode;
			char					BasePriority;
			union {
				char					PriorityDecrement;
				struct {
					uint8					ForegroundBoost:4;
					uint8					UnusualBoost:4;
				};
			};
			uint8					Preempted;
			uint8					AdjustReason;
			char					AdjustIncrement;
		};
	};
	uint64					AffinityVersion;
	union {
		_GROUP_AFFINITY			Affinity;
		struct {
			uint8					AffinityFill[10];
			uint8					ApcStateIndex;
			uint8					WaitBlockCount;
			uint32					IdealProcessor;
		};
	};
	uint64					NpxState;
	union {
		_KAPC_STATE				SavedApcState;
		struct {
			uint8					SavedApcStateFill[43];
			uint8					WaitReason;
			char					SuspendCount;
			char					Saturation;
			uint16					SListFaultCount;
		};
	};
	union {
		_KAPC					SchedulerApc;
		struct {
			uint8					SchedulerApcFill0[1];
			uint8					ResourceIndex;
		};
		struct {
			uint8					SchedulerApcFill1[3];
			uint8					QuantumReset;
		};
		struct {
			uint8					SchedulerApcFill2[4];
			uint32					KernelTime;
		};
		struct {
			uint8					SchedulerApcFill3[64];
			_KPRCB					volatile *WaitPrcb;
		};
		struct {
			uint8					SchedulerApcFill4[72];
			void					*LegoData;
		};
		struct {
			uint8					SchedulerApcFill5[83];
			uint8					CallbackNestingLevel;
			uint32					UserTime;
		};
	};
	_KEVENT					SuspendEvent;
	_LIST_ENTRY				ThreadListEntry;
	_LIST_ENTRY				MutantListHead;
	uint8					AbEntrySummary;
	uint8					AbWaitEntryCount;
	uint8					AbAllocationRegionCount;
	char					SystemPriority;
	uint32					SecureThreadCookie;
	_KLOCK_ENTRY			LockEntries[6];
	_SINGLE_LIST_ENTRY		PropagateBoostsEntry;
	_SINGLE_LIST_ENTRY		IoSelfBoostsEntry;
	uint8					PriorityFloorCounts[16];
	uint32					PriorityFloorSummary;
	volatile int			AbCompletedIoBoostCount;
	volatile int			AbCompletedIoQoSBoostCount;
	volatile int16			KeReferenceCount;
	uint8					AbOrphanedEntrySummary;
	uint8					AbOwnedEntryCount;
	uint32					ForegroundLossTime;
	union {
		_LIST_ENTRY				GlobalForegroundListEntry;
		struct {
			_SINGLE_LIST_ENTRY		ForegroundDpcStackListEntry;
			uint64					InGlobalForegroundList;
		};
	};
	int64					ReadOperationCount;
	int64					WriteOperationCount;
	int64					OtherOperationCount;
	int64					ReadTransferCount;
	int64					WriteTransferCount;
	int64					OtherTransferCount;
	_KSCB					*QueuedScb;
	volatile uint32			ThreadTimerDelay;
	union {
		volatile int			ThreadFlags2;
		struct {
			uint32					PpmPolicy:2;
			uint32					ThreadFlags2Reserved:30;
		};
	};
	void					*SchedulerAssist;
};

// TI = 0x1a6a
union _KIDTENTRY64 {
	struct {
		uint16	OffsetLow;
		uint16	Selector;
		uint16	IstIndex:3;
		uint16	Reserved0:5;
		uint16	Type:5;
		uint16	Dpl:2;
		uint16	Present:1;
		uint16	OffsetMiddle;
		uint32	OffsetHigh;
		uint32	Reserved1;
	};
	uint64	Alignment;
};

// TI = 0x1188 - forward reference
struct _KPCR;

// TI = 0x1aa4
struct _KTSS64 {
	uint32	Reserved0;
	uint64	Rsp0;
	uint64	Rsp1;
	uint64	Rsp2;
	uint64	Ist[8];
	uint64	Reserved1;
	uint16	Reserved2;
	uint16	IoMapBase;
};

// TI = 0x1a32
union _KGDTENTRY64 {
	struct {
		uint16	LimitLow;
		uint16	BaseLow;
		union {
			struct {
				uint8	BaseMiddle;
				uint8	Flags1;
				uint8	Flags2;
				uint8	BaseHigh;
			}		Bytes;
			struct {
				uint32	BaseMiddle:8;
				uint32	Type:5;
				uint32	Dpl:2;
				uint32	Present:1;
				uint32	LimitHigh:4;
				uint32	System:1;
				uint32	LongMode:1;
				uint32	DefaultBig:1;
				uint32	Granularity:1;
				uint32	BaseHigh:8;
			}		Bits;
		};
		uint32	BaseUpper;
		uint32	MustBeZero;
	};
	struct {
		int64	DataLow;
		int64	DataHigh;
	};
};

// TI = 0x1199
struct _KPCR {
	union {
		_NT_TIB				NtTib;
		struct {
			_KGDTENTRY64		*GdtBase;
			_KTSS64				*TssBase;
			uint64				UserRsp;
			_KPCR				*Self;
			_KPRCB				*CurrentPrcb;
			_KSPIN_LOCK_QUEUE	*LockArray;
			void				*Used_Self;
		};
	};
	_KIDTENTRY64		*IdtBase;
	uint64				Unused[2];
	uint8				Irql;
	uint8				SecondLevelCacheAssociativity;
	uint8				ObsoleteNumber;
	uint8				Fill0;
	uint32				Unused0[3];
	uint16				MajorVersion;
	uint16				MinorVersion;
	uint32				StallScaleFactor;
	void				*Unused1[3];
	uint32				KernelReserved[15];
	uint32				SecondLevelCacheSize;
	uint32				HalReserved[16];
	uint32				Unused2;
	void				*KdVersionBlock;
	void				*Unused3;
	uint32				PcrAlign1[24];
	char				_pdb_padding0[8];
	_KPRCB				Prcb;
};

// TI = 0x11ee
struct _KFLOATING_SAVE {
	uint32	Dummy;
};

// TI = 0x11fc
union _INVPCID_DESCRIPTOR {
	struct {
		union {
			struct {
				uint64	Pcid:12;
				uint64	Reserved:52;
			};
			uint64	EntirePcid;
		};
		uint64	Virtual;
	}		IndividualAddress;
	struct {
		union {
			struct {
				uint64	Pcid:12;
				uint64	Reserved:52;
			};
			uint64	EntirePcid;
		};
		uint64	Reserved2;
	}		SingleContext;
	struct {
		uint64	Reserved[2];
	}		AllContextAndGlobals;
	struct {
		uint64	Reserved[2];
	}		AllContext;
};

// TI = 0x120e
struct _SINGLE_LIST_ENTRY32 {
	uint32	Next;
};

// TI = 0x1214
struct _EXT_SET_PARAMETERS_V0 {
	uint32	Version;
	uint32	Reserved;
	int64	NoWakeTolerance;
};

// TI = 0x199c
union _PS_TRUSTLET_ATTRIBUTE_ACCESSRIGHTS {
	struct {
		uint8	Trustlet:1;
		uint8	Ntos:1;
		uint8	WriteHandle:1;
		uint8	ReadHandle:1;
		uint8	Reserved:4;
	};
	uint8	AccessRights;
};

// TI = 0x19fd
struct _PS_TRUSTLET_ATTRIBUTE_TYPE {
	union {
		struct {
			uint8								Version;
			uint8								DataCount;
			uint8								SemanticType;
			_PS_TRUSTLET_ATTRIBUTE_ACCESSRIGHTS	AccessRights;
		};
		uint32								AttributeType;
	};
};

// TI = 0x1229
struct _PS_TRUSTLET_ATTRIBUTE_HEADER {
	_PS_TRUSTLET_ATTRIBUTE_TYPE	AttributeType;
	uint32						InstanceNumber:8;
	uint32						Reserved:24;
};

// TI = 0x1221
struct _PS_TRUSTLET_ATTRIBUTE_DATA {
	_PS_TRUSTLET_ATTRIBUTE_HEADER	Header;
	uint64							Data[1];
};

// TI = 0x121b
struct _PS_TRUSTLET_CREATE_ATTRIBUTES {
	uint64						TrustletIdentity;
	_PS_TRUSTLET_ATTRIBUTE_DATA	Attributes[1];
};

// TI = 0x122e
struct _TRUSTLET_MAILBOX_KEY {
	uint64	SecretValue[2];
};

// TI = 0x1234
struct _TRUSTLET_COLLABORATION_ID {
	uint64	Value[2];
};

// TI = 0x1b7d
struct _KERNEL_STACK_SEGMENT {
	uint64	StackBase;
	uint64	StackLimit;
	uint64	KernelStack;
	uint64	InitialStack;
};

// TI = 0x128e
struct _KSTACK_CONTROL {
	uint64					StackBase;
	union {
		uint64					ActualLimit;
		uint64					StackExpansion:1;
	};
	_KERNEL_STACK_SEGMENT	Previous;
};

// TI = 0x12a9
struct _FAST_MUTEX {
	int		Count;
	void	*Owner;
	uint32	Contention;
	_KEVENT	Event;
	uint32	OldIrql;
};

// TI = 0x12ad
enum _EVENT_TYPE {
	NotificationEvent		= 0,
	SynchronizationEvent	= 1,
};

// TI = 0x12c4 - forward reference
struct _SLIST_ENTRY;

// TI = 0x12c7
struct _SLIST_ENTRY {
	_SLIST_ENTRY	*Next;
};

// TI = 0x12e0
struct _NPAGED_LOOKASIDE_LIST {
	_GENERAL_LOOKASIDE	L;
};

// TI = 0x12eb
struct _PAGED_LOOKASIDE_LIST {
	_GENERAL_LOOKASIDE	L;
};

// TI = 0x1318
struct _IO_STATUS_BLOCK {
	union {
		int		Status;
		void	*Pointer;
	};
	uint64	Information;
};

// TI = 0x132e
struct _QUAD {
	union {
		int64	UseThisFieldToCopy;
		double	DoNotUseThisField;
	};
};

// TI = 0x1338
struct _WORK_QUEUE_ITEM {
	_LIST_ENTRY	List;
	void (*WorkerRoutine)(void*);
	void		*Parameter;
};

// TI = 0x1342
struct _EXT_DELETE_PARAMETERS {
	uint32	Version;
	uint32	Reserved;
	void (*DeleteCallback)(void*);
	void	*DeleteContext;
};

// TI = 0x1348
struct _EX_PUSH_LOCK {
	union {
		struct {
			uint64	Locked:1;
			uint64	Waiting:1;
			uint64	Waking:1;
			uint64	MultipleShared:1;
			uint64	Shared:60;
		};
		uint64	Value;
		void	*Ptr;
	};
};

// TI = 0x134f
enum _PP_NPAGED_LOOKASIDE_NUMBER {
	LookasideSmallIrpList		= 0,
	LookasideMediumIrpList		= 1,
	LookasideLargeIrpList		= 2,
	LookasideMdlList			= 3,
	LookasideCreateInfoList		= 4,
	LookasideNameBufferList		= 5,
	LookasideTwilightList		= 6,
	LookasideCompletionList		= 7,
	LookasideScratchBufferList	= 8,
	LookasideMaximumList		= 9,
};

// TI = 0x135f
struct _ENODE {
	_KNODE				Ncb;
	_WORK_QUEUE_ITEM	HotAddProcessorWorkItem;
};

// TI = 0x1bb6
struct _HANDLE_TRACE_DB_ENTRY {
	_CLIENT_ID	ClientId;
	void		*Handle;
	uint32		Type;
	void		*StackTrace[16];
};

// TI = 0x1996
struct _HANDLE_TRACE_DEBUG_INFO {
	int						RefCount;
	uint32					TableSize;
	uint32					BitMaskFlags;
	_FAST_MUTEX				CloseCompactionLock;
	uint32					CurrentStackIndex;
	_HANDLE_TRACE_DB_ENTRY	TraceDb[1];
};

// TI = 0x195b
struct _EXHANDLE {
	union {
		struct {
			uint32	TagBits:2;
			uint32	Index:30;
		};
		void	*GenericHandleOverlay;
		uint64	Value;
	};
};

// TI = 0x1372 - forward reference
union _HANDLE_TABLE_ENTRY;

// TI = 0x136e
struct _HANDLE_TABLE_ENTRY_INFO {
	uint32	AuditMask;
	uint32	MaxRelativeAccessMask;
};

// TI = 0x137d
union _HANDLE_TABLE_ENTRY {
	volatile int64				VolatileLowValue;
	int64						LowValue;
	struct {
		_HANDLE_TABLE_ENTRY_INFO	volatile *InfoTable;
		union {
			int64						HighValue;
			_HANDLE_TABLE_ENTRY			*NextFreeHandleEntry;
			_EXHANDLE					LeafHandleValue;
		};
	};
	int64						RefCountField;
	struct {
		uint64						Unlocked:1;
		uint64						RefCnt:16;
		uint64						Attributes:3;
		uint64						ObjectPointerBits:44;
		uint32						GrantedAccessBits:25;
		uint32						NoRightsUpgrade:1;
		uint32						Spare1:6;
		uint32						Spare2;
	};
};

// TI = 0x18cf
struct _HANDLE_TABLE_FREE_LIST {
	_EX_PUSH_LOCK		FreeListLock;
	_HANDLE_TABLE_ENTRY	*FirstFreeHandleEntry;
	_HANDLE_TABLE_ENTRY	*LastFreeHandleEntry;
	int					HandleCount;
	uint32				HighWaterMark;
};

// TI = 0x19b6
struct _JOBOBJECT_WAKE_FILTER {
	uint32	HighEdgeFilter;
	uint32	LowEdgeFilter;
};

// TI = 0x19a6
struct _PS_PROCESS_WAKE_INFORMATION {
	uint64					NotificationChannel;
	uint32					WakeCounters[7];
	_JOBOBJECT_WAKE_FILTER	WakeFilter;
	uint32					NoWakeCounter;
};

// TI = 0x1c1e
struct _WNF_STATE_NAME {
	uint32	Data[2];
};

// TI = 0x1ad4
union _PS_INTERLOCKED_TIMER_DELAY_VALUES {
	struct {
		uint64	DelayMs:30;
		uint64	CoalescingWindowMs:30;
		uint64	Reserved:1;
		uint64	NewTimerWheel:1;
		uint64	Retry:1;
		uint64	Locked:1;
	};
	uint64	All;
};

// TI = 0x13f1
struct _PO_PROCESS_ENERGY_CONTEXT {};

// TI = 0x18f4
struct _PROCESS_DISK_COUNTERS {
	uint64	BytesRead;
	uint64	BytesWritten;
	uint64	ReadOperationCount;
	uint64	WriteOperationCount;
	uint64	FlushOperationCount;
};

// TI = 0x19e3
struct _PS_PROTECTION {
	union {
		uint8	Level;
		struct {
			uint8	Type:3;
			uint8	Audit:1;
			uint8	Signer:4;
		};
	};
};

// TI = 0x1915
union _JOBOBJECT_ENERGY_TRACKING_STATE {
	uint64	Value;
	struct {
		uint32	UpdateMask;
		uint32	DesiredState;
	};
};

// TI = 0x1530
struct _EX_RUNDOWN_REF {
	union {
		uint64	Count;
		void	*Ptr;
	};
};

// TI = 0x1a04
struct _PS_IO_CONTROL_ENTRY {
	union {
		_RTL_BALANCED_NODE	VolumeTreeNode;
		struct {
			_LIST_ENTRY			FreeListEntry;
			uint64				ReservedForParentValue;
		};
	};
	uint64				VolumeKey;
	_EX_RUNDOWN_REF		Rundown;
	void				*IoControl;
	void				*VolumeIoAttribution;
};

// TI = 0x1939
struct _JOB_RATE_CONTROL_HEADER {
	void		*RateControlQuotaReference;
	_RTL_BITMAP	OverQuotaHistory;
	uint8		*BitMapBuffer;
	uint64		BitMapBufferSize;
};

// TI = 0x1aa0
union _ENERGY_STATE_DURATION {
	uint64	Value;
	struct {
		uint32	LastChangeTime;
		uint32	Duration:31;
		uint32	IsInState:1;
	};
};

// TI = 0x1b8c
union _TIMELINE_BITMAP {
	uint64	Value;
	struct {
		uint32	EndTime;
		uint32	Bitmap;
	};
};

// TI = 0x1b1c
struct _PROCESS_ENERGY_VALUES_EXTENSION {
	union {
		_TIMELINE_BITMAP		Timelines[14];
		struct {
			_TIMELINE_BITMAP		CpuTimeline;
			_TIMELINE_BITMAP		DiskTimeline;
			_TIMELINE_BITMAP		NetworkTimeline;
			_TIMELINE_BITMAP		MBBTimeline;
			_TIMELINE_BITMAP		ForegroundTimeline;
			_TIMELINE_BITMAP		DesktopVisibleTimeline;
			_TIMELINE_BITMAP		CompositionRenderedTimeline;
			_TIMELINE_BITMAP		CompositionDirtyGeneratedTimeline;
			_TIMELINE_BITMAP		CompositionDirtyPropagatedTimeline;
			_TIMELINE_BITMAP		InputTimeline;
			_TIMELINE_BITMAP		AudioInTimeline;
			_TIMELINE_BITMAP		AudioOutTimeline;
			_TIMELINE_BITMAP		DisplayRequiredTimeline;
			_TIMELINE_BITMAP		KeyboardInputTimeline;
		};
	};
	union {
		_ENERGY_STATE_DURATION	Durations[5];
		struct {
			_ENERGY_STATE_DURATION	InputDuration;
			_ENERGY_STATE_DURATION	AudioInDuration;
			_ENERGY_STATE_DURATION	AudioOutDuration;
			_ENERGY_STATE_DURATION	DisplayRequiredDuration;
			_ENERGY_STATE_DURATION	PSMBackgroundDuration;
		};
	};
	uint32					KeyboardInput;
	uint32					MouseInput;
};

// TI = 0x19aa
struct _PROCESS_ENERGY_VALUES {
	uint64					Cycles[4][2];
	uint64					DiskEnergy;
	uint64					NetworkTailEnergy;
	uint64					MBBTailEnergy;
	uint64					NetworkTxRxBytes;
	uint64					MBBTxRxBytes;
	union {
		_ENERGY_STATE_DURATION	Durations[3];
		struct {
			_ENERGY_STATE_DURATION	ForegroundDuration;
			_ENERGY_STATE_DURATION	DesktopVisibleDuration;
			_ENERGY_STATE_DURATION	PSMForegroundDuration;
		};
	};
	uint32					CompositionRendered;
	uint32					CompositionDirtyGenerated;
	uint32					CompositionDirtyPropagated;
	uint32					Reserved1;
	uint64					AttributedCycles[4][2];
	uint64					WorkOnBehalfCycles[4][2];
};

// TI = 0x156b
struct _PROCESS_EXTENDED_ENERGY_VALUES {
	_PROCESS_ENERGY_VALUES				Base;
	_PROCESS_ENERGY_VALUES_EXTENSION	Extension;
};

// TI = 0x14e9
struct _JOB_NET_RATE_CONTROL {};

// TI = 0x14e7
struct _PSP_STORAGE {};

// TI = 0x1949
struct _PS_PROPERTY_SET {
	_LIST_ENTRY	ListHead;
	uint64		Lock;
};

// TI = 0x1d18
struct _SILO_USER_SHARED_DATA {
	uint32				ServiceSessionId;
	uint32				ActiveConsoleId;
	int64				ConsoleSessionForegroundProcessId;
	_NT_PRODUCT_TYPE	NtProductType;
	uint32				SuiteMask;
	uint32				SharedUserSessionId;
	uint8				IsMultiSessionSku;
	char				_pdb_padding0[1];
	wchar_t				NtSystemRoot[520];
	uint16				UserModeGlobalLogger[16];
};

// TI = 0x1c18
enum _SERVERSILO_STATE {
	SERVERSILO_INITING			= 0,
	SERVERSILO_STARTED			= 1,
	SERVERSILO_SHUTTING_DOWN	= 2,
	SERVERSILO_TERMINATING		= 3,
	SERVERSILO_TERMINATED		= 4,
};

// TI = 0x1362 - forward reference
struct _EPROCESS;

// TI = 0x1d19
struct _DBGKP_ERROR_PORT {};

// TI = 0x1d1c
struct _DBGK_SILOSTATE {
	_EX_PUSH_LOCK				ErrorPortLock;
	struct _DBGKP_ERROR_PORT	*ErrorPort;
	_EPROCESS					*ErrorProcess;
	_KEVENT						*ErrorPortRegisteredEvent;
};

// TI = 0x1daf
struct _WNF_LOCK {
	_EX_PUSH_LOCK	PushLock;
};

// TI = 0x1d31
struct _WNF_SCOPE_MAP {};

// TI = 0x1d35
struct _WNF_SILODRIVERSTATE {
	struct _WNF_SCOPE_MAP	*ScopeMap;
	void					volatile *PermanentNameStoreRootKey;
	void					volatile *PersistentNameStoreRootKey;
	volatile int64			PermanentNameSequenceNumber;
	_WNF_LOCK				PermanentNameSequenceNumberLock;
	volatile int64			PermanentNameSequenceNumberPool;
	volatile int64			RuntimeNameSequenceNumber;
};

// TI = 0x1c13
struct _ETW_SILODRIVERSTATE {};

// TI = 0x1cdd
struct _SEP_RM_LSA_CONNECTION_STATE {
	void			*LsaProcessHandle;
	void			*LsaCommandPortHandle;
	void			*SepRmThreadHandle;
	void			*RmCommandPortHandle;
	void			*RmCommandServerPortHandle;
	void			*LsaCommandPortSectionHandle;
	_LARGE_INTEGER	LsaCommandPortSectionSize;
	void			*LsaViewPortMemory;
	void			*RmViewPortMemory;
	int				LsaCommandPortMemoryDelta;
	uint8			LsaCommandPortActive;
};

// TI = 0x1c3b
struct _CI_NGEN_PATHS {};

// TI = 0x1c39
struct _SEP_LOGON_SESSION_REFERENCES {};

// TI = 0x1c3e
struct _SEP_SILOSTATE {
	struct _SEP_LOGON_SESSION_REFERENCES	*SystemLogonSession;
	struct _SEP_LOGON_SESSION_REFERENCES	*AnonymousLogonSession;
	void									*AnonymousLogonToken;
	void									*AnonymousLogonTokenNoEveryone;
	_UNICODE_STRING							*UncSystemPaths;
	struct _CI_NGEN_PATHS					*NgenPaths;
};

// TI = 0x1e13
struct _OBJECT_NAMESPACE_LOOKUPTABLE {
	_LIST_ENTRY		HashBuckets[37];
	_EX_PUSH_LOCK	Lock;
	uint32			NumberOfPrivateSpaces;
};

// TI = 0x1da9
struct _OBP_SYSTEM_DOS_DEVICE_STATE {
	uint32	GlobalDeviceMap;
	uint32	LocalDeviceCount[26];
};

// TI = 0x13cd - forward reference
struct _EJOB;

// TI = 0x1dba - forward reference
struct _OBJECT_DIRECTORY;

// TI = 0x1d8e - forward reference
struct _DEVICE_MAP;

// TI = 0x1e0c - forward reference
struct _OBJECT_DIRECTORY_ENTRY;

// TI = 0x1e19
struct _OBJECT_DIRECTORY_ENTRY {
	_OBJECT_DIRECTORY_ENTRY	*ChainLink;
	void					*Object;
	uint32					HashValue;
};

// TI = 0x1e10
struct _OBJECT_DIRECTORY {
	_OBJECT_DIRECTORY_ENTRY	*HashBuckets[37];
	_EX_PUSH_LOCK			Lock;
	_DEVICE_MAP				*DeviceMap;
	_OBJECT_DIRECTORY		*ShadowDirectory;
	void					*NamespaceEntry;
	void					*SessionObject;
	uint32					Flags;
	uint32					SessionId;
};

// TI = 0x1dbd
struct _DEVICE_MAP {
	_OBJECT_DIRECTORY	*DosDevicesDirectory;
	_OBJECT_DIRECTORY	*GlobalDosDevicesDirectory;
	void				*DosDevicesDirectoryHandle;
	volatile int		ReferenceCount;
	uint32				DriveMap;
	uint8				DriveType[32];
	_EJOB				*ServerSilo;
};

// TI = 0x1d93
struct _OBP_SILODRIVERSTATE {
	_DEVICE_MAP						*SystemDeviceMap;
	_OBP_SYSTEM_DOS_DEVICE_STATE	SystemDosDeviceState;
	_EX_PUSH_LOCK					DeviceMapLock;
	_OBJECT_NAMESPACE_LOOKUPTABLE	PrivateNamespaceLookupTable;
};

// TI = 0x1c1c
struct _ESERVERSILO_GLOBALS {
	_OBP_SILODRIVERSTATE			ObSiloState;
	_SEP_SILOSTATE					SeSiloState;
	_SEP_RM_LSA_CONNECTION_STATE	SeRmSiloState;
	struct _ETW_SILODRIVERSTATE		*EtwSiloState;
	_EPROCESS						*MiSessionLeaderProcess;
	_EPROCESS						*ExpDefaultErrorPortProcess;
	void							*ExpDefaultErrorPort;
	uint32							HardErrorState;
	_WNF_SILODRIVERSTATE			WnfSiloState;
	_DBGK_SILOSTATE					DbgkSiloState;
	_UNICODE_STRING					PsProtectedCurrentDirectory;
	_UNICODE_STRING					PsProtectedEnvironment;
	void							*ApiSetSection;
	void							*ApiSetSchema;
	uint8							OneCoreForwardersEnabled;
	_UNICODE_STRING					NtSystemRoot;
	_UNICODE_STRING					SiloRootDirectoryName;
	struct _PSP_STORAGE				*Storage;
	_SERVERSILO_STATE				State;
	int								ExitStatus;
	_KEVENT							*DeleteEvent;
	_SILO_USER_SHARED_DATA			*UserSharedData;
	void							*UserSharedSection;
	_WORK_QUEUE_ITEM				TerminateWorkItem;
};

// TI = 0x191e
struct _EPROCESS_VALUES {
	uint64	KernelTime;
	uint64	UserTime;
	uint64	ReadyTime;
	uint64	CycleTime;
	uint64	ContextSwitches;
	int64	ReadOperationCount;
	int64	WriteOperationCount;
	int64	OtherOperationCount;
	int64	ReadTransferCount;
	int64	WriteTransferCount;
	int64	OtherTransferCount;
};

// TI = 0x14e2
struct _JOB_CPU_RATE_CONTROL {};

// TI = 0x14e0 - forward reference
struct _IO_MINI_COMPLETION_PACKET_USER;

// TI = 0x1cf6
struct _IO_MINI_COMPLETION_PACKET_USER {
	_LIST_ENTRY	ListEntry;
	uint32		PacketType;
	void		*KeyContext;
	void		*ApcContext;
	int			IoStatus;
	uint64		IoStatusInformation;
	void (*MiniPacketCallback)(_IO_MINI_COMPLETION_PACKET_USER *, void*);
	void		*Context;
	uint8		Allocated;
};

// TI = 0x14de
struct _JOB_NOTIFICATION_INFORMATION {};

// TI = 0x1b4c
struct _PS_JOB_WAKE_INFORMATION {
	uint64	NotificationChannel;
	uint64	WakeCounters[7];
	uint64	NoWakeCounter;
};

// TI = 0x14da
struct _JOB_ACCESS_STATE {};

// TI = 0x1b23
struct _OWNER_ENTRY {
	uint64	OwnerThread;
	union {
		struct {
			uint32	IoPriorityBoosted:1;
			uint32	OwnerReferenced:1;
			uint32	IoQoSPriorityBoosted:1;
			uint32	OwnerCount:29;
		};
		uint32	TableSize;
	};
};

// TI = 0x1a40
struct _ERESOURCE {
	_LIST_ENTRY		SystemResourcesList;
	_OWNER_ENTRY	*OwnerTable;
	int16			ActiveCount;
	union {
		uint16			Flag;
		struct {
			uint8			ReservedLowFlags;
			uint8			WaiterPriority;
		};
	};
	void			*SharedWaiters;
	void			*ExclusiveWaiters;
	_OWNER_ENTRY	OwnerEntry;
	uint32			ActiveEntries;
	uint32			ContentionCount;
	uint32			NumberOfSharedWaiters;
	uint32			NumberOfExclusiveWaiters;
	void			*Reserved2;
	union {
		void			*Address;
		uint64			CreatorBackTraceIndex;
	};
	uint64			SpinLock;
};

// TI = 0x14f1
struct _EJOB {
	_KEVENT									Event;
	_LIST_ENTRY								JobLinks;
	_LIST_ENTRY								ProcessListHead;
	_ERESOURCE								JobLock;
	_LARGE_INTEGER							TotalUserTime;
	_LARGE_INTEGER							TotalKernelTime;
	_LARGE_INTEGER							TotalCycleTime;
	_LARGE_INTEGER							ThisPeriodTotalUserTime;
	_LARGE_INTEGER							ThisPeriodTotalKernelTime;
	uint64									TotalContextSwitches;
	uint32									TotalPageFaultCount;
	uint32									TotalProcesses;
	uint32									ActiveProcesses;
	uint32									TotalTerminatedProcesses;
	_LARGE_INTEGER							PerProcessUserTimeLimit;
	_LARGE_INTEGER							PerJobUserTimeLimit;
	uint64									MinimumWorkingSetSize;
	uint64									MaximumWorkingSetSize;
	uint32									LimitFlags;
	uint32									ActiveProcessLimit;
	_KAFFINITY_EX							Affinity;
	struct _JOB_ACCESS_STATE				*AccessState;
	void									*AccessStateQuotaReference;
	uint32									UIRestrictionsClass;
	uint32									EndOfJobTimeAction;
	void									*CompletionPort;
	void									*CompletionKey;
	uint64									CompletionCount;
	uint32									SessionId;
	uint32									SchedulingClass;
	uint64									ReadOperationCount;
	uint64									WriteOperationCount;
	uint64									OtherOperationCount;
	uint64									ReadTransferCount;
	uint64									WriteTransferCount;
	uint64									OtherTransferCount;
	_PROCESS_DISK_COUNTERS					DiskIoInfo;
	uint64									ProcessMemoryLimit;
	uint64									JobMemoryLimit;
	uint64									JobTotalMemoryLimit;
	uint64									PeakProcessMemoryUsed;
	uint64									PeakJobMemoryUsed;
	_KAFFINITY_EX							EffectiveAffinity;
	_LARGE_INTEGER							EffectivePerProcessUserTimeLimit;
	uint64									EffectiveMinimumWorkingSetSize;
	uint64									EffectiveMaximumWorkingSetSize;
	uint64									EffectiveProcessMemoryLimit;
	_EJOB									*EffectiveProcessMemoryLimitJob;
	_EJOB									*EffectivePerProcessUserTimeLimitJob;
	_EJOB									*EffectiveNetIoRateLimitJob;
	_EJOB									*EffectiveHeapAttributionJob;
	uint32									EffectiveLimitFlags;
	uint32									EffectiveSchedulingClass;
	uint32									EffectiveFreezeCount;
	uint32									EffectiveBackgroundCount;
	uint32									EffectiveSwapCount;
	uint32									EffectiveNotificationLimitCount;
	uint8									EffectivePriorityClass;
	uint8									PriorityClass;
	uint8									NestingDepth;
	uint8									Reserved1[1];
	uint32									CompletionFilter;
	union {
		_WNF_STATE_NAME							WakeChannel;
		_PS_JOB_WAKE_INFORMATION				WakeInfo;
	};
	char									_pdb_padding0[64];
	_JOBOBJECT_WAKE_FILTER					WakeFilter;
	uint32									LowEdgeLatchFilter;
	_EJOB									*NotificationLink;
	uint64									CurrentJobMemoryUsed;
	struct _JOB_NOTIFICATION_INFORMATION	*NotificationInfo;
	void									*NotificationInfoQuotaReference;
	_IO_MINI_COMPLETION_PACKET_USER			*NotificationPacket;
	struct _JOB_CPU_RATE_CONTROL			*CpuRateControl;
	void									*EffectiveSchedulingGroup;
	uint64									ReadyTime;
	_EX_PUSH_LOCK							MemoryLimitsLock;
	_LIST_ENTRY								SiblingJobLinks;
	_LIST_ENTRY								ChildJobListHead;
	_EJOB									*ParentJob;
	_EJOB									volatile *RootJob;
	_LIST_ENTRY								IteratorListHead;
	uint64									AncestorCount;
	union {
		_EJOB									**Ancestors;
		void									*SessionObject;
	};
	_EPROCESS_VALUES						Accounting;
	uint32									ShadowActiveProcessCount;
	uint32									ActiveAuxiliaryProcessCount;
	uint32									SequenceNumber;
	uint32									JobId;
	_GUID									ContainerId;
	_GUID									ContainerTelemetryId;
	_ESERVERSILO_GLOBALS					*ServerSiloGlobals;
	_PS_PROPERTY_SET						PropertySet;
	struct _PSP_STORAGE						*Storage;
	struct _JOB_NET_RATE_CONTROL			*NetRateControl;
	union {
		uint32									JobFlags;
		struct {
			uint32									CloseDone:1;
			uint32									MultiGroup:1;
			uint32									OutstandingNotification:1;
			uint32									NotificationInProgress:1;
			uint32									UILimits:1;
			uint32									CpuRateControlActive:1;
			uint32									OwnCpuRateControl:1;
			uint32									Terminating:1;
			uint32									WorkingSetLock:1;
			uint32									JobFrozen:1;
			uint32									Background:1;
			uint32									WakeNotificationAllocated:1;
			uint32									WakeNotificationEnabled:1;
			uint32									WakeNotificationPending:1;
			uint32									LimitNotificationRequired:1;
			uint32									ZeroCountNotificationRequired:1;
			uint32									CycleTimeNotificationRequired:1;
			uint32									CycleTimeNotificationPending:1;
			uint32									TimersVirtualized:1;
			uint32									JobSwapped:1;
			uint32									ViolationDetected:1;
			uint32									EmptyJobNotified:1;
			uint32									NoSystemCharge:1;
			uint32									DropNoWakeCharges:1;
			uint32									NoWakeChargePolicyDecided:1;
			uint32									NetRateControlActive:1;
			uint32									OwnNetRateControl:1;
			uint32									IoRateControlActive:1;
			uint32									OwnIoRateControl:1;
			uint32									DisallowNewProcesses:1;
			uint32									Silo:1;
			uint32									ContainerTelemetryIdSet:1;
		};
	};
	union {
		uint32									JobFlags2;
		struct {
			uint32									ParentLocked:1;
			uint32									EnableUsermodeSiloThreadImpersonation:1;
			uint32									DisallowUsermodeSiloThreadImpersonation:1;
		};
	};
	_PROCESS_EXTENDED_ENERGY_VALUES			*EnergyValues;
	volatile uint64							SharedCommitCharge;
	uint32									DiskIoAttributionUserRefCount;
	uint32									DiskIoAttributionRefCount;
	union {
		void									*DiskIoAttributionContext;
		_EJOB									*DiskIoAttributionOwnerJob;
	};
	_JOB_RATE_CONTROL_HEADER				IoRateControlHeader;
	_PS_IO_CONTROL_ENTRY					GlobalIoControl;
	volatile int							IoControlStateLock;
	_RTL_RB_TREE							VolumeIoControlTree;
	uint64									IoRateOverQuotaHistory;
	uint32									IoRateCurrentGeneration;
	uint32									IoRateLastQueryGeneration;
	uint32									IoRateGenerationLength;
	uint32									IoRateOverQuotaNotifySequenceId;
	uint64									LastThrottledIoTime;
	_EX_PUSH_LOCK							IoControlLock;
	uint64									SiloHardReferenceCount;
	_WORK_QUEUE_ITEM						RundownWorkItem;
	void									*PartitionObject;
	_EJOB									*PartitionOwnerJob;
	_JOBOBJECT_ENERGY_TRACKING_STATE		EnergyTrackingState;
};

// TI = 0x1c5b
enum _FUNCTION_TABLE_TYPE {
	RF_SORTED			= 0,
	RF_UNSORTED			= 1,
	RF_CALLBACK			= 2,
	RF_KERNEL_DYNAMIC	= 3,
};

// TI = 0x1c65
struct _IMAGE_RUNTIME_FUNCTION_ENTRY {
	uint32	BeginAddress;
	uint32	EndAddress;
	union {
		uint32	UnwindInfoAddress;
		uint32	UnwindData;
	};
};

// TI = 0x1c5d
struct _DYNAMIC_FUNCTION_TABLE {
	_LIST_ENTRY						ListEntry;
	_IMAGE_RUNTIME_FUNCTION_ENTRY	*FunctionTable;
	_LARGE_INTEGER					TimeStamp;
	uint64							MinimumAddress;
	uint64							MaximumAddress;
	uint64							BaseAddress;
	_IMAGE_RUNTIME_FUNCTION_ENTRY *(*Callback)(uint64, void*);
	void							*Context;
	wchar_t							*OutOfProcessCallbackDll;
	_FUNCTION_TABLE_TYPE			Type;
	uint32							EntryCount;
	_RTL_BALANCED_NODE				TreeNode;
};

// TI = 0x1c49
struct _INVERTED_FUNCTION_TABLE_ENTRY {
	union {
		_IMAGE_RUNTIME_FUNCTION_ENTRY	*FunctionTable;
		_DYNAMIC_FUNCTION_TABLE			*DynamicTable;
	};
	void							*ImageBase;
	uint32							SizeOfImage;
	uint32							SizeOfTable;
};

// TI = 0x1ab7
struct _INVERTED_FUNCTION_TABLE {
	uint32							CurrentSize;
	uint32							MaximumSize;
	volatile uint32					Epoch;
	uint8							Overflow;
	_INVERTED_FUNCTION_TABLE_ENTRY	TableEntry[512];
};

// TI = 0x1b18
struct _PO_DIAG_STACK_RECORD {
	uint32	StackDepth;
	void	*Stack[1];
};

// TI = 0x1a16
struct _ALPC_PROCESS_CONTEXT {
	_EX_PUSH_LOCK	Lock;
	_LIST_ENTRY		ViewListHead;
	volatile uint64	PagedPoolQuotaCache;
};

// TI = 0x1aed
struct _MMSUPPORT_SHARED {
	volatile int	WorkingSetLock;
	int				GoodCitizenWaiting;
	uint64			ReleasedCommitDebt;
	uint64			ResetPagesRepurposedCount;
	void			*WsSwapSupport;
	void			*CommitReleaseContext;
	volatile int	WorkingSetCoreLock;
	void			*AccessLog;
	volatile uint64	ChargedWslePages;
	uint64			ActualWslePages;
	void			*ShadowMapping;
};

// TI = 0x1b80
struct _MMSUPPORT_FLAGS {
	union {
		struct {
			uint8	WorkingSetType:3;
			uint8	Reserved0:3;
			uint8	MaximumWorkingSetHard:1;
			uint8	MinimumWorkingSetHard:1;
			uint8	SessionMaster:1;
			uint8	TrimmerState:2;
			uint8	Reserved:1;
			uint8	PageStealers:4;
		};
		uint16	u1;
	};
	char	_pdb_padding0[1];
	uint8	MemoryPriority;
	union {
		struct {
			uint8	WsleDeleted:1;
			uint8	SvmEnabled:1;
			uint8	ForceAge:1;
			uint8	ForceTrim:1;
			uint8	NewMaximum:1;
			uint8	CommitReleaseState:2;
		};
		uint8	u2;
	};
};

// TI = 0x1aa5
struct _MMWSL_INSTANCE {};

// TI = 0x1aaa
struct _MMSUPPORT_INSTANCE {
	uint32					NextPageColor;
	uint32					PageFaultCount;
	uint64					TrimmedPageCount;
	struct _MMWSL_INSTANCE	*VmWorkingSetList;
	_LIST_ENTRY				WorkingSetExpansionLinks;
	uint64					AgeDistribution[8];
	_KGATE					*ExitOutswapGate;
	uint64					MinimumWorkingSetSize;
	uint64					WorkingSetLeafSize;
	uint64					WorkingSetLeafPrivateSize;
	uint64					WorkingSetSize;
	uint64					WorkingSetPrivateSize;
	uint64					MaximumWorkingSetSize;
	uint64					PeakWorkingSetSize;
	uint32					HardFaultCount;
	uint16					LastTrimStamp;
	uint16					PartitionId;
	uint64					SelfmapLock;
	_MMSUPPORT_FLAGS		Flags;
};

// TI = 0x19b4
struct _MMSUPPORT_FULL {
	_MMSUPPORT_INSTANCE	Instance;
	_MMSUPPORT_SHARED	Shared;
};

// TI = 0x1ce6
struct _OBJECT_NAME_INFORMATION {
	_UNICODE_STRING	Name;
};

// TI = 0x1b27
struct _SE_AUDIT_PROCESS_CREATION_INFO {
	_OBJECT_NAME_INFORMATION	*ImageFileName;
};

// TI = 0x1a0a
struct _IO_COMPLETION_CONTEXT {
	void	*Port;
	void	*Key;
};

// TI = 0x13e2 - forward reference
struct _FILE_OBJECT;

// TI = 0x1a7f
struct _SECTION_OBJECT_POINTERS {
	void	*DataSectionObject;
	void	*SharedCacheMap;
	void	*ImageSectionObject;
};

// TI = 0x1460 - forward reference
struct _VPB;

// TI = 0x1a1d
struct _DEVICE_OBJECT_POWER_EXTENSION {};

// TI = 0x13bf - forward reference
struct _DEVICE_OBJECT;

// TI = 0x1a20
struct _DEVOBJ_EXTENSION {
	int16									Type;
	uint16									Size;
	_DEVICE_OBJECT							*DeviceObject;
	uint32									PowerFlags;
	struct _DEVICE_OBJECT_POWER_EXTENSION	*Dope;
	uint32									ExtensionFlags;
	void									*DeviceNode;
	_DEVICE_OBJECT							*AttachedTo;
	int										StartIoCount;
	int										StartIoKey;
	uint32									StartIoFlags;
	_VPB									*Vpb;
	void									*DependencyNode;
	void									*InterruptContext;
	void									*VerifierContext;
};

// TI = 0x1a29
struct _KDEVICE_QUEUE {
	int16		Type;
	int16		Size;
	_LIST_ENTRY	DeviceListHead;
	uint64		Lock;
	union {
		uint8		Busy;
		struct {
			int64		Reserved:8;
			int64		Hint:56;
		};
	};
};

// TI = 0x1416 - forward reference
struct _IRP;

// TI = 0x1df5
struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {
	uint8	Type;
	uint8	ShareDisposition;
	uint16	Flags;
	union  {
		struct {
			_LARGE_INTEGER	Start;
			uint32			Length;
		}		Generic;
		struct {
			_LARGE_INTEGER	Start;
			uint32			Length;
		}		Port;
		struct {
			uint16	Level;
			uint16	Group;
			uint32	Vector;
			uint64	Affinity;
		}		Interrupt;
		struct {
			union {
				struct {
					uint16	Group;
					uint16	MessageCount;
					uint32	Vector;
					uint64	Affinity;
				}		Raw;
				struct {
					uint16	Level;
					uint16	Group;
					uint32	Vector;
					uint64	Affinity;
				}		Translated;
			};
		}		MessageInterrupt;
		struct {
			_LARGE_INTEGER	Start;
			uint32			Length;
		}		Memory;
		struct {
			uint32	Channel;
			uint32	Port;
			uint32	Reserved1;
		}		Dma;
		struct {
			uint32	Channel;
			uint32	RequestLine;
			uint8	TransferWidth;
			uint8	Reserved1;
			uint8	Reserved2;
			uint8	Reserved3;
		}		DmaV3;
		struct {
			uint32	Data[3];
		}		DevicePrivate;
		struct {
			uint32	Start;
			uint32	Length;
			uint32	Reserved;
		}		BusNumber;
		struct {
			uint32	DataSize;
			uint32	Reserved1;
			uint32	Reserved2;
		}		DeviceSpecificData;
		struct {
			_LARGE_INTEGER	Start;
			uint32			Length40;
		}		Memory40;
		struct {
			_LARGE_INTEGER	Start;
			uint32			Length48;
		}		Memory48;
		struct {
			_LARGE_INTEGER	Start;
			uint32			Length64;
		}		Memory64;
		struct {
			uint8	Class;
			uint8	Type;
			uint8	Reserved1;
			uint8	Reserved2;
			uint32	IdLowPart;
			uint32	IdHighPart;
		}		Connection;
	}		u;
};

// TI = 0x1dd6
struct _CM_PARTIAL_RESOURCE_LIST {
	uint16							Version;
	uint16							Revision;
	uint32							Count;
	_CM_PARTIAL_RESOURCE_DESCRIPTOR	PartialDescriptors[1];
};

// TI = 0x1a89
enum _INTERFACE_TYPE {
	InterfaceTypeUndefined	= -1,
	Internal				= 0,
	Isa						= 1,
	Eisa					= 2,
	MicroChannel			= 3,
	TurboChannel			= 4,
	PCIBus					= 5,
	VMEBus					= 6,
	NuBus					= 7,
	PCMCIABus				= 8,
	CBus					= 9,
	MPIBus					= 10,
	MPSABus					= 11,
	ProcessorInternal		= 12,
	InternalPowerBus		= 13,
	PNPISABus				= 14,
	PNPBus					= 15,
	Vmcs					= 16,
	ACPIBus					= 17,
	MaximumInterfaceType	= 18,
};

// TI = 0x1d46
struct _CM_FULL_RESOURCE_DESCRIPTOR {
	_INTERFACE_TYPE				InterfaceType;
	uint32						BusNumber;
	_CM_PARTIAL_RESOURCE_LIST	PartialResourceList;
};

// TI = 0x1af6
struct _CM_RESOURCE_LIST {
	uint32							Count;
	_CM_FULL_RESOURCE_DESCRIPTOR	List[1];
};

// TI = 0x1499
enum POWER_ACTION {
	PowerActionNone				= 0,
	PowerActionReserved			= 1,
	PowerActionSleep			= 2,
	PowerActionHibernate		= 3,
	PowerActionShutdown			= 4,
	PowerActionShutdownReset	= 5,
	PowerActionShutdownOff		= 6,
	PowerActionWarmEject		= 7,
	PowerActionDisplayOff		= 8,
};

// TI = 0x1559
enum _DEVICE_POWER_STATE {
	PowerDeviceUnspecified	= 0,
	PowerDeviceD0			= 1,
	PowerDeviceD1			= 2,
	PowerDeviceD2			= 3,
	PowerDeviceD3			= 4,
	PowerDeviceMaximum		= 5,
};

// TI = 0x148d
enum _SYSTEM_POWER_STATE {
	PowerSystemUnspecified	= 0,
	PowerSystemWorking		= 1,
	PowerSystemSleeping1	= 2,
	PowerSystemSleeping2	= 3,
	PowerSystemSleeping3	= 4,
	PowerSystemHibernate	= 5,
	PowerSystemShutdown		= 6,
	PowerSystemMaximum		= 7,
};

// TI = 0x1a14
union _POWER_STATE {
	_SYSTEM_POWER_STATE	SystemState;
	_DEVICE_POWER_STATE	DeviceState;
};

// TI = 0x1496
enum _POWER_STATE_TYPE {
	SystemPowerState	= 0,
	DevicePowerState	= 1,
};

// TI = 0x1a62
struct _SYSTEM_POWER_STATE_CONTEXT {
	union {
		struct {
			uint32	Reserved1:8;
			uint32	TargetSystemState:4;
			uint32	EffectiveSystemState:4;
			uint32	CurrentSystemState:4;
			uint32	IgnoreHibernationPath:1;
			uint32	PseudoTransition:1;
			uint32	KernelSoftReboot:1;
			uint32	DirectedDripsTransition:1;
			uint32	Reserved2:8;
		};
		uint32	ContextAsUlong;
	};
};

// TI = 0x1ad6
struct _POWER_SEQUENCE {
	uint32	SequenceD1;
	uint32	SequenceD2;
	uint32	SequenceD3;
};

// TI = 0x1489
enum _DEVICE_USAGE_NOTIFICATION_TYPE {
	DeviceUsageTypeUndefined	= 0,
	DeviceUsageTypePaging		= 1,
	DeviceUsageTypeHibernation	= 2,
	DeviceUsageTypeDumpFile		= 3,
	DeviceUsageTypeBoot			= 4,
	DeviceUsageTypePostDisplay	= 5,
};

// TI = 0x1485
enum DEVICE_TEXT_TYPE {
	DeviceTextDescription			= 0,
	DeviceTextLocationInformation	= 1,
};

// TI = 0x1481
enum BUS_QUERY_ID_TYPE {
	BusQueryDeviceID			= 0,
	BusQueryHardwareIDs			= 1,
	BusQueryCompatibleIDs		= 2,
	BusQueryInstanceID			= 3,
	BusQueryDeviceSerialNumber	= 4,
	BusQueryContainerID			= 5,
};

// TI = 0x1cfa
enum _IRQ_PRIORITY {
	IrqPriorityUndefined	= 0,
	IrqPriorityLow			= 1,
	IrqPriorityNormal		= 2,
	IrqPriorityHigh			= 3,
};

// TI = 0x1d12
struct _IO_RESOURCE_DESCRIPTOR {
	uint8	Option;
	uint8	Type;
	uint8	ShareDisposition;
	uint8	Spare1;
	uint16	Flags;
	uint16	Spare2;
	union  {
		struct {
			uint32			Length;
			uint32			Alignment;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}		Port;
		struct {
			uint32			Length;
			uint32			Alignment;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}		Memory;
		struct {
			uint32			MinimumVector;
			uint32			MaximumVector;
			uint16			AffinityPolicy;
			uint16			Group;
			_IRQ_PRIORITY	PriorityPolicy;
			uint64			TargetedProcessors;
		}		Interrupt;
		struct {
			uint32	MinimumChannel;
			uint32	MaximumChannel;
		}		Dma;
		struct {
			uint32	RequestLine;
			uint32	Reserved;
			uint32	Channel;
			uint32	TransferWidth;
		}		DmaV3;
		struct {
			uint32			Length;
			uint32			Alignment;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}		Generic;
		struct {
			uint32	Data[3];
		}		DevicePrivate;
		struct {
			uint32	Length;
			uint32	MinBusNumber;
			uint32	MaxBusNumber;
			uint32	Reserved;
		}		BusNumber;
		struct {
			uint32	Priority;
			uint32	Reserved1;
			uint32	Reserved2;
		}		ConfigData;
		struct {
			uint32			Length40;
			uint32			Alignment40;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}		Memory40;
		struct {
			uint32			Length48;
			uint32			Alignment48;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}		Memory48;
		struct {
			uint32			Length64;
			uint32			Alignment64;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}		Memory64;
		struct {
			uint8	Class;
			uint8	Type;
			uint8	Reserved1;
			uint8	Reserved2;
			uint32	IdLowPart;
			uint32	IdHighPart;
		}		Connection;
	}		u;
};

// TI = 0x1ca9
struct _IO_RESOURCE_LIST {
	uint16					Version;
	uint16					Revision;
	uint32					Count;
	_IO_RESOURCE_DESCRIPTOR	Descriptors[1];
};

// TI = 0x1a8d
struct _IO_RESOURCE_REQUIREMENTS_LIST {
	uint32				ListSize;
	_INTERFACE_TYPE		InterfaceType;
	uint32				BusNumber;
	uint32				SlotNumber;
	uint32				Reserved[3];
	uint32				AlternativeLists;
	_IO_RESOURCE_LIST	List[1];
};

// TI = 0x1913
struct _DEVICE_CAPABILITIES {
	uint16				Size;
	uint16				Version;
	uint32				DeviceD1:1;
	uint32				DeviceD2:1;
	uint32				LockSupported:1;
	uint32				EjectSupported:1;
	uint32				Removable:1;
	uint32				DockDevice:1;
	uint32				UniqueID:1;
	uint32				SilentInstall:1;
	uint32				RawDeviceOK:1;
	uint32				SurpriseRemovalOK:1;
	uint32				WakeFromD0:1;
	uint32				WakeFromD1:1;
	uint32				WakeFromD2:1;
	uint32				WakeFromD3:1;
	uint32				HardwareDisabled:1;
	uint32				NonDynamic:1;
	uint32				WarmEjectSupported:1;
	uint32				NoDisplayInUI:1;
	uint32				Reserved1:1;
	uint32				WakeFromInterrupt:1;
	uint32				SecureDevice:1;
	uint32				ChildOfVgaEnabledBridge:1;
	uint32				DecodeIoOnBoot:1;
	uint32				Reserved:9;
	uint32				Address;
	uint32				UINumber;
	_DEVICE_POWER_STATE	DeviceState[7];
	_SYSTEM_POWER_STATE	SystemWake;
	_DEVICE_POWER_STATE	DeviceWake;
	uint32				D1Latency;
	uint32				D2Latency;
	uint32				D3Latency;
};

// TI = 0x18ec
struct _INTERFACE {
	uint16	Size;
	uint16	Version;
	void	*Context;
	void (*InterfaceReference)(void*);
	void (*InterfaceDereference)(void*);
};

// TI = 0x146d
enum _DEVICE_RELATION_TYPE {
	BusRelations			= 0,
	EjectionRelations		= 1,
	PowerRelations			= 2,
	RemovalRelations		= 3,
	TargetDeviceRelation	= 4,
	SingleBusRelations		= 5,
	TransportRelations		= 6,
};

// TI = 0x1bcc
struct _SID_IDENTIFIER_AUTHORITY {
	uint8	Value[6];
};

// TI = 0x1b98
struct _SID {
	uint8						Revision;
	uint8						SubAuthorityCount;
	_SID_IDENTIFIER_AUTHORITY	IdentifierAuthority;
	uint32						SubAuthority[1];
};

// TI = 0x1a9a
struct _FILE_GET_QUOTA_INFORMATION {
	uint32	NextEntryOffset;
	uint32	SidLength;
	_SID	Sid;
};

// TI = 0x1464
struct _SCSI_REQUEST_BLOCK {};

// TI = 0x1453
enum _FSINFOCLASS {
	FileFsVolumeInformation			= 1,
	FileFsLabelInformation			= 2,
	FileFsSizeInformation			= 3,
	FileFsDeviceInformation			= 4,
	FileFsAttributeInformation		= 5,
	FileFsControlInformation		= 6,
	FileFsFullSizeInformation		= 7,
	FileFsObjectIdInformation		= 8,
	FileFsDriverPathInformation		= 9,
	FileFsVolumeFlagsInformation	= 10,
	FileFsSectorSizeInformation		= 11,
	FileFsDataCopyInformation		= 12,
	FileFsMetadataSizeInformation	= 13,
	FileFsFullSizeInformationEx		= 14,
	FileFsMaximumInformation		= 15,
};

// TI = 0x1441
enum _FILE_INFORMATION_CLASS {
	FileDirectoryInformation						= 1,
	FileFullDirectoryInformation					= 2,
	FileBothDirectoryInformation					= 3,
	FileBasicInformation							= 4,
	FileStandardInformation							= 5,
	FileInternalInformation							= 6,
	FileEaInformation								= 7,
	FileAccessInformation							= 8,
	FileNameInformation								= 9,
	FileRenameInformation							= 10,
	FileLinkInformation								= 11,
	FileNamesInformation							= 12,
	FileDispositionInformation						= 13,
	FilePositionInformation							= 14,
	FileFullEaInformation							= 15,
	FileModeInformation								= 16,
	FileAlignmentInformation						= 17,
	FileAllInformation								= 18,
	FileAllocationInformation						= 19,
	FileEndOfFileInformation						= 20,
	FileAlternateNameInformation					= 21,
	FileStreamInformation							= 22,
	FilePipeInformation								= 23,
	FilePipeLocalInformation						= 24,
	FilePipeRemoteInformation						= 25,
	FileMailslotQueryInformation					= 26,
	FileMailslotSetInformation						= 27,
	FileCompressionInformation						= 28,
	FileObjectIdInformation							= 29,
	FileCompletionInformation						= 30,
	FileMoveClusterInformation						= 31,
	FileQuotaInformation							= 32,
	FileReparsePointInformation						= 33,
	FileNetworkOpenInformation						= 34,
	FileAttributeTagInformation						= 35,
	FileTrackingInformation							= 36,
	FileIdBothDirectoryInformation					= 37,
	FileIdFullDirectoryInformation					= 38,
	FileValidDataLengthInformation					= 39,
	FileShortNameInformation						= 40,
	FileIoCompletionNotificationInformation			= 41,
	FileIoStatusBlockRangeInformation				= 42,
	FileIoPriorityHintInformation					= 43,
	FileSfioReserveInformation						= 44,
	FileSfioVolumeInformation						= 45,
	FileHardLinkInformation							= 46,
	FileProcessIdsUsingFileInformation				= 47,
	FileNormalizedNameInformation					= 48,
	FileNetworkPhysicalNameInformation				= 49,
	FileIdGlobalTxDirectoryInformation				= 50,
	FileIsRemoteDeviceInformation					= 51,
	FileUnusedInformation							= 52,
	FileNumaNodeInformation							= 53,
	FileStandardLinkInformation						= 54,
	FileRemoteProtocolInformation					= 55,
	FileRenameInformationBypassAccessCheck			= 56,
	FileLinkInformationBypassAccessCheck			= 57,
	FileVolumeNameInformation						= 58,
	FileIdInformation								= 59,
	FileIdExtdDirectoryInformation					= 60,
	FileReplaceCompletionInformation				= 61,
	FileHardLinkFullIdInformation					= 62,
	FileIdExtdBothDirectoryInformation				= 63,
	FileDispositionInformationEx					= 64,
	FileRenameInformationEx							= 65,
	FileRenameInformationExBypassAccessCheck		= 66,
	FileDesiredStorageClassInformation				= 67,
	FileStatInformation								= 68,
	FileMemoryPartitionInformation					= 69,
	FileStatLxInformation							= 70,
	FileCaseSensitiveInformation					= 71,
	FileLinkInformationEx							= 72,
	FileLinkInformationExBypassAccessCheck			= 73,
	FileStorageReserveIdInformation					= 74,
	FileCaseSensitiveInformationForceAccessCheck	= 75,
	FileMaximumInformation							= 76,
};

// TI = 0x1447
enum _DIRECTORY_NOTIFY_INFORMATION_CLASS {
	DirectoryNotifyInformation			= 1,
	DirectoryNotifyExtendedInformation	= 2,
};

// TI = 0x1ab3
struct _MAILSLOT_CREATE_PARAMETERS {
	uint32			MailslotQuota;
	uint32			MaximumMessageSize;
	_LARGE_INTEGER	ReadTimeout;
	uint8			TimeoutSpecified;
};

// TI = 0x1afa
struct _LUID_AND_ATTRIBUTES {
	_LUID	Luid;
	uint32	Attributes;
};

// TI = 0x1a6e
struct _PRIVILEGE_SET {
	uint32					PrivilegeCount;
	uint32					Control;
	_LUID_AND_ATTRIBUTES	Privilege[1];
};

// TI = 0x1a7d
struct _INITIAL_PRIVILEGE_SET {
	uint32					PrivilegeCount;
	uint32					Control;
	_LUID_AND_ATTRIBUTES	Privilege[3];
};

// TI = 0x1afc
enum _SECURITY_IMPERSONATION_LEVEL {
	SecurityAnonymous		= 0,
	SecurityIdentification	= 1,
	SecurityImpersonation	= 2,
	SecurityDelegation		= 3,
};

// TI = 0x1b21
struct _SECURITY_SUBJECT_CONTEXT {
	void							*ClientToken;
	_SECURITY_IMPERSONATION_LEVEL	ImpersonationLevel;
	void							*PrimaryToken;
	void							*ProcessAuditId;
};

// TI = 0x13a4
struct _ACCESS_STATE {
	_LUID						OperationID;
	uint8						SecurityEvaluated;
	uint8						GenerateAudit;
	uint8						GenerateOnClose;
	uint8						PrivilegesAllocated;
	uint32						Flags;
	uint32						RemainingDesiredAccess;
	uint32						PreviouslyGrantedAccess;
	uint32						OriginalDesiredAccess;
	_SECURITY_SUBJECT_CONTEXT	SubjectSecurityContext;
	void						*SecurityDescriptor;
	void						*AuxData;
	union  {
		_INITIAL_PRIVILEGE_SET	InitialPrivilegeSet;
		_PRIVILEGE_SET			PrivilegeSet;
	}							Privileges;
	uint8						AuditPrivileges;
	_UNICODE_STRING				ObjectName;
	_UNICODE_STRING				ObjectTypeName;
};

// TI = 0x1afe
struct _SECURITY_QUALITY_OF_SERVICE {
	uint32							Length;
	_SECURITY_IMPERSONATION_LEVEL	ImpersonationLevel;
	uint8							ContextTrackingMode;
	uint8							EffectiveOnly;
};

// TI = 0x1a9e
struct _IO_SECURITY_CONTEXT {
	_SECURITY_QUALITY_OF_SERVICE	*SecurityQos;
	_ACCESS_STATE					*AccessState;
	uint32							DesiredAccess;
	uint32							FullCreateOptions;
};

// TI = 0x19cc
struct _NAMED_PIPE_CREATE_PARAMETERS {
	uint32			NamedPipeType;
	uint32			ReadMode;
	uint32			CompletionMode;
	uint32			MaximumInstances;
	uint32			InboundQuota;
	uint32			OutboundQuota;
	_LARGE_INTEGER	DefaultTimeout;
	uint8			TimeoutSpecified;
};

// TI = 0x14aa
struct _IO_STACK_LOCATION {
	uint8			MajorFunction;
	uint8			MinorFunction;
	uint8			Flags;
	uint8			Control;
	union  {
		struct {
			_IO_SECURITY_CONTEXT	*SecurityContext;
			uint32					Options;
			char					_pdb_padding0[4];
			uint16					FileAttributes;
			uint16					ShareAccess;
			char					_pdb_padding1[4];
			uint32					EaLength;
		}		Create;
		struct {
			_IO_SECURITY_CONTEXT			*SecurityContext;
			uint32							Options;
			char							_pdb_padding0[4];
			uint16							Reserved;
			uint16							ShareAccess;
			_NAMED_PIPE_CREATE_PARAMETERS	*Parameters;
		}		CreatePipe;
		struct {
			_IO_SECURITY_CONTEXT		*SecurityContext;
			uint32						Options;
			char						_pdb_padding0[4];
			uint16						Reserved;
			uint16						ShareAccess;
			_MAILSLOT_CREATE_PARAMETERS	*Parameters;
		}		CreateMailslot;
		struct {
			uint32			Length;
			char			_pdb_padding0[4];
			uint32			Key;
			uint32			Flags;
			_LARGE_INTEGER	ByteOffset;
		}		Read;
		struct {
			uint32			Length;
			char			_pdb_padding0[4];
			uint32			Key;
			uint32			Flags;
			_LARGE_INTEGER	ByteOffset;
		}		Write;
		struct {
			uint32					Length;
			_UNICODE_STRING			*FileName;
			_FILE_INFORMATION_CLASS	FileInformationClass;
			char					_pdb_padding0[4];
			uint32					FileIndex;
		}		QueryDirectory;
		struct {
			uint32	Length;
			char	_pdb_padding0[4];
			uint32	CompletionFilter;
		}		NotifyDirectory;
		struct {
			uint32								Length;
			char								_pdb_padding0[4];
			uint32								CompletionFilter;
			char								_pdb_padding1[4];
			_DIRECTORY_NOTIFY_INFORMATION_CLASS	DirectoryNotifyInformationClass;
		}		NotifyDirectoryEx;
		struct {
			uint32					Length;
			char					_pdb_padding0[4];
			_FILE_INFORMATION_CLASS	FileInformationClass;
		}		QueryFile;
		struct {
			uint32					Length;
			char					_pdb_padding0[4];
			_FILE_INFORMATION_CLASS	FileInformationClass;
			_FILE_OBJECT			*FileObject;
			union {
				struct {
					uint8					ReplaceIfExists;
					uint8					AdvanceOnly;
				};
				uint32					ClusterCount;
				void					*DeleteHandle;
			};
		}		SetFile;
		struct {
			uint32	Length;
			void	*EaList;
			uint32	EaListLength;
			char	_pdb_padding0[4];
			uint32	EaIndex;
		}		QueryEa;
		struct {
			uint32	Length;
		}		SetEa;
		struct {
			uint32			Length;
			char			_pdb_padding0[4];
			_FSINFOCLASS	FsInformationClass;
		}		QueryVolume;
		struct {
			uint32			Length;
			char			_pdb_padding0[4];
			_FSINFOCLASS	FsInformationClass;
		}		SetVolume;
		struct {
			uint32	OutputBufferLength;
			char	_pdb_padding0[4];
			uint32	InputBufferLength;
			char	_pdb_padding1[4];
			uint32	FsControlCode;
			void	*Type3InputBuffer;
		}		FileSystemControl;
		struct {
			_LARGE_INTEGER	*Length;
			uint32			Key;
			_LARGE_INTEGER	ByteOffset;
		}		LockControl;
		struct {
			uint32	OutputBufferLength;
			char	_pdb_padding0[4];
			uint32	InputBufferLength;
			char	_pdb_padding1[4];
			uint32	IoControlCode;
			void	*Type3InputBuffer;
		}		DeviceIoControl;
		struct {
			uint32	SecurityInformation;
			char	_pdb_padding0[4];
			uint32	Length;
		}		QuerySecurity;
		struct {
			uint32	SecurityInformation;
			void	*SecurityDescriptor;
		}		SetSecurity;
		struct {
			_VPB			*Vpb;
			_DEVICE_OBJECT	*DeviceObject;
		}		MountVolume;
		struct {
			_VPB			*Vpb;
			_DEVICE_OBJECT	*DeviceObject;
		}		VerifyVolume;
		struct {
			struct _SCSI_REQUEST_BLOCK	*Srb;
		}		Scsi;
		struct {
			uint32						Length;
			void						*StartSid;
			_FILE_GET_QUOTA_INFORMATION	*SidList;
			uint32						SidListLength;
		}		QueryQuota;
		struct {
			uint32	Length;
		}		SetQuota;
		struct {
			_DEVICE_RELATION_TYPE	Type;
		}		QueryDeviceRelations;
		struct {
			const _GUID	*InterfaceType;
			uint16		Size;
			uint16		Version;
			_INTERFACE	*Interface;
			void		*InterfaceSpecificData;
		}		QueryInterface;
		struct {
			_DEVICE_CAPABILITIES	*Capabilities;
		}		DeviceCapabilities;
		struct {
			_IO_RESOURCE_REQUIREMENTS_LIST	*IoResourceRequirementList;
		}		FilterResourceRequirements;
		struct {
			uint32	WhichSpace;
			void	*Buffer;
			uint32	Offset;
			char	_pdb_padding0[4];
			uint32	Length;
		}		ReadWriteConfig;
		struct {
			uint8	Lock;
		}		SetLock;
		struct {
			BUS_QUERY_ID_TYPE	IdType;
		}		QueryId;
		struct {
			DEVICE_TEXT_TYPE	DeviceTextType;
			char				_pdb_padding0[4];
			uint32				LocaleId;
		}		QueryDeviceText;
		struct {
			uint8							InPath;
			uint8							Reserved[3];
			char							_pdb_padding0[4];
			_DEVICE_USAGE_NOTIFICATION_TYPE	Type;
		}		UsageNotification;
		struct {
			_SYSTEM_POWER_STATE	PowerState;
		}		WaitWake;
		struct {
			_POWER_SEQUENCE	*PowerSequence;
		}		PowerSequence;
		struct {
			union {
				uint32						SystemContext;
				_SYSTEM_POWER_STATE_CONTEXT	SystemPowerStateContext;
			};
			char						_pdb_padding0[4];
			_POWER_STATE_TYPE			Type;
			char						_pdb_padding1[4];
			_POWER_STATE				State;
			char						_pdb_padding2[4];
			POWER_ACTION				ShutdownType;
		}		Power;
		struct {
			_CM_RESOURCE_LIST	*AllocatedResources;
			_CM_RESOURCE_LIST	*AllocatedResourcesTranslated;
		}		StartDevice;
		struct {
			uint64	ProviderId;
			void	*DataPath;
			uint32	BufferSize;
			void	*Buffer;
		}		WMI;
		struct {
			void	*Argument1;
			void	*Argument2;
			void	*Argument3;
			void	*Argument4;
		}		Others;
	}				Parameters;
	_DEVICE_OBJECT	*DeviceObject;
	_FILE_OBJECT	*FileObject;
	int (*CompletionRoutine)(_DEVICE_OBJECT *, _IRP *, void*);
	void			*Context;
};

// TI = 0x1950
struct _THREAD_ENERGY_VALUES {
	uint64				Cycles[4][2];
	uint64				AttributedCycles[4][2];
	uint64				WorkOnBehalfCycles[4][2];
	_TIMELINE_BITMAP	CpuTimeline;
};

// TI = 0x1af0
union _PS_CLIENT_SECURITY_CONTEXT {
	uint64	ImpersonationData;
	void	*ImpersonationToken;
	struct {
		uint64	ImpersonationLevel:2;
		uint64	EffectiveOnly:1;
	};
};

// TI = 0x1a5c
struct _KSEMAPHORE {
	_DISPATCHER_HEADER	Header;
	int					Limit;
};

// TI = 0x139a - forward reference
struct _ETHREAD;

// TI = 0x13bb - forward reference
struct _TERMINATION_PORT;

// TI = 0x1acc
struct _TERMINATION_PORT {
	_TERMINATION_PORT	*Next;
	void				*Port;
};

// TI = 0x13d0
struct _ETHREAD {
	_KTHREAD					Tcb;
	_LARGE_INTEGER				CreateTime;
	union {
		_LARGE_INTEGER				ExitTime;
		_LIST_ENTRY					KeyedWaitChain;
	};
	char						_pdb_padding0[8];
	union {
		_LIST_ENTRY					PostBlockList;
		struct {
			void						*ForwardLinkShadow;
			void						*StartAddress;
		};
	};
	union {
		_TERMINATION_PORT			*TerminationPort;
		_ETHREAD					*ReaperLink;
		void						*KeyedWaitValue;
	};
	uint64						ActiveTimerListLock;
	_LIST_ENTRY					ActiveTimerListHead;
	_CLIENT_ID					Cid;
	union {
		_KSEMAPHORE					KeyedWaitSemaphore;
		_KSEMAPHORE					AlpcWaitSemaphore;
	};
	_PS_CLIENT_SECURITY_CONTEXT	ClientSecurity;
	_LIST_ENTRY					IrpList;
	uint64						TopLevelIrp;
	_DEVICE_OBJECT				*DeviceToVerify;
	void						*Win32StartAddress;
	void						*ChargeOnlySession;
	void						*LegacyPowerObject;
	_LIST_ENTRY					ThreadListEntry;
	_EX_RUNDOWN_REF				RundownProtect;
	_EX_PUSH_LOCK				ThreadLock;
	uint32						ReadClusterSize;
	volatile int				MmLockOrdering;
	union {
		uint32						CrossThreadFlags;
		struct {
			uint32						Terminated:1;
			uint32						ThreadInserted:1;
			uint32						HideFromDebugger:1;
			uint32						ActiveImpersonationInfo:1;
			uint32						HardErrorsAreDisabled:1;
			uint32						BreakOnTermination:1;
			uint32						SkipCreationMsg:1;
			uint32						SkipTerminationMsg:1;
			uint32						CopyTokenOnOpen:1;
			uint32						ThreadIoPriority:3;
			uint32						ThreadPagePriority:3;
			uint32						RundownFail:1;
			uint32						UmsForceQueueTermination:1;
			uint32						IndirectCpuSets:1;
			uint32						DisableDynamicCodeOptOut:1;
			uint32						ExplicitCaseSensitivity:1;
			uint32						PicoNotifyExit:1;
			uint32						DbgWerUserReportActive:1;
			uint32						ForcedSelfTrimActive:1;
			uint32						SamplingCoverage:1;
			uint32						ReservedCrossThreadFlags:8;
		};
	};
	union {
		uint32						SameThreadPassiveFlags;
		struct {
			uint32						ActiveExWorker:1;
			uint32						MemoryMaker:1;
			uint32						StoreLockThread:2;
			uint32						ClonedThread:1;
			uint32						KeyedEventInUse:1;
			uint32						SelfTerminate:1;
			uint32						RespectIoPriority:1;
			uint32						ActivePageLists:1;
			uint32						SecureContext:1;
			uint32						ZeroPageThread:1;
			uint32						WorkloadClass:1;
			uint32						ReservedSameThreadPassiveFlags:20;
		};
	};
	union {
		uint32						SameThreadApcFlags;
		struct {
			uint8						OwnsProcessAddressSpaceExclusive:1;
			uint8						OwnsProcessAddressSpaceShared:1;
			uint8						HardFaultBehavior:1;
			volatile uint8				StartAddressInvalid:1;
			uint8						EtwCalloutActive:1;
			uint8						SuppressSymbolLoad:1;
			uint8						Prefetching:1;
			uint8						OwnsVadExclusive:1;
			uint8						SystemPagePriorityActive:1;
			uint8						SystemPagePriority:3;
			uint8						AllowWritesToExecutableMemory:1;
			uint8						OwnsVadShared:1;
		};
	};
	uint8						CacheManagerActive;
	uint8						DisablePageFaultClustering;
	uint8						ActiveFaultCount;
	uint8						LockOrderState;
	uint64						AlpcMessageId;
	union {
		void						*AlpcMessage;
		uint32						AlpcReceiveAttributeSet;
	};
	_LIST_ENTRY					AlpcWaitListEntry;
	int							ExitStatus;
	uint32						CacheManagerCount;
	uint32						IoBoostCount;
	uint32						IoQoSBoostCount;
	uint32						IoQoSThrottleCount;
	uint32						KernelStackReference;
	_LIST_ENTRY					BoostList;
	_LIST_ENTRY					DeboostList;
	uint64						BoostListLock;
	uint64						IrpListLock;
	void						*ReservedForSynchTracking;
	_SINGLE_LIST_ENTRY			CmCallbackListHead;
	const _GUID					*ActivityId;
	_SINGLE_LIST_ENTRY			SeLearningModeListHead;
	void						*VerifierContext;
	void						*AdjustedClientToken;
	void						*WorkOnBehalfThread;
	_PS_PROPERTY_SET			PropertySet;
	void						*PicoContext;
	uint64						UserFsBase;
	uint64						UserGsBase;
	_THREAD_ENERGY_VALUES		*EnergyValues;
	void						*CmDbgInfo;
	union {
		uint64						SelectedCpuSets;
		uint64						*SelectedCpuSetsIndirect;
	};
	_EJOB						*Silo;
	_UNICODE_STRING				*ThreadName;
	_CONTEXT					*SetContextState;
	uint32						LastExpectedRunTime;
	uint32						HeapData;
	_LIST_ENTRY					OwnerEntryListHead;
	uint64						DisownedOwnerEntryListLock;
	_LIST_ENTRY					DisownedOwnerEntryListHead;
};

// TI = 0x1a7a
struct _KDEVICE_QUEUE_ENTRY {
	_LIST_ENTRY	DeviceListEntry;
	uint32		SortKey;
	uint8		Inserted;
};

// TI = 0x1418 - forward reference
struct _MDL;

// TI = 0x14fa
struct _MDL {
	_MDL		*Next;
	int16		Size;
	int16		MdlFlags;
	uint16		AllocationProcessorNumber;
	uint16		Reserved;
	_EPROCESS	*Process;
	void		*MappedSystemVa;
	void		*StartVa;
	uint32		ByteCount;
	uint32		ByteOffset;
};

// TI = 0x1430
struct _IRP {
	int16				Type;
	uint16				Size;
	uint16				AllocationProcessorNumber;
	uint16				Reserved;
	_MDL				*MdlAddress;
	uint32				Flags;
	union  {
		_IRP	*MasterIrp;
		int		IrpCount;
		void	*SystemBuffer;
	}					AssociatedIrp;
	_LIST_ENTRY			ThreadListEntry;
	_IO_STATUS_BLOCK	IoStatus;
	char				RequestorMode;
	uint8				PendingReturned;
	char				StackCount;
	char				CurrentLocation;
	uint8				Cancel;
	uint8				CancelIrql;
	char				ApcEnvironment;
	uint8				AllocationFlags;
	_IO_STATUS_BLOCK	*UserIosb;
	_KEVENT				*UserEvent;
	union  {
		struct {
			union {
				void (*UserApcRoutine)(void*, _IO_STATUS_BLOCK *, uint32);
				void	*IssuingProcess;
			};
			void	*UserApcContext;
		}				AsynchronousParameters;
		_LARGE_INTEGER	AllocationSize;
	}					Overlay;
	void (*CancelRoutine)(_DEVICE_OBJECT *, _IRP *);
	void				*UserBuffer;
	union  {
		struct {
			union {
				_KDEVICE_QUEUE_ENTRY	DeviceQueueEntry;
				void					*DriverContext[4];
			};
			char					_pdb_padding0[8];
			_ETHREAD				*Thread;
			char					*AuxiliaryBuffer;
			_LIST_ENTRY				ListEntry;
			union {
				_IO_STACK_LOCATION		*CurrentStackLocation;
				uint32					PacketType;
			};
			_FILE_OBJECT			*OriginalFileObject;
			void					*IrpExtension;
		}		Overlay;
		_KAPC	Apc;
		void	*CompletionKey;
	}					Tail;
};

// TI = 0x19d3
enum _IO_ALLOCATION_ACTION {
	KeepObject						= 1,
	DeallocateObject				= 2,
	DeallocateObjectKeepRegisters	= 3,
};

// TI = 0x19d8
struct _WAIT_CONTEXT_BLOCK {
	union {
		_KDEVICE_QUEUE_ENTRY	WaitQueueEntry;
		struct {
			_LIST_ENTRY				DmaWaitEntry;
			uint32					NumberOfChannels;
			uint32					SyncCallback:1;
			uint32					DmaContext:1;
			uint32					ZeroMapRegisters:1;
			uint32					Reserved:9;
			uint32					NumberOfRemapPages:20;
		};
	};
	_IO_ALLOCATION_ACTION (*DeviceRoutine)(_DEVICE_OBJECT *, _IRP *, void*, void*);
	void					*DeviceContext;
	uint32					NumberOfMapRegisters;
	void					*DeviceObject;
	void					*CurrentIrp;
	_KDPC					*BufferChainingDpc;
};

// TI = 0x19bd
struct _IO_TIMER {
	int16			Type;
	int16			TimerFlag;
	_LIST_ENTRY		TimerList;
	void (*TimerRoutine)(_DEVICE_OBJECT *, void*);
	void			*Context;
	_DEVICE_OBJECT	*DeviceObject;
};

// TI = 0x14b5 - forward reference
struct _DRIVER_OBJECT;

// TI = 0x1da3
struct _FILE_NETWORK_OPEN_INFORMATION {
	_LARGE_INTEGER	CreationTime;
	_LARGE_INTEGER	LastAccessTime;
	_LARGE_INTEGER	LastWriteTime;
	_LARGE_INTEGER	ChangeTime;
	_LARGE_INTEGER	AllocationSize;
	_LARGE_INTEGER	EndOfFile;
	uint32			FileAttributes;
};

// TI = 0x1db1
struct _COMPRESSED_DATA_INFO {
	uint16	CompressionFormatAndEngine;
	uint8	CompressionUnitShift;
	uint8	ChunkShift;
	uint8	ClusterShift;
	uint8	Reserved;
	uint16	NumberOfChunks;
	uint32	CompressedChunkSizes[1];
};

// TI = 0x1db3
struct _FILE_STANDARD_INFORMATION {
	_LARGE_INTEGER	AllocationSize;
	_LARGE_INTEGER	EndOfFile;
	uint32			NumberOfLinks;
	uint8			DeletePending;
	uint8			Directory;
};

// TI = 0x1d99
struct _FILE_BASIC_INFORMATION {
	_LARGE_INTEGER	CreationTime;
	_LARGE_INTEGER	LastAccessTime;
	_LARGE_INTEGER	LastWriteTime;
	_LARGE_INTEGER	ChangeTime;
	uint32			FileAttributes;
};

// TI = 0x1d8d
struct _FAST_IO_DISPATCH {
	uint32	SizeOfFastIoDispatch;
	uint8 (*FastIoCheckIfPossible)(_FILE_OBJECT *, _LARGE_INTEGER *, uint32, uint8, uint32, uint8, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoRead)(_FILE_OBJECT *, _LARGE_INTEGER *, uint32, uint8, uint32, void*, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoWrite)(_FILE_OBJECT *, _LARGE_INTEGER *, uint32, uint8, uint32, void*, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoQueryBasicInfo)(_FILE_OBJECT *, uint8, _FILE_BASIC_INFORMATION *, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoQueryStandardInfo)(_FILE_OBJECT *, uint8, _FILE_STANDARD_INFORMATION *, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoLock)(_FILE_OBJECT *, _LARGE_INTEGER *, _LARGE_INTEGER *, _EPROCESS *, uint32, uint8, uint8, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoUnlockSingle)(_FILE_OBJECT *, _LARGE_INTEGER *, _LARGE_INTEGER *, _EPROCESS *, uint32, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoUnlockAll)(_FILE_OBJECT *, _EPROCESS *, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoUnlockAllByKey)(_FILE_OBJECT *, void*, uint32, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*FastIoDeviceControl)(_FILE_OBJECT *, uint8, void*, uint32, void*, uint32, uint32, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	void (*AcquireFileForNtCreateSection)(_FILE_OBJECT *);
	void (*ReleaseFileForNtCreateSection)(_FILE_OBJECT *);
	void (*FastIoDetachDevice)(_DEVICE_OBJECT *, _DEVICE_OBJECT *);
	uint8 (*FastIoQueryNetworkOpenInfo)(_FILE_OBJECT *, uint8, _FILE_NETWORK_OPEN_INFORMATION *, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	int (*AcquireForModWrite)(_FILE_OBJECT *, _LARGE_INTEGER *, _ERESOURCE **, _DEVICE_OBJECT *);
	uint8 (*MdlRead)(_FILE_OBJECT *, _LARGE_INTEGER *, uint32, uint32, _MDL **, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*MdlReadComplete)(_FILE_OBJECT *, _MDL *, _DEVICE_OBJECT *);
	uint8 (*PrepareMdlWrite)(_FILE_OBJECT *, _LARGE_INTEGER *, uint32, uint32, _MDL **, _IO_STATUS_BLOCK *, _DEVICE_OBJECT *);
	uint8 (*MdlWriteComplete)(_FILE_OBJECT *, _LARGE_INTEGER *, _MDL *, _DEVICE_OBJECT *);
	uint8 (*FastIoReadCompressed)(_FILE_OBJECT *, _LARGE_INTEGER *, uint32, uint32, void*, _MDL **, _IO_STATUS_BLOCK *, _COMPRESSED_DATA_INFO *, uint32, _DEVICE_OBJECT *);
	uint8 (*FastIoWriteCompressed)(_FILE_OBJECT *, _LARGE_INTEGER *, uint32, uint32, void*, _MDL **, _IO_STATUS_BLOCK *, _COMPRESSED_DATA_INFO *, uint32, _DEVICE_OBJECT *);
	uint8 (*MdlReadCompleteCompressed)(_FILE_OBJECT *, _MDL *, _DEVICE_OBJECT *);
	uint8 (*MdlWriteCompleteCompressed)(_FILE_OBJECT *, _LARGE_INTEGER *, _MDL *, _DEVICE_OBJECT *);
	uint8 (*FastIoQueryOpen)(_IRP *, _FILE_NETWORK_OPEN_INFORMATION *, _DEVICE_OBJECT *);
	int (*ReleaseForModWrite)(_FILE_OBJECT *, _ERESOURCE *, _DEVICE_OBJECT *);
	int (*AcquireForCcFlush)(_FILE_OBJECT *, _DEVICE_OBJECT *);
	int (*ReleaseForCcFlush)(_FILE_OBJECT *, _DEVICE_OBJECT *);
};

// TI = 0x1e03
enum _FS_FILTER_STREAM_FO_NOTIFICATION_TYPE {
	NotifyTypeCreate	= 0,
	NotifyTypeRetired	= 1,
};

// TI = 0x1e15
struct _FS_FILTER_SECTION_SYNC_OUTPUT {
	uint32	StructureSize;
	uint32	SizeReturned;
	uint32	Flags;
	uint32	DesiredReadAlignment;
};

// TI = 0x1dfd
enum _FS_FILTER_SECTION_SYNC_TYPE {
	SyncTypeOther			= 0,
	SyncTypeCreateSection	= 1,
};

// TI = 0x1e0b
union _FS_FILTER_PARAMETERS {
	struct {
		_LARGE_INTEGER	*EndingOffset;
		_ERESOURCE		**ResourceToRelease;
	}		AcquireForModifiedPageWriter;
	struct {
		_ERESOURCE	*ResourceToRelease;
	}		ReleaseForModifiedPageWriter;
	struct {
		_FS_FILTER_SECTION_SYNC_TYPE	SyncType;
		uint32							PageProtection;
		_FS_FILTER_SECTION_SYNC_OUTPUT	*OutputInformation;
	}		AcquireForSectionSynchronization;
	struct {
		_FS_FILTER_STREAM_FO_NOTIFICATION_TYPE	NotificationType;
		char									_pdb_padding0[4];
		uint8									SafeToRecurse;
	}		NotifyStreamFileObject;
	struct {
		_IRP					*Irp;
		void					*FileInformation;
		uint32					*Length;
		_FILE_INFORMATION_CLASS	FileInformationClass;
		int						CompletionStatus;
	}		QueryOpen;
	struct {
		void	*Argument1;
		void	*Argument2;
		void	*Argument3;
		void	*Argument4;
		void	*Argument5;
	}		Others;
};

// TI = 0x1dd9
struct _FS_FILTER_CALLBACK_DATA {
	uint32					SizeOfFsFilterCallbackData;
	uint8					Operation;
	uint8					Reserved;
	_DEVICE_OBJECT			*DeviceObject;
	_FILE_OBJECT			*FileObject;
	_FS_FILTER_PARAMETERS	Parameters;
};

// TI = 0x1dc9
struct _FS_FILTER_CALLBACKS {
	uint32	SizeOfFsFilterCallbacks;
	uint32	Reserved;
	int (*PreAcquireForSectionSynchronization)(_FS_FILTER_CALLBACK_DATA *, void **);
	void (*PostAcquireForSectionSynchronization)(_FS_FILTER_CALLBACK_DATA *, int, void*);
	int (*PreReleaseForSectionSynchronization)(_FS_FILTER_CALLBACK_DATA *, void **);
	void (*PostReleaseForSectionSynchronization)(_FS_FILTER_CALLBACK_DATA *, int, void*);
	int (*PreAcquireForCcFlush)(_FS_FILTER_CALLBACK_DATA *, void **);
	void (*PostAcquireForCcFlush)(_FS_FILTER_CALLBACK_DATA *, int, void*);
	int (*PreReleaseForCcFlush)(_FS_FILTER_CALLBACK_DATA *, void **);
	void (*PostReleaseForCcFlush)(_FS_FILTER_CALLBACK_DATA *, int, void*);
	int (*PreAcquireForModifiedPageWriter)(_FS_FILTER_CALLBACK_DATA *, void **);
	void (*PostAcquireForModifiedPageWriter)(_FS_FILTER_CALLBACK_DATA *, int, void*);
	int (*PreReleaseForModifiedPageWriter)(_FS_FILTER_CALLBACK_DATA *, void **);
	void (*PostReleaseForModifiedPageWriter)(_FS_FILTER_CALLBACK_DATA *, int, void*);
	int (*PreQueryOpen)(_FS_FILTER_CALLBACK_DATA *, void **);
	void (*PostQueryOpen)(_FS_FILTER_CALLBACK_DATA *, int, void*);
};

// TI = 0x1ca0 - forward reference
struct _IO_CLIENT_EXTENSION;

// TI = 0x1da7
struct _IO_CLIENT_EXTENSION {
	_IO_CLIENT_EXTENSION	*NextExtension;
	void					*ClientIdentificationAddress;
};

// TI = 0x1ca5
struct _DRIVER_EXTENSION {
	_DRIVER_OBJECT			*DriverObject;
	int (*AddDevice)(_DRIVER_OBJECT *, _DEVICE_OBJECT *);
	uint32					Count;
	_UNICODE_STRING			ServiceKeyName;
	_IO_CLIENT_EXTENSION	*ClientDriverExtension;
	_FS_FILTER_CALLBACKS	*FsFilterCallbacks;
	void					*KseCallbacks;
	void					*DvCallbacks;
	void					*VerifierContext;
};

// TI = 0x1c74
struct _DRIVER_OBJECT {
	int16				Type;
	int16				Size;
	_DEVICE_OBJECT		*DeviceObject;
	uint32				Flags;
	void				*DriverStart;
	uint32				DriverSize;
	void				*DriverSection;
	_DRIVER_EXTENSION	*DriverExtension;
	_UNICODE_STRING		DriverName;
	_UNICODE_STRING		*HardwareDatabase;
	_FAST_IO_DISPATCH	*FastIoDispatch;
	int (*DriverInit)(_DRIVER_OBJECT *, _UNICODE_STRING *);
	void (*DriverStartIo)(_DEVICE_OBJECT *, _IRP *);
	void (*DriverUnload)(_DRIVER_OBJECT *);
	int (*MajorFunction[28])(_DEVICE_OBJECT *, _IRP *);
};

// TI = 0x14c0
struct _DEVICE_OBJECT {
	int16				Type;
	uint16				Size;
	int					ReferenceCount;
	_DRIVER_OBJECT		*DriverObject;
	_DEVICE_OBJECT		*NextDevice;
	_DEVICE_OBJECT		*AttachedDevice;
	_IRP				*CurrentIrp;
	_IO_TIMER			*Timer;
	uint32				Flags;
	uint32				Characteristics;
	_VPB				*Vpb;
	void				*DeviceExtension;
	uint32				DeviceType;
	char				StackSize;
	union  {
		_LIST_ENTRY			ListEntry;
		_WAIT_CONTEXT_BLOCK	Wcb;
	}					Queue;
	uint32				AlignmentRequirement;
	_KDEVICE_QUEUE		DeviceQueue;
	_KDPC				Dpc;
	uint32				ActiveThreadCount;
	void				*SecurityDescriptor;
	_KEVENT				DeviceLock;
	uint16				SectorSize;
	uint16				Spare1;
	_DEVOBJ_EXTENSION	*DeviceObjectExtension;
	void				*Reserved;
};

// TI = 0x1ab1
struct _VPB {
	int16			Type;
	int16			Size;
	uint16			Flags;
	uint16			VolumeLabelLength;
	_DEVICE_OBJECT	*DeviceObject;
	_DEVICE_OBJECT	*RealDevice;
	uint32			SerialNumber;
	uint32			ReferenceCount;
	wchar_t			VolumeLabel[64];
};

// TI = 0x1529
struct _FILE_OBJECT {
	int16						Type;
	int16						Size;
	_DEVICE_OBJECT				*DeviceObject;
	_VPB						*Vpb;
	void						*FsContext;
	void						*FsContext2;
	_SECTION_OBJECT_POINTERS	*SectionObjectPointer;
	void						*PrivateCacheMap;
	int							FinalStatus;
	_FILE_OBJECT				*RelatedFileObject;
	uint8						LockOperation;
	uint8						DeletePending;
	uint8						ReadAccess;
	uint8						WriteAccess;
	uint8						DeleteAccess;
	uint8						SharedRead;
	uint8						SharedWrite;
	uint8						SharedDelete;
	uint32						Flags;
	_UNICODE_STRING				FileName;
	_LARGE_INTEGER				CurrentByteOffset;
	uint32						Waiters;
	uint32						Busy;
	void						*LastLock;
	_KEVENT						Lock;
	_KEVENT						Event;
	_IO_COMPLETION_CONTEXT		*CompletionContext;
	uint64						IrpListLock;
	_LIST_ENTRY					IrpList;
	void						*FileObjectExtension;
};

// TI = 0x1410
enum _SYSTEM_DLL_TYPE {
	PsNativeSystemDll		= 0,
	PsWowX86SystemDll		= 1,
	PsWowArm32SystemDll		= 2,
	PsWowAmd64SystemDll		= 3,
	PsWowChpeX86SystemDll	= 4,
	PsVsmEnclaveRuntimeDll	= 5,
	PsSystemDllTotalTypes	= 6,
};

// TI = 0x1412
struct _EWOW64PROCESS {
	void				*Peb;
	uint16				Machine;
	_SYSTEM_DLL_TYPE	NtdllType;
};

// TI = 0x136a
struct _HANDLE_TABLE {
	uint32						NextHandleNeedingPool;
	int							ExtraInfoPages;
	volatile uint64				TableCode;
	_EPROCESS					*QuotaProcess;
	_LIST_ENTRY					HandleTableList;
	uint32						UniqueProcessId;
	union {
		uint32						Flags;
		struct {
			uint8						StrictFIFO:1;
			uint8						EnableHandleExceptions:1;
			uint8						Rundown:1;
			uint8						Duplicated:1;
			uint8						RaiseUMExceptionOnInvalidHandleClose:1;
		};
	};
	_EX_PUSH_LOCK				HandleContentionEvent;
	_EX_PUSH_LOCK				HandleTableLock;
	union {
		_HANDLE_TABLE_FREE_LIST		FreeLists[1];
		struct {
			uint8						ActualEntry[32];
			_HANDLE_TRACE_DEBUG_INFO	*DebugInfo;
		};
	};
};

// TI = 0x13de
struct _EPROCESS_QUOTA_BLOCK {};

// TI = 0x13dc
struct _MM_SESSION_SPACE {};

// TI = 0x13da
struct _PAGEFAULT_HISTORY {};

// TI = 0x1384
struct _EX_FAST_REF {
	union {
		void	*Object;
		uint64	RefCnt:4;
		uint64	Value;
	};
};

// TI = 0x1400
struct _EPROCESS {
	_KPROCESS									Pcb;
	_EX_PUSH_LOCK								ProcessLock;
	void										*UniqueProcessId;
	_LIST_ENTRY									ActiveProcessLinks;
	_EX_RUNDOWN_REF								RundownProtect;
	union {
		uint32										Flags2;
		struct {
			uint32										JobNotReallyActive:1;
			uint32										AccountingFolded:1;
			uint32										NewProcessReported:1;
			uint32										ExitProcessReported:1;
			uint32										ReportCommitChanges:1;
			uint32										LastReportMemory:1;
			uint32										ForceWakeCharge:1;
			uint32										CrossSessionCreate:1;
			uint32										NeedsHandleRundown:1;
			uint32										RefTraceEnabled:1;
			uint32										PicoCreated:1;
			uint32										EmptyJobEvaluated:1;
			uint32										DefaultPagePriority:3;
			uint32										PrimaryTokenFrozen:1;
			uint32										ProcessVerifierTarget:1;
			uint32										RestrictSetThreadContext:1;
			uint32										AffinityPermanent:1;
			uint32										AffinityUpdateEnable:1;
			uint32										PropagateNode:1;
			uint32										ExplicitAffinity:1;
			uint32										ProcessExecutionState:2;
			uint32										EnableReadVmLogging:1;
			uint32										EnableWriteVmLogging:1;
			uint32										FatalAccessTerminationRequested:1;
			uint32										DisableSystemAllowedCpuSet:1;
			uint32										ProcessStateChangeRequest:2;
			uint32										ProcessStateChangeInProgress:1;
			uint32										InPrivate:1;
		};
	};
	union {
		uint32										Flags;
		struct {
			uint32										CreateReported:1;
			uint32										NoDebugInherit:1;
			uint32										ProcessExiting:1;
			uint32										ProcessDelete:1;
			uint32										ManageExecutableMemoryWrites:1;
			uint32										VmDeleted:1;
			uint32										OutswapEnabled:1;
			uint32										Outswapped:1;
			uint32										FailFastOnCommitFail:1;
			uint32										Wow64VaSpace4Gb:1;
			uint32										AddressSpaceInitialized:2;
			uint32										SetTimerResolution:1;
			uint32										BreakOnTermination:1;
			uint32										DeprioritizeViews:1;
			uint32										WriteWatch:1;
			uint32										ProcessInSession:1;
			uint32										OverrideAddressSpace:1;
			uint32										HasAddressSpace:1;
			uint32										LaunchPrefetched:1;
			uint32										Background:1;
			uint32										VmTopDown:1;
			uint32										ImageNotifyDone:1;
			uint32										PdeUpdateNeeded:1;
			uint32										VdmAllowed:1;
			uint32										ProcessRundown:1;
			uint32										ProcessInserted:1;
			uint32										DefaultIoPriority:3;
			uint32										ProcessSelfDelete:1;
			uint32										SetTimerResolutionLink:1;
		};
	};
	_LARGE_INTEGER								CreateTime;
	uint64										ProcessQuotaUsage[2];
	uint64										ProcessQuotaPeak[2];
	uint64										PeakVirtualSize;
	uint64										VirtualSize;
	_LIST_ENTRY									SessionProcessLinks;
	union {
		void										*ExceptionPortData;
		uint64										ExceptionPortValue;
		uint64										ExceptionPortState:3;
	};
	_EX_FAST_REF								Token;
	uint64										MmReserved;
	_EX_PUSH_LOCK								AddressCreationLock;
	_EX_PUSH_LOCK								PageTableCommitmentLock;
	_ETHREAD									*RotateInProgress;
	_ETHREAD									*ForkInProgress;
	_EJOB										volatile *CommitChargeJob;
	_RTL_AVL_TREE								CloneRoot;
	volatile uint64								NumberOfPrivatePages;
	volatile uint64								NumberOfLockedPages;
	void										*Win32Process;
	_EJOB										volatile *Job;
	void										*SectionObject;
	void										*SectionBaseAddress;
	uint32										Cookie;
	struct _PAGEFAULT_HISTORY					*WorkingSetWatch;
	void										*Win32WindowStation;
	void										*InheritedFromUniqueProcessId;
	void										*Spare0;
	volatile uint64								OwnerProcessId;
	_PEB										*Peb;
	struct _MM_SESSION_SPACE					*Session;
	void										*Spare1;
	struct _EPROCESS_QUOTA_BLOCK				*QuotaBlock;
	_HANDLE_TABLE								*ObjectTable;
	void										*DebugPort;
	_EWOW64PROCESS								*WoW64Process;
	void										*DeviceMap;
	void										*EtwDataSource;
	uint64										PageDirectoryPte;
	_FILE_OBJECT								*ImageFilePointer;
	uint8										ImageFileName[15];
	uint8										PriorityClass;
	void										*SecurityPort;
	_SE_AUDIT_PROCESS_CREATION_INFO				SeAuditProcessCreationInfo;
	_LIST_ENTRY									JobLinks;
	void										*HighestUserAddress;
	_LIST_ENTRY									ThreadListHead;
	volatile uint32								ActiveThreads;
	uint32										ImagePathHash;
	uint32										DefaultHardErrorProcessing;
	int											LastThreadExitStatus;
	_EX_FAST_REF								PrefetchTrace;
	void										*LockedPagesList;
	_LARGE_INTEGER								ReadOperationCount;
	_LARGE_INTEGER								WriteOperationCount;
	_LARGE_INTEGER								OtherOperationCount;
	_LARGE_INTEGER								ReadTransferCount;
	_LARGE_INTEGER								WriteTransferCount;
	_LARGE_INTEGER								OtherTransferCount;
	uint64										CommitChargeLimit;
	volatile uint64								CommitCharge;
	volatile uint64								CommitChargePeak;
	_MMSUPPORT_FULL								Vm;
	_LIST_ENTRY									MmProcessLinks;
	uint32										ModifiedPageCount;
	int											ExitStatus;
	_RTL_AVL_TREE								VadRoot;
	void										*VadHint;
	uint64										VadCount;
	volatile uint64								VadPhysicalPages;
	uint64										VadPhysicalPagesLimit;
	_ALPC_PROCESS_CONTEXT						AlpcContext;
	_LIST_ENTRY									TimerResolutionLink;
	_PO_DIAG_STACK_RECORD						*TimerResolutionStackRecord;
	uint32										RequestedTimerResolution;
	uint32										SmallestTimerResolution;
	_LARGE_INTEGER								ExitTime;
	_INVERTED_FUNCTION_TABLE					*InvertedFunctionTable;
	_EX_PUSH_LOCK								InvertedFunctionTableLock;
	uint32										ActiveThreadsHighWatermark;
	uint32										LargePrivateVadCount;
	_EX_PUSH_LOCK								ThreadListLock;
	void										*WnfContext;
	_EJOB										*ServerSilo;
	uint8										SignatureLevel;
	uint8										SectionSignatureLevel;
	_PS_PROTECTION								Protection;
	uint8										HangCount:3;
	union {
		uint8										GhostCount:3;
		uint8										PrefilterException:1;
		uint32										Flags3;
		struct {
			uint32										Minimal:1;
			uint32										ReplacingPageRoot:1;
			uint32										Crashed:1;
			uint32										JobVadsAreTracked:1;
			uint32										VadTrackingDisabled:1;
			uint32										AuxiliaryProcess:1;
			uint32										SubsystemProcess:1;
			uint32										IndirectCpuSets:1;
			uint32										RelinquishedCommit:1;
			uint32										HighGraphicsPriority:1;
			uint32										CommitFailLogged:1;
			uint32										ReserveFailLogged:1;
			uint32										SystemProcess:1;
			uint32										HideImageBaseAddresses:1;
			uint32										AddressPolicyFrozen:1;
			uint32										ProcessFirstResume:1;
			uint32										ForegroundExternal:1;
			uint32										ForegroundSystem:1;
			uint32										HighMemoryPriority:1;
			uint32										EnableProcessSuspendResumeLogging:1;
			uint32										EnableThreadSuspendResumeLogging:1;
			uint32										SecurityDomainChanged:1;
			uint32										SecurityFreezeComplete:1;
			uint32										VmProcessorHost:1;
		};
	};
	int											DeviceAsid;
	void										*SvmData;
	_EX_PUSH_LOCK								SvmProcessLock;
	uint64										SvmLock;
	_LIST_ENTRY									SvmProcessDeviceListHead;
	uint64										LastFreezeInterruptTime;
	_PROCESS_DISK_COUNTERS						*DiskCounters;
	void										*PicoContext;
	void										*EnclaveTable;
	uint64										EnclaveNumber;
	_EX_PUSH_LOCK								EnclaveLock;
	uint32										HighPriorityFaultsAllowed;
	struct _PO_PROCESS_ENERGY_CONTEXT			*EnergyContext;
	void										*VmContext;
	uint64										SequenceNumber;
	uint64										CreateInterruptTime;
	uint64										CreateUnbiasedInterruptTime;
	uint64										TotalUnbiasedFrozenTime;
	uint64										LastAppStateUpdateTime;
	uint64										LastAppStateUptime:61;
	uint64										LastAppState:3;
	volatile uint64								SharedCommitCharge;
	_EX_PUSH_LOCK								SharedCommitLock;
	_LIST_ENTRY									SharedCommitLinks;
	union {
		struct {
			uint64										AllowedCpuSets;
			uint64										DefaultCpuSets;
		};
		struct {
			uint64										*AllowedCpuSetsIndirect;
			uint64										*DefaultCpuSetsIndirect;
		};
	};
	void										*DiskIoAttribution;
	void										*DxgProcess;
	uint32										Win32KFilterSet;
	volatile _PS_INTERLOCKED_TIMER_DELAY_VALUES	ProcessTimerDelay;
	volatile uint32								KTimerSets;
	volatile uint32								KTimer2Sets;
	volatile uint32								ThreadTimerSets;
	uint64										VirtualTimerListLock;
	_LIST_ENTRY									VirtualTimerListHead;
	union {
		_WNF_STATE_NAME								WakeChannel;
		_PS_PROCESS_WAKE_INFORMATION				WakeInfo;
	};
	char										_pdb_padding0[40];
	union {
		uint32										MitigationFlags;
		struct {
			uint32	ControlFlowGuardEnabled:1;
			uint32	ControlFlowGuardExportSuppressionEnabled:1;
			uint32	ControlFlowGuardStrict:1;
			uint32	DisallowStrippedImages:1;
			uint32	ForceRelocateImages:1;
			uint32	HighEntropyASLREnabled:1;
			uint32	StackRandomizationDisabled:1;
			uint32	ExtensionPointDisable:1;
			uint32	DisableDynamicCode:1;
			uint32	DisableDynamicCodeAllowOptOut:1;
			uint32	DisableDynamicCodeAllowRemoteDowngrade:1;
			uint32	AuditDisableDynamicCode:1;
			uint32	DisallowWin32kSystemCalls:1;
			uint32	AuditDisallowWin32kSystemCalls:1;
			uint32	EnableFilteredWin32kAPIs:1;
			uint32	AuditFilteredWin32kAPIs:1;
			uint32	DisableNonSystemFonts:1;
			uint32	AuditNonSystemFontLoading:1;
			uint32	PreferSystem32Images:1;
			uint32	ProhibitRemoteImageMap:1;
			uint32	AuditProhibitRemoteImageMap:1;
			uint32	ProhibitLowILImageMap:1;
			uint32	AuditProhibitLowILImageMap:1;
			uint32	SignatureMitigationOptIn:1;
			uint32	AuditBlockNonMicrosoftBinaries:1;
			uint32	AuditBlockNonMicrosoftBinariesAllowStore:1;
			uint32	LoaderIntegrityContinuityEnabled:1;
			uint32	AuditLoaderIntegrityContinuity:1;
			uint32	EnableModuleTamperingProtection:1;
			uint32	EnableModuleTamperingProtectionNoInherit:1;
			uint32	RestrictIndirectBranchPrediction:1;
			uint32	IsolateSecurityDomain:1;
		}											MitigationFlagsValues;
	};
	union {
		uint32										MitigationFlags2;
		struct {
			uint32	EnableExportAddressFilter:1;
			uint32	AuditExportAddressFilter:1;
			uint32	EnableExportAddressFilterPlus:1;
			uint32	AuditExportAddressFilterPlus:1;
			uint32	EnableRopStackPivot:1;
			uint32	AuditRopStackPivot:1;
			uint32	EnableRopCallerCheck:1;
			uint32	AuditRopCallerCheck:1;
			uint32	EnableRopSimExec:1;
			uint32	AuditRopSimExec:1;
			uint32	EnableImportAddressFilter:1;
			uint32	AuditImportAddressFilter:1;
			uint32	DisablePageCombine:1;
			uint32	SpeculativeStoreBypassDisable:1;
			uint32	CetShadowStacks:1;
		}											MitigationFlags2Values;
	};
	void										*PartitionObject;
	uint64										SecurityDomain;
	uint64										ParentSecurityDomain;
	void										*CoverageSamplerContext;
	void										*MmHotPatchContext;
};

// TI = 0x1396
enum _EX_GEN_RANDOM_DOMAIN {
	ExGenRandomDomainKernel			= 0,
	ExGenRandomDomainFirst			= 0,
	ExGenRandomDomainUserVisible	= 1,
	ExGenRandomDomainMax			= 2,
};

// TI = 0x1bbc
struct _ACCESS_REASONS {
	uint32	Data[32];
};

// TI = 0x1c38
struct _GENERIC_MAPPING {
	uint32	GenericRead;
	uint32	GenericWrite;
	uint32	GenericExecute;
	uint32	GenericAll;
};

// TI = 0x13ab
struct _AUX_ACCESS_DATA {
	_PRIVILEGE_SET		*PrivilegesUsed;
	_GENERIC_MAPPING	GenericMapping;
	uint32				AccessesToAudit;
	uint32				MaximumAuditMask;
	_GUID				TransactionId;
	void				*NewSecurityDescriptor;
	void				*ExistingSecurityDescriptor;
	void				*ParentSecurityDescriptor;
	void (*DeRefSecurityDescriptor)(void*, void*);
	void				*SDLock;
	_ACCESS_REASONS		AccessReasons;
	uint8				GenerateStagingEvents;
};

// TI = 0x1bfe
enum _SECURITY_OPERATION_CODE {
	SetSecurityDescriptor		= 0,
	QuerySecurityDescriptor		= 1,
	DeleteSecurityDescriptor	= 2,
	AssignSecurityDescriptor	= 3,
};

// TI = 0x1c20
struct _OB_EXTENDED_PARSE_PARAMETERS {
	uint16	Length;
	uint32	RestrictedAccessMask;
	_EJOB	*Silo;
};

// TI = 0x1bee
enum _OB_OPEN_REASON {
	ObCreateHandle		= 0,
	ObOpenHandle		= 1,
	ObDuplicateHandle	= 2,
	ObInheritHandle		= 3,
	ObMaxOpenReason		= 4,
};

// TI = 0x1c63
struct _OBJECT_DUMP_CONTROL {
	void	*Stream;
	uint32	Detail;
};

// TI = 0x1c0a
struct _OBJECT_TYPE_INITIALIZER {
	uint16				Length;
	union {
		uint16				ObjectTypeFlags;
		struct {
			uint8				CaseInsensitive:1;
			uint8				UnnamedObjectsOnly:1;
			uint8				UseDefaultObject:1;
			uint8				SecurityRequired:1;
			uint8				MaintainHandleCount:1;
			uint8				MaintainTypeList:1;
			uint8				SupportsObjectCallbacks:1;
			uint8				CacheAligned:1;
			uint8				UseExtendedParameters:1;
			uint8				Reserved:7;
		};
	};
	uint32				ObjectTypeCode;
	uint32				InvalidAttributes;
	_GENERIC_MAPPING	GenericMapping;
	uint32				ValidAccessMask;
	uint32				RetainAccess;
	_POOL_TYPE			PoolType;
	uint32				DefaultPagedPoolCharge;
	uint32				DefaultNonPagedPoolCharge;
	void (*DumpProcedure)(void*, _OBJECT_DUMP_CONTROL *);
	int (*OpenProcedure)(_OB_OPEN_REASON, char, _EPROCESS *, void*, uint32*, uint32);
	void (*CloseProcedure)(_EPROCESS *, void*, uint64, uint64);
	void (*DeleteProcedure)(void*);
	union {
		int (*ParseProcedure)(void*, void*, _ACCESS_STATE *, char, uint32, _UNICODE_STRING *, _UNICODE_STRING *, void*, _SECURITY_QUALITY_OF_SERVICE *, void **);
		int (*ParseProcedureEx)(void*, void*, _ACCESS_STATE *, char, uint32, _UNICODE_STRING *, _UNICODE_STRING *, void*, _SECURITY_QUALITY_OF_SERVICE *, _OB_EXTENDED_PARSE_PARAMETERS *, void **);
	};
	int (*SecurityProcedure)(void*, _SECURITY_OPERATION_CODE, uint32*, void*, uint32*, void **, _POOL_TYPE, _GENERIC_MAPPING *, char);
	int (*QueryNameProcedure)(void*, uint8, _OBJECT_NAME_INFORMATION *, uint32, uint32*, char);
	uint8 (*OkayToCloseProcedure)(_EPROCESS *, void*, void*, char);
	uint32				WaitObjectFlagMask;
	uint16				WaitObjectFlagOffset;
	uint16				WaitObjectPointerOffset;
};

// TI = 0x1947
struct _OBJECT_TYPE {
	_LIST_ENTRY					TypeList;
	_UNICODE_STRING				Name;
	void						*DefaultObject;
	uint8						Index;
	uint32						TotalNumberOfObjects;
	uint32						TotalNumberOfHandles;
	uint32						HighWaterNumberOfObjects;
	uint32						HighWaterNumberOfHandles;
	_OBJECT_TYPE_INITIALIZER	TypeInfo;
	_EX_PUSH_LOCK				TypeLock;
	uint32						Key;
	_LIST_ENTRY					CallbackList;
};

// TI = 0x13b7
struct _OBJECT_HANDLE_INFORMATION {
	uint32	HandleAttributes;
	uint32	GrantedAccess;
};

// TI = 0x1bd7
struct _TXN_PARAMETER_BLOCK {
	uint16	Length;
	uint16	TxFsContext;
	void	*TransactionObject;
};

// TI = 0x14d2
struct _ECP_LIST {};

// TI = 0x14d7
struct _IO_DRIVER_CREATE_CONTEXT {
	int16					Size;
	struct _ECP_LIST		*ExtraCreateParameter;
	void					*DeviceObjectHint;
	_TXN_PARAMETER_BLOCK	*TxnParameters;
	_EJOB					*SiloContext;
};

// TI = 0x14f5
enum _IO_PRIORITY_HINT {
	IoPriorityVeryLow	= 0,
	IoPriorityLow		= 1,
	IoPriorityNormal	= 2,
	IoPriorityHigh		= 3,
	IoPriorityCritical	= 4,
	MaxIoPriorityTypes	= 5,
};

// TI = 0x14f7
struct _IO_PRIORITY_INFO {
	uint32				Size;
	uint32				ThreadPriority;
	uint32				PagePriority;
	_IO_PRIORITY_HINT	IoPriority;
};

// TI = 0x14fd
enum _MEMORY_CACHING_TYPE {
	MmNonCached					= 0,
	MmCached					= 1,
	MmWriteCombined				= 2,
	MmHardwareCoherentCached	= 3,
	MmNonCachedUnordered		= 4,
	MmUSWCCached				= 5,
	MmMaximumCacheType			= 6,
	MmNotMapped					= -1,
};

// TI = 0x1507
struct _EVENT_DATA_DESCRIPTOR {
	uint64	Ptr;
	uint32	Size;
	union {
		uint32	Reserved;
		struct {
			uint8	Type;
			uint8	Reserved1;
			uint16	Reserved2;
		};
	};
};

// TI = 0x150b
struct _EVENT_DESCRIPTOR {
	uint16	Id;
	uint8	Version;
	uint8	Channel;
	uint8	Level;
	uint8	Opcode;
	uint16	Task;
	uint64	Keyword;
};

// TI = 0x1900
struct _EVENT_HEADER_EXTENDED_DATA_ITEM {
	uint16	Reserved1;
	uint16	ExtType;
	uint16	Linkage:1;
	uint16	Reserved2:15;
	uint16	DataSize;
	uint64	DataPtr;
};

// TI = 0x1bba
struct _ETW_BUFFER_CONTEXT {
	union {
		struct {
			uint8	ProcessorNumber;
			uint8	Alignment;
		};
		uint16	ProcessorIndex;
	};
	uint16	LoggerId;
};

// TI = 0x1a5e
struct _EVENT_HEADER {
	uint16				Size;
	uint16				HeaderType;
	uint16				Flags;
	uint16				EventProperty;
	uint32				ThreadId;
	uint32				ProcessId;
	_LARGE_INTEGER		TimeStamp;
	_GUID				ProviderId;
	_EVENT_DESCRIPTOR	EventDescriptor;
	union {
		struct {
			uint32				KernelTime;
			uint32				UserTime;
		};
		uint64				ProcessorTime;
	};
	_GUID				ActivityId;
};

// TI = 0x1516
struct _EVENT_RECORD {
	_EVENT_HEADER						EventHeader;
	_ETW_BUFFER_CONTEXT					BufferContext;
	uint16								ExtendedDataCount;
	uint16								UserDataLength;
	_EVENT_HEADER_EXTENDED_DATA_ITEM	*ExtendedData;
	void								*UserData;
	void								*UserContext;
};

// TI = 0x151f
struct _PERFINFO_GROUPMASK {
	uint32	Masks[8];
};

// TI = 0x19f8
union _MM_PAGE_ACCESS_INFO_FLAGS {
	struct {
		uint32	FilePointerIndex:9;
		uint32	HardFault:1;
		uint32	Image:1;
		uint32	Spare0:1;
	}		File;
	struct {
		uint32	FilePointerIndex:9;
		uint32	HardFault:1;
		uint32	Spare1:2;
	}		Private;
};

// TI = 0x19ea
struct _MM_PAGE_ACCESS_INFO {
	union {
		_MM_PAGE_ACCESS_INFO_FLAGS	Flags;
		uint64						FileOffset;
		void						*VirtualAddress;
		void						*PointerProtoPte;
	};
};

// TI = 0x1534
enum _MM_PAGE_ACCESS_TYPE {
	MmPteAccessType			= 0,
	MmCcReadAheadType		= 1,
	MmPfnRepurposeType		= 2,
	MmMaximumPageAccessType	= 3,
};

// TI = 0x1538
struct _MM_PAGE_ACCESS_INFO_HEADER {
	_SINGLE_LIST_ENTRY		Link;
	_MM_PAGE_ACCESS_TYPE	Type;
	union {
		uint32					EmptySequenceNumber;
		uint32					CurrentFileIndex;
	};
	uint64					CreateTime;
	union {
		uint64					EmptyTime;
		_MM_PAGE_ACCESS_INFO	*TempEntry;
	};
	union {
		struct {
			_MM_PAGE_ACCESS_INFO	*PageEntry;
			uint64					*FileEntry;
			uint64					*FirstFileEntry;
			_EPROCESS				*Process;
			uint32					SessionId;
		};
		struct {
			uint64					*PageFrameEntry;
			uint64					*LastPageFrameEntry;
		};
	};
};

// TI = 0x154e
enum _PF_FILE_ACCESS_TYPE {
	PfFileAccessTypeRead	= 0,
	PfFileAccessTypeWrite	= 1,
	PfFileAccessTypeMax		= 2,
};

// TI = 0x155d
enum _DEVICE_WAKE_DEPTH {
	DeviceWakeDepthNotWakeable	= 0,
	DeviceWakeDepthD0			= 1,
	DeviceWakeDepthD1			= 2,
	DeviceWakeDepthD2			= 3,
	DeviceWakeDepthD3hot		= 4,
	DeviceWakeDepthD3cold		= 5,
	DeviceWakeDepthMaximum		= 6,
};

// TI = 0x1565
struct _MCUPDATE_INFO {
	_LIST_ENTRY	List;
	uint32		Status;
	uint64		Id;
	uint64		VendorScratch[2];
};

// TI = 0x157e
union _WHEA_EVENT_LOG_ENTRY_FLAGS {
	struct {
		uint32	LogTelemetry:1;
		uint32	LogInternalEtw:1;
		uint32	LogBlackbox:1;
		uint32	Reserved:29;
	};
	uint32	AsULONG;
};

// TI = 0x1578
enum _WHEA_EVENT_LOG_ENTRY_ID {
	WheaEventLogEntryIdCmcPollingTimeout		= -2147483647,
	WheaEventLogEntryIdWheaInit					= -2147483646,
	WheaEventLogEntryIdCmcSwitchToPolling		= -2147483645,
	WheaEventLogEntryIdDroppedCorrectedError	= -2147483644,
	WheaEventLogEntryIdStartedReportHwError		= -2147483643,
};

// TI = 0x1575
enum _WHEA_EVENT_LOG_ENTRY_TYPE {
	WheaEventLogEntryTypeInformational	= 0,
	WheaEventLogEntryTypeWarning		= 1,
	WheaEventLogEntryTypeError			= 2,
};

// TI = 0x1b0a
struct _WHEA_EVENT_LOG_ENTRY_HEADER {
	uint32						Signature;
	uint32						Version;
	uint32						Length;
	_WHEA_EVENT_LOG_ENTRY_TYPE	Type;
	uint32						OwnerTag;
	_WHEA_EVENT_LOG_ENTRY_ID	Id;
	_WHEA_EVENT_LOG_ENTRY_FLAGS	Flags;
	uint32						PayloadLength;
};

// TI = 0x1573
struct _WHEA_EVENT_LOG_ENTRY {
	_WHEA_EVENT_LOG_ENTRY_HEADER	Header;
};

// TI = 0x1589
enum _WHEA_ERROR_PACKET_DATA_FORMAT {
	WheaDataFormatIPFSalRecord	= 0,
	WheaDataFormatXPFMCA		= 1,
	WheaDataFormatMemory		= 2,
	WheaDataFormatPCIExpress	= 3,
	WheaDataFormatNMIPort		= 4,
	WheaDataFormatPCIXBus		= 5,
	WheaDataFormatPCIXDevice	= 6,
	WheaDataFormatGeneric		= 7,
	WheaDataFormatMax			= 8,
};

// TI = 0x1587
enum _WHEA_ERROR_SOURCE_TYPE {
	WheaErrSrcTypeMCE			= 0,
	WheaErrSrcTypeCMC			= 1,
	WheaErrSrcTypeCPE			= 2,
	WheaErrSrcTypeNMI			= 3,
	WheaErrSrcTypePCIe			= 4,
	WheaErrSrcTypeGeneric		= 5,
	WheaErrSrcTypeINIT			= 6,
	WheaErrSrcTypeBOOT			= 7,
	WheaErrSrcTypeSCIGeneric	= 8,
	WheaErrSrcTypeIPFMCA		= 9,
	WheaErrSrcTypeIPFCMC		= 10,
	WheaErrSrcTypeIPFCPE		= 11,
	WheaErrSrcTypeGenericV2		= 12,
	WheaErrSrcTypeSCIGenericV2	= 13,
	WheaErrSrcTypeMax			= 14,
};

// TI = 0x1585
enum _WHEA_ERROR_SEVERITY {
	WheaErrSevRecoverable	= 0,
	WheaErrSevFatal			= 1,
	WheaErrSevCorrected		= 2,
	WheaErrSevInformational	= 3,
};

// TI = 0x1583
enum _WHEA_ERROR_TYPE {
	WheaErrTypeProcessor	= 0,
	WheaErrTypeMemory		= 1,
	WheaErrTypePCIExpress	= 2,
	WheaErrTypeNMI			= 3,
	WheaErrTypePCIXBus		= 4,
	WheaErrTypePCIXDevice	= 5,
	WheaErrTypeGeneric		= 6,
};

// TI = 0x1b83
union _WHEA_ERROR_PACKET_FLAGS {
	struct {
		uint32	PreviousError:1;
		uint32	Reserved1:1;
		uint32	HypervisorError:1;
		uint32	Simulated:1;
		uint32	PlatformPfaControl:1;
		uint32	PlatformDirectedOffline:1;
		uint32	Reserved2:26;
	};
	uint32	AsULONG;
};

// TI = 0x158b
struct _WHEA_ERROR_PACKET_V2 {
	uint32							Signature;
	uint32							Version;
	uint32							Length;
	_WHEA_ERROR_PACKET_FLAGS		Flags;
	_WHEA_ERROR_TYPE				ErrorType;
	_WHEA_ERROR_SEVERITY			ErrorSeverity;
	uint32							ErrorSourceId;
	_WHEA_ERROR_SOURCE_TYPE			ErrorSourceType;
	_GUID							NotifyType;
	uint64							Context;
	_WHEA_ERROR_PACKET_DATA_FORMAT	DataFormat;
	uint32							Reserved1;
	uint32							DataOffset;
	uint32							DataLength;
	uint32							PshedDataOffset;
	uint32							PshedDataLength;
};

// TI = 0x1b08
union _WHEA_ERROR_RECORD_SECTION_DESCRIPTOR_FLAGS {
	struct {
		uint32	Primary:1;
		uint32	ContainmentWarning:1;
		uint32	Reset:1;
		uint32	ThresholdExceeded:1;
		uint32	ResourceNotAvailable:1;
		uint32	LatentError:1;
		uint32	Propagated:1;
		uint32	Reserved:25;
	};
	uint32	AsULONG;
};

// TI = 0x1b66
union _WHEA_ERROR_RECORD_SECTION_DESCRIPTOR_VALIDBITS {
	struct {
		uint8	FRUId:1;
		uint8	FRUText:1;
		uint8	Reserved:6;
	};
	uint8	AsUCHAR;
};

// TI = 0x1a42
union _WHEA_REVISION {
	struct {
		uint8	MinorRevision;
		uint8	MajorRevision;
	};
	uint16	AsUSHORT;
};

// TI = 0x159a
struct _WHEA_ERROR_RECORD_SECTION_DESCRIPTOR {
	uint32											SectionOffset;
	uint32											SectionLength;
	_WHEA_REVISION									Revision;
	_WHEA_ERROR_RECORD_SECTION_DESCRIPTOR_VALIDBITS	ValidBits;
	uint8											Reserved;
	_WHEA_ERROR_RECORD_SECTION_DESCRIPTOR_FLAGS		Flags;
	_GUID											SectionType;
	_GUID											FRUId;
	_WHEA_ERROR_SEVERITY							SectionSeverity;
	char											FRUText[20];
};

// TI = 0x1c36
union _WHEA_PERSISTENCE_INFO {
	struct {
		uint64	Signature:16;
		uint64	Length:24;
		uint64	Identifier:16;
		uint64	Attributes:2;
		uint64	DoNotLog:1;
		uint64	Reserved:5;
	};
	uint64	AsULONGLONG;
};

// TI = 0x1d3c
union _WHEA_ERROR_RECORD_HEADER_FLAGS {
	struct {
		uint32	Recovered:1;
		uint32	PreviousError:1;
		uint32	Simulated:1;
		uint32	Reserved:29;
	};
	uint32	AsULONG;
};

// TI = 0x1bca
union _WHEA_TIMESTAMP {
	struct {
		uint64			Seconds:8;
		uint64			Minutes:8;
		uint64			Hours:8;
		uint64			Precise:1;
		uint64			Reserved:7;
		uint64			Day:8;
		uint64			Month:8;
		uint64			Year:8;
		uint64			Century:8;
	};
	_LARGE_INTEGER	AsLARGE_INTEGER;
};

// TI = 0x1ba2
union _WHEA_ERROR_RECORD_HEADER_VALIDBITS {
	struct {
		uint32	PlatformId:1;
		uint32	Timestamp:1;
		uint32	PartitionId:1;
		uint32	Reserved:29;
	};
	uint32	AsULONG;
};

// TI = 0x1ae1
struct _WHEA_ERROR_RECORD_HEADER {
	uint32								Signature;
	_WHEA_REVISION						Revision;
	uint32								SignatureEnd;
	uint16								SectionCount;
	_WHEA_ERROR_SEVERITY				Severity;
	_WHEA_ERROR_RECORD_HEADER_VALIDBITS	ValidBits;
	uint32								Length;
	_WHEA_TIMESTAMP						Timestamp;
	_GUID								PlatformId;
	_GUID								PartitionId;
	_GUID								CreatorId;
	_GUID								NotifyType;
	uint64								RecordId;
	_WHEA_ERROR_RECORD_HEADER_FLAGS		Flags;
	_WHEA_PERSISTENCE_INFO				PersistenceInfo;
	uint8								Reserved[12];
};

// TI = 0x1592
struct _WHEA_ERROR_RECORD {
	_WHEA_ERROR_RECORD_HEADER				Header;
	_WHEA_ERROR_RECORD_SECTION_DESCRIPTOR	SectionDescriptor[1];
};

// TI = 0x15e3
struct _HEAP_SUBALLOCATOR_CALLBACKS {
	uint64	Allocate;
	uint64	Free;
	uint64	Commit;
	uint64	Decommit;
	uint64	ExtendContext;
};

// TI = 0x15f5
struct _SEGMENT_HEAP_EXTRA {
	uint16	AllocationTag;
	uint8	InterceptorIndex:4;
	uint8	UserFlags:4;
	uint8	ExtraSizeInUnits;
	void	*Settable;
};

// TI = 0x1600
struct _RTL_CSPARSE_BITMAP {
	uint64	*CommitBitmap;
	uint64	*UserBitmap;
	int64	BitCount;
	uint64	BitmapLock;
	uint64	DecommitPageIndex;
	uint64	RtlpCSparseBitmapWakeLock;
	uint8	LockType;
	uint8	AddressSpace;
	uint8	MemType;
	uint8	AllocAlignment;
	uint32	CommitDirectoryMaxSize;
	uint64	CommitDirectory[1];
};

// TI = 0x1603
enum RTLP_CSPARSE_BITMAP_STATE {
	CommitBitmapInvalid	= 0,
	UserBitmapInvalid	= 1,
	UserBitmapValid		= 2,
};

// TI = 0x1607
struct _RTL_SPARSE_ARRAY {
	uint64				ElementCount;
	uint32				ElementSizeShift;
	_RTL_CSPARSE_BITMAP	Bitmap;
};

// TI = 0x1610
enum _RTLP_HP_ADDRESS_SPACE_TYPE {
	HeapAddressUser		= 0,
	HeapAddressKernel	= 1,
	HeapAddressSession	= 2,
	HeapAddressTypeMax	= 3,
};

// TI = 0x1613
struct _HEAP_VAMGR_VASPACE {
	_RTLP_HP_ADDRESS_SPACE_TYPE	AddressSpaceType;
	uint64						BaseAddress;
	union {
		_RTL_SPARSE_ARRAY			VaRangeArray;
		uint8						VaRangeArrayBuffer[2128];
	};
};

// TI = 0x160e
struct _HEAP_VAMGR_ALLOCATOR {
	uint64				TreeLock;
	_RTL_RB_TREE		FreeRanges;
	_HEAP_VAMGR_VASPACE	*VaSpace;
	void				*PartitionHandle;
	uint16				ChunksPerRegion;
	uint16				RefCount;
	uint8				AllocatorIndex;
	uint8				NumaNode;
	uint8				LockType:1;
	uint8				MemoryType:2;
	uint8				ConstrainedVA:1;
	uint8				AllowFreeHead:1;
	uint8				Spare0:3;
	uint8				Spare1;
};

// TI = 0x161b
struct _HEAP_VAMGR_RANGE {
	union {
		_RTL_BALANCED_NODE	RbNode;
		_SINGLE_LIST_ENTRY	Next;
		struct {
			uint8				Allocated:1;
			uint8				Internal:1;
			uint8				Standalone:1;
			uint8				Spare0:5;
			uint8				AllocatorIndex;
			uint64				OwnerCtx[2];
		};
	};
	union {
		uint64				SizeInChunks;
		struct {
			uint16				ChunkCount;
			uint16				PrevChunkCount;
		};
		uint64				Signature;
	};
};

// TI = 0x1626
enum _RTLP_HP_LOCK_TYPE {
	HeapLockPaged		= 0,
	HeapLockNonPaged	= 1,
	HeapLockTypeMax		= 2,
};

// TI = 0x163e
struct _RTLP_HP_ALLOC_TRACKER {
	uint64				BaseAddress;
	union {
		_RTL_CSPARSE_BITMAP	AllocTrackerBitmap;
		uint8				AllocTrackerBitmapBuffer[72];
	};
};

// TI = 0x1959
struct _RTL_HP_VS_CONFIG {
	struct {
		uint32	PageAlignLargeAllocs:1;
		uint32	FullDecommit:1;
		uint32	EnableDelayFree:1;
	}		Flags;
};

// TI = 0x1a08
struct _RTL_HP_LFH_CONFIG {
	uint16	MaxBlockSize;
	uint16	WitholdPageCrossingBlocks:1;
};

// TI = 0x1b54
struct _RTL_HP_SUB_ALLOCATOR_CONFIGS {
	_RTL_HP_LFH_CONFIG	LfhConfigs;
	_RTL_HP_VS_CONFIG	VsConfigs;
};

// TI = 0x1953
union _RTL_RUN_ONCE {
	void	*Ptr;
	uint64	Value;
	uint64	State:2;
};

// TI = 0x179f
struct _HEAP_LFH_FAST_REF {
	union {
		void	*Target;
		uint64	Value;
		uint64	RefCount:12;
	};
};

// TI = 0x17a4
struct _HEAP_LFH_SUBSEGMENT_OWNER {
	uint8		IsBucket:1;
	uint8		Spare0:7;
	uint8		BucketIndex;
	union {
		uint8		SlotCount;
		uint8		SlotIndex;
	};
	uint8		Spare1;
	uint64		AvailableSubsegmentCount;
	uint64		Lock;
	_LIST_ENTRY	AvailableSubsegmentList;
	_LIST_ENTRY	FullSubsegmentList;
};

// TI = 0x1a0e
struct _HEAP_LFH_AFFINITY_SLOT {
	_HEAP_LFH_SUBSEGMENT_OWNER	State;
	_HEAP_LFH_FAST_REF			ActiveSubsegment;
};

// TI = 0x17b4
struct _HEAP_LFH_BUCKET {
	_HEAP_LFH_SUBSEGMENT_OWNER	State;
	uint64						TotalBlockCount;
	uint64						TotalSubsegmentCount;
	uint32						ReciprocalBlockSize;
	uint8						Shift;
	uint8						ContentionCount;
	uint64						AffinityMappingLock;
	uint8						*ProcAffinityMapping;
	_HEAP_LFH_AFFINITY_SLOT		**AffinitySlots;
};

// TI = 0x1b12
struct _HEAP_LFH_SUBSEGMENT_STAT {
	uint8	Index;
	uint8	Count;
};

// TI = 0x1a48
union _HEAP_LFH_SUBSEGMENT_STATS {
	_HEAP_LFH_SUBSEGMENT_STAT	Buckets[4];
	void						*AllStats;
};

// TI = 0x17ad
struct _HEAP_LFH_CONTEXT {
	void							*BackendCtx;
	_HEAP_SUBALLOCATOR_CALLBACKS	Callbacks;
	const uint8						*AffinityModArray;
	uint8							MaxAffinity;
	uint8							LockType;
	int16							MemStatsOffset;
	_RTL_HP_LFH_CONFIG				Config;
	_HEAP_LFH_SUBSEGMENT_STATS		BucketStats;
	uint64							SubsegmentCreationLock;
	char							_pdb_padding0[48];
	_HEAP_LFH_BUCKET				*Buckets[129];
};

// TI = 0x19d0
struct _HEAP_VS_DELAY_FREE_CONTEXT {
	_SLIST_HEADER	ListHead;
};

// TI = 0x17fe
struct _HEAP_VS_CONTEXT {
	uint64							Lock;
	_RTLP_HP_LOCK_TYPE				LockType;
	_RTL_RB_TREE					FreeChunkTree;
	_LIST_ENTRY						SubsegmentList;
	uint64							TotalCommittedUnits;
	uint64							FreeCommittedUnits;
	_HEAP_VS_DELAY_FREE_CONTEXT		DelayFreeContext;
	char							_pdb_padding0[48];
	void							*BackendCtx;
	_HEAP_SUBALLOCATOR_CALLBACKS	Callbacks;
	_RTL_HP_VS_CONFIG				Config;
	uint32							Flags;
};

// TI = 0x1861
struct RTL_HP_ENV_HANDLE {
	void	*h[2];
};

// TI = 0x1849
struct _HEAP_SEG_CONTEXT {
	uint64				SegmentMask;
	uint8				UnitShift;
	uint8				PagesPerUnitShift;
	uint8				FirstDescriptorIndex;
	uint8				CachedCommitSoftShift;
	uint8				CachedCommitHighShift;
	union  {
		struct {
			uint8	LargePagePolicy:3;
			uint8	FullDecommit:1;
			uint8	ReleaseEmptySegments:1;
		};
		uint8	AllFlags;
	}					Flags;
	uint32				MaxAllocationSize;
	int16				OlpStatsOffset;
	int16				MemStatsOffset;
	void				*LfhContext;
	void				*VsContext;
	RTL_HP_ENV_HANDLE	EnvHandle;
	void				*Heap;
	uint64				SegmentLock;
	_LIST_ENTRY			SegmentListHead;
	uint64				SegmentCount;
	_RTL_RB_TREE		FreePageRanges;
	uint64				FreeSegmentListLock;
	_SINGLE_LIST_ENTRY	FreeSegmentList[2];
};

// TI = 0x1a25
struct _RTL_HP_SEG_ALLOC_POLICY {
	uint64	MinLargePages;
	uint64	MaxLargePages;
	uint8	MinUtilization;
};

// TI = 0x1bd2
struct _HEAP_OPPORTUNISTIC_LARGE_PAGE_STATS {
	volatile uint64	SmallPagesInUseWithinLarge;
	volatile uint64	OpportunisticLargePageCount;
};

// TI = 0x1856
struct _HEAP_RUNTIME_MEMORY_STATS {
	volatile uint64							TotalReservedPages;
	volatile uint64							TotalCommittedPages;
	uint64									FreeCommittedPages;
	uint64									LfhFreeCommittedPages;
	_HEAP_OPPORTUNISTIC_LARGE_PAGE_STATS	LargePageStats[2];
	_RTL_HP_SEG_ALLOC_POLICY				LargePageUtilizationPolicy;
};

// TI = 0x1904
struct _RTL_HEAP_MEMORY_LIMIT_DATA {
	uint64	CommitLimitBytes;
	uint64	CommitLimitFailureCode;
	uint64	MaxAllocationSizeBytes;
	uint64	AllocationLimitFailureCode;
};

// TI = 0x1874
struct _SEGMENT_HEAP {
	RTL_HP_ENV_HANDLE			EnvHandle;
	uint32						Signature;
	uint32						GlobalFlags;
	uint32						Interceptor;
	uint16						ProcessHeapListIndex;
	uint16						AllocatedFromMetadata:1;
	union {
		_RTL_HEAP_MEMORY_LIMIT_DATA	CommitLimitData;
		struct {
			uint64						ReservedMustBeZero1;
			void						*UserContext;
			uint64						ReservedMustBeZero2;
			void						*Spare;
		};
	};
	uint64						LargeMetadataLock;
	_RTL_RB_TREE				LargeAllocMetadata;
	volatile uint64				LargeReservedPages;
	volatile uint64				LargeCommittedPages;
	_RTL_RUN_ONCE				StackTraceInitVar;
	char						_pdb_padding0[16];
	_HEAP_RUNTIME_MEMORY_STATS	MemStats;
	uint16						GlobalLockCount;
	uint32						GlobalLockOwner;
	uint64						ContextExtendLock;
	uint8						*AllocatedBase;
	uint8						*UncommittedBase;
	uint8						*ReservedLimit;
	_HEAP_SEG_CONTEXT			SegContexts[2];
	_HEAP_VS_CONTEXT			VsContext;
	_HEAP_LFH_CONTEXT			LfhContext;
};

// TI = 0x1a97
struct _RTLP_HP_METADATA_HEAP_CTX {
	_SEGMENT_HEAP	*Heap;
	_RTL_RUN_ONCE	InitOnce;
};

// TI = 0x1a82
struct _HEAP_VAMGR_CTX {
	_HEAP_VAMGR_VASPACE		VaSpace;
	uint64					AllocatorLock;
	uint32					AllocatorCount;
	_HEAP_VAMGR_ALLOCATOR	Allocators[255];
};

// TI = 0x1d1e
struct _FAKE_HEAP_ENTRY {
	uint64	Size;
	uint64	PreviousSize;
};

// TI = 0x168d
enum _HEAP_FAILURE_TYPE {
	heap_failure_internal						= 0,
	heap_failure_unknown						= 1,
	heap_failure_generic						= 2,
	heap_failure_entry_corruption				= 3,
	heap_failure_multiple_entries_corruption	= 4,
	heap_failure_virtual_block_corruption		= 5,
	heap_failure_buffer_overrun					= 6,
	heap_failure_buffer_underrun				= 7,
	heap_failure_block_not_busy					= 8,
	heap_failure_invalid_argument				= 9,
	heap_failure_invalid_allocation_type		= 10,
	heap_failure_usage_after_free				= 11,
	heap_failure_cross_heap_operation			= 12,
	heap_failure_freelists_corruption			= 13,
	heap_failure_listentry_corruption			= 14,
	heap_failure_lfh_bitmap_mismatch			= 15,
	heap_failure_segment_lfh_bitmap_corruption	= 16,
	heap_failure_segment_lfh_double_free		= 17,
	heap_failure_vs_subsegment_corruption		= 18,
	heap_failure_null_heap						= 19,
	heap_failure_allocation_limit				= 20,
	heap_failure_commit_limit					= 21,
};

// TI = 0x1b4a
struct _HEAP_FAILURE_INFORMATION {
	uint32				Version;
	uint32				StructureSize;
	_HEAP_FAILURE_TYPE	FailureType;
	void				*HeapAddress;
	void				*Address;
	void				*Param1;
	void				*Param2;
	void				*Param3;
	void				*PreviousBlock;
	void				*NextBlock;
	_FAKE_HEAP_ENTRY	ExpectedDecodedEntry;
	void				*StackTrace[32];
	uint8				HeapMajorVersion;
	uint8				HeapMinorVersion;
	_EXCEPTION_RECORD	ExceptionRecord;
	char				_pdb_padding0[8];
	_CONTEXT			ContextRecord;
};

// TI = 0x19c4
struct _RTLP_HP_HEAP_GLOBALS {
	uint64						HeapKey;
	uint64						LfhKey;
	_HEAP_FAILURE_INFORMATION	*FailureInfo;
	_RTL_HEAP_MEMORY_LIMIT_DATA	CommitLimitData;
};

// TI = 0x163b
struct _RTLP_HP_HEAP_MANAGER {
	_RTLP_HP_HEAP_GLOBALS			*Globals;
	_RTLP_HP_ALLOC_TRACKER			AllocTracker;
	_HEAP_VAMGR_CTX					VaMgr;
	_RTLP_HP_METADATA_HEAP_CTX		MetadataHeaps[3];
	_RTL_HP_SUB_ALLOCATOR_CONFIGS	SubAllocConfigs;
};

// TI = 0x1642 - forward reference
struct _HEAP_LIST_LOOKUP;

// TI = 0x1645
struct _HEAP_LIST_LOOKUP {
	_HEAP_LIST_LOOKUP	*ExtendedLookup;
	uint32				ArraySize;
	uint32				ExtraItem;
	uint32				ItemCount;
	uint32				OutOfRangeItems;
	uint32				BaseIndex;
	_LIST_ENTRY			*ListHead;
	uint32				*ListsInUseUlong;
	_LIST_ENTRY			**ListHints;
};

// TI = 0x1944
struct _HEAP_TUNING_PARAMETERS {
	uint32	CommittThresholdShift;
	uint64	MaxPreCommittThreshold;
};

// TI = 0x18ea
struct _HEAP_COUNTERS {
	uint64	TotalMemoryReserved;
	uint64	TotalMemoryCommitted;
	uint64	TotalMemoryLargeUCR;
	uint64	TotalSizeInVirtualBlocks;
	uint32	TotalSegments;
	uint32	TotalUCRs;
	uint32	CommittOps;
	uint32	DeCommitOps;
	uint32	LockAcquires;
	uint32	LockCollisions;
	uint32	CommitRate;
	uint32	DecommittRate;
	uint32	CommitFailures;
	uint32	InBlockCommitFailures;
	uint32	PollIntervalCounter;
	uint32	DecommitsSinceLastCheck;
	uint32	HeapPollInterval;
	uint32	AllocAndFreeOps;
	uint32	AllocationIndicesActive;
	uint32	InBlockDeccommits;
	uint64	InBlockDeccomitSize;
	uint64	HighWatermarkSize;
	uint64	LastPolledSize;
};

// TI = 0x1669
struct _HEAP_LOCK {
	union  {
		_RTL_CRITICAL_SECTION	CriticalSection;
	}		Lock;
};

// TI = 0x18ee
struct _HEAP_PSEUDO_TAG_ENTRY {
	uint32	Allocs;
	uint32	Frees;
	uint64	Size;
};

// TI = 0x18e4
struct _HEAP_TAG_ENTRY {
	uint32	Allocs;
	uint32	Frees;
	uint64	Size;
	uint16	TagIndex;
	uint16	CreatorBackTraceIndex;
	wchar_t	TagName[48];
};

// TI = 0x1c8d
struct _HEAP_EXTENDED_ENTRY {
	void	*Reserved;
	union {
		struct {
			uint16	FunctionIndex;
			uint16	ContextValue;
		};
		uint32	InterceptorValue;
	};
	uint16	UnusedBytesLength;
	uint8	EntryOffset;
	uint8	ExtendedBlockSignature;
};

// TI = 0x1c99
struct _HEAP_UNPACKED_ENTRY {
	void	*PreviousBlockPrivateData;
	union {
		struct {
			uint16	Size;
			uint8	Flags;
			uint8	SmallTagIndex;
		};
		struct {
			uint32	SubSegmentCode;
			uint16	PreviousSize;
			union {
				uint8	SegmentOffset;
				uint8	LFHFlags;
			};
			uint8	UnusedBytes;
		};
		uint64	CompactHeader;
	};
};

// TI = 0x1676
struct _HEAP_ENTRY {
	union {
		_HEAP_UNPACKED_ENTRY	UnpackedEntry;
		struct {
			void					*PreviousBlockPrivateData;
			union {
				struct {
					uint16					Size;
					uint8					Flags;
					uint8					SmallTagIndex;
				};
				struct {
					uint32					SubSegmentCode;
					uint16					PreviousSize;
					union {
						uint8					SegmentOffset;
						uint8					LFHFlags;
					};
					uint8					UnusedBytes;
				};
				uint64					CompactHeader;
			};
		};
		_HEAP_EXTENDED_ENTRY	ExtendedEntry;
		struct {
			void					*Reserved;
			union {
				struct {
					uint16					FunctionIndex;
					uint16					ContextValue;
				};
				uint32					InterceptorValue;
			};
			uint16					UnusedBytesLength;
			uint8					EntryOffset;
			uint8					ExtendedBlockSignature;
		};
		struct {
			void					*ReservedForAlignment;
			union {
				struct {
					uint32					Code1;
					union {
						struct {
							uint16					Code2;
							uint8					Code3;
							uint8					Code4;
						};
						uint32					Code234;
					};
				};
				uint64					AgregateCode;
			};
		};
	};
};

// TI = 0x164b - forward reference
struct _HEAP;

// TI = 0x1679
struct _HEAP_SEGMENT {
	_HEAP_ENTRY	Entry;
	uint32		SegmentSignature;
	uint32		SegmentFlags;
	_LIST_ENTRY	SegmentListEntry;
	_HEAP		*Heap;
	void		*BaseAddress;
	uint32		NumberOfPages;
	_HEAP_ENTRY	*FirstEntry;
	_HEAP_ENTRY	*LastValidEntry;
	uint32		NumberOfUnCommittedPages;
	uint32		NumberOfUnCommittedRanges;
	uint16		SegmentAllocatorBackTraceIndex;
	uint16		Reserved;
	_LIST_ENTRY	UCRSegmentList;
};

// TI = 0x165c
struct _HEAP {
	union {
		_HEAP_SEGMENT				Segment;
		struct {
			_HEAP_ENTRY					Entry;
			uint32						SegmentSignature;
			uint32						SegmentFlags;
			_LIST_ENTRY					SegmentListEntry;
			_HEAP						*Heap;
			void						*BaseAddress;
			uint32						NumberOfPages;
			_HEAP_ENTRY					*FirstEntry;
			_HEAP_ENTRY					*LastValidEntry;
			uint32						NumberOfUnCommittedPages;
			uint32						NumberOfUnCommittedRanges;
			uint16						SegmentAllocatorBackTraceIndex;
			uint16						Reserved;
			_LIST_ENTRY					UCRSegmentList;
		};
	};
	uint32						Flags;
	uint32						ForceFlags;
	uint32						CompatibilityFlags;
	uint32						EncodeFlagMask;
	_HEAP_ENTRY					Encoding;
	uint32						Interceptor;
	uint32						VirtualMemoryThreshold;
	uint32						Signature;
	uint64						SegmentReserve;
	uint64						SegmentCommit;
	uint64						DeCommitFreeBlockThreshold;
	uint64						DeCommitTotalFreeThreshold;
	uint64						TotalFreeSize;
	uint64						MaximumAllocationSize;
	uint16						ProcessHeapsListIndex;
	uint16						HeaderValidateLength;
	void						*HeaderValidateCopy;
	uint16						NextAvailableTagIndex;
	uint16						MaximumTagIndex;
	_HEAP_TAG_ENTRY				*TagEntries;
	_LIST_ENTRY					UCRList;
	uint64						AlignRound;
	uint64						AlignMask;
	_LIST_ENTRY					VirtualAllocdBlocks;
	_LIST_ENTRY					SegmentList;
	uint16						AllocatorBackTraceIndex;
	uint32						NonDedicatedListLength;
	void						*BlocksIndex;
	void						*UCRIndex;
	_HEAP_PSEUDO_TAG_ENTRY		*PseudoTagEntries;
	_LIST_ENTRY					FreeLists;
	_HEAP_LOCK					*LockVariable;
	int (*CommitRoutine)(void*, void **, uint64*);
	_RTL_RUN_ONCE				StackTraceInitVar;
	_RTL_HEAP_MEMORY_LIMIT_DATA	CommitLimitData;
	void						*FrontEndHeap;
	uint16						FrontHeapLockCount;
	uint8						FrontEndHeapType;
	uint8						RequestedFrontEndHeapType;
	wchar_t						*FrontEndHeapUsageData;
	uint16						FrontEndHeapMaximumIndex;
	volatile uint8				FrontEndHeapStatusBitmap[129];
	_HEAP_COUNTERS				Counters;
	_HEAP_TUNING_PARAMETERS		TuningParameters;
};

// TI = 0x1a93
struct _HEAP_ENTRY_EXTRA {
	union {
		struct {
			uint16	AllocatorBackTraceIndex;
			uint16	TagIndex;
			uint64	Settable;
		};
		struct {
			uint64	ZeroInit;
			uint64	ZeroInit1;
		};
	};
};

// TI = 0x1681
struct _HEAP_VIRTUAL_ALLOC_ENTRY {
	_LIST_ENTRY			Entry;
	_HEAP_ENTRY_EXTRA	ExtraStuff;
	uint64				CommitSize;
	uint64				ReserveSize;
	_HEAP_ENTRY			BusyBlock;
};

// TI = 0x1694
struct _HEAP_FREE_ENTRY {
	union {
		_HEAP_ENTRY				HeapEntry;
		_HEAP_UNPACKED_ENTRY	UnpackedEntry;
		struct {
			void					*PreviousBlockPrivateData;
			union {
				struct {
					uint16					Size;
					uint8					Flags;
					uint8					SmallTagIndex;
				};
				struct {
					uint32					SubSegmentCode;
					uint16					PreviousSize;
					union {
						uint8					SegmentOffset;
						uint8					LFHFlags;
					};
					uint8					UnusedBytes;
				};
				uint64					CompactHeader;
			};
		};
		_HEAP_EXTENDED_ENTRY	ExtendedEntry;
		struct {
			void					*Reserved;
			union {
				struct {
					uint16					FunctionIndex;
					uint16					ContextValue;
				};
				uint32					InterceptorValue;
			};
			uint16					UnusedBytesLength;
			uint8					EntryOffset;
			uint8					ExtendedBlockSignature;
		};
		struct {
			void					*ReservedForAlignment;
			union {
				struct {
					uint32					Code1;
					union {
						struct {
							uint16					Code2;
							uint8					Code3;
							uint8					Code4;
						};
						uint32					Code234;
					};
				};
				uint64					AgregateCode;
			};
		};
	};
	_LIST_ENTRY				FreeList;
};

// TI = 0x16cc
enum _LDR_DLL_LOAD_REASON {
	LoadReasonStaticDependency				= 0,
	LoadReasonStaticForwarderDependency		= 1,
	LoadReasonDynamicForwarderDependency	= 2,
	LoadReasonDelayloadDependency			= 3,
	LoadReasonDynamicLoad					= 4,
	LoadReasonAsImageLoad					= 5,
	LoadReasonAsDataLoad					= 6,
	LoadReasonEnclavePrimary				= 7,
	LoadReasonEnclaveDependency				= 8,
	LoadReasonUnknown						= -1,
};

// TI = 0x16c9
struct _LDRP_LOAD_CONTEXT {};

// TI = 0x1931
enum _LDR_DDAG_STATE {
	LdrModulesMerged					= -5,
	LdrModulesInitError					= -4,
	LdrModulesSnapError					= -3,
	LdrModulesUnloaded					= -2,
	LdrModulesUnloading					= -1,
	LdrModulesPlaceHolder				= 0,
	LdrModulesMapping					= 1,
	LdrModulesMapped					= 2,
	LdrModulesWaitingForDependencies	= 3,
	LdrModulesSnapping					= 4,
	LdrModulesSnapped					= 5,
	LdrModulesCondensed					= 6,
	LdrModulesReadyToInit				= 7,
	LdrModulesInitializing				= 8,
	LdrModulesReadyToRun				= 9,
};

// TI = 0x1b74
struct _LDRP_CSLIST {
	_SINGLE_LIST_ENTRY	*Tail;
};

// TI = 0x192d - forward reference
struct _LDR_SERVICE_TAG_RECORD;

// TI = 0x1c43
struct _LDR_SERVICE_TAG_RECORD {
	_LDR_SERVICE_TAG_RECORD	*Next;
	uint32					ServiceTag;
};

// TI = 0x1933
struct _LDR_DDAG_NODE {
	_LIST_ENTRY				Modules;
	_LDR_SERVICE_TAG_RECORD	*ServiceTagList;
	uint32					LoadCount;
	uint32					LoadWhileUnloadingCount;
	uint32					LowestLink;
	_LDRP_CSLIST			Dependencies;
	_LDRP_CSLIST			IncomingDependencies;
	_LDR_DDAG_STATE			State;
	_SINGLE_LIST_ENTRY		CondenseLink;
	uint32					PreorderNumber;
};

// TI = 0x16ce
struct _LDR_DATA_TABLE_ENTRY {
	_LIST_ENTRY					InLoadOrderLinks;
	_LIST_ENTRY					InMemoryOrderLinks;
	_LIST_ENTRY					InInitializationOrderLinks;
	void						*DllBase;
	void						*EntryPoint;
	uint32						SizeOfImage;
	_UNICODE_STRING				FullDllName;
	_UNICODE_STRING				BaseDllName;
	union {
		uint8						FlagGroup[4];
		uint32						Flags;
		struct {
			uint32						PackagedBinary:1;
			uint32						MarkedForRemoval:1;
			uint32						ImageDll:1;
			uint32						LoadNotificationsSent:1;
			uint32						TelemetryEntryProcessed:1;
			uint32						ProcessStaticImport:1;
			uint32						InLegacyLists:1;
			uint32						InIndexes:1;
			uint32						ShimDll:1;
			uint32						InExceptionTable:1;
			uint32						ReservedFlags1:2;
			uint32						LoadInProgress:1;
			uint32						LoadConfigProcessed:1;
			uint32						EntryProcessed:1;
			uint32						ProtectDelayLoad:1;
			uint32						ReservedFlags3:2;
			uint32						DontCallForThreads:1;
			uint32						ProcessAttachCalled:1;
			uint32						ProcessAttachFailed:1;
			uint32						CorDeferredValidate:1;
			uint32						CorImage:1;
			uint32						DontRelocate:1;
			uint32						CorILOnly:1;
			uint32						ChpeImage:1;
			uint32						ReservedFlags5:2;
			uint32						Redirected:1;
			uint32						ReservedFlags6:2;
			uint32						CompatDatabaseProcessed:1;
		};
	};
	uint16						ObsoleteLoadCount;
	uint16						TlsIndex;
	_LIST_ENTRY					HashLinks;
	uint32						TimeDateStamp;
	struct _ACTIVATION_CONTEXT	*EntryPointActivationContext;
	void						*Lock;
	_LDR_DDAG_NODE				*DdagNode;
	_LIST_ENTRY					NodeModuleLink;
	struct _LDRP_LOAD_CONTEXT	*LoadContext;
	void						*ParentDllBase;
	void						*SwitchBackContext;
	_RTL_BALANCED_NODE			BaseAddressIndexNode;
	_RTL_BALANCED_NODE			MappingInfoIndexNode;
	uint64						OriginalBase;
	_LARGE_INTEGER				LoadTime;
	uint32						BaseNameHashValue;
	_LDR_DLL_LOAD_REASON		LoadReason;
	uint32						ImplicitPathOptions;
	uint32						ReferenceCount;
	uint32						DependentLoadFlags;
	uint8						SigningLevel;
};

// TI = 0x1991
struct _INTERLOCK_SEQ {
	union {
		struct {
			uint16	Depth;
			union {
				struct {
					uint16	Hint:15;
					uint16	Lock:1;
				};
				uint16	Hint16;
			};
		};
		int		Exchg;
	};
};

// TI = 0x1937
struct _RTL_BITMAP_EX {
	uint64	SizeOfBitMap;
	uint64	*Buffer;
};

// TI = 0x1bac
struct _HEAP_USERDATA_OFFSETS {
	union {
		struct {
			uint16	FirstAllocationOffset;
			uint16	BlockStride;
		};
		uint32	StrideAndOffset;
	};
};

// TI = 0x16e6 - forward reference
struct _HEAP_USERDATA_HEADER;

// TI = 0x1ba0
union _HEAP_BUCKET_COUNTERS {
	struct {
		uint32	TotalBlocks;
		uint32	SubSegmentCounts;
	};
	int64	Aggregate64;
};

// TI = 0x16e4 - forward reference
struct _HEAP_LOCAL_SEGMENT_INFO;

// TI = 0x16eb
struct _HEAP_SUBSEGMENT {
	_HEAP_LOCAL_SEGMENT_INFO	*LocalInfo;
	_HEAP_USERDATA_HEADER		*UserBlocks;
	_SLIST_HEADER				DelayFreeList;
	volatile _INTERLOCK_SEQ		AggregateExchg;
	union {
		struct {
			volatile uint16				BlockSize;
			uint16						Flags;
			uint16						BlockCount;
			uint8						SizeIndex;
			uint8						AffinityIndex;
		};
		uint32						Alignment[2];
	};
	volatile uint32				Lock;
	_SINGLE_LIST_ENTRY			SFreeListEntry;
};

// TI = 0x1885 - forward reference
struct _LFH_HEAP;

// TI = 0x1bb8
struct _LFH_BLOCK_ZONE {
	_LIST_ENTRY		ListEntry;
	volatile int	NextIndex;
};

// TI = 0x1888
struct _HEAP_LOCAL_DATA {
	_SLIST_HEADER	DeletedSubSegments;
	_LFH_BLOCK_ZONE	volatile *CrtZone;
	_LFH_HEAP		*LowFragHeap;
	uint32			Sequence;
	uint32			DeleteRateThreshold;
};

// TI = 0x1898
struct _HEAP_BUCKET {
	uint16			BlockUnits;
	uint8			SizeIndex;
	union {
		struct {
			uint8			UseAffinity:1;
			uint8			DebugFlags:2;
		};
		volatile uint8	Flags;
	};
};

// TI = 0x19e5
struct _HEAP_LFH_MEM_POLICIES {
	union {
		struct {
			uint32	DisableAffinity:1;
			uint32	SlowSubsegmentGrowth:1;
			uint32	Spare:30;
		};
		uint32	AllPolicies;
	};
};

// TI = 0x1b4e
struct _USER_MEMORY_CACHE_ENTRY {
	_SLIST_HEADER	UserBlocks;
	volatile uint32	AvailableBlocks;
	volatile uint32	MinimumDepth;
	volatile uint32	CacheShiftThreshold;
	volatile uint16	Allocations;
	volatile uint16	Frees;
	volatile uint16	CacheHits;
};

// TI = 0x1902
union _HEAP_BUCKET_RUN_INFO {
	struct {
		uint32	Bucket;
		uint32	RunLength;
	};
	int64	Aggregate64;
};

// TI = 0x1a8e
struct _RTL_SRWLOCK {
	union {
		struct {
			uint64	Locked:1;
			uint64	Waiting:1;
			uint64	Waking:1;
			uint64	MultipleShared:1;
			uint64	Shared:60;
		};
		uint64	Value;
		void	*Ptr;
	};
};

// TI = 0x1894
struct _LFH_HEAP {
	_RTL_SRWLOCK					Lock;
	_LIST_ENTRY						SubSegmentZones;
	void							*Heap;
	void							*NextSegmentInfoArrayAddress;
	void							*FirstUncommittedAddress;
	void							*ReservedAddressLimit;
	uint32							SegmentCreate;
	uint32							SegmentDelete;
	volatile uint32					MinimumCacheDepth;
	volatile uint32					CacheShiftThreshold;
	volatile uint64					SizeInCache;
	volatile _HEAP_BUCKET_RUN_INFO	RunInfo;
	char							_pdb_padding0[8];
	_USER_MEMORY_CACHE_ENTRY		UserBlockCache[12];
	_HEAP_LFH_MEM_POLICIES			MemoryPolicies;
	_HEAP_BUCKET					Buckets[129];
	_HEAP_LOCAL_SEGMENT_INFO		*SegmentInfoArrays[129];
	_HEAP_LOCAL_SEGMENT_INFO		*AffinitizedInfoArrays[129];
	_SEGMENT_HEAP					*SegmentAllocator;
	_HEAP_LOCAL_DATA				LocalData[1];
};

// TI = 0x1881
struct _HEAP_LOCAL_SEGMENT_INFO {
	_HEAP_LOCAL_DATA				*LocalData;
	_HEAP_SUBSEGMENT				volatile *ActiveSubsegment;
	_HEAP_SUBSEGMENT				volatile *CachedItems[16];
	_SLIST_HEADER					SListHeader;
	volatile _HEAP_BUCKET_COUNTERS	Counters;
	uint32							LastOpSequence;
	uint16							BucketIndex;
	uint16							LastUsed;
	uint16							NoThrashCount;
};

// TI = 0x16ef
struct _HEAP_USERDATA_HEADER {
	union {
		_SINGLE_LIST_ENTRY		SFreeListEntry;
		_HEAP_SUBSEGMENT		*SubSegment;
	};
	void					*Reserved;
	union {
		uint32					SizeIndexAndPadding;
		struct {
			uint8					SizeIndex;
			uint8					GuardPagePresent;
			uint16					PaddingBytes;
		};
	};
	uint32					Signature;
	_HEAP_USERDATA_OFFSETS	EncodedOffsets;
	_RTL_BITMAP_EX			BusyBitmap;
	uint64					BitmapData[1];
};

// TI = 0x1708
struct _RTLP_HP_PADDING_HEADER {
	uint64	PaddingSize;
	uint64	Spare;
};

// TI = 0x1743
struct _RTL_HASH_TABLE {
	uint32				EntryCount;
	uint32				MaskBitCount:5;
	uint32				BucketCount:27;
	_SINGLE_LIST_ENTRY	*Buckets;
};

// TI = 0x174b
struct _RTL_HASH_ENTRY {
	_SINGLE_LIST_ENTRY	BucketLink;
	uint64				Key;
};

// TI = 0x1774
struct _RTL_HASH_TABLE_ITERATOR {
	_RTL_HASH_TABLE		*Hash;
	_RTL_HASH_ENTRY		*HashEntry;
	_SINGLE_LIST_ENTRY	*Bucket;
};

// TI = 0x1788
struct _RTL_CHASH_ENTRY {
	uint64	Key;
};

// TI = 0x1785
struct _RTL_CHASH_TABLE {
	_RTL_CHASH_ENTRY	*Table;
	uint32				EntrySizeShift;
	uint32				EntryMax;
	uint32				EntryCount;
};

// TI = 0x1799
struct _RTL_STACKDB_CONTEXT {
	_RTL_HASH_TABLE	StackSegmentTable;
	_RTL_HASH_TABLE	StackEntryTable;
	_RTL_SRWLOCK	StackEntryTableLock;
	_RTL_SRWLOCK	SegmentTableLock;
	void *(*Allocate)(uint64, void*);
	void (*Free)(void*, void*);
	void			*AllocatorContext;
};

// TI = 0x17b8
union _HEAP_LFH_ONDEMAND_POINTER {
	struct {
		uint16	Invalid:1;
		uint16	AllocationInProgress:1;
		uint16	Spare0:14;
		uint16	UsageData;
	};
	void	*AllBits;
};

// TI = 0x17bc
struct _HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS {
	union {
		struct {
			uint16	BlockSize;
			uint16	FirstBlockOffset;
		};
		uint32	EncodedData;
	};
};

// TI = 0x1b68
union _HEAP_LFH_SUBSEGMENT_DELAY_FREE {
	struct {
		uint64	DelayFree:1;
		uint64	Count:63;
	};
	void	*AllBits;
};

// TI = 0x17c1
struct _HEAP_LFH_SUBSEGMENT {
	_LIST_ENTRY								ListEntry;
	union {
		_HEAP_LFH_SUBSEGMENT_OWNER				*Owner;
		_HEAP_LFH_SUBSEGMENT_DELAY_FREE			DelayFree;
	};
	uint64									CommitLock;
	union {
		struct {
			uint16									FreeCount;
			uint16									BlockCount;
		};
		volatile int16							InterlockedShort;
		volatile int							InterlockedLong;
	};
	uint16									FreeHint;
	uint8									Location;
	uint8									WitheldBlockCount;
	_HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS	BlockOffsets;
	uint8									CommitUnitShift;
	uint8									CommitUnitCount;
	uint16									CommitStateOffset;
	uint64									BlockBitmap[1];
};

// TI = 0x17d0
struct _HEAP_LFH_UNUSED_BYTES_INFO {
	union {
		struct {
			uint16	UnusedBytes:14;
			uint16	ExtraPresent:1;
			uint16	OneByteUnused:1;
		};
		uint8	Bytes[2];
	};
};

// TI = 0x17d2
enum _HEAP_LFH_LOCKMODE {
	HeapLockNotHeld		= 0,
	HeapLockShared		= 1,
	HeapLockExclusive	= 2,
};

// TI = 0x17f8
struct _RTLP_HP_QUEUE_LOCK_HANDLE {
	uint64	Reserved1;
	uint64	LockPtr;
	uint64	HandleData;
};

// TI = 0x180d
union _HEAP_VS_CHUNK_HEADER_SIZE {
	struct {
		uint32	MemoryCost:16;
		uint32	UnsafeSize:16;
		uint32	UnsafePrevSize:16;
		uint32	Allocated:8;
	};
	uint16	KeyUShort;
	uint32	KeyULong;
	uint64	HeaderBits;
};

// TI = 0x1808
struct _HEAP_VS_CHUNK_HEADER {
	_HEAP_VS_CHUNK_HEADER_SIZE	Sizes;
	union {
		struct {
			uint32						EncodedSegmentPageOffset:8;
			uint32						UnusedBytes:1;
			uint32						SkipDuringWalk:1;
			uint32						Spare:22;
		};
		uint32						AllocatedChunkBits;
	};
};

// TI = 0x1819
struct _HEAP_VS_CHUNK_FREE_HEADER {
	union {
		_HEAP_VS_CHUNK_HEADER	Header;
		struct {
			uint64					OverlapsHeader;
			_RTL_BALANCED_NODE		Node;
		};
	};
};

// TI = 0x1822
struct _HEAP_VS_SUBSEGMENT {
	_LIST_ENTRY	ListEntry;
	uint64		CommitBitmap;
	uint64		CommitLock;
	uint16		Size;
	uint16		Signature:15;
	uint16		FullCommit:1;
};

// TI = 0x1831
struct _HEAP_VS_UNUSED_BYTES_INFO {
	union {
		struct {
			uint16	UnusedBytes:13;
			uint16	LfhSubsegment:1;
			uint16	ExtraPresent:1;
			uint16	OneByteUnused:1;
		};
		uint8	Bytes[2];
	};
};

// TI = 0x185a
struct _HEAP_DESCRIPTOR_KEY {
	union {
		uint32	Key;
		struct {
			uint32	EncodedCommittedPageCount:16;
			uint32	LargePageCost:8;
			uint32	UnitCount:8;
		};
	};
};

// TI = 0x1837
struct _HEAP_PAGE_RANGE_DESCRIPTOR {
	union {
		_RTL_BALANCED_NODE		TreeNode;
		struct {
			uint32					TreeSignature;
			uint32					UnusedBytes;
			uint16					ExtraPresent:1;
			uint16					Spare0:15;
		};
	};
	volatile uint8			RangeFlags;
	uint8					CommittedPageCount;
	uint16					Spare;
	union {
		_HEAP_DESCRIPTOR_KEY	Key;
		struct {
			uint8					Align[3];
			union {
				uint8					UnitOffset;
				uint8					UnitSize;
			};
		};
	};
};

// TI = 0x1839
enum _HEAP_SEG_RANGE_TYPE {
	HeapSegRangeUser		= 0,
	HeapSegRangeInternal	= 1,
	HeapSegRangeLFH			= 2,
	HeapSegRangeVS			= 3,
	HeapSegRangeTypeMax		= 3,
};

// TI = 0x1a23
union _HEAP_SEGMENT_MGR_COMMIT_STATE {
	struct {
		uint16			CommittedPageCount:11;
		uint16			Spare:3;
		uint16			LargePageOperationInProgress:1;
		uint16			LargePageCommit:1;
	};
	volatile uint16	EntireUShortV;
	uint16			EntireUShort;
};

// TI = 0x1840
union _HEAP_PAGE_SEGMENT {
	struct {
		_LIST_ENTRY						ListEntry;
		uint64							Signature;
		_HEAP_SEGMENT_MGR_COMMIT_STATE	*SegmentCommitState;
		uint8							UnusedWatermark;
	};
	_HEAP_PAGE_RANGE_DESCRIPTOR		DescArray[256];
};

// TI = 0x185c
enum _RTLP_HP_ALLOCATOR {
	RtlpHpSegmentSm		= 0,
	RtlpHpSegmentLg		= 1,
	RtlpHpSegmentTypes	= 2,
	RtlpHpHugeAllocator	= 2,
	RtlpHpAllocatorMax	= 3,
};

// TI = 0x18a0
struct _HEAP_LARGE_ALLOC_DATA {
	_RTL_BALANCED_NODE	TreeNode;
	union {
		uint64				VirtualAddress;
		struct {
			uint64				UnusedBytes:16;
			uint64				ExtraPresent:1;
			uint64				GuardPageCount:1;
			uint64				GuardPageAlignment:6;
			uint64				Spare:4;
			uint64				AllocatedPages:52;
		};
	};
};

// TI = 0x1ada
struct _STRING32 {
	uint16	Length;
	uint16	MaximumLength;
	uint32	Buffer;
};

// TI = 0x197a
struct _GDI_TEB_BATCH32 {
	uint32	Offset:31;
	uint32	HasRenderingCommand:1;
	uint32	HDC;
	uint32	Buffer[310];
};

// TI = 0x18d5
struct _ACTIVATION_CONTEXT_STACK32 {
	uint32			ActiveFrame;
	LIST_ENTRY32	FrameListCache;
	uint32			Flags;
	uint32			NextCookieSequenceNumber;
	uint32			StackId;
};

// TI = 0x1978
struct _CLIENT_ID32 {
	uint32	UniqueProcess;
	uint32	UniqueThread;
};

// TI = 0x1b5e
struct _NT_TIB32 {
	uint32	ExceptionList;
	uint32	StackBase;
	uint32	StackLimit;
	uint32	SubSystemTib;
	union {
		uint32	FiberData;
		uint32	Version;
	};
	uint32	ArbitraryUserPointer;
	uint32	Self;
};

// TI = 0x18b1
struct _TEB32 {
	_NT_TIB32					NtTib;
	uint32						EnvironmentPointer;
	_CLIENT_ID32				ClientId;
	uint32						ActiveRpcHandle;
	uint32						ThreadLocalStoragePointer;
	uint32						ProcessEnvironmentBlock;
	uint32						LastErrorValue;
	uint32						CountOfOwnedCriticalSections;
	uint32						CsrClientThread;
	uint32						Win32ThreadInfo;
	uint32						User32Reserved[26];
	uint32						UserReserved[5];
	uint32						WOW32Reserved;
	uint32						CurrentLocale;
	uint32						FpSoftwareStatusRegister;
	uint32						ReservedForDebuggerInstrumentation[16];
	uint32						SystemReserved1[26];
	char						PlaceholderCompatibilityMode;
	uint8						PlaceholderHydrationAlwaysExplicit;
	char						PlaceholderReserved[10];
	uint32						ProxiedProcessId;
	_ACTIVATION_CONTEXT_STACK32	_ActivationStack;
	uint8						WorkingOnBehalfTicket[8];
	int							ExceptionCode;
	uint32						ActivationContextStackPointer;
	uint32						InstrumentationCallbackSp;
	uint32						InstrumentationCallbackPreviousPc;
	uint32						InstrumentationCallbackPreviousSp;
	uint8						InstrumentationCallbackDisabled;
	uint8						SpareBytes[23];
	uint32						TxFsContext;
	_GDI_TEB_BATCH32			GdiTebBatch;
	_CLIENT_ID32				RealClientId;
	uint32						GdiCachedProcessHandle;
	uint32						GdiClientPID;
	uint32						GdiClientTID;
	uint32						GdiThreadLocalInfo;
	uint32						Win32ClientInfo[62];
	uint32						glDispatchTable[233];
	uint32						glReserved1[29];
	uint32						glReserved2;
	uint32						glSectionInfo;
	uint32						glSection;
	uint32						glTable;
	uint32						glCurrentRC;
	uint32						glContext;
	uint32						LastStatusValue;
	_STRING32					StaticUnicodeString;
	wchar_t						StaticUnicodeBuffer[522];
	uint32						DeallocationStack;
	uint32						TlsSlots[64];
	LIST_ENTRY32				TlsLinks;
	uint32						Vdm;
	uint32						ReservedForNtRpc;
	uint32						DbgSsReserved[2];
	uint32						HardErrorMode;
	uint32						Instrumentation[9];
	_GUID						ActivityId;
	uint32						SubProcessTag;
	uint32						PerflibData;
	uint32						EtwTraceData;
	uint32						WinSockData;
	uint32						GdiBatchCount;
	union {
		_PROCESSOR_NUMBER			CurrentIdealProcessor;
		uint32						IdealProcessorValue;
		struct {
			uint8						ReservedPad0;
			uint8						ReservedPad1;
			uint8						ReservedPad2;
			uint8						IdealProcessor;
		};
	};
	uint32						GuaranteedStackBytes;
	uint32						ReservedForPerf;
	uint32						ReservedForOle;
	uint32						WaitingOnLoaderLock;
	uint32						SavedPriorityState;
	uint32						ReservedForCodeCoverage;
	uint32						ThreadPoolData;
	uint32						TlsExpansionSlots;
	uint32						MuiGeneration;
	uint32						IsImpersonating;
	uint32						NlsCache;
	uint32						pShimData;
	uint32						HeapData;
	uint32						CurrentTransactionHandle;
	uint32						ActiveFrame;
	uint32						FlsData;
	uint32						PreferredLanguages;
	uint32						UserPrefLanguages;
	uint32						MergedPrefLanguages;
	uint32						MuiImpersonation;
	union {
		volatile uint16				CrossTebFlags;
		uint16						SpareCrossTebBits:16;
	};
	union {
		uint16						SameTebFlags;
		struct {
			uint16						SafeThunkCall:1;
			uint16						InDebugPrint:1;
			uint16						HasFiberData:1;
			uint16						SkipThreadAttach:1;
			uint16						WerInShipAssertCode:1;
			uint16						RanProcessInit:1;
			uint16						ClonedThread:1;
			uint16						SuppressDebugMsg:1;
			uint16						DisableUserStackWalk:1;
			uint16						RtlExceptionAttached:1;
			uint16						InitialThread:1;
			uint16						SessionAware:1;
			uint16						LoadOwner:1;
			uint16						LoaderWorker:1;
			uint16						SkipLoaderInit:1;
			uint16						SpareSameTebBits:1;
		};
	};
	uint32						TxnScopeEnterCallback;
	uint32						TxnScopeExitCallback;
	uint32						TxnScopeContext;
	uint32						LockCount;
	int							WowTebOffset;
	uint32						ResourceRetValue;
	uint32						ReservedForWdf;
	uint64						ReservedForCrt;
	_GUID						EffectiveContainerId;
};

// TI = 0x1af2
struct _STRING64 {
	uint16	Length;
	uint16	MaximumLength;
	uint64	Buffer;
};

// TI = 0x1942
struct _GDI_TEB_BATCH64 {
	uint32	Offset:31;
	uint32	HasRenderingCommand:1;
	uint64	HDC;
	uint32	Buffer[310];
};

// TI = 0x1b29
struct _ACTIVATION_CONTEXT_STACK64 {
	uint64			ActiveFrame;
	LIST_ENTRY64	FrameListCache;
	uint32			Flags;
	uint32			NextCookieSequenceNumber;
	uint32			StackId;
};

// TI = 0x191c
struct _CLIENT_ID64 {
	uint64	UniqueProcess;
	uint64	UniqueThread;
};

// TI = 0x19ec
struct _NT_TIB64 {
	uint64	ExceptionList;
	uint64	StackBase;
	uint64	StackLimit;
	uint64	SubSystemTib;
	union {
		uint64	FiberData;
		uint32	Version;
	};
	uint64	ArbitraryUserPointer;
	uint64	Self;
};

// TI = 0x18bf
struct _TEB64 {
	_NT_TIB64					NtTib;
	uint64						EnvironmentPointer;
	_CLIENT_ID64				ClientId;
	uint64						ActiveRpcHandle;
	uint64						ThreadLocalStoragePointer;
	uint64						ProcessEnvironmentBlock;
	uint32						LastErrorValue;
	uint32						CountOfOwnedCriticalSections;
	uint64						CsrClientThread;
	uint64						Win32ThreadInfo;
	uint32						User32Reserved[26];
	uint32						UserReserved[5];
	uint64						WOW32Reserved;
	uint32						CurrentLocale;
	uint32						FpSoftwareStatusRegister;
	uint64						ReservedForDebuggerInstrumentation[16];
	uint64						SystemReserved1[30];
	char						PlaceholderCompatibilityMode;
	uint8						PlaceholderHydrationAlwaysExplicit;
	char						PlaceholderReserved[10];
	uint32						ProxiedProcessId;
	_ACTIVATION_CONTEXT_STACK64	_ActivationStack;
	uint8						WorkingOnBehalfTicket[8];
	int							ExceptionCode;
	uint8						Padding0[4];
	uint64						ActivationContextStackPointer;
	uint64						InstrumentationCallbackSp;
	uint64						InstrumentationCallbackPreviousPc;
	uint64						InstrumentationCallbackPreviousSp;
	uint32						TxFsContext;
	uint8						InstrumentationCallbackDisabled;
	uint8						UnalignedLoadStoreExceptions;
	uint8						Padding1[2];
	_GDI_TEB_BATCH64			GdiTebBatch;
	_CLIENT_ID64				RealClientId;
	uint64						GdiCachedProcessHandle;
	uint32						GdiClientPID;
	uint32						GdiClientTID;
	uint64						GdiThreadLocalInfo;
	uint64						Win32ClientInfo[62];
	uint64						glDispatchTable[233];
	uint64						glReserved1[29];
	uint64						glReserved2;
	uint64						glSectionInfo;
	uint64						glSection;
	uint64						glTable;
	uint64						glCurrentRC;
	uint64						glContext;
	uint32						LastStatusValue;
	uint8						Padding2[4];
	_STRING64					StaticUnicodeString;
	wchar_t						StaticUnicodeBuffer[522];
	uint8						Padding3[6];
	uint64						DeallocationStack;
	uint64						TlsSlots[64];
	LIST_ENTRY64				TlsLinks;
	uint64						Vdm;
	uint64						ReservedForNtRpc;
	uint64						DbgSsReserved[2];
	uint32						HardErrorMode;
	uint8						Padding4[4];
	uint64						Instrumentation[11];
	_GUID						ActivityId;
	uint64						SubProcessTag;
	uint64						PerflibData;
	uint64						EtwTraceData;
	uint64						WinSockData;
	uint32						GdiBatchCount;
	union {
		_PROCESSOR_NUMBER			CurrentIdealProcessor;
		uint32						IdealProcessorValue;
		struct {
			uint8						ReservedPad0;
			uint8						ReservedPad1;
			uint8						ReservedPad2;
			uint8						IdealProcessor;
		};
	};
	uint32						GuaranteedStackBytes;
	uint8						Padding5[4];
	uint64						ReservedForPerf;
	uint64						ReservedForOle;
	uint32						WaitingOnLoaderLock;
	uint8						Padding6[4];
	uint64						SavedPriorityState;
	uint64						ReservedForCodeCoverage;
	uint64						ThreadPoolData;
	uint64						TlsExpansionSlots;
	uint64						DeallocationBStore;
	uint64						BStoreLimit;
	uint32						MuiGeneration;
	uint32						IsImpersonating;
	uint64						NlsCache;
	uint64						pShimData;
	uint32						HeapData;
	uint8						Padding7[4];
	uint64						CurrentTransactionHandle;
	uint64						ActiveFrame;
	uint64						FlsData;
	uint64						PreferredLanguages;
	uint64						UserPrefLanguages;
	uint64						MergedPrefLanguages;
	uint32						MuiImpersonation;
	union {
		volatile uint16				CrossTebFlags;
		uint16						SpareCrossTebBits:16;
	};
	union {
		uint16						SameTebFlags;
		struct {
			uint16						SafeThunkCall:1;
			uint16						InDebugPrint:1;
			uint16						HasFiberData:1;
			uint16						SkipThreadAttach:1;
			uint16						WerInShipAssertCode:1;
			uint16						RanProcessInit:1;
			uint16						ClonedThread:1;
			uint16						SuppressDebugMsg:1;
			uint16						DisableUserStackWalk:1;
			uint16						RtlExceptionAttached:1;
			uint16						InitialThread:1;
			uint16						SessionAware:1;
			uint16						LoadOwner:1;
			uint16						LoaderWorker:1;
			uint16						SkipLoaderInit:1;
			uint16						SpareSameTebBits:1;
		};
	};
	uint64						TxnScopeEnterCallback;
	uint64						TxnScopeExitCallback;
	uint64						TxnScopeContext;
	uint32						LockCount;
	int							WowTebOffset;
	uint64						ResourceRetValue;
	uint64						ReservedForWdf;
	uint64						ReservedForCrt;
	_GUID						EffectiveContainerId;
};

// TI = 0x18c9
enum _IO_RATE_CONTROL_TYPE {
	IoRateControlTypeCapMin							= 0,
	IoRateControlTypeIopsCap						= 0,
	IoRateControlTypeBandwidthCap					= 1,
	IoRateControlTypeTimePercentCap					= 2,
	IoRateControlTypeCapMax							= 2,
	IoRateControlTypeReservationMin					= 3,
	IoRateControlTypeIopsReservation				= 3,
	IoRateControlTypeBandwidthReservation			= 4,
	IoRateControlTypeTimePercentReservation			= 5,
	IoRateControlTypeReservationMax					= 5,
	IoRateControlTypeCriticalReservationMin			= 6,
	IoRateControlTypeIopsCriticalReservation		= 6,
	IoRateControlTypeBandwidthCriticalReservation	= 7,
	IoRateControlTypeTimePercentCriticalReservation	= 8,
	IoRateControlTypeCriticalReservationMax			= 8,
	IoRateControlTypeSoftCapMin						= 9,
	IoRateControlTypeIopsSoftCap					= 9,
	IoRateControlTypeBandwidthSoftCap				= 10,
	IoRateControlTypeTimePercentSoftCap				= 11,
	IoRateControlTypeSoftCapMax						= 11,
	IoRateControlTypeLimitExcessNotifyMin			= 12,
	IoRateControlTypeIopsLimitExcessNotify			= 12,
	IoRateControlTypeBandwidthLimitExcessNotify		= 13,
	IoRateControlTypeTimePercentLimitExcessNotify	= 14,
	IoRateControlTypeLimitExcessNotifyMax			= 14,
	IoRateControlTypeMax							= 15,
};

// TI = 0x18cb
enum _KINTERRUPT_POLARITY {
	InterruptPolarityUnknown		= 0,
	InterruptActiveHigh				= 1,
	InterruptRisingEdge				= 1,
	InterruptActiveLow				= 2,
	InterruptFallingEdge			= 2,
	InterruptActiveBoth				= 3,
	InterruptActiveBothTriggerLow	= 3,
	InterruptActiveBothTriggerHigh	= 4,
};

// TI = 0x18cd
enum _JOBOBJECTINFOCLASS {
	JobObjectBasicAccountingInformation			= 1,
	JobObjectBasicLimitInformation				= 2,
	JobObjectBasicProcessIdList					= 3,
	JobObjectBasicUIRestrictions				= 4,
	JobObjectSecurityLimitInformation			= 5,
	JobObjectEndOfJobTimeInformation			= 6,
	JobObjectAssociateCompletionPortInformation	= 7,
	JobObjectBasicAndIoAccountingInformation	= 8,
	JobObjectExtendedLimitInformation			= 9,
	JobObjectJobSetInformation					= 10,
	JobObjectGroupInformation					= 11,
	JobObjectNotificationLimitInformation		= 12,
	JobObjectLimitViolationInformation			= 13,
	JobObjectGroupInformationEx					= 14,
	JobObjectCpuRateControlInformation			= 15,
	JobObjectCompletionFilter					= 16,
	JobObjectCompletionCounter					= 17,
	JobObjectFreezeInformation					= 18,
	JobObjectExtendedAccountingInformation		= 19,
	JobObjectWakeInformation					= 20,
	JobObjectBackgroundInformation				= 21,
	JobObjectSchedulingRankBiasInformation		= 22,
	JobObjectTimerVirtualizationInformation		= 23,
	JobObjectCycleTimeNotification				= 24,
	JobObjectClearEvent							= 25,
	JobObjectInterferenceInformation			= 26,
	JobObjectClearPeakJobMemoryUsed				= 27,
	JobObjectMemoryUsageInformation				= 28,
	JobObjectSharedCommit						= 29,
	JobObjectContainerId						= 30,
	JobObjectIoRateControlInformation			= 31,
	JobObjectSiloRootDirectory					= 37,
	JobObjectServerSiloBasicInformation			= 38,
	JobObjectServerSiloUserSharedData			= 39,
	JobObjectServerSiloInitialize				= 40,
	JobObjectServerSiloRunningState				= 41,
	JobObjectIoAttribution						= 42,
	JobObjectMemoryPartitionInformation			= 43,
	JobObjectContainerTelemetryId				= 44,
	JobObjectSiloSystemRoot						= 45,
	JobObjectEnergyTrackingState				= 46,
	JobObjectThreadImpersonationInformation		= 47,
	JobObjectReserved1Information				= 18,
	JobObjectReserved2Information				= 19,
	JobObjectReserved3Information				= 20,
	JobObjectReserved4Information				= 21,
	JobObjectReserved5Information				= 22,
	JobObjectReserved6Information				= 23,
	JobObjectReserved7Information				= 24,
	JobObjectReserved8Information				= 25,
	JobObjectReserved9Information				= 26,
	JobObjectReserved10Information				= 27,
	JobObjectReserved11Information				= 28,
	JobObjectReserved12Information				= 29,
	JobObjectReserved13Information				= 30,
	JobObjectReserved14Information				= 31,
	JobObjectNetRateControlInformation			= 32,
	JobObjectNotificationLimitInformation2		= 33,
	JobObjectLimitViolationInformation2			= 34,
	JobObjectCreateSilo							= 35,
	JobObjectSiloBasicInformation				= 36,
	JobObjectReserved15Information				= 37,
	JobObjectReserved16Information				= 38,
	JobObjectReserved17Information				= 39,
	JobObjectReserved18Information				= 40,
	JobObjectReserved19Information				= 41,
	JobObjectReserved20Information				= 42,
	JobObjectReserved21Information				= 43,
	JobObjectReserved22Information				= 44,
	JobObjectReserved23Information				= 45,
	JobObjectReserved24Information				= 46,
	JobObjectReserved25Information				= 47,
	MaxJobObjectInfoClass						= 48,
};

// TI = 0x18d7
enum _PROCESS_SECTION_TYPE {
	ProcessSectionData				= 0,
	ProcessSectionImage				= 1,
	ProcessSectionImageNx			= 2,
	ProcessSectionPagefileBacked	= 3,
	ProcessSectionMax				= 4,
};

// TI = 0x18f0
enum _RTLP_HP_MEMORY_TYPE {
	HeapMemoryPaged		= 0,
	HeapMemoryNonPaged	= 1,
	HeapMemoryLargePage	= 2,
	HeapMemoryHugePage	= 3,
	HeapMemoryTypeMax	= 4,
};

// TI = 0x18fe
enum _KWAIT_BLOCK_STATE {
	WaitBlockBypassStart			= 0,
	WaitBlockBypassComplete			= 1,
	WaitBlockSuspendBypassStart		= 2,
	WaitBlockSuspendBypassComplete	= 3,
	WaitBlockActive					= 4,
	WaitBlockInactive				= 5,
	WaitBlockSuspended				= 6,
	WaitBlockAllStates				= 7,
};

// TI = 0x1906
enum _KHETERO_CPU_POLICY {
	KHeteroCpuPolicyAll			= 0,
	KHeteroCpuPolicyLarge		= 1,
	KHeteroCpuPolicyLargeOrIdle	= 2,
	KHeteroCpuPolicySmall		= 3,
	KHeteroCpuPolicySmallOrIdle	= 4,
	KHeteroCpuPolicyDynamic		= 5,
	KHeteroCpuPolicyStaticMax	= 5,
	KHeteroCpuPolicyBiasedSmall	= 6,
	KHeteroCpuPolicyBiasedLarge	= 7,
	KHeteroCpuPolicyDefault		= 8,
	KHeteroCpuPolicyMax			= 9,
};

// TI = 0x1923 - forward reference
struct _RTL_TRACE_BLOCK;

// TI = 0x1a37
struct _RTL_TRACE_BLOCK {
	uint32				Magic;
	uint32				Count;
	uint32				Size;
	uint64				UserCount;
	uint64				UserSize;
	void				*UserContext;
	_RTL_TRACE_BLOCK	*Next;
	void				**Trace;
};

// TI = 0x1921 - forward reference
struct _RTL_TRACE_SEGMENT;

// TI = 0x192a
struct _RTL_TRACE_DATABASE {
	uint32					Magic;
	uint32					Flags;
	uint32					Tag;
	_RTL_TRACE_SEGMENT		*SegmentList;
	uint64					MaximumSize;
	uint64					CurrentSize;
	void					*Owner;
	_RTL_CRITICAL_SECTION	Lock;
	uint32					NoOfBuckets;
	_RTL_TRACE_BLOCK		**Buckets;
	uint32 (*HashFunction)(uint32, void **);
	uint64					NoOfTraces;
	uint64					NoOfHits;
	uint32					HashCounter[16];
};

// TI = 0x1ac0
struct _RTL_TRACE_SEGMENT {
	uint32				Magic;
	_RTL_TRACE_DATABASE	*Database;
	_RTL_TRACE_SEGMENT	*NextSegment;
	uint64				TotalSize;
	char				*SegmentStart;
	char				*SegmentEnd;
	char				*SegmentFree;
};

// TI = 0x192c
enum JOB_OBJECT_IO_RATE_CONTROL_FLAGS {
	JOB_OBJECT_IO_RATE_CONTROL_ENABLE							= 1,
	JOB_OBJECT_IO_RATE_CONTROL_STANDALONE_VOLUME				= 2,
	JOB_OBJECT_IO_RATE_CONTROL_FORCE_UNIT_ACCESS_ALL			= 4,
	JOB_OBJECT_IO_RATE_CONTROL_FORCE_UNIT_ACCESS_ON_SOFT_CAP	= 8,
	JOB_OBJECT_IO_RATE_CONTROL_VALID_FLAGS						= 15,
};

// TI = 0x1935
enum _KOBJECTS {
	EventNotificationObject		= 0,
	EventSynchronizationObject	= 1,
	MutantObject				= 2,
	ProcessObject				= 3,
	QueueObject					= 4,
	SemaphoreObject				= 5,
	ThreadObject				= 6,
	GateObject					= 7,
	TimerNotificationObject		= 8,
	TimerSynchronizationObject	= 9,
	Spare2Object				= 10,
	Spare3Object				= 11,
	Spare4Object				= 12,
	Spare5Object				= 13,
	Spare6Object				= 14,
	Spare7Object				= 15,
	Spare8Object				= 16,
	ProfileCallbackObject		= 17,
	ApcObject					= 18,
	DpcObject					= 19,
	DeviceQueueObject			= 20,
	PriQueueObject				= 21,
	InterruptObject				= 22,
	ProfileObject				= 23,
	Timer2NotificationObject	= 24,
	Timer2SynchronizationObject	= 25,
	ThreadedDpcObject			= 26,
	MaximumKernelObject			= 27,
};

// TI = 0x193f
enum _PS_STD_HANDLE_STATE {
	PsNeverDuplicate		= 0,
	PsRequestDuplicate		= 1,
	PsAlwaysDuplicate		= 2,
	PsMaxStdHandleStates	= 3,
};

// TI = 0x194b
enum _PS_WAKE_REASON {
	PsWakeReasonUser				= 0,
	PsWakeReasonExecutionRequired	= 1,
	PsWakeReasonKernel				= 2,
	PsWakeReasonInstrumentation		= 3,
	PsWakeReasonPreserveProcess		= 4,
	PsWakeReasonActivityReference	= 5,
	PsWakeReasonWorkOnBehalf		= 6,
	PsMaxWakeReasons				= 7,
};

// TI = 0x194d
enum _RTL_MEMORY_TYPE {
	MemoryTypePaged		= 0,
	MemoryTypeNonPaged	= 1,
	MemoryTypeLargePage	= 2,
	MemoryTypeHugePage	= 3,
	MemoryTypeMax		= 4,
};

// TI = 0x1955
enum _KHETERO_RUNNING_TYPE {
	KHeteroShortRunning		= 0,
	KHeteroLongRunning		= 1,
	KHeteroRunningTypeMax	= 2,
};

// TI = 0x19ce
struct _ACL {
	uint8	AclRevision;
	uint8	Sbz1;
	uint16	AclSize;
	uint16	AceCount;
	uint16	Sbz2;
};

// TI = 0x1964
struct _SECURITY_DESCRIPTOR {
	uint8	Revision;
	uint8	Sbz1;
	uint16	Control;
	void	*Owner;
	void	*Group;
	_ACL	*Sacl;
	_ACL	*Dacl;
};

// TI = 0x1966
enum _REG_NOTIFY_CLASS {
	RegNtDeleteKey						= 0,
	RegNtPreDeleteKey					= 0,
	RegNtSetValueKey					= 1,
	RegNtPreSetValueKey					= 1,
	RegNtDeleteValueKey					= 2,
	RegNtPreDeleteValueKey				= 2,
	RegNtSetInformationKey				= 3,
	RegNtPreSetInformationKey			= 3,
	RegNtRenameKey						= 4,
	RegNtPreRenameKey					= 4,
	RegNtEnumerateKey					= 5,
	RegNtPreEnumerateKey				= 5,
	RegNtEnumerateValueKey				= 6,
	RegNtPreEnumerateValueKey			= 6,
	RegNtQueryKey						= 7,
	RegNtPreQueryKey					= 7,
	RegNtQueryValueKey					= 8,
	RegNtPreQueryValueKey				= 8,
	RegNtQueryMultipleValueKey			= 9,
	RegNtPreQueryMultipleValueKey		= 9,
	RegNtPreCreateKey					= 10,
	RegNtPostCreateKey					= 11,
	RegNtPreOpenKey						= 12,
	RegNtPostOpenKey					= 13,
	RegNtKeyHandleClose					= 14,
	RegNtPreKeyHandleClose				= 14,
	RegNtPostDeleteKey					= 15,
	RegNtPostSetValueKey				= 16,
	RegNtPostDeleteValueKey				= 17,
	RegNtPostSetInformationKey			= 18,
	RegNtPostRenameKey					= 19,
	RegNtPostEnumerateKey				= 20,
	RegNtPostEnumerateValueKey			= 21,
	RegNtPostQueryKey					= 22,
	RegNtPostQueryValueKey				= 23,
	RegNtPostQueryMultipleValueKey		= 24,
	RegNtPostKeyHandleClose				= 25,
	RegNtPreCreateKeyEx					= 26,
	RegNtPostCreateKeyEx				= 27,
	RegNtPreOpenKeyEx					= 28,
	RegNtPostOpenKeyEx					= 29,
	RegNtPreFlushKey					= 30,
	RegNtPostFlushKey					= 31,
	RegNtPreLoadKey						= 32,
	RegNtPostLoadKey					= 33,
	RegNtPreUnLoadKey					= 34,
	RegNtPostUnLoadKey					= 35,
	RegNtPreQueryKeySecurity			= 36,
	RegNtPostQueryKeySecurity			= 37,
	RegNtPreSetKeySecurity				= 38,
	RegNtPostSetKeySecurity				= 39,
	RegNtCallbackObjectContextCleanup	= 40,
	RegNtPreRestoreKey					= 41,
	RegNtPostRestoreKey					= 42,
	RegNtPreSaveKey						= 43,
	RegNtPostSaveKey					= 44,
	RegNtPreReplaceKey					= 45,
	RegNtPostReplaceKey					= 46,
	RegNtPreQueryKeyName				= 47,
	RegNtPostQueryKeyName				= 48,
	MaxRegNtNotifyClass					= 49,
};

// TI = 0x1974
enum _KTHREAD_TAG {
	KThreadTagNone				= 0,
	KThreadTagMediaBuffering	= 1,
	KThreadTagMax				= 2,
};

// TI = 0x1a70
struct _RTL_STACK_DATABASE_LOCK {
	_RTL_SRWLOCK	Lock;
};

// TI = 0x19c5 - forward reference
struct _RTL_STD_LIST_ENTRY;

// TI = 0x1b57
struct _RTL_STD_LIST_ENTRY {
	_RTL_STD_LIST_ENTRY	*Next;
};

// TI = 0x1bd9
struct _RTL_STD_LIST_HEAD {
	_RTL_STD_LIST_ENTRY			*Next;
	_RTL_STACK_DATABASE_LOCK	Lock;
};

// TI = 0x19ca
struct _RTL_STACK_TRACE_ENTRY {
	_RTL_STD_LIST_ENTRY	HashChain;
	uint16				TraceCount:11;
	uint16				BlockDepth:5;
	uint16				IndexHigh;
	uint16				Index;
	uint16				Depth;
	union {
		void				*BackTrace[32];
		_SLIST_ENTRY		FreeChain;
	};
};

// TI = 0x1985
struct _STACK_TRACE_DATABASE {
	union {
		char						Reserved[104];
		_RTL_STACK_DATABASE_LOCK	Lock;
	};
	void						*Reserved2;
	uint64						PeakHashCollisionListLength;
	void						*LowerMemoryStart;
	uint8						PreCommitted;
	uint8						DumpInProgress;
	void						*CommitBase;
	void						*CurrentLowerCommitLimit;
	void						*CurrentUpperCommitLimit;
	char						*NextFreeLowerMemory;
	char						*NextFreeUpperMemory;
	uint32						NumberOfEntriesLookedUp;
	uint32						NumberOfEntriesAdded;
	_RTL_STACK_TRACE_ENTRY		**EntryIndexArray;
	uint32						NumberOfEntriesAllocated;
	uint32						NumberOfEntriesAvailable;
	uint32						NumberOfAllocationFailures;
	_SLIST_HEADER				FreeLists[32];
	uint32						NumberOfBuckets;
	_RTL_STD_LIST_HEAD			Buckets[1];
};

// TI = 0x198f
enum _KE_WAKE_SOURCE_TYPE {
	KeWakeSourceTypeSpuriousWake		= 0,
	KeWakeSourceTypeSpuriousClock		= 1,
	KeWakeSourceTypeSpuriousInterrupt	= 2,
	KeWakeSourceTypeQueryFailure		= 3,
	KeWakeSourceTypeAccountingFailure	= 4,
	KeWakeSourceTypeStaticSourceMax		= 4,
	KeWakeSourceTypeInterrupt			= 5,
	KeWakeSourceTypeIRTimer				= 6,
	KeWakeSourceTypeMax					= 7,
};

// TI = 0x1998
enum _PS_PROTECTED_TYPE {
	PsProtectedTypeNone				= 0,
	PsProtectedTypeProtectedLight	= 1,
	PsProtectedTypeProtected		= 2,
	PsProtectedTypeMax				= 3,
};

// TI = 0x19ac
enum _PROCESS_VA_TYPE {
	ProcessVAImage		= 0,
	ProcessVASection	= 1,
	ProcessVAPrivate	= 2,
	ProcessVAMax		= 3,
};

// TI = 0x19ae
enum _PS_RESOURCE_TYPE {
	PsResourceNonPagedPool	= 0,
	PsResourcePagedPool		= 1,
	PsResourcePageFile		= 2,
	PsResourceWorkingSet	= 3,
	PsResourceMax			= 4,
};

// TI = 0x19b8
enum _HEAP_SEGMGR_LARGE_PAGE_POLICY {
	HeapSegMgrNoLargePages			= 0,
	HeapSegMgrEnableLargePages		= 1,
	HeapSegMgrNormalPolicy			= 1,
	HeapSegMgrForceSmall			= 2,
	HeapSegMgrForceLarge			= 3,
	HeapSegMgrForceRandom			= 4,
	HeapSegMgrLargePagePolicyMax	= 5,
};

// TI = 0x19c0
enum _PERFINFO_KERNELMEMORY_USAGE_TYPE {
	PerfInfoMemUsagePfnMetadata	= 0,
	PerfInfoMemUsageMax			= 1,
};

// TI = 0x19da
enum _PS_PROTECTED_SIGNER {
	PsProtectedSignerNone			= 0,
	PsProtectedSignerAuthenticode	= 1,
	PsProtectedSignerCodeGen		= 2,
	PsProtectedSignerAntimalware	= 3,
	PsProtectedSignerLsa			= 4,
	PsProtectedSignerWindows		= 5,
	PsProtectedSignerWinTcb			= 6,
	PsProtectedSignerWinSystem		= 7,
	PsProtectedSignerApp			= 8,
	PsProtectedSignerMax			= 9,
};

// TI = 0x19dc
enum _WORKING_SET_TYPE {
	WorkingSetTypeUser				= 0,
	WorkingSetTypeSession			= 1,
	WorkingSetTypeSystemTypes		= 2,
	WorkingSetTypeSystemCache		= 2,
	WorkingSetTypePagedPool			= 3,
	WorkingSetTypeSystemViews		= 4,
	WorkingSetTypePagableMaximum	= 4,
	WorkingSetTypeSystemPtes		= 5,
	WorkingSetTypeKernelStacks		= 6,
	WorkingSetTypeNonPagedPool		= 7,
	WorkingSetTypeMaximum			= 8,
};

// TI = 0x19e7
enum DISPLAYCONFIG_SCANLINE_ORDERING {
	DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED					= 0,
	DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE					= 1,
	DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED					= 2,
	DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED_UPPERFIELDFIRST	= 2,
	DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED_LOWERFIELDFIRST	= 3,
	DISPLAYCONFIG_SCANLINE_ORDERING_FORCE_UINT32				= -1,
};

// TI = 0x19ef - forward reference
struct _RTL_BALANCED_LINKS;

// TI = 0x1c8b
struct _RTL_BALANCED_LINKS {
	_RTL_BALANCED_LINKS	*Parent;
	_RTL_BALANCED_LINKS	*LeftChild;
	_RTL_BALANCED_LINKS	*RightChild;
	char				Balance;
	uint8				Reserved[3];
};

// TI = 0x19ed - forward reference
struct _DPH_HEAP_BLOCK;

// TI = 0x19f1
struct _DPH_HEAP_BLOCK {
	union {
		_DPH_HEAP_BLOCK		*pNextAlloc;
		_LIST_ENTRY			AvailableEntry;
		_RTL_BALANCED_LINKS	TableLinks;
	};
	char				_pdb_padding0[24];
	uint8				*pUserAllocation;
	uint8				*pVirtualBlock;
	uint64				nVirtualBlockSize;
	uint64				nVirtualAccessSize;
	uint64				nUserRequestedSize;
	uint64				nUserActualSize;
	void				*UserValue;
	uint32				UserFlags;
	_RTL_TRACE_BLOCK	*StackTrace;
	_LIST_ENTRY			AdjacencyEntry;
	uint8				*pVirtualRegion;
};

// TI = 0x19fa
enum PS_CREATE_STATE {
	PsCreateInitialState		= 0,
	PsCreateFailOnFileOpen		= 1,
	PsCreateFailOnSectionCreate	= 2,
	PsCreateFailExeFormat		= 3,
	PsCreateFailMachineMismatch	= 4,
	PsCreateFailExeName			= 5,
	PsCreateSuccess				= 6,
	PsCreateMaximumStates		= 7,
};

// TI = 0x19ff
enum _KTHREAD_PPM_POLICY {
	ThreadPpmDefault		= 0,
	ThreadPpmThrottle		= 1,
	ThreadPpmSemiThrottle	= 2,
	ThreadPpmNoThrottle		= 3,
	MaxThreadPpmPolicy		= 4,
};

// TI = 0x1a0c
enum _VRF_RULE_CLASS_ID {
	VrfSpecialPoolRuleClass				= 0,
	VrfForceIrqlRuleClass				= 1,
	VrfAllocationFailuresRuleClass		= 2,
	VrfTrackingPoolAllocationsRuleClass	= 3,
	VrfIORuleClass						= 4,
	VrfDeadlockPreventionRuleClass		= 5,
	VrfEnhancedIORuleClass				= 6,
	VrfDMARuleClass						= 7,
	VrfSecurityRuleClass				= 8,
	VrfForcePendingIORequestRuleClass	= 9,
	VrfIRPTrackingRuleClass				= 10,
	VrfMiscellaneousRuleClass			= 11,
	VrfMoreDebuggingRuleClass			= 12,
	VrfMDLInvariantStackRuleClass		= 13,
	VrfMDLInvariantDriverRuleClass		= 14,
	VrfPowerDelayFuzzingRuleClass		= 15,
	VrfPortMiniportRuleClass			= 16,
	VrfStandardDDIRuleClass				= 17,
	VrfAutoFailRuleClass				= 18,
	VrfAdditionalDDIRuleClass			= 19,
	VrfRuleClassBase					= 20,
	VrfNdisWifiRuleClass				= 21,
	VrfDriverLoggingRuleClass			= 22,
	VrfSyncDelayFuzzingRuleClass		= 23,
	VrfVMSwitchingRuleClass				= 24,
	VrfCodeIntegrityRuleClass			= 25,
	VrfBelow4GBAllocationRuleClass		= 26,
	VrfProcessorBranchTraceRuleClass	= 27,
	VrfAdvancedMMRuleClass				= 28,
	VrfExtendingXDVTimeLimit			= 29,
	VrfSystemBIOSRuleClass				= 30,
	VrfHardwareRuleClass				= 31,
	VrfStateSepRuleClass				= 32,
	VrfWDFRuleClass						= 33,
	VrfMoreIrqlRuleClass				= 34,
	VrfXDVPlatformMode					= 35,
	VrfStandalonePlatformMode			= 36,
	VrfPlatformModeTest					= 37,
	ReservedForDVRF38					= 38,
	ReservedForDVRF39					= 39,
	ReservedForDVRF40					= 40,
	ReservedForDVRF41					= 41,
	ReservedForDVRF42					= 42,
	ReservedForDVRF43					= 43,
	ReservedForDVRF44					= 44,
	ReservedForDVRF45					= 45,
	ReservedForDVRF46					= 46,
	ReservedForDVRF47					= 47,
	ReservedForDVRF48					= 48,
	ReservedForDVRF49					= 49,
	ReservedForDVRF50					= 50,
	ReservedForDVRF51					= 51,
	ReservedForDVRF52					= 52,
	ReservedForDVRF53					= 53,
	ReservedForDVRF54					= 54,
	ReservedForDVRF55					= 55,
	ReservedForDVRF56					= 56,
	ReservedForDVRF57					= 57,
	ReservedForDVRF58					= 58,
	ReservedForDVRF59					= 59,
	ReservedForDVRF60					= 60,
	ReservedForDVRF61					= 61,
	ReservedForDVRF62					= 62,
	ReservedForDVRF63					= 63,
	VrfRuleClassSizeMax					= 64,
};

// TI = 0x1a12
enum _KPROCESS_PPM_POLICY {
	ProcessPpmDefault			= 0,
	ProcessPpmThrottle			= 1,
	ProcessPpmSemiThrottle		= 2,
	ProcessPpmNoThrottle		= 3,
	ProcessPpmWindowMinimized	= 4,
	ProcessPpmWindowOccluded	= 5,
	ProcessPpmWindowVisible		= 6,
	ProcessPpmWindowInFocus		= 7,
	MaxProcessPpmPolicy			= 8,
};

// TI = 0x1a18
enum _MEMORY_CACHING_TYPE_ORIG {
	MmFrameBufferCached	= 2,
};

// TI = 0x1a3c
enum _INTERLOCKED_RESULT {
	ResultNegative	= 1,
	ResultZero		= 0,
	ResultPositive	= 2,
};

// TI = 0x1a44
enum _SYSTEM_PROCESS_CLASSIFICATION {
	SystemProcessClassificationNormal			= 0,
	SystemProcessClassificationSystem			= 1,
	SystemProcessClassificationSecureSystem		= 2,
	SystemProcessClassificationMemCompression	= 3,
	SystemProcessClassificationRegistry			= 4,
	SystemProcessClassificationMaximum			= 5,
};

// TI = 0x1a4a
enum _WOW64_SHARED_INFORMATION {
	SharedNtdll32LdrInitializeThunk						= 0,
	SharedNtdll32KiUserExceptionDispatcher				= 1,
	SharedNtdll32KiUserApcDispatcher					= 2,
	SharedNtdll32KiUserCallbackDispatcher				= 3,
	SharedNtdll32RtlUserThreadStart						= 4,
	SharedNtdll32pQueryProcessDebugInformationRemote	= 5,
	SharedNtdll32BaseAddress							= 6,
	SharedNtdll32LdrSystemDllInitBlock					= 7,
	SharedNtdll32RtlpFreezeTimeBias						= 8,
	Wow64SharedPageEntriesCount							= 9,
};

// TI = 0x1a5a
enum _KWAIT_STATE {
	WaitInProgress			= 0,
	WaitCommitted			= 1,
	WaitAborted				= 2,
	WaitSuspendInProgress	= 3,
	WaitSuspended			= 4,
	WaitResumeInProgress	= 5,
	WaitResumeAborted		= 6,
	WaitFirstSuspendState	= 3,
	WaitLastSuspendState	= 6,
	MaximumWaitState		= 7,
};

// TI = 0x1a84
enum _USER_ACTIVITY_PRESENCE {
	PowerUserPresent	= 0,
	PowerUserNotPresent	= 1,
	PowerUserInactive	= 2,
	PowerUserMaximum	= 3,
	PowerUserInvalid	= 3,
};

// TI = 0x1a95
enum _KPROCESS_STATE {
	ProcessInMemory			= 0,
	ProcessOutOfMemory		= 1,
	ProcessInTransition		= 2,
	ProcessOutTransition	= 3,
	ProcessInSwap			= 4,
	ProcessOutSwap			= 5,
	ProcessRetryOutSwap		= 6,
	ProcessAllSwapStates	= 7,
};

// TI = 0x1aa2
enum _INVPCID_TYPE {
	InvpcidIndividualAddress	= 0,
	InvpcidSingleContext		= 1,
	InvpcidAllContextAndGlobals	= 2,
	InvpcidAllContext			= 3,
};

// TI = 0x1abd
union _LFH_RANDOM_DATA {
	uint8	Bytes[256];
	uint16	Words[128];
	uint64	Quadwords[32];
};

// TI = 0x1ad8
enum _TRACE_INFORMATION_CLASS {
	TraceIdClass					= 0,
	TraceHandleClass				= 1,
	TraceEnableFlagsClass			= 2,
	TraceEnableLevelClass			= 3,
	GlobalLoggerHandleClass			= 4,
	EventLoggerHandleClass			= 5,
	AllLoggerHandlesClass			= 6,
	TraceHandleByNameClass			= 7,
	LoggerEventsLostClass			= 8,
	TraceSessionSettingsClass		= 9,
	LoggerEventsLoggedClass			= 10,
	DiskIoNotifyRoutinesClass		= 11,
	TraceInformationClassReserved1	= 12,
	AllPossibleNotifyRoutinesClass	= 12,
	FltIoNotifyRoutinesClass		= 13,
	TraceInformationClassReserved2	= 14,
	WdfNotifyRoutinesClass			= 15,
	MaxTraceInformationClass		= 16,
};

// TI = 0x1b0c
enum _PERFINFO_MM_STAT {
	PerfInfoMMStatNotUsed				= 0,
	PerfInfoMMStatAggregatePageCombine	= 1,
	PerfInfoMMStatIterationPageCombine	= 2,
	PerfInfoMMStatMax					= 3,
};

// TI = 0x1b10
struct _PS_TRUSTLET_TKSESSION_ID {
	uint64	SessionId[4];
};

// TI = 0x1b15
struct _HEAP_GLOBAL_APPCOMPAT_FLAGS {
	uint32	SafeInputValidation:1;
	uint32	Padding:1;
	uint32	CommitLFHSubsegments:1;
	uint32	AllocateHeapFromEnv:1;
};

// TI = 0x1b50
enum _THREAD_WORKLOAD_CLASS {
	ThreadWorkloadClassDefault	= 0,
	ThreadWorkloadClassGraphics	= 1,
	MaxThreadWorkloadClass		= 2,
};

// TI = 0x1b62 - forward reference
struct _RTL_AVL_TABLE;

// TI = 0x1c23
enum _RTL_GENERIC_COMPARE_RESULTS {
	GenericLessThan		= 0,
	GenericGreaterThan	= 1,
	GenericEqual		= 2,
};

// TI = 0x1c2f
struct _RTL_AVL_TABLE {
	_RTL_BALANCED_LINKS				BalancedRoot;
	void							*OrderedPointer;
	uint32							WhichOrderedElement;
	uint32							NumberGenericTableElements;
	uint32							DepthOfTree;
	_RTL_BALANCED_LINKS				*RestartKey;
	uint32							DeleteCount;
	_RTL_GENERIC_COMPARE_RESULTS (*CompareRoutine)(_RTL_AVL_TABLE *, void*, void*);
	void *(*AllocateRoutine)(_RTL_AVL_TABLE *, uint32);
	void (*FreeRoutine)(_RTL_AVL_TABLE *, void*);
	void							*TableContext;
};

// TI = 0x1b64
struct _DPH_HEAP_ROOT {
	uint32					Signature;
	uint32					HeapFlags;
	_RTL_CRITICAL_SECTION	*HeapCritSect;
	uint32					nRemoteLockAcquired;
	_DPH_HEAP_BLOCK			*pVirtualStorageListHead;
	_DPH_HEAP_BLOCK			*pVirtualStorageListTail;
	uint32					nVirtualStorageRanges;
	uint64					nVirtualStorageBytes;
	_RTL_AVL_TABLE			BusyNodesTable;
	_DPH_HEAP_BLOCK			*NodeToAllocate;
	uint32					nBusyAllocations;
	uint64					nBusyAllocationBytesCommitted;
	_DPH_HEAP_BLOCK			*pFreeAllocationListHead;
	_DPH_HEAP_BLOCK			*pFreeAllocationListTail;
	uint32					nFreeAllocations;
	uint64					nFreeAllocationBytesCommitted;
	_LIST_ENTRY				AvailableAllocationHead;
	uint32					nAvailableAllocations;
	uint64					nAvailableAllocationBytesCommitted;
	_DPH_HEAP_BLOCK			*pUnusedNodeListHead;
	_DPH_HEAP_BLOCK			*pUnusedNodeListTail;
	uint32					nUnusedNodes;
	uint64					nBusyAllocationBytesAccessible;
	_DPH_HEAP_BLOCK			*pNodePoolListHead;
	_DPH_HEAP_BLOCK			*pNodePoolListTail;
	uint32					nNodePools;
	uint64					nNodePoolBytes;
	_LIST_ENTRY				NextHeap;
	uint32					ExtraFlags;
	uint32					Seed;
	void					*NormalHeap;
	_RTL_TRACE_BLOCK		*CreateStackTrace;
	void					*FirstThread;
};

// TI = 0x1ba8
struct BATTERY_REPORTING_SCALE {
	uint32	Granularity;
	uint32	Capacity;
};

// TI = 0x1b6d
struct SYSTEM_POWER_CAPABILITIES {
	uint8					PowerButtonPresent;
	uint8					SleepButtonPresent;
	uint8					LidPresent;
	uint8					SystemS1;
	uint8					SystemS2;
	uint8					SystemS3;
	uint8					SystemS4;
	uint8					SystemS5;
	uint8					HiberFilePresent;
	uint8					FullWake;
	uint8					VideoDimPresent;
	uint8					ApmPresent;
	uint8					UpsPresent;
	uint8					ThermalControl;
	uint8					ProcessorThrottle;
	uint8					ProcessorMinThrottle;
	uint8					ProcessorMaxThrottle;
	uint8					FastSystemS4;
	uint8					Hiberboot;
	uint8					WakeAlarmPresent;
	uint8					AoAc;
	uint8					DiskSpinDown;
	uint8					HiberFileType;
	uint8					AoAcConnectivitySupported;
	uint8					spare3[6];
	uint8					SystemBatteriesPresent;
	uint8					BatteriesAreShortTerm;
	BATTERY_REPORTING_SCALE	BatteryScale[3];
	_SYSTEM_POWER_STATE		AcOnLineWake;
	_SYSTEM_POWER_STATE		SoftLidWake;
	_SYSTEM_POWER_STATE		RtcWake;
	_SYSTEM_POWER_STATE		MinDeviceWakeState;
	_SYSTEM_POWER_STATE		DefaultLowLatencyWake;
};

// TI = 0x1b70
union RTLP_HP_LFH_PERF_FLAGS {
	struct {
		uint32	HotspotDetection:1;
		uint32	HotspotFullCommit:1;
		uint32	ActiveSubsegment:1;
		uint32	SmallerSubsegment:1;
		uint32	SingleAffinitySlot:1;
		uint32	ApplyLfhDecommitPolicy:1;
		uint32	EnableGarbageCollection:1;
		uint32	LargePagePreCommit:1;
		uint32	OpportunisticLargePreCommit:1;
		uint32	LfhForcedAffinity:1;
		uint32	LfhCachelinePadding:1;
	};
	uint32	AllFlags;
};

// TI = 0x1b7b
enum _PS_ATTRIBUTE_NUM {
	PsAttributeParentProcess				= 0,
	PsAttributeDebugObject					= 1,
	PsAttributeToken						= 2,
	PsAttributeClientId						= 3,
	PsAttributeTebAddress					= 4,
	PsAttributeImageName					= 5,
	PsAttributeImageInfo					= 6,
	PsAttributeMemoryReserve				= 7,
	PsAttributePriorityClass				= 8,
	PsAttributeErrorMode					= 9,
	PsAttributeStdHandleInfo				= 10,
	PsAttributeHandleList					= 11,
	PsAttributeGroupAffinity				= 12,
	PsAttributePreferredNode				= 13,
	PsAttributeIdealProcessor				= 14,
	PsAttributeUmsThread					= 15,
	PsAttributeMitigationOptions			= 16,
	PsAttributeProtectionLevel				= 17,
	PsAttributeSecureProcess				= 18,
	PsAttributeJobList						= 19,
	PsAttributeChildProcessPolicy			= 20,
	PsAttributeAllApplicationPackagesPolicy	= 21,
	PsAttributeWin32kFilter					= 22,
	PsAttributeSafeOpenPromptOriginClaim	= 23,
	PsAttributeBnoIsolation					= 24,
	PsAttributeDesktopAppPolicy				= 25,
	PsAttributeChpe							= 26,
	PsAttributeMax							= 27,
};

// TI = 0x1b9a
enum _PROCESS_TERMINATE_REQUEST_REASON {
	ProcessTerminateRequestReasonNone				= 0,
	ProcessTerminateCommitFail						= 1,
	ProcessTerminateWriteToExecuteMemory			= 2,
	ProcessTerminateAttachedWriteToExecuteMemory	= 3,
	ProcessTerminateRequestReasonMax				= 4,
};

// TI = 0x1ba4
enum _EXQUEUEINDEX {
	ExPoolUntrusted	= 0,
	IoPoolUntrusted	= 1,
	ExPoolMax		= 8,
};

// TI = 0x1ba6
enum LSA_FOREST_TRUST_RECORD_TYPE {
	ForestTrustTopLevelName		= 0,
	ForestTrustTopLevelNameEx	= 1,
	ForestTrustDomainInfo		= 2,
	ForestTrustRecordTypeLast	= 2,
};

// TI = 0x1bae
enum ReplacesCorHdrNumericDefines {
	COMIMAGE_FLAGS_ILONLY						= 1,
	COMIMAGE_FLAGS_32BITREQUIRED				= 2,
	COMIMAGE_FLAGS_IL_LIBRARY					= 4,
	COMIMAGE_FLAGS_STRONGNAMESIGNED				= 8,
	COMIMAGE_FLAGS_NATIVE_ENTRYPOINT			= 16,
	COMIMAGE_FLAGS_TRACKDEBUGDATA				= 65536,
	COMIMAGE_FLAGS_32BITPREFERRED				= 131072,
	COR_VERSION_MAJOR_V2						= 2,
	COR_VERSION_MAJOR							= 2,
	COR_VERSION_MINOR							= 5,
	COR_DELETED_NAME_LENGTH						= 8,
	COR_VTABLEGAP_NAME_LENGTH					= 8,
	NATIVE_TYPE_MAX_CB							= 1,
	COR_ILMETHOD_SECT_SMALL_MAX_DATASIZE		= 255,
	IMAGE_COR_MIH_METHODRVA						= 1,
	IMAGE_COR_MIH_EHRVA							= 2,
	IMAGE_COR_MIH_BASICBLOCK					= 8,
	COR_VTABLE_32BIT							= 1,
	COR_VTABLE_64BIT							= 2,
	COR_VTABLE_FROM_UNMANAGED					= 4,
	COR_VTABLE_FROM_UNMANAGED_RETAIN_APPDOMAIN	= 8,
	COR_VTABLE_CALL_MOST_DERIVED				= 16,
	IMAGE_COR_EATJ_THUNK_SIZE					= 32,
	MAX_CLASS_NAME								= 1024,
	MAX_PACKAGE_NAME							= 1024,
};

// TI = 0x1bb2
enum JOB_OBJECT_NET_RATE_CONTROL_FLAGS {
	JOB_OBJECT_NET_RATE_CONTROL_ENABLE			= 1,
	JOB_OBJECT_NET_RATE_CONTROL_MAX_BANDWIDTH	= 2,
	JOB_OBJECT_NET_RATE_CONTROL_DSCP_TAG		= 4,
	JOB_OBJECT_NET_RATE_CONTROL_VALID_FLAGS		= 7,
};

// TI = 0x1bd5
struct _DPH_BLOCK_INFORMATION {
	uint32			StartStamp;
	void			*Heap;
	uint64			RequestedSize;
	uint64			ActualSize;
	union {
		_LIST_ENTRY		FreeQueue;
		_SLIST_ENTRY	FreePushList;
		uint16			TraceIndex;
	};
	void			*StackTrace;
	uint32			Padding;
	uint32			EndStamp;
};

// TI = 0x1c0e
struct _MM_DRIVER_VERIFIER_DATA {
	uint32			Level;
	volatile uint32	RaiseIrqls;
	volatile uint32	AcquireSpinLocks;
	volatile uint32	SynchronizeExecutions;
	volatile uint32	AllocationsAttempted;
	volatile uint32	AllocationsSucceeded;
	volatile uint32	AllocationsSucceededSpecialPool;
	uint32			AllocationsWithNoTag;
	uint32			TrimRequests;
	uint32			Trims;
	uint32			AllocationsFailed;
	volatile uint32	AllocationsFailedDeliberately;
	volatile uint32	Loads;
	volatile uint32	Unloads;
	uint32			UnTrackedPool;
	uint32			UserTrims;
	volatile uint32	CurrentPagedPoolAllocations;
	volatile uint32	CurrentNonPagedPoolAllocations;
	uint32			PeakPagedPoolAllocations;
	uint32			PeakNonPagedPoolAllocations;
	volatile uint64	PagedBytes;
	volatile uint64	NonPagedBytes;
	uint64			PeakPagedBytes;
	uint64			PeakNonPagedBytes;
	volatile uint32	BurstAllocationsFailedDeliberately;
	uint32			SessionTrims;
	volatile uint32	OptionChanges;
	volatile uint32	VerifyMode;
	_UNICODE_STRING	PreviousBucketName;
	volatile uint32	ExecutePoolTypes;
	volatile uint32	ExecutePageProtections;
	volatile uint32	ExecutePageMappings;
	volatile uint32	ExecuteWriteSections;
	volatile uint32	SectionAlignmentFailures;
	volatile uint32	IATInExecutableSection;
};

// TI = 0x1d9d
union _MCI_ADDR {
	struct {
		uint32	Address;
		uint32	Reserved;
	};
	uint64	QuadPart;
};

// TI = 0x1db7
union _MCI_STATS {
	struct {
		uint16	McaErrorCode;
		uint16	ModelErrorCode;
		uint32	OtherInformation:25;
		uint32	ContextCorrupt:1;
		uint32	AddressValid:1;
		uint32	MiscValid:1;
		uint32	ErrorEnabled:1;
		uint32	UncorrectedError:1;
		uint32	StatusOverFlow:1;
		uint32	Valid:1;
	}		MciStatus;
	uint64	QuadPart;
};

// TI = 0x1c4c
enum MCA_EXCEPTION_TYPE {
	HAL_MCE_RECORD	= 0,
	HAL_MCA_RECORD	= 1,
};

// TI = 0x1c57
struct _MCA_EXCEPTION {
	uint32				VersionNumber;
	MCA_EXCEPTION_TYPE	ExceptionType;
	_LARGE_INTEGER		TimeStamp;
	uint32				ProcessorNumber;
	uint32				Reserved1;
	union  {
		struct {
			uint8		BankNumber;
			uint8		Reserved2[7];
			_MCI_STATS	Status;
			_MCI_ADDR	Address;
			uint64		Misc;
		}		Mca;
		struct {
			uint64	Address;
			uint64	Type;
		}		Mce;
	}					u;
	uint32				ExtCnt;
	uint32				Reserved3;
	uint64				ExtReg[24];
};

// TI = 0x1c9c
struct _HEAP_UCR_DESCRIPTOR {
	_LIST_ENTRY	ListEntry;
	_LIST_ENTRY	SegmentEntry;
	void		*Address;
	uint64		Size;
};

// TI = 0x1cab
enum _MACHINE_CHECK_NESTING_LEVEL {
	McheckNormal		= 0,
	McheckNmi			= 1,
	McheckNestingLevels	= 2,
};

// TI = 0x1cd8
struct _PEB32 {
	uint8			InheritedAddressSpace;
	uint8			ReadImageFileExecOptions;
	uint8			BeingDebugged;
	union {
		uint8			BitField;
		struct {
			uint8			ImageUsesLargePages:1;
			uint8			IsProtectedProcess:1;
			uint8			IsImageDynamicallyRelocated:1;
			uint8			SkipPatchingUser32Forwarders:1;
			uint8			IsPackagedProcess:1;
			uint8			IsAppContainer:1;
			uint8			IsProtectedProcessLight:1;
			uint8			IsLongPathAwareProcess:1;
		};
	};
	uint32			Mutant;
	uint32			ImageBaseAddress;
	uint32			Ldr;
	uint32			ProcessParameters;
	uint32			SubSystemData;
	uint32			ProcessHeap;
	uint32			FastPebLock;
	uint32			AtlThunkSListPtr;
	uint32			IFEOKey;
	union {
		uint32			CrossProcessFlags;
		struct {
			uint32			ProcessInJob:1;
			uint32			ProcessInitializing:1;
			uint32			ProcessUsingVEH:1;
			uint32			ProcessUsingVCH:1;
			uint32			ProcessUsingFTH:1;
			uint32			ProcessPreviouslyThrottled:1;
			uint32			ProcessCurrentlyThrottled:1;
			uint32			ProcessImagesHotPatched:1;
			uint32			ReservedBits0:24;
		};
	};
	union {
		uint32			KernelCallbackTable;
		uint32			UserSharedInfoPtr;
	};
	uint32			SystemReserved;
	uint32			AtlThunkSListPtr32;
	uint32			ApiSetMap;
	uint32			TlsExpansionCounter;
	uint32			TlsBitmap;
	uint32			TlsBitmapBits[2];
	uint32			ReadOnlySharedMemoryBase;
	uint32			SharedData;
	uint32			ReadOnlyStaticServerData;
	uint32			AnsiCodePageData;
	uint32			OemCodePageData;
	uint32			UnicodeCaseTableData;
	uint32			NumberOfProcessors;
	uint32			NtGlobalFlag;
	_LARGE_INTEGER	CriticalSectionTimeout;
	uint32			HeapSegmentReserve;
	uint32			HeapSegmentCommit;
	uint32			HeapDeCommitTotalFreeThreshold;
	uint32			HeapDeCommitFreeBlockThreshold;
	uint32			NumberOfHeaps;
	uint32			MaximumNumberOfHeaps;
	uint32			ProcessHeaps;
	uint32			GdiSharedHandleTable;
	uint32			ProcessStarterHelper;
	uint32			GdiDCAttributeList;
	uint32			LoaderLock;
	uint32			OSMajorVersion;
	uint32			OSMinorVersion;
	uint16			OSBuildNumber;
	uint16			OSCSDVersion;
	uint32			OSPlatformId;
	uint32			ImageSubsystem;
	uint32			ImageSubsystemMajorVersion;
	uint32			ImageSubsystemMinorVersion;
	uint32			ActiveProcessAffinityMask;
	uint32			GdiHandleBuffer[34];
	uint32			PostProcessInitRoutine;
	uint32			TlsExpansionBitmap;
	uint32			TlsExpansionBitmapBits[32];
	uint32			SessionId;
	_ULARGE_INTEGER	AppCompatFlags;
	_ULARGE_INTEGER	AppCompatFlagsUser;
	uint32			pShimData;
	uint32			AppCompatInfo;
	_STRING32		CSDVersion;
	uint32			ActivationContextData;
	uint32			ProcessAssemblyStorageMap;
	uint32			SystemDefaultActivationContextData;
	uint32			SystemAssemblyStorageMap;
	uint32			MinimumStackCommit;
	uint32			FlsCallback;
	LIST_ENTRY32	FlsListHead;
	uint32			FlsBitmap;
	uint32			FlsBitmapBits[4];
	uint32			FlsHighIndex;
	uint32			WerRegistrationData;
	uint32			WerShipAssertPtr;
	uint32			pUnused;
	uint32			pImageHeaderHash;
	union {
		uint32			TracingFlags;
		struct {
			uint32			HeapTracingEnabled:1;
			uint32			CritSecTracingEnabled:1;
			uint32			LibLoaderTracingEnabled:1;
			uint32			SpareTracingBits:29;
		};
	};
	uint64			CsrServerReadOnlySharedMemoryBase;
	uint32			TppWorkerpListLock;
	LIST_ENTRY32	TppWorkerpList;
	uint32			WaitOnAddressHashTable[128];
	uint32			TelemetryCoverageHeader;
	uint32			CloudFileFlags;
	uint32			CloudFileDiagFlags;
	char			PlaceholderCompatibilityMode;
	char			PlaceholderCompatibilityModeReserved[7];
	uint32			LeapSecondData;
	union {
		uint32			LeapSecondFlags;
		struct {
			uint32			SixtySecondEnabled:1;
			uint32			Reserved:31;
		};
	};
	uint32			NtGlobalFlag2;
};

// TI = 0x1ceb
struct _PF_KERNEL_GLOBALS {
	uint64			AccessBufferAgeThreshold;
	_EX_RUNDOWN_REF	AccessBufferRef;
	_KEVENT			AccessBufferExistsEvent;
	uint32			AccessBufferMax;
	char			_pdb_padding0[16];
	_SLIST_HEADER	AccessBufferList;
	int				StreamSequenceNumber;
	uint32			Flags;
	int				ScenarioPrefetchCount;
};