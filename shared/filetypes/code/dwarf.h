#ifndef DWARF_H
#define DWARF_H

#include "base/defs.h"
#include "base/algorithm.h"
#include "base/strings.h"
#include "base/array.h"
#include "comms/leb128.h"
#include "stream.h"

//-----------------------------------------------------------------------------
//	DWARF
//-----------------------------------------------------------------------------

namespace dwarf {
using namespace iso;
#if 0
template<typename T, bool sign = num_traits<T>::is_signed> struct leb128 {
	template<class W> static void put(W &w, const T &t) {
		T	t2;
		for (t2 = t; t2 > 127; t2 >>= 7)
			w.putc((t2 & 0x7f) | 0x80);
		w.putc(t2);
	}
	template<class R> static T get(R &r) {
		T	t = 0;
		for (int s = 0;; s += 7) {
			int	b = r.getc();
			if (b == EOF)
				break;
			t |= (b & 0x7f) << s;
			if (!(b & 0x80))
				break;
		}
		return t;
	}
};

template<typename T> struct leb128<T,true> {
	typedef unsigned_t<T>	U;
	template<class W> static void put(W &w, const U &t) {
		U	t2;
		for (t2 = t; t2 > 127 || t < -127; t2 /= 128)
			w.putc((t2 & 0x7f) | 0x80);
		w.putc(t2 & 0x7f);
	}
	template<class R> static U get(R &r) {
		U	t = 0;
		int	s = 0;
		for (int b; (b = r.getc()) != EOF; s += 7) {
			t |= (b & 0x7f) << s;
			if (!(b & 0x80))
				break;
		}
		return t - ((t & (64<<s)) << 1);
	}
};

template<typename T, class W> static void write_leb128(W &w, const T &t) {
	leb128<T>::put(w, t);
}

template<typename T, class R> static T read_leb128(R &r) {
	return leb128<T>::get(r);
}
#endif
enum TAG {
	TAG_array_type				= 0x01,
	TAG_class_type				= 0x02,
	TAG_entry_point				= 0x03,
	TAG_enumeration_type		= 0x04,
	TAG_formal_parameter		= 0x05,
	TAG_imported_declaration	= 0x08,
	TAG_label					= 0x0a,
	TAG_lexical_block			= 0x0b,
	TAG_member					= 0x0d,
	TAG_pointer_type			= 0x0f,
	TAG_reference_type			= 0x10,
	TAG_compile_unit			= 0x11,
	TAG_string_type				= 0x12,
	TAG_structure_type			= 0x13,
	TAG_subroutine_type			= 0x15,
	TAG_typedef					= 0x16,
	TAG_union_type				= 0x17,
	TAG_unspecified_parameters	= 0x18,
	TAG_variant					= 0x19,
	TAG_common_block			= 0x1a,
	TAG_common_inclusion		= 0x1b,
	TAG_inheritance				= 0x1c,
	TAG_inlined_subroutine		= 0x1d,
	TAG_module					= 0x1e,
	TAG_ptr_to_member_type		= 0x1f,
	TAG_set_type				= 0x20,
	TAG_subrange_type			= 0x21,
	TAG_with_stmt				= 0x22,
	TAG_access_declaration		= 0x23,
	TAG_base_type				= 0x24,
	TAG_catch_block				= 0x25,
	TAG_const_type				= 0x26,
	TAG_constant				= 0x27,
	TAG_enumerator				= 0x28,
	TAG_file_type				= 0x29,
	TAG_friend					= 0x2a,
	TAG_namelist				= 0x2b,
	TAG_namelist_item			= 0x2c,
	TAG_packed_type				= 0x2d,
	TAG_subprogram				= 0x2e,
	TAG_template_type_parameter	= 0x2f,
	TAG_template_value_parameter= 0x30,
	TAG_thrown_type				= 0x31,
	TAG_try_block				= 0x32,
	TAG_variant_part			= 0x33,
	TAG_variable				= 0x34,
	TAG_volatile_type			= 0x35,
	TAG_dwarf_procedure			= 0x36,
	TAG_restrict_type			= 0x37,
	TAG_interface_type			= 0x38,
	TAG_namespace				= 0x39,
	TAG_imported_module			= 0x3a,
	TAG_unspecified_type		= 0x3b,
	TAG_partial_unit			= 0x3c,
	TAG_imported_unit			= 0x3d,
	TAG_condition				= 0x3f,
	TAG_shared_type				= 0x40,
	TAG_type_unit				= 0x41,
	TAG_rvalue_reference_type	= 0x42,
	TAG_template_alias			= 0x43,
	_TAG_table_size,
	TAG_lo_user					= 0x4080,
	TAG_hi_user					= 0xffff,
};
enum ATTRIBUTE {
	AT_sibling					= 0x01,		//reference
	AT_location					= 0x02,		//block, loclistptr
	AT_name						= 0x03,		//string
	AT_ordering					= 0x09,		//constant
	AT_byte_size				= 0x0b,		//block, constant, reference
	AT_bit_offset				= 0x0c,		//block, constant, reference
	AT_bit_size					= 0x0d,		//block, constant, reference
	AT_stmt_list				= 0x10,		//lineptr
	AT_low_pc					= 0x11,		//address
	AT_high_pc					= 0x12,		//address
	AT_language					= 0x13,		//constant
	AT_discr					= 0x15,		//reference
	AT_discr_value				= 0x16,		//constant
	AT_visibility				= 0x17,		//constant
	AT_import					= 0x18,		//reference
	AT_string_length			= 0x19,		//block, loclistptr
	AT_common_reference			= 0x1a,		//reference
	AT_comp_dir					= 0x1b,		//string
	AT_const_value				= 0x1c,		//block, constant, string
	AT_containing_type			= 0x1d,		//reference
	AT_default_value			= 0x1e,		//reference
	AT_inline					= 0x20,		//constant
	AT_is_optional				= 0x21,		//flag
	AT_lower_bound				= 0x22,		//block, constant, reference
	AT_producer					= 0x25,		//string
	AT_prototyped				= 0x27,		//flag
	AT_return_addr				= 0x2a,		//block, loclistptr
	AT_start_scope				= 0x2c,		//constant
	AT_bit_stride				= 0x2e,		//constant
	AT_upper_bound				= 0x2f,		//block, constant, reference
	AT_abstract_origin			= 0x31,		//reference
	AT_accessibility			= 0x32,		//constant
	AT_address_class			= 0x33,		//constant
	AT_artificial				= 0x34,		//flag
	AT_base_types				= 0x35,		//reference
	AT_calling_convention		= 0x36,		//constant
	AT_count					= 0x37,		//block, constant, reference
	AT_data_member_location		= 0x38,		//block, constant, loclistptr
	AT_decl_column				= 0x39,		//constant
	AT_decl_file				= 0x3a,		//constant
	AT_decl_line				= 0x3b,		//constant
	AT_declaration				= 0x3c,		//flag
	AT_discr_list				= 0x3d,		//block
	AT_encoding					= 0x3e,		//constant
	AT_external					= 0x3f,		//flag
	AT_frame_base				= 0x40,		//block, loclistptr
	AT_friend					= 0x41,		//reference
	AT_identifier_case			= 0x42,		//constant
	AT_macro_info				= 0x43,		//macptr
	AT_namelist_item			= 0x44,		//block
	AT_priority					= 0x45,		//reference
	AT_segment					= 0x46,		//block, loclistptr
	AT_specification			= 0x47,		//reference
	AT_static_link				= 0x48,		//block, loclistptr
	AT_type						= 0x49,		//reference
	AT_use_location				= 0x4a,		//block, loclistptr
	AT_variable_parameter		= 0x4b,		//flag
	AT_virtuality				= 0x4c,		//constant
	AT_vtable_elem_location		= 0x4d,		//block, loclistptr
	AT_allocated				= 0x4e,		//block, constant, reference
	AT_associated				= 0x4f,		//block, constant, reference
	AT_data_location			= 0x50,		//block
	AT_byte_stride				= 0x51,		//block, constant, reference
	AT_entry_pc					= 0x52,		//address
	AT_use_UTF8					= 0x53,		//flag
	AT_extension				= 0x54,		//reference
	AT_ranges					= 0x55,		//rangelistptr
	AT_trampoline				= 0x56,		//address, flag, reference, string
	AT_call_column				= 0x57,		//constant
	AT_call_file				= 0x58,		//constant
	AT_call_line				= 0x59,		//constant
	AT_description				= 0x5a,		//string
	AT_binary_scale				= 0x5b,		//constant
	AT_decimal_scale			= 0x5c,		//constant
	AT_small					= 0x5d,		//reference
	AT_decimal_sign				= 0x5e,		//constant
	AT_digit_count				= 0x5f,		//constant
	AT_picture_string			= 0x60,		//string
	AT_mutable					= 0x61,		//flag
	AT_threads_scaled			= 0x62,		//flag
	AT_explicit					= 0x63,		//flag
	AT_object_pointer			= 0x64,		//reference
	AT_endianity				= 0x65,		//constant
	AT_elemental				= 0x66,		//flag
	AT_pure						= 0x67,		//flag
	AT_recursive				= 0x68,		//flag
	AT_signature				= 0x69,		//reference
	AT_main_subprogram			= 0x6a,		//flag
	AT_data_bit_offset			= 0x6b,		//constant
	AT_const_expr				= 0x6c,		//flag
	AT_enum_class				= 0x6d,		//flag
	AT_linkage_name				= 0x6e,		//string
	_AT_table_size,
	AT_lo_user					= 0x2000,	//---
	AT_hi_user					= 0x3fff,	//---
};
enum FORM {
	FORM_addr					= 0x01,		//address
	FORM_block2					= 0x03,		//block
	FORM_block4					= 0x04,		//block
	FORM_data2					= 0x05,		//constant
	FORM_data4					= 0x06,		//constant, (lineptr, loclistptr, macptr, rangelistptr)
	FORM_data8					= 0x07,		//constant, (lineptr, loclistptr, macptr, rangelistptr)
	FORM_string					= 0x08,		//string
	FORM_block					= 0x09,		//block
	FORM_block1					= 0x0a,		//block
	FORM_data1					= 0x0b,		//constant
	FORM_flag					= 0x0c,		//flag
	FORM_sdata					= 0x0d,		//constant
	FORM_strp					= 0x0e,		//string
	FORM_udata					= 0x0f,		//constant
	FORM_ref_addr				= 0x10,		//reference
	FORM_ref1					= 0x11,		//reference
	FORM_ref2					= 0x12,		//reference
	FORM_ref4					= 0x13,		//reference
	FORM_ref8					= 0x14,		//reference
	FORM_ref_udata				= 0x15,		//reference
	FORM_indirect				= 0x16,		//(see Section 7.5.3)
	FORM_sec_offset				= 0x17,		//lineptr, loclistptr, macptr, rangelistptr
	FORM_exprloc				= 0x18,		//exprloc
	FORM_flag_present			= 0x19,		//flag
	FORM_ref_sig8				= 0x20,		//reference
};

enum OP {
	OP_addr						= 0x03,		//args=1	constant address (size target specific)
	OP_deref					= 0x06,		//args=0
	OP_const1u					= 0x08,		//args=1	1-byte constant
	OP_const1s					= 0x09,		//args=1	1-byte constant
	OP_const2u					= 0x0a,		//args=1	2-byte constant
	OP_const2s					= 0x0b,		//args=1	2-byte constant
	OP_const4u					= 0x0c,		//args=1	4-byte constant
	OP_const4s					= 0x0d,		//args=1	4-byte constant
	OP_const8u					= 0x0e,		//args=1	8-byte constant
	OP_const8s					= 0x0f,		//args=1	8-byte constant
	OP_constu					= 0x10,		//args=1	ULEB128 constant
	OP_consts					= 0x11,		//args=1	SLEB128 constant
	OP_dup						= 0x12,		//args=0
	OP_drop						= 0x13,		//args=0
	OP_over						= 0x14,		//args=0
	OP_pick						= 0x15,		//args=1	1-byte stack index
	OP_swap						= 0x16,		//args=0
	OP_rot						= 0x17,		//args=0
	OP_xderef					= 0x18,		//args=0
	OP_abs						= 0x19,		//args=0
	OP_and						= 0x1a,		//args=0
	OP_div						= 0x1b,		//args=0
	OP_minus					= 0x1c,		//args=0
	OP_mod						= 0x1d,		//args=0
	OP_mul						= 0x1e,		//args=0
	OP_neg						= 0x1f,		//args=0
	OP_not						= 0x20,		//args=0
	OP_or						= 0x21,		//args=0
	OP_plus						= 0x22,		//args=0
	OP_plus_uconst				= 0x23,		//args=1	ULEB128 addend
	OP_shl						= 0x24,		//args=0
	OP_shr						= 0x25,		//args=0
	OP_shra						= 0x26,		//args=0
	OP_xor						= 0x27,		//args=0
	OP_skip						= 0x2f,		//args=1	signed 2-byte constant
	OP_bra						= 0x28,		//args=1	signed 2-byte constant
	OP_eq						= 0x29,		//args=0
	OP_ge						= 0x2a,		//args=0
	OP_gt						= 0x2b,		//args=0
	OP_le						= 0x2c,		//args=0
	OP_lt						= 0x2d,		//args=0
	OP_ne						= 0x2e,		//args=0
	OP_lit0						= 0x30,		//args=0	...0x4f	literals
	OP_reg0						= 0x50,		//args=0	...0x6f
	OP_breg0					= 0x70,		//args=1	...0x8f	SLEB128 offset base
	OP_regx						= 0x90,		//args=1	ULEB128 register
	OP_fbreg					= 0x91,		//args=1	SLEB128 offset
	OP_bregx					= 0x92,		//args=2	ULEB128 register followed by SLEB128 offset
	OP_piece					= 0x93,		//args=1	ULEB128 size of piece addressed
	OP_deref_size				= 0x94,		//args=1	1-byte size of data retrieved
	OP_xderef_size				= 0x95,		//args=1	1-byte size of data retrieved
	OP_nop						= 0x96,		//args=0
	OP_push_object_address		= 0x97,		//args=0
	OP_call2					= 0x98,		//args=12-byte offset of DIE
	OP_call4					= 0x99,		//args=1	4-byte offset of DIE
	OP_call_ref					= 0x9a,		//args=1	4- or 8-byte offset of DIE
	OP_form_tls_address			= 0x9b,		//args=0
	OP_call_frame_cfa			= 0x9c,		//args=0
	OP_bit_piece				= 0x9d,		//args=2
	OP_implicit_value			= 0x9e,		//args=2	ULEB128 size followed by block of that size
	OP_stack_value				= 0x9f,		//args=0
	OP_lo_user					= 0xe0,
	OP_hi_user					= 0xff,
};

enum ATE {
	ATE_address					= 0x01,
	ATE_boolean					= 0x02,
	ATE_complex_float			= 0x03,
	ATE_float					= 0x04,
	ATE_signed					= 0x05,
	ATE_signed_char				= 0x06,
	ATE_unsigned				= 0x07,
	ATE_unsigned_char			= 0x08,
	ATE_imaginary_float			= 0x09,
	ATE_packed_decimal			= 0x0a,
	ATE_numeric_string			= 0x0b,
	ATE_edited					= 0x0c,
	ATE_signed_fixed			= 0x0d,
	ATE_unsigned_fixed			= 0x0e,
	ATE_decimal_float			= 0x0f,
	ATE_UTF						= 0x10,
	ATE_lo_user					= 0x80,
	ATE_hi_user					= 0xff,
};

enum DS {
	DS_unsigned					= 0x01,
	DS_leading_overpunch		= 0x02,
	DS_trailing_overpunch		= 0x03,
	DS_leading_separate			= 0x04,
	DS_trailing_separate		= 0x05,
};

enum END {
	END_default					= 0x00,
	END_big						= 0x01,
	END_little					= 0x02,
	END_lo_user					= 0x40,
	END_hi_user					= 0xff,
};

enum ACCESS {
	ACCESS_public				= 0x01,
	ACCESS_protected			= 0x02,
	ACCESS_private				= 0x03,
};

enum VIS {
	VIS_local					= 0x01,
	VIS_exported				= 0x02,
	VIS_qualified				= 0x03,
};

enum VIRTUALITY {
	VIRTUALITY_none				= 0x00,
	VIRTUALITY_virtual			= 0x01,
	VIRTUALITY_pure_virtual		= 0x02,
};

enum LANG {
	LANG_C89					= 0x0001,
	LANG_C						= 0x0002,
	LANG_Ada83					= 0x0003,
	LANG_C_plus_plus			= 0x0004,
	LANG_Cobol74 				= 0x0005,
	LANG_Cobol85 				= 0x0006,
	LANG_Fortran77				= 0x0007,
	LANG_Fortran90				= 0x0008,
	LANG_Pascal83				= 0x0009,
	LANG_Modula2				= 0x000a,
	LANG_Java					= 0x000b,
	LANG_C99					= 0x000c,
	LANG_Ada95					= 0x000d,
	LANG_Fortran95				= 0x000e,
	LANG_PLI					= 0x000f,
	LANG_ObjC					= 0x0010,
	LANG_ObjC_plus_plus			= 0x0011,
	LANG_UPC					= 0x0012,
	LANG_D						= 0x0013,
	LANG_Python					= 0x0014,
	LANG_OpenCL					= 0x0015,
	LANG_Go						= 0x0016,
	LANG_Modula3				= 0x0017,
	LANG_Haskell				= 0x0018,
	LANG_C_plus_plus_03			= 0x0019,
	LANG_C_plus_plus_11			= 0x001a,
	LANG_OCaml					= 0x001b,
	LANG_Rust					= 0x001c,
	LANG_C11					= 0x001d,
	LANG_Swift					= 0x001e,
	LANG_Julia					= 0x001f,
	LANG_Dylan					= 0x0020,
	LANG_C_plus_plus_14			= 0x0021,
	LANG_Fortran03				= 0x0022,
	LANG_Fortran08				= 0x0023,
	LANG_RenderScript			= 0x0024,
	LANG_BLISS					= 0x0025,
	LANG_lo_user				= 0x8000,
	LANG_hi_user				= 0xffff,
};

enum ID {
	ID_case_sensitive			= 0x00,
	ID_up_case					= 0x01,
	ID_down_case				= 0x02,
	ID_istr						= 0x03,
};

enum CC {
	CC_normal					= 0x01,
	CC_program					= 0x02,
	CC_nocall					= 0x03,
	CC_lo_user					= 0x40,
	CC_hi_user					= 0xff,
};

enum INL {
	INL_not_inlined				= 0x00,
	INL_inlined					= 0x01,
	INL_declared_not_inlined	= 0x02,
	INL_declared_inlined		= 0x03,
};

enum ORD {
	ORD_row_major				= 0x00,
	ORD_col_major				= 0x01,
};

enum DSC {
	DSC_label					= 0x00,
	DSC_range					= 0x01,
};

enum LNS {
	LNS_extended				= 0x00,
	LNS_copy					= 0x01,
	LNS_advance_pc				= 0x02,
	LNS_advance_line			= 0x03,
	LNS_set_file				= 0x04,
	LNS_set_column				= 0x05,
	LNS_negate_stmt				= 0x06,
	LNS_set_basic_block			= 0x07,
	LNS_const_add_pc			= 0x08,
	LNS_fixed_advance_pc		= 0x09,
	LNS_set_prologue_end		= 0x0a,
	LNS_set_epilogue_begin		= 0x0b,
	LNS_set_isa					= 0x0c,
};

enum LNE {
	LNE_end_sequence			= 0x01,
	LNE_set_address				= 0x02,
	LNE_define_file				= 0x03,
	LNE_set_discriminator		= 0x04,
	LNE_lo_user					= 0x80,
	LNE_hi_user					= 0xff,
};

enum MACINFO {
	MACINFO_define				= 0x01,
	MACINFO_undef				= 0x02,
	MACINFO_start_file			= 0x03,
	MACINFO_end_file			= 0x04,
	MACINFO_vendor_ext			= 0xff,
};

enum CFA {
	CFA_nop						= 0,
	CFA_advance_loc				= 0x40,		//+delta,
	CFA_offset					= 0x80,		//register op1=ULEB128 offset
	CFA_restore					= 0xc0,		//register
	CFA_set_loc					= 0x01,		//op1=address
	CFA_advance_loc1			= 0x02,		//op1=1-byte delta
	CFA_advance_loc2			= 0x03,		//op1=2-byte delta
	CFA_advance_loc4			= 0x04,		//op1=4-byte delta
	CFA_offset_extended			= 0x05,		//op1=ULEB128 register op2=ULEB128 offset
	CFA_restore_extended		= 0x06,		//op1=ULEB128 register
	CFA_undefined				= 0x07,		//op1=ULEB128 register
	CFA_same_value				= 0x08,		//op1=ULEB128 register
	CFA_register				= 0x09,		//op1=ULEB128 register op2=ULEB128 register
	CFA_remember_state			= 0x0a,
	CFA_restore_state			= 0x0b,
	CFA_def_cfa					= 0x0c,		//op1=ULEB128 register op2=ULEB128 offset
	CFA_def_cfa_register		= 0x0d,		//op1=ULEB128 register
	CFA_def_cfa_offset			= 0x0e,		//op1=ULEB128 offset
	CFA_def_cfa_expression		= 0x0f,		//op1=BLOCK
	CFA_expression				= 0x10,		//op1=ULEB128 register op2=BLOCK
	CFA_offset_extended_sf		= 0x11,		//op1=ULEB128 register op2=SLEB128 offset
	CFA_def_cfa_sf				= 0x12,		//op1=ULEB128 register op2=SLEB128 offset
	CFA_def_cfa_offset_sf		= 0x13,		//op1=SLEB128 offset
	CFA_val_offset				= 0x14,		//op1=ULEB128 op2=ULEB128
	CFA_val_offset_sf			= 0x15,		//op1=ULEB128 op2=SLEB128
	CFA_val_expression			= 0x16,		//op1=ULEB128 op2=BLOCK
	CFA_lo_user					= 0x1c,
	CFA_hi_user					= 0x3f,
};

enum EH_PE {
	EH_PE_absptr				= 0x00,		//An absolute pointer. The size is determined by whether this is a 32-bit or 64-bit address space, and will be 32 or 64 bits.
	EH_PE_omit					= 0xff,		//The value is omitted.
	EH_PE_uleb128				= 0x01,		//The value is an unsigned LEB128.
	EH_PE_udata2				= 0x02,
	EH_PE_udata4				= 0x03,
	EH_PE_udata8				= 0x04,		//The value is stored as unsigned data with the specified number of bytes.
	EH_PE_signed				= 0x08,		//A signed number. The size is determined by whether this is a 32-bit or 64-bit address space. I don’t think this ever appears in a CIE or FDE in practice.
	EH_PE_sleb128				= 0x09,		//A signed LEB128. Not used in practice.
	EH_PE_sdata2				= 0x0a,
	EH_PE_sdata4				= 0x0b,
	EH_PE_sdata8				= 0x0c,		//The value is stored as signed data with the specified number of bytes. Not used in practice.
	//modifiers:
	EH_PE_pcrel					= 0x10,		//Value is PC relative.
	EH_PE_textrel				= 0x20,		//Value is text relative.
	EH_PE_datarel				= 0x30,		//Value is data relative.
	EH_PE_funcrel				= 0x40,		//Value is relative to start of function.
	EH_PE_aligned				= 0x50,		//Value is aligned: padding bytes are inserted as required to make value be naturally aligned.
	EH_PE_indirect				= 0x80,		//This is actually the address of the real value.
};

enum AT_CLASS {
	ATC_none					= 0,
	ATC_constant				= 1 << 0,
	ATC_flag					= 1 << 1,
	ATC_address					= 1 << 2,
	ATC_block					= 1 << 3,

	_ATC_refmask				= 0xf << 4,
	ATC_string					= 1 << 4,
	ATC_reference				= 2 << 4,
	ATC_loclistptr				= 3 << 4,
	ATC_rangelistptr			= 4 << 4,
	ATC_lineptr					= 5 << 4,
	ATC_macptr					= 6 << 4,

	ATC_blk_loc					= ATC_block | ATC_loclistptr,
	ATC_blk_const_ref			= ATC_block | ATC_constant | ATC_reference,
	ATC_blk_const_str			= ATC_block | ATC_constant | ATC_string,
	ATC_blk_const_loc			= ATC_block | ATC_constant | ATC_loclistptr,
	ATC_add_flg_ref_str			= ATC_address | ATC_flag | ATC_reference | ATC_string,
};
uint8 GetClass(ATTRIBUTE attr);
extern const char *attr_ids[];
extern const char *tag_ids[];

struct endian_reader {
	bool	bigendian;
	template<typename T, typename R> T get(R &r) {
		return endian(r.template get<T>(), bigendian);
	}

	template<typename R> uint64	get_val(R &r, uint8 size) {
		uint64	temp = 0;
		ISO_ASSERT(size < sizeof(temp));
		r.readbuff((uint8*)&temp + (bigendian ? 8 - size : 0), size);
		return endian(temp, bigendian);
	}

	endian_reader(bool bigendian) : bigendian(bigendian) {}
};

struct initial_length {
	const uint8	*start;
	const uint8	*end;
	uint64		size;
	bool		large;

	void	read(byte_reader &b, bool bigendian) {
		start	= b.p;
		uint32	s = endian(b.get<uint32>(), bigendian);
		large	= s == 0xffffffff;
		size	= large ? endian(b.get<uint64>(), bigendian) : s;
		end		= b.p + size;
	}
	uint64	read_offset(byte_reader &b, bool bigendian) {
		return large ? endian(b.get<uint64>(), bigendian) : endian(b.get<uint32>(), bigendian);
	}
};

struct initial_length_ver : initial_length {
	uint16		version;

	void	read(byte_reader &b, bool bigendian) {
		initial_length::read(b, bigendian);
		version	= endian(b.get<uint16>(), bigendian);
	}
};

struct info_unit : initial_length_ver {
	uint64		abbr_off;
	uint8		addr_size;

	void	read(byte_reader &b, bool bigendian) {
		initial_length_ver::read(b, bigendian);
		abbr_off	= read_offset(b, bigendian);
		addr_size	= b.get<uint8>();
	}
};

struct arange_unit : initial_length_ver {
	uint64		info_offset;
	uint8		addr_size;
	uint8		seg_size;

	void	read(byte_reader &b, bool bigendian) {
		initial_length_ver::read(b, bigendian);
		info_offset	= read_offset(b, bigendian);
		addr_size	= b.get<uint8>();
		seg_size	= b.get<uint8>();
	}
	uint32	entry_size()	{ return seg_size + 2 * addr_size; }
};

struct names_unit : initial_length_ver {
	uint64		info_offset;
	uint64		info_length;

	void	read(byte_reader &b, bool bigendian) {
		initial_length_ver::read(b, bigendian);
		info_offset	= read_offset(b, bigendian);
		info_length	= read_offset(b, bigendian);
	}
};

struct line_unit : initial_length_ver {
	struct header {
		uint8		min_instr_len;
		uint8		max_ops_per_instr;
		uint8		default_is_stmt;
		int8		line_base;
		uint8		line_range;
		uint8		opcode_base;
		uint8		opcode_lengths[];
	};
	uint64		offset;
	header		*h;

	void	read(byte_reader &b, bool bigendian) {
		initial_length_ver::read(b, bigendian);
		offset		= read_offset(b, bigendian);
		h			= (header*)b.p;
		b.p			= &h->opcode_lengths[h->opcode_base - 1];
	}
};

struct line_info {
	enum FLAGS {
		is_stmt			= 1 << 0,	// current instruction is a recommended breakpoint location
		basic_block		= 1 << 1,	// current instruction is the beginning of a basic block.
		end_sequence	= 1 << 2,	// current address is that of the first byte after the end of a sequence of target machine instructions. end_sequence terminates a sequence of lines; therefore other information in the same row is not meaningful.
		prologue_end	= 1 << 3,	// current address is one (of possibly many) where execution should be suspended for an entry breakpoint of a function.
		epilogue_begin	= 1 << 4,	// current address is one (of possibly many) where execution should be suspended for an exit breakpoint of a function.
	};
	iso::flags<FLAGS>	flags;
	uint64			address;
	uint32			file;			// identity of the source file corresponding to a machine instruction.
	uint32			line;			// source line number
	uint32			column;			// column number within a source line
	uint32			isa;			// the applicable instruction set architecture for the current instruction.
	uint32			discriminator;	// the block to which the current instruction belongs
};

struct file_info {
	string	name;
	uint32	dir;
	uint64	time;
	uint64	len;
	bool read(byte_reader &b) {
		name	= b.get<string>();
		dir		= b.get<leb128<uint32> >();
		time	= b.get<leb128<uint64> >();
		len		= b.get<leb128<uint64> >();
		return true;
	}
};

struct line_machine : line_info, endian_reader {
	uint8						addr_size;
	dynamic_array<string>		dirs;
	dynamic_array<file_info>	files;
	dynamic_array<line_info>	lines;

	void	output() {
		lines.push_back(*this);
		discriminator = 0;
		flags.clear_all(basic_block | prologue_end | epilogue_begin);
	}

	void	advance_op(line_unit::header *h, uint32 x) {
		address += h->min_instr_len * x;
	}

	uint64	get_addr(byte_reader &b) {
		return get_val(b, addr_size);
	}

	void	op(line_unit::header *h, byte_reader &b);
	void	header(line_unit::header *h, byte_reader &b);

	line_machine(bool _bigendian, uint8 _addr_size) : endian_reader(_bigendian), addr_size(_addr_size) {
		address	= 0;
		file	= line	= 1;
		column	= isa	= discriminator = 0;
	}
};

struct expr_machine : endian_reader {
	const uint8	*global_start, *unit_start;

	uint64		frame_base;
	bool		stack_value;
	uint8		addr_size;

	uint64		stack[64], *sp;

	uint64	get_addr(byte_reader &b) {
		return get_val(b, addr_size);
	}
	uint64	deref(uint64 addr, uint64 space, uint8 size)	{ return 0; }
	uint64	reg(uint64 r)									{ return 0; }
	uint64	breg(uint64 r, int64 offset)					{ return 0; }
	void	call(const uint8 *p)							{}
	uint64	tls(uint64 addr)								{ return 0; }
	uint64	cfa()											{ return 0; }

	void	process(const uint8 *p, const uint8 *end);

	expr_machine(bool _bigendian, uint8 addr_size) : endian_reader(_bigendian), stack_value(false), addr_size(addr_size), sp(stack) {}
};

struct frame_unit : initial_length {
	uint64		cie_offset;

	bool	is_cie()	const { return cie_offset == (uint64(!large) << 32) - 1; }
	bool	is_cie_eh() const { return cie_offset == 0; }
	void	read(byte_reader &b, bool bigendian) {
		initial_length::read(b, bigendian);
		cie_offset	= b.p < end ? read_offset(b, bigendian) : 0;
	}
};

struct frame_unit_cie : frame_unit {
	uint8		version;
	const char	*augmentation;
	uint8		addr_size;
	uint8		seg_size;
	uint32		code_align;
	int32		data_align;
	uint32		return_reg;
	const uint8	*init;

	void	read(byte_reader &b, bool bigendian);
};

struct frame_unit_cie_eh : frame_unit {
	uint8		version;	//=1 or 3
	uint64		eh_data;	//only present if the Augmentation String contains the string "eh"
	uint32		code_align;
	uint32		data_align;
	uint32		return_reg;

	bool		aug;
	bool		signal;
	EH_PE		lsda_enc;
	EH_PE		fde_enc;
	uint64		personality;

	const uint8	*init;

	uint64	read_encoded(byte_reader &b, EH_PE enc, bool bigendian);
	void	read(byte_reader &b, bool bigendian);
};

struct frame_machine : endian_reader {
	enum type {undef, same, reg, offset, valoff, exp, valexp};
	struct reg_state {
		type	t;
		int64	v;
	};
	typedef dynamic_array<reg_state> state;

	frame_unit_cie			cie;
	dynamic_array<state>	stack;
	state					init;
	state					curr;
	uint64					loc;
	uint32					cfa_reg;
	int64					cfa_offset;

	void	restore(uint32 r)					{ curr[r] = init[r]; }
	void	setreg(uint32 r, type t)			{ curr[r].t = t; }
	void	setreg(uint32 r, type t, uint64 v)	{ curr[r].t = t; curr[r].v = v; }
	void	setreg(uint32 r, type t, const void *v)	{ curr[r].t = t; curr[r].v = intptr_t(v); }

	void	output() {
	}

	void	process(const uint8 *p, const uint8 *end);

	frame_machine(bool _bigendian, frame_unit_cie &_cie) : endian_reader(_bigendian), cie(_cie) {
	}
};

struct Sections {
	const_memory_block
		abbrev,		aranges,	frame,		info,
		line,		loc,		macinfo,	pubnames,
		pubtypes,	ranges,		str,		types;

	Sections() { clear(*this); }
	const_memory_block&	operator[](int i) { return (&abbrev)[i]; }
};

}//namespace dwarf

#endif	//DWARF_H