#include "Scene.hh"

#include "core/Tracing.hh"
#include "ecs/EntityReferenceManager.hh"

namespace sp {
    Scene::~Scene() {
        Assertf(!active, "%s scene destroyed while active: %s", type, name);
    }

    ecs::Name Scene::GenerateEntityName(const ecs::Name &prefix) {
        unnamedEntityCount++;
        if (!prefix.entity.empty()) {
            return ecs::Name(prefix.scene, prefix.entity + ".entity" + std::to_string(unnamedEntityCount));
        } else {
            return ecs::Name(prefix.scene, "entity" + std::to_string(unnamedEntityCount));
        }
    }

    bool Scene::ValidateEntityName(const ecs::Name &name) const {
        if (!name) return false;
        if (starts_with(name.entity, "entity")) {
            return std::find_if(name.entity.begin() + 7, name.entity.end(), [](char ch) {
                return !std::isdigit(ch);
            }) != name.entity.end();
        }
        return true;
    }
} // namespace sp
