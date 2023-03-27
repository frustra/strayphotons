#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <picojson/picojson.h>

namespace ecs {
    struct TemplateParser {
        std::shared_ptr<sp::Scene> scene;
        Entity rootEnt;
        size_t prefabScriptId;
        std::string sourceName;

        ecs::EntityScope rootScope;
        std::shared_ptr<sp::Asset> asset;
        picojson::value root;

        picojson::object *rootObj = nullptr, *componentsObj = nullptr;
        picojson::array *entityList = nullptr;

        TemplateParser(const std::shared_ptr<sp::Scene> &scene,
            Entity rootEnt,
            size_t prefabScriptId,
            std::string source)
            : scene(scene), rootEnt(rootEnt), prefabScriptId(prefabScriptId), sourceName(source) {}

        bool Parse(Lock<AddRemove> lock) {
            if (sourceName.empty()) return false;

            if (rootEnt.Has<Name>(lock)) {
                rootScope = rootEnt.Get<Name>(lock);
            } else {
                rootScope = ecs::Name(scene->data->name, "");
            }

            Debugf("Loading template: %s with scope '%s'", sourceName, rootScope.String());

            asset = sp::Assets().Load("scenes/templates/" + sourceName + ".json", sp::AssetType::Bundled, true)->Get();
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

        // Add defined components to the template root entity with the given name
        Entity ApplyComponents(const picojson::object &source,
            Lock<AddRemove> lock,
            std::string name,
            ecs::EntityScope scope,
            Transform offset = {}) {
            Entity newEntity = scene->NewPrefabEntity(lock, rootEnt, prefabScriptId, name, scope);
            for (auto &comp : source) {
                if (comp.first.empty() || comp.first[0] == '_' || comp.first == "name") continue;

                auto componentType = ecs::LookupComponent(comp.first);
                if (componentType != nullptr) {
                    if (!componentType->LoadEntity(lock, scope, newEntity, comp.second)) {
                        Errorf("LoadTemplate(%s): Failed to load component, ignoring: %s", sourceName, comp.first);
                    }
                } else {
                    Errorf("LoadTemplate(%s): Unknown component, ignoring: %s", sourceName, comp.first);
                }
            }
            if (newEntity.Has<ecs::TransformTree>(lock)) {
                auto &transform = newEntity.Get<ecs::TransformTree>(lock);
                if (!transform.parent) {
                    if (rootEnt.Has<TransformTree>(lock)) transform.parent = rootEnt;
                    transform.pose = offset * transform.pose.Get();
                }
            }
            return newEntity;
        }

        // Add defined entities as sub-entities of the template root
        void AddEntities(Lock<AddRemove> lock, ecs::EntityScope scope, Transform offset = {}) {
            if (!entityList) return;

            std::vector<ecs::Entity> entities;
            for (auto &value : *entityList) {
                auto &obj = value.get<picojson::object>();

                bool hasName = obj.count("name") && obj["name"].is<string>();
                auto relativeName = hasName ? obj["name"].get<string>() : "";
                if (relativeName == "scoperoot") {
                    Errorf("Entity name 'scoperoot' in template not allowed, ignoring");
                    continue;
                }

                Entity newEntity = ApplyComponents(obj, lock, relativeName, scope, offset);
                entities.emplace_back(newEntity);
            }

            for (auto &e : entities) {
                if (e.Has<ecs::Scripts>(lock)) {
                    ecs::Scripts::RunPrefabs(lock, e);
                }
            }
        }
    };

    struct TemplatePrefab {
        std::string source;

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            TemplateParser parser(scene, ent, state.GetInstanceId(), source);
            if (!parser.Parse(lock)) return;

            Entity rootOverride;
            if (parser.componentsObj) {
                rootOverride = parser.ApplyComponents(*parser.componentsObj, lock, "scoperoot", parser.rootScope);
            }
            parser.AddEntities(lock, parser.rootScope);
            if (rootOverride.Has<ecs::Scripts>(lock)) {
                ecs::Scripts::RunPrefabs(lock, rootOverride);
            }
        }
    };
    StructMetadata MetadataTemplatePrefab(typeid(TemplatePrefab), StructField::New("source", &TemplatePrefab::source));
    PrefabScript<TemplatePrefab> templatePrefab("template", MetadataTemplatePrefab);

    struct TilePrefab {
        glm::ivec2 count = {1, 1};
        glm::vec2 stride = {1, 1};
        std::string axes = "xy";
        std::string surfaceTemplate, edgeTemplate, cornerTemplate;

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            if (axes.size() != 2) {
                Errorf("'%s' axes are invalid, must tile on 2 unique axes: %s", axes, ToString(lock, ent));
                return;
            }
            std::pair<int, int> axesIndex = {axes[0] - 'x', axes[1] - 'x'};
            if (axesIndex.first < 0 || axesIndex.second < 0 || axesIndex.first > 2 || axesIndex.second > 2) {
                Errorf("'%s' axes are invalid, must tile on x, y, or z: %s", axes, ToString(lock, ent));
                return;
            } else if (axesIndex.first == axesIndex.second) {
                Errorf("'%s' axes are invalid, must tile on 2 unique axes: %s", axes, ToString(lock, ent));
                return;
            }

            TemplateParser surface(scene, ent, state.GetInstanceId(), surfaceTemplate);
            if (!surface.Parse(lock)) return;

            TemplateParser edge(scene, ent, state.GetInstanceId(), edgeTemplate);
            auto haveEdge = edge.Parse(lock);

            TemplateParser corner(scene, ent, state.GetInstanceId(), cornerTemplate);
            auto haveCorner = corner.Parse(lock);

            glm::vec3 normal{1, 1, 1};
            normal[axesIndex.first] = 0;
            normal[axesIndex.second] = 0;

            for (int x = 0; x < count.x; x++) {
                for (int y = 0; y < count.y; y++) {
                    auto offset2D = (glm::vec2{float(x), float(y)} + glm::vec2(0.5)) * stride;

                    glm::vec3 offset3D;
                    offset3D[axesIndex.first] = offset2D.x;
                    offset3D[axesIndex.second] = offset2D.y;

                    ecs::EntityScope scope = ecs::Name(std::to_string(x) + "_" + std::to_string(y), surface.rootScope);

                    ecs::Entity tileEnt;
                    if (surface.componentsObj) {
                        tileEnt = surface.ApplyComponents(*surface.componentsObj, lock, "scoperoot", scope, offset3D);
                    } else {
                        static const picojson::object empty = {};
                        tileEnt = surface.ApplyComponents(empty, lock, "scoperoot", scope, offset3D);
                    }
                    Assertf(tileEnt, "Failed to create tile entity: %s", ToString(lock, ent));
                    auto &signalOutput = tileEnt.Get<ecs::SignalOutput>(lock);
                    signalOutput.SetSignal("tile.x", x);
                    signalOutput.SetSignal("tile.y", y);
                    surface.AddEntities(lock, scope, offset3D);

                    auto xEdge = x == 0 || x == (count.x - 1);
                    auto yEdge = y == 0 || y == (count.y - 1);

                    if (xEdge && yEdge) {
                        if (haveCorner) {
                            Transform transform;
                            if (x != 0 && y != 0) {
                                transform.Rotate(M_PI, normal);
                            } else if (x != 0) {
                                transform.Rotate(-M_PI / 2, normal);
                            } else if (y != 0) {
                                transform.Rotate(M_PI / 2, normal);
                            }
                            transform.Translate(offset3D);
                            if (corner.componentsObj) {
                                corner.ApplyComponents(*corner.componentsObj, lock, "scoperoot", scope, transform);
                            }
                            corner.AddEntities(lock, scope, transform);
                        }
                    } else if (xEdge || yEdge) {
                        if (haveEdge) {
                            Transform transform;
                            if (x == (count.x - 1)) {
                                transform.Rotate(-M_PI / 2, normal);
                            } else if (y == (count.y - 1)) {
                                transform.Rotate(M_PI, normal);
                            } else if (x == 0) {
                                transform.Rotate(M_PI / 2, normal);
                            }
                            transform.Translate(offset3D);
                            if (edge.componentsObj) {
                                edge.ApplyComponents(*edge.componentsObj, lock, "scoperoot", scope, transform);
                            }
                            edge.AddEntities(lock, scope, transform);
                        }
                    }
                    if (tileEnt.Has<ecs::Scripts>(lock)) {
                        ecs::Scripts::RunPrefabs(lock, tileEnt);
                    }
                }
            }
        }
    };
    StructMetadata MetadataTilePrefab(typeid(TilePrefab),
        StructField::New("count", &TilePrefab::count),
        StructField::New("stride", &TilePrefab::stride),
        StructField::New("axes", &TilePrefab::axes),
        StructField::New("surface", &TilePrefab::surfaceTemplate),
        StructField::New("edge", &TilePrefab::edgeTemplate),
        StructField::New("corner", &TilePrefab::cornerTemplate));
    PrefabScript<TilePrefab> tilePrefab("tile", MetadataTilePrefab);
} // namespace ecs
