//
// atom.c
//
#include "atom.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "atom.h"
#include "support.h"

using namespace cgc;
#include "parser.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////// String table: //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

namespace cgc {

static const struct {
	int val;
	const char *str;
} tokens[] = {
	AND_SY,         "&&",
	ASSIGNMINUS_SY, "-=",
	ASSIGNMOD_SY,   "%=",
	ASSIGNPLUS_SY,  "+=",
	ASSIGNSLASH_SY, "/=",
	ASSIGNSTAR_SY,  "*=",
	ASSIGNAND_SY,  	"&=",
	ASSIGNOR_SY,  	"|=",
	ASSIGNXOR_SY,  	"^=",
	ASM_SY,         "asm",
	BOOLEAN_SY,     "bool",
	BREAK_SY,       "break",
	CASE_SY,        "case",
	COLONCOLON_SY,  "::",
	CONST_SY,       "const",
	CONTINUE_SY,    "continue",
	DEFAULT_SY,     "default",
	DISCARD_SY,     "discard",
	DO_SY,          "do",
	EQ_SY,          "==",
	ELSE_SY,        "else",
	EXTERN_SY,      "extern",
	FLOAT_SY,       "float",
	FLOATCONST_SY,  "<float-const>",
	FLOATHCONST_SY, "<floath-const>",
	FLOATXCONST_SY, "<floatx-const>",
	FOR_SY,         "for",
	GE_SY,          ">=",
	GG_SY,          ">>",
	GOTO_SY,        "goto",
	IDENT_SY,       "<ident>",
	PASTING_SY,		"##",
	IF_SY,          "if",
	IN_SY,          "in",
	INLINE_SY,      "inline",
	INOUT_SY,       "inout",
	INT_SY,         "int",
	UNSIGNED_SY,    "unsigned",
	INTCONST_SY,    "<int-const>",
	UINTCONST_SY,    "<uint-const>",
	INTERNAL_SY,    "__internal",
	LE_SY,          "<=",
	LL_SY,          "<<",
	MINUSMINUS_SY,  "--",
	NE_SY,          "!=",
	OR_SY,          "||",
	OUT_SY,         "out",
	PACKED_SY,      "__packed",
	PACKED_SY,      "packed",
	PLUSPLUS_SY,    "++",
	RETURN_SY,      "return",
	STATIC_SY,      "static",
	STRUCT_SY,      "struct",
	STRCONST_SY,    "<string-const>",
	SWITCH_SY,      "switch",
	THIS_SY,        "this",
	TYPEDEF_SY,     "typedef",
	TYPEIDENT_SY,   "<type-ident>",
	TEMPLATEIDENT_SY,"<template-ident>",
	UNIFORM_SY,     "uniform",
	VOID_SY,        "void",
	WHILE_SY,       "while",
	SAMPLERSTATE_SY,"sampler_state",
	TECHNIQUE_SY,	"technique",
	PASS_SY,		"pass",
	COMPILE_SY,		"compile",
	ROWMAJOR_SY,	"row_major",
	COLMAJOR_SY,	"column_major",
	NOINTERP_SY,	"nointerpolation",
	PRECISE_SY,		"precise",
	SHARED_SY,		"shared",
	GROUPSHARED_SY,	"groupshared",
	VOLATILE_SY,	"volatile",
	REGISTER_SY,	"register",
	ENUM_SY,		"enum",
	LOWP_SY,		"lowp",
	MEDIUMP_SY,		"mediump",
	HIGHP_SY,		"highp",
	CBUFFER_SY,		"ConstantBuffer",
	TEMPLATE_SY,	"template",
	OPERATOR_SY,	"operator",
};

///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////// String table: //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

	StringTable::StringTable() : strings((char*)malloc(INIT_STRING_TABLE_SIZE)), size(INIT_STRING_TABLE_SIZE), nextFree(1) {}
	StringTable::~StringTable() { free(strings); }


 // Add a string to a string table.  Return its offset.
int StringTable::AddString(const char *s) {
	int	len = (int)strlen(s);
	if (nextFree + len + 1 >= size) {
		size	= size * 2;
		strings = (char*)realloc(strings, size);
	}
	int	loc = nextFree;
	strcpy(strings + loc, s);
	nextFree += len + 1;
	return loc;
}

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Hash table: ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

#define INIT_HASH_TABLE_SIZE		2047
#define HASH_TABLE_MAX_COLLISIONS	3

// Hash a string with the base hash function.
static uint32_t HashString(const char *s) {
	uint32_t hval = 0;
	while (*s) {
		hval = (hval * 13507 + *s * 197) ^ (hval >> 2);
		s++;
	}
	return hval;
}

// Hash a string with the incrementing hash function.
static int HashString2(const char *s) {
	int hval = 0;
	while (*s) {
		hval = (hval * 729 + *s * 37) ^ (hval >> 1);
		s++;
	}
	return hval;
}

HashTable::HashTable(uint32_t size) : size(size), entry(new HashEntry[size]) {}
HashTable::~HashTable() { delete[] entry; }

bool	HashTable::Empty(uint32_t hashindex) {
	assert(hashindex >= 0 && hashindex < size);
	return entry[hashindex].offset == 0;
}
bool	HashTable::Match(StringTable &stable, const char *s, uint32_t hashindex) {
	return !strcmp(s, stable.strings + entry[hashindex].offset);
}

// Find the hash location for this string.  Return -1 it hash table is full.
int HashTable::FindHashIndex(StringTable &stable, const char *s) {
	uint32_t	hashindex = HashString(s) % size;
	if (Empty(hashindex) || Match(stable, s, hashindex))
		return hashindex;

	int	hashdelta = HashString2(s);
	for (int count = 0; count < HASH_TABLE_MAX_COLLISIONS; ++count) {
		hashindex = uint32_t(hashindex + hashdelta) % size;
		if (Empty(hashindex) || Match(stable, s, hashindex))
			return hashindex;
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Atom table: ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

 // Reverse the bottom 20 bits of a 32 bit int.
static int lReverse(int fval) {
	unsigned int in = fval;
	int result = 0, cnt = 0;

	while (in) {
		result <<= 1;
		result |= in&1;
		in >>= 1;
		cnt++;
	}
	// Don't use all 31 bits.  One million atoms is plenty and sometimes the
	// upper bits are used for other things.
	if (cnt < 20)
		result <<= 20 - cnt;
	return result;
}

AtomTable::AtomTable(int htsize) : htable(htsize <= 0 ? INIT_HASH_TABLE_SIZE : htsize), nextFree(0), size(INIT_SIZE) {
	amap = (int*)malloc(sizeof(int) * size);
	arev = (int*)malloc(sizeof(int) * size);
	memset(amap, 0, size * sizeof(int));
	memset(arev, 0, size * sizeof(int));

	// Initialize lower part of atom table to "<undefined>" atom:
	AddAtomFixed("<undefined>", 0);
	for (int i = 0; i < FIRST_USER_TOKEN_SY; i++)
		amap[i] = amap[0];

	// Add single character tokens to the atom table:
	char t[2];
	t[1] = '\0';
	for (const char *s = "~!@%^&*()-+=|,.<>/?;:[]{}"; *s; ++s) {
		t[0] = *s;
		AddAtomFixed(t, *s);
	}

	// Add multiple character scanner tokens and reserved words:
	for (int i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
		AddAtomFixed(tokens[i].str, tokens[i].val);

	AddAtom("<*** end fixed atoms ***>");
}

AtomTable::~AtomTable() {
	free(amap);
	free(arev);
}

// Grow the atom table to at least "newsize" if it's smaller.
void AtomTable::Grow(int newsize) {
	if (newsize > size) {
		amap = (int*)realloc(amap, sizeof(int) * newsize);
		arev = (int*)realloc(arev, sizeof(int) * newsize);
		memset(amap + size, 0, (newsize - size) * sizeof(int));
		memset(arev + size, 0, (newsize - size) * sizeof(int));
		size = newsize;
	}
}

// Allocate a new atom.  Associated with the "undefined" value of -1.
int AtomTable::AllocateAtom() {
	if (nextFree >= size)
		Grow(nextFree * 2);
	amap[nextFree] = -1;
	arev[nextFree] = lReverse(nextFree);
	return nextFree++;
}

// Add an atom to the hash and string tables if it isn't already there.
// Assign it the atom value of "atom".
int AtomTable::AddAtomFixed(const char *s, int atom) {
	int	hashindex = LookUpAddStringHash(s);
	if (nextFree >= size || atom >= size) {
		int	lsize = size * 2;
		if (lsize <= atom)
			lsize = atom + 1;
		Grow(lsize);
	}
	SetAtomValue(atom, hashindex);
	while (atom >= nextFree) {
		arev[nextFree] = lReverse(nextFree);
		nextFree++;
	}
	return atom;
}

template<typename T> void swap(T &a, T &b) {
	T	t = a;
	a = b;
	b = t;
}

void AtomTable::GrowHash(int size) {
	HashTable	newhtable(size);
	// Add all the existing values to the new atom table preserving their atom values:
	for (int i = 0; i < nextFree; i++) {
		const char	*s		= GetAtomString(i);
		int	oldhashindex	= htable.FindHashIndex(stable, s);
		assert(oldhashindex >= 0);
		int	newhashindex	= newhtable.FindHashIndex(stable, s);
		newhtable.entry[newhashindex] = htable.entry[oldhashindex];
	}
	swap(htable.entry, newhtable.entry);
	swap(htable.size, newhtable.size);
}

// Lookup a string in the hash table.  If it's not there, add it and initialize the atom value in the hash table to 0.
// Return the hash table index.
int AtomTable::LookUpAddStringHash(const char *s) {
	int	hashindex;
	while ((hashindex = FindHashIndex(s)) < 0)
		GrowHash(htable.size * 2 + 1);

	if (htable.Empty(hashindex))
		htable.Set(stable, s, hashindex, 0);

	return hashindex;
}

} //namespace cgc
