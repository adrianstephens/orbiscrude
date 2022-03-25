#ifndef HASH_STREAM_H
#define HASH_STREAM_H

#include "stream.h"

namespace iso {

//-----------------------------------------------------------------------------
//	block_writer/block_reader
//-----------------------------------------------------------------------------

template<int N> struct stream_block {
	enum {BLOCK_SIZE = N};
	uint8		block[N];
	streamptr	p;
	stream_block() : p(0)	{}
	memory_block		buffered()			{ return memory_block(block, p % N); }
	const_memory_block	buffered()	const	{ return const_memory_block(block, p % N); }
};

// has a process(uint8 block[N]) for one block at a time

template<typename T, int N> struct block_writer : writer_mixin<block_writer<T, N> >, stream_block<N> {
	using stream_block<N>::block;
	using stream_block<N>::p;
	size_t		writebuff(const void *buffer, size_t size);
	streamptr	tell()		const	{ return p;	}
};

template<typename T, int N> size_t block_writer<T, N>::writebuff(const void *buffer, size_t size) {
	uint32	n = 0;

	if (uint32 o = p % N) {
		n = N - o;
		if (size < n) {
			memcpy(block + o, buffer, size);
			p += size;
			return size;
		}
		memcpy(block + o, buffer, n);
		static_cast<T*>(this)->process(block);
	}

	buffer	= (uint8*)buffer + n;
	size_t remain = size - n;
	while (remain >= N) {
		static_cast<T*>(this)->process((uint8*)buffer);
		buffer	= (uint8*)buffer + N;
		remain -= N;
	}
	memcpy(block, buffer, remain);

	p += size;
	return size;
}

// has a process(uint8 block[N], size_t len) for multiple blocks

template<typename T, int N> struct block_writer2 : writer_mixin<block_writer2<T, N> >, stream_block<N> {
	using stream_block<N>::block;
	using stream_block<N>::p;
	size_t		writebuff(const void *buffer, size_t size);
	streamptr	tell()		const	{ return p;	}
};

template<typename T, int N> size_t block_writer2<T, N>::writebuff(const void *buffer, size_t size) {
	uint32	n = 0;

	if (uint32 o = p % N) {
		n = N - o;
		if (size < n) {
			memcpy(block + o, buffer, size);
			p += size;
			return int(size);
		}
		memcpy(block + o, buffer, n);
		static_cast<T*>(this)->process(block, N);
	}

	size_t rem = (size - n) % N;
	static_cast<T*>(this)->process((const uint8*)buffer + n, size - n - rem);
	memcpy(block, (const uint8*)buffer + size - rem, rem);

	p += size;
	return int(size);
}

template<typename T, int N> struct block_reader : reader_mixin<block_reader<T, N> >, stream_block<N> {
	using stream_block<N>::block;
	using stream_block<N>::p;
	size_t		readbuff(void *buffer, size_t size);
	streamptr	tell()		const	{ return this->p;	}
};

template<typename T, int N> size_t block_reader<T, N>::readbuff(void *buffer, size_t size) {
	uint32	n = 0;

	if (uint32 o = p % N) {
		n = N - o;
		if (size < n) {
			memcpy(buffer, block + o, size);
			p += size;
			return int(size);
		}
		memcpy(buffer, block + o, n);
	}

	buffer	= (uint8*)buffer + n;
	size_t remain = size - n;
	while (remain >= N) {
		static_cast<T*>(this)->process((uint8*)buffer);
		buffer	= (uint8*)buffer + N;
		remain -= N;
	}

	if (remain) {
		static_cast<T*>(this)->process(block);
		memcpy(buffer, block, remain);
	}

	p += size;
	return int(size);
}

//-----------------------------------------------------------------------------
//	non blocked hash_writer/hash_reader
//-----------------------------------------------------------------------------

template<typename T, typename S = reader_intf> class hash_reader : public reader_mixin<hash_reader<T, S>> {
	S			file;
	T			hash;
public:
	typedef hash_reader B;
	template<typename S1, typename...P> hash_reader(S1&& file, P&&... p) : file(forward<S1>(file)), hash(forward<P>(p)...)  {}
	size_t	readbuff(void *buffer, size_t size)	{
		size_t	n = file.readbuff(buffer, size);
		hash.process(buffer, n);
		return n;
	}
	auto		digest()		{ return hash.digest(); }
	inline S&	get_stream()	{ return file; }
};

template<typename T, typename S = writer_intf> class hash_writer : public writer_mixin<hash_writer<T, S>> {
	S			file;
	T			hash;
public:
	typedef hash_writer B;
	template<typename S1, typename...P> hash_writer(S1&& file, P&&... p) : file(forward<S1>(file)), hash(forward<P>(p)...)  {}
	size_t	writebuff(const void *buffer, size_t size)	{
		size_t	n = file.writebuff(buffer, size);
		hash.process(buffer, n);
		return n;
	}
	auto		digest()		{ return hash.digest(); }
	inline S&	get_stream()	{ return file; }
};

template<typename T> class hash_sink : public writer_mixin<hash_sink<T>> {
	T			hash;
	typedef decltype(hash.digest())	D;
public:
	template<typename...P> hash_sink(P&&... p) : hash(forward<P>(p)...)  {}
	size_t	writebuff(const void *buffer, size_t size)	{
		hash.process(buffer, size);
		return size;
	}
	constexpr operator D() const { return hash.digest(); }
	D		digest()			{ return hash.digest(); }
	template<typename P> void	set(const P &p) { hash_sink	h; h.write(p); *this = h; }
};

} // namespace iso

#endif // HASH_STREAM_H
