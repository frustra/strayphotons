#include "StructMetadata.hh"

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "ecs/StructFieldTypes.hh"

#include <cstring>

namespace ecs {
    typedef std::map<std::type_index, const StructMetadata *> MetadataTypeMap;
    MetadataTypeMap *metadataTypeMap = nullptr;

    const StructMetadata *StructMetadata::Get(const std::type_index &idx) {
        if (metadataTypeMap == nullptr) metadataTypeMap = new MetadataTypeMap();

        auto it = metadataTypeMap->find(idx);
        if (it != metadataTypeMap->end()) return it->second;
        return nullptr;
    }

    void StructMetadata::Register(const std::type_index &idx, const StructMetadata *comp) {
        if (metadataTypeMap == nullptr) metadataTypeMap = new MetadataTypeMap();
        metadataTypeMap->emplace(idx, comp);
    }

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

    void StructField::InitUndefined(void *dstStruct, const void *defaultStruct) const {
        auto *field = static_cast<char *>(dstStruct) + offset;
        auto *defaultField = static_cast<const char *>(defaultStruct) + offset;

        GetFieldType(type, field, [&](auto &value) {
            using T = std::decay_t<decltype(value)>;

            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);

            if constexpr (has_type<T, UndefinedValuesTuple>()) {
                value = std::get<T>(getUndefinedFieldValues());
            } else {
                value = defaultValue;
            }
        });
    }

    bool StructField::Compare(const void *a, const void *b) const {
        auto *fieldA = static_cast<const char *>(a) + offset;
        auto *fieldB = static_cast<const char *>(b) + offset;

        return GetFieldType(type, fieldA, [&](auto &valueA) {
            using T = std::decay_t<decltype(valueA)>;
            auto &valueB = *reinterpret_cast<const T *>(fieldB);

            if constexpr (std::equality_comparable<T>) {
                return valueA == valueB;
            } else {
                Abortf("StructField::Compare called on unsupported type: %s", type.name());
                return false;
            }
        });
    }

    bool StructField::Load(const EntityScope &scope, void *dstStruct, const picojson::value &src) const {
        if (!(actions & FieldAction::AutoLoad)) return true;

        auto *dstfield = static_cast<char *>(dstStruct) + offset;
        auto *srcField = &src;

        if (!name.empty()) {
            if (!src.is<picojson::object>()) {
                // Silently leave missing fields as default
                return true;
            }
            auto &obj = src.get<picojson::object>();
            auto it = obj.find(name);
            if (it == obj.end()) {
                // Silently leave missing fields as default
                return true;
            }
            srcField = &it->second;
        }

        return GetFieldType(type, dstfield, [&](auto &dstValue) {
            if (!sp::json::Load(scope, dstValue, *srcField)) {
                Errorf("Invalid %s field value: %s", type.name(), src.to_str());
                return false;
            }
            return true;
        });
    }

    void StructField::Save(const EntityScope &scope,
        picojson::value &dst,
        const void *srcStruct,
        const void *defaultStruct) const {
        if (!(actions & FieldAction::AutoSave)) return;

        auto *field = static_cast<const char *>(srcStruct) + offset;
        auto *defaultField = defaultStruct ? static_cast<const char *>(defaultStruct) + offset : nullptr;

        if (!name.empty()) {
            GetFieldType(type, field, [&](auto &value) {
                using T = std::decay_t<decltype(value)>;

                if (defaultField) {
                    auto &defaultValue = *reinterpret_cast<const T *>(defaultField);
                    sp::json::SaveIfChanged(scope, dst, name, value, defaultValue);
                } else {
                    if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
                    auto &obj = dst.get<picojson::object>();
                    sp::json::Save(scope, obj[name], value);
                }
            });
        } else {
            GetFieldType(type, field, [&](auto &value) {
                sp::json::Save(scope, dst, value);
            });
        }
    }

    void StructField::Apply(void *dstStruct, const void *srcStruct, const void *defaultStruct) const {
        if (!(actions & FieldAction::AutoApply)) return;

        auto *dstField = static_cast<char *>(dstStruct) + offset;
        auto *srcField = static_cast<const char *>(srcStruct) + offset;
        auto *defaultField = static_cast<const char *>(defaultStruct) + offset;

        GetFieldType(type, dstField, [&](auto &dstValue) {
            using T = std::decay_t<decltype(dstValue)>;

            auto &srcValue = *reinterpret_cast<const T *>(srcField);
            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);

            if constexpr (std::equality_comparable<T>) {
                if (dstValue == defaultValue && !isFieldUndefined(srcValue)) dstValue = srcValue;
            } else {
                Abortf("StructField::Apply called on unsupported type: %s", type.name());
            }
        });
    }
} // namespace ecs
