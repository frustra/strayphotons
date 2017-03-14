#pragma once

#include "Common.hh"
#include <vector>

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
}