#pragma once

#include <string>

namespace sp {
    static const char *InputBindingConfigPath = "input_bindings.json";

    class BindingLoader {
    public:
        BindingLoader();

        void Load(std::string bindingConfigPath);
    };
} // namespace sp
