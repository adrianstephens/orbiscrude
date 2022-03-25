#include "bitmapfile.h"
#include "openjpeg/openjpeg.h"

using namespace iso;

#define JP2_RFC3745_MAGIC		"\x00\x00\x00\x0c\x6a\x50\x20\x20\x0d\x0a\x87\x0a"
#define JP2_MAGIC				"\x0d\x0a\x87\x0a"
/* position 45: "\xff\x52" */
#define J2K_CODESTREAM_MAGIC	"\xff\x4f\xff\x51"

struct JPG2000stream {
	opj_stream_t	*jstream;

	JPG2000stream(bool input) {
		jstream = opj_stream_default_create(input);
	}
	~JPG2000stream() {
		opj_stream_destroy(jstream);
	}
	operator opj_stream_t*() { return jstream; }
};

struct JPG2000istream : JPG2000stream, istream_offset {
	
	static OPJ_SIZE_T	jread(void *buffer, OPJ_SIZE_T nb_bytes, void *user) {
		JPG2000istream	*me = (JPG2000istream*)user;
		nb_bytes = me->readbuff(buffer, nb_bytes);
		return nb_bytes ? nb_bytes : -1;
	}
	static OPJ_OFF_T	jskip(OPJ_OFF_T nb_bytes, void *user) {
		JPG2000istream	*me = (JPG2000istream*)user;
		me->seek_cur(nb_bytes);
		return nb_bytes;
	}
	static OPJ_BOOL		jseek(OPJ_OFF_T nb_bytes, void *user) {
		JPG2000istream	*me = (JPG2000istream*)user;
		me->seek(nb_bytes);
		return true;
	}
	static void			jfree_user_data(void *user) {
	}
	
	JPG2000istream(istream_ref file) : JPG2000stream(true), istream_offset(file) {
		opj_stream_set_read_function(jstream, jread);
		opj_stream_set_skip_function(jstream, jskip);
		opj_stream_set_seek_function(jstream, jseek);
		opj_stream_set_user_data(jstream, this, jfree_user_data);
		opj_stream_set_user_data_length(jstream, length());
	}
};

struct JPG2000ostream : JPG2000stream, ostream_offset {
	static OPJ_SIZE_T	jwrite(void *buffer, OPJ_SIZE_T nb_bytes, void *user) {
		JPG2000ostream	*me = (JPG2000ostream*)user;
		return me->writebuff(buffer, nb_bytes);
	}
	static OPJ_OFF_T	jskip(OPJ_OFF_T nb_bytes, void *user) {
		JPG2000ostream	*me = (JPG2000ostream*)user;
		me->seek_cur(nb_bytes);
		return nb_bytes;
	}
	static OPJ_BOOL		jseek(OPJ_OFF_T nb_bytes, void *user) {
		JPG2000ostream	*me = (JPG2000ostream*)user;
		me->seek(nb_bytes);
		return true;
	}
	static void			jfree_user_data(void *user) {
	}
	
	JPG2000ostream(ostream_ref file) : JPG2000stream(false), ostream_offset(file) {
		opj_stream_set_write_function(jstream, jwrite);
		opj_stream_set_skip_function(jstream, jskip);
		opj_stream_set_seek_function(jstream, jseek);
		opj_stream_set_user_data(jstream, this, jfree_user_data);
		opj_stream_set_user_data_length(jstream, length());
	}
};

class JPG2000FileHandler : public BitmapFileHandler {
	const char*		GetExt() override { return "jp2";	}

	int				Check(istream_ref file) override {
		uint8	buf[12];
		return file.read(buf) && (
			memcmp(buf, JP2_RFC3745_MAGIC, 12) == 0
		||	memcmp(buf, JP2_MAGIC, 4) == 0
		||	memcmp(buf, J2K_CODESTREAM_MAGIC, 4) == 0
		) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
	
	static void error_callback(const char *msg, void *client_data) {
		fprintf(stdout, "[ERROR] %s", msg);
	}
	static void warning_callback(const char *msg, void *client_data) {
		fprintf(stdout, "[WARNING] %s", msg);
	}
	static void info_callback(const char *msg, void *client_data) {
		fprintf(stdout, "[INFO] %s", msg);
	}

public:
	JPG2000FileHandler()		{ ISO::getdef<bitmap>(); }
} jp2;

ISO_ptr<void> JPG2000FileHandler::Read(tag id, istream_ref file) {
	JPG2000istream		stream(file);
	OPJ_CODEC_FORMAT	fmt			= OPJ_CODEC_JP2;
	opj_codec_t			*l_codec	= opj_create_decompress(fmt);
	opj_image_t			*image		= 0;
	opj_dparameters_t	params;

	// catch events using our callbacks and give a local context
	opj_set_info_handler(l_codec, info_callback, this);
	opj_set_warning_handler(l_codec, warning_callback, this);
	opj_set_error_handler(l_codec, error_callback, this);

	// Setup the decoder decoding parameters using default parameters
	opj_set_default_decoder_parameters(&params);
	if (!opj_setup_decoder(l_codec, &params)) {
		opj_destroy_codec(l_codec);
		return ISO_NULL;
	}

	// Read the main header of the codestream and if necessary the JP2 boxes
	if (!opj_read_header(stream, l_codec, &image)){
		opj_destroy_codec(l_codec);
		opj_image_destroy(image);
		return ISO_NULL;
	}

	// Get the decoded image
	if (!(opj_decode(l_codec, stream, image) && opj_end_decompress(l_codec, stream))) {
		opj_destroy_codec(l_codec);
		opj_image_destroy(image);
		return ISO_NULL;
	}

	if (image->color_space != OPJ_CLRSPC_SYCC && image->numcomps == 3 && image->comps[0].dx == image->comps[0].dy && image->comps[1].dx != 1)
		image->color_space = OPJ_CLRSPC_SYCC;
	else if (image->numcomps <= 2)
		image->color_space = OPJ_CLRSPC_GRAY;
	
	int	width = image->x1 - image->x0, height = image->y1 - image->y0;
	
	ISO_ptr<bitmap>	bm(id);
	bm->Create(width, height);
	fill(bm->All(), ISO_rgba(0,0,0,255));
	
	switch (image->color_space) {
		case OPJ_CLRSPC_SRGB: {
			for (int c = 0; c < image->numcomps; c++) {
				auto		&comp	= image->comps[c];
				uint8		*d		= &bm->ScanLine(0)->r + c;
				const int32	*s		= comp.data;
				for (int n = width * height; n--; d += 4, s++)
					*d = *s;
			}
			break;
		}
	}
	
	// free remaining structures
	opj_destroy_codec(l_codec);
	opj_image_destroy(image);

	return bm;
}

bool JPG2000FileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
	if (!bm)
		return false;
	
	bm->Unpalette();

	int		width		= bm->Width(), height = bm->Height();
	int		numcomps	= (bm->IsIntensity() ? 1 : 3) + bm->HasAlpha();
	
	// create the image
	opj_image_cmptparm_t	comp[4];
	clear(comp);
	for (int i = 0; i < numcomps; i++) {
		comp[i].prec = 8;
		comp[i].bpp  = 8;
		comp[i].sgnd = 0;
		comp[i].dx   = 1;
		comp[i].dy   = 1;
		comp[i].w    = width;
		comp[i].h    = height;
	}

	opj_image_t	*image = opj_image_create(numcomps, comp, numcomps == 1 ? OPJ_CLRSPC_GRAY : OPJ_CLRSPC_SRGB);
	if (!image)
		return false;

	if (bm->HasAlpha())
		image->comps[numcomps - 1].alpha = 1;
	
	// set image offset and reference grid
	image->x0 = 0;
	image->y0 = 0;
	image->x1 =	width;
	image->y1 = height;
	
	for (int c = 0; c < image->numcomps; c++) {
		auto		&comp	= image->comps[c];
		const uint8	*s		= &bm->ScanLine(0)->r + c;
		int32		*d		= comp.data;
		for (int n = width * height; n--; s += 4, d++)
			*d = *s;
	}
	
	JPG2000ostream		stream(file);
	OPJ_CODEC_FORMAT	fmt			= OPJ_CODEC_JP2;
	opj_codec_t			*l_codec	= opj_create_compress(fmt);
	opj_cparameters_t	params;

	// catch events using our callbacks and give a local context
	opj_set_info_handler(l_codec, info_callback, this);
	opj_set_warning_handler(l_codec, warning_callback, this);
	opj_set_error_handler(l_codec, error_callback, this);

	// Setup the decoder encoding parameters using default parameters
	opj_set_default_encoder_parameters(&params);
	
	// if no rate entered, lossless by default
	if (params.tcp_numlayers == 0) {
		params.tcp_rates[0] = 0;
		params.tcp_numlayers++;
		params.cp_disto_alloc = 1;
	}
	
	if (!opj_setup_encoder(l_codec, &params, image)) {
		opj_destroy_codec(l_codec);
		return false;
	}
	
	if (!opj_start_compress(l_codec, image, stream)
	||	!opj_encode(l_codec, stream)
	||	!opj_end_compress(l_codec, stream)
	) {
		opj_destroy_codec(l_codec);
		opj_image_destroy(image);
		return false;
	}

	// free remaining structures
	opj_destroy_codec(l_codec);
	opj_image_destroy(image);

	return true;
}
