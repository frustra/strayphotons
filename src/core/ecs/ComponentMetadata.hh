#pragma once

#include "assets/Async.hh"
#include "core/Common.hh"
#include "core/EnumTypes.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>
#include <type_traits>
#include <typeindex>

namespace picojson {
    class value;
}

namespace sp {
    class Gltf;
}

namespace ecs {
    struct EntityScope;
    struct AnimationState;
    enum class FocusLayer : uint8_t;
    enum class GuiTarget;
    enum class InterpolationMode;
    enum class OpticType;
    enum class PhysicsGroup : uint16_t;
    enum class TriggerShape : uint8_t;
    enum class VisibilityMask;

    using FieldTypes = std::tuple<
        // Basic types
        bool,
        int32_t,
        uint32_t,
        size_t,
        sp::angle_t,
        float,
        double,

        // Vector types
        glm::vec2,
        glm::vec3,
        glm::vec4,
        sp::color_t,
        glm::ivec2,
        glm::ivec3,
        glm::quat,

        // Structs
        std::string,
        EntityRef,
        Transform,
        std::vector<AnimationState>,

        // Enums
        FocusLayer,
        GuiTarget,
        InterpolationMode,
        OpticType,
        PhysicsGroup,
        TriggerGroup,
        TriggerShape,
        VisibilityMask>;

    template<typename Func, size_t I = 0>
    inline static constexpr auto GetFieldType(std::type_index type, Func func) {
        if (type == std::type_index(typeid(std::tuple_element_t<I, FieldTypes>))) {
            return std::invoke(func, (std::tuple_element_t<I, FieldTypes> *)nullptr);
        }
        if constexpr (I + 1 < std::tuple_size_v<FieldTypes>) {
            return GetFieldType<Func, I + 1>(type, func);
        } else {
            Abortf("Type missing from FieldTypes definition: %s", type.name());
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
        std::type_index type;
        size_t offset = 0;
        int fieldIndex = -1;
        FieldAction actions = ~FieldAction::None;

        ComponentField(const char *name, std::type_index type, size_t offset, FieldAction actions)
            : name(name), type(type), offset(offset), actions(actions) {}

        /**
         * Registers a component's field for serialization as a named field. For example:
         * ComponentField::New("model", &Renderable::modelName)
         *
         * Result:
         * {
         *   "component": {
         *     "model": "box"
         *   }
         * }
         */
        template<typename T, typename F>
        static constexpr ComponentField New(const char *name, const F T::*M, FieldAction actions = ~FieldAction::None) {
            size_t offset = reinterpret_cast<size_t>(&(((T *)0)->*M));
            return ComponentField(name, std::type_index(typeid(std::remove_cv_t<F>)), offset, actions);
        }

        /**
         * Registers a component's field for serialization directly. For example:
         * ComponentField::New(&TransformTree::pose)
         *
         * Result:
         * {
         *   "component": {
         *     "translate": [1, 2, 3]
         *   }
         * }
         */
        template<typename T, typename F>
        static constexpr ComponentField New(const F T::*M, FieldAction actions = ~FieldAction::None) {
            return ComponentField::New(nullptr, M, actions);
        }

        /**
         * Registers a component type for serialization directly. For example:
         * ComponentField::New<TriggerGroup>()
         *
         * Result:
         * {
         *   "component": "Player"
         * }
         */
        template<typename T>
        static constexpr ComponentField New(FieldAction actions = ~FieldAction::None) {
            return ComponentField(nullptr, std::type_index(typeid(std::remove_cv_t<T>)), 0, actions);
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
