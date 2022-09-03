#pragma once

#include "core/DispatchQueue.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"

#include <functional>

namespace sp {
    class EditorSystem : public RegisteredThread {
    public:
        EditorSystem();
        ~EditorSystem();

        void OpenEditor(std::string targetName);

        AsyncPtr<void> PushEdit(std::function<void()> editFunction) {
            return workQueue.Dispatch<void>(editFunction);
        }

    private:
        void Frame() override;

        DispatchQueue workQueue;

        ecs::EntityRef targetEntity;
        ecs::Entity previousTargetEntity = {};
        ecs::EntityRef playerEntity = ecs::Name("player", "player");
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");

        CFuncCollection funcs;
    };

    extern EditorSystem GEditor;

} // namespace sp
