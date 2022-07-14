#pragma once

#include <array>
#include <bitset>

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

    template<class EnumType, typename BitsetT = std::bitset<static_cast<size_t>(EnumType::Count)>>
    class EnumFlags : public BitsetT {
    public:
        template<class... Args>
        EnumFlags(Args... args) {
            (BitsetT::set(static_cast<size_t>(args)), ...);
        }

        EnumFlags &set() {
            BitsetT::set();
            return *this;
        }

        EnumFlags &set(const EnumType &flag, bool value = true) {
            BitsetT::set(static_cast<size_t>(flag), value);
            return *this;
        }

        EnumFlags &reset() {
            BitsetT::reset();
            return *this;
        }

        EnumFlags &reset(const EnumType &flag) {
            BitsetT::reset(static_cast<size_t>(flag));
            return *this;
        }

        EnumFlags &flip(const EnumType &flag) {
            BitsetT::flip(static_cast<size_t>(flag));
            return *this;
        }

        constexpr BitsetT::reference operator[](const EnumType &flag) {
            return (*this)[static_cast<size_t>(flag)];
        }

        constexpr bool operator[](const EnumType &flag) const {
            return (*this)[static_cast<size_t>(flag)];
        }
    };
} // namespace sp
