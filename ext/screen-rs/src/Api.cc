#include "Api.hh"
#include "Screen.hh"

namespace sp {

class Api::impl {
    friend Api;
};

const uint8_t * Api::pFrame = nullptr;
size_t Api::frame_size = 0;

Api::Api() : impl(new class Api::impl) {}

void Api::set_frame(rust::Slice<uint8_t> slice) const {
    this->pFrame = slice.data();
    this->frame_size = slice.size();
}

const uint8_t* Api::get_frame() {
    this->pFrame;
}

size_t Api::get_size() {
    this->frame_size;
}

std::unique_ptr<Api> connect() {
    return std::make_unique<Api>();
}
}