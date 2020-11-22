#include "ecs/components/SlideDoor.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<SlideDoor>::LoadEntity(Entity &dst, picojson::value &src) {
        auto slideDoor = dst.Assign<SlideDoor>();

        for (auto param : src.get<picojson::object>()) {
            if (param.first == "left") {
                slideDoor->left = NamedEntity(param.second.get<string>());
            } else if (param.first == "right") {
                slideDoor->right = NamedEntity(param.second.get<string>());
            } else if (param.first == "width") {
                slideDoor->width = param.second.get<double>();
            } else if (param.first == "openTime") {
                slideDoor->openTime = param.second.get<double>();
            } else if (param.first == "forward") {
                slideDoor->forward = sp::MakeVec3(param.second);
            }
        }
        return true;
    }

    void SlideDoor::ValidateDoor(EntityManager &em) {
        if (!left.Load(em) || !right.Load(em)) { throw std::runtime_error("door panel is no longer valid"); }
        if (!left->Has<Animation>()) { SetAnimation(left, LeftDirection(left)); }
        if (!right->Has<Animation>()) { SetAnimation(right, -LeftDirection(right)); }
    }

    SlideDoor::State SlideDoor::GetState(EntityManager &em) {
        ValidateDoor(em);
        auto lPanel = left->Get<Animation>();
        auto rPanel = right->Get<Animation>();

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

        auto lPanel = left->Get<Animation>();
        auto rPanel = right->Get<Animation>();
        lPanel->AnimateToState(1);
        rPanel->AnimateToState(1);
    }

    void SlideDoor::Close(EntityManager &em) {
        ValidateDoor(em);

        auto lPanel = left->Get<Animation>();
        auto rPanel = right->Get<Animation>();
        lPanel->AnimateToState(0);
        rPanel->AnimateToState(0);
    }

    void SlideDoor::SetAnimation(NamedEntity &panel, glm::vec3 openDir) {
        if (!panel->Valid() || !panel->Has<Transform>() || panel->Has<Animation>()) { return; }

        auto transform = panel->Get<Transform>();
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

        panel->Assign<Animation>(animation);
    }

    glm::vec3 SlideDoor::LeftDirection(NamedEntity &panel) {
        sp::Assert(panel->Valid() && panel->Has<Transform>(), "Panel must have valid transform");
        auto transform = panel->Get<Transform>();
        return glm::normalize(glm::cross(this->forward, transform->GetUp()));
    }
} // namespace ecs
