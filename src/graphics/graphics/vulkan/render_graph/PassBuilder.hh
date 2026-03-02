/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "graphics/vulkan/render_graph/Pass.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

#include <string_view>

namespace sp::vulkan::render_graph {
    class PassBuilder {
    public:
        PassBuilder(Resources &resources, Pass &pass) : resources(resources), pass(pass) {}

        ResourceID GetID(std::string_view name, bool assertExists = true) {
            return resources.GetID(name, assertExists);
        }
        Resource GetResource(ResourceID id) {
            return resources.GetResource(id);
        }
        Resource GetResource(std::string_view name) {
            return resources.GetResource(GetID(name));
        }

        void Read(ResourceID id, Access access);
        ResourceID Read(std::string_view name, Access access);
        ResourceID ReadPreviousFrame(std::string_view name, Access access, uint32_t framesAgo = 1);

        void Write(ResourceID id, Access access);
        ResourceID Write(std::string_view name, Access access);

        // Indicates an access to a uniform buffer from any shader, equivalent to Read with Access::AnyShaderUniformRead
        const Resource &ReadUniform(std::string_view name);
        const Resource &ReadUniform(ResourceID id);

        void SetColorAttachment(uint32_t index, std::string_view name, const AttachmentInfo &info);
        void SetColorAttachment(uint32_t index, ResourceID id, const AttachmentInfo &info);
        Resource OutputColorAttachment(uint32_t index,
            std::string_view name,
            ImageDesc desc,
            const AttachmentInfo &info);

        void SetDepthAttachment(std::string_view name, const AttachmentInfo &info);
        void SetDepthAttachment(ResourceID id, const AttachmentInfo &info);
        Resource OutputDepthAttachment(std::string_view name, ImageDesc desc, const AttachmentInfo &info);

        // The attachment at this index will become the LastOutput of the graph after the pass, defaults to 0
        void SetPrimaryAttachment(uint32_t index);

        Resource CreateImage(std::string_view name, const ImageDesc &desc, Access access);

        ImageDesc DeriveImage(ResourceID id) {
            return resources.GetResourceRef(id).DeriveImage();
        }

        Resource CreateBuffer(BufferLayout layout, Residency residency, Access access);
        Resource CreateBuffer(std::string_view name, BufferLayout layout, Residency residency, Access access);

        Resource CreateUniform(std::string_view name, size_t size) {
            return CreateBuffer(name, size, Residency::CPU_TO_GPU, Access::HostWrite);
        }

        const ResourceName &GetName(ResourceID id) const {
            return resources.GetName(id);
        }

        ResourceID LastOutputID() const {
            return resources.lastOutputID;
        }
        Resource LastOutput() const {
            return resources.LastOutput();
        }
        const ResourceName &LastOutputName() const {
            return resources.GetName(resources.lastOutputID);
        }

        // Indicates pending command buffers should be submitted before Execute is called
        void FlushCommands() {
            pass.flushCommands = true;
        }

        void RequirePass() {
            pass.required = true;
        }

    private:
        Resource OutputAttachment(uint32_t index,
            std::string_view name,
            const ImageDesc &desc,
            const AttachmentInfo &info);

        void SetAttachment(uint32_t index, ResourceID id, const AttachmentInfo &info);

        Resources &resources;
        Pass &pass;
    };
} // namespace sp::vulkan::render_graph
