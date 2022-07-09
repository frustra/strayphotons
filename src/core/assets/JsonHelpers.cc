#include "JsonHelpers.hh"

#include "core/Common.hh"

#include <string>
#include <vector>

namespace sp::json {
    bool ParametersExist(const picojson::value &json, std::vector<std::string> reqParams) {
        std::vector<bool> found(reqParams.size(), false);

        for (auto param : json.get<picojson::object>()) {
            for (uint32 i = 0; i < found.size(); ++i) {
                if (param.first == reqParams.at(i)) {
                    found.at(i) = true;
                    break;
                }
            }
        }

        for (uint32_t i = 0; i < found.size(); ++i) {
            if (!found.at(i)) {
                return false;
            }
        }
        return true;
    }
} // namespace sp::json
