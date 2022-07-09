#include "ComponentMetadata.hh"

#include "assets/JsonHelpers.hh"

namespace ecs {
    bool ComponentField::SaveIfChanged(picojson::value &dst,
        const void *component,
        const void *defaultComponent) const {
        auto *field = static_cast<const char *>(component) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        if (type == FieldType::Bool) {
            auto &value = *reinterpret_cast<const bool *>(field);
            auto &defaultValue = *reinterpret_cast<const bool *>(defaultField);

            if (value != defaultValue) {
                obj[name] = picojson::value(value);
                return true;
            }
        } else if (type == FieldType::Int) {
            auto &value = *reinterpret_cast<const int *>(field);
            auto &defaultValue = *reinterpret_cast<const int *>(defaultField);

            if (value != defaultValue) {
                obj[name] = picojson::value((double)value);
                return true;
            }
        } else if (type == FieldType::Uint) {
            auto &value = *reinterpret_cast<const unsigned int *>(field);
            auto &defaultValue = *reinterpret_cast<const unsigned int *>(defaultField);

            if (value != defaultValue) {
                obj[name] = picojson::value((double)value);
                return true;
            }
        } else if (type == FieldType::Float) {
            auto &value = *reinterpret_cast<const float *>(field);
            auto &defaultValue = *reinterpret_cast<const float *>(defaultField);

            if (value != defaultValue) {
                obj[name] = picojson::value((double)value);
                return true;
            }
        } else if (type == FieldType::Vec2) {
            auto &value = *reinterpret_cast<const glm::vec2 *>(field);
            auto &defaultValue = *reinterpret_cast<const glm::vec2 *>(defaultField);

            return sp::json::SaveIfChanged(obj, name, value, defaultValue);
        } else if (type == FieldType::Vec3) {
            auto &value = *reinterpret_cast<const glm::vec3 *>(field);
            auto &defaultValue = *reinterpret_cast<const glm::vec3 *>(defaultField);

            return sp::json::SaveIfChanged(obj, name, value, defaultValue);
        } else if (type == FieldType::String) {
            auto &value = *reinterpret_cast<const std::string *>(field);
            auto &defaultValue = *reinterpret_cast<const std::string *>(defaultField);

            if (value != defaultValue) {
                obj[name] = picojson::value(value);
                return true;
            }
        } else {
            Abortf("Unknown component field type: %u", type);
        }
        return false;
    }
} // namespace ecs
