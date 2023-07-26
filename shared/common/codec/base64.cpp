#include "base64.h"

using namespace iso;

const char	_to_base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnosqrstuvwxyz0123456789+/";

const uint8	_from_base64[256] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0x00
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0x10
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,	//0x20
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff,	//0x30
	0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,	//0x40
	0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,	//0x50
	0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,	//0x60
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,	//0x70
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0x80
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0x90
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0xa0
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0xb0
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0xc0
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0xd0
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0xe0
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	//0xf0
};

const uint8* base64_decoder::process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
	const uint8	*s = src,			*e	= src_end;
	uint8		*d = (uint8*)dst,	*de = d ? dst_end : (uint8*)uintptr_t(-1);

	while (s < e && d <= de - 3) {
		uint32	v;
		uint8	b	= 0;

		while (s < e && (b = _from_base64[*s++]) == 0xff);
		if (s == e)
			break;
		v	= b << 18;

		while (s < e && (b = _from_base64[*s++]) == 0xff);
		v	|= b << 12;

		while (s < e && (b = _from_base64[*s++]) == 0xff);
		v	|= b << 6;


		if (s[-1] == '=') {
			if (dst)
				d[0] = v >> 16;
			d += 1;
			break;
		}

		while (s < e && (b = _from_base64[*s++]) == 0xff);
		v	|= b << 0;

		if (s[-1] == '=') {
			if (dst) {
				d[0] = v >> 16;
				d[1] = v >> 8;
			}
			d += 2;
			break;
		}

		if (dst) {
			d[0] = v >> 16;
			d[1] = v >> 8;
			d[2] = v >> 0;
		}
		d += 3;
	}

	if (s < e && d < de) {
		uint32	v;
		uint8	b	= 0;
		auto	s0	= s;

		while (s < e && (b = _from_base64[*s++]) == 0xff);
		v	= b << 18;

		while (s < e && (b = _from_base64[*s++]) == 0xff);
		v	|= b << 12;

		while (s < e && (b = _from_base64[*s++]) == 0xff);
		v	|= b << 6;

		if (s[-1] == '=') {
			*d++ = v >> 16;
		} else if (d < de - 1 && *s++ == '=') {
			*d++ = v >> 16;
			*d++ = v >> 8;
		} else {
			s = s0;	// put s back to start of this set of chars
		}
	}
	dst = d;
	return s;
}

const uint8* base64_encoder::process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) const {
	uint32	nextline	= linelen / 4;
	size_t	lflen		= strlen(lf);
	size_t	src_size	= src_end - src;
	//size_t	needed		= div_round_up(src_size, 3) * 4 + div_round_up(src_size, linelen / 4 * 3) * lflen;

	if (dst == 0)
		return src + src_size;

	const uint8 *s = src,	*e	= src_end - 2;
	uint8		*d = dst,	*de = dst_end - 4;

	while (s < e && d < de) {
		uint32	v	= (s[0] << 16) | (s[1] << 8) | (s[2] << 0);
		s	+= 3;
		d[0]		= _to_base64[(v >> 18) & 0x3f];
		d[1]		= _to_base64[(v >> 12) & 0x3f];
		d[2]		= _to_base64[(v >>  6) & 0x3f];
		d[3]		= _to_base64[(v >>  0) & 0x3f];
		d	+= 4;
		if (d + lflen < de + 4 && --nextline == 0) {
			nextline = linelen / 4;
			memcpy(d, lf, lflen);
			d	+= lflen;
		}
	}
	if (d < de) {
		switch (s - e) {
			default:
				break;
			case 1: {	//1 more byte
				uint32	v	= (s[0] << 16);
				d[0]		= _to_base64[(v >> 18) & 0x3f];
				d[1]		= _to_base64[(v >> 12) & 0x3f];
				d[2]		= '=';
				d[3]		= '=';
				nextline	= 0;
				s	+= 1;
				d	+= 4;
				break;
			}
			case 0: {	//2 more bytes
				uint32	v	= (s[0] << 16) | (s[1] << 8);
				d[0]		= _to_base64[(v >> 18) & 0x3f];
				d[1]		= _to_base64[(v >> 12) & 0x3f];
				d[2]		= _to_base64[(v >>  6) & 0x3f];
				d[3]		= '=';
				nextline	= 0;
				s	+= 2;
				d	+= 4;
				break;
			}
		}
		/*
		if (d + lflen < de + 4 && nextline != linelen / 4) {
			memcpy(d, lf, lflen);
			d	+= lflen;
		}
		*/
	}
	dst = d;
	return s;
}
