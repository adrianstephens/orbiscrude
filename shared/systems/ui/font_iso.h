#ifndef FONT_ISO_H
#define FONT_ISO_H

#include "iso/iso.h"
#include "font.h"
#include "vector_iso.h"

namespace ISO {

#ifndef SCENEGRAPH_H
ISO_DEFCALLBACK(Texture, ISO_ptr_machine<void>);
ISO_DEFCALLBACK(DataBuffer, ISO_ptr_machine<void>);
#endif

ISO_DEFUSERCOMPV(TexGlyphInfo, w, s, k, flags, u, v);
ISO_DEFUSERCOMPV(TexFont, height, baseline, top, spacing, firstchar, numchars, outline, glyph_info, tex);
ISO_DEFUSERCOMPV(SlugGlyphInfo, w, s, k, flags, indices_offset, curves_offset, bands, y, h);
ISO_DEFUSERCOMPV(SlugFont, height, baseline, top, spacing, firstchar, numchars, glyph_buffer, band_buffer, indices_buffer, curve_buffer, palette);
}

#endif	// FONT_ISO_H
