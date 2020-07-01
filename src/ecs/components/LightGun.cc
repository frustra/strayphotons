#include "ecs/components/LightGun.hh"

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

	LightGun::LightGun()
	{
		this->hasLight = false;
	}
}