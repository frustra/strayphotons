#pragma once

#include <string>

namespace sp {
    static const char *const InputBindingConfigPath = "input_bindings.json";

    class BindingLoader {
    public:
        BindingLoader();

        void Load(std::string bindingConfigPath);
    };
} // namespace sp
