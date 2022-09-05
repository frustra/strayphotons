#include "ComponentMetadata.hh"

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"

#include <cstring>

namespace ecs {
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

        return GetFieldType(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            auto &field = *reinterpret_cast<T *>(fieldDst);
            if (!sp::json::Load(scope, field, fieldSrc)) {
                Errorf("Invalid %s field value: %s", typeid(T).name(), src.to_str());
                return false;
            }
            return true;
        });
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

        GetFieldType(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            auto &value = *reinterpret_cast<const T *>(field);
            if (name == nullptr) {
                sp::json::Save(scope, dst, value);
            } else {
                auto &defaultValue = *reinterpret_cast<const T *>(defaultField);
                sp::json::SaveIfChanged(scope, obj, name, value, defaultValue);
            }
        });
    }

    void ComponentField::Apply(void *dstComponent, const void *srcComponent, const void *defaultComponent) const {
        if (!(actions & FieldAction::AutoApply)) return;

        auto *dstField = static_cast<char *>(dstComponent) + offset;
        auto *srcField = static_cast<const char *>(srcComponent) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        GetFieldType(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            auto &dstValue = *reinterpret_cast<T *>(dstField);
            auto &srcValue = *reinterpret_cast<const T *>(srcField);
            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);

            if (dstValue == defaultValue) dstValue = srcValue;
        });
    }
} // namespace ecs
