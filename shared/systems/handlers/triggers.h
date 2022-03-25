#ifndef TRIGGERS_H
#define TRIGGERS_H

#include "object.h"
#include "crc_handler.h"

namespace iso {
	enum {
		QUERY_TRIGGER = ISO_CRC("QUERY_TRIGGER", 0x8ffb5ecc),
		DISABLE_TRIGGER = ISO_CRC("DISABLE_TRIGGER", 0xcdde7b73),
	};

	// QueryTrigger
	template<> struct EventMessage<QUERY_TRIGGER> {
		Object *contact_obj;
		float3x4 contact_mat;
		unsigned contact_mask;
		void *contact_param;
		EventMessage()
			: contact_obj(NULL)
			, contact_mask(0)
			, contact_param(NULL)
		{}
	};
	typedef EventMessage<QUERY_TRIGGER> QueryTriggerMessage;
	typedef EventHandlerCRC<QUERY_TRIGGER> QueryTriggerHandler;

	// DisableTrigger
	template<> struct EventMessage<DISABLE_TRIGGER> {
		bool disable;
		EventMessage(bool _disable)
			: disable(_disable)
		{}
	};
	typedef EventMessage<DISABLE_TRIGGER> DisableTriggerMessage;
	typedef EventHandlerCRC<DISABLE_TRIGGER> DisableTriggerHandler;
}

iso::Object* GetTriggerContact(iso::Object *obj, iso::float3x4 *contact_mat = NULL, unsigned *contact_mask = NULL);

#endif // TRIGGERS_H