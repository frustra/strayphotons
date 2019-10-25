#include "ecs/components/Mirror.hh"

#include <ecs/Components.hh>
#include <tinygltfloader/picojson.h>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<Mirror>::LoadEntity(Entity &dst, picojson::value &src)
	{
		auto mirror = dst.Assign<Mirror>();
		mirror->size = sp::MakeVec2(src);
		return true;
	}
}
