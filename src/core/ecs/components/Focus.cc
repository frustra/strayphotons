#include "Focus.hh"

#include "core/Common.hh"
#include "core/Logging.hh"

#include <picojson/picojson.h>

namespace ecs {
    std::ostream &operator<<(std::ostream &out, const FocusLock &v) {
        bool first = true;
        for (size_t i = static_cast<size_t>(FocusLayer::Never); i < static_cast<size_t>(FocusLayer::Always); i++) {
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
        if (layer != FocusLayer::Never && layer != FocusLayer::Always) {
            AcquireFocus(layer);
        }
    }

    bool FocusLock::AcquireFocus(FocusLayer layer) {
        if (layer == FocusLayer::Never) {
            Errorf("Trying to acquire focus layer Never");
            return false;
        } else if (layer == FocusLayer::Always) {
            Errorf("Trying to acquire focus layer Always");
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
        if (layer == FocusLayer::Never) {
            Errorf("Trying to release focus layer Never");
            return;
        } else if (layer == FocusLayer::Always) {
            Errorf("Trying to release focus layer Always");
            return;
        }

        layers.reset(static_cast<size_t>(layer) - 1);
    }

    bool FocusLock::HasPrimaryFocus(FocusLayer layer) const {
        if (layer == FocusLayer::Never) return false;
        if (layer == FocusLayer::Always) return true;

        size_t index = static_cast<size_t>(layer) - 1;
        for (size_t i = index + 1; i < layers.size(); i++) {
            if (layers.test(i)) return false;
        }
        return layers.test(index);
    }

    bool FocusLock::HasFocus(FocusLayer layer) const {
        if (layer == FocusLayer::Never) return false;
        if (layer == FocusLayer::Always) return true;

        return layers.test(static_cast<size_t>(layer) - 1);
    }

    FocusLayer FocusLock::PrimaryFocus() const {
        for (size_t i = layers.size() - 1; i >= 0; i--) {
            if (layers.test(i)) return static_cast<FocusLayer>(i + 1);
        }
        return FocusLayer::Never;
    }
} // namespace ecs
