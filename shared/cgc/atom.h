#ifndef ATOM_H
#define ATOM_H

namespace cgc {

typedef unsigned int uint32_t;

struct StringTable {
	static const int INIT_STRING_TABLE_SIZE = 16384;
	char	*strings;
	int		size, nextFree;

	StringTable();
	~StringTable();
	int AddString(const char *s);
};

struct HashTable {
	struct HashEntry {
		int			offset;	// String table offset of string representation
		int			value;	// Atom (symbol) value
		HashEntry() : offset(0), value(0) {}
	};
	HashEntry	*entry;
	uint32_t	size;

	HashTable(uint32_t size);
	~HashTable();
	bool	Empty(uint32_t hashindex);
	bool	Match(StringTable &stable, const char *s, uint32_t hashindex);
	void	Set(StringTable &stable, const char *s, uint32_t hashindex, int value) {
		entry[hashindex].offset	= stable.AddString(s);
		entry[hashindex].value	= value;
	}
	int		FindHashIndex(StringTable &stable, const char *s);
};

struct AtomTable {
	enum { INIT_SIZE = 1024 };

	StringTable stable;	// String table.
	HashTable	htable;	// Hashes string to atom number and token value.
    int			*amap;	// Maps atom value to offset in string table.
	int			*arev;	// Reversed atom for symbol table use.
	int			nextFree;
	int			size;

	AtomTable(int htsize);
	~AtomTable();
	const char *GetAtomString(int atom) {
		return stable.strings + amap[atom];
	}
	const char *GetAtomString2(int atom) {
		return	atom > 0 && atom < nextFree	? GetAtomString(atom)
			:	atom == 0 	? "<null atom>"
			:	atom == -1	? "<EOF>"
			:	"<invalid atom>";
	}
	
	int		LookUpAddStringHash(const char *s);
	void	Grow(int size);
	void	GrowHash(int size);

	// Lookup a string in the hash table.  If it's not there, add it and initialize the atom value in the hash table to the next atom number.
	// Return the atom value of string.
	int 	FindAtom(const char *s) {
		return htable.entry[LookUpAddStringHash(s)].value;
	}

	void	SetAtomValue(int atom, int hashindex) {
		amap[atom] = htable.entry[hashindex].offset;
		htable.entry[hashindex].value = atom;
	}

	int		FindHashIndex(const char *s)  {
		return htable.FindHashIndex(stable, s);
	}

	int		AllocateAtom();
	int		AddAtomFixed(const char *s, int atom);
	
	// Lookup a string in the hash table.  If it's not there, add it and initialize the atom value in the hash table to the next atom number.
	// Return the atom value of string.
	int 	AddAtom(const char *s) {
		if (s == 0)
			return 0;
		int	hashindex 	= LookUpAddStringHash(s);
		int	atom 		= htable.entry[hashindex].value;
		if (atom == 0) {
			atom = AllocateAtom();
			SetAtomValue(atom, hashindex);
		}
		return atom;
	}


	int 	GetReversedAtom(int atom) {
		return atom > 0 && atom < nextFree ? arev[atom] : 0;
	}
};

} //namespace cgc

#endif // ATOM_H
