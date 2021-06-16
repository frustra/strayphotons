#pragma once

#include "Common.hh"
#include "assets/Model.hh"
namespace sp {

    class Model;
    namespace xr {
        class XrModel : public sp::Model {
        public:
            XrModel(const string &name) : sp::Model(name) {}
            XrModel(const string &name, shared_ptr<tinygltf::Model> model) : Model(name, model) {}
        };
    } // namespace xr
} // namespace sp