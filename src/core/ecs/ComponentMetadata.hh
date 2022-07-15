#pragma once

#include "assets/Async.hh"
#include "core/Common.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>
#include <type_traits>

namespace picojson {
    class value;
}

namespace sp {
    class Gltf;
}

namespace ecs {
    struct EntityScope;
    enum class VisibilityMask;

    enum class FieldType {
        Bool = 0,
        Int32,
        Uint32,
        SizeT,
        AngleT,
        Float,
        Double,
        Vec2,
        Vec3,
        Vec4,
        IVec2,
        IVec3,
        String,
        EntityRef,
        GltfPtr,
        VisibilityMask,
        Count,
    };

    struct ComponentField {
        const char *name = nullptr;
        FieldType type = FieldType::Count;
        size_t offset = 0;
        size_t size = 0;

        ComponentField(const char *name, FieldType type, size_t offset, size_t size)
            : name(name), type(type), offset(offset), size(size) {}

        template<typename T, typename F>
        static constexpr ComponentField New(const char *name, const F T::*M) {
            using BaseType = std::remove_cv_t<F>;

            size_t offset = reinterpret_cast<size_t>(&(((T *)0)->*M));

            if constexpr (std::is_same<BaseType, bool>()) {
                return ComponentField(name, FieldType::Bool, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, int32_t>()) {
                return ComponentField(name, FieldType::Int32, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, uint32_t>()) {
                return ComponentField(name, FieldType::Uint32, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, size_t>()) {
                return ComponentField(name, FieldType::SizeT, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, sp::angle_t>()) {
                return ComponentField(name, FieldType::AngleT, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, float>()) {
                return ComponentField(name, FieldType::Float, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, glm::vec2>()) {
                return ComponentField(name, FieldType::Vec2, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, glm::vec3>()) {
                return ComponentField(name, FieldType::Vec3, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, glm::vec4>()) {
                return ComponentField(name, FieldType::Vec4, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, glm::ivec2>()) {
                return ComponentField(name, FieldType::IVec2, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, glm::ivec3>()) {
                return ComponentField(name, FieldType::IVec3, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, std::string>()) {
                return ComponentField(name, FieldType::String, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, EntityRef>()) {
                return ComponentField(name, FieldType::EntityRef, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, sp::AsyncPtr<sp::Gltf>>()) {
                return ComponentField(name, FieldType::GltfPtr, offset, sizeof(F));
            } else if constexpr (std::is_same<BaseType, VisibilityMask>()) {
                return ComponentField(name, FieldType::VisibilityMask, offset, sizeof(F));
            } else {
                Abortf("Component field %s type must be custom: %s", name, typeid(BaseType).name());
            }
        }

        bool Load(const EntityScope &scope, void *component, const picojson::value &src) const;
        bool SaveIfChanged(const EntityScope &scope,
            picojson::value &dst,
            const void *component,
            const void *defaultComponent) const;
        void Apply(void *dstComponent, const void *srcComponent, const void *defaultComponent) const;
    };

}; // namespace ecs
