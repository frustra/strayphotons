#pragma once

#include "Common.hh"
#include "core/Game.hh"
#include "ecs/components/View.hh"
#include "ecs/components/VoxelInfo.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/Renderer.hh"

namespace sp {
	class Renderer;
	class PostProcessPassBase;
	class PostProcessingContext;

	class ProcessPassOutput {
	public:
		RenderTargetDesc TargetDesc;
		RenderTarget::Ref TargetRef;

		void AddDependency() {
			dependencies++;
		}

		void ReleaseDependency() {
			if (--dependencies == 0) {
				// Debugf("Release target %d", TargetRef->GetID());
				TargetRef.reset();
			}
		}

		RenderTarget::Ref AllocateTarget(const PostProcessingContext *context);

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
		void SetOutputTarget(uint32 id, RenderTarget::Ref target) {
			outputs[id].TargetRef = target;
		}

	private:
		ProcessPassOutputRef inputs[inputCount ? inputCount : 1];

	protected:
		ProcessPassOutput outputs[outputCount];
		ProcessPassOutputRef dependencies[dependencyCount ? dependencyCount : 1];
	};

	struct EngineRenderTargets {
		RenderTarget::Ref gBuffer0, gBuffer1, gBuffer2, gBuffer3;
		RenderTarget::Ref shadowMap, mirrorShadowMap;
		RenderTarget::Ref mirrorIndexStencil, lightingGel;
		VoxelData voxelData;
		Buffer mirrorVisData;
		Buffer mirrorSceneData;

		RenderTarget::Ref finalOutput;
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

		~PostProcessingContext() {
			for (auto pass : passes)
				delete pass;
		}

		Renderer *renderer;
		sp::Game *game;
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

		Buffer MirrorVisData;
		Buffer MirrorSceneData;

	private:
		vector<PostProcessPassBase *> passes;
	};

	namespace PostProcessing {
		void Process(Renderer *renderer, sp::Game *game, ecs::View view, const EngineRenderTargets &targets);
	}
} // namespace sp
