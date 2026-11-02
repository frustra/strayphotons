/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GameLogic.hh"

#include "common/Tracing.hh"
#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EventScriptManager.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/components/Events.hh"
#include "strayphotons/LockFreeEventQueue.hh"
#include "strayphotons/Utility.hh"
#include "strayphotons/input/BindingNames.hh"
#include "strayphotons/input/KeyCodes.hh"

#include <chrono>

namespace sp {
    static CVar<uint32_t> CVarLogicFPS("g.LogicFPS", 144, "Target frame rate for game logic scripts (0 for unlimited)");

    GameLogic::GameLogic(LockFreeEventQueue<ecs::Event> &windowInputQueue)
        : RegisteredThread("GameLogic", CVarLogicFPS.Get(), true), windowInputQueue(windowInputQueue) {
        funcs.Register<unsigned int>("steplogic",
            "Advance the game logic by N frames, default is 1",
            [this](unsigned int arg) {
                this->Step(std::max(1u, arg));
            });
        funcs.Register("pauselogic", "Pause the game logic thread (See also: resumelogic)", [this] {
            this->Pause(true);
        });
        funcs.Register("resumelogic", "Pause the game logic thread (See also: pauselogic)", [this] {
            this->Pause(false);
        });
    }

    void GameLogic::StartThread(bool startPaused) {
        RegisteredThread::StartThread(startPaused);
    }

    bool GameLogic::PreFrame() {
        auto targetFPS = CVarLogicFPS.Get();
        if (targetFPS > 0) {
            interval = std::chrono::nanoseconds((int64_t)(1e9 / targetFPS));
        } else {
            interval = std::chrono::nanoseconds(0);
        }
        return true;
    }

    void GameLogic::Frame() {
        ZoneScoped;
        {
            ZoneScopedN("RunLogicUpdate");
            ecs::LogicUpdateLock lock = ecs::StartTransaction<ecs::LogicUpdateLock>();
            ecs::EventScriptManager::UpdateInputEvents(lock, windowInputQueue);
            ecs::GetScriptManager().RunLogicUpdate(lock, interval);
        }
    }
} // namespace sp
