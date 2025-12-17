/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "common/PreservingMap.hh"
#include "graphics/GenericCompositor.hh"
#include "graphics/core/Texture.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/RenderGraph.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

struct ImFontAtlas;
struct ImDrawData;

namespace sp {
    class GuiContext;
} // namespace sp

namespace sp::vulkan {
    struct VertexLayout;

    class Compositor final : public GenericCompositor {
    public:
        Compositor(DeviceContext &device, render_graph::RenderGraph &graph);

        enum class PassOrder : uint8_t {
            BeforeViews,
            AfterViews,
        };

        void DrawGui(const GuiDrawData &drawData, glm::ivec4 viewport, glm::vec2 scale) override;

        std::shared_ptr<GpuTexture> UploadStaticImage(std::shared_ptr<const sp::Image> image,
            bool genMipmap = true,
            bool srgb = true) override;
        rg::ResourceID AddStaticImage(const rg::ResourceName &name, std::shared_ptr<GpuTexture> image) override;
        void UpdateSourceImage(ecs::Entity dst, std::shared_ptr<sp::Image> src) override;

        void BeforeFrame(rg::RenderGraph &graph);
        void UpdateRenderOutputs(ecs::Lock<ecs::Read<ecs::Name, ecs::RenderOutput>> lock);
        void AddOutputPasses(PassOrder order);
        void EndFrame();

    private:
        void internalDrawGui(const GuiDrawData &drawData, vk::Rect2D viewport, glm::vec2 scale);
        void internalDrawGui(ImDrawData *drawData, vk::Rect2D viewport, glm::vec2 scale, bool allowUserCallback);

        DeviceContext &device;
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

        struct DynamicImageSource {
            std::shared_ptr<sp::Image> cpuImage;
            bool cpuImageModified = false;
            std::deque<AsyncPtr<Image>> pendingUploads;
            ImageViewPtr imageView;
        };
        struct StaticImageSource {
            std::shared_ptr<sp::Image> cpuImage;
            AsyncPtr<ImageView> gpuImage;
        };
        LockFreeMutex dynamicSourceMutex;
        robin_hood::unordered_map<ecs::Entity, DynamicImageSource> dynamicImageSources;

        unique_ptr<VertexLayout> vertexLayout;

        shared_ptr<ImFontAtlas> fontAtlas;
        AsyncPtr<ImageView> fontView;
    };
} // namespace sp::vulkan
