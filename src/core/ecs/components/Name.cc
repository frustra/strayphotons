#include "Name.hh"

#include "core/Common.hh"
#include "core/Hashing.hh"
#include "core/Logging.hh"

namespace ecs {
    Name::Name(std::string_view scene, std::string_view entity) : scene(scene), entity(entity) {
        Assertf(scene.find_first_of(",():/# ") == std::string::npos, "Scene name has invalid character: '%s'", scene);
        Assertf(entity.find_first_of(",():/# ") == std::string::npos,
            "Entity name has invalid character: '%s'",
            entity);
        Assertf(scene.find('-') != 0, "Scene name must not start with '-': '%s'", scene);
        Assertf(entity.find('-') != 0, "Entity name must not start with '-': '%s'", entity);
    }

    Name::Name(std::string_view relativeName, const EntityScope &scope) {
        Parse(relativeName, scope);
    }

    bool Name::Parse(std::string_view relativeName, const EntityScope &scope) {
        size_t i = relativeName.find(':');
        if (i != std::string::npos) {
            scene = relativeName.substr(0, i);
            entity = relativeName.substr(i + 1);
        } else if (!scope.scene.empty()) {
            scene = scope.scene;
            if (relativeName == "scoperoot") {
                if (scope.entity.empty()) {
                    scene.clear();
                    entity.clear();
                    Errorf("Invalid scope root name has invalid scope: %s", scope.String());
                    return false;
                } else {
                    entity = scope.entity;
                }
            } else {
                if (scope.entity.empty()) {
                    entity = relativeName;
                } else {
                    entity = scope.entity + "." + std::string(relativeName);
                }
            }
        } else {
            scene.clear();
            entity.clear();
            Errorf("Invalid name has no scene: %s", std::string(relativeName));
            return false;
        }
        if (scene.find_first_of(",():/# ") != std::string::npos) {
            scene.clear();
            entity.clear();
            Errorf("Scene name has invalid character: '%s'", scene);
            return false;
        } else if (scene.find('-') == 0) {
            scene.clear();
            entity.clear();
            Errorf("Scene name must not start with '-': '%s'", scene);
            return false;
        }
        if (entity.find_first_of(",():/# ") != std::string::npos) {
            scene.clear();
            entity.clear();
            Errorf("Entity name has invalid character: '%s'", entity);
            return false;
        } else if (entity.find('-') == 0) {
            scene.clear();
            entity.clear();
            Errorf("Entity name must not start with '-': '%s'", entity);
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
