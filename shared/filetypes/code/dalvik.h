#ifndef DALVIK_H
#define DALVIK_H

#include "base/defs.h"
#include "base/array.h"
#include "base/pointer.h"
#include "comms/leb128.h"
#include "stream.h"

namespace dalvik {
using namespace iso;

struct DEXheader;

typedef leb128<uint32>	uleb128;
typedef leb128<int32>	sleb128;

struct uleb128p1 : uleb128 {
	uleb128p1(int32 t) : uleb128(uint32(t + 1)) {}
	operator int32() const { return (int32)t - 1; }
};

template<typename T, typename O = uint32> using offset = offset_pointer<T, O, DEXheader, true>;

template<typename T, typename C = uint32> struct index {
	C			idx;
	operator bool() const { return !!C(~idx); }
};

static const uint32 NO_INDEX = 0xffffffff;

enum ACCESS_FLAGS {
	//Name						Value		For Classes (and InnerClass annotations)					For Fields													For Methods
	ACC_PUBLIC					= 0x1,		//public: visible everywhere								public: visible everywhere									public: visible everywhere
	ACC_PRIVATE					= 0x2,		//* private: only visible to defining class					private: only visible to defining class						private: only visible to defining class
	ACC_PROTECTED				= 0x4,		//* protected: visible to package and subclasses			protected: visible to package and subclasses				protected: visible to package and subclasses
	ACC_STATIC					= 0x8,		//* static: is not constructed with an outer this reference	static: global to defining class							static: does not take a this argument
	ACC_FINAL					= 0x10,		//final: not subclassable									final: immutable after construction							final: not overridable
	ACC_SYNCHRONIZED			= 0x20,		//																														synchronized: associated lock automatically acquired around call to this method. Note: This is on= y valid to set when ACC_NATIVE is also set.
	ACC_VOLATILE				= 0x40,		//															volatile: special access rules to help with thread safety
	ACC_BRIDGE					= 0x40,		//																														bridge method, added automatically by compiler as a type-safe bridge
	ACC_TRANSIENT				= 0x80,		//															transient: not to be saved by default serialization
	ACC_VARARGS					= 0x80,		//																														last argument should be treated as a "rest" argument by compiler
	ACC_NATIVE					= 0x100,	//																														native: implemented in native code
	ACC_INTERFACE				= 0x200,	//interface: multiply-implementable abstract class
	ACC_ABSTRACT				= 0x400,	//abstract: not directly instantiable																					abstract: unimplemented by this class
	ACC_STRICT					= 0x800,	//																														strictfp: strict rules for floating-point arithmetic
	ACC_SYNTHETIC				= 0x1000,	//not directly defined in source code						not directly defined in source code							not directly defined in source code
	ACC_ANNOTATION				= 0x2000,	//declared as an annotation class
	ACC_ENUM					= 0x4000,	//declared as an enumerated type							declared as an enumerated value
	ACC_MANDATED				= 0x8000,	//the parameter is synthetic but also implied by the language specification ** ONLY in dalvik.annotation.MethodParameters **
	ACC_CONSTRUCTOR				= 0x10000,	//																														constructor method (class or instance initializer)
	ACC_DECLARED_SYNCHRONIZED	= 0x20000,	//																														declared synchronized. Note: This has no effect on execution (other than in reflection of this flag, per se).
};

struct encoded_value {
	enum VALUE_TYPE {
		//Type Name				value_type	value_arg Format	value Format		Description
		VALUE_BYTE				= 0x00,		//(none; must be 0)	ubyte[1]			signed one-byte integer value
		VALUE_SHORT				= 0x02,		//size - 1 (0…1)	ubyte[size]			signed two-byte integer value, sign-extended
		VALUE_CHAR				= 0x03,		//size - 1 (0…1)	ubyte[size]			unsigned two-byte integer value, zero-extended
		VALUE_INT				= 0x04,		//size - 1 (0…3)	ubyte[size]			signed four-byte integer value, sign-extended
		VALUE_LONG				= 0x06,		//size - 1 (0…7)	ubyte[size]			signed eight-byte integer value, sign-extended
		VALUE_FLOAT				= 0x10,		//size - 1 (0…3)	ubyte[size]			four-byte bit pattern, zero-extended to the right, and interpreted as an IEEE754 32-bit floating point value
		VALUE_DOUBLE			= 0x11,		//size - 1 (0…7)	ubyte[size]			eight-byte bit pattern, zero-extended to the right, and interpreted as an IEEE754 64-bit floating point value
		VALUE_METHOD_TYPE		= 0x15,		//size - 1 (0…3)	ubyte[size]			unsigned (zero-extended) four-byte integer value, interpreted as an index into the proto_ids section and representing a method type value
		VALUE_METHOD_HANDLE		= 0x16,		//size - 1 (0…3)	ubyte[size]			unsigned (zero-extended) four-byte integer value, interpreted as an index into the method_handles section and representing a method handle value
		VALUE_STRING			= 0x17,		//size - 1 (0…3)	ubyte[size]			unsigned (zero-extended) four-byte integer value, interpreted as an index into the string_ids section and representing a string value
		VALUE_TYPE				= 0x18,		//size - 1 (0…3)	ubyte[size]			unsigned (zero-extended) four-byte integer value, interpreted as an index into the type_ids section and representing a reflective type/class value
		VALUE_FIELD				= 0x19,		//size - 1 (0…3)	ubyte[size]			unsigned (zero-extended) four-byte integer value, interpreted as an index into the field_ids section and representing a reflective field value
		VALUE_METHOD			= 0x1a,		//size - 1 (0…3)	ubyte[size]			unsigned (zero-extended) four-byte integer value, interpreted as an index into the method_ids section and representing a reflective method value
		VALUE_ENUM				= 0x1b,		//size - 1 (0…3)	ubyte[size]			unsigned (zero-extended) four-byte integer value, interpreted as an index into the field_ids section and representing the value of an enumerated type constant
		VALUE_ARRAY				= 0x1c,		//(none; must be 0)	encoded_array		an array of values, in the format specified by "encoded_array format" below. The size of the value is implicit in the encoding.
		VALUE_ANNOTATION		= 0x1d,		//(none; must be 0)	encoded_annotation	a sub-annotation, in the format specified by "encoded_annotation format" below. The size of the value is implicit in the encoding.
		VALUE_NULL				= 0x1e,		//(none; must be 0)	(none)				null reference value
		VALUE_BOOLEAN			= 0x1f,		//boolean (0…1)		(none)				one-bit value; 0 for false and 1 for true. The bit is represented in the value_arg.
	};
	union {
		struct { uint8	type:5, arg:3; };
		uint8	byte;
	};
};

typedef embedded_array<encoded_value, uleb128::raw>	encoded_array;
typedef encoded_array			encoded_array_item;

enum DESCRIPTOR {
	DESC_VOID		= 'V',	//void
	DESC_BYTE		= 'B',	//signed byte
	DESC_CHAR		= 'C',	//Unicode character code point in the Basic Multilingual Plane, encoded with UTF-16
	DESC_DOUBLE		= 'D',	//double-precision floating-point value
	DESC_FLOAT		= 'F',	//single-precision floating-point value
	DESC_INT		= 'I',	//integer
	DESC_LONG		= 'J',	//long integer
	DESC_REFERENCE	= 'L',	//reference	(followed by '<ClassName>;')
	DESC_SHORT		= 'S',	//signed short
	DESC_BOOLEAN	= 'Z',	//true or false
	DESC_ARRAY		= '[',	//reference	one array dimension
};

struct map_item {
	enum TYPE {									//									item size in bytes
		HEADER_ITEM					= 0x0000,	//header_item						0x70
		STRING_ID_ITEM				= 0x0001,	//string_id_item					0x04
		TYPE_ID_ITEM				= 0x0002,	//type_id_item						0x04
		PROTO_ID_ITEM				= 0x0003,	//proto_id_item						0x0c
		FIELD_ID_ITEM				= 0x0004,	//field_id_item						0x08
		METHOD_ID_ITEM				= 0x0005,	//method_id_item					0x08
		CLASS_DEF_ITEM				= 0x0006,	//class_def_item					0x20
		CALL_SITE_ID_ITEM			= 0x0007,	//call_site_id_item					0x04
		METHOD_HANDLE_ITEM			= 0x0008,	//method_handle_item				0x08
		MAP_LIST					= 0x1000,	//map_list							4 + (item.size * 12)
		TYPE_LIST					= 0x1001,	//type_list							4 + (item.size * 2)
		ANNOTATION_SET_REF_LIST		= 0x1002,	//annotation_set_ref_list			4 + (item.size * 4)
		ANNOTATION_SET_ITEM			= 0x1003,	//annotation_set_item				4 + (item.size * 4)
		CLASS_DATA_ITEM				= 0x2000,	//class_data_item					implicit; must parse
		CODE_ITEM					= 0x2001,	//code_item							implicit; must parse
		STRING_DATA_ITEM			= 0x2002,	//string_data_item					implicit; must parse
		DEBUG_INFO_ITEM				= 0x2003,	//debug_info_item					implicit; must parse
		ANNOTATION_ITEM				= 0x2004,	//annotation_item					implicit; must parse
		ENCODED_ARRAY_ITEM			= 0x2005,	//encoded_array_item				implicit; must parse
		ANNOTATIONS_DIRECTORY_ITEM	= 0x2006,	//annotations_directory_item		implicit; must parse
	};
	uint16		type;				//type of the items; see table below
	uint16		unused;
	uint32		size;				//count of the number of items to be found at the indicated offset
	uint32		offset;				//offset from the start of the file to the items in question
};
typedef embedded_array<map_item, uint32> map_list;

enum DBG {
	DBG_END_SEQUENCE		= 0x00,		//@(none)@terminates a debug info sequence for a code_item
	DBG_ADVANCE_PC			= 0x01,		//uleb128 addr_diff@addr_diff: amount to add to address register@advances the address register without emitting a positions entry
	DBG_ADVANCE_LINE		= 0x02,		//sleb128 line_diff@line_diff: amount to change line register by@advances the line register without emitting a positions entry
	DBG_START_LOCAL			= 0x03,		//uleb128 register_num
	DBG_END_LOCAL			= 0x05,		//uleb128 register_num@register_num: register that contained local@marks a currently-live local variable as out of scope at the current address
	DBG_RESTART_LOCAL		= 0x06,		//uleb128 register_num@register_num: register to restart@re-introduces a local variable at the current address. The name and type are the same as the last local that was live in the specified register.
	DBG_SET_PROLOGUE_END	= 0x07,		//@(none)@sets the prologue_end state machine register, indicating that the next position entry that is added should be considered the end of a method prologue (an appropriate place for a method breakpoint). The prologue_end register is cleared by any special (>= 0x0a) opcode.
	DBG_SET_EPILOGUE_BEGIN	= 0x08,		//@(none)@sets the epilogue_begin state machine register, indicating that the next position entry that is added should be considered the beginning of a method epilogue (an appropriate place to suspend execution before method exit). The epilogue_begin register is cleared by any special (>= 0x0a) opcode.
	DBG_SET_FILE			= 0x09,		//uleb128p1 name_idx@name_idx: string index of source file name; NO_INDEX if unknown @indicates that all subsequent line number entries make reference to this source file name, instead of the default name specified in code_item

	//Opcodes with values between 0x0a and 0xff (inclusive) move both the line and address registers by a small amount and then emit a new position table entry. The formula for the increments are as follows:
	//adjusted_opcode = opcode - DBG_FIRST_SPECIAL
	//line += DBG_LINE_BASE + (adjusted_opcode % DBG_LINE_RANGE)
	//address += (adjusted_opcode / DBG_LINE_RANGE)
	DBG_FIRST_SPECIAL		= 0x0a,		// the smallest special opcode
	DBG_LINE_BASE			= -4,		// the smallest line number increment
	DBG_LINE_RANGE			= 15,		// the number of line increments represented
};

enum VISIBILITY {
	VISIBILITY_BUILD			= 0x00,	//intended only to be visible at build time (e.g., during compilation of other code)
	VISIBILITY_RUNTIME			= 0x01,	//intended to visible at runtime
	VISIBILITY_SYSTEM			= 0x02,	//intended to visible at runtime, but only to the underlying system (and not to regular user code)
};

#if 1
struct string_data_item : embedded_array<uint8, uleb128::raw> {};
#else
struct string_data_item {
	uleb128		utf16_size;											//size of this string, in UTF-16 code units (which is the "string length" in many systems). That is, this is the decoded length of the string. (The encoded length is implied by the position of the 0 byte.)
	//uint8		data[1];											//a series of MUTF-8 code units (a.k.a. octets, a.k.a. bytes) followed by a byte of value 0. See "MUTF-8 (Modified UTF-8) Encoding" above for details and discussion about the data format.
	//Note: It is acceptable to have a string which includes (the encoded form of) UTF-16 surrogate code units (that is, U+d800 … U+dfff) either in isolation or out-of-order with respect to the usual encoding of Unicode into UTF-16. It is up to higher-level uses of strings to reject such invalid encodings, if appropriate.
};
#endif
typedef offset<string_data_item>	string_id_item;

struct type_id_item {
	index<string_id_item, uint32>	descriptor;						//index into the string_ids list for the descriptor string of this type. The string must conform to the syntax for TypeDescriptor, defined above.
};

typedef index<type_id_item, uint16>	type_item;						//index into the type_ids list
typedef embedded_array<type_item, uint32>	type_list;

struct proto_id_item {
	index<string_id_item, uint32>	shorty;							//index into the string_ids list for the short-form descriptor string of this prototype. The string must conform to the syntax for ShortyDescriptor, defined above, and must correspond to the return type and parameters of this item.
	index<type_id_item, uint32>		return_type;					//index into the type_ids list for the return type of this prototype
	offset<type_list, uint32>		parameters;						//offset from the start of the file to the list of parameter types for this prototype, or 0 if this prototype has no parameters. This offset, if non-zero, should be in the data section, and the data there should be in the format specified by "type_list" below. Additionally, there should be no reference to the type void in the list.
};
struct field_id_item {
	index<type_id_item, uint16>		klass;							//index into the type_ids list for the definer of this field. This must be a class type, and not an array or primitive type.
	index<type_id_item, uint16>		type;							//index into the type_ids list for the type of this field
	index<string_id_item, uint32>	name;							//index into the string_ids list for the name of this field. The string must conform to the syntax for MemberName, defined above.
};
struct method_id_item {
	index<type_id_item, uint16>		klass;							//index into the type_ids list for the definer of this method. This must be a class or array type, and not a primitive type.
	index<proto_id_item, uint16>	proto;							//index into the proto_ids list for the prototype of this method
	index<string_id_item, uint32>	name;							//index into the string_ids list for the name of this method. The string must conform to the syntax for MemberName, defined above.
};

typedef encoded_array_item		call_site_item;
typedef offset<call_site_item>	call_site_id_item;

struct method_handle_item {
	enum TYPE {
		STATIC_PUT		= 0,	//static field setter (accessor)
		STATIC_GET		= 1,	//static field getter (accessor)
		INSTANCE_PUT	= 2,	//instance field setter (accessor)
		INSTANCE_GET	= 3,	//instance field getter (accessor)
		INVOKE_STATIC	= 4,	//static method invoker
		INVOKE_INSTANCE	= 5,	//instance method invoker
	};
	uint16		method_handle_type;
	uint16		unused1;
	uint16		field_or_method_id;
	uint16		unused2;
};

struct debug_info_item {
	uleb128						line_start;							//the initial value for the state machine's line register. Does not represent an actual positions entry.
	embedded_array<uleb128p1, uleb128>	parameter_names;			//string index of the method parameter name. An encoded value of NO_INDEX indicates that no name is available for the associated parameter. The type descriptor and signature are implied from the method descriptor and signature.
};

struct code_item {
	struct encoded_catch_handler {
		struct encoded_type_addr_pair {
			index<type_id_item, uleb128>	type;					//index into the type_ids list for the type of the exception to catch
			uleb128							addr;					//bytecode address of the associated exception handler
		};
		sleb128		size;											//number of catch types in this list. If non-positive, then this is the negative of the number of catch types, and the catches are followed by a catch-all handler. For example: A size of 0 means that there is a catch-all but no explicitly typed catches. A size of 2 means that there are two explicitly typed catches and no catch-all. And a size of -1 means that there is one typed catch along with a catch-all.
//		dynamic_array<encoded_type_addr_pair>	handlers;			//[abs(size)];			//stream of abs(size) encoded items, one for each caught type, in the order that the types should be tested.
		uleb128		catch_all_addr;									//(optional)bytecode address of the catch-all handler. This element is only present if size is non-positive.
	};
	struct try_item {
		uint32		start_addr;										//start address of the block of code covered by this entry. The address is a count of 16-bit code units to the start of the first covered instruction.
		uint16		insn_count;										//number of 16-bit code units covered by this entry. The last code unit covered (inclusive) is start_addr + insn_count - 1.
		offset<encoded_catch_handler, uint16>	handler;			//offset in bytes from the start of the associated encoded_catch_hander_list to the encoded_catch_handler for this entry. This must be an offset to the start of an encoded_catch_handler.
	};

	uint16		registers_size;										//the number of registers used by this code
	uint16		ins_size;											//the number of words of incoming arguments to the method that this code is for
	uint16		outs_size;											//the number of words of outgoing argument space required by this code for method invocation
	uint16		tries_size;											//the number of try_items for this instance. If non-zero, then these appear as the tries array just after the insns in this instance.
	offset<debug_info_item, uint32>	debug_info;						//offset from the start of the file to the debug info (line numbers + local variable info) sequence for this code, or 0 if there simply is no information. The offset, if non-zero, should be to a location in the data section. The format of the data is specified by "debug_info_item" below.
	uint32		insns_size;											//size of the instructions list, in 16-bit code units
//	dynamic_array<uint16>	insns;	//[insns_size];					//actual array of bytecode. The format of code in an insns array is specified by the companion document Dalvik bytecode. Note that though this is defined as an array of uint16, there are some internal structures that prefer four-byte alignment. Also, if this happens to be in an endian-swapped file, then the swapping is only done on individual ushorts and not on the larger internal structures.
//	uint16		padding;											//(optional)two bytes of padding to make tries four-byte aligned. This element is only present if tries_size is non-zero and insns_size is odd.
//	dynamic_array<try_item>	tries;	//[tries_size];					//(optional)array indicating where in the code exceptions are caught and how to handle them. Elements of the array must be non-overlapping in range and in order from low to high address. This element is only present if tries_size is non-zero.
//	embedded_array<encoded_catch_handler, uleb128>	handlers;		//(optional)bytes representing a list of lists of catch types and associated handler addresses. Each try_item has a byte-wise offset into this structure. This element is only present if tries_size is non-zero.
};

struct class_data_item {
	struct encoded_field {
		index<field_id_item, uleb128>	field_diff;		//index into the field_ids list for the identity of this field (includes the name and descriptor), represented as a difference from the index of previous element in the list. The index of the first element in a list is represented directly.
		uleb128							access_flags;	//access flags for the field (public, final, etc.). See "access_flags Definitions" for details.
	};
	struct encoded_method {
		index<method_id_item, uleb128>	method_diff;	//index into the method_ids list for the identity of this method (includes the name and descriptor), represented as a difference from the index of previous element in the list. The index of the first element in a list is represented directly.
		uleb128							access_flags;	//access flags for the method (public, final, etc.). See "access_flags Definitions" for details.
		offset<code_item, uleb128>		code;
	};

	uleb128		static_fields_size;
	uleb128		instance_fields_size;
	uleb128		direct_methods_size;
	uleb128		virtual_methods_size;
	//dynamic_array<encoded_field>	static_fields;//[static_fields_size];			must be sorted by field_idx in increasing order
	//dynamic_array<encoded_field>	instance_fields;//[instance_fields_size];		must be sorted by field_idx in increasing order
	//dynamic_array<encoded_method>	direct_methods;//[direct_methods_size];			must be sorted by method_idx in increasing order
	//dynamic_array<encoded_method>	virtual_methods;//[virtual_methods_size];		This list should not include inherited methods unless overridden by the class that this item represents. The methods must be sorted by method_idx in increasing order. The method_idx of a virtual method must not be the same as any direct method.
};

struct annotation_element {
	index<string_id_item, uleb128>		name;		//element name, represented as an index into the string_ids section. The string must conform to the syntax for MemberName, defined above.
	encoded_value						value;
};
struct encoded_annotation {
	index<type_id_item, uleb128>		type;		//type of the annotation. This must be a class (not array or primitive) type.
	embedded_array<annotation_element, uleb128>	elements;	//elements of the annotation, represented directly in-line (not as offsets). Elements must be sorted in increasing order by string_id index.
};

struct annotation_item {
	uint8				visibility;
	encoded_annotation	annotation;
};
typedef offset<annotation_item, uint32>			annotation_off_item;
typedef embedded_array<annotation_off_item, uint32>		annotation_set_item;

typedef offset<annotation_set_item, uint32>		annotation_set_ref_item;
typedef embedded_array<annotation_set_ref_item, uint32>	annotation_set_ref_list;

struct annotations_directory_item {
	struct field_annotation {
		index<field_id_item, uint32>			field;			//index into the field_ids list for the identity of the field being annotated
		offset<annotation_set_item, uint32>		annotations;
	};
	struct method_annotation {
		index<method_id_item, uint32>			method;			//index into the method_ids list for the identity of the method being annotated
		offset<annotation_set_item, uint32>		annotations;
	};
	struct parameter_annotation {
		index<method_id_item, uint32>			method;			//index into the method_ids list for the identity of the method whose parameters are being annotated
		offset<annotation_set_ref_list, uint32>	annotations;
	};

	offset<annotation_set_item, uint32>	class_annotations;		//offset from the start of the file to the annotations made directly on the class, or 0 if the class has no direct annotations. The offset, if non-zero, should be to a location in the data section. The format of the data is specified by "annotation_set_item" below.
	uint32					fields_size;				//count of fields annotated by this item
	uint32					annotated_methods_size;		//count of methods annotated by this item
	uint32					annotated_parameters_size;	//count method parameter lists annotated by this item
//	dynamic_array<field_annotation>		field_annotations;		//[fields_size];		//(optional)list of associated field annotations. The elements of the list must be sorted in increasing order, by field_idx.
//	dynamic_array<method_annotation>	method_annotations;		//[methods_size];		//(optional)list of associated method annotations. The elements of the list must be sorted in increasing order, by method_idx.
//	dynamic_array<parameter_annotation>	parameter_annotations;	//[parameters_size];	//(optional)list of associated method parameter annotations. The elements of the list must be sorted in increasing order, by method_idx.
};
struct class_def_item {
	index<type_id_item, uint32>					klass;				//index into the type_ids list for this class. This must be a class type, and not an array or primitive type.
	uint32										access_flags;		//access flags for the class (public, final, etc.). See "access_flags Definitions" for details.
	index<type_id_item, uint32>					superclass;			//index into the type_ids list for the superclass, or the constant value NO_INDEX if this class has no superclass (i.e., it is a root class such as Object). If present, this must be a class type, and not an array or primitive type.
	offset<type_list,uint32>					interfaces;			//offset from the start of the file to the list of interfaces, or 0 if there are none. This offset should be in the data section, and the data there should be in the format specified by "type_list" below. Each of the elements of the list must be a class type (not an array or primitive type), and there must not be any duplicates.
	index<string_id_item, uint32>				source_file;		//index into the string_ids list for the name of the file containing the original source for (at least most of) this class, or the special value NO_INDEX to represent a lack of this information. The debug_info_item of any given method may override this source file, but the expectation is that most classes will only come from one source file.
	offset<annotations_directory_item, uint32>	annotations;		//offset from the start of the file to the annotations structure for this class, or 0 if there are no annotations on this class. This offset, if non-zero, should be in the data section, and the data there should be in the format specified by "annotations_directory_item" below, with all items referring to this class as the definer.
	offset<class_data_item, uint32>				class_data;			//offset from the start of the file to the associated class data for this item, or 0 if there is no class data for this class. (This may be the case, for example, if this class is a marker interface.) The offset, if non-zero, should be in the data section, and the data there should be in the format specified by "class_data_item" below, with all items referring to this class as the definer.
	offset<encoded_array_item, uint32>			static_values;		//offset from the start of the file to the list of initial values for static fields, or 0 if there are none (and all static fields are to be initialized with 0 or null). This offset should be in the data section, and the data there should be in the format specified by "encoded_array_item" below. The size of the array must be no larger than the number of static fields declared by this class, and the elements correspond to the static fields in the same order as declared in the corresponding field_list. The type of each array element must match the declared type of its corresponding field. If there are fewer elements in the array than there are static fields, then the leftover fields are initialized with a type-appropriate 0 or null.
};

struct DEXheader {
	static const uint64 MAGIC_038		= 0x6465780a30333800; //"dex\n038\0"
	static const uint64 MAGIC_035		= 0x6465780a30333500; //"dex\n035\0"
	static const uint32 ENDIAN_CONSTANT = 0x12345678;

	uint64be				magic;
	uint32					checksum;			//adler32 checksum of the rest of the file (everything but magic and this field); used to detect file corruption
	uint8					signature[20];		//SHA-1 signature (hash) of the rest of the file (everything but magic, checksum, and this field); used to uniquely identify files
	uint32					file_size;			//size of the entire file (including the header), in bytes
	uint32					header_size;		//size of the header (this entire section), in bytes. This allows for at least a limited amount of backwards/forwards compatibility without invalidating the format.
	uint32					endian_tag;			//endianness tag. See discussion above under "ENDIAN_CONSTANT and REVERSE_ENDIAN_CONSTANT" for more details.
	uint32					link_size;			//size of the link section, or 0 if this file isn't statically linked
	uint32					link_off;			//offset from the start of the file to the link section, or 0 if link_size == 0. The offset, if non-zero, should be to an offset into the link_data section. The format of the data pointed at is left unspecified by this document; this header field (and the previous) are left as hooks for use by runtime implementations.
	offset<map_list>		map;
	uint32					string_ids_size;	//count of strings in the string identifiers list
	offset<string_id_item>	string_ids;
	uint32					type_ids_size;		//count of elements in the type identifiers list, at most 65535
	offset<type_id_item>	type_ids;
	uint32					proto_ids_size;		//count of elements in the prototype identifiers list, at most 65535
	offset<proto_id_item>	proto_ids;
	uint32					field_ids_size;		//count of elements in the field identifiers list
	offset<field_id_item>	field_ids;
	uint32					method_ids_size;	//count of elements in the method identifiers list
	offset<method_id_item>	method_ids;
	uint32					class_defs_size;	//count of elements in the class definitions list
	offset<class_def_item>	class_defs;
	uint32					data_size;			//Size of data section in bytes. Must be an even multiple of sizeof(uint32).
	uint32					data_off;			//offset from the start of the file to the start of the data section.

	bool valid() const {
		return magic == MAGIC_038 || magic == MAGIC_035;
	}
};

#if 0
//System annotations
namespace annotation {

struct AnnotationDefault {
	Annotation	value;												//the default bindings for this annotation, represented as an annotation of this type. The annotation need not include all names defined by the annotation; missing names simply do not have defaults.
};
struct EnclosingClass {
	Class		value;												//the class which most closely lexically scopes this class
};
struct EnclosingMethod {
	Method		value;												//the method which most closely lexically scopes this class
};
struct InnerClass {
	String		name;												//the originally declared simple name of this class (not including any package prefix). If this class is anonymous, then the name is null.
	int			accessFlags;										//the originally declared access flags of the class (which may differ from the effective flags because of a mismatch between the execution models of the source language and target virtual machine)
};
struct MemberClasses {
	Class		value[];											//array of the member classes
};

struct MethodParameters {
	String		names[];											//The names of formal parameters for the associated method. The array must not be null but must be empty if there are no formal parameters. A value in the array must be null if the formal parameter with that index has no name.
	int			accessFlags;										//The access flags of the formal parameters for the associated method. The array must not be null but must be empty if there are no formal parameters.
//	ACCESS_FLAGS, only final,synthetic, mandated. If any bits are set outside of this set then a java.lang.reflect.MalformedParametersException will be thrown at runtime.
};
struct Signature {
	String		value[];											//the signature of this class or member, as an array of strings that is to be concatenated together
};
struct Throws {
	Class		value[];											//the array of exception types thrown
};
}
#endif

enum Opcode {
	OP_NOP							= 0x00,
	OP_MOVE							= 0x01,
	OP_MOVE_FROM16					= 0x02,
	OP_MOVE_16						= 0x03,
	OP_MOVE_WIDE					= 0x04,
	OP_MOVE_WIDE_FROM16				= 0x05,
	OP_MOVE_WIDE_16					= 0x06,
	OP_MOVE_OBJECT					= 0x07,
	OP_MOVE_OBJECT_FROM16			= 0x08,
	OP_MOVE_OBJECT_16				= 0x09,
	OP_MOVE_RESULT					= 0x0a,
	OP_MOVE_RESULT_WIDE				= 0x0b,
	OP_MOVE_RESULT_OBJECT			= 0x0c,
	OP_MOVE_EXCEPTION				= 0x0d,
	OP_RETURN_VOID					= 0x0e,
	OP_RETURN						= 0x0f,
	OP_RETURN_WIDE					= 0x10,
	OP_RETURN_OBJECT				= 0x11,
	OP_CONST_4						= 0x12,
	OP_CONST_16						= 0x13,
	OP_CONST						= 0x14,
	OP_CONST_HIGH16					= 0x15,
	OP_CONST_WIDE_16				= 0x16,
	OP_CONST_WIDE_32				= 0x17,
	OP_CONST_WIDE					= 0x18,
	OP_CONST_WIDE_HIGH16			= 0x19,
	OP_CONST_STRING					= 0x1a,
	OP_CONST_STRING_JUMBO			= 0x1b,
	OP_CONST_CLASS					= 0x1c,
	OP_MONITOR_ENTER				= 0x1d,
	OP_MONITOR_EXIT					= 0x1e,
	OP_CHECK_CAST					= 0x1f,
	OP_INSTANCE_OF					= 0x20,
	OP_ARRAY_LENGTH					= 0x21,
	OP_NEW_INSTANCE					= 0x22,
	OP_NEW_ARRAY					= 0x23,
	OP_FILLED_NEW_ARRAY				= 0x24,
	OP_FILLED_NEW_ARRAY_RANGE		= 0x25,
	OP_FILL_ARRAY_DATA				= 0x26,
	OP_THROW						= 0x27,
	OP_GOTO							= 0x28,
	OP_GOTO_16						= 0x29,
	OP_GOTO_32						= 0x2a,
	OP_PACKED_SWITCH				= 0x2b,
	OP_SPARSE_SWITCH				= 0x2c,
	OP_CMPL_FLOAT					= 0x2d,
	OP_CMPG_FLOAT					= 0x2e,
	OP_CMPL_DOUBLE					= 0x2f,
	OP_CMPG_DOUBLE					= 0x30,
	OP_CMP_LONG						= 0x31,
	OP_IF_EQ						= 0x32,
	OP_IF_NE						= 0x33,
	OP_IF_LT						= 0x34,
	OP_IF_GE						= 0x35,
	OP_IF_GT						= 0x36,
	OP_IF_LE						= 0x37,
	OP_IF_EQZ						= 0x38,
	OP_IF_NEZ						= 0x39,
	OP_IF_LTZ						= 0x3a,
	OP_IF_GEZ						= 0x3b,
	OP_IF_GTZ						= 0x3c,
	OP_IF_LEZ						= 0x3d,
	OP_UNUSED_3E					= 0x3e,
	OP_UNUSED_3F					= 0x3f,
	OP_UNUSED_40					= 0x40,
	OP_UNUSED_41					= 0x41,
	OP_UNUSED_42					= 0x42,
	OP_UNUSED_43					= 0x43,
	OP_AGET							= 0x44,
	OP_AGET_WIDE					= 0x45,
	OP_AGET_OBJECT					= 0x46,
	OP_AGET_BOOLEAN					= 0x47,
	OP_AGET_BYTE					= 0x48,
	OP_AGET_CHAR					= 0x49,
	OP_AGET_SHORT					= 0x4a,
	OP_APUT							= 0x4b,
	OP_APUT_WIDE					= 0x4c,
	OP_APUT_OBJECT					= 0x4d,
	OP_APUT_BOOLEAN					= 0x4e,
	OP_APUT_BYTE					= 0x4f,
	OP_APUT_CHAR					= 0x50,
	OP_APUT_SHORT					= 0x51,
	OP_IGET							= 0x52,
	OP_IGET_WIDE					= 0x53,
	OP_IGET_OBJECT					= 0x54,
	OP_IGET_BOOLEAN					= 0x55,
	OP_IGET_BYTE					= 0x56,
	OP_IGET_CHAR					= 0x57,
	OP_IGET_SHORT					= 0x58,
	OP_IPUT							= 0x59,
	OP_IPUT_WIDE					= 0x5a,
	OP_IPUT_OBJECT					= 0x5b,
	OP_IPUT_BOOLEAN					= 0x5c,
	OP_IPUT_BYTE					= 0x5d,
	OP_IPUT_CHAR					= 0x5e,
	OP_IPUT_SHORT					= 0x5f,
	OP_SGET							= 0x60,
	OP_SGET_WIDE					= 0x61,
	OP_SGET_OBJECT					= 0x62,
	OP_SGET_BOOLEAN					= 0x63,
	OP_SGET_BYTE					= 0x64,
	OP_SGET_CHAR					= 0x65,
	OP_SGET_SHORT					= 0x66,
	OP_SPUT							= 0x67,
	OP_SPUT_WIDE					= 0x68,
	OP_SPUT_OBJECT					= 0x69,
	OP_SPUT_BOOLEAN					= 0x6a,
	OP_SPUT_BYTE					= 0x6b,
	OP_SPUT_CHAR					= 0x6c,
	OP_SPUT_SHORT					= 0x6d,
	OP_INVOKE_VIRTUAL				= 0x6e,
	OP_INVOKE_SUPER					= 0x6f,
	OP_INVOKE_DIRECT				= 0x70,
	OP_INVOKE_STATIC				= 0x71,
	OP_INVOKE_INTERFACE				= 0x72,
	OP_UNUSED_73					= 0x73,
	OP_INVOKE_VIRTUAL_RANGE			= 0x74,
	OP_INVOKE_SUPER_RANGE			= 0x75,
	OP_INVOKE_DIRECT_RANGE			= 0x76,
	OP_INVOKE_STATIC_RANGE			= 0x77,
	OP_INVOKE_INTERFACE_RANGE		= 0x78,
	OP_UNUSED_79					= 0x79,
	OP_UNUSED_7A					= 0x7a,
	OP_NEG_INT						= 0x7b,
	OP_NOT_INT						= 0x7c,
	OP_NEG_LONG						= 0x7d,
	OP_NOT_LONG						= 0x7e,
	OP_NEG_FLOAT					= 0x7f,
	OP_NEG_DOUBLE					= 0x80,
	OP_INT_TO_LONG					= 0x81,
	OP_INT_TO_FLOAT					= 0x82,
	OP_INT_TO_DOUBLE				= 0x83,
	OP_LONG_TO_INT					= 0x84,
	OP_LONG_TO_FLOAT				= 0x85,
	OP_LONG_TO_DOUBLE				= 0x86,
	OP_FLOAT_TO_INT					= 0x87,
	OP_FLOAT_TO_LONG				= 0x88,
	OP_FLOAT_TO_DOUBLE				= 0x89,
	OP_DOUBLE_TO_INT				= 0x8a,
	OP_DOUBLE_TO_LONG				= 0x8b,
	OP_DOUBLE_TO_FLOAT				= 0x8c,
	OP_INT_TO_BYTE					= 0x8d,
	OP_INT_TO_CHAR					= 0x8e,
	OP_INT_TO_SHORT					= 0x8f,
	OP_ADD_INT						= 0x90,
	OP_SUB_INT						= 0x91,
	OP_MUL_INT						= 0x92,
	OP_DIV_INT						= 0x93,
	OP_REM_INT						= 0x94,
	OP_AND_INT						= 0x95,
	OP_OR_INT						= 0x96,
	OP_XOR_INT						= 0x97,
	OP_SHL_INT						= 0x98,
	OP_SHR_INT						= 0x99,
	OP_USHR_INT						= 0x9a,
	OP_ADD_LONG						= 0x9b,
	OP_SUB_LONG						= 0x9c,
	OP_MUL_LONG						= 0x9d,
	OP_DIV_LONG						= 0x9e,
	OP_REM_LONG						= 0x9f,
	OP_AND_LONG						= 0xa0,
	OP_OR_LONG						= 0xa1,
	OP_XOR_LONG						= 0xa2,
	OP_SHL_LONG						= 0xa3,
	OP_SHR_LONG						= 0xa4,
	OP_USHR_LONG					= 0xa5,
	OP_ADD_FLOAT					= 0xa6,
	OP_SUB_FLOAT					= 0xa7,
	OP_MUL_FLOAT					= 0xa8,
	OP_DIV_FLOAT					= 0xa9,
	OP_REM_FLOAT					= 0xaa,
	OP_ADD_DOUBLE					= 0xab,
	OP_SUB_DOUBLE					= 0xac,
	OP_MUL_DOUBLE					= 0xad,
	OP_DIV_DOUBLE					= 0xae,
	OP_REM_DOUBLE					= 0xaf,
	OP_ADD_INT_2ADDR				= 0xb0,
	OP_SUB_INT_2ADDR				= 0xb1,
	OP_MUL_INT_2ADDR				= 0xb2,
	OP_DIV_INT_2ADDR				= 0xb3,
	OP_REM_INT_2ADDR				= 0xb4,
	OP_AND_INT_2ADDR				= 0xb5,
	OP_OR_INT_2ADDR					= 0xb6,
	OP_XOR_INT_2ADDR				= 0xb7,
	OP_SHL_INT_2ADDR				= 0xb8,
	OP_SHR_INT_2ADDR				= 0xb9,
	OP_USHR_INT_2ADDR				= 0xba,
	OP_ADD_LONG_2ADDR				= 0xbb,
	OP_SUB_LONG_2ADDR				= 0xbc,
	OP_MUL_LONG_2ADDR				= 0xbd,
	OP_DIV_LONG_2ADDR				= 0xbe,
	OP_REM_LONG_2ADDR				= 0xbf,
	OP_AND_LONG_2ADDR				= 0xc0,
	OP_OR_LONG_2ADDR				= 0xc1,
	OP_XOR_LONG_2ADDR				= 0xc2,
	OP_SHL_LONG_2ADDR				= 0xc3,
	OP_SHR_LONG_2ADDR				= 0xc4,
	OP_USHR_LONG_2ADDR				= 0xc5,
	OP_ADD_FLOAT_2ADDR				= 0xc6,
	OP_SUB_FLOAT_2ADDR				= 0xc7,
	OP_MUL_FLOAT_2ADDR				= 0xc8,
	OP_DIV_FLOAT_2ADDR				= 0xc9,
	OP_REM_FLOAT_2ADDR				= 0xca,
	OP_ADD_DOUBLE_2ADDR				= 0xcb,
	OP_SUB_DOUBLE_2ADDR				= 0xcc,
	OP_MUL_DOUBLE_2ADDR				= 0xcd,
	OP_DIV_DOUBLE_2ADDR				= 0xce,
	OP_REM_DOUBLE_2ADDR				= 0xcf,
	OP_ADD_INT_LIT16				= 0xd0,
	OP_RSUB_INT						= 0xd1,
	OP_MUL_INT_LIT16				= 0xd2,
	OP_DIV_INT_LIT16				= 0xd3,
	OP_REM_INT_LIT16				= 0xd4,
	OP_AND_INT_LIT16				= 0xd5,
	OP_OR_INT_LIT16					= 0xd6,
	OP_XOR_INT_LIT16				= 0xd7,
	OP_ADD_INT_LIT8					= 0xd8,
	OP_RSUB_INT_LIT8				= 0xd9,
	OP_MUL_INT_LIT8					= 0xda,
	OP_DIV_INT_LIT8					= 0xdb,
	OP_REM_INT_LIT8					= 0xdc,
	OP_AND_INT_LIT8					= 0xdd,
	OP_OR_INT_LIT8					= 0xde,
	OP_XOR_INT_LIT8					= 0xdf,
	OP_SHL_INT_LIT8					= 0xe0,
	OP_SHR_INT_LIT8					= 0xe1,
	OP_USHR_INT_LIT8				= 0xe2,
	OP_IGET_VOLATILE				= 0xe3,
	OP_IPUT_VOLATILE				= 0xe4,
	OP_SGET_VOLATILE				= 0xe5,
	OP_SPUT_VOLATILE				= 0xe6,
	OP_IGET_OBJECT_VOLATILE			= 0xe7,
	OP_IGET_WIDE_VOLATILE			= 0xe8,
	OP_IPUT_WIDE_VOLATILE			= 0xe9,
	OP_SGET_WIDE_VOLATILE			= 0xea,
	OP_SPUT_WIDE_VOLATILE			= 0xeb,
	OP_BREAKPOINT					= 0xec,
	OP_THROW_VERIFICATION_ERROR		= 0xed,
	OP_EXECUTE_INLINE				= 0xee,
	OP_EXECUTE_INLINE_RANGE			= 0xef,
	OP_INVOKE_OBJECT_INIT_RANGE		= 0xf0,
	OP_RETURN_VOID_BARRIER			= 0xf1,
	OP_IGET_QUICK					= 0xf2,
	OP_IGET_WIDE_QUICK				= 0xf3,
	OP_IGET_OBJECT_QUICK			= 0xf4,
	OP_IPUT_QUICK					= 0xf5,
	OP_IPUT_WIDE_QUICK				= 0xf6,
	OP_IPUT_OBJECT_QUICK			= 0xf7,
	OP_INVOKE_VIRTUAL_QUICK			= 0xf8,
	OP_INVOKE_VIRTUAL_QUICK_RANGE	= 0xf9,
	OP_INVOKE_SUPER_QUICK			= 0xfa,
	OP_INVOKE_SUPER_QUICK_RANGE		= 0xfb,
	OP_IPUT_OBJECT_VOLATILE			= 0xfc,
	OP_SGET_OBJECT_VOLATILE			= 0xfd,
	OP_SPUT_OBJECT_VOLATILE			= 0xfe,
	OP_UNUSED_FF					= 0xff,
};

} //namespace dalvik

#endif //DALVIK_H
