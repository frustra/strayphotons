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

    using FieldTypes = std::tuple<bool,
        int32_t,
        uint32_t,
        size_t,
        sp::angle_t,
        float,
        double,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        glm::ivec2,
        glm::ivec3,
        glm::quat,
        std::string,
        EntityRef,
        Transform,
        std::vector<AnimationState>,
        InterpolationMode,
        VisibilityMask>;

    static_assert(std::tuple_size_v<FieldTypes> == (size_t)FieldType::Count, "ComponentMetatdata field types mismatch");

    template<typename T, size_t I = 0>
    inline static constexpr FieldType GetFieldType() {
        if constexpr (I >= std::tuple_size_v<FieldTypes>) {
            Abortf("Field type is not defined: %s", typeid(T).name());
        } else if constexpr (std::is_same_v<T, std::tuple_element_t<I, FieldTypes>>) {
            return (FieldType)I;
        } else {
            return GetFieldType<T, I + 1>();
        }
    }

    template<typename Func, size_t I = 0>
    inline static constexpr auto GetFieldType(FieldType type, Func func) {
        if ((size_t)type == I) {
            return std::invoke(func, (std::tuple_element_t<I, FieldTypes> *)nullptr);
        }
        if constexpr (I + 1 < std::tuple_size_v<FieldTypes>) {
            return GetFieldType<Func, I + 1>(type, func);
        } else {
            Abortf("Unknown field type: %s", type);
        }
    }

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
            auto fieldType = GetFieldType<std::remove_cv_t<F>>();

            size_t offset = reinterpret_cast<size_t>(&(((T *)0)->*M));
            return ComponentField(name, fieldType, offset, actions);
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
