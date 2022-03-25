#include "base/defs.h"
#include "base/bits.h"
#include "base/strings.h"
#include "gpu_address.h"
#include "gnm.h"

#pragma comment(lib, "libSceGnm-debug.lib")
#pragma comment(lib, "libSceGpuAddress-debug.lib")

using namespace iso;
using namespace sce::Gnm;
using namespace sce::GpuAddress;

struct {
	DataFormat	format;
	uint32		size;
} data_formats[] = {
	kDataFormatA8Unorm,				1,
	kDataFormatR16Unorm,			2,
	kDataFormatR8G8B8A8Unorm,		4,
	kDataFormatR16G16B16A16Unorm,	8,
	kDataFormatR32G32B32A32Float,	16,
};

TileMode	tilemodes_1d[] = {
	kTileModeDepth_1dThin,
	kTileModeDisplay_1dThin,
	kTileModeThin_1dThin,
	kTileModeThick_1dThick,
};

TileMode	tilemodes_2d[] = {
	kTileModeDepth_2dThin_64,
	kTileModeDepth_2dThin_128,
	kTileModeDepth_2dThin_256,
	kTileModeDepth_2dThin_512,
	kTileModeDepth_2dThin_1K,
	kTileModeDepth_2dThinPrt_256,
	kTileModeDepth_2dThinPrt_1K,
	kTileModeDisplay_2dThin,
	kTileModeDisplay_2dThinPrt,
	kTileModeThin_2dThin,
	kTileModeThick_2dThick,
	kTileModeThin_2dThinPrt,
	kTileModeThick_2dThickPrt,
	kTileModeThick_2dXThick,
//};
//
//TileMode	tilemodes_3d[] = {
	kTileModeThin_3dThin,
	kTileModeThin_3dThinPrt,
	kTileModeThick_3dThick,
	kTileModeThick_3dXThick,
	kTileModeThick_3dThickPrt,
};
//kTileModeDisplay_LinearAligned
//kTileModeDisplay_ThinPrt
//kTileModeThin_ThinPrt
//kTileModeThick_ThickPrt

bool IsDisplay(TileMode t) {
	switch (t) {
		case kTileModeDisplay_1dThin:
		case kTileModeDisplay_2dThin:
		case kTileModeDisplay_2dThinPrt:
			return true;
		default:
			return false;
	}
}

int test_gpuaddress() {

	uint64				offset, size;
	sce::Gnm::Texture	tex;

	tex.initAsCubemap(256, 256, 9, sce::Gnm::kDataFormatBc1UnormSrgb, sce::Gnm::kTileModeThin_1dThin);

	for (int i = 0; i < 8; i++) {
		computeTextureSurfaceOffsetAndSize(&offset, &size, &tex, i,0);
	}


	TilingParameters	tile;

	tile.m_tileMode				= kTileModeThin_1dThin;
	tile.m_elemFormat			= kDataFormatR8G8B8A8Unorm;
	tile.m_linearWidth			= 16384;
	tile.m_linearHeight			= 16384;
	tile.m_linearDepth			= 16384;
	tile.m_numElementsPerPixel	= 1;
	tile.m_baseTiledPitch		= 0;
	tile.m_mipLevel				= 0;
	tile.m_arraySlice			= 0;
	tile.m_surfaceFlags.m_value	= 0;
	tile.m_surfaceFlags.m_pow2Pad	= 1;
	tile.m_surfaceFlags.m_volume	= 1;

	if (0) {
	tile.m_tileMode		= kTileModeThick_2dThick;
	tile.m_elemFormat	= kDataFormatR16G16B16A16Unorm;
	sce::GpuAddress::Tiler2d tiler(&tile);

	uint64	linear_offset, tiled_offset;
	tiler.getElementByteOffset(&linear_offset, &tiled_offset, 0, 0, 0, 0);
	}

	uint64	linear_offset[14], tiled_offset[14];
	uint8	bit[14];

	for (int i = 0; i < num_elements(tilemodes_1d); i++) {
		ISO_TRACEF("\n\nTilemode %i", tilemodes_1d[i]);
		tile.m_tileMode		= tilemodes_1d[i];
		for (int j = 0; j < num_elements(data_formats); j++) {
			tile.m_elemFormat	= data_formats[j].format;
			uint32	size		= data_formats[j].size;

			if (size > 8 && IsDisplay(tile.m_tileMode))
				continue;

			ISO_TRACEF("\nTexel Size %i", size);

			sce::GpuAddress::Tiler1d tiler(&tile);
			ISO_TRACEF("\nx:");
			for (uint32 x = 0; x < 14; ++x) {
				tiler.getElementByteOffset(&linear_offset[x], &tiled_offset[x], 1<<x, 0, 0);
				ISO_ASSERT(is_pow2(tiled_offset[x]));
				bit[x] = lowest_set_index(tiled_offset[x]/size);
				ISO_TRACEF(" %i", bit[x]);
			}
			ISO_TRACEF("\ny:");
			for (uint32 x = 0; x < 14; ++x) {
				tiler.getElementByteOffset(&linear_offset[x], &tiled_offset[x], 0, 1<<x, 0);
				ISO_ASSERT(is_pow2(tiled_offset[x]));
				bit[x] = lowest_set_index(tiled_offset[x]/size);
				ISO_TRACEF(" %i", bit[x]);
			}
			ISO_TRACEF("\nz:");
			for (uint32 x = 0; x < 14; ++x) {
				tiler.getElementByteOffset(&linear_offset[x], &tiled_offset[x], 0, 0, 1<<x);
				ISO_ASSERT(is_pow2(tiled_offset[x]));
				bit[x] = lowest_set_index(tiled_offset[x]/size);
				ISO_TRACEF(" %i", bit[x]);
			}
		}
	}

	for (int i = 0; i < num_elements(tilemodes_2d); i++) {
		ISO_TRACEF("\n\nTilemode %i", tilemodes_2d[i]);
		tile.m_tileMode		= tilemodes_2d[i];
		for (int j = 0; j < num_elements(data_formats); j++) {
			tile.m_elemFormat	= data_formats[j].format;
			uint32	size		= data_formats[j].size;

			if (size > 8 && IsDisplay(tile.m_tileMode))
				continue;

			ISO_TRACEF("\nTexel Size %i", size);

			sce::GpuAddress::Tiler2d tiler(&tile);
			ISO_TRACEF("\nx:\t\t\ty:\t\t\tz:\t\t\t");
			for (uint32 x = 0; x < 14; ++x) {
				tiler.getElementByteOffset(&linear_offset[x], &tiled_offset[x], 1<<x, 0, 0, 0);
				ISO_TRACEF("\n0x%08I64x", tiled_offset[x]/size);
				tiler.getElementByteOffset(&linear_offset[x], &tiled_offset[x], 0, 1<<x, 0, 0);
				ISO_TRACEF("\t0x%08I64x", tiled_offset[x]/size);
				tiler.getElementByteOffset(&linear_offset[x], &tiled_offset[x], 0, 0, 1<<x, 0);
				ISO_TRACEF("\t0x%08I64x", tiled_offset[x]/size);
			}
		}
	}

	/*
	for (int i = 0; i < num_elements(tilemodes_3d); i++) {
		ISO_TRACEF("\n\nTilemode %i\n", tilemodes_3d[i]);
		tile.m_tileMode		= tilemodes_1d[i];
		sce::GpuAddress::Tiler2d tiler(&tile);
		ISO_TRACEF("x:\n");
		for (uint32 x = 0; x < 14; ++x) {
			tiler.getElementByteOffset(&linear_offset[x], &tiled_offset[x], 1<<x, 0, 0, 0);
			ISO_ASSERT(is_pow2(tiled_offset[x]));
			bit[x] = lowest_set_index(tiled_offset[x]);
			ISO_TRACEF("%i ", bit[x]);
		}
		ISO_TRACEF("\ny:\n");
		for (uint32 x = 0; x < 14; ++x) {
			tiler.getElementByteOffset(&linear_offset[x], &tiled_offset[x], 0, 1<<x, 0, 0);
			ISO_ASSERT(is_pow2(tiled_offset[x]));
			bit[x] = lowest_set_index(tiled_offset[x]);
			ISO_TRACEF("%i ", bit[x]);
		}
	}
	*/
	return 0;
}

static int test = test_gpuaddress();
