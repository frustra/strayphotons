#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <picojson/picojson.h>

namespace ecs {
    InternalPrefab templatePrefab("template", [](ScriptState &state, Lock<AddRemove> lock, Entity ent) {
        auto scene = state.scene.lock();
        Assertf(scene, "Template prefab does not have a valid scene: %s", ToString(lock, ent));

        auto sourceName = state.GetParam<std::string>("source");

        ecs::Name scope(scene->name, "");
        if (ent.Has<Name>(lock)) scope = ent.Get<Name>(lock);
        Logf("Loading template: %s with scope %s", sourceName, scope.String());

        auto asset = sp::GAssets.Load("scenes/templates/" + sourceName + ".json", sp::AssetType::Bundled, true)->Get();
        if (!asset) {
            Errorf("Template not found: %s", sourceName);
            return;
        }

        picojson::value root;
        string err = picojson::parse(root, asset->String());
        if (!err.empty()) {
            Errorf("Failed to parse template (%s): %s", sourceName, err);
            return;
        }

        auto entityList = root.get<picojson::object>()["entities"];
        // Find all named entities first so they can be referenced.
        for (auto value : entityList.get<picojson::array>()) {
            auto obj = value.get<picojson::object>();

            if (obj.count("name")) {
                auto fullName = obj["name"].get<string>();
                ecs::Name name;
                name.Parse(fullName, scope);
                scene->NewPrefabEntity(lock, ent, name);
            }
        }

        std::vector<ecs::Entity> entities;
        for (auto value : entityList.get<picojson::array>()) {
            auto obj = value.get<picojson::object>();

            ecs::Entity newEntity;
            if (obj.count("name")) {
                auto fullName = obj["name"].get<string>();
                newEntity = scene->GetStagingEntity(fullName, scope);
                if (!newEntity) {
                    Errorf("Skipping entity with invalid name: %s", fullName);
                    continue;
                }
            } else {
                newEntity = scene->NewPrefabEntity(lock, ent);
            }

            for (auto comp : obj) {
                if (comp.first.empty() || comp.first[0] == '_' || comp.first == "name") continue;

                auto componentType = ecs::LookupComponent(comp.first);
                if (componentType != nullptr) {
                    if (!componentType->LoadEntity(lock, newEntity, comp.second)) {
                        Errorf("Failed to load component, ignoring: %s", comp.first);
                    }
                } else {
                    Errorf("Unknown component, ignoring: %s", comp.first);
                }
            }

            if (newEntity.Has<ecs::TransformTree>(lock)) {
                auto &transform = newEntity.Get<ecs::TransformTree>(lock);
                if (!transform.parent && ent.Has<TransformTree>(lock)) transform.parent = ent;
                newEntity.Set<ecs::TransformSnapshot>(lock);
            }

            entities.emplace_back(newEntity);
        }

        for (auto &e : entities) {
            if (e.Has<ecs::Script>(lock)) { e.Get<ecs::Script>(lock).Prefab(lock, e); }
        }
    });
} // namespace ecs
