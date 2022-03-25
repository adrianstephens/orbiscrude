#include "bitmap.h"
#include "base/strings.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "PowerVR/Tools/PVRTexTool/Library/Include/PVRTextureUtilities.h"

//-----------------------------------------------------------------------------
//	Power VR bitmaps
//-----------------------------------------------------------------------------

using namespace iso;
using namespace pvrtexture;

class PVRFileHandler : public FileHandler {
	const char*		GetExt() override { return "pvr";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;

	bool					GetLibrary();
public:
	PVRFileHandler()		{ ISO::getdef<bitmap>(); }
} pvr;

bool PVRFileHandler::GetLibrary() {
#if 0//def PLAT_PC
	static HMODULE	h = LoadLibrary("PVRTexLib.dll");
	return !!h;
#else
	return true;
#endif
}

ISO_ptr<void> PVRFileHandler::Read(tag id, istream_ref file) {
	ISO_ptr<bitmap> bm;
	if (GetLibrary()) {
		CPVRTexture		tex((void*)malloc_block::unterminated(file));
		//	static const PixelType PVRStandard16PixelType = PixelType('r','g','b','a',16,16,16,16);
		//	static const PixelType PVRStandard32PixelType = PixelType('r','g','b','a',32,32,32,32);

		if (Transcode(tex, PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8), ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB)) {
			int		width = tex.getWidth(), height = tex.getHeight();
			memcpy(bm.Create(id)->Create(width, height), tex.getDataPtr(), width * height * sizeof(ISO_rgba));
		}
	}
	return bm;
}

bool PVRFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
	if (!bm || !GetLibrary())
		return false;

	int		flags		= bm->Scan();
	int		width		= bm->BaseWidth(), depth = bm->Depth(), height = bm->BaseHeight();
	bool	volume		= bm->IsVolume();
	bool	cube		= bm->IsCube();
	bool	alpha		= !!(flags & BMF_ALPHA);
	bool	intensity	= !!(flags & BMF_GREY);

	CPVRTextureHeader	header(PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8).PixelTypeID, height, width, depth);
	CPVRTexture			tex(header, bm->ScanLine(0));
//	TexelFormat<32, 8,8, 16,8, 24,8, 0,8>::Copy(bm->ScanLine(0), pvrtex.getDataPtr(), npix);

	PixelType	fmt = MGLPT_PVRTC4;
	if (const char *p = ISO::root("variables")["format"].GetString()) {
		static const struct {
			const char *name;
			PixelType	fmt;
		} formats[] = {
			{"PVRTCI_2bpp",		ePVRTPF_PVRTCI_2bpp_RGBA},
			{"PVRTCI_4bpp",		ePVRTPF_PVRTCI_4bpp_RGBA},
			{"PVRTCII_2bpp",	ePVRTPF_PVRTCII_2bpp},
			{"PVRTCII_4bpp",	ePVRTPF_PVRTCII_4bpp},
			{"ETC1",			ePVRTPF_ETC1},
			{"DXT1",			ePVRTPF_DXT1},
			{"DXT2",			ePVRTPF_DXT2},
			{"DXT3",			ePVRTPF_DXT3},
			{"DXT4",			ePVRTPF_DXT4},
			{"DXT5",			ePVRTPF_DXT5},
		};
		for (int i = 0; i < num_elements(formats); i++) {
			if (istr(p) == formats[i].name) {
				fmt = formats[i].fmt;
				break;
			}
		}
	} else {
		if (intensity)
			fmt = alpha ? MGLPT_AI_88 : MGLPT_I_8;
	}

	Transcode(tex, fmt, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB);
	file.write(tex.getHeader().getFileHeader());
	file.writebuff(tex.getDataPtr(), tex.getDataSize());
	return true;
}
