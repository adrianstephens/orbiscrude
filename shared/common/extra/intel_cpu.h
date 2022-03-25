#ifndef INTEL_CPU_H
#define INTEL_CPU_H

#include "base/defs.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace iso {

inline void cpuid(int info[4], int type, int idx = 0) {
#ifdef _MSC_VER
	__cpuidex(info, type, idx);
#else
	__cpuid_count(type, idx, info[0], info[1], info[2], info[3]);
#endif
}

template<uint32 N> struct _cpu_id { protected: _cpu_id() { cpuid((int*)this, N); } };

struct cpu_id0 : _cpu_id<0> {
	uint32	max_type;
	uint8	ident[12];
};

struct cpu_features : _cpu_id<1> {
	uint32	SteppingID:4, Model:4, Family:4, Type:2, :2, ModelEx:4, FamilyEx:8, :3;
	uint32	Brand:8, CLFLUSHsize:8, LogProcs:8, APIC:8;
	uint32	SSE3:1, :2, MONITOR:1, CPLQDS:1, VME:1, SME:1, EISS:1, TM:1, SSSE3:1, L1:1, :1, FMA256:1, CMPXCHG16B:1, xTPR_update:1, PerfDebug:1,
			:2, DCA:1, SSE4_1:1, SSE4_2:1, x2APIC:1, MOVBE:1, POPCNT:1, :1, AES:1, XSAVE:1, OSXSAVE:1, AVE256:1, :3;
	uint32	x87:1, VMEe:1, DE:1, PSE:1, TSC:1, MSR:1, PAE:1, MCE:1, CX8:1, APICe:1, :1, SEP:1, MTRR:1, PGE:1, MCA:1,
			CMOV:1, PAT:1, PSE36:1, PSN:1, CLFSH:1, :1, DS:1, ACPI:1, MMX:1, FXSR:1, SSE:1, SSE2:1, SS:1, HTT:1, TMe:1, :1, PBE:1;
};

struct cache_information {
	uint32	CacheType:5, CacheLevel:3, SelfInitializing:1, FullyAssociative:1, :4, MaxThreads:12, MaxIDs:6;
	uint32	SystemCoherencyLineSize:12, PhysicalPartitions:10, Associativity:10;
	uint32	Sets;
	uint32	_;
	cache_information(int n) { cpuid((int*)this, n, 4); }
};

//2		cache information
//5		information about the monitor feature (see _mm_monitor). 
//6		information about the digital temperature and power management. 
//0x0A	information obtained by monitoring the architectural performance. 

struct cpu_extended : _cpu_id<0x80000000> {
	uint32	max_ext;
	uint32	_[3];
};

struct cpu_extended1 : _cpu_id<0x80000001> {
	uint32	ext_signature;
	uint32	_;
	uint32	LAHF64:1, CmpLegacy:1, SVM:1, ExtApicSpace:1, AltMovCr8:1, LZCNT:1, SSE4A:1, MisalignedSSE:1, PREFETCH:1, :3, SKINIT:1, :19;
	uint32	:11, SYSCALL64:1, :8, ExecuteDisable:1, :1, MMXext:1, :2, FFXSR:1, GBpage:1, RDTSCP:1, :1, AMD64:1, _3DnowExt:1, _3Dnow:1;
};

struct cpu_brand {
	char	brand[48];
	cpu_brand() {
		cpuid((int*)(brand +  0), 0x80000002);
		cpuid((int*)(brand + 16), 0x80000003);
		cpuid((int*)(brand + 32), 0x80000004);
	}
};
struct cpu_L1 : _cpu_id<0x80000005> {};
struct cpu_L2 : _cpu_id<0x80000006> {
	uint32	_[2];
	uint32	LineSize:8, :4, Associativity:4, CacheSize1K:16;
	uint32	__;
};
struct cpu_APM : _cpu_id<0x80000007> {};
struct cpu_address : _cpu_id<0x80000008> {
	uint32	PhysicalBits:8, VirtualBits:8, _:16;
	uint32	__[3];
};
struct cpu_address2 : _cpu_id<0x8000000A> {
	uint32	SVMrev:8, _:24;
	uint32	NASID;
	uint32	__;
	uint32	NestedPaging:1, LBRvisualisation:1, :30;
};
struct cpu_TLB_1G : _cpu_id<0x80000019> {
	uint32	id[4];
};
struct cpu_extended_0x1a : _cpu_id<0x8000001a> {
	uint32	FP128:1, MOVU:1, :30;
	uint32	__[3];
};

} // namespace iso

#endif
