#include "Ecs.hh"

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <typeindex>

namespace ecs {
    ECS &World() {
        static struct {
            sp::LogOnExit logOnExit = "ECS shut down =========================================================";
            ECS instance;
        } world;
        return world.instance;
    }

    ECS &StagingWorld() {
        static ECS stagingWorld;
        return stagingWorld;
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
