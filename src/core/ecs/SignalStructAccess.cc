#include "SignalStructAccess.hh"

#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"
#include "ecs/StructMetadata.hh"

#include <string_view>

namespace ecs {
    namespace detail {
        template<typename T>
        std::optional<StructField> GetVectorSubfield(std::string_view subField) {
            if (subField.empty() || subField.size() > (size_t)T::length()) {
                Errorf("GetVectorSubfield invalid subfield: %s '%s'", typeid(T).name(), std::string(subField));
                return {};
            }

            static const std::array<std::string, 3> indexChars = {"xyzw", "rgba", "0123"};
            for (auto &chars : indexChars) {
                auto it = std::find(chars.begin(), chars.end(), subField[0]);
                auto index = it - chars.begin();
                if (index < 0 || index + subField.size() > (size_t)T::length()) continue;
                if (!std::equal(it, it + subField.size(), subField.begin(), subField.end())) continue;

                size_t offset = sizeof(T::value_type) * index;
                return StructField(std::string(subField), typeid(T::value_type), offset, FieldAction::None);
            }
            Errorf("GetVectorSubfield invalid subfield: %s '%s'", typeid(T).name(), std::string(subField));
            return {};
        }

        template<typename ArgT, typename T, typename Fn>
        bool ConvertAccessor(T &value, Fn &&accessor) {
            if constexpr (std::is_same_v<T, ArgT>) {
                accessor((ArgT &)value);
                return true;
            } else if constexpr (std::is_convertible_v<T, ArgT> && std::is_convertible_v<ArgT, T>) {
                ArgT tmp = (ArgT)value;
                accessor(tmp);
                if constexpr (!std::is_const_v<ArgT>) {
                    if constexpr (std::is_same_v<T, bool> && !std::is_same_v<ArgT, bool>) {
                        value = (double)tmp > 0.5;
                    } else {
                        value = (T)tmp;
                    }
                }
                return true;
            } else if constexpr (sp::is_glm_vec<T>::value || std::is_same_v<T, sp::color_t> ||
                                 std::is_same_v<T, sp::color_alpha_t>) {
                if (!ConvertAccessor<ArgT>(value[0], accessor)) return false;
                if constexpr (!std::is_const_v<ArgT>) {
                    for (size_t i = 1; i < value.length(); i++) {
                        value[i] = value[0];
                    }
                }
                return true;
            } else {
                return false;
            }
        }

        template<typename ArgT, typename BaseType, typename Fn>
        bool AccessStructField(BaseType *basePtr, const StructField &field, Fn &&accessor) {
            Assertf(basePtr != nullptr,
                "AccessStructField was provided nullptr: %s '%s'",
                field.type.name(),
                field.name);

            return ecs::GetFieldType(field.type, field.Access(basePtr), [&](auto &value) {
                using T = std::decay_t<decltype(value)>;

                if constexpr (sp::is_glm_vec<T>::value || std::is_same_v<T, sp::color_t> ||
                              std::is_same_v<T, sp::color_alpha_t>) {
                    std::string_view fieldName = field.name;
                    size_t delimiter = fieldName.find_last_of('.');
                    auto subField = detail::GetVectorSubfield<T>(fieldName.substr(delimiter + 1));
                    if (subField) {
                        auto &subValue = subField->Access<typename T::value_type>(&value);
                        if (ConvertAccessor<ArgT>(subValue, accessor)) return true;
                    }
                    Errorf("AccessStructField unable to vector convert from: %s to %s '%s'",
                        field.type.name(),
                        typeid(ArgT).name(),
                        field.name);
                    return false;
                } else if constexpr (std::is_same_v<T, EventData>) {
                    return std::visit(
                        [&](auto &&event) {
                            using EventT = std::decay_t<decltype(event)>;
                            auto subField = field;
                            if (field.type == typeid(T)) subField.type = typeid(EventT);
                            return AccessStructField<ArgT>(&event, subField, accessor);
                        },
                        value);
                } else {
                    bool success = ConvertAccessor<ArgT>(value, accessor);
                    if (!success) {
                        Errorf("AccessStructField unable to convert from: %s to %s '%s'",
                            field.type.name(),
                            typeid(ArgT).name(),
                            field.name);
                    }
                    return success;
                }
            });
        }
    } // namespace detail

    double ReadStructField(const void *basePtr, const StructField &field) {
        double result = 0.0;
        detail::AccessStructField<const double>(basePtr, field, [&](const double &value) {
            result = value;
        });
        return result;
    }

    bool WriteStructField(void *basePtr, const StructField &field, std::function<void(double &)> accessor) {
        return detail::AccessStructField<double>(basePtr, field, accessor);
    }

    std::optional<StructField> GetStructField(std::type_index baseType,
        std::string_view fieldName,
        size_t fieldNameOffset) {
        return ecs::GetFieldType(baseType, [&](auto *typePtr) -> std::optional<StructField> {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            auto delimiter = fieldName.find('.', fieldNameOffset);
            if (delimiter == std::string_view::npos) {
                return StructField(std::string(fieldName), baseType, 0, FieldAction::None);
            }

            auto subField = fieldName.substr(delimiter + 1);
            if (subField.empty()) {
                Errorf("GetStructField empty subfield: %s '%s'", baseType.name(), std::string(fieldName));
                return {};
            }

            // Short-circuit the template generation for most types that have no subfields to speed up compile
            if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                Errorf("GetStructField invalid subfield: %s '%s'", baseType.name(), std::string(fieldName));
                return {};
            } else if constexpr (std::is_enum_v<T> || sp::is_vector<T>()) {
                Errorf("GetStructField invalid subfield: %s '%s'", baseType.name(), std::string(fieldName));
                return {};
            } else if constexpr (std::is_same_v<T, EventData>) {
                // EventData variants can't be processed without knowing the value
                return StructField(std::string(fieldName), baseType, 0, FieldAction::None);
            } else if constexpr (sp::is_glm_vec<T>::value || std::is_same_v<T, sp::color_t> ||
                                 std::is_same_v<T, sp::color_alpha_t>) {
                return detail::GetVectorSubfield<T>(subField);
            } else {
                auto *metadata = StructMetadata::Get(baseType);
                if (metadata) {
                    for (const StructField &field : metadata->fields) {
                        if (!sp::starts_with(subField, field.name)) continue;
                        if (subField.length() > field.name.length() && subField[field.name.length()] != '.') {
                            continue;
                        }

                        auto result = GetStructField(field.type, subField, delimiter + 1);
                        if (result) {
                            result->offset += field.offset;
                        }
                        return result;
                    }
                    Errorf("GetStructField missing subfield: %s '%s'", baseType.name(), std::string(fieldName));
                    return {};
                } else {
                    Errorf("GetStructField unsupported type subfield: %s '%s'",
                        baseType.name(),
                        std::string(fieldName));
                    return {};
                }
            }
        });
    }
} // namespace ecs
