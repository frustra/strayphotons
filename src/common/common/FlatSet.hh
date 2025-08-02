/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <algorithm>
#include <functional>
#include <vector>

namespace sp {
    template<typename T, typename Compare = std::less<T>, typename ContainerT = std::vector<T>>
    class FlatSet : private ContainerT, Compare {
        Compare &cmp() {
            return *this;
        }
        const Compare &cmp() const {
            return *this;
        }

    public:
        using key_type = typename ContainerT::value_type;
        using value_type = typename ContainerT::value_type;
        using difference_type = typename ContainerT::difference_type;
        using size_type = typename ContainerT::size_type;
        using container_type = ContainerT;
        using key_compare = Compare;
        using reference = typename ContainerT::reference;
        using const_reference = typename ContainerT::const_reference;
        using pointer = typename ContainerT::pointer;
        using const_pointer = typename ContainerT::const_pointer;
        using iterator = typename ContainerT::iterator;
        using const_iterator = typename ContainerT::const_iterator;
        using reverse_iterator = typename ContainerT::reverse_iterator;
        using const_reverse_iterator = typename ContainerT::const_reverse_iterator;
        using ContainerT::back;
        using ContainerT::begin;
        using ContainerT::capacity;
        using ContainerT::clear;
        using ContainerT::ContainerT;
        using ContainerT::data;
        using ContainerT::end;
        using ContainerT::erase;
        using ContainerT::front;
        using ContainerT::max_size;
        using ContainerT::rbegin;
        using ContainerT::rend;
        using ContainerT::size;

        FlatSet() : ContainerT() {
            ContainerT::resize(0);
        }

        FlatSet(std::initializer_list<T> init) : ContainerT() {
            for (auto it = init.begin(); it != init.end(); it++) {
                insert(*it);
            }
        }

        template<typename Key>
        iterator find(const Key &key) const {
            auto it = std::lower_bound(begin(), end(), key, cmp());
            if (it != end() && !cmp()(key, *it)) return it;

            return end();
        }

        template<typename Key>
        size_t count(const Key &key) const {
            return find(key) == end() ? 0 : 1;
        }

        std::pair<iterator, bool> insert(const T &val) {
            auto it = std::lower_bound(begin(), end(), val, cmp());
            if (it != end() && !cmp()(val, *it)) {
                return {it, false};
            }

            return {ContainerT::emplace(it, val), true};
        }

        std::pair<iterator, bool> insert(T &&val) {
            auto it = std::lower_bound(begin(), end(), val, cmp());
            if (it != end() && !cmp()(val, *it)) {
                return {it, false};
            }

            return {ContainerT::emplace(it, std::forward<T>(val)), true};
        }

        template<typename... Args>
        std::pair<iterator, bool> emplace(Args &&...args) {
            return insert(std::move(T(std::forward<Args>(args)...)));
        }

        template<typename Key>
        size_t erase(const Key &key) {
            auto it = find(key);
            if (it == end()) return 0;
            ContainerT::erase(it);
            return 1;
        }

        bool operator==(const FlatSet<T, Compare, ContainerT> &other) const {
            return size() == other.size() && std::equal(begin(), end(), other.begin());
        }
    };
} // namespace sp
