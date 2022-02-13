#include "Logging.hh"

namespace sp::logging {
    static chrono_clock::time_point LogEpoch = chrono_clock::now();

    float LogTime() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(chrono_clock::now() - LogEpoch).count() / 1000.0f;
    }
} // namespace sp::logging
