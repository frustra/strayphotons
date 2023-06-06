/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Ecs.hh"

#include "core/DispatchQueue.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <typeindex>

namespace ecs {
    auto &ECSContext() {
        static struct {
            sp::LogOnExit logOnExit = "ECS shut down =========================================================";
            ECS live;
            ECS staging;
            sp::DispatchQueue transactionQueue = sp::DispatchQueue("ECSTransactionQueue");
        } context;
        return context;
    }

    ECS &World() {
        return ECSContext().live;
    }

    ECS &StagingWorld() {
        return ECSContext().staging;
    }

    sp::DispatchQueue &TransactionQueue() {
        return ECSContext().transactionQueue;
    }

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    int getComponentIndex(ECSType<AllComponentTypes...> *, const std::string &componentName) {
        static const std::array<std::string, sizeof...(AllComponentTypes)> componentNames = {[&] {
            if constexpr (Tecs::is_global_component<AllComponentTypes>()) {
                return "";
            } else {
                auto comp = LookupComponent(typeid(AllComponentTypes));
                Assertf(comp, "Unknown component name: %s", typeid(AllComponentTypes).name());
                return comp->name;
            }
        }()...};

        auto it = std::find(componentNames.begin(), componentNames.end(), componentName);
        if (it == componentNames.end()) return -1;
        return it - componentNames.begin();
    }

    int GetComponentIndex(const std::string &componentName) {
        return getComponentIndex((ECS *)nullptr, componentName);
    }

    std::string ToString(Lock<Read<Name>> lock, Entity e) {
        if (!e.Has<Name>(lock)) return std::to_string(e);
        auto generation = Tecs::GenerationWithoutIdentifier(e.generation);
        return e.Get<Name>(lock).String() + "(" + (IsLive(e) ? "gen " : "staging gen ") + std::to_string(generation) +
               ", index " + std::to_string(e.index) + ")";
    }
} // namespace ecs
