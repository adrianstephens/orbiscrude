#ifndef REMOTE_H
#define REMOTE_H

#include "ISO.h"
#include "..\Systems\Communication\Connection.h"

namespace iso {
struct Remote : Connection::Sink {
	friend struct RemotePointer;
	Connection			*connection;
	string				spec;
	dynamic_array<ISO_ptr<RemotePointer> >	entries;

	virtual void Message(Connection::reason r, const char *buffer)	{}
//	template<typename T> bool FixReferences(const ISO_type *type, void *data);
//	bool		FixReferences(const ISO_type *type, void *data) { return BigEndian()
//		? FixReferences<bigendian>(type, data)
//		: FixReferences<littleendian>(type, data);
//	}
	void		Init(Connection *_connection, const char *_spec);
	int			Count();
	const char*	GetName(int i)		{ return entries[i].ID(); }
	ISO_browser	Index(int i)		{ return ISO_browser(entries[i]); }
	int			Find(tag id)		{ return -1; }
	bool		Update(ISO_browser &b, const char *spec2);

	Remote()	{}
	~Remote()	{}
};

struct RemotePointer {
	Remote			*remote;
	uint32			addr;
	const ISO_type	*type;
	ISO_ptr<void>	ptr;

	void		Init(Remote *_remote, uint32 _addr, const ISO_type *_type);
	void		Init(Remote *_remote);
	int			Count()				{ return 1;		}
	const char*	GetName( int i)		{ return NULL;	}
	int			Find(tag id)		{ return -1;	}
	ISO_browser	Index(int i);
	bool		Update(ISO_browser &b, const char *spec2);
//	bool		BigEndian()			{ return remote->BigEndian();	}
};

template<> struct ISO_def<Remote> : public TISO_virtual<Remote> {};
template<> struct ISO_def<RemotePointer> : public TISO_virtual<RemotePointer> {};

ISO_ptr<void> GetRemote(tag id, Connection *connection, const char *spec);
}


#endif REMOTE_H
