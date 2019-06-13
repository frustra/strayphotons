#pragma once

#include <memory>
using std::make_shared;
using std::unique_ptr;
using std::shared_ptr;
using std::weak_ptr;

#include <vector>
using std::vector;

#include <string>
using std::string;

#include <stdexcept>
using std::runtime_error;
using std::invalid_argument;

typedef unsigned char uint8;
typedef signed char int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;

namespace sp
{
	void Assert(bool condition, const string &message);

	void DebugBreak();

	uint32 CeilToPowerOfTwo(uint32 v);
	uint32 Uint32Log2(uint32 v);

	class NonCopyable
	{
	public:
		NonCopyable &operator=(const NonCopyable &) = delete;
		NonCopyable(const NonCopyable &) = delete;
		NonCopyable() = default;
	};

	// Boost replacement functions
	template<typename T>
	void hash_combine(std::size_t &seed, const T &val)
	{
		seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	bool starts_with(const string &str, const string &prefix);
	string to_lower_copy(const string &str);
	void trim(string &str);
	void trim_left(string &str);
	void trim_right(string &str);
}
