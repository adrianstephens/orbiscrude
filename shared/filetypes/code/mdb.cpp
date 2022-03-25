#include "iso/iso_files.h"
#include "dwarf.h"
#include "base/algorithm.h"
#include "hashes/md5.h"

//-----------------------------------------------------------------------------
//	Mono symbol file
//-----------------------------------------------------------------------------

using namespace iso;
using namespace dwarf;

void write_string(ostream_ref file, const string &s) {
	char	temp[8];
	uint32	len	= s.size32();
	file.writebuff(temp, put_char(len, temp));
	file.writebuff(s, len);
}

string read_string(istream_ref file) {
	char	temp[8];
	temp[0]		= file.getc();
	uint32	len	= char_length(temp[0]);
	file.readbuff(temp, len - 1);
	char32	c;
	get_char(c, temp);
	string	s(c);
	file.readbuff(s, c);
	return s;
}


struct LineNumberEntry {
	// This is actually written to the symbol file
	const int	Row;
	int			Column;
	int			EndRow, EndColumn;
	const int	File;
	const int	Offset;
	const bool	IsHidden;	// Obsolete is never used

	LineNumberEntry() : File(0), Row(0), Column(0), EndRow(-1), EndColumn(-1), Offset(0), IsHidden(false) {}
	LineNumberEntry(int file, int row, int column, int offset) : File(file), Row(row), Column(column), EndRow(-1), EndColumn(-1), Offset(offset), IsHidden(false) {}
	LineNumberEntry(int file, int row, int offset) : File(file), Row(row), Column(-1), EndRow(-1), EndColumn(-1), Offset(offset), IsHidden(false) {}
	LineNumberEntry(int file, int row, int column, int offset, bool is_hidden) : File(file), Row(row), Column(column), EndRow(-1), EndColumn(-1), Offset(offset), IsHidden(false) {}
	LineNumberEntry(int file, int row, int column, int end_row, int end_column, int offset, bool is_hidden) : File(file), Row(row), Column(column), EndRow(end_row), EndColumn(end_column), Offset(offset), IsHidden(false) {}
};

struct LineNumberTableOpts {
	int		LineBase;
	int		LineRange;
	uint8	OpcodeBase;
};

struct LineNumberTable : LineNumberTableOpts {
	dynamic_array<LineNumberEntry> line_numbers;

	int		MaxAddressIncrement;

	enum {
		DW_LNS_copy			= 1,
		DW_LNS_advance_pc	= 2,
		DW_LNS_advance_line	= 3,
		DW_LNS_set_file		= 4,
		DW_LNS_const_add_pc	= 8,
		DW_LNE_end_sequence	= 1,

		// MONO extensions.
		DW_LNE_MONO_negate_is_hidden	= 0x40,
		DW_LNE_MONO__extensions_start	= 0x40,
		DW_LNE_MONO__extensions_end		= 0x7f,
	};

	LineNumberTable(const LineNumberTableOpts &opts) : LineNumberTableOpts(opts) {
		MaxAddressIncrement = (255 - OpcodeBase) / LineRange;
	}

	void write(ostream_ref file, bool hasColumnsInfo, bool hasEndInfo) {
		int start = file.tell32();

		bool last_is_hidden = false;
		int last_line = 1, last_offset = 0, last_file = 1;

		for (auto &i : line_numbers) {
			int line_inc = i.Row - last_line;
			int offset_inc = i.Offset - last_offset;

			if (i.File != last_file) {
				file.putc(DW_LNS_set_file);
				write_leb128(file, i.File);
				last_file = i.File;
			}

			if (i.IsHidden != last_is_hidden) {
				file.putc(0);
				file.putc(1);
				file.putc(DW_LNE_MONO_negate_is_hidden);
				last_is_hidden = i.IsHidden;
			}

			if (offset_inc >= MaxAddressIncrement) {
				if (offset_inc < 2 * MaxAddressIncrement) {
					file.putc(DW_LNS_const_add_pc);
					offset_inc -= MaxAddressIncrement;
				} else {
					file.putc(DW_LNS_advance_pc);
					write_leb128(file, offset_inc);
					offset_inc = 0;
				}
			}

			if ((line_inc < LineBase) || (line_inc >= LineBase + LineRange)) {
				file.putc(DW_LNS_advance_line);
				write_leb128(file, line_inc);
				if (offset_inc != 0) {
					file.putc(DW_LNS_advance_pc);
					write_leb128(file, offset_inc);
				}
				file.putc(DW_LNS_copy);
			} else {
				file.putc(line_inc - LineBase + (LineRange * offset_inc) + OpcodeBase);
			}

			last_line = i.Row;
			last_offset = i.Offset;
		}

		file.putc(0);
		file.putc(1);
		file.putc(DW_LNE_end_sequence);

		if (hasColumnsInfo) {
			for (auto &i : line_numbers) {
				if (i.Row >= 0)
					write_leb128(file, i.Column);
			}
		}

		if (hasEndInfo) {
			for (auto &i : line_numbers) {
				if (i.EndRow == -1 || i.EndColumn == -1 || i.Row > i.EndRow) {
					write_leb128(file, 0xffffff);
				} else {
					write_leb128(file, i.EndRow - i.Row);
					write_leb128(file, i.EndColumn);
				}
			}
		}
	}

	bool read(istream_ref file, bool includesColumns, bool includesEnds) {
		line_numbers.clear();

		bool	is_hidden = false, modified = false;
		int		stm_line = 1, stm_offset = 0, stm_file = 1;
		for (;;) {
			uint8 opcode = file.getc();

			if (opcode == 0) {
				uint8		size = file.getc();
				streamptr	end_pos = file.tell() + size;
				opcode = file.getc();

				if (opcode == DW_LNE_end_sequence) {
					if (modified)
						new(line_numbers) LineNumberEntry (stm_file, stm_line, -1, stm_offset, is_hidden);
					break;

				} else if (opcode == DW_LNE_MONO_negate_is_hidden) {
					is_hidden = !is_hidden;
					modified = true;

				} else if (between(opcode, DW_LNE_MONO__extensions_start, DW_LNE_MONO__extensions_end)) {
					; // reserved for future extensions
				} else {
					return false;
				}

				file.seek(end_pos);
				continue;

			} else if (opcode < OpcodeBase) {
				switch (opcode) {
				case DW_LNS_copy:
					new(line_numbers) LineNumberEntry (stm_file, stm_line, -1, stm_offset, is_hidden);
					modified = false;
					break;
				case DW_LNS_advance_pc:
					stm_offset += get_leb128<uint32>(file);
					modified = true;
					break;
				case DW_LNS_advance_line:
					stm_line += get_leb128<uint32>(file);
					modified = true;
					break;
				case DW_LNS_set_file:
					stm_file = get_leb128<uint32>(file);
					modified = true;
					break;
				case DW_LNS_const_add_pc:
					stm_offset += MaxAddressIncrement;
					modified = true;
					break;
				default:
					return false;
				}
			} else {
				opcode -= OpcodeBase;

				stm_offset += opcode / LineRange;
				stm_line += LineBase + (opcode % LineRange);
				new(line_numbers) LineNumberEntry (stm_file, stm_line, -1, stm_offset, is_hidden);
				modified = false;
			}
		}

		if (includesColumns) {
			for (auto &i : line_numbers) {
				if (i.Row >= 0)
					i.Column = get_leb128<uint32>(file);
			}
		}
		if (includesEnds) {
			for (auto &i : line_numbers) {
				int row = get_leb128<uint32>(file);
				if (row == 0xffffff) {
					i.EndRow = -1;
					i.EndColumn = -1;
				} else {
					i.EndRow = i.Row + row;
					i.EndColumn = get_leb128<uint32>(file);
				}
			}
		}
		return true;
	}
};

struct CodeBlockEntry {
	enum Type {
		Lexical				= 1,
		CompilerGenerated	= 2,
		IteratorBody		= 3,
		IteratorDispatcher	= 4
	};

	int		Index;

	// This is actually written to the symbol file
	int		Parent;
	Type	BlockType;
	int		StartOffset;
	int		EndOffset;


//	CodeBlockEntry (int index, int parent, Type type, int start_offset) : Index(index), Parent(parent), BlockType(type), StartOffset(start_offset) {}

	void read(istream_ref file) {
		int type_flag = get_leb128<int>(file);
		BlockType	= Type(type_flag & 0x3f);
		Parent		= get_leb128<int>(file);
		StartOffset = get_leb128<int>(file);
		EndOffset	= get_leb128<int>(file);

		// Reserved for future extensions
		if (type_flag & 0x40)
			file.seek_cur(file.get<uint16>());
	}

	void Close (int end_offset) {
		EndOffset = end_offset;
	}

	void write(ostream_ref file) {
		write_leb128(file,(int) BlockType);
		write_leb128(file,Parent);
		write_leb128(file,StartOffset);
		write_leb128(file,EndOffset);
	}
};

struct LocalVariableEntry {
	// This is actually written to the symbol file
	int		Index;
	string	Name;
	int		BlockIndex;

//	LocalVariableEntry(int index, string name, int block) : Index(index), Name(name), BlockIndex(block) {}

	void read(istream_ref file) {
		Index		= get_leb128<int>(file);
		Name		= read_string(file);
		BlockIndex	= get_leb128<int>(file);
	}

	void write(ostream_ref file) {
		write_leb128(file, Index);
		write_string(file, Name);
		write_leb128(file, BlockIndex);
	}
};

struct ScopeVariable {
	// This is actually written to the symbol file
	int Scope;
	int Index;

//	ScopeVariable (int scope, int index) : Scope(scope), Index(index) {}

	void read(istream_ref file) {
		Scope = get_leb128<int>(file);
		Index = get_leb128<int>(file);
	}
	void write(ostream_ref file) {
		write_leb128(file,Scope);
		write_leb128(file,Index);
	}
};

struct MethodEntry {
	struct Data {
		uint32	Token;
		uint32	DataOffset;
		uint32	LineNumberTableOffset;
	} data;

	enum Flags {
		LocalNamesAmbiguous	= 1,
		ColumnsInfoIncluded = 1 << 1,
		EndInfoIncluded = 1 << 2
	};

	uint32	NamespaceID;
	uint32	CompileUnitIndex;
	uint32	LocalVariableTableOffset;
	uint32	CodeBlockTableOffset;
	uint32	ScopeVariableTableOffset;
	uint32	RealNameOffset;
	uint32	flags;

	dynamic_array<LocalVariableEntry>	locals;
	dynamic_array<CodeBlockEntry>		code_blocks;
	dynamic_array<ScopeVariable>		scope_vars;
	LineNumberTable						lnt;
	string								real_name;

	MethodEntry(const LineNumberTableOpts &opts) : lnt(opts) {}

	bool operator<(const MethodEntry &b) const {
		return data.Token < b.data.Token;
	}

	void write_data(ostream_ref file) {
		LocalVariableTableOffset = file.tell32();
		write_leb128(file, locals.size());
		for (auto &i : locals)
			i.write(file);

		CodeBlockTableOffset = file.tell32();
		write_leb128(file, code_blocks.size());
		for (auto &i : code_blocks)
			i.write(file);

		ScopeVariableTableOffset = file.tell32();
		write_leb128(file, scope_vars.size());
		for (auto &i : scope_vars)
			i.write(file);

		if (real_name) {
			RealNameOffset = file.tell32();
			write_string(file, real_name);
		}

		for (auto &i : lnt.line_numbers) {
			if (i.EndRow != -1 || i.EndColumn != -1)
				flags |= EndInfoIncluded;
		}

		data.LineNumberTableOffset = file.tell32();
		lnt.write(file, (flags & ColumnsInfoIncluded) != 0, (flags & EndInfoIncluded) != 0);

		data.DataOffset = file.tell32();

		write_leb128(file, CompileUnitIndex);
		write_leb128(file, LocalVariableTableOffset);
		write_leb128(file, NamespaceID);
		write_leb128(file, CodeBlockTableOffset);
		write_leb128(file, ScopeVariableTableOffset);
		write_leb128(file, RealNameOffset);
		write_leb128(file, flags);
	}

	void write(ostream_ref file) {
		file.write(data);
	}

	void read(istream_ref file) {
		file.read(data);

		file.seek(data.DataOffset);
		CompileUnitIndex			= get_leb128<uint32>(file);
		LocalVariableTableOffset	= get_leb128<uint32>(file);
		NamespaceID					= get_leb128<uint32>(file);
		CodeBlockTableOffset		= get_leb128<uint32>(file);
		ScopeVariableTableOffset	= get_leb128<uint32>(file);
		RealNameOffset				= get_leb128<uint32>(file);
		flags						= get_leb128<uint32>(file);


		if (data.LineNumberTableOffset) {
			file.seek(data.LineNumberTableOffset);
			lnt.read(file, (flags & ColumnsInfoIncluded) != 0, (flags & EndInfoIncluded) != 0);
		}

		if (LocalVariableTableOffset) {
			file.seek(LocalVariableTableOffset);
			locals.resize(get_leb128<uint32>(file));
			for (auto &i : locals)
				i.read(file);
		}

		if (CodeBlockTableOffset) {
			file.seek(CodeBlockTableOffset);
			code_blocks.resize(get_leb128<uint32>(file));
			for (auto &i : code_blocks)
				i.read(file);
		}

		if (ScopeVariableTableOffset) {
			file.seek(ScopeVariableTableOffset);
			scope_vars.resize(get_leb128<uint32>(file));
			for (auto &i : scope_vars)
				i.read(file);
		}

		if (RealNameOffset) {
			file.seek(RealNameOffset);
			real_name = read_string(file);
		}
	}
};

struct SourceFileEntry {
	struct Data {
		uint32		Index;
		uint32		DataOffset;
	} data;

	string		filename;
	GUID		guid;
	MD::CODE	hash;
	bool8		auto_generated;

	SourceFileEntry() {
		clear(hash);
	}

	void write_data(ostream_ref file) {
		data.DataOffset = file.tell32();
		write_string(file, filename);

		if (exists(filename)) {
			MD5			md5;
			stream_copy<1024>(md5, lvalue(FileInput(filename)));
			hash		= md5;
		}

		file.write(guid);
		file.write(hash);
		file.putc(auto_generated ? 1 : 0);
	}

	void write(ostream_ref file) {
		file.write(data);
	}

	void read(istream_ref file) {
		file.read(data);

		file.seek(data.DataOffset);
		filename = read_string(file);
		file.read(guid);
		file.read(hash);
		auto_generated = file.getc() == 1;
	}
};

struct NamespaceEntry {
	string	Name;
	int		Index;
	int		Parent;
	dynamic_array<string> UsingClauses;

	void write(ostream_ref file) {
		write_string(file, Name);
		write_leb128(file, Index);
		write_leb128(file, Parent);
		write_leb128(file, UsingClauses.size());
		for (auto &i : UsingClauses)
			write_string(file, i);
	}


	void read(istream_ref file) {
		Name	= read_string(file);
		Index	= get_leb128<int>(file);
		Parent	= get_leb128<int>(file);

		UsingClauses.resize(get_leb128<uint32>(file));
		for (auto &i : UsingClauses)
			i = read_string(file);
	}
};

struct CompileUnitEntry {
	struct Data {
		uint32 Index;
		uint32 DataOffset;
	} data;

	uint32					source;
	dynamic_array<uint32>	include_files;
	dynamic_array<NamespaceEntry>	namespaces;

	void write_data(ostream_ref file) {
		data.DataOffset = file.tell32();
		write_leb128(file, source);

		write_leb128(file, include_files.size());
		for (auto &i : include_files)
			write_leb128(file, i);

		write_leb128(file, namespaces.size());
		for (auto &i : namespaces)
			i.write(file);
	}

	void write(ostream_ref file) {
		file.write(data);
	}

	void read(istream_ref file) {
		file.read(data);

		file.seek(data.DataOffset);

		source = get_leb128<uint32>(file);

		include_files.resize(get_leb128<uint32>(file));
		for (auto &i : include_files)
			i = get_leb128<uint32>(file);

		namespaces.resize(get_leb128<uint32>(file));
		for (auto &i : namespaces)
			i.read(file);
	}
};

struct CapturedVariable {
	enum CapturedKind {
		Local,
		Parameter,
		This
	};
	// This is actually written to the symbol file
	string			Name;
	string			CapturedName;
	CapturedKind	Kind;


	CapturedVariable (string name, string captured_name, CapturedKind kind) : Name(name), CapturedName(captured_name), Kind(kind) {}
#if 0
	CapturedVariable (MyBinaryReader reader)
	{
		Name = reader.ReadString ();
		CapturedName = reader.ReadString ();
		Kind = (CapturedKind) reader.ReadByte ();
	}
#endif
	void write(ostream_ref file) {
		write_string(file, Name);
		write_string(file, CapturedName);
		file.putc(Kind);
	}
};

struct CapturedScope {
	// This is actually written to the symbol file
	int		Scope;
	string	CapturedName;

	CapturedScope(int scope, string captured_name) : Scope(scope), CapturedName(captured_name) {}
#if 0
	CapturedScope (MyBinaryReader reader)
	{
		Scope = reader.ReadLeb128 ();
		CapturedName = reader.ReadString ();
	}
#endif
	void write(ostream_ref file) {
		write_leb128(file,Scope);
		write_string(file, CapturedName);
	}
};

struct AnonymousScopeEntry {
	uint32	ID;

	dynamic_array<CapturedVariable> captured_vars;
	dynamic_array<CapturedScope>	captured_scopes;

	AnonymousScopeEntry (int id) : ID(id) {}

#if 0
	AnonymousScopeEntry (MyBinaryReader reader)
	{
		ID = reader.ReadLeb128 ();

		int num_captured_vars = reader.ReadLeb128 ();
		for (int i = 0; i < num_captured_vars; i++)
			captured_vars.Add (new CapturedVariable (reader));

		int num_captured_scopes = reader.ReadLeb128 ();
		for (int i = 0; i < num_captured_scopes; i++)
			captured_scopes.Add (new CapturedScope (reader));
	}
#endif
	void AddCapturedVariable(string name, string captured_name, CapturedVariable::CapturedKind kind) {
		new (captured_vars) CapturedVariable (name, captured_name, kind);
	}

	void AddCapturedScope (int scope, string captured_name) {
		new (captured_scopes) CapturedScope (scope, captured_name);
	}

	void write(ostream_ref file) {
		write_leb128(file,ID);

		write_leb128(file, captured_vars.size());
		for (auto &i : captured_vars)
			i.write(file);

		write_leb128(file, captured_scopes.size());
		for (auto &i : captured_scopes)
			i.write(file);
	}
};

struct MonoSymbolFile {
	struct OffsetTable {
		uint64	Magic;
		uint32	MajorVersion;
		uint32	MinorVersion;
		GUID	guid;

		uint32	TotalFileSize;
		uint32	DataSectionOffset;
		uint32	DataSectionSize;
		uint32	CompileUnitCount;
		uint32	CompileUnitTableOffset;
		uint32	CompileUnitTableSize;
		uint32	SourceCount;
		uint32	SourceTableOffset;
		uint32	SourceTableSize;
		uint32	MethodCount;
		uint32	MethodTableOffset;
		uint32	MethodTableSize;
		uint32	TypeCount;
		uint32	AnonymousScopeCount;
		uint32	AnonymousScopeTableOffset;
		uint32	AnonymousScopeTableSize;

		LineNumberTableOpts	line_opts;

		uint32 Flags;

		OffsetTable() : Magic(0x45e82623fd7fa614ll), MajorVersion(50), MinorVersion(0) {}
		bool	valid() const {
			return Magic == 0x45e82623fd7fa614ll && MajorVersion == 50;
		}
	};

	dynamic_array<SourceFileEntry>	sources;
	dynamic_array<CompileUnitEntry> comp_units;
	dynamic_array<MethodEntry*>		methods;
	map<int, AnonymousScopeEntry>	anonymous_scopes;
	int		last_type_index;

	void	write(ostream_ref file);
	bool	read(istream_ref file);

};

void MonoSymbolFile::write(ostream_ref file) {
	OffsetTable	ot;
	file.seek(sizeof(ot));

	// Sort the methods according to their tokens and update their index

	sort(methods);

	// Write data sections

	ot.DataSectionOffset = file.tell32();
	for (auto &i : sources)
		i.write_data(file);

	for (auto &i : comp_units)
		i.write_data(file);

	for (auto &i : methods)
		i->write_data(file);

	ot.DataSectionSize = file.tell32() - ot.DataSectionOffset;

	// Write the method index table

	ot.MethodTableOffset = file.tell32();
	for (auto &i : methods)
		i->write(file);

	ot.MethodTableSize = file.tell32() - ot.MethodTableOffset;

	// Write source table

	ot.SourceTableOffset = file.tell32();
	for (auto &i : sources)
		i.write(file);
	ot.SourceTableSize = file.tell32() - ot.SourceTableOffset;

	// Write compilation unit table

	ot.CompileUnitTableOffset = file.tell32();
	for (auto &i : comp_units)
		i.write(file);
	ot.CompileUnitTableSize = file.tell32() - ot.CompileUnitTableOffset;

	// Write anonymous scope table

	ot.AnonymousScopeCount = uint32(anonymous_scopes.size());
	ot.AnonymousScopeTableOffset = file.tell32();
	for (auto &i : anonymous_scopes)
		i.write(file);
	ot.AnonymousScopeTableSize = file.tell32() - ot.AnonymousScopeTableOffset;

	// Fixup offset table

	ot.TypeCount = last_type_index;
	ot.MethodCount = methods.size32();
	ot.SourceCount = sources.size32();
	ot.CompileUnitCount = comp_units.size32();

	// Write offset table

	ot.TotalFileSize = file.tell32();
	file.seek(0);
	file.write(ot);
}

bool MonoSymbolFile::read(istream_ref file) {
	OffsetTable	ot;
	if (!file.read(ot) || !ot.valid())
		return false;

	sources.resize(ot.SourceCount);
	for (int i = 0; i < ot.SourceCount; i++) {
		file.seek(ot.SourceTableOffset + sizeof(SourceFileEntry::Data) * i);
		sources[i].read(file);
	}

	comp_units.resize(ot.CompileUnitCount);
	for (int i = 0; i < ot.CompileUnitCount; i++) {
		file.seek(ot.CompileUnitTableOffset + sizeof(CompileUnitEntry::Data) * i);
		comp_units[i].read(file);
	}


	methods.resize(ot.MethodCount);
	for (int i = 0; i < ot.MethodCount; i++) {
		file.seek(ot.MethodTableOffset + sizeof(MethodEntry::Data) * i);
		methods[i] = new MethodEntry(ot.line_opts);
		methods[i]->read(file);
	}

	return true;
}

class MDBFileHandler : FileHandler {
	const char*		GetExt() override { return "mdb";					}
	const char*		GetDescription() override { return "Mono Symbol file";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		MonoSymbolFile	mdb;
		if (mdb.read(file)) {
			return ISO_NULL;
		}
		return ISO_NULL;
	}
} mdb;

