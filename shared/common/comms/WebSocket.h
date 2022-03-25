#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "HTTP.h"
#ifdef USE_SSL
#include "ssl.h"
#endif

namespace iso {

class WebSocket {
public:
	enum StatusCode {
		StatusNormal			= 1000,
		StatusGoingAway			= 1001,
		StatusProtocolError		= 1002,
		StatusUnhandledType		= 1003,
		StatusNoStatusReceived	= 1005,
		StatusAbnormal			= 1006,
		StatusInvalidUTF8		= 1007,
		StatusPolicyViolated	= 1008,
		StatusMessageTooBig		= 1009,
		StatusMissingExtension	= 1010,
		StatusInternalError		= 1011,
	};
	enum OpCode : uint8 {
		FIN			= 0x80,
		RSV1		= 0x40,
		RSV2		= 0x20,
		RSV3		= 0x10,

		OpCodeMask	= 0xf,
		Cont		= 0x0,
		Text		= 0x1,
		Binary		= 0x2,
		Close		= 0x8,
		Ping		= 0x9,
		Pong		= 0xA,
	};
	friend constexpr OpCode operator|(OpCode a, OpCode b) { return OpCode(int(a) | int(b)); }

	struct Message {
		OpCode			type;
		malloc_block	data;
		explicit constexpr operator bool() const { return type != Cont; }
		Message() : type(Cont) {}
		Message(OpCode type, malloc_block&& data) : type(type), data(move(data)) {}
	};

	StatusCode	status_code = StatusNoStatusReceived;	// Closing state

protected:
	struct header {
		OpCode	opcode;
		uint8	len;
	};

	static Message	InputPacket(istream_ref in, ostream_ref out, bool out_mask);
	static bool		SendPacket(ostream_ref file, OpCode opcode, const void* data, uint32 len, bool mask);
	static bool		SendClose(ostream_ref file, StatusCode code, const char* reason, bool mask);
	static bool		SendPacket(ostream_ref file, OpCode opcode, const const_memory_block& data, bool mask) {
		return SendPacket(file, opcode, data, data.size32(), mask);
	}

	Message ProcessPackets(istream_ref in, ostream_ref out, bool out_mask);
};

class WebSocketClient : WebSocket {
#ifdef USE_SSL
	SSL::Connection	con;
#endif
	iostream_ptr	io;

public:
	bool SendText(const char* text)							{ return SendPacket(io, Text | FIN, text, true); }
	bool SendBinary(const const_memory_block &data)			{ return SendPacket(io, Binary | FIN, data, true); }
	bool SendPing(const const_memory_block &data)			{ return SendPacket(io, Ping | FIN, data, true); }
	bool Close(StatusCode status_code = StatusNormal, const char* reason = "") { return SendClose(io, status_code, reason, true); }
	Message Process()	{ return ProcessPackets(io, io, true); }

	WebSocketClient(HTTP::Context http_context, const char *url);
	~WebSocketClient() { Close(); }
};

class WebSocketServer : WebSocket {
	uint32	version;

	iostream_ptr	io;

public:
	bool SendText(const char* text)							{ return SendPacket(io, Text | FIN, text, false); }
	bool SendBinary(const const_memory_block &data)			{ return SendPacket(io, Binary | FIN, data, false); }
	bool SendPing(const const_memory_block &data)			{ return SendPacket(io, Ping | FIN, data, false); }
	bool Close(StatusCode status_code = StatusNormal, const char* reason = "") { return SendClose(io, status_code, reason, false); }
	Message Process()	{ return ProcessPackets(io, io, false); }

	WebSocketServer(HTTP::Context http_context, const HTTP_Headers& headers, iostream_ref io);
};

} //namespace iso
#endif //WEBSOCKET_H
