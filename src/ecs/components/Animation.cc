#include "ecs/components/Animation.hh"

#include "core/Logging.hh"

#include <sstream>

namespace ecs {
    void Animation::AnimateToState(uint32 i) {
        if (i > states.size()) {
            std::stringstream ss;
            ss << "\"" << i << "\" is an invalid state for this Animation with " << states.size() << " states";
            throw std::runtime_error(ss.str());
        }

        if (nextState >= 0) curState = nextState;

        nextState = i;
    }
} // namespace ecs