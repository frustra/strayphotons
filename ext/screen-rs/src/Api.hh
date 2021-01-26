#pragma once
#include "../target/cxxbridge/rust/cxx.h"
#include <memory>

namespace sp {

class Api {
public:
  Api();
  void set_frame(rust::Slice<uint8_t> slice) const;
  const uint8_t* get_frame();
  size_t get_size();

private:
  class impl;
  std::shared_ptr<impl> impl;
  static const uint8_t *pFrame;
  static size_t frame_size;
};

std::unique_ptr<Api> connect();

} // namespace sp
