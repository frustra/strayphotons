#pragma once

#include "Common.hh"

#include <ecs/Components.hh>

namespace ecs
{
	class LightGun
	{
	public:
		LightGun();

		bool hasLight;
	};

	static Component<LightGun> ComponentLightGun("lightGun");

	template<>
	bool Component<LightGun>::LoadEntity(Entity &dst, picojson::value &src);
}