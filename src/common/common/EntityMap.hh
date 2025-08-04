/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/Logging.hh"

#include <Tecs.hh>
#include <map>
#include <vector>

namespace sp {
    /**
     * An entity map implementation meant to mimic the behavior of std::map<Tecs::Entity, T>.
     * Values are stored in a vector according to their entity index, allowing for O(1) insertions and deletions.
     */
    template<typename T, typename VectorT = std::vector<std::pair<TECS_ENTITY_GENERATION_TYPE, T>>>
    class EntityMap : private VectorT {
    public:
        // Warning: This function will overwrite data when an entity index is reused.
        T &operator[](const Tecs::Entity &e) {
            Assert(e, "Referencing EntityMap with null entity");
            if (e.index >= VectorT::size()) VectorT::resize(e.index + 1);
            auto &entry = VectorT::operator[](e.index);
            if (entry.first == 0) entry.first = e.generation;
            if (entry.first == e.generation) {
                return entry.second;
            } else {
                entry = {e.generation, {}};
            }
            return entry.second;
        }

        const T &operator[](const Tecs::Entity &e) const {
            Assert(e, "Referencing EntityMap with null entity");
            Assert(e.index < VectorT::size(), "Referencing EntityMap with out of range entity");
            auto &entry = VectorT::operator[](e.index);
            Assert(entry.first == e.generation, "Referencing EntityMap with mismatched generation id");
            return entry.second;
        }

        T *find(const Tecs::Entity &e) {
            if (!e || e.index >= VectorT::size()) return nullptr;
            auto &entry = VectorT::operator[](e.index);
            if (entry.first == e.generation) return &entry.second;
            return nullptr;
        }

        const T *find(const Tecs::Entity &e) const {
            if (!e || e.index >= VectorT::size()) return nullptr;
            auto &entry = VectorT::operator[](e.index);
            if (entry.first == e.generation) return &entry.second;
            return nullptr;
        }

        // Iteration of this type is allowed, but inefficient due to its sparse layout.
        using VectorT::begin;
        using VectorT::end;

        size_t count(const Tecs::Entity &e) const {
            if (!e || e.index >= VectorT::size()) return 0;
            auto &entry = VectorT::operator[](e.index);
            if (entry.first == e.generation) return 1;
            return 0;
        }

        void erase(const Tecs::Entity &e) {
            if (!e || e.index >= VectorT::size()) return;
            auto &entry = VectorT::operator[](e.index);
            if (entry.first == e.generation) entry = {};
        }

        void erase(const T &value) {
            for (auto &entry : *this) {
                if (entry.first != 0 && entry.second == value) {
                    entry = {};
                }
            }
        }

        using VectorT::clear;
    };
} // namespace sp
