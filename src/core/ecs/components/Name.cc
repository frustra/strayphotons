#include "Name.hh"

#include "core/Common.hh"
#include "core/Hashing.hh"
#include "game/Scene.hh"

#include <picojson/picojson.h>

namespace ecs {
    bool Name::Parse(std::string fullName, const sp::Scene *currentScene) {
        size_t i = fullName.find(':');
        if (i != std::string::npos) {
            scene = fullName.substr(0, i);
            entity = fullName.substr(i + 1);
        } else if (currentScene != nullptr) {
            scene = currentScene->name;
            entity = fullName;
        } else {
            Errorf("Invalid name has no scene: %s", fullName);
            return false;
        }
        if (scene.find_first_of(":/ ") != std::string::npos) {
            Errorf("Scene name has invalid character: '%s'", scene);
            return false;
        }
        if (entity.find_first_of(":/ ") != std::string::npos) {
            Errorf("Entity name has invalid character: '%s'", entity);
            return false;
        }
        return true;
    }

    template<>
    bool Component<Name>::Load(sp::Scene *scene, Name &name, const picojson::value &src) {
        return name.Parse(src.get<std::string>(), scene);
    }
} // namespace ecs

namespace std {
    std::size_t hash<ecs::Name>::operator()(const ecs::Name &n) const {
        auto val = hash<string>()(n.scene);
        sp::hash_combine(val, n.entity);
        return val;
    }
} // namespace std
