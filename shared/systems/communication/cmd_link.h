#ifndef CMD_LINK_H
#define CMD_LINK_H

#if USE_CMDLINK

#include "base/defs.h"
#include "iso/iso.h"
#include "events.h"
#include "isolink.h"

//-----------------------------------------------------------------------------
//	CmdHandler
//-----------------------------------------------------------------------------

typedef iso::virtfunc<bool(isolink_handle_t handle, const ISO::Browser &b, bool immediate)>	VF;

class _CmdHandler : public iso::static_list<_CmdHandler>, public VF {
	iso::crc32 id;
public:
	template<typename T> _CmdHandler(iso::crc32 _id, T *t) : VF(t), id(_id)	{}
	static _CmdHandler* Find(iso::crc32 id);
};

template<iso::uint32 CRC> class CmdHandler : public _CmdHandler {
	void Process(ISO::Browser b)	{}
	void Process(isolink_handle_t handle, ISO::Browser b) {
		Process(b), isolink_close(handle);
	}
	bool Process(ISO::Browser b, bool immediate) {
		if (!immediate)
			Process(b);
		return false;
	}
	bool Process(isolink_handle_t handle, ISO::Browser b, bool immediate) {
		if (immediate) {
			if (Process(b, immediate)) {
				isolink_close(handle);
				return true;
			}
		} else {
			Process(handle, b);
		}
		return false;
	}

public:
	bool	operator()(isolink_handle_t handle, const ISO::Browser &b, bool immediate) {
		return Process(handle, b, immediate);
	}
	CmdHandler() : _CmdHandler(CRC, this) {}
};

bool CmdLinkServe();
void CmdLinkFlush();

#else
class _CmdHandler {
public:
	_CmdHandler(iso::crc32 _id, void *t)	{}
};
#endif // USE_CMDLINK

#endif // CMD_LINK_H