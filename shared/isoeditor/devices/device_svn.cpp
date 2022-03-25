#include "iso/iso_files.h"
#include "filetypes/container/archive_help.h"
#include "hashes/md5.h"
#include "comms/http.h"

using namespace iso;

static void writenum(ostream_ref file, size_t i) {
	size_t	p = 1;
	while (i >= p * 10)
		p *= 10;
	while (p) {
		file.putc(char(i / p) + '0');
		i %= p;
		p /= 10;
	}
}

//-----------------------------------------------------------------------------
//	svn_item
//-----------------------------------------------------------------------------

enum svn_type {
	SVN_UNKNOWN,
	SVN_WORD,
	SVN_NUMBER,
	SVN_STRING,
	SVN_LIST,
};

class svn_item {
public:
	virtual ~svn_item()	{}
	virtual	svn_type	type()				{ return SVN_UNKNOWN;	}
	virtual bool		write(iso::ostream_ref file)= 0;
};

class svn_word : public svn_item, public string {
public:
	svn_type	type()						{ return SVN_WORD;	}
	svn_word(const char *_v) : string(_v)	{}
	bool		write(iso::ostream_ref file)	{ return file.write(begin());	}
};

class svn_number : public svn_item {
	uint32		v;
public:
	svn_type	type()						{ return SVN_NUMBER;	}
	svn_number(uint32 _v) : v(_v)			{}
	bool		write(iso::ostream_ref file)	{ writenum(file, v); return true; }

	operator uint32()	const				{ return v; }
};

class svn_string : public svn_item, public memory_block {
public:
	svn_type	type()						{ return SVN_STRING;	}
	svn_string(const char *v)		: memory_block(v ? iso::strdup(v) : 0, v ? strlen(v) : 0)	{}
	svn_string(void *v, uint32 len)	: memory_block(v, len)			{}
	svn_string(const memory_block &v)		: memory_block(v)		{}
	~svn_string()							{ iso::free(p); }
	bool		write(iso::ostream_ref file)	{ writenum(file, length()); file.putc(':'); file.writebuff(*this, length()); return true; }
};

class svn_list : public svn_item, public dynamic_array<svn_item*>	{
public:
	~svn_list()								{ for (size_t i = 0, n = size(); i < n; i++) delete (*this)[i]; }
	svn_type	type()						{ return SVN_LIST;	}
	bool		write(iso::ostream_ref file)	{
		file.putc('(');
		for (size_t i = 0, n = size(); i < n; i++) {
			file.putc(' ');
			(*this)[i]->write(file);
		}
		file.putc(' ');
		file.putc(')');
		return true;
	}

	svn_list*	add(svn_item *i)		{ push_back(i);	return this; }
	svn_list*	add(uint32 v)			{ push_back(new svn_number(v));	return this; }
//	svn_list*	add(const char *&v)		{ push_back(new svn_string(v));	return this; }
	svn_list*	add(const string &v)	{ push_back(new svn_string((const char*)v));	return this; }
	template<int N> svn_list *add(const char (&v)[N])	{ push_back(new svn_word(v));	return this;	}
};

svn_item *make_svn() {
	return new svn_list;
}
template<typename T1> svn_item *make_svn(const T1 &t1) {
	return (new svn_list)->add(t1);
}
template<typename T1, typename T2> svn_item *make_svn(const T1 &t1, const T2 &t2)	{
	return (new svn_list)->add(t1)->add(t2);
}
template<typename T1, typename T2, typename T3> svn_item *make_svn(const T1 &t1, const T2 &t2, const T3 &t3)	{
	return (new svn_list)->add(t1)->add(t2)->add(t3);
}
template<typename T1, typename T2, typename T3, typename T4> svn_item *make_svn(const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)	{
	return (new svn_list)->add(t1)->add(t2)->add(t3)->add(t4);
}

int skip_whitespace(iso::istream_ref file) {
	int	c;
	while ((c = file.getc()) <= ' ');
	return c;
}

svn_item *svn_parse(iso::istream_ref file, int c = 0) {
	if (c == 0)
		c = file.getc();

	while (c <= ' ')
		c = file.getc();

	if (c == '(') {
		svn_list	*v = new svn_list;

		while ((c = skip_whitespace(file)) != ')')
			v->add(svn_parse(file, c));

		c = file.getc();	// get trailing whitespace
		return v;

	} else if (c >= '0' && c <= '9') {

		uint32	i = c - '0';
		while ((c = file.getc()) >= '0' && c <= '9')
			i = i * 10 + c - '0';

		if (c == ' ')
			return new svn_number(i);

		malloc_block	block(i + 1);
		file.readbuff(block, i);
		((char*)block)[i] = 0;
		return new svn_string(block.detach(), i);

	} else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {

		char	temp[1024], *p = temp;
		do {
			*p++ = c;
		} while (((c = file.getc()) >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-');
		*p = 0;
		return new svn_word(temp);

	} else {
		return 0;
	}
}

string	blank_string;

class svn_browser {
protected:
	svn_item *item;
public:
	svn_browser(svn_item *_item) : item(_item)	{}
	svn_type		type()					const	{ return item ? item->type() : SVN_UNKNOWN;						}
	int				count()					const	{ return type() == SVN_LIST ? int(((svn_list*)item)->size()) : 0;	}
	svn_browser		operator[](int i)		const	{ return type() == SVN_LIST ? (*(svn_list*)item)[i] : 0;		}
	const string&	word()					const	{ return type() == SVN_WORD ? *(svn_word*)item : blank_string;	}
	uint32			number()				const	{ return type() == SVN_NUMBER ? uint32(*(svn_number*)item) : ~uint32(0); }
	void*			data()					const	{ return type() == SVN_STRING ? ((svn_string*)item)->p : 0;	}
	size_t			size()					const	{ return type() == SVN_STRING ? ((svn_string*)item)->size() : 0; }

	bool	operator==(const char *v)		const	{ return type() == SVN_WORD && *(svn_word*)item == v;			}
	bool	operator==(uint32 v)			const	{ return type() == SVN_NUMBER && *(svn_number*)item == v;		}
	bool	operator!=(const char *v)		const	{ return type() != SVN_WORD || *(svn_word*)item != v;			}
	bool	operator!=(uint32 v)			const	{ return type() != SVN_NUMBER || *(svn_number*)item != v;		}
};

class svn_browser_root : public svn_browser {
public:
	svn_browser_root(SOCKET sock) : svn_browser(svn_parse(socket_input(sock))) {}
	svn_browser_root(svn_item *_item) : svn_browser(_item)	{}

	svn_browser_root &operator=(const svn_browser_root &b) {
		delete item;
		item = b.item;
		const_cast<svn_browser_root&>(b).item = 0;
		return *this;
	}

	~svn_browser_root()	{ delete item; }

	bool			write(iso::ostream_ref file)	const {
		return item && item->write(file) && file.putc(' ');
	}

	bool			send(SOCKET sock) const {
		dynamic_memory_writer	out;
		return write(out) && ::send(sock, (const char*)out.data(), out.length(), 0) == out.length();
	}
};

class svn_session {
public:
	SOCKET		sock;
	string		uuid, root, path;
	uint32		latest_rev;
	const char	*user;
	const char	*password;

	bool	Init(const char *url, const char *_user, const char *_password);
	bool	auth_ANON();
	bool	auth_CRAM_MD5();
};

bool svn_session::auth_ANON() {
	svn_browser_root	b = make_svn("ANONYMOUS", make_svn(blank_string));
	if (!b.send(sock))
		return false;

	b	= sock;
	return b[0] == "success";
}

bool svn_session::auth_CRAM_MD5() {
	svn_browser_root	b	= make_svn("CRAM-MD5", make_svn(blank_string));
	if (!b.send(sock))
		return false;

	for (;;) {
		b	= sock;
		if (b[0] == "success")
			return true;

		if (b[0] == "step") {

			const char *challenge = (char*)b[1][0].data();
	//		const char *challenge = "<13760131157977624390.1282553113069400@SERVER>";
			MD5::CODE	digest;
			uint8		secret[64];
			size_t		len = strlen(password);

			// Munge the password into a 64-byte secret.
			clear(secret);
			if (len <= sizeof(secret))
				memcpy(secret, password, len);
			else {
				MD5	md5;
				md5.write(password);
				*(MD5::CODE*)secret = md5;
			}

			for (int i = 0; i < sizeof(secret); i++)
				secret[i] ^= 0x36;

			{
				MD5	md5;
				md5.write(secret);
				md5.write(challenge);
				digest = md5;
			}
			for (int i = 0; i < sizeof(secret); i++)
				secret[i] ^= (0x36 ^ 0x5c);
			{
				MD5	md5;
				md5.write(secret);
				md5.write(digest);
				digest = md5;
			}

			svn_browser_root(new svn_string(buffer_accum<256>(user) << digest)).send(sock);
		}
	}
}


bool svn_session::Init(const char *url, const char *_user, const char *_password) {
	user		= _user;
	password	= _password;

	URLcomponents	comps1(url);
	sock	= socket_address::TCP().connect(comps1.host, 3690);

	if (sock == INVALID_SOCKET)
		return false;

	svn_browser_root	b(sock);
	if (b[0] != "success")
		return false;

//	svn_browser	p		= b[1];
//	int	minver	= p[0].number();
//	int	maxver	= p[1].number();

	b	= make_svn(2, make_svn("edit-pipeline", "mergeinfo"), url);
	if (!b.send(sock))
		return false;

	b	= sock;
	if (b[0] != "success")
		return false;

	uuid	= (const char*)b[1][1].data();

	svn_browser	auth = b[1][0];
	bool		success = false;
	for (int i = 0, n = auth.count(); !success && i < n; i++) {
		if (auth[i] == "ANONYMOUS") {
			success = auth_ANON();
		} else if (auth[i] == "CRAM-MD5") {
			success = auth_CRAM_MD5();
		}
	}

	if (!success)
		return false;

	b	= sock;
	if (b[0] != "success")
		return false;
	root	= (const char*)b[1][1].data();

	URLcomponents	comps2(url);
	path	= comps1.path + strlen(comps2.path);

	b	= make_svn("get-latest-rev", make_svn());
	if (!b.send(sock))
		return false;

	b	= sock;
	if (b[0] != "success")
		return false;
	b	= sock;
	if (b[0] != "success")
		return false;

	latest_rev	= b[1][0].number();

	b	= make_svn("stat", make_svn(blank_string, make_svn(latest_rev)));
	if (!b.send(sock))
		return false;

	if ((b = sock)[0] != "success" || (b = sock)[0] != "success")
		return false;

	return true;
}


class SVN2 {
public:
	SVN2()	{}
	bool	Init(const char *user, const char *pw);

} svn2;

bool SVN2::Init(const char *user, const char *pw) {
	return socket_init();
}

//-----------------------------------------------------------------------------
//	ISO
//-----------------------------------------------------------------------------

struct SVN2_repository : public ISO::VirtualDefaults, svn_session {
	dynamic_array<ISO_ptr<void> >	entries;

	bool Init(const char *url, const char *user, const char *password);

	uint32			Count();
	tag				GetName(int i)		{ return entries[i].ID();	}
	ISO::Browser2	Index(int i)		{ return entries[i];		}
	void			Set(const char *spec, void *val)		{}
};

struct SVN2_dir : public ISO::VirtualDefaults {
	SVN2_repository	*rep;
	string			path;
	dynamic_array<ISO_ptr<void> >	entries;

	void Init(SVN2_repository *_rep, const char *_path);
	uint32			Count();
	tag				GetName(int i)		{ return entries[i].ID();	}
	ISO::Browser2	Index(int i)		{ return entries[i];		}
	void			Set(const char *spec, void *val)		{}
};

struct SVN2_file : public ISO::VirtualDefaults {
	SVN2_repository	*rep;
	string			path;
	ISO_ptr<void>	ptr;

	SVN2_file()	{}
	void Init(SVN2_repository *_rep, const char *_path);
	tag				GetName(int i)		{ return (const char*)filename(filename(path).name()).set_ext(filename(path).ext());	}
	ISO::Browser2	Deref();
	void			Set(const char *spec, void *val)		{}
};

ISO_DEFUSERVIRT(SVN2_repository);
ISO_DEFUSERVIRT(SVN2_dir);
ISO_DEFUSERVIRT(SVN2_file);

bool SVN2_repository::Init(const char *url, const char *user, const char *password) {
	return svn_session::Init(url, user, password);
}

uint32 SVN2_repository::Count() {
	if (!entries.size()) {
		svn_browser_root b	= (new svn_list)
		->add("get-dir")
		->add((new svn_list)
			->add(path)
			->add(make_svn(latest_rev))
			->add("false")
			->add("true")
			->add(
				(new svn_list)->add("kind")->add("size")->add("has-props")->add("created-rev")->add("time")->add("last-author")
			)
		);
		if (b.send(sock) && (b = sock)[0] == "success" && (b = sock)[0] == "success") {
			svn_browser	p = b[1][2];
			for (int i = 0, n = p.count(); i < n; i++) {
				svn_browser	p2		= p[i];
				char		*name	= (char*)p2[0].data();
				if (p2[1] == "dir") {
					ISO_ptr<SVN2_dir>	dir(name);
					dir->Init(this, path + "/" + name);
					entries.push_back(dir);
				} else {
					ISO_ptr<SVN2_file>	file(name);
					file->Init(this, path + "/" + name);
					entries.push_back(file);
				}
			}
		}
	}
	return uint32(entries.size());
}

void SVN2_dir::Init(SVN2_repository *_rep, const char *_path) {
	rep = _rep;
	path = _path;
}

uint32 SVN2_dir::Count() {
	if (!entries.size()) {
		svn_browser_root b	= (new svn_list)
		->add("get-dir")
		->add((new svn_list)
			->add(path)
			->add(make_svn(rep->latest_rev))
			->add("false")
			->add("true")
			->add(
				(new svn_list)->add("kind")->add("size")->add("has-props")->add("created-rev")->add("time")->add("last-author")
			)
		);
		if (b.send(rep->sock) && (b = rep->sock)[0] == "success" && (b = rep->sock)[0] == "success") {
			svn_browser	p = b[1][2];
			for (int i = 0, n = p.count(); i < n; i++) {
				svn_browser	p2		= p[i];
				char		*name	= (char*)p2[0].data();
				if (p2[1] == "dir") {
					ISO_ptr<SVN2_dir>	dir(name);
					dir->Init(rep, path + "/" + name);
					entries.push_back(dir);
				} else {
					ISO_ptr<SVN2_file>	file(name);
					file->Init(rep, path + "/" + name);
					entries.push_back(file);
				}
			}
		}
	}
	return uint32(entries.size());
}

void SVN2_file::Init(SVN2_repository *_rep, const char *_path) {
	rep = _rep;
	path = _path;
}

ISO::Browser2 SVN2_file::Deref() {
	if (!ptr) {
		svn_browser_root b	= (new svn_list)
			->add("get-file")
			->add((new svn_list)
				->add((const char*)path)
				->add(make_svn())
				->add("true")
				->add("true")
			);
		if (b.send(rep->sock) && (b = rep->sock)[0] == "success" && (b = rep->sock)[0] == "success") {
			dynamic_memory_writer	file;
			for (;;) {
				b = rep->sock;
				if (b.type() != SVN_STRING)
					break;
				if (b.size() == 0)
					break;
				file.writebuff(b.data(), b.size());
			}
			b = rep->sock;
			if (b[0] == "success")
				ptr = ReadData1(filename(filename(path).name()).set_ext(filename(path).ext()), memory_reader(file.data()), file.size32(), false);
		}
	}
	return ptr;
};


ISO_ptr<void> GetSVN2(const char *url, const char *user, const char *pw) {
	svn2.Init(user, pw);
	ISO_ptr<SVN2_repository>	rep(url);
	if (rep->Init(url, user, pw))
		return rep;
	return ISO_NULL;
}


//-----------------------------------------------------------------------------
//	device
//-----------------------------------------------------------------------------
#ifdef PLAT_PC
#include "device.h"
#include "resource.h"
#include "windows\registry.h"

namespace app {
RegKey	Settings(bool write = false);
}

struct SVNDevice : app::DeviceT<SVNDevice>, app::DeviceCreateT<SVNDevice> {
	string			svn, user, pw;
	void			operator()(const app::DeviceAdd &add) {
		win::RegKey	reg	= app::Settings();
		svn			= reg.values()["svn"];
		user		= reg.values()["user"];
		pw			= reg.values()["pw"];
		add("SVN...", this, app::LoadPNG("IDB_DEVICE_SVN"));
	}
	ISO_ptr<void>	operator()(const win::Control &main) {
		app::GetURLPWDialog dialog(main, svn, user, pw);
		if (dialog) {
			win::Busy	bee;
			win::RegKey	reg	= app::Settings();
			reg.values()["svn"]		= svn = dialog.url;
			reg.values()["user"]	= user = dialog.user;
			reg.values()["pw"]		= pw = dialog.pw;
			return GetSVN2(svn, user, pw);
		}
		return ISO_NULL;
	}
} svn_device;
#endif


