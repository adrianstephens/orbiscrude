#include "bitcode.h"
#include "base/soft_float.h"
#include "filename.h"

//-----------------------------------------------------------------------------
//	LLVM bitcode
//-----------------------------------------------------------------------------

using namespace bitcode;

//-----------------------------------------------------------------------------
// Abbrev
//-----------------------------------------------------------------------------

bool Abbrev::read_record(vlc &file, dynamic_array<uint64> &v, malloc_block *blob) const {
	for (auto &op : slice(1)) {
		switch (op.encoding) {
			case AbbrevOp::Array_Fixed:
			case AbbrevOp::Array_VBR:
			case AbbrevOp::Array_Char6: {
				uint32 n = file.get_vbr<uint32>(6);
				if (blob) {
					uint8	*d = blob->resize(n);
					while (n--)
						*d++ = op.get_value(file);

				} else {
					while (n--)
						v.push_back(op.get_value(file));
				}
				break;
			}

			case AbbrevOp::Blob: {
				uint32 n = file.get_vbr<uint32>(6);
				file.SkipToFourByteBoundary();
				if (blob) {
					blob->read(file.get_stream(), n);

				} else {
					while (n--)
						v.push_back(file.get_stream().getc());
				}
				file.SkipToFourByteBoundary();
				break;
			}
			default:
				v.push_back(op.get_value(file));
				break;
		}
	}
	return true;
}

bool Abbrev::write_record(wvlc& file, range<const uint64*> vals, const malloc_block* blob) const {
	const uint64	*v	= vals.begin();
	for (auto &op : slice(1)) {

		switch (op.encoding) {
			case AbbrevOp::Array_Fixed:
			case AbbrevOp::Array_VBR:
			case AbbrevOp::Array_Char6:
				file.put_vbr(blob ? blob->size() : vals.end() - v, 6);
				if (blob) {
					for (auto i : make_range<uint8>(*blob))
						op.put_value(file, i);

				} else {
					while (v < vals.end())
						op.put_value(file, *v++);
				}
				return true;

			case AbbrevOp::Blob: {
				file.put_vbr(blob ? blob->size() : vals.end() - v, 6);
				file.SkipToFourByteBoundary();
				auto&	file2	= file.get_stream();
				if (blob) {
					file2.write(*blob);

				} else {
					while (v < vals.end())
						file2.putc(*v++);
				}
				file.SkipToFourByteBoundary();
				return true;
			}

			default:
				ISO_ASSERT(v < vals.end());
				op.put_value(file, *v++);
				break;
		}
	}
	return true;
}

bool Abbrev::skip(vlc &file) const {
	for (auto &op : slice(1))
		op.skip(file);
	return true;
}

//-----------------------------------------------------------------------------
// BlockReader
//-----------------------------------------------------------------------------

BlockReader::BlockReader(vlc &file) : file(file), code_size(2) {
	end = file.get_stream().length();
}

BlockReader::BlockReader(vlc &file, const Abbrevs *info) : Abbrevs(info), file(file), code_size(file.get_vbr<uint32>(4)) {
	file.SkipToFourByteBoundary();

	uint32	num_words = file.get_stream().get<uint32>();
	end	= file.get_stream().tell() + num_words * 4;
}

BlockReader::~BlockReader() {
	file.reset();
	file.get_stream().seek(end);
}

AbbrevID BlockReader::nextRecord(uint32 &code, bool process_abbrev) {
	while (file.tell_bit() < end * 8) {
		auto id = (AbbrevID)file.get(code_size);
		switch (id) {
			case END_BLOCK:
				file.SkipToFourByteBoundary();
				return id;

			case ENTER_SUBBLOCK:
				code = file.get_vbr<uint32>(8);
				return id;

			case DEFINE_ABBREV:
				if (process_abbrev) {
					abbrevs.push_back(new Abbrev(file));
					continue;
				}
				return id;

			case UNABBREV_RECORD:
				code = file.get_vbr<uint32>(6);
				return id;

			default:
				code = get_abbrev(id)->get_code(file);
				return id;
		}
	}
	return END_BLOCK;
}

AbbrevID BlockReader::nextRecordNoSub(uint32 &code) {
	AbbrevID	id;
	while ((id = nextRecord(code)) == ENTER_SUBBLOCK)
		BlockReader(file, nullptr);
	return id;
}

bool BlockReader::skipRecord(AbbrevID id) const {
	switch (id) {
		case ENTER_SUBBLOCK:
			BlockReader(file, nullptr);
			return true;

		case UNABBREV_RECORD:
			for (uint32 n = file.get_vbr<uint32>(6); n--;)
				(void)file.get_vbr<uint64>(6);
			return true;

		default:
			return get_abbrev(id)->skip(file);
	}
}

bool BlockReader::readRecord(AbbrevID id, dynamic_array<uint64> &v, malloc_block *blob) const {
	v.clear();
	if (id == UNABBREV_RECORD) {
		for (uint32 n = file.get_vbr<uint32>(6); n--;)
			v.push_back(file.get_vbr<uint64>(6));
		return true;

	} else {
		return get_abbrev(id)->read_record(file, v, blob);
	}
}

//-----------------------------------------------------------------------------
// BlockWriter
//-----------------------------------------------------------------------------

BlockWriter::BlockWriter(wvlc &file, uint32 code_size, const Abbrevs *info) : Abbrevs(info), file(file), code_size(code_size) {
	auto&	file2	= file.get_stream();
	start			= file2.tell();
	file2.write(uint32(0));	//put a dummy uint32 for length
}


BlockWriter::~BlockWriter() {
	if (start) {
		file.put(END_BLOCK, code_size);
		file.SkipToFourByteBoundary();
		auto&	file2	= file.get_stream();
		auto	here	= file2.tell();
		file2.seek(start);
		file2.write(uint32((here - start) / 4 - 1));
		file2.seek(here);
	}
}

BlockWriter	BlockWriter::enterBlock(BlockID id, uint32 _code_size, const Abbrevs *info) {
	file.put(ENTER_SUBBLOCK, code_size);
	file.put_vbr((uint32)id, 8);
	file.put_vbr(_code_size, 4);
	file.SkipToFourByteBoundary();
	return {file, _code_size, info};
}

enum STRING_ABBREV {
	SA_NONE		= 0,
	SA_CHAR7	= 1,
	SA_CHAR6	= 2,
};

STRING_ABBREV add_string(dynamic_array<uint64> &v, const string &s) {
	STRING_ABBREV	abbr = SA_CHAR6;
	for (auto &i : s) {
		v.push_back(i);
		if (i & 128)
			abbr = SA_NONE;
		if (abbr == SA_CHAR6 && !AbbrevOp::isChar6(i))
			abbr = SA_CHAR7;
	}
	return abbr;
}

void BlockWriter::write_string(uint32 code, const string &s, AbbrevID char6_abbrev) const {
	dynamic_array<uint64> v;
	if (add_string(v, s) != SA_CHAR6)
		char6_abbrev = AbbrevID(0);
	write_record(code, v, char6_abbrev);
}

//-----------------------------------------------------------------------------
// BlockInfoBlock
//-----------------------------------------------------------------------------

bool BlockInfoBlock::read(BlockReader &&reader, bool ignore_names) {
	if (!reader.verify())
		return false;

	dynamic_array<uint64>	v;
	uint32					code;
	BlockInfo				*block = nullptr;

	while (auto id = reader.nextRecord(code, false)) {
		if (id == DEFINE_ABBREV) {
			if (!block)
				return false;

			block->abbrevs.push_back(new Abbrev(reader));
			continue;
		}
		switch (code) {
			default:
				reader.skipRecord(id);
				break;

			case BLOCKINFO_CODE_SETBID:
				if (!reader.readRecord(id, v) || v.size() < 1)
					return false;

				block	= (*this)[(uint32)v[0]];
				break;

			case BLOCKINFO_CODE_BLOCKNAME:
				if (!block || !reader.readRecord(id, v))
					return false;

				if (!ignore_names) {
					string name;
					for (auto &i : v)
						name += (char)i;
					block->name = name;
				}
				break;

			case BLOCKINFO_CODE_SETRECORDNAME:
				if (!block || !reader.readRecord(id, v))
					return false;

				if (!ignore_names) {
					string name;
					for (auto &i : v.slice(1))
						name += (char)i;
					block->record_names.push_back(make_pair((uint32)v[0], name));
				}
				break;
		}
	}
	return true;
}

bool BlockInfoBlock::write(BlockWriter &&writer, bool ignore_names) {
	for (auto &i : info) {
		writer.write_record(BLOCKINFO_CODE_SETBID, {i.id});
		for (auto &j : i.abbrevs) {
			writer.put_code(DEFINE_ABBREV);
			j->write(writer);
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
//	Type
//-----------------------------------------------------------------------------

bool Type::operator==(const Type& b) const {
	if (type != b.type)
		return false;

	switch (type) {
		case Float:
		case Int:
			return size == b.size;

		case Pointer:
			return subtype == b.subtype && addrSpace == b.addrSpace;

		case Function:
			if (subtype != b.subtype)
				return false;
		case Struct:
			return members == b.members;

		case Vector:
		case Array:
			return subtype == b.subtype && size == b.size;

		default:
			return true;
	}
}

uint32 Type::bit_size(uint32& alignment) const {
	switch (type) {
		case Float:
		case Int:
			alignment	= (size + 7) / 8;
			return size;

		case Pointer:
			alignment	= 8;
			return 64;

		case Struct: {
			uint32	total_size = 0, total_align = 1;
			for (auto& i : members) {
				uint32	sub_size = i->bit_size(alignment);
				if (!packed) {
					total_size	= align(total_size, alignment);
					total_align	= max(total_align, alignment);
				}
				total_size += sub_size;
			}
			alignment = total_align;
			return total_size;
		}

		case Vector:
		case Array:
			return subtype->bit_size(alignment) * size;

		case Function:
		case Void:
		case Metadata:
		case Label:
			return 0;
	}
}

uint32 Type::bit_offset(uint32 i) const {
	switch (type) {
		case Vector:
		case Array: {
			uint32	alignment, sub_size	= subtype->bit_size(alignment);
			return align(sub_size, alignment) * i;
		}

		case Struct: {
			uint32	alignment, total_offset = 0;
			for (auto& e : members) {
				if (!i--)
					break;
				uint32	sub_size = e->bit_size(alignment);
				if (!packed)
					total_offset	= align(total_offset, alignment);
				total_offset += sub_size;
			}
			return total_offset;
		}
		default:
			return 0;
	}
}

uint32 Type::bit_offset(uint32 i, const Type *&sub) const {
	sub	= nullptr;
	switch (type) {
		case Vector:
		case Array: {
			sub	= subtype;
			uint32	alignment, sub_size	= subtype->bit_size(alignment);
			return align(sub_size, alignment) * i;
		}

		case Struct: {
			uint32	alignment, total_offset = 0;
			for (auto& e : members) {
				if (!i--) {
					sub	= e;
					break;
				}
				uint32	sub_size = e->bit_size(alignment);
				if (!packed)
					total_offset	= align(total_offset, alignment);
				total_offset += sub_size;
			}
			return total_offset;
		}
		default:
			return 0;
	}
}

template<> float Type::get<float>(const void *p) const {
	ISO_ASSERT(type == Float);
	switch (size) {
		case 16:	return *(const float16*)p;
		case 32:	return *(const float*)p;
		case 64:	return *(const double*)p;
		case 80:	return (double)*(const float80*)p;
		case 128:	return (double)*(const float128*)p;
		default:
			ISO_ASSERT(0);
			return 0;
	}
}

void Type::set(void *p, float t) const {
	ISO_ASSERT(type == Float);
	switch (size) {
		case 16:	*(float16*)p	= t; break;
		case 32:	*(float*)p		= t; break;
		case 64:	*(double*)p		= t; break;
		case 80:	*(float80*)p	= t; break;
		case 128:	*(float128*)p	= t; break;
		default:
			ISO_ASSERT(0);
	}
}

void Type::set(void *p, const TypedPointer &t) const {
	ISO_ASSERT(type == Pointer);
//	ISO_ASSERT(t.type->type == Type::Array ? *subtype == *t.type->subtype : *subtype == *t.type);
	ISO_ASSERT(*subtype == *t.type);
	*(void**)p = t.p;
}

void Type::set(void *p, const Typed &t) const {
	ISO_ASSERT(*this == *t.type);
	memcpy(p, &t.value, byte_size());
}

Typed::Typed(const Value &v) : Typed() {
	switch (v.kind()) {
		default:
			ISO_ASSERT(0);

		case Value::best_index<const Metadata*>: {
			auto	m	= v.get_known<const Metadata*>();
			type	= m->type;
			value	= m->get64();
			break;
		}

		case Value::best_index<uint64>:
			type = Type::get<int>();
			value = v.get<uint64>();
			break;

		case Value::best_index<const GlobalVar*>:
		case Value::best_index<const Constant*>: {
			auto	c = (v.is<const GlobalVar*>() ? v.get_known<const GlobalVar*>()->value : v).get_known<const Constant*>();
			type	= c->type;

			switch (c->value.kind()) {
				case Value::best_index<_none>:
				case Value::best_index<_zero>:
					break;

				case Value::best_index<uint64>:
					value	= c->value.get_known<uint64>();
					break;

				case Value::best_index<const Eval*>:
					value	= c->value.get_known<const Eval*>()->Evaluate(type);
					break;

				case Value::best_index<ValueArray>:
					value	= (uint64)c->value.get_known<ValueArray>().begin();
					break;

				default:
					ISO_ASSERT(0);
			}
		}
	}
}

void bitcode::LayoutValue(const Type *type, const Value &v, void* p, bool clear_undef) {
	switch (v.kind()) {
		case Value::best_index<uint64>: {
			uint64	u = v.get_known<uint64>();
			memcpy(p, &u, type->byte_size());
			break;
		}

		case Value::best_index<const Constant*>: {
			auto	c = v.get_known<const Constant*>();
			LayoutValue(type, c->value, p, clear_undef);
			break;
		}

		case Value::best_index<_none>:
			if (!clear_undef)
				break;
		case Value::best_index<_zero>: {
			memset(p, 0, type->byte_size());
			break;
		}

		case Value::best_index<ValueArray>: {
			auto&	vals = v.get_known<ValueArray>();

			switch (type->type) {
				case Type::Struct: {
					auto	pv		= vals.begin();
					uint32	offset	= 0;
					for (auto& i : type->members) {
						uint32	alignment, size = i->byte_size(alignment);
						if (!type->packed)
							offset	= align(offset, alignment);

						LayoutValue(i, *pv++, (char*)p + offset, clear_undef);
						offset += size;
					}
					break;
				}

				case Type::Vector:
				case Type::Array: {
					uint32	size = type->subtype->byte_size();
					for (int i = 0; i < type->size; i++)
						LayoutValue(type->subtype, vals[i], (char*)p + size * i, clear_undef);
					break;
				}

			}
			break;
		}
	}
}

// walk the type list to get the return type
const Type *WalkGEP(const Type *type, range<const Value*> indices) {
	for (auto &i : indices) {
		switch (type->type) {
			case Type::Vector:
			case Type::Array:
				type = type->subtype;
				break;
			case Type::Struct:
				type = type->members[Typed(i)];		// if it's a struct the index must be constant
				break;
			default:
				ISO_ASSERTF(0, "Unexpected type %d encountered in GEP", type->type);
		}
	}
	return type;
}

//-----------------------------------------------------------------------------
//	Live
//-----------------------------------------------------------------------------

dynamic_array<const Block*> Function::Successors(const Block *b) const {
	auto	last = Instructions(b).back();
	switch (last->op) {
		case Operation::Switch:
			return transformc(make_split_range<2>(last->args), [](range<const Value*> p){ return p[1].get_known<const Block*>(); });

		case Operation::Br:
			if (last->args.size() > 1)
				return {last->args[0].get_known<const Block*>(), last->args[1].get_known<const Block*>()};

			return {last->args[0].get_known<const Block*>()};

		default:
			return none;
	}
}


void Function::GetLive() const {
	struct BlockData {
		hash_set<const Instruction*>			insts;
		hash_set_with_key<const Instruction*>	in, out;
		bool	visited	= false;
	};
	dynamic_array<BlockData>	data(blocks.size());

	for (auto& b : blocks) {
		auto	&d		= data[blocks.index_of(b)];
		for (auto i : Instructions(&b))
			d.insts.insert(i.i);
	}

	for (auto& b : blocks) {
		auto	&d	= data[blocks.index_of(b)];
		for (auto i : Instructions(&b)) {
			if (i->op != Operation::Phi) {
				for (auto &a : i->args) {
					if (a.is<const Instruction*>()) {
						d.in.insert(a.get_known<const Instruction*>());

					} else if (a.is<const Metadata*>()) {
						auto	*m = a.get_known<const Metadata*>();
						if (m->value.is<const Instruction*>())
							d.in.insert(m->value.get_known<const Instruction*>());
					}

				}
			} else {
				for (auto* p = i->args.begin(); p != i->args.end(); p += 2) {
					auto	&a = p[0];
					if (a.is<const Instruction*>()) {
						auto	i2	= a.get_known<const Instruction*>();
						auto	b2	= p[1].get_known<const Block*>();
						auto	&d2	= data[blocks.index_of(b2)];
						if (!d2.insts.count(i2))
							d2.in.insert(i2);
					}
				}

			}
		}
		d.in -= d.insts;
	}

	dynamic_array_de1<const Block*>	queue;

	for (auto &b : blocks) {
		if (!Successors(&b)) {
			queue.push_back(&b);
			//make any alloca's in the first block be always live
			auto		&d	= data[blocks.index_of(b)];
			for (auto i : Instructions(blocks.begin())) {
				if (i->op == Operation::Alloca) {
					d.in.insert(i);
				}
			}

		}
	}

	while (!queue.empty()) {
		const Block* b	= queue.pop_front_value();
		auto		&d	= data[blocks.index_of(b)];
		d.visited		= true;

		d.out.clear();
		for (auto &i : Successors(b))
			d.out += data[blocks.index_of(i)].in;

		auto	new_in	= (d.in | d.out) - d.insts;
		bool	changed	= new_in != d.in;

		if (changed)
			d.in = new_in;

		for (auto &i : b->preds) {
			if (changed || !data[blocks.index_of(i)].visited)
				queue.push_back(i);
		}
	}

	for (auto& b : blocks) {
		auto	&d		= data[blocks.index_of(b)];
//		for (auto &i : d.out)
		for (auto &i : d.in)
			unconst(b.live).push_back(i);
	}
}

//-----------------------------------------------------------------------------
//	Value
//-----------------------------------------------------------------------------

const Type *Value::GetType(bool warn) const {
	switch (kind()) {
		case best_index<const Constant*>:	return get_known<const Constant*>()->type;
		case best_index<const Instruction*>:return get_known<const Instruction*>()->type;
		case best_index<const GlobalVar*>:	return get_known<const GlobalVar*>()->type;
		case best_index<const Function*>:	return get_known<const Function*>()->type;
		case best_index<const Metadata*>:	return get_known<const Metadata*>()->type;
		default:
			if (warn)
				ISO_OUTPUTF("Unexpected symbol to get type for %d", kind());
			return nullptr;
	}
}

const string* Value::GetName(bool warn) const {
	switch (kind()) {
		case best_index<const Instruction*>:return &get_known<const Instruction*>()->name;
		case best_index<const GlobalVar*>:	return &get_known<const GlobalVar*>()->name;
		case best_index<const Function*>:	return &get_known<const Function*>()->name;
		case best_index<const Alias*>:		return &get_known<const Instruction*>()->name;
		case best_index<const Block*>:		return &get_known<const Instruction*>()->name;
		default:
			if (warn)
				ISO_OUTPUTF("Unexpected value symtab entry referring to %d", kind());
			return nullptr;
	}
}

//-----------------------------------------------------------------------------
// Collector
//-----------------------------------------------------------------------------

template<typename T> struct unique_table : dynamic_array<T> {
	using dynamic_array<T>::operator=;

	T		get(uint64 i)			const { return (*this)[i]; }
	T		getOrNull(uint64 i)		const { return i == 0 ? T() : get(i - 1); }
	uint32	getID(const T &s)		const { return index_of(find(*this, s)); }
	uint32	getIDorNull(const T &s)	const { return s ? index_of(find(*this, s)) + 1 : 0; }

	bool	add_unique(const T &s) {
		if (s && find(*this, s) == end()) {
			push_back(s);
			return true;
		}
		return false;
	}
};

using string_table = unique_table<const char*>;

struct Collector {
	dynamic_array<Value>			values;
	dynamic_array<const Metadata*>	metadata;
	Types							types;
	unique_table<AttributeGroup*>	attr_groups;
	unique_table<AttributeSet>		attr_sets;
	string_table					meta_kinds;
	string_table					sections;
	string_table					gcs;
	unique_table<const Comdat*>		comdats;
};

//-----------------------------------------------------------------------------
// Reading
//-----------------------------------------------------------------------------

string	get_string(range<const uint64*> r) {
	string	s(r.size());
	auto	p = s.begin();
	for (auto &i : r) {
		if (!i)
			break;
		*p++ = (char)i;
	}
	s.resize(p - s.begin());
	return s;
}

namespace bitcode {
	uint64 hash(const DILocation &loc) {
		hash_stream<FNV<64>>	fnv;
		loc.write(fnv);
		return fnv;
	}
}

inline Value MaybeForwardRef(size_t index) {
	if (index)
		return ForwardRef(index - 1);
	return none;
}

struct ModuleReader : Collector {
	typedef	decltype(Module::allocator)	allocator_t;
	allocator_t&	allocator;
	hash_map<DILocation, const DILocation*>	locs;

	ModuleReader(allocator_t& allocator) : allocator(allocator) {}

	bool Resolve(Value &v) const {
		if (!v.is<ForwardRef>())
			return false;
		v = values[v.get<ForwardRef>()->index];
		return true;
	}
	void Restore(uint32 n, uint32 m) {
		values.resize(n);
		metadata.resize(m);
	}

	const DILocation* get_loc(const DILocation& loc) {
		auto	r = locs[loc];
		if (!r.exists())
			r = new(allocator) DILocation(loc);
		return r;
	}

	auto GetMeta(uint64 i) {
		auto	&d = metadata.set(i);
		return d ? d : (d = new(allocator) Metadata(nullptr, none));
	}
	auto GetMetaOrNull(uint64 i) {
		return i == 0 ? nullptr : GetMeta(i - 1);
	}
	void SetMeta(uint64 i, const Type *type, Value value, bool distinct = false) {
		auto	&d = metadata.set(i);
		if (!d)
			d = new(allocator) Metadata(type, value, distinct);
		else
			*unconst(d) = Metadata(type, value, distinct);
	}

	void read_attrgroups(BlockReader&& reader);
	void read_attrsets(BlockReader&& reader);
	bool read_types(BlockReader&& reader);
	bool read_consts(BlockReader&& reader);
	bool read_metadata(BlockReader&& reader, dynamic_array<NamedMetadata> &named_meta);
	bool read_function(BlockReader&& reader, Function &f, const BlockInfoBlock &infos);
};

class RecordReader {
protected:
	range<const uint64*>	v;
	uint64		get()		{ return v.empty() ? 0 : v.pop_front_value(); }
public:
	ModuleReader	&r;

	RecordReader(ModuleReader &r, range<const uint64*> v) : v(v), r(r) {}
	size_t					remaining()	const	{ return v.size(); }
	range<const uint64*>	remainder()	const	{ return v; }

	size_t	readbuff(void*, size_t) { ISO_ASSERT(0); return 0; }

	template<typename...T>	bool	read(T&&...x)	{ return iso::read(*this, x...); }
	template<typename T>	T		get()			{ T t; ISO_VERIFY(read(t)); return t; }

	template<typename T> enable_if_t<(is_builtin_int<T> && !is_signed<T>) || is_char<T> || same_v<T, bool> || is_enum<T>, bool> custom_read(T &x) {
		x = T(get()); return true;
	}
	template<typename T> enable_if_t<is_builtin_int<T> && is_signed<T> && !is_char<T>, bool> custom_read(T &x) {
		x = decode_svbr(get()); return true;
	}
	bool	custom_read(MetadataRef& x) {
		x = r.GetMetaOrNull(get());
		return true;
	}
	bool	custom_read(MetadataRef1& x) {
		x = r.GetMeta(get());
		return true;
	}
	bool	custom_read(MetaString &x) {
		x = r.GetMetaOrNull(get());
		ISO_ASSERT(!x.m || x.m->value.is<string>());
		return true;
	}
	string	get_string() {
		string	s(remaining());
		auto	p = s.begin();
		while (auto i = get())
			*p++ = (char)i;
		s.resize(p - s.begin());
		return s;
	}
	const Type*	get_type() {
		return r.types[get<size_t>()];
	}
};

void ModuleReader::read_attrgroups(BlockReader&& reader) {
	uint32					code;
	dynamic_array<uint64>	v;
	while (auto id = reader.nextRecordNoSub(code)) {
		if (code == PARAMATTR_GRP_CODE_ENTRY) {
			reader.readRecord(id, v);
			auto	g	= attr_groups.set(v[0]) = new(allocator) AttributeGroup;
			RecordReader	r(*this, v.slice(1));

			r.read(g->slot);
			while (r.remaining()) {
				switch (r.get<uint64>()) {
					case 0:
						g->set(r.get<AttributeIndex>());
						break;
					case 1: {
						auto	i = r.get<AttributeIndex>();
						g->set(i, r.get<uint64>());
						break;
					}
					case 3: {
						auto	a = r.get_string();
						g->strs.emplace_back(move(a), none);
						break;
					}
					case 4: {
						auto	a = r.get_string();
						auto	b = r.get_string();
						g->strs.emplace_back(move(a), move(b));
						break;
					}
					default:
						ISO_ASSERT(0);
				}
			}

		} else {
			reader.skipRecord(id);
		}
	}
}

void ModuleReader::read_attrsets(BlockReader&& reader) {
	uint32					code;
	dynamic_array<uint64>	v;
	while (auto id = reader.nextRecordNoSub(code)) {
		if (code == PARAMATTR_CODE_ENTRY) {
			reader.readRecord(id, v);
			auto	&attr_set = attr_sets.set(v[0] - 1).emplace();
			for (uint64 g : v)
				attr_set->add(attr_groups[g]);

		} else {
			reader.skipRecord(id);
		}
	}
}

bool ModuleReader::read_types(BlockReader&& reader) {
	ptr_max_array<Type>	types0;
	uint32	code;
	dynamic_array<uint64>	v;
	string	struct_name;

	while (auto id = reader.nextRecordNoSub(code)) {
		reader.readRecord(id, v);
		switch (code) {
			case TYPE_CODE_NUMENTRY:	types0 = _ptr_max_array<Type>(allocator.alloc<Type>(v[0]), 0, v[0]); break;

			case TYPE_CODE_VOID:		types0.emplace_back(Type::Void); break;

			case TYPE_CODE_HALF:		types0.emplace_back(Type::Float, 16); break;
			case TYPE_CODE_FLOAT:		types0.emplace_back(Type::Float, 32); break;
			case TYPE_CODE_DOUBLE:		types0.emplace_back(Type::Float, 64); break;
			case TYPE_CODE_X86_FP80:	types0.emplace_back(Type::Float, 80); break;
			case TYPE_CODE_FP128:		types0.emplace_back(Type::Float, 128); break;
			case TYPE_CODE_PPC_FP128:	types0.emplace_back(Type::Float, 128+1); break;
			case TYPE_CODE_X86_MMX:		types0.emplace_back(Type::Float, 64+1); break;

			case TYPE_CODE_INTEGER:		types0.emplace_back(Type::Int,		v[0]); break;
			case TYPE_CODE_POINTER:		types0.emplace_back(Type::Pointer,	v[1], &types0[v[0]]); break;
			case TYPE_CODE_ARRAY:		types0.emplace_back(Type::Array,	v[0], &types0[v[1]]); break;
			case TYPE_CODE_VECTOR:		types0.emplace_back(Type::Vector,	v[0], &types0[v[1]]); break;

			case TYPE_CODE_LABEL:		types0.emplace_back(Type::Label); break;
			case TYPE_CODE_METADATA:	types0.emplace_back(Type::Metadata); break;

			case TYPE_CODE_STRUCT_NAME:	struct_name = get_string(v); break;

			case TYPE_CODE_OPAQUE: {
				auto&	str = types0.emplace_back(Type::Struct);
				str.name	= move(struct_name);
				str.opaque	= true;
				struct_name.clear();
				break;
			}

			case TYPE_CODE_STRUCT_ANON:
			case TYPE_CODE_STRUCT_NAMED: {
				auto&	str = types0.emplace_back(Type::Struct, v[0] != 0);
				if (code == TYPE_CODE_STRUCT_NAMED) {
					str.name = move(struct_name);
					struct_name.clear();
				}
				for (auto &i : v.slice(1))
					str.members.push_back(&types0[i]);
				break;
			}

			case TYPE_CODE_FUNCTION_OLD: {
				auto&	fn = types0.emplace_back(Type::Function, v[0] != 0, &types0[v[2]]);
				for (auto &i : v.slice(3))
					fn.members.push_back(&types0[i]);
				break;
			}
			case TYPE_CODE_FUNCTION: {
				auto&	fn = types0.emplace_back(Type::Function, v[0] != 0, &types0[v[1]]);
				for (auto &i : v.slice(2))
					fn.members.push_back(&types0[i]);
				break;
			}
			default:
				break;
		}
	}

	types = with_iterator(types0.detach());
	return true;
}

int64 decode_svbr(uint64 v, const Type *type) {
	if (v == 1)
		return bit64(type->size - 1);
	return decode_svbr(v);
}

bool ModuleReader::read_consts(BlockReader&& reader) {
	uint32	code;
	dynamic_array<uint64>	v;
	const Type *cur_type	= nullptr;
	auto	start			= values.size();

	auto	make_eval = [&](Operation op, uint32 flags, const Value &arg) {
		values.push_back(new(allocator) Constant(cur_type, new(allocator) Eval(op, flags, arg)));
	};

	while (auto id = reader.nextRecordNoSub(code)) {
		reader.readRecord(id, v);

		switch (code) {
			case CST_CODE_SETTYPE:
				cur_type = types[v[0]];
				break;

			case CST_CODE_NULL:
				values.push_back(new(allocator) Constant(cur_type, zero));
				break;

			case CST_CODE_UNDEF:
				values.push_back(new(allocator) Constant(cur_type, none));
				break;

			case CST_CODE_INTEGER:
				values.push_back(new(allocator) Constant(cur_type, decode_svbr(v[0], cur_type)));
				break;

			case CST_CODE_FLOAT:
				values.push_back(new(allocator) Constant(cur_type, v[0]));
				break;

			case CST_CODE_STRING:
			case CST_CODE_CSTRING:
				values.push_back(new(allocator) Constant(cur_type, get_string(v)));
				break;

			case CST_CODE_CE_BINOP:
				make_eval(BinaryOperation((BinaryOpcodes)v[0], cur_type->is_float()), 0, ValueArray{values[v[1]], values[v[2]]});
				break;

			case CST_CODE_CE_SELECT:
				make_eval(Operation::Select, 0, ValueArray{values[v[0]], values[v[1]], values[v[2]]});
				break;

			case CST_CODE_CE_EXTRACTELT:
				make_eval(Operation::ExtractElement, 0, ValueArray{values[v[1]], values[v[2]]});
				break;

			case CST_CODE_CE_INSERTELT:
				make_eval(Operation::InsertElement, 0, ValueArray{values[v[0]], values[v[1]], values[v[2]]});
				break;

			case CST_CODE_CE_SHUFFLEVEC:
				make_eval(Operation::ShuffleVector, 0, ValueArray{values[v[0]], values[v[1]], values[v[2]]});
				break;

			case CST_CODE_CE_CMP:
				make_eval(v[3] & 32 ? Operation::ICmp : Operation::FCmp, v[3] & 31, ValueArray{values[v[1]], values[v[2]]});
				break;

			case CST_CODE_CE_SHUFVEC_EX:
				make_eval(Operation::ShuffleVector, 0, ValueArray{values[v[1]], values[v[2]], values[v[3]]});
				break;

			case CST_CODE_CE_UNOP:
				make_eval(UnaryOperation((UnaryOpcodes)v[0]), 0, values[v[1]]);
				break;

			case CST_CODE_CE_CAST:
				make_eval(CastOperation((CastOpcodes)v[0]), 0, values[v[2]]);
				break;

			//case CST_CODE_CE_GEP_WITH_INRANGE_INDEX:
			case CST_CODE_CE_GEP:
			case CST_CODE_CE_INBOUNDS_GEP: {
				uint64	*p	= v.begin();
				const Type *type = v.size() & 1 ? types[*p++] : nullptr;

				ValueArray	args;
				while (p < v.end()) {
					const Type	*t	= types[*p++];
					const Value	&v	= values[*p++];
					args.push_back(v);
				}

				auto	type0 = args[0].GetType();
				ISO_ASSERT(!type || type == type0->subtype);

				// walk the type list to get the return type
				type = types.find(Type::make_pointer(WalkGEP(type0->subtype, args.slice(2)), type0->addrSpace));
				make_eval(Operation::GetElementPtr, code == CST_CODE_CE_INBOUNDS_GEP, move(args));
				break;
			}

			case CST_CODE_INLINEASM_OLD:
			case CST_CODE_INLINEASM_OLD2:	// adds support for the asm dialect keywords
			case CST_CODE_INLINEASM_OLD3: {	// adds support for the unwind keyword
				RecordReader	r(*this, v);
				uint32	flags		= r.get<uint32>();
				auto	code		= string::get(r, r.get<uint32>());
				auto	constraints	= string::get(r, r.get<uint32>());
				values.push_back(new(allocator) Constant(cur_type, new(allocator) InlineAsm(flags, code, constraints)));
				break;
			}

			case CST_CODE_INLINEASM: {		// adds explicit function type
				RecordReader	r(*this, v);
				auto	type		= r.get_type();
				uint32	flags		= r.get<uint32>();
				auto	code		= string::get(r, r.get<uint32>());
				auto	constraints	= string::get(r, r.get<uint32>());
				values.push_back(new(allocator) Constant(type, new(allocator) InlineAsm(flags, code, constraints)));
				break;
			}

			case CST_CODE_AGGREGATE:
				values.push_back(new(allocator) Constant(cur_type, (ValueArray)transformc(v, [](uint64 i) { return ForwardRef(i); })));
				break;

			case CST_CODE_DATA:
				values.push_back(new(allocator) Constant(cur_type, (ValueArray)v));
				break;

			default:
				ISO_ASSERT(0);
				break;
		}
	}

	for (auto i : values.slice(start)) {
		auto	c = i.get_known<const Constant*>();
		for (Value &v : c->value.get<ValueArray>())
			if (Resolve(v))
				ISO_ASSERT(v.is<const Constant*>() || v.is<uint64>());
		if (Resolve(unconst(c)->value))
			ISO_ASSERT(c->value.is<const Constant*>() || c->value.is<uint64>());
	}

	return true;
}

struct read_debug_meta : RecordReader {
	read_debug_meta(ModuleReader &values, range<const uint64*> r) : RecordReader(values, r) {}

	template<typename T> auto operator()(const T*) {
		auto	d	= new(r.allocator) T;
		d->read(*this);
		return d;
	}
	auto operator()(const DebugInfo*) {
		return nullptr;
	}
};

bool ModuleReader::read_metadata(BlockReader&& reader, dynamic_array<NamedMetadata> &named_meta) {
	uint32	code;
	dynamic_array<uint64>	v;
	string	name;
	auto	i	= metadata.size();

	while (auto id = reader.nextRecordNoSub(code)) {
		reader.readRecord(id, v);
		switch (code) {
			case METADATA_STRING_OLD:
				SetMeta(i++, nullptr, get_string(v));
				break;

			case METADATA_VALUE:
				SetMeta(i++, types[v[0]], values[v[1]]);
				break;

			case METADATA_DISTINCT_NODE:
			case METADATA_NODE:
				SetMeta(i++, nullptr, (ValueArray)transformc(v, [&](uint64 i) { return GetMetaOrNull(i); }), code == METADATA_DISTINCT_NODE);
				break;

			case METADATA_NAME:
				name = get_string(v);
				break;

			case METADATA_KIND:
				meta_kinds.set(v[0]) = allocator.make((const char*)get_string(v.slice(1)));
				break;

			case METADATA_NAMED_NODE:
				named_meta.push_back({move(name), transformc(v, [&](uint64 i) { return GetMeta(i); })});
				break;

			case METADATA_LOCATION:
			case METADATA_BASIC_TYPE:
			case METADATA_FILE:
			case METADATA_DERIVED_TYPE:
			case METADATA_COMPOSITE_TYPE:
			case METADATA_SUBROUTINE_TYPE:
			case METADATA_COMPILE_UNIT:
			case METADATA_SUBPROGRAM:
			case METADATA_LEXICAL_BLOCK:
			case METADATA_TEMPLATE_TYPE:
			case METADATA_TEMPLATE_VALUE:
			case METADATA_GLOBAL_VAR:
			case METADATA_LOCAL_VAR:
			case METADATA_EXPRESSION:
			case METADATA_SUBRANGE:

			case METADATA_LEXICAL_BLOCK_FILE:
			case METADATA_NAMESPACE:
			case METADATA_GENERIC_DEBUG:
			case METADATA_ENUMERATOR:
			case METADATA_OBJC_PROPERTY:
			case METADATA_IMPORTED_ENTITY:
			case METADATA_MODULE:
			case METADATA_MACRO:
			case METADATA_MACRO_FILE:
			case METADATA_GLOBAL_VAR_EXPR:
			case METADATA_LABEL:
			case METADATA_STRING_TYPE:
			case METADATA_COMMON_BLOCK:
			case METADATA_GENERIC_SUBRANGE:
			{
				DebugInfo	dummy((MetadataCodes)code);
				if (auto d = process<DebugInfo*>(&dummy, read_debug_meta(*this, v.slice(1))))
					SetMeta(i, nullptr, d, v[0] & 1);
				++i;
				break;
			}

			case METADATA_OLD_NODE:
			case METADATA_OLD_FN_NODE:
			case METADATA_ATTACHMENT:
			case METADATA_STRINGS:
			case METADATA_GLOBAL_DECL_ATTACHMENT:
			case METADATA_INDEX_OFFSET:
			case METADATA_INDEX:
			case METADATA_ARG_LIST:
			default:
				ISO_ASSERT2(0, "Unsupported");
				break;
		}
	}
	return true;
}

// helper struct for reading ops
class OpReader : public RecordReader {
	const Value&	getSymbol(uint64 val)	{ return r.values[r.values.size() - val]; }

public:
	//template<typename T> T get() { return (T)RecordReader::get(); }
	
	OpReader(ModuleReader &r, const dynamic_array<uint64> &v) : RecordReader(r, v) {}

	const Value&	getSymbolAbsolute()		{ return r.values[get<size_t>()]; }

	Value	getSymbol(bool withType = true) {
		uint64 val = get<uint64>();

		// if it's not a forward reference, resolve the relative-ness and return
		if (val <= r.values.size())
			return getSymbol(val);

		// sometimes forward references have types
		ISO_ASSERT(!withType);
		if (withType)
			(void)get_type();

		return Value(ForwardRef(r.values.size() - (int32)val));
	}

	Value	getSymbolAndType(const Type* &type) {
		uint64 val = get<uint64>();

		// if it's not a forward reference, resolve the relative-ness and return
		if (val <= r.values.size()) {
			Value v	= getSymbol(val);
			type	= v.GetType();
			return v;
		}

		type = get_type();
		return Value(ForwardRef(r.values.size() - (int32)val));
	}

	Value	getSymbolS() {
		int64	val	 = decode_svbr(get<uint64>());
		if (val <= 0)
			return ForwardRef(r.values.size() - val);
		return getSymbol((uint64)val);
	}
};

bool ModuleReader::read_function(BlockReader&& reader, Function &f, const BlockInfoBlock &infos) {
	uint32					code;
	dynamic_array<uint64>	v;

	size_t	prev_values		= values.size();
	size_t	prev_meta		= metadata.size();
	Block*	cur_block		= nullptr;
	const DILocation* cur_loc	= nullptr;

	auto	add_branch		= [&](OpReader &op, Instruction *inst) {
		auto&	dest = f.blocks[op.get<size_t>()];
		inst->args.push_back(&dest);
		dest.preds.insert(dest.preds.begin(), cur_block);
	};

	f.args.reserve(f.type->members.size());
	for (auto &i : f.type->members) {
		auto	&arg	= f.args.emplace_back(Operation::Nop, i);
		arg.name		= format_string("arg%i", f.type->members.index_of(i));
		values.push_back(&arg);
	}

	while (auto id = reader.nextRecord(code)) {
		if (id == ENTER_SUBBLOCK) {
			switch (code) {
				case CONSTANTS_BLOCK_ID:
					read_consts(reader.enterBlock(infos[code]));
					break;

				case METADATA_BLOCK_ID: {
					auto	reader2 = reader.enterBlock(infos[code]);
					while (auto id = reader2.nextRecordNoSub(code)) {
						if (code == METADATA_VALUE) {
							//isConstant
							reader2.readRecord(id, v);
							if (v[1] < values.size())
								metadata.push_back(new(allocator) Metadata(types[v[0]], values[v[1]]));
							else
								metadata.push_back(new(allocator) Metadata(types[v[0]], ForwardRef(v[1])));
						} else {
							reader.skipRecord(id);
						}
					}
					break;
				}

				case METADATA_ATTACHMENT_ID: {
					auto	reader2 = reader.enterBlock(infos[code]);
					while (auto id = reader2.nextRecordNoSub(code)) {
						if (code == METADATA_ATTACHMENT) {
							reader2.readRecord(id, v);
							auto& attach = v.size() & 1 ? f.instructions[v[0]]->attachedMeta : f.attachedMeta;
							for (auto p = v.begin() + (v.size() & 1); p < v.end(); p += 2)
								attach.push_back({meta_kinds[p[0]], GetMeta(p[1])});
						} else {
							reader2.skipRecord(id);
						}
					}
					break;
				}

				case VALUE_SYMTAB_BLOCK_ID: {
					auto	reader2 = reader.enterBlock(infos[code]);
						while (auto id = reader2.nextRecordNoSub(code)) {
							reader2.readRecord(id, v);
							switch (code) {
								case VST_CODE_ENTRY: {
									if (v[0] < values.size()) {
										if (auto *s = values[v[0]].GetName())
											*unconst(s) = get_string(v.slice(1));
									}
									break;
								}
								case VST_CODE_BBENTRY:
									f.blocks[v[0]].name = get_string(v.slice(1));
									break;
							}
						}
					}
					break;

				case USELIST_BLOCK_ID: {
					auto	reader2 = reader.enterBlock(infos[code]);
					while (auto id = reader2.nextRecordNoSub(code)) {
						if (code == USELIST_CODE_DEFAULT || code == USELIST_CODE_BB) {
							reader2.readRecord(id, v);
							int	i = v.pop_back_value();
							f.uselist.emplace_back(code == USELIST_CODE_BB ? Value(&f.blocks[i]) : values[i], v);
						} else {
							reader2.skipRecord(id);
						}
					}
					break;
				}

				default:
					reader.skipRecord(id);
					break;
			}
			continue;
		}
		
		reader.readRecord(id, v);
		OpReader op(*this, v);

		switch (code) {
			case FUNC_CODE_DECLAREBLOCKS:
				f.blocks.resize(v[0]);
				cur_block = f.blocks.begin();
				break;

			case FUNC_CODE_DEBUG_LOC:
				cur_loc	= get_loc(DILocation(v[0], v[1], GetMetaOrNull(v[2]), GetMetaOrNull(v[3])));
				//fall through
			case FUNC_CODE_DEBUG_LOC_AGAIN:
				f.instructions.back()->debug_loc = cur_loc;
				break;

			case FUNC_CODE_INST_BINOP: {
				auto	inst = f.instructions.push_back();
				inst->args.push_back(op.getSymbolAndType(inst->type));
				inst->args.push_back(op.getSymbol(false));

				inst->op = BinaryOperation(op.get<BinaryOpcodes>(), inst->type->is_float());

				if (op.remaining() > 0)
					op.read(inst->optimisation);// = op.get<uint8>();

				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_CAST: {
				auto	inst = f.instructions.push_back();
				inst->args.push_back(op.getSymbol());
				inst->type	= op.get_type();
				inst->op		= CastOperation(op.get<CastOpcodes>());
				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_INBOUNDS_GEP_OLD:
			case FUNC_CODE_INST_GEP_OLD:
			case FUNC_CODE_INST_GEP: {
				auto	inst = f.instructions.emplace_back(Operation::GetElementPtr);
				inst->InBounds = code == FUNC_CODE_INST_INBOUNDS_GEP_OLD || (code == FUNC_CODE_INST_GEP && op.get<uint64>());

				const Type *type = code == FUNC_CODE_INST_GEP ? op.get_type() : nullptr;
				const Type *type0;
				inst->args.push_back(op.getSymbolAndType(type0));

				if (!type)
					type = type0;

				while (op.remaining() > 0)
					inst->args.push_back(op.getSymbol());

				// walk the type list to get the return type
				inst->type = types.find(Type::make_pointer(WalkGEP(type, inst->args.slice(2)), type0->addrSpace));
				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_SELECT:
			case FUNC_CODE_INST_VSELECT: {
				auto	inst = f.instructions.emplace_back(Operation::Select);
				inst->args.push_back(op.getSymbolAndType(inst->type));				// if true
				inst->args.push_back(op.getSymbol(false));							// if false
				inst->args.push_back(op.getSymbol(code == FUNC_CODE_INST_VSELECT));	// selector
				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_EXTRACTELT: {
				auto	inst = f.instructions.emplace_back(Operation::ExtractElement);
				inst->args.push_back(op.getSymbolAndType(inst->type));			// vector
				inst->type = inst->type->subtype;									// result is the scalar type within the vector
				inst->args.push_back(op.getSymbol());							// index
				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_INSERTELT: {
				auto	inst = f.instructions.emplace_back(Operation::InsertElement);
				inst->args.push_back(op.getSymbolAndType(inst->type));			// vector
				inst->args.push_back(op.getSymbol(false));						// replacement element
				inst->args.push_back(op.getSymbol());							// index
				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_SHUFFLEVEC: {
				auto	inst = f.instructions.emplace_back(Operation::ShuffleVector);
				const Type* vecType;
				inst->args.push_back(op.getSymbolAndType(vecType));				// vector 1
				inst->args.push_back(op.getSymbol(false));						// vector 2

				const Type* maskType;
				inst->args.push_back(op.getSymbolAndType(maskType));				// indexes
				
				// result is a vector with the inner type of the first two vectors and the element count of the last vector
				inst->type = types.find(Type::make_vector(vecType->subtype, maskType->size));
				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_CMP:
			case FUNC_CODE_INST_CMP2: {
				auto	inst = f.instructions.emplace_back(Operation::FCmp);
				const Type* argType;
				inst->args.push_back(op.getSymbolAndType(argType));				// a
				inst->args.push_back(op.getSymbol(false));						// b

				auto	predicate = op.get<uint8>();
				if (predicate & 32) {
					inst->op = Operation::ICmp;
					predicate &= 31;
				}
				inst->predicate	= predicate;

				if (op.remaining() > 0)
					inst->float_flags = op.get<FastMathFlags>();

				// if we're comparing vectors, the return type is an equal sized bool vector
				inst->type = argType->type == Type::Vector 
					? types.find(Type::make_vector(types.get<bool>(), argType->size))
					: types.get<bool>();

				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_RET: {
				auto	inst = f.instructions.emplace_back(Operation::Ret, types.get<void>());

				if (op.remaining()) {
					inst->args.push_back(op.getSymbolAndType(inst->type));
					values.push_back(inst);
				}

				cur_block++;
				break;
			}

			case FUNC_CODE_INST_BR: {
				auto	inst = f.instructions.emplace_back(Operation::Br, types.get<void>());
				add_branch(op, inst);							// true destination
				if (op.remaining() > 0) {
					add_branch(op, inst);						// false destination
					inst->args.push_back(op.getSymbol(false));	// predicate
				}
				cur_block++;
				break;
			}

			case FUNC_CODE_INST_SWITCH: {
				static const uint64 SWITCH_INST_MAGIC = 0x4B5;

				auto	inst = f.instructions.emplace_back(Operation::Switch, types.get<void>());
				uint64	typeIdx = op.get<uint64>();

				if ((typeIdx >> 16) == SWITCH_INST_MAGIC) {
					// 'new' format with case ranges, which was reverted
					ISO_ASSERT(0);

				} else {
					inst->args.push_back(op.getSymbol(false));		// condition
					add_branch(op, inst);							// default block
					while (op.remaining()) {
						inst->args.push_back(op.getSymbolAbsolute());// case value
						add_branch(op, inst);						// case block
					}
				}

				cur_block++;
				break;
			}

		#if 0
			case FUNC_CODE_INST_INVOKE: {
				auto	inst = f.instructions.emplace_back(Operation::Invoke);

				if (size_t attr = op.get<size_t>())
					inst->paramAttrs = &attr_sets[attr - 1];

				auto	callingFlags = op.get<CallMarkersFlags>();
				if (callingFlags & CALL_FMF)
					inst->float_flags = op.get<Instruction::FastMathFlags>();

				const Type* funcCallType = callingFlags & CALL_EXPLICIT_TYPE ? op.get_type() : nullptr;
				inst->type		= inst->funcCall->type->subtype;

				for (size_t i = 0; op.remaining() > 0; i++) {
					if (inst->funcCall->type->members[i]->type == Type::Metadata) {
						size_t	idx	 = values.size() - op.get<int32>();
						inst->args.push_back(GetMeta(idx, &f));
					} else {
						inst->args.push_back(op.getSymbol(false));
					}
				}

				if (inst->type->type != Type::Void)
					values.push_back(inst);
				break;
			}
		#endif

			case FUNC_CODE_INST_UNREACHABLE:
				f.instructions.emplace_back(Operation::Unreachable, types.get<void>());
				cur_block++;
				break;

			case FUNC_CODE_INST_PHI: {
				auto	inst = f.instructions.emplace_back(Operation::Phi, op.get_type());
				while (op.remaining() > 0) {
					inst->args.push_back(op.getSymbolS());
					inst->args.push_back(&f.blocks[op.get<uint64>()]);
				}
				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_ALLOCA: {
				auto	inst = f.instructions.emplace_back(Operation::Alloca, types.find(Type::make_pointer(op.get_type(), Type::AddrSpace::Default)));
				const Type* sizeType = op.get_type();	// type of the size - ignored
				inst->args.push_back(op.getSymbolAbsolute());

				AllocaPackedValues flags(op.get<uint64>());
				inst->alloca_argument	= flags.call_argument;
				if (!flags.explicit_type) {
					ISO_ASSERT(inst->type->type == Type::Pointer);
					inst->type = inst->type->subtype;
				}
				inst->align	= (1u << flags.align()) >> 1;

				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_LOAD: {
				auto	inst = f.instructions.emplace_back(Operation::Load);
				inst->args.push_back(op.getSymbolAndType(inst->type));

				if (op.remaining() == 3) {
					inst->type = op.get_type();
				} else {
					ISO_ASSERT(inst->type->type == Type::Pointer);
					inst->type = inst->type->subtype;
				}

				inst->align		= (1U << op.get<uint64>()) >> 1;
				inst->Volatile	= op.get<bool>();

				values.push_back(inst);
				break;
			}

//			case FUNC_CODE_INST_VAARG:

			case FUNC_CODE_INST_STORE_OLD:
			case FUNC_CODE_INST_STORE: {
				auto	inst = f.instructions.emplace_back(Operation::Store, types.get<void>());

				inst->args.push_back(op.getSymbol());
				inst->args.push_back(op.getSymbol(code != FUNC_CODE_INST_STORE_OLD));

				inst->align = (1U << op.get<uint64>()) >> 1;
				inst->Volatile	= op.get<bool>();
				break;
			}

			case FUNC_CODE_INST_EXTRACTVAL: {
				auto	inst = f.instructions.emplace_back(Operation::ExtractValue);
				inst->args.push_back(op.getSymbolAndType(inst->type));

				while (op.remaining() > 0) {
					uint64 val = op.get<uint64>();
					if (inst->type->type == Type::Array)
						inst->type = inst->type->subtype;
					else
						inst->type = inst->type->members[val];
					inst->args.emplace_back(val);
				}

				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_INSERTVAL: {
				// DXIL claims to be scalarised so should this appear?
				auto	inst = f.instructions.emplace_back(Operation::InsertValue);
				inst->args.push_back(op.getSymbolAndType(inst->type));		// aggregate
				inst->args.push_back(op.getSymbol());						// replacement element
				
				// indices as literals
				while (op.remaining() > 0)
					inst->args.emplace_back(op.get<uint64>());

				values.push_back(inst);
				break;
			}

//			case FUNC_CODE_INST_INDIRECTBR:

			case FUNC_CODE_INST_CALL: {
				auto	inst = f.instructions.emplace_back(Operation::Call);

				inst->paramAttrs = attr_sets.getOrNull(op.get<size_t>());

				auto callingFlags = op.get<CallMarkersFlags>();
				inst->call_tail = !!(callingFlags & CALL_TAIL) + !!(callingFlags & CALL_MUSTTAIL) * 2;

				if (callingFlags & CALL_FMF)
					inst->float_flags = op.get<FastMathFlags>();

				const Type* funcCallType = callingFlags & CALL_EXPLICIT_TYPE ? op.get_type() : nullptr;

				inst->funcCall	= op.getSymbol().get<const Function*>();
				inst->type		= inst->funcCall->type->subtype;

				ISO_ASSERT(!funcCallType || funcCallType == inst->funcCall->type);

				for (size_t i = 0; op.remaining() > 0; i++) {
					if (inst->funcCall->type->members[i]->type == Type::Metadata) {
						size_t	idx	 = values.size() - (int)op.get<uint32>();
						inst->args.push_back(GetMeta(idx));
					} else {
						inst->args.push_back(op.getSymbol(false));
					}
				}

				if (inst->type->type != Type::Void)
					values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_FENCE: {
				auto	inst = f.instructions.emplace_back(Operation::Fence, types.get<void>());
				inst->success	= op.get<AtomicOrderingCodes>();			// success ordering
				inst->SyncScope	= op.get<bool>();							// synchronisation scope
				break;
			}

			case FUNC_CODE_INST_CMPXCHG:
			case FUNC_CODE_INST_CMPXCHG_OLD: {
				auto	inst = f.instructions.emplace_back(Operation::CmpXchg);
				const Type	*type;
				inst->args.push_back(op.getSymbolAndType(type));		// pointer to atomically modify
				ISO_ASSERT(type->type == Type::Pointer);
				type = type->subtype;

				// search for a {type, bool} struct
				if (auto t = types.find(Type::make_struct({type, types.get<bool>()})))
					type = t;

				ISO_ASSERT(type->type == Type::Struct);
				ISO_ASSERT(v.size() >= 8);									// expect modern encoding with weak parameters.

				inst->type		= type;
				inst->args.push_back(op.getSymbol(code == FUNC_CODE_INST_CMPXCHG));	// compare value
				inst->args.push_back(op.getSymbol(false));					// new replacement value

				inst->Volatile	= op.get<bool>();
				inst->success	= op.get<AtomicOrderingCodes>();			// success ordering
				inst->SyncScope	= op.get<bool>();							// synchronisation scope
				inst->failure	= op.get<AtomicOrderingCodes>();			// failure ordering
				inst->Weak		= !!op.get<uint64>();

				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_ATOMICRMW_OLD:
			case FUNC_CODE_INST_ATOMICRMW: {
				auto	inst = f.instructions.push_back(Operation::AtomicRMW);

				inst->args.push_back(op.getSymbolAndType(inst->type));	// pointer to atomically modify
				//ISO_ASSERT(inst->type->type == Type::Pointer);
				if (inst->type->type == Type::Pointer)
					inst->type = inst->type->subtype;

				inst->args.push_back(op.getSymbol(false));				// parameter value
				inst->rmw		= op.get<RMWOperations>();
				inst->Volatile	= op.get<bool>();
				inst->success	= op.get<AtomicOrderingCodes>();		// success ordering
				inst->SyncScope	= op.get<bool>();						// synchronisation scope

				values.push_back(inst);
				break;
			}

//			case FUNC_CODE_INST_RESUME:
			case FUNC_CODE_INST_LANDINGPAD_OLD:
			case FUNC_CODE_INST_LANDINGPAD:

			case FUNC_CODE_INST_LOADATOMIC: {
				auto	inst = f.instructions.emplace_back(Operation::LoadAtomic);
				inst->args.push_back(op.getSymbolAndType(inst->type));

				if (op.remaining() == 5) {
					inst->type = op.get_type();
				} else {
					ISO_ASSERT(inst->type->type == Type::Pointer);
					inst->type = inst->type->subtype;
				}

				inst->align		= (1u << op.get<uint64>()) >> 1;
				inst->Volatile	= op.get<bool>();
				inst->success	= op.get<AtomicOrderingCodes>();		// success ordering
				inst->SyncScope	= op.get<bool>();						// synchronisation scope

				values.push_back(inst);
				break;
			}

			case FUNC_CODE_INST_STOREATOMIC_OLD:
			case FUNC_CODE_INST_STOREATOMIC: {
				auto	inst = f.instructions.emplace_back(Operation::StoreAtomic, types.get<void>());

				inst->args.push_back(op.getSymbol());
				inst->args.push_back(op.getSymbol(code == FUNC_CODE_INST_STOREATOMIC));
				inst->align		= (1u << op.get<uint64>()) >> 1;
				inst->Volatile	= op.get<bool>();
				inst->success	= op.get<AtomicOrderingCodes>();		// success ordering
				inst->SyncScope	= op.get<bool>();						// synchronisation scope

				break;
			}

			default:
				ISO_ASSERT2(0, "Unexpected record in FUNCTION_BLOCK");
				continue;
		}
	}

	// fix up forward references

	//for (auto& i : f.instructions) {
	//	for (auto& j : i.args) {
	//		if (j.is<ForwardInstruction>())
	//			j = &f.instructions[j.get<ForwardInstruction>()->index];
	//	}
	//}

	for (auto i : metadata.slice(prev_meta))
		Resolve(unconst(i)->value);

	uint32	id	= 0;
	bool	new_block	= true;
	
	cur_block = f.blocks.begin();
	for (auto &i : f.instructions) {
		for (Value &v : i->args)
			Resolve(v);

		if (new_block) {
			cur_block->first = &i;
			if (cur_block->name.empty())
				cur_block->id = id++;
		}

		if (new_block = is_terminator(i->op)) {
			cur_block++;

		} else if (i->type->type != Type::Void && !i->name) {
			i->id = id++;
		}
	}

	Restore(prev_values, prev_meta);
//	f.GetLive();
	return true;
}


bool Module::read(BlockReader &&reader) {
	if (!reader.verify())
		return false;

	BlockInfoBlock	infos;
	ModuleReader	r(allocator);

	uint32			next_function = 0;
	uint32			code;
	dynamic_array<uint64>	v;

	while (auto id = reader.nextRecord(code)) {
		if (id == ENTER_SUBBLOCK) {
			switch (code) {
				case BLOCKINFO_BLOCK_ID:
					infos.read(reader.enterBlock(nullptr));
					break;

				case PARAMATTR_BLOCK_ID:
					r.read_attrsets(reader.enterBlock(infos[code]));
					break;

				case PARAMATTR_GROUP_BLOCK_ID:
					r.read_attrgroups(reader.enterBlock(infos[code]));
					break;

				case CONSTANTS_BLOCK_ID:
					r.read_consts(reader.enterBlock(infos[code]));//, consts);
					break;

				case FUNCTION_BLOCK_ID: {
					while (functions[next_function]->prototype)
						++next_function;

					r.read_function(reader.enterBlock(infos[code]), *functions[next_function++], infos);
					break;
				}

				case VALUE_SYMTAB_BLOCK_ID: {
					auto	reader2 = reader.enterBlock(infos[code]);
					while (auto id = reader2.nextRecordNoSub(code)) {
						reader2.readRecord(id, v);
						if (code == VST_CODE_ENTRY && v[0] < r.values.size()) {
							if (auto *s = r.values[v[0]].GetName())
								*unconst(s) = get_string(v.slice(1));
						}
					}
					break;
				}

				case METADATA_BLOCK_ID:
					r.read_metadata(reader.enterBlock(infos[code]), named_meta);
					break;

				case METADATA_ATTACHMENT_ID:
					ISO_ASSERT(0);
					break;

				case TYPE_BLOCK_ID_NEW:
					r.read_types(reader.enterBlock(infos[code]));
					break;

				case USELIST_BLOCK_ID: {
					auto	reader2 = reader.enterBlock(infos[code]);
					while (auto id = reader2.nextRecordNoSub(code)) {
						if (code == USELIST_CODE_DEFAULT) {
							reader2.readRecord(id, v);
							int	i = v.pop_back_value();
							uselist.emplace_back(r.values[i], v);
						} else {
							reader2.skipRecord(id);
						}
					}
					break;
				}

				default:
					reader.skipRecord(id);
					break;

			}
			continue;
		}
		reader.readRecord(id, v);

		switch (code) {
			case MODULE_CODE_VERSION:
				version		= v[0];
				ISO_ASSERT(version == VERSION);
				break;

			case MODULE_CODE_TRIPLE:
				triple		= get_string(v);
				break;

			case MODULE_CODE_DATALAYOUT:
				datalayout	= get_string(v);
				break;

			case MODULE_CODE_ASM:
				assembly	= get_string(v);
				break;

			case MODULE_CODE_SECTIONNAME:
				r.sections.push_back(allocator.make((const char*)get_string(v)));
				break;

			case MODULE_CODE_GCNAME:
				r.gcs.push_back(allocator.make((const char*)get_string(v)));
				break;

			case MODULE_CODE_COMDAT:
				r.comdats.push_back(new Comdat{Comdat::Selection(v[0]), get_string(v.slice(2, v[1]))});
				break;

			case MODULE_CODE_GLOBALVAR: {
				auto	type		= r.types[v[0]];
				auto	addrSpace	= v[1] & 2 ? Type::AddrSpace(v[1] >> 2) : type->addrSpace;
				auto	g			= new(allocator) GlobalVar(r.types.find(Type::make_pointer(type, addrSpace)));
				globalvars.push_back(g);
				r.values.push_back(g);

				g->is_const = v[1] & 1;
				g->value	= MaybeForwardRef(v[2]);
				g->linkage	= Linkage(v[3]);
				g->align	= (1ULL << v[4]) >> 1;
				g->section	= r.sections.getOrNull(v[5]);

				switch (v.size()) {
					default:	g->comdat		= v[11] ? r.comdats[v[11] - 1] : nullptr;
					case 11:	g->dll_storage	= DLLStorageClass(v[10]);
					case 10:	g->external_init = v[9];
					case 9:		g->unnamed_addr = UnnamedAddr(v[8]);
					case 8:		g->threadlocal	= ThreadLocalMode(v[7]);
					case 7:		g->visibility	= Visibility(v[6]);
					case 6:		break;
				}

				break;
			}
			case MODULE_CODE_FUNCTION: {
				auto	f	= new(allocator) Function(r.types[v[0]]);
				functions.push_back(f);
				r.values.push_back(f);

				f->calling_conv = CallingConv(v[1]);
				f->prototype	= v[2];
				f->linkage		= Linkage(v[3]);
				f->attrs		= r.attr_sets.getOrNull(v[4]);
				f->align		= v[5];

				switch (v.size()) {
					default:	//prefix
					case 13:	f->comdat		= v[12] ? r.comdats[v[12] - 1] : nullptr;
					case 12:	f->dll_storage	= DLLStorageClass(v[11]);
					case 11:	f->prolog_data	= MaybeForwardRef(v[10]);
					case 10:	f->unnamed_addr = UnnamedAddr(v[9]);
					case 9:		f->gc			= r.gcs.getOrNull(v[8]);
					case 8:		f->visibility	= Visibility(v[7]);
					case 7:		f->section		= r.sections.getOrNull(v[6]);
					case 6:		break;
				}

				break;
			}
			case MODULE_CODE_ALIAS: {
				auto	a	= new(allocator) Alias(r.types[v[0]], r.values[v[1]]);
				r.values.push_back(a);

				switch (v.size()) {
					default:	//prefix
					case 4:		a->visibility	= Visibility(v[3]);
					case 3:		a->linkage		= Linkage(v[2]);
					case 2:		break;
				}
				break;
			}
		}
	}

	for (auto g : globalvars)
		r.Resolve(g->value);

	for (auto f : functions) {
		r.Resolve(f->prolog_data);
		r.Resolve(f->prefix_data);
		r.Resolve(f->personality_function);
	}

	return true;
}

//-----------------------------------------------------------------------------
//	Writing
//-----------------------------------------------------------------------------

// Pinned metadata names, which always have the same value
// This is a compile-time performance optimization, not a correctness optimization
const char *pinned_metadata_kinds[] = {
	"dbg",
	"tbaa",
	"prof",
	"fpmath",
	"range",
	"tbaa.struct",
	"invariant.load",
	"alias.scope",
	"noalias",
	"nontemporal",
	"llvm.mem.parallel_loop_access",
	"nonnull",
	"dereferenceable",
	"dereferenceable_or_null",
};

struct ModuleWriter : Collector {
	struct Abbrev {
		AbbrevID	VST_ENTRY[3];
		AbbrevID	VST_BBENTRY_6;
		AbbrevID	CONSTANTS_SETTYPE;
		AbbrevID	CONSTANTS_INTEGER;
		AbbrevID	CONSTANTS_CE_CAST;
		AbbrevID	CONSTANTS_NULL;
		AbbrevID	FUNCTION_INST_LOAD;
		AbbrevID	FUNCTION_INST_BINOP;
		AbbrevID	FUNCTION_INST_BINOP_FLAGS;
		AbbrevID	FUNCTION_INST_CAST;
		AbbrevID	FUNCTION_INST_RET_VOID;
		AbbrevID	FUNCTION_INST_RET_VAL;
		AbbrevID	FUNCTION_INST_UNREACHABLE;
		AbbrevID	FUNCTION_INST_GEP;
		void	init(BlockInfoBlock &info, uint32 type_bits);
	} abbr;

	hash_map<Value, uint32>				value_map;
	hash_map<const Metadata*, uint32>	meta_map;
	hash_map<const Type*, uint32>		type_map;

	struct DebugInfoWriter {
		ModuleWriter	&w;
		DebugInfoWriter(ModuleWriter &w) : w(w) {}
		template<typename...T>	bool	write(T&&...t) {
			bool dummy[] = { add(t)...};
			return true;
		}
		template<typename T> bool	add(const T&) { return false; }
		bool	add(const MetadataRef &m)	{ w.collect(m.m); return true; }
		bool	add(const MetadataRef1 &m)	{ w.collect(m.m); return true; }
		bool	add(const MetaString &m)	{ w.collect(m.m); return true; }
	};

	ModuleWriter() {
		meta_kinds = pinned_metadata_kinds;
	}

	uint32	getID(const Value &v)					const { return value_map[v]; }
	uint32	getIDorNull(const Value &v)				const { return v ? value_map[v] + 1 : 0; }
	uint32	getTypeID(const Type *t)				const { return type_map[t]; }
	uint32	getMetadataID(const Metadata* m)		const { return meta_map[m]; }
	uint32	getMetadataOrNullID(const Metadata* m)	const { return m ? getMetadataID(m) + 1 : 0; }
	uint32	getAttributeID(const AttributeSet &attr)const { return attr ? attr_sets.index_of(find(attr_sets, attr)) : 0; }

	void	Restore(uint32 n, uint32 m) {
		for (auto &i : values.slice(n))
			value_map.remove(i);
		values.resize(n);

		for (auto &i : metadata.slice(m))
			meta_map.remove(i);
		metadata.resize(m);
	}

	void	add_type(const Type *t) {
		if (t && !type_map[t].exists()) {
			type_map[t] = -1;
			add_type(t->subtype);
			for (auto i : t->members)
				add_type(i);
			type_map[t] = types.size32();
			types.push_back(t);
		}
	}

	void	add_value(const Value& v) {
		value_map[v] = values.size32();
		values.push_back(v);
	}

	bool	try_add_value(const Value& v) {
		if (value_map[v].exists())
			return false;
		add_value(v);
		return true;
	}

	void	collect(const AttributeSet& set) {
		if (attr_sets.add_unique(set)) {
			for (auto &g : *set)
				attr_groups.add_unique(g);
		}
	}

	void	collect(const Metadata *m, bool in_func = false) {
		if (m && !meta_map[m].exists()) {
			if (m->type)
				add_type(m->type);

			if (in_func || !m->value.is<const Instruction*>()) {
				meta_map[m] = -1;
				collect(m->value);
				meta_map[m] = metadata.size32();
				metadata.push_back(m);
			}
		}
	}
	void	collect(const AttachedMetadata &m) {
		meta_kinds.add_unique(m.kind);
		collect(m.metadata);
	}
	void	collect(const Value& v, bool in_func = false) {
		switch (v.kind()) {
			case Value::best_index<_none>:
			case Value::best_index<uint64>:
			case Value::best_index<string>:
			case Value::best_index<ForwardRef>:
			case Value::best_index<_zero>:
			case Value::best_index<const Block*>:
				break;

			case Value::best_index<const Instruction*>:
				//ISO_ASSERT(in_func);
				break;

			case Value::best_index<ValueArray>:
				for (auto &i : v.get_known<ValueArray>())
					collect(i, in_func);
				break;

			case Value::best_index<const Eval*>:
				if (try_add_value(v))
					collect(v.get_known<const Eval*>()->arg);
				break;

			case Value::best_index<const Constant*>:
				if (try_add_value(v)) {
					auto	c = v.get_known<const Constant*>();
					add_type(c->type);
					collect(c->value);
				}
				break;

			case Value::best_index<const GlobalVar*>:
				if (try_add_value(v)) {
					auto	g = v.get_known<const GlobalVar*>();
					sections.add_unique(g->section);
					comdats.add_unique(g->comdat);
					add_type(g->type);
					collect(g->value);
				}
				break;

			case Value::best_index<const Alias*>:
				if (try_add_value(v)) {
					auto	a = v.get_known<const Alias*>();
					add_type(a->type);
					collect(a->value);
				}
				break;

			case Value::best_index<const Metadata*>:
				collect(v.get_known<const Metadata*>(), in_func);
				break;

			case Value::best_index<const Function*>:
				if (try_add_value(v)) {
					auto	f = v.get_known<const Function*>();
					sections.add_unique(f->section);
					comdats.add_unique(f->comdat);
					collect(f->attrs);
					add_type(f->type);
					for (auto &m : f->attachedMeta)
						collect(m);

					const DILocation *last_loc	= nullptr;
					for (auto inst : f->instructions) {
						collect(inst->paramAttrs);
						add_type(inst->type);
						for (auto &a : inst->args) {
							if (a.is<const Constant*>()) {
								add_type(a.get_known<const Constant*>()->type);
								continue;
							}
							if (a.is<const Metadata*>()) {
								if (a.get_known<const Metadata*>()->value.is<const Instruction*>())
									continue;
							}
							collect(a);
						}

						for (auto &m : inst->attachedMeta)
							collect(m);

						if (inst->debug_loc && inst->debug_loc != last_loc) {
							last_loc = inst->debug_loc;
							collect(last_loc->scope);
							collect(last_loc->inlined_at);
						}
					}
				}
				break;

			case Value::best_index<const DebugInfo*>: {
				DebugInfoWriter w(*this);
				process<void>(v.get_known<const DebugInfo*>(), [&](auto d) mutable { d->write(w); });
				break;
			}

			default:
				ISO_ASSERT(0);
		}
	}

	range<const Value*>	get_all(int kind, uint32 from) {
		auto	i0 = values.begin() + from, i = i0, d = i;
		while (i != values.end() && i->kind() != kind)
			++i;

		while (i != values.end()) {
			if (i->kind() == kind) {
				swap(*d, *i);
				value_map[*d] = values.index_of(d);
				++d;
			}
			++i;
		}

		return {i0, d};
	}

	template<typename T> range<T*>	get_all(uint32 from) {
		return element_cast<T>(get_all(Value::best_index<T>, from));
	}

	bool	has_symbols(uint32 from) const {
		for (auto &v : values.slice(from)) {
			if (auto *s = v.GetName(false)) {
				if (*s)
					return true;
			}
		}
		return false;
	}
	void	make_abbrevs(BlockInfoBlock &info) {
		abbr.init(info, log2_ceil(types.size()));
	}

	void	write_attrgroups(BlockWriter&& writer);
	void	write_attrsets(BlockWriter&& writer);
	bool	write_types(BlockWriter&& writer) const;
	bool	write_consts(BlockWriter&& writer, range<const Constant**> consts, bool global) const;
	bool	write_metadata(BlockWriter &writer) const;
	bool	write_function(BlockWriter&& writer, const Function &f, const BlockInfoBlock &info);
};


void ModuleWriter::Abbrev::init(BlockInfoBlock &info, uint32 type_bits) {
	auto	abbrevs		= info[VALUE_SYMTAB_BLOCK_ID];

	VST_ENTRY[0] = abbrevs->add_abbrev({
		{AbbrevOp::Fixed, 3},
		{AbbrevOp::VBR, 8},
		{AbbrevOp::Array_Fixed, 8},
	});

	VST_ENTRY[1] = abbrevs->add_abbrev({
		VST_CODE_ENTRY,
		{AbbrevOp::VBR, 8},
		{AbbrevOp::Array_Fixed, 7}
	});

	VST_ENTRY[2] = abbrevs->add_abbrev({
		VST_CODE_ENTRY,
		{AbbrevOp::VBR, 8},
		{AbbrevOp::Array_Char6}
	});

	VST_BBENTRY_6 = abbrevs->add_abbrev({
		VST_CODE_BBENTRY,
		{AbbrevOp::VBR, 8},
		{AbbrevOp::Array_Char6}
	});

	abbrevs = info[CONSTANTS_BLOCK_ID];

	CONSTANTS_SETTYPE = abbrevs->add_abbrev({
		CST_CODE_SETTYPE,
		{AbbrevOp::Fixed, type_bits}
	});

	CONSTANTS_INTEGER = abbrevs->add_abbrev({
		CST_CODE_INTEGER,
		{AbbrevOp::VBR, 8}
	});

	CONSTANTS_CE_CAST = abbrevs->add_abbrev({
		CST_CODE_CE_CAST,
		{AbbrevOp::Fixed, 4},			// cast opc
		{AbbrevOp::Fixed, type_bits},	// typeid
		{AbbrevOp::VBR, 8}				// value id
	});

	// NULL abbrev for CONSTANTS_BLOCK
	CONSTANTS_NULL = abbrevs->add_abbrev({
		CST_CODE_NULL
	});

	abbrevs = info[FUNCTION_BLOCK_ID];

	FUNCTION_INST_LOAD = abbrevs->add_abbrev({
		FUNC_CODE_INST_LOAD,
		{AbbrevOp::VBR, 6},				// Ptr
		{AbbrevOp::Fixed, type_bits},	// dest ty
		{AbbrevOp::VBR, 4},				// Align
		{AbbrevOp::Fixed, 1}			// volatile
	});

	FUNCTION_INST_BINOP = abbrevs->add_abbrev({
		FUNC_CODE_INST_BINOP,
		{AbbrevOp::VBR, 6},				// LHS
		{AbbrevOp::VBR, 6},				// RHS
		{AbbrevOp::Fixed, 4}			// opc
	});

	FUNCTION_INST_BINOP_FLAGS = abbrevs->add_abbrev({
		FUNC_CODE_INST_BINOP,
		{AbbrevOp::VBR, 6},				// LHS
		{AbbrevOp::VBR, 6},				// RHS
		{AbbrevOp::Fixed, 4},			// opc
		{AbbrevOp::Fixed, 7}			// flags
	});

	FUNCTION_INST_CAST = abbrevs->add_abbrev({
		FUNC_CODE_INST_CAST,
		{AbbrevOp::VBR, 6},				// OpVal
		{AbbrevOp::Fixed, type_bits},	// dest ty
		{AbbrevOp::Fixed, 4}			// opc
	});

	FUNCTION_INST_RET_VOID = abbrevs->add_abbrev({
		FUNC_CODE_INST_RET
	});

	FUNCTION_INST_RET_VAL = abbrevs->add_abbrev({
		FUNC_CODE_INST_RET,
		{AbbrevOp::VBR, 6}				// id
	});

	FUNCTION_INST_UNREACHABLE = abbrevs->add_abbrev({
		FUNC_CODE_INST_UNREACHABLE
	});

	FUNCTION_INST_GEP = abbrevs->add_abbrev({
		FUNC_CODE_INST_GEP,
		{AbbrevOp::Fixed, 1},
		{AbbrevOp::Fixed, type_bits},	// dest ty
		{AbbrevOp::Array_VBR, 6}
	});
}


class RecordWriter {
protected:
	const ModuleWriter		&w;
	dynamic_array<uint64>	&v;
	bool _put(uint64 x)		{ v.push_back(x); return true; }

public:
	RecordWriter(const ModuleWriter &w, dynamic_array<uint64>& v) : w(w), v(v) {}
	template<typename...T>	bool write(T&&...x)	{
		return write_early(*this, x...);
	}

	template<typename T> enable_if_t<(is_builtin_int<T> && !is_signed<T>) || is_char<T> || same_v<T, bool> || is_enum<T>, bool> custom_write(T x) {
		return _put((uint64)x);
	}
	template<typename T> enable_if_t<is_builtin_int<T> && is_signed<T>, bool> custom_write(T x) {
		return _put(encode_svbr(x));
	}
	bool	custom_write(const MetadataRef& x) {
		return _put(w.getMetadataOrNullID(x));
	}
	bool	custom_write(const MetadataRef1& x) {
		return _put(w.getMetadataID(x));
	}
	bool	custom_write(const MetaString &x) {
		return _put(w.getMetadataOrNullID(x.m));
	}
	bool	custom_write(const Type *type) {
		return _put(w.getTypeID(type));
	}
	friend bool	write(RecordWriter &w, const string &x) {
		w.v.append(x.begin(), x.end());
		return true;
	}
};

void ModuleWriter::write_attrgroups(BlockWriter&& writer) {
	dynamic_array<uint64>	v;
	for (auto &i : attr_groups) {
		if (i) {
			v.push_back(attr_groups.index_of(i));
			RecordWriter			w(*this, v);
			const AttributeGroup	*g = i;
			w.write(g->slot);
			for (uint64 p = g->params; p; p = clear_lowest(p)) {
				auto	i = AttributeIndex(lowest_set_index(p));
				if (g->values[i].exists())
					w.write(1u, i, g->values.get_known(i));
				else
					w.write(0u, i);
			}
			for (auto &i : g->strs) {
				w.write(i.b ? 4u : 3u, i.a, 0u);
				if (i.b)
					w.write(i.b, 0u);
			}

			writer.write_record(PARAMATTR_GRP_CODE_ENTRY, v);
			v.clear();
		}
	}
}

void ModuleWriter::write_attrsets(BlockWriter&& writer) {
	dynamic_array<uint64>	v;
	for (auto &a : attr_sets) {
		for (auto &s : *a)
			v.push_back(attr_groups.index_of(s));
		writer.write_record(PARAMATTR_CODE_ENTRY, v);
		v.clear();
	}
}

bool ModuleWriter::write_types(BlockWriter&& writer) const {
	uint64 type_bits = log2_ceil(types.size());

	auto PtrAbbrev = writer.add_abbrev({
		TYPE_CODE_POINTER,
		{AbbrevOp::Fixed, type_bits},
		0						// Addrspace = 0
	});
	auto FunctionAbbrev = writer.add_abbrev({
		TYPE_CODE_FUNCTION,
		{AbbrevOp::Fixed, 1},	// isvararg
		{AbbrevOp::Array_Fixed, type_bits}
	});
	auto StructAnonAbbrev = writer.add_abbrev({
		TYPE_CODE_STRUCT_ANON,
		{AbbrevOp::Fixed, 1},	// ispacked
		{AbbrevOp::Array_Fixed, type_bits}
	});
	auto StructNameAbbrev = writer.add_abbrev({
		TYPE_CODE_STRUCT_NAME,
		{AbbrevOp::Array_Char6}
	});
	auto StructNamedAbbrev = writer.add_abbrev({
		TYPE_CODE_STRUCT_NAMED,
		{AbbrevOp::Fixed, 1},	// ispacked
		{AbbrevOp::Array_Fixed, type_bits}
	});
	auto ArrayAbbrev = writer.add_abbrev({
		TYPE_CODE_ARRAY,
		{AbbrevOp::VBR, 8},		// size
		{AbbrevOp::Fixed, type_bits}
	});

	writer.write_record(TYPE_CODE_NUMENTRY, {types.size()});

	dynamic_array<uint64>	v;

	for (auto t : types) {
		TypeCodes	code	= TypeCodes(0);
		AbbrevID	abbrev	= AbbrevID(0);

		switch (t->type) {
			case Type::Void:
				code	= TYPE_CODE_VOID;
				break;

			case Type::Float:
				code =	t->size == 16		? TYPE_CODE_HALF
					:	t->size == 32		? TYPE_CODE_FLOAT
					:	t->size == 64		? TYPE_CODE_DOUBLE
					:	t->size == 80		? TYPE_CODE_X86_FP80
					:	t->size == 128		? TYPE_CODE_FP128
					:	t->size == 64+1		? TYPE_CODE_X86_MMX
					:	t->size == 128+1	? TYPE_CODE_PPC_FP128
					:	TypeCodes(0);	// error
				break;

			case Type::Int:
				code	= TYPE_CODE_INTEGER;
				v		= {t->size};
				break;

			case Type::Vector:
				code	= TYPE_CODE_VECTOR;
				v.push_back(t->size);
				v.push_back(getTypeID(t->subtype));
				break;

			case Type::Pointer:
				code	= TYPE_CODE_POINTER;
				v		= {getTypeID(t->subtype), t->addrSpace};
				if (t->addrSpace == 0)
					abbrev = PtrAbbrev;
				break;

			case Type::Array:
				code	= TYPE_CODE_ARRAY;
				abbrev	= ArrayAbbrev;
				v		= {t->size, getTypeID(t->subtype)};
				break;

			case Type::Function:
				code	= TYPE_CODE_FUNCTION;
				v		= {t->vararg, getTypeID(t->subtype)};
				for (auto &i : t->members)
					v.push_back(getTypeID(i));
				abbrev = FunctionAbbrev;
				break;

			case Type::Struct:
				if (t->name)
					writer.write_string(TYPE_CODE_STRUCT_NAME, t->name, StructNameAbbrev);

				if (t->opaque) {
					code	= TYPE_CODE_OPAQUE;

				} else {
					v.push_back(t->packed);
					for (auto &i : t->members)
						v.push_back(getTypeID(i));

					if (!t->name) {
						code	= TYPE_CODE_STRUCT_ANON;
						abbrev	= StructAnonAbbrev;

					} else {
						code	= TYPE_CODE_STRUCT_NAMED;
						abbrev	= StructNamedAbbrev;
					}
				}
				break;

			case Type::Metadata:
				code	= TYPE_CODE_METADATA;
				break;

			case Type::Label:
				code	= TYPE_CODE_LABEL;
				break;
		}

		// Emit the finished record
		ISO_ASSERT(code);
		writer.write_record(code, v, abbrev);
		v.clear();
	}
	return true;
}

bool ModuleWriter::write_consts(BlockWriter&& writer, range<const Constant**> consts, bool global) const {
	AbbrevID	AggregateAbbrev	= AbbrevID(0);
	AbbrevID	StringAbbrev[3]	= {AbbrevID(0)};

	// If this is a constant pool for the module, emit module-specific abbrevs
	if (global) {
		AggregateAbbrev = writer.add_abbrev({
			CST_CODE_AGGREGATE,
			{AbbrevOp::Array_Fixed, log2_ceil(values.size())}
		});
		StringAbbrev[1] = writer.add_abbrev({
			CST_CODE_CSTRING,
			{AbbrevOp::Array_Fixed, 7}
		});
		StringAbbrev[2] = writer.add_abbrev({
			CST_CODE_CSTRING,
			{AbbrevOp::Array_Char6}
		});
	}

	const Type*	cur_type = nullptr;
	dynamic_array<uint64>	v;

	for (auto c : consts) {
		// If we need to switch types, do so now
		if (c->type != cur_type) {
			cur_type = c->type;
			writer.write_record(CST_CODE_SETTYPE, {getTypeID(cur_type)}, abbr.CONSTANTS_SETTYPE);
		}

		ConstantsCodes	code	= ConstantsCodes(0);
		AbbrevID		abbrev	= AbbrevID(0);

		if (c->value.is<_none>()) {
			code	= CST_CODE_UNDEF;

		} else if (c->value.is<_zero>()) {
			code	= CST_CODE_NULL;

		} else if (c->value.is<string>()) {
			code	= CST_CODE_CSTRING;
			abbrev	= StringAbbrev[add_string(v, c->value.get_known<string>())];

		} else if (c->value.is<const Eval*>()) {
			auto	e		= c->value.get_known<const Eval*>();
			auto	args	= e->args_always();

			if (e->op == Operation::GetElementPtr) {
				code	= e->flags & 1 ? CST_CODE_CE_INBOUNDS_GEP : CST_CODE_CE_GEP;
				v.push_back(getTypeID(args[0].GetType()->subtype));
				for (auto &i : args) {
					v.push_back(getTypeID(i.GetType()));
					v.push_back(getID(i));
				}

			} else {
				code	= e->get_code();
				switch (code) {
					case CST_CODE_CE_CAST:
						abbrev	= abbr.CONSTANTS_CE_CAST;
						v.push_back((int)CastOperation(e->op));
						v.push_back(getTypeID(args[0].GetType()));
						break;
					case CST_CODE_CE_UNOP:
						v.push_back((int)UnaryOperation(e->op));
						break;
					case CST_CODE_CE_BINOP:
						v.push_back((int)BinaryOperation(e->op));
						break;
					case CST_CODE_CE_EXTRACTELT:
						v.push_back(getTypeID(args[0].GetType()));
						break;
					case CST_CODE_CE_CMP:
						v.push_back((e->flags & 0xff) + (e->op == Operation::ICmp) * 32);
						break;
				}
				for (auto &i : args)
					v.push_back(getID(i));
			}

		} else if (c->value.is<const InlineAsm*>()) {
			code	= CST_CODE_INLINEASM_OLD3;
			auto	a = c->value.get_known<const InlineAsm*>();
			v.push_back(a->flags);
			v.push_back(a->code.length());
			v.append(a->code);
			v.push_back(a->constraints.length());
			v.append(a->constraints);

		} else if (c->value.is<ValueArray>()) {
			code	= CST_CODE_DATA;
			for (auto &i : c->value.get_known<ValueArray>()) {
				if (!i.is<uint64>()) {
					code = CST_CODE_AGGREGATE;
					break;
				}
			}
			if (code == CST_CODE_DATA) {
				v = transformc(c->value.get_known<ValueArray>(), [](const Value &i) { return i.get_known<uint64>(); });

			} else {
				abbrev	= AggregateAbbrev;
				v = transformc(c->value.get_known<ValueArray>(), [this](const Value &i) { return getID(i); });
			}

		} else {
			switch (cur_type->type) {
				case Type::Int:
					code	= CST_CODE_INTEGER;
					abbrev	= abbr.CONSTANTS_INTEGER;
					v.push_back(encode_svbr(c->value.get_known<uint64>()));
					break;

				case Type::Float:
					code	= CST_CODE_FLOAT;
					v.push_back(c->value.get_known<uint64>());
					break;
			}
		}
		ISO_ASSERT(code);
		writer.write_record(code, v, abbrev);
		v.clear();
	}
	return true;
}

bool ModuleWriter::write_metadata(BlockWriter &writer) const {
	uint64	has	= 0;

	for (auto m : metadata) {
		if (m->value.is<string>())
			has |= 1 << METADATA_STRING_OLD;
		else if (m->value.is<const DebugInfo*>())
			has |= 1 << m->value.get_known<const DebugInfo*>()->kind;
	}

	AbbrevID MDSAbbrev = has & (1 << METADATA_STRING_OLD) ? writer.add_abbrev({
		METADATA_STRING_OLD,
		{AbbrevOp::Array_Fixed, 8},
	}) : AbbrevID(0);

	// Assume the column is usually under 128, and always output the inlined-at location (it's never more expensive than building an array size 1).
	auto DILocationAbbrev = has & (1 << METADATA_LOCATION) ? writer.add_abbrev({
		METADATA_LOCATION,
		{AbbrevOp::Fixed, 1},
		{AbbrevOp::VBR, 6},
		{AbbrevOp::VBR, 8},
		{AbbrevOp::VBR, 6},
		{AbbrevOp::VBR, 6}
	}) : AbbrevID(0);

	// Assume the column is usually under 128, and always output the inlined-at location (it's never more expensive than building an array size 1).
	auto GenericDINodeAbbrev = has & (1 << METADATA_GENERIC_DEBUG) ? writer.add_abbrev({
		METADATA_GENERIC_DEBUG,
		{AbbrevOp::Fixed, 1},
		{AbbrevOp::VBR, 6},
		{AbbrevOp::Fixed, 1},
		{AbbrevOp::VBR, 6},
		{AbbrevOp::Array_VBR, 6}
	}) : AbbrevID(0);

	dynamic_array<uint64>	v;

	for (auto m : metadata) {
		if (m->type) {
			writer.write_record(METADATA_VALUE, {getTypeID(m->type), getID(m->value)});

		} else if (m->value.is<string>()) {
			v.append(m->value.get_known<string>());
			writer.write_record(METADATA_STRING_OLD, v, MDSAbbrev);
			//writer.write_string(METADATA_STRING_OLD, m.value.get_known<string>(), MDSAbbrev);
			v.clear();

		} else if (m->value.is<ValueArray>()) {
			v = transformc(make_const(m->value.get_known<ValueArray>()), [&](const Value &v) { return getMetadataOrNullID(v.get<const Metadata*>().or_default()); });
			writer.write_record(m->distinct ? METADATA_DISTINCT_NODE : METADATA_NODE, v);
			v.clear();

		} else if (m->value.is<const DebugInfo*>()) {
			auto	di		= m->value.get_known<const DebugInfo*>();
			auto	abbrev	= di->kind == METADATA_LOCATION ? DILocationAbbrev : AbbrevID(0);
			v.push_back(m->distinct);
			process<void>(di, [w = RecordWriter(*this, v)](const auto *m) mutable { m->write(w); });
			writer.write_record(di->kind, v, abbrev);
			v.clear();

		} else {
			ISO_ASSERT(0);
		}
	}

	for (auto &i : meta_kinds) {
		v.push_back(meta_kinds.index_of(i));
		add_string(v, i);
		writer.write_record(METADATA_KIND, v);
		v.clear();
	}

	return true;
}

// helper struct for writing ops

struct Rel {// relative to Inst
	const Value &val;
	Rel(const Value &val) : val(val) {}
};
struct RelS {// relative to Inst, but likely to be -ve
	const Value &val;
	RelS(const Value &val) : val(val) {}
};
struct WithType {// relative to Inst, plus type if forward ref
	const Value &val;
	WithType(const Value &val) : val(val) {}
};

class OpWriter : public RecordWriter {
	const Function	&f;
	uint32			offset;

	uint32	getID(const Value& val) {
		if (val.is<const Metadata*>())
			return w.getMetadataID(val.get_known<const Metadata*>());
		return w.getID(val);
	}

public:
	using RecordWriter::custom_write;
	
	OpWriter(ModuleWriter& w, const Function &f, dynamic_array<uint64> &v, uint32 offset) : RecordWriter(w, v), f(f), offset(offset) {}

	template<typename...T>	bool write(T&&...x)	{
		return write_early(*this, x...);
	}

	bool putSymbolAndType(const Value &val) {
		uint32 id = getID(val);
		_put(offset - id);
		if (id >= offset) {
			write(val.GetType());
			return true;
		}
		return false;
	}

	bool	custom_write(const Value &val) {
		if (val.is<const Block*>())
			return _put(f.blocks.index_of(val.get_known<const Block*>()));
		return _put(getID(val));
	}
	bool	custom_write(const Rel &rel) {
		return _put(offset - getID(rel.val));
	}
	bool	custom_write(const RelS &rel) {
		return _put(encode_svbr(((int)offset - (int)getID(rel.val))));
	}
	bool	custom_write(const WithType &typed) {
		putSymbolAndType(typed.val);
		return true;
	}
};

bool ModuleWriter::write_function(BlockWriter&& writer, const Function &f, const BlockInfoBlock &info) {
	// Emit the number of basic blocks, so the reader can create them ahead of time
	writer.write_record(FUNC_CODE_DECLAREBLOCKS, {f.blocks.size()});
	
	uint32	prev_values	= values.size();
	uint32	prev_meta	= metadata.size();

	for (auto &i : f.args)
		add_value(&i);

	// gather constants and any metadata args
	for (auto i : f.instructions) {
		for (auto &a : i->args)
			collect(a, true);
	}

	if (auto consts = get_all<const Constant*>(prev_values + f.args.size()))
		write_consts(writer.enterBlock(CONSTANTS_BLOCK_ID, 4, info[CONSTANTS_BLOCK_ID]), consts, false);

	uint32	offset	= values.size();

	for (auto i : f.instructions) {
		if (i->type && i->type->type != Type::Void)
			add_value(i.i);
	}

	if (metadata.size() > prev_meta) {
		auto	writer2 = writer.enterBlock(METADATA_BLOCK_ID, 3, info[METADATA_BLOCK_ID]);
		for (auto &i : metadata.slice(prev_meta))
			writer2.write_record(METADATA_VALUE, {getTypeID(i->type), getID(i->value)});
	}

	const DILocation *last_loc	= nullptr;
	bool	has_meta_attachments = !!f.attachedMeta;
	bool	has_symbols			= false;

	dynamic_array<uint64>	v;
	for (const Instruction *inst : f.instructions) {
		OpWriter		op(*this, f, v, offset);
		FunctionCodes	code	= FunctionCodes(0);
		AbbrevID		abbrev	= AbbrevID(0);

		switch (inst->op) {
			default:
				if (is_cast(inst->op)) {
					code = FUNC_CODE_INST_CAST;
					if (!op.putSymbolAndType(inst->args[0]))
						abbrev = abbr.FUNCTION_INST_CAST;
					op.write(inst->type, CastOperation(inst->op));

				} else if (is_binary(inst->op)) {
					code = FUNC_CODE_INST_BINOP;
					if (!op.putSymbolAndType(inst->args[0]))
						abbrev = abbr.FUNCTION_INST_BINOP;
					op.write(Rel(inst->args[1]), BinaryOperation(inst->op));

					if (inst->optimisation) {
						if (abbrev == abbr.FUNCTION_INST_BINOP)
							abbrev = abbr.FUNCTION_INST_BINOP_FLAGS;
						op.write(inst->optimisation);
					}

				} else {
					ISO_ASSERT(0);
				}
				break;

			case Operation::GetElementPtr:
				code	= FUNC_CODE_INST_GEP;
				abbrev	= abbr.FUNCTION_INST_GEP;
				op.write(inst->InBounds, inst->args[0].GetType()->subtype);
				op.write(transformc(inst->args, [](const Value &a) { return WithType(a); }));
				break;

			case Operation::ExtractValue:
				code	= FUNC_CODE_INST_EXTRACTVAL;
				op.write(WithType(inst->args[0]));
				op.write(transformc(inst->args.slice(1), [](const Value &a) { return a.get_known<uint64>(); }));
				break;

			case Operation::InsertValue:
				code	= FUNC_CODE_INST_INSERTVAL;
				op.write(WithType(inst->args[0]), WithType(inst->args[1]));
				op.write(transformc(inst->args.slice(1), [](const Value &a) { return a.get_known<uint64>(); }));
				break;

			case Operation::Select:
				code	= FUNC_CODE_INST_VSELECT;
				op.write(WithType(inst->args[0]), WithType(inst->args[1]), Rel(inst->args[2]));
				break;

			case Operation::ExtractElement:
				code	= FUNC_CODE_INST_EXTRACTELT;
				op.write(WithType(inst->args[0]), WithType(inst->args[1]));
				break;

			case Operation::InsertElement:
				code	= FUNC_CODE_INST_INSERTELT;
				op.write(WithType(inst->args[0]), Rel(inst->args[1]), WithType(inst->args[2]));
				break;

			case Operation::ShuffleVector:
				code	= FUNC_CODE_INST_SHUFFLEVEC;
				op.write(WithType(inst->args[0]), Rel(inst->args[1]), Rel(inst->args[2]));
				break;
			
			case Operation::ICmp:
			case Operation::FCmp:
				code	= FUNC_CODE_INST_CMP2;
				op.write(WithType(inst->args[0]), Rel(inst->args[1]), inst->predicate | (inst->op == Operation::ICmp ? 32u : 0u));
				if (inst->optimisation)
					op.write(inst->optimisation);
				break;

			case Operation::Ret:
				code	= FUNC_CODE_INST_RET;
				switch (inst->args.size()) {
					case 0:
						abbrev = abbr.FUNCTION_INST_RET_VOID;
						break;
					case 1:
						if (!op.putSymbolAndType(inst->args[0]))
							abbrev = abbr.FUNCTION_INST_RET_VAL;
						break;
					default:
						op.write(transformc(inst->args, [](const Value &a) { return WithType(a); }));
						break;
				}
				break;

			case Operation::Br:
				code	= FUNC_CODE_INST_BR;
				op.write(inst->args[0]);
				if (inst->args.size() > 1)
					op.write(inst->args[1], Rel(inst->args[2]));
				break;
			
			case Operation::Switch:
				code	= FUNC_CODE_INST_SWITCH;
				op.write(inst->type, Rel(inst->args[0]), inst->args.slice(1));
				break;

			#if 0
			case Operation::IndirectBr:
				code = FUNC_CODE_INST_INDIRECTBR;
				// Encode the address operand as relative, but not the basic blocks.
				op.write(inst->args[0]->get_type(), Rel(inst->args[0]));
				for (auto &i : inst->args.slice(1))
					op.write(i);
				break;

			case Operation::Invoke: {
				const InvokeInst*	II	 = cast<InvokeInst>(inst);
				const Value*		Callee = II->getCalledValue();
				FunctionType*		FTy	 = II->getFunctionType();
				code					 = FUNC_CODE_INST_INVOKE;

				op.write(II->getAttributes(), II->getCallingConv() | 1 << 13, II->getNormalDest(), II->getUnwindDest(), FTy, WithType(Callee));

				for (unsigned inst = 0, e = FTy->getNumParams(); inst != e; ++inst)
					op.write(Rel(inst->args[inst]));	// fixed param.

				// Emit type/value pairs for varargs params
				if (FTy->isVarArg()) {
					for (unsigned inst = FTy->getNumParams(), e = inst->getNumOperands() - 3; inst != e; ++inst)
						op.write(WithType(inst->args[inst]));	// vararg
				}
				break;
			}
			case Operation::Resume:
				code		= FUNC_CODE_INST_RESUME;
				op.write(WithType(inst->args[0]));
				break;

			case Operation::LandingPad: {
				const LandingPadInst& LP = cast<LandingPadInst>(inst);
				code					 = FUNC_CODE_INST_LANDINGPAD;
				op.write(LP.get_type(), LP.isCleanup(), LP.getNumClauses());
				for (unsigned inst = 0, E = LP.getNumClauses(); inst != E; ++inst) {
					if (LP.isCatch(inst))
						v.push_back(LandingPadInst::Catch);
					else
						v.push_back(LandingPadInst::Filter);
					op.write(WithType(LP.getClause(inst)));
				}
				break;
			}

			case Operation::VAArg:
				code = FUNC_CODE_INST_VAARG;
				op.write(inst->args[0]->get_type(), Rel(inst->args[0]), inst->type);
				break;

			#endif
			
			case Operation::Unreachable:
				code	= FUNC_CODE_INST_UNREACHABLE;
				abbrev	= abbr.FUNCTION_INST_UNREACHABLE;
				break;

			case Operation::Phi:
				code	= FUNC_CODE_INST_PHI;
				op.write(inst->type);
				for (auto* p = inst->args.begin(); p != inst->args.end(); p += 2)
					op.write(RelS(p[0]), p[1]);
				break;

			case Operation::Alloca:
				code	= FUNC_CODE_INST_ALLOCA;
				op.write(inst->type->subtype, inst->args[0].GetType(), inst->args[0], (log2(inst->align) + 1) | (inst->alloca_argument << 5) | (1 << 6));
				break;

			case Operation::Load:
				code	= FUNC_CODE_INST_LOAD;
				if (!op.putSymbolAndType(inst->args[0]))	// ptr
					abbrev = abbr.FUNCTION_INST_LOAD;

				op.write(inst->type, log2(inst->align) + 1, inst->Volatile);
				break;

			case Operation::Store:
				code	= FUNC_CODE_INST_STORE;
				op.write(WithType(inst->args[0]), WithType(inst->args[1]), log2(inst->align) + 1, inst->Volatile);
				break;

			case Operation::LoadAtomic:
				code	= FUNC_CODE_INST_LOADATOMIC;
				op.write(WithType(inst->args[0]), inst->type, log2(inst->align) + 1, inst->Volatile, inst->success, inst->SyncScope);
				break;

			case Operation::StoreAtomic:
				code	= FUNC_CODE_INST_STOREATOMIC;
				op.write(WithType(inst->args[0]), WithType(inst->args[1]), log2(inst->align) + 1, inst->Volatile, inst->success, inst->SyncScope);
				break;

			case Operation::CmpXchg:
				code	= FUNC_CODE_INST_CMPXCHG;
				op.write(WithType(inst->args[0]), WithType(inst->args[1]), Rel(inst->args[2]), inst->Volatile, inst->success, inst->SyncScope, inst->failure, inst->Weak);
				break;

			case Operation::AtomicRMW:
				//code = FUNC_CODE_INST_ATOMICRMW;
				//op.write(WithType(inst->args[0]), WithType(inst->args[1]), inst->rmw, inst->Volatile, inst->success, inst->SyncScope);
				code	= FUNC_CODE_INST_ATOMICRMW_OLD;
				op.write(WithType(inst->args[0]), Rel(inst->args[1]), inst->rmw, inst->Volatile, inst->success, inst->SyncScope);
				break;

			case Operation::Fence:
				code = FUNC_CODE_INST_FENCE;
				op.write(inst->success, inst->SyncScope);
				break;

			case Operation::Call: {
				code	= FUNC_CODE_INST_CALL;
				auto	ftype	= inst->funcCall->type;
				op.write(getAttributeID(inst->paramAttrs), CallMarkersFlags((inst->call_tail & 1) | ((inst->call_tail & 2) ? CALL_MUSTTAIL : 0) | CALL_EXPLICIT_TYPE), ftype, WithType(inst->funcCall));

				auto	p		= inst->args.begin();
				for (auto i : ftype->members) {
					if (i->type == Type::Label)
						op.write(*p);	// can happen with asm labels
					else
						op.write(Rel(*p));
					++p;
				}

				//varargs
				while (p != inst->args.end())
					op.write(WithType(*p++));
				break;
			}
		}

		writer.write_record(code, v, abbrev);
		v.clear();
		
		if (inst->name)
			has_symbols = true;

		if (inst->type && inst->type->type != Type::Void)
			++offset;

		has_meta_attachments = has_meta_attachments || !!inst->attachedMeta;

		if (inst->debug_loc) {
			if (inst->debug_loc == last_loc) {
				writer.write_record(FUNC_CODE_DEBUG_LOC_AGAIN, {});
			} else {
				last_loc = inst->debug_loc;
				writer.write_record(FUNC_CODE_DEBUG_LOC, {
					last_loc->line,
					last_loc->col,
					getMetadataOrNullID(last_loc->scope),
					getMetadataOrNullID(last_loc->inlined_at)
				});
			}
		}
	}

	if (!has_symbols) {
		for (auto &i : f.blocks) {
			if (i.name) {
				has_symbols = true;
				break;
			}
		}
	}

	// names for all the instructions etc
	if (has_symbols) {
		auto	writer2 = writer.enterBlock(VALUE_SYMTAB_BLOCK_ID, 4, info[VALUE_SYMTAB_BLOCK_ID]);
		for (auto i : f.instructions) {
			if (i->name) {
				v.push_back(getID(i.i));
				auto	abbrev = abbr.VST_ENTRY[add_string(v, i->name)];
				writer2.write_record(VST_CODE_ENTRY, v, abbrev);
				v.clear();
			}
		}
		for (auto &i : f.blocks) {
			if (i.name) {
				v.push_back(f.blocks.index_of(i));
				writer2.write_record(VST_CODE_BBENTRY, v, add_string(v, i.name) == 2 ? abbr.VST_BBENTRY_6 : AbbrevID(0));
				v.clear();
			}
		}
	}

	if (has_meta_attachments) {
		auto	writer2 = writer.enterBlock(METADATA_ATTACHMENT_ID, 3);
		if (f.attachedMeta) {
			for (auto &i : f.attachedMeta) {
				v.push_back(meta_kinds.getID(i.kind));
				v.push_back(getMetadataID(i.metadata));
			}
			writer2.write_record(METADATA_ATTACHMENT, v);
			v.clear();
		}

		for (auto &i : f.instructions) {
			if (i->attachedMeta) {
				v.push_back(f.instructions.index_of(i));
				for (auto &j : i->attachedMeta) {
					v.push_back(meta_kinds.getID(j.kind));
					v.push_back(getMetadataID(j.metadata));
				}
				writer2.write_record(METADATA_ATTACHMENT, v);
				v.clear();
			}
		}
	}

	// function-level use-lists
	if (f.uselist) {
		auto	writer2 = writer.enterBlock(USELIST_BLOCK_ID, 3);
		for (auto& i : f.uselist) {
			v = i.shuffle;
			if (i.value.is<const Block*>()) {
				v.push_back(f.blocks.index_of(i.value.get_known<const Block*>()));
				writer2.write_record(USELIST_CODE_BB, v);
			} else {
				v.push_back(getID(i.value));
				writer2.write_record(USELIST_CODE_DEFAULT, v);
			}
		}
	}

	Restore(prev_values, prev_meta);
	return true;
}

bool Module::write(BlockWriter &&writer) const {
	writer.write_record(MODULE_CODE_VERSION, {1});

	ModuleWriter	w;

	for (auto &i : named_meta) {
		for (auto j : i)
			w.collect(j);
	}
	for (auto f : functions)
		w.collect(f);

	BlockInfoBlock	info;
	w.make_abbrevs(info);

	info.write(writer.enterBlock(BLOCKINFO_BLOCK_ID, 2));
	
	dynamic_array<uint64>	v;

	if (w.attr_groups)
		w.write_attrgroups(writer.enterBlock(PARAMATTR_GROUP_BLOCK_ID, 3));

	if (w.attr_sets)
		w.write_attrsets(writer.enterBlock(PARAMATTR_BLOCK_ID, 3));

	w.write_types(writer.enterBlock(TYPE_BLOCK_ID_NEW, 4));

	if (triple)
		writer.write_string(MODULE_CODE_TRIPLE, triple);

	if (datalayout)
		writer.write_string(MODULE_CODE_DATALAYOUT, datalayout);

	if (assembly)
		writer.write_string(MODULE_CODE_ASM, assembly);

	for (auto i : w.sections)
		writer.write_string(MODULE_CODE_SECTIONNAME, i);

	for (auto i : w.gcs)
		writer.write_string(MODULE_CODE_GCNAME, i);

	for (auto i : w.comdats) {
		v.push_back(i->sel);
		v.push_back(i->name.length());
		add_string(v, i->name);
		writer.write_record(MODULE_CODE_GCNAME, v);
		v.clear();
	}

	uint32	offset		= 0;
	auto	globalvars	= w.get_all<const GlobalVar*>(offset);
	offset += globalvars.size();

	auto	functions	= w.get_all<const Function*>(offset);
	offset += functions.size();
	
	auto	aliases		= w.get_all<const Alias*>(offset);
	offset += aliases.size();
	
	auto	consts		= w.get_all<const Constant*>(offset);

	if (globalvars) {
		offset += globalvars.size();

		uint32	max_alignment	= 0;
		uint32	max_global_type = 0;
		for (auto g : globalvars) {
			max_alignment	= max(max_alignment, g->align);
			max_global_type	= max(max_global_type, w.getTypeID(g->type));
		}

		auto	SimpleGVarAbbrev	= writer.add_abbrev({
			MODULE_CODE_GLOBALVAR,
			{AbbrevOp::Fixed, log2_ceil(max_global_type + 1)},
			{AbbrevOp::VBR, 6},		// flags
			{AbbrevOp::VBR, 6},		// initializer
			{AbbrevOp::Fixed, 5},	// linkage
			{AbbrevOp::Fixed, log2_ceil(log2(max_alignment) + 2)},
			{AbbrevOp::Fixed, log2_ceil(w.sections.size() + 1)}
		});

		for (auto g : globalvars) {
			// GLOBALVAR: [type, flags, initid, linkage, alignment, section, visibility, threadlocal, unnamed_addr, externally_initialized, dllstorageclass, comdat]
			uint64	v[] = {
				w.getTypeID(g->type->subtype),
				(g->type->addrSpace << 2) | 2 | g->is_const,
				w.getIDorNull(g->value),
				(uint64)g->linkage,
				log2(g->align) + 1,
				w.sections.getIDorNull(g->section),
				(uint64)g->visibility,
				(uint64)g->threadlocal,
				(uint64)g->unnamed_addr,
				(uint64)g->external_init,
				(uint64)g->dll_storage,
				0	//g->hasComdat() ? getComdatID(g->getComdat()) : 0);
			};

			auto	abbrev = SimpleGVarAbbrev;
			for (auto &i : slice(v, 6)) {
				if (i)
					abbrev = AbbrevID(0);
			}

			writer.write_record(MODULE_CODE_GLOBALVAR, v, abbrev);
		}
	}
	for (auto f : functions) {
		++offset;
		// FUNCTION: [type, callingconv, isproto, linkage, paramattrs, alignment, section, visibility, gc, unnamed_addr, prologuedata, dllstorageclass, comdat, prefixdata, personalityfn]
		writer.write_record(MODULE_CODE_FUNCTION, {
			w.getTypeID(f->type),
			(uint64)f->calling_conv,
			f->prototype,
			(uint64)f->linkage,
			w.getAttributeID(f->attrs),
			log2(f->align) + 1,
			w.sections.getIDorNull(f->section),
			(uint64)f->visibility,
			w.gcs.getIDorNull(f->gc),
			(uint64)f->unnamed_addr,
			w.getIDorNull(f->prolog_data),
			(uint64)f->dll_storage,
			0,	//f->hasComdat()			? getComdatID(f->getComdat()) : 0,
			w.getIDorNull(f->prefix_data),
			w.getIDorNull(f->personality_function)
		});
	}

	for (auto a : aliases) {
		++offset;
		// ALIAS: [alias type, aliasee val#, linkage, visibility]
		writer.write_record(MODULE_CODE_ALIAS, {
			w.getTypeID(a->type),
			w.getID(a->value),
			(uint64)a->linkage,
			(uint64)a->visibility,
			(uint64)a->dll_storage,
			(uint64)a->threadlocal,
			(uint64)a->unnamed_addr
		});
	}

	if (consts)
		w.write_consts(writer.enterBlock(CONSTANTS_BLOCK_ID, 4, info[CONSTANTS_BLOCK_ID]), consts, true);

	if (named_meta || w.metadata) {
		auto	writer2 = writer.enterBlock(METADATA_BLOCK_ID, 3, info[METADATA_BLOCK_ID]);
		w.write_metadata(writer2);
		if (named_meta) {
			auto NameAbbrev = writer2.add_abbrev({
				METADATA_NAME,
				{AbbrevOp::Array_Fixed, 8}
			});

			for (auto &i : named_meta) {
				add_string(v, i.name);
				writer2.write_record(METADATA_NAME, v, NameAbbrev);
				v = transformc(make_const(i), [&](const Metadata* m) { return w.getMetadataID(m); });
				writer2.write_record(METADATA_NAMED_NODE, v);
				v.clear();
			}

		}
	}

	if (w.has_symbols(0)) {
		auto	writer2 = writer.enterBlock(VALUE_SYMTAB_BLOCK_ID, 4, info[VALUE_SYMTAB_BLOCK_ID]);
		for (auto &i : w.values) {
			if (auto *s = i.GetName(false)) {
				if (*s) {
					v.push_back(w.values.index_of(i));
					auto	abbrev = w.abbr.VST_ENTRY[add_string(v, *s)];
					writer2.write_record(VST_CODE_ENTRY, v, abbrev);
					v.clear();
				}
			}
		}
	}

	// module-level use-lists
	if (uselist) {
		auto	writer2 = writer.enterBlock(USELIST_BLOCK_ID, 3);
		for (auto& i : uselist) {
			v = i.shuffle;
			v.push_back(w.getID(i.value));
			writer2.write_record(USELIST_CODE_DEFAULT, v);
			v.clear();
		}
	}

	// function bodies
	dynamic_array<const Function*> functions2 = functions;//make copy
	for (auto &f : functions2) {
		if (!f->prototype)
			w.write_function(writer.enterBlock(FUNCTION_BLOCK_ID, 4, info[FUNCTION_BLOCK_ID]), *f, info);
	}

	return true;
}

//-----------------------------------------------------------------------------
//	DI
//-----------------------------------------------------------------------------

const char* DebugInfo::get_name() const {
	struct proc {
		const char *operator()(const DITag *p)			{ return *p->name; }
		const char *operator()(const DIExternal *p)		{ return *p->name; }
		const char *operator()(const DILexicalBlock *p) { return process<const char*>(p->scope, *this); }
		const char *operator()(const DILocation *p)		{ return process<const char*>(p->scope, *this); }
		const char *operator()(const void *p)			{ return nullptr; }
	};
	return process<const char*>(this, proc());
}

const DIFile* DebugInfo::get_file() const {
	struct proc {
		const DIFile* operator()(const DILocation *p)	{ return process<const DIFile*>(p->scope, *this); }
		const DIFile* operator()(const DIFile *p)		{ return p; }
		const DIFile* operator()(const DIScoped *p)		{ return process<const DIFile*>(p->file, *this); }
		const DIFile* operator()(const void *p)			{ return nullptr; }
	};
	return process<const DIFile*>(this, proc());
}

const DISubprogram* DebugInfo::get_subprogram() const {
	struct proc {
		const DISubprogram* operator()(const DILocation *p)		{ return process<const DISubprogram*>(p->scope, *this); }
		const DISubprogram* operator()(const DIScoped *p)		{ return process<const DISubprogram*>(p->scope, *this); }
		const DISubprogram* operator()(const DISubprogram *p)	{ return p; }
		const DISubprogram* operator()(const void *p)			{ return nullptr; }
	};
	return process<const DISubprogram*>(this, proc());
}

filename DILocation::get_filename()	const {
	const DIFile *p	= get_file();
	filename	fn(*p->file);
	return fn.convert_to_backslash();
}

bool DILocation::IsInScope(const DebugInfo *scopeb) const {
	struct scope_process {
		auto operator()(const DILocation *p)	{ return p->scope; }
		auto operator()(const DIScoped *p)		{ return p->scope; }
		auto operator()(const void *p)			{ return nullptr; }
	};

	struct inline_process {
		auto operator()(const DILocation *p)	{ return p->inlined_at; }
		auto operator()(const void *p)			{ return nullptr; }
	};

	// a is variable
	// b is current scope

	if (const DebugInfo* a = scope) {
		auto b = scopeb;
		while (b) {
			if (a == b)
				return true;
			b = process<const DebugInfo*>(b, scope_process());
		}

		b = scopeb;
		while (b && !b->as<DILocation>())
			b = process<const DebugInfo*>(b, scope_process());

		for (auto loc = b->as<DILocation>(); loc; loc = (const DILocation*)loc->inlined_at) {
			if (a == loc->scope)
				return true;
		}
	}

	return false;
}

#if 1

uint64 DIExpression::Evaluate(uint64 v, uint32 &bit_size) const {
	for (auto p = expr.begin(); p != expr.end();) {
		switch (auto op = (dwarf::OP)*p++) {
			case dwarf::OP_deref:
				v = *(uint64*)v;
				break;

			case dwarf::OP_plus:
				v += *p++;
				break;

			case dwarf::OP_bit_piece:
				v			+= *p++ / 8;
				bit_size	= *p++;
				break;
		}
	}
	return v;
}

#else

void DIExpression::Evaluate(uint64 v) const {
	uint64	stack[64], *sp = stack;

	for (auto p = expr.begin(); p != expr.end();) {
		switch (auto op = (dwarf::OP)*p++) {
			case dward::OP_bit_piece:
				v			+= *p++ / 8;
				bit_size	= *p++;
				break;

			case dwarf::OP_deref:			//dereferences the top of the expression stack.
				v = *(uint64*)v;
				break;

			case dwarf::OP_plus:
				v += *p++;
				break;

			case dwarf::OP_minus:
				v -= *p++;
				break;

			case dwarf::OP_plus_uconst:
				v += *p++;
				break;

			case dwarf::OP_swap:
				swap(sp[-1], v);
				break;

			case dwarf::OP_xderef:			//provides extended dereference mechanism. The entry at the top of the stack is treated as an address. The second stack entry is treated as an address space identifier.
			case dwarf::OP_stack_value:		//marks a constant value.

			case dwarf::OP_LLVM_fragment:	//, 16, 8 specifies the offset and size (16 and 8 here, respectively) of the variable fragment from the working expression. Note that contrary to DW_OP_bit_piece, the offset is describing the location within the described source variable.
			case dwarf::OP_LLVM_convert:	//, 16, DW_ATE_signed specifies a bit size and encoding (16 and DW_ATE_signed here, respectively) to which the top of the expression stack is to be converted. Maps into a DW_OP_convert operation that references a base type constructed from the supplied values.
			case dwarf::OP_LLVM_tag_offset:	//, tag_offset specifies that a memory tag should be optionally applied to the pointer. The memory tag is derived from the given tag offset in an implementation-defined manner.
			case dwarf::OP_LLVM_entry_value:
				//, N may only appear in MIR and at the beginning of a DIExpression. In DWARF a DBG_VALUE instruction binding a DIExpression(DW_OP_LLVM_entry_value to a register is lowered to a DW_OP_entry_value [reg], pushing the value the register had upon function entry onto the stack. The next (N - 1) operations will be part of the DW_OP_entry_value block argument. For example, !DIExpression(DW_OP_LLVM_entry_value, 1, DW_OP_plus_uconst, 123, DW_OP_stack_value) specifies an expression where the entry value of the debug value instruction’s value/address operand is pushed to the stack, and is added with 123. Due to framework limitations N can currently only be 1.
				//The operation is introduced by the LiveDebugValues pass, which applies it only to function parameters that are unmodified throughout the function. Support is limited to simple register location descriptions, or as indirect locations (e.g., when a struct is passed-by-value to a callee via a pointer to a temporary copy made in the caller). The entry value op is also introduced by the AsmPrinter pass when a call site parameter value (DW_AT_call_site_parameter_value) is represented as entry value of the parameter.

			case dwarf::OP_LLVM_arg:		//, N is used in debug intrinsics that refer to more than one value, such as one that calculates the sum of two registers. This is always used in combination with an ordered list of values, such that DW_OP_LLVM_arg, N refers to the N``th element in that list. For example, ``!DIExpression(DW_OP_LLVM_arg, 0, DW_OP_LLVM_arg, 1, DW_OP_minus, DW_OP_stack_value) used with the list (%reg1, %reg2) would evaluate to %reg1 - reg2. This list of values should be provided by the containing intrinsic/instruction.
			case dwarf::OP_LLVM_implicit_pointer:	//It specifies the dereferenced value. It can be used to represent pointer variables which are optimized out but the value it points to is known. This operator is required as it is different than DWARF operator DW_OP_implicit_pointer in representation and specification (number and types of operands) and later can not be used as multiple level.
				break;

			case dwarf::OP_breg0:
			case dwarf::OP_bregx:
				//represents a content on the provided signed offset of the specified register. The opcode is only generated by the AsmPrinter pass to describe call site parameter value which requires an expression over two registers.
				break;

			case dwarf::OP_push_object_address:	//pushes the address of the object which can then serve as a descriptor in subsequent calculation. This opcode can be used to calculate bounds of fortran allocatable array which has array descriptors.
				break;

			case dwarf::OP_over:			//duplicates the entry currently second in the stack at the top of the stack. This opcode can be used to calculate bounds of fortran assumed rank array which has rank known at run time and current dimension number is implicitly first element of the stack.
				*sp = sp[-2];
				++sp;
				break;
		}
	}
}
#endif

//-----------------------------------------------------------------------------
//	Filehandler
//-----------------------------------------------------------------------------

#include "iso/iso_files.h"

void DumpBitcode(bitcode::BlockReader &&br, BlockInfoBlock &infos, int indent) {
	uint32	code;
	while (auto id = br.nextRecord(code)) {
		ISO_TRACEF() << repeat('\t', indent) << "code = " << code << '\n';

		if (id == bitcode::ENTER_SUBBLOCK) {
			switch (code) {
				case bitcode::BLOCKINFO_BLOCK_ID:
					infos.read(br.enterBlock(nullptr));
					break;

				default:
					DumpBitcode(br.enterBlock(infos[code]), infos, indent + 1);
					break;
			}

		} else {
			br.skipRecord(id);
		}
	}
}

class BitcodeFileHandler : public FileHandler {
	const char*		GetDescription() override { return "Bitcode"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		uint32	magic = file.get<uint32>();

		ISO_ptr<anything>	p(id);
		vlc					v(file);

		BlockInfoBlock		infos;
		DumpBitcode(BlockReader(v), infos, 0);

		return p;
	}
} bitcode_reader;

#include "dx/dx_shaders.h"

struct tester {
	bitcode::Module mod;
	bitcode::Module mod2;
	tester() {
		auto	type = Type::make<float>();
		Typed	val1(&type, type.set(1.2f));
		Typed	val2(&type, type.set(2.3f));

		float	r = Typed(&type, Evaluate(&type, Operation::FAdd, 0, make_range({val1, val2})));


		//mod.read(FileInput("D:\\test.bin"));
		//mod.write(FileOutput("D:\\test2.bin"));
		//mod2.read(FileInput("D:\\test2.bin"));
	}
} tester;
