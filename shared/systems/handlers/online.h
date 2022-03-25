#ifndef ONLINE_H
#define ONLINE_H

#include "session.h"
#include "tree.h"
#include "object.h"
#include "utilities.h"

//-----------------------------------------------------------------------------
//	MPMessage
//-----------------------------------------------------------------------------
enum {
	MPID_PLAYERCONFIG	= 42,
	MPID_VETO,
	MPID_UPDATE,
};

//-----------------------------------------------------------------------------
//	OnlineEntity
//-----------------------------------------------------------------------------
class OnlineEntity;

class OnlineController
	: public iso::Handles2<OnlineController, iso::FrameEvent2>
	, public iso::Handles2<OnlineController, iso::WorldBegin>
	, public iso::e_link<OnlineController>
	, public iso::e_tree<OnlineEntity>
{
	static	OnlineController	*current, *host;
	static	iso::Random			random;
	static	iso::e_list<OnlineController>	list;

	// UpdateMsg
	struct UpdateMsg : iso::MPMessage<MPID_UPDATE> {
		iso::packed<float>	time;
		iso::uint8			data[1024];
	};
	// UpdateQueue
	struct UpdateQueue {
		UpdateMsg			packet_buffer[2];
		UpdateMsg*			packet;
		iso::uint32			packet_offset;
		iso::crc32			id_last;

		UpdateQueue() : packet(packet_buffer)	{ Reset(); }
		void	Reset()							{ id_last = ~0; packet_offset = 0; }
		void	Flush(float time, iso::MPSYSTEM system, int flags);
		void*	Queue(float time, OnlineController *online, iso::crc32 id, size_t size);
	};

	UpdateQueue		update_queue[2];
	float			flush_time, flush_period;
	iso::MPSYSTEM	system;

public:
	using iso::e_tree<OnlineEntity>::remove;

	OnlineController() : flush_period(0.05f), system(0)	{ list.push_back(this);	}

	void			Reset();
	void			Flush(float time);
	bool			TimeToFlush(float time)	const	{ return time > flush_time; }
	void*			GetPacketSpace(iso::crc32 id, size_t size, bool reliable)	{ return update_queue[reliable].Queue(flush_time, this, id, size); }
	bool			Receive(const void *data, size_t size);

	void operator()(iso::WorldBegin *ev) {
		Reset();
	}

	void operator()(iso::FrameEvent2 *ev) {
		if (TimeToFlush(ev->time))
			Flush(ev->time);
	}

	static iso::uint32		RandomID()		{ return (iso::uint32)(int)random;	}
	static OnlineController	*GetCurrent()	{ return current; }
	static void				MigrateHost(OnlineController *_host);
	static OnlineController	*Get(iso::MPSYSTEM system);
};

class OnlineEntity 
	: public iso::e_treenode<OnlineEntity>
	, public iso::callback<void(OnlineController *online, float time, const void *data)>
{
	iso::crc32			id;
	OnlineController	*online;
public:
	template<typename T> OnlineEntity(T *t, iso::crc32 _id, OnlineController *_online = OnlineController::GetCurrent())
		: iso::callback<void(OnlineController *online, float time, const void *data)>(t)
		, id(_id)
	{
		SetOnline(_online);
	}
	~OnlineEntity();

public:
	void				SetOnline(OnlineController *_online);
	OnlineController*	GetOnline()														const	{ return online; }
	void				AddPacket(const void *data, size_t size, bool reliable = false) const	{ memcpy(online->GetPacketSpace(id, size, reliable), data, size); }
	template<typename T> void AddPacket(const T &t, bool reliable = false) const				{ AddPacket(&t, sizeof(t), reliable); }

	bool			operator<(const OnlineEntity &oe2) const				{ return id < oe2.id;	}
	bool			operator<(const iso::crc32 id2)							{ return id < id2;		}
	friend bool		operator<(const iso::crc32 id2, const OnlineEntity &oe)	{ return id2 < oe.id;	}
};

void deleter(OnlineEntity *poe);

#endif
