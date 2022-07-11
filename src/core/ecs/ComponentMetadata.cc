#include "ComponentMetadata.hh"

#include "assets/JsonHelpers.hh"

#include <cstring>

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

        switch (type) {
        case FieldType::Bool:
            return LoadField<bool>(fieldDst, fieldSrc);
        case FieldType::Int32:
            return LoadField<int32_t>(fieldDst, fieldSrc);
        case FieldType::Uint32:
            return LoadField<uint32_t>(fieldDst, fieldSrc);
        case FieldType::SizeT:
            return LoadField<size_t>(fieldDst, fieldSrc);
        case FieldType::AngleT:
            return LoadField<sp::angle_t>(fieldDst, fieldSrc);
        case FieldType::Float:
            return LoadField<float>(fieldDst, fieldSrc);
        case FieldType::Double:
            return LoadField<double>(fieldDst, fieldSrc);
        case FieldType::Vec2:
            return LoadField<glm::vec2>(fieldDst, fieldSrc);
        case FieldType::Vec3:
            return LoadField<glm::vec3>(fieldDst, fieldSrc);
        case FieldType::Vec4:
            return LoadField<glm::vec4>(fieldDst, fieldSrc);
        case FieldType::String:
            return LoadField<std::string>(fieldDst, fieldSrc);
        default:
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

        switch (type) {
        case FieldType::Bool:
            return SaveField<bool>(obj, name, field, defaultField);
        case FieldType::Int32:
            return SaveField<int32_t>(obj, name, field, defaultField);
        case FieldType::Uint32:
            return SaveField<uint32_t>(obj, name, field, defaultField);
        case FieldType::SizeT:
            return SaveField<size_t>(obj, name, field, defaultField);
        case FieldType::AngleT:
            return SaveField<sp::angle_t>(obj, name, field, defaultField);
        case FieldType::Float:
            return SaveField<float>(obj, name, field, defaultField);
        case FieldType::Double:
            return SaveField<double>(obj, name, field, defaultField);
        case FieldType::Vec2:
            return SaveField<glm::vec2>(obj, name, field, defaultField);
        case FieldType::Vec3:
            return SaveField<glm::vec3>(obj, name, field, defaultField);
        case FieldType::Vec4:
            return SaveField<glm::vec4>(obj, name, field, defaultField);
        case FieldType::String:
            return SaveField<std::string>(obj, name, field, defaultField);
        default:
            Abortf("ComponentField::SaveIfChanged unknown component field type: %u", type);
        }
    }

    void ComponentField::Apply(void *dstComponent, const void *srcComponent, const void *defaultComponent) const {
        auto *dstField = static_cast<char *>(dstComponent) + offset;
        auto *srcField = static_cast<const char *>(srcComponent) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        if (std::memcmp(dstField, defaultField, size) == 0) {
            std::memcpy(dstField, srcField, size);
        }
    }
} // namespace ecs
