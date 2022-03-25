#include "object.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	External
//-----------------------------------------------------------------------------
namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("External", 0xa124c3ee)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
		ISO_ptr<void> p = root["data"][ISO_browser(t).GetString()];
		cp.obj->AddEntities(p);
	}
	template<> void TypeHandlerCRC<ISO_CRC("External", 0xa124c3ee)>::Load(LoadParams &lp, ISO_ptr<void> t) {
		if (ISO_ptr<void> p = lp.Hold(root["data"], ISO_browser(t).GetString()))
			_TypeHandler::FindAndLoad(lp, p);
	}
	static TypeHandlerCRC<ISO_CRC("External", 0xa124c3ee)> thExternal;
}
