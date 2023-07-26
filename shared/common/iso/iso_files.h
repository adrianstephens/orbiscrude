#ifndef FILES_H
#define FILES_H

#include "iso.h"
#include "filename.h"
#include "stream.h"
#include "base/functions.h"

namespace ISO {

ptr<void>	GetDirectory(tag2 id, const char *path, bool raw = false);
const char*	GetDirectoryPath(const ptr<void> &d);
ptr<void>	GetEnvironment(tag2 id, char **envp);

//-----------------------------------------------------------------------------
//	FileHandlerCache
//-----------------------------------------------------------------------------

typedef callbacks<
	ptr<void>&(const filename&),
	const char*(ptr<void>)
>	FileHandlerCache;
typedef saver<FileHandlerCache>	FileHandlerCacheSave;

struct DefaultFileHandlerCache {
	struct entry : pair<string, ptr<void>>	{
		entry(const filename &fn) : pair<string, ptr<void>>(fn, ISO_NULL) {}
	};
	order_array<entry>	cache;

	ptr<void>&	operator()(const filename &fn) {
		for (auto &i : cache) {
			if (istr(i.a) == fn)
				return i.b;
		}
		return cache.emplace_back(fn).b;
	}
	const char*		operator()(ptr<void> p) {
		for (auto &i : cache) {
			if (i.b == p)
				return i.a;
		}
		return 0;
	}
};

//-----------------------------------------------------------------------------
//	FileHandler
//-----------------------------------------------------------------------------

class FileHandler : public static_list<FileHandler> {
public:
	enum CHECK {	// Check results
		CHECK_DEFINITE_NO	= -2,
		CHECK_UNLIKELY		= -1,
		CHECK_NO_OPINION	= 0,
		CHECK_POSSIBLE		= 1,
		CHECK_PROBABLE		= 2,
	};

	class Include {
		const filename	&fn;
		Include			*back;
		static Include	*top;
		static int		depth;
	public:
		static int		Depth()					{ return depth; }
		static const filename& Top()			{ return top->fn; }
		static filename FindAbsolute(const filename &fn);
		Include(const filename &fn) : fn(fn)	{ back = top; top = this; ++depth; }
		~Include()								{ top = back; --depth;	}
	};

	static const filename& CurrentFilename()			{ return Include::Top(); }
	static filename FindAbsolute(const filename &fn)	{ return Include::FindAbsolute(fn); }

	//	global (static) functions

	static iso_export FileHandler*			Get(const char *ext);
	static iso_export FileHandler*			GetNext(FileHandler *fh);
	static iso_export FileHandler*			GetMIME(const string_param &mime);
	static iso_export FileHandler*			GetNextMIME(FileHandler *fh);
	static iso_export FileHandler*			Identify(iso::istream_ref file, CHECK mincheck = CHECK_PROBABLE);
	static iso_export FileHandler*			Identify(const char *fn, CHECK mincheck = CHECK_PROBABLE);

	static iso_export ptr<void>				Read(tag id, const char *fn);
	static iso_export ptr<void>				Read(tag id, iso::istream_ref file, const char *ext);
	static iso_export ptr64<void>			Read64(tag id, const char *fn);
	static iso_export ptr64<void>			Read64(tag id, iso::istream_ref file, const char *ext);
	template<int B> static iso_export bool	Read(ptr<void, B> &p, tag id, const char *fn);
	template<int B> static iso_export bool	Read(ptr<void, B> &p, tag id, iso::istream_ref file, const char *ext);
	
	static iso_export bool					Write(ptr_machine<void> p, const filename &fn);

	static iso_export FileHandlerCacheSave	PushCache(const FileHandlerCache &_cache);
	static iso_export ptr<void>				CachedRead(const filename &fn, bool external = false);
	static iso_export bool					CachedRead(ptr<void> &p, const filename &fn, bool external = false);
	static iso_export ptr<void>&			AddToCache(const filename &fn);
	static iso_export void					AddToCache(const filename &fn, ptr_machine<void> p);
	static iso_export const char*			FindInCache(ptr_machine<void> p);
	static iso_export void					ExpandVars(const char *s, char *d);

	static iso_export Browser2				ReadExternal(const char *spec);
	static iso_export ptr_machine<void>		ExpandExternals(ptr_machine<void> p, bool force = false);

	static iso_export bool					ModifiedFilenameExists(filename &fn, const char *mod_dir);

	//	per handler (virtual) functions

	template<int B>	ptr64<void>		ReadT(tag id, iso::istream_ref file)							{ return Read(id, file); }
	template<int B>	bool			ReadT(ptr<void, B> &p, tag id, iso::istream_ref file)			{ return p = ReadT<B>(id, file); }
	template<int B>	ptr64<void>		ReadWithFilenameT(tag id, const filename &fn)					{ return ReadWithFilename(id, fn); }
	template<int B>	bool			ReadWithFilenameT(ptr<void, B> &p, tag id, const filename &fn)	{ return p = ReadWithFilenameT<B>(id, fn); }

#if 0
	const char*		(*vGetExt)				(FileHandler *me);
	const char*		(*vGetDescription)		(FileHandler *me);

	int				(*vCheck)				(FileHandler *me, iso::istream_ref file);
	ptr<void>		(*vRead1)				(FileHandler *me, tag id, iso::istream_ref file);
	bool			(*vRead2)				(FileHandler *me, ptr<void> &p, tag id, iso::istream_ref file);
	ptr<void>		(*vReadWithFilename1)	(FileHandler *me, tag id, const filename &fn);
	bool			(*vReadWithFilename2)	(FileHandler *me, ptr<void> &p, tag id, const filename &fn);

	bool			(*vWrite)				(FileHandler *me, ptr<void> p, iso::ostream_ref file);
	bool			(*vWriteWithFilename)	(FileHandler *me, ptr<void> p, const filename &fn);

	const char*		GetExt()													{ return vGetExt(this); }
	const char*		GetDescription()											{ return vGetDescription(this); }

	int				Check(iso::istream_ref file)								{ return vCheck(this, file); }
	ptr<void>		Read(tag id, iso::istream_ref file)							{ return vRead1(this, id, file); }
	bool			Read(ptr<void> &p, tag id, iso::istream_ref file)			{ return vRead2(this, p, id, file); }
	ptr<void>		ReadWithFilename(tag id, const filename &fn)				{ return vReadWithFilename1(this, id, fn); }
	bool			ReadWithFilename(ptr<void> &p, tag id, const filename &fn)	{ return vReadWithFilename2(this, p, id, fn); }

	bool			Write(ptr<void> p, iso::ostream_ref file)					{ return vWrite(this, p, file); }
	bool			WriteWithFilename(ptr<void> p, const filename &fn)			{ return vWriteWithFilename(this, p, fn); }
#else
	virtual	iso_export	const char*		GetExt()								{ return 0; }
	virtual	iso_export	const char*		GetMIME()								{ return 0; }
	virtual	iso_export	const char*		GetDescription();
	virtual	iso_export	const char*		GetCategory()							{ return 0; }
	virtual	iso_export	bool			NeedSeek()								{ return false; }

	virtual	iso_export	int				Check(iso::istream_ref file)			{ return CHECK_NO_OPINION; }
	virtual	iso_export	int				Check(const char *file)					{ return CHECK_NO_OPINION; }

	virtual iso_export	ptr<void>		Read(tag id, iso::istream_ref file)								{ return ISO_NULL; }
	virtual iso_export	bool			Read(ptr<void> &p, tag id, iso::istream_ref file)				{ return p = Read(id, file); }
	virtual	iso_export	ptr<void>		ReadWithFilename(tag id, const filename &fn);
	virtual	iso_export	bool			ReadWithFilename(ptr<void> &p, tag id, const filename &fn)		{ return p = ReadWithFilename(id, fn); }

	virtual iso_export	ptr64<void>		Read64(tag id, iso::istream_ref file)							{ return Read(id, file); }
	virtual iso_export	bool			Read64(ptr64<void> &p, tag id, iso::istream_ref file)			{ return p = Read64(id, file); }
	virtual	iso_export	ptr64<void>		ReadWithFilename64(tag id, const filename &fn);
	virtual	iso_export	bool			ReadWithFilename64(ptr64<void> &p, tag id, const filename &fn)	{ return p = ReadWithFilename64(id, fn); }

	virtual iso_export	bool			Write(ptr<void> p, iso::ostream_ref file)						{ return false; }
	virtual iso_export	bool			WriteWithFilename(ptr<void> p, const filename &fn);
	virtual iso_export	bool			Write64(ptr64<void> p, iso::ostream_ref file)					{ return false; }
	virtual iso_export	bool			WriteWithFilename64(ptr64<void> p, const filename &fn);
#endif

};

template<>	inline ptr64<void> FileHandler::ReadT<64>(tag id, iso::istream_ref file)			{ return Read64(id, file); }
template<>	inline ptr64<void> FileHandler::ReadWithFilenameT<64>(tag id, const filename &fn)	{ return ReadWithFilename64(id, fn); }

//-----------------------------------------------------------------------------
//	DeviceHandler
//-----------------------------------------------------------------------------

class DeviceHandler : public static_list<DeviceHandler> {
public:
	const char *prefix;
	virtual iso_export	ptr<void>	Read(tag id, const char *spec) = 0;
	DeviceHandler(const char *prefix) : prefix(prefix) {}
};

//-----------------------------------------------------------------------------
//	FileVariable
//-----------------------------------------------------------------------------

struct FileVariable : static_list<FileVariable> {
	const char *name, *syntax, *description;
	FileVariable(const char *name, const char *syntax, const char *description)
		: name(name), syntax(syntax), description(description) {}
};


}//namespace ISO

namespace iso {

using ISO::FileHandler;
using ISO::DeviceHandler;

//-----------------------------------------------------------------------------
//	Common IncludeHandler
//-----------------------------------------------------------------------------

class IncludeHandler {
	const filename	*fn;
	const char		*incs;

	struct stack_entry : filename {
		stack_entry	*next;
		stack_entry(const char *f, stack_entry *next) : filename(f), next(next)	{}
	} *stack;

public:
	size_t			open(const char *f, const void **data);
	malloc_block	open(const char *f);
	void			close(const void *data);
	IncludeHandler(const filename *fn, const char *incs);
};

}

#endif//FILES_H
