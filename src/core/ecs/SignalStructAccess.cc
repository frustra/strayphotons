/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalStructAccess.hh"

#include "SignalStructAccess_common.hh"
#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"
#include "ecs/StructMetadata.hh"

#include <string_view>

namespace ecs {
    std::optional<StructField> GetStructField(std::type_index baseType,
        std::string_view fieldName,
        size_t fieldNameOffset) {
        return ecs::GetFieldType<std::optional<StructField>>(baseType,
            [&](auto *typePtr) -> std::optional<StructField> {
                using T = std::remove_pointer_t<decltype(typePtr)>;

                auto delimiter = fieldName.find('.', fieldNameOffset);
                if (delimiter == std::string_view::npos) {
                    return StructField(std::string(fieldName), "", baseType, 0, FieldAction::None);
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
                    return StructField(std::string(fieldName), "", baseType, 0, FieldAction::None);
                } else if constexpr (sp::is_glm_vec<T>::value || std::is_same_v<T, sp::color_t> ||
                                     std::is_same_v<T, sp::color_alpha_t>) {
                    return detail::GetVectorSubfield<T>(subField);
                } else {
                    auto *metadata = StructMetadata::Get(baseType);
                    if (metadata) {
                        for (const StructField &field : metadata->fields) {
                            if (field.name.empty()) {
                                auto result = GetStructField(field.type, fieldName, fieldNameOffset);
                                if (result) return result;
                            } else {
                                if (!sp::starts_with(subField, field.name)) continue;
                                if (subField.length() > field.name.length() && subField[field.name.length()] != '.') {
                                    continue;
                                }

                                auto result = GetStructField(field.type, fieldName, delimiter + 1);
                                if (result) {
                                    result->offset += field.offset;
                                }
                                return result;
                            }
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
