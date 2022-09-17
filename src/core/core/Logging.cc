#include "Logging.hh"

namespace sp::logging {
    #ifdef SP_PACKAGE_RELEASE
    static Level logLevel = Level::Log;
    #else
    static Level logLevel = Level::Debug;
    #endif
    static chrono_clock::time_point LogEpoch = chrono_clock::now();

    float LogTime() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(chrono_clock::now() - LogEpoch).count() / 1000.0f;
    }

    Level GetLogLevel() {
        return logLevel;
    }

    void SetLogLevel(Level level) {
        logLevel = level;
    }
} // namespace sp::logging
