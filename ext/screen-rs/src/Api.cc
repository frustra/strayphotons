#include "Api.hh"
#include "../target/cxxbridge/screen-rs/src/lib.rs.h"

namespace sp {

class Api::impl {
    friend Api;
};

const uint8_t * Api::pFrame = nullptr;
size_t Api::frame_size = 0;

Api::Api() : impl(new class Api::impl) {}

void Api::set_frame(rust::Slice<uint8_t> slice) const {
    pFrame = slice.data();
    frame_size = slice.size();
}

const uint8_t* Api::get_frame() {
    return pFrame;
}

size_t Api::get_size() {
    return frame_size;
}

std::unique_ptr<Api> connect() {
    return std::make_unique<Api>();
}
}