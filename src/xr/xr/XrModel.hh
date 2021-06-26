#pragma once

#include "assets/Model.hh"

#include <memory>

namespace sp {

    class Model;
    namespace xr {
        class XrModel : public sp::Model {
        public:
            XrModel(const string &name) : sp::Model(name) {}
            XrModel(const string &name, std::shared_ptr<tinygltf::Model> model) : Model(name, model) {}
        };
    } // namespace xr
} // namespace sp
