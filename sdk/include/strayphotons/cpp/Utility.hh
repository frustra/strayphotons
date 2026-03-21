/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <robin_hood.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

typedef std::chrono::steady_clock chrono_clock;

namespace sp {
    [[noreturn]] void Abort();

    template<typename T>
    T CeilToPowerOfTwo(T v) {
        static_assert(std::is_unsigned<T>(), "CeilToPowerOfTwo expects unsigned integer types");
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        if constexpr (sizeof(T) > 1) {
            v |= v >> 8;
        }
        if constexpr (sizeof(T) > 2) {
            v |= v >> 16;
        }
        if constexpr (sizeof(T) > 4) {
            v |= v >> 32;
        }
        v++;
        return v;
    }

    template<typename T>
    T UintLog2(T v) {
        static_assert(std::is_unsigned<T>(), "UintLog2 expects unsigned integer types");
        T r = 0;
        while (v >>= 1)
            r++;
        return r;
    }

    template<typename T>
    void ForEachBit(uint32_t value, const T &func) {
        uint32_t bit = 1, index = 0;
        while (value) {
            if (value & bit) func(index);
            value &= ~bit;
            bit <<= 1;
            index++;
        }
    }

    class NonCopyable {
    public:
        NonCopyable() = default;
        NonCopyable(const NonCopyable &) = delete;
        NonCopyable &operator=(const NonCopyable &) = delete;
    };

    class NonMoveable : public NonCopyable {
    public:
        NonMoveable() = default;
        NonMoveable(NonMoveable &&) = delete;
        NonMoveable &operator=(NonMoveable &&) = delete;
    };

    typedef std::array<uint64_t, 2> Hash128;
    typedef uint64_t Hash64;

    class angle_t {
    public:
        angle_t() : radians_(0.0f) {}
        angle_t(const float &angle) : radians_(angle) {}

        operator float() const {
            return radians_;
        }

        const float &radians() const {
            return radians_;
        }

        float &radians() {
            return radians_;
        }

        float degrees() const;

    private:
        float radians_;
    };

    struct color_t {
        glm::vec3 color;

        color_t() : color(1) {}
        color_t(const glm::vec3 &color) : color(color) {}

        typedef glm::vec3::value_type value_type;

        operator glm::vec3() const {
            return color;
        }

        const float &operator[](size_t i) const {
            return color[i];
        }

        float &operator[](size_t i) {
            return color[i];
        }

        color_t operator*(const color_t &other) const {
            return color * other.color;
        }

        color_t operator*(const float &multiplier) const {
            return color * multiplier;
        }

        color_t &operator*=(const color_t &other) {
            color *= other.color;
            return *this;
        }

        color_t &operator+=(const color_t &other) {
            color += other.color;
            return *this;
        }

        color_t operator+(const color_t &other) const {
            return color + other.color;
        }

        bool operator==(const color_t &other) const {
            return color == other.color;
        }

        static int length() {
            return 3;
        }
    };

    struct color_alpha_t {
        glm::vec4 color;

        color_alpha_t() : color(1) {}
        color_alpha_t(const glm::vec3 &rgb) : color(rgb, 1) {}
        color_alpha_t(const glm::vec4 &rgba) : color(rgba) {}

        typedef glm::vec4::value_type value_type;

        operator glm::vec4() const {
            return color;
        }

        const float &operator[](size_t i) const {
            return color[i];
        }

        float &operator[](size_t i) {
            return color[i];
        }

        bool operator==(const color_alpha_t &other) const {
            return color == other.color;
        }

        static int length() {
            return 4;
        }
    };

    template<typename T, typename V>
    inline void erase(T &vec, const V &val) {
        vec.erase(std::remove(vec.begin(), vec.end(), val), vec.end());
    }

    template<typename T, typename Func>
    inline void erase_if(T &vec, Func f) {
        vec.erase(std::remove_if(vec.begin(), vec.end(), f), vec.end());
    }

    template<typename T, typename V>
    inline bool contains(const T &vec, const V &val) {
        return std::find(vec.begin(), vec.end(), val) != vec.end();
    }

    template<size_t, typename, typename>
    class InlineString;
    class HeapString;
    template<typename, size_t, typename>
    class InlineVector;
    template<typename T>
    class HeapVector;

    template<typename T>
    struct is_vector : std::false_type {};
    template<typename T>
    struct is_vector<std::vector<T>> : std::true_type {};

    template<typename T>
    struct is_inline_vector : std::false_type {};
    template<typename T, size_t MaxSize, typename ArrayT>
    struct is_inline_vector<InlineVector<T, MaxSize, ArrayT>> : std::true_type {};

    template<typename T>
    struct is_heap_vector : std::false_type {};
    template<typename T>
    struct is_heap_vector<HeapVector<T>> : std::true_type {};

    template<typename T>
    struct is_inline_string : std::false_type {};
    template<size_t MaxSize, typename CharT, typename ArrayT>
    struct is_inline_string<InlineString<MaxSize, CharT, ArrayT>> : std::true_type {};

    template<typename T>
    struct is_heap_string : std::false_type {};
    template<>
    struct is_heap_string<HeapString> : std::true_type {};

    template<typename T>
    struct is_array : std::false_type {};
    template<typename T, size_t N>
    struct is_array<std::array<T, N>> : std::true_type {};

    template<typename T>
    struct is_pair : std::false_type {};
    template<typename A, typename B>
    struct is_pair<std::pair<A, B>> : std::true_type {};

    template<typename T>
    struct is_unordered_flat_map : std::false_type {};
    template<typename K, typename V, typename H, typename E>
    struct is_unordered_flat_map<robin_hood::unordered_flat_map<K, V, H, E>> : std::true_type {};
    template<typename T>
    struct is_unordered_node_map : std::false_type {};
    template<typename K, typename V, typename H, typename E>
    struct is_unordered_node_map<robin_hood::unordered_node_map<K, V, H, E>> : std::true_type {};

    template<typename T>
    struct is_consistent_size : std::true_type {};
    template<>
    struct is_consistent_size<std::string> : std::false_type {};
    template<typename T>
    struct is_consistent_size<std::vector<T>> : std::false_type {};
    template<typename K, typename V, typename H, typename E>
    struct is_consistent_size<robin_hood::unordered_flat_map<K, V, H, E>> : std::false_type {};
    template<typename K, typename V, typename H, typename E>
    struct is_consistent_size<robin_hood::unordered_node_map<K, V, H, E>> : std::false_type {};

    template<typename T>
    struct is_optional : std::false_type {};
    template<typename T>
    struct is_optional<std::optional<T>> : std::true_type {};

    template<typename T>
    struct is_variant : std::false_type {};
    template<typename... Tn>
    struct is_variant<std::variant<Tn...>> : std::true_type {};

    template<typename T>
    struct is_glm_vec : std::false_type {};
    template<glm::length_t L, typename T, glm::qualifier Q>
    struct is_glm_vec<glm::vec<L, T, Q>> : std::true_type {};

    bool is_float(std::string_view str);
    bool all_lower(const std::string &str);

    struct float16_t {
        uint16_t value;

        float16_t() : value(0) {}
        float16_t(const uint16_t &value) : value(value) {}

        float16_t(const float &value_) {
            if (value_ == 0.0f) {
                value = 0u;
            } else {
                auto x = *reinterpret_cast<const uint32_t *>(&value_);
                value = ((x >> 16) & 0x8000) | ((((x & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) |
                        ((x >> 13) & 0x03ff);
            }
        }

        operator uint16_t() const {
            return value;
        }
    };

    namespace boost_replacements {
        bool starts_with(const std::string &str, const std::string &prefix);
        bool starts_with(const std::string_view &str, const std::string_view &prefix);
        bool ends_with(const std::string &str, const std::string &suffix);
        bool ends_with(const std::string_view &str, const std::string_view &suffix);
        std::string to_lower(std::string &str);
        std::string to_upper(std::string &str);
        std::string to_lower_copy(const std::string &str);
        std::string to_upper_copy(const std::string &str);
        std::string to_lower_copy(const std::string_view &str);
        std::string to_upper_copy(const std::string_view &str);
        bool iequals(const std::string &str1, const std::string &str2);
        bool iequals(const std::string_view &str1, const std::string_view &str2);
        void trim(std::string &str);
        void trim_left(std::string &str);
        void trim_right(std::string &str);
        std::string_view trim(const std::string_view &str);
        std::string_view trim_left(const std::string_view &str);
        std::string_view trim_right(const std::string_view &str);
    } // namespace boost_replacements
    using namespace boost_replacements;

} // namespace sp
