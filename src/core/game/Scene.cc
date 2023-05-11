#include "Scene.hh"

#include "core/Tracing.hh"

#include <memory>

namespace sp {
    Scene::Scene(SceneMetadata &&metadata, std::shared_ptr<const Asset> asset)
        : data(std::make_shared<const SceneMetadata>(std::move(metadata))), asset(asset) {}

    Scene::~Scene() {
        Assertf(data, "Scene destroyed with null data");
        Assertf(!active, "%s scene destroyed while active: %s", data->type, data->name);
    }

    ecs::Entity Scene::GetStagingEntity(const ecs::Name &entityName) const {
        auto it = namedEntities.find(entityName);
        if (it != namedEntities.end()) return it->second;
        return {};
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
