/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "graphics/GenericCompositor.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

struct ImFontAtlas;
struct ImDrawData;

namespace sp {
    class GuiContext;
} // namespace sp

namespace sp::vulkan {
    class Renderer;
    struct VertexLayout;

    class Compositor : public GenericCompositor {
    public:
        Compositor(DeviceContext &device, Renderer &renderer);

        enum class PassOrder : uint8_t {
            BeforeViews,
            AfterViews,
        };

        void DrawGui(const GuiDrawData &drawData, glm::ivec4 viewport, glm::vec2 scale) override;
        void UpdateSourceImage(ecs::Entity dst, std::shared_ptr<sp::Image> src) override;

        void BeforeFrame();
        void UpdateRenderOutputs(ecs::Lock<ecs::Read<ecs::Name, ecs::RenderOutput>> lock);
        void AddOutputPasses(PassOrder order);
        void EndFrame();

    private:
        void internalDrawGui(const GuiDrawData &drawData, vk::Rect2D viewport, glm::vec2 scale);
        void internalDrawGui(ImDrawData *drawData, vk::Rect2D viewport, glm::vec2 scale, bool allowUserCallback);

        Renderer &renderer;
        GPUScene &scene;
        rg::RenderGraph &graph;
        double lastTime = 0.0, deltaTime;

        struct RenderOutputInfo {
            ecs::Name entityName;
            ecs::Entity entity;
            glm::ivec2 outputSize;
            glm::vec2 scale;
            std::shared_ptr<GuiContext> guiContext;
            bool enableGui;
            std::vector<ecs::EntityRef> guiElements;
            rg::ResourceName sourceName;
            rg::ResourceID sourceResourceID = rg::InvalidResource;
            rg::ResourceID outputResourceID = rg::InvalidResource;
        };
        vector<RenderOutputInfo> renderOutputs;
        robin_hood::unordered_map<ecs::Entity, size_t> existingOutputs;
        // 3D views are rendered after this many renderOutputs, allowing later renderOutputs to reference view outputs
        size_t viewRenderPassOffset = 0;

        PreservingMap<ecs::Entity, GuiContext, 500> ephemeralGuiContexts;

        struct CpuImageSource {
            std::shared_ptr<sp::Image> cpuImage;
            bool cpuImageModified = false;
            std::deque<AsyncPtr<Image>> pendingUploads;
            ImageViewPtr imageView;
        };
        LockFreeMutex uploadMutex;
        robin_hood::unordered_map<ecs::Entity, CpuImageSource> cpuImageSources;

        unique_ptr<VertexLayout> vertexLayout;

        shared_ptr<ImFontAtlas> fontAtlas;
        AsyncPtr<ImageView> fontView;
    };
} // namespace sp::vulkan
