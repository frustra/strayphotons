#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/View.hh"
#include "ecs/components/VoxelInfo.hh"
#include "graphics/opengl/GLRenderTarget.hh"
#include "graphics/opengl/RenderTargetPool.hh"
#include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"

#include <memory>

namespace sp {
    class VoxelRenderer;
    class PostProcessPassBase;
    class PostProcessingContext;
    class Game;

    class ProcessPassOutput {
    public:
        RenderTargetDesc TargetDesc;
        std::shared_ptr<GLRenderTarget> TargetRef;

        void AddDependency() {
            dependencies++;
        }

        void ReleaseDependency() {
            if (--dependencies == 0) {
                // Debugf("Release target %d", TargetRef->GetID());
                TargetRef.reset();
            }
        }

        std::shared_ptr<GLRenderTarget> AllocateTarget(const PostProcessingContext *context);

    private:
        size_t dependencies = 0;
    };

    struct ProcessPassOutputRef {
        ProcessPassOutputRef() : pass(nullptr), outputIndex(0) {}

        ProcessPassOutputRef(PostProcessPassBase *pass, uint32 outputIndex = 0)
            : pass(pass), outputIndex(outputIndex) {}

        ProcessPassOutput *GetOutput();

        PostProcessPassBase *pass;
        uint32 outputIndex;
    };

    class PostProcessPassBase {
    public:
        virtual ~PostProcessPassBase() {}

        virtual void Process(const PostProcessingContext *context) = 0;
        virtual RenderTargetDesc GetOutputDesc(uint32 id) = 0;

        virtual ProcessPassOutput *GetOutput(uint32 id) = 0;
        virtual void SetInput(uint32 id, ProcessPassOutputRef input) = 0;
        virtual void SetDependency(uint32 id, ProcessPassOutputRef depend) = 0;
        virtual ProcessPassOutputRef *GetInput(uint32 id) = 0;
        virtual ProcessPassOutputRef *GetDependency(uint32 id) = 0;
        virtual ProcessPassOutputRef *GetAllDependencies(uint32 id) = 0;
        virtual string Name() = 0;
    };

    template<uint32 inputCount, uint32 outputCount, uint32 dependencyCount = 0>
    class PostProcessPass : public PostProcessPassBase {
    public:
        PostProcessPass() {}

        ProcessPassOutput *GetOutput(uint32 id) {
            if (id >= outputCount) return nullptr;
            return &outputs[id];
        }

        void SetInput(uint32 id, ProcessPassOutputRef input) {
            Assert(id < inputCount, "post-process input overflow");
            inputs[id] = input;
        }

        void SetDependency(uint32 id, ProcessPassOutputRef depend) {
            Assert(id < dependencyCount, "post-process dependency overflow");
            dependencies[id] = depend;
        }

        ProcessPassOutputRef *GetInput(uint32 id) {
            if (id >= inputCount) return nullptr;
            return &inputs[id];
        }

        ProcessPassOutputRef *GetDependency(uint32 id) {
            if (id >= dependencyCount) return nullptr;
            return &dependencies[id];
        }

        ProcessPassOutputRef *GetAllDependencies(uint32 id) {
            auto ref = GetInput(id);
            if (ref) return ref;
            return GetDependency(id - inputCount);
        }

    protected:
        void SetOutputTarget(uint32 id, std::shared_ptr<GLRenderTarget> &target) {
            outputs[id].TargetRef = target;
        }

    private:
        ProcessPassOutputRef inputs[inputCount ? inputCount : 1];

    protected:
        ProcessPassOutput outputs[outputCount];
        ProcessPassOutputRef dependencies[dependencyCount ? dependencyCount : 1];
    };

    struct EngineRenderTargets {
        std::shared_ptr<GLRenderTarget> gBuffer0, gBuffer1, gBuffer2, gBuffer3;
        std::shared_ptr<GLRenderTarget> shadowMap, mirrorShadowMap;
        std::shared_ptr<GLRenderTarget> mirrorIndexStencil, lightingGel;
        VoxelData voxelData;
        GLBuffer mirrorVisData;
        GLBuffer mirrorSceneData;

        RenderTarget *finalOutput = nullptr;
    };

    class PostProcessingContext {
    public:
        void ProcessAllPasses();

        template<typename PassType, typename... ArgTypes>
        PassType *AddPass(ArgTypes... args) {
            // TODO(pushrax): don't use the heap
            PassType *pass = new PassType(args...);
            passes.push_back(pass);
            return pass;
        }

        PostProcessingContext(VoxelRenderer *renderer, ecs::EntityManager &ecs, ecs::View view)
            : renderer(renderer), ecs(ecs), view(view) {}

        ~PostProcessingContext() {
            for (auto pass : passes)
                delete pass;
        }

        VoxelRenderer *renderer;
        ecs::EntityManager &ecs;
        ecs::View view;

        ProcessPassOutputRef LastOutput;
        ProcessPassOutputRef GBuffer0;
        ProcessPassOutputRef GBuffer1;
        ProcessPassOutputRef GBuffer2;
        ProcessPassOutputRef GBuffer3;
        ProcessPassOutputRef ShadowMap;
        ProcessPassOutputRef MirrorShadowMap;
        ProcessPassOutputRef VoxelRadiance;
        ProcessPassOutputRef VoxelRadianceMips;
        ProcessPassOutputRef MirrorIndexStencil;
        ProcessPassOutputRef LightingGel;
        ProcessPassOutputRef AoBuffer;

        GLBuffer MirrorVisData;
        GLBuffer MirrorSceneData;

    private:
        vector<PostProcessPassBase *> passes;
    };

    namespace PostProcessing {
        void Process(VoxelRenderer *renderer,
                     ecs::EntityManager &ecs,
                     ecs::View view,
                     const EngineRenderTargets &targets);
    }
} // namespace sp
