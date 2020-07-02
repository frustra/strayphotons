#pragma once

#include "assets/Model.hh"

namespace sp
{
namespace xr
{
    class XrModel : public Model
    {
    public:
        XrModel(const string &name) : Model(name) {};
        XrModel(const string &name, shared_ptr<tinygltf::Model> model) : Model(name, model) {};
    };
}
}