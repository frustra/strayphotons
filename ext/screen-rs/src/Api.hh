#pragma once
#include <memory>

namespace sp {

class Api {
public:
  Api();
  void draw(uint8_t &buf, size_t size);

private:
  class impl;
  std::shared_ptr<impl> impl;
};

std::unique_ptr<Api> connect();

} // namespace sp
