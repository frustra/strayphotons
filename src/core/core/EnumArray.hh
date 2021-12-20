#pragma once

#include <array>

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
} // namespace sp
