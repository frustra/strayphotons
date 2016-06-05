#pragma once

#include "Common.hh"
#include "graphics/RenderTargetPool.hh"

namespace sp
{
	class Renderer;
	class PostProcessPassBase;
	class PostProcessingContext;

	class ProcessPassOutput
	{
	public:
		RenderTargetDesc RenderTargetDesc;
		RenderTarget::Ref RenderTarget;

		void AddDependency()
		{
			dependencies++;
		}

		void ReleaseDependency()
		{
			if (--dependencies == 0)
			{
				RenderTarget.reset();
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
		virtual void Process(const PostProcessingContext *context) = 0;
		virtual RenderTargetDesc GetOutputDesc(uint32 id) = 0;

		virtual ProcessPassOutput *GetOutput(uint32 id) = 0;
		virtual void SetInput(uint32 id, ProcessPassOutputRef input) = 0;
		virtual ProcessPassOutputRef *GetInput(uint32 id) = 0;
	};

	template <uint32 inputCount, uint32 outputCount>
	class PostProcessPass : public PostProcessPassBase
	{
	public:
		PostProcessPass()
		{
		}

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
			outputs[id].RenderTarget = target;
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

		PostProcessPassBase *AddPass(PostProcessPassBase *pass)
		{
			passes.push_back(pass);
			return pass;
		}

		ProcessPassOutputRef LastOutput;
		Renderer *Renderer;

	private:
		vector<PostProcessPassBase *> passes;
	};

	namespace PostProcessing
	{
		void Process(Renderer *renderer, const EngineRenderTargets &context);
	}
}