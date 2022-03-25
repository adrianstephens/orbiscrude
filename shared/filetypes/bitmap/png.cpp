#include "libpng/png.h"
#include "bitmapfile.h"

using namespace iso;

struct png_struct_holder : holder<png_struct*> {
	png_info		*info_ptr;
	png_byte		*image_data;

	png_struct_holder(png_struct *p) : holder<png_struct*>(p), info_ptr(0), image_data(0)	{}

	~png_struct_holder()	{
		png_destroy_read_struct((png_struct**)this, &info_ptr, NULL);
		iso::free(image_data);
	}
};

class PNGreader {
	istream_ref	file;
	int			state;
	streamptr	offset;
	png_size_t	chunklen;
	crc32		crc;
	bool		CgBI;
	int			idat;

	struct taghead {
		uint32be	len, tag;
	};

	void read(png_struct *png_ptr, png_byte *data, png_size_t length) {
		for (;;) {
			offset	= file.tell();
			switch (state) {
				case 0: {
					file.readbuff(data, length);
					taghead	&head	= *(taghead*)data;
					chunklen		= head.len;
					switch (head.tag) {
						case 'CgBI':
							CgBI	= true;
							idat	= 0;
							file.seek_cur(chunklen + 4);
							continue;
						case 'IDAT':
							if (CgBI && idat++ == 0) {
								head.len = uint32(chunklen += 2);
								state	= 1;
								crc		= const_memory_block(data + 4, 4);
								break;
							}
						default:
							state		= -1;
							crc			= const_memory_block(data + 4, 4);
							break;
					}
					return;
				}

				case -1:
					file.readbuff(data, length);
					chunklen	-= length;
					if (chunklen == 0)
						state = -2;
					crc.writebuff(data, length);
					return;

				case -2:
//					*(uint32be*)data = crc.as<uint32>();
					file.readbuff(data, length);
					state = 0;
					return;

				case 1:
					data[0] = 0x78;
					data[1] = 0x9c;
					file.readbuff(data + 2, length - 2);
					state	= 2;
					chunklen	-= length;
					crc.writebuff(data, length);
					return;

				case 2:
					file.readbuff(data, length);
					chunklen	-= length;
					if (chunklen == 0)
						state = 3;
					crc.writebuff(data, length);
					return;
				case 3:
					file.readbuff(data, length);
					*(uint32be*)data = crc;
					state = 0;
					return;
				default:
					file.readbuff(data, length);
					return;
			}
		}
	}
	static void _read(png_struct *png_ptr, png_byte *data, png_size_t length) {
		((PNGreader*)png_get_io_ptr(png_ptr))->read(png_ptr, data, length);
	}
public:
	PNGreader(png_struct *png_ptr, istream_ref _file) : file(_file), state(0), CgBI(false) {
		png_set_read_fn(png_ptr, this, _read);
	}
};

class PNGFileHandler : public BitmapFileHandler {
	const char*		GetExt() override { return "png";	}
	const char*		GetMIME() override { return "image/png"; }
	int				Check(istream_ref file) override { file.seek(0); return file.get<uint32le>() == 0x474E5089 ? CHECK_PROBABLE : CHECK_DEFINITE_NO; }

#if 0
	png_struct		*png_ptr;
	png_info		*info_ptr;
	png_uint_32		width, height;
	int				bit_depth, color_type;
	png_byte		*image_data;
#endif

//	static void _read_data(png_struct *png_ptr, png_byte *data, png_size_t length) {
//		((istream*)png_get_io_ptr(png_ptr))->readbuff(data, length);
//	}
	static void _write_data(png_struct *png_ptr, png_byte *data, png_size_t length) {
		((writer_intf*)png_get_io_ptr(png_ptr))->writebuff(data, length);
	}
	static void _flush_data(png_struct *png_ptr)	{}

	static void _error_fn(png_struct *png_ptr, const char *error) {
		(throw_accum(error));
	}

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		png_byte		sig[8];
		png_uint_32		width, height;
		int				bit_depth, color_type;

		file.readbuff(sig, 8);
		if (!png_check_sig(sig, 8))
			return ISO_NULL;

		png_struct_holder png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, _error_fn, NULL);
		if (!png_ptr)
			return ISO_NULL;   /* out of memory */

		png_ptr.info_ptr = png_create_info_struct(png_ptr);
		if (!png_ptr.info_ptr)
			return ISO_NULL;   /* out of memory */

//		if (setjmp(png_ptr->jmpbuf)) {
//			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
//			return ISO_NULL;
//		}

		PNGreader	reader(png_ptr, file);
//		png_set_read_fn(png_ptr, &file, _read_data);
		png_set_sig_bytes(png_ptr, 8);		// we already read the 8 signature bytes
		png_read_info(png_ptr, png_ptr.info_ptr);	// read all PNG info up to image data
		png_get_IHDR(png_ptr, png_ptr.info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

		//-----------------------------

//		double  gamma;
		png_uint_32	channels;
		png_size_t	rowbytes;
		png_byte	**row_pointers = NULL;

		ISO_ptr<bitmap> bm(id);
		bm->Create(width, height);

		if (color_type & PNG_COLOR_MASK_PALETTE) {
			png_color	*palette;
			int			num_palette;
			png_get_PLTE(png_ptr, png_ptr.info_ptr, &palette, &num_palette);
			bm->CreateClut(num_palette);
			for (int i = 0; i < num_palette; i++ )
				bm->Clut(i) = ISO_rgba(palette[i].red, palette[i].green, palette[i].blue, 255);
			if (png_get_valid(png_ptr, png_ptr.info_ptr, PNG_INFO_tRNS)) {
				png_byte		*trans;
				int				num_trans;
				png_color_16	*trans_values;
				png_get_tRNS(png_ptr, png_ptr.info_ptr, &trans, &num_trans, &trans_values);
				for (int i = 0; i < num_trans; i++)
					bm->Clut(i).a = trans[i];
				//bm.SetFlag(BMF_CLUTALPHA);
			}
		}

//		if (!(color_type & PNG_COLOR_MASK_COLOR))
//			bm.SetFlag(BMF_INTENSITY);

//		if (color_type & PNG_COLOR_MASK_ALPHA)
//			bm.SetFlag(BMF_ALPHA);

		if (bit_depth < 8)
			png_set_packing(png_ptr);
		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
			png_set_expand_gray_1_2_4_to_8(png_ptr);
//		if (png_get_valid(png_ptr, png_ptr.info_ptr, PNG_INFO_tRNS))
//			png_set_tRNS_to_alpha(png_ptr);
		if (bit_depth == 16)
			png_set_strip_16(png_ptr);

//		if (png_get_gAMA(png_ptr, png_ptr.info_ptr, &gamma))
//			png_set_gamma(png_ptr, display_exponent, gamma);

		png_read_update_info(png_ptr, png_ptr.info_ptr);

		rowbytes = png_get_rowbytes(png_ptr, png_ptr.info_ptr);
		channels = (int)png_get_channels(png_ptr, png_ptr.info_ptr);

		if ((png_ptr.image_data = (png_byte*)iso::malloc(rowbytes*height)) == NULL)
			return ISO_NULL;

		if ((row_pointers = (png_byte**)iso::malloc(height*sizeof(png_byte*))) == NULL)
			return ISO_NULL;

		png_uint_32	i;
		for (i = 0; i < height; i++)
			row_pointers[i] = png_ptr.image_data + i*rowbytes;

		png_read_image(png_ptr, row_pointers);
		png_read_end(png_ptr, NULL);

		for (i = 0; i < height; i++) {
			ISO_rgba	*dest = bm->ScanLine(i);
			png_byte	*srce = row_pointers[i];
			switch(channels) {
				case 1:
					for (unsigned x = 0; x < width; x++, srce++)
						dest[x] = *srce;
					break;
				case 2:
					for (unsigned x = 0; x < width; x++, srce += 2)
						dest[x] = ISO_rgba(srce[0], srce[1]);
					break;
				case 3:
					for (unsigned x = 0; x < width; x++, srce += 3)
						dest[x] = ISO_rgba(srce[0],srce[1],srce[2]);
					break;
				case 4:
#if 0
					for (unsigned x = 0; x < width; x++, srce += 4) {
						uint8	a = srce[3];
						if (a == 0) {
							dest[x] = ISO_rgba(
								srce[0] ? 0xff : 0,
								srce[1] ? 0xff : 0,
								srce[2] ? 0xff : 0,
								a);
						} else {
							dest[x] = ISO_rgba(
								srce[0] < a ? srce[0] * 255 / a : 0xff,
								srce[1] < a ? srce[1] * 255 / a : 0xff,
								srce[2] < a ? srce[2] * 255 / a : 0xff,
								a);
						}
					}
#else
					for (unsigned x = 0; x < width; x++, srce += 4)
						dest[x] = ISO_rgba(srce[0], srce[1], srce[2], srce[3]);
#endif
					break;
			}
		}

		iso::free(row_pointers);
		return bm;
	}

	bool			Write(ISO_ptr<void> p, ostream_ref file) override {
		ISO_ptr<bitmap> bm = ISO_conversion::convert<bitmap>(p);
		if (!bm)
			return false;

		png_uint_32		width= bm->Width(), height= bm->Height();

		png_struct_holder png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, _error_fn, NULL);
		if (!png_ptr)
			return false;	// out of memory

		png_ptr.info_ptr = png_create_info_struct(png_ptr);
		if (!png_ptr.info_ptr)
			return false;	// out of memory

		/* setjmp() must be called in every function that calls a PNG-writing
		 * libpng function, unless an alternate error handler was installed--
		 * but compatible error handlers must either use longjmp() themselves
		 * (as in this program) or exit immediately, so here we go: */

//		if (setjmp(png_ptr->jmpbuf)) {
//			png_destroy_write_struct(&png_ptr, &info_ptr);
//			return false;
//		}


		png_set_write_fn(png_ptr, (void*)&file, _write_data, _flush_data);


		/* set the compression levels--in general, always want to leave filtering
		 * turned on (except for palette images) and allow all of the filters,
		 * which is the default; want 32K zlib window, unless entire image buffer
		 * is 16K or smaller (unknown here)--also the default; usually want max
		 * compression (NOT the default); and remaining compression flags should
		 * be left alone */

		png_set_compression_level(png_ptr, 6);
		/*
		>> this is default for no filtering; Z_FILTERED is default otherwise:
		png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
		>> these are all defaults:
		png_set_compression_mem_level(png_ptr, 8);
		png_set_compression_window_bits(png_ptr, 15);
		png_set_compression_method(png_ptr, 8);
		*/


		/* set the image parameters appropriately */

		int	color_type	= PNG_COLOR_MASK_COLOR;//(!bm.IsIntensity()	? PNG_COLOR_MASK_COLOR		: 0);
		if (bm->IsPaletted())
			color_type |= PNG_COLOR_MASK_PALETTE;
		else if (bm->HasAlpha())
			color_type |= PNG_COLOR_MASK_ALPHA;

		int	interlace_type	= /*flags & BMWF_TWIDDLE ? PNG_INTERLACE_ADAM7 : */PNG_INTERLACE_NONE;
		int	bit_depth		= bm->IsPaletted() && bm->ClutSize() <= 16 ? 4 : 8;

		png_set_IHDR(png_ptr, png_ptr.info_ptr, width, height, bit_depth, color_type, interlace_type,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

		if (color_type & PNG_COLOR_MASK_PALETTE) {
			png_color	palette[256];
			uint32		n = bm->ClutSize();
			for ( int i = 0; i < n; i++ ) {
				ISO_rgba	&entry = bm->Clut(i);
				palette[i].red	= entry.r;
				palette[i].green= entry.g;
				palette[i].blue	= entry.b;
			}
			png_set_PLTE(png_ptr, png_ptr.info_ptr, palette, n);
/*			if (bm.Flags() & BMF_CLUTALPHA) {
				png_byte		trans[256];
				for ( int i = 0; i < n; i++ )
					trans[i] = bm.Clut(i).a;
				png_set_tRNS(png_ptr, png_ptr.info_ptr, trans, n, NULL);
			}
*/
		}

//		if (mainprog_ptr->gamma > 0.0)
//			png_set_gAMA(png_ptr, png_ptr.info_ptr, mainprog_ptr->gamma);
#if 0
		if (mainprog_ptr->have_bg) {   /* we know it's RGBA, not gray+alpha */
			png_color_16  background;
			background.red		= keycolour.red;
			background.green	= keycolour.green;
			background.blue		= keycolour.blue;
			png_set_bKGD(png_ptr, png_ptr.info_ptr, &background);
		}

		if (mainprog_ptr->have_time) {
			png_time  modtime;
			png_convert_from_time_t(&modtime, mainprog_ptr->modtime);
			png_set_tIME(png_ptr, png_ptr.info_ptr, &modtime);
		}

		if (mainprog_ptr->have_text) {
			png_text  text[6];
			int  num_text = 0;

			if (mainprog_ptr->have_text & TEXT_TITLE) {
				text[num_text].compression = PNG_TEXT_COMPRESSION_NONE;
				text[num_text].key = "Title";
				text[num_text].text = mainprog_ptr->title;
				++num_text;
			}
			if (mainprog_ptr->have_text & TEXT_AUTHOR) {
				text[num_text].compression = PNG_TEXT_COMPRESSION_NONE;
				text[num_text].key = "Author";
				text[num_text].text = mainprog_ptr->author;
				++num_text;
			}
			if (mainprog_ptr->have_text & TEXT_DESC) {
				text[num_text].compression = PNG_TEXT_COMPRESSION_NONE;
				text[num_text].key = "Description";
				text[num_text].text = mainprog_ptr->desc;
				++num_text;
			}
			if (mainprog_ptr->have_text & TEXT_COPY) {
				text[num_text].compression = PNG_TEXT_COMPRESSION_NONE;
				text[num_text].key = "Copyright";
				text[num_text].text = mainprog_ptr->copyright;
				++num_text;
			}
			if (mainprog_ptr->have_text & TEXT_EMAIL) {
				text[num_text].compression = PNG_TEXT_COMPRESSION_NONE;
				text[num_text].key = "E-mail";
				text[num_text].text = mainprog_ptr->email;
				++num_text;
			}
			if (mainprog_ptr->have_text & TEXT_URL) {
				text[num_text].compression = PNG_TEXT_COMPRESSION_NONE;
				text[num_text].key = "URL";
				text[num_text].text = mainprog_ptr->url;
				++num_text;
			}
			png_set_text(png_ptr, png_ptr.info_ptr, text, num_text);
		}
#endif

		// write all chunks up to (but not including) first IDAT

		png_write_info(png_ptr, png_ptr.info_ptr);


		/* set up the transformations:  for now, just pack low-bit-depth pixels
		 * into bytes (one, two or four pixels per byte) */

		png_set_packing(png_ptr);
		/*  png_set_shift(png_ptr, &sig_bit);  to scale low-bit-depth values */
//		png_read_update_info(png_ptr, png_ptr.info_ptr);

		png_uint_32	i, rowbytes, channels;
		png_byte	**row_pointers = NULL;

//		rowbytes = png_get_rowbytes(png_ptr, png_ptr.info_ptr);
		channels = (int)png_get_channels(png_ptr, png_ptr.info_ptr);
		rowbytes = channels * width;

		if ((png_ptr.image_data = (png_byte*)iso::malloc(rowbytes * height)) == NULL)
			return NULL;

		if ((row_pointers = (png_byte**)iso::malloc(height * sizeof(png_byte*))) == NULL)
			return NULL;

		for (i = 0; i < height; i++) {
			ISO_rgba	*srce = bm->ScanLine(i);
			png_byte	*dest = row_pointers[i] = png_ptr.image_data + i*rowbytes;
			unsigned int x;
			switch(channels) {
			case 1:
				for (x = 0; x < width; x++, srce++ )
					*dest++ = srce->r;
				break;
			case 2:
				for (x = 0; x < width; x++, srce++ ) {
					*dest++ = srce->r;
					*dest++ = srce->a;
				}
				break;
			case 3:
				for (x = 0; x < width; x++, srce++ ) {
					*dest++ = srce->r;
					*dest++ = srce->g;
					*dest++ = srce->b;
				}
				break;
			case 4:
				for (x = 0; x < width; x++, srce++ ) {
					*dest++ = srce->r;
					*dest++ = srce->g;
					*dest++ = srce->b;
					*dest++ = srce->a;
				}
				break;
			}
		}

		png_write_image(png_ptr, row_pointers);
		png_write_end(png_ptr, NULL);

		iso::free(row_pointers);
		png_destroy_write_struct(&png_ptr, &png_ptr.info_ptr);
		return true;
	}
} png;
