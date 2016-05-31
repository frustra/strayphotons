#ifndef SP_MODEL_H
#define SP_MODEL_H

#include "Common.hh"
#include "graphics/Graphics.hh"

#include <tiny_gltf_loader.h>

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
			int componentType;
			size_t componentCount;
			string bufferName;
		};

		struct Primitive
		{
			glm::mat4 matrix;
			int drawMode;
			Attribute indexBuffer;
			string textureName;
			Attribute attributes[3];

			GLuint vertexBufferHandle;
			GLuint indexBufferHandle;
			GLuint textureHandle;
		};

		const string name;
		tinygltf::Scene *scene;
		vector<Primitive *> primitives;
	private:
		void AddNode(string nodeName, glm::mat4 parentMatrix);

		shared_ptr<Asset> asset;

		bool glLoaded;
	};
}

#endif
