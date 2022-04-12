#include "Focus.hh"

#include "core/Common.hh"
#include "core/Logging.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<FocusLayer>::Load(const EntityScope &scope, FocusLayer &focus, const picojson::value &src) {
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

    std::ostream &operator<<(std::ostream &out, const FocusLayer &v) {
        switch (v) {
        case FocusLayer::NEVER:
            return out << "FocusLayer::NEVER";
        case FocusLayer::GAME:
            return out << "FocusLayer::GAME";
        case FocusLayer::MENU:
            return out << "FocusLayer::MENU";
        case FocusLayer::OVERLAY:
            return out << "FocusLayer::OVERLAY";
        case FocusLayer::ALWAYS:
            return out << "FocusLayer::ALWAYS";
        default:
            return out << "FocusLayer::INVALID";
        }
    }

    std::istream &operator>>(std::istream &in, FocusLayer &v) {
        std::string layer;
        in >> layer;
        sp::to_upper(layer);
        if (layer == "NEVER") {
            v = FocusLayer::NEVER;
        } else if (layer == "GAME") {
            v = FocusLayer::GAME;
        } else if (layer == "MENU") {
            v = FocusLayer::MENU;
        } else if (layer == "OVERLAY") {
            v = FocusLayer::OVERLAY;
        } else if (layer == "ALWAYS") {
            v = FocusLayer::ALWAYS;
        } else {
            Errorf("Invalid FocusLayer name: %s", layer);
            v = FocusLayer::NEVER;
        }
        return in;
    }

    std::ostream &operator<<(std::ostream &out, const FocusLock &v) {
        bool first = true;
        for (size_t i = static_cast<size_t>(FocusLayer::NEVER); i < static_cast<size_t>(FocusLayer::ALWAYS); i++) {
            auto layer = static_cast<FocusLayer>(i);
            if (v.HasFocus(layer)) {
                if (!first) out << " ";
                out << layer;
                first = false;
            }
        }
        return out;
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
        layers.set(index);
        for (size_t i = index + 1; i < layers.size(); i++) {
            if (layers.test(i)) return false;
        }
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

    bool FocusLock::HasPrimaryFocus(FocusLayer layer) const {
        if (layer == FocusLayer::NEVER) return false;
        if (layer == FocusLayer::ALWAYS) return true;

        size_t index = static_cast<size_t>(layer) - 1;
        for (size_t i = index + 1; i < layers.size(); i++) {
            if (layers.test(i)) return false;
        }
        return layers.test(index);
    }

    bool FocusLock::HasFocus(FocusLayer layer) const {
        if (layer == FocusLayer::NEVER) return false;
        if (layer == FocusLayer::ALWAYS) return true;

        return layers.test(static_cast<size_t>(layer) - 1);
    }

    FocusLayer FocusLock::PrimaryFocus() const {
        for (size_t i = layers.size() - 1; i >= 0; i--) {
            if (layers.test(i)) return static_cast<FocusLayer>(i + 1);
        }
        return FocusLayer::NEVER;
    }
} // namespace ecs
