#pragma once

#include <memory>
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;

#include <vector>
using std::vector;

#include <array>
#include <string>
#include <string_view>
using std::string;
using std::string_view;

#include <chrono>
typedef std::chrono::steady_clock chrono_clock;

#include <algorithm>
#include <glm/glm.hpp>

typedef unsigned char uint8;
typedef signed char int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;

#define Assert(condition, message) \
    if (!(condition)) ::sp::Abort(message);

#ifdef SP_DEBUG
    #define DebugAssert(condition, message) Assert(condition, message)
#else
    #define DebugAssert(condition, message)
#endif

namespace sp {
    [[noreturn]] void Abort(const string &message = "");
    void DebugBreak();

    uint32 CeilToPowerOfTwo(uint32 v);
    uint32 Uint32Log2(uint32 v);
    uint64 Uint64Log2(uint64 v);

    template<typename T>
    void ForEachBit(uint32 value, const T &func) {
        uint32 bit = 1, index = 0;
        while (value) {
            if (value & bit) func(index);
            value &= ~bit;
            bit <<= 1;
            index++;
        }
    }

    class NonCopyable {
    public:
        NonCopyable &operator=(const NonCopyable &) = delete;
        NonCopyable(const NonCopyable &) = delete;
        NonCopyable() = default;
    };

    typedef std::array<uint64, 2> Hash128;
    typedef uint64 Hash64;

    class angle_t {
    public:
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

        operator glm::vec3() const {
            return color;
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

        bool operator==(const color_t &other) const {
            return color == other.color;
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

    bool is_float(const string &str);

    namespace boost_replacements {
        bool starts_with(const string &str, const string &prefix);
        bool starts_with(const string_view &str, const string_view &prefix);
        bool ends_with(const string &str, const string &suffix);
        string to_lower(string &str);
        string to_upper(string &str);
        string to_lower_copy(const string &str);
        string to_upper_copy(const string &str);
        void trim(string &str);
        void trim_left(string &str);
        void trim_right(string &str);
    } // namespace boost_replacements
    using namespace boost_replacements;

    struct ClockTimer {
        chrono_clock::time_point start = chrono_clock::now();

        chrono_clock::duration Duration() const {
            return chrono_clock::now() - start;
        }
    };

} // namespace sp
