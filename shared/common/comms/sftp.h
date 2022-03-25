#ifndef SFTP_H
#define SFTP_H

#include "ip.h"

namespace iso {

//-----------------------------------------------------------------------------
//	SFTP
//-----------------------------------------------------------------------------

class SSH {
	char	*v_c, *v_s;
	void	*exhash;

	Socket s;

	void	*ldisc;
	void	*logctx;

	uint8	session_key[32];
	int		v1_compressing;
	int		v1_remote_protoflags;
	int		v1_local_protoflags;
	int		agentfwd_enabled;
	int		X11_fwd_enabled;
	int		remote_bugs;
	const	ssh_cipher *cipher;
	void	*v1_cipher_ctx;
	void	*crcda_ctx;
	const	ssh2_cipher *cscipher, *sccipher;
	void	*cs_cipher_ctx, *sc_cipher_ctx;
	const	ssh_mac *csmac, *scmac;
	void	*cs_mac_ctx, *sc_mac_ctx;
	const	ssh_compress *cscomp, *sccomp;
	void	*cs_comp_ctx, *sc_comp_ctx;
	const	ssh_kex *kex;
	const	ssh_signkey *hostkey;
	char	*hostkey_str;			// string representation, for easy checking in rekeys
	uint8	v2_session_id[SSH2_KEX_MAX_HASH_LEN];
	int		v2_session_id_len;
	void	*kex_ctx;

	int		bare_connection;
	int		attempting_connshare;
	void	*connshare;

	int		send_ok;
	int		echoing, editing;

	void	*frontend;

	tree234 *channels;				// indexed by local id
	ssh_channel *mainchan;			// primary session channel
	int		ncmode;					// is primary channel direct-tcpip?
	int		exitcode;
	int		close_expected;
	int		clean_exit;

	tree234 *rportfwds, *portfwds;

	enum {
		SSH_STATE_PREPACKET,
		SSH_STATE_BEFORE_SIZE,
		SSH_STATE_INTERMED,
		SSH_STATE_SESSION,
		SSH_STATE_CLOSED
	} state;

	int		size_needed, eof_needed;
	int		sent_console_eof;
	int		got_pty;			// affects EOF behaviour on main channel

	Packet **queue;
	int		queuelen, queuesize;
	int		queueing;
	uint8	*deferred_send_data;
	int		deferred_len, deferred_size;

	bufchain banner;			// accumulates banners during do_ssh2_authconn

	Pkt_KCtx pkt_kctx;
	Pkt_ACtx pkt_actx;

	struct X11Display *x11disp;
	struct X11FakeAuth *x11auth;
	tree234 *x11authtree;

	int		version;
	int		conn_throttle_count;
	int		overall_bufsize;
	int		throttled_all;
	int		v1_stdout_throttling;
	uint32	v2_outgoing_sequence;

	int		ssh1_rdpkt_crstate;
	int		ssh2_rdpkt_crstate;
	int		ssh2_bare_rdpkt_crstate;
	int		ssh_gotdata_crstate;
	int		do_ssh1_connection_crstate;

	void	*do_ssh_init_state;
	void	*do_ssh1_login_state;
	void	*do_ssh2_transport_state;
	void	*do_ssh2_authconn_state;
	void	*do_ssh_connection_init_state;

	rdpkt1_state_tag		rdpkt1_state;
	rdpkt2_state_tag		rdpkt2_state;
	rdpkt2_bare_state_tag	rdpkt2_bare_state;

	// SSH-1 and SSH-2 use this for different things, but both use it
	int protocol_initial_phase_done;

	void	(*protocol)		(SSH *ssh, void *vin, int inlen, Packet *pkt);
	Packet*	(*s_rdpkt)		(SSH *ssh, uint8 **data, int *datalen);
	int		(*do_ssh_init)	(SSH *ssh, uint8 c);

	Conf	*conf;
	int		logomitdata;

	void	*agent_response;
	int		agent_response_len;
	int		user_response;

	int		frozen;
	bufchain queued_incoming_data;

	handler_fn_t packet_dispatch[256];

	queued_handler *qhead, *qtail;
	handler_fn_t q_saved_handler1, q_saved_handler2;

	Pinger pinger;

	uint32	incoming_data_size, outgoing_data_size, deferred_data_size;
	uint32	max_data_size;
	int		kex_in_progress;
	uint32	next_rekey, last_rekey;
	char	*deferred_rekey_reason;
};


public:
	enum {DEFAULT_PORT = 22};

	int Recv(char *buf, int len) {
		return ssh_scp_recv((unsigned char *) buf, len);
	}
	int Send(char *buf, int len) {
		back->send(backhandle, buf, len);
	}
};

class SFTP : public SSH {
	const char	*host;
	uint16		port;
	char		buffer[256];
	string_scan	ss;

	bool	ReadMore(SOCKET sock);

public:
	enum FXP {
		FXP_INIT			= 1,
		FXP_VERSION			= 2,
		FXP_OPEN			= 3,
		FXP_CLOSE			= 4,
		FXP_READ			= 5,
		FXP_WRITE			= 6,
		FXP_LSTAT			= 7,
		FXP_FSTAT			= 8,
		FXP_SETSTAT			= 9,
		FXP_FSETSTAT		= 10,
		FXP_OPENDIR			= 11,
		FXP_READDIR			= 12,
		FXP_REMOVE			= 13,
		FXP_MKDIR			= 14,
		FXP_RMDIR			= 15,
		FXP_REALPATH		= 16,
		FXP_STAT			= 17,
		FXP_RENAME			= 18,
		FXP_STATUS			= 101,
		FXP_HANDLE			= 102,
		FXP_DATA			= 103,
		FXP_NAME			= 104,
		FXP_ATTRS			= 105,
		FXP_EXTENDED		= 200,
		FXP_EXTENDED_REPLY	= 201,
	};
	enum FX {
		FX_OK				= 0,
		FX_EOF				= 1,
		FX_NO_SUCH_FILE		= 2,
		FX_PERMISSION_DENIED= 3,
		FX_FAILURE			= 4,
		FX_BAD_MESSAGE		= 5,
		FX_NO_CONNECTION	= 6,
		FX_CONNECTION_LOST	= 7,
		FX_OP_UNSUPPORTED	= 8,
	};
	enum FXF {
		FXF_READ			= 0x00000001,
		FXF_WRITE			= 0x00000002,
		FXF_APPEND			= 0x00000004,
		FXF_CREAT			= 0x00000008,
		FXF_TRUNC			= 0x00000010,
		FXF_EXCL			= 0x00000020,
	};

	struct attrs {
		enum ATTR {
			SIZE		= 1,
			UIDGID		= 2,
			PERMISSIONS	= 4,
			ACMODTIME	= 8,
			EXTENDED	= 0,
		};
		uint32	flags;
		uint64	size;
		uint32	uid;
		uint32	gid;
		uint32	permissions;
		uint32	atime;
		uint32	mtime;
	};

	struct request {
		uint32	id;
		int		registered;
		void	*userdata;
	};
	
	struct packet {
		void	data;
		uint32	length, maxlen;
		uint32	savedpos;
		FXP		type;

		void add_data(const void *_data, int len) {
			uint32	offset = length;
			length += len;
			if ((int)maxlen < length) {
				maxlen	= length + 256;
				data	= relloc(data, maxlen);
			}
			memcpy((uint8*)data + offset, _data, len);
		}
		void add_byte(uint8 byte) {
			add_data(&byte, 1);
		}
		void add_uint32(uint32 value) {
			uint32be	x = value;
			add_data(&x, 4);
		}

		bool get_byte(uint8 *ret) {
			if (length - savedpos < 1)
				return false;
			*ret = ((uint8*)data)[savedpos++];
			return true;
		}
		bool get_uint32(uint32 *ret) {
			if (length - savedpos < 4)
				return false;
			*ret = *(uint32be*)((uint8*)data + savedpos);
			savedpos += 4;
			return true;
		}
		bool get_string(char **p, int *length) {
			*p = NULL;
			if (length - savedpos < 4)
				return false;
			getuint32((uint32*)length);
			if ((int)(length - savedpos) < *length || *length < 0) {
				*length = 0;
				return false;
			}
			*p = (char*)data + savedpos;
			savedpos += *length;
			return true;
		}
		bool get_attrs(attrs *ret) {
			if (!getuint32(&ret->flags))
				return false;
			if (ret->flags & SSH_FILEXFER_ATTR_SIZE) {
				uint32 hi, lo;
				if (!get_uint32(&hi) || !get_uint32(&lo))
					return false;
				ret->size = (uint64(hi) << 32) | lo;
			}
			if (ret->flags & SSH_FILEXFER_ATTR_UIDGID) {
				if (!get_uint32(&ret->uid) || !get_uint32(&ret->gid))
					return false;
			}
			if (ret->flags & SSH_FILEXFER_ATTR_PERMISSIONS) {
				if (!get_uint32(&ret->permissions))
					return false;
			}
			if (ret->flags & SSH_FILEXFER_ATTR_ACMODTIME) {
				if (!get_uint32(&ret->atime) || !get_uint32(&ret->mtime))
					return false;
			}
			if (ret->flags & SSH_FILEXFER_ATTR_EXTENDED) {
				uint32 count;
				if (!get_uint32(pkt, &count))
					return false;
				while (count--) {
					char	*str;
					int		len;
					if (!get_string(pkt, &str, &len) || !get_string(pkt, &str, &len))
						return 0;
				}
			}
			return true;
		}

		packet(uint32 size) : savedpos(0), length(size), maxlen(size) {
			data = malloc(size);
		}
		packet(FXP _type) : type(_type), data(0), savedpos(-1), length(0), maxlen(0) {
			add_uint32(0); // length field will be filled in later
			add_byte((uint8)type);
		}
		~packet() {
			free(data);
		}
	};

	int Send(packet *pkt) {
		*(uint32be*)pkt->data = pkt->length - 4;
		int		ret = SSH::Send(pkt->data, pkt->length);
		delete pkt;
		return ret;
	}

	packet	*Recv() {
		uint32be	size;
		if (!sftp_recvdata(size, 4))
			return NULL;

		packet	p = new packet(uint32(size));

		if (!SSH::Recv(p->data, p->length)) {
			delete p;
			return 0;
		}

		uint8		uc;
		if (!get_byte(p, &uc)) {
			delete p;
			return 0;
		} else {
			p->type = (FXP)uc;
		}
		return p;
	}

//#define SFTP_PROTO_VERSION 3

	static bool			Init()							{ return socket_init(); }
	static void			DeInit()						{ socket_term();		}
	static void			SetError()						{ socket_error("FTP");	}

	SFTP(const char *_host, uint16 _port = DEFAULT_PORT) : host(_host), port(_port), ss(buffer, buffer)	{}
	SOCKET	Connect()	const { return socket_address(AF_INET, SOCK_STREAM, IPPROTO_TCP).connect(host, port); }
};

class SFTPinput : public istream {
	SOCKET				sock;
	streamptr			current, end;
	uint16				buff_off, buff_end;
	char				buff[1024];
public:
	SFTPinput(SOCKET _sock, bool _own = true);
	~SFTPinput()							{}
	bool		exists()				{ return sock != INVALID_SOCKET; }
	streamptr	length()				{ return end;		}
	streamptr	tell()					{ return current;	}
	int			readbuff(void *buffer, size_t size);
	void		seek(streamptr offset, int origin = SEEK_SET);
};

} // namespace iso
#endif //SFTP_H