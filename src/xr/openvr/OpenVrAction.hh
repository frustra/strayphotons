#pragma once

#include <xr/XrAction.hh>
#include <openvr.h>
#include <string>

namespace sp
{
	namespace xr
	{
        class OpenVrActionSet : public XrActionSet
        {
            public:
                OpenVrActionSet(std::string setName, std::string description);

                std::shared_ptr<XrAction> CreateAction(std::string name, XrActionType type) override;

                void Sync();

            private:
                vr::VRActionSetHandle_t handle;
        };

        class OpenVrAction : public XrAction
        {
            public:
                OpenVrAction(std::string name, XrActionType type);

                // Boolean action manipulation
                bool GetBooleanActionValue(std::string subpath);

                // Returns true if the boolean action transitioned from false to true
                // during this update loop
                bool GetRisingEdgeActionValue(std::string subpath);

                // Returns true if the boolean action transitioned from true to false
                // during this update loop
                bool GetFallingEdgeActionValue(std::string subpath);
                
            private:
                vr::VRActionHandle_t handle;
        };
    }
}
