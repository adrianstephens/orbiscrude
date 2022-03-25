#include "online.h"

using namespace iso;

OnlineController			*OnlineController::host, *OnlineController::current;
e_list<OnlineController>	OnlineController::list;
Random						OnlineController::random;

void OnlineController::UpdateQueue::Flush(float time, MPSYSTEM system, int flags) {
	if (packet_offset) {
		packet->time	= time;
		void*	data	= packet;
		size_t	size	= sizeof(UpdateMsg) + packet_offset - 1024;

		packet			= &packet_buffer[packet == packet_buffer];
		packet_offset	= 0;
		id_last			= ~0;
		multiplayer.Send(system, data, size, flags);
	}
}

void* OnlineController::UpdateQueue::Queue(float time, OnlineController *online, crc32 id, size_t size) {
	uint32 total_size = size + (id == id_last ? 0 : sizeof(id)) + 1;
	if (packet_offset + total_size > num_elements(packet->data)) {
		online->Flush(time);
		total_size = size + sizeof(id) + 1;
	}
	uint8 *p = packet->data + packet_offset;
	if (id == id_last) {
		*p = size;
	} else {
		*p = size | 128;
		*(iso_unaligned(crc32)*)(p + 1) = id_last = id;
	}
	packet_offset += total_size;
	return p + total_size - size;
}

void OnlineController::Flush(float time) {
	update_queue[0].Flush(time, system, MP_SD_UNRELIABLE);
	update_queue[1].Flush(time, system, MP_SD_RELIABLE);
	flush_time = time + flush_period;
}

void OnlineController::Reset() {
	clear();
	random.Init(0x31415926);
	update_queue[0].Reset();
	update_queue[1].Reset();
	flush_time = 0;
}

bool OnlineController::Receive(const void *data, size_t size) {
	switch (int id = *(uint8*)data) {
		case MPID_UPDATE: {
			const iso_unaligned(UpdateMsg) *update = (UpdateMsg*)data;
			float t = update->time;
			OnlineEntity *entity = NULL;

			const uint8 *p = update->data;
			const uint8 *e = (uint8*)update + size;
			while (p < e) {
				uint8 s = *p++;
				if (s & 128) {
					crc32 id = *(iso_unaligned(crc32)*)p;
					entity = find(id);
					p += sizeof(crc32);
					s &= 127;
				}
				if (entity)
					(*entity)(this, t, p);
				p += s;
			}
			break;
		}
	}
	return false;
}

void OnlineController::MigrateHost(OnlineController *_host) {
	while (OnlineEntity *entity = host->pop_head())
		entity->SetOnline(_host);
	host = _host;
}

OnlineController *OnlineController::Get(MPSYSTEM system) {
	for (e_list<OnlineController>::iterator i = list.begin(); i != list.end(); ++i) {
		if (i->system == system)
			return i;
	}
	return 0;
}

//-------------------------------------

OnlineEntity::~OnlineEntity() {
	if (online)
		online->remove(this);
}

void OnlineEntity::SetOnline(OnlineController *_online) {
	if (online = _online) {
		while (id.blank() || online->find(id))
			id = OnlineController::RandomID();
		online->insert(this);
	}
}

void deleter(OnlineEntity *poe) {
	delete poe;
}
