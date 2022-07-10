#include "ComponentMetadata.hh"

#include "assets/JsonHelpers.hh"

namespace ecs {
    template<typename T>
    bool LoadField(void *dst, const picojson::value &src) {
        auto &field = *reinterpret_cast<T *>(dst);
        if (!sp::json::Load(field, src)) {
            Errorf("Invalid %s field value: %s", typeid(T).name(), src.to_str());
            return false;
        }
        return true;
    }

    bool ComponentField::Load(const EntityScope &scope, void *component, const picojson::value &src) const {
        if (!src.is<picojson::object>()) {
            Errorf("ComponentField::Load invalid component object: %s", src.to_str());
            return false;
        }
        auto &obj = src.get<picojson::object>();
        if (!src.contains(name)) {
            // Silently leave missing fields as default
            return true;
        }

        auto *fieldDst = static_cast<char *>(component) + offset;
        auto &fieldSrc = obj.at(name);

        if (type == FieldType::Bool) {
            return LoadField<bool>(fieldDst, fieldSrc);
        } else if (type == FieldType::Int32) {
            return LoadField<int32_t>(fieldDst, fieldSrc);
        } else if (type == FieldType::Uint32) {
            return LoadField<uint32_t>(fieldDst, fieldSrc);
        } else if (type == FieldType::SizeT) {
            return LoadField<size_t>(fieldDst, fieldSrc);
        } else if (type == FieldType::Float) {
            return LoadField<float>(fieldDst, fieldSrc);
        } else if (type == FieldType::Double) {
            return LoadField<double>(fieldDst, fieldSrc);
        } else if (type == FieldType::Vec2) {
            return LoadField<glm::vec2>(fieldDst, fieldSrc);
        } else if (type == FieldType::Vec3) {
            return LoadField<glm::vec3>(fieldDst, fieldSrc);
        } else if (type == FieldType::Vec4) {
            return LoadField<glm::vec4>(fieldDst, fieldSrc);
        } else if (type == FieldType::String) {
            return LoadField<std::string>(fieldDst, fieldSrc);
        } else {
            Abortf("ComponentField::Load unknown component field type: %u", type);
        }
    }

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
        } else if (type == FieldType::Int32) {
            auto &value = *reinterpret_cast<const int32_t *>(field);
            auto &defaultValue = *reinterpret_cast<const int32_t *>(defaultField);

            if (value != defaultValue) {
                obj[name] = picojson::value((double)value);
                return true;
            }
        } else if (type == FieldType::Uint32) {
            auto &value = *reinterpret_cast<const uint32_t *>(field);
            auto &defaultValue = *reinterpret_cast<const uint32_t *>(defaultField);

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
        } else if (type == FieldType::Double) {
            auto &value = *reinterpret_cast<const double *>(field);
            auto &defaultValue = *reinterpret_cast<const double *>(defaultField);

            if (value != defaultValue) {
                obj[name] = picojson::value(value);
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
        } else if (type == FieldType::Vec4) {
            auto &value = *reinterpret_cast<const glm::vec4 *>(field);
            auto &defaultValue = *reinterpret_cast<const glm::vec4 *>(defaultField);

            return sp::json::SaveIfChanged(obj, name, value, defaultValue);
        } else if (type == FieldType::String) {
            auto &value = *reinterpret_cast<const std::string *>(field);
            auto &defaultValue = *reinterpret_cast<const std::string *>(defaultField);

            if (value != defaultValue) {
                obj[name] = picojson::value(value);
                return true;
            }
        } else {
            Abortf("ComponentField::SaveIfChanged unknown component field type: %u", type);
        }
        return false;
    }
} // namespace ecs
