#include "lzo.h"

using namespace iso;
using namespace LZO;

static uint32 LZO_compress(const uint8* in, uint32 in_len, uint8 *out, uint32 ti, uint16* dict, size_t *bytes_written) {
	const uint8	*ip		= in + (ti < 4 ? 4 - ti : 0) + 1;
	uint8		*op		= out;
	const uint8	*in_end	= in + in_len;
	const uint8	*ip_end	= in + in_len - MIN_LOOKAHEAD;
	const uint8	*ii		= in;

	for (;;) {
		const uint8	*m_pos;

		for (;;) {
			if (ip >= ip_end) {
				*bytes_written = op - out;
				return uint32(in_end - (ii - ti));
			}

			uint32	dv		= *(uint32*)ip;
			uint32	dindex	= (0x1824429d * dv) >> (32 - D_BITS);
			dict[dindex]	= uint16(ip - in);
			m_pos			= in + dict[dindex];

			if (dv == *(uint32*)m_pos)
				break;

			ip += 1 + ((ip - ii) >> 5);
		}

		ii -= ti;
		ti	= 0;
		if (uint32 t = uint32(ip - ii)) {
			if (t <= 3) {
				op[-2] |= uint8(t);

			} else if (t <= 18) {
				*op++ = uint8(t - 3);

			} else {
				uint32 tt = t - 18;
				*op++ = 0;
				while (tt > 255) {
					tt -= 255;
					*(uint8*)op++ = 0;
				}
				*op++ = uint8(tt);
			}
			memcpy(op, ii, t);
			op += t;
		}

		uint32	m_len	= (uint32)match_len_loose(ip + 4, m_pos + 4, ip_end) + 4;
		uint32	m_off	= uint32(ip - m_pos);

		ip		+= m_len;
		ii		= ip;
		if (m_len <= M2_MAX_LEN && m_off <= M2_MAX_OFFSET) {
			--m_off;
			*op++ = uint8(((m_len - 1) << 5) | ((m_off & 7) << 2));
			*op++ = uint8(m_off >> 3);

		} else if (m_off <= M3_MAX_OFFSET) {
			--m_off;
			if (m_len <= M3_MAX_LEN) {
				*op++ = uint8(M3_MARKER | (m_len - 2));
			} else {
				m_len -= M3_MAX_LEN;
				*op++ = M3_MARKER | 0;
				while (m_len > 255) {
					m_len	-= 255;
					*op++	= 0;
				}
				*op++ = uint8(m_len);
			}
			*op++ = uint8(m_off << 2);
			*op++ = uint8(m_off >> 6);

		} else {
			m_off -= 0x4000;
			if (m_len <= M4_MAX_LEN) {
				*op++ = uint8(M4_MARKER | ((m_off >> 11) & 8) | (m_len - 2));
			} else {
				m_len -= M4_MAX_LEN;
				*op++ = uint8(M4_MARKER | ((m_off >> 11) & 8));
				while (m_len > 255) {
					m_len -= 255;
					*op++ = 0;
				}
				*op++ = uint8(m_len);
			}
			*op++ = uint8(m_off << 2);
			*op++ = uint8(m_off >> 6);
		}
	}
}

const uint8* LZO::encoder::process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
	const uint8*	ip		= src;
	uint8*			op		= dst;
	size_t			src_rem	= src_end - src;
	uint32			t	= 0;

	while (src_rem > 20) {
		size_t	written;
		uint32	n = min(src_rem, 49152);
		t		= LZO_compress(ip, n, op, t, dict, &written);
		ip		+= n;
		op		+= written;
		src_rem	-= n;
	}

	t += uint32(src_rem);
	if (t > 0) {
		if (op == dst && t <= 238)
			*op++ = uint8(17 + t);
		else if (t <= 3)
			op[-2] |= (uint8)t;
		else if (t <= 18)
			*op++ = (uint8)(t - 3);
		else {
			uint32 tt = t - 18;
			*op++ = 0;
			while (tt > 255) {
				tt -= 255;
				*(uint8*)op++ = 0;
			}
			*op++ = (uint8)tt;
		}
		loose_copy<uint8>(op, src_end - t, op + t);
		op += t;
	}
	*op++	= M4_MARKER | 1;
	*op++	= 0;
	*op++	= 0;

	dst		= op;
	return ip;
}

const uint8* LZO::decoder::process(uint8*& dst, uint8* dst_end, const uint8* src, const uint8* src_end, TRANSCODE_FLAGS flags) {
	uint8		*op	= dst;
	const uint8	*ip	= src;
	uint32		t;

	bool	gt_first_literal_run	= false;
	bool	gt_match_done			= false;

	if (*ip > 17) {
		t = (uint32)(*ip++ - 17);
		if (t < 4) {
			do *op++ = *ip++; while (--t > 0);
			t = *ip++;
		} else {
			do *op++ = *ip++; while (--t > 0);
			gt_first_literal_run = true;
		}
	}

	for (;;) {
		if (gt_first_literal_run) {
			gt_first_literal_run = false;
			goto first_literal_run;
		}

		t = *ip++;
		if (t < 16) {
			if (t == 0) {
				while (*ip == 0) {
					t += 255;
					ip++;
				}
				t += (uint32)(15 + *ip++);
			}
			*(uint32*)op = *(uint32*)ip;
			op += 4;
			ip += 4;
			if (--t > 0) {
				while (t--)
					*op++ = *ip++;
				//memcpy(op, ip, t);
				//op += t;
				//ip += t;
			}
		first_literal_run:
			t = *ip++;
			if (t < 16) {
				uint8 *m_pos = op - uint32(1 + 0x0800 + (t >> 2) + (*ip++ << 2));
				*op++ = *m_pos++;
				*op++ = *m_pos++;
				*op++ = *m_pos;
				gt_match_done = true;
			}
		}

		for (;;) {
			if (gt_match_done) {
				gt_match_done = false;

			} else {
				uint8* m_pos;
				if (t >= 64) {
					m_pos	= op - uint32(1 + ((t >> 2) & 7) + (*ip++ << 3));
					t = (t >> 5) - 1;

				} else if (t >= 32) {
					t &= 31;
					if (t == 0) {
						while (*ip == 0) {
							t += 255;
							ip++;
						}
						t += (uint32)(31 + *ip++);
					}
					m_pos	= op - 1 - (*(uint16*)ip >> 2);
					ip		+= 2;

				} else if (t >= 16) {
					m_pos = op - uint32((t & 8) << 11);
					t &= 7;
					if (t == 0) {
						while (*ip == 0) {
							t += 255;
							ip++;
						}
						t += (uint32)(7 + *ip++);
					}
					m_pos	-= *(uint16*)ip >> 2;
					ip		+= 2;

					if (m_pos == op) {
						//finished
						dst	= op;
						return ip;
					}

					m_pos	-= 0x4000;

				} else {
					m_pos	= op - uint32(1 + (t >> 2) + (*ip++ << 2));
					t		= 0;
				}

				t += 2;
				while (t--)
					*op++ = *m_pos++;
				//memcpy(op, m_pos, t);
				//op += t;
			}

			t = ip[-2] & 3;
			if (t == 0)
				break;

			*op++ = *ip++;
			if (t > 1) {
				*op++ = *ip++;
				if (t > 2)
					*op++ = *ip++;
			}
			t = *ip++;
		}
	}

}
