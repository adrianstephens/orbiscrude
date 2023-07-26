#include "iso/iso_files.h"
#include "dalvik.h"

//-----------------------------------------------------------------------------
//	dalvik - android execution engine
//-----------------------------------------------------------------------------

using namespace iso;

template<typename T, typename P> struct param_container : T {
	P			p;
	template<typename...A> param_container(P &&p, A&&... a) : T(forward<A>(a)...), p(forward<P>(p)) {}
	auto	begin()			{ return make_param_iterator(static_begin(*(T*)this), p); }
	auto	end()			{ return make_param_iterator(static_end(*(T*)this), p); }
	auto	begin()	const	{ return make_param_iterator(static_begin(*(const T*)this), p); }
	auto	end()	const	{ return make_param_iterator(static_end(*(const T*)this), p); }
};

template<typename T, typename P>	auto	get(param_element<dynamic_array<T>&, P> &a)				{ return with_param(a.t, a.p); }


struct DEXFile;

ISO_DEFSAME(dalvik::uleb128, uint32);

template<typename T, typename C> struct ISO::def<param_element<embedded_array<T, C>&, const DEXFile*> > : ISO::VirtualT2<param_element<embedded_array<T, C>&, const DEXFile*> > {
	typedef	param_element<embedded_array<T, C>&, const DEXFile*>	A;
	static uint32			Count(A &a)				{ return uint32(a.t.size());	}
	static ISO::Browser2	Index(A &a, int i)		{ return ISO::MakeBrowser(param_element<const T&, const DEXFile*>(a.t[i], a.p));	}
	static tag2				GetName(A &a, int i)	{ return __GetName(a.t[i], a.p);	}
};

template<typename T, typename C> struct ISO::def<param_element<dalvik::offset<T, C>&, const DEXFile*> > : ISO::VirtualT2<param_element<dalvik::offset<T, C>&, const DEXFile*> > {
	typedef	param_element<dalvik::offset<T, C>&, const DEXFile*>	A;
	ISO::Browser2	Deref(A &a)	{
		if (auto *p = a.t.get(a.p))
			return ISO::MakeBrowser(make_param_element(*p, a.p));
		return ISO::Browser2();
	}
};

template<typename T, typename C> struct ISO::def<param_element<dalvik::index<T, C>&, const DEXFile*> > : ISO::VirtualT2<param_element<dalvik::index<T, C>&, const DEXFile*> > {
	typedef	param_element<dalvik::index<T, C>&, const DEXFile*>	A;
	ISO::Browser2	Deref(A &a)	{
		if (a.t)
			return ISO::MakeBrowser(make_param_element(a.p->get(a.t), a.p));
		return ISO::Browser2();
	}
};


ISO_DEFUSERCOMPPV(dalvik::type_id_item,		const DEXFile*,descriptor);
ISO_DEFUSERCOMPPV(dalvik::proto_id_item,	const DEXFile*,shorty, return_type, parameters);
ISO_DEFUSERCOMPPV(dalvik::field_id_item,	const DEXFile*,klass, type, name);
ISO_DEFUSERCOMPPV(dalvik::method_id_item,	const DEXFile*,klass, proto, name);

ISO_DEFUSERCOMPV(dalvik::encoded_value,			byte);
ISO_DEFUSERCOMPV(dalvik::DEXheader,				signature);
ISO_DEFUSERCOMPPV(dalvik::annotation_element,	const DEXFile*,value);
ISO_DEFUSERCOMPPV(dalvik::annotation_item,		const DEXFile*,annotation);
ISO_DEFUSERCOMPPV(dalvik::annotations_directory_item::field_annotation,		const DEXFile*,field, annotations);
ISO_DEFUSERCOMPPV(dalvik::annotations_directory_item::method_annotation,	const DEXFile*,method,annotations);
ISO_DEFUSERCOMPPV(dalvik::annotations_directory_item::parameter_annotation,	const DEXFile*,method, annotations);
ISO_DEFUSERCOMPPV(dalvik::annotations_directory_item,						const DEXFile*,class_annotations, fields_size, annotated_methods_size, annotated_parameters_size);//, field_annotations, method_annotations, parameter_annotations);

struct map_item : dalvik::map_item {
	ISO::Browser2	array(const DEXFile *p) const;
};
typedef embedded_array<map_item, uint32> map_list;

ISO_DEFUSERCOMPV(dalvik::map_item, type, size, offset);
ISO_DEFCOMPPV(map_item&, const DEXFile*, type, size, array);

template<> struct ISO::def<dalvik::string_data_item> : ISO::VirtualT2<dalvik::string_data_item> {
	static ISO::Browser2	Deref(dalvik::string_data_item &a) {
#if 0
		byte_reader	b(&a);
		uint32		len = b.get<dalvik::uleb128>();
		return ISO_ptr<string>(0, str((const char*)b.p, len));
#else
		return ISO_ptr<string>(0, str(a.begin(), a.end()));
#endif
	}
};

namespace iso {
const dalvik::string_data_item *next(const dalvik::string_data_item *p) {
	byte_reader	b(p);
	uint32		len = b.get<dalvik::uleb128>();
	return (const dalvik::string_data_item*)(b.p + len + 1);
}
}


struct method_handle_item : dalvik::method_handle_item {
	ISO::Browser2	field_or_method(const DEXFile *p)		const;
};
ISO_DEFCOMPPV(method_handle_item&, const DEXFile*,method_handle_type, field_or_method);

namespace iso {
	template<typename R, typename T, typename C> bool read(R &r, dalvik::index<T, C> &x)		{ return read(r, x.idx); }
	template<typename R, typename T, typename O> bool read(R &r, dalvik::offset<T, O> &x)		{ return read(r, x.offset); }

	template<typename R> bool read(R &r, dalvik::class_data_item::encoded_field &x)				{ return r.read(x.field_diff) && r.read(x.access_flags); }
	template<typename R> bool readn(R &r, dalvik::class_data_item::encoded_field *t, size_t n)	{ return readnx1(r, t, n); }

	template<typename R> bool read(R &r, dalvik::class_data_item::encoded_method &x)			{ return r.read(x.method_diff) && r.read(x.access_flags) && r.read(x.code); }
	template<typename R> bool readn(R &r, dalvik::class_data_item::encoded_method *t, size_t n)	{ return readnx1(r, t, n); }

	template<typename R> bool read(R &r, dalvik::annotation_element &x)							{ return r.read(x.name) && r.read(x.value); }
	template<typename R> bool readn(R &r, dalvik::annotation_element *t, size_t n)				{ return readnx1(r, t, n); }
}

struct encoded_annotation {
	dalvik::index<dalvik::type_id_item, dalvik::uleb128>	type;		//type of the annotation. This must be a class (not array or primitive) type.
	dynamic_array<dalvik::annotation_element>				elements;	//elements of the annotation, represented directly in-line (not as offsets). Elements must be sorted in increasing order by string_id index.
	encoded_annotation(byte_reader b) : type(b.get()) { elements.read(b, b.get<dalvik::uleb128>()); }
};
ISO_DEFUSERCOMPP0(encoded_annotation, const DEXFile*, 2) { ISO_SETFIELDS(0,	type, elements); }};

template<typename P> auto get(param_element<dalvik::encoded_annotation&, P> &a) {
	return make_param_element(encoded_annotation(&a.t), a.p);
}

// code_item

struct code_item : dalvik::code_item {
	const DEXFile *dex;

	const_memory_block						insns;
	ptr_array<const try_item>				tries;
	dynamic_array<encoded_catch_handler>	handlers;

	code_item() {
		clear(*(dalvik::code_item*)this);
	}
	code_item(const dalvik::code_item *p, const DEXFile *_dex) : dalvik::code_item(*p), dex(_dex) {
		byte_reader	b(p + 1);

		insns	= b.get_block(p->insns_size * 2);
		tries	= {b.get_ptr<try_item>(tries_size), tries_size};

		if (tries_size) {
			handlers.read(b, b.get<dalvik::uleb128>());
		}
	}
};

template<typename P> code_item get(param_element<dalvik::code_item&, P> &a) {
	return code_item(&a.t, a.p);
}

ISO_DEFUSERCOMPV(code_item::try_item, start_addr, insn_count);
ISO_DEFUSERCOMPV(code_item,	registers_size, ins_size, outs_size, tries, insns);//, insns, padding, tries, handlers);

// class_data_item

struct class_data_item : dalvik::class_data_item {
	const DEXFile *dex;

	dynamic_array<encoded_field>	static_fields;//[static_fields_size];			must be sorted by field_idx in increasing order
	dynamic_array<encoded_field>	instance_fields;//[instance_fields_size];		must be sorted by field_idx in increasing order
	dynamic_array<encoded_method>	direct_methods;//[direct_methods_size];			must be sorted by method_idx in increasing order
	dynamic_array<encoded_method>	virtual_methods;//[virtual_methods_size];		This list should not include inherited methods unless overridden by the class that this item represents. The methods must be sorted by method_idx in increasing order. The method_idx of a virtual method must not be the same as any direct method.

	auto	get_static_fields()		const { return with_param(static_fields, dex); }
	auto	get_instance_fields()	const { return with_param(instance_fields, dex); }
	auto	get_direct_methods()	const { return with_param(direct_methods, dex); }
	auto	get_virtual_methods()	const { return with_param(virtual_methods, dex); }

	class_data_item(const dalvik::class_data_item *p, const DEXFile *_dex) : dex(_dex) {
		if (p) {
			byte_reader	b(p);
			b.read(static_fields_size);
			b.read(instance_fields_size);
			b.read(direct_methods_size);
			b.read(virtual_methods_size);

			static_fields.read(b, static_fields_size);
			instance_fields.read(b, instance_fields_size);
			direct_methods.read(b, direct_methods_size);
			virtual_methods.read(b, virtual_methods_size);
		}
	}
};

template<typename P> class_data_item get(param_element<dalvik::class_data_item&, P> &a) {
	return class_data_item(&a.t, a.p);
}

ISO_DEFUSERCOMPPV(class_data_item::encoded_field,	const DEXFile*, field_diff, access_flags);
ISO_DEFUSERCOMPPV(class_data_item::encoded_method,	const DEXFile*, method_diff, access_flags, code);
ISO_DEFUSERCOMPV(class_data_item, get_static_fields, get_instance_fields, get_direct_methods, get_virtual_methods);


//struct DEXFile

struct DEXFile : dalvik::DEXheader {
	auto								get_map()			const { return with_param(*(const map_list*)(map.get(this)), this); }
	const dalvik::string_id_item&		get_string(int i)	const {	return string_ids.get(this)[i]; }
	const dalvik::type_id_item&			get_type(int i)		const {	return type_ids.get(this)[i]; }
	const dalvik::proto_id_item&		get_proto(int i)	const {	return proto_ids.get(this)[i]; }
	const dalvik::field_id_item&		get_field(int i)	const {	return field_ids.get(this)[i]; }
	const dalvik::method_id_item&		get_method(int i)	const {	return method_ids.get(this)[i]; }
//	const method_handle_item&	get_method_handle(int i)	const {	return static_cast<const method_handle_item&>(method_handle_ids.get(this)[i]); }

	template<typename O>	const dalvik::string_id_item&		get(dalvik::index<dalvik::string_id_item,	O> i)	const {	return string_ids.get(this)	[i.idx]; }
	template<typename O>	const dalvik::type_id_item&			get(dalvik::index<dalvik::type_id_item,		O> i)	const {	return type_ids.get(this)	[i.idx]; }
	template<typename O>	const dalvik::proto_id_item&		get(dalvik::index<dalvik::proto_id_item,	O> i)	const {	return proto_ids.get(this)	[i.idx]; }
	template<typename O>	const dalvik::field_id_item&		get(dalvik::index<dalvik::field_id_item,	O> i)	const {	return field_ids.get(this)	[i.idx]; }
	template<typename O>	const dalvik::method_id_item&		get(dalvik::index<dalvik::method_id_item,	O> i)	const {	return method_ids.get(this)	[i.idx]; }
//	const method_handle_item&	get(dalvik::index<dalvik::method_handle_item> i)	const {	return get_method(i.idx); }
};
ISO_DEFUSERCOMPV(DEXFile, get_map);

ISO::Browser2	method_handle_item::field_or_method(const DEXFile *p) const {
	switch (method_handle_type) {
		default:
			return ISO::MakeBrowser(make_param_element(p->get_field(field_or_method_id), p));

		case INVOKE_STATIC:
		case INVOKE_INSTANCE:
			return ISO::MakeBrowser(make_param_element(p->get_method(field_or_method_id), p));
	}
}

ISO_DEFCOMPPV(dalvik::class_def_item&, const DEXFile*,klass, access_flags, superclass, interfaces, source_file, annotations, class_data, static_values);
//ISO_DEFCOMPPV(dalvik::class_def_item&, const DEXFile*,klass, access_flags, superclass, interfaces, source_file, class_data, static_values);

ISO::Browser2	map_item::array(const DEXFile *p) const {
	const void	*data	= (const uint8*)p + offset;
	switch (type) {
		case dalvik::map_item::HEADER_ITEM:					return ISO::MakePtr("Header",				*(dalvik::DEXheader*)data);
		case dalvik::map_item::STRING_ID_ITEM:				return ISO::MakePtr("String ids",			make_range_n(make_param_iterator((const dalvik::string_id_item			*)data, p), size));
		case dalvik::map_item::TYPE_ID_ITEM:				return ISO::MakePtr("Type ids",				make_range_n(make_param_iterator((const dalvik::type_id_item			*)data, p), size));
		case dalvik::map_item::PROTO_ID_ITEM:				return ISO::MakePtr("Proto ids",			make_range_n(make_param_iterator((const dalvik::proto_id_item			*)data, p), size));
		case dalvik::map_item::FIELD_ID_ITEM:				return ISO::MakePtr("Field ids",			make_range_n(make_param_iterator((const dalvik::field_id_item			*)data, p), size));
		case dalvik::map_item::METHOD_ID_ITEM:				return ISO::MakePtr("Method ids",			make_range_n(make_param_iterator((const dalvik::method_id_item			*)data, p), size));
		case dalvik::map_item::CLASS_DEF_ITEM:				return ISO::MakePtr("Class defs",			make_range_n(make_param_iterator((const dalvik::class_def_item			*)data, p), size));
		case dalvik::map_item::CALL_SITE_ID_ITEM:			return ISO::MakePtr("Call site_ids",		make_range_n(make_param_iterator((const dalvik::call_site_id_item		*)data, p), size));
		case dalvik::map_item::METHOD_HANDLE_ITEM:			return ISO::MakePtr("Method handles",		make_range_n(make_param_iterator((const method_handle_item				*)data, p), size));
		case dalvik::map_item::MAP_LIST:					return ISO::MakePtr("Maps",					make_range_n(make_param_iterator((const dalvik::map_list				*)data, p), size));
		case dalvik::map_item::TYPE_LIST:					return ISO::MakePtr("Types",				make_range_n(make_param_iterator((const dalvik::type_list				*)data, p), size));
		case dalvik::map_item::ANNOTATION_SET_REF_LIST:		return ISO::MakePtr("Annotation Set Refs",	make_range_n(make_param_iterator((const dalvik::annotation_set_ref_list	*)data, p), size));
		case dalvik::map_item::ANNOTATION_SET_ITEM:			return ISO::MakePtr("Annotation Sets",		make_range_n(make_param_iterator((const dalvik::annotation_set_item		*)data, p), size));
//		case dalvik::map_item::CLASS_DATA_ITEM:				return ISO::MakePtr("Class Data",			make_range_n(make_param_iterator((const dalvik::class_data_item			*)data, p), size));
//		case dalvik::map_item::CODE_ITEM:					return ISO::MakePtr("Code",					make_range_n(make_param_iterator((const dalvik::code_item				*)data, p), size));
		case dalvik::map_item::STRING_DATA_ITEM:			return ISO::MakePtr("String Data",			make_range_n(make_next_iterator((const dalvik::string_data_item*)data), size));
//		case dalvik::map_item::DEBUG_INFO_ITEM:				return ISO::MakePtr("Debug Info",			make_range_n(make_param_iterator((const dalvik::debug_info_item			*)data, p), size));
		case dalvik::map_item::ANNOTATION_ITEM:				return ISO::MakePtr("Annotation",			make_range_n(make_param_iterator((const dalvik::annotation_item			*)data, p), size));
//		case dalvik::map_item::ENCODED_ARRAY_ITEM:			return ISO::MakePtr("Encoded Array",		make_range_n(make_param_iterator((const dalvik::encoded_array_item		*)data, p), size));
		case dalvik::map_item::ANNOTATIONS_DIRECTORY_ITEM:	return ISO::MakePtr("Annotations Directory",make_range_n(make_param_iterator((const dalvik::annotations_directory_item*)data, p), size));
		default: return ISO::MakeBrowser(const_memory_block(data, size));
	}
}

tag2 _GetName(const map_item &t) {
	switch (t.type) {
		case dalvik::map_item::HEADER_ITEM:					return "Header";
		case dalvik::map_item::STRING_ID_ITEM:				return "String ids";
		case dalvik::map_item::TYPE_ID_ITEM:				return "Type ids";
		case dalvik::map_item::PROTO_ID_ITEM:				return "Proto ids";
		case dalvik::map_item::FIELD_ID_ITEM:				return "Field ids";
		case dalvik::map_item::METHOD_ID_ITEM:				return "Method ids";
		case dalvik::map_item::CLASS_DEF_ITEM:				return "Class defs";
		case dalvik::map_item::CALL_SITE_ID_ITEM:			return "Call site_ids";
		case dalvik::map_item::METHOD_HANDLE_ITEM:			return "Method handles";
		case dalvik::map_item::MAP_LIST:					return "Maps";
		case dalvik::map_item::TYPE_LIST:					return "Types";
		case dalvik::map_item::ANNOTATION_SET_REF_LIST:		return "Annotation Set Refs";
		case dalvik::map_item::ANNOTATION_SET_ITEM:			return "Annotation Sets";
		case dalvik::map_item::CLASS_DATA_ITEM:				return "Class Data";
		case dalvik::map_item::CODE_ITEM:					return "Code";
		case dalvik::map_item::STRING_DATA_ITEM:			return "String Data";
		case dalvik::map_item::DEBUG_INFO_ITEM:				return "Debug Info";
		case dalvik::map_item::ANNOTATION_ITEM:				return "Annotation";
		case dalvik::map_item::ENCODED_ARRAY_ITEM:			return "Encoded Array";
		case dalvik::map_item::ANNOTATIONS_DIRECTORY_ITEM:	return "Annotations Directory";
		default: return tag2();
	}
}

tag2 _GetName(const param_element<const dalvik::class_def_item&, const DEXFile*> &a) {
	auto t = a.p->get(a.t.klass);
	auto n = a.p->get(t.descriptor);
	auto n2 = n.get(a.p);
	return tag2(str(n2->begin(), n2->end()));
}

tag2 _GetName(const param_element<const dalvik::class_data_item::encoded_field&, const DEXFile*> &a) {
	auto f = a.p->get(a.t.field_diff);
	auto n = a.p->get(f.name);
	auto n2 = n.get(a.p);
	return tag2(str(n2->begin(), n2->end()));
}

tag2 _GetName(const param_element<const dalvik::class_data_item::encoded_method&, const DEXFile*> &a) {
	auto f = a.p->get(a.t.method_diff);
	auto n = a.p->get(f.name);
	auto n2 = n.get(a.p);
	return tag2(str(n2->begin(), n2->end()));
}

class DEXFileHandler : public FileHandler {
	const char*			GetExt() override { return "dex";	}
	const char*			GetDescription() override { return "Dalvik executable file";	}

	int					Check(istream_ref file) override {
		dalvik::DEXheader	header;
		file.seek(0);
		return file.read(header) && header.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>		Read(tag id, istream_ref file) override {
		dalvik::DEXheader	header;
		if (!file.read(header) || !header.valid())
			return ISO_NULL;

		auto p = ISO::MakePtrSize<32, DEXFile>(id, header.file_size);
		file.seek(0);
		file.readbuff(p, header.file_size);
		return p;
	}
} dex;

//-----------------------------------------------------------------------------
//	Disassembler
//-----------------------------------------------------------------------------

#ifdef ISO_EDITOR
#include "disassembler.h"
//Mnemonic	Bit Sizes	Meaning
//b			8			immediate signed byte
//c			16, 32		constant pool index
//f			16			interface constants (only used in statically linked formats)
//h			16			immediate signed hat (high-order bits of a 32- or 64-bit value; low-order bits are all 0)
//i			32			immediate signed int, or 32-bit float
//l			64			immediate signed long, or 64-bit double
//m			16			method constants (only used in statically linked formats)
//n			4			immediate signed nibble
//s			16			immediate signed short
//t			8, 16, 32	branch target
//x			0			no additional data



/*
//ID is # words, # regs, extra
//Format						ID			Syntax													Notable Opcodes Covered
N/A								00x			N/A														pseudo-format used for unused opcodes; suggested for use as the nominal format for a breakpoint opcode
ии|op							10x			op
B|A|op							12x			op vA, vB
								11n			op vA, #+B
AA|op							11x			op vAA
								10t			op +AA													goto
ии|op AAAA						20t			op +AAAA												goto/16
AA|op BBBB						20bc		op AA, kind@BBBB
AA|op BBBB						22x			op vAA, vBBBB
								21t			op vAA, +BBBB
								21s			op vAA, #+BBBB
								21h			op vAA, #+BBBB0000
											op vAA, #+BBBB000000000000
								21c			op vAA, type@BBBB										check-cast
											op vAA, field@BBBB										const-class
											op vAA, string@BBBB										const-string
AA|op CC|BB						23x			op vAA, vBB, vCC
								22b			op vAA, vBB, #+CC
B|A|op CCCC						22t			op vA, vB, +CCCC
								22s			op vA, vB, #+CCCC
								22c			op vA, vB, type@CCCC
											op vA, vB, field@CCCC									instance-of
								22cs		op vA, vB, fieldoff@CCCC
ии|op AAAAlo AAAAhi				30t			op +AAAAAAAA											goto/32
ии|op AAAA BBBB					32x			op vAAAA, vBBBB
AA|op BBBBlo BBBBhi				31i			op vAA, #+BBBBBBBB
								31t			op vAA, +BBBBBBBB
								31c			op vAA, string@BBBBBBBB									const-string/jumbo
A|G|op BBBB F|E|D|C				35c			[A=5] op {vC, vD, vE, vF, vG}, meth@BBBB
											[A=5] op {vC, vD, vE, vF, vG}, site@BBBB
											[A=5] op {vC, vD, vE, vF, vG}, type@BBBB
											[A=4] op {vC, vD, vE, vF}, kind@BBBB
											[A=3] op {vC, vD, vE}, kind@BBBB
											[A=2] op {vC, vD}, kind@BBBB
											[A=1] op {vC}, kind@BBBB
											[A=0] op {}, kind@BBBB
								35ms		[A=5] op {vC, vD, vE, vF, vG}, vtaboff@BBBB
											[A=4] op {vC, vD, vE, vF}, vtaboff@BBBB
											[A=3] op {vC, vD, vE}, vtaboff@BBBB
											[A=2] op {vC, vD}, vtaboff@BBBB
											[A=1] op {vC}, vtaboff@BBBB
								35mi		[A=5] op {vC, vD, vE, vF, vG}, inline@BBBB
											[A=4] op {vC, vD, vE, vF}, inline@BBBB
											[A=3] op {vC, vD, vE}, inline@BBBB
											[A=2] op {vC, vD}, inline@BBBB
											[A=1] op {vC}, inline@BBBB
AA|op BBBB CCCC					3rc			op {vCCCC .. vNNNN}, meth@BBBB
											op {vCCCC .. vNNNN}, site@BBBB
											op {vCCCC .. vNNNN}, type@BBBB
								3rms		op {vCCCC .. vNNNN}, vtaboff@BBBB
								3rmi		op {vCCCC .. vNNNN}, inline@BBBB
A|G|op BBBB F|E|D|C HHHH		45cc		[A=5] op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH
											[A=4] op {vC, vD, vE, vF}, meth@BBBB, proto	HHHH
											[A=3] op {vC, vD, vE}, meth@BBBB, proto	HHHH
											[A=2] op {vC, vD}, meth@BBBB, proto	HHHH
											[A=1] op {vC}, meth@BBBB, proto	HHHH					invoke-polymorphic
AA|op BBBB CCCC HHHH			4rcc		op> {vCCCC .. vNNNN}, meth@BBBB, proto@HHHH
AA|op BBBBlo BBBB BBBB BBBBhi	51l			op vAA, #+BBBBBBBBBBBBBBBB								const-wide
*/
enum MODE {
	MODE_10x,
	MODE_12x,
	MODE_11n,
	MODE_11x,
	MODE_10t,
	MODE_20t,
	MODE_20bc,
	MODE_22x,
	MODE_21t,
	MODE_21s,
	MODE_21h,
	MODE_21c,
	MODE_23x,
	MODE_22b,
	MODE_22t,
	MODE_22s,
	MODE_22c,
	MODE_30t,
	MODE_32x,
	MODE_31i,
	MODE_31t,
	MODE_31c,
	MODE_35c,
	MODE_3rc,
	MODE_45cc,
	MODE_4rcc,
	MODE_51l,

	MODE_kind	= 1 << 5,
	MODE_type	= MODE_kind * 0,
	MODE_field	= MODE_kind * 1,
	MODE_string = MODE_kind * 2,
	MODE_meth	= MODE_kind * 3,
	MODE_site	= MODE_kind * 4,
};

MODE operator|(MODE a, MODE b) { return MODE(int(a) | int(b)); }

uint8 mode_lengths[] = {
	1,
	1,
	1,
	1,
	1,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	4,
	4,
	5,
};

enum OPCODE {
	op_nop						= 0x00,		//10x	nop
	op_move						= 0x01,		//12x	move vA, vB
	op_move_from16				= 0x02,		//22x	move/from16 vAA, vBBBB
	op_move_16					= 0x03,		//32x	move/16 vAAAA, vBBBB
	op_move_wide				= 0x04,		//12x	move-wide vA, vB
	op_move_wide_from16			= 0x05,		//22x	move-wide/from16 vAA, vBBBB
	op_move_wide_16				= 0x06,		//32x	move-wide/16 vAAAA, vBBBB
	op_move_object				= 0x07,		//12x	move-object vA, vB
	op_move_object_from16		= 0x08,		//22x	move-object/from16 vAA, vBBBB
	op_move_object_16			= 0x09,		//32x	move-object/16 vAAAA, vBBBB
	op_move_result				= 0x0a,		//11x	move-result vAA
	op_move_result_wide			= 0x0b,		//11x	move-result-wide vAA
	op_move_result_object		= 0x0c,		//11x	move-result-object vAA
	op_move_exception			= 0x0d,		//11x	move-exception vAA
	op_return_void				= 0x0e,		//10x	return-void
	op_return					= 0x0f,		//11x	return vAA
	op_return_wide				= 0x10,		//11x	return-wide vAA
	op_return_object			= 0x11,		//11x	return-object vAA
	op_const_4					= 0x12,		//11n	const/4 vA, #+B
	op_const_16					= 0x13,		//21s	const/16 vAA, #+BBBB
	op_const					= 0x14,		//31i	const vAA, #+BBBBBBBB
	op_const_high16				= 0x15,		//21h	const/high16 vAA, #+BBBB0000
	op_const_wide_16			= 0x16,		//21s	const-wide/16 vAA, #+BBBB
	op_const_wide_32			= 0x17,		//31i	const-wide/32 vAA, #+BBBBBBBB
	op_const_wide				= 0x18,		//51l	const-wide vAA, #+BBBBBBBBBBBBBBBB
	op_const_wide_high16		= 0x19,		//21h	const-wide/high16 vAA, #+BBBB000000000000
	op_const_string				= 0x1a,		//21c	const-string vAA, string@BBBB
	op_const_string_jumbo		= 0x1b,		//31c	const-string/jumbo vAA, string@BBBBBBBB
	op_const_class				= 0x1c,		//21c	const-class vAA, type@BBBB
	op_monitor_enter			= 0x1d,		//11x	monitor-enter vAA
	op_monitor_exit				= 0x1e,		//11x	monitor-exit vAA
	op_check_cast				= 0x1f,		//21c	check-cast vAA, type@BBBB
	op_instance_of				= 0x20,		//22c	instance-of vA, vB, type@CCCC
	op_array_length				= 0x21,		//12x	array-length vA, vB
	op_new_instance				= 0x22,		//21c	new-instance vAA, type@BBBB
	op_new_array				= 0x23,		//22c	new-array vA, vB, type@CCCC
	op_filled_new_array			= 0x24,		//35c	filled-new-array {vC, vD, vE, vF, vG}, type@BBBB
	op_filled_new_array_range	= 0x25,		//3rc	filled-new-array/range {vCCCC .. vNNNN}, type@BBBB
	op_fill_array_data			= 0x26,		//31t	fill-array-data vAA, +BBBBBBBB (with supplemental data as specified below in "fill-array-data-payload Format")
	op_throw					= 0x27,		//11x	throw vAA
	op_goto						= 0x28,		//10t	goto +AA
	op_goto_16					= 0x29,		//20t	goto/16 +AAAA
	op_goto_32					= 0x2a,		//30t	goto/32 +AAAAAAAA
	op_packed_switch			= 0x2b,		//31t	packed-switch vAA, +BBBBBBBB (with supplemental data as specified below in "packed-switch-payload Format")
	op_sparse_switch			= 0x2c,		//31t	sparse-switch vAA, +BBBBBBBB (with supplemental data as specified below in "sparse-switch-payload Format")
	op_cmpl_float				= 0x2d,		//23x	cmpl-float (lt bias)
	op_cmpg_float				= 0x2e,		//23x	cmpg-float (gt bias)
	op_cmpl_double				= 0x2f,		//23x	cmpl-double (lt bias)
	op_cmpg_double				= 0x30,		//23x	cmpg-double (gt bias)
	op_cmp_long					= 0x31,		//23x	cmp-long
	op_if_eq					= 0x32,		//22t	if-eq
	op_if_ne					= 0x33,		//22t	if-ne
	op_if_lt					= 0x34,		//22t	if-lt
	op_if_ge					= 0x35,		//22t	if-ge
	op_if_gt					= 0x36,		//22t	if-gt
	op_if_le					= 0x37,		//22t	if-le
	op_if_eqz					= 0x38,		//22t	if-eqz
	op_if_nez					= 0x39,		//22t	if-nez
	op_if_ltz					= 0x3a,		//22t	if-ltz
	op_if_gez					= 0x3b,		//22t	if-gez
	op_if_gtz					= 0x3c,		//22t	if-gtz
	op_if_lez					= 0x3d,		//22t	if-lez
	op_unused3e					= 0x3e,
	op_unused3f					= 0x3f,
	op_unused40					= 0x40,
	op_unused41					= 0x41,
	op_unused42					= 0x42,
	op_unused43					= 0x43,
	op_aget						= 0x44,		//23x	aget
	op_aget_wide				= 0x45,		//23x	aget-wide
	op_aget_object				= 0x46,		//23x	aget-object
	op_aget_boolean				= 0x47,		//23x	aget-boolean
	op_aget_byte				= 0x48,		//23x	aget-byte
	op_aget_char				= 0x49,		//23x	aget-char
	op_aget_short				= 0x4a,		//23x	aget-short
	op_aput						= 0x4b,		//23x	aput
	op_aput_wide				= 0x4c,		//23x	aput-wide
	op_aput_object				= 0x4d,		//23x	aput-object
	op_aput_boolean				= 0x4e,		//23x	aput-boolean
	op_aput_byte				= 0x4f,		//23x	aput-byte
	op_aput_char				= 0x50,		//23x	aput-char
	op_aput_short				= 0x51,		//23x	aput-short
	op_iget						= 0x52,		//22c	iget
	op_iget_wide				= 0x53,		//22c	iget-wide
	op_iget_object				= 0x54,		//22c	iget-object
	op_iget_boolean				= 0x55,		//22c	iget-boolean
	op_iget_byte				= 0x56,		//22c	iget-byte
	op_iget_char				= 0x57,		//22c	iget-char
	op_iget_short				= 0x58,		//22c	iget-short
	op_iput						= 0x59,		//22c	iput
	op_iput_wide				= 0x5a,		//22c	iput-wide
	op_iput_object				= 0x5b,		//22c	iput-object
	op_iput_boolean				= 0x5c,		//22c	iput-boolean
	op_iput_byte				= 0x5d,		//22c	iput-byte
	op_iput_char				= 0x5e,		//22c	iput-char
	op_iput_short				= 0x5f,		//22c	iput-short
	op_sget						= 0x60,		//21c	sget
	op_sget_wide				= 0x61,		//21c	sget-wide
	op_sget_object				= 0x62,		//21c	sget-object
	op_sget_boolean				= 0x63,		//21c	sget-boolean
	op_sget_byte				= 0x64,		//21c	sget-byte
	op_sget_char				= 0x65,		//21c	sget-char
	op_sget_short				= 0x66,		//21c	sget-short
	op_sput						= 0x67,		//21c	sput
	op_sput_wide				= 0x68,		//21c	sput-wide
	op_sput_object				= 0x69,		//21c	sput-object
	op_sput_boolean				= 0x6a,		//21c	sput-boolean
	op_sput_byte				= 0x6b,		//21c	sput-byte
	op_sput_char				= 0x6c,		//21c	sput-char
	op_sput_short				= 0x6d,		//21c	sput-short
	op_invoke_virtual			= 0x6e,		//35c	invoke-virtual
	op_invoke_super				= 0x6f,		//35c	invoke-super
	op_invoke_direct			= 0x70,		//35c	invoke-direct
	op_invoke_static			= 0x71,		//35c	invoke-static
	op_invoke_interface			= 0x72,		//35c	invoke-interface
	op_unused73					= 0x73,
	op_invoke_virtual_range		= 0x74,		//3rc	invoke-virtual/range
	op_invoke_super_range		= 0x75,		//3rc	invoke-super/range
	op_invoke_direct_range		= 0x76,		//3rc	invoke-direct/range
	op_invoke_static_range		= 0x77,		//3rc	invoke-static/range
	op_invoke_interface_range	= 0x78,		//3rc	invoke-interface/range
	op_unused79					= 0x79,
	op_unused7a					= 0x7a,
	op_neg_int					= 0x7b,		//12x	neg-int
	op_not_int					= 0x7c,		//12x	not-int
	op_neg_long					= 0x7d,		//12x	neg-long
	op_not_long					= 0x7e,		//12x	not-long
	op_neg_float				= 0x7f,		//12x	neg-float
	op_neg_double				= 0x80,		//12x	neg-double
	op_int_to_long				= 0x81,		//12x	int-to-long
	op_int_to_float				= 0x82,		//12x	int-to-float
	op_int_to_double			= 0x83,		//12x	int-to-double
	op_long_to_int				= 0x84,		//12x	long-to-int
	op_long_to_float			= 0x85,		//12x	long-to-float
	op_long_to_double			= 0x86,		//12x	long-to-double
	op_float_to_int				= 0x87,		//12x	float-to-int
	op_float_to_long			= 0x88,		//12x	float-to-long
	op_float_to_double			= 0x89,		//12x	float-to-double
	op_double_to_int			= 0x8a,		//12x	double-to-int
	op_double_to_long			= 0x8b,		//12x	double-to-long
	op_double_to_float			= 0x8c,		//12x	double-to-float
	op_int_to_byte				= 0x8d,		//12x	int-to-byte
	op_int_to_char				= 0x8e,		//12x	int-to-char
	op_int_to_short				= 0x8f,		//12x	int-to-short
	op_add_int					= 0x90,		//23x	add-int
	op_sub_int					= 0x91,		//23x	sub-int
	op_mul_int					= 0x92,		//23x	mul-int
	op_div_int					= 0x93,		//23x	div-int
	op_rem_int					= 0x94,		//23x	rem-int
	op_and_int					= 0x95,		//23x	and-int
	op_or_int					= 0x96,		//23x	or-int
	op_xor_int					= 0x97,		//23x	xor-int
	op_shl_int					= 0x98,		//23x	shl-int
	op_shr_int					= 0x99,		//23x	shr-int
	op_ushr_int					= 0x9a,		//23x	ushr-int
	op_add_long					= 0x9b,		//23x	add-long
	op_sub_long					= 0x9c,		//23x	sub-long
	op_mul_long					= 0x9d,		//23x	mul-long
	op_div_long					= 0x9e,		//23x	div-long
	op_rem_long					= 0x9f,		//23x	rem-long
	op_and_long					= 0xa0,		//23x	and-long
	op_or_long					= 0xa1,		//23x	or-long
	op_xor_long					= 0xa2,		//23x	xor-long
	op_shl_long					= 0xa3,		//23x	shl-long
	op_shr_long					= 0xa4,		//23x	shr-long
	op_ushr_long				= 0xa5,		//23x	ushr-long
	op_add_float				= 0xa6,		//23x	add-float
	op_sub_float				= 0xa7,		//23x	sub-float
	op_mul_float				= 0xa8,		//23x	mul-float
	op_div_float				= 0xa9,		//23x	div-float
	op_rem_float				= 0xaa,		//23x	rem-float
	op_add_double				= 0xab,		//23x	add-double
	op_sub_double				= 0xac,		//23x	sub-double
	op_mul_double				= 0xad,		//23x	mul-double
	op_div_double				= 0xae,		//23x	div-double
	op_rem_double				= 0xaf,		//23x	rem-double
	op_add_int_2addr			= 0xb0,		//12x	add-int/2addr
	op_sub_int_2addr			= 0xb1,		//12x	sub-int/2addr
	op_mul_int_2addr			= 0xb2,		//12x	mul-int/2addr
	op_div_int_2addr			= 0xb3,		//12x	div-int/2addr
	op_rem_int_2addr			= 0xb4,		//12x	rem-int/2addr
	op_and_int_2addr			= 0xb5,		//12x	and-int/2addr
	op_or_int_2addr				= 0xb6,		//12x	or-int/2addr
	op_xor_int_2addr			= 0xb7,		//12x	xor-int/2addr
	op_shl_int_2addr			= 0xb8,		//12x	shl-int/2addr
	op_shr_int_2addr			= 0xb9,		//12x	shr-int/2addr
	op_ushr_int_2addr			= 0xba,		//12x	ushr-int/2addr
	op_add_long_2addr			= 0xbb,		//12x	add-long/2addr
	op_sub_long_2addr			= 0xbc,		//12x	sub-long/2addr
	op_mul_long_2addr			= 0xbd,		//12x	mul-long/2addr
	op_div_long_2addr			= 0xbe,		//12x	div-long/2addr
	op_rem_long_2addr			= 0xbf,		//12x	rem-long/2addr
	op_and_long_2addr			= 0xc0,		//12x	and-long/2addr
	op_or_long_2addr			= 0xc1,		//12x	or-long/2addr
	op_xor_long_2addr			= 0xc2,		//12x	xor-long/2addr
	op_shl_long_2addr			= 0xc3,		//12x	shl-long/2addr
	op_shr_long_2addr			= 0xc4,		//12x	shr-long/2addr
	op_ushr_long_2addr			= 0xc5,		//12x	ushr-long/2addr
	op_add_float_2addr			= 0xc6,		//12x	add-float/2addr
	op_sub_float_2addr			= 0xc7,		//12x	sub-float/2addr
	op_mul_float_2addr			= 0xc8,		//12x	mul-float/2addr
	op_div_float_2addr			= 0xc9,		//12x	div-float/2addr
	op_rem_float_2addr			= 0xca,		//12x	rem-float/2addr
	op_add_double_2addr			= 0xcb,		//12x	add-double/2addr
	op_sub_double_2addr			= 0xcc,		//12x	sub-double/2addr
	op_mul_double_2addr			= 0xcd,		//12x	mul-double/2addr
	op_div_double_2addr			= 0xce,		//12x	div-double/2addr
	op_rem_double_2addr			= 0xcf,		//12x	rem-double/2addr
	op_add_int_lit16			= 0xd0,		//22s	add-int/lit16
	op_rsub_int					= 0xd1,		//22s	rsub-int (reverse subtract)
	op_mul_int_lit16			= 0xd2,		//22s	mul-int/lit16
	op_div_int_lit16			= 0xd3,		//22s	div-int/lit16
	op_rem_int_lit16			= 0xd4,		//22s	rem-int/lit16
	op_and_int_lit16			= 0xd5,		//22s	and-int/lit16
	op_or_int_lit16				= 0xd6,		//22s	or-int/lit16
	op_xor_int_lit16			= 0xd7,		//22s	xor-int/lit16
	op_add_int_lit8				= 0xd8,		//22b	add-int/lit8
	op_rsub_int_lit8			= 0xd9,		//22b	rsub-int/lit8
	op_mul_int_lit8				= 0xda,		//22b	mul-int/lit8
	op_div_int_lit8				= 0xdb,		//22b	div-int/lit8
	op_rem_int_lit8				= 0xdc,		//22b	rem-int/lit8
	op_and_int_lit8				= 0xdd,		//22b	and-int/lit8
	op_or_int_lit8				= 0xde,		//22b	or-int/lit8
	op_xor_int_lit8				= 0xdf,		//22b	xor-int/lit8
	op_shl_int_lit8				= 0xe0,		//22b	shl-int/lit8
	op_shr_int_lit8				= 0xe1,		//22b	shr-int/lit8
	op_ushr_int_lit8			= 0xe2,		//22b	ushr-int/lit8
	op_unusede3					= 0xe3,
	op_unusede4					= 0xe4,
	op_unusede5					= 0xe5,
	op_unusede6					= 0xe6,
	op_unusede7					= 0xe7,
	op_unusede8					= 0xe8,
	op_unusede9					= 0xe9,
	op_unusedea					= 0xea,
	op_unusedeb					= 0xeb,
	op_unusedec					= 0xec,
	op_unuseded					= 0xed,
	op_unusedee					= 0xee,
	op_unusedef					= 0xef,
	op_unusedf0					= 0xf0,
	op_unusedf1					= 0xf1,
	op_unusedf2					= 0xf2,
	op_unusedf3					= 0xf3,
	op_unusedf4					= 0xf4,
	op_unusedf5					= 0xf5,
	op_unusedf6					= 0xf6,
	op_unusedf7					= 0xf7,
	op_unusedf8					= 0xf8,
	op_unusedf9					= 0xf9,
	op_invoke_polymorphic		= 0xfa,		//45cc	invoke-polymorphic
	op_invoke_polymorphic_range	= 0xfb,		//4rcc	invoke-polymorphic/range
	op_invoke_custom 			= 0xfc,		//35c	invoke-custom
	op_invoke_custom_range 		= 0xfd,		//3rc	invoke-custom/range
	op_unusedfe					= 0xfe,
	op_unusedff					= 0xff,
};

struct OpCode { const char *mnemonic; MODE mode; } opcodes[] = {
	{"nop",						MODE_10x,		},
	{"move",					MODE_12x,		},
	{"move/from16",				MODE_22x,		},
	{"move/16",					MODE_32x,		},
	{"move-wide",				MODE_12x,		},
	{"move-wide/from16",		MODE_22x,		},
	{"move-wide/16",			MODE_32x,		},
	{"move-object",				MODE_12x,		},
	{"move-object/from16",		MODE_22x,		},
	{"move-object/16",			MODE_32x,		},
	{"move-result",				MODE_11x,		},
	{"move-result-wide",		MODE_11x,		},
	{"move-result-object",		MODE_11x,		},
	{"move-exception",			MODE_11x,		},
	{"return-void",				MODE_10x,		},
	{"return",					MODE_11x,		},
	{"return-wide",				MODE_11x,		},
	{"return-object",			MODE_11x,		},
	{"const/4",					MODE_11n,		},
	{"const/16",				MODE_21s,		},
	{"const",					MODE_31i,		},
	{"const/high16",			MODE_21h,		},
	{"const-wide/16",			MODE_21s,		},
	{"const-wide/32",			MODE_31i,		},
	{"const-wide",				MODE_51l,		},
	{"const-wide/high16",		MODE_21h,		},
	{"const-string",			MODE_21c|MODE_string,	},
	{"const-string/jumbo",		MODE_31c|MODE_string,	},
	{"const-class",				MODE_21c|MODE_field,	},
	{"monitor-enter",			MODE_11x,		},
	{"monitor-exit",			MODE_11x,		},
	{"check-cast",				MODE_21c|MODE_type,		},
	{"instance-of",				MODE_22c|MODE_field,	},
	{"array-length",			MODE_12x,		},
	{"new-instance",			MODE_21c|MODE_type,		},
	{"new-array",				MODE_22c|MODE_type,		},
	{"filled-new-array",		MODE_35c,		},
	{"filled-new-array/range",	MODE_3rc,		},
	{"fill-array-data",			MODE_31t,		},
	{"throw",					MODE_11x,		},
	{"goto",					MODE_10t,		},
	{"goto/16",					MODE_20t,		},
	{"goto/32",					MODE_30t,		},
	{"packed-switch",			MODE_31t,		},
	{"sparse-switch",			MODE_31t,		},
	{"cmpl-float",				MODE_23x,		},
	{"cmpg-float",				MODE_23x,		},
	{"cmpl-double",				MODE_23x,		},
	{"cmpg-double",				MODE_23x,		},
	{"cmp-long",				MODE_23x,		},
	{"if-eq",					MODE_22t,		},
	{"if-ne",					MODE_22t,		},
	{"if-lt",					MODE_22t,		},
	{"if-ge",					MODE_22t,		},
	{"if-gt",					MODE_22t,		},
	{"if-le",					MODE_22t,		},
	{"if-eqz",					MODE_22t,		},
	{"if-nez",					MODE_22t,		},
	{"if-ltz",					MODE_22t,		},
	{"if-gez",					MODE_22t,		},
	{"if-gtz",					MODE_22t,		},
	{"if-lez",					MODE_22t,		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{"aget",					MODE_23x,		},
	{"aget-wide",				MODE_23x,		},
	{"aget-object",				MODE_23x,		},
	{"aget-boolean",			MODE_23x,		},
	{"aget-byte",				MODE_23x,		},
	{"aget-char",				MODE_23x,		},
	{"aget-short",				MODE_23x,		},
	{"aput",					MODE_23x,		},
	{"aput-wide",				MODE_23x,		},
	{"aput-object",				MODE_23x,		},
	{"aput-boolean",			MODE_23x,		},
	{"aput-byte",				MODE_23x,		},
	{"aput-char",				MODE_23x,		},
	{"aput-short",				MODE_23x,		},
	{"iget",					MODE_22c|MODE_field,	},
	{"iget-wide",				MODE_22c|MODE_field,	},
	{"iget-object",				MODE_22c|MODE_field,	},
	{"iget-boolean",			MODE_22c|MODE_field,	},
	{"iget-byte",				MODE_22c|MODE_field,	},
	{"iget-char",				MODE_22c|MODE_field,	},
	{"iget-short",				MODE_22c|MODE_field,	},
	{"iput",					MODE_22c|MODE_field,	},
	{"iput-wide",				MODE_22c|MODE_field,	},
	{"iput-object",				MODE_22c|MODE_field,	},
	{"iput-boolean",			MODE_22c|MODE_field,	},
	{"iput-byte",				MODE_22c|MODE_field,	},
	{"iput-char",				MODE_22c|MODE_field,	},
	{"iput-short",				MODE_22c|MODE_field,	},
	{"sget",					MODE_21c|MODE_string,	},
	{"sget-wide",				MODE_21c|MODE_string,	},
	{"sget-object",				MODE_21c|MODE_string,	},
	{"sget-boolean",			MODE_21c|MODE_string,	},
	{"sget-byte",				MODE_21c|MODE_string,	},
	{"sget-char",				MODE_21c|MODE_string,	},
	{"sget-short",				MODE_21c|MODE_string,	},
	{"sput",					MODE_21c|MODE_string,	},
	{"sput-wide",				MODE_21c|MODE_string,	},
	{"sput-object",				MODE_21c|MODE_string,	},
	{"sput-boolean",			MODE_21c|MODE_string,	},
	{"sput-byte",				MODE_21c|MODE_string,	},
	{"sput-char",				MODE_21c|MODE_string,	},
	{"sput-short",				MODE_21c|MODE_string,	},
	{"invoke-virtual",			MODE_35c,		},
	{"invoke-super",			MODE_35c,		},
	{"invoke-direct",			MODE_35c,		},
	{"invoke-static",			MODE_35c,		},
	{"invoke-interface",		MODE_35c,		},
	{0,							MODE_10x		},
	{"invoke-virtual/range",	MODE_3rc,		},
	{"invoke-super/range",		MODE_3rc,		},
	{"invoke-direct/range",		MODE_3rc,		},
	{"invoke-static/range",		MODE_3rc,		},
	{"invoke-interface/range",	MODE_3rc,		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{"neg-int",					MODE_12x,		},
	{"not-int",					MODE_12x,		},
	{"neg-long",				MODE_12x,		},
	{"not-long",				MODE_12x,		},
	{"neg-float",				MODE_12x,		},
	{"neg-double",				MODE_12x,		},
	{"int-to-long",				MODE_12x,		},
	{"int-to-float",			MODE_12x,		},
	{"int-to-double",			MODE_12x,		},
	{"long-to-int",				MODE_12x,		},
	{"long-to-float",			MODE_12x,		},
	{"long-to-double",			MODE_12x,		},
	{"float-to-int",			MODE_12x,		},
	{"float-to-long",			MODE_12x,		},
	{"float-to-double",			MODE_12x,		},
	{"double-to-int",			MODE_12x,		},
	{"double-to-long",			MODE_12x,		},
	{"double-to-float",			MODE_12x,		},
	{"int-to-byte",				MODE_12x,		},
	{"int-to-char",				MODE_12x,		},
	{"int-to-short",			MODE_12x,		},
	{"add-int",					MODE_23x,		},
	{"sub-int",					MODE_23x,		},
	{"mul-int",					MODE_23x,		},
	{"div-int",					MODE_23x,		},
	{"rem-int",					MODE_23x,		},
	{"and-int",					MODE_23x,		},
	{"or-int",					MODE_23x,		},
	{"xor-int",					MODE_23x,		},
	{"shl-int",					MODE_23x,		},
	{"shr-int",					MODE_23x,		},
	{"ushr-int",				MODE_23x,		},
	{"add-long",				MODE_23x,		},
	{"sub-long",				MODE_23x,		},
	{"mul-long",				MODE_23x,		},
	{"div-long",				MODE_23x,		},
	{"rem-long",				MODE_23x,		},
	{"and-long",				MODE_23x,		},
	{"or-long",					MODE_23x,		},
	{"xor-long",				MODE_23x,		},
	{"shl-long",				MODE_23x,		},
	{"shr-long",				MODE_23x,		},
	{"ushr-long",				MODE_23x,		},
	{"add-float",				MODE_23x,		},
	{"sub-float",				MODE_23x,		},
	{"mul-float",				MODE_23x,		},
	{"div-float",				MODE_23x,		},
	{"rem-float",				MODE_23x,		},
	{"add-double",				MODE_23x,		},
	{"sub-double",				MODE_23x,		},
	{"mul-double",				MODE_23x,		},
	{"div-double",				MODE_23x,		},
	{"rem-double",				MODE_23x,		},
	{"add-int/2addr",			MODE_12x,		},
	{"sub-int/2addr",			MODE_12x,		},
	{"mul-int/2addr",			MODE_12x,		},
	{"div-int/2addr",			MODE_12x,		},
	{"rem-int/2addr",			MODE_12x,		},
	{"and-int/2addr",			MODE_12x,		},
	{"or-int/2addr",			MODE_12x,		},
	{"xor-int/2addr",			MODE_12x,		},
	{"shl-int/2addr",			MODE_12x,		},
	{"shr-int/2addr",			MODE_12x,		},
	{"ushr-int/2addr",			MODE_12x,		},
	{"add-long/2addr",			MODE_12x,		},
	{"sub-long/2addr",			MODE_12x,		},
	{"mul-long/2addr",			MODE_12x,		},
	{"div-long/2addr",			MODE_12x,		},
	{"rem-long/2addr",			MODE_12x,		},
	{"and-long/2addr",			MODE_12x,		},
	{"or-long/2addr",			MODE_12x,		},
	{"xor-long/2addr",			MODE_12x,		},
	{"shl-long/2addr",			MODE_12x,		},
	{"shr-long/2addr",			MODE_12x,		},
	{"ushr-long/2addr",			MODE_12x,		},
	{"add-float/2addr",			MODE_12x,		},
	{"sub-float/2addr",			MODE_12x,		},
	{"mul-float/2addr",			MODE_12x,		},
	{"div-float/2addr",			MODE_12x,		},
	{"rem-float/2addr",			MODE_12x,		},
	{"add-double/2addr",		MODE_12x,		},
	{"sub-double/2addr",		MODE_12x,		},
	{"mul-double/2addr",		MODE_12x,		},
	{"div-double/2addr",		MODE_12x,		},
	{"rem-double/2addr",		MODE_12x,		},
	{"add-int/lit16",			MODE_22s,		},
	{"rsub-int",				MODE_22s,		},
	{"mul-int/lit16",			MODE_22s,		},
	{"div-int/lit16",			MODE_22s,		},
	{"rem-int/lit16",			MODE_22s,		},
	{"and-int/lit16",			MODE_22s,		},
	{"or-int/lit16",			MODE_22s,		},
	{"xor-int/lit16",			MODE_22s,		},
	{"add-int/lit8",			MODE_22b,		},
	{"rsub-int/lit8",			MODE_22b,		},
	{"mul-int/lit8",			MODE_22b,		},
	{"div-int/lit8",			MODE_22b,		},
	{"rem-int/lit8",			MODE_22b,		},
	{"and-int/lit8",			MODE_22b,		},
	{"or-int/lit8",				MODE_22b,		},
	{"xor-int/lit8",			MODE_22b,		},
	{"shl-int/lit8",			MODE_22b,		},
	{"shr-int/lit8",			MODE_22b,		},
	{"ushr-int/lit8",			MODE_22b,		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
	{"invoke-polymorphic",		MODE_45cc,		},
	{"invoke-polymorphic/range",MODE_4rcc,		},
	{"invoke-custom ",			MODE_35c,		},
	{"invoke-custom/range ",	MODE_3rc,		},
	{0,							MODE_10x		},
	{0,							MODE_10x		},
};

struct Instruction {
	union {
		uint16	u;
		//10x		op
		//20t		op +AAAA												goto/16
		//30t		op +AAAAAAAA											goto/32
		//32x		op vAAAA, vBBBB
		struct { uint16	op:8, :8;	};
		//12x		op vA, vB
		//11n		op vA, #+B
		//22t		op vA, vB, +CCCC
		//22s		op vA, vB, #+CCCC
		//22c		op vA, vB, type@CCCC
		//			op vA, vB, field@CCCC									instance-of
		//22cs		op vA, vB, fieldoff@CCCC
		struct { uint16	:8, a:4, b:4;	};
		//11x		op vAA
		//10t		op +AA													goto
		//20bc		op AA, kind@BBBB
		//22x		op vAA, vBBBB
		//21t		op vAA, +BBBB
		//21s		op vAA, #+BBBB
		//21h		op vAA, #+BBBB0000
		//			op vAA, #+BBBB000000000000
		//21c		op vAA, type@BBBB										check-cast
		//			op vAA, field@BBBB										const-class
		//			op vAA, string@BBBB										const-string
		//23x		op vAA, vBB, vCC
		//22b		op vAA, vBB, #+CC
		//31i		op vAA, #+BBBBBBBB
		//31t		op vAA, +BBBBBBBB
		//31c		op vAA, string@BBBBBBBB									const-string/jumbo
		//3rc		op {vCCCC .. vNNNN}, meth@BBBB
		//			op {vCCCC .. vNNNN}, site@BBBB
		//			op {vCCCC .. vNNNN}, type@BBBB
		//3rms		op {vCCCC .. vNNNN}, vtaboff@BBBB
		//3rmi		op {vCCCC .. vNNNN}, inline@BBBB
		//4rcc		op> {vCCCC .. vNNNN}, meth@BBBB, proto@HHHH
		//51l		op vAA, #+BBBBBBBBBBBBBBBB								const-wide
		struct { uint16	:8, aa:8;	};
		//35c		[A=5] op {vC, vD, vE, vF, vG}, meth@BBBB
		//			[A=5] op {vC, vD, vE, vF, vG}, site@BBBB
		//			[A=5] op {vC, vD, vE, vF, vG}, type@BBBB
		//			[A=4] op {vC, vD, vE, vF}, kind@BBBB
		//			[A=3] op {vC, vD, vE}, kind@BBBB
		//			[A=2] op {vC, vD}, kind@BBBB
		//			[A=1] op {vC}, kind@BBBB
		//			[A=0] op {}, kind@BBBB
		//35ms		[A=5] op {vC, vD, vE, vF, vG}, vtaboff@BBBB
		//			[A=4] op {vC, vD, vE, vF}, vtaboff@BBBB
		//			[A=3] op {vC, vD, vE}, vtaboff@BBBB
		//			[A=2] op {vC, vD}, vtaboff@BBBB
		//			[A=1] op {vC}, vtaboff@BBBB
		//35mi		[A=5] op {vC, vD, vE, vF, vG}, inline@BBBB
		//			[A=4] op {vC, vD, vE, vF}, inline@BBBB
		//			[A=3] op {vC, vD, vE}, inline@BBBB
		//			[A=2] op {vC, vD}, inline@BBBB
		//			[A=1] op {vC}, inline@BBBB
		//45cc		[A=5] op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH
		//			[A=4] op {vC, vD, vE, vF}, meth@BBBB, proto	HHHH
		//			[A=3] op {vC, vD, vE}, meth@BBBB, proto	HHHH
		//			[A=2] op {vC, vD}, meth@BBBB, proto	HHHH
		//			[A=1] op {vC}, meth@BBBB, proto	HHHH					invoke-polymorphic
		struct { uint16	:8, g:4, num:4;	};
	};

	union {
		uint16	u;
		int16	i;
		struct { uint16	bb:8, cc:8;	};
	} u2;

	union {
		uint16	u;
		struct { uint16	c:4, d:4, e:4, f:4;	};
	} u3;
	uint16	u4, u5;
	uint32	u32() const { return u2.u + (u3.u << 16); }
	int32	i32() const { return int32(u32()); }
	uint32	u64() const { return u2.u + (u3.u << 16) + (uint64(u4 + (u5 << 16)) << 32); }
};

class DisassemblerDalvik : public Disassembler {
	static string_accum&	reg(string_accum &a, int r)			{ return a << 'v' << r; }
	static string_accum&	imm(string_accum &a, int r)			{ return a << '#' << r; }
	static string_accum&	branch(string_accum &a, uint32 r)	{ return a << "0x" << hex(r); }
public:
	const char*	GetDescription() override { return "Dalvik bytecode"; }
	State*		Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) override;
} dalvikbytecode;

Disassembler::State *DisassemblerDalvik::Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) {
	static const char *kinds[] = {
		"type",
		"field",
		"string",
		"meth",
		"site",
	};

	StateDefault		*state	= new StateDefault;
	const uint16		*p		= block;

	while (p < block.end()) {
		const Instruction	*in		= (const Instruction*)p;
		auto				mode	= opcodes[in->op].mode;
		uint32				len		= mode_lengths[mode & 0x1f];
		uint32				offset	= (uint8*)p - block;

		buffer_accum<1024>	ba("%08x ", offset);

		for (int i = 0; i < len; i++)
			ba.format("%04x ", p[i]);
		for (int i = len; i < 6; i++)
			ba << "     ";

		ba << opcodes[in->op].mnemonic;

		int	kind = mode / MODE_kind;
		switch (mode & 0x1f) {
			case MODE_10x:
				break;
			case MODE_12x:
				reg(reg(ba << ' ', in->a) << ", ", in->b);
				break;
			case MODE_11n:
				imm(reg(ba << ' ', in->a) << ", ", in->b);
				break;
			case MODE_11x:
				reg(ba << ' ', in->aa);
				break;
			case MODE_10t:
				branch(ba << ' ', offset + int8(in->aa) * 2);
				break;
			case MODE_20t:
				branch(ba << ' ', offset + in->u2.u * 2);
				break;
			case MODE_20bc:
				ba << ' ' << in->aa << ", " << kinds[kind] << '@' << in->u2.u;
				break;
			case MODE_22x:
				reg(reg(ba << ' ', in->aa) << ", ", in->u2.u);
				break;
			case MODE_21t:
				branch(reg(ba << ' ', in->aa) << ", ", offset + in->u2.i * 2);
				break;
			case MODE_21s:
				imm(reg(ba << ' ', in->aa) << ", ", in->u2.u);
				break;
			case MODE_21h:
				imm(reg(ba << ' ', in->aa) << ", ", in->u2.u << 16);
				break;
			case MODE_21c:
				reg(ba << ' ', in->aa) << ", " << kinds[kind] << '@' << in->u2.u;
				break;
			case MODE_23x:
				reg(reg(reg(ba << ' ', in->aa) << ", ", in->u2.bb) << ", ", in->u2.cc);
				break;
			case MODE_22b:
				imm(reg(reg(ba << ' ', in->aa) << ", ", in->u2.bb) << ", ", in->u2.cc);
				break;
			case MODE_22t:
				branch(reg(reg(ba << ' ', in->a) << ", ", in->b) << ", ", offset + in->u2.i * 2);
				break;
			case MODE_22s:
				imm(reg(reg(ba << ' ', in->a) << ", ", in->b) << ", ", in->u2.u);
				break;
			case MODE_22c:
				reg(reg(ba << ' ', in->a) << ", ", in->b) << ", " << kinds[kind] << '@' << in->u2.u;
				break;
			case MODE_30t:
				branch(ba << ' ', offset + in->i32() * 2);
				break;
			case MODE_32x:
				reg(reg(ba << ' ', in->u2.u) << ", ", in->u3.u);
				break;
			case MODE_31i:
				imm(reg(ba << ' ', in->aa) << ", ", in->u32());
				break;
			case MODE_31t:
				branch(reg(ba << ' ', in->aa) << ", ", offset + in->i32() * 2);
				break;
			case MODE_31c:
				reg(ba << ' ', in->aa) << ", " << kinds[kind] << '@' << in->u32();
				break;
			case MODE_35c:
				ba << " {";
				if (in->num > 0)
					reg(ba, in->u3.c);
				if (in->num > 1)
					reg(ba << ", ", in->u3.d);
				if (in->num > 2)
					reg(ba << ", ", in->u3.e);
				if (in->num > 3)
					reg(ba << ", ", in->u3.f);
				if (in->num > 4)
					reg(ba << ", ", in->g);
				ba << "}, " << kinds[kind] << '@' << in->u2.u;
				break;
			case MODE_3rc:
				reg(reg(ba << " {", in->u3.u) << "..", in->u3.u + in->aa - 1) << "}, " << kinds[kind] << '@' << in->u2.u;
				break;
				break;
			case MODE_45cc:
				ba << " {";
				if (in->num > 0)
					reg(ba, in->u3.c);
				if (in->num > 1)
					reg(ba << ", ", in->u3.d);
				if (in->num > 2)
					reg(ba << ", ", in->u3.e);
				if (in->num > 3)
					reg(ba << ", ", in->u3.f);
				if (in->num > 4)
					reg(ba << ", ", in->g);
				ba << "}, meth@" << in->u2.u << ", proto@" << in->u4;
				break;
			case MODE_4rcc:
				reg(reg(ba << " {", in->u3.u) << "..", in->u3.u + in->aa - 1) << "}, meth@" << in->u2.u << ", proto@" << in->u4;
				break;
			case MODE_51l:
				reg(ba << " ", in->aa) << ", #" << in->u64();
		}

		p += len;
		state->lines.push_back(ba);
	}
	return state;
}

#endif	//ISO_EDITOR

