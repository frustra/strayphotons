#pragma once
#include "cxx.h"
#include <memory>

namespace sp {

class Api {
public:
  Api();
  void draw(rust::Slice<const uint8_t> slice, size_t size) const;

private:
  class impl;
  std::shared_ptr<impl> impl;
};

std::unique_ptr<Api> connect();

namespace PostProcessing {
  void DrawFrame(uint8 &buf, size_t size);
}

} // namespace sp
