#include "Focus.hh"

#include "core/Common.hh"
#include "core/Logging.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<FocusLayer>::Load(Lock<Read<ecs::Name>> lock, FocusLayer &focus, const picojson::value &src) {
        auto layer = src.get<std::string>();
        sp::to_upper(layer);
        if (layer == "NEVER") {
            focus = FocusLayer::NEVER;
        } else if (layer == "GAME") {
            focus = FocusLayer::GAME;
        } else if (layer == "MENU") {
            focus = FocusLayer::MENU;
        } else if (layer == "OVERLAY") {
            focus = FocusLayer::OVERLAY;
        } else if (layer == "ALWAYS") {
            focus = FocusLayer::ALWAYS;
        } else {
            Errorf("Unknown focus layer: %s", layer);
            return false;
        }
        return true;
    }

    FocusLock::FocusLock(FocusLayer layer) {
        if (layer != FocusLayer::NEVER && layer != FocusLayer::ALWAYS) { AcquireFocus(layer); }
    }

    bool FocusLock::AcquireFocus(FocusLayer layer) {
        if (layer == FocusLayer::NEVER) {
            Errorf("Trying to acquire focus layer NEVER");
            return false;
        } else if (layer == FocusLayer::ALWAYS) {
            Errorf("Trying to acquire focus layer ALWAYS");
            return true;
        }

        size_t index = static_cast<size_t>(layer) - 1;
        for (size_t i = index + 1; i < layers.size(); i++) {
            if (layers.test(i)) return false;
        }
        layers.set(index);
        return true;
    }

    void FocusLock::ReleaseFocus(FocusLayer layer) {
        if (layer == FocusLayer::NEVER) {
            Errorf("Trying to release focus layer NEVER");
            return;
        } else if (layer == FocusLayer::ALWAYS) {
            Errorf("Trying to release focus layer ALWAYS");
            return;
        }

        layers.reset(static_cast<size_t>(layer) - 1);
    }

    bool FocusLock::HasFocus(FocusLayer layer) const {
        if (layer == FocusLayer::NEVER) return false;
        if (layer == FocusLayer::ALWAYS) return true;

        size_t index = static_cast<size_t>(layer) - 1;
        for (size_t i = index + 1; i < layers.size(); i++) {
            if (layers.test(i)) return false;
        }
        return layers.test(index);
    }
} // namespace ecs
