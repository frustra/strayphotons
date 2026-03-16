/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "strayphotons/cpp/HeapVector.hh"
#include "strayphotons/cpp/Logging.hh"

#include <cstddef>
#include <cstring>
#include <string_view>

namespace sp {
    class HeapString : private HeapVector<char> {
        using StorageT = HeapVector<char>;

    public:
        using traits_type = std::char_traits<char>;
        using value_type = typename StorageT::value_type;
        using size_type = typename StorageT::size_type;
        using difference_type = typename StorageT::difference_type;
        using reference = typename StorageT::reference;
        using const_reference = typename StorageT::const_reference;
        using pointer = typename StorageT::pointer;
        using const_pointer = typename StorageT::const_pointer;
        using iterator = typename StorageT::iterator;
        using const_iterator = typename StorageT::const_iterator;
        using reverse_iterator = typename StorageT::reverse_iterator;
        using const_reverse_iterator = typename StorageT::const_reverse_iterator;

        static constexpr size_type npos = std::string::npos;

        using StorageT::at;
        using StorageT::begin;
        using StorageT::capacity;
        using StorageT::clear;
        using StorageT::data;
        using StorageT::front;
        using StorageT::max_size;
        using StorageT::operator[];
        using StorageT::StorageT;

        HeapString() : StorageT() {}

        HeapString(size_t count, char ch) : StorageT() {
            resize(count, ch);
        }

        HeapString(const char *init) : StorageT() {
            size_type count = init ? traits_type::length(init) : 0;
            if (count > 0) {
                resize(count);
                traits_type::copy(data(), init, count);
            }
        }

        HeapString(const std::string &init) : StorageT() {
            if (!init.empty()) {
                resize(init.size());
                traits_type::copy(data(), init.data(), init.size());
            }
        }

        HeapString(const std::string_view &init) : StorageT() {
            if (!init.empty()) {
                resize(init.size());
                traits_type::copy(data(), init.data(), init.size());
            }
        }

        size_type length() const {
            return size();
        }

        void resize(size_type newSize) {
            auto oldSize = size();
            StorageT::resize(newSize + 1);
            if (newSize > oldSize) {
                std::fill(begin() + oldSize, begin() + newSize + 1, '\0');
            } else {
                StorageT::back() = '\0';
            }
        }

        void resize(size_type newSize, char ch) {
            auto oldSize = size();
            resize(newSize);
            if (newSize > oldSize) {
                std::fill(begin() + oldSize, begin() + newSize, ch);
            }
        }

        bool empty() const {
            return StorageT::size() <= 1;
        }

        reference back() {
            Assert(StorageT::size() > 1, "HeapString::back() called on empty string");
            return data()[StorageT::size() - 2];
        }

        const_reference back() const {
            Assert(StorageT::size() > 1, "HeapString::back() called on empty string");
            return data()[StorageT::size() - 2];
        }

        iterator end() {
            return data() + size();
        }

        const_iterator end() const {
            return data() + size();
        }

        size_type size() const {
            size_type bufferSize = StorageT::size();
            return bufferSize > 1 ? bufferSize - 1 : 0;
        }

        void pop_back() {
            if (StorageT::size() > 1) {
                data()[StorageT::size() - 2] = '\0';
                StorageT::pop_back();
            }
        }

        void push_back(char ch) {
            resize(size() + 1);
            data()[StorageT::size() - 2] = ch;
        }

        reverse_iterator rend() {
            return std::reverse_iterator(begin());
        }
        const const_reverse_iterator rend() const {
            return std::reverse_iterator(begin());
        }

        reverse_iterator rbegin() {
            return rend() - size();
        }
        const_reverse_iterator rbegin() const {
            return rend() - size();
        }

        const char *c_str() const {
            if (StorageT::size() == 0) return "";
            Assertf(at(size()) == '\0', "HeapString missing null termination");
            return data();
        }

        std::string str() const {
            return std::string(data(), size());
        }

        size_type find(std::string_view str, size_type pos = 0) const {
            return std::string_view(*this).find(str, pos);
        }

        size_type find(char ch, size_type pos = 0) const {
            return std::string_view(*this).find(ch, pos);
        }

        size_type rfind(std::string_view str, size_type pos = npos) const {
            return std::string_view(*this).rfind(str, pos);
        }

        size_type rfind(char ch, size_type pos = npos) const {
            return std::string_view(*this).rfind(ch, pos);
        }

        size_type find_first_of(std::string_view str, size_type pos = 0) const {
            return std::string_view(*this).find_first_of(str, pos);
        }

        size_type find_first_of(char ch, size_type pos = 0) const {
            return std::string_view(*this).find_first_of(ch, pos);
        }

        size_type find_last_of(std::string_view str, size_type pos = npos) const {
            return std::string_view(*this).find_last_of(str, pos);
        }

        size_type find_last_of(char ch, size_type pos = npos) const {
            return std::string_view(*this).find_last_of(ch, pos);
        }

        std::string_view substr(size_type pos = 0, size_type count = npos) const {
            return std::string_view(*this).substr(pos, count);
        }

        explicit operator std::string() const {
            if (empty()) return std::string();
            Assertf(at(size()) == '\0', "HeapString missing null termination");
            return std::string(data(), size());
        }

        operator std::string_view() const {
            if (empty()) return std::string_view();
            Assertf(at(size()) == '\0', "HeapString missing null termination");
            return std::string_view(data(), size());
        }

        int compare(const HeapString &other) const {
            return std::string_view(*this).compare(other);
        }

        int compare(const std::string_view &other) const {
            return std::string_view(*this).compare(other);
        }

        int compare(const char *other) const {
            return std::string_view(*this).compare(other);
        }

        bool operator==(const HeapString &other) const {
            return std::string_view(*this) == std::string_view(other);
        }

        bool operator==(const char *other) const {
            return std::string_view(*this) == other;
        }

        template<typename StringViewLike>
        bool operator==(const StringViewLike &other) const {
            return std::string_view(*this) == other;
        }

        bool operator<(const HeapString &other) const {
            return std::string_view(*this) < std::string_view(other);
        }

        template<typename StringViewLike>
        bool operator<(const StringViewLike &other) const {
            return std::string_view(*this) < other;
        }

        template<typename StringT>
        HeapString &operator+=(const StringT &str) {
            size_t oldSize = size();
            if constexpr (std::is_same<StringT, const char *>()) {
                size_type count = str ? traits_type::length(str) : 0;
                resize(oldSize + count);
                traits_type::copy(data() + oldSize, str, count);
            } else {
                resize(oldSize + str.size());
                traits_type::copy(data() + oldSize, str.data(), str.size());
            }
            return *this;
        }

        HeapString &operator+=(char ch) {
            push_back(ch);
            return *this;
        }
    };
} // namespace sp

inline sp::HeapString operator+(const sp::HeapString &lhs, const sp::HeapString &rhs) {
    sp::HeapString result = lhs;
    result += rhs;
    return result;
}

inline sp::HeapString operator+(const sp::HeapString &lhs, const std::string &rhs) {
    sp::HeapString result = lhs;
    result += rhs;
    return result;
}

inline std::string operator+(const std::string &lhs, const sp::HeapString &rhs) {
    return lhs + std::string(rhs);
}

inline sp::HeapString operator+(const sp::HeapString &lhs, const std::string_view &rhs) {
    sp::HeapString result = lhs;
    result += rhs;
    return result;
}

inline sp::HeapString operator+(const std::string_view &lhs, const sp::HeapString &rhs) {
    sp::HeapString result = lhs;
    result += rhs;
    return result;
}

inline sp::HeapString operator+(const sp::HeapString &lhs, const char *rhs) {
    sp::HeapString result = lhs;
    result += rhs;
    return result;
}

inline sp::HeapString operator+(const char *lhs, const sp::HeapString &rhs) {
    sp::HeapString result = lhs;
    result += rhs;
    return result;
}

inline std::ostream &operator<<(std::ostream &out, const sp::HeapString &str) {
    return out << str.c_str();
}
