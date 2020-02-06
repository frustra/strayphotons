#include "xr/openvr/OpenVrInputSources.hh"

using namespace sp;
using namespace xr;
using namespace std;

bool OculusInputSource::GetInputSourceState(string inputSource, string prefix, vr::VRControllerState_t *state)
{
	uint64_t buttonMask = 0;

	// Bindings for Oculus touch controller buttons
	if (inputSource == (prefix + "input/x/click") ||
		inputSource == (prefix + "input/a/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_A);
	}
	else if (inputSource == (prefix + "input/y/click") ||
			 inputSource == (prefix + "input/b/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_ApplicationMenu);
	}
	// Bindings for the trigger and grip axis
	// Say the activation threshold is at 0.8
	else if (inputSource == (prefix + "input/squeeze/value"))
	{
		// Axis 2
		if (state->rAxis[2].x > 0.8)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (inputSource == (prefix + "input/trigger/value"))
	{
		// Axis 1
		if (state->rAxis[1].x > 0.8)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	if ((state->ulButtonPressed & buttonMask) == buttonMask)
	{
		return true;
	}

	return false;
}

bool ViveInputSource::GetInputSourceState(string inputSource, string prefix, vr::VRControllerState_t *state)
{
	uint64_t buttonMask = 0;

	// Bindings for HTC Vive Controller buttons
	if (inputSource == (prefix + "input/trackpad/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_Axis0);
	}
	else if (inputSource == (prefix + "input/squeeze/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_Grip);
	}
	else if (inputSource == (prefix + "input/trigger/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_Axis1);
	}
	// Boolean bindings for the trigger axis
	// Say the activation threshold is at 0.8
	else if (inputSource == (prefix + "input/trigger/value"))
	{
		// Axis 1
		if (state->rAxis[1].x > 0.8)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	if ((state->ulButtonPressed & buttonMask) == buttonMask)
	{
		return true;
	}

	return false;
}

bool IndexInputSource::GetInputSourceState(string inputSource, string prefix, vr::VRControllerState_t *state)
{
	uint64_t buttonMask = 0;

	// Bindings for HTC Vive Controller buttons
	if (inputSource == (prefix + "input/a/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_Grip);
	}
	else if (inputSource == (prefix + "input/b/click"))  // bin 2
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_ApplicationMenu);
	}
	else if (inputSource == (prefix + "input/touchpad/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_Axis0);
	}
	else if (inputSource == (prefix + "input/trigger/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_Axis1);
	}
	else if (inputSource == (prefix + "input/squeeze/click"))
	{
		buttonMask = ButtonMaskFromId(vr::k_EButton_Grip);
	}
	// Boolean bindings for the trigger axis
	// Say the activation threshold is at 0.8
	else if (inputSource == (prefix + "input/trigger/value"))
	{
		// Axis 1
		if (state->rAxis[1].x > 0.8)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	if ((state->ulButtonPressed & buttonMask) == buttonMask)
	{
		return true;
	}

	return false;
}