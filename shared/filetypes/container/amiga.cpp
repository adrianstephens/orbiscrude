
//Dirk Stoecker <stoecker@epost.de>
//17th February 2002
//
//Format description of Amiga Icon Format.
//
//This format is used by Amiga computers to display icons for each program or project you want to access from the graphical user interface Workbench.
//
//There are 3 different formats.
//1) The OS1.x/OS2.x icons.
//2) The NewIcon icon extension.
//3) The OS3.5 icon extension.

#include "base/defs.h"


namespace amiga {
using namespace iso;

typedef uint32be	APTR;		// - a memory pointer (usually this gets a boolean meaning on disk)
typedef int8		BYTE;		// - a single byte                   -128..127
typedef uint8		UBYTE;		// - an unsigned byte                   0..255
typedef int16be		WORD;		// - a signed 16 bit value         -32768..32767
typedef uint16be	UWORD;		// - an unsigned 16 bit value           0..65535
typedef int32be		LONG;		// - a signed 32 bit value    -2147483648..2147483647
typedef uint32be	ULONG;		// - a unsigned 32 bit value            0..4294967295

// There are lots of elements marked with ???. These are usually filled with values, which have no effect at all. Thus they can normally be ignored. For some values the usual contents is described.

//******************************
//***** OS1.x/OS2.x format *****
//******************************
//The OS1.x/OS2.x format is mainly an storage of the in-memory structures on disk.
//This means there is many crap in that format, which may have undefined values. This text tries to show which values are important and which not.

//b) The Gadget structure:
struct GADGET {
	enum FLAGS {
		ALWAYS		= 1 << 2,	//always set (image 1 is an image ;-)
		TWO_IMAGE	= 1 << 1,	//if set, we use 2 image-mode
		BACKFILL	= 1 << 0,	//if set we use backfill mode, else complement mode
								//complement mode: gadget colors are inverted
								//backfill mode: like complement, but region outside (color 0) of image is not inverted
	};
	APTR  ga_Next;			//0x00 <undefined> always 0
	WORD  ga_LeftEdge;		//0x04 unused ???
	WORD  ga_TopEdge;		//0x06 unused ???
	WORD  ga_Width;			//0x08 the width of the gadget
	WORD  ga_Height;		//0x0A the height of the gadget
	UWORD ga_Flags;			//0x0C gadget flags
	UWORD ga_Activation;	//0x0E <undefined>
	UWORD ga_GadgetType;	//0x10 <undefined>
	APTR  ga_GadgetRender;	//0x12 <boolean> unused??? always true
	APTR  ga_SelectRender;	//0x16 <boolean> (true if second image present)
	APTR  ga_GadgetText;	//0x1A <undefined> always 0 ???
	LONG  ga_MutualExclude;	//0x1E <undefined>
	APTR  ga_SpecialInfo;	//0x22 <undefined>
	UWORD ga_GadgetID;		//0x26 <undefined>
	APTR  ga_UserData;		//0x28 lower 8 bits:  0 for old, 1 for icons >= OS2.x upper 24 bits: undefined
};

struct ICON {
	enum TYPE {
		DISK	= 1,	//a disk
		DRAWER	= 2,	//a directory
		TOOL	= 3,	//a program
		PROJECT	= 4,	//a project file with defined program to start
		GARBAGE	= 5,	//the trashcan
		DEVICE	= 6,	//should never appear
		KICK	= 7,	//a kickstart disk
		APPICON	= 8,	//should never appear
	};
	UWORD	ic_Magic;			//0x00 always 0xE310
	UWORD	ic_Version;			//0x00 always 1
	GADGET	ic_Gadget;			//0x04 (described above)
	UBYTE	ic_Type;			//0x30 
	UBYTE	ic_Pad;				//0x31 <undefined>
	APTR	ic_DefaultTool;		//0x32 <boolean>
	APTR	ic_ToolTypes;		//0x36 <boolean>
	LONG	ic_CurrentX;		//0x3A X position of icon in drawer/on WorkBench
	LONG	ic_CurrentY;		//0x3E Y position of icon in drawer/on WorkBench
	APTR	ic_DrawerData;		//0x42 <boolean>
	APTR	ic_ToolWindow;		//0x46 <boolean>
	LONG	ic_StackSize;		//0x4A the stack size for program execution (values < 4096 mean 4096 is used)
};

//	This is followed by certain other data structures:
//struct DrawerData            if ic_DrawerData is not zero (see below)
//struct Image                 first image
//struct Image                 second image if ga_SelectRender not zero (see below) in gadget structure
//DefaultTool text             if ic_DefaultTool not zero (format see below)
//ToolTypes texts              if ic_ToolTypes not zero (format see below)
//ToolWindow text              if ic_ToolWindow not zero (format see below) this is an extension, which was never implemented
//struct DrawerData2           if ic_DrawerData is not zero and ga_UserData is 1 (see below)

// Now a description of the sub-formats:

//a) The text storage method (DefaultTool, ToolWindow and ToolTypes):
struct TEXT {
	ULONG	tx_Size;	//0x00 the size of tx_Text including zero byte (tx_Zero)
	char	tx_Text[];	//0x04 the plain text
};
//	This means the text "Hallo" will be encoded as \00\00\00\06Hallo\00.

//As ToolTypes are an array of texts the encoding is preceeded by another ULONG value containing the number of entries.
//But to make parsing more interessting it is not the number as one would expect, but the number of entries increased by one and multiplied by 4. Thus 10 entries will have	44 as count.

//d) The NewWindow structure used by DrawerData:
struct NEWWINDOW {
	WORD  nw_LeftEdge;		//0x00 left edge distance of window
	WORD  nw_TopEdge;		//0x02 top edge distance of widndow
	WORD  nw_Width;			//0x04 the width of the window (outer width)
	WORD  nw_Height;		//0x06 the height of the window (outer height)
	UBYTE nw_DetailPen;		//0x08 always 255 ???
	UBYTE nw_BlockPen;		//0x09 always 255 ???
	ULONG nw_IDCMPFlags;	//0x0A <undefined>
	ULONG nw_Flags;			//0x0E <undefined>
	APTR  nw_FirstGadget;	//0x12 <undefined>
	APTR  nw_CheckMark;		//0x16 <undefined>
	APTR  nw_Title;			//0x1A <undefined>
	APTR  nw_Screen;		//0x1E <undefined>
	APTR  nw_BitMap;		//0x22 <undefined>
	WORD  nw_MinWidth;		//0x26 <undefined> often 94, minimum window width
	WORD  nw_MinHeight;		//0x28 <undefined> often 65, minimum window height
	UWORD nw_MaxWidth;		//0x2A <undefined> often 0xFFFF, maximum window width
	UWORD nw_MaxHeight;		//0x2C <undefined> often 0xFFFF, maximum window width
	UWORD nw_Type;			//0x2E <undefined>
};

//c) The DrawerData structure:
//This structure is useful for drawers and disks (but there are some icons of other types, which still have these obsolete entries).
struct DRAWERDATA : NEWWINDOW {
	LONG  dd_CurrentX;		//0x30 the current X position of the drawer window contents (this is the relative offset of the drawer drawmap)
	LONG  dd_CurrentY;		//0x34 the current Y position of the drawer window contents
};


//e) The DrawerData2 structure for OS2.x drawers:
struct DRAWERDATA2 {
	enum FLAGS {
		//value 0	handle viewmode like parent drawer current setting (OS1.x compatibility mode)
		VIEW_ICONS	= 1 << 0,	// view icons
		VIEW_ALL	= 1 << 1,	//view all files (bit 0 maybe set or unset with this)
	};
	enum MODES {
		icons_OS1			= 0,                
		icons				= 1,                
		sorted_by_name		= 2,                
		sorted_by_date		= 3,                
		sorted_by_size		= 4,                
		sorted_by_type		= 5,                
	};
	ULONG dd_Flags;		// flags for drawer display
	UWORD dd_ViewModes;	// viewmodes of drawer display
};

//f) And now the last element, the Image structure:
struct IMAGE {
	WORD  im_LeftEdge;		//0x00 always 0 ???
	WORD  im_TopEdge;		//0x00 always 0 ???
	WORD  im_Width;			//0x04 the width of the image
	WORD  im_Height;		//0x06 the height of the image
	WORD  im_Depth;			//0x08 the image bitmap depth
	APTR  im_ImageData;		//0x0A <boolean> always true ???
	UBYTE im_PlanePick;		//0x0E foreground color register index
	UBYTE im_PlaneOnOff;	//0x0F background color register index
	APTR  im_Next;			//0x10 always 0 ???
};

//This is followed by the image data in planar mode. The width of the image is always rounded to next 16bit boundary.

//******************************
//***** NewIcon extension ******
//******************************
//
//As the original format is very limited when using more than the 4 or 8 default colors and also when using different palette sets than the default, there have been ideas how to circumvent this.
//A Shareware author invented NewIcons format, which uses the ToolTypes to store image data, as expanding the original format very surely would haven broken compatibility.
//The NewIcons stuff usually starts with following 2 ToolTypes text (text inside of the "" only):
//" "
//"*** DON'T EDIT THE FOLLOWING LINES!! ***"

//Aftwerwards the image data is encoded as ASCII. The lines for first image always start with "IM1=". If present the second image starts with "IM2=".

//The first line of each image set contains the image information and the palette.
//Example: "IM1=B}}!'��5(3%;ll����T�S9`�"

struct NEWICON {
	UBYTE ni_Transparency;	//'B' = on, 'C' = off
	UBYTE ni_Width;			//0x01 image width + 0x21  - "}" means width 92
	UBYTE ni_Height;		//0x02 image height + 0x21 - "}" means height 92
	UWORD ni_Colors;		//0x03 ASCII coded number of palette entries:
};

//entries are: ((buf[3]-0x21)<<6)+(buf[4]-0x21)
//"!'" means 6 entries
//Afterwards the encoded palette is stored. Each element has 8 bit and colors are stored in order red, green, blue. The encoded format is described below.
//The ni_Width and ni_Height maximum values are 93. The maximum color value is theoretically 255. I have seen images with at least 257 stored colors (but less than 256 used).

//The following lines contain the image data encoded with the same system as the palette.
//The number of bits used to encode an entry depends of the number of colors (6 colors f.e. need 3 bit). The lines have maximum 127 bytes including the "IM1=" or "IM2=" header.
//Thus including the zero byte, the string will be 128 byte.

//En/Decoding algorithm:
//Each byte encodes 7bit (except the RLE bytes)
//Bytes 0x20 to 0x6F represent 7bit value 0x00 to 0x4F
//Bytes 0xA1 to 0xD0 represent 7bit value 0x50 to 0x7F
//Bytes 0xD1 to 0xFF are RLE bytes:
//0xD1 represents  1*7 zero bits,
//0xD2 represents  2*7 zero bits and the last value
//0xFF represents 47*7 zero bits.
//
//Opposite to the original icon format, the NewIcons format uses chunky modus to store the image data.

//The encoding for images and palette stops at the string boundary (127 bytes) with buffer flush (and adding pad bits) and is restarted with next line.

//******************************
//****** OS3.5 extension *******
//******************************

//The OS3.5 format introduces nearly the same information as in NewIcons, but in a more usable format. The tooltypes are no longer misused, but a new data block is appended at the end of the icon file. This data block is in IFF format.
//Currently 3 chunks are used with following data

//1)
struct FACE {
	enum FLAGS {
		FRAMELESS = 1 << 0,	//	icon is frameless
	};
	UBYTE	fc_Header[4];		//0x00 set to "FACE"
	ULONG    fc_Size;			//0x04 size [excluding the first 8 bytes!]
	UBYTE    fc_Width;			//0x08 icon width subtracted by 1
	UBYTE    fc_Height;			//0x09 icon height subtracted by 1
	UBYTE    fc_Flags;			//0x0A flags
	UBYTE    fc_Aspect;			//0x0B image aspect ratio: upper 4 bits x aspect, lower 4 bits y aspect
	UWORD    fc_MaxPalBytes;	//0x0C maximum number of bytes used in image palettes subtracted by 1 (i.e. if palette 1 has 17 and palette 2 has 45 entries, then this is 45)
};

//2) Now 2 chunks of this type may come, where first chunk is image 1 and second chunk is image 2.
struct IMAG {
	enum FLAGS {
		TRANS		= 1 << 0,	//there exists a transparent color
		PALETTE		= 1 << 1,	//a palette data is attached (NOTE, that first image always needs palette data, whereas the second one can reuse the first palette.)
	};
	UBYTE	im_Header[4];		//0x00 set to "IMAG"
	ULONG	im_Size;			//0x04 size [excluding the first 8 bytes!]
	UBYTE	im_Transparent;		//0x08 number of the transparent color
	UBYTE	im_NumColors;		//0x09 number of colors subtracted by 1
	UBYTE	im_Flags;			//0x0A 
	UBYTE	im_ImageFormat;		//0x0B storage format of image data
	UBYTE	im_PalFormat;		//0x0C storage format of palette data (same as above)
	UBYTE	im_Depth;			//0x0D the number of bits used to store a pixel
	UWORD	im_ImageSize;		//0x0E number of bytes used to store image (subtracted by 1)
	UWORD	im_PalSize;			//0x10 number of bytes used to store palette (subtracted by 1)
	//0x12 UBYTE[...]             the image data
	//.... UBYTE[...]             the palette data (if existing)
};

//Now about the run-length compression. This is equal to the run-length method in IFF-ILBM format: The input data is seen as a bit-stream, where each entry has im_Depth bits for image or 8 bits for palette.
//First comes an 8 bit RLE block with following meaning:
//0x00 .. 0x7F copy the next n entries as they are, where n is "RLE-value"+1
//0x80         ignore this, do nothing
//0x81 .. 0xFF produce the next entry n times, where n is 256-"RLE-value"+1
//(if using signed chars n is "RLE-value"+1)

//In uncompressed mode, each byte represents one pixel (even if lower depth is used).
} // namespace amiga