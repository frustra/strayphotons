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
        view.UpdateProjectionMatrix();
        return true;
    }

    template<>
    void Component<View>::Apply(const View &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstView = dst.Get<View>(lock);
        dstView.UpdateProjectionMatrix();
    }

    void View::UpdateProjectionMatrix() {
        if (this->fov > 0 && (this->extents.x != 0 || this->extents.y != 0)) {
            auto aspect = this->extents.x / (float)this->extents.y;

            this->projMat = glm::perspective(this->fov.radians(), aspect, this->clip[0], this->clip[1]);
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
