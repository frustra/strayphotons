#pragma once

#include "assets/Async.hh"
#include "core/Common.hh"
#include "core/EnumTypes.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>
#include <robin_hood.h>
#include <type_traits>
#include <typeindex>

namespace picojson {
    class value;
}

namespace sp {
    class Gltf;
}

namespace ecs {
    enum class FieldAction;
    struct EntityScope;
    struct AnimationState;
    struct EventBinding;
    enum class FocusLayer : uint8_t;
    enum class GuiTarget;
    enum class InterpolationMode;
    enum class OpticType;
    enum class PhysicsGroup : uint16_t;
    enum class TriggerShape : uint8_t;
    enum class VisibilityMask;
    enum class XrEye;
}; // namespace ecs

template<>
struct magic_enum::customize::enum_range<ecs::FieldAction> {
    static constexpr bool is_flags = true;
};

namespace ecs {
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
        sp::color_alpha_t,
        glm::ivec2,
        glm::ivec3,
        glm::quat,

        // Structs
        std::string,
        EntityRef,
        Transform,
        std::vector<AnimationState>,
        std::optional<double>,
        robin_hood::unordered_map<std::string, std::vector<EventBinding>>,

        // Enums
        FocusLayer,
        GuiTarget,
        InterpolationMode,
        OpticType,
        PhysicsGroup,
        TriggerGroup,
        TriggerShape,
        VisibilityMask,
        XrEye>;

    template<typename Func, size_t I = 0>
    inline static const auto GetFieldType(std::type_index type, Func func) {
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

    struct StructField {
        const char *name = nullptr;
        std::type_index type;
        size_t offset = 0;
        int fieldIndex = -1;
        FieldAction actions = ~FieldAction::None;

        StructField(const char *name, std::type_index type, size_t offset, FieldAction actions)
            : name(name), type(type), offset(offset), actions(actions) {}

        /**
         * Registers a struct's field for serialization as a named field. For example:
         * StructField::New("model", &Renderable::modelName)
         *
         * Result:
         * {
         *   "renderable": {
         *     "model": "box"
         *   }
         * }
         */
        template<typename T, typename F>
        static const StructField New(const char *name, const F T::*M, FieldAction actions = ~FieldAction::None) {
            size_t offset = reinterpret_cast<size_t>(&(((T *)0)->*M));
            return StructField(name, std::type_index(typeid(std::remove_cv_t<F>)), offset, actions);
        }

        /**
         * Registers a struct's field for serialization directly. For example:
         * StructField::New(&TransformTree::pose)
         *
         * Result:
         * {
         *   "transform": {
         *     "translate": [1, 2, 3]
         *   }
         * }
         */
        template<typename T, typename F>
        static const StructField New(const F T::*M, FieldAction actions = ~FieldAction::None) {
            return StructField::New(nullptr, M, actions);
        }

        /**
         * Registers a type for serialization directly. For example:
         * StructField::New<TriggerGroup>()
         *
         * Result:
         * {
         *   "trigger_group": "Player"
         * }
         *
         * This field variant may also be used to define custom serialization functions for a type.
         */
        template<typename T>
        static const StructField New(FieldAction actions = ~FieldAction::None) {
            return StructField(nullptr, std::type_index(typeid(std::remove_cv_t<T>)), 0, actions);
        }

        template<typename T>
        T *Access(void *structPtr) const {
            auto *field = static_cast<char *>(structPtr) + offset;
            return reinterpret_cast<T *>(field);
        }

        template<typename T>
        const T *Access(const void *structPtr) const {
            auto *field = static_cast<const char *>(structPtr) + offset;
            return reinterpret_cast<const T *>(field);
        }

        void InitUndefined(void *dstStruct, const void *defaultStruct) const;
        bool Load(const EntityScope &scope, void *dstStruct, const picojson::value &src) const;
        void Save(const EntityScope &scope,
            picojson::value &dst,
            const void *srcStruct,
            const void *defaultStruct) const;
        void Apply(void *dstStruct, const void *srcStruct, const void *defaultPtr) const;
    };

    class StructMetadata {
    public:
        template<typename... Fields>
        StructMetadata(const std::type_index &idx, Fields &&...fields) : type(idx) {
            (this->fields.emplace_back(fields), ...);
            for (int i = 0; i < this->fields.size(); i++) {
                this->fields[i].fieldIndex = i;
            }
            Register(type, this);
        }

        static const StructMetadata *Get(const std::type_index &idx);

        template<typename T>
        static const StructMetadata &Get() {
            auto ptr = Get(std::type_index(typeid(T)));
            Assertf(ptr != nullptr, "Couldn't lookup metadata for type: %s", typeid(T).name());
            return *dynamic_cast<const StructMetadata *>(ptr);
        }

        const std::type_index type;
        std::vector<StructField> fields;

        template<typename T>
        static void InitUndefined(T &dst) {
            // Custom field init is always called, default to no-op.
        }

        template<typename T>
        static bool Load(const EntityScope &scope, T &dst, const picojson::value &src) {
            // Custom field serialization is always called, default to no-op.
            return true;
        }

        template<typename T>
        static void Save(const EntityScope &scope, picojson::value &dst, const T &src) {
            // Custom field serialization is always called, default to no-op.
        }

    private:
        static void Register(const std::type_index &idx, const StructMetadata *comp);
    };
}; // namespace ecs
