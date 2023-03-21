#pragma once

#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"
#include "ecs/StructMetadata.hh"

#include <string_view>

namespace ecs {
    /**
     * Calls the provided accessor callback function with a reference to the requested field.
     *
     * Example:
     * ```
     * struct Component {
     *     SubType field;
     * } comp;
     * void *basePtr = &comp;
     * ecs::AccessStructField(typeid(Component), basePtr, "<ignored>.field", [](auto &value) {
     *     // if basePtr is a const void*, value is a (const SubType &)
     *     // if basePtr is a void*, value is a (SubType &)
     * }) == true;
     *
     * ecs::AccessStructField(metadata, basePtr, "component.missing", [](auto &value) {
     *     // Callback is never called, ecs::AccessStructField() returns false
     * }) == false;
     * ```
     */
    bool AccessStructField(std::type_index baseType,
        void *basePtr,
        std::string_view fieldName,
        std::function<void(double &)> accessor);
    bool AccessStructField(std::type_index baseType,
        const void *basePtr,
        std::string_view fieldName,
        std::function<void(const double &)> accessor);
    // New accessor function argument types can be appended here as needed
} // namespace ecs
