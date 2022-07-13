#include "ComponentMetadata.hh"

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"

#include <cstring>

namespace ecs {
    struct FieldCastBase {
        virtual bool LoadField(const EntityScope &scope, void *dst, const picojson::value &src) const {
            Abortf("Uninitialized FieldCastBase::LoadField!");
        }
        virtual bool SaveField(const EntityScope &scope,
            picojson::object &dst,
            const char *fieldName,
            const void *field,
            const void *defaultField) const {
            Abortf("Uninitialized FieldCastBase::SaveField!");
        }
        virtual void ApplyField(void *dstField, const void *srcField, const void *defaultField) const {
            Abortf("Uninitialized FieldCastBase::ApplyField!");
        }
    };

    template<typename T>
    struct FieldCast : public FieldCastBase {
        bool LoadField(const EntityScope &scope, void *dst, const picojson::value &src) const override {
            auto &field = *reinterpret_cast<T *>(dst);
            if (!sp::json::Load(scope, field, src)) {
                Errorf("Invalid %s field value: %s", typeid(T).name(), src.to_str());
                return false;
            }
            return true;
        }

        bool SaveField(const EntityScope &scope,
            picojson::object &dst,
            const char *fieldName,
            const void *field,
            const void *defaultField) const override {
            auto &value = *reinterpret_cast<const T *>(field);
            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);
            return sp::json::SaveIfChanged(scope, dst, fieldName, value, defaultValue);
        }

        void ApplyField(void *dstField, const void *srcField, const void *defaultField) const override {
            auto &dstValue = *reinterpret_cast<T *>(dstField);
            auto &srcValue = *reinterpret_cast<const T *>(srcField);
            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);

            if (dstValue == defaultValue) dstValue = srcValue;
        }
    };

    static_assert(sizeof(FieldCast<bool>) == sizeof(FieldCastBase), "Expected FieldCast<T> to fit in Base class");

    const FieldCastBase *CastField(FieldCastBase &cast, ecs::FieldType type) {
        switch (type) {
        case FieldType::Bool:
            return new (&cast) FieldCast<bool>();
        case FieldType::Int32:
            return new (&cast) FieldCast<int32_t>();
        case FieldType::Uint32:
            return new (&cast) FieldCast<uint32_t>();
        case FieldType::SizeT:
            return new (&cast) FieldCast<size_t>();
        case FieldType::AngleT:
            return new (&cast) FieldCast<sp::angle_t>();
        case FieldType::Float:
            return new (&cast) FieldCast<float>();
        case FieldType::Double:
            return new (&cast) FieldCast<double>();
        case FieldType::Vec2:
            return new (&cast) FieldCast<glm::vec2>();
        case FieldType::Vec3:
            return new (&cast) FieldCast<glm::vec3>();
        case FieldType::Vec4:
            return new (&cast) FieldCast<glm::vec4>();
        case FieldType::IVec2:
            return new (&cast) FieldCast<glm::ivec2>();
        case FieldType::IVec3:
            return new (&cast) FieldCast<glm::ivec3>();
        case FieldType::String:
            return new (&cast) FieldCast<std::string>();
        case FieldType::EntityRef:
            return new (&cast) FieldCast<EntityRef>();
        default:
            Abortf("CastField unknown component field type: %u", type);
        }
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

        FieldCastBase cast;
        return CastField(cast, type)->LoadField(scope, fieldDst, fieldSrc);
    }

    bool ComponentField::SaveIfChanged(const EntityScope &scope,
        picojson::value &dst,
        const void *component,
        const void *defaultComponent) const {
        auto *field = static_cast<const char *>(component) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        FieldCastBase cast;
        return CastField(cast, type)->SaveField(scope, obj, name, field, defaultField);
    }

    void ComponentField::Apply(void *dstComponent, const void *srcComponent, const void *defaultComponent) const {
        auto *dstField = static_cast<char *>(dstComponent) + offset;
        auto *srcField = static_cast<const char *>(srcComponent) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        FieldCastBase cast;
        CastField(cast, type)->ApplyField(dstField, srcField, defaultField);
    }
} // namespace ecs
