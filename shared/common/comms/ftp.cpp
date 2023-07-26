#include "ftp.h"

namespace iso {

SOCKET FTP::Command(SOCKET sock, COMMANDS command, const char *params) const {
	buffer_accum<1024>	ba;
	*(uint32be*)ba.getp() = command;
	ba.move(3 + int(command > 0x1000000));
	if (params && params[0])
		ba << ' ' << params;
	ba << "\r\n";

	socklen_t	len = socklen_t(ba.length());
	if (send(sock, ba.term(), len, 0) != len) {
		SetError();
		socket_close(sock);
		sock = INVALID_SOCKET;
	}
	return sock;
}

bool FTP::ReadMore(SOCKET sock) {
	size_t	rem = ss.remaining();
	memcpy(buffer, ss.getp(), rem);
	size_t	read = socket_receive(sock, buffer + rem, sizeof(buffer) - rem);
	if (read == 0)
		return false;
	ss	= string_scan(buffer, buffer + rem + read);
	return true;
}

FTP::CODE FTP::ReceiveReply(SOCKET sock) {
	if (!ss.remaining() && !ReadMore(sock))
		return CODE(-1);

	uint32 code = ss.get<uint32>();
	if (ss.peekc() == '-') {
		do {
			if (ss.scan('\r'))
				ss.move(2);
			else if (!ReadMore(sock))
				return CODE(-1);
		} while (ss.get<uint32>() != code || ss.peekc() != ' ');
	}
	return CODE(code);
}

FTP::CODE FTP::ReceiveReply(SOCKET sock, string_accum &s) {
	if (!ss.remaining() && !ReadMore(sock))
		return CODE(-1);

	uint32 code = ss.get<uint32>();
	if (ss.peekc() == '-') {
		do {
			const char	*p = ss.getp();
			if (ss.scan('\r')) {
				ss.move(2);
				s.merge(p, ss.getp() - p);
			} else if (!ReadMore(sock)) {
				return CODE(-1);
			}
		} while (ss.get<uint32>() != code || ss.peekc() != ' ');
	}
	return CODE(code);
}

void FTP::DiscardReply(SOCKET sock) {
	while (!ss.scan('\r'))
		ReadMore(sock);
	ss.move(2);
}

FTP::CODE FTP::ReceiveDiscardReply(SOCKET sock) {
	CODE code = ReceiveReply(sock);
	if (code != CODE(-1))
		DiscardReply(sock);
	return code;
}

FTP::CODE FTP::Login(SOCKET sock, const char *user, const char *password) {
	for (;;) {
		switch (CODE code = ReceiveDiscardReply(sock)) {
			case ServiceReady:
				Command(sock, USER, user);
				break;
			case NeedPassword:
				Command(sock, PASS, password);
				break;
			default:
				return code;
		}
	}
}

FTP::CODE FTP::GetPassiveSocket(SOCKET sock, SOCKET &sock_in) {
	sock_in	= INVALID_SOCKET;

	Command(sock, PASV);
	CODE	code = ReceiveReply(sock);

	if (code == Passive) {
		string_scan	ss(str(buffer).find(' '), str(buffer).find('\r'));
		if (ss.scan('(')) {
			uint32	nums[6];
			int		i	= 0;
			while (i < 6 && ss.getc() != ')')
				ss >> nums[i++];
			IP4::addr ip;
			ip[0] = nums[0];
			ip[1] = nums[1];
			ip[2] = nums[2];
			ip[3] = nums[3];
			sock_in = IP4::socket_addr(ip, (nums[4] << 8) + nums[5]).connect_or_close(IP4::TCP());
		}
	}
	DiscardReply(sock);
	return code;
}

FTP::CODE FTP::ClosePassiveSocket(SOCKET sock, SOCKET sock_in) {
	socket_close(sock_in);
	return ReceiveDiscardReply(sock);
}

FTP::CODE FTP::RequestFile(SOCKET sock, const char *filename, SOCKET &sock_in) {
	sock_in	= INVALID_SOCKET;

	Command(sock, TYPE, "I");
	CODE	code = ReceiveDiscardReply(sock);

	if (code == OK) {
		code = GetPassiveSocket(sock, sock_in);
		if (code == Passive) {
			Command(sock, RETR, filename);
			code = ReceiveDiscardReply(sock);
		}
	}
	return code;
}

FTP::CODE FTP::RequestFileSeek(SOCKET sock, const char *filename, SOCKET &sock_in, streamptr offset) {
	sock_in	= INVALID_SOCKET;

	Command(sock, TYPE, "I");
	CODE	code = ReceiveDiscardReply(sock);

	if (code == OK) {
		code = GetPassiveSocket(sock, sock_in);
		if (code == Passive) {
			Command(sock, REST, to_string(offset));
			code = ReceiveDiscardReply(sock);
			Command(sock, RETR, filename);
			code = ReceiveDiscardReply(sock);
		}
	}
	return code;
}

}	// namespace iso
