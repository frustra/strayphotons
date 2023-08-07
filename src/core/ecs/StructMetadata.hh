/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/Async.hh"
#include "core/Common.hh"
#include "core/EnumTypes.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalRef.hh"
#include "ecs/components/Name.hh"

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
    enum class FieldAction {
        None = 0,
        AutoLoad = 1 << 0,
        AutoSave = 1 << 1,
        AutoApply = 1 << 2,
    };

    struct StructField {
        std::string name, desc;
        std::type_index type;
        size_t offset = 0;
        int fieldIndex = -1;
        FieldAction actions = ~FieldAction::None;

        StructField(const std::string &name,
            const std::string &desc,
            std::type_index type,
            size_t offset,
            FieldAction actions)
            : name(name), desc(sp::trim(desc)), type(type), offset(offset), actions(actions) {}

        template<typename T, typename F>
        static size_t OffsetOf(const F T::*M) {
            // This is technically undefined behavior, but this is how the offsetof() macro works in MSVC.
            return reinterpret_cast<size_t>(&(reinterpret_cast<T const volatile *>(NULL)->*M));
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
                std::type_index(typeid(std::remove_cv_t<F>)),
                OffsetOf(M),
                actions);
        }

        template<typename T, typename F>
        static const StructField New(const std::string &name,
            const std::string &desc,
            const F T::*M,
            FieldAction actions = ~FieldAction::None) {
            return StructField(name, desc, std::type_index(typeid(std::remove_cv_t<F>)), OffsetOf(M), actions);
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
            return StructField("", "No description", std::type_index(typeid(std::remove_cv_t<T>)), 0, actions);
        }

        void *Access(void *structPtr) const {
            return static_cast<char *>(structPtr) + offset;
        }

        const void *Access(const void *structPtr) const {
            return static_cast<const char *>(structPtr) + offset;
        }

        template<typename T>
        T &Access(void *structPtr) const {
            Assertf(type == typeid(T),
                "StructMetadata::Access called with wrong type: %s, expected %s",
                typeid(T).name(),
                type.name());
            return *reinterpret_cast<T *>(Access(structPtr));
        }

        template<typename T>
        const T &Access(const void *structPtr) const {
            Assertf(type == typeid(T),
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

    class StructMetadata {
    public:
        using EnumDescriptions = std::map<uint32_t, std::string>;

        template<typename... Fields>
        StructMetadata(const std::type_index &idx, const char *name, const char *desc, Fields &&...fields)
            : type(idx), name(name), description(sp::trim(desc)) {
            (
                [&] {
                    using FieldT = std::decay_t<Fields>;
                    if constexpr (std::is_same_v<FieldT, const EnumDescriptions *>) {
                        enumMap = fields;
                    } else {
                        this->fields.emplace_back(fields);
                    }
                }(),
                ...);
            for (size_t i = 0; i < this->fields.size(); i++) {
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
        const std::string name;
        const std::string description;
        std::vector<StructField> fields;
        const EnumDescriptions *enumMap = nullptr;

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

        template<typename T>
        inline void SetScope(robin_hood::unordered_flat_map<std::string, T> &dst, const EntityScope &scope) {
            for (auto &item : dst) {
                SetScope(item.second, scope);
            }
        }

        template<typename T>
        inline void SetScope(robin_hood::unordered_node_map<std::string, T> &dst, const EntityScope &scope) {
            for (auto &item : dst) {
                SetScope(item.second, scope);
            }
        }
    } // namespace scope
}; // namespace ecs
