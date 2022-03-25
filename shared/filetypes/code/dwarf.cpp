#include "base/hash.h"
#include "dwarf.h"

namespace dwarf {

void line_machine::op(line_unit::header *h, byte_reader &b) {
	uint8	op	= b.getc();
	if (op >= h->opcode_base) {		//special
		advance_op(h, (op - h->opcode_base) / h->line_range);
		line		+= h->line_base + (op - h->opcode_base) % h->line_range;
		output();
	} else switch (op) {
		case LNS_copy:					output(); break;
		case LNS_advance_pc:			advance_op(h, b.get<leb128<uint32> >()); break;
		case LNS_advance_line:			line	+= b.get<leb128<int32> >(); break;
		case LNS_set_file:				file	= b.get<leb128<uint32> >(); break;
		case LNS_set_column:			column	= b.get<leb128<uint32> >(); break;
		case LNS_negate_stmt:			flags.flip(is_stmt); break;
		case LNS_set_basic_block:		flags.set(basic_block); break;
		case LNS_const_add_pc:			advance_op(h, (255 - h->opcode_base) / h->line_range); break;
		case LNS_fixed_advance_pc:		address += get<uint16>(b); break;
		case LNS_set_prologue_end:		flags.set(prologue_end); break;
		case LNS_set_epilogue_begin:	flags.set(epilogue_begin); break;
		case LNS_set_isa:				isa = b.get<leb128<uint32> >(); break;
		case LNS_extended: {
			uint32	len = b.get<leb128<uint32> >();
			switch (b.getc()) {
				case LNE_end_sequence:
					flags.set(end_sequence);
					output();
					flags.clear(end_sequence);
					flags.set(is_stmt, !!h->default_is_stmt);
					file	= line = 1;
					column	= isa = 0;
					break;

				case LNE_set_address:		address = get_addr(b); break;
				case LNE_define_file:		b.get<file_info>(); break;
				case LNE_set_discriminator:	discriminator = b.get<leb128<uint32> >(); break;
				default: b.p += len - 1; break;
			}
			break;
		}
		default:
			for (int i = h->opcode_lengths[op]; i--; b.get<leb128<uint32> >());
			break;
	}
}

void line_machine::header(line_unit::header *h, byte_reader &b) {
	while (b.peekc())
		dirs.push_back() = b.get<string>();
	b.getc();

	while (b.peekc())
		files.push_back().read(b);// = b.get();
	b.getc();
}

void expr_machine::process(const uint8 *p, const uint8 *end) {
	for (byte_reader b(p); b.p < end;) {
		uint8	op	= b.getc();
		if (op < OP_lit0 || op >= OP_breg0 + 32) switch (op) {
			case OP_addr:				*sp++ = get_addr(b); break;
			case OP_deref:				sp[-1] = deref(sp[-1], 0, addr_size); break;
			case OP_const1u:			*sp++ = b.get<uint8>(); break;
			case OP_const1s:			*sp++ = b.get<int8>(); break;
			case OP_const2u:			*sp++ = get<uint16>(b); break;
			case OP_const2s:			*sp++ = get<int16>(b); break;
			case OP_const4u:			*sp++ = get<uint32>(b); break;
			case OP_const4s:			*sp++ = get<int32>(b); break;
			case OP_const8u:			*sp++ = get<uint64>(b); break;
			case OP_const8s:			*sp++ = get<int64>(b); break;
			case OP_constu:				*sp++ = b.get<leb128<int64> >(); break;
			case OP_consts:				*sp++ = b.get<leb128<uint64> >(); break;
			case OP_dup:				*sp = sp[-1]; ++sp; break;
			case OP_drop:				--sp; break;
			case OP_over:				*sp = sp[-2]; ++sp; break;
			case OP_pick:				*sp = sp[-(b.get<uint8>() + 1)]; ++sp; break;
			case OP_swap:				swap(sp[-1], sp[-2]); break;
			case OP_rot:				{uint64 t = sp[-1]; sp[-1] = sp[-2]; sp[-2] = sp[-3]; sp[-3] = t; break; }
			case OP_xderef:				sp[-2] = deref(sp[-1], sp[-2], addr_size); --sp; break;
			case OP_abs:				sp[-1] = abs(int64(sp[-1])); break;
			case OP_and:				sp[-2] = sp[-1] & sp[-2]; --sp; break;
			case OP_div:				sp[-2] = int64(sp[-2]) / int64(sp[-1]); --sp; break;
			case OP_minus:				sp[-2] = int64(sp[-2]) - int64(sp[-1]); --sp; break;
			case OP_mod:				sp[-2] = int64(sp[-2]) % int64(sp[-1]); --sp; break;
			case OP_mul:				sp[-2] = int64(sp[-2]) * int64(sp[-1]); --sp; break;
			case OP_neg:				sp[-1] = -int64(sp[-1]); break;
			case OP_not:				sp[-1] = ~sp[-1]; break;
			case OP_or:					sp[-2] = sp[-1] | sp[-2]; --sp; break;
			case OP_plus:				sp[-2] = sp[-1] + sp[-2]; --sp; break;
			case OP_plus_uconst:		sp[-1] += b.get<leb128<uint64> >(); break;
			case OP_shl:				sp[-2] = sp[-2] << sp[-1]; --sp; break;
			case OP_shr:				sp[-2] = sp[-2] >> sp[-1]; --sp; break;
			case OP_shra:				sp[-2] = int64(sp[-2]) >> sp[-1]; --sp; break;
			case OP_xor:				sp[-2] = sp[-2] ^ sp[-1]; --sp; break;
			case OP_skip:				{ int s = get<int16>(b); b.p += s; break; }
			case OP_bra:				{ int s = get<int16>(b); if (*--sp) b.p += s; break; }
			case OP_eq:					sp[-2] = int64(sp[-2]) == int64(sp[-1]); --sp; break;
			case OP_ge:					sp[-2] = int64(sp[-2]) >= int64(sp[-1]); --sp; break;
			case OP_gt:					sp[-2] = int64(sp[-2]) >  int64(sp[-1]); --sp; break;
			case OP_le:					sp[-2] = int64(sp[-2]) <= int64(sp[-1]); --sp; break;
			case OP_lt:					sp[-2] = int64(sp[-2]) <  int64(sp[-1]); --sp; break;
			case OP_ne:					sp[-2] = int64(sp[-2]) != int64(sp[-1]); --sp; break;
			case OP_regx:				*sp++ = reg(b.get<leb128<uint64> >()); break;
			case OP_fbreg:				*sp++ = frame_base + b.get<leb128<int64> >(); break;
			case OP_bregx:				{ uint64 r = b.get<leb128<uint64> >(); *sp++ = breg(r, b.get<leb128<int64> >()); break; }
			case OP_piece:				{ uint64 size = b.get<leb128<uint64> >(); break; }
			case OP_deref_size:			sp[-1] = deref(sp[-1], 0, b.get<uint8>()); break;
			case OP_xderef_size:		sp[-2] = deref(sp[-1], sp[-2], b.get<uint8>()); --sp; break;
			case OP_nop:				break;
			case OP_push_object_address:break;
			case OP_call2:				call(unit_start + get<uint16>(b)); break;
			case OP_call4:				call(unit_start + get<uint32>(b)); break;
			case OP_call_ref:			call(global_start + get<uint64>(b)); break;
			case OP_form_tls_address:	sp[-1] = tls(sp[-1]); break;
			case OP_call_frame_cfa:		*sp++	= cfa(); break;
			case OP_bit_piece:			{ uint64 size = b.get<leb128<uint64> >(), offset = b.get<leb128<uint64> >(); break; }
			case OP_implicit_value:		*sp++	= get_val(b, b.get<leb128<uint64> >()); break;
			case OP_stack_value:		stack_value = true; break;
		} else if (op < OP_reg0) {
			*sp++ = op - OP_lit0;
		} else if (op < OP_breg0) {
			*sp++ = reg(op - OP_reg0);
		} else  {
			*sp++ = breg(op - OP_breg0, b.get<leb128<int64> >());
		}
	}
}

void frame_unit_cie::read(byte_reader &b, bool bigendian) {
	frame_unit::read(b, bigendian);
	version		= b.get<uint8>();
	augmentation= (const char*)b.p;
	b.p			+= strlen(augmentation) + 1;
	if (version < 3) {
		addr_size	= seg_size = 0;
	} else {
		addr_size	= b.get<uint8>();
		seg_size	= b.get<uint8>();
	}
	code_align	= b.get<leb128<uint32> >();
	data_align	= b.get<leb128<int32> >();
	return_reg	= b.get<leb128<uint32> >();
	init		= b.p;
}

uint64 frame_unit_cie_eh::read_encoded(byte_reader &b, EH_PE enc, bool bigendian) {
	if (enc == EH_PE_omit)
		return 0;
	switch (enc & 0xf) {
		case EH_PE_absptr:	return read_offset(b, bigendian);
		case EH_PE_uleb128:	return b.get<leb128<uint64> >();
		case EH_PE_udata2:	return endian(b.get<uint16>(), bigendian);
		case EH_PE_udata4:	return endian(b.get<uint32>(), bigendian);
		case EH_PE_udata8:	return endian(b.get<uint64>(), bigendian);
		case EH_PE_signed:	return read_offset(b, bigendian);
		case EH_PE_sleb128:	return b.get<leb128<int64> >();
		case EH_PE_sdata2:	return endian(b.get<int16>(), bigendian);
		case EH_PE_sdata4:	return endian(b.get<int32>(), bigendian);
		case EH_PE_sdata8:	return endian(b.get<int64>(), bigendian);
		default: return 0;
	}
}

void frame_unit_cie_eh::read(byte_reader &b, bool bigendian) {
	frame_unit::read(b, bigendian);
	version		= b.get<uint8>();

	const char	*augmentation = (const char*)b.p;
	b.p			+= strlen(augmentation) + 1;

	eh_data		= str(augmentation) == "eh" ? read_offset(b, bigendian) : 0;
	code_align	= b.get<leb128<uint32> >();
	data_align	= b.get<leb128<int32> >();
	return_reg	= version == 1 ? (uint32)b.get<uint8>() : (uint32)b.get<leb128<uint32> >();

	signal		= false;
	lsda_enc	= EH_PE_absptr;
	fde_enc		= EH_PE_absptr;
	personality	= 0;

	if (aug = (augmentation && augmentation[0] == 'z')) {
		const uint8	*end = b.p + b.get<leb128<uint32> >();
		while (char c = *++augmentation) {
			switch (c) {
				case 'L':	lsda_enc = EH_PE(b.get<uint8>()); break;
				case 'P':	personality = read_encoded(b, EH_PE(b.get<uint8>()), bigendian); break;
				case 'R':	fde_enc = EH_PE(b.get<uint8>()); break;
				case 'S':	signal = true; break;
			}
		}
	}
	init		= b.p;
}

void frame_machine::process(const uint8 *p, const uint8 *end) {
	for (byte_reader b(p); b.p < end;) {
		uint8	op	= b.getc();
		switch (op) {
			case CFA_nop:				break;
			case CFA_set_loc:			loc = get_val(b, cie.seg_size + cie.addr_size);
			case CFA_advance_loc1:		loc += b.get<uint8>() * cie.code_align; break;
			case CFA_advance_loc2:		loc += get<uint16>(b) * cie.code_align; break;
			case CFA_advance_loc4:		loc += get<uint32>(b) * cie.code_align; break;
			case CFA_offset_extended:	{ uint32 r = b.get<leb128<uint32> >(); setreg(r, offset, b.get<leb128<uint64> >() * cie.data_align); break; }
			case CFA_restore_extended:	restore(b.get<leb128<uint32> >()); break;
			case CFA_undefined:			setreg(b.get<leb128<uint32> >(), undef); break;
			case CFA_same_value:		setreg(b.get<leb128<uint32> >(), same); break;
			case CFA_register:			{ uint32 r = b.get<leb128<uint32> >(); setreg(r, reg, b.get<leb128<uint32> >()); break; }
			case CFA_remember_state:	stack.push_back(curr); break;
			case CFA_restore_state:		curr = stack.back(); stack.pop_back(); break;
			case CFA_def_cfa:			cfa_reg = b.get<leb128<uint32> >();//fall through
			case CFA_def_cfa_offset:	cfa_offset = b.get<leb128<uint64> >(); break;
			case CFA_def_cfa_register:	cfa_reg = b.get<leb128<uint32> >(); break;
			case CFA_def_cfa_expression:cfa_reg = -1; cfa_offset = intptr_t(b.p); b.p += b.get<leb128<uint32> >(); break;
			case CFA_expression:		{ uint32 r = b.get<leb128<uint32> >(); setreg(r, exp, b.p); b.p += b.get<leb128<uint32> >(); break; }
			case CFA_offset_extended_sf:{ uint32 r = b.get<leb128<uint32> >(); setreg(r, offset, b.get<leb128<int64> >() * cie.data_align); break; }
			case CFA_def_cfa_sf:		cfa_reg = b.get<leb128<uint32> >();// fall through
			case CFA_def_cfa_offset_sf:	cfa_offset = b.get<leb128<int64> >() * cie.data_align; break;
			case CFA_val_offset:		{ uint32 r = b.get<leb128<uint32> >(); setreg(r, valoff, b.get<leb128<uint64> >() * cie.data_align); break; }
			case CFA_val_offset_sf:		{ uint32 r = b.get<leb128<uint32> >(); setreg(r, valoff, b.get<leb128<int64> >() * cie.data_align); break; }
			case CFA_val_expression:	{ uint32 r = b.get<leb128<uint32> >(); setreg(r, valexp, b.p); b.p += b.get<leb128<uint32> >(); break; }
			default: switch (op & 0xc0) {
				case CFA_advance_loc:	loc += (op & 0x1f) * cie.code_align; break;
				case CFA_offset:		setreg(op & 0x1f, offset, b.get<leb128<uint64> >() * cie.data_align); break;
				case CFA_restore:		restore(op & 0x1f); break;
			}
		}
	}
}


const uint8 attr_classes[] = {
	ATC_none,			ATC_reference,		ATC_blk_loc,		ATC_string,			ATC_none,			ATC_none,			ATC_none,			ATC_none,
	ATC_none,			ATC_constant,		ATC_none,			ATC_blk_const_ref,	ATC_blk_const_ref,	ATC_blk_const_ref,	ATC_none,			ATC_none,
	ATC_lineptr,		ATC_address,		ATC_address,		ATC_constant,		ATC_none,			ATC_reference,		ATC_constant,		ATC_constant,
	ATC_reference,		ATC_blk_loc,		ATC_reference,		ATC_string,			ATC_blk_const_str,	ATC_reference,		ATC_reference,		ATC_none,
	ATC_constant,		ATC_flag,			ATC_blk_const_ref,	ATC_none,			ATC_none,			ATC_string,			ATC_none,			ATC_flag,
	ATC_none,			ATC_none,			ATC_blk_loc,		ATC_none,			ATC_constant,		ATC_none,			ATC_constant,		ATC_blk_const_ref,
	ATC_none,			ATC_reference,		ATC_constant,		ATC_constant,		ATC_flag,			ATC_reference,		ATC_constant,		ATC_blk_const_ref,
	ATC_blk_const_loc,	ATC_constant,		ATC_constant,		ATC_constant,		ATC_flag,			ATC_block,			ATC_constant,		ATC_flag,
	ATC_blk_loc,		ATC_reference,		ATC_constant,		ATC_macptr,			ATC_block,			ATC_reference,		ATC_blk_loc,		ATC_reference,
	ATC_blk_loc,		ATC_reference,		ATC_blk_loc,		ATC_flag,			ATC_constant,		ATC_blk_loc,		ATC_blk_const_ref,	ATC_blk_const_ref,
	ATC_block,			ATC_blk_const_ref,	ATC_address,		ATC_flag,			ATC_reference,		ATC_rangelistptr,	ATC_add_flg_ref_str,ATC_constant,
	ATC_constant,		ATC_constant,		ATC_string,			ATC_constant,		ATC_constant,		ATC_reference,		ATC_constant,		ATC_constant,
	ATC_string,			ATC_flag,			ATC_flag,			ATC_flag,			ATC_reference,		ATC_constant,		ATC_flag,			ATC_flag,
	ATC_flag,			ATC_reference,		ATC_flag,			ATC_constant,		ATC_flag,			ATC_flag,			ATC_string,
};

const char *attr_ids[] = {
	0,								"sibling",						"location",						"name",							0,								0,								0,								0,
	0,								"ordering",						0,								"byte_size",					"bit_offset",					"bit_size",						0,								0,
	"stmt_list",					"low_pc",						"high_pc",						"language",						0,								"discr",						"discr_value",					"visibility",
	"import",						"string_length",				"common_reference",				"comp_dir",						"const_value",					"containing_type",				"default_value",				0,
	"inline",						"is_optional",					"lower_bound",					0,								0,								"producer",						0,								"prototyped",
	0,								0,								"return_addr",					0,								"start_scope",					0,								"bit_stride",					"upper_bound",
	0,								"abstract_origin",				"accessibility",				"address_class",				"artificial",					"base_types",					"calling_convention",			"count",
	"data_member_location",			"decl_column",					"decl_file",					"decl_line",					"declaration",					"discr_list",					"encoding",						"external",
	"frame_base",					"friend",						"identifier_case",				"macro_info",					"namelist_item",				"priority",						"segment",						"specification",
	"static_link",					"type",							"use_location",					"variable_parameter",			"virtuality",					"vtable_elem_location",			"allocated",					"associated",
	"data_location",				"byte_stride",					"entry_pc",						"use_UTF8",						"extension",					"ranges",						"trampoline",					"call_column",
	"call_file",					"call_line",					"description",					"binary_scale",					"decimal_scale",				"small",						"decimal_sign",					"digit_count",
	"picture_string",				"mutable",						"threads_scaled",				"explicit",						"object_pointer",				"endianity",					"elemental",					"pure",
	"recursive",					"signature",					"main_subprogram",				"data_bit_offset",				 "const_expr",					"enum_class",					"linkage_name",
};

const char *tag_ids[] = {
	0,								"Dwarf_array_type",				"Dwarf_class_type",				"Dwarf_entry_point",			"Dwarf_enumeration_type",		"Dwarf_formal_parameter",		0,								0,
	"Dwarf_imported_declaration",	0,								"Dwarf_label",					"Dwarf_lexical_block",			0,								"Dwarf_member",					0,								"Dwarf_pointer_type",
	"Dwarf_reference_type",			"Dwarf_compile_unit",			"Dwarf_string_type",			"Dwarf_structure_type",			0,								"Dwarf_subroutine_type",		"Dwarf_typedef",				"Dwarf_union_type",
	"Dwarf_unspecified_parameters",	"Dwarf_variant",				"Dwarf_common_block",			"Dwarf_common_inclusion",		"Dwarf_inheritance",			"Dwarf_inlined_subroutine",		"Dwarf_module",					"Dwarf_ptr_to_member_type",
	"Dwarf_set_type",				"Dwarf_subrange_type",			"Dwarf_with_stmt",				"Dwarf_access_declaration",		"Dwarf_base_type",				"Dwarf_catch_block",			"Dwarf_const_type",				"Dwarf_constant",
	"Dwarf_enumerator",				"Dwarf_file_type",				"Dwarf_friend",					"Dwarf_namelist",				"Dwarf_namelist_item",			"Dwarf_packed_type",			"Dwarf_subprogram",				"Dwarf_template_type_parameter",
	"Dwarf_template_value_parameter","Dwarf_thrown_type",			"Dwarf_try_block",				"Dwarf_variant_part",			"Dwarf_variable",				"Dwarf_volatile_type",			"Dwarf_dwarf_procedure",		"Dwarf_restrict_type",
	"Dwarf_interface_type",			"Dwarf_namespace",				"Dwarf_imported_module",		"Dwarf_unspecified_type",		"Dwarf_partial_unit",			"Dwarf_imported_unit",			0,								"Dwarf_condition",
	"Dwarf_shared_type",			"Dwarf_type_unit",				"Dwarf_rvalue_reference_type",	"Dwarf_template_alias",
};

uint8 GetClass(ATTRIBUTE attr) {
	return attr_classes[attr];
}

}//namespace dwarf
