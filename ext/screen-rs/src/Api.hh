#pragma once
#include "cxx.h"
#include <memory>

namespace sp {

class Api {
public:
  Api();
  void draw(rust::Slice<uint8_t>, size_t size) const;

private:
  class impl;
  std::shared_ptr<impl> impl;
};

std::unique_ptr<Api> connect();

} // namespace sp
