#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "extra/xml.h"
#include "hashes/md5.h"
#include "base/algorithm.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	xcode project
//-----------------------------------------------------------------------------

extern class PLISTFileHandler plist;

class XcodeFileHandler : public FileHandler {
protected:
	bool					Write(ISO::Browser b, ostream_ref file, const filename &fn);

	const char*		GetExt() override { return "pbxproj"; }
	const char*		GetDescription() override { return "Xcode 4 project";	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return ((FileHandler*)&plist)->Read(id, file);
	}

	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		return Write(ISO::Browser(p), FileOutput(fn).me(), filename(fn).rem_dir());
	}
} xcode;

class XcodeDirHandler : public XcodeFileHandler {
	const char*		GetExt() override { return "xcodeproj"; }
	//ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override {
		return (is_dir(fn) || create_dir(fn))
			&& Write(ISO::Browser(p), FileOutput(filename(fn).add_dir("project.pbxproj")).me(), fn);
	}
} xcode_dir;

struct entry {
	struct ref {
		entry	*p;
		ref()					: p(0) {}
		ref(entry *_p)			: p(_p) {}
		ref(const char *_name)	: p(new entry(_name)) {}
		bool	operator==(const char *n)	const	{ return p->name == n; }
		bool	operator==(entry *r)		const	{ return p == r; }
		operator entry*()					const	{ return p; }
		entry*	operator->()				const	{ return p; }
		ref		operator[](const char *n)	const	{ return (*p)[n]; }
	};
	string					name;
	ISO_ptr<void>			p;
	dynamic_array<ref>		children;
	entry(const char *_name) : name(_name) {}
	ref		operator[](const char *n) const { const ref *r = find(children, n); return r == children.end() ? ref() : *r; }
	bool	operator==(const char *n) const { return name == n; }
};

struct xlist : anything {};
ISO_DEFUSERX(xlist, anything, "array");

typedef ISO_ptr<ISO_ptr<void> >	identifier;

identifier make_identifier(const char *s) {
	return identifier(s);
}

tag make_tag(const char *s) {
	MD5::CODE	md5 = MD5(s);
	return to_string(md5).slice(0, 24);
}

ISO_ptr<anything> make_item(const identifier &isa, const char *hashname, const char *name = 0) {
	ISO_ptr<anything>	p(make_tag(hashname));
	p->Append(isa);
	if (name)
		p->Append(ISO_ptr<string>("name", name));
	return p;
}
ISO_ptr<anything> make_item(const char *isa, const char *hashname, const char *name = 0) {
	return make_item(identifier("isa", ISO_ptr<void>(isa)), hashname, name);
}

template<typename T, typename V> T &add_entry(dynamic_array<T> &a, const V &v) {
	T *r = find(a, v);
	return r == a.end() ? a.push_back(v) : *r;
}

class tracewriter : public stream_defaults<tracewriter> {
public:
	size_t		writebuff(const void *buffer, size_t size)		{ trace_accum() << str((char*)buffer, size); return size;	}
	int			putc(int c)										{ char s[2] = {(char)c, 0}; _iso_debug_print(s); return 1;	}
};
typedef writer_mixout<tracewriter> TraceOutput;

bool XcodeFileHandler::Write(ISO::Browser b, ostream_ref file, const filename &fn) {
	static const char *settings[][2] = {
		{"ALWAYS_SEARCH_USER_PATHS",			"NO"			 },
		{"CLANG_CXX_LANGUAGE_STANDARD",			"gnu++0x"		 },
		{"CLANG_CXX_LIBRARY",					"libc++"		 },
		{"CLANG_ENABLE_OBJC_ARC",				"YES"			 },
		{"CLANG_WARN_BOOL_CONVERSION",			"YES"			 },
		{"CLANG_WARN_CONSTANT_CONVERSION",		"YES"			 },
		{"CLANG_WARN_DIRECT_OBJC_ISA_USAGE",	"YES_ERROR"		 },
		{"CLANG_WARN_EMPTY_BODY",				"YES"			 },
		{"CLANG_WARN_ENUM_CONVERSION",			"YES"			 },
		{"CLANG_WARN_INT_CONVERSION",			"YES"			 },
		{"CLANG_WARN_OBJC_ROOT_CLASS",			"YES_ERROR"		 },
		{"CLANG_WARN__DUPLICATE_METHOD_MATCH",	"YES"			 },
		{"COPY_PHASE_STRIP",					"YES"			 },
		{"DEBUG_INFORMATION_FORMAT",			"dwarf-with-dsym"},
		{"ENABLE_NS_ASSERTIONS",				"NO"			 },
		{"GCC_C_LANGUAGE_STANDARD",				"gnu99"			 },
		{"GCC_ENABLE_OBJC_EXCEPTIONS",			"YES"			 },
		{"GCC_WARN_64_TO_32_BIT_CONVERSION",	"YES"			 },
		{"GCC_WARN_ABOUT_RETURN_TYPE",			"YES_ERROR"		 },
		{"GCC_WARN_UNDECLARED_SELECTOR",		"YES"			 },
		{"GCC_WARN_UNINITIALIZED_AUTOS",		"YES_AGGRESSIVE" },
		{"GCC_WARN_UNUSED_FUNCTION",			"YES"			 },
		{"GCC_WARN_UNUSED_VARIABLE",			"YES"			 },
		{"MACOSX_DEPLOYMENT_TARGET",			"10.9"			 },
		{"SDKROOT",								"macosx"		 },
	};

	identifier	isa_file("isa", "PBXFileReference");
	identifier	isa_build("isa", "PBXBuildFile");
	identifier	isa_group("isa", "PBXGroup");

	entry		globals("globals");
	entry		groups("groups");
	entry		filters("filters");
	entry		filter_tree("filters");
	add_entry(filters.children, &filter_tree);

	for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
		if (i.GetName() == "ItemGroup") {
			for (ISO::Browser::iterator i2 = i->begin(), e2 = i->end(); i2 != e2; ++i2) {
				tag2		id	= i2.GetName();
				ISO::Browser	b	= **i2;

				if (b.GetType() == ISO::OPENARRAY) {
					entry::ref	&g = add_entry(groups.children, id.get_tag());
					if (const char *include = b["Include"].GetString()) {
						entry::ref	&e = add_entry(g->children, include);

						if (ISO::Browser	b2 = b["Filter"]) {
							const char *filter = b2[0].GetString();
							entry	*parent = &filter_tree;
							while (const char *sep = string_find(filter, '\\')) {
								parent = add_entry(parent->children, string(filter, sep));
								filter = sep + 1;
							}
							entry::ref	&f = add_entry(parent->children, filter);
							add_entry(filters.children, f);
							add_entry(f->children, e);
						}
					}
				}
			}
		} else if (i.GetName() == "PropertyGroup") {
			ISO::Browser	b	= **i;
			entry::ref	g	= groups["ProjectConfiguration"];
			entry		*f	= &globals;
			if (const char *cond = b["Condition"].GetString()) {
				if (const char *eq = string_find(cond, "=='"))
					f	= g[string(str(eq + 3, string_find(eq + 3, '\'')))];
			}
			for (ISO::Browser::iterator i2 = i->begin(), e2 = i->end(); i2 != e2; ++i2) {
				entry::ref	e	= f->children.push_back(i2.GetName().get_tag());
				e->p			= **i2;
			}
		}
	}

	ISO_ptr<const char*> sourceTree("sourceTree", "<group>");

	ISO_ptr<anything>	objects("objects");
	ISO_ptr<anything>	project = make_item(identifier("isa", "PBXProject"), "project");
	ISO_ptr<xlist>		targets("targets");
	project->Append(targets);
	project->Append(ISO_ptr<const char*>("compatibilityVersion", "Xcode 3.2"));

	ISO_ptr<xlist>		buildfiles("files");

	for (entry::ref *i = groups.children.begin(), *e = groups.children.end(); i != e; ++i) {
		const char *type	= 0;
		bool		build	= false;
		if ((*i)->name == "ClCompile") {
			type	= "sourcecode.cpp.cpp";
			build	= true;
		} else if ((*i)->name == "ClInclude") {
			type	= "sourcecode.c.h";
		}

		if (type) {
			identifier	ft("lastKnownFileType", type);
			for (entry::ref *i2 = (*i)->children.begin(), *e2 = (*i)->children.end(); i2 != e2; ++i2) {
				filename		name((*i2)->name);
				ISO_ptr<anything> p = make_item(isa_file, name, name.name_ext());
				p->Append(ft);
				p->Append(ISO_ptr<string>("path", name.relative_to(fn).convert_to_fwdslash()));
				p->Append(sourceTree);
				(*i2)->p = p;
				objects->Append(p);

				if (build) {
					ISO_ptr<anything> p = make_item(isa_build, (*i2)->name + ".ref");
					p->Append(ISO_ptr<ISO_ptr<void> >("fileRef", (*i2)->p));
					buildfiles->Append(p);
					objects->Append(p);
				}
			}
		}
	}

	// PBXGroup:

	for (entry::ref *i = filters.children.begin(), *e = filters.children.end(); i != e; ++i) {
		ISO_ptr<anything>	p = make_item(isa_group, (*i)->name, (*i)->name);
		(*i)->p = p;
		p->Append(sourceTree);
		objects->Append(p);
	}

	ISO_ptr<xlist>		root("children");
	{
		ISO_ptr<anything> p = (*filters.children.begin())->p;
		p->Append(root);
		project->Append(ISO_ptr<ISO_ptr<void> >("mainGroup", p));
	}

	for (entry::ref *i = filters.children.begin(), *e = filters.children.end(); i != e; ++i) {
		ISO_ptr<anything>	p = (*i)->p;
		ISO_ptr<xlist>		c("children");
		p->Append(c);
		p->Append(sourceTree);
		objects->Append(p);
		root->Append(p);

		for (entry::ref *i2 = (*i)->children.begin(), *e2 = (*i)->children.end(); i2 != e2; ++i2) {
			if ((*i2)->p)
				c->Append((*i2)->p);
		}
	}

	for (entry::ref *i = filters.children.begin(), *e = filters.children.end(); i != e; ++i) {
		for (entry::ref *i2 = (*i)->children.begin(), *e2 = (*i)->children.end(); i2 != e2; ++i2)
			(*i2)->p.Clear();
	}

	for (entry::ref *i = groups.children.begin(), *e = groups.children.end(); i != e; ++i) {
		for (entry::ref *i2 = (*i)->children.begin(), *e2 = (*i)->children.end(); i2 != e2; ++i2) {
			if ((*i2)->p)
				root->Append((*i2)->p);
		}
	}


	// PBXFileReference:

	const char *rootnamespace	= ISO::Browser(globals["RootNamespace"]->p)[0].GetString();
	const char *projectname = rootnamespace;
	if (auto name = globals["ProjectName"])
		projectname = ISO::Browser(name->p)[0].GetString();

	entry		*ct	= globals["ConfigurationType"];
	if (!ct) {
		entry	*configs	= groups["ProjectConfiguration"];
		for (entry::ref *i = configs->children.begin(), *e = configs->children.end(); !ct && i != e; ++i)
			ct	= (*i)["ConfigurationType"];
	}

	const char *configtype	= ISO::Browser(ct->p)[0].GetString();

	ISO_ptr<anything> target	= make_item("PBXNativeTarget", "nativetarget");
	ISO_ptr<anything> product	= make_item(isa_file, projectname);

	if (str(configtype) == "StaticLibrary") {
		product->Append(ISO_ptr<const char*>("path", filename(projectname).set_ext("a")));
		product->Append(ISO_ptr<const char*>("explicitFileType", "archive.ar"));
		target->Append(ISO_ptr<const char*>("productType", "com.apple.product-type.library.static"));
	} else {
		product->Append(ISO_ptr<const char*>("path", projectname));
		product->Append(ISO_ptr<const char*>("explicitFileType", "compiled.mach-o.executable"));
		target->Append(ISO_ptr<const char*>("productType", "com.apple.product-type.tool"));
	}

	product->Append(ISO_ptr<int>("includeInIndex", 0));
	product->Append(ISO_ptr<ISO_ptr<void> >("sourceTree", make_identifier("BUILT_PRODUCTS_DIR")));
	objects->Append(product);

	target->Append(ISO_ptr<const char*>("name", projectname));
	target->Append(ISO_ptr<const char*>("productName", projectname));
	target->Append(ISO_ptr<ISO_ptr<void> >("productReference", product));

	// PBXGroup:
	{
		ISO_ptr<anything> p = make_item(isa_group, "Products", "Products");
		objects->Append(p);
		ISO_ptr<xlist> c("children");
		c->Append(product);
		p->Append(c);
		p->Append(sourceTree);

		root->Append(p);
		project->Append(ISO_ptr<ISO_ptr<void> >("productRefGroup", p));
	}

	// PBXNativeTarget:
	identifier	isa_configlist("isa", "XCConfigurationList");
	identifier	isa_buildconfig("isa", "XCBuildConfiguration");

	// XCConfigurationList:
	{	// for PBXNativeTarget
		ISO_ptr<anything> p = make_item(isa_configlist, "NativeTarget.configs");
		objects->Append(p);
		ISO_ptr<xlist> c("buildConfigurations");
		p->Append(c);
		p->Append(ISO_ptr<int>("defaultConfigurationIsVisible", 0));
		{
			ISO_ptr<anything> p = make_item(isa_buildconfig, "NativeTarget.Debug", "Debug");
			objects->Append(p);
			ISO_ptr<anything>	s("buildSettings");
			s->Append(ISO_ptr<const char*>("PRODUCT_NAME", "$(TARGET_NAME)"));
			p->Append(s);
			c->Append(p);
		}
		{
			ISO_ptr<anything> p = make_item(isa_buildconfig, "NativeTarget.Release", "Release");
			objects->Append(p);
			ISO_ptr<anything>	s("buildSettings");
			s->Append(ISO_ptr<const char*>("PRODUCT_NAME", "$(TARGET_NAME)"));
			p->Append(s);
			c->Append(p);
		}
		target->Append(identifier("buildConfigurationList", p));
	}

	{	// for PBXProject
		ISO_ptr<anything> p = make_item(isa_configlist, "Project.configs");
		objects->Append(p);
		ISO_ptr<xlist> c("buildConfigurations");
		p->Append(c);
		p->Append(ISO_ptr<int>("defaultConfigurationIsVisible", 0));
		{
			ISO_ptr<anything> p = make_item(isa_buildconfig, "Project.Debug", "Debug");
			objects->Append(p);
			ISO_ptr<anything>	s("buildSettings");
			s->Append(ISO_ptr<int>("GCC_OPTIMIZATION_LEVEL", 0));
			for (int i = 0; i < num_elements(settings); ++i)
				s->Append(ISO_ptr<const char*>(settings[i][0], settings[i][1]));
			p->Append(s);
			c->Append(p);
		}
		{
			ISO_ptr<anything> p = make_item(isa_buildconfig, "Project.Release", "Release");
			objects->Append(p);
			ISO_ptr<anything>	s("buildSettings");
			for (int i = 0; i < num_elements(settings); ++i)
				s->Append(ISO_ptr<const char*>(settings[i][0], settings[i][1]));
			p->Append(s);
			c->Append(p);
		}
		project->Append(identifier("buildConfigurationList", p));
	}

	// PBXSourcesBuildPhase:
	ISO_ptr<xlist> phases("buildPhases");
	target->Append(phases);
	targets->Append(target);
	objects->Append(target);

	{
		ISO_ptr<anything> p = make_item("PBXSourcesBuildPhase", "phase1");
		objects->Append(p);
		p->Append(ISO_ptr<int>("buildActionMask", 0x7fffffff));
		p->Append(buildfiles);
		p->Append(ISO_ptr<int>("runOnlyForDeploymentPostprocessing", 0));
		phases->Append(p);
	}
	objects->Append(project);

	ISO_ptr<anything>	t(0);
	t->Append(ISO_ptr<int>("archiveVersion", 1));
	t->Append(ISO_ptr<anything>("classes"));
	t->Append(ISO_ptr<int>("objectVersion", 46));
	t->Append(objects);
	t->Append(ISO_ptr<ISO_ptr<void> >("rootObject", project));

	file.write("// !$*UTF8*$!\n");

	return ((FileHandler*)&plist)->Write(t, file);


//	xcode_writer(TraceOutput()).put_item(ISO::Browser(t));
//	xcode_writer(file).put_item(ISO::Browser(t));
//	return false;
}

//-----------------------------------------------------------------------------
//	xcode workspace
//-----------------------------------------------------------------------------

class XcodeWSFileHandler : public FileHandler {
	struct Writer : XMLwriter {
		filename	fn;
		void		Write(const ISO::Browser &b);
		Writer(ostream_ref _file, const char *_fn = 0) : XMLwriter(_file, true), fn(_fn) {}
	};

	const char*		GetExt() override { return "xcworkspacedata"; }
	const char*		GetDescription() override { return "Xcode 4 workspace";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		XMLreader	xml(file);
		XMLreader::Data	data;
		return xml.CheckVersion()	>= 0
			&& xml.ReadNext(data)	== XMLreader::TAG_BEGIN
			&& data.Is("Workspace")
			? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
	bool			WriteWithFilename(ISO_ptr<void> p, const filename &fn) override;
} xcode_ws;

ISO_ptr<void> XcodeWSFileHandler::Read(tag id, istream_ref file) {
	XMLreader	xml(file);
	XMLreader::Data	data;

	try {
		if (xml.ReadNext(data, XMLreader::TAG_BEGIN) != XMLreader::TAG_BEGIN || !data.Is("Workspace"))
			return ISO_NULL;

		ISO_ptr<ISO_openarray<string> > p(id);
		while (xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN) {
			if (data.Is("FileRef")) {
				const char *loc = data.Find("location");
				p->Append(loc);
			}
			xml.ReadNext(data, XMLreader::TAG_END);
		}
		return p;

	} catch (const char *error) {
		throw_accum(error << " at line " << xml.GetLineNumber());
		return ISO_NULL;
	}
}

void XcodeWSFileHandler::Writer::Write(const ISO::Browser &b) {
	for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
		if (i->GetType() != ISO::STRING) {
			if (ISO::Browser2 proj = (*i)["project"]) {
				filename	projfn = proj.External();
				if (!projfn)
					projfn = proj["ProjectPath"].GetString();
				XMLelement(*this, "FileRef").Attribute("location", buffer_accum<256>("group:") << projfn.relative_to(fn).set_ext("xcodeproj"));

			} else {
				XMLelement(*this, "Group").Attribute("location", "container:").Attribute("name", to_string(i.GetName())),
					Write(*i);
			}
		}
	}
}

bool XcodeWSFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO::Browser	b(p);
	if (b = b["projects"]) {
		Writer	w(file);
		XMLelement(w, "Workspace").Attribute("version", "1.0"), w.Write(b);
		return true;
	}

	return false;
}

bool XcodeWSFileHandler::WriteWithFilename(ISO_ptr<void> p, const filename &fn) {
	ISO::Browser	b	= ISO::Browser(p)["projects"];
	if (!b)
		return false;

	if (!exists(fn)) {
		if (!create_dir(fn))
			return false;

	} else if (!is_dir(fn)) {
		FileOutput	file(fn);
		Writer		w(file, fn);
		XMLelement(w, "Workspace").Attribute("version", "1.0"), w.Write(b);
		return true;
	}

	{
		FileOutput	file(filename(fn).add_dir("contents.xcworkspacedata"));
		Writer		w(file, fn);
		XMLelement(w, "Workspace").Attribute("version", "1.0"), w.Write(b);
	}

	return true;
}
