#include "ecs/components/Physics.hh"

#include <ecs/Components.hh>
#include <tinygltfloader/picojson.h>
#include <assets/AssetManager.hh>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<Physics>::LoadEntity(Entity &dst, picojson::value &src)
	{
        auto physics = dst.Assign<Physics>();

        for (auto param : src.get<picojson::object>())
        {
            if (param.first == "model")
            {
                physics->model = sp::GAssets.LoadModel(param.second.get<string>());
            }
            else if (param.first == "dynamic")
            {
                physics->desc.dynamic = param.second.get<bool>();
            }
            else if (param.first == "kinematic")
            {
                physics->desc.kinematic = param.second.get<bool>();
            }
            else if (param.first == "decomposeHull")
            {
                physics->desc.decomposeHull = param.second.get<bool>();
            }
            else if (param.first == "density")
            {
                physics->desc.density = param.second.get<double>();
            }
        }
		return true;
	}
}
