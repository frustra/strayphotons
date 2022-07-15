#pragma once

#include <array>
#include <bitset>
#include <magic_enum.hpp>
#include <type_traits>

namespace sp {
    template<typename T, class EnumType, typename ArrayT = std::array<T, static_cast<size_t>(EnumType::Count)>>
    class EnumArray : public ArrayT {
    public:
        constexpr T &operator[](const EnumType &e) {
            return ArrayT::at(static_cast<size_t>(e));
        }

        constexpr const T &operator[](const EnumType &e) const {
            return ArrayT::at(static_cast<size_t>(e));
        }
    };

    namespace enum_flag_operators {
        template<typename E, magic_enum::detail::enable_if_t<E, int> = 0>
        constexpr E operator~(E rhs) noexcept {
            magic_enum::underlying_type_t<E> mask = 0;
            for (auto &val : magic_enum::enum_values<E>()) {
                mask |= static_cast<magic_enum::underlying_type_t<E>>(val);
            }
            auto flipped = ~static_cast<magic_enum::underlying_type_t<E>>(rhs);
            return static_cast<E>(flipped & mask);
        }

        template<typename E, magic_enum::detail::enable_if_t<E, int> = 0>
        constexpr E operator|(E lhs, E rhs) noexcept {
            return static_cast<E>(static_cast<magic_enum::underlying_type_t<E>>(lhs) |
                                  static_cast<magic_enum::underlying_type_t<E>>(rhs));
        }

        template<typename E, magic_enum::detail::enable_if_t<E, int> = 0>
        constexpr E operator&(E lhs, E rhs) noexcept {
            return static_cast<E>(static_cast<magic_enum::underlying_type_t<E>>(lhs) &
                                  static_cast<magic_enum::underlying_type_t<E>>(rhs));
        }

        template<typename E, magic_enum::detail::enable_if_t<E, int> = 0>
        constexpr E operator^(E lhs, E rhs) noexcept {
            return static_cast<E>(static_cast<magic_enum::underlying_type_t<E>>(lhs) ^
                                  static_cast<magic_enum::underlying_type_t<E>>(rhs));
        }

        template<typename E, magic_enum::detail::enable_if_t<E, int> = 0>
        constexpr E &operator|=(E &lhs, E rhs) noexcept {
            return lhs = (lhs | rhs);
        }

        template<typename E, magic_enum::detail::enable_if_t<E, int> = 0>
        constexpr E &operator&=(E &lhs, E rhs) noexcept {
            return lhs = (lhs & rhs);
        }

        template<typename E, magic_enum::detail::enable_if_t<E, int> = 0>
        constexpr E &operator^=(E &lhs, E rhs) noexcept {
            return lhs = (lhs ^ rhs);
        }
    } // namespace enum_flag_operators
} // namespace sp

using namespace sp::enum_flag_operators;
