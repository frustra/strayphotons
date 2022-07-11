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

    template<typename T>
    bool SaveField(picojson::object &dst, const char *fieldName, const void *field, const void *defaultField) {
        auto &value = *reinterpret_cast<const T *>(field);
        auto &defaultValue = *reinterpret_cast<const T *>(defaultField);
        return sp::json::SaveIfChanged(dst, fieldName, value, defaultValue);
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
            return SaveField<bool>(obj, name, field, defaultField);
        } else if (type == FieldType::Int32) {
            return SaveField<int32_t>(obj, name, field, defaultField);
        } else if (type == FieldType::Uint32) {
            return SaveField<uint32_t>(obj, name, field, defaultField);
        } else if (type == FieldType::Float) {
            return SaveField<float>(obj, name, field, defaultField);
        } else if (type == FieldType::Double) {
            return SaveField<double>(obj, name, field, defaultField);
        } else if (type == FieldType::Vec2) {
            return SaveField<glm::vec2>(obj, name, field, defaultField);
        } else if (type == FieldType::Vec3) {
            return SaveField<glm::vec3>(obj, name, field, defaultField);
        } else if (type == FieldType::Vec4) {
            return SaveField<glm::vec4>(obj, name, field, defaultField);
        } else if (type == FieldType::String) {
            return SaveField<std::string>(obj, name, field, defaultField);
        } else {
            Abortf("ComponentField::SaveIfChanged unknown component field type: %u", type);
        }
    }
} // namespace ecs
