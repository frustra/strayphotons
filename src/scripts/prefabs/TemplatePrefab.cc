#include "assets/Asset.hh"
#include "assets/AssetHelpers.hh"
#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <picojson/picojson.h>

namespace ecs {
    struct TemplateParser {
        ScriptState &state;
        Entity rootEnt;

        std::shared_ptr<sp::Scene> scene;
        std::string sourceName;
        ecs::Name rootScope;
        std::shared_ptr<sp::Asset> asset;
        picojson::value root;

        picojson::object *rootObj = nullptr, *componentsObj = nullptr;
        picojson::array *entityList = nullptr;

        TemplateParser(ScriptState &state, Entity rootEnt) : state(state), rootEnt(rootEnt) {}

        bool Parse(Lock<AddRemove> lock) {
            scene = state.scope.scene.lock();
            Assertf(scene, "Template prefab does not have a valid scene: %s", ToString(lock, rootEnt));

            sourceName = state.GetParam<std::string>("source");
            rootScope = {scene->name, ""};
            if (rootEnt.Has<Name>(lock)) rootScope = rootEnt.Get<Name>(lock);

            Debugf("Loading template: %s with scope '%s'", sourceName, rootScope.String());

            asset = sp::GAssets.Load("scenes/templates/" + sourceName + ".json", sp::AssetType::Bundled, true)->Get();
            if (!asset) {
                Errorf("Template not found: %s", sourceName);
                return false;
            }

            string err = picojson::parse(root, asset->String());
            if (!err.empty()) {
                Errorf("Failed to parse template (%s): %s", sourceName, err);
                return false;
            }

            if (!root.is<picojson::object>()) {
                Errorf("Template parameters must be an object (%s)", sourceName);
                return false;
            }

            rootObj = &root.get<picojson::object>();

            auto entitiesIter = rootObj->find("entities");
            if (entitiesIter != rootObj->end()) entityList = &entitiesIter->second.get<picojson::array>();

            auto componentsIter = rootObj->find("components");
            if (componentsIter != rootObj->end()) componentsObj = &componentsIter->second.get<picojson::object>();

            return true;
        }

        // Add defined components to the template root entity
        void ApplyComponents(Lock<AddRemove> lock) {
            if (!componentsObj) return;

            for (auto comp : *componentsObj) {
                if (comp.first.empty() || comp.first[0] == '_') continue;
                Assertf(comp.first != "name",
                    "Template components can't override entity name: %s",
                    comp.second.to_str());

                auto componentType = ecs::LookupComponent(comp.first);
                if (componentType != nullptr) {
                    if (!componentType->LoadEntity(lock, rootEnt, comp.second)) {
                        Errorf("Failed to load component, ignoring: %s", comp.first);
                    }
                } else {
                    Errorf("Unknown component, ignoring: %s", comp.first);
                }
            }
        }

        // Add defined entities as sub-entities of the template root
        void AddEntities(Lock<AddRemove> lock, std::string_view nestedScope = {}, Transform offset = {}) {
            if (!entityList) return;

            ecs::Name scope;
            if (nestedScope.empty()) {
                scope = rootScope;
            } else {
                scope = ecs::Name(nestedScope, rootScope);
            }

            // Find all named entities first so they can be referenced.
            for (auto value : *entityList) {
                auto obj = value.get<picojson::object>();

                if (obj.count("name")) {
                    auto relativeName = obj["name"].get<string>();
                    ecs::Name name(relativeName, scope);
                    if (!name) {
                        Errorf("Template %s contains invalid entity name: %s", sourceName, relativeName);
                        continue;
                    }
                    scene->NewPrefabEntity(lock, rootEnt, name);
                }
            }

            std::vector<ecs::Entity> entities;
            for (auto value : *entityList) {
                auto obj = value.get<picojson::object>();

                ecs::Entity newEntity;
                if (obj.count("name")) {
                    auto relativeName = obj["name"].get<string>();
                    ecs::Name entityName(relativeName, scope);
                    if (!entityName) {
                        Errorf("Template %s contains invalid entity name: %s", sourceName, relativeName);
                        continue;
                    }
                    newEntity = scene->GetStagingEntity(entityName);
                    if (!newEntity) {
                        Errorf("Skipping entity with invalid name: %s", entityName.String());
                        continue;
                    }
                } else {
                    newEntity = scene->NewPrefabEntity(lock, rootEnt);
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
                    if (!transform.parent) {
                        if (rootEnt.Has<TransformTree>(lock)) transform.parent = rootEnt;
                        transform.pose = offset * transform.pose;
                    }
                    newEntity.Set<ecs::TransformSnapshot>(lock);
                }

                entities.emplace_back(newEntity);
            }

            for (auto &e : entities) {
                if (e.Has<ecs::Script>(lock)) { e.Get<ecs::Script>(lock).Prefab(lock, e); }
            }
        }
    };

    InternalPrefab templatePrefab("template", [](ScriptState &state, Lock<AddRemove> lock, Entity ent) {
        TemplateParser parser(state, ent);
        if (!parser.Parse(lock)) return;

        parser.ApplyComponents(lock);
        parser.AddEntities(lock);
    });

    InternalPrefab tilePrefab("tile", [](ScriptState &state, Lock<AddRemove> lock, Entity ent) {
        TemplateParser parser(state, ent);
        if (!parser.Parse(lock)) return;
        parser.ApplyComponents(lock);

        glm::ivec3 count = {1, 1, 1};
        glm::vec3 stride = {1, 1, 1};

        if (state.HasParam<vector<double>>("count")) {
            auto &source = state.GetParamRef<vector<double>>("count");
            for (size_t i = 0; i < (size_t)count.length() && i < source.size(); i++) {
                count[i] = source[i];
            }
        }
        if (state.HasParam<vector<double>>("stride")) {
            auto &source = state.GetParamRef<vector<double>>("stride");
            for (size_t i = 0; i < (size_t)count.length() && i < source.size(); i++) {
                stride[i] = source[i];
            }
        }

        string scope;
        auto appendIndex = [&scope](int index) {
            if (!scope.empty()) scope += "_";
            scope += std::to_string(index);
        };

        for (int x = 0; x < count.x; x++) {
            for (int y = 0; y < count.y; y++) {
                for (int z = 0; z < count.z; z++) {
                    scope.clear();
                    if (count.x > 1) appendIndex(x);
                    if (count.y > 1) appendIndex(y);
                    if (count.z > 1) appendIndex(z);

                    Transform offset;
                    offset.Translate({float(x) * stride.x, float(y) * stride.y, float(z) * stride.z});
                    parser.AddEntities(lock, scope, offset);
                }
            }
        }
    });
} // namespace ecs
