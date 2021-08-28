#pragma once

#include "assets/Model.hh"

#include <memory>

namespace sp {

    class Model;
    namespace xr {
        class XrModel : public sp::Model {
        public:
            XrModel(const string &name) : sp::Model(name) {}
        };
    } // namespace xr
} // namespace sp
