#include "ComponentMetadata.hh"

#include "assets/JsonHelpers.hh"

#include <cstring>

namespace ecs {
    template<typename T>
    bool LoadField(const EntityScope &scope, void *dst, const picojson::value &src) {
        auto &field = *reinterpret_cast<T *>(dst);
        if (!sp::json::Load(scope, field, src)) {
            Errorf("Invalid %s field value: %s", typeid(T).name(), src.to_str());
            return false;
        }
        return true;
    }

    template<typename T>
    bool SaveField(const EntityScope &scope,
        picojson::object &dst,
        const char *fieldName,
        const void *field,
        const void *defaultField) {
        auto &value = *reinterpret_cast<const T *>(field);
        auto &defaultValue = *reinterpret_cast<const T *>(defaultField);
        return sp::json::SaveIfChanged(scope, dst, fieldName, value, defaultValue);
    }

    template<typename T>
    void ApplyField(void *dstField, const void *srcField, const void *defaultField) {
        auto &dstValue = *reinterpret_cast<T *>(dstField);
        auto &srcValue = *reinterpret_cast<const T *>(srcField);
        auto &defaultValue = *reinterpret_cast<const T *>(defaultField);

        if (dstValue == defaultValue) dstValue = srcValue;
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
            return LoadField<bool>(scope, fieldDst, fieldSrc);
        case FieldType::Int32:
            return LoadField<int32_t>(scope, fieldDst, fieldSrc);
        case FieldType::Uint32:
            return LoadField<uint32_t>(scope, fieldDst, fieldSrc);
        case FieldType::SizeT:
            return LoadField<size_t>(scope, fieldDst, fieldSrc);
        case FieldType::AngleT:
            return LoadField<sp::angle_t>(scope, fieldDst, fieldSrc);
        case FieldType::Float:
            return LoadField<float>(scope, fieldDst, fieldSrc);
        case FieldType::Double:
            return LoadField<double>(scope, fieldDst, fieldSrc);
        case FieldType::Vec2:
            return LoadField<glm::vec2>(scope, fieldDst, fieldSrc);
        case FieldType::Vec3:
            return LoadField<glm::vec3>(scope, fieldDst, fieldSrc);
        case FieldType::Vec4:
            return LoadField<glm::vec4>(scope, fieldDst, fieldSrc);
        case FieldType::String:
            return LoadField<std::string>(scope, fieldDst, fieldSrc);
        case FieldType::EntityRef:
            return LoadField<EntityRef>(scope, fieldDst, fieldSrc);
        default:
            Abortf("ComponentField::Load unknown component field type: %u", type);
        }
    }

    bool ComponentField::SaveIfChanged(const EntityScope &scope,
        picojson::value &dst,
        const void *component,
        const void *defaultComponent) const {
        auto *field = static_cast<const char *>(component) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        switch (type) {
        case FieldType::Bool:
            return SaveField<bool>(scope, obj, name, field, defaultField);
        case FieldType::Int32:
            return SaveField<int32_t>(scope, obj, name, field, defaultField);
        case FieldType::Uint32:
            return SaveField<uint32_t>(scope, obj, name, field, defaultField);
        case FieldType::SizeT:
            return SaveField<size_t>(scope, obj, name, field, defaultField);
        case FieldType::AngleT:
            return SaveField<sp::angle_t>(scope, obj, name, field, defaultField);
        case FieldType::Float:
            return SaveField<float>(scope, obj, name, field, defaultField);
        case FieldType::Double:
            return SaveField<double>(scope, obj, name, field, defaultField);
        case FieldType::Vec2:
            return SaveField<glm::vec2>(scope, obj, name, field, defaultField);
        case FieldType::Vec3:
            return SaveField<glm::vec3>(scope, obj, name, field, defaultField);
        case FieldType::Vec4:
            return SaveField<glm::vec4>(scope, obj, name, field, defaultField);
        case FieldType::String:
            return SaveField<std::string>(scope, obj, name, field, defaultField);
        case FieldType::EntityRef:
            return SaveField<EntityRef>(scope, obj, name, field, defaultField);
        default:
            Abortf("ComponentField::SaveIfChanged unknown component field type: %u", type);
        }
    }

    void ComponentField::Apply(void *dstComponent, const void *srcComponent, const void *defaultComponent) const {
        auto *dstField = static_cast<char *>(dstComponent) + offset;
        auto *srcField = static_cast<const char *>(srcComponent) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        switch (type) {
        case FieldType::Bool:
            return ApplyField<bool>(dstField, srcField, defaultField);
        case FieldType::Int32:
            return ApplyField<int32_t>(dstField, srcField, defaultField);
        case FieldType::Uint32:
            return ApplyField<uint32_t>(dstField, srcField, defaultField);
        case FieldType::SizeT:
            return ApplyField<size_t>(dstField, srcField, defaultField);
        case FieldType::AngleT:
            return ApplyField<sp::angle_t>(dstField, srcField, defaultField);
        case FieldType::Float:
            return ApplyField<float>(dstField, srcField, defaultField);
        case FieldType::Double:
            return ApplyField<double>(dstField, srcField, defaultField);
        case FieldType::Vec2:
            return ApplyField<glm::vec2>(dstField, srcField, defaultField);
        case FieldType::Vec3:
            return ApplyField<glm::vec3>(dstField, srcField, defaultField);
        case FieldType::Vec4:
            return ApplyField<glm::vec4>(dstField, srcField, defaultField);
        case FieldType::String:
            return ApplyField<std::string>(dstField, srcField, defaultField);
        case FieldType::EntityRef:
            return ApplyField<EntityRef>(dstField, srcField, defaultField);
        default:
            Abortf("ComponentField::Apply unknown component field type: %u", type);
        }
    }
} // namespace ecs
