#include "snappy.h"
#include "base/bits.h"

#define UNALIGNED_LOAD32(p)	(*(packed<uint32>*)p)
#define UNALIGNED_LOAD64(p)	(*(packed<uint64>*)p)

namespace snappy {

typedef intptr_t	ssize_t;

static const uint32 kuint32max	= static_cast<uint32>(0xFFFFFFFF);

// This can be more efficient than UNALIGNED_LOAD64 + UNALIGNED_STORE64 on some platforms, in particular ARM.
inline void UnalignedCopy64(const void *src, void *dst) {
	*(packed<uint64>*)dst = *(packed<uint64>*)src;
}

// Variable-length integer encoding.
class Varint {
public:
	// Maximum lengths of varint encoding of uint32.
	static const int kMax32 = 5;

	// Attempts to parse a varint32 from a prefix of the bytes in [ptr,limit-1].
	// Never reads a character at or beyond limit.  If a valid/terminated varint32
	// was found in the range, stores it in *OUTPUT and returns a pointer just
	// past the last byte of the varint32. Else returns NULL.  On success,
	// "result <= limit".
	static const char* Parse32WithLimit(const char* p, const char* l, uint32* OUTPUT) {
		const unsigned char* ptr	= reinterpret_cast<const unsigned char*>(p);
		const unsigned char* limit	= reinterpret_cast<const unsigned char*>(l);
		uint32 b, result;
		if (ptr >= limit) return NULL;
		b = *(ptr++); result = b & 127;          if (b < 128) goto done;
		if (ptr >= limit) return NULL;
		b = *(ptr++); result |= (b & 127) << 7; if (b < 128) goto done;
		if (ptr >= limit) return NULL;
		b = *(ptr++); result |= (b & 127) << 14; if (b < 128) goto done;
		if (ptr >= limit) return NULL;
		b = *(ptr++); result |= (b & 127) << 21; if (b < 128) goto done;
		if (ptr >= limit) return NULL;
		b = *(ptr++); result |= (b & 127) << 28; if (b < 16) goto done;
		return NULL;       // Value is too long to be a varint32
	done:
		*OUTPUT = result;
		return reinterpret_cast<const char*>(ptr);
	}
	// REQUIRES   "ptr" points to a buffer of length sufficient to hold "v".
	// EFFECTS    Encodes "v" into "ptr" and returns a pointer to the
	//            byte just past the last encoded byte.
	static char* Encode32(char* p, uint32 v) {
		// Operate on characters as unsigneds
		unsigned char* ptr = reinterpret_cast<unsigned char*>(p);
		static const int B = 128;
		if (v < (1 << 7)) {
			*(ptr++) = v;
		} else if (v < (1 << 14)) {
			*(ptr++) = v | B;
			*(ptr++) = v >> 7;
		} else if (v < (1 << 21)) {
			*(ptr++) = v | B;
			*(ptr++) = (v >> 7) | B;
			*(ptr++) = v >> 14;
		} else if (v < (1 << 28)) {
			*(ptr++) = v | B;
			*(ptr++) = (v >> 7) | B;
			*(ptr++) = (v >> 14) | B;
			*(ptr++) = v >> 21;
		} else {
			*(ptr++) = v | B;
			*(ptr++) = (v >> 7) | B;
			*(ptr++) = (v >> 14) | B;
			*(ptr++) = (v >> 21) | B;
			*(ptr++) = v >> 28;
		}
		return reinterpret_cast<char*>(ptr);
	}
	// EFFECTS    Appends the varint representation of "value" to "*s".
	static void Append32(string* s, uint32 value);
};

namespace internal {
	class WorkingMemory {
	public:
		WorkingMemory() : large_table_(NULL) {}
		~WorkingMemory() { delete[] large_table_; }

		// Allocates and clears a hash table using memory in "*this",
		// stores the number of buckets in "*table_size" and returns a pointer to
		// the base of the hash table.
		uint16* GetHashTable(size_t input_size, int* table_size);

	private:
		uint16	small_table_[1 << 10];	// 2KB
		uint16* large_table_;			// Allocated only when needed
	};

	// Flat array compression that does not emit the "uncompressed length"
	// prefix. Compresses "input" string to the "*op" buffer.
	//
	// REQUIRES: "input_length <= kBlockSize"
	// REQUIRES: "op" points to an array of memory that is at least
	// "MaxCompressedLength(input_length)" in size.
	// REQUIRES: All elements in "table[0..table_size-1]" are initialized to zero.
	// REQUIRES: "table_size" is a power of two
	//
	// Returns an "end" pointer into "op" buffer.
	// "end - op" is the compressed size of "input".
	char* CompressFragment(const char* input, size_t input_length, char* op, uint16* table, const int table_size);

	// Return the largest n such that
	//
	//   s1[0,n-1] == s2[0,n-1]
	//   and n <= (s2_limit - s2).
	//
	// Does not read *s2_limit or beyond.
	// Does not read *(s1 + (s2_limit - s2)) or beyond.
	// Requires that s2_limit >= s2.
	//
	// Separate implementation for x86_64, for speed.  Uses the fact that
	// x86_64 is little endian.
#if defined(ISO_PTR64)
	static inline int FindMatchLength(const char* s1,
		const char* s2,
		const char* s2_limit) {
		ISO_ASSERT(s2_limit >= s2);
		int matched = 0;

		// Find out how long the match is. We loop over the data 64 bits at a
		// time until we find a 64-bit block that doesn't match; then we find
		// the first non-matching bit and use that to calculate the total
		// length of the match.
		while (likely(s2 <= s2_limit - 8)) {
			if (unlikely(UNALIGNED_LOAD64(s2) == UNALIGNED_LOAD64(s1 + matched))) {
				s2 += 8;
				matched += 8;
			} else {
				// On current (mid-2008) Opteron models there is a 3% more
				// efficient code sequence to find the first non-matching byte.
				// However, what follows is ~10% better on Intel Core 2 and newer,
				// and we expect AMD's bsf instruction to improve.
				uint64 x = UNALIGNED_LOAD64(s2) ^ UNALIGNED_LOAD64(s1 + matched);
				int matching_bits = lowest_set_index(x);
				matched += matching_bits >> 3;
				return matched;
			}
		}
		while (likely(s2 < s2_limit)) {
			if (likely(s1[matched] == *s2)) {
				++s2;
				++matched;
			} else {
				return matched;
			}
		}
		return matched;
	}
#else
	static inline int FindMatchLength(const char* s1,
		const char* s2,
		const char* s2_limit) {
		// Implementation based on the x86-64 version, above.
		ISO_ASSERT(s2_limit >= s2);
		int matched = 0;

		while (s2 <= s2_limit - 4 &&
			UNALIGNED_LOAD32(s2) == UNALIGNED_LOAD32(s1 + matched)) {
			s2 += 4;
			matched += 4;
		}
		if (!iso_bigendian && s2 <= s2_limit - 4) {
			uint32 x = UNALIGNED_LOAD32(s2) ^ UNALIGNED_LOAD32(s1 + matched);
			int matching_bits = lowest_set_index(x);
			matched += matching_bits >> 3;
		} else {
			while ((s2 < s2_limit) && (s1[matched] == *s2)) {
				++s2;
				++matched;
			}
		}
		return matched;
	}
#endif

}  // end namespace internal
}  // end namespace snappy

namespace snappy {

// A Sink is an interface that consumes a sequence of bytes.
class Sink {
public:
	Sink() {}
	virtual ~Sink() {}

	// Append "bytes[0,n-1]" to this.
	virtual void Append(const char* bytes, size_t n) = 0;
	virtual char* GetAppendBuffer(size_t length, char* scratch) { return scratch; }
private:
	// No copying
	Sink(const Sink&);
	void operator=(const Sink&);
};

// A Source is an interface that yields a sequence of bytes
class Source {
public:
	Source() {}
	virtual ~Source() {}

	// Return the number of bytes left to read from the source
	virtual size_t Available() const = 0;

	virtual const char* Peek(size_t* len) = 0;

	virtual void Skip(size_t n) = 0;

private:
	// No copying
	Source(const Source&);
	void operator=(const Source&);
};

// A Source implementation that yields the contents of a flat array
class ByteArraySource : public Source {
public:
	ByteArraySource(const char* p, size_t n) : ptr_(p), left_(n) {}
	size_t Available() const override {
		return left_;
	}
	const char* Peek(size_t* len) override {
		*len = left_;
		return ptr_;
	}
	void Skip(size_t n) override {
		left_ -= n;
		ptr_ += n;
	}
private:
	const char* ptr_;
	size_t left_;
};

// A Sink implementation that writes to a flat array without any bound checks.
class UncheckedByteArraySink : public Sink {
public:
	explicit UncheckedByteArraySink(char* dest) : dest_(dest) {}
	void Append(const char* data, size_t n) override {
		// Do no copying if the caller filled in the result of GetAppendBuffer()
		if (data != dest_)
			memcpy(dest_, data, n);
		dest_ += n;
	}
	char* GetAppendBuffer(size_t len, char* scratch) override {
		return dest_;
	}
	char* CurrentDestination() const { return dest_; }
private:
	char* dest_;
};

}

namespace snappy {

// Any hash function will produce a valid compressed bitstream, but a good
// hash function reduces the number of collisions and thus yields better
// compression for compressible input, and more speed for incompressible
// input. Of course, it doesn't hurt if the hash function is reasonably fast
// either, as it gets called a lot.
static inline uint32 HashBytes(uint32 bytes, int shift) {
	uint32 kMul = 0x1e35a7bd;
	return (bytes * kMul) >> shift;
}
static inline uint32 Hash(const char* p, int shift) {
	return HashBytes(UNALIGNED_LOAD32(p), shift);
}

size_t MaxCompressedLength(size_t source_len) {
	// Compressed data can be defined as:
	//    compressed := item* literal*
	//    item       := literal* copy
	//
	// The trailing literal sequence has a space blowup of at most 62/60
	// since a literal of length 60 needs one tag byte + one extra byte
	// for length information.
	//
	// Item blowup is trickier to measure.  Suppose the "copy" op copies
	// 4 bytes of data.  Because of a special check in the encoding code,
	// we produce a 4-byte copy only if the offset is < 65536.  Therefore
	// the copy op takes 3 bytes to encode, and this type of item leads
	// to at most the 62/60 blowup for representing literals.
	//
	// Suppose the "copy" op copies 5 bytes of data.  If the offset is big
	// enough, it will take 5 bytes to encode the copy op.  Therefore the
	// worst case here is a one-byte literal followed by a five-byte copy.
	// I.e., 6 bytes of input turn into 7 bytes of "compressed" data.
	//
	// This last factor dominates the blowup, so the final estimate is:
	return 32 + source_len + source_len / 6;
}

enum {
	LITERAL = 0,
	COPY_1_BYTE_OFFSET = 1,  // 3 bit length + 3 bits of offset in opcode
	COPY_2_BYTE_OFFSET = 2,
	COPY_4_BYTE_OFFSET = 3
};
static const int kMaximumTagLength = 5;  // COPY_4_BYTE_OFFSET plus the actual offset.

// Copy "len" bytes from "src" to "op", one byte at a time.  Used for
// handling COPY operations where the input and output regions may
// overlap.  For example, suppose:
//    src    == "ab"
//    op     == src + 2
//    len    == 20
// After IncrementalCopy(src, op, len), the result will have
// eleven copies of "ab"
//    ababababababababababab
// Note that this does not match the semantics of either memcpy()
// or memmove().
static inline void IncrementalCopy(const char* src, char* op, ssize_t len) {
	ISO_ASSERT(len > 0);
	do {
		*op++ = *src++;
	} while (--len > 0);
}

// Equivalent to IncrementalCopy except that it can write up to ten extra
// bytes after the end of the copy, and that it is faster.
//
// The main part of this loop is a simple copy of eight bytes at a time until
// we've copied (at least) the requested amount of bytes.  However, if op and
// src are less than eight bytes apart (indicating a repeating pattern of
// length < 8), we first need to expand the pattern in order to get the correct
// results. For instance, if the buffer looks like this, with the eight-byte
// <src> and <op> patterns marked as intervals:
//
//    abxxxxxxxxxxxx
//    [------]           src
//      [------]         op
//
// a single eight-byte copy from <src> to <op> will repeat the pattern once,
// after which we can move <op> two bytes without moving <src>:
//
//    ababxxxxxxxxxx
//    [------]           src
//        [------]       op
//
// and repeat the exercise until the two no longer overlap.
//
// This allows us to do very well in the special case of one single byte
// repeated many times, without taking a big hit for more general cases.
//
// The worst case of extra writing past the end of the match occurs when
// op - src == 1 and len == 1; the last copy will read from byte positions
// [0..7] and write to [4..11], whereas it was only supposed to write to
// position 1. Thus, ten excess bytes.

namespace {

const int kMaxIncrementCopyOverflow = 10;

inline void IncrementalCopyFastPath(const char* src, char* op, ssize_t len) {
	while (op - src < 8) {
		UnalignedCopy64(src, op);
		len -= op - src;
		op += op - src;
	}
	while (len > 0) {
		UnalignedCopy64(src, op);
		src += 8;
		op += 8;
		len -= 8;
	}
}

}  // namespace

static inline char* EmitLiteral(char* op,
	const char* literal,
	int len,
	bool allow_fast_path) {
	int n = len - 1;      // Zero-length literals are disallowed
	if (n < 60) {
		// Fits in tag byte
		*op++ = LITERAL | (n << 2);

		// The vast majority of copies are below 16 bytes, for which a
		// call to memcpy is overkill. This fast path can sometimes
		// copy up to 15 bytes too much, but that is okay in the
		// main loop, since we have a bit to go on for both sides:
		//
		//   - The input will always have kInputMarginBytes = 15 extra
		//     available bytes, as long as we're in the main loop, and
		//     if not, allow_fast_path = false.
		//   - The output will always have 32 spare bytes (see
		//     MaxCompressedLength).
		if (allow_fast_path && len <= 16) {
			UnalignedCopy64(literal, op);
			UnalignedCopy64(literal + 8, op + 8);
			return op + len;
		}
	} else {
		// Encode in upcoming bytes
		char* base = op;
		int count = 0;
		op++;
		while (n > 0) {
			*op++ = n & 0xff;
			n >>= 8;
			count++;
		}
		ISO_ASSERT(count >= 1);
		ISO_ASSERT(count <= 4);
		*base = LITERAL | ((59 + count) << 2);
	}
	memcpy(op, literal, len);
	return op + len;
}

static inline char* EmitCopyLessThan64(char* op, size_t offset, int len) {
	ISO_ASSERT(len <= 64);
	ISO_ASSERT(len >= 4);
	ISO_ASSERT(offset < 65536);

	if ((len < 12) && (offset < 2048)) {
		size_t len_minus_4 = len - 4;
		ISO_ASSERT(len_minus_4 < 8);            // Must fit in 3 bits
		*op++ = char(COPY_1_BYTE_OFFSET + ((len_minus_4) << 2) + ((offset >> 8) << 5));
		*op++ = offset & 0xff;
	} else {
		*op++ = COPY_2_BYTE_OFFSET + ((len - 1) << 2);
		*(int16le*)op = int16(offset);
		op += 2;
	}
	return op;
}

static inline char* EmitCopy(char* op, size_t offset, int len) {
	// Emit 64 byte copies but make sure to keep at least four bytes reserved
	while (len >= 68) {
		op = EmitCopyLessThan64(op, offset, 64);
		len -= 64;
	}

	// Emit an extra 60 byte copy if have too much data to fit in one copy
	if (len > 64) {
		op = EmitCopyLessThan64(op, offset, 60);
		len -= 60;
	}

	// Emit remainder
	op = EmitCopyLessThan64(op, offset, len);
	return op;
}


bool GetUncompressedLength(const char* start, size_t n, size_t* result) {
	uint32 v = 0;
	const char* limit = start + n;
	if (Varint::Parse32WithLimit(start, limit, &v) != NULL) {
		*result = v;
		return true;
	} else {
		return false;
	}
}

namespace internal {
	uint16* WorkingMemory::GetHashTable(size_t input_size, int* table_size) {
		// Use smaller hash table when input.size() is smaller, since we
		// fill the table, incurring O(hash table size) overhead for
		// compression, and if the input is short, we won't need that
		// many hash table entries anyway.
		ISO_ASSERT(kMaxHashTableSize >= 256);
		size_t htsize = 256;
		while (htsize < kMaxHashTableSize && htsize < input_size) {
			htsize <<= 1;
		}

		uint16* table;
		if (htsize <= num_elements(small_table_)) {
			table = small_table_;
		} else {
			if (large_table_ == NULL) {
				large_table_ = new uint16[kMaxHashTableSize];
			}
			table = large_table_;
		}

		*table_size = int(htsize);
		memset(table, 0, htsize * sizeof(*table));
		return table;
	}
}  // end namespace internal

// For 0 <= offset <= 4, GetUint32AtOffset(GetEightBytesAt(p), offset) will
// equal UNALIGNED_LOAD32(p + offset).  Motivation: On x86-64 hardware we have
// empirically found that overlapping loads such as
//  UNALIGNED_LOAD32(p) ... UNALIGNED_LOAD32(p+1) ... UNALIGNED_LOAD32(p+2)
// are slower than UNALIGNED_LOAD64(p) followed by shifts and casts to uint32.
//
// We have different versions for 64- and 32-bit; ideally we would avoid the
// two functions and just inline the UNALIGNED_LOAD64 call into
// GetUint32AtOffset, but GCC (at least not as of 4.6) is seemingly not clever
// enough to avoid loading the value multiple times then. For 64-bit, the load
// is done when GetEightBytesAt() is called, whereas for 32-bit, the load is
// done at GetUint32AtOffset() time.

#ifdef ISO_PTR64

typedef uint64 EightBytesReference;

static inline EightBytesReference GetEightBytesAt(const char* ptr) {
	return UNALIGNED_LOAD64(ptr);
}

static inline uint32 GetUint32AtOffset(uint64 v, int offset) {
	ISO_ASSERT(offset >= 0 && offset <= 4);
	return v >> (!iso_bigendian ? 8 * offset : 32 - 8 * offset);
}

#else

typedef const char* EightBytesReference;

static inline EightBytesReference GetEightBytesAt(const char* ptr) {
	return ptr;
}

static inline uint32 GetUint32AtOffset(const char* v, int offset) {
	ISO_ASSERT(offset >= 0 && offset <= 4);
	return UNALIGNED_LOAD32(v + offset);
}

#endif

// Flat array compression that does not emit the "uncompressed length"
// prefix. Compresses "input" string to the "*op" buffer.
//
// REQUIRES: "input" is at most "kBlockSize" bytes long.
// REQUIRES: "op" points to an array of memory that is at least
// "MaxCompressedLength(input.size())" in size.
// REQUIRES: All elements in "table[0..table_size-1]" are initialized to zero.
// REQUIRES: "table_size" is a power of two
//
// Returns an "end" pointer into "op" buffer.
// "end - op" is the compressed size of "input".
namespace internal {
	char* CompressFragment(const char* input, size_t input_size, char* op, uint16* table, const int table_size) {
		// "ip" is the input pointer, and "op" is the output pointer.
		const char* ip = input;
		ISO_ASSERT(input_size <= kBlockSize);
		ISO_ASSERT((table_size & (table_size - 1)) == 0); // table must be power of two
		const int shift = 32 - log2_floor(table_size);
		ISO_ASSERT(static_cast<int>(kuint32max >> shift) == table_size - 1);
		const char* ip_end = input + input_size;
		const char* base_ip = ip;
		// Bytes in [next_emit, ip) will be emitted as literal bytes.  Or
		// [next_emit, ip_end) after the main loop.
		const char* next_emit = ip;

		const size_t kInputMarginBytes = 15;
		if (likely(input_size >= kInputMarginBytes)) {
			const char* ip_limit = input + input_size - kInputMarginBytes;

			for (uint32 next_hash = Hash(++ip, shift); ; ) {
				ISO_ASSERT(next_emit < ip);
				// The body of this loop calls EmitLiteral once and then EmitCopy one or
				// more times.  (The exception is that when we're close to exhausting
				// the input we goto emit_remainder.)
				//
				// In the first iteration of this loop we're just starting, so
				// there's nothing to copy, so calling EmitLiteral once is
				// necessary.  And we only start a new iteration when the
				// current iteration has determined that a call to EmitLiteral will
				// precede the next call to EmitCopy (if any).
				//
				// Step 1: Scan forward in the input looking for a 4-byte-long match.
				// If we get close to exhausting the input then goto emit_remainder.
				//
				// Heuristic match skipping: If 32 bytes are scanned with no matches
				// found, start looking only at every other byte. If 32 more bytes are
				// scanned, look at every third byte, etc.. When a match is found,
				// immediately go back to looking at every byte. This is a small loss
				// (~5% performance, ~0.1% density) for compressible data due to more
				// bookkeeping, but for non-compressible data (such as JPEG) it's a huge
				// win since the compressor quickly "realizes" the data is incompressible
				// and doesn't bother looking for matches everywhere.
				//
				// The "skip" variable keeps track of how many bytes there are since the
				// last match; dividing it by 32 (ie. right-shifting by five) gives the
				// number of bytes to move ahead for each iteration.
				uint32 skip = 32;

				const char* next_ip = ip;
				const char* candidate;
				do {
					ip = next_ip;
					uint32 hash = next_hash;
					ISO_ASSERT(hash == Hash(ip, shift));
					uint32 bytes_between_hash_lookups = skip++ >> 5;
					next_ip = ip + bytes_between_hash_lookups;
					if (unlikely(next_ip > ip_limit))
						goto emit_remainder;
					next_hash = Hash(next_ip, shift);
					candidate = base_ip + table[hash];
					ISO_ASSERT(candidate >= base_ip);
					ISO_ASSERT(candidate < ip);

					table[hash] = ip - base_ip;
				} while (likely(UNALIGNED_LOAD32(ip) != UNALIGNED_LOAD32(candidate)));

				// Step 2: A 4-byte match has been found.  We'll later see if more
				// than 4 bytes match.  But, prior to the match, input
				// bytes [next_emit, ip) are unmatched.  Emit them as "literal bytes."
				ISO_ASSERT(next_emit + 16 <= ip_end);
				op = EmitLiteral(op, next_emit, ip - next_emit, true);

				// Step 3: Call EmitCopy, and then see if another EmitCopy could
				// be our next move.  Repeat until we find no match for the
				// input immediately after what was consumed by the last EmitCopy call.
				//
				// If we exit this loop normally then we need to call EmitLiteral next,
				// though we don't yet know how big the literal will be.  We handle that
				// by proceeding to the next iteration of the main loop.  We also can exit
				// this loop via goto if we get close to exhausting the input.
				EightBytesReference input_bytes;
				uint32 candidate_bytes = 0;

				do {
					// We have a 4-byte match at ip, and no need to emit any
					// "literal bytes" prior to ip.
					const char* base = ip;
					int matched = 4 + FindMatchLength(candidate + 4, ip + 4, ip_end);
					ip += matched;
					size_t offset = base - candidate;
					ISO_ASSERT(0 == memcmp(base, candidate, matched));
					op = EmitCopy(op, offset, matched);
					// We could immediately start working at ip now, but to improve
					// compression we first update table[Hash(ip - 1, ...)].
					const char* insert_tail = ip - 1;
					next_emit = ip;
					if (unlikely(ip >= ip_limit))
						goto emit_remainder;
					input_bytes = GetEightBytesAt(insert_tail);
					uint32 prev_hash = HashBytes(GetUint32AtOffset(input_bytes, 0), shift);
					table[prev_hash] = ip - base_ip - 1;
					uint32 cur_hash = HashBytes(GetUint32AtOffset(input_bytes, 1), shift);
					candidate = base_ip + table[cur_hash];
					candidate_bytes = UNALIGNED_LOAD32(candidate);
					table[cur_hash] = ip - base_ip;
				} while (GetUint32AtOffset(input_bytes, 1) == candidate_bytes);

				next_hash = HashBytes(GetUint32AtOffset(input_bytes, 2), shift);
				++ip;
			}
		}

	emit_remainder:
		// Emit the remaining bytes as a literal
		if (next_emit < ip_end)
			op = EmitLiteral(op, next_emit, ip_end - next_emit, false);

		return op;
	}
}  // end namespace internal

// Signature of output types needed by decompression code.
// The decompression code is templatized on a type that obeys this
// signature so that we do not pay virtual function call overhead in
// the middle of a tight decompression loop.
//
// class DecompressionWriter {
//  public:
//   // Called before decompression
//   void SetExpectedLength(size_t length);
//
//   // Called after decompression
//   bool CheckLength() const;
//
//   // Called repeatedly during decompression
//   bool Append(const char* ip, size_t length);
//   bool AppendFromSelf(uint32 offset, size_t length);
//
//   // The rules for how TryFastAppend differs from Append are somewhat
//   // convoluted:
//   //
//   //  - TryFastAppend is allowed to decline (return false) at any
//   //    time, for any reason -- just "return false" would be
//   //    a perfectly legal implementation of TryFastAppend.
//   //    The intention is for TryFastAppend to allow a fast path
//   //    in the common case of a small append.
//   //  - TryFastAppend is allowed to read up to <available> bytes
//   //    from the input buffer, whereas Append is allowed to read
//   //    <length>. However, if it returns true, it must leave
//   //    at least five (kMaximumTagLength) bytes in the input buffer
//   //    afterwards, so that there is always enough space to read the
//   //    next tag without checking for a refill.
//   //  - TryFastAppend must always return decline (return false)
//   //    if <length> is 61 or more, as in this case the literal length is not
//   //    decoded fully. In practice, this should not be a big problem,
//   //    as it is unlikely that one would implement a fast path accepting
//   //    this much data.
//   //
//   bool TryFastAppend(const char* ip, size_t available, size_t length);
// };

// -----------------------------------------------------------------------
// Lookup table for decompression code.  Generated by ComputeTable() below.
// -----------------------------------------------------------------------

// Mapping from i in range [0,4] to a mask to extract the bottom 8*i bits
static const uint32 wordmask[] = {
  0u, 0xffu, 0xffffu, 0xffffffu, 0xffffffffu
};

// Data stored per entry in lookup table:
//      Range   Bits-used       Description
//      ------------------------------------
//      1..64   0..7            Literal/copy length encoded in opcode byte
//      0..7    8..10           Copy offset encoded in opcode byte / 256
//      0..4    11..13          Extra bytes after opcode
//
// We use eight bits for the length even though 7 would have sufficed
// because of efficiency reasons:
//      (1) Extracting a byte is faster than a bit-field
//      (2) It properly aligns copy offset so we do not need a <<8
static const uint16 char_table[256] = {
  0x0001, 0x0804, 0x1001, 0x2001, 0x0002, 0x0805, 0x1002, 0x2002,
  0x0003, 0x0806, 0x1003, 0x2003, 0x0004, 0x0807, 0x1004, 0x2004,
  0x0005, 0x0808, 0x1005, 0x2005, 0x0006, 0x0809, 0x1006, 0x2006,
  0x0007, 0x080a, 0x1007, 0x2007, 0x0008, 0x080b, 0x1008, 0x2008,
  0x0009, 0x0904, 0x1009, 0x2009, 0x000a, 0x0905, 0x100a, 0x200a,
  0x000b, 0x0906, 0x100b, 0x200b, 0x000c, 0x0907, 0x100c, 0x200c,
  0x000d, 0x0908, 0x100d, 0x200d, 0x000e, 0x0909, 0x100e, 0x200e,
  0x000f, 0x090a, 0x100f, 0x200f, 0x0010, 0x090b, 0x1010, 0x2010,
  0x0011, 0x0a04, 0x1011, 0x2011, 0x0012, 0x0a05, 0x1012, 0x2012,
  0x0013, 0x0a06, 0x1013, 0x2013, 0x0014, 0x0a07, 0x1014, 0x2014,
  0x0015, 0x0a08, 0x1015, 0x2015, 0x0016, 0x0a09, 0x1016, 0x2016,
  0x0017, 0x0a0a, 0x1017, 0x2017, 0x0018, 0x0a0b, 0x1018, 0x2018,
  0x0019, 0x0b04, 0x1019, 0x2019, 0x001a, 0x0b05, 0x101a, 0x201a,
  0x001b, 0x0b06, 0x101b, 0x201b, 0x001c, 0x0b07, 0x101c, 0x201c,
  0x001d, 0x0b08, 0x101d, 0x201d, 0x001e, 0x0b09, 0x101e, 0x201e,
  0x001f, 0x0b0a, 0x101f, 0x201f, 0x0020, 0x0b0b, 0x1020, 0x2020,
  0x0021, 0x0c04, 0x1021, 0x2021, 0x0022, 0x0c05, 0x1022, 0x2022,
  0x0023, 0x0c06, 0x1023, 0x2023, 0x0024, 0x0c07, 0x1024, 0x2024,
  0x0025, 0x0c08, 0x1025, 0x2025, 0x0026, 0x0c09, 0x1026, 0x2026,
  0x0027, 0x0c0a, 0x1027, 0x2027, 0x0028, 0x0c0b, 0x1028, 0x2028,
  0x0029, 0x0d04, 0x1029, 0x2029, 0x002a, 0x0d05, 0x102a, 0x202a,
  0x002b, 0x0d06, 0x102b, 0x202b, 0x002c, 0x0d07, 0x102c, 0x202c,
  0x002d, 0x0d08, 0x102d, 0x202d, 0x002e, 0x0d09, 0x102e, 0x202e,
  0x002f, 0x0d0a, 0x102f, 0x202f, 0x0030, 0x0d0b, 0x1030, 0x2030,
  0x0031, 0x0e04, 0x1031, 0x2031, 0x0032, 0x0e05, 0x1032, 0x2032,
  0x0033, 0x0e06, 0x1033, 0x2033, 0x0034, 0x0e07, 0x1034, 0x2034,
  0x0035, 0x0e08, 0x1035, 0x2035, 0x0036, 0x0e09, 0x1036, 0x2036,
  0x0037, 0x0e0a, 0x1037, 0x2037, 0x0038, 0x0e0b, 0x1038, 0x2038,
  0x0039, 0x0f04, 0x1039, 0x2039, 0x003a, 0x0f05, 0x103a, 0x203a,
  0x003b, 0x0f06, 0x103b, 0x203b, 0x003c, 0x0f07, 0x103c, 0x203c,
  0x0801, 0x0f08, 0x103d, 0x203d, 0x1001, 0x0f09, 0x103e, 0x203e,
  0x1801, 0x0f0a, 0x103f, 0x203f, 0x2001, 0x0f0b, 0x1040, 0x2040
};

// Helper class for decompression
class SnappyDecompressor {
private:
	Source*       reader_;         // Underlying source of bytes to decompress
	const char*   ip_;             // Points to next buffered byte
	const char*   ip_limit_;       // Points just past buffered bytes
	uint32        peeked_;         // Bytes peeked from reader (need to skip)
	bool          eof_;            // Hit end of input without an error?
	char          scratch_[kMaximumTagLength];  // See RefillTag().

	// Ensure that all of the tag metadata for the next tag is available
	// in [ip_..ip_limit_-1].  Also ensures that [ip,ip+4] is readable even
	// if (ip_limit_ - ip_ < 5).
	//
	// Returns true on success, false on error or end of input.
	bool RefillTag();

public:
	explicit SnappyDecompressor(Source* reader) : reader_(reader), ip_(NULL), ip_limit_(NULL), peeked_(0), eof_(false) {}

	~SnappyDecompressor() {
		// Advance past any bytes we peeked at from the reader
		reader_->Skip(peeked_);
	}

	// Returns true iff we have hit the end of the input without an error.
	bool eof() const {
		return eof_;
	}

	// Read the uncompressed length stored at the start of the compressed data.
	// On succcess, stores the length in *result and returns true.
	// On failure, returns false.
	bool ReadUncompressedLength(uint32* result) {
		ISO_ASSERT(ip_ == NULL);       // Must not have read anything yet
		// Length is encoded in 1..5 bytes
		*result = 0;
		uint32 shift = 0;
		while (true) {
			if (shift >= 32)
				return false;
			size_t n;
			const char* ip = reader_->Peek(&n);
			if (n == 0)
				return false;
			const unsigned char c = *(reinterpret_cast<const unsigned char*>(ip));
			reader_->Skip(1);
			*result |= static_cast<uint32>(c & 0x7f) << shift;
			if (c < 128)
				break;
			shift += 7;
		}
		return true;
	}

	// Process the next item found in the input.
	// Returns true if successful, false on error or end of input.
	template<class Writer>	void DecompressAllTags(Writer* writer) {
		const char* ip = ip_;

		// We could have put this refill fragment only at the beginning of the loop.
		// However, duplicating it at the end of each branch gives the compiler more
		// scope to optimize the <ip_limit_ - ip> expression based on the local
		// context, which overall increases speed.
	#define MAYBE_REFILL() if (ip_limit_ - ip < kMaximumTagLength) { ip_ = ip; if (!RefillTag()) return; ip = ip_; }

		MAYBE_REFILL();
		for (;; ) {
			const unsigned char c = *(reinterpret_cast<const unsigned char*>(ip++));

			if ((c & 0x3) == LITERAL) {
				size_t literal_length = (c >> 2) + 1u;
				if (writer->TryFastAppend(ip, ip_limit_ - ip, literal_length)) {
					ISO_ASSERT(literal_length < 61);
					ip += literal_length;
					// NOTE(user): There is no MAYBE_REFILL() here, as TryFastAppend()
					// will not return true unless there's already at least five spare
					// bytes in addition to the literal.
					continue;
				}
				if (unlikely(literal_length >= 61)) {
					// Long literal.
					const size_t literal_length_length = literal_length - 60;
					literal_length = (*(uint32le*)(ip) & wordmask[literal_length_length]) + 1;
					ip += literal_length_length;
				}

				size_t avail = ip_limit_ - ip;
				while (avail < literal_length) {
					if (!writer->Append(ip, avail))
						return;
					literal_length -= avail;
					reader_->Skip(peeked_);
					size_t n;
					ip = reader_->Peek(&n);
					avail = n;
					peeked_ = (uint32)avail;
					if (avail == 0)
						return;  // Premature end of input
					ip_limit_ = ip + avail;
				}
				if (!writer->Append(ip, literal_length))
					return;
				ip += literal_length;
				MAYBE_REFILL();
			} else {
				const uint32 entry = char_table[c];
				const uint32 trailer = *(uint32le*)ip & wordmask[entry >> 11];
				const uint32 length = entry & 0xff;
				ip += entry >> 11;

				// copy_offset/256 is encoded in bits 8..10.  By just fetching
				// those bits, we get copy_offset (since the bit-field starts at
				// bit 8).
				const uint32 copy_offset = entry & 0x700;
				if (!writer->AppendFromSelf(copy_offset + trailer, length))
					return;
				MAYBE_REFILL();
			}
		}

	#undef MAYBE_REFILL
	}
};

bool SnappyDecompressor::RefillTag() {
	const char* ip = ip_;
	if (ip == ip_limit_) {
		// Fetch a new fragment from the reader
		reader_->Skip(peeked_);   // All peeked bytes are used up
		size_t n;
		ip = reader_->Peek(&n);
		peeked_ = uint32(n);
		if (n == 0) {
			eof_ = true;
			return false;
		}
		ip_limit_ = ip + n;
	}

	// Read the tag character
	ISO_ASSERT(ip < ip_limit_);
	const unsigned char c = *(reinterpret_cast<const unsigned char*>(ip));
	const uint32 entry = char_table[c];
	const uint32 needed = (entry >> 11) + 1;  // +1 byte for 'c'
	ISO_ASSERT(needed <= sizeof(scratch_));

	// Read more bytes from reader if needed
	uint32 nbuf = ip_limit_ - ip;
	if (nbuf < needed) {
		// Stitch together bytes from ip and reader to form the word
		// contents.  We store the needed bytes in "scratch_".  They
		// will be consumed immediately by the caller since we do not
		// read more than we need.
		memmove(scratch_, ip, nbuf);
		reader_->Skip(peeked_);  // All peeked bytes are used up
		peeked_ = 0;
		while (nbuf < needed) {
			size_t length;
			const char* src = reader_->Peek(&length);
			if (length == 0) return false;
			uint32 to_add = min<uint32>(needed - nbuf, (uint32)length);
			memcpy(scratch_ + nbuf, src, to_add);
			nbuf += to_add;
			reader_->Skip(to_add);
		}
		ISO_ASSERT(nbuf == needed);
		ip_ = scratch_;
		ip_limit_ = scratch_ + needed;
	} else if (nbuf < kMaximumTagLength) {
		// Have enough bytes, but move into scratch_ so that we do not
		// read past end of input
		memmove(scratch_, ip, nbuf);
		reader_->Skip(peeked_);  // All peeked bytes are used up
		peeked_ = 0;
		ip_ = scratch_;
		ip_limit_ = scratch_ + nbuf;
	} else {
		// Pass pointer to buffer returned by reader_.
		ip_ = ip;
	}
	return true;
}

template<typename Writer> static bool InternalUncompress(Source* r, Writer* writer) {
	// Read the uncompressed length from the front of the compressed input
	SnappyDecompressor decompressor(r);
	uint32 uncompressed_len = 0;
	return decompressor.ReadUncompressedLength(&uncompressed_len)
		&& InternalUncompressAllTags(&decompressor, writer, uncompressed_len);
}

template<typename Writer> static bool InternalUncompressAllTags(SnappyDecompressor* decompressor,
	Writer* writer,
	uint32 uncompressed_len) {
	writer->SetExpectedLength(uncompressed_len);

	// Process the entire input
	decompressor->DecompressAllTags(writer);
	return decompressor->eof() && writer->CheckLength();
}

bool GetUncompressedLength(Source* source, uint32* result) {
	SnappyDecompressor decompressor(source);
	return decompressor.ReadUncompressedLength(result);
}

size_t Compress(Source* reader, Sink* writer) {
	size_t written = 0;
	size_t N = reader->Available();
	char ulength[Varint::kMax32];
	char* p = Varint::Encode32(ulength, (uint32)N);
	writer->Append(ulength, p - ulength);
	written += (p - ulength);

	internal::WorkingMemory wmem;
	char* scratch = NULL;
	char* scratch_output = NULL;

	while (N > 0) {
		// Get next block to compress (without copying if possible)
		size_t fragment_size;
		const char* fragment = reader->Peek(&fragment_size);
		ISO_ASSERT(fragment_size != 0);  // premature end of input
		const size_t num_to_read = min(N, kBlockSize);
		size_t bytes_read = fragment_size;

		size_t pending_advance = 0;
		if (bytes_read >= num_to_read) {
			// Buffer returned by reader is large enough
			pending_advance = num_to_read;
			fragment_size = num_to_read;
		} else {
			// Read into scratch buffer
			if (scratch == NULL) {
				// If this is the last iteration, we want to allocate N bytes
				// of space, otherwise the max possible kBlockSize space.
				// num_to_read contains exactly the correct value
				scratch = new char[num_to_read];
			}
			memcpy(scratch, fragment, bytes_read);
			reader->Skip(bytes_read);

			while (bytes_read < num_to_read) {
				fragment = reader->Peek(&fragment_size);
				size_t n = min<size_t>(fragment_size, num_to_read - bytes_read);
				memcpy(scratch + bytes_read, fragment, n);
				bytes_read += n;
				reader->Skip(n);
			}
			ISO_ASSERT(bytes_read == num_to_read);
			fragment = scratch;
			fragment_size = num_to_read;
		}
		ISO_ASSERT(fragment_size == num_to_read);

		// Get encoding table for compression
		int table_size;
		uint16* table = wmem.GetHashTable(num_to_read, &table_size);

		// Compress input_fragment and append to dest
		const int max_output = (int)MaxCompressedLength(num_to_read);

		// Need a scratch buffer for the output, in case the byte sink doesn't
		// have room for us directly.
		if (scratch_output == NULL) {
			scratch_output = new char[max_output];
		} else {
			// Since we encode kBlockSize regions followed by a region
			// which is <= kBlockSize in length, a previously allocated
			// scratch_output[] region is big enough for this iteration.
		}
		char* dest = writer->GetAppendBuffer(max_output, scratch_output);
		char* end = internal::CompressFragment(fragment, fragment_size, dest, table, table_size);
		writer->Append(dest, end - dest);
		written += (end - dest);

		N -= num_to_read;
		reader->Skip(pending_advance);
	}

	delete[] scratch;
	delete[] scratch_output;

	return written;
}

// -----------------------------------------------------------------------
// Flat array interfaces
// -----------------------------------------------------------------------

// A type that writes to a flat array.
// Note that this is not a "ByteSink", but a type that matches the
// Writer template argument to SnappyDecompressor::DecompressAllTags().
class SnappyArrayWriter {
private:
	char* base_;
	char* op_;
	char* op_limit_;

public:
	inline explicit SnappyArrayWriter(char* dst) : base_(dst), op_(dst) {}

	inline void SetExpectedLength(size_t len) {
		op_limit_ = op_ + len;
	}

	inline bool CheckLength() const {
		return op_ == op_limit_;
	}

	inline bool Append(const char* ip, size_t len) {
		char* op = op_;
		const size_t space_left = op_limit_ - op;
		if (space_left < len)
			return false;
		memcpy(op, ip, len);
		op_ = op + len;
		return true;
	}

	inline bool TryFastAppend(const char* ip, size_t available, size_t len) {
		char* op = op_;
		const size_t space_left = op_limit_ - op;
		if (len <= 16 && available >= 16 + kMaximumTagLength && space_left >= 16) {
			// Fast path, used for the majority (about 95%) of invocations.
			UnalignedCopy64(ip, op);
			UnalignedCopy64(ip + 8, op + 8);
			op_ = op + len;
			return true;
		} else {
			return false;
		}
	}

	inline bool AppendFromSelf(size_t offset, size_t len) {
		char* op = op_;
		const size_t space_left = op_limit_ - op;

		// Check if we try to append from before the start of the buffer.
		// Normally this would just be a check for "produced < offset",
		// but "produced <= offset - 1u" is equivalent for every case
		// except the one where offset==0, where the right side will wrap around
		// to a very big number. This is convenient, as offset==0 is another
		// invalid case that we also want to catch, so that we do not go
		// into an infinite loop.
		ISO_ASSERT(op >= base_);
		size_t produced = op - base_;
		if (produced <= offset - 1u)
			return false;
		if (len <= 16 && offset >= 8 && space_left >= 16) {
			// Fast path, used for the majority (70-80%) of dynamic invocations.
			UnalignedCopy64(op - offset, op);
			UnalignedCopy64(op - offset + 8, op + 8);
		} else {
			if (space_left >= len + kMaxIncrementCopyOverflow) {
				IncrementalCopyFastPath(op - offset, op, len);
			} else {
				if (space_left < len)
					return false;
				IncrementalCopy(op - offset, op, len);
			}
		}

		op_ = op + len;
		return true;
	}
};

bool RawUncompress(const char* compressed, size_t n, char* uncompressed) {
	ByteArraySource reader(compressed, n);
	return RawUncompress(&reader, uncompressed);
}

bool RawUncompress(Source* compressed, char* uncompressed) {
	SnappyArrayWriter output(uncompressed);
	return InternalUncompress(compressed, &output);
}

bool Uncompress(const char* compressed, size_t n, malloc_block &output) {
	size_t ulength;
	if (!GetUncompressedLength(compressed, n, &ulength))
		return false;
	output.create(ulength);
	return RawUncompress(compressed, n, output);
}

// A Writer that drops everything on the floor and just does validation
class SnappyDecompressionValidator {
	size_t expected_;
	size_t produced_;

public:
	inline SnappyDecompressionValidator() : produced_(0) {}
	inline void SetExpectedLength(size_t len) {
		expected_ = len;
	}
	inline bool CheckLength() const {
		return expected_ == produced_;
	}
	inline bool Append(const char* ip, size_t len) {
		produced_ += len;
		return produced_ <= expected_;
	}
	inline bool TryFastAppend(const char* ip, size_t available, size_t length) {
		return false;
	}
	inline bool AppendFromSelf(size_t offset, size_t len) {
		// See SnappyArrayWriter::AppendFromSelf for an explanation of the "offset - 1u" trick.
		if (produced_ <= offset - 1u)
			return false;
		produced_ += len;
		return produced_ <= expected_;
	}
};

bool IsValidCompressedBuffer(const char* compressed, size_t n) {
	ByteArraySource reader(compressed, n);
	SnappyDecompressionValidator writer;
	return InternalUncompress(&reader, &writer);
}

void RawCompress(const char* input, size_t input_length, char* compressed, size_t* compressed_length) {
	ByteArraySource reader(input, input_length);
	UncheckedByteArraySink writer(compressed);
	Compress(&reader, &writer);

	// Compute how many bytes were added
	*compressed_length = (writer.CurrentDestination() - compressed);
}

size_t Compress(const char* input, size_t input_length, malloc_block &compressed) {
	// Pre-grow the buffer to the max length of the compressed output
	compressed.create(MaxCompressedLength(input_length));

	size_t compressed_length;
	RawCompress(input, input_length, compressed, &compressed_length);
	compressed.resize(compressed_length);
	return compressed_length;
}

} // end namespace snappy

