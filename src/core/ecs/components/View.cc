#include "View.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/Ecs.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <picojson/picojson.h>

namespace ecs {

    template<>
    bool Component<View>::Load(sp::Scene *scene, View &view, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "fov") {
                view.fov = glm::radians(param.second.get<double>());
            } else {
                if (param.first == "extents") {
                    view.extents = sp::MakeVec2(param.second);
                } else if (param.first == "clip") {
                    view.clip = sp::MakeVec2(param.second);
                } else if (param.first == "offset") {
                    view.offset = sp::MakeVec2(param.second);
                } else if (param.first == "clear") {
                    view.clearColor = glm::vec4(sp::MakeVec3(param.second), 1.0f);
                } else if (param.first == "sky") {
                    view.skyIlluminance = param.second.get<double>();
                } else if (param.first == "visibility") {
                    auto &value = param.second.get<string>();
                    if (value == "camera") { view.visibilityMask.set(Renderable::Visibility::VISIBLE_DIRECT_CAMERA); }
                }
            }
        }
        view.UpdateProjectionMatrix();
        return true;
    }

    void View::UpdateProjectionMatrix() {
        if (this->fov > 0 && (this->extents.x != 0 || this->extents.y != 0)) {
            auto aspect = this->extents.x / (float)this->extents.y;

            this->projMat = glm::perspective(this->fov, aspect, this->clip[0], this->clip[1]);
            this->invProjMat = glm::inverse(this->projMat);
        }
    }

    void View::UpdateViewMatrix(Lock<Read<TransformSnapshot>> lock, Tecs::Entity e) {
        if (e.Has<TransformSnapshot>(lock)) {
            this->invViewMat = e.Get<TransformSnapshot>(lock).matrix;
            this->viewMat = glm::inverse(this->invViewMat);
        }
    }
} // namespace ecs
