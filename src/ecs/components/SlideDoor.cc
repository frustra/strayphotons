#include "ecs/components/SlideDoor.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SlideDoor>::Load(Lock<Read<ecs::Name>> lock, SlideDoor &slideDoor, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "left") {
                auto panelName = param.second.get<string>();
                for (auto ent : lock.EntitiesWith<Name>()) {
                    if (ent.Get<Name>(lock) == panelName) {
                        slideDoor.left = ent;
                        break;
                    }
                }
            } else if (param.first == "right") {
                auto panelName = param.second.get<string>();
                for (auto ent : lock.EntitiesWith<Name>()) {
                    if (ent.Get<Name>(lock) == panelName) {
                        slideDoor.right = ent;
                        break;
                    }
                }
            }
        }
        return true;
    }

    void SlideDoor::ValidateDoor(Lock<Read<Animation>> lock) const {
        if (left && !left.Has<Animation>(lock)) { throw std::runtime_error("Left door panel is not setup"); }
        if (right && !right.Has<Animation>(lock)) { throw std::runtime_error("Right door panel is not setup"); }
    }

    SlideDoor::State SlideDoor::GetState(Lock<Read<Animation>> lock) const {
        ValidateDoor(lock);
        auto lPanel = left.Get<Animation>(lock);
        // auto rPanel = right.Get<Animation>(lock);

        // SlideDoor::State state;
        // if (lPanel.curState == 1 && lPanel.nextState < 0) {
        //     state = SlideDoor::State::OPENED;
        // } else if (lPanel.curState == 1 && lPanel.nextState >= 0) {
        //     state = SlideDoor::State::CLOSING;
        // } else if (lPanel.curState == 0 && lPanel.nextState < 0) {
        //     state = SlideDoor::State::CLOSED;
        // } else {
        //     state = SlideDoor::State::OPENING;
        // }

        // return state;
        return SlideDoor::State::CLOSED;
    }

    void SlideDoor::Open(Lock<Write<Animation>> lock) const {
        ValidateDoor(lock);

        auto &lPanel = left.Get<Animation>(lock);
        auto &rPanel = right.Get<Animation>(lock);
        lPanel.AnimateToState(1);
        rPanel.AnimateToState(1);
    }

    void SlideDoor::Close(Lock<Write<Animation>> lock) const {
        ValidateDoor(lock);

        auto &lPanel = left.Get<Animation>(lock);
        auto &rPanel = right.Get<Animation>(lock);
        lPanel.AnimateToState(0);
        rPanel.AnimateToState(0);
    }
} // namespace ecs
