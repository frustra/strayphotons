#pragma once

#include "Common.hh"
#include "graphics/RenderTargetPool.hh"

namespace sp
{
	class PostProcessPassInterface;

	struct ProcessPassOutput
	{
		RenderTargetDesc renderTargetDesc;
		RenderTarget::Ref renderTarget;
	};

	struct ProcessPassOutputRef
	{
		ProcessPassOutputRef() : pass(nullptr), outputIndex(0) { }

		ProcessPassOutputRef(PostProcessPassInterface *pass, uint32 outputIndex) :
			pass(pass), outputIndex(outputIndex) { }

		ProcessPassOutput *GetOutput();

		PostProcessPassInterface *pass;
		uint32 outputIndex;
	};

	class PostProcessPassInterface
	{
	public:
		virtual ProcessPassOutput *GetOutput(uint32 id) = 0;
		virtual void SetInput(uint32 id, ProcessPassOutputRef input) = 0;
		virtual void Process() = 0;
	};

	template <uint32 inputCount, uint32 outputCount>
	class PostProcessPass : public PostProcessPassInterface
	{
	public:
		PostProcessPass()
		{
		}

		ProcessPassOutput *GetOutput(uint32 id)
		{
			Assert(id < outputCount);
			return &outputs[id];
		}

		void SetInput(uint32 id, ProcessPassOutputRef input)
		{
			Assert(id < inputCount);
			inputs[id] = input;
		}

	private:
		ProcessPassOutputRef inputs[inputCount];
		ProcessPassOutput outputs[outputCount];
	};

	struct PostProcessingContext
	{
		RenderTarget::Ref GBuffer0, GBuffer1, GBuffer2;
		RenderTarget::Ref DepthStencil;
	};

	namespace PostProcessing
	{
		void Process(const PostProcessingContext &context);
	}
}