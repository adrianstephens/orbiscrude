#ifndef FONT_ISO_H
#define FONT_ISO_H

#include "iso/iso.h"
#include "font.h"
#include "vector_iso.h"

namespace ISO {

template<typename R, typename T> struct def<_read_as<R,T>> { Type* operator&() { return getdef<R>(); } };

#ifndef SCENEGRAPH_H
ISO_DEFCALLBACK(Texture, ISO_ptr_machine<void>);
#endif

ISO_DEFUSERCOMPV(GlyphInfo, u, v, w, s, k, flags);

ISO_DEFUSERCOMPV(Font, 
	firstchar, numchars, height, baseline, top, spacing, outline,
	glyph_info, tex
);
}

#endif	// FONT_ISO_H
