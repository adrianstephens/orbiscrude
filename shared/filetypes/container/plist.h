#ifndef PLIST_H
#define PLIST_H

#include "base/defs.h"
#include "base/algorithm.h"
#include "base/hash.h"
#include "extra/xml.h"
#include "extra/date.h"
#include "iso/iso.h"

namespace iso {

struct plist_dictionary : anything_machine {
	plist_dictionary()	{}
	plist_dictionary(int n) : anything_machine(n) {}
};
struct plist_array : anything_machine {
	plist_array()		{}
	plist_array(int n) : anything_machine(n) {}
};

//-----------------------------------------------------------------------------
//	helpers for building plist_array and plist_dictionary
//-----------------------------------------------------------------------------

struct iso_index {
	uint64	i;
	iso_index()				: i(0)	{}
	iso_index(uint64 _i)	: i(_i)	{}
};

struct iso_dictionary;
struct iso_array;
template<typename T> struct iso_ref;

struct iso_array : ISO_ptr_machine<plist_array> {
	iso_array()	: ISO_ptr_machine<plist_array>(0) {}
	template<typename... TT> iso_array(const TT&... tt)	: ISO_ptr_machine<plist_array>(0) {
		add(tt...);
	}

	template<typename T, int B> uint64	_add(ISO::ptr<T,B> p)		{ (*this)->Append(p); return (*this)->Count() - 1; }
//	uint64							_add(ISO_ptr_machine<void> p)	{ (*this)->Append(p); return (*this)->Count() - 1; }
	template<typename T> iso_index	add(const T &v)					{ return _add(MakePtr(tag2(), v)); }
	template<typename T> void		add(const iso_ref<T> &v)		{ add((iso_index)v); }
	iso_index						add(const iso_index &v)			{ return add(hex(v.i)); }
	inline iso_index				add(const iso_array &v);
	inline iso_index				add(const iso_dictionary &v);
	template<typename T, typename... TT> void	add(const T &v, const TT&... tt) {
		add(v);
		add(tt...);
	}
};


struct iso_dictionary : ISO_ptr_machine<plist_dictionary> {
	iso_dictionary() : ISO_ptr_machine<plist_dictionary>(0) {}
	iso_dictionary(const iso_dictionary &b) : ISO_ptr_machine<plist_dictionary>(b) {}
	template<typename... TT> iso_dictionary(const TT&... tt) : ISO_ptr_machine<plist_dictionary>(0) {
		add(tt...);
	}
	template<typename T> void	add(const char *name, const T &v)			{ (*this)->Append(ISO::MakePtr(name, v)); }
	template<typename T> void	add(const char *name, const iso_ref<T> &v)	{ add(name, (iso_index)v); }
	void	add(const char *name, const iso_index &v)						{ (*this)->Append(ISO::MakePtr(name, hex(v.i))); }
	void	add(const char *name, const iso_array &v)						{ v.SetID(name); (*this)->Append(v); }
	void	add(const char *name, const iso_dictionary &v)					{ v.SetID(name); (*this)->Append(v); }

	template<typename T> void add(const char *name, const dynamic_array<T> &v) {
		iso_array	a;
		add(name, a);
		for (auto &i : v)
			a.add(i);
	}
	template<typename T, typename... TT> void	add(const char *name, const T &v, const TT&... tt) {
		add(name, v);
		add(tt...);
	}
};

template<typename T> struct _iso_ref : ISO_ptr_machine<T> {
	_iso_ref() {}
	_iso_ref(const T &t) : ISO_ptr_machine<T>(0, t) {}
};
template<> struct _iso_ref<iso_array> : iso_array {
	_iso_ref() {}
	_iso_ref(const iso_array &t) : iso_array(t) {}
	operator iso_array*() { return this; }
};
template<> struct _iso_ref<iso_dictionary> : iso_dictionary {
	_iso_ref() {}
	_iso_ref(const iso_dictionary &t) : iso_dictionary(t) {}
	operator iso_dictionary*() { return this; }
};

template<typename T> struct iso_ref {
	iso_array			*a;
	_iso_ref<T>			p;
	mutable iso_index	x;

	iso_ref()						: a(0) {}
	iso_ref(iso_array &_a)			: a(&_a) {}
	iso_ref(iso_array &_a, T _p)	: a(&_a), p(_p) {}

	void		create(iso_array &_a, T _p) {
		a = &_a;
		p = _p;
	}
	iso_index	fix() const {
		if (!x.i) {
			x = (*a)->Count();
			(*a)->Append(p);
		}
		return x;
	}
	T*	operator->()			{ return p; }
	operator iso_index() const	{ return fix(); }
};

inline iso_index iso_array::add(const iso_array &v)			{ return _add(v); }
inline iso_index iso_array::add(const iso_dictionary &v)	{ return _add(v); }

//-----------------------------------------------------------------------------
//	binary plist
//-----------------------------------------------------------------------------

struct bplist {
	enum Marker {
		Null			= 0x00,
		False			= 0x08,		// false
		True			= 0x09,		// true
		Fill			= 0x0F,		// fill byte
		Int				= 0x10,		// 0001 nnnn		...				// # of bytes is 2^nnnn, big-endian bytes
		Real			= 0x20,		// 0010 nnnn		...				// # of bytes is 2^nnnn, big-endian bytes
		Date			= 0x33,		// 0011 0011		...				// 8 byte float follows, big-endian bytes
		Data			= 0x40,		// 0100 nnnn [int]	...				// nnnn is number of bytes unless 1111 then int count follows, followed by bytes
		ASCIIString		= 0x50,		// 0101 nnnn [int]	...				// ASCII string, nnnn is # of chars, else 1111 then int count, then bytes
		Unicode16String	= 0x60,		// 0110 nnnn [int]	...				// Unicode string, nnnn is # of chars, else 1111 then int count, then big-endian 2-byte uint16_t
		UID				= 0x80,		// 1000 nnnn		...				// nnnn+1 is # of bytes
		Array			= 0xA0,		// 1010 nnnn [int]	objref*			// nnnn is count, unless '1111', then int count follows
		Set				= 0xC0,		// 1100 nnnn [int]	objref*			// nnnn is count, unless '1111', then int count follows
		Dict			= 0xD0		// 1101 nnnn [int]	keyref* objref*	// nnnn is count, unless '1111', then int count follows
	};

	struct Header {
		uint8	magic[6];
		uint8	version[2];
		bool	valid() const { return memcmp(this, "bplist00", 8) == 0; }
	};

	struct Trailer {
		uint8		unused[5];
		uint8		sortVersion;
		uint8		offsetIntSize;
		uint8		objectRefSize;
		uint64be	numObjects;
		uint64be	topObject;
		uint64be	offsetTableOffset;
	};

	struct index {
		uint64	i;
		index()				: i(0)	{}
		index(uint64 _i)	: i(_i)	{}
	};

	struct item_hash {
		uint64	u;
		item_hash(const index &v)				: u(v.i | (uint64(UID) << 56))					{}
		item_hash(bool v)						: u(uint64(v) | (uint64(1) << 60))				{}
		item_hash(uint64 v)						: u(v)											{}
		item_hash(float v)						: u((uint32&)v ^ (uint64(Real) << 56))			{}
		item_hash(double v)						: u((uint64&)v ^ (uint64(Real) << 56))			{}
		item_hash(DateTime v)					: u((uint64&)v ^ (uint64(Date) << 56))			{}
		item_hash(const const_memory_block &v)	: u(crc32(v))						{}
		item_hash(const char *v)				: u(crc32(v) | (uint64(ASCIIString) << 56))		{}
		item_hash(const char16 *v)				: u(crc32(v) | (uint64(Unicode16String) << 56))	{}
		item_hash(const GUID &v)				: u(crc32(v) | (uint64(UID) << 56))	{}
		item_hash(int  v)						: u(v)											{}
		item_hash(const tag2 &v)				: u(v.get_crc32() | (uint64(ASCIIString) << 56))	{}
	};

	static uint64	get_int(istream_ref file, int size);
	static uint64	get_int(istream_ref file);

	static int		calc_intsize(uint64 v)		{ return (v >> 8) == 0 ? 0 : (v >> 16) == 0 ? 1 : (v >> 32) == 0 ? 2 : 3;	}
	static uint8	calc_ref_size(uint64 v)		{ return highest_set_index(v) / 8 + 1;	}

	static void		put_int(ostream_ref file, int size, uint64 v);
	static void		put_int(ostream_ref file, uint64 v);
	static void		put_marker_length(ostream_ref file, Marker m, uint64 v);

	static void		_put_element(ostream_ref file, const index &v);
	static void		_put_element(ostream_ref file, bool v);
	static void		_put_element(ostream_ref file, uint64 v);
	static void		_put_element(ostream_ref file, float v);
	static void		_put_element(ostream_ref file, double v);
	static void		_put_element(ostream_ref file, DateTime v);
	static void		_put_element(ostream_ref file, const const_memory_block &v);
	static void		_put_element(ostream_ref file, const char *v);
	static void		_put_element(ostream_ref file, const char16 *v);
	static void		_put_element(ostream_ref file, const GUID &v);

	static void		_put_element(ostream_ref file, int v)			{ _put_element(file, (uint64)v); }
	static void		_put_element(ostream_ref file, const tag2 &v)	{ _put_element(file, (const char*)v.get_tag()); }
};

//template<> struct hash_type<bplist::item_hash> { typedef bplist::item_hash type; };

struct bplist_reader : bplist, bplist::Header, bplist::Trailer {
	istream_ref file;
	dynamic_array<pair<uint64, ISO::ptr_machine<void>>>	table;
	dynamic_array<uint64>								objects;

	bplist_reader(istream_ref _file) : file(_file) {
		if (file.read(*(bplist::Header*)this) && valid()) {
			file.seek_end(-int64(sizeof(bplist::Trailer)));
			file.read(*(bplist::Trailer*)this);

			file.seek(offsetTableOffset);
			table.resize(numObjects);
			for (int i = 0; i < numObjects; i++)
				table[i].a = get_int(file, offsetIntSize);
		}
	}

	ISO_ptr_machine<void>	_get_element(streamptr pos, tag id);
	ISO_ptr_machine<void>	get_element(uint64 i, tag id = 0) {
		if (table[i].b)
			return table[i].b;
		return table[i].b =_get_element(table[i].a, id);
	}
	ISO_ptr_machine<void>	get_root(tag id = 0) { return get_element(topObject, id); }
};

struct bplist_writer : bplist, bplist::Trailer {
	ostream_ref	file;
	dynamic_array<uint64>	table;
	hash_map<item_hash, uint64>	reuse;

	struct _array;
	struct _dictionary;
	struct ref;

	struct array_dict {
		bplist_writer			&w;
		mutable uint64			i;
		dynamic_array<uint64>	table;
		array_dict(bplist_writer &w) : w(w), i(~uint64(0)) {}

		uint64	put_offset() {
			uint64	x = *this;
			if (w.table[x])
				w.file.seek(w.table[x]);
			else
				w.table[x] = w.file.tell();
			return x;
		}

		uint64				_add(uint64 v)				{ table.push_back(v); return table.size() - 1; }
		template<typename T> uint64	add(const T &v)		{ return _add(w.add(v)); }
		uint64				add(const _array &v)		{ return _add(v); }
		uint64				add(const _dictionary &v)	{ return _add(v); }
//		uint64				add(const ref &v)			{ return _add(v); }
		template<typename T> uint64 add(const dynamic_array<T> &v) {
			_array	a(w);
			uint64	x = add(a);
			for (auto &i : v)
				a.add(i);
			return x;
		}
		operator uint64() const {
			if (!~i)
				i = w._add();
			return i;
		}
	};

	struct _array : array_dict {
		_array(bplist_writer &w) : array_dict(w) {}
		~_array() {
			put_offset();
			w.put_marker_length(w.file, Array, table.size());
			for (auto &i : table)
				bplist::put_int(w.file, w.objectRefSize, i);
			w.file.seek_end(0);
		}
		template<typename T> index	add(const T &v) {
			return array_dict::add(v);
		}
		template<typename T, typename... TT> void	add(const T &v, const TT&... tt) {
			add(v);
			add(tt...);
		}
		uint64	put_here(size_t n) {
			uint64	x = put_offset();
			w.put_marker_length(w.file, Array, n);
			w.file.seek_cur(n * w.objectRefSize);
			return x;
		}
	};

	struct _dictionary : array_dict {
		_dictionary(bplist_writer &_w) : array_dict(_w) {}
		~_dictionary() {
			put_offset();
			int		n	= table.size32() / 2;
			w.put_marker_length(w.file, Dict, n);
			for (int i = 0; i < n; ++i)
				bplist::put_int(w.file, w.objectRefSize, table[i * 2 + 0]);
			for (int i = 0; i < n; ++i)
				bplist::put_int(w.file, w.objectRefSize, table[i * 2 + 1]);
			w.file.seek_end(0);
		}
		void	add_id(const char *name) {
			array_dict::add(name);
			array_dict::_add(0);
		}
		template<typename T> void	set(int i, const T &v) {
			ISO_ASSERT(table[i * 2 + 1] == 0);
			table[i * 2 + 1] = w.add(v);
		}
		template<typename T> void	add(const char *name, const T &v) {
			array_dict::add(name);
			array_dict::add(v);
		}
		template<typename T, typename... TT> void	add(const char *name, const T &v, const TT&... tt) {
			add(name, v);
			add(tt...);
		}
		uint64	put_here(size_t n) {
			uint64	x = put_offset();
			w.put_marker_length(w.file, Dict, n);
			w.file.seek_cur(n * 2 * w.objectRefSize);
			return x;
		}
	};

	struct ref {
		_array	&a;
		uint64	e;
		mutable index	x;
		ref(_array &_a, uint64 _e) : a(_a), e(_e) {}
		operator index() const {
			if (!x.i)
				x = a._add(e);
			return x;
		}
	};

	static uint64	count_array(void *data, const ISO::Type *type, size_t num);
	static uint64	count_elements(ISO_ptr_machine<void> p);

	bplist_writer(ostream_ref _file) : file(_file) {
		clear(*(bplist::Trailer*)this);
		objectRefSize		= 2;
		file.writebuff("bplist00", 8);
	}
	void	set_root(uint64 i) {
		topObject = i;
	}
	void	set_max_elements(uint64 i) {
		objectRefSize		= calc_ref_size(i);
	}
	~bplist_writer() {
		numObjects			= table.size();
		offsetTableOffset	= file.tell();
		offsetIntSize		= calc_ref_size(offsetTableOffset);

		ISO_ASSERT(objectRefSize >= calc_ref_size(numObjects));

		for (auto &i : table)
			 put_int(file, offsetIntSize, i);

		file.write(*(bplist::Trailer*)this);
	}

	uint64	_add(uint64 v = 0)	{ table.push_back(v); return table.size() - 1; }
	uint64	put_array(void *data, const ISO::Type *type, size_t num);
	uint64	add(ISO_ptr_machine<void> p);

	template<typename T>uint64 add(const T &v) {
	#if 0
		uint64	&i = reuse[item_hash(v)];
		if (!i) {
			i = _add(file.tell());
			_put_element(file, v);
		}
	#else
		uint64	i = _add(file.tell());
		_put_element(file, v);
	#endif
		return i;
	}

	_dictionary	dictionary()	{ return *this; }
	_array		array()			{ return *this; }

	template<typename... TT> _dictionary	dictionary(const TT&... tt)	{ _dictionary	d(*this); d.add(tt...); return d; }
	template<typename... TT> _array			array(const TT&... tt)		{ _array		a(*this); a.add(tt...); return a; }
};


//-----------------------------------------------------------------------------
//	ascii plist
//-----------------------------------------------------------------------------

class PLISTreader : public text_mode_reader<istream_ref> {
	void expect(char c, int n)	{
		if (n != c)
			throw_accum("Missing '" << c << "' at line " << line_number);
	}
	void expect(char c)	{
		expect(c, skip_whitespace());
	}
	int skip_whitespace() {
		for (;;) {
			int	c = iso::skip_whitespace(*this);
			if (c != '/')
				return c;

			c = getc();
			if (c == '/') {
				while ((c = getc()) != EOF && c != '\n');
			} else if (c == '*') {
				while ((c = getc()) != EOF && (c != '*' || peekc() != '/'));
				c = getc();
			}
		}
	}
public:
	ISO_ptr<void> get_item(tag id);
	PLISTreader(istream_ref file) : text_mode_reader<istream_ref>(file) {}
};

class PLISTwriter : public text_mode_writer<ostream_ref> {
	int		indent;
	void	new_line();
public:
	void	put_item(ISO::Browser b);
	PLISTwriter(ostream_ref file) : text_mode_writer<ostream_ref>(file, text_mode::UTF8, false), indent(0) {}
};

//-----------------------------------------------------------------------------
//	xml plist
//-----------------------------------------------------------------------------

class XPLISTreader : public XMLreader {
	XMLreader::Data	data;
public:
	XPLISTreader(istream_ref file) : XMLreader(file) {}
	bool	valid() {
		return CheckVersion() >= 0
			&& ReadNext(data) == TAG_DECL && data.Is("DOCTYPE") && str(data.values[1]) == "plist"
			&& ReadNext(data) == XMLreader::TAG_BEGIN && data.Is("plist");
	}
	ISO_ptr<void> get_value(XMLiterator &i, tag id);
	ISO_ptr<void> get_item(tag id) {
		XMLiterator i(*this, data);
		i.Next();
		return get_value(i, id);
	}
};

class XPLISTwriter : public XMLwriter {
public:
	XPLISTwriter(ostream_ref file) : XMLwriter(file, true) {
		puts("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
		ElementBegin("plist");
	}
	~XPLISTwriter() {
		ElementEnd("plist");
	}
	void put_item(ISO::Browser b);
};


} //namespace iso

template<> struct ISO::def<iso::DateTime> : ISO::VirtualT2<iso::DateTime> {
	static ISO::Browser2	Deref(DateTime &a) { return ISO_ptr<string>(0, to_string(a)); }
};
ISO_DEFUSERX(iso::plist_array, anything_machine, "array");
ISO_DEFUSERX(iso::plist_dictionary, anything_machine, "dictionary");

#endif //PLIST_H
