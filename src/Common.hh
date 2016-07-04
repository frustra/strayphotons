#pragma once

#define _DEBUG

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
	void Assert(bool condition);
	void Assert(bool condition, string message);

	uint32 CeilToPowerOfTwo(uint32 v);
	uint32 Uint32Log2(uint32 v);

	class NonCopyable
	{
	public:
		NonCopyable &operator=(const NonCopyable &) = delete;
		NonCopyable(const NonCopyable &) = delete;
		NonCopyable() = default;
	};

	/**
	 * Functor that can be instantiated as a hash function for an enum class.
	 * Useful for when you want to use an enum class as an unordered_map key
	 */
	struct EnumHash
	{
		template <typename T>
		std::size_t operator()(T t) const
		{
			return static_cast<std::size_t>(t);
		}
	};
}
