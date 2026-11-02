/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ecs/EcsImpl.hh"
#include "ecs/ScriptImpl.hh"
#include "game/Scene.hh"
#include "strayphotons/Logging.hh"

#include <glm/glm.hpp>
#include <string>

namespace sp::scripts {
    using namespace ecs;

    struct LifeCellPrefab {
        glm::uvec2 boardSize = glm::uvec2(32, 32);

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            ZoneScopedN("LifeCellPrefab");
            if (!ent.Has<Name, EventBindings>(lock)) {
                Errorf("LifeCellPrefab requires Name, and EventBindings: %s", ToString(lock, ent));
                return;
            }

            auto &name = ent.Get<const Name>(lock);
            auto &eventBindings = ent.Get<EventBindings>(lock);
            auto &signalOutput = ent.Get<SignalOutput>(lock).signals;
            auto &signalBindings = ent.Get<SignalBindings>(lock).bindings;
            signalBindings.reserve(signalBindings.size() + 8);

            Name prefix(name.scene, name.entity.substr(0, name.entity.find_last_of('.')));
            glm::uvec2 pos(signalOutput.at("tile.x"), signalOutput.at("tile.y"));

            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;

                    glm::uvec2 wrapped = (pos + glm::uvec2(boardSize.x + dx, boardSize.y + dy)) % boardSize;
                    sp::InlineString<127> relativeName(127, '\0');
                    int count = std::snprintf(relativeName.data(), sizeof(relativeName), "%u_%u", wrapped.x, wrapped.y);
                    relativeName.resize(count);
                    EntityRef neighbor = Name(relativeName, prefix);

                    sp::InlineString<127> bindingName(127, '\0');
                    count = std::snprintf(bindingName.data(), sizeof(bindingName), "neighbor[%d][%d]", dx, dy);
                    bindingName.resize(count);
                    signalBindings.emplace(bindingName, SignalRef(neighbor, "alive"));

                    eventBindings.Bind("/life/notify_neighbors", neighbor, "/life/neighbor_alive");
                }
            }
        }
    };
    StructMetadata MetadataLifeCellPrefab(typeid(LifeCellPrefab),
        sizeof(LifeCellPrefab),
        "LifeCellPrefab",
        "",
        StructField::New("board_size", &LifeCellPrefab::boardSize));
    BuiltInScript<LifeCellPrefab> lifeCellPrefab("prefab_life_cell", MetadataLifeCellPrefab);
} // namespace sp::scripts
