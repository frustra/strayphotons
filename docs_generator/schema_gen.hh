#pragma once

#include "assets/JsonHelpers.hh"
#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"

struct SchemaContext {
    picojson::object definitions;

    void AddDefinition(const ecs::StructMetadata &metadata) {
        if (definitions.count(metadata.name)) return;

        ecs::GetFieldType(metadata.type, [&](auto *typePtr) {
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
};
