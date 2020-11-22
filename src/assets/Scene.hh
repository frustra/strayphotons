#pragma once

#include "Common.hh"

#include <ecs/Ecs.hh>
#include <unordered_map>

namespace sp {
    class Asset;

    class Scene : public NonCopyable {
    public:
        Scene(const string &name, shared_ptr<Asset> asset);
        ~Scene() {}

        const string name;
        vector<ecs::Entity> entities;

        vector<string> autoExecList, unloadExecList;

    private:
        shared_ptr<Asset> asset;
    };
} // namespace sp
