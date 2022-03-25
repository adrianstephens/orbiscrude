#include "bitmapfile.h"
#include "comms/zip.h"
#include "container/plist.h"
#include "codec/lzo.h"
#include "utilities.h"

//-----------------------------------------------------------------------------
//	Procreate bitmaps
//-----------------------------------------------------------------------------

using namespace iso;

enum LAYER_TYPE {
	LAYER_LAYER		= 0,
	LAYER_COMPOSITE	= 1,
	LAYER_MASK		= 2,
};

class ProcreateFileHandler : public BitmapFileHandler {
	const char*		GetExt() override { return "procreate"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		ZIPfile		zf;
		for (ZIPreader zip(file); zip.Next(zf); ) {
			if (zf.fn == "Document.archive" && zf.Reader(file).get<bplist::Header>().valid())
				return CHECK_PROBABLE + 1;
		}
		return CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file, bool layers);

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		return Read(id, FileInput(fn).me(), filename(fn.name()).ext() == ".layers" || ISO::root("variables")["layers"].GetInt());
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		return Read(id, file, ISO::root("variables")["layers"].GetInt() != 0);
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override;

} procreate;

void ReadLayer(bitmap *bm, int tilesize, const char *layer_id,  LAYER_TYPE type, ZIPreaderCD2 &zip, istream_ref file) {
	int	width	= bm->Width();
	int	height	= bm->Height();
	int	maxx	= div_round_up(width, tilesize);
	int	maxy	= div_round_up(height, tilesize);

	for (int y = 0; y < maxy; ++y) {
		for (int x = 0; x < maxx; ++x) {
			if (auto *cd = zip.Find(iso::format_string("%s/%i~%i.chunk", layer_id, x, y))) {
				ZIPfile			zf(file, cd);
				malloc_block	input	= zf.Extract(file);
				malloc_block	output(tilesize * tilesize * 4);
				auto			size	= transcode(LZO::decoder(), output, input);
				int				x0		= x * tilesize;
				int				y0		= y * tilesize;


				if (type == LAYER_MASK) {
					copy(
						make_block((Texel<A8>*)input, min(width - x0, tilesize), min(height - y0, tilesize)),
						get(flip_vertical(bm->All()).sub<1>(x0, tilesize).sub<2>(y0, tilesize))
					);

				} else {
					copy(
						make_block((ISO_rgba*)output, min(width - x0, tilesize), min(height - y0, tilesize)),
						flip_vertical(bm->All()).sub<1>(x0, tilesize).sub<2>(y0, tilesize)
					);
				}

			}
		}
	}
}

ISO_ptr<void> ProcreateFileHandler::Read(tag id, istream_ref file, bool layers) {
	ZIPreaderCD2	zip(file);
	if (auto *cd = zip.Find("Document.archive")) {
		ZIPfile			zf(file, cd);
		ISO::Browser2	archive		= bplist_reader(lvalue(iso::memory_reader(zf.Extract(file)))).get_root();
		ISO::Browser2	objects		= archive["$objects"];
		ISO::Browser		settings	= objects[1];

		string			name		= objects[0].GetString();
		int				tilesize	= settings["tileSize"].GetInt();
		int				width, height;

		bool	flippedHorizontally = settings["flippedHorizontally"].GetInt();
		bool	flippedVertically	= settings["flippedVertically"].GetInt();
		int		orientation			= settings["orientation"].GetInt();

		iso::string_scan ss(objects[settings["size"].GetInt()].GetString());
		ss.check("{");
		ss >> width;
		ss.check(",");
		ss >> height;

		if (layers) {
			auto	layers	= objects[settings["layers"].GetInt()]["NS.objects"];
			int		nlayers	= layers.Count();

			ISO_ptr<ISO_openarray<ISO_ptr<bitmap> > > bms(id, nlayers + 1);
			for (int i = 0; i < nlayers; i++) {
				auto			layer	= objects[layers[nlayers - i - 1].GetInt()];
				ISO_ptr<bitmap>	&bm		= (*bms)[i];
				bm.Create(objects[layer["name"].GetInt()].GetString());
				bm->Create(width, height);
				ReadLayer(bm, tilesize, objects[layer["UUID"].GetInt()].GetString(), LAYER_LAYER, zip, file);
			}

			auto			layer	= objects[settings["mask"].GetInt()];
			ISO_ptr<bitmap>	&bm		= (*bms)[nlayers];
			bm.Create("Mask");
			bm->Create(width, height);
			ReadLayer(bm, tilesize, objects[layer["UUID"].GetInt()].GetString(), LAYER_MASK, zip, file);

			return bms;

		} else {
			ISO_ptr<bitmap>	bm(id);
			bm->Create(width, height);

			auto	composite	= objects[settings["composite"].GetInt()];
			ReadLayer(bm, tilesize, objects[composite["UUID"].GetInt()].GetString(), LAYER_COMPOSITE, zip, file);
			return bm;
		}
	}

	return ISO_NULL;
}

typedef simple_vec<double4p, 4>	double4x4p;

void WriteLayerData(ISO_ptr<bitmap> bm, int tilesize, const char *layer_id, LAYER_TYPE type, ZIPwriter &zip) {
	int				width		= bm->Width(), height = bm->Height();
	int				maxx		= div_round_up(width, tilesize);
	int				maxy		= div_round_up(height, tilesize);
	uint32			bpp			= type == LAYER_MASK ? 1 : 4;
	uint32			max_size	= tilesize * tilesize * bpp;
	malloc_block	input(max_size), output(max_size * 2);

	for (int y = 0; y < maxy; ++y) {
		for (int x = 0; x < maxx; ++x) {
			int				x0		= x * tilesize;
			int				y0		= y * tilesize;

			if (type == LAYER_MASK) {
				input.fill(0);
				/*copy(
					get(flip_vertical(bm->All()).sub<1>(x0, tilesize).sub<2>(y0, tilesize)),
					make_block((Texel<A8>*)input, min(width - x0, tilesize), min(height - y0, tilesize))
				);*/

			} else {
				copy(
					get(flip_vertical(bm->All()).sub<1>(x0, tilesize).sub<2>(y0, tilesize)),
					make_block((ISO_rgba*)input, min(width - x0, tilesize), min(height - y0, tilesize))
				);
			}

			uint32	in_size		= min(width - x * tilesize, tilesize) * min(height - y * tilesize, tilesize) * bpp;
			size_t	out_size;
			transcode(LZO::encoder(), output, input, &out_size);
			zip.Write(format_string("%s/%i~%i.chunk", layer_id, x, y), output.slice(intptr_t(0), out_size));
		}
	}
}

bplist::index WriteLayer(ISO_ptr<bitmap> bm, int tilesize, const char *layer_id, LAYER_TYPE type, ZIPwriter &zip, bplist_writer &bp, bplist_writer::_array &objects, bplist_writer::ref &layer_class, bplist::index document) {
	auto	d	= bp.dictionary();
	auto	ix	= objects.add(d);

	double4p	contents_rect;
	double4x4p	transform;

	clear(contents_rect);
	clear(transform);
	transform.x.x = transform.y.y = transform.z.z = transform.w.w = 1;

	const char *name = 0;
	if (type == LAYER_LAYER) {
		if (!(name = bm.ID().get_tag()))
			name = "Layer 1";
	}

	auto	UUID_ix			= objects.add(layer_id);
	auto	name_ix			= name ? objects.add(name) : bplist::index();
	auto	contentsRect_ix	= objects.add(const_memory_block(&contents_rect));
	auto	transform_ix	= objects.add(const_memory_block(&transform));

	d.add(
		"version",				2,
		"blend",				0,
		"bundledMaskPath",		bplist_writer::index(),
		"preserve",				false,
		"contentsRect",			contentsRect_ix,
		"type",					type,
		"bundledImagePath",		bplist_writer::index(),
		"UUID",					UUID_ix,
		"opacity",				1.f,
		"contentsRectValid",	type == LAYER_MASK,
		"hidden",				false,
		"perspectiveAssisted",	false,
		"transform",			transform_ix,
		"$class",				layer_class,
		"name",					name_ix,
		"document",				document
	);

	WriteLayerData(bm, tilesize, layer_id, type, zip);
	return ix;
}

iso_index WriteLayer(ISO_ptr<bitmap> bm, int tilesize, const char *layer_id, LAYER_TYPE type, ZIPwriter &zip, iso_array &objects, iso_ref<iso_dictionary> &layer_class, iso_index document) {
	auto	d	= iso_dictionary();
	auto	ix	= objects.add(d);

	double4p	contents_rect;
	double4x4p	transform;

	clear(contents_rect);
	clear(transform);
	transform.x.x = transform.y.y = transform.z.z = transform.w.w = 1;

	const char *name = 0;
	if (type == LAYER_LAYER) {
		if (!(name = bm.ID().get_tag()))
			name = "Layer 1";
	}

	auto	UUID_ix			= objects.add(layer_id);
	auto	name_ix			= name ? objects.add(name) : iso_index();
	auto	contentsRect_ix	= objects.add(malloc_block(const_memory_block(&contents_rect)));
	auto	transform_ix	= objects.add(malloc_block(const_memory_block(&transform)));

	d.add(
		"version",				2,
		"blend",				0,
		"bundledMaskPath",		iso_index(),
		"preserve",				false,
		"contentsRect",			contentsRect_ix,
		"type",					int(type),
		"bundledImagePath",		iso_index(),
		"UUID",					UUID_ix,
		"opacity",				1.f,
		"contentsRectValid",	type == LAYER_MASK,
		"hidden",				false,
		"perspectiveAssisted",	false,
		"transform",			transform_ix,
		"$class",				layer_class,
		"name",					name_ix,
		"document",				document
	);

	WriteLayerData(bm, tilesize, layer_id, type, zip);
	return ix;
}

GUID RandomGUID() {
	GUID	guid;
	for (int i = 0; i < 4; i++)
		((int*)&guid)[i] = random;
	guid.Data4[0] = (guid.Data4[0] & 0x3f) | 0x80;
	return guid;
}

fixed_string<64> to_string(const GUID &guid) {
	fixed_string<64>	f;
	char				*s = f;
	put_num_base<16>(s, 8, guid.Data1);
	s[8] = '-';
	put_num_base<16>(s + 9, 4, guid.Data2);
	s[13] = '-';
	put_num_base<16>(s + 14, 4, guid.Data3);
	s[18] = '-';
	put_num_base<16>(s + 19, 4, (uint16)*(uint16be*)guid.Data4);
	s[23] = '-';
	put_num_base<16>(s + 24, 12, (uint64)(uint64be&)guid.Data4);
	s[36] = 0;
	return f;
}

bool ProcreateFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	if (p.IsType<ISO_openarray<ISO_ptr<bitmap> > >()) {

	} else if (ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p)) {
		int				width		= bm->Width(), height = bm->Height();
		int				tilesize	= 256;
		float4p			bg			= {0.51358757f, 0.51358757f, 0.51358757f, 1};

		ZIPwriter		zip(file);
		dynamic_memory_writer	mo;
		{
			bplist_writer	bp(mo);

		#if 0
			dynamic_array<bplist::index>	layers;
			auto			d = bp.dictionary();
			bp.set_root(d);

			d.add("$version", 100000ull);

			auto	objects	= bp.array();
			d.add("$objects", objects);

			objects.add(p.ID() ? (const char*)p.ID().get_tag() : "$null");

			auto	top		= bp.dictionary();
			auto	top_ix	= objects.add(top);

			auto	layer_class		= objects.add(bp.dictionary(
				"$classname",	"SilicaLayer",
				"$classes",	bp.array(
					"SilicaLayer",
					"NSObject"
				)
			));

			auto NSMutableArray = objects.add(bp.dictionary(
				"$classname",	"NSMutableArray",
				"$classes",	bp.array(
					"NSMutableArray",
					"NSArray",
					"NSObject"
				)
			));

			layers.push_back(WriteLayer(bm, tilesize, to_string(RandomGUID()), LAYER_LAYER, zip, bp, objects, layer_class, top_ix));
			auto	composite	= WriteLayer(bm, tilesize, to_string(RandomGUID()), LAYER_COMPOSITE, zip, bp, objects, layer_class, top_ix);
			auto	mask		= WriteLayer(bm, tilesize, to_string(RandomGUID()), LAYER_MASK, zip, bp, objects, layer_class, top_ix);

			top.add(
				"SilicaPerspectiveHorizonAngleKey",		0.0,
				"SilicaPerspectiveVisibilityKey",		false,
				"$class",								objects.add(bp.dictionary(
					"$classname",	"SilicaDocument",
					"$classes",		bp.array(
						"SilicaDocument",
						"NSObject"
					)
				)),
				"backgroundColor",						objects.add(const_memory_block(bg)),
				"SilicaDocumentTrackedTimeKey",			893.4618,
				"backgroundHidden",						false,
				"flippedHorizontally",					false,
				"version",								1,
				"layers",								objects.add(bp.dictionary(
					"NS.objects",	layers,
					"$class",  		NSMutableArray
				)),
				"selectedSamplerLayer",					bplist::index(),
				"size",									objects.add(format_string("{%i, %i}", width, height).begin()),
				"mask",									mask,
				"name",									bplist::index(),
				"SilicaDocumentArchiveUnitKey",			0,
				"orientation",							3,
				"SilicaPerspectiveOpacityKey",			1.0,
				"SilicaDocumentArchiveDPIKey",			132.0,
				"SilicaPerspectiveVanishingPointsKey",	objects.add(bp.dictionary(
					"NS.objects",	bp.array(),
					"$class",  		NSMutableArray
				)),
				"SilicaDocumentVideoPurgedKey",			false,
				"SilicaPerspectiveLineThicknessKey",	0.0,
				"SilicaDocumentVideoSegmentInfoKey",	objects.add(bp.dictionary(
					"$class",				objects.add(bp.dictionary(
						"$classname",	"VideoSegmentInfo",
						"$classes",	bp.array(
							"VideoSegmentInfo",
							"NSObject"
						)
					)),

					"framesPerSecond",		30,
					"keyframeInterval",		objects.add(300),
					"sourceOrientation",	3,
					"bitrate",				objects.add(933768.1),
					"qualityPreferenceKey",	objects.add("vqNormal"),
					"frameSize",			objects.add("{1442, 1080}")
				)),
				"composite",							composite,
				"tileSize",								tilesize,
				"selectedLayer",						layers[0],
				"flippedVertically",					false
			);
			d.add("$archiver", "NSKeyedArchiver");
			d.add("$top", bp.dictionary("root", top_ix));
		#elif 0
			dynamic_array<bplist::index>	layers;
			auto			d	= bp.dictionary();
			bp.set_root(d);

			d.add("$version", 100000ull);

			auto	objects		= bp.array();
			d.add("$objects", objects);

			objects.add(p.ID() ? (const char*)p.ID().get_tag() : "$null");

			auto	top			= bp.dictionary();
			auto	top_ix		= objects.add(top);

			auto	size		= objects.add(format_string("{%i, %i}", width, height).begin());
			auto	layers_dic	= bp.dictionary();
			auto	layers_ix	= objects.add(layers_dic);

			bplist_writer::ref	layer_class(objects, bp.dictionary(
				"$classname",	"SilicaLayer",
				"$classes",	bp.array(
					"SilicaLayer",
					"NSObject"
				)
			));

			layers.push_back(WriteLayer(bm, tilesize, "E6E71DE3-7A4E-4A71-A458-1FB4DE93C9B6", LAYER_LAYER, zip, bp, objects, layer_class, top_ix));

			auto NSMutableArray = objects.add(bp.dictionary(
				"$classname",	"NSMutableArray",
				"$classes",	bp.array(
					"NSMutableArray",
					"NSArray",
					"NSObject"
				)
			));

			layers_dic.add(
				"NS.objects",	layers,
				"$class",  		NSMutableArray
			);

			auto	composite	= WriteLayer(bm, tilesize, "4BBE65AD-E759-4460-A5BA-E6C6B4E48711", LAYER_COMPOSITE, zip, bp, objects, layer_class, top_ix);
			auto	mask		= WriteLayer(bm, tilesize, "1BF07114-40F1-4B23-8B96-3531C81777DA", LAYER_MASK, zip, bp, objects, layer_class, top_ix);

			auto	backgroundColor	 = objects.add(const_memory_block(bg));

			auto	SilicaDocumentVideoSegmentInfoKey_dic	= bp.dictionary();
			auto	SilicaDocumentVideoSegmentInfoKey		= objects.add(SilicaDocumentVideoSegmentInfoKey_dic);

			auto	frameSize				= objects.add("{1442, 1080}");
			auto	keyframeInterval		= objects.add(300);
			auto	bitrate					= objects.add(933768.1);
			auto	qualityPreferenceKey	= objects.add("vqNormal");

			SilicaDocumentVideoSegmentInfoKey_dic.add(
				"$class",				objects.add(bp.dictionary(
					"$classname",	"VideoSegmentInfo",
					"$classes",	bp.array(
						"VideoSegmentInfo",
						"NSObject"
					)
				)),

				"framesPerSecond",		30,
				"keyframeInterval",		keyframeInterval,
				"sourceOrientation",	3,
				"bitrate",				bitrate,
				"qualityPreferenceKey",	qualityPreferenceKey,
				"frameSize",			frameSize
			);

			auto SilicaPerspectiveVanishingPointsKey = objects.add(bp.dictionary(
				"NS.objects",	bp.array(),
				"$class",  		NSMutableArray
			));

			top.add(
				"SilicaPerspectiveHorizonAngleKey",		0.0,
				"SilicaPerspectiveVisibilityKey",		false,
				"$class",								objects.add(bp.dictionary(
					"$classname",	"SilicaDocument",
					"$classes",		bp.array(
						"SilicaDocument",
						"NSObject"
					)
				)),
				"backgroundColor",						backgroundColor,
				"SilicaDocumentTrackedTimeKey",			893.4618,
				"backgroundHidden",						false,
				"flippedHorizontally",					false,
				"version",								1,
				"layers",								layers_ix,
				"selectedSamplerLayer",					bplist::index(),
				"size",									size,
				"mask",									mask,
				"name",									bplist::index(),
				"SilicaDocumentArchiveUnitKey",			0,
				"orientation",							3,
				"SilicaPerspectiveOpacityKey",			1.0,
				"SilicaDocumentArchiveDPIKey",			132.0,
				"SilicaPerspectiveVanishingPointsKey",	SilicaPerspectiveVanishingPointsKey,
				"SilicaDocumentVideoPurgedKey",			false,
				"SilicaPerspectiveLineThicknessKey",	0.0,
				"SilicaDocumentVideoSegmentInfoKey",	SilicaDocumentVideoSegmentInfoKey,
				"composite",							composite,
				"tileSize",								tilesize,
				"selectedLayer",						layers[0],
				"flippedVertically",					false
			);
			d.add("$archiver", "NSKeyedArchiver");
			d.add("$top", bp.dictionary("root", top_ix));
		#else
			dynamic_array<iso_index>	layers;
			auto			d = iso_dictionary();
			d.add("$version", 100000ull);

			auto	objects	= iso_array();
			d.add("$objects", objects);

			objects.add(p.ID() ? (const char*)p.ID().get_tag() : "$null");

			auto	top		= iso_dictionary();
			auto	top_ix	= objects.add(top);

			auto	size		= objects.add(format_string("{%i, %i}", width, height).begin());
			auto	layers_dic	= iso_dictionary();
			auto	layers_ix	= objects.add(layers_dic);

			iso_ref<iso_dictionary>		layer_class(objects);
			layer_class->add(
				"$classname",	"SilicaLayer",
				"$classes",	iso_array(
					"SilicaLayer",
					"NSObject"
				)
			);

			layers.push_back(WriteLayer(bm, tilesize, to_string(RandomGUID()), LAYER_LAYER, zip, objects, layer_class, top_ix));

			auto NSMutableArray = objects.add(iso_dictionary(
				"$classname",	"NSMutableArray",
				"$classes",	iso_array(
					"NSMutableArray",
					"NSArray",
					"NSObject"
				)
			));

			layers_dic.add(
				"NS.objects",	layers,
				"$class",  		NSMutableArray
			);

			auto	composite	= WriteLayer(bm, tilesize, to_string(RandomGUID()), LAYER_COMPOSITE, zip, objects, layer_class, top_ix);
			auto	mask		= WriteLayer(bm, tilesize, to_string(RandomGUID()), LAYER_MASK, zip, objects, layer_class, top_ix);

			auto	backgroundColor	 = objects.add(const_memory_block(&bg));

			auto	SilicaDocumentVideoSegmentInfoKey_dic	= iso_dictionary();
			auto	SilicaDocumentVideoSegmentInfoKey		= objects.add(SilicaDocumentVideoSegmentInfoKey_dic);

			auto	frameSize				= objects.add("{1442, 1080}");
			auto	keyframeInterval		= objects.add(300);
			auto	bitrate					= objects.add(933768.1);
			auto	qualityPreferenceKey	= objects.add("vqNormal");

			SilicaDocumentVideoSegmentInfoKey_dic.add(
				"$class",				objects.add(iso_dictionary(
					"$classname",	"VideoSegmentInfo",
					"$classes",	iso_array(
						"VideoSegmentInfo",
						"NSObject"
					)
				)),

				"framesPerSecond",		30,
				"keyframeInterval",		keyframeInterval,
				"sourceOrientation",	3,
				"bitrate",				bitrate,
				"qualityPreferenceKey",	qualityPreferenceKey,
				"frameSize",			frameSize
			);

			auto SilicaPerspectiveVanishingPointsKey = objects.add(iso_dictionary(
				"NS.objects",	iso_array(),
				"$class",  		NSMutableArray
			));

			top.add(
				"SilicaPerspectiveHorizonAngleKey",		0.0,
				"SilicaPerspectiveVisibilityKey",		false,
				"$class",								objects.add(iso_dictionary(
					"$classname",	"SilicaDocument",
					"$classes",		iso_array(
						"SilicaDocument",
						"NSObject"
					)
				)),
				"backgroundColor",						backgroundColor,
				"SilicaDocumentTrackedTimeKey",			893.4618,
				"backgroundHidden",						false,
				"flippedHorizontally",					false,
				"version",								1,
				"layers",								layers_ix,
				"selectedSamplerLayer",					iso_index(),
				"size",									size,
				"mask",									mask,
				"name",									iso_index(),
				"SilicaDocumentArchiveUnitKey",			0,
				"orientation",							3,
				"SilicaPerspectiveOpacityKey",			1.0,
				"SilicaDocumentArchiveDPIKey",			132.0,
				"SilicaPerspectiveVanishingPointsKey",	SilicaPerspectiveVanishingPointsKey,
				"SilicaDocumentVideoPurgedKey",			false,
				"SilicaPerspectiveLineThicknessKey",	0.0,
				"SilicaDocumentVideoSegmentInfoKey",	SilicaDocumentVideoSegmentInfoKey,
				"composite",							composite,
				"tileSize",								tilesize,
				"selectedLayer",						layers[0],
				"flippedVertically",					false
			);
			d.add("$archiver", "NSKeyedArchiver");
			d.add("$top", iso_dictionary("root", top_ix));
			bp.set_max_elements(bp.count_elements(d));
			bp.set_root(bp.add((ISO_ptr_machine<void>)d));
		#endif
		}
		zip.Write("Document.archive", mo);

		if (FileHandler	*png = FileHandler::Get("png")) {
			int	thwidth, thheight;
			if (width > height) {
				thwidth		= 136;
				thheight	= 136 * height / width;
			} else {
				thheight	= 136;
				thwidth		= 136 * width / height;
			}
			ISO_ptr<bitmap>	thumbnail(0);
			thumbnail->Create(thwidth, thheight);
			resample_via<HDRpixel>(thumbnail->All(), bm->All());
			mo.seek(0);
			png->Write(thumbnail, mo);
			zip.Write("QuickLook/Thumbnail.png", mo);
		}
	}

	return false;
}
