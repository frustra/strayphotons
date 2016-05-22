#ifndef SP_MODEL_H
#define SP_MODEL_H

#include "Common.hh"

#include "tiny_gltf_loader.h"

namespace sp
{
	class Asset;

	class Model : public NonCopyable
	{
	public:
		Model(const string &name, shared_ptr<Asset> asset);
		~Model();

		const string name;
		tinygltf::Scene scene;

	private:
		shared_ptr<Asset> asset;
	};
}

#endif
