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
