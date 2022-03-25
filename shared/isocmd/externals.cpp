#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "base/algorithm.h"
#include "utilities.h"

using namespace iso;

static ISO_ptr<void> _ReadExternal(char *path) {
	char *field = strchr(path, ';');
	if (field)
		*field++ = 0;
	ISO::Browser2 b = FileHandler::CachedRead(path);
	if (field)
		b = b.Parse(field);
	if (!b) {
		if (field)
			throw_accum("Can't find " << path << " in " << field);
		else
			throw_accum("Can't read " << path);
	}
	if (b.IsPtr())
		return b;
	if (b.GetType() == ISO::REFERENCE)
		return *b;
	return ISO_NULL;
}

static filename _GetPlatformFilename(const filename &fn) {
	const char *platform = ISO::root("variables")["exportfor"].GetString();
	if (!platform)
		return fn;
	for (filename dir = fn.dir(); *dir.name(); dir = dir.dir()) {
		if (dir.name() == platform)
			return fn;
	}
	filename platform_fn = filename(fn.dir()).add_dir(platform).add_dir(fn.name_ext());
	if (!exists(platform_fn))
		return fn;
	return platform_fn;
}

// Indent
struct Indent {
	static size_t size;
	Indent(const char *msg = NULL, int verbosity = 0) {
		static int verbose = ISO::root("variables")["verbose"].GetInt();
		if (msg && verbose > verbosity) {
			for (size_t i = 0; i < size; ++i)
				putchar(' ');
			puts(msg);
		}
		size += 2;
	}
	~Indent() {
		size -= 2;
	}
};
size_t Indent::size = 0;

// ReferenceHistory
struct ReferenceHistory : dynamic_array<void*> {
	bool Append(void *p) {
		// trivial
		if (empty() || p > back())
			push_back(p);
		else {
			// search
			auto iter = lower_bound(begin(), end(), p);
			if (iter != end() && *iter == p)
				return false;
			insert(iter, p);
		}
		return true;
	}
};

// Reference, References
struct Reference {
	ISO_ptr<void>	p;
	const ISO::Type	*type;
	string			path;
	bool			recurse;

	Reference(const ISO_ptr<void> &p, const ISO::Type *type = NULL, const char *path = NULL, bool recurse = true)
		: p(p)
		, type(type ? type : p.GetType())
		, path(path)
		, recurse(recurse)
	{}
};

struct References : public dynamic_array<Reference*> {
	~References() {
		for (auto i : *this)
			delete i;
	}
	auto At(void *p) {
		return p > back()->p ? end() : lower_boundc(*this, p, [](const Reference *ref, void *p) { return ref->p < p;});
	}
	Reference* Find(const ISO_ptr<void> &p) {
		auto at = At(p);
		return at != end() && (*at)->p == p ? *at : NULL;
	}
	Reference* Find(const char *path) {
		auto at = find_if(*this, [path](Reference *r) { return r->path && istr(r->path) == path; });
		return at != end() ? *at : NULL;
	}
};

struct _IndexReferences {
	References		&refs;
	size_t			xrefs;
	ISO::Browser	user;

	void Enum(ISO::Browser b) {
		for (;;) {
			switch (b.GetType()) {
				case ISO::COMPOSITE:
				case ISO::ARRAY:
				case ISO::OPENARRAY: {
					for (uint32 i = 0, count = b.Count(); i < count; ++i)
						Enum((user = b[i]).SkipUser());
					return;
				}
				case ISO::REFERENCE: {
					// skip
					ISO_ptr<void> *pp = b;
					if (!*pp)
						return;
					// lookup
					auto at = refs.At(*pp);
					if (at != refs.end() && (*at)->p == *pp)
						return;
					// enumerate
					if (pp->IsExternal()) {
						tag2 id = pp->ID();
						const char *path = pp->External();
						Reference *xref = refs.Find(path);
						if (!xref) {
							// xref
							ISO_ptr<void> p = _ReadExternal(filename(path));
							++xrefs;
							if (!p)
								throw_accum("Can't open " << path);
							// id
							if (id)
								p.SetID(id);
							// log
							Indent indent(_GetPlatformFilename(path), 1);
							// reference
							if (user && user.GetType() == ISO::REFERENCE)
								user = *user;
							Reference *ref = new Reference(p, user.GetTypeDef(), path);
							refs.insert(refs.At(p), ref);
							// recurse
							size_t _xrefs = xrefs;
							Enum(ISO::Browser(*pp = p));
							ref->recurse = xrefs != _xrefs;

						} else {
							// duplicate, retain id
							if (id && xref->p.ID() && id != xref->p.ID()) {
								*pp = Duplicate(id, xref->p);
								refs.insert(refs.At(*pp), new Reference(*pp, xref->type, xref->path));
							} else
								*pp = xref->p;
						}

					} else {
						// recurse
						refs.insert(at, new Reference(*pp));
						Enum(ISO::Browser(*pp));
						pp->ClearFlags(ISO::Value::HASEXTERNAL);
					}
					return;
				}
				case ISO::USER: {
					b = (user = b).SkipUser();
					break;
				}
				default:
					return;
			}
		}
	}
	_IndexReferences(References &refs) : refs(refs), xrefs(0) {}
};
ISO_ptr<void> IndexReferences(const char *filename, References &refs) {
	// root
	ISO_ptr<void> p = FileHandler::CachedRead(filename);
	refs.push_back(new Reference(p, NULL, filename));
	_IndexReferences(refs).Enum(ISO::Browser(p));
	return p;
}

// External, Externals
struct Externals : dynamic_array<struct External*> {
	~Externals();
	struct External* Find(const char *path);
	bool Append(struct External *xref);
};

struct External {
	string			path;
	const ISO::Type *type;
	Externals		dependants;

	const External* Root() const {
		if (dependants.empty())
			return this;
		// common root
		auto iter = dependants.begin();
		const External *root = (*iter)->Root();
		while (++iter != dependants.end()) {
			if ((*iter)->Root() != root)
				return NULL;
		}
		return root;
	}
	bool IsShared() const {
		// root reference
		if (!(dependants.size() > 1))
			return false;
		// all references shared
		auto iter = dependants.begin();
		while (iter != dependants.end() && (*iter)->IsShared())
			++iter;
		// has no common root
		return iter != dependants.end() && !Root();
	}
	External(const char *path, const ISO::Type *type) : path(path), type(type) {}
};

Externals::~Externals() {
#if 0
	for (iterator _iter = begin(), _end = end(); _iter != _end; ++_iter) {
		(*_iter)->dependants.clear();
		delete *_iter;
	}
#endif
}

External* Externals::Find(const char *path) {
	auto at = find_if(*this, [path](External *p) { return istr(p->path.begin()) == path; });
	return at != end() ? *at : NULL;
}

bool Externals::Append(External *xref) {
	if (find(*this, xref) != end())
		return false;
	push_back(xref);
	return true;
}

struct _GetExternals {
	References	&refs;
	Externals	&xrefs;
	External	*top;
	ReferenceHistory ptrs;

	void Enum(ISO::Browser b) {
		for (;;) {
			switch (b.GetType()) {
				case ISO::COMPOSITE:
				case ISO::ARRAY:
				case ISO::OPENARRAY: {
					for (uint32 i = 0, count = b.Count(); i < count; ++i)
						Enum(b[i].SkipUser());
					return;
				}
				case ISO::REFERENCE: {
					// skip
					ISO_ptr<void> p = *b;
					if (!p)
						return;
					// enumerate
					Reference *ref = refs.Find(p);
					if (!(ref && ref->path)) {
						if (!ptrs.Append(p))
							return;
						Enum(ISO::Browser(p));
					} else
						Enum(ISO::Browser(p), ref);
					return;
				}
				case ISO::USER: {
					b = b.SkipUser();
					break;
				}
				default:
					return;
			}
		}
	}
	void Enum(ISO::Browser b, const Reference *ref) {
		// log
		Indent indent(_GetPlatformFilename((const char*)ref->path), 1);
		// lookup
		if (External *xref = xrefs.Find(ref->path)) {
			// append
			xref->dependants.Append(top);
		} else {
			// stack
			External *parent = top;
			top = new External(ref->path, ref->type);
			if (parent)
				top->dependants.push_back(parent);
			xrefs.push_back(top);
			if (ref->recurse)
				Enum(b);
			top = parent;
		}
	}
	_GetExternals(References &refs, Externals &xrefs) : refs(refs), xrefs(xrefs), top(NULL) {}
};
void GetExternals(ISO_ptr<void> p, References &refs, Externals &xrefs) {
	_GetExternals(refs, xrefs).Enum(ISO::Browser(p), refs.Find(p));
}

// Convert
void _Convert(ISO::Browser b, ReferenceHistory &ptrs) {
	for (;;) {
		switch (b.GetType()) {
			case ISO::COMPOSITE:
			case ISO::ARRAY:
			case ISO::OPENARRAY: {
				for (uint32 i = 0, count = b.Count(); i < count; ++i)
					_Convert(b[i].SkipUser(), ptrs);
				return;
			}
			case ISO::REFERENCE: {
				// skip
				ISO_ptr<void> *pp = b;
				if (!(*pp && ptrs.Append(*pp)))
					return;
				// convert
				if (!pp->IsExternal()) {
					_Convert(ISO::Browser(*pp), ptrs);
					ISO_ptr<void> p = ISO_conversion::convert(*pp, ((ISO::TypeReference*)b.GetTypeDef())->subtype);
					if (p.Flags() & ISO::Value::REDIRECT)
						p = *ISO_ptr<ISO_ptr<void> >(p);
					if (p && !(p == *pp)) {
						if (pp->Flags() & ISO::Value::FROMBIN)
							pp->Header()->addref();
						ptrs.Append(*pp = p);
					}
				}
				return;
			}
			case ISO::USER: {
				b = b.SkipUser();
				break;
			}
			default:
				return;
		}
	}
};
void Convert(const ISO_ptr<void> p) {
	ReferenceHistory ptrs;
	_Convert(ISO::Browser(p), ptrs);
}

ISO_ptr<void> SharedExternals(const ISO_openarray<string> &filenames) {
	// setup
	ISO::Browser variables = ISO::root("variables");
	variables.SetMember("exportfor", "none");
	variables.SetMember("keepexternals", 1);
	variables.SetMember("dontconvert", 1);
	const char *dependants = variables["listdependants"].GetString();

	Externals xrefs[2];
	for (uint32 i = 0, count = filenames.Count(); i < count; i++) {
		Indent indent(filenames[i]);
		// index
		References refs;
		ISO_ptr<void> p;
		{
			Indent indent("Indexing...");
			p = IndexReferences(filenames[i], refs);
		}
		// pre-convert dependants
		if (dependants) {
			Indent indent("Dependencies...");
			GetExternals(p, refs, xrefs[0]);
		}
		// convert
		{
			Indent indent("Converting...");
			Convert(p);
		}
		// post-convert dependants
		{
			Indent indent("Shared dependencies...");
			GetExternals(p, refs, xrefs[1]);
		}
	}

	// dependants
	if (dependants) {
		bool converted = *dependants == '-';
		FileOutput out(dependants + converted);
		if (out.exists()) {
			for (auto iter = xrefs[converted].begin(), end = xrefs[converted].end(); iter != end; ++iter) {
				External *xref = *iter;
				if (xref->dependants.size()) {
					out.write(static_cast<const char*>(_GetPlatformFilename(filename(xref->path))));
					out.write("\r\n");
					for (auto &i : xref->dependants) {
						out.write("  ");
						out.write(static_cast<const char*>(_GetPlatformFilename(filename((i)->path))));
						out.write("\r\n");
					}
					out.write("\r\n");
				}
			}
		} else {
			throw_accum("Can't create " << dependants);
		}
	}

	// shared
	ISO_ptr<anything> shared(NULL);
	for (auto iter = xrefs[1].begin(), end = xrefs[1].end(); iter != end; ++iter) {
		if ((*iter)->IsShared())
			shared->Append(MakePtrExternal((*iter)->type, (*iter)->path, (*iter)->path));
	}
	return shared;
}

static initialise init(
	ISO_get_operation(SharedExternals)
);

// ShareExternals
struct _ShareExternals {
	const filename		&dir;
	ISO_ptr<anything>	externals;
	ReferenceHistory	ptrs;

	void Enum(ISO::Browser b) {
		for (;;) {
			switch (b.GetType()) {
				case ISO::COMPOSITE:
				case ISO::ARRAY:
				case ISO::OPENARRAY: {
					for (uint32 i = 0, count = b.Count(); i < count; ++i)
						Enum(b[i].SkipUser());
					return;
				}
				case ISO::REFERENCE: {
					// skip
					ISO_ptr<void> *pp = b;
					if (!*pp)
						return;
					// enumerate
					if (pp->IsExternal()) {
						const char *id		= pp->ID().get_tag();
						filename	path	= filename(dir).relative(pp->External());
						for (uint32 i = 0, count = externals->Count(); i < count; ++i) {
							// dereference
							ISO::Browser b((*externals)[i]);
							if (b.SkipUser().GetType() == ISO::REFERENCE)
								b = *b;
							if (istr((const char*)b) == path) {
								// patch
								Indent indent(_GetPlatformFilename(path));
								pp->CreateExternal(format_string("%s;'%s'", static_cast<const char*>(externals.ID().get_tag()), static_cast<const char*>(b)), id);
								ptrs.Append(*pp);
								return;
							}
						}
						// recurse
						ISO_ptr<void> p = _ReadExternal(filename(path));
						if (*pp = p) {
							if (id)
								pp->SetID(id);
							ptrs.Append(*pp);
							Enum(ISO::Browser(*pp));
						} else
							throw_accum("Can't open " << path);

					} else {
						// recurse
						if (!ptrs.Append(*pp))
							return;
						Enum(ISO::Browser(*pp));
						pp->ClearFlags(ISO::Value::HASEXTERNAL);
					}
					return;
				}
				case ISO::USER: {
					b = b.SkipUser();
					break;
				}
				default:
					return;
			}
		}
	}
	_ShareExternals(const filename &_externals, const filename &_dir) : dir(_dir) {
		if (!(externals = FileHandler::Read(_externals.name(), _externals)))
			throw_accum("Can't open " << _externals);
	}
};

void ShareExternals(ISO_ptr<void> p, const filename &dir, const char *externals, bool dontconvert) {
	// alt-todo: index, convert, index driven patching
	// patch
	{
		Indent indent("Sharing...");
		_ShareExternals(externals, dir).Enum(ISO::Browser(p));
	}
	// convert
	if (!dontconvert) {
		Indent indent("Converting...");
		Convert(p);
	}
}
