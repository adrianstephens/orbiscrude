#include "ebml.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	EBMB Reader
//-----------------------------------------------------------------------------

uint64 read_ebml_num(istream_ref stream, int &len) {
	int		first	= stream.getc();
	if (!first)
		return 0;

	int		mask	= 0x80;
	int		size	= 1;
	while (!(first & mask)) {
		mask >>= 1;
		size++;
	}

	len = size;

	uint64	num = first & ~mask;
	while (--size)
		num = (num << 8) | stream.getc();

	return num;
}

uint64 EBMLreader::read_uint() {
	uint64	num = 0;
	for (uint32 i = 0, n = uint32(size); i < n; i++)
		num = (num << 8) | istream_chain::getc();
	return num;
}

int64 EBMLreader::read_int() {
	int64	num = istream_chain::getc();
	bool	neg = (num & 0x80) != 0;
	for (int i = 1, n = int(size); i < n; i++)
		num = (num << 8) | istream_chain::getc();
	return neg ? num - (1LL << (8 * int(size))) : num;
}

double EBMLreader::read_float() {
	if (size == 4)
		return istream_chain::get<floatbe>();
	else
		return istream_chain::get<doublebe>();
}

string EBMLreader::read_ascii() {
	int		size32	= int(size);
	string	s(size32);
	if (size32) {
		istream_chain::readbuff(s, size32);
		s[size32] = 0;
	}
	return s;
}

malloc_block EBMLreader::read_binary() {
	return malloc_block(istream_chain::t, size);
}

//-----------------------------------------------------------------------------
//	EBMB Writer
//-----------------------------------------------------------------------------

int EBMLwriter::len_num(uint64 num) {
	int		size = 1;
	while ((num + 1) >> (size * 7))
		size++;
	return size;
}

void EBMLwriter::write_id(uint64 id) {
	int		size = 1;
	while (id >> (size * 7 + 1))
		size++;

	while (size--)
		ostream_chain::putc(id >> size * 8);
}

void EBMLwriter::write_packed_num(uint64 num, int size) {
    num |= 1ULL << (size * 7);
	while (size--)
		ostream_chain::putc(num >> size * 8);
}

void EBMLwriter::write_void(uint32 id, uint64 size) {
	streamptr	fp = ostream_chain::tell();
	write_id(EBMLHeader::ID_Void);
	if (size < 10)
		write_packed_num(size - 2, 1);
	else
		write_packed_num(size - 9, 8);
	ostream_chain::seek(fp + size);
}

void EBMLwriter::write_uint(uint32 id, uint64 n)	{
	int		size = 1;
	while (n >> (size * 8))
		size++;
	write_id(id);
	write_packed_num(size, 1);
	while (size--)
		ostream_chain::putc(n >> (size * 8));
}
/*
void BMLwrite::write_sint(uint32 id, int64 n)	{
	bool	neg		= n < 0;
	int		count	= 1;
	while (n >> (count * 8))
		count++;
	write_id(id);
	write_num(count, len_num(count));
	for (int i = 0; i < count; i++)
		file.putc(n >> (count * 8));
}
*/
