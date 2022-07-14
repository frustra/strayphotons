#include "View.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ecs {
    template<>
    bool Component<View>::Load(const EntityScope &scope, View &view, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "fov") {
                view.fov = glm::radians(param.second.get<double>());
            } else {
                if (param.first == "extents") {
                    if (!sp::json::Load(scope, view.extents, param.second)) {
                        Errorf("Invalid view extents: %s", param.second.to_str());
                        return false;
                    }
                } else if (param.first == "clip") {
                    if (!sp::json::Load(scope, view.clip, param.second)) {
                        Errorf("Invalid view clip: %s", param.second.to_str());
                        return false;
                    }
                } else if (param.first == "offset") {
                    if (!sp::json::Load(scope, view.offset, param.second)) {
                        Errorf("Invalid view offset: %s", param.second.to_str());
                        return false;
                    }
                } else if (param.first == "visibility") {
                    auto &value = param.second.get<string>();
                    if (value == "camera") {
                        view.visibilityMask.set(Renderable::Visibility::DirectCamera);
                    }
                } else {
                    Errorf("Unknown view component parameter: %s", param.first);
                    return false;
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

    void View::UpdateViewMatrix(Lock<Read<TransformSnapshot>> lock, Entity e) {
        if (e.Has<TransformSnapshot>(lock)) {
            this->invViewMat = e.Get<TransformSnapshot>(lock).matrix;
            this->viewMat = glm::inverse(this->invViewMat);
        }
    }
} // namespace ecs
