/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Common.hh"

#include "core/Logging.hh"

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
    [[noreturn]] void Abort(const string &message) {
        if (!message.empty()) Errorf("assertion failed: %s", message);
        os_break();
        throw std::runtime_error(message);
    }

    void DebugBreak() {
        os_break();
    }

    uint32 CeilToPowerOfTwo(uint32 v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return v;
    }

    uint32 Uint32Log2(uint32 v) {
        uint32 r = 0;
        while (v >>= 1)
            r++;
        return r;
    }

    uint64 Uint64Log2(uint64 v) {
        uint64 r = 0;
        while (v >>= 1)
            r++;
        return r;
    }

    float angle_t::degrees() const {
        return glm::degrees(radians_);
    }

    bool is_float(string_view str) {
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

    bool all_lower(const string &str) {
        return std::all_of(str.begin(), str.end(), [](unsigned char c) {
            return std::islower(c);
        });
    }

    namespace boost_replacements {
        bool starts_with(const string &str, const string &prefix) {
            return str.rfind(prefix, 0) == 0;
        }

        bool starts_with(const string_view &str, const string_view &prefix) {
            return str.rfind(prefix, 0) == 0;
        }

        bool ends_with(const string &str, const string &suffix) {
            if (str.length() >= suffix.length()) {
                return (str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
            } else {
                return false;
            }
        }

        string to_lower(string &str) {
            std::transform(str.begin(), str.end(), str.begin(), [](auto &ch) {
                return std::tolower(ch);
            });
            return str;
        }

        string to_upper(string &str) {
            std::transform(str.begin(), str.end(), str.begin(), [](auto &ch) {
                return std::toupper(ch);
            });
            return str;
        }

        string to_lower_copy(const string &str) {
            string out(str);
            std::transform(str.begin(), str.end(), out.begin(), [](auto &ch) {
                return std::tolower(ch);
            });
            return out;
        }

        string to_upper_copy(const string &str) {
            string out(str);
            std::transform(str.begin(), str.end(), out.begin(), [](auto &ch) {
                return std::toupper(ch);
            });
            return out;
        }

        string to_lower_copy(const string_view &str) {
            string out(str);
            std::transform(str.begin(), str.end(), out.begin(), [](auto &ch) {
                return std::tolower(ch);
            });
            return out;
        }

        string to_upper_copy(const string_view &str) {
            string out(str);
            std::transform(str.begin(), str.end(), out.begin(), [](auto &ch) {
                return std::toupper(ch);
            });
            return out;
        }

        bool iequals(const string &str1, const string &str2) {
            return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(), [](auto &a, auto &b) {
                return std::tolower(a) == std::tolower(b);
            });
        }

        void trim(string &str) {
            trim_right(str);
            trim_left(str);
        }

        void trim_left(string &str) {
            auto left = std::find_if(str.begin(), str.end(), [](char ch) {
                return !std::isspace(ch);
            });
            str.erase(str.begin(), left);
        }

        void trim_right(string &str) {
            auto right = std::find_if(str.rbegin(), str.rend(), [](char ch) {
                return !std::isspace(ch);
            }).base();
            str.erase(right, str.end());
        }
    } // namespace boost_replacements
} // namespace sp
