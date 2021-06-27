#pragma once

#ifdef SP_XR_SUPPORT

    #include "xr/XrSystem.hh"

    #include <list>
    #include <memory>

namespace sp {
    class GraphicsContext;

    namespace xr {

        class XrSystemFactory {
        public:
            XrSystemFactory(GraphicsContext *context);

            // This function picks the "best" XR system based on the current execution environment.
            // In many cases, this returns a NULL pointer
            std::shared_ptr<XrSystem> GetBestXrSystem();

        private:
            std::list<std::shared_ptr<XrSystem>> compiledXrSystems;
        };

    } // namespace xr
} // namespace sp

#endif
