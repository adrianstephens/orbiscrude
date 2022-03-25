#include "iso_files.h"
#include "iso_binary.h"
#include "directory.h"

#ifdef USE_HTTP
#include "comms/HTTP.h"
#endif

namespace ISO {

//-----------------------------------------------------------------------------
//	FileHandler - static functions
//-----------------------------------------------------------------------------

FileHandler::Include*	FileHandler::Include::top;
int						FileHandler::Include::depth;
FileHandlerCache		cache;

bool FileHandler::ModifiedFilenameExists(filename &fn, const char *mod_dir) {
	for (filename dir = fn.dir(); !dir.name().blank(); dir = dir.dir()) {
		if (dir.name() == mod_dir)
			return true;
	}

	filename	fn2 = filename(fn.dir()).add_dir(mod_dir).add_dir(fn.name_ext());
	if (fn2.exists()) {
		fn = fn2;
		return true;
	}
	return false;
}

FileHandler *FileHandler::Get(const char *ext) {
	if (ext) {
		ext += ext[0] == '.';
		for (iterator i = begin(); i != end(); ++i) {
			if (const char *ext2 = i->GetExt()) {
				if (istr(ext2) == ext)
					return i;
			}
		}
	}
	return 0;
}

FileHandler *FileHandler::GetNext(FileHandler *fh) {
	const char *ext	= fh->GetExt();
	for (iterator i = fh->next; i != end(); ++i) {
		if (const char *ext2 = i->GetExt()) {
			if (istr(ext2) == ext)
				return i;
		}
	}
	return 0;
}

FileHandler *FileHandler::GetMIME(const char *type) {
	for (iterator i = begin(); i != end(); ++i) {
		if (const char *type2 = i->GetMIME()) {
			if (istr(type2) == type)
				return i;
		}
	}
	return 0;
}
FileHandler *FileHandler::GetNextMIME(FileHandler *fh) {
	const char *type	= fh->GetMIME();
	for (iterator i = fh->next; i != end(); ++i) {
		if (const char *type2 = i->GetMIME()) {
			if (istr(type2) == type)
				return i;
		}
	}
	return 0;
}

FileHandler *FileHandler::Identify(istream_ref file, CHECK mincheck) {
	FileHandler *fh		= 0;
	int			best	= mincheck - 1;
	for (iterator i = begin(); i != end(); ++i) {
		int	c = i->Check(file);
		if (c > best) {
			best	= c;
			fh		= i;
		}
	}
	file.seek(0);
	return fh;
}

FileHandler *FileHandler::Identify(const filename &fn, CHECK mincheck) {
	FileHandler *fh		= 0;
	int			best	= mincheck - 1;
	for (iterator i = begin(); i != end(); ++i) {
		int	c = i->Check(fn);
		if (c > best) {
			best	= c;
			fh		= i;
		}
	}
	return fh;
}


template<int B> bool FileHandler::Read(ptr<void, B> &p, tag id, const filename &fn) {
	ISO_TRACEF("Read %s\n", (const char*)fn);
	Include	inc(fn);

	if (fn.is_url()) {
		auto	sep = fn.slice_to_find(':');
		for (auto &i : DeviceHandler::all()) {
			if (i.prefix == sep) {
				if (p = i.Read(id, sep.end() + 1))
					return true;
			}
		}
#if defined USE_HTTP
		auto	file = HTTPopenURL(HTTP::Context("isoeditor"), fn,
			"Accept: text/html, application/xhtml+xml, image/jxr, */*\r\n"
			"Accept-Language: en-US,en-GB;q=0.7,en;q=0.3\r\n"
			"Accept-Encoding: gzip, deflate\r\n"
			"Connection: Keep-Alive\r\n"
		);

		if (file.exists()) {
			const char	*type = file.headers.get("Content-Type");
			for (FileHandler *fh = GetMIME(type); fh; fh = GetNextMIME(fh)) {
				try {
					if (fh->ReadT<B>(p, id, file))
						return true;
					file.seek(0);
				} catch (const char *error) {
					if (str(error).find(istr(fn)))
						rethrow;
					throw_accum(error << " in " << fn);
				}
			}
		}
		memory_reader_owner	file2(malloc_block(file, file.length()));
		if (FileHandler *fh = Identify(file2, CHECK_PROBABLE)) {
			try {
				if (fh->ReadT<B>(p, id, file2))
					return true;
			} catch (const char *error) {
				throw_accum(error << " in " << fn);
			}
		}
#endif
	} else {
		for (FileHandler *fh = Get(fn.ext()); fh; fh = GetNext(fh)) {
			try {
				if (fh->ReadWithFilenameT<B>(p, id, fn))
					return true;
			} catch (const char *error) {
				if (str(error).find(istr(fn)))
					rethrow;
				throw_accum(error << " in " << fn);
			}
		}

		if (is_dir(fn)) {
			if (FileHandler *fh = Identify(fn, CHECK_PROBABLE)) {
				try {
					if (fh->ReadWithFilenameT<B>(p, id, fn))
						return true;
				} catch (const char *error) {
					throw_accum(error << " in " << fn);
				}
			}
			return p = GetDirectory(id, fn);
		}

		FileInput	file(fn);
		if (FileHandler *fh = Identify(file, CHECK_PROBABLE)) {
			try {
				if (fh->ReadT<B>(p, id, file))
					return true;
			} catch (const char *error) {
				throw_accum(error << " in " << fn);
			}
		}
	}

	return false;
}

template<int B> bool FileHandler::Read(ptr<void, B> &p, tag id, istream_ref file, const char *ext) {
	for (FileHandler *fh = Get(ext); fh; fh = GetNext(fh)) {
		if (fh->ReadT<B>(p, id, file))
			return true;
		file.seek(0);
	}

	if (FileHandler *fh = Identify(file, CHECK_PROBABLE)) {
		if (fh->ReadT<B>(p, id, file))
			return true;
	}

	return false;
}

ptr<void> FileHandler::Read(tag id, const filename &fn) {
	ptr<void>	p;
	Read(p, id, fn);
	return p;
}

ptr<void> FileHandler::Read(tag id, istream_ref file, const char *ext) {
	ptr<void>	p;
	Read(p, id, file, ext);
	return p;
}
ptr64<void> FileHandler::Read64(tag id, const filename &fn) {
	ptr64<void>	p;
	Read(p, id, fn);
	return p;
}

ptr64<void> FileHandler::Read64(tag id, istream_ref file, const char *ext) {
	ptr64<void>	p;
	Read(p, id, file, ext);
	return p;
}
FileHandlerCacheSave FileHandler::PushCache(const FileHandlerCache &_cache) {
	return FileHandlerCacheSave(cache, _cache);
}
ptr<void> &FileHandler::AddToCache(const filename &fn) {
	return cache.get<0>()(fn);
}
void FileHandler::AddToCache(const filename &fn, ptr_machine<void> p) {
	if (cache)
		cache.get<0>()(fn) = p;
}
const char *FileHandler::FindInCache(ptr_machine<void> p) {
	return cache ? cache.get<1>()(p) : 0;
}

static bool FindExt(filename &fn) {
	for (directory_iterator name(filename(fn).set_ext("*")); name; ++name) {
		if (!name.is_dir() && FileHandler::Get(filename(name).ext())) {
			fn = fn.dir().add_dir(name);
			return true;
		}
	}
	return false;
}

filename FileHandler::Include::FindAbsolute(const filename &fn) {
	if (fn.is_url())
		return fn;

	bool	find_ext = fn.ext().blank();
	if (fn.is_relative()) {
		for (Include *i = top; i; i = i->back) {
			filename	fn2 = i->fn.dir().relative(fn);
			if (fn2.exists() || (find_ext && FindExt(fn2)))
				return fn2;
		}
		if (fn.exists()) {
			#ifdef CROSS_PLATFORM
			return get_cwd().add_dir(fn);
			#else
			return fn;
			#endif
		}
		for (Include *i = top; i; i = i->back) {
			filename	fn2 = filename(i->fn).relative(fn.name_ext());
			if (fn2.exists() || (find_ext && FindExt(fn2)))
				return fn2;
		}
	}

	if (fn.exists())
		return fn;

	if (Browser2 mappings = root("variables")["mappings"]) {
		for (int i = 0, n = mappings.Count(); i < n; i++) {
			if (const char *path = mappings[i].GetString()) {
				if (exists(path)) {
					for (filename temp(fn.dir()); ; temp.rem_first()) {
						filename	fn2 = filename(path).add_dir(temp).add_dir(fn.name_ext());
						if (fn2.exists() || (find_ext && FindExt(fn2)))
							return fn2;
						if (temp.blank())
							break;
					}
				}
			}
		}
	}

	for (Include *i = top; i; i = i->back) {
		for (filename temp1(i->fn.dir()); !temp1.blank(); temp1.rem_dir()) {
			for (filename temp(fn.dir()); ; temp.rem_first()) {
				filename	fn2 = filename(temp1).add_dir(temp).add_dir(fn.name_ext());
				if (fn2.exists() || (find_ext && FindExt(fn2)))
					return fn2;
				if (temp.blank())
					break;
			}
		}
	}

	return "";
}

static ptr<void> CachedRead(const filename &fn, bool external) {
	ptr<void> &p = cache.get<0>()(fn);
	if (p) {
		if (!external && p.IsExternal())
			FileHandler::Read(p, 0, fn);
		if (!p.ID())
			p.SetID(fn.name());

	} else if (external) {
		p.CreateExternal(fn, 0);

	} else if (fn.is_url()) {
		FileHandler::Read(p, fn.name(), fn);

	} else {
		filename	fn2		= FileHandler::Include::FindAbsolute(fn);
		if (p = cache.get<0>()(fn2))
			return p;
	#ifdef CROSS_PLATFORM
		if (const char *platform = root("variables")["exportfor"].GetString()) {
			if (FileHandler::ModifiedFilenameExists(fn2, platform)) {
				FileHandler::Read(p, fn.name(), fn2);
				p.SetFlags(Value::SPECIFIC);
				return p;
			}
		}
	#endif
		FileHandler::Read(p, fn.name(), fn2);
	}
	return p;
}

#ifdef CROSS_PLATFORM
char *ExpandVar(const char *start, const char *end, char *dest) {
	memcpy(dest, start, end - start);
	dest[end - start] = 0;
	const char *env = getenv(dest);
	if (env || (env = root("variables")[dest].GetString())) {
		strcpy(dest, env + int(*env == '"'));
		dest += strlen(dest);
		return dest - int(dest[-1] == '"');
	}
	return 0;
}
#endif

void FileHandler::ExpandVars(const char *s, char *d) {
#ifdef CROSS_PLATFORM
	char_set	cs("%$");
	while (const char *p = str(s).find(cs)) {
		memcpy(d, s, p - s);
		d += p - s;
		s = p + 1;
		if (p[0] == '%') {
			if (const char *e = str(p + 1).find('%')) {
				if (char *d1 = ExpandVar(p + 1, e, d)) {
					s = e + 1;
					d = d1;
					continue;
				}
			}
			*d++ = '%';
		} else {
			if (p[1] == '(') {
				if (const char *e = str(p + 2).find(')')) {
					if (char *d1 = ExpandVar(p + 2, e, d)) {
						s = e + 1;
						d = d1;
						continue;
					}
				}
				*d++ = '$';
				*d++ = '(';
			} else if (const char *e = str(p + 1).find(' ')) {
				if (char *d1 = ExpandVar(p + 1, e, d)) {
					s = e;
					d = d1;
					continue;
				}
				*d++ = '$';

			}
		}
		break;
	}
#endif
	strcpy(d, s);
}

bool FileHandler::CachedRead(ptr<void> &p, const filename &fn, bool external) {
	if (cache)
		return p = ISO::CachedRead(fn, external);
	return Read(p, fn.name(), Include::FindAbsolute(fn));
}

ptr<void> FileHandler::CachedRead(const filename &fn, bool external) {
	if (cache)
		return ISO::CachedRead(fn, external);
	return Read(fn.name(), Include::FindAbsolute(fn));
}

Browser2 FileHandler::ReadExternal(const char *spec) {
	filename	fn;
#ifdef CROSS_PLATFORM
	ExpandVars(spec, fn);
	spec = fn;
	char	*sc	= fn.find(';');
	if (sc)
		*sc++ = 0;
	Browser2	b = root("externals");
	int	i = b.GetIndex(spec);
	if (i >= 0) {
		b = b[i];
		if (b.External()) {
			ptr<void>	*p = b;
			*p	= ExpandExternals(*p);
			b	= Browser2(*p);
		}
	} else {
		b = Browser2(CachedRead(fn));
	}
#else
	const char	*sc		= strchr(spec, ';');
	if (sc) {
		memcpy(fn, spec, sc - spec);
		fn[sc - spec]	= 0;
		spec			= fn;
		sc++;
	}
	Browser2 b = root("externals")[spec];
#endif
	if (!b)
		throw_accum("Can't find " << spec);
	if (sc && !(b = b.Parse(sc)))
		throw_accum("Can't find " << sc << " in " << spec);
	if (b.SkipUser().GetType() == REFERENCE)
		b = *b;
	return b;
}

static int ExpandExternals(const Browser2 &b, bool force) {
	auto	type = b.SkipUser().GetTypeDef();
	
	if (type->IsPlainData())
		return 0;

	switch (type->GetType()) {
		case OPENARRAY:
			if (type->SubType()->IsPlainData())
				return 0;
			fallthrough
		case COMPOSITE:
		case ARRAY: {
			int	ret = 0;
			for (int i = 0, n = b.Count(); i < n; i++)
				ret |= ExpandExternals(b[i], force);
			return ret;
		}
		case REFERENCE: {
			auto	ref	= (TypeReference*)type;
			ptr_machine<void>	p	= ref->get(b);
			if (p.Flags() & Value::EXTERNAL) {
				tag2 id = p.ID();
				p		= FileHandler::ReadExternal(p);
				if (id)
					p = Duplicate(id, p);
				ref->set(b, p);
				return 1;

			} else if (p.Flags() & Value::HASEXTERNAL) {
				if (!force && TypeType(p.GetType()) == USER && ((TypeUser*)p.GetType())->KeepExternals())
					return 2;
					
				ptr<void> 	p2 = Duplicate(p);
				int			ret = ExpandExternals(p2, force);
				if (ret & 1) {
					p = p2;
					ref->set(b, p);
				}
				if (!(ret & 2))
					p.ClearFlags(Value::HASEXTERNAL);
				return ret;
			}
			break;
		}
		default:
			break;
	}
	return 0;
}

ptr_machine<void> FileHandler::ExpandExternals(ptr_machine<void> p, bool force) {
	if (p.Flags() & Value::EXTERNAL) {
		tag2 id = p.ID();
		p		= ReadExternal(p);
		if (id)
			p.SetID(id);
	} else if (p.Flags() & Value::HASEXTERNAL) {
		ptr_machine<void> p2 = Duplicate(p);
		int		ret		= ISO::ExpandExternals(Browser2(p2), force);
		if (ret & 1)
			p = p2;
		if (!(ret & 2))
			p.ClearFlags(Value::HASEXTERNAL);
	}
	return p;
}

bool FileHandler::Write(ptr_machine<void> p, const filename &fn) {
	ISO_TRACEF("Write %s\n", (const char*)fn);
	for (FileHandler *fh = Get(fn.ext()); fh; fh = GetNext(fh)) {
		if (fh->WriteWithFilename(p, fn))
			return true;
		ISO_OUTPUT("FileHandler::Write return false\n");
	}
	return false;
}

//-----------------------------------------------------------------------------
//	FileHandler - default virtual implementations
//-----------------------------------------------------------------------------

const char *FileHandler::GetDescription() {
	if (const char *ext = GetExt()) {
		static char	buffer[64];
		_format(buffer, "%s file", ext);
		return buffer;
	}
	return 0;
}

ptr<void> FileHandler::ReadWithFilename(tag id, const filename &fn) {
#if defined USE_HTTP
	if (fn.is_url()) {
		auto	file = HTTPopenURL(HTTP::Context("isoeditor"), fn,
			"Accept: text/html, application/xhtml+xml, image/jxr, */*\r\n"
			"Accept-Language: en-US,en-GB;q=0.7,en;q=0.3\r\n"
			"Accept-Encoding: gzip, deflate\r\n"
			"Connection: Keep-Alive\r\n"
		);
		if (file.exists())
			return Read(id, file);
		return ISO_NULL;
	}
#endif
	FileInput	file(fn);
	if (file.exists())
		return Read(id, file);
	return ISO_NULL;
}

ptr64<void> FileHandler::ReadWithFilename64(tag id, const filename &fn) {
#if defined USE_HTTP
	if (fn.is_url()) {
		auto	file = HTTPopenURL(HTTP::Context("isoeditor"), fn,
			"Accept: text/html, application/xhtml+xml, image/jxr, */*\r\n"
			"Accept-Language: en-US,en-GB;q=0.7,en;q=0.3\r\n"
			"Accept-Encoding: gzip, deflate\r\n"
			"Connection: Keep-Alive\r\n"
		);
		if (file.exists())
			return Read64(id, file);
		return ISO_NULL64;
	}
#endif
	FileInput	file(fn);
	if (file.exists())
		return Read64(id, file);
	return ISO_NULL64;
}

bool FileHandler::WriteWithFilename(ptr<void> p, const filename &fn) {
#ifdef HAS_FILE_WRITER
	FileOutput	file(fn);
	Include		inc(fn);
	return file.exists() && Write(p, file);
#else
	return false;
#endif
}

bool FileHandler::WriteWithFilename64(ptr64<void> p, const filename &fn) {
#ifdef HAS_FILE_WRITER
	FileOutput	file(fn);
	Include		inc(fn);
	return file.exists() && Write64(p, file);
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
//	Directory
//-----------------------------------------------------------------------------

#ifndef	ISO_EDITOR
#define USE_WEAK
#endif

struct Directory : VirtualDefaults {
	string							dirname;
	dynamic_array<string>			names;
#ifdef USE_WEAK
	dynamic_array<WeakRef<void> >	ptrs;
#else
	dynamic_array<ptr<void> >		ptrs;
#endif
	bool	scanned, raw;

	Directory(const char *dirname, bool raw) : dirname(dirname), scanned(false), raw(raw) {}
	void		Init();
	void		Prolog()			{ if (!scanned) Init(); }

	uint32		Count()				{ Prolog(); return uint32(names.size()); }
	const char*	GetName(int i = 0)	{ Prolog(); return names[i];	}
	Browser2	Index(int i);

	int			GetIndex(tag2 id, int from) {
		int			count	= Count();
		if (tag t = id) {
			size_t n = t.find('.') ? 0 : t.length();
			for (int i = from; i < count; i++) {
				if (istr(names[i]) == t)
					return i;
				if (n && istr(names[i].begin(), n) == t && names[i][n] == '.' && FileHandler::Get(names[i] + n + 1))
					return i;
			}
		} else {
			crc32	c = id;
			for (int i = from; i < count; i++) {
				const char *s = names[i], *e;
				if (c == crc32(s) || ((e = str(s).find('.')) && c == const_memory_block(s, e - s)))
					return i;
			}
		}

		return -1;
	}

	bool		Update(const char *s, bool from) {
		if (GetIndex(s, 0) == -1) {
			names.push_back(s);
			ptrs.push_back(ISO_NULL);
		}
		return false;
	}
};

void Directory::Init() {
	for (directory_iterator name(filename(dirname).add_dir(DIRECTORY_ALL)); name; ++name) {
		if (name[0] != '.')
			names.push_back(name);
	}
	ptrs.resize(names.size());
	scanned	= true;
}

Browser2 Directory::Index(int i) {
	Prolog();
#ifdef USE_WEAK
	WeakRef<void>	&w = ptrs[i];
	ptr<void>		p = w;
#else
	ptr<void>		&p = ptrs[i];
#endif
	if (!p) {
		filename	fn	= filename(dirname).add_dir(names[i]);
#ifdef ISO_EDITOR
		if (raw || !(p = FileHandler::CachedRead(fn))) {
			if (is_dir(fn))
				p = GetDirectory(fn.name_ext(), fn, raw);
			else if (raw)
				FileHandler::AddToCache(fn, p = FileHandler::Get("bin")->ReadWithFilename(fn.name_ext(), fn));
		}
#else
		if (FileHandler::CachedRead(p, fn)) {
			if (!p.ID())
				p.SetID(filename(names[i]).name());
		}
#endif
#ifdef USE_WEAK
		w = p;
#endif
	}
	return p;
}

ptr<void> GetDirectory(tag2 id, const char *path, bool raw) {
	return exists(path) ? (ptr<void>)ptr<Directory>(id, path, raw) : ISO_NULL;
}

const char *GetDirectoryPath(const ptr<void> &d) {
	return ((Directory*)d)->dirname;
}

ISO_DEFUSERVIRT(Directory);

//-----------------------------------------------------------------------------
//	Environment variable access
//-----------------------------------------------------------------------------

class Environment : public VirtualDefaults {
	char		**envp;
	int			count;
public:
	Environment(char **_envp) : envp(_envp), count(0) {
		while (char *p = envp[count]) {
			*str(p).find('=') = 0;
			count++;
		}
	}
	int			Count()			{ return count; }
	tag			GetName(int i)	{ return envp[i]; }
	Browser2	Index(int i)	{ return ptr<const char*>(envp[i], envp[i] + strlen(envp[i]) + 1);	}
	int				GetIndex(tag id, int from) {
		for (int i = from; envp[i]; i++) {
			if (istr(envp[i]) == id)
				return i;
		}
		return -1;
	}
};

ptr<void> GetEnvironment(tag2 id, char **envp) {
	return ptr<Environment>(id, envp);
}

ISO_DEFVIRT(Environment);

}//namespace ISO

namespace iso {

//-----------------------------------------------------------------------------
//	Common IncludeHandler
//-----------------------------------------------------------------------------

size_t IncludeHandler::open(const char *f, const void **data) {
	filename	f2(f);
	if (f2.is_relative() && (fn || stack))
		f2 = filename(*(stack ? stack : fn)).rem_dir().add_dir(f);

	for (const char *p = incs; p && !exists(f2);) {
		if (const char	*sc = strchr(p, ';')) {
			f2	= filename(str(p, sc - p));
			p	= sc + 1;
		} else {
			FileHandler::ExpandVars(p, f2);
			p	= 0;
		}
		if (f2.is_relative())
			f2 = filename(*(stack ? stack : fn)).rem_dir().add_dir(f2);
		f2.add_dir(f);
	}

	FileInput	file(f2);
	if (!file.exists())
		return 0;

	FileHandler::AddToCache(f2);

	size_t	len		= (size_t)file.length();
	void	*buffer	= iso::malloc(len);
	file.readbuff(buffer, len);
	*data	= buffer;

	stack = new stack_entry(f2, stack);
	return len;
}

void IncludeHandler::close(const void *data) {
	iso::free(const_cast<void*>(data));
	stack_entry	*e = stack;
	stack	= e->next;
	delete e;
}

IncludeHandler::IncludeHandler(const filename *fn, const char *incs) : fn(fn), incs(incs), stack(0)	{}


}//namespace iso
