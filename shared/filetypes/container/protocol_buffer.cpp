#include "iso/iso_files.h"
#include "comms/leb128.h"

using namespace iso;

namespace protocol_buffers {

struct FieldDescriptor {
	enum Type {
		TYPE_DOUBLE		= 1,	// double, exactly eight bytes on the wire.
		TYPE_FLOAT		= 2,	// float, exactly four bytes on the wire.
		TYPE_INT64		= 3,	// int64, varint on the wire.  Negative numbers take 10 bytes.  Use TYPE_SINT64 if negative values are likely.
		TYPE_UINT64		= 4,	// uint64, varint on the wire.
		TYPE_INT32		= 5,	// int32, varint on the wire.  Negative numbers take 10 bytes.  Use TYPE_SINT32 if negative values are likely.
		TYPE_FIXED64	= 6,	// uint64, exactly eight bytes on the wire.
		TYPE_FIXED32	= 7,	// uint32, exactly four bytes on the wire.
		TYPE_BOOL		= 8,	// bool, varint on the wire.
		TYPE_STRING		= 9,	// UTF-8 text.
		TYPE_GROUP		= 10,	// Tag-delimited message.  Deprecated.
		TYPE_MESSAGE	= 11,	// Length-delimited message.
		TYPE_BYTES		= 12,	// Arbitrary byte array.
		TYPE_UINT32		= 13,	// uint32, varint on the wire
		TYPE_ENUM		= 14,	// Enum, varint on the wire
		TYPE_SFIXED32	= 15,	// int32, exactly four bytes on the wire
		TYPE_SFIXED64	= 16,	// int64, exactly eight bytes on the wire
		TYPE_SINT32		= 17,	// int32, ZigZag-encoded varint on the wire
		TYPE_SINT64		= 18,	// int64, ZigZag-encoded varint on the wire
		MAX_TYPE		= 18,	// Constant useful for defining lookup tablesindexed by Type.
	};

	enum CppType {
		CPPTYPE_INT32	= 1,	// TYPE_INT32, TYPE_SINT32, TYPE_SFIXED32
		CPPTYPE_INT64	= 2,	// TYPE_INT64, TYPE_SINT64, TYPE_SFIXED64
		CPPTYPE_UINT32	= 3,	// TYPE_UINT32, TYPE_FIXED32
		CPPTYPE_UINT64	= 4,	// TYPE_UINT64, TYPE_FIXED64
		CPPTYPE_DOUBLE	= 5,	// TYPE_DOUBLE
		CPPTYPE_FLOAT	= 6,	// TYPE_FLOAT
		CPPTYPE_BOOL	= 7,	// TYPE_BOOL
		CPPTYPE_ENUM	= 8,	// TYPE_ENUM
		CPPTYPE_STRING	= 9,	// TYPE_STRING, TYPE_BYTES
		CPPTYPE_MESSAGE	= 10,	// TYPE_MESSAGE, TYPE_GROUP
		MAX_CPPTYPE		= 10,	// Constant useful for defining lookup tables indexed by CppType.
	};

	enum Label {
		LABEL_OPTIONAL	= 1,
		LABEL_REQUIRED	= 2,
		LABEL_REPEATED	= 3,
		MAX_LABEL		= 3,
	};
    
	static const int kMaxNumber = (1 << 29) - 1;
    static const int kFirstReservedNumber = 19000;
    static const int kLastReservedNumber = 19999;

    FieldDescriptor() {}
};

enum WIRE {
	WIRETYPE_VARINT				= 0,
	WIRETYPE_FIXED64			= 1,
	WIRETYPE_LENGTH_DELIMITED	= 2,
	WIRETYPE_START_GROUP		= 3,
	WIRETYPE_END_GROUP			= 4,
	WIRETYPE_FIXED32			= 5,
};

template<typename T> struct pbmeta;

template<typename T, typename TL> bool parse_typelist(T &t, const_memory_block data, const TL&) {
	return false;
}

template<typename T> struct zigzag {
	typedef	uint_for_t<T>	U;
	U	u;
	static	U	encode(T t)		{ return (U(t) << 1) ^ U(t >> (BIT_COUNT<T> - 1)); }
	static	T	decode(U u)		{ return T((u >> 1) ^ -T(u & 1)); }
	operator T()	const		{ return decode(u); }
};
template<typename T> struct T_isint<zigzag<T>>	: T_isint<T>	{};

struct WireMessage {
	struct TAG {
		uint32	type:3, field:29;
	};

	TAG						tag;
	uint64					i;
	const_memory_block_own	data;
	
	WireMessage() {}
	WireMessage(WireMessage &&b) : tag(b.tag), i(b.i), data(move(b.data)) {}

	bool	read(istream_ref file) {
		data.clear();
		if (read_leb128(file, tag)) {
			switch (tag.type) {
				case WIRETYPE_VARINT:
					return read_leb128(file, i);
				case WIRETYPE_FIXED64:
					return file.read(i);
				case WIRETYPE_LENGTH_DELIMITED:
					data = file.get_block(get_leb128<uint32>(file));
					return !!data;
				case WIRETYPE_FIXED32:
					return file.read((uint32&)i);
				default:
					ISO_ASSERT(0);
					break;
			}
		}
		return false;
	}

	template<typename T> enable_if_t<is_num<T>, bool>	get(T& t) const {
		switch (tag.type) {
			case WIRETYPE_VARINT:
			case WIRETYPE_FIXED64:
			case WIRETYPE_FIXED32:
				t = (T&)i;
				return true;
			default:
				return false;
		}
	}
//	template<typename T> bool	get(zigzag<T>& t) const {
//		return get(t.u);
//	}
	template<typename T> enable_if_t<!is_num<T>, bool>	get(T& t) const {
		return parse_typelist(t, data, typename pbmeta<T>::type());
	}

	bool	get(bool& t) const {
		switch (tag.type) {
			case WIRETYPE_VARINT:
			case WIRETYPE_FIXED64:
			case WIRETYPE_FIXED32:
				t = !!i;
				return true;
			default:
				return false;
		}
	}
	bool	get(string& s) const {
		if (tag.type == WIRETYPE_LENGTH_DELIMITED) {
			s = make_range<char>(data);
			return true;
		}
		return false;
	}

	template<typename T> bool	get(optional<T> &o) const {
		T	t;
		if (!get(t))
			return false;
		o = t;
		return true;
	}
	/*
	double			get_double	()	{ return reinterpret_cast<double&>(i); }
	float			get_float	()	{ return reinterpret_cast<float&>(i); }
	int64			get_int64	()	{ return (int64)i; }
	uint64			get_uint64	()	{ return i; }
	int32			get_int32	()	{ return (int32)i; }
	uint64			get_fixed64	()	{ return i; }
	uint32			get_fixed32	()	{ return (uint32)i; }
	bool			get_bool	()	{ return i != 0; }
	string			get_string	()	{ return make_range<char>(data); }
//	group			get_group	()	{ return reinterpret_cast<double&>(i); }
	memory_reader	get_message	()	{ return memory_reader(data); }
	const_memory_block	get_bytes	()	{ return data; }
	uint32			get_uint32	()	{ return (uint32)i; }
//	enum			get_enum	()	{ return reinterpret_cast<double&>(i); }
	int32			get_sfixed32()	{ return (int32)i; }
	int64			get_sfixed64()	{ return (int64)i; }
	int32			get_sint32	()	{ return zigzag<int32>::decode(i); }
	int64			get_sint64	()	{ return zigzag<int64>::decode(i); }
	*/
};

template<int I, typename T, T t> struct pbfield;

template<int I, typename T, typename C, T C::* F> struct pbfield<I, T C::*, F> {
	static bool	read(C *c, const WireMessage &m) {
		return I == m.tag.field && m.get(c->*F);
	}
};

#define PB_FIELD(I, C, F)	pbfield<I, decltype(&C::F), &C::F>

template<typename T, typename...F> bool parse_typelist(T &t, const_memory_block data, const type_list<F...>&) {
	WireMessage		m;
	memory_reader	file(data);
	while (m.read(file)) {
		bool	b		= false;
		bool	b2[]	= {(b = b || F::read(&t, m))...};
		//if (!b)
		//	return false;
	}
	return true;
}

#if 0
message TracePacket {
	optional uint64 timestamp = 8;
	optional uint32 timestamp_clock_id = 58;
	
	oneof data {
		ProcessTree process_tree = 2;
		ProcessStats process_stats = 9;
		InodeFileMap inode_file_map = 4;
		ChromeEventBundle chrome_events = 5;
		ClockSnapshot clock_snapshot = 6;
		SysStats sys_stats = 7;
		TrackEvent track_event = 11;
		TraceConfig trace_config = 33;
		FtraceStats ftrace_stats = 34;
		TraceStats trace_stats = 35;
		ProfilePacket profile_packet = 37;
		StreamingAllocation streaming_allocation = 74;
		StreamingFree streaming_free = 75;
		BatteryCounters battery = 38;
		PowerRails power_rails = 40;
		AndroidLogPacket android_log = 39;
		SystemInfo system_info = 45;
		Trigger trigger = 46;
		PackagesList packages_list = 47;
		ChromeBenchmarkMetadata chrome_benchmark_metadata = 48;
		PerfettoMetatrace perfetto_metatrace = 49;
		ChromeMetadataPacket chrome_metadata = 51;
		GpuCounterEvent gpu_counter_event = 52;
		GpuRenderStageEvent gpu_render_stage_event = 53;
		StreamingProfilePacket streaming_profile_packet = 54;
		HeapGraph heap_graph = 56;
		GraphicsFrameEvent graphics_frame_event = 57;
		VulkanMemoryEvent vulkan_memory_event = 62;
		GpuLog gpu_log = 63;
		VulkanApiEvent vulkan_api_event = 65;
		PerfSample perf_sample = 66;
		CpuInfo cpu_info = 67;
		SmapsPacket smaps_packet = 68;
		TracingServiceEvent service_event = 69;
		InitialDisplayState initial_display_state = 70;
		GpuMemTotalEvent gpu_mem_total_event = 71;
		MemoryTrackerSnapshot memory_tracker_snapshot = 73;
		FrameTimelineEvent frame_timeline_event = 76;
		AndroidEnergyEstimationBreakdown android_energy_estimation_breakdown = 77;
		UiState ui_state = 78;
		ProfiledFrameSymbols profiled_frame_symbols = 55;
		ModuleSymbols module_symbols = 61;
		DeobfuscationMapping deobfuscation_mapping = 64;
		TrackDescriptor track_descriptor = 60;
		ProcessDescriptor process_descriptor = 43;
		ThreadDescriptor thread_descriptor = 44;
		FtraceEventBundle ftrace_events = 1;
		bytes synchronization_marker = 36;
		bytes compressed_packets = 50;
		ExtensionDescriptor extension_descriptor = 72;
		TestEvent for_testing = 900;
	}

	oneof optional_trusted_uid {
		int32 trusted_uid = 3;
	};
	
	oneof optional_trusted_packet_sequence_id {
		uint32 trusted_packet_sequence_id = 10;
	}

	optional InternedData interned_data = 12;

	enum SequenceFlags {
		SEQ_UNSPECIFIED = 0;
		SEQ_INCREMENTAL_STATE_CLEARED = 1;
		SEQ_NEEDS_INCREMENTAL_STATE = 2;
	};

	optional uint32 sequence_flags = 13;
	optional bool incremental_state_cleared = 41;
	optional TracePacketDefaults trace_packet_defaults = 59;
	optional bool previous_packet_dropped = 42;
}

message Trace {
	repeated TracePacket packet = 1;
}
#endif


//message Timestamp {
//	int64 seconds = 1;
//	int32 nanos = 2;
//}

struct timestamp {
	int64 seconds;
	int32 nanos;
};

template<> struct pbmeta<timestamp> : T_type<type_list<
	PB_FIELD(1, timestamp, seconds),
	PB_FIELD(2, timestamp, nanos)
>> {};

struct InternedData	{};
struct TracePacketDefaults	{};

struct TracePacket {
	optional<uint64> timestamp;
	optional<uint32> timestamp_clock_id;
	/*
	union {
		ProcessTree						 process_tree;
		ProcessStats					 process_stats;
		InodeFileMap					 inode_file_map;
		ChromeEventBundle				 chrome_events;
		ClockSnapshot					 clock_snapshot;
		SysStats						 sys_stats;
		TrackEvent						 track_event;
		TraceConfig						 trace_config;
		FtraceStats						 ftrace_stats;
		TraceStats						 trace_stats;
		ProfilePacket					 profile_packet;
		StreamingAllocation				 streaming_allocation;
		StreamingFree					 streaming_free;
		BatteryCounters					 battery;
		PowerRails						 power_rails;
		AndroidLogPacket				 android_log;
		SystemInfo						 system_info;
		Trigger							 trigger;
		PackagesList					 packages_list;
		ChromeBenchmarkMetadata			 chrome_benchmark_metadata;
		PerfettoMetatrace				 perfetto_metatrace;
		ChromeMetadataPacket			 chrome_metadata;
		GpuCounterEvent					 gpu_counter_event;
		GpuRenderStageEvent				 gpu_render_stage_event;
		StreamingProfilePacket			 streaming_profile_packet;
		HeapGraph						 heap_graph;
		GraphicsFrameEvent				 graphics_frame_event;
		VulkanMemoryEvent				 vulkan_memory_event;
		GpuLog							 gpu_log;
		VulkanApiEvent					 vulkan_api_event;
		PerfSample						 perf_sample;
		CpuInfo							 cpu_info;
		SmapsPacket						 smaps_packet;
		TracingServiceEvent				 service_event;
		InitialDisplayState				 initial_display_state;
		GpuMemTotalEvent				 gpu_mem_total_event;
		MemoryTrackerSnapshot			 memory_tracker_snapshot;
		FrameTimelineEvent				 frame_timeline_event;
		AndroidEnergyEstimationBreakdown android_energy_estimation_breakdown;
		UiState							 ui_state;
		ProfiledFrameSymbols			 profiled_frame_symbols;
		ModuleSymbols					 module_symbols;
		DeobfuscationMapping			 deobfuscation_mapping;
		TrackDescriptor					 track_descriptor;
		ProcessDescriptor				 process_descriptor;
		ThreadDescriptor				 thread_descriptor;
		FtraceEventBundle				 ftrace_events;
		bytes							 synchronization_marker;
		bytes							 compressed_packets;
		ExtensionDescriptor				 extension_descriptor;
		TestEvent						 for_testing;
	};
	*/
	union {
		int32 trusted_uid;
	};

	union {
		uint32 trusted_packet_sequence_id;
	};

	optional<InternedData> interned_data;

	enum SequenceFlags { SEQ_UNSPECIFIED, SEQ_INCREMENTAL_STATE_CLEARED, SEQ_NEEDS_INCREMENTAL_STATE };

	optional<uint32>			  sequence_flags;
	optional<bool>				  incremental_state_cleared;
	optional<TracePacketDefaults> trace_packet_defaults;
	optional<bool>				  previous_packet_dropped;
};

template<> struct pbmeta<TracePacket> : T_type<type_list<
	PB_FIELD(8, TracePacket, timestamp),
	PB_FIELD(58, TracePacket, timestamp_clock_id),
	PB_FIELD(3, TracePacket, trusted_uid),
	PB_FIELD(10, TracePacket, trusted_packet_sequence_id),
	PB_FIELD(12, TracePacket, interned_data),
	PB_FIELD(13, TracePacket, sequence_flags),
	PB_FIELD(41, TracePacket, incremental_state_cleared),
	PB_FIELD(59, TracePacket, trace_packet_defaults),
	PB_FIELD(42, TracePacket, previous_packet_dropped)
>> {};

template<> struct pbmeta<TracePacketDefaults> : T_type<type_list<>> {};
template<> struct pbmeta<InternedData> : T_type<type_list<>> {};

ISO_DEFUSERCOMPV(TracePacket,
	timestamp,
	timestamp_clock_id,
	trusted_uid,
	trusted_packet_sequence_id,
	interned_data,
	sequence_flags,
	incremental_state_cleared,
	trace_packet_defaults,
	previous_packet_dropped);
ISO_DEFUSER(TracePacketDefaults, void);
ISO_DEFUSER(InternedData, void);

}	// namespace pro

class ProtocolBufferFileHandler : public FileHandler {
	const char*	GetExt() override { return "pb";	}
	const char* GetDescription() override { return "Google ProtocolBuffer"; }

	ISO_ptr<void>	ParseMessage(tag id, istream_ref file) {
		using namespace protocol_buffers;
		ISO_ptr<anything>	p(id);

		WireMessage			m;
		while (m.read(file)) {
			auto	id = to_string(m.tag.field);
			switch (m.tag.type) {
				case WIRETYPE_VARINT:
				case WIRETYPE_FIXED64:
					p->Append(ISO_ptr<uint64>(id, m.i));
					break;

				case WIRETYPE_LENGTH_DELIMITED:
					p->Append(ISO::MakePtr(id, malloc_block(m.data)));
					break;

				case WIRETYPE_FIXED32:
					p->Append(ISO_ptr<uint32>(id, m.i));
					break;
			}
		}

		return p;
	}


	ISO_ptr<void> Read(tag id, istream_ref file) override {
		using namespace protocol_buffers;

		ISO_ptr<anything>	p(id);
		WireMessage			m;
		while (m.read(file)) {
			auto	id = to_string(m.tag.field);
			switch (m.tag.type) {
				case WIRETYPE_VARINT:
				case WIRETYPE_FIXED64:
					p->Append(ISO_ptr<uint64>(id, m.i));
					break;

				case WIRETYPE_LENGTH_DELIMITED: {
					TracePacket	trace_packet;
					m.get(trace_packet);
					//p->Append(ParseMessage(id, m.get_message()));
					//p->Append(ISO::MakePtr(id, m.data));
					p->Append(ISO::MakePtr(id, trace_packet));
					break;
				}

				case WIRETYPE_FIXED32:
					p->Append(ISO_ptr<uint32>(id, m.i));
					break;
			}
		}
		return p;
	}
} protocolbuffer;

class PerfettoTraceFileHandler : public ProtocolBufferFileHandler {
	const char*	GetExt() override { return "pftrace";	}
} perfetto_trace;