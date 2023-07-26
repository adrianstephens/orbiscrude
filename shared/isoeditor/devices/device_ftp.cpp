#include "device.h"
#include "comms/http.h"
#include "comms/ftp.h"
#include "filename.h"
#include "events.h"
#include "windows/registry.h"

#ifdef PLAT_PC
#include "windows/win_file.h"
#endif

namespace app {
RegKey	Settings(bool write = false);
}

using namespace app;

struct FTPsite : public refs<FTPsite>, public FTP {
	SOCKET	sock;
	CODE	last_code;

	FTPsite(const char *host, const char *user, const char *password) : FTP(host), last_code(NO_CONNECTION) {
		sock	= Connect();
		if (sock != INVALID_SOCKET)
			last_code = Login(sock, user, password);
	}
	~FTPsite() {
		if (sock != INVALID_SOCKET) {
			Command(sock, QUIT);
			ReceiveDiscardReply(sock);
		}
	}

	CODE		GetDirectory(const char *dir, dynamic_array<ISO_ptr<void> > &entries);
	SOCKET		RequestFile(const char *name) {
		if (sock != INVALID_SOCKET) {
			SOCKET	sock_in;
			if (FTP::RequestFile(sock, filename(name).convert_to_fwdslash(), sock_in) == FileOK)
				return sock_in;
			socket_close(sock_in);
		}
		return INVALID_SOCKET;
	}
};

struct FTPdir : public ISO::VirtualDefaults {
	ref_ptr<FTPsite>	site;
	string				name;
	bool				scanned;
	dynamic_array<ISO_ptr<void> >	entries;

	FTPdir(const pair<FTPsite*,const char *> &args) : site(args.a), name(args.b), scanned(false) {}

	uint32			Count()				{ if (!scanned) {scanned = true; site->GetDirectory(name, entries); } return (uint32)entries.size(); }
	tag				GetName(int i)		{ return entries[i].ID();	}
	ISO::Browser2	Index(int i)		{ return entries[i];		}
};

struct FTPfile : public ISO::VirtualDefaults {
	ref_ptr<FTPsite>	site;
	string				name;
	uint64				size;
	ISO_ptr<void>		ptr;

	FTPfile(const triple<FTPsite*,const char*,uint64> &args) : site(args.a), name(args.b), size(args.c) {}
	ISO::Browser2	Deref() {
		if (!ptr) {
			Socket	sock = site->RequestFile(name);
			if (sock.exists()) {
				ISO_ptr<ISO_openarray<uint8> >	p(tag2(), size);
				readbuff_all(sock, *p, size);
				ptr	= p;
				site->ReceiveDiscardReply(site->sock);
			}
		}
		return ISO::MakeBrowser(ptr);
	}
};

FTP::CODE FTPsite::GetDirectory(const char *dir, dynamic_array<ISO_ptr<void> > &entries) {
	if (sock == INVALID_SOCKET)
		return (last_code = NO_CONNECTION);

	SOCKET	sock_in;
	CODE	code	= GetPassiveSocket(sock, sock_in);
	if (code == Passive) {
		Command(sock, MLSD, dir);

		code = ReceiveDiscardReply(sock);
		if (code == FileOK) {
			string_builder	b;
			char	buffer[256];
			while (size_t len = socket_receive(sock_in, buffer, sizeof(buffer)))
				b.merge(buffer, len);
			code = ClosePassiveSocket(sock, sock_in);

			string_scan	ss(b.term());
			while (ss.skip_whitespace().remaining()) {
				count_string	tok;
				int				type	= 0;
				uint64			size	= 0;
				while (ss.peekc() != ' ') {
					tok = ss.get_token(~char_set(';'));
					if (tok.begins("type=")) {
						count_string	val = str(tok.find('=') + 1, tok.end());
						type = val == "file" ? 1 : val == "dir" ? 2 : 0;
					} else if (tok.begins("size=")) {
						from_string(tok.find('=') + 1, size);
					}
					ss.move(1);
				}
				tok	= ss.get_token();
				if (type == 1) {
					entries.push_back(ISO_ptr<FTPfile>(tok, make_triple(this, filename(dir).add_dir(tok), size)));
				} else if (type == 2) {
					entries.push_back(ISO_ptr<FTPdir>(tok, make_pair(this, filename(dir).add_dir(tok))));
				}
			}
		}
	}
	return (last_code = code);
}

ISO_DEFUSERVIRT(FTPdir);
ISO_DEFUSERVIRT(FTPfile);

//ftp://[<user>[:<password>]@]<host>[:<port>]/<url-path>

#ifdef PLAT_PC

struct FTPDevice : app::DeviceT<FTPDevice>, MenuCallbackT<FTPDevice> {
	int	id;

	struct FTPentry : DeviceCreateT<FTPentry>, Handles2<FTPentry, AppEvent> {
		string name;
		string text;
		ISO_ptr<void>	operator()(const Control &main) {
			Busy			bee;
			URLcomponents	url(text);
			FTPsite			*site = new FTPsite(URLcomponents::Unescape(url.host), URLcomponents::Unescape(url.username), URLcomponents::Unescape(url.password));
			if (site->last_code == FTP::LoggedIn)
				return ISO_ptr<FTPdir>(name, make_pair(site, url.path));
			delete site;
			return ISO_NULL;
		}
		void	operator()(AppEvent *ev) {
			if (ev->state == AppEvent::END)
				delete this;
		}
		FTPentry(const char *name, const char *text) : name(name), text(text)	{}
	};
	void			ClearMenu(Menu m) {
		Menu::Item item(MIIM_DATA | MIIM_SUBMENU);
		while (item._GetByPos(m, 0)) {
			if (Menu m2 = item.SubMenu())
				ClearMenu(m2);
			else if (FTPentry *entry = item.Param())
				delete entry;
			m.RemoveByPos(0);
		}
	}

	void			operator()(const DeviceAdd &add) {
		add("FTP sites", this);
	}
	void			operator()(Control c, Menu m) {
		ClearMenu(m);
		RegKey	reg	= Settings()["ftp"];
		DeviceAdd	add(m, id);
		for (auto v : reg.values())
			add(v.name, new FTPentry(v.name, str<256>(v.get_text())));
	}
} ftpdevice;

#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#include "thread.h"

#ifdef PLAT_PC

struct FTPThread : Thread {
	FTP				ftp;
	const char		*user, *password, *infile, *outfile;
	streamptr		offset, size;

	int operator()() {
		Socket		sock	= ftp.Connect(), psock;
		FTP::CODE	code	= ftp.Login(sock, user, password);

		if (code == FTP::LoggedIn) {
			code = ftp.RequestFileSeek(sock, infile, psock, offset);
			if (code == FTP::FileOK) {
				win_filewriter	output(outfile, GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_ALWAYS);
				output.seek(offset);
//				FileOutput		output(format_string("C:\\test_%08x.bin", offset));
				socket_input	input(psock);
				char			buffer[1024];
				uint32			read;
				while (size && (read = input.readbuff(buffer, min(size, 1024)))) {
					output.writebuff(buffer, read);
					size -= read;
				}
			}
		}

		delete this;
		return 0;
	}

	FTPThread(const char *_host, const char *_user, const char *_password, const char *_infile, const char *_outfile, streamptr _offset, streamptr _size)
	: Thread(this), ftp(_host), user(_user), password(_password), infile(_infile), outfile(_outfile), offset(_offset), size(_size) {
		Start();
	}
};
void GetFTP(const char *host, const char *user, const char *password, const char *infile, const char *outfile) {
	socket_init();

	uint64		filelength	= 0;

	FTP			ftp(host);
	Socket		sock	= ftp.Connect();
	FTP::CODE	code	= ftp.Login(sock, user, password);
	if (code == FTP::LoggedIn) {
		ftp.Command(sock, FTP::MLST, infile);
		string_builder	reply;
		if (ftp.ReceiveReply(sock, reply) == FTP::ActionCompleted) {
			string_scan	ss(reply.term());
			if (ss.scan_skip("size="))
				filelength = ss.get<uint64>();
		}
	}

#if 1
	if (filelength) {
		uint64	chunk = next_pow2(filelength / 8);
		for (int i = 0; i < 8; i++) {
			new FTPThread(host, user, password, infile, outfile, i * chunk, i == 7 ? filelength - chunk * 7 : chunk);
		}
	}
#else
	FTP			ftp(host);
	FTP::CODE	code;
	Socket		sock	= ftp.Connect();

	code = ftp.Login(sock, user, password);
	if (code == FTP::LoggedIn) {
		Socket	psock;
		code = ftp.RequestFileSeek(sock, file, psock, 0);
		if (code == FTP::FileOK) {
			socket_input	input(psock);
			FileOutput		output("C:\\test.bin");
			char			buffer[1024];
			copy(output, input, buffer, sizeof(buffer));
		}
	}
#endif
}
#endif

