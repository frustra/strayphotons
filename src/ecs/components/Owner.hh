#pragma once

#include "Common.hh"

#include <ecs/Ecs.hh>

namespace ecs {
	enum class OwnerType { GAME_LOGIC };
	struct Owner {
		Owner(){};
		Owner(OwnerType type) : type(type){};
		~Owner(){};
		inline bool operator==(const Owner &other) const {
			return type == other.type;
		}

		size_t id;
		OwnerType type;
	};

	static Component<Owner> ComponentCreator("owner");
} // namespace ecs
