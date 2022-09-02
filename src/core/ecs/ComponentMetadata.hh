#pragma once

#include "assets/Async.hh"
#include "core/Common.hh"
#include "core/EnumTypes.hh"
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
    struct AnimationState;
    enum class VisibilityMask;
    enum class InterpolationMode;

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
        Quat,
        String,

        // Custom Types
        EntityRef,
        Transform,
        AnimationStates,

        // Enums
        InterpolationMode,
        VisibilityMask,
        Count,
    };

    enum class FieldAction {
        None = 0,
        AutoLoad = 1 << 0,
        AutoSave = 1 << 1,
        AutoApply = 1 << 2,
    };

    struct ComponentField {
        const char *name = nullptr;
        FieldType type = FieldType::Count;
        size_t offset = 0;
        FieldAction actions = ~FieldAction::None;

        ComponentField(const char *name, FieldType type, size_t offset, FieldAction actions)
            : name(name), type(type), offset(offset), actions(actions) {}

        template<typename T, typename F>
        static constexpr ComponentField New(const char *name, const F T::*M, FieldAction actions = ~FieldAction::None) {
            using BaseType = std::remove_cv_t<F>;

            size_t offset = reinterpret_cast<size_t>(&(((T *)0)->*M));

            if constexpr (std::is_same<BaseType, bool>()) {
                return ComponentField(name, FieldType::Bool, offset, actions);
            } else if constexpr (std::is_same<BaseType, int32_t>()) {
                return ComponentField(name, FieldType::Int32, offset, actions);
            } else if constexpr (std::is_same<BaseType, uint32_t>()) {
                return ComponentField(name, FieldType::Uint32, offset, actions);
            } else if constexpr (std::is_same<BaseType, size_t>()) {
                return ComponentField(name, FieldType::SizeT, offset, actions);
            } else if constexpr (std::is_same<BaseType, sp::angle_t>()) {
                return ComponentField(name, FieldType::AngleT, offset, actions);
            } else if constexpr (std::is_same<BaseType, float>()) {
                return ComponentField(name, FieldType::Float, offset, actions);
            } else if constexpr (std::is_same<BaseType, glm::vec2>()) {
                return ComponentField(name, FieldType::Vec2, offset, actions);
            } else if constexpr (std::is_same<BaseType, glm::vec3>()) {
                return ComponentField(name, FieldType::Vec3, offset, actions);
            } else if constexpr (std::is_same<BaseType, glm::vec4>()) {
                return ComponentField(name, FieldType::Vec4, offset, actions);
            } else if constexpr (std::is_same<BaseType, glm::ivec2>()) {
                return ComponentField(name, FieldType::IVec2, offset, actions);
            } else if constexpr (std::is_same<BaseType, glm::ivec3>()) {
                return ComponentField(name, FieldType::IVec3, offset, actions);
            } else if constexpr (std::is_same<BaseType, glm::quat>()) {
                return ComponentField(name, FieldType::Quat, offset, actions);
            } else if constexpr (std::is_same<BaseType, std::string>()) {
                return ComponentField(name, FieldType::String, offset, actions);
            } else if constexpr (std::is_same<BaseType, EntityRef>()) {
                return ComponentField(name, FieldType::EntityRef, offset, actions);
            } else if constexpr (std::is_same<BaseType, Transform>()) {
                return ComponentField(name, FieldType::Transform, offset, actions);
            } else if constexpr (std::is_same<BaseType, std::vector<AnimationState>>()) {
                return ComponentField(name, FieldType::AnimationStates, offset, actions);
            } else if constexpr (std::is_same<BaseType, InterpolationMode>()) {
                return ComponentField(name, FieldType::InterpolationMode, offset, actions);
            } else if constexpr (std::is_same<BaseType, VisibilityMask>()) {
                return ComponentField(name, FieldType::VisibilityMask, offset, actions);
            } else {
                Abortf("Component field %s type must be custom: %s", name, typeid(BaseType).name());
            }
        }

        template<typename T, typename F>
        static constexpr ComponentField New(const F T::*M, FieldAction actions = ~FieldAction::None) {
            return ComponentField::New(nullptr, M, actions);
        }

        template<typename T>
        T *Access(void *component) const {
            auto *field = static_cast<char *>(component) + offset;
            return reinterpret_cast<T *>(field);
        }

        template<typename T>
        const T *Access(const void *component) const {
            auto *field = static_cast<const char *>(component) + offset;
            return reinterpret_cast<const T *>(field);
        }

        bool Load(const EntityScope &scope, void *component, const picojson::value &src) const;
        void Save(const EntityScope &scope,
            picojson::value &dst,
            const void *component,
            const void *defaultComponent) const;
        void Apply(void *dstComponent, const void *srcComponent, const void *defaultComponent) const;
    };

}; // namespace ecs
