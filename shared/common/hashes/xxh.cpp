#include "xxh.h"

using namespace iso;

// xxHash - Fast Hash algorithm Copyright (C) 2012-2016, Yann Collet

uint32 XXH32::finalize(uint32 h, const_memory_block data) {
#if 0
	const uint8* p = data;
	switch (data.length() & 15) { /* or switch(end - p) */
		case 12: h = process4(load_packed<uint32le>(p), h); p += 4;
		case  8: h = process4(load_packed<uint32le>(p), h); p += 4;
		case  4: h = process4(load_packed<uint32le>(p), h); p += 4;
			break;

		case 13: h = process4(load_packed<uint32le>(p), h); p += 4;
		case  9: h = process4(load_packed<uint32le>(p), h); p += 4;
		case  5: h = process4(load_packed<uint32le>(p), h); p += 4;
				 h = process1(*p++, h);
			break;
				 
		case 14: h = process4(load_packed<uint32le>(p), h); p += 4;
		case 10: h = process4(load_packed<uint32le>(p), h); p += 4;
		case  6: h = process4(load_packed<uint32le>(p), h); p += 4;
				 h = process1(*p++, h);
				 h = process1(*p++, h);
			break;
				 
		case 15: h = process4(load_packed<uint32le>(p), h); p += 4;
		case 11: h = process4(load_packed<uint32le>(p), h); p += 4;
		case  7: h = process4(load_packed<uint32le>(p), h); p += 4;
		case  3: h = process1(*p++, h);
		case  2: h = process1(*p++, h);
		case  1: h = process1(*p++, h);
		case  0:
			break;
	}
#else
	const uint32le* p4 = data;
	switch (data.length() >> 2) {
		case 3: h = process4(*p4++, h);
		case 2: h = process4(*p4++, h);
		case 1: h = process4(*p4++, h);
		case 0: break;
	}
	const uint8* p1 = (const uint8*)p4;
	switch (data.length() & 3) {
		case 3: h = process1(*p1++, h);
		case 2: h = process1(*p1++, h);
		case 1: h = process1(*p1++, h);
		case 0: break;
	}
#endif
	// avalanche
	h = (h ^ (h >> 15)) * PRIME_2;
	h = (h ^ (h >> 13)) * PRIME_3;
	return h ^ (h >> 16);
}

uint64 XXH64::finalize(uint64 h, const_memory_block data) {
#if 0
	const uint8* p = data;
	switch (data.length() & 31) {
		case 24: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 16: h = process8(load_packed<uint64le>(p), h); p += 8;
		case  8: h = process8(load_packed<uint64le>(p), h); p += 8;
			break;

		case 28: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 20: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 12: h = process8(load_packed<uint64le>(p), h); p += 8;
		case  4: h = process4(load_packed<uint32le>(p), h); p += 4;
			break;

		case 25: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 17: h = process8(load_packed<uint64le>(p), h); p += 8;
		case  9: h = process8(load_packed<uint64le>(p), h); p += 8;
				 h = process1(*p++, h);
			break;

		case 29: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 21: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 13: h = process8(load_packed<uint64le>(p), h); p += 8;
		case  5: h = process4(load_packed<uint32le>(p), h); p += 4;
				 h = process1(*p++, h);
			break;

		case 26: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 18: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 10: h = process8(load_packed<uint64le>(p), h); p += 8;
				 h = process1(*p++, h);
				 h = process1(*p++, h);
			break;

		case 30: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 22: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 14: h = process8(load_packed<uint64le>(p), h); p += 8;
		case  6: h = process4(load_packed<uint32le>(p), h); p += 4;
				 h = process1(*p++, h);
				 h = process1(*p++, h);
			break;

		case 27: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 19: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 11: h = process8(load_packed<uint64le>(p), h); p += 8;
				 h = process1(*p++, h);
				 h = process1(*p++, h);
				 h = process1(*p++, h);
			break;

		case 31: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 23: h = process8(load_packed<uint64le>(p), h); p += 8;
		case 15: h = process8(load_packed<uint64le>(p), h); p += 8;
		case  7: h = process4(load_packed<uint32le>(p), h); p += 4;
		case  3: h = process1(*p++, h);
		case  2: h = process1(*p++, h);
		case  1: h = process1(*p++, h);
		case  0:
			break;
	}
#else
	const uint64le* p8 = data;
	switch (data.length() >> 3) {
		case 3: h = process8(*p8++, h);
		case 2: h = process8(*p8++, h);
		case 1: h = process8(*p8++, h);
		case 0: break;
	}
	const uint32le* p4 = (const uint32le*)p8;
	if (data.length() & 4)
		h = process4(*p4++, h);

	const uint8* p1 = (const uint8*)p4;
	switch (data.length() & 3) {
		case 3: h = process1(*p1++, h);
		case 2: h = process1(*p1++, h);
		case 1: h = process1(*p1++, h);
		case 0: break;
	}
#endif
	//	avalanche
	h = (h ^ (h >> 33)) * PRIME_2;
	h = (h ^ (h >> 29)) * PRIME_3;
	return h ^ (h >> 32);
}
