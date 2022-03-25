#ifndef STRIP_H
#define STRIP_H

#include "base/defs.h"
#include "base/array.h"
#include "base/vector.h"

class Strip: public iso::dynamic_array<int> {
public:
	bool	dir;
	Strip(size_t n = 0, bool dir = false) : dynamic_array<int>(n), dir(dir)	{}
	bool	Dir()			{ return dir;}
	bool	ReverseDir()	{ return (size() & 1) ^ dir;}
	void	Reverse();
};

class StripList : public iso::dynamic_array<Strip*> {
	size_t		total;
public:
	size_t		Total()		const		{ return total; }
	void		Append(Strip *strip)	{ push_back(strip);			total += strip->size(); }
	void		Prepend(Strip *strip)	{ insert(begin(), strip);	total += strip->size(); }

	StripList() : total(0)	{}
	StripList(iso::range<iso::array<iso::uint16,3>*> faces, iso::range<iso::stride_iterator<iso::float3p>> verts, iso::int32 maxstriplen = 0x7fffffff);
	~StripList();
};

#endif// STRIP_H
