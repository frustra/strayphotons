/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Tecs_entity.hh"
#include "strayphotons/FlatSet.hh"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/Logging.hh"

#include <Tecs.hh>
#include <string>

namespace sp {
    /**
     * An entity map implementation meant to mimic the behavior of std::map<Tecs::Entity, T>.
     * Values are stored in a vector according to their entity index, allowing for O(1) insertions and deletions.
     */
    template<typename T>
    class EntityMap {
    public:
        typedef Tecs::Entity key_type;
        typedef T mapped_type;
        typedef std::pair<Tecs::Entity, T> value_type;
        typedef size_t size_type;
        typedef std::ptrdiff_t difference_type;

        typedef value_type *pointer;
        typedef const value_type *const_pointer;
        typedef value_type &reference;
        typedef const value_type &const_reference;

        class iterator {
        public:
            typedef std::ptrdiff_t difference_type;
            typedef std::pair<Tecs::Entity, T> value_type;
            typedef std::pair<Tecs::Entity, T> *pointer;
            typedef std::pair<Tecs::Entity, T> &reference;
            typedef std::random_access_iterator_tag iterator_category;

            iterator(EntityMap &map, size_t index = 0) : map(map), i(index) {}

            inline reference operator*() const {
                DebugAssertf(i < map.validEntities.size(),
                    "EntityMap::iterator::operator*: index out of bounds: %u >= %u",
                    i,
                    map.validEntities.size());
                TECS_ENTITY_INDEX_TYPE index = map.validEntities[i];
                DebugAssertf(index < map.storage.size(),
                    "EntityMap::iterator::operator*: entity out of bounds: %u >= %u",
                    index,
                    map.storage.size());
                return map.storage[index];
            }

            inline pointer operator->() const {
                DebugAssertf(i < map.validEntities.size(),
                    "EntityMap::iterator::operator->: index out of bounds: %u >= %u",
                    i,
                    map.validEntities.size());
                TECS_ENTITY_INDEX_TYPE index = map.validEntities[i];
                DebugAssertf(index < map.storage.size(),
                    "EntityMap::iterator::operator->: entity out of bounds: %u >= %u",
                    index,
                    map.storage.size());
                return &map.storage[index];
            }

            inline reference operator[](difference_type n) const {
                DebugAssertf(i + n < map.validEntities.size(),
                    "EntityMap::iterator::operator[]: index out of bounds: %u >= %u",
                    i + n,
                    map.validEntities.size());
                TECS_ENTITY_INDEX_TYPE index = map.validEntities[i + n];
                DebugAssertf(index < map.storage.size(),
                    "EntityMap::iterator::operator[]: entity out of bounds: %u >= %u",
                    index,
                    map.storage.size());
                return map.storage[index];
            }

            inline iterator &operator++() noexcept {
                i++;
                return *this;
            }

            inline iterator &operator--() noexcept {
                i--;
                return *this;
            }

            inline iterator operator++(int) noexcept {
                iterator tmp = *this;
                i++;
                return tmp;
            }

            inline iterator operator--(int) noexcept {
                iterator tmp = *this;
                i--;
                return tmp;
            }

            inline iterator operator+(difference_type n) const noexcept {
                return iterator(map, i + n);
            }

            inline iterator operator-(difference_type n) const noexcept {
                return iterator(map, i - n);
            }

            inline iterator &operator+=(difference_type n) noexcept {
                i += n;
                return *this;
            }

            inline iterator &operator-=(difference_type n) noexcept {
                i -= n;
                return *this;
            }

            inline bool operator==(const iterator &other) const noexcept {
                return map == other.map && i == other.i;
            }

            inline bool operator!=(const iterator &other) const noexcept {
                return map != other.map || i != other.i;
            }

            EntityMap<T> &map;
            size_t i;
        };

        class const_iterator {
        public:
            typedef std::ptrdiff_t difference_type;
            typedef const std::pair<Tecs::Entity, T> value_type;
            typedef const std::pair<Tecs::Entity, T> *pointer;
            typedef const std::pair<Tecs::Entity, T> &reference;
            typedef std::random_access_iterator_tag iterator_category;

            const_iterator(const EntityMap &map, size_t index = 0) : map(map), i(index) {}

            inline const_reference operator*() const {
                DebugAssertf(i < map.validEntities.size(),
                    "EntityMap::iterator::operator*: index out of bounds: %u >= %u",
                    i,
                    map.validEntities.size());
                TECS_ENTITY_INDEX_TYPE index = map.validEntities[i];
                DebugAssertf(index < map.storage.size(),
                    "EntityMap::iterator::operator*: entity out of bounds: %u >= %u",
                    index,
                    map.storage.size());
                return map.storage[index];
            }

            inline const_pointer operator->() const {
                DebugAssertf(i < map.validEntities.size(),
                    "EntityMap::iterator::operator->: index out of bounds: %u >= %u",
                    i,
                    map.validEntities.size());
                TECS_ENTITY_INDEX_TYPE index = map.validEntities[i];
                DebugAssertf(index < map.storage.size(),
                    "EntityMap::iterator::operator->: entity out of bounds: %u >= %u",
                    index,
                    map.storage.size());
                return &map.storage[index];
            }

            inline const_reference operator[](difference_type n) const {
                DebugAssertf(i + n < map.validEntities.size(),
                    "EntityMap::iterator::operator[]: index out of bounds: %u >= %u",
                    i + n,
                    map.validEntities.size());
                TECS_ENTITY_INDEX_TYPE index = map.validEntities[i + n];
                DebugAssertf(index < map.storage.size(),
                    "EntityMap::iterator::operator[]: entity out of bounds: %u >= %u",
                    index,
                    map.storage.size());
                return map.storage[index];
            }

            inline const_iterator &operator++() noexcept {
                i++;
                return *this;
            }

            inline const_iterator &operator--() noexcept {
                i--;
                return *this;
            }

            inline const_iterator operator++(int) noexcept {
                const_iterator tmp = *this;
                i++;
                return tmp;
            }

            inline const_iterator operator--(int) noexcept {
                const_iterator tmp = *this;
                i--;
                return tmp;
            }

            inline const_iterator operator+(difference_type n) const noexcept {
                return const_iterator(map, i + n);
            }

            inline const_iterator operator-(difference_type n) const noexcept {
                return const_iterator(map, i - n);
            }

            inline const_iterator &operator+=(difference_type n) noexcept {
                i += n;
                return *this;
            }

            inline const_iterator &operator-=(difference_type n) noexcept {
                i -= n;
                return *this;
            }

            inline bool operator==(const const_iterator &other) const noexcept {
                return map == other.map && i == other.i;
            }

            inline bool operator!=(const const_iterator &other) const noexcept {
                return map != other.map || i != other.i;
            }

            const EntityMap<T> &map;
            size_t i;
        };

        typedef std::reverse_iterator<iterator> reverse_iterator;
        typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

        // Warning: This function will overwrite data when an entity index is reused.
        T &operator[](const Tecs::Entity &e) {
            Assert(e, "Referencing EntityMap with null entity");
            if (e.index >= storage.size()) storage.resize(e.index + 1);
            value_type &entry = storage[e.index];
            if (!entry.first) {
                entry.first = e;
                if (validEntities.empty()) {
                    identifier = Tecs::IdentifierFromGeneration(e.generation);
                } else {
                    Assertf(Tecs::IdentifierFromGeneration(e.generation) == identifier,
                        "EntityMap referencing entity from wrong ECS instance");
                }
                validEntities.emplace(e.index);
            }
            if (entry.first == e) {
                return entry.second;
            } else {
                Assertf(Tecs::IdentifierFromGeneration(e.generation) == identifier,
                    "EntityMap referencing entity from wrong ECS instance");
                // The index is already valid, but from the wrong generation, just overwrite the old value.
                entry = {e, {}};
            }
            return entry.second;
        }

        const T &operator[](const Tecs::Entity &e) const {
            Assert(e, "Referencing EntityMap with null entity");
            Assert(e.index < storage.size(), "Referencing EntityMap with out of range entity");
            auto &entry = storage[e.index];
            Assert(entry.first == e, "Referencing EntityMap with mismatched generation id");
            return entry.second;
        }

        iterator begin() {
            return iterator(*this);
        }
        const_iterator begin() const {
            return const_iterator(*this);
        }

        iterator end() {
            return iterator(*this, validEntities.size());
        }
        const_iterator end() const {
            return const_iterator(*this, validEntities.size());
        }

        reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        }
        const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        }
        const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        bool empty() const noexcept {
            return validEntities.empty();
        }

        size_type size() const noexcept {
            return validEntities.size();
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<TECS_ENTITY_INDEX_TYPE>::max();
        }

        void clear() {
            storage.clear();
            validEntities.clear();
        }

        size_type erase(const Tecs::Entity &e) {
            Assertf(!e || validEntities.empty() || Tecs::IdentifierFromGeneration(e.generation) == identifier,
                "EntityMap referencing entity from wrong ECS instance");
            if (!e || e.index >= storage.size()) return 0;
            auto &entry = storage[e.index];
            if (entry.first == e) {
                validEntities.erase(e.index);
                entry = {};
                return 1;
            }
            return 0;
        }

        size_type erase(const T &value) {
            size_type count = 0;
            for (auto it = validEntities.begin(); it != validEntities.end();) {
                const TECS_ENTITY_INDEX_TYPE &index = *it;
                auto &entry = storage[index];
                if (entry.second == value) {
                    it = validEntities.erase(it);
                    entry = {};
                    count++;
                } else {
                    it++;
                }
            }
            return count;
        }

        size_t count(const Tecs::Entity &e) const {
            Assertf(!e || validEntities.empty() || Tecs::IdentifierFromGeneration(e.generation) == identifier,
                "EntityMap referencing entity from wrong ECS instance");
            if (!e || e.index >= storage.size()) return 0;
            auto &entry = storage[e.index];
            if (entry.first == e) return 1;
            return 0;
        }

        T *find(const Tecs::Entity &e) {
            Assertf(!e || validEntities.empty() || Tecs::IdentifierFromGeneration(e.generation) == identifier,
                "EntityMap referencing entity from wrong ECS instance");
            if (!e || e.index >= storage.size()) return nullptr;
            auto &entry = storage[e.index];
            if (entry.first == e) return &entry.second;
            return nullptr;
        }

        const T *find(const Tecs::Entity &e) const {
            Assertf(!e || validEntities.empty() || Tecs::IdentifierFromGeneration(e.generation) == identifier,
                "EntityMap referencing entity from wrong ECS instance");
            if (!e || e.index >= storage.size()) return nullptr;
            auto &entry = storage[e.index];
            if (entry.first == e) return &entry.second;
            return nullptr;
        }

        bool operator==(const EntityMap<T> &other) const noexcept {
            return storage == other.storage;
        }

    private:
        HeapVector<value_type> storage;
        FlatSet<TECS_ENTITY_INDEX_TYPE> validEntities;

        TECS_ENTITY_ECS_IDENTIFIER_TYPE identifier;
    };
} // namespace sp
