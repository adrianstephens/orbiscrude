#include "object.h"
#include "triggers.h"
#include "variable.h"
#include "utilities.h"
#include "crc_dictionary.h"

namespace iso {
	template<> void TypeHandlerCRC<ISO_CRC("ConsoleLog", 0xec97355a)>::Create(const CreateParams &cp, ISO_ptr<void> t) {
#ifndef RELEASE
		if (const char *fmt = ISO_browser(t).GetMember(ISO_CRC("log", 0x8f3f68c5)).GetString()) {
			Object	*obj	= cp.obj;
			// timestamp
			int time = int(GetGlobalTime());
			// variables
			if (const char *srce = ::strchr(fmt, '%')) {
				// contact
				Object *contact_obj = GetTriggerContact(obj);
				// format
				fixed_string<1024> buffer;
				fixed_string<32> symbol;
				string_accum dest(buffer);
				do {
					// copy, advance
					dest.merge(fmt, srce - fmt);
					fmt = srce++;
					if (*srce != '%') {
						// symbol
						char *_dest = symbol;
						while (*srce && *srce != '%' && *srce != ' ')
							*_dest++ = *srce++;
						*_dest = 0;
						// ids
						crc32 symbol_id(symbol);
						if (symbol_id == ISO_CRC("_obj", 0x0b0dcd96)) {
							dest.format("0x%08x \"%s\"", obj, LookupCRC32(obj->GetName(), "n/a"));

						} else if (symbol_id == ISO_CRC("_trigger_obj", 0x1ce0a43c)) {
							if (contact_obj)
								dest.format("0x%08x \"%s\"", contact_obj, LookupCRC32(contact_obj->GetName(), "n/a"));
							else
								dest << "(n/a)";

						}/* else {
							// query
							int value;
							if (
								Variable::Query(obj, symbol_id, &value, 0) ||
								(obj->Parent() && Variable::Query(obj->Parent(), symbol_id, &value, 0)) ||
								(contact_obj && Variable::Query(contact_obj, symbol_id, &value, 0))
							)
								dest << value;
							else
								dest << "(n/a)";

						}*/
						if (*srce == '%')
							++srce;
						fmt = srce;
					} else {
						dest << '%';
						fmt = ++srce;
					}
				} while (srce = ::strchr(srce, '%'));
				dest << fmt;
				ISO_TRACEF("NFO[ConsoleLog] %02i:%02i:%02i.%i: %s\n", time / (60 * 60), (time / 60) % 60, time % 60, int((GetGlobalTime() - time) * 10), (const char*)buffer);
			} else
				ISO_TRACEF("NFO[ConsoleLog] %02i:%02i:%02i.%i: %s\n", time / (60 * 60), (time / 60) % 60, time % 60, int((GetGlobalTime() - time) * 10), fmt);
		}
#endif
	}
	TypeHandlerCRC<ISO_CRC("ConsoleLog", 0xec97355a)> thConsoleLog;
}
