#pragma once

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    class Entity;
    template<typename>
    class Handle;

    class View {
    public:
        enum ViewType {
            VIEW_TYPE_PANCAKE,
            VIEW_TYPE_XR,
            VIEW_TYPE_LIGHT,
        };

        // Define a struct (built from bitfields) that can be used to store clear modes independent of any 
        // graphics backend.
        struct ClearMode { 
            bool ColorBuffer        : 1;
            bool DepthBuffer        : 1;
            bool AccumulationBuffer : 1; 
            bool StencilBuffer      : 1;

            ClearMode& setColorBuffer(bool val) {
                ColorBuffer = val;
                return *this;
            }

            ClearMode& setDepthBuffer(bool val) {
                DepthBuffer = val;
                return *this;
            }

            ClearMode& setAccumulationBuffer(bool val) {
                AccumulationBuffer = val;
                return *this;
            }

            ClearMode& setStencilBuffer(bool val) {
                StencilBuffer = val;
                return *this;
            }

            void clear() {
                ColorBuffer = false;
                DepthBuffer = false;
                AccumulationBuffer = false;
                StencilBuffer = false;
            }

            bool any() const {
                return ColorBuffer | DepthBuffer | AccumulationBuffer | StencilBuffer;
            }
        };

        View() {}
        View(glm::ivec2 extents) : extents(extents) {}

        // When setting these parameters, View needs to recompute some internal params
        void SetProjMat(glm::mat4 proj);
        void SetProjMat(float _fov, glm::vec2 _clip, glm::ivec2 _extents);
        void SetInvViewMat(glm::mat4 invView);

        glm::ivec2 GetExtents();
        glm::vec2 GetClip();
        float GetFov();

        glm::mat4 GetProjMat();
        glm::mat4 GetInvProjMat();
        glm::mat4 GetViewMat();
        glm::mat4 GetInvViewMat();

        // Optional parameters;
        glm::ivec2 offset = {0, 0};
        // TODO(any): Maybe remove color clear once we have interior spaces
        ClearMode clearMode = ClearMode().setColorBuffer(true).setDepthBuffer(true);
        glm::vec4 clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        bool stencil = false;
        bool blend = false;
        float skyIlluminance = 0.0f;
        float scale = 1.0f;

        // For XR Views
        ViewType viewType = VIEW_TYPE_PANCAKE;

        // private:
        // Required parameters.
        glm::ivec2 extents = {0, 0};
        glm::vec2 clip = {0, 0}; // {near, far}
        float fov = 0.0f;

        // Updated automatically.
        float aspect = 1.0f;
        glm::mat4 projMat, invProjMat;
        glm::mat4 viewMat, invViewMat;
    };

    static Component<View> ComponentView("view");

    template<>
    bool Component<View>::Load(Lock<Read<ecs::Name>> lock, View &dst, const picojson::value &src);

    void ValidateView(Entity viewEntity);
    Handle<ecs::View> UpdateViewCache(Entity entity, float fov = 0.0);
} // namespace ecs
