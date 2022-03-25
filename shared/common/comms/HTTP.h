#ifndef HTTP_H
#define HTTP_H

#include "ip.h"
#include "base/array.h"
#include "base/algorithm.h"

namespace iso {

//-----------------------------------------------------------------------------
//	URLcomponents
//-----------------------------------------------------------------------------

struct URLcomponents0 {
	enum {
		FTP			= 21,
		HTTP		= 80,
		HTTPS		= 443,	// Hypertext Transfer Protocol with SSL(https).
		SSMTP		= 465,	// Simple Mail Transfer Protocol with SSL(ssmtp).
		SNNTP		= 563,	// Network News Transfer Protocol(snntp).
	};
	enum class FLAGS {
		SCHEME		= 1 << 0,
		CREDENTIALS	= 1 << 1,
		HOST		= 1 << 2,
		PORT		= 1 << 3,
		PATH		= 1 << 4,
		PARAMS		= 1 << 5,
		ANCHOR		= 1 << 6,

		FORCE_PORT	= 1 << 16,
		LOCATION	= SCHEME | CREDENTIALS | HOST | PORT | PATH,
		ALL			= LOCATION | PARAMS | ANCHOR
	};
	friend constexpr FLAGS	operator|(FLAGS a, FLAGS b) { return FLAGS((int)a | (int)b); }
	friend constexpr bool	operator&(FLAGS a, FLAGS b) { return !!((int)a & (int)b); }

	char	*scheme, *username, *password, *host, *path, *params, *anchor;
	uint16	port;

	static string Escape(const char *s);
	static string Unescape(const char *s);
	static uint16 DefaultPort(const char *scheme);

	char*	Parse(const char *s, const char *end, char *d);

	URLcomponents0(uint16 def_port = 0) : scheme(0), username(0), password(0), host(0), path(0), params(0), anchor(0), port(def_port) {}

	parts<'?'>			Params()		const	{ return params; }
	fixed_string<1024>	PathParams() 	const	{ fixed_string<1024> t(path); if (params) t << '?' << params; return t; }
	fixed_string<1024>	GetURL(uint16 def_port = 0, FLAGS flags = FLAGS::ALL) const;
	fixed_string<1024>	GetURL(const char *rel, uint16 def_port = 0, FLAGS flags = FLAGS::ALL) const;
	fixed_string<1024>	GetURL(const URLcomponents0 &url2, uint16 def_port = 0, FLAGS flags = FLAGS::ALL) const;

	SOCKET	Connect()	const { return socket_address::TCP().connect(host, port); }
};

class URLcomponents : public URLcomponents0 {
	malloc_block	buffer;

	void	Parse(const char* s, const char* end) {
		auto	d	= URLcomponents0::Parse(s, end, buffer.create(end - s + 8));
		ISO_ASSERT(d <= buffer.end());
	}
public:
	URLcomponents(const char *url, uint16 def_port = 0) 		: URLcomponents0(def_port)	{ Parse(url, string_end(url)); }
	URLcomponents(const string &url, uint16 def_port = 0) 		: URLcomponents0(def_port)	{ Parse(url.begin(), url.end()); }
	URLcomponents(const count_string &url, uint16 def_port = 0) : URLcomponents0(def_port)	{ Parse(url.begin(), url.end()); }
};

struct HTTP_Header {
	string	key, value;
	HTTP_Header() {}
	HTTP_Header(const char *key, const char *value) : key(key), value(value) {}
	bool	operator==(const char *_key) const	{ return key == istr(_key); }
};
struct HTTP_Headers : dynamic_array<HTTP_Header> {
	HTTP_Headers()	{}
	template<typename V, typename...TT> HTTP_Headers(const char *key, const V &value, const TT&...tt) {
		add(key, value);
		add(tt...);
	}
	template<typename V, typename...TT> void add(const char *key, const V &value, const TT&...tt) {
		add(key, value);
		add(tt...);
	}
	template<typename V> void add(const char *key, const V &value) {
		emplace_back(key, to_string(value));
	}
	void	add() {}
	void	add(const char *key, const char *value) {
		emplace_back(key, value);
	}
	const char*	get(const char *key) const {
		if (auto h = find_check(*this, key))
			return h->value;
		return 0;
	}
	template<typename T> T	get(const char *key, T def = T()) const {
		if (auto h = find_check(*this, key))
			return from_string<T>(h->value);
		return def;
	}
	bool	check(const char *key, const char *value) const {
		if (auto h = find_check(*this, key))
			return h->value == istr(value);
		return false;
	}
	void	remove(const char *key) {
		if (auto h = find_check(*this, key))
			erase(h);
	}

	friend string_accum& operator<<(string_accum& sa, const HTTP_Headers& headers) {
		for (auto &i : headers)
			sa << i.key << ": " << i.value << "\r\n";
		return sa;
	}
};

struct HTTP_Context : refs<HTTP_Context> {
	uint16			version;
	string			user_agent;
	HTTP_Headers	headers;
	HTTP_Context(const char *user_agent) : version(0x0101), user_agent(user_agent), headers("Cache-Control", "no-cache") {}
};

//-----------------------------------------------------------------------------
//	HTTP streams
//-----------------------------------------------------------------------------

struct HTTPbuffer {
	streamptr	current, end;
	uint16		buff_off, buff_end;
	char		buff[2048];
	HTTPbuffer() : current(0), end(-1), buff_off(0), buff_end(0) {}
	void				reset()				{ current = 0; end = -1; buff_off = buff_end = 0; }
	const_memory_block	buffered() const	{ return const_memory_block(buff + buff_off, buff_end - buff_off); }
};

class HTTPistream : public istream_ptr {
	struct input : istream_ptr, HTTPbuffer {
		bool		chunked;
		input(istream_ptr &&file) : istream_ptr(move(file)), chunked(false) {}
		size_t		more() {
			size_t		r	= istream_ptr::readbuff(buff + buff_end, sizeof(buff) - buff_end);
			buff_end	+= r;
			return r;
		}
		void	reset(istream_ptr &&file) {
			HTTPbuffer::reset();
			istream_ptr::operator=(move(file));
		}
		size_t		readbuff(void *buffer, size_t size);
		int			getc()						{ uint8 c; return readbuff(&c, 1) ? c : EOF; }
		void		seek(streamptr offset);
		void		seek_cur(streamptr offset)	{ seek(current + offset); }
		void		seek_end(streamptr offset)	{ seek(end + offset); }
		streamptr	length()					{ return end; }
		streamptr	tell()						{ return current; }
		istream_ptr	_clone()					{ return none; }
	};
	input	in;
	void	start();
public:
	uint16			http_code, http_version;
	string			request;
	HTTP_Headers	headers;

	HTTPistream(HTTPistream&&)		= default;
	HTTPistream(istream_ptr &&file) : in(move(file))	{ start(); }
	HTTPistream(istream_ref file)	: in(file)	{ start(); }
	istream_ptr		_clone()		{ return none; }

	bool		keep_alive() const	{ return headers.check("connection", "keep-alive"); }
	auto&		raw()				{ return (istream_ptr&)in; }
	auto		detach_raw()		{ return move((istream_ptr&)in); }
};

struct HTTPinput : holder<socket_input>, HTTPistream {
	HTTPinput(SOCKET sock)	: holder<socket_input>(sock), HTTPistream(reader_intf(t)) {}
	SOCKET		_clone()		{ return INVALID_SOCKET; }
};

class HTTPostream : writer_mixin<HTTPostream> {
	struct output : HTTPbuffer, ostream_ptr {
		bool		chunked;
		output(ostream_ptr &&file) : ostream_ptr(move(file)), chunked(true) {}
		void		flush();
		void		finish();
		streamptr	tell() { return current; }
		size_t		writebuff(const void *buffer, size_t size);
	};
	output	out;
public:
	HTTPostream(HTTPostream&&)		= default;
	HTTPostream(ostream_ptr &&file) : out(move(file))		{}
	~HTTPostream()											{ out.finish(); }
	size_t		writebuff(const void *buffer, size_t size)	{ return out.writebuff(buffer, size); }
};

struct HTTPoutput : holder<socket_output>, HTTPostream {
	HTTPoutput(SOCKET sock)	: holder<socket_output>(sock), HTTPostream(writer_intf(t)) {}
};

//-----------------------------------------------------------------------------
//	HTTP
//-----------------------------------------------------------------------------

class HTTP : public URLcomponents {
public:
	enum CODE {
		Continue					= 100,	// Continue
		SwitchingProtocols			= 101,	// Switching Protocols
		OK							= 200,	// OK
		Created						= 201,	// Created
		Accepted					= 202,	// Accepted
		NonAuthoritativeInformation	= 203,	// Non-Authoritative Information
		NoContent					= 204,	// No Content
		ResetContent				= 205,	// Reset Content
		PartialContent				= 206,	// Partial Content
		MultipleChoices				= 300,	// Multiple Choices
		MovedPermanently			= 301,	// Moved Permanently
		Found						= 302,	// Found
		SeeOther					= 303,	// See Other
		NotModified					= 304,	// Not Modified
		UseProxy					= 305,	// Use Proxy
		TemporaryRedirect			= 307,	// Temporary Redirect
		BadRequest					= 400,	// Bad Request
		Unauthorized				= 401,	// Unauthorized
		PaymentRequired				= 402,	// Payment Required
		Forbidden					= 403,	// Forbidden
		NotFound					= 404,	// Not Found
		MethodNotAllowed			= 405,	// Method Not Allowed
		NotAcceptable				= 406,	// Not Acceptable
		ProxyAuthenticationRequired	= 407,	// Proxy Authentication Required
		RequestTimeout				= 408,	// Request Time-out
		Conflict					= 409,	// Conflict
		Gone						= 410,	// Gone
		LengthRequired				= 411,	// Length Required
		PreconditionFailed			= 412,	// Precondition Failed
		RequestEntityTooLarge		= 413,	// Request Entity Too Large
		RequestURITooLarge			= 414,	// Request-URI Too Large
		UnsupportedMediaType		= 415,	// Unsupported Media Type
		Requestedrangenotsatisfiable= 416,	// Requested range not satisfiable
		ExpectationFailed			= 417,	// Expectation Failed
		InternalServerError			= 500,	// Internal Server Error
		NotImplemented				= 501,	// Not Implemented
		BadGateway					= 502,	// Bad Gateway
		ServiceUnavailable			= 503,	// Service Unavailable
		GatewayTimeout				= 504,	// Gateway Time-out
		HTTPVersionnotsupported		= 505,	// HTTP Version not supported
	};

	struct Context : ref_ptr<HTTP_Context> {
		Context(const char *user_agent) : ref_ptr<HTTP_Context>(make(user_agent)) {}
		template<typename...TT> Context(const char *user_agent, const TT&... tt) : ref_ptr<HTTP_Context>(make(user_agent)) {
			get()->headers.add(tt...);
		}
	};

	Context			context;

	HTTP(Context context, const char *url) : URLcomponents(url), context(context) {}
	bool			PutMessage(ostream_ref file, string_accum &sa, const char *headers = 0, const void *data = 0, size_t datalen = 0) const;
	bool			Response(ostream_ref file, CODE code, const char *headers = 0, const void *data = 0, size_t datalen = 0) const;
	bool			Request(ostream_ref file, const char *verb, const char *object, const char *headers = 0, const void *data = 0, size_t datalen = 0) const;

	bool			Response(ostream_ref file, CODE code, const char* headers, const const_memory_block& data) const {
		return Response(file, code, headers, data, data.length());
	}
	bool			Request(ostream_ref file, const char* verb, const char* object, const char* headers, const const_memory_block& data) const {
		return Request(file, verb, object, headers, data, data.length());
	}

	// with sockets
	SOCKET			Response(SOCKET sock, CODE code, const char *headers = 0, const const_memory_block &data = none) const {
		if (sock != INVALID_SOCKET) {
			if (Response(socket_output(sock), code, headers, data))
				return sock;
			socket_error("HTTP");
			socket_close(sock);
		}
		return INVALID_SOCKET;
	}
	SOCKET			Request(SOCKET sock, const char *verb, const char *object, const char *headers = 0, const const_memory_block &data = none) const {
		if (sock != INVALID_SOCKET) {
			if (Request(socket_output(sock), verb, object, headers, data))
				return sock;
			socket_error("HTTP");
			socket_close(sock);
		}
		return INVALID_SOCKET;
	}

	// HTTP_Headers headers
	template<typename S> auto	Response(S&& file, CODE code, const HTTP_Headers &headers, const const_memory_block &data = none) const {
		return Response(forward<S>(file), code, string() << headers, data);
	}
	template<typename S> auto	Request(S&& file, const char *verb, const char *object, const HTTP_Headers &headers, const const_memory_block &data = none) const {
		return Request(forward<S>(file), verb, object, string() << headers, data);
	}

	// make socket
	SOCKET			Request(const char *verb, const char *object, const char *headers = 0, const const_memory_block &data = none) const {
		return Request(Connect(), verb, object, headers, data);
	}
	SOCKET			Request(const char *verb, const char *object, const HTTP_Headers &headers, const const_memory_block &data = none) const {
		return Request(Connect(), verb, object, headers, data);
	}

	istream_ptr		Get(const char *object, const char *headers = 0);
	ostream_ptr		Put(const char *object, const char *headers = 0);
	
	istream_ptr		GetSecure(const char *object, const char *headers = 0);
	ostream_ptr		PutSecure(const char *object, const char *headers = 0);

	bool	is_secure() const { return scheme == cstr("https"); }
};

inline HTTPistream HTTPopenURL(HTTP::Context context, const char *url, const char *headers = 0) {
	HTTP	http(context, url);
	return http.Get(http.PathParams(), headers);
}

} // namespace iso
#endif //HTTP_H
