#include "bitmap.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"

namespace iso {

/*
var StringValues = {
	ExposureProgram: {
		0: "Not defined",
		1: "Manual",
		2: "Normal program",
		3: "Aperture priority",
		4: "Shutter priority",
		5: "Creative program",
		6: "Action program",
		7: "Portrait mode",
		8: "Landscape mode"
	},
	MeteringMode: {
		0: "Unknown",
		1: "Average",
		2: "CenterWeightedAverage",
		3: "Spot",
		4: "MultiSpot",
		5: "Pattern",
		6: "Partial",
		255: "Other"
	},
	LightSource: {
		0: "Unknown",
		1: "Daylight",
		2: "Fluorescent",
		3: "Tungsten (incandescent light)",
		4: "Flash",
		9: "Fine weather",
		10: "Cloudy weather",
		11: "Shade",
		12: "Daylight fluorescent (D 5700 - 7100K)",
		13: "Day white fluorescent (N 4600 - 5400K)",
		14: "Cool white fluorescent (W 3900 - 4500K)",
		15: "White fluorescent (WW 3200 - 3700K)",
		17: "Standard light A",
		18: "Standard light B",
		19: "Standard light C",
		20: "D55",
		21: "D65",
		22: "D75",
		23: "D50",
		24: "ISO studio tungsten",
		255: "Other"
	},
	Flash: {
		Flash did not fire = 0x0000,
		Flash fired = 0x0001,
		Strobe return light not detected = 0x0005,
		Strobe return light detected = 0x0007,
		Flash fired, compulsory flash mode = 0x0009,
		Flash fired, compulsory flash mode, return light not detected = 0x000D,
		Flash fired, compulsory flash mode, return light detected = 0x000F,
		Flash did not fire, compulsory flash mode = 0x0010,
		Flash did not fire, auto mode = 0x0018,
		Flash fired, auto mode = 0x0019,
		Flash fired, auto mode, return light not detected = 0x001D,
		Flash fired, auto mode, return light detected = 0x001F,
		No flash function = 0x0020,
		Flash fired, red-eye reduction mode = 0x0041,
		Flash fired, red-eye reduction mode, return light not detected = 0x0045,
		Flash fired, red-eye reduction mode, return light detected = 0x0047,
		Flash fired, compulsory flash mode, red-eye reduction mode = 0x0049,
		Flash fired, compulsory flash mode, red-eye reduction mode, return light not detected = 0x004D,
		Flash fired, compulsory flash mode, red-eye reduction mode, return light detected = 0x004F,
		Flash fired, auto mode, red-eye reduction mode = 0x0059,
		Flash fired, auto mode, return light not detected, red-eye reduction mode = 0x005D,
		0x005F: "Flash fired, auto mode, return light detected, red-eye reduction mode"
	},
	SensingMethod: {
		1: "Not defined",
		2: "One-chip color area sensor",
		3: "Two-chip color area sensor",
		4: "Three-chip color area sensor",
		5: "Color sequential area sensor",
		7: "Trilinear sensor",
		8: "Color sequential linear sensor"
	},
	SceneCaptureType: {
		0: "Standard",
		1: "Landscape",
		2: "Portrait",
		3: "Night scene"
	},
	SceneType: {
		1: "Directly photographed"
	},
	CustomRendered: {
		0: "Normal process",
		1: "Custom process"
	},
	WhiteBalance: {
		0: "Auto white balance",
		1: "Manual white balance"
	},
	GainControl: {
		0: "None",
		1: "Low gain up",
		2: "High gain up",
		3: "Low gain down",
		4: "High gain down"
	},
	Contrast: {
		0: "Normal",
		1: "Soft",
		2: "Hard"
	},
	Saturation: {
		0: "Normal",
		1: "Low saturation",
		2: "High saturation"
	},
	Sharpness: {
		0: "Normal",
		1: "Soft",
		2: "Hard"
	},
	SubjectDistanceRange: {
		0: "Unknown",
		1: "Macro",
		2: "Close view",
		3: "Distant view"
	},
	FileSource: {
		3: "DSC"
	},

	Components: {
		0: "",
		1: "Y",
		2: "Cb",
		3: "Cr",
		4: "R",
		5: "G",
		6: "B"
	}
};
*/

class BitmapFileHandler : public FileHandler {
	const char*			GetCategory() override { return "bitmap"; }
	ISO_ptr<void>		ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<bitmap> bm;
		if (filename(fn.name()).ext() == ".mip") {
			if (id == fn.name())
				id = filename(fn.name()).name();
			if (bm = Read(id, FileInput(fn).me()))
				bm->SetMips(MaxMips(bm->Width() / 2, bm->Height()));
		} else {
			bm = FileHandler::ReadWithFilename(id, fn);
		}
		if (bm.IsType<bitmap>() || bm.IsType<HDRbitmap>())
			SetBitmapFlags(bm.get());
		return bm;
	}
protected:
	static int			WantThumbnail() {
		return ISO::root("variables")["thumbnail"].GetInt();
	}
};
}