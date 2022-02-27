#include "Screen.hh"

#include "assets/AssetHelpers.hh"

namespace ecs {
    template<>
    bool Component<Screen>::Load(ScenePtr scenePtr, Screen &dst, const picojson::value &src) {
        if (src.is<string>()) {
            dst.textureName = src.get<string>();
        } else {
            for (auto param : src.get<picojson::object>()) {
                if (param.first == "target") {
                    dst.textureName = param.second.get<string>();
                } else if (param.first == "luminance") {
                    dst.luminanceScale = sp::MakeVec3(param.second);
                }
            }
        }
        return true;
    }
} // namespace ecs
