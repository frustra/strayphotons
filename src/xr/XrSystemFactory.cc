#ifdef SP_XR_SUPPORT

#include "XrSystemFactory.hh"

#include "graphics/core/GraphicsContext.hh"

#ifdef SP_XR_SUPPORT_OPENVR
    #include "graphics/opengl/GlfwGraphicsContext.hh"
    #include "xr/openvr/OpenVrSystem.hh"
#endif

using namespace sp;
using namespace xr;

XrSystemFactory::XrSystemFactory(GraphicsContext *context) {

#ifdef SP_XR_SUPPORT_OPENVR
    GlfwGraphicsContext *glContext = dynamic_cast<GlfwGraphicsContext *>(context);
    Assert(glContext, "OpenVrSystem requires a GLFW Graphics Context");
    compiledXrSystems.push_back(std::shared_ptr<XrSystem>(new OpenVrSystem(*glContext)));
#endif

    // TODO: add more XrSystems
}

std::shared_ptr<XrSystem> XrSystemFactory::GetBestXrSystem() {
    std::shared_ptr<XrSystem> selectedXrSystem;

    for (auto xrSystem : compiledXrSystems) {
        if (xrSystem->IsHmdPresent()) {
            selectedXrSystem = xrSystem;
            break;
        }
    }

    return selectedXrSystem;
}

#endif
