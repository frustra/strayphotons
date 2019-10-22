#pragma once

#include "Common.hh"
#include <vector>

#include <ecs/Components.hh>

namespace ecs
{
	class LightGun
	{
	public:
		LightGun(const vector<int> *suckLightKeys = nullptr,
				 const vector<int> *shootLightKeys = nullptr);


		bool hasLight;
		vector<int> suckLightKeys;
		vector<int> shootLightKeys;

	private:
		const static vector<int> DEFAULT_SUCK_LIGHT_KEYS;
		const static vector<int> DEFAULT_SHOOT_LIGHT_KEYS;
	};

	static Component<LightGun> ComponentLightGun("lightGun");

	template<>
	bool Component<LightGun>::LoadEntity(Entity &dst, picojson::value &src);
}