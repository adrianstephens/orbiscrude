#ifndef BASE64_H
#define BASE64_H

#include "codec.h"

namespace iso {
struct base64_decoder : codec_defaults {
	static const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags);
	size_t		estimate_output(const void* src, size_t src_size) const {
		return src_size * 4 / 3;
	}
};

struct base64_encoder : codec_defaults {
	int			linelen;
	const char	*lf;
	base64_encoder(int linelen = 76, const char *lf = "\n") : linelen(linelen), lf(lf) {}
	const uint8*	process(uint8 *&dst, uint8 *dst_end, const uint8 *src, const uint8 *src_end, TRANSCODE_FLAGS flags) const;
	size_t		estimate_output(const void* src, size_t src_size) const {
		return div_round_up(src_size, 3) * 4 + div_round_up(src_size, linelen / 4 * 3) * strlen(lf);
	}
};
}
#endif	// BASE64_H
