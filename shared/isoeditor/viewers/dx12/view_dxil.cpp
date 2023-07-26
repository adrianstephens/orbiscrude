#include "view_dxil.h"
#include "disassemblers/dis_bitcode.h"
#include "../viewer_identifier.h"
#include "dxcapi.h"

using namespace app;

struct DXBCbuilder : dx::DXBC_base {
	struct Blob1 {
		FourCC	code;
		const_memory_block	data;
		Blob1(const BlobHeader &b) : code(b.code), data(b.data()) {}
		Blob1(FourCC code, const_memory_block data) : code(code), data(data) {}
	};

	Version		version;
	dynamic_array<Blob1>	blobs;

	DXBCbuilder()	{}
	DXBCbuilder(const dx::DXBC *dxbc) : version(dxbc->version), blobs(dxbc->Blobs()) {}

	bool	add_part(FourCC code, const_memory_block data) {
		for (auto& i : blobs) {
			if (i.code == code)
				return false;
		}
		blobs.emplace_back(code, data);
		return true;
	}

	bool	remove_part(FourCC code) {
		for (auto& i : blobs) {
			if (i.code == code) {
				blobs.erase_unordered(&i);
				return true;
			}
		}
		return false;
	}

	bool	write(ostream_ref file) {
		size_t	size = sizeof(dx::DXBC);
		for (auto &i : blobs)
			size += sizeof(BlobHeader) + i.data.size() + sizeof(uint32);

		dx::DXBC	header;
		clear(header.digest);
		header.HeaderFourCC	= "DXBC"_u32;
		header.version		= version;
		header.size			= size;
		header.num_blobs	= blobs.size();
		file.write(header);

		uint32 offset = sizeof(header) + blobs.size() * sizeof(uint32);
		for (auto &i : blobs) {
			file.write(offset);
			offset += sizeof(BlobHeader) + i.data.size();
		}

		for (auto &i : blobs)
			file.write(i.code, i.data.size32(), i.data);

		return true;
	}
};

struct DxcBloc : com<IDxcBlob> {
	const_memory_block	b;
public:
	LPVOID STDMETHODCALLTYPE GetBufferPointer()	{ return unconst(b.begin()); }
	SIZE_T STDMETHODCALLTYPE GetBufferSize()	{ return b.size(); }

	DxcBloc(const_memory_block b) : b(b) {}
	operator IDxcBlob*() { return this; }
};

bool app::ReadDXILModule(bitcode::Module &mod, const dx::DXBC::BlobT<dx::DXBC::DXIL> *dxil) {
	return	dxil && dxil->valid()
		&&	mod.read(memory_reader(dxil->data()));
}

malloc_block app::SignShader(const dx::DXBC *dxbc) {
	HMODULE dxil_module = ::LoadLibrary("dxil.dll");
	DxcCreateInstanceProc create = (DxcCreateInstanceProc)GetProcAddress(dxil_module, "DxcCreateInstance");

	com_ptr<IDxcValidator> validator;
	create(CLSID_DxcValidator, COM_CREATE(&validator));

	com_ptr<IDxcOperationResult> result;
	validator->Validate(DxcBloc(dxbc->data()), DxcValidatorFlags_InPlaceEdit, &result);

	HRESULT status;
	result->GetStatus(&status);

	if (SUCCEEDED(status)) {
		com_ptr<IDxcBlob> signed_blob;
		result->GetResult(&signed_blob);
		return const_memory_block(signed_blob->GetBufferPointer(), signed_blob->GetBufferSize());
	}

	com_ptr<IDxcBlobEncoding> printBlob;
	result->GetErrorBuffer(&printBlob);
	ISO_TRACEF() << (const char*)printBlob->GetBufferPointer();

	return none;
}

malloc_block app::ReplaceDXIL(const dx::DXBC *dxbc, const bitcode::Module &mod, bool sign) {
	auto dxil0	= dxbc->GetBlob<dx::DXBC::DXIL>();

	dynamic_memory_writer	out(0x10000);
	out.write(dx::DXBC::BlobT<dx::DXBC::DXIL>(dxil0->ProgramType));
	mod.write(out);

	auto dxil1	= unconst((const dx::DXBC::BlobT<dx::DXBC::DXIL>*)out.data());
	dxil1->set_total_size(out.data().size());

	DXBCbuilder	builder(dxbc);
	builder.remove_part(dx::DXBC::DXIL);
	builder.add_part(dx::DXBC::DXIL, out.data());

	dynamic_memory_writer	out2(0x10000);
	builder.write(out2);

	if (sign)
		return SignShader(out2.data());

	return out2;
}

void app::GetDXILSource(const bitcode::Module &mod, Disassembler::Files &files, Disassembler::SharedFiles &shared_files) {
	if (auto ref = mod.GetMeta("dx.source.contents")) {
	#if 1
		dynamic_array<bitcode::MetadataRef>	contents(ref);
		for (dynamic_array<bitcode::MetadataRef> i : contents) {
			string	name(i[0]), source(i[1]);
			auto	file	= shared_files[source];
			auto	id		= hash(source);
			if (!file.exists())
				file = new Disassembler::File(move(name), move(source));
			files[id] = file;
		}
	#else
		dynamic_array<bitcode::MetadataRef>	contents(ref[0]);
		for (auto &i : make_split_range<2>(contents)) {
			string	name(i[0]), source(i[1]);
			auto	file	= shared_files[source];
			auto	id		= hash(source);
			if (!file.exists())
				file = new Disassembler::File(move(name), move(source));
			files[id] = file;
		}
	#endif
	}
}

dxil::meta_entryPoint app::GetEntryPoint(const bitcode::Module &mod, const char *name) {
	for (dxil::meta_entryPoint i : mod.GetMeta("dx.entryPoints")) {
		if (auto func = (const bitcode::Function*)i.function) {
			if (!name || func->name == name)
				return i;
		}
	}
	for (dxil::meta_entryPoint i : mod.GetMeta("dx.entryPoints")) {
		if (auto func = (const bitcode::Function*)i.function)
			return i;
	}
	return mod.GetMeta("dx.entryPoints")[0];
}

void app::GetDXILSourceLines(const bitcode::Module &mod, Disassembler::Files &files, Disassembler::Locations &locations, const char *name) {
	const bitcode::Function*	func(GetEntryPoint(mod, name).function);
	if (func) {
		hash_map<const char*, uint32>	filename_to_id;
		for (auto& i : files)
			filename_to_id[i.b->name] = i.a;

		for (auto &i : func->instructions) {
			if (auto loc = i->debug_loc) {
				if (i->op == bitcode::Operation::Call && i->funcCall->name.begins("llvm."))
					continue;

				auto	fn		= loc->get_filename();
				auto	file_id	= filename_to_id[fn];
				ISO_ASSERT(file_id.exists());
				auto	&loc2	= locations.emplace_back(func->instructions.index_of(i) + 1, file_id, loc->line, loc->col);
				loc2.func_id	= hash(loc->get_name());
			}
		}
	}
}

string app::GetDXILShaderName(const dx::DXBC::BlobT<dx::DXBC::DXIL> *dxil, uint64 addr) {
	bitcode::Module mod;
	if (ReadDXILModule(mod, dxil)) {
		dxil::meta_entryPoint	entry(mod.GetMetaFirst("dx.entryPoints"));
		return entry.name;//((const bitcode::Function*)entry.function)->name;
	}

	return (format_string("dxil_") << hex(addr)).term();
}

struct DXIL_type_converter {
	C_types			&types;
	C_type_struct*	curr	= 0;

	const C_type *process(const bitcode::DebugInfo* d) {
		return d ? bitcode::process<const C_type*>(d, *this) : nullptr;
	}

	const C_type *operator()(const bitcode::DIFile* p)			{ return nullptr; }
	const C_type *operator()(const bitcode::DICompileUnit* p)	{ return nullptr; }
	const C_type *operator()(const bitcode::DIBasicType* p) {
		switch (p->encoding) {
			case dwarf::ATE_unknown:		break;
			case dwarf::ATE_address:		break;
			case dwarf::ATE_boolean:		return types.get_type<bool>();
			case dwarf::ATE_complex_float:	break;
			case dwarf::ATE_float:			return types.add(C_type_float(p->size_bits));
			case dwarf::ATE_signed:			return types.add(C_type_int(p->size_bits, true));
			case dwarf::ATE_signed_char:	break;
			case dwarf::ATE_unsigned:		return types.add(C_type_int(p->size_bits, false));
			case dwarf::ATE_unsigned_char:	break;
			case dwarf::ATE_imaginary_float:break;
			case dwarf::ATE_packed_decimal:	break;
			case dwarf::ATE_numeric_string:	break;
			case dwarf::ATE_edited:			break;
			case dwarf::ATE_signed_fixed:	break;
			case dwarf::ATE_unsigned_fixed:	break;
			case dwarf::ATE_decimal_float:	break;
			case dwarf::ATE_UTF:			break;
		}
		return nullptr;
	}

	const C_type *operator()(const bitcode::DIDerivedType* p) {
		if (p->tag == dwarf::TAG_member) {
			curr->add_atoffset(*p->name, process(p->base), p->offset_bits / 8);
			return nullptr;
		}

		return process(p->base);
	}

	const C_type *operator()(const bitcode::DICompositeType* p) {
		auto	base = process(p->base);

		switch (p->tag) {
			case dwarf::TAG_array_type: {
				for (auto &i : p->elements.children()) {
					const bitcode::DebugInfo* c = i;
					if (c->kind == bitcode::METADATA_SUBRANGE)
						base = types.add(C_type_array(base, c->as<bitcode::DISubrange>()->count));
				}
				return base;
			}
			case dwarf::TAG_class_type:
			case dwarf::TAG_structure_type: {
				C_type_struct	s(*p->name);
				auto	saved = save(curr, &s);
				//ctypes.add(p->name, curr);
				for (auto &i : p->elements.children())
					process(i);
				return types.add(s);
			}
			default:
				return nullptr;
		}

	}

	//const C_type *operator()(const bitcode::DITemplateTypeParameter* p)	{ return nullptr; }
	//const C_type *operator()(const bitcode::DITemplateValueParameter* p)	{ return nullptr; }
	//const C_type *operator()(const bitcode::DISubprogram* p)				{ return nullptr; }
	//const C_type *operator()(const bitcode::DISubroutineType* p)			{ return nullptr; }
	//const C_type *operator()(const bitcode::DIGlobalVariable* p)			{ return nullptr; }
	//const C_type *operator()(const bitcode::DILocalVariable* p)			{ return nullptr; }
	//const C_type *operator()(const bitcode::DILexicalBlock* p)			{ return nullptr; }
	//const C_type *operator()(const bitcode::DISubrange* p)				{ return nullptr; }
	//const C_type *operator()(const bitcode::DIExpression* p)				{ return nullptr; }
	//const C_type *operator()(const bitcode::DILocation* p)				{ return nullptr; }
	const C_type *operator()(const bitcode::DebugInfo*p)					{ return nullptr; }

	DXIL_type_converter(C_types &types) : types(types) {}
};

//-----------------------------------------------------------------------------
//	C_type
//-----------------------------------------------------------------------------

#define CT(T)		ctypes.get_type<T>()
#define CTS2(T,N)	ctypes.get_type<T>(N)
#define CTS(T)		ctypes.get_type<T>(#T)

const C_type* to_c_type(dxil::ComponentType comp) {
	static const C_type*	types[] = {
		0,
		CT(bool),
		CT(int16),
		CT(uint16),
		CT(int),
		CTS2(uint32,"uint"),
		CT(int64),
		CT(uint64),
		CTS2(float16, "half"),
		CT(float),
		CT(double),
	#if 1
		CT(norm16),
		CT(unorm16),
		CT(norm32),
		CT(unorm32),
//		CT(norm64),
//		CT(unorm64),
	#else
		CT(int16),
		CT(uint16),
		CT(int32),
		CT(uint32),
		CT(int64),
		CT(uint64),
	#endif
	};
	return types[comp];
}
#undef CT

const C_type *struct_to_c_type(const char *name, const dxil::Type* type, const dxil::TypeSystem& types);

const C_type *make_vector(string_ref name, const C_type *base, int n) {
	C_type_struct	ctype(name);
	for (int i = 0; i < n; i++)
		ctype.add(str("xyzw"[i]), base);

	return ctypes.add(ctype);
}

const C_type *make_vector(const C_type *base, int n) {
	string_builder	a;
	if (const char *name = ctypes.get_name(base))
		a << name;
	else
		a << base;

	return make_vector(a << n, base, n);
}

const C_type *app::to_c_type(const dxil::Type* type) {
	using namespace dxil;

	switch (type->type) {
		case Type::Float:
			switch (type->size) {
				case 16: return ctypes.get_type<float16>();
				case 32: return ctypes.get_type<float>();
				case 64: return ctypes.get_type<double>();
			}
			return 0;

		case Type::Int:
			switch (type->size) {
				case 1:		return ctypes.get_type<bool>();
				case 8:		return ctypes.get_type<uint8>();
				case 16:	return ctypes.get_type<uint16>();
				case 32:	return ctypes.get_type<uint32>();
				case 64:	return ctypes.get_type<uint64>();
			}
			return 0;

		case Type::Vector:
			return make_vector(to_c_type(type->subtype), type->size);
		case Type::Pointer:
			return ctypes.add(C_type_pointer(to_c_type(type->subtype)));
		case Type::Array:
			return ctypes.add(C_type_array(to_c_type(type->subtype), type->size));
		case Type::Struct: {
			C_type_struct ctype(type->name);
			for (auto& i : type->members)
				ctype.add("?", to_c_type(i));
			return ctypes.add(ctype);
		}
		default:
			return 0;
	}
}


const C_type *to_c_type(const dxil::Type* type, const dxil::TypeSystem &types, const dxil::meta_tags2 &annotation) {
	using namespace dxil;
	auto	matrix_anno	= annotation.find(meta_typeAnnotations::Matrix);

	if (type->type == Type::Struct && !matrix_anno.exists())
		return struct_to_c_type((string)annotation.find(meta_typeAnnotations::FieldName), type, types);

	uint32	arraySize	= 0;
	uint32	arrayLevel	= 0;

	while (!is_matrix(type) && type->type == Type::Array) {
		arraySize	= arraySize ? arraySize * type->size : type->size;
		type		= type->subtype;
		arrayLevel++;
	}

	if (matrix_anno.exists()) {
		meta_typeAnnotations::matrix	matrix(matrix_anno);
		switch (matrix.Orientation) {
			case matrix.RowMajor:		arraySize /= matrix.Rows; break;
			case matrix.ColumnMajor:	arraySize /= matrix.Cols; break;
		}
		if (type->type == Type::Vector)
			type = type->subtype;
		else if (type->type == Type::Struct)
			type = type->members[0];//HLMatrixType::cast(type).getElementTypeForReg();

		if (arrayLevel == 1)
			arraySize = 0;
	}

	const C_type	*base;

	if (type->type == Type::Struct && !is_matrix(type)) {
		base = struct_to_c_type((string)annotation.find(meta_typeAnnotations::FieldName), type, types);

	} else {
		while (type->type == Type::Array)
			type = type->subtype;

		base = to_c_type(dxil::ComponentType(annotation.find(meta_typeAnnotations::ComponentType)));
		if (matrix_anno.exists()) {
			meta_typeAnnotations::matrix	matrix(matrix_anno);

			string_builder	a;
			switch (matrix.Orientation) {
				case matrix.RowMajor:		a << "row_major "; break;
				case matrix.ColumnMajor:	a << "column_major "; break;
			}
			if (const char *name = ctypes.get_name(base))
				a << name;
			else
				a << base;
			a << matrix.Rows << 'x' << matrix.Cols;
			
			base = make_vector(a, make_vector(base, matrix.Cols), matrix.Rows);
			
			//base = ctypes.add(C_type_array(base, matrix.Rows));
			//base = ctypes.add(C_type_array(base, matrix.Cols));

		} else if (type->type == Type::Vector) {
			base = make_vector(base, type->size);
		}
	}

	if (arraySize)
		base = ctypes.add(C_type_array(base, arraySize));

	return base;
}

const C_type *struct_to_c_type(const char *name, const dxil::Type* type, const dxil::TypeSystem& types) {
	using namespace dxil;

	auto	annotation	= types.annotations.strucs[type];
	if (annotation.exists()) {
		C_type_struct	ctype(name);

		for (int i = 0; i < type->members.size32(); i++) {
			auto	&field_annot	= annotation->fields[i];
			auto	meta_offset		= field_annot.find(meta_typeAnnotations::BufferOffset);	// Constant buffer data layout
			uint32	field_offset	= meta_offset.exists() ? (uint32)meta_offset : type->bit_offset(i) / 8;
			string	field_name		= field_annot.find(meta_typeAnnotations::FieldName);
			ctype.add_atoffset(field_name, to_c_type(type->members[i], types, field_annot), field_offset);
		}
		return ctypes.add(move(ctype));
	}
	return nullptr;
}

const C_type *app::to_c_type(const char *name, const dxil::Type* type, const dxil::TypeSystem& types) {
	using namespace dxil;
	if (type->type == Type::Struct)
		return struct_to_c_type(name, type, types);

	meta_tags2 annotation;
	return ::to_c_type(type, types, annotation);
}

//-----------------------------------------------------------------------------
//	DXILRegisterWindow
//-----------------------------------------------------------------------------

template<> field fields<dxil::ResourceProperties>::f[]	= {
	_MAKE_FIELD(dxil::ResourceProperties, kind)
	0,
};

MAKE_FIELDS(dx::SimulatorDXIL::Handle, props);

void	DXILRegisterWindow::AddOverlay(ListViewControl lv, int row, int col) {
}

uint32	DXILRegisterWindow::AddInOut(const dx::SimulatorDXIL *sim, int thread, const dxil::meta_entryPoint::signature &sig, dx::Operand::Type type, uint32 offset) {
	for (auto& i : sig) {
		auto	p		= sim->GetRegFile<uint4p>(type, i.row);
		int		semantic_index	= i.indices[0];
		for (uint8 x = i.col; x < i.col + i.cols; x++) {
			auto	&e		= entries.emplace_back(offset, string_builder(i.name) << onlyif(semantic_index, semantic_index) << '.' << "xyzw"[x], float_field);
			auto	&val	= *(uint32*)(prev_regs + offset);
			auto	val2	= p[thread][x];
			e.flags			= (val2 == 0xcdcdcdcd ? Entry::UNSET : val2 != val ? Entry::CHANGED : 0);

			val		= val2;
			offset	+= 4;
		}
	}
	return offset;
}

template<typename A, typename B>  int numeric_compare(const A* a, const B* b) {
	if (!a || !b)
		return a && *a ? 1 : b && *b ? -1 : 0;
	bool	last_digit = false;
	for (;;) {
		A	ca = *a++;
		B	cb = *b++;
		if (ca == 0 || cb == 0)
			return ca - cb;

		if (int d = simple_compare(ca, cb)) {
			if (last_digit || (is_digit(ca) && is_digit(cb))) {
				while (is_digit(ca)) {
					if (!is_digit(cb))
						return 1;
					ca = *a++;
					cb = *b++;
				}
				if (is_digit(cb))
					return -1;
			}
			return d;
		}
		last_digit = is_digit(ca);
	}
}

void DXILRegisterWindow::Update(uint32 relpc, uint64 pc, int thread) {
	bool	init	= entries.empty();

	entries.clear();

	auto	n_inout	= sim->sig_in.size();
	if (sim->stage != dx::MS)
		n_inout	+= sim->sig_out.size();

	auto	res = make_dynamic_array(sim->GetResults(thread));

//	auto	n_temp	= sim->GetResults(thread).size();
	auto	n_temp	= res.size();
	uint32	total	= 8 + n_inout * 16 + n_temp * 8;
	prev_regs.resize(total);

	entries.emplace_back(0, "pc", nullptr, Entry::SIZE64);
	((uint64*)prev_regs)[0]	= pc;

	uint32	offset = 8;
	offset	= AddInOut(sim, thread, sim->sig_in, dx::Operand::TYPE_INPUT, offset);
	if (sim->stage != dx::MS)
		offset	= AddInOut(sim, thread, sim->sig_out, dx::Operand::TYPE_OUTPUT, offset);

	uint32	active = sim->ThreadFlags(thread) & sim->THREAD_ACTIVE ? 0 : Entry::DISABLED;

	sort(res, [](auto &a, auto &b) {
		return numeric_compare(get_string<char>(a.a).begin(), get_string<char>(b.a).begin()) < 0;
		//string_builder	aname;
		//string_builder	bname;
		//aname << a.a;
		//bname << b.a;
		//return aname.term() < bname.term();
	});

	for (auto &i : res) {
		const field*	pf		= nullptr;
		const char*		type	= nullptr;
		uint32			flags	= 0;

		switch (i.b.type->type) {
			//case bitcode::Type::Int:	field;
			case bitcode::Type::Float:
				pf = float_field;
				type	= "float";
				break;

			case bitcode::Type::Pointer:
				flags = Entry::SIZE64;
				type	= "ptr";
				break;

			case bitcode::Type::Struct: {
				flags = Entry::SIZE64;
				if (i.b.type->name == "dx.types.Handle") {
					pf = fields<dx::SimulatorDXIL::Handle*>::f;
				} else if (i.b.type->name == "dx.types.CBufRet.f32") {
					pf = fields<float(*)[4]>::f;
				} else if (i.b.type->name == "dx.types.CBufRet.i32" || i.b.type->name == "dx.types.fouri32") {
					pf = fields<int32(*)[4]>::f;
				} else if (i.b.type->name == "dx.types.ResRet.f32") {
					pf = fields<float(*)[4]>::f;
				} else if (i.b.type->name == "dx.types.ResRet.i32") {
					pf = fields<int32(*)[4]>::f;
				} else if (i.b.type->name == "dx.types.Dimensions") {
					pf = fields<int32(*)[4]>::f;
				}
				break;
			}
		}

		string_builder	name;
		name << i.a;

		if (*(uint64*)(prev_regs + offset) != i.b.value)
			flags |= Entry::CHANGED;

		auto	&e = entries.emplace_back(offset, name, pf, flags | active);
		e.type	= type;
		*(uint64*)(prev_regs + offset) = i.b.value;
		offset += 8;
	}

	if (init)
		SetPane(1, SendMessage(WM_ISO_NEWPANE));
	
	for (auto c : Descendants())
		((ListViewControl)c).SetCount(entries.size32());

	RedrawWindow(*this, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

//-----------------------------------------------------------------------------
//	DXILLocalsWindow
//-----------------------------------------------------------------------------

void DXILLocalsWindow::Update(uint32 relpc, uint64 pc, int thread) {
	DXIL_type_converter	conv(unconst(types));

	frame.clear();

	// find existing locals
	hash_map<const void*, const bitcode::DILocalVariable*>	in_scope;

	auto		tree			= tc.GetTreeControl();
	int32		needed_zeroes	= 0;
	uint32		needed_consts	= 0;

	for (auto& i : sim->locals) {
		const bitcode::DILocalVariable*	var = i.a;
		bool	has_scoped	= false;

		for (auto &x : i.b.values) {
			if (sim->IsInScope(x.loc) && sim->Exists(x.val)) {
				has_scoped	= true;
				auto	t	= sim->GetTyped(x.val);
				if (t.count == 0) {
					uint32	alignment;
					uint32	bit_size	= t.type->bit_size(alignment);
					uint64	offset		= x.exp->Evaluate(0, bit_size);
					if (t.v)
						needed_consts	+= bit_size / 8;
					else
						needed_zeroes	= max(needed_zeroes, bit_size / 8);
				}
			}
		}
		
		for (auto &x : i.b.declares)
			has_scoped = has_scoped || sim->IsInScope(x.loc);

		if (has_scoped/* || !i.b.declares.empty()*/) {
			auto	type		= conv.process(var->type);
			uint64	loc_size	= type->size();
			uint64	loc_base	= frame.add_chunk(loc_size);

			HTREEITEM	h	= 0;
			for (auto hi : tree.ChildItems()) {
				auto	lit = GetEntry(hi);
				if (lit->id == (const string&)var->name && lit->type == type) {
					h = hi;
					break;
				}
			}

			if (h)
				GetEntry(h)->v = loc_base;
			else
				h = AppendEntry(*var->name, type, loc_base);

			in_scope[h] = var;
		}

	}

	for (auto i = tree.ChildItems().begin(), e = tree.ChildItems().end(); i != e;) {
		auto	h = *i++;
		if (!in_scope[h].exists())
			tree.DeleteItem(h);
	}

	if (needed_zeroes > zeroes.size())
		zeroes.set(needed_zeroes * 2);

	if (needed_consts > consts.size())
		consts.set(needed_consts * 2);

	uint8	*constp	= consts.begin();

	for (auto h : tree.ChildItems()) {
		const bitcode::DILocalVariable*	var = in_scope[h];
		auto	lit			= GetEntry(h);
		uint64	loc_size	= lit->type->size();
		uint64	loc_base	= lit->v;

		auto	&loc		= get(sim->locals[var]);

		for (auto &x : loc.declares) {
			if (sim->IsInScope(x.loc) && sim->Exists(x.val)) {
				auto	t			= sim->GetTyped(x.val);
				uint32	bit_size	= loc_size * 8;
				uint64	offset		= x.exp->Evaluate(loc_base, bit_size);
				ISO_ASSERT(offset + bit_size / 8 <= loc_base + loc_size && offset >= loc_base);
				uint32	size		= bit_size / 8;
				auto	src			= t.count ? *(uint8**)t[thread].p : (uint8*)t.v;

				if (x.layout.array_dims.empty()) {
					frame.add_block(offset, src, size);

				} else {
					//src	+= x.layout.start_offset_bits / 8;
					uint32	offset2			= offset;// + x.layout.start_offset_bits / 8;
					uint32	stride			= x.layout.array_dims[0].stride_bits / 8;
					uint32	num				= x.layout.array_dims[0].num_elements;
					while (num--) {
						frame.add_block(offset2, src, size);
						offset2 += stride;
						src		+= size;
					}
				}
			}
		}

		for (auto &x : loc.values) {
			if (sim->IsInScope(x.loc) && sim->Exists(x.val)) {
				auto	t			= sim->GetTyped(x.val);
				uint32	bit_size	= loc_size * 8;
				uint64	offset		= x.exp->Evaluate(loc_base, bit_size);
				ISO_ASSERT(offset + bit_size / 8 <= loc_base + loc_size && offset >= loc_base);
				uint32	size		= bit_size / 8;

				if (t.count) {
					frame.add_block(offset, t[thread].p, size);
				} else if (t.v) {
					memcpy(constp, (uint8*)&t.v, size);
					frame.add_block(offset, constp, size);
					constp += size;
				} else {
					frame.add_block(offset, zeroes, size);
				}
			}
		}
	}

	tc.Invalidate();
}

//-----------------------------------------------------------------------------
//	DXILTraceWindow
//-----------------------------------------------------------------------------

struct DXILTraceWindow::RegAdder : TraceWindow::RegAdder {
	using TraceWindow::RegAdder::RegAdder;

	void	SetAddress(uint64 addr) {
		format("%i", (uint32)addr);
		term();
		Insert(c);
	}
	void	AddValue(int i, const bitcode::Typed &v) {
		if (!v.type)
			return;

		switch (v.type->type) {
			case bitcode::Type::Float:
				reset() << force_cast<float>(v.value);
				break;
			case bitcode::Type::Int:
				reset() << (int)v.value;
				break;
			default:
				return;
		}
		SetColumn(i + 2);
	}

};

DXILTraceWindow::DXILTraceWindow(const WindowPos &wpos, dx::SimulatorDXIL *sim, int thread, int max_steps) : TraceWindow(wpos, 1) {
	static Disassembler	*dis = Disassembler::Find("DXIL");
	auto	state = dis->Disassemble(sim->GetUCode(), -1);

	int	nc = 2;
	ListViewControl::Column("dest").Width(120).Insert(c, nc++);
	ListViewControl::Column("input 0").Width(120).Insert(c, nc++);
	ListViewControl::Column("input 1").Width(120).Insert(c, nc++);
	ListViewControl::Column("input 2").Width(120).Insert(c, nc++);

	auto *p = sim->Run(0);

	while (p && max_steps--) {
		RegAdder	ra(c, rows.push_back());
		auto		addr	= sim->Offset(p);

		ra.SetAddress(addr);
		auto	line	= state->AddressToLine(addr);
		state->GetLine(ra.reset(), line);
		ra.SetColumn(1);

		auto *op = (const bitcode::Instruction*)p;

		ra.source = true;
		int		x = 1;
		for (auto &i : op->args)
			ra.AddValue(x++, sim->GetTyped(i, thread));

		p	= sim->Continue(p, 1);
		ra.source = false;

		auto	result = sim->GetResult(op, thread);
		if (result.type)
			ra.AddValue(0, result);
	}

	selected	= 0x100;
}