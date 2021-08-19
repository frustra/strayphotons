#include "Scene.hh"

#include "assets/Asset.hh"

namespace sp {
    Scene::Scene(const string &name, shared_ptr<const Asset> asset) : name(name), asset(asset) {}
} // namespace sp