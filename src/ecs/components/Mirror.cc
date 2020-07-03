#include "ecs/components/Mirror.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/Components.hh>
#include <picojson/picojson.h>

namespace ecs {
	template<>
	bool Component<Mirror>::LoadEntity(Entity &dst, picojson::value &src) {
		auto mirror = dst.Assign<Mirror>();
		mirror->size = sp::MakeVec2(src);
		return true;
	}
} // namespace ecs
