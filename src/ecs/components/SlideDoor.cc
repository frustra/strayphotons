#include "ecs/components/SlideDoor.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SlideDoor>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src) {
        auto &slideDoor = dst.Set<SlideDoor>(lock);
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
            } else if (param.first == "width") {
                slideDoor.width = param.second.get<double>();
            } else if (param.first == "openTime") {
                slideDoor.openTime = param.second.get<double>();
            } else if (param.first == "forward") {
                slideDoor.forward = sp::MakeVec3(param.second);
            }
        }
        return true;
    }

    void SlideDoor::ValidateDoor(EntityManager &em) {
        if (left && !Entity(&em, left).Has<Animation>()) {
            SetAnimation(Entity(&em, left), LeftDirection(Entity(&em, left)));
        }
        if (right && !Entity(&em, right).Has<Animation>()) {
            SetAnimation(Entity(&em, right), -LeftDirection(Entity(&em, right)));
        }
    }

    SlideDoor::State SlideDoor::GetState(EntityManager &em) {
        ValidateDoor(em);
        auto lPanel = Entity(&em, left).Get<Animation>();
        // auto rPanel = right.Get<Animation>();

        SlideDoor::State state;
        if (lPanel->curState == 1 && lPanel->nextState < 0) {
            state = SlideDoor::State::OPENED;
        } else if (lPanel->curState == 1 && lPanel->nextState >= 0) {
            state = SlideDoor::State::CLOSING;
        } else if (lPanel->curState == 0 && lPanel->nextState < 0) {
            state = SlideDoor::State::CLOSED;
        } else {
            state = SlideDoor::State::OPENING;
        }

        return state;
    }

    void SlideDoor::Open(EntityManager &em) {
        ValidateDoor(em);

        auto lPanel = Entity(&em, left).Get<Animation>();
        auto rPanel = Entity(&em, right).Get<Animation>();
        lPanel->AnimateToState(1);
        rPanel->AnimateToState(1);
    }

    void SlideDoor::Close(EntityManager &em) {
        ValidateDoor(em);

        auto lPanel = Entity(&em, left).Get<Animation>();
        auto rPanel = Entity(&em, right).Get<Animation>();
        lPanel->AnimateToState(0);
        rPanel->AnimateToState(0);
    }

    void SlideDoor::SetAnimation(Entity panel, glm::vec3 openDir) {
        if (!panel.Valid() || !panel.Has<Transform>() || panel.Has<Animation>()) { return; }

        auto transform = panel.Get<Transform>();
        Animation animation;
        float panelWidth = this->width / 2;
        glm::vec3 panelPos = transform->GetPosition();
        glm::vec3 animatePos = panelPos + panelWidth * openDir;

        animation.states.resize(2);
        animation.animationTimes.resize(2);

        // closed
        Animation::State closeState;
        closeState.scale = transform->GetScaleVec();
        closeState.pos = panelPos;
        animation.states[0] = closeState;
        animation.animationTimes[0] = this->openTime;

        // open
        Animation::State openState;
        openState.scale = transform->GetScaleVec();
        openState.pos = animatePos;
        animation.states[1] = openState;
        animation.animationTimes[1] = this->openTime;

        animation.curState = 0;

        panel.Assign<Animation>(animation);
    }

    glm::vec3 SlideDoor::LeftDirection(Entity panel) {
        sp::Assert(panel.Valid() && panel.Has<Transform>(), "Panel must have valid transform");
        auto transform = panel.Get<Transform>();
        return glm::normalize(glm::cross(this->forward, transform->GetUp()));
    }
} // namespace ecs
