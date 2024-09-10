/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "SignalStructAccess.hh"
#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"
#include "ecs/StructMetadata.hh"

#include <string_view>

namespace ecs::detail {
    using namespace ecs;

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

            size_t offset = sizeof(typename T::value_type) * index;
            std::type_index type = typeid(void);
            switch (subField.size()) {
            case 1:
                type = typeid(typename T::value_type);
                break;
            case 2:
                type = typeid(glm::vec<2, typename T::value_type>);
                break;
            case 3:
                type = typeid(glm::vec<3, typename T::value_type>);
                break;
            case 4:
                type = typeid(glm::vec<4, typename T::value_type>);
                break;
            default:
                Abortf("GetVectorSubfield unexpected subfield size: %s '%s'", typeid(T).name(), std::string(subField));
            };
            return StructField(std::string(subField), "", type, offset, FieldAction::None);
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
            if (!ConvertAccessor<ArgT, typename T::value_type>(value[0], accessor)) return false;
            if constexpr (!std::is_const_v<ArgT>) {
                for (glm::length_t i = 1; i < value.length(); i++) {
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
        Assertf(basePtr != nullptr, "AccessStructField was provided nullptr: %s '%s'", field.type.name(), field.name);

        return ecs::GetFieldType(field.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            if constexpr (sp::is_glm_vec<T>::value || std::is_same_v<T, sp::color_t> ||
                          std::is_same_v<T, sp::color_alpha_t>) {
                std::string_view fieldName = field.name;
                size_t delimiter = fieldName.find_last_of('.');
                std::optional<StructField> subField = detail::GetVectorSubfield<T>(fieldName.substr(delimiter + 1));
                if (subField) {
                    subField->offset += field.offset;
                    if (subField->type == typeid(typename T::value_type)) {
                        auto &subValue = subField->Access<typename T::value_type>(basePtr);
                        if (ConvertAccessor<ArgT>(subValue, accessor)) return true;
                    } else if (subField->type == typeid(glm::vec<2, typename T::value_type>)) {
                        auto &subValue = subField->Access<glm::vec<2, typename T::value_type>>(basePtr);
                        if (ConvertAccessor<ArgT>(subValue, accessor)) return true;
                    } else if (subField->type == typeid(glm::vec<3, typename T::value_type>)) {
                        auto &subValue = subField->Access<glm::vec<3, typename T::value_type>>(basePtr);
                        if (ConvertAccessor<ArgT>(subValue, accessor)) return true;
                    } else if (subField->type == typeid(glm::vec<4, typename T::value_type>)) {
                        auto &subValue = subField->Access<glm::vec<4, typename T::value_type>>(basePtr);
                        if (ConvertAccessor<ArgT>(subValue, accessor)) return true;
                    }
                }
                Errorf("AccessStructField unable to vector convert from: %s to %s '%s'",
                    field.type.name(),
                    typeid(ArgT).name(),
                    field.name);
                return false;
            } else if constexpr (std::is_same_v<T, EventData>) {
                auto &value = field.Access<EventData>(basePtr);
                return std::visit(
                    [&](auto &&event) {
                        using EventT = std::decay_t<decltype(event)>;
                        auto subField = field;
                        if (field.type == typeid(T)) subField.type = typeid(EventT);
                        return AccessStructField<ArgT>(&event, subField, accessor);
                    },
                    value);
            } else {
                auto &value = field.Access<T>(basePtr);
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
} // namespace ecs::detail
