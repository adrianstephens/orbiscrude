#ifndef REGISTRY_H
#define REGISTRY_H

#include "base/strings.h"

namespace iso { namespace win {

//-----------------------------------------------------------------------------
//	Registry
//-----------------------------------------------------------------------------

class RegKey {
	HKEY		h;

	class enumerator {
	protected:
		HKEY		h;
		DWORD		index, name_len;
		char		*name;
		enumerator(HKEY h, DWORD name_len, DWORD index) : h(h), index(index), name_len(name_len + 1), name(0) {}
		~enumerator()					{ free(name);	}
		char*			NeedName()		{ if (!name) name = (char*)malloc(name_len); return name; }
	public:
		typedef random_access_iterator_t	iterator_category;
		bool	operator==(const enumerator &b)	const	{ return index == b.index; }
		bool	operator!=(const enumerator &b)	const	{ return index != b.index; }
		int		operator-(const enumerator &b)	const	{ return index - b.index; }
		int		Index()							const	{ return index; }
	};

	struct _class {
		HKEY	h;
		_class(HKEY	h) : h(h) {}
		size_t	string_len() const {
			DWORD			size;
			RegQueryInfoKeyA(h, NULL, &size, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			return size;
		}
		size_t	string_get(char *s, size_t len) const {
			if (len) {
				DWORD size = DWORD(len + 1);
				RegQueryInfoKeyA(h, s, &size, 0, 0, 0, 0, 0, 0, 0, 0, 0);
				len = size - 1;
			}
			return len;
		}
	};

public:
	enum TYPE {
		none						= REG_NONE,
		sz							= REG_SZ,
		expand_sz					= REG_EXPAND_SZ,
		binary						= REG_BINARY,
		uint32						= REG_DWORD,
		uint32be					= REG_DWORD_BIG_ENDIAN,
		link						= REG_LINK,
		multi_sz					= REG_MULTI_SZ,
		resource_list				= REG_RESOURCE_LIST,
		full_resource_descriptor	= REG_FULL_RESOURCE_DESCRIPTOR,
		resource_requirements_list	= REG_RESOURCE_REQUIREMENTS_LIST,
		uint64						= REG_QWORD,
	};

	struct Value {
		HKEY		h;
		const char	*name;
		TYPE		type;
		DWORD		size;

		struct _string_getter {
			const Value	*v;
			_string_getter(const Value *v) : v(v) {}
			size_t	string_len() const {
				return v->size ? v->size - 1 : 0;
			}
			size_t	string_get(char *s, size_t len) const {
				if (len) {
					DWORD size2 = DWORD(len + 1);
					RegQueryValueExA(v->h, v->name, NULL, NULL, (BYTE*)s, &size2);
					len = size2 - 1;
				}
				return len;
			}
			size_t	string_get(char16 *s, size_t len) const {
				if (len) {
					DWORD	size2 = DWORD((len + 1) * 2);
					RegQueryValueExW(v->h, str16(v->name), NULL, NULL, (BYTE*)s, &size2);
					len = size2 / 2 - 1;
				}
				return len;
			}
		};

		bool	get_raw(void *p, size_t size) 					const	{ return RegQueryValueExA(h, name, 0, 0, (BYTE*)p, (DWORD*)&size) == ERROR_SUCCESS; }
		bool	set_raw(TYPE type, const void *p, size_t size)	const	{ return RegSetValueExA(h, name, 0, type, (BYTE*)p, DWORD(size)) == ERROR_SUCCESS; }
		string_getter<_string_getter>	get_text()				const	{ return _string_getter(this); }

		Value(HKEY h, const char *name) : h(h), name(name), type(none), size(0) {
			RegQueryValueExA(h, name, 0, (DWORD*)&type, 0, &size);
		}
		Value(HKEY h, char *name, DWORD name_len, DWORD index) : h(h), name(name) {
			if (RegEnumValueA(h, index, name, &name_len, 0, (DWORD*)&type, 0, &size) != ERROR_SUCCESS) {
				name		= 0;
				type		= none;
				size		= 0;
			}
		}
		bool		exists()								const	{ return type != none; }
		const char	*Name()									const	{ return name;	}

		bool	set(const void *p, size_t size)				const	{ return set_raw(binary, p, size); }
		bool	set_expand(const char *name, const char *v)	const	{ return set_raw(expand_sz, v, strlen(v) + 1); }
		bool	set_int(void *p, size_t size)				const	{ iso::uint64 x = 0; TYPE type = binary; if (size <= 8) { memcpy(&x, p, size); p = &x; type = size <= 4 ? uint32 : uint64; } return set_raw(type, p, size); }

		bool	set(iso::uint32 v)							const	{ return set_raw(uint32, &v, 4);		}
		bool	set(iso::uint32be v)						const	{ return set_raw(uint32be, &v, 4);		}
		bool	set(iso::uint64 v)							const	{ return set_raw(uint64, &v, 8);		}
		bool	set(const char *v)							const	{ if (!v) v = ""; return set_raw(type == expand_sz ? expand_sz : sz, v, strlen(v) + 1); }
		bool	set(const multi_string_base<char> &v)		const	{ return set_raw(multi_sz, *v.begin(), v.length() + 1);	}

		template<typename T> enable_if_t<has_int_rep<T>, bool>	set(T v) const {
			return set(uint_type_tmin<T, iso::uint32>(v));
		}
		template<typename T> enable_if_t<has_int_rep<T>, bool>	get(T &v) const {
			uint_type_tmin<T, iso::uint32>	v2 = 0;
			if (!get_raw(&v2, sizeof(v2)))
				return false;
			v = (T)v2;
			return true;
		}
		template<typename T> enable_if_t<is_int<T>, bool>	get_clear(T &v)	const {
			v = 0;
			return get_raw(&v, sizeof(T));
		}
		template<typename T, typename B> void	get_clear(string_base<B> &v) const {
			static_cast<T&>(v) = get_text();
		}

		template<typename T> T	get()	const	{ T t; get_clear<T>(t); return t; }
		getter<const Value>		get()	const	{ return *this; }


		template<> multi_string_alloc<char>	get<multi_string_alloc<char> >() const {
			DWORD	size2	= size;
			multi_string_alloc<char>	val(size2 + (type != multi_sz));
			char	*p	= val.begin().p;
			RegQueryValueExA(h, name, NULL, NULL, (BYTE*)p, &size2);
			if (type != multi_sz) {
				p[size2] = 0;
				for (parts<';'>::iterator i = p; *i; ++i)  {
					if ((string_count(i.p, '"', i.n) & 1) == 0)
						*unconst(i.n) = 0;
				}
			}
			return val;
		}

		template<typename T> bool	operator=(const T &v)	const	{ return set(v); }
		operator string()									const	{ return get_text(); }
	};

	class Values {
		HKEY	h;
		DWORD	num, name_len;
	public:
		struct iterator : enumerator {
			iterator(HKEY h, DWORD name_len, DWORD index) : enumerator(h, name_len, index) {}
			Value				operator*()				{ return Value(h, NeedName(), name_len, index); }
			ref_helper<Value>	operator->()			{ return operator*(); }
			iterator&			operator++()			{ ++index; return *this; }
			iterator			operator+(int i) const	{ return iterator(h, name_len, index + i); }
			Value				operator[](int i)		{ return *(*this + i); }
		};
		Values(HKEY	h) : h(h), num(0), name_len(0)		{ RegQueryInfoKey(h, 0, 0, 0, 0, 0, 0, &num, &name_len, 0, 0, 0); }
		iterator	begin()						const	{ return iterator(h, name_len, 0);	}
		iterator	end()						const	{ return iterator(h, name_len, num);	}
		size_t		size()						const	{ return num; }
		Value		operator[](int i)			const	{ return *iterator(h, name_len, i);	}
		Value		operator[](const char *s)	const	{ return Value(h, s); }
	};

	class SubKeys {
		HKEY	h;
		DWORD	access;
		DWORD	num, name_len;
	public:
		struct iterator : enumerator {
			DWORD	access;
			iterator(HKEY h, DWORD access, DWORD name_len, DWORD index) : enumerator(h, name_len, index), access(access) {}
			const char			*Name()						{ DWORD name_len2 = name_len; RegEnumKeyExA(h, index, NeedName(), &name_len2, 0, 0, 0, 0); return name;	}
			iterator&			operator++()				{ ++index; return *this; }
			HKEY				operator*()					{ HKEY _h; return RegCreateKeyExA(h, Name(), 0, NULL, 0, access, NULL, &_h, NULL) == ERROR_SUCCESS ? _h : 0; }
			ref_helper<RegKey>	operator->()				{ return operator*(); }
			iterator			operator+(int i)	const	{ return iterator(h, access, name_len, index + i); }
			HKEY				operator[](int i)	const	{ return *(*this + i); }
		};
		SubKeys(HKEY h, DWORD access = KEY_READ) : h(h), access(access), num(0), name_len(0) { RegQueryInfoKey(h, 0, 0, 0, &num, &name_len, 0, 0, 0, 0, 0, 0); }
		iterator		begin()					const	{ return iterator(h, access, name_len, 0);	 }
		iterator		end()					const	{ return iterator(h, access, name_len, num); }
		size_t			size()					const	{ return num; }
		RegKey			operator[](int i)		const	{ return *iterator(h, access, name_len, i); }
	};

	RegKey()			: h(0)							{}
	RegKey(HKEY h)		: h(h)							{}
	RegKey(HKEY h, const char *s, DWORD access = KEY_READ)	{ if (RegCreateKeyExA(h, s, 0, NULL, 0, access, NULL, &this->h, NULL) != ERROR_SUCCESS) this->h = 0; }
	RegKey(const RegKey &r)								{ DuplicateHandle(GetCurrentProcess(), r.h, GetCurrentProcess(), (HANDLE*)&h, 0, FALSE, DUPLICATE_SAME_ACCESS); }
	~RegKey()											{ ISO_VERIFY(!h || RegCloseKey(h) == ERROR_SUCCESS); }
	void			operator=(const RegKey &r)			{ if (&r != this) {RegCloseKey(h); DuplicateHandle(GetCurrentProcess(), r.h, GetCurrentProcess(), (HANDLE*)&h, 0, FALSE, DUPLICATE_SAME_ACCESS); } }
	void			operator=(HKEY _h)					{ if (h != _h) { RegCloseKey(h); h = _h; } }
	operator		HKEY()						const	{ return h; }
	HKEY			detach()					const	{ HKEY _h = h; const_cast<RegKey*>(this)->h = 0; return _h;	}

#ifdef USE_RVALUE_REFS
	RegKey(RegKey &&r)	: h(r.detach())					{}
	void			operator=(RegKey &&r)				{ swap(h, r.h); }
#endif

	RegKey			subkey(const char *s, DWORD access = KEY_READ) const {
		HKEY _h;
		return RegCreateKeyExA(h, s, 0, NULL, 0, access, NULL, &_h, NULL) == ERROR_SUCCESS ? _h : 0;
	}
	RegKey			subkey(int i, DWORD access = KEY_READ) const {
		return SubKeys(h, access)[i];
	}
	auto			begin(DWORD access = KEY_READ)			const	{ return SubKeys(h, access).begin(); }
	auto			end(DWORD access = KEY_READ)			const	{ return SubKeys(h, access).end(); }
	size_t			size()									const	{ return SubKeys(h).size(); }
	RegKey			operator[](int i)						const	{ return subkey(i); }
	RegKey			operator[](const char *s)				const	{ return subkey(s); }
	bool			HasSubKey(const char *s)				const	{ HKEY _h; return s && (RegOpenKeyExA(h, s, 0, STANDARD_RIGHTS_READ, &_h) == ERROR_SUCCESS && RegCloseKey(_h) == ERROR_SUCCESS); }

	Value			value(const char *s = 0)				const	{ return Value(h, s); }
	Values			values()								const	{ return h; }
	auto			values_begin()							const	{ return values().begin(); }
	auto			values_end()							const	{ return values().end(); }

	string_getter<_class>	GetClass()	const { return _class(h); }
};

} } //namespace iso::win

#endif	//REGISTRY_H
