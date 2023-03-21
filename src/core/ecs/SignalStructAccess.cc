#include "SignalStructAccess.hh"

#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"
#include "ecs/StructMetadata.hh"

#include <string_view>

namespace ecs {
    namespace detail {
        template<typename ArgT, typename BaseType, typename Fn>
        bool AccessStructField(std::type_index baseType, BaseType *basePtr, std::string_view fieldName, Fn &&accessor) {
            Assertf(basePtr != nullptr,
                "AccessStructField was provided nullptr: %s '%s'",
                baseType.name(),
                std::string(fieldName));

            return ecs::GetFieldType(baseType, basePtr, [&](auto &base) {
                using T = std::decay_t<decltype(base)>;

                auto delim = fieldName.find('.');
                if (delim == std::string_view::npos) {
                    if constexpr (std::is_same_v<T, std::decay_t<ArgT>>) {
                        accessor((ArgT &)base);
                        return true;
                    } else if constexpr (std::is_convertible_v<T, ArgT> && std::is_convertible_v<ArgT, T>) {
                        ArgT tmp = (ArgT)base;
                        accessor(tmp);
                        if constexpr (!std::is_const_v<ArgT>) {
                            if constexpr (std::is_same_v<T, bool> && !std::is_same_v<ArgT, bool>) {
                                base = (double)tmp > 0.5;
                            } else {
                                base = (T)tmp;
                            }
                        }
                        return true;
                    } else {
                        Errorf("AccessStructField field missing delimiter: %s '%s'",
                            baseType.name(),
                            std::string(fieldName));
                        return false;
                    }
                }

                auto subField = fieldName.substr(delim + 1);
                if (subField.empty()) {
                    Errorf("AccessStructField empty subfield: %s '%s'", baseType.name(), std::string(fieldName));
                    return false;
                }

                // Short-circuit the template generation for most types that have no subfields to speed up compile
                if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                    Errorf("AccessStructField invalid subfield: %s '%s'", baseType.name(), std::string(fieldName));
                    return false;
                } else if constexpr (std::is_enum_v<T> || sp::is_vector<T>()) {
                    Errorf("AccessStructField invalid subfield: %s '%s'", baseType.name(), std::string(fieldName));
                    return false;
                } else if constexpr (sp::is_glm_vec<T>::value || std::is_same_v<T, sp::color_t> ||
                                     std::is_same_v<T, sp::color_alpha_t>) {
                    if (subField.empty() || subField.size() > (size_t)T::length()) {
                        Errorf("AccessStructField invalid glm::vec subfield: %s '%s'",
                            baseType.name(),
                            std::string(fieldName));
                        return false;
                    }

                    static const std::array<std::string, 3> indexChars = {"xyzw", "rgba", "0123"};
                    for (auto &chars : indexChars) {
                        auto it = std::find(chars.begin(), chars.end(), subField[0]);
                        auto index = it - chars.begin();
                        if (index < 0 || index + subField.size() > (size_t)T::length()) continue;
                        if (!std::equal(it, it + subField.size(), subField.begin(), subField.end())) continue;

                        if constexpr (std::is_same_v<typename T::value_type, std::decay_t<ArgT>>) {
                            accessor((ArgT &)base[index]);
                            if constexpr (!std::is_const_v<ArgT>) {
                                for (size_t i = 1; i < subField.size(); i++) {
                                    base[index + i] = base[index];
                                }
                            } else if (subField.size() > 1) {
                                Errorf("AccessStructField invalid glm::vec subfield: %s '%s'",
                                    baseType.name(),
                                    std::string(fieldName));
                                return false;
                            }
                            return true;
                        } else {
                            ArgT tmp = (ArgT)base[index];
                            accessor(tmp);
                            if constexpr (!std::is_const_v<ArgT>) {
                                if constexpr (std::is_same_v<typename T::value_type, bool> &&
                                              !std::is_same_v<ArgT, bool>) {
                                    base[index] = (double)tmp > 0.5;
                                } else {
                                    base[index] = (typename T::value_type)tmp;
                                }
                                for (size_t i = 1; i < subField.size(); i++) {
                                    base[index + i] = base[index];
                                }
                            } else if (subField.size() > 1) {
                                Errorf("AccessStructField invalid glm::vec subfield: %s '%s'",
                                    baseType.name(),
                                    std::string(fieldName));
                                return false;
                            }
                            return true;
                        }
                    }
                    Errorf("AccessStructField invalid glm::vec subfield: %s '%s'",
                        baseType.name(),
                        std::string(fieldName));
                    return false;
                } else {
                    auto *metadata = StructMetadata::Get(baseType);
                    if (metadata) {
                        for (const StructField &field : metadata->fields) {
                            if (!sp::starts_with(subField, field.name)) continue;
                            if (subField.length() > field.name.length() && subField[field.name.length()] != '.') {
                                continue;
                            }

                            return AccessStructField<ArgT>(field.type,
                                field.Access(basePtr),
                                subField,
                                std::forward<Fn>(accessor));
                        }
                        Errorf("AccessStructField missing subfield: %s '%s'", baseType.name(), std::string(fieldName));
                        return false;
                    } else {
                        Errorf("AccessStructField unsupported type subfield: %s '%s'",
                            baseType.name(),
                            std::string(fieldName));
                        return false;
                    }
                }
            });
        }
    } // namespace detail

    bool AccessStructField(std::type_index baseType,
        void *basePtr,
        std::string_view fieldName,
        std::function<void(double &)> accessor) {
        return detail::AccessStructField<double>(baseType, basePtr, fieldName, accessor);
    }

    bool AccessStructField(std::type_index baseType,
        const void *basePtr,
        std::string_view fieldName,
        std::function<void(const double &)> accessor) {
        return detail::AccessStructField<const double>(baseType, basePtr, fieldName, accessor);
    }
} // namespace ecs
