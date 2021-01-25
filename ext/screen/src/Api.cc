#include "Api.hh"
#include "main.rs.h"
#include "graphics/postprocess/PostProcess.hh"

class Api::impl {
  friend Api;
};

Api::Api() : impl(new class Api::impl) {}

void Api::draw(uint8_t &buf, size_t size) {
    DrawFrame(buf, size);
}

std::unique_ptr<Api> connect() {
    return std::make_unique<Api>();
}