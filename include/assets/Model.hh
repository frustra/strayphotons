#ifndef SP_MODEL_H
#define SP_MODEL_H

#include "Common.hh"
#include <tiny_gltf_loader.h>
#include <glm/glm.hpp>

namespace sp
{
	class Asset;

	class Model : public NonCopyable
	{
	public:
		Model(const string &name, shared_ptr<Asset> asset, tinygltf::Scene *scene);
		~Model() {}

		struct Attribute
		{
			size_t byteOffset, byteStride;
			int componentType, componentCount;
			string bufferName;
		};

		struct Primitive
		{
			glm::mat4 matrix;
			size_t elementCount;
			string indexBuffer;
			size_t indexOffset;
			Attribute attributes[3];
		};

		const string name;
		tinygltf::Scene *scene;
		vector<Primitive> primitives;
	private:
		void AddNode(string nodeName, glm::mat4 parentMatrix);

		shared_ptr<Asset> asset;
	};
}

#endif
