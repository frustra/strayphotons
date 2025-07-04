/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"

#include <memory>
#include <string_view>

namespace ecs {
#ifdef __GNUC__
    __attribute__((unused))
#endif
    static const char *DocsDescriptionEntityRef = R"(
An `EntityRef` is a stable reference to an entity via a string name. 

Referenced entities do not need to exist at the point an `EntityRef` is defined.
The reference will be automatically tracked and updated once the referenced entity is created.

Reference names are defined the same as the `name` component:  
`"<scene_name>:<entity_name>"`

References can also be defined relative to their entity scope, the same as a `name` component.
If just a relative name is provided, the reference will be expanded based on the scope root:  
`"<scene_name>:<root_name>.<relative_name>"`

The special `"scoperoot"` alias can be used to reference the parent entity during template generation.
)";

    class EntityRef;

    struct NamedEntity {
        ecs::Name name;
        Entity ent;

        NamedEntity() {}
        NamedEntity(Entity ent);
        NamedEntity(const ecs::Name &name, Entity ent = Entity());

        ecs::Name Name() const;
        Entity Get(const Lock<> &lock) const;
        Entity GetLive() const;
        Entity GetStaging() const;
        bool IsValid() const;

        void SetScope(const EntityScope &scope);
        void Clear();

        static NamedEntity Lookup(Entity ent);
        static NamedEntity Find(const char *name, const EntityScope *scope = nullptr);

        operator EntityRef() const;

        explicit operator bool() const {
            return name || ent;
        }

        bool operator==(const NamedEntity &other) const;
        bool operator==(const Entity &other) const;
        bool operator<(const NamedEntity &other) const;
    };

    class EntityRef {
    private:
        struct Ref;

    public:
        EntityRef() {}
        EntityRef(const Entity &ent);
        EntityRef(const ecs::Name &name, const Entity &ent = Entity());
        EntityRef(const EntityRef &ref) : ptr(ref.ptr) {}
        EntityRef(const std::shared_ptr<Ref> &ptr) : ptr(ptr) {}

        ecs::Name Name() const;
        Entity Get(const Lock<> &lock) const;
        Entity GetLive() const;
        Entity GetStaging() const;
        bool IsValid() const;

        void SetScope(const EntityScope &scope);
        void Clear();

        static EntityRef Empty();
        static EntityRef New(Entity ent);
        static EntityRef Copy(const EntityRef &ref);
        static EntityRef Lookup(const char *name, const EntityScope *scope = nullptr);

        explicit operator bool() const {
            return !!ptr;
        }

        bool operator==(const EntityRef &other) const;
        bool operator==(const NamedEntity &other) const;
        bool operator==(const Entity &other) const;
        bool operator<(const EntityRef &other) const;

    private:
        std::shared_ptr<Ref> ptr;

        friend class EntityReferenceManager;
    };
} // namespace ecs
