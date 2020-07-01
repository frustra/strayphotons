#include "xr/XrSystemFactory.hh"

#if defined(XRSYSTEM_OPENVR)
#include "xr/openvr/OpenVrSystem.hh"
#endif

using namespace sp;
using namespace xr;

XrSystemFactory::XrSystemFactory()
{

#if defined(XRSYSTEM_OPENVR)
    compiledXrSystems.push_back(std::shared_ptr<XrSystem>(new OpenVrSystem));
#endif

    // TODO: add more XrSystems

}

std::shared_ptr<XrSystem> XrSystemFactory::GetBestXrSystem()
{
    std::shared_ptr<XrSystem> selectedXrSystem;

    for (auto xrSystem : compiledXrSystems)
    {
        if (xrSystem->IsHmdPresent())
        {
            selectedXrSystem = xrSystem;
            break;
        }
    }

    return selectedXrSystem;
}