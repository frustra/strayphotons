#pragma once

#include "Common.hh"
#include "graphics/RenderTargetPool.hh"
#include "ecs/components/View.hh"

namespace sp
{
	class Renderer;
	class PostProcessPassBase;
	class PostProcessingContext;

	class ProcessPassOutput
	{
	public:
		RenderTargetDesc TargetDesc;
		RenderTarget::Ref TargetRef;

		void AddDependency()
		{
			dependencies++;
		}

		void ReleaseDependency()
		{
			if (--dependencies == 0)
			{
				//Debugf("Release target %d", TargetRef->GetID());
				TargetRef.reset();
			}
		}

		RenderTarget::Ref AllocateTarget(const PostProcessingContext *context);

	private:
		size_t dependencies = 0;
	};

	struct ProcessPassOutputRef
	{
		ProcessPassOutputRef() : pass(nullptr), outputIndex(0) { }

		ProcessPassOutputRef(PostProcessPassBase *pass, uint32 outputIndex = 0) :
			pass(pass), outputIndex(outputIndex) { }

		ProcessPassOutput *GetOutput();

		PostProcessPassBase *pass;
		uint32 outputIndex;
	};

	class PostProcessPassBase
	{
	public:
		virtual ~PostProcessPassBase() {}

		virtual void Process(const PostProcessingContext *context) = 0;
		virtual RenderTargetDesc GetOutputDesc(uint32 id) = 0;

		virtual ProcessPassOutput *GetOutput(uint32 id) = 0;
		virtual void SetInput(uint32 id, ProcessPassOutputRef input) = 0;
		virtual ProcessPassOutputRef *GetInput(uint32 id) = 0;
		virtual string Name() = 0;
	};

	template <uint32 inputCount, uint32 outputCount>
	class PostProcessPass : public PostProcessPassBase
	{
	public:
		PostProcessPass() {}

		virtual ProcessPassOutput *GetOutput(uint32 id)
		{
			if (id >= outputCount) return nullptr;
			return &outputs[id];
		}

		virtual void SetInput(uint32 id, ProcessPassOutputRef input)
		{
			Assert(id < inputCount);
			inputs[id] = input;
		}

		virtual ProcessPassOutputRef *GetInput(uint32 id)
		{
			if (id >= inputCount) return nullptr;
			return &inputs[id];
		}

	protected:
		void SetOutputTarget(uint32 id, RenderTarget::Ref target)
		{
			outputs[id].TargetRef = target;
		}

	private:
		ProcessPassOutputRef inputs[inputCount ? inputCount : 1];

	protected:
		ProcessPassOutput outputs[outputCount];
	};

	struct EngineRenderTargets
	{
		RenderTarget::Ref GBuffer0, GBuffer1, GBuffer2;
		RenderTarget::Ref DepthStencil;
	};

	class PostProcessingContext
	{
	public:
		void ProcessAllPasses();

		template<typename PassType, typename ...ArgTypes>
		PassType *AddPass(ArgTypes... args)
		{
			// TODO(pushrax): don't use the heap
			PassType *pass = new PassType(args...);
			passes.push_back(pass);
			return pass;
		}

		~PostProcessingContext()
		{
			for (auto pass : passes)
				delete pass;
		}

		Renderer *renderer;
		ECS::View view;

		ProcessPassOutputRef LastOutput;
		ProcessPassOutputRef GBuffer0;
		ProcessPassOutputRef GBuffer1;
		ProcessPassOutputRef Depth;

	private:
		vector<PostProcessPassBase *> passes;
	};

	namespace PostProcessing
	{
		void Process(Renderer *renderer, const ECS::View &view, const EngineRenderTargets &targets);
	}
}
