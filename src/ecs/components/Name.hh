#pragma once

#include "Common.hh"

#include <glm/glm.hpp>

namespace ecs
{
	class Name
	{
	public:
		Name(const string &name) : name(name) {}
		string name;
	};
}
