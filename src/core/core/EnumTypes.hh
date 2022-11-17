#pragma once

#include <array>
#include <bitset>
#include <magic_enum.hpp>
#include <type_traits>

namespace sp {
    template<typename T, class EnumType, typename ArrayT = std::array<T, magic_enum::enum_count<EnumType>()>>
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
        template<typename E>
        constexpr bool is_flags_enum() noexcept {
            if constexpr (!std::is_enum_v<E>) {
                return false;
            } else if constexpr (magic_enum::detail::has_is_flags<E>::value) {
                return magic_enum::customize::enum_range<E>::is_flags;
            } else {
                return false;
            }
        }

        template<typename E, std::enable_if_t<is_flags_enum<E>(), int> = 0>
        constexpr E operator~(E rhs) noexcept {
            magic_enum::underlying_type_t<E> mask = 0;
            for (auto &val : magic_enum::enum_values<E>()) {
                mask |= static_cast<magic_enum::underlying_type_t<E>>(val);
            }
            auto flipped = ~static_cast<magic_enum::underlying_type_t<E>>(rhs);
            return static_cast<E>(flipped & mask);
        }

        template<typename E, std::enable_if_t<is_flags_enum<E>(), int> = 0>
        constexpr E operator|(E lhs, E rhs) noexcept {
            return static_cast<E>(static_cast<magic_enum::underlying_type_t<E>>(lhs) |
                                  static_cast<magic_enum::underlying_type_t<E>>(rhs));
        }

        template<typename E, std::enable_if_t<is_flags_enum<E>(), int> = 0>
        constexpr E operator&(E lhs, E rhs) noexcept {
            return static_cast<E>(static_cast<magic_enum::underlying_type_t<E>>(lhs) &
                                  static_cast<magic_enum::underlying_type_t<E>>(rhs));
        }

        template<typename E, std::enable_if_t<is_flags_enum<E>(), int> = 0>
        constexpr E operator^(E lhs, E rhs) noexcept {
            return static_cast<E>(static_cast<magic_enum::underlying_type_t<E>>(lhs) ^
                                  static_cast<magic_enum::underlying_type_t<E>>(rhs));
        }

        template<typename E, std::enable_if_t<is_flags_enum<E>(), int> = 0>
        constexpr E &operator|=(E &lhs, E rhs) noexcept {
            return lhs = (lhs | rhs);
        }

        template<typename E, std::enable_if_t<is_flags_enum<E>(), int> = 0>
        constexpr E &operator&=(E &lhs, E rhs) noexcept {
            return lhs = (lhs & rhs);
        }

        template<typename E, std::enable_if_t<is_flags_enum<E>(), int> = 0>
        constexpr E &operator^=(E &lhs, E rhs) noexcept {
            return lhs = (lhs ^ rhs);
        }

        template<typename E, std::enable_if_t<is_flags_enum<E>(), int> = 0>
        constexpr bool operator!(E rhs) noexcept {
            return !static_cast<magic_enum::underlying_type_t<E>>(rhs);
        }
    } // namespace enum_flag_operators
} // namespace sp

using namespace sp::enum_flag_operators;
