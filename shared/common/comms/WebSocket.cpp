#include "WebSocket.h"
#include "codec/base64.h"
#include "codec/rc.h"
#include "codec/base64.h"
#include "hashes/SHA.h"
#include "utilities.h"

#ifdef USE_SSL
#include "ssl.h"
#endif

using namespace iso;

rng<random_stir<RC4>>	g_rc4;

bool WebSocket::SendPacket(ostream_ref file, OpCode opcode, const void* data, uint32 len, bool mask) {
	uint32		packet_len = 2 + int(mask) * 4 + (len < 126 ? 0 : len < 65536 ? 2 : 8) + len;
	temp_block	packet(packet_len);
	byte_writer	b(packet);

	b.putc(opcode);
	uint8	len_mask	= int(mask) * 0x80;
	if (len < 126) {
		b.putc(len_mask | len);
	} else if (len < 65536) {
		b.putc(len_mask | 126);
		b.write(uint16be((len)));
	} else {
		b.putc(len_mask | 127);
		b.write(uint64be((len)));
	}

	if (mask) {
		uint32 mask_val = g_rc4.get<uint32>();	//0xdeadbeef
		b.write(mask_val);
		
		for (uint32	*p = (uint32*)data, *e = p + len / 4; p != e; ++p)
			b.write(*p ^ mask_val);
		if (auto x = len & 3) {
			uint8	*p	= (uint8*)data + len - x;
			switch (x) {
				case 3:		b.putc(*p++ ^ mask_val); mask_val >>= 8;
				case 2:		b.putc(*p++ ^ mask_val); mask_val >>= 8;
				default:	b.putc(*p++ ^ mask_val); 
			}
		}
	} else {
		b.writebuff(data, len);
	}

	return file.writebuff(packet, packet_len) == packet_len;
}

bool WebSocket::SendClose(ostream_ref file, StatusCode code, const char* reason, bool mask) {
	uint32 len	= reason ? strlen(reason) : 0;
	char*  msg	= (char*)alloca(len + 2);
	msg[0]		= code >> 8;
	msg[1]		= code;
	memcpy(msg + 2, reason, len);
	return SendPacket(file, Close | FIN, msg, len + 2, mask);
}

WebSocket::Message WebSocket::InputPacket(istream_ref in, ostream_ref out, bool out_mask) {
	header	h;
	if (!in.read(h))
		return {};

	uint64	len		= h.len & 0x7f;
	if (len == 126)
		len = in.get<uint16be>();
	else if (len == 127)
		len = in.get<uint64be>();

	malloc_block	data(len);

	if (h.len & 0x80) {
		uint32	mask = in.get<uint32>();
		in.read(data);
		for (auto &i : make_range<uint32>(data))
			i ^= mask;
		if (auto x = len & 3) {
			uint8	*p = (uint8*)data + len - x;
			switch (x) {
				case 3:		p[2] ^= mask >> 16;
				case 2:		p[1] ^= mask >> 8;
				default:	p[0] ^= mask;
			}
		}

	} else {
		in.read(data);
	}

	return {h.opcode, move(data)};
}

WebSocket::Message WebSocket::ProcessPackets(istream_ref in, ostream_ref out, bool out_mask) {
	Message	message;

	while (auto packet = InputPacket(in, out, out_mask)) {
		//ISO_TRACEF("Websocket packet = 0x%x, len = %i\n", h.opcode, len);
		switch (packet.type) {
			// control
			case Close | FIN:
				// First two bytes of a close payload are a status_code (if payload is present)
				status_code	= packet.data.length() >= 2 ? (StatusCode)(uint16) * (uint16be*)packet.data : StatusNoStatusReceived;
				SendClose(out, packet.data.length() == 1 || (!between(status_code, StatusNormal, StatusInternalError) && !between(status_code, 3000, 4999)) ? StatusProtocolError : StatusNormal, "", out_mask);
				break;

			case Ping | FIN:
				// We have to reply with a pong with the same payload as the ping
				SendPacket(out, Pong | FIN, packet.data, out_mask);
				break;

			case Pong | FIN:
				break;

			//message
			case Cont | FIN:
				if (message.type != Cont) {
					message.data += packet.data;
					return message;
				}
				SendClose(out, StatusProtocolError, "", out_mask);
				break;

			case Text | FIN:
			case Binary | FIN:
				if (message.type == Cont) {
					packet.type = OpCode(packet.type & OpCodeMask);
					return packet;
				}
				SendClose(out, StatusProtocolError, "", out_mask);
				break;

			case Text:
			case Binary:
				if (message.type == Cont) {
					message.type = packet.type;
					message.data = move(packet.data);
				} else {
					SendClose(out, StatusProtocolError, "", out_mask);
				}
				break;

			case Cont:
				if (message.type != Cont) {
					message.data += packet.data;
				} else {
					SendClose(out, StatusProtocolError, "", out_mask);
				}
				break;

			default:
				SendClose(out, StatusProtocolError, "", out_mask);
				break;
		}
	}
	return {};
}

//-----------------------------------------------------------------------------
// WebSocketClient
//-----------------------------------------------------------------------------

WebSocketClient::WebSocketClient(HTTP::Context http_context, const char *url) {
	HTTP	http(http_context, url);
	uint8	nonce[16];
	g_rc4.fill(nonce);
	char	nonce_base64[32];
	size_t	len;
	transcode(base64_encoder(), nonce_base64, nonce, &len);
	nonce_base64[len] = 0;

	auto	sock = http.Connect();

#ifdef USE_SSL
	if (http.is_secure()) {
		io		= new SSL::SSL_iostream(con, Socket(sock));
	} else
#endif
	{
		io		= new Socket(sock);
	}

	// Send Upgrade request
	HTTP_Headers	headers(
		"Origin",				format_string("%s://%s:%i", http.scheme, http.host, http.port),
		"Upgrade",				"websocket",
		"Connection",			"Upgrade",
		"Sec-WebSocket-Key",	nonce_base64,
		"Sec-WebSocket-Version", "13"
	);
	http.Request(io, "GET", http.PathParams(), headers);

	HTTPistream	http_in(io);
	if (auto accept = http_in.headers.get("Sec-WebSocket-Accept")) {
		SHA1	sha1((const char*)nonce_base64);
		sha1.write("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
		auto	code	= sha1.digest();
		transcode(base64_encoder(), nonce_base64, const_memory_block(&code), &len);
		nonce_base64[len] = 0;
		ISO_ASSERT(nonce_base64 == str(accept));
	}
}

//-----------------------------------------------------------------------------
// WebSocketServer
//-----------------------------------------------------------------------------

WebSocketServer::WebSocketServer( HTTP::Context http_context, const HTTP_Headers& headers, iostream_ref io) : io(io) {
	version		= headers.get<int>("Sec-WebSocket-Version");
	auto	origin		= headers.get("origin");

	//auto	extensions	= headers.get("Sec-WebSocket-Extensions");// "permessage-deflate; client_max_window_bits"
	//auto	protocols	= headers.get("Sec-WebSocket-Protocol");

	uint8	nonce[16];
	auto	key = headers.get("Sec-WebSocket-Key");
	transcode(base64_decoder(), nonce, key);

	SHA1	sha1(key);
	sha1.write("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	auto	code	= sha1.digest();

	char	nonce_base64[32];
	size_t	len;
	transcode(base64_encoder(), nonce_base64, const_memory_block(&code), &len);
	nonce_base64[len] = 0;

	HTTP			http(http_context, origin);
	HTTP_Headers	headers2(
		"Sec-WebSocket-Accept", nonce_base64,
		"Upgrade", "websocket",
		"Connection", "Upgrade"
	);
	//headers2.add("Sec-WebSocket-Protocol", "chat");
	http.Response(io, HTTP::SwitchingProtocols, headers2);
}
