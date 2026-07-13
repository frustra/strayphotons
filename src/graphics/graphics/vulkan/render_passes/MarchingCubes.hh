/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/CFunc.hh"
#include "ecs/components/Renderable.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    extern CVar<uint32_t> CVarEnableMarchingCubes;
    extern CVar<uint32_t> CVarMarchingCubesLayer;

    class Voxels;

    class MarchingCubes {
    public:
        MarchingCubes(GPUScene &scene);

        void AddMarchingCubes(rg::RenderGraph &graph, const Voxels &voxels);

    private:
        // GPUScene &scene;

        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;

        std::atomic_flag debugThisFrame;
        CFuncCollection funcs;
    };
} // namespace sp::vulkan::renderer
