#include "code/bitcode.h"
#include "disassembler.h"

namespace bitcode {

string_accum& operator<<(string_accum &a, const Type *t);
string_accum& operator<<(string_accum &a, const Block *b);
string_accum& operator<<(string_accum &a, const Instruction *i);

class DisassemblerBitcode : public Disassembler {
//protected:
public:
	struct Context {
		enum FLAGS {
			SKIP_META		= 1 << 0,
			SKIP_TYPES		= 1 << 1,
			SKIP_GLOBALS	= 1 << 2,
			SKIP_DEBUG		= 1 << 3,
		};
		typedef	void custom_t(Context&, string_accum&, bitcode::Instruction&);
		const Module&				mod;
		uint32						flags;
		async_callback<custom_t>	custom;
		ValueArray					numbered_meta;
		hash_map<const AttributeGroup*, uint32>	attr_group_ids;
		hash_map<const void*, uint32>	metadata_ids;

		void	AssignMetaID(const Metadata *m);
		void	AssignMetaID(const DILocation *loc);

		string_accum&	DumpArg(string_accum& a, const Value &v, bool with_types, const string& attr = "");
		string_accum&	DumpMetaVal(string_accum &a, const Metadata *m) const;
		string_accum&	DumpMetaRef(string_accum &a, const Metadata *m) const;

		auto Arg(const Value& v, bool with_types, const string& attr = "") {
			return [=](string_accum& a) { DumpArg(a, v, with_types, attr); };
		}
		auto Args(const range<const Value*> &args, bool with_types) {
			return transformc(args, [this, with_types](const Value& v) { return Arg(v, with_types); });
		}

		void	Disassemble(string_accum& a, Instruction& inst);
		void	Disassemble(StateDefault2 *state,  Function& func);
		void	Disassemble(StateDefault2 *state);

		Context(const Module& mod, uint32 flags = 0, async_callback<custom_t>&& custom = none);
	};

public:
	const char*	GetDescription() override { return "Bitcode"; }
	State*		Disassemble(const_memory_block block, uint64 addr, SymbolFinder sym_finder) override;

};

}