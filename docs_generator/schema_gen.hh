/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/JsonHelpers.hh"
#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"

namespace detail {
    template<typename... AllComponentTypes, template<typename...> typename ECSType, typename Func>
    void ForEachComponentType(ECSType<AllComponentTypes...> *, Func &&callback) {
        ( // For each component:
            [&] {
                using T = AllComponentTypes;

                if constexpr (std::is_same_v<T, ecs::Name> || std::is_same_v<T, ecs::SceneInfo>) {
                    // Skip
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    callback((T *)nullptr);
                }
            }(),
            ...);
    }

    // Calls the provided auto-lambda for all components except Name and SceneInfo
    template<typename Func>
    void ForEachComponentType(Func &&callback) {
        ForEachComponentType((ecs::ECS *)nullptr, std::forward<Func>(callback));
    }
} // namespace detail

struct SchemaContext {
    picojson::object definitions;

    std::ofstream file;

    SchemaContext(const std::filesystem::path &outputPath) : file(outputPath) {
        Assertf(file, "Failed to open output file: '%s'", outputPath.string());
    }

    void AddDefinition(const ecs::StructMetadata &metadata) {
        if (definitions.count(metadata.name)) return;

        ecs::GetFieldType<void>(metadata.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            sp::json::SchemaTypeReferences references;
            picojson::value typeSchema;
            sp::json::SaveSchema<T>(typeSchema, &references);
            definitions.emplace(metadata.name, typeSchema.get<picojson::object>());

            for (auto *ref : references) {
                if (ref) AddDefinition(*ref);
            }
        });
    }

    void SaveSchema() {
        picojson::object entityProperties;

        auto &nameComp = ecs::LookupComponent<ecs::Name>();
        sp::json::SaveSchema<ecs::Name>(entityProperties[nameComp.name]);
        detail::ForEachComponentType([&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            auto &comp = ecs::LookupComponent<T>();
            sp::json::SchemaTypeReferences references;
            sp::json::SaveSchema<T>(entityProperties[comp.name], &references);

            for (auto *metadata : references) {
                if (metadata) AddDefinition(*metadata);
            }
        });

        picojson::object root;
        root["$schema"] = picojson::value("http://json-schema.org/draft-07/schema#");
        root["title"] = picojson::value("Scene Definition");
        root["type"] = picojson::value("object");
        if (!definitions.empty()) root["definitions"] = picojson::value(definitions);

        picojson::object entityItems;
        entityItems["type"] = picojson::value("object");
        entityItems["properties"] = picojson::value(entityProperties);

        picojson::object entityProperty;
        entityProperty["type"] = picojson::value("array");
        entityProperty["items"] = picojson::value(entityItems);

        picojson::object properties;
        properties["entities"] = picojson::value(entityProperty);

        root["properties"] = picojson::value(properties);
        file << picojson::value(root).serialize(true);
    }
};
