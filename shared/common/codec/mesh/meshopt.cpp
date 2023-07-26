#include "meshopt.h"
#include "comms/leb128.h"
#include "stream.h"
#include "utilities.h"

namespace meshopt {

//-----------------------------------------------------------------------------
// index compression
//-----------------------------------------------------------------------------
// This work is based on:
// Fabian Giesen. Simple lossless index buffer compression & follow-up. 2013
// Conor Stokes. Vertex Cache Optimised Index Buffer Compression. 2014

const uint8 kIndexHeader	= 0xe0;
const uint8 kSequenceHeader = 0xd0;

struct EdgeFifo {
	uint32	fifo[16][2] = {{-1,-1}};
	uint32	offset	= 0;
	void push(uint32 a, uint32 b) {
		fifo[offset][0] = a;
		fifo[offset][1] = b;
		offset			= (offset + 1) & 15;
	}
	auto& operator[](int i) {
		return fifo[(offset - 1 - i) & 15];
	}
	int get(uint32 a, uint32 b, uint32 c) {
		for (int i = 0; i < 16; ++i) {
			size_t index	= (offset - 1 - i) & 15;
			uint32 e0		= fifo[index][0];
			uint32 e1		= fifo[index][1];

			if (e0 == a && e1 == b)
				return (i << 2) | 0;
			if (e0 == b && e1 == c)
				return (i << 2) | 1;
			if (e0 == c && e1 == a)
				return (i << 2) | 2;
		}

		return -1;
	}
};

struct VertexFifo {
	uint32	fifo[16] = {-1};
	uint32	offset	= 0;
	void push(uint32 v, int cond = 1) {
		fifo[offset] = v;
		offset		 = (offset + cond) & 15;
	}
	int get(uint32 v) {
		for (int i = 0; i < 16; ++i) {
			if (fifo[(offset - 1 - i) & 15] == v)
				return i;
		}
		return -1;
	}
	uint32 operator[](int i) {
		return fifo[(offset - i) & 15];
	}
	void reset() {
		memset(fifo, -1, sizeof(fifo));
	}
};

static const uint32 kTriangleIndexOrder[] = {0, 1, 2, 0, 1, 2};

static const uint8 kCodeAuxEncodingTable[16] = {
	0x00, 0x76, 0x87, 0x56, 0x67, 0x78, 0xa9, 0x86, 0x65, 0x89, 0x68, 0x98, 0x01, 0x69,
	0, 0, // last two entries aren't used for encoding
};

static void encodeIndex(ostream_ref file, uint32 index, uint32 last) {
	uint32 d = index - last;
	uint32 v = (d << 1) ^ (int(d) >> 31);
	write_leb128(file, v);
}

static uint32 decodeIndex(istream_ref file, uint32 last) {
	uint32 v = get_leb128<uint32>(file);
	uint32 d = (v >> 1) ^ -int(v & 1);
	return last + d;
}

static int getCodeAuxIndex(uint8 v, const uint8* table) {
	for (int i = 0; i < 16; ++i)
		if (table[i] == v)
			return i;

	return -1;
}

size_t encodeIndexBuffer(memory_block buffer, const uint32* indices, size_t count, int version) {
	ISO_ASSERT(count % 3 == 0);

	((uint8*)buffer)[0] = kIndexHeader | version;

	EdgeFifo	edgefifo;
	VertexFifo	vertexfifo;

	uint32	next	= 0;
	uint32	last	= 0;
	uint8*	code	= buffer + 1;
	int		fecmax	= version >= 1 ? 13 : 15;
	memory_writer	file(buffer + 1 + count / 3);

	// use static encoding table; it's possible to pack the result and then build an optimal table and repack
	// for now we keep it simple and use the table that has been generated based on symbol frequency on a training mesh set
	const uint8* codeaux_table = kCodeAuxEncodingTable;

	for (size_t i = 0; i < count; i += 3) {
		int fer = edgefifo.get(indices[i + 0], indices[i + 1], indices[i + 2]);

		if (fer >= 0 && (fer >> 2) < 15) {
			auto order = kTriangleIndexOrder + (fer & 3);

			uint32 a = indices[i + order[0]], b = indices[i + order[1]], c = indices[i + order[2]];

			// encode edge index and vertex fifo index, next or free index
			int	fe	= fer >> 2;
			int	fc	= vertexfifo.get(c);
			int	fec	= fc >= 1 && fc < fecmax ? fc : c == next ? (next++, 0) : 15;

			if (fec == 15 && version >= 1) {
				// encode last-1 and last+1 to optimize strip-like sequences
				if (c + 1 == last)
					fec = 13, last = c;
				if (c == last + 1)
					fec = 14, last = c;
			}

			*code++ = (uint8)((fe << 4) | fec);

			// note that we need to update the last index since free indices are delta-encoded
			if (fec == 15)
				encodeIndex(file, c, last), last = c;

			// we only need to push third vertex since first two are likely already in the vertex fifo
			if (fec == 0 || fec >= fecmax)
				vertexfifo.push(c);

			// we only need to push two new edges to edge fifo since the third one is already there
			edgefifo.push(c, b);
			edgefifo.push(a, c);

		} else {
			int		rot		= indices[i + 1] == next ? 1 : indices[i + 2] == next ? 2 : 0;
			auto	order	= kTriangleIndexOrder + rot;
			uint32	a		= indices[i + order[0]], b = indices[i + order[1]], c = indices[i + order[2]];

			// if a/b/c are 0/1/2, we emit a reset code
			bool reset = a == 0 && b == 1 && c == 2 && next > 0 && version >= 1;
			if (reset) {
				next  = 0;
				// reset vertex fifo to make sure we don't accidentally reference vertices from that in the future
				// this makes sure next continues to get incremented instead of being stuck
				vertexfifo.reset();
			}

			int	fb	= vertexfifo.get(b);
			int	fc	= vertexfifo.get(c);

			// after rotation, a is almost always equal to next, so we don't waste bits on FIFO encoding for a
			int fea	= a == next ? (next++, 0) : 15;
			int feb	= fb >= 0 && fb < 14 ? fb + 1 : b == next ? (next++, 0) : 15;
			int fec	= fc >= 0 && fc < 14 ? fc + 1 : c == next ? (next++, 0) : 15;

			// we encode feb & fec in 4 bits using a table if possible, and as a full byte otherwise
			uint8	codeaux			= (uint8)((feb << 4) | fec);
			int		codeauxindex	= getCodeAuxIndex(codeaux, codeaux_table);

			// <14 encodes an index into codeaux table, 14 encodes fea=0, 15 encodes fea=15
			if (fea == 0 && codeauxindex >= 0 && codeauxindex < 14 && !reset) {
				*code++ = (uint8)((15 << 4) | codeauxindex);
			} else {
				*code++ = (uint8)((15 << 4) | 14 | fea);
				file.putc(codeaux);
			}

			// note that we need to update the last index since free indices are delta-encoded
			if (fea == 15)
				encodeIndex(file, a, last), last = a;

			if (feb == 15)
				encodeIndex(file, b, last), last = b;

			if (fec == 15)
				encodeIndex(file, c, last), last = c;

			// only push vertices that weren't already in fifo
			if (fea == 0 || fea == 15)
				vertexfifo.push(a);

			if (feb == 0 || feb == 15)
				vertexfifo.push(b);

			if (fec == 0 || fec == 15)
				vertexfifo.push(c);

			// all three edges aren't in the fifo; pushing all of them is important so that we can match them for later triangles
			edgefifo.push(b, a);
			edgefifo.push(c, b);
			edgefifo.push(a, c);
		}
	}

	// add codeaux encoding table to the end of the stream; this is used for decoding codeaux *and* as padding
	// we need padding for decoding to be able to assume that each triangle is encoded as <= 16 bytes of extra data
	// this is enough space for aux byte + 5 bytes per varint index which is the absolute worst case for any input
	for (size_t i = 0; i < 16; ++i) {
		// decoder assumes that table entries never refer to separately encoded indices
		ISO_ASSERT((codeaux_table[i] & 0xf) != 0xf && (codeaux_table[i] >> 4) != 0xf);
		file.putc(codeaux_table[i]);
	}

	// since we encode restarts as codeaux without a table reference, we need to make sure 00 is encoded as a table reference
	ISO_ASSERT(codeaux_table[0] == 0);

	return file.tell();
}

bool decodeIndexBuffer(const_memory_block buffer, uint32* destination, size_t count) {
	ISO_ASSERT(count % 3 == 0);
	
	uint8	h = ((const uint8*)buffer)[0];
	if ((h & 0xf0) != kIndexHeader)
		return false;

	int version = h & 0x0f;
	if (version > 1)
		return false;

	EdgeFifo	edgefifo;
	VertexFifo	vertexfifo;

	uint32	next	= 0;
	uint32	last	= 0;
	int		fecmax	= version >= 1 ? 13 : 15;

	// since we store 16-byte codeaux table at the end, triangle data has to begin before data_safe_end
	const uint8*	code			= buffer + 1;
	const uint8*	codeaux_table	= buffer.end() - 16;
	memory_reader	file(buffer + 1 + count / 3);

	for (size_t i = 0; i < count; i += 3) {
		uint8 codetri = *code++;

		if (codetri < 0xf0) {
			int		fe	= codetri >> 4;
			uint32	a	= edgefifo[fe][0];
			uint32	b	= edgefifo[fe][1];
			int		fec	= codetri & 15;

			uint32	c = fec < fecmax
				? (fec == 0 ? next++ : vertexfifo[fec])
				: (last = fec != 15 ? last + (fec - (fec ^ 3)) : decodeIndex(file, last));

			destination[i + 0] = a;
			destination[i + 1] = b;
			destination[i + 2] = c;

			vertexfifo.push(c, fec == 0 || fec >= fecmax);
			edgefifo.push(c, b);
			edgefifo.push(a, c);

		} else if (codetri < 0xfe) {
			// fast path: read codeaux from the table
			uint8	codeaux = codeaux_table[codetri & 15];
			// note: table can't contain feb/fec=15
			int		feb	= codeaux >> 4;
			int		fec	= codeaux & 15;

			// increment next for all three vertices before decoding indices to match encoder behavior
			uint32	a	= next++;
			uint32	b	= feb == 0 ? next++ : vertexfifo[feb - 1];
			uint32	c	= fec == 0 ? next++ : vertexfifo[fec - 1];

			destination[i + 0] = a;
			destination[i + 1] = b;
			destination[i + 2] = c;

			vertexfifo.push(a);
			vertexfifo.push(b, feb == 0);
			vertexfifo.push(c, fec == 0);
			edgefifo.push(b, a);
			edgefifo.push(c, b);
			edgefifo.push(a, c);

		} else {
			// slow path: read a full byte for codeaux instead of using a table lookup
			uint8	codeaux = file.getc();
			int		fea = codetri == 0xfe ? 0 : 15;
			int		feb = codeaux >> 4;
			int		fec = codeaux & 15;

			// reset: codeaux is 0 but encoded as not-a-table
			if (codeaux == 0)
				next = 0;

			// increment next for all three vertices before decoding indices to matchs encoder behavior
			uint32	a	= fea == 0 ? next++ : 0;
			uint32	b	= feb == 0 ? next++ : vertexfifo[feb - 1];
			uint32	c	= fec == 0 ? next++ : vertexfifo[fec - 1];

			// need to update the last index since free indices are delta-encoded
			if (fea == 15)
				last = a = decodeIndex(file, last);

			if (feb == 15)
				last = b = decodeIndex(file, last);

			if (fec == 15)
				last = c = decodeIndex(file, last);

			destination[i + 0] = a;
			destination[i + 1] = b;
			destination[i + 2] = c;

			vertexfifo.push(a);
			vertexfifo.push(b, feb == 0 | feb == 15);
			vertexfifo.push(c, fec == 0 | fec == 15);
			edgefifo.push(b, a);
			edgefifo.push(c, b);
			edgefifo.push(a, c);
		}
	}

	return true;
}

size_t encodeIndexSequence(memory_block buffer, const uint32* indices, size_t count, int version) {
	memory_writer	file(buffer);
	file.putc(kSequenceHeader | version);

	uint32 last[2] = {};
	uint32 current = 0;

	for (size_t i = 0; i < count; ++i) {
		uint32 index = indices[i];

		// this is a heuristic that switches between baselines when the delta grows too large
		// we want the encoded delta to fit into one byte (7 bits), but 2 bits are used for sign and baseline index
		// for now we immediately switch the baseline when delta grows too large - this can be adjusted arbitrarily
		int cd = int(index - last[current]);
		current ^= abs(cd) >= 30;

		// encode delta from the last index
		uint32 d = index - last[current];
		uint32 v = (d << 1) ^ (int(d) >> 31);

		// note: low bit encodes the index of the last baseline which will be used for reconstruction
		write_leb128(file, (v << 1) | current);

		// update last for the next iteration that uses it
		last[current] = index;
	}

	file.write(uint32(0));
	return file.tell();
}

bool decodeIndexSequence(const_memory_block buffer, uint32* destination, size_t count) {
	memory_reader	file(buffer);
	uint8	h = file.getc();
	if ((h & 0xf0) != kSequenceHeader)
		return false;

	int version = h & 0x0f;
	if (version > 1)
		return false;

	uint32 last[2] = {};

	for (size_t i = 0; i < count; ++i) {
		uint32	v = get_leb128<uint32>(file);

		// decode the index of the last baseline
		uint32	current = v & 1;
		v >>= 1;

		// reconstruct index as a delta
		uint32	d		= (v >> 1) ^ -int(v & 1);
		uint32	index	= last[current] + d;

		// update last for the next iteration that uses it
		destination[i] = last[current] = index;
	}
	return true;
}

malloc_block compressIndexStream(const void *data, size_t count, size_t max_index, size_t stride, int version) {
	ISO_ASSERT(stride == 2 || stride == 4);
	ISO_ASSERT(count % 3 == 0);

	auto	vertex_bits		= log2_ceil(max_index);			// number of bits required for each index
	auto	vertex_groups	= (vertex_bits + 1 + 6) / 7;	// worst-case encoding is 2 header bytes + 3 leb128 index deltas
	auto	size			= 1 + (count / 3) * (2 + 3 * vertex_groups) + 16;
	malloc_block	buffer(size);

	size = stride == 2
		? encodeIndexBuffer(buffer, temp_array<uint32>(make_range_n((uint16*)data, count)), count, version)
		: encodeIndexBuffer(buffer, (const uint32*)data, count, version);
	return buffer.resize(size);
}

malloc_block compressIndexSequence(const void *data, size_t count, size_t max_index, size_t stride, int version) {
	ISO_ASSERT(stride == 2 || stride == 4);
	
	auto	vertex_bits		= log2_ceil(max_index);				// number of bits required for each index
	auto	vertex_groups	= (vertex_bits + 1 + 1 + 6) / 7;	// worst-case encoding is 1 leb28 index delta for a K bit value and an extra bit
	auto	size			= count * vertex_groups + 4;
	malloc_block	buffer(size);

	size	= stride == 2
		? encodeIndexSequence(buffer, temp_array<uint32>(make_range_n((uint16*)data, count)), count, version)
		: encodeIndexSequence(buffer, (const uint32*)data, count, version);

	return buffer.resize(size);
}

//-----------------------------------------------------------------------------
// vertex compression
//-----------------------------------------------------------------------------

const uint8 kVertexHeader = 0xa0;

const size_t kVertexBlockSizeBytes = 8192;
const size_t kVertexBlockMaxSize   = 256;
const size_t kByteGroupSize		   = 16;
const size_t kByteGroupDecodeLimit = 24;
const size_t kTailMaxSize		   = 32;

static size_t getVertexBlockSize(size_t vertex_size) {
	return min((kVertexBlockSizeBytes / vertex_size) & ~(kByteGroupSize - 1), kVertexBlockMaxSize);
}

inline uint8 zigzag8(uint8 v)	{ return ((int8)(v) >> 7) ^ (v << 1); }
inline uint8 unzigzag8(uint8 v) { return -(v & 1) ^ (v >> 1); }

static uint8* encodeBytesGroup(uint8* data, const uint8* buffer, int bitslog2) {
	ISO_ASSERT(bitslog2 >= 0 && bitslog2 <= 3);

	if (bitslog2 == 0)
		return data;

	if (bitslog2 == 3) {
		memcpy(data, buffer, kByteGroupSize);
		return data + kByteGroupSize;
	}

	size_t byte_size = 8 >> bitslog2;

	// fixed portion: bits bits for each value
	// variable portion: full byte for each out-of-range value (using 1...1 as sentinel)
	int		bits		= 1 << bitslog2;
	uint8	sentinel	= (1 << bits) - 1;
	for (size_t i = 0; i < kByteGroupSize; i += byte_size) {
		uint8 byte = 0;
		for (size_t k = 0; k < byte_size; ++k)
			byte = (byte << bits) | min(buffer[i + k], sentinel);
		*data++ = byte;
	}

	for (size_t i = 0; i < kByteGroupSize; ++i) {
		if (buffer[i] >= sentinel)
			*data++ = buffer[i];
	}

	return data;
}

static int count_greater(const uint8* buffer, uint8 max) {
	int	size = 0;
	for (int i = 0; i < kByteGroupSize; ++i)
		size += buffer[i] >= max;
	return size;
}

static uint8* encodeBytes(uint8* data, const uint8* buffer, size_t buffer_size) {
	ISO_ASSERT(buffer_size % kByteGroupSize == 0);

	uint8* header		= data;
	size_t header_size	= (buffer_size / kByteGroupSize + 3) / 4;	// round number of groups to 4 to get number of header bytes
	data += header_size;
	memset(header, 0, header_size);

	for (size_t i = 0; i < buffer_size; i += kByteGroupSize) {
		auto	p	= buffer + i;
		int		best_bitslog2	= 0;
		size_t	best_size		= 0;

		if (count_greater(p, 0)) {
			size_t	size1		= kByteGroupSize / 4 + count_greater(p, 3);
			if (size1 < best_size) {
				best_bitslog2	= 1;
				best_size		= size1;
			} else {
				best_bitslog2	= 3;
				best_size		= kByteGroupSize;
			}
			
			if (kByteGroupSize / 2 + count_greater(p, 15) < best_size)
				best_bitslog2	= 2;
		}
		
		size_t	header_offset	= i / kByteGroupSize;
		header[header_offset / 4] |= best_bitslog2 << ((header_offset % 4) * 2);
		data = encodeBytesGroup(data, p, best_bitslog2);
	}

	return data;
}

static uint8* encodeVertexBlock(uint8* data, const uint8* vertex_data, size_t count, size_t vertex_size, uint8 last_vertex[256]) {
	ISO_ASSERT(count > 0 && count <= kVertexBlockMaxSize);

	uint8 buffer[kVertexBlockMaxSize] = {0};

	for (size_t k = 0; k < vertex_size; ++k) {
		size_t	offset	= k;
		uint8	p		= last_vertex[k];

		for (size_t i = 0; i < count; ++i) {
			buffer[i] = zigzag8(vertex_data[offset] - p);
			p		= vertex_data[offset];
			offset	+= vertex_size;
		}

		data = encodeBytes(data, buffer, align(count, kByteGroupSize));
	}

	memcpy(last_vertex, vertex_data + vertex_size * (count - 1), vertex_size);
	return data;
}

static const uint8* decodeBytesGroup(const uint8* data, uint8* buffer, int bitslog2) {
#define READ() byte = *data++
#define NEXT(bits) enc = byte >> (8 - bits), byte <<= bits, encv = *data_var, *buffer++ = (enc == (1 << bits) - 1) ? encv : enc, data_var += (enc == (1 << bits) - 1)

	uint8		 byte, enc, encv;
	const uint8* data_var;

	switch (bitslog2) {
		case 0:
			memset(buffer, 0, kByteGroupSize);
			return data;
		case 1:			// 4 groups with 4 2-bit values in each byte
			data_var = data + 4;
			READ(), NEXT(2), NEXT(2), NEXT(2), NEXT(2);
			READ(), NEXT(2), NEXT(2), NEXT(2), NEXT(2);
			READ(), NEXT(2), NEXT(2), NEXT(2), NEXT(2);
			READ(), NEXT(2), NEXT(2), NEXT(2), NEXT(2);
			return data_var;

		case 2:			// 8 groups with 2 4-bit values in each byte
			data_var = data + 8;
			READ(), NEXT(4), NEXT(4);
			READ(), NEXT(4), NEXT(4);
			READ(), NEXT(4), NEXT(4);
			READ(), NEXT(4), NEXT(4);
			READ(), NEXT(4), NEXT(4);
			READ(), NEXT(4), NEXT(4);
			READ(), NEXT(4), NEXT(4);
			READ(), NEXT(4), NEXT(4);
			return data_var;

		case 3:
			memcpy(buffer, data, kByteGroupSize);
			return data + kByteGroupSize;

		default:  // unreachable since bitslog2 is a 2-bit value
			ISO_ASSERT(!"Unexpected bit length");
			return data;
	}

#undef READ
#undef NEXT
}

#if 1//defined(SIMD_SSE)

struct decode_tables {
	uint8x8	shuffle[256];
	uint8	count[256];

	decode_tables() {
		for (int mask = 0; mask < 256; ++mask) {
			uint8x8 s;
			uint8	c = 0;
			for (int i = 0; i < 8; ++i)
				s[i] = (mask >> i) & 1 ? c++ : 0x80;

			shuffle[mask]	= s;
			count[mask]		= c;
		}
	}

	uint8x16 get_shuffle(uint16 mask) {
		return concat(shuffle[mask & 255], shuffle[mask >> 8] + count[mask & 255]);

	}
} decode_tables;

static const uint8* decodeBytesGroupSimd(const uint8* data, uint8* buffer, int bitslog2) {
	switch (bitslog2) {
		case 0:
			*(uint128*)buffer = zero;
			return data;

		case 1: {
			uint32	data32	= *(packed<uint32>*)data;
			data32	&= data32 >> 1;
			data32	= (data32 & 0x11111111) + ((data32 >> 2) & 0x11111111);
			int		datacnt	= (data32 * 0x11111111) >> 28;
			
			auto	sel2	= as<uint8>(*(packed<uint32>*)data);
			auto	sel22	= swizzle<0, 4+0, 1, 4+1, 2, 4+2, 3, 4+3>(as<uint8>(as<uint16>(sel2) >> 4), sel2);
			auto	sel2222 = swizzle<0, 8+0, 1, 8+1, 2, 8+2, 3, 8+3, 4, 8+4, 5, 8+5, 6, 8+6, 7, 8+7>(as<uint8>(as<uint16>(sel22) >> 2), sel22);
			auto	sel		= sel2222 & 3;

			auto	mask	= sel == 3;
			auto	shuf	= decode_tables.get_shuffle(bit_mask(mask));
			uint8x16 rest	= *(packed<uint8x16>*)(data + 4);

			*(uint8x16*)buffer = select(mask, dynamic_swizzle(shuf, rest), sel);
			return data + 4 + datacnt;
		}

		case 2: {
			uint64 data64	= *(packed<uint64>*)data;
			data64	&= data64 >> 1;
			data64	&= data64 >> 2;
			int		datacnt = int(((data64 & 0x1111111111111111ull) * 0x1111111111111111ull) >> 60);

			auto	sel4	= as<uint8>(*(packed<uint64>*)data);
			auto	sel44	= swizzle<0, 8+0, 1, 8+1, 2, 8+2, 3, 8+3, 4, 8+4, 5, 8+5, 6, 8+6, 7, 8+7>(as<uint8>(as<uint16>(sel4) >> 4), sel4);
			auto	sel		= sel44 & 15;

			auto	mask	= sel == 15;
			auto	shuf	= decode_tables.get_shuffle(bit_mask(mask));
			uint8x16 rest	= *(packed<uint8x16>*)(data + 8);

			*(uint8x16*)buffer = select(mask, dynamic_swizzle(shuf, rest), sel);
			return data + 8 + datacnt;
		}

		default:
		case 3:
			*(uint128*)buffer = *(packed<uint128>*)(data);;
			return data + 16;
	}
}
#endif

static const uint8* decodeBytes(const uint8* data, uint8* buffer, size_t buffer_size) {
	ISO_ASSERT(buffer_size % kByteGroupSize == 0);

	const uint8* header = data;
	data += (buffer_size / kByteGroupSize + 3) / 4;	// round number of groups to 4 to get number of header bytes

	for (size_t i = 0; i < buffer_size; i += kByteGroupSize) {
		size_t		offset		= i / kByteGroupSize;
		int			bitslog2	= (header[offset / 4] >> ((offset % 4) * 2)) & 3;
	#if 0
		uint8x16	test;
		auto data0 = decodeBytesGroup(data, (uint8*)&test, bitslog2);
		data = decodeBytesGroupSimd(data, buffer + i, bitslog2);
		ISO_ASSERT(data0 == data && all(*(uint8x16*)(buffer + i) == test));
	#else
		data = decodeBytesGroupSimd(data, buffer + i, bitslog2);
	#endif
	}

	return data;
}

static const uint8* decodeVertexBlock(const uint8* data, const uint8* data_end, uint8* vertex_data, size_t count, size_t vertex_size, uint8 last_vertex[256]) {
	ISO_ASSERT(count > 0 && count <= kVertexBlockMaxSize);

	uint8	buffer[kVertexBlockMaxSize];
	uint8	transposed[kVertexBlockSizeBytes];

	size_t	count_aligned = align(count, kByteGroupSize);

	for (size_t k = 0; k < vertex_size; ++k) {
		data = decodeBytes(data, buffer, count_aligned);

		size_t	offset	= k;
		uint8	p		= last_vertex[k];
		for (size_t i = 0; i < count; ++i) {
			transposed[offset]	= (p += unzigzag8(buffer[i]));
			offset	+= vertex_size;
		}
	}

	memcpy(vertex_data, transposed, count * vertex_size);
	memcpy(last_vertex, transposed + vertex_size * (count - 1), vertex_size);
	return data;
}

size_t encodeVertexBuffer(memory_block buffer, const void* vertices, size_t count, size_t vertex_size, int version) {
	ISO_ASSERT(vertex_size > 0 && vertex_size <= 256 && vertex_size % 4 == 0);

	const uint8* vertex_data = (const uint8*)vertices;
	uint8*	data		= buffer;
	uint8*	data_end	= buffer.end();

	if (data_end - data < 1 + vertex_size)
		return 0;

	*data++ = (uint8)(kVertexHeader | version);

	uint8	first_vertex[256];
	uint8	last_vertex[256];

	if (count > 0)
		memcpy(first_vertex, vertex_data, vertex_size);
	memcpy(last_vertex, first_vertex, vertex_size);

	for (size_t offset = 0, block_size = getVertexBlockSize(vertex_size); offset < count; offset += block_size)
		data = encodeVertexBlock(data, vertex_data + offset * vertex_size, min(block_size, count - offset), vertex_size, last_vertex);

	size_t tail_size = max(vertex_size, kTailMaxSize);
	if (data_end - data < tail_size)
		return 0;

	// write first vertex to the end of the stream and pad it to 32 bytes; this is important to simplify bounds checks in decoder
	if (vertex_size < kTailMaxSize) {
		memset(data, 0, kTailMaxSize - vertex_size);
		data += kTailMaxSize - vertex_size;
	}

	memcpy(data, first_vertex, vertex_size);
	data += vertex_size;

	ISO_ASSERT(data >= buffer + tail_size && data <= data_end);
	return data - buffer;
}

bool decodeVertexBuffer(const_memory_block buffer, void* destination, size_t count, size_t vertex_size) {
	ISO_ASSERT(vertex_size > 0 && vertex_size <= 256 && vertex_size % 4 == 0);

	uint8* vertex_data		= (uint8*)destination;
	const uint8* data		= buffer;
	const uint8* data_end	= buffer.end();

	if (data_end - data < 1 + vertex_size)
		return false;

	uint8 h = *data++;
	if ((h & 0xf0) != kVertexHeader)
		return false;

	int version = h & 0x0f;
	if (version > 0)
		return false;

	uint8	last_vertex[256];
	memcpy(last_vertex, data_end - vertex_size, vertex_size);

	for (size_t offset = 0, block_size = getVertexBlockSize(vertex_size); offset < count; offset += block_size)
		data = decodeVertexBlock(data, data_end, vertex_data + offset * vertex_size, min(block_size, count - offset), vertex_size, last_vertex);

	return data_end - data == max(vertex_size, kTailMaxSize);
}


malloc_block compressVertexStream(const void *data, size_t count, size_t vertex_size, int version) {
	ISO_ASSERT(vertex_size > 0 && vertex_size <= 256 && vertex_size % 4 == 0);

	size_t	block_size			= getVertexBlockSize(vertex_size);
	size_t	block_count			= (count + block_size - 1) / block_size;
	size_t	block_header_size	= (block_size / kByteGroupSize + 3) / 4;
	size_t	block_data_size		= block_size;
	size_t	tail_size			= max(vertex_size, kTailMaxSize);
	size_t	size				= 1 + block_count * vertex_size * (block_header_size + block_data_size) + tail_size;

	malloc_block	buffer(size);
	size	= encodeVertexBuffer(buffer, data, count, vertex_size, version);
	return buffer.resize(size);
}
//-----------------------------------------------------------------------------
// vertex filters
//-----------------------------------------------------------------------------

void encodeExpParallel(int32x3 *out, range<const float3*> data, int bits) {
	int32x3 exp = 0;
	for (auto &i : data)
		exp	= max(exp, (as<int>(i) >> 23) & 0x7f);

	// scale the mantissa to make it a K-bit signed integer (K-1 bits for magnitude)
	exp -= 128 + (bits - 1);
	auto	scale = make_exp<3>(-exp);

	for (auto &i : data) {
		int32x3 m = to<int>(round(i * scale));
		*out++ = (m & iso::bits(24)) | (exp << 24);
	}
}

bool	defilterVertexBuffer(memory_block buffer, int filter, size_t vertex_size) {
	size_t count = buffer.size() / vertex_size;

	switch (filter) {
		case 1:	//OCTAHEDRAL
			switch (vertex_size) {
				case 4: {
					int8x4	*p = buffer;
					for (int i = 0; i < count; i++) {
						auto	n	= meshopt::decodeOct(to<int>(*p));
						(*p).xyz	= to<int8>(n.xyz * 127);
						++p;
					}
					break;
				}
				case 8: {
					int16x4	*p = buffer;
					for (int i = 0; i < count; i++) {
						auto	n	= meshopt::decodeOct(to<int>(*p));
						(*p++).xyz	= to<int16>(n.xyz * 32767);
					}
					break;
				}
				default:
					ISO_ASSERT(0);
			}
			break;

		case 2: {
			ISO_ASSERT(vertex_size == 8);
			int16x4	*p = buffer;
			for (int i = 0; i < count; i++) {
				auto	n	= meshopt::decodeQuat(to<int>(*p));
				*p++		= to<int16>(n * 32767);
			}
			break;
		}
		case 3: {
			ISO_ASSERT(vertex_size % 4 == 0);
			int32x4	*p = buffer;
			for (int i = 0, n = count * (vertex_size / 4); i < n; i++) {
				auto	f	= meshopt::decodeExp(*p);
				*p++		= as<int>(f);
			}
			break;
		}
		default:
			break;
	}
	return true;
}

} // namespace meshopt