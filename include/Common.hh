#ifndef SP_SHARED_H
#define SP_SHARED_H

#include <memory>
using std::make_shared;
using std::shared_ptr;
using std::weak_ptr;

#include <vector>
using std::vector;

#include <string>
using std::string;

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
}

#endif