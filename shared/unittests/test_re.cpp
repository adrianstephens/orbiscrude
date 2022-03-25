#include "extra/regex.h"
#include "stream.h"

using namespace iso;
using namespace re2;

struct test_regex {
	test_regex() {

		dynamic_array<count_string>		matches;
//		regex	re("[^ab]*(?:a+|b)+");
//		regex	re(".*?(?:a+|b)+");
		regex	re(".*(ac|bc)");
		bool	got = re.match("abaaacbx", matches, encoding_ascii);

		regex	rekw(
			"\\b("
			"alignas|alignof|and|and_eq|asm|atomic_cancel|atomic_commit|atomic_noexcept"
			"|auto|bitand|bitor|bool|break|case|catch|char"
			"|char16_t|char32_t|class|compl|concept|const|constexpr|const_cast"
			"|continue|co_await|co_return|co_yield|decltype|default|delete|do"
			"|double|dynamic_cast|else|enum|explicit|export|extern|false"
			"|float|for|friend|goto|if|import|inline|int"
			"|long|module|mutable|namespace|new|noexcept|not|not_eq"
			"|nullptr|operator|or|or_eq|private|protected|public|register"
			"|reinterpret_cast|requires|return|short|signed|sizeof|static|static_assert"
			"|static_cast|struct|switch|synchronized|template|this|thread_local|throw"
			"|true|try|typedef|typeid|typename|union|unsigned|using"
			"|virtual|void|volatile|wchar_t|while|xor|xor_eq"
			")\\b"
		);
		got = rekw.search("hairy template x", matches);

		regex_set	set;

		const char *comment	= "/\\*.*?\\*/|//.*?$";
		set.add(comment);

		const char *str		= "\".*?[^\\\\]\"|'.*?[^\\\\]'";
		set.add(str);

		const char *num_fp	= "\\b[-+]?(0\\.|[1-9][0-9]*\\.?)[0-9]+([eE][-+]?[0-9]+)?\\b";
		set.add(num_fp);

		const char *num_int	= "\\b[-+]?("
			"0[0-7]*"				//oct
			"|[1-9][0-9]*"			//dec
			"|0[xX][0-9a-fA-F]+"	//hex
			"|0[bB][01]+"			//bin
			")[uU]?(l|L|ll|LL)?[uU]?\\b";
		set.add(num_int);

		set.add(
			"\\b("
			"alignas|alignof|and|and_eq|asm|atomic_cancel|atomic_commit|atomic_noexcept"
			"|auto|bitand|bitor|bool|break|case|catch|char"
			"|char16_t|char32_t|class|compl|concept|const|constexpr|const_cast"
			"|continue|co_await|co_return|co_yield|decltype|default|delete|do"
			"|double|dynamic_cast|else|enum|explicit|export|extern|false"
			"|float|for|friend|goto|if|import|inline|int"
			"|long|module|mutable|namespace|new|noexcept|not|not_eq"
			"|nullptr|operator|or|or_eq|private|protected|public|register"
			"|reinterpret_cast|requires|return|short|signed|sizeof|static|static_assert"
			"|static_cast|struct|switch|synchronized|template|this|thread_local|throw"
			"|true|try|typedef|typeid|typename|union|unsigned|using"
			"|virtual|void|volatile|wchar_t|while|xor|xor_eq"
			")\\b"
		);

		malloc_block	block	= malloc_block::unterminated(FileInput("D:\\dev\\shared\\common\\base\\defs.cpp"));
		count_string	text(block, block.length());
		count_string	section = text;

		const char *repl[] = {
			"<comment>",
			"<string>",
			"<float>",
			"<int>",
			"<keyword>"
		};
		string	s = set.replace(section, repl);
		
		auto	flags	= match_default;
		while (int id = set.search(section, matches)) {
			section = section.slice(matches[0].end());
			flags |= match_prev_avail;
		}
	}
} _test_regex;
