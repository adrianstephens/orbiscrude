#include "stdint.h"
#include "stdio.h"
#include "malloc.h"

#define YY_NUM_RULES 9
#define YY_ACCEPT	70

//last_dfa = 43, jam = 44

#define YY_END_OF_BUFFER 10
// Action number for EOF rule of a given start state.
#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)

static const int16_t yy_accept[45] = {   0,
0,    0,   10,    8,    7,    7,    5,    1,    4,    4,
4,    4,    4,    4,    4,    8,    7,    2,    1,    4,
4,    4,    4,    3,    4,    4,    0,    6,    2,    4,
4,    4,    4,    4,    4,    4,    4,    4,    4,    4,
4,    4,    4,    0
};

//ec[char]=CHAR
static const int32_t yy_ec[256] = {   0,
1,    1,    1,    1,    1,    1,    1,    1,    2,    3,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    2,    1,    1,    1,    1,    1,    1,    1,    1,
1,    4,    5,    1,    6,    7,    8,    9,    9,    9,
9,    9,    9,    9,    9,    9,    9,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,   10,   11,   12,   13,

14,   15,   16,   17,   18,   10,   10,   10,   10,   19,
20,   21,   10,   22,   10,   23,   24,   10,   10,   10,
10,   10,   25,    1,   26,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,

1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1
};

// meta[CHAR1] = CHAR2
// meta-equivalence class
static const int32_t yy_meta[27] = {   0,
1,    1,    2,    1,    1,    1,    1,    1,    3,    3,
3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
3,    3,    3,    3,    2,    1
};

// base[STATE] is offset into nxt,chk; YY_ACCEPT->ready to accept
static const int16_t yy_base[47] = {   0,
0,    0,   69,   70,   25,   27,   70,   24,    0,   54,
48,   42,   50,   42,   46,   36,   32,   52,   29,    0,
44,   46,   39,    0,   37,   42,   29,   70,   45,   35,
40,   39,   31,   30,   25,   33,   28,   32,   24,   19,
23,   18,   23,   70,   29,   38
};

// def[state]=state2; if state2 > |accept|, update c
static const int16_t yy_def[47] = {   0,
44,    1,   44,   44,   44,   44,   44,   44,   45,   45,
45,   45,   45,   45,   45,   46,   44,   44,   44,   45,
45,   45,   45,   45,   45,   45,   46,   44,   44,   45,
45,   45,   45,   45,   45,   45,   45,   45,   45,   45,
45,   45,   45,    0,   44,   44
} ;

// nxt[base[state]+char] = next state
static const int16_t yy_nxt[97] = {   0,
4,    5,    6,    7,    7,    7,    4,    7,    8,    9,
10,    9,    9,   11,   12,    9,    9,   13,    9,    9,
14,    9,   15,    9,   16,    4,   17,   17,   17,   17,
18,   20,   19,   17,   17,   18,   24,   19,   27,   43,
27,   24,   42,   41,   40,   39,   38,   37,   24,   24,
36,   35,   34,   29,   28,   33,   32,   31,   24,   30,
29,   28,   26,   25,   24,   23,   22,   21,   44,    3,
44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
44,   44,   44,   44,   44,   44
};

// chk[base[state]+char] = char if nxt is valid
static const int16_t yy_chk[97] = {   0,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
1,    1,    1,    1,    1,    1,    5,    5,    6,    6,
8,   45,    8,   17,   17,   19,   43,   19,   46,   42,
46,   41,   40,   39,   38,   37,   36,   35,   34,   33,
32,   31,   30,   29,   27,   26,   25,   23,   22,   21,
18,   16,   15,   14,   13,   12,   11,   10,    3,   44,
44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
44,   44,   44,   44,   44,   44
};

#define INITIAL 0
#define NUM_ELEMENTS(A) sizeof(A)/sizeof(A[0])

struct LEXER {
	typedef uint8_t YY_CHAR;

	struct BUFFER {
		enum { SIZE = 16384};	// Size of default input buffer.
		enum STATUS : uint8_t {
			NEW			= 0,
			NORMAL		= 1,
			EOF_PENDING = 2,	// EOF's been seen but there's still some text to process
		};
		BUFFER*	next;
		FILE*	in;
		char*	buffer;			// input buffer
		char*	pos;			// current position in input buffer
		size_t	size;			// Size of input buffer in bytes, not including room for EOB characters.
		size_t	n_chars;		// Number of characters read into ch_buf, not including EOB characters.
		bool	own;
		bool	at_bol;			// Whether we're considered to be at the beginning of a line. If so, '^' rules will be active on the next match, otherwise not.
		bool	fill;			// Whether to try to fill the input buffer when we reach the end of it.
		int		lineno;			// The line count
		int		column;			// The column count
		STATUS	status;

		void flush() {
			n_chars		= 0;
			buffer[0]	= buffer[1]	= 0;
			pos			= buffer;
			at_bol		= true;
			status		= NEW;
		}
		BUFFER(FILE * file, int size) noexcept : next(0), in(file), size(size), own(true), fill(true) {
			buffer	= (char*)malloc(size + 2);
			lineno	= 1;
			column	= 0;
			flush();
		}
		~BUFFER() {
			if (own)
				free(buffer);
		}
	};

	BUFFER		*top;
	int			start_state;

	FILE		*in;
	char		hold_char;	// hold_char holds the character lost when yytext is formed.
	size_t		n_chars;	// number of characters read into ch_buf
	char*		buf_pos;	// Points to current character in buffer.

	int			last_accepting_state;
	char*		last_accepting_pos;

	void load(BUFFER *b) {
		n_chars		= b->n_chars;
		buf_pos		= b->pos;
		in			= b->in;
		hold_char	= *buf_pos;
	}

	LEXER() : top(0) {
		start_state = 1;	 // first start state
		in		= stdin;
		top		= new BUFFER(in, BUFFER::SIZE);
		load(top);
	}

	~LEXER() {
		while (BUFFER *b = top) {
			top = b->next;
			delete b;
		}
	}

	int		lex();
};

int LEXER::lex() {
	for (;;) { // loops until end-of-file is reached
		int		state	= start_state;
		char	*cp		= buf_pos, *bp = cp;
		*cp				= hold_char;

		do {
			if (yy_accept[state]) {
				last_accepting_state	= state;
				last_accepting_pos		= cp;
			}

			YY_CHAR c = yy_ec[(uint8_t)*cp];
			while (yy_chk[yy_base[state] + c] != state) {
				state = yy_def[state];
				if (state >= NUM_ELEMENTS(yy_accept)) // >= lastdfa + 2
					c = yy_meta[(uint8_t)c];
			}
			state = yy_nxt[yy_base[state] + (uint8_t)c];
			++cp;

		} while (yy_base[state] != YY_ACCEPT);

		int	act = yy_accept[state];
		if (act == 0) {	// have to back up
			cp		= last_accepting_pos;
			state	= last_accepting_state;
			act		= yy_accept[state];
		}

		hold_char	= *cp;
		*cp			= 0;
		buf_pos		= cp;

		char*	yytext	= bp;
		size_t	yyleng	= cp - bp;

		switch (act) {
			//actions
			case YY_STATE_EOF(INITIAL):
				return 0;

			case YY_END_OF_BUFFER: {
				//read more
				break;
			}

			default:
				//fatal_error("fatal flex scanner internal error--no action found");
				break;
		}
	}
}

#include "base/array.h"
#include "base/algorithm.h"

using namespace iso;

#define YY_TRAILING_MASK		0x2000	// Mask to mark a trailing context accepting number
#define YY_TRAILING_HEAD_MASK	0x4000	// Mask to mark the accepting number of the "head" of a trailing context rule.

#define MAX_RULE (YY_TRAILING_MASK - 1)	// Maximum number of rules, as outlined in the above note.

#define CSIZE 256
#define SYM_EPSILON (CSIZE + 1)			// to mark transitions on the symbol epsilon

bool reject;
// True if the input rules include a rule with both variable-length head and trailing context, false otherwise.
bool variable_trailing_context_rules;

/* Variables for ccl information:
* lastccl - ccl index of the last created ccl
* current_maxccls - current limit on the maximum number of unique ccl's
* cclmap - maps a ccl index to its set pointer
* ccllen - gives the length of a ccl
* cclng - true for a given ccl if the ccl is negated
* cclreuse - counts how many times a ccl is re-used
* current_max_ccl_tbl_size - current limit on number of characters needed to represent the unique ccl's
* ccltbl - holds the characters in each ccl - indexed by cclmap
*/


struct CCL {
	struct ENTRY {
		int		map;	//maps a ccl index to its set pointer
		int		len;	//gives the length of a ccl
		bool	neg;	//true for a given ccl if the ccl is negated
		range<uint8*>	table(uint8 *t) const { return make_range_n(t + map, len); }
	};
	dynamic_array<ENTRY>			entries;
	dynamic_array<unsigned char>	tbl;
	int		reuse;
};


/* Variables for managing equivalence classes:
* numecs - number of equivalence classes
* nextecm - forward link of Equivalence Class members
* ecgroup - class number or backward link of EC members
* nummecs - number of meta-equivalence classes (used to compress
*   templates)
* tecfwd - forward link of meta-equivalence classes members
* tecbck - backward link of MEC's
*/


struct EQUIVALENCE_CLASSES {
	int numecs, nextecm[CSIZE + 1], ecgroup[CSIZE + 1], nummecs;
	int	NUL_ec;

	static void mkechar(int tch, int fwd[], int bck[]) {
		// If until now the character has been a proper subset of an equivalence class, break it away to create a new ec

		if (fwd[tch] != 0)
			bck[fwd[tch]] = bck[tch];

		if (bck[tch] != 0)
			fwd[bck[tch]] = fwd[tch];

		fwd[tch] = 0;
		bck[tch] = 0;
	}

	void mkechar(int tch) {
		mkechar(tch ? tch : NUL_ec, nextecm, ecgroup);
	}

	void sympartition(CCL &ccl, range<const int*> chars, bool symlist[], int duplist[]);
};

void  mkeccl(range<uint8*> ccls, int fwd[], int bck[], int llsiz, int NUL_mapping) {
	static bool cclflags[CSIZE];	/* initialized to all '\0' */
	for (uint8 *cclp = ccls.begin(); cclp < ccls.end(); ++cclp) {
		if (cclflags[cclp - ccls.begin()]) {
			// don't process, but reset "doesn't need processing" flag
			cclflags[cclp - ccls.begin()] = false;

		} else {
			int	cclm = *cclp;
			if (cclm == 0)
				cclm = NUL_mapping;

			int	oldec = bck[cclm];
			int	newec = cclm;

			auto	j = cclp + 1;
			for (int i = fwd[cclm], ccl_char = 0; i && i <= llsiz; i = fwd[i]) {	// look for the symbol in the character class
				for (; j < ccls.end(); ++j) {
					ccl_char = *j == 0 ? NUL_mapping : *j;
					if (ccl_char >= i)
						break;

				}
				if (ccl_char == i && !cclflags[j - ccls.begin()]) {
					// We found an old companion of cclm in the ccl. Link it into the new equivalence class and flag it as having been processed.
					bck[i]		= newec;
					fwd[newec]	= i;
					newec		= i;
					cclflags[j - ccls.begin()] = true;	// Set flag so we don't reprocess

				} else {
					// Symbol isn't in character class.  Put it in the old equivalence class.
					bck[i] = oldec;
					if (oldec)
						fwd[oldec] = i;
					oldec = i;
				}
			}

			if (bck[cclm] || oldec != bck[cclm]) {
				bck[cclm]	= 0;
				fwd[oldec]	= 0;
			}

			fwd[newec] = 0;
		}
	}
}


// partition characters with same out-transitions
// Partitioning is done by creating equivalence classes for those characters which have out-transitions from the given state.  Thus we are really creating equivalence classes of equivalence classes.
void EQUIVALENCE_CLASSES::sympartition(CCL &ccl, range<const int*> chars, bool symlist[], int duplist[]) {
	int     dupfwd[CSIZE + 1];

	for (int i = 1; i <= numecs; ++i) {	/* initialize equivalence class list */
		duplist[i]	= i - 1;
		dupfwd[i]	= i + 1;
	}

	duplist[1]		= 0;
	dupfwd[numecs]	= 0;

	for (auto tch : chars) {

		if (tch != SYM_EPSILON) {
			if (tch >= 0) {
				// character transition
				int     ec = ecgroup[tch];
				mkechar(ec, dupfwd, duplist);
				symlist[ec] = true;

			} else {
				// character class
				auto	&e = ccl.entries[-tch];
				mkeccl(e.table(ccl.tbl), dupfwd, duplist, numecs, NUL_ec);

				if (e.neg) {
					int j = 0;
					for (auto ich : e.table(ccl.tbl)) {
						if (ich == 0)
							ich = NUL_ec;
						for (++j; j < ich; ++j)
							symlist[j] = true;
					}
					for (++j; j <= numecs; ++j)
						symlist[j] = true;

				} else {
					for (auto ich : e.table(ccl.tbl)) {
						if (ich == 0)
							ich = NUL_ec;
						symlist[ich] = true;
					}
				}
			}
		}
	}
}



/* Variables for nfa machine data:
* maximum_mns - maximal number of NFA states supported by tables
* current_mns - current maximum on number of NFA states
* num_rules - number of the last accepting state; also is number of rules created so far
* num_eof_rules - number of <<EOF>> rules
* default_rule - number of the default rule
* current_max_rules - current maximum number of rules
* lastnfa - last nfa state number created
* ccl_has_nl - true if current ccl could match a newline
* nlch - default eol char
*/

struct NFA {

	struct RULE {
		enum TYPE {
			NORMAL		= 0,
			VARIABLE	= 1,
		};
		TYPE	type;		// whether this a ho-hum normal rule or one which has variable head & trailing context
		int		linenum;
		bool	useful;		// true if we've determined that the rule can be matched
		bool	has_nl;		// true if rule could possibly match a newline
	};

	struct STATE {
		enum TYPE {
			NORMAL				= 1,
			TRAILING_CONTEXT	= 2,
		};
		TYPE	type;		//identifying whether the state is part of a normal rule, the leading state in a trailing context rule (i.e., the state which marks the transition from recognizing the text-to-be-matched to the beginning of the trailing context), or a subsequent state in a trailing context rule
		int		rule;		// rule associated with this NFA state (or 0 if none)
		int		accptnum;	// accepting number
		int		trans1;		// transition state
		int		trans2;		// 2nd transition state for epsilons
		int		transchar;	// transition character
		int		firstst;	// physically the first state of a fragment
		int		lastst;		// last physical state of fragment
		int		finalst;	// last logical state of fragment

		STATE(int index, int sym, int rule, TYPE type) : firstst(index), finalst(index), lastst(index), transchar(sym), trans1(0), trans2(0), accptnum(0), rule(rule), type(type) {}
	};

	dynamic_array<RULE>		rules;
	dynamic_array<STATE>	states;

	int maximum_mns, current_mns, current_max_rules;
	int num_rules, num_eof_rules, default_rule, lastnfa;
	bool *ccl_has_nl;
	int nlch;
	int	csize;
	int	numeps;

	int mkstate(int sym, STATE::TYPE type, EQUIVALENCE_CLASSES *ec) {
		int	index = states.size32();
		states.emplace_back(index, sym, rules.size32(), type);
		if (sym >= 0) {
			if (sym == SYM_EPSILON)
				++numeps;
			else if (ec)
				ec->mkechar(sym);
		}
		return index;
	}

	void mkxtion (int statefrom, int stateto) {
		if (!states[statefrom].trans1)
			states[statefrom].trans1 = stateto;
		else if ((states[statefrom].transchar == SYM_EPSILON) && !states[statefrom].trans2)
			states[statefrom].trans2 = stateto;
		else
			;//flexfatal (_("found too many transitions in mkxtion()"));
	}

	int mkbranch (int first, int second, STATE::TYPE type, EQUIVALENCE_CLASSES *ec) {
		if (!first)
			return second;
		else if (!second)
			return first;

		int	eps = mkstate(SYM_EPSILON, type, ec);
		mkxtion(eps, first);
		mkxtion(eps, second);
		return eps;
	}


	//	check to see if NFA state set constitutes "dangerous" trailing context
	//	Trailing context is "dangerous" if both the head and the trailing part are of variable size AND there's a DFA state which contains both an accepting state for the head part of the rule and NFA states which occur after the beginning of the trailing context.
	//	When such a rule is matched, it's impossible to tell if having been in the DFA state indicates the beginning of the trailing context or further-along scanning of the pattern.  In these cases, a warning message is issued.
	//		nfa_states[1 .. num_states] is the list of NFA states in the DFA.
	//		accset[1 .. nacc] is the list of accepting numbers for the DFA state.
	void	check_trailing_context (range<const int*> nfa_states, range<const int*> accset) {
		for (auto ns : nfa_states) {
			auto&	state	= states[ns];
			int		type	= state.type;
			int		ar		= state.rule;
			if (type != STATE::NORMAL && rules[ar].type == RULE::VARIABLE && type == STATE::TRAILING_CONTEXT) {
				// Potential trouble - scan set of accepting numbers for the one marking the end of the "head"
				for (auto j : accset)
					if (j & YY_TRAILING_HEAD_MASK) {
						//line_warning (_("dangerous trailing context"),rule_linenum[ar]);
						return;
					}
			}
		}
	}
	int		epsclosure(dynamic_array<int> &t, int *accset, int *hv_addr);
	int		symfollowset(range<int*> ds, int transsym, int nset[], CCL &ccl, EQUIVALENCE_CLASSES *ec);
};

/* Variables for dfa machine data:
* nxt - state to enter upon reading character
* chk - check value to see if "nxt" applies
* tnxt - internal nxt table for templates
* NUL_ec - equivalence class of the NUL character
* firstfree - first empty entry in "nxt/chk" table
* numas - number of DFA accepting states created; note that this is not necessarily the same value as num_rules, which is the analogous value for the NFA
* numsnpairs - number of state/nextstate transition pairs
* jambase - position in base/def where the default jam table starts
* jamstate - state number corresponding to "jam" state
* end_of_buffer_state - end-of-buffer dfa state number
*/

struct DFA {
	int NUL_ec;
	int num_backing_up;
	int numas;
	int numsnpairs, jambase, jamstate;

	struct ENTRY {
		dynamic_array<int>	set;		//nfa state set
		int					dhash;		//dfa state hash value
		int					accsiz;		//size of accepting set
		dynamic_array<int>	dfaacc_set;
		int					dfaacc_state;
		int					nultrans;	//NUL transition
		int					base;		// offset into "nxt" for given state
		int					def;		// where to go if "chk" disallows "nxt" entry

		bool check_for_backing_up(int state[]) {
			return reject ? dfaacc_set.empty() : dfaacc_state == 0;
		}
		ENTRY(range<const int*> set, int dhash) : set(set), dhash(dhash), accsiz(0), dfaacc_state(0) {}
	};
	dynamic_array<ENTRY>	entries;
	ENTRY	*end_of_buffer_state;

	void	ntod(NFA &nfa, NFA::STATE::TYPE type, CCL &ccl, EQUIVALENCE_CLASSES *ec);
	bool	snstods(NFA &nfa, range<int*> sns, range<int*> accset, int hashval, ENTRY **newds_addr);
};


/* Variables for miscellaneous information:
* sectnum - section number currently being parsed
* nummt - number of empty nxt/chk table entries
* numeps - number of epsilon NFA states created
* tmpuses - number of DFA states that chain to templates
* totnst - total number of NFA states used to make DFA states
* num_backing_up - number of DFA states requiring backing up
* bol_needed - whether scanner needs beginning-of-line recognition
*/

int sectnum, nummt;
int tmpuses, totnst;
int num_backing_up, bol_needed;

/* Variables for start conditions:
* lastsc - last start condition created
* current_max_scs - current limit on number of start conditions
* scset - set of rules active in start condition
* scbol - set of rules active only at the beginning of line in a s.c.
* scxclu - true if start condition is exclusive
* sceof - true if start condition has EOF rule
* scname - start condition name
*/

int lastsc, *scset, *scbol, *scxclu, *sceof;
int current_max_scs;
char **scname;


// epsclosure - construct the epsilon closure of a set of ndfa states
//	The epsilon closure is the set of all states reachable by an arbitrary number of epsilon transitions, which themselves do not have epsilon transitions going out, unioned with the set of states which have non-null accepting numbers.
//	t is an array of size numstates of nfa state numbers. on return, t holds the epsilon closure and numstates_addr is updated; t may be subjected to reallocation if it is not large enough to hold the epsilon closure
//	accset holds a list of the accepting numbers, and the size of accset is given by *nacc_addr
//	hashval is the hash value for the dfa corresponding to the state set.

int NFA::epsclosure(dynamic_array<int> &t, int *accset, int *hv_addr) {
	dynamic_array<int>	stk;
	int	nacc	= 0;
	int	hashval = 0;

	for (auto ns : t) {
		auto&	s	= states[ns];

		// The state could be marked if we've already pushed it onto the stack.
		if (s.trans1 >= 0) {
			stk.push_back(ns);
			s.trans1 = ~s.trans1;
			if (int nfaccnum = s.accptnum)
				accset[++nacc] = nfaccnum;
			hashval += ns;
		}
	}

	for (auto stkpos = stk.begin(); stkpos != stk.end(); ++stkpos) {
		int		ns	= *stkpos;
		auto&	s	= states[ns];

		if (s.transchar == SYM_EPSILON) {
			int	tsp = ~s.trans1;

			if (tsp) {
				if (states[tsp].trans1 >= 0) {
					auto&	s2 = states[tsp];
					stk.push_back(tsp);
					s2.trans1 = ~s2.trans1;
					if (s2.accptnum) 
						accset[++nacc] = s2.accptnum;
					if (s2.accptnum || s2.transchar != SYM_EPSILON) {
						t.push_back(tsp);
						hashval	+= tsp;
					}
				}

				if ((tsp = s.trans2) && states[tsp].trans1 >= 0) {
					auto&	s2 = states[tsp];
					stk.push_back(tsp);
					s2.trans1 = ~s2.trans1;
					if (s2.accptnum)
						accset[++nacc] = s2.accptnum;
					if (s2.accptnum || s2.transchar != SYM_EPSILON) {
						t.push_back(tsp);
						hashval	+= tsp;
					}
				}
			}
		}
	}

	// Clear out "visit" markers
	for (auto i : stk) {
		if (states[i].trans1 < 0)
			states[i].trans1 = ~states[i].trans1;
		else
			;//flexfatal (("consistency check failed in epsclosure()"));
	}

	*hv_addr	= hashval;
	return nacc;
}

struct TABLES {
	struct ONE {
		int     state, sym, next, def;
		ONE(int state, int sym, int next, int def) : state(state), sym(sym), next(next), def(def) {}
	};
	dynamic_array<ONE>	ones;
	dynamic_array<int>	nxt, chk, tnxt;
	int					firstfree;

	void stack1 (int state, int sym, int next, int def) {
		ones.emplace_back(state, sym, next, def);
	}
};

// Creates the dfa corresponding to the ndfa we've constructed.  The dfa starts out in state #1.
void DFA::ntod(NFA &nfa, NFA::STATE::TYPE type, CCL &ccl, EQUIVALENCE_CLASSES *ec) {
	bool	symlist[CSIZE + 1];
	int		duplist[CSIZE + 1], state[CSIZE + 1];
	int		targfreq[CSIZE + 1] = {0}, targstate[CSIZE + 1];

	// accset needs to be large enough to hold all of the rules present in the input, *plus* their YY_TRAILING_HEAD_MASK variants.
	dynamic_array<int>	accset((nfa.num_rules + 1) * 2);
	dynamic_array<int>	nset;

	// The "todo" queue is represented by the head, which is the DFA state currently being processed, and the "next", which is the next DFA state number available (not in use). 
	// We depend on the fact that snstods() returns DFA's \in increasing order/, and thus need only know the bounds of the dfas to be processed.
	int	todo_head = 0, todo_next = 0;

	for (int i = 0; i <= nfa.csize; ++i) {
		duplist[i] = 0;
		symlist[i] = false;
	}

	for (int i = 0; i <= nfa.num_rules; ++i)
		accset[i] = 0;

//	inittbl();

	// Create the first states
	int	num_start_states = lastsc * 2;

	for (int i = 1; i <= num_start_states; ++i) {
		int	numstates = 1;

		// For each start condition, make one state for the case when we're at the beginning of the line (the '^' operator) and one for the case when we're not.
		nset.push_back(
			i % 2 == 1 ? scset[(i / 2) + 1] : nfa.mkbranch(scbol[i / 2], scset[i / 2], type, ec)
		);

		ENTRY	*ds;
		int	hashval;
		int nacc = nfa.epsclosure(nset, accset, &hashval);

		if (snstods(nfa, nset, accset, hashval, &ds)) {
			numas	+= nacc;
			totnst	+= numstates;
			++todo_next;
			if (variable_trailing_context_rules && nacc > 0)
				nfa.check_trailing_context(nset, accset);
		}
	}

	if (!snstods (nfa, none, none, 0, &end_of_buffer_state))
		;//flexfatal (("could not create unique end-of-buffer state"));

	++numas;
	++num_start_states;
	++todo_next;

	while (todo_head < todo_next) {
		for (int i = 1; i <= ec->numecs; ++i)
			state[i] = 0;

		ENTRY	&ds		= entries[++todo_head];
		dynamic_array<int>	transchar = filter(ds.set, [&nfa](int i) { return nfa.states[i].transchar; });
		ec->sympartition(ccl, transchar, symlist, duplist);

		int	targptr = 0;
		int	totaltrans = 0;
		for (int sym = 1; sym <= ec->numecs; ++sym) {
			if (symlist[sym]) {
				symlist[sym] = false;

				if (duplist[sym] == 0) {
					/* Symbol has unique out-transitions. */
					ENTRY	*newds;
					int	hashval;
					int	numstates = nfa.symfollowset(ds.set, sym, nset, ccl, ec);
					int	nacc = nfa.epsclosure(nset, accset, &hashval);

					if (snstods(nfa, nset, accset, hashval, &newds)) {
						totnst = totnst + numstates;
						++todo_next;
						numas += nacc;

						if (variable_trailing_context_rules && nacc > 0)
							nfa.check_trailing_context(nset, accset);
					}

					state[sym] = entries.index_of(newds);

					targfreq[++targptr] = 1;
					targstate[targptr]	= entries.index_of(newds);

				} else {
					// sym's equivalence class has the same transitions as duplist(sym)'s equivalence class.
					int	targ = state[duplist[sym]];
					state[sym] = targ;

					// Update frequency count for destination state.
					int i = 0;
					while (targstate[++i] != targ) ;

					++targfreq[i];
				}

				++totaltrans;
				duplist[sym] = 0;
			}
		}


		numsnpairs += totaltrans;

		if (&ds > entries + num_start_states && ds.check_for_backing_up(state))
			++num_backing_up;

		ds.nultrans	= exchange(state[NUL_ec], 0);

		if (&ds == end_of_buffer_state) {
			// Special case this state to make sure it does what it's supposed to, i.e., jam on end-of-buffer.
			//stack1(ds, 0, 0, JAMSTATE);

		} else {
			// normal, compressed state: Determine which destination state is the most common, and how many transitions to it there are.
			int	comfreq = 0;
			int	comstate = 0;
			for (int i = 1; i <= targptr; ++i)
				if (targfreq[i] > comfreq) {
					comfreq		= targfreq[i];
					comstate	= targstate[i];
				}

			//bldtbl(state, ds, totaltrans, comstate, comfreq);
		}
	}
}


/* snstods - converts a set of ndfa states into a dfa state
*
* synopsis
*    is_new_state = snstods( int sns[numstates], int numstates,
*				int accset[num_rules+1], int nacc,
*				int hashval, int *newds_addr );
*
* On return, the dfa state number is in newds.
*/

bool DFA::snstods(NFA &nfa, range<int*> sns, range<int*> accset, int hashval, ENTRY **newds_addr) {
	int numstates = sns.size32();
	
	sort(sns);

	for (auto &i : entries) {
		if (hashval == i.dhash) {
			if (numstates == i.set.size32()) {
				bool	same = true;
				for (int j = 0; same && j < numstates; ++j)
					same = sns[j] == i.set[j];
				if (same) {
					*newds_addr = &i;
					return false;
				}
			}
		}
	}

	// Make a new dfa.
	auto	&e	= entries.emplace_back(sns, hashval);

	if (!accset.empty()) {
		if (reject) {
			// We sort the accepting set in increasing order so the disambiguating rule that the first rule listed is considered match in the event of ties will work.
			sort(accset);

			// Save the accepting set for later
			e.dfaacc_set = accset;
			for (auto i : accset) {
				if (i < nfa.rules.size())
					nfa.rules[i].useful = true;		// Who knows, perhaps a REJECT can yield this rule.
			}

		} else {
			// Find lowest numbered rule so the disambiguating rule will work.
			auto	j = nfa.rules.size();
			for (auto &i : accset)
				if (i < j)
					j = i;

			e.dfaacc_state = j;
			if (j < nfa.rules.size())
				nfa.rules[j].useful = true;
		}
	}

	*newds_addr = &e;
	return true;
}


// follow the symbol transitions one step
int NFA::symfollowset(range<int*> ds, int transsym, int nset[], CCL &ccl, EQUIVALENCE_CLASSES *ec) {
	int     numstates = 0;
	for (auto ns : ds) {	/* for each nfa state ns in the state set of ds */
		int	sym = states[ns].transchar;
		int	tsp = states[ns].trans1;

		if (sym < 0) {	// it's a character class
			auto&	e = ccl.entries[-sym];

			if (e.neg) {
				for (auto ch : e.table(ccl.tbl)) {
					// Loop through negated character class.
					if (ch == 0)
						ch = ec->NUL_ec;

					if (ch > transsym)
						// Transsym isn't in negated ccl
						break;

					if (ch == transsym)
						continue;
				}

				// Didn't find transsym in ccl
				nset[++numstates] = tsp;
			} else {
				for (auto ch : e.table(ccl.tbl)) {
					if (ch == 0)
						ch = ec->NUL_ec;

					if (ch > transsym)
						break;

					else if (ch == transsym) {
						nset[++numstates] = tsp;
						break;
					}
				}
			}
		} else if (sym != SYM_EPSILON && abs(ec->ecgroup[sym]) == transsym) {
			nset[++numstates] = tsp;
		}
	}

	return numstates;
}


