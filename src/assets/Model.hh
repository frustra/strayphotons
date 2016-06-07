#pragma once

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
		vector<Primitive *> primitives;
		std::map<string, GLuint> buffers;
		std::map<string, GLuint> textures;

		GLuint LoadBuffer(string name);
		GLuint LoadTexture(string name);
	private:
		void AddNode(string nodeName, glm::mat4 parentMatrix);

		tinygltf::Scene *scene;
		shared_ptr<Asset> asset;
	};
}
