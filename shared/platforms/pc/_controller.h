#ifndef _CONTROLLER_H
#define _CONTROLLER_H

namespace iso {

enum ControllerButton {
	CBUT_A					= 1<<0,
	CBUT_B					= 1<<1,
	CBUT_X					= 1<<2,
	CBUT_Y					= 1<<3,
	CBUT_L1					= 1<<4,
	CBUT_R1					= 1<<5,
	CBUT_BACK				= 1<<6,
	CBUT_START				= 1<<7,
	CBUT_L3					= 1<<8,
	CBUT_R3					= 1<<9,
	CBUT_L2					= 1<<10,
	CBUT_R2					= 1<<11,
	CBUT_DPAD_UP			= 1<<12,
	CBUT_DPAD_DOWN			= 1<<13,
	CBUT_DPAD_LEFT			= 1<<14,
	CBUT_DPAD_RIGHT			= 1<<15,
	CBUT_ANAL_UP			= 1<<16,
	CBUT_ANAL_DOWN			= 1<<17,
	CBUT_ANAL_LEFT			= 1<<18,
	CBUT_ANAL_RIGHT			= 1<<19,
	CBUT_ANAR_UP			= 1<<20,
	CBUT_ANAR_DOWN			= 1<<21,
	CBUT_ANAR_LEFT			= 1<<22,
	CBUT_ANAR_RIGHT			= 1<<23,
	CBUT_TRIGGER_L			= 1<<24,
	CBUT_TRIGGER_R			= 1<<25,
	CBUT_SELECT				= 1<<26,
	CBUT_MOUSE_L			= 1<<27,
	CBUT_MOUSE_R			= 1<<28,

	CBUT_PAUSE				= CBUT_START,
	CBUT_ACTION				= CBUT_A | CBUT_MOUSE_L,
	CBUT_ACTION2			= CBUT_X,
	CBUT_ACTION3			= CBUT_Y,
	CBUT_RETURN				= CBUT_BACK,
	CBUT_SPECIAL			= CBUT_SELECT,
	CBUT_UP					= CBUT_DPAD_UP		| CBUT_ANAL_UP,
	CBUT_DOWN				= CBUT_DPAD_DOWN	| CBUT_ANAL_DOWN,
	CBUT_LEFT				= CBUT_DPAD_LEFT	| CBUT_ANAL_LEFT,
	CBUT_RIGHT				= CBUT_DPAD_RIGHT	| CBUT_ANAL_RIGHT,
	CBUT_PTR				= CBUT_MOUSE_L,
};
enum ControllerAnalog {
	CANA_PTR_X,
	CANA_PTR_Y,

	CANA_LEFT_X,
	CANA_LEFT_Y,
	CANA_RIGHT_X,
	CANA_RIGHT_Y,
	CANA_TRIGGER_L,
	CANA_TRIGGER_R,

	CANA_TRIGGER_LR,

	CANA_LEFT_POS_X,
	CANA_LEFT_POS_Y,
	CANA_LEFT_POS_Z,

	CANA_RIGHT_POS_X,
	CANA_RIGHT_POS_Y,
	CANA_RIGHT_POS_Z,


	CANA_TOTAL
};
enum ControllerAnalog2 {
	CANA_PTR		= CANA_PTR_X,
	CANA_LEFT		= CANA_LEFT_X,
	CANA_RIGHT		= CANA_RIGHT_X,
	CANA_TRIGGER	= CANA_TRIGGER_L,
};
enum ControllerAnalog3 {
	CANA_LEFT_POS	= CANA_LEFT_POS_X,
	CANA_RIGHT_POS	= CANA_RIGHT_POS_X,
};
enum ControllerType {
	CTYPE_UNPLUGGED,
	CTYPE_UNKNOWN,
	CTYPE_CONTROLLER,
	CTYPE_WHEEL,
};

class _Controller {
	static	float	Adjust(int i, int deadzone, int maximum)					{ return clamp(float(i - deadzone) / (maximum - deadzone), 0.f, 1.f);	}
	static	float	Adjust(int i, int deadzone0, int deadzone1, int maximum)	{ return clamp(float(min(i - deadzone0, 0) + max(i - deadzone1, 0)) / (maximum + deadzone0 - deadzone1), -1.0f, 1.0f);	}
protected:

	_Controller()	{ clear(*this); }
	uint32		Update(class Controller *cont, int i);

public:
	static const int	RANGEDEADZONE = 4096;
	static const int	RANGEMINVAL = -32767;
	static const int	RANGEMAXVAL = 32767;

	static float	fake_analog[CANA_TOTAL];
	static uint32	fake_buttons;
	static void		_SetAnalog(ControllerAnalog a, float d)				{ fake_analog[a] = d;	}
	static void		_SetAnalog(ControllerAnalog2 a, param(float2) d)	{ store(d, fake_analog + a); }
	static void		_SetAnalog(ControllerAnalog3 a, param(float3) d)	{ store(d, fake_analog + a); }
	static void		_AddAnalog(ControllerAnalog a, float d)				{ fake_analog[a] += d;	}
	static void		_AddAnalog(ControllerAnalog2 a, param(float2) d)	{ fake_analog[a] += d.x; fake_analog[a + 1] += d.y; }
	static void		_SetButton(ControllerButton b)						{ fake_buttons |= b;	}
	static void		_ClearButton(ControllerButton b)					{ fake_buttons &= ~b;	}

	void		SetRumble(float left, float right)		{}
	bool		HasForceFeedback()			const		{ return false;	}
	void		SetForceFeedback(int effect, float gain){}

	bool		IsValid()					const		{ return true;	}

	static bool	InitSystem();
};

}//namespace iso

#endif // _CONTROLLER_H
