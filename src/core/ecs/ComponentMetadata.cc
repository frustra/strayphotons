#include "ComponentMetadata.hh"

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"

#include <cstring>

namespace ecs {
    template<typename, typename>
    struct has_type;
    template<typename T, typename... Un>
    struct has_type<T, std::tuple<Un...>> : std::disjunction<std::is_same<T, Un>...> {};

    const auto &getUndefinedFieldValues() {
        static const auto undefinedValues = std::make_tuple(sp::angle_t(-INFINITY),
            -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<double>::infinity(),
            glm::vec2(-INFINITY),
            glm::vec3(-INFINITY),
            glm::vec4(-INFINITY),
            sp::color_t(glm::vec3(-INFINITY)),
            sp::color_alpha_t(glm::vec4(-INFINITY)),
            glm::quat(-INFINITY, -INFINITY, -INFINITY, -INFINITY));
        return undefinedValues;
    };

    using UndefinedValuesTuple = std::remove_cvref_t<std::invoke_result_t<decltype(&getUndefinedFieldValues)>>;

    template<typename T>
    inline static constexpr bool isFieldUndefined(const T &value) {
        if constexpr (has_type<T, UndefinedValuesTuple>()) {
            return value == std::get<T>(getUndefinedFieldValues());
        } else {
            return false;
        }
    }

    void ComponentField::InitUndefined(void *component, const void *defaultComponent) const {
        auto *field = static_cast<char *>(component) + offset;
        auto *defaultField = static_cast<const char *>(defaultComponent) + offset;

        GetFieldType(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            auto &value = *reinterpret_cast<T *>(field);
            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);

            if constexpr (has_type<T, UndefinedValuesTuple>()) {
                value = std::get<T>(getUndefinedFieldValues());
            } else {
                value = defaultValue;
            }
        });
    }

    bool ComponentField::Load(const EntityScope &scope, void *component, const picojson::value &src) const {
        if (!(actions & FieldAction::AutoLoad)) return true;

        auto *dstfield = static_cast<char *>(component) + offset;
        auto *srcField = &src;

        if (name != nullptr) {
            if (!src.is<picojson::object>()) {
                Errorf("ComponentField::Load '%s' invalid component object: %s", name, src.to_str());
                return false;
            }
            auto &obj = src.get<picojson::object>();
            auto it = obj.find(name);
            if (it == obj.end()) {
                // Silently leave missing fields as default
                return true;
            }
            srcField = &it->second;
        }

        return GetFieldType(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            auto &field = *reinterpret_cast<T *>(dstfield);
            if (!sp::json::Load(scope, field, *srcField)) {
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

        if (name != nullptr) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            auto &obj = dst.get<picojson::object>();

            GetFieldType(type, [&](auto *typePtr) {
                using T = std::remove_pointer_t<decltype(typePtr)>;

                auto &value = *reinterpret_cast<const T *>(field);
                auto &defaultValue = *reinterpret_cast<const T *>(defaultField);
                sp::json::SaveIfChanged(scope, obj, name, value, defaultValue);
            });
        } else {
            GetFieldType(type, [&](auto *typePtr) {
                using T = std::remove_pointer_t<decltype(typePtr)>;

                auto &value = *reinterpret_cast<const T *>(field);
                sp::json::Save(scope, dst, value);
            });
        }
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

            if (dstValue == defaultValue && !isFieldUndefined(srcValue)) dstValue = srcValue;
        });
    }
} // namespace ecs
