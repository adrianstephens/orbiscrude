#ifdef PLAT_WINRT
#include "controller.h"

namespace iso {

uint32 _Controller::Update(Controller *cont, int i) {
	return 0;
}

bool _Controller::InitSystem() {
	return false;
}
}// namespace iso

#else

#define DIRECTINPUT_VERSION 0x0800
#define INITGUID
#include <dinput.h>
#include "controller.h"

#pragma comment(lib, "dinput8")

namespace iso {

LPDIRECTINPUT8			dInput8			= NULL;
LPDIRECTINPUTDEVICE8	dInpJoystick	= NULL;	// This should really be an array or list of joysticks.
float					_Controller::fake_analog[CANA_TOTAL];
uint32					_Controller::fake_buttons;

uint32 _Controller::Update(Controller *cont, int i) {

	if (dInpJoystick) {
		HRESULT     hr;
		DIJOYSTATE2 js;           // DInput joystick state

		// Poll the dInput8Joystick to read the current state
		hr = dInpJoystick->Poll();
		if (FAILED(hr)) {
			hr = dInpJoystick->Acquire();
			while (hr == DIERR_INPUTLOST)
				hr = dInpJoystick->Acquire();
		} else {
			// Get the input's device state
			if (SUCCEEDED(hr = dInpJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &js))) {
				float	*analog		= cont->analog.begin();
				float	maxabsval;
				int		modPOV;

				maxabsval = abs(analog[CANA_LEFT_X]			= Adjust(js.lX,		-RANGEDEADZONE, RANGEDEADZONE, RANGEMAXVAL));
				maxabsval = max(abs(analog[CANA_LEFT_Y]		= Adjust(js.lY,		-RANGEDEADZONE, RANGEDEADZONE, RANGEMAXVAL)), maxabsval);
				maxabsval = max(abs(analog[CANA_RIGHT_X]	= Adjust(js.lRx,	-RANGEDEADZONE, RANGEDEADZONE, RANGEMAXVAL)), maxabsval);
				maxabsval = max(abs(analog[CANA_RIGHT_Y]	=-Adjust(js.lRy,	-RANGEDEADZONE, RANGEDEADZONE, RANGEMAXVAL)), maxabsval);
				maxabsval = max(abs(analog[CANA_TRIGGER_L]	= js.lZ > 0 ? Adjust(js.lZ,	RANGEDEADZONE, RANGEMAXVAL) : 0.f), maxabsval);
				maxabsval = max(abs(analog[CANA_TRIGGER_R]	= js.lZ < 0 ? Adjust(abs(js.lZ), RANGEDEADZONE, RANGEMAXVAL) : 0.f), maxabsval);

				uint32	buttons = 0;
				for (int i = 0; i < 16; i++) {
					if (js.rgbButtons[i] & 0x80)
						buttons |= 1 << i;
				}
				if (analog[CANA_TRIGGER_L]	> 0.9f)
					buttons |= CBUT_TRIGGER_L;
				if (analog[CANA_TRIGGER_R]	> 0.9f)
					buttons |= CBUT_TRIGGER_R;

				// Convert the pov input to be the dpad
				// -1 = none, 0 is up, 9000 is right, 18000 is down,...
				if ((modPOV = (int)js.rgdwPOV[0]) >= 0) {
					modPOV = modPOV <= 18000 ? modPOV : modPOV - 36000;
					if (modPOV < 0)
						buttons |= CBUT_DPAD_LEFT;
					else if (abs(modPOV - 9000) < 9000)
						buttons |= CBUT_DPAD_RIGHT;
					modPOV = abs(modPOV);
					if (modPOV < 9000)
						buttons |= CBUT_DPAD_UP;
					else if (modPOV > 9000)
						buttons |= CBUT_DPAD_DOWN;
				}
				if ((maxabsval > .1f) || buttons)
					return buttons;
			}
		}
	}
	// Joystick isn't active (could exist, but not being touched), then apply mouse / keyboard
	float	*analog		= cont->analog.begin();
	for (int i = 0; i < CANA_TOTAL; i++) {
		analog[i] = clamp(fake_analog[i], -1.f, 1.f);
		if (i >= 2)
			fake_analog[i] = fake_analog[i] * 0.9f;
	}
	return fake_buttons;
}

BOOL CALLBACK EnumAxesCallback(LPCDIDEVICEOBJECTINSTANCE devObjInstance, void *context) {
	LPDIRECTINPUTDEVICE8 dInput8Joystick = (LPDIRECTINPUTDEVICE8)context;

	DIPROPRANGE range;
	range.diph.dwSize       = sizeof(DIPROPRANGE);
	range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	range.diph.dwHow        = DIPH_BYOFFSET;
	range.diph.dwObj        = devObjInstance->dwOfs; // Specify the enumerated axis
	range.lMin              = Controller::RANGEMINVAL;
	range.lMax              = Controller::RANGEMAXVAL;

	// Set the range for the axis
	dInput8Joystick->SetProperty(DIPROP_RANGE, &range.diph);
	return DIENUM_CONTINUE;
}

BOOL CALLBACK EnumJoysticksCallback(LPCDIDEVICEINSTANCE devInstance, void *context) {
	if (dInpJoystick == NULL) {
		LPDIRECTINPUTDEVICE8 dInput8Joystick = NULL;

		if (SUCCEEDED(dInput8->CreateDevice(devInstance->guidInstance, &dInput8Joystick, NULL))) {
			DIDEVCAPS joystickCaps;

			joystickCaps.dwSize = sizeof(joystickCaps);
			if (SUCCEEDED(dInput8Joystick->SetDataFormat(&c_dfDIJoystick2)) &&
				SUCCEEDED(dInput8Joystick->GetCapabilities(&joystickCaps)) &&
				SUCCEEDED(dInput8Joystick->EnumObjects(EnumAxesCallback, dInput8Joystick, DIDFT_AXIS))) {
				// Support X360 controller
	//			if( joystickCaps.dwAxes			== 5
	//				&&	joystickCaps.dwButtons	== 10
	//				&&	joystickCaps.dwPOVs		== 1 )
				// Good enough to use as controller
				if (joystickCaps.dwAxes >= 5 && joystickCaps.dwButtons >= 10) {
					// Register controller here.
					dInpJoystick = dInput8Joystick; // Use this joystick for now.
				} else {
					// Not valid, so skip
					dInput8Joystick->Release();
					dInput8Joystick = NULL;
				}
			}
		}
	}

	return DIENUM_CONTINUE;
}

bool _Controller::InitSystem() {
	HINSTANCE	hInstance = GetModuleHandle(NULL);
	if (SUCCEEDED(DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&dInput8, NULL))) {
		dInput8->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY);
		return true;
	}
	return false;
}

}

#endif