#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"

#include <cmath>
#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct LifeCell {
        int neighborCount = 0;
        bool alive = false;
        bool initialized = false;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!initialized) {
                if (alive) EventBindings::SendEvent(lock, ent, Event{"/life/notify_neighbors", ent, alive});
                initialized = true;
                return;
            }

            bool forceToggle = false;
            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name == "/life/neighbor_alive") {
                    auto *neighborAlive = std::get_if<bool>(&event.data);
                    if (neighborAlive == nullptr) continue;
                    neighborCount += *neighborAlive ? 1 : -1;
                } else if (event.name == "/physics/collision/force_found") {
                    forceToggle = true;
                }
            }

            bool nextAlive = neighborCount == 3 || (neighborCount == 2 && alive);
            if (forceToggle || nextAlive != alive) {
                alive = !alive;
                EventBindings::SendEvent(lock, ent, Event{"/life/notify_neighbors", ent, alive});
            }
        }
    };
    StructMetadata MetadataLifeCell(typeid(LifeCell), StructField::New("alive", &LifeCell::alive));
    InternalScript<LifeCell> lifeCell("life_cell",
        MetadataLifeCell,
        false,
        "/life/neighbor_alive",
        "/physics/collision/force_found");
} // namespace sp::scripts
