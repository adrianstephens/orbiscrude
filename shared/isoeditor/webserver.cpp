#include "base/array.h"
#include "comms/HTTP.h"
#include "comms/WebSocket.h"
#include "thread.h"
#include "filename.h"
#include "comms/ssl.h"
#include "extra/text_stream.h"

#ifdef PLAT_PC
#include "windows/registry.h"
#include "windows/win_file.h"
#endif

using namespace iso;

#ifdef PLAT_PC
struct RedirectedProcess : PROCESS_INFORMATION {
	HANDLE	stdin_r, stdin_w, stdout_r, stdout_w;
	bool	ok;

	RedirectedProcess() {
		SECURITY_ATTRIBUTES sa;
		clear(sa);
		sa.nLength				= sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle		= TRUE;
		ok	=	CreatePipe(&stdin_r, &stdin_w, &sa, 0) && SetHandleInformation(stdin_r, HANDLE_FLAG_INHERIT, 0)
			&&	CreatePipe(&stdout_r, &stdout_w, &sa, 0) && SetHandleInformation(stdout_r, HANDLE_FLAG_INHERIT, 0);
	}

	~RedirectedProcess() {
		CloseHandle(hProcess);
		CloseHandle(hThread);
	}

	void FinishInput() {
		CloseHandle(stdin_w);
		CloseHandle(stdout_w);
		stdin_w = stdout_w = 0;
	}

	int	 GetError() {
		return GetLastError();
	}

	bool Run(char *cmdline, void *env) {
		if (!ok)
			return false;

		STARTUPINFO		si;
		clear(si);

		si.cb			= sizeof(STARTUPINFO);
		si.hStdError	= stdout_w;
		si.hStdOutput	= stdout_w;
		si.hStdInput	= stdin_r;
		si.dwFlags		= STARTF_USESTDHANDLES;

		return CreateProcess(NULL,
			cmdline,
			NULL,				// LPSECURITY_ATTRIBUTES	process_attributes,
			NULL,				// LPSECURITY_ATTRIBUTES	thread_attributes,
			TRUE,				// BOOL						inherit_handles,
			CREATE_NO_WINDOW,	// DWORD					creation_flags,
			env,
			NULL,				// current_directory,
			&si,
			this
		);
	}
};
#elif defined PLAT_MAC
struct RedirectedProcess {
	void FinishInput() {
	}
	int	 GetError() {
		return 0;
	}
	bool Run(char *cmdline, void *env) {
		return false;
	}
};
#endif

template<typename T> string EnvVar(const char *name, const T &value) {
	return (buffer_accum<256>(name) << '=' << value).term();
}

void CGI(ostream_ref file, const HTTP &h, const char *base, IP4::addr ip, PORT port) {

	const char *path	= h.path;
	filename	fn		= filename(base);
	bool		got		= false;

	while (const char *next = str(path + 1).find('/')) {
		fn.add_dir(count_string(path, next));
		path = next;
		if (got = is_file(fn))
			break;
	}

	if (!got) {
		fn.add_dir(path);
		if (is_file(fn)) {
			path = 0;
		} else {
			fn	= path + 1;
			path = 0;
		}
	}

	dynamic_array<string>	env;

#if 1
	const char *p = getenv("PATH");
	env.push_back(str("PATH=") + p);
#else
	multi_string		env0(GetEnvironmentStrings());
	for (multi_string::iterator i = env0.begin(); i; ++i) {
		if (istr((char*)i).begins("PATH="))
			env.push_back(i);
	}
#endif

	env.push_back(EnvVar("GATEWAY_INTERFACE",	"CGI/1.1"));
	env.push_back(EnvVar("PATH_INFO",			path));
	env.push_back(EnvVar("PATH_TRANSLATED",		filename(base).add_dir(path)));
	env.push_back(EnvVar("QUERY_STRING",		h.params));
	env.push_back(EnvVar("REMOTE_ADDR",			ip));
	env.push_back(EnvVar("REQUEST_METHOD",		"GET"));
	env.push_back(EnvVar("SCRIPT_NAME",			fn));
	env.push_back(EnvVar("SERVER_NAME",			"127.0.0.1"));
	env.push_back(EnvVar("SERVER_PORT",			port));
	env.push_back(EnvVar("SERVER_PROTOCOL",		"HTTP/1.1"));
	env.push_back(EnvVar("SERVER_SOFTWARE",		"IsoEditor"));

	if (fn.ext() == ".pl") {
		fn = "perl " + fn;
	} else if (fn.ext() == ".php") {
		fn = "php-cgi " + fn;
	} else if (fn.ext() == ".exe") {
		fn = fn + " " + HTTP::Unescape(h.params);
	} else {
		string	s;
		make_text_reader(FileInput(fn)).read_line(s);
		if (s.begins("#!")) {
			fn = filename(s.slice(2)).name() + " " + fn;
		}
	}

	RedirectedProcess	proc;
	if (!proc.Run(fn, *multi_string_alloc<char>((const char**)env.begin(), env.size()).begin())) {
		ISO_TRACEF("Error: %i\n", proc.GetError());
		return;
	}

	proc.FinishInput();

	h.Response(file, HTTP::OK);
#ifdef PLAT_PC
	stream_copy<1024>(HTTPostream(file), lvalue(WinFileInput(proc.stdout_r)));
#else
//	stream_copy<1024>(move(HTTPostream(file)), move(FileInput(proc.stdout_r)));
#endif
}

void WebServer2(HTTP::Context context, const char *base, iostream_ref io, IP4::addr ip, PORT port) {
	for (;;) {
		HTTPistream	in(io);
		if (!in.exists())
			break;

		if (in.request.begins("GET")) {

			if (in.headers.check("upgrade", "websocket")) {
				WebSocketServer	ws(context, in.headers, io);
				while (auto message = ws.Process()) {
					if (message.type == WebSocket::Text)
						ISO_OUTPUTF("Websock received: ") << str8(message.data) << '\n';
				
					ws.SendText("S'up Dawk?");
				}

			} else {
				HTTP		http(context, in.request + 4);

				if (http.path == cstr("/"))
					http.path = (char*)"/home.html";

				if (http.params && *http.params) {
					CGI(io, http, base, ip, port);

				} else {
					filename	fn	= filename(base).add_dir(http.path);
					if (!exists(fn)) {
						http.Response(io, HTTP::NotFound);
					} else {
						streamptr		len = filelength(fn);
						if (http.Response(io, HTTP::OK, "Content-Encoding: text\r\n", 0, len)) {
							stream_copy<256>(io, unconst(FileInput(fn)));
						}
					}
				}
			}
		}
		if (!in.keep_alive())
			break;
	}
}

void WebServer(const char *dir, PORT port) {
	HTTP::Context context("WebServer");

	Socket listener = IP4::socket_addr(port).listener();
	if (listener.exists()) for (;;) {
		IP4::socket_addr	addr;
		SOCKET				sock = addr.accept(listener);

		if (sock == INVALID_SOCKET)
			break;

		ISO_TRACE("Create Webserver thread\n");
		RunThread([=]() {
			WebServer2(context, dir, Socket(sock), IP4::socket_addr(sock).ip, port);
		});
	}
}

void WebServerS(const char *dir, PORT port) {
	HTTP::Context context("WebserverS");

	Socket listener = IP4::socket_addr(port).listener();
	if (listener.exists()) for (;;) {
		IP4::socket_addr	addr;
		SOCKET				sock = addr.accept(listener);

		if (sock == INVALID_SOCKET)
			break;

		ISO_TRACE("Create Webserver thread\n");
		RunThread([=]() {
			SSL::Connection		con;
			Socket				socket(sock);
			if (con.ServerConnect(socket))
				WebServer2(context, dir, SSL::SSL_iostream(con, socket), IP4::socket_addr(sock).ip, port);
		});
	}
}

void StartWebServer(const char *dir, PORT port, bool secure) {
	socket_init();
	RunThread([=]()	{
		secure ? WebServerS(dir, port) : WebServer(dir, port);
	});
}

