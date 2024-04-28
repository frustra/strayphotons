/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Logging.hh"

#include <array>
#include <cstring>

namespace sp {
    template<typename T, size_t MaxSize, typename ArrayT = std::array<T, MaxSize>>
    class InlineVector : private ArrayT {
    public:
        using value_type = typename ArrayT::value_type;
        using size_type = typename ArrayT::size_type;
        using difference_type = typename ArrayT::difference_type;
        using reference = typename ArrayT::reference;
        using const_reference = typename ArrayT::const_reference;
        using pointer = typename ArrayT::pointer;
        using const_pointer = typename ArrayT::const_pointer;
        using iterator = typename ArrayT::iterator;
        using const_iterator = typename ArrayT::const_iterator;
        using reverse_iterator = typename ArrayT::reverse_iterator;
        using const_reverse_iterator = typename ArrayT::const_reverse_iterator;
        using ArrayT::at;
        using ArrayT::data;
        using ArrayT::front;
        using ArrayT::max_size;
        using ArrayT::operator[];
        using ArrayT::ArrayT;

        InlineVector(size_t initialSize = 0) {
            resize(initialSize);
        }

        InlineVector(size_t initialSize, const T &value) {
            resize(initialSize);
            fill(value);
        }

        InlineVector(std::initializer_list<T> init) {
            for (auto it = init.begin(); it != init.end(); it++) {
                push_back(*it);
            }
        }

        size_type size() const {
            return offset;
        }

        size_type capacity() const {
            return ArrayT::size();
        }

        void resize(size_type size) {
            Assert(size <= MaxSize, "InlineVector overflow");
            offset = size;
        }

        void fill(const T &value) {
            std::fill(begin(), end(), value);
        }

        bool empty() const {
            return !offset;
        }

        T &back() {
            return at(offset - 1);
        }

        void push_back(const T &value) {
            Assert(offset < MaxSize, "InlineVector overflow");
            at(offset++) = value;
        }

        void pop_back() {
            Assert(offset > 0, "InlineVector underflow");
            offset -= 1;
        }

        template<class... Args>
        T &emplace_back(Args &&...args) {
            Assert(offset + 1 <= MaxSize, "InlineVector overflow");
            auto &ref = at(offset++);
            ref = T(std::forward<Args>(args)...);
            return ref;
        }

        iterator insert(const_iterator pos, const T &value) {
            return insert(pos, &value, &value + 1);
        }
        template<class InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last) {
            auto n = std::distance(first, last);
            Assert(offset + n <= MaxSize, "InlineVector overflow");

            auto posOffset = pos - begin();
            T *posPtr = &at(posOffset);
            T *oldEndPtr = &at(offset);

            if (posPtr < oldEndPtr) std::memmove(posPtr + n, posPtr, (oldEndPtr - posPtr) * sizeof(T));
            std::copy(first, last, posPtr);
            offset += n;
            return begin() + posOffset;
        }
        iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        void clear() {
            offset = 0;
        }

        using ArrayT::begin;
        iterator end() {
            return begin() + offset;
        }
        const_iterator end() const {
            return begin() + offset;
        }

        using ArrayT::rend;
        reverse_iterator rbegin() {
            return rend() - offset;
        }
        const_reverse_iterator rbegin() const {
            return rend() - offset;
        }

        bool operator==(const InlineVector<T, MaxSize> &other) const {
            return offset == other.offset && std::equal(begin(), end(), other.begin());
        }

    private:
        size_type offset = 0;
    };
} // namespace sp
