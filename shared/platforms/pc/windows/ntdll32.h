union _KSTACK_COUNT {
	int32	Value;
	struct {
		uint32	State:3, StackCount:29;
	};
};
union _LARGE_INTEGER {
	struct {
		uint32	LowPart;
		int32	HighPart;
	};
	struct {
		uint32	LowPart;
		int32	HighPart;
	}	u;
	int64	QuadPart;
};
enum _IRQ_PRIORITY {
	IrqPriorityUndefined	= 0,
	IrqPriorityLow	= 1,
	IrqPriorityNormal	= 2,
	IrqPriorityHigh	= 3,
};
struct _IO_RESOURCE_DESCRIPTOR {
	uint8	Option;
	uint8	Type;
	uint8	ShareDisposition;
	uint8	Spare1;
	uint16	Flags;
	uint16	Spare2;
	union {
		struct {
			uint32	Length;
			uint32	Alignment;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}	Port;
		struct {
			uint32	Length;
			uint32	Alignment;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}	Memory;
		struct {
			uint32	MinimumVector;
			uint32	MaximumVector;
			uint16	AffinityPolicy;
			uint16	Group;
			_IRQ_PRIORITY	PriorityPolicy;
			uint32	TargetedProcessors;
		}	Interrupt;
		struct {
			uint32	MinimumChannel;
			uint32	MaximumChannel;
		}	Dma;
		struct {
			uint32	RequestLine;
			uint32	Reserved;
			uint32	Channel;
			uint32	TransferWidth;
		}	DmaV3;
		struct {
			uint32	Length;
			uint32	Alignment;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}	Generic;
		struct {
			uint32	Data[3];
		}	DevicePrivate;
		struct {
			uint32	Length;
			uint32	MinBusNumber;
			uint32	MaxBusNumber;
			uint32	Reserved;
		}	BusNumber;
		struct {
			uint32	Priority;
			uint32	Reserved1;
			uint32	Reserved2;
		}	ConfigData;
		struct {
			uint32	Length40;
			uint32	Alignment40;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}	Memory40;
		struct {
			uint32	Length48;
			uint32	Alignment48;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}	Memory48;
		struct {
			uint32	Length64;
			uint32	Alignment64;
			_LARGE_INTEGER	MinimumAddress;
			_LARGE_INTEGER	MaximumAddress;
		}	Memory64;
		struct {
			uint8	Class;
			uint8	Type;
			uint8	Reserved1;
			uint8	Reserved2;
			uint32	IdLowPart;
			uint32	IdHighPart;
		}	Connection;
	}	u;
};
struct _IO_RESOURCE_LIST {
	uint16	Version;
	uint16	Revision;
	uint32	Count;
	_IO_RESOURCE_DESCRIPTOR	Descriptors[1];
};
struct _SID_IDENTIFIER_AUTHORITY {
	uint8	Value[6];
};
union _ULARGE_INTEGER {
	struct {
		uint32	LowPart;
		uint32	HighPart;
	};
	struct {
		uint32	LowPart;
		uint32	HighPart;
	}	u;
	uint64	QuadPart;
};
union _HEAP_VS_CHUNK_HEADER_SIZE {
	struct {
		uint32	MemoryCost:1, UnsafeSize:15, UnsafePrevSize:15, Allocated:1;
	};
	uint16	KeyUShort;
	uint32	KeyULong;
	uint32	HeaderBits;
};
struct _HEAP_VS_CHUNK_HEADER {
	_HEAP_VS_CHUNK_HEADER_SIZE	Sizes;
	union {
		struct {
			uint32	EncodedSegmentPageOffset:8, UnusedBytes:1, SkipDuringWalk:1, Spare:22;
		};
		uint32	AllocatedChunkBits;
	};
};
struct _RTL_BALANCED_NODE {
	union {
		pointer32<_RTL_BALANCED_NODE>	Children[2];
		struct {
			pointer32<_RTL_BALANCED_NODE>	Left;
			pointer32<_RTL_BALANCED_NODE>	Right;
		};
	};
	union {
		uint8	Red:1;
		uint8	Balance:2;
		uint32	ParentValue;
	};
};
struct _HEAP_VS_CHUNK_FREE_HEADER {
	union {
		_HEAP_VS_CHUNK_HEADER	Header;
		struct {
			uint32	OverlapsHeader;
			_RTL_BALANCED_NODE	Node;
		};
	};
};
struct _UNICODE_STRING {
	uint16	Length;
	uint16	MaximumLength;
	pointer32<uint16>	Buffer;
};
struct _LIST_ENTRY {
	pointer32<_LIST_ENTRY>	Flink;
	pointer32<_LIST_ENTRY>	Blink;
};
struct _KDEVICE_QUEUE_ENTRY {
	_LIST_ENTRY	DeviceListEntry;
	uint32	SortKey;
	uint8	Inserted;
};
enum _IO_ALLOCATION_ACTION {
	KeepObject	= 1,
	DeallocateObject	= 2,
	DeallocateObjectKeepRegisters	= 3,
};
struct _DEVICE_OBJECT;
struct _IO_STATUS_BLOCK {
	union {
		int32	Status;
		pointer32<void>	Pointer;
	};
	uint32	Information;
};
struct _DISPATCHER_HEADER {
	union {
		int32	Lock;
		int32	LockNV;
		struct {
			uint8	Type;
			uint8	Signalling;
			uint8	Size;
			uint8	Reserved1;
		};
		struct {
			uint8	TimerType;
			union {
				uint8	TimerControlFlags;
				struct {
					uint8	Absolute:1, Wake:1, EncodedTolerableDelay:6;
				};
			};
			uint8	Hand;
			union {
				uint8	TimerMiscFlags;
				struct {
					uint8	Index:1, Processor:5, Inserted:1, Expired:1;
				};
			};
		};
		struct {
			uint8	Timer2Type;
			union {
				uint8	Timer2Flags;
				struct {
					uint8	Timer2Inserted:1, Timer2Expiring:1, Timer2CancelPending:1, Timer2SetPending:1, Timer2Running:1, Timer2Disabled:1, Timer2ReservedFlags:2;
				};
			};
			uint8	Timer2Reserved1;
			uint8	Timer2Reserved2;
		};
		struct {
			uint8	QueueType;
			union {
				uint8	QueueControlFlags;
				struct {
					uint8	Abandoned:1, DisableIncrement:1, QueueReservedControlFlags:6;
				};
			};
			uint8	QueueSize;
			uint8	QueueReserved;
		};
		struct {
			uint8	ThreadType;
			uint8	ThreadReserved;
			union {
				uint8	ThreadControlFlags;
				struct {
					uint8	CycleProfiling:1, CounterProfiling:1, GroupScheduling:1, AffinitySet:1, Tagged:1, EnergyProfiling:1, Instrumented:1, ThreadReservedControlFlags:1;
				};
			};
			uint8	DebugActive;
		};
		struct {
			uint8	MutantType;
			uint8	MutantSize;
			uint8	DpcActive;
			uint8	MutantReserved;
		};
	};
	int32	SignalState;
	_LIST_ENTRY	WaitListHead;
};
union _KWAIT_STATUS_REGISTER {
	uint8	Flags;
	struct {
		uint8	State:3, Affinity:1, Priority:1, Apc:1, UserApc:1, Alert:1;
	};
};
struct _KGDTENTRY {
	uint16	LimitLow;
	uint16	BaseLow;
	union {
		struct {
			uint8	BaseMid;
			uint8	Flags1;
			uint8	Flags2;
			uint8	BaseHi;
		}	Bytes;
		struct {
			uint32	BaseMid:8, Type:5, Dpl:2, Pres:1, LimitHi:4, Sys:1, Reserved_0:1, Default_Big:1, Granularity:1, BaseHi:8;
		}	Bits;
	}	HighWord;
};
struct _KIDTENTRY {
	uint16	Offset;
	uint16	Selector;
	uint16	Access;
	uint16	ExtendedOffset;
};
struct _KAFFINITY_EX {
	uint16	Count;
	uint16	Size;
	uint32	Reserved;
	uint32	Bitmap[20];
};
struct _SINGLE_LIST_ENTRY {
	pointer32<_SINGLE_LIST_ENTRY>	Next;
};
union _KEXECUTE_OPTIONS {
	struct {
		uint8	ExecuteDisable:1, ExecuteEnable:1, DisableThunkEmulation:1, Permanent:1, ExecuteDispatchEnable:1, ImageDispatchEnable:1, DisableExceptionChainValidation:1, Spare:1;
	};
	uint8	ExecuteOptions;
	uint8	ExecuteOptionsNV;
};
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
			uint32	Type:1, Disabled:1, Spare1:30;
		};
	};
};
struct _RTL_RB_TREE {
	pointer32<_RTL_BALANCED_NODE>	Root;
	pointer32<_RTL_BALANCED_NODE>	Min;
};
struct _KSCB {
	uint64	GenerationCycles;
	uint64	MinQuotaCycleTarget;
	uint64	MaxQuotaCycleTarget;
	uint64	RankCycleTarget;
	uint64	LongTermCycles;
	uint64	LastReportedCycles;
	uint64	OverQuotaHistory;
	uint64	ReadyTime;
	uint64	InsertTime;
	_LIST_ENTRY	PerProcessorList;
	_RTL_BALANCED_NODE	QueueNode;
	uint8	Inserted:1, MaxOverQuota:1, MinOverQuota:1, RankBias:1, SoftCap:1, Spare1:3;
	uint8	Depth;
	uint16	ReadySummary;
	uint32	Rank;
	_LIST_ENTRY	ReadyListHead[16];
	_RTL_RB_TREE	ChildScbQueue;
	pointer32<_KSCB>	Parent;
	pointer32<_KSCB>	Root;
};
struct _KDPC {
	union {
		uint32	TargetInfoAsUlong;
		struct {
			uint8	Type;
			uint8	Importance;
			uint16	Number;
		};
	};
	_SINGLE_LIST_ENTRY	DpcListEntry;
	uint32	ProcessorHistory;
	void	(*DeferredRoutine)(pointer32<_KDPC>, pointer32<void>, pointer32<void>, pointer32<void>);
	pointer32<void>	DeferredContext;
	pointer32<void>	SystemArgument1;
	pointer32<void>	SystemArgument2;
	pointer32<void>	DpcData;
};
struct _KSCHEDULING_GROUP {
	_KSCHEDULING_GROUP_POLICY	Policy;
	uint32	RelativeWeight;
	uint32	ChildMinRate;
	uint32	ChildMinWeight;
	uint32	ChildTotalWeight;
	uint64	QueryHistoryTimeStamp;
	int64	NotificationCycles;
	union {
		_LIST_ENTRY	SchedulingGroupList;
		_LIST_ENTRY	Sibling;
	};
	pointer32<_KDPC>	NotificationDpc;
	_LIST_ENTRY	ChildList;
	pointer32<_KSCHEDULING_GROUP>	Parent;
	_KSCB	PerProcessor[1];
};
struct _KPROCESS {
	_DISPATCHER_HEADER	Header;
	_LIST_ENTRY	ProfileListHead;
	uint32	DirectoryTableBase;
	_KGDTENTRY	LdtDescriptor;
	_KIDTENTRY	Int21Descriptor;
	_LIST_ENTRY	ThreadListHead;
	uint32	ProcessLock;
	uint64	DeepFreezeStartTime;
	_KAFFINITY_EX	Affinity;
	_LIST_ENTRY	ReadyListHead;
	_SINGLE_LIST_ENTRY	SwapListEntry;
	_KAFFINITY_EX	ActiveProcessors;
	union {
		struct {
			int32	AutoAlignment:1, DisableBoost:1, DisableQuantum:1;
			uint32	DeepFreeze:1, TimerVirtualization:1, CheckStackExtents:1, SpareFlags0:2, ActiveGroupsMask:20;
			int32	ReservedFlags:4;
		};
		int32	ProcessFlags;
	};
	char	BasePriority;
	char	QuantumReset;
	uint8	Visited;
	_KEXECUTE_OPTIONS	Flags;
	uint32	ThreadSeed[20];
	uint16	IdealNode[20];
	uint16	IdealGlobalNode;
	uint16	Spare1;
	uint16	IopmOffset;
	pointer32<_KSCHEDULING_GROUP>	SchedulingGroup;
	_KSTACK_COUNT	StackCount;
	_LIST_ENTRY	ProcessListEntry;
	uint64	CycleTime;
	uint64	ContextSwitches;
	uint32	FreezeCount;
	uint32	KernelTime;
	uint32	UserTime;
	pointer32<void>	VdmTrapcHandler;
};
struct _KAPC_STATE {
	_LIST_ENTRY	ApcListHead[2];
	pointer32<_KPROCESS>	Process;
	union {
		uint8	InProgressFlags;
		struct {
			uint8	KernelApcInProgress:1, SpecialApcInProgress:1;
		};
	};
	uint8	KernelApcPending;
	uint8	UserApcPending;
};
struct _KTIMER {
	_DISPATCHER_HEADER	Header;
	_ULARGE_INTEGER	DueTime;
	_LIST_ENTRY	TimerListEntry;
	pointer32<_KDPC>	Dpc;
	uint32	Period;
};
struct _KTHREAD;
struct _KQUEUE {
	_DISPATCHER_HEADER	Header;
	_LIST_ENTRY	EntryListHead;
	uint32	CurrentCount;
	uint32	MaximumCount;
	_LIST_ENTRY	ThreadListHead;
};
struct _KWAIT_BLOCK {
	_LIST_ENTRY	WaitListEntry;
	uint8	WaitType;
	uint8	BlockState;
	uint16	WaitKey;
	union {
		pointer32<_KTHREAD>	Thread;
		pointer32<_KQUEUE>	NotificationQueue;
	};
	pointer32<void>	Object;
	pointer32<void>	SparePtr;
};
struct _GROUP_AFFINITY {
	uint32	Mask;
	uint16	Group;
	uint16	Reserved[3];
};
struct _KAPC {
	uint8	Type;
	uint8	SpareByte0;
	uint8	Size;
	uint8	SpareByte1;
	uint32	SpareLong0;
	pointer32<_KTHREAD>	Thread;
	_LIST_ENTRY	ApcListEntry;
	union {
		struct {
			void	(*KernelRoutine)(pointer32<_KAPC>, void (**)(pointer32<void>, pointer32<void>, pointer32<void>), pointer32<void>*, pointer32<void>*, pointer32<void>*);
			void	(*RundownRoutine)(pointer32<_KAPC>);
			void	(*NormalRoutine)();
		};
		pointer32<void>	Reserved[3];
	};
	pointer32<void>	NormalContext;
	pointer32<void>	SystemArgument1;
	pointer32<void>	SystemArgument2;
	char	ApcStateIndex;
	char	ApcMode;
	uint8	Inserted;
};
struct _EX_PUSH_LOCK {
	union {
		struct {
			uint32	Locked:1, Waiting:1, Waking:1, MultipleShared:1, Shared:28;
		};
		uint32	Value;
		pointer32<void>	Ptr;
	};
};
struct _EX_RUNDOWN_REF {
	union {
		uint32	Count;
		pointer32<void>	Ptr;
	};
};
struct _EX_FAST_REF {
	union {
		pointer32<void>	Object;
		uint32	RefCnt:3;
		uint32	Value;
	};
};
struct _RTL_AVL_TREE {
	pointer32<_RTL_BALANCED_NODE>	Root;
};
struct _OBJECT_NAME_INFORMATION {
	_UNICODE_STRING	Name;
};
struct _SE_AUDIT_PROCESS_CREATION_INFO {
	pointer32<_OBJECT_NAME_INFORMATION>	ImageFileName;
};
struct _MMSUPPORT_FLAGS {
	uint8	WorkingSetType:3, Reserved0:3, MaximumWorkingSetHard:1, MinimumWorkingSetHard:1;
	uint8	SessionMaster:1, TrimmerState:2, Reserved:1, PageStealers:4;
	uint8	MemoryPriority;
	uint8	WsleDeleted:1, VmExiting:1, ExpansionFailed:1, SvmEnabled:1, ForceAge:1, NewMaximum:1, CommitReleaseState:2;
};
struct _KGATE {
	_DISPATCHER_HEADER	Header;
};
struct _MMWSL {
};
struct _MMSUPPORT {
	int32	WorkingSetLock;
	pointer32<_KGATE>	ExitOutswapGate;
	pointer32<void>	AccessLog;
	_LIST_ENTRY	WorkingSetExpansionLinks;
	uint32	AgeDistribution[7];
	uint32	MinimumWorkingSetSize;
	uint32	WorkingSetLeafSize;
	uint32	WorkingSetLeafPrivateSize;
	uint32	WorkingSetSize;
	uint32	WorkingSetPrivateSize;
	uint32	MaximumWorkingSetSize;
	uint32	ChargedWslePages;
	uint32	ActualWslePages;
	uint32	WorkingSetSizeOverhead;
	uint32	PeakWorkingSetSize;
	uint32	HardFaultCount;
	pointer32<_MMWSL>	VmWorkingSetList;
	uint16	NextPageColor;
	uint16	LastTrimStamp;
	uint32	PageFaultCount;
	uint32	TrimmedPageCount;
	uint32	Reserved0;
	_MMSUPPORT_FLAGS	Flags;
	uint32	ReleasedCommitDebt;
	pointer32<void>	WsSwapSupport;
	pointer32<void>	CommitReAcquireFailSupport;
};
struct _ALPC_PROCESS_CONTEXT {
	_EX_PUSH_LOCK	Lock;
	_LIST_ENTRY	ViewListHead;
	uint32	PagedPoolQuotaCache;
};
struct _PS_PROTECTION {
	union {
		uint8	Level;
		struct {
			uint8	Type:3, Audit:1, Signer:4;
		};
	};
};
struct _KEVENT {
	_DISPATCHER_HEADER	Header;
};
struct _KLOCK_ENTRY_LOCK_STATE {
	union {
		struct {
			uint32	CrossThreadReleasable:1, Busy:1, Reserved:29, InTree:1;
		};
		pointer32<void>	LockState;
	};
	union {
		pointer32<void>	SessionState;
		uint32	SessionId;
	};
};
struct _KLOCK_ENTRY {
	union {
		_RTL_BALANCED_NODE	TreeNode;
		_SINGLE_LIST_ENTRY	FreeListEntry;
	};
	union {
		uint32	EntryFlags;
		struct {
			uint8	EntryOffset;
			union {
				uint8	ThreadLocalFlags;
				struct {
					uint8	WaitingBit:1, Spare0:7;
				};
			};
			union {
				uint8	AcquiredByte;
				uint8	AcquiredBit:1;
			};
			union {
				uint8	CrossThreadFlags;
				struct {
					uint8	HeadNodeBit:1, IoPriorityBit:1, Spare1:6;
				};
			};
		};
		struct {
			uint32	StaticState:8, AllFlags:24;
		};
	};
	union {
		_KLOCK_ENTRY_LOCK_STATE	LockState;
		pointer32<void>	LockUnsafe;
		struct {
			uint8	CrossThreadReleasableAndBusyByte;
			uint8	Reserved[2];
			uint8	InTreeByte;
			union {
				pointer32<void>	SessionState;
				uint32	SessionId;
			};
		};
	};
	union {
		struct {
			_RTL_RB_TREE	OwnerTree;
			_RTL_RB_TREE	WaiterTree;
		};
		char	CpuPriorityKey;
	};
	uint32	EntryLock;
	union {
		uint16	AllBoosts;
		struct {
			uint16	IoBoost:1, CpuBoostsBitmap:15;
		};
	};
	uint16	IoNormalPriorityWaiterCount;
};
struct _M128A {
	uint64	Low;
	int64	High;
};
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
	_M128A	XmmRegisters[8];
	uint8	Reserved4[224];
};
enum _EXCEPTION_DISPOSITION {
	ExceptionContinueExecution	= 0,
	ExceptionContinueSearch	= 1,
	ExceptionNestedException	= 2,
	ExceptionCollidedUnwind	= 3,
};
struct _EXCEPTION_RECORD {
	int32	ExceptionCode;
	uint32	ExceptionFlags;
	pointer32<_EXCEPTION_RECORD>	ExceptionRecord;
	pointer32<void>	ExceptionAddress;
	uint32	NumberParameters;
	uint32	ExceptionInformation[15];
};
struct _FLOATING_SAVE_AREA {
	uint32	ControlWord;
	uint32	StatusWord;
	uint32	TagWord;
	uint32	ErrorOffset;
	uint32	ErrorSelector;
	uint32	DataOffset;
	uint32	DataSelector;
	uint8	RegisterArea[80];
	uint32	Spare0;
};
struct _CONTEXT {
	uint32	ContextFlags;
	uint32	Dr0;
	uint32	Dr1;
	uint32	Dr2;
	uint32	Dr3;
	uint32	Dr6;
	uint32	Dr7;
	_FLOATING_SAVE_AREA	FloatSave;
	uint32	SegGs;
	uint32	SegFs;
	uint32	SegEs;
	uint32	SegDs;
	uint32	Edi;
	uint32	Esi;
	uint32	Ebx;
	uint32	Edx;
	uint32	Ecx;
	uint32	Eax;
	uint32	Ebp;
	uint32	Eip;
	uint32	SegCs;
	uint32	EFlags;
	uint32	Esp;
	uint32	SegSs;
	uint8	ExtendedRegisters[512];
};
struct _EXCEPTION_REGISTRATION_RECORD {
	pointer32<_EXCEPTION_REGISTRATION_RECORD>	Next;
	_EXCEPTION_DISPOSITION	(*Handler)(pointer32<_EXCEPTION_RECORD>, pointer32<void>, pointer32<_CONTEXT>, pointer32<void>);
};
struct _KTRAP_FRAME {
	uint32	DbgEbp;
	uint32	DbgEip;
	uint32	DbgArgMark;
	uint16	TempSegCs;
	uint8	Logging;
	uint8	FrameType;
	uint32	TempEsp;
	uint32	Dr0;
	uint32	Dr1;
	uint32	Dr2;
	uint32	Dr3;
	uint32	Dr6;
	uint32	Dr7;
	uint32	SegGs;
	uint32	SegEs;
	uint32	SegDs;
	uint32	Edx;
	uint32	Ecx;
	uint32	Eax;
	uint8	PreviousPreviousMode;
	uint8	EntropyQueueDpc;
	uint8	Reserved[2];
	uint32	MxCsr;
	pointer32<_EXCEPTION_REGISTRATION_RECORD>	ExceptionList;
	uint32	SegFs;
	uint32	Edi;
	uint32	Esi;
	uint32	Ebx;
	uint32	Ebp;
	uint32	ErrCode;
	uint32	Eip;
	uint32	SegCs;
	uint32	EFlags;
	uint32	HardwareEsp;
	uint32	HardwareSegSs;
	uint32	V86Es;
	uint32	V86Ds;
	uint32	V86Fs;
	uint32	V86Gs;
};
enum _HARDWARE_COUNTER_TYPE {
	PMCCounter	= 0,
	MaxHardwareCounterType	= 1,
};
struct _COUNTER_READING {
	_HARDWARE_COUNTER_TYPE	Type;
	uint32	Index;
	uint64	Start;
	uint64	Total;
};
struct _PROCESSOR_NUMBER {
	uint16	Group;
	uint8	Number;
	uint8	Reserved;
};
struct _THREAD_PERFORMANCE_DATA {
	uint16	Size;
	uint16	Version;
	_PROCESSOR_NUMBER	ProcessorNumber;
	uint32	ContextSwitches;
	uint32	HwCountersCount;
	uint64	UpdateCount;
	uint64	WaitReasonBitMap;
	uint64	HardwareCounters;
	_COUNTER_READING	CycleTime;
	_COUNTER_READING	HwCounters[16];
};
struct _KTHREAD_COUNTERS {
	uint64	WaitReasonBitMap;
	pointer32<_THREAD_PERFORMANCE_DATA>	UserData;
	uint32	Flags;
	uint32	ContextSwitches;
	uint64	CycleTimeBias;
	uint64	HardwareCounters;
	_COUNTER_READING	HwCounter[16];
};
struct _XSAVE_AREA_HEADER {
	uint64	Mask;
	uint64	CompactionMask;
	uint64	Reserved2[6];
};
struct _XSAVE_AREA {
	_XSAVE_FORMAT	LegacyState;
	_XSAVE_AREA_HEADER	Header;
};
struct _XSTATE_CONTEXT {
	uint64	Mask;
	uint32	Length;
	uint32	Reserved1;
	pointer32<_XSAVE_AREA>	Area;
	uint32	Reserved2;
	pointer32<void>	Buffer;
	uint32	Reserved3;
};
struct _XSTATE_SAVE;
struct _DESCRIPTOR {
	uint16	Pad;
	uint16	Limit;
	uint32	Base;
};
struct _KSPECIAL_REGISTERS {
	uint32	Cr0;
	uint32	Cr2;
	uint32	Cr3;
	uint32	Cr4;
	uint32	KernelDr0;
	uint32	KernelDr1;
	uint32	KernelDr2;
	uint32	KernelDr3;
	uint32	KernelDr6;
	uint32	KernelDr7;
	_DESCRIPTOR	Gdtr;
	_DESCRIPTOR	Idtr;
	uint16	Tr;
	uint16	Ldtr;
	uint64	Xcr0;
	uint32	ExceptionList;
	uint32	Reserved[3];
};
struct _KPROCESSOR_STATE {
	_CONTEXT	ContextFrame;
	_KSPECIAL_REGISTERS	SpecialRegisters;
};
struct _KSPIN_LOCK_QUEUE {
	pointer32<_KSPIN_LOCK_QUEUE>	Next;
	pointer32<uint32>	Lock;
};
union _SLIST_HEADER {
	uint64	Alignment;
	struct {
		_SINGLE_LIST_ENTRY	Next;
		uint16	Depth;
		uint16	CpuId;
	};
};
enum _POOL_TYPE {
	NonPagedPool	= 0,
	NonPagedPoolExecute	= 0,
	PagedPool	= 1,
	NonPagedPoolMustSucceed	= 2,
	DontUseThisType	= 3,
	NonPagedPoolCacheAligned	= 4,
	PagedPoolCacheAligned	= 5,
	NonPagedPoolCacheAlignedMustS	= 6,
	MaxPoolType	= 7,
	NonPagedPoolBase	= 0,
	NonPagedPoolBaseMustSucceed	= 2,
	NonPagedPoolBaseCacheAligned	= 4,
	NonPagedPoolBaseCacheAlignedMustS	= 6,
	NonPagedPoolSession	= 32,
	PagedPoolSession	= 33,
	NonPagedPoolMustSucceedSession	= 34,
	DontUseThisTypeSession	= 35,
	NonPagedPoolCacheAlignedSession	= 36,
	PagedPoolCacheAlignedSession	= 37,
	NonPagedPoolCacheAlignedMustSSession	= 38,
	NonPagedPoolNx	= 512,
	NonPagedPoolNxCacheAligned	= 516,
	NonPagedPoolSessionNx	= 544,
};
struct _LOOKASIDE_LIST_EX;
struct _GENERAL_LOOKASIDE;
struct _KPRCB;
struct _KTHREAD {
	_DISPATCHER_HEADER	Header;
	pointer32<void>	SListFaultAddress;
	uint64	QuantumTarget;
	pointer32<void>	InitialStack;
	pointer32<void>	StackLimit;
	pointer32<void>	StackBase;
	uint32	ThreadLock;
	uint64	CycleTime;
	uint32	HighCycleTime;
	pointer32<void>	ServiceTable;
	uint32	CurrentRunTime;
	uint32	ExpectedRunTime;
	pointer32<void>	KernelStack;
	pointer32<_XSAVE_FORMAT>	StateSaveArea;
	pointer32<_KSCHEDULING_GROUP>	SchedulingGroup;
	_KWAIT_STATUS_REGISTER	WaitRegister;
	uint8	Running;
	uint8	Alerted[2];
	union {
		struct {
			uint32	AutoBoostActive:1, ReadyTransition:1, WaitNext:1, SystemAffinityActive:1, Alertable:1, UserStackWalkActive:1, ApcInterruptRequest:1, QuantumEndMigrate:1, UmsDirectedSwitchEnable:1, TimerActive:1, SystemThread:1, ProcessDetachActive:1, CalloutActive:1, ScbReadyQueue:1, ApcQueueable:1, ReservedStackInUse:1, UmsPerformingSyscall:1, TimerSuspended:1, SuspendedWaitMode:1, SuspendSchedulerApcWait:1, Reserved:12;
		};
		int32	MiscFlags;
	};
	union {
		struct {
			uint32	AutoAlignment:1, DisableBoost:1, ThreadFlagsSpare0:1, AlertedByThreadId:1, QuantumDonation:1, EnableStackSwap:1, GuiThread:1, DisableQuantum:1, ChargeOnlySchedulingGroup:1, DeferPreemption:1, QueueDeferPreemption:1, ForceDeferSchedule:1, SharedReadyQueueAffinity:1, FreezeCount:1, TerminationApcRequest:1, AutoBoostEntriesExhausted:1, KernelStackResident:1, CommitFailTerminateRequest:1, ProcessStackCountDecremented:1, ThreadFlagsSpare:5, EtwStackTraceApcInserted:8;
		};
		int32	ThreadFlags;
	};
	uint8	Tag;
	uint8	SystemHeteroCpuPolicy;
	uint8	UserHeteroCpuPolicy:7, ExplicitSystemHeteroCpuPolicy:1;
	uint8	Spare0;
	uint32	SystemCallNumber;
	pointer32<void>	FirstArgument;
	pointer32<_KTRAP_FRAME>	TrapFrame;
	union {
		_KAPC_STATE	ApcState;
		struct {
			uint8	ApcStateFill[23];
			char	Priority;
		};
	};
	uint32	UserIdealProcessor;
	uint32	ContextSwitches;
	uint8	State;
	char	Spare12;
	uint8	WaitIrql;
	char	WaitMode;
	int32	WaitStatus;
	pointer32<_KWAIT_BLOCK>	WaitBlockList;
	union {
		_LIST_ENTRY	WaitListEntry;
		_SINGLE_LIST_ENTRY	SwapListEntry;
	};
	pointer32<_DISPATCHER_HEADER>	Queue;
	pointer32<void>	Teb;
	uint64	RelativeTimerBias;
	_KTIMER	Timer;
	union {
		_KWAIT_BLOCK	WaitBlock[4];
		struct {
			uint8	WaitBlockFill8[20];
			pointer32<_KTHREAD_COUNTERS>	ThreadCounters;
		};
		struct {
			uint8	WaitBlockFill9[44];
			pointer32<_XSTATE_SAVE>	XStateSave;
		};
		struct {
			uint8	WaitBlockFill10[68];
			pointer32<void>	Win32Thread;
		};
		struct {
			uint8	WaitBlockFill11[88];
			uint32	WaitTime;
			union {
				struct {
					int16	KernelApcDisable;
					int16	SpecialApcDisable;
				};
				uint32	CombinedApcDisable;
			};
		};
	};
	_LIST_ENTRY	QueueListEntry;
	union {
		uint32	NextProcessor;
		struct {
			uint32	NextProcessorNumber:31, SharedReadyQueue:1;
		};
	};
	int32	QueuePriority;
	pointer32<_KPROCESS>	Process;
	union {
		_GROUP_AFFINITY	UserAffinity;
		struct {
			uint8	UserAffinityFill[6];
			char	PreviousMode;
			char	BasePriority;
			union {
				char	PriorityDecrement;
				struct {
					uint8	ForegroundBoost:4, UnusualBoost:4;
				};
			};
			uint8	Preempted;
			uint8	AdjustReason;
			char	AdjustIncrement;
		};
	};
	uint32	AffinityVersion;
	union {
		_GROUP_AFFINITY	Affinity;
		struct {
			uint8	AffinityFill[6];
			uint8	ApcStateIndex;
			uint8	WaitBlockCount;
			uint32	IdealProcessor;
		};
	};
	uint32	Spare15[1];
	union {
		_KAPC_STATE	SavedApcState;
		struct {
			uint8	SavedApcStateFill[23];
			uint8	WaitReason;
		};
	};
	char	SuspendCount;
	char	Saturation;
	uint16	SListFaultCount;
	union {
		_KAPC	SchedulerApc;
		struct {
			uint8	SchedulerApcFill0[1];
			uint8	ResourceIndex;
		};
		struct {
			uint8	SchedulerApcFill1[3];
			uint8	QuantumReset;
		};
		struct {
			uint8	SchedulerApcFill2[4];
			uint32	KernelTime;
		};
		struct {
			uint8	SchedulerApcFill3[36];
			pointer32<_KPRCB>	WaitPrcb;
		};
		struct {
			uint8	SchedulerApcFill4[40];
			pointer32<void>	LegoData;
		};
		struct {
			uint8	SchedulerApcFill5[47];
			uint8	CallbackNestingLevel;
		};
	};
	uint32	UserTime;
	_KEVENT	SuspendEvent;
	_LIST_ENTRY	ThreadListEntry;
	_LIST_ENTRY	MutantListHead;
	uint8	AbEntrySummary;
	uint8	AbWaitEntryCount;
	uint16	Spare20;
	_KLOCK_ENTRY	LockEntries[6];
	_SINGLE_LIST_ENTRY	PropagateBoostsEntry;
	_SINGLE_LIST_ENTRY	IoSelfBoostsEntry;
	uint8	PriorityFloorCounts[16];
	uint32	PriorityFloorSummary;
	int32	AbCompletedIoBoostCount;
	int16	KeReferenceCount;
	uint8	AbOrphanedEntrySummary;
	uint8	AbOwnedEntryCount;
	uint32	ForegroundLossTime;
	union {
		_LIST_ENTRY	GlobalForegroundListEntry;
		struct {
			_SINGLE_LIST_ENTRY	ForegroundDpcStackListEntry;
			uint32	InGlobalForegroundList;
		};
	};
	pointer32<_KSCB>	QueuedScb;
	uint64	NpxState;
};
struct _CLIENT_ID {
	pointer32<void>	UniqueProcess;
	pointer32<void>	UniqueThread;
};
struct _KSEMAPHORE {
	_DISPATCHER_HEADER	Header;
	int32	Limit;
};
union _PS_CLIENT_SECURITY_CONTEXT {
	uint32	ImpersonationData;
	pointer32<void>	ImpersonationToken;
	struct {
		uint32	ImpersonationLevel:2, EffectiveOnly:1;
	};
};
struct _PS_PROPERTY_SET {
	_LIST_ENTRY	ListHead;
	uint32	Lock;
};
struct _TERMINATION_PORT {
	pointer32<_TERMINATION_PORT>	Next;
	pointer32<void>	Port;
};
struct _ETHREAD;
struct _EPROCESS;
struct _MDL {
	pointer32<_MDL>	Next;
	int16	Size;
	int16	MdlFlags;
	pointer32<_EPROCESS>	Process;
	pointer32<void>	MappedSystemVa;
	pointer32<void>	StartVa;
	uint32	ByteCount;
	uint32	ByteOffset;
};
struct _IRP;
struct _WAIT_CONTEXT_BLOCK {
	union {
		_KDEVICE_QUEUE_ENTRY	WaitQueueEntry;
		struct {
			_LIST_ENTRY	DmaWaitEntry;
			uint32	NumberOfChannels;
			uint32	SyncCallback:1, DmaContext:1, Reserved:30;
		};
	};
	_IO_ALLOCATION_ACTION	(*DeviceRoutine)(pointer32<_DEVICE_OBJECT>, pointer32<_IRP>, pointer32<void>, pointer32<void>);
	pointer32<void>	DeviceContext;
	uint32	NumberOfMapRegisters;
	pointer32<void>	DeviceObject;
	pointer32<void>	CurrentIrp;
	pointer32<_KDPC>	BufferChainingDpc;
};
struct _KDEVICE_QUEUE {
	int16	Type;
	int16	Size;
	_LIST_ENTRY	DeviceListHead;
	uint32	Lock;
	uint8	Busy;
};
struct _DRIVER_OBJECT;
struct LIST_ENTRY32 {
	uint32	Flink;
	uint32	Blink;
};
struct _NT_TIB {
	pointer32<_EXCEPTION_REGISTRATION_RECORD>	ExceptionList;
	pointer32<void>	StackBase;
	pointer32<void>	StackLimit;
	pointer32<void>	SubSystemTib;
	union {
		pointer32<void>	FiberData;
		uint32	Version;
	};
	pointer32<void>	ArbitraryUserPointer;
	pointer32<_NT_TIB>	Self;
};
struct _flags {
	uint8	Removable:1, GroupAssigned:1, GroupCommitted:1, GroupAssignmentFixed:1, Fill:4;
};
struct _KHETERO_PROCESSOR_SET {
	uint32	PreferredMask;
	uint32	AvailableMask;
};
struct _KNODE {
	uint32	IdleNonParkedCpuSet;
	uint32	IdleSmtSet;
	uint32	IdleCpuSet;
	char	_pdb_padding0[52];
	uint32	DeepIdleSet;
	uint32	IdleConstrainedSet;
	uint32	NonParkedSet;
	int32	ParkLock;
	uint32	Seed;
	char	_pdb_padding1[44];
	uint32	SiblingMask;
	union {
		_GROUP_AFFINITY	Affinity;
		struct {
			uint8	AffinityFill[6];
			uint16	NodeNumber;
			uint16	PrimaryNodeNumber;
			uint8	Stride;
			uint8	Spare0;
		};
	};
	uint32	SharedReadyQueueLeaders;
	uint32	ProximityId;
	uint32	Lowest;
	uint32	Highest;
	uint8	MaximumProcessors;
	_flags	Flags;
	uint8	Spare10;
	_KHETERO_PROCESSOR_SET	HeteroSets[5];
};
struct _KPRIQUEUE {
	_DISPATCHER_HEADER	Header;
	_LIST_ENTRY	EntryListHead[32];
	int32	CurrentCount[32];
	uint32	MaximumCount;
	_LIST_ENTRY	ThreadListHead;
};
enum _EXQUEUEINDEX {
	ExPoolUntrusted	= 0,
	ExPoolTrusted	= 1,
	ExPoolMax	= 8,
};
struct _ENODE;
struct _EX_WORK_QUEUE {
	_KPRIQUEUE	WorkPriQueue;
	pointer32<_ENODE>	Node;
	uint32	WorkItemsProcessed;
	uint32	WorkItemsProcessedLastPass;
	int32	ThreadCount;
	int32	MinThreads:31;
	uint32	TryFailed:1;
	int32	MaxThreads;
	_EXQUEUEINDEX	QueueIndex;
};
enum _MM_PAGE_ACCESS_TYPE {
	MmPteAccessType	= 0,
	MmCcReadAheadType	= 1,
	MmPfnRepurposeType	= 2,
	MmMaximumPageAccessType	= 3,
};
union _MM_PAGE_ACCESS_INFO_FLAGS {
	struct {
		uint32	FilePointerIndex:9, HardFault:1, Image:1, Spare0:1;
	}	File;
	struct {
		uint32	FilePointerIndex:9, HardFault:1, Spare1:2;
	}	Private;
};
struct _MM_PAGE_ACCESS_INFO {
	union {
		_MM_PAGE_ACCESS_INFO_FLAGS	Flags;
		uint64	FileOffset;
		pointer32<void>	VirtualAddress;
		struct {
			uint32	DontUse0:3, Spare0:29;
			pointer32<void>	PointerProtoPte;
		};
	};
};
enum _WHEA_ERROR_SEVERITY {
	WheaErrSevRecoverable	= 0,
	WheaErrSevFatal	= 1,
	WheaErrSevCorrected	= 2,
	WheaErrSevInformational	= 3,
};
struct _KREQUEST_PACKET {
	pointer32<void>	CurrentPacket[3];
	void	(*WorkerRoutine)(pointer32<void>, pointer32<void>, pointer32<void>, pointer32<void>);
};
struct _REQUEST_MAILBOX {
	pointer32<_REQUEST_MAILBOX>	Next;
	uint32	RequestSummary;
	_KREQUEST_PACKET	RequestPacket;
	pointer32<int32>	NodeTargetCountAddr;
	int32	NodeTargetCount;
};
struct _KSHARED_READY_QUEUE {
	uint32	Lock;
	uint32	ReadySummary;
	_LIST_ENTRY	ReadyListHead[32];
	char	RunningSummary[32];
	uint8	Span;
	uint8	LowProcIndex;
	uint8	QueueIndex;
	uint8	ProcCount;
	uint8	ScanOwner;
	uint8	Spare[3];
	uint32	Affinity;
};
struct _ASSEMBLY_STORAGE_MAP {
};
union _HEAP_BUCKET_RUN_INFO {
	struct {
		uint32	Bucket;
		uint32	RunLength;
	};
	int64	Aggregate64;
};
enum BUS_QUERY_ID_TYPE {
	BusQueryDeviceID	= 0,
	BusQueryHardwareIDs	= 1,
	BusQueryCompatibleIDs	= 2,
	BusQueryInstanceID	= 3,
	BusQueryDeviceSerialNumber	= 4,
	BusQueryContainerID	= 5,
};
enum DEVICE_TEXT_TYPE {
	DeviceTextDescription	= 0,
	DeviceTextLocationInformation	= 1,
};
struct _OBJECT_HANDLE_INFORMATION {
	uint32	HandleAttributes;
	uint32	GrantedAccess;
};
struct _PPM_FFH_THROTTLE_STATE_INFO {
	uint8	EnableLogging;
	uint32	MismatchCount;
	uint8	Initialized;
	uint64	LastValue;
	_LARGE_INTEGER	LastLogTickCount;
};
struct _DEVICE_OBJECT_POWER_EXTENSION {
};
struct _PPM_COORDINATED_SELECTION {
	uint32	MaximumStates;
	uint32	SelectedStates;
	uint32	DefaultSelection;
	pointer32<uint32>	Selection;
};
struct _IMAGE_FILE_HEADER {
	uint16	Machine;
	uint16	NumberOfSections;
	uint32	TimeDateStamp;
	uint32	PointerToSymbolTable;
	uint32	NumberOfSymbols;
	uint16	SizeOfOptionalHeader;
	uint16	Characteristics;
};
struct _IMAGE_DATA_DIRECTORY {
	uint32	VirtualAddress;
	uint32	Size;
};
struct _IMAGE_OPTIONAL_HEADER {
	uint16	Magic;
	uint8	MajorLinkerVersion;
	uint8	MinorLinkerVersion;
	uint32	SizeOfCode;
	uint32	SizeOfInitializedData;
	uint32	SizeOfUninitializedData;
	uint32	AddressOfEntryPoint;
	uint32	BaseOfCode;
	uint32	BaseOfData;
	uint32	ImageBase;
	uint32	SectionAlignment;
	uint32	FileAlignment;
	uint16	MajorOperatingSystemVersion;
	uint16	MinorOperatingSystemVersion;
	uint16	MajorImageVersion;
	uint16	MinorImageVersion;
	uint16	MajorSubsystemVersion;
	uint16	MinorSubsystemVersion;
	uint32	Win32VersionValue;
	uint32	SizeOfImage;
	uint32	SizeOfHeaders;
	uint32	CheckSum;
	uint16	Subsystem;
	uint16	DllCharacteristics;
	uint32	SizeOfStackReserve;
	uint32	SizeOfStackCommit;
	uint32	SizeOfHeapReserve;
	uint32	SizeOfHeapCommit;
	uint32	LoaderFlags;
	uint32	NumberOfRvaAndSizes;
	_IMAGE_DATA_DIRECTORY	DataDirectory[16];
};
struct _IMAGE_NT_HEADERS {
	uint32	Signature;
	_IMAGE_FILE_HEADER	FileHeader;
	_IMAGE_OPTIONAL_HEADER	OptionalHeader;
};
struct _PROC_IDLE_POLICY {
	uint8	PromotePercent;
	uint8	DemotePercent;
	uint8	PromotePercentBase;
	uint8	DemotePercentBase;
	uint8	AllowScaling;
	uint8	ForceLightIdle;
};
union _PPM_IDLE_SYNCHRONIZATION_STATE {
	int32	AsLong;
	struct {
		int32	RefCount:24;
		uint32	State:8;
	};
};
struct _PROC_FEEDBACK_COUNTER {
	union {
		void	(*InstantaneousRead)(uint32, pointer32<uint32>);
		void	(*DifferentialRead)(uint32, uint8, pointer32<uint64>, pointer32<uint64>);
	};
	uint64	LastActualCount;
	uint64	LastReferenceCount;
	uint32	CachedValue;
	char	_pdb_padding0[4];
	uint8	Affinitized;
	uint8	Differential;
	uint8	Scaling;
	uint32	Context;
};
struct _PROC_FEEDBACK {
	uint32	Lock;
	uint64	CyclesLast;
	uint64	CyclesActive;
	pointer32<_PROC_FEEDBACK_COUNTER>	Counters[2];
	uint64	LastUpdateTime;
	uint64	UnscaledTime;
	int64	UnaccountedTime;
	uint64	ScaledTime[2];
	uint64	UnaccountedKernelTime;
	uint64	PerformanceScaledKernelTime;
	uint32	UserTimeLast;
	uint32	KernelTimeLast;
	uint64	IdleGenerationNumberLast;
	uint64	HvActiveTimeLast;
	uint64	StallCyclesLast;
	uint64	StallTime;
	uint8	KernelTimesIndex;
};
enum _PROC_HYPERVISOR_STATE {
	ProcHypervisorNone	= 0,
	ProcHypervisorPresent	= 1,
	ProcHypervisorPower	= 2,
	ProcHypervisorHvCounters	= 3,
};
struct _PROC_IDLE_SNAP {
	uint64	Time;
	uint64	Idle;
};
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
};
struct _PROCESSOR_IDLE_DEPENDENCY {
	uint32	ProcessorIndex;
	uint8	ExpectedState;
	uint8	AllowDeeperStates;
	uint8	LooseDependency;
};
struct _PROCESSOR_IDLE_PREPARE_INFO {
	pointer32<void>	Context;
	_PROCESSOR_IDLE_CONSTRAINTS	Constraints;
	uint32	DependencyCount;
	uint32	DependencyUsed;
	pointer32<_PROCESSOR_IDLE_DEPENDENCY>	DependencyArray;
	uint32	PlatformIdleStateIndex;
	uint32	ProcessorIdleStateIndex;
	uint32	IdleSelectFailureMask;
};
struct _PPM_SELECTION_DEPENDENCY;
struct _PPM_SELECTION_MENU_ENTRY {
	uint8	StrictDependency;
	uint8	InitiatingState;
	uint8	DependentState;
	uint32	StateIndex;
	uint32	Dependencies;
	pointer32<_PPM_SELECTION_DEPENDENCY>	DependencyList;
};
struct _PPM_SELECTION_MENU {
	uint32	Count;
	pointer32<_PPM_SELECTION_MENU_ENTRY>	Entries;
};
struct _PPM_VETO_ENTRY {
	_LIST_ENTRY	Link;
	uint32	VetoReason;
	uint32	ReferenceCount;
	uint64	HitCount;
	uint64	LastActivationTime;
	uint64	TotalActiveTime;
	uint64	CsActivationTime;
	uint64	CsActiveTime;
};
struct _PPM_VETO_ACCOUNTING {
	int32	VetoPresent;
	_LIST_ENTRY	VetoListHead;
	uint8	CsAccountingBlocks;
	uint8	BlocksDrips;
	uint32	PreallocatedVetoCount;
	pointer32<_PPM_VETO_ENTRY>	PreallocatedVetoList;
};
struct _PPM_IDLE_STATE {
	_KAFFINITY_EX	DomainMembers;
	_UNICODE_STRING	Name;
	uint32	Latency;
	uint32	BreakEvenDuration;
	uint32	Power;
	uint32	StateFlags;
	_PPM_VETO_ACCOUNTING	VetoAccounting;
	uint8	StateType;
	uint8	InterruptsEnabled;
	uint8	Interruptible;
	uint8	ContextRetained;
	uint8	CacheCoherent;
	uint8	WakesSpuriously;
	uint8	PlatformOnly;
	uint8	NoCState;
};
struct _PERFINFO_PPM_STATE_SELECTION {
	uint32	SelectedState;
	uint32	VetoedStates;
	uint32	VetoReason[1];
};
struct _PPM_IDLE_STATES {
	uint8	InterfaceVersion;
	uint8	ForceIdle;
	uint8	EstimateIdleDuration;
	uint8	ExitLatencyTraceEnabled;
	uint8	NonInterruptibleTransition;
	uint8	UnaccountedTransition;
	uint8	IdleDurationLimited;
	uint32	ExitLatencyCountdown;
	uint32	TargetState;
	uint32	ActualState;
	uint32	OldState;
	uint32	OverrideIndex;
	uint32	ProcessorIdleCount;
	uint32	Type;
	uint16	ReasonFlags;
	uint64	InitiateWakeStamp;
	int32	PreviousStatus;
	uint32	PreviousCancelReason;
	_KAFFINITY_EX	PrimaryProcessorMask;
	_KAFFINITY_EX	SecondaryProcessorMask;
	void	(*IdlePrepare)();
	int32	(*IdlePreExecute)();
	int32	(*IdleExecute)();
	uint32	(*IdlePreselect)();
	uint32	(*IdleTest)();
	uint32	(*IdleAvailabilityCheck)();
	void	(*IdleComplete)();
	void	(*IdleCancel)();
	uint8	(*IdleIsHalted)();
	uint8	(*IdleInitiateWake)();
	_PROCESSOR_IDLE_PREPARE_INFO	PrepareInfo;
	_KAFFINITY_EX	DeepIdleSnapshot;
	pointer32<_PERFINFO_PPM_STATE_SELECTION>	Tracing;
	pointer32<_PERFINFO_PPM_STATE_SELECTION>	CoordinatedTracing;
	_PPM_SELECTION_MENU	ProcessorMenu;
	_PPM_SELECTION_MENU	CoordinatedMenu;
	_PPM_COORDINATED_SELECTION	CoordinatedSelection;
	_PPM_IDLE_STATE	State[1];
};
enum PPM_IDLE_BUCKET_TIME_TYPE {
	PpmIdleBucketTimeInQpc	= 0,
	PpmIdleBucketTimeIn100ns	= 1,
	PpmIdleBucketTimeMaximum	= 2,
};
struct _PPM_SELECTION_STATISTICS {
	uint64	SelectedCount;
	uint64	VetoCount;
	uint64	PreVetoCount;
	uint64	WrongProcessorCount;
	uint64	LatencyCount;
	uint64	IdleDurationCount;
	uint64	DeviceDependencyCount;
	uint64	ProcessorDependencyCount;
	uint64	PlatformOnlyCount;
	uint64	InterruptibleCount;
	uint64	LegacyOverrideCount;
	uint64	CstateCheckCount;
	uint64	NoCStateCount;
	uint64	CoordinatedDependencyCount;
	pointer32<_PPM_VETO_ACCOUNTING>	PreVetoAccounting;
};
struct _PROC_IDLE_STATE_BUCKET {
	uint64	TotalTime;
	uint64	MinTime;
	uint64	MaxTime;
	uint32	Count;
};
struct _PROC_IDLE_STATE_ACCOUNTING {
	uint64	TotalTime;
	uint32	CancelCount;
	uint32	FailureCount;
	uint32	SuccessCount;
	uint32	InvalidBucketIndex;
	uint64	MinTime;
	uint64	MaxTime;
	_PPM_SELECTION_STATISTICS	SelectionStatistics;
	_PROC_IDLE_STATE_BUCKET	IdleTimeBuckets[26];
};
struct _PROC_IDLE_ACCOUNTING {
	uint32	StateCount;
	uint32	TotalTransitions;
	uint32	ResetCount;
	uint32	AbortCount;
	uint64	StartTime;
	uint64	PriorIdleTime;
	PPM_IDLE_BUCKET_TIME_TYPE	TimeUnit;
	_PROC_IDLE_STATE_ACCOUNTING	State[1];
};
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
};
struct _PROC_PERF_CHECK {
	uint64	LastActive;
	uint64	LastTime;
	uint64	LastStall;
	_PROC_PERF_CHECK_SNAP	Snap;
	_PROC_PERF_CHECK_SNAP	TempSnap;
	uint8	TaggedThreadPercent[2];
};
struct _PROC_PERF_DOMAIN;
struct _PROC_PERF_CONSTRAINT;
struct _PPM_CONCURRENCY_ACCOUNTING {
	uint32	Lock;
	uint32	Processors;
	uint32	ActiveProcessors;
	uint64	LastUpdateTime;
	uint64	TotalTime;
	uint64	AccumulatedTime[1];
};
struct _PROC_PERF_LOAD {
	uint8	BusyPercentage;
	uint8	FrequencyPercentage;
};
struct _PROC_PERF_HISTORY_ENTRY {
	uint16	Utility;
	uint16	AffinitizedUtility;
	uint8	Frequency;
	uint8	TaggedPercent[2];
};
struct _PROC_PERF_HISTORY {
	uint32	Count;
	uint32	Slot;
	uint32	UtilityTotal;
	uint32	AffinitizedUtilityTotal;
	uint32	FrequencyTotal;
	uint32	TaggedPercentTotal[2];
	_PROC_PERF_HISTORY_ENTRY	HistoryList[1];
};
struct _PROCESSOR_POWER_STATE {
	pointer32<_PPM_IDLE_STATES>	IdleStates;
	pointer32<_PROC_IDLE_ACCOUNTING>	IdleAccounting;
	uint64	IdleTimeLast;
	uint64	IdleTimeTotal;
	uint64	IdleTimeEntry;
	uint64	IdleTimeExpiration;
	uint8	NonInterruptibleTransition;
	uint8	PepWokenTransition;
	uint8	Class;
	uint32	TargetIdleState;
	_PROC_IDLE_POLICY	IdlePolicy;
	_PPM_IDLE_SYNCHRONIZATION_STATE	Synchronization;
	_PROC_FEEDBACK	PerfFeedback;
	_PROC_HYPERVISOR_STATE	Hypervisor;
	uint32	LastSysTime;
	uint32	WmiDispatchPtr;
	int32	WmiInterfaceEnabled;
	_PPM_FFH_THROTTLE_STATE_INFO	FFHThrottleStateInfo;
	_KDPC	PerfActionDpc;
	int32	PerfActionMask;
	_PROC_IDLE_SNAP	HvIdleCheck;
	pointer32<_PROC_PERF_CHECK>	PerfCheck;
	pointer32<_PROC_PERF_DOMAIN>	Domain;
	pointer32<_PROC_PERF_CONSTRAINT>	PerfConstraint;
	pointer32<_PPM_CONCURRENCY_ACCOUNTING>	Concurrency;
	pointer32<_PROC_PERF_LOAD>	Load;
	pointer32<_PROC_PERF_HISTORY>	PerfHistory;
	uint8	GuaranteedPerformancePercent;
	uint8	HvTargetState;
	uint8	Parked;
	uint32	LatestPerformancePercent;
	uint32	AveragePerformancePercent;
	uint32	LatestAffinitizedPercent;
	uint32	RelativePerformance;
	uint32	Utility;
	uint32	AffinitizedUtility;
	union {
		uint64	SnapTimeLast;
		uint64	EnergyConsumed;
	};
	uint64	ActiveTime;
	uint64	TotalTime;
};
struct _HANDLE_TRACE_DB_ENTRY {
	_CLIENT_ID	ClientId;
	pointer32<void>	Handle;
	uint32	Type;
	pointer32<void>	StackTrace[16];
};
struct _RTL_SRWLOCK {
	union {
		struct {
			uint32	Locked:1, Waiting:1, Waking:1, MultipleShared:1, Shared:28;
		};
		uint32	Value;
		pointer32<void>	Ptr;
	};
};
struct _HEAP_SUBALLOCATOR_CALLBACKS {
	uint32	Allocate;
	uint32	Free;
	uint32	Commit;
	uint32	Decommit;
	uint32	ExtendContext;
};
struct _HEAP_VS_CONTEXT {
	_RTL_SRWLOCK	Lock;
	_RTL_RB_TREE	FreeChunkTree;
	_LIST_ENTRY	SubsegmentList;
	uint32	TotalCommittedUnits;
	uint32	FreeCommittedUnits;
	pointer32<void>	BackendCtx;
	_HEAP_SUBALLOCATOR_CALLBACKS	Callbacks;
};
struct _OWNER_ENTRY {
	uint32	OwnerThread;
	union {
		struct {
			uint32	IoPriorityBoosted:1, OwnerReferenced:1, OwnerCount:30;
		};
		uint32	TableSize;
	};
};
struct _INTERFACE {
	uint16	Size;
	uint16	Version;
	pointer32<void>	Context;
	void	(*InterfaceReference)(pointer32<void>);
	void	(*InterfaceDereference)(pointer32<void>);
};
struct _HEAP_VS_UNUSED_BYTES_INFO {
	union {
		struct {
			uint16	UnusedBytes:13, LfhSubsegment:1, ExtraPresent:1, OneByteUnused:1;
		};
		uint8	Bytes[2];
	};
};
struct _USER_MEMORY_CACHE_ENTRY {
	_SLIST_HEADER	UserBlocks;
	uint32	AvailableBlocks;
	uint32	MinimumDepth;
	uint32	CacheShiftThreshold;
	uint16	Allocations;
	uint16	Frees;
	uint16	CacheHits;
};
struct _EXHANDLE {
	union {
		struct {
			uint32	TagBits:2, Index:30;
		};
		pointer32<void>	GenericHandleOverlay;
		uint32	Value;
	};
};
struct _HANDLE_TABLE_ENTRY_INFO {
	uint32	AuditMask;
};
union _HANDLE_TABLE_ENTRY {
	int32	VolatileLowValue;
	int32	LowValue;
	struct {
		pointer32<_HANDLE_TABLE_ENTRY_INFO>	InfoTable;
		union {
			int32	HighValue;
			pointer32<_HANDLE_TABLE_ENTRY>	NextFreeHandleEntry;
			_EXHANDLE	LeafHandleValue;
		};
	};
	struct {
		uint32	Unlocked:1, Attributes:2, ObjectPointerBits:29;
		union {
			int32	RefCountField;
			struct {
				uint32	GrantedAccessBits:25, ProtectFromClose:1, NoRightsUpgrade:1, RefCnt:5;
			};
		};
	};
};
struct _HANDLE_TABLE_FREE_LIST {
	_EX_PUSH_LOCK	FreeListLock;
	pointer32<_HANDLE_TABLE_ENTRY>	FirstFreeHandleEntry;
	pointer32<_HANDLE_TABLE_ENTRY>	LastFreeHandleEntry;
	int32	HandleCount;
	uint32	HighWaterMark;
	uint32	Reserved[8];
};
struct _HEAP_ENTRY_EXTRA {
	union {
		struct {
			uint16	AllocatorBackTraceIndex;
			uint16	TagIndex;
			uint32	Settable;
		};
		uint64	ZeroInit;
	};
};
struct _HEAP_UNPACKED_ENTRY {
	union {
		struct {
			uint16	Size;
			uint8	Flags;
			uint8	SmallTagIndex;
		};
		uint32	SubSegmentCode;
	};
	uint16	PreviousSize;
	union {
		uint8	SegmentOffset;
		uint8	LFHFlags;
	};
	uint8	UnusedBytes;
};
struct _HEAP_EXTENDED_ENTRY {
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
struct _HEAP_ENTRY {
	union {
		_HEAP_UNPACKED_ENTRY	UnpackedEntry;
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
		_HEAP_EXTENDED_ENTRY	ExtendedEntry;
		struct {
			uint16	FunctionIndex;
			uint16	ContextValue;
		};
		struct {
			uint32	InterceptorValue;
			uint16	UnusedBytesLength;
			uint8	EntryOffset;
			uint8	ExtendedBlockSignature;
		};
		struct {
			uint32	Code1;
			union {
				struct {
					uint16	Code2;
					uint8	Code3;
					uint8	Code4;
				};
				uint32	Code234;
			};
		};
		uint64	AgregateCode;
	};
};
struct _HEAP_VIRTUAL_ALLOC_ENTRY {
	_LIST_ENTRY	Entry;
	_HEAP_ENTRY_EXTRA	ExtraStuff;
	uint32	CommitSize;
	uint32	ReserveSize;
	_HEAP_ENTRY	BusyBlock;
};
enum _INTERFACE_TYPE {
	InterfaceTypeUndefined	= -1,
	Internal	= 0,
	Isa	= 1,
	Eisa	= 2,
	MicroChannel	= 3,
	TurboChannel	= 4,
	PCIBus	= 5,
	VMEBus	= 6,
	NuBus	= 7,
	PCMCIABus	= 8,
	CBus	= 9,
	MPIBus	= 10,
	MPSABus	= 11,
	ProcessorInternal	= 12,
	InternalPowerBus	= 13,
	PNPISABus	= 14,
	PNPBus	= 15,
	Vmcs	= 16,
	ACPIBus	= 17,
	MaximumInterfaceType	= 18,
};
enum MCA_EXCEPTION_TYPE {
	HAL_MCE_RECORD	= 0,
	HAL_MCA_RECORD	= 1,
};
union _MCI_STATS {
	struct {
		uint16	McaCod;
		uint16	MsCod;
		uint32	OtherInfo:25, Damage:1, AddressValid:1, MiscValid:1, Enabled:1, UnCorrected:1, OverFlow:1, Valid:1;
	}	MciStats;
	uint64	QuadPart;
};
union _MCI_ADDR {
	struct {
		uint32	Address;
		uint32	Reserved;
	};
	uint64	QuadPart;
};
struct _MCA_EXCEPTION {
	uint32	VersionNumber;
	MCA_EXCEPTION_TYPE	ExceptionType;
	_LARGE_INTEGER	TimeStamp;
	uint32	ProcessorNumber;
	uint32	Reserved1;
	union {
		struct {
			uint8	BankNumber;
			uint8	Reserved2[7];
			_MCI_STATS	Status;
			_MCI_ADDR	Address;
			uint64	Misc;
		}	Mca;
		struct {
			uint64	Address;
			uint64	Type;
		}	Mce;
	}	u;
	uint32	ExtCnt;
	uint32	Reserved3;
	uint64	ExtReg[24];
};
struct _RTL_BALANCED_LINKS {
	pointer32<_RTL_BALANCED_LINKS>	Parent;
	pointer32<_RTL_BALANCED_LINKS>	LeftChild;
	pointer32<_RTL_BALANCED_LINKS>	RightChild;
	char	Balance;
	uint8	Reserved[3];
};
enum _RTL_GENERIC_COMPARE_RESULTS {
	GenericLessThan	= 0,
	GenericGreaterThan	= 1,
	GenericEqual	= 2,
};
struct _RTL_AVL_TABLE {
	_RTL_BALANCED_LINKS	BalancedRoot;
	pointer32<void>	OrderedPointer;
	uint32	WhichOrderedElement;
	uint32	NumberGenericTableElements;
	uint32	DepthOfTree;
	pointer32<_RTL_BALANCED_LINKS>	RestartKey;
	uint32	DeleteCount;
	_RTL_GENERIC_COMPARE_RESULTS	(*CompareRoutine)(pointer32<_RTL_AVL_TABLE>, pointer32<void>, pointer32<void>);
	pointer32<void>	(*AllocateRoutine)(pointer32<_RTL_AVL_TABLE>, uint32);
	void	(*FreeRoutine)(pointer32<_RTL_AVL_TABLE>, pointer32<void>);
	pointer32<void>	TableContext;
};
struct _RTL_CRITICAL_SECTION;
struct _RTL_CRITICAL_SECTION_DEBUG {
	uint16	Type;
	uint16	CreatorBackTraceIndex;
	pointer32<_RTL_CRITICAL_SECTION>	CriticalSection;
	_LIST_ENTRY	ProcessLocksList;
	uint32	EntryCount;
	uint32	ContentionCount;
	uint32	Flags;
	uint16	CreatorBackTraceIndexHigh;
	uint16	SpareUSHORT;
};
struct _RTL_CRITICAL_SECTION {
	pointer32<_RTL_CRITICAL_SECTION_DEBUG>	DebugInfo;
	int32	LockCount;
	int32	RecursionCount;
	pointer32<void>	OwningThread;
	pointer32<void>	LockSemaphore;
	uint32	SpinCount;
};
struct _RTL_TRACE_BLOCK {
	uint32	Magic;
	uint32	Count;
	uint32	Size;
	uint32	UserCount;
	uint32	UserSize;
	pointer32<void>	UserContext;
	pointer32<_RTL_TRACE_BLOCK>	Next;
	pointer32<void>	*Trace;
};
struct _DPH_HEAP_BLOCK {
	union {
		pointer32<_DPH_HEAP_BLOCK>	pNextAlloc;
		_LIST_ENTRY	AvailableEntry;
		_RTL_BALANCED_LINKS	TableLinks;
	};
	pointer32<uint8>	pUserAllocation;
	pointer32<uint8>	pVirtualBlock;
	uint32	nVirtualBlockSize;
	uint32	nVirtualAccessSize;
	uint32	nUserRequestedSize;
	uint32	nUserActualSize;
	pointer32<void>	UserValue;
	uint32	UserFlags;
	pointer32<_RTL_TRACE_BLOCK>	StackTrace;
	_LIST_ENTRY	AdjacencyEntry;
	pointer32<uint8>	pVirtualRegion;
};
struct _DPH_HEAP_ROOT {
	uint32	Signature;
	uint32	HeapFlags;
	pointer32<_RTL_CRITICAL_SECTION>	HeapCritSect;
	uint32	nRemoteLockAcquired;
	pointer32<_DPH_HEAP_BLOCK>	pVirtualStorageListHead;
	pointer32<_DPH_HEAP_BLOCK>	pVirtualStorageListTail;
	uint32	nVirtualStorageRanges;
	uint32	nVirtualStorageBytes;
	_RTL_AVL_TABLE	BusyNodesTable;
	pointer32<_DPH_HEAP_BLOCK>	NodeToAllocate;
	uint32	nBusyAllocations;
	uint32	nBusyAllocationBytesCommitted;
	pointer32<_DPH_HEAP_BLOCK>	pFreeAllocationListHead;
	pointer32<_DPH_HEAP_BLOCK>	pFreeAllocationListTail;
	uint32	nFreeAllocations;
	uint32	nFreeAllocationBytesCommitted;
	_LIST_ENTRY	AvailableAllocationHead;
	uint32	nAvailableAllocations;
	uint32	nAvailableAllocationBytesCommitted;
	pointer32<_DPH_HEAP_BLOCK>	pUnusedNodeListHead;
	pointer32<_DPH_HEAP_BLOCK>	pUnusedNodeListTail;
	uint32	nUnusedNodes;
	uint32	nBusyAllocationBytesAccessible;
	pointer32<_DPH_HEAP_BLOCK>	pNodePoolListHead;
	pointer32<_DPH_HEAP_BLOCK>	pNodePoolListTail;
	uint32	nNodePools;
	uint32	nNodePoolBytes;
	_LIST_ENTRY	NextHeap;
	uint32	ExtraFlags;
	uint32	Seed;
	pointer32<void>	NormalHeap;
	pointer32<_RTL_TRACE_BLOCK>	CreateStackTrace;
	pointer32<void>	FirstThread;
};
union _WHEA_ERROR_RECORD_HEADER_VALIDBITS {
	struct {
		uint32	PlatformId:1, Timestamp:1, PartitionId:1, Reserved:29;
	};
	uint32	AsULONG;
};
enum POWER_ACTION {
	PowerActionNone	= 0,
	PowerActionReserved	= 1,
	PowerActionSleep	= 2,
	PowerActionHibernate	= 3,
	PowerActionShutdown	= 4,
	PowerActionShutdownReset	= 5,
	PowerActionShutdownOff	= 6,
	PowerActionWarmEject	= 7,
	PowerActionDisplayOff	= 8,
};
struct _SYSTEM_POWER_STATE_CONTEXT {
	union {
		struct {
			uint32	Reserved1:8, TargetSystemState:4, EffectiveSystemState:4, CurrentSystemState:4, IgnoreHibernationPath:1, PseudoTransition:1, Reserved2:10;
		};
		uint32	ContextAsUlong;
	};
};
struct _QUAD {
	union {
		int64	UseThisFieldToCopy;
		float	DoNotUseThisField;
	};
};
enum _HEAP_FAILURE_TYPE {
	heap_failure_internal	= 0,
	heap_failure_unknown	= 1,
	heap_failure_generic	= 2,
	heap_failure_entry_corruption	= 3,
	heap_failure_multiple_entries_corruption	= 4,
	heap_failure_virtual_block_corruption	= 5,
	heap_failure_buffer_overrun	= 6,
	heap_failure_buffer_underrun	= 7,
	heap_failure_block_not_busy	= 8,
	heap_failure_invalid_argument	= 9,
	heap_failure_usage_after_free	= 10,
	heap_failure_cross_heap_operation	= 11,
	heap_failure_freelists_corruption	= 12,
	heap_failure_listentry_corruption	= 13,
	heap_failure_lfh_bitmap_mismatch	= 14,
	heap_failure_segment_lfh_bitmap_corruption	= 15,
	heap_failure_segment_lfh_double_free	= 16,
	heap_failure_vs_subsegment_corruption	= 17,
};
struct _HEAP_FAILURE_INFORMATION {
	uint32	Version;
	uint32	StructureSize;
	_HEAP_FAILURE_TYPE	FailureType;
	pointer32<void>	HeapAddress;
	pointer32<void>	Address;
	pointer32<void>	Param1;
	pointer32<void>	Param2;
	pointer32<void>	Param3;
	pointer32<_HEAP_ENTRY>	PreviousBlock;
	pointer32<_HEAP_ENTRY>	NextBlock;
	_HEAP_ENTRY	ExpectedEncodedEntry;
	_HEAP_ENTRY	ExpectedDecodedEntry;
	pointer32<void>	StackTrace[32];
	uint8	HeapMajorVersion;
	uint8	HeapMinorVersion;
	_EXCEPTION_RECORD	ExceptionRecord;
	_CONTEXT	ContextRecord;
};
struct _ACL {
	uint8	AclRevision;
	uint8	Sbz1;
	uint16	AclSize;
	uint16	AceCount;
	uint16	Sbz2;
};
enum _SYSTEM_POWER_STATE {
	PowerSystemUnspecified	= 0,
	PowerSystemWorking	= 1,
	PowerSystemSleeping1	= 2,
	PowerSystemSleeping2	= 3,
	PowerSystemSleeping3	= 4,
	PowerSystemHibernate	= 5,
	PowerSystemShutdown	= 6,
	PowerSystemMaximum	= 7,
};
struct _EXT_DELETE_PARAMETERS {
	uint32	Version;
	uint32	Reserved;
	void	(*DeleteCallback)(pointer32<void>);
	pointer32<void>	DeleteContext;
};
enum _KINTERRUPT_MODE {
	LevelSensitive	= 0,
	Latched	= 1,
};
enum _KINTERRUPT_POLARITY {
	InterruptPolarityUnknown	= 0,
	InterruptActiveHigh	= 1,
	InterruptRisingEdge	= 1,
	InterruptActiveLow	= 2,
	InterruptFallingEdge	= 2,
	InterruptActiveBoth	= 3,
	InterruptActiveBothTriggerLow	= 3,
	InterruptActiveBothTriggerHigh	= 4,
};
struct _ISRDPCSTATS {
	uint64	IsrTime;
	uint64	IsrTimeStart;
	uint64	IsrCount;
	uint64	DpcTime;
	uint64	DpcTimeStart;
	uint64	DpcCount;
	uint8	IsrActive;
	uint8	Reserved[15];
};
enum INTERRUPT_CONNECTION_TYPE {
	InterruptTypeControllerInput	= 0,
	InterruptTypeXapicMessage	= 1,
	InterruptTypeHypertransport	= 2,
	InterruptTypeMessageRequest	= 3,
};
struct _INTERRUPT_REMAPPING_INFO {
	uint32	IrtIndex:30, FlagHalInternal:1, FlagTranslated:1;
	union {
		_ULARGE_INTEGER	RemappedFormat;
		struct {
			uint32	MessageAddressLow;
			uint16	MessageData;
			uint16	Reserved;
		}	Msi;
	}	u;
};
struct _INTERRUPT_HT_INTR_INFO {
	union {
		struct {
			uint32	Mask:1, Polarity:1, MessageType:3, RequestEOI:1, DestinationMode:1, MessageType3:1, Destination:8, Vector:8, ExtendedAddress:8;
		}	bits;
		uint32	AsULONG;
	}	LowPart;
	union {
		struct {
			uint32	ExtendedDestination:24, Reserved:6, PassPW:1, WaitingForEOI:1;
		}	bits;
		uint32	AsULONG;
	}	HighPart;
};
enum HAL_APIC_DESTINATION_MODE {
	ApicDestinationModePhysical	= 1,
	ApicDestinationModeLogicalFlat	= 2,
	ApicDestinationModeLogicalClustered	= 3,
	ApicDestinationModeUnknown	= 4,
};
struct _INTERRUPT_VECTOR_DATA {
	INTERRUPT_CONNECTION_TYPE	Type;
	uint32	Vector;
	uint8	Irql;
	_KINTERRUPT_POLARITY	Polarity;
	_KINTERRUPT_MODE	Mode;
	_GROUP_AFFINITY	TargetProcessors;
	_INTERRUPT_REMAPPING_INFO	IntRemapInfo;
	struct {
		uint32	Gsiv;
		uint32	WakeInterrupt:1, ReservedFlags:31;
	}	ControllerInput;
	uint64	HvDeviceId;
	union {
		struct {
			_LARGE_INTEGER	Address;
			uint32	DataPayload;
		}	XapicMessage;
		struct {
			_INTERRUPT_HT_INTR_INFO	IntrInfo;
		}	Hypertransport;
		struct {
			_LARGE_INTEGER	Address;
			uint32	DataPayload;
		}	GenericMessage;
		struct {
			HAL_APIC_DESTINATION_MODE	DestinationMode;
		}	MessageRequest;
	};
};
struct _INTERRUPT_CONNECTION_DATA {
	uint32	Count;
	_INTERRUPT_VECTOR_DATA	Vectors[1];
};
struct _KINTERRUPT {
	int16	Type;
	int16	Size;
	_LIST_ENTRY	InterruptListEntry;
	uint8	(*ServiceRoutine)();
	uint8	(*MessageServiceRoutine)(pointer32<_KINTERRUPT>, pointer32<void>, uint32);
	uint32	MessageIndex;
	pointer32<void>	ServiceContext;
	uint32	SpinLock;
	uint32	TickCount;
	pointer32<uint32>	ActualLock;
	void	(*DispatchAddress)();
	uint32	Vector;
	uint8	Irql;
	uint8	SynchronizeIrql;
	uint8	FloatingSave;
	uint8	Connected;
	uint32	Number;
	uint8	ShareVector;
	uint8	EmulateActiveBoth;
	uint16	ActiveCount;
	int32	InternalState;
	_KINTERRUPT_MODE	Mode;
	_KINTERRUPT_POLARITY	Polarity;
	uint32	ServiceCount;
	uint32	DispatchCount;
	pointer32<_KEVENT>	PassiveEvent;
	pointer32<void>	DisconnectData;
	pointer32<_KTHREAD>	ServiceThread;
	pointer32<_INTERRUPT_CONNECTION_DATA>	ConnectionData;
	pointer32<void>	IntTrackEntry;
	_ISRDPCSTATS	IsrDpcStats;
	pointer32<void>	RedirectObject;
};
enum _ALTERNATIVE_ARCHITECTURE_TYPE {
	StandardDesign	= 0,
	NEC98x86	= 1,
	EndAlternatives	= 2,
};
enum _OB_OPEN_REASON {
	ObCreateHandle	= 0,
	ObOpenHandle	= 1,
	ObDuplicateHandle	= 2,
	ObInheritHandle	= 3,
	ObMaxOpenReason	= 4,
};
enum _FS_FILTER_SECTION_SYNC_TYPE {
	SyncTypeOther	= 0,
	SyncTypeCreateSection	= 1,
};
struct _LFH_BLOCK_ZONE {
	_LIST_ENTRY	ListEntry;
	int32	NextIndex;
};
struct _HEAP_LFH_MEM_POLICIES {
	union {
		struct {
			uint32	DisableAffinity:1, SlowSubsegmentGrowth:1, Spare:30;
		};
		uint32	AllPolicies;
	};
};
struct _HEAP_BUCKET {
	uint16	BlockUnits;
	uint8	SizeIndex;
	union {
		struct {
			uint8	UseAffinity:1, DebugFlags:2;
		};
		uint8	Flags;
	};
};
struct _LFH_HEAP;
struct _HEAP_LOCAL_DATA {
	_SLIST_HEADER	DeletedSubSegments;
	pointer32<_LFH_BLOCK_ZONE>	CrtZone;
	pointer32<_LFH_HEAP>	LowFragHeap;
	uint32	Sequence;
	uint32	DeleteRateThreshold;
};
struct _ACTIVATION_CONTEXT {
};
struct _GENERIC_MAPPING {
	uint32	GenericRead;
	uint32	GenericWrite;
	uint32	GenericExecute;
	uint32	GenericAll;
};
struct _OBJECT_DUMP_CONTROL {
	pointer32<void>	Stream;
	uint32	Detail;
};
union _WHEA_ERROR_PACKET_FLAGS {
	struct {
		uint32	PreviousError:1, Reserved1:1, HypervisorError:1, Simulated:1, PlatformPfaControl:1, PlatformDirectedOffline:1, Reserved2:26;
	};
	uint32	AsULONG;
};
enum _WHEA_ERROR_TYPE {
	WheaErrTypeProcessor	= 0,
	WheaErrTypeMemory	= 1,
	WheaErrTypePCIExpress	= 2,
	WheaErrTypeNMI	= 3,
	WheaErrTypePCIXBus	= 4,
	WheaErrTypePCIXDevice	= 5,
	WheaErrTypeGeneric	= 6,
};
enum _WHEA_ERROR_SOURCE_TYPE {
	WheaErrSrcTypeMCE	= 0,
	WheaErrSrcTypeCMC	= 1,
	WheaErrSrcTypeCPE	= 2,
	WheaErrSrcTypeNMI	= 3,
	WheaErrSrcTypePCIe	= 4,
	WheaErrSrcTypeGeneric	= 5,
	WheaErrSrcTypeINIT	= 6,
	WheaErrSrcTypeBOOT	= 7,
	WheaErrSrcTypeSCIGeneric	= 8,
	WheaErrSrcTypeIPFMCA	= 9,
	WheaErrSrcTypeIPFCMC	= 10,
	WheaErrSrcTypeIPFCPE	= 11,
	WheaErrSrcTypeMax	= 12,
};
struct _GUID {
	uint32	Data1;
	uint16	Data2;
	uint16	Data3;
	uint8	Data4[8];
};
enum _WHEA_ERROR_PACKET_DATA_FORMAT {
	WheaDataFormatIPFSalRecord	= 0,
	WheaDataFormatXPFMCA	= 1,
	WheaDataFormatMemory	= 2,
	WheaDataFormatPCIExpress	= 3,
	WheaDataFormatNMIPort	= 4,
	WheaDataFormatPCIXBus	= 5,
	WheaDataFormatPCIXDevice	= 6,
	WheaDataFormatGeneric	= 7,
	WheaDataFormatMax	= 8,
};
struct _WHEA_ERROR_PACKET_V2 {
	uint32	Signature;
	uint32	Version;
	uint32	Length;
	_WHEA_ERROR_PACKET_FLAGS	Flags;
	_WHEA_ERROR_TYPE	ErrorType;
	_WHEA_ERROR_SEVERITY	ErrorSeverity;
	uint32	ErrorSourceId;
	_WHEA_ERROR_SOURCE_TYPE	ErrorSourceType;
	_GUID	NotifyType;
	uint64	Context;
	_WHEA_ERROR_PACKET_DATA_FORMAT	DataFormat;
	uint32	Reserved1;
	uint32	DataOffset;
	uint32	DataLength;
	uint32	PshedDataOffset;
	uint32	PshedDataLength;
};
struct _ECP_LIST {
};
struct _TXN_PARAMETER_BLOCK {
	uint16	Length;
	uint16	TxFsContext;
	pointer32<void>	TransactionObject;
};
struct _KWAIT_CHAIN {
	_SINGLE_LIST_ENTRY	Head;
};
struct _ERESOURCE {
	_LIST_ENTRY	SystemResourcesList;
	pointer32<_OWNER_ENTRY>	OwnerTable;
	int16	ActiveCount;
	union {
		uint16	Flag;
		struct {
			uint8	ReservedLowFlags;
			uint8	WaiterPriority;
		};
	};
	_KWAIT_CHAIN	SharedWaiters;
	pointer32<_KEVENT>	ExclusiveWaiters;
	_OWNER_ENTRY	OwnerEntry;
	uint32	ActiveEntries;
	uint32	ContentionCount;
	uint32	NumberOfSharedWaiters;
	uint32	NumberOfExclusiveWaiters;
	union {
		pointer32<void>	Address;
		uint32	CreatorBackTraceIndex;
	};
	uint32	SpinLock;
};
struct _PROCESS_DISK_COUNTERS {
	uint64	BytesRead;
	uint64	BytesWritten;
	uint64	ReadOperationCount;
	uint64	WriteOperationCount;
	uint64	FlushOperationCount;
};
struct _WNF_STATE_NAME {
	uint32	Data[2];
};
struct _PS_WAKE_INFORMATION {
	uint64	NotificationChannel;
	uint64	WakeCounters[5];
	uint64	NoWakeCounter;
};
struct _JOBOBJECT_WAKE_FILTER {
	uint32	HighEdgeFilter;
	uint32	LowEdgeFilter;
};
struct _EPROCESS_VALUES {
	uint64	KernelTime;
	uint64	UserTime;
	uint64	CycleTime;
	uint64	ContextSwitches;
	int64	ReadOperationCount;
	int64	WriteOperationCount;
	int64	OtherOperationCount;
	int64	ReadTransferCount;
	int64	WriteTransferCount;
	int64	OtherTransferCount;
};
struct _JOB_ACCESS_STATE {
};
struct _JOB_NOTIFICATION_INFORMATION {
};
struct _IO_MINI_COMPLETION_PACKET_USER {
	_LIST_ENTRY	ListEntry;
	uint32	PacketType;
	pointer32<void>	KeyContext;
	pointer32<void>	ApcContext;
	int32	IoStatus;
	uint32	IoStatusInformation;
	void	(*MiniPacketCallback)(pointer32<_IO_MINI_COMPLETION_PACKET_USER>, pointer32<void>);
	pointer32<void>	Context;
	uint8	Allocated;
};
struct _JOB_CPU_RATE_CONTROL {
};
struct _SILO_CONTEXT {
};
struct _JOB_NET_RATE_CONTROL {
};
struct _JOB_IO_RATE_CONTROL {
};
struct _PROCESS_ENERGY_VALUES {
	uint64	Cycles[2][4];
	uint64	DiskEnergy;
	uint64	NetworkTailEnergy;
	uint64	MBBTailEnergy;
	uint64	NetworkTxRxBytes;
	uint64	MBBTxRxBytes;
	union {
		uint32	Foreground:1;
		uint32	WindowInformation;
	};
	uint32	PixelArea;
	int64	PixelReportTimestamp;
	uint64	PixelTime;
	int64	ForegroundReportTimestamp;
	uint64	ForegroundTime;
};
struct _EJOB {
	_KEVENT	Event;
	_LIST_ENTRY	JobLinks;
	_LIST_ENTRY	ProcessListHead;
	_ERESOURCE	JobLock;
	_LARGE_INTEGER	TotalUserTime;
	_LARGE_INTEGER	TotalKernelTime;
	_LARGE_INTEGER	TotalCycleTime;
	_LARGE_INTEGER	ThisPeriodTotalUserTime;
	_LARGE_INTEGER	ThisPeriodTotalKernelTime;
	uint64	TotalContextSwitches;
	uint32	TotalPageFaultCount;
	uint32	TotalProcesses;
	uint32	ActiveProcesses;
	uint32	TotalTerminatedProcesses;
	_LARGE_INTEGER	PerProcessUserTimeLimit;
	_LARGE_INTEGER	PerJobUserTimeLimit;
	uint32	MinimumWorkingSetSize;
	uint32	MaximumWorkingSetSize;
	uint32	LimitFlags;
	uint32	ActiveProcessLimit;
	_KAFFINITY_EX	Affinity;
	pointer32<_JOB_ACCESS_STATE>	AccessState;
	pointer32<void>	AccessStateQuotaReference;
	uint32	UIRestrictionsClass;
	uint32	EndOfJobTimeAction;
	pointer32<void>	CompletionPort;
	pointer32<void>	CompletionKey;
	uint64	CompletionCount;
	uint32	SessionId;
	uint32	SchedulingClass;
	uint64	ReadOperationCount;
	uint64	WriteOperationCount;
	uint64	OtherOperationCount;
	uint64	ReadTransferCount;
	uint64	WriteTransferCount;
	uint64	OtherTransferCount;
	_PROCESS_DISK_COUNTERS	DiskIoInfo;
	uint32	ProcessMemoryLimit;
	uint32	JobMemoryLimit;
	uint32	JobTotalMemoryLimit;
	uint32	PeakProcessMemoryUsed;
	uint32	PeakJobMemoryUsed;
	_KAFFINITY_EX	EffectiveAffinity;
	_LARGE_INTEGER	EffectivePerProcessUserTimeLimit;
	uint32	EffectiveMinimumWorkingSetSize;
	uint32	EffectiveMaximumWorkingSetSize;
	uint32	EffectiveProcessMemoryLimit;
	pointer32<_EJOB>	EffectiveProcessMemoryLimitJob;
	pointer32<_EJOB>	EffectivePerProcessUserTimeLimitJob;
	pointer32<_EJOB>	EffectiveDiskIoRateLimitJob;
	pointer32<_EJOB>	EffectiveNetIoRateLimitJob;
	pointer32<_EJOB>	EffectiveHeapAttributionJob;
	uint32	EffectiveLimitFlags;
	uint32	EffectiveSchedulingClass;
	uint32	EffectiveFreezeCount;
	uint32	EffectiveBackgroundCount;
	uint32	EffectiveSwapCount;
	uint32	EffectiveNotificationLimitCount;
	uint8	EffectivePriorityClass;
	uint8	PriorityClass;
	uint8	NestingDepth;
	uint8	Reserved1[1];
	uint32	CompletionFilter;
	union {
		_WNF_STATE_NAME	WakeChannel;
		_PS_WAKE_INFORMATION	WakeInfo;
	};
	_JOBOBJECT_WAKE_FILTER	WakeFilter;
	uint32	LowEdgeLatchFilter;
	uint32	OwnedHighEdgeFilters;
	pointer32<_EJOB>	NotificationLink;
	uint64	CurrentJobMemoryUsed;
	pointer32<_JOB_NOTIFICATION_INFORMATION>	NotificationInfo;
	pointer32<void>	NotificationInfoQuotaReference;
	pointer32<_IO_MINI_COMPLETION_PACKET_USER>	NotificationPacket;
	pointer32<_JOB_CPU_RATE_CONTROL>	CpuRateControl;
	pointer32<void>	EffectiveSchedulingGroup;
	uint64	ReadyTime;
	_EX_PUSH_LOCK	MemoryLimitsLock;
	_LIST_ENTRY	SiblingJobLinks;
	_LIST_ENTRY	ChildJobListHead;
	pointer32<_EJOB>	ParentJob;
	pointer32<_EJOB>	RootJob;
	_LIST_ENTRY	IteratorListHead;
	uint32	AncestorCount;
	union {
		pointer32<_EJOB>	*Ancestors;
		pointer32<void>	SessionObject;
	};
	_EPROCESS_VALUES	Accounting;
	uint32	ShadowActiveProcessCount;
	uint32	ActiveAuxiliaryProcessCount;
	uint32	SequenceNumber;
	uint32	TimerListLock;
	_LIST_ENTRY	TimerListHead;
	_GUID	ContainerId;
	pointer32<_SILO_CONTEXT>	Container;
	_PS_PROPERTY_SET	PropertySet;
	pointer32<_JOB_NET_RATE_CONTROL>	NetRateControl;
	pointer32<_JOB_IO_RATE_CONTROL>	IoRateControl;
	union {
		uint32	JobFlags;
		struct {
			uint32	CloseDone:1, MultiGroup:1, OutstandingNotification:1, NotificationInProgress:1, UILimits:1, CpuRateControlActive:1, OwnCpuRateControl:1, Terminating:1, WorkingSetLock:1, JobFrozen:1, Background:1, WakeNotificationAllocated:1, WakeNotificationEnabled:1, WakeNotificationPending:1, LimitNotificationRequired:1, ZeroCountNotificationRequired:1, CycleTimeNotificationRequired:1, CycleTimeNotificationPending:1, TimersVirtualized:1, JobSwapped:1, ViolationDetected:1, EmptyJobNotified:1, NoSystemCharge:1, DropNoWakeCharges:1, NoWakeChargePolicyDecided:1, NetRateControlActive:1, OwnNetRateControl:1, IoRateControlActive:1, OwnIoRateControl:1, DisallowNewProcesses:1, SpareJobFlags:2;
		};
	};
	uint32	EffectiveHighEdgeFilters;
	pointer32<_PROCESS_ENERGY_VALUES>	EnergyValues;
	uint32	SharedCommitCharge;
};
struct _IO_DRIVER_CREATE_CONTEXT {
	int16	Size;
	pointer32<_ECP_LIST>	ExtraCreateParameter;
	pointer32<void>	DeviceObjectHint;
	pointer32<_TXN_PARAMETER_BLOCK>	TxnParameters;
	pointer32<_EJOB>	SiloContext;
};
struct _SID {
	uint8	Revision;
	uint8	SubAuthorityCount;
	_SID_IDENTIFIER_AUTHORITY	IdentifierAuthority;
	uint32	SubAuthority[1];
};
enum _FILE_INFORMATION_CLASS {
	FileDirectoryInformation	= 1,
	FileFullDirectoryInformation	= 2,
	FileBothDirectoryInformation	= 3,
	FileBasicInformation	= 4,
	FileStandardInformation	= 5,
	FileInternalInformation	= 6,
	FileEaInformation	= 7,
	FileAccessInformation	= 8,
	FileNameInformation	= 9,
	FileRenameInformation	= 10,
	FileLinkInformation	= 11,
	FileNamesInformation	= 12,
	FileDispositionInformation	= 13,
	FilePositionInformation	= 14,
	FileFullEaInformation	= 15,
	FileModeInformation	= 16,
	FileAlignmentInformation	= 17,
	FileAllInformation	= 18,
	FileAllocationInformation	= 19,
	FileEndOfFileInformation	= 20,
	FileAlternateNameInformation	= 21,
	FileStreamInformation	= 22,
	FilePipeInformation	= 23,
	FilePipeLocalInformation	= 24,
	FilePipeRemoteInformation	= 25,
	FileMailslotQueryInformation	= 26,
	FileMailslotSetInformation	= 27,
	FileCompressionInformation	= 28,
	FileObjectIdInformation	= 29,
	FileCompletionInformation	= 30,
	FileMoveClusterInformation	= 31,
	FileQuotaInformation	= 32,
	FileReparsePointInformation	= 33,
	FileNetworkOpenInformation	= 34,
	FileAttributeTagInformation	= 35,
	FileTrackingInformation	= 36,
	FileIdBothDirectoryInformation	= 37,
	FileIdFullDirectoryInformation	= 38,
	FileValidDataLengthInformation	= 39,
	FileShortNameInformation	= 40,
	FileIoCompletionNotificationInformation	= 41,
	FileIoStatusBlockRangeInformation	= 42,
	FileIoPriorityHintInformation	= 43,
	FileSfioReserveInformation	= 44,
	FileSfioVolumeInformation	= 45,
	FileHardLinkInformation	= 46,
	FileProcessIdsUsingFileInformation	= 47,
	FileNormalizedNameInformation	= 48,
	FileNetworkPhysicalNameInformation	= 49,
	FileIdGlobalTxDirectoryInformation	= 50,
	FileIsRemoteDeviceInformation	= 51,
	FileUnusedInformation	= 52,
	FileNumaNodeInformation	= 53,
	FileStandardLinkInformation	= 54,
	FileRemoteProtocolInformation	= 55,
	FileRenameInformationBypassAccessCheck	= 56,
	FileLinkInformationBypassAccessCheck	= 57,
	FileVolumeNameInformation	= 58,
	FileIdInformation	= 59,
	FileIdExtdDirectoryInformation	= 60,
	FileReplaceCompletionInformation	= 61,
	FileHardLinkFullIdInformation	= 62,
	FileIdExtdBothDirectoryInformation	= 63,
	FileMaximumInformation	= 64,
};
enum _HEAP_LFH_LOCKMODE {
	HeapLockNotHeld	= 0,
	HeapLockShared	= 1,
	HeapLockExclusive	= 2,
};
struct _STRING {
	uint16	Length;
	uint16	MaximumLength;
	pointer32<char>	Buffer;
};
struct _RTL_DRIVE_LETTER_CURDIR {
	uint16	Flags;
	uint16	Length;
	uint32	TimeStamp;
	_STRING	DosPath;
};
enum _TP_CALLBACK_PRIORITY {
	TP_CALLBACK_PRIORITY_HIGH	= 0,
	TP_CALLBACK_PRIORITY_NORMAL	= 1,
	TP_CALLBACK_PRIORITY_LOW	= 2,
	TP_CALLBACK_PRIORITY_INVALID	= 3,
	TP_CALLBACK_PRIORITY_COUNT	= 3,
};
struct _FAST_MUTEX {
	int32	Count;
	pointer32<void>	Owner;
	uint32	Contention;
	_KEVENT	Event;
	uint32	OldIrql;
};
struct _HANDLE_TRACE_DEBUG_INFO {
	int32	RefCount;
	uint32	TableSize;
	uint32	BitMaskFlags;
	_FAST_MUTEX	CloseCompactionLock;
	uint32	CurrentStackIndex;
	_HANDLE_TRACE_DB_ENTRY	TraceDb[1];
};
struct _HEAP_LFH_SUBSEGMENT_OWNER {
	uint8	IsBucket:1, Spare0:7;
	uint8	BucketIndex;
	union {
		uint8	SlotCount;
		uint8	SlotIndex;
	};
	uint8	Spare1;
	uint32	AvailableSubsegmentCount;
	_RTL_SRWLOCK	Lock;
	_LIST_ENTRY	AvailableSubsegmentList;
	_LIST_ENTRY	FullSubsegmentList;
};
enum _EX_GEN_RANDOM_DOMAIN {
	ExGenRandomDomainKernel	= 0,
	ExGenRandomDomainFirst	= 0,
	ExGenRandomDomainUserVisible	= 1,
	ExGenRandomDomainMax	= 2,
};
struct _SECURITY_DESCRIPTOR {
	uint8	Revision;
	uint8	Sbz1;
	uint16	Control;
	pointer32<void>	Owner;
	pointer32<void>	Group;
	pointer32<_ACL>	Sacl;
	pointer32<_ACL>	Dacl;
};
enum JOB_OBJECT_NET_RATE_CONTROL_FLAGS {
	JOB_OBJECT_NET_RATE_CONTROL_ENABLE	= 1,
	JOB_OBJECT_NET_RATE_CONTROL_MAX_BANDWIDTH	= 2,
	JOB_OBJECT_NET_RATE_CONTROL_DSCP_TAG	= 4,
	JOB_OBJECT_NET_RATE_CONTROL_VALID_FLAGS	= 7,
};
union _WHEA_TIMESTAMP {
	struct {
		uint64	Seconds:8, Minutes:8, Hours:8, Precise:1, Reserved:7, Day:8, Month:8, Year:8, Century:8;
	};
	_LARGE_INTEGER	AsLARGE_INTEGER;
};
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
struct _CLIENT_ID64 {
	uint64	UniqueProcess;
	uint64	UniqueThread;
};
struct _GDI_TEB_BATCH64 {
	uint32	Offset:31, HasRenderingCommand:1;
	uint64	HDC;
	uint32	Buffer[310];
};
struct _STRING64 {
	uint16	Length;
	uint16	MaximumLength;
	uint64	Buffer;
};
struct LIST_ENTRY64 {
	uint64	Flink;
	uint64	Blink;
};
struct _TEB64 {
	_NT_TIB64	NtTib;
	uint64	EnvironmentPointer;
	_CLIENT_ID64	ClientId;
	uint64	ActiveRpcHandle;
	uint64	ThreadLocalStoragePointer;
	uint64	ProcessEnvironmentBlock;
	uint32	LastErrorValue;
	uint32	CountOfOwnedCriticalSections;
	uint64	CsrClientThread;
	uint64	Win32ThreadInfo;
	uint32	User32Reserved[26];
	uint32	UserReserved[5];
	uint64	WOW32Reserved;
	uint32	CurrentLocale;
	uint32	FpSoftwareStatusRegister;
	uint64	ReservedForDebuggerInstrumentation[16];
	uint64	SystemReserved1[38];
	int32	ExceptionCode;
	uint8	Padding0[4];
	uint64	ActivationContextStackPointer;
	uint64	InstrumentationCallbackSp;
	uint64	InstrumentationCallbackPreviousPc;
	uint64	InstrumentationCallbackPreviousSp;
	uint32	TxFsContext;
	uint8	InstrumentationCallbackDisabled;
	uint8	Padding1[3];
	_GDI_TEB_BATCH64	GdiTebBatch;
	_CLIENT_ID64	RealClientId;
	uint64	GdiCachedProcessHandle;
	uint32	GdiClientPID;
	uint32	GdiClientTID;
	uint64	GdiThreadLocalInfo;
	uint64	Win32ClientInfo[62];
	uint64	glDispatchTable[233];
	uint64	glReserved1[29];
	uint64	glReserved2;
	uint64	glSectionInfo;
	uint64	glSection;
	uint64	glTable;
	uint64	glCurrentRC;
	uint64	glContext;
	uint32	LastStatusValue;
	uint8	Padding2[4];
	_STRING64	StaticUnicodeString;
	wchar_t	StaticUnicodeBuffer[261];
	uint8	Padding3[6];
	uint64	DeallocationStack;
	uint64	TlsSlots[64];
	LIST_ENTRY64	TlsLinks;
	uint64	Vdm;
	uint64	ReservedForNtRpc;
	uint64	DbgSsReserved[2];
	uint32	HardErrorMode;
	uint8	Padding4[4];
	uint64	Instrumentation[11];
	_GUID	ActivityId;
	uint64	SubProcessTag;
	uint64	PerflibData;
	uint64	EtwTraceData;
	uint64	WinSockData;
	uint32	GdiBatchCount;
	union {
		_PROCESSOR_NUMBER	CurrentIdealProcessor;
		uint32	IdealProcessorValue;
		struct {
			uint8	ReservedPad0;
			uint8	ReservedPad1;
			uint8	ReservedPad2;
			uint8	IdealProcessor;
		};
	};
	uint32	GuaranteedStackBytes;
	uint8	Padding5[4];
	uint64	ReservedForPerf;
	uint64	ReservedForOle;
	uint32	WaitingOnLoaderLock;
	uint8	Padding6[4];
	uint64	SavedPriorityState;
	uint64	ReservedForCodeCoverage;
	uint64	ThreadPoolData;
	uint64	TlsExpansionSlots;
	uint64	DeallocationBStore;
	uint64	BStoreLimit;
	uint32	MuiGeneration;
	uint32	IsImpersonating;
	uint64	NlsCache;
	uint64	pShimData;
	uint16	HeapVirtualAffinity;
	uint16	LowFragHeapDataSlot;
	uint8	Padding7[4];
	uint64	CurrentTransactionHandle;
	uint64	ActiveFrame;
	uint64	FlsData;
	uint64	PreferredLanguages;
	uint64	UserPrefLanguages;
	uint64	MergedPrefLanguages;
	uint32	MuiImpersonation;
	union {
		uint16	CrossTebFlags;
		uint16	SpareCrossTebBits:16;
	};
	union {
		uint16	SameTebFlags;
		struct {
			uint16	SafeThunkCall:1, InDebugPrint:1, HasFiberData:1, SkipThreadAttach:1, WerInShipAssertCode:1, RanProcessInit:1, ClonedThread:1, SuppressDebugMsg:1, DisableUserStackWalk:1, RtlExceptionAttached:1, InitialThread:1, SessionAware:1, LoadOwner:1, LoaderWorker:1, SpareSameTebBits:2;
		};
	};
	uint64	TxnScopeEnterCallback;
	uint64	TxnScopeExitCallback;
	uint64	TxnScopeContext;
	uint32	LockCount;
	int32	WowTebOffset;
	uint64	ResourceRetValue;
	uint64	ReservedForWdf;
	uint64	ReservedForCrt;
	_GUID	EffectiveContainerId;
};
struct _RTL_SPLAY_LINKS {
	pointer32<_RTL_SPLAY_LINKS>	Parent;
	pointer32<_RTL_SPLAY_LINKS>	LeftChild;
	pointer32<_RTL_SPLAY_LINKS>	RightChild;
};
struct _KERNEL_STACK_SEGMENT {
	uint32	StackBase;
	uint32	StackLimit;
	uint32	KernelStack;
	uint32	InitialStack;
};
struct _KTIMER_EXPIRATION_TRACE {
	uint64	InterruptTime;
	_LARGE_INTEGER	PerformanceCounter;
};
struct _HEAP_LFH_AFFINITY_SLOT {
	_HEAP_LFH_SUBSEGMENT_OWNER	State;
};
struct _KSYSTEM_TIME {
	uint32	LowPart;
	int32	High1Time;
	int32	High2Time;
};
enum _NT_PRODUCT_TYPE {
	NtProductWinNt	= 1,
	NtProductLanManNt	= 2,
	NtProductServer	= 3,
};
struct _XSTATE_FEATURE {
	uint32	Offset;
	uint32	Size;
};
struct _XSTATE_CONFIGURATION {
	uint64	EnabledFeatures;
	uint64	EnabledVolatileFeatures;
	uint32	Size;
	uint32	OptimizedSave:1, CompactionEnabled:1;
	_XSTATE_FEATURE	Features[64];
	uint64	EnabledSupervisorFeatures;
	uint64	AlignedFeatures;
	uint32	AllFeatureSize;
	uint32	AllFeatures[64];
};
struct _KUSER_SHARED_DATA {
	uint32	TickCountLowDeprecated;
	uint32	TickCountMultiplier;
	_KSYSTEM_TIME	InterruptTime;
	_KSYSTEM_TIME	SystemTime;
	_KSYSTEM_TIME	TimeZoneBias;
	uint16	ImageNumberLow;
	uint16	ImageNumberHigh;
	wchar_t	NtSystemRoot[260];
	uint32	MaxStackTraceDepth;
	uint32	CryptoExponent;
	uint32	TimeZoneId;
	uint32	LargePageMinimum;
	uint32	AitSamplingValue;
	uint32	AppCompatFlag;
	uint64	RNGSeedVersion;
	uint32	GlobalValidationRunlevel;
	int32	TimeZoneBiasStamp;
	uint32	NtBuildNumber;
	_NT_PRODUCT_TYPE	NtProductType;
	uint8	ProductTypeIsValid;
	uint8	Reserved0[1];
	uint16	NativeProcessorArchitecture;
	uint32	NtMajorVersion;
	uint32	NtMinorVersion;
	uint8	ProcessorFeatures[64];
	uint32	Reserved1;
	uint32	Reserved3;
	uint32	TimeSlip;
	_ALTERNATIVE_ARCHITECTURE_TYPE	AlternativeArchitecture;
	uint32	BootId;
	_LARGE_INTEGER	SystemExpirationDate;
	uint32	SuiteMask;
	uint8	KdDebuggerEnabled;
	union {
		uint8	MitigationPolicies;
		struct {
			uint8	NXSupportPolicy:2, SEHValidationPolicy:2, CurDirDevicesSkippedForDlls:2, Reserved:2;
		};
	};
	uint8	Reserved6[2];
	uint32	ActiveConsoleId;
	uint32	DismountCount;
	uint32	ComPlusPackage;
	uint32	LastSystemRITEventTickCount;
	uint32	NumberOfPhysicalPages;
	uint8	SafeBootMode;
	uint8	Reserved12[3];
	union {
		uint32	SharedDataFlags;
		struct {
			uint32	DbgErrorPortPresent:1, DbgElevationEnabled:1, DbgVirtEnabled:1, DbgInstallerDetectEnabled:1, DbgLkgEnabled:1, DbgDynProcessorEnabled:1, DbgConsoleBrokerEnabled:1, DbgSecureBootEnabled:1, DbgMultiSessionSku:1, SpareBits:23;
		};
	};
	uint32	DataFlagsPad[1];
	uint64	TestRetInstruction;
	int64	QpcFrequency;
	uint32	SystemCall;
	uint32	SystemCallPad0;
	uint64	SystemCallPad[2];
	union {
		_KSYSTEM_TIME	TickCount;
		uint64	TickCountQuad;
		struct {
			uint32	ReservedTickCountOverlay[3];
			uint32	TickCountPad[1];
		};
	};
	uint32	Cookie;
	uint32	CookiePad[1];
	int64	ConsoleSessionForegroundProcessId;
	uint64	TimeUpdateLock;
	uint64	BaselineSystemTimeQpc;
	uint64	BaselineInterruptTimeQpc;
	uint64	QpcSystemTimeIncrement;
	uint64	QpcInterruptTimeIncrement;
	uint8	QpcSystemTimeIncrementShift;
	uint8	QpcInterruptTimeIncrementShift;
	uint16	UnparkedProcessorCount;
	uint32	EnclaveFeatureMask[4];
	uint32	Reserved8;
	uint16	UserModeGlobalLogger[16];
	uint32	ImageFileExecutionOptions;
	uint32	LangGenerationCount;
	uint64	Reserved4;
	uint64	InterruptTimeBias;
	uint64	QpcBias;
	uint32	ActiveProcessorCount;
	uint8	ActiveGroupCount;
	uint8	Reserved9;
	union {
		uint16	QpcData;
		struct {
			uint8	QpcBypassEnabled;
			uint8	QpcShift;
		};
	};
	_LARGE_INTEGER	TimeZoneBiasEffectiveStart;
	_LARGE_INTEGER	TimeZoneBiasEffectiveEnd;
	_XSTATE_CONFIGURATION	XState;
};
struct _RTL_HEAP_WALK_ENTRY {
	pointer32<void>	DataAddress;
	uint32	DataSize;
	uint8	OverheadBytes;
	uint8	SegmentIndex;
	uint16	Flags;
	union {
		struct {
			uint32	Settable;
			uint16	TagIndex;
			uint16	AllocatorBackTraceIndex;
			uint32	Reserved[2];
		}	Block;
		struct {
			uint32	CommittedSize;
			uint32	UnCommittedSize;
			pointer32<void>	FirstEntry;
			pointer32<void>	LastEntry;
		}	Segment;
	};
};
struct _FILE_OBJECT;
struct _HEAP_LFH_SUBSEGMENT_CACHE {
	_SLIST_HEADER	SLists[7];
};
struct _HEAP_LFH_BUCKET {
	_HEAP_LFH_SUBSEGMENT_OWNER	State;
	uint32	TotalBlockCount;
	uint32	TotalSubsegmentCount;
	uint32	ReciprocalBlockSize;
	uint8	Shift;
	_RTL_SRWLOCK	AffinityMappingLock;
	uint32	ContentionCount;
	pointer32<uint8>	ProcAffinityMapping;
	pointer32<_HEAP_LFH_AFFINITY_SLOT>	*AffinitySlots;
};
struct _HEAP_LFH_CONTEXT {
	pointer32<void>	BackendCtx;
	_HEAP_SUBALLOCATOR_CALLBACKS	Callbacks;
	_RTL_SRWLOCK	SubsegmentCreationLock;
	uint8	MaxAffinity;
	pointer32<uint8>	AffinityModArray;
	_HEAP_LFH_SUBSEGMENT_CACHE	SubsegmentCache;
	pointer32<_HEAP_LFH_BUCKET>	Buckets[129];
};
struct _SEGMENT_HEAP {
	uint32	TotalReservedPages;
	uint32	TotalCommittedPages;
	uint32	Signature;
	uint32	GlobalFlags;
	uint32	FreeCommittedPages;
	uint32	Interceptor;
	uint16	ProcessHeapListIndex;
	uint16	GlobalLockCount;
	uint32	GlobalLockOwner;
	_RTL_SRWLOCK	LargeMetadataLock;
	_RTL_RB_TREE	LargeAllocMetadata;
	uint32	LargeReservedPages;
	uint32	LargeCommittedPages;
	_RTL_SRWLOCK	SegmentAllocatorLock;
	_LIST_ENTRY	SegmentListHead;
	uint32	SegmentCount;
	_RTL_RB_TREE	FreePageRanges;
	_RTL_SRWLOCK	ContextExtendLock;
	pointer32<uint8>	AllocatedBase;
	pointer32<uint8>	UncommittedBase;
	pointer32<uint8>	ReservedLimit;
	_HEAP_VS_CONTEXT	VsContext;
	_HEAP_LFH_CONTEXT	LfhContext;
};
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
struct _DPH_BLOCK_INFORMATION {
	uint32	StartStamp;
	pointer32<void>	Heap;
	uint32	RequestedSize;
	uint32	ActualSize;
	union {
		_LIST_ENTRY	FreeQueue;
		_SINGLE_LIST_ENTRY	FreePushList;
		uint16	TraceIndex;
	};
	pointer32<void>	StackTrace;
	uint32	EndStamp;
};
struct _THREAD_ENERGY_VALUES {
	uint64	Cycles[2][4];
};
struct _PO_DIAG_STACK_RECORD {
	uint32	StackDepth;
	pointer32<void>	Stack[1];
};
enum _MEMORY_CACHING_TYPE_ORIG {
	MmFrameBufferCached	= 2,
};
struct _RTL_STACK_DATABASE_LOCK {
	_RTL_SRWLOCK	Lock;
};
struct _RTL_STD_LIST_ENTRY {
	pointer32<_RTL_STD_LIST_ENTRY>	Next;
};
struct _RTL_STD_LIST_HEAD {
	pointer32<_RTL_STD_LIST_ENTRY>	Next;
	_RTL_STACK_DATABASE_LOCK	Lock;
};
struct _PAGEFAULT_HISTORY {
};
enum _SECURITY_IMPERSONATION_LEVEL {
	SecurityAnonymous	= 0,
	SecurityIdentification	= 1,
	SecurityImpersonation	= 2,
	SecurityDelegation	= 3,
};
struct _SECURITY_QUALITY_OF_SERVICE {
	uint32	Length;
	_SECURITY_IMPERSONATION_LEVEL	ImpersonationLevel;
	uint8	ContextTrackingMode;
	uint8	EffectiveOnly;
};
struct _LUID {
	uint32	LowPart;
	int32	HighPart;
};
struct _SECURITY_SUBJECT_CONTEXT {
	pointer32<void>	ClientToken;
	_SECURITY_IMPERSONATION_LEVEL	ImpersonationLevel;
	pointer32<void>	PrimaryToken;
	pointer32<void>	ProcessAuditId;
};
struct _LUID_AND_ATTRIBUTES {
	_LUID	Luid;
	uint32	Attributes;
};
struct _INITIAL_PRIVILEGE_SET {
	uint32	PrivilegeCount;
	uint32	Control;
	_LUID_AND_ATTRIBUTES	Privilege[3];
};
struct _PRIVILEGE_SET {
	uint32	PrivilegeCount;
	uint32	Control;
	_LUID_AND_ATTRIBUTES	Privilege[1];
};
struct _ACCESS_STATE {
	_LUID	OperationID;
	uint8	SecurityEvaluated;
	uint8	GenerateAudit;
	uint8	GenerateOnClose;
	uint8	PrivilegesAllocated;
	uint32	Flags;
	uint32	RemainingDesiredAccess;
	uint32	PreviouslyGrantedAccess;
	uint32	OriginalDesiredAccess;
	_SECURITY_SUBJECT_CONTEXT	SubjectSecurityContext;
	pointer32<void>	SecurityDescriptor;
	pointer32<void>	AuxData;
	union {
		_INITIAL_PRIVILEGE_SET	InitialPrivilegeSet;
		_PRIVILEGE_SET	PrivilegeSet;
	}	Privileges;
	uint8	AuditPrivileges;
	_UNICODE_STRING	ObjectName;
	_UNICODE_STRING	ObjectTypeName;
};
struct _IO_SECURITY_CONTEXT {
	pointer32<_SECURITY_QUALITY_OF_SERVICE>	SecurityQos;
	pointer32<_ACCESS_STATE>	AccessState;
	uint32	DesiredAccess;
	uint32	FullCreateOptions;
};
union _CPU_INFO {
	unsigned int	AsUINT32[4];
	struct {
		uint32	Eax;
		uint32	Ebx;
		uint32	Ecx;
		uint32	Edx;
	};
};
struct _TP_CLEANUP_GROUP {
};
enum DISPLAYCONFIG_SCANLINE_ORDERING {
	DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED	= 0,
	DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE	= 1,
	DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED	= 2,
	DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED_UPPERFIELDFIRST	= 2,
	DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED_LOWERFIELDFIRST	= 3,
	DISPLAYCONFIG_SCANLINE_ORDERING_FORCE_UINT32	= -1,
};
struct _PS_TRUSTLET_TKSESSION_ID {
	uint64	SessionId[4];
};
enum _KOBJECTS {
	EventNotificationObject	= 0,
	EventSynchronizationObject	= 1,
	MutantObject	= 2,
	ProcessObject	= 3,
	QueueObject	= 4,
	SemaphoreObject	= 5,
	ThreadObject	= 6,
	GateObject	= 7,
	TimerNotificationObject	= 8,
	TimerSynchronizationObject	= 9,
	Spare2Object	= 10,
	Spare3Object	= 11,
	Spare4Object	= 12,
	Spare5Object	= 13,
	Spare6Object	= 14,
	Spare7Object	= 15,
	Spare8Object	= 16,
	ProfileCallbackObject	= 17,
	ApcObject	= 18,
	DpcObject	= 19,
	DeviceQueueObject	= 20,
	PriQueueObject	= 21,
	InterruptObject	= 22,
	ProfileObject	= 23,
	Timer2NotificationObject	= 24,
	Timer2SynchronizationObject	= 25,
	ThreadedDpcObject	= 26,
	MaximumKernelObject	= 27,
};
struct _PEB_LDR_DATA {
	uint32	Length;
	uint8	Initialized;
	pointer32<void>	SsHandle;
	_LIST_ENTRY	InLoadOrderModuleList;
	_LIST_ENTRY	InMemoryOrderModuleList;
	_LIST_ENTRY	InInitializationOrderModuleList;
	pointer32<void>	EntryInProgress;
	uint8	ShutdownInProgress;
	pointer32<void>	ShutdownThreadId;
};
enum _FS_FILTER_STREAM_FO_NOTIFICATION_TYPE {
	NotifyTypeCreate	= 0,
	NotifyTypeRetired	= 1,
};
struct _FS_FILTER_SECTION_SYNC_OUTPUT {
	uint32	StructureSize;
	uint32	SizeReturned;
	uint32	Flags;
	uint32	DesiredReadAlignment;
};
union _FS_FILTER_PARAMETERS {
	struct {
		pointer32<_LARGE_INTEGER>	EndingOffset;
		pointer32<_ERESOURCE>	*ResourceToRelease;
	}	AcquireForModifiedPageWriter;
	struct {
		pointer32<_ERESOURCE>	ResourceToRelease;
	}	ReleaseForModifiedPageWriter;
	struct {
		_FS_FILTER_SECTION_SYNC_TYPE	SyncType;
		uint32	PageProtection;
		pointer32<_FS_FILTER_SECTION_SYNC_OUTPUT>	OutputInformation;
	}	AcquireForSectionSynchronization;
	struct {
		_FS_FILTER_STREAM_FO_NOTIFICATION_TYPE	NotificationType;
		uint8	SafeToRecurse;
	}	NotifyStreamFileObject;
	struct {
		pointer32<void>	Argument1;
		pointer32<void>	Argument2;
		pointer32<void>	Argument3;
		pointer32<void>	Argument4;
		pointer32<void>	Argument5;
	}	Others;
};
struct _FS_FILTER_CALLBACK_DATA;
enum _PS_PROTECTED_SIGNER {
	PsProtectedSignerNone	= 0,
	PsProtectedSignerAuthenticode	= 1,
	PsProtectedSignerCodeGen	= 2,
	PsProtectedSignerAntimalware	= 3,
	PsProtectedSignerLsa	= 4,
	PsProtectedSignerWindows	= 5,
	PsProtectedSignerWinTcb	= 6,
	PsProtectedSignerMax	= 7,
};
struct _XSTATE_SAVE {
	union {
		struct {
			int64	Reserved1;
			uint32	Reserved2;
			pointer32<_XSTATE_SAVE>	Prev;
			pointer32<_XSAVE_AREA>	Reserved3;
			pointer32<_KTHREAD>	Thread;
			pointer32<void>	Reserved4;
			uint8	Level;
		};
		_XSTATE_CONTEXT	XStateContext;
	};
};
struct _FILE_GET_QUOTA_INFORMATION {
	uint32	NextEntryOffset;
	uint32	SidLength;
	_SID	Sid;
};
enum _PROCESS_VA_TYPE {
	ProcessVAImage	= 0,
	ProcessVASection	= 1,
	ProcessVAPrivate	= 2,
	ProcessVAMax	= 3,
};
struct _CLIENT_ID32 {
	uint32	UniqueProcess;
	uint32	UniqueThread;
};
struct _TRUSTLET_MAILBOX_KEY {
	uint64	SecretValue[2];
};
struct _CURDIR {
	_UNICODE_STRING	DosPath;
	pointer32<void>	Handle;
};
struct _TP_POOL {
};
union _WHEA_REVISION {
	struct {
		uint8	MinorRevision;
		uint8	MajorRevision;
	};
	uint16	AsUSHORT;
};
union _WHEA_ERROR_RECORD_HEADER_FLAGS {
	struct {
		uint32	Recovered:1, PreviousError:1, Simulated:1, Reserved:29;
	};
	uint32	AsULONG;
};
union _WHEA_PERSISTENCE_INFO {
	struct {
		uint64	Signature:16, Length:24, Identifier:16, Attributes:2, DoNotLog:1, Reserved:5;
	};
	uint64	AsULONGLONG;
};
struct _WHEA_ERROR_RECORD_HEADER {
	uint32	Signature;
	_WHEA_REVISION	Revision;
	uint32	SignatureEnd;
	uint16	SectionCount;
	_WHEA_ERROR_SEVERITY	Severity;
	_WHEA_ERROR_RECORD_HEADER_VALIDBITS	ValidBits;
	uint32	Length;
	_WHEA_TIMESTAMP	Timestamp;
	_GUID	PlatformId;
	_GUID	PartitionId;
	_GUID	CreatorId;
	_GUID	NotifyType;
	uint64	RecordId;
	_WHEA_ERROR_RECORD_HEADER_FLAGS	Flags;
	_WHEA_PERSISTENCE_INFO	PersistenceInfo;
	uint8	Reserved[12];
};
struct _LDRP_CSLIST {
	pointer32<_SINGLE_LIST_ENTRY>	Tail;
};
struct _RTL_USER_PROCESS_PARAMETERS {
	uint32	MaximumLength;
	uint32	Length;
	uint32	Flags;
	uint32	DebugFlags;
	pointer32<void>	ConsoleHandle;
	uint32	ConsoleFlags;
	pointer32<void>	StandardInput;
	pointer32<void>	StandardOutput;
	pointer32<void>	StandardError;
	_CURDIR	CurrentDirectory;
	_UNICODE_STRING	DllPath;
	_UNICODE_STRING	ImagePathName;
	_UNICODE_STRING	CommandLine;
	pointer32<void>	Environment;
	uint32	StartingX;
	uint32	StartingY;
	uint32	CountX;
	uint32	CountY;
	uint32	CountCharsX;
	uint32	CountCharsY;
	uint32	FillAttribute;
	uint32	WindowFlags;
	uint32	ShowWindowFlags;
	_UNICODE_STRING	WindowTitle;
	_UNICODE_STRING	DesktopInfo;
	_UNICODE_STRING	ShellInfo;
	_UNICODE_STRING	RuntimeData;
	_RTL_DRIVE_LETTER_CURDIR	CurrentDirectores[32];
	uint32	EnvironmentSize;
	uint32	EnvironmentVersion;
	pointer32<void>	PackageDependencyData;
	uint32	ProcessGroupId;
	uint32	LoaderThreads;
};
struct _INTERLOCK_SEQ {
	union {
		struct {
			uint16	Depth;
			union {
				struct {
					uint16	Hint:15, Lock:1;
				};
				uint16	Hint16;
			};
		};
		int32	Exchg;
	};
};
union _HEAP_BUCKET_COUNTERS {
	struct {
		uint32	TotalBlocks;
		uint32	SubSegmentCounts;
	};
	int64	Aggregate64;
};
struct _HEAP_SUBSEGMENT;
struct _HEAP_LOCAL_SEGMENT_INFO;
struct _HEAP_USERDATA_OFFSETS {
	union {
		struct {
			uint16	FirstAllocationOffset;
			uint16	BlockStride;
		};
		uint32	StrideAndOffset;
	};
};
struct _RTL_BITMAP {
	uint32	SizeOfBitMap;
	pointer32<uint32>	Buffer;
};
struct _HEAP_USERDATA_HEADER;
struct _HEAP_SUBSEGMENT {
	pointer32<_HEAP_LOCAL_SEGMENT_INFO>	LocalInfo;
	pointer32<_HEAP_USERDATA_HEADER>	UserBlocks;
	_SLIST_HEADER	DelayFreeList;
	_INTERLOCK_SEQ	AggregateExchg;
	union {
		struct {
			uint16	BlockSize;
			uint16	Flags;
			uint16	BlockCount;
			uint8	SizeIndex;
			uint8	AffinityIndex;
		};
		uint32	Alignment[2];
	};
	uint32	Lock;
	_SINGLE_LIST_ENTRY	SFreeListEntry;
};
struct _HEAP_LOCAL_SEGMENT_INFO {
	pointer32<_HEAP_LOCAL_DATA>	LocalData;
	pointer32<_HEAP_SUBSEGMENT>	ActiveSubsegment;
	pointer32<_HEAP_SUBSEGMENT>	CachedItems[16];
	_SLIST_HEADER	SListHeader;
	_HEAP_BUCKET_COUNTERS	Counters;
	uint32	LastOpSequence;
	uint16	BucketIndex;
	uint16	LastUsed;
	uint16	NoThrashCount;
};
struct _HEAP_USERDATA_HEADER {
	union {
		_SINGLE_LIST_ENTRY	SFreeListEntry;
		pointer32<_HEAP_SUBSEGMENT>	SubSegment;
	};
	pointer32<void>	Reserved;
	union {
		uint32	SizeIndexAndPadding;
		struct {
			uint8	SizeIndex;
			uint8	GuardPagePresent;
			uint16	PaddingBytes;
		};
	};
	uint32	Signature;
	_HEAP_USERDATA_OFFSETS	EncodedOffsets;
	_RTL_BITMAP	BusyBitmap;
	uint32	BitmapData[1];
};
_HEAP_SUBSEGMENT;
enum _EVENT_TYPE {
	NotificationEvent	= 0,
	SynchronizationEvent	= 1,
};
struct _KENTROPY_TIMING_STATE {
	uint32	EntropyCount;
	uint32	Buffer[64];
	_KDPC	Dpc;
	uint32	LastDeliveredBuffer;
};
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
	int32	e_lfanew;
};
struct _HEAP_PSEUDO_TAG_ENTRY {
	uint32	Allocs;
	uint32	Frees;
	uint32	Size;
};
struct _HEAP;
struct _HEAP_SEGMENT {
	_HEAP_ENTRY	Entry;
	uint32	SegmentSignature;
	uint32	SegmentFlags;
	_LIST_ENTRY	SegmentListEntry;
	pointer32<_HEAP>	Heap;
	pointer32<void>	BaseAddress;
	uint32	NumberOfPages;
	pointer32<_HEAP_ENTRY>	FirstEntry;
	pointer32<_HEAP_ENTRY>	LastValidEntry;
	uint32	NumberOfUnCommittedPages;
	uint32	NumberOfUnCommittedRanges;
	uint16	SegmentAllocatorBackTraceIndex;
	uint16	Reserved;
	_LIST_ENTRY	UCRSegmentList;
};
struct _IO_RESOURCE_REQUIREMENTS_LIST {
	uint32	ListSize;
	_INTERFACE_TYPE	InterfaceType;
	uint32	BusNumber;
	uint32	SlotNumber;
	uint32	Reserved[3];
	uint32	AlternativeLists;
	_IO_RESOURCE_LIST	List[1];
};
struct _LDR_SERVICE_TAG_RECORD {
	pointer32<_LDR_SERVICE_TAG_RECORD>	Next;
	uint32	ServiceTag;
};
struct _HEAP_DESCRIPTOR_KEY {
	union {
		uint16	Key;
		struct {
			uint8	EncodedCommitCount;
			uint8	PageCount;
		};
	};
};
struct _HEAP_PAGE_RANGE_DESCRIPTOR {
	union {
		_RTL_BALANCED_NODE	TreeNode;
		struct {
			uint32	TreeSignature;
			uint16	ExtraPresent:1, Spare0:15;
			uint16	UnusedBytes;
		};
	};
	uint8	RangeFlags;
	uint8	Spare1;
	union {
		_HEAP_DESCRIPTOR_KEY	Key;
		struct {
			uint8	Align;
			union {
				uint8	Offset;
				uint8	Size;
			};
		};
	};
};
struct _ACCESS_REASONS {
	uint32	Data[32];
};
struct _AUX_ACCESS_DATA {
	pointer32<_PRIVILEGE_SET>	PrivilegesUsed;
	_GENERIC_MAPPING	GenericMapping;
	uint32	AccessesToAudit;
	uint32	MaximumAuditMask;
	_GUID	TransactionId;
	pointer32<void>	NewSecurityDescriptor;
	pointer32<void>	ExistingSecurityDescriptor;
	pointer32<void>	ParentSecurityDescriptor;
	void	(*DeRefSecurityDescriptor)(pointer32<void>, pointer32<void>);
	pointer32<void>	SDLock;
	_ACCESS_REASONS	AccessReasons;
	uint8	GenerateStagingEvents;
};
struct BATTERY_REPORTING_SCALE {
	uint32	Granularity;
	uint32	Capacity;
};
struct _KiIoAccessMap {
	uint8	DirectionMap[32];
	uint8	IoMap[8196];
};
struct _KTSS {
	uint16	Backlink;
	uint16	Reserved0;
	uint32	Esp0;
	uint16	Ss0;
	uint16	Reserved1;
	uint32	NotUsed1[4];
	uint32	CR3;
	uint32	Eip;
	uint32	EFlags;
	uint32	Eax;
	uint32	Ecx;
	uint32	Edx;
	uint32	Ebx;
	uint32	Esp;
	uint32	Ebp;
	uint32	Esi;
	uint32	Edi;
	uint16	Es;
	uint16	Reserved2;
	uint16	Cs;
	uint16	Reserved3;
	uint16	Ss;
	uint16	Reserved4;
	uint16	Ds;
	uint16	Reserved5;
	uint16	Fs;
	uint16	Reserved6;
	uint16	Gs;
	uint16	Reserved7;
	uint16	LDT;
	uint16	Reserved8;
	uint16	Flags;
	uint16	IoMapBase;
	_KiIoAccessMap	IoMaps[1];
	uint8	IntDirectionMap[32];
};
enum _LDR_DLL_LOAD_REASON {
	LoadReasonStaticDependency	= 0,
	LoadReasonStaticForwarderDependency	= 1,
	LoadReasonDynamicForwarderDependency	= 2,
	LoadReasonDelayloadDependency	= 3,
	LoadReasonDynamicLoad	= 4,
	LoadReasonAsImageLoad	= 5,
	LoadReasonAsDataLoad	= 6,
	LoadReasonUnknown	= -1,
};
enum _LDR_DDAG_STATE {
	LdrModulesMerged	= -5,
	LdrModulesInitError	= -4,
	LdrModulesSnapError	= -3,
	LdrModulesUnloaded	= -2,
	LdrModulesUnloading	= -1,
	LdrModulesPlaceHolder	= 0,
	LdrModulesMapping	= 1,
	LdrModulesMapped	= 2,
	LdrModulesWaitingForDependencies	= 3,
	LdrModulesSnapping	= 4,
	LdrModulesSnapped	= 5,
	LdrModulesCondensed	= 6,
	LdrModulesReadyToInit	= 7,
	LdrModulesInitializing	= 8,
	LdrModulesReadyToRun	= 9,
};
struct _LDR_DDAG_NODE {
	_LIST_ENTRY	Modules;
	pointer32<_LDR_SERVICE_TAG_RECORD>	ServiceTagList;
	uint32	LoadCount;
	uint32	LoadWhileUnloadingCount;
	uint32	LowestLink;
	_LDRP_CSLIST	Dependencies;
	_LDRP_CSLIST	IncomingDependencies;
	_LDR_DDAG_STATE	State;
	_SINGLE_LIST_ENTRY	CondenseLink;
	uint32	PreorderNumber;
};
struct _LDRP_LOAD_CONTEXT {
};
struct _LDR_DATA_TABLE_ENTRY {
	_LIST_ENTRY	InLoadOrderLinks;
	_LIST_ENTRY	InMemoryOrderLinks;
	_LIST_ENTRY	InInitializationOrderLinks;
	pointer32<void>	DllBase;
	pointer32<void>	EntryPoint;
	uint32	SizeOfImage;
	_UNICODE_STRING	FullDllName;
	_UNICODE_STRING	BaseDllName;
	union {
		uint8	FlagGroup[4];
		uint32	Flags;
		struct {
			uint32	PackagedBinary:1, MarkedForRemoval:1, ImageDll:1, LoadNotificationsSent:1, TelemetryEntryProcessed:1, ProcessStaticImport:1, InLegacyLists:1, InIndexes:1, ShimDll:1, InExceptionTable:1, ReservedFlags1:2, LoadInProgress:1, LoadConfigProcessed:1, EntryProcessed:1, ProtectDelayLoad:1, ReservedFlags3:2, DontCallForThreads:1, ProcessAttachCalled:1, ProcessAttachFailed:1, CorDeferredValidate:1, CorImage:1, DontRelocate:1, CorILOnly:1, ReservedFlags5:3, Redirected:1, ReservedFlags6:2, CompatDatabaseProcessed:1;
		};
	};
	uint16	ObsoleteLoadCount;
	uint16	TlsIndex;
	_LIST_ENTRY	HashLinks;
	uint32	TimeDateStamp;
	pointer32<_ACTIVATION_CONTEXT>	EntryPointActivationContext;
	pointer32<void>	Lock;
	pointer32<_LDR_DDAG_NODE>	DdagNode;
	_LIST_ENTRY	NodeModuleLink;
	pointer32<_LDRP_LOAD_CONTEXT>	LoadContext;
	pointer32<void>	ParentDllBase;
	pointer32<void>	SwitchBackContext;
	_RTL_BALANCED_NODE	BaseAddressIndexNode;
	_RTL_BALANCED_NODE	MappingInfoIndexNode;
	uint32	OriginalBase;
	_LARGE_INTEGER	LoadTime;
	uint32	BaseNameHashValue;
	_LDR_DLL_LOAD_REASON	LoadReason;
	uint32	ImplicitPathOptions;
	uint32	ReferenceCount;
};
struct _EVENT_DESCRIPTOR {
	uint16	Id;
	uint8	Version;
	uint8	Channel;
	uint8	Level;
	uint8	Opcode;
	uint16	Task;
	uint64	Keyword;
};
struct _EVENT_HEADER {
	uint16	Size;
	uint16	HeaderType;
	uint16	Flags;
	uint16	EventProperty;
	uint32	ThreadId;
	uint32	ProcessId;
	_LARGE_INTEGER	TimeStamp;
	_GUID	ProviderId;
	_EVENT_DESCRIPTOR	EventDescriptor;
	union {
		struct {
			uint32	KernelTime;
			uint32	UserTime;
		};
		uint64	ProcessorTime;
	};
	_GUID	ActivityId;
};
struct _ACTIVATION_CONTEXT_DATA {
};
struct _FLS_CALLBACK_INFO {
};
struct _PEB {
	uint8	InheritedAddressSpace;
	uint8	ReadImageFileExecOptions;
	uint8	BeingDebugged;
	union {
		uint8	BitField;
		struct {
			uint8	ImageUsesLargePages:1, IsProtectedProcess:1, IsImageDynamicallyRelocated:1, SkipPatchingUser32Forwarders:1, IsPackagedProcess:1, IsAppContainer:1, IsProtectedProcessLight:1, SpareBits:1;
		};
	};
	pointer32<void>	Mutant;
	pointer32<void>	ImageBaseAddress;
	pointer32<_PEB_LDR_DATA>	Ldr;
	pointer32<_RTL_USER_PROCESS_PARAMETERS>	ProcessParameters;
	pointer32<void>	SubSystemData;
	pointer32<void>	ProcessHeap;
	pointer32<_RTL_CRITICAL_SECTION>	FastPebLock;
	pointer32<void>	AtlThunkSListPtr;
	pointer32<void>	IFEOKey;
	union {
		uint32	CrossProcessFlags;
		struct {
			uint32	ProcessInJob:1, ProcessInitializing:1, ProcessUsingVEH:1, ProcessUsingVCH:1, ProcessUsingFTH:1, ReservedBits0:27;
		};
	};
	union {
		pointer32<void>	KernelCallbackTable;
		pointer32<void>	UserSharedInfoPtr;
	};
	uint32	SystemReserved[1];
	uint32	AtlThunkSListPtr32;
	pointer32<void>	ApiSetMap;
	uint32	TlsExpansionCounter;
	pointer32<void>	TlsBitmap;
	uint32	TlsBitmapBits[2];
	pointer32<void>	ReadOnlySharedMemoryBase;
	pointer32<void>	SparePvoid0;
	pointer32<void>	*ReadOnlyStaticServerData;
	pointer32<void>	AnsiCodePageData;
	pointer32<void>	OemCodePageData;
	pointer32<void>	UnicodeCaseTableData;
	uint32	NumberOfProcessors;
	uint32	NtGlobalFlag;
	_LARGE_INTEGER	CriticalSectionTimeout;
	uint32	HeapSegmentReserve;
	uint32	HeapSegmentCommit;
	uint32	HeapDeCommitTotalFreeThreshold;
	uint32	HeapDeCommitFreeBlockThreshold;
	uint32	NumberOfHeaps;
	uint32	MaximumNumberOfHeaps;
	pointer32<void>	*ProcessHeaps;
	pointer32<void>	GdiSharedHandleTable;
	pointer32<void>	ProcessStarterHelper;
	uint32	GdiDCAttributeList;
	pointer32<_RTL_CRITICAL_SECTION>	LoaderLock;
	uint32	OSMajorVersion;
	uint32	OSMinorVersion;
	uint16	OSBuildNumber;
	uint16	OSCSDVersion;
	uint32	OSPlatformId;
	uint32	ImageSubsystem;
	uint32	ImageSubsystemMajorVersion;
	uint32	ImageSubsystemMinorVersion;
	uint32	ActiveProcessAffinityMask;
	uint32	GdiHandleBuffer[34];
	void	(*PostProcessInitRoutine)();
	pointer32<void>	TlsExpansionBitmap;
	uint32	TlsExpansionBitmapBits[32];
	uint32	SessionId;
	_ULARGE_INTEGER	AppCompatFlags;
	_ULARGE_INTEGER	AppCompatFlagsUser;
	pointer32<void>	pShimData;
	pointer32<void>	AppCompatInfo;
	_UNICODE_STRING	CSDVersion;
	pointer32<_ACTIVATION_CONTEXT_DATA>	ActivationContextData;
	pointer32<_ASSEMBLY_STORAGE_MAP>	ProcessAssemblyStorageMap;
	pointer32<_ACTIVATION_CONTEXT_DATA>	SystemDefaultActivationContextData;
	pointer32<_ASSEMBLY_STORAGE_MAP>	SystemAssemblyStorageMap;
	uint32	MinimumStackCommit;
	pointer32<_FLS_CALLBACK_INFO>	FlsCallback;
	_LIST_ENTRY	FlsListHead;
	pointer32<void>	FlsBitmap;
	uint32	FlsBitmapBits[4];
	uint32	FlsHighIndex;
	pointer32<void>	WerRegistrationData;
	pointer32<void>	WerShipAssertPtr;
	pointer32<void>	pUnused;
	pointer32<void>	pImageHeaderHash;
	union {
		uint32	TracingFlags;
		struct {
			uint32	HeapTracingEnabled:1, CritSecTracingEnabled:1, LibLoaderTracingEnabled:1, SpareTracingBits:29;
		};
	};
	uint64	CsrServerReadOnlySharedMemoryBase;
	uint32	TppWorkerpListLock;
	_LIST_ENTRY	TppWorkerpList;
	pointer32<void>	WaitOnAddressHashTable[128];
};
struct _HEAP_LIST_LOOKUP {
	pointer32<_HEAP_LIST_LOOKUP>	ExtendedLookup;
	uint32	ArraySize;
	uint32	ExtraItem;
	uint32	ItemCount;
	uint32	OutOfRangeItems;
	uint32	BaseIndex;
	pointer32<_LIST_ENTRY>	ListHead;
	pointer32<uint32>	ListsInUseUlong;
	pointer32<_LIST_ENTRY>	*ListHints;
};
union _WHEA_ERROR_RECORD_SECTION_DESCRIPTOR_VALIDBITS {
	struct {
		uint8	FRUId:1, FRUText:1, Reserved:6;
	};
	uint8	AsUCHAR;
};
union _WHEA_ERROR_RECORD_SECTION_DESCRIPTOR_FLAGS {
	struct {
		uint32	Primary:1, ContainmentWarning:1, Reset:1, ThresholdExceeded:1, ResourceNotAvailable:1, LatentError:1, Reserved:26;
	};
	uint32	AsULONG;
};
struct _WHEA_ERROR_RECORD_SECTION_DESCRIPTOR {
	uint32	SectionOffset;
	uint32	SectionLength;
	_WHEA_REVISION	Revision;
	_WHEA_ERROR_RECORD_SECTION_DESCRIPTOR_VALIDBITS	ValidBits;
	uint8	Reserved;
	_WHEA_ERROR_RECORD_SECTION_DESCRIPTOR_FLAGS	Flags;
	_GUID	SectionType;
	_GUID	FRUId;
	_WHEA_ERROR_SEVERITY	SectionSeverity;
	char	FRUText[20];
};
enum _PS_WAKE_REASON {
	PsWakeReasonUser	= 0,
	PsWakeReasonExecutionRequired	= 1,
	PsWakeReasonKernel	= 2,
	PsWakeReasonInstrumentation	= 3,
	PsWakeReasonPreserveProcess	= 4,
	PsMaxWakeReasons	= 5,
};
struct _PERFINFO_GROUPMASK {
	uint32	Masks[8];
};
struct _TRUSTLET_COLLABORATION_ID {
	uint64	Value[2];
};
enum _POWER_STATE_TYPE {
	SystemPowerState	= 0,
	DevicePowerState	= 1,
};
enum _KWAIT_BLOCK_STATE {
	WaitBlockBypassStart	= 0,
	WaitBlockBypassComplete	= 1,
	WaitBlockSuspendBypassStart	= 2,
	WaitBlockSuspendBypassComplete	= 3,
	WaitBlockActive	= 4,
	WaitBlockInactive	= 5,
	WaitBlockSuspended	= 6,
	WaitBlockAllStates	= 7,
};
enum _PS_RESOURCE_TYPE {
	PsResourceNonPagedPool	= 0,
	PsResourcePagedPool	= 1,
	PsResourcePageFile	= 2,
	PsResourceWorkingSet	= 3,
	PsResourceMax	= 4,
};
struct _IO_CLIENT_EXTENSION {
	pointer32<_IO_CLIENT_EXTENSION>	NextExtension;
	pointer32<void>	ClientIdentificationAddress;
};
enum _DEVICE_POWER_STATE {
	PowerDeviceUnspecified	= 0,
	PowerDeviceD0	= 1,
	PowerDeviceD1	= 2,
	PowerDeviceD2	= 3,
	PowerDeviceD3	= 4,
	PowerDeviceMaximum	= 5,
};
union _POWER_STATE {
	_SYSTEM_POWER_STATE	SystemState;
	_DEVICE_POWER_STATE	DeviceState;
};
struct _WHEA_ERROR_RECORD {
	_WHEA_ERROR_RECORD_HEADER	Header;
	_WHEA_ERROR_RECORD_SECTION_DESCRIPTOR	SectionDescriptor[1];
};
struct _POWER_SEQUENCE {
	uint32	SequenceD1;
	uint32	SequenceD2;
	uint32	SequenceD3;
};
struct _HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS {
	union {
		struct {
			uint16	BlockSize;
			uint16	FirstBlockOffset;
		};
		uint32	EncodedData;
	};
};
union _PS_TRUSTLET_ATTRIBUTE_ACCESSRIGHTS {
	struct {
		uint8	Trustlet:1, Ntos:1, WriteHandle:1, ReadHandle:1, Reserved:4;
	};
	uint8	AccessRights;
};
struct _PS_TRUSTLET_ATTRIBUTE_TYPE {
	union {
		struct {
			uint8	Version;
			uint8	DataCount;
			uint8	SemanticType;
			_PS_TRUSTLET_ATTRIBUTE_ACCESSRIGHTS	AccessRights;
		};
		uint32	AttributeType;
	};
};
struct _PS_TRUSTLET_ATTRIBUTE_HEADER {
	_PS_TRUSTLET_ATTRIBUTE_TYPE	AttributeType;
	uint32	InstanceNumber:8, Reserved:24;
};
struct _PS_TRUSTLET_ATTRIBUTE_DATA {
	_PS_TRUSTLET_ATTRIBUTE_HEADER	Header;
	uint64	Data[1];
};
struct _HEAP_LOCK {
	union {
		_RTL_CRITICAL_SECTION	CriticalSection;
	}	Lock;
};
union _LFH_RANDOM_DATA {
	uint8	Bytes[256];
	uint16	Words[128];
	uint64	Quadwords[32];
};
enum _IO_PRIORITY_HINT {
	IoPriorityVeryLow	= 0,
	IoPriorityLow	= 1,
	IoPriorityNormal	= 2,
	IoPriorityHigh	= 3,
	IoPriorityCritical	= 4,
	MaxIoPriorityTypes	= 5,
};
struct _IO_PRIORITY_INFO {
	uint32	Size;
	uint32	ThreadPriority;
	uint32	PagePriority;
	_IO_PRIORITY_HINT	IoPriority;
};
struct _FILE_STANDARD_INFORMATION {
	_LARGE_INTEGER	AllocationSize;
	_LARGE_INTEGER	EndOfFile;
	uint32	NumberOfLinks;
	uint8	DeletePending;
	uint8	Directory;
};
struct _COMPRESSED_DATA_INFO {
	uint16	CompressionFormatAndEngine;
	uint8	CompressionUnitShift;
	uint8	ChunkShift;
	uint8	ClusterShift;
	uint8	Reserved;
	uint16	NumberOfChunks;
	uint32	CompressedChunkSizes[1];
};
struct _HEAP_FREE_ENTRY {
	union {
		_HEAP_ENTRY	HeapEntry;
		_HEAP_UNPACKED_ENTRY	UnpackedEntry;
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
		_HEAP_EXTENDED_ENTRY	ExtendedEntry;
		struct {
			uint16	FunctionIndex;
			uint16	ContextValue;
		};
		struct {
			uint32	InterceptorValue;
			uint16	UnusedBytesLength;
			uint8	EntryOffset;
			uint8	ExtendedBlockSignature;
		};
		struct {
			uint32	Code1;
			union {
				struct {
					uint16	Code2;
					uint8	Code3;
					uint8	Code4;
				};
				uint32	Code234;
			};
		};
		uint64	AgregateCode;
	};
	_LIST_ENTRY	FreeList;
};
struct SYSTEM_POWER_CAPABILITIES {
	uint8	PowerButtonPresent;
	uint8	SleepButtonPresent;
	uint8	LidPresent;
	uint8	SystemS1;
	uint8	SystemS2;
	uint8	SystemS3;
	uint8	SystemS4;
	uint8	SystemS5;
	uint8	HiberFilePresent;
	uint8	FullWake;
	uint8	VideoDimPresent;
	uint8	ApmPresent;
	uint8	UpsPresent;
	uint8	ThermalControl;
	uint8	ProcessorThrottle;
	uint8	ProcessorMinThrottle;
	uint8	ProcessorMaxThrottle;
	uint8	FastSystemS4;
	uint8	Hiberboot;
	uint8	WakeAlarmPresent;
	uint8	AoAc;
	uint8	DiskSpinDown;
	uint8	HiberFileType;
	uint8	AoAcConnectivitySupported;
	uint8	spare3[6];
	uint8	SystemBatteriesPresent;
	uint8	BatteriesAreShortTerm;
	BATTERY_REPORTING_SCALE	BatteryScale[3];
	_SYSTEM_POWER_STATE	AcOnLineWake;
	_SYSTEM_POWER_STATE	SoftLidWake;
	_SYSTEM_POWER_STATE	RtcWake;
	_SYSTEM_POWER_STATE	MinDeviceWakeState;
	_SYSTEM_POWER_STATE	DefaultLowLatencyWake;
};
struct _STRING32 {
	uint16	Length;
	uint16	MaximumLength;
	uint32	Buffer;
};
struct _PEB32 {
	uint8	InheritedAddressSpace;
	uint8	ReadImageFileExecOptions;
	uint8	BeingDebugged;
	union {
		uint8	BitField;
		struct {
			uint8	ImageUsesLargePages:1, IsProtectedProcess:1, IsImageDynamicallyRelocated:1, SkipPatchingUser32Forwarders:1, IsPackagedProcess:1, IsAppContainer:1, IsProtectedProcessLight:1, SpareBits:1;
		};
	};
	uint32	Mutant;
	uint32	ImageBaseAddress;
	uint32	Ldr;
	uint32	ProcessParameters;
	uint32	SubSystemData;
	uint32	ProcessHeap;
	uint32	FastPebLock;
	uint32	AtlThunkSListPtr;
	uint32	IFEOKey;
	union {
		uint32	CrossProcessFlags;
		struct {
			uint32	ProcessInJob:1, ProcessInitializing:1, ProcessUsingVEH:1, ProcessUsingVCH:1, ProcessUsingFTH:1, ReservedBits0:27;
		};
	};
	union {
		uint32	KernelCallbackTable;
		uint32	UserSharedInfoPtr;
	};
	uint32	SystemReserved[1];
	uint32	AtlThunkSListPtr32;
	uint32	ApiSetMap;
	uint32	TlsExpansionCounter;
	uint32	TlsBitmap;
	uint32	TlsBitmapBits[2];
	uint32	ReadOnlySharedMemoryBase;
	uint32	SparePvoid0;
	uint32	ReadOnlyStaticServerData;
	uint32	AnsiCodePageData;
	uint32	OemCodePageData;
	uint32	UnicodeCaseTableData;
	uint32	NumberOfProcessors;
	uint32	NtGlobalFlag;
	_LARGE_INTEGER	CriticalSectionTimeout;
	uint32	HeapSegmentReserve;
	uint32	HeapSegmentCommit;
	uint32	HeapDeCommitTotalFreeThreshold;
	uint32	HeapDeCommitFreeBlockThreshold;
	uint32	NumberOfHeaps;
	uint32	MaximumNumberOfHeaps;
	uint32	ProcessHeaps;
	uint32	GdiSharedHandleTable;
	uint32	ProcessStarterHelper;
	uint32	GdiDCAttributeList;
	uint32	LoaderLock;
	uint32	OSMajorVersion;
	uint32	OSMinorVersion;
	uint16	OSBuildNumber;
	uint16	OSCSDVersion;
	uint32	OSPlatformId;
	uint32	ImageSubsystem;
	uint32	ImageSubsystemMajorVersion;
	uint32	ImageSubsystemMinorVersion;
	uint32	ActiveProcessAffinityMask;
	uint32	GdiHandleBuffer[34];
	uint32	PostProcessInitRoutine;
	uint32	TlsExpansionBitmap;
	uint32	TlsExpansionBitmapBits[32];
	uint32	SessionId;
	_ULARGE_INTEGER	AppCompatFlags;
	_ULARGE_INTEGER	AppCompatFlagsUser;
	uint32	pShimData;
	uint32	AppCompatInfo;
	_STRING32	CSDVersion;
	uint32	ActivationContextData;
	uint32	ProcessAssemblyStorageMap;
	uint32	SystemDefaultActivationContextData;
	uint32	SystemAssemblyStorageMap;
	uint32	MinimumStackCommit;
	uint32	FlsCallback;
	LIST_ENTRY32	FlsListHead;
	uint32	FlsBitmap;
	uint32	FlsBitmapBits[4];
	uint32	FlsHighIndex;
	uint32	WerRegistrationData;
	uint32	WerShipAssertPtr;
	uint32	pUnused;
	uint32	pImageHeaderHash;
	union {
		uint32	TracingFlags;
		struct {
			uint32	HeapTracingEnabled:1, CritSecTracingEnabled:1, LibLoaderTracingEnabled:1, SpareTracingBits:29;
		};
	};
	uint64	CsrServerReadOnlySharedMemoryBase;
	uint32	TppWorkerpListLock;
	LIST_ENTRY32	TppWorkerpList;
	uint32	WaitOnAddressHashTable[128];
};
struct _TEB_ACTIVE_FRAME_CONTEXT {
	uint32	Flags;
	pointer32<char>	FrameName;
};
struct _TEB_ACTIVE_FRAME {
	uint32	Flags;
	pointer32<_TEB_ACTIVE_FRAME>	Previous;
	pointer32<_TEB_ACTIVE_FRAME_CONTEXT>	Context;
};
struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {
	uint8	Type;
	uint8	ShareDisposition;
	uint16	Flags;
	union {
		struct {
			_LARGE_INTEGER	Start;
			uint32	Length;
		}	Generic;
		struct {
			_LARGE_INTEGER	Start;
			uint32	Length;
		}	Port;
		struct {
			uint16	Level;
			uint16	Group;
			uint32	Vector;
			uint32	Affinity;
		}	Interrupt;
		struct {
			union {
				struct {
					uint16	Group;
					uint16	MessageCount;
					uint32	Vector;
					uint32	Affinity;
				}	Raw;
				struct {
					uint16	Level;
					uint16	Group;
					uint32	Vector;
					uint32	Affinity;
				}	Translated;
			};
		}	MessageInterrupt;
		struct {
			_LARGE_INTEGER	Start;
			uint32	Length;
		}	Memory;
		struct {
			uint32	Channel;
			uint32	Port;
			uint32	Reserved1;
		}	Dma;
		struct {
			uint32	Channel;
			uint32	RequestLine;
			uint8	TransferWidth;
			uint8	Reserved1;
			uint8	Reserved2;
			uint8	Reserved3;
		}	DmaV3;
		struct {
			uint32	Data[3];
		}	DevicePrivate;
		struct {
			uint32	Start;
			uint32	Length;
			uint32	Reserved;
		}	BusNumber;
		struct {
			uint32	DataSize;
			uint32	Reserved1;
			uint32	Reserved2;
		}	DeviceSpecificData;
		struct {
			_LARGE_INTEGER	Start;
			uint32	Length40;
		}	Memory40;
		struct {
			_LARGE_INTEGER	Start;
			uint32	Length48;
		}	Memory48;
		struct {
			_LARGE_INTEGER	Start;
			uint32	Length64;
		}	Memory64;
		struct {
			uint8	Class;
			uint8	Type;
			uint8	Reserved1;
			uint8	Reserved2;
			uint32	IdLowPart;
			uint32	IdHighPart;
		}	Connection;
	}	u;
};
struct _CM_PARTIAL_RESOURCE_LIST {
	uint16	Version;
	uint16	Revision;
	uint32	Count;
	_CM_PARTIAL_RESOURCE_DESCRIPTOR	PartialDescriptors[1];
};
struct _HEAP_GLOBAL_APPCOMPAT_FLAGS {
	uint32	SafeInputValidation:1, Padding:1, CommitLFHSubsegments:1;
};
enum _PERFINFO_KERNELMEMORY_USAGE_TYPE {
	PerfInfoMemUsagePfnMetadata	= 0,
	PerfInfoMemUsageMax	= 1,
};
enum ReplacesCorHdrNumericDefines {
	COMIMAGE_FLAGS_ILONLY	= 1,
	COMIMAGE_FLAGS_32BITREQUIRED	= 2,
	COMIMAGE_FLAGS_IL_LIBRARY	= 4,
	COMIMAGE_FLAGS_STRONGNAMESIGNED	= 8,
	COMIMAGE_FLAGS_NATIVE_ENTRYPOINT	= 16,
	COMIMAGE_FLAGS_TRACKDEBUGDATA	= 65536,
	COMIMAGE_FLAGS_32BITPREFERRED	= 131072,
	COR_VERSION_MAJOR_V2	= 2,
	COR_VERSION_MAJOR	= 2,
	COR_VERSION_MINOR	= 5,
	COR_DELETED_NAME_LENGTH	= 8,
	COR_VTABLEGAP_NAME_LENGTH	= 8,
	NATIVE_TYPE_MAX_CB	= 1,
	COR_ILMETHOD_SECT_SMALL_MAX_DATASIZE	= 255,
	IMAGE_COR_MIH_METHODRVA	= 1,
	IMAGE_COR_MIH_EHRVA	= 2,
	IMAGE_COR_MIH_BASICBLOCK	= 8,
	COR_VTABLE_32BIT	= 1,
	COR_VTABLE_64BIT	= 2,
	COR_VTABLE_FROM_UNMANAGED	= 4,
	COR_VTABLE_FROM_UNMANAGED_RETAIN_APPDOMAIN	= 8,
	COR_VTABLE_CALL_MOST_DERIVED	= 16,
	IMAGE_COR_EATJ_THUNK_SIZE	= 32,
	MAX_CLASS_NAME	= 1024,
	MAX_PACKAGE_NAME	= 1024,
};
struct _SEGMENT_HEAP_EXTRA {
	uint16	AllocationTag;
	uint8	InterceptorIndex:4, UserFlags:4;
	uint8	ExtraSizeInUnits;
	pointer32<void>	Settable;
};
struct _RTL_TRACE_DATABASE;
struct _RTL_TRACE_SEGMENT {
	uint32	Magic;
	pointer32<_RTL_TRACE_DATABASE>	Database;
	pointer32<_RTL_TRACE_SEGMENT>	NextSegment;
	uint32	TotalSize;
	pointer32<char>	SegmentStart;
	pointer32<char>	SegmentEnd;
	pointer32<char>	SegmentFree;
};
struct _RTL_TRACE_DATABASE {
	uint32	Magic;
	uint32	Flags;
	uint32	Tag;
	pointer32<_RTL_TRACE_SEGMENT>	SegmentList;
	uint32	MaximumSize;
	uint32	CurrentSize;
	pointer32<void>	Owner;
	_RTL_CRITICAL_SECTION	Lock;
	uint32	NoOfBuckets;
	pointer32<_RTL_TRACE_BLOCK>	*Buckets;
	uint32	(*HashFunction)();
	uint32	NoOfTraces;
	uint32	NoOfHits;
	uint32	HashCounter[16];
};
struct _MAILSLOT_CREATE_PARAMETERS {
	uint32	MailslotQuota;
	uint32	MaximumMessageSize;
	_LARGE_INTEGER	ReadTimeout;
	uint8	TimeoutSpecified;
};
enum _USER_ACTIVITY_PRESENCE {
	PowerUserPresent	= 0,
	PowerUserNotPresent	= 1,
	PowerUserInactive	= 2,
	PowerUserMaximum	= 3,
	PowerUserInvalid	= 3,
};
struct _TP_CALLBACK_INSTANCE {
};
struct _TP_CALLBACK_ENVIRON_V3 {
	uint32	Version;
	pointer32<_TP_POOL>	Pool;
	pointer32<_TP_CLEANUP_GROUP>	CleanupGroup;
	void	(*CleanupGroupCancelCallback)(pointer32<void>, pointer32<void>);
	pointer32<void>	RaceDll;
	pointer32<_ACTIVATION_CONTEXT>	ActivationContext;
	void	(*FinalizationCallback)(pointer32<_TP_CALLBACK_INSTANCE>, pointer32<void>);
	union {
		uint32	Flags;
		struct {
			uint32	LongFunction:1, Persistent:1, Private:30;
		}	s;
	}	u;
	_TP_CALLBACK_PRIORITY	CallbackPriority;
	uint32	Size;
};
struct _DEVICE_CAPABILITIES {
	uint16	Size;
	uint16	Version;
	uint32	DeviceD1:1, DeviceD2:1, LockSupported:1, EjectSupported:1, Removable:1, DockDevice:1, UniqueID:1, SilentInstall:1, RawDeviceOK:1, SurpriseRemovalOK:1, WakeFromD0:1, WakeFromD1:1, WakeFromD2:1, WakeFromD3:1, HardwareDisabled:1, NonDynamic:1, WarmEjectSupported:1, NoDisplayInUI:1, Reserved1:1, WakeFromInterrupt:1, Reserved:12;
	uint32	Address;
	uint32	UINumber;
	_DEVICE_POWER_STATE	DeviceState[7];
	_SYSTEM_POWER_STATE	SystemWake;
	_DEVICE_POWER_STATE	DeviceWake;
	uint32	D1Latency;
	uint32	D2Latency;
	uint32	D3Latency;
};
struct _RTL_STACK_TRACE_ENTRY {
	_RTL_STD_LIST_ENTRY	HashChain;
	uint16	TraceCount:11, BlockDepth:5;
	uint16	IndexHigh;
	uint16	Index;
	uint16	Depth;
	union {
		pointer32<void>	BackTrace[32];
		_SINGLE_LIST_ENTRY	FreeChain;
	};
};
struct _WORK_QUEUE_ITEM {
	_LIST_ENTRY	List;
	void	(*WorkerRoutine)(pointer32<void>);
	pointer32<void>	Parameter;
};
struct _HEAP_VS_SUBSEGMENT {
	_LIST_ENTRY	ListEntry;
	uint64	CommitBitmap;
	_RTL_SRWLOCK	CommitLock;
	uint16	Size;
	uint16	Signature;
};
struct _HEAP_COUNTERS {
	uint32	TotalMemoryReserved;
	uint32	TotalMemoryCommitted;
	uint32	TotalMemoryLargeUCR;
	uint32	TotalSizeInVirtualBlocks;
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
	uint32	InBlockDeccomitSize;
	uint32	HighWatermarkSize;
	uint32	LastPolledSize;
};
enum _PROCESSOR_CACHE_TYPE {
	CacheUnified	= 0,
	CacheInstruction	= 1,
	CacheData	= 2,
	CacheTrace	= 3,
};
struct _CACHE_DESCRIPTOR {
	uint8	Level;
	uint8	Associativity;
	uint16	LineSize;
	uint32	Size;
	_PROCESSOR_CACHE_TYPE	Type;
};
enum _REG_NOTIFY_CLASS {
	RegNtDeleteKey	= 0,
	RegNtPreDeleteKey	= 0,
	RegNtSetValueKey	= 1,
	RegNtPreSetValueKey	= 1,
	RegNtDeleteValueKey	= 2,
	RegNtPreDeleteValueKey	= 2,
	RegNtSetInformationKey	= 3,
	RegNtPreSetInformationKey	= 3,
	RegNtRenameKey	= 4,
	RegNtPreRenameKey	= 4,
	RegNtEnumerateKey	= 5,
	RegNtPreEnumerateKey	= 5,
	RegNtEnumerateValueKey	= 6,
	RegNtPreEnumerateValueKey	= 6,
	RegNtQueryKey	= 7,
	RegNtPreQueryKey	= 7,
	RegNtQueryValueKey	= 8,
	RegNtPreQueryValueKey	= 8,
	RegNtQueryMultipleValueKey	= 9,
	RegNtPreQueryMultipleValueKey	= 9,
	RegNtPreCreateKey	= 10,
	RegNtPostCreateKey	= 11,
	RegNtPreOpenKey	= 12,
	RegNtPostOpenKey	= 13,
	RegNtKeyHandleClose	= 14,
	RegNtPreKeyHandleClose	= 14,
	RegNtPostDeleteKey	= 15,
	RegNtPostSetValueKey	= 16,
	RegNtPostDeleteValueKey	= 17,
	RegNtPostSetInformationKey	= 18,
	RegNtPostRenameKey	= 19,
	RegNtPostEnumerateKey	= 20,
	RegNtPostEnumerateValueKey	= 21,
	RegNtPostQueryKey	= 22,
	RegNtPostQueryValueKey	= 23,
	RegNtPostQueryMultipleValueKey	= 24,
	RegNtPostKeyHandleClose	= 25,
	RegNtPreCreateKeyEx	= 26,
	RegNtPostCreateKeyEx	= 27,
	RegNtPreOpenKeyEx	= 28,
	RegNtPostOpenKeyEx	= 29,
	RegNtPreFlushKey	= 30,
	RegNtPostFlushKey	= 31,
	RegNtPreLoadKey	= 32,
	RegNtPostLoadKey	= 33,
	RegNtPreUnLoadKey	= 34,
	RegNtPostUnLoadKey	= 35,
	RegNtPreQueryKeySecurity	= 36,
	RegNtPostQueryKeySecurity	= 37,
	RegNtPreSetKeySecurity	= 38,
	RegNtPostSetKeySecurity	= 39,
	RegNtCallbackObjectContextCleanup	= 40,
	RegNtPreRestoreKey	= 41,
	RegNtPostRestoreKey	= 42,
	RegNtPreSaveKey	= 43,
	RegNtPostSaveKey	= 44,
	RegNtPreReplaceKey	= 45,
	RegNtPostReplaceKey	= 46,
	RegNtPreQueryKeyName	= 47,
	RegNtPostQueryKeyName	= 48,
	MaxRegNtNotifyClass	= 49,
};
enum _DEVICE_USAGE_NOTIFICATION_TYPE {
	DeviceUsageTypeUndefined	= 0,
	DeviceUsageTypePaging	= 1,
	DeviceUsageTypeHibernation	= 2,
	DeviceUsageTypeDumpFile	= 3,
	DeviceUsageTypeBoot	= 4,
	DeviceUsageTypePostDisplay	= 5,
};
enum _WOW64_SHARED_INFORMATION {
	SharedNtdll32LdrInitializeThunk	= 0,
	SharedNtdll32KiUserExceptionDispatcher	= 1,
	SharedNtdll32KiUserApcDispatcher	= 2,
	SharedNtdll32KiUserCallbackDispatcher	= 3,
	SharedNtdll32RtlUserThreadStart	= 4,
	SharedNtdll32pQueryProcessDebugInformationRemote	= 5,
	SharedNtdll32BaseAddress	= 6,
	SharedNtdll32LdrSystemDllInitBlock	= 7,
	Wow64SharedPageEntriesCount	= 8,
};
enum _WORKING_SET_TYPE {
	WorkingSetTypeUser	= 0,
	WorkingSetTypeSession	= 1,
	WorkingSetTypeSystemTypes	= 2,
	WorkingSetTypeSystemCache	= 2,
	WorkingSetTypePagedPool	= 3,
	WorkingSetTypeSystemPtes	= 4,
	WorkingSetTypeMaximum	= 5,
};
struct _EVENT_HEADER_EXTENDED_DATA_ITEM {
	uint16	Reserved1;
	uint16	ExtType;
	uint16	Linkage:1, Reserved2:15;
	uint16	DataSize;
	uint64	DataPtr;
};
struct _EVENT_RECORD {
	_EVENT_HEADER	EventHeader;
	_ETW_BUFFER_CONTEXT	BufferContext;
	uint16	ExtendedDataCount;
	uint16	UserDataLength;
	pointer32<_EVENT_HEADER_EXTENDED_DATA_ITEM>	ExtendedData;
	pointer32<void>	UserData;
	pointer32<void>	UserContext;
};
struct _RTL_DYNAMIC_HASH_TABLE_CONTEXT {
	pointer32<_LIST_ENTRY>	ChainHead;
	pointer32<_LIST_ENTRY>	PrevLinkage;
	uint32	Signature;
};
enum _JOBOBJECTINFOCLASS {
	JobObjectBasicAccountingInformation	= 1,
	JobObjectBasicLimitInformation	= 2,
	JobObjectBasicProcessIdList	= 3,
	JobObjectBasicUIRestrictions	= 4,
	JobObjectSecurityLimitInformation	= 5,
	JobObjectEndOfJobTimeInformation	= 6,
	JobObjectAssociateCompletionPortInformation	= 7,
	JobObjectBasicAndIoAccountingInformation	= 8,
	JobObjectExtendedLimitInformation	= 9,
	JobObjectJobSetInformation	= 10,
	JobObjectGroupInformation	= 11,
	JobObjectNotificationLimitInformation	= 12,
	JobObjectLimitViolationInformation	= 13,
	JobObjectGroupInformationEx	= 14,
	JobObjectCpuRateControlInformation	= 15,
	JobObjectCompletionFilter	= 16,
	JobObjectCompletionCounter	= 17,
	JobObjectFreezeInformation	= 18,
	JobObjectExtendedAccountingInformation	= 19,
	JobObjectWakeInformation	= 20,
	JobObjectBackgroundInformation	= 21,
	JobObjectSchedulingRankBiasInformation	= 22,
	JobObjectTimerVirtualizationInformation	= 23,
	JobObjectCycleTimeNotification	= 24,
	JobObjectClearEvent	= 25,
	JobObjectInterferenceInformation	= 26,
	JobObjectClearPeakJobMemoryUsed	= 27,
	JobObjectMemoryUsageInformation	= 28,
	JobObjectSharedCommit	= 29,
	JobObjectContainerId	= 30,
	JobObjectIoRateControlInformation	= 31,
	JobObjectReserved1Information	= 18,
	JobObjectReserved2Information	= 19,
	JobObjectReserved3Information	= 20,
	JobObjectReserved4Information	= 21,
	JobObjectReserved5Information	= 22,
	JobObjectReserved6Information	= 23,
	JobObjectReserved7Information	= 24,
	JobObjectReserved8Information	= 25,
	JobObjectReserved9Information	= 26,
	JobObjectReserved10Information	= 27,
	JobObjectReserved11Information	= 28,
	JobObjectReserved12Information	= 29,
	JobObjectReserved13Information	= 30,
	JobObjectReserved14Information	= 31,
	JobObjectNetRateControlInformation	= 32,
	JobObjectNotificationLimitInformation2	= 33,
	JobObjectLimitViolationInformation2	= 34,
	JobObjectCreateSilo	= 35,
	JobObjectSiloBasicInformation	= 36,
	JobObjectSiloRootDirectory	= 37,
	JobObjectServerSiloBasicInformation	= 38,
	JobObjectServerSiloServiceSessionId	= 39,
	JobObjectServerSiloInitialize	= 40,
	JobObjectServerSiloRunningState	= 41,
	MaxJobObjectInfoClass	= 42,
};
struct _HEAP_TUNING_PARAMETERS {
	uint32	CommittThresholdShift;
	uint32	MaxPreCommittThreshold;
};
struct _IO_COMPLETION_CONTEXT {
	pointer32<void>	Port;
	pointer32<void>	Key;
};
struct _CM_FULL_RESOURCE_DESCRIPTOR {
	_INTERFACE_TYPE	InterfaceType;
	uint32	BusNumber;
	_CM_PARTIAL_RESOURCE_LIST	PartialResourceList;
};
enum CPU_VENDORS {
	CPU_NONE	= 0,
	CPU_INTEL	= 1,
	CPU_AMD	= 2,
	CPU_CYRIX	= 3,
	CPU_TRANSMETA	= 4,
	CPU_VIA	= 5,
	CPU_CENTAUR	= 5,
	CPU_RISE	= 6,
	CPU_UNKNOWN	= 7,
};
struct _MM_DRIVER_VERIFIER_DATA {
	uint32	Level;
	uint32	RaiseIrqls;
	uint32	AcquireSpinLocks;
	uint32	SynchronizeExecutions;
	uint32	AllocationsAttempted;
	uint32	AllocationsSucceeded;
	uint32	AllocationsSucceededSpecialPool;
	uint32	AllocationsWithNoTag;
	uint32	TrimRequests;
	uint32	Trims;
	uint32	AllocationsFailed;
	uint32	AllocationsFailedDeliberately;
	uint32	Loads;
	uint32	Unloads;
	uint32	UnTrackedPool;
	uint32	UserTrims;
	uint32	CurrentPagedPoolAllocations;
	uint32	CurrentNonPagedPoolAllocations;
	uint32	PeakPagedPoolAllocations;
	uint32	PeakNonPagedPoolAllocations;
	uint32	PagedBytes;
	uint32	NonPagedBytes;
	uint32	PeakPagedBytes;
	uint32	PeakNonPagedBytes;
	uint32	BurstAllocationsFailedDeliberately;
	uint32	SessionTrims;
	uint32	OptionChanges;
	uint32	VerifyMode;
	_UNICODE_STRING	PreviousBucketName;
	uint32	ExecutePoolTypes;
	uint32	ExecutePageProtections;
	uint32	ExecutePageMappings;
	uint32	ExecuteWriteSections;
	uint32	SectionAlignmentFailures;
};
struct _LFH_HEAP {
	_RTL_SRWLOCK	Lock;
	_LIST_ENTRY	SubSegmentZones;
	pointer32<void>	Heap;
	pointer32<void>	NextSegmentInfoArrayAddress;
	pointer32<void>	FirstUncommittedAddress;
	pointer32<void>	ReservedAddressLimit;
	uint32	SegmentCreate;
	uint32	SegmentDelete;
	uint32	MinimumCacheDepth;
	uint32	CacheShiftThreshold;
	uint32	SizeInCache;
	_HEAP_BUCKET_RUN_INFO	RunInfo;
	_USER_MEMORY_CACHE_ENTRY	UserBlockCache[12];
	_HEAP_LFH_MEM_POLICIES	MemoryPolicies;
	_HEAP_BUCKET	Buckets[129];
	pointer32<_HEAP_LOCAL_SEGMENT_INFO>	SegmentInfoArrays[129];
	pointer32<_HEAP_LOCAL_SEGMENT_INFO>	AffinitizedInfoArrays[129];
	pointer32<_SEGMENT_HEAP>	SegmentAllocator;
	_HEAP_LOCAL_DATA	LocalData[1];
};
enum _FSINFOCLASS {
	FileFsVolumeInformation	= 1,
	FileFsLabelInformation	= 2,
	FileFsSizeInformation	= 3,
	FileFsDeviceInformation	= 4,
	FileFsAttributeInformation	= 5,
	FileFsControlInformation	= 6,
	FileFsFullSizeInformation	= 7,
	FileFsObjectIdInformation	= 8,
	FileFsDriverPathInformation	= 9,
	FileFsVolumeFlagsInformation	= 10,
	FileFsSectorSizeInformation	= 11,
	FileFsDataCopyInformation	= 12,
	FileFsMetadataSizeInformation	= 13,
	FileFsMaximumInformation	= 14,
};
enum _DEVICE_RELATION_TYPE {
	BusRelations	= 0,
	EjectionRelations	= 1,
	PowerRelations	= 2,
	RemovalRelations	= 3,
	TargetDeviceRelation	= 4,
	SingleBusRelations	= 5,
	TransportRelations	= 6,
};
struct _NAMED_PIPE_CREATE_PARAMETERS {
	uint32	NamedPipeType;
	uint32	ReadMode;
	uint32	CompletionMode;
	uint32	MaximumInstances;
	uint32	InboundQuota;
	uint32	OutboundQuota;
	_LARGE_INTEGER	DefaultTimeout;
	uint8	TimeoutSpecified;
};
struct _HEAP_LARGE_ALLOC_DATA {
	_RTL_BALANCED_NODE	TreeNode;
	union {
		uint32	VirtualAddress;
		struct {
			uint32	UnusedBytes:16;
			uint32	ExtraPresent:1, Spare:11, AllocatedPages:20;
		};
	};
};
union _HEAP_LFH_ONDEMAND_POINTER {
	struct {
		uint16	Invalid:1, AllocationInProgress:1, Spare0:14;
		uint16	UsageData;
	};
	pointer32<void>	AllBits;
};
struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME {
	pointer32<_RTL_ACTIVATION_CONTEXT_STACK_FRAME>	Previous;
	pointer32<_ACTIVATION_CONTEXT>	ActivationContext;
	uint32	Flags;
};
struct _ACTIVATION_CONTEXT_STACK {
	pointer32<_RTL_ACTIVATION_CONTEXT_STACK_FRAME>	ActiveFrame;
	_LIST_ENTRY	FrameListCache;
	uint32	Flags;
	uint32	NextCookieSequenceNumber;
	uint32	StackId;
};
struct _HEAP_TAG_ENTRY {
	uint32	Allocs;
	uint32	Frees;
	uint32	Size;
	uint16	TagIndex;
	uint16	CreatorBackTraceIndex;
	wchar_t	TagName[24];
};
struct _IOP_IRP_STACK_PROFILER {
	uint32	Profile[20];
	uint32	TotalIrps;
};
enum _PERFINFO_MM_STAT {
	PerfInfoMMStatNotUsed	= 0,
	PerfInfoMMStatAggregatePageCombine	= 1,
	PerfInfoMMStatIterationPageCombine	= 2,
	PerfInfoMMStatMax	= 3,
};
struct _CM_RESOURCE_LIST {
	uint32	Count;
	_CM_FULL_RESOURCE_DESCRIPTOR	List[1];
};
struct _SECTION_OBJECT_POINTERS {
	pointer32<void>	DataSectionObject;
	pointer32<void>	SharedCacheMap;
	pointer32<void>	ImageSectionObject;
};
struct _HEAP_UCR_DESCRIPTOR {
	_LIST_ENTRY	ListEntry;
	_LIST_ENTRY	SegmentEntry;
	pointer32<void>	Address;
	uint32	Size;
};
enum _KWAIT_STATE {
	WaitInProgress	= 0,
	WaitCommitted	= 1,
	WaitAborted	= 2,
	WaitSuspendInProgress	= 3,
	WaitSuspended	= 4,
	WaitResumeInProgress	= 5,
	WaitResumeAborted	= 6,
	WaitFirstSuspendState	= 3,
	WaitLastSuspendState	= 6,
	MaximumWaitState	= 7,
};
struct _GDI_TEB_BATCH {
	uint32	Offset:31, HasRenderingCommand:1;
	uint32	HDC;
	uint32	Buffer[310];
};
enum _MEMORY_CACHING_TYPE {
	MmNonCached	= 0,
	MmCached	= 1,
	MmWriteCombined	= 2,
	MmHardwareCoherentCached	= 3,
	MmNonCachedUnordered	= 4,
	MmUSWCCached	= 5,
	MmMaximumCacheType	= 6,
	MmNotMapped	= -1,
};
struct _HEAP {
	union {
		_HEAP_SEGMENT	Segment;
		struct {
			_HEAP_ENTRY	Entry;
			uint32	SegmentSignature;
			uint32	SegmentFlags;
			_LIST_ENTRY	SegmentListEntry;
			pointer32<_HEAP>	Heap;
			pointer32<void>	BaseAddress;
			uint32	NumberOfPages;
			pointer32<_HEAP_ENTRY>	FirstEntry;
			pointer32<_HEAP_ENTRY>	LastValidEntry;
			uint32	NumberOfUnCommittedPages;
			uint32	NumberOfUnCommittedRanges;
			uint16	SegmentAllocatorBackTraceIndex;
			uint16	Reserved;
			_LIST_ENTRY	UCRSegmentList;
		};
	};
	uint32	Flags;
	uint32	ForceFlags;
	uint32	CompatibilityFlags;
	uint32	EncodeFlagMask;
	_HEAP_ENTRY	Encoding;
	uint32	Interceptor;
	uint32	VirtualMemoryThreshold;
	uint32	Signature;
	uint32	SegmentReserve;
	uint32	SegmentCommit;
	uint32	DeCommitFreeBlockThreshold;
	uint32	DeCommitTotalFreeThreshold;
	uint32	TotalFreeSize;
	uint32	MaximumAllocationSize;
	uint16	ProcessHeapsListIndex;
	uint16	HeaderValidateLength;
	pointer32<void>	HeaderValidateCopy;
	uint16	NextAvailableTagIndex;
	uint16	MaximumTagIndex;
	pointer32<_HEAP_TAG_ENTRY>	TagEntries;
	_LIST_ENTRY	UCRList;
	uint32	AlignRound;
	uint32	AlignMask;
	_LIST_ENTRY	VirtualAllocdBlocks;
	_LIST_ENTRY	SegmentList;
	uint16	AllocatorBackTraceIndex;
	uint32	NonDedicatedListLength;
	pointer32<void>	BlocksIndex;
	pointer32<void>	UCRIndex;
	pointer32<_HEAP_PSEUDO_TAG_ENTRY>	PseudoTagEntries;
	_LIST_ENTRY	FreeLists;
	pointer32<_HEAP_LOCK>	LockVariable;
	int32	(*CommitRoutine)();
	pointer32<void>	FrontEndHeap;
	uint16	FrontHeapLockCount;
	uint8	FrontEndHeapType;
	uint8	RequestedFrontEndHeapType;
	pointer32<uint16>	FrontEndHeapUsageData;
	uint16	FrontEndHeapMaximumIndex;
	uint8	FrontEndHeapStatusBitmap[257];
	_HEAP_COUNTERS	Counters;
	_HEAP_TUNING_PARAMETERS	TuningParameters;
};
enum _SECURITY_OPERATION_CODE {
	SetSecurityDescriptor	= 0,
	QuerySecurityDescriptor	= 1,
	DeleteSecurityDescriptor	= 2,
	AssignSecurityDescriptor	= 3,
};
struct _HEAP_LFH_UNUSED_BYTES_INFO {
	union {
		struct {
			uint16	UnusedBytes:14, ExtraPresent:1, OneByteUnused:1;
		};
		uint8	Bytes[2];
	};
};
struct _HEAP_PAGE_SEGMENT {
	_LIST_ENTRY	ListEntry;
	uint32	Signature;
};
struct _STACK_TRACE_DATABASE {
	union {
		char	Reserved[56];
		_RTL_STACK_DATABASE_LOCK	Lock;
	};
	pointer32<void>	Reserved2;
	uint32	PeakHashCollisionListLength;
	pointer32<void>	LowerMemoryStart;
	uint8	PreCommitted;
	uint8	DumpInProgress;
	pointer32<void>	CommitBase;
	pointer32<void>	CurrentLowerCommitLimit;
	pointer32<void>	CurrentUpperCommitLimit;
	pointer32<char>	NextFreeLowerMemory;
	pointer32<char>	NextFreeUpperMemory;
	uint32	NumberOfEntriesLookedUp;
	uint32	NumberOfEntriesAdded;
	pointer32<_RTL_STACK_TRACE_ENTRY>	*EntryIndexArray;
	uint32	NumberOfEntriesAllocated;
	uint32	NumberOfEntriesAvailable;
	uint32	NumberOfAllocationFailures;
	_SLIST_HEADER	FreeLists[32];
	uint32	NumberOfBuckets;
	_RTL_STD_LIST_HEAD	Buckets[1];
};
union _HEAP_LFH_SUBSEGMENT_DELAY_FREE {
	struct {
		uint32	DelayFree:1, Count:31;
	};
	pointer32<void>	AllBits;
};
struct _HEAP_LFH_SUBSEGMENT {
	union {
		_LIST_ENTRY	ListEntry;
		_SINGLE_LIST_ENTRY	Link;
	};
	union {
		pointer32<_HEAP_LFH_SUBSEGMENT_OWNER>	Owner;
		_HEAP_LFH_SUBSEGMENT_DELAY_FREE	DelayFree;
	};
	_RTL_SRWLOCK	CommitLock;
	union {
		struct {
			uint16	FreeCount;
			uint16	BlockCount;
		};
		int16	InterlockedShort;
		int32	InterlockedLong;
	};
	uint16	FreeHint;
	uint8	Location;
	uint8	Spare;
	_HEAP_LFH_SUBSEGMENT_ENCODED_OFFSETS	BlockOffsets;
	uint8	CommitUnitShift;
	uint8	CommitUnitCount;
	uint16	CommitStateOffset;
	uint32	BlockBitmap[1];
};
struct _KDPC_LIST {
	_SINGLE_LIST_ENTRY	ListHead;
	pointer32<_SINGLE_LIST_ENTRY>	LastEntry;
};
struct _KDPC_DATA {
	_KDPC_LIST	DpcList;
	uint32	DpcLock;
	int32	DpcQueueDepth;
	uint32	DpcCount;
	pointer32<_KDPC>	ActiveDpc;
};
enum _KSPIN_LOCK_QUEUE_NUMBER {
	LockQueueUnusedSpare0	= 0,
	LockQueueUnusedSpare1	= 1,
	LockQueueUnusedSpare2	= 2,
	LockQueueUnusedSpare3	= 3,
	LockQueueVacbLock	= 4,
	LockQueueMasterLock	= 5,
	LockQueueNonPagedPoolLock	= 6,
	LockQueueIoCancelLock	= 7,
	LockQueueWorkQueueLock	= 8,
	LockQueueIoVpbLock	= 9,
	LockQueueIoDatabaseLock	= 10,
	LockQueueIoCompletionLock	= 11,
	LockQueueNtfsStructLock	= 12,
	LockQueueAfdWorkQueueLock	= 13,
	LockQueueBcbLock	= 14,
	LockQueueUnusedSpare15	= 15,
	LockQueueUnusedSpare16	= 16,
	LockQueueMaximumLock	= 17,
};
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
struct _EXT_SET_PARAMETERS_V0 {
	uint32	Version;
	uint32	Reserved;
	int64	NoWakeTolerance;
};
struct _EPROCESS_QUOTA_BLOCK {
};
struct _TEB {
	_NT_TIB	NtTib;
	pointer32<void>	EnvironmentPointer;
	_CLIENT_ID	ClientId;
	pointer32<void>	ActiveRpcHandle;
	pointer32<void>	ThreadLocalStoragePointer;
	pointer32<_PEB>	ProcessEnvironmentBlock;
	uint32	LastErrorValue;
	uint32	CountOfOwnedCriticalSections;
	pointer32<void>	CsrClientThread;
	pointer32<void>	Win32ThreadInfo;
	uint32	User32Reserved[26];
	uint32	UserReserved[5];
	pointer32<void>	WOW32Reserved;
	uint32	CurrentLocale;
	uint32	FpSoftwareStatusRegister;
	pointer32<void>	ReservedForDebuggerInstrumentation[16];
	pointer32<void>	SystemReserved1[38];
	int32	ExceptionCode;
	pointer32<_ACTIVATION_CONTEXT_STACK>	ActivationContextStackPointer;
	uint32	InstrumentationCallbackSp;
	uint32	InstrumentationCallbackPreviousPc;
	uint32	InstrumentationCallbackPreviousSp;
	uint8	InstrumentationCallbackDisabled;
	uint8	SpareBytes[23];
	uint32	TxFsContext;
	_GDI_TEB_BATCH	GdiTebBatch;
	_CLIENT_ID	RealClientId;
	pointer32<void>	GdiCachedProcessHandle;
	uint32	GdiClientPID;
	uint32	GdiClientTID;
	pointer32<void>	GdiThreadLocalInfo;
	uint32	Win32ClientInfo[62];
	pointer32<void>	glDispatchTable[233];
	uint32	glReserved1[29];
	pointer32<void>	glReserved2;
	pointer32<void>	glSectionInfo;
	pointer32<void>	glSection;
	pointer32<void>	glTable;
	pointer32<void>	glCurrentRC;
	pointer32<void>	glContext;
	uint32	LastStatusValue;
	_UNICODE_STRING	StaticUnicodeString;
	wchar_t	StaticUnicodeBuffer[261];
	pointer32<void>	DeallocationStack;
	pointer32<void>	TlsSlots[64];
	_LIST_ENTRY	TlsLinks;
	pointer32<void>	Vdm;
	pointer32<void>	ReservedForNtRpc;
	pointer32<void>	DbgSsReserved[2];
	uint32	HardErrorMode;
	pointer32<void>	Instrumentation[9];
	_GUID	ActivityId;
	pointer32<void>	SubProcessTag;
	pointer32<void>	PerflibData;
	pointer32<void>	EtwTraceData;
	pointer32<void>	WinSockData;
	uint32	GdiBatchCount;
	union {
		_PROCESSOR_NUMBER	CurrentIdealProcessor;
		uint32	IdealProcessorValue;
		struct {
			uint8	ReservedPad0;
			uint8	ReservedPad1;
			uint8	ReservedPad2;
			uint8	IdealProcessor;
		};
	};
	uint32	GuaranteedStackBytes;
	pointer32<void>	ReservedForPerf;
	pointer32<void>	ReservedForOle;
	uint32	WaitingOnLoaderLock;
	pointer32<void>	SavedPriorityState;
	uint32	ReservedForCodeCoverage;
	pointer32<void>	ThreadPoolData;
	pointer32<void>	*TlsExpansionSlots;
	uint32	MuiGeneration;
	uint32	IsImpersonating;
	pointer32<void>	NlsCache;
	pointer32<void>	pShimData;
	uint16	HeapVirtualAffinity;
	uint16	LowFragHeapDataSlot;
	pointer32<void>	CurrentTransactionHandle;
	pointer32<_TEB_ACTIVE_FRAME>	ActiveFrame;
	pointer32<void>	FlsData;
	pointer32<void>	PreferredLanguages;
	pointer32<void>	UserPrefLanguages;
	pointer32<void>	MergedPrefLanguages;
	uint32	MuiImpersonation;
	union {
		uint16	CrossTebFlags;
		uint16	SpareCrossTebBits:16;
	};
	union {
		uint16	SameTebFlags;
		struct {
			uint16	SafeThunkCall:1, InDebugPrint:1, HasFiberData:1, SkipThreadAttach:1, WerInShipAssertCode:1, RanProcessInit:1, ClonedThread:1, SuppressDebugMsg:1, DisableUserStackWalk:1, RtlExceptionAttached:1, InitialThread:1, SessionAware:1, LoadOwner:1, LoaderWorker:1, SpareSameTebBits:2;
		};
	};
	pointer32<void>	TxnScopeEnterCallback;
	pointer32<void>	TxnScopeExitCallback;
	pointer32<void>	TxnScopeContext;
	uint32	LockCount;
	int32	WowTebOffset;
	pointer32<void>	ResourceRetValue;
	pointer32<void>	ReservedForWdf;
	uint64	ReservedForCrt;
	_GUID	EffectiveContainerId;
};
struct _EXCEPTION_POINTERS {
	pointer32<_EXCEPTION_RECORD>	ExceptionRecord;
	pointer32<_CONTEXT>	ContextRecord;
};
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
struct _FILESYSTEM_DISK_COUNTERS {
	uint64	FsBytesRead;
	uint64	FsBytesWritten;
};
struct _RTL_DYNAMIC_HASH_TABLE_ENTRY {
	_LIST_ENTRY	Linkage;
	uint32	Signature;
};
struct _RTL_DYNAMIC_HASH_TABLE_ENUMERATOR {
	union {
		_RTL_DYNAMIC_HASH_TABLE_ENTRY	HashEntry;
		pointer32<_LIST_ENTRY>	CurEntry;
	};
	pointer32<_LIST_ENTRY>	ChainHead;
	uint32	BucketIndex;
};
struct _KTIMER_TABLE_ENTRY {
	uint32	Lock;
	_LIST_ENTRY	Entry;
	_ULARGE_INTEGER	Time;
};
struct _KTIMER_TABLE {
	pointer32<_KTIMER>	TimerExpiry[16];
	_KTIMER_TABLE_ENTRY	TimerEntries[256];
};
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
struct _GDI_TEB_BATCH32 {
	uint32	Offset:31, HasRenderingCommand:1;
	uint32	HDC;
	uint32	Buffer[310];
};
struct _TEB32 {
	_NT_TIB32	NtTib;
	uint32	EnvironmentPointer;
	_CLIENT_ID32	ClientId;
	uint32	ActiveRpcHandle;
	uint32	ThreadLocalStoragePointer;
	uint32	ProcessEnvironmentBlock;
	uint32	LastErrorValue;
	uint32	CountOfOwnedCriticalSections;
	uint32	CsrClientThread;
	uint32	Win32ThreadInfo;
	uint32	User32Reserved[26];
	uint32	UserReserved[5];
	uint32	WOW32Reserved;
	uint32	CurrentLocale;
	uint32	FpSoftwareStatusRegister;
	uint32	ReservedForDebuggerInstrumentation[16];
	uint32	SystemReserved1[38];
	int32	ExceptionCode;
	uint32	ActivationContextStackPointer;
	uint32	InstrumentationCallbackSp;
	uint32	InstrumentationCallbackPreviousPc;
	uint32	InstrumentationCallbackPreviousSp;
	uint8	InstrumentationCallbackDisabled;
	uint8	SpareBytes[23];
	uint32	TxFsContext;
	_GDI_TEB_BATCH32	GdiTebBatch;
	_CLIENT_ID32	RealClientId;
	uint32	GdiCachedProcessHandle;
	uint32	GdiClientPID;
	uint32	GdiClientTID;
	uint32	GdiThreadLocalInfo;
	uint32	Win32ClientInfo[62];
	uint32	glDispatchTable[233];
	uint32	glReserved1[29];
	uint32	glReserved2;
	uint32	glSectionInfo;
	uint32	glSection;
	uint32	glTable;
	uint32	glCurrentRC;
	uint32	glContext;
	uint32	LastStatusValue;
	_STRING32	StaticUnicodeString;
	wchar_t	StaticUnicodeBuffer[261];
	uint32	DeallocationStack;
	uint32	TlsSlots[64];
	LIST_ENTRY32	TlsLinks;
	uint32	Vdm;
	uint32	ReservedForNtRpc;
	uint32	DbgSsReserved[2];
	uint32	HardErrorMode;
	uint32	Instrumentation[9];
	_GUID	ActivityId;
	uint32	SubProcessTag;
	uint32	PerflibData;
	uint32	EtwTraceData;
	uint32	WinSockData;
	uint32	GdiBatchCount;
	union {
		_PROCESSOR_NUMBER	CurrentIdealProcessor;
		uint32	IdealProcessorValue;
		struct {
			uint8	ReservedPad0;
			uint8	ReservedPad1;
			uint8	ReservedPad2;
			uint8	IdealProcessor;
		};
	};
	uint32	GuaranteedStackBytes;
	uint32	ReservedForPerf;
	uint32	ReservedForOle;
	uint32	WaitingOnLoaderLock;
	uint32	SavedPriorityState;
	uint32	ReservedForCodeCoverage;
	uint32	ThreadPoolData;
	uint32	TlsExpansionSlots;
	uint32	MuiGeneration;
	uint32	IsImpersonating;
	uint32	NlsCache;
	uint32	pShimData;
	uint16	HeapVirtualAffinity;
	uint16	LowFragHeapDataSlot;
	uint32	CurrentTransactionHandle;
	uint32	ActiveFrame;
	uint32	FlsData;
	uint32	PreferredLanguages;
	uint32	UserPrefLanguages;
	uint32	MergedPrefLanguages;
	uint32	MuiImpersonation;
	union {
		uint16	CrossTebFlags;
		uint16	SpareCrossTebBits:16;
	};
	union {
		uint16	SameTebFlags;
		struct {
			uint16	SafeThunkCall:1, InDebugPrint:1, HasFiberData:1, SkipThreadAttach:1, WerInShipAssertCode:1, RanProcessInit:1, ClonedThread:1, SuppressDebugMsg:1, DisableUserStackWalk:1, RtlExceptionAttached:1, InitialThread:1, SessionAware:1, LoadOwner:1, LoaderWorker:1, SpareSameTebBits:2;
		};
	};
	uint32	TxnScopeEnterCallback;
	uint32	TxnScopeExitCallback;
	uint32	TxnScopeContext;
	uint32	LockCount;
	int32	WowTebOffset;
	uint32	ResourceRetValue;
	uint32	ReservedForWdf;
	uint64	ReservedForCrt;
	_GUID	EffectiveContainerId;
};
enum _PF_FILE_ACCESS_TYPE {
	PfFileAccessTypeRead	= 0,
	PfFileAccessTypeWrite	= 1,
	PfFileAccessTypeMax	= 2,
};
struct _RTL_SPARSE_BITMAP_RANGE {
	union {
		struct {
			uint32	Lock;
			_RTL_BITMAP	RangeBitmap;
		};
		_SINGLE_LIST_ENTRY	Next;
	};
};
enum _KHETERO_CPU_POLICY {
	KHeteroCpuPolicyAll	= 0,
	KHeteroCpuPolicyLarge	= 1,
	KHeteroCpuPolicyLargeOrIdle	= 2,
	KHeteroCpuPolicySmall	= 3,
	KHeteroCpuPolicySmallOrIdle	= 4,
	KHeteroCpuPolicyDynamic	= 5,
	KHeteroCpuPolicyStaticMax	= 5,
	KHeteroCpuPolicyBiasedSmall	= 6,
	KHeteroCpuPolicyBiasedLarge	= 7,
	KHeteroCpuPolicyDefault	= 8,
	KHeteroCpuPolicyMax	= 9,
};
struct _PS_TRUSTLET_CREATE_ATTRIBUTES {
	uint64	TrustletIdentity;
	_PS_TRUSTLET_ATTRIBUTE_DATA	Attributes[1];
};
struct _PPM_SELECTION_DEPENDENCY {
	uint32	Processor;
	_PPM_SELECTION_MENU	Menu;
};
enum _PP_NPAGED_LOOKASIDE_NUMBER {
	LookasideSmallIrpList	= 0,
	LookasideMediumIrpList	= 1,
	LookasideLargeIrpList	= 2,
	LookasideMdlList	= 3,
	LookasideCreateInfoList	= 4,
	LookasideNameBufferList	= 5,
	LookasideTwilightList	= 6,
	LookasideCompletionList	= 7,
	LookasideScratchBufferList	= 8,
	LookasideMaximumList	= 9,
};
struct _PF_KERNEL_GLOBALS {
	uint64	AccessBufferAgeThreshold;
	_EX_RUNDOWN_REF	AccessBufferRef;
	_KEVENT	AccessBufferExistsEvent;
	uint32	AccessBufferMax;
	_SLIST_HEADER	AccessBufferList;
	int32	StreamSequenceNumber;
	uint32	Flags;
	int32	ScenarioPrefetchCount;
};
enum _PROCESS_SECTION_TYPE {
	ProcessSectionData	= 0,
	ProcessSectionImage	= 1,
	ProcessSectionImageNx	= 2,
	ProcessSectionPagefileBacked	= 3,
	ProcessSectionMax	= 4,
};
enum _DEVICE_WAKE_DEPTH {
	DeviceWakeDepthNotWakeable	= 0,
	DeviceWakeDepthD0	= 1,
	DeviceWakeDepthD1	= 2,
	DeviceWakeDepthD2	= 3,
	DeviceWakeDepthD3hot	= 4,
	DeviceWakeDepthD3cold	= 5,
	DeviceWakeDepthMaximum	= 6,
};
struct _SCSI_REQUEST_BLOCK {
};
struct _RTL_DYNAMIC_HASH_TABLE {
	uint32	Flags;
	uint32	Shift;
	uint32	TableSize;
	uint32	Pivot;
	uint32	DivisorMask;
	uint32	NumEntries;
	uint32	NonEmptyBuckets;
	uint32	NumEnumerators;
	pointer32<void>	Directory;
};
struct _FILE_BASIC_INFORMATION {
	_LARGE_INTEGER	CreationTime;
	_LARGE_INTEGER	LastAccessTime;
	_LARGE_INTEGER	LastWriteTime;
	_LARGE_INTEGER	ChangeTime;
	uint32	FileAttributes;
};
enum _KTHREAD_TAG {
	KThreadTagNone	= 0,
	KThreadTagMediaBuffering	= 1,
	KThreadTagMax	= 2,
};
enum _MODE {
	KernelMode	= 0,
	UserMode	= 1,
	MaximumMode	= 2,
};
struct _RTL_SPARSE_BITMAP_CTX {
	uint32	Lock;
	pointer32<_RTL_SPARSE_BITMAP_RANGE>	*BitmapRanges;
	_RTL_BITMAP	RangeArrayCommitStatus;
	pointer32<void>	(*AllocateRoutine)(uint32);
	void	(*FreeRoutine)(pointer32<void>);
	uint32	RangeCount;
	uint32	RangeIndexLimit;
	uint32	BitsPerRange;
	uint32	RangeCountMax;
	uint32	RangeMetadataOffset;
	uint32	MetadataSizePerBit;
	uint32	DefaultBitsSet:1, SparseRangeArray:1, NoInternalLocking:1, SpareFlags:29;
};
enum _EX_BALANCE_OBJECT {
	ExTimerExpiration	= 0,
	ExThreadSetManagerEvent	= 1,
	ExThreadReaperEvent	= 2,
	ExMaximumBalanceObject	= 3,
};
struct _KSTACK_CONTROL {
	uint32	StackBase;
	union {
		uint32	ActualLimit;
		uint32	StackExpansion:1;
	};
	pointer32<_KTRAP_FRAME>	PreviousTrapFrame;
	pointer32<void>	PreviousExceptionList;
	_KERNEL_STACK_SEGMENT	Previous;
};
enum _TRACE_INFORMATION_CLASS {
	TraceIdClass	= 0,
	TraceHandleClass	= 1,
	TraceEnableFlagsClass	= 2,
	TraceEnableLevelClass	= 3,
	GlobalLoggerHandleClass	= 4,
	EventLoggerHandleClass	= 5,
	AllLoggerHandlesClass	= 6,
	TraceHandleByNameClass	= 7,
	LoggerEventsLostClass	= 8,
	TraceSessionSettingsClass	= 9,
	LoggerEventsLoggedClass	= 10,
	DiskIoNotifyRoutinesClass	= 11,
	TraceInformationClassReserved1	= 12,
	AllPossibleNotifyRoutinesClass	= 12,
	FltIoNotifyRoutinesClass	= 13,
	TraceInformationClassReserved2	= 14,
	WdfNotifyRoutinesClass	= 15,
	MaxTraceInformationClass	= 16,
};
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
enum _KPROCESS_STATE {
	ProcessInMemory	= 0,
	ProcessOutOfMemory	= 1,
	ProcessInTransition	= 2,
	ProcessOutTransition	= 3,
	ProcessInSwap	= 4,
	ProcessOutSwap	= 5,
	ProcessRetryOutSwap	= 6,
	ProcessAllSwapStates	= 7,
};
enum SE_WS_APPX_SIGNATURE_ORIGIN {
	SE_WS_APPX_SIGNATURE_ORIGIN_NOT_VALIDATED	= 0,
	SE_WS_APPX_SIGNATURE_ORIGIN_UNKNOWN	= 1,
	SE_WS_APPX_SIGNATURE_ORIGIN_APPSTORE	= 2,
	SE_WS_APPX_SIGNATURE_ORIGIN_WINDOWS	= 3,
	SE_WS_APPX_SIGNATURE_ORIGIN_ENTERPRISE	= 4,
};
enum LSA_FOREST_TRUST_RECORD_TYPE {
	ForestTrustTopLevelName	= 0,
	ForestTrustTopLevelNameEx	= 1,
	ForestTrustDomainInfo	= 2,
	ForestTrustRecordTypeLast	= 2,
};
struct _FILE_NETWORK_OPEN_INFORMATION {
	_LARGE_INTEGER	CreationTime;
	_LARGE_INTEGER	LastAccessTime;
	_LARGE_INTEGER	LastWriteTime;
	_LARGE_INTEGER	ChangeTime;
	_LARGE_INTEGER	AllocationSize;
	_LARGE_INTEGER	EndOfFile;
	uint32	FileAttributes;
};
enum JOB_OBJECT_IO_RATE_CONTROL_FLAGS {
	JOB_OBJECT_IO_RATE_CONTROL_ENABLE	= 1,
	JOB_OBJECT_IO_RATE_CONTROL_VALID_FLAGS	= 1,
};
struct _PROCESSOR_PROFILE_CONTROL_AREA {
	_PEBS_DS_SAVE_AREA	PebsDsSaveArea;
};
