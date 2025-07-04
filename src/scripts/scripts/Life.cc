/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptImpl.hh"
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
                    auto *neighborAlive = EventData::TryGet<bool>(event.data);
                    if (neighborAlive == nullptr) continue;
                    neighborCount += *neighborAlive ? 1 : -1;
                } else if (event.name == "/life/toggle_alive") {
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
    StructMetadata MetadataLifeCell(typeid(LifeCell),
        sizeof(LifeCell),
        "LifeCell",
        "",
        StructField::New("alive", &LifeCell::alive),
        StructField::New("initialized", &LifeCell::initialized, FieldAction::None),
        StructField::New("neighbor_count", &LifeCell::neighborCount, FieldAction::None));
    // LogicScript<LifeCell> lifeCell("life_cell", MetadataLifeCell, false, "/life/neighbor_alive",
    // "/life/toggle_alive");
} // namespace sp::scripts
