#include "sp-rs/src/api.hh"

#include "sp-rs/src/api.rs.h"

namespace sp {
    namespace api {
        Api::Api() {}

        std::unique_ptr<Api> new_api() {
            return std::make_unique<Api>();
        }
    } // namespace api
} // namespace sp
