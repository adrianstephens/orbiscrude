#include "crc_handler.h"
#include "triggers.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Spawner
//-----------------------------------------------------------------------------
enum AttachPoint {
	ATTACH_OBJECT_POSITION,
	ATTACH_OBJECT,
	ATTACH_TRIGGER_POSITION,
	ATTACH_TRIGGER_OBJECT_POSITION,
	ATTACH_TRIGGER_OBJECT
};

namespace ent {
	struct Spawner {
		ISO_ptr<void>	spawn;
		uint8			attach;
		crc32			bone;
		float3x4p		matrix;
	};
}

namespace iso {
	template<> void TypeHandler<ent::Spawner>::Create(const CreateParams &cp, ISO_ptr<ent::Spawner> data) {
		switch (data->attach) {
			case ATTACH_OBJECT_POSITION: {
				float3x4 tm = data->bone ? cp.obj->GetWorldMat() * cp.obj->GetBoneMat(data->bone) : cp.obj->GetWorldMat();
				_TypeHandler::FindAndCreate(tm * float3x4(data->matrix), data->spawn);
				break;
			}

			case ATTACH_OBJECT: {
				_TypeHandler::FindAndCreate(CreateParams(cp.obj, data->bone, data->matrix), data->spawn);
				break;
			}

			case ATTACH_TRIGGER_POSITION: {
				QueryTriggerMessage info_msg;
				cp.obj->Send(info_msg);
				if (info_msg.contact_obj)
					_TypeHandler::FindAndCreate(info_msg.contact_mat * float3x4(data->matrix), data->spawn);
				break;
			}

			case ATTACH_TRIGGER_OBJECT_POSITION: {
				QueryTriggerMessage info_msg;
				cp.obj->Send(info_msg);
				if (info_msg.contact_obj) {
					float3x4 tm = data->bone ? info_msg.contact_obj->GetWorldMat() * info_msg.contact_obj->GetBoneMat(data->bone) : info_msg.contact_obj->GetWorldMat();
					_TypeHandler::FindAndCreate(tm * float3x4(data->matrix), data->spawn);
				}
				break;
			}

			case ATTACH_TRIGGER_OBJECT: {
				QueryTriggerMessage info_msg;
				cp.obj->Send(info_msg);
				if (info_msg.contact_obj)
					_TypeHandler::FindAndCreate(CreateParams(info_msg.contact_obj, data->bone, float3x4(data->matrix)), data->spawn);
				break;
			}
		}
	}

	ISO_DEFUSERCOMPX(ent::Spawner, 4, "Spawner") {
		ISO_SETFIELDS4(0, spawn, attach, bone, matrix);
	}};
	static TypeHandler<ent::Spawner> thSpawner;
}