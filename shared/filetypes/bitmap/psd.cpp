#include "bitmapfile.h"

//-----------------------------------------------------------------------------
//	Adobe photoshop bitmaps
//-----------------------------------------------------------------------------

using namespace iso;

class PSDFileHandler : public BitmapFileHandler {
protected:
	enum PSDRESOURCE {
		PSDR_PHOTOSHOP2INFO		= 0x03E8,	// 1000 (Obsolete--Photoshop 2.0 only) Contains five 2-byte values: number of channels, rows, columns, depth, and mode
		PSDR_MAC_PRINTINFO		= 0x03E9,	// 1001 Macintosh print manager print info record
		PSDR_INDEXEDCOLORS		= 0x03EB,	// 1003 (Obsolete--Photoshop 2.0 only) Indexed color table
		PSDR_RESOLUTIONINFO		= 0x03ED,	// 1005 ResolutionInfo structure. See Appendix A in Photoshop SDK Guide.pdf
		PSDR_ALPHANAMES			= 0x03EE,	// 1006 Names of the alpha channels as a series of Pascal strings.
		PSDR_DISPLAYINFO		= 0x03EF,	// 1007 DisplayInfo structure. See Appendix A in Photoshop SDK Guide.pdf
		PSDR_CAPTION			= 0x03F0,	// 1008 Optional. The caption as a Pascal string.
		PSDR_BORDER				= 0x03F1,	// 1009 Border information. Contains a fixed-number for the border width, and 2 bytes for border units (1=inches, 2=cm, 3=points, 4=picas, 5=columns).
		PSDR_BACKGROUNDCOLOUR	= 0x03F2,	// 1010 Background color. See the Colors additional file information.
		PSDR_PRINTFLAGS			= 0x03F3,	// 1011 Print flags. A series of one byte boolean values (see Page Setup dialog): labels, crop marks, color bars, registration marks, negative, flip, interpolate, caption.
		PSDR_GRAYSCALEINGO		= 0x03F4,	// 1012 Grayscale and multichannel halftoning information.
		PSDR_HALFTONEINFO		= 0x03F5,	// 1013 Color halftoning information.
		PSDR_DUOHALFTONEINGO	= 0x03F6,	// 1014 Duotone halftoning information.
		PSDR_GRAYSCALETRANSFER	= 0x03F7,	// 1015 Grayscale and multichannel transfer function.
		PSDR_COLOURTRANSFER		= 0x03F8,	// 1016 Color transfer functions.
		PSDR_DUOTONETRANSFER	= 0x03F9,	// 1017 Duotone transfer functions.
		PSDR_DUOTONEIMAGEINFO	= 0x03FA,	// 1018 Duotone image information.
		PSDR_EFFECTIVEBW		= 0x03FB,	// 1019 Two bytes for the effective black and white values for the dot range.
		PSDR_EPSOPTS			= 0x03FD,	// 1021 EPS options.
		PSDR_QUICKMASK			= 0x03FE,	// 1022 Quick Mask information. 2 bytes containing Quick Mask channel ID, 1 byte boolean indicating whether the mask was initially empty.
		PSDR_LAYERSTATE			= 0x0400,	// 1024 Layer state information. 2 bytes containing the index of target layer. 0=bottom layer.
		PSDR_WORKINGPATH		= 0x0401,	// 1025 Working path (not saved). See path resource format later in this chapter.
		PSDR_LAYERSGROUP		= 0x0402,	// 1026 Layers group information. 2 bytes per layer containing a group ID for the dragging groups. Layers in a group have the same group ID.
		PSDR_IPTC_NAA			= 0x0404,	// 1028 IPTC-NAA record. This contains the File Info... information. See the IIMV4.pdf document.
		PSDR_RAWIMAGEMODE		= 0x0405,	// 1029 Image mode for raw format files.
		PSDR_JPEGQUALITY		= 0x0406,	// 1030 JPEG quality. Private.
		PSDR_GRIDGUIDES			= 0x0408,	// 1032 Grid and guides information. See grid and guides resource format later in this chapter.
		PSDR_THUMBNAIL			= 0x0409,	// 1033 Thumbnail resource. See thumbnail resource format later in this chapter.
		PSDR_COPYRIGHT			= 0x040A,	// 1034 Copyright flag. Boolean indicating whether image is copyrighted. Can be set via Property suite or by user in File Info...
		PSDR_URL				= 0x040B,	// 1035 URL. Handle of a text string with uniform resource locator. Can be set via Property suite or by user in File Info...
		PSDR_THUMBNAIL2			= 0x040C,	// 1036 Thumbnail resource. See thumbnail resource format later in this chapter.
		PSDR_GLOBALANGLE		= 0x040D,	// 1037 Global Angle. 4 bytes that contain an integer between 0..359 which is the global lighting angle for effects layer. If not present assumes 30.
		PSDR_COLOURSAMPLERS		= 0x040E,	// 1038 Color samplers resource. See color samplers resource format later in this chapter.
		PSDR_ICCPROFILE			= 0x040F,	// 1039 ICC Profile. The raw bytes of an ICC format profile, see the ICC34.pdf and ICC34.h files from the Internation Color Consortium located in the documentation section
		PSDR_WATERMARK			= 0x0410,	// 1040 One byte for Watermark.
		PSDR_ICC_UNTAGGED		= 0x0411,	// 1041 ICC Untagged. 1 byte that disables any assumed profile handling when opening the file. 1 = intentionally untagged.
		PSDR_EFFECTSVISIBLE		= 0x0412,	// 1042 Effects visible. 1 byte global flag to show/hide all the effects layer. Only present when they are hidden.
		PSDR_SPOTHALFTONE		= 0x0413,	// 1043 Spot Halftone. 4 bytes for version, 4 bytes for length, and the variable length data.
		PSDR_CUSTOMIDS			= 0x0414,	// 1044 Document specific IDs, layer IDs will be generated starting at this base value or a greater value if we find existing IDs to already exceed it.
		PSDR_UNICODEALPHANAMES	= 0x0415,	// 1045 Unicode Alpha Names. 4 bytes for length and the string as a unicode string.
		PSDR_COLOURCOUNT		= 0x0416,	// 1046 Indexed Color Table Count. 2 bytes for the number of colors in table that are actually defined
		PSDR_TRANSPARENTINDEX	= 0x0417,	// 1047 Tansparent Index. 2 bytes for the index of transparent color, if any.
		PSDR_GLOBALALT			= 0x0419,	// 1049 Global Altitude. 4 byte entry for altitude
		PSDR_SLICES				= 0x041A,	// 1050 Slices. See description later in this chapter
		PSDR_WORKFLOWURL		= 0x041B,	// 1051 Workflow URL. Unicode string, 4 bytes of length followed by unicode string.
		PSDR_JUMPTOXPEP			= 0x041C,	// 1052 Jump To XPEP. 2 bytes major version, 2 bytes minor version, 4 bytes count. Following is repeated for count: 4 bytes block size, 4 bytes key, if key = 'jtDd' then next is a Boolean for the dirty flag otherwise it’s a 4 byte entry for the mod date.
		PSDR_ALPHAIDENT			= 0x041D,	// 1053 Alpha Identifiers. 4 bytes of length, followed by 4 bytes each for every alpha identifier.
		PSDR_URLLIST			= 0x041E,	// 1054 URL List. 4 byte count of URLs, followed by 4 byte long, 4 byte ID, and unicode string for each count.
		PSDR_VERSIONINFO		= 0x0421,	// 1057 Version Info. 4 byte version, 1 byte HasRealMergedData, unicode string of writer name, unicode string of reader name, 4 bytes of file version.
		PSDR_EXIF1				= 0x0422,	// 1058 EXIF data 1. See http://www.kodak.com/global/plugins/acrobat/en/service/digCam/exifStandard2.pdf
		PSDR_EXIF3				= 0x0423,	// 1059 EXIF data 3. See http://www.kodak.com/global/plugins/acrobat/en/service/digCam/exifStandard2.pdf
		PSDR_XMPMETADATA		= 0x0424,	// 1060 XMP Metadata. File info as XML description. Use XMPToolkitMT.lib from Adobe XMP SDK to parse this resource. See http://Partners.adobe.com/asn/developer/xmp/main.html
		PSDR_CAPTIONDIGEST		= 0x0425,	// 1061 Caption digest. 16 bytes: RSA Data Security, MD5 message-digest algorithm
		PSDR_PRINTSCALE			= 0x0426,	// 1062 Print scale. 2 bytes style (0 = centered, 1 = size to fit, 2 = user defined). 4 bytes x location (floating point). 4 bytes y location (floating point). 4 bytes scale (floating point)
		PSDR_PIXELASPECT		= 0x0428,	// 1064 Pixel Aspect Ratio. 4 bytes (version = 1 or 2), 8 bytes double, x / y of a pixel. Version 2, attempting to correct values for NTSC and PAL, previously off by a factor of approx. 5%.
		PSDR_LAYERCOMPS			= 0x0429,	// 1065 Layer Comps. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure)
		PSDR_ALTDUOTONE			= 0x042A,	// 1066 Alternate Duotone Colors. 2 bytes (version = 1), 2 bytes count, following is repeated for each count: [ Color: 2 bytes for space followed by 4 * 2 byte color component ], following this is another 2 byte count, usually 256, followed by Lab colors one byte each for L, a, b. This resource is not read or used by Photoshop.
		PSDR_ALTSPOT			= 0x042B,	// 1067 Alternate Spot Colors. 2 bytes (version = 1), 2 bytes channel count, following is repeated for each count: 4 bytes channel ID, Color: 2 bytes for space followed by 4 * 2 byte color component. This resource is not read or used by Photoshop.
		PSDR_LAYERSEL			= 0x042D,	// 1069 Layer Selection ID(s). 2 bytes count, following is repeated for each count: 4 bytes layer ID
		PSDR_HDRTONING			= 0x042E,	// 1070 HDR Toning information
		PSDR_PRINTINFO			= 0x042F,	// 1071 Print info
		PSDR_LAYERGROUPS		= 0x0430,	// 1072 Layer Group(s) Enabled ID. 1 byte for each layer in the document, repeated by length of the resource. NOTE: Layer groups have start and end markers
		PSDR_COLORSAMPLERS		= 0x0431,	// 1073 Color samplers resource. Also see ID 1038 for old format. See See Color samplers resource format.
		PSDR_MEASUREMENTSCALE	= 0x0432,	// 1074 Measurement Scale. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure)
		PSDR_TIMELINE			= 0x0433,	// 1075 Timeline Information. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure)
		PSDR_SHEETDISCLOSURE	= 0x0434,	// 1076 Sheet Disclosure. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure)
		PSDR_DISPLAYINFO2		= 0x0435,	// 1077 DisplayInfo structure to support floating point clors. Also see ID 1007. See Appendix A in Photoshop API Guide.pdf .
		PSDR_ONIONSKINS			= 0x0436,	// 1078 Onion Skins. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure)
		PSDR_COUNTINFO			= 0x0438,	// 1080 Count Information. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure) Information about the count in the document. See the Count Tool.
		PSDR_PRINTINFO2			= 0x043A,	// 1082 Print Information. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure) Information about the current print settings in the document. The color management options.
		PSDR_PRINTSTYLE			= 0x043B,	// 1083 Print Style. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure) Information about the current print style in the document. The printing marks, labels, ornaments, etc.
		PSDR_MAC_NSPRINTINFO	= 0x043C,	// 1084 Macintosh NSPrintInfo. Variable OS specific info for Macintosh. NSPrintInfo. It is recommened that you do not interpret or use this data.
		PSDR_WIN_DEVMODE		= 0x043D,	// 1085 Windows DEVMODE. Variable OS specific info for Windows. DEVMODE. It is recommened that you do not interpret or use this data.
		PSDR_AUTOSAVEPATH		= 0x043E,	// 1086 Auto Save File Path. Unicode string. It is recommened that you do not interpret or use this data.
		PSDR_AUTOSAVEFORMAT		= 0x043F,	// 1087 Auto Save Format. Unicode string. It is recommened that you do not interpret or use this data.
		PSDR_PATHSELECTION		= 0x0440,	// 1088 Path Selection State. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure) Information about the current path selection state.
		PSDR_PATHINFO			= 0x07D0,	// 2000-2997 Path Information (saved paths). See See Path resource format.
		PSDR_CLIPPINGPATH		= 0x0BB7,	// 2999 Name of clipping path. See path resource format later in this chapter.
		PSDR_ORIGINPATHINFO		= 0x0BB8,	// 3000 Origin Path Info. 4 bytes (descriptor version = 16), Descriptor (see See Descriptor structure) Information about the origin path data.
		PSDR_PLUGINRESOURCES	= 0x0FA0,	// 4000-4999 Plug-In resource(s). Resources added by a plug-in. See the plug-in API found in the SDK documentation
		PSDR_IMAGEREADYVARS		= 0x1B58,	// 7000 Image Ready variables. XML representation of variables definition
		PSDR_IMAGEREADYDATA		= 0x1B59,	// 7001 Image Ready data sets
		PSDR_LIGHTROOMWORKFLOW	= 0x1F40,	// 8000 Lightroom workflow, if present the document is in the middle of a Lightroom workflow.
		PSDR_PRINTFLAGSINFO		= 0x2710,	// 10000 Print flags information. 2 bytes version (=1), 1 byte center crop marks, 1 byte (=0), 4 bytes bleed width value, 2 bytes bleed width scale.
	};

	enum PSDMODE {
		PSD_Bitmap				= 0,
		PSD_Grayscale			= 1,
		PSD_Indexed				= 2,
		PSD_RGB					= 3,
		PSD_CMYK				= 4,
		PSD_Multichannel		= 7,
		PSD_Duotone				= 8,
		PSD_Lab					= 9,
		PSD_MaxMode				= 9,
	};

	enum PSDCOMPRESSION : uint16 {
		PSDC_RAW				= 0,
		PSDC_RLE				= 1,
		PSDC_ZIP				= 2,
		PSDC_ZIPpred			= 3
	};

	#pragma pack(1)
	struct PSDheader : bigendian_types {
		uint32	signature;		// Always equal to 8BPS. Do not try to read the file if the signature does not match this value.
		uint16	version;		// Always equal to 1. Do not try to read the file if the version does not match this value.
		char	reserved[6];	// Must be zero.
		uint16	Channels;		// The number of channels in the image, including any alpha channels.  Supported range is 1 to 24.
		uint32	Rows;			// The height of the image in pixels. Supported range is 1 to 30,000.
		uint32	Columns;		// The width of the image in pixels. Supported range is 1 to 30,000.
		uint16	Depth;			// The number of bits per channel. Supported values are 1, 8, and 16.
		uint16	Mode;			// The color mode of the file. Supported values are the PSDMODEs
		bool	valid() const	{ return signature == '8BPS' && (version == 1 || version == 2); }
	};
	struct PSDthumbnailheader : bigendian_types {
		uint32	format;			// = 1 (kJpegRGB). Also supports kRawRGB (0).
		uint32	width;			// Width of thumbnail in pixels.
		uint32	height;			// Height of thumbnail in pixels.
		uint32	widthbytes;		// Padded row bytes as (width * bitspixel + 31) / 32 * 4.
		uint32	size;			// Total size as widthbytes * height * planes
		uint32	compressedsize;	// Size after compression. Used for consistentcy check.
		uint16	bitspixel;		// = 24. Bits per pixel.
		uint16	planes;			// = 1. Number of planes.
	};
	struct PSDchannelinfo {
		int16	channel_id;	//0=red,1=green, -1=transparency mask,-2=user supplied layer mask, -3=real user supplied layer mask (when both a user mask and a vector mask are present)
		uint64	length;
		void	read(istream_ref file, bool big) {
			file.read(channel_id);
			length = big ? file.get<uint64be>() : file.get<uint32be>();
		}
	};
	template<typename T> struct _PSDrect {
		T			top, left, bottom, right;
		template<typename U> void operator=(const _PSDrect<U> &r) { top = r.top; left = r.left; bottom = r.bottom; right = r.right; }
		auto		Width()		const	{ return right - left; }
		auto		Height()	const	{ return bottom - top; }
	};
	typedef _PSDrect<uint32le>	PSDrect;

	struct PSDlayerinfo {
		PSDrect			rect;
		dynamic_array<PSDchannelinfo>	chans;
		uint32			blend;
		uint8			opacity;
		uint8			clipping;
		uint8			flags;

		PSDrect			mask_rect;
		uint8			mask_default;
		uint8			mask_flags;

		uint8			mask_user_density;
		double			mask_user_feather;
		uint8			mask_vector_density;
		double			mask_vector_feather;
		char			name[256];

		bool	read(istream_ref file, bool big) {
			rect		= file.get<_PSDrect<uint32be> >();

			chans.resize(file.get<int16be>());
			for (auto &c : chans)
				c.read(file, big);

			if (file.get<uint32be>() != '8BIM')
				return false;

			blend		= file.get<uint32be>();
			opacity		= file.getc();
			clipping	= file.getc();
			flags		= file.getc();

			file.getc();
			streamptr	endoflayer	= file.get<uint32be>();
			endoflayer				+= file.tell();

			if (uint32	mask_data = file.get<uint32be>()) {	// if there's mask data
				mask_rect		= file.get<_PSDrect<uint32be> >();
				mask_default	= file.getc();
				mask_flags		= file.getc();
				if (mask_flags & 1) {
					mask_rect.top		+= rect.top;
					mask_rect.left		+= rect.left;
					mask_rect.bottom	+= rect.top;
					mask_rect.right		+= rect.left;
				}
				if (mask_flags & 4) {
					uint8	mask_params	= file.getc();
					if (mask_params & 1)
						mask_user_density = file.getc();
					if (mask_params & 2)
						mask_user_feather = file.get<doublebe>();
					if (mask_params & 4)
						mask_vector_density = file.getc();
					if (mask_params & 8)
						mask_vector_feather = file.get<doublebe>();
				}
				if (mask_data == 20) {
					file.get<uint16be>();	// layer mask padding
				} else {
					file.getc();	//real flags
					file.getc();	//real user mask background
					file.get<_PSDrect<uint32be>>();	// repeated
				}
			} else {
				mask_rect		= rect;
				mask_default	= 0;
				mask_flags		= 0;
			}

			file.seek_cur(file.get<uint32be>());		// skip layer blending range info

			uint8	namelen	= file.getc();			// get layer name
			file.readbuff(name, namelen);
			name[namelen] = 0;

			file.seek(endoflayer);
			return true;
		}
	};
	#pragma pack()

	static bool ReadRow(istream_ref file, uint8 *buffer, int width, PSDCOMPRESSION compression) {
		switch (compression) {
			case PSDC_RAW:
				return check_readbuff(file, buffer, width);

			case PSDC_RLE:
				for (int x = 0; x < width;) {
					int8 n = file.getc();
					if (n >= 0) {
						if (x + n + 1 > width)
							return false;
						file.readbuff(buffer + x, n + 1);
						x += n + 1;
					} else if (n > -128) {
						if (x + -n + 1 > width)
							return false;
						char c = file.getc();
						memset(buffer + x, c, -n + 1);
						x += -n + 1;
					}
				}
				return true;

			case PSDC_ZIP:
			case PSDC_ZIPpred:
			default:
				return false;
		}
	}

	static int WriteRow(ostream_ref file, char *buffer, int width, bool compression) {
		if (compression) {
			streamptr	start = file.tell();
			int			i = 0;
			char		c = buffer[i];

			do {
				int		s = i, run;
				char	p;
				do {
					p	= c;
					run	= 1;
					while (++i < width && (c = buffer[i]) == p && run < 0x80)
						run++;
				} while (i < width && run <= 2);

				if (run <= 2)
					run = 0;

				while (i - run > s) {
					int	t = i - run - s;
					if (t > 0x80)
						t = 0x80;
					file.putc(t - 1);
					file.writebuff(buffer + s, t);
					s = s + t;
				}

				if (run) {
					file.putc(0x101-run);
					file.putc(p);
				}
			} while (i < width);
			return int(file.tell() - start);
		} else {
			file.writebuff(buffer, width);
			return width;
		}
	}

	static bool ReadLayer(istream_ref file, const block<ISO_rgba, 2> &slice, PSDlayerinfo &layer, int mode) {
		uint32	width	= slice.size<1>(), height = slice.size<2>();
		uint8	*buffer	= new uint8[max(layer.rect.Width(), layer.mask_rect.Width())];
		bool	ok		= true;

		for (int c = 0; ok && c < layer.chans.size(); c++) {
			int		channel	= layer.chans[c].channel_id;
			PSDrect	&rect	= channel == -2 ? layer.mask_rect : layer.rect;
			int		rwidth	= rect.Width();
			int		rheight	= rect.Height();

			if (channel == -2) {
				ISO_rgba	*p	= slice[0];
				uint8		a	= layer.mask_default;
				for (int n = height * width; n--; p++->a = a);
			}

			uint16	compression = file.get<uint16be>();
			if (compression)
				file.seek_cur(rheight * 2);

			if (channel < 0)
				channel = 3;

			for (int y = rect.top; y < rect.bottom; y++) {
				if (!(ok = ReadRow(file, buffer, rwidth, (PSDCOMPRESSION)compression)))
					break;
				if (channel < 4 && y >= 0 && y < height) {
					uint8	*dest = (uint8*)(slice[y] + rect.left) + channel;
					for (int x = 0; x < rwidth; x++, dest += 4) {
						if (x + rect.left >= 0 && x + rect.left < width)
							*dest = buffer[x];
					}
				}
			}
		}
		if (mode == PSD_Grayscale) {
			ISO_rgba	*p = slice[0];
			for (int n = height * width; n--; p++)
				p->g = p->b = p->r;
		}
		delete[] buffer;
		return ok;
	}

	static ISO_ptr<void>	Read(tag id, istream_ref file, bool layers);

	const char*		GetExt() override { return "psd";				}
	const char*		GetDescription() override { return "Photoshop Image";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		PSDheader	header = file.get();
		return header.signature == '8BPS' && header.version == 1 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		return Read(id, FileInput(fn).me(), filename(fn.name()).ext() == ".layers" || ISO::root("variables")["layers"].GetInt());
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return Read(id, file, ISO::root("variables")["layers"].GetInt() != 0);
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} psd;

ISO_ptr<void> PSDFileHandler::Read(tag id, istream_ref file, bool layers) {
	PSDheader	header = file.get();
	if (!header.valid())
		return ISO_NULL;

	bool		big			= header.version > 1;
	int			channels	= header.Channels;
	int			rows		= header.Rows;
	int			columns		= header.Columns;
	int			mode		= header.Mode;
	int			depth		= header.Depth;
	int			alphachan	= -1;
	int			clut		= 0;
	uint16be	*groups		= 0;
	uint32		version;

	if (mode > PSD_MaxMode || (depth != 1 && depth != 8 && depth != 16 && depth != 32))
		return ISO_NULL;

	// COLOR MODE DATA
	uint8	colours[256 * 3];
	if (uint32 length = file.get<uint32be>()) {
		file.readbuff(colours, length);
		clut = length / 3;
	}

	malloc_block	alphanames;

	// IMAGE RESOURCES
	for (uint64 length = file.get<uint32be>(); length;) {
		uint32	osType = file.get<uint32be>();
		if (osType != '8BIM' && osType != 'MeSa')
			return ISO_NULL;

		PSDRESOURCE	id		= PSDRESOURCE((uint16)file.get<uint16be>());
		uint8		len		= file.getc();
		if (!(len & 1))
			len++;
		file.seek_cur(len);
		uint32		size	= file.get<uint32be>();
		streamptr	tell	= file.tell();

		switch (id) {
			case PSDR_ALPHANAMES:
				file.readbuff(alphanames.create(size), size);
				break;
			case PSDR_THUMBNAIL:
			case PSDR_THUMBNAIL2: {
//				if (ReadThumbnail(bm, file, size, id == 1033))
//					return true;
				break;
			}
			case PSDR_ALPHAIDENT: {
				file.get<uint32be>();
				if (size > 4)
					alphachan = file.get<uint32be>();
				break;
			}
			case PSDR_COLOURCOUNT:
				break;

			case PSDR_TRANSPARENTINDEX: {
#if 0
				int	i = file.GetBEW();
				if (i < 256)
					bm.Clut(i).a = 0;
				bm.SetFlag(BMF_CLUTALPHA | BMF_ONEBITALPHA);
#endif
				break;
			}
			case PSDR_VERSIONINFO:
				file.read(version);
				break;

			case PSDR_LAYERSGROUP:
				if (layers)
					file.readbuff(groups = new uint16be[size / 2], size);
				break;

		}
		size = (size + 1) & ~1;
		file.seek(tell + size);
		length -= 11 + len + size;
	}

	// LAYER AND MASK INFORMATION
	streamptr	imagedata	= big ? file.get<uint64be>() : file.get<uint32be>();
	imagedata	+= file.tell();

	if (layers) {
		uint64	layerslength	= big ? file.get<uint64be>() : file.get<uint32be>();
		int		nlayers			= file.get<int16be>();
		if (nlayers < 0)
			nlayers = -nlayers;

		dynamic_array<PSDlayerinfo>	layers(nlayers);
		for (auto &i : layers)
			i.read(file, big);

		if (true) {
			ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > > bms(id, nlayers);
			for (int i = 0; i < nlayers; i++) {
				ISO_ptr<bitmap> &bm = (*bms)[i];
				bm.Create(layers[i].name);
				if (!bm->Create(columns, rows))
					break;
				fill(bm->All(), ISO_rgba(0,0,0,255));
				if (!ReadLayer(file, bm->All(), layers[i], mode))
					break;
			}
			return bms;
		} else {
			ISO_ptr<bitmap> bm(id);
			bm->Create(columns, rows * nlayers, 0, nlayers);
			fill(bm->All(), ISO_rgba(0,0,0,255));
			for (int i = 0; i < nlayers; i++)
				if (!ReadLayer(file, bm->Slice(i), layers[i], mode))
					break;
			return bm;
		}
	}

	file.seek(imagedata);

	// IMAGE DATA
	if (depth > 8) {
		ISO_ptr<HDRbitmap> bm(id);
		bm->Create(columns, rows);

		uint16	compression = file.get<uint16be>();
		if (compression)
			file.seek_cur(rows * channels * 2);

		uint32	*buffer	= new uint32[columns];
		for (int c = 0; c < channels; c++) {
			for (int y = 0; y < rows; y++) {
				file.readbuff(buffer, columns * sizeof(uint32));
//				ReadRow(file, buffer, columns, compression);
				if (c < (mode < PSD_RGB ? 2 : 4) || c == alphachan) {
					float	*dest = (float*)bm->ScanLine(y) + (c == alphachan ? 3 : c);
					floatbe	*srce = (floatbe*)buffer;
					for (int x = 0; x < columns; x++, dest += 4)
						*dest = *srce++;
				}
			}
		}
		delete[] buffer;
		return bm;
	}

	ISO_ptr<bitmap> bm(id);
	bm->Create(columns, rows);

	if (clut) {
		bm->CreateClut(clut);
		for (int i = 0; i < clut; i++)
			bm->Clut(i) = ISO_rgba(colours[i + clut * 0], colours[i + clut * 1], colours[i + clut * 2]);
	}

	uint16	compression = file.get<uint16be>();
	if (compression)
		file.seek_cur(rows * channels * 2);

	alphachan = channels;
	if (channels >= (mode < PSD_RGB ? 2 : 4))
		alphachan = channels - 1;

	bool	ok = true;
	if (mode == PSD_Bitmap) {
		int				w		= (columns + 7) / 8;
		malloc_block	buffer(w);
		for (int y = 0; y < rows; y++) {
			if (!(ok = ReadRow(file, buffer, w, (PSDCOMPRESSION)compression)))
				break;
			ISO_rgba	*dest = bm->ScanLine(y);
			uint8		*srce = buffer;
			uint8		byte;
			for (int x = 0; x < columns; x++, dest++) {
				if ((x & 7) == 0)
					byte = *srce++;
				else
					byte <<= 1;
				*dest = (byte & 0x80) ? 0 : 255;
			}
		}
	} else {
		malloc_block	buffer(columns);
		int				visible	= mode < PSD_RGB ? 1 : 3;
		for (int c = 0; ok && c < channels; c++) {
			for (int y = 0; y < rows; y++) {
				if (!(ok = ReadRow(file, buffer, columns, (PSDCOMPRESSION)compression)))
					break;
				if (c < visible || c == alphachan) {
					uint8		*dest = (uint8*)bm->ScanLine(y) + (c == alphachan ? 3 : c);
					uint8		*srce = buffer;
					for (int x = columns; x--; srce++, dest += 4)
						*dest = *srce;
				}
			}
		}
		if (alphanames && str((char*)alphanames + 1, *(uint8*)alphanames) == "Transparency") {
			ISO_rgba	*p = bm->ScanLine(0);
			for (int n = rows * columns; n--; p++) {
				uint8 a = p->a;
				if (a != 0 && a != 255) {
					p->r = (p->r - (255 - a)) * 255 / a;
					p->g = (p->g - (255 - a)) * 255 / a;
					p->b = (p->b - (255 - a)) * 255 / a;
				}
			}
		}
		if (channels != alphachan + 1) {
			uint8	*alpha = (uint8*)bm->ScanLine(0) + 3;
			for (int n = rows * columns; n--; alpha += 4)
				*alpha = 255;
		}
		if (mode == PSD_Grayscale) {
			ISO_rgba	*p = bm->ScanLine(0);
			for (int n = rows * columns; n--; p++)
				p->g = p->b = p->r;
		}
	}
	SetBitmapFlags(bm.get());
	return bm;
}

bool PSDFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
	if (!bm)
		return false;

	PSDheader	header;
	int			channels, rows, columns;

//	bm.NoClutAlpha();

	channels	= (bm->IsPaletted() || bm->IsIntensity() ? 1 : 3) + bm->HasAlpha();
	rows		= bm->Height();
	columns		= bm->Width();

	// FILE HEADER
	header.signature	= '8BPS';
	header.version		= 1;
	header.Channels		= channels;
	header.Rows			= rows;
	header.Columns		= columns;
	header.Depth		= 8;
	header.Mode			= bm->IsIntensity() ? PSD_Grayscale : bm->IsPaletted() ? PSD_Indexed : PSD_RGB;
	clear(header.reserved);

	file.write(header);

	// COLOR MODE DATA
	if (bm->IsPaletted()) {
		uint32	clutsize	= bm->ClutSize();
		Texel<R8G8B8>	colours[256];
		copy_n(bm->Clut(), colours, clutsize);
		file.write(uint32be(clutsize * 3));
		file.writebuff(colours, sizeof(colours[0]) * clutsize);
	} else {
		file.write(uint32(0));
	}

	// IMAGE RESOURCES
	file.write(uint32(0));

	// LAYER AND MASK INFORMATION
	file.write(uint32(0));

	// IMAGE DATA
	bool		compression = false;//(flags & BMWF_COMPRESS) != 0;
	uint16be	*lengths	= new uint16be[rows * channels],
				*lenptr		= lengths;
	streamptr	lenstart;

	file.write(uint16be(compression));
	if (compression) {
		lenstart = file.tell();
		file.seek_cur(rows * channels * 2);
	}

	char	*buffer	= new char[columns];
	for (int c = 0; c < channels; c++) {
		for (int y = 0; y < rows; y++) {
			char	*srce = (char*)bm->ScanLine(y) + c + (channels == 2 && c == 1 ? 2 : 0);
			char	*dest = buffer;
			for (int x = 0; x < columns; x++, srce+=4)
				*dest++ = *srce;
			*lenptr++ = WriteRow(file, buffer, columns, compression);
		}
	}
	delete[] buffer;

	if (compression) {
		file.seek(lenstart);
		file.writebuff(lengths, rows * channels);
		file.seek_end(0);
	}
	delete[] lengths;

	return true;
}


class PSNFileHandler : public PSDFileHandler {
	const char*		GetExt() override { return "psb";				}
	const char*		GetDescription() override { return "Big Photoshop Image";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		PSDheader	header = file.get();
		return header.signature == '8BPS' && header.version == 2 ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

} psb;
