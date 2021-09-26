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
using std::string;

#include <chrono>
typedef std::chrono::steady_clock chrono_clock;

typedef unsigned char uint8;
typedef signed char int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;

#define Assert(condition, message)                                                                                     \
    if (!(condition)) ::sp::Abort(message);

namespace sp {
    [[noreturn]] void Abort(const string &message);
    void DebugBreak();

    uint32 CeilToPowerOfTwo(uint32 v);
    uint32 Uint32Log2(uint32 v);

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

    // Boost replacement functions
    bool starts_with(const string &str, const string &prefix);
    bool ends_with(const string &str, const string &suffix);
    string to_lower(string &str);
    string to_upper(string &str);
    string to_lower_copy(const string &str);
    string to_upper_copy(const string &str);
    void trim(string &str);
    void trim_left(string &str);
    void trim_right(string &str);
} // namespace sp
