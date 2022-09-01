#pragma once

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
        void PreFrame() override;
        void Frame() override;

        ecs::EntityRef targetEntity;
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");

        CFuncCollection funcs;
    };

} // namespace sp
