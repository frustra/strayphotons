#pragma once

#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace sp {
    class GpuTexture;
    class RenderTarget;

    namespace xr {

        // This class encapsulates all functions required to render a frame to the HMD.
        // Conceptually, this means it must:
        //  - Provide methods to submit a swapchain texture to the underlying runtime compositor
        //  - Provide methods to initialize swapchains
        //  - Provide methods to estimate the motion-to-photon latency

        class XrCompositor {
        public:
            // Returns the recommended render target size for this XrSystem. Throws an exception on failure.
            virtual void GetRecommendedRenderTargetSize(uint32_t &width, uint32_t &height) = 0;

            // Some XR Systems support more than 2 eyes. For example, HoloLens and Oculus support a "3rd eye" view for
            // doing Mixed Reality Capture. The Boolean input "Minimum" asks the XR System for the absolute,
            // bare-minimum number of eye views required to render correctly. This is useful on performance-constrained
            // systems. Normally, this value is 2.
            virtual size_t GetNumViews(bool minimum = true) = 0;

            // Get the projection matrix for a given View. Must provide the nearZ and farZ clipping distances
            // for the projection matrix.
            virtual glm::mat4 GetViewProjectionMatrix(size_t view, float nearZ, float farZ) = 0;

            // This function is called on each frame to get the current texture the
            // compositor would like this View rendered to.
            // This can be statically allocated, or part of a swapchain on some systems.
            // This is guaranteed to only be called once per frame, per view.
            virtual RenderTarget *GetRenderTarget(size_t view) = 0;

            // Updates a provided ecs::View entity with the properties required to render from the perspective of
            // a particular XR view.
            virtual void PopulateView(size_t view, ecs::View &ecsView) = 0;

            // Submit a GpuTexture to the compositing system to be displayed to the user.
            // TODO: in theory, the XrCompositor should be able to keep track of which RT
            virtual void SubmitView(size_t view, GpuTexture *tex) = 0;

            // This function is used to synchronize the engine framerate with the display timing
            // for the XR device. This must be called EXACTLY ONCE per frame. This function will block
            // until the underlying XR runtime is ready for the next frame. The underlying XR runtime
            // is measuring the amount of time between calls to "WaitFrame()" to determine how long
            // it takes the application (on average) to render a frame. The XR runtime will unblock
            // the application when it is ready for a new frame (or more accurately, slightly before
            // it is ready for the frame to give the application time to render the frame)
            virtual void WaitFrame() = 0;

            // Used by the underlying XR runtime to help manage state or track application render time.
            // Generally called immediately after WaitFrame()
            // On systems with Swapchain render targets, calling BeginFrame() will increment the position in the RT
            // swapchain, which will cause the result of GetRenderTarget() to change.
            virtual void BeginFrame() = 0;

            // Used by the underlying XR runtime to help manage state or track application render time.
            // Generally called immediately submitting the last View for this XR runtime.
            virtual void EndFrame() = 0;

        protected:
            // TODO: for systems where the glTexture comes from the XR runtime itself, instead
            // of just allowing it to be generated automatically.
            // void AssignRenderTargetTexture(RenderTarget::Ref renderTarget, const RenderTargetDesc, GLuint texHandle);
        };
    } // namespace xr
} // namespace sp
