#include "Api.hh"
#include "Screen.h"
#include "graphics/postprocess/PostProcess.hh"

namespace sp {

class Api::impl {
    friend Api;
};

Api::Api() : impl(new class Api::impl) {}

void Api::draw(rust::Slice<const uint8_t> slice, size_t size) const {
    if (slice.size() == size) {
        PostProcessing::DrawFrame(slice.data(), slice.size());
    }
}

std::unique_ptr<Api> connect() {
    return std::make_unique<Api>();
}
}