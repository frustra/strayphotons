/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "strayphotons/cpp/Logging.hh"
#include "strayphotons/cpp/Utility.hh"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <sys/types.h>
#include <type_traits>

namespace sp {
    template<typename T>
    class HeapVector {
    public:
        using value_type = T;
        using size_type = uint64_t;
        using difference_type = int64_t;
        using reference = T &;
        using const_reference = const T &;
        using pointer = T *;
        using const_pointer = const T *;
        using iterator = T *;
        using const_iterator = const T *;
        using reverse_iterator = std::reverse_iterator<T *>;
        using const_reverse_iterator = std::reverse_iterator<const T *>;

        static_assert(alignof(T) <= alignof(std::max_align_t), "HeapVector value type has unsupported alignment");

        HeapVector() : cap(0), offset(0), storage(nullptr) {}

        HeapVector(size_t initialSize) : cap(0), offset(0), storage(nullptr) {
            resize(initialSize);
        }

        HeapVector(size_t initialSize, const T &value) : cap(0), offset(0), storage(nullptr) {
            reserve(initialSize);
            if (initialSize) {
                std::uninitialized_fill_n(begin(), initialSize, value);
                offset = initialSize;
            }
        }

        HeapVector(std::initializer_list<T> init) : cap(0), offset(0), storage(nullptr) {
            reserve(init.size());
            for (auto it = init.begin(); it != init.end(); it++) {
                push_back(*it);
            }
        }

        HeapVector(const HeapVector &other) : cap(0), offset(0), storage(nullptr) {
            if (!other.empty()) {
                size_type count = other.size();
                reserve(count);
                std::uninitialized_copy_n(other.begin(), count, begin());
                offset = count;
            }
        }

        HeapVector(HeapVector &&other) : cap(0), offset(0), storage(nullptr) {
            storage = other.storage;
            offset = other.offset;
            cap = other.cap;
            other.storage = nullptr;
            other.offset = 0;
            other.cap = 0;
        }

        ~HeapVector() {
            reset();
        }

        HeapVector &operator=(const HeapVector &other) {
            reset();
            if (!other.empty()) {
                size_type count = other.size();
                reserve(count);
                std::uninitialized_copy_n(other.begin(), count, begin());
                offset = count;
            }
            return *this;
        }

        HeapVector &operator=(HeapVector &&other) {
            reset();
            storage = other.storage;
            offset = other.offset;
            cap = other.cap;
            other.storage = nullptr;
            other.offset = 0;
            other.cap = 0;
            return *this;
        }

        HeapVector &operator=(std::initializer_list<T> init) {
            clear();
            reserve(init.size());
            for (auto it = init.begin(); it != init.end(); it++) {
                push_back(*it);
            }
            return *this;
        }

        reference at(size_type pos) {
            Assertf(pos < offset, "HeapVector index out of range: %llu/%llu", pos, offset);
            return data()[pos];
        }
        const_reference at(size_type pos) const {
            Assertf(pos < offset, "HeapVector index out of range: %llu/%llu", pos, offset);
            return data()[pos];
        }

        T &operator[](size_type pos) {
            DebugAssertf(pos < offset, "HeapOffset index out of range: %llu/%llu", pos, offset);
            return data()[pos];
        }
        const T &operator[](size_type pos) const {
            DebugAssertf(pos < offset, "HeapOffset index out of range: %llu/%llu", pos, offset);
            return data()[pos];
        }

        reference front() {
            DebugAssertf(offset > 0, "HeapVector::front() called on empty vector");
            return data()[0];
        }
        const_reference front() const {
            DebugAssertf(offset > 0, "HeapVector::front() called on empty vector");
            return data()[0];
        }

        T &back() {
            DebugAssertf(offset > 0, "HeapVector::back() called on empty vector");
            return data()[offset - 1];
        }
        const T &back() const {
            DebugAssertf(offset > 0, "HeapVector::back() called on empty vector");
            return data()[offset - 1];
        }

        T *data() {
            return static_cast<T *>(storage);
        }
        const T *data() const {
            return static_cast<const T *>(storage);
        }

        iterator begin() {
            return data();
        }
        const_iterator begin() const {
            return data();
        }

        iterator end() {
            return begin() + offset;
        }
        const_iterator end() const {
            return begin() + offset;
        }

        reverse_iterator rend() {
            return std::reverse_iterator(begin());
        }
        const const_reverse_iterator rend() const {
            return std::reverse_iterator(begin());
        }

        reverse_iterator rbegin() {
            return rend() - offset;
        }
        const_reverse_iterator rbegin() const {
            return rend() - offset;
        }

        bool empty() const {
            return !offset;
        }

        size_type size() const {
            return offset;
        }

        size_type max_size() const {
            return std::numeric_limits<difference_type>::max();
        }

        void reserve(size_type new_cap) {
            if (new_cap > cap) {
                void *old_storage = storage;
                cap = CeilToPowerOfTwo(new_cap);
                storage = ::operator new(cap * sizeof(T));
                if (old_storage) {
                    T *old_data = static_cast<T *>(old_storage);
                    std::uninitialized_move_n(old_data, offset, data());
                    std::destroy_n(old_data, offset);
                    ::operator delete(old_storage);
                }
            }
        }

        size_type capacity() const {
            return cap;
        }

        void shrink_to_fit() {
            if (cap > offset * 2) {
                void *old_storage = storage;
                cap = CeilToPowerOfTwo(offset);
                storage = ::operator new(cap * sizeof(T));
                T *old_data = static_cast<T *>(old_storage);
                std::uninitialized_move_n(old_data, offset, data());
                std::destroy_n(old_data, offset);
                ::operator delete(old_storage);
            }
        }

        void reset() {
            if (storage) {
                clear();
                ::operator delete(storage);
            }
            storage = nullptr;
            offset = 0;
            cap = 0;
        }

        void clear() {
            std::destroy(begin(), end());
            offset = 0;
        }

        iterator insert(const_iterator pos, const T &value) {
            return insert(pos, {value});
        }
        iterator insert(const_iterator pos, T &&value) {
            return insert(pos, &value, &value + 1);
        }
        template<class InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last) {
            auto n = std::distance(first, last);
            size_type posOffset = pos == nullptr ? 0 : pos - begin();
            if (offset + n > cap) reserve(offset + n);

            Assertf(posOffset >= 0 && (size_type)posOffset <= offset, "HeapVector::insert pos out of range");
            T *posPtr = data() + posOffset;
            T *oldEndPtr = data() + offset;

            std::uninitialized_default_construct_n(oldEndPtr, n);
            if (posPtr < oldEndPtr) std::move_backward(posPtr, oldEndPtr, oldEndPtr + n);
            if (last != first) {
                if constexpr (std::is_constructible<T, std::add_rvalue_reference_t<decltype(*first)>>()) {
                    std::move(first, last, posPtr);
                } else {
                    std::copy(first, last, posPtr);
                }
            }
            offset += n;
            return begin() + posOffset;
        }
        iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        template<class... Args>
        iterator emplace(const_iterator pos, Args &&...args) {
            size_type posOffset = pos == nullptr ? 0 : pos - begin();
            if (offset + 1 > cap) reserve(offset + 1);

            Assertf(posOffset >= 0 && posOffset <= offset, "HeapVector::emplace pos out of range");
            T *posPtr = data() + posOffset;
            T *oldEndPtr = data() + offset;

            std::uninitialized_default_construct_n(oldEndPtr, 1);
            if (posPtr < oldEndPtr) std::move_backward(posPtr, oldEndPtr, oldEndPtr + 1);
            *posPtr = T(std::forward<Args>(args)...);
            offset++;
            return begin() + posOffset;
        }

        iterator erase(const_iterator pos) {
            return erase(pos, pos + 1);
        }
        iterator erase(const_iterator first, const_iterator last) {
            size_type startOffset = first - begin();
            size_type endOffset = last - begin();
            Assertf(startOffset <= endOffset, "HeapVector::erase iterators out of order");
            Assertf(endOffset <= offset, "HeapVector::erase iterators out of range");
            if (endOffset == offset) {
                std::destroy(first, last);
                offset = startOffset;
                return end();
            } else if (startOffset != endOffset) {
                iterator newEnd = std::move(begin() + endOffset, end(), begin() + startOffset);
                std::destroy(newEnd, end());
                offset = newEnd - begin();
                return begin() + endOffset;
            } else {
                return begin() + endOffset;
            }
        }

        void push_back(const T &value) {
            if (offset + 1 > cap) reserve(offset + 1);
            new (begin() + offset) T{value};
            offset++;
        }

        template<class... Args>
        T &emplace_back(Args &&...args) {
            if (offset + 1 > cap) reserve(offset + 1);
            T *ptr = new (begin() + offset) T(std::forward<Args>(args)...);
            offset++;
            return *ptr;
        }

        void pop_back() {
            Assert(offset > 0, "HeapVector::pop_back() called on empty vector");
            std::destroy_at(data() + offset - 1);
            offset--;
        }

        void resize(size_type count) {
            if (count > cap) reserve(count);
            if (count < offset) {
                std::destroy(begin() + count, end());
            } else if (count > offset) {
                std::uninitialized_default_construct(begin() + offset, begin() + count);
            }
            offset = count;
        }

        bool operator==(const HeapVector<T> &other) const {
            return offset == other.offset && (storage == other.storage || std::equal(begin(), end(), other.begin()));
        }

    private:
        size_type cap = 0;
        size_type offset = 0;

        void *storage = nullptr;
    };
} // namespace sp
