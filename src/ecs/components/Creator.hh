#pragma once

#include "Common.hh"

#include <ecs/Ecs.hh>

namespace ecs {
	enum class Creator { GAME_LOGIC };

	static Component<Creator> ComponentCreator("creator");
} // namespace ecs