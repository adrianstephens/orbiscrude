#ifndef CIPHER_MODE_H
#define CIPHER_MODE_H

#include "hashes/hash_stream.h"

namespace iso {

inline void xor_block(uint8 *x, const uint8 *a, const uint8 *b, int n) {
	for (int i = 0; i < n; i++)
		x[i] = a[i] ^ b[i];
}

inline void inc_block(uint8 *x, int n) {
	for (int i = n; i-- && ++x[i] != 0;);
}

template<typename T> struct decryptor : T {
	template<typename TP> decryptor(TP tp) : T(tp) {}
	void	process(arbitrary_ptr data)					{ T::decrypt(data, data); }
	void	process(arbitrary_ptr data, size_t size)	{ T::decrypt(data, data, size); }
};

template<typename T> struct encryptor : T {
	template<typename TP> encryptor(TP tp) : T(tp) {}
	void	process(arbitrary_ptr data)					{ T::encrypt(data, data); }
	void	process(arbitrary_ptr data, size_t size)	{ T::encrypt(data, data, size); }
};

template<typename T, int N, typename W> struct Stream_encrypt : buffered_writer0<N>, writer_mixin<Stream_encrypt<T, N, W>> {
	typedef buffered_writer0<N>	B;
	T		t;
	W		w;
	
	auto	writer() {
		return [this](const void *buffer, size_t size) {
			t.encrypt((uint8*)buffer, size);
			return w.writebuff(buffer, size);
		};
	}

	template<typename TP, typename WP> Stream_encrypt(TP tp, WP wp) : t(tp), w(wp) {}
	~Stream_encrypt()										{ B::flush(writer()); }
	size_t		writebuff(const void *buffer, size_t size)	{ return B::buffered_write(writer(), buffer, size); }
	int			putc(int c)									{ return B::buffered_putc(writer(), c); }
	void		seek(streamptr offset)						{ if (B::buffered_preseek(writer())) w.seek(offset); }
	void		seek_cur(streamptr offset)					{ seek(B::current + offset); }
	void		seek_end(streamptr offset)					{ seek(w.length() + offset); }
};

template<typename T, int N, typename R> struct Stream_decrypt : buffered_reader0<N>, reader_mixin<Stream_decrypt<T, N, R>> {
	typedef buffered_reader0<N> B;
	T		t;
	R		r;
	
	auto	reader() {
		return [this](void *buffer, size_t size) {
			auto	n = r.readbuff(buffer, size);
			t.decrypt((uint8*)buffer, n);
			return n;
		};
	}

	template<typename TP, typename RP> Stream_decrypt(TP tp, RP rp) : t(tp), r(rp) {}
	size_t		readbuff(void *buffer, size_t size)	{ return B::buffered_read(reader(), buffer, size); }
	int			getc()								{ return B::buffered_getc(reader()); }
	void		seek(streamptr offset)				{ if (B::buffered_preseek(offset)) r.seek(offset); }
	void		seek_cur(streamptr offset)			{ seek(B::current + offset); }
	void		seek_end(streamptr offset)			{ seek(r.length() + offset); }
};

} //namespace iso

#endif // CIPHER_MODE_H
