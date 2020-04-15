#pragma once

#include <xr/XrAction.hh>
#include <xr/XrModel.hh>
#include <openvr.h>
#include <string>

namespace sp
{
	namespace xr
	{
        class OpenVrActionSet : public XrActionSet
        {
            public:
                // Inherited from XrActionSet
                OpenVrActionSet(std::string setName, std::string description);

                std::shared_ptr<XrAction> CreateAction(std::string name, XrActionType type) override;

                void Sync();

                // Specific to OpenVrActionSet
                vr::VRActionSetHandle_t GetHandle() { return handle; };

            private:
                vr::VRActionSetHandle_t handle;
        };

        class OpenVrAction : public XrAction
        {
            public:
                vr::VRActionHandle_t GetHandle() { return handle; };

                // Boolean action manipulation
                bool GetBooleanActionValue(std::string subpath);

                // Returns true if the boolean action transitioned from false to true
                // during this update loop
                bool GetRisingEdgeActionValue(std::string subpath);

                // Returns true if the boolean action transitioned from true to false
                // during this update loop
                bool GetFallingEdgeActionValue(std::string subpath);

                bool GetPoseActionValueForNextFrame(std::string subpath, glm::mat4 &pose);

                bool GetSkeletonActionValue(std::vector<XrBoneData> &bones, bool withController);

                std::shared_ptr<XrModel> GetInputSourceModel();

                bool IsInputSourceConnected();
                
            private:
                friend class OpenVrActionSet;

                struct BoneData {
                    vr::BoneIndex_t steamVrBoneIndex;
                    glm::mat4 inverseBindPose;
                };

                // Only the OpenVrActionSet is allowed to construct actions
                OpenVrAction(std::string name, XrActionType type, std::shared_ptr<OpenVrActionSet> actionSet);

                void ComputeBoneLookupTable(std::shared_ptr<XrModel> model);
                std::vector<std::string> GetSteamVRBoneNames();

                vr::VRActionHandle_t handle;
                std::shared_ptr<OpenVrActionSet> parentActionSet;

                // Contains bone data for each bone found _in the GLTF model_.
                // This data stores the SteamVR Bone Index relevant to this Model Bone, 
                // as well as the inverse bind pose for the bone.
                std::vector<BoneData> modelBoneData;
        };
    }
}
