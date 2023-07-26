#include "iso/iso_files.h"
#include "comms/http.h"
#include "device.h"
#include "main.h"

using namespace app;

#ifdef PLAT_PC

class GetURLDialog : public Dialog<GetURLDialog> {
	static fixed_string<1024>	url;
	fixed_string<8>				ext;
public:
	LRESULT	Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_INITDIALOG: {
				Item(IDC_URL).SetText(url);
				ComboControl	c = Item(IDC_FILETYPES);
				c.Add("<auto>");
				for (FileHandler::iterator i = FileHandler::begin(); i != FileHandler::end(); ++i) {
					if (i->GetExt())
						c.Add(i->GetExt());
				}
				c.SetText("<auto>");
				break;
			}
			case WM_COMMAND: {
				switch (LOWORD(wParam)) {
					case IDOK: {
						ext = Item(IDC_FILETYPES).GetText();
						url = Item(IDC_URL).GetText();
						return EndDialog(1);
					}
					case IDCANCEL:
						return EndDialog(0);
				}
				break;
			}
		}
		return FALSE;
	}
	GetURLDialog(HWND hWndParent) {
		Modal(hWndParent, IDD_OPENURL);
	}
	operator const char *()		{ return url.blank() ? NULL : (const char*)url; }
	const char *GetExt()		{ return ext; }
};

fixed_string<1024>	GetURLDialog::url;
#endif

namespace mime {
	struct Type {
		const char *name, *ext;
	};
	const Type application[] = {
		{"atom+xml",	"xml"},			//Atom feeds
		{"ecmascript",	"js"},			//ECMAScript/JavaScript; Defined in RFC 4329
	//	{"EDI-X12",		"EDI-X12"},		//EDI X12 data; Defined in RFC 1767
	//	{"EDIFACT",		"EDIFACT"},		//EDI EDIFACT data; Defined in RFC 1767
		{"json",		"json"},		//JavaScript Object Notation JSON; Defined in RFC 4627
		{"javascript",	"js"},			//ECMAScript/JavaScript; Defined in RFC 4329
		{"octet-stream","bin"},			//Arbitrary binary data
		{"ogg",			"ogg"},			//Ogg, a multimedia bitstream container format; Defined in RFC 5334
		{"pdf",			"pdf"},			//Portable Document Format
		{"postscript",	"ps"},			//PostScript; Defined in RFC 2046
		{"rdf+xml",		"xml"},			//Resource Description Framework; Defined by RFC 3870
		{"rss+xml",		"xml"},			//RSS feeds
		{"soap+xml",	"xml"},			//SOAP; Defined by RFC 3902
	//	{"font-woff",	"font-woff"},	//Web Open Font Format
		{"xhtml+xml",	"xml"},			//XHTML; Defined by RFC 3236
		{"xml",			"xml"},			//XML files; Defined by RFC 3023
		{"xml-dtd",		"dtd"},			//DTD files; Defined by RFC 3023
		{"xop+xml",		"xop"},			//XOP
		{"zip",			"zip"},			//ZIP archive files; Registered
		{"gzip",		"gz"},			//Gzip, Defined in RFC 6713
		0,
	};

	const Type audio[] = {
		{"basic",		"basic"},		//?-law audio at 8 kHz, 1 channel; Defined in RFC 2046
		{"L24",			"L24"},			//24bit Linear PCM audio at 8–48 kHz, 1-N channels; Defined in RFC 3190
		{"mp4",			"mp4"},			//MP4 audio
		{"mpeg",		"mpeg"},		//MP3 or other MPEG audio; Defined in RFC 3003
		{"ogg",			"ogg"},			//Ogg Vorbis, Speex, Flac and other audio; Defined in RFC 5334
		{"vorbis",		"vorbis"},		//Vorbis encoded audio; Defined in RFC 5215
		{"vnd.rn-realaudio","ra"},		//RealAudio; Documented in RealPlayer Help
		{"vnd.wave",	"wav"},			//WAV audio; Defined in RFC 2361
		{"webm",		"webm"},		//WebM open media format
		0,
	};

	const Type image[] = {
		{"jpeg",		"jpg"},			//JPEG JFIF image; Defined in RFC 2045 and RFC 2046
		{"pjpeg",		"jpg"},			//JPEG JFIF image
		{"png",			"png"},			//Portable Network Graphics; Registered, Defined in RFC 2083
		{"svg+xml",		"svg"},			//SVG vector image; Defined in SVG Tiny 1.2 Specification Appendix M
		{"tiff",		"tif"},			//Tag Image File Format (only for Baseline TIFF); Defined in RFC 3302
		0,
	};

	const Type message[] = {
		{"imdn+xml",	"xml"},			//IMDN Instant Message Disposition Notification; Defined in RFC 5438
	//	{"partial",		"partial"},		//Email; Defined in RFC 2045 and RFC 2046
	//	{"rfc822",		"rfc822"},		//Email; EML files, MIME files, MHT files, MHTML files; Defined in RFC 2045 and RFC 2046
		0,
	};

	const Type model[] = {
	//	{"example",		"example"},		//Defined in RFC 4735
	//	{"iges",		"iges"},		//IGS files, IGES files; Defined in RFC 2077
	//	{"mesh",		"mesh"},		//MSH files, MESH files; Defined in RFC 2077, SILO files
	//	{"vrml",		"vrml"},		//WRL files, VRML files; Defined in RFC 2077
	//	{"x3d+binary",	"x3d+binary"},	//X3D ISO standard for representing 3D computer graphics, X3DB binary files
	//	{"x3d+vrml",	"x3d+vrml"},	//X3D ISO standard for representing 3D computer graphics, X3DV VRML files
		{"x3d+xml",		"xml"},			//X3D ISO standard for representing 3D computer graphics, X3D XML files
		0,
	};

	const Type multipart[] = {
	//	{"mixed",		"mixed"},		//MIME Email; Defined in RFC 2045 and RFC 2046
	//	{"alternative",	"alternative"},	//MIME Email; Defined in RFC 2045 and RFC 2046
	//	{"related",		"related"},		//MIME Email; Defined in RFC 2387 and used by MHTML (HTML mail)
	//	{"form-data",	"form-data"},	//MIME Webform; Defined in RFC 2388
	//	{"signed",		"signed"},		//Defined in RFC 1847
	//	{"encrypted",	"encrypted"},	//Defined in RFC 1847
		0,
	};

	const Type text[] = {
		{"cmd",			"txt"},			//commands; subtype resident in Gecko browsers like Firefox 3.5
		{"css",			"css"},			//Cascading Style Sheets; Defined in RFC 2318
		{"csv",			"csv"},			//Comma-separated values; Defined in RFC 4180
		{"html",		"html"},		//HTML; Defined in RFC 2854
		{"javascript",	"js"},			//JavaScript
		{"plain",		"txt"},			//Textual data; Defined in RFC 2046 and RFC 3676
		{"vcard",		"txt"},			//vCard (contact information); Defined in RFC 6350
		{"xml",			"xml"},			//Extensible Markup Language; Defined in RFC 3023
		0,
	};

	const Type video[] = {
		{"mpeg",		"mpg"},			//MPEG-1 video with multiplexed audio; Defined in RFC 2045 and RFC 2046
		{"mp4",			"mp4"},			//MP4 video; Defined in RFC 4337
		{"ogg",			"ogg"},			//Ogg Theora or other video (with audio); Defined in RFC 5334
		{"quicktime",	"mov"},			//QuickTime video; Registered
	//	{"webm",		"webm"},		//WebM Matroska-based open media format
	//	{"x-matroska",	"x-matroska"},	//Matroska open media format
		{"x-ms-wmv",	"wmv"},			//Windows Media Video; Documented in Microsoft KB 288102
		{"x-flv",		"flv"},			//Flash video (FLV files)
	0,
	};

	const struct Group {
		const char *name;
		const Type *types;
	} groups[] = {
		{"application",	application,	},
		{"audio",		audio,			},
		{"image",		image,			},
		{"message",		message,		},
		{"model",		model,			},
		{"multipart",	multipart,		},
		{"text",		text,			},
		{"video",		video,			},
	};

	FileHandler *GetFileHandler(const char *mime) {
		if (const char *sep = str(mime).find('/')) {
			for (int i = 0; i < num_elements(groups); i++) {
				if (istr(mime, sep) == groups[i].name) {
					for (const Type *type = groups[i].types; type->name; ++type) {
						if (type->name == istr(sep + 1))
							return FileHandler::Get(type->ext);
					}
					break;
				}
			}
		}
		return 0;
	}

} // namespace mime

#ifdef PLAT_PC

struct URLDevice : DeviceT<URLDevice>, DeviceCreateT<URLDevice> {
	void			operator()(const DeviceAdd &add) {
		add("URL...", this, LoadPNG("IDB_DEVICE_URL"));
	}
	ISO_ptr<void>	operator()(const Control &main) {
		GetURLDialog	dialog(main);
		if (const char *url = dialog) {
			HTTP	http("IsoEditor", url);
			HTTPistream	input = http.Get(http.PathParams());
			if (input.exists()) {
				FileHandler	*fh;
				if (dialog.GetExt() == str("<auto>")) {
					fh	= mime::GetFileHandler(input.headers.get("content-type"));
				} else {
					//const char *ext = filename(URLcomponents(url).path).ext();
					fh	= FileHandler::Get(dialog.GetExt());
				}
				if (!fh)
					fh = FileHandler::Get("bin");
				return fh->Read("url", input);
			}
		}
		return ISO_NULL;
	}
} url_device;

#endif
