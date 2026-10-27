/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "strayphotons/HeapString.hh"
#include "strayphotons/Logging.hh"

#include <array>
#include <cstring>
#include <limits>
#include <type_traits>

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

        constexpr InlineString() noexcept : ArrayT() {
            setSize(0);
        }

        constexpr InlineString(size_t count, CharT ch) noexcept : ArrayT() {
            resize(count, ch);
        }

        constexpr InlineString(const CharT *init) : ArrayT() {
            size_type count = init ? traits_type::length(init) : 0;
            Assert(count <= MaxSize, "InlineString overflow");
            if (count > 0) traits_type::copy(data(), init, count);
            setSize(count);
        }

        constexpr InlineString(const std::basic_string<CharT> &init) : ArrayT() {
            Assert(init.size() <= MaxSize, "InlineString overflow");
            traits_type::copy(data(), init.data(), init.size());
            setSize(init.size());
        }

        constexpr InlineString(const std::basic_string_view<CharT> &init) : ArrayT() {
            Assert(init.size() <= MaxSize, "InlineString overflow");
            traits_type::copy(data(), init.data(), init.size());
            setSize(init.size());
        }

        template<std::enable_if_t<std::is_same_v<CharT, char>, int> = 0>
        constexpr InlineString(const sp::HeapString &init) : ArrayT() {
            Assert(init.size() <= MaxSize, "InlineString overflow");
            traits_type::copy(data(), init.data(), init.size());
            setSize(init.size());
        }

        constexpr size_type size() {
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

        constexpr size_type size() const {
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

        constexpr size_type length() {
            return size();
        }

        constexpr size_type length() const {
            return size();
        }

        constexpr size_type capacity() const {
            return ArrayT::size() - 1;
        }

        static constexpr size_type max_size() {
            return MaxSize;
        }

        constexpr void resize(size_type newSize, CharT ch = CharT()) {
            Assert(newSize <= MaxSize, "InlineString overflow");
            auto oldSize = size();
            if (newSize < oldSize) {
                std::fill(begin() + newSize, end(), CharT());
            } else if (newSize > oldSize) {
                std::fill(begin() + oldSize, begin() + newSize, ch);
            }
            setSize(newSize);
        }

        constexpr void fill(const CharT &value) noexcept {
            std::fill(begin(), end(), value);
            setSize(value == CharT() ? 0 : MaxSize);
        }

        constexpr bool empty() const noexcept {
            return at(0) == CharT();
        }

        constexpr CharT &back() {
            return at(size() - 1);
        }

        constexpr const CharT *c_str() const {
            DebugAssertf(at(size()) == CharT(), "String missing null termination");
            return data();
        }

        constexpr std::basic_string<CharT> str() const {
            return std::basic_string<CharT>(data(), size());
        }

        constexpr void push_back(const CharT &value) {
            Assert(size() <= MaxSize, "InlineString overflow");
            at(size()) = value;
            setSize(size() + 1);
        }

        constexpr void pop_back() {
            Assert(!empty() && MaxSize > 0, "InlineString underflow");
            at(size() - 1) = CharT();
            setSize(size() - 1);
        }

        template<class... Args>
        constexpr CharT &emplace_back(Args &&...args) {
            Assert(size() + 1 <= MaxSize, "InlineString overflow");
            auto &ref = at(size());
            ref = CharT(std::forward<Args>(args)...);
            setSize(size() + 1);
            return ref;
        }

        constexpr void clear() noexcept {
            fill(CharT());
        }

        using ArrayT::begin;
        constexpr iterator end() {
            return begin() + size();
        }
        constexpr const_iterator end() const {
            return begin() + size();
        }

        using ArrayT::rend;
        constexpr reverse_iterator rbegin() {
            return std::reverse_iterator(end());
        }
        constexpr const_reverse_iterator rbegin() const {
            return std::reverse_iterator(end());
        }

        constexpr size_type find(std::basic_string_view<CharT> str, size_type pos = 0) const {
            return std::basic_string_view<CharT>(*this).find(str, pos);
        }

        constexpr size_type find(CharT ch, size_type pos = 0) const {
            return std::basic_string_view<CharT>(*this).find(ch, pos);
        }

        constexpr size_type rfind(std::basic_string_view<CharT> str, size_type pos = npos) const {
            return std::basic_string_view<CharT>(*this).rfind(str, pos);
        }

        constexpr size_type rfind(CharT ch, size_type pos = npos) const {
            return std::basic_string_view<CharT>(*this).rfind(ch, pos);
        }

        constexpr size_type find_first_of(std::basic_string_view<CharT> str, size_type pos = 0) const {
            return std::basic_string_view<CharT>(*this).find_first_of(str, pos);
        }

        constexpr size_type find_first_of(CharT ch, size_type pos = 0) const {
            return std::basic_string_view<CharT>(*this).find_first_of(ch, pos);
        }

        constexpr size_type find_last_of(std::basic_string_view<CharT> str, size_type pos = npos) const {
            return std::basic_string_view<CharT>(*this).find_last_of(str, pos);
        }

        constexpr size_type find_last_of(CharT ch, size_type pos = npos) const {
            return std::basic_string_view<CharT>(*this).find_last_of(ch, pos);
        }

        constexpr std::basic_string_view<CharT> substr(size_type pos = 0, size_type count = npos) const {
            return std::basic_string_view<CharT>(*this).substr(pos, count);
        }

        constexpr operator std::basic_string_view<CharT>() const {
            DebugAssertf(at(size()) == CharT(), "String missing null termination");
            return std::basic_string_view<CharT>(data(), size());
        }

        constexpr int compare(const InlineString<MaxSize, CharT, ArrayT> &other) const {
            return std::basic_string_view<CharT>(*this).compare(other);
        }

        constexpr int compare(const std::basic_string_view<CharT> &other) const {
            return std::basic_string_view<CharT>(*this).compare(other);
        }

        constexpr int compare(const CharT *other) const {
            return std::basic_string_view<CharT>(*this).compare(other);
        }

        constexpr bool operator==(const InlineString<MaxSize, CharT, ArrayT> &other) const {
            return std::basic_string_view<CharT>(*this) == std::basic_string_view<CharT>(other);
        }

        constexpr bool operator==(const CharT *other) const {
            return std::basic_string_view<CharT>(*this) == other;
        }

        template<typename StringViewLike>
        constexpr bool operator==(const StringViewLike &other) const {
            return std::basic_string_view<CharT>(*this) == other;
        }

        constexpr bool operator<(const InlineString<MaxSize, CharT, ArrayT> &other) const {
            return std::basic_string_view<CharT>(*this) < std::basic_string_view<CharT>(other);
        }

        template<typename StringViewLike>
        constexpr bool operator<(const StringViewLike &other) const {
            return std::basic_string_view<CharT>(*this) < other;
        }

        constexpr InlineString<MaxSize, CharT, ArrayT> &operator+=(const std::basic_string_view<CharT> &str) {
            size_t oldSize = size();
            Assert(oldSize + str.size() <= MaxSize, "InlineString overflow");
            traits_type::copy(data() + oldSize, str.data(), str.size());
            setSize(oldSize + str.size());
            return *this;
        }

        constexpr InlineString<MaxSize, CharT, ArrayT> &operator+=(CharT ch) {
            push_back(ch);
            return *this;
        }

    private:
        constexpr void setSize(size_type newSize) {
            DebugAssertf(newSize <= MaxSize, "InlineString overflow: %llu", newSize);
            if (newSize == 0) {
                // Leave an empty string as all-zeros so strcpy doesn't result in invalid strings
                ArrayT::back() = CharT();
            } else {
                ArrayT::back() = (std::make_unsigned_t<CharT>)(MaxSize - newSize);
            }
        }
    };
} // namespace sp

template<typename CharT, size_t MaxSize, typename ArrayT, size_t OtherMaxSize, typename OtherArrayT>
sp::InlineString<MaxSize, CharT, ArrayT> operator+(const sp::InlineString<MaxSize, CharT, ArrayT> &lhs,
    const sp::InlineString<OtherMaxSize, CharT, OtherArrayT> &rhs) {
    sp::InlineString<MaxSize, CharT, ArrayT> result = lhs;
    result += rhs;
    return result;
}

template<typename CharT, size_t MaxSize, typename ArrayT>
sp::InlineString<MaxSize, CharT, ArrayT> operator+(const sp::InlineString<MaxSize, CharT, ArrayT> &lhs,
    const std::basic_string<CharT> &rhs) {
    sp::InlineString<MaxSize, CharT, ArrayT> result = lhs;
    result += rhs;
    return result;
}

template<typename CharT, size_t MaxSize, typename ArrayT>
std::basic_string<CharT> operator+(const std::basic_string<CharT> &lhs,
    const sp::InlineString<MaxSize, CharT, ArrayT> &rhs) {
    return lhs + std::basic_string<CharT>(rhs);
}

template<typename CharT, size_t MaxSize, typename ArrayT>
sp::InlineString<MaxSize, CharT, ArrayT> operator+(const sp::InlineString<MaxSize, CharT, ArrayT> &lhs,
    const std::basic_string_view<CharT> &rhs) {
    sp::InlineString<MaxSize, CharT, ArrayT> result = lhs;
    result += rhs;
    return result;
}

template<typename CharT, size_t MaxSize, typename ArrayT>
std::basic_string_view<CharT> operator+(const std::basic_string_view<CharT> &lhs,
    const sp::InlineString<MaxSize, CharT, ArrayT> &rhs) {
    return lhs + std::basic_string<CharT>(rhs);
}

template<typename CharT, size_t MaxSize, typename ArrayT>
sp::InlineString<MaxSize, CharT, ArrayT> operator+(const sp::InlineString<MaxSize, CharT, ArrayT> &lhs,
    const char *rhs) {
    sp::InlineString<MaxSize, CharT, ArrayT> result = lhs;
    result += rhs;
    return result;
}

template<typename CharT, size_t MaxSize, typename ArrayT>
std::basic_string<CharT> operator+(const char *lhs, const sp::InlineString<MaxSize, CharT, ArrayT> &rhs) {
    return lhs + std::basic_string<CharT>(rhs);
}

template<typename CharT, size_t MaxSize, typename ArrayT>
std::ostream &operator<<(std::ostream &out, const sp::InlineString<MaxSize, CharT, ArrayT> &str) {
    return out << str.c_str();
}
