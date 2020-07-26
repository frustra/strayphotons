#pragma once

#include <memory>
#include <openvr.h>
#include <string>
#include <xr/XrAction.hh>
#include <xr/XrModel.hh>

namespace sp {
	namespace xr {
		class OpenVrActionSet : public XrActionSet, public std::enable_shared_from_this<OpenVrActionSet> {
		public:
			// Inherited from XrActionSet
			OpenVrActionSet(std::string setName, std::string description);

			std::shared_ptr<XrAction> CreateAction(std::string name, XrActionType type) override;

			void Sync() override;

			// Specific to OpenVrActionSet
			vr::VRActionSetHandle_t GetHandle() {
				return handle;
			};

		private:
			vr::VRActionSetHandle_t handle;
		};

		class OpenVrAction : public XrAction {
		public:
			vr::VRActionHandle_t GetHandle() {
				return handle;
			};

			// Boolean action manipulation
			bool GetBooleanActionValue(std::string subpath, bool &value);

			// Returns true if the boolean action transitioned from false to true
			// during this update loop
			bool GetRisingEdgeActionValue(std::string subpath, bool &value);

			// Returns true if the boolean action transitioned from true to false
			// during this update loop
			bool GetFallingEdgeActionValue(std::string subpath, bool &value);

			bool GetPoseActionValueForNextFrame(std::string subpath, glm::mat4 &pose);

			bool GetSkeletonActionValue(std::vector<XrBoneData> &bones, bool withController);

			std::shared_ptr<XrModel> GetInputSourceModel();

		private:
			// Only OpenVrActionSet is allowed to construct actions
			friend class OpenVrActionSet;

			struct BoneData {
				vr::BoneIndex_t steamVrBoneIndex;
				glm::mat4 inverseBindPose;
			};

			OpenVrAction(std::string name, XrActionType type, std::shared_ptr<OpenVrActionSet> actionSet);

			bool ComputeBoneLookupTable(std::shared_ptr<XrModel> model);
			std::vector<std::string> GetSteamVRBoneNames();

			vr::VRActionHandle_t handle;
			std::shared_ptr<OpenVrActionSet> parentActionSet;

			// Contains bone data for each bone found _in the GLTF model_.
			// This data stores the SteamVR Bone Index relevant to this Model Bone,
			// as well as the inverse bind pose for the bone.
			std::vector<BoneData> modelBoneData;

			// Cached OpenVR models that can be provided to the engine
			// as the user switches between controllers
			//
			// TODO: maybe make this static / atomic and share these models between all
			// instances of the OpenVrAction class? Say left and right hand Vive wands use
			// the same model. Is it useful / allowed to only have a single underlying XrModel
			// provided to the engine to render twice?
			std::map<std::string, std::shared_ptr<XrModel>> cachedModels;
		};
	} // namespace xr
} // namespace sp
