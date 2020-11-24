#include "ecs/components/View.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/Ecs.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<View>::Load(Lock<Read<ecs::Name>> lock, View &view, const picojson::value &src) {
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
                }
            }
        }
        return true;
    }

    void ValidateView(Entity viewEntity) {
        if (!viewEntity.Valid()) {
            throw std::runtime_error("view entity is not valid because the entity has been deleted");
        }
        if (!viewEntity.Has<View>()) {
            throw std::runtime_error("view entity is not valid because it has no View component");
        }
        if (!viewEntity.Has<ecs::XRView>() && !viewEntity.Has<Transform>()) {
            throw std::runtime_error("view entity is not valid because it has no Transform component");
        }
    }

    Handle<View> UpdateViewCache(Entity entity, float fov) {
        ValidateView(entity);

        auto view = entity.Get<View>();
        view->aspect = (float)view->extents.x / (float)view->extents.y;

        if (!entity.Has<ecs::XRView>()) {
            view->projMat = glm::perspective(fov > 0.0 ? fov : view->fov, view->aspect, view->clip[0], view->clip[1]);

            auto transform = entity.Get<Transform>();
            view->invViewMat = transform->GetGlobalTransform(*entity.GetManager());
        }

        view->invProjMat = glm::inverse(view->projMat);
        view->viewMat = glm::inverse(view->invViewMat);

        return view;
    }

    void View::SetProjMat(float _fov, glm::vec2 _clip, glm::ivec2 _extents) {
        extents = _extents;
        fov = _fov;
        clip = _clip;

        aspect = (float)extents.x / (float)extents.y;

        SetProjMat(glm::perspective(fov, aspect, clip[0], clip[1]));
    }

    void View::SetProjMat(glm::mat4 proj) {
        projMat = proj;
        invProjMat = glm::inverse(projMat);
    }

    void View::SetInvViewMat(glm::mat4 invView) {
        invViewMat = invView;
        viewMat = glm::inverse(invViewMat);
    }

    glm::ivec2 View::GetExtents() {
        return extents;
    }

    glm::vec2 View::GetClip() {
        return clip;
    }

    float View::GetFov() {
        return fov;
    }

    glm::mat4 View::GetProjMat() {
        return projMat;
    }

    glm::mat4 View::GetInvProjMat() {
        return invProjMat;
    }

    glm::mat4 View::GetViewMat() {
        return viewMat;
    }

    glm::mat4 View::GetInvViewMat() {
        return invViewMat;
    }
} // namespace ecs
