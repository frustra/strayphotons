#pragma once
// #include "rust/cxx.h"

#include <memory>

namespace sp {
    namespace api {
        class Api {
        public:
            Api();
        };

        std::unique_ptr<Api> new_api();
    } // namespace api
} // namespace sp
