#pragma once

#include "core/DispatchQueue.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"

namespace sp {
    class EditorSystem : public RegisteredThread {
    public:
        EditorSystem();
        ~EditorSystem();

        void OpenEditor(std::string targetName);

    private:
        bool ThreadInit() override;
        void Frame() override;

        DispatchQueue workQueue;

        ecs::EntityRef targetEntity;
        ecs::EntityRef playerEntity = ecs::Name("player", "player");
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");

        CFuncCollection funcs;
    };

} // namespace sp
