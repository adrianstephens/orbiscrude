#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include "base/defs.h"

namespace iso {

enum ControllerButton {
	CBUT_1DOWN				= 1<<0,
	CBUT_2DOWN				= 1<<1,
	CBUT_3DOWN				= 1<<2,
	CBUT_4DOWN				= 1<<3,

	CBUT_PAUSE				= 0,
	CBUT_ACTION				= 0,
	CBUT_ACTION2			= 0,
	CBUT_ACTION3			= 0,
	CBUT_RETURN				= 0,
	CBUT_SPECIAL			= 0,
	CBUT_BACK				= 0,
	CBUT_UP					= 0,
	CBUT_DOWN				= 0,
	CBUT_LEFT				= 0,
	CBUT_RIGHT				= 0,

	CBUT_PTR				= CBUT_1DOWN,
};
enum ControllerAnalog {
	_CANA_FINGERS,
	CANA_FINGER1_X	= _CANA_FINGERS,
	CANA_FINGER1_Y,
	CANA_FINGER2_X,
	CANA_FINGER2_Y,
	CANA_FINGER3_X,
	CANA_FINGER3_Y,
	CANA_FINGER4_X,
	CANA_FINGER4_Y,

	CANA_PTR_X,	// not clear on lift-up
	CANA_PTR_Y,

	_CANA_ATTITUDE,
	CANA_ROLL		= _CANA_ATTITUDE,
	CANA_PITCH,
	CANA_YAW,

	_CANA_ACCEL,
	CANA_ACCEL_X	= _CANA_ACCEL,
	CANA_ACCEL_Y,
	CANA_ACCEL_Z,

	_CANA_SENSOR,
	CANA_SENSOR_X	= _CANA_SENSOR,
	CANA_SENSOR_Y,
	CANA_SENSOR_Z,

	CANA_NONE,
	CANA_TOTAL
};
enum ControllerAnalog2 {
	CANA_FINGER1	= CANA_FINGER1_X,
	CANA_FINGER2	= CANA_FINGER2_X,
	CANA_FINGER3	= CANA_FINGER3_X,
	CANA_FINGER4	= CANA_FINGER4_X,

	CANA_PTR		= CANA_PTR_X,	// not clear on lift-up
};

enum ControllerType {
	CTYPE_UNPLUGGED,
	CTYPE_UNKNOWN,
	CTYPE_CONTROLLER,
	CTYPE_WHEEL,
};

class _Controller {
protected:
	uint32		Update(class Controller *cont, int i);
public:
	void		SetRumble(float left, float right)		{}
	bool		HasForceFeedback()				const	{ return false; }
	void		SetForceFeedback(int effect, float gain){}
	static bool	InitSystem()							{ return true; }
};

}//namespace iso

#endif
