/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "common/Common.hh"
#include "common/EnumTypes.hh"
#include "common/InlineVector.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Name.hh"

#include <map>
#include <robin_hood.h>
#include <set>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace picojson {
    class value;
} // namespace picojson

namespace ecs {
    enum class FieldAction;
    class StructMetadata;
}; // namespace ecs

namespace sp {
    class Gltf;

    namespace json {
        using SchemaTypeReferences = std::set<const ecs::StructMetadata *>;
    }
} // namespace sp

template<>
struct magic_enum::customize::enum_range<ecs::FieldAction> {
    static constexpr bool is_flags = true;
};

namespace ecs {
    struct ArgDesc {
        std::string name, desc;

        ArgDesc(const std::string_view &name, const std::string_view &desc) : name(name), desc(desc) {}

        bool operator==(const ArgDesc &other) const = default;
    };

    uint32_t GetFieldTypeIndex(const std::type_index &idx);
    std::type_index GetFieldTypeIndex(uint32_t typeIndex);

    struct TypeInfo {
        uint32_t typeIndex;
        bool isTrivial;
        bool isConst;
        bool isPointer;
        bool isReference;
        bool isTecsLock;
        bool isFunctionPointer;

        template<typename T>
        static inline TypeInfo Lookup() {
            using StrippedT = std::remove_pointer_t<std::decay_t<T>>;
            return TypeInfo{
                .typeIndex = GetFieldTypeIndex(typeid(std::conditional_t<std::is_function_v<StrippedT>, T, StrippedT>)),
                .isTrivial = std::is_fundamental<std::remove_cv_t<T>>() || std::is_pointer<T>() ||
                             std::is_reference<T>() || std::is_same<T, Entity>(),
                .isConst = (std::is_pointer<T>() || std::is_reference<T>()) &&
                           std::is_const<std::remove_reference_t<std::remove_pointer_t<T>>>(),
                .isPointer = std::is_pointer<T>() || std::is_reference<T>(),
                .isReference = std::is_reference<T>(),
                .isTecsLock = Tecs::is_lock<std::decay_t<T>>() || Tecs::is_dynamic_lock<std::decay_t<T>>(),
                .isFunctionPointer = std::is_pointer<T>() && std::is_function<std::remove_pointer_t<T>>(),
            };
        }

        inline operator std::type_index() const {
            return GetFieldTypeIndex(typeIndex);
        }

        inline const char *name() const {
            return GetFieldTypeIndex(typeIndex).name();
        }

        bool operator==(const std::type_index &other) const {
            return GetFieldTypeIndex(typeIndex) == other;
        };

        bool operator==(const TypeInfo &other) const = default;
    };

    struct StructFunction {
        std::string name, desc;
        bool isStatic, isConst;
        TypeInfo returnType;
        std::vector<TypeInfo> argTypes;
        std::vector<ArgDesc> argDescs;
        int funcIndex = -1;

        StructFunction(const std::string &name,
            const std::string &desc,
            bool isStatic,
            bool isConst,
            TypeInfo returnType,
            std::vector<TypeInfo> argTypes,
            std::vector<ArgDesc> argDescs)
            : name(name), desc(sp::trim(desc)), isStatic(isStatic), isConst(isConst), returnType(returnType),
              argTypes(argTypes), argDescs(argDescs) {}

        /**
         * Registers a struct's member function as a callable scripting function. For example:
         * StructFunction::New("GetParent", &Transform::GetParent)
         */
        template<typename T, typename R, typename... Args>
        static const StructFunction New(const std::string &name, R (T::*F)(Args...)) {
            return StructFunction(name,
                "No description",
                false, // isStatic
                false, // isConst
                TypeInfo::Lookup<R>(),
                std::vector<TypeInfo>({TypeInfo::Lookup<Args>()...}),
                {});
        }

        template<typename T, typename R, typename... Args, typename... Descs>
        static const StructFunction New(const std::string &name,
            const std::string &desc,
            R (T::*F)(Args...),
            Descs &&...argDescs) {
            return StructFunction(name,
                desc,
                false, // isStatic
                false, // isConst
                TypeInfo::Lookup<R>(),
                std::vector<TypeInfo>({TypeInfo::Lookup<Args>()...}),
                std::vector<ArgDesc>({argDescs...}));
        }

        template<typename T, typename R, typename... Args>
        static const StructFunction New(const std::string &name, R (T::*F)(Args...) const) {
            return StructFunction(name,
                "No description",
                false, // isStatic
                true, // isConst
                TypeInfo::Lookup<R>(),
                std::vector<TypeInfo>({TypeInfo::Lookup<Args>()...}),
                {});
        }

        template<typename T, typename R, typename... Args, typename... Descs>
        static const StructFunction New(const std::string &name,
            const std::string &desc,
            R (T::*F)(Args...) const,
            Descs &&...argDescs) {
            return StructFunction(name,
                desc,
                false, // isStatic
                true, // isConst
                TypeInfo::Lookup<R>(),
                std::vector<TypeInfo>({TypeInfo::Lookup<Args>()...}),
                std::vector<ArgDesc>({argDescs...}));
        }

        /**
         * Registers a static function as a callable scripting function. For example:
         * StructFunction::New("NewScript", &Script::New)
         */
        template<typename R, typename... Args>
        static const StructFunction New(const std::string &name, R (*F)(Args...)) {
            return StructFunction(name,
                "No description",
                true, // isStatic
                false, // isConst
                TypeInfo::Lookup<R>(),
                std::vector<TypeInfo>({TypeInfo::Lookup<Args>()...}),
                {});
        }

        template<typename R, typename... Args, typename... Descs>
        static const StructFunction New(const std::string &name,
            const std::string &desc,
            R (*F)(Args...),
            Descs &&...argDescs) {
            return StructFunction(name,
                desc,
                true, // isStatic
                false, // isConst
                TypeInfo::Lookup<R>(),
                std::vector<TypeInfo>({TypeInfo::Lookup<Args>()...}),
                std::vector<ArgDesc>({argDescs...}));
        }

        bool operator==(const StructFunction &) const = default;
    };

    enum class FieldAction {
        None = 0,
        AutoLoad = 1 << 0,
        AutoSave = 1 << 1,
        AutoApply = 1 << 2,
    };

    struct StructField {
        std::string name, desc;
        TypeInfo type;
        size_t size;
        size_t offset = 0;
        int fieldIndex = -1;
        FieldAction actions = ~FieldAction::None;
        std::optional<StructFunction> functionPointer;

        StructField(const std::string &name,
            const std::string &desc,
            const TypeInfo &type,
            size_t size,
            size_t offset,
            FieldAction actions,
            const std::optional<StructFunction> &functionPointer)
            : name(name), desc(sp::trim(desc)), type(type), size(size), offset(offset), actions(actions),
              functionPointer(functionPointer) {}

        template<typename T, typename F>
        static size_t OffsetOf(const F T::*M) {
            // This is technically undefined behavior, but this is how the offsetof() macro works in MSVC.
            return reinterpret_cast<size_t>(&(reinterpret_cast<T const volatile *>(NULL)->*M));
        }

        template<typename T>
        static std::optional<StructFunction> AsFunctionPointer() {
            if constexpr (std::is_pointer<T>() && std::is_function<std::remove_pointer_t<T>>()) {
                T funcPtr = nullptr;
                return StructFunction::New("", funcPtr);
            } else {
                return {};
            }
        }

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
        static const StructField New(const std::string &name, const F T::*M, FieldAction actions = ~FieldAction::None) {
            return StructField(name,
                "No description",
                TypeInfo::Lookup<std::remove_cv_t<F>>(),
                sizeof(F),
                OffsetOf(M),
                actions,
                AsFunctionPointer<F>());
        }

        template<typename T, typename F>
        static const StructField New(const std::string &name,
            const std::string &desc,
            const F T::*M,
            FieldAction actions = ~FieldAction::None) {
            return StructField(name,
                desc,
                TypeInfo::Lookup<std::remove_cv_t<F>>(),
                sizeof(F),
                OffsetOf(M),
                actions,
                AsFunctionPointer<F>());
        }

        template<typename T>
        static const StructField New(const std::string &name,
            const std::string &desc,
            size_t offset,
            FieldAction actions = FieldAction::None,
            const std::optional<StructFunction> &functionPointer = {}) {
            return StructField(name,
                desc,
                TypeInfo::Lookup<std::remove_cv_t<T>>(),
                sizeof(T),
                offset,
                actions,
                functionPointer);
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
            return StructField::New("", "No description", M, actions);
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
            return StructField("",
                "No description",
                TypeInfo::Lookup<std::remove_cv_t<T>>(),
                sizeof(T),
                0,
                actions,
                AsFunctionPointer<T>());
        }

        void *AccessMut(void *structPtr) const {
            return static_cast<char *>(structPtr) + offset;
        }

        const void *Access(const void *structPtr) const {
            return static_cast<const char *>(structPtr) + offset;
        }

        template<typename T>
        T &Access(void *structPtr) const {
            Assertf(type == TypeInfo::Lookup<T>(),
                "StructMetadata::Access called with wrong type: %s, expected %s",
                typeid(T).name(),
                type.name());
            return *reinterpret_cast<T *>(AccessMut(structPtr));
        }

        template<typename T>
        const T &Access(const void *structPtr) const {
            Assertf(type == TypeInfo::Lookup<T>(),
                "StructMetadata::Access called with wrong type: %s, expected %s",
                typeid(T).name(),
                type.name());
            return *reinterpret_cast<const T *>(Access(structPtr));
        }

        bool operator==(const StructField &) const = default;

        void InitUndefined(void *dstStruct, const void *defaultStruct) const;
        void DefineSchema(picojson::value &dst, sp::json::SchemaTypeReferences *references) const;
        picojson::value SaveDefault(const EntityScope &scope, const void *defaultStruct) const;
        void SetScope(void *dstStruct, const EntityScope &scope) const;
        bool Compare(const void *a, const void *b) const;
        bool Load(void *dstStruct, const picojson::value &src) const;
        // If defaultStruct is nullptr, the field value is always saved
        void Save(const EntityScope &scope,
            picojson::value &dst,
            const void *srcStruct,
            const void *defaultStruct) const;
        void Apply(void *dstStruct, const void *srcStruct, const void *defaultPtr) const;
    };

    class StructMetadata : public sp::NonCopyable {
    public:
        using EnumDescriptions = std::map<uint32_t, std::string>;

        static constexpr bool foo = std::is_trivially_copy_constructible_v<std::type_index>;

        template<typename... Fields>
        StructMetadata(const std::type_index &idx, size_t size, const char *name, const char *desc, Fields &&...fields)
            : type(idx), size(size), name(name), description(sp::trim(desc)) {
            (
                [&] {
                    using FieldT = std::decay_t<Fields>;
                    if constexpr (std::is_same_v<FieldT, EnumDescriptions>) {
                        enumMap = fields;
                    } else if constexpr (std::is_same_v<FieldT, StructFunction>) {
                        this->functions.emplace_back(fields);
                    } else {
                        this->fields.emplace_back(fields);
                    }
                }(),
                ...);
            for (size_t i = 0; i < this->fields.size(); i++) {
                this->fields[i].fieldIndex = i;
            }
            for (size_t i = 0; i < this->functions.size(); i++) {
                this->functions[i].funcIndex = i;
            }
            Register(type, this);
        }

        StructMetadata(const StructMetadata &&other)
            : type(other.type), size(other.size), name(other.name), description(other.description),
              fields(other.fields), functions(other.functions), enumMap(other.enumMap) {
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
        const size_t size;
        const std::string name;
        const std::string description;
        std::vector<StructField> fields;
        std::vector<StructFunction> functions;
        EnumDescriptions enumMap;

        // === The following functions are meant to specialized by individual structs

        template<typename T>
        static void InitUndefined(T &dst) {
            // Custom field init is always called, default to no-op.
        }

        template<typename T>
        static void DefineSchema(picojson::value &dst, sp::json::SchemaTypeReferences *references) {
            // Custom field serialization is always called, default to no-op.
        }

        template<typename T>
        static void SetScope(T &dst, const EntityScope &scope) {
            // Custom field SetScope is always called, default to no-op.
        }

        template<typename T>
        static bool Load(T &dst, const picojson::value &src) {
            // Custom field serialization is always called, default to no-op.
            return true;
        }

        template<typename T>
        static void Save(const EntityScope &scope, picojson::value &dst, const T &src, const T *def) {
            // Custom field serialization is always called, default to no-op.
        }

        bool operator==(const StructMetadata &other) const {
            return type == other.type && name == other.name && description == other.description &&
                   fields == other.fields && enumMap == other.enumMap;
        }

    private:
        static void Register(const std::type_index &idx, const StructMetadata *comp);
    };

    namespace scope {
        template<typename T>
        inline void SetScope(T &dst, const EntityScope &scope) {
            auto *metadata = StructMetadata::Get(typeid(T));
            if (metadata) {
                for (auto &field : metadata->fields) {
                    if (field.name.empty() && field.type == metadata->type) continue;
                    field.SetScope(&dst, scope);
                }
                StructMetadata::SetScope(dst, scope);
            }
        }

        template<>
        inline void SetScope(EntityRef &dst, const EntityScope &scope) {
            dst.SetScope(scope);
        }

        template<>
        inline void SetScope(SignalRef &dst, const EntityScope &scope) {
            dst.SetScope(scope);
        }

        template<typename T>
        inline void SetScope(std::vector<T> &dst, const EntityScope &scope) {
            for (auto &item : dst) {
                SetScope(item, scope);
            }
        }

        template<typename T>
        inline void SetScope(std::optional<T> &dst, const EntityScope &scope) {
            if (dst) SetScope(*dst, scope);
        }

        template<typename K, typename T, typename H, typename E>
        inline void SetScope(robin_hood::unordered_flat_map<K, T, H, E> &dst, const EntityScope &scope) {
            for (auto &item : dst) {
                SetScope(item.second, scope);
            }
        }

        template<typename K, typename T, typename H, typename E>
        inline void SetScope(robin_hood::unordered_node_map<K, T, H, E> &dst, const EntityScope &scope) {
            for (auto &item : dst) {
                SetScope(item.second, scope);
            }
        }
    } // namespace scope
}; // namespace ecs
