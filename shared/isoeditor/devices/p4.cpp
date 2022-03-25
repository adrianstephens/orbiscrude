#include "device.h"
#include "clientapi.h"
#include "p4libs.h"
#include "extra/regex.h"
#include "hashes/fnv.h"
#include "filename.h"
#include "base/algorithm.h"
#include "directory.h"

using namespace iso;

class IsoClientUser : public ClientUser {
public:
	virtual void	InputData(StrBuf *strbuf, Error *e)	{}

	virtual void 	HandleError(Error *err)	{
		StrBuf buf;
		err->Fmt(buf, EF_NEWLINE);
		OutputError(buf.Text());
	}
	virtual void 	Message(Error *err)		{
		if (err->IsInfo()) {
			StrBuf buf;
			err->Fmt(buf, EF_PLAIN);
			OutputInfo( (char)err->GetGeneric() + '0', buf.Text() );
		} else {
			HandleError(err);
		}
	}
	virtual void 	OutputError(const char *errBuf)				{ ISO_TRACE(errBuf); }
	virtual void	OutputInfo(char level, const char *data)	{ ISO_TRACE(data); }
	virtual void 	OutputBinary(const char *data, int length)	{}
	virtual void 	OutputText(const char *data, int length)	{}

	virtual void	OutputStat(StrDict *varList)	{
		StrRef var, val;
		for (int i = 0; varList->GetVar(i, var, val); i++) {
			if (var != "func" && var != P4Tag::v_specFormatted) {
				StrBuf msg;
				msg << var << " " << val;
				char level = strncmp( var.Text(), "other", 5 ) ? '1' : '2';
				OutputInfo(level, msg.Text());
			}
		}
		OutputInfo('0', "");
	}

	virtual void	Prompt(const StrPtr &msg, StrBuf &rsp, int noEcho, Error *e)				{}
	virtual void	Prompt(const StrPtr &msg, StrBuf &rsp, int noEcho, int noOutput, Error *e)	{}
	virtual void	ErrorPause(char *errBuf, Error *e)					{ OutputError(errBuf); }

	virtual void	Edit(FileSys *f1, Error *e)	{}

	virtual void	Diff(FileSys *f1, FileSys *f2, int doPage, char *diffFlags, Error *e)					{}
	virtual void	Diff(FileSys *f1, FileSys *f2, FileSys *fout, int doPage, char *diffFlags, Error *e)	{}

	virtual void	Merge(FileSys *base, FileSys *leg1, FileSys *leg2, FileSys *result, Error *e)	{}

	virtual int		Resolve(ClientMerge *m, Error *e)					{ return 0; }
	virtual int		Resolve(ClientResolveA *r, int preview, Error *e)	{ return 0; }

	virtual void	Help(const char *const *help)	{}

	virtual FileSys	*File(FileSysType type)			{ return 0; }
	virtual ClientProgress *CreateProgress(int)		{ return 0; }
	virtual int		ProgressIndicator()				{ return 0; }

	virtual void	Finished()						{}
	virtual void	SetOutputCharset(int)			{}
	virtual void	DisableTmpCleanup()				{}
	virtual void	SetQuiet()						{}
};

void ReadSettings(map<string, string>& settings, const char* data) {
	string_scan ss(data);
	do {
		auto tok = ss.get_token(char_set::alphanum);
		if (ss.check(':')) {
			/*switch (hash(tok)) {
			case "Client"_fnv:
			case "Update"_fnv:
			case "Access"_fnv:
			case "Owner"_fnv:
			case "Host"_fnv:
			case "Description"_fnv:
			case "Root"_fnv:
			case "Options"_fnv:
			case "SubmitOptions"_fnv:
			case "LineEnd"_fnv:
			case "View"_fnv: {*/
			auto begin = ss.skip_whitespace().getp();
			auto end   = ss.scan("\n\n");
			if (!end)
				end = ss.end();
			settings.put(tok, str(begin, end));
			/*break;
			}
			}*/
		}
	} while (ss.scan_skip('\n'));
}

//-----------------------------------------------------------------------------
// P4Spec
//-----------------------------------------------------------------------------
struct P4Spec {
	dynamic_array<P4Spec>	children;
	string				depot, client;
	uint32				flags = 0;
	ISO_ptr<anything>	files;

	void	init(const char* view, const char *ws_name);
	auto	dir()	{
		if (!files && client) {
			files.Create(0);
			filename	fn = filename::cleaned(client);
			if (fn.name_ext() == "...")
				fn.rem_dir();
			for (directory_iterator name(fn.add_dir(DIRECTORY_ALL)); name; ++name) {
				if (name[0] != '.') {
					files->push_back(ISO_ptr<void>((const char*)name));
					if (name.is_dir()) {
						if (!find_if_check(children, [&name](const P4Spec &i) { return i.depot == name; })) {
							auto	&child	= children.push_back();
							child.depot		= name;
							child.client	= client + "/" + (const char*)name;
						}
					}
				}
			}
		}
		return files;
	}
};

void P4Spec::init(const char* view, const char *ws_name) {
	for (;;) {
		view		= skip_whitespace(view);
		auto	end = string_find(view, " //");
		if (!end)
			break;

		string_scan	ss(trim(view, end));

		bool	neg	= ss.check('-');
		if (!ss.check("//"))
			break;

		P4Spec	*spec = this;
		do {
			auto	tok = ss.get_token(~char_set('/'));
			if (tok != "...") {
				if (auto i = find_if_check(spec->children, [&tok](const P4Spec &i) { return i.depot == tok; })) {
					spec = i;
				} else {
					spec = &spec->children.push_back();
					spec->depot = tok;
				}
			}
		} while (ss.check('/'));

		spec->flags		= neg;

		view = string_find(end, '\n');
		if (!view)
			view = string_end(end);

		ss = string_scan(end + 3, view);
		ISO_ASSERT(ss.check(ws_name));
		spec->client	= client + ss.remainder();
	}
}
ISO_DEFUSERCOMPV(P4Spec, depot, client, flags, children, dir);

//-----------------------------------------------------------------------------
// P4Client
//-----------------------------------------------------------------------------

struct P4Client {
	struct P4Workspace : ISO::VirtualDefaults {
		P4Client	*client;
		P4Spec		spec;

		P4Workspace(P4Client *client) : client(client) {}

		ISO::Browser2 Deref() {
			if (!spec.depot) {
				spec.depot = client->name;
				spec.client = client->settings["Root"];
				spec.init(client->settings["View"], client->name);
			}
			return ISO::MakePtr(0, &spec);
		}

	};
	string				name;
	map<string, string>	settings;
	P4Workspace			workspace;

	P4Client(string&& name) : name(move(name)), workspace(this) {}
	friend tag2 _GetName(const P4Client &c)	{ return c.name; }
};

template<> struct ISO::def<P4Client::P4Workspace> : VirtualT<P4Client::P4Workspace> {};
ISO_DEFUSERCOMPV(P4Client, settings, workspace);

//-----------------------------------------------------------------------------
// P4Depot
//-----------------------------------------------------------------------------

struct P4Depot {
	struct Dir {
		string					name;
		dynamic_array<string>	children;
		Dir(const char *name) : name(name) {}
		auto	children2(P4Depot* depot) {
			return children;
		}
		friend tag2 _GetName(const Dir &c)	{ return c.name; }
	};
	string				name;
	map<string, string>	settings;
	dynamic_array<Dir>	dirs;

	P4Depot(string&& name) : name(move(name)) {}
	auto	dirs2()		{ return with_param(dirs, this); }
	friend tag2 _GetName(const P4Depot &c)	{ return c.name; }
};

ISO_DEFUSERCOMPV(P4Depot::Dir, name, children);
ISO_DEFUSERCOMPV(P4Depot, settings, dirs2);

//-----------------------------------------------------------------------------
// P4
//-----------------------------------------------------------------------------

template<typename F> struct P4Getter : IsoClientUser {
	F		f;
	P4Getter(F &&f) : f(forward<F>(f)) {}
	virtual void	OutputInfo(char level, const char *data) {
		f(level, data);
	}
	operator ClientUser*() { return this; }
};

template<typename F> P4Getter<F> make_P4Getter(F &&f) { return forward<F>(f); }

template<typename T> constexpr T* addr2(T&&t) { return &t; }
/*
template<typename...A> void Run(ClientApi &api, ClientUser *user, const char *command, A&&... args) {
	string	sargs[] = {
		to_string(args)...
	};

	api.SetArgv(num_elements(sargs), (char *const *)sargs);
	api.Run(command, user);
}
*/
template<typename F, typename...A> void Run(ClientApi &api, F&& f, const char *command, A&&... args) {
	string	sargs[] = {
		to_string(args)...
	};
	api.SetArgv(num_elements(sargs), (char *const *)sargs);
	api.Run(command, make_P4Getter(forward<F>(f)));
}

template<typename F> void Run(ClientApi &api, F&& f, const char *command) {
	api.SetArgv(0, 0);
	api.Run(command, make_P4Getter(forward<F>(f)));
}

struct P4 {
	ClientApi	api;
	dynamic_array<P4Depot>	depots;
	dynamic_array<P4Client>	clients;

	P4() {
		Error		e;
		api.Init(&e);
		if (e.Test())
			return;

		Run(api, [this](char level, const char *data) {
			static auto	re	= "Client (.*) (.*) root (.*) '(.*)'"_re;
			dynamic_array<count_string> matches;
			if (re.match(count_string(data), matches)) {
				auto	&client = clients.emplace_back(matches[1]);
				client.settings["Update"]		= matches[2];
				client.settings["Root"]			= matches[3];
				client.settings["Description"]	= matches[4];
			}
		}, "clients");

		Run(api, [this](char level, const char *data) {
			static auto	re	= "Depot (.*) (.*) local (.*) '(.*)'"_re;
			dynamic_array<count_string> matches;
			if (re.match(count_string(data), matches)) {
				auto	&depot = depots.emplace_back(matches[1]);
				depot.settings["Update"]		= matches[2];
				depot.settings["Root"]			= matches[3];
				depot.settings["Description"]	= matches[4];
			}
		}, "depots");

		for (auto &client : clients) {
			Run(api, [&client](char level, const char *data) {
				ReadSettings(client.settings, data);
			}, "client", "-o", client.name);
		}

		for (auto &depot : depots) {
			Run(api, [&depot](char level, const char *data) {
				depot.dirs.emplace_back(data);
			}, "dirs", cstr("//") + depot.name + "/*");
		}

	}

	~P4() {
		Error		e;
		api.Final(&e);
	}
};
ISO_DEFUSERCOMPV(P4, depots, clients);

//-----------------------------------------------------------------------------
// P4Device
//-----------------------------------------------------------------------------

struct P4Device : app::DeviceT<P4Device>, app::DeviceCreateT<P4Device> {
	void			operator()(const app::DeviceAdd &add) {
		add("Perforce...", this, app::LoadPNG("IDB_DEVICE_P4"));
	}
	ISO_ptr<void>	operator()(const win::Control &main) {
		Error		e;
		static bool init = (P4Libraries::Initialize(P4LIBRARIES_INIT_ALL, &e), true);
		return ISO_ptr<P4>("p4");
	}
} p4device;
