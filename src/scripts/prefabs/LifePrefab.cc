#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <cmath>
#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct LifeCellPrefab {
        glm::uvec2 boardSize = glm::uvec2(32, 32);

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            if (!ent.Has<Name, EventBindings>(lock)) {
                Errorf("LifeCellPrefab requires Name, and EventBindings: %s", ToString(lock, ent));
                return;
            }

            auto &name = ent.Get<const Name>(lock);
            auto &eventBindings = ent.Get<EventBindings>(lock);

            auto prefix = Name(name.scene, name.entity.substr(0, name.entity.find_last_of('.')));
            glm::uvec2 pos = glm::uvec2(SignalRef(ent, "tile.x").GetSignal(lock),
                SignalRef(ent, "tile.y").GetSignal(lock));

            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;

                    glm::uvec2 wrapped = (pos + glm::uvec2(boardSize.x + dx, boardSize.y + dy)) % boardSize;
                    EntityRef neighbor = Name(std::to_string(wrapped.x) + "_" + std::to_string(wrapped.y), prefix);

                    std::string bindingName = "neighbor[" + std::to_string(dx) + "][" + std::to_string(dy) + "]";
                    SignalRef(ent, bindingName).SetBinding(lock, SignalRef(neighbor, "alive"));

                    eventBindings.Bind("/life/notify_neighbors", neighbor, "/life/neighbor_alive");
                }
            }
        }
    };
    StructMetadata MetadataLifeCellPrefab(typeid(LifeCellPrefab),
        StructField::New("board_size", &LifeCellPrefab::boardSize));
    PrefabScript<LifeCellPrefab> lifeCellPrefab("life_cell", MetadataLifeCellPrefab);
} // namespace sp::scripts
