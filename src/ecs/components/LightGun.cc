#include "ecs/components/LightGun.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/Components.hh>
#include <picojson/picojson.h>

namespace ecs {
	template<>
	bool Component<LightGun>::LoadEntity(Entity &dst, picojson::value &src) {
		dst.Assign<LightGun>();
		return true;
	}

	LightGun::LightGun() {
		this->hasLight = false;
	}
} // namespace ecs
