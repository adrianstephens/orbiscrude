#include "base/defs.h"
#include "base/array.h"
#include "base/hash.h"
#include "base/strings.h"

//-----------------------------------------------------------------------------
//	GIMPLE - GNU IL
//-----------------------------------------------------------------------------

using namespace iso;

static hash_set<string> *file_name_hash_table;
typedef uint8	lto_decl_flags_t;

struct symtab_node;

struct location_t {
	const char *file;
	int			line;
};

struct bitpack_d {
	uint32	pos;
	uint32	word;
	void	*stream;
};

enum tree_code {
	ERROR_MARK,						// exceptional	0
	IDENTIFIER_NODE,				// exceptional	0
	TREE_LIST,						// exceptional	0
	TREE_VEC,						// exceptional	0
	BLOCK,							// exceptional	0

	OFFSET_TYPE,					// type		0
	ENUMERAL_TYPE,					// type		0
	BOOLEAN_TYPE,					// type		0
	INTEGER_TYPE,					// type		0
	REAL_TYPE,						// type		0
	POINTER_TYPE,					// type		0
	REFERENCE_TYPE,					// type		0
	NULLPTR_TYPE,					// type		0
	FIXED_POINT_TYPE,				// type		0
	COMPLEX_TYPE,					// type		0
	VECTOR_TYPE,					// type		0
	ARRAY_TYPE,						// type		0
	RECORD_TYPE,					// type		0
	UNION_TYPE,						// type		0
	QUAL_UNION_TYPE,				// type		0
	VOID_TYPE,						// type		0
	FUNCTION_TYPE,					// type		0
	METHOD_TYPE,					// type		0
	LANG_TYPE,						// type		0

	VOID_CST,						// constant	0
	INTEGER_CST,					// constant	0
	POLY_INT_CST,					// constant	0
	REAL_CST,						// constant	0
	FIXED_CST,						// constant	0
	COMPLEX_CST,					// constant	0
	VECTOR_CST,						// constant	0
	STRING_CST,						// constant	0
	FUNCTION_DECL,					// declaration	0
	LABEL_DECL,						// declaration	0
	FIELD_DECL,						// declaration	0
	VAR_DECL,						// declaration	0
	CONST_DECL,						// declaration	0
	PARM_DECL,						// declaration	0
	TYPE_DECL,						// declaration	0
	RESULT_DECL,					// declaration	0
	DEBUG_EXPR_DECL,				// declaration	0
	DEBUG_BEGIN_STMT,				// statement	0
	NAMESPACE_DECL,					// declaration	0
	IMPORTED_DECL,					// declaration	0
	NAMELIST_DECL,					// declaration	0
	TRANSLATION_UNIT_DECL,			// declaration	0

	COMPONENT_REF,					// reference	3
	BIT_FIELD_REF,					// reference	3
	ARRAY_REF,						// reference	4
	ARRAY_RANGE_REF,				// reference	4
	REALPART_EXPR,					// reference	1
	IMAGPART_EXPR,					// reference	1
	VIEW_CONVERT_EXPR,				// reference	1
	INDIRECT_REF,					// reference	1
	OBJ_TYPE_REF,					// expression	3
	CONSTRUCTOR,					// exceptional	0
	COMPOUND_EXPR,					// expression	2
	MODIFY_EXPR,					// expression	2
	INIT_EXPR,						// expression	2
	TARGET_EXPR,					// expression	4
	COND_EXPR,						// expression	3
	VEC_DUPLICATE_EXPR,				// unary		1
	VEC_SERIES_EXPR,				// binary		2
	VEC_COND_EXPR,					// expression	3
	VEC_PERM_EXPR,					// expression	3
	BIND_EXPR,						// expression	3
	CALL_EXPR,						// vl_exp		3
	WITH_CLEANUP_EXPR,				// expression	1
	CLEANUP_POINT_EXPR,				// expression	1
	PLACEHOLDER_EXPR,				// exceptional	0
	PLUS_EXPR,						// binary		2
	MINUS_EXPR,						// binary		2
	MULT_EXPR,						// binary		2
	POINTER_PLUS_EXPR,				// binary		2
	POINTER_DIFF_EXPR,				// binary		2
	MULT_HIGHPART_EXPR,				// binary		2
	TRUNC_DIV_EXPR,					// binary		2
	CEIL_DIV_EXPR,					// binary		2
	FLOOR_DIV_EXPR,					// binary		2
	ROUND_DIV_EXPR,					// binary		2
	TRUNC_MOD_EXPR,					// binary		2
	CEIL_MOD_EXPR,					// binary		2
	FLOOR_MOD_EXPR,					// binary		2
	ROUND_MOD_EXPR,					// binary		2
	RDIV_EXPR,						// binary		2
	EXACT_DIV_EXPR,					// binary		2
	FIX_TRUNC_EXPR,					// unary		1
	FLOAT_EXPR,						// unary		1
	NEGATE_EXPR,					// unary		1
	MIN_EXPR,						// binary		2
	MAX_EXPR,						// binary		2
	ABS_EXPR,						// unary		1
	ABSU_EXPR,						// unary		1
	LSHIFT_EXPR,					// binary		2
	RSHIFT_EXPR,					// binary		2
	LROTATE_EXPR,					// binary		2
	RROTATE_EXPR,					// binary		2
	BIT_IOR_EXPR,					// binary		2
	BIT_XOR_EXPR,					// binary		2
	BIT_AND_EXPR,					// binary		2
	BIT_NOT_EXPR,					// unary		1
	TRUTH_ANDIF_EXPR,				// expression	2
	TRUTH_ORIF_EXPR,				// expression	2
	TRUTH_AND_EXPR,					// expression	2
	TRUTH_OR_EXPR,					// expression	2
	TRUTH_XOR_EXPR,					// expression	2
	TRUTH_NOT_EXPR,					// expression	1
	LT_EXPR,						// comparison	2
	LE_EXPR,						// comparison	2
	GT_EXPR,						// comparison	2
	GE_EXPR,						// comparison	2
	EQ_EXPR,						// comparison	2
	NE_EXPR,						// comparison	2
	UNORDERED_EXPR,					// comparison	2
	ORDERED_EXPR,					// comparison	2
	UNLT_EXPR,						// comparison	2
	UNLE_EXPR,						// comparison	2
	UNGT_EXPR,						// comparison	2
	UNGE_EXPR,						// comparison	2
	UNEQ_EXPR,						// comparison	2
	LTGT_EXPR,						// comparison	2
	RANGE_EXPR,						// binary		2
	PAREN_EXPR,						// unary		1
	CONVERT_EXPR,					// unary		1
	ADDR_SPACE_CONVERT_EXPR,		// unary		1
	FIXED_CONVERT_EXPR,				// unary		1
	NOP_EXPR,						// unary		1
	NON_LVALUE_EXPR,				// unary		1
	COMPOUND_LITERAL_EXPR,			// expression	1
	SAVE_EXPR,						// expression	1
	ADDR_EXPR,						// expression	1
	FDESC_EXPR,						// expression	2
	BIT_INSERT_EXPR,				// expression	3
	COMPLEX_EXPR,					// binary		2
	CONJ_EXPR,						// unary		1
	PREDECREMENT_EXPR,				// expression	2
	PREINCREMENT_EXPR,				// expression	2
	POSTDECREMENT_EXPR,				// expression	2
	POSTINCREMENT_EXPR,				// expression	2
	VA_ARG_EXPR,					// expression	1
	TRY_CATCH_EXPR,					// statement	2
	TRY_FINALLY_EXPR,				// statement	2

	DECL_EXPR,						// statement	1
	LABEL_EXPR,						// statement	1
	GOTO_EXPR,						// statement	1
	RETURN_EXPR,					// statement	1
	EXIT_EXPR,						// statement	1
	LOOP_EXPR,						// statement	1
	SWITCH_EXPR,					// statement	2
	CASE_LABEL_EXPR,				// statement	4
	ASM_EXPR,						// statement	5
	SSA_NAME,						// exceptional	0
	CATCH_EXPR,						// statement	2
	EH_FILTER_EXPR,					// statement	2
	SCEV_KNOWN,						// expression	0
	SCEV_NOT_KNOWN,					// expression	0
	POLYNOMIAL_CHREC,				// expression	2
	STATEMENT_LIST,					// exceptional	0
	ASSERT_EXPR,					// expression	2
	TREE_BINFO,						// exceptional	0
	WITH_SIZE_EXPR,					// expression	2
	REALIGN_LOAD_EXPR,				// expression	3
	TARGET_MEM_REF,					// reference	5
	MEM_REF,						// reference	2
	OACC_PARALLEL,					// statement	2
	OACC_KERNELS,					// statement	2
	OACC_DATA,						// statement	2
	OACC_HOST_DATA,					// statement	2
	OMP_PARALLEL,					// statement	2
	OMP_TASK,						// statement	2
	OMP_FOR,						// statement	7
	OMP_SIMD,						// statement	7
	OMP_DISTRIBUTE,					// statement	7
	OMP_TASKLOOP,					// statement	7
	OACC_LOOP,						// statement	7
	OMP_TEAMS,						// statement	2
	OMP_TARGET_DATA,				// statement	2
	OMP_TARGET,						// statement	2
	OMP_SECTIONS,					// statement	2
	OMP_ORDERED,					// statement	2
	OMP_CRITICAL,					// statement	3
	OMP_SINGLE,						// statement	2
	OMP_SECTION,					// statement	1
	OMP_MASTER,						// statement	1
	OMP_TASKGROUP,					// statement	1
	OACC_CACHE,						// statement	1
	OACC_DECLARE,					// statement	1
	OACC_ENTER_DATA,				// statement	1
	OACC_EXIT_DATA,					// statement	1
	OACC_UPDATE,					// statement	1
	OMP_TARGET_UPDATE,				// statement	1
	OMP_TARGET_ENTER_DATA,			// statement	1
	OMP_TARGET_EXIT_DATA,			// statement	1
	OMP_ATOMIC,						// statement	2
	OMP_ATOMIC_READ,				// statement	1
	OMP_ATOMIC_CAPTURE_OLD,			// statement	2
	OMP_ATOMIC_CAPTURE_NEW,			// statement	2
	OMP_CLAUSE,						// exceptional	0
	TRANSACTION_EXPR,				// expression	1
	DOT_PROD_EXPR,					// expression	3
	WIDEN_SUM_EXPR,					// binary		2
	SAD_EXPR,						// expression	3
	WIDEN_MULT_EXPR,				// binary		2
	WIDEN_MULT_PLUS_EXPR,			// expression	3
	WIDEN_MULT_MINUS_EXPR,			// expression	3
	WIDEN_LSHIFT_EXPR,				// binary		2
	VEC_WIDEN_MULT_HI_EXPR,			// binary		2
	VEC_WIDEN_MULT_LO_EXPR,			// binary		2
	VEC_WIDEN_MULT_EVEN_EXPR,		// binary		2
	VEC_WIDEN_MULT_ODD_EXPR,		// binary		2
	VEC_UNPACK_HI_EXPR,				// unary		1
	VEC_UNPACK_LO_EXPR,				// unary		1
	VEC_UNPACK_FLOAT_HI_EXPR,		// unary		1
	VEC_UNPACK_FLOAT_LO_EXPR,		// unary		1
	VEC_UNPACK_FIX_TRUNC_HI_EXPR,	// unary		1
	VEC_UNPACK_FIX_TRUNC_LO_EXPR,	// unary		1
	VEC_PACK_TRUNC_EXPR,			// binary		2
	VEC_PACK_SAT_EXPR,				// binary		2
	VEC_PACK_FIX_TRUNC_EXPR,		// binary		2
	VEC_PACK_FLOAT_EXPR,			// binary		2
	VEC_WIDEN_LSHIFT_HI_EXPR,		// binary		2
	VEC_WIDEN_LSHIFT_LO_EXPR,		// binary		2
	PREDICT_EXPR,					// expression	1
	OPTIMIZATION_NODE,				// exceptional	0
	TARGET_OPTION_NODE,				// exceptional	0
	ANNOTATE_EXPR,					// expression	3

	CONVERT0,
	CONVERT1,
	CONVERT2,
	VIEW_CONVERT0,
	VIEW_CONVERT1,
	VIEW_CONVERT2,
	MAX_TREE_CODES
};

enum gimple_code {
	GIMPLE_ERROR_MARK,				//GSS_BASE
	GIMPLE_COND,					//GSS_WITH_OPS
	GIMPLE_DEBUG,					//GSS_WITH_OPS
	GIMPLE_GOTO,					//GSS_WITH_OPS
	GIMPLE_LABEL,					//GSS_WITH_OPS
	GIMPLE_SWITCH,					//GSS_WITH_OPS
	GIMPLE_ASSIGN,					//GSS_WITH_MEM_OPS
	GIMPLE_ASM,						//GSS_ASM
	GIMPLE_CALL,					//GSS_CALL
	GIMPLE_TRANSACTION,				//GSS_TRANSACTION
	GIMPLE_RETURN,					//GSS_WITH_MEM_OPS
	GIMPLE_BIND,					//GSS_BIND
	GIMPLE_CATCH,					//GSS_CATCH
	GIMPLE_EH_FILTER,				//GSS_EH_FILTER
	GIMPLE_EH_MUST_NOT_THROW,		//GSS_EH_MNT
	GIMPLE_EH_ELSE,					//GSS_EH_ELSE
	GIMPLE_RESX,					//GSS_EH_CTRL
	GIMPLE_EH_DISPATCH,				//GSS_EH_CTRL
	GIMPLE_PHI,						//GSS_PHI
	GIMPLE_TRY,						//GSS_TRY
	GIMPLE_NOP,						//GSS_BASE
	GIMPLE_OMP_ATOMIC_LOAD,			//GSS_OMP_ATOMIC_LOAD
	GIMPLE_OMP_ATOMIC_STORE,		//GSS_OMP_ATOMIC_STORE_LAYOUT
	GIMPLE_OMP_CONTINUE,			//GSS_OMP_CONTINUE
	GIMPLE_OMP_CRITICAL,			//GSS_OMP_CRITICAL
	GIMPLE_OMP_FOR,					//GSS_OMP_FOR
	GIMPLE_OMP_MASTER,				//GSS_OMP
	GIMPLE_OMP_TASKGROUP,			//GSS_OMP
	GIMPLE_OMP_PARALLEL,			//GSS_OMP_PARALLEL_LAYOUT
	GIMPLE_OMP_TASK,				//GSS_OMP_TASK
	GIMPLE_OMP_RETURN,				//GSS_OMP_ATOMIC_STORE_LAYOUT
	GIMPLE_OMP_SECTION,				//GSS_OMP
	GIMPLE_OMP_SECTIONS,			//GSS_OMP_SECTIONS
	GIMPLE_OMP_SECTIONS_SWITCH,		//GSS_BASE
	GIMPLE_OMP_SINGLE,				//GSS_OMP_SINGLE_LAYOUT
	GIMPLE_OMP_TARGET,				//GSS_OMP_PARALLEL_LAYOUT
	GIMPLE_OMP_TEAMS,				//GSS_OMP_SINGLE_LAYOUT
	GIMPLE_OMP_ORDERED,				//GSS_OMP_SINGLE_LAYOUT
	GIMPLE_OMP_GRID_BODY,			//GSS_OMP
	GIMPLE_PREDICT,					//GSS_BASE
	GIMPLE_WITH_CLEANUP_EXPR,		//GSS_WCE
	LAST_AND_UNUSED_GIMPLE_CODE
};


enum LTO_tags {
	LTO_null = 0,
	LTO_tree_pickle_reference,
	LTO_bb0 = 1 + MAX_TREE_CODES + LAST_AND_UNUSED_GIMPLE_CODE,
	LTO_bb1,
	LTO_eh_region,
	LTO_integer_cst,
	LTO_function,
	LTO_eh_table,
	LTO_ert_cleanup,
	LTO_ert_try,
	LTO_ert_allowed_exceptions,
	LTO_ert_must_not_throw,
	LTO_eh_landing_pad,
	LTO_eh_catch,
	LTO_tree_scc,
	LTO_field_decl_ref,
	LTO_function_decl_ref,
	LTO_label_decl_ref,
	LTO_namespace_decl_ref,
	LTO_result_decl_ref,
	LTO_ssa_name_ref,
	LTO_type_decl_ref,
	LTO_type_ref,
	LTO_const_decl_ref,
	LTO_imported_decl_ref,
	LTO_translation_unit_decl_ref,
	LTO_global_decl_ref,
	LTO_namelist_decl_ref,
	LTO_NUM_TAGS
};

enum lto_section_type {
	LTO_section_decls = 0,
	LTO_section_function_body,
	LTO_section_static_initializer,
	LTO_section_symtab,
	LTO_section_refs,
	LTO_section_asm,
	LTO_section_jump_functions,
	LTO_section_ipa_pure_const,
	LTO_section_ipa_reference,
	LTO_section_ipa_profile,
	LTO_section_symtab_nodes,
	LTO_section_opts,
	LTO_section_cgraph_opt_sum,
	LTO_section_ipa_fn_summary,
	LTO_section_ipcp_transform,
	LTO_section_ipa_icf,
	LTO_section_offload_table,
	LTO_section_mode_table,
	LTO_section_ipa_hsa,
	LTO_N_SECTION_TYPES
};
enum lto_decl_stream_e_t {
	LTO_DECL_STREAM_TYPE = 0,
	LTO_DECL_STREAM_FIELD_DECL,
	LTO_DECL_STREAM_FN_DECL,
	LTO_DECL_STREAM_VAR_DECL,
	LTO_DECL_STREAM_TYPE_DECL,
	LTO_DECL_STREAM_NAMESPACE_DECL,
	LTO_DECL_STREAM_LABEL_DECL,
	LTO_N_DECL_STREAMS
};
/*
#define DEFINE_DECL_STREAM_FUNCS(UPPER_NAME, name)
static inline tree lto_file_decl_data_get_ ## name (lto_file_decl_data *data, uint32 idx) {
  lto_in_decl_state *state = data->current_decl_state;
   return (*state->streams[LTO_DECL_STREAM_## UPPER_NAME])[idx];
}

static inline uint32 lto_file_decl_data_num_ ## name ## s (lto_file_decl_data *data) {
	lto_in_decl_state *state = data->current_decl_state;
	return vec_safe_length (state->streams[LTO_DECL_STREAM_## UPPER_NAME]);
}
*/

typedef const char* (lto_get_section_data_f)(lto_file_decl_data *, lto_section_type, const char *, size_t *);
typedef void (lto_free_section_data_f)(lto_file_decl_data *, lto_section_type, const char *, const char *, size_t);

class lto_location_cache {
	static int cmp_loc(const void *pa, const void *pb);
	struct cached_location {
		const char *file;
		location_t *loc;
		int line, col;
		bool sysp;
	};

	auto_vec<cached_location> loc_cache;

	int accepted_length;

	const char *current_file;
	int current_line;
	int current_col;
	bool current_sysp;
	location_t current_loc;
public:
	bool apply_location_cache();
	void accept_location_cache();

	void revert_location_cache();
	void input_location(location_t *loc, bitpack_d *bp, data_in *data_in);
	lto_location_cache() : loc_cache(), accepted_length(0), current_file(NULL), current_line(0), current_col(0), current_sysp(false), current_loc(UNKNOWN_LOCATION) {
		ISO_ASSERT(!current_cache);
		current_cache = this;
	}
	~lto_location_cache() {
		apply_location_cache();
		ISO_ASSERT(current_cache == this);
		current_cache = NULL;
	}

	static lto_location_cache *current_cache;

};

class lto_input_block {
public:
	lto_input_block(const char *data_, uint32 p_, uint32 len_, const uint8 *mode_table_) : data(data_), mode_table(mode_table_), p(p_), len(len_) {}
	lto_input_block(const char *data_, uint32 len_, const uint8 *mode_table_) : data(data_), mode_table(mode_table_), p(0), len(len_) {}
	const char *data;
	const uint8 *mode_table;
	uint32 p;
	uint32 len;
};

struct lto_header {
	int16 major_version;
	int16 minor_version;
};
struct lto_simple_header : lto_header {
	int32 main_size;
};
struct lto_simple_header_with_strings : lto_simple_header {
	int32 string_size;
};
struct lto_function_header : lto_simple_header_with_strings {
	int32 cfg_size;
};
struct lto_decl_header : lto_simple_header_with_strings {
	int32 decl_state_size;
	int32 num_nodes;
};

struct lto_stats_d {
	uint32 num_input_cgraph_nodes;
	uint32 num_output_symtab_nodes;
	uint32 num_input_files;
	uint32 num_output_files;
	uint32 num_cgraph_partitions;
	uint32 section_size[LTO_N_SECTION_TYPES];
	uint32 num_function_bodies;
	uint32 num_trees[NUM_TREE_CODES];
	uint32 num_output_il_bytes;
	uint32 num_compressed_il_bytes;
	uint32 num_input_il_bytes;
	uint32 num_uncompressed_il_bytes;
	uint32 num_tree_bodies_output;
	uint32 num_pickle_refs_output;
};

struct lto_encoder_entry {
	symtab_node *node;
	uint32 in_partition : 1;
	uint32 body : 1;
	uint32 initializer : 1;
};

struct lto_symtab_encoder_d {
	dynamic_array<lto_encoder_entry> nodes;
	hash_map<symtab_node *, size_t> *map;
};

typedef struct lto_symtab_encoder_d *lto_symtab_encoder_t;

struct lto_symtab_encoder_iterator {
	lto_symtab_encoder_t encoder;
	unsigned index;
};

struct lto_tree_ref_encoder {
	hash_map<tree, unsigned> *tree_hash_table;
	dynamic_array<tree> trees;
};

struct GTY((for_user)) lto_in_decl_state {
	dynamic_array<tree, va_gc> *streams[LTO_N_DECL_STREAMS];
	tree fn_decl;
	bool compressed;
};

typedef struct lto_in_decl_state *lto_in_decl_state_ptr;

struct decl_state_hasher : ggc_ptr_hash<lto_in_decl_state> {
	static hashval_t hash(lto_in_decl_state *s) {
		return htab_hash_pointer(s->fn_decl);
	}
	static bool equal(lto_in_decl_state *a, lto_in_decl_state *b) {
		return a->fn_decl == b->fn_decl;
	}
};

struct lto_out_decl_state {
	lto_tree_ref_encoder streams[LTO_N_DECL_STREAMS];
	lto_symtab_encoder_t symtab_node_encoder;
	tree fn_decl;
	bool compressed;
};

typedef struct lto_out_decl_state *lto_out_decl_state_ptr;

struct res_pair {
	ld_plugin_symbol_resolution res;
	unsigned index;
};

struct lto_file_decl_data {
	lto_in_decl_state *current_decl_state;
	lto_in_decl_state *global_decl_state;

	lto_symtab_encoder_t GTY((skip)) symtab_node_encoder;
	hash_table<decl_state_hasher> *function_decl_states;

	const char *GTY((skip)) file_name;
	htab_t GTY((skip)) section_hash_table;
	htab_t GTY((skip)) renaming_hash_table;
	lto_file_decl_data *next;

	uint32 id;

	dynamic_array<res_pair>  GTY((skip)) respairs;
	unsigned max_index;
	gcov_summary GTY((skip)) profile_info;

	hash_map<tree, ld_plugin_symbol_resolution> * GTY((skip)) resolution_map;

	const uint8 *mode_table;
};

typedef struct lto_file_decl_data *lto_file_decl_data_ptr;

struct lto_char_ptr_base {
	char *ptr;
};

struct lto_output_stream {
	lto_char_ptr_base * first_block;
	lto_char_ptr_base * current_block;
	char * current_pointer;
	uint32 left_in_block;
	uint32 block_size;
	uint32 total_size;
};

struct lto_simple_output_block {
	lto_section_type section_type;
	lto_out_decl_state *decl_state;
	lto_output_stream *main_stream;
};

struct string_slot {
	const char *s;
	int len;
	uint32 slot_num;
};

struct string_slot_hasher : nofree_ptr_hash <string_slot> {
	static inline hashval_t hash(const string_slot *);
	static inline bool equal(const string_slot *, const string_slot *);
};

inline hashval_t string_slot_hasher::hash(const string_slot *ds) {
	hashval_t r = ds->len;
	int i;
	for (i = 0; i < ds->len; i++)
		r = r * 67 + (unsigned)ds->s[i] - 113;
	return r;
}

inline bool string_slot_hasher::equal(const string_slot *ds1, const string_slot *ds2) {
	if (ds1->len == ds2->len)
		return memcmp(ds1->s, ds2->s, ds1->len) == 0;
	return 0;
}

struct output_block {
	lto_section_type section_type;
	lto_out_decl_state *decl_state;
	lto_output_stream *main_stream;
	lto_output_stream *string_stream;
	lto_output_stream *cfg_stream;
	hash_table<string_slot_hasher> *string_hash_table;
	symtab_node *symbol;
	const char *current_file;
	int current_line;
	int current_col;
	bool current_sysp;

	streamer_tree_cache_d *writer_cache;
	obstack obstack;
};

struct data_in {
	lto_file_decl_data *file_data;
	const char *strings;
	uint32 strings_len;
	dynamic_array<ld_plugin_symbol_resolution> globals_resolution;
	streamer_tree_cache_d *reader_cache;
	lto_location_cache location_cache;
};

extern lto_input_block * lto_create_simple_input_block(lto_file_decl_data *, lto_section_type, const char **, size_t *);
extern void lto_destroy_simple_input_block(lto_file_decl_data *, lto_section_type, lto_input_block *, const char *, size_t);
extern void lto_set_in_hooks(lto_file_decl_data **, lto_get_section_data_f *, lto_free_section_data_f *);
extern lto_file_decl_data **lto_get_file_decl_data(void);
extern const char *lto_get_section_data(lto_file_decl_data *, lto_section_type, const char *, size_t *, bool decompress = false);
extern const char *lto_get_raw_section_data(lto_file_decl_data *, lto_section_type, const char *, size_t *);
extern void lto_free_section_data(lto_file_decl_data *, lto_section_type, const char *, const char *, size_t, bool decompress = false);
extern void lto_free_raw_section_data(lto_file_decl_data *, lto_section_type, const char *, const char *, size_t);
extern htab_t lto_create_renaming_table(void);
extern void lto_record_renamed_decl(lto_file_decl_data *, const char *, const char *);
extern const char *lto_get_decl_name_mapping(lto_file_decl_data *, const char *);
extern lto_in_decl_state *lto_new_in_decl_state(void);
extern void lto_delete_in_decl_state(lto_in_decl_state *);
extern lto_in_decl_state *lto_get_function_in_decl_state(lto_file_decl_data *, tree);
extern void lto_free_function_in_decl_state(lto_in_decl_state *);
extern void lto_free_function_in_decl_state_for_node(symtab_node *);
extern void lto_section_overrun(lto_input_block *);
extern void lto_value_range_error(const char *, long, long, long);
extern void lto_begin_section(const char *, bool);
extern void lto_end_section(void);
extern void lto_write_data(const void *, uint32);
extern void lto_write_raw_data(const void *, uint32);
extern void lto_write_stream(lto_output_stream *);
extern bool lto_output_decl_index(lto_output_stream *, lto_tree_ref_encoder *, tree, uint32 *);
extern void lto_output_field_decl_index(lto_out_decl_state *, lto_output_stream *, tree);
extern void lto_output_fn_decl_index(lto_out_decl_state *, lto_output_stream *, tree);
extern void lto_output_namespace_decl_index(lto_out_decl_state *, lto_output_stream *, tree);
extern void lto_output_var_decl_index(lto_out_decl_state *, lto_output_stream *, tree);
extern void lto_output_type_decl_index(lto_out_decl_state *, lto_output_stream *, tree);
extern void lto_output_type_ref_index(lto_out_decl_state *, lto_output_stream *, tree);
extern lto_simple_output_block *lto_create_simple_output_block(lto_section_type);
extern void lto_destroy_simple_output_block(lto_simple_output_block *);
extern lto_out_decl_state *lto_new_out_decl_state(void);
extern void lto_delete_out_decl_state(lto_out_decl_state *);
extern lto_out_decl_state *lto_get_out_decl_state(void);
extern void lto_push_out_decl_state(lto_out_decl_state *);
extern lto_out_decl_state *lto_pop_out_decl_state(void);
extern void lto_record_function_out_decl_state(tree, lto_out_decl_state *);
extern void lto_append_block(lto_output_stream *);
extern bool lto_stream_offload_p;
extern const char *lto_tag_name(LTO_tags);
extern bitmap lto_bitmap_alloc(void);
extern void lto_bitmap_free(bitmap);
extern char *lto_get_section_name(int, const char *, lto_file_decl_data *);
extern void print_lto_report(const char *);
extern void lto_streamer_init(void);
extern bool gate_lto_out(void);
extern void lto_check_version(int, int, const char *);
extern void lto_streamer_hooks_init(void);
extern void lto_input_cgraph(lto_file_decl_data *, const char *);
extern void lto_reader_init(void);
extern void lto_input_function_body(lto_file_decl_data *, cgraph_node *, const char *);
extern void lto_input_variable_constructor(lto_file_decl_data *, varpool_node *, const char *);
extern void lto_input_constructors_and_inits(lto_file_decl_data *, const char *);
extern void lto_input_toplevel_asms(lto_file_decl_data *, int);
extern void lto_input_mode_table(lto_file_decl_data *);
extern data_in *lto_data_in_create(lto_file_decl_data *, const char *, unsigned, dynamic_array<ld_plugin_symbol_resolution>);
extern void lto_data_in_delete(data_in *);
extern void lto_input_data_block(lto_input_block *, void *, size_t);
void lto_input_location(location_t *, bitpack_d *, data_in *);
location_t stream_input_location_now(bitpack_d *bp, data_in *data);
tree lto_input_tree_ref(lto_input_block *, data_in *, function *, LTO_tags);
void lto_tag_check_set(LTO_tags, int, ...);
void lto_init_eh(void);
hashval_t lto_input_scc(lto_input_block *, data_in *, unsigned *, unsigned *);
tree lto_input_tree_1(lto_input_block *, data_in *, LTO_tags, hashval_t hash);
tree lto_input_tree(lto_input_block *, data_in *);
extern void lto_register_decl_definition(tree, lto_file_decl_data *);
extern output_block *create_output_block(lto_section_type);
extern void destroy_output_block(output_block *);
extern void lto_output_tree(output_block *, tree, bool, bool);
extern void lto_output_toplevel_asms(void);
extern void produce_asm(output_block *ob, tree fn);
extern void lto_output();
extern void produce_asm_for_decls();
void lto_output_decl_state_streams(output_block *, lto_out_decl_state *);
void lto_output_decl_state_refs(output_block *, lto_output_stream *, lto_out_decl_state *);
void lto_output_location(output_block *, bitpack_d *, location_t);
void lto_output_init_mode_table(void);
extern bool asm_nodes_output;
lto_symtab_encoder_t lto_symtab_encoder_new(bool);
int lto_symtab_encoder_encode(lto_symtab_encoder_t, symtab_node *);
void lto_symtab_encoder_delete(lto_symtab_encoder_t);
bool lto_symtab_encoder_delete_node(lto_symtab_encoder_t, symtab_node *);
bool lto_symtab_encoder_encode_body_p(lto_symtab_encoder_t, cgraph_node *);
bool lto_symtab_encoder_in_partition_p(lto_symtab_encoder_t, symtab_node *);
void lto_set_symtab_encoder_in_partition(lto_symtab_encoder_t, symtab_node *);
bool lto_symtab_encoder_encode_initializer_p(lto_symtab_encoder_t, varpool_node *);
void output_symtab(void);
void input_symtab(void);
void output_offload_tables(void);
void input_offload_tables(bool);
bool referenced_from_other_partition_p(ipa_ref_list *, lto_symtab_encoder_t);
bool reachable_from_other_partition_p(cgraph_node *, lto_symtab_encoder_t);
bool referenced_from_this_partition_p(symtab_node *, lto_symtab_encoder_t);
bool reachable_from_this_partition_p(cgraph_node *, lto_symtab_encoder_t);
lto_symtab_encoder_t compute_ltrans_boundary(lto_symtab_encoder_t encoder);
void select_what_to_stream(void);
void cl_target_option_stream_out(output_block *, bitpack_d *, cl_target_option *);
void cl_target_option_stream_in(data_in *, bitpack_d *, cl_target_option *);
void cl_optimization_stream_out(bitpack_d *, cl_optimization *);
void cl_optimization_stream_in(bitpack_d *, cl_optimization *);
extern void lto_write_options(void);
extern lto_stats_d lto_stats;
extern const char *lto_section_name[];
extern dynamic_array<lto_out_decl_state_ptr> lto_function_decl_states;

static inline bool lto_tag_is_tree_code_p(LTO_tags tag) {
	return tag > LTO_tree_pickle_reference && (unsigned)tag <= MAX_TREE_CODES;
}

static inline bool lto_tag_is_gimple_code_p(LTO_tags tag) {
	return (unsigned)tag >= NUM_TREE_CODES + 2 && (unsigned)tag < 2 + NUM_TREE_CODES + LAST_AND_UNUSED_GIMPLE_CODE;
}

static inline LTO_tags lto_gimple_code_to_tag(gimple_code code) {
	return (LTO_tags) ((unsigned)code + NUM_TREE_CODES + 2);
}

static inline gimple_code lto_tag_to_gimple_code(LTO_tags tag) {
	ISO_ASSERT(lto_tag_is_gimple_code_p(tag));
	return (gimple_code) ((unsigned)tag - NUM_TREE_CODES - 2);
}

static inline LTO_tags lto_tree_code_to_tag(tree_code code) {
	return (LTO_tags) ((unsigned)code + 2);
}

static inline tree_code lto_tag_to_tree_code(LTO_tags tag) {
	ISO_ASSERT(lto_tag_is_tree_code_p(tag));
	return (tree_code) ((unsigned)tag - 2);
}

static inline void lto_tag_check(LTO_tags actual, LTO_tags expected) {
	if (actual != expected)
		internal_error("bytecode stream: expected tag %s instead of %s", lto_tag_name(expected), lto_tag_name(actual));
}

static inline void lto_tag_check_range(LTO_tags actual, LTO_tags tag1, LTO_tags tag2) {
	if (actual < tag1 || actual > tag2)
		internal_error("bytecode stream: tag %s is not in the expected range [%s, %s]", lto_tag_name(actual), lto_tag_name(tag1), lto_tag_name(tag2));
}

static inline void lto_init_tree_ref_encoder(lto_tree_ref_encoder *encoder) {
	encoder->tree_hash_table = new hash_map<tree, unsigned>(251);
	encoder->trees.create(0);
}

static inline void lto_destroy_tree_ref_encoder(lto_tree_ref_encoder *encoder) {
	delete encoder->tree_hash_table;
	encoder->tree_hash_table = NULL;
	encoder->trees.release();
}

static inline uint32 lto_tree_ref_encoder_size(lto_tree_ref_encoder *encoder) {
	return encoder->trees.length();
}

static inline tree lto_tree_ref_encoder_get_tree(lto_tree_ref_encoder *encoder, uint32 idx) {
	return encoder->trees[idx];
}

static inline int lto_symtab_encoder_size(lto_symtab_encoder_t encoder) {
	return encoder->nodes.length();
}
#define LCC_NOT_FOUND	(-1)
static inline int lto_symtab_encoder_lookup(lto_symtab_encoder_t encoder, symtab_node *node) {
	size_t *slot = encoder->map->get(node);
	return (slot && *slot ? *(slot)-1 : LCC_NOT_FOUND);
}

static inline bool lsei_end_p(lto_symtab_encoder_iterator lsei) {
	return lsei.index >= (unsigned)lto_symtab_encoder_size(lsei.encoder);
}

static inline void lsei_next(lto_symtab_encoder_iterator *lsei) {
	lsei->index++;
}

static inline symtab_node *lsei_node(lto_symtab_encoder_iterator lsei) {
	return lsei.encoder->nodes[lsei.index].node;
}

static inline cgraph_node *lsei_cgraph_node(lto_symtab_encoder_iterator lsei) {
	return dyn_cast<cgraph_node *> (lsei.encoder->nodes[lsei.index].node);
}

static inline varpool_node *lsei_varpool_node(lto_symtab_encoder_iterator lsei) {
	return dyn_cast<varpool_node *> (lsei.encoder->nodes[lsei.index].node);
}

static inline symtab_node *lto_symtab_encoder_deref(lto_symtab_encoder_t encoder, int ref) {
	if (ref == LCC_NOT_FOUND)
		return NULL;
	return encoder->nodes[ref].node;
}

static inline lto_symtab_encoder_iterator lsei_start(lto_symtab_encoder_t encoder) {
	lto_symtab_encoder_iterator lsei;
	lsei.encoder = encoder;
	lsei.index = 0;
	return lsei;
}

static inline void lsei_next_in_partition(lto_symtab_encoder_iterator *lsei) {
	lsei_next(lsei);
	while (!lsei_end_p(*lsei) && !lto_symtab_encoder_in_partition_p(lsei->encoder, lsei_node(*lsei)))
		lsei_next(lsei);
}

static inline lto_symtab_encoder_iterator lsei_start_in_partition(lto_symtab_encoder_t encoder) {
	lto_symtab_encoder_iterator lsei = lsei_start(encoder);
	if (lsei_end_p(lsei))
		return lsei;
	if (!lto_symtab_encoder_in_partition_p(encoder, lsei_node(lsei)))
		lsei_next_in_partition(&lsei);
	return lsei;
}

static inline void lsei_next_function_in_partition(lto_symtab_encoder_iterator *lsei) {
	lsei_next(lsei);
	while (!lsei_end_p(*lsei) && (!is_a <cgraph_node *>(lsei_node(*lsei)) || !lto_symtab_encoder_in_partition_p(lsei->encoder, lsei_node(*lsei))))
		lsei_next(lsei);
}

static inline lto_symtab_encoder_iterator lsei_start_function_in_partition(lto_symtab_encoder_t encoder) {
	lto_symtab_encoder_iterator lsei = lsei_start(encoder);
	if (lsei_end_p(lsei))
		return lsei;
	if (!is_a <cgraph_node *>(lsei_node(lsei)) || !lto_symtab_encoder_in_partition_p(encoder, lsei_node(lsei)))
		lsei_next_function_in_partition(&lsei);
	return lsei;
}

static inline void lsei_next_variable_in_partition(lto_symtab_encoder_iterator *lsei) {
	lsei_next(lsei);
	while (!lsei_end_p(*lsei) && (!is_a <varpool_node *>(lsei_node(*lsei)) || !lto_symtab_encoder_in_partition_p(lsei->encoder, lsei_node(*lsei))))
		lsei_next(lsei);
}

static inline lto_symtab_encoder_iterator lsei_start_variable_in_partition(lto_symtab_encoder_t encoder) {
	lto_symtab_encoder_iterator lsei = lsei_start(encoder);
	if (lsei_end_p(lsei))
		return lsei;
	if (!is_a <varpool_node *>(lsei_node(lsei)) || !lto_symtab_encoder_in_partition_p(encoder, lsei_node(lsei)))
		lsei_next_variable_in_partition(&lsei);
	return lsei;
}

DEFINE_DECL_STREAM_FUNCS(TYPE, type)
DEFINE_DECL_STREAM_FUNCS(FIELD_DECL, field_decl)
DEFINE_DECL_STREAM_FUNCS(FN_DECL, fn_decl)
DEFINE_DECL_STREAM_FUNCS(VAR_DECL, var_decl)
DEFINE_DECL_STREAM_FUNCS(TYPE_DECL, type_decl)
DEFINE_DECL_STREAM_FUNCS(NAMESPACE_DECL, namespace_decl)
DEFINE_DECL_STREAM_FUNCS(LABEL_DECL, label_decl)

struct dref_entry {
	tree decl;
	const char *sym;
	uint32 off;
};
extern dynamic_array<dref_entry> dref_queue;
extern FILE *streamer_dump_file;

// Check that tag ACTUAL has one of the given values.  NUM_TAGS is the number of valid tag values to check
void lto_tag_check_set(LTO_tags actual, int ntags, ...) {
	va_list ap;
	va_start(ap, ntags);
	for (int i = 0; i < ntags; i++)
		if ((unsigned)actual == va_arg(ap, unsigned)) {
			va_end(ap);
			return;
		}
	va_end(ap);
	internal_error("bytecode stream: unexpected tag %s", lto_tag_name(actual));
}

void lto_input_data_block(lto_input_block *ib, void *addr, size_t length) {
	uint8 *const buffer = (uint8 *)addr;
	for (size_t i = 0; i < length; i++)
		buffer[i] = streamer_read_uchar(ib);
}

static const char *canon_file_name(const char *string) {
	string_slot s_slot;
	size_t len = strlen(string);
	s_slot.s = string;
	s_slot.len = len;
	string_slot **slot = file_name_hash_table->find_slot(&s_slot, INSERT);
	if (*slot == NULL) {
		char *saved_string = (char *)xmalloc(len + 1);
		string_slot *new_slot = XCNEW(string_slot);
		memcpy(saved_string, string, len + 1);
		new_slot->s = saved_string;
		new_slot->len = len;
		*slot = new_slot;
		return saved_string;
	} else {
		string_slot *old_slot = *slot;
		return old_slot->s;
	}
}
lto_location_cache *lto_location_cache::current_cache;

int lto_location_cache::cmp_loc(const void *pa, const void *pb) {
	const cached_location *a = ((const cached_location *)pa);
	const cached_location *b = ((const cached_location *)pb);
	const char *current_file = current_cache->current_file;
	int current_line = current_cache->current_line;
	if (a->file == current_file && b->file != current_file)
		return -1;
	if (a->file != current_file && b->file == current_file)
		return 1;
	if (a->file == current_file && b->file == current_file) {
		if (a->line == current_line && b->line != current_line)
			return -1;
		if (a->line != current_line && b->line == current_line)
			return 1;
	}
	if (a->file != b->file)
		return strcmp(a->file, b->file);
	if (a->sysp != b->sysp)
		return a->sysp ? 1 : -1;
	if (a->line != b->line)
		return a->line - b->line;
	return a->col - b->col;
}

bool lto_location_cache::apply_location_cache() {
	static const char *prev_file;
	if (!loc_cache.length())
		return false;
	if (loc_cache.length() > 1)
		loc_cache.qsort(cmp_loc);
	for (uint32 i = 0; i < loc_cache.length(); i++) {
		cached_location loc = loc_cache[i];
		if (current_file != loc.file)
			linemap_add(line_table, prev_file ? LC_RENAME : LC_ENTER, loc.sysp, loc.file, loc.line);
		else if (current_line != loc.line) {
			int max = loc.col;
			for (uint32 j = i + 1; j < loc_cache.length(); j++)
				if (loc.file != loc_cache[j].file || loc.line != loc_cache[j].line)
					break;
				else if (max < loc_cache[j].col)
					max = loc_cache[j].col;
			linemap_line_start(line_table, loc.line, max + 1);
		}
		ISO_ASSERT(*loc.loc == BUILTINS_LOCATION + 1);
		if (current_file == loc.file && current_line == loc.line && current_col == loc.col)
			*loc.loc = current_loc;
		else
			current_loc = *loc.loc = linemap_position_for_column(line_table, loc.col);
		current_line = loc.line;
		prev_file = current_file = loc.file;
		current_col = loc.col;
	}
	loc_cache.truncate(0);
	accepted_length = 0;
	return true;
}

void lto_location_cache::accept_location_cache() {
	ISO_ASSERT(current_cache == this);
	accepted_length = loc_cache.length();
}

void lto_location_cache::revert_location_cache() {
	loc_cache.truncate(accepted_length);
}

void lto_location_cache::input_location(location_t *loc, bitpack_d *bp, data_in *data_in) {
	static const char *stream_file;
	static int stream_line;
	static int stream_col;
	static bool stream_sysp;
	ISO_ASSERT(current_cache == this);
	*loc = bp_unpack_int_in_range(bp, "location", 0, RESERVED_LOCATION_COUNT);
	if (*loc < RESERVED_LOCATION_COUNT)
		return;

	bool	file_change = bp_unpack_value(bp, 1);
	bool	line_change = bp_unpack_value(bp, 1);
	bool	column_change = bp_unpack_value(bp, 1);
	if (file_change) {
		stream_file = canon_file_name(bp_unpack_string(data_in, bp));
		stream_sysp = bp_unpack_value(bp, 1);
	}
	if (line_change)
		stream_line = bp_unpack_var_len_unsigned(bp);
	if (column_change)
		stream_col = bp_unpack_var_len_unsigned(bp);

	if (current_file == stream_file && current_line == stream_line && current_col == stream_col && current_sysp == stream_sysp) {
		*loc = current_loc;
		return;
	}
	cached_location entry = {stream_file, loc, stream_line, stream_col, stream_sysp};
	loc_cache.safe_push(entry);
}

void lto_input_location(location_t *loc, bitpack_d *bp, data_in *data_in) {
	data_in->location_cache.input_location(loc, bp, data_in);
}

location_t stream_input_location_now(bitpack_d *bp, data_in *data_in) {
	location_t loc;
	stream_input_location(&loc, bp, data_in);
	data_in->location_cache.apply_location_cache();
	return loc;
}

tree lto_input_tree_ref(lto_input_block *ib, data_in *data_in, function *fn, LTO_tags tag) {
	uint32 ix_u;
	tree result = NULL_TREE;
	lto_tag_check_range(tag, LTO_field_decl_ref, LTO_namelist_decl_ref);
	switch (tag) {
		case LTO_type_ref:
			ix_u = streamer_read_uhwi(ib);
			result = lto_file_decl_data_get_type(data_in->file_data, ix_u);
			break;
		case LTO_ssa_name_ref:
			ix_u = streamer_read_uhwi(ib);
			result = (*SSANAMES(fn))[ix_u];
			break;
		case LTO_field_decl_ref:
			ix_u = streamer_read_uhwi(ib);
			result = lto_file_decl_data_get_field_decl(data_in->file_data, ix_u);
			break;
		case LTO_function_decl_ref:
			ix_u = streamer_read_uhwi(ib);
			result = lto_file_decl_data_get_fn_decl(data_in->file_data, ix_u);
			break;
		case LTO_type_decl_ref:
			ix_u = streamer_read_uhwi(ib);
			result = lto_file_decl_data_get_type_decl(data_in->file_data, ix_u);
			break;
		case LTO_namespace_decl_ref:
			ix_u = streamer_read_uhwi(ib);
			result = lto_file_decl_data_get_namespace_decl(data_in->file_data, ix_u);
			break;
		case LTO_global_decl_ref:
		case LTO_result_decl_ref:
		case LTO_const_decl_ref:
		case LTO_imported_decl_ref:
		case LTO_label_decl_ref:
		case LTO_translation_unit_decl_ref:
		case LTO_namelist_decl_ref:
			ix_u = streamer_read_uhwi(ib);
			result = lto_file_decl_data_get_var_decl(data_in->file_data, ix_u);
			break;
		default:
			ISO_ASSERT(0);
	}
	ISO_ASSERT(result);
	return result;
}

static eh_catch_d *lto_input_eh_catch_list(lto_input_block *ib, data_in *data_in, eh_catch *last_p) {
	eh_catch first	= NULL
	*last_p = first;
	LTO_tags tag = streamer_read_record_start(ib);
	while (tag) {
		tree list;
		eh_catch n;
		lto_tag_check_range(tag, LTO_eh_catch, LTO_eh_catch);

		n = ggc_cleared_alloc<eh_catch_d>();
		n->type_list = stream_read_tree(ib, data_in);
		n->filter_list = stream_read_tree(ib, data_in);
		n->label = stream_read_tree(ib, data_in);

		for (list = n->filter_list; list; list = TREE_CHAIN(list))
			add_type_for_runtime(TREE_VALUE(list));

		if (*last_p)
			(*last_p)->next_catch = n;
		n->prev_catch = *last_p;
		*last_p = n;

		if (first == NULL)
			first = n;
		tag = streamer_read_record_start(ib);
	}
	return first;
}

static eh_region input_eh_region(lto_input_block *ib, data_in *data_in, int ix) {
	LTO_tags tag;
	eh_region r;

	tag = streamer_read_record_start(ib);
	if (tag == LTO_null)
		return NULL;
	r = ggc_cleared_alloc<eh_region_d>();
	r->index = streamer_read_hwi(ib);
	ISO_ASSERT(r->index == ix);

	r->outer = (eh_region)(intptr_t)streamer_read_hwi(ib);
	r->inner = (eh_region)(intptr_t)streamer_read_hwi(ib);
	r->next_peer = (eh_region)(intptr_t)streamer_read_hwi(ib);
	switch (tag) {
		case LTO_ert_cleanup:
			r->type = ERT_CLEANUP;
			break;
		case LTO_ert_try:
		{
			eh_catch_d *last_catch;
			r->type = ERT_TRY;
			r->u.eh_try.first_catch = lto_input_eh_catch_list(ib, data_in, 			&last_catch);
			r->u.eh_try.last_catch = last_catch;
			break;
		}
		case LTO_ert_allowed_exceptions:
		{
			tree l;
			r->type = ERT_ALLOWED_EXCEPTIONS;
			r->u.allowed.type_list = stream_read_tree(ib, data_in);
			r->u.allowed.label = stream_read_tree(ib, data_in);
			r->u.allowed.filter = streamer_read_uhwi(ib);
			for (l = r->u.allowed.type_list; l; l = TREE_CHAIN(l))
				add_type_for_runtime(TREE_VALUE(l));
		}
		break;
		case LTO_ert_must_not_throw:
		{
			r->type = ERT_MUST_NOT_THROW;
			r->u.must_not_throw.failure_decl = stream_read_tree(ib, data_in);
			bitpack_d bp = streamer_read_bitpack(ib);
			r->u.must_not_throw.failure_loc
				= stream_input_location_now(&bp, data_in);
		}
		break;
		default:
			ISO_ASSERT(0);
	}
	r->landing_pads = (eh_landing_pad)(intptr_t)streamer_read_hwi(ib);
	return r;
}

static eh_landing_pad input_eh_lp(lto_input_block *ib, data_in *data_in, int ix) {
	LTO_tags tag;
	eh_landing_pad lp;

	tag = streamer_read_record_start(ib);
	if (tag == LTO_null)
		return NULL;
	lto_tag_check_range(tag, LTO_eh_landing_pad, LTO_eh_landing_pad);
	lp = ggc_cleared_alloc<eh_landing_pad_d>();
	lp->index = streamer_read_hwi(ib);
	ISO_ASSERT(lp->index == ix);
	lp->next_lp = (eh_landing_pad)(intptr_t)streamer_read_hwi(ib);
	lp->region = (eh_region)(intptr_t)streamer_read_hwi(ib);
	lp->post_landing_pad = stream_read_tree(ib, data_in);
	return lp;
}

static void fixup_eh_region_pointers(function *fn, long root_region) {
	unsigned i;
	dynamic_array<eh_region, va_gc> *eh_array = fn->eh->region_array;
	dynamic_array<eh_landing_pad, va_gc> *lp_array = fn->eh->lp_array;
	eh_region r;
	eh_landing_pad lp;
	ISO_ASSERT(eh_array && lp_array);
	ISO_ASSERT(root_region >= 0);
	fn->eh->region_tree = (*eh_array)[root_region];
#define FIXUP_EH_REGION(r) (r) = (*eh_array)[(long) (intptr_t) (r)]
#define FIXUP_EH_LP(p) (p) = (*lp_array)[(long) (intptr_t) (p)]

	FOR_EACH_VEC_ELT(*eh_array, i, r) {

		if (r == NULL)
			continue;
		ISO_ASSERT(i == (unsigned)r->index);
		FIXUP_EH_REGION(r->outer);
		FIXUP_EH_REGION(r->inner);
		FIXUP_EH_REGION(r->next_peer);
		FIXUP_EH_LP(r->landing_pads);
	}

	FOR_EACH_VEC_ELT(*lp_array, i, lp) {

		if (lp == NULL)
			continue;
		ISO_ASSERT(i == (unsigned)lp->index);
		FIXUP_EH_LP(lp->next_lp);
		FIXUP_EH_REGION(lp->region);
	}
#undef FIXUP_EH_REGION
#undef FIXUP_EH_LP
}

void lto_init_eh(void) {
	static bool eh_initialized_p = false;
	if (eh_initialized_p)
		return;

	flag_exceptions = 1;
	init_eh();
	eh_initialized_p = true;
}

static void input_eh_regions(lto_input_block *ib, data_in *data_in, function *fn) {
	long i, root_region, len;
	LTO_tags tag;
	tag = streamer_read_record_start(ib);
	if (tag == LTO_null)
		return;
	lto_tag_check_range(tag, LTO_eh_table, LTO_eh_table);

	lto_init_eh();
	ISO_ASSERT(fn->eh);
	root_region = streamer_read_hwi(ib);
	ISO_ASSERT(root_region == (int)root_region);

	len = streamer_read_hwi(ib);
	ISO_ASSERT(len == (int)len);
	if (len > 0) {
		vec_safe_grow_cleared(fn->eh->region_array, len);
		for (i = 0; i < len; i++) {
			eh_region r = input_eh_region(ib, data_in, i);
			(*fn->eh->region_array)[i] = r;
		}
	}

	len = streamer_read_hwi(ib);
	ISO_ASSERT(len == (int)len);
	if (len > 0) {
		vec_safe_grow_cleared(fn->eh->lp_array, len);
		for (i = 0; i < len; i++) {
			eh_landing_pad lp = input_eh_lp(ib, data_in, i);
			(*fn->eh->lp_array)[i] = lp;
		}
	}

	len = streamer_read_hwi(ib);
	ISO_ASSERT(len == (int)len);
	if (len > 0) {
		vec_safe_grow_cleared(fn->eh->ttype_data, len);
		for (i = 0; i < len; i++) {
			tree ttype = stream_read_tree(ib, data_in);
			(*fn->eh->ttype_data)[i] = ttype;
		}
	}

	len = streamer_read_hwi(ib);
	ISO_ASSERT(len == (int)len);
	if (len > 0) {
		if (targetm.arm_eabi_unwinder) {
			vec_safe_grow_cleared(fn->eh->ehspec_data.arm_eabi, len);
			for (i = 0; i < len; i++) {
				tree t = stream_read_tree(ib, data_in);
				(*fn->eh->ehspec_data.arm_eabi)[i] = t;
			}
		} else {
			vec_safe_grow_cleared(fn->eh->ehspec_data.other, len);
			for (i = 0; i < len; i++) {
				uchar c = streamer_read_uchar(ib);
				(*fn->eh->ehspec_data.other)[i] = c;
			}
		}
	}

	fixup_eh_region_pointers(fn, root_region);
	tag = streamer_read_record_start(ib);
	lto_tag_check_range(tag, LTO_null, LTO_null);
}

static basic_block make_new_block(function *fn, uint32 index) {
	basic_block bb = alloc_block();
	bb->index = index;
	SET_BASIC_BLOCK_FOR_FN(fn, index, bb);
	n_basic_blocks_for_fn(fn)++;
	return bb;
}

static void input_cfg(lto_input_block *ib, data_in *data_in, function *fn) {
	uint32 bb_count;
	basic_block p_bb;
	uint32 i;
	int index;
	init_empty_tree_cfg_for_function(fn);
	init_ssa_operands(fn);
	profile_status_for_fn(fn) = streamer_read_enum(ib, profile_status_d, 	PROFILE_LAST);
	bb_count = streamer_read_uhwi(ib);
	last_basic_block_for_fn(fn) = bb_count;
	if (bb_count > basic_block_info_for_fn(fn)->length())
		vec_safe_grow_cleared(basic_block_info_for_fn(fn), bb_count);
	if (bb_count > label_to_block_map_for_fn(fn)->length())
		vec_safe_grow_cleared(label_to_block_map_for_fn(fn), bb_count);
	index = streamer_read_hwi(ib);
	while (index != -1) {
		basic_block bb = BASIC_BLOCK_FOR_FN(fn, index);
		uint32 edge_count;
		if (bb == NULL)
			bb = make_new_block(fn, index);
		edge_count = streamer_read_uhwi(ib);

		for (i = 0; i < edge_count; i++) {
			uint32 dest_index;
			uint32 edge_flags;
			basic_block dest;
			profile_probability probability;
			edge e;
			dest_index = streamer_read_uhwi(ib);
			probability = profile_probability::stream_in(ib);
			edge_flags = streamer_read_uhwi(ib);
			dest = BASIC_BLOCK_FOR_FN(fn, dest_index);
			if (dest == NULL)
				dest = make_new_block(fn, dest_index);
			e = make_edge(bb, dest, edge_flags);
			e->probability = probability;
		}
		index = streamer_read_hwi(ib);
	}
	p_bb = ENTRY_BLOCK_PTR_FOR_FN(fn);
	index = streamer_read_hwi(ib);
	while (index != -1) {
		basic_block bb = BASIC_BLOCK_FOR_FN(fn, index);
		bb->prev_bb = p_bb;
		p_bb->next_bb = bb;
		p_bb = bb;
		index = streamer_read_hwi(ib);
	}

	ISO_ASSERT(cfun == fn);

	unsigned n_loops = streamer_read_uhwi(ib);
	if (n_loops == 0)
		return;
	loops *loops = ggc_cleared_alloc<loops>();
	init_loops_structure(fn, loops, n_loops);
	set_loops_for_fn(fn, loops);

	for (unsigned i = 1; i < n_loops; ++i) {
		int header_index = streamer_read_hwi(ib);
		if (header_index == -1) {
			loops->larray->quick_push(NULL);
			continue;
		}
		loop *loop = alloc_loop();
		loop->header = BASIC_BLOCK_FOR_FN(fn, header_index);
		loop->header->loop_father = loop;

		loop->estimate_state = streamer_read_enum(ib, loop_estimation, EST_LAST);
		loop->any_upper_bound = streamer_read_hwi(ib);
		if (loop->any_upper_bound)
			loop->nb_iterations_upper_bound = streamer_read_widest_int(ib);
		loop->any_likely_upper_bound = streamer_read_hwi(ib);
		if (loop->any_likely_upper_bound)
			loop->nb_iterations_likely_upper_bound = streamer_read_widest_int(ib);
		loop->any_estimate = streamer_read_hwi(ib);
		if (loop->any_estimate)
			loop->nb_iterations_estimate = streamer_read_widest_int(ib);

		loop->safelen = streamer_read_hwi(ib);
		loop->unroll = streamer_read_hwi(ib);
		loop->dont_vectorize = streamer_read_hwi(ib);
		loop->force_vectorize = streamer_read_hwi(ib);
		loop->simduid = stream_read_tree(ib, data_in);
		place_new_loop(fn, loop);

		flow_loop_tree_node_add(loops->tree_root, loop);
	}

	flow_loops_find(loops);
}

static void input_ssa_names(lto_input_block *ib, data_in *data_in, function *fn) {
	uint32 i, size;
	size = streamer_read_uhwi(ib);
	init_ssanames(fn, size);
	i = streamer_read_uhwi(ib);
	while (i) {
		tree ssa_name, name;
		bool is_default_def;

		while (SSANAMES(fn)->length() < i)
			SSANAMES(fn)->quick_push(NULL_TREE);
		is_default_def = (streamer_read_uchar(ib) != 0);
		name = stream_read_tree(ib, data_in);
		ssa_name = make_ssa_name_fn(fn, name, NULL);
		if (is_default_def) {
			set_ssa_default_def(cfun, SSA_NAME_VAR(ssa_name), ssa_name);
			SSA_NAME_DEF_STMT(ssa_name) = gimple_build_nop();
		}
		i = streamer_read_uhwi(ib);
	}
}

static void fixup_call_stmt_edges_1(cgraph_node *node, gimple **stmts, function *fn) {
#define STMT_UID_NOT_IN_RANGE(uid) (gimple_stmt_max_uid (fn) < uid || uid == 0)
	for (cgraph_edge *cedge = node->callees; cedge; cedge = cedge->next_callee) {
		if (STMT_UID_NOT_IN_RANGE(cedge->lto_stmt_uid))
			fatal_error(input_location, "Cgraph edge statement index out of range");
		cedge->call_stmt = as_a <gcall *>(stmts[cedge->lto_stmt_uid - 1]);
		if (!cedge->call_stmt)
			fatal_error(input_location, "Cgraph edge statement index not found");
	}
	for (cgraph_edge *cedge = node->indirect_calls; cedge; cedge = cedge->next_callee) {
		if (STMT_UID_NOT_IN_RANGE(cedge->lto_stmt_uid))
			fatal_error(input_location, "Cgraph edge statement index out of range");
		cedge->call_stmt = as_a <gcall *>(stmts[cedge->lto_stmt_uid - 1]);
		if (!cedge->call_stmt)
			fatal_error(input_location, "Cgraph edge statement index not found");
	}

	ipa_ref *ref = NULL;
	for (uint32 i = 0; node->iterate_reference(i, ref); i++)
		if (ref->lto_stmt_uid) {
			if (STMT_UID_NOT_IN_RANGE(ref->lto_stmt_uid))
				fatal_error(input_location, "Reference statement index out of range");
			ref->stmt = stmts[ref->lto_stmt_uid - 1];
			if (!ref->stmt)
				fatal_error(input_location, "Reference statement index not found");
		}
}

static void fixup_call_stmt_edges(cgraph_node *orig, gimple **stmts) {
	while (orig->clone_of)
		orig = orig->clone_of;

	function *fn = DECL_STRUCT_FUNCTION(orig->decl);
	if (!orig->thunk.thunk_p)
		fixup_call_stmt_edges_1(orig, stmts, fn);
	if (orig->clones)
		for (cgraph_node *node = orig->clones; node != orig;) {
			if (!node->thunk.thunk_p)
				fixup_call_stmt_edges_1(node, stmts, fn);
			if (node->clones)
				node = node->clones;
			else if (node->next_sibling_clone)
				node = node->next_sibling_clone;
			else {
				while (node != orig && !node->next_sibling_clone)
					node = node->clone_of;
				if (node != orig)
					node = node->next_sibling_clone;
			}
		}
}

static void input_struct_function_base(function *fn, data_in *data_in, lto_input_block *ib) {
	fn->static_chain_decl		= stream_read_tree(ib, data_in);
	fn->nonlocal_goto_save_area = stream_read_tree(ib, data_in);

	int len = streamer_read_hwi(ib);
	if (len > 0) {
		int i;
		vec_safe_grow_cleared(fn->local_decls, len);
		for (i = 0; i < len; i++) {
			tree t = stream_read_tree(ib, data_in);
			(*fn->local_decls)[i] = t;
		}
	}

	fn->curr_properties = streamer_read_uhwi(ib);

	bitpack_d bp = streamer_read_bitpack(ib);
	fn->is_thunk = bp_unpack_value(&bp, 1);
	fn->has_local_explicit_reg_vars = bp_unpack_value(&bp, 1);
	fn->returns_pcc_struct = bp_unpack_value(&bp, 1);
	fn->returns_struct = bp_unpack_value(&bp, 1);
	fn->can_throw_non_call_exceptions = bp_unpack_value(&bp, 1);
	fn->can_delete_dead_exceptions = bp_unpack_value(&bp, 1);
	fn->always_inline_functions_inlined = bp_unpack_value(&bp, 1);
	fn->after_inlining = bp_unpack_value(&bp, 1);
	fn->stdarg = bp_unpack_value(&bp, 1);
	fn->has_nonlocal_label = bp_unpack_value(&bp, 1);
	fn->has_forced_label_in_static = bp_unpack_value(&bp, 1);
	fn->calls_alloca = bp_unpack_value(&bp, 1);
	fn->calls_setjmp = bp_unpack_value(&bp, 1);
	fn->has_force_vectorize_loops = bp_unpack_value(&bp, 1);
	fn->has_simduid_loops = bp_unpack_value(&bp, 1);
	fn->va_list_fpr_size = bp_unpack_value(&bp, 8);
	fn->va_list_gpr_size = bp_unpack_value(&bp, 8);
	fn->last_clique = bp_unpack_value(&bp, sizeof(short) * 8);

	fn->function_start_locus = stream_input_location_now(&bp, data_in);
	fn->function_end_locus = stream_input_location_now(&bp, data_in);
}

static void input_function(tree fn_decl, data_in *data_in, lto_input_block *ib, lto_input_block *ib_cfg) {
	basic_block bb;
	LTO_tags tag = streamer_read_record_start(ib);
	lto_tag_check(tag, LTO_function);

	DECL_RESULT(fn_decl) = stream_read_tree(ib, data_in);
	DECL_ARGUMENTS(fn_decl) = streamer_read_chain(ib, data_in);

	unsigned n_debugargs = streamer_read_uhwi(ib);
	if (n_debugargs) {
		dynamic_array<tree, va_gc> **debugargs = decl_debug_args_insert(fn_decl);
		vec_safe_grow(*debugargs, n_debugargs);
		for (unsigned i = 0; i < n_debugargs; ++i)
			(**debugargs)[i] = stream_read_tree(ib, data_in);
	}

	DECL_INITIAL(fn_decl) = stream_read_tree(ib, data_in);
	unsigned block_leaf_count = streamer_read_uhwi(ib);
	while (block_leaf_count--)
		stream_read_tree(ib, data_in);
	if (!streamer_read_uhwi(ib))
		return;
	push_struct_function(fn_decl);
	function *fn = DECL_STRUCT_FUNCTION(fn_decl);
	init_tree_ssa(fn);

	cfun->gimple_df->in_ssa_p = true;
	gimple_register_cfg_hooks();
	cgraph_node *node = cgraph_node::get(fn_decl);
	if (!node)
		node = cgraph_node::create(fn_decl);
	input_struct_function_base(fn, data_in, ib);
	input_cfg(ib_cfg, data_in, fn);

	input_ssa_names(ib, data_in, fn);

	input_eh_regions(ib, data_in, fn);
	ISO_ASSERT(DECL_INITIAL(fn_decl));
	DECL_SAVED_TREE(fn_decl) = NULL_TREE;

	tag = streamer_read_record_start(ib);
	while (tag) {
		input_bb(ib, tag, data_in, fn, 		node->count_materialization_scale);
		tag = streamer_read_record_start(ib);
	}

	set_gimple_stmt_max_uid(cfun, 0);
	FOR_ALL_BB_FN(bb, cfun) {
		gimple_stmt_iterator gsi;
		for (gsi = gsi_start_phis(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
			gimple *stmt = gsi_stmt(gsi);
			gimple_set_uid(stmt, inc_gimple_stmt_max_uid(cfun));
		}
		for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
			gimple *stmt = gsi_stmt(gsi);
			gimple_set_uid(stmt, inc_gimple_stmt_max_uid(cfun));
		}
	}
	gimple **stmts = (gimple **)xcalloc(gimple_stmt_max_uid(fn), sizeof(gimple *));
	FOR_ALL_BB_FN(bb, cfun) {
		gimple_stmt_iterator bsi = gsi_start_phis(bb);
		while (!gsi_end_p(bsi)) {
			gimple *stmt = gsi_stmt(bsi);
			gsi_next(&bsi);
			stmts[gimple_uid(stmt)] = stmt;
		}
		bsi = gsi_start_bb(bb);
		while (!gsi_end_p(bsi)) {
			gimple *stmt = gsi_stmt(bsi);
			bool remove = false;

			if (!flag_wpa) {
				if (is_gimple_debug(stmt)
					&& (gimple_debug_nonbind_marker_p(stmt)
						? !MAY_HAVE_DEBUG_MARKER_STMTS
						: !MAY_HAVE_DEBUG_BIND_STMTS))
					remove = true;
				if (is_gimple_call(stmt)
					&& gimple_call_internal_p(stmt)) {
					bool replace = false;
					switch (gimple_call_internal_fn(stmt)) {
						case IFN_UBSAN_NULL:
							if ((flag_sanitize
								& (SANITIZE_NULL | SANITIZE_ALIGNMENT)) == 0)
								replace = true;
							break;
						case IFN_UBSAN_BOUNDS:
							if ((flag_sanitize & SANITIZE_BOUNDS) == 0)
								replace = true;
							break;
						case IFN_UBSAN_VPTR:
							if ((flag_sanitize & SANITIZE_VPTR) == 0)
								replace = true;
							break;
						case IFN_UBSAN_OBJECT_SIZE:
							if ((flag_sanitize & SANITIZE_OBJECT_SIZE) == 0)
								replace = true;
							break;
						case IFN_UBSAN_PTR:
							if ((flag_sanitize & SANITIZE_POINTER_OVERFLOW) == 0)
								replace = true;
							break;
						case IFN_ASAN_MARK:
							if ((flag_sanitize & SANITIZE_ADDRESS) == 0)
								replace = true;
							break;
						case IFN_TSAN_FUNC_EXIT:
							if ((flag_sanitize & SANITIZE_THREAD) == 0)
								replace = true;
							break;
						default:
							break;
					}
					if (replace) {
						gimple_call_set_internal_fn(as_a <gcall *>(stmt), IFN_NOP);
						update_stmt(stmt);
					}
				}
			}
			if (remove) {
				gimple_stmt_iterator gsi = bsi;
				gsi_next(&bsi);
				unlink_stmt_vdef(stmt);
				release_defs(stmt);
				gsi_remove(&gsi, true);
			} else {
				gsi_next(&bsi);
				stmts[gimple_uid(stmt)] = stmt;

				if (!cfun->debug_nonbind_markers
					&& gimple_debug_nonbind_marker_p(stmt))
					cfun->debug_nonbind_markers = true;
			}
		}
	}

	{
		edge_iterator ei = ei_start(ENTRY_BLOCK_PTR_FOR_FN(cfun)->succs);
		gimple_set_body(fn_decl, bb_seq(ei_edge(ei)->dest));
	}
	update_max_bb_count();
	fixup_call_stmt_edges(node, stmts);
	execute_all_ipa_stmt_fixups(node, stmts);
	update_ssa(TODO_update_ssa_only_virtuals);
	free_dominance_info(CDI_DOMINATORS);
	free_dominance_info(CDI_POST_DOMINATORS);
	free(stmts);
	pop_cfun();
}

static void input_constructor(tree var, data_in *data_in, lto_input_block *ib) {
	DECL_INITIAL(var) = stream_read_tree(ib, data_in);
}

static void lto_read_body_or_constructor(lto_file_decl_data *file_data, symtab_node *node, const char *data, lto_section_type section_type) {
	int cfg_offset;
	int main_offset;
	int string_offset;
	tree fn_decl = node->decl;
	const lto_function_header *header = (const lto_function_header *) data;
	if (TREE_CODE(node->decl) == FUNCTION_DECL) {
		cfg_offset = sizeof(lto_function_header);
		main_offset = cfg_offset + header->cfg_size;
		string_offset = main_offset + header->main_size;
	} else {
		main_offset = sizeof(lto_function_header);
		string_offset = main_offset + header->main_size;
	}
	data_in *data_in = lto_data_in_create(file_data, data + string_offset, 	header->string_size, vNULL);
	if (section_type == LTO_section_function_body) {
		lto_in_decl_state *decl_state;
		unsigned from;
		gcc_checking_assert(node);

		decl_state = lto_get_function_in_decl_state(file_data, fn_decl);
		ISO_ASSERT(decl_state);
		file_data->current_decl_state = decl_state;

		from = data_in->reader_cache->nodes.length();
		lto_input_block ib_main(data + main_offset, header->main_size, file_data->mode_table);
		if (TREE_CODE(node->decl) == FUNCTION_DECL) {
			lto_input_block ib_cfg(data + cfg_offset, header->cfg_size, file_data->mode_table);
			input_function(fn_decl, data_in, &ib_main, &ib_cfg);
		} else
			input_constructor(fn_decl, data_in, &ib_main);
		data_in->location_cache.apply_location_cache();

		{
			streamer_tree_cache_d *cache = data_in->reader_cache;
			unsigned len = cache->nodes.length();
			unsigned i;
			for (i = len; i-- > from;) {
				tree t = streamer_tree_cache_get_tree(cache, i);
				if (t == NULL_TREE)
					continue;
				if (TYPE_P(t)) {
					ISO_ASSERT(TYPE_CANONICAL(t) == NULL_TREE);
					if (type_with_alias_set_p(t)
						&& canonical_type_used_p(t))
						TYPE_CANONICAL(t) = TYPE_MAIN_VARIANT(t);
					if (TYPE_MAIN_VARIANT(t) != t) {
						ISO_ASSERT(TYPE_NEXT_VARIANT(t) == NULL_TREE);
						TYPE_NEXT_VARIANT(t)
							= TYPE_NEXT_VARIANT(TYPE_MAIN_VARIANT(t));
						TYPE_NEXT_VARIANT(TYPE_MAIN_VARIANT(t)) = t;
					}
				}
			}
		}

		file_data->current_decl_state = file_data->global_decl_state;
	}
	lto_data_in_delete(data_in);
}

void lto_input_function_body(lto_file_decl_data *file_data, cgraph_node *node, const char *data) {
	lto_read_body_or_constructor(file_data, node, data, LTO_section_function_body);
}

void lto_input_variable_constructor(lto_file_decl_data *file_data, varpool_node *node, const char *data) {
	lto_read_body_or_constructor(file_data, node, data, LTO_section_function_body);
}

dynamic_array<dref_entry> dref_queue;

static void lto_read_tree_1(lto_input_block *ib, data_in *data_in, tree expr) {
	streamer_read_tree_bitfields(ib, data_in, expr);
	streamer_read_tree_body(ib, data_in, expr);

	if (DECL_P(expr) && TREE_CODE(expr) != FUNCTION_DECL && TREE_CODE(expr) != TRANSLATION_UNIT_DECL)
		DECL_INITIAL(expr) = stream_read_tree(ib, data_in);

	if ((DECL_P(expr) && TREE_CODE(expr) != FIELD_DECL && TREE_CODE(expr) != DEBUG_EXPR_DECL && TREE_CODE(expr) != TYPE_DECL) || TREE_CODE(expr) == BLOCK) {
		const char *str = streamer_read_string(data_in, ib);
		if (str) {
			uint32 off = streamer_read_uhwi(ib);
			dref_entry e = {expr, str, off};
			dref_queue.safe_push(e);
		}
	}
}
static tree lto_read_tree(lto_input_block *ib, data_in *data_in, LTO_tags tag, hashval_t hash) {
	tree result = streamer_alloc_tree(ib, data_in, tag);
	streamer_tree_cache_append(data_in->reader_cache, result, hash);
	lto_read_tree_1(ib, data_in, result);
	streamer_read_uchar(ib);
	return result;
}

hashval_t lto_input_scc(lto_input_block *ib, data_in *data_in, unsigned *len, unsigned *entry_len) {
	unsigned size = streamer_read_uhwi(ib);
	hashval_t scc_hash = streamer_read_uhwi(ib);
	unsigned scc_entry_len = 1;
	if (size == 1) {
		LTO_tags tag = streamer_read_record_start(ib);
		lto_input_tree_1(ib, data_in, tag, scc_hash);
	} else {
		uint32 first = data_in->reader_cache->nodes.length();
		tree result;
		scc_entry_len = streamer_read_uhwi(ib);

		for (unsigned i = 0; i < size; ++i) {
			LTO_tags tag = streamer_read_record_start(ib);
			if (tag == LTO_null
				|| (tag >= LTO_field_decl_ref && tag <= LTO_global_decl_ref)
				|| tag == LTO_tree_pickle_reference
				|| tag == LTO_integer_cst
				|| tag == LTO_tree_scc)
				ISO_ASSERT(0);
			result = streamer_alloc_tree(ib, data_in, tag);
			streamer_tree_cache_append(data_in->reader_cache, result, 0);
		}

		for (unsigned i = 0; i < size; ++i) {
			result = streamer_tree_cache_get_tree(data_in->reader_cache, 			first + i);
			lto_read_tree_1(ib, data_in, result);
			 streamer_read_uchar(ib);
		}
	}
	*len = size;
	*entry_len = scc_entry_len;
	return scc_hash;
}

tree lto_input_tree_1(lto_input_block *ib, data_in *data_in, LTO_tags tag, hashval_t hash) {
	tree result;
	ISO_ASSERT((unsigned)tag < (unsigned)LTO_NUM_TAGS);
	if (tag == LTO_null) {
		result = NULL_TREE;
	} else if (tag >= LTO_field_decl_ref && tag <= LTO_namelist_decl_ref) {
		result = lto_input_tree_ref(ib, data_in, cfun, tag);
	} else if (tag == LTO_tree_pickle_reference) {
		result = streamer_get_pickled_tree(ib, data_in);
	} else if (tag == LTO_integer_cst) {
		tree type = stream_read_tree(ib, data_in);
		uint32 len = streamer_read_uhwi(ib);
		uint32 i;
		long a[WIDE_INT_MAX_ELTS];
		for (i = 0; i < len; i++)
			a[i] = streamer_read_hwi(ib);
		ISO_ASSERT(TYPE_PRECISION(type) <= MAX_BITSIZE_MODE_ANY_INT);
		result = wide_int_to_tree(type, wide_int::from_array(a, len, TYPE_PRECISION(type)));
		streamer_tree_cache_append(data_in->reader_cache, result, hash);
	} else if (tag == LTO_tree_scc) {
		ISO_ASSERT(0);
	} else {
		result = lto_read_tree(ib, data_in, tag, hash);
	}
	return result;
}

tree lto_input_tree(lto_input_block *ib, data_in *data_in) {
	LTO_tags tag;

	while ((tag = streamer_read_record_start(ib)) == LTO_tree_scc) {
		unsigned len, entry_len;
		lto_input_scc(ib, data_in, &len, &entry_len);

		while (!dref_queue.is_empty()) {
			dref_entry e = dref_queue.pop();
			debug_hooks->register_external_die(e.decl, e.sym, e.off);
		}
	}
	return lto_input_tree_1(ib, data_in, tag, 0);
}

void lto_input_toplevel_asms(lto_file_decl_data *file_data, int order_base) {
	size_t len;
	const char *data = lto_get_section_data(file_data, LTO_section_asm, NULL, &len);
	const lto_simple_header_with_strings *header = (const lto_simple_header_with_strings *) data;
	tree str;
	if (!data)
		return;
	int string_offset = sizeof(*header) + header->main_size;
	lto_input_block ib(data + sizeof(*header), header->main_size, 	file_data->mode_table);
	data_in *data_in = lto_data_in_create(file_data, data + string_offset, header->string_size, vNULL);
	while ((str = streamer_read_string_cst(data_in, &ib))) {
		asm_node *node = symtab->finalize_toplevel_asm(str);
		node->order = streamer_read_hwi(&ib) + order_base;
		if (node->order >= symtab->order)
			symtab->order = node->order + 1;
	}
	lto_data_in_delete(data_in);
	lto_free_section_data(file_data, LTO_section_asm, NULL, data, len);
}

void lto_input_mode_table(lto_file_decl_data *file_data) {
	size_t len;
	const char *data = lto_get_section_data(file_data, LTO_section_mode_table, 	NULL, &len);
	if (!data) {
		internal_error("cannot read LTO mode table from %s", file_data->file_name);
		return;
	}
	uint8 *table = ggc_cleared_vec_alloc<uint8>(1 << 8);
	file_data->mode_table = table;
	const lto_simple_header_with_strings *header = (const lto_simple_header_with_strings *) data;
	int string_offset = sizeof(*header) + header->main_size;
	lto_input_block ib(data + sizeof(*header), header->main_size, NULL);
	data_in *data_in = lto_data_in_create(file_data, data + string_offset, 	header->string_size, vNULL);
	bitpack_d bp = streamer_read_bitpack(&ib);
	table[VOIDmode] = VOIDmode;
	table[BLKmode] = BLKmode;
	uint32 m;
	while ((m = bp_unpack_value(&bp, 8)) != VOIDmode) {
		mode_class mclass = bp_unpack_enum(&bp, mode_class, MAX_MODE_CLASS);
		poly_uint16 size = bp_unpack_poly_value(&bp, 16);
		poly_uint16 prec = bp_unpack_poly_value(&bp, 16);
		machine_mode inner = (machine_mode)bp_unpack_value(&bp, 8);
		poly_uint16 nunits = bp_unpack_poly_value(&bp, 16);
		uint32 ibit = 0, fbit = 0;
		uint32 real_fmt_len = 0;
		const char *real_fmt_name = NULL;
		switch (mclass) {
			case MODE_FRACT:
			case MODE_UFRACT:
			case MODE_ACCUM:
			case MODE_UACCUM:
				ibit = bp_unpack_value(&bp, 8);
				fbit = bp_unpack_value(&bp, 8);
				break;
			case MODE_FLOAT:
			case MODE_DECIMAL_FLOAT:
				real_fmt_name = bp_unpack_indexed_string(data_in, &bp, &real_fmt_len);
				break;
			default:
				break;
		}

		int pass;
		for (pass = 0; pass < 2; pass++)
			for (machine_mode mr = pass ? VOIDmode : GET_CLASS_NARROWEST_MODE(mclass); pass ? mr < MAX_MACHINE_MODE : mr != VOIDmode; pass ? mr = (machine_mode)(mr + 1) : mr = GET_MODE_WIDER_MODE(mr).else_void())
				if (GET_MODE_CLASS(mr) != mclass
					|| maybe_ne(GET_MODE_SIZE(mr), size)
					|| maybe_ne(GET_MODE_PRECISION(mr), prec)
					|| (inner == m
						? GET_MODE_INNER(mr) != mr
						: GET_MODE_INNER(mr) != table[(int)inner])
					|| GET_MODE_IBIT(mr) != ibit
					|| GET_MODE_FBIT(mr) != fbit
					|| maybe_ne(GET_MODE_NUNITS(mr), nunits))
					continue;
				else if ((mclass == MODE_FLOAT || mclass == MODE_DECIMAL_FLOAT)
					&& strcmp(REAL_MODE_FORMAT(mr)->name, real_fmt_name) != 0)
					continue;
				else {
					table[m] = mr;
					pass = 2;
					break;
				}
				uint32 mname_len;
				const char *mname = bp_unpack_indexed_string(data_in, &bp, &mname_len);
				if (pass == 2) {
					switch (mclass) {
						case MODE_VECTOR_BOOL:
						case MODE_VECTOR_INT:
						case MODE_VECTOR_FLOAT:
						case MODE_VECTOR_FRACT:
						case MODE_VECTOR_UFRACT:
						case MODE_VECTOR_ACCUM:
						case MODE_VECTOR_UACCUM:
							if (table[(int)inner] != VOIDmode) {
								table[m] = BLKmode;
								break;
							}
						default:
							fatal_error(UNKNOWN_LOCATION, "unsupported mode %s\n", mname);
							break;
					}
				}
	}
	lto_data_in_delete(data_in);
	lto_free_section_data(file_data, LTO_section_mode_table, NULL, data, len);
}

void lto_reader_init(void) {
	lto_streamer_init();
	file_name_hash_table = new hash_table<freeing_string_slot_hasher>(37);
}

data_in *lto_data_in_create(lto_file_decl_data *file_data, const char *strings, 	unsigned len, dynamic_array<ld_plugin_symbol_resolution> resolutions) {
	data_in *data_in = new (data_in);
	data_in->file_data = file_data;
	data_in->strings = strings;
	data_in->strings_len = len;
	data_in->globals_resolution = resolutions;
	data_in->reader_cache = streamer_tree_cache_create(false, false, true);
	return data_in;
}

void lto_data_in_delete(data_in *data_in) {
	data_in->globals_resolution.release();
	streamer_tree_cache_delete(data_in->reader_cache);
	delete data_in;
}
//-----------------------------------------------------------------------------
//	Filehandler
//-----------------------------------------------------------------------------
#include "iso/iso_files.h"
class GimpleFileHandler : public FileHandler {
	const char*		GetDescription() override { return "gcc gimple"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		ISO_ptr<anything> p(id);
		return p;
	}
} gimple;
