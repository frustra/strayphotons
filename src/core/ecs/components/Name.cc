#include "Name.hh"

#include "core/Common.hh"
#include "core/Hashing.hh"
#include "game/Scene.hh"

namespace ecs {
    Name::Name(std::string_view scene, std::string_view entity) : scene(scene), entity(entity) {
        Assertf(scene.find_first_of(":/ ") == std::string::npos, "Scene name has invalid character: '%s'", scene);
        Assertf(entity.find_first_of(":/ ") == std::string::npos, "Entity name has invalid character: '%s'", scene);
    }

    bool Name::Parse(std::string_view fullName, const Name &scope) {
        size_t i = fullName.find(':');
        if (i != std::string::npos) {
            scene = fullName.substr(0, i);
            entity = fullName.substr(i + 1);
        } else if (!scope.scene.empty()) {
            scene = scope.scene;
            if (scope.entity.empty()) {
                entity = fullName;
            } else if (!sp::starts_with(fullName, scope.entity + ".")) {
                entity = scope.entity + "." + std::string(fullName);
            }
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

    std::ostream &operator<<(std::ostream &out, const Name &v) {
        return out << v.String();
    }
} // namespace ecs

namespace std {
    std::size_t hash<ecs::Name>::operator()(const ecs::Name &n) const {
        auto val = hash<string>()(n.scene);
        sp::hash_combine(val, n.entity);
        return val;
    }
} // namespace std
