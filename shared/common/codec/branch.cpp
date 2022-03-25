#include "branch.h"
#include "base/bits.h"

namespace branch {

inline bool is_x86_MSuint8(uint8 b) { return ((b + 1) & 0xFE) == 0; }

uint32 BCJ_Convert(memory_block mem, uint32 ip, uint32 mask, bool encoding) {
	uint8	*end = (uint8*)mem.end() - 4;
	if (end <= mem)
		return mask;

	ip += 5;
	for (uint8* p = mem;;) {
		uint8* p0 = p;
		while (p < end && (*p & 0xFE) != 0xE8)
			++p;

		size_t d = p - p0;
		mask	 = d > 2 ? 0 : mask >> d;

		if (p >= end)
			return mask;

		if ((mask > 4 || mask == 3 || (mask && is_x86_MSuint8(p[(mask >> 1) + 1]))) || !is_x86_MSuint8(p[4])) {
			mask = (mask >> 1) | 4;
			++p;

		} else {
			uint32 v   = load_packed<uint32le>(p + 1);
			uint32 cur = ip + (uint32)(p - mem);

			if (encoding)
				v += cur;
			else
				v -= cur;

			if (mask) {
				uint32 sh = (mask & 6) << 2;
				if (is_x86_MSuint8(uint8(v >> sh))) {
					v ^= (0x100 << sh) - 1;
					if (encoding)
						v += cur;
					else
						v -= cur;
				}
				mask = 0;
			}
			store_packed<uint32le>(p + 1, sign_extend<24>(v));
			p += 5;
		}
	}
}

void ARM_Convert(memory_block mem, uint32 ip, bool encoding) {
	for (uint8*	p = mem, *end = (uint8*)mem.end() - 4; p < end; p += 4) {
		if (p[3] == 0xEB) {
			uint32	d = ip + uint32(p - mem);
			uint32	v = load_packed<uint32le>(p) << 2;

			if (encoding)
				v += d;
			else
				v -= d;

			store_packed<uint32le>(p, (v >> 2 & 0x00FFFFFF) | 0xEB000000);
		}
	}
}

void ARMT_Convert(memory_block mem, uint32 ip, bool encoding) {
	for (uint8*	p = mem, *end = (uint8*)mem.end() - 4; p < end; p += 2) {
		uint32 b1 = p[1] ^ 8;
		if ((p[3] & b1) >= 0xF8) {
			uint32	d = (ip + uint32(p - mem)) >> 1;
			uint32	v = ((uint32)b1 << 19) + (((uint32)p[-2] << 11)) + p[0] + (((uint32)p[1] & 0x7) << 8);

			if (encoding)
				v += d;
			else
				v -= d;

			p[-2] = uint8(v >> 11);
			p[-1] = uint8(0xF0 | ((v >> 19) & 0x7));
			p[ 0] = uint8(v);
			p[ 1] = uint8(0xF8 | (v >> 8));
			p += 2;
		}
	}
}

void PPC_Convert(memory_block mem, uint32 ip, bool encoding) {
	for (uint8*	p = mem, *end = (uint8*)mem.end() - 4; p < end; p += 4) {
		if ((p[0] & 0xFC) == 0x48 && (p[3] & 3) == 1) {
			uint32	v = load_packed<uint32be>(p);
			uint32	d = ip + uint32(p - mem);

			if (encoding)
				v += d;
			else
				v -= d;

			store_packed<uint32be>(p, (v & 0x03FFFFFF) | 0x48000000);
		}
	}
}

void SPARC_Convert(memory_block mem, uint32 ip, bool encoding) {
	for (uint8*	p = mem, *end = (uint8*)mem.end() - 4; p < end; p += 4) {
		if ((p[0] == 0x40 && (p[1] & 0xC0) == 0) || (p[0] == 0x7F && p[1] >= 0xC0)) {
			uint32	v = load_packed<uint32be>(p) << 2;
			uint32	d = ip + uint32(p - mem);

			if (encoding)
				v += d;
			else
				v -= d;

			store_packed<uint32be>(p, ((((v & 0x01FFFFFF) - 0x01000000) ^ 0xFF000000) >> 2) | 0x40000000);
		}
	}
}

void X86_Convert(memory_block mem, uint32 ip, uint8 Mask, bool encoding) {
	const uint32 FileSize = 0x1000000;
	for (uint8*	p = mem, *end = (uint8*)mem.end(); p < end; ++p) {
		if ((p[0] & Mask) == 0xe8) {
			uint32	v = load_packed<uint32be>(p - 4) << 2;
			uint32	d = ip + uint32(p - mem);

			if (encoding)
				v += d;
			else
				v -= d;

			store_packed<uint32be>(p, v);
		}
	}
}

void Itanium_Convert(memory_block mem, uint32 ip, bool encoding) {
	static const uint8 masks[16] = {4, 4, 6, 6, 0, 0, 7, 7, 4, 4, 0, 0, 4, 4, 0, 0};

	for (uint8*	p = mem, *end = (uint8*)mem.end() - 21; p < end; p += 16) {
		int Byte = p[0] & 0x1f;
		if (Byte >= 0x10) {
			if (uint8 cmd_mask = masks[Byte - 0x10]) {
				for (uint32 I = 0; I <= 2; I++) {
					if (cmd_mask & (1 << I)) {
						uint32 bit_pos = I * 41 + 5;
						if (read_bits((uint32le*)p, bit_pos + 37, 4) == 5) {
							uint32	v = read_bits((uint32le*)p, bit_pos + 13, 20);
							uint32	d = (ip + uint32(p - mem)) >> 4;
							if (encoding)
								v += d;
							else
								v -= d;
							write_bits((uint32le*)p, v & 0xfffff, bit_pos + 13, 20);
						}
					}
				}
			}
		}
	}

}

}  // namespace branch
