#include "HTTP.h"
#include "gz.h"
#include "zlib_stream.h"

#ifdef USE_SSL
#include "ssl.h"
#endif

namespace iso {

uint16 URLcomponents0::DefaultPort(const char *scheme) {
	return	scheme == cstr("http")	? HTTP
		:	scheme == cstr("https")	? HTTPS
		:	scheme == cstr("ftp")	? FTP
		:	scheme == cstr("ssmtp")	? SSMTP
		:	scheme == cstr("snntp")	? SNNTP
		:	scheme == cstr("ws")	? HTTP
		:	scheme == cstr("wss")	? HTTPS
		:	0;
}

char *URLcomponents0::Parse(const char *s, const char *end, char *d) {
	const char	*colon;

	if ((colon = string_find(s, end, ':')) && (end - colon > 6 || !string_check(colon + 1, char_set::digit, end))) {
		memcpy(scheme = d, s, colon - s);
		d += colon - s;
		*d++ = 0;
		s = colon + 1;
	}

	const char *query	= string_find(s, end, '?');
	const char *hash	= string_find(query ? query : s, end, '#');
	const char *end_path = query ? query : hash ? hash : end;

	if (s[0] == '/' && s[1] == '/') {
		s += 2;
		if (const char *at = string_find(s, end_path, '@')) {
			if (colon = string_find(s, at, ':')) {
				memcpy(username = d, s, colon - s);
				d	+= colon - s;
				*d++ = 0;
				s	= colon + 1;
				memcpy(password = d, s, at - s);
			} else {
				memcpy(username = d, s, at - s);
			}
			d	+= at - s;
			*d++ = 0;
			s	= at + 1;
		}

		const char *slash = string_find(s, end_path, '/');
		if (!slash)
			slash = end_path;

		if (colon = string_find(s, slash, ':')) {
			auto	pe = get_num_base<10>(colon + 1, port);
			ISO_ASSERT(pe == slash);
			memcpy(host = d, s, colon - s);
			d += colon - s;
			*d++ = 0;
		} else {
			memcpy(host = d, s, slash - s);
			d += slash - s;
			*d++ = 0;

			if (!port)
				port = DefaultPort(scheme);
		}

		s = slash;// + int(*slash != 0);
	}

	if (s != end_path)
		memcpy(path = d, s, end_path - s);
		
	d += end_path - s;
	*d++ = 0;

	if (query) {
		const char *end_query = hash ? hash : end;
		memcpy(params = d, query + 1, end_query - query - 1);
		d += end_query - query - 1;
		*d++ = 0;
	}

	if (hash) {
		memcpy(anchor = d, hash + 1, end - hash + 1);
		d += end - hash - 1;
		*d++ = 0;
	}
	
	return d;

}

fixed_string<1024> URLcomponents0::GetURL(uint16 def_port, FLAGS flags) const {
	buffer_accum<1024>	a;

	if (scheme && (flags & FLAGS::SCHEME))
		a << scheme << "://";

	if (username && (flags & FLAGS::CREDENTIALS)) {
		a << '@' << username;
		if (password)
			a << ':' << password;
	}

	if (host && (flags & FLAGS::HOST))
		a << host;
		
	if (flags & (FLAGS::PORT|FLAGS::FORCE_PORT)) {
		if (!def_port)
			def_port = DefaultPort(scheme);
		if ((flags & FLAGS::FORCE_PORT) || port != def_port)
			a << ':' << (port ? port : def_port);
	}

	if (path && (flags & FLAGS::PATH))
		a << onlyif(path[0] != '/', '/') << path;

	if (params && (flags & FLAGS::PARAMS))
		a << '?' << params;

	if (anchor && (flags & FLAGS::ANCHOR))
		a << '#' << anchor;

	return str(a);
}

fixed_string<1024> URLcomponents0::GetURL(const char *rel, uint16 def_port, FLAGS flags) const {
	buffer_accum<1024>	a;

	if (scheme && (flags & FLAGS::SCHEME))
		a << scheme << "://";

	if (username && (flags & FLAGS::CREDENTIALS)) {
		a << '@' << username;
		if (password)
			a << ':' << password;
	}

	if (host && (flags & FLAGS::HOST))
		a << host;

	if (flags & (FLAGS::PORT|FLAGS::FORCE_PORT)) {
		if (!def_port)
			def_port = DefaultPort(scheme);
		if ((flags & FLAGS::FORCE_PORT) || port != def_port)
			a << ':' << (port ? port : def_port);
	}

	if (rel[0] == '/') {
		a << rel;
	} else {
		if (path) {
			if (const char *s = str(path).rfind('/'))
				a << str(path, s - path);
		}
		a << '/' << rel;
	}

	return str(a);
}

fixed_string<1024> URLcomponents0::GetURL(const URLcomponents0 &url2, uint16 def_port, FLAGS flags) const {
	buffer_accum<1024>	a;
		
	if (flags & FLAGS::SCHEME) {
		if (url2.scheme)
			a << url2.scheme << "://";
		else if (scheme)
			a << scheme << "://";
	}

	if (username && (flags & FLAGS::CREDENTIALS)) {
		a << '@' << username;
		if (password)
			a << ':' << password;
	}

	if (flags & FLAGS::HOST) {
		if (url2.host)
			a << url2.host;
		else if (host)
			a << host;
	}
		
	if (flags & (FLAGS::PORT|FLAGS::FORCE_PORT)) {
		if (!def_port)
			def_port = DefaultPort(url2.scheme ? url2.scheme : scheme);
		if ((flags & FLAGS::FORCE_PORT) || port != def_port)
			a << ':' << (port ? port : def_port);
	}

	if (flags & FLAGS::PATH) {
		if (url2.path) {
			if (url2.path[0] == '/') {
				a << url2.path;
		
			} else if (path) {
				if (const char *s = str(path).rfind('/'))
					a << str(path, s - path);
				a << '/' << url2.path;
			}
		} else if (path) {
			a << path;
		}
	}

	if ((flags & FLAGS::PARAMS) && url2.params)
		a << '?' << url2.params;

	if ((flags & FLAGS::ANCHOR) && (anchor || url2.anchor))
		a << '#' << ifelse(url2.anchor, url2.anchor, anchor);
		
	return str(a);
}


string URLcomponents0::Escape(const char *s) {
	static char_set need_escape("<>#%\";/?:@&={}|\\^~[]`");
	string_builder	b;
	while (char c = *s++) {
		if (need_escape.test(c)) {
			b.putc('%');
			b.putc(to_digit(uint8(c) >> 4));
			c = to_digit(uint8(c) & 15);
		}
		b.putc(c);
	}
	return b;
}

string URLcomponents0::Unescape(const char *s) {
	string_builder	b;
	if (s) {
		while (char c = *s++) {
			if (c == '%') {
				c = from_digit(s[0]) * 16 + from_digit(s[1]);
				s += 2;
			}
			b.putc(c);
		}
	}
	return b;
}

//-----------------------------------------------------------------------------
//	HTTP
//-----------------------------------------------------------------------------

//-----------------------------------------------
//	Header								Status
//-----------------------------------------------
/*
A-IM
Accept									standard
Accept-Additions
Accept-Charset							standard
Accept-Datetime							informational
Accept-Encoding							standard
Accept-Features
Accept-Language							standard
Accept-Patch
Accept-Post								standard
Accept-Ranges							standard
Age										standard
Allow									standard
ALPN									standard
Alt-Svc									standard
Alt-Used								standard
Alternates
Apply-To-Redirect-Ref
Authentication-Control					experimental
Authentication-Info						standard
Authorization							standard
C-Ext
C-Man
C-Opt
C-PEP
C-PEP-Info
Cache-Control							standard
CalDAV-Timezones						standard
Close									reserved
Connection								standard
Content-Disposition						standard
Content-Encoding						standard
Content-ID
Content-Language						standard
Content-Length							standard
Content-Location						standard
Content-MD5
Content-Range							standard
Content-Script-Type
Content-Style-Type
Content-Type							standard
Content-Version
Cookie									standard
DASL									standard
DAV										standard
Date									standard
Default-Style
Delta-Base
Depth									standard
Derived-From
Destination								standard
Differential-ID
Digest
ETag									standard
Expect									standard
Expires									standard
Ext
Forwarded								standard
From									standard
GetProfile
Hobareg									experimental
Host									standard
HTTP2-Settings							standard
IM
If										standard
If-Match								standard
If-Modified-Since						standard
If-None-Match							standard
If-Range								standard
If-Schedule-Tag-Match					standard
If-Unmodified-Since						standard
Keep-Alive
Label
Last-Modified							standard
Link
Location								standard
Lock-Token								standard
Man
Max-Forwards							standard
Memento-Datetime						informational
Meter
MIME-Version							standard
Negotiate
Opt
Optional-WWW-Authenticate				experimental
Ordering-Type							standard
Origin									standard
Overwrite								standard
P3P
PEP
PICS-Label
Pep-Info
Position								standard
Pragma									standard
Prefer									standard
Preference-Applied						standard
ProfileObject
Protocol
Protocol-Info
Protocol-Query
Protocol-Request
Proxy-Authenticate						standard
Proxy-Authentication-Info				standard
Proxy-Authorization						standard
Proxy-Features
Proxy-Instruction
Public
Public-Key-Pins							standard
Public-Key-Pins-Report-Only				standard
Range									standard
Redirect-Ref
Referer									standard
Retry-After								standard
Safe
Schedule-Reply							standard
Schedule-Tag							standard
Sec-WebSocket-Accept					standard
Sec-WebSocket-Extensions				standard
Sec-WebSocket-Key						standard
Sec-WebSocket-Protocol					standard
Sec-WebSocket-Version					standard
Security-Scheme
Server									standard
Set-Cookie								standard
SetProfile
SLUG									standard
SoapAction
Status-URI
Strict-Transport-Security				standard
Surrogate-Capability
Surrogate-Control
TCN
TE										standard
Timeout									standard
Topic									standard
Trailer									standard
Transfer-Encoding						standard
TTL										standard
Urgency									standard
URI
Upgrade									standard
User-Agent								standard
Variant-Vary
Vary									standard
Via										standard
WWW-Authenticate						standard
Want-Digest
Warning									standard
X-Frame-Options							informational
*/

const char *get_http_version(const char *p, uint16 &version) {
	uint8	hi = 0, lo = 0;
	p += from_string(p, hi);
	if (*p == '.') {
		++p;
		p += from_string(p, lo);
	}
	version	= (hi << 8) + lo;
	return p;
};

string_accum& put_http_version(string_accum &sa, uint16 version) {
	return sa << "HTTP/" << (version >> 8) << '.' << (version & 0xff);
};

bool HTTP::PutMessage(ostream_ref file, string_accum &sa, const char *headers, const void *data, size_t datalen) const {
	sa	<< "Host: " << host << ':' << port << "\r\n"
		<< "User-Agent: " << context->user_agent << "\r\n"
		<< context->headers
		<< headers;

	if (datalen)
		sa << "Content-length: " << datalen << "\r\n";

	sa << "\r\n";

	if (data && datalen <= sa.remaining()) {
		sa.merge((const char*)data, datalen);
		data = 0;
	}
	return check_writebuff(file, sa.begin(), sa.length())
		&& (!data || check_writebuff(file, data, datalen));
}

bool HTTP::Request(ostream_ref file, const char *verb, const char *object, const char *headers, const void *data, size_t datalen) const {
	if (!object)
		object = "";
	else if (object[0] == '/')
		object++;

	return PutMessage(file,
		put_http_version(buffer_accum<1024>() << verb << " /" << object << ' ', context->version) << "\r\n",
		headers, data, datalen
	);
}

bool HTTP::Response(ostream_ref file, CODE code, const char *headers, const void *data, size_t datalen) const {
	return PutMessage(file,
		put_http_version(unconst(buffer_accum<1024>()), context->version) << ' ' << code << "\r\n",
		headers, data, datalen
	);
}

#ifdef SSL_H
struct SSL_istream2 : public reader_mixin<SSL_istream2>, SSL::SSL_input {
	SSL::Connection		con;
	Socket				sock;

	SSL_istream2(SOCKET s) : sock(s) {
		if (!con.ClientConnect(sock))
			sock.close();
	}
	SSL_istream2(const HTTP &http, const char *object, const char *headers = 0) : sock(http.Connect()) {
		if (!con.ClientConnect(sock) || !http.Request(SSL::SSL_ostream(con, sock), "GET", object, headers))
			sock.close();
	}
	size_t	readbuff(void *buffer, size_t size) { return SSL_input::readbuff(con, sock, buffer, size); }
	SOCKET	_clone() const	{ return sock; }
};

struct SSL_ostream2 : public writer_mixin<SSL_ostream2>, SSL::SSL_output {
	SSL::Connection		con;
	Socket				sock;

	SSL_ostream2(SOCKET s) : sock(s) {
		if (!con.ServerConnect(sock))
			sock.close();
	}
	SSL_ostream2(const HTTP &http, const char *object, const char *headers = 0) : sock(http.Connect()) {
		if (!con.ServerConnect(sock) || !http.Request(SSL::SSL_ostream(con, sock), "PUT", object, headers))
			sock.close();
	}
	size_t	writebuff(const void *buffer, size_t size) { return SSL_output::writebuff(con, sock, buffer, size); }
	SOCKET	_clone() const	{ return sock; }
};

istream_ptr HTTP::GetSecure(const char *object, const char *headers) {
	return istream_ptr(new SSL_istream2(*this, object, headers));
}
ostream_ptr HTTP::PutSecure(const char *object, const char *headers) {
	return ostream_ptr(new SSL_ostream2(*this, object, headers));
}
istream_ptr HTTP::Get(const char *object, const char *headers) {
	socket_init();
	return is_secure() ? GetSecure(object, headers) : istream_ptr(new socket_input(Request("GET", object, headers)));
}
ostream_ptr HTTP::Put(const char *object, const char *headers) {
	return is_secure() ? PutSecure(object, headers) : ostream_ptr(new socket_output(Request("PUT", object, headers)));
}

#else

istream_ptr HTTP::Get(const char *object, const char *headers) {
	socket_init();
	return new socket_input(Request("GET", object, headers));
}
ostream_ptr HTTP::Put(const char *object, const char *headers) {
	socket_init();
	return new socket_output(Request("PUT", object, headers));
}

#endif


//-----------------------------------------------------------------------------
//	HTTPinput
//-----------------------------------------------------------------------------

void HTTPistream::start() {
//	p = 0;
	for (;;) {
		if (!in.more())
			return;

		headers.clear();
		http_code =  http_version = 0;

		for (;;) {
			auto		b		= str8(in.buffered());
			const char	*cr;
			while (!(cr = b.find('\r'))) {
				if (!in.more())
					return;
				b	= str8(in.buffered());
			}
			in.seek_cur(cr - b.begin() + 2);

			if (!http_version) {
				if (cr == b)
					continue;
				if (b.begins("HTTP/")) {
					from_string(get_http_version(b.begin() + 5, http_version), http_code);
					continue;
				}
				if (const char *h = b.find("HTTP/")) {
					get_http_version(h + 5, http_version);
					while (h > b && is_whitespace(*--h));
					request = b.slice_to(h);
					continue;
				}
				return;
			}

			if (cr == b.begin())
				break;

			if (const char *colon = b.slice_to(cr).find(':')) {
				auto &h	= headers.push_back();
				h.key	= str(b.begin(), colon);
				while (*++colon == ' ');
				h.value	= str(colon, cr);
			}
		}

		if (auto x = headers.get("content-length"))
			from_string(x, in.end);

		if (in.chunked = headers.check("transfer-encoding", "chunked"))
			in.end = 0;

		if (auto location = headers.get("location")) {
			HTTP	http("orbiscrude", location);
			in.reset(http.Get(http.PathParams()));

		} else {
			break;
		}
	}

	in.current	= 0;
	istream_ptr::operator=(reader_intf(in));
//	p			= &in;

	if (auto encoding = istr(headers.get("content-encoding"))) {
		if (encoding != "identity") {
			if (encoding == "gzip" || encoding == "x-gzip") {
				(void)in.get<GZheader>();
				istream_ptr::operator=(new GZistream(in));
			} else if (encoding == "deflate") {
				istream_ptr::operator=(new deflate_reader(in));
			} else {
				istream_ptr::clear();
			}
	//		if (encoding == "compress")
	//		if (encoding == "br")
		}
	}
}

size_t HTTPistream::input::readbuff(void *buffer, size_t size) {
	uint8	*p = (uint8*)buffer, *e = p + size;
	while (p < e) {
		if (uint32 size2 = min(e - p, uint32(end - current))) {
			if (uint32 size3 = buff_end - buff_off) {
				size2		= min(size2, size3);
				memcpy(p, buff + buff_off, size2);
				buff_off	+= size2;

			} else if (size2 < sizeof(buff)) {
				buff_end	= uint16(istream_ptr::readbuff(buff, min(size_t(end - current), sizeof(buff))));
				buff_off	= size2 = min(size2, buff_end);
				if (buff_end == 0)
					break;
				memcpy(p, buff, size2);

			} else {
				if ((size2 = uint32(istream_ptr::readbuff(p, size2))) == 0)
					break;
			}
			current += size2;
			p		+= size2;

		} else if (chunked) {
			if (buff_off == buff_end) {
				buff_off	= 0;
				buff_end	= uint16(istream_ptr::readbuff(buff, sizeof(buff)));
			}
			if (end)
				buff_off += 2;	// skip blank line

			char *p		= buff + buff_off;
			char *cr;
			while (!(cr	= str(p, buff_end - buff_off).find('\r'))) {
				if (!more())
					break;
			}
			if (!cr)
				break;
			buff_off	= cr + 2 - buff;

			uint32	chunk;
			if (!sscanf(p, "%x", &chunk))
				break;
			if (chunk == 0) {
				while (cr = str(buff + buff_off, buff_end - buff_off).find('\r'))	// skip trailers
					buff_off = cr + 2 - buff;
				break;
			}
			end += chunk;
		} else {
			break;
		}
	}
	return p - (uint8*)buffer;
}

void HTTPistream::input::seek(streamptr offset) {
	buff_off	+= offset - current;
	if (buff_off < 0 || buff_off > buff_end) {
		istream_ptr::seek_cur(buff_off - buff_end);
		buff_off = buff_end = 0;
	}
	current		= offset;
}

//-----------------------------------------------------------------------------
//	HTTPoutput
//-----------------------------------------------------------------------------

void HTTPostream::output::flush() {
	if (buff_off) {
		if (chunked) {
			buffer_accum<16>	ba;
			ba << hex(buff_off) << "\r\n";
			ostream_ptr::writebuff(ba.begin(), ba.length());
			ostream_ptr::writebuff(buff, buff_off);
			ostream_ptr::writebuff("\r\n", 2);
		} else {
			ostream_ptr::writebuff(buff, buff_off);
		}
	}
	buff_off = 0;
}

size_t HTTPostream::output::writebuff(const void *buffer, size_t size) {
	const uint8	*p = (const uint8*)buffer, *e = p + size;
	while (p < e) {
		if (buff_off == sizeof(buff))
			flush();

		uint32 size2 = min(e - p, sizeof(buff) - buff_off);
		memcpy(buff + buff_off, p, size2);
		buff_off	+= size2;
		current		+= size2;
		p			+= size2;
	}
	return p - (uint8*)buffer;
}

void HTTPostream::output::finish() {
	flush();
	if (chunked)
		ostream_ptr::writebuff("0\r\n\r\n", 5);
}

}	// namespace iso
