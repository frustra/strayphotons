/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "AnimationSystem.hh"

#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"

namespace sp {
    AnimationSystem::AnimationSystem(PhysxManager &manager) : frameInterval(manager.interval.count() / 1e9) {}

    void AnimationSystem::Frame(ecs::Lock<ecs::ReadSignalsLock,
        ecs::Read<ecs::Animation, ecs::LightSensor, ecs::LaserSensor>,
        ecs::Write<ecs::Signals, ecs::TransformTree>> lock) {
        ZoneScoped;
        for (auto ent : lock.EntitiesWith<ecs::Animation>()) {
            if (!ent.Has<ecs::Animation>(lock)) continue;
            auto &animation = ent.Get<ecs::Animation>(lock);
            if (animation.states.empty()) continue;

            ecs::SignalRef stateRef(ent, "animation_state");
            double currentState = stateRef.GetSignal(lock);
            double targetState = ecs::SignalRef(ent, "animation_target").GetSignal(lock);
            double originalState = currentState;
            currentState = std::clamp(currentState, 0.0, animation.states.size() - 1.0);
            targetState = std::clamp(targetState, 0.0, animation.states.size() - 1.0);
            auto state = animation.GetCurrNextState(currentState, targetState);
            if (targetState != currentState) {
                auto &next = animation.states[state.next];

                double frameDelta = frameInterval / std::max(next.delay, frameInterval);
                if (frameDelta >= std::abs(targetState - currentState)) {
                    currentState = targetState;
                } else {
                    currentState += state.direction * frameDelta;
                }
            }

            ecs::Animation::UpdateTransform(lock, ent);

            if (originalState != currentState) {
                stateRef.SetValue(lock, currentState);
            }
        }
    }
} // namespace sp
