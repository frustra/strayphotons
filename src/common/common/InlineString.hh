/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Logging.hh"

#include <array>
#include <cstring>
#include <limits>
#include <ostream>

namespace sp {
    template<size_t MaxSize, typename CharT = char, typename ArrayT = std::array<CharT, MaxSize + 1>>
    class InlineString : private ArrayT {
    public:
        using traits_type = std::char_traits<CharT>;
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

        static_assert(MaxSize <= std::numeric_limits<std::make_unsigned_t<CharT>>::max(),
            "InlineString size too large");

        static constexpr size_type npos = std::basic_string<CharT>::npos;

        using ArrayT::at;
        using ArrayT::data;
        using ArrayT::front;
        using ArrayT::operator[];
        using ArrayT::ArrayT;

        InlineString() : ArrayT({}) {
            setSize(0);
        }

        InlineString(size_t count, CharT ch) : ArrayT({}) {
            resize(count, ch);
        }

        InlineString(const CharT *init) : ArrayT({}) {
            size_type count = init ? traits_type::length(init) : 0;
            Assert(count <= MaxSize, "InlineString overflow");
            if (count > 0) traits_type::copy(data(), init, count);
            setSize(count);
        }

        InlineString(const std::basic_string<CharT> &init) : ArrayT({}) {
            Assert(init.size() <= MaxSize, "InlineString overflow");
            traits_type::copy(data(), init.data(), init.size());
            setSize(init.size());
        }

        InlineString(const std::basic_string_view<CharT> &init) : ArrayT({}) {
            Assert(init.size() <= MaxSize, "InlineString overflow");
            traits_type::copy(data(), init.data(), init.size());
            setSize(init.size());
        }

        size_type size() {
            size_type unusedCount = (size_type)(std::make_unsigned_t<CharT>)(ArrayT::back());
            DebugAssertf(unusedCount <= MaxSize, "Corrupted InlineString size: %llu/%llu", unusedCount, MaxSize);
            DebugAssertf(at(MaxSize - unusedCount) == CharT(),
                "Corrupted InlineString null terminator: %llu/%llu, %.*s",
                unusedCount,
                MaxSize,
                MaxSize,
                data());
            if (unusedCount < MaxSize && at(MaxSize - unusedCount - 1) == CharT()) {
                // String has zero padding, recalculate length
                ArrayT::back() = CharT();
                size_type len = traits_type::length(data());
                setSize(len);
                return len;
            }
            return MaxSize - unusedCount;
        }

        size_type size() const {
            size_type unusedCount = (size_type)(std::make_unsigned_t<CharT>)(ArrayT::back());
            DebugAssertf(unusedCount <= MaxSize, "Corrupted InlineString size: %llu/%llu", unusedCount, MaxSize);
            DebugAssertf(at(MaxSize - unusedCount) == CharT(),
                "Corrupted InlineString null terminator: %llu/%llu, %.*s",
                unusedCount,
                MaxSize,
                MaxSize,
                data());
            if (unusedCount < MaxSize && at(MaxSize - unusedCount - 1) == CharT()) {
                // String has zero padding, recalculate length
                return traits_type::length(data());
            }
            return MaxSize - unusedCount;
        }

        size_type capacity() const {
            return ArrayT::size() - 1;
        }

        static constexpr size_type max_size() {
            return MaxSize;
        }

        void resize(size_type newSize, CharT ch = CharT()) {
            Assert(newSize <= MaxSize, "InlineString overflow");
            if (newSize < size()) {
                std::fill(begin() + newSize, end(), ch);
            }
            setSize(newSize);
        }

        void fill(const CharT &value) {
            std::fill(begin(), end(), value);
            setSize(value == CharT() ? 0 : MaxSize);
        }

        bool empty() const {
            return at(0) == CharT();
        }

        CharT &back() {
            return at(size() - 1);
        }

        const CharT *c_str() const {
            DebugAssertf(at(size()) == CharT(), "String missing null termination");
            return data();
        }

        std::basic_string<CharT> str() const {
            return std::basic_string<CharT>(data(), size());
        }

        void push_back(const CharT &value) {
            Assert(size() <= MaxSize, "InlineString overflow");
            at(size()) = value;
            setSize(size() + 1);
        }

        void pop_back() {
            Assert(!empty() && MaxSize > 0, "InlineString underflow");
            at(size() - 1) = CharT();
            setSize(size() - 1);
        }

        template<class... Args>
        CharT &emplace_back(Args &&...args) {
            Assert(size() + 1 <= MaxSize, "InlineString overflow");
            auto &ref = at(size());
            ref = CharT(std::forward<Args>(args)...);
            setSize(size() + 1);
            return ref;
        }

        void clear() {
            fill(CharT());
        }

        using ArrayT::begin;
        iterator end() {
            return begin() + size();
        }
        const_iterator end() const {
            return begin() + size();
        }

        using ArrayT::rend;
        reverse_iterator rbegin() {
            return rend() - size();
        }
        const_reverse_iterator rbegin() const {
            return rend() - size();
        }

        size_type find(const std::basic_string<CharT> &str, size_type pos = 0) const {
            return std::basic_string_view<CharT>(*this).find(str, pos);
        }

        size_type find(CharT ch, size_type pos = 0) const {
            return std::basic_string_view<CharT>(*this).find(ch, pos);
        }

        size_type rfind(const std::basic_string<CharT> &str, size_type pos = npos) const {
            return std::basic_string_view<CharT>(*this).rfind(str, pos);
        }

        size_type rfind(CharT ch, size_type pos = npos) const {
            return std::basic_string_view<CharT>(*this).rfind(ch, pos);
        }

        size_type find_first_of(const std::basic_string<CharT> &str, size_type pos = 0) const {
            return std::basic_string_view<CharT>(*this).find_first_of(str, pos);
        }

        size_type find_first_of(CharT ch, size_type pos = 0) const {
            return std::basic_string_view<CharT>(*this).find_first_of(ch, pos);
        }

        size_type find_last_of(const std::basic_string<CharT> &str, size_type pos = npos) const {
            return std::basic_string_view<CharT>(*this).find_last_of(str, pos);
        }

        size_type find_last_of(CharT ch, size_type pos = npos) const {
            return std::basic_string_view<CharT>(*this).find_last_of(ch, pos);
        }

        std::basic_string_view<CharT> substr(size_type pos = 0, size_type count = npos) const {
            return std::basic_string_view<CharT>(*this).substr(pos, count);
        }

        operator std::basic_string_view<CharT>() const {
            DebugAssertf(at(size()) == CharT(), "String missing null termination");
            return std::basic_string_view<CharT>(data(), size());
        }

        int compare(const InlineString<MaxSize, CharT> &other) const {
            return std::basic_string_view<CharT>(*this).compare(other);
        }

        int compare(const std::basic_string_view<CharT> &other) const {
            return std::basic_string_view<CharT>(*this).compare(other);
        }

        int compare(const CharT *other) const {
            return std::basic_string_view<CharT>(*this).compare(other);
        }

        bool operator==(const InlineString<MaxSize, CharT> &other) const {
            return std::basic_string_view<CharT>(*this) == std::basic_string_view<CharT>(other);
        }

        bool operator==(const CharT *other) const {
            return std::basic_string_view<CharT>(*this) == other;
        }

        template<typename StringViewLike>
        bool operator==(const StringViewLike &other) const {
            return std::basic_string_view<CharT>(*this) == other;
        }

        bool operator<(const InlineString<MaxSize, CharT> &other) const {
            return std::basic_string_view<CharT>(*this) < std::basic_string_view<CharT>(other);
        }

        template<typename StringViewLike>
        bool operator<(const StringViewLike &other) const {
            return std::basic_string_view<CharT>(*this) < other;
        }

        InlineString<MaxSize, CharT> &operator+=(const std::basic_string<CharT> &str) {
            Assert(size() + str.size() <= MaxSize, "InlineString overflow");
            traits_type::copy(data() + size(), str.data(), str.size());
            setSize(size() + str.size());
            return *this;
        }

        InlineString<MaxSize, CharT> &operator+=(const CharT *str) {
            size_type count = str ? traits_type::length(str) : 0;
            Assert(size() + count <= MaxSize, "InlineString overflow");
            if (count > 0) {
                traits_type::copy(data() + size(), str, count);
                setSize(size() + count);
            }
            return *this;
        }

        InlineString<MaxSize, CharT> &operator+=(CharT ch) {
            push_back(ch);
            return *this;
        }

    private:
        void setSize(size_type newSize) {
            DebugAssertf(newSize <= MaxSize, "InlineString overflow: %llu", newSize);
            ArrayT::back() = (std::make_unsigned_t<CharT>)(MaxSize - newSize);
        }
    };
} // namespace sp

template<typename CharT, size_t MaxSize>
std::basic_string<CharT> operator+(const sp::InlineString<MaxSize, CharT> &lhs, const std::basic_string<CharT> &rhs) {
    return std::basic_string<CharT>(lhs) + rhs;
}

template<typename CharT, size_t MaxSize>
std::basic_string<CharT> operator+(const std::basic_string<CharT> &lhs, const sp::InlineString<MaxSize, CharT> &rhs) {
    return lhs + std::basic_string<CharT>(rhs);
}

template<typename CharT, size_t MaxSize>
std::basic_string<CharT> operator+(const sp::InlineString<MaxSize, CharT> &lhs, const char *rhs) {
    return std::basic_string<CharT>(lhs) + rhs;
}
