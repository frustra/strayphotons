#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"

#include <unordered_map>

namespace sp {
    class Asset;

    class Scene : public NonCopyable {
    public:
        Scene(const string &name, shared_ptr<const Asset> asset);
        ~Scene() {}

        const string name;
        vector<Tecs::Entity> entities;

        vector<string> autoExecList, unloadExecList;

    private:
        shared_ptr<const Asset> asset;
    };
} // namespace sp
