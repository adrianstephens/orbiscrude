#include "java.h"
#include "iso/iso_files.h"
#include "base/functions.h"

//-----------------------------------------------------------------------------
//	Java data and code
//-----------------------------------------------------------------------------


using namespace java;

//-----------------------------------------------------------------------------
//	ClassFile
//-----------------------------------------------------------------------------

struct ClassFile2 : ClassFile {

	ClassFile2(malloc_block &&block) : ClassFile(block.detach()) {
		byte_reader	r(header + 1);

		uint32	n = r.get<uint16be>();
		_constants.resize(n);
		for (int i = 1; i < n; i++) {
			_constants[i] = readp<Constant>(r);
			if (_constants[i]->tag == Constant::Long || _constants[i]->tag == Constant::Double)
				++i;
		}

		read(r, _info, _interfaces, _fields, _methods, _attributes);
	}
	~ClassFile2() {
		iso::free(header);
	}

	auto constants()	const { return with_param(_constants, this); }
	auto info()			const { return make_param_element(*_info, this); }
	auto interfaces()	const { return with_param(_interfaces, this); }
	auto fields()		const { return with_param(_fields, this); }
	auto methods()		const { return with_param(_methods, this); }
	auto attributes()	const { return with_param(_attributes, this); }

	ISO::Browser2	get_constant(const Constant *c) const {
		if (!c)
			return ISO::Browser2();
		switch (c->tag) {
			case Constant::Utf8:				return ISO::MakeBrowser(string(c->info<Constant::Utf8>().get()));
			case Constant::Integer:				return ISO::MakeBrowser(make_param_element(c->info<Constant::Integer			>(), this));
			case Constant::Float:				return ISO::MakeBrowser(make_param_element(c->info<Constant::Float				>(), this));
			case Constant::Long:				return ISO::MakeBrowser(make_param_element(c->info<Constant::Long				>(), this));
			case Constant::Double:				return ISO::MakeBrowser(make_param_element(c->info<Constant::Double				>(), this));
			case Constant::Class:				return ISO::MakeBrowser(make_param_element(c->info<Constant::Class				>(), this));
			case Constant::String:				return ISO::MakeBrowser(make_param_element(c->info<Constant::String				>(), this));
			case Constant::Fieldref:			return ISO::MakeBrowser(make_param_element(c->info<Constant::Fieldref			>(), this));
			case Constant::Methodref:			return ISO::MakeBrowser(make_param_element(c->info<Constant::Methodref			>(), this));
			case Constant::InterfaceMethodref:	return ISO::MakeBrowser(make_param_element(c->info<Constant::InterfaceMethodref	>(), this));
			case Constant::NameAndType:			return ISO::MakeBrowser(make_param_element(c->info<Constant::NameAndType		>(), this));
			case Constant::MethodHandle:		return ISO::MakeBrowser(make_param_element(c->info<Constant::MethodHandle		>(), this));
			case Constant::MethodType:			return ISO::MakeBrowser(make_param_element(c->info<Constant::MethodType			>(), this));
			case Constant::InvokeDynamic:		return ISO::MakeBrowser(make_param_element(c->info<Constant::InvokeDynamic		>(), this));
			default:							return ISO::MakeBrowser(make_param_element(*c, this));
		}
	}
	ISO::Browser2	get_constant(int i) const {
		return get_constant(ClassFile::get_constant(i));
	}

	ISO::Browser2	get_attribute(const Attribute *a) const {
		auto	name = ClassFile::get_constant<Constant::Utf8>(a->name.idx)->get();

		if (name == "SourceFile") {
			return ISO::MakePtr(name, make_param_element(*(SourceFile*)a, this));

		} else if (name == "EnclosingMethod") {
			return ISO::MakePtr(name, make_param_element(*(EnclosingMethod*)a, this));

//		} else if (name == "InnerClasses") {
//			return ISO::MakePtr(name, make_param_element(*(InnerClasses*)a, this));

		} else if (name == "ConstantValue") {
			return ISO::MakePtr(name, make_param_element(*(ConstantValue*)a, this));

		} else if (name == "Code") {
			return ISO::MakePtr(name, make_param_element(*(Code*)a, this));

		} else {
			ISO_ptr<ISO_openarray<uint8> >	p(name, a->length);
			memcpy(*p, a + 1, a->length);
			return p;
		}
	}
};

tag2 _GetName(const param_element<const Attribute&, const ClassFile2*> &a) {
	return a.p->ClassFile::get_constant<Constant::Utf8>(a.t.name.idx)->get();
}

template<typename T> auto	get(param_element<const T*&, const ClassFile2*> &a)	{
	return param_element<const T&, const ClassFile2*>(*a.t, a.p);
}

template<typename C> const_memory_block	get(const param_element<embedded_array<packed<uint8>, C>&, const ClassFile2*> &a)	{
	return const_memory_block(a.t.begin(), a.t.size());
}

template<typename T, typename C> auto	get(const param_element<embedded_array<T, C>&, const ClassFile2*> &a)	{
	return with_param(a.t, a.p);
}
template<typename T, typename C> auto	get(param_element<embedded_array<T, C>&, const ClassFile2*> &a)	{
	return with_param(a.t, a.p);
}

template<typename T, typename C> auto	get(const param_element<embedded_next_array<T, C>&, const ClassFile2*> &a)	{
	return with_param(a.t, a.p);
}
template<typename T, typename C> auto	get(param_element<embedded_next_array<T, C>&, const ClassFile2*> &a)	{
	return with_param(a.t, a.p);
}

template<typename C> ISO::Browser2 get(const param_element<java::index<C>&, const ClassFile2*> &a)	{
	return a.p->get_constant(a.t.idx);
}

ISO::Browser2	get(param_element<Constant&, const ClassFile2*> &a) {
	return a.p->get_constant(&a.t);
}
ISO::Browser2	get(param_element<Attribute&, const ClassFile2*> &a)	{
	return a.p->get_attribute(&a.t);
}


ISO_DEFUSERENUMXFQV(Constant::CONSTANT, "CONSTANT", 8, NONE,
	Utf8, Integer, Float, Long, Double, Class, String, Fieldref,
	Methodref,  InterfaceMethodref, NameAndType, MethodHandle, MethodType, InvokeDynamic
);

ISO_DEFUSERCOMPPV(ConstantT<Constant::Class>,				const ClassFile2*,name);
ISO_DEFUSERCOMPPV(ConstantT<Constant::Fieldref>,			const ClassFile2*,clazz, name_and_type);
ISO_DEFUSERCOMPPV(ConstantT<Constant::Methodref>,			const ClassFile2*,clazz, name_and_type);
ISO_DEFUSERCOMPPV(ConstantT<Constant::InterfaceMethodref>,	const ClassFile2*,clazz, name_and_type);
ISO_DEFUSERCOMPPV(ConstantT<Constant::String>,				const ClassFile2*,string);
ISO_DEFUSERCOMPPV(ConstantT<Constant::Integer>,				const ClassFile2*,v);
ISO_DEFUSERCOMPPV(ConstantT<Constant::Float>,				const ClassFile2*,v);
ISO_DEFUSERCOMPPV(ConstantT<Constant::Long>,				const ClassFile2*,v);
ISO_DEFUSERCOMPPV(ConstantT<Constant::Double>,				const ClassFile2*,v);
ISO_DEFUSERCOMPPV(ConstantT<Constant::NameAndType>,			const ClassFile2*,name, descriptor);
ISO_DEFUSERCOMPPV(ConstantT<Constant::Utf8>,				const ClassFile2*,len);
ISO_DEFUSERCOMPPV(ConstantT<Constant::MethodHandle>,		const ClassFile2*,reference_kind, reference);
ISO_DEFUSERCOMPPV(ConstantT<Constant::MethodType>,			const ClassFile2*,descriptor);
ISO_DEFUSERCOMPPV(ConstantT<Constant::InvokeDynamic>,		const ClassFile2*,bootstrap_method_attr, name_and_type);

ISO_DEFUSERCOMPPV(ConstantValue,							const ClassFile2*,constantvalue);

ISO_DEFUSERCOMPV(Code::Exception, start_pc, end_pc, handler_pc, catch_type);
ISO_DEFUSERCOMPPV(Code,										const ClassFile2*,max_stack, max_locals, code, exception_table, attributes);
//ISO_DEFUSERCOMPPV(Code,									const ClassFile2*,max_stack, max_locals, code, exception_table);

ISO_DEFUSERCOMPV(ElementValue, tag);

//ISO_DEFUSERCOMPV(StackMapTable::VerificationItem, tag);
//ISO_DEFUSERCOMPV(StackMapTable::ObjectVerificationItem, cpool);
//ISO_DEFUSERCOMPV(StackMapTable::UninitializedVerificationItem, offset);
ISO_DEFUSERCOMPV(StackMapTable::Frame, type);
//ISO_DEFUSERCOMPP(same_frame};
//ISO_DEFUSERCOMPP(same_locals_1_stack_item_frame, stack);
//ISO_DEFUSERCOMPP(same_locals_1_stack_item_frame_extended, offset_delta, stack[1]);
//ISO_DEFUSERCOMPP(chop_frame, offset_delta);
//ISO_DEFUSERCOMPP(same_frame_extended,offset_delta);
//ISO_DEFUSERCOMPP(append_frame, offset_delta);
//ISO_DEFUSERCOMPP(full_frame, offset_delta, locals, stack);
ISO_DEFUSERCOMPPV(StackMapTable,							const ClassFile2*,entries);
ISO_DEFUSERCOMPPV(Exceptions,								const ClassFile2*,exception_index_table);

//ISO_DEFUSERCOMPV(InnerClasses::Class, inner_class_info, outer_class_info, inner_name, inner_class_access_flags);
//ISO_DEFUSERCOMPPV(InnerClasses,                           const ClassFile2*,classes);
ISO_DEFUSERCOMPPV(EnclosingMethod,							const ClassFile2*,clazz, method);
//ISO_DEFUSERCOMPPV(Synthetic,								const ClassFile2*,};
ISO_DEFUSERCOMPPV(Signature,								const ClassFile2*,signature);
ISO_DEFUSERCOMPPV(SourceFile,								const ClassFile2*,sourcefile);
//ISO_DEFUSERCOMPPV(SourceDebugExtension,					const ClassFile2*,debug_extension);
//ISO_DEFUSERCOMPV(LineNumberTable::Record, start_pc, line_number);
//ISO_DEFUSERCOMPPV(LineNumberTable,						const ClassFile2*,line_number_table);
//ISO_DEFUSERCOMPV(LocalVariableTable::Record, start_pc, length, name, descriptor, index);
//ISO_DEFUSERCOMPPV(LocalVariableTable,						const ClassFile2*,local_variable_table);
//ISO_DEFUSERCOMPV(LocalVariableTypeTable::Record, start_pc, length, name, signature, index);
//ISO_DEFUSERCOMPPV(LocalVariableTypeTable,					const ClassFile2*,local_variable_type_table);
//ISO_DEFUSERCOMPPV(Deprecated,								const ClassFile2*,};
//ISO_DEFUSERCOMPPV(RuntimeVisibleAnnotations,				const ClassFile2*,annotations);
//ISO_DEFUSERCOMPPV(RuntimeInvisibleAnnotations,			const ClassFile2*,annotations);
//ISO_DEFUSERCOMPPV(RuntimeVisibleParameterAnnotations,		const ClassFile2*,parameter_annotations);
//ISO_DEFUSERCOMPPV(RuntimeInvisibleParameterAnnotations,	const ClassFile2*,parameter_annotations);
ISO_DEFUSERCOMPPV(AnnotationDefault,						const ClassFile2*,default_value);

//ISO_DEFUSERCOMPV(BootstrapMethods::Record, bootstrap_method_ref, bootstrap_arguments);
//ISO_DEFUSERCOMPPV(BootstrapMethods,						const ClassFile2*,bootstrap_methods);

ISO_DEFUSERCOMPPV(Constant,			const ClassFile2*,tag);
ISO_DEFUSERCOMPPV(ClassInfo,		const ClassFile2*,access_flags, this_class, super_class);
ISO_DEFUSERCOMPPV(FieldorMethod,	const ClassFile2*,access_flags, name, descriptor, attributes);

ISO_DEFUSERCOMPV(ClassFile2, constants);//, info, interfaces, fields, methods, attributes);

class JavaClass_FileHandler : public FileHandler {
	const char*			GetExt() override { return "class";	}
	const char*			GetDescription() override { return "JAVA class file";	}
	ISO_ptr<void>		Read(tag id, istream_ref file) override;
} java_class;

ISO_ptr<void> JavaClass_FileHandler::Read(tag id, istream_ref file) {
	java::ClassHeader	header;

	if (!file.read(header) || !header.valid())
		return ISO_NULL;

	file.seek(0);
	return ISO_ptr<ClassFile2>(id, malloc_block(file, file.length()));
}

//-----------------------------------------------------------------------------
//	Disassembler
//-----------------------------------------------------------------------------

#ifdef ISO_EDITOR
#include "disassembler.h"

static const char *NewArrayTypes[] = {
	"boolean",	//	4
	"char",		//	5
	"float",	//	6
	"double",	//	7
	"byte",		//	8
	"short",	//	9
	"int",		//	10
	"long",		//	11
};

namespace java {
OPCODE_INFO ops[] = {
//constants
	"nop",				NONE,
	"aconst_null",		NONE,
	"iconst_m1",		NONE,
	"iconst_0",			NONE,
	"iconst_1",			NONE,
	"iconst_2",			NONE,
	"iconst_3",			NONE,
	"iconst_4",			NONE,
	"iconst_5",			NONE,
	"lconst_0",			NONE,
	"lconst_1",			NONE,
	"fconst_0",			NONE,
	"fconst_1",			NONE,
	"fconst_2",			NONE,
	"dconst_0",			NONE,
	"dconst_1",			NONE,
	"bipush",			CONSTANT,
	"sipush",			CONSTANT2,
	"ldc",				INDEX,
	"ldc_w",			INDEX2,
	"ldc2_w",			INDEX2,
//loads
	"iload",			INDEX,
	"lload",			INDEX,
	"fload",			INDEX,
	"dload",			INDEX,
	"aload",			INDEX,
	"iload_0",			NONE,
	"iload_1",			NONE,
	"iload_2",			NONE,
	"iload_3",			NONE,
	"lload_0",			NONE,
	"lload_1",			NONE,
	"lload_2",			NONE,
	"lload_3",			NONE,
	"fload_0",			NONE,
	"fload_1",			NONE,
	"fload_2",			NONE,
	"fload_3",			NONE,
	"dload_0",			NONE,
	"dload_1",			NONE,
	"dload_2",			NONE,
	"dload_3",			NONE,
	"aload_0",			NONE,
	"aload_1",			NONE,
	"aload_2",			NONE,
	"aload_3",			NONE,
	"iaload",			NONE,
	"laload",			NONE,
	"faload",			NONE,
	"daload",			NONE,
	"aaload",			NONE,
	"baload",			NONE,
	"caload",			NONE,
	"saload",			NONE,
//stores
	"istore",			INDEX,
	"lstore",			INDEX,
	"fstore",			INDEX,
	"dstore",			INDEX,
	"astore",			INDEX,
	"istore_0",			NONE,
	"istore_1",			NONE,
	"istore_2",			NONE,
	"istore_3",			NONE,
	"lstore_0",			NONE,
	"lstore_1",			NONE,
	"lstore_2",			NONE,
	"lstore_3",			NONE,
	"fstore_0",			NONE,
	"fstore_1",			NONE,
	"fstore_2",			NONE,
	"fstore_3",			NONE,
	"dstore_0",			NONE,
	"dstore_1",			NONE,
	"dstore_2",			NONE,
	"dstore_3",			NONE,
	"astore_0",			NONE,
	"astore_1",			NONE,
	"astore_2",			NONE,
	"astore_3",			NONE,
	"iastore",			NONE,
	"lastore",			NONE,
	"fastore",			NONE,
	"dastore",			NONE,
	"aastore",			NONE,
	"bastore",			NONE,
	"castore",			NONE,
	"sastore",			NONE,
//stack
	"pop",				NONE,
	"pop2",				NONE,
	"dup",				NONE,
	"dup_x1",			NONE,
	"dup_x2",			NONE,
	"dup2",				NONE,
	"dup2_x1",			NONE,
	"dup2_x2",			NONE,
	"swap",				NONE,
//math
	"iadd",				NONE,
	"ladd",				NONE,
	"fadd",				NONE,
	"dadd",				NONE,
	"isub",				NONE,
	"lsub",				NONE,
	"fsub",				NONE,
	"dsub",				NONE,
	"imul",				NONE,
	"lmul",				NONE,
	"fmul",				NONE,
	"dmul",				NONE,
	"idiv",				NONE,
	"ldiv",				NONE,
	"fdiv",				NONE,
	"ddiv",				NONE,
	"irem",				NONE,
	"lrem",				NONE,
	"frem",				NONE,
	"drem",				NONE,
	"ineg",				NONE,
	"lneg",				NONE,
	"fneg",				NONE,
	"dneg",				NONE,
	"ishl",				NONE,
	"lshl",				NONE,
	"ishr",				NONE,
	"lshr",				NONE,
	"iushr",			NONE,
	"lushr",			NONE,
	"iand",				NONE,
	"land",				NONE,
	"ior",				NONE,
	"lor",				NONE,
	"ixor",				NONE,
	"lxor",				NONE,
	"iinc",				INDEX_CONST,
//conversion
	"i2l",				NONE,
	"i2f",				NONE,
	"i2d",				NONE,
	"l2i",				NONE,
	"l2f",				NONE,
	"l2d",				NONE,
	"f2i",				NONE,
	"f2l",				NONE,
	"f2d",				NONE,
	"d2i",				NONE,
	"d2l",				NONE,
	"d2f",				NONE,
	"i2b",				NONE,
	"i2c",				NONE,
	"i2s",				NONE,
//comparison
	"lcmp",				NONE,
	"fcmpl",			NONE,
	"fcmpg",			NONE,
	"dcmpl",			NONE,
	"dcmpg",			NONE,
	"ifeq",				BRANCH2,
	"ifne",				BRANCH2,
	"iflt",				BRANCH2,
	"ifge",				BRANCH2,
	"ifgt",				BRANCH2,
	"ifle",				BRANCH2,
	"if_icmpeq",		BRANCH2,
	"if_icmpne",		BRANCH2,
	"if_icmplt",		BRANCH2,
	"if_icmpge",		BRANCH2,
	"if_icmpgt",		BRANCH2,
	"if_icmple",		BRANCH2,
	"if_acmpeq",		BRANCH2,
	"if_acmpne",		BRANCH2,
//control
	"goto",				BRANCH2,
	"jsr",				BRANCH2,
	"ret",				INDEX,
	"tableswitch",		TABLESWITCH,
	"lookupswitch",		LOOKUPSWITCH,
	"ireturn",			NONE,
	"lreturn",			NONE,
	"freturn",			NONE,
	"dreturn",			NONE,
	"areturn",			NONE,
	"return",			NONE,
//references
	"getstatic",		INDEX2,
	"putstatic",		INDEX2,
	"getfield",			INDEX2,
	"putfield",			INDEX2,
	"invokevirtual",	INDEX2,
	"invokespecial",	INDEX2,
	"invokestatic",		INDEX2,
	"invokeinterface",	INDEX2_COUNT_0,
	"invokedynamic",	INDEX2_0_0,
	"new",				INDEX2,
	"newarray",			TYPE,
	"anewarray",		INDEX2,
	"arraylength",		NONE,
	"athrow",			NONE,
	"checkcast",		INDEX2,
	"instanceof",		INDEX2,
	"monitorenter",		NONE,
	"monitorexit",		NONE,
//extended
	"wide",				WIDE,
	"multianewarray",	INDEX2_DIMS,
	"ifnull",			NONE,
	"ifnonnull",		NONE,
	"goto_w",			BRANCH4,
	"jsr_w",			BRANCH4,
//reserved
	"breakpoint",		NONE,
//	"impdep1",			NONE,
//	"impdep2",			NONE,
};

const OPCODE_INFO &get_info(OPCODE op) {
	return ops[op];
}

uint32 get_len(PARAMS params) {
	return params >= WIDE ? 0 : (params & 0xf) + 1;
}

uint32 get_len(const uint8 *p) {
	PARAMS params = get_info((OPCODE)p[0]).params;
	switch (params) {
		case WIDE:			return p[1] == op_iinc ? 5 : 3;
		case TABLESWITCH:	return ((const TableSwitch*)align(p, 4))->end() - p;
		case LOOKUPSWITCH:	return ((const LookupSwitch*)align(p, 4))->end() - p;
		default: 			return (params & 0xf) + 1;
	}
}

}


class DisassemblerJAVA : public Disassembler {
public:
	const char*	GetDescription() override { return "JAVA bytecode"; }
	State*		Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) override;
	static uint8		GetNextOp(byte_reader &r, uint32 *params);
} javabytecode;

Disassembler::State *DisassemblerJAVA::Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) {
	StateDefault	*state	= new StateDefault;
	const uint8		*p		= block;
	while (p < block.end()) {
		uint32	offset		= p - block;
		uint8	op			= *p;
		PARAMS	params		= ops[op].params;
		uint8	len;

		switch (params) {
			case WIDE:			len = p[1] == op_iinc ? 5 : 3; break;
			case TABLESWITCH:	len	= ((TableSwitch*)align(p, 4))->end() - p; break;
			case LOOKUPSWITCH:	len	= ((LookupSwitch*)align(p, 4))->end() - p; break;
			default: 			len = (params & 0xf) + 1; break;
		}

		buffer_accum<1024>	ba("%08x ", offset);
		for (int i = 0; i < min(len, 6); i++)
			ba.format("%02x ", p[i]);
		for (int i = len; i < 6; i++)
			ba << "   ";

		if (op <= op_breakpoint)
			ba << ops[op].op;

		if (len > 1)
			ba << "  ";

		switch (params) {
			case INDEX:			ba << "0x" << hex(p[1]); break;
			case INDEX2_0_0:
			case INDEX2:		ba << "0x" << hex(*(packed<uint16be>*)(p + 1)); break;
			case CONSTANT:		ba << int8(p[1]); break;
			case CONSTANT2:		ba << get(*(packed<int16be>*)(p + 1)); break;
			case BRANCH2:		ba << "0x" << hex(offset + *(packed<int16be>*)(p + 1)); break;
			case BRANCH4:		ba << "0x" << hex(offset + (int32)*(packed<int32be>*)(p + 1)); break;
			case TYPE:			ba << (p[1] >= 4 && p[1] <= 11 ? NewArrayTypes[p[1] - 4] : "illegal");
			case INDEX_CONST:	ba << hex(p[1]) << ", " << int8(p[2]); break;
			case INDEX2_COUNT_0:
			case INDEX2_DIMS:	ba << "0x" << hex(*(packed<uint16be>*)(p + 1)) << ", " << p[3]; break;
			case WIDE:
				ba << ops[p[1]].op << " 0x" << hex(*(packed<int16be>*)(p + 2));
				if (p[1] == op_iinc)
					ba << ", " << *(packed<int16be>*)(p + 1);
				break;
			case TABLESWITCH: {
				TableSwitch		*t = (TableSwitch*)align(p, 4);
				for (int i = 0, n = t->high - t->low; i <= n; ++i)
					ba << i + t->low << ": 0x" << hex(offset + t->offsets[i]) << ", ";
				ba << "default: 0x" << hex(offset + t->def);
				break;
			}
			case LOOKUPSWITCH: {
				LookupSwitch	*t = (LookupSwitch*)align(p, 4);
				for (int i = 0, n = t->count; i < n; ++i)
					ba << t->pairs[i].val << ": 0x" << hex(offset + t->pairs[i].offset) << ", ";
				ba << "default: 0x" << hex(offset + t->def);
				break;
			}
		}

		p += len;
		state->lines.push_back(ba);
	}
	return state;
}

#endif	//ISO_EDITOR

