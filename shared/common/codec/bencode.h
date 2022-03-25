#ifndef BENCODE_H
#define BENCODE_H

//-----------------------------------------------------------------------------
//	Bencode
//	used by BitTorrent for storing and transmitting loosely structured data
//-----------------------------------------------------------------------------

#include "base/defs.h"
#include "stream.h"

enum bencode_type {
	BENCODE_UNKNOWN,
	BENCODE_INT,
	BENCODE_BYTES,
	BENCODE_LIST,
	BENCODE_DICT,
};

class bencode_item {
public:

	static	bencode_item*	parse(iso::istream_ref file, int c = 0);

	virtual ~bencode_item()	{}
	virtual	bencode_type	type()						{ return BENCODE_UNKNOWN;	}
	virtual	int				count()						{ return 0;					}
	virtual bencode_item*	operator[](int i)			{ return 0;					}
	virtual bencode_item*	operator[](const char *s)	{ return 0;					}
	virtual bool			read(iso::istream_ref file, int c)	= 0;
	virtual bool			write(iso::ostream_ref file)		= 0;
};

class bencode_browser {
	bencode_item *item;
public:
	bencode_browser(bencode_item *_item) : item(_item)	{}
	bencode_type	type()						const	{ return item ? item->type() : BENCODE_UNKNOWN;	}
	int				count()						const	{ return item ? item->count() : 0;				}
	bencode_browser	operator[](int i)			const	{ return item ? (*item)[i] : 0;					}
	bencode_browser	operator[](const char *s)	const	{ return item ? (*item)[s] : 0;					}
	bool			write(iso::ostream_ref file)	const	{ return item && item->write(file);				}
};

#endif// BENCODE_H
