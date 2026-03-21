/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "strayphotons/cpp/Utility.hh"

#include <algorithm>
#include <cctype>

#ifdef _WIN32
    #include <intrin.h>
    #define os_break() __debugbreak()
#elif defined(__arm__) || defined(__aarch64__)
    #define os_break()
#else
    #define os_break() asm("int $3")
#endif

namespace sp {
    [[noreturn]] void Abort() {
        os_break();
        throw std::runtime_error("sp::Abort() called");
    }

    float angle_t::degrees() const {
        return glm::degrees(radians_);
    }

    bool is_float(std::string_view str) {
        if (str.empty()) return false;
        int state = 0;
        // States:
        // 0: Start of string
        // 1: After negative sign
        // 2: After first digit
        // 3: After decimal place
        if (!std::all_of(str.begin(), str.end(), [&](char ch) {
                if (ch == '-') {
                    if (state != 0) return false;
                    state = 1;
                    return true;
                } else if (std::isdigit(ch)) {
                    if (state < 2) state = 2;
                    return true;
                } else if (ch == '.') {
                    if (state == 3) return false;
                    state = 3;
                    return true;
                }
                return false;
            })) {
            return false;
        }
        if (state == 3 && str.length() == 1) return false;
        return state > 1;
    }

    bool all_lower(const std::string &str) {
        return std::all_of(str.begin(), str.end(), [](unsigned char c) {
            return std::islower(c);
        });
    }

    namespace boost_replacements {
        bool starts_with(const std::string &str, const std::string &prefix) {
            return str.rfind(prefix, 0) == 0;
        }

        bool starts_with(const std::string_view &str, const std::string_view &prefix) {
            return str.rfind(prefix, 0) == 0;
        }

        bool ends_with(const std::string &str, const std::string &suffix) {
            if (str.length() >= suffix.length()) {
                return (str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
            } else {
                return false;
            }
        }

        bool ends_with(const std::string_view &str, const std::string_view &suffix) {
            if (str.length() >= suffix.length()) {
                return (str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
            } else {
                return false;
            }
        }

        std::string to_lower(std::string &str) {
            std::transform(str.begin(), str.end(), str.begin(), [](auto &ch) {
                return std::tolower(ch);
            });
            return str;
        }

        std::string to_upper(std::string &str) {
            std::transform(str.begin(), str.end(), str.begin(), [](auto &ch) {
                return std::toupper(ch);
            });
            return str;
        }

        std::string to_lower_copy(const std::string &str) {
            std::string out(str);
            std::transform(str.begin(), str.end(), out.begin(), [](auto &ch) {
                return std::tolower(ch);
            });
            return out;
        }

        std::string to_upper_copy(const std::string &str) {
            std::string out(str);
            std::transform(str.begin(), str.end(), out.begin(), [](auto &ch) {
                return std::toupper(ch);
            });
            return out;
        }

        std::string to_lower_copy(const std::string_view &str) {
            std::string out(str);
            std::transform(str.begin(), str.end(), out.begin(), [](auto &ch) {
                return std::tolower(ch);
            });
            return out;
        }

        std::string to_upper_copy(const std::string_view &str) {
            std::string out(str);
            std::transform(str.begin(), str.end(), out.begin(), [](auto &ch) {
                return std::toupper(ch);
            });
            return out;
        }

        bool iequals(const std::string &str1, const std::string &str2) {
            return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(), [](auto &a, auto &b) {
                return std::tolower(a) == std::tolower(b);
            });
        }

        bool iequals(const std::string_view &str1, const std::string_view &str2) {
            return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(), [](auto &a, auto &b) {
                return std::tolower(a) == std::tolower(b);
            });
        }

        void trim(std::string &str) {
            trim_right(str);
            trim_left(str);
        }

        void trim_left(std::string &str) {
            auto left = std::find_if(str.begin(), str.end(), [](char ch) {
                return !std::isspace(ch);
            });
            str.erase(str.begin(), left);
        }

        void trim_right(std::string &str) {
            auto right = std::find_if(str.rbegin(), str.rend(), [](char ch) {
                return !std::isspace(ch);
            }).base();
            str.erase(right, str.end());
        }

        std::string_view trim(const std::string_view &str) {
            return trim_right(trim_left(str));
        }

        std::string_view trim_left(const std::string_view &str) {
            auto left = std::find_if(str.begin(), str.end(), [](char ch) {
                return !std::isspace(ch);
            });
            return str.substr(left - str.begin());
        }

        std::string_view trim_right(const std::string_view &str) {
            auto right = std::find_if(str.rbegin(), str.rend(), [](char ch) {
                return !std::isspace(ch);
            }).base();
            return str.substr(0, right - str.begin());
        }
    } // namespace boost_replacements
} // namespace sp
