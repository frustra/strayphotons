#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <type_traits>

namespace picojson {
    class value;
}

namespace ecs {
    enum class FieldType {
        Bool = 0,
        Int,
        Uint,
        Float,
        Vec2,
        Vec3,
        String,
        Count,
    };

    struct ComponentField {
        const char *name = nullptr;
        FieldType type = FieldType::Count;
        size_t offset = 0;

        ComponentField(const char *name, FieldType type, size_t offset) : name(name), type(type), offset(offset) {}

        template<typename T, typename F>
        static constexpr ComponentField New(const char *name, const F T::*M) {
            using BaseType = std::remove_cv_t<F>;

            size_t offset = reinterpret_cast<size_t>(&(((T *)0)->*M));

            if constexpr (std::is_same<BaseType, bool>::value) {
                return ComponentField(name, FieldType::Bool, offset);
            } else if constexpr (std::is_same<BaseType, int>::value) {
                return ComponentField(name, FieldType::Int, offset);
            } else if constexpr (std::is_same<BaseType, unsigned int>::value) {
                return ComponentField(name, FieldType::Uint, offset);
            } else if constexpr (std::is_same<BaseType, float>::value) {
                return ComponentField(name, FieldType::Float, offset);
            } else if constexpr (std::is_same<BaseType, glm::vec2>::value) {
                return ComponentField(name, FieldType::Vec2, offset);
            } else if constexpr (std::is_same<BaseType, glm::vec3>::value) {
                return ComponentField(name, FieldType::Vec3, offset);
            } else if constexpr (std::is_same<BaseType, std::string>::value) {
                return ComponentField(name, FieldType::String, offset);
            } else {
                Abortf("Component field %s type must be custom: %s", name, typeid(BaseType).name());
            }
        }

        bool SaveIfChanged(picojson::value &dst, const void *component, const void *defaultComponent) const;
    };

}; // namespace ecs
