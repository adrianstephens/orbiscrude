#include "ip.h"
#include "base/array.h"
#include "extra/date.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	IMAP
//-----------------------------------------------------------------------------

class IMAP {
	enum STATE {
		NOT_AUTHENTICATED,
		AUTHENTICATED,
		SELECTED,
		LOGOUT,
	};

	enum COMMAND {
		COMMAND_CAPABILITY,
		COMMAND_NOOP,
		COMMAND_LOGOUT,
		COMMAND_STARTTLS,
		COMMAND_AUTHENTICATE,
		COMMAND_LOGIN,
		COMMAND_SELECT,
		COMMAND_EXAMINE,
		COMMAND_CREATE,
		COMMAND_DELETE,
		COMMAND_RENAME,
		COMMAND_SUBSCRIBE,
		COMMAND_UNSUBSCRIBE,
		COMMAND_LIST,
		COMMAND_LSUB,
		COMMAND_STATUS,
		COMMAND_APPEND,
		COMMAND_CHECK,
		COMMAND_CLOSE,
		COMMAND_EXPUNGE,
		COMMAND_SEARCH,
		COMMAND_FETCH,
		COMMAND_STORE,
		COMMAND_COPY,
		COMMAND_UID,
		COMMAND_ATOM,	//X<atom>
	};

	enum RESPONSE {
		RESPONSE_OK,
		RESPONSE_NO,
		RESPONSE_BAD,
		RESPONSE_PREAUTH,
		RESPONSE_BYE,
		RESPONSE_CAPABILITY,
		RESPONSE_LIST,
		RESPONSE_LSUB,
		RESPONSE_STATUS,
		RESPONSE_SEARCH,
		RESPONSE_FLAGS,
		RESPONSE_EXISTS,
		RESPONSE_RECENT,
		RESPONSE_EXPUNGE,
		RESPONSE_FETCH,
	};


	const char	*host;
	uint16		port;
	uint16		seq;
	char		buffer[256];
	string_scan	ss;

	bool	ReadMore(SOCKET sock);

public:
	enum {DEFAULT_PORT = 143};

	struct Message {
		uint64	uid;
		uint32	seq;
		dynamic_array<string>	flags;
	};
};

/*
Sample IMAP4rev1 connection

 The following is a transcript of an IMAP4rev1 connection.  A long line in this sample is broken for editorial clarity.

S:   * OK IMAP4rev1 Service Ready
C:   a001 login mrc secret
S:   a001 OK LOGIN completed
C:   a002 select inbox
S:   * 18 EXISTS
S:   * FLAGS (\Answered \Flagged \Deleted \Seen \Draft)
S:   * 2 RECENT
S:   * OK [UNSEEN 17] Message 17 is the first unseen message
S:   * OK [UIDVALIDITY 3857529045] UIDs valid
S:   a002 OK [READ-WRITE] SELECT completed
C:   a003 fetch 12 full
S:   * 12 FETCH (FLAGS (\Seen) INTERNALDATE "17-Jul-1996 02:44:25 -0700"
 RFC822.SIZE 4286 ENVELOPE ("Wed, 17 Jul 1996 02:23:25 -0700 (PDT)"
 "IMAP4rev1 WG mtg summary and minutes"
 (("Terry Gray" NIL "gray" "cac.washington.edu"))
 (("Terry Gray" NIL "gray" "cac.washington.edu"))
 (("Terry Gray" NIL "gray" "cac.washington.edu"))
 ((NIL NIL "imap" "cac.washington.edu"))
 ((NIL NIL "minutes" "CNRI.Reston.VA.US")
 ("John Klensin" NIL "KLENSIN" "MIT.EDU")) NIL NIL
 "<B27397-0100000@cac.washington.edu>")
 BODY ("TEXT" "PLAIN" ("CHARSET" "US-ASCII") NIL NIL "7BIT" 3028
 92))
S:    a003 OK FETCH completed
C:    a004 fetch 12 body[header]
S:    * 12 FETCH (BODY[HEADER] {342}
S:    Date: Wed, 17 Jul 1996 02:23:25 -0700 (PDT)
S:    From: Terry Gray <gray@cac.washington.edu>
S:    Subject: IMAP4rev1 WG mtg summary and minutes
S:    To: imap@cac.washington.edu
S:    cc: minutes@CNRI.Reston.VA.US, John Klensin <KLENSIN@MIT.EDU>
S:    Message-Id: <B27397-0100000@cac.washington.edu>
S:    MIME-Version: 1.0
S:    Content-Type: TEXT/PLAIN; CHARSET=US-ASCII
S:
S:    )
S:    a004 OK FETCH completed
C:    a005 store 12 +flags \deleted
S:    * 12 FETCH (FLAGS (\Seen \Deleted))
S:    a005 OK +FLAGS completed
C:    a006 logout
S:    * BYE IMAP4rev1 server terminating connection
S:    a006 OK LOGOUT completed
*/

/*
syntax:
address         = "(" addr-name SP addr-adl SP addr-mailbox SP addr-host ")"
addr-adl        = nstring
addr-host       = nstring
addr-mailbox    = nstring
addr-name       = nstring
append          = "APPEND" SP mailbox [SP flag-list] [SP date-time] SP literal
astring         = 1*ASTRING-CHAR / string
ASTRING-CHAR   = ATOM-CHAR / resp-specials
atom            = 1*ATOM-CHAR
ATOM-CHAR       = <any CHAR except atom-specials>
atom-specials   = "(" / ")" / "{" / SP / CTL / list-wildcards / quoted-specials / resp-specials
authenticate    = "AUTHENTICATE" SP auth-type *(CRLF base64)
auth-type       = atom
base64          = *(4base64-char) [base64-terminal]
base64-char     = ALPHA / DIGIT / "+" / "/"               // Case-sensitive
base64-terminal = (2base64-char "==") / (3base64-char "=")
body            = "(" (body-type-1part / body-type-mpart) ")"
body-extension  = nstring / number / "(" body-extension *(SP body-extension) ")"
body-ext-1part  = body-fld-md5 [SP body-fld-dsp [SP body-fld-lang [SP body-fld-loc *(SP body-extension)]]]
body-ext-mpart  = body-fld-param [SP body-fld-dsp [SP body-fld-lang [SP body-fld-loc *(SP body-extension)]]]
body-fields     = body-fld-param SP body-fld-id SP body-fld-desc SP body-fld-enc SP body-fld-octets
body-fld-desc   = nstring
body-fld-dsp    = "(" string SP body-fld-param ")" / nil
body-fld-enc    = (DQUOTE ("7BIT" / "8BIT" / "BINARY" / "BASE64"/ "QUOTED-PRINTABLE") DQUOTE) / string
body-fld-id     = nstring
body-fld-lang   = nstring / "(" string *(SP string) ")"
body-fld-loc    = nstring
body-fld-lines  = number
body-fld-md5    = nstring
body-fld-octets = number
body-fld-param  = "(" string SP string *(SP string SP string) ")" / nil
body-type-1part = (body-type-basic / body-type-msg / body-type-text) [SP body-ext-1part]
body-type-basic = media-basic SP body-fields
body-type-mpart = 1*body SP media-subtype [SP body-ext-mpart]
body-type-msg   = media-message SP body-fields SP envelope SP body SP body-fld-lines
body-type-text  = media-text SP body-fields SP body-fld-lines
capability      = ("AUTH=" auth-type) / atom
capability-data = "CAPABILITY" *(SP capability) SP "IMAP4rev1" *(SP capability)
CHAR8           = %x01-ff
command         = tag SP (command-any / command-auth / command-nonauth / command-select) CRLF
command-any     = "CAPABILITY" / "LOGOUT" / "NOOP" / x-command
command-auth    = append / create / delete / examine / list / lsub / rename / select / status / subscribe / unsubscribe
command-nonauth = login / authenticate / "STARTTLS"
command-select  = "CHECK" / "CLOSE" / "EXPUNGE" / copy / fetch / store / uid / search
continue-req    = "+" SP (resp-text / base64) CRLF
copy            = "COPY" SP sequence-set SP mailbox
create          = "CREATE" SP mailbox
date            = date-text / DQUOTE date-text DQUOTE
date-day        = 1*2DIGIT
date-day-fixed  = (SP DIGIT) / 2DIGIT
date-month      = "Jan" / "Feb" / "Mar" / "Apr" / "May" / "Jun" / "Jul" / "Aug" / "Sep" / "Oct" / "Nov" / "Dec"
date-text       = date-day "-" date-month "-" date-year
date-year       = 4DIGIT
date-time       = DQUOTE date-day-fixed "-" date-month "-" date-year SP time SP zone DQUOTE
delete          = "DELETE" SP mailbox
digit-nz        = %x31-39
envelope        = "(" env-date SP env-subject SP env-from SP env-sender SP env-reply-to SP env-to SP env-cc SP env-bcc SP env-in-reply-to SP env-message-id ")"
env-bcc         = "(" 1*address ")" / nil
env-cc          = "(" 1*address ")" / nil
env-date        = nstring
env-from        = "(" 1*address ")" / nil
env-in-reply-to = nstring
env-message-id  = nstring
env-reply-to    = "(" 1*address ")" / nil
env-sender      = "(" 1*address ")" / nil
env-subject     = nstring
env-to          = "(" 1*address ")" / nil
examine         = "EXAMINE" SP mailbox
fetch           = "FETCH" SP sequence-set SP ("ALL" / "FULL" / "FAST" / fetch-att / "(" fetch-att *(SP fetch-att) ")")
fetch-att       = "ENVELOPE" / "FLAGS" / "INTERNALDATE" / "RFC822" [".HEADER" / ".SIZE" / ".TEXT"] / "BODY" ["STRUCTURE"] / "UID" / "BODY" section ["<" number "." nz-number ">"] / "BODY.PEEK" section ["<" number "." nz-number ">"]
flag            = "\Answered" / "\Flagged" / "\Deleted" / "\Seen" / "\Draft" / flag-keyword / flag-extension 
flag-extension  = "\" atom
flag-fetch      = flag / "\Recent"
flag-keyword    = atom
flag-list       = "(" [flag *(SP flag)] ")"
flag-perm       = flag / "\*"
greeting        = "*" SP (resp-cond-auth / resp-cond-bye) CRLF
header-fld-name = astring
header-list     = "(" header-fld-name *(SP header-fld-name) ")"
list            = "LIST" SP mailbox SP list-mailbox
list-mailbox    = 1*list-char / string
list-char       = ATOM-CHAR / list-wildcards / resp-specials
list-wildcards  = "%" / "*"
literal         = "{" number "}" CRLF *CHAR8
login           = "LOGIN" SP userid SP password
lsub            = "LSUB" SP mailbox SP list-mailbox
mailbox         = "INBOX" / astring
mailbox-data    =  "FLAGS" SP flag-list / "LIST" SP mailbox-list / "LSUB" SP mailbox-list / "SEARCH" *(SP nz-number) / "STATUS" SP mailbox SP "(" [status-att-list] ")" / number SP "EXISTS" / number SP "RECENT"
mailbox-list    = "(" [mbx-list-flags] ")" SP (DQUOTE QUOTED-CHAR DQUOTE / nil) SP mailbox
mbx-list-flags  = *(mbx-list-oflag SP) mbx-list-sflag *(SP mbx-list-oflag) / mbx-list-oflag *(SP mbx-list-oflag)
mbx-list-oflag  = "\Noinferiors" / flag-extension
mbx-list-sflag  = "\Noselect" / "\Marked" / "\Unmarked"
media-basic     = ((DQUOTE ("APPLICATION" / "AUDIO" / "IMAGE" / "MESSAGE" / "VIDEO") DQUOTE) / string) SP media-subtype
media-message   = DQUOTE "MESSAGE" DQUOTE SP DQUOTE "RFC822" DQUOTE
media-subtype   = string
media-text      = DQUOTE "TEXT" DQUOTE SP media-subtype
message-data    = nz-number SP ("EXPUNGE" / ("FETCH" SP msg-att))
msg-att         = "(" (msg-att-dynamic / msg-att-static) *(SP (msg-att-dynamic / msg-att-static)) ")"
msg-att-dynamic = "FLAGS" SP "(" [flag-fetch *(SP flag-fetch)] ")"
msg-att-static  = "ENVELOPE" SP envelope / "INTERNALDATE" SP date-time / "RFC822" [".HEADER" / ".TEXT"] SP nstring / "RFC822.SIZE" SP number / "BODY" ["STRUCTURE"] SP body / "BODY" section ["<" number ">"] SP nstring / "UID" SP uniqueid
nil             = "NIL"
nstring         = string / nil
number          = 1*DIGIT
nz-number       = digit-nz *DIGIT
password        = astring
quoted          = DQUOTE *QUOTED-CHAR DQUOTE
QUOTED-CHAR     = <any TEXT-CHAR except quoted-specials> / "\" quoted-specials
quoted-specials = DQUOTE / "\"
rename          = "RENAME" SP mailbox SP mailbox
response        = *(continue-req / response-data) response-done
response-data   = "*" SP (resp-cond-state / resp-cond-bye / mailbox-data / message-data / capability-data) CRLF
response-done   = response-tagged / response-fatal
response-fatal  = "*" SP resp-cond-bye CRLF
response-tagged = tag SP resp-cond-state CRLF
resp-cond-auth  = ("OK" / "PREAUTH") SP resp-text
resp-cond-bye   = "BYE" SP resp-text
resp-cond-state = ("OK" / "NO" / "BAD") SP resp-text
resp-specials   = "]"
resp-text       = ["[" resp-text-code "]" SP] text
resp-text-code  = "ALERT" / "BADCHARSET" [SP "(" astring *(SP astring) ")" ] / capability-data / "PARSE" / "PERMANENTFLAGS" SP "(" [flag-perm *(SP flag-perm)] ")" / "READ-ONLY" / "READ-WRITE" / "TRYCREATE" / "UIDNEXT" SP nz-number / "UIDVALIDITY" SP nz-number / "UNSEEN" SP nz-number / atom [SP 1*<any TEXT-CHAR except "]">]
search          = "SEARCH" [SP "CHARSET" SP astring] 1*(SP search-key)
search-key      = "ALL" / "ANSWERED" / "BCC" SP astring / "BEFORE" SP date / "BODY" SP astring / "CC" SP astring / "DELETED" / "FLAGGED" / "FROM" SP astring / "KEYWORD" SP flag-keyword / "NEW" / "OLD" / "ON" SP date / "RECENT" / "SEEN" / "SINCE" SP date / "SUBJECT" SP astring / "TEXT" SP astring / "TO" SP astring / "UNANSWERED" / "UNDELETED" / "UNFLAGGED" / "UNKEYWORD" SP flag-keyword / "UNSEEN" /  "DRAFT" / "HEADER" SP header-fld-name SP astring / "LARGER" SP number / "NOT" SP search-key / "OR" SP search-key SP search-key / "SENTBEFORE" SP date / "SENTON" SP date / "SENTSINCE" SP date / "SMALLER" SP number / "UID" SP sequence-set / "UNDRAFT" / sequence-set / "(" search-key *(SP search-key) ")"
section         = "[" [section-spec] "]"
section-msgtext = "HEADER" / "HEADER.FIELDS" [".NOT"] SP header-list / "TEXT"
section-part    = nz-number *("." nz-number)
section-spec    = section-msgtext / (section-part ["." section-text])
section-text    = section-msgtext / "MIME"
select          = "SELECT" SP mailbox
seq-number      = nz-number / "*" seq-range       = seq-number ":" seq-number
sequence-set    = (seq-number / seq-range) *("," sequence-set)
status          = "STATUS" SP mailbox SP "(" status-att *(SP status-att) ")"
status-att      = "MESSAGES" / "RECENT" / "UIDNEXT" / "UIDVALIDITY" / "UNSEEN"
status-att-list =  status-att SP number *(SP status-att SP number)
store           = "STORE" SP sequence-set SP store-att-flags
store-att-flags = (["+" / "-"] "FLAGS" [".SILENT"]) SP (flag-list / (flag *(SP flag)))
string          = quoted / literal
subscribe       = "SUBSCRIBE" SP mailbox
tag             = 1*<any ASTRING-CHAR except "+">
text            = 1*TEXT-CHAR
TEXT-CHAR       = <any CHAR except CR and LF>
time            = 2DIGIT ":" 2DIGIT ":" 2DIGIT
uid             = "UID" SP (copy / fetch / search / store)
uniqueid        = nz-number
unsubscribe     = "UNSUBSCRIBE" SP mailbox
userid          = astring
x-command       = "X" atom <experimental command arguments>
zone            = ("+" / "-") 4DIGIT
*/