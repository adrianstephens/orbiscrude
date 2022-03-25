#ifndef FTP_H
#define FTP_H

#include "ip.h"

namespace iso {

//-----------------------------------------------------------------------------
//	FTP
//-----------------------------------------------------------------------------

class FTP {
	const char	*host;
	uint16		port;
	char		buffer[256];
	string_scan	ss;

	bool	ReadMore(SOCKET sock);

public:
	enum {DEFAULT_PORT = 21};
	enum CODE {
		NO_CONNECTION				= 0,
		//100 Series The requested action is being initiated, expect another reply before proceeding with a new command.
		Restart						= 110,		//Restart marker replay . In this case, the text is exact and not left to the particular implementation; it must read: MARK yyyy 	= mmmm where yyyy is User-process data stream marker, and mmmm server's equivalent marker (note the spaces between markers and "	=").
		Service						= 120,		//Service ready in nnn minutes.
		AlreadyOpen					= 125,		//Data connection already open; transfer starting.
		FileOK						= 150,		//File status okay; about to open data connection.
		//200 Series The requested action has been successfully completed.
		OK							= 200,
		Superfluous					= 202,		//Command not implemented, superfluous at this site.
		SystemStatus				= 211,		//System status, or system help reply.
		DirectoryStatus				= 212,		//Directory status.
		FileStatus					= 213,		//File status.
		HelpMessage					= 214,		//Help message.On how to use the server or the meaning of a particular non-standard command. This reply is useful only to the human user.
		//NAME						= 215,		//NAME system type. Where NAME is an official system name from the registry kept by IANA.
		ServiceReady				= 220,		//Service ready for new user.
		ServiceClosing				= 221,		//Service closing control connection.
		DataOpen					= 225,		//Data connection open; no transfer in progress.
		DataClosing					= 226,		//Closing data connection. Requested file action successful (for example, file transfer or file abort).
		Passive						= 227,		//Entering Passive Mode (h1,h2,h3,h4,p1,p2).
		LongPassive					= 228,		//Entering Long Passive Mode (long address, port).
		ExtendePassive				= 229,		//Entering Extended Passive Mode (|||port|).
		LoggedIn					= 230,		//User logged in, proceed. Logged out if appropriate.
		LoggedOut					= 231,		//User logged out; service terminated.
		LogoutReceived				= 232,		//Logout command noted, will complete when transfer done.
		ActionCompleted				= 250,		//Requested file action okay, completed.
		PATHNAMEcreated				= 257,		//"PATHNAME" created.
		//300 Series The command has been accepted, but the requested action is on hold, pending receipt of further information.
		NeedPassword				= 331,		//User name okay, need password.
		NeedAccount					= 332,		//Need account for login.
		ActionPending				= 350,		//Requested file action pending further information
		//400 Series The command was not accepted and the requested action did not take place, but the error condition is temporary and the action may be requested again.
		ServiceNotAvail				= 421,		//Service not available, closing control connection. This may be a reply to any command if the service knows it must shut down.
		CantOpenData				= 425,		//Can't open data connection.
		ConnectionClosed			= 426,		//Connection closed; transfer aborted.
		InvalidAccount				= 430,		//Invalid username or password
		HostUnavailable				= 434,		//Requested host unavailable.
		ActionRejected				= 450,		//Requested file action not taken.
		ActionAborted				= 451,		//Requested action aborted. Local error in processing.
		InsufficientStorage			= 452,		//Requested action not taken. Insufficient storage space in system.File unavailable (e.g., file busy).
		//500 Series Syntax error, command unrecognized and the requested action did not take place. This may include errors such as command line too long.
		SyntaxError					= 501,		//Syntax error in parameters or arguments.
		NotImplemented				= 502,		//Command not implemented.
		BadSequence					= 503,		//Bad sequence of commands.
		NotImplemented2				= 504,		//Command not implemented for that parameter.
		NotLoggedIn					= 530,		//Not logged in.
		NeedAccountToStore			= 532,		//Need account for storing files.
		FileUnavailable				= 550,		//Requested action not taken. File unavailable (e.g., file not found, no access).
		UnknownPageType				= 551,		//Requested action aborted. Page type unknown.
		StorageExceeded 			= 552,		//Requested file action aborted. Exceeded storage allocation (for current directory or dataset).
		BadFilename					= 553,		//Requested action not taken. File name not allowed.
		//600 Series Replies regarding confidentiality and integrity
		IntegReply					= 631,		//Integrity protected reply.
		ConfIntegReply				= 632,		//Confidentiality and integrity protected reply.
		ConfReply					= 633,		//Confidentiality protected reply.
		//10000 Series Common Winsock Error Codes
		ConnectionReset				= 10054,	//Connection reset by peer. The connection was forcibly closed by the remote host.
		CannotConnect				= 10060,	//Cannot connect to remote server.
		ActivelyRefused				= 10061,	//Cannot connect to remote server. The connection is actively refused by the server.
		DirectoryNotEmpty			= 10066,	//Directory not empty.
		TooManyUsers				= 10068,	//Too many users, server is full.
	};

	enum COMMANDS {
		ABOR		='ABOR',	//Abort an active file transfer.
		ACCT		='ACCT',	//Account information.
		ADAT		='ADAT',	//RFC 2228 Authentication/Security Data
		ALLO		='ALLO',	//Allocate sufficient disk space to receive a file.
		APPE		='APPE',	//Append.
		AUTH		='AUTH',	//RFC 2228 Authentication/Security Mechanism
		CCC			='CCC',		//RFC 2228 Clear Command Channel
		CDUP		='CDUP',	//Change to Parent Directory.
		CONF		='CONF',	//RFC 2228 Confidentiality Protection Command
		CWD			='CWD',		//Change working directory.
		DELE		='DELE',	//Delete file.
		ENC			='ENC',		//RFC 2228 Privacy Protected Channel
		EPRT		='EPRT',	//RFC 2428 Specifies an extended address and port to which the server should connect.
		EPSV		='EPSV',	//RFC 2428 Enter extended passive mode.
		FEAT		='FEAT',	//RFC 2389 Get the feature list implemented by the server.
		HELP		='HELP',	//Returns usage documentation on a command if specified, else a general help document is returned.
		LANG		='LANG',	//RFC 2640 Language Negotiation
		LIST		='LIST',	//Returns information of a file or directory if specified, else information of the current working directory is returned.
		LPRT		='LPRT',	//RFC 1639 Specifies a long address and port to which the server should connect.
		LPSV		='LPSV',	//RFC 1639 Enter long passive mode.
		MDTM		='MDTM',	//RFC 3659 Return the last-modified time of a specified file.
		MIC			='MIC',		//RFC 2228 Integrity Protected Command
		MKD			='MKD',		//Make directory.
		MLSD		='MLSD',	//RFC 3659 Lists the contents of a directory if a directory is named.
		MLST		='MLST',	//RFC 3659 Provides data about exactly the object named on its command line, and no others.
		MODE		='MODE',	//Sets the transfer mode (Stream, Block, or Compressed).
		NLST		='NLST',	//Returns a list of file names in a specified directory.
		NOOP		='NOOP',	//No operation (dummy packet; used mostly on keepalives).
		OPTS		='OPTS',	//RFC 2389 Select options for a feature.
		PASS		='PASS',	//Authentication password.
		PASV		='PASV',	//Enter passive mode.
		PBSZ		='PBSZ',	//RFC 2228 Protection Buffer Size
		PORT		='PORT',	//Specifies an address and port to which the server should connect.
		PROT		='PROT',	//RFC 2228 Data Channel Protection Level.
		PWD			='PWD',		//Print working directory. Returns the current directory of the host.
		QUIT		='QUIT',	//Disconnect.
		REIN		='REIN',	//Re initializes the connection.
		REST		='REST',	//Restart transfer from the specified point.
		RETR		='RETR',	//Transfer a copy of the file
		RMD			='RMD',		//Remove a directory.
		RNFR		='RNFR',	//Rename from.
		RNTO		='RNTO',	//Rename to.
		SITE		='SITE',	//Sends site specific commands to remote server.
		SIZE		='SIZE',	//RFC 3659 Return the size of a file.
		SMNT		='SMNT',	//Mount file structure.
		STAT		='STAT',	//Returns the current status.
		STOR		='STOR',	//Accept the data and to store the data as a file at the server site
		STOU		='STOU',	//Store file uniquely.
		STRU		='STRU',	//Set file transfer structure.
		SYST		='SYST',	//Return system type.
		TYPE		='TYPE',	//Sets the transfer mode (ASCII/Binary).
		USER		='USER',	//Authentication username.
		XCUP		='XCUP',	//&1000775 RFC 775 Change to the parent of the current working directory
		XMKD		='XMKD',	//&1000775 RFC 775 Make a directory
		XPWD		='XPWD',	//&1000775 RFC 775 Print the current working directory
		XRCP		='XRCP',	//&1000743 RFC 743
		XRMD		='XRMD',	//&1000775 RFC 775 Remove the directory
		XRSQ		='XRSQ',	//&1000743 RFC 743
		XSEM		='XSEM',	//&1000737 RFC 737 Send, mail if cannot
		XSEN		='XSEN',	//&1000737 RFC 737 Send to terminal
	};

	static bool			Init()							{ return socket_init(); }
	static void			DeInit()						{ socket_term();		}
	static void			SetError()						{ socket_error("FTP");	}

	FTP(const char *_host, uint16 _port = DEFAULT_PORT) : host(_host), port(_port), ss(buffer, buffer)	{}
	SOCKET	Connect()	const { return socket_address::TCP().connect(host, port); }
	SOCKET	Command(SOCKET sock, COMMANDS command, const char *params = 0) const;

	CODE	ReceiveReply(SOCKET sock);
	void	DiscardReply(SOCKET sock);
	CODE	ReceiveDiscardReply(SOCKET sock);
	CODE	ReceiveReply(SOCKET sock, string_accum &s);

	CODE	Login(SOCKET sock, const char *user, const char *password);
	CODE	GetPassiveSocket(SOCKET sock, SOCKET &sock_in);
	CODE	ClosePassiveSocket(SOCKET sock, SOCKET sock_in);
	CODE	RequestFile(SOCKET sock, const char *filename, SOCKET &sock_in);
	CODE	RequestFileSeek(SOCKET sock, const char *filename, SOCKET &sock_in, streamptr offset);
};

} // namespace iso
#endif //FTP_H