#pragma once

#include <memory>
using std::make_shared;
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;

#include <vector>
using std::vector;

#include <array>
#include <string>
using std::string;

#include <stdexcept>
using std::invalid_argument;
using std::runtime_error;

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

namespace sp {
    void Assert(bool condition, const string &message);
    void DebugBreak();

    uint32 CeilToPowerOfTwo(uint32 v);
    uint32 Uint32Log2(uint32 v);

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
