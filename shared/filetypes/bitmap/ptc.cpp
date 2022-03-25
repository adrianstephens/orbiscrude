#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

#include <windows.h>
#include "d3d9.h"

#if 1
#define __XBOXMATH2_H__
typedef __m128 XMVECTOR;
#endif
#include "xgraphics.h"


//-----------------------------------------------------------------------------
//	XBOX 360 texture compression
//-----------------------------------------------------------------------------

using namespace iso;

class PTCFileHandler : public FileHandler {
	const char*		GetExt() override { return "ptc";	}
	const char*		GetDescription() override { return "XBOX 360 PTC texture compression"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
public:
	PTCFileHandler()		{ ISO::getdef<bitmap>(); }
} bmp;

#define D3DFMT_R8G8B8A8 (D3DFORMAT)MAKED3DFMT(GPUTEXTUREFORMAT_8_8_8_8, GPUENDIAN_8IN32, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_RGBA)
#define D3DFMT_LIN_R8G8B8A8 (D3DFORMAT)MAKELINFMT(D3DFMT_R8G8B8A8)

ISO_ptr<void> PTCFileHandler::Read(tag id, istream_ref file) {
	HRESULT			hr;
	unsigned		width, height;
	D3DFORMAT		format;

	malloc_block	mb	= malloc_block::unterminated(file);
	XGGetPTCImageDesc(mb, UINT(mb.length()), &width, &height, &format);

	ISO_ptr<bitmap>	bm(id);
	bm->Create(width, height);

	hr = XGPTCDecompressSurface(
		bm->ScanLine(0),
		width * 4,
		width, height, D3DFMT_LIN_R8G8B8A8,//format,
		NULL, mb, UINT(mb.length())
	);

	return bm;
}

bool PTCFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (bitmap *bm = ISO_conversion::convert<bitmap>(p)) {
		void		*buffer	= NULL;
		unsigned	size;
		int			width	= bm->Width();
		int			height	= bm->Height();
		HRESULT	hr = XGPTCCompressSurface(
			&buffer, &size,
			bm->ScanLine(0),
			width * 4,
			width, height, D3DFMT_LIN_R8G8B8A8,
			NULL,
			100
		);
		file.writebuff(buffer, size);
		XGPTCFreeMemory(buffer);
		return true;
	}
	return false;
}
