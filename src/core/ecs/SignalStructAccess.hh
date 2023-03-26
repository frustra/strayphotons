#pragma once

#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"
#include "ecs/StructMetadata.hh"

#include <optional>
#include <string_view>

namespace ecs {
    double ReadStructField(const void *basePtr, const StructField &field);
    bool WriteStructField(void *basePtr, const StructField &field, std::function<void(double &)> accessor);
    // New accessor function argument types can be appended here as needed

    /**
     * Determines the offset and type of a field in a struct so it can be accessed dynamically.
     *
     * Example:
     * ```
     * struct SubType {
     *     int value1;
     *     glm::vec3 value2;
     * };
     * struct Component {
     *     SubType field1;
     *     SubType field2;
     * } comp;
     *
     * auto field = ecs::GetStructField(typeid(Component), "component.field1.value2.y");
     * // field.name == "component.field1.value2.x"
     * // field.type == typeid(glm::vec3::value_type)
     * // field.offset == offsetof(Component, field1) + offsetof(SubType, value2) + sizeof(glm::vec3::value_type)
     * ```
     */
    std::optional<StructField> GetStructField(std::type_index baseType,
        std::string_view fieldName,
        size_t fieldNameOffset = 0);
} // namespace ecs
