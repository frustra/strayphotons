#include "ecs/components/LightGun.hh"
#include "game/InputManager.hh"

#include <picojson/picojson.h>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<LightGun>::LoadEntity(Entity &dst, picojson::value &src)
	{
		dst.Assign<LightGun>();
		return true;
	}

	const vector<int> LightGun::DEFAULT_SUCK_LIGHT_KEYS =
	{
		sp::MouseButtonToKey(GLFW_MOUSE_BUTTON_RIGHT)
	};
	const vector<int> LightGun::DEFAULT_SHOOT_LIGHT_KEYS =
	{
		sp::MouseButtonToKey(GLFW_MOUSE_BUTTON_LEFT)
	};

	LightGun::LightGun(const vector<int> *suckLightKeys, const vector<int> *shootLightKeys)
	{
		if (shootLightKeys == nullptr)
		{
			shootLightKeys = &DEFAULT_SHOOT_LIGHT_KEYS;
		}
		if (suckLightKeys == nullptr)
		{
			suckLightKeys = &DEFAULT_SUCK_LIGHT_KEYS;
		}

		this->shootLightKeys = *shootLightKeys;
		this->suckLightKeys = *suckLightKeys;
		this->hasLight = false;
	}
}