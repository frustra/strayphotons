#pragma once

#include <string>
#include <openvr.h>

namespace sp
{
	namespace xr
	{

		class OpenVrInputSource
		{
		public:
			virtual std::string GetInteractionProfile() = 0;
			virtual bool GetInputSourceState(std::string inputSource, std::string prefix, vr::VRControllerState_t *state) = 0;
		};

		class OculusInputSource : public OpenVrInputSource
		{
		public:
			std::string GetInteractionProfile()
			{
				return "/interaction_profiles/oculus/touch_controller";
			};
			bool GetInputSourceState(std::string inputSource, std::string prefix, vr::VRControllerState_t *state);
		};

		class ViveInputSource : public OpenVrInputSource
		{
		public:
			std::string GetInteractionProfile()
			{
				return "/interaction_profiles/htc/vive_controller";
			};
			bool GetInputSourceState(std::string inputSource, std::string prefix, vr::VRControllerState_t *state);
		};

		class IndexInputSource : public OpenVrInputSource
		{
		public:
			std::string GetInteractionProfile()
			{
				return "/interaction_profiles/valve/index_controller";
			};
			bool GetInputSourceState(std::string inputSource, std::string prefix, vr::VRControllerState_t *state);
		};

		class NullInputSource : public OpenVrInputSource
		{
		public:
			std::string GetInteractionProfile()
			{
				return "/interaction_profiles/none/invalid";
			};
			bool GetInputSourceState(std::string inputSource, std::string prefix, vr::VRControllerState_t *state)
			{
				return false;
			};
		};

	}
}