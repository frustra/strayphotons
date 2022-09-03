#include "ComponentMetadata.hh"

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"

#include <cstring>

namespace ecs {
    struct FieldCastBase {
        virtual bool LoadField(const EntityScope &scope, void *dst, const picojson::value &src) const {
            Abortf("Uninitialized FieldCastBase::LoadField!");
        }
        virtual void SaveField(const EntityScope &scope,
            picojson::object &dst,
            const char *fieldName,
            const void *field,
            const void *defaultField) const {
            Abortf("Uninitialized FieldCastBase::SaveField!");
        }
        virtual void SaveBaseField(const EntityScope &scope, picojson::value &dst, const void *field) const {
            Abortf("Uninitialized FieldCastBase::SaveBaseField!");
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

        void SaveField(const EntityScope &scope,
            picojson::object &dst,
            const char *fieldName,
            const void *field,
            const void *defaultField) const override {
            auto &value = *reinterpret_cast<const T *>(field);
            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);
            sp::json::SaveIfChanged(scope, dst, fieldName, value, defaultValue);
        }

        void SaveBaseField(const EntityScope &scope, picojson::value &dst, const void *field) const override {
            auto &value = *reinterpret_cast<const T *>(field);
            sp::json::Save(scope, dst, value);
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
        FieldCastBase *base = nullptr;
        GetFieldType(type, [&](auto *typePtr) {
            base = new (&cast) FieldCast<std::remove_pointer_t<decltype(typePtr)>>();
        });
        Assertf(base != nullptr, "CastField unknown component field type: %u", type);
        return base;
    }

    bool ComponentField::Load(const EntityScope &scope, void *component, const picojson::value &src) const {
        if (!(actions & FieldAction::AutoLoad)) return true;

        if (!src.is<picojson::object>()) {
            Errorf("ComponentField::Load invalid component object: %s", src.to_str());
            return false;
        }
        auto &obj = src.get<picojson::object>();
        if (name != nullptr && !src.contains(name)) {
            // Silently leave missing fields as default
            return true;
        }

        auto *fieldDst = static_cast<char *>(component) + offset;
        auto &fieldSrc = name == nullptr ? src : obj.at(name);

        FieldCastBase base;
        return CastField(base, type)->LoadField(scope, fieldDst, fieldSrc);
    }

    void ComponentField::Save(const EntityScope &scope,
        picojson::value &dst,
        const void *component,
        const void *defaultComponent) const {
        if (!(actions & FieldAction::AutoSave)) return;

        auto *field = static_cast<const char *>(component) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        FieldCastBase base;
        auto cast = CastField(base, type);
        if (name == nullptr) {
            cast->SaveBaseField(scope, dst, field);
        } else {
            cast->SaveField(scope, obj, name, field, defaultField);
        }
    }

    void ComponentField::Apply(void *dstComponent, const void *srcComponent, const void *defaultComponent) const {
        if (!(actions & FieldAction::AutoApply)) return;

        auto *dstField = static_cast<char *>(dstComponent) + offset;
        auto *srcField = static_cast<const char *>(srcComponent) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        FieldCastBase base;
        CastField(base, type)->ApplyField(dstField, srcField, defaultField);
    }
} // namespace ecs
